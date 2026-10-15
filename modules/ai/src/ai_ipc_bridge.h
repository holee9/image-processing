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
#include "ai_bodypart_decision.h"
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
    /** Set by xpe_ai_ipc_bridge_bodypart and (QA-B-195 D6) xpe_ai_ipc_bridge_bone_suppress: the LAST call got a worker ERROR frame that says the MODEL cannot be used
     *  (QA-B-191 M4b): a state of the installation, not a fault of the worker or the transport. Reset every call. */
    bool last_model_unavailable = false;

    XpeAiIpcBridge(const std::string& name, uint32_t timeout)
        : pipe_name(name), timeout_ms(timeout), pipe_handle(INVALID_HANDLE_VALUE),
          connected(false) {}
};

using XpeAiBodyPartReply = xpe::ai::BodyPartReply;

/**
 * Client half of XPE_AI_MSG_BODYPART_RECOGNIZE (QA-B-191 M4b): send a float32 image, read what the model said.
 * One time budget for the whole exchange (REQ-AI-092). XPE_OK means a VALID reply arrived and *reply is set
 * (including a refusal such as non-finite output -- the worker answered correctly). A worker ERROR frame returns
 * the worker's own code, and bridge->last_model_unavailable says whether it meant "the model cannot be used".
 * A reply that is not valid (wrong type, bad JSON, a label or confidence the protocol forbids) drops the
 * connection and returns XPE_ERR_IO_FAILED.
 */
extern "C" XpeErrorCode xpe_ai_ipc_bridge_bodypart(XpeAiIpcBridge* bridge, uint32_t width, uint32_t height,
                                                   const float* pixels_in, XpeAiBodyPartReply* reply);

#endif /* XPE_AI_IPC_BRIDGE_H */
