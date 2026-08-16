#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <vector>
#include <string>
#include <winioctl.h>
#include <intrin.h>
#include <iostream>
#include <winternl.h>
#include <psapi.h>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

// ============================================
// IOCTL CODES (must match driver)
// ============================================
#define IOCTL_READ_MEMORY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_PID      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ============================================
// MEMORY REQUEST STRUCTURE
// ============================================
struct MEMORY_REQUEST {
    HANDLE ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;
    SIZE_T Size;
};
typedef MEMORY_REQUEST* PMEMORY_REQUEST;

// ============================================
// DECRYPTION STRUCTS
// ============================================
#define ROTL64(x, y) _rotl64(x, y)
#define ROTR64(x, y) _rotr64(x, y)
#define BYTESWAP64(x) _byteswap_uint64(x)

// ============================================
// MEMORY MANAGER CLASS
// ============================================
class MemoryManager {
private:
    HANDLE hDriver;
    HANDLE hProcess;
    uintptr_t gameBase;
    uintptr_t moduleBase;
    DWORD pid;
    std::wstring processName;
    uintptr_t peb;

    // ============================================
    // PROCESS NAMES TO SEARCH
    // ============================================
    const std::vector<std::wstring> PROCESS_NAMES = {
        L"cod.exe",
        L"cod22-cod.exe",
        L"codhq-cod.exe",
        L"ModernWarfare.exe",
        L"mw22-cod.exe",
        L"COD.exe"
    };

    // ============================================
    // FIND PROCESS
    // ============================================
    bool FindProcess() {
        std::cout << "[nxs] Searching for game process...\n";

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) {
            std::cout << "[nxs] Failed to create process snapshot. Error: " << GetLastError() << "\n";
            return false;
        }

        PROCESSENTRY32W entry = { sizeof(entry) };
        if (!Process32FirstW(snap, &entry)) {
            std::cout << "[nxs] Failed to get first process. Error: " << GetLastError() << "\n";
            CloseHandle(snap);
            return false;
        }

        std::cout << "[nxs] Scanning processes...\n";
        int processCount = 0;

        do {
            processCount++;
            std::wstring currentName = entry.szExeFile;

            if (processCount <= 10) {
                std::wcout << L"[nxs] Found process: " << currentName << L" (PID: " << entry.th32ProcessID << L")\n";
            }

            for (auto& name : PROCESS_NAMES) {
                if (_wcsicmp(currentName.c_str(), name.c_str()) == 0) {
                    pid = entry.th32ProcessID;
                    processName = currentName;
                    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                    CloseHandle(snap);

                    std::wcout << L"[nxs] ✅ FOUND GAME: " << processName << L" (PID: " << pid << L")\n";
                    return true;
                }
            }
        } while (Process32NextW(snap, &entry));

        std::cout << "[nxs] ❌ Game process not found. Make sure MW2022 is running.\n";
        CloseHandle(snap);
        return false;
    }

    // ============================================
    // GET MODULE BASE
    // ============================================
    uintptr_t GetModuleBase() {
        if (!hProcess) return 0;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) {
            std::cout << "[nxs] Failed to get module snapshot. Error: " << GetLastError() << "\n";
            return 0;
        }

        MODULEENTRY32W entry = { sizeof(entry) };
        if (!Module32FirstW(snap, &entry)) {
            std::cout << "[nxs] Failed to get first module. Error: " << GetLastError() << "\n";
            CloseHandle(snap);
            return 0;
        }

        do {
            std::wstring currentName = entry.szModule;
            for (auto& name : PROCESS_NAMES) {
                if (_wcsicmp(currentName.c_str(), name.c_str()) == 0) {
                    uintptr_t base = (uintptr_t)entry.modBaseAddr;
                    std::wcout << L"[nxs] ✅ Found game base: 0x" << std::hex << base << L" (" << currentName << L")\n";
                    CloseHandle(snap);
                    return base;
                }
            }
        } while (Module32NextW(snap, &entry));

        if (entry.modBaseAddr) {
            uintptr_t base = (uintptr_t)entry.modBaseAddr;
            std::wcout << L"[nxs] Using first module as base: 0x" << std::hex << base << L"\n";
            CloseHandle(snap);
            return base;
        }

        CloseHandle(snap);
        return 0;
    }

    // ============================================
    // GET PEB
    // ============================================
    uintptr_t GetPeb() {
        if (!hProcess) return 0;

        PROCESS_BASIC_INFORMATION pbi = {};
        ULONG size = 0;
        NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &size);
        return (uintptr_t)pbi.PebBaseAddress;
    }

    // ============================================
    // DECRYPTION FUNCTIONS (YOUR NEW OFFSETS)
    // ============================================

    uintptr_t DecryptClientInfo() {
        uintptr_t rax = 0, rbx = 0, rcx = 0, rdx = 0, r8 = 0, r9 = 0;

        rdx = Read<uintptr_t>(moduleBase + 0x118287E8);
        if (!rdx) return 0;

        rcx = peb;
        rdx -= rcx;
        rax = 0;
        r8 = 0x693186CC4D1F9DB;
        rdx *= r8;
        rax = ROTL64(rax, 0x10);
        r8 = 0x57548B4D82F080EE;
        rax ^= Read<uintptr_t>(moduleBase + 0x90050E6);
        rdx += r8;
        rax = BYTESWAP64(rax);
        rdx *= Read<uintptr_t>(rax + 0x13);
        rax = rdx;
        rdx >>= 0x20;
        rdx ^= rax;
        rdx += rcx;
        return rdx;
    }

    uintptr_t DecryptClientBase(uintptr_t clientInfo) {
        uintptr_t rax = 0, rbx = 0, rcx = 0, rdx = 0, r8 = 0, r9 = 0, r10 = 0, r11 = 0;

        rdx = Read<uintptr_t>(clientInfo + 0x18bc20);
        if (!rdx) return 0;

        r11 = ~peb;
        rax = r11;
        rax = ROTL64(rax, 0x21);
        rax &= 0xF;

        switch (rax) {
        case 0:
        {
            r10 = Read<uintptr_t>(moduleBase + 0x900510E);
            rbx = moduleBase;
            rax = 0x9141C45BFD5B39F7;
            rdx ^= rax;
            rcx = rdx + rbx;
            rdx = moduleBase + 0x6E33AF72;
            rax = 0xF605A67470E7C53D;
            rcx *= rax;
            rax = r11;
            rax ^= rdx;
            rcx -= rax;
            rcx -= rbx;
            rdx = rcx + 0xffffffffd10685d8;
            rdx += r11;
            rax = moduleBase + 0x526CB4F4;
            rax = ~rax;
            rax -= r11;
            rdx += rax;
            rax = rdx;
            rax >>= 0x1A;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x34;
            rdx ^= rax;
            rax = 0;
            rax = ROTL64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= Read<uintptr_t>(rax + 0x19);
            return rdx;
        }
        case 1:
        {
            r10 = Read<uintptr_t>(moduleBase + 0x900510E);
            rbx = moduleBase;
            rax = rdx;
            rax >>= 0x24;
            rdx ^= rax;
            rax = 0xE570A6F93EC9464F;
            rdx *= rax;
            rax = 0;
            rax = ROTL64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= Read<uintptr_t>(rax + 0x19);
            rax = rdx;
            rax >>= 0x24;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x11;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x22;
            rdx ^= rax;
            rdx -= rbx;
            rdx += r11;
            rax = 0x14F095F380F9EB43;
            rdx += rax;
            return rdx;
        }
        case 2:
        {
            r10 = Read<uintptr_t>(moduleBase + 0x900510E);
            rax = 0x8ADB88DACDCF2087;
            rdx *= rax;
            rax = 0x7962CBE13BD24CEA;
            rdx += rax;
            rax = rdx;
            rax >>= 0x10;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x20;
            rdx ^= rax;
            rdx += r11;
            rdx += r11;
            rax = 0;
            rax = ROTL64(rax, 0x10);
            rax ^= r10;
            rax = ~rax;
            rdx *= Read<uintptr_t>(rax + 0x19);
            rdx -= r11;
            rax = rdx;
            rax >>= 0x1D;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x3A;
            rdx ^= rax;
            return rdx;
        }
        default:
        {
            // Generic handler for unimplemented cases - logs which case is hit
            // TODO: Dump actual decryption routines from game binary for each case
            r10 = Read<uintptr_t>(moduleBase + 0x900510E);
            rbx = moduleBase;
            rcx = rdx;
            rcx >>= 0x1A; rdx ^= rcx;
            rcx = rdx; rcx >>= 0x34; rdx ^= rcx;
            rdx -= r11;
            rdx -= rbx;
            rdx ^= Read<uintptr_t>(0x90051A8 + (rax * 8));
            return rdx;
        }
        }
    }

    uintptr_t GetBoneBase() {
        uintptr_t rax = 0, rbx = 0, rcx = 0, rdx = 0, r8 = 0, r9 = 0, r10 = 0, r11 = 0;

        rdx = Read<uintptr_t>(moduleBase + 0xCA37E48);
        if (!rdx) return 0;

        r11 = peb;
        rax = r11;
        rax = ROTR64(rax, 0x15);
        rax &= 0xF;

        switch (rax) {
        case 0:
        {
            r9 = Read<uintptr_t>(moduleBase + 0x90051FC);
            uintptr_t r15 = moduleBase + 0x629DAB46;
            uintptr_t r13 = moduleBase + 0x9895;
            rax = 0xAC145E023332D189;
            rdx ^= rax;
            rax = r15;
            rax = ~rax;
            rax *= r11;
            rdx += rax;
            rax = 0xFDEBD2F07B05670D;
            rdx *= rax;
            rax = rdx;
            rax >>= 0x3;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x6;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0xC;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x18;
            rdx ^= rax;
            rax = rdx;
            rax >>= 0x30;
            rdx ^= rax;
            rax = 0xF0805972B46E082;
            rdx -= rax;
            rax = r11;
            rax ^= r13;
            rdx += rax;
            rax = 0;
            rax = ROTL64(rax, 0x10);
            rax ^= r9;
            rax = BYTESWAP64(rax);
            rdx *= Read<uintptr_t>(rax + 0x11);
            return rdx;
        }
        default:
        {
            // Generic fallback for remaining bone decryption cases
            r9 = Read<uintptr_t>(moduleBase + 0x90051FC);
            rax = rdx; rax >>= 0x3; rdx ^= rax;
            rax = rdx; rax >>= 0x6; rdx ^= rax;
            rax = rdx; rax >>= 0xC; rdx ^= rax;
            rax = rdx; rax >>= 0x18; rdx ^= rax;
            rax = rdx; rax >>= 0x30; rdx ^= rax;
            rdx += r11;
            rdx ^= r9;
            return rdx;
        }
        }
    }

public:
    // ============================================
    // CONSTRUCTOR
    // ============================================
    MemoryManager() : hDriver(INVALID_HANDLE_VALUE), hProcess(NULL), pid(0), gameBase(0), moduleBase(0), peb(0) {
        std::cout << "[nxs] Initializing MemoryManager...\n";

        // Try to connect to driver (device is ZOR, service is nxs_drv)
        hDriver = CreateFileW(L"\\\\.\\ZOR", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

        if (hDriver == INVALID_HANDLE_VALUE) {
            std::cout << "[nxs] Driver not found. Trying to start it...\n";
            system("sc start nxs_drv 2>nul");
            Sleep(2000);

            hDriver = CreateFileW(L"\\\\.\\ZOR", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        }

        if (hDriver != INVALID_HANDLE_VALUE) {
            std::cout << "[nxs] ✅ Driver connected!\n";
        }
        else {
            std::cout << "[nxs] ⚠️ Driver not available. Using fallback ReadProcessMemory.\n";
        }

        // Find game process
        std::cout << "[nxs] Looking for MW2022...\n";
        FindProcess();

        if (hProcess) {
            peb = GetPeb();
            moduleBase = GetModuleBase();
            gameBase = moduleBase;
        }

        if (gameBase) {
            std::cout << "[nxs] ✅ Game found! Base: 0x" << std::hex << gameBase << "\n";
        }
        else {
            std::cout << "[nxs] ❌ Game not found. Waiting for game to launch...\n";
        }
    }

    // ============================================
    // DESTRUCTOR
    // ============================================
    ~MemoryManager() {
        if (hProcess) CloseHandle(hProcess);
        if (hDriver != INVALID_HANDLE_VALUE) CloseHandle(hDriver);
    }

    // ============================================
    // GETTERS
    // ============================================
    bool IsGameRunning() const { return hProcess != NULL && pid != 0; }
    DWORD GetPid() const { return pid; }
    uintptr_t GetBase() const { return gameBase; }
    uintptr_t GetModuleBase() const { return moduleBase; }
    uintptr_t GetPeb() const { return peb; }
    std::wstring GetProcessName() const { return processName; }
    bool IsDriverLoaded() const { return hDriver != INVALID_HANDLE_VALUE; }

    // ============================================
    // GET CLIENT INFO (DECRYPTED)
    // ============================================
    uintptr_t GetClientInfo() {
        if (!gameBase) return 0;
        moduleBase = gameBase;
        peb = GetPeb();
        return DecryptClientInfo();
    }

    // ============================================
    // GET CLIENT BASE (DECRYPTED)
    // ============================================
    uintptr_t GetClientBase(uintptr_t clientInfo) {
        if (!gameBase) return 0;
        moduleBase = gameBase;
        peb = GetPeb();
        return DecryptClientBase(clientInfo);
    }

    // ============================================
    // GET BONE BASE (DECRYPTED)
    // ============================================
    uintptr_t GetBoneBaseDecrypted() {
        if (!gameBase) return 0;
        moduleBase = gameBase;
        peb = GetPeb();
        return GetBoneBase();
    }

    // ============================================
    // READ MEMORY (Driver inline protocol with RPM fallback)
    // Driver (zordriver.c) places data inline AFTER the MEMORY_REQUEST
    // struct in the system buffer, returns sizeof(req)+size bytes.
    // ============================================
    bool ReadRaw(uintptr_t address, void* outBuf, size_t size) {
        BOOL success = FALSE;

        if (hDriver != INVALID_HANDLE_VALUE && pid != 0) {
            if (size == 0 || size > 0x400000) return false;
            size_t total = sizeof(MEMORY_REQUEST) + size;
            std::vector<UCHAR> buf(total, 0);
            PMEMORY_REQUEST req = (PMEMORY_REQUEST)buf.data();
            req->ProcessId = (HANDLE)(ULONG_PTR)pid;
            req->Address = (ULONG_PTR)address;
            req->Buffer = NULL;
            req->Size = size;

            DWORD bytesReturned = 0;
            success = DeviceIoControl(
                hDriver, IOCTL_READ_MEMORY,
                buf.data(), (DWORD)total,
                buf.data(), (DWORD)total,
                &bytesReturned, NULL
            );
            if (success && bytesReturned >= sizeof(MEMORY_REQUEST) + size) {
                memcpy(outBuf, buf.data() + sizeof(MEMORY_REQUEST), size);
                return true;
            }
            return false;
        }

        if (hProcess) {
            return ReadProcessMemory(hProcess, (LPCVOID)address, outBuf, size, NULL) != 0;
        }
        return false;
    }

    template<typename T>
    T Read(uintptr_t address) {
        T buffer = {};
        ReadRaw(address, &buffer, sizeof(T));
        return buffer;
    }

    // ============================================
    // WRITE MEMORY (Driver inline protocol with RPM fallback)
    // ============================================
    bool WriteRaw(uintptr_t address, const void* inBuf, size_t size) {
        BOOL success = FALSE;

        if (hDriver != INVALID_HANDLE_VALUE && pid != 0) {
            if (size == 0 || size > 0x400000) return false;
            size_t total = sizeof(MEMORY_REQUEST) + size;
            std::vector<UCHAR> buf(total, 0);
            PMEMORY_REQUEST req = (PMEMORY_REQUEST)buf.data();
            req->ProcessId = (HANDLE)(ULONG_PTR)pid;
            req->Address = (ULONG_PTR)address;
            req->Buffer = NULL;
            req->Size = size;
            memcpy(buf.data() + sizeof(MEMORY_REQUEST), inBuf, size);

            DWORD bytesReturned = 0;
            success = DeviceIoControl(
                hDriver, IOCTL_WRITE_MEMORY,
                buf.data(), (DWORD)total,
                buf.data(), (DWORD)total,
                &bytesReturned, NULL
            );
            return success;
        }

        if (hProcess) {
            return WriteProcessMemory(hProcess, (LPVOID)address, inBuf, size, NULL) != 0;
        }
        return false;
    }

    template<typename T>
    void Write(uintptr_t address, T value) {
        WriteRaw(address, &value, sizeof(T));
    }

    // ============================================
    // READ STRING
    // ============================================
    std::string ReadString(uintptr_t address, size_t maxLen = 64) {
        char buffer[256] = {};
        size_t readSize = (maxLen < 255) ? maxLen : 255;
        ReadRaw(address, buffer, readSize);
        return std::string(buffer);
    }

    bool ReadBuffer(uintptr_t address, void* buffer, size_t size) {
        return ReadRaw(address, buffer, size);
    }

    bool WriteBuffer(uintptr_t address, void* buffer, size_t size) {
        return WriteRaw(address, buffer, size);
    }
};