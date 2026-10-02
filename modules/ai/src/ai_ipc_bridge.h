/**
 * @file ai_ipc_bridge.h
 * @brief Internal IPC bridge implementation for XPE AI named pipe communication.
 *
 * Implements the client-side of the named pipe IPC between xpe_ai.dll
 * (in-process proxy) and xpe_ai_worker.exe (sandboxed worker process).
 *
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-092: Time budget enforcement (inference timeout).
 *   Was AI requirement 009, which SPEC-XPE-P3-AI never defined (#210,
 *   QA-B-157, QA-B-190).
 *   This file carries the per-exchange budget. The fallback and the alert are
 *   ai.cpp's, on the opt-in worker path of xpe_bone_suppress only -- the
 *   accounting of what is and is not covered is in ai_ipc_bridge.cpp.
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
    /** Set by xpe_ai_ipc_bridge_bone_suppress: the LAST call was refused because the worker's reply was well formed
     *  but held a non-finite pixel (QA-B-181i). Reset at the start of every call. Not a transport fault. */
    bool last_result_nonfinite = false;

    XpeAiIpcBridge(const std::string& name, uint32_t timeout)
        : pipe_name(name), timeout_ms(timeout), pipe_handle(INVALID_HANDLE_VALUE),
          connected(false) {}
};

#endif /* XPE_AI_IPC_BRIDGE_H */
