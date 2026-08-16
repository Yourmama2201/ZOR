#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <d3d9.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

// Shared memory for IPC
#define SHMEM_SIZE 8192
#define SHMEM_NAME "RobloxExecutor_IPC"

struct IPCMessage {
    char script[2048];
    bool execute;
    bool clear;
    bool scanRemoteEvents;
    bool scanBackdoors;
    char remoteEventName[256];
    char remoteEventArgs[1024];
    bool fireRemoteEvent;
    int remoteEventCount;
    char remoteEvents[4096];
    int backdoorCount;
    char backdoors[4096];
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
        
        overlayWindow = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            "Executor UI Overlay", "Executor",
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
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.1f, 0.98f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.3f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.35f, 0.7f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.45f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2f, 0.4f, 0.75f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.05f, 0.08f, 1.0f);
        
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

class ExecutorUI {
private:
    HANDLE hSharedMemory = nullptr;
    IPCMessage* sharedData = nullptr;
    char scriptBuffer[2048];
    std::vector<std::string> scriptHistory;
    bool menuOpen = true;
    bool dllInjected = false;
    
public:
    bool Init() {
        // Try to open shared memory (DLL should create it)
        hSharedMemory = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, SHMEM_SIZE, SHMEM_NAME);
        if (!hSharedMemory) return false;

        sharedData = (IPCMessage*)MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHMEM_SIZE);
        if (!sharedData) {
            CloseHandle(hSharedMemory);
            return false;
        }

        if (!g_overlay.Init()) {
            return false;
        }

        scriptBuffer[0] = '\0';
        return true;
    }
    
    bool InjectDLL() {
        // Find Roblox process
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

        // Get DLL path
        char dllPath[MAX_PATH];
        GetModuleFileNameA(GetModuleHandleA(nullptr), dllPath, MAX_PATH);
        char* lastSlash = strrchr(dllPath, '\\');
        if (lastSlash) {
            strcpy(lastSlash + 1, "executor.dll");
        }

        // Allocate memory in target process
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
        
        // Add to history
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
        ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
        ImGui::Begin("Executor", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::Text("Professional Script Executor");
        ImGui::Separator();
        
        // DLL status
        ImGui::Text("DLL Status: %s", dllInjected ? "Injected" : "Not Injected");
        if (!dllInjected) {
            if (ImGui::Button("Inject DLL", ImVec2(680, 30))) {
                InjectDLL();
            }
        }
        
        ImGui::Separator();
        
        // Server-sided exploitation section
        ImGui::Text("Server-Sided Exploitation");
        ImGui::Separator();
        
        // RemoteEvent scanning
        ImGui::Text("RemoteEvents Found: %d", sharedData ? sharedData->remoteEventCount : 0);
        if (ImGui::Button("Scan RemoteEvents", ImVec2(200, 30))) {
            if (sharedData) sharedData->scanRemoteEvents = true;
        }
        
        // Display RemoteEvents
        if (sharedData && sharedData->remoteEventCount > 0) {
            if (ImGui::BeginChild("##RemoteEvents", ImVec2(680, 100))) {
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
        
        // Fire RemoteEvent
        ImGui::Text("Fire RemoteEvent:");
        ImGui::InputText("Event Name", sharedData ? sharedData->remoteEventName : "", 256);
        ImGui::InputTextMultiline("Arguments (Lua table format)", sharedData ? sharedData->remoteEventArgs : "", 1024, ImVec2(680, 60));
        if (ImGui::Button("Fire RemoteEvent", ImVec2(200, 30))) {
            if (sharedData) sharedData->fireRemoteEvent = true;
        }
        
        ImGui::Separator();
        
        // Backdoor scanning
        ImGui::Text("Backdoors Found: %d", sharedData ? sharedData->backdoorCount : 0);
        if (ImGui::Button("Scan Backdoors", ImVec2(200, 30))) {
            if (sharedData) sharedData->scanBackdoors = true;
        }
        
        // Display Backdoors
        if (sharedData && sharedData->backdoorCount > 0) {
            if (ImGui::BeginChild("##Backdoors", ImVec2(680, 100))) {
                std::string backdoors = sharedData->backdoors;
                size_t pos = 0;
                std::string delimiter = ",";
                while ((pos = backdoors.find(delimiter)) != std::string::npos) {
                    std::string token = backdoors.substr(0, pos);
                    ImGui::Text(token.c_str());
                    backdoors.erase(0, pos + delimiter.length());
                }
                if (!backdoors.empty()) {
                    ImGui::Text(backdoors.c_str());
                }
                ImGui::EndChild();
            }
        }
        
        ImGui::Separator();
        
        // Script editor
        ImGui::Text("Client-Side Script Editor:");
        ImGui::InputTextMultiline("##Script", scriptBuffer, sizeof(scriptBuffer), 
            ImVec2(680, 150), ImGuiInputTextFlags_AllowTabInput);
        
        // Execute button
        if (ImGui::Button("Execute", ImVec2(680, 35))) {
            ExecuteScript();
        }
        
        ImGui::Separator();
        
        // Script history
        ImGui::Text("Script History:");
        if (ImGui::BeginChild("##History", ImVec2(680, 80))) {
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
    
    std::cout << "[+] Professional Executor UI" << std::endl;
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
