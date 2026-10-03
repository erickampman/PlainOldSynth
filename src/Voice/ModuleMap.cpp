//
//  ModuleMap.cpp
//
//  Created by Eric Kampman on 4/19/25.
//

#include "ModuleMap.hpp"

void ModuleMap::reserveModules(size_t n) {
    owned.reserve(n);
    moduleList.reserve(n);
}

void ModuleMap::add(AUParameterAddress id, std::unique_ptr<Module> module) {
    const size_t kind     = MODULE_KIND_FROM_ADDRESS(id);
    const size_t instance = MODULE_INSTANCE_FROM_ADDRESS(id);

    // Bounds check — asserts in debug, silent clamp in release.
    // Both values are 8-bit fields so exceeding kMaxKind/kMaxInstance would
    // require a new module kind or >24 instances, neither of which is expected.
    if (kind >= kMaxKind || instance >= kMaxInstance) {
        // Programming error: increase kMaxKind/kMaxInstance if needed.
        return;
    }

    Module* raw = module.get();
    slots[kind][instance] = raw;
    moduleList.push_back(raw);
    owned.push_back(std::move(module));
}

Module* ModuleMap::get(AUParameterAddress id) const {
    // Extracting kind and instance ignores the item byte naturally,
    // preserving the same semantics as the old MODULE_BASE_ADDRESS mask.
    const size_t kind     = MODULE_KIND_FROM_ADDRESS(id);
    const size_t instance = MODULE_INSTANCE_FROM_ADDRESS(id);

    if (kind >= kMaxKind || instance >= kMaxInstance) {
        return nullptr;
    }
    return slots[kind][instance];
}

void ModuleMap::clear() {
    for (auto& row : slots) {
        for (auto& slot : row) {
            slot = nullptr;
        }
    }
    moduleList.clear();
    owned.clear(); // destructs all Module objects
}
