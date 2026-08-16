#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winioctl.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <iostream>

static std::ofstream g_log;

#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_PID     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BASE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_FIND_PID    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MEMORY_REQUEST {
    HANDLE ProcessId;
    ULONG_PTR Address;
    PVOID Buffer;  // Not used with inline data, kept for compatibility
    SIZE_T Size;
} MEMORY_REQUEST, * PMEMORY_REQUEST;

#include "sig_patterns.hpp"

static std::mt19937 g_rng(std::random_device{}());

template<int N>
struct XStr {
    char data[N];
    constexpr XStr(const char(&s)[N]) {
        for (int i = 0; i < N; i++) data[i] = s[i] ^ 0x55;
    }
    char* Decrypt() {
        for (int i = 0; i < N; i++) data[i] ^= 0x55;
        return data;
    }
};

HANDLE OpenDriver() {
    XStr dev("\\\\.\\ZOR");
    return CreateFileA(dev.Decrypt(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
}

bool DriverRead(HANDLE hDrv, HANDLE pid, uintptr_t addr, void* buf, size_t size) {
    // Allocate buffer for request + data
    std::vector<BYTE> buffer(sizeof(MEMORY_REQUEST) + size);
    MEMORY_REQUEST* req = (MEMORY_REQUEST*)buffer.data();
    req->ProcessId = pid;
    req->Address = addr;
    req->Buffer = nullptr;  // Not used with inline data
    req->Size = size;

    DWORD bytes = 0;
    BOOL result = DeviceIoControl(hDrv, IOCTL_READ_MEMORY, buffer.data(), (DWORD)buffer.size(),
                                  buffer.data(), (DWORD)buffer.size(), &bytes, NULL);
    if (result && bytes >= sizeof(MEMORY_REQUEST) + size) {
        memcpy(buf, buffer.data() + sizeof(MEMORY_REQUEST), size);
        return true;
    }
    return false;
}

DWORD FindPidViaDriver(HANDLE hDrv, const char* targetName) {
    if (!hDrv || hDrv == INVALID_HANDLE_VALUE) return 0;
    char buf[64] = {0};
    strncpy(buf, targetName, 63);
    DWORD pid = 0;
    DWORD bytes = 0;
    if (DeviceIoControl(hDrv, IOCTL_FIND_PID, buf, (DWORD)strlen(buf) + 1,
        &pid, sizeof(pid), &bytes, NULL) && bytes >= sizeof(pid) && pid)
        return pid;
    return 0;
}

typedef LONG NTSTATUS;
typedef struct _UNICODE_STRING_STUB {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING_STUB;

typedef struct _SYSTEM_PROCESS_INFO {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    BYTE Reserved1[48];
    UNICODE_STRING_STUB ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    PVOID Reserved2;
    ULONG HandleCount;
    ULONG SessionId;
    PVOID Reserved3;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG Reserved4;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    PVOID Reserved5;
    SIZE_T QuotaPagedPoolUsage;
    PVOID Reserved6;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
} SYSTEM_PROCESS_INFO;

DWORD FindPidFallback(const wchar_t* targetName) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll");
    if (!ntdll) return 0;
    auto NtQSI = (NTSTATUS(WINAPI*)(int, PVOID, ULONG, PULONG))GetProcAddress(ntdll, "NtQuerySystemInformation");
    if (!NtQSI) return 0;

    ULONG size = 0x10000;
    std::vector<BYTE> buf(size);
    while (NtQSI(5, buf.data(), size, &size) != 0) {
        size += 0x10000;
        buf.resize(size);
    }

    SYSTEM_PROCESS_INFO* p = (SYSTEM_PROCESS_INFO*)buf.data();
    while (true) {
        if (p->ImageName.Buffer && p->UniqueProcessId)
            if (_wcsicmp(p->ImageName.Buffer, targetName) == 0)
                return (DWORD)(SIZE_T)p->UniqueProcessId;
        if (!p->NextEntryOffset) break;
        p = (SYSTEM_PROCESS_INFO*)((BYTE*)p + p->NextEntryOffset);
    }
    return 0;
}

typedef NTSTATUS(NTAPI* fnNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);

void LaunchGameViaBattlenet() {
    const char* paths[] = {
        "C:\\Program Files (x86)\\Battle.net\\Battle.net Launcher.exe",
        "C:\\Program Files (x86)\\Battle.net\\Battle.net.exe",
    };
    const char* bnetUrl = "battle.net://wim/launch";
    const char* args = "--exec=\"launch nina\"";
    for (auto p : paths) {
        if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) {
            std::cout << "Launching game via Battle.net...\n";
            g_log << "LAUNCH: " << p << " " << args << "\n";
            ShellExecuteA(NULL, "open", p, args, NULL, SW_SHOWNORMAL);
            return;
        }
    }
    std::cout << "Battle.net not found, trying protocol...\n";
    g_log << "LAUNCH: battle.net protocol fallback\n";
    ShellExecuteA(NULL, "open", bnetUrl, NULL, NULL, SW_SHOWNORMAL);
}

uintptr_t GetBaseViaDriver(HANDLE hDrv, HANDLE pid) {
    if (!hDrv || hDrv == INVALID_HANDLE_VALUE) return 0;
    uintptr_t base = 0;
    DWORD bytes = 0;
    DeviceIoControl(hDrv, IOCTL_GET_BASE, &pid, sizeof(pid), &base, sizeof(base), &bytes, NULL);
    if (bytes >= sizeof(base) && base) { g_log << "DBG: IOCTL_GET_BASE ok\n"; return base; }
    g_log << "DBG: IOCTL_GET_BASE failed (bytes=" << bytes << " base=0x" << std::hex << base << ")\n";

    HMODULE ntdll = GetModuleHandleW(L"ntdll");
    if (!ntdll) { g_log << "DBG: no ntdll\n"; return 0; }
    fnNtQIP NtQIP = (fnNtQIP)GetProcAddress(ntdll, "NtQueryInformationProcess");
    if (!NtQIP) { g_log << "DBG: no NtQIP\n"; return 0; }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)(SIZE_T)pid);
    if (!hProc) { g_log << "DBG: OpenProcess failed err=" << GetLastError() << "\n"; return 0; }

    struct {
        ULONG_PTR ExitStatus;
        ULONG_PTR PebBaseAddress;
        ULONG_PTR AffinityMask;
        ULONG_PTR BasePriority;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR InheritedFromUniqueProcessId;
    } pbi = { 0 };

    NTSTATUS ns = NtQIP(hProc, 0, &pbi, sizeof(pbi), &bytes);
    if (ns < 0) { g_log << "DBG: NtQIP failed status=0x" << std::hex << (ULONG)ns << "\n"; CloseHandle(hProc); return 0; }
    if (!pbi.PebBaseAddress) { g_log << "DBG: no PebBaseAddress\n"; CloseHandle(hProc); return 0; }
    g_log << "DBG: PEB=0x" << std::hex << pbi.PebBaseAddress << "\n";

    SIZE_T readBytes = 0;
    if (ReadProcessMemory(hProc, (LPCVOID)(pbi.PebBaseAddress + 0x10), &base, sizeof(base), &readBytes) && readBytes == sizeof(base) && base) {
        g_log << "DBG: RPM base=0x" << std::hex << base << "\n";
        CloseHandle(hProc);
        return base;
    }
    g_log << "DBG: RPM PEB+0x10 failed err=" << GetLastError() << "\n";

    if (DriverRead(hDrv, pid, pbi.PebBaseAddress + 0x10, &base, sizeof(base)) && base) {
        g_log << "DBG: DriverRead fallback base=0x" << std::hex << base << "\n";
        CloseHandle(hProc);
        return base;
    }
    g_log << "DBG: DriverRead PEB+0x10 also failed\n";

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)(SIZE_T)pid);
    if (snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me = { sizeof(me) };
        if (Module32First(snap, &me)) {
            base = (uintptr_t)me.modBaseAddr;
            g_log << "DBG: Toolhelp32 base=0x" << std::hex << base << "\n";
            CloseHandle(snap);
            CloseHandle(hProc);
            return base;
        }
        CloseHandle(snap);
    }
    g_log << "DBG: Toolhelp32 failed err=" << GetLastError() << "\n";

    CloseHandle(hProc);
    return 0;
}

void EvasiveSleep(int ms) {
    Sleep(ms + (g_rng() % (ms / 4 + 1)));
}

struct Signature {
    std::string name;
    std::vector<uint8_t> pattern;
    std::vector<uint8_t> mask;
    int extra;
    bool relative;
    uintptr_t result;

    Signature(const SigPattern& sp) : name(sp.name), extra(sp.extra),
        relative(sp.relative), result(0) {
        std::string h = sp.hexPattern;
        size_t pos = 0;
        while (pos < h.size()) {
            while (pos < h.size() && h[pos] == ' ') pos++;
            if (pos >= h.size()) break;
            if (h[pos] == '?') {
                pattern.push_back(0); mask.push_back(0); pos++;
            } else {
                uint8_t byte = 0;
                for (int i = 0; i < 2; i++) {
                    if (pos < h.size()) {
                        char c = h[pos++]; byte <<= 4;
                        if (c >= '0' && c <= '9') byte |= (c - '0');
                        else if (c >= 'A' && c <= 'F') byte |= (c - 'A' + 10);
                        else if (c >= 'a' && c <= 'f') byte |= (c - 'a' + 10);
                    }
                }
                pattern.push_back(byte); mask.push_back(1);
            }
        }
    }

    bool Match(const uint8_t* data) const {
        for (size_t i = 0; i < pattern.size(); i++)
            if (mask[i] && data[i] != pattern[i]) return false;
        return true;
    }
};

bool ReadSection(HANDLE hDrv, HANDLE pid, uintptr_t moduleBase,
    const char* sectionName, std::vector<uint8_t>& outData, uintptr_t& outVA) {
    IMAGE_DOS_HEADER dos = {};
    if (!DriverRead(hDrv, pid, moduleBase, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    IMAGE_NT_HEADERS64 nt = {};
    if (!DriverRead(hDrv, pid, moduleBase + dos.e_lfanew, &nt, sizeof(nt)))
        return false;
    IMAGE_SECTION_HEADER sections[64] = {};
    if (!DriverRead(hDrv, pid, moduleBase + dos.e_lfanew + sizeof(IMAGE_NT_HEADERS64),
        &sections, sizeof(sections)))
        return false;
    for (int i = 0; i < nt.FileHeader.NumberOfSections; i++) {
        char name[9] = {};
        memcpy(name, sections[i].Name, 8);
        g_log << "DBG: Section " << i << " name='" << name << "' VA=0x" << std::hex << sections[i].VirtualAddress << " Size=0x" << sections[i].SizeOfRawData << std::dec << "\n";
        std::ofstream hf("dump_headers.txt", std::ios::app);
        hf << "SECT " << i << " " << name << " VA=0x" << std::hex << sections[i].VirtualAddress
           << " Size=0x" << sections[i].SizeOfRawData
           << " RawPtr=0x" << sections[i].PointerToRawData
           << " RVA=0x" << (sections[i].VirtualAddress + moduleBase) << "\n";
        hf.close();
        if (strcmp(name, sectionName) == 0) {
            outVA = moduleBase + sections[i].VirtualAddress;
            size_t size = sections[i].SizeOfRawData;
            if (!size) return false;
            outData.resize(size);
            memset(outData.data(), 0, size);  // Zero initialize
            const size_t CS_LARGE = 0x10000;   // 64KB fast path
            const size_t CS_SMALL = 0x2000;    // 8KB fallback
            size_t readSuccess = 0;
            size_t totalChunks = (size + CS_SMALL - 1) / CS_SMALL;
            size_t chunkCount = 0;
            std::cout << "Reading " << sectionName << " (" << (size/1024) << " KB)...\n";
            for (size_t off = 0; off < size; ) {
                size_t remain = size - off;
                size_t trySz = min(CS_LARGE, remain);
                bool ok = DriverRead(hDrv, pid, outVA + off, outData.data() + off, trySz);
                if (!ok && trySz > CS_SMALL) {
                    for (size_t sub = 0; sub < trySz && off + sub < size; sub += CS_SMALL) {
                        size_t sz = min(CS_SMALL, size - (off + sub));
                        if (DriverRead(hDrv, pid, outVA + off + sub, outData.data() + off + sub, sz))
                            readSuccess += sz;
                        chunkCount++;
                    }
                    off += trySz;
                } else {
                    if (ok) readSuccess += trySz;
                    off += trySz;
                    chunkCount++;
                }
                if (chunkCount % 1000 == 0) {
                    std::cout << "  " << (chunkCount * 100 / totalChunks) << "%\n";
                    g_log << "Reading " << sectionName << ": " << (chunkCount * 100 / totalChunks) << "%\n";
                }
            }
            std::cout << "Read " << readSuccess << " of " << size << " bytes from section " << sectionName << "\n";
            g_log << "Read " << readSuccess << " of " << size << " bytes from section " << sectionName << "\n";
            if (strcmp(sectionName, ".text") == 0) {
                std::ofstream tf("dump_text.bin", std::ios::binary);
                tf.write((const char*)outData.data(), outData.size());
                tf.close();
                std::cout << "DUMPED dump_text.bin (" << (outData.size()/1024) << " KB)\n";
            } else if (strcmp(sectionName, ".rdata") == 0) {
                std::ofstream rf("dump_rdata.bin", std::ios::binary);
                rf.write((const char*)outData.data(), outData.size());
                rf.close();
                std::cout << "DUMPED dump_rdata.bin (" << (outData.size()/1024) << " KB)\n";
            }
            return readSuccess > 0;  // Success if we read at least some data
        }
    }
    return false;
}

void ScanSection(std::vector<Signature>& sigs, const std::vector<uint8_t>& data,
    uintptr_t sectionVA, uintptr_t moduleBase) {
    for (auto& sig : sigs) {
        if (sig.result) continue;
        size_t end = data.size() - sig.pattern.size();
        for (size_t pos = 0; pos < end; pos++) {
            if (sig.Match(data.data() + pos)) {
                uintptr_t addr = moduleBase + sectionVA + pos;
                if (sig.relative && sig.extra >= 0) {
                    int32_t rel = 0;
                    memcpy(&rel, data.data() + pos + sig.extra, 4);
                    sig.result = addr + sig.extra + 4 + rel;
                } else {
                    sig.result = addr;
                }
                break;
            }
        }
    }
}

int main() {
    XStr logName("scanner.log");
    g_log.open(logName.Decrypt());

    HANDLE hDrv = OpenDriver();
    if (hDrv == INVALID_HANDLE_VALUE) {
        g_log << "ERR: no driver\n";
        return 1;
    }
    g_log << "OK: driver\n";

    const char* targets[] = { "cod22-cod.exe", "cod.exe", "codhq-cod.exe", "ModernWarfare.exe" };
    DWORD pid = 0;

    bool launched = false;
    for (int at = 0; at < 150 && !pid; at++) {
        for (auto t : targets) {
            pid = FindPidViaDriver(hDrv, t);
            if (pid) { g_log << "FOUND: " << t << " PID=" << pid << "\n"; break; }
        }
        if (!pid) {
            for (auto t : targets) {
                wchar_t wb[64];
                mbstowcs(wb, t, 64);
                pid = FindPidFallback(wb);
                if (pid) { g_log << "FOUND(fb): " << t << " PID=" << pid << "\n"; break; }
            }
        }
        if (!pid && at == 10 && !launched) {
            launched = true;
            LaunchGameViaBattlenet();
        }
        if (!pid) EvasiveSleep(2000);
    }
    if (!pid) { std::cout << "ERR: not found\n"; g_log << "ERR: not found\n"; CloseHandle(hDrv); return 1; }

    EvasiveSleep(1000 + (g_rng() % 1000));

    HANDLE hPid = (HANDLE)(ULONG_PTR)pid;
    uintptr_t base = GetBaseViaDriver(hDrv, hPid);
    if (!base) { std::cout << "ERR: no base\n"; g_log << "ERR: no base\n"; CloseHandle(hDrv); return 1; }

    std::cout << "BASE: 0x" << std::hex << base << "\n";
    g_log << "BASE: 0x" << std::hex << base << "\n";

    std::vector<uint8_t> textData, rdataData;
    uintptr_t textVA = 0, rdataVA = 0;
    bool hasText = ReadSection(hDrv, hPid, base, ".text", textData, textVA);
    bool hasRdata = ReadSection(hDrv, hPid, base, ".rdata", rdataData, rdataVA);
    CloseHandle(hDrv);

    if (!hasText) { std::cout << "ERR: no .text\n"; g_log << "ERR: no .text\n"; return 1; }

    std::cout << ".text: 0x" << std::hex << textVA << " (" << std::dec << (textData.size()/1024) << " KB)\n";
    g_log << ".text: 0x" << std::hex << textVA << " (" << std::dec << (textData.size()/1024) << " KB)\n";
    if (hasRdata) {
        std::cout << ".rdata: 0x" << std::hex << rdataVA << " (" << std::dec << (rdataData.size()/1024) << " KB)\n";
        g_log << ".rdata: 0x" << std::hex << rdataVA << " (" << std::dec << (rdataData.size()/1024) << " KB)\n";
    }

    auto patterns = GetPatterns();
    std::vector<Signature> sigs;
    for (auto& p : patterns) sigs.emplace_back(p);

    std::cout << "SCANNING...\n";
    g_log << "SCANNING...\n";
    ScanSection(sigs, textData, textVA, base);
    if (hasRdata) ScanSection(sigs, rdataData, rdataVA, base);

    std::ofstream out("offsets_generated.hpp");
    out << "#pragma once\n#include <cstdint>\n\nnamespace Offsets {\n\n";

    int found = 0, missed = 0;
    for (auto& s : sigs) {
        if (s.result) {
            uintptr_t off = s.result - base;
            out << "    constexpr uintptr_t " << s.name << " = 0x" << std::hex << off << ";\n";
            std::cout << "  " << s.name << " = 0x" << std::hex << off << "\n";
            g_log << "  " << s.name << " = 0x" << std::hex << off << "\n";
            found++;
        } else {
            out << "    // constexpr uintptr_t " << s.name << " = 0x0;\n";
            std::cout << "  " << s.name << " = NOT FOUND\n";
            g_log << "  " << s.name << " = NOT FOUND\n";
            missed++;
        }
    }
    out << "\n} // namespace Offsets\n";
    out.close();

    std::cout << "DONE: " << found << "/" << (found + missed) << " found\n";
    g_log << "DONE: " << found << "/" << (found + missed) << " found\n";
    g_log.close();

    char tmpPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpPath);
    strcat_s(tmpPath, "svctmp.log");
    rename("scanner.log", tmpPath);

    return 0;
}