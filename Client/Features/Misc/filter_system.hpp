#pragma once
#include <vector>
#include <string>
#include <map>
#include <imgui.h>

enum FilterType {
    FILTER_NONE = 0,
    FILTER_PLAYERS = 1,
    FILTER_AI = 2,
    FILTER_LOOT = 3,
    FILTER_VEHICLES = 4,
    FILTER_EXFIL = 5,
    FILTER_CONTRACTS = 6,
    FILTER_STRONGHOLDS = 7,
    FILTER_WEAPONS = 8,
    FILTER_KEYS = 9,
    FILTER_PLATES = 10,
    FILTER_CASH = 11,
    FILTER_BOSS = 12,
    FILTER_ELITE = 13,
};

struct FilterRule {
    FilterType type;
    bool enabled;
    std::string name;
    ImColor color;
    float minDistance;
    float maxDistance;
    bool visibleOnly;
    bool showName;
    bool showDistance;
};

class FilterSystem {
private:
    std::map<FilterType, FilterRule> filters;
    bool filterEnabled = true;
    bool showAll = true;
    
public:
    FilterSystem() {
        filters[FILTER_PLAYERS] = {FILTER_PLAYERS, true, "Players", ImColor(255,0,0,255), 0, 1000, false, true, true};
        filters[FILTER_AI] = {FILTER_AI, true, "AI Bots", ImColor(255,255,0,255), 0, 500, false, true, true};
        filters[FILTER_LOOT] = {FILTER_LOOT, true, "Loot", ImColor(0,255,0,255), 0, 200, false, true, true};
        filters[FILTER_VEHICLES] = {FILTER_VEHICLES, true, "Vehicles", ImColor(0,255,255,255), 0, 800, false, true, true};
        filters[FILTER_EXFIL] = {FILTER_EXFIL, true, "Exfil", ImColor(0,255,0,255), 0, 1000, false, true, true};
        filters[FILTER_CONTRACTS] = {FILTER_CONTRACTS, true, "Contracts", ImColor(255,165,0,255), 0, 1000, false, true, true};
        filters[FILTER_STRONGHOLDS] = {FILTER_STRONGHOLDS, true, "Strongholds", ImColor(128,0,128,255), 0, 1000, false, true, true};
        filters[FILTER_WEAPONS] = {FILTER_WEAPONS, true, "Weapons", ImColor(255,215,0,255), 0, 200, false, true, true};
        filters[FILTER_KEYS] = {FILTER_KEYS, true, "Keys", ImColor(0,255,255,255), 0, 200, false, true, true};
        filters[FILTER_PLATES] = {FILTER_PLATES, true, "Plates", ImColor(0,150,255,255), 0, 100, false, true, true};
        filters[FILTER_CASH] = {FILTER_CASH, true, "Cash", ImColor(0,255,0,255), 0, 100, false, true, true};
        filters[FILTER_BOSS] = {FILTER_BOSS, true, "Bosses", ImColor(255,0,0,255), 0, 1000, false, true, true};
        filters[FILTER_ELITE] = {FILTER_ELITE, true, "Elite AI", ImColor(255,165,0,255), 0, 1000, false, true, true};
    }
    
    bool IsFiltered(FilterType type, float distance = 0) {
        if (!filterEnabled) return false;
        if (showAll) return false;
        auto it = filters.find(type);
        if (it == filters.end()) return false;
        FilterRule& rule = it->second;
        if (!rule.enabled) return true;
        if (distance < rule.minDistance || distance > rule.maxDistance) return true;
        return false;
    }
    
    ImColor GetFilterColor(FilterType type) {
        auto it = filters.find(type);
        if (it != filters.end()) return it->second.color;
        return ImColor(255,255,255,255);
    }
    
    void RenderUI() {
        if (!ImGui::CollapsingHeader("🔍 FILTER SYSTEM")) return;
        ImGui::Checkbox("Filter Enabled", &filterEnabled);
        ImGui::Checkbox("Show All", &showAll);
        ImGui::Separator();
        
        for (auto& [type, rule] : filters) {
            ImGui::Checkbox(rule.name.c_str(), &rule.enabled);
            ImGui::SameLine();
            ImGui::ColorEdit4(("##" + rule.name).c_str(), (float*)&rule.color, ImGuiColorEditFlags_NoInputs);
        }
    }
};