#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../offsets.hpp"
#include "../../player.hpp"
#include <vector>
#include <string>
#include <imgui.h>

enum GameMode {
    MODE_UNKNOWN = 0,
    MODE_DMZ = 1,
    MODE_WARZONE = 2,
    MODE_MULTIPLAYER = 3,
    MODE_LOADING = 4,
    MODE_MENU = 5,
};

class WorldVisuals {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool drawGameMode;
    bool drawPlayerCount;
    bool drawLocalInfo;
    bool drawCompass;
    bool drawWatermark;
    bool drawTimer;
    bool drawAmmoCounter;
    bool drawWeaponInfo;

public:
    WorldVisuals(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(true),
        drawGameMode(true), drawPlayerCount(true), drawLocalInfo(true),
        drawCompass(false), drawWatermark(true), drawTimer(true),
        drawAmmoCounter(true), drawWeaponInfo(true) {}

    GameMode GetGameMode() {
        if (!mem || !gameBase) return MODE_UNKNOWN;
        int mode = mem->Read<int>(gameBase + Offsets::GAME_MODE);
        switch (mode) {
        case 0: return MODE_MENU;
        case 1: return MODE_LOADING;
        case 2: return MODE_DMZ;
        case 3: return MODE_WARZONE;
        case 4: return MODE_MULTIPLAYER;
        default: return MODE_UNKNOWN;
        }
    }

    int GetActivePlayerCount() {
        if (!mem || !gameBase) return 0;
        uintptr_t entityList = mem->Read<uintptr_t>(gameBase + Offsets::DISTRIBUTE);
        if (!entityList) return 0;

        int count = 0;
        for (int i = 0; i < 150; i++) {
            uintptr_t entity = mem->Read<uintptr_t>(entityList + (i * 0x8));
            if (!entity) continue;
            if (mem->Read<uint8_t>(entity + Offsets::PLAYER_VALID)) count++;
        }
        return count;
    }

    uint32_t GetGameTimestamp() {
        if (!mem || !gameBase) return 0;
        return mem->Read<uint32_t>(gameBase + Offsets::TIMESTAMP);
    }

    std::string GetGameModeString() {
        switch (GetGameMode()) {
        case MODE_DMZ: return "DMZ";
        case MODE_WARZONE: return "WARZONE";
        case MODE_MULTIPLAYER: return "MULTIPLAYER";
        case MODE_LOADING: return "LOADING...";
        case MODE_MENU: return "MENU";
        default: return "UNKNOWN";
        }
    }

    void RenderHUD(int screenWidth, int screenHeight, Vec3 localPos, int health, int armor) {
        if (!enabled) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        int yOffset = 10;

        if (drawWatermark) {
            char watermark[96];
            sprintf_s(watermark, "ZORMenu v4.0 | Build 2026 | Players: %d | FPS: %.0f",
                GetActivePlayerCount(), ImGui::GetIO().Framerate);
            draw->AddText(
                ImVec2(screenWidth - 320, yOffset),
                ImColor(0, 255, 255, 180), watermark
            );
            yOffset += 18;
        }

        if (drawGameMode) {
            std::string modeStr = "Mode: " + GetGameModeString();
            draw->AddText(
                ImVec2(screenWidth - 200, yOffset),
                ImColor(0, 200, 255, 200), modeStr.c_str()
            );
            yOffset += 18;
        }

        if (drawPlayerCount) {
            char buf[64];
            sprintf_s(buf, "Players: %d", GetActivePlayerCount());
            draw->AddText(
                ImVec2(screenWidth - 200, yOffset),
                ImColor(200, 200, 200, 200), buf
            );
            yOffset += 18;
        }

        if (drawLocalInfo) {
            char buf[128];
            sprintf_s(buf, "Pos: %.0f, %.0f, %.0f", localPos.x, localPos.y, localPos.z);
            draw->AddText(
                ImVec2(screenWidth - 200, yOffset),
                ImColor(150, 150, 150, 150), buf
            );
            yOffset += 18;

            sprintf_s(buf, "HP: %d | Armor: %d", health, armor);
            draw->AddText(
                ImVec2(screenWidth - 200, yOffset),
                ImColor(0, 255, 0, 200), buf
            );
            yOffset += 18;
        }

        if (drawTimer) {
            uint32_t timestamp = GetGameTimestamp();
            if (timestamp > 0) {
                int minutes = timestamp / 60000;
                int seconds = (timestamp % 60000) / 1000;
                char buf[32];
                sprintf_s(buf, "%02d:%02d", minutes, seconds);
                draw->AddText(
                    ImVec2(screenWidth - 200, yOffset),
                    ImColor(255, 255, 255, 200), buf
                );
            }
        }
    }

    void RenderCompass(int screenWidth, int screenHeight, float yaw) {
        if (!drawCompass || !enabled) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        float compassWidth = 300.0f;
        float compassHeight = 40.0f;
        Vec2 compassCenter(screenWidth / 2, screenHeight - 60);

        draw->AddRectFilled(
            ImVec2(compassCenter.x - compassWidth / 2, compassCenter.y - compassHeight / 2),
            ImVec2(compassCenter.x + compassWidth / 2, compassCenter.y + compassHeight / 2),
            ImColor(0, 0, 0, 120), 4.0f
        );

        const char* directions[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
        for (int i = -6; i <= 6; i++) {
            float angle = yaw + i * 15.0f;
            float x = compassCenter.x + (angle * compassWidth / 360.0f);
            int dirIndex = ((int)(angle + 180 + 22.5f) / 45) % 8;
            bool isMain = (i % 3 == 0);

            draw->AddLine(
                ImVec2(x, compassCenter.y - (isMain ? 12 : 6)),
                ImVec2(x, compassCenter.y + 2),
                ImColor(200, 200, 200, isMain ? 200 : 80)
            );

            if (isMain) {
                draw->AddText(
                    ImVec2(x - 6, compassCenter.y - compassHeight / 2 + 2),
                    ImColor(255, 255, 255, 200), directions[dirIndex]
                );
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetDrawGameMode(bool d) { drawGameMode = d; }
    void SetDrawWatermark(bool d) { drawWatermark = d; }
    void SetDrawCompass(bool d) { drawCompass = d; }
    void SetDrawTimer(bool d) { drawTimer = d; }
    void SetDrawLocalInfo(bool d) { drawLocalInfo = d; }
};
