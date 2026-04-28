/**
 * @file ai_ipc_bridge.cpp
 * @brief IPC bridge implementation for XPE AI named pipe communication.
 *
 * Implements the client-side of the named pipe IPC between xpe_ai.dll
 * (in-process proxy) and xpe_ai_worker.exe (sandboxed worker process).
 *
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-009: Time budget enforcement (inference timeout).
 *
 * @ingroup xpe_ai
 */

#include "ai_ipc_bridge.h"
#include <cstring>
#include <stdexcept>

// ============================================================================
// API Implementation
// ============================================================================

extern "C" {

// @MX:ANCHOR: Public API for IPC bridge creation (fan_in >= 3: connect, send, receive)
// @MX:REASON: Entry point for all IPC operations, validated by multiple callers
XPE_API XpeAiIpcBridge* xpe_ai_ipc_bridge_create(const char* pipe_name, uint32_t timeout_ms) {
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
XPE_API XpeErrorCode xpe_ai_ipc_bridge_connect(XpeAiIpcBridge* bridge) {
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
XPE_API XpeErrorCode xpe_ai_ipc_bridge_send(XpeAiIpcBridge* bridge,
                                             const XpeAiMessageHeader* header,
                                             const void* payload,
                                             uint32_t payload_size) {
    // Validate input
    if (!bridge || !header) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Check connection state
    if (!bridge->connected || bridge->pipe_handle == INVALID_HANDLE_VALUE) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    // Validate protocol fields
    if (header->magic != XPE_AI_MSG_MAGIC) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (header->payloadSize != payload_size) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (payload_size > XPE_AI_MAX_PAYLOAD_SIZE) {
        return XPE_ERR_INVALID_INPUT;
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
XPE_API XpeErrorCode xpe_ai_ipc_bridge_receive(XpeAiIpcBridge* bridge,
                                                XpeAiMessageHeader* header_out,
                                                void* payload_out,
                                                uint32_t payload_size,
                                                uint32_t* bytes_received) {
    // Validate input
    if (!bridge || !header_out || !bytes_received) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Check connection state
    if (!bridge->connected || bridge->pipe_handle == INVALID_HANDLE_VALUE) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    // Initialize output
    *bytes_received = 0;
    std::memset(header_out, 0, sizeof(XpeAiMessageHeader));

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

XPE_API void xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge) {
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
