//
//  ModuleMap.hpp
//
//  Created by Eric Kampman on 4/19/25.
//

#ifndef ModuleMap_hpp
#define ModuleMap_hpp

#include <vector>
#include <memory>
#include "Module.hpp"

class ModuleMap {
public:
    // Add a Module to the map (takes ownership). Must only be called at init time.
    void add(AUParameterAddress id, std::unique_ptr<Module> module);

    // Get raw pointer to Module by ID. O(1), no heap access.
    // Ignores the item byte of the address (same semantics as before).
    Module* get(AUParameterAddress id) const;

    // Remove all modules and release ownership.
    void clear();

    // Reserve internal storage for n modules. Call once before the add() loop
    // to avoid reallocation during initialization.
    void reserveModules(size_t n);

    // Iterate all modules (replaces the old map() accessor).
    const std::vector<Module*>& allModules() const { return moduleList; }

private:
    // kind: 0..kMaxKind-1  (actual max used: 58 = ModuleParamKindOverdrive)
    // instance: 0..kMaxInstance-1  (actual used: 1..24 = ModuleInstance1..24)
    static constexpr size_t kMaxKind     = 128;
    static constexpr size_t kMaxInstance = 25;

    // O(1) lookup table indexed by [kind][instance].
    // Zero-initialised by default constructor — all null.
    Module* slots[kMaxKind][kMaxInstance] = {};

    // Owns the Module objects (heap lifetime).
    std::vector<std::unique_ptr<Module>> owned;

    // Flat list for iteration (no key, no unique_ptr overhead).
    std::vector<Module*> moduleList;
};


#endif /* ModuleMap_hpp */
