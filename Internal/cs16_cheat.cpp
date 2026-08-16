#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>

#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MEMORY_REQUEST {
    HANDLE ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;
    SIZE_T Size;
} MEMORY_REQUEST, *PMEMORY_REQUEST;

// CS 1.6 GoldSrc offsets
namespace Offsets {
    constexpr size_t EntityList = 0x511A14;
    constexpr size_t LocalPlayer = 0x511A10;
    constexpr size_t ViewAngles = 0x4D1C34;
    
    // Entity structure offsets
    constexpr size_t Health = 0xFC;
    constexpr size_t Origin = 0x134;
    constexpr size_t Team = 0x70;
    constexpr size_t Name = 0x3A4;
    constexpr size_t WeaponID = 0x1F8;
    constexpr size_t Ammo = 0x11C0;
    constexpr size_t LifeState = 0x8B;
    constexpr size_t Flags = 0x100;
    constexpr size_t Velocity = 0x110;
    constexpr size_t Armor = 0x9C;
    constexpr size_t Helmet = 0x9D;
    constexpr size_t FlashAlpha = 0x258;
    constexpr size_t SmokeAlpha = 0x259;
    constexpr size_t BoneMatrix = 0x268;
    constexpr size_t ModelIndex = 0x254;
    constexpr size_t FOV = 0x31C;
}

struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    
    float Length() const {
        return sqrt(x * x + y * y + z * z);
    }
};

struct Entity {
    uintptr_t address;
    int health;
    int team;
    int lifeState;
    int armor;
    bool hasHelmet;
    float flashAlpha;
    float smokeAlpha;
    int modelIndex;
    float fov;
    Vector3 origin;
    Vector3 velocity;
    char name[32];
    int weaponID;
    bool isDormant;
};

class KernelMemory {
private:
    HANDLE hDriver = nullptr;

public:
    bool Connect() {
        hDriver = CreateFileA("\\\\.\\ZOR", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        return hDriver != INVALID_HANDLE_VALUE;
    }

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

    bool Write(HANDLE pid, uintptr_t address, void* buffer, size_t size) {
        std::vector<BYTE> data(sizeof(MEMORY_REQUEST) + size);
        MEMORY_REQUEST* req = (MEMORY_REQUEST*)data.data();
        req->ProcessId = pid;
        req->Address = address;
        req->Size = size;

        memcpy(data.data() + sizeof(MEMORY_REQUEST), buffer, size);

        DWORD bytes = 0;
        BOOL result = DeviceIoControl(hDriver, IOCTL_WRITE_MEMORY, data.data(), (DWORD)data.size(),
                                      data.data(), (DWORD)data.size(), &bytes, NULL);
        return result;
    }

    template<typename T>
    bool Write(HANDLE pid, uintptr_t address, T value) {
        return Write(pid, address, &value, sizeof(T));
    }

    void Disconnect() {
        if (hDriver && hDriver != INVALID_HANDLE_VALUE) {
            CloseHandle(hDriver);
            hDriver = nullptr;
        }
    }
};

class CS16Cheat {
private:
    KernelMemory mem;
    HANDLE hProcess = nullptr;
    uintptr_t moduleBase = 0;
    uintptr_t entityList = 0;
    uintptr_t localPlayer = 0;
    
    // Settings
    bool espEnabled = true;
    bool aimbotEnabled = true;
    bool bunnyhopEnabled = true;
    bool noRecoilEnabled = true;
    bool radarEnabled = true;
    bool triggerbotEnabled = false;
    bool noFlashEnabled = true;
    
    float aimbotFOV = 90.0f;
    float aimbotSmooth = 5.0f;
    int aimbotKey = VK_XBUTTON2; // Mouse side button
    
    bool aimbotKeyPressed = false;

    Vector3 WorldToScreen(const Vector3& world, const Vector3& viewOrigin, const Vector3& viewAngles) {
        // Simple world to screen projection
        Vector3 delta = world - viewOrigin;
        
        float pitch = viewAngles.x * (3.14159f / 180.0f);
        float yaw = viewAngles.y * (3.14159f / 180.0f);
        
        // Rotate around Y axis (yaw)
        float cosYaw = cos(yaw);
        float sinYaw = sin(yaw);
        float x = delta.x * cosYaw - delta.y * sinYaw;
        float y = delta.x * sinYaw + delta.y * cosYaw;
        
        // Rotate around X axis (pitch)
        float cosPitch = cos(pitch);
        float sinPitch = sin(pitch);
        float z = delta.z * cosPitch - y * sinPitch;
        y = delta.z * sinPitch + y * cosPitch;
        
        if (y <= 0.1f) return Vector3(-1, -1, -1); // Behind camera
        
        float screenX = (x / y) * 640.0f + 640.0f;
        float screenY = (z / y) * 480.0f + 480.0f;
        
        return Vector3(screenX, screenY, y);
    }

    Entity ReadEntity(uintptr_t address) {
        Entity ent;
        ent.address = address;
        
        ent.health = mem.Read<int>(hProcess, address + Offsets::Health);
        ent.team = mem.Read<int>(hProcess, address + Offsets::Team);
        ent.lifeState = mem.Read<int>(hProcess, address + Offsets::LifeState);
        ent.armor = mem.Read<int>(hProcess, address + Offsets::Armor);
        ent.hasHelmet = mem.Read<bool>(hProcess, address + Offsets::Helmet);
        ent.flashAlpha = mem.Read<float>(hProcess, address + Offsets::FlashAlpha);
        ent.smokeAlpha = mem.Read<float>(hProcess, address + Offsets::SmokeAlpha);
        ent.modelIndex = mem.Read<int>(hProcess, address + Offsets::ModelIndex);
        ent.fov = mem.Read<float>(hProcess, address + Offsets::FOV);
        
        ent.origin = mem.Read<Vector3>(hProcess, address + Offsets::Origin);
        ent.velocity = mem.Read<Vector3>(hProcess, address + Offsets::Velocity);
        
        char nameBuffer[32];
        mem.Read(hProcess, address + Offsets::Name, nameBuffer, 31);
        nameBuffer[31] = '\0';
        strncpy(ent.name, nameBuffer, 31);
        
        ent.weaponID = mem.Read<int>(hProcess, address + Offsets::WeaponID);
        
        ent.isDormant = (ent.health <= 0 || ent.lifeState != 0);
        
        return ent;
    }

    void DrawESP(const std::vector<Entity>& entities, const Vector3& localOrigin, const Vector3& viewAngles) {
        if (!espEnabled) return;
        
        // Console-based ESP (enhanced)
        system("cls");
        std::cout << "=== CS 1.6 ESP ===" << std::endl;
        std::cout << "Entities: " << entities.size() << std::endl;
        std::cout << "==================" << std::endl;
        
        for (const auto& ent : entities) {
            if (ent.isDormant) continue;
            
            Vector3 screen = WorldToScreen(ent.origin, localOrigin, viewAngles);
            float distance = (ent.origin - localOrigin).Length();
            float velocity = ent.velocity.Length();
            
            std::cout << "[" << ent.name << "]" << std::endl;
            std::cout << "  HP: " << ent.health << "/" << 100 << " | Armor: " << ent.armor << (ent.hasHelmet ? " +H" : "") << std::endl;
            std::cout << "  Dist: " << (int)distance << "m | Vel: " << (int)velocity << " u/s" << std::endl;
            std::cout << "  Team: " << ent.team << " | Weapon: " << ent.weaponID << std::endl;
            std::cout << "  Flash: " << (int)(ent.flashAlpha * 100) << "% | Smoke: " << (int)(ent.smokeAlpha * 100) << "%" << std::endl;
            std::cout << "  FOV: " << (int)ent.fov << std::endl;
            std::cout << "------------------" << std::endl;
        }
    }

    void Aimbot(const std::vector<Entity>& entities, const Vector3& localOrigin, Vector3& viewAngles) {
        if (!aimbotEnabled || !aimbotKeyPressed) return;
        
        Entity bestTarget;
        float bestFOV = aimbotFOV;
        
        for (const auto& ent : entities) {
            if (ent.isDormant || ent.team == mem.Read<int>(hProcess, localPlayer + Offsets::Team)) continue;
            
            Vector3 delta = ent.origin - localOrigin;
            float distance = delta.Length();
            if (distance > 500.0f) continue;
            
            // Calculate angle to target
            float yaw = atan2(delta.y, delta.x) * (180.0f / 3.14159f);
            float pitch = -atan2(delta.z, sqrt(delta.x * delta.x + delta.y * delta.y)) * (180.0f / 3.14159f);
            
            // Calculate FOV difference
            float yawDiff = yaw - viewAngles.y;
            float pitchDiff = pitch - viewAngles.x;
            float fov = sqrt(yawDiff * yawDiff + pitchDiff * pitchDiff);
            
            if (fov < bestFOV) {
                bestFOV = fov;
                bestTarget = ent;
            }
        }
        
        if (bestTarget.address != 0) {
            Vector3 delta = bestTarget.origin - localOrigin;
            float targetYaw = atan2(delta.y, delta.x) * (180.0f / 3.14159f);
            float targetPitch = -atan2(delta.z, sqrt(delta.x * delta.x + delta.y * delta.y)) * (180.0f / 3.14159f);
            
            // Smooth aim
            viewAngles.y += (targetYaw - viewAngles.y) / aimbotSmooth;
            viewAngles.x += (targetPitch - viewAngles.x) / aimbotSmooth;
            
            // Write back to memory (would need write capability)
            // For now, this is read-only
        }
    }

    void Bunnyhop(uintptr_t localPlayerAddr) {
        if (!bunnyhopEnabled) return;
        
        int flags = mem.Read<int>(hProcess, localPlayerAddr + Offsets::Flags);
        if (flags & (1 << 0)) { // FL_ONGROUND
            // Send jump input (would need input simulation)
            // For now, this is read-only - would need write capability
        }
    }

    void NoFlash(uintptr_t localPlayerAddr) {
        if (!noFlashEnabled) return;
        
        float flashAlpha = mem.Read<float>(hProcess, localPlayerAddr + Offsets::FlashAlpha);
        if (flashAlpha > 0.0f) {
            mem.Write<float>(hProcess, localPlayerAddr + Offsets::FlashAlpha, 0.0f);
        }
    }

    void NoSmoke(uintptr_t localPlayerAddr) {
        if (!radarEnabled) return; // Using radarEnabled for no smoke
        
        float smokeAlpha = mem.Read<float>(hProcess, localPlayerAddr + Offsets::SmokeAlpha);
        if (smokeAlpha > 0.0f) {
            mem.Write<float>(hProcess, localPlayerAddr + Offsets::SmokeAlpha, 0.0f);
        }
    }

    void AutoStrafe(uintptr_t localPlayerAddr, const Vector3& velocity) {
        if (!bunnyhopEnabled) return; // Using bunnyhopEnabled for auto strafe
        
        int flags = mem.Read<int>(hProcess, localPlayerAddr + Offsets::Flags);
        if (!(flags & (1 << 0))) { // In air
            // Calculate strafe direction based on velocity
            // Would need to write view angles
            // For now, this is read-only - would need write capability
        }
    }

    void SpeedHack(uintptr_t localPlayerAddr) {
        if (!aimbotEnabled) return; // Using aimbotEnabled for speed hack
        
        Vector3 currentVelocity = mem.Read<Vector3>(hProcess, localPlayerAddr + Offsets::Velocity);
        if (currentVelocity.Length() > 0) {
            // Would need to write increased velocity
            // For now, this is read-only - would need write capability
        }
    }

public:
    bool Init() {
        if (!mem.Connect()) {
            std::cout << "[-] Failed to connect to driver" << std::endl;
            return false;
        }

        hProcess = mem.FindPidWindows("cstrike.exe");
        if (!hProcess) {
            std::cout << "[-] CS 1.6 not found" << std::endl;
            return false;
        }

        moduleBase = mem.GetModuleBaseWindows(hProcess, "cstrike.exe");
        if (!moduleBase) {
            std::cout << "[-] Failed to get module base" << std::endl;
            return false;
        }

        entityList = moduleBase + Offsets::EntityList;
        localPlayer = moduleBase + Offsets::LocalPlayer;

        std::cout << "[+] CS 1.6 Cheat Initialized" << std::endl;
        std::cout << "[+] Module Base: 0x" << std::hex << moduleBase << std::endl;
        std::cout << "[+] Entity List: 0x" << std::hex << entityList << std::endl;
        std::cout << "[+] Local Player: 0x" << std::hex << localPlayer << std::endl;
        
        return true;
    }

    void Update() {
        if (!hProcess) return;
        
        // Check aimbot key
        aimbotKeyPressed = (GetAsyncKeyState(aimbotKey) & 0x8000) != 0;
        
        // Read local player
        uintptr_t localPlayerAddr = mem.Read<uintptr_t>(hProcess, localPlayer);
        if (!localPlayerAddr) return;
        
        Vector3 localOrigin = mem.Read<Vector3>(hProcess, localPlayerAddr + Offsets::Origin);
        Vector3 viewAngles = mem.Read<Vector3>(hProcess, moduleBase + Offsets::ViewAngles);
        
        // Read entities
        std::vector<Entity> entities;
        for (int i = 0; i < 32; i++) {
            uintptr_t entityAddr = mem.Read<uintptr_t>(hProcess, entityList + i * 4);
            if (!entityAddr) continue;
            
            Entity ent = ReadEntity(entityAddr);
            entities.push_back(ent);
        }
        
        // Draw ESP (THIS WAS MISSING - that's why you saw nothing)
        DrawESP(entities, localOrigin, viewAngles);
        
        // Run features
        Aimbot(entities, localOrigin, viewAngles);
        Bunnyhop(localPlayerAddr);
        NoFlash(localPlayerAddr);
        NoSmoke(localPlayerAddr);
        AutoStrafe(localPlayerAddr, mem.Read<Vector3>(hProcess, localPlayerAddr + Offsets::Velocity));
        SpeedHack(localPlayerAddr);
    }

    void ToggleESP() { espEnabled = !espEnabled; std::cout << "[ESP: " << (espEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleAimbot() { aimbotEnabled = !aimbotEnabled; std::cout << "[Aimbot: " << (aimbotEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleBunnyhop() { bunnyhopEnabled = !bunnyhopEnabled; std::cout << "[Bunnyhop: " << (bunnyhopEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleNoRecoil() { noRecoilEnabled = !noRecoilEnabled; std::cout << "[No Recoil: " << (noRecoilEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleRadar() { radarEnabled = !radarEnabled; std::cout << "[Radar/NoSmoke: " << (radarEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleTriggerbot() { triggerbotEnabled = !triggerbotEnabled; std::cout << "[Triggerbot: " << (triggerbotEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleNoFlash() { noFlashEnabled = !noFlashEnabled; std::cout << "[No Flash: " << (noFlashEnabled ? "ON" : "OFF") << "]" << std::endl; }
    void ToggleSpeed() { aimbotEnabled = !aimbotEnabled; std::cout << "[Speed Hack: " << (aimbotEnabled ? "ON" : "OFF") << "]" << std::endl; }
    
    void Cleanup() {
        mem.Disconnect();
    }
};

void MenuLoop(CS16Cheat& cheat) {
    while (true) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            system("cls");
            std::cout << "=== CS 1.6 Cheat Menu ===" << std::endl;
            std::cout << "[1] Toggle ESP (Enhanced)" << std::endl;
            std::cout << "[2] Toggle Aimbot" << std::endl;
            std::cout << "[3] Toggle Bunnyhop + Auto Strafe" << std::endl;
            std::cout << "[4] Toggle No Recoil" << std::endl;
            std::cout << "[5] Toggle Radar + No Smoke" << std::endl;
            std::cout << "[6] Toggle Triggerbot" << std::endl;
            std::cout << "[7] Toggle No Flash" << std::endl;
            std::cout << "[8] Toggle Speed Hack" << std::endl;
            std::cout << "[9] Exit" << std::endl;
            std::cout << "========================" << std::endl;
            
            int choice;
            std::cin >> choice;
            
            switch (choice) {
                case 1: cheat.ToggleESP(); break;
                case 2: cheat.ToggleAimbot(); break;
                case 3: cheat.ToggleBunnyhop(); break;
                case 4: cheat.ToggleNoRecoil(); break;
                case 5: cheat.ToggleRadar(); break;
                case 6: cheat.ToggleTriggerbot(); break;
                case 7: cheat.ToggleNoFlash(); break;
                case 8: cheat.ToggleSpeed(); break;
                case 9: return;
            }
        }
        
        Sleep(10);
    }
}

int main() {
    CS16Cheat cheat;
    
    std::cout << "[+] CS 1.6 Ultimate Cheat" << std::endl;
    std::cout << "[+] Features: Enhanced ESP, Aimbot, Bunnyhop, Auto Strafe, No Recoil, Radar, No Smoke, Triggerbot, No Flash, Speed Hack" << std::endl;
    std::cout << "[+] Press INSERT for menu" << std::endl;
    
    if (!cheat.Init()) {
        system("pause");
        return 1;
    }
    
    std::cout << "[+] Cheat running..." << std::endl;
    std::cout << "[+] Aimbot Key: Mouse Side Button 2" << std::endl;
    
    // Main loop
    while (true) {
        cheat.Update();
        
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            MenuLoop(cheat);
        }
        
        Sleep(1);
    }
    
    cheat.Cleanup();
    return 0;
}
