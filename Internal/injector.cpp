#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

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

bool InjectDLL(HANDLE hProcess, const char* dllPath) {
    // Convert to absolute path
    char fullPath[MAX_PATH];
    GetFullPathNameA(dllPath, MAX_PATH, fullPath, nullptr);
    
    SIZE_T pathLen = strlen(fullPath) + 1;
    
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT, PAGE_READWRITE);
    if (!pRemoteMem) {
        std::cout << "[-] VirtualAllocEx failed" << std::endl;
        return false;
    }
    
    if (!WriteProcessMemory(hProcess, pRemoteMem, fullPath, pathLen, nullptr)) {
        std::cout << "[-] WriteProcessMemory failed" << std::endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");
    
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, 
        (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemoteMem, 0, nullptr);
    
    if (!hThread) {
        std::cout << "[-] CreateRemoteThread failed" << std::endl;
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }
    
    WaitForSingleObject(hThread, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    
    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
    CloseHandle(hThread);
    
    std::cout << "[+] LoadLibrary returned: 0x" << std::hex << exitCode << std::endl;
    return exitCode != 0;
}

int main(int argc, char* argv[]) {
    std::cout << "=== CS 1.6 DLL Injector ===" << std::endl;
    
    if (argc < 3) {
        std::cout << "Usage: injector.exe <process_name> <dll_path>" << std::endl;
        std::cout << "Example: injector.exe cstrike.exe cs16_cheat.dll" << std::endl;
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
    std::cout << "[+] Injecting DLL: " << dllPath << std::endl;
    
    if (InjectDLL(hProcess, dllPath)) {
        std::cout << "[+] DLL injected successfully!" << std::endl;
    } else {
        std::cout << "[-] DLL injection failed" << std::endl;
        CloseHandle(hProcess);
        system("pause");
        return 1;
    }
    
    CloseHandle(hProcess);
    std::cout << "[+] Done. Press any key to exit..." << std::endl;
    system("pause");
    return 0;
}
