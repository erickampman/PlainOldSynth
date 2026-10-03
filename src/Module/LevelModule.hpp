// LevelModule.hpp — a trivial gain stage: output = input × level. In IFSynth it scales the sine
// oscillator's audible contribution to the osc mixer, leaving the sine's raw output free to drive
// the IFS hard-sync input (so sync works even when the sine's mix level is 0).

#pragma once
#include "Module.hpp"

const KPBuffPair* ProcessLevelModule(uint64_t processCallID, uint32_t count, Module* module);
