// TriSawModule.cpp — see TriSawModule.hpp. Cross-platform port of PM's TriSaw: the vectorized
// Accelerate delta precompute is replaced by the per-sample pitchConverter->pitchToFrequency path
// (identical math) our SinOsc/IFSOsc use. Keeps PM's Peak morph, hard sync, and phase self-mod.

#include "Module.hpp"
#include <algorithm>
#include <cmath>

#include "pm_midi.h"
#include "PitchConverter.hpp"
#include "TriSawModule.hpp"

#define MOD_PHASE_ATTENUATOR    0.5

// Self-modulation depth clamp (COMBINED knob + input). The symmetric-triangle case (Peak ≈ 0.5) is
// the first to break into noise (~0.98), so ±0.9 covers the worst peak. (SinOsc's smooth sine takes
// 1.2; the tri/saw's cornered waveform does not.)
static constexpr double kSelfModMax = 0.9;

// Empirically-tuned scale for the corner polyBLAMP correction (see its use below).
static constexpr float kBlampScale = 0.6f;

static inline double wrapPhase(double x) { return x - std::floor(x); }

// Variable tri↔saw shaper. x in [0,1), peak p in (0,1): a line from -1 up to +1 over [0,p), then back
// down to -1 over [p,1). p=0.5 → symmetric triangle; p→0 or →1 → sawtooth.
static float sawToTri(float x, float p) {
    float m1 = 2.0f / p;
    float b1 = -1.0f;
    float m2 = 2.0f / (p - 1.0f);
    float b2 = -1.0f - m2;
    float y1 = m1 * x + b1;
    float y2 = m2 * x + b2;
    return (x < p) ? y1 : y2;
}

// polyBLAMP: 2-sample band-limited-ramp residual for a SLOPE discontinuity (a corner), evaluated at
// phase-distance t (in [0,1)) from the corner with per-sample phase increment dt. It's the integral
// of the 2-sample polyBLEP, so it rounds a piecewise-linear corner instead of a value step. Returns
// the correction for a UNIT per-sample slope change; the caller scales by the actual slope change.
// Positive bump centered on the corner (±dt wide). See Esqueda/Välimäki, "Rounding Corners with BLAMP".
static inline float polyBLAMP(float t, float dt) {
    if (t < dt)        { float a = t / dt - 1.0f;         return -(1.0f / 3.0f) * a * a * a; }
    if (t > 1.0f - dt) { float a = (t - 1.0f) / dt + 1.0f; return  (1.0f / 3.0f) * a * a * a; }
    return 0.0f;
}

struct TriSawSyncState {
    float prevSyncL     = 0.0f;
    float prevSyncR     = 0.0f;
    float blepResidualL = 0.0f;  ///< carry-over BLEP correction for the next sample (left)
    float blepResidualR = 0.0f;  ///< carry-over BLEP correction for the next sample (right)
    float midiPitch     = 0.0f;  ///< pitch (octaves re A440) of the last note; used only when Pitch input unconnected
    float prevSelf1L    = 0.0f;  // 2-sample output history for self-mod feedback (see SinOsc)
    float prevSelf2L    = 0.0f;
    float prevSelf1R    = 0.0f;
    float prevSelf2R    = 0.0f;
};

const void InitTriSawOscillator(uint32_t /*count*/, Module* module) {
    double hz = 440.0;
    module->mBaseDeltaOmegaL = hz * module->mFrequencyScale;
    module->mBaseDeltaOmegaR = hz * module->mFrequencyScale;
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;
    module->state = new TriSawSyncState();
}

const void DeinitTriSawOscillator(Module* module) {
    delete static_cast<TriSawSyncState*>(module->state);
    module->state = nullptr;
}

const void ResetTriSawOscillator(Module* module) {
    module->mOmegaL = 0.0;
    module->mOmegaR = 0.0;
    if (module->state) {
        auto* s = static_cast<TriSawSyncState*>(module->state);
        s->prevSyncL = s->prevSyncR = 0.0f;
        s->blepResidualL = s->blepResidualR = 0.0f;
        s->midiPitch = 0.0f;
        s->prevSelf1L = s->prevSelf2L = s->prevSelf1R = s->prevSelf2R = 0.0f;
    }
}

// Note On sets the pitch from the played note (mirrors SinOsc/IFSOsc). A patched Pitch input overrides
// it, so the note is ignored whenever that input is connected.
void MIDITriSawOscillator(const pm::UmpMessage& message, Module* module) {
    if (module->isInputConnected(TriSawParamItemPitchInput)) return;
    if (message.status == pm::kStatusNoteOn && message.velocity > 0) {
        double freq = 0.0;
        if (message.attributeType == pm::kAttrPitch7_9) {
            uint16_t attr     = message.attribute;
            uint8_t  note     = (attr & 0xFE00) >> 9;
            uint16_t fraction = attr & 0x01FF;
            if (fraction >= 512) fraction = 511;
            freq = pm::noteToFrequency(note) * pm::pitchFractionRatio(fraction);
        } else {
            freq = pm::noteToFrequency(message.note);
        }
        static_cast<TriSawSyncState*>(module->state)->midiPitch =
            module->pitchConverter->frequencyToPitch(freq);
        // Retune above, but only reset phase for a fresh note — a legato continuation keeps its
        // phase so the slide is click-free. See Module::legatoContinuation's own comment.
        if (!module->legatoContinuation) {
            module->mOmegaL = 0.0;
            module->mOmegaR = 0.0;
        }
    }
}

const KPBuffPair* ProcessTriSawOscillator(uint64_t processCallID, uint32_t count, Module* module) {
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;
    const double freqScale = module->mFrequencyScale;

    auto inputPair     = module->retrieveInput(TriSawParamItemPitchInput,   processCallID, count);
    auto modPitchPair  = module->retrieveInput(TriSawParamItemModPitch,     processCallID, count);
    auto modPitch2Pair = module->retrieveInput(TriSawParamItemModPitch2,    processCallID, count);
    auto modPhasePair  = module->retrieveInput(TriSawParamItemModPhase,     processCallID, count);
    auto modPeakPair   = module->retrieveInput(TriSawParamItemModPeak,      processCallID, count);
    auto modSyncPair   = module->retrieveInput(TriSawParamItemModSync,      processCallID, count);
    auto selfModPair   = module->retrieveInput(TriSawParamItemSelfModInput, processCallID, count);

    AUValue freqParam        = module->paramByEnum(TriSawParamItemPitch)     * (float)SEMITONES_TO_PITCH;
    AUValue fineParam        = module->paramByEnum(TriSawParamItemPitchFine) * (float)CENTS_TO_PITCH;
    AUValue peakParam        = module->paramByEnum(TriSawParamItemPeak);
    AUValue fmLevelParam     = module->paramByEnum(TriSawParamItemFreqModLevel);
    AUValue fmLevel2Param    = module->paramByEnum(TriSawParamItemFreqModLevel2);
    AUValue pmLevelParam     = module->paramByEnum(TriSawParamItemPhaseModLevel);
    AUValue peakModLevelParam= module->paramByEnum(TriSawParamItemPeakModLevel);
    AUValue selfModLevelParam   = module->paramByEnum(TriSawParamItemSelfModLevel);
    AUValue selfModInLevelParam = module->paramByEnum(TriSawParamItemSelfModInputLevel);

    const bool hasSyncInput = module->isInputConnected(TriSawParamItemModSync);
    const OscSyncType syncType = static_cast<OscSyncType>((int)module->paramByEnum(TriSawParamItemModSyncType));

    // Direct-MIDI pitch: applied only when nothing is patched to the Pitch input.
    const float midiPitch = module->isInputConnected(TriSawParamItemPitchInput) ? 0.0f
                          : static_cast<TriSawSyncState*>(module->state)->midiPitch;

    const float* ilptr  = inputPair->first->data;    const float* irptr  = inputPair->second->data;
    const float* mlptr  = modPitchPair->first->data; const float* mrptr  = modPitchPair->second->data;
    const float* m2lptr = modPitch2Pair->first->data;const float* m2rptr = modPitch2Pair->second->data;
    const float* nlptr  = modPhasePair->first->data; const float* nrptr  = modPhasePair->second->data;
    const float* plptr  = modPeakPair->first->data;  const float* prptr  = modPeakPair->second->data;
    const float* slptr  = modSyncPair->first->data;  const float* srptr  = modSyncPair->second->data;
    const float* fblptr = selfModPair->first->data;  const float* fbrptr = selfModPair->second->data;

    auto pitchConverter = module->pitchConverter;
    double& phaseL = module->mOmegaL;
    double& phaseR = module->mOmegaR;
    double& deltaL = module->mBaseDeltaOmegaL;
    double& deltaR = module->mBaseDeltaOmegaR;

    auto* st = static_cast<TriSawSyncState*>(module->state);
    float& prevSyncL     = st->prevSyncL;     float& prevSyncR     = st->prevSyncR;
    float& blepResidualL = st->blepResidualL; float& blepResidualR = st->blepResidualR;
    float& prevSelf1L    = st->prevSelf1L;    float& prevSelf2L    = st->prevSelf2L;
    float& prevSelf1R    = st->prevSelf1R;    float& prevSelf2R    = st->prevSelf2R;

    for (uint32_t i = 0; i < count; ++i) {
        deltaL = pitchConverter->pitchToFrequency(ilptr[i] + midiPitch + freqParam + fineParam + mlptr[i] * fmLevelParam + m2lptr[i] * fmLevel2Param) * freqScale;
        deltaR = pitchConverter->pitchToFrequency(irptr[i] + midiPitch + freqParam + fineParam + mrptr[i] * fmLevelParam + m2rptr[i] * fmLevel2Param) * freqScale;

        const float clampedL = std::clamp(plptr[i] * peakModLevelParam + peakParam, 0.01f, 0.99f);
        const float clampedR = std::clamp(prptr[i] * peakModLevelParam + peakParam, 0.01f, 0.99f);

        const double modL = nlptr[i] * MOD_PHASE_ATTENUATOR * pmLevelParam;
        const double modR = nrptr[i] * MOD_PHASE_ATTENUATOR * pmLevelParam;
        const double selfDepthL = std::clamp((double)selfModLevelParam + fblptr[i] * selfModInLevelParam, -kSelfModMax, kSelfModMax);
        const double selfDepthR = std::clamp((double)selfModLevelParam + fbrptr[i] * selfModInLevelParam, -kSelfModMax, kSelfModMax);
        const double smL = 0.5 * (prevSelf1L + prevSelf2L) * MOD_PHASE_ATTENUATOR * selfDepthL;
        const double smR = 0.5 * (prevSelf1R + prevSelf2R) * MOD_PHASE_ATTENUATOR * selfDepthR;

        const double xL = wrapPhase(phaseL + modL + smL);
        const double xR = wrapPhase(phaseR + modR + smR);
        float rawSampleL = sawToTri((float)xL, clampedL);
        float rawSampleR = sawToTri((float)xR, clampedR);
        prevSelf2L = prevSelf1L;  prevSelf1L = rawSampleL;   // shift the 2-sample history
        prevSelf2R = prevSelf1R;  prevSelf1R = rawSampleR;

        float sampleL = rawSampleL + blepResidualL;
        float sampleR = rawSampleR + blepResidualR;
        blepResidualL = 0.0f;
        blepResidualR = 0.0f;

        // polyBLAMP the two corners of the tri/saw shape (its slope discontinuities) so they don't
        // alias. Bottom vertex at x=0 (slope jumps m2→m1) and peak vertex at x=p (m1→m2); the
        // per-sample slope change is ΔslopePerPhase × delta. Rounds the corners → far less aliasing.
        // kBlampScale (≈0.6) was tuned empirically by minimizing aliased/harmonic energy across
        // pitches (tests/kernel_sweep --dump + FFT); it flattens near this value. It absorbs the
        // small normalization gap between the ideal correction and the low-order (2-sample) kernel.
        // The 2-sample kernel is only valid below ~quarter Nyquist (dt < 0.5); above that the pitch is
        // already aliasing garbage, and the (m1-m2)·dt term would balloon — so skip it there.
        if (deltaL < 0.5) {
            const float m1 = 2.0f / clampedL, m2 = 2.0f / (clampedL - 1.0f);
            const float dt = (float)deltaL;
            float u = (float)xL - clampedL; if (u < 0.0f) u += 1.0f;
            sampleL += kBlampScale * (m1 - m2) * dt * polyBLAMP((float)xL, dt);   // bottom corner (x=0)
            sampleL += kBlampScale * (m2 - m1) * dt * polyBLAMP(u,        dt);   // peak corner   (x=p)
        }
        if (deltaR < 0.5) {
            const float m1 = 2.0f / clampedR, m2 = 2.0f / (clampedR - 1.0f);
            const float dt = (float)deltaR;
            float u = (float)xR - clampedR; if (u < 0.0f) u += 1.0f;
            sampleR += kBlampScale * (m1 - m2) * dt * polyBLAMP((float)xR, dt);
            sampleR += kBlampScale * (m2 - m1) * dt * polyBLAMP(u,        dt);
        }

        phaseL += deltaL;  if (phaseL >= 1.0) phaseL -= 1.0;
        phaseR += deltaR;  if (phaseR >= 1.0) phaseR -= 1.0;

        if (hasSyncInput) {
            const float sL = slptr[i], sR = srptr[i];
            bool syncResetL = false, syncResetR = false;
            switch (syncType) {
                case OscSyncTypeDown:
                    syncResetL = (prevSyncL > 0.0f) && (sL <= 0.0f);
                    syncResetR = (prevSyncR > 0.0f) && (sR <= 0.0f);
                    break;
                case OscSyncTypeUp:
                    syncResetL = (prevSyncL < 0.0f) && (sL >= 0.0f);
                    syncResetR = (prevSyncR < 0.0f) && (sR >= 0.0f);
                    break;
                case OscSyncTypeBoth:
                    syncResetL = ((prevSyncL > 0.0f) && (sL <= 0.0f)) || ((prevSyncL < 0.0f) && (sL >= 0.0f));
                    syncResetR = ((prevSyncR > 0.0f) && (sR <= 0.0f)) || ((prevSyncR < 0.0f) && (sR >= 0.0f));
                    break;
            }
            if (syncResetL) {
                float tL = prevSyncL / (prevSyncL - sL);
                phaseL = (1.0 - tL) * deltaL;
                const double newXL = wrapPhase(phaseL + modL + smL);
                float stepL   = sawToTri((float)newXL, clampedL) - rawSampleL;
                float omtL    = 1.0f - tL;
                sampleL      += omtL * omtL * 0.5f * stepL;
                blepResidualL = -tL * tL * 0.5f * stepL;
            }
            if (syncResetR) {
                float tR = prevSyncR / (prevSyncR - sR);
                phaseR = (1.0 - tR) * deltaR;
                const double newXR = wrapPhase(phaseR + modR + smR);
                float stepR   = sawToTri((float)newXR, clampedR) - rawSampleR;
                float omtR    = 1.0f - tR;
                sampleR      += omtR * omtR * 0.5f * stepR;
                blepResidualR = -tR * tR * 0.5f * stepR;
            }
            prevSyncL = sL;
            prevSyncR = sR;
        }

        outL[i] = sampleL;
        outR[i] = sampleR;
    }

    return &module->output;
}
