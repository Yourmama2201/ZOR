#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../offsets.hpp"
#include <vector>
#include <string>
#include <imgui.h>

struct ExfilPoint {
    Vec3 position;
    std::string name;
    bool isActive;
    bool isAvailable;
    float extractionTime;
    float distance;
};

struct BuyStation {
    Vec3 position;
    std::string name;
    bool isActive;
    float distance;
};

struct Stronghold {
    Vec3 position;
    std::string name;
    int threatLevel;
    bool isActive;
    bool isLocked;
    float distance;
};

struct SamSite {
    Vec3 position;
    bool isActive;
    bool onCooldown;
    float distance;
};

struct SupplyDrop {
    Vec3 position;
    std::string contents;
    float distance;
};

struct BossNPC {
    Vec3 position;
    std::string name;
    int health;
    int type;
    float distance;
};

class GameWorldESP {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool drawExfils;
    bool drawContracts;
    bool drawStrongholds;
    bool drawBuyStations;
    bool drawSamSites;
    bool drawSupplyDrops;
    bool drawBosses;
    bool drawDeadBodies;
    float maxExfilDistance;
    float maxContractDistance;

public:
    GameWorldESP(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(true),
        drawExfils(true), drawContracts(true), drawStrongholds(true),
        drawBuyStations(true), drawSamSites(true), drawSupplyDrops(true),
        drawBosses(true), drawDeadBodies(true),
        maxExfilDistance(800.0f), maxContractDistance(500.0f) {}

    std::vector<ExfilPoint> GetExfilPoints() {
        std::vector<ExfilPoint> exfils;
        uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
        if (!activeState) return exfils;
        for (int i = 0; i < 12; i++) {
            uintptr_t exfilPtr = mem->Read<uintptr_t>(activeState + 0x100 + (i * 0x8));
            if (!exfilPtr) continue;
            ExfilPoint exfil;
            exfil.position = mem->Read<Vec3>(exfilPtr + 0x40);
            exfil.name = mem->ReadString(exfilPtr + 0x80, 24);
            exfil.isActive = mem->Read<uint8_t>(exfilPtr + 0x70) != 0;
            exfil.isAvailable = mem->Read<uint8_t>(exfilPtr + 0x71) != 0;
            exfil.extractionTime = mem->Read<float>(exfilPtr + 0x74);
            exfils.push_back(exfil);
        }
        return exfils;
    }

    std::vector<BuyStation> GetBuyStations() {
        std::vector<BuyStation> stations;
        uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
        if (!activeState) return stations;
        for (int i = 0; i < 10; i++) {
            uintptr_t ptr = mem->Read<uintptr_t>(activeState + 0x400 + (i * 0x8));
            if (!ptr) continue;
            BuyStation bs;
            bs.position = mem->Read<Vec3>(ptr + 0x40);
            bs.name = mem->ReadString(ptr + 0x80, 20);
            bs.isActive = mem->Read<uint8_t>(ptr + 0x70) != 0;
            stations.push_back(bs);
        }
        return stations;
    }

    std::vector<Stronghold> GetStrongholds() {
        std::vector<Stronghold> strongholds;
        uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
        if (!activeState) return strongholds;
        for (int i = 0; i < 8; i++) {
            uintptr_t ptr = mem->Read<uintptr_t>(activeState + 0x500 + (i * 0x8));
            if (!ptr) continue;
            Stronghold sh;
            sh.position = mem->Read<Vec3>(ptr + 0x40);
            sh.name = mem->ReadString(ptr + 0x80, 20);
            sh.isActive = mem->Read<uint8_t>(ptr + 0x70) != 0;
            sh.isLocked = mem->Read<uint8_t>(ptr + 0x71) != 0;
            sh.threatLevel = mem->Read<int>(ptr + 0x74);
            strongholds.push_back(sh);
        }
        return strongholds;
    }

    std::vector<SamSite> GetSamSites() {
        std::vector<SamSite> sites;
        uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
        if (!activeState) return sites;
        for (int i = 0; i < 6; i++) {
            uintptr_t ptr = mem->Read<uintptr_t>(activeState + 0x600 + (i * 0x8));
            if (!ptr) continue;
            SamSite ss;
            ss.position = mem->Read<Vec3>(ptr + 0x40);
            ss.isActive = mem->Read<uint8_t>(ptr + 0x70) != 0;
            ss.onCooldown = mem->Read<uint8_t>(ptr + 0x71) != 0;
            sites.push_back(ss);
        }
        return sites;
    }

    std::vector<SupplyDrop> GetSupplyDrops() {
        std::vector<SupplyDrop> drops;
        uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
        if (!activeState) return drops;
        for (int i = 0; i < 10; i++) {
            uintptr_t ptr = mem->Read<uintptr_t>(activeState + 0x700 + (i * 0x8));
            if (!ptr) continue;
            SupplyDrop sd;
            sd.position = mem->Read<Vec3>(ptr + 0x40);
            sd.contents = mem->ReadString(ptr + 0x80, 24);
            drops.push_back(sd);
        }
        return drops;
    }

    std::vector<Vec3> GetBuyStationPositions() {
        std::vector<Vec3> positions;
        auto stations = GetBuyStations();
        for (auto& s : stations) positions.push_back(s.position);
        return positions;
    }

    void Render(Matrix4x4& viewMatrix, int width, int height, Vec3 localPos) {
        if (!enabled || !mem) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        if (drawExfils) {
            auto exfils = GetExfilPoints();
            for (auto& exfil : exfils) {
                exfil.distance = localPos.Distance(exfil.position);
                if (exfil.distance > maxExfilDistance) continue;
                Vec2 screen = Math::WorldToScreen(exfil.position, viewMatrix, width, height);
                if (screen.x < -100) continue;
                ImColor color = exfil.isAvailable ? ImColor(0, 255, 0, 255) : ImColor(255, 0, 0, 255);
                draw->AddRectFilled(ImVec2(screen.x - 15, screen.y - 10), ImVec2(screen.x + 15, screen.y + 10), ImColor(0, 0, 0, 150), 2.0f);
                std::string label = exfil.name + " [" + std::to_string((int)exfil.distance) + "m]";
                draw->AddText(ImVec2(screen.x - 30, screen.y - 25), color, label.c_str());
                draw->AddRect(ImVec2(screen.x - 15, screen.y - 10), ImVec2(screen.x + 15, screen.y + 10), color, 2.0f);
                if (exfil.isAvailable) {
                    draw->AddCircle(ImVec2(screen.x, screen.y), (1.0f - exfil.extractionTime / 60.0f) * 20.0f + 15.0f, color, 32, 2.0f);
                }
            }
        }

        if (drawContracts) {
            uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
            if (activeState) {
                for (int i = 0; i < 20; i++) {
                    uintptr_t contractPtr = mem->Read<uintptr_t>(activeState + 0x200 + (i * 0x8));
                    if (!contractPtr) continue;
                    Vec3 pos = mem->Read<Vec3>(contractPtr + 0x40);
                    float dist = localPos.Distance(pos);
                    if (dist > maxContractDistance) continue;
                    Vec2 screen = Math::WorldToScreen(pos, viewMatrix, width, height);
                    if (screen.x < -100) continue;
                    std::string type = mem->ReadString(contractPtr + 0x80, 20);
                    int reward = mem->Read<int>(contractPtr + 0x90);
                    draw->AddRectFilled(ImVec2(screen.x - 12, screen.y - 8), ImVec2(screen.x + 12, screen.y + 8), ImColor(0, 0, 0, 150), 2.0f);
                    char label[64]; sprintf_s(label, "%s [$%d] %.0fm", type.c_str(), reward, dist);
                    draw->AddText(ImVec2(screen.x - 25, screen.y - 25), ImColor(255, 165, 0, 255), label);
                    draw->AddRect(ImVec2(screen.x - 12, screen.y - 8), ImVec2(screen.x + 12, screen.y + 8), ImColor(255, 165, 0, 255), 2.0f);
                }
            }
        }

        if (drawBuyStations) {
            auto stations = GetBuyStations();
            for (auto& bs : stations) {
                bs.distance = localPos.Distance(bs.position);
                if (bs.distance > 500.0f) continue;
                Vec2 screen = Math::WorldToScreen(bs.position, viewMatrix, width, height);
                if (screen.x < -100) continue;
                draw->AddRectFilled(ImVec2(screen.x - 14, screen.y - 8), ImVec2(screen.x + 14, screen.y + 8), ImColor(0, 0, 0, 150), 2.0f);
                std::string label = bs.name + " [" + std::to_string((int)bs.distance) + "m]";
                draw->AddText(ImVec2(screen.x - 20, screen.y - 22), ImColor(0, 150, 255, 255), label.c_str());
                draw->AddRect(ImVec2(screen.x - 14, screen.y - 8), ImVec2(screen.x + 14, screen.y + 8), ImColor(0, 150, 255, 255), 2.0f);
            }
        }

        if (drawStrongholds) {
            auto strongholds = GetStrongholds();
            for (auto& sh : strongholds) {
                sh.distance = localPos.Distance(sh.position);
                if (sh.distance > 500.0f) continue;
                Vec2 screen = Math::WorldToScreen(sh.position, viewMatrix, width, height);
                if (screen.x < -100) continue;
                ImColor color = sh.isLocked ? ImColor(255, 0, 0, 255) : ImColor(128, 0, 128, 255);
                draw->AddRectFilled(ImVec2(screen.x - 14, screen.y - 8), ImVec2(screen.x + 14, screen.y + 8), ImColor(0, 0, 0, 150), 2.0f);
                std::string label = sh.name + " [" + std::to_string((int)sh.distance) + "m]";
                draw->AddText(ImVec2(screen.x - 20, screen.y - 22), color, label.c_str());
                draw->AddRect(ImVec2(screen.x - 14, screen.y - 8), ImVec2(screen.x + 14, screen.y + 8), color, 2.0f);
            }
        }

        if (drawSamSites) {
            auto sites = GetSamSites();
            for (auto& ss : sites) {
                ss.distance = localPos.Distance(ss.position);
                if (ss.distance > 500.0f) continue;
                Vec2 screen = Math::WorldToScreen(ss.position, viewMatrix, width, height);
                if (screen.x < -100) continue;
                ImColor color = ss.isActive ? ImColor(0, 255, 100, 255) : ImColor(100, 100, 100, 255);
                draw->AddText(ImVec2(screen.x - 20, screen.y - 10), color, ss.onCooldown ? "SAM [COOLDOWN]" : "SAM [ACTIVE]");
            }
        }

        if (drawSupplyDrops) {
            auto drops = GetSupplyDrops();
            for (auto& sd : drops) {
                sd.distance = localPos.Distance(sd.position);
                if (sd.distance > 500.0f) continue;
                Vec2 screen = Math::WorldToScreen(sd.position, viewMatrix, width, height);
                if (screen.x < -100) continue;
                draw->AddCircleFilled(ImVec2(screen.x, screen.y), 6.0f, ImColor(255, 215, 0, 200));
                std::string label = "Supply: " + sd.contents + " [" + std::to_string((int)sd.distance) + "m]";
                draw->AddText(ImVec2(screen.x - 20, screen.y - 20), ImColor(255, 215, 0, 255), label.c_str());
            }
        }

        if (drawBosses) {
            uintptr_t entityList = mem->Read<uintptr_t>(gameBase + Offsets::DISTRIBUTE);
            if (entityList) {
                for (int i = 0; i < 100; i++) {
                    uintptr_t entity = mem->Read<uintptr_t>(entityList + (i * 0x8));
                    if (!entity) continue;
                    if (!mem->Read<uint8_t>(entity + Offsets::PLAYER_VALID)) continue;
                    uint8_t dead1 = mem->Read<uint8_t>(entity + Offsets::PLAYER_DEAD_1);
                    float hp = mem->Read<float>(entity + Offsets::PLAYER_HEALTH);
                    if (dead1 || hp <= 0) continue;

                    std::string name = mem->ReadString(entity + 0x800, 32);
                    bool isBoss = (name.find("Juggernaut") != std::string::npos ||
                        name.find("Commander") != std::string::npos ||
                        name.find("Chemist") != std::string::npos ||
                        name.find("Boss") != std::string::npos ||
                        name.find("Scavenger") != std::string::npos ||
                        name.find("Warlord") != std::string::npos);
                    if (!isBoss) continue;

                    Vec3 pos = mem->Read<Vec3>(entity + Offsets::PLAYER_POS);
                    float dist = localPos.Distance(pos);
                    if (dist > 500.0f) continue;
                    Vec2 screen = Math::WorldToScreen(pos, viewMatrix, width, height);
                    if (screen.x < -100) continue;

                    draw->AddCircleFilled(ImVec2(screen.x, screen.y), 8.0f, ImColor(255, 0, 0, 200));
                    draw->AddCircle(ImVec2(screen.x, screen.y), 16.0f, ImColor(255, 0, 0, 100), 0, 2.0f);
                    char label[64]; sprintf_s(label, "%s [%.0fHP] %.0fm", name.c_str(), hp, dist);
                    draw->AddText(ImVec2(screen.x - 25, screen.y - 28), ImColor(255, 50, 50, 255), label);
                }
            }
        }

        if (drawDeadBodies) {
            uintptr_t activeState = mem->Read<uintptr_t>(gameBase + Offsets::ACTIVE_STATE);
            if (activeState) {
                for (int i = 0; i < 30; i++) {
                    uintptr_t bodyPtr = mem->Read<uintptr_t>(activeState + 0x300 + (i * 0x8));
                    if (!bodyPtr) continue;
                    Vec3 pos = mem->Read<Vec3>(bodyPtr + 0x40);
                    float dist = localPos.Distance(pos);
                    if (dist > 200.0f) continue;
                    Vec2 screen = Math::WorldToScreen(pos, viewMatrix, width, height);
                    if (screen.x < -100) continue;
                    draw->AddCircleFilled(ImVec2(screen.x, screen.y), 4.0f, ImColor(255, 50, 50, 180));
                    char distStr[32]; sprintf_s(distStr, "%.0fm", dist);
                    draw->AddText(ImVec2(screen.x - 10, screen.y - 18), ImColor(255, 100, 100, 150), distStr);
                }
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetDrawExfils(bool d) { drawExfils = d; }
    void SetDrawContracts(bool d) { drawContracts = d; }
    void SetDrawStrongholds(bool d) { drawStrongholds = d; }
    void SetDrawBuyStations(bool d) { drawBuyStations = d; }
    void SetDrawSamSites(bool d) { drawSamSites = d; }
    void SetDrawSupplyDrops(bool d) { drawSupplyDrops = d; }
    void SetDrawBosses(bool d) { drawBosses = d; }
    void SetDrawDeadBodies(bool d) { drawDeadBodies = d; }
};
