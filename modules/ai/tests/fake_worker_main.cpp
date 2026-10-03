/**
 * @file fake_worker_main.cpp
 * @brief A worker that speaks the protocol but misbehaves on request (QA-B-171B, Codex audit #11).
 *
 * TEST ONLY. The real worker always answers a heartbeat correctly, so a supervisor test that needs a
 * WRONG answer cannot get one from it. This process is a pipe server like the real worker, started the same
 * way (`fake_worker.exe <pipe-name>`), and what it gets wrong is chosen by the environment variable
 * XPE_FAKE_WORKER_MODE, which the child inherits:
 *
 *   (unset / "ok")  answers correctly
 *   "wrong_type"    answers a heartbeat with a frame of the wrong type (right request id)
 *   "wrong_id"      answers a heartbeat with the right type but someone else's request id
 *   "no_pipe"       never creates its pipe: alive, and nobody can ever connect (a worker that hangs on start)
 *   "exit_on_start" exits at once with code 7 (a worker that dies on start)
 *   "bone_valid_nan"    answers a BONE_SUPPRESS request with a VALID success envelope and NaN pixels (QA-B-181j)
 *   "bone_garbage_nan"  answers a BONE_SUPPRESS request with an EMPTY JSON envelope and NaN pixels (QA-B-181j)
 *   "bodypart_raw"      answers a BODYPART_RECOGNIZE request with a RESP frame whose JSON is the environment
 *                       variable XPE_FAKE_WORKER_JSON, verbatim (QA-B-191 M4b: the strict-parse table)
 *   "bodypart_error_raw" answers it with an ERROR frame whose JSON is XPE_FAKE_WORKER_JSON, verbatim
 *   (the two raw ERROR modes also set the frame's header flags to XPE_FAKE_WORKER_FLAGS, a decimal number; QA-B-193b)
 *   "bone_error_raw"    answers a BONE_SUPPRESS request with an ERROR frame whose JSON is XPE_FAKE_WORKER_JSON,
 *                       verbatim (QA-B-193)
 *   "capability_probe_exit"  answers the session-start message, then TRIES five things and exits with the code
 *                       kProbeMarker | (one bit for each that WORKED) -- the way a restricted worker can report what it
 *                       can still do when it cannot write a file to say so (QA-B-198 M2, REQ-AI-093):
 *                         bit 0 (1)  create a file in %TEMP%
 *                         bit 1 (2)  create a file in the current directory
 *                         bit 2 (4)  create a registry key under HKCU\Software
 *                         bit 3 (8)  start a child process (cmd.exe /c exit 0)
 *                         bit 4 (16) connect to 127.0.0.1:XPE_FAKE_WORKER_PORT (the NETWORK)
 *
 * It answers the session-start message correctly in every mode, so the supervisor reaches the heartbeat.
 * It exits 0 on a shutdown request, like the real worker.
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "xpe/ai/ai_worker_protocol.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

constexpr DWORD kProbeMarker = 0x100;   // so that "no capability left" is not the exit code 0 of a clean shutdown

/** True when a file can be created (and is deleted again) in @p dir. */
bool CanCreateFileIn(const std::string& dir) {
    if (dir.empty()) return false;
    const std::string p = dir + "\\xpe_fake_probe_" + std::to_string(GetCurrentProcessId()) + ".tmp";
    HANDLE h = CreateFileA(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

/** The exit code of capability_probe_exit: kProbeMarker | a bit for each thing that worked (see the file header). */
DWORD ProbeCapabilities() {
    DWORD bits = 0;
    char temp[MAX_PATH] = {0};
    if (GetTempPathA(MAX_PATH, temp) > 0) {
        std::string t = temp;
        while (!t.empty() && (t.back() == '\\' || t.back() == '/')) t.pop_back();
        if (CanCreateFileIn(t)) bits |= 1;
    }
    char cwd[MAX_PATH] = {0};
    if (GetCurrentDirectoryA(MAX_PATH, cwd) > 0 && CanCreateFileIn(cwd)) bits |= 2;
    HKEY key = nullptr;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\xpe_fake_probe", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) ==
        ERROR_SUCCESS) {
        bits |= 4;
        RegCloseKey(key);
        RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\xpe_fake_probe");
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char cmd[] = "C:\\Windows\\System32\\cmd.exe /c exit 0";
    if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        bits |= 8;
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    char port[16] = {0};
    if (GetEnvironmentVariableA("XPE_FAKE_WORKER_PORT", port, sizeof(port)) > 0) {
        WSADATA wd;
        if (WSAStartup(MAKEWORD(2, 2), &wd) == 0) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s != INVALID_SOCKET) {
                sockaddr_in a{};
                a.sin_family = AF_INET;
                a.sin_port = htons(static_cast<u_short>(std::atoi(port)));
                inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
                if (connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0) bits |= 16;
                closesocket(s);
            }
            WSACleanup();
        }
    }
    return kProbeMarker | bits;
}

HANDLE g_pipe = INVALID_HANDLE_VALUE;

void Reply(uint32_t type, uint32_t request_id, const char* json, uint32_t flags = 0) {
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
                static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
    h.messageType = type;
    h.requestId = request_id;
    h.payloadSize = static_cast<uint32_t>(std::strlen(json));
    h.flags = flags;
    h.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    DWORD w = 0;
    WriteFile(g_pipe, &h, sizeof(h), &w, nullptr);
    if (h.payloadSize > 0) WriteFile(g_pipe, json, h.payloadSize, &w, nullptr);
}

/** Answer a BONE_SUPPRESS request with an envelope of the caller's choice and every pixel NaN. */
void ReplyBoneNaN(const XpeAiMessageHeader& req, const std::vector<char>& payload, const std::string& json) {
    uint32_t reqjson = 0;
    if (payload.size() >= sizeof(reqjson)) std::memcpy(&reqjson, payload.data(), sizeof(reqjson));
    const size_t pixel_bytes = payload.size() >= sizeof(reqjson) + reqjson ? payload.size() - sizeof(reqjson) - reqjson : 0;
    const uint32_t jn = static_cast<uint32_t>(json.size());
    std::vector<char> body(sizeof(jn) + jn + pixel_bytes);
    std::memcpy(body.data(), &jn, sizeof(jn));
    std::memcpy(body.data() + sizeof(jn), json.data(), jn);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (size_t i = 0; i + sizeof(float) <= pixel_bytes; i += sizeof(float)) {
        std::memcpy(body.data() + sizeof(jn) + jn + i, &nan, sizeof(float));
    }
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
                static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
    h.messageType = XPE_AI_MSG_BONE_SUPPRESS_RESP;
    h.requestId = req.requestId;
    h.payloadSize = static_cast<uint32_t>(body.size());
    h.flags = XPE_AI_FLAG_HAS_BINARY_PAYLOAD;
    DWORD w = 0;
    WriteFile(g_pipe, &h, sizeof(h), &w, nullptr);
    WriteFile(g_pipe, body.data(), h.payloadSize, &w, nullptr);
}

/** The reply JSON a "bodypart_raw" / "bodypart_error_raw" worker sends (up to 900 bytes). */
std::string EnvJson() {
    char buf[1024] = {0};
    size_t len = 0;
    if (getenv_s(&len, buf, sizeof(buf), "XPE_FAKE_WORKER_JSON") != 0 || len == 0) return std::string();
    return buf;
}

/** The header flags a raw ERROR mode sends (decimal in XPE_FAKE_WORKER_FLAGS; unset = 0). QA-B-193b. */
uint32_t EnvFlags() {
    char buf[32] = {0};
    size_t len = 0;
    if (getenv_s(&len, buf, sizeof(buf), "XPE_FAKE_WORKER_FLAGS") != 0 || len == 0) return 0;
    return static_cast<uint32_t>(std::strtoul(buf, nullptr, 10));
}

std::string Mode() {
    char buf[64] = {0};
    size_t len = 0;
    if (getenv_s(&len, buf, sizeof(buf), "XPE_FAKE_WORKER_MODE") != 0 || len == 0) return "ok";
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string mode = Mode();
    if (mode == "exit_on_start") return 7;
    if (mode == "no_pipe") Sleep(INFINITE);
    g_pipe = CreateNamedPipeA(argv[1], PIPE_ACCESS_DUPLEX,
                              PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                              XPE_AI_PIPE_BUFFER_SIZE, XPE_AI_PIPE_BUFFER_SIZE, 0, nullptr);
    if (g_pipe == INVALID_HANDLE_VALUE) return 3;
    if (!ConnectNamedPipe(g_pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) return 4;

    for (;;) {
        XpeAiMessageHeader h{};
        DWORD got = 0;
        if (!ReadFile(g_pipe, &h, sizeof(h), &got, nullptr) || got != sizeof(h)) return 5;
        std::vector<char> payload(h.payloadSize);
        if (h.payloadSize > 0 &&
            (!ReadFile(g_pipe, payload.data(), h.payloadSize, &got, nullptr) ||
             got != h.payloadSize)) {
            return 6;
        }
        switch (h.messageType) {
            case XPE_AI_MSG_INIT:
                Reply(XPE_AI_MSG_INIT_RESPONSE, h.requestId, "{\"success\":true}");
                if (mode == "capability_probe_exit") ExitProcess(ProbeCapabilities());
                break;
            case XPE_AI_MSG_HEARTBEAT:
                if (mode == "wrong_type") {
                    Reply(XPE_AI_MSG_INIT_RESPONSE, h.requestId, "{\"state\":0}");
                } else if (mode == "wrong_id") {
                    Reply(XPE_AI_MSG_HEARTBEAT_ACK, h.requestId + 1000, "{\"state\":0}");
                } else {
                    Reply(XPE_AI_MSG_HEARTBEAT_ACK, h.requestId, "{\"state\":0}");
                }
                break;
            case XPE_AI_MSG_BONE_SUPPRESS:
                if (mode == "bone_valid_nan") {
                    ReplyBoneNaN(h, payload, "{\"success\":true,\"width\":3,\"height\":3,\"format\":\"float32\"}");
                } else if (mode == "bone_garbage_nan") {
                    ReplyBoneNaN(h, payload, "");
                } else if (mode == "bone_error_raw") {
                    Reply(XPE_AI_MSG_ERROR, h.requestId, EnvJson().c_str(), EnvFlags());
                }
                break;
            case XPE_AI_MSG_BODYPART_RECOGNIZE:
                if (mode == "bodypart_raw") {
                    Reply(XPE_AI_MSG_BODYPART_RECOGNIZE_RESP, h.requestId, EnvJson().c_str());
                } else if (mode == "bodypart_error_raw") {
                    Reply(XPE_AI_MSG_ERROR, h.requestId, EnvJson().c_str(), EnvFlags());
                }
                break;
            case XPE_AI_MSG_SHUTDOWN:
                return 0;
            default:
                break;
        }
    }
}
