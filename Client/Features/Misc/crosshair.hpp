#pragma once
#include <imgui.h>
#include <cmath>

class CustomCrosshair {
private:
    bool enabled;
    int type;
    float size;
    float thickness;
    float gap;
    bool outline;
    float color[4];

public:
    CustomCrosshair()
        : enabled(false), type(0), size(12.0f), thickness(2.0f),
        gap(4.0f), outline(true) { color[0] = 0.0f; color[1] = 1.0f; color[2] = 0.0f; color[3] = 1.0f; }

    void Render(int screenWidth, int screenHeight) {
        if (!enabled) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImColor col(color[0], color[1], color[2], color[3]);
        ImColor outlineCol(0.0f, 0.0f, 0.0f, 0.85f);
        ImVec2 center(screenWidth / 2.0f, screenHeight / 2.0f);

        switch (type) {
        case 0: {
            if (outline) {
                draw->AddLine(ImVec2(center.x - gap - size, center.y), ImVec2(center.x - gap, center.y), outlineCol, thickness + 2.0f);
                draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + size, center.y), outlineCol, thickness + 2.0f);
                draw->AddLine(ImVec2(center.x, center.y - gap - size), ImVec2(center.x, center.y - gap), outlineCol, thickness + 2.0f);
                draw->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + size), outlineCol, thickness + 2.0f);
            }
            draw->AddLine(ImVec2(center.x - gap - size, center.y), ImVec2(center.x - gap, center.y), col, thickness);
            draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + size, center.y), col, thickness);
            draw->AddLine(ImVec2(center.x, center.y - gap - size), ImVec2(center.x, center.y - gap), col, thickness);
            draw->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + size), col, thickness);
            break;
        }
        case 1: {
            if (outline) draw->AddCircle(center, size, outlineCol, 32, thickness + 2.0f);
            draw->AddCircle(center, size, col, 32, thickness);
            break;
        }
        case 2: {
            if (outline) draw->AddCircle(center, thickness + 3, outlineCol, 32, 2.0f);
            draw->AddCircleFilled(center, thickness + 1, col);
            break;
        }
        case 3: {
            if (outline) {
                draw->AddLine(ImVec2(center.x - gap - size, center.y), ImVec2(center.x - gap, center.y), outlineCol, thickness + 2.0f);
                draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + size, center.y), outlineCol, thickness + 2.0f);
                draw->AddLine(ImVec2(center.x - gap, center.y), ImVec2(center.x, center.y - gap - size), outlineCol, thickness + 2.0f);
                draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x, center.y - gap - size), outlineCol, thickness + 2.0f);
            }
            draw->AddLine(ImVec2(center.x - gap - size, center.y), ImVec2(center.x - gap, center.y), col, thickness);
            draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + size, center.y), col, thickness);
            draw->AddLine(ImVec2(center.x - gap, center.y), ImVec2(center.x, center.y - gap - size), col, thickness);
            draw->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x, center.y - gap - size), col, thickness);
            break;
        }
        case 4: {
            float len = size;
            ImVec2 c = center;
            auto bracket = [&](float ox, float oy, int signX, int signY) {
                float gx = gap, gy = gap;
                if (outline) {
                    draw->AddLine(ImVec2(c.x + ox * (gx + len), c.y + oy * (gy + len)), ImVec2(c.x + ox * gx, c.y + oy * (gy + len)), outlineCol, thickness + 2.0f);
                    draw->AddLine(ImVec2(c.x + ox * (gx + len), c.y + oy * (gy + len)), ImVec2(c.x + ox * (gx + len), c.y + oy * gy), outlineCol, thickness + 2.0f);
                }
                draw->AddLine(ImVec2(c.x + ox * (gx + len), c.y + oy * (gy + len)), ImVec2(c.x + ox * gx, c.y + oy * (gy + len)), col, thickness);
                draw->AddLine(ImVec2(c.x + ox * (gx + len), c.y + oy * (gy + len)), ImVec2(c.x + ox * (gx + len), c.y + oy * gy), col, thickness);
                (void)signX; (void)signY;
            };
            bracket(1, 1, 1, 1); bracket(-1, 1, -1, 1); bracket(1, -1, 1, -1); bracket(-1, -1, -1, -1);
            break;
        }
        case 5: {
            // ================= YOUR CUSTOM CROSSHAIR HERE =================
            // Available: center (ImVec2, screen center), col (ImColor),
            //            size, thickness, gap (floats), draw (ImDrawList*).
            // Emoji now work too (seguiemj.ttf is merged into the font):
            // draw->AddText(ImVec2(center.x - 10, center.y - 10), col, "YOUR_EMOJI_HERE");
            // ===============================================================
            break;
        }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetType(int t) { type = t; }
    void SetSize(float s) { size = s; }
    void SetThickness(float t) { thickness = t; }
    void SetGap(float g) { gap = g; }
    void SetOutline(bool o) { outline = o; }
    void SetColor(const float c[4]) { for (int i = 0; i < 4; i++) color[i] = c[i]; }
};
