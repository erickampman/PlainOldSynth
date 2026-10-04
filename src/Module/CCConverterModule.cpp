//
//  CCConverterModule.cpp
//
//  A chosen MIDI CC number → a held value (Boolean gate or continuous), optionally scaling a
//  patched input as a VCA (out = in × (offset + amount·cc)). Ported from PolygraphModular's
//  CCConverterModule (AUv3); the CC index/value arrive in pm::UmpMessage::ccIndex / ccData.
//

#include "CCConverterModule.hpp"
#include "pm_midi.h"
#include <algorithm>

struct CCConverterHelperStruct {
    double curValue = 0.0;
};

const void InitCCConverter(uint32_t /*count*/, Module* module)
{
    module->state = new CCConverterHelperStruct();
}

const void DeinitCCConverter(Module* module)
{
    delete static_cast<CCConverterHelperStruct*>(module->state);
    module->state = nullptr;
}

const KPBuffPair* ProcessCCConverter(uint64_t processCallID, uint32_t count, Module* module)
{
    auto* state = static_cast<CCConverterHelperStruct*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    const double amount = module->paramByEnum(CCParamItemAmount);
    const double offset = module->paramByEnum(CCParamItemOffset);
    const double shaped = offset + amount * state->curValue;   // amount=1, offset=0 -> just the CC

    // With a signal patched, the CC scales it (VCA): out = in × shaped. Unpatched, in = 1, so it's
    // a plain CC source (out = shaped). Stereo: each channel scaled independently.
    if (module->isInputConnected(CCParamItemInput)) {
        const KPBuffPair* in = module->retrieveInput(CCParamItemInput, processCallID, count);
        const float* inL = in->first->data;
        const float* inR = in->second->data;
        for (uint32_t i = 0; i < count; ++i) {
            outL[i] = (float)(inL[i] * shaped);
            outR[i] = (float)(inR[i] * shaped);
        }
    } else {
        std::fill_n(outL, count, (float)shaped);
        std::fill_n(outR, count, (float)shaped);
    }

    return &module->output;
}

void MIDICCConverter(const pm::UmpMessage& message, Module* module)
{
    auto* state = static_cast<CCConverterHelperStruct*>(module->state);

    const uint8_t index = static_cast<uint8_t>(module->paramByEnum(CCParamItemNumber));
    const bool    isBool = module->paramByEnum(CCParamItemIsBoolean) >= 0.5f;

    if (pm::isChannelVoice2(message) && message.status == pm::kStatusControlChange) {
        if (message.ccIndex == index) {
            if (isBool) {
                state->curValue = (message.ccData != 0) ? 1.0 : 0.0;
            } else {
                constexpr double kMidi2CCNormalization = 1.0 / 4294967295.0;
                state->curValue = message.ccData * kMidi2CCNormalization;
            }
        }
    }
}
