//
//  SlewModule.mm
//
//  Created by Eric Kampman on 5/11/25.
//

#include "SlewModule.hpp"


inline float slewSample(float input, float prev, float risePerSample, float fallPerSample);


struct SlewModuleHelperStruct {
    float prevL;
    float prevR;

    explicit SlewModuleHelperStruct()
        : prevL(0), prevR(0) {}
};

const void InitSlewModule(uint32_t /*count*/, Module* module)
{
    auto* state = new SlewModuleHelperStruct();
    module->state = state;

}
const void DeinitSlewModule(Module* module)
{
    delete static_cast<SlewModuleHelperStruct*>(module->state);
    module->state = nullptr;
}

const KPBuffPair* ProcessSlewModule(uint64_t processCallID, uint32_t count, Module* module)
{
    const KPBuffPair* inputBuffer = module->retrieveInput(SlewParamItemInput, processCallID, count);

    AUValue rise = module->paramByEnum(SlewParamItemRise); // seconds for +1 unit
    AUValue fall = module->paramByEnum(SlewParamItemFall); // seconds for -1 unit

    // ✓ robust mapping: step_per_sample = (1/SR) / seconds_per_unit
    const float invSR   = static_cast<float>(module->mFrequencyScale); // 1.0 / mSampleRate
    const float minSecs = 1e-4f;  // 0.1 ms floor; treats 0 as "almost instantaneous"

    const float risePerSample = invSR / std::max(rise, minSecs);
    const float fallPerSample = invSR / std::max(fall, minSecs);

    auto* state = static_cast<SlewModuleHelperStruct*>(module->state);
    float prevL = state->prevL, prevR = state->prevR;

    float* inL = inputBuffer->first->data;
    float* inR = inputBuffer->second->data;
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    for (uint32_t i = 0; i < count; ++i) {
        prevL = slewSample(inL[i], prevL, risePerSample, fallPerSample);
        prevR = slewSample(inR[i], prevR, risePerSample, fallPerSample);
        outL[i] = prevL;
        outR[i] = prevR;
    }

    state->prevL = prevL;
    state->prevR = prevR;
    return &module->output;
}

inline float slewSample(float input, float prev, float risePerSample, float fallPerSample)
{
    float delta = input - prev;
    
    if (delta == 0) {
        prev = input;
    }
    else if (delta > 0) {
        prev += risePerSample;
        if (prev > input) { // overshoot
            prev = input;
        }
    } else {
        prev -= fallPerSample;
        if (prev < input) { // undershoot
            prev = input;
        }
    }
    
    return prev;
}
