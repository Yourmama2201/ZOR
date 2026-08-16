#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include "math.hpp"
#include <string>
#include <vector>
#include <cctype>

class Player {
private:
    MemoryManager& mem;
    uintptr_t ptr;
    Vec3 position;
    Vec3 headPos;
    Vec3 footPos;
    Vec3 velocity;
    int team;
    float health;
    float armor;
    bool alive;
    float distance;
    std::string name;
    int weaponIndex;
    bool visible;
    uintptr_t boneBase;
    float stanceHeight;

public:
    Player(MemoryManager& memory, uintptr_t address) : mem(memory), ptr(address) {
        Update();
    }

    void Update() {
        if (!ptr) return;

        team = mem.Read<int>(ptr + Offsets::PLAYER_TEAM);
        health = mem.Read<float>(ptr + Offsets::PLAYER_HEALTH);
        armor = mem.Read<float>(ptr + Offsets::PLAYER_ARMOR);

        // Check if dead using updated offsets
        bool dead1 = mem.Read<uint8_t>(ptr + Offsets::PLAYER_DEAD_1) != 0;
        bool dead2 = mem.Read<uint8_t>(ptr + Offsets::PLAYER_DEAD_2) != 0;
        alive = health > 0 && health < 1000 && !dead1 && !dead2;

        position = mem.Read<Vec3>(ptr + Offsets::PLAYER_POS);
        velocity = mem.Read<Vec3>(ptr + 0x12B0);
        weaponIndex = mem.Read<int>(ptr + Offsets::PLAYER_WEAPON);
        name = mem.ReadString(ptr + 0x800, 32);

        boneBase = mem.Read<uintptr_t>(ptr + Offsets::BONE_BASE_OFFSET);
        if (boneBase) {
            headPos = GetBonePos(Offsets::HEAD);
            footPos = GetBonePos(Offsets::ROOT);
        }

        visible = (mem.Read<uint8_t>(ptr + Offsets::VISIBLE_BIT) & 0x1) != 0;
    }

    bool IsAlive() const { return alive && ptr != 0; }
    bool IsVisible() const { return visible; }
    bool IsOnTeam(int localTeam) const { return team == localTeam; }

    uintptr_t GetPtr() const { return ptr; }
    Vec3 GetPosition() const { return position; }
    Vec3 GetHeadPos() const { return headPos; }
    Vec3 GetFootPos() const { return footPos; }
    Vec3 GetBonePos(int boneId) const { return const_cast<Player*>(this)->GetBonePos(boneId); }
    Vec3 GetVelocity() const { return velocity; }
    int GetTeam() const { return team; }
    float GetHealth() const { return health; }
    float GetArmor() const { return armor; }
    float GetDistance() const { return distance; }
    std::string GetName() const { return name; }
    int GetWeapon() const { return weaponIndex; }

    // Resolve the player's weapon name via WEAPON_DEFS (requires gameBase).
    // Used for riot-shield detection + ESP weapon labels.
    std::string GetWeaponName(uintptr_t gameBase) const {
        if (!weaponIndex || weaponIndex < 0) return "";
        uintptr_t weaponDefs = mem.Read<uintptr_t>(gameBase + Offsets::WEAPON_DEFS);
        if (!weaponDefs) return "";
        int count = mem.Read<int>(weaponDefs + 0x8);
        if (count <= 0 || count > 600 || weaponIndex >= count) return "";
        uintptr_t entry = mem.Read<uintptr_t>(weaponDefs + 0x20 + (uintptr_t)(weaponIndex * 0x8));
        if (!entry) return "";
        std::string name = mem.ReadString(entry + 0x10, 48);
        return name;
    }

    bool HasRiotShield(uintptr_t gameBase) const {
        std::string w = GetWeaponName(gameBase);
        if (w.empty()) return false;
        std::string lw;
        for (char c : w) lw += (char)tolower(c);
        return lw.find("shield") != std::string::npos ||
            lw.find("riot") != std::string::npos;
    }

    void SetDistance(const Vec3& localPos) {
        distance = position.Distance(localPos);
    }

    Vec3 GetBonePos(int boneId) {
        if (!boneBase) return Vec3();
        uintptr_t bonePtr = mem.Read<uintptr_t>(boneBase + 0x8 * boneId);
        if (!bonePtr) return Vec3();

        // Try the known layout first: position at fixed offset within bone struct.
        Vec3 pos = mem.Read<Vec3>(bonePtr + 0x10);
        if (pos.x != 0 || pos.y != 0 || pos.z != 0) return pos;
        return mem.Read<Vec3>(bonePtr + 0x30 * boneId + 0x10);
    }

    std::vector<Vec3> GetSkeleton() {
        std::vector<Vec3> bones;
        if (!boneBase) return bones;
        for (int i = 0; i < 20; i++) {
            bones.push_back(GetBonePos(i));
        }
        return bones;
    }
};