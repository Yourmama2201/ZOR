#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <imgui.h>
#include "../../memory.hpp"
#include "../../math.hpp"

struct SoundEvent {
    Vec3 position;
    std::string type;
    float time;
    float duration;
    float intensity;
    float distance;
    ImColor color;
};

class SoundESP {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool showFootsteps;
    bool showGunshots;
    bool showExplosions;
    bool showVehicleSounds;
    float maxSoundDistance;
    float soundTimeout;
    std::vector<SoundEvent> sounds;

public:
    SoundESP(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false),
        showFootsteps(true), showGunshots(true), showExplosions(true),
        showVehicleSounds(true), maxSoundDistance(300.0f), soundTimeout(3.0f) {}

    void Update(Vec3 localPos) {
        if (!enabled || !mem || !gameBase) return;

        // Clean old sounds
        float now = ImGui::GetTime();
        sounds.erase(std::remove_if(sounds.begin(), sounds.end(),
            [&](SoundEvent& s) { return (now - s.time) > s.duration; }), sounds.end());

        // Read sound events from game memory
        uintptr_t audioSystem = mem->Read<uintptr_t>(gameBase + 0x5000000); // audio sys ptr
        if (!audioSystem) return;

        // Scan sound emitter list
        for (int i = 0; i < 64; i++) {
            uintptr_t emitter = mem->Read<uintptr_t>(audioSystem + 0x100 + (i * 0x8));
            if (!emitter) continue;

            int soundType = mem->Read<int>(emitter + 0x20);
            Vec3 pos = mem->Read<Vec3>(emitter + 0x30);
            float intensity = mem->Read<float>(emitter + 0x40);
            float dist = localPos.Distance(pos);

            if (dist > maxSoundDistance) continue;
            if (intensity < 0.1f) continue;

            std::string typeName;
            ImColor color;

            switch (soundType) {
            case 1:
                if (!showFootsteps) continue;
                typeName = "Footstep";
                color = ImColor(200, 200, 200, 200);
                break;
            case 2:
                if (!showGunshots) continue;
                typeName = "Gunshot";
                color = ImColor(255, 200, 0, 255);
                break;
            case 3:
                if (!showExplosions) continue;
                typeName = "Explosion";
                color = ImColor(255, 50, 50, 255);
                break;
            case 4:
                if (!showVehicleSounds) continue;
                typeName = "Vehicle";
                color = ImColor(0, 100, 255, 255);
                break;
            default:
                continue;
            }

            // Check if already exists (avoid duplicates)
            bool exists = false;
            for (auto& s : sounds) {
                if (s.position.Distance(pos) < 5.0f &&
                    s.type == typeName &&
                    (now - s.time) < 0.5f) {
                    exists = true;
                    s.time = now;
                    s.intensity = max(s.intensity, intensity);
                    break;
                }
            }

            if (!exists) {
                SoundEvent se;
                se.position = pos;
                se.type = typeName;
                se.time = now;
                se.duration = soundTimeout;
                se.intensity = intensity;
                se.distance = dist;
                se.color = color;
                sounds.push_back(se);
            }
        }
    }

    void Render(Matrix4x4& viewMatrix, int width, int height) {
        if (!enabled || sounds.empty()) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        float now = ImGui::GetTime();

        for (auto& s : sounds) {
            float age = (now - s.time);
            float alpha = 1.0f - (age / s.duration);
            if (alpha <= 0) continue;

            Vec2 screen = Math::WorldToScreen(s.position, viewMatrix, width, height);
            if (screen.x < -100) continue;

            ImColor c = s.color;
            c.Value.w = alpha * 0.8f;

            float pulse = sinf(age * 8.0f) * 2.0f + 4.0f;
            draw->AddCircleFilled(ImVec2(screen.x, screen.y),
                pulse + (s.intensity * 3.0f), ImColor(1.0f, 1.0f, 1.0f, alpha * 0.15f));
            draw->AddCircle(ImVec2(screen.x, screen.y),
                pulse + (s.intensity * 3.0f), c, 0, 1.5f);

            // Icon/text
            const char* icon = "";
            if (s.type == "Footstep") icon = ">";
            else if (s.type == "Gunshot") icon = "*";
            else if (s.type == "Explosion") icon = "!";
            else if (s.type == "Vehicle") icon = "V";

            draw->AddText(ImVec2(screen.x - 10, screen.y - 20),
                c, icon);

            char distStr[16];
            sprintf_s(distStr, "%.0fm", s.distance);
            draw->AddText(ImVec2(screen.x - 12, screen.y + 8),
                ImColor(1.0f, 1.0f, 1.0f, alpha * 0.5f), distStr);
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetShowFootsteps(bool s) { showFootsteps = s; }
    void SetShowGunshots(bool s) { showGunshots = s; }
    void SetShowExplosions(bool s) { showExplosions = s; }
    void SetShowVehicleSounds(bool s) { showVehicleSounds = s; }
    void SetMaxDistance(float d) { maxSoundDistance = d; }
    bool IsEnabled() const { return enabled; }
};
