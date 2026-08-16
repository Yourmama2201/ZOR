#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include <random>
#include "../../memory.hpp"
#include "../../offsets.hpp"

enum class AccountStatus {
    HEALTHY,
    SHADOWBAN,
    BANNED,
    LIMITED,
    SUSPENDED,
    UNKNOWN
};

class AccountHealth {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    AccountStatus status;
    std::string statusMessage;
    float lastCheck;
    float checkInterval;
    int lobbyPlayerCount;
    int averagePing;
    int matchmakingTime;
    bool inRestrictedLobby;

    AccountStatus CheckStatus() {
        if (!mem || !gameBase) return AccountStatus::UNKNOWN;

        uintptr_t lobbyInfo = mem->Read<uintptr_t>(gameBase + 0x4500000);
        if (!lobbyInfo) return AccountStatus::UNKNOWN;

        int lobbyType = mem->Read<int>(lobbyInfo + 0x20);
        lobbyPlayerCount = mem->Read<int>(lobbyInfo + 0x28);
        averagePing = mem->Read<int>(lobbyInfo + 0x2C);
        matchmakingTime = mem->Read<int>(lobbyInfo + 0x30);

        // Shadowban lobbies typically have:
        // - High ping lobbies (>150ms)
        // - Low player count lobbies
        // - Same few players repeatedly
        // - Longer matchmaking times

        inRestrictedLobby = false;

        if (lobbyPlayerCount < 20 && lobbyPlayerCount > 0) {
            inRestrictedLobby = true;
            // Likely shadowban or limited matchmaking
            return AccountStatus::LIMITED;
        }

        if (averagePing > 200) {
            // High ping = could be shadowban region
            inRestrictedLobby = true;
        }

        if (matchmakingTime > 120) {
            // Matchmaking taking >2min = restricted
            inRestrictedLobby = true;
            return AccountStatus::SHADOWBAN;
        }

        // Check for ban flags
        uintptr_t banCheck = mem->Read<uintptr_t>(gameBase + 0x5000);
        if (banCheck) {
            int banFlags = mem->Read<int>(banCheck + 0x10);
            if (banFlags & 1) return AccountStatus::BANNED;
            if (banFlags & 2) return AccountStatus::SUSPENDED;
            if (banFlags & 4) return AccountStatus::LIMITED;
        }

        return AccountStatus::HEALTHY;
    }

public:
    AccountHealth(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(true),
        status(AccountStatus::UNKNOWN), lastCheck(0),
        checkInterval(30.0f), lobbyPlayerCount(0),
        averagePing(0), matchmakingTime(0),
        inRestrictedLobby(false) {}

    void Update() {
        if (!enabled || !mem || !gameBase) return;

        float now = ImGui::GetTime();
        if ((now - lastCheck) < checkInterval) return;

        status = CheckStatus();
        lastCheck = now;

        switch (status) {
        case AccountStatus::HEALTHY:
            statusMessage = "Account: Healthy";
            break;
        case AccountStatus::SHADOWBAN:
            statusMessage = "SHADOWBAN DETECTED!";
            break;
        case AccountStatus::BANNED:
            statusMessage = "ACCOUNT BANNED";
            break;
        case AccountStatus::LIMITED:
            statusMessage = "Matchmaking limited";
            break;
        case AccountStatus::SUSPENDED:
            statusMessage = "Account suspended";
            break;
        default:
            statusMessage = "Status unknown";
        }
    }

    void RenderHUD(int screenWidth, int screenHeight) {
        if (!enabled) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImColor statusColor;
        switch (status) {
        case AccountStatus::HEALTHY:
            statusColor = ImColor(0, 255, 0, 200);
            break;
        case AccountStatus::SHADOWBAN:
            statusColor = ImColor(255, 100, 0, 255);
            break;
        case AccountStatus::BANNED:
        case AccountStatus::SUSPENDED:
            statusColor = ImColor(255, 0, 0, 255);
            break;
        case AccountStatus::LIMITED:
            statusColor = ImColor(255, 200, 0, 200);
            break;
        default:
            statusColor = ImColor(128, 128, 128, 200);
        }

        draw->AddText(ImVec2(screenWidth - 250, 60),
            statusColor, statusMessage.c_str());

        if (inRestrictedLobby) {
            draw->AddText(ImVec2(screenWidth - 250, 78),
                ImColor(255, 100, 0, 200),
                "Lobby: RESTRICTED");
        }

        if (lobbyPlayerCount > 0) {
            char buf[64];
            sprintf_s(buf, "Players: %d | Ping: %dms",
                lobbyPlayerCount, averagePing);
            draw->AddText(ImVec2(screenWidth - 250, 96),
                ImColor(180, 180, 180, 150), buf);
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    bool IsRestricted() const { return inRestrictedLobby; }
    AccountStatus GetStatus() const { return status; }
    std::string GetStatusMessage() const { return statusMessage; }
};
