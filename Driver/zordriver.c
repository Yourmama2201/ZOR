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

// NtCreateThreadEx is not exported by ntoskrnl.lib; resolved at runtime.
typedef NTSTATUS(*pfnNtCreateThreadEx)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID StartRoutine,
    PVOID Argument,
    ULONG CreateFlags,
    SIZE_T ZeroBits,
    SIZE_T StackSize,
    SIZE_T MaximumStackSize,
    PULONG AttributeList
);

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

// Documented NT export (not declared in this WDK's headers). Creates a REAL
// DRIVER_OBJECT via ObCreateObject + ObInsertObject and calls the supplied
// entry routine with it -- so IoCreateDevice's internal ObfReferenceObject
// sees a valid object header (a pool-allocated fake object BSODs with 0x18).
NTKERNELAPI
NTSTATUS
IoCreateDriver(
    _In_opt_ PUNICODE_STRING DriverName,
    _In_ PDRIVER_INITIALIZE InitializationRoutine
    );

static NTSTATUS RealDriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry);

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

// Find an export by name in the remote (manually-mapped) DLL image.
static PVOID FindRemoteExport(PVOID imageBase, PIMAGE_NT_HEADERS nt, const char* wanted) {
    PIMAGE_DATA_DIRECTORY edDir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!edDir->VirtualAddress || !edDir->Size) return NULL;
    PIMAGE_EXPORT_DIRECTORY ed = (PIMAGE_EXPORT_DIRECTORY)((PUCHAR)imageBase + edDir->VirtualAddress);
    ULONG* names = (ULONG*)((PUCHAR)imageBase + ed->AddressOfNames);
    USHORT* ordinals = (USHORT*)((PUCHAR)imageBase + ed->AddressOfNameOrdinals);
    ULONG* funcs = (ULONG*)((PUCHAR)imageBase + ed->AddressOfFunctions);

    SIZE_T wantLen = 0;
    while (wanted[wantLen]) wantLen++;

    for (ULONG i = 0; i < ed->NumberOfNames; i++) {
        const char* name = (const char*)((PUCHAR)imageBase + names[i]);
        SIZE_T n = 0;
        while (name[n]) n++;
        if (n == wantLen) {
            BOOLEAN same = TRUE;
            for (SIZE_T k = 0; k < wantLen; k++) {
                if (name[k] != wanted[k]) { same = FALSE; break; }
            }
            if (same) {
                ULONG ord = ordinals[i];
                return (PUCHAR)imageBase + funcs[ord];
            }
        }
    }
    return NULL;
}

NTSTATUS InjectDLL_Allocate(HANDLE pid, SIZE_T sizeOfImage, PVOID* outBase) {
    if (KeGetCurrentIrql() > PASSIVE_LEVEL) return STATUS_UNSUCCESSFUL;
    if (!sizeOfImage) return STATUS_INVALID_PARAMETER;

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) return status;

    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);

    PVOID pRemoteDll = NULL;
    SIZE_T regionSize = sizeOfImage;
    status = ZwAllocateVirtualMemory(
        NtCurrentProcess(),
        &pRemoteDll, 0, &regionSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);

    if (!NT_SUCCESS(status)) return status;
    if (outBase) *outBase = pRemoteDll;
    return STATUS_SUCCESS;
}

// Find any thread owned by the target process (used for thread hijacking).
static NTSTATUS FindTargetThread(PEPROCESS process, ULONG_PTR* outThreadId) {
    if (!process || !outThreadId) return STATUS_INVALID_PARAMETER;
    HANDLE targetPid = PsGetProcessId(process);
    if (!targetPid) return STATUS_INVALID_PARAMETER;

    ULONG bufSize = 0x10000;
    PVOID procBuf = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufSize, 0x4E445550);
    if (!procBuf) return STATUS_INSUFFICIENT_RESOURCES;

    NTSTATUS ns = ZwQuerySystemInformation(5, procBuf, bufSize, NULL);
    if (ns == STATUS_INFO_LENGTH_MISMATCH) {
        ExFreePool(procBuf);
        bufSize = 0x20000;
        procBuf = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufSize, 0x4E445550);
        if (!procBuf) return STATUS_INSUFFICIENT_RESOURCES;
        ns = ZwQuerySystemInformation(5, procBuf, bufSize, NULL);
    }

    if (!NT_SUCCESS(ns)) {
        ExFreePool(procBuf);
        return ns;
    }

    struct _SPI {
        ULONG NextEntryOffset;
        ULONG NumberOfThreads;
        LARGE_INTEGER WorkingSetPrivateSize;
        ULONG HardFaultCount;
        ULONG NumberOfThreadsHighWatermark;
        ULONGLONG CycleTime;
        LARGE_INTEGER CreateTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER KernelTime;
        UNICODE_STRING ImageName;
        LONG BasePriority;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR InheritedFromUniqueProcessId;
        ULONG HandleCount;
        ULONG SessionId;
        ULONG_PTR UniqueProcessKey;
        ULONG_PTR PeakVirtualSize;
        ULONG_PTR VirtualSize;
        ULONG PageFaultCount;
        ULONG_PTR PeakWorkingSetSize;
        ULONG_PTR WorkingSetSize;
        ULONG_PTR QuotaPeakPagedPoolUsage;
        ULONG_PTR QuotaPagedPoolUsage;
        ULONG_PTR QuotaPeakNonPagedPoolUsage;
        ULONG_PTR QuotaNonPagedPoolUsage;
        ULONG_PTR PagefileUsage;
        ULONG_PTR PeakPagefileUsage;
        ULONG_PTR PrivatePageCount;
        LARGE_INTEGER ReadOperationCount;
        LARGE_INTEGER WriteOperationCount;
        LARGE_INTEGER OtherOperationCount;
        LARGE_INTEGER ReadTransferCount;
        LARGE_INTEGER WriteTransferCount;
        LARGE_INTEGER OtherTransferCount;
    };
    // SYSTEM_THREAD_INFORMATION (array follows the process struct).
    struct _STI {
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER CreateTime;
        ULONG WaitTime;
        PVOID StartAddress;
        CLIENT_ID ClientId;
        LONG Priority;
        LONG BasePriority;
        ULONG ContextSwitches;
        ULONG ThreadState;
        ULONG WaitReason;
    };

    NTSTATUS result = STATUS_NOT_FOUND;
    PUCHAR p = (PUCHAR)procBuf;
    for (;;) {
        struct _SPI* spi = (struct _SPI*)p;
        if ((ULONG_PTR)spi->UniqueProcessId == (ULONG_PTR)targetPid) {
            struct _STI* threads = (struct _STI*)((PUCHAR)spi + sizeof(struct _SPI));
            // Prefer a thread that is Ready(1)/Running(2)/Standby(3) so it
            // returns to user mode soon and picks up our hijacked context.
            // ThreadState 5 (Waiting) threads may be parked in a kernel wait
            // and never come back, so only use them as a last resort.
            ULONG fallback = 0xFFFFFFFF;
            for (ULONG i = 0; i < spi->NumberOfThreads && i < 256; i++) {
                struct _STI* t = &threads[i];
                if (!t->ClientId.UniqueThread) continue;
                if (t->ThreadState == 1 || t->ThreadState == 2 || t->ThreadState == 3) {
                    *outThreadId = (ULONG_PTR)t->ClientId.UniqueThread;
                    result = STATUS_SUCCESS;
                    break;
                }
                // Last resort: any live thread (not Initialized/Terminated).
                if (fallback == 0xFFFFFFFF && t->ThreadState != 0 && t->ThreadState != 4) {
                    fallback = (ULONG_PTR)t->ClientId.UniqueThread;
                }
            }
            if (!NT_SUCCESS(result) && fallback != 0xFFFFFFFF) {
                *outThreadId = fallback;
                result = STATUS_SUCCESS;
            }
            break;
        }
        if (spi->NextEntryOffset == 0) break;
        p += spi->NextEntryOffset;
    }

    ExFreePool(procBuf);
    return result;
}

NTSTATUS InjectDLL_Exec(HANDLE pid, PVOID pRemoteDll, PVOID* outBase) {
    if (KeGetCurrentIrql() > PASSIVE_LEVEL) return STATUS_UNSUCCESSFUL;
    if (!pRemoteDll) return STATUS_INVALID_PARAMETER;

    // Read the remote PE header to drive relocation/import/entry processing.
    // pRemoteDll lives in the target process; attach so direct derefs work.
    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) return status;

    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);

    PIMAGE_DOS_HEADER dos = NULL;
    PIMAGE_NT_HEADERS nt = NULL;
    __try {
        dos = (PIMAGE_DOS_HEADER)pRemoteDll;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            status = STATUS_INVALID_IMAGE_FORMAT;
            goto done;
        }
        nt = (PIMAGE_NT_HEADERS)((PUCHAR)pRemoteDll + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            status = STATUS_INVALID_IMAGE_FORMAT;
            goto done;
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
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
        goto done;
    }

    status = ResolveImportsRemotely(pRemoteDll, nt);
    if (!NT_SUCCESS(status)) goto done;

    // The client exports ZorMain(), which runs the whole cheat inline on the
    // hijacked thread and never returns. This avoids NtCreateThreadEx entirely
    // (Ricochet hooks thread creation).
    PVOID zorMain = FindRemoteExport(pRemoteDll, nt, "ZorMain");
    if (!zorMain) { status = STATUS_PROCEDURE_NOT_FOUND; goto done; }

    typedef NTSTATUS(*DLL_ENTRY)(PVOID, ULONG, PVOID);
    DLL_ENTRY entryPoint = (DLL_ENTRY)((PUCHAR)pRemoteDll + nt->OptionalHeader.AddressOfEntryPoint);
    if (!entryPoint) { status = STATUS_INVALID_IMAGE_FORMAT; goto done; }

    // Run the entry point on a REAL user-mode thread in the target via thread
    // hijacking. Calling it directly from this attached kernel thread lacks a
    // user TEB/TLS/thread context, so DllMain (CreateThread/CRT) faults. We
    // hijack an existing game thread instead of creating a new one (Ricochet
    // hooks thread creation).
    PVOID stubAddr = NULL;
    SIZE_T stubSize = 0x1000;
    status = ZwAllocateVirtualMemory(
        NtCurrentProcess(),
        &stubAddr, 0, &stubSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );
    if (!NT_SUCCESS(status)) goto done;

// Create the user thread in the target via SPECIAL USER APC (no
    // NtCreateThreadEx -- Ricochet hooks thread creation). We queue a user
    // APC to an existing game thread; it fires on the thread's next
    // kernel->user transition and runs the stub in the target process.
    PETHREAD hThread = NULL;
    {
        ULONG_PTR threadId = 0;
        PEPROCESS p = process;
        status = FindTargetThread(p, &threadId);
        if (!NT_SUCCESS(status) || !threadId) {
            status = STATUS_NOT_FOUND;
            ZwFreeVirtualMemory(NtCurrentProcess(), &stubAddr, &stubSize, MEM_RELEASE);
            goto done;
        }
        status = PsLookupThreadByThreadId((HANDLE)threadId, &hThread);
        if (!NT_SUCCESS(status)) {
            ZwFreeVirtualMemory(NtCurrentProcess(), &stubAddr, &stubSize, MEM_RELEASE);
            goto done;
        }
    }

    // Flags the stub sets: ranFlag = 1 on first instruction (before DllMain),
    // doneFlag = 1 after the entry point returns. Placed at stub+0x100.
    volatile LONG* ranFlag = (volatile LONG*)((PUCHAR)stubAddr + 0x100);
    volatile LONG* doneFlag = (volatile LONG*)((PUCHAR)stubAddr + 0x104);
    *ranFlag = 0;
    *doneFlag = 0;

    // Build the APC stub: set ran flag, reserve shadow space, set args,
    // call entry (CRT init), set done flag, then call ZorMain which runs the
    // cheat forever on this thread (never returns).
    //   mov rcx, ranFlag | mov dword [rcx], 1
    //   sub rsp,0x28 | mov rcx, hModule | mov edx, DLL_PROCESS_ATTACH |
    //   xor r8d,r8d | mov rax, entryPoint | call rax
    //   mov rcx, doneFlag | mov dword [rcx], 1
    //   mov rax, ZorMain | call rax
    //   jmp $   (safety spin; normally unreachable)
    UCHAR stub[96] = { 0 };
    UCHAR* s = stub;
    *s++ = 0x48; *s++ = 0xB9;                                     // mov rcx, imm64 (ran flag)
    *(ULONG_PTR*)s = (ULONG_PTR)ranFlag; s += 8;
    *s++ = 0xC7; *s++ = 0x01; *s++ = 0x01; *s++ = 0x00; *s++ = 0x00; *s++ = 0x00; // mov dword [rcx], 1
    *s++ = 0x48; *s++ = 0x83; *s++ = 0xEC; *s++ = 0x28;          // sub rsp, 0x28
    *s++ = 0x48; *s++ = 0xB9;                                     // mov rcx, imm64
    *(ULONG_PTR*)s = (ULONG_PTR)pRemoteDll; s += 8;
    *s++ = 0xBA; *(ULONG*)s = DLL_PROCESS_ATTACH; s += 4;         // mov edx, 1
    *s++ = 0x45; *s++ = 0x33; *s++ = 0xC0;                        // xor r8d, r8d
    *s++ = 0x48; *s++ = 0xB8;                                     // mov rax, imm64
    *(ULONG_PTR*)s = (ULONG_PTR)entryPoint; s += 8;
    *s++ = 0xFF; *s++ = 0xD0;                                     // call rax
    *s++ = 0x48; *s++ = 0xB9;                                     // mov rcx, imm64 (done flag)
    *(ULONG_PTR*)s = (ULONG_PTR)doneFlag; s += 8;
    *s++ = 0xC7; *s++ = 0x01; *s++ = 0x01; *s++ = 0x00; *s++ = 0x00; *s++ = 0x00; // mov dword [rcx], 1
    *s++ = 0x48; *s++ = 0xB8;                                     // mov rax, imm64
    *(ULONG_PTR*)s = (ULONG_PTR)zorMain; s += 8;
    *s++ = 0xFF; *s++ = 0xD0;                                     // call rax (ZorMain; never returns)
    *s++ = 0xEB; *s++ = 0xFE;                                     // jmp $ (safety)
    SIZE_T stubLen = (SIZE_T)(s - stub);

    __try {
        RtlCopyMemory(stubAddr, stub, stubLen);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
        ObDereferenceObject(hThread);
        ZwFreeVirtualMemory(NtCurrentProcess(), &stubAddr, &stubSize, MEM_RELEASE);
        goto done;
    }

    // Deliver the stub via a SPECIAL USER APC. Unlike a normal user APC
    // (QueueUserAPC), a special user APC fires on the thread's next
    // kernel->user transition regardless of alertable state, so it runs even
    // on threads that are not in an alertable wait. NtQueueApcThreadEx is not
    // in ntoskrnl.lib; resolve it at runtime. In kernel mode a thread HANDLE
    // is the PETHREAD pointer directly (kernel-handle convention: high bit
    // set => direct object pointer).
    //   NTSTATUS NtQueueApcThreadEx(HANDLE ThreadHandle, ULONG UserApcOption,
    //       PVOID ApcRoutine, PVOID ApcArgument1, PVOID ApcArgument2,
    //       PVOID ApcArgument3);
    typedef NTSTATUS(*pfnQueueApcThreadEx)(
        HANDLE, ULONG, PVOID, PVOID, PVOID, PVOID);
#define USER_APC_OPTION_SPECIAL_USER_APC 0x0001
    UNICODE_STRING uQueueApc;
    RtlInitUnicodeString(&uQueueApc, L"NtQueueApcThreadEx");
    pfnQueueApcThreadEx pQueueApc = (pfnQueueApcThreadEx)MmGetSystemRoutineAddress(&uQueueApc);
    if (!pQueueApc) {
        status = STATUS_PROCEDURE_NOT_FOUND;
        ObDereferenceObject(hThread);
        ZwFreeVirtualMemory(NtCurrentProcess(), &stubAddr, &stubSize, MEM_RELEASE);
        goto done;
    }

    status = pQueueApc((HANDLE)hThread, USER_APC_OPTION_SPECIAL_USER_APC,
                       stubAddr, NULL, NULL, NULL);
    if (!NT_SUCCESS(status)) {
        ObDereferenceObject(hThread);
        ZwFreeVirtualMemory(NtCurrentProcess(), &stubAddr, &stubSize, MEM_RELEASE);
        goto done;
    }

    // Wait for the stub to signal (bounded so we never hang the game thread).
    status = STATUS_UNSUCCESSFUL;
    for (int i = 0; i < 200; i++) { // ~20s max (100ms sleeps)
        if (*doneFlag) { status = STATUS_SUCCESS; break; }
        LARGE_INTEGER delay;
        delay.QuadPart = -100 * 10000; // 100ms
        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    if (!NT_SUCCESS(status)) {
        // Distinguish "stub never ran" from "stub ran but entry crashed/hung".
        if (*ranFlag) status = STATUS_ACCESS_VIOLATION;  // entry threw/hung
        else status = STATUS_UNSUCCESSFUL;               // APC never fired

        // On failure the APC either never fired or the stub crashed; the APC is
        // still pending on the target thread, so we must NOT free the stub (a
        // late-firing APC would execute freed memory). Leak it; the loader
        // reboots/retries anyway. The game thread is untouched.
        ObDereferenceObject(hThread);
    } else {
        // Thread consumed by the cheat; don't touch it. Stub stays mapped.
        ObDereferenceObject(hThread);
        if (outBase) *outBase = pRemoteDll;
    }

done:
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);
    return status;
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

    case IOCTL_INJECT_ALLOC: {
        if (inputBufferLength < sizeof(INJECT_ALLOC_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        PINJECT_ALLOC_REQUEST req = (PINJECT_ALLOC_REQUEST)buffer;
        PVOID remoteBase = NULL;
        NTSTATUS allocStatus = InjectDLL_Allocate(req->ProcessId, req->SizeOfImage, &remoteBase);
        req->RemoteBase = remoteBase;
        req->ErrorStatus = allocStatus;
        bytes = sizeof(INJECT_ALLOC_REQUEST);
        // Always report success so the loader receives ErrorStatus back.
        status = STATUS_SUCCESS;
        break;
    }

    case IOCTL_INJECT_EXEC: {
        if (inputBufferLength < sizeof(INJECT_EXEC_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        PINJECT_EXEC_REQUEST req = (PINJECT_EXEC_REQUEST)buffer;
        NTSTATUS execStatus = InjectDLL_Exec(req->ProcessId, req->RemoteBase, &req->RemoteBase);
        req->ErrorStatus = execStatus;
        bytes = sizeof(INJECT_EXEC_REQUEST);
        // Always report success so the loader receives ErrorStatus back.
        status = STATUS_SUCCESS;
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
}

// Real driver init: the DRIVER_OBJECT handed here is a genuine kernel object
// (either from the I/O manager for SCM-loaded drivers, or from IoCreateDriver
// for manual-mapped drivers), so IoCreateDevice is safe to call.
static NTSTATUS RealDriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);

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

    // IoCreateDriver does not run the I/O manager's post-DriverEntry cleanup,
    // so the DO_DEVICE_INITIALIZING flag set by IoCreateDevice would otherwise
    // stay set forever and make the device unopenable from user mode.
    g_DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    status = IoCreateSymbolicLink(&g_SymLink, &g_DevName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(g_DeviceObject);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);

    // Manual-mapped (kdmapper) drivers are called with a NULL DriverObject.
    // IoCreateDevice needs a REAL DRIVER_OBJECT with a valid object header.
    // IoCreateDriver allocates a genuine one (via ObCreateObject) and then
    // invokes RealDriverEntry with it -- no fake pool object, no 0x18 BSOD.
    if (!driver) {
        return IoCreateDriver(NULL, RealDriverEntry);
    }

    // SCM-loaded drivers: use the I/O manager-provided object directly.
    return RealDriverEntry(driver, registry);
}
