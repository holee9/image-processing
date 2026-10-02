/**
 * @file ai_ipc_bridge.cpp
 * @brief IPC bridge implementation for XPE AI named pipe communication.
 *
 * Implements the client-side of the named pipe IPC between xpe_ai.dll
 * (in-process proxy) and xpe_ai_worker.exe (sandboxed worker process).
 *
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-092: Time budget enforcement (inference timeout).
 *   Was cited as AI requirement 009 here and in three other places (#210,
 *   QA-B-157). The literal "REQ-AI-" + "009" form is deliberately not
 *   written anywhere in this repo: the citation checker greps for it and
 *   cannot tell a citation from a historical note, so spelling it out here
 *   ADDED an orphan (4 -> 5) instead of removing one.
 *   That number is not defined in SPEC-XPE-P3-AI at all (0 definitions among
 *   its 46) and never was: it first appears in the bulk commit dd7c8e05
 *   (2026-04-28), in code and in the RTM at once, so it was a mis-numbering
 *   and not a renaming (QA-B-190).
 *
 *   The requirement reads: "AI inference shall enforce time budget
 *   (configurable, default 5s); exceeding budget triggers fallback and alert."
 *   Where each part lives today (QA-B-171, QA-B-181):
 *     configurable  "timeout_ms" is parsed in ai.cpp (xpe_ai_init) and passed
 *                   to the WorkerSupervisor, which gives it to this bridge as
 *                   the budget of one whole exchange
 *     default 5s    XPE_AI_DEFAULT_TIMEOUT_MS is 5000
 *     fallback      a timeout here is a non-OK result; ai.cpp's worker path
 *                   (xpe_bone_suppress with "use_worker": true) then returns
 *                   the INPUT unchanged with that code
 *     alert         NOT in this file (it makes no alert call): ai.cpp raises
 *                   one XPE_ALERT_WARNING per failed worker call, citing
 *                   REQ-AI-002 and REQ-AI-092
 *   What is still NOT covered: the budget exists only on that opt-in worker
 *   path. xpe_bone_suppress without "use_worker" (the default) runs
 *   OnnxSession::Run in-process with no time limit, and xpe_bodypart_recognize,
 *   xpe_stitch_images and xpe_dl_denoise are stubs with nothing to budget.
 *   So read the corrected citation as "REQ-AI-092 is satisfied for the
 *   opt-in worker path of xpe_bone_suppress", not for AI inference as a whole.
 *
 * @ingroup xpe_ai
 */

#include "ai_ipc_bridge.h"
#include "ai_finite.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================================
// Time-budgeted transfers (QA-B-171, #130, REQ-AI-092)
// ============================================================================
//
// The pipe is opened FILE_FLAG_OVERLAPPED (see connect), but send and receive
// used to call ReadFile/WriteFile with a NULL OVERLAPPED -- no time limit at
// all, and not a defined use of an overlapped handle. timeout_ms bounded only
// the connect wait. A worker that took a request and went quiet therefore held
// the caller for ever (measured: tests/test_ipc_deadline.cpp, before this
// change, a 400 ms budget was still waiting at 4000 ms).
//
// One DEADLINE is fixed when a call starts and covers every read and write in
// it, header and payload alike, so a peer that trickles bytes cannot stretch
// the budget past what was configured. On expiry the pending operation is
// cancelled and awaited before returning, so no I/O is left running against a
// buffer the caller is about to free.

namespace {

enum class Io { kOk, kTimeout, kBroken, kError };

Io Classify(DWORD error) {
    switch (error) {
        case ERROR_BROKEN_PIPE:
        case ERROR_PIPE_NOT_CONNECTED:
        case ERROR_NO_DATA:
        case ERROR_OPERATION_ABORTED:
            return Io::kBroken;
        default:
            return Io::kError;
    }
}

DWORD RemainingMs(ULONGLONG deadline) {
    const ULONGLONG now = GetTickCount64();
    if (now >= deadline) return 0;
    const ULONGLONG left = deadline - now;
    return left >= INFINITE ? (INFINITE - 1) : static_cast<DWORD>(left);
}

/** Read or write exactly @p len bytes before @p deadline (GetTickCount64 ms). */
Io TransferUntil(HANDLE h, bool read, void* buf, DWORD len, ULONGLONG deadline) {
    HANDLE ev = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!ev) return Io::kError;
    char* p = static_cast<char*>(buf);
    DWORD done = 0;
    Io result = Io::kOk;

    while (done < len) {
        OVERLAPPED ov{};
        ov.hEvent = ev;
        ResetEvent(ev);
        const BOOL ok = read ? ReadFile(h, p + done, len - done, nullptr, &ov)
                             : WriteFile(h, p + done, len - done, nullptr, &ov);
        if (!ok) {
            const DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                result = Classify(err);
                break;
            }
            const DWORD w = WaitForSingleObject(ev, RemainingMs(deadline));
            if (w != WAIT_OBJECT_0) {
                // Timeout OR a failed wait: either way the I/O is still pending
                // against this stack's OVERLAPPED and this event, and a pending
                // operation must not outlive them. Cancel it and wait for the
                // cancel to land before anything is released (Codex audit #10).
                DWORD ignored = 0;
                CancelIoEx(h, &ov);
                GetOverlappedResult(h, &ov, &ignored, TRUE);
                result = (w == WAIT_TIMEOUT) ? Io::kTimeout : Io::kError;
                break;
            }
        }
        DWORD n = 0;
        if (!GetOverlappedResult(h, &ov, &n, FALSE) && GetLastError() != ERROR_MORE_DATA) {
            result = Classify(GetLastError());
            break;
        }
        if (n == 0) {          // a zero-byte read is the peer closing
            result = Io::kBroken;
            break;
        }
        done += n;
    }
    CloseHandle(ev);
    return result;
}

/**
 * Whenever the byte stream may be at a position nobody can know, the connection
 * is dropped instead of reused: after a timeout, a half transfer, a broken pipe,
 * a frame consumed in part (wrong magic, a body the buffer cannot hold), or a
 * reply that is not the answer to this request (wrong request id, wrong type or
 * length). A late or leftover byte would otherwise be read as the NEXT reply.
 * The next call then fails loudly (not connected) and the owner reconnects -- or
 * restarts the worker.
 *
 * NOT dropped: argument validation that fails before any I/O, and a worker's
 * own well-formed XPE_AI_MSG_ERROR frame -- a complete answer, after which the
 * stream is exactly where it should be.
 */
void DropConnection(XpeAiIpcBridge* bridge) {
    if (bridge->pipe_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(bridge->pipe_handle);
        bridge->pipe_handle = INVALID_HANDLE_VALUE;
    }
    bridge->connected = false;
}

/**
 * Read `"error_code":<integer>` from a worker's ERROR frame. Accepts only a plain integer in
 * [-99, -1]: the XPE_ERR_* codes are -1 .. -17 today, and the margin keeps a future code from being
 * mistaken for garbage. Anything else (missing key, a string, 0, a positive, trailing junk in the
 * number, out of range) is rejected.
 */
bool ParseErrorCode(const std::string& body, int* out) {
    static const char kKey[] = "\"error_code\":";
    const size_t at = body.find(kKey);
    if (at == std::string::npos) return false;
    const char* p = body.c_str() + at + sizeof(kKey) - 1;
    while (*p == ' ') ++p;
    if (*p != '-') return false;   // every error code is negative
    ++p;
    if (*p < '0' || *p > '9') return false;
    long value = 0;
    for (; *p >= '0' && *p <= '9'; ++p) {
        value = value * 10 + (*p - '0');
        if (value > 99) return false;
    }
    if (value < 1) return false;
    while (*p == ' ') ++p;
    if (*p != ',' && *p != '}' && *p != '\0') return false;   // "-9.5", "-9abc": not an integer
    *out = static_cast<int>(-value);
    return true;
}

/**
 * QA-B-181j (Codex #65): is @p json a valid success envelope for a request of @p width x @p height?
 *
 * The worker's reply JSON is one flat object: {"success":true,"width":W,"height":H,"format":"float32"}.
 * This is a strict parser for exactly that shape -- an object of string keys whose values are strings,
 * non-negative integers, true, false or null; no nesting, no duplicate keys, nothing after the closing
 * brace -- followed by the semantic checks: success is the boolean true, width and height equal the request,
 * format is "float32". Anything else is false. (Keys the worker may add later are accepted if they are
 * well formed; the four named ones are required.)
 *
 * WHAT THIS DOES NOT PROVE: that the worker is healthy. A well-formed envelope on a reply whose pixels
 * are garbage is still a garbage reply that happens to be well formed; the check only stops a reply with a
 * BROKEN envelope from being taken for an answer.
 */
bool ValidSuccessEnvelope(const char* json, size_t n, uint32_t width, uint32_t height) {
    size_t i = 0;
    auto ws = [&] { while (i < n && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n')) ++i; };
    auto str = [&](std::string* out) {
        if (i >= n || json[i] != '"') return false;
        ++i;
        const size_t start = i;
        while (i < n && json[i] != '"') {
            if (json[i] == '\\' || static_cast<unsigned char>(json[i]) < 0x20) return false;   // no escapes in this protocol
            ++i;
        }
        if (i >= n) return false;
        out->assign(json + start, i - start);
        ++i;
        return true;
    };
    ws();
    if (i >= n || json[i] != '{') return false;
    ++i;
    std::map<std::string, std::string> kv;   // value kept as its raw text; strings keep a leading quote marker
    ws();
    if (i < n && json[i] == '}') {
        ++i;
    } else {
        for (;;) {
            ws();
            std::string key;
            if (!str(&key)) return false;
            ws();
            if (i >= n || json[i] != ':') return false;
            ++i;
            ws();
            std::string val;
            if (i < n && json[i] == '"') {
                std::string s;
                if (!str(&s)) return false;
                val = "\"" + s;
            } else if (i < n && json[i] >= '0' && json[i] <= '9') {
                const size_t start = i;
                while (i < n && json[i] >= '0' && json[i] <= '9') ++i;
                if (i - start > 10) return false;
                val.assign(json + start, i - start);
            } else if (n - i >= 4 && std::memcmp(json + i, "true", 4) == 0) {
                val = "true"; i += 4;
            } else if (n - i >= 5 && std::memcmp(json + i, "false", 5) == 0) {
                val = "false"; i += 5;
            } else if (n - i >= 4 && std::memcmp(json + i, "null", 4) == 0) {
                val = "null"; i += 4;
            } else {
                return false;
            }
            if (!kv.emplace(key, val).second) return false;   // duplicate key
            ws();
            if (i < n && json[i] == ',') { ++i; continue; }
            if (i < n && json[i] == '}') { ++i; break; }
            return false;
        }
    }
    ws();
    if (i != n) return false;   // trailing junk
    auto it = kv.find("success");
    if (it == kv.end() || it->second != "true") return false;
    it = kv.find("width");
    if (it == kv.end() || it->second != std::to_string(width)) return false;
    it = kv.find("height");
    if (it == kv.end() || it->second != std::to_string(height)) return false;
    it = kv.find("format");
    if (it == kv.end() || it->second != "\"float32") return false;
    return true;
}

ULONGLONG DeadlineFor(const XpeAiIpcBridge* bridge) {
    return GetTickCount64() + bridge->timeout_ms;
}

XpeErrorCode SendUntil(XpeAiIpcBridge* bridge, const XpeAiMessageHeader* header,
                       const void* payload, uint32_t payload_size, ULONGLONG deadline);
XpeErrorCode ReceiveUntil(XpeAiIpcBridge* bridge, XpeAiMessageHeader* header_out,
                          void* payload_out, uint32_t payload_size, uint32_t* bytes_received,
                          ULONGLONG deadline);

}  // namespace

// ============================================================================
// API Implementation
// ============================================================================

extern "C" {

// @MX:ANCHOR: Public API for IPC bridge creation (fan_in >= 3: connect, send, receive)
// @MX:REASON: Entry point for all IPC operations, validated by multiple callers
XpeAiIpcBridge* xpe_ai_ipc_bridge_create(const char* pipe_name, uint32_t timeout_ms) {
    // Validate input
    if (!pipe_name) {
        return nullptr;
    }

    try {
        // Allocate bridge instance
        XpeAiIpcBridge* bridge = new XpeAiIpcBridge(pipe_name, timeout_ms);
        return bridge;
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

// @MX:ANCHOR: Public API for pipe connection (fan_in >= 3: tests, send, receive)
// @MX:REASON: Establishes IPC channel, required before send/receive operations
XpeErrorCode xpe_ai_ipc_bridge_connect(XpeAiIpcBridge* bridge) {
    // Validate input
    if (!bridge) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (bridge->connected) {
        // Already connected
        return XPE_OK;
    }

    // Try to connect to the named pipe with timeout
    DWORD timeout_ms = bridge->timeout_ms;
    DWORD start_time = GetTickCount();
    bool connected = false;

    while (!connected && (GetTickCount() - start_time) < timeout_ms) {
        // Try to create file handle (connect to pipe)
        bridge->pipe_handle = CreateFileA(
            bridge->pipe_name.c_str(),   // Pipe name
            GENERIC_READ | GENERIC_WRITE, // Read/write access
            0,                            // No sharing
            NULL,                         // Default security attributes
            OPEN_EXISTING,                // Opens existing pipe
            FILE_FLAG_OVERLAPPED,         // Use overlapped I/O for async operations
            NULL                          // Default template
        );

        if (bridge->pipe_handle != INVALID_HANDLE_VALUE) {
            // Connected successfully
            connected = true;
            bridge->connected = true;
            return XPE_OK;
        }

        // Pipe not available yet, wait a bit and retry
        DWORD error = GetLastError();
        if (error == ERROR_PIPE_BUSY) {
            // Wait for pipe to become available
            WaitNamedPipeA(bridge->pipe_name.c_str(), 100);  // Wait 100ms
        } else {
            // Other error (e.g., pipe not found)
            break;
        }
    }

    // Timeout or error
    return XPE_ERR_PROCESSING_FAILED;
}

// @MX:ANCHOR: Public API for message sending (fan_in >= 3: tests, multiple inference paths)
// @MX:REASON: Validates protocol and writes to pipe, critical for IPC communication
XpeErrorCode xpe_ai_ipc_bridge_send(XpeAiIpcBridge* bridge,
                                             const XpeAiMessageHeader* header,
                                             const void* payload,
                                             uint32_t payload_size) {
    if (!bridge) {
        return XPE_ERR_INVALID_INPUT;
    }
    return SendUntil(bridge, header, payload, payload_size, DeadlineFor(bridge));
}

// @MX:ANCHOR: Public API for message receiving (fan_in >= 3: tests, multiple inference paths)
// @MX:REASON: Reads and validates protocol from pipe, critical for IPC communication
XpeErrorCode xpe_ai_ipc_bridge_receive(XpeAiIpcBridge* bridge,
                                                XpeAiMessageHeader* header_out,
                                                void* payload_out,
                                                uint32_t payload_size,
                                                uint32_t* bytes_received) {
    if (!bridge) {
        return XPE_ERR_INVALID_INPUT;
    }
    return ReceiveUntil(bridge, header_out, payload_out, payload_size, bytes_received,
                        DeadlineFor(bridge));
}

}  // extern "C"

// ============================================================================
// Internal send/receive with an explicit deadline
// ============================================================================
//
// Result codes keep the contract the callers already had:
//   no connection        send NOT_INITIALIZED, receive PROCESSING_FAILED
//   time budget exceeded PROCESSING_FAILED (the documented fallback signal,
//                        REQ-AI-002, now with REQ-AI-092's budget behind it)
//   peer closed (header) PROCESSING_FAILED   peer closed (payload/send) IO_FAILED
// After any failure past argument validation the connection is dropped, except
// when the worker answered with a complete, well-formed ERROR frame.

namespace {

XpeErrorCode SendUntil(XpeAiIpcBridge* bridge, const XpeAiMessageHeader* header,
                       const void* payload, uint32_t payload_size, ULONGLONG deadline) {
    // Validate input
    if (!bridge || !header) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Validate protocol fields
    if (header->magic != XPE_AI_MSG_MAGIC) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (header->payloadSize > XPE_AI_MAX_PAYLOAD_SIZE ||
        payload_size > XPE_AI_MAX_PAYLOAD_SIZE) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (header->payloadSize != payload_size) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (payload_size > 0 && payload == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Check connection state after protocol validation so callers get
    // deterministic validation errors even before a worker is connected.
    if (!bridge->connected || bridge->pipe_handle == INVALID_HANDLE_VALUE) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    Io io = TransferUntil(bridge->pipe_handle, false, const_cast<XpeAiMessageHeader*>(header),
                          sizeof(XpeAiMessageHeader), deadline);
    if (io == Io::kOk && payload_size > 0) {
        io = TransferUntil(bridge->pipe_handle, false, const_cast<void*>(payload), payload_size,
                           deadline);
    }
    if (io != Io::kOk) {
        DropConnection(bridge);
        return io == Io::kTimeout ? XPE_ERR_PROCESSING_FAILED : XPE_ERR_IO_FAILED;
    }
    return XPE_OK;
}

XpeErrorCode ReceiveUntil(XpeAiIpcBridge* bridge, XpeAiMessageHeader* header_out,
                          void* payload_out, uint32_t payload_size, uint32_t* bytes_received,
                          ULONGLONG deadline) {
    // Validate input
    if (!bridge || !header_out || !bytes_received) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Initialize output
    *bytes_received = 0;
    std::memset(header_out, 0, sizeof(XpeAiMessageHeader));

    // A receive call with no worker connection is treated as a timeout-like
    // processing failure by the IPC contract.
    if (!bridge->connected || bridge->pipe_handle == INVALID_HANDLE_VALUE) {
        return XPE_ERR_PROCESSING_FAILED;
    }

    Io io = TransferUntil(bridge->pipe_handle, true, header_out, sizeof(XpeAiMessageHeader),
                          deadline);
    if (io != Io::kOk) {
        DropConnection(bridge);
        return io == Io::kError ? XPE_ERR_IO_FAILED : XPE_ERR_PROCESSING_FAILED;
    }
    *bytes_received += static_cast<uint32_t>(sizeof(XpeAiMessageHeader));

    // From here a frame has been (partly) consumed. Any path that does not read
    // it to its end, or reads one that is not a valid frame, leaves the stream
    // at a position nobody can know -- the next receive would take leftover body
    // bytes for a header -- so each of them drops the connection (Codex audit #10).

    // Validate received header
    if (header_out->magic != XPE_AI_MSG_MAGIC) {
        DropConnection(bridge);
        return XPE_ERR_INVALID_INPUT;
    }

    // Read payload (if any)
    if (header_out->payloadSize > 0) {
        if (!payload_out || payload_size < header_out->payloadSize) {
            DropConnection(bridge);   // the body is still in the pipe
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        io = TransferUntil(bridge->pipe_handle, true, payload_out, header_out->payloadSize,
                           deadline);
        if (io != Io::kOk) {
            DropConnection(bridge);
            return io == Io::kTimeout ? XPE_ERR_PROCESSING_FAILED : XPE_ERR_IO_FAILED;
        }
        *bytes_received += header_out->payloadSize;
    }

    return XPE_OK;
}

}  // namespace

extern "C" {

// QA-B-170 (#130): the client half of XPE_AI_MSG_BONE_SUPPRESS.
//
// One request, one response, blocking. The wire layout (length-prefixed JSON,
// then raw float32 pixels) is documented in ai_worker_protocol.h.
//
// STILL NO PRODUCT CALLER. The header comment above says nothing outside this
// file calls the bridge, and that remains true: this function is exercised by
// tests/test_worker_protocol_conformance.cpp only. Routing xpe_bone_suppress
// through it is QA-B-171.
//
// A worker ERROR frame is passed through: the returned code is the worker's
// own error_code, so a caller sees the same XPE_ERR_* the in-process
// xpe_bone_suppress would have returned for the same fault. Transport faults
// (no connection, unreadable frame, mismatched reply) are IO_FAILED or
// PROCESSING_FAILED and cannot be confused with a model fault by number alone.
XpeErrorCode xpe_ai_ipc_bridge_bone_suppress(XpeAiIpcBridge* bridge,
                                             uint32_t width,
                                             uint32_t height,
                                             const float* pixels_in,
                                             float* pixels_out) {
    if (!bridge || !pixels_in || !pixels_out || width == 0 || height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    bridge->last_result_nonfinite = false;
    const uint64_t count = static_cast<uint64_t>(width) * height;
    // 4 (length prefix) + metadata + pixels must fit one protocol payload.
    if (count > (XPE_AI_MAX_PAYLOAD_SIZE - 512u) / sizeof(float)) {
        return XPE_ERR_INVALID_INPUT;
    }
    const size_t pixel_bytes = static_cast<size_t>(count) * sizeof(float);

    char json[96];
    const int json_len = std::snprintf(
        json, sizeof(json), "{\"width\":%u,\"height\":%u,\"format\":\"float32\"}",
        static_cast<unsigned>(width), static_cast<unsigned>(height));
    if (json_len <= 0 || static_cast<size_t>(json_len) >= sizeof(json)) {
        return XPE_ERR_PROCESSING_FAILED;
    }

    std::vector<uint8_t> payload(sizeof(uint32_t) + static_cast<size_t>(json_len) + pixel_bytes);
    const uint32_t json_size = static_cast<uint32_t>(json_len);
    std::memcpy(payload.data(), &json_size, sizeof(json_size));
    std::memcpy(payload.data() + sizeof(json_size), json, json_size);
    std::memcpy(payload.data() + sizeof(json_size) + json_size, pixels_in, pixel_bytes);

    static std::atomic<uint32_t> next_request_id{1};
    XpeAiMessageHeader header{};
    header.magic = XPE_AI_MSG_MAGIC;
    header.version = (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
                     static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
    header.messageType = XPE_AI_MSG_BONE_SUPPRESS;
    header.requestId = next_request_id.fetch_add(1);
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.flags = XPE_AI_FLAG_HAS_BINARY_PAYLOAD;
    header.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    // ONE time budget (REQ-AI-092) for the whole exchange: the request write,
    // the worker's inference and the reply read. Splitting it per call would
    // let a slow-but-answering worker spend the budget twice.
    const ULONGLONG deadline = DeadlineFor(bridge);
    XpeErrorCode rc = SendUntil(bridge, &header, payload.data(),
                                static_cast<uint32_t>(payload.size()), deadline);
    if (rc != XPE_OK) {
        return rc;
    }

    // A response is at most the same size as the request; an error frame is
    // small. receive() reads the payload into this buffer and reports
    // BUFFER_TOO_SMALL if the worker sends more -- after the header is already
    // consumed, so that case leaves the stream unusable and is not retried.
    std::vector<uint8_t> reply(sizeof(uint32_t) + 512u + pixel_bytes);
    XpeAiMessageHeader rh{};
    uint32_t received = 0;
    rc = ReceiveUntil(bridge, &rh, reply.data(), static_cast<uint32_t>(reply.size()),
                      &received, deadline);
    if (rc != XPE_OK) {
        return rc;
    }
    if (rh.requestId != header.requestId) {
        DropConnection(bridge);   // someone else's answer: the stream is out of step
        return XPE_ERR_IO_FAILED;
    }

    if (rh.messageType == XPE_AI_MSG_ERROR) {
        // The worker's own code, verbatim -- but only a code that parses as a number in the range
        // the XPE_ERR_* codes occupy. An ERROR frame with no code, a code that is not a number,
        // zero (success) or a value outside that range is a worker speaking garbage: the answer is
        // a failure of unknown kind (never a success) and the connection is dropped, because
        // nothing else this worker says can be trusted either (Codex audit #11).
        const std::string body(reinterpret_cast<const char*>(reply.data()), rh.payloadSize);
        int code = 0;
        if (!ParseErrorCode(body, &code)) {
            DropConnection(bridge);
            return XPE_ERR_PROCESSING_FAILED;
        }
        return static_cast<XpeErrorCode>(code);
    }

    if (rh.messageType != XPE_AI_MSG_BONE_SUPPRESS_RESP ||
        (rh.flags & XPE_AI_FLAG_HAS_BINARY_PAYLOAD) == 0 ||
        rh.payloadSize < sizeof(uint32_t)) {
        DropConnection(bridge);   // not the reply this request can get
        return XPE_ERR_IO_FAILED;
    }
    uint32_t reply_json = 0;
    std::memcpy(&reply_json, reply.data(), sizeof(reply_json));
    if (static_cast<uint64_t>(sizeof(uint32_t)) + reply_json + pixel_bytes != rh.payloadSize) {
        DropConnection(bridge);   // pixel count does not match what was sent
        return XPE_ERR_IO_FAILED;
    }
    // QA-B-181j: the envelope is judged BEFORE the pixels. A reply whose JSON is missing, unparseable, not
    // a success, or about another size or format is a protocol fault -- the connection is dropped and the
    // call is XPE_ERR_IO_FAILED, which the product counts -- whatever its pixels hold. Only a valid success
    // envelope can reach the non-finite classification below, so garbage cannot borrow its exemption.
    if (!ValidSuccessEnvelope(reinterpret_cast<const char*>(reply.data()) + sizeof(uint32_t), reply_json,
                              width, height)) {
        DropConnection(bridge);
        return XPE_ERR_IO_FAILED;
    }
    // QA-B-181h: the bridge, not the worker, is where a non-finite result is refused. The worker is a
    // separate process whose reply this side cannot vouch for, and this check also holds for any worker
    // that answers success with such pixels (a model without the check, a fake one). The judgment comes
    // BEFORE the copy, so a refusal leaves pixels_out untouched. The connection stays up: the frame was
    // well formed, the pixels are what the model computed.
    const uint8_t* reply_pixels = reply.data() + sizeof(uint32_t) + reply_json;
    // The answer is XPE_ERR_PROCESSING_FAILED (the request was valid and finite; the model could not give a
    // result), and last_result_nonfinite tells the caller this is NOT a worker or transport fault (QA-B-181i,
    // Codex #63): the worker answered correctly, so it must not count toward the failure ceiling.
    if (!xpe::ai::AllFinite(reply_pixels, count)) {
        bridge->last_result_nonfinite = true;
        return XPE_ERR_PROCESSING_FAILED;
    }
    std::memcpy(pixels_out, reply_pixels, pixel_bytes);
    return XPE_OK;
}

void xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge) {
    if (!bridge) {
        return;
    }

    // Close pipe handle if open
    if (bridge->pipe_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(bridge->pipe_handle);
        bridge->pipe_handle = INVALID_HANDLE_VALUE;
    }

    // Free bridge instance
    delete bridge;
}

} // extern "C"
