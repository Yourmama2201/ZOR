// nxsClient - DMZ ULTIMATE CHEAT - UI DRIVEN

#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "memory.hpp"
#include "offsets.hpp"
#include "player.hpp"
#include "math.hpp"
#include "vehicle.hpp"
#include "esp.hpp"
#include "aimbot.hpp"
#include "triggerbot.hpp"
#include "protection.hpp"
#include "menu.hpp"

#include "Features/Exploits/weapon_exploits.hpp"
#include "Features/Exploits/weapon_editor.hpp"
#include "Features/Exploits/camo_changer.hpp"
#include "Features/Aimbot/silent_aim.hpp"
#include "Features/AntiAim/anti_aim.hpp"
#include "Features/Radar/radar.hpp"
#include "Features/Triggerbot/triggerbot_advanced.hpp"
#include "Features/Visuals/world_visuals.hpp"
#include "Features/Visuals/camera.hpp"
#include "Features/Visuals/night_vision.hpp"
#include "Features/Visuals/fov_circle.hpp"
#include "Features/Visuals/grenade_prediction.hpp"
#include "Features/ESP/skeleton_esp.hpp"
#include "Features/ESP/exfil_esp.hpp"
#include "Features/Misc/name_system.hpp"
#include "Features/Misc/config_system.hpp"
#include "Features/Misc/filter_system.hpp"
#include "Features/Misc/movement.hpp"
#include "Features/Misc/crosshair.hpp"
#include "Features/Misc/player_list.hpp"
// Friend: #include "Features/Misc/auth_system.hpp"  -- your own auth header
#include "Features/Misc/keybinds.hpp"
#include "Features/Misc/name_spoofer.hpp"
#include "Features/Misc/spectator_tracker.hpp"
#include "Features/Misc/account_health.hpp"
#include "Features/ESP/loot_filter.hpp"
#include "Features/Visuals/sound_esp.hpp"
#include "Features/Misc/session_timer.hpp"
#include "Features/Misc/discord_rpc.hpp"

#include <fstream>
#include <string>
#include <ctime>
#include <mutex>

class DebugLogger {
private:
    std::string logFile; std::mutex mtx; bool enabled;
    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        struct tm tb; localtime_s(&tb, &time);
        char b[64]; sprintf_s(b, "%02d:%02d:%02d.%03d", tb.tm_hour, tb.tm_min, tb.tm_sec, (int)ms.count());
        return std::string(b);
    }
public:
    DebugLogger(const std::string& f = "nxs_dbg.log") : logFile(f), enabled(true) {
        std::ofstream o(logFile, std::ios::trunc);
        o << "DEBUG LOG\nStarted: " << GetTimestamp() << "\n\n"; o.close();
        Log("[INIT] Logger ready");
    }
    void Log(const std::string& m) {
        if (!enabled) return;
        std::lock_guard<std::mutex> l(mtx);
        std::ofstream o(logFile, std::ios::app);
        if (o.is_open()) { o << GetTimestamp() << " | " << m << "\n"; o.flush(); o.close(); }
    }
};
static DebugLogger g_Debug;

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

MemoryManager* g_mem = nullptr;
StealthProtection* g_protection = nullptr;
ESP* g_esp = nullptr;
Aimbot* g_aimbot = nullptr;
Triggerbot* g_triggerbot = nullptr;
Menu* g_menu = nullptr;

WeaponExploits* g_weaponExp = nullptr;
WeaponEditor* g_weaponEditor = nullptr;
CamoChanger* g_camoChanger = nullptr;
SilentAim* g_silentAim = nullptr;
AntiAim* g_antiAim = nullptr;
Radar* g_radar = nullptr;
TriggerbotAdvanced* g_triggerAdvanced = nullptr;
WorldVisuals* g_worldVis = nullptr;
Camera* g_camera = nullptr;
NightVision* g_nightVis = nullptr;
SkeletonESP* g_skeleton = nullptr;
GameWorldESP* g_gameWorld = nullptr;
NameSystem* g_nameSys = nullptr;
ConfigSystem* g_config = nullptr;
FilterSystem* g_filter = nullptr;
Movement* g_movement = nullptr;
CustomCrosshair* g_crosshair = nullptr;
FOVRenderer* g_fovRenderer = nullptr;
GrenadePrediction* g_grenadePred = nullptr;
PlayerList* g_playerList = nullptr;
VehicleHacks* g_vehicleHacks = nullptr;
// Friend: AuthSystem* g_auth = nullptr;  -- your auth system here
KeybindManager* g_keybinds = nullptr;
NameSpoofer* g_nameSpoof = nullptr;
SpectatorTracker* g_spectator = nullptr;
AccountHealth* g_accountHealth = nullptr;
LootFilter* g_lootFilter = nullptr;
SoundESP* g_soundESP = nullptr;
SessionTimer* g_sessionTimer = nullptr;
DiscordRPC* g_discordRPC = nullptr;

char g_customNameBuf[32] = "nxs_USER";
char g_clanTagBuf[16] = "YK";

std::vector<Player> g_players;
std::vector<Vehicle> g_vehicles;
bool g_running = true, g_gameFound = false;
HWND g_gameWindow = NULL;
uintptr_t g_gameBase = 0;
DWORD g_gamePid = 0;

// ===================== TOGGLES =====================
bool  g_aimbotEnabled = true, g_silentAimEnabled = false,
      g_triggerbotEnabled = false, g_trackAimEnabled = false,
      g_headshotOnly = false, g_wallBang = false,
      g_drawFovCircle = false, g_bulletTracking = true;
float g_aimFOV = 30.0f, g_aimSmooth = 0.05f, g_silentFOV = 15.0f,
      g_headOffset = 0.0f, g_aimMaxDistance = 5000.0f;
int   g_targetBone = Offsets::HEAD, g_triggerDelay = 20,
      g_aimPriority = 0, g_aimMode = 0;
bool  g_silentHeadshot = true, g_triggerAutoShoot = false;
bool  g_stickToTarget = false, g_autoWall = false, g_bestBone = false, g_boneOverride = false;
int   g_boneOverrideKey = VK_XBUTTON2, g_boneOverrideId = Offsets::HEAD;
bool  g_aimVisibleOnly = false, g_riotShieldBypass = true;
int   g_shieldBone = Offsets::LEFT_KNEE;
bool  g_autoScope = false, g_zoomFOV = false;
float g_zoomFactor = 0.5f;
bool  g_autoShoot = false, g_recoilComp = false;
float g_recoilStrength = 1.0f;

bool  g_espEnabled = true, g_wallhack = true, g_vehicleESP = true, g_lootESP = true,
      g_espBoxes = true, g_espSnaplines = true, g_espHealth = true,
      g_espNames = true, g_espDistance = true, g_espHeadCircle = true,
      g_espArmor = true, g_espBoxFill = false, g_espTextBackground = true,
      g_espDistanceFade = true,
      g_skeletonEnabled = true, g_skeletonBones = true, g_skeletonJoints = true,
      g_exfilESP = true, g_contractESP = true, g_deadBodyESP = true,
      g_strongholdESP = true, g_buyStationESP = true,
      g_samSiteESP = true, g_supplyDropESP = true, g_bossESP = true,
      g_worldHUD = true, g_drawWatermark = true, g_drawCompass = false,
      g_drawGameMode = true, g_drawTimer = true,
      g_visibleOnly = false, g_visibilityIndicator = false, g_grenadePrediction = false,
      g_weaponNames = true, g_squadCount = true, g_itemRarity = true;

bool  g_noRecoil = true, g_noSpread = true, g_rapidFire = false,
      g_infiniteAmmo = false, g_fastReload = false;
int   g_rapidFireDelay = 10, g_weaponAmmoMod = 0;
int   g_espBoxStyle = 0;
int   g_activeCamo = -1;
float g_reloadSpeed = 0.25f;

bool  g_radarEnabled = true, g_radarRotate = true,
      g_radarShowEnemies = true, g_radarShowTeammates = true,
      g_radarShowVehicles = true, g_radarShowAI = true;
float g_radarRange = 300.0f, g_radarSize = 200.0f, g_radarOpacity = 0.55f;

bool  g_bhopEnabled = false, g_jumpSpam = false, g_superSlide = false, g_superStrafe = false, g_airStrafing = false;

bool  g_antiAimEnabled = false;
int   g_antiAimMode = 0, g_jitterRange = 30;
float g_spinSpeed = 20.0f;

bool  g_thirdPerson = false, g_fovChanger = false, g_nightVision = false,
      g_thermalVision = false, g_freeCam = false;
float g_thirdPersonDist = 80.0f, g_customFOV = 90.0f;

bool  g_stealthEnabled = true, g_antiDebug = true, g_blacklistScan = true;
bool  g_crosshairEnabled = false, g_playerListOpen = false;
int   g_crosshairType = 0;
float g_crosshairSize = 12.0f, g_crosshairThickness = 2.0f, g_crosshairGap = 4.0f;
bool  g_crosshairOutline = true;
float g_crosshairColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
float g_fovCircleColor[4] = { 1.0f, 1.0f, 1.0f, 0.25f };
float g_fovCircleThickness = 1.0f;

bool  g_vehicleSpeedBoost = false, g_vehicleGodMode = false;
float g_vehicleSpeedMult = 2.0f;

bool  g_nameSpoofEnabled = false, g_bypassProfanity = true;
bool  g_spectatorEnabled = true, g_spectatorAutoCloak = true;
bool  g_accountHealthEnabled = true;
bool  g_lootFilterEnabled = true, g_autoPickupEnabled = false;
bool  g_soundESPEnabled = false, g_soundESPFootsteps = true, g_soundESPGunshots = true,
      g_soundESPExplosions = true, g_soundESPVehicles = true;

bool g_menuOpen = true;

bool g_sessionTimerEnabled = false;
float g_menuAccentR = 0.95f, g_menuAccentG = 0.35f, g_menuAccentB = 0.00f;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pMainRenderTargetView = nullptr;

void CreateRenderTarget() {
    ID3D11Texture2D* buf = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&buf));
    if (buf) { g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &g_pMainRenderTargetView); buf->Release(); }
}
void CleanupRenderTarget() { if (g_pMainRenderTargetView) { g_pMainRenderTargetView->Release(); g_pMainRenderTargetView=nullptr; } }
void CleanupDeviceD3D() { CleanupRenderTarget(); if(g_pSwapChain){g_pSwapChain->Release();g_pSwapChain=nullptr;} if(g_pd3dDeviceContext){g_pd3dDeviceContext->Release();g_pd3dDeviceContext=nullptr;} if(g_pd3dDevice){g_pd3dDevice->Release();g_pd3dDevice=nullptr;} }

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd={}; sd.BufferCount=2; sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow=hWnd; sd.SampleDesc.Count=1; sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl; const D3D_FEATURE_LEVEL fla[2]={D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};
    HRESULT r = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, fla, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext);
    if (r!=S_OK) return false; CreateRenderTarget(); return true;
}

void FindGameWindow() {
    const wchar_t* t[]={L"Call of Duty",L"Modern Warfare II",L"COD",L"cod22",L"Call of Duty\xae"};
    for(int i=0;i<5;i++){g_gameWindow=FindWindowW(NULL,t[i]); if(g_gameWindow)break;}
}
void UpdatePlayers() {
    if(!g_gameFound||!g_mem)return; g_players.clear(); g_players.reserve(100);
    uintptr_t el=g_mem->Read<uintptr_t>(g_gameBase+Offsets::DISTRIBUTE);
    if(!el)return;
    for(int i=0;i<100;i++){uintptr_t e=g_mem->Read<uintptr_t>(el+(i*0x8));if(!e)continue;if(!g_mem->Read<uint8_t>(e+Offsets::PLAYER_VALID))continue;    Player pl(*g_mem,e);if(pl.IsAlive())g_players.push_back(pl);}
}
void UpdateVehicles() {
    if(!g_gameFound||!g_mem)return; g_vehicles.clear(); g_vehicles.reserve(50);
    uintptr_t vl=g_mem->Read<uintptr_t>(g_gameBase+0x5000000);
    if(!vl)return;
    for(int i=0;i<50;i++){uintptr_t v=g_mem->Read<uintptr_t>(vl+(i*0x8));if(!v)continue;    Vehicle veh(*g_mem,v);if(veh.health>0)g_vehicles.push_back(veh);}
}

void SetupImGui() { IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    ImFontConfig baseCfg; baseCfg.SizePixels = 16.0f;
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &baseCfg);
    ImFontConfig cfg; cfg.MergeMode = true; cfg.GlyphOffset.y = 4.0f; cfg.SizePixels = 16.0f;
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 16.0f, &cfg);
    g_menu->Setup(); ImGui_ImplWin32_Init(g_gameWindow); ImGui_ImplDX11_Init(g_pd3dDevice,g_pd3dDeviceContext); }

void SaveConfig() {
    if(!g_config)return;
    auto s=[&](const char* k,bool v){g_config->SetBool(k,v);};
    auto sf=[&](const char* k,float v){g_config->SetFloat(k,v);};
    auto si=[&](const char* k,int v){g_config->SetInt(k,v);};
    auto ss=[&](const char* k,const char* v){g_config->SetString(k,v);};
    s("aimbot",g_aimbotEnabled); s("silent",g_silentAimEnabled); s("trigger",g_triggerbotEnabled);
    s("tadv",g_trackAimEnabled); s("headshot",g_headshotOnly); s("wallbang",g_wallBang);
    sf("fov",g_aimFOV); sf("smooth",g_aimSmooth); si("bone",g_targetBone);
    sf("sfov",g_silentFOV); s("shead",g_silentHeadshot); si("tdelay",g_triggerDelay);
    s("tauto",g_triggerAutoShoot); s("fovcirc",g_drawFovCircle); si("aimode",g_aimMode);
    s("bullet",g_bulletTracking);
    sf("headoff",g_headOffset); sf("maxdist",g_aimMaxDistance);
    s("aimvo",g_aimVisibleOnly); s("rsb",g_riotShieldBypass); si("shbone",g_shieldBone);
    s("ascope",g_autoScope); s("zoomfov",g_zoomFOV); sf("zoomfct",g_zoomFactor);
    s("autoshoot",g_autoShoot); s("recoilcomp",g_recoilComp); sf("recoilstr",g_recoilStrength);
    s("esp",g_espEnabled); s("wallhack",g_wallhack); s("vesp",g_vehicleESP); s("lesp",g_lootESP);
    s("ebox",g_espBoxes); s("esnap",g_espSnaplines); s("ehealth",g_espHealth); s("enames",g_espNames);
    s("edist",g_espDistance); s("ehead",g_espHeadCircle);
    s("earm",g_espArmor); s("ebf",g_espBoxFill); si("ebs",g_espBoxStyle); s("etb",g_espTextBackground); s("edf",g_espDistanceFade);
    s("skel",g_skeletonEnabled); s("skelb",g_skeletonBones); s("skelj",g_skeletonJoints);
    s("exfil",g_exfilESP); s("contract",g_contractESP); s("dead",g_deadBodyESP);
    s("shESP",g_strongholdESP); s("bsESP",g_buyStationESP); s("sam",g_samSiteESP);
    s("sdESP",g_supplyDropESP); s("bossESP",g_bossESP);
    s("hud",g_worldHUD); s("wm",g_drawWatermark); s("cmp",g_drawCompass); s("gm",g_drawGameMode); s("tm",g_drawTimer);
    s("vo",g_visibleOnly); s("vi",g_visibilityIndicator); s("np",g_grenadePrediction);
    s("wnames",g_weaponNames); s("sqcount",g_squadCount); s("irarity",g_itemRarity);
    s("recoil",g_noRecoil); s("spread",g_noSpread); s("rapid",g_rapidFire); si("rdelay",g_rapidFireDelay);
    s("fastreload",g_fastReload); sf("rspd",g_reloadSpeed);
    s("ammo",g_infiniteAmmo);
    si("ammmod",g_weaponAmmoMod);
    si("camo",g_activeCamo);
    s("radar",g_radarEnabled); s("rrot",g_radarRotate); sf("rrange",g_radarRange); sf("rsize",g_radarSize); sf("ropac",g_radarOpacity);
    s("ren",g_radarShowEnemies); s("rtm",g_radarShowTeammates); s("rveh",g_radarShowVehicles); s("rai",g_radarShowAI);
    s("bhop",g_bhopEnabled); s("jumpspam",g_jumpSpam); s("superslide",g_superSlide); s("superstrafe",g_superStrafe); s("airstrafe",g_airStrafing);
    s("aa",g_antiAimEnabled); si("aamode",g_antiAimMode); sf("sspeed",g_spinSpeed); si("jitter",g_jitterRange);
    s("tp",g_thirdPerson); sf("tdist",g_thirdPersonDist); s("fovc",g_fovChanger); sf("cfov",g_customFOV);
    s("nv",g_nightVision); s("th",g_thermalVision); s("freecam",g_freeCam);
    s("stealth",g_stealthEnabled); s("adbg",g_antiDebug); s("blk",g_blacklistScan);
    s("crosshair",g_crosshairEnabled); si("crtype",g_crosshairType); sf("crsize",g_crosshairSize);
    sf("crth",g_crosshairThickness); sf("crgap",g_crosshairGap); s("plist",g_playerListOpen);
    s("crout",g_crosshairOutline);
    sf("crcr",g_crosshairColor[0]); sf("crcg",g_crosshairColor[1]); sf("crcb",g_crosshairColor[2]); sf("crca",g_crosshairColor[3]);
    sf("fccr",g_fovCircleColor[0]); sf("fccg",g_fovCircleColor[1]); sf("fccb",g_fovCircleColor[2]); sf("fcca",g_fovCircleColor[3]);
    sf("fcth",g_fovCircleThickness);
    s("vboost",g_vehicleSpeedBoost); s("vgod",g_vehicleGodMode); sf("vsmult",g_vehicleSpeedMult);
    s("nspoof",g_nameSpoofEnabled); s("byp",g_bypassProfanity);
    s("spe",g_spectatorEnabled); s("spa",g_spectatorAutoCloak); s("ahealth",g_accountHealthEnabled);
    s("lfilter",g_lootFilterEnabled); s("apickup",g_autoPickupEnabled);
    s("sESP",g_soundESPEnabled); s("sESPF",g_soundESPFootsteps); s("sESPG",g_soundESPGunshots);
    s("sESPE",g_soundESPExplosions); s("sESPV",g_soundESPVehicles);
    s("sessTimer",g_sessionTimerEnabled);
    sf("accentR",g_menuAccentR); sf("accentG",g_menuAccentG); sf("accentB",g_menuAccentB);
    ss("spoofname",g_customNameBuf); ss("clantag",g_clanTagBuf);
    g_config->Save();
}

void LoadConfig() {
    if(!g_config)return; g_config->Load();
    auto b=[&](const char* k,bool d){return g_config->GetBool(k,d);};
    auto f=[&](const char* k,float d){return g_config->GetFloat(k,d);};
    auto i=[&](const char* k,int d){return g_config->GetInt(k,d);};
    auto st=[&](const char* k,const char* d){return g_config->GetString(k,d);};
    g_aimbotEnabled=b("aimbot",1); g_silentAimEnabled=b("silent",0); g_triggerbotEnabled=b("trigger",0);
    g_trackAimEnabled=b("tadv",0); g_headshotOnly=b("headshot",0); g_wallBang=b("wallbang",0);
    g_aimFOV=f("fov",30); g_aimSmooth=f("smooth",0.05f); g_targetBone=i("bone",Offsets::HEAD);
    g_silentFOV=f("sfov",15); g_silentHeadshot=b("shead",1); g_triggerDelay=i("tdelay",20);
    g_triggerAutoShoot=b("tauto",0); g_drawFovCircle=b("fovcirc",0); g_aimMode=i("aimode",0);
    g_bulletTracking=b("bullet",1);
    g_headOffset=f("headoff",0); g_aimMaxDistance=f("maxdist",5000);
    g_aimVisibleOnly=b("aimvo",0); g_riotShieldBypass=b("rsb",1); g_shieldBone=i("shbone",Offsets::LEFT_KNEE);
    g_autoScope=b("ascope",0); g_zoomFOV=b("zoomfov",0); g_zoomFactor=f("zoomfct",0.5f);
    g_autoShoot=b("autoshoot",0); g_recoilComp=b("recoilcomp",0); g_recoilStrength=f("recoilstr",1.0f);
    g_espEnabled=b("esp",1); g_wallhack=b("wallhack",1); g_vehicleESP=b("vesp",1); g_lootESP=b("lesp",1);
    g_espBoxes=b("ebox",1); g_espSnaplines=b("esnap",1); g_espHealth=b("ehealth",1); g_espNames=b("enames",1);
    g_espDistance=b("edist",1); g_espHeadCircle=b("ehead",1);
    g_espArmor=b("earm",1); g_espBoxFill=b("ebf",0); g_espBoxStyle=i("ebs",0); g_espTextBackground=b("etb",1); g_espDistanceFade=b("edf",1);
    g_skeletonEnabled=b("skel",1); g_skeletonBones=b("skelb",1); g_skeletonJoints=b("skelj",1);
    g_exfilESP=b("exfil",1); g_contractESP=b("contract",1); g_deadBodyESP=b("dead",1);
    g_strongholdESP=b("shESP",1); g_buyStationESP=b("bsESP",1); g_samSiteESP=b("sam",1);
    g_supplyDropESP=b("sdESP",1); g_bossESP=b("bossESP",1);
    g_worldHUD=b("hud",1); g_drawWatermark=b("wm",1); g_drawCompass=b("cmp",0); g_drawGameMode=b("gm",1); g_drawTimer=b("tm",1);
    g_visibleOnly=b("vo",0); g_visibilityIndicator=b("vi",0); g_grenadePrediction=b("np",0);
    g_weaponNames=b("wnames",1); g_squadCount=b("sqcount",1); g_itemRarity=b("irarity",1);
    g_noRecoil=b("recoil",1); g_noSpread=b("spread",1); g_rapidFire=b("rapid",0); g_rapidFireDelay=i("rdelay",10);
    g_fastReload=b("fastreload",0); g_reloadSpeed=f("rspd",0.25f);
    g_infiniteAmmo=b("ammo",0);
    g_weaponAmmoMod=i("ammmod",0);
    g_activeCamo=i("camo",-1);
    g_radarEnabled=b("radar",1); g_radarRotate=b("rrot",1); g_radarRange=f("rrange",300); g_radarSize=f("rsize",200); g_radarOpacity=f("ropac",0.55f);
    g_radarShowEnemies=b("ren",1); g_radarShowTeammates=b("rtm",1); g_radarShowVehicles=b("rveh",1); g_radarShowAI=b("rai",1);
    g_bhopEnabled=b("bhop",0); g_jumpSpam=b("jumpspam",0); g_superSlide=b("superslide",0); g_superStrafe=b("superstrafe",0); g_airStrafing=b("airstrafe",0);
    g_antiAimEnabled=b("aa",0); g_antiAimMode=i("aamode",0); g_spinSpeed=f("sspeed",20); g_jitterRange=i("jitter",30);
    g_thirdPerson=b("tp",0); g_thirdPersonDist=f("tdist",80); g_fovChanger=b("fovc",0); g_customFOV=f("cfov",90);
    g_nightVision=b("nv",0); g_thermalVision=b("th",0); g_freeCam=b("freecam",0);
    g_stealthEnabled=b("stealth",1); g_antiDebug=b("adbg",1); g_blacklistScan=b("blk",1);
    g_crosshairEnabled=b("crosshair",0); g_crosshairType=i("crtype",0); g_crosshairSize=f("crsize",12);
    g_crosshairThickness=f("crth",2); g_crosshairGap=f("crgap",4); g_playerListOpen=b("plist",0);
    g_crosshairOutline=b("crout",1);
    g_crosshairColor[0]=f("crcr",0); g_crosshairColor[1]=f("crcg",1); g_crosshairColor[2]=f("crcb",0); g_crosshairColor[3]=f("crca",1);
    g_fovCircleColor[0]=f("fccr",1); g_fovCircleColor[1]=f("fccg",1); g_fovCircleColor[2]=f("fccb",1); g_fovCircleColor[3]=f("fcca",0.25f);
    g_fovCircleThickness=f("fcth",1);
    g_vehicleSpeedBoost=b("vboost",0); g_vehicleGodMode=b("vgod",0); g_vehicleSpeedMult=f("vsmult",2);
    g_nameSpoofEnabled=b("nspoof",0); g_bypassProfanity=b("byp",1);
    g_spectatorEnabled=b("spe",1); g_spectatorAutoCloak=b("spa",1); g_accountHealthEnabled=b("ahealth",1);
    g_lootFilterEnabled=b("lfilter",1); g_autoPickupEnabled=b("apickup",0);
    g_soundESPEnabled=b("sESP",0); g_soundESPFootsteps=b("sESPF",1); g_soundESPGunshots=b("sESPG",1);
    g_soundESPExplosions=b("sESPE",1); g_soundESPVehicles=b("sESPV",1);
    g_sessionTimerEnabled=b("sessTimer",0);
    g_menuAccentR=f("accentR",0.00f); g_menuAccentG=f("accentG",0.85f); g_menuAccentB=f("accentB",1.00f);
    std::string nm = st("spoofname", "nxs_USER");
    if (!nm.empty()) strncpy_s(g_customNameBuf, nm.c_str(), sizeof(g_customNameBuf));
    std::string ct = st("clantag", "YK");
    if (!ct.empty()) strncpy_s(g_clanTagBuf, ct.c_str(), sizeof(g_clanTagBuf));
}

void ResetConfig() {
    if (!g_config) return;
    g_config->Clear();
    g_config->Save();
    LoadConfig();
    g_Debug.Log("[Config] Reset to defaults");
}

DWORD WINAPI MainThread(LPVOID) {
    g_Debug.Log("[Main] Starting...");

    g_protection = new StealthProtection(); g_protection->Initialize();
    g_mem = new MemoryManager();
    FindGameWindow();
    for (int tries = 0; !g_gameWindow && tries < 10; tries++) { Sleep(1000); FindGameWindow(); }
    if (!g_gameWindow) { delete g_mem; return 0; }
    if (g_mem->IsGameRunning()) { g_gameFound = true; g_gamePid = g_mem->GetPid(); g_gameBase = g_mem->GetBase(); }
    if (!CreateDeviceD3D(g_gameWindow)) { delete g_mem; return 0; }

    g_menu = new Menu(); SetupImGui();
    // Friend: g_auth = new YourAuthSystem(); here
    g_keybinds = new KeybindManager();
    g_nameSpoof = new NameSpoofer(g_mem, g_gameBase);
    g_spectator = new SpectatorTracker(g_mem, g_gameBase);
    g_accountHealth = new AccountHealth(g_mem, g_gameBase);
    g_lootFilter = new LootFilter(g_mem, g_gameBase);
    g_soundESP = new SoundESP(g_mem, g_gameBase);
    g_sessionTimer = new SessionTimer();
g_discordRPC = new DiscordRPC();
g_discordRPC->SetStatus("zor - the best cheat known :)", "dominating the lobby", "zor");
    g_keybinds->Add(Keybind("menu_toggle", "Menu Toggle", VK_INSERT, &g_menuOpen));
    g_keybinds->Add(Keybind("aimbot_toggle", "Aimbot", VK_XBUTTON1, &g_aimbotEnabled));
    g_keybinds->Add(Keybind("silent_aim", "Silent Aim", VK_F6, &g_silentAimEnabled));
    g_keybinds->Add(Keybind("triggerbot", "Triggerbot", VK_F7, &g_triggerbotEnabled));
    g_keybinds->Add(Keybind("anti_aim", "Anti-Aim", VK_F8, &g_antiAimEnabled));
    g_keybinds->Add(Keybind("esp_toggle", "ESP", VK_F9, &g_espEnabled));
    g_keybinds->Add(Keybind("radar_toggle", "Radar", VK_F10, &g_radarEnabled));
    g_keybinds->Add(Keybind("no_recoil", "No Recoil", VK_F5, &g_noRecoil));
    g_keybinds->Add(Keybind("stealth", "Stealth Mode", VK_F11, &g_stealthEnabled));
    g_esp = new ESP(g_mem, g_gameWindow);
    g_aimbot = new Aimbot(g_mem, g_gameBase);
    g_triggerbot = new Triggerbot(g_mem, g_gameBase);
    g_weaponExp = new WeaponExploits(g_mem, g_gameBase);
    g_weaponEditor = new WeaponEditor(g_mem, g_gameBase);
    g_camoChanger = new CamoChanger(g_mem, g_gameBase);
    g_silentAim = new SilentAim(g_mem, g_gameBase);
    g_antiAim = new AntiAim(g_mem, g_gameBase);
    g_radar = new Radar(g_mem, g_gameBase);
    g_triggerAdvanced = new TriggerbotAdvanced(g_mem, g_gameBase);
    g_worldVis = new WorldVisuals(g_mem, g_gameBase);
    g_camera = new Camera(g_mem, g_gameBase);
    g_nightVis = new NightVision(g_mem, g_gameBase);
    g_skeleton = new SkeletonESP(g_mem);
    g_gameWorld = new GameWorldESP(g_mem, g_gameBase);
    g_nameSys = new NameSystem(g_mem, g_gameBase);
    g_config = new ConfigSystem();
    g_filter = new FilterSystem();
    g_movement = new Movement(g_mem, g_gameBase);
    g_crosshair = new CustomCrosshair();
    g_fovRenderer = new FOVRenderer(g_mem, g_gameBase);
    g_grenadePred = new GrenadePrediction(g_mem, g_gameBase);
    g_playerList = new PlayerList(g_mem);
    g_vehicleHacks = new VehicleHacks(g_mem, g_gameBase);

    LoadConfig();

    g_menu->Init(
        &g_aimbotEnabled,&g_silentAimEnabled,&g_triggerbotEnabled,&g_trackAimEnabled,
        &g_headshotOnly,&g_wallBang,&g_aimFOV,&g_aimSmooth,&g_targetBone,
        &g_silentFOV,&g_silentHeadshot,&g_triggerDelay,&g_triggerAutoShoot,
        &g_drawFovCircle,&g_bulletTracking,&g_aimMode,
        &g_aimMaxDistance,
        &g_stickToTarget,&g_autoWall,&g_bestBone,&g_boneOverride,
        &g_boneOverrideKey,&g_boneOverrideId,&g_headOffset,
        &g_aimVisibleOnly,&g_riotShieldBypass,&g_shieldBone,
        &g_autoScope,&g_zoomFOV,&g_zoomFactor,&g_autoShoot,&g_recoilComp,&g_recoilStrength,
        &g_espEnabled,&g_wallhack,&g_vehicleESP,&g_lootESP,
        &g_espBoxes,&g_espSnaplines,&g_espHealth,&g_espNames,
        &g_espDistance,&g_espHeadCircle,
        &g_espArmor,&g_espBoxFill,&g_espBoxStyle,&g_espTextBackground,&g_espDistanceFade,
        &g_skeletonEnabled,&g_skeletonBones,&g_skeletonJoints,
        &g_exfilESP,&g_contractESP,&g_deadBodyESP,
        &g_strongholdESP,&g_buyStationESP,
        &g_samSiteESP,&g_supplyDropESP,&g_bossESP,
        &g_worldHUD,&g_drawWatermark,&g_drawCompass,&g_drawGameMode,&g_drawTimer,
        &g_visibleOnly,&g_visibilityIndicator,&g_grenadePrediction,
        &g_weaponNames,&g_squadCount,&g_itemRarity,
        &g_noRecoil,&g_noSpread,&g_rapidFire,&g_rapidFireDelay,
        &g_infiniteAmmo,&g_weaponAmmoMod,&g_fastReload,&g_reloadSpeed,
        &g_activeCamo,
        &g_radarEnabled,&g_radarRotate,&g_radarRange,&g_radarSize,&g_radarOpacity,
        &g_radarShowEnemies,&g_radarShowTeammates,&g_radarShowVehicles,&g_radarShowAI,
        &g_bhopEnabled,&g_jumpSpam,&g_superSlide,&g_superStrafe,&g_airStrafing,
        &g_antiAimEnabled,&g_antiAimMode,&g_spinSpeed,&g_jitterRange,
        &g_thirdPerson,&g_thirdPersonDist,&g_fovChanger,&g_customFOV,
        &g_nightVision,&g_thermalVision,&g_freeCam,
        &g_stealthEnabled,&g_antiDebug,&g_blacklistScan,
        &g_crosshairEnabled,&g_crosshairType,&g_crosshairSize,&g_crosshairThickness,&g_crosshairGap,
        &g_crosshairOutline, g_crosshairColor, g_fovCircleColor, &g_fovCircleThickness,
        &g_playerListOpen,
        &g_vehicleSpeedBoost,&g_vehicleGodMode,&g_vehicleSpeedMult,
        &g_nameSpoofEnabled,&g_bypassProfanity,g_customNameBuf,g_clanTagBuf,
        &g_spectatorEnabled,&g_spectatorAutoCloak,
        &g_accountHealthEnabled,
        &g_lootFilterEnabled,&g_autoPickupEnabled,
        &g_soundESPEnabled,&g_soundESPFootsteps,&g_soundESPGunshots,&g_soundESPExplosions,&g_soundESPVehicles,
        &g_sessionTimerEnabled,
        &g_menuAccentR,&g_menuAccentG,&g_menuAccentB
    );

    g_menu->SetConfigCallbacks(SaveConfig, LoadConfig, ResetConfig);
    g_menu->SetProfileCallbacks(
        [&]() { return g_config->ListProfiles(); },
        [&](const char* n) { g_config->LoadProfile(n); LoadConfig(); g_Debug.Log("[Config] Loaded profile: " + std::string(n)); },
        [&](const char* n) { SaveConfig(); g_config->SaveProfile(n); g_Debug.Log("[Config] Saved profile: " + std::string(n)); },
        [&](const char* n) { g_config->DeleteProfile(n); },
        [&]() -> const char* { static std::string c = g_config->GetCurrentProfile(); c = g_config->GetCurrentProfile(); return c.c_str(); }
    );

    MSG msg = {};
    while (g_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        if (g_protection && g_stealthEnabled) g_protection->Run();

        if (g_gameFound && g_mem) {
            UpdatePlayers(); UpdateVehicles();

            Vec3 localPos = g_mem->Read<Vec3>(g_gameBase + Offsets::LOCAL_POS);
            int localTeam = g_mem->Read<int>(g_gameBase + Offsets::PLAYER_TEAM);
            int localHealth = (int)g_mem->Read<float>(g_gameBase + Offsets::PLAYER_HEALTH);
            int localArmor = (int)g_mem->Read<float>(g_gameBase + Offsets::PLAYER_ARMOR);
            uintptr_t cameraBase = g_mem->Read<uintptr_t>(g_gameBase + Offsets::CAMERA_BASE);
            Matrix4x4 vm = {};
            if (cameraBase) vm = g_mem->Read<Matrix4x4>(cameraBase + 0x100);
            if (vm.IsIdentity()) vm = g_mem->Read<Matrix4x4>(g_gameBase + Offsets::CAMERA_MATRIX);
            if (!vm.IsIdentity()) cameraBase = (uintptr_t)1; // CAMERA_BASE is dead; gate ESP on valid view matrix
            RECT rc; GetWindowRect(g_gameWindow, &rc);
            int sw = rc.right - rc.left, sh = rc.bottom - rc.top;

            // ---- ESP ----
            g_esp->SetEnabled(g_espEnabled); g_esp->SetWallhack(g_wallhack);
            g_esp->SetVehicleESP(g_vehicleESP); g_esp->SetLootESP(g_lootESP);
            g_esp->SetSnaplines(g_espSnaplines); g_esp->SetHealth(g_espHealth);
            g_esp->SetArmor(g_espArmor);
            g_esp->SetNames(g_espNames); g_esp->SetDistance(g_espDistance);
            g_esp->SetBoxes(g_espBoxes); g_esp->SetHeadCircle(g_espHeadCircle);
            g_esp->SetBoxFill(g_espBoxFill); g_esp->SetBoxStyle(g_espBoxStyle);
            g_esp->SetTextBackground(g_espTextBackground); g_esp->SetDistanceFade(g_espDistanceFade);
            g_esp->SetWeaponNames(g_weaponNames); g_esp->SetSquadCount(g_squadCount);
            g_esp->SetItemRarity(g_itemRarity);
            g_esp->Render(g_players, g_vehicles, g_gameBase, localPos, localTeam);

            // ---- Skeleton ----
            if (g_skeletonEnabled && cameraBase) {
                g_skeleton->SetEnabled(true); g_skeleton->SetDrawBones(g_skeletonBones);
                g_skeleton->SetDrawJoints(g_skeletonJoints);
                g_skeleton->Render(g_players, vm, sw, sh, localTeam);
            }

            // ---- World ESP ----
            if (cameraBase) {
                g_gameWorld->SetEnabled(true); g_gameWorld->SetDrawExfils(g_exfilESP);
                g_gameWorld->SetDrawContracts(g_contractESP);
                g_gameWorld->SetDrawDeadBodies(g_deadBodyESP);
                g_gameWorld->SetDrawStrongholds(g_strongholdESP);
                g_gameWorld->SetDrawBuyStations(g_buyStationESP);
                g_gameWorld->SetDrawSamSites(g_samSiteESP);
                g_gameWorld->SetDrawSupplyDrops(g_supplyDropESP);
                g_gameWorld->SetDrawBosses(g_bossESP);
                g_gameWorld->Render(vm, sw, sh, localPos);
            }

            // ---- Radar ----
            if (g_radarEnabled) {
                g_radar->SetEnabled(true); g_radar->SetRotateWithPlayer(g_radarRotate);
                g_radar->SetRadarRange(g_radarRange); g_radar->SetRadarSize(g_radarSize);
                g_radar->SetShowEnemies(g_radarShowEnemies);
                g_radar->SetShowTeammates(g_radarShowTeammates);
                g_radar->SetShowVehicles(g_radarShowVehicles);
                g_radar->SetShowAI(g_radarShowAI);
                g_radar->Render(g_players, g_vehicles, localPos, localTeam, sw, sh);
            }

            // ---- HUD ----
            if (g_worldHUD) {
                g_worldVis->SetEnabled(true); g_worldVis->SetDrawGameMode(g_drawGameMode);
                g_worldVis->SetDrawWatermark(g_drawWatermark);
                g_worldVis->SetDrawTimer(g_drawTimer);
                g_worldVis->SetDrawCompass(g_drawCompass);
                g_worldVis->SetDrawLocalInfo(true);
                g_worldVis->RenderHUD(sw, sh, localPos, localHealth, localArmor);
                if (g_drawCompass) {
                    Vec3 va = g_mem->Read<Vec3>(g_gameBase + Offsets::VIEW_ANGLES);
                    g_worldVis->RenderCompass(sw, sh, va.y);
                }
            }

            // ---- Aimbot ----
            g_aimbot->SetEnabled(g_aimbotEnabled); g_aimbot->SetHeadshotOnly(g_headshotOnly);
            g_aimbot->SetWallBang(g_wallBang); g_aimbot->SetFOV(g_aimFOV);
            g_aimbot->SetSmoothness(g_aimSmooth); g_aimbot->SetAimMode(g_aimMode);
            g_aimbot->SetMaxDistance(g_aimMaxDistance);
            g_aimbot->SetBulletTracking(g_bulletTracking);
            g_aimbot->SetStickToTarget(g_stickToTarget); g_aimbot->SetAutoWall(g_autoWall);
            g_aimbot->SetBestBone(g_bestBone); g_aimbot->SetBoneOverride(g_boneOverride);
            g_aimbot->SetBoneOverrideKey(g_boneOverrideKey); g_aimbot->SetBoneOverrideId(g_boneOverrideId);
            g_aimbot->SetHeadOffset(g_headOffset);
            g_aimbot->SetVisibleOnly(g_aimVisibleOnly);
            g_aimbot->SetRiotShieldBypass(g_riotShieldBypass);
            g_aimbot->SetShieldBone(g_shieldBone);
            g_aimbot->SetAutoScope(g_autoScope);
            g_aimbot->SetZoomFOV(g_zoomFOV); g_aimbot->SetZoomFactor(g_zoomFactor);
            g_aimbot->SetAutoShoot(g_autoShoot);
            g_aimbot->SetRecoilComp(g_recoilComp); g_aimbot->SetRecoilStrength(g_recoilStrength);

            g_aimbot->Run(g_players, localPos, localTeam);

            // ---- Silent Aim ----
            if (g_silentAimEnabled) {
                g_silentAim->SetEnabled(true); g_silentAim->SetHeadshotOnly(g_silentHeadshot);
                g_silentAim->SetFOV(g_silentFOV); g_silentAim->SetWallBang(g_wallBang);
                g_silentAim->Run(g_players, localPos, localTeam);
            }

            // ---- Triggerbot ----
            g_triggerbot->SetEnabled(g_triggerbotEnabled); g_triggerbot->Run(localTeam);
            if (g_trackAimEnabled) {
                g_triggerAdvanced->SetEnabled(true); g_triggerAdvanced->SetDelay(g_triggerDelay);
                g_triggerAdvanced->SetHeadOnly(g_silentHeadshot);
                g_triggerAdvanced->SetAutoShoot(g_triggerAutoShoot);
                g_triggerAdvanced->Run(localTeam, g_players, localPos);
            }

            // ---- Weapon Exploits ----
            g_weaponExp->SetNoRecoil(g_noRecoil); g_weaponExp->SetNoSpread(g_noSpread);
            g_weaponExp->SetRapidFire(g_rapidFire);
            g_weaponExp->SetFastReload(g_fastReload); g_weaponExp->SetReloadSpeed(g_reloadSpeed);
            g_weaponExp->Run();

            // ---- Weapon Editor ----
            if (g_infiniteAmmo || g_weaponAmmoMod > 0) {
                g_weaponEditor->SetInfiniteAmmo(g_infiniteAmmo);
                g_weaponEditor->SetAmmoModifier(g_weaponAmmoMod);
                g_weaponEditor->Run();
            }

            // ---- Camo Changer ----
            if (g_activeCamo >= 0) { g_camoChanger->SetCamo(g_activeCamo); g_camoChanger->Run(); }

            // ---- Anti Aim ----
            if (g_antiAimEnabled) { g_antiAim->SetEnabled(true); g_antiAim->SetMode(g_antiAimMode);
                g_antiAim->SetSpinSpeed(g_spinSpeed); g_antiAim->SetJitterRange(g_jitterRange); g_antiAim->Run(); }

            // ---- Camera ----
            if (g_thirdPerson || g_fovChanger) { g_camera->SetEnabled(true);
                g_camera->SetThirdPerson(g_thirdPerson);
                g_camera->SetThirdPersonDistance(g_thirdPersonDist);
                g_camera->SetFOVChanger(g_fovChanger); g_camera->SetCustomFOV(g_customFOV); g_camera->Run(); }

            // ---- Night/Thermal ----
            if (g_nightVision || g_thermalVision) { g_nightVis->SetEnabled(true);
                g_nightVis->SetNightVision(g_nightVision); g_nightVis->SetThermal(g_thermalVision); g_nightVis->Run(); }
            else if (g_nightVis) g_nightVis->Reset();

            // ---- Movement ----
            g_movement->SetBHop(g_bhopEnabled);
            g_movement->SetJumpSpam(g_jumpSpam);
            g_movement->SetSuperSlide(g_superSlide);
            g_movement->SetSuperStrafe(g_superStrafe);
            g_movement->SetAirStrafing(g_airStrafing);
            g_movement->Run();

            // ---- Vehicle Hacks ----
            g_vehicleHacks->SetSpeedBoost(g_vehicleSpeedBoost);
            g_vehicleHacks->SetGodMode(g_vehicleGodMode);
            g_vehicleHacks->SetSpeedMultiplier(g_vehicleSpeedMult);
            g_vehicleHacks->Run();

            // ---- Spectator Tracker ----
            if (g_spectator && g_gameFound) {
                g_spectator->Update(localTeam,
                    g_aimbotEnabled, g_espEnabled, g_triggerbotEnabled);
                if (g_spectator->IsEnabled())
                    g_spectator->RenderHUD(sw, sh);
            }

            // ---- Account Health ----
            if (g_accountHealth && g_gameFound) {
                g_accountHealth->Update();
                g_accountHealth->RenderHUD(sw, sh);
            }

            // ---- Name Spoofer ----
            if (g_nameSpoof) {
                g_nameSpoof->SetEnabled(g_nameSpoofEnabled);
                g_nameSpoof->SetBypassProfanity(g_bypassProfanity);
                g_nameSpoof->SetCustomName(g_customNameBuf);
                g_nameSpoof->SetCustomClanTag(g_clanTagBuf);
                g_nameSpoof->Run();
            }

            // ---- Auto Loot ----
            if (g_autoPickupEnabled && g_lootFilter) {
                g_lootFilter->SetEnabled(true);
                g_lootFilter->SetAutoPickup(true);
                g_lootFilter->AutoPickup();
            }

            // ---- Sound ESP ----
            if (g_soundESPEnabled && g_soundESP && cameraBase) {
                g_soundESP->SetEnabled(true);
                g_soundESP->SetShowFootsteps(g_soundESPFootsteps);
                g_soundESP->SetShowGunshots(g_soundESPGunshots);
                g_soundESP->SetShowExplosions(g_soundESPExplosions);
                g_soundESP->SetShowVehicleSounds(g_soundESPVehicles);
                g_soundESP->Update(localPos);
                g_soundESP->Render(vm, sw, sh);
            }

            // ---- Crosshair ----
            g_crosshair->SetEnabled(g_crosshairEnabled); g_crosshair->SetType(g_crosshairType);
            g_crosshair->SetSize(g_crosshairSize); g_crosshair->SetThickness(g_crosshairThickness);
            g_crosshair->SetGap(g_crosshairGap); g_crosshair->SetOutline(g_crosshairOutline);
            g_crosshair->SetColor(g_crosshairColor);
            g_crosshair->Render(sw, sh);

            // ---- Overlays ----
            g_fovRenderer->SetDrawFovCircle(g_drawFovCircle); g_fovRenderer->SetFov(g_aimFOV);
            g_fovRenderer->SetThickness(g_fovCircleThickness); g_fovRenderer->SetColor(g_fovCircleColor);
            g_fovRenderer->SetVisibilityIndicator(g_visibilityIndicator);
            g_fovRenderer->RenderCrosshairIndicator(sw, sh, localTeam);

            if (g_drawFovCircle) {
                const float silentCol[4] = { 0.2f, 1.0f, 0.4f, 0.35f };
                const float triggerCol[4] = { 1.0f, 0.65f, 0.1f, 0.35f };
                if (g_silentAimEnabled) g_fovRenderer->RenderFovCircle(g_silentFOV, g_fovCircleThickness, silentCol, sw, sh);
                if (g_triggerbotEnabled || g_trackAimEnabled) g_fovRenderer->RenderFovCircle(5.0f, g_fovCircleThickness, triggerCol, sw, sh);
            }

            if (g_grenadePrediction) { g_grenadePred->SetEnabled(true); g_grenadePred->Render(sw, sh); }
        }

        // ---- Session Timer ----
        if (g_sessionTimer) {
            g_sessionTimer->SetEnabled(g_sessionTimerEnabled);
            g_sessionTimer->Render(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        }

        // ---- IMGUI ----
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

        // Friend: insert auth check + login window render here

        if (g_menu) {
            g_menu->RenderOverlays(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
            std::string st = g_gameFound ? "Connected" : "Waiting";
            int hp = 0, ar = 0, lt = 0;
            if (g_mem && g_gameFound) { hp = (int)g_mem->Read<float>(g_gameBase + Offsets::PLAYER_HEALTH); ar = (int)g_mem->Read<float>(g_gameBase + Offsets::PLAYER_ARMOR); lt = g_mem->Read<int>(g_gameBase + Offsets::PLAYER_TEAM); }
            g_menu->SetPlayerData(&g_players, lt);
            g_menu->Render(&g_menuOpen, st, (int)g_players.size(), (int)g_vehicles.size(), hp, ar);
        }

        if (g_playerListOpen && g_playerList) {
            g_playerList->SetEnabled(true);
            int lt = 0; if (g_mem && g_gameFound) lt = g_mem->Read<int>(g_gameBase + Offsets::PLAYER_TEAM);
            g_playerList->Render(g_players, lt);
        }

        if (g_menu && g_menu->IsKeybindEditorOpen() && g_keybinds) {
            g_keybinds->RenderEditor();
        }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pMainRenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        if (g_keybinds) g_keybinds->Poll();
        if (g_discordRPC) g_discordRPC->Update();
        Sleep(16);
    }

    SaveConfig(); g_Debug.Log("[Main] Shutdown");
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); CleanupDeviceD3D();
    if(g_protection){g_protection->Shutdown();delete g_protection;}
    delete g_mem; delete g_esp; delete g_aimbot; delete g_triggerbot;
    delete g_weaponExp; delete g_weaponEditor; delete g_silentAim; delete g_antiAim;
    delete g_radar; delete g_triggerAdvanced; delete g_worldVis; delete g_camera;
    delete g_nightVis; delete g_skeleton; delete g_gameWorld; delete g_nameSys;
    delete g_config; delete g_filter; delete g_movement; delete g_crosshair;
    delete g_fovRenderer; delete g_grenadePred; delete g_playerList; delete g_vehicleHacks;
    delete g_nameSpoof; delete g_spectator; delete g_accountHealth;
    delete g_lootFilter; delete g_soundESP;
    if (g_discordRPC) { g_discordRPC->Shutdown(); delete g_discordRPC; }
    delete g_keybinds; // Friend: delete YourAuthSystem here
    delete g_menu;
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    // No thread creation here: Ricochet hooks NtCreateThreadEx. The driver
    // calls ZorMain() directly on the hijacked game thread instead.
    if (reason == DLL_PROCESS_ATTACH) { /* CRT init only */ }
    return TRUE;
}

// Entry point for the kernel manual-mapper: runs the whole cheat on the
// hijacked game thread, never returns.
extern "C" __declspec(dllexport) void ZorMain() {
    MainThread(0);
}
