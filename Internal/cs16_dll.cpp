#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>

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
    float fov;
    Vector3 origin;
    Vector3 velocity;
    char name[32];
    int weaponID;
    bool isDormant;
};

class CS16Cheat {
private:
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
    bool speedEnabled = true;
    
    float aimbotFOV = 90.0f;
    float aimbotSmooth = 5.0f;
    int aimbotKey = VK_XBUTTON2;
    
    bool aimbotKeyPressed = false;
    bool menuOpen = false;

    Vector3 WorldToScreen(const Vector3& world, const Vector3& viewOrigin, const Vector3& viewAngles) {
        Vector3 delta = world - viewOrigin;
        
        float pitch = viewAngles.x * (3.14159f / 180.0f);
        float yaw = viewAngles.y * (3.14159f / 180.0f);
        
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

    Entity ReadEntity(uintptr_t address) {
        Entity ent;
        ent.address = address;
        ent.isDormant = true; // Default to dormant
        
        if (IsBadReadPtr((void*)address, 0x500)) return ent;
        
        ent.health = *(int*)(address + Offsets::Health);
        ent.team = *(int*)(address + Offsets::Team);
        ent.lifeState = *(int*)(address + Offsets::LifeState);
        ent.armor = *(int*)(address + Offsets::Armor);
        ent.hasHelmet = *(bool*)(address + Offsets::Helmet);
        ent.flashAlpha = *(float*)(address + Offsets::FlashAlpha);
        ent.smokeAlpha = *(float*)(address + Offsets::SmokeAlpha);
        ent.fov = *(float*)(address + Offsets::FOV);
        
        ent.origin = *(Vector3*)(address + Offsets::Origin);
        ent.velocity = *(Vector3*)(address + Offsets::Velocity);
        
        char nameBuffer[32] = {0};
        if (!IsBadReadPtr((void*)(address + Offsets::Name), 32)) {
            memcpy(nameBuffer, (void*)(address + Offsets::Name), 31);
            strncpy(ent.name, nameBuffer, 31);
        }
        
        ent.weaponID = *(int*)(address + Offsets::WeaponID);
        
        ent.isDormant = (ent.health <= 0 || ent.lifeState != 0);
        
        return ent;
    }

    void DrawESP(const std::vector<Entity>& entities, const Vector3& localOrigin, const Vector3& viewAngles) {
        if (!espEnabled || menuOpen) return;
        
        // Console ESP for DLL
        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter % 30 != 0) return; // Update every 30 frames
        
        system("cls");
        std::cout << "=== CS 1.6 DLL ESP ===" << std::endl;
        std::cout << "Entities: " << entities.size() << std::endl;
        std::cout << "====================" << std::endl;
        
        for (const auto& ent : entities) {
            if (ent.isDormant) continue;
            
            float distance = (ent.origin - localOrigin).Length();
            float velocity = ent.velocity.Length();
            
            std::cout << "[" << ent.name << "]" << std::endl;
            std::cout << "  HP: " << ent.health << "/" << 100 << " | Armor: " << ent.armor << (ent.hasHelmet ? " +H" : "") << std::endl;
            std::cout << "  Dist: " << (int)distance << "m | Vel: " << (int)velocity << " u/s" << std::endl;
            std::cout << "  Team: " << ent.team << " | Weapon: " << ent.weaponID << std::endl;
            std::cout << "  Flash: " << (int)(ent.flashAlpha * 100) << "% | Smoke: " << (int)(ent.smokeAlpha * 100) << "%" << std::endl;
            std::cout << "--------------------" << std::endl;
        }
    }

    void Aimbot(const std::vector<Entity>& entities, const Vector3& localOrigin, Vector3& viewAngles) {
        if (!aimbotEnabled || !aimbotKeyPressed || menuOpen) return;
        
        Entity bestTarget;
        float bestFOV = aimbotFOV;
        
        for (const auto& ent : entities) {
            if (ent.isDormant || ent.team == *(int*)(localPlayer + Offsets::Team)) continue;
            
            Vector3 delta = ent.origin - localOrigin;
            float distance = delta.Length();
            if (distance > 500.0f) continue;
            
            float yaw = atan2(delta.y, delta.x) * (180.0f / 3.14159f);
            float pitch = -atan2(delta.z, sqrt(delta.x * delta.x + delta.y * delta.y)) * (180.0f / 3.14159f);
            
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
            
            viewAngles.y += (targetYaw - viewAngles.y) / aimbotSmooth;
            viewAngles.x += (targetPitch - viewAngles.x) / aimbotSmooth;
            
            *(Vector3*)(moduleBase + Offsets::ViewAngles) = viewAngles;
        }
    }

    void Bunnyhop(uintptr_t localPlayerAddr) {
        if (!bunnyhopEnabled || menuOpen) return;
        
        int flags = *(int*)(localPlayerAddr + Offsets::Flags);
        if (flags & (1 << 0)) {
            // Send jump - would need input simulation
            // For DLL, we can use SendInput or write to jump address
        }
    }

    void NoFlash(uintptr_t localPlayerAddr) {
        if (!noFlashEnabled || menuOpen) return;
        
        float flashAlpha = *(float*)(localPlayerAddr + Offsets::FlashAlpha);
        if (flashAlpha > 0.0f) {
            *(float*)(localPlayerAddr + Offsets::FlashAlpha) = 0.0f;
        }
    }

    void NoSmoke(uintptr_t localPlayerAddr) {
        if (!radarEnabled || menuOpen) return;
        
        float smokeAlpha = *(float*)(localPlayerAddr + Offsets::SmokeAlpha);
        if (smokeAlpha > 0.0f) {
            *(float*)(localPlayerAddr + Offsets::SmokeAlpha) = 0.0f;
        }
    }

    void SpeedHack(uintptr_t localPlayerAddr) {
        if (!speedEnabled || menuOpen) return;
        
        Vector3 currentVelocity = *(Vector3*)(localPlayerAddr + Offsets::Velocity);
        if (currentVelocity.Length() > 0) {
            // Increase velocity
            currentVelocity.x *= 1.5f;
            currentVelocity.y *= 1.5f;
            *(Vector3*)(localPlayerAddr + Offsets::Velocity) = currentVelocity;
        }
    }

public:
    bool Init() {
        moduleBase = (uintptr_t)GetModuleHandleA("cstrike.exe");
        if (!moduleBase) {
            moduleBase = (uintptr_t)GetModuleHandleA("hl.exe");
        }
        
        if (!moduleBase) {
            return false;
        }

        entityList = moduleBase + Offsets::EntityList;
        localPlayer = moduleBase + Offsets::LocalPlayer;

        // Validate by checking if we can read the local player pointer
        if (IsBadReadPtr((void*)localPlayer, sizeof(uintptr_t))) {
            return false;
        }
        
        uintptr_t test = *(uintptr_t*)localPlayer;
        if (!test || IsBadReadPtr((void*)test, sizeof(uintptr_t))) {
            return false;
        }
        
        return true;
    }

    void Update() {
        if (!moduleBase) return;
        
        aimbotKeyPressed = (GetAsyncKeyState(aimbotKey) & 0x8000) != 0;
        
        if (IsBadReadPtr((void*)localPlayer, sizeof(uintptr_t))) return;
        uintptr_t localPlayerAddr = *(uintptr_t*)localPlayer;
        if (!localPlayerAddr || IsBadReadPtr((void*)localPlayerAddr, 0x100)) return;
        
        if (IsBadReadPtr((void*)(localPlayerAddr + Offsets::Origin), sizeof(Vector3))) return;
        Vector3 localOrigin = *(Vector3*)(localPlayerAddr + Offsets::Origin);
        
        if (IsBadReadPtr((void*)(moduleBase + Offsets::ViewAngles), sizeof(Vector3))) return;
        Vector3 viewAngles = *(Vector3*)(moduleBase + Offsets::ViewAngles);
        
        std::vector<Entity> entities;
        for (int i = 0; i < 32; i++) {
            if (IsBadReadPtr((void*)(entityList + i * 4), sizeof(uintptr_t))) continue;
            uintptr_t entityAddr = *(uintptr_t*)(entityList + i * 4);
            if (!entityAddr || IsBadReadPtr((void*)entityAddr, 0x100)) continue;
            
            Entity ent = ReadEntity(entityAddr);
            entities.push_back(ent);
        }
        
        DrawESP(entities, localOrigin, viewAngles);
        Aimbot(entities, localOrigin, viewAngles);
        Bunnyhop(localPlayerAddr);
        NoFlash(localPlayerAddr);
        NoSmoke(localPlayerAddr);
        SpeedHack(localPlayerAddr);
    }

    void ToggleESP() { espEnabled = !espEnabled; }
    void ToggleAimbot() { aimbotEnabled = !aimbotEnabled; }
    void ToggleBunnyhop() { bunnyhopEnabled = !bunnyhopEnabled; }
    void ToggleNoRecoil() { noRecoilEnabled = !noRecoilEnabled; }
    void ToggleRadar() { radarEnabled = !radarEnabled; }
    void ToggleTriggerbot() { triggerbotEnabled = !triggerbotEnabled; }
    void ToggleNoFlash() { noFlashEnabled = !noFlashEnabled; }
    void ToggleSpeed() { speedEnabled = !speedEnabled; }
    void ToggleMenu() { menuOpen = !menuOpen; }
    bool IsMenuOpen() { return menuOpen; }
};

CS16Cheat* g_cheat = nullptr;
HANDLE g_thread = nullptr;

DWORD WINAPI CheatThread(LPVOID param) {
    CS16Cheat* cheat = (CS16Cheat*)param;
    
    if (!cheat->Init()) {
        return 1;
    }
    
    while (true) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            cheat->ToggleMenu();
            
            if (cheat->IsMenuOpen()) {
                system("cls");
                std::cout << "=== CS 1.6 DLL Cheat Menu ===" << std::endl;
                std::cout << "[1] Toggle ESP" << std::endl;
                std::cout << "[2] Toggle Aimbot" << std::endl;
                std::cout << "[3] Toggle Bunnyhop" << std::endl;
                std::cout << "[4] Toggle No Recoil" << std::endl;
                std::cout << "[5] Toggle Radar + No Smoke" << std::endl;
                std::cout << "[6] Toggle Triggerbot" << std::endl;
                std::cout << "[7] Toggle No Flash" << std::endl;
                std::cout << "[8] Toggle Speed Hack" << std::endl;
                std::cout << "[9] Close Menu" << std::endl;
                std::cout << "============================" << std::endl;
                
                int choice;
                std::cin >> choice;
                
                switch (choice) {
                    case 1: cheat->ToggleESP(); break;
                    case 2: cheat->ToggleAimbot(); break;
                    case 3: cheat->ToggleBunnyhop(); break;
                    case 4: cheat->ToggleNoRecoil(); break;
                    case 5: cheat->ToggleRadar(); break;
                    case 6: cheat->ToggleTriggerbot(); break;
                    case 7: cheat->ToggleNoFlash(); break;
                    case 8: cheat->ToggleSpeed(); break;
                    case 9: cheat->ToggleMenu(); break;
                }
            }
        }
        
        cheat->Update();
        Sleep(1);
    }
    
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_cheat = new CS16Cheat();
        g_thread = CreateThread(nullptr, 0, CheatThread, g_cheat, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        if (g_thread) {
            TerminateThread(g_thread, 0);
            CloseHandle(g_thread);
        }
        if (g_cheat) {
            delete g_cheat;
        }
        break;
    }
    return TRUE;
}
