//
//  FinalModule.hpp
//
//  Created by Eric Kampman on 4/13/25.
//

#ifndef FinalModule_h
#define FinalModule_h

#include "Module.hpp"

const void InitFinalModule(uint32_t count, Module* module);
const void DeinitFinalModule(Module* module);
const void SampleRateFinalModule(Module* module, double newSampleRate);

const KPBuffPair* ProcessFinalModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDIFinalModule(const pm::UmpMessage& message, Module* module);

#endif /* FinalModule_h */
