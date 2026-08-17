#pragma once
#include <imgui.h>

namespace MenuUI {

// Default accent when no saved config exists: refined cyan.
inline ImVec4 DefaultAccent() { return ImVec4(0.00f, 0.85f, 1.00f, 1.0f); }

// Professional theme: clean dark, flat panels, subtle borders, side nav.
// Call once after ImGui context creation and again whenever the user changes
// the accent.
inline void ApplyStyle(const ImVec4& accent) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6.0f; s.FrameRounding = 3.0f; s.GrabRounding = 3.0f;
    s.PopupRounding = 5.0f; s.ScrollbarRounding = 3.0f; s.ChildRounding = 4.0f;
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f); s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6); s.WindowPadding = ImVec2(10, 10);
    s.ScrollbarSize = 6.0f; s.GrabMinSize = 8.0f;
    s.WindowBorderSize = 1.0f; s.ChildBorderSize = 1.0f; s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;

    ImVec4 a(accent.x, accent.y, accent.z, 1.0f), aH(accent.x, accent.y, accent.z, 0.85f);
    ImVec4 aD(accent.x*0.35f, accent.y*0.35f, accent.z*0.35f, 1.0f);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.045f, 0.05f, 0.065f, 0.98f);
    c[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.065f, 0.085f, 0.92f);
    c[ImGuiCol_PopupBg] = ImVec4(0.055f, 0.06f, 0.08f, 0.98f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.045f, 0.06f, 0.60f);
    c[ImGuiCol_ScrollbarGrab] = aD; c[ImGuiCol_ScrollbarGrabHovered] = aH; c[ImGuiCol_ScrollbarGrabActive] = a;
    c[ImGuiCol_CheckMark] = a;
    c[ImGuiCol_SliderGrab] = a; c[ImGuiCol_SliderGrabActive] = a;
    c[ImGuiCol_Button] = ImVec4(0.08f, 0.09f, 0.12f, 0.95f);
    c[ImGuiCol_ButtonHovered] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
    c[ImGuiCol_ButtonActive] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
    c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.18f);
    c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.32f);
    c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.48f);
    c[ImGuiCol_Tab] = ImVec4(0.055f, 0.06f, 0.08f, 0.95f);
    c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.15f);
    c[ImGuiCol_TabActive] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.055f, 0.06f, 0.08f, 0.95f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(accent.x, accent.y, accent.z, 0.24f);
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.97f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.60f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.14f, 0.15f, 0.21f, 0.75f);
    c[ImGuiCol_Border] = ImVec4(0.16f, 0.18f, 0.26f, 0.70f);
    c[ImGuiCol_FrameBg] = ImVec4(0.065f, 0.07f, 0.095f, 0.95f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.11f, 0.15f, 0.95f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.13f, 0.14f, 0.19f, 0.95f);
    c[ImGuiCol_PlotHistogram] = a;
    c[ImGuiCol_PlotHistogramHovered] = aH;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);
    // subtle accent tint on the window frame
    c[ImGuiCol_WindowBg].x = 0.045f + accent.x * 0.010f;
    c[ImGuiCol_WindowBg].y = 0.050f + accent.y * 0.010f;
    c[ImGuiCol_WindowBg].z = 0.065f + accent.z * 0.010f;
}

} // namespace MenuUI