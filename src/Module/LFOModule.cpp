// LFOModule.cpp — see LFOModule.hpp.

#include "Module.hpp"
#include <cmath>
#include <numbers>

#include "pm_params.h"
#include "LFOModule.hpp"
#include "LFOClockSync.hpp"

static constexpr double TWO_PI = 2.0 * std::numbers::pi_v<double>;

struct LFOState {
    float prevReset = 0.0f;   // last reset-input sample, for rising-edge detection
    LFOClockSync clock;       // Clock-input rate/phase lock (see LFOClockSync.hpp)
};

const void InitLFOModule(uint32_t /*count*/, Module* module) {
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;
    module->state = new LFOState();
}

const void DeinitLFOModule(Module* module) {
    delete static_cast<LFOState*>(module->state);
    module->state = nullptr;
}

const void ResetLFOModule(Module* module) {
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;
    if (module->state) {
        auto* st = static_cast<LFOState*>(module->state);
        st->prevReset = 0.0f;
        st->clock.reset();
    }
}

// Waveform value at phase ph in [0,1). Matches PM's tables: triangle 0→+1 at ¼ → 0 at ½ → −1 at ¾;
// square ±1 half-and-half; impulse a single-sample 1 at cycle start; sine starts at 0.
static inline float lfoWave(int wave, double ph, double delta) {
    switch (wave) {
        case LFOParamWaveTriangle:
            if (ph < 0.25) return (float)(4.0 * ph);
            if (ph < 0.75) return (float)(2.0 - 4.0 * ph);
            return (float)(4.0 * ph - 4.0);
        case LFOParamWaveSquare:  return ph < 0.5   ? 1.0f : -1.0f;
        case LFOParamWaveImpulse: return ph < delta ? 1.0f :  0.0f;
        default: /* Sine */       return (float)std::sin(ph * TWO_PI);
    }
}

const KPBuffPair* ProcessLFOModule(uint64_t processCallID, uint32_t count, Module* module) {
    auto* st = static_cast<LFOState*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;
    const double freqScale = module->mFrequencyScale;

    auto pitchPair = module->retrieveInput(LFOParamItemPitchInput, processCallID, count);
    auto resetPair = module->retrieveInput(LFOParamItemResetInput, processCallID, count);
    const float* lpp = pitchPair->first->data;   // per-sample exponential rate FM (L/R)
    const float* rpp = pitchPair->second->data;
    const float* lrp = resetPair->first->data;   // reset trigger

    // Clock input: when patched, rate and phase lock to the clock (Pitch input then only steers
    // Auto's ratio choice). Unpatched, the clock state is dropped so re-patching starts clean.
    const bool clockConn = module->isInputConnected(LFOParamItemClockInput);
    const float* clk = module->retrieveInput(LFOParamItemClockInput, processCallID, count)->first->data;
    const LFOClockRatio clockRatio = lfoClockRatioForIndex(module->paramByEnum(LFOParamItemClockRatio));
    if (!clockConn && st->clock.edgeCount) st->clock.reset();

    double& phaseL = module->mOmegaL;
    double& phaseR = module->mOmegaR;

    const float freqParam = module->paramByEnum(LFOParamItemPitch);   // octaves; base 1 Hz
    const int   wave      = (int)(module->paramByEnum(LFOParamItemWave) + 0.5f);

    float phaseShift = 0.0f;
    switch (static_cast<LFOParamLRPhase>((int)module->paramByEnum(LFOParamItemPhase))) {
        case LFOParamLRPhase60:  phaseShift = 1.0f / 6.0f; break;
        case LFOParamLRPhase90:  phaseShift = 1.0f / 4.0f; break;
        case LFOParamLRPhase120: phaseShift = 1.0f / 3.0f; break;
        case LFOParamLRPhase180: phaseShift = 1.0f / 2.0f; break;
        default: break;   // LFOParamLRPhase0
    }
    // Re-establish the L/R phase offset at the block boundary (matches PM), then advance independently.
    phaseR = phaseL + phaseShift;  if (phaseR >= 1.0) phaseR -= 1.0;

    for (uint32_t i = 0; i < count; ++i) {
        const float r = lrp[i];                                // reset on rising edge
        if (r >= 0.5f && st->prevReset < 0.5f) {
            phaseL = 0.0;
            phaseR = phaseShift;  if (phaseR >= 1.0) phaseR -= 1.0;
            st->clock.restartGroup();                          // next clock edge is a cycle start
        }
        st->prevReset = r;

        double dL = std::exp2((double)(freqParam + lpp[i])) * freqScale;   // base 1 Hz → freq = 2^pitch
        double dR = std::exp2((double)(freqParam + rpp[i])) * freqScale;

        if (clockConn) {                                       // slewed rate + phase lock
            st->clock.edge(clk[i], clockRatio, dL, phaseL);
            const double d = st->clock.step(dL);
            if (st->clock.hasPeriod()) dL = dR = d;            // locked: L/R share the clock rate
        }

        outL[i] = lfoWave(wave, phaseL, dL);
        outR[i] = lfoWave(wave, phaseR, dR);

        phaseL += dL;  if (phaseL >= 1.0) phaseL -= 1.0;
        phaseR += dR;  if (phaseR >= 1.0) phaseR -= 1.0;
    }

    return &module->output;
}
