#pragma once
#include "../../memory.hpp"
#include "../../offsets.hpp"
#include "../../player.hpp"
#include <vector>
#include <string>
#include <imgui.h>
#include <algorithm>

class PlayerList {
private:
    MemoryManager* mem;
    bool enabled;

public:
    PlayerList(MemoryManager* memory) : mem(memory), enabled(false) {}

    void Render(const std::vector<Player>& players, int localTeam) {
        if (!enabled || !mem) return;

        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Player List", &enabled,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

            const char* columns[] = { "Name", "Team", "Health", "Armor", "Dist", "Weapon", "Visible" };
            int colCount = 7;

            ImGui::Columns(colCount);
            for (int i = 0; i < colCount; i++) {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.00f, 1.0f), "%s", columns[i]);
                ImGui::NextColumn();
            }
            ImGui::Separator();

            for (const auto& p : players) {
                bool isEnemy = (p.GetTeam() != localTeam);

                ImColor nameColor = isEnemy
                    ? ImColor(255, 80, 80, 255)
                    : ImColor(80, 200, 80, 255);

                ImGui::TextColored((ImVec4)nameColor, "%s", p.GetName().c_str()); ImGui::NextColumn();
                ImGui::Text("%d", p.GetTeam()); ImGui::NextColumn();
                ImGui::Text("%.0f", p.GetHealth()); ImGui::NextColumn();
                ImGui::Text("%.0f", p.GetArmor()); ImGui::NextColumn();
                ImGui::Text("%.0f", p.GetDistance()); ImGui::NextColumn();
                ImGui::Text("%d", p.GetWeapon()); ImGui::NextColumn();

                if (p.IsVisible()) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Yes");
                }
                else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No");
                }
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
        }
        ImGui::End();
    }

    void SetEnabled(bool e) { enabled = e; }
    bool IsEnabled() const { return enabled; }
};
