#pragma once
#include "../../memory.hpp"
#include "../../offsets.hpp"
#include <vector>
#include <string>
#include <unordered_map>

class NameSystem {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    std::unordered_map<int, std::string> nameCache;

public:
    NameSystem(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base) {}

    std::string GetPlayerName(int index) {
        auto it = nameCache.find(index);
        if (it != nameCache.end()) return it->second;

        uintptr_t nameArray = mem->Read<uintptr_t>(gameBase + Offsets::NAME_ARRAY);
        if (!nameArray) return "Unknown";

        uintptr_t nameEntry = nameArray + (index * Offsets::NAME_ARRAY_SIZE);
        std::string name = mem->ReadString(nameEntry, 32);

        if (name.empty()) name = "Unknown";
        nameCache[index] = name;
        return name;
    }

    std::string GetWeaponName(uintptr_t weaponDefPtr) {
        if (!weaponDefPtr) return "None";
        std::string name = mem->ReadString(weaponDefPtr + 0x10, 48);
        return name.empty() ? "Unknown Weapon" : name;
    }

    std::vector<std::string> GetAllWeaponNames() {
        std::vector<std::string> names;
        uintptr_t weaponDefs = mem->Read<uintptr_t>(gameBase + Offsets::WEAPON_DEFS);
        if (!weaponDefs) return names;

        int count = mem->Read<int>(weaponDefs + 0x8);
        if (count > 500) count = 500;

        for (int i = 0; i < count; i++) {
            uintptr_t entry = mem->Read<uintptr_t>(weaponDefs + 0x20 + (i * 0x8));
            if (!entry) continue;
            names.push_back(GetWeaponName(entry));
        }
        return names;
    }

    void ClearCache() { nameCache.clear(); }
};
