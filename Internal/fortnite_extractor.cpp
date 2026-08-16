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

        moduleSize = 0x20000000; // 512MB for Fortnite
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

// Fortnite UE4 patterns - these are common UE4 patterns
namespace Patterns {
    // GWorld - main world pointer
    const char* GWorld = "48 8B 1D ? ? ? ? 48 85 DB 74 ?";
    
    // UWorld structure
    const char* UWorld = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // ULevel - level/actor array
    const char* ULevel = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // AActor - actor base
    const char* AActor = "48 8B 81 ? ? ? ? 48 85 C0 74 ?";
    
    // UPlayer - player controller
    const char* UPlayer = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // UGameInstance - game instance
    const char* UGameInstance = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // LocalPlayer
    const char* LocalPlayer = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // PlayerController
    const char* PlayerController = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // Pawn - player character
    const char* Pawn = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // RootComponent - scene component
    const char* RootComponent = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
    
    // ComponentToWorld - transform matrix
    const char* ComponentToWorld = "48 8B 88 ? ? ? ? 48 85 C9 74 ?";
}

struct FortniteData {
    uintptr_t gWorld = 0;
    uintptr_t uWorld = 0;
    uintptr_t uLevel = 0;
    uintptr_t aActor = 0;
    uintptr_t uPlayer = 0;
    uintptr_t uGameInstance = 0;
    uintptr_t localPlayer = 0;
    uintptr_t playerController = 0;
    uintptr_t pawn = 0;
    uintptr_t rootComponent = 0;
    uintptr_t componentToWorld = 0;
    bool valid = false;
};

class FortniteExtractor {
private:
    KernelMemory mem;
    PatternScanner scanner;
    FortniteData data;
    std::ofstream logFile;

    void Log(const std::string& message) {
        std::cout << message << std::endl;
        if (logFile.is_open()) {
            logFile << message << std::endl;
        }
    }

public:
    FortniteExtractor() : scanner(mem) {}

    bool Init() {
        if (!mem.Connect()) {
            Log("[-] Failed to connect to driver");
            return false;
        }

        if (!scanner.Init("FortniteClient-Win64-Shipping.exe")) {
            Log("[-] Failed to initialize scanner");
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string filename = "fortnite_dump_" + std::string(ctime(&time));
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
        Log("[+] === FORTNITE UE4 PATTERN SCAN ===");
        
        data.gWorld = scanner.ScanPattern(Patterns::GWorld);
        data.uWorld = scanner.ScanPattern(Patterns::UWorld);
        data.uLevel = scanner.ScanPattern(Patterns::ULevel);
        data.aActor = scanner.ScanPattern(Patterns::AActor);
        data.uPlayer = scanner.ScanPattern(Patterns::UPlayer);
        data.uGameInstance = scanner.ScanPattern(Patterns::UGameInstance);
        data.localPlayer = scanner.ScanPattern(Patterns::LocalPlayer);
        data.playerController = scanner.ScanPattern(Patterns::PlayerController);
        data.pawn = scanner.ScanPattern(Patterns::Pawn);
        data.rootComponent = scanner.ScanPattern(Patterns::RootComponent);
        data.componentToWorld = scanner.ScanPattern(Patterns::ComponentToWorld);

        Log("\n=== RESULTS ===");
        Log("GWorld: 0x" + std::to_string(data.gWorld));
        Log("UWorld: 0x" + std::to_string(data.uWorld));
        Log("ULevel: 0x" + std::to_string(data.uLevel));
        Log("AActor: 0x" + std::to_string(data.aActor));
        Log("UPlayer: 0x" + std::to_string(data.uPlayer));
        Log("UGameInstance: 0x" + std::to_string(data.uGameInstance));
        Log("LocalPlayer: 0x" + std::to_string(data.localPlayer));
        Log("PlayerController: 0x" + std::to_string(data.playerController));
        Log("Pawn: 0x" + std::to_string(data.pawn));
        Log("RootComponent: 0x" + std::to_string(data.rootComponent));
        Log("ComponentToWorld: 0x" + std::to_string(data.componentToWorld));

        data.valid = (data.gWorld && data.uWorld && data.uLevel);
        
        if (!data.valid) {
            Log("[-] Some patterns failed - update UE4 patterns");
        } else {
            Log("[+] Core patterns found successfully!");
        }
    }

    void GenerateHeaderFile() {
        Log("[+] === GENERATING FORTNITE OFFSET HEADER ===");
        
        std::ofstream header("fortnite_offsets.hpp");
        if (header.is_open()) {
            header << "// Fortnite UE4 Offsets - Auto-generated\n";
            header << "// Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
            header << "#pragma once\n\n";
            header << "namespace Fortnite {\n";
            header << "    constexpr uintptr_t GWorld = 0x" << std::hex << data.gWorld << ";\n";
            header << "    constexpr uintptr_t UWorld = 0x" << std::hex << data.uWorld << ";\n";
            header << "    constexpr uintptr_t ULevel = 0x" << std::hex << data.uLevel << ";\n";
            header << "    constexpr uintptr_t AActor = 0x" << std::hex << data.aActor << ";\n";
            header << "    constexpr uintptr_t UPlayer = 0x" << std::hex << data.uPlayer << ";\n";
            header << "    constexpr uintptr_t UGameInstance = 0x" << std::hex << data.uGameInstance << ";\n";
            header << "    constexpr uintptr_t LocalPlayer = 0x" << std::hex << data.localPlayer << ";\n";
            header << "    constexpr uintptr_t PlayerController = 0x" << std::hex << data.playerController << ";\n";
            header << "    constexpr uintptr_t Pawn = 0x" << std::hex << data.pawn << ";\n";
            header << "    constexpr uintptr_t RootComponent = 0x" << std::hex << data.rootComponent << ";\n";
            header << "    constexpr uintptr_t ComponentToWorld = 0x" << std::hex << data.componentToWorld << ";\n";
            header << "}\n";
            header.close();
            Log("[+] Generated: fortnite_offsets.hpp");
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
    FortniteExtractor extractor;
    
    std::cout << "[+] Fortnite UE4 Data Extractor - Read-Only Driver\n";
    std::cout << "[!] Update UE4 patterns if needed\n\n";
    
    if (!extractor.Init()) {
        system("pause");
        return 1;
    }

    extractor.ScanPatterns();
    extractor.GenerateHeaderFile();
    
    std::cout << "\n[+] Fortnite extraction complete!\n";
    
    extractor.Cleanup();
    system("pause");
    return 0;
}
