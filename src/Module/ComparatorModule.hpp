// ComparatorModule.hpp — Schmitt-trigger comparator with hysteresis and CV-able threshold/clip.
// Type: Full Clip (±clip), Top (clip high / passthrough low), Bottom (passthrough high / −clip low).
// Ported from PolygraphModular.

#pragma once
#include "Module.hpp"

const void InitComparatorModule(uint32_t count, Module* module);
const void DeinitComparatorModule(Module* module);
const void ResetComparatorModule(Module* module);
const KPBuffPair* ProcessComparatorModule(uint64_t processCallID, uint32_t count, Module* module);
