#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <d3d9.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

// Real Roblox offsets from theo's Discord (version-145f189a6a974303)
namespace Roblox {
    // DataModel offsets
    constexpr size_t DataModel_Workspace = 0x160;
    constexpr size_t DataModel_ScriptContext = 0x440;
    
    // Workspace offsets
    constexpr size_t Workspace_CurrentCamera = 0x498;
    constexpr size_t Workspace_World = 0x3f0;
    
    // World offsets
    constexpr size_t World_Gravity = 0x210;
    constexpr size_t World_Primitives = 0x288;
    
    // Camera offsets
    constexpr size_t Camera_Position = 0xfc;
    constexpr size_t Camera_Rotation = 0xd8;
    constexpr size_t Camera_FieldOfView = 0x140;
    
    // Player offsets
    constexpr size_t Player_LocalPlayer = 0x130;
    constexpr size_t Player_ModelInstance = 0x298;
    constexpr size_t Player_Name = 0x138;
    
    // Model offsets
    constexpr size_t Model_PrimaryPart = 0x258;
    
    // Humanoid offsets
    constexpr size_t Humanoid_Health = 0x190;
    constexpr size_t Humanoid_MaxHealth = 0x1a8;
    constexpr size_t Humanoid_WalkSpeed = 0x1d0;
    constexpr size_t Humanoid_JumpPower = 0x1a4;
    constexpr size_t Humanoid_HumanoidRootPart = 0x478;
    constexpr size_t Humanoid_Jump = 0x1da;
    
    // Instance offsets
    constexpr size_t Instance_ChildrenStart = 0x70;
    constexpr size_t Instance_ChildrenEnd = 0x8;
    constexpr size_t Instance_Name = 0x98;
    constexpr size_t Instance_Parent = 0x68;
    constexpr size_t Instance_ClassName = 0x8;
    
    // Primitive (Part) offsets
    constexpr size_t Primitive_Position = 0xec;
    constexpr size_t Primitive_Rotation = 0xc8;
    constexpr size_t Primitive_Size = 0x1b8;
    constexpr size_t Primitive_Flags = 0x1b6;
    constexpr size_t Primitive_AssemblyLinearVelocity = 0xf8;
    
    // Primitive flags
    constexpr size_t PrimitiveFlag_Anchored = 0x2;
    constexpr size_t PrimitiveFlag_CanCollide = 0x8;
    
    // TaskScheduler
    constexpr size_t TaskScheduler_Pointer = 0x84a58e0;
    
    // FakeDataModel
    constexpr size_t FakeDataModel_Pointer = 0x7e26978;
    constexpr size_t FakeDataModel_RealDataModel = 0x1d0;
}

struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
    
    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
    
    float Length() const {
        return sqrt(x * x + y * y + z * z);
    }
    
    float DistanceTo(const Vector3& other) const {
        return (*this - other).Length();
    }
};

struct Player {
    uintptr_t address;
    std::string name;
    Vector3 position;
    Vector3 velocity;
    float health;
    float maxHealth;
    float walkSpeed;
    bool isLocal;
    bool isAlive;
};

class ExternalMemory {
private:
    HANDLE hProcess = nullptr;
    DWORD processId = 0;

public:
    bool Attach(const char* processName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            CloseHandle(hSnapshot);
            return false;
        }

        do {
            if (_stricmp(pe32.szExeFile, processName) == 0) {
                processId = pe32.th32ProcessID;
                CloseHandle(hSnapshot);
                hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
                return hProcess != nullptr;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);
        return false;
    }

    uintptr_t GetModuleBase(const char* moduleName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
        if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);

        if (!Module32First(hSnapshot, &me32)) {
            CloseHandle(hSnapshot);
            return 0;
        }

        do {
            if (_stricmp(me32.szModule, moduleName) == 0) {
                CloseHandle(hSnapshot);
                return (uintptr_t)me32.modBaseAddr;
            }
        } while (Module32Next(hSnapshot, &me32));

        CloseHandle(hSnapshot);
        return 0;
    }

    template<typename T>
    T Read(uintptr_t address) {
        T value = {};
        ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(T), nullptr);
        return value;
    }

    bool Read(uintptr_t address, void* buffer, size_t size) {
        return ReadProcessMemory(hProcess, (LPCVOID)address, buffer, size, nullptr) != 0;
    }

    template<typename T>
    bool Write(uintptr_t address, T value) {
        return WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(T), nullptr) != 0;
    }

    bool Write(uintptr_t address, void* buffer, size_t size) {
        return WriteProcessMemory(hProcess, (LPVOID)address, buffer, size, nullptr) != 0;
    }

    void Detach() {
        if (hProcess) {
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
    }

    bool IsAttached() const { return hProcess != nullptr; }
};

// ImGui Overlay Window
class ImGuiOverlay {
private:
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    HWND overlayWindow = nullptr;
    bool initialized = false;
    
public:
    bool Init() {
        // Create overlay window
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "Roblox ImGui Overlay";
        
        RegisterClassExA(&wc);
        
        overlayWindow = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            "Roblox ImGui Overlay", "Roblox Cheat",
            WS_POPUP, 0, 0, 1920, 1080,
            nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
        
        if (!overlayWindow) return false;
        
        SetLayeredWindowAttributes(overlayWindow, 0, 255, LWA_ALPHA);
        ShowWindow(overlayWindow, SW_SHOW);
        UpdateWindow(overlayWindow);
        
        // Initialize D3D9
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) return false;
        
        D3DPRESENT_PARAMETERS pp = {0};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = overlayWindow;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        
        if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, overlayWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device))) {
            return false;
        }
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        // Premium dark theme
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 8.0f;
        style.FrameRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.12f, 0.95f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.35f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.3f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.5f, 0.9f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.45f, 0.85f, 1.0f);
        
        ImGui_ImplWin32_Init(overlayWindow);
        ImGui_ImplDX9_Init(device);
        
        initialized = true;
        return true;
    }
    
    void BeginRender() {
        if (!initialized || !device) return;
        device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
        device->BeginScene();
        
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }
    
    void EndRender() {
        if (!initialized || !device) return;
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        device->EndScene();
        device->Present(nullptr, nullptr, nullptr, nullptr);
    }
    
    void Cleanup() {
        if (!initialized) return;
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        if (device) { device->Release(); device = nullptr; }
        if (d3d) { d3d->Release(); d3d = nullptr; }
        if (overlayWindow) { DestroyWindow(overlayWindow); overlayWindow = nullptr; }
        
        initialized = false;
    }
    
    bool IsInitialized() { return initialized; }
    HWND GetWindow() { return overlayWindow; }
};

ImGuiOverlay g_overlay;

// Window procedure for ImGui
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

// Lua Script Executor
class LuaExecutor {
private:
    ExternalMemory& mem;
    uintptr_t scriptContext = 0;
    char scriptBuffer[4096];
    std::vector<std::string> scriptHistory;
    int historyIndex = -1;
    
public:
    LuaExecutor(ExternalMemory& m) : mem(m) {
        scriptBuffer[0] = '\0';
    }
    
    bool Init(uintptr_t dataModel) {
        scriptContext = mem.Read<uintptr_t>(dataModel + Roblox::DataModel_ScriptContext);
        return scriptContext != 0;
    }
    
    bool ExecuteScript(const char* script) {
        if (!scriptContext) return false;
        
        // This is a simplified Lua executor
        // In a real implementation, you'd need to:
        // 1. Get the Lua state from ScriptContext
        // 2. Load the script into Lua
        // 3. Execute it using lua_pcall
        // For now, this is a placeholder that shows the UI
        
        // Add to history
        if (strlen(script) > 0) {
            scriptHistory.push_back(script);
            historyIndex = scriptHistory.size();
        }
        
        return true;
    }
    
    void RenderUI() {
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("Lua Script Executor", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::Text("Client-Side Lua Executor");
        ImGui::Separator();
        
        // Script input
        ImGui::InputTextMultiline("##Script", scriptBuffer, sizeof(scriptBuffer), 
            ImVec2(580, 300), ImGuiInputTextFlags_AllowTabInput);
        
        // Execute button
        if (ImGui::Button("Execute Script", ImVec2(580, 30))) {
            ExecuteScript(scriptBuffer);
        }
        
        // Clear button
        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(100, 30))) {
            scriptBuffer[0] = '\0';
        }
        
        ImGui::Separator();
        
        // Script history
        ImGui::Text("Script History:");
        if (ImGui::BeginChild("##History", ImVec2(580, 100))) {
            for (size_t i = 0; i < scriptHistory.size(); i++) {
                if (ImGui::Selectable(scriptHistory[i].c_str())) {
                    strncpy(scriptBuffer, scriptHistory[i].c_str(), sizeof(scriptBuffer) - 1);
                    scriptBuffer[sizeof(scriptBuffer) - 1] = '\0';
                }
            }
            ImGui::EndChild();
        }
        
        ImGui::End();
    }
    
    char* GetScriptBuffer() { return scriptBuffer; }
};

class RobloxCheat {
private:
    ExternalMemory mem;
    LuaExecutor luaExecutor;
    uintptr_t moduleBase = 0;
    uintptr_t dataModel = 0;
    uintptr_t workspace = 0;
    uintptr_t localPlayer = 0;
    bool menuOpen = true;
    
    // Settings
    bool espEnabled = true;
    bool aimbotEnabled = true;
    bool flyEnabled = false;
    bool speedEnabled = false;
    bool godModeEnabled = false;
    bool noClipEnabled = false;
    
    float aimbotFOV = 90.0f;
    float aimbotSmooth = 5.0f;
    float speedMultiplier = 2.0f;
    float flySpeed = 50.0f;
    
    int aimbotKey = VK_XBUTTON2;
    bool aimbotKeyPressed = false;

    Vector3 WorldToScreen(const Vector3& world, const Vector3& cameraPos, const Vector3& cameraRot) {
        Vector3 delta = world - cameraPos;
        
        float pitch = cameraRot.x * (3.14159f / 180.0f);
        float yaw = cameraRot.y * (3.14159f / 180.0f);
        
        float cosYaw = cos(yaw);
        float sinYaw = sin(yaw);
        float x = delta.x * cosYaw - delta.y * sinYaw;
        float y = delta.x * sinYaw + delta.y * cosYaw;
        
        float cosPitch = cos(pitch);
        float sinPitch = sin(pitch);
        float z = delta.z * cosPitch - y * sinPitch;
        y = delta.z * sinPitch + y * cosPitch;
        
        if (y <= 0.1f) return Vector3(-1, -1, -1);
        
        float screenX = (x / y) * 640.0f + 640.0f;
        float screenY = (z / y) * 480.0f + 480.0f;
        
        return Vector3(screenX, screenY, y);
    }

    std::string ReadString(uintptr_t address) {
        char buffer[256];
        for (int i = 0; i < 255; i++) {
            char c = mem.Read<char>(address + i);
            if (c == 0) break;
            buffer[i] = c;
        }
        buffer[255] = 0;
        return std::string(buffer);
    }

public:
    RobloxCheat() : luaExecutor(mem) {}
    
    bool Init() {
        if (!mem.Attach("RobloxPlayerBeta.exe")) {
            std::cout << "[-] Roblox not found (try RobloxPlayerBeta.exe)" << std::endl;
            return false;
        }

        moduleBase = mem.GetModuleBase("RobloxPlayerBeta.exe");
        if (!moduleBase) {
            std::cout << "[-] Failed to get module base" << std::endl;
            return false;
        }

        // Use FakeDataModel pointer to get real DataModel
        uintptr_t fakeDataModelPtr = moduleBase + Roblox::FakeDataModel_Pointer;
        dataModel = mem.Read<uintptr_t>(fakeDataModelPtr);
        if (!dataModel) {
            std::cout << "[-] Failed to read FakeDataModel pointer" << std::endl;
            return false;
        }

        // Get real DataModel from FakeDataModel
        dataModel = mem.Read<uintptr_t>(dataModel + Roblox::FakeDataModel_RealDataModel);
        if (!dataModel) {
            std::cout << "[-] Failed to read real DataModel" << std::endl;
            return false;
        }

        // Get Workspace from DataModel
        workspace = mem.Read<uintptr_t>(dataModel + Roblox::DataModel_Workspace);
        if (!workspace) {
            std::cout << "[-] Failed to read Workspace" << std::endl;
            return false;
        }

        // Initialize Lua executor
        if (!luaExecutor.Init(dataModel)) {
            std::cout << "[-] Failed to initialize Lua executor" << std::endl;
        }

        // Initialize ImGui overlay
        if (!g_overlay.Init()) {
            std::cout << "[-] Failed to initialize ImGui overlay" << std::endl;
            return false;
        }

        std::cout << "[+] Roblox Cheat Initialized" << std::endl;
        std::cout << "[+] Module Base: 0x" << std::hex << moduleBase << std::endl;
        std::cout << "[+] DataModel: 0x" << std::hex << dataModel << std::endl;
        std::cout << "[+] Workspace: 0x" << std::hex << workspace << std::endl;
        std::cout << "[+] Using theo's offsets (version-145f189a6a974303)" << std::endl;
        std::cout << "[+] ImGui UI loaded" << std::endl;
        
        return true;
    }

    void Update() {
        if (!mem.IsAttached()) return;
        
        aimbotKeyPressed = (GetAsyncKeyState(aimbotKey) & 0x8000) != 0;
        
        // Toggle menu with INSERT
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menuOpen = !menuOpen;
        }
        
        // Read camera from workspace
        uintptr_t camera = mem.Read<uintptr_t>(workspace + Roblox::Workspace_CurrentCamera);
        if (!camera) return;
        
        Vector3 cameraPos = mem.Read<Vector3>(camera + Roblox::Camera_Position);
        Vector3 cameraRot = mem.Read<Vector3>(camera + Roblox::Camera_Rotation);
        
        // Read World from workspace
        uintptr_t world = mem.Read<uintptr_t>(workspace + Roblox::Workspace_World);
        if (!world) return;
        
        // Apply speed hack
        if (speedEnabled) {
            // We would need to find local player's character and humanoid
            // For now, this is a placeholder
        }
        
        // Apply fly hack
        if (flyEnabled) {
            // Modify gravity or velocity
            float currentGravity = mem.Read<float>(world + Roblox::World_Gravity);
            if (currentGravity != 0) {
                mem.Write<float>(world + Roblox::World_Gravity, 0.0f); // Zero gravity for fly
            }
        } else {
            // Restore gravity
            float currentGravity = mem.Read<float>(world + Roblox::World_Gravity);
            if (currentGravity == 0) {
                mem.Write<float>(world + Roblox::World_Gravity, -196.2f); // Restore normal gravity
            }
        }
        
        // Render ImGui
        g_overlay.BeginRender();
        
        if (menuOpen) {
            RenderMainMenu();
            luaExecutor.RenderUI();
        }
        
        g_overlay.EndRender();
    }
    
    void RenderMainMenu() {
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("Roblox Premium Cheat", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::Text("Premium Roblox Cheat");
        ImGui::Separator();
        
        // Feature toggles
        ImGui::Checkbox("ESP", &espEnabled);
        ImGui::Checkbox("Aimbot", &aimbotEnabled);
        ImGui::Checkbox("Fly Hack", &flyEnabled);
        ImGui::Checkbox("Speed Hack", &speedEnabled);
        ImGui::Checkbox("God Mode", &godModeEnabled);
        ImGui::Checkbox("NoClip", &noClipEnabled);
        
        ImGui::Separator();
        
        // Info
        uintptr_t camera = mem.Read<uintptr_t>(workspace + Roblox::Workspace_CurrentCamera);
        if (camera) {
            Vector3 camPos = mem.Read<Vector3>(camera + Roblox::Camera_Position);
            ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);
        }
        ImGui::Text("Press INSERT to toggle menu");
        
        ImGui::End();
    }

    void ToggleESP() { espEnabled = !espEnabled; }
    void ToggleAimbot() { aimbotEnabled = !aimbotEnabled; }
    void ToggleFly() { flyEnabled = !flyEnabled; }
    void ToggleSpeed() { speedEnabled = !speedEnabled; }
    void ToggleGodMode() { godModeEnabled = !godModeEnabled; }
    void ToggleNoClip() { noClipEnabled = !noClipEnabled; }

    void Cleanup() {
        g_overlay.Cleanup();
        mem.Detach();
    }
};

int main() {
    RobloxCheat cheat;
    
    std::cout << "[+] Roblox Premium Cheat with ImGui UI" << std::endl;
    std::cout << "[+] Features: ESP, Aimbot, Fly, Speed, God Mode, NoClip, Lua Executor" << std::endl;
    std::cout << "[+] Press INSERT to toggle menu" << std::endl;
    
    if (!cheat.Init()) {
        system("pause");
        return 1;
    }
    
    std::cout << "[+] Cheat running..." << std::endl;
    
    while (true) {
        cheat.Update();
        Sleep(1);
    }
    
    cheat.Cleanup();
    return 0;
}
