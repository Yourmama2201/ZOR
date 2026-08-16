#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <windows.h>

class ConfigSystem {
private:
    std::string configPath;
    std::string profileDir;
    std::string currentProfile;
    std::unordered_map<std::string, std::string> values;

    static std::string ModuleDir() {
        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&ModuleDir, &mod);
        char path[MAX_PATH] = {};
        GetModuleFileNameA(mod, path, MAX_PATH);
        std::string p(path);
        size_t slash = p.find_last_of("\\/");
        if (slash != std::string::npos) p = p.substr(0, slash);
        return p;
    }

    std::string Trim(const std::string& s) {
        size_t st = 0, en = s.size();
        while (st < en && (s[st] == ' ' || s[st] == '\t' || s[st] == '\"' || s[st] == '\n' || s[st] == '\r')) st++;
        while (en > st && (s[en - 1] == ' ' || s[en - 1] == '\t' || s[en - 1] == '\"' || s[en - 1] == '\n' || s[en - 1] == '\r')) en--;
        return s.substr(st, en - st);
    }

public:
    ConfigSystem(const std::string& path = "")
        : configPath(path), profileDir("Configs/Profiles/"), currentProfile("default") {
        if (path.empty()) {
            configPath = ModuleDir() + "\\nxs_CONFIG.json";
        }
        profileDir = ModuleDir() + "\\Configs\\Profiles\\";
        std::filesystem::create_directories(profileDir);
    }

    std::string GetProfileDir() const { return profileDir; }
    std::string GetCurrentProfile() const { return currentProfile; }

    std::vector<std::string> ListProfiles() {
        std::vector<std::string> profiles;
        if (!std::filesystem::exists(profileDir)) return profiles;
        for (auto& e : std::filesystem::directory_iterator(profileDir)) {
            if (e.path().extension() == ".json") {
                profiles.push_back(e.path().stem().string());
            }
        }
        return profiles;
    }

    bool LoadProfile(const std::string& name) {
        currentProfile = name;
        std::string path = profileDir + name + ".json";
        if (!std::filesystem::exists(path)) return false;
        configPath = path;
        Load();
        return true;
    }

    bool SaveProfile(const std::string& name) {
        currentProfile = name;
        configPath = profileDir + name + ".json";
        Save();
        return true;
    }

    void DeleteProfile(const std::string& name) {
        std::string path = profileDir + name + ".json";
        if (std::filesystem::exists(path)) std::filesystem::remove(path);
        if (currentProfile == name) {
            currentProfile = "default";
            configPath = ModuleDir() + "\\nxs_CONFIG.json";
        }
    }

    void Save() {
        std::string json = "{\n";
        bool first = true;
        for (auto& [key, val] : values) {
            if (!first) json += ",\n";
            first = false;
            json += "  \"" + key + "\": \"" + val + "\"";
        }
        json += "\n}\n";

        std::ofstream file(configPath);
        if (file.is_open()) {
            file << json;
            file.close();
        }
    }

    void Load() {
        std::ifstream file(configPath);
        if (!file.is_open()) return;

        values.clear();
        std::string content((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
        file.close();

        // Simple JSON key-value parser
        size_t pos = 0;
        while ((pos = content.find('\"', pos)) != std::string::npos) {
            size_t keyStart = pos + 1;
            size_t keyEnd = content.find('\"', keyStart);
            if (keyEnd == std::string::npos) break;
            std::string key = content.substr(keyStart, keyEnd - keyStart);

            size_t colon = content.find(':', keyEnd);
            if (colon == std::string::npos) break;

            size_t valStart = content.find('\"', colon + 1);
            if (valStart == std::string::npos) break;
            valStart++;
            size_t valEnd = content.find('\"', valStart);
            if (valEnd == std::string::npos) break;
            std::string val = content.substr(valStart, valEnd - valStart);

            values[key] = val;
            pos = valEnd + 1;
        }
    }

    void Clear() { values.clear(); }

    void SetBool(const std::string& key, bool val) { values[key] = val ? "1" : "0"; }
    void SetInt(const std::string& key, int val) { values[key] = std::to_string(val); }
    void SetFloat(const std::string& key, float val) { values[key] = std::to_string(val); }
    void SetString(const std::string& key, const std::string& val) { values[key] = val; }

    bool GetBool(const std::string& key, bool def = false) {
        auto it = values.find(key);
        if (it == values.end()) return def;
        return it->second == "1";
    }
    int GetInt(const std::string& key, int def = 0) {
        auto it = values.find(key);
        if (it == values.end()) return def;
        try { return std::stoi(it->second); } catch (...) { return def; }
    }
    float GetFloat(const std::string& key, float def = 0.0f) {
        auto it = values.find(key);
        if (it == values.end()) return def;
        try { return std::stof(it->second); } catch (...) { return def; }
    }
    std::string GetString(const std::string& key, const std::string& def = "") {
        auto it = values.find(key);
        if (it == values.end()) return def;
        return it->second;
    }
};
