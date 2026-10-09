//
//  PitchBendConverterModule.hpp
//
//  MIDI 2.0 pitch bend → a bipolar octaves signal for an oscillator's pitch-mod input. Vendored from
//  PolygraphModularUnified's PitchBendConverterModule and extended for PlainOldSynth with MIDI 2.0
//  per-note pitch bend, per-channel bend tracking and de-zippering — see the .cpp.
//

#pragma once
#include "Module.hpp"

const void InitPitchBendConverter(uint32_t count, Module* module);
const void DeinitPitchBendConverter(Module* module);
const KPBuffPair* ProcessPitchBendConverter(uint64_t processCallID, uint32_t count, Module* module);
void MIDIPitchBendConverter(const pm::UmpMessage& message, Module* module);
const void ResetPitchBendConverter(Module* module);
