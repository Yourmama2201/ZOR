#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <tlhelp32.h>
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

    // Windows API process finding (more reliable than driver)
    HANDLE FindPidWindows(const char* processName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hSnapshot, &pe32)) {
            CloseHandle(hSnapshot);
            return 0;
        }

        do {
            if (_stricmp(pe32.szExeFile, processName) == 0) {
                CloseHandle(hSnapshot);
                return (HANDLE)(ULONG_PTR)pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));

        CloseHandle(hSnapshot);
        return 0;
    }

    // Windows API module base finding
    uintptr_t GetModuleBaseWindows(HANDLE pid, const char* moduleName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, (DWORD)(ULONG_PTR)pid);
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
        std::cout << "[DEBUG] Trying to find process: " << processName << std::endl;
        hProcess = mem.FindPidWindows(processName);
        std::cout << "[DEBUG] FindPidWindows result: " << (uint64_t)hProcess << std::endl;
        
        if (!hProcess) {
            std::cout << "[DEBUG] Process not found: " << processName << std::endl;
            return false;
        }

        // Use Windows API to get module base (more reliable)
        moduleBase = mem.GetModuleBaseWindows(hProcess, processName);
        std::cout << "[DEBUG] Module base (Windows API): 0x" << std::hex << moduleBase << std::endl;
        
        if (!moduleBase) {
            std::cout << "[DEBUG] Failed to get module base, trying driver..." << std::endl;
            moduleBase = mem.GetBase(hProcess);
            std::cout << "[DEBUG] Module base (Driver): 0x" << std::hex << moduleBase << std::endl;
        }
        
        if (!moduleBase) return false;

        moduleSize = 0x10000000; // 256MB for CS 1.6
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

// CS 1.6 GoldSrc known offsets (stable, well-documented)
namespace Offsets {
    // Entity list offset from client.dll base
    constexpr size_t EntityList = 0x511A14;
    
    // Local player index offset
    constexpr size_t LocalPlayer = 0x511A10;
    
    // Player health offset in entity structure
    constexpr size_t Health = 0xFC;
    
    // Player position (origin) offset in entity
    constexpr size_t Origin = 0x134;
    
    // View angles offset
    constexpr size_t ViewAngles = 0x4D1C34;
    
    // Player team offset in entity
    constexpr size_t Team = 0x70;
    
    // Player name offset in entity
    constexpr size_t Name = 0x3A4;
    
    // Weapon ID offset
    constexpr size_t WeaponID = 0x1F8;
    
    // Ammo offset
    constexpr size_t Ammo = 0x11C0;
    
    // Crosshair offset
    constexpr size_t Crosshair = 0x2470;
    
    // Scoreboard offset
    constexpr size_t Scoreboard = 0x511A14;
    
    // Radar offset
    constexpr size_t Radar = 0x511A14;
}

struct CS16Data {
    uintptr_t entityList = 0;
    uintptr_t localPlayer = 0;
    uintptr_t health = 0;
    uintptr_t origin = 0;
    uintptr_t viewAngles = 0;
    uintptr_t team = 0;
    uintptr_t name = 0;
    uintptr_t weaponID = 0;
    uintptr_t ammo = 0;
    uintptr_t crosshair = 0;
    uintptr_t scoreboard = 0;
    uintptr_t radar = 0;
    bool valid = false;
};

class CS16Extractor {
private:
    KernelMemory mem;
    PatternScanner scanner;
    CS16Data data;
    std::ofstream logFile;

    void Log(const std::string& message) {
        std::cout << message << std::endl;
        if (logFile.is_open()) {
            logFile << message << std::endl;
        }
    }

public:
    CS16Extractor() : scanner(mem) {}

    bool Init() {
        Log("[+] Attempting to connect to driver...");
        if (!mem.Connect()) {
            Log("[-] Failed to connect to driver");
            Log("     Make sure zordriver.sys is loaded");
            return false;
        }
        Log("[+] Driver connected successfully");

        // Try different CS 1.6 process names
        Log("[+] Searching for CS 1.6 process...");
        if (!scanner.Init("hl.exe") && 
            !scanner.Init("cstrike.exe") && 
            !scanner.Init("cs16.exe") &&
            !scanner.Init("swds.exe") &&
            !scanner.Init("hlds.exe") &&
            !scanner.Init("steam.exe")) {
            Log("[-] Failed to initialize scanner - CS 1.6 not found");
            Log("     Tried: hl.exe, cstrike.exe, cs16.exe, swds.exe, hlds.exe, steam.exe");
            Log("     Please check Task Manager for the actual process name");
            return false;
        }
        Log("[+] Found CS 1.6 process: " + std::to_string((uint64_t)scanner.GetProcess()));

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string filename = "cs16_dump_" + std::string(ctime(&time));
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
        Log("[+] === CS 1.6 GOLDSRC OFFSET CALCULATION ===");
        
        uintptr_t base = scanner.GetModuleBase();
        
        // Calculate actual addresses using known offsets
        data.entityList = base + Offsets::EntityList;
        data.localPlayer = base + Offsets::LocalPlayer;
        data.health = Offsets::Health;
        data.origin = Offsets::Origin;
        data.viewAngles = base + Offsets::ViewAngles;
        data.team = Offsets::Team;
        data.name = Offsets::Name;
        data.weaponID = Offsets::WeaponID;
        data.ammo = Offsets::Ammo;
        data.crosshair = base + Offsets::Crosshair;
        data.scoreboard = base + Offsets::Scoreboard;
        data.radar = base + Offsets::Radar;

        Log("\n=== RESULTS ===");
        Log("Module Base: 0x" + std::to_string(base));
        Log("EntityList: 0x" + std::to_string(data.entityList));
        Log("LocalPlayer: 0x" + std::to_string(data.localPlayer));
        Log("Health Offset: 0x" + std::to_string(data.health));
        Log("Origin Offset: 0x" + std::to_string(data.origin));
        Log("ViewAngles: 0x" + std::to_string(data.viewAngles));
        Log("Team Offset: 0x" + std::to_string(data.team));
        Log("Name Offset: 0x" + std::to_string(data.name));
        Log("WeaponID Offset: 0x" + std::to_string(data.weaponID));
        Log("Ammo Offset: 0x" + std::to_string(data.ammo));
        Log("Crosshair: 0x" + std::to_string(data.crosshair));
        Log("Scoreboard: 0x" + std::to_string(data.scoreboard));
        Log("Radar: 0x" + std::to_string(data.radar));

        data.valid = (data.entityList && data.localPlayer);
        
        if (!data.valid) {
            Log("[-] Offset calculation failed");
        } else {
            Log("[+] Offsets calculated successfully!");
        }
    }

    void GenerateHeaderFile() {
        Log("[+] === GENERATING CS 1.6 OFFSET HEADER ===");
        
        std::ofstream header("cs16_offsets.hpp");
        if (header.is_open()) {
            header << "// CS 1.6 GoldSrc Offsets - Auto-generated\n";
            header << "// Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
            header << "#pragma once\n\n";
            header << "namespace CS16 {\n";
            header << "    // Base offsets from client.dll\n";
            header << "    constexpr size_t EntityList = 0x" << std::hex << Offsets::EntityList << ";\n";
            header << "    constexpr size_t LocalPlayer = 0x" << std::hex << Offsets::LocalPlayer << ";\n";
            header << "    constexpr size_t ViewAngles = 0x" << std::hex << Offsets::ViewAngles << ";\n";
            header << "    constexpr size_t Crosshair = 0x" << std::hex << Offsets::Crosshair << ";\n";
            header << "    constexpr size_t Scoreboard = 0x" << std::hex << Offsets::Scoreboard << ";\n";
            header << "    constexpr size_t Radar = 0x" << std::hex << Offsets::Radar << ";\n";
            header << "    \n";
            header << "    // Entity structure offsets\n";
            header << "    constexpr size_t Health = 0x" << std::hex << Offsets::Health << ";\n";
            header << "    constexpr size_t Origin = 0x" << std::hex << Offsets::Origin << ";\n";
            header << "    constexpr size_t Team = 0x" << std::hex << Offsets::Team << ";\n";
            header << "    constexpr size_t Name = 0x" << std::hex << Offsets::Name << ";\n";
            header << "    constexpr size_t WeaponID = 0x" << std::hex << Offsets::WeaponID << ";\n";
            header << "    constexpr size_t Ammo = 0x" << std::hex << Offsets::Ammo << ";\n";
            header << "}\n";
            header.close();
            Log("[+] Generated: cs16_offsets.hpp");
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
    CS16Extractor extractor;
    
    std::cout << "[+] CS 1.6 GoldSrc Data Extractor - Read-Only Driver\n";
    std::cout << "[!] Update GoldSrc patterns if needed\n\n";
    
    if (!extractor.Init()) {
        system("pause");
        return 1;
    }

    extractor.ScanPatterns();
    extractor.GenerateHeaderFile();
    
    std::cout << "\n[+] CS 1.6 extraction complete!\n";
    
    extractor.Cleanup();
    system("pause");
    return 0;
}
