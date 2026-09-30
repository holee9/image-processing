/**
 * @file ai_worker_main.cpp
 * @brief Worker process entry point for AI inference (REQ-AI-003, REQ-AI-006)
 * @date 2026-04-23
 *
 * Wire protocol: ai_worker_protocol.h is the ONLY definition of it. Every
 * message is an XpeAiMessageHeader followed by payloadSize bytes of JSON.
 * (QA-B-169, #130: this file used to define a private message enum and a
 * private struct-per-message framing, "Stub message types for worker
 * (simplified)". Three shared numbers were crossed and the envelope was a
 * different shape, so the worker could not answer even a heartbeat from the
 * bridge. Nothing noticed because nothing in the product calls the bridge.)
 *
 * Lifecycle:
 * 1. Launch: Create named pipe server
 * 2. Init: Wait for INIT, respond with INIT_RESPONSE
 * 3. Heartbeat: Respond to HEARTBEAT with HEARTBEAT_ACK
 * 4. Shutdown: Handle SHUTDOWN by exiting (the header defines no SHUTDOWN
 *    acknowledgement; the pipe closing is the acknowledgement)
 *
 * NOT HANDLED YET: the twelve inference and model-management messages
 * (10-23). They get an XPE_AI_MSG_ERROR reply that says so (QA-B-170).
 *
 * Build modes:
 * - STUB (default): No ONNX Runtime linked
 * - FULL: ONNX Runtime linked for actual inference
 */

// @MX:NOTE: Worker process uses Windows named pipes for IPC. Each message is a 40-byte header plus JSON payload.
// @MX:NOTE: Single-client design - only one pipe instance allowed per worker.

#include <windows.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Protocol definitions -- the single source of truth for the wire format.
#include "xpe/ai/ai_worker_protocol.h"

namespace {
    constexpr DWORD PIPE_BUFFER_SIZE = 512;
    constexpr DWORD PIPE_TIMEOUT_MS = 0;
    constexpr const char* XPE_AI_WORKER_PIPE_NAME = "\\\\.\\pipe\\xpe_ai_worker";

    // Reported in INIT_RESPONSE, so the string has to be true for THIS build.
    // It used to sit outside any #ifdef and answer "0.1.0-stub" from a full
    // build too -- a diagnostic that lies about the thing it diagnoses.
#ifdef XPE_AI_STUB_BUILD
    constexpr const char* WORKER_VERSION_STRING = "0.1.0-stub";
    constexpr const char* WORKER_MODE_STRING    = "stub";
#else
    constexpr const char* WORKER_VERSION_STRING = "0.1.0-full";
    constexpr const char* WORKER_MODE_STRING    = "full";
#endif

    // xpe_error.h is not on this target's include path. Same value as
    // XPE_ERR_INVALID_INPUT; the bridge reports the same code for the same
    // class of fault.
    constexpr int32_t ERR_INVALID_INPUT = -1;

    constexpr uint32_t PROTOCOL_VERSION_WORD =
        (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
        static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
}

/**
 * @class WorkerServer
 * @brief Named pipe server for worker process communication
 */
class WorkerServer {
public:
    /**
     * @brief Construct worker server
     * @param pipe_name Named pipe name
     */
    explicit WorkerServer(const std::string& pipe_name)
        : pipe_name_(pipe_name)
        , pipe_handle_(INVALID_HANDLE_VALUE)
        , running_(false)
    {
    }

    /**
     * @brief Destructor - cleanup resources
     */
    ~WorkerServer() {
        Stop();
    }

    /**
     * @brief Start server and wait for client connection
     * @return true if successful
     * @MX:ANCHOR: Creates named pipe server for IPC. Called by main() once at startup.
     * @MX:REASON: Single client design - max_instances=1 prevents concurrent connections.
     */
    bool Start() {
        // Create named pipe
        pipe_handle_ = CreateNamedPipeA(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,                          // Max instances (single client)
            PIPE_BUFFER_SIZE,           // Output buffer size
            PIPE_BUFFER_SIZE,           // Input buffer size
            PIPE_TIMEOUT_MS,            // Default timeout
            nullptr                     // Default security
        );

        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "CreateNamedPipe failed: " << GetLastError() << std::endl;
            return false;
        }

        std::cout << "[Worker] Pipe created: " << pipe_name_ << std::endl;
        running_ = true;
        return true;
    }

    /**
     * @brief Wait for client to connect
     * @return true if connection successful
     */
    bool WaitForConnection() {
        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "[Worker] Invalid pipe handle" << std::endl;
            return false;
        }

        std::cout << "[Worker] Waiting for client connection..." << std::endl;

        BOOL result = ConnectNamedPipe(pipe_handle_, nullptr);
        if (!result) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                // Client already connected - this is OK
                std::cout << "[Worker] Client already connected" << std::endl;
                return true;
            }
            std::cerr << "[Worker] ConnectNamedPipe failed: " << error << std::endl;
            return false;
        }

        std::cout << "[Worker] Client connected" << std::endl;
        return true;
    }

    /**
     * @brief Main message loop
     *
     * Reads one XpeAiMessageHeader, then payloadSize bytes of payload, then
     * dispatches on header.messageType. A frame that cannot be trusted (wrong
     * magic, wrong protocol major, oversized payload) is answered with an
     * XPE_AI_MSG_ERROR and the loop ends: with no way to find the next frame
     * boundary, carrying on would decode garbage as messages.
     */
    void Run() {
        while (running_) {
            XpeAiMessageHeader header{};
            if (!ReadHeader(header)) {
                if (running_) {
                    std::cout << "[Worker] Client disconnected or error" << std::endl;
                }
                break;
            }

            if (header.magic != XPE_AI_MSG_MAGIC) {
                std::cerr << "[Worker] Bad magic 0x" << std::hex << header.magic
                          << std::dec << std::endl;
                SendError(header.requestId, "bad message magic");
                break;
            }
            if ((header.version >> 16) != static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR)) {
                SendError(header.requestId, "unsupported protocol major version");
                break;
            }
            if (header.payloadSize > XPE_AI_MAX_PAYLOAD_SIZE) {
                SendError(header.requestId, "payload exceeds the protocol maximum");
                break;
            }

            std::vector<char> payload(header.payloadSize);
            if (header.payloadSize > 0 && !ReadPayload(payload)) {
                std::cerr << "[Worker] Payload read failed" << std::endl;
                break;
            }

            HandleMessage(header);
        }
    }

    /**
     * @brief Stop server and cleanup
     */
    void Stop() {
        if (running_) {
            running_ = false;

            if (pipe_handle_ != INVALID_HANDLE_VALUE) {
                FlushFileBuffers(pipe_handle_);
                DisconnectNamedPipe(pipe_handle_);
                CloseHandle(pipe_handle_);
                pipe_handle_ = INVALID_HANDLE_VALUE;
            }
        }
    }

private:
    bool ReadHeader(XpeAiMessageHeader& header) {
        DWORD bytes_read = 0;
        const BOOL ok = ReadFile(pipe_handle_, &header, sizeof(header), &bytes_read, nullptr);
        return ok && bytes_read == sizeof(header);
    }

    bool ReadPayload(std::vector<char>& payload) {
        DWORD bytes_read = 0;
        const BOOL ok = ReadFile(pipe_handle_, payload.data(),
                                 static_cast<DWORD>(payload.size()), &bytes_read, nullptr);
        return ok && bytes_read == payload.size();
    }

    /**
     * @brief Handle a validated message by its header type.
     *
     * The numbers are the header enum. There is deliberately no second copy of
     * them in this file.
     */
    void HandleMessage(const XpeAiMessageHeader& header) {
        switch (header.messageType) {
            case XPE_AI_MSG_INIT:
                HandleInit(header);
                break;

            case XPE_AI_MSG_HEARTBEAT:
                HandleHeartbeat(header);
                break;

            case XPE_AI_MSG_SHUTDOWN:
                HandleShutdown();
                break;

            default:
                HandleUnsupported(header);
                break;
        }
    }

    /**
     * @brief Handle INIT
     * @MX:ANCHOR: Initializes worker protocol session. Called once per client connection.
     * @MX:REASON: Protocol handshake - must report what this build actually is.
     */
    void HandleInit(const XpeAiMessageHeader& header) {
        std::cout << "[Worker] Received INIT message" << std::endl;

        // Extends the documented INIT response (ai_worker_protocol.h) with
        // worker_version, mode and capabilities: the fields the old private
        // INIT_ACK carried, so the handshake does not get less informative.
        // capabilities is 0 because no inference message is handled yet.
        char json[256];
        std::snprintf(json, sizeof(json),
                      "{\"success\":true,\"loaded_models\":[],\"execution_provider\":\"cpu\","
                      "\"worker_pid\":%lu,\"worker_version\":\"%s\",\"mode\":\"%s\","
                      "\"capabilities\":0}",
                      static_cast<unsigned long>(GetCurrentProcessId()),
                      WORKER_VERSION_STRING, WORKER_MODE_STRING);
        SendFrame(XPE_AI_MSG_INIT_RESPONSE, header.requestId, json);
    }

    /**
     * @brief Handle HEARTBEAT
     * @MX:ANCHOR: Health check for worker process. Called periodically by host process.
     * @MX:REASON: Liveness check - must respond quickly to indicate worker is alive.
     */
    void HandleHeartbeat(const XpeAiMessageHeader& header) {
        std::cout << "[Worker] Received HEARTBEAT" << std::endl;

        char json[64];
        std::snprintf(json, sizeof(json), "{\"state\":%d}", static_cast<int>(XPE_AI_WORKER_IDLE));
        SendFrame(XPE_AI_MSG_HEARTBEAT_ACK, header.requestId, json);
    }

    /**
     * @brief Handle SHUTDOWN
     * @MX:ANCHOR: Graceful shutdown sequence. Sets running_ flag to false to exit Run() loop.
     * @MX:REASON: Clean shutdown - the process must actually exit, or REQ-AI-003 isolation leaves it resident.
     *
     * The header defines no SHUTDOWN acknowledgement. The old private enum had
     * SHUTDOWN_ACK = 6; it was dropped rather than added to the header, and the
     * pipe closing when this process exits is the acknowledgement.
     */
    void HandleShutdown() {
        std::cout << "[Worker] Received SHUTDOWN" << std::endl;
        running_ = false;
    }

    /**
     * @brief Handle a type this worker does not implement
     *
     * Covers the inference and model-management messages (10-23), which are
     * defined by the protocol and not yet handled here (QA-B-170), as well as
     * numbers the protocol does not define at all. The reply says which, so a
     * caller does not have to guess whether it sent nonsense or reached
     * something unfinished.
     */
    void HandleUnsupported(const XpeAiMessageHeader& header) {
        std::cerr << "[Worker] Unhandled message type: " << header.messageType << std::endl;

        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "message type %u is not handled by this worker",
                      static_cast<unsigned>(header.messageType));
        SendError(header.requestId, msg);
    }

    void SendError(uint32_t request_id, const char* message) {
        char json[256];
        std::snprintf(json, sizeof(json),
                      "{\"error_code\":%d,\"error_message\":\"%s\"}",
                      static_cast<int>(ERR_INVALID_INPUT), message);
        SendFrame(XPE_AI_MSG_ERROR, request_id, json);
    }

    /**
     * @brief Send one framed message: header, then JSON payload
     */
    void SendFrame(XpeAiMessageType type, uint32_t request_id, const char* json) {
        const uint32_t size = static_cast<uint32_t>(std::strlen(json));

        XpeAiMessageHeader header{};
        header.magic = XPE_AI_MSG_MAGIC;
        header.version = PROTOCOL_VERSION_WORD;
        header.messageType = static_cast<uint32_t>(type);
        header.requestId = request_id;
        header.payloadSize = size;
        header.flags = 0;
        header.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        DWORD written = 0;
        if (!WriteFile(pipe_handle_, &header, sizeof(header), &written, nullptr) ||
            written != sizeof(header)) {
            std::cerr << "[Worker] WriteFile (header) failed: " << GetLastError() << std::endl;
            return;
        }
        if (size > 0 &&
            (!WriteFile(pipe_handle_, json, size, &written, nullptr) || written != size)) {
            std::cerr << "[Worker] WriteFile (payload) failed: " << GetLastError() << std::endl;
        }
    }

    std::string pipe_name_;
    HANDLE pipe_handle_;
    bool running_;
};

/**
 * @brief Main entry point for worker process
 */
int main(int argc, char* argv[]) {
    std::cout << "[Worker] XPE AI Worker Process v" << WORKER_VERSION_STRING << std::endl;
    std::cout << "[Worker] Built: " << __DATE__ << " " << __TIME__ << std::endl;

#ifdef XPE_AI_STUB_BUILD
    std::cout << "[Worker] Mode: STUB (no ONNX Runtime)" << std::endl;
#else
    std::cout << "[Worker] Mode: FULL (with ONNX Runtime)" << std::endl;
#endif

    // Use default pipe name
    std::string pipe_name = XPE_AI_WORKER_PIPE_NAME;

    // Allow override via command line
    if (argc > 1) {
        pipe_name = argv[1];
    }

    std::cout << "[Worker] Pipe: " << pipe_name << std::endl;

    // Create and start server
    WorkerServer server(pipe_name);

    if (!server.Start()) {
        std::cerr << "[Worker] Failed to start server" << std::endl;
        return 1;
    }

    if (!server.WaitForConnection()) {
        std::cerr << "[Worker] Failed to wait for connection" << std::endl;
        return 1;
    }

    // Run message loop
    server.Run();

    std::cout << "[Worker] Exiting cleanly" << std::endl;
    return 0;
}
