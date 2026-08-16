#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../player.hpp"
#include "../../offsets.hpp"
#include <vector>
#include <algorithm>

class SilentAim {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool headshotOnly;
    bool wallBang;
    float fov;
    bool visibleCheck;
    bool skipKnocked;

public:
    SilentAim(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false), headshotOnly(true),
        wallBang(false), fov(15.0f), visibleCheck(true),
        skipKnocked(true) {}

    void Run(std::vector<Player>& players, Vec3 localPos, int localTeam) {
        if (!enabled || !mem || players.empty()) return;

        uintptr_t cmdArray = mem->Read<uintptr_t>(gameBase + Offsets::CMD_ARRAY);
        if (!cmdArray) return;

        int cmdCount = mem->Read<int>(cmdArray + 0x8);
        if (cmdCount == 0) return;

        Vec3 viewAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);

        Player* bestTarget = nullptr;
        float bestFov = fov;
        Vec3 bestAngle;

        for (auto& player : players) {
            if (player.GetTeam() == localTeam) continue;
            if (!player.IsAlive()) continue;
            if (skipKnocked && !player.IsVisible()) continue;
            if (visibleCheck && !player.IsVisible()) continue;
            if (!wallBang && !player.IsVisible()) continue;

            Vec3 targetPos = headshotOnly ? player.GetHeadPos() : player.GetPosition();
            Vec3 targetAngle = Math::CalculateAngle(localPos, targetPos);
            float fovDist = Math::GetFOV(viewAngles, targetAngle);

            if (fovDist < bestFov) {
                bestFov = fovDist;
                bestTarget = &player;
                bestAngle = targetAngle;
            }
        }

        if (!bestTarget) return;

        uintptr_t cmd = mem->Read<uintptr_t>(cmdArray + 0x20);
        if (!cmd) return;

        mem->Write<Vec3>(cmd + 0x80, bestAngle);
        mem->Write<Vec3>(cmd + 0x8C, bestAngle);

        if (wallBang) {
            mem->Write<uint8_t>(cmd + 0xA0, 1);
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetHeadshotOnly(bool h) { headshotOnly = h; }
    void SetWallBang(bool w) { wallBang = w; }
    void SetFOV(float f) { fov = f; }
    void SetVisibleCheck(bool v) { visibleCheck = v; }
};
