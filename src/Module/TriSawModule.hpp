// TriSawModule.hpp — variable tri↔saw oscillator (the "Peak" control morphs the apex position:
// 0.5 = symmetric triangle, →0/→1 = sawtooth). Hard sync + DX7-style phase self-modulation, like
// SinOsc. Ported from PM (the Accelerate/vDSP delta precompute is replaced by the per-sample
// PitchConverter path our other oscillators use). See TriSawModule.cpp.

#pragma once
#include "Module.hpp"

const void InitTriSawOscillator(uint32_t count, Module* module);
const void DeinitTriSawOscillator(Module* module);
const void ResetTriSawOscillator(Module* module);
void MIDITriSawOscillator(const pm::UmpMessage& message, Module* module);
const KPBuffPair* ProcessTriSawOscillator(uint64_t processCallID, uint32_t count, Module* module);
