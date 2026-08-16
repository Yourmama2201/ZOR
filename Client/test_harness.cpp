// nxs UI Test Harness - renders menu, crosshair, FOV circle offline (no game)
// Build with build_harness.bat

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <string>
#include <cmath>
#include <cstdarg>
#include <random>
#include <vector>

#include "offsets.hpp"
#include "math.hpp"
#include "menu.hpp"
#include "Features/Misc/crosshair.hpp"
#include "Features/Visuals/fov_circle.hpp"
#include "Features/Misc/config_system.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ---- ImGui platform backend hook ----
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ===================== GLOBALS (mirrors main.cpp) =====================
bool  g_aimbotEnabled = true, g_silentAimEnabled = false,
      g_triggerbotEnabled = false, g_trackAimEnabled = false,
      g_headshotOnly = false, g_wallBang = false,
      g_drawFovCircle = false, g_bulletTracking = true;
float g_aimFOV = 30.0f, g_aimSmooth = 0.05f, g_silentFOV = 15.0f;
int   g_targetBone = Offsets::HEAD, g_triggerDelay = 20,
      g_aimPriority = 0, g_aimMode = 0;
bool  g_silentHeadshot = true, g_triggerAutoShoot = false;
bool  g_stickToTarget = false, g_autoWall = false, g_bestBone = false, g_boneOverride = false;
int   g_boneOverrideKey = VK_XBUTTON2, g_boneOverrideId = Offsets::HEAD;
float g_headOffset = 0.0f;
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
      g_infiniteAmmo = false, g_instantSwap = false,
      g_autoFire = false, g_instantReload = false;
int   g_rapidFireDelay = 10, g_weaponAmmoMod = 0;
int   g_espBoxStyle = 0;

bool  g_radarEnabled = true, g_radarRotate = true,
      g_radarShowEnemies = true, g_radarShowTeammates = true,
      g_radarShowVehicles = true, g_radarShowAI = true;
float g_radarRange = 300.0f, g_radarSize = 200.0f, g_radarOpacity = 0.55f;

bool  g_bhopEnabled = false, g_jumpSpam = false, g_superSlide = false, g_superStrafe = false, g_airStrafing = false;

bool  g_antiAimEnabled = false, g_antiAimOnFire = true;
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
bool  g_nameRotatorEnabled = false;
bool  g_spectatorEnabled = true, g_spectatorAutoCloak = true;
bool  g_accountHealthEnabled = true;
bool  g_lootFilterEnabled = true, g_autoPickupEnabled = false;
bool  g_soundESPEnabled = false, g_soundESPFootsteps = true, g_soundESPGunshots = true,
      g_soundESPExplosions = true, g_soundESPVehicles = true;

bool g_menuOpen = true;
bool g_sessionTimerEnabled = false;
float g_menuAccentR = 0.95f, g_menuAccentG = 0.35f, g_menuAccentB = 0.00f;

char g_customNameBuf[32] = "nxs_USER";
char g_clanTagBuf[16] = "YK";

Menu* g_menu = nullptr;
CustomCrosshair* g_crosshair = nullptr;
FOVRenderer* g_fovRenderer = nullptr;
ConfigSystem* g_config = nullptr;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pMainRenderTargetView = nullptr;
HWND g_hwnd = nullptr;

void CreateRenderTarget() {
    ID3D11Texture2D* buf = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&buf));
    if (buf) { g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &g_pMainRenderTargetView); buf->Release(); }
}
void CleanupRenderTarget() { if (g_pMainRenderTargetView) { g_pMainRenderTargetView->Release(); g_pMainRenderTargetView = nullptr; } }
void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {}; sd.BufferCount = 2; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width = 1280; sd.BufferDesc.Height = 720;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl; const D3D_FEATURE_LEVEL fla[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT r = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, fla, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext);
    if (r != S_OK) return false;
    CreateRenderTarget();
    return true;
}

// Save the swapchain backbuffer to a BMP via ReadBack (immune to desktop overlays)
static int g_shotCount = 0;

static void DbgLog(const char* fmt, ...) {
    FILE* dbg = nullptr;
    fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
    if (dbg) {
        va_list args; va_start(args, fmt);
        vfprintf(dbg, fmt, args);
        va_end(args);
        fclose(dbg);
    }
}

static LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xE06D7363) return EXCEPTION_CONTINUE_SEARCH;
    DbgLog("VEH exception code=%08X addr=%p\n", ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress);
    HMODULE self = GetModuleHandleW(NULL);
    DbgLog("  exe base=%p offset=%p\n", (void*)self,
        (void*)((uintptr_t)ep->ExceptionRecord->ExceptionAddress - (uintptr_t)self));
    DbgLog("  regs rbx=%p rsi=%p rdi=%p rcx=%p rdx=%p rbp=%p rsp=%p\n",
        (void*)ep->ContextRecord->Rbx, (void*)ep->ContextRecord->Rsi,
        (void*)ep->ContextRecord->Rdi, (void*)ep->ContextRecord->Rcx,
        (void*)ep->ContextRecord->Rdx, (void*)ep->ContextRecord->Rbp,
        (void*)ep->ContextRecord->Rsp);
    {
        uintptr_t b = (uintptr_t)ep->ContextRecord->Rbx;
        uintptr_t s = (uintptr_t)ep->ContextRecord->Rsi;
        for (int k = 0; k < 6; k++) {
            void* val = (void*)0;
            memcpy(&val, (void*)(b + 0x200 + (size_t)k * 8), 8);
            DbgLog("  [rbx+0x%X] = %p\n", 0x200 + k * 8, val);
        }
        DbgLog("  [rsi] deref: %s\n", s ? (IsBadReadPtr((void*)s, 1) ? "BAD" : "OK") : "NULL");
    }
    void* stack[32];
    USHORT cnt = CaptureStackBackTrace(0, 32, stack, NULL);
    for (USHORT i = 0; i < cnt; i++)
        DbgLog("  stack[%d]=%p\n", i, stack[i]);
    return EXCEPTION_CONTINUE_SEARCH;
}
void SaveSwapchainShot() {
    FILE* dbg = nullptr;
    fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
    if (dbg) { fprintf(dbg, "shot called, count=%d\n", g_shotCount); }
    ID3D11Texture2D* back = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (dbg) fprintf(dbg, "GetBuffer hr=%08X back=%p\n", (unsigned)hr, (void*)back);
    if (!back) { if (dbg) { fclose(dbg); } return; }
    D3D11_TEXTURE2D_DESC d; back->GetDesc(&d);
    if (dbg) fprintf(dbg, "desc %ux%u fmt=%d mips=%u arr=%u sample=%u\n", d.Width, d.Height, d.Format, d.MipLevels, d.ArraySize, d.SampleDesc.Count);
    D3D11_TEXTURE2D_DESC sd = d; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.Usage = D3D11_USAGE_STAGING; sd.MiscFlags = 0;
    ID3D11Texture2D* stage = nullptr;
    HRESULT hr2 = g_pd3dDevice->CreateTexture2D(&sd, nullptr, &stage);
    if (dbg) fprintf(dbg, "CreateStaging hr=%08X\n", (unsigned)hr2);
    if (FAILED(hr2)) { back->Release(); if (dbg) fclose(dbg); return; }
    g_pd3dDeviceContext->CopyResource(stage, back);
    D3D11_MAPPED_SUBRESOURCE map;
    HRESULT hr3 = g_pd3dDeviceContext->Map(stage, 0, D3D11_MAP_READ, 0, &map);
    if (dbg) fprintf(dbg, "Map hr=%08X\n", (unsigned)hr3);
    if (SUCCEEDED(hr3)) {
        char path[MAX_PATH];
        sprintf_s(path, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\hcap_%d.bmp", g_shotCount);
        unsigned int w = d.Width, h = d.Height, row = w * 4;
        int rowPad = (4 - (row % 4)) % 4;
        unsigned int dataSize = (row + rowPad) * h;
        unsigned int fileSize = 54 + dataSize;
        FILE* f = nullptr;
        fopen_s(&f, path, "wb");
        if (f) {
            BITMAPFILEHEADER bfh = { 0x4D42, fileSize, 0, 0, 54 };
            BITMAPINFOHEADER bih = { 40, (LONG)w, (LONG)h, 1, 32, 0, dataSize, 2835, 2835, 0, 0 };
            fwrite(&bfh, 1, 14, f); fwrite(&bih, 1, 40, f);
            const BYTE* src = (const BYTE*)map.pData;
            for (int y = (int)h - 1; y >= 0; y--) { // BMP is bottom-up
                const BYTE* p = src + (size_t)y * map.RowPitch;
                for (unsigned int x = 0; x < w; x++) {
                    fputc(p[x*4+0], f); fputc(p[x*4+1], f); fputc(p[x*4+2], f); fputc(p[x*4+3], f);
                }
                for (int i = 0; i < rowPad; i++) fputc(0, f);
            }
            fclose(f);
        }
        g_pd3dDeviceContext->Unmap(stage, 0);
        if (dbg) fprintf(dbg, "wrote %s\n", path);
        g_shotCount++;
    } else if (dbg) fprintf(dbg, "Map FAILED hr=%08X\n", (unsigned)hr3);
    stage->Release(); back->Release();
    if (dbg) { fclose(dbg); }
}

// Minimal config hooks so the menu's Save/Load/Reset buttons work offline
void SaveConfigHarness() {
    if (!g_config) return;
    g_config->SetBool("crosshair", g_crosshairEnabled);
    g_config->SetBool("crout", g_crosshairOutline);
    g_config->SetInt("crtype", g_crosshairType);
    g_config->SetFloat("crsize", g_crosshairSize);
    g_config->SetFloat("crth", g_crosshairThickness);
    g_config->SetFloat("crgap", g_crosshairGap);
    g_config->SetBool("fovcirc", g_drawFovCircle);
    g_config->SetFloat("fov", g_aimFOV);
    g_config->Save();
}
void LoadConfigHarness() {
    if (!g_config) return;
    g_config->Load();
    g_crosshairEnabled = g_config->GetBool("crosshair", false);
    g_crosshairOutline = g_config->GetBool("crout", true);
    g_crosshairType = g_config->GetInt("crtype", 0);
    g_crosshairSize = g_config->GetFloat("crsize", 12.0f);
    g_crosshairThickness = g_config->GetFloat("crth", 2.0f);
    g_crosshairGap = g_config->GetFloat("crgap", 4.0f);
    g_drawFovCircle = g_config->GetBool("fovcirc", false);
    g_aimFOV = g_config->GetFloat("fov", 30.0f);
}
void ResetConfigHarness() {
    if (!g_config) return;
    g_config->Clear(); g_config->Save(); LoadConfigHarness();
}

// ---- Fake background scene: animated enemy boxes + grid, drawn directly ----
static std::vector<Vec2> g_fakeEnemies;
static void InitFakeScene() {
    std::mt19937 rng(1337);
    for (int i = 0; i < 14; i++) {
        float x = 100.0f + (float)(rng() % 1080);
        float y = 120.0f + (float)(rng() % 480);
        g_fakeEnemies.push_back(Vec2(x, y));
    }
}
static void RenderFakeScene(ImDrawList* draw, int sw, int sh, float t) {
    // grid
    ImU32 gridCol = IM_COL32(20, 24, 36, 60);
    for (int x = 0; x < sw; x += 80) draw->AddLine(ImVec2((float)x, 0), ImVec2((float)x, (float)sh), gridCol);
    for (int y = 0; y < sh; y += 80) draw->AddLine(ImVec2(0, (float)y), ImVec2((float)sw, (float)y), gridCol);

    int i = 0;
    for (auto& e : g_fakeEnemies) {
        float bob = sinf(t * 1.5f + i * 0.8f) * 14.0f;
        float cx = e.x + bob * 0.5f, cy = e.y + bob;
        float hp = 0.45f + 0.5f * (0.5f + 0.5f * sinf(t * 0.7f + i));
        float hw = 32.0f, hh = 72.0f;
        ImColor boxCol = (i % 3 == 0) ? ImColor(255, 0, 0, 255) : ImColor(255, 180, 0, 200);
        draw->AddRect(ImVec2(cx - hw / 2, cy - hh), ImVec2(cx + hw / 2, cy), boxCol, 1.0f, 0, 1.0f);
        draw->AddRectFilled(ImVec2(cx - hw / 2 - 4, cy), ImVec2(cx - hw / 2 - 2, cy - hh * hp),
            ImColor(1.0f - hp, hp, 0.0f, 1.0f));
        char nm[24]; sprintf_s(nm, "BOT%d", i);
        draw->AddText(ImVec2(cx - 14, cy - hh - 14), ImColor(255, 255, 255, 200), nm);
        i++;
    }

    // local player HUD block (fake)
    draw->AddRectFilled(ImVec2((float)sw - 210, 10), ImVec2((float)sw - 10, 82), ImColor(0, 0, 0, 110), 6.0f);
    draw->AddText(ImVec2((float)sw - 200, 16), ImColor(0, 255, 255, 180), "ZORMenu v4.0 | TEST HARNESS");
    draw->AddText(ImVec2((float)sw - 200, 38), ImColor(0, 200, 255, 200), "Mode: DMZ");
    draw->AddText(ImVec2((float)sw - 200, 58), ImColor(0, 255, 0, 200), "HP: 100 | Armor: 150");
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && g_pd3dDevice != nullptr) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    { FILE* dbg = nullptr; fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "w");
      if (dbg) { fprintf(dbg, "winmain entered\n"); fclose(dbg); } }
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc; wc.hInstance = hInstance;
    wc.lpszClassName = L"nxs_test_harness";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"nxs UI Test Harness", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL);
    if (!g_hwnd) return 1;

    if (!CreateDeviceD3D(g_hwnd)) { CleanupDeviceD3D(); return 1; }
    AddVectoredExceptionHandler(1, VectoredHandler);
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);

    ImGui::CreateContext();
    { FILE* dbg = nullptr; fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
      if (dbg) { fprintf(dbg, "ctx created\n"); fclose(dbg); } }
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    { FILE* dbg = nullptr; fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
      if (dbg) { fprintf(dbg, "imgui inited\n"); fclose(dbg); } }

    // load emoji font like the real cheat
    ImFontConfig baseCfg; baseCfg.SizePixels = 13.0f;
    ImGui::GetIO().Fonts->AddFontDefault(&baseCfg);
    ImFontConfig cfg; cfg.MergeMode = true; cfg.GlyphOffset.y = 4.0f; cfg.SizePixels = 13.0f;
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 13.0f, &cfg);
    { FILE* dbg = nullptr; fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
      if (dbg) { fprintf(dbg, "fonts loaded\n"); fclose(dbg); } }

    g_menu = new Menu();
    g_crosshair = new CustomCrosshair();
    g_fovRenderer = new FOVRenderer(nullptr, 0);
    g_config = new ConfigSystem("nxs_harness_config.json");
    { FILE* dbg = nullptr; fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
      if (dbg) { fprintf(dbg, "objects created\n"); fclose(dbg); } }

    g_menu->Init(
        &g_aimbotEnabled,&g_silentAimEnabled,&g_triggerbotEnabled,&g_trackAimEnabled,
        &g_headshotOnly,&g_wallBang,&g_aimFOV,&g_aimSmooth,&g_targetBone,
        &g_silentFOV,&g_silentHeadshot,&g_triggerDelay,&g_triggerAutoShoot,
        &g_drawFovCircle,&g_bulletTracking,&g_aimMode,
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
        &g_infiniteAmmo,&g_instantSwap,&g_autoFire,&g_instantReload,&g_weaponAmmoMod,
        &g_radarEnabled,&g_radarRotate,&g_radarRange,&g_radarSize,&g_radarOpacity,
        &g_radarShowEnemies,&g_radarShowTeammates,&g_radarShowVehicles,&g_radarShowAI,
        &g_bhopEnabled,&g_jumpSpam,&g_superSlide,&g_superStrafe,&g_airStrafing,
        &g_antiAimEnabled,&g_antiAimMode,&g_spinSpeed,&g_jitterRange,&g_antiAimOnFire,
        &g_thirdPerson,&g_thirdPersonDist,&g_fovChanger,&g_customFOV,
        &g_nightVision,&g_thermalVision,&g_freeCam,
        &g_stealthEnabled,&g_antiDebug,&g_blacklistScan,
        &g_crosshairEnabled,&g_crosshairType,&g_crosshairSize,&g_crosshairThickness,&g_crosshairGap,
        &g_crosshairOutline, g_crosshairColor, g_fovCircleColor, &g_fovCircleThickness,
        &g_playerListOpen,
        &g_vehicleSpeedBoost,&g_vehicleGodMode,&g_vehicleSpeedMult,
        &g_nameSpoofEnabled,&g_bypassProfanity,g_customNameBuf,g_clanTagBuf,
        &g_nameRotatorEnabled,
        &g_spectatorEnabled,&g_spectatorAutoCloak,
        &g_accountHealthEnabled,
        &g_lootFilterEnabled,&g_autoPickupEnabled,
        &g_soundESPEnabled,&g_soundESPFootsteps,&g_soundESPGunshots,&g_soundESPExplosions,&g_soundESPVehicles,
        &g_sessionTimerEnabled,
        &g_menuAccentR,&g_menuAccentG,&g_menuAccentB
    );
    g_menu->Setup();
    g_menu->SetConfigCallbacks(SaveConfigHarness, LoadConfigHarness, ResetConfigHarness);
    LoadConfigHarness();
    InitFakeScene();

    MSG msg = {};
    bool running = true;
    int g_frameCount = 0;
    {
        FILE* dbg = nullptr;
        fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
        if (dbg) { fprintf(dbg, "loop started\n"); fclose(dbg); }
    }
    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        g_frameCount++;

        try {
        if ((g_frameCount % 60) == 0) {
            FILE* dbg = nullptr;
            fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
            if (dbg) { fprintf(dbg, "frame %d\n", g_frameCount); fclose(dbg); }
        }

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

        float t = ImGui::GetTime();
        int sw = 1280, sh = 720;
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        RenderFakeScene(draw, sw, sh, t);

        // crosshair + FOV circle (same call flow as the cheat)
        g_crosshair->SetEnabled(g_crosshairEnabled); g_crosshair->SetType(g_crosshairType);
        g_crosshair->SetSize(g_crosshairSize); g_crosshair->SetThickness(g_crosshairThickness);
        g_crosshair->SetGap(g_crosshairGap); g_crosshair->SetOutline(g_crosshairOutline);
        g_crosshair->SetColor(g_crosshairColor);
        g_crosshair->Render(sw, sh);

        g_fovRenderer->SetDrawFovCircle(g_drawFovCircle); g_fovRenderer->SetFov(g_aimFOV);
        g_fovRenderer->SetThickness(g_fovCircleThickness); g_fovRenderer->SetColor(g_fovCircleColor);
        g_fovRenderer->SetVisibilityIndicator(false);
        g_fovRenderer->RenderCrosshairIndicator(sw, sh, 0);

        if (g_drawFovCircle) {
            const float silentCol[4] = { 0.2f, 1.0f, 0.4f, 0.35f };
            const float triggerCol[4] = { 1.0f, 0.65f, 0.1f, 0.35f };
            if (g_silentAimEnabled) g_fovRenderer->RenderFovCircle(g_silentFOV, g_fovCircleThickness, silentCol, sw, sh);
            if (g_triggerbotEnabled || g_trackAimEnabled) g_fovRenderer->RenderFovCircle(5.0f, g_fovCircleThickness, triggerCol, sw, sh);
        }

        g_menu->Render(&g_menuOpen, "Connected", 42, 7, 100, 150);

        static float lastTabSwitch = 0.0f;
        if (g_shotCount < 9 && ImGui::GetTime() - lastTabSwitch > 2.0f) {
            lastTabSwitch = ImGui::GetTime();
            int t = g_menu->GetActiveTab() + 1;
            if (t > 8) t = 0;
            g_menu->SetActiveTab(t);
            { FILE* dbg = nullptr;
              fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
              if (dbg) { fprintf(dbg, "switched to tab %d\n", t); fclose(dbg); } }
        }

        if ((g_frameCount % 20) == 0) {
            FILE* dbg = nullptr;
            fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
            if (dbg) { fprintf(dbg, "frame %d tab %d\n", g_frameCount, g_menu->GetActiveTab()); fclose(dbg); }
        }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pMainRenderTargetView, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);

        static float lastShot = 0.0f;
        if (g_shotCount < 6 && ImGui::GetTime() - lastShot > 2.0f) {
            lastShot = ImGui::GetTime();
            SaveSwapchainShot();
        }
        } catch (...) {
            FILE* dbg = nullptr;
            fopen_s(&dbg, "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\harness_dbg.txt", "a");
            if (dbg) {
                fprintf(dbg, "EXCEPTION at frame %d tab %d\n",
                    g_frameCount, g_menu ? g_menu->GetActiveTab() : -1);
                fclose(dbg);
            }
            running = false;
        }
    }

    SaveConfigHarness();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}