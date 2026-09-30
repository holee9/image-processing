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
 *   That number is not defined in SPEC-XPE-P3-AI at all -- 0 definitions among
 *   its 46, while every other REQ-AI number this module cites has exactly one.
 *
 *   REQ-AI-092 is the right requirement for this subject, but this file
 *   implements only PART of it. The requirement reads: "AI inference shall
 *   enforce time budget (configurable, default 5s); exceeding budget triggers
 *   fallback and alert."
 *     configurable  YES -- "timeout_ms" is parsed in ai.cpp
 *     default 5s    YES -- XPE_AI_DEFAULT_TIMEOUT_MS is 5000
 *     fallback      YES -- a timeout here returns XPE_ERR_PROCESSING_FAILED
 *     alert         NO  -- this file makes no alert call (count: 0), while
 *                          xpe_common's alert API exists and is used elsewhere
 *   And the budget does not run in a shipped build at all: nothing outside
 *   this file calls xpe_ai_ipc_bridge_create (callers in src/: 0), ai.cpp
 *   never references the bridge, and the timeout it parses into
 *   state->timeoutMs is only logged, never passed here. The inference entry
 *   points return the stub before any IPC (#130).
 *
 *   The number is corrected because it pointed at nothing. Do not read the
 *   corrected citation as "REQ-AI-092 is satisfied".
 *
 * @ingroup xpe_ai
 */

#include "ai_ipc_bridge.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

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

    // Write header
    DWORD bytes_written = 0;
    BOOL success = WriteFile(
        bridge->pipe_handle,
        header,
        sizeof(XpeAiMessageHeader),
        &bytes_written,
        NULL
    );

    if (!success || bytes_written != sizeof(XpeAiMessageHeader)) {
        return XPE_ERR_IO_FAILED;
    }

    // Write payload (if any)
    if (payload_size > 0 && payload) {
        bytes_written = 0;
        success = WriteFile(
            bridge->pipe_handle,
            payload,
            payload_size,
            &bytes_written,
            NULL
        );

        if (!success || bytes_written != payload_size) {
            return XPE_ERR_IO_FAILED;
        }
    }

    return XPE_OK;
}

// @MX:ANCHOR: Public API for message receiving (fan_in >= 3: tests, multiple inference paths)
// @MX:REASON: Reads and validates protocol from pipe, critical for IPC communication
XpeErrorCode xpe_ai_ipc_bridge_receive(XpeAiIpcBridge* bridge,
                                                XpeAiMessageHeader* header_out,
                                                void* payload_out,
                                                uint32_t payload_size,
                                                uint32_t* bytes_received) {
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

    // Read header with timeout
    DWORD bytes_read = 0;
    BOOL success = ReadFile(
        bridge->pipe_handle,
        header_out,
        sizeof(XpeAiMessageHeader),
        &bytes_read,
        NULL
    );

    if (!success) {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT || error == ERROR_BROKEN_PIPE) {
            return XPE_ERR_PROCESSING_FAILED;  // Timeout
        }
        return XPE_ERR_IO_FAILED;
    }

    if (bytes_read != sizeof(XpeAiMessageHeader)) {
        return XPE_ERR_IO_FAILED;
    }

    *bytes_received += static_cast<uint32_t>(bytes_read);

    // Validate received header
    if (header_out->magic != XPE_AI_MSG_MAGIC) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Read payload (if any)
    if (header_out->payloadSize > 0) {
        if (!payload_out || payload_size < header_out->payloadSize) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        bytes_read = 0;
        success = ReadFile(
            bridge->pipe_handle,
            payload_out,
            header_out->payloadSize,
            &bytes_read,
            NULL
        );

        if (!success) {
            return XPE_ERR_IO_FAILED;
        }

        if (bytes_read != header_out->payloadSize) {
            return XPE_ERR_IO_FAILED;
        }

        *bytes_received += static_cast<uint32_t>(bytes_read);
    }

    return XPE_OK;
}

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

    XpeErrorCode rc = xpe_ai_ipc_bridge_send(bridge, &header, payload.data(),
                                             static_cast<uint32_t>(payload.size()));
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
    rc = xpe_ai_ipc_bridge_receive(bridge, &rh, reply.data(),
                                   static_cast<uint32_t>(reply.size()), &received);
    if (rc != XPE_OK) {
        return rc;
    }
    if (rh.requestId != header.requestId) {
        return XPE_ERR_IO_FAILED;   // someone else's answer
    }

    if (rh.messageType == XPE_AI_MSG_ERROR) {
        // The worker's own code, verbatim. A frame without a usable code is a
        // failure of unknown kind, never a success.
        const std::string body(reinterpret_cast<const char*>(reply.data()), rh.payloadSize);
        const size_t at = body.find("\"error_code\":");
        if (at == std::string::npos) {
            return XPE_ERR_PROCESSING_FAILED;
        }
        const int code = std::atoi(body.c_str() + at + std::strlen("\"error_code\":"));
        return code == XPE_OK ? XPE_ERR_PROCESSING_FAILED : static_cast<XpeErrorCode>(code);
    }

    if (rh.messageType != XPE_AI_MSG_BONE_SUPPRESS_RESP ||
        (rh.flags & XPE_AI_FLAG_HAS_BINARY_PAYLOAD) == 0 ||
        rh.payloadSize < sizeof(uint32_t)) {
        return XPE_ERR_IO_FAILED;
    }
    uint32_t reply_json = 0;
    std::memcpy(&reply_json, reply.data(), sizeof(reply_json));
    if (static_cast<uint64_t>(sizeof(uint32_t)) + reply_json + pixel_bytes != rh.payloadSize) {
        return XPE_ERR_IO_FAILED;   // pixel count does not match what was sent
    }
    std::memcpy(pixels_out, reply.data() + sizeof(uint32_t) + reply_json, pixel_bytes);
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
