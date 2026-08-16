#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <fstream>
#include <chrono>

#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FIND_PID    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MEMORY_REQUEST {
    HANDLE ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;
    SIZE_T Size;
} MEMORY_REQUEST, *PMEMORY_REQUEST;

class KernelMemory {
private:
    HANDLE hDriver = nullptr;

public:
    bool Connect() {
        hDriver = CreateFileA("\\\\.\\ZOR", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        return hDriver != INVALID_HANDLE_VALUE;
    }

    bool Read(HANDLE pid, uintptr_t address, void* buffer, size_t size) {
        std::vector<BYTE> data(sizeof(MEMORY_REQUEST) + size);
        MEMORY_REQUEST* req = (MEMORY_REQUEST*)data.data();
        req->ProcessId = pid;
        req->Address = address;
        req->Size = size;

        DWORD bytes = 0;
        BOOL result = DeviceIoControl(hDriver, IOCTL_READ_MEMORY, data.data(), (DWORD)data.size(),
                                      data.data(), (DWORD)data.size(), &bytes, NULL);
        if (result && bytes >= sizeof(MEMORY_REQUEST) + size) {
            memcpy(buffer, data.data() + sizeof(MEMORY_REQUEST), size);
            return true;
        }
        return false;
    }

    template<typename T>
    T Read(HANDLE pid, uintptr_t address) {
        T value = {};
        Read(pid, address, &value, sizeof(T));
        return value;
    }

    HANDLE FindPid(const char* processName) {
        char buf[64] = {0};
        strncpy(buf, processName, 63);
        DWORD pid = 0;
        DWORD bytes = 0;
        if (DeviceIoControl(hDriver, IOCTL_FIND_PID, buf, (DWORD)strlen(buf) + 1,
                          &pid, sizeof(pid), &bytes, NULL)) {
            return (HANDLE)(ULONG_PTR)pid;
        }
        return 0;
    }

    uintptr_t GetBase(HANDLE pid) {
        DWORD bytes = 0;
        uintptr_t base = 0;
        if (DeviceIoControl(hDriver, IOCTL_GET_BASE, &pid, sizeof(pid),
                          &base, sizeof(base), &bytes, NULL)) {
            return base;
        }
        return 0;
    }

    void Disconnect() {
        if (hDriver && hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(hDriver);
            hDriver = nullptr;
        }
    }
};

class PatternScanner {
private:
    KernelMemory& mem;
    HANDLE hProcess = nullptr;
    uintptr_t moduleBase = 0;
    size_t moduleSize = 0;

    std::vector<uint8_t> PatternToBytes(const char* pattern) {
        std::vector<uint8_t> bytes;
        const char* start = pattern;
        const char* end = pattern + strlen(pattern);
        
        while (start < end) {
            if (*start == ' ') {
                start++;
                continue;
            }
            if (*start == '?') {
                bytes.push_back(0xFF);
                start++;
                if (*start == '?') start++;
            } else {
                bytes.push_back(strtol(start, nullptr, 16));
                start += 2;
            }
        }
        return bytes;
    }

public:
    PatternScanner(KernelMemory& m) : mem(m) {}

    bool Init(const char* processName) {
        hProcess = mem.FindPid(processName);
        if (!hProcess) return false;

        moduleBase = mem.GetBase(hProcess);
        if (!moduleBase) return false;

        moduleSize = 0x10000000; // 256MB for Roblox
        return true;
    }

    uintptr_t ScanPattern(const char* pattern) {
        std::vector<uint8_t> patternBytes = PatternToBytes(pattern);
        if (patternBytes.empty()) return 0;

        size_t scanSize = moduleSize;
        uintptr_t scanStart = moduleBase;

        const size_t chunkSize = 0x10000;
        std::vector<uint8_t> buffer(chunkSize);

        for (size_t offset = 0; offset < scanSize; offset += chunkSize) {
            size_t readSize = min(chunkSize, scanSize - offset);
            
            if (!mem.Read(hProcess, scanStart + offset, buffer.data(), readSize)) {
                continue;
            }

            for (size_t i = 0; i < readSize - patternBytes.size() + 1; i++) {
                bool found = true;
                for (size_t j = 0; j < patternBytes.size(); j++) {
                    if (patternBytes[j] != 0xFF && buffer[i + j] != patternBytes[j]) {
                        found = false;
                        break;
                    }
                }
                if (found) {
                    return scanStart + offset + i;
                }
            }
        }

        return 0;
    }

    uintptr_t GetModuleBase() const { return moduleBase; }
    HANDLE GetProcess() const { return hProcess; }
};

// Roblox patterns - common Lua VM and game structures
namespace Patterns {
    // Lua state - rL register pattern
    const char* LuaState = "48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01 FF 50 ?";
    
    // Workspace - DataModel pattern
    const char* Workspace = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 10";
    
    // Players - Players service pattern
    const char* Players = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 18";
    
    // LocalPlayer - pattern in Players service
    const char* LocalPlayer = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 20";
    
    // Character - Character model pattern
    const char* Character = "48 8B 88 ? ? ? ? 48 85 C9 74 ? 48 8B 49 08";
    
    // Humanoid - Humanoid instance pattern
    const char* Humanoid = "48 8B 88 ? ? ? ? 48 85 C9 74 ? F3 0F 10 81";
    
    // Health - Health property pattern
    const char* Health = "F3 0F 10 81 ? ? ? ? F3 0F 59 05";
    
    // Position - CFrame position pattern
    const char* Position = "F3 0F 10 05 ? ? ? ? 48 8D 0C";
    
    // Camera - Workspace.CurrentCamera pattern
    const char* Camera = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 28";
    
    // CFrame - Coordinate frame pattern
    const char* CFrame = "48 8B 88 ? ? ? ? 48 85 C9 74 ? F3 0F 10 00";
    
    // Lighting - Lighting service pattern
    const char* Lighting = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 30";
    
    // ReplicatedStorage - ReplicatedStorage pattern
    const char* ReplicatedStorage = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 40 38";
}

struct RobloxData {
    uintptr_t luaState = 0;
    uintptr_t workspace = 0;
    uintptr_t players = 0;
    uintptr_t localPlayer = 0;
    uintptr_t character = 0;
    uintptr_t humanoid = 0;
    uintptr_t health = 0;
    uintptr_t position = 0;
    uintptr_t camera = 0;
    uintptr_t cframe = 0;
    uintptr_t lighting = 0;
    uintptr_t replicatedStorage = 0;
    bool valid = false;
};

class RobloxExtractor {
private:
    KernelMemory mem;
    PatternScanner scanner;
    RobloxData data;
    std::ofstream logFile;

    void Log(const std::string& message) {
        std::cout << message << std::endl;
        if (logFile.is_open()) {
            logFile << message << std::endl;
        }
    }

public:
    RobloxExtractor() : scanner(mem) {}

    bool Init() {
        if (!mem.Connect()) {
            Log("[-] Failed to connect to driver");
            return false;
        }

        if (!scanner.Init("RobloxPlayerBeta.exe")) {
            Log("[-] Failed to initialize scanner");
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string filename = "roblox_dump_" + std::string(ctime(&time));
        filename.erase(std::remove(filename.begin(), filename.end(), ':'), filename.end());
        filename.erase(std::remove(filename.begin(), filename.end(), '\n'), filename.end());
        filename += ".txt";
        
        logFile.open(filename);
        if (logFile.is_open()) {
            Log("[+] Log file: " + filename);
        }

        Log("[+] Module base: 0x" + std::to_string(scanner.GetModuleBase()));
        return true;
    }

    void ScanPatterns() {
        Log("[+] === ROBLOX PATTERN SCAN ===");
        
        data.luaState = scanner.ScanPattern(Patterns::LuaState);
        data.workspace = scanner.ScanPattern(Patterns::Workspace);
        data.players = scanner.ScanPattern(Patterns::Players);
        data.localPlayer = scanner.ScanPattern(Patterns::LocalPlayer);
        data.character = scanner.ScanPattern(Patterns::Character);
        data.humanoid = scanner.ScanPattern(Patterns::Humanoid);
        data.health = scanner.ScanPattern(Patterns::Health);
        data.position = scanner.ScanPattern(Patterns::Position);
        data.camera = scanner.ScanPattern(Patterns::Camera);
        data.cframe = scanner.ScanPattern(Patterns::CFrame);
        data.lighting = scanner.ScanPattern(Patterns::Lighting);
        data.replicatedStorage = scanner.ScanPattern(Patterns::ReplicatedStorage);

        Log("\n=== RESULTS ===");
        Log("LuaState: 0x" + std::to_string(data.luaState));
        Log("Workspace: 0x" + std::to_string(data.workspace));
        Log("Players: 0x" + std::to_string(data.players));
        Log("LocalPlayer: 0x" + std::to_string(data.localPlayer));
        Log("Character: 0x" + std::to_string(data.character));
        Log("Humanoid: 0x" + std::to_string(data.humanoid));
        Log("Health: 0x" + std::to_string(data.health));
        Log("Position: 0x" + std::to_string(data.position));
        Log("Camera: 0x" + std::to_string(data.camera));
        Log("CFrame: 0x" + std::to_string(data.cframe));
        Log("Lighting: 0x" + std::to_string(data.lighting));
        Log("ReplicatedStorage: 0x" + std::to_string(data.replicatedStorage));

        data.valid = (data.workspace && data.players && data.localPlayer);
        
        if (!data.valid) {
            Log("[-] Some patterns failed - update Roblox patterns");
        } else {
            Log("[+] Core patterns found successfully!");
        }
    }

    void GenerateHeaderFile() {
        Log("[+] === GENERATING ROBLOX OFFSET HEADER ===");
        
        std::ofstream header("roblox_offsets.hpp");
        if (header.is_open()) {
            header << "// Roblox Offsets - Auto-generated\n";
            header << "// Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
            header << "#pragma once\n\n";
            header << "namespace Roblox {\n";
            header << "    constexpr uintptr_t LuaState = 0x" << std::hex << data.luaState << ";\n";
            header << "    constexpr uintptr_t Workspace = 0x" << std::hex << data.workspace << ";\n";
            header << "    constexpr uintptr_t Players = 0x" << std::hex << data.players << ";\n";
            header << "    constexpr uintptr_t LocalPlayer = 0x" << std::hex << data.localPlayer << ";\n";
            header << "    constexpr uintptr_t Character = 0x" << std::hex << data.character << ";\n";
            header << "    constexpr uintptr_t Humanoid = 0x" << std::hex << data.humanoid << ";\n";
            header << "    constexpr uintptr_t Health = 0x" << std::hex << data.health << ";\n";
            header << "    constexpr uintptr_t Position = 0x" << std::hex << data.position << ";\n";
            header << "    constexpr uintptr_t Camera = 0x" << std::hex << data.camera << ";\n";
            header << "    constexpr uintptr_t CFrame = 0x" << std::hex << data.cframe << ";\n";
            header << "    constexpr uintptr_t Lighting = 0x" << std::hex << data.lighting << ";\n";
            header << "    constexpr uintptr_t ReplicatedStorage = 0x" << std::hex << data.replicatedStorage << ";\n";
            header << "}\n";
            header.close();
            Log("[+] Generated: roblox_offsets.hpp");
        }
    }

    void Cleanup() {
        if (logFile.is_open()) {
            logFile.close();
        }
        mem.Disconnect();
    }
};

int main() {
    RobloxExtractor extractor;
    
    std::cout << "[+] Roblox Data Extractor - Read-Only Driver\n";
    std::cout << "[!] Update Roblox patterns if needed\n\n";
    
    if (!extractor.Init()) {
        system("pause");
        return 1;
    }

    extractor.ScanPatterns();
    extractor.GenerateHeaderFile();
    
    std::cout << "\n[+] Roblox extraction complete!\n";
    
    extractor.Cleanup();
    system("pause");
    return 0;
}
