#pragma once
#include "signatures.hpp"
#include "offsets.hpp"
#include <unordered_map>

class OffsetResolver {
private:
    SigScanner scanner;
    std::unordered_map<std::string, uintptr_t> resolved;
    bool ready;

    void SetupMW2Signatures() {
        scanner.AddSignature(Signature("CAMERA_BASE",
            "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? 48 85 C9",
            3, true));

        scanner.AddSignature(Signature("DISTRIBUTE",
            "48 8B 05 ? ? ? ? 48 8B 48 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ?",
            3, true));

        scanner.AddSignature(Signature("BONE_BASE",
            "48 8B 05 ? ? ? ? 48 8B 08 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88",
            3, true));

        scanner.AddSignature(Signature("VIEW_ANGLES",
            "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? 48 83 C4 ? 5B C3",
            3, true));

        scanner.AddSignature(Signature("LOCAL_POS",
            "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? F3 0F 10 0D ? ? ? ?",
            3, true));

        scanner.AddSignature(Signature("CMD_ARRAY",
            "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?",
            3, true));

        scanner.AddSignature(Signature("WEAPON_DEFS",
            "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8D 55",
            3, true));

        scanner.AddSignature(Signature("ACTIVE_STATE",
            "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? E8",
            3, true));

        scanner.AddSignature(Signature("NAME_ARRAY",
            "48 8B 05 ? ? ? ? 48 8D 14 80 48 8B 04 D0 48 85 C0 74 ?",
            3, true));

        scanner.AddSignature(Signature("GAME_MODE",
            "83 3D ? ? ? ? ? 75 ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? E8",
            2, true));
    }

public:
    OffsetResolver() : ready(false) {}

    bool Initialize() {
        if (ready) return true;
        if (!scanner.Initialize()) return false;
        bool ok = scanner.ScanAll();

        auto results = scanner.GetResults();
        for (auto& [name, offset] : results) {
            resolved[name] = offset;
        }

        ready = ok;
        return ok;
    }

    uintptr_t Get(const std::string& name, uintptr_t fallback = 0) {
        auto it = resolved.find(name);
        if (it != resolved.end()) return it->second;
        return fallback;
    }

    bool IsReady() const { return ready; }
    size_t ResolvedCount() const { return scanner.ResolvedCount(); }

    // Try runtime sig first, fall back to hardcoded Offsets:: value
    uintptr_t ResolveWithFallback(const std::string& name, uintptr_t hardcoded) {
        uintptr_t sig = Get(name);
        if (sig) return sig;
        return hardcoded;
    }

    // Resolve all key pointers, falling back to hardcoded offsets
    void ResolveAllPointers(uintptr_t& cameraBase, uintptr_t& distribute,
        uintptr_t& boneBase, uintptr_t& cmdArray,
        uintptr_t& weaponDefs, uintptr_t& activeState,
        uintptr_t& nameArray, uintptr_t& gameMode) {

        cameraBase = ResolveWithFallback("CAMERA_BASE", Offsets::CAMERA_BASE);
        distribute = ResolveWithFallback("DISTRIBUTE", Offsets::DISTRIBUTE);
        boneBase = ResolveWithFallback("BONE_BASE", Offsets::BONE_BASE);
        cmdArray = ResolveWithFallback("CMD_ARRAY", Offsets::CMD_ARRAY);
        weaponDefs = ResolveWithFallback("WEAPON_DEFS", Offsets::WEAPON_DEFS);
        activeState = ResolveWithFallback("ACTIVE_STATE", Offsets::ACTIVE_STATE);
        nameArray = ResolveWithFallback("NAME_ARRAY", Offsets::NAME_ARRAY);
        gameMode = ResolveWithFallback("GAME_MODE", Offsets::GAME_MODE);
    }
};
