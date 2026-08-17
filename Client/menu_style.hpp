#pragma once
#include <imgui.h>

namespace MenuUI {

// Default accent when no saved config exists: loader neon cyan.
inline ImVec4 DefaultAccent() { return ImVec4(0.00f, 0.90f, 1.00f, 1.0f); }

// Loader-matched theme: dark navy/purple gradient, neon cyan + magenta accents,
// soft rounded corners, thin cyan borders. Call once after ImGui context
// creation and again whenever the user changes the accent.
inline void ApplyStyle(const ImVec4& accent) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f; s.FrameRounding = 4.0f; s.GrabRounding = 4.0f;
    s.PopupRounding = 6.0f; s.ScrollbarRounding = 4.0f; s.ChildRounding = 6.0f;
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f); s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6); s.WindowPadding = ImVec2(10, 10);
    s.ScrollbarSize = 6.0f; s.GrabMinSize = 8.0f;
    s.WindowBorderSize = 1.0f; s.ChildBorderSize = 1.0f; s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;

    ImVec4 a(accent.x, accent.y, accent.z, 1.0f), aH(accent.x, accent.y, accent.z, 0.85f);
    ImVec4 aD(accent.x*0.35f, accent.y*0.35f, accent.z*0.35f, 1.0f);
    // Loader neon magenta highlight.
    ImVec4 pink(1.00f, 0.00f, 0.50f, 1.0f);
    ImVec4 purple(0.49f, 0.00f, 1.00f, 1.0f);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.031f, 0.031f, 0.086f, 0.98f);
    c[ImGuiCol_ChildBg] = ImVec4(0.094f, 0.078f, 0.204f, 0.92f);
    c[ImGuiCol_PopupBg] = ImVec4(0.031f, 0.031f, 0.086f, 0.98f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.045f, 0.06f, 0.60f);
    c[ImGuiCol_ScrollbarGrab] = aD; c[ImGuiCol_ScrollbarGrabHovered] = aH; c[ImGuiCol_ScrollbarGrabActive] = a;
    c[ImGuiCol_CheckMark] = a;
    c[ImGuiCol_SliderGrab] = a; c[ImGuiCol_SliderGrabActive] = a;
    c[ImGuiCol_Button] = ImVec4(0.094f, 0.078f, 0.204f, 0.95f);
    c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
    c[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
    c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.18f);
    c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.32f);
    c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.48f);
    c[ImGuiCol_Tab] = ImVec4(0.031f, 0.031f, 0.086f, 0.95f);
    c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.15f);
    c[ImGuiCol_TabActive] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.031f, 0.031f, 0.086f, 0.95f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(accent.x, accent.y, accent.z, 0.24f);
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.63f, 0.63f, 0.71f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.00f, 0.90f, 1.00f, 0.35f);
    c[ImGuiCol_Border] = ImVec4(0.00f, 0.90f, 1.00f, 0.55f);
    c[ImGuiCol_FrameBg] = ImVec4(0.094f, 0.078f, 0.204f, 0.95f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.11f, 0.26f, 0.95f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.14f, 0.32f, 0.95f);
    c[ImGuiCol_PlotHistogram] = a;
    c[ImGuiCol_PlotHistogramHovered] = aH;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
    // Loader magenta hints on selected text.
    (void)pink; (void)purple;
}

} // namespace MenuUI
