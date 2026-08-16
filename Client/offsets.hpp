#pragma once
#include <cstdint>

namespace Offsets {
    // ============ CLIENT ============
    constexpr uintptr_t CLIENT_INFO = 0x1198C7F8;
    constexpr uintptr_t CLIENT_BASE_OFFSET = 0x1908A0;
    constexpr uintptr_t CMD_ARRAY = 0xCD3D810;
    constexpr uintptr_t ACTIVE_STATE = 0xF27D528;
    constexpr uintptr_t BONE_BASE = 0xCB97E48;
    constexpr uintptr_t CAMERA_MATRIX = 0xCD1031C; // view matrix, static (CAMERA_BASE chain is dead/null)
    constexpr uintptr_t CAMERA_MATRIX_ALT = 0xCD24ADC;
    constexpr uintptr_t CAMERA_POS = 0x218; // camera position offset (in camera struct)
    constexpr uintptr_t CAMERA_BASE = 0x122B1380;  // DEAD (null) as of 08/2026 update
    constexpr uintptr_t DISTRIBUTE = 0xA88DD10;
    constexpr uintptr_t VISIBLE_BIT = 0x8700;
    constexpr uintptr_t VIEW_ANGLES = 0x12C00;
    constexpr uintptr_t LOCAL_POS = 0x12C08;
    constexpr uintptr_t LOCAL_INDEX = 0x105150;
    constexpr uintptr_t LOCAL_INDEX_POS = 0x2F0;
    constexpr uintptr_t GAME_MODE = 0xF07D000;
    constexpr uintptr_t WEAPON_DEFS = 0x11886930;
    constexpr uintptr_t WEAPON_INIT = 0x11A4;
    constexpr uintptr_t NAME_ARRAY = 0x119C48F8;
    constexpr uintptr_t NAME_ARRAY_POS = 0x5E80;
    constexpr uintptr_t NAME_ARRAY_SIZE = 0xD8;
    constexpr uintptr_t REF_DEF = 0x119A2DA0;
    constexpr uintptr_t TIMESTAMP = 0x6A18E58F;
    constexpr uintptr_t RECOIL = 0x0;

    // ============ LOOT ============
    constexpr uintptr_t LOOT_PTR = 0xB8BD04C;

    // ============ PLAYER ============
    constexpr uintptr_t PLAYER_SIZE = 0x13D58;
    constexpr uintptr_t PLAYER_VALID = 0x1236;
    constexpr uintptr_t PLAYER_POS = 0x1740;
    constexpr uintptr_t PLAYER_TEAM = 0x2F1;
    constexpr uintptr_t PLAYER_WEAPON = 0x1312;
    constexpr uintptr_t PLAYER_DEAD_1 = 0x1390;
    constexpr uintptr_t PLAYER_DEAD_2 = 0xAB0;
    constexpr uintptr_t PLAYER_HEALTH = 0x520;
    constexpr uintptr_t PLAYER_ARMOR = 0x524;

    // ============ BONE ============
    constexpr uintptr_t BONE_BASE_OFFSET = 0x17D760;
    constexpr uintptr_t BONE_SIZE = 0x188;
    constexpr uintptr_t BONE_OFFSET = 0xD8;
    constexpr uintptr_t BONE_DISTRIBUTE = 0xA88DD10;
    constexpr uintptr_t BONE_VISIBLE_OFFSET = 0xA90;
    constexpr uintptr_t BONE_VISIBLE = 0x244B760;

    // ============ CENTITY ============
    constexpr uintptr_t CENTITY_SIZE = 0xE18;
    constexpr uintptr_t CENTITY_VALID = 0x88;
    constexpr uintptr_t CENTITY_ORIGIN = 0xDAC;
    constexpr uintptr_t CENTITY_YAW = 0xDD4;

    // ============ BONE IDs ============
    enum BoneID : int {
        ROOT = 0,
        SPINE1 = 6,
        NECK = 7,
        HEAD = 8,
        LEFT_FOOT = 9,
        LEFT_SHOULDER = 10,
        LEFT_ELBOW = 11,
        LEFT_HAND = 12,
        RIGHT_KNEE = 13,
        RIGHT_SHOULDER = 14,
        RIGHT_ELBOW = 15,
        RIGHT_HAND = 16,
        RIGHT_FOOT = 17,
        LEFT_KNEE = 1,
        CHEST = 3,
        SPINE2 = 4,
        SPINE3 = 5,
        LEFT_HIP = 2,
        RIGHT_HIP = 18,
    };
}