// QuadCombineModule.hpp — 4-input version of Combine (Add/Multiply). Unconnected inputs use the
// operation's identity value (0 for Add, 1 for Multiply). Stateless. See .cpp. Ported from
// PolygraphModular.

#pragma once
#include "Module.hpp"

const KPBuffPair* ProcessQuadCombineModule(uint64_t processCallID, uint32_t count, Module* module);
