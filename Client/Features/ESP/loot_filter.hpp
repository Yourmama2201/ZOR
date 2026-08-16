#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include "../../memory.hpp"
#include "../../offsets.hpp"

struct LootFilterEntry {
    std::string name;
    bool enabled;
    bool highlight;
    ImColor color;
    float minDistance;
    float maxDistance;

    LootFilterEntry() : enabled(true), highlight(false),
        color(255, 215, 0, 255), minDistance(0), maxDistance(500) {}
};

class LootFilter {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool autoPickup;
    float autoPickupRange;
    std::vector<LootFilterEntry> filters;

    void LoadDefaultFilters() {
        filters.clear();

        auto add = [&](const std::string& n, bool en, bool hl, ImColor c, float maxD) {
            LootFilterEntry e;
            e.name = n; e.enabled = en; e.highlight = hl;
            e.color = c; e.maxDistance = maxD;
            filters.push_back(e);
            };

        // Weapons
        add("weapon", true, true, ImColor(0, 200, 255, 255), 300);
        add("ammo", true, false, ImColor(255, 200, 0, 200), 200);
        add("armor", true, true, ImColor(0, 255, 0, 255), 200);
        add("plate", true, true, ImColor(0, 255, 100, 255), 200);
        add("cash", true, true, ImColor(255, 215, 0, 255), 150);
        add("key", true, true, ImColor(255, 0, 255, 255), 300);
        add("killstreak", true, true, ImColor(255, 100, 0, 255), 300);
        add("self revive", true, true, ImColor(255, 0, 100, 255), 200);
        add("gpu", true, true, ImColor(0, 255, 200, 255), 300);
        add("drill", true, true, ImColor(255, 200, 0, 255), 200);
        add("bandage", true, false, ImColor(255, 255, 255, 200), 150);
        add("grenade", true, false, ImColor(200, 100, 0, 200), 150);
        add("stim", true, false, ImColor(200, 200, 200, 200), 150);
        add("c4", true, false, ImColor(255, 80, 80, 200), 150);
        add("contract", true, true, ImColor(255, 165, 0, 255), 400);
        add("backpack", true, true, ImColor(100, 200, 255, 255), 200);
    }

public:
    LootFilter(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(true),
        autoPickup(false), autoPickupRange(5.0f) {
        LoadDefaultFilters();
    }

    std::vector<LootFilterEntry>& GetFilters() { return filters; }

    bool ShouldRender(const std::string& itemName, int rarity, float distance, ImColor& outColor) {
        if (!enabled) return true;

        for (auto& f : filters) {
            if (!f.enabled) continue;
            if (distance < f.minDistance || distance > f.maxDistance) continue;

            // Check if item name contains filter keyword
            std::string lowerItem = itemName;
            std::string lowerFilter = f.name;
            std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(), ::tolower);
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

            if (lowerItem.find(lowerFilter) != std::string::npos) {
                outColor = f.highlight ? ImColor(255, 0, 255, 255) : f.color;
                return true;
            }
        }
        return false;
    }

    void AutoPickup() {
        if (!autoPickup || !mem || !gameBase) return;

        uintptr_t lootPtr = mem->Read<uintptr_t>(gameBase + Offsets::LOOT_PTR);
        if (!lootPtr) return;

        uintptr_t localPosPtr = gameBase + Offsets::LOCAL_POS;
        Vec3 localPos = mem->Read<Vec3>(localPosPtr);

        for (int i = 0; i < 100; i++) {
            uintptr_t item = mem->Read<uintptr_t>(lootPtr + (i * 0x8));
            if (!item) continue;

            Vec3 itemPos = mem->Read<Vec3>(item + 0x50);
            float dist = localPos.Distance(itemPos);
            if (dist > autoPickupRange) continue;

            // Get item name
            std::string name = mem->ReadString(item + 0x200, 32);

            // Check if we should pick up this item
            for (auto& f : filters) {
                if (!f.enabled) continue;
                std::string lowerItem = name;
                std::string lowerFilter = f.name;
                std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                if (lowerItem.find(lowerFilter) != std::string::npos) {
                    // Simulate 'F' key press to pick up
                    // Write USE/INTERACT action
                    uintptr_t cmdArray = mem->Read<uintptr_t>(gameBase + Offsets::CMD_ARRAY);
                    if (cmdArray) {
                        mem->Write<uint8_t>(cmdArray + 0x60, 1); // use key
                    }
                    break;
                }
            }
        }
    }

    void RenderUI() {
        if (!ImGui::Begin("Loot Filter", &enabled,
            ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::End();
            return;
        }

        ImGui::Checkbox("Enable Loot Filter", &enabled);
        ImGui::Checkbox("Auto Pickup", &autoPickup);
        if (autoPickup) {
            ImGui::SliderFloat("Pickup Range", &autoPickupRange, 1.0f, 20.0f, "%.0f m");
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.00f, 1.0f), "Item Filters");
        ImGui::BeginChild("Filters", ImVec2(350, 300), true);

        for (auto& f : filters) {
            ImGui::PushID(&f);
            ImGui::Checkbox("##en", &f.enabled);
            ImGui::SameLine();
            ImGui::TextColored((ImVec4)f.color, "%s", f.name.c_str());
            ImGui::SameLine(200);
            ImGui::Checkbox("Highlight", &f.highlight);
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetAutoPickup(bool a) { autoPickup = a; }
    void SetAutoPickupRange(float r) { autoPickupRange = r; }
    bool IsEnabled() const { return enabled; }
};
