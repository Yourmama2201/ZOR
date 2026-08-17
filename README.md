# ZOR - DMZ Cheat v4.0

ZOR is an external cheat for Call of Duty: DMZ. It combines a kernel driver (for PID + privilege), an external loader (`ZORLoader`) that fetches fresh binaries and launches the client, and an internal overlay client (`ZORClient.dll`) with a full DX11 menu.

## Features

### Aimbot
- FOV, smoothing, headshot-only, bullet tracking (movement prediction)
- **Best Bone** auto-picking by distance (head close / neck mid / chest far)
- Main bones: Best Bone · Head · Torso · Pelvis · Left Shoulder · Right Shoulder
- **Alt-bone override** via keybind — keyboard **or controller** (auto-detects device), with the full remaining-body bone list
- Bullet drop compensation + adjustable bullet speed
- Humanized smoothing (fast flick in, gentle ease near target)
- Lowest-health-first targeting, stick-to-target, visible-only, auto-wall
- Riot shield bypass (aims legs), auto scope, zoom FOV, auto shoot, recoil comp
- One-click **playstyle presets**: Rage / Semi Rage / Sweat / Semi Legit / Legit
- Max distance cap (default 5000m)

### ESP / Visuals
- Boxes, snaplines, health bars, names, distance, head circles
- Skeleton ESP, vehicle ESP, loot ESP, world visuals, night vision
- FOV circle (color + thickness), custom crosshair, grenade prediction

### Misc
- Silent aim, triggerbot (advanced + auto shoot), anti-aim, no recoil
- No spread, weapon editor / exploiter, camo changer, movement
- Radar, name spoofing, spectator tracker, session timer
- Stealth mode, sound ESP, Discord RPC

## Project Layout

| Path | What it is |
|---|---|
| `Driver/` | Kernel driver (`zordriver.c`) — PID lookup + privilege, builds to `nxs_drv.sys` |
| `Client/` | The main cheat — `ZORClient.dll` (injected) + `ZORLoader.exe` (external loader) |
| `Client/ZORLoader/` | Loader: installs driver, finds game, injects client, fetches updates |
| `Injector_Resource/` | Standalone friend-injector — manually maps a `cheat.dll` resource into the game |
| `Resources/` | Icons / bundled assets |
| `Tools/` | Offset dumping and build helpers |

## Building

### Client (`ZORClient.dll` + `ZORLoader.exe`)
```
cd Client
build_client.bat
```
Outputs to `Client\x64\Release\`.

### Driver
```
cd Driver
build_driver.bat        # requires WDK 10.0.28000.0 (MSBuild)
```
Outputs `nxs_drv.sys`.

### Friend Injector
```
cd Injector_Resource
copy ..\Client\x64\Release\ZORClient.dll cheat.dll
build.bat
```
Outputs `injector.exe` with the DLL embedded as resource `101 RCDATA`.

Generate a license:
```
injector.exe --gen 30        # 30-day license.lic
```

## Usage

1. Run `ZORLoader.exe` (installs driver, self-updates).
2. Launch Call of Duty: DMZ.
3. In the loader: **DRIVER → PROCESS → INJECT**.
4. Press `INSERT` in-game to open the menu.

Check `ZORLoader_runtime.log` for injection status (`Exec ErrorStatus`).

## Default Keybinds

| Action | Key |
|---|---|
| Menu Toggle | INSERT |
| Aimbot | MOUSE 4 |
| Silent Aim | F6 |
| Triggerbot | F7 |
| Anti-Aim | F8 |
| ESP | F9 |
| Radar | F10 |
| No Recoil | F5 |
| Stealth Mode | F11 |
| Bone Override (Alt Bone) | MOUSE 5 |

Every bind can be remapped in the in-game Keybind Editor and supports a controller button too.

## Disclaimer

For private use on your own account. Using cheats may violate the game's terms of service and can result in account bans. Use at your own risk.
