#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../player.hpp"
#include "../../offsets.hpp"
#include <vector>
#include <imgui.h>

struct BoneConnection {
    int bone1;
    int bone2;
    ImColor color;
};

class SkeletonESP {
private:
    MemoryManager* mem;
    bool enabled;
    bool drawBones;
    bool drawJoints;
    bool drawHealthColor;
    float boneThickness;
    ImColor boneColor;
    std::vector<BoneConnection> connections;

public:
    SkeletonESP(MemoryManager* memory)
        : mem(memory), enabled(true), drawBones(true), drawJoints(true),
        drawHealthColor(true), boneThickness(1.5f),
        boneColor(ImColor(255, 255, 255, 200)) {

        connections = {
            {Offsets::HEAD, Offsets::NECK, ImColor(255,255,255,200)},
            {Offsets::NECK, Offsets::SPINE1, ImColor(255,255,255,200)},
            {Offsets::SPINE1, Offsets::LEFT_SHOULDER, ImColor(255,255,255,200)},
            {Offsets::SPINE1, Offsets::RIGHT_SHOULDER, ImColor(255,255,255,200)},
            {Offsets::LEFT_SHOULDER, Offsets::LEFT_ELBOW, ImColor(255,255,255,200)},
            {Offsets::RIGHT_SHOULDER, Offsets::RIGHT_ELBOW, ImColor(255,255,255,200)},
            {Offsets::LEFT_ELBOW, Offsets::LEFT_HAND, ImColor(255,255,255,200)},
            {Offsets::RIGHT_ELBOW, Offsets::RIGHT_HAND, ImColor(255,255,255,200)},
            {Offsets::ROOT, Offsets::LEFT_KNEE, ImColor(255,255,255,200)},
            {Offsets::ROOT, Offsets::RIGHT_KNEE, ImColor(255,255,255,200)},
            {Offsets::LEFT_KNEE, Offsets::LEFT_FOOT, ImColor(255,255,255,200)},
            {Offsets::RIGHT_KNEE, Offsets::RIGHT_FOOT, ImColor(255,255,255,200)},
        };
    }

    void Render(std::vector<Player>& players, Matrix4x4& viewMatrix,
        int width, int height, int localTeam) {
        if (!enabled || !mem) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        for (auto& player : players) {
            if (!player.IsAlive()) continue;
            if (player.GetTeam() == localTeam) continue;

            std::vector<Vec3> skeleton = player.GetSkeleton();
            if (skeleton.size() < 18) continue;

            std::vector<Vec2> screenBones;
            for (auto& bone : skeleton) {
                screenBones.push_back(Math::WorldToScreen(bone, viewMatrix, width, height));
            }

            // Health-based line color
            float hp = player.GetHealth() / 100.0f;
            if (hp < 0) hp = 0;
            ImColor lineColor = drawHealthColor
                ? ImColor((int)(255 * (1 - hp)), (int)(255 * hp), 0, 210)
                : boneColor;
            ImColor outlineColor = ImColor(0, 0, 0, 180);

            // Two-pass render: black outline underneath for readability on any bg
            for (int pass = 0; pass < 2; pass++) {
                ImColor col = (pass == 0) ? outlineColor : lineColor;
                float th = (pass == 0) ? boneThickness + 2.5f : boneThickness;

                if (drawBones) {
                    for (auto& conn : connections) {
                        if (conn.bone1 >= (int)screenBones.size() || conn.bone2 >= (int)screenBones.size())
                            continue;

                        Vec2 p1 = screenBones[conn.bone1];
                        Vec2 p2 = screenBones[conn.bone2];

                        if (p1.x < -100 && p2.x < -100) continue;

                        draw->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), col, th);
                    }
                }

                if (drawJoints) {
                    for (int i = 0; i < 18 && i < (int)screenBones.size(); i++) {
                        if (screenBones[i].x < -100) continue;
                        float r = (pass == 0) ? 3.0f : 2.0f;
                        draw->AddCircleFilled(ImVec2(screenBones[i].x, screenBones[i].y),
                            r, col);
                    }
                }
            }

            // Head highlight ring
            if (drawJoints && screenBones[Offsets::HEAD].x > -100) {
                draw->AddCircle(ImVec2(screenBones[Offsets::HEAD].x, screenBones[Offsets::HEAD].y),
                    6.0f, lineColor, 14, 1.5f);
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetDrawBones(bool b) { drawBones = b; }
    void SetDrawJoints(bool j) { drawJoints = j; }
    void SetBoneThickness(float t) { boneThickness = t; }
};
