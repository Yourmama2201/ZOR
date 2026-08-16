#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../offsets.hpp"
#include <vector>
#include <imgui.h>

class GrenadePrediction {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;

    struct NadeInfo {
        Vec3 origin;
        Vec3 velocity;
        bool isCooking;
        float cookTime;
        int type;
        bool isActive;
    };

public:
    GrenadePrediction(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false) {}

    void Render(int screenWidth, int screenHeight) {
        if (!enabled || !mem || !gameBase) return;

        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!cameraBase) return;

        Matrix4x4 viewMatrix = mem->Read<Matrix4x4>(cameraBase + 0x100);

        NadeInfo nade = GetNadeInfo();
        if (!nade.isActive) return;

        std::vector<Vec3> path = PredictPath(nade);
        if (path.empty()) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        Vec2 lastScreen = Math::WorldToScreen(path[0], viewMatrix, screenWidth, screenHeight);

        for (size_t i = 1; i < path.size(); i++) {
            Vec2 screen = Math::WorldToScreen(path[i], viewMatrix, screenWidth, screenHeight);
            if (screen.x < -100 || lastScreen.x < -100) {
                lastScreen = screen;
                continue;
            }

            float alpha = 1.0f - ((float)i / path.size()) * 0.7f;
            draw->AddLine(
                ImVec2(lastScreen.x, lastScreen.y),
                ImVec2(screen.x, screen.y),
                ImColor(1.0f, 0.80f, 0.0f, alpha), 2.0f
            );

            if (i % 5 == 0) {
                draw->AddCircleFilled(
                    ImVec2(screen.x, screen.y), 2.0f,
                    ImColor(1.0f, 0.80f, 0.0f, alpha * 0.5f)
                );
            }

            lastScreen = screen;
        }

        if (!path.empty()) {
            Vec2 end = Math::WorldToScreen(path.back(), viewMatrix, screenWidth, screenHeight);
            if (end.x > -100) {
                draw->AddCircleFilled(
                    ImVec2(end.x, end.y), 5.0f,
                    ImColor(1.0f, 0.0f, 0.0f, 0.80f)
                );
                draw->AddCircle(
                    ImVec2(end.x, end.y), 10.0f,
                    ImColor(1.0f, 0.0f, 0.0f, 0.40f), 0, 2.0f
                );
            }
        }
    }

private:
    NadeInfo GetNadeInfo() {
        NadeInfo info = {};
        uintptr_t nadePtr = mem->Read<uintptr_t>(gameBase + 0x12A8);
        if (!nadePtr) return info;

        info.origin = mem->Read<Vec3>(nadePtr + 0x20);
        info.velocity = mem->Read<Vec3>(nadePtr + 0x40);
        info.isCooking = mem->Read<uint8_t>(nadePtr + 0x60) != 0;
        info.cookTime = mem->Read<float>(nadePtr + 0x64);
        info.type = mem->Read<int>(nadePtr + 0x68);
        info.isActive = (info.velocity.Length() > 0.1f || info.isCooking);

        return info;
    }

    std::vector<Vec3> PredictPath(const NadeInfo& nade) {
        std::vector<Vec3> path;
        Vec3 pos = nade.origin;
        Vec3 vel = nade.velocity;

        float timeStep = 0.03f;
        float gravity = 9.81f * 40.0f; // scaled for game units

        // Add throwing velocity from view angles if cooking
        if (nade.isCooking) {
            Vec3 viewAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);
            float pitch = viewAngles.x * (3.14159265f / 180.0f);
            float yaw = viewAngles.y * (3.14159265f / 180.0f);

            float throwPower = 25.0f;
            vel.x = -sin(yaw) * cos(pitch) * throwPower;
            vel.y = cos(yaw) * cos(pitch) * throwPower;
            vel.z = -sin(pitch) * throwPower;
        }

        for (int i = 0; i < 200; i++) {
            path.push_back(pos);
            vel.z -= gravity * timeStep;
            pos.x += vel.x * timeStep;
            pos.y += vel.y * timeStep;
            pos.z += vel.z * timeStep;

            // Simple ground collision
            if (pos.z < 0) break;
        }

        return path;
    }

public:
    void SetEnabled(bool e) { enabled = e; }
};
