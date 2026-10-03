//
//  SinOscillatorModule.hpp
//
//  Created by Eric Kampman on 4/11/25.
//

#ifndef SinOscillatorModule_hpp
#define SinOscillatorModule_hpp

#include "Module.hpp"

const void InitSinOscillator(uint32_t count, Module* module);
const void DeinitSinOscillator(Module* module);
const void ResetSinOscillator(Module* module);

const KPBuffPair* ProcessSinOscillator(uint64_t processCallID, uint32_t count, Module* module);
void MIDISinOscillator(const pm::UmpMessage& message, Module* module);

const void SampleRateSinOscillator(Module* module, double newSampleRate);

#endif /* SinOscillatorModule_hpp */
