//
//  PitchConverterModule.hpp
//
//  Created by Eric Kampman on 5/10/25.
//

#ifndef PitchConverterModule_hpp
#define PitchConverterModule_hpp

#include "Module.hpp"

const void InitPitchConverter(uint32_t count, Module* module);
const void DeinitPitchConverter(Module* module);

const KPBuffPair* ProcessPitchConverter(uint64_t processCallID, uint32_t count, Module* module);
void MIDIPitchConverter(const pm::UmpMessage& message, Module* module);


#endif /* PitchConverterModule_hpp */
