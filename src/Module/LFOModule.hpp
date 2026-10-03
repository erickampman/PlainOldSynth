// LFOModule.hpp — low-frequency control oscillator: Sine / Triangle / Square / Impulse, exponential
// rate (0 = 1 Hz), stereo L/R phase offset, and a reset-trigger input. Bipolar (−1..1) except Impulse
// (0/1). Analytic reimplementation of PM's LFO (PM's version used Accelerate + wavetables); a
// control-rate LFO needs no band-limiting, so the shapes are computed directly. See LFOModule.cpp.

#pragma once
#include "Module.hpp"

const void InitLFOModule(uint32_t count, Module* module);
const void DeinitLFOModule(Module* module);
const void ResetLFOModule(Module* module);
const KPBuffPair* ProcessLFOModule(uint64_t processCallID, uint32_t count, Module* module);
