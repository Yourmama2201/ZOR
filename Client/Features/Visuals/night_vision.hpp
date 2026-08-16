#pragma once
#include "../../memory.hpp"
#include "../../offsets.hpp"

class NightVision {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool thermalVision;
    bool nightVision;
    float brightness;
    float contrast;
    uintptr_t lastPostFX;

public:
    NightVision(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false), thermalVision(false),
        nightVision(false), brightness(2.0f), contrast(0.5f), lastPostFX(0) {}

    void Run() {
        if (!enabled || !mem) return;

        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!cameraBase) return;

        uintptr_t postFX = mem->Read<uintptr_t>(cameraBase + 0x200);
        if (!postFX) return;
        lastPostFX = postFX;

        if (nightVision) {
            mem->Write<float>(postFX + 0x10, brightness);
            mem->Write<float>(postFX + 0x14, contrast);
            mem->Write<int>(postFX + 0x20, 1);
            mem->Write<float>(postFX + 0x24, 0.2f);
            mem->Write<float>(postFX + 0x28, 0.8f);
            mem->Write<float>(postFX + 0x2C, 0.1f);
        }

        if (thermalVision) {
            mem->Write<int>(postFX + 0x20, 2);
            mem->Write<float>(postFX + 0x24, 1.0f);
            mem->Write<float>(postFX + 0x28, 0.0f);
            mem->Write<float>(postFX + 0x2C, 0.0f);
            mem->Write<float>(postFX + 0x30, 0.5f);
            mem->Write<float>(postFX + 0x34, 0.7f);
        }
    }

    void Reset() {
        if (!lastPostFX) return;
        mem->Write<int>(lastPostFX + 0x20, 0);
        mem->Write<float>(lastPostFX + 0x10, 1.0f);
        mem->Write<float>(lastPostFX + 0x14, 1.0f);
        mem->Write<float>(lastPostFX + 0x24, 0.0f);
        mem->Write<float>(lastPostFX + 0x28, 0.0f);
        mem->Write<float>(lastPostFX + 0x2C, 0.0f);
        mem->Write<float>(lastPostFX + 0x30, 0.0f);
        mem->Write<float>(lastPostFX + 0x34, 0.0f);
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetThermal(bool t) { thermalVision = t; }
    void SetNightVision(bool n) { nightVision = n; if (n) thermalVision = false; }
    void SetBrightness(float b) { brightness = b; }
    void SetContrast(float c) { contrast = c; }
};
