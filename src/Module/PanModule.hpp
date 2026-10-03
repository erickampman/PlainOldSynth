// PanModule.hpp — true stereo panner: pans an L/R pair by Position ∈ [-1, +1], collapsing to
// mono (scaled by 0.5 so loudness matches) at either extreme. Ported from PolygraphModular.

#pragma once
#include "Module.hpp"

const KPBuffPair* ProcessPanModule(uint64_t processCallID, uint32_t count, Module* module);
