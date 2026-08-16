#pragma once
#include <imgui.h>
#include <chrono>
#include <string>

class SessionTimer {
private:
    bool enabled;
    std::chrono::steady_clock::time_point start;

public:
    SessionTimer() : enabled(false), start(std::chrono::steady_clock::now()) {}

    void SetEnabled(bool e) { enabled = e; }

    void Render(int screenWidth, int screenHeight) {
        if (!enabled) return;

        auto now = std::chrono::steady_clock::now();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

        std::string text;
        text += "Session: ";
        long long h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
        char buf[32];
        sprintf_s(buf, "%02lld:%02lld:%02lld", h, m, s);
        text += buf;

        char fps[16];
        sprintf_s(fps, "  FPS: %.0f", ImGui::GetIO().Framerate);
        text += fps;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImVec2 size = ImGui::CalcTextSize(text.c_str());
        ImVec2 pos((float)screenWidth - size.x - 16.0f, (float)screenHeight - 26.0f);
        draw->AddText(pos, ImColor(0.0f, 0.85f, 0.20f, 0.9f), text.c_str());
    }
};