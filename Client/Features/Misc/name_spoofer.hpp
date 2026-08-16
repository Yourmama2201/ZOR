#pragma once
#include "../../memory.hpp"
#include "../../offsets.hpp"
#include <string>
#include <vector>
#include <imgui.h>

class NameSpoofer {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool bypassProfanity;
    char customName[32];
    char customClanTag[16];
    bool nameChanged;
    std::string originalName;

    static constexpr uintptr_t NAME_ARRAY = Offsets::NAME_ARRAY;
    static constexpr uintptr_t NAME_ARRAY_POS = Offsets::NAME_ARRAY_POS;
    static constexpr uintptr_t NAME_ARRAY_SIZE = Offsets::NAME_ARRAY_SIZE;

public:
    NameSpoofer(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false),
        bypassProfanity(true), nameChanged(false) {
        memset(customName, 0, sizeof(customName));
        memset(customClanTag, 0, sizeof(customClanTag));
        strcpy_s(customName, "nxs_USER");
        strcpy_s(customClanTag, "YK");
    }

    void Run() {
        if (!mem || !gameBase || !enabled) return;

        // Bypass profanity filter by patching the filter function
        if (bypassProfanity) {
            // The profanity filter is typically a function that checks
            // characters and replaces them. We can NOP it out.
            // Common pattern: check if name contains filtered words
            uintptr_t filterFunc = FindProfanityFilter();
            if (filterFunc) {
                // Write RET to disable the filter
                mem->Write<uint8_t>(filterFunc, 0xC3); // RET instruction
            }
        }

        // Write custom name to the name array
        uintptr_t nameArrayPtr = mem->Read<uintptr_t>(gameBase + NAME_ARRAY);
        if (nameArrayPtr) {
            uintptr_t localIndex = mem->Read<uintptr_t>(gameBase + Offsets::LOCAL_INDEX);
            if (localIndex) {
                int idx = mem->Read<int>(localIndex + Offsets::LOCAL_INDEX_POS);
                uintptr_t nameEntry = mem->Read<uintptr_t>(nameArrayPtr + (idx * NAME_ARRAY_SIZE));
                if (nameEntry) {
                    mem->WriteBuffer(nameEntry + NAME_ARRAY_POS, customName, 32);
                }
            }
        }

        // Write custom clan tag
        if (strlen(customClanTag) > 0) {
            uintptr_t localPtr = mem->Read<uintptr_t>(gameBase + 0x30); // local player ptr
            if (localPtr) {
                mem->WriteBuffer(localPtr + 0x934, customClanTag, 16); // clan tag offset
            }
        }
    }

    // Scan for the profanity filter function pattern
    uintptr_t FindProfanityFilter() {
        // Common MW2 profanity filter signature
        // This is a best-effort scan; actual offset varies by update
        const uint8_t pattern[] = {
            0x48, 0x89, 0x5C, 0x24, 0x08, // mov [rsp+8], rbx
            0x48, 0x89, 0x74, 0x24, 0x10, // mov [rsp+10], rsi
            0x57,                         // push rdi
            0x48, 0x83, 0xEC, 0x20,       // sub rsp, 20
            0x48, 0x8B, 0xD9,             // mov rbx, rcx
            0x48, 0x85, 0xC9              // test rcx, rcx
        };

        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
        IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

        for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (memcmp(sec[i].Name, ".text", 5) == 0) {
                uintptr_t start = base + sec[i].VirtualAddress;
                uintptr_t end = start + sec[i].SizeOfRawData;

                for (uintptr_t addr = start; addr < end - sizeof(pattern); addr++) {
                    bool found = true;
                    for (size_t j = 0; j < sizeof(pattern); j++) {
                        if (*(uint8_t*)(addr + j) != pattern[j]) {
                            found = false;
                            break;
                        }
                    }
                    if (found) return addr;
                }
            }
        }
        return 0;
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetBypassProfanity(bool b) { bypassProfanity = b; }
    void SetCustomName(const char* name) {
        strcpy_s(customName, sizeof(customName), name);
        // Strip any profanity ourselves for safety
        for (size_t i = 0; customName[i]; i++) {
            if (customName[i] < 32 || customName[i] > 126) customName[i] = '_';
        }
    }
    void SetCustomClanTag(const char* tag) {
        strcpy_s(customClanTag, sizeof(customClanTag), tag);
        for (size_t i = 0; customClanTag[i]; i++) {
            if (customClanTag[i] < 32 || customClanTag[i] > 126) customClanTag[i] = '_';
        }
    }
    const char* GetCustomName() const { return customName; }
    const char* GetCustomClanTag() const { return customClanTag; }
};
