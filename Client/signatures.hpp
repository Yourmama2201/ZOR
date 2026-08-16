#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <array>
#include <random>
#include <algorithm>
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

struct Signature {
    std::string name;
    std::vector<uint8_t> pattern;
    std::vector<uint8_t> mask;
    uintptr_t offset;
    int extra;
    bool relative;
    bool resolved;

    Signature() : offset(0), extra(0), relative(false), resolved(false) {}

    Signature(const std::string& n, const std::string& hexPattern, int ext = 0, bool rel = false)
        : name(n), offset(0), extra(ext), relative(rel), resolved(false) {
        // Parse hex pattern like "48 8B 05 ? ? ? ? 48 85 C0 74 ?"
        std::string h = hexPattern;
        size_t pos = 0;
        while (pos < h.size()) {
            while (pos < h.size() && h[pos] == ' ') pos++;
            if (pos >= h.size()) break;
            if (h[pos] == '?') {
                pattern.push_back(0);
                mask.push_back(0);
                pos++;
            }
            else {
                uint8_t byte = 0;
                for (int i = 0; i < 2; i++) {
                    if (pos < h.size()) {
                        char c = h[pos++];
                        byte <<= 4;
                        if (c >= '0' && c <= '9') byte |= (c - '0');
                        else if (c >= 'A' && c <= 'F') byte |= (c - 'A' + 10);
                        else if (c >= 'a' && c <= 'f') byte |= (c - 'a' + 10);
                    }
                }
                pattern.push_back(byte);
                mask.push_back(1);
            }
        }
    }

    bool Match(const uint8_t* data) const {
        for (size_t i = 0; i < pattern.size(); i++) {
            if (mask[i] && data[i] != pattern[i])
                return false;
        }
        return true;
    }
};

class SigScanner {
private:
    uintptr_t moduleBase;
    size_t moduleSize;
    std::vector<Signature> signatures;
    std::mt19937_64 rng;
    bool initialized;

    bool GetModuleInfo() {
        moduleBase = (uintptr_t)GetModuleHandleA(NULL);
        if (!moduleBase) return false;

        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)moduleBase;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

        IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(moduleBase + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        moduleSize = nt->OptionalHeader.SizeOfImage;
        return true;
    }

    void ScrambleScanOrder() {
        // Randomize scan order to avoid pattern detection
        std::vector<size_t> indices(signatures.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng);

        std::vector<Signature> scrambled;
        for (size_t idx : indices)
            scrambled.push_back(signatures[idx]);
        signatures = scrambled;
    }

    void EvasiveDelay() {
        // Random micro-delays to avoid timing analysis
        std::uniform_int_distribution<int> dist(1, 50);
        volatile int junk = 0;
        for (int i = 0; i < dist(rng); i++) junk += i;
    }

public:
    SigScanner() : moduleBase(0), moduleSize(0), initialized(false) {
        std::random_device rd;
        rng.seed(rd());
    }

    void AddSignature(const Signature& sig) {
        signatures.push_back(sig);
    }

    void AddSignatures(const std::vector<Signature>& sigs) {
        signatures.insert(signatures.end(), sigs.begin(), sigs.end());
    }

    bool Initialize() {
        if (initialized) return true;
        if (!GetModuleInfo()) return false;

        // Scramble the order we scan signatures to obfuscate intent
        ScrambleScanOrder();
        initialized = true;
        return true;
    }

    bool ScanAll() {
        if (!initialized && !Initialize()) return false;
        if (!moduleBase || !moduleSize) return false;

        // Scan .text section specifically (faster, more targeted)
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)moduleBase;
        IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(moduleBase + dos->e_lfanew);
        IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);

        uintptr_t textStart = 0, textEnd = 0;
        for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            char secName[9] = {};
            memcpy(secName, sections[i].Name, 8);
            if (strcmp(secName, ".text") == 0) {
                textStart = moduleBase + sections[i].VirtualAddress;
                textEnd = textStart + sections[i].SizeOfRawData;
                break;
            }
            // Also check .rdata for some constant-based patterns
            if (strcmp(secName, ".rdata") == 0) {
                // Keep as fallback
            }
        }

        if (!textStart) {
            textStart = moduleBase;
            textEnd = moduleBase + moduleSize;
        }

        // Resolve each sig
        for (auto& sig : signatures) {
            if (sig.resolved) continue;

            // Evasion: randomize scan direction per sig
            bool forward = rng() % 2;
            uintptr_t scanStart = forward ? textStart : textEnd;
            uintptr_t scanEnd = forward ? textEnd : textStart;
            intptr_t step = forward ? 1 : -1;

            for (uintptr_t addr = scanStart;
                forward ? (addr < scanEnd) : (addr > scanEnd);
                addr += step) {

                if (sig.Match((uint8_t*)addr)) {
                    uintptr_t result = addr;
                    if (sig.relative && sig.extra > 0) {
                        // RIP-relative: read 4-byte offset at addr + extra
                        int32_t relOffset = *(int32_t*)(addr + sig.extra);
                        result = addr + sig.extra + 4 + relOffset;
                    }
                    sig.offset = result;
                    sig.resolved = true;

                    // Random delay between sigs to evade timing heuristics
                    EvasiveDelay();
                    break;
                }
            }
        }

        return true;
    }

    bool Resolve(const std::string& name, uintptr_t& outOffset) {
        for (auto& sig : signatures) {
            if (sig.name == name && sig.resolved) {
                outOffset = sig.offset;
                return true;
            }
        }
        return false;
    }

    uintptr_t Get(const std::string& name, uintptr_t fallback = 0) {
        for (auto& sig : signatures) {
            if (sig.name == name && sig.resolved)
                return sig.offset;
        }
        return fallback;
    }

    size_t ResolvedCount() const {
        size_t count = 0;
        for (auto& sig : signatures)
            if (sig.resolved) count++;
        return count;
    }

    size_t TotalCount() const { return signatures.size(); }

    std::vector<std::pair<std::string, uintptr_t>> GetResults() const {
        std::vector<std::pair<std::string, uintptr_t>> results;
        for (auto& sig : signatures) {
            if (sig.resolved)
                results.emplace_back(sig.name, sig.offset);
        }
        return results;
    }
};
