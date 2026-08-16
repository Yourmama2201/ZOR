#pragma once
#include <imgui.h>
#include "../../memory.hpp"
#include "../../offsets.hpp"

class FOVRenderer {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool drawFovCircle;
    bool drawVisibilityIndicator;
    float fov;
    float thickness;
    float color[4];

public:
    FOVRenderer(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), drawFovCircle(false),
        drawVisibilityIndicator(false), fov(30.0f), thickness(1.0f) {
        color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 0.25f;
    }

    void RenderCrosshairIndicator(int screenWidth, int screenHeight, int localTeam) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImVec2 center(screenWidth / 2.0f, screenHeight / 2.0f);

        // Draw FOV circle - scale radius to the game's actual FOV so it tracks zoom
        if (drawFovCircle && fov > 0.0f) {
            RenderFovCircle(fov, thickness, color, screenWidth, screenHeight);
        }

        if (!drawVisibilityIndicator || !mem || !gameBase) return;

        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!cameraBase) return;

        uintptr_t crosshairEntity = mem->Read<uintptr_t>(cameraBase + 0x380);
        bool onEnemy = false;

        if (crosshairEntity) {
            int entityTeam = mem->Read<int>(crosshairEntity + Offsets::PLAYER_TEAM);
            float entityHealth = mem->Read<float>(crosshairEntity + Offsets::PLAYER_HEALTH);
            bool isDead = (mem->Read<uint8_t>(crosshairEntity + Offsets::PLAYER_DEAD_1) != 0);

            onEnemy = (entityTeam != localTeam && entityHealth > 0 && !isDead);
        }

        if (onEnemy) {
            draw->AddCircleFilled(center, 6, ImColor(1.0f, 0.0f, 0.0f, 0.80f));
            draw->AddCircle(center, 10, ImColor(1.0f, 0.0f, 0.0f, 0.40f), 0, 2.0f);
        }
    }

    void RenderFovCircle(float fovDegrees, float circleThickness, const float c[4], int screenWidth, int screenHeight) {
        if (fovDegrees <= 0.0f) return;
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImVec2 center(screenWidth / 2.0f, screenHeight / 2.0f);

        float gameFov = 90.0f;
        if (mem && gameBase) {
            uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
            if (cameraBase) {
                float camFov = mem->Read<float>(cameraBase + 0x120);
                if (camFov > 20.0f && camFov < 170.0f) gameFov = camFov;
            }
        }
        float radius = tanf(fovDegrees * 0.5f * 0.01745329252f) * (screenHeight / 2.0f) / tanf(gameFov * 0.5f * 0.01745329252f);
        draw->AddCircle(center, radius, ImColor(c[0], c[1], c[2], c[3]), 48, circleThickness);
    }

    void SetDrawFovCircle(bool e) { drawFovCircle = e; }
    void SetFov(float f) { fov = f; }
    void SetThickness(float t) { thickness = t; }
    void SetColor(const float c[4]) { for (int i = 0; i < 4; i++) color[i] = c[i]; }
    void SetVisibilityIndicator(bool e) { drawVisibilityIndicator = e; }
};
