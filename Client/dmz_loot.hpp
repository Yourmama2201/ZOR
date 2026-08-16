#pragma once
#include "memory.hpp"
#include "math.hpp"
#include "offsets.hpp"
#include <vector>
#include <string>

struct LootItem {
    Vec3 position;
    std::string name;
    int value;
    int rarity;
    bool isWeapon;
    bool isKey;
    bool isPlate;
    bool isCash;
    float distance;
};

class DZMLoot {
private:
    MemoryManager& mem;
    std::vector<LootItem> items;
    bool enabled = true;
    float maxDistance = 500.0f;

public:
    DZMLoot(MemoryManager& memory) : mem(memory) {}

    void Update() {
        items.clear();

        uintptr_t lootList = mem.Read<uintptr_t>(mem.GetBase() + Offsets::LOOT_PTR);
        if (!lootList) return;

        int count = mem.Read<int>(lootList + 0x10);
        if (count > 200) count = 200; // Sanity check

        for (int i = 0; i < count; i++) {
            uintptr_t item = mem.Read<uintptr_t>(lootList + 0x20 + (i * 0x8));
            if (!item) continue;

            LootItem loot;
            loot.position = mem.Read<Vec3>(item + 0x40);
            loot.name = mem.ReadString(item + 0x100, 32);
            loot.value = mem.Read<int>(item + 0x104);
            loot.rarity = mem.Read<int>(item + 0x108);

            // Determine item type from name
            std::string lowerName = loot.name;
            for (char& c : lowerName) c = tolower(c);

            loot.isWeapon = (lowerName.find("weapon") != std::string::npos ||
                lowerName.find("gun") != std::string::npos ||
                lowerName.find("rifle") != std::string::npos);
            loot.isKey = (lowerName.find("key") != std::string::npos);
            loot.isPlate = (lowerName.find("plate") != std::string::npos ||
                lowerName.find("armor") != std::string::npos);
            loot.isCash = (lowerName.find("cash") != std::string::npos ||
                lowerName.find("money") != std::string::npos);

            items.push_back(loot);
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetMaxDistance(float d) { maxDistance = d; }
    std::vector<LootItem>& GetItems() { return items; }

    void Render(Matrix4x4& viewMatrix, int width, int height, Vec3 localPos) {
        if (!enabled) return;

        ImDrawList* draw = ImGui::GetOverlayDrawList();
        if (!draw) return;

        for (auto& loot : items) {
            loot.distance = localPos.Distance(loot.position);
            if (loot.distance > maxDistance) continue;

            Vec2 screenPos = Math::WorldToScreen(loot.position, viewMatrix, width, height);
            if (screenPos.x < -100 || screenPos.x > width + 100) continue;

            // Color based on rarity/value
            ImColor color = ImColor(255, 255, 255, 255);
            if (loot.value > 5000) color = ImColor(255, 215, 0, 255);      // Legendary
            else if (loot.value > 1000) color = ImColor(128, 0, 255, 255); // Epic
            else if (loot.value > 500) color = ImColor(0, 100, 255, 255);  // Rare
            else if (loot.value > 100) color = ImColor(0, 255, 0, 255);    // Uncommon

            if (loot.isWeapon) color = ImColor(255, 100, 0, 255);
            if (loot.isKey) color = ImColor(0, 255, 255, 255);
            if (loot.isPlate) color = ImColor(0, 150, 255, 255);
            if (loot.isCash) color = ImColor(0, 255, 0, 255);

            // Draw item name
            draw->AddText(ImVec2(screenPos.x, screenPos.y), color, loot.name.c_str());

            // Draw value
            char valueText[32];
            sprintf_s(valueText, "$%d", loot.value);
            draw->AddText(ImVec2(screenPos.x, screenPos.y + 16), ImColor(200, 200, 200, 200), valueText);

            // Draw distance
            char distText[32];
            sprintf_s(distText, "%.0fm", loot.distance);
            draw->AddText(ImVec2(screenPos.x + 100, screenPos.y), ImColor(150, 150, 150, 150), distText);
        }
    }
};