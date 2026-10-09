//
//  CCConverterModule.hpp
//
//  A chosen MIDI CC number → a held value (Boolean gate or continuous), optionally scaling a
//  patched input as a VCA. Ported from PolygraphModular's CCConverterModule (AUv3); MIDI is the
//  pm::UmpMessage form.
//

#pragma once
#include "Module.hpp"

const void InitCCConverter(uint32_t count, Module* module);
const void DeinitCCConverter(Module* module);
const KPBuffPair* ProcessCCConverter(uint64_t processCallID, uint32_t count, Module* module);
void MIDICCConverter(const pm::UmpMessage& message, Module* module);
const void ResetCCConverter(Module* module);
