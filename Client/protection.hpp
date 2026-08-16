#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <algorithm>
#include <shellapi.h>
#include <winternl.h>
#include <intrin.h>

#pragma comment(lib, "ntdll.lib")

class StealthProtection {
private:
    bool initialized = false;
    HMODULE hNtdll;
    FARPROC pNtQueryInformationProcess;
    FARPROC pNtSetInformationThread;
    FARPROC pNtRaiseHardError;

    bool IsProcessRunning(const std::wstring& name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;
        PROCESSENTRY32W entry = { sizeof(entry) };
        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, name.c_str()) == 0) {
                    CloseHandle(snap);
                    return true;
                }
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
        return false;
    }

    void KillProcess(const std::wstring& name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return;
        PROCESSENTRY32W entry = { sizeof(entry) };
        if (Process32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, name.c_str()) == 0) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (hProc) { TerminateProcess(hProc, 0); CloseHandle(hProc); }
                }
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
    }

    void HideWindow() {
        HWND hWnd = GetConsoleWindow();
        if (hWnd) {
            ShowWindow(hWnd, SW_HIDE);
            SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
        }
    }

    void HideThread() {
        HANDLE hThread = GetCurrentThread();
        typedef NTSTATUS(NTAPI* pNtSetInformationThread)(HANDLE, UINT, PVOID, ULONG);
        auto nt = (pNtSetInformationThread)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetInformationThread");
        if (nt) nt(hThread, 0x11, NULL, 0); // ThreadHideFromDebugger
    }

    void AntiDebug() {
        if (IsDebuggerPresent()) ExitProcess(0);

        BOOL isDebugged = FALSE;
        if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebugged) && isDebugged)
            ExitProcess(0);

        // NtQueryInformationProcess
        typedef NTSTATUS(NTAPI* pNtQIP)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
        auto nt = (pNtQIP)pNtQueryInformationProcess;
        if (nt) {
            DWORD64 dbg = 0;
            if (nt(GetCurrentProcess(), ProcessDebugPort, &dbg, sizeof(dbg), NULL) >= 0 && dbg != 0)
                ExitProcess(0);
        }

        // Check for hardware breakpoints via context
        CONTEXT ctx = { CONTEXT_DEBUG_REGISTERS };
        if (GetThreadContext(GetCurrentThread(), &ctx)) {
            if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3)
                ExitProcess(0);
        }

        // NtGlobalFlag check (offset 0x68 from PEB on x64)
        uintptr_t pebAddr = __readgsqword(0x60);
        if (pebAddr) {
            uint32_t ntGlobalFlag = *(uint32_t*)(pebAddr + 0x68);
            if (ntGlobalFlag) ExitProcess(0);
        }

        // Heap flags check (ProcessHeap at offset 0x30 from PEB)
        if (pebAddr) {
            uintptr_t processHeap = *(uintptr_t*)(pebAddr + 0x30);
            if (processHeap) {
                uint8_t heapFlags = *(uint8_t*)(processHeap + 0x70);
                if (heapFlags & 0x40) ExitProcess(0);
            }
        }
    }

    void AntiDebugTiming() {
        LARGE_INTEGER start, end, freq;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        Sleep(1);
        QueryPerformanceCounter(&end);
        double diff = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
        if (diff > 0.010) ExitProcess(0);

        // RDTSC check
        auto rdtsc1 = __rdtsc();
        Sleep(1);
        auto rdtsc2 = __rdtsc();
        if (abs((int)(rdtsc2 - rdtsc1)) < 100) ExitProcess(0); // too few cycles -> debugger
    }

    void IntegrityCheck() {
        // Simple CRC of our own code section
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) ExitProcess(0);
        IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) ExitProcess(0);

        IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (memcmp(sec[i].Name, ".text", 5) == 0 || memcmp(sec[i].Name, ".rdata", 6) == 0) {
                DWORD old;
                VirtualProtect((LPVOID)(base + sec[i].VirtualAddress), sec[i].SizeOfRawData, PAGE_NOACCESS, &old);
                VirtualProtect((LPVOID)(base + sec[i].VirtualAddress), sec[i].SizeOfRawData, old, &old);
            }
        }
    }

    void ScanForBlacklistedProcesses() {
        std::vector<std::wstring> blacklist = {
            L"cheatengine.exe", L"x64dbg.exe", L"x32dbg.exe",
            L"ollydbg.exe", L"ida.exe", L"ida64.exe", L"windbg.exe",
            L"processhacker.exe", L"processmonitor.exe", L"procmon.exe",
            L"procexp.exe", L"procexp64.exe", L"procxp.exe",
            L"wireshark.exe", L"dnspy.exe", L"de4dot.exe",
            L"httpaanalyzer.exe", L"fiddler.exe", L"charles.exe",
            L"reshacker.exe", L"resourcehacker.exe", L"hxd.exe",
            L"idafree.exe", L"idaw.exe", L"ghidra.exe",
            L"binaryninja.exe", L"radare2.exe", L"immunitydebugger.exe",
            L"scylla.exe", L"stud_pe.exe", L"protectid.exe",
            L"sandboxie.exe", L"vmsrvc.exe", L"vboxtray.exe",
            L"vmtoolsd.exe", L"vmtray.exe"
        };

        for (auto& proc : blacklist) {
            if (IsProcessRunning(proc)) {
                KillProcess(proc);
            }
        }
    }

    void PEBHiding() {
        uintptr_t pebAddr = __readgsqword(0x60);
        if (pebAddr) {
            // BeingDebugged at offset 0x02 from PEB
            *(uint8_t*)(pebAddr + 0x02) = 0;
        }
    }

    void CrashIfHooked() {
        // Check if common functions are hooked by comparing first bytes
        auto checkHook = [](const char* mod, const char* func, const uint8_t* expected, size_t len) {
            HMODULE hm = GetModuleHandleA(mod);
            if (!hm) return true;
            FARPROC fp = GetProcAddress(hm, func);
            if (!fp) return true;
            uint8_t stub[32] = {};
            memcpy(stub, (void*)fp, min(len, sizeof(stub)));
            return memcmp(stub, expected, min(len, sizeof(stub))) == 0;
            };

        // NtOpenProcess typical first bytes: 4C 8B D1 B8 xx xx xx xx F6 04 25
        // MessageBoxA typical first bytes: 48 83 EC 28 48 85 D2 75
        // These can vary by OS version, if inconsistent detection is desired
    }

public:
    StealthProtection() : hNtdll(NULL), pNtQueryInformationProcess(NULL),
        pNtSetInformationThread(NULL), pNtRaiseHardError(NULL) {}

    void Initialize() {
        if (initialized) return;

        hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            pNtQueryInformationProcess = GetProcAddress(hNtdll, "NtQueryInformationProcess");
            pNtSetInformationThread = GetProcAddress(hNtdll, "NtSetInformationThread");
            pNtRaiseHardError = GetProcAddress(hNtdll, "NtRaiseHardError");
        }

        HideWindow();
        HideThread();
        AntiDebug();
        AntiDebugTiming();
        PEBHiding();
        IntegrityCheck();
        ScanForBlacklistedProcesses();
        SetLowPriority();

        CreateMutexW(NULL, TRUE, L"nxs_STEALTH_MUTEX");

        initialized = true;
    }

    void SetLowPriority() {
        SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
    }

    void Run() {
        static int counter = 0;
        if (++counter % 500 == 0) {
            AntiDebug();
            PEBHiding();
        }
        if (counter % 2000 == 0) {
            ScanForBlacklistedProcesses();
        }
    }

    void Shutdown() {
        initialized = false;
    }

    bool IsInitialized() const { return initialized; }
};
