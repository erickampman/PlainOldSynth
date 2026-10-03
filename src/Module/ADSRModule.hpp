//
//  ADSRModule.hpp
//
//  Created by Eric Kampman on 5/31/25.
//

#ifndef ADSRModule_hpp
#define ADSRModule_hpp

#include "Module.hpp"

const void InitADSRModule(uint32_t count, Module* module);
const void DeinitADSRModule(Module* module);

const void SampleRateADSRModule(Module* module, double newSampleRate);

const KPBuffPair* ProcessADSRModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDIProcADSRModule(const pm::UmpMessage& message, Module* module);

const void ParamADSR(Module *module, AUParameterAddress address, AUValue value);
const void ADSRModuleReset(Module* module);

#endif /* ADSRModule_hpp */
