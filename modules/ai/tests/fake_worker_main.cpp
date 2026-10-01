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
 *
 * It answers the session-start message correctly in every mode, so the supervisor reaches the heartbeat.
 * It exits 0 on a shutdown request, like the real worker.
 */

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "xpe/ai/ai_worker_protocol.h"

namespace {

HANDLE g_pipe = INVALID_HANDLE_VALUE;

void Reply(uint32_t type, uint32_t request_id, const char* json) {
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
                static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
    h.messageType = type;
    h.requestId = request_id;
    h.payloadSize = static_cast<uint32_t>(std::strlen(json));
    h.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    DWORD w = 0;
    WriteFile(g_pipe, &h, sizeof(h), &w, nullptr);
    if (h.payloadSize > 0) WriteFile(g_pipe, json, h.payloadSize, &w, nullptr);
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
            case XPE_AI_MSG_SHUTDOWN:
                return 0;
            default:
                break;
        }
    }
}
