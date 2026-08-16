#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <ctime>

class DiscordRPC {
private:
    HANDLE pipe = INVALID_HANDLE_VALUE;
    bool connected = false;
    DWORD lastUpdate = 0;

    static std::string EscapeJson(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    }

    bool Connect() {
        for (int i = 0; i < 10; i++) {
            std::string name = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
            HANDLE h = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                pipe = h;
                return true;
            }
        }
        return false;
    }

    bool SendFrame(DWORD opcode, const std::string& payload) {
        if (pipe == INVALID_HANDLE_VALUE) return false;
        DWORD len = (DWORD)payload.size();
        BYTE hdr[8];
        memcpy(hdr, &opcode, 4);
        memcpy(hdr + 4, &len, 4);

        DWORD written = 0;
        if (!WriteFile(pipe, hdr, 8, &written, NULL)) return false;
        if (!WriteFile(pipe, payload.c_str(), len, &written, NULL)) return false;
        return true;
    }

    bool Handshake() {
        std::string json = "{\"v\":1,\"client_id\":\"1494304092846166016\"}";
        if (!SendFrame(0, json)) return false;

        // read READY frame
        BYTE hdr[8];
        DWORD got = 0;
        while (got < 8) {
            DWORD r = 0;
            if (!ReadFile(pipe, hdr + got, 8 - got, &r, NULL) || r == 0) return false;
            got += r;
        }
        DWORD len;
        memcpy(&len, hdr + 4, 4);
        if (len == 0 || len > 65536) return false;
        std::vector<BYTE> body(len);
        got = 0;
        while (got < len) {
            DWORD r = 0;
            if (!ReadFile(pipe, body.data() + got, len - got, &r, NULL) || r == 0) return false;
            got += r;
        }
        return true;
    }

public:
    DiscordRPC() {}

    void SetStatus(const std::string& details, const std::string& state, const std::string& largeText) {
        if (!connected) {
            if (!Connect() || !Handshake()) return;
            connected = true;
            lastUpdate = 0;
        }

        time_t now = time(NULL);
        std::string json =
            "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(GetCurrentProcessId()) +
            ",\"activity\":{\"details\":\"" + EscapeJson(details) + "\",\"state\":\"" + EscapeJson(state) +
            "\",\"timestamps\":{\"start\":" + std::to_string((long long)now) + "},\"assets\":{\"large_text\":\"" +
            EscapeJson(largeText) + "\"},\"instance\":true}},\"nonce\":\"" +
            std::to_string((long long)now) + std::to_string(GetTickCount64()) + "\"}";

        SendFrame(1, json);
    }

    void Update() {
        DWORD now = GetTickCount();
        if (connected && now - lastUpdate > 15000) {
            lastUpdate = now;
            SetStatus("zor - the best cheat known :)", "dominating the lobby", "zor");
        }
    }

    void Shutdown() {
        if (pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
        }
        connected = false;
    }
};
