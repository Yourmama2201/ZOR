#pragma once
#include <windows.h>
#include <thread>
#include <chrono>
#include "memory.hpp"
#include "offsets.hpp"

class Triggerbot {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    int delay;
    bool teamCheck;

public:
    Triggerbot(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false), delay(50), teamCheck(true) {
    }

    void Run(int localTeam) {
        if (!enabled || !mem) return;
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!cameraBase) return;

        uintptr_t crosshairEntity = mem->Read<uintptr_t>(cameraBase + 0x380);
        if (!crosshairEntity) return;

        int entityTeam = mem->Read<int>(crosshairEntity + Offsets::PLAYER_TEAM);
        if (teamCheck && entityTeam == localTeam) return;

        float entityHealth = mem->Read<float>(crosshairEntity + Offsets::PLAYER_HEALTH);
        bool isDead = (mem->Read<uint8_t>(crosshairEntity + Offsets::PLAYER_DEAD_1) != 0);
        if (entityHealth <= 0 || isDead) return;

        Sleep(delay);

        INPUT inputs[2] = {};
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, inputs, sizeof(INPUT));
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetDelay(int d) { delay = d; }
};