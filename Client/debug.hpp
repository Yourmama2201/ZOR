// debug.hpp
#pragma once
#include <windows.h>
#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <mutex>

class DebugLogger {
private:
    std::string logFile;
    std::mutex mtx;
    bool enabled;

    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        struct tm tm_buf;
        localtime_s(&tm_buf, &time);
        char buf[64];
        sprintf_s(buf, "%02d:%02d:%02d.%03d",
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());
        return std::string(buf);
    }

public:
    DebugLogger(const std::string& file = "nxs_dbg.log")
        : logFile(file), enabled(true) {
        std::ofstream ofs(logFile, std::ios::trunc);
        ofs << "========================================\n";
        ofs << "DEBUG LOG\n";
        ofs << "Started: " << GetTimestamp() << "\n";
        ofs << "========================================\n\n";
        ofs.close();
        Log("[INIT] Debug logger initialized");
    }

    void Log(const std::string& message) {
        if (!enabled) return;
        std::lock_guard<std::mutex> lock(mtx);
        std::ofstream ofs(logFile, std::ios::app);
        if (ofs.is_open()) {
            ofs << GetTimestamp() << " | " << message << "\n";
            ofs.flush();
            ofs.close();
        }
    }

    void Log(const std::string& category, const std::string& message) {
        Log("[" + category + "] " + message);
    }

    void LogHex(const std::string& name, uintptr_t value) {
        char buf[128];
        sprintf_s(buf, "%s = 0x%llX", name.c_str(), (unsigned long long)value);
        Log(buf);
    }

    void LogInt(const std::string& name, int value) {
        Log(name + " = " + std::to_string(value));
    }

    void LogBool(const std::string& name, bool value) {
        Log(name + " = " + (value ? "TRUE" : "FALSE"));
    }

    void Section(const std::string& name) {
        Log("");
        Log("========== " + name + " ==========");
    }

    void Separator() {
        Log("----------------------------------------");
    }
};

// Global instance
static DebugLogger g_Debug;