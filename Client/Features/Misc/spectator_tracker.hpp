#pragma once
#include <vector>
#include <string>
#include <imgui.h>
#include "../../memory.hpp"
#include "../../offsets.hpp"

class SpectatorTracker {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool autoCloak;
    bool cloakActive;
    std::vector<std::string> spectators;
    int lastSpectatorCount;
    float cloakTimer;
    float cloakCooldown;

public:
    SpectatorTracker(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false),
        autoCloak(false), cloakActive(false),
        lastSpectatorCount(-1), cloakTimer(0), cloakCooldown(0) {}

    std::vector<std::string> GetSpectators(int localTeam) {
        std::vector<std::string> list;
        if (!mem || !gameBase) return list;

        uintptr_t camBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!camBase) return list;

        // Read spectator list from camera system
        uintptr_t specList = mem->Read<uintptr_t>(camBase + 0x4A0);
        if (!specList) return list;

        int specCount = mem->Read<int>(specList + 0x8);
        if (specCount > 12) specCount = 12;
        if (specCount < 0) return list;

        for (int i = 0; i < specCount; i++) {
            uintptr_t specEntry = mem->Read<uintptr_t>(specList + 0x10 + (i * 0x8));
            if (!specEntry) continue;

            std::string name = mem->ReadString(specEntry + 0x800, 32);
            int specTeam = mem->Read<int>(specEntry + Offsets::PLAYER_TEAM);
            float health = mem->Read<float>(specEntry + Offsets::PLAYER_HEALTH);
            bool dead = mem->Read<uint8_t>(specEntry + Offsets::PLAYER_DEAD_1) != 0;

            // They're spectating if they're dead or on spectator cam
            if (dead || health <= 0) {
                if (!name.empty() && specTeam != localTeam) {
                    list.push_back(name);
                }
            }
        }

        return list;
    }

    void Update(int localTeam, bool& aimbotToggle, bool& espToggle, bool& triggerToggle) {
        if (!enabled || !mem || !gameBase) return;

        spectators = GetSpectators(localTeam);
        int currentCount = (int)spectators.size();

        float now = ImGui::GetTime();

        // Cloak cooldown: prevent rapid toggling
        if (cloakCooldown > now) return;

        if (autoCloak && currentCount > 0) {
            if (!cloakActive) {
                // Save states and disable cheats
                cloakActive = true;
                aimbotToggle = false;
                espToggle = false;
                triggerToggle = false;
                cloakCooldown = now + 30.0f; // stay cloaked for 30s
                cloakTimer = now;
            }
        }
        else if (autoCloak && currentCount == 0 && cloakActive) {
            // Only re-enable after 30s cooldown
            if ((now - cloakTimer) > 30.0f) {
                cloakActive = false;
            }
        }

        lastSpectatorCount = currentCount;
    }

    void RenderHUD(int screenWidth, int screenHeight) {
        if (!enabled || spectators.empty()) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        // Red warning banner at top
        char buf[128];
        sprintf_s(buf, "! SPECTATORS: %d watching - %s",
            (int)spectators.size(),
            autoCloak ? (cloakActive ? "CLOAKED" : "AUTO-CLOAK ARMED") : "VISIBLE");

        ImColor bg = cloakActive ? ImColor(1.0f, 0.0f, 0.0f, 0.35f)
            : ImColor(1.0f, 0.5f, 0.0f, 0.25f);

        draw->AddRectFilled(
            ImVec2(screenWidth / 2 - 200, 5),
            ImVec2(screenWidth / 2 + 200, 32),
            bg, 4.0f);
        draw->AddText(
            ImVec2(screenWidth / 2 - 180, 9),
            ImColor(1.0f, 1.0f, 1.0f, 0.90f), buf);

        // List spectators
        if (!cloakActive) {
            int yOff = 38;
            for (auto& name : spectators) {
                draw->AddText(
                    ImVec2(screenWidth / 2 - 180, yOff),
                    ImColor(1.0f, 0.6f, 0.2f, 0.80f),
                    name.c_str());
                yOff += 16;
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetAutoCloak(bool a) { autoCloak = a; }
    bool IsCloaked() const { return cloakActive; }
    bool HasSpectators() const { return !spectators.empty(); }
    int SpectatorCount() const { return (int)spectators.size(); }
    bool IsEnabled() const { return enabled; }
};
