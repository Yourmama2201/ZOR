#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <intrin.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "iphlpapi.lib")

class AuthSystem {
private:
    std::string serverHost;
    int serverPort;
    bool authenticated;
    bool authAttempted;
    std::string authToken;
    std::string ownerName;
    std::string errorMessage;
    char keyInput[128];
    bool loginOpen;
    bool loading;
    float animProgress;

    std::string GetHWID() {
        std::string hwid;

        // Volume Serial
        DWORD serial = 0;
        if (GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) {
            hwid += std::to_string(serial);
        }
        hwid += "-";

        // CPU ID
        int cpuInfo[4] = { -1 };
        __cpuid(cpuInfo, 0);
        char cpuId[32] = {};
        sprintf_s(cpuId, "%08X%08X", cpuInfo[0], cpuInfo[3]);
        hwid += cpuId;
        hwid += "-";

        // MAC address (first adapter)
        IP_ADAPTER_INFO adapterInfo[16];
        DWORD bufLen = sizeof(adapterInfo);
        if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_SUCCESS) {
            for (PIP_ADAPTER_INFO p = adapterInfo; p; p = p->Next) {
                if (p->AddressLength >= 6 && p->Type != MIB_IF_TYPE_LOOPBACK) {
                    char mac[18];
                    sprintf_s(mac, "%02X%02X%02X%02X%02X%02X",
                        p->Address[0], p->Address[1], p->Address[2],
                        p->Address[3], p->Address[4], p->Address[5]);
                    hwid += mac;
                    break;
                }
            }
        }
        hwid += "-";

        // Username
        char user[256];
        DWORD userLen = sizeof(user);
        if (GetUserNameA(user, &userLen)) {
            hwid += user;
        }

        return hwid;
    }

    std::string HttpPost(const std::string& endpoint, const std::string& jsonBody) {
        std::string result;
        HINTERNET hSession = WinHttpOpen(L"Mozlient/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) return "";

        std::wstring whost(serverHost.begin(), serverHost.end());
        HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), serverPort, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                std::wstring(endpoint.begin(), endpoint.end()).c_str(),
                NULL, NULL, NULL, 0);
            if (hRequest) {
                std::wstring headers = L"Content-Type: application/json\r\n";
                WinHttpSendRequest(hRequest, headers.c_str(), headers.length(),
                    (LPVOID)jsonBody.c_str(), jsonBody.length(), jsonBody.length(), 0);
                if (WinHttpReceiveResponse(hRequest, NULL)) {
                    DWORD size = 0;
                    WinHttpQueryDataAvailable(hRequest, &size);
                    if (size > 0) {
                        std::vector<char> buf(size + 1);
                        DWORD read = 0;
                        WinHttpReadData(hRequest, buf.data(), size, &read);
                        buf[read] = 0;
                        result = buf.data();
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
        return result;
    }

public:
    AuthSystem(const std::string& host = "127.0.0.1", int port = 5732)
        : serverHost(host), serverPort(port), authenticated(false),
        authAttempted(false), errorMessage(""), loginOpen(true),
        loading(false), animProgress(0.0f) {
        memset(keyInput, 0, sizeof(keyInput));
    }

    bool IsAuthenticated() const { return authenticated; }
    std::string GetOwner() const { return ownerName; }
    bool IsLoginOpen() const { return loginOpen; }

    void SetServer(const std::string& host, int port) {
        serverHost = host;
        serverPort = port;
    }

    bool Authenticate(const std::string& key) {
        loading = true;
        errorMessage = "";

        std::string hwid = GetHWID();
        std::string json = "{\"key\":\"" + key + "\",\"hwid\":\"" + hwid + "\"}";

        std::string response = HttpPost("/api/verify", json);
        loading = false;

        if (response.empty()) {
            // Offline fallback: if server isn't reachable, allow with warning
            errorMessage = "Server unreachable - offline mode";
            authenticated = true;
            authAttempted = true;
            loginOpen = false;
            return true;
        }

        // Simple JSON parsing
        auto findVal = [&](const std::string& key) -> std::string {
            auto pos = response.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            pos = response.find(':', pos);
            if (pos == std::string::npos) return "";
            pos++;
            while (pos < response.size() && (response[pos] == ' ' || response[pos] == '\t')) pos++;
            if (pos >= response.size()) return "";
            if (response[pos] == '"') {
                pos++;
                std::string val;
                while (pos < response.size() && response[pos] != '"') {
                    if (response[pos] == '\\' && pos + 1 < response.size()) pos++;
                    val += response[pos++];
                }
                return val;
            }
            std::string val;
            while (pos < response.size() && response[pos] != ',' && response[pos] != '}' && response[pos] != ']') {
                val += response[pos++];
            }
            return val;
        };

        std::string success = findVal("success");
        if (success == "true") {
            authenticated = true;
            authToken = findVal("token");
            ownerName = findVal("owner");
            authAttempted = true;
            loginOpen = false;
            return true;
        }
        else {
            errorMessage = findVal("message");
            if (errorMessage.empty()) errorMessage = "Authentication failed";
            authAttempted = true;
            return false;
        }
    }

    void RenderLoginWindow() {
        if (!loginOpen && authenticated) return;

        ImGui::SetNextWindowSize(ImVec2(380, 320), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(
            (GetSystemMetrics(SM_CXSCREEN) - 380) / 2,
            (GetSystemMetrics(SM_CYSCREEN) - 320) / 2
        ), ImGuiCond_Always);

        ImGui::Begin("AUTHENTICATION", &loginOpen,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();

        draw->AddRectFilledMultiColor(
            pos, ImVec2(pos.x + size.x, pos.y + 80),
            ImColor(0.95f, 0.35f, 0.00f, 0.12f),
            ImColor(0.95f, 0.35f, 0.00f, 0.04f),
            ImColor(0.95f, 0.35f, 0.00f, 0.04f),
            ImColor(0.95f, 0.35f, 0.00f, 0.12f)
        );

        ImGui::SetCursorPos(ImVec2(20, 16));
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.00f, 1.0f), "AUTH");
        ImGui::SetCursorPos(ImVec2(20, 36));
        ImGui::TextColored(ImVec4(0.50f, 0.50f, 0.55f, 1.0f), "Enter your license key to continue");

        ImGui::SetCursorPos(ImVec2(20, 100));
        ImGui::TextColored(ImVec4(0.70f, 0.70f, 0.75f, 1.0f), "License Key");
        ImGui::SetCursorPos(ImVec2(20, 118));
        ImGui::PushItemWidth(340);

        char hint[64] = "XXXX-XXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX";
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.10f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.80f, 1.00f, 1.00f));
        ImGui::InputText("##key", keyInput, sizeof(keyInput));
        ImGui::PopStyleColor(2);

        if (loading) {
            animProgress += ImGui::GetIO().DeltaTime * 2.0f;
            ImGui::SetCursorPos(ImVec2(20, 158));
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.00f, 0.5f + sinf(animProgress) * 0.3f), "Authenticating...");
        }
        else {
            ImGui::SetCursorPos(ImVec2(20, 158));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.35f, 0.00f, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.00f, 1.00f));
            if (ImGui::Button("AUTHENTICATE", ImVec2(340, 36))) {
                std::string key(keyInput);
                if (!key.empty()) Authenticate(key);
            }
            ImGui::PopStyleColor(2);
        }

        if (!errorMessage.empty()) {
            bool isOffline = (errorMessage.find("offline") != std::string::npos);
            ImGui::SetCursorPos(ImVec2(20, 208));
            ImVec4 errColor = isOffline
                ? ImVec4(1.0f, 0.80f, 0.00f, 0.90f)
                : ImVec4(1.00f, 0.25f, 0.25f, 0.90f);
            ImGui::TextColored(errColor, "%s", errorMessage.c_str());

            if (isOffline && errorMessage.find("offline") != std::string::npos) {
                ImGui::SetCursorPos(ImVec2(20, 226));
                ImGui::TextColored(ImVec4(0.50f, 0.50f, 0.55f, 0.80f),
                    "Running in offline mode - features limited");
            }
        }

        if (authAttempted && !authenticated) {
            ImGui::SetCursorPos(ImVec2(20, 250));
            ImGui::TextColored(ImVec4(0.30f, 0.30f, 0.35f, 0.70f),
                "Server: %s:%d", serverHost.c_str(), serverPort);
        }

        ImGui::End();
    }

    void ForceAuthenticated() {
        authenticated = true;
        loginOpen = false;
    }
};
