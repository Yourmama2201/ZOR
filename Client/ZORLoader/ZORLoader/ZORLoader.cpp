#include <windows.h>
#include <windowsx.h>
#include <wintrust.h>
#include <softpub.h>
#include <shellapi.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <ctime>
#include <tlhelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <winhttp.h>
#include <gdiplus.h>
#include <random>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

using namespace Gdiplus;

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

// ============================================================================
//  Disguise helpers (random identity each launch)
// ============================================================================
static const wchar_t* g_legitNames[] = {
    L"RuntimeBroker", L"SearchHost", L"svchost", L"conhost", L"dwm",
    L"ApplicationFrameHost", L"backgroundTaskHost", L"dllhost", L"WmiPrvSE",
    L"ShellExperienceHost", L"StartMenuExperienceHost", L"TextInputHost",
    L"SystemSettings", L"SecurityHealthSystray", L"SgrmBroker", L"fontdrvhost",
    L"Widgets", L"LogonUI", L"CompatTelRunner", L"MicrosoftEdgeUpdate"
};

static std::wstring g_fakeName;
static std::wstring g_fakeClass;
static std::wstring g_fakeExeName;

static std::wstring RandomWStringFrom(const wchar_t* const* pool, int count) {
    std::mt19937 rng((unsigned)GetTickCount() ^ ((unsigned)GetCurrentProcessId() << 16));
    int idx = (int)(rng() % (unsigned)count);
    std::wstring s = pool[idx];
    // append a numeric suffix so it looks like a real spawned instance
    s += std::to_wstring(1000 + (rng() % 9000));
    return s;
}

static void InitDisguise() {
    // Fixed identity every launch - no random rename. The loader always runs
    // as ZORLoader.exe with a stable window/log name.
    g_fakeName = L"ZOR Loader v8.0";
    g_fakeClass = L"ZOR";
    g_fakeExeName = L"ZORLoader.exe";
}

// Self-rename: copy this exe to a random fake name and relaunch, then kill the
// original. Keeps a different process name in Task Manager each run.
static void SelfRenameAndRelaunch() {
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);

    std::wstring dir = selfPath;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);

    std::wstring newPath = dir + g_fakeExeName;

    // Already running under a fake name? Don't loop.
    std::wstring curName = selfPath;
    size_t curSlash = curName.find_last_of(L"\\/");
    if (curSlash != std::wstring::npos) curName = curName.substr(curSlash + 1);
    if (curName == g_fakeExeName) return;

    if (!CopyFileW(selfPath, newPath.c_str(), FALSE)) return;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpFile = newPath.c_str();
    sei.nShow = SW_SHOW;
    if (ShellExecuteExW(&sei)) {
        ExitProcess(0);
    }
}

static HICON LoadFakeSystemIcon() {
    // Pull a real system icon from shell32.dll so it looks like a Windows utility.
    HICON hIcon = ExtractIconW(GetModuleHandleW(NULL), L"shell32.dll", 70);
    if (!hIcon) hIcon = LoadIconW(NULL, IDI_APPLICATION);
    return hIcon;
}

// ============================================================================
//  Local file helpers (load driver + client from DMZ_FILES, hide on disk)
// ============================================================================
// The loader prefers local built artifacts but falls back to downloading the
// driver / kdmapper / client DLL from the public GitHub repo (raw download), so
// a fresh machine only needs the loader exe.

// Write bytes to a hidden temp file with a random-looking name. Returns full path
// or empty string on failure. Caller should delete the file after use.
static std::wstring WriteHiddenTemp(const std::vector<BYTE>& data, const wchar_t* ext) {
    wchar_t tempDir[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempDir)) return L"";

    std::wstring name = L"sys" + std::to_wstring(GetTickCount()) + L"_" +
        std::to_wstring(rand() % 100000) + ext;
    std::wstring full = std::wstring(tempDir) + name;

    HANDLE hFile = CreateFileW(full.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return L"";
    DWORD written = 0;
    BOOL w = WriteFile(hFile, data.data(), (DWORD)data.size(), &written, NULL);
    CloseHandle(hFile);
    if (!w || written != data.size()) { DeleteFileW(full.c_str()); return L""; }
    return full;
}

// Read a whole file into a byte vector. Returns false on any failure.
static bool ReadFileToVector(const char* path, std::vector<BYTE>& out) {
    out.clear();
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) { CloseHandle(hFile); return false; }
    out.resize(fileSize);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, out.data(), fileSize, &bytesRead, NULL);
    CloseHandle(hFile);
    if (!ok || bytesRead != fileSize) { out.clear(); return false; }
    return true;
}

// ============================================================================
//  Debug logger (log -> UI panel)
// ============================================================================
static std::vector<std::wstring> g_log;
static CRITICAL_SECTION g_logLock;
static DWORD g_lastLogTick = 0;

class DebugLogger {
private:
    std::string logFile;
public:
    DebugLogger() {
        // Log to a fixed path next to the exe so every run is captured
        // regardless of the working directory the user launched from.
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring dir = exePath;
        size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
        std::wstring wlog = dir + L"ZORLoader_runtime.log";
        logFile.assign(wlog.begin(), wlog.end());
        std::ofstream ofs(logFile, std::ios::trunc);
        ofs << "ZOR LOADER LOG\n";
        ofs.close();
        Log("[INIT] ZOR Loader v8.0");
    }

    void Log(const std::string& message) {
        std::ofstream ofs(logFile, std::ios::app);
        if (ofs.is_open()) { ofs << message << "\n"; ofs.flush(); ofs.close(); }
        EnterCriticalSection(&g_logLock);
        if (g_log.size() > 200) g_log.erase(g_log.begin());
        std::wstring w(message.begin(), message.end());
        g_log.push_back(w);
        LeaveCriticalSection(&g_logLock);
        g_lastLogTick = GetTickCount();
    }

    void LogError(const std::string& function, DWORD error) {
        char buf[256];
        sprintf_s(buf, "[ERROR] %s failed: %d (0x%X)", function.c_str(), error, error);
        Log(buf);
    }
};

static DebugLogger* g_Debug = nullptr;

// ============================================================================
//  GitHub raw download: fetches the driver / kdmapper / client DLL from the
//  public repo so a fresh machine needs only this single loader exe. Uses the
//  same WinHTTP transport as the client auth system. Falls back to local files
//  automatically when offline.
// ============================================================================
static const wchar_t* g_ghHost = L"raw.githubusercontent.com";
static const wchar_t* g_ghPathPrefix = L"/Yourmama2201/ZOR/master/";

// Download a file from the public repo. Returns false on any failure.
static bool DownloadFromGitHub(const char* repoPath, std::vector<BYTE>& out) {
    out.clear();
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) ZORLoader/8.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return false;

    bool ok = false;
    HINTERNET hConnect = WinHttpConnect(hSession, g_ghHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect) {
        std::wstring path = std::wstring(g_ghPathPrefix) +
            std::wstring(repoPath, repoPath + strlen(repoPath));
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL,
            NULL, NULL, WINHTTP_FLAG_SECURE);
        if (hRequest) {
            BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD status = 0, statusLen = sizeof(status);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    DWORD avail = 0, read = 0;
                    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                        size_t oldSize = out.size();
                        out.resize(oldSize + avail);
                        if (!WinHttpReadData(hRequest, out.data() + oldSize, avail, &read)) break;
                        out.resize(oldSize + read);
                        if (read == 0) break;
                    }
                    ok = (out.size() > 0);
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return ok;
}

// Try local path first, then GitHub. Returns false if both fail.
static bool AcquireFile(const char* localPath, const char* repoPath, std::vector<BYTE>& out,
    const char* label) {
    if (ReadFileToVector(localPath, out)) {
        g_Debug->Log(std::string("Loaded ") + label + " from local: " + localPath);
        return true;
    }
    g_Debug->Log(std::string("Local ") + label + " not found, downloading from GitHub...");
    if (DownloadFromGitHub(repoPath, out)) {
        g_Debug->Log(std::string("[+] Downloaded ") + label + " from GitHub (" +
            std::to_string(out.size()) + " bytes)");
        return true;
    }
    g_Debug->LogError(std::string(label) + " fetch failed", 0);
    return false;
}

// Copy the on-screen console contents to the Windows clipboard.
static void CopyConsoleToClipboard(HWND hwnd) {
    EnterCriticalSection(&g_logLock);
    std::wstring all;
    for (size_t i = 0; i < g_log.size(); i++) {
        all += g_log[i];
        all += L"\r\n";
    }
    LeaveCriticalSection(&g_logLock);
    if (all.empty()) return;

    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (all.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        wchar_t* dst = (wchar_t*)GlobalLock(hMem);
        if (dst) {
            wcscpy_s(dst, all.size() + 1, all.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
    if (g_Debug) g_Debug->Log("[+] Console copied to clipboard");
}

// ============================================================================
//  kdmapper: map unsigned driver via a signed-but-vulnerable driver (BYOVD).
//  kdmapper.exe embeds iqvw64e.sys itself. It is dropped with the target .sys
//  in one hidden temp dir, run, and the \\.\ZOR device is polled; everything
//  is deleted when done. No SCM, no test signing required.
// ============================================================================
static bool ZORDeviceExists() {
    HANDLE h = CreateFileW(L"\\\\.\\ZOR", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

static bool WriteAllBytes(const std::wstring& path, const std::vector<BYTE>& data) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL w = WriteFile(hFile, data.data(), (DWORD)data.size(), &written, NULL);
    CloseHandle(hFile);
    return w && written == data.size();
}

static bool MapViaKdmapper(const std::vector<BYTE>& kdmapperData,
                           const std::vector<BYTE>& driverData) {
    wchar_t tempDir[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempDir)) return false;

    // Unique hidden subfolder.
    std::wstring dir = std::wstring(tempDir) + L"ns" + std::to_wstring(GetTickCount()) +
        L"_" + std::to_wstring(rand() % 100000) + L"\\";
    if (!CreateDirectoryW(dir.c_str(), NULL)) return false;
    SetFileAttributesW(dir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    std::wstring kdmPath = dir + L"kdmapper.exe";
    std::wstring drvPath = dir + L"nxs_drv.sys";

    bool ok = WriteAllBytes(kdmPath, kdmapperData) &&
              WriteAllBytes(drvPath, driverData);
    if (!ok) {
        DeleteFileW(kdmPath.c_str()); DeleteFileW(drvPath.c_str());
        RemoveDirectoryW(dir.c_str());
        return false;
    }

    // Keep the driver mapped (no --free) so \\.\ZOR stays resident for inject.
    // --copy-header gives a valid driver header which keeps our own PE checks happy.
    std::wstring cmd = L"\"" + kdmPath + L"\" --copy-header \"" + drvPath + L"\"";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    BOOL launched = CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, dir.c_str(), &si, &pi);
    if (!launched) {
        g_Debug->LogError("kdmapper CreateProcessW", GetLastError());
        DeleteFileW(kdmPath.c_str()); DeleteFileW(drvPath.c_str());
        RemoveDirectoryW(dir.c_str());
        return false;
    }

    // Wait for \\.\ZOR to appear (kdmapper loads, maps, calls DriverEntry).
    bool mapped = false;
    for (int i = 0; i < 120; i++) {
        if (ZORDeviceExists()) { mapped = true; break; }
        DWORD rc = WaitForSingleObject(pi.hProcess, 500);
        if (rc == WAIT_OBJECT_0) break;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Cleanup: remove kdmapper + the driver file it mapped from temp.
    DeleteFileW(kdmPath.c_str());
    DeleteFileW(drvPath.c_str());
    RemoveDirectoryW(dir.c_str());
    g_Debug->Log(mapped ? "[+] Driver mapped via kdmapper" : "[!] kdmapper did not produce \\\\.\\ZOR");
    return mapped;
}

// ============================================================================
//  UI state
// ============================================================================
static const int WIN_W = 880;
static const int WIN_H = 640;

static const Color ACCENT(255, 0, 229, 255);        // neon cyan
static const Color ACCENT_DIM(200, 0, 229, 255);
static const Color NEON_PINK(255, 255, 0, 128);      // neon magenta
static const Color NEON_PURPLE(255, 124, 0, 255);
static const Color BG_TOP(255, 8, 8, 22);
static const Color BG_BOT(255, 22, 12, 46);
static const Color CARD_BG(190, 24, 20, 52);
static const Color CARD_BORDER(150, 0, 229, 255);
static const Color TEXT_MAIN(255, 235, 235, 240);
static const Color TEXT_DIM(255, 160, 160, 180);
static const Color GREEN(255, 60, 220, 110);
static const Color RED(255, 240, 70, 70);
static const Color YELLOW(255, 240, 200, 70);

enum StepState { ST_WAIT, ST_ACTIVE, ST_OK, ST_FAIL };

struct StatusCard {
    const wchar_t* title;
    StepState state;
    const wchar_t* detail;
};

static StatusCard g_cards[3] = {
    { L"DRIVER", ST_WAIT, L"Not loaded" },
    { L"PROCESS", ST_WAIT, L"Not found" },
    { L"INJECT", ST_WAIT, L"Idle" },
};

static float g_pulse = 0.0f;
static bool g_working = false;
static bool g_injected = false;
static bool g_btnHover = false;
static bool g_btnDown = false;
static bool g_copyHover = false;
static bool g_copyDown = false;
static bool g_closeHover = false;
static bool g_closeDown = false;
static bool g_helpHover = false;
static bool g_helpDown = false;
static bool g_showHelp = false;
static const int TITLEBAR_H = 34;
static Font* g_fontTitle = NULL;
static Font* g_fontBig = NULL;
static Font* g_fontNormal = NULL;
static Font* g_fontSmall = NULL;
static Font* g_fontMono = NULL;

// Gaming platform selector.
enum GamePlatform { PLAT_BATTLE, PLAT_STEAM, PLAT_XBOX };
static GamePlatform g_platform = PLAT_BATTLE;
static int g_platHover = -1;
static int g_platDown = -1;

// Floating particle field.
struct Particle { float x, y, r, speed, drift, alpha; };
static Particle g_particles[28];
static bool g_particlesInit = false;

// ============================================================================
//  Debug logger (log -> UI panel)
// ============================================================================
#define IOCTL_INJECT_ALLOC CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_INJECT_EXEC  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_READ_MEMORY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _INJECT_ALLOC_REQUEST {
    HANDLE ProcessId;
    SIZE_T SizeOfImage;
    PVOID RemoteBase;
    NTSTATUS ErrorStatus;
} INJECT_ALLOC_REQUEST, * PINJECT_ALLOC_REQUEST;

typedef struct _INJECT_EXEC_REQUEST {
    HANDLE ProcessId;
    PVOID RemoteBase;
    NTSTATUS ErrorStatus;
} INJECT_EXEC_REQUEST, * PINJECT_EXEC_REQUEST;

typedef struct _MEMORY_REQUEST {
    HANDLE ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;
    SIZE_T Size;
} MEMORY_REQUEST, * PMEMORY_REQUEST;

// ============================================================================
//  Core logic (unchanged from v7.1)
// ============================================================================
DWORD FindProcess(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, name) == 0) { DWORD pid = pe.th32ProcessID; CloseHandle(snap); return pid; } }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

PVOID ReadDLL(const char* path, SIZE_T* outSize) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { g_Debug->LogError("CreateFileA", GetLastError()); return NULL; }
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) { CloseHandle(hFile); return NULL; }
    PVOID dllData = VirtualAlloc(NULL, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!dllData) { CloseHandle(hFile); return NULL; }
    DWORD bytesRead;
    if (!ReadFile(hFile, dllData, fileSize, &bytesRead, NULL)) {
        VirtualFree(dllData, 0, MEM_RELEASE); CloseHandle(hFile); return NULL;
    }
    CloseHandle(hFile);
    *outSize = fileSize;
    return dllData;
}

bool LoadDriver(const char* driverPath) {
    wchar_t wDriverPath[MAX_PATH];
    mbstowcs_s(NULL, wDriverPath, MAX_PATH, driverPath, MAX_PATH);
    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) { g_Debug->LogError("OpenSCManagerW", GetLastError()); return false; }
    SC_HANDLE hService = OpenServiceW(hSCManager, L"nxs_drv", SERVICE_ALL_ACCESS);
    if (!hService) {
        hService = CreateServiceW(hSCManager, L"nxs_drv", L"System Network Driver",
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL, wDriverPath, NULL, NULL, NULL, NULL, NULL);
        if (!hService) { g_Debug->LogError("CreateServiceW", GetLastError()); CloseServiceHandle(hSCManager); return false; }
    }
    if (!StartServiceW(hService, 0, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            g_Debug->LogError("StartServiceW", err);
            CloseServiceHandle(hService); CloseServiceHandle(hSCManager);
            return false;
        }
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

PVOID InjectViaDriver(DWORD pid, PVOID dllData, SIZE_T dllSize) {
    HANDLE hDevice = CreateFileW(L"\\\\.\\ZOR", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) { g_Debug->LogError("CreateFileW \\\\.\\ZOR", GetLastError()); return NULL; }

    // Parse the local DLL image headers to drive the manual map.
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)dllData;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { CloseHandle(hDevice); return NULL; }
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)dllData + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { CloseHandle(hDevice); return NULL; }

    // Phase 1: allocate a remote RWX region of SizeOfImage in the target.
    INJECT_ALLOC_REQUEST allocReq = {};
    allocReq.ProcessId = (HANDLE)(ULONG_PTR)pid;
    allocReq.SizeOfImage = nt->OptionalHeader.SizeOfImage;
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(hDevice, IOCTL_INJECT_ALLOC, &allocReq, sizeof(allocReq),
        &allocReq, sizeof(allocReq), &bytesReturned, NULL);
    if (!ok || !allocReq.RemoteBase) {
        char b[96];
        sprintf_s(b, "Alloc: ioctl=%d lasterr=%u status=0x%08X", ok ? 1 : 0, GetLastError(),
            (unsigned long)allocReq.ErrorStatus);
        g_Debug->Log(b);
        CloseHandle(hDevice);
        return NULL;
    }
    PVOID remoteBase = allocReq.RemoteBase;
    g_Debug->Log("[+] Allocated 0x" + std::to_string((unsigned long long)remoteBase));

    // Phase 2: write the DLL image in chunks (headers + each section) via IOCTL_WRITE_MEMORY.
    const SIZE_T chunkSize = 0x4000; // 16KB per request, well under buffer limits
    struct { DWORD pid; ULONG_PTR addr; PVOID buf; SIZE_T size; } wr = {};
    wr.pid = pid;

    auto writeRemote = [&](ULONG_PTR remoteAddr, const BYTE* src, SIZE_T len) -> bool {
        for (SIZE_T off = 0; off < len; ) {
            SIZE_T n = (len - off > chunkSize) ? chunkSize : (len - off);
            SIZE_T reqSize = sizeof(MEMORY_REQUEST) + n;
            std::vector<BYTE> buf(reqSize);
            PMEMORY_REQUEST mr = (PMEMORY_REQUEST)buf.data();
            mr->ProcessId = (HANDLE)(ULONG_PTR)pid;
            mr->Address = remoteAddr + off;
            mr->Size = n;
            memcpy(buf.data() + sizeof(MEMORY_REQUEST), src + off, n);
            DWORD br = 0;
            if (!DeviceIoControl(hDevice, IOCTL_WRITE_MEMORY, buf.data(), (DWORD)reqSize,
                buf.data(), (DWORD)reqSize, &br, NULL)) {
                char b[96];
                sprintf_s(b, "Write 0x%llX+0x%X failed lasterr=%u", (unsigned long long)remoteAddr,
                    (unsigned)off, GetLastError());
                g_Debug->Log(b);
                return false;
            }
            off += n;
        }
        return true;
    };

    // Headers -> base.
    SIZE_T headerSize = nt->OptionalHeader.SizeOfHeaders;
    if (!writeRemote((ULONG_PTR)remoteBase, (const BYTE*)dllData, headerSize)) {
        CloseHandle(hDevice); return NULL;
    }

    // Sections -> base + VirtualAddress.
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (!sec->SizeOfRawData) continue;
        if (!writeRemote((ULONG_PTR)remoteBase + sec->VirtualAddress,
            (const BYTE*)dllData + sec->PointerToRawData, sec->SizeOfRawData)) {
            CloseHandle(hDevice); return NULL;
        }
    }
    g_Debug->Log("[+] DLL image written to target");

    // Phase 3: relocations + imports + entry.
    INJECT_EXEC_REQUEST execReq = {};
    execReq.ProcessId = (HANDLE)(ULONG_PTR)pid;
    execReq.RemoteBase = remoteBase;
    ok = DeviceIoControl(hDevice, IOCTL_INJECT_EXEC, &execReq, sizeof(execReq),
        &execReq, sizeof(execReq), &bytesReturned, NULL);
    if (!ok || !NT_SUCCESS(execReq.ErrorStatus)) {
        char b[160];
        sprintf_s(b, "Exec: ioctl=%d lasterr=%u ErrorStatus=0x%08X", ok ? 1 : 0, GetLastError(),
            (unsigned long)execReq.ErrorStatus);
        g_Debug->Log(b);
        CloseHandle(hDevice);
        return NULL;
    }
    CloseHandle(hDevice);
    return remoteBase;
}

bool ResolvePath(const char* relativePath, char* outPath, size_t outSize) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir = exePath;
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
    std::string dirA(WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, NULL, 0, NULL, NULL), 0);
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, &dirA[0], (int)dirA.size(), NULL, NULL);
    dirA.pop_back();
    std::string candidate = dirA + relativePath;
    if (GetFileAttributesA(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
        strncpy_s(outPath, outSize, candidate.c_str(), _TRUNCATE);
        return true;
    }
    return false;
}

const char* FindExisting(const char* fallback, const char* path1, const char* path2 = nullptr) {
    static char buf[2][MAX_PATH];
    static int idx = 0;
    if (ResolvePath(path1, buf[idx], MAX_PATH)) return buf[idx];
    if (path2 && ResolvePath(path2, buf[idx], MAX_PATH)) return buf[idx];
    return fallback;
}

// ============================================================================
//  Inject worker thread
// ============================================================================
static const wchar_t* g_gameLaunchPath = L"C:\\Program Files (x86)\\Call of Duty Modern Warfare II\\_retail_\\cod22-cod.exe";

static const wchar_t* PlatformNames[3] = { L"Battle.net", L"Steam", L"Xbox PC/App" };

static void ResolvePlatformLaunchPath(GamePlatform p, wchar_t* out, size_t cap) {
    wcscpy_s(out, cap, g_gameLaunchPath);
    const wchar_t* cand[3];
    int n = 0;
    switch (p) {
        case PLAT_BATTLE:
            cand[n++] = L"C:\\Program Files (x86)\\Call of Duty Modern Warfare II\\_retail_\\cod22-cod.exe";
            break;
        case PLAT_STEAM:
            cand[n++] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Call of Duty HQ\\_retail_\\cod22-cod.exe";
            cand[n++] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Call of Duty\\_retail_\\cod22-cod.exe";
            break;
        case PLAT_XBOX:
            cand[n++] = L"C:\\XboxGames\\Call of Duty\\Content\\_retail_\\cod22-cod.exe";
            cand[n++] = L"C:\\Program Files\\WindowsApps\\Call of Duty\\_retail_\\cod22-cod.exe";
            break;
    }
    for (int i = 0; i < n; i++) {
        if (GetFileAttributesW(cand[i]) != INVALID_FILE_ATTRIBUTES) {
            wcscpy_s(out, cap, cand[i]);
            return;
        }
    }
}

static bool LaunchGame() {
    wchar_t exe[MAX_PATH] = {};
    ResolvePlatformLaunchPath(g_platform, exe, MAX_PATH);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) return false;

    // Let the OS resolve the per-user Steam/Battle.net context so the game boots
    // normally (launcher associates the exe with your account).
    HINSTANCE r = ShellExecuteW(NULL, L"open", exe, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)r) > 32;
}

DWORD WINAPI InjectThread(LPVOID) {
    g_working = true;
    g_injected = false;

    char platBuf[64];
    sprintf_s(platBuf, "Platform: %ls", PlatformNames[g_platform]);
    g_Debug->Log(std::string(platBuf));
    wchar_t launchBuf[MAX_PATH];
    ResolvePlatformLaunchPath(g_platform, launchBuf, MAX_PATH);
    char lpBuf[MAX_PATH + 16];
    sprintf_s(lpBuf, "Launch path: %ls", launchBuf);
    g_Debug->Log(std::string(lpBuf));

    // Load driver + kdmapper + client from LOCAL built artifacts first, then
    // fall back to the public GitHub repo so a fresh machine only needs the
    // loader exe.
    std::vector<BYTE> remoteDrv, kdmData, remoteDll;
    bool haveDrvBytes = false, haveKdmapper = false, haveRemoteDll = false;

    {
        const char* p = FindExisting(
            "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Driver\\nxs_drv.sys",
            "..\\..\\..\\Driver\\nxs_drv.sys");
        if (AcquireFile(p, "Driver/nxs_drv.sys", remoteDrv, "driver")) {
            haveDrvBytes = true;
        }
    }
    {
        const char* p = FindExisting(
            "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Tools\\kdmapper\\kdmapper.exe",
            "..\\..\\..\\Tools\\kdmapper\\kdmapper.exe");
        if (AcquireFile(p, "Tools/kdmapper/kdmapper.exe", kdmData, "kdmapper")) {
            haveKdmapper = true;
        }
    }

    std::string dllPath;
    {
        const char* p = FindExisting(
            "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\x64\\Release\\ZORClient.dll",
            "..\\..\\..\\Client\\x64\\Release\\ZORClient.dll",
            "ZORClient.dll");
        dllPath = p;
        if (AcquireFile(dllPath.c_str(), "Client/x64/Release/ZORClient.dll", remoteDll, "client DLL")) {
            haveRemoteDll = true;
        }
    }
    const wchar_t* targetProcess = L"cod22-cod.exe";

    // Step 1: driver
    g_cards[0].state = ST_ACTIVE; g_cards[0].detail = L"Loading...";

    bool driverReady = false;
    std::wstring droppedDriver;

    // Preferred: kdmapper (BYOVD) -- works without test signing, driver stays in
    // memory, all files deleted. kdmapper.exe embeds iqvw64e.sys itself.
    if (haveDrvBytes && haveKdmapper) {
        g_Debug->Log("Mapping driver via kdmapper (BYOVD)...");
        driverReady = MapViaKdmapper(kdmData, remoteDrv);
        if (driverReady) {
            g_cards[0].state = ST_OK; g_cards[0].detail = L"Mapped";
            g_Debug->Log("[+] Driver mapped via kdmapper");
        } else {
            g_Debug->Log("[!] kdmapper failed, trying SCM fallback...");
        }
    } else {
        g_Debug->Log("kdmapper files unavailable, using SCM (needs test signing)");
    }

    // Fallback: classic SCM service load (requires test signing enabled).
    if (!driverReady) {
        if (haveDrvBytes) {
            droppedDriver = WriteHiddenTemp(remoteDrv, L".sys");
            if (droppedDriver.empty()) {
                g_Debug->Log("[!] Driver temp write failed, falling back to local");
                haveDrvBytes = false;
            }
        }
        char driverPath[MAX_PATH] = {};
        if (haveDrvBytes) {
            size_t conv = 0;
            wcstombs_s(&conv, driverPath, MAX_PATH, droppedDriver.c_str(), MAX_PATH);
            g_Debug->Log("Driver dropped to: " + std::string(driverPath) + " (hidden)");
        } else {
            const char* p = FindExisting(
                "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Driver\\nxs_drv.sys",
                "..\\..\\..\\Driver\\nxs_drv.sys",
                "..\\..\\..\\Driver2.0\\x64\\Release\\nxs_drv.sys");
            strncpy_s(driverPath, MAX_PATH, p, _TRUNCATE);
        }
        g_Debug->Log("Loading driver: " + std::string(driverPath));
        Sleep(200);
        if (LoadDriver(driverPath)) {
            driverReady = true;
            g_cards[0].state = ST_OK; g_cards[0].detail = L"Loaded";
            g_Debug->Log("[+] Driver loaded");
        } else {
            const char* driverPath2 = FindExisting(
                "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Driver2.0\\x64\\Release\\nxs_drv.sys",
                "..\\..\\..\\Driver2.0\\x64\\Release\\nxs_drv.sys",
                "..\\..\\..\\Driver\\nxs_drv.sys");
            g_Debug->Log("Retrying with Driver2.0...");
            Sleep(300);
            if (LoadDriver(driverPath2)) {
                driverReady = true;
                g_cards[0].state = ST_OK; g_cards[0].detail = L"Loaded (2.0)";
                g_Debug->Log("[+] Driver loaded (Driver2.0)");
            }
        }
    }

    if (!driverReady) {
        g_cards[0].state = ST_FAIL; g_cards[0].detail = L"Failed";
        g_Debug->Log("[!] Driver load failed");
        if (!droppedDriver.empty()) DeleteFileW(droppedDriver.c_str());
        g_working = false;
        return 1;
    }

    // Driver is running; the temp copy is no longer needed.
    if (!droppedDriver.empty()) {
        DeleteFileW(droppedDriver.c_str());
        g_Debug->Log("[-] Deleted dropped driver copy");
    }

    // Step 2: process
    g_cards[1].state = ST_ACTIVE; g_cards[1].detail = L"Searching...";
    g_Debug->Log("Looking for " + std::string(targetProcess, targetProcess + wcslen(targetProcess)) + "...");
    DWORD pid = 0;
    for (int i = 0; i < 120 && !pid; i++) {
        pid = FindProcess(targetProcess);
        if (pid) break;
        Sleep(1000);
    }
    if (!pid) {
        const wchar_t* altNames[] = { L"cod.exe", L"cod22-cod.exe", L"codhq-cod.exe",
            L"ModernWarfare.exe", L"mw22-cod.exe", L"COD.exe" };
        for (auto& name : altNames) { pid = FindProcess(name); if (pid) break; }
    }
    if (!pid) {
        // Diagnostic: list what IS running so we can see the mismatch.
        g_cards[1].state = ST_FAIL; g_cards[1].detail = L"Not found";
        g_Debug->Log("[!] Game process not found");
        g_Debug->Log("[!] Checked: cod22-cod.exe, cod.exe, codhq-cod.exe, ModernWarfare.exe, mw22-cod.exe");
        g_Debug->Log("[!] Running processes:");
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(snap, &pe)) {
                do {
                    std::wstring n(pe.szExeFile);
                    if (n.find(L"cod") != std::wstring::npos ||
                        n.find(L"MW") != std::wstring::npos ||
                        n.find(L"Battle") != std::wstring::npos ||
                        n.find(L"Steam") != std::wstring::npos ||
                        n.find(L"activision") != std::wstring::npos) {
                        std::string a(n.begin(), n.end());
                        g_Debug->Log("  RUNNING: " + a + " (PID " + std::to_string(pe.th32ProcessID) + ")");
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
        g_working = false;
        return 1;
    }
    g_cards[1].state = ST_OK;
    char pidBuf[32]; sprintf_s(pidBuf, "PID %d", pid);
    g_cards[1].detail = L"Found";
    g_Debug->Log("[+] Process found PID=" + std::to_string(pid));

    // Step 3: inject
    g_cards[2].state = ST_ACTIVE; g_cards[2].detail = L"Injecting...";
    SIZE_T dllSize = 0;
    PVOID dllData = NULL;

    if (haveRemoteDll) {
        g_Debug->Log("Remote DLL: " + std::to_string(remoteDll.size()) + " bytes");
        dllData = VirtualAlloc(NULL, remoteDll.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (dllData) {
            memcpy(dllData, remoteDll.data(), remoteDll.size());
            dllSize = remoteDll.size();
            remoteDll.clear();
        }
    } else {
        g_Debug->Log("Reading DLL: " + dllPath);
        dllData = ReadDLL(dllPath.c_str(), &dllSize);
    }
    if (!dllData) {
        g_cards[2].state = ST_FAIL; g_cards[2].detail = L"Read failed";
        g_Debug->Log("[!] Failed to get DLL bytes");
        g_working = false;
        return 1;
    }
    g_Debug->Log("[+] DLL " + std::to_string(dllSize) + " bytes");
    Sleep(150);
    PVOID remoteBase = InjectViaDriver(pid, dllData, dllSize);
    VirtualFree(dllData, 0, MEM_RELEASE);
    if (!remoteBase) {
        g_cards[2].state = ST_FAIL; g_cards[2].detail = L"Inject failed";
        g_Debug->Log("[!] Injection failed");
        g_working = false;
        return 1;
    }
    g_cards[2].state = ST_OK; g_cards[2].detail = L"Loaded";
    g_injected = true;
    g_Debug->Log("[+] ZOR injected @ 0x" + std::to_string((unsigned long long)remoteBase));
    g_Debug->Log("[+] ALL DONE");
    g_working = false;
    return 0;
}

// ============================================================================
//  GDI+ drawing helpers
// ============================================================================
static void FillRoundRect(Graphics& g, const RectF& r, float rad, const Color& col) {
    GraphicsPath path;
    float d = rad * 2.0f;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
    SolidBrush br(col);
    g.FillPath(&br, &path);
}

static void StrokeRoundRect(Graphics& g, const RectF& r, float rad, const Color& col, float w = 1.0f) {
    GraphicsPath path;
    float d = rad * 2.0f;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
    Pen pen(col, w);
    g.DrawPath(&pen, &path);
}

static void GText(Graphics& g, const wchar_t* txt, const Font& f, const RectF& r, const Color& col,
    StringAlignment halign = StringAlignmentNear, StringAlignment valign = StringAlignmentCenter) {
    StringFormat sf;
    sf.SetAlignment(halign);
    sf.SetLineAlignment(valign);
    SolidBrush br(col);
    g.DrawString(txt, -1, &f, r, &sf, &br);
}

static Color StepColor(StepState s, bool& pulse) {
    pulse = false;
    switch (s) {
        case ST_WAIT:  return Color(200, 110, 110, 120);
        case ST_ACTIVE: pulse = true; return ACCENT;
        case ST_OK:    return GREEN;
        case ST_FAIL:  return RED;
    }
    return Color(200, 110, 110, 120);
}

// Soft radial glow (layered translucent ellipses -> cheap fake blur).
static void DrawGlow(Graphics& g, float cx, float cy, float radius, const Color& col) {
    const int layers = 5;
    for (int i = layers; i >= 1; i--) {
        float r = radius * ((float)i / (float)layers);
        BYTE a = (BYTE)((col.GetAlpha() * (layers - i + 1)) / (layers * 2));
        SolidBrush br(Color(a, col.GetR(), col.GetG(), col.GetB()));
        g.FillEllipse(&br, cx - r, cy - r, r * 2.0f, r * 2.0f);
    }
}

// Hollow progress ring.
static void DrawRing(Graphics& g, float cx, float cy, float radius, float thick,
    float start, float sweep, const Color& col) {
    Pen pen(col, thick);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    g.DrawArc(&pen, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, start, sweep);
}

// Diagonal shimmer sweep used on the inject button.
static void DrawShimmer(Graphics& g, const RectF& r, float phase) {
    float x0 = r.X - r.Width + fmodf(phase, 1.0f) * (r.Width * 2.5f);
    LinearGradientBrush sh(PointF(x0, r.Y), PointF(x0 + r.Width * 0.6f, r.Y + r.Height),
        Color(60, 255, 255, 255), Color(0, 255, 255, 255));
    GraphicsPath path;
    float d = r.Height * 2.0f;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
    g.FillPath(&sh, &path);
}

// Hexagonal emblem badge with a letter.
static void DrawEmblem(Graphics& g, float cx, float cy, float size, const Color& edge,
    const Color& fillTop, const Color& fillBot, wchar_t letter) {
    PointF pts[6];
    for (int i = 0; i < 6; i++) {
        float ang = (float)(i * 60 - 90) * 3.14159265f / 180.0f;
        pts[i].X = cx + cosf(ang) * size;
        pts[i].Y = cy + sinf(ang) * size;
    }
    GraphicsPath hex;
    hex.AddPolygon(pts, 6);
    LinearGradientBrush fill(PointF(cx - size, cy - size), PointF(cx + size, cy + size), fillTop, fillBot);
    g.FillPath(&fill, &hex);
    Pen pen(edge, 1.8f);
    g.DrawPath(&pen, &hex);

    Gdiplus::FontFamily ff(L"Segoe UI");
    Font fnt(&ff, size * 1.1f, FontStyleBold, UnitPixel);
    RectF tr(cx - size, cy - size * 1.05f, size * 2.0f, size * 2.1f);
    GText(g, &letter, fnt, tr, Color(255, 245, 245, 255), StringAlignmentCenter, StringAlignmentCenter);
}

// Status pills / badges.
static void DrawPill(Graphics& g, const RectF& r, const Color& fill, const Color& border,
    const wchar_t* txt, const Font& f, const Color& txtCol) {
    FillRoundRect(g, r, r.Height / 2.0f, fill);
    StrokeRoundRect(g, r, r.Height / 2.0f, border, 1.0f);
    GText(g, txt, f, r, txtCol, StringAlignmentCenter, StringAlignmentCenter);
}

static const wchar_t* CardGlyphs[3] = { L"DRV", L"EXE", L"DLL" };

// ============================================================================
//  Window proc
// ============================================================================
static void DrawScene(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);
    Bitmap backBmp(W, H, PixelFormat32bppARGB);
    Graphics g(&backBmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    // ---- background gradient + vignette ----
    RectF full(0, 0, (float)W, (float)H);
    LinearGradientBrush bg(Rect(0, 0, W, H), BG_TOP, BG_BOT, 90.0f);
    g.FillRectangle(&bg, full);

    // fine grid lines for depth
    Pen gridPen(Color(24, 40, 60, 110), 1.0f);
    for (int x = 24; x < W; x += 32) g.DrawLine(&gridPen, (float)x, 0.0f, (float)x, (float)H);
    for (int y = 24; y < H; y += 32) g.DrawLine(&gridPen, 0.0f, (float)y, (float)W, (float)y);

    // ---- drifting glow orbs ----
    struct Orb { float cx, cy, r; DWORD a; Color c; float sx, sy; };
    Orb orbs[5] = {
        { (float)W * 0.16f, (float)H * 0.14f, 300.0f, 55, NEON_PURPLE, 0.40f, 0.22f },
        { (float)W * 0.82f, (float)H * 0.12f, 260.0f, 45, ACCENT,      0.55f, 0.30f },
        { (float)W * 0.60f, (float)H * 0.85f, 320.0f, 40, NEON_PINK,   0.50f, 0.18f },
        { (float)W * 0.30f, (float)H * 0.72f, 220.0f, 35, ACCENT,      0.35f, 0.40f },
        { (float)W * 0.90f, (float)H * 0.55f, 240.0f, 40, NEON_PURPLE, 0.60f, 0.25f },
    };
    for (int i = 0; i < 5; i++) {
        float ox = orbs[i].cx + sinf(g_pulse * orbs[i].sx + i * 1.7f) * 70.0f;
        float oy = orbs[i].cy + cosf(g_pulse * orbs[i].sy + i * 2.3f) * 50.0f;
        LinearGradientBrush orb(PointF(ox - orbs[i].r, oy - orbs[i].r),
            PointF(ox + orbs[i].r, oy + orbs[i].r),
            Color((BYTE)orbs[i].a, orbs[i].c.GetR(), orbs[i].c.GetG(), orbs[i].c.GetB()),
            Color(0, orbs[i].c.GetR(), orbs[i].c.GetG(), orbs[i].c.GetB()));
        g.FillEllipse(&orb, RectF(ox - orbs[i].r, oy - orbs[i].r, orbs[i].r * 2.0f, orbs[i].r * 2.0f));
    }

    // ---- rising particle motes ----
    if (!g_particlesInit) {
        std::mt19937 rng((unsigned)GetTickCount());
        for (int i = 0; i < 28; i++) {
            g_particles[i].x = (float)(rng() % W);
            g_particles[i].y = (float)(rng() % H);
            g_particles[i].r = 1.0f + (float)(rng() % 4) * 0.7f;
            g_particles[i].speed = 12.0f + (float)(rng() % 40);
            g_particles[i].drift = (float)(rng() % 30) - 15.0f;
            g_particles[i].alpha = 30.0f + (float)(rng() % 70);
        }
        g_particlesInit = true;
    }
    for (int i = 0; i < 28; i++) {
        g_particles[i].y -= g_particles[i].speed * 0.016f;
        g_particles[i].x += sinf(g_pulse * 1.2f + i) * g_particles[i].drift * 0.01f;
        if (g_particles[i].y < -10.0f) { g_particles[i].y = (float)H + 10.0f; g_particles[i].x = (float)(rand() % W); }
        SolidBrush pb(Color((BYTE)g_particles[i].alpha, 140, 220, 255));
        g.FillEllipse(&pb, g_particles[i].x, g_particles[i].y, g_particles[i].r * 2.0f, g_particles[i].r * 2.0f);
    }

    // neon scanline sweep (subtle)
    float sweepY = fmodf(g_pulse * 90.0f, (float)H + 200.0f) - 100.0f;
    LinearGradientBrush sweep(PointF(0, sweepY - 70.0f), PointF(0, sweepY + 70.0f),
        Color(0, 255, 255, 255), Color(0, 255, 255, 255));
    g.FillRectangle(&sweep, RectF(0, sweepY - 70.0f, (float)W, 140.0f));

    // top accent gradient line
    LinearGradientBrush topLine(RectF(0, 0, (float)W, 3), ACCENT, NEON_PINK, LinearGradientModeHorizontal);
    g.FillRectangle(&topLine, RectF(0, 0, (float)W, 3.0f));

    // ---- custom title bar (no native caption) ----
    {
        LinearGradientBrush tbFill(RectF(0, 0, (float)W, (float)TITLEBAR_H), Color(230, 10, 10, 26), Color(230, 20, 8, 40), LinearGradientModeHorizontal);
        g.FillRectangle(&tbFill, RectF(0, 0, (float)W, (float)TITLEBAR_H));
        SolidBrush tbLine(ACCENT);
        g.FillRectangle(&tbLine, RectF(0, (float)TITLEBAR_H - 1, (float)W, 1.0f));
        GText(g, L"ZOR Loader v8.0", *g_fontSmall, RectF(14, 7, 160, 20), TEXT_DIM);

        // close (X) button on the right, drawn on the UI
        float cx = (float)W - 34.0f;
        RectF cb(cx, 6, 26, 22);
        Color cbCol = g_closeHover ? (g_closeDown ? RED : Color(220, 240, 60, 80)) :
            (g_closeHover ? Color(180, 240, 60, 80) : Color(80, 60, 60, 80));
        FillRoundRect(g, cb, 6.0f, cbCol);
        StrokeRoundRect(g, cb, 6.0f, g_closeHover ? RED : Color(100, 120, 0, 160), 1.0f);
        Pen xp(Color(255, 235, 235, 240), 1.6f);
        g.DrawLine(&xp, cx + 8.0f, 11.0f, cx + 18.0f, 21.0f);
        g.DrawLine(&xp, cx + 18.0f, 11.0f, cx + 8.0f, 21.0f);
    }

    // ---- header ----
    DrawEmblem(g, 44.0f, 78.0f, 26.0f, ACCENT, Color(150, 0, 60, 140), Color(150, 120, 0, 255), L'Z');

    RectF titleR(78, 52, 200, 40);
    // glow behind title
    DrawGlow(g, 120, 74, 70, Color(70, 0, 229, 255));
    GText(g, L"ZOR", *g_fontTitle, titleR, ACCENT);
    GText(g, L"LOADER v8.0", *g_fontSmall, RectF(80, 92, 160, 18), TEXT_DIM);
    GText(g, L"the best cheat made by eddie", *g_fontNormal, RectF(80, 112, 340, 20), NEON_PINK);

    // support line: own full-width row, names highlighted
    GText(g, L"support: dc ", *g_fontNormal, RectF(80, 136, 130, 20), TEXT_DIM);
    GText(g, L"@spokenedwin", *g_fontNormal, RectF(186, 136, 130, 20), ACCENT);
    GText(g, L" // tg ", *g_fontNormal, RectF(312, 136, 60, 20), TEXT_DIM);
    GText(g, L"@spookyeddie", *g_fontNormal, RectF(366, 136, 130, 20), NEON_PINK);

    // right-side status pill
    {
        float pillW = 130.0f, pillH = 30.0f;
        RectF pill((float)W - pillW - 28, 68, pillW, pillH);
        Color pc = g_injected ? GREEN : ACCENT;
        DrawPill(g, pill, Color(60, 10, 10, 30), pc,
            g_injected ? L"ACTIVE" : L"READY", *g_fontMono, pc);
        float dotPulse = 4.0f + (g_injected ? 0 : sinf(g_pulse * 3.0f) * 1.5f);
        SolidBrush dotBr(pc);
        g.FillEllipse(&dotBr, pill.X - 8.0f - dotPulse, pill.Y + pillH / 2.0f - dotPulse,
            dotPulse * 2.0f, dotPulse * 2.0f);
    }

    // ---- platform selector ----
    float pillGap = 10.0f;
    float pillW = (float)(W - 56 - pillGap * 2) / 3.0f;
    float platY = 192.0f;
    GText(g, L"GAMING PLATFORM //", *g_fontSmall, RectF(28, platY - 20, 200, 16), TEXT_DIM);
    for (int i = 0; i < 3; i++) {
        float px = 28.0f + i * (pillW + pillGap);
        RectF pill(px, platY, pillW, 26.0f);
        bool sel = (i == (int)g_platform);
        bool hover = (i == g_platHover);
        Color pc = sel ? ACCENT : (hover ? Color(120, 0, 160, 220) : CARD_BORDER);
        FillRoundRect(g, pill, 8.0f, sel ? Color(140, 0, 40, 90) : (hover ? Color(120, 20, 30, 70) : CARD_BG));
        StrokeRoundRect(g, pill, 8.0f, pc, sel ? 1.8f : 1.0f);
        if (sel) DrawGlow(g, px + pillW / 2.0f, platY + 13.0f, pillW * 0.5f, Color(40, 0, 229, 255));
        GText(g, PlatformNames[i], *g_fontSmall, pill, sel ? ACCENT : TEXT_DIM,
            StringAlignmentCenter, StringAlignmentCenter);
    }

    // ---- status cards ----
    float cardW = (float)((W - 56 - 24) / 3);
    float cardY = 232.0f;
    float cardH = 92.0f;
    for (int i = 0; i < 3; i++) {
        float x = 28.0f + i * (cardW + 12.0f);
        RectF card(x, cardY, cardW, cardH);

        // glow when active
        bool pulse; Color sc = StepColor(g_cards[i].state, pulse);
        if (g_cards[i].state == ST_ACTIVE) DrawGlow(g, x + cardW / 2.0f, cardY + cardH / 2.0f, cardW * 0.6f, Color(40, 0, 229, 255));

        FillRoundRect(g, card, 12.0f, CARD_BG);
        StrokeRoundRect(g, card, 12.0f, CARD_BORDER, 1.0f);

        // top thin accent per card
        FillRoundRect(g, RectF(x + 8, cardY + 8, cardW - 16, 2.0f), 1.0f, sc);

        // glyph chip
        float cx = x + 24, cy = cardY + 34;
        DrawGlow(g, cx, cy, 16.0f, Color(60, 0, 229, 255));
        FillRoundRect(g, RectF(cx - 16, cy - 16, 32, 32), 8.0f, Color(200, 16, 16, 40));
        StrokeRoundRect(g, RectF(cx - 16, cy - 16, 32, 32), 8.0f, sc, 1.0f);
        GText(g, CardGlyphs[i], *g_fontSmall, RectF(cx - 16, cy - 9, 32, 16), sc, StringAlignmentCenter, StringAlignmentCenter);

        GText(g, g_cards[i].title, *g_fontNormal, RectF(x + 50, cardY + 18, cardW - 60, 20), TEXT_MAIN);
        GText(g, g_cards[i].detail, *g_fontSmall, RectF(x + 50, cardY + 44, cardW - 60, 18), sc);

        // bottom progress / state
        if (g_cards[i].state == ST_ACTIVE) {
            float prog = fmodf(g_pulse, 1.0f);
            FillRoundRect(g, RectF(x + 12, cardY + cardH - 14, (cardW - 24) * prog, 4.0f), 2.0f, ACCENT);
        } else if (g_cards[i].state == ST_OK) {
            FillRoundRect(g, RectF(x + 12, cardY + cardH - 14, cardW - 24, 4.0f), 2.0f, GREEN);
        } else if (g_cards[i].state == ST_FAIL) {
            FillRoundRect(g, RectF(x + 12, cardY + cardH - 14, cardW - 24, 4.0f), 2.0f, RED);
        }
    }

    // ---- big inject button ----
    float btnW = 340.0f, btnH = 70.0f;
    float btnX = (float)(W - btnW) / 2.0f, btnY = 334.0f;
    RectF btn(btnX, btnY, btnW, btnH);

    // outer glow
    Color glowCol = g_injected ? GREEN : (g_btnHover ? ACCENT : Color(120, 0, 160, 220));
    DrawGlow(g, btnX + btnW / 2.0f, btnY + btnH / 2.0f, btnW * 0.7f, Color(60, glowCol.GetR(), glowCol.GetG(), glowCol.GetB()));

    // body gradient
    LinearGradientBrush btnFill(PointF(btnX, btnY), PointF(btnX + btnW, btnY + btnH),
        g_btnHover ? Color(240, 20, 50, 130) : Color(210, 16, 32, 96),
        g_btnHover ? Color(240, 60, 0, 200) : Color(210, 40, 10, 160));
    GraphicsPath btnPath;
    float bd = btnH * 2.0f;
    btnPath.AddArc(btnX, btnY, bd, bd, 180, 90);
    btnPath.AddArc(btnX + btnW - bd, btnY, bd, bd, 270, 90);
    btnPath.AddArc(btnX + btnW - bd, btnY + btnH - bd, bd, bd, 0, 90);
    btnPath.AddArc(btnX, btnY + btnH - bd, bd, bd, 90, 90);
    btnPath.CloseFigure();
    g.FillPath(&btnFill, &btnPath);

    // shimmer when idle / working
    if (!g_injected) DrawShimmer(g, btn, g_pulse * 0.6f);

    Pen btnPen(glowCol, 1.6f);
    g.DrawPath(&btnPen, &btnPath);

    if (g_working) {
        // animated ring around button
        DrawRing(g, btnX + btnW / 2.0f, btnY + btnH / 2.0f, btnH / 2.0f + 8.0f, 2.5f, -90.0f,
            fmodf(g_pulse, 1.0f) * 360.0f, ACCENT);
        GText(g, L"WORKING...", *g_fontBig, btn, Color(255, 255, 255, 255), StringAlignmentCenter);
    } else if (g_injected) {
        DrawGlow(g, btnX + btnW / 2.0f, btnY + btnH / 2.0f, 60.0f, Color(80, 60, 220, 110));
        GText(g, L"INJECTED", *g_fontBig, btn, GREEN, StringAlignmentCenter);
    } else {
        GText(g, L"INJECT", *g_fontBig, btn, g_btnHover ? Color(255, 255, 255, 255) : TEXT_MAIN, StringAlignmentCenter);
    }

    // ---- log panel ----
    float logY = 420.0f;
    RectF logBox(28, logY, (float)(W - 56), (float)(H - logY - 28));
    FillRoundRect(g, logBox, 12.0f, Color(200, 14, 14, 26));
    StrokeRoundRect(g, logBox, 12.0f, CARD_BORDER, 1.0f);

    // console header bar
    FillRoundRect(g, RectF(28, logY, (float)(W - 56), 30.0f), 12.0f, Color(220, 8, 8, 20));
    GText(g, L"CONSOLE", *g_fontSmall, RectF(40, logY + 5, 100, 18), TEXT_DIM);
    GText(g, L"// LIVE FEED", *g_fontMono, RectF(118, logY + 5, 120, 18), ACCENT);

    // COPY button (top-right of console panel)
    {
        RectF copyBtn(logBox.GetRight() - 86.0f, logY + 5.0f, 64.0f, 20.0f);
        FillRoundRect(g, copyBtn, 6.0f, g_copyDown ? Color(220, 0, 229, 255) :
            (g_copyHover ? Color(160, 40, 90, 200) : Color(120, 24, 40, 90)));
        StrokeRoundRect(g, copyBtn, 6.0f, g_copyHover ? ACCENT : Color(100, 0, 160, 200), 1.0f);
        GText(g, L"COPY", *g_fontMono, copyBtn, g_copyHover ? Color(255, 255, 255, 255) : TEXT_DIM,
            StringAlignmentCenter, StringAlignmentCenter);
    }
    // HELP button (to the left of COPY)
    {
        RectF helpBtn(logBox.GetRight() - 158.0f, logY + 5.0f, 64.0f, 20.0f);
        FillRoundRect(g, helpBtn, 6.0f, g_helpDown ? Color(220, 0, 229, 255) :
            (g_helpHover ? Color(160, 40, 90, 200) : Color(120, 24, 40, 90)));
        StrokeRoundRect(g, helpBtn, 6.0f, g_helpHover ? ACCENT : Color(100, 0, 160, 200), 1.0f);
        GText(g, L"HELP", *g_fontMono, helpBtn, g_helpHover ? Color(255, 255, 255, 255) : TEXT_DIM,
            StringAlignmentCenter, StringAlignmentCenter);
    }

    // log lines (scrolled to bottom)
    EnterCriticalSection(&g_logLock);
    int n = (int)g_log.size();
    float textTop = logY + 36.0f;
    float maxLines = (logBox.Height - 40.0f) / 16.0f;
    int start = (n > (int)maxLines) ? n - (int)maxLines : 0;
    for (int i = start; i < n; i++) {
        float ly = textTop + (float)(i - start) * 16.0f;
        const std::wstring& line = g_log[i];
        Color lc = TEXT_DIM;
        if (line.find(L"[+]") != std::wstring::npos) lc = GREEN;
        else if (line.find(L"[!]") != std::wstring::npos) lc = YELLOW;
        else if (line.find(L"[ERROR]") != std::wstring::npos) lc = RED;
        else if (line.find(L"ZOR injected") != std::wstring::npos || line.find(L"ALL DONE") != std::wstring::npos) lc = ACCENT;
        GText(g, line.c_str(), *g_fontMono, RectF(40, ly, logBox.Width - 24, 16), lc);
    }
    LeaveCriticalSection(&g_logLock);

    // watermark
    GText(g, L"ZOR // system v8 // eddie", *g_fontSmall, RectF(28, (float)(H - 22), 220, 16), Color(130, 0, 160, 200));
    GText(g, L"BYOVD READY", *g_fontMono, RectF((float)(W - 150), (float)(H - 22), 130, 16),
        Color(130, 0, 160, 200), StringAlignmentFar);

    // ---- help page overlay ----
    if (g_showHelp) {
        SolidBrush dim( Color(200, 0, 0, 0) );
        g.FillRectangle(&dim, (REAL)0, (REAL)0, (REAL)W, (REAL)H);

        RectF page(70, 46, (float)(W - 140), (float)(H - 92));
        FillRoundRect(g, page, 14.0f, Color(245, 10, 10, 26));
        StrokeRoundRect(g, page, 14.0f, CARD_BORDER, 1.4f);

        GText(g, L"HOW TO INJECT", *g_fontBig, RectF(page.X + 24, page.Y + 16, page.Width - 48, 30), ACCENT);
        GText(g, L"ESC to close", *g_fontSmall, RectF(page.GetRight() - 120, page.Y + 20, 96, 16), TEXT_DIM,
            StringAlignmentFar);

        // Minimal system requirements
        GText(g, L"GRAPHICS SETTINGS (REQUIRED)", *g_fontNormal, RectF(page.X + 24, page.Y + 60, page.Width - 48, 18), NEON_PINK);
        const wchar_t* gfx[] = {
            L"  [1]  Game must run in FULLSCREEN or BORDERLESS - NOT windowed",
            L"  [2]  Rendering: 1280x720 up to 2560x1440 (higher = more VRAM)",
            L"  [3]  VRAM usage should stay under ~70% in the game menu",
            L"  [4]  Turn off DX12 'Renderer Worker Count' overrides",
            L"  [5]  FPS cap 60-165 recommended for a stable overlay",
            L"  [6]  Close Discord overlay / OBS / RTSS (they hook the same APIs)",
        };
        for (int i = 0; i < 6; i++) {
            GText(g, gfx[i], *g_fontSmall, RectF(page.X + 24, page.Y + 84 + (REAL)i * 20, page.Width - 48, 18), TEXT_MAIN);
        }

        // Steps
        GText(g, L"STEPS", *g_fontNormal, RectF(page.X + 24, page.Y + 84 + 6 * 20 + 18, page.Width - 48, 18), NEON_PINK);
        const wchar_t* steps[] = {
            L"  1.  Boot the game into a DMZ lobby (or any in-game menu)",
            L"  2.  Select your platform: BATTLE / STEAM / XBOX",
            L"  3.  Wait until the game is fully in the lobby, then press INJECT",
            L"  4.  DRIVER -> PROCESS -> INJECT must all turn green",
            L"  5.  Once INJECTED, alt-tab to the overlay (default key: END)",
        };
        for (int i = 0; i < 5; i++) {
            GText(g, steps[i], *g_fontSmall, RectF(page.X + 24, page.Y + 108 + 6 * 20 + (REAL)i * 20, page.Width - 48, 18), TEXT_MAIN);
        }

        // Troubleshooting
        GText(g, L"IF SOMETHING FAILS", *g_fontNormal, RectF(page.X + 24, page.Y + 108 + 11 * 20 + 18, page.Width - 48, 18), NEON_PINK);
        const wchar_t* fix[] = {
            L"  PROCESS red  ->  the game is not running. Launch it first, retry.",
            L"  DRIVER red   ->  hit 'COPY' and send the log; a driver update may be pending.",
            L"  INJECT red   ->  press COPY and send the log. Usually fixed by restarting the game.",
            L"  No overlay   ->  press END in-game, or restart the game and re-inject.",
        };
        for (int i = 0; i < 4; i++) {
            GText(g, fix[i], *g_fontSmall, RectF(page.X + 24, page.Y + 132 + 11 * 20 + (REAL)i * 20, page.Width - 48, 18), TEXT_MAIN);
        }

        GText(g, L"ZOR v8.0 // built by eddie // if it breaks, COPY the log and send it",
            *g_fontSmall, RectF(page.X + 24, page.GetBottom() - 26, page.Width - 48, 16), Color(130, 0, 160, 200));
    }

    // blit
    Graphics mb(memDC);
    mb.DrawImage(&backBmp, 0, 0, W, H);
    BitBlt(hdc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 16, NULL);
            return 0;
        }
        case WM_TIMER: {
            g_pulse += 0.016f;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            RECT rc; GetClientRect(hwnd, &rc);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            float btnW = 340.0f, btnH = 70.0f;
            float btnX = (float)(rc.right - btnW) / 2.0f, btnY = 334.0f;
            bool hover = (pt.x >= btnX && pt.x <= btnX + btnW && pt.y >= btnY && pt.y <= btnY + btnH);
            if (hover != g_btnHover) { g_btnHover = hover; InvalidateRect(hwnd, NULL, FALSE); }
            // platform pills: y = 192..218, 3 across
            float pillGap = 10.0f;
            float pillW = (float)(rc.right - 56 - pillGap * 2) / 3.0f;
            int ph = -1;
            for (int i = 0; i < 3; i++) {
                float px = 28.0f + i * (pillW + pillGap);
                if (pt.x >= px && pt.x <= px + pillW && pt.y >= 192 && pt.y <= 218) { ph = i; break; }
            }
            if (ph != g_platHover) { g_platHover = ph; InvalidateRect(hwnd, NULL, FALSE); }
            // close (X) button: right-34 .. right-8, y 6..28
            bool clHover = (pt.x >= rc.right - 34 && pt.x <= rc.right - 8 && pt.y >= 6 && pt.y <= 28);
            if (clHover != g_closeHover) { g_closeHover = clHover; InvalidateRect(hwnd, NULL, FALSE); }
            // COPY button: (right - 58 .. right - 22) x (logY+5 .. logY+25)
            float logY = 420.0f;
            bool cHover = (pt.x >= rc.right - 58 && pt.x <= rc.right - 22 &&
                           pt.y >= logY + 5 && pt.y <= logY + 25);
            if (cHover != g_copyHover) { g_copyHover = cHover; InvalidateRect(hwnd, NULL, FALSE); }
            // HELP button: to the left of COPY (right - 150 .. right - 98)
            bool hHover = (pt.x >= rc.right - 150 && pt.x <= rc.right - 98 &&
                           pt.y >= logY + 5 && pt.y <= logY + 25);
            if (hHover != g_helpHover) { g_helpHover = hHover; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            RECT rc; GetClientRect(hwnd, &rc);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            // close (X) button
            if (pt.x >= rc.right - 34 && pt.x <= rc.right - 8 && pt.y >= 6 && pt.y <= 28) {
                g_closeDown = true;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            // title bar drag
            if (pt.y < TITLEBAR_H) {
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
            // platform selection
            {
                float pillGap = 10.0f;
                float pillW = (float)(rc.right - 56 - pillGap * 2) / 3.0f;
                for (int i = 0; i < 3; i++) {
                    float px = 28.0f + i * (pillW + pillGap);
                    if (pt.x >= px && pt.x <= px + pillW && pt.y >= 192 && pt.y <= 218) {
                        g_platDown = i;
                        g_platform = (GamePlatform)i;
                        InvalidateRect(hwnd, NULL, FALSE);
                        break;
                    }
                }
            }
            float btnW = 340.0f, btnH = 70.0f;
            float btnX = (float)(rc.right - btnW) / 2.0f, btnY = 334.0f;
            if (pt.x >= btnX && pt.x <= btnX + btnW && pt.y >= btnY && pt.y <= btnY + btnH) {
                g_btnDown = true;
                if (!g_working && !g_injected) {
                    HANDLE th = CreateThread(NULL, 0, InjectThread, NULL, 0, NULL);
                    if (th) CloseHandle(th);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            float logY = 420.0f;
            if (pt.x >= rc.right - 58 && pt.x <= rc.right - 22 &&
                pt.y >= logY + 5 && pt.y <= logY + 25) {
                g_copyDown = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            // HELP button toggles the help page
            if (pt.x >= rc.right - 150 && pt.x <= rc.right - 98 &&
                pt.y >= logY + 5 && pt.y <= logY + 25) {
                g_helpDown = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            RECT rc; GetClientRect(hwnd, &rc);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            float logY = 420.0f;
            bool wasCopyDown = g_copyDown;
            bool wasCloseDown = g_closeDown;
            bool wasHelpDown = g_helpDown;
            g_copyDown = false;
            g_btnDown = false;
            g_platDown = -1;
            g_closeDown = false;
            g_helpDown = false;
            if (wasCloseDown && pt.x >= rc.right - 34 && pt.x <= rc.right - 8 && pt.y >= 6 && pt.y <= 28) {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
            if (wasCopyDown && pt.x >= rc.right - 58 && pt.x <= rc.right - 22 &&
                pt.y >= logY + 5 && pt.y <= logY + 25) {
                CopyConsoleToClipboard(hwnd);
            }
            if (wasHelpDown && pt.x >= rc.right - 150 && pt.x <= rc.right - 98 &&
                pt.y >= logY + 5 && pt.y <= logY + 25) {
                g_showHelp = !g_showHelp;
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            if (wp == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                CopyConsoleToClipboard(hwnd);
                return 0;
            }
            if (wp == VK_ESCAPE && g_showHelp) {
                g_showHelp = false;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            DrawScene(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    InitializeCriticalSection(&g_logLock);

    // Random identity + self-rename before any window or log is created.
    InitDisguise();
    SelfRenameAndRelaunch();

    GdiplusStartupInput gdiInput;
    ULONG_PTR gdiToken = 0;
    GdiplusStartup(&gdiToken, &gdiInput, NULL);

    g_Debug = new DebugLogger();
    g_Debug->Log("[INIT] ZOR Loader UI ready");

    FontFamily segoe(L"Segoe UI");
    FontFamily consolas(L"Consolas");
    g_fontTitle = new Font(&segoe, 44.0f, FontStyleBold, UnitPixel);
    g_fontBig = new Font(&segoe, 26.0f, FontStyleBold, UnitPixel);
    g_fontNormal = new Font(&segoe, 16.0f, FontStyleBold, UnitPixel);
    g_fontSmall = new Font(&consolas, 14.0f, FontStyleRegular, UnitPixel);
    g_fontMono = new Font(&consolas, 12.0f, FontStyleRegular, UnitPixel);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = g_fakeClass.c_str();
    wc.hIcon = LoadFakeSystemIcon();
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&wr, WS_POPUP, FALSE);

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, g_fakeName.c_str(),
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInst, NULL);
    if (!hwnd) {
        GdiplusShutdown(gdiToken);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(gdiToken);
    DeleteCriticalSection(&g_logLock);
    return (int)msg.wParam;
}
