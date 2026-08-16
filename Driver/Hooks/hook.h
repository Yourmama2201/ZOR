// ============================================
// nxs Driver Hooks - Header
// ============================================

#pragma once

#include <ntddk.h>

// Hook initialization and cleanup
NTSTATUS InitializeHooks();
VOID CleanupHooks();

// Hook function declarations
NTSTATUS HookNtReadVirtualMemory(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    ULONG NumberOfBytesToRead,
    PULONG NumberOfBytesReaded
);

NTSTATUS HookNtWriteVirtualMemory(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    ULONG NumberOfBytesToWrite,
    PULONG NumberOfBytesWritten
);