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
 * 5. Bone suppress (QA-B-170): the one inference message served so far. It is
 *    the only one with an in-process reference (ai.cpp xpe_bone_suppress), so
 *    its answer can be checked against the answer the other path gives.
 *
 * 6. Body-part recognize (QA-B-191 M4b): the second inference message. The model is
 *    run here, but the DECISION is not: this reply carries what the model said (a
 *    label and a confidence, or that the output was refused), and the host applies
 *    the threshold and fallback_mode, which are the host's state.
 *
 * NOT HANDLED YET: the other ten inference and model-management messages
 * (12, 13, 16-23). They get an XPE_AI_MSG_ERROR reply that says so.
 *
 * Build modes:
 * - STUB (default): No ONNX Runtime linked
 * - FULL: ONNX Runtime linked for actual inference
 */

// @MX:NOTE: Worker process uses Windows named pipes for IPC. Each message is a 40-byte header plus JSON payload.
// @MX:NOTE: Single-client design - only one pipe instance allowed per worker.

#include <windows.h>
#include <sddl.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>

// Protocol definitions -- the single source of truth for the wire format.
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_error.h"

// The body-part model loading and judgement are shared with the in-process path (xpe_ai.dll compiles the same
// headers): one definition of "usable model" and of "what the output means".
#include "ai_bodypart.h"
#include "ai_bodypart_decision.h"
#include "ai_bodypart_model.h"

#include <atomic>
#include <new>
#include <stdexcept>

#ifdef XPE_AI_TEST_HOOKS
namespace xpe::ai {
void TestSetBeforeFileReadHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
void TestSetBeforeLabelParseHook(void (*hook)()); // ai_onnx_session.cpp, test builds only (QA-B-195c)
}
namespace {
// TEST-ONLY (QA-B-195b). Environment variables of the worker's process, read in main() of a test build only:
//   XPE_AI_TEST_FAIL_MODEL_READ=<n>      the first n file reads of model loading fail like a shortage of memory
//   XPE_AI_TEST_FAIL_LABELS=<k>          the k-th call of the label reader's hook fails with std::bad_alloc (QA-B-195c):
//                                        k=1 is the start of the parse, k=2 the first label stored, and so on
//   XPE_AI_TEST_FAIL_WORKER_REQUEST=oom|std|payload   ONE request fails at the request boundary: with std::bad_alloc
//                                        inside the handler, with another exception type, or at the allocation of its payload
std::atomic<int> g_failFileReads{0};
std::atomic<int> g_labelCallsUntilFailure{0};
void FailLabelParseForTest() {
    if (g_labelCallsUntilFailure.fetch_sub(1) == 1) throw std::bad_alloc();
}
std::atomic<int> g_requestFault{0};   // 0 none, 1 oom, 2 other exception, 3 payload allocation
void FailFileReadForTest() {
    if (g_failFileReads.fetch_sub(1) > 0) throw std::bad_alloc();
}
bool ConsumeFault(int kind) {
    int expected = kind;
    return g_requestFault.compare_exchange_strong(expected, 0);
}
void MaybeFailRequestForTest() {
    if (ConsumeFault(1)) throw std::bad_alloc();
    if (ConsumeFault(2)) throw std::runtime_error("injected exception");
}
/** Only a request that carries an image can fail here: the heartbeat that starts every worker must not use the fault up. */
void MaybeFailPayloadForTest(uint32_t messageType) {
    if (messageType != XPE_AI_MSG_BONE_SUPPRESS && messageType != XPE_AI_MSG_BODYPART_RECOGNIZE) return;
    if (ConsumeFault(3)) throw std::bad_alloc();
}
}  // namespace
#else
namespace {
inline void MaybeFailRequestForTest() {}
inline void MaybeFailPayloadForTest(uint32_t) {}
}  // namespace
#endif

namespace {
    constexpr DWORD PIPE_BUFFER_SIZE = XPE_AI_PIPE_BUFFER_SIZE;
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

    constexpr uint32_t PROTOCOL_VERSION_WORD =
        (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
        static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
}

namespace {

/**
 * @brief Read a string field from a small flat JSON object.
 *
 * Deliberately minimal (no nlohmann on this target): finds "key", the colon,
 * and a quoted value, and undoes backslash escapes. It does not parse nested
 * objects and would match a key name that also appears inside a string value.
 * The peer is the one local single client this pipe admits and the fields are
 * the ones ai_worker_protocol.h documents, so that limit is accepted rather
 * than hidden.
 */
bool JsonString(const std::string& json, const char* key, std::string& out) {
    const std::string needle = std::string("\"") + key + "\"";
    size_t at = json.find(needle);
    if (at == std::string::npos) return false;
    at = json.find(':', at + needle.size());
    if (at == std::string::npos) return false;
    ++at;
    while (at < json.size() && json[at] == ' ') ++at;
    if (at >= json.size() || json[at] != '"') return false;
    ++at;
    out.clear();
    for (; at < json.size(); ++at) {
        char c = json[at];
        if (c == '"') return true;
        if (c == '\\' && at + 1 < json.size()) c = json[++at];
        out += c;
    }
    return false;   // unterminated string
}

/** Read an unsigned integer field; rejects a missing key, no digits, overflow. */
bool JsonUInt(const std::string& json, const char* key, uint32_t& out) {
    const std::string needle = std::string("\"") + key + "\"";
    size_t at = json.find(needle);
    if (at == std::string::npos) return false;
    at = json.find(':', at + needle.size());
    if (at == std::string::npos) return false;
    ++at;
    while (at < json.size() && json[at] == ' ') ++at;
    uint64_t value = 0;
    size_t digits = 0;
    for (; at < json.size() && json[at] >= '0' && json[at] <= '9'; ++at, ++digits) {
        value = value * 10 + static_cast<uint64_t>(json[at] - '0');
        if (value > 0xFFFFFFFFull) return false;
    }
    if (digits == 0) return false;
    out = static_cast<uint32_t>(value);
    return true;
}

/** Escape a message for embedding in a JSON string (session errors carry paths). */
std::string JsonEscape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (out.size() > 300) break;   // an error message, not a log
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) < 0x20) c = ' ';
        out += c;
    }
    return out;
}

}  // namespace

/**
 * The security of the worker's pipe: full access for the user this process runs as, and for SYSTEM; nothing for anyone
 * else (no Everyone, no Anonymous, no Authenticated Users, no Administrators). Built from the process token's own user
 * SID, never from a name. The SID text and the descriptor live as long as this object.
 */
struct PipeSecurity {
    SECURITY_ATTRIBUTES attributes{};
    PSECURITY_DESCRIPTOR descriptor = nullptr;

    bool Build() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<char> buf(size);
        const BOOL got = GetTokenInformation(token, TokenUser, buf.data(), size, &size);
        CloseHandle(token);
        if (!got) return false;
        LPSTR sidText = nullptr;
        if (!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buf.data())->User.Sid, &sidText)) return false;
        const std::string sddl = std::string("D:P(A;;GA;;;") + sidText + ")(A;;GA;;;SY)";
        LocalFree(sidText);
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
            return false;
        }
        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = descriptor;
        attributes.bInheritHandle = FALSE;
        return true;
    }
    ~PipeSecurity() {
        if (descriptor) LocalFree(descriptor);
    }
    PipeSecurity() = default;
    PipeSecurity(const PipeSecurity&) = delete;
    PipeSecurity& operator=(const PipeSecurity&) = delete;
};

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
        // QA-B-198 M2 (REQ-AI-093, D5): the pipe is open to the user that runs this worker and to SYSTEM, and to no one
        // else. The default security of a pipe grants read to Everyone and to Anonymous, and with ONE instance the first
        // process to connect owns the conversation: it could be someone other than the host. No security descriptor, no
        // pipe -- the worker does not fall back to the default.
        PipeSecurity security;
        if (!security.Build()) {
            std::cerr << "Pipe security descriptor failed: " << GetLastError() << std::endl;
            return false;
        }
        // Create named pipe
        pipe_handle_ = CreateNamedPipeA(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,                          // Max instances (single client)
            PIPE_BUFFER_SIZE,           // Output buffer size
            PIPE_BUFFER_SIZE,           // Input buffer size
            PIPE_TIMEOUT_MS,            // Default timeout
            &security.attributes        // Current user + SYSTEM only
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
                SendError(header.requestId, XPE_ERR_INVALID_INPUT, "bad message magic");
                break;
            }
            if ((header.version >> 16) != static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR)) {
                SendError(header.requestId, XPE_ERR_INVALID_INPUT, "unsupported protocol major version");
                break;
            }
            if (header.payloadSize > XPE_AI_MAX_PAYLOAD_SIZE) {
                SendError(header.requestId, XPE_ERR_INVALID_INPUT, "payload exceeds the protocol maximum");
                break;
            }

            // QA-B-195b: nothing a request does may take the worker down with an uncaught exception. A shortage of memory
            // is answered with XPE_ERR_OUT_OF_MEMORY and any other exception with XPE_ERR_PROCESSING_FAILED, and the loop
            // goes on: the host sees the worker's own answer instead of a dead pipe. If even the answer cannot be sent
            // the loop ends, so the host finds out at once instead of at the end of its time budget.
            std::vector<char> payload;
            try {
                MaybeFailPayloadForTest(header.messageType);
                payload.resize(header.payloadSize);
            } catch (const std::bad_alloc&) {
                // The payload's bytes are still in the pipe: they are read and thrown away, or the next frame would be
                // decoded from the middle of this one.
                if ((header.payloadSize > 0 && !DiscardPayload(header.payloadSize)) ||
                    !SendErrorSafe(header.requestId, XPE_ERR_OUT_OF_MEMORY, "out of memory")) {
                    break;
                }
                continue;
            }
            if (header.payloadSize > 0 && !ReadPayload(payload)) {
                std::cerr << "[Worker] Payload read failed" << std::endl;
                break;
            }

            try {
                HandleMessage(header, payload);
            } catch (const std::bad_alloc&) {
                if (!SendErrorSafe(header.requestId, XPE_ERR_OUT_OF_MEMORY, "out of memory")) break;
            } catch (...) {
                if (!SendErrorSafe(header.requestId, XPE_ERR_PROCESSING_FAILED, "unexpected exception in the worker")) break;
            }
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
    /** SendError that cannot throw; false when the answer could not be built or sent. */
    bool SendErrorSafe(uint32_t request_id, int code, const char* message) noexcept {
        try {
            SendError(request_id, code, message);
            return true;
        } catch (...) {
            return false;
        }
    }

    /** Read and throw away the rest of one message of @p remaining bytes (the pipe is in message mode). */
    bool DiscardPayload(uint32_t remaining) {
        char buf[4096];
        for (;;) {
            DWORD got = 0;
            const BOOL ok = ReadFile(pipe_handle_, buf, sizeof(buf), &got, nullptr);
            if (!ok && GetLastError() != ERROR_MORE_DATA) return false;
            if (got > remaining) return false;   // more than the header promised
            remaining -= got;
            if (ok) return remaining == 0;       // the message ended
        }
    }

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
    void HandleMessage(const XpeAiMessageHeader& header, const std::vector<char>& payload) {
        switch (header.messageType) {
            case XPE_AI_MSG_INIT:
                HandleInit(header, payload);
                break;

            case XPE_AI_MSG_HEARTBEAT:
                HandleHeartbeat(header);
                break;

            case XPE_AI_MSG_SHUTDOWN:
                HandleShutdown();
                break;

            case XPE_AI_MSG_BONE_SUPPRESS:
                HandleBoneSuppress(header, payload);
                break;

            case XPE_AI_MSG_BODYPART_RECOGNIZE:
                HandleBodyPart(header, payload);
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
    void HandleInit(const XpeAiMessageHeader& header, const std::vector<char>& payload) {
        std::cout << "[Worker] Received INIT message" << std::endl;

        // model_dir is the one INIT field this worker acts on. Absent leaves the
        // previous directory alone; the session cache is keyed on it, so a
        // changed directory reloads the model on the next request.
        if (!payload.empty()) {
            std::string dir;
            if (JsonString(std::string(payload.data(), payload.size()), "model_dir", dir)) {
                model_dir_ = dir;
            }
        }

        // Extends the documented INIT response (ai_worker_protocol.h) with
        // worker_version, mode and capabilities: the fields the old private
        // INIT_ACK carried, so the handshake does not get less informative.
        // capabilities: 1 when a real model serves BONE_SUPPRESS, 0 when none
        // does. It is read from what the session layer actually is rather than
        // from a constant, so a stub worker cannot claim it.
        char json[256];
        std::snprintf(json, sizeof(json),
                      "{\"success\":true,\"loaded_models\":[],\"execution_provider\":\"cpu\","
                      "\"worker_pid\":%lu,\"worker_version\":\"%s\",\"mode\":\"%s\","
                      "\"capabilities\":%d}",
                      static_cast<unsigned long>(GetCurrentProcessId()),
                      WORKER_VERSION_STRING, WORKER_MODE_STRING,
                      xpe::ai::OnnxSession::IsStubBuild() ? 0 : 1);
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
     * Covers the eleven inference and model-management messages still to do
     * (10-13, 16-23), which the protocol defines and this worker does not yet
     * handle, as well as numbers the protocol does not define at all. The reply
     * says which, so a caller does not have to guess whether it sent nonsense
     * or reached something unfinished.
     */
    void HandleUnsupported(const XpeAiMessageHeader& header) {
        std::cerr << "[Worker] Unhandled message type: " << header.messageType << std::endl;

        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "message type %u is not handled by this worker",
                      static_cast<unsigned>(header.messageType));
        SendError(header.requestId, XPE_ERR_INVALID_INPUT, msg);
    }

    /**
     * @brief Handle BONE_SUPPRESS (QA-B-170): float32 image in, float32 image out
     *
     * The same model, the same path resolution and the same error mapping as
     * the in-process xpe_bone_suppress in ai.cpp, on purpose: the two paths are
     * compared against each other by tests/test_worker_protocol_conformance.cpp,
     * and an error code that differed for the same fault would be a difference
     * nobody chose. The check order also follows ai.cpp -- format first, then
     * the size of the pixel data, then the model -- so a request that is
     * malformed AND points at a missing model gets the malformed answer.
     */
    void HandleBoneSuppress(const XpeAiMessageHeader& header, const std::vector<char>& payload) {
        const uint32_t id = header.requestId;

        uint32_t width = 0, height = 0;
        size_t pixel_offset = 0;
        if (!ParseImageRequest(header, payload, "bone suppress", width, height, pixel_offset)) return;
        MaybeFailRequestForTest();
        const size_t pixel_bytes = payload.size() - pixel_offset;
        const uint64_t count = static_cast<uint64_t>(width) * height;

        const std::string model_path = model_dir_.empty()
            ? std::string("bone_suppress.onnx")
            : model_dir_ + "/bone_suppress.onnx";

        // Lazy load, and reload when INIT pointed somewhere else.
        if (!session_ || session_dir_ != model_dir_) {
            xpe::ai::OnnxSessionConfig cfg;
            cfg.model_path = model_path;
            cfg.role = "bone_suppress";   // part of what the signature covers (QA-B-195)
            cfg.execution_provider = xpe::ai::ExecutionProvider::kCpu;
            cfg.num_threads = 1;

            auto created = xpe::ai::OnnxSession::Create(cfg);
            if (!created.has_value()) {
                // Three causes, three codes -- the same three ai.cpp separates.
                switch (created.code) {
                    case xpe::ai::OnnxErrorCode::kInvalidModelPath:
                        // Still counted by the host (user-approved policy 2026-10-01, REQ-CHANGE-LOG-P3-AI.md row 3):
                        // QA-B-195 D6 changes only the signature refusal below.
                        SendError(id, XPE_ERR_IO_FAILED, "no model at " + model_path);
                        break;
                    case xpe::ai::OnnxErrorCode::kModelLoadFailed:
                        SendError(id, XPE_ERR_CONFIG_INVALID,
                                  "model unreadable: " + created.message);
                        break;
                    case xpe::ai::OnnxErrorCode::kOutOfMemory:
                        // QA-B-194b: named as a shortage of memory, never as a failed or unusable model.
                        SendError(id, XPE_ERR_OUT_OF_MEMORY, "out of memory while loading the model");
                        break;
                    case xpe::ai::OnnxErrorCode::kModelNotTrusted:
                        // QA-B-195 D6: a model that fails signature verification is unavailable, like a missing
                        // one: the worker is healthy, so the host must not count it toward switching the worker off.
                        SendError(id, XPE_ERR_CONFIG_INVALID,
                                  "model not trusted: " + created.message, /*model_unavailable=*/true);
                        break;
                    case xpe::ai::OnnxErrorCode::kSidecarInvalid:
                        // QA-B-197: like a signature refusal -- unavailable, not a fault of the worker.
                        SendError(id, XPE_ERR_CONFIG_INVALID,
                                  "model sidecar invalid: " + created.message, /*model_unavailable=*/true);
                        break;
                    default:
                        SendError(id, XPE_ERR_PROCESSING_FAILED,
                                  "session failed: " + created.message);
                        break;
                }
                return;
            }
            session_ = std::move(created.value);
            session_dir_ = model_dir_;
        }

        std::vector<float> input(static_cast<size_t>(count));
        std::memcpy(input.data(), payload.data() + pixel_offset, pixel_bytes);

        auto out = session_->Run(input);
        if (out.code != xpe::ai::OnnxErrorCode::kOk) {
            // A stub build lands here on every request (its Run() refuses).
            SendError(id,
                      out.code == xpe::ai::OnnxErrorCode::kInvalidInput
                          ? XPE_ERR_INVALID_INPUT : XPE_ERR_PROCESSING_FAILED,
                      "run failed: " + out.message);
            return;
        }
        if (out.value.size() != count) {
            SendError(id, XPE_ERR_PROCESSING_FAILED, "model returned an unexpected pixel count");
            return;
        }

        char json[128];
        const int json_len = std::snprintf(
            json, sizeof(json),
            "{\"success\":true,\"width\":%u,\"height\":%u,\"format\":\"float32\"}",
            static_cast<unsigned>(width), static_cast<unsigned>(height));
        std::vector<char> reply(sizeof(uint32_t) + static_cast<size_t>(json_len) + pixel_bytes);
        const uint32_t reply_json = static_cast<uint32_t>(json_len);
        std::memcpy(reply.data(), &reply_json, sizeof(reply_json));
        std::memcpy(reply.data() + sizeof(reply_json), json, reply_json);
        std::memcpy(reply.data() + sizeof(reply_json) + reply_json,
                    out.value.data(), pixel_bytes);
        SendPayload(XPE_AI_MSG_BONE_SUPPRESS_RESP, id, XPE_AI_FLAG_HAS_BINARY_PAYLOAD,
                    reply.data(), static_cast<uint32_t>(reply.size()));
    }

    /**
     * @brief Parse an image request (length-prefixed JSON + raw float32 pixels), answering with an ERROR frame
     * and returning false when it is malformed. Shared by every message that carries an image, so they all
     * refuse the same faults with the same codes, in the same order: framing, metadata, format, pixel count.
     */
    bool ParseImageRequest(const XpeAiMessageHeader& header, const std::vector<char>& payload, const char* what,
                           uint32_t& width, uint32_t& height, size_t& pixel_offset) {
        const uint32_t id = header.requestId;
        if ((header.flags & XPE_AI_FLAG_HAS_BINARY_PAYLOAD) == 0 || payload.size() < sizeof(uint32_t)) {
            SendError(id, XPE_ERR_INVALID_INPUT,
                      std::string(what) + " needs a length-prefixed JSON followed by pixel data");
            return false;
        }
        uint32_t json_size = 0;
        std::memcpy(&json_size, payload.data(), sizeof(json_size));
        if (json_size > payload.size() - sizeof(uint32_t)) {
            SendError(id, XPE_ERR_INVALID_INPUT, "metadata length exceeds the payload");
            return false;
        }
        const std::string meta(payload.data() + sizeof(uint32_t), json_size);
        pixel_offset = sizeof(uint32_t) + json_size;
        const size_t pixel_bytes = payload.size() - pixel_offset;

        std::string format;
        if (!JsonUInt(meta, "width", width) || !JsonUInt(meta, "height", height) ||
            !JsonString(meta, "format", format)) {
            SendError(id, XPE_ERR_INVALID_INPUT, "metadata must carry width, height and format");
            return false;
        }
        if (format != "float32") {
            SendError(id, XPE_ERR_UNSUPPORTED_FORMAT, "only float32 pixels are supported");
            return false;
        }
        const uint64_t count = static_cast<uint64_t>(width) * height;
        if (count == 0 || count * sizeof(float) != pixel_bytes) {
            SendError(id, XPE_ERR_INVALID_INPUT, "pixel data does not match width*height");
            return false;
        }
        return true;
    }

    /**
     * @brief Handle BODYPART_RECOGNIZE (QA-B-191 M4b): a float32 image in, what the model said out.
     *
     * Reply (JSON, no pixels): {"success":true,"outcome":"ok","body_part":"CHEST","confidence":0.75}
     * outcome is "ok", "non_finite" or "out_of_range"; the last two carry no label and no confidence. The
     * judgement is xpe::ai::JudgeBodyPartOutput, the function the in-process path calls.
     *
     * An unusable MODEL (no file, unreadable, bad labels, bad shape, wrong output size) is an ERROR frame that
     * carries "model_unavailable":true -- a configuration state, not a worker fault, and the host does not count
     * it. A model that exists but fails to RUN is an ordinary ERROR frame, and the host counts it.
     */
    void HandleBodyPart(const XpeAiMessageHeader& header, const std::vector<char>& payload) {
        const uint32_t id = header.requestId;
        uint32_t width = 0, height = 0;
        size_t pixel_offset = 0;
        if (!ParseImageRequest(header, payload, "body-part recognition", width, height, pixel_offset)) return;
        MaybeFailRequestForTest();
        const uint64_t count = static_cast<uint64_t>(width) * height;

        if (!bodypart_ || bodypart_dir_ != model_dir_) {
            std::unique_ptr<xpe::ai::BodyPartModel> built;
            xpe::ai::BodyPartLoadFailure kind = xpe::ai::BodyPartLoadFailure::kNone;
            std::string detail;
            if (const char* why = xpe::ai::LoadBodyPartModel(model_dir_, &built, &kind, &detail)) {
                bodypart_.reset();
                if (!detail.empty()) std::cerr << "[Worker] bodypart: " << detail << std::endl;
                if (kind == xpe::ai::BodyPartLoadFailure::kOutOfMemory) {
                    SendError(id, XPE_ERR_OUT_OF_MEMORY, why);   // QA-B-194b: not "unavailable"
                    return;
                }
                SendError(id, kind == xpe::ai::BodyPartLoadFailure::kNoModelFile ? XPE_ERR_IO_FAILED
                                                                                 : XPE_ERR_CONFIG_INVALID,
                          why, /*model_unavailable=*/true);
                return;
            }
            bodypart_ = std::move(built);
            bodypart_dir_ = model_dir_;
        }

        std::vector<float> pixels(static_cast<size_t>(count));
        std::memcpy(pixels.data(), payload.data() + pixel_offset, pixels.size() * sizeof(float));
        const std::vector<float> input = xpe::ai::ResizeImageFloat(pixels.data(), width, height,
                                                                   bodypart_->inputWidth, bodypart_->inputHeight);
        const auto out = bodypart_->session->Run(input);
        if (out.code != xpe::ai::OnnxErrorCode::kOk) {
            SendError(id, out.code == xpe::ai::OnnxErrorCode::kInvalidInput ? XPE_ERR_INVALID_INPUT
                                                                            : XPE_ERR_PROCESSING_FAILED,
                      "run failed: " + out.message);
            return;
        }
        if (out.value.size() != bodypart_->labels.size()) {
            SendError(id, XPE_ERR_CONFIG_INVALID, "the model output size differs from the number of labels",
                      /*model_unavailable=*/true);
            return;
        }

        const xpe::ai::BodyPartVerdict verdict = xpe::ai::JudgeBodyPartOutput(out.value.data(), out.value.size());
        std::string json = "{\"success\":true,\"outcome\":\"";
        switch (verdict.judgement) {
            case xpe::ai::BodyPartJudgement::kOk:
                json += "ok\",\"body_part\":\"" + bodypart_->labels[verdict.best] + "\",\"confidence\":" +
                        xpe::ai::ShortestFloatText(verdict.confidence) + "}";
                break;
            case xpe::ai::BodyPartJudgement::kNonFinite:
                json += "non_finite\"}";
                break;
            default:   // kOutOfRange; kEmpty cannot happen (the size equals the label count, which is non-empty)
                json += "out_of_range\"}";
                break;
        }
        SendFrame(XPE_AI_MSG_BODYPART_RECOGNIZE_RESP, id, json.c_str());
    }

    /** @p model_unavailable marks "the model cannot be used" (a state, not a fault); it precedes the message text. */
    void SendError(uint32_t request_id, int code, const std::string& message, bool model_unavailable = false) {
        const std::string json = "{\"error_code\":" + std::to_string(code) +
                                 (model_unavailable ? ",\"model_unavailable\":true" : "") +
                                 ",\"error_message\":\"" + JsonEscape(message) + "\"}";
        SendFrame(XPE_AI_MSG_ERROR, request_id, json.c_str());
    }

    /**
     * @brief Send one framed message with a JSON payload
     */
    void SendFrame(XpeAiMessageType type, uint32_t request_id, const char* json) {
        SendPayload(type, request_id, 0, json, static_cast<uint32_t>(std::strlen(json)));
    }

    /**
     * @brief Send one framed message: header, then payload bytes
     *
     * @p flags carries XPE_AI_FLAG_HAS_BINARY_PAYLOAD when the payload is the
     * length-prefixed JSON + pixels layout ai_worker_protocol.h documents.
     */
    void SendPayload(XpeAiMessageType type, uint32_t request_id, uint32_t flags,
                     const void* payload, uint32_t size) {
        XpeAiMessageHeader header{};
        header.magic = XPE_AI_MSG_MAGIC;
        header.version = PROTOCOL_VERSION_WORD;
        header.messageType = static_cast<uint32_t>(type);
        header.requestId = request_id;
        header.payloadSize = size;
        header.flags = flags;
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
            (!WriteFile(pipe_handle_, payload, size, &written, nullptr) || written != size)) {
            std::cerr << "[Worker] WriteFile (payload) failed: " << GetLastError() << std::endl;
        }
    }

    std::string pipe_name_;
    HANDLE pipe_handle_;
    bool running_;

    // Set by INIT; BONE_SUPPRESS resolves <model_dir_>/bone_suppress.onnx.
    std::string model_dir_;
    // The loaded model and the directory it came from, so a changed model_dir_
    // reloads instead of silently serving the old model.
    std::unique_ptr<xpe::ai::OnnxSession> session_;
    std::string session_dir_;
    // The body-part model (QA-B-191 M4b), cached the same way and for the same reason.
    std::unique_ptr<xpe::ai::BodyPartModel> bodypart_;
    std::string bodypart_dir_;
};

/**
 * @brief Main entry point for worker process
 */
#ifdef XPE_AI_TEST_HOOKS
namespace xpe::ai {
void TestSetBeforeSessionHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
}
// TEST-ONLY (QA-B-194b): with XPE_AI_TEST_FAIL_SESSION_CREATE=oom in the environment the worker fails every session
// creation the way a shortage of memory does, so the host's handling of that answer can be tested through a REAL
// worker. Not compiled into a delivery build.
static void ThrowOutOfMemory() { throw std::bad_alloc(); }
#endif

int main(int argc, char* argv[]) {
#ifdef XPE_AI_TEST_HOOKS
    {
        char v[8] = {0};
        if (GetEnvironmentVariableA("XPE_AI_TEST_FAIL_SESSION_CREATE", v, sizeof(v)) > 0 && std::strcmp(v, "oom") == 0) {
            xpe::ai::TestSetBeforeSessionHook(&ThrowOutOfMemory);
        }
        char n[16] = {0};
        if (GetEnvironmentVariableA("XPE_AI_TEST_FAIL_MODEL_READ", n, sizeof(n)) > 0) {
            g_failFileReads = std::atoi(n);
            xpe::ai::TestSetBeforeFileReadHook(&FailFileReadForTest);
        }
        char lb[16] = {0};
        if (GetEnvironmentVariableA("XPE_AI_TEST_FAIL_LABELS", lb, sizeof(lb)) > 0) {
            g_labelCallsUntilFailure = std::atoi(lb);
            xpe::ai::TestSetBeforeLabelParseHook(&FailLabelParseForTest);
        }
        char f[16] = {0};
        if (GetEnvironmentVariableA("XPE_AI_TEST_FAIL_WORKER_REQUEST", f, sizeof(f)) > 0) {
            if (std::strcmp(f, "oom") == 0) g_requestFault = 1;
            else if (std::strcmp(f, "std") == 0) g_requestFault = 2;
            else if (std::strcmp(f, "payload") == 0) g_requestFault = 3;
        }
    }
#endif
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
