//
//  ADRModule.hpp — Delay-Attack-Decay-Release envelope module (gate input or MIDI note gate).
//  Ported from PolygraphModular.
//

#pragma once
#include "Module.hpp"

const void InitADRModule(uint32_t count, Module* module);
const void DeinitADRModule(Module* module);
const void SampleRateADRModule(Module* module, double newSampleRate);
const KPBuffPair* ProcessADRModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDIProcADRModule(const pm::UmpMessage& message, Module* module);
const void ParamADR(Module* module, AUParameterAddress address, AUValue value);
const void ADRModuleReset(Module* module);
