#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../player.hpp"
#include "../../offsets.hpp"
#include <vector>
#include <imgui.h>

class Radar {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    float radarSize;
    float radarRange;
    Vec2 radarCenter;
    bool rotateWithPlayer;
    bool showEnemies;
    bool showTeammates;
    bool showVehicles;
    bool showLoot;
    bool showAI;

public:
    Radar(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(true), radarSize(200.0f),
        radarRange(300.0f), radarCenter(Vec2(0, 0)), rotateWithPlayer(true),
        showEnemies(true), showTeammates(true), showVehicles(true),
        showLoot(false), showAI(true) {}

    void Render(std::vector<Player>& players, std::vector<Vehicle>& vehicles,
        Vec3 localPos, int localTeam, int screenWidth, int screenHeight) {
        if (!enabled || !mem) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        float localYaw = 0.0f;
        if (gameBase) {
            Vec3 viewAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);
            localYaw = viewAngles.y * (3.14159265f / 180.0f);
        }

        radarCenter = Vec2(screenWidth - radarSize - 20, screenHeight - radarSize - 20);

        draw->AddRectFilled(
            ImVec2(radarCenter.x - radarSize / 2, radarCenter.y - radarSize / 2),
            ImVec2(radarCenter.x + radarSize / 2, radarCenter.y + radarSize / 2),
            ImColor(0, 0, 0, 140), 4.0f
        );

        draw->AddRect(
            ImVec2(radarCenter.x - radarSize / 2, radarCenter.y - radarSize / 2),
            ImVec2(radarCenter.x + radarSize / 2, radarCenter.y + radarSize / 2),
            ImColor(100, 100, 100, 200), 4.0f
        );

        draw->AddLine(
            ImVec2(radarCenter.x, radarCenter.y - radarSize / 2),
            ImVec2(radarCenter.x, radarCenter.y + radarSize / 2),
            ImColor(80, 80, 80, 100)
        );
        draw->AddLine(
            ImVec2(radarCenter.x - radarSize / 2, radarCenter.y),
            ImVec2(radarCenter.x + radarSize / 2, radarCenter.y),
            ImColor(80, 80, 80, 100)
        );

        draw->AddCircleFilled(
            ImVec2(radarCenter.x, radarCenter.y),
            4.0f, ImColor(0, 255, 255, 255)
        );

        if (rotateWithPlayer) {
            float arrowSize = 10.0f;
            Vec2 arrowEnd = Vec2(
                radarCenter.x + sin(localYaw) * arrowSize,
                radarCenter.y - cos(localYaw) * arrowSize
            );
            draw->AddLine(
                ImVec2(radarCenter.x, radarCenter.y),
                ImVec2(arrowEnd.x, arrowEnd.y),
                ImColor(0, 255, 255, 255), 2.0f
            );
        }

        if (showEnemies || showTeammates) {
            for (auto& player : players) {
                if (!player.IsAlive()) continue;
                bool isAI = player.GetTeam() == 0;
                if (isAI && !showAI) continue;
                bool isEnemy = player.GetTeam() != localTeam;

                if (isEnemy && !showEnemies) continue;
                if (!isEnemy && !showTeammates) continue;

                Vec3 pPos = player.GetPosition();
                Vec3 delta = pPos - localPos;

                if (rotateWithPlayer) {
                    float cosY = cos(-localYaw);
                    float sinY = sin(-localYaw);
                    float dx = delta.x * cosY - delta.y * sinY;
                    float dy = delta.x * sinY + delta.y * cosY;
                    delta.x = dx;
                    delta.y = dy;
                }

                float dist = delta.Length();
                if (dist > radarRange || dist < 0.5f) continue;

                float scale = radarSize / 2 / radarRange;
                Vec2 screenPos = Vec2(
                    radarCenter.x + delta.x * scale,
                    radarCenter.y - delta.y * scale
                );

                screenPos.x = std::clamp(screenPos.x, radarCenter.x - radarSize / 2 + 3, radarCenter.x + radarSize / 2 - 3);
                screenPos.y = std::clamp(screenPos.y, radarCenter.y - radarSize / 2 + 3, radarCenter.y + radarSize / 2 - 3);

                ImColor dotColor = isEnemy ? ImColor(255, 0, 0, 255) : ImColor(0, 255, 0, 255);
                draw->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 3.0f, dotColor);

                // Direction arrow from velocity (avoid dead/still players)
                Vec3 vel = player.GetVelocity();
                float velLen = vel.Length();
                if (velLen > 0.5f) {
                    float vx = vel.x, vy = vel.y;
                    if (rotateWithPlayer) {
                        float cosY = cos(-localYaw);
                        float sinY = sin(-localYaw);
                        float dx = vx * cosY - vy * sinY;
                        float dy = vx * sinY + vy * cosY;
                        vx = dx; vy = dy;
                    }
                    float scale = radarSize / 2 / radarRange;
                    float arrowLen = 7.0f;
                    Vec2 dirN = Vec2(vx, vy).Normalize();
                    draw->AddLine(
                        ImVec2(screenPos.x, screenPos.y),
                        ImVec2(screenPos.x + dirN.x * arrowLen, screenPos.y - dirN.y * arrowLen),
                        dotColor, 1.5f
                    );
                }
            }
        }

        if (showVehicles) {
            for (auto& v : vehicles) {
                Vec3 delta = v.position - localPos;

                if (rotateWithPlayer) {
                    float cosY = cos(-localYaw);
                    float sinY = sin(-localYaw);
                    float dx = delta.x * cosY - delta.y * sinY;
                    float dy = delta.x * sinY + delta.y * cosY;
                    delta.x = dx;
                    delta.y = dy;
                }

                float dist = delta.Length();
                if (dist > radarRange) continue;

                float scale = radarSize / 2 / radarRange;
                Vec2 screenPos = Vec2(
                    radarCenter.x + delta.x * scale,
                    radarCenter.y - delta.y * scale
                );

                draw->AddRectFilled(
                    ImVec2(screenPos.x - 2, screenPos.y - 2),
                    ImVec2(screenPos.x + 2, screenPos.y + 2),
                    ImColor(0, 100, 255, 200)
                );
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetRadarSize(float s) { radarSize = s; }
    void SetRadarRange(float r) { radarRange = r; }
    void SetRotateWithPlayer(bool r) { rotateWithPlayer = r; }
    void SetShowEnemies(bool e) { showEnemies = e; }
    void SetShowTeammates(bool t) { showTeammates = t; }
    void SetShowVehicles(bool v) { showVehicles = v; }
    void SetShowAI(bool a) { showAI = a; }
};
