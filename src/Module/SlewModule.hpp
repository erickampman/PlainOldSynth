//
//  SlewModule.hpp
//
//  Created by Eric Kampman on 5/11/25.
//

#ifndef SlewModule_hpp
#define SlewModule_hpp

#include "Module.hpp"

const void InitSlewModule(uint32_t count, Module* module);
const void DeinitSlewModule(Module* module);

const KPBuffPair* ProcessSlewModule(uint64_t processCallID, uint32_t count, Module* module);

#endif /* SlewModule_hpp */
