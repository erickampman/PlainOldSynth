//
//  ChannelPressureModule.hpp
//
//  Pressure (aftertouch) → a 0..1 control value. Vendored from PolygraphModularUnified's
//  ChannelPressureModule and extended for PlainOldSynth with polyphonic key pressure, per-channel
//  tracking and de-zippering — see the .cpp.
//

#pragma once
#include "Module.hpp"

const void InitChannelPressure(uint32_t count, Module* module);
const void DeinitChannelPressure(Module* module);
const KPBuffPair* ProcessChannelPressure(uint64_t processCallID, uint32_t count, Module* module);
void MIDIChannelPressure(const pm::UmpMessage& message, Module* module);
const void ResetChannelPressure(Module* module);
