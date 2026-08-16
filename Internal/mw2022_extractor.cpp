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
#include <iomanip>

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

        moduleSize = 0x10000000; // 256MB
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

// Comprehensive pattern database - update these with x64dbg
namespace Patterns {
    // Core structures
    const char* REF_DEF = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* CLIENT_INFO = "48 8B 0D ? ? ? ? 48 85 C9 74 ?";
    const char* LOCAL_INDEX = "0B 3D 89 6D E6 ED";
    const char* NAME_ARRAY = "48 8D 0D ? ? ? ? E8 ? ? ? ?";
    
    // Player/Entity systems
    const char* ENTITY_LIST = "48 8B 05 ? ? ? ? 48 85 C0 75 ?";
    const char* BONE_BASE = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* CAMERA_BASE = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // Weapon systems
    const char* WEAPON_ARRAY = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* WEAPON_DEFINITIONS = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // Game state
    const char* GAME_MODE = "8B 05 ? ? ? ? 89 05 ? ? ? ?";
    const char* SCOREBOARD = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // UI/HUD
    const char* HUD_ELEMENTS = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* MINIMAP = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // Network
    const char* NET_CLIENT = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* SERVER_INFO = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // Movement/Physics
    const char* PHYSICS_WORLD = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* MOVEMENT_CONTROLLER = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // Rendering
    const char* RENDERER = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    const char* VIEW_MATRIX = "48 8B 05 ? ? ? ? 48 85 C0 74 ?";
    
    // Add more patterns as needed for comprehensive extraction
}

struct GameData {
    uintptr_t refDef = 0;
    uintptr_t clientInfo = 0;
    uintptr_t localIndex = 0;
    uintptr_t nameArray = 0;
    uintptr_t entityList = 0;
    uintptr_t boneBase = 0;
    uintptr_t cameraBase = 0;
    uintptr_t weaponArray = 0;
    uintptr_t weaponDefinitions = 0;
    uintptr_t gameMode = 0;
    uintptr_t scoreboard = 0;
    uintptr_t hudElements = 0;
    uintptr_t minimap = 0;
    uintptr_t netClient = 0;
    uintptr_t serverInfo = 0;
    uintptr_t physicsWorld = 0;
    uintptr_t movementController = 0;
    uintptr_t renderer = 0;
    uintptr_t viewMatrix = 0;
};

class MW2022Extractor {
private:
    KernelMemory mem;
    PatternScanner scanner;
    GameData data;
    std::ofstream logFile;

    void Log(const std::string& message) {
        std::cout << message << std::endl;
        if (logFile.is_open()) {
            logFile << message << std::endl;
        }
    }

public:
    MW2022Extractor() : scanner(mem) {}

    bool Init() {
        if (!mem.Connect()) {
            Log("[-] Failed to connect to driver");
            return false;
        }

        if (!scanner.Init("cod22-cod.exe")) {
            Log("[-] Failed to initialize scanner");
            return false;
        }

        // Open log file with timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string filename = "mw2022_dump_" + std::string(ctime(&time));
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

    void ScanAllPatterns() {
        Log("[+] === COMPREHENSIVE PATTERN SCAN ===");
        
        data.refDef = scanner.ScanPattern(Patterns::REF_DEF);
        data.clientInfo = scanner.ScanPattern(Patterns::CLIENT_INFO);
        data.localIndex = scanner.ScanPattern(Patterns::LOCAL_INDEX);
        data.nameArray = scanner.ScanPattern(Patterns::NAME_ARRAY);
        data.entityList = scanner.ScanPattern(Patterns::ENTITY_LIST);
        data.boneBase = scanner.ScanPattern(Patterns::BONE_BASE);
        data.cameraBase = scanner.ScanPattern(Patterns::CAMERA_BASE);
        data.weaponArray = scanner.ScanPattern(Patterns::WEAPON_ARRAY);
        data.weaponDefinitions = scanner.ScanPattern(Patterns::WEAPON_DEFINITIONS);
        data.gameMode = scanner.ScanPattern(Patterns::GAME_MODE);
        data.scoreboard = scanner.ScanPattern(Patterns::SCOREBOARD);
        data.hudElements = scanner.ScanPattern(Patterns::HUD_ELEMENTS);
        data.minimap = scanner.ScanPattern(Patterns::MINIMAP);
        data.netClient = scanner.ScanPattern(Patterns::NET_CLIENT);
        data.serverInfo = scanner.ScanPattern(Patterns::SERVER_INFO);
        data.physicsWorld = scanner.ScanPattern(Patterns::PHYSICS_WORLD);
        data.movementController = scanner.ScanPattern(Patterns::MOVEMENT_CONTROLLER);
        data.renderer = scanner.ScanPattern(Patterns::RENDERER);
        data.viewMatrix = scanner.ScanPattern(Patterns::VIEW_MATRIX);

        Log("\n=== RESULTS ===");
        Log("REF_DEF: 0x" + std::to_string(data.refDef));
        Log("CLIENT_INFO: 0x" + std::to_string(data.clientInfo));
        Log("LOCAL_INDEX: 0x" + std::to_string(data.localIndex));
        Log("NAME_ARRAY: 0x" + std::to_string(data.nameArray));
        Log("ENTITY_LIST: 0x" + std::to_string(data.entityList));
        Log("BONE_BASE: 0x" + std::to_string(data.boneBase));
        Log("CAMERA_BASE: 0x" + std::to_string(data.cameraBase));
        Log("WEAPON_ARRAY: 0x" + std::to_string(data.weaponArray));
        Log("WEAPON_DEFINITIONS: 0x" + std::to_string(data.weaponDefinitions));
        Log("GAME_MODE: 0x" + std::to_string(data.gameMode));
        Log("SCOREBOARD: 0x" + std::to_string(data.scoreboard));
        Log("HUD_ELEMENTS: 0x" + std::to_string(data.hudElements));
        Log("MINIMAP: 0x" + std::to_string(data.minimap));
        Log("NET_CLIENT: 0x" + std::to_string(data.netClient));
        Log("SERVER_INFO: 0x" + std::to_string(data.serverInfo));
        Log("PHYSICS_WORLD: 0x" + std::to_string(data.physicsWorld));
        Log("MOVEMENT_CONTROLLER: 0x" + std::to_string(data.movementController));
        Log("RENDERER: 0x" + std::to_string(data.renderer));
        Log("VIEW_MATRIX: 0x" + std::to_string(data.viewMatrix));
    }

    void DumpMemoryRegion(uintptr_t address, size_t size, const std::string& name) {
        if (!address) return;
        
        Log("[+] Dumping " + name + " at 0x" + std::to_string(address));
        
        std::vector<uint8_t> buffer(size);
        if (mem.Read(scanner.GetProcess(), address, buffer.data(), size)) {
            std::string filename = name + "_0x" + std::to_string(address) + ".bin";
            std::ofstream dumpFile(filename, std::ios::binary);
            if (dumpFile.is_open()) {
                dumpFile.write((char*)buffer.data(), size);
                dumpFile.close();
                Log("[+] Saved: " + filename);
            }
        }
    }

    void ExtractAllData() {
        Log("[+] === COMPREHENSIVE DATA EXTRACTION ===");
        
        // Dump key memory regions
        if (data.clientInfo) {
            DumpMemoryRegion(data.clientInfo, 0x1000, "client_info");
        }
        if (data.entityList) {
            DumpMemoryRegion(data.entityList, 0x10000, "entity_list");
        }
        if (data.weaponArray) {
            DumpMemoryRegion(data.weaponArray, 0x1000, "weapon_array");
        }
        if (data.nameArray) {
            DumpMemoryRegion(data.nameArray, 0x10000, "name_array");
        }
        
        Log("[+] Data extraction complete");
    }

    void GenerateHeaderFile() {
        Log("[+] === GENERATING OFFSET HEADER ===");
        
        std::ofstream header("mw2022_offsets.hpp");
        if (header.is_open()) {
            header << "// MW2022 Offsets - Auto-generated\n";
            header << "// Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
            header << "#pragma once\n\n";
            header << "namespace MW2022 {\n";
            header << "    constexpr uintptr_t REF_DEF = 0x" << std::hex << data.refDef << ";\n";
            header << "    constexpr uintptr_t CLIENT_INFO = 0x" << std::hex << data.clientInfo << ";\n";
            header << "    constexpr uintptr_t LOCAL_INDEX = 0x" << std::hex << data.localIndex << ";\n";
            header << "    constexpr uintptr_t NAME_ARRAY = 0x" << std::hex << data.nameArray << ";\n";
            header << "    constexpr uintptr_t ENTITY_LIST = 0x" << std::hex << data.entityList << ";\n";
            header << "    constexpr uintptr_t BONE_BASE = 0x" << std::hex << data.boneBase << ";\n";
            header << "    constexpr uintptr_t CAMERA_BASE = 0x" << std::hex << data.cameraBase << ";\n";
            header << "    constexpr uintptr_t WEAPON_ARRAY = 0x" << std::hex << data.weaponArray << ";\n";
            header << "    constexpr uintptr_t WEAPON_DEFINITIONS = 0x" << std::hex << data.weaponDefinitions << ";\n";
            header << "    constexpr uintptr_t GAME_MODE = 0x" << std::hex << data.gameMode << ";\n";
            header << "    constexpr uintptr_t SCOREBOARD = 0x" << std::hex << data.scoreboard << ";\n";
            header << "    constexpr uintptr_t HUD_ELEMENTS = 0x" << std::hex << data.hudElements << ";\n";
            header << "    constexpr uintptr_t MINIMAP = 0x" << std::hex << data.minimap << ";\n";
            header << "    constexpr uintptr_t NET_CLIENT = 0x" << std::hex << data.netClient << ";\n";
            header << "    constexpr uintptr_t SERVER_INFO = 0x" << std::hex << data.serverInfo << ";\n";
            header << "    constexpr uintptr_t PHYSICS_WORLD = 0x" << std::hex << data.physicsWorld << ";\n";
            header << "    constexpr uintptr_t MOVEMENT_CONTROLLER = 0x" << std::hex << data.movementController << ";\n";
            header << "    constexpr uintptr_t RENDERER = 0x" << std::hex << data.renderer << ";\n";
            header << "    constexpr uintptr_t VIEW_MATRIX = 0x" << std::hex << data.viewMatrix << ";\n";
            header << "}\n";
            header.close();
            Log("[+] Generated: mw2022_offsets.hpp");
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
    MW2022Extractor extractor;
    
    std::cout << "[+] MW2022 Comprehensive Data Extractor\n";
    std::cout << "[!] Update patterns in Patterns namespace with x64dbg\n\n";
    
    if (!extractor.Init()) {
        system("pause");
        return 1;
    }

    extractor.ScanAllPatterns();
    extractor.ExtractAllData();
    extractor.GenerateHeaderFile();
    
    std::cout << "\n[+] Extraction complete!\n";
    std::cout << "[!] Check the generated files for extracted data\n";
    
    extractor.Cleanup();
    system("pause");
    return 0;
}
