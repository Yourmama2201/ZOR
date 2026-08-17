#pragma once
#include <vector>
#include <cmath>
#include <string>
#include "player.hpp"
#include "math.hpp"
#include "memory.hpp"
#include "offsets.hpp"

class Aimbot {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool headshotOnly;
    bool wallBang;
    float fov;
    float smoothness;
    int targetBone;
    int aimMode; // 0=all, 1=players only, 2=AI only, 3=boss priority
    bool bulletTracking;
    bool stickToTarget;
    bool autoWall;
    bool bestBone;
    bool boneOverride;
    int boneOverrideKey;
    int boneOverrideId;
    uintptr_t lockedTarget;
    float headOffset;
    bool visibleOnly;
    bool riotShieldBypass;
    int shieldBone;
    bool autoScope;
    bool zoomFOV;
    float zoomFactor;
    bool autoShoot;
    bool recoilComp;
    float recoilStrength;
    float maxDistance;
    bool bulletDrop;
    float bulletSpeed;
    float gravity;
    bool lowestHealthFirst;
    bool humanSmooth;
    bool wasScoped;
    DWORD lastShot;

    Vec3 CorrectHead(const Player& p, const Vec3& headPos) {
        Vec3 pos = headPos;
        if (headOffset != 0.0f) {
            // Ghost Rook / glitched-hitbox skins: the visual head sits below the
            // real head hitbox, so nudge the aim point up by headOffset units.
            pos.z += headOffset;
        }
        return pos;
    }

    // Pick the aim bone for a target. Riot shield users block chest/head from the
    // front, so we aim at the legs when they have a shield up.
    int PickBone(const Player& target, const Vec3& localPos) {
        int bone = targetBone;
        if (boneOverride && (GetAsyncKeyState(boneOverrideKey) & 0x8000)) {
            bone = boneOverrideId;
        }
        else if (riotShieldBypass && target.HasRiotShield(gameBase)) {
            bone = shieldBone;
        }
        else if (bestBone) {
            float d = localPos.Distance(target.GetPosition());
            // Range-aware: head up close, neck at medium range, chest far out
            // (bigger hitboxes at distance beat precision we can't hold).
            bone = (d < 20.0f) ? Offsets::HEAD :
                   (d < 60.0f) ? Offsets::NECK : Offsets::SPINE1;
        }
        return bone;
    }

    // Simulate a mouse button press (for auto-scope and auto-shoot)
    void MouseButton(int button, bool down) {
        INPUT inp = {};
        inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = (button == 0) ? (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP)
                                       : (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        SendInput(1, &inp, sizeof(INPUT));
    }

public:
    Aimbot(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(true), headshotOnly(false),
        wallBang(false), fov(30.0f), smoothness(0.05f), targetBone(Offsets::HEAD),
        aimMode(0),
        bulletTracking(true), stickToTarget(false), autoWall(false),
        bestBone(false), boneOverride(false), boneOverrideKey(VK_XBUTTON2),
        boneOverrideId(Offsets::HEAD), lockedTarget(0), headOffset(0.0f),
        visibleOnly(false), riotShieldBypass(true), shieldBone(Offsets::LEFT_KNEE),
        autoScope(false), zoomFOV(false), zoomFactor(0.5f), autoShoot(false),
        recoilComp(false), recoilStrength(1.0f), maxDistance(5000.0f),
        bulletDrop(true), bulletSpeed(800.0f), gravity(9.8f),
        lowestHealthFirst(true), humanSmooth(true), wasScoped(false), lastShot(0) {}

    void Run(std::vector<Player>& players, Vec3 localPos, int localTeam) {
        if (!enabled || !mem || players.empty()) return;

        Vec3 viewAngles = mem->Read<Vec3>(gameBase + Offsets::VIEW_ANGLES);

        // Effective FOV: shrink while scoped (zoom FOV)
        float effFOV = fov;
        if (zoomFOV && wasScoped) effFOV *= zoomFactor;

        Player* target = nullptr;
        float bestScore = 1e30f;

        for (auto& player : players) {
            if (player.GetTeam() == localTeam) continue;
            if (!player.IsAlive()) continue;
            if (visibleOnly && !player.IsVisible()) continue;
            if (!wallBang && !autoWall && !player.IsVisible()) continue;

            // Players Only Mode
            if (aimMode == 1 && player.GetTeam() == 0) continue;
            // AI Only Mode
            if (aimMode == 2 && player.GetTeam() != 0) continue;

            // Stick to target: once locked, don't re-acquire unless it's gone
            if (stickToTarget && lockedTarget != 0 && player.GetPtr() != lockedTarget) {
                bool stillValid = false;
                for (auto& p : players) {
                    if (p.GetPtr() == lockedTarget && p.IsAlive()) { stillValid = true; break; }
                }
                if (stillValid) continue;
            }

            int bone = PickBone(player, localPos);
            Vec3 targetPos = headshotOnly ? CorrectHead(player, player.GetHeadPos()) : player.GetBonePos(bone);
            Vec3 delta = targetPos - localPos;
            float distance = delta.Length();
            if (distance < 1.0f) continue;
            if (distance > maxDistance) continue;

            Vec3 angle = Math::CalculateAngle(localPos, targetPos);
            float fovDist = Math::GetFOV(viewAngles, angle);

            // Boss priority: reduce effective FOV for boss-type entities
            if (aimMode == 3) {
                std::string name = player.GetName();
                bool isBoss = (name.find("Juggernaut") != std::string::npos ||
                    name.find("Commander") != std::string::npos ||
                    name.find("Chemist") != std::string::npos ||
                    name.find("Boss") != std::string::npos);
                if (isBoss) fovDist *= 0.3f; // prioritize bosses
            }

            // Combined target score: FOV is king, but distance and (optionally)
            // health break ties. Within the FOV cone we prefer the nearest and
            // weakest target -> faster, more reliable kills.
            float score = fovDist;
            score += (distance / maxDistance) * effFOV * 0.5f;   // distance weight
            if (lowestHealthFirst) {
                float hp = player.GetHealth() + player.GetArmor();
                score += (hp / 400.0f) * effFOV * 0.25f;          // weakest first
            }

            if (score < bestScore) {
                bestScore = score;
                target = &player;
            }
        }

        if (!target) return;

        if (stickToTarget) lockedTarget = target->GetPtr();

        int bone = PickBone(*target, localPos);
        Vec3 targetPos = headshotOnly ? CorrectHead(*target, target->GetHeadPos()) : target->GetBonePos(bone);

        // Bullet tracking: predict movement AND account for projectile drop.
        if (bulletTracking) {
            Vec3 targetVel = target->GetVelocity();
            float dist = localPos.Distance(targetPos);
            float travelTime = dist / bulletSpeed;

            // Linear velocity lead on the moving target.
            targetPos.x += targetVel.x * travelTime;
            targetPos.y += targetVel.y * travelTime;
            targetPos.z += targetVel.z * travelTime;

            // Projectile gravity: aim ABOVE the target so the bullet arcs down
            // onto it. drop = 0.5 * g * t^2 (world Z is up).
            if (bulletDrop) {
                float drop = 0.5f * gravity * travelTime * travelTime;
                targetPos.z += drop;
            }
        }

        Vec3 angle = Math::CalculateAngle(localPos, targetPos);

        // Humanized smoothing: fast correction when far off, gentle ease-in near
        // the target (avoids robotic micro-snaps).
        float remain = Math::GetFOV(viewAngles, angle);
        angle = humanSmooth ? Math::HumanSmooth(viewAngles, angle, smoothness, remain)
                            : Math::SmoothAngle(viewAngles, angle, smoothness);

        // Recoil compensation: pull the view down by a proportional amount to fight kick
        if (recoilComp) {
            // small downward nudge on the pitch (x = yaw, y = pitch, z = roll)
            angle.y += recoilStrength * 0.015f;
        }

        mem->Write<Vec3>(gameBase + Offsets::VIEW_ANGLES, angle);

        // Auto scope: ADS while we have a target
        if (autoScope) {
            if (!wasScoped) { MouseButton(1, true); wasScoped = true; }
        }
        else if (wasScoped) { MouseButton(1, false); wasScoped = false; }

        // Auto shoot: fire when we're on target (within a tight FOV)
        if (autoShoot) {
            float curFOV = Math::GetFOV(viewAngles, angle);
            if (curFOV < 3.0f && GetTickCount() - lastShot > 180) {
                MouseButton(0, true);
                Sleep(30);
                MouseButton(0, false);
                lastShot = GetTickCount();
            }
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetHeadshotOnly(bool h) { headshotOnly = h; }
    void SetWallBang(bool w) { wallBang = w; }
    void SetFOV(float f) { fov = f; }
    void SetSmoothness(float s) { smoothness = s; }
    void SetMaxDistance(float d) { maxDistance = d; }
    void SetBulletDrop(bool b) { bulletDrop = b; }
    void SetBulletSpeed(float s) { bulletSpeed = s; }
    void SetLowestHealthFirst(bool b) { lowestHealthFirst = b; }
    void SetHumanSmooth(bool b) { humanSmooth = b; }
    void SetTargetBone(int b) { targetBone = b; }
    void SetAimMode(int m) { aimMode = m; }
    void SetBulletTracking(bool b) { bulletTracking = b; }
    void SetStickToTarget(bool s) { stickToTarget = s; if (!s) lockedTarget = 0; }
    void SetAutoWall(bool w) { autoWall = w; }
    void SetBestBone(bool b) { bestBone = b; }
    void SetBoneOverride(bool e) { boneOverride = e; }
    void SetBoneOverrideKey(int k) { boneOverrideKey = k; }
    void SetBoneOverrideId(int b) { boneOverrideId = b; }
    void SetHeadOffset(float o) { headOffset = o; }
    void SetVisibleOnly(bool v) { visibleOnly = v; }
    void SetRiotShieldBypass(bool e) { riotShieldBypass = e; }
    void SetShieldBone(int b) { shieldBone = b; }
    void SetAutoScope(bool e) { autoScope = e; }
    void SetZoomFOV(bool e) { zoomFOV = e; }
    void SetZoomFactor(float f) { zoomFactor = f; }
    void SetAutoShoot(bool e) { autoShoot = e; }
    void SetRecoilComp(bool e) { recoilComp = e; }
    void SetRecoilStrength(float s) { recoilStrength = s; }
    uintptr_t GetLockedTarget() const { return lockedTarget; }
};
