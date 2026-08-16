#pragma once
#include "../../memory.hpp"
#include "../../math.hpp"
#include "../../offsets.hpp"

class Camera {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool thirdPerson;
    float thirdPersonDistance;
    bool fovChanger;
    float customFOV;
    bool freeCam;
    Vec3 freeCamOffset;
    bool noWeapon;
    float cl_thirdPerson;
    float lerpSpeed;

public:
    Camera(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false), thirdPerson(false),
        thirdPersonDistance(80.0f), fovChanger(false), customFOV(90.0f),
        freeCam(false), freeCamOffset(Vec3()), noWeapon(false),
        cl_thirdPerson(0.0f), lerpSpeed(0.15f) {}

    void Run() {
        if (!enabled || !mem) return;

        uintptr_t cameraBase = mem->Read<uintptr_t>(gameBase + Offsets::CAMERA_BASE);
        if (!cameraBase) return;

        Vec3 cameraPos = mem->Read<Vec3>(cameraBase + Offsets::CAMERA_POS);

        if (thirdPerson) {
            Vec3 viewAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);
            float yawRad = viewAngles.y * (3.14159265f / 180.0f);
            float pitchRad = viewAngles.x * (3.14159265f / 180.0f);

            Vec3 offset;
            offset.x = -sin(yawRad) * cos(pitchRad) * thirdPersonDistance;
            offset.y = cos(yawRad) * cos(pitchRad) * thirdPersonDistance;
            offset.z = -sin(pitchRad) * thirdPersonDistance * 0.5f;

            Vec3 newCamPos = cameraPos + offset;
            mem->Write<Vec3>(cameraBase + Offsets::CAMERA_POS, newCamPos);
            cameraPos = newCamPos;
        }

        if (fovChanger) {
            mem->Write<float>(cameraBase + 0x120, customFOV);
            mem->Write<float>(cameraBase + 0x124, customFOV);
        }

        if (freeCam) {
            Vec3 newPos = cameraPos + freeCamOffset;
            Vec3 cur = mem->Read<Vec3>(cameraBase + Offsets::CAMERA_POS);
            Vec3 smoothed = cur.Lerp(newPos, lerpSpeed);
            mem->Write<Vec3>(cameraBase + Offsets::CAMERA_POS, smoothed);
        }

        if (noWeapon) {
            mem->Write<uint8_t>(cameraBase + 0x1F0, 1);
        }
    }

    void ToggleThirdPerson() {
        thirdPerson = !thirdPerson;
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetThirdPerson(bool t) { thirdPerson = t; }
    void SetThirdPersonDistance(float d) { thirdPersonDistance = d; }
    void SetFOVChanger(bool f) { fovChanger = f; }
    void SetCustomFOV(float f) { customFOV = f; }
    void SetFreeCam(bool f) { freeCam = f; }
    void SetFreeCamOffset(Vec3 o) { freeCamOffset = o; }
};
