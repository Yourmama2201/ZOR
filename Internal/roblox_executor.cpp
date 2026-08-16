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
        return ReadProcessMemory(hProcess, (LPVOID)address, buffer, size, nullptr) != 0;
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
        WNDCLASSEXA wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "Roblox Executor Overlay";
        
        RegisterClassExA(&wc);
        
        overlayWindow = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            "Roblox Executor Overlay", "Roblox Executor",
            WS_POPUP, 0, 0, 1920, 1080,
            nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
        
        if (!overlayWindow) return false;
        
        SetLayeredWindowAttributes(overlayWindow, 0, 255, LWA_ALPHA);
        ShowWindow(overlayWindow, SW_SHOW);
        UpdateWindow(overlayWindow);
        
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
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
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
};

ImGuiOverlay g_overlay;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

class RobloxExecutor {
private:
    ExternalMemory mem;
    uintptr_t moduleBase = 0;
    uintptr_t dataModel = 0;
    uintptr_t workspace = 0;
    uintptr_t world = 0;
    
    char scriptBuffer[4096];
    std::vector<std::string> scriptHistory;
    bool menuOpen = true;
    
    // Trolling features
    bool flyEnabled = false;
    bool superSpeedEnabled = false;
    bool godModeEnabled = false;
    bool noClipEnabled = false;
    bool teleportEnabled = false;
    Vector3 teleportPos;
    
public:
    bool Init() {
        if (!mem.Attach("RobloxPlayerBeta.exe")) {
            std::cout << "[-] Roblox not found" << std::endl;
            return false;
        }

        moduleBase = mem.GetModuleBase("RobloxPlayerBeta.exe");
        if (!moduleBase) {
            std::cout << "[-] Failed to get module base" << std::endl;
            return false;
        }

        uintptr_t fakeDataModelPtr = moduleBase + Roblox::FakeDataModel_Pointer;
        dataModel = mem.Read<uintptr_t>(fakeDataModelPtr);
        if (!dataModel) {
            std::cout << "[-] Failed to read FakeDataModel pointer" << std::endl;
            return false;
        }

        dataModel = mem.Read<uintptr_t>(dataModel + Roblox::FakeDataModel_RealDataModel);
        if (!dataModel) {
            std::cout << "[-] Failed to read real DataModel" << std::endl;
            return false;
        }

        workspace = mem.Read<uintptr_t>(dataModel + Roblox::DataModel_Workspace);
        if (!workspace) {
            std::cout << "[-] Failed to read Workspace" << std::endl;
            return false;
        }

        world = mem.Read<uintptr_t>(workspace + Roblox::Workspace_World);
        
        if (!g_overlay.Init()) {
            std::cout << "[-] Failed to initialize ImGui overlay" << std::endl;
            return false;
        }

        std::cout << "[+] Roblox Executor Initialized" << std::endl;
        std::cout << "[+] Module Base: 0x" << std::hex << moduleBase << std::endl;
        std::cout << "[+] DataModel: 0x" << std::hex << dataModel << std::endl;
        std::cout << "[+] Workspace: 0x" << std::hex << workspace << std::endl;
        
        return true;
    }
    
    void ExecuteTrollScript(const char* scriptType) {
        if (!world) return;
        
        if (strcmp(scriptType, "fly") == 0) {
            mem.Write<float>(world + Roblox::World_Gravity, 0.0f);
        } else if (strcmp(scriptType, "normal_gravity") == 0) {
            mem.Write<float>(world + Roblox::World_Gravity, -196.2f);
        } else if (strcmp(scriptType, "super_gravity") == 0) {
            mem.Write<float>(world + Roblox::World_Gravity, -500.0f);
        } else if (strcmp(scriptType, "anti_gravity") == 0) {
            mem.Write<float>(world + Roblox::World_Gravity, 196.2f);
        }
    }
    
    void Update() {
        if (!mem.IsAttached()) return;
        
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menuOpen = !menuOpen;
        }
        
        // Apply trolling features
        if (flyEnabled) {
            mem.Write<float>(world + Roblox::World_Gravity, 0.0f);
        }
        
        // Render ImGui
        g_overlay.BeginRender();
        
        if (menuOpen) {
            RenderUI();
        }
        
        g_overlay.EndRender();
    }
    
    void RenderUI() {
        ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("Roblox Troll Executor", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::Text("Premium Troll Executor");
        ImGui::Separator();
        
        // Quick troll buttons
        ImGui::Text("Quick Trolls:");
        if (ImGui::Button("Enable Fly (Zero Gravity)", ImVec2(200, 30))) {
            flyEnabled = true;
            ExecuteTrollScript("fly");
        }
        ImGui::SameLine();
        if (ImGui::Button("Disable Fly", ImVec2(200, 30))) {
            flyEnabled = false;
            ExecuteTrollScript("normal_gravity");
        }
        
        if (ImGui::Button("Super Gravity", ImVec2(200, 30))) {
            ExecuteTrollScript("super_gravity");
        }
        ImGui::SameLine();
        if (ImGui::Button("Anti Gravity", ImVec2(200, 30))) {
            ExecuteTrollScript("anti_gravity");
        }
        
        ImGui::Separator();
        
        // Script executor
        ImGui::Text("Custom Script:");
        ImGui::InputTextMultiline("##Script", scriptBuffer, sizeof(scriptBuffer), 
            ImVec2(480, 200), ImGuiInputTextFlags_AllowTabInput);
        
        if (ImGui::Button("Execute", ImVec2(480, 30))) {
            // Add to history
            if (strlen(scriptBuffer) > 0) {
                scriptHistory.push_back(scriptBuffer);
            }
            // Execute would need proper Lua integration
            std::cout << "[+] Script executed (UI only - needs Lua integration)" << std::endl;
        }
        
        ImGui::Separator();
        
        // Script history
        ImGui::Text("Script History:");
        if (ImGui::BeginChild("##History", ImVec2(480, 100))) {
            for (size_t i = 0; i < scriptHistory.size(); i++) {
                if (ImGui::Selectable(scriptHistory[i].c_str())) {
                    strncpy(scriptBuffer, scriptHistory[i].c_str(), sizeof(scriptBuffer) - 1);
                    scriptBuffer[sizeof(scriptBuffer) - 1] = '\0';
                }
            }
            ImGui::EndChild();
        }
        
        ImGui::Separator();
        ImGui::Text("Press INSERT to toggle menu");
        
        ImGui::End();
    }
    
    void Cleanup() {
        g_overlay.Cleanup();
        mem.Detach();
    }
};

int main() {
    RobloxExecutor executor;
    
    std::cout << "[+] Roblox Troll Executor with ImGui UI" << std::endl;
    std::cout << "[+] Features: Fly, Gravity manipulation, Custom scripts" << std::endl;
    std::cout << "[+] Press INSERT to toggle menu" << std::endl;
    
    if (!executor.Init()) {
        system("pause");
        return 1;
    }
    
    std::cout << "[+] Executor running..." << std::endl;
    
    while (true) {
        executor.Update();
        Sleep(1);
    }
    
    executor.Cleanup();
    return 0;
}
