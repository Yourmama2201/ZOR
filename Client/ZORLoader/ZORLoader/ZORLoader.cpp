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
    int n = sizeof(g_legitNames) / sizeof(g_legitNames[0]);
    g_fakeName = RandomWStringFrom(g_legitNames, n);
    g_fakeClass = RandomWStringFrom(g_legitNames, n);
    g_fakeExeName = g_fakeName + L".exe";
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
//  Remote drop helpers (download driver + client from GitHub, hide on disk)
// ============================================================================
// TODO: point these at your real GitHub release URLs (raw.githubusercontent
// works for public repos; for private repos use an authenticated release URL).
static const wchar_t* g_remoteDriverUrl = L"https://raw.githubusercontent.com/YOUR_USER/YOUR_REPO/main/Driver/nxs_drv.sys";

static const wchar_t* g_remoteDllUrl   = L"https://raw.githubusercontent.com/YOUR_USER/YOUR_REPO/main/Client/x64/Release/ZORClient.dll";

static const wchar_t* g_remoteKdmapperUrl = L"https://raw.githubusercontent.com/YOUR_USER/YOUR_REPO/main/Tools/kdmapper/kdmapper.exe";

static const wchar_t* g_remoteVulnDrvUrl = L"https://raw.githubusercontent.com/YOUR_USER/YOUR_REPO/main/Tools/kdmapper/iqvw64e.sys";
static bool DownloadToMemory(const wchar_t* url, std::vector<BYTE>& out) {
    out.clear();
    HINTERNET hSession = WinHttpOpen(L"ZORUpdater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t host[256] = {}, path[2048] = {}, extra[256] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 256;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { WinHttpCloseHandle(hSession); return false; }

    bool ok = false;
    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (hConnect) {
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER,
            NULL, flags);
        if (hReq) {
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hReq, NULL)) {
                DWORD avail = 0;
                BYTE buf[16384];
                while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                    DWORD read = 0;
                    if (!WinHttpReadData(hReq, buf, min(avail, (DWORD)sizeof(buf)), &read) || read == 0) break;
                    out.insert(out.end(), buf, buf + read);
                }
                ok = !out.empty();
            }
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return ok;
}

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
        logFile = std::string(g_fakeName.begin(), g_fakeName.end()) + ".log";
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
//  kdmapper: map unsigned driver via a signed-but-vulnerable driver (BYOVD).
//  kdmapper + vulnerable driver + target .sys all land in one hidden temp dir,
//  then the device is polled; every file is deleted when done. No SCM, no
//  test signing required.
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
                           const std::vector<BYTE>& vulnDrvData,
                           const std::vector<BYTE>& driverData) {
    wchar_t tempDir[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempDir)) return false;

    // Unique hidden subfolder so kdmapper + vuln driver sit side by side.
    std::wstring dir = std::wstring(tempDir) + L"ns" + std::to_wstring(GetTickCount()) +
        L"_" + std::to_wstring(rand() % 100000) + L"\\";
    if (!CreateDirectoryW(dir.c_str(), NULL)) return false;
    SetFileAttributesW(dir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    std::wstring kdmPath = dir + L"kdmapper.exe";
    std::wstring vulnPath = dir + L"iqvw64e.sys";
    std::wstring drvPath = dir + L"nxs_drv.sys";

    bool ok = WriteAllBytes(kdmPath, kdmapperData) &&
              WriteAllBytes(vulnPath, vulnDrvData) &&
              WriteAllBytes(drvPath, driverData);
    if (!ok) {
        DeleteFileW(kdmPath.c_str()); DeleteFileW(vulnPath.c_str()); DeleteFileW(drvPath.c_str());
        RemoveDirectoryW(dir.c_str());
        return false;
    }

    // kdmapper <driver.sys> [original_driver] -- clean up after itself.
    std::wstring cmd = L"\"" + kdmPath + L"\" \"" + drvPath + L"\" \"" + vulnPath + L"\" -d";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    BOOL launched = CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE,
        CREATE_NO_WINDOW, NULL, dir.c_str(), &si, &pi);
    if (!launched) {
        g_Debug->LogError("kdmapper CreateProcessW", GetLastError());
        DeleteFileW(kdmPath.c_str()); DeleteFileW(vulnPath.c_str()); DeleteFileW(drvPath.c_str());
        RemoveDirectoryW(dir.c_str());
        return false;
    }

    // Wait for \\.\ZOR to appear (kdmapper + driver start, unload, delete -d).
    bool mapped = false;
    for (int i = 0; i < 120; i++) {
        if (ZORDeviceExists()) { mapped = true; break; }
        DWORD rc = WaitForSingleObject(pi.hProcess, 500);
        if (rc == WAIT_OBJECT_0) break;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Cleanup: kdmapper -d removes the .sys it mapped; remove the rest.
    DeleteFileW(kdmPath.c_str());
    DeleteFileW(vulnPath.c_str());
    DeleteFileW(drvPath.c_str());
    RemoveDirectoryW(dir.c_str());
    g_Debug->Log(mapped ? "[+] Driver mapped via kdmapper" : "[!] kdmapper did not produce \\\\.\\ZOR");
    return mapped;
}

// ============================================================================
//  UI state
// ============================================================================
static const int WIN_W = 760;
static const int WIN_H = 560;

static const Color ACCENT(255, 242, 89, 0);        // zor orange
static const Color ACCENT_DIM(200, 242, 89, 0);
static const Color BG_TOP(255, 12, 12, 18);
static const Color BG_BOT(255, 20, 20, 30);
static const Color CARD_BG(200, 26, 26, 38);
static const Color CARD_BORDER(120, 60, 60, 80);
static const Color TEXT_MAIN(255, 235, 235, 240);
static const Color TEXT_DIM(255, 150, 150, 160);
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
static Font* g_fontTitle = NULL;
static Font* g_fontBig = NULL;
static Font* g_fontNormal = NULL;
static Font* g_fontSmall = NULL;

// ============================================================================
//  Debug logger (log -> UI panel)
// ============================================================================
#define IOCTL_INJECT_DLL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _INJECT_REQUEST {
    HANDLE ProcessId;
    SIZE_T DllSize;
    BOOLEAN Success;
    PVOID RemoteBase;
    UCHAR DllData[1];
} INJECT_REQUEST, * PINJECT_REQUEST;

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

    SIZE_T totalSize = FIELD_OFFSET(INJECT_REQUEST, DllData) + dllSize;
    PUCHAR buffer = (PUCHAR)malloc(totalSize);
    if (!buffer) { CloseHandle(hDevice); return NULL; }

    PINJECT_REQUEST req = (PINJECT_REQUEST)buffer;
    req->ProcessId = (HANDLE)(ULONG_PTR)pid;
    req->DllSize = dllSize;
    req->Success = FALSE;
    req->RemoteBase = NULL;
    memcpy(req->DllData, dllData, dllSize);

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(hDevice, IOCTL_INJECT_DLL, buffer, (DWORD)totalSize,
        buffer, (DWORD)totalSize, &bytesReturned, NULL);
    PVOID remoteBase = NULL;
    if (result && req->Success) remoteBase = req->RemoteBase;
    free(buffer);
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

static bool LaunchGame() {
    wchar_t exe[MAX_PATH] = {};
    wcscpy_s(exe, g_gameLaunchPath);
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) return false;

    // Let the OS resolve the per-user Steam/Battle.net context so the game boots
    // normally (launcher associates the exe with your account).
    HINSTANCE r = ShellExecuteW(NULL, L"open", exe, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)r) > 32;
}

DWORD WINAPI InjectThread(LPVOID) {
    g_working = true;
    g_injected = false;

    // Fetch all three pieces from GitHub into memory (no disk until mapped).
    std::vector<BYTE> remoteDrv, kdmData, vulnData, remoteDll;
    bool haveDrvBytes  = DownloadToMemory(g_remoteDriverUrl, remoteDrv);
    bool haveKdmapper  = DownloadToMemory(g_remoteKdmapperUrl, kdmData);
    bool haveVulnDrv   = DownloadToMemory(g_remoteVulnDrvUrl, vulnData);
    bool haveRemoteDll = DownloadToMemory(g_remoteDllUrl, remoteDll);

    std::string dllPath;
    if (haveRemoteDll) {
        dllPath = "<remote>";
    } else {
        const char* p = FindExisting(
            "C:\\Users\\Admin\\Desktop\\DMZ_FILES\\Client\\x64\\Release\\ZORClient.dll",
            "..\\..\\..\\Client\\x64\\Release\\ZORClient.dll",
            "ZORClient.dll");
        dllPath = p;
    }
    const wchar_t* targetProcess = L"cod22-cod.exe";

    // Step 1: driver
    g_cards[0].state = ST_ACTIVE; g_cards[0].detail = L"Loading...";

    bool driverReady = false;
    std::wstring droppedDriver;

    // Preferred: kdmapper (BYOVD) -- works without test signing, driver stays in
    // memory, all files deleted. Requires all 3 remote files to be present.
    if (haveDrvBytes && haveKdmapper && haveVulnDrv) {
        g_Debug->Log("Mapping driver via kdmapper (BYOVD)...");
        driverReady = MapViaKdmapper(kdmData, vulnData, remoteDrv);
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
                g_Debug->Log("[!] Remote driver write failed, falling back to local");
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
        g_cards[1].state = ST_FAIL; g_cards[1].detail = L"Not found";
        g_Debug->Log("[!] Game process not found");
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

    // background gradient
    RectF full(0, 0, (float)W, (float)H);
    LinearGradientBrush bg(Rect(0, 0, W, H), BG_TOP, BG_BOT, 90.0f);
    g.FillRectangle(&bg, full);

    // accent glow (animated)
    float glowX = (float)(W / 2) + sinf(g_pulse * 0.8f) * 60.0f;
    LinearGradientBrush glow(PointF(glowX - 300.0f, 0), PointF(glowX + 300.0f, 0),
        Color(25, 242, 89, 0), Color(0, 242, 89, 0));
    g.FillRectangle(&glow, RectF(glowX - 300.0f, -100.0f, 600.0f, 400.0f));

    // top accent line
    SolidBrush accentLine(ACCENT);
    g.FillRectangle(&accentLine, RectF(0, 0, (float)W, 3.0f));

    // ---- header ----
    GText(g, L"ZOR", *g_fontTitle, RectF(28, 22, 120, 44), ACCENT);
    GText(g, L"LOADER v8.0", *g_fontSmall, RectF(28, 64, 160, 20), TEXT_DIM);
    GText(g, L"the best cheat known :)", *g_fontNormal, RectF(28, 84, 260, 20), TEXT_DIM);

    // connection dot
    Color dotCol = g_injected ? GREEN : ACCENT;
    SolidBrush dotBr(dotCol);
    float dotPulse = 5.0f + (g_injected ? 0 : sinf(g_pulse * 3.0f) * 1.5f);
    g.FillEllipse(&dotBr, (float)W - 90.0f, 34.0f, dotPulse * 2.0f, dotPulse * 2.0f);
    GText(g, g_injected ? L"ACTIVE" : L"READY", *g_fontSmall,
        RectF((float)W - 70, 30, 60, 16), g_injected ? GREEN : ACCENT);

    // ---- status cards ----
    const wchar_t* icons[] = { L"SHIELD", L"GAME", L"INJECT" };
    float cardW = (float)((W - 56 - 24) / 3);
    float cardY = 120.0f;
    for (int i = 0; i < 3; i++) {
        float x = 28.0f + i * (cardW + 12.0f);
        RectF card(x, cardY, cardW, 92.0f);
        FillRoundRect(g, card, 10.0f, CARD_BG);
        StrokeRoundRect(g, card, 10.0f, CARD_BORDER, 1.0f);

        bool pulse;
        Color sc = StepColor(g_cards[i].state, pulse);
        float cx = x + 22, cy = cardY + 24;
        SolidBrush iconBr(sc);
        g.FillEllipse(&iconBr, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
        GText(g, icons[i], *g_fontSmall, RectF(cx - 8, cy - 7, 60, 14), Color(255, 20, 20, 28), StringAlignmentNear);

        GText(g, g_cards[i].title, *g_fontNormal, RectF(x + 44, cardY + 14, cardW - 50, 20), TEXT_MAIN);
        GText(g, g_cards[i].detail, *g_fontSmall, RectF(x + 44, cardY + 38, cardW - 50, 18), sc);

        // bottom indicator
        if (g_cards[i].state == ST_ACTIVE) {
            float prog = fmodf(g_pulse, 1.0f);
            FillRoundRect(g, RectF(x + 12, cardY + 78, (cardW - 24) * prog, 3.0f), 1.5f, ACCENT);
        } else if (g_cards[i].state == ST_OK) {
            FillRoundRect(g, RectF(x + 12, cardY + 78, cardW - 24, 3.0f), 1.5f, GREEN);
        }
    }

    // ---- big inject button ----
    float btnW = 300.0f, btnH = 64.0f;
    float btnX = (float)(W - btnW) / 2.0f, btnY = 248.0f;
    RectF btn(btnX, btnY, btnW, btnH);
    if (g_btnHover) {
        FillRoundRect(g, btn, 14.0f, Color(220, 255, 110, 20));
        if (g_btnDown) FillRoundRect(g, btn, 14.0f, Color(255, 200, 80, 10));
    } else {
        FillRoundRect(g, btn, 14.0f, Color(200, 60, 25, 10));
    }
    StrokeRoundRect(g, btn, 14.0f, g_btnHover ? ACCENT : Color(150, 242, 89, 0), 1.5f);

    if (g_working) {
        // animated progress bar inside button
        float pw = fmodf(g_pulse, 1.0f);
        FillRoundRect(g, RectF(btnX + 8, btnY + 8, (btnW - 16) * pw, btnH - 16), 10.0f, Color(120, 242, 89, 0));
        GText(g, L"WORKING...", *g_fontBig, btn, Color(255, 255, 255, 255), StringAlignmentCenter);
    } else if (g_injected) {
        GText(g, L"INJECTED", *g_fontBig, btn, GREEN, StringAlignmentCenter);
    } else {
        GText(g, L"INJECT", *g_fontBig, btn, g_btnHover ? Color(255, 255, 255, 255) : TEXT_MAIN, StringAlignmentCenter);
    }

    // ---- log panel ----
    float logY = 336.0f;
    RectF logBox(28, logY, (float)(W - 56), (float)(H - logY - 30));
    FillRoundRect(g, logBox, 10.0f, Color(190, 18, 18, 28));
    StrokeRoundRect(g, logBox, 10.0f, CARD_BORDER, 1.0f);
    GText(g, L"CONSOLE", *g_fontSmall, RectF(40, logY + 8, 100, 16), TEXT_DIM);

    // log lines (scrolled to bottom)
    EnterCriticalSection(&g_logLock);
    int n = (int)g_log.size();
    int maxLines = (int)((logBox.Height - 34) / 16.0f);
    int start = (n > maxLines) ? n - maxLines : 0;
    for (int i = start; i < n; i++) {
        float ly = logY + 30.0f + (float)(i - start) * 16.0f;
        const std::wstring& line = g_log[i];
        Color lc = TEXT_DIM;
        if (line.find(L"[+]") != std::wstring::npos) lc = GREEN;
        else if (line.find(L"[!]") != std::wstring::npos) lc = YELLOW;
        else if (line.find(L"[ERROR]") != std::wstring::npos) lc = RED;
        else if (line.find(L"ZOR injected") != std::wstring::npos || line.find(L"ALL DONE") != std::wstring::npos) lc = ACCENT;
        GText(g, line.c_str(), *g_fontSmall, RectF(40, ly, logBox.Width - 24, 16), lc);
    }
    LeaveCriticalSection(&g_logLock);

    // watermark
    GText(g, L"ZOR // system v8", *g_fontSmall, RectF(28, (float)(H - 22), 200, 16), Color(120, 90, 90, 100));

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
            float btnX = (float)(rc.right - 300) / 2.0f;
            bool hover = (pt.x >= btnX && pt.x <= btnX + 300 && pt.y >= 248 && pt.y <= 248 + 64);
            if (hover != g_btnHover) { g_btnHover = hover; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            RECT rc; GetClientRect(hwnd, &rc);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            float btnX = (float)(rc.right - 300) / 2.0f;
            if (pt.x >= btnX && pt.x <= btnX + 300 && pt.y >= 248 && pt.y <= 248 + 64) {
                g_btnDown = true;
                if (!g_working && !g_injected) {
                    HANDLE th = CreateThread(NULL, 0, InjectThread, NULL, 0, NULL);
                    if (th) CloseHandle(th);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            g_btnDown = false;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
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
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, g_fakeName.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
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
