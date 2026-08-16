#pragma once
#include "../../memory.hpp"
#include "../../offsets.hpp"
#include "../../math.hpp"
#include "../../player.hpp"

class Movement {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool bhopEnabled;
    bool airStrafing;
    bool prevJumpState;
    bool jumpSpam;
    bool superSlide;
    bool superStrafe;
    DWORD lastSlideJump;
    DWORD lastStrafeTurn;

    void SetJumpBit(bool on) {
        uintptr_t cmdArray = mem->Read<uintptr_t>(gameBase + Offsets::CMD_ARRAY);
        if (!cmdArray) return;
        mem->Write<uint8_t>(cmdArray + 0x70, on ? 1 : 0);
    }

    void KeyKey(int vk, bool down) {
        INPUT inp = {};
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = (WORD)vk;
        if (!down) inp.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &inp, sizeof(INPUT));
    }

public:
    Movement(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), bhopEnabled(false),
        airStrafing(false),
        prevJumpState(false), jumpSpam(false), superSlide(false),
        superStrafe(false), lastSlideJump(0), lastStrafeTurn(0) {}

    void Run() {
        if (!mem || !gameBase) return;

        uintptr_t cmdArray = mem->Read<uintptr_t>(gameBase + Offsets::CMD_ARRAY);
        if (cmdArray) {
            bool isOnGround = mem->Read<uint8_t>(gameBase + 0x12C90) != 0;

            // Jump spam: force jump every 80ms while the bind is held (mouse scroll style)
            if (jumpSpam) {
                if (GetTickCount() - lastSlideJump > 80) {
                    SetJumpBit(true);
                    lastSlideJump = GetTickCount();
                }
            }
            else if (bhopEnabled) {
                bool isJumping = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                if (isJumping && isOnGround) SetJumpBit(true);
                else if (!isJumping && !isOnGround) SetJumpBit(false);
            }

            // Super slide: repeatedly micro-tap crouch (C) while sprinting forward
            if (superSlide) {
                bool movingForward = (GetAsyncKeyState('W') & 0x8000) != 0;
                bool sprinting = movingForward && !(GetAsyncKeyState(VK_CONTROL) & 0x8000);
                if (sprinting && GetTickCount() - lastSlideJump > 220) {
                    KeyKey('C', true);
                    Sleep(25);
                    KeyKey('C', false);
                    lastSlideJump = GetTickCount();
                }
            }

            // Super strafe: tap-turn while in air to build momentum
            if (superStrafe) {
                bool isInAir = mem->Read<uint8_t>(gameBase + 0x12C90) == 0;
                if (isInAir) {
                    bool left = (GetAsyncKeyState('A') & 0x8000) != 0;
                    bool right = (GetAsyncKeyState('D') & 0x8000) != 0;
                    mem->Write<float>(cmdArray + 0x7C, left ? -1.0f : (right ? 1.0f : 0.0f));
                    if ((left || right) && GetTickCount() - lastStrafeTurn > 30) {
                        Vec3 ang = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);
                        ang.y += left ? 6.0f : -6.0f;
                        mem->Write<Vec3>(gameBase + Offsets::VIEW_ANGLES, ang);
                        lastStrafeTurn = GetTickCount();
                    }
                }
            }
        }
    }

    void SetBHop(bool e) { bhopEnabled = e; }
    void SetAirStrafing(bool e) { airStrafing = e; }
    void SetJumpSpam(bool e) { jumpSpam = e; }
    void SetSuperSlide(bool e) { superSlide = e; }
    void SetSuperStrafe(bool e) { superStrafe = e; }
};
