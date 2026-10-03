//
//  LPFilterModule.hpp — per-voice 24 dB/oct Butterworth low-pass filter (IFSynth port of PM's
//  LPFilterModule). Sits after the IFS oscillator, before the VCA. Cutoff is an absolute pitch
//  (fed through the voice's shared PitchConverter); Q adds resonance. Freq/Q modulation inputs
//  exist for a future LPF-mod panel but are dormant until connected.
//

#pragma once
#include "Module.hpp"

const void InitLPFilterModule(uint32_t count, Module* module);
const void DeinitLPFilterModule(Module* module);
const void SampleRateLPFilterModule(Module* module, double newSampleRate);
const KPBuffPair* ProcessLPFilterModule(uint64_t processCallID, uint32_t count, Module* module);
