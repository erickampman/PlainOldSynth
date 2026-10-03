//
//  CombineModule.mm
//
//  Created by Eric Kampman on 4/22/25.
//
#include "CombineModule.hpp"

const KPBuffPair* ProcessCombineModule(uint64_t processCallID, uint32_t count, Module* module)
{
    const KPBuffPair *in1Buff = nullptr;
    const KPBuffPair *in2Buff = nullptr;
    const KPBuffPair *in3Buff = nullptr;

    ParamItemCombineType combineType = static_cast<ParamItemCombineType>(module->getParamValueAsInt(CombineParamItemType));
    AUValue level = module->getParamValue(CombineParamItemLevel);

    in1Buff = module->retrieveInput(CombineParamItemInput1, processCallID, count);
    in2Buff = module->retrieveInput(CombineParamItemInput2, processCallID, count);

    int in3Result = module->retrieveInput(CombineParamItemInput3, &in3Buff, processCallID, count, nullptr);

    if (nullptr == in1Buff || nullptr == in2Buff) {
        module->clearOutputs();
        return &module->output;
    }

    const float *inL3p, *inR3p;
    if (0 == in3Result) {
        inL3p = in3Buff->first->data;
        inR3p = in3Buff->second->data;
    } else {
        // No third input: use identity value for the operation.
        if (combineType == ParamItemCombineType_Multiply) {
            const KPBuffPair *ones = KPBuffPairHelper::ones();
            inL3p = ones->first->data;
            inR3p = ones->second->data;
        } else {
            const KPBuffPair *zeroes = KPBuffPairHelper::zeroes();
            inL3p = zeroes->first->data;
            inR3p = zeroes->second->data;
        }
    }

    const float *inL1p = in1Buff->first->data;
    const float *inR1p = in1Buff->second->data;
    const float *inL2p = in2Buff->first->data;
    const float *inR2p = in2Buff->second->data;
    float *outlp = module->output.first->data;
    float *outrp = module->output.second->data;

    // Hoist the mode branch outside the sample loop so each path can be
    // vectorised independently. No function pointer or std::function overhead.
    if (combineType == ParamItemCombineType_Multiply) {
        for (uint32_t i = 0; i < count; ++i) {
            outlp[i] = inL1p[i] * inL2p[i] * inL3p[i] * level;
            outrp[i] = inR1p[i] * inR2p[i] * inR3p[i] * level;
        }
    } else if (combineType == ParamItemCombineType_Add) {
        for (uint32_t i = 0; i < count; ++i) {
            outlp[i] = (inL1p[i] + inL2p[i] + inL3p[i]) * level;
            outrp[i] = (inR1p[i] + inR2p[i] + inR3p[i]) * level;
        }
    } else {
        module->clearOutputs();
    }

    return &module->output;
}
