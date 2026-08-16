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

NTSTATUS LdrLoadDll(
    PWSTR PathToFile,
    ULONG Flags,
    PUNICODE_STRING ModuleFileName,
    PHANDLE ModuleHandle
);

NTSTATUS LdrGetProcedureAddress(
    HANDLE ModuleHandle,
    PUNICODE_STRING FunctionName,
    USHORT Oid,
    PVOID* FunctionAddress
);

NTSTATUS ZwQueryInformationProcess(
    HANDLE ProcessHandle,
    ULONG ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
);

#define DEVICE_NAME_BLOCK SO_BLOCK("\\Device\\nxs")
#define SYMLINK_NAME_BLOCK SO_BLOCK("\\DosDevices\\nxs")

#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0

PDEVICE_OBJECT g_DeviceObject = NULL;
UNICODE_STRING g_DevName, g_SymLink;

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

NTSTATUS ResolveImportsRemotely(PVOID pRemoteDll, PIMAGE_NT_HEADERS nt) {
    DbgPrint("[+] Resolving imports remotely...\n");

    DWORD importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRVA) {
        DbgPrint("[+] No imports to resolve\n");
        return STATUS_SUCCESS;
    }

    PIMAGE_IMPORT_DESCRIPTOR imports = (PIMAGE_IMPORT_DESCRIPTOR)((PUCHAR)pRemoteDll + importRVA);
    int resolvedCount = 0;

    for (; imports->Name; imports++) {
        char* moduleName = (char*)((PUCHAR)pRemoteDll + imports->Name);
        DbgPrint("[+] Resolving imports from: %s\n", moduleName);

        ANSI_STRING ansiModName;
        UNICODE_STRING uniModName;
        RtlInitAnsiString(&ansiModName, moduleName);
        NTSTATUS status = RtlAnsiStringToUnicodeString(&uniModName, &ansiModName, TRUE);
        if (!NT_SUCCESS(status)) {
            DbgPrint("[+] Failed to convert module name: 0x%X\n", status);
            continue;
        }

        HANDLE hModule = NULL;
        status = LdrLoadDll(NULL, 0, &uniModName, &hModule);
        RtlFreeUnicodeString(&uniModName);

        if (!NT_SUCCESS(status) || !hModule) {
            DbgPrint("[+] LdrLoadDll failed for %s: 0x%X\n", moduleName, status);
            return STATUS_UNSUCCESSFUL;
        }
        DbgPrint("[+] Loaded %s at 0x%p\n", moduleName, hModule);

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
                LdrGetProcedureAddress(hModule, &uniFunc, 0, &func);
                RtlFreeUnicodeString(&uniFunc);
            }
            else {
                PIMAGE_IMPORT_BY_NAME byName = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)pRemoteDll + thunk->u1.AddressOfData);
                ANSI_STRING ansiFunc;
                RtlInitAnsiString(&ansiFunc, byName->Name);

                UNICODE_STRING uniFunc;
                RtlAnsiStringToUnicodeString(&uniFunc, &ansiFunc, TRUE);
                LdrGetProcedureAddress(hModule, &uniFunc, 0, &func);
                RtlFreeUnicodeString(&uniFunc);
            }

            if (!func) {
                DbgPrint("[+] Failed to resolve import in %s\n", moduleName);
                return STATUS_UNSUCCESSFUL;
            }

            iat->u1.Function = (ULONGLONG)func;
            resolvedCount++;
        }
    }

    DbgPrint("[+] Resolved %d imports successfully\n", resolvedCount);
    return STATUS_SUCCESS;
}

NTSTATUS InjectDLL_APC(HANDLE pid, PUCHAR dllData, SIZE_T dllSize, PVOID* outBase) {
    DbgPrint("[+] === APC INJECTION START ===\n");
    DbgPrint("[+] PID: %d, DLL Size: %zu\n", (int)pid, dllSize);

    if (KeGetCurrentIrql() > PASSIVE_LEVEL) {
        DbgPrint("[+] IRQL too high: %d\n", KeGetCurrentIrql());
        return STATUS_UNSUCCESSFUL;
    }

    if (!dllData || !dllSize) {
        DbgPrint("[+] Invalid DLL data\n");
        return STATUS_INVALID_PARAMETER;
    }

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)dllData;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        DbgPrint("[+] Invalid DOS signature: 0x%X\n", dos->e_magic);
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((PUCHAR)dllData + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        DbgPrint("[+] Invalid NT signature: 0x%X\n", nt->Signature);
        return STATUS_INVALID_IMAGE_FORMAT;
    }
    DbgPrint("[+] Image size: 0x%X\n", nt->OptionalHeader.SizeOfImage);

    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[+] PsLookupProcessByProcessId failed: 0x%X\n", status);
        return status;
    }
    DbgPrint("[+] Process found: 0x%p\n", process);

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
        DbgPrint("[+] ZwAllocateVirtualMemory failed: 0x%X\n", status);
        KeUnstackDetachProcess(&apcState);
        ObDereferenceObject(process);
        return status;
    }
    DbgPrint("[+] Allocated at: 0x%p (size: 0x%zX)\n", pRemoteDll, regionSize);

    RtlCopyMemory(pRemoteDll, dllData, nt->OptionalHeader.SizeOfHeaders);
    DbgPrint("[+] Headers copied\n");

    PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        PVOID dest = (PUCHAR)pRemoteDll + sections[i].VirtualAddress;
        PVOID src = (PUCHAR)dllData + sections[i].PointerToRawData;
        SIZE_T size = sections[i].SizeOfRawData;
        if (size > 0) {
            RtlCopyMemory(dest, src, size);
        }
    }
    DbgPrint("[+] Sections copied\n");

    PIMAGE_DATA_DIRECTORY relocDir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir->Size > 0) {
        PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)((PUCHAR)pRemoteDll + relocDir->VirtualAddress);
        PUCHAR relocEnd = (PUCHAR)reloc + relocDir->Size;
        SIZE_T delta = (SIZE_T)pRemoteDll - nt->OptionalHeader.ImageBase;

        ULONG relocCount = 0;
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
                    relocCount++;
                }
            }
            reloc = (PIMAGE_BASE_RELOCATION)((PUCHAR)reloc + reloc->SizeOfBlock);
        }
        DbgPrint("[+] Relocations processed: %d\n", relocCount);
    }

    status = ResolveImportsRemotely(pRemoteDll, nt);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[+] Import resolution failed: 0x%X\n", status);
        ZwFreeVirtualMemory(NtCurrentProcess(), &pRemoteDll, &regionSize, MEM_RELEASE);
        KeUnstackDetachProcess(&apcState);
        ObDereferenceObject(process);
        return status;
    }
    DbgPrint("[+] Imports resolved, calling DllMain...\n");

    typedef NTSTATUS(*DLL_ENTRY)(PVOID, ULONG, PVOID);
    DLL_ENTRY entryPoint = (DLL_ENTRY)((PUCHAR)pRemoteDll + nt->OptionalHeader.AddressOfEntryPoint);
    DbgPrint("[+] Entry: 0x%p\n", entryPoint);

    __try {
        NTSTATUS dllResult = entryPoint(pRemoteDll, DLL_PROCESS_ATTACH, NULL);
        DbgPrint("[+] DllMain returned: 0x%X\n", dllResult);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        DbgPrint("[+] DllMain crashed: 0x%X\n", GetExceptionCode());
        KeUnstackDetachProcess(&apcState);
        ObDereferenceObject(process);
        return STATUS_UNSUCCESSFUL;
    }

    if (outBase) *outBase = pRemoteDll;

    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(process);

    DbgPrint("[+] === APC INJECTION COMPLETE ===\n");
    return STATUS_SUCCESS;
}

NTSTATUS ReadMemory(HANDLE pid, ULONG_PTR address, PVOID buffer, SIZE_T size) {
    PEPROCESS process = NULL;
    NTSTATUS status = PsLookupProcessByProcessId(pid, &process);
    if (!NT_SUCCESS(status)) return status;
    if (size == 0 || size > 0x400000 || address < 0x10000 ||
        address >= 0x0000800000000000ULL || address + size > 0x0000800000000000ULL ||
        address + size < address) {
        ObDereferenceObject(process);
        return STATUS_INVALID_PARAMETER;
    }

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
    if (size == 0 || size > 0x400000 || address < 0x10000 ||
        address >= 0x0000800000000000ULL || address + size > 0x0000800000000000ULL ||
        address + size < address) {
        ObDereferenceObject(process);
        return STATUS_INVALID_PARAMETER;
    }

    KAPC_STATE apcState;
    KeStackAttachProcess(process, &apcState);

    __try {
        RtlCopyMemory((PVOID)address, buffer, size);
        status = STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_ACCESS_VIOLATION;
    }

    KeUnstackDetachProcess(&apcState);
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
        DbgPrint("[+] IOCTL_INJECT_DLL\n");

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
            DbgPrint("[+] Injection result: 0x%X, Base: 0x%p\n", status, remoteBase);
        }
        else {
            status = STATUS_INVALID_PARAMETER;
        }
        break;
    }

    case IOCTL_READ_MEMORY: {
        PMEMORY_REQUEST req = (PMEMORY_REQUEST)buffer;
        if (inputBufferLength < sizeof(MEMORY_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (req->Size == 0 || req->Size > 0x400000 ||
            (ULONG_PTR)req->Buffer < 0x10000 ||
            (ULONG_PTR)req->Buffer >= 0x0000800000000000ULL) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = ReadMemory(req->ProcessId, req->Address, req->Buffer, req->Size);
        bytes = sizeof(MEMORY_REQUEST);
        break;
    }

    case IOCTL_WRITE_MEMORY: {
        PMEMORY_REQUEST req = (PMEMORY_REQUEST)buffer;
        if (inputBufferLength < sizeof(MEMORY_REQUEST)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (req->Size == 0 || req->Size > 0x400000 ||
            (ULONG_PTR)req->Buffer < 0x10000 ||
            (ULONG_PTR)req->Buffer >= 0x0000800000000000ULL) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        status = WriteMemory(req->ProcessId, req->Address, req->Buffer, req->Size);
        bytes = sizeof(MEMORY_REQUEST);
        break;
    }

    case IOCTL_GET_PID: {
        DbgPrint("[+] IOCTL_GET_PID\n");
        // User-mode sends PROCESS_ID_REQUEST with PID prompt, driver returns its PID or target PID
        // For now, return the caller's PID via echo in buffer
        if (buffer && inputBufferLength >= sizeof(HANDLE)) {
            HANDLE callerPid = PsGetCurrentProcessId();
            memcpy(buffer, &callerPid, sizeof(HANDLE));
            bytes = sizeof(HANDLE);
            status = STATUS_SUCCESS;
        }
        break;
    }

    case IOCTL_GET_BASE: {
        DbgPrint("[+] IOCTL_GET_BASE\n");
        // User-mode sends a PID, driver returns module base of that process
        // Structure: [HANDLE pid] -> [ULONG64 base]
        if (buffer && inputBufferLength >= sizeof(HANDLE)) {
            HANDLE targetPid = *(HANDLE*)buffer;
            PEPROCESS targetProcess = NULL;
            NTSTATUS st = PsLookupProcessByProcessId(targetPid, &targetProcess);
            if (NT_SUCCESS(st) && targetProcess) {
                // Read PEB->ImageBaseAddress
                PROCESS_BASIC_INFORMATION pbi;
                ULONG retLen = 0;
                st = ZwQueryInformationProcess(targetProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &retLen);
                if (NT_SUCCESS(st) && pbi.PebBaseAddress) {
                    ULONG_PTR imageBase = 0;
                    SIZE_T done = 0;
                    st = MmCopyVirtualMemory(targetProcess, (PVOID)((PUCHAR)pbi.PebBaseAddress + 0x10),
                        PsGetCurrentProcess(), &imageBase, sizeof(imageBase), KernelMode, &done);
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
        DbgPrint("[+] IOCTL_FIND_PID\n");
        if (!buffer || inputBufferLength < 2) { status = STATUS_BUFFER_TOO_SMALL; break; }
        CHAR targetName[64] = {0};
        ULONG copyLen = min(inputBufferLength, 63);
        memcpy(targetName, buffer, copyLen);
        targetName[copyLen] = '\0';

        // Convert target to UNICODE once
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
    DbgPrint("[+] Driver unloading...\n");
    if (g_DeviceObject) {
        IoDeleteDevice(g_DeviceObject);
        g_DeviceObject = NULL;
    }
    IoDeleteSymbolicLink(&g_SymLink);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    UNREFERENCED_PARAMETER(registry);

    DbgPrint("[+] === DRIVER ENTRY ===\n");

    driver->DriverUnload = DriverUnload;

    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        driver->MajorFunction[i] = DispatchCreateClose;
    }
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    driver->MajorFunction[IRP_MJ_CREATE] = DispatchCreateClose;
    driver->MajorFunction[IRP_MJ_CLOSE] = DispatchCreateClose;

    // Decode the device/symlink names at runtime (never in plaintext in the image).
    WCHAR devName[32], symName[32];
    char devA[32], symA[32];
    SO_DEC(devA, DEVICE_NAME_BLOCK);
    SO_DEC(symA, SYMLINK_NAME_BLOCK);
    for (int i = 0; i < 32; i++) { devName[i] = (WCHAR)devA[i]; symName[i] = (WCHAR)symA[i]; }

    RtlInitUnicodeString(&g_DevName, devName);
    RtlInitUnicodeString(&g_SymLink, symName);

    NTSTATUS status = IoCreateDevice(driver, 0, &g_DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_DeviceObject);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[+] IoCreateDevice failed: 0x%X\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&g_SymLink, &g_DevName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[+] IoCreateSymbolicLink failed: 0x%X\n", status);
        IoDeleteDevice(g_DeviceObject);
        return status;
    }

    DbgPrint("[+] === DRIVER READY ===\n");

    return STATUS_SUCCESS;
}
