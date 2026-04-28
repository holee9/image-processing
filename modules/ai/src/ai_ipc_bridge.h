/**
 * @file ai_ipc_bridge.h
 * @brief Internal IPC bridge implementation for XPE AI named pipe communication.
 *
 * Implements the client-side of the named pipe IPC between xpe_ai.dll
 * (in-process proxy) and xpe_ai_worker.exe (sandboxed worker process).
 *
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-009: Time budget enforcement (inference timeout).
 *
 * @ingroup xpe_ai
 */

#ifndef XPE_AI_IPC_BRIDGE_H
#define XPE_AI_IPC_BRIDGE_H

#include <xpe/ai/ai_worker_protocol.h>
#include <xpe/common/xpe_error.h>
#include <string>
#include <windows.h>

/**
 * @brief Internal IPC bridge state.
 */
struct XpeAiIpcBridge {
    std::string pipe_name;      /**< Named pipe name (e.g., "\\\\.\\pipe\\xpe_ai_worker_12345") */
    uint32_t timeout_ms;        /**< Timeout in milliseconds for operations */
    HANDLE pipe_handle;         /**< Win32 handle to named pipe */
    bool connected;             /**< Connection state flag */

    XpeAiIpcBridge(const std::string& name, uint32_t timeout)
        : pipe_name(name), timeout_ms(timeout), pipe_handle(INVALID_HANDLE_VALUE),
          connected(false) {}
};

#endif /* XPE_AI_IPC_BRIDGE_H */
