//
//  LPFilterModule.cpp — 24 dB/oct Butterworth low-pass (IFSynth port of PM's LPFilterModule).
//
//  Two cascaded DF1 biquads (ButterLow4) per channel. Coefficients are recomputed once per
//  16-sample control block: cutoff = pitchConverter->pitchToFrequency(pitch + freq-mods), Q is
//  clamped to [1,20]. Freq/Q mod inputs are slew-limited (FMSlewLimiter) so a fast modulation
//  signal can't destabilise the IIR. The inner loop is pure IIR arithmetic.
//

#include <algorithm>
#include <cmath>

#include "ButterLow4.hpp"
#include "FMSlewLimiter.hpp"
#include "LPFilterModule.hpp"
#include "ResonanceComp.hpp"
#include "PitchConverter.hpp"
#include "pm_params.h"

static constexpr float kFMSlewSeconds = 0.005f;

struct LPFHelperStruct {
    ButterLow4    butterLowL;
    ButterLow4    butterLowR;
    FMSlewLimiter fmSlew1;
    FMSlewLimiter fmSlew2;

    explicit LPFHelperStruct(double sampleRate)
        : butterLowL(sampleRate), butterLowR(sampleRate)
    {}
};

const void InitLPFilterModule(uint32_t /*count*/, Module* module)
{
    module->state = new LPFHelperStruct(1.0 / module->mFrequencyScale);
}

const void DeinitLPFilterModule(Module* module)
{
    delete static_cast<LPFHelperStruct*>(module->state);
    module->state = nullptr;
}

const void SampleRateLPFilterModule(Module* module, double newSampleRate)
{
    auto* state = static_cast<LPFHelperStruct*>(module->state);
    state->butterLowL.setSampleRate(newSampleRate);
    state->butterLowR.setSampleRate(newSampleRate);
}

const KPBuffPair* ProcessLPFilterModule(uint64_t processCallID, uint32_t count, Module* module)
{
    if (!module->isInputConnected(LPFParamItemInput))
        return KPBuffPairHelper::zeroes();

    auto inputPair = module->retrieveInput(LPFParamItemInput,       processCallID, count);
    auto fmPair    = module->retrieveInput(LPFParamItemModFreqInput,  processCallID, count);
    auto fmPair2   = module->retrieveInput(LPFParamItemModFreqInput2, processCallID, count);
    auto qmPair    = module->retrieveInput(LPFParamItemModQInput,     processCallID, count);

    const AUValue pitchParam         = module->paramByEnum(LPFParamItemPitch);
    const AUValue freqModLevelParam  = module->paramByEnum(LPFParamItemFreqModLevel);
    const AUValue freqModLevel2Param = module->paramByEnum(LPFParamItemFreqModLevel2);
    const AUValue qParam             = module->paramByEnum(LPFParamItemQ);
    const AUValue qModLevelParam     = module->paramByEnum(LPFParamItemModQInputLevel);

    auto* state = static_cast<LPFHelperStruct*>(module->state);

    const float* lip   = inputPair->first->data;
    const float* rip   = inputPair->second->data;
    const float* lfmp  = fmPair->first->data;
    const float* rfmp  = fmPair->second->data;
    const float* lfmp2 = fmPair2->first->data;
    const float* rfmp2 = fmPair2->second->data;
    const float* lqmp  = qmPair->first->data;
    const float* rqmp  = qmPair->second->data;

    float* lop = module->output.first->data;
    float* rop = module->output.second->data;

    const double sampleRate   = 1.0 / module->mFrequencyScale;
    const double nyquistGuard = 0.4999 * sampleRate;
    const float  qMin         = 1.0f;
    const float  qMax         = 20.0f;
    const int    kBlockSize   = 16;

    for (int i = 0; i < (int)count; i += kBlockSize) {
        const int n = std::min(kBlockSize, (int)count - i);

        // Sample the (control-rate) modulation inputs once per block, then slew-limit.
        float modL  = lfmp[0];
        float modR  = rfmp[0];
        float mod2L = lfmp2[0];
        float mod2R = rfmp2[0];
        float qModL = lqmp[0];
        float qModR = rqmp[0];

        {
            const float risePerBlock = n * (float)module->mFrequencyScale / kFMSlewSeconds;
            float slewedModL, slewedModR, slewedMod2L, slewedMod2R;
            state->fmSlew1.process(slewedModL,  slewedModR,  modL,  modR,  risePerBlock, risePerBlock);
            state->fmSlew2.process(slewedMod2L, slewedMod2R, mod2L, mod2R, risePerBlock, risePerBlock);
            modL  = slewedModL;  modR  = slewedModR;
            mod2L = slewedMod2L; mod2R = slewedMod2R;
        }

        // Cutoff frequency — pitchToFrequency once per block per channel.
        double cutoffL = module->pitchConverter->pitchToFrequency(
                             pitchParam + modL * freqModLevelParam + mod2L * freqModLevel2Param);
        double cutoffR = module->pitchConverter->pitchToFrequency(
                             pitchParam + modR * freqModLevelParam + mod2R * freqModLevel2Param);
        if (cutoffL > nyquistGuard) cutoffL = nyquistGuard;
        if (cutoffR > nyquistGuard) cutoffR = nyquistGuard;

        const double qL = std::clamp((double)(qParam + qModL * qModLevelParam), (double)qMin, (double)qMax);
        const double qR = std::clamp((double)(qParam + qModR * qModLevelParam), (double)qMin, (double)qMax);

        // Coefficients once per block per channel (recalc contains tan()). Resonance (q) is applied to
        // ONE stage only; the other stays at its Butterworth damping (q=1). Driving both stages, as
        // before, stacked their peaks into a huge spike (+37 dB @ Q10, +48 @ Q20). One resonant stage
        // gives a single peak (~like a lone biquad), which resonanceComp then tames consistently.
        ButterCoeff bcL1, bcL2, bcR1, bcR2;
        state->butterLowL.recalc(sampleRate, cutoffL, ButterLow4::K_STAGE_1, qL,  &bcL1);  // resonant
        state->butterLowL.recalc(sampleRate, cutoffL, ButterLow4::K_STAGE_2, 1.0, &bcL2);  // Butterworth
        state->butterLowR.recalc(sampleRate, cutoffR, ButterLow4::K_STAGE_1, qR,  &bcR1);  // resonant
        state->butterLowR.recalc(sampleRate, cutoffR, ButterLow4::K_STAGE_2, 1.0, &bcR2);  // Butterworth

        ButterLow4State& stL = state->butterLowL.getState();
        ButterLow4State& stR = state->butterLowR.getState();
        const float compL = resonanceComp((float)qL);   // tame the high-Q peak (see ResonanceComp.hpp)
        const float compR = resonanceComp((float)qR);
        for (int j = 0; j < n; ++j) {
            lop[i + j] = ButterLow4::processWithCoeff(lip[i + j], bcL1, bcL2, stL) * compL;
            rop[i + j] = ButterLow4::processWithCoeff(rip[i + j], bcR1, bcR2, stR) * compR;
        }

        lfmp  += n;  rfmp  += n;
        lfmp2 += n;  rfmp2 += n;
        lqmp  += n;  rqmp  += n;
    }

    return &module->output;
}
