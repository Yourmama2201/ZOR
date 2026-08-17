#include <Windows.h>
#include <tlhelp32.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cstdlib>
#include <cstring>

// =================================================================
// 1. SECRET KEY - DO NOT SHARE THIS WITH ANYONE!
// =================================================================
static const std::string SECRET_KEY = "key1111111";
static const std::string LICENSE_FILE = "license.lic";

// =================================================================
// 2. SIMPLE XOR ENCRYPTION
// =================================================================
std::string XOREncryptDecrypt(const std::string& input, const std::string& key) {
    std::string output = input;
    for (size_t i = 0; i < output.size(); ++i) {
        output[i] = output[i] ^ key[i % key.size()];
    }
    return output;
}

// =================================================================
// 3. LICENSE GENERATOR
// =================================================================
void GenerateLicense(int days) {
    auto now = std::chrono::system_clock::now();
    auto expiry = now + std::chrono::hours(24 * days);
    auto expiryTime = std::chrono::system_clock::to_time_t(expiry);

    std::string data = std::to_string(expiryTime) + "|" + std::to_string(days);
    std::string encrypted = XOREncryptDecrypt(data, SECRET_KEY);

    std::ofstream file(LICENSE_FILE, std::ios::binary);
    if (file.is_open()) {
        for (unsigned char c : encrypted) {
            file << std::hex << std::setw(2) << std::setfill('0') << (int)c;
        }
        file.close();
        MessageBoxA(NULL, ("License generated for " + std::to_string(days) + " days.\nFile: " + LICENSE_FILE).c_str(), "Success", MB_OK);
    } else {
        MessageBoxA(NULL, "Failed to write the license file!", "Error", MB_ICONERROR);
    }
}

// =================================================================
// 4. LICENSE VALIDATOR
// =================================================================
bool ValidateLicense() {
    if (!std::filesystem::exists(LICENSE_FILE)) {
        MessageBoxA(NULL, "License file (license.lic) not found!\nRun the loader with --gen [days] to create one.", "License", MB_OK | MB_ICONERROR);
        return false;
    }

    std::ifstream file(LICENSE_FILE);
    if (!file.is_open()) return false;

    std::string hexData;
    file >> hexData;
    file.close();

    std::string encrypted;
    for (size_t i = 0; i < hexData.length(); i += 2) {
        std::string byteStr = hexData.substr(i, 2);
        char byte = (char)strtol(byteStr.c_str(), NULL, 16);
        encrypted.push_back(byte);
    }

    std::string decrypted = XOREncryptDecrypt(encrypted, SECRET_KEY);

    size_t delim = decrypted.find('|');
    if (delim == std::string::npos) {
        MessageBoxA(NULL, "Corrupted license file!", "Error", MB_ICONERROR);
        return false;
    }

    std::string timestampStr = decrypted.substr(0, delim);
    time_t expiryTime = (time_t)std::stoll(timestampStr);
    auto now = std::chrono::system_clock::now();
    time_t currentTime = std::chrono::system_clock::to_time_t(now);

    if (currentTime > expiryTime) {
        MessageBoxA(NULL, "License has expired!", "Expired", MB_OK | MB_ICONERROR);
        return false;
    }

    if (currentTime < expiryTime - 60 * 60 * 24 * 365 * 10) {
        MessageBoxA(NULL, "System date is incorrect!", "Error", MB_ICONERROR);
        return false;
    }

    return true;
}

// =================================================================
// 5. EXTRACT DLL FROM RESOURCES
// =================================================================
bool ExtractResourceToMemory(int resourceId, std::vector<BYTE>& outBuffer) {
    HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW((WORD)((ULONG_PTR)RT_RCDATA & 0xFFFF)));
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;

    void* pData = LockResource(hData);
    DWORD dataSize = SizeofResource(NULL, hRes);
    if (!pData || dataSize == 0) return false;

    outBuffer.resize(dataSize);
    memcpy(outBuffer.data(), pData, dataSize);
    return true;
}

// =================================================================
// 6. FIND PROCESS BY NAME
// =================================================================
DWORD GetProcessIdByName(const std::wstring& processName) {
    DWORD processId = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (processName == entry.szExeFile) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return processId;
}

// =================================================================
// 7. MANUAL MAP INJECTION (no LoadLibraryW, no files on disk)
// =================================================================

// Return the load address of a module inside the target process.
HMODULE GetRemoteModuleBase(DWORD processId, const wchar_t* moduleName) {
    HMODULE result = NULL;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snap == INVALID_HANDLE_VALUE) return NULL;

    MODULEENTRY32W entry = { sizeof(entry) };
    if (Module32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName) == 0) {
                result = entry.hModule;
                break;
            }
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return result;
}

// Look up one exported function in a module inside the target process.
// Returns moduleBase + function RVA (i.e. the absolute address in the target).
uintptr_t GetRemoteExport(HANDLE hProcess, uintptr_t moduleBase, const char* funcName) {
    IMAGE_DOS_HEADER dos = {};
    IMAGE_NT_HEADERS64 nt = {};
    if (!ReadProcessMemory(hProcess, (LPCVOID)moduleBase, &dos, sizeof(dos), NULL)) return 0;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + dos.e_lfanew), &nt, sizeof(nt), NULL)) return 0;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;

    DWORD exportRva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportSize = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    if (!exportRva) return 0;

    IMAGE_EXPORT_DIRECTORY exp = {};
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + exportRva), &exp, sizeof(exp), NULL)) return 0;

    std::vector<DWORD> names(exp.NumberOfNames);
    std::vector<WORD> ordinals(exp.NumberOfNames);
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + exp.AddressOfNames), names.data(), names.size() * sizeof(DWORD), NULL)) return 0;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + exp.AddressOfNameOrdinals), ordinals.data(), ordinals.size() * sizeof(WORD), NULL)) return 0;

    for (DWORD i = 0; i < exp.NumberOfNames; i++) {
        char name[256] = {};
        if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + names[i]), name, sizeof(name) - 1, NULL)) continue;
        if (strcmp(name, funcName) == 0) {
            DWORD funcRva = 0;
            if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + exp.AddressOfFunctions + (DWORD)ordinals[i] * 4), &funcRva, sizeof(funcRva), NULL)) return 0;
            return moduleBase + funcRva;
        }
    }
    return 0;
}

// Look up an exported function by ordinal (for ordinal-style imports).
uintptr_t GetRemoteExportByOrdinal(HANDLE hProcess, uintptr_t moduleBase, WORD ordinal) {
    IMAGE_DOS_HEADER dos = {};
    IMAGE_NT_HEADERS64 nt = {};
    if (!ReadProcessMemory(hProcess, (LPCVOID)moduleBase, &dos, sizeof(dos), NULL)) return 0;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + dos.e_lfanew), &nt, sizeof(nt), NULL)) return 0;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;

    DWORD exportRva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportRva) return 0;

    IMAGE_EXPORT_DIRECTORY exp = {};
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + exportRva), &exp, sizeof(exp), NULL)) return 0;

    if (ordinal < exp.Base) return 0;
    WORD index = (WORD)(ordinal - exp.Base);
    if (index >= exp.NumberOfFunctions) return 0;

    DWORD funcRva = 0;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(moduleBase + exp.AddressOfFunctions + (DWORD)index * 4), &funcRva, sizeof(funcRva), NULL)) return 0;
    return moduleBase + funcRva;
}

// Resolve a single imported function inside the target process.
// Follows forwarders (e.g. "KERNEL32.HeapAlloc" -> ntdll, or "api-ms-win-*" chains).
uintptr_t ResolveImport(HANDLE hProcess, DWORD processId, const char* moduleName, const char* funcName, WORD ordinal = 0) {
    wchar_t wideName[128] = {};
    MultiByteToWideChar(CP_ACP, 0, moduleName, -1, wideName, 128);

    uintptr_t modBase = (uintptr_t)GetRemoteModuleBase(processId, wideName);
    if (!modBase) return 0;

    uintptr_t addr = ordinal ? GetRemoteExportByOrdinal(hProcess, modBase, ordinal)
                             : GetRemoteExport(hProcess, modBase, funcName);
    if (!addr) return 0;

    // If the resolved address lands inside the export directory of that module,
    // it is a forwarder string like "NTDLL.RtlAllocateHeap".
    IMAGE_DOS_HEADER dos = {};
    IMAGE_NT_HEADERS64 nt = {};
    if (!ReadProcessMemory(hProcess, (LPCVOID)modBase, &dos, sizeof(dos), NULL)) return addr;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(modBase + dos.e_lfanew), &nt, sizeof(nt), NULL)) return addr;

    DWORD exportRva = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportSize = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    if (exportRva && addr >= modBase + exportRva && addr < modBase + exportRva + exportSize) {
        char fwd[256] = {};
        ReadProcessMemory(hProcess, (LPCVOID)addr, fwd, sizeof(fwd) - 1, NULL);
        char* dot = strchr(fwd, '.');
        if (dot) {
            *dot = 0;
            char targetMod[128] = {};
            strncpy_s(targetMod, fwd, sizeof(targetMod) - 1);
            strncat_s(targetMod, ".dll", sizeof(targetMod) - strlen(targetMod) - 1);
            return ResolveImport(hProcess, processId, targetMod, dot + 1);
        }
    }
    return addr;
}

// x64 thread stub that runs inside the target process:
//   1) RtlAddFunctionTable(pdata)  - register .pdata so C++ exceptions work
//   2) DllMain(imageBase, DLL_PROCESS_ATTACH, NULL)
// Offsets are field-based so the compiler lays them out for us.
#pragma pack(push,1)
struct ManualMapStub {
    uint8_t  sub_rsp[4];         // 48 83 EC 28              sub rsp, 0x28
    uint8_t  mov_rcx_pdata[2];   // 48 B9                    mov rcx, imm64
    uint64_t pdataAddr;
    uint8_t  mov_edx_count;      // BA                       mov edx, imm32
    uint32_t pdataCount;
    uint8_t  mov_r8_base[2];     // 49 B8                    mov r8, imm64
    uint64_t imageBaseForPdata;
    uint8_t  mov_rax_rtl[2];     // 48 B8                    mov rax, imm64
    uint64_t rtlAddFunctionTable;
    uint8_t  call_rax[2];        // FF D0                    call rax
    uint8_t  mov_rcx_dll[2];     // 48 B9                    mov rcx, imm64
    uint64_t dllBase;
    uint8_t  mov_edx_reason[5];  // BA 01 00 00 00           mov edx, 1 (ATTACH)
    uint8_t  xor_r8[3];          // 45 31 C0                 xor r8d, r8d
    uint8_t  mov_rax_entry[2];   // 48 B8                    mov rax, imm64
    uint64_t entryPoint;
    uint8_t  call_rax2[2];       // FF D0                    call rax
    uint8_t  add_rsp[4];         // 48 83 C4 28              add rsp, 0x28
    uint8_t  xor_eax[2];         // 33 C0                    xor eax, eax
    uint8_t  ret;                // C3                       ret
};
#pragma pack(pop)

bool ManualMapInject(DWORD processId, const std::vector<BYTE>& dllBuffer) {
    // ---- Parse PE from the in-memory buffer (nothing written to disk) ----
    if (dllBuffer.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    const IMAGE_DOS_HEADER* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(dllBuffer.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > dllBuffer.size()) return false;

    const IMAGE_NT_HEADERS64* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(dllBuffer.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        MessageBoxW(NULL, L"The DLL must be a 64-bit (x64) binary!", L"Error", MB_ICONERROR);
        return false;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        MessageBoxW(NULL, L"Failed to open the process!\nRun as Administrator.", L"Error", MB_ICONERROR);
        return false;
    }

    const uintptr_t imageBase  = nt->OptionalHeader.ImageBase;
    const DWORD     imageSize  = nt->OptionalHeader.SizeOfImage;

    // ---- Allocate and map the image into the target ----
    uintptr_t remoteBase = (uintptr_t)VirtualAllocEx(hProcess, NULL, imageSize,
                                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        CloseHandle(hProcess);
        return false;
    }

    // Headers
    WriteProcessMemory(hProcess, (LPVOID)remoteBase, dllBuffer.data(),
                       nt->OptionalHeader.SizeOfHeaders, NULL);

    // Sections
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, section++) {
        if (section->SizeOfRawData == 0) continue;
        if (section->PointerToRawData + section->SizeOfRawData > dllBuffer.size()) continue;
        WriteProcessMemory(hProcess, (LPVOID)(remoteBase + section->VirtualAddress),
                           dllBuffer.data() + section->PointerToRawData,
                           section->SizeOfRawData, NULL);
    }

    // ---- Relocations (fix up since we almost never land at ImageBase) ----
    intptr_t delta = (intptr_t)(remoteBase - imageBase);
    if (delta != 0) {
        IMAGE_DATA_DIRECTORY relocDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocDir.VirtualAddress && relocDir.Size) {
            size_t offset = 0;
            while (offset < relocDir.Size) {
                IMAGE_BASE_RELOCATION block = {};
                if (!ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + relocDir.VirtualAddress + offset),
                                       &block, sizeof(block), NULL)) break;
                if (block.SizeOfBlock == 0) break;

                DWORD numEntries = (block.SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                std::vector<WORD> entries(numEntries);
                ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + relocDir.VirtualAddress + offset + sizeof(IMAGE_BASE_RELOCATION)),
                                  entries.data(), numEntries * sizeof(WORD), NULL);

                for (DWORD e = 0; e < numEntries; e++) {
                    WORD type = entries[e] >> 12;
                    WORD off  = entries[e] & 0xFFF;
                    if (type == IMAGE_REL_BASED_DIR64) {
                        uintptr_t val = 0;
                        ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + block.VirtualAddress + off),
                                          &val, sizeof(val), NULL);
                        val += delta;
                        WriteProcessMemory(hProcess, (LPVOID)(remoteBase + block.VirtualAddress + off),
                                           &val, sizeof(val), NULL);
                    }
                }
                offset += block.SizeOfBlock;
            }
        }
    }

    // ---- Imports: walk the import directory, resolve each function in the
    //      target's own module list, write the resolved IAT into the mapped image ----
    IMAGE_DATA_DIRECTORY importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.VirtualAddress) {
        size_t descIndex = 0;
        for (;;) {
            IMAGE_IMPORT_DESCRIPTOR desc = {};
            if (!ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + importDir.VirtualAddress + descIndex * sizeof(desc)),
                                   &desc, sizeof(desc), NULL)) break;
            if (desc.Name == 0 && desc.FirstThunk == 0) break; // end of table

            char moduleName[128] = {};
            ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + desc.Name), moduleName, sizeof(moduleName) - 1, NULL);

            uintptr_t thunkRva = desc.OriginalFirstThunk ? desc.OriginalFirstThunk : desc.FirstThunk;
            uintptr_t iatRva   = desc.FirstThunk;

            for (size_t t = 0; ; t++) {
                uintptr_t thunkVal = 0;
                ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + thunkRva + t * sizeof(uintptr_t)),
                                  &thunkVal, sizeof(thunkVal), NULL);
                if (!thunkVal) break;

                uintptr_t resolved = 0;
                if (thunkVal & IMAGE_ORDINAL_FLAG64) {
                    // Imported by ordinal - resolve by index in that module's exports
                    WORD ordinal = (WORD)(thunkVal & 0xFFFF);
                    wchar_t wideMod[128] = {};
                    MultiByteToWideChar(CP_ACP, 0, moduleName, -1, wideMod, 128);
                    uintptr_t modBase = (uintptr_t)GetRemoteModuleBase(processId, wideMod);
                    if (modBase) resolved = GetRemoteExportByOrdinal(hProcess, modBase, ordinal);
                } else {
                    char funcName[128] = {};
                    ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + thunkVal + sizeof(WORD)),
                                      funcName, sizeof(funcName) - 1, NULL);
                    resolved = ResolveImport(hProcess, processId, moduleName, funcName);
                }

                if (!resolved) {
                    CloseHandle(hProcess);
                    return false; // failed to resolve an import - abort safely
                }
                WriteProcessMemory(hProcess, (LPVOID)(remoteBase + iatRva + t * sizeof(uintptr_t)),
                                   &resolved, sizeof(resolved), NULL);
            }
            descIndex++;
        }
    }

    // ---- Prepare the .pdata registration so C++ exceptions work in the DLL ----
    bool havePdata = false;
    uintptr_t pdataAddr = remoteBase;
    DWORD pdataCount = 0;
    IMAGE_DATA_DIRECTORY pdataDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (pdataDir.VirtualAddress && pdataDir.Size >= sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY)) {
        pdataAddr  = remoteBase + pdataDir.VirtualAddress;
        pdataCount = pdataDir.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
        havePdata  = true;
    }

    uintptr_t ntdllBase = (uintptr_t)GetRemoteModuleBase(processId, L"ntdll.dll");
    uintptr_t rtlAddFuncTable = havePdata ? GetRemoteExport(hProcess, ntdllBase, "RtlAddFunctionTable") : 0;

    // ---- Build the stub and launch it ----
    ManualMapStub stub = {};
    stub.sub_rsp[0] = 0x48; stub.sub_rsp[1] = 0x83; stub.sub_rsp[2] = 0xEC; stub.sub_rsp[3] = 0x28;
    stub.mov_rcx_pdata[0] = 0x48; stub.mov_rcx_pdata[1] = 0xB9;
    stub.pdataAddr = pdataAddr;
    stub.mov_edx_count = 0xBA;
    stub.pdataCount = pdataCount;
    stub.mov_r8_base[0] = 0x49; stub.mov_r8_base[1] = 0xB8;
    stub.imageBaseForPdata = imageBase;
    stub.mov_rax_rtl[0] = 0x48; stub.mov_rax_rtl[1] = 0xB8;
    stub.rtlAddFunctionTable = rtlAddFuncTable;
    stub.call_rax[0] = 0xFF; stub.call_rax[1] = 0xD0;
    stub.mov_rcx_dll[0] = 0x48; stub.mov_rcx_dll[1] = 0xB9;
    stub.dllBase = remoteBase;
    stub.mov_edx_reason[0] = 0xBA; stub.mov_edx_reason[1] = 0x01;
    stub.mov_edx_reason[2] = 0x00; stub.mov_edx_reason[3] = 0x00; stub.mov_edx_reason[4] = 0x00;
    stub.xor_r8[0] = 0x45; stub.xor_r8[1] = 0x31; stub.xor_r8[2] = 0xC0;
    stub.mov_rax_entry[0] = 0x48; stub.mov_rax_entry[1] = 0xB8;
    stub.entryPoint = remoteBase + nt->OptionalHeader.AddressOfEntryPoint;
    stub.call_rax2[0] = 0xFF; stub.call_rax2[1] = 0xD0;
    stub.add_rsp[0] = 0x48; stub.add_rsp[1] = 0x83; stub.add_rsp[2] = 0xC4; stub.add_rsp[3] = 0x28;
    stub.xor_eax[0] = 0x33; stub.xor_eax[1] = 0xC0;
    stub.ret = 0xC3;

    LPVOID stubMem = VirtualAllocEx(hProcess, NULL, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stubMem) {
        CloseHandle(hProcess);
        return false;
    }
    WriteProcessMemory(hProcess, stubMem, &stub, sizeof(stub), NULL);

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)stubMem,
                                        (LPVOID)remoteBase, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, stubMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, stubMem, 0, MEM_RELEASE); // free stub only; the mapped DLL stays
    CloseHandle(hProcess);

    return true;
}

// =================================================================
// 8. MAIN ENTRY - AUTO-GENERATE 30 DAY LICENSE
// =================================================================
int main(int argc, char* argv[]) {
    FreeConsole();

    if (argc >= 3 && strcmp(argv[1], "--gen") == 0) {
        int days = atoi(argv[2]);
        if (days <= 0) {
            MessageBoxA(NULL, "Enter a valid number of days (e.g. --gen 30)", "Error", MB_ICONERROR);
            return 1;
        }
        GenerateLicense(days);
        return 0;
    }

    if (!std::filesystem::exists(LICENSE_FILE)) {
        GenerateLicense(30);
    }

    if (!ValidateLicense()) {
        return 1;
    }

    const std::wstring gameProcess = L"cod.exe"; // CHANGE THIS TO YOUR GAME!

    const int resourceId = 101;

    std::vector<BYTE> dllBuffer;
    if (!ExtractResourceToMemory(resourceId, dllBuffer)) {
        MessageBoxW(NULL, L"DLL resource not found in the EXE!\nAdd a resource.rc with IDR_DLL1.", L"Fatal error", MB_ICONERROR);
        return 1;
    }

    DWORD pid = 0;
    while (pid == 0) {
        pid = GetProcessIdByName(gameProcess);
        Sleep(500);
    }

    if (!ManualMapInject(pid, dllBuffer)) {
        MessageBoxW(NULL, L"Injection failed!", L"Error", MB_ICONERROR);
        return 1;
    }

    return 0;
}
