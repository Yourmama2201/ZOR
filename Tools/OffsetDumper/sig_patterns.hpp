#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

struct SigPattern {
    std::string name;
    std::string hexPattern;
    int extra;
    bool relative;
    std::string description;

    SigPattern(const std::string& n, const std::string& hex, int ext = 0,
        bool rel = false, const std::string& desc = "")
        : name(n), hexPattern(hex), extra(ext), relative(rel), description(desc) {}
};

static std::vector<SigPattern> GetPatterns() {
    return {
        {"CAMERA_BASE",
        "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? 48 85 C9",
        3, true, "Camera/viewMatrix base pointer"},

        {"DISTRIBUTE",
        "48 8B 05 ? ? ? ? 48 8B 48 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ?",
        3, true, "Entity distribution list pointer"},

        {"BONE_BASE",
        "48 8B 05 ? ? ? ? 48 8B 08 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88",
        3, true, "Bone matrix base pointer"},

        {"VIEW_ANGLES",
        "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? 48 83 C4 ? 5B C3",
        3, true, "View angles (pitch/yaw)"},

        {"LOCAL_POS",
        "F3 0F 10 05 ? ? ? ? F3 0F 11 05 ? ? ? ? F3 0F 10 0D ? ? ? ?",
        3, true, "Local player world position"},

        {"CMD_ARRAY",
        "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?",
        3, true, "Command/input array pointer"},

        {"WEAPON_DEFS",
        "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8D 55",
        3, true, "Weapon definitions table"},

        {"ACTIVE_STATE",
        "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ? E8",
        3, true, "Active game state (exfil, contract data)"},

        {"NAME_ARRAY",
        "48 8B 05 ? ? ? ? 48 8D 14 80 48 8B 04 D0 48 85 C0 74 ?",
        3, true, "Player name array"},

        {"GAME_MODE",
        "83 3D ? ? ? ? ? 75 ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? E8",
        2, true, "Current game mode identifier"},

        {"LOOT_PTR",
        "48 8B 0D ? ? ? ? 48 8B 49 ? 48 85 C9 74 ? 48 8B 01 48 8B 40 ? 48 8B 88 ? ? ? ?",
        3, true, "Loot item list pointer"},

        {"REF_DEF",
        "48 8B 05 ? ? ? ? 48 8D 0C 80 48 8B 04 C8 48 85 C0 74 ?",
        3, true, "Reference definition pointer"},

        {"CLIENT_INFO",
        "48 8B 05 ? ? ? ? 48 33 05 ? ? ? ? 48 8D 0D ? ? ? ? 48 89 05",
        3, true, "Encrypted client info pointer"},

        {"LOCAL_INDEX",
        "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 80 ? ? ? ? 48 8B 5C 24 ?",
        3, true, "Local player index pointer"},

        {"TIMESTAMP",
        "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 ? 8B 80 ? ? ? ? C3",
        3, true, "Game timestamp pointer"},
    };
}
