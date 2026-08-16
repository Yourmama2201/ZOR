#pragma warning(disable: 4311)
#pragma warning(disable: 4100)

#include <ntifs.h>
#include <ntimage.h>
#include "obf.h"
#include "zordriver.h"

#define DWORD ULONG

NTSTATUS MmCopyVirtualMemory(
    PEPROCESS SourceProcess,
    PVOID SourceAddress,
    PEPROCESS TargetProcess,
    PVOID TargetAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T ReturnSize
);

// ZwQuerySystemInformation for process enumeration
NTSTATUS ZwQuerySystemInformation(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
);

NTSTATUS ZwQueryInformationProcess(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

// LdrLoadDll / LdrGetProcedureAddress are resolved at runtime from the target's
// ntdll exports (never linked, so the driver has no user-mode import table).
typedef NTSTATUS(*pfnLdrLoadDll)(PWSTR, ULONG, PUNICODE_STRING, PHANDLE);
typedef NTSTATUS(*pfnLdrGetProcedureAddress)(HANDLE, PUNICODE_STRING, USHORT, PVOID*);

#define LDR_LOAD_BLOCK (const unsigned char[]){0x16,0x3E,0x28,0x16,0x35,0x3B,0x3E,0x1E,0x36,0x36}
#define LDR_GET_PROC_BLOCK (const unsigned char[]){0x16,0x3E,0x28,0x1D,0x3F,0x2E,0x0A,0x28,0x35,0x39,0x3F,0x3E,0x2F,0x28,0x3F,0x1B,0x3E,0x3E,0x28,0x3F,0x29,0x29}

static void DecodeName(char* dst, int dstLen, const unsigned char* block, int blockLen) {
    int n = blockLen < dstLen ? blockLen : dstLen - 1;
    for (int i = 0; i < n; i++) dst[i] = (char)(block[i] ^ SO_KEY);
    dst[n] = '\0';
}

static PVOID FindNtdllExport(ULONG_PTR ntdllBase, const char* name) {
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ntdllBase;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(ntdllBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

    IMAGE_DATA_DIRECTORY expDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!expDir.VirtualAddress || !expDir.Size) return NULL;

    PIMAGE_EXPORT_DIRECTORY ed = (PIMAGE_EXPORT_DIRECTORY)(ntdllBase + expDir.VirtualAddress);
    DWORD* names = (DWORD*)(ntdllBase + ed->AddressOfNames);
    USHORT* ordinals = (USHORT*)(ntdllBase + ed->AddressOfNameOrdinals);
    DWORD* funcs = (DWORD*)(ntdllBase + ed->AddressOfFunctions);

    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        const char* n = (const char*)(ntdllBase + names[i]);
        if (strcmp(n, name) == 0) {
            DWORD rva = funcs[ordinals[i]];
            return (PVOID)(ntdllBase + rva);
        }
    }
    return NULL;
}

static ULONG_PTR GetNtdllBase();

static pfnLdrLoadDll g_LdrLoadDll = NULL;
static pfnLdrGetProcedureAddress g_LdrGetProcAddr = NULL;

static NTSTATUS ResolveNtdllRoutines() {
    if (g_LdrLoadDll && g_LdrGetProcAddr) return STATUS_SUCCESS;
    ULONG_PTR ntdllBase = GetNtdllBase();
    if (!ntdllBase) return STATUS_UNSUCCESSFUL;
    char ldrName[32], getProcName[32];
    DecodeName(ldrName, sizeof(ldrName), LDR_LOAD_BLOCK, (int)sizeof(LDR_LOAD_BLOCK));
    DecodeName(getProcName, sizeof(getProcName), LDR_GET_PROC_BLOCK, (int)sizeof(LDR_GET_PROC_BLOCK));
    g_LdrLoadDll = (pfnLdrLoadDll)FindNtdllExport(ntdllBase, ldrName);
    g_LdrGetProcAddr = (pfnLdrGetProcedureAddress)FindNtdllExport(ntdllBase, getProcName);
    if (!g_LdrLoadDll || !g_LdrGetProcAddr) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}
#define DEVICE_NAME_BLOCK (const unsigned char[]){0x06,0x1E,0x3F,0x2C,0x33,0x39,0x3F,0x06,0x00,0x15,0x08,0x5A}

#define SYMLINK_NAME_BLOCK (const unsigned char[]){0x06,0x1E,0x35,0x29,0x1E,0x3F,0x2C,0x33,0x39,0x3F,0x29,0x06,0x00,0x15,0x08,0x5A}

#define MAX_MEMORY_REQUEST_SIZE 0x400000 // 4MB per request cap

// Validate a user-mode address range before any copy. Rejects kernel addresses,
// non-canonical addresses, NULL page, and overflow/oversized requests so a bad
// IOCTL can never fault the kernel (bugcheck 0x50).
static BOOLEAN IsValidUserAddress(ULONG_PTR address, SIZE_T size) {
    if (size == 0 || size > MAX_MEMORY_REQUEST_SIZE) return FALSE;
    if (address >= 0x0000800000000000ULL) return FALSE; // kernel / non-canonical
    if (address < 0x10000) return FALSE;                // NULL page
    ULONG_PTR end = address + size;
    if (end < address) return FALSE;                    // overflow
    if (end > 0x0000800000000000ULL) return FALSE;      // must not cross into kernel
    return TRUE;
}

#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0

PDEVICE_OBJECT g_DeviceObject = NULL;
UNICODE_STRING g_DevName, g_SymLink;
PDRIVER_OBJECT g_FakeDriverObject = NULL;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    ULONG Flags;
    USHORT LoadCount;
    USHORT TlsIndex;
    LIST_ENTRY HashLinks;
    PVOID SectionPointer;
    ULONG CheckSum;
    ULONG TimeDateStamp;
    PVOID LoadedImports;
    PVOID EntryPointActivationContext;
    PVOID PatchInformation;
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    union {
        BOOLEAN BitField;
        struct {
            BOOLEAN ImageUsesLargePages : 1;
            BOOLEAN IsProtectedProcess : 1;
            BOOLEAN IsImageDynamicallyRelocated : 1;
            BOOLEAN SkipPatchingUserModeForwarders : 1;
            BOOLEAN IsPackagedProcess : 1;
            BOOLEAN IsAppContainer : 1;
            BOOLEAN IsProtectedProcessLight : 1;
            BOOLEAN IsLongPathAwareProcess : 1;
        };
    };
    UCHAR Spare;
    PVOID Mutant;
    PVOID ImageBaseAddress;
    PPEB_LDR_DATA Ldr;
    // ... rest not needed
} PEB, *PPEB;

static ULONG_PTR GetNtdllBase() {
    PEPROCESS cur = PsGetCurrentProcess();
    PROCESS_BASIC_INFORMATION pbi;
    ULONG retLen = 0;
    NTSTATUS st = ZwQueryInformationProcess((HANDLE)cur, ProcessBasicInformation, &pbi, sizeof(pbi), &retLen);
    if (!NT_SUCCESS(st) || !pbi.PebBaseAddress) return 0;

    ULONG_PTR pebAddr = (ULONG_PTR)pbi.PebBaseAddress;
    __try {
        PPEB peb = (PPEB)pebAddr;
        PPEB_LDR_DATA ldr = peb->Ldr;
        if (!ldr) return 0;

        LIST_ENTRY head = ldr->InLoadOrderModuleList;
        PLDR_DATA_TABLE_ENTRY entry = (PLDR_DATA_TABLE_ENTRY)head.Flink;
        for (int i = 0; i < 1024 && (ULONG_PTR)entry != (ULONG_PTR)&head; i++) {
            LDR_DATA_TABLE_ENTRY e;
            RtlCopyMemory(&e, entry, sizeof(e));
            if (e.BaseDllName.Buffer && e.BaseDllName.Length >= 7 * sizeof(WCHAR)) {
                WCHAR nameBuf[16] = { 0 };
                ULONG cb = e.BaseDllName.Length;
                if (cb > 14 * sizeof(WCHAR)) cb = 14 * sizeof(WCHAR);
                RtlCopyMemory(nameBuf, e.BaseDllName.Buffer, cb);
                if (nameBuf[0] == L'n' && nameBuf[1] == L't' && nameBuf[2] == L'd' &&
                    nameBuf[3] == L'l' && nameBuf[4] == L'l') {
                    return (ULONG_PTR)e.DllBase;
                }
            }
            entry = (PLDR_DATA_TABLE_ENTRY)e.InLoadOrderLinks.Flink;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return 0;
}

// Random delay for evasion (0-10ms)
static void RandomDelay() {
    LARGE_INTEGER delay;
    delay.QuadPart = -((LONGLONG)(KeQueryPerformanceCounter(NULL).QuadPart % 1000) * 100); // 0-10ms
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

NTSTATUS ResolveImportsRemotely(PVOID pRemoteDll, PIMAGE_NT_HEADERS nt) {
    DWORD importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRVA) return STATUS_SUCCESS;

    if (!g_LdrLoadDll || !g_LdrGetProcAddr) {
        NTSTATUS rs = ResolveNtdllRoutines();
        if (!NT_SUCCESS(rs)) return rs;
    }

    PIMAGE_IMPORT_DESCRIPTOR imports = (PIMAGE_IMPORT_DESCRIPTOR)((PUCHAR)pRemoteDll + importRVA);
    int resolvedCount = 0;

    for (; imports->Name; imports++) {
        char* moduleName = (char*)((PUCHAR)pRemoteDll + imports->Name);

        ANSI_STRING ansiModName;
        UNICODE_STRING uniModName;
        RtlInitAnsiString(&ansiModName, moduleName);
        NTSTATUS status = RtlAnsiStringToUnicodeString(&uniModName, &ansiModName, TRUE);
        if (!NT_SUCCESS(status)) continue;

        HANDLE hModule = NULL;
        status = g_LdrLoadDll(NULL, 0, &uniModName, &hModule);
        RtlFreeUnicodeString(&uniModName);

        if (!NT_SUCCESS(status) || !hModule) return STATUS_UNSUCCESSFUL;

        PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((PUCHAR)pRemoteDll + imports->OriginalFirstThunk);
        PIMAGE_THUNK_DATA iat = (PIMAGE_THUNK_DATA)((PUCHAR)pRemoteDll + imports->FirstThunk);

        for (; thunk->u1.AddressOfData; thunk++, iat++) {
            PVOID func = NULL;

            if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                LPCSTR ordName = (LPCSTR)(thunk->u1.Ordinal & 0xFFFF);
                ANSI_STRING ansiFunc;
                RtlInitAnsiString(&ansiFunc, ordName);
                UNICODE_STRING uniFunc;
                RtlAnsiStringToUnicodeString(&uniFunc, &ansiFunc, TRUE);
                g_LdrGetProcAddr(hModule, &uniFunc, 0, &func);
                RtlFreeUnicodeString(&uniFunc);
            }
            else {
                PIMAGE_IMPORT_BY_NAME byName = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)pRemoteDll + thunk->u1.AddressOfData);
                ANSI_STRING ansiFunc;
                RtlInitAnsiString(&ansiFunc, byName->Name);
                UNICODE_STRING uniFunc;
                RtlAnsiStringToUnicodeString(&uniFunc, &ansiFunc, TRUE);
                g_LdrGetProcAddr(hModule, &uniFunc, 0, &func);
                RtlFreeUnicodeString(&uniFunc);
            }

            if (!func) return STATUS_UNSUCCESSFUL;

            iat->u1.Function = (ULONGLONG)func;
            resolvedCount++;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS InjectDLL_APC(HANDLE pid, PUCHAR dllData, SIZE_T dllSize, PVOID* outBase) {
    if (KeGetCurrentIrql() > PASSIVE_LEVEL) return STATUS_UNSUCCESSFUL;
    if (!dllData || !dllSize) return STATUS_INVALID_PARAMETER;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)dllData;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return STATUS_INVALID_IMAGE_FORMAT;

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((PUCHAR)dllData + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return STATUS_INVALID_IMAGE_FORMAT;

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) return status;

    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);

    PVOID pRemoteDll = NULL;
    SIZE_T regionSize = nt->OptionalHeader.SizeOfImage;

    status = ZwAllocateVirtualMemory(
        NtCurrentProcess(),
        &pRemoteDll, 0, &regionSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (!NT_SUCCESS(status)) {
        KeUnstackDetachProcess(&apcState);
        ObDereferenceObject(process);
        return status;
    }

    RtlCopyMemory(pRemoteDll, dllData, nt->OptionalHeader.SizeOfHeaders);

    PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        PVOID dest = (PUCHAR)pRemoteDll + sections[i].VirtualAddress;
        PVOID src = (PUCHAR)dllData + sections[i].PointerToRawData;
        SIZE_T size = sections[i].SizeOfRawData;
        if (size > 0) RtlCopyMemory(dest, src, size);
    }

    PIMAGE_DATA_DIRECTORY relocDir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir->Size > 0) {
        PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)((PUCHAR)pRemoteDll + relocDir->VirtualAddress);
        PUCHAR relocEnd = (PUCHAR)reloc + relocDir->Size;
        SIZE_T delta = (SIZE_T)pRemoteDll - nt->OptionalHeader.ImageBase;

        while (reloc < (PIMAGE_BASE_RELOCATION)relocEnd && reloc->SizeOfBlock > 0) {
            PUCHAR page = (PUCHAR)pRemoteDll + reloc->VirtualAddress;
            USHORT* typeOffset = (USHORT*)(reloc + 1);
            ULONG count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(USHORT);

            for (ULONG i = 0; i < count; i++) {
                USHORT entry = typeOffset[i];
                USHORT type = entry >> 12;
                USHORT offset = entry & 0xFFF;

                if (type == IMAGE_REL_BASED_DIR64) {
                    *(ULONG_PTR*)(page + offset) += delta;
                }
            }
            reloc = (PIMAGE_BASE_RELOCATION)((PUCHAR)reloc + reloc->SizeOfBlock);
        }
    }

    status = ResolveImportsRemotely(pRemoteDll, nt);
    if (!NT_SUCCESS(status)) {
        ZwFreeVirtualMemory(NtCurrentProcess(), &pRemoteDll, &regionSize, MEM_RELEASE);
        KeUnstackDetachProcess(&apcState);
        ObDereferenceObject(process);
        return status;
    }

    typedef NTSTATUS(*DLL_ENTRY)(PVOID, ULONG, PVOID);
    DLL_ENTRY entryPoint = (DLL_ENTRY)((PUCHAR)pRemoteDll + nt->OptionalHeader.AddressOfEntryPoint);

    __try {
        entryPoint(pRemoteDll, DLL_PROCESS_ATTACH, NULL);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ZwFreeVirtualMemory(NtCurrentProcess(), &pRemoteDll, &regionSize, MEM_RELEASE);
        KeUnstackDetachProcess(&apcState);
        ObDereferenceObject(process);
        return STATUS_UNSUCCESSFUL;
    }

    if (outBase) *outBase = pRemoteDll;

    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);

    return STATUS_SUCCESS;
}

NTSTATUS ReadMemory(HANDLE pid, ULONG_PTR address, PVOID buffer, SIZE_T size) {
    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) return status;
    if (!IsValidUserAddress(address, size)) {
        ObDereferenceObject(process);
        return STATUS_INVALID_PARAMETER;
    }

    // Add random delay to evade pattern detection
    RandomDelay();

    SIZE_T done = 0;
    __try {
        status = MmCopyVirtualMemory(process, (PVOID)address, PsGetCurrentProcess(), buffer, size, KernelMode, &done);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
        done = 0;
    }
    if (NT_SUCCESS(status) && done == size) status = STATUS_SUCCESS;
    else if (NT_SUCCESS(status)) status = STATUS_PARTIAL_COPY;

    ObDereferenceObject(process);
    return status;
}

NTSTATUS WriteMemory(HANDLE pid, ULONG_PTR address, PVOID buffer, SIZE_T size) {
    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) return status;
    if (!IsValidUserAddress(address, size)) {
        ObDereferenceObject(process);
        return STATUS_INVALID_PARAMETER;
    }

    // Add random delay to evade pattern detection
    RandomDelay();

    SIZE_T done = 0;
    __try {
        status = MmCopyVirtualMemory(PsGetCurrentProcess(), buffer, process, (PVOID)address, size, KernelMode, &done);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
        done = 0;
    }
    if (NT_SUCCESS(status) && done == size) status = STATUS_SUCCESS;
    else if (NT_SUCCESS(status)) status = STATUS_PARTIAL_COPY;

    ObDereferenceObject(process);
    return status;
}

NTSTATUS DeviceControl(PDEVICE_OBJECT device, PIRP irp) {
    UNREFERENCED_PARAMETER(device);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    PVOID buffer = irp->AssociatedIrp.SystemBuffer;
    ULONG inputBufferLength = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outputBufferLength = stack->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG bytes = 0;
    NTSTATUS status = STATUS_SUCCESS;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {

    case IOCTL_INJECT_DLL: {
        if (inputBufferLength < (ULONG)FIELD_OFFSET(INJECT_REQUEST, DllData)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PINJECT_REQUEST req = (PINJECT_REQUEST)buffer;
        if (req && req->DllSize > 0) {
            SIZE_T requiredSize = FIELD_OFFSET(INJECT_REQUEST, DllData) + req->DllSize;
            if (inputBufferLength < (ULONG)requiredSize) {
                status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            PVOID remoteBase = NULL;
            status = InjectDLL_APC(req->ProcessId, req->DllData, req->DllSize, &remoteBase);
            req->Success = NT_SUCCESS(status);
            req->RemoteBase = remoteBase;
            bytes = sizeof(INJECT_REQUEST);
        }
        else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;
    }

    case IOCTL_READ_MEMORY: {
        // Removed debug output for stealth
        PMEMORY_REQUEST req = (PMEMORY_REQUEST)buffer;
        if (inputBufferLength < sizeof(MEMORY_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (!IsValidUserAddress(req->Address, req->Size)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        // METHOD_BUFFERED: inline data after the request must fit the system buffer
        if (inputBufferLength < (ULONG)(sizeof(MEMORY_REQUEST) + req->Size) ||
            outputBufferLength < (ULONG)(sizeof(MEMORY_REQUEST) + req->Size)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        // Add random delay before each read
        RandomDelay();
        PEPROCESS targetProcess = NULL;
        status = PsLookupProcessByProcessId(req->ProcessId, &targetProcess);
        if (!NT_SUCCESS(status)) {
            break;
        }
        SIZE_T done = 0;
        PVOID dataBuffer = (PUCHAR)buffer + sizeof(MEMORY_REQUEST);
        __try {
            status = MmCopyVirtualMemory(targetProcess, (PVOID)req->Address, PsGetCurrentProcess(), dataBuffer, req->Size, KernelMode, &done);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            status = STATUS_ACCESS_VIOLATION;
            done = 0;
        }
        if (NT_SUCCESS(status) && done == req->Size) status = STATUS_SUCCESS;
        else if (NT_SUCCESS(status)) status = STATUS_PARTIAL_COPY;
        ObDereferenceObject(targetProcess);
        bytes = sizeof(MEMORY_REQUEST) + req->Size;
        break;
    }

    case IOCTL_WRITE_MEMORY: {
        PMEMORY_REQUEST req = (PMEMORY_REQUEST)buffer;
        if (inputBufferLength < sizeof(MEMORY_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (!IsValidUserAddress(req->Address, req->Size)) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        if (inputBufferLength < (ULONG)(sizeof(MEMORY_REQUEST) + req->Size)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        // Add random delay before each write
        RandomDelay();
        PEPROCESS targetProcess = NULL;
        status = PsLookupProcessByProcessId(req->ProcessId, &targetProcess);
        if (!NT_SUCCESS(status)) {
            break;
        }
        SIZE_T done = 0;
        PVOID dataBuffer = (PUCHAR)buffer + sizeof(MEMORY_REQUEST);
        __try {
            status = MmCopyVirtualMemory(PsGetCurrentProcess(), dataBuffer, targetProcess, (PVOID)req->Address, req->Size, KernelMode, &done);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            status = STATUS_ACCESS_VIOLATION;
            done = 0;
        }
        if (NT_SUCCESS(status) && done == req->Size) status = STATUS_SUCCESS;
        else if (NT_SUCCESS(status)) status = STATUS_PARTIAL_COPY;
        ObDereferenceObject(targetProcess);
        bytes = sizeof(MEMORY_REQUEST);
        break;
    }

    case IOCTL_GET_PID: {
        // Removed debug output for stealth
        if (buffer && inputBufferLength >= sizeof(HANDLE)) {
            HANDLE callerPid = PsGetCurrentProcessId();
            memcpy(buffer, &callerPid, sizeof(HANDLE));
            bytes = sizeof(HANDLE);
            status = STATUS_SUCCESS;
        }
        break;
    }

    case IOCTL_GET_BASE: {
        // Removed debug output for stealth
        if (buffer && inputBufferLength >= sizeof(HANDLE)) {
            HANDLE targetPid = *(HANDLE*)buffer;
            PEPROCESS targetProcess = NULL;
            NTSTATUS st = PsLookupProcessByProcessId(targetPid, &targetProcess);
            if (NT_SUCCESS(st) && targetProcess) {
                PROCESS_BASIC_INFORMATION pbi;
                ULONG retLen = 0;
                st = ZwQueryInformationProcess(targetProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &retLen);
                if (NT_SUCCESS(st) && pbi.PebBaseAddress) {
                    ULONG_PTR imageBase = 0;
                    SIZE_T done = 0;
                    __try {
                        st = MmCopyVirtualMemory(targetProcess, (PVOID)((PUCHAR)pbi.PebBaseAddress + 0x10),
                            PsGetCurrentProcess(), &imageBase, sizeof(imageBase), KernelMode, &done);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                        st = STATUS_ACCESS_VIOLATION;
                        done = 0;
                    }
                    if (NT_SUCCESS(st) && done == sizeof(imageBase) && imageBase) {
                        memcpy(buffer, &imageBase, sizeof(ULONG_PTR));
                        bytes = sizeof(ULONG_PTR);
                        status = STATUS_SUCCESS;
                    }
                }
                ObDereferenceObject(targetProcess);
            }
        }
        break;
    }

    case IOCTL_FIND_PID: {
        // Removed debug output for stealth
        if (!buffer || inputBufferLength < 2) { status = STATUS_BUFFER_TOO_SMALL; break; }
        CHAR targetName[64] = {0};
        ULONG copyLen = min(inputBufferLength, 63);
        memcpy(targetName, buffer, copyLen);
        targetName[copyLen] = '\0';

        ANSI_STRING targetA;
        RtlInitAnsiString(&targetA, targetName);
        UNICODE_STRING targetU;
        if (!NT_SUCCESS(RtlAnsiStringToUnicodeString(&targetU, &targetA, TRUE))) break;

        HANDLE foundPid = NULL;
        ULONG bufSize = 0x10000;
        PVOID procBuf = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufSize, 0x4E445550);
        if (!procBuf) { RtlFreeUnicodeString(&targetU); break; }

        NTSTATUS ns = ZwQuerySystemInformation(5, procBuf, bufSize, NULL);
        if (ns == STATUS_INFO_LENGTH_MISMATCH) {
            ExFreePool(procBuf);
            bufSize = 0x20000;
            procBuf = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufSize, 0x4E445550);
            if (procBuf) ns = ZwQuerySystemInformation(5, procBuf, bufSize, NULL);
        }

        if (NT_SUCCESS(ns) && procBuf) {
            struct _SPI {
                ULONG NextEntryOffset;
                ULONG NumberOfThreads;
                UCHAR Reserved1[48];
                UNICODE_STRING ImageName;
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
            };
            PUCHAR p = (PUCHAR)procBuf;
            for (;;) {
                struct _SPI* spi = (struct _SPI*)p;
                if (spi->ImageName.Buffer && spi->ImageName.Length && spi->UniqueProcessId) {
                    PWCHAR namePart = spi->ImageName.Buffer;
                    for (ULONG ci = 0; ci < spi->ImageName.Length / sizeof(WCHAR); ci++) {
                        if (spi->ImageName.Buffer[ci] == L'\\')
                            namePart = &spi->ImageName.Buffer[ci + 1];
                    }
                    UNICODE_STRING compStr;
                    RtlInitUnicodeString(&compStr, namePart);
                    if (RtlCompareUnicodeString(&compStr, &targetU, TRUE) == 0) {
                        foundPid = spi->UniqueProcessId;
                        break;
                    }
                }
                if (spi->NextEntryOffset == 0) break;
                p += spi->NextEntryOffset;
            }
        }

        if (procBuf) ExFreePool(procBuf);
        RtlFreeUnicodeString(&targetU);

        if (foundPid && outputBufferLength >= sizeof(HANDLE)) {
            memcpy(buffer, &foundPid, sizeof(HANDLE));
            bytes = sizeof(HANDLE);
            status = STATUS_SUCCESS;
        }
        break;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS DispatchCreateClose(PDEVICE_OBJECT device, PIRP irp) {
    UNREFERENCED_PARAMETER(device);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT driver) {
    UNREFERENCED_PARAMETER(driver);
    if (g_DeviceObject) {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
    }
    IoDeleteSymbolicLink(&g_SymLink);
    if (g_FakeDriverObject) {
        ExFreePoolWithTag(g_FakeDriverObject, 0x5A4F5244);
        g_FakeDriverObject = NULL;
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);

    // Manual-mapped (kdmapper) drivers get a NULL DriverObject. IoCreateDevice
    // needs a real DRIVER_OBJECT, so hand it a non-paged fake one. Normally
    // loaded drivers use the SCM-provided object instead.
    if (!driver) {
        g_FakeDriverObject = (PDRIVER_OBJECT)ExAllocatePoolWithTag(
            NonPagedPool, sizeof(DRIVER_OBJECT), 0x5A4F5244);
        if (!g_FakeDriverObject) return STATUS_INSUFFICIENT_RESOURCES;
        RtlZeroMemory(g_FakeDriverObject, sizeof(DRIVER_OBJECT));
        driver = g_FakeDriverObject;
    }

    driver->DriverUnload = DriverUnload;

    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        driver->MajorFunction[i] = DispatchCreateClose;
    }
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    driver->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    driver->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;

    // Decode device/symlink names at runtime (never plaintext in the image).
    WCHAR devName[65], symName[65];
    char devA[65], symA[65];
    SO_DEC(devA, DEVICE_NAME_BLOCK);
    SO_DEC(symA, SYMLINK_NAME_BLOCK);
    for (int i = 0; i < 65; i++) { devName[i] = (WCHAR)devA[i]; symName[i] = (WCHAR)symA[i]; }

    RtlInitUnicodeString(&g_DevName, devName);
    RtlInitUnicodeString(&g_SymLink, symName);

    NTSTATUS status = IoCreateDevice(driver, 0, &g_DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_DeviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(&g_SymLink, &g_DevName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_DeviceObject);
        return status;
    }

    return STATUS_SUCCESS;
}
