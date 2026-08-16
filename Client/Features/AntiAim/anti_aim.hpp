#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../offsets.hpp"
#include <random>

class AntiAim {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool on;
    bool lastRight;
    float spinSpeed;
    int jitterRange;
    int mode;
    float noisePhase;
    std::mt19937 rng;

public:
    AntiAim(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false), on(false),
        lastRight(false), spinSpeed(20.0f), jitterRange(30), mode(0),
        noisePhase(0.0f), rng(std::random_device{}()) {}

    void Run() {
        if (!enabled || !mem) return;

        // Right-click toggles anti-aim on/off (edge-detected)
        bool right = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        if (right && !lastRight) on = !on;
        lastRight = right;

        if (!on) return;

        Vec3 currentAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);
        Vec3 newAngles = currentAngles;

        // Keeps running even while ADS or firing
        switch (mode) {
        case 0: // Spin - speed varies smoothly so it doesn't look like a rigid constant rotation
        {
            noisePhase += 0.35f;
            float wobble = 0.15f * sinf(noisePhase) * spinSpeed;
            float speed = spinSpeed + wobble;
            newAngles.y += speed;
            if (newAngles.y > 180.0f) newAngles.y -= 360.0f;
            break;
        }

        case 1: // Jitter
            newAngles.y += (rand() % jitterRange * 2) - jitterRange;
            newAngles.x += (rand() % 10) - 5;
            break;

        case 2: // 180
            newAngles.y += 180.0f;
            if (newAngles.y > 180.0f) newAngles.y -= 360.0f;
            break;
        }

        newAngles = Math::NormalizeAngles(newAngles);
        mem->Write<Vec3>(gameBase + Offsets::VIEW_ANGLES, newAngles);
    }

    void SetEnabled(bool e) { enabled = e; if (!e) on = false; }
    void SetMode(int m) { mode = m; }
    void SetSpinSpeed(float s) { spinSpeed = s; }
    void SetJitterRange(int r) { jitterRange = r; }
};
