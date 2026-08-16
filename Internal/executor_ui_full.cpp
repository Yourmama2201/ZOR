#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <d3d9.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

// Shared memory for IPC (encrypted)
#define SHMEM_SIZE 16384
#define SHMEM_NAME "RobloxExecutor_IPC_v2"

struct IPCMessage {
    char script[4096];
    bool execute;
    bool clear;
    bool scanRemoteEvents;
    bool scanBackdoors;
    char remoteEventName[256];
    char remoteEventArgs[2048];
    bool fireRemoteEvent;
    int remoteEventCount;
    char remoteEvents[8192];
    int backdoorCount;
    char backdoors[8192];
    bool autoExec;
    char autoExecScript[4096];
    bool loadScript;
    char scriptPath[512];
};

// Script Library
class ScriptLibrary {
private:
    std::string libraryPath;
    struct Script {
        std::string name;
        std::string code;
        std::string category;
        bool autoExec;
    };
    std::vector<Script> scripts;
    
public:
    bool Init() {
        char modulePath[MAX_PATH];
        GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        char* lastSlash = strrchr(modulePath, '\\');
        if (lastSlash) {
            *lastSlash = '\0';
            libraryPath = std::string(modulePath) + "\\scripts";
            CreateDirectoryA(libraryPath.c_str(), nullptr);
            
            // Create categories
            CreateDirectoryA((libraryPath + "\\trolling").c_str(), nullptr);
            CreateDirectoryA((libraryPath + "\\utility").c_str(), nullptr);
            CreateDirectoryA((libraryPath + "\\admin").c_str(), nullptr);
            CreateDirectoryA((libraryPath + "\\misc").c_str(), nullptr);
        }
        LoadScripts();
        return true;
    }
    
    void LoadScripts() {
        scripts.clear();
        
        if (!fs::exists(libraryPath)) return;
        
        for (const auto& entry : fs::recursive_directory_iterator(libraryPath)) {
            if (entry.path().extension() == ".lua") {
                Script script;
                script.name = entry.path().filename().string();
                script.name = script.name.substr(0, script.name.size() - 4);
                
                std::ifstream file(entry.path());
                std::string content((std::istreambuf_iterator<char>(file)), 
                                   std::istreambuf_iterator<char>());
                script.code = content;
                
                // Determine category from path
                std::string pathStr = entry.path().string();
                if (pathStr.find("\\trolling\\") != std::string::npos) {
                    script.category = "trolling";
                } else if (pathStr.find("\\utility\\") != std::string::npos) {
                    script.category = "utility";
                } else if (pathStr.find("\\admin\\") != std::string::npos) {
                    script.category = "admin";
                } else {
                    script.category = "misc";
                }
                
                script.autoExec = false;
                scripts.push_back(script);
            }
        }
    }
    
    bool SaveScript(const char* name, const char* code, const char* category) {
        std::string path = libraryPath + "\\" + category + "\\" + name + ".lua";
        std::ofstream file(path);
        if (!file) return false;
        
        file << code;
        file.close();
        
        LoadScripts();
        return true;
    }
    
    bool DeleteScript(const char* name) {
        for (auto& script : scripts) {
            if (script.name == name) {
                std::string path = libraryPath + "\\" + script.category + "\\" + name + ".lua";
                fs::remove(path);
                LoadScripts();
                return true;
            }
        }
        return false;
    }
    
    const std::vector<Script>& GetScripts() const { return scripts; }
    const std::string& GetLibraryPath() const { return libraryPath; }
};

// ImGui Overlay
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
        wc.lpszClassName = "Executor UI Overlay";
        
        RegisterClassExA(&wc);
        
        // Create smaller window positioned in center - not full screen
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowWidth = 950;
        int windowHeight = 750;
        int posX = (screenWidth - windowWidth) / 2;
        int posY = (screenHeight - windowHeight) / 2;
        
        overlayWindow = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            "Executor UI Overlay", "Executor",
            WS_POPUP, posX, posY, windowWidth, windowHeight,
            nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
        
        if (!overlayWindow) return false;
        
        // Use alpha blending for transparency
        SetLayeredWindowAttributes(overlayWindow, 0, 240, LWA_ALPHA);
        
        ShowWindow(overlayWindow, SW_SHOW);
        UpdateWindow(overlayWindow);
        
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) return false;
        
        D3DPRESENT_PARAMETERS pp = {0};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = overlayWindow;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferWidth = windowWidth;
        pp.BackBufferHeight = windowHeight;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        
        if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, overlayWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device))) {
            return false;
        }
        
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        
        // Premium dark theme with gradients
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 12.0f;
        style.FrameRounding = 8.0f;
        style.GrabRounding = 6.0f;
        style.ScrollbarRounding = 6.0f;
        style.WindowPadding = ImVec2(16, 16);
        style.ItemSpacing = ImVec2(12, 8);
        style.FramePadding = ImVec2(8, 6);
        style.IndentSpacing = 24.0f;
        style.ScrollbarSize = 14.0f;
        
        // Premium color scheme
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.95f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.04f, 0.04f, 0.06f, 0.95f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.98f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.16f, 0.95f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.24f, 0.95f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.28f, 0.95f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.04f, 0.06f, 0.95f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.24f, 0.95f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.28f, 0.36f, 0.95f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.32f, 0.40f, 0.95f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.65f, 0.95f, 0.95f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.75f, 1.0f, 0.95f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.38f, 0.72f, 0.95f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.48f, 0.82f, 0.95f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.58f, 0.92f, 0.95f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.18f, 0.95f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.18f, 0.24f, 0.95f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.22f, 0.28f, 0.95f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.24f, 0.95f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.28f, 0.28f, 0.36f, 0.95f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.32f, 0.32f, 0.40f, 0.95f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.18f, 0.95f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.18f, 0.24f, 0.95f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.28f, 0.95f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.18f, 0.95f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.24f, 0.95f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.45f, 0.75f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.45f, 0.75f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.65f, 0.95f, 0.35f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.35f, 0.65f, 0.95f, 0.95f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.35f, 0.65f, 0.95f, 0.80f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.35f, 0.65f, 0.95f, 0.80f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.06f, 0.06f, 0.08f, 0.20f);
        
        ImGui_ImplWin32_Init(overlayWindow);
        ImGui_ImplDX9_Init(device);
        
        initialized = true;
        return true;
    }
    
    void BeginRender() {
        if (!initialized || !device) return;
        
        // Clear with full transparency
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

class ExecutorUI {
private:
    HANDLE hSharedMemory = nullptr;
    IPCMessage* sharedData = nullptr;
    ScriptLibrary scriptLibrary;
    
    char scriptBuffer[4096];
    std::vector<std::string> scriptHistory;
    bool menuOpen = true;
    bool dllInjected = false;
    
    int currentTab = 0; // 0: Script Editor, 1: Script Library, 2: RemoteEvents, 3: Settings
    
    // Settings
    bool autoInject = true;
    bool minimizeToTray = false;
    bool topMost = true;
    int fontSize = 14;
    bool showWatermark = true;
    
    // Script editor
    char newScriptName[256] = "";
    char newScriptCategory[64] = "misc";
    
public:
    bool Init() {
        hSharedMemory = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, SHMEM_SIZE, SHMEM_NAME);
        if (!hSharedMemory) return false;

        sharedData = (IPCMessage*)MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHMEM_SIZE);
        if (!sharedData) {
            CloseHandle(hSharedMemory);
            return false;
        }

        if (!scriptLibrary.Init()) {
            return false;
        }

        if (!g_overlay.Init()) {
            return false;
        }

        scriptBuffer[0] = '\0';
        return true;
    }
    
    bool InjectDLL() {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        DWORD pid = 0;
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_stricmp(pe32.szExeFile, "RobloxPlayerBeta.exe") == 0) {
                    pid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);

        if (!pid) return false;

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) return false;

        char dllPath[MAX_PATH];
        GetModuleFileNameA(GetModuleHandleA(nullptr), dllPath, MAX_PATH);
        char* lastSlash = strrchr(dllPath, '\\');
        if (lastSlash) {
            strcpy(lastSlash + 1, "executor_full.dll");
        }

        SIZE_T pathLen = strlen(dllPath) + 1;
        LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT, PAGE_READWRITE);
        if (!remoteMem) {
            CloseHandle(hProcess);
            return false;
        }

        WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, nullptr);

        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

        HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, 
            (LPTHREAD_START_ROUTINE)pLoadLibrary, remoteMem, 0, nullptr);

        if (hThread) {
            WaitForSingleObject(hThread, 5000);
            CloseHandle(hThread);
        }

        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);

        dllInjected = true;
        return true;
    }
    
    void ExecuteScript() {
        if (!sharedData || !dllInjected) return;
        
        strncpy(sharedData->script, scriptBuffer, sizeof(sharedData->script) - 1);
        sharedData->script[sizeof(sharedData->script) - 1] = '\0';
        sharedData->execute = true;
        
        if (strlen(scriptBuffer) > 0) {
            scriptHistory.push_back(scriptBuffer);
        }
    }
    
    void Update() {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menuOpen = !menuOpen;
        }
        
        g_overlay.BeginRender();
        
        if (menuOpen) {
            RenderUI();
        }
        
        g_overlay.EndRender();
    }
    
    void RenderUI() {
        ImGui::SetNextWindowBgAlpha(0.95f);
        
        // Premium window styling
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        
        ImGui::Begin("Professional Executor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        
        ImGui::PopStyleVar(2);
        
        // Custom header with gradient effect
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.12f, 0.98f));
        ImGui::BeginChild("##Header", ImVec2(930, 60), true);
        ImGui::PopStyleColor();
        
        // Logo/title area
        ImGui::SetCursorPos(ImVec2(20, 15));
        ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.95f, 1.0f), "⚡");
        ImGui::SameLine();
        ImGui::SetCursorPosY(15);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Professional Executor");
        ImGui::SameLine();
        ImGui::SetCursorPosX(760);
        ImGui::SetCursorPosY(15);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "v2.0");
        
        ImGui::EndChild();
        
        // Main content area
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));
        ImGui::BeginChild("##MainContent", ImVec2(930, 630), true);
        ImGui::PopStyleVar();
        
        // DLL status with premium styling
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.04f, 0.06f, 0.95f));
        ImGui::BeginChild("##Status", ImVec2(898, 50), true);
        ImGui::PopStyleColor();
        
        ImGui::SetCursorPos(ImVec2(20, 15));
        ImGui::Text("DLL Status: ");
        ImGui::SameLine();
        if (dllInjected) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "● Injected");
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f), "● Not Injected");
        }
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(730);
        if (!dllInjected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 0.95f));
            if (ImGui::Button("Inject DLL", ImVec2(140, 30))) {
                InjectDLL();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "✓ Ready");
        }
        
        ImGui::EndChild();
        
        ImGui::Spacing();
        
        // Premium tabs
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.08f, 0.08f, 0.12f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.15f, 0.15f, 0.2f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.2f, 0.4f, 0.8f, 0.95f));
        
        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("📝 Script Editor")) {
                RenderScriptEditor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("📚 Script Library")) {
                RenderScriptLibrary();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("⚡ RemoteEvents")) {
                RenderRemoteEvents();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("⚙️ Settings")) {
                RenderSettings();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndChild();
        
        // Footer
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.12f, 0.98f));
        ImGui::BeginChild("##Footer", ImVec2(930, 40), true);
        ImGui::PopStyleColor();
        
        ImGui::SetCursorPos(ImVec2(20, 10));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Press INSERT to toggle menu");
        
        ImGui::SetCursorPosX(780);
        ImGui::SetCursorPosY(10);
        ImGui::TextColored(ImVec4(0.35f, 0.65f, 0.95f, 1.0f), "Built with ❤️");
        
        ImGui::EndChild();
        
        ImGui::End();
    }
    
    void RenderScriptEditor() {
        ImGui::Text("Script Editor");
        ImGui::Separator();
        
        ImGui::InputTextMultiline("##Script", scriptBuffer, sizeof(scriptBuffer), 
            ImVec2(880, 400), ImGuiInputTextFlags_AllowTabInput);
        
        if (ImGui::Button("Execute", ImVec2(200, 35))) {
            ExecuteScript();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(200, 35))) {
            scriptBuffer[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to Library", ImVec2(200, 35))) {
            ImGui::OpenPopup("SaveScriptPopup");
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy", ImVec2(200, 35))) {
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE, strlen(scriptBuffer) + 1);
                if (hglb) {
                    char* lptstr = (char*)GlobalLock(hglb);
                    memcpy(lptstr, scriptBuffer, strlen(scriptBuffer) + 1);
                    GlobalUnlock(hglb);
                    SetClipboardData(CF_TEXT, hglb);
                }
                CloseClipboard();
            }
        }
        
        // Save script popup
        if (ImGui::BeginPopup("SaveScriptPopup")) {
            ImGui::InputText("Script Name", newScriptName, sizeof(newScriptName));
            ImGui::InputText("Category", newScriptCategory, sizeof(newScriptCategory));
            if (ImGui::Button("Save")) {
                scriptLibrary.SaveScript(newScriptName, scriptBuffer, newScriptCategory);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        
        ImGui::Separator();
        
        // Script history
        ImGui::Text("Execution History:");
        if (ImGui::BeginChild("##History", ImVec2(880, 100))) {
            for (int i = (int)scriptHistory.size() - 1; i >= 0; i--) {
                if (ImGui::Selectable(scriptHistory[i].c_str())) {
                    strncpy(scriptBuffer, scriptHistory[i].c_str(), sizeof(scriptBuffer) - 1);
                    scriptBuffer[sizeof(scriptBuffer) - 1] = '\0';
                }
            }
            ImGui::EndChild();
        }
    }
    
    void RenderScriptLibrary() {
        ImGui::Text("Script Library");
        ImGui::Separator();
        
        const auto& scripts = scriptLibrary.GetScripts();
        
        // Category filter
        static int selectedCategory = 0;
        const char* categories[] = {"All", "trolling", "utility", "admin", "misc"};
        ImGui::Combo("Category", &selectedCategory, categories, 5);
        
        ImGui::Separator();
        
        // Script list
        if (ImGui::BeginChild("##ScriptList", ImVec2(300, 400))) {
            for (const auto& script : scripts) {
                if (selectedCategory > 0 && script.category != categories[selectedCategory]) {
                    continue;
                }
                
                std::string label = script.name + " (" + script.category + ")";
                if (ImGui::Selectable(label.c_str())) {
                    strncpy(scriptBuffer, script.code.c_str(), sizeof(scriptBuffer) - 1);
                    scriptBuffer[sizeof(scriptBuffer) - 1] = '\0';
                }
            }
            ImGui::EndChild();
        }
        
        ImGui::SameLine();
        
        // Script preview
        if (ImGui::BeginChild("##ScriptPreview", ImVec2(560, 400))) {
            ImGui::Text("Preview:");
            ImGui::Separator();
            ImGui::TextWrapped(scriptBuffer);
            ImGui::EndChild();
        }
        
        ImGui::Separator();
        
        // Actions
        if (ImGui::Button("Execute Selected", ImVec2(200, 30))) {
            ExecuteScript();
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh Library", ImVec2(200, 30))) {
            scriptLibrary.LoadScripts();
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Folder", ImVec2(200, 30))) {
            ShellExecuteA(nullptr, "open", scriptLibrary.GetLibraryPath().c_str(), nullptr, nullptr, SW_SHOW);
        }
    }
    
    void RenderRemoteEvents() {
        ImGui::Text("RemoteEvent Exploitation");
        ImGui::Separator();
        
        ImGui::Text("RemoteEvents Found: %d", sharedData ? sharedData->remoteEventCount : 0);
        if (ImGui::Button("Scan RemoteEvents", ImVec2(200, 30))) {
            if (sharedData) sharedData->scanRemoteEvents = true;
        }
        
        ImGui::Separator();
        
        // RemoteEvents list
        if (sharedData && sharedData->remoteEventCount > 0) {
            if (ImGui::BeginChild("##RemoteEvents", ImVec2(880, 200))) {
                std::string events = sharedData->remoteEvents;
                size_t pos = 0;
                std::string delimiter = ",";
                while ((pos = events.find(delimiter)) != std::string::npos) {
                    std::string token = events.substr(0, pos);
                    if (ImGui::Selectable(token.c_str())) {
                        strncpy(sharedData->remoteEventName, token.c_str(), sizeof(sharedData->remoteEventName) - 1);
                    }
                    events.erase(0, pos + delimiter.length());
                }
                if (!events.empty() && ImGui::Selectable(events.c_str())) {
                    strncpy(sharedData->remoteEventName, events.c_str(), sizeof(sharedData->remoteEventName) - 1);
                }
                ImGui::EndChild();
            }
        }
        
        ImGui::Separator();
        
        // Fire RemoteEvent
        ImGui::Text("Fire RemoteEvent:");
        ImGui::InputText("Event Name", sharedData ? sharedData->remoteEventName : "", 256);
        ImGui::InputTextMultiline("Arguments (Lua table format)", sharedData ? sharedData->remoteEventArgs : "", 2048, ImVec2(880, 100));
        if (ImGui::Button("Fire RemoteEvent", ImVec2(200, 30))) {
            if (sharedData) sharedData->fireRemoteEvent = true;
        }
    }
    
    void RenderSettings() {
        ImGui::Text("Settings");
        ImGui::Separator();
        
        ImGui::Checkbox("Auto-inject on startup", &autoInject);
        ImGui::Checkbox("Minimize to tray", &minimizeToTray);
        ImGui::Checkbox("Always on top", &topMost);
        ImGui::Checkbox("Show watermark", &showWatermark);
        
        ImGui::Separator();
        
        ImGui::SliderInt("Font Size", &fontSize, 12, 24);
        
        ImGui::Separator();
        
        if (ImGui::Button("Reset to Defaults", ImVec2(200, 30))) {
            autoInject = true;
            minimizeToTray = false;
            topMost = true;
            fontSize = 14;
            showWatermark = true;
        }
    }
    
    void Cleanup() {
        g_overlay.Cleanup();
        if (sharedData) {
            UnmapViewOfFile(sharedData);
            sharedData = nullptr;
        }
        if (hSharedMemory) {
            CloseHandle(hSharedMemory);
            hSharedMemory = nullptr;
        }
    }
};

int main() {
    ExecutorUI ui;
    
    std::cout << "[+] Professional Executor UI v2.0" << std::endl;
    std::cout << "[+] Features: Script Editor, Library, RemoteEvents, Settings" << std::endl;
    std::cout << "[+] Press INSERT to toggle menu" << std::endl;
    
    if (!ui.Init()) {
        std::cout << "[-] Failed to initialize" << std::endl;
        system("pause");
        return 1;
    }
    
    std::cout << "[+] UI running..." << std::endl;
    
    while (true) {
        ui.Update();
        Sleep(1);
    }
    
    ui.Cleanup();
    return 0;
}
