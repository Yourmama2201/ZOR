#pragma once
#include "../../memory.hpp"
#include "../../offsets.hpp"
#include "../../player.hpp"
#include <thread>
#include <chrono>

class TriggerbotAdvanced {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool teamCheck;
    bool visibilityCheck;
    bool weaponCheck;
    int delayMs;
    int holdTimeMs;
    float maxDistance;
    bool autoShoot;
    bool knifeOnly;
    bool headOnly;

public:
    TriggerbotAdvanced(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false), teamCheck(true),
        visibilityCheck(true), weaponCheck(false), delayMs(20),
        holdTimeMs(5), maxDistance(200.0f), autoShoot(false),
        knifeOnly(false), headOnly(true) {}

    void Run(int localTeam, std::vector<Player>& players, Vec3 localPos) {
        if (!enabled || !mem) return;

        uintptr_t cmdArray = mem->Read<uintptr_t>(gameBase + Offsets::CMD_ARRAY);
        if (!cmdArray) return;

        bool isShooting = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!isShooting && !autoShoot) return;

        uintptr_t crosshairEntity = 0;
        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (cameraBase) {
            crosshairEntity = mem->Read<uintptr_t>(cameraBase + 0x380);
        }

        if (!crosshairEntity) return;

        int entityId = 0;
        for (auto& player : players) {
            if (!player.IsAlive()) continue;
            if (teamCheck && player.GetTeam() == localTeam) continue;
            if (visibilityCheck && !player.IsVisible()) continue;

            Vec3 targetPos = headOnly ? player.GetHeadPos() : player.GetPosition();
            float dist = localPos.Distance(targetPos);
            if (dist > maxDistance) continue;

            Vec3 targetAngle = Math::CalculateAngle(localPos, targetPos);
            Vec3 viewAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);
            float fov = Math::GetFOV(viewAngles, targetAngle);

            if (fov < 5.0f) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

                INPUT inputs[2] = {};
                inputs[0].type = INPUT_MOUSE;
                inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                inputs[1].type = INPUT_MOUSE;
                inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

                SendInput(2, inputs, sizeof(INPUT));
                break;
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetDelay(int d) { delayMs = d; }
    void SetTeamCheck(bool t) { teamCheck = t; }
    void SetAutoShoot(bool a) { autoShoot = a; }
    void SetMaxDistance(float d) { maxDistance = d; }
    void SetHeadOnly(bool h) { headOnly = h; }
};
