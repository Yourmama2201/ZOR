#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <random>
#include <unordered_map>
#include <windows.h>
#include "player.hpp"
#include "offsets.hpp"
#include "memory.hpp"

static std::mt19937 g_rng(std::random_device{}());

struct Particle { ImVec2 pos, vel; float life, maxLife, size; ImColor color; };

class Menu {
private:
    bool open; int activeTab; float openAnim, tabAnim, time;
    std::vector<Particle> particles; float particleTimer;
    std::unordered_map<std::string, bool> sectionOpen;
    char searchBuf[64];
    ImVec4 accent;
    float *accentR, *accentG, *accentB;

    const std::vector<Player>* playersRef = nullptr;
    int localTeam = 0;
    std::string lastReported;
    float reportFlash = 0.0f;

    void CopyToClipboard(const std::string& text) {
        if (!OpenClipboard(NULL)) return;
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hg) {
            memcpy(GlobalLock(hg), text.c_str(), text.size() + 1);
            GlobalUnlock(hg);
            SetClipboardData(CF_TEXT, hg);
        }
        CloseClipboard();
    }

    void LeaveMatch() {
        INPUT inp;
        memset(&inp, 0, sizeof(inp));
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = VK_ESCAPE;
        SendInput(1, &inp, sizeof(INPUT));
        memset(&inp, 0, sizeof(inp));
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = VK_ESCAPE;
        inp.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &inp, sizeof(INPUT));
    }

    bool* aimbotEnabled, *silentAimEnabled, *triggerbotEnabled, *triggerAdvanced;
    bool* headshotOnly, *wallBang; float *aimFOV, *aimSmooth; int *targetBone;
    float *silentFOV; bool *silentHeadshot; int *triggerDelay;
    bool *triggerAutoShoot, *drawFovCircle, *bulletTracking;
    int *aimMode;
    bool *stickToTarget, *autoWall, *bestBone, *boneOverride;
    int *boneOverrideKey, *boneOverrideId;
    float *headOffset;
    bool *aimVisibleOnly, *riotShieldBypass;
    int *shieldBone;
    bool *autoScope, *zoomFOV;
    float *zoomFactor;
    bool *autoShoot, *recoilComp;
    float *recoilStrength;

    bool *espOverlay, *wallhack, *vehicleESP, *lootESP;
    bool *espBoxes, *espSnaplines, *espHealth, *espNames, *espDistance, *espHeadCircle;
    bool *espArmor, *espBoxFill, *espTextBackground, *espDistanceFade;
    int *espBoxStyle;
    bool *skeletonEnabled, *skeletonBones, *skeletonJoints;
    bool *exfilESP, *contractESP, *deadBodyESP, *strongholdESP, *buyStationESP;
    bool *samSiteESP, *supplyDropESP, *bossESP;
    bool *worldHUD, *drawWatermark, *drawCompass, *drawGameMode, *drawTimer;
    bool *visibleOnly, *visibilityIndicator, *grenadePrediction;
    bool *weaponNames, *squadCount, *itemRarity;

    bool *noRecoil, *noSpread, *rapidFire; int *rapidFireDelay;
    bool *infiniteAmmo, *instantSwap, *autoFire, *instantReload;
    int *weaponAmmoMod;
    bool *fastReload; float *reloadSpeed;

    bool *radarEnabled, *radarRotate; float *radarRange, *radarSize, *radarOpacity;
    bool *radarShowEnemies, *radarShowTeammates, *radarShowVehicles, *radarShowAI;

    bool *bhopEnabled, *jumpSpam, *superSlide, *superStrafe, *airStrafing;

    bool *antiAimEnabled; int *antiAimMode; float *spinSpeed;
    int *jitterRange;

    bool *thirdPerson; float *thirdPersonDist;
    bool *fovChanger; float *customFOV;
    bool *nightVision, *thermalVision, *freeCam;

    bool *stealthEnabled, *antiDebug, *blacklistScan;
    bool *crosshairEnabled; int *crosshairType;
    float *crosshairSize, *crosshairThickness, *crosshairGap;
    bool *crosshairOutline;
    float *crosshairColor;
    float *fovCircleColor, *fovCircleThickness;
    bool *playerListOpen;
    bool keybindEditorOpen;

    bool *vehicleSpeedBoost, *vehicleGodMode; float *vehicleSpeedMult;

    bool *nameSpoofEnabled, *bypassProfanity;
    bool *spectatorEnabled, *spectatorAutoCloak;
    bool *accountHealthEnabled;
    bool *lootFilterEnabled, *autoPickupEnabled;
    bool *soundESPEnabled, *soundESPFootsteps, *soundESPGunshots, *soundESPExplosions, *soundESPVehicles;
    char *nameSpoofBuffer, *clanTagBuffer;
    bool *sessionTimerEnabled;

    std::function<void()> onSaveConfig, onLoadConfig, onResetConfig;
    std::function<std::vector<std::string>()> onListProfiles;
    std::function<void(const char*)> onLoadProfile, onSaveProfile, onDeleteProfile;
    std::function<const char*()> onCurrentProfile;

    void ApplyStyle() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 12.0f; s.FrameRounding = 8.0f; s.GrabRounding = 8.0f;
        s.PopupRounding = 8.0f; s.ScrollbarRounding = 8.0f; s.ChildRounding = 8.0f;
        s.WindowTitleAlign = ImVec2(0.5f, 0.5f); s.FramePadding = ImVec2(10, 5);
        s.ItemSpacing = ImVec2(10, 6); s.WindowPadding = ImVec2(14, 14);
        s.ScrollbarSize = 10.0f; s.GrabMinSize = 8.0f;

        ImVec4 a(accent.x, accent.y, accent.z, 1.0f), aH(accent.x, accent.y, accent.z, 0.80f);
        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg] = ImVec4(0.06f,0.06f,0.10f,0.95f);
        c[ImGuiCol_ChildBg] = ImVec4(0.08f,0.08f,0.12f,0.80f);
        c[ImGuiCol_PopupBg] = ImVec4(0.08f,0.08f,0.12f,0.95f);
        c[ImGuiCol_ScrollbarBg] = ImVec4(0.08f,0.08f,0.12f,0.60f);
        c[ImGuiCol_ScrollbarGrab] = a; c[ImGuiCol_ScrollbarGrabHovered] = aH; c[ImGuiCol_ScrollbarGrabActive] = a;
        c[ImGuiCol_CheckMark] = ImVec4(0.00f,0.85f,0.20f,1.00f);
        c[ImGuiCol_SliderGrab] = a; c[ImGuiCol_SliderGrabActive] = a;
        c[ImGuiCol_Button] = ImVec4(0.15f,0.15f,0.22f,0.80f);
        c[ImGuiCol_ButtonHovered] = ImVec4(accent.x,accent.y,accent.z,0.30f);
        c[ImGuiCol_ButtonActive] = ImVec4(accent.x,accent.y,accent.z,0.50f);
        c[ImGuiCol_Header] = ImVec4(accent.x,accent.y,accent.z,0.25f);
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x,accent.y,accent.z,0.40f);
        c[ImGuiCol_HeaderActive] = ImVec4(accent.x,accent.y,accent.z,0.60f);
        c[ImGuiCol_Tab] = ImVec4(0.10f,0.10f,0.15f,0.85f);
        c[ImGuiCol_TabHovered] = ImVec4(accent.x,accent.y,accent.z,0.25f);
        c[ImGuiCol_TabActive] = ImVec4(accent.x,accent.y,accent.z,0.35f);
        c[ImGuiCol_Text] = ImVec4(0.92f,0.92f,0.96f,1.00f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f,0.50f,0.55f,1.00f);
        c[ImGuiCol_Separator] = ImVec4(0.20f,0.20f,0.28f,0.60f);
        c[ImGuiCol_Border] = ImVec4(0.20f,0.20f,0.28f,0.50f);
        c[ImGuiCol_FrameBg] = ImVec4(0.10f,0.10f,0.16f,0.80f);
        c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f,0.15f,0.22f,0.80f);
        c[ImGuiCol_FrameBgActive] = ImVec4(0.18f,0.18f,0.26f,0.80f);
        c[ImGuiCol_PlotHistogram] = a;
    }

    void TabBtn(const char* label, const char* icon, int idx, int count) {
        bool sel = idx == activeTab;
        std::string full = std::string(icon) + " " + label;
        float w = ImGui::GetContentRegionAvail().x / count;
        ImColor bg = sel ? ImColor(accent.x,accent.y,accent.z,0.28f) : ImColor(0.0f,0.0f,0.0f,0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(accent.x,accent.y,accent.z,0.20f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(accent.x,accent.y,accent.z,0.32f));
        if (ImGui::Button(full.c_str(), ImVec2(w, 34))) { activeTab = idx; tabAnim = 0; }
        ImGui::PopStyleColor(3);
        if (idx < count - 1) ImGui::SameLine();
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        if (sel) {
            d->AddRectFilled(ImVec2(mn.x+2,mx.y-2), ImVec2(mx.x-2,mx.y), ImColor(accent.x,accent.y,accent.z,0.95f), 1.0f);
            d->AddRect(ImVec2(mn.x,mn.y), ImVec2(mx.x,mx.y), ImColor(accent.x,accent.y,accent.z,0.15f), 3.0f);
        }
    }

    void Sec(const char* l) {
        ImGui::Spacing();
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 c = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        d->AddRectFilled(c, ImVec2(c.x+3, c.y+16), ImColor(accent.x,accent.y,accent.z,0.95f), 1.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+8);
        ImGui::TextColored(ImVec4(accent.x,accent.y,accent.z,0.95f), l);
        ImGui::Separator();
    }
    bool Sub(const char* l) {
        auto it = sectionOpen.find(l);
        if (it == sectionOpen.end()) it = sectionOpen.emplace(l, true).first;
        bool* openPtr = &it->second;
        ImGui::TextColored(ImVec4(0.70f,0.70f,0.80f,1.00f), l);
        ImGui::SameLine();
        ImGui::TextDisabled(*openPtr ? "[ - ]" : "[ + ]");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24);
        ImGui::InvisibleButton(("##sub_"+std::string(l)).c_str(), ImVec2(20, 14));
        if (ImGui::IsItemClicked(0)) *openPtr = !*openPtr;
        bool open = *openPtr;
        if (open) ImGui::Indent(10.0f);
        return open;
    }
    void End(bool open = true) { if (open) ImGui::Unindent(10.0f); ImGui::Spacing(); }
    void T(const char* l, bool* v, const char* tip = nullptr) {
        Switch(l, v);
        if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    }
    void TL(const char* l, bool* v, const char* tip = nullptr) {
        Switch(l, v);
        if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::SameLine();
    }
    // Colored switch/pill toggle instead of a plain checkbox
    void Switch(const char* l, bool* v) {
        ImGui::PushID(l);
        float x = ImGui::GetCursorPosX();
        ImVec2 sz(28, 16);
        bool on = *v;
        ImVec2 mn = ImGui::GetCursorScreenPos();
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImU32 track = on ? ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x,accent.y,accent.z,0.85f))
                         : ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f,0.18f,0.26f,0.95f));
        d->AddRectFilled(mn, ImVec2(mn.x+sz.x, mn.y+sz.y), track, 8.0f);
        float pad = 2.0f, k = sz.y - pad*2;
        ImVec2 kb = on ? ImVec2(mn.x+sz.x-pad-k, mn.y+pad) : ImVec2(mn.x+pad, mn.y+pad);
        d->AddCircleFilled(ImVec2(kb.x+k/2, kb.y+k/2), k/2, ImGui::ColorConvertFloat4ToU32(ImVec4(1,1,1,0.95f)));
        ImGui::InvisibleButton("", sz);
        if (ImGui::IsItemClicked(0)) *v = !*v;
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::TextUnformatted(l);
    }

    void TabAimbot() {
        if (Sub("Aimbot")) {
            T("Enable", aimbotEnabled, "Toggle aim assist targeting");
            if (*aimbotEnabled) {
                ImGui::SliderFloat("FOV", aimFOV, 0.5f, 90.0f, "%.1f");
                ImGui::SliderFloat("Smoothness", aimSmooth, 0.01f, 0.50f, "%.3f");
                TL("Headshot Only", headshotOnly, "Only lock onto the head bone"); T("Wall Bang", wallBang, "Allow targets through walls");
                ImGui::Combo("Target Mode", aimMode, "All\0Players Only\0AI Only\0Boss Priority\0");
                if (!*headshotOnly) {
                    int boneMap[6] = { Offsets::ROOT, Offsets::NECK, Offsets::SPINE1, Offsets::HEAD, Offsets::LEFT_KNEE, Offsets::RIGHT_KNEE };
                    static const char* boneNames = "Feet\0Neck\0Chest\0Head\0Left Knee\0Right Knee\0";
                    int boneIdx = 0;
                    for (int i = 0; i < 6; i++) if (*targetBone == boneMap[i]) { boneIdx = i; break; }
                    if (ImGui::Combo("Target Bone", &boneIdx, boneNames)) *targetBone = boneMap[boneIdx];
                }
                TL("Bullet Tracking", bulletTracking, "Predict target movement"); T("Draw FOV Circle", drawFovCircle, "Show aim radius on screen");
                ImGui::SliderFloat("Head Offset", headOffset, -2.0f, 2.0f, "%.2f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ghost Rook skin fix: nudge head aim up/down to hit the real hitbox");
                if (*drawFovCircle && fovCircleColor) {
                    ImGui::SliderFloat("Circle Thickness", fovCircleThickness, 0.5f, 4.0f, "%.1f");
                    ImGui::ColorEdit4("Circle Color", fovCircleColor);
                }
                TL("Best Bone", bestBone, "Auto-pick bone by distance (head close, chest far)");
                TL("Stick to Target", stickToTarget, "Keep the same target until it's gone");
                TL("Visible Only", aimVisibleOnly, "Only aim at players that are on screen");
                TL("Auto Wall", autoWall, "Aim at targets through walls when no visible target is in FOV");
                TL("Riot Shield Bypass", riotShieldBypass, "Aim at legs on shield users (shield blocks chest/head)");
                if (*riotShieldBypass) {
                    int boneMap3[6] = { Offsets::ROOT, Offsets::NECK, Offsets::SPINE1, Offsets::HEAD, Offsets::LEFT_KNEE, Offsets::RIGHT_KNEE };
                    static const char* boneNames3 = "Feet\0Neck\0Chest\0Head\0Left Knee\0Right Knee\0";
                    int boneIdx3 = 0;
                    for (int i = 0; i < 6; i++) if (*shieldBone == boneMap3[i]) { boneIdx3 = i; break; }
                    if (ImGui::Combo("Shield Aim Bone", &boneIdx3, boneNames3)) *shieldBone = boneMap3[boneIdx3];
                }
                TL("Bone Override", boneOverride, "Hold a key to force a specific bone");
                if (*boneOverride) {
                    ImGui::SliderInt("Bone Override Key", boneOverrideKey, 1, 255, "VK %d");
                    int boneMap2[6] = { Offsets::ROOT, Offsets::NECK, Offsets::SPINE1, Offsets::HEAD, Offsets::LEFT_KNEE, Offsets::RIGHT_KNEE };
                    static const char* boneNames2 = "Feet\0Neck\0Chest\0Head\0Left Knee\0Right Knee\0";
                    int boneIdx2 = 0;
                    for (int i = 0; i < 6; i++) if (*boneOverrideId == boneMap2[i]) { boneIdx2 = i; break; }
                    if (ImGui::Combo("Override Bone", &boneIdx2, boneNames2)) *boneOverrideId = boneMap2[boneIdx2];
                }
                TL("Auto Scope", autoScope, "ADS while a target is locked");
                TL("Zoom FOV", zoomFOV, "Shrink aim FOV while scoped for precision");
                if (*zoomFOV) ImGui::SliderFloat("Zoom Factor", zoomFactor, 0.1f, 0.9f, "%.2f");
                TL("Auto Shoot", autoShoot, "Fire when your crosshair is on target");
                TL("Recoil Compensation", recoilComp, "Pull view down to counter weapon kick");
                if (*recoilComp) ImGui::SliderFloat("Recoil Strength", recoilStrength, 0.0f, 5.0f, "%.2f");
            } End();
        }
        if (Sub("Silent Aim")) {
            T("Enable", silentAimEnabled, "Redirect bullets without moving your view");
            if (*silentAimEnabled) { ImGui::SliderFloat("FOV", silentFOV, 0.5f, 30.0f, "%.1f"); T("Headshot Only", silentHeadshot, "Only redirect to the head"); } End();
        }
        if (Sub("Triggerbot")) {
            TL("Basic", triggerbotEnabled, "Shoot when crosshair is on target"); T("Advanced", triggerAdvanced, "Extra triggerbot options");
            if (*triggerAdvanced) { ImGui::SliderInt("Delay", triggerDelay, 0, 200); T("Auto Shoot", triggerAutoShoot, "Fire automatically when locked"); } End();
        }
    }

    void TabVisuals() {
        if (Sub("ESP")) {
            TL("ESP", espOverlay, "Master ESP switch"); TL("Wallhack", wallhack, "See enemies through walls"); T("Visible Only", visibleOnly, "Only ESP visible targets"); ImGui::Spacing();
            TL("Boxes", espBoxes, "Draw boxes around players");
            if (*espBoxes) { ImGui::Indent(); ImGui::Combo("Style", espBoxStyle, "2D Box\0Corner Box\0Both\0"); T("Fill", espBoxFill, "Subtle translucent fill"); ImGui::Unindent(); }
            TL("Snaplines", espSnaplines, "Draw lines to players"); TL("Health", espHealth, "Show health bars"); TL("Armor", espArmor, "Show armor bars");
            TL("Names", espNames, "Show player names"); TL("Distance", espDistance, "Show distance to player"); T("Head Circle", espHeadCircle, "Circle on head");
            T("Text Background", espTextBackground, "Dark pill behind text"); T("Distance Fade", espDistanceFade, "Fade ESP far away"); ImGui::Spacing();
            TL("Loot", lootESP, "ESP for ground loot"); TL("Vehicles", vehicleESP, "ESP for vehicles"); T("Skeleton", skeletonEnabled, "Draw bone skeletons");
            if (*skeletonEnabled) { ImGui::Indent(); TL("Bones", skeletonBones); T("Joints", skeletonJoints); ImGui::Unindent(); }
            TL("Weapon Names", weaponNames, "Show held weapon"); TL("Squad Count", squadCount, "Show squad size"); T("Item Rarity", itemRarity, "Color loot by rarity");
            End();
        }
        if (Sub("World")) {
            TL("Exfils", exfilESP); TL("Contracts", contractESP); T("Dead Bodies", deadBodyESP);
            TL("Buy Stations", buyStationESP); TL("Strongholds", strongholdESP); T("Boss Locations", bossESP);
            TL("SAM Sites", samSiteESP); T("Supply Drops", supplyDropESP);
            End();
        }
        if (Sub("HUD")) {
            T("World HUD", worldHUD);
            if (*worldHUD) { ImGui::Indent(); TL("Watermark", drawWatermark); TL("Game Mode", drawGameMode); TL("Compass", drawCompass); T("Timer", drawTimer); ImGui::Unindent(); } End();
        }
        if (Sub("Overlays")) {
            TL("Visibility Indicator", visibilityIndicator); T("Grenade Prediction", grenadePrediction); End();
        }
        if (Sub("Sound ESP")) {
            T("Sound ESP", soundESPEnabled, "Show directional sound markers");
            if (*soundESPEnabled) { ImGui::Indent(); TL("Footsteps", soundESPFootsteps); TL("Gunshots", soundESPGunshots); TL("Explosions", soundESPExplosions); T("Vehicles", soundESPVehicles); ImGui::Unindent(); } End();
        }
    }

    void TabWeapons() {
        if (Sub("Stabilization")) { TL("No Recoil", noRecoil, "Eliminate weapon recoil"); T("No Spread", noSpread, "Eliminate bullet spread"); End(); }
        if (Sub("Fire Rate")) {
            T("Rapid Fire", rapidFire, "Increase fire rate");
            if (*rapidFire) ImGui::SliderInt("Delay", rapidFireDelay, 1, 100);
            End();
        }
        if (Sub("Ammunition")) {
            T("Fast Reload", fastReload, "Speed up reload times");
            if (*fastReload && reloadSpeed) ImGui::SliderFloat("Reload Speed", reloadSpeed, 0.05f, 1.0f, "%.2f");
            T("Infinite Ammo", infiniteAmmo, "Never run out of ammo");
            End();
        }
    }

    void TabRadar() {
        if (Sub("Radar")) {
            T("Enable Radar", radarEnabled, "Draw a minimap of nearby entities");
            if (*radarEnabled) {
                ImGui::SliderFloat("Range", radarRange, 50.0f, 1000.0f, "%.0f");
                ImGui::SliderFloat("Size", radarSize, 100.0f, 400.0f, "%.0f");
                ImGui::SliderFloat("Opacity", radarOpacity, 0.1f, 1.0f, "%.2f");
                T("Rotate with Player", radarRotate, "Rotate radar to face your aim direction"); ImGui::Separator();
                TL("Enemies", radarShowEnemies); TL("Teammates", radarShowTeammates);
                TL("Vehicles", radarShowVehicles); T("Show AI", radarShowAI);
            }
            End();
        }
    }

    void TabMovement() {
        if (Sub("Movement")) {
            TL("Bunny Hop", bhopEnabled, "Auto-jump while holding space on landing");
            TL("Jump Spam", jumpSpam, "Auto-jump every 80ms (rocket jump spam)");
            TL("Super Slide", superSlide, "Auto-micro-tap crouch while sprinting forward");
            TL("Super Strafe", superStrafe, "Auto-turn in air with A/D to build momentum");
            T("Air Strafe", airStrafing, "Full air control while in the air");
            End();
        }
    }

    void TabAntiAim() {
        if (Sub("Anti-Aim")) {
            T("Enable Anti-Aim", antiAimEnabled, "Right-click toggles anti-aim on/off");
            if (*antiAimEnabled) {
                ImGui::Combo("Mode", antiAimMode, "Spin\0Jitter\0Backwards 180\0");
                ImGui::SliderFloat("Spin Speed", spinSpeed, 1.0f, 100.0f, "%.0f");
                if (*antiAimMode == 1) ImGui::SliderInt("Jitter Range", jitterRange, 5, 180);
                ImGui::TextDisabled("Active right-click. No need to ADS or fire.");
            } End();
        }
    }

    void TabCamera() {
        if (Sub("View")) {
            T("Third Person", thirdPerson, "Move camera behind your player");
            if (*thirdPerson) ImGui::SliderFloat("Distance", thirdPersonDist, 20.0f, 400.0f, "%.0f");
            T("FOV Changer", fovChanger, "Override the field of view");
            if (*fovChanger) ImGui::SliderFloat("Custom FOV", customFOV, 50.0f, 180.0f, "%.0f");
            T("Free Cam", freeCam, "Detach camera and fly around"); End();
        }
        if (Sub("Vision Modes")) { T("Night Vision", nightVision, "Green night-vision effect"); T("Thermal Vision", thermalVision, "Thermal view of players"); End(); }
    }

    void TabDMZ() {
        if (Sub("Vehicle Hacks")) { TL("Speed Boost", vehicleSpeedBoost, "Boost vehicle speed"); T("God Mode", vehicleGodMode, "Vehicle invulnerability");
            if (*vehicleSpeedBoost) ImGui::SliderFloat("Speed Mult", vehicleSpeedMult, 1.0f, 10.0f, "%.1fx");
            End(); }
        if (Sub("BOSS Priority")) { T("Boss Priority Aim", bulletTracking); End(); }
        if (Sub("World ESP Toggles")) { TL("Buy Stations", buyStationESP); TL("Strongholds", strongholdESP);
            TL("SAM Sites", samSiteESP); TL("Supply Drops", supplyDropESP); T("Boss Locations", bossESP);
            End(); }
        if (Sub("Name Spoofer")) { T("Enable Name Spoof", nameSpoofEnabled, "Spoof your display name"); T("Bypass Profanity", bypassProfanity, "Allow any name text");
            if (*nameSpoofEnabled) {
                ImGui::Text("Name"); ImGui::SameLine(); ImGui::PushItemWidth(160);
                ImGui::InputText("##spoofname", nameSpoofBuffer, 32); ImGui::PopItemWidth();
                ImGui::Text("Clan"); ImGui::SameLine(); ImGui::PushItemWidth(100);
                ImGui::InputText("##clantag", clanTagBuffer, 16); ImGui::PopItemWidth();
            }
            End(); }
        if (Sub("Auto Loot")) { T("Loot Filter", lootFilterEnabled); T("Auto Pickup", autoPickupEnabled); End(); }
        if (Sub("Spectator Tracker")) { T("Detect Spectators", spectatorEnabled, "Detect when spectated"); T("Auto Cloak", spectatorAutoCloak, "Auto enable stealth when spectated");
            if (*spectatorEnabled) ImGui::TextDisabled("Red warning when spectated"); End(); }
        if (Sub("Account Health")) { T("Monitor Status", accountHealthEnabled, "Shadowban / Ban / Restricted detection");
            ImGui::TextDisabled("Shadowban / Ban / Restricted detection"); End(); }
    }

    void TabPlayers() {
        if (Sub("Detected Players")) {
            ImGui::TextColored(ImVec4(0.70f,0.70f,0.80f,1.0f), "Players detected: %d", playersRef ? (int)playersRef->size() : 0);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f,0.20f,0.20f,0.80f));
            if (ImGui::Button("Leave Match", ImVec2(110, 28))) LeaveMatch();
            ImGui::PopStyleColor(1);
            ImGui::Separator();
            End();
        }

        if (!playersRef || playersRef->empty()) {
            ImGui::TextDisabled("No players detected yet");
            return;
        }

        const char* cols[] = { "Name", "Team", "HP", "Dist", "Weapon", "" };
        ImGui::Columns(6);
        for (int i = 0; i < 6; i++) {
            ImGui::TextColored(ImVec4(0.95f,0.35f,0.00f,1.0f), "%s", cols[i]);
            ImGui::NextColumn();
        }
        ImGui::Separator();

        int idx = 0;
        for (const auto& p : *playersRef) {
            if (!p.IsAlive()) continue;
            bool enemy = (p.GetTeam() != localTeam);
            ImColor nc = enemy ? ImColor(255,90,90,255) : ImColor(90,200,90,255);

            ImGui::PushID(idx++);
            ImGui::TextColored((ImVec4)nc, "%s", p.GetName().c_str()); ImGui::NextColumn();
            ImGui::Text("%d", p.GetTeam()); ImGui::NextColumn();
            ImGui::Text("%.0f", p.GetHealth()); ImGui::NextColumn();
            ImGui::Text("%.0f", p.GetDistance()); ImGui::NextColumn();
            ImGui::Text("%d", p.GetWeapon()); ImGui::NextColumn();

            std::string rl = "Report##r" + std::to_string(idx);
            if (ImGui::Button(rl.c_str(), ImVec2(70, 0))) {
                std::string txt = "REPORT: " + p.GetName() + " (team " + std::to_string(p.GetTeam()) + ")";
                CopyToClipboard(p.GetName());
                lastReported = p.GetName();
                reportFlash = 2.0f;
            }
            ImGui::NextColumn();
            ImGui::PopID();
        }
        ImGui::Columns(1);

        if (reportFlash > 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.20f,0.85f,0.30f,1.0f), "Reported %s (name copied to clipboard)", lastReported.c_str());
            reportFlash -= ImGui::GetIO().DeltaTime;
        }
    }

    void TabConfig() {
        if (Sub("Protection")) { TL("Stealth", stealthEnabled, "Hide from anticheat scans"); TL("Anti-Debug", antiDebug, "Block debugger detection"); T("Blacklist Scan", blacklistScan, "Scan for blacklisted processes"); End(); }
        if (Sub("Appearance")) {
            ImGui::Text("Accent Color");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Customize the UI theme color");
            if (accentR && accentG && accentB) {
                ImGui::ColorEdit3("##accent", &accent.x, ImGuiColorEditFlags_NoInputs);
                if (*accentR != accent.x || *accentG != accent.y || *accentB != accent.z) { *accentR = accent.x; *accentG = accent.y; *accentB = accent.z; ApplyStyle(); }
                if (ImGui::Button("Reset Accent", ImVec2(140, 24))) { accent = ImVec4(0.95f,0.35f,0.00f,1.0f); *accentR = accent.x; *accentG = accent.y; *accentB = accent.z; ApplyStyle(); }
            }
            End();
        }
        if (Sub("Crosshair")) {
            T("Custom Crosshair", crosshairEnabled, "Draw a custom crosshair");
            if (*crosshairEnabled) {
                ImGui::Combo("Type", crosshairType, "Cross\0Circle\0Dot\0T-Shape\0Brackets\0Custom\0");
                ImGui::SliderFloat("Size", crosshairSize, 2.0f, 30.0f, "%.0f");
                ImGui::SliderFloat("Thickness", crosshairThickness, 1.0f, 6.0f, "%.0f");
                ImGui::SliderFloat("Gap", crosshairGap, 0.0f, 15.0f, "%.0f");
                ImGui::ColorEdit4("Color", crosshairColor);
                ImGui::Checkbox("Outline", crosshairOutline);
            } End();
        }
        if (Sub("Config")) {
            T("Session Timer", sessionTimerEnabled, "Show session time + FPS on screen");
            if (onListProfiles) {
                static char newProfile[32] = {};
                ImGui::TextColored(ImVec4(accent.x,accent.y,accent.z,0.90f), "Profiles");
                std::vector<std::string> profs = onListProfiles();
                static std::vector<std::string> cached;
                cached = profs;
                int cur = 0;
                const char* curName = onCurrentProfile ? onCurrentProfile() : "default";
                for (int i = 0; i < (int)cached.size(); i++) if (cached[i] == curName) { cur = i; break; }
                if (ImGui::BeginCombo("Profile", cached.empty() ? "No profiles" : cached[cur].c_str())) {
                    for (int i = 0; i < (int)cached.size(); i++) {
                        bool sel = (i == cur);
                        if (ImGui::Selectable(cached[i].c_str(), sel)) { if (onLoadProfile) onLoadProfile(cached[i].c_str()); }
                    }
                    ImGui::EndCombo();
                }
                ImGui::InputText("New Profile", newProfile, sizeof(newProfile));
                if (newProfile[0] != 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("Save As")) { if (onSaveProfile) onSaveProfile(newProfile); newProfile[0] = 0; }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete") && !cached.empty()) { if (onDeleteProfile) onDeleteProfile(cached[cur].c_str()); }
                }
                ImGui::Separator();
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(accent.x,accent.y,accent.z,0.50f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x,accent.y,accent.z,0.70f));
            if (ImGui::Button("Save Config", ImVec2(140, 28))) { if (onSaveConfig) onSaveConfig(); }
            ImGui::SameLine();
            if (ImGui::Button("Load Config", ImVec2(140, 28))) { if (onLoadConfig) onLoadConfig(); }
            if (ImGui::Button("Reset to Defaults", ImVec2(140, 28))) { if (onResetConfig) onResetConfig(); }
            ImGui::PopStyleColor(2); End();
        }
        if (Sub("Player List")) { T("Show Player Info", playerListOpen, "Show live player list overlay"); End(); }
        ImGui::Dummy(ImVec2(0,4));
        if (ImGui::Button("Open Keybind Editor", ImVec2(240, 28))) { keybindEditorOpen = !keybindEditorOpen; }
    }

public:
    void SetPlayerData(const std::vector<Player>* players, int team) { playersRef = players; localTeam = team; }

    Menu() : open(true), activeTab(0), openAnim(1.0f), tabAnim(0), time(0), particleTimer(0), keybindEditorOpen(false),
        searchBuf{0}, accent(0.95f,0.35f,0.00f,1.0f), accentR(nullptr), accentG(nullptr), accentB(nullptr) {}

    void Setup() { ApplyStyle(); }

    void Init(
        bool* ab, bool* sa, bool* tr, bool* ta, bool* hs, bool* wb,
        float* af, float* asmth, int* tb, float* sf, bool* sh, int* td, bool* tau, bool* dfc, bool* bt, int* am,
        bool* stt, bool* aw, bool* bb, bool* bo, int* bok, int* boid, float* ho,
        bool* avo, bool* rsb, int* sb,
        bool* asc, bool* zf, float* zfct, bool* ash, bool* rc, float* rs_,
        bool* ov, bool* wh, bool* ve, bool* le,
        bool* ebox, bool* esn, bool* eh, bool* en, bool* ed, bool* ehc,
        bool* earm, bool* ebf, int* ebs_, bool* etb, bool* edf,
        bool* sk, bool* skb, bool* skj,
        bool* exf, bool* con, bool* db, bool* shESP, bool* bsESP,
        bool* samESP, bool* sdESP, bool* bse_,
        bool* hud, bool* wm, bool* cmp, bool* gm, bool* tm,
        bool* vo, bool* vi, bool* np,
        bool* wn, bool* sc, bool* ir,
        bool* nr, bool* ns, bool* rf, int* rd, bool* ia, bool* isw,
        bool* afr, bool* irl, int* amm,
        bool* fr_, float* rs_,
        bool* rad, bool* rr, float* rrg, float* rsz, float* ro,
        bool* re, bool* rt, bool* rv, bool* ra,
        bool* bh_, bool* js_, bool* ssl, bool* sst, bool* ast_,
        bool* aa, int* aam, float* ss, int* jr,
        bool* tp, float* tpd, bool* fc, float* cf,
        bool* nv, bool* tv, bool* fcm,
        bool* st, bool* ad, bool* bl,
        bool* crh, int* crt, float* crs, float* crth, float* crg, bool* cro, float* crc,
        float* fcc_, float* fct_,
        bool* pl,
        bool* vsb, bool* vgm, float* vsm,
        bool* nse, bool* bp_, char* nb_, char* ctb_,
        bool* spe_, bool* spa_,
        bool* ahe_,
        bool* lfe_, bool* ape_,
        bool* se_, bool* sfs_, bool* sgs_, bool* sex_, bool* sv_,
        bool* stm_,
        float* accR_, float* accG_, float* accB_
    ) {
        nameSpoofEnabled=nse; bypassProfanity=bp_; nameSpoofBuffer=nb_; clanTagBuffer=ctb_;
        spectatorEnabled=spe_; spectatorAutoCloak=spa_;
        accountHealthEnabled=ahe_;
        lootFilterEnabled=lfe_; autoPickupEnabled=ape_;
        soundESPEnabled=se_; soundESPFootsteps=sfs_; soundESPGunshots=sgs_; soundESPExplosions=sex_; soundESPVehicles=sv_;
        sessionTimerEnabled=stm_;
        accentR=accR_; accentG=accG_; accentB=accB_;
        if (accentR && accentG && accentB) accent = ImVec4(*accentR, *accentG, *accentB, 1.0f);
        aimbotEnabled=ab; silentAimEnabled=sa; triggerbotEnabled=tr; triggerAdvanced=ta;
        headshotOnly=hs; wallBang=wb;         aimFOV=af; aimSmooth=asmth; targetBone=tb;
        silentFOV=sf; silentHeadshot=sh; triggerDelay=td; triggerAutoShoot=tau;
        drawFovCircle=dfc; bulletTracking=bt; aimMode=am;
        stickToTarget=stt; autoWall=aw; bestBone=bb; boneOverride=bo; boneOverrideKey=bok; boneOverrideId=boid;
        headOffset=ho;
        aimVisibleOnly=avo; riotShieldBypass=rsb; shieldBone=sb;
        autoScope=asc; zoomFOV=zf; zoomFactor=zfct; autoShoot=ash; recoilComp=rc; recoilStrength=rs_;

        espOverlay=ov; wallhack=wh; vehicleESP=ve; lootESP=le;
        espBoxes=ebox; espSnaplines=esn; espHealth=eh; espNames=en;
        espDistance=ed; espHeadCircle=ehc;
        espArmor=earm; espBoxFill=ebf; espBoxStyle=ebs_; espTextBackground=etb; espDistanceFade=edf;
        skeletonEnabled=sk; skeletonBones=skb; skeletonJoints=skj;
        exfilESP=exf; contractESP=con; deadBodyESP=db;
        strongholdESP=shESP; buyStationESP=bsESP; samSiteESP=samESP;
        supplyDropESP=sdESP; bossESP=bse_;
        worldHUD=hud; drawWatermark=wm; drawCompass=cmp; drawGameMode=gm; drawTimer=tm;
        visibleOnly=vo; visibilityIndicator=vi; grenadePrediction=np;
        weaponNames=wn; squadCount=sc; itemRarity=ir;

        noRecoil=nr; noSpread=ns; rapidFire=rf; rapidFireDelay=rd;
        infiniteAmmo=ia; instantSwap=isw; autoFire=afr; instantReload=irl;
        weaponAmmoMod=amm;
        fastReload=fr_; reloadSpeed=rs_;

        radarEnabled=rad; radarRotate=rr; radarRange=rrg; radarSize=rsz; radarOpacity=ro;
        radarShowEnemies=re; radarShowTeammates=rt; radarShowVehicles=rv; radarShowAI=ra;

        bhopEnabled=bh_; jumpSpam=js_; superSlide=ssl; superStrafe=sst; airStrafing=ast_;
        antiAimEnabled=aa; antiAimMode=aam; spinSpeed=ss; jitterRange=jr;
        thirdPerson=tp; thirdPersonDist=tpd; fovChanger=fc; customFOV=cf;
        nightVision=nv; thermalVision=tv; freeCam=fcm;
        stealthEnabled=st; antiDebug=ad; blacklistScan=bl;
        crosshairEnabled=crh; crosshairType=crt; crosshairSize=crs; crosshairThickness=crth; crosshairGap=crg;
        crosshairOutline=cro; crosshairColor=crc; fovCircleColor=fcc_; fovCircleThickness=fct_;
        playerListOpen=pl;
        vehicleSpeedBoost=vsb; vehicleGodMode=vgm; vehicleSpeedMult=vsm;
    }

    void SetConfigCallbacks(std::function<void()> save, std::function<void()> load, std::function<void()> reset) {
        onSaveConfig = save; onLoadConfig = load; onResetConfig = reset;
    }
    void SetProfileCallbacks(std::function<std::vector<std::string>()> list,
        std::function<void(const char*)> loadP, std::function<void(const char*)> saveP,
        std::function<void(const char*)> delP, std::function<const char*()> cur) {
        onListProfiles = list; onLoadProfile = loadP; onSaveProfile = saveP;
        onDeleteProfile = delP; onCurrentProfile = cur;
    }

    void RenderOverlays(int sw, int sh) {
        // FOV circle now handled by FOVRenderer
    }

    void RenderSearch() {
        ImGui::SetCursorPos(ImVec2(6, 120));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 28);
        ImGui::InputTextWithHint("##search", "Search features...  (ESC to clear)", searchBuf, sizeof(searchBuf));
        ImGui::PopStyleVar();

        static const struct { const char* label; int tab; } registry[] = {
            {"Aimbot Enable",0},{"Aimbot FOV",0},{"Aimbot Smoothness",0},{"Headshot Only",0},{"Wall Bang",0},
            {"Target Mode",0},{"Bullet Tracking",0},{"Draw FOV Circle",0},{"Exfil Camp",0},{"Buy Station Camp",0},
            {"Camp Range",0},{"Silent Aim",0},{"Silent FOV",0},{"Silent Headshot",0},{"Triggerbot",0},
            {"Trigger Advanced",0},{"Trigger Delay",0},{"Auto Shoot",0},
            {"ESP",1},{"Wallhack",1},{"Visible Only",1},{"ESP Boxes",1},{"ESP Snaplines",1},{"ESP Health",1},
            {"ESP Names",1},{"ESP Distance",1},{"Head Circle",1},{"Loot ESP",1},{"Vehicle ESP",1},
            {"Skeleton",1},{"Skeleton Bones",1},{"Skeleton Joints",1},{"Weapon Names",1},{"Squad Count",1},
            {"Item Rarity",1},{"Exfil ESP",1},{"Contract ESP",1},{"Dead Bodies",1},{"Buy Station ESP",1},
            {"Stronghold ESP",1},{"Boss Locations",1},{"SAM Sites",1},{"Supply Drops",1},{"World HUD",1},
            {"Watermark",1},{"Game Mode",1},{"Compass",1},{"HUD Timer",1},{"Visibility Indicator",1},
            {"Grenade Prediction",1},{"Sound ESP",1},{"Sound Footsteps",1},{"Sound Gunshots",1},
            {"Sound Explosions",1},{"Sound Vehicles",1},
            {"No Recoil",2},{"No Spread",2},{"Rapid Fire",2},{"Rapid Fire Delay",2},{"Auto Fire",2},
            {"Infinite Ammo",2},{"Instant Swap",2},{"Instant Reload",2},{"Ammo Modifier",2},
            {"Radar",3},{"Radar Range",3},{"Radar Size",3},{"Radar Opacity",3},{"Rotate Radar",3},
            {"Radar Enemies",3},{"Radar Teammates",3},{"Radar Vehicles",3},{"Radar AI",3},
            {"Bunny Hop",4},{"Speed Multiplier",4},{"No Fall Damage",4},{"Air Strafing",4},
            {"Anti-Aim",5},{"Anti-Aim Mode",5},{"Spin Speed",5},{"Jitter Range",5},{"Disable On Fire",5},
            {"Third Person",6},{"Third Person Distance",6},{"FOV Changer",6},{"Custom FOV",6},{"Free Cam",6},
            {"Night Vision",6},{"Thermal Vision",6},
            {"Vehicle Speed Boost",7},{"Vehicle God Mode",7},{"Speed Mult",7},{"Exfil Camp Mode",7},
            {"Boss Priority Aim",7},{"Name Spoofer",7},{"Name Spoof",7},{"Bypass Profanity",7},
            {"Clan Tag",7},{"Auto Loot",7},{"Loot Filter",7},{"Auto Pickup",7},{"Name Rotator",7},
            {"Spectator Tracker",7},{"Detect Spectators",7},{"Auto Cloak",7},{"Account Health",7},
            {"Stealth",8},{"Anti-Debug",8},{"Blacklist Scan",8},{"Accent Color",8},{"Custom Crosshair",8},
            {"Crosshair Type",8},{"Crosshair Size",8},{"Crosshair Thickness",8},{"Crosshair Gap",8},
            {"Crosshair Color",8},{"FOV Circle Color",8},{"FOV Circle Thickness",8},
            {"Crosshair Outline",8},{"Reset to Defaults",8},
            {"Session Timer",8},{"Save Config",8},{"Load Config",8},{"Player List",8},{"Keybind Editor",8},
        };
        const size_t n = sizeof(registry)/sizeof(registry[0]);

        std::string q = searchBuf;
        std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c){ return (char)std::tolower(c); });

        float h = ImGui::GetWindowHeight() - 200;
        if (!q.empty() && h > 60) {
            if (ImGui::BeginChild("##sr", ImVec2(ImGui::GetWindowWidth()-12, h), true)) {
                const char* tabNames[] = {"AIM","VIS","WEP","RAD","MOVE","AA","CAM","DMZ","CFG","PLY"};
                bool any = false;
                for (size_t i = 0; i < n; i++) {
                    std::string l = registry[i].label;
                    std::string lc = l; std::transform(lc.begin(), lc.end(), lc.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    if (lc.find(q) != std::string::npos) {
                        any = true;
                        if (ImGui::Button((l+"  ["+tabNames[registry[i].tab]+"]").c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                            activeTab = registry[i].tab;
                            searchBuf[0] = 0;
                        }
                    }
                }
                if (!any) ImGui::TextDisabled("No matches");
            }
            ImGui::EndChild();
            return;
        }
    }

    int Render(bool* menuOpen, const std::string& status, int pc, int vc, int lh, int la) {
        if (!open) return activeTab;
        time += ImGui::GetIO().DeltaTime;
        particleTimer += ImGui::GetIO().DeltaTime;
        if (particles.size() < 14 && particleTimer > 0.15f) {
            particleTimer = 0;
            Particle p;
            p.pos = ImVec2((float)(rand()%300)+40, -10.0f);
            p.vel = ImVec2(((float)rand()/RAND_MAX-0.5f)*0.15f, ((float)rand()/RAND_MAX)*0.3f+0.1f);
            p.life = p.maxLife = (float)rand()/RAND_MAX*2.0f+1.0f;
            p.size = (float)rand()/RAND_MAX*1.5f+0.5f;
            p.color = ImColor((int)(accent.x*255)+(rand()%30), (int)(accent.y*255)+(rand()%40), (int)(accent.z*255), 120);
            particles.push_back(p);
        }
        for (int i=(int)particles.size()-1; i>=0; i--) {
            auto& p = particles[i];
            float dt = ImGui::GetIO().DeltaTime;
            p.pos.x += p.vel.x*60*dt; p.pos.y += p.vel.y*60*dt; p.life -= dt;
            if (p.life <= 0 || p.pos.y > 600) particles.erase(particles.begin()+i);
        }

        float dspX = ImGui::GetIO().DisplaySize.x, dspY = ImGui::GetIO().DisplaySize.y;
        float winW = 620.0f, winH = 480.0f;
        if (dspX > 0 && dspY > 0) {
            winW = (dspX < 640) ? dspX - 16 : 620.0f;
            winH = (dspY < 520) ? dspY - 16 : 480.0f;
        }
        ImGui::SetNextWindowSizeConstraints(ImVec2(winW - 40, winH - 30), ImVec2(winW, winH));
        ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_FirstUseEver);
        ImGui::Begin("ZORMenu v4.0", menuOpen,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 wPos = ImGui::GetWindowPos(), wSize = ImGui::GetWindowSize();
        for (auto& p : particles) {
            float alpha = (p.life/p.maxLife)*0.3f;
            ImColor c = p.color; c.Value.w = alpha;
            draw->AddCircleFilled(ImVec2(wPos.x+p.pos.x, wPos.y+p.pos.y), p.size, c);
        }

        draw->AddRectFilledMultiColor(ImVec2(wPos.x,wPos.y), ImVec2(wPos.x+wSize.x,wPos.y+70),
            ImColor(accent.x,accent.y,accent.z,0.08f), ImColor(accent.x,accent.y,accent.z,0.02f),
            ImColor(accent.x,accent.y,accent.z,0.02f), ImColor(accent.x,accent.y,accent.z,0.08f));
        draw->AddLine(ImVec2(wPos.x,wPos.y+70), ImVec2(wPos.x+wSize.x,wPos.y+70), ImColor(0.20f,0.20f,0.28f,0.50f));

        ImGui::SetCursorPos(ImVec2(12,12));
        ImGui::TextColored(ImVec4(accent.x,accent.y,accent.z,1.0f), "ZORMenu");
        ImGui::SameLine(); ImGui::TextColored(ImVec4(0.50f,0.50f,0.55f,1.0f), "v4.0");

        ImGui::SameLine(wSize.x-120);
        bool connected = status.find("Connected") != std::string::npos;
        ImColor stC = connected ? ImColor(0.0f,0.85f,0.20f,1.0f) : ImColor(0.85f,0.20f,0.0f,1.0f);
        draw->AddCircleFilled(ImVec2(wPos.x+wSize.x-100, wPos.y+18), 4, stC);
        ImGui::TextColored((ImVec4)stC, connected ? "CONNECTED" : "DISCONNECTED");

        ImGui::SetCursorPos(ImVec2(12,32));
        ImGui::TextColored(ImVec4(0.60f,0.60f,0.65f,1.0f), "P: %d  V: %d  HP: %d  Armor: %d", pc, vc, lh, la);

        ImGui::SetCursorPos(ImVec2(4,76)); ImGui::BeginGroup();
        draw->AddRectFilled(ImVec2(wPos.x+4,wPos.y+74), ImVec2(wPos.x+wSize.x-4,wPos.y+116),
            ImColor(0.08f,0.08f,0.12f,0.40f), 6.0f);
        const char* lbl[] = {"AIM","VIS","WEP","RAD","MOVE","AA","CAM","DMZ","CFG","PLY"};
        const char* icn[] = {"\xe2\x9b\x85","\xf0\x9f\x91\x81","\xe2\x99\xaa","\xf0\x9f\x93\xa1","\xf0\x9f\x8f\x83","\xf0\x9f\x8e\xaf","\xf0\x9f\x93\xb7","\xf0\x9f\x92\xa3","\xe2\x9a\x99\xef\xb8\x8f","\xf0\x9f\x91\xa5"};
        for (int i=0;i<10;i++) TabBtn(lbl[i], icn[i], i, 10);
        ImGui::EndGroup();

        RenderSearch();
        if (searchBuf[0] != 0) { ImGui::End(); return activeTab; }

        ImGui::SetCursorPos(ImVec2(6,170));
        float cH = wSize.y-130;
        if (ImGui::BeginChild("C", ImVec2(wSize.x-12,cH), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,6));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8,4));
            switch(activeTab) {
                case 0: TabAimbot(); break; case 1: TabVisuals(); break; case 2: TabWeapons(); break;
                case 3: TabRadar(); break; case 4: TabMovement(); break; case 5: TabAntiAim(); break;
                case 6: TabCamera(); break; case 7: TabDMZ(); break; case 8: TabConfig(); break;
                case 9: TabPlayers(); break;
            }
            ImGui::PopStyleVar(2);
        }
        ImGui::EndChild();
        ImGui::End();
        return activeTab;
    }

    bool IsKeybindEditorOpen() const { return keybindEditorOpen; }
    bool* GetKeybindEditorOpenPtr() { return &keybindEditorOpen; }
    void SetOpen(bool o) { open = o; }
    bool IsOpen() const { return open; }
    int GetActiveTab() const { return activeTab; }
    void SetActiveTab(int t) { if (t >= 0 && t < 10) { activeTab = t; tabAnim = 0; } }
};
