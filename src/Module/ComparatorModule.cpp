// ComparatorModule.cpp — see .hpp. Per-channel Schmitt trigger: state flips high above
// threshold+halfBand and low below threshold−halfBand (hysteresis). Output depends on Type.
//
// The output steps at each threshold crossing (a square/pulse-like discontinuity), which aliases on
// high-frequency input. Each crossing is band-limited with a 2-point polyBLEP: the sub-sample crossing
// time is found by linear interpolation between the previous and current input, and a scaled residual
// corrects the current sample and is carried to the next. This handles multiple crossings per cycle
// (each detected flip is corrected as it occurs) and uses the direction-appropriate hysteresis
// threshold. The one case it cannot resolve is more than one crossing within a single sample (input
// already aliased at the base rate); there the Schmitt state simply reflects the net flip.

#include "ComparatorModule.hpp"
#include "pm_params.h"

#include <algorithm>
#include <cmath>

struct ComparatorState {
    bool  isHighL = false;
    bool  isHighR = false;
    float prevInL = 0.0f;   // previous input sample, for sub-sample crossing time
    float prevInR = 0.0f;
    float blepL   = 0.0f;   // polyBLEP residual carried into the next sample
    float blepR   = 0.0f;
};

static ComparatorType auValueToComparatorType(AUValue inType) {
    const NSUInteger index = (NSUInteger)std::lround(inType);
    return (index <= ComparatorTypeBottom) ? (ComparatorType)index : ComparatorTypeFullClip;
}

const void InitComparatorModule(uint32_t /*count*/, Module* module) {
    module->state = new ComparatorState();
}
const void DeinitComparatorModule(Module* module) {
    delete static_cast<ComparatorState*>(module->state);
    module->state = nullptr;
}
const void ResetComparatorModule(Module* module) {
    if (auto* s = static_cast<ComparatorState*>(module->state)) {
        s->isHighL = s->isHighR = false;
        s->prevInL = s->prevInR = 0.0f;
        s->blepL = s->blepR = 0.0f;
    }
}

// Process one channel with band-limited crossings. `in`/`prevIn` are the current/previous input;
// `isHigh`/`blep` are the per-channel Schmitt state and carried residual (updated in place).
static inline float comparatorStep(float in, float& prevIn, bool& isHigh, float& blep,
                                   ComparatorType type, float thr, float halfBand, float clip,
                                   bool bandlimit)
{
    const bool wasHigh = isHigh;
    if (isHigh) { if (in <= thr - halfBand) isHigh = false; }
    else        { if (in >  thr + halfBand) isHigh = true;  }

    // Naive output for the (possibly new) state.
    auto levelFor = [&](bool high, float passthru) -> float {
        switch (type) {
            case ComparatorTypeTop:    return high ?  clip : passthru;
            case ComparatorTypeBottom: return high ? passthru : -clip;
            case ComparatorTypeFullClip:
            default:                   return high ?  clip : -clip;
        }
    };

    float out = levelFor(isHigh, in) + blep;
    blep = 0.0f;

    if (bandlimit && isHigh != wasHigh) {
        // Sub-sample crossing time between prevIn and in, at the direction's hysteresis threshold.
        const float thrCross = isHigh ? (thr + halfBand) : (thr - halfBand);
        const float denom    = in - prevIn;
        float t = (std::fabs(denom) > 1e-20f) ? (thrCross - prevIn) / denom : 0.0f;
        t = std::fmax(0.0f, std::fmin(1.0f, t));
        const float omt = 1.0f - t;
        // Step size at the crossing (passthrough side evaluated at the crossing level = thrCross).
        const float step = levelFor(isHigh, thrCross) - levelFor(wasHigh, thrCross);
        // Convert the naive full-step into a band-limited transition: current sample gets a partial
        // step, the remainder is carried to the next sample.
        out  += step * (0.5f * omt * omt - 1.0f);
        blep  = -0.5f * t * t * step;
    }

    prevIn = in;
    return out;
}

const KPBuffPair* ProcessComparatorModule(uint64_t processCallID, uint32_t count, Module* module)
{
    if (!module->hasInput(ComparatorParamItemInput)) {
        std::fill_n(module->output.first->data,  count, 0.0f);
        std::fill_n(module->output.second->data, count, 0.0f);
        return &module->output;
    }

    const ComparatorType type = auValueToComparatorType(module->paramByEnum(ComparatorParamItemType));
    const AUValue threshold           = module->paramByEnum(ComparatorParamItemThreshold);
    const AUValue thresholdInputLevel = module->paramByEnum(ComparatorParamItemThresholdInputLevel);
    const AUValue hysteresis          = module->paramByEnum(ComparatorParamItemHysteresis);
    const float   halfBand            = hysteresis * 0.5f;
    const float   clipLevel           = module->paramByEnum(ComparatorParamItemClipLevel);
    const AUValue clipLevelInputLevel = module->paramByEnum(ComparatorParamItemClipLevelInputLevel);
    const bool    bandlimit           = module->paramByEnum(ComparatorParamItemBandlimit) > 0.5f;

    auto inputPair       = module->retrieveInput(ComparatorParamItemInput,          processCallID, count);
    auto thresholdPair   = module->retrieveInput(ComparatorParamItemThresholdInput, processCallID, count);
    auto clipLevelCVPair = module->retrieveInput(ComparatorParamItemClipLevelInput, processCallID, count);
    auto* state = static_cast<ComparatorState*>(module->state);

    const float* lip = inputPair->first->data;
    const float* rip = inputPair->second->data;
    const float* liopl = lip + count;
    const float* ltp  = thresholdPair->first->data;
    const float* rtp  = thresholdPair->second->data;
    const float* lclp = clipLevelCVPair->first->data;
    const float* rclp = clipLevelCVPair->second->data;
    float* lop = module->output.first->data;
    float* rop = module->output.second->data;

    for (; lip < liopl; ++lip, ++rip, ++lop, ++rop, ++ltp, ++rtp, ++lclp, ++rclp) {
        const float lEffective = std::fmax(-0.99f, std::fmin(0.99f, *ltp * thresholdInputLevel + threshold));
        const float rEffective = std::fmax(-0.99f, std::fmin(0.99f, *rtp * thresholdInputLevel + threshold));
        const float lClip = std::fmax(0.0f, std::fmin(0.99f, *lclp * clipLevelInputLevel + clipLevel));
        const float rClip = std::fmax(0.0f, std::fmin(0.99f, *rclp * clipLevelInputLevel + clipLevel));

        *lop = comparatorStep(*lip, state->prevInL, state->isHighL, state->blepL, type, lEffective, halfBand, lClip, bandlimit);
        *rop = comparatorStep(*rip, state->prevInR, state->isHighR, state->blepR, type, rEffective, halfBand, rClip, bandlimit);
    }
    return &module->output;
}
