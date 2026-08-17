#pragma once
#include <vector>
#include <string>
#include <functional>
#include <imgui.h>
#include <windows.h>
#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

// Controller button identifiers (XInput)
enum GamepadBtn {
    GP_DPAD_UP = 0, GP_DPAD_DOWN, GP_DPAD_LEFT, GP_DPAD_RIGHT,
    GP_START, GP_BACK, GP_LS, GP_RS, GP_LB, GP_RB,
    GP_A, GP_B, GP_X, GP_Y, GP_NONE = 0xFFFF
};

// Single bind: either a keyboard/mouse key or a controller button, or both.
struct Keybind {
    std::string name;
    std::string label;
    int key;               // VK code (keyboard/mouse). 0 = none
    int padKey;            // GamepadBtn. 0xFFFF = none
    int defaultKey;
    int defaultPadKey;
    bool* toggle;
    std::function<void()> onPress;
    bool justPressed;
    bool prevState;
    bool padPrevState;

    Keybind(const std::string& n, const std::string& lbl, int dk, bool* t = nullptr)
        : name(n), label(lbl), key(dk), padKey(GP_NONE), defaultKey(dk), defaultPadKey(GP_NONE),
        toggle(t), justPressed(false), prevState(false), padPrevState(false) {}
};

class KeybindManager {
private:
    std::vector<Keybind> binds;
    int editingIndex;
    bool waitingForKey;
    bool waitingForPad;
    int lastDevice;         // 0 = unknown, 1 = keyboard/mouse, 2 = controller

    static bool PadConnected() {
        for (DWORD i = 0; i < 4; i++) {
            XINPUT_STATE st;
            if (XInputGetState(i, &st) == ERROR_SUCCESS) return true;
        }
        return false;
    }

    static int PollPad() {
        XINPUT_STATE st;
        for (DWORD i = 0; i < 4; i++) {
            if (XInputGetState(i, &st) != ERROR_SUCCESS) continue;
            WORD w = st.Gamepad.wButtons;
            if (w & XINPUT_GAMEPAD_DPAD_UP) return GP_DPAD_UP;
            if (w & XINPUT_GAMEPAD_DPAD_DOWN) return GP_DPAD_DOWN;
            if (w & XINPUT_GAMEPAD_DPAD_LEFT) return GP_DPAD_LEFT;
            if (w & XINPUT_GAMEPAD_DPAD_RIGHT) return GP_DPAD_RIGHT;
            if (w & XINPUT_GAMEPAD_START) return GP_START;
            if (w & XINPUT_GAMEPAD_BACK) return GP_BACK;
            if (w & XINPUT_GAMEPAD_LEFT_THUMB) return GP_LS;
            if (w & XINPUT_GAMEPAD_RIGHT_THUMB) return GP_RS;
            if (w & XINPUT_GAMEPAD_LEFT_SHOULDER) return GP_LB;
            if (w & XINPUT_GAMEPAD_RIGHT_SHOULDER) return GP_RB;
            if (w & XINPUT_GAMEPAD_A) return GP_A;
            if (w & XINPUT_GAMEPAD_B) return GP_B;
            if (w & XINPUT_GAMEPAD_X) return GP_X;
            if (w & XINPUT_GAMEPAD_Y) return GP_Y;
            if (st.Gamepad.bLeftTrigger > 100) return 14; // LT
            if (st.Gamepad.bRightTrigger > 100) return 15; // RT
            return GP_NONE;
        }
        return GP_NONE;
    }

public:
    KeybindManager() : editingIndex(-1), waitingForKey(false), waitingForPad(false), lastDevice(0) {}

    void Add(const Keybind& bind) { binds.push_back(bind); }

    // Which input device is the player currently using (auto-detect)
    int ActiveDevice() {
        bool kbd = false;
        for (int vk = 0; vk < 256; vk++) {
            SHORT s = GetAsyncKeyState(vk);
            if (s & 0x8000) { kbd = true; break; }
        }
        // Gamepad sticks/triggers count as activity
        XINPUT_STATE st;
        bool pad = false;
        for (DWORD i = 0; i < 4; i++) {
            if (XInputGetState(i, &st) != ERROR_SUCCESS) continue;
            XINPUT_GAMEPAD& gp = st.Gamepad;
            if (gp.wButtons || gp.bLeftTrigger > 20 || gp.bRightTrigger > 20 ||
                abs((int)gp.sThumbLX) > 5000 || abs((int)gp.sThumbLY) > 5000 ||
                abs((int)gp.sThumbRX) > 5000 || abs((int)gp.sThumbRY) > 5000) { pad = true; break; }
        }
        if (kbd) lastDevice = 1;
        else if (pad) lastDevice = 2;
        return lastDevice;
    }

    bool IsControllerMode() { return ActiveDevice() == 2; }

    static bool AnyPadInput() {
        for (int vk = 0; vk < 256; vk++) {
            if (GetAsyncKeyState(vk) & 0x8000) return false; // kbd active
        }
        return PadConnected();
    }

    void Poll() {
        int curPad = PollPad();
        bool padConnected = PadConnected();
        for (auto& b : binds) {
            bool kbdPressed = b.key > 0 && (GetAsyncKeyState(b.key) & 0x8000);
            bool padPressed = padConnected && b.padKey != GP_NONE && curPad == b.padKey;
            bool cur = kbdPressed || padPressed;

            if (cur && !b.prevState) {
                b.justPressed = true;
                if (b.toggle) *b.toggle = !*b.toggle;
                if (b.onPress) b.onPress();
            } else {
                b.justPressed = false;
            }
            b.prevState = cur;
        }
    }

    static const char* KeyName(int vk) {
        switch (vk) {
        case VK_LBUTTON: return "MOUSE L";
        case VK_RBUTTON: return "MOUSE R";
        case VK_MBUTTON: return "MOUSE M";
        case VK_XBUTTON1: return "MOUSE 4";
        case VK_XBUTTON2: return "MOUSE 5";
        case VK_INSERT: return "INSERT";
        case VK_DELETE: return "DELETE";
        case VK_HOME: return "HOME";
        case VK_END: return "END";
        case VK_PRIOR: return "PAGE UP";
        case VK_NEXT: return "PAGE DOWN";
        case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3";
        case VK_F4: return "F4"; case VK_F5: return "F5"; case VK_F6: return "F6";
        case VK_F7: return "F7"; case VK_F8: return "F8"; case VK_F9: return "F9";
        case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
        case VK_LSHIFT: return "LEFT SHIFT"; case VK_RSHIFT: return "RIGHT SHIFT";
        case VK_LCONTROL: return "LEFT CTRL"; case VK_RCONTROL: return "RIGHT CTRL";
        case VK_LMENU: return "LEFT ALT"; case VK_RMENU: return "RIGHT ALT";
        case VK_TAB: return "TAB"; case VK_SPACE: return "SPACE";
        case VK_RETURN: return "ENTER"; case VK_ESCAPE: return "ESC";
        case VK_OEM_COMMA: return ","; case VK_OEM_PERIOD: return ".";
        case VK_OEM_MINUS: return "-"; case VK_OEM_PLUS: return "=";
        case VK_OEM_1: return ";"; case VK_OEM_7: return "'";
        case VK_LEFT: return "ARROW L"; case VK_RIGHT: return "ARROW R";
        case VK_UP: return "ARROW U"; case VK_DOWN: return "ARROW D";
        case 0: return "NONE";
        default: {
            if (vk >= 'A' && vk <= 'Z') { static char b[2] = {}; b[0] = (char)vk; return b; }
            if (vk >= '0' && vk <= '9') { static char b[2] = {}; b[0] = (char)vk; return b; }
            static char b[16];
            sprintf_s(b, "VK_%d", vk);
            return b;
        }
        }
    }

    static const char* PadName(int pb) {
        switch (pb) {
        case GP_DPAD_UP: return "DPAD UP";
        case GP_DPAD_DOWN: return "DPAD DOWN";
        case GP_DPAD_LEFT: return "DPAD LEFT";
        case GP_DPAD_RIGHT: return "DPAD RIGHT";
        case GP_START: return "START";
        case GP_BACK: return "BACK";
        case GP_LS: return "L STICK";
        case GP_RS: return "R STICK";
        case GP_LB: return "LB";
        case GP_RB: return "RB";
        case GP_A: return "A";
        case GP_B: return "B";
        case GP_X: return "X";
        case GP_Y: return "Y";
        case 14: return "LT";
        case 15: return "RT";
        default: return "NONE";
        }
    }

    void RenderEditor() {
        if (!ImGui::Begin("Keybinds", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }

        int dev = ActiveDevice();
        if (dev == 2) ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1), "Controller active - binds use gamepad");
        else if (dev == 1) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1), "Keyboard / Mouse active - binds use keys");
        else ImGui::TextDisabled("No input detected");
        ImGui::Separator();

        for (size_t i = 0; i < binds.size(); i++) {
            auto& b = binds[i];
            ImGui::Text("%s", b.label.c_str());
            ImGui::SameLine(200);

            // Primary (keyboard) bind button
            std::string btnLabel = (b.key > 0) ? KeyName(b.key) : "NONE";
            if (editingIndex == (int)i && waitingForKey) btnLabel = "...";
            if (ImGui::Button(("##kb" + std::to_string(i)).c_str(), ImVec2(120, 0))) {
                editingIndex = (int)i; waitingForKey = true; waitingForPad = false;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(btnLabel.c_str());

            ImGui::SameLine(360);
            // Controller bind button
            std::string pbLabel = (b.padKey != GP_NONE) ? PadName(b.padKey) : "NONE";
            if (editingIndex == (int)i && waitingForPad) pbLabel = "...";
            if (ImGui::Button(("##pd" + std::to_string(i)).c_str(), ImVec2(100, 0))) {
                editingIndex = (int)i; waitingForPad = true; waitingForKey = false;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(pbLabel.c_str());

            ImGui::SameLine(500);
            if (ImGui::Button(("X##cl" + std::to_string(i)).c_str(), ImVec2(24, 0))) { b.key = 0; b.padKey = GP_NONE; }
        }

        // Capture input for the currently editing bind
        if (editingIndex >= 0 && editingIndex < (int)binds.size()) {
            auto& b = binds[editingIndex];
            if (waitingForKey) {
                for (int vk = 0; vk < 256; vk++) {
                    if (GetAsyncKeyState(vk) & 1) {
                        if (vk == VK_ESCAPE) { editingIndex = -1; waitingForKey = false; }
                        else { b.key = vk; editingIndex = -1; waitingForKey = false; }
                        break;
                    }
                }
            } else if (waitingForPad) {
                int pb = PollPad();
                if (pb != GP_NONE) { b.padKey = pb; editingIndex = -1; waitingForPad = false; }
                else {
                    // fallback: allow clearing via back/start double press or esc keyboard
                    if (GetAsyncKeyState(VK_ESCAPE) & 1) { editingIndex = -1; waitingForPad = false; }
                }
            }
        }

        if (ImGui::Button("Reset Binds")) ResetToDefaults();
        ImGui::End();
    }

    int GetKey(const std::string& name, int def = VK_INSERT) const {
        for (auto& b : binds) if (b.name == name) return b.key;
        return def;
    }

    // Hold-style check: true while the bind's keyboard key OR controller
    // button is currently down (auto-detects whichever device is active).
    bool IsHeld(const std::string& name) const {
        for (auto& b : binds) {
            if (b.name != name) continue;
            if (b.key > 0 && (GetAsyncKeyState(b.key) & 0x8000)) return true;
            if (b.padKey != GP_NONE && PadConnected() && PollPad() == b.padKey) return true;
        }
        return false;
    }

    int GetPadKey(const std::string& name) const {
        for (auto& b : binds) if (b.name == name) return b.padKey;
        return GP_NONE;
    }

    void SetKey(const std::string& name, int key) {
        for (auto& b : binds) if (b.name == name) { b.key = key; return; }
    }

    void SetPadKey(const std::string& name, int pb) {
        for (auto& b : binds) if (b.name == name) { b.padKey = pb; return; }
    }

    std::vector<Keybind>& GetAll() { return binds; }

    void ResetToDefaults() {
        for (auto& b : binds) { b.key = b.defaultKey; b.padKey = b.defaultPadKey; }
    }
};
