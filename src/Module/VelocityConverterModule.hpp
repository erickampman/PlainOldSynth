//
//  VelocityConverterModule.hpp
//
//  MIDI note velocity → a held control value, with a floor (Min) and a response curve. Vendored from
//  PolygraphModularUnified's VelocityConverterModule and extended with de-zippering — see the .cpp.
//

#pragma once
#include "Module.hpp"

const void InitVelocityConverter(uint32_t count, Module* module);
const void DeinitVelocityConverter(Module* module);
const KPBuffPair* ProcessVelocityConverter(uint64_t processCallID, uint32_t count, Module* module);
void MIDIVelocityConverter(const pm::UmpMessage& message, Module* module);
const void ResetVelocityConverter(Module* module);
