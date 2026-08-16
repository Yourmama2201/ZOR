#pragma once
#include <string>
#include <vector>
#include <random>
#include <imgui.h>
#include "../../memory.hpp"
#include "../../offsets.hpp"

class NameRotator {
private:
    MemoryManager* mem;
    uintptr_t gameBase;
    bool enabled;
    bool changeOnReport;
    int rotateInterval;
    float lastChange;
    int currentIndex;
    std::vector<std::string> presetNames;
    std::mt19937 rng;

    void GenerateRandomName(char* buf, size_t len) {
        static const char* parts[] = {
            "xx", "xX", "Xx", "No", "Re", "De", "Co", "Fa", "Ze", "Qu",
            "_", "-", "|", "//", "\\", "::", "..",
            "Pro", "Ultra", "Mega", "Super", "Alpha", "Beta", "Omega",
            "Fire", "Ice", "Dark", "Light", "Storm", "Shadow", "Ghost",
            "007", "042", "069", "123", "404", "666", "777", "999",
            "x", "zZ", "Zz", "qT", "Tq", "pO", "Oi", "Iu", "Uy", "Yt"
        };

        std::uniform_int_distribution<int> dist(0, sizeof(parts) / sizeof(parts[0]) - 1);
        std::string name;
        int count = 2 + (rng() % 3); // 2-4 parts

        for (int i = 0; i < count; i++) {
            name += parts[dist(rng)];
        }

        // Truncate to len
        if (name.size() >= len) name = name.substr(0, len - 1);
        strcpy_s(buf, len, name.c_str());
    }

public:
    NameRotator(MemoryManager* memory, uintptr_t base)
        : mem(memory), gameBase(base), enabled(false),
        changeOnReport(true), rotateInterval(300),
        lastChange(0), currentIndex(0) {
        std::random_device rd;
        rng.seed(rd());

        presetNames = {
            "nxs_USER", "Player", "Unknown", "Sniper", "Shadow",
            "Ghost", "Reaper", "Viper", "Frost", "Blaze",
            "Stealth", "Phantom", "Wraith", "Spectre", "Shade"
        };
    }

    void AddPreset(const std::string& name) {
        if (presetNames.size() < 50)
            presetNames.push_back(name);
    }

    void Update(char* nameBuffer, size_t bufSize, bool spectatorDetected) {
        if (!enabled) return;

        float now = ImGui::GetTime();

        bool shouldChange = false;

        // Interval-based change
        if (rotateInterval > 0 && (now - lastChange) >= rotateInterval) {
            shouldChange = true;
        }

        // Report-based change (when spectators detected)
        if (changeOnReport && spectatorDetected) {
            shouldChange = true;
        }

        if (shouldChange) {
            if (currentIndex < (int)presetNames.size()) {
                strcpy_s(nameBuffer, bufSize, presetNames[currentIndex].c_str());
                currentIndex = (currentIndex + 1) % presetNames.size();
            }
            else {
                GenerateRandomName(nameBuffer, bufSize);
            }
            lastChange = now;
        }
    }

    void SetEnabled(bool e) { enabled = e; }
    void SetRotateInterval(int s) { rotateInterval = s; }
    void SetChangeOnReport(bool c) { changeOnReport = c; }
    bool IsEnabled() const { return enabled; }
};
