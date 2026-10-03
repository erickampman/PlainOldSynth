// PanModule.cpp — out = pan(inL, inR, Position + ModInput). See .hpp.

#include "PanModule.hpp"
#include "pm_params.h"

#include <algorithm>

// Pan a stereo signal by position p ∈ [-1, +1].
//
// At p = 0: full stereo pass-through.
// At p = -1: mono collapsed to left channel only, (inL + inR) * 0.5.
// At p = +1: mono collapsed to right channel only, (inL + inR) * 0.5.
//
// Between 0 and an extreme, the image narrows linearly:
//   p ∈ [-1, 0], t = -p:
//     outL = inL * (1 - t*0.5) + inR * (t*0.5)
//     outR = inR * (1 - t)
//
//   p ∈ [0, +1], t = p:
//     outL = inL * (1 - t)
//     outR = inR * (1 - t*0.5) + inL * (t*0.5)
//
// The mono endpoint scales by 0.5 so loudness matches the stereo input.

const KPBuffPair* ProcessPanModule(uint64_t processCallID, uint32_t count, Module* module)
{
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    if (!module->hasInput(PanParamItemInput)) {
        std::fill_n(outL, count, 0.0f);
        std::fill_n(outR, count, 0.0f);
        return &module->output;
    }

    auto inputPair = module->retrieveInput(PanParamItemInput,    processCallID, count);
    auto modPair   = module->retrieveInput(PanParamItemModInput, processCallID, count);

    const float* lip = inputPair->first->data;
    const float* rip = inputPair->second->data;

    // CV position: read left channel of mod input only — pan is a scalar.
    // Sum static position param with per-sample mod, clamped to [-1, 1].
    const float* modp = modPair->first->data;
    const float  base = module->paramByEnum(PanParamItemPosition);

    const float* end = outL + count;

    for (; outL < end; ++outL, ++outR, ++lip, ++rip, ++modp) {
        const float p = std::clamp(base + *modp, -1.0f, 1.0f);
        const float inL = *lip;
        const float inR = *rip;

        if (p < 0.0f) {
            const float t = -p;                              // t ∈ (0, 1]
            *outL = inL * (1.0f - t * 0.5f) + inR * (t * 0.5f);
            *outR = inR * (1.0f - t);
        } else if (p > 0.0f) {
            const float t = p;                               // t ∈ (0, 1]
            *outL = inL * (1.0f - t);
            *outR = inR * (1.0f - t * 0.5f) + inL * (t * 0.5f);
        } else {
            *outL = inL;
            *outR = inR;
        }
    }

    return &module->output;
}
