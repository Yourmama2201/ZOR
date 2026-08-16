#pragma once
#include <vector>
#include <string>
#include <imgui.h>
#include "player.hpp"
#include "vehicle.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "offsets.hpp"

class ESP {
private:
    MemoryManager* mem;
    HWND gameWindow;
    bool enabled;
    bool wallhack;
    bool vehicleESP;
    bool lootESP;
    bool drawSnaplines;
    bool drawHealth;
    bool drawArmor;
    bool drawNames;
    bool drawDistance;
    bool drawBoxes;
    bool drawBoxFill;
    bool drawHeadCircle;
    bool drawWeaponNames;
    bool drawSquadCount;
    bool drawItemRarity;
    bool drawTextBackground;
    bool distanceFade;
    int boxStyle;
    float boxFillAlpha;

    ImColor Alpha(ImColor c, float a) {
        c.Value.w = a;
        return c;
    }

    // Sort so far players draw first (they end up underneath close ones).
    struct SortEntry {
        float dist;
        int idx;
    };

    void DrawBoxOutlined(ImDrawList* draw, Vec2 a, Vec2 b, ImColor col, float thickness, float rounding) {
        // black outline pass (thicker) then color pass for readability on any bg
        draw->AddRect(ImVec2(a.x, a.y), ImVec2(b.x, b.y),
            ImColor(0, 0, 0, 200), rounding, 0, thickness + 2.0f);
        draw->AddRect(ImVec2(a.x, a.y), ImVec2(b.x, b.y),
            col, rounding, 0, thickness);
    }

    void DrawCornerBox(ImDrawList* draw, Vec2 head, Vec2 foot, float width, ImColor col, float thickness) {
        float x = head.x, y = head.y;
        float w = width, h = foot.y - head.y;
        float len = (w < 30.0f) ? 6.0f : 10.0f;
        ImU32 outline = ImColor(0, 0, 0, 200);

        // black outline pass
        const float o = thickness + 2.0f;
        draw->AddLine(ImVec2(x, y), ImVec2(x + len, y), outline, o);
        draw->AddLine(ImVec2(x, y), ImVec2(x, y + len), outline, o);
        draw->AddLine(ImVec2(x + w, y), ImVec2(x + w - len, y), outline, o);
        draw->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + len), outline, o);
        draw->AddLine(ImVec2(x, y + h), ImVec2(x + len, y + h), outline, o);
        draw->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - len), outline, o);
        draw->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - len, y + h), outline, o);
        draw->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - len), outline, o);

        // color pass
        draw->AddLine(ImVec2(x, y), ImVec2(x + len, y), col, thickness);
        draw->AddLine(ImVec2(x, y), ImVec2(x, y + len), col, thickness);
        draw->AddLine(ImVec2(x + w, y), ImVec2(x + w - len, y), col, thickness);
        draw->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + len), col, thickness);
        draw->AddLine(ImVec2(x, y + h), ImVec2(x + len, y + h), col, thickness);
        draw->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - len), col, thickness);
        draw->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - len, y + h), col, thickness);
        draw->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - len), col, thickness);
    }

    void TextWithBackground(ImDrawList* draw, ImVec2 pos, const std::string& text, ImColor col) {
        if (drawTextBackground) {
            ImVec2 sz = ImGui::CalcTextSize(text.c_str());
            draw->AddRectFilled(ImVec2(pos.x - 3, pos.y - 2),
                ImVec2(pos.x + sz.x + 3, pos.y + sz.y + 2),
                ImColor(0, 0, 0, 110), 2.0f);
        }
        draw->AddText(pos, col, text.c_str());
    }

    void RenderPlayers(std::vector<Player>& players, Matrix4x4& viewMatrix,
        int width, int height, Vec3 localPos, int localTeam,
        ImDrawList* draw, uintptr_t gameBase) {

        // Collect on-screen entries with distance, then sort far-to-near so close
        // targets render on top.
        std::vector<SortEntry> order;
        for (size_t i = 0; i < players.size(); i++) {
            if (players[i].GetTeam() == localTeam) continue;
            if (!players[i].IsAlive()) continue;
            if (!wallhack && !players[i].IsVisible()) continue;
            players[i].SetDistance(localPos);
            order.push_back({ players[i].GetDistance(), (int)i });
        }
        std::sort(order.begin(), order.end(),
            [](const SortEntry& a, const SortEntry& b) { return a.dist > b.dist; });

        int teammateCount = 0;
        if (drawSquadCount) {
            for (auto& p : players) if (p.GetTeam() == localTeam && p.IsAlive()) teammateCount++;
        }

        for (auto& oe : order) {
            auto& player = players[oe.idx];
            if (player.GetTeam() == localTeam) continue;
            if (!player.IsAlive()) continue;

            // AI detection
            bool isAI = (player.GetTeam() == 0);
            bool isBoss = false;
            std::string pname = player.GetName();
            if (isAI) {
                isBoss = (pname.find("Juggernaut") != std::string::npos ||
                    pname.find("Commander") != std::string::npos ||
                    pname.find("Chemist") != std::string::npos ||
                    pname.find("Boss") != std::string::npos ||
                    pname.find("Scavenger") != std::string::npos);
            }

            Vec2 headScreen = Math::WorldToScreen(player.GetHeadPos(), viewMatrix, width, height);
            Vec2 footScreen = Math::WorldToScreen(player.GetFootPos(), viewMatrix, width, height);

            if (headScreen.x < -100 || footScreen.x < -100) continue;

            float heightBox = footScreen.y - headScreen.y;
            float widthBox = heightBox * 0.35f;
            if (widthBox < 8.0f) widthBox = 8.0f;

            // Distance fade: alpha drops off past 120m
            float alpha = 1.0f;
            if (distanceFade) {
                float d = player.GetDistance();
                if (d > 120.0f) alpha = (d > 400.0f) ? 0.35f : 1.0f - (d - 120.0f) / 400.0f * 0.65f;
            }

            // Color: green=visible, yellow=hidden, red=boss, blue=teammate
            ImColor boxColor;
            if (isBoss) boxColor = ImColor(255, 0, 0, 255);
            else if (player.IsVisible()) boxColor = ImColor(0, 255, 0, 255);
            else boxColor = ImColor(255, 200, 0, 200);
            if (alpha < 1.0f) boxColor = Alpha(boxColor, alpha * (boxColor.Value.w));

            float thickness = isBoss ? 2.0f : 1.5f;

            if (drawBoxes && boxStyle >= 0) {
                if (drawBoxFill) {
                    draw->AddRectFilled(ImVec2(headScreen.x - widthBox / 2, headScreen.y),
                        ImVec2(headScreen.x + widthBox / 2, footScreen.y),
                        ImColor(0, 0, 0, (int)(boxFillAlpha * 255 * alpha)));
                }
                if (boxStyle == 0) {
                    DrawBoxOutlined(draw, Vec2(headScreen.x - widthBox / 2, headScreen.y),
                        Vec2(headScreen.x + widthBox / 2, footScreen.y), boxColor, thickness, 1.0f);
                }
                else if (boxStyle == 1) {
                    DrawCornerBox(draw, Vec2(headScreen.x - widthBox / 2, headScreen.y),
                        footScreen, widthBox, boxColor, thickness);
                }
                else {
                    DrawBoxOutlined(draw, Vec2(headScreen.x - widthBox / 2, headScreen.y),
                        Vec2(headScreen.x + widthBox / 2, footScreen.y), boxColor, thickness, 1.0f);
                    DrawCornerBox(draw, Vec2(headScreen.x - widthBox / 2, headScreen.y),
                        footScreen, widthBox, boxColor, thickness);
                }
            }

            // Health + armor bars with track + border
            if (drawHealth || drawArmor) {
                float bx = headScreen.x - widthBox / 2;
                float barW = (widthBox < 20.0f) ? 4.0f : 3.0f;
                float healthPercent = min(player.GetHealth() / 100.0f, 1.0f);
                if (healthPercent < 0) healthPercent = 0;
                int hbarH = (int)(heightBox * healthPercent);
                if (hbarH < 1) hbarH = 1;

                // track
                draw->AddRectFilled(ImVec2(bx - barW - 3, footScreen.y),
                    ImVec2(bx - barW - 1, footScreen.y - heightBox), ImColor(0, 0, 0, (int)(140 * alpha)));
                // fill
                draw->AddRectFilled(ImVec2(bx - barW - 3, footScreen.y - hbarH),
                    ImVec2(bx - barW - 1, footScreen.y),
                    ImColor(1.0f - healthPercent, healthPercent, 0.0f, 1.0f * alpha));
                // border
                draw->AddRect(ImVec2(bx - barW - 3, footScreen.y - heightBox),
                    ImVec2(bx - barW - 1, footScreen.y), ImColor(0, 0, 0, 220), 0, 0, 1.0f);

                if (drawHealth) {
                    char hpText[16];
                    sprintf_s(hpText, "%d", (int)player.GetHealth());
                    draw->AddText(
                        ImVec2(bx - barW - 40, footScreen.y - hbarH - 8),
                        ImColor(255, 255, 255, 220), hpText);
                }

                if (drawArmor) {
                    float armorPercent = min(player.GetArmor() / 100.0f, 1.0f);
                    if (armorPercent < 0) armorPercent = 0;
                    int abarH = (int)(heightBox * armorPercent);
                    if (abarH < 1) abarH = 1;
                    float ax = bx + barW + 1;
                    draw->AddRectFilled(ImVec2(ax, footScreen.y),
                        ImVec2(ax + barW, footScreen.y - heightBox), ImColor(0, 0, 0, (int)(140 * alpha)));
                    draw->AddRectFilled(ImVec2(ax, footScreen.y - abarH),
                        ImVec2(ax + barW, footScreen.y), ImColor(0, 120, 255, (int)(255 * alpha)));
                    draw->AddRect(ImVec2(ax, footScreen.y - heightBox),
                        ImVec2(ax + barW, footScreen.y), ImColor(0, 0, 0, 220), 0, 0, 1.0f);
                }
            }

            if (drawNames) {
                std::string displayName = pname;
                if (isAI && !isBoss) displayName = "AI [" + std::to_string((int)player.GetHealth()) + "]";
                else if (isBoss) displayName = "BOSS: " + pname;
                if (!displayName.empty()) {
                    ImVec2 sz = ImGui::CalcTextSize(displayName.c_str());
                    ImVec2 pos = ImVec2(headScreen.x - sz.x / 2, footScreen.y + 2);
                    TextWithBackground(draw, pos, displayName, boxColor);
                }
            }

            if (drawWeaponNames) {
                std::string wName = "W:" + std::to_string(player.GetWeapon());
                std::string resolved = player.GetWeaponName(gameBase);
                if (!resolved.empty()) wName = resolved;
                ImVec2 sz = ImGui::CalcTextSize(wName.c_str());
                ImVec2 pos = ImVec2(headScreen.x - sz.x / 2, headScreen.y - 30);
                TextWithBackground(draw, pos, wName, ImColor(200, 200, 255, 200));
            }

            if (drawDistance) {
                char dist[32];
                sprintf_s(dist, "%.0fm", player.GetDistance());
                ImVec2 sz = ImGui::CalcTextSize(dist);
                ImVec2 pos = ImVec2(headScreen.x - sz.x / 2, headScreen.y - 16);
                TextWithBackground(draw, pos, dist, ImColor(255, 255, 255, 200));
            }

            if (drawHeadCircle) {
                draw->AddCircle(ImVec2(headScreen.x, headScreen.y), 4.0f, boxColor, 12, isBoss ? 2.0f : 1.0f);
            }

            if (drawSnaplines) {
                draw->AddLine(ImVec2(width / 2, height), ImVec2(headScreen.x, footScreen.y),
                    ImColor(255, 255, 255, (int)(60 * alpha)), 1.0f);
            }
        }

        // Squad count display
        if (drawSquadCount && teammateCount > 0) {
            char squad[16];
            sprintf_s(squad, "Sq: %d", teammateCount);
            draw->AddText(ImVec2(width - 80, 30), ImColor(0, 200, 255, 200), squad);
        }
    }

    void RenderVehicles(std::vector<Vehicle>& vehicles, Matrix4x4& viewMatrix,
        int width, int height, ImDrawList* draw) {
        for (auto& vehicle : vehicles) {
            Vec2 screenPos = Math::WorldToScreen(vehicle.position, viewMatrix, width, height);
            if (screenPos.x < -100) continue;

            draw->AddRect(
                ImVec2(screenPos.x - 20, screenPos.y - 15),
                ImVec2(screenPos.x + 20, screenPos.y + 15),
                ImColor(0, 100, 255, 255), 1.0f, 0, 1.0f
            );

            if (!vehicle.name.empty()) {
                draw->AddText(ImVec2(screenPos.x - (vehicle.name.length() * 3), screenPos.y - 25),
                    ImColor(255, 255, 255, 255), vehicle.name.c_str());
            }

            // Health bar
            float healthPct = vehicle.health / max(vehicle.maxHealth, 1.0f);
            draw->AddRectFilled(
                ImVec2(screenPos.x - 20, screenPos.y + 15),
                ImVec2(screenPos.x - 20 + (40 * healthPct), screenPos.y + 20),
                ImColor(1.0f - healthPct, healthPct, 0.0f, 1.0f)
            );

            // Fuel bar
            float fuelPct = vehicle.fuel / max(vehicle.maxFuel, 1.0f);
            draw->AddRectFilled(
                ImVec2(screenPos.x - 20, screenPos.y + 21),
                ImVec2(screenPos.x - 20 + (40 * fuelPct), screenPos.y + 25),
                ImColor(0, 100, 255, 200)
            );
        }
    }

    void RenderLoot(uintptr_t gameBase, Matrix4x4& viewMatrix,
        int width, int height, ImDrawList* draw) {
        uintptr_t lootPtr = mem->Read<uintptr_t>(gameBase + Offsets::LOOT_PTR);
        if (!lootPtr) return;

        for (int i = 0; i < 50; i++) {
            uintptr_t item = mem->Read<uintptr_t>(lootPtr + (i * 0x8));
            if (!item) continue;

            Vec3 pos = mem->Read<Vec3>(item + 0x50);
            Vec2 screenPos = Math::WorldToScreen(pos, viewMatrix, width, height);
            if (screenPos.x < -100) continue;

            std::string name = mem->ReadString(item + 0x200, 32);
            int rarity = mem->Read<int>(item + 0x210);

            // Color by rarity
            ImColor lootColor;
            if (drawItemRarity) {
                switch (rarity) {
                case 0: lootColor = ImColor(255, 255, 255, 200); break; // common
                case 1: lootColor = ImColor(0, 255, 0, 200); break;     // uncommon
                case 2: lootColor = ImColor(0, 150, 255, 200); break;   // rare
                case 3: lootColor = ImColor(128, 0, 128, 200); break;   // epic
                case 4: lootColor = ImColor(255, 165, 0, 200); break;   // legendary
                default: lootColor = ImColor(255, 215, 0, 200);
                }
            }
            else {
                lootColor = ImColor(255, 215, 0, 200);
            }

            draw->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 5.0f, lootColor);
            if (!name.empty()) {
                draw->AddText(ImVec2(screenPos.x - 15, screenPos.y - 20), lootColor, name.c_str());
            }
        }
    }

public:
    ESP(MemoryManager* memory, HWND window)
        : mem(memory), gameWindow(window), enabled(true), wallhack(true),
        vehicleESP(true), lootESP(true), drawSnaplines(true),
        drawHealth(true), drawArmor(true), drawNames(true), drawDistance(true),
        drawBoxes(true), drawBoxFill(false), drawHeadCircle(true), drawWeaponNames(true),
        drawSquadCount(true), drawItemRarity(true), drawTextBackground(true),
        distanceFade(true), boxStyle(0), boxFillAlpha(0.12f) {}

    void Render(std::vector<Player>& players, std::vector<Vehicle>& vehicles,
        uintptr_t gameBase, Vec3 localPos, int localTeam) {
        if (!enabled || !mem || !gameWindow) return;

        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!cameraBase) return;

        Matrix4x4 viewMatrix = mem->Read<Matrix4x4>(cameraBase + 0x100);

        RECT rect;
        GetWindowRect(gameWindow, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        RenderPlayers(players, viewMatrix, width, height, localPos, localTeam, draw, gameBase);
        if (vehicleESP) RenderVehicles(vehicles, viewMatrix, width, height, draw);
        if (lootESP) RenderLoot(gameBase, viewMatrix, width, height, draw);
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetWallhack(bool w) { wallhack = w; }
    void SetVehicleESP(bool v) { vehicleESP = v; }
    void SetLootESP(bool l) { lootESP = l; }
    void SetSnaplines(bool s) { drawSnaplines = s; }
    void SetHealth(bool h) { drawHealth = h; }
    void SetArmor(bool a) { drawArmor = a; }
    void SetNames(bool n) { drawNames = n; }
    void SetDistance(bool d) { drawDistance = d; }
    void SetBoxes(bool b) { drawBoxes = b; }
    void SetBoxFill(bool f) { drawBoxFill = f; }
    void SetBoxStyle(int s) { boxStyle = s; }
    void SetTextBackground(bool t) { drawTextBackground = t; }
    void SetDistanceFade(bool f) { distanceFade = f; }
    void SetHeadCircle(bool h) { drawHeadCircle = h; }
    void SetWeaponNames(bool w) { drawWeaponNames = w; }
    void SetSquadCount(bool s) { drawSquadCount = s; }
    void SetItemRarity(bool r) { drawItemRarity = r; }
};
