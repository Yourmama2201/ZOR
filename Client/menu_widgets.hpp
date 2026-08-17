#pragma once
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace MenuUI {

// Loader neon palette constants (match ZORLoader.cpp).
static const ImU32 LOADER_CYAN = IM_COL32(0, 229, 255, 255);
static const ImU32 LOADER_PINK = IM_COL32(255, 0, 128, 255);
static const ImU32 LOADER_PURPLE = IM_COL32(124, 0, 255, 255);

// Sidebar nav tab: full-width vertical button, active gets accent left bar +
// subtle bg tint. Returns true if clicked.
inline bool SidebarTab(const ImVec4& accent, bool selected, const char* label,
                       const char* icon, int idx, int count) {
    (void)idx; (void)count;
    float w = ImGui::GetContentRegionAvail().x;
    std::string full = std::string(icon) + "  " + label;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        selected ? (ImVec4)ImColor(accent.x, accent.y, accent.z, 0.16f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(accent.x, accent.y, accent.z, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(accent.x, accent.y, accent.z, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_Text, selected
        ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
        : ImVec4(0.62f, 0.64f, 0.74f, 1.0f));
    bool clicked = ImGui::Button(full.c_str(), ImVec2(w, 36));
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(1);
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    if (selected) {
        // accent bar on the left edge
        d->AddRectFilled(ImVec2(mn.x - 6, mn.y + 3), ImVec2(mn.x - 3, mx.y - 3),
            ImColor(accent.x, accent.y, accent.z, 1.0f), 1.5f);
        d->AddRectFilled(ImVec2(mn.x, mn.y), ImVec2(mx.x, mx.y),
            ImColor(accent.x, accent.y, accent.z, 0.05f));
    }
    return clicked;
}

// Section header: small accent chip + label + hairline divider.
inline void Sec(const ImVec4& accent, const char* l) {
    ImGui::Spacing();
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    d->AddRectFilled(ImVec2(c.x, c.y + 2), ImVec2(c.x + 3, c.y + 14),
        ImColor(accent.x, accent.y, accent.z, 0.95f), 1.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 9);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.85f), l);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX());
    ImGui::Separator();
    ImGui::Spacing();
}

// Collapsible sub-section row. Returns whether it's open.
inline bool Sub(const ImVec4& accent, const char* l, std::unordered_map<std::string, bool>& sectionOpen) {
    auto it = sectionOpen.find(l);
    if (it == sectionOpen.end()) it = sectionOpen.emplace(l, true).first;
    bool* openPtr = &it->second;
    ImGui::TextColored(ImGui::IsItemHovered() ?
        ImVec4(accent.x, accent.y, accent.z, 1.0f) : ImVec4(0.66f, 0.68f, 0.78f, 1.00f), l);
    ImGui::SameLine();
    ImGui::TextDisabled(*openPtr ? "\xe2\x96\xb2" : "\xe2\x96\xbc");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24);
    ImGui::InvisibleButton(("##sub_" + std::string(l)).c_str(), ImVec2(20, 14));
    if (ImGui::IsItemClicked(0)) *openPtr = !*openPtr;
    bool open = *openPtr;
    if (open) ImGui::Indent(10.0f);
    return open;
}

// Clean checkbox: small rounded square, accent fill + white check when on.
inline void Switch(const ImVec4& accent, const char* l, bool* v) {
    ImGui::PushID(l);
    ImVec2 sz(16, 16);
    bool on = *v;
    ImVec2 mn = ImGui::GetCursorScreenPos();
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImU32 box = on ? ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, 0.95f))
                   : ImGui::ColorConvertFloat4ToU32(ImVec4(0.094f, 0.078f, 0.204f, 0.95f));
    d->AddRectFilled(mn, ImVec2(mn.x + sz.x, mn.y + sz.y), box, 3.0f);
    d->AddRect(mn, ImVec2(mn.x + sz.x, mn.y + sz.y),
        ImColor(accent.x, accent.y, accent.z, on ? 0.55f : 0.35f), 3.0f);
    if (on) {
        d->AddLine(ImVec2(mn.x + 3, mn.y + 8), ImVec2(mn.x + 7, mn.y + 12), IM_COL32(255, 255, 255, 255), 2.0f);
        d->AddLine(ImVec2(mn.x + 7, mn.y + 12), ImVec2(mn.x + 13, mn.y + 4), IM_COL32(255, 255, 255, 255), 2.0f);
    }
    ImGui::InvisibleButton("", sz);
    if (ImGui::IsItemClicked(0)) *v = !*v;
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::TextUnformatted(l);
}

// Loader-matched header: dark navy bar, cyan top line, magenta "ZORMENU" logo.
inline void DrawHeader(ImDrawList* draw, ImVec2 wPos, ImVec2 wSize, const ImVec4& accent,
                       float time, bool connected, int pc, int vc, int lh, int la) {
    (void)time;
    // top neon line (cyan -> magenta gradient like the loader)
    draw->AddRectFilled(ImVec2(wPos.x, wPos.y), ImVec2(wPos.x + wSize.x * 0.55f, wPos.y + 3), LOADER_CYAN);
    draw->AddRectFilled(ImVec2(wPos.x + wSize.x * 0.55f, wPos.y), ImVec2(wPos.x + wSize.x, wPos.y + 3), LOADER_PINK);
    draw->AddRectFilled(ImVec2(wPos.x, wPos.y + 3), ImVec2(wPos.x + wSize.x, wPos.y + 48),
        ImColor(0.031f, 0.031f, 0.086f, 0.98f));
    draw->AddRectFilled(ImVec2(wPos.x, wPos.y + 48), ImVec2(wPos.x + wSize.x, wPos.y + 49),
        ImColor(accent.x, accent.y, accent.z, 0.75f));

    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "ZORMENU");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(accent.x, accent.y, accent.z, 1.0f), "v4.0");
    ImGui::SameLine(104);
    ImGui::TextColored(ImVec4(0.46f, 0.48f, 0.58f, 1.0f), "DMZ CHEAT // EDDIE");

    ImGui::SameLine(wSize.x - 128);
    ImColor stC = connected ? ImColor(0.20f, 1.00f, 0.55f, 1.0f) : ImColor(1.00f, 0.30f, 0.38f, 1.0f);
    draw->AddCircleFilled(ImVec2(wPos.x + wSize.x - 116, wPos.y + 15), 3.5f, stC);
    ImGui::TextColored((ImVec4)stC, connected ? "CONNECTED" : "DISCONNECTED");

    ImGui::SetCursorPos(ImVec2(12, 30));
    ImGui::TextColored(ImVec4(0.58f, 0.60f, 0.70f, 1.0f),
        "P: %d    V: %d    HP: %d    ARMOR: %d", pc, vc, lh, la);
}

// Loader-matched footer: magenta "made by eddie" tagline.
inline void DrawFooter(ImDrawList* draw, ImVec2 wPos, ImVec2 wSize, const ImVec4& accent,
                       int pc, int vc) {
    float fh = 24.0f;
    draw->AddRectFilled(ImVec2(wPos.x, wPos.y + wSize.y - fh), ImVec2(wPos.x + wSize.x, wPos.y + wSize.y),
        ImColor(0.031f, 0.031f, 0.086f, 0.98f));
    draw->AddRectFilled(ImVec2(wPos.x, wPos.y + wSize.y - fh - 1), ImVec2(wPos.x + wSize.x, wPos.y + wSize.y - fh),
        ImColor(accent.x, accent.y, accent.z, 0.45f));
    ImGui::SetCursorPos(ImVec2(12, wSize.y - 20));
    ImGui::TextColored(ImVec4(0.52f, 0.54f, 0.64f, 1.0f), "PC: %d    VEH: %d", pc, vc);
    ImGui::SameLine(wSize.x - 250);
    ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.86f, 1.0f), "ZOR v4.0  //  made by eddie");
}

} // namespace MenuUI