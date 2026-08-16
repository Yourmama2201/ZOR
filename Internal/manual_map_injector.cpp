#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>

// IOCTL codes from driver
#define IOCTL_READ_MEMORY CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
    HANDLE ProcessId;
    ULONG_PTR Address;
    SIZE_T Size;
} MEMORY_REQUEST;

HANDLE FindProcess(const char* processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return nullptr;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
        CloseHandle(hSnapshot);
        return nullptr;
    }

    do {
        if (_stricmp(pe32.szExeFile, processName) == 0) {
            CloseHandle(hSnapshot);
            return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
        }
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return nullptr;
}

bool ManualMapInject(HANDLE hProcess, const char* dllPath) {
    // Read DLL file
    FILE* f = fopen(dllPath, "rb");
    if (!f) {
        std::cout << "[-] Failed to open DLL file" << std::endl;
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    std::vector<uint8_t> dllData(fileSize);
    fread(dllData.data(), 1, fileSize, f);
    fclose(f);
    
    // This is a simplified manual map - in production you'd need full PE parsing
    // For now, let's try a simpler approach using the driver
    
    // Allocate memory in target process
    LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteMem) {
        std::cout << "[-] VirtualAllocEx failed" << std::endl;
        return false;
    }
    
    // Write DLL to target process
    SIZE_T written = 0;
    if (!WriteProcessMemory(hProcess, remoteMem, dllData.data(), fileSize, &written)) {
        std::cout << "[-] WriteProcessMemory failed" << std::endl;
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    // Create remote thread to execute DLL
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, 
        (LPTHREAD_START_ROUTINE)remoteMem, nullptr, 0, nullptr);
    
    if (!hThread) {
        std::cout << "[-] CreateRemoteThread failed" << std::endl;
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    WaitForSingleObject(hThread, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    
    std::cout << "[+] Thread exit code: " << exitCode << std::endl;
    return exitCode != 0;
}

int main(int argc, char* argv[]) {
    std::cout << "=== Manual Map Injector ===" << std::endl;
    
    if (argc < 3) {
        std::cout << "Usage: manual_map_injector.exe <process_name> <dll_path>" << std::endl;
        system("pause");
        return 1;
    }
    
    const char* processName = argv[1];
    const char* dllPath = argv[2];
    
    std::cout << "[+] Finding process: " << processName << std::endl;
    HANDLE hProcess = FindProcess(processName);
    if (!hProcess) {
        std::cout << "[-] Process not found" << std::endl;
        system("pause");
        return 1;
    }
    
    std::cout << "[+] Process found" << std::endl;
    std::cout << "[+] Manual mapping DLL: " << dllPath << std::endl;
    
    if (ManualMapInject(hProcess, dllPath)) {
        std::cout << "[+] DLL manual mapped successfully!" << std::endl;
    } else {
        std::cout << "[-] DLL manual mapping failed" << std::endl;
    }
    
    CloseHandle(hProcess);
    system("pause");
    return 0;
}
