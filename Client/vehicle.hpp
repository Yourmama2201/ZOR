#pragma once
#include <string>
#include "memory.hpp"
#include "math.hpp"

struct Vehicle {
    uintptr_t ptr;
    Vec3 position;
    float health;
    float maxHealth;
    float fuel;
    float maxFuel;
    float speed;
    std::string name;
    bool isOccupied;
    int occupantCount;

    Vehicle(MemoryManager& mem, uintptr_t address) : ptr(address) {
        position = mem.Read<Vec3>(ptr + 0x50);
        health = mem.Read<float>(ptr + 0x100);
        maxHealth = mem.Read<float>(ptr + 0x104);
        fuel = mem.Read<float>(ptr + 0x108);
        maxFuel = mem.Read<float>(ptr + 0x10C);
        speed = mem.Read<float>(ptr + 0x110);
        name = mem.ReadString(ptr + 0x200, 32);
        isOccupied = mem.Read<uint8_t>(ptr + 0x300) != 0;
        occupantCount = mem.Read<int>(ptr + 0x304);
    }

    // Vehicle hacks applied externally
};

class VehicleHacks {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool speedBoost;
    bool godMode;
    float speedMultiplier;
    float maxVehicleHealth;

public:
    VehicleHacks(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), speedBoost(false), godMode(false),
        speedMultiplier(2.0f), maxVehicleHealth(9999.0f) {}

    void Run() {
        if (!mem || !gameBase) return;

        uintptr_t vehicleList = mem->Read<uintptr_t>(gameBase + 0x5000000);
        if (!vehicleList) return;

        for (int i = 0; i < 50; i++) {
            uintptr_t vPtr = mem->Read<uintptr_t>(vehicleList + (i * 0x8));
            if (!vPtr) continue;

            if (godMode) {
                mem->Write<float>(vPtr + 0x100, maxVehicleHealth);
            }

            if (speedBoost) {
                float currentSpeed = mem->Read<float>(vPtr + 0x110);
                mem->Write<float>(vPtr + 0x110, currentSpeed * speedMultiplier);
                // Also write to velocity
                Vec3 vel = mem->Read<Vec3>(vPtr + 0x60);
                vel.x *= speedMultiplier;
                vel.y *= speedMultiplier;
                vel.z *= speedMultiplier;
                mem->Write<Vec3>(vPtr + 0x60, vel);
            }
        }
    }

    void SetSpeedBoost(bool e) { speedBoost = e; }
    void SetGodMode(bool e) { godMode = e; }
    void SetSpeedMultiplier(float f) { speedMultiplier = f; }
};
