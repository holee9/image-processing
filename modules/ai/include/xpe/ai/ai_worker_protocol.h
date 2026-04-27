/**
 * @file ai_worker_protocol.h
 * @brief IPC protocol between xpe_ai.dll (proxy) and xpe_ai_worker.exe.
 *
 * Defines the message envelope, request/response types, and wire format
 * for communication over named pipes between the in-process DLL proxy
 * and the sandboxed AI worker process.
 *
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-004: Sidecar metadata delivery (not mutating XpeImageMetadata).
 * REQ-AI-009: Time budget enforcement (inference timeout).
 *
 * Pipe naming convention:
 *   Windows: \\.\pipe\xpe_ai_worker_{PID}
 *   The PID suffix ensures uniqueness when multiple XPE host processes run.
 *
 * Wire format:
 *   All messages are little-endian. The envelope is fixed-size (32 bytes)
 *   followed by a variable-length JSON payload.
 *
 * @ingroup xpe_ai
 */
#ifndef XPE_AI_WORKER_PROTOCOL_H
#define XPE_AI_WORKER_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Protocol Version
 * -------------------------------------------------------------------------- */

/** Major.minor protocol version for DLL-worker compatibility check. */
#define XPE_AI_PROTOCOL_VERSION_MAJOR  1
#define XPE_AI_PROTOCOL_VERSION_MINOR  0

/* ==========================================================================
 * IPC Constants
 * -------------------------------------------------------------------------- */

/** Maximum payload size per message (64 MB = max image buffer). */
#define XPE_AI_MAX_PAYLOAD_SIZE    (64 * 1024 * 1024)

/** Default IPC timeout in milliseconds. */
#define XPE_AI_DEFAULT_TIMEOUT_MS  5000

/** Default confidence threshold for fallback routing. */
#define XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD  0.6f

/** Named pipe buffer size (64 KB). */
#define XPE_AI_PIPE_BUFFER_SIZE    65536

/** Maximum model ID length. */
#define XPE_AI_MAX_MODEL_ID_LEN    128

/** Maximum body-part label length. */
#define XPE_AI_MAX_BODYPART_LEN    64

/* ==========================================================================
 * Message Envelope (fixed 32-byte header)
 * -------------------------------------------------------------------------- */

#pragma pack(push, 8)

/**
 * @brief Fixed-size message envelope preceding every IPC payload.
 *
 * The worker reads this header first to determine payload type and size,
 * then reads exactly @c payloadSize bytes of JSON or binary data.
 */
typedef struct XpeAiMessageHeader {
    uint32_t magic;           /**< Magic number: 0x58504541 ("XPEA") */
    uint32_t version;         /**< Protocol version (major << 16 | minor) */
    uint32_t messageType;     /**< XpeAiMessageType enum value */
    uint32_t requestId;       /**< Monotonically increasing request counter */
    uint32_t payloadSize;     /**< Size of payload following this header */
    uint32_t flags;           /**< Message flags (reserved, set to 0) */
    uint64_t timestamp;       /**< POSIX timestamp (milliseconds since epoch) */
    uint8_t  reserved[8];     /**< Reserved for future use, must be zero */
} XpeAiMessageHeader;

#pragma pack(pop)

/** Magic number for protocol validation. */
#define XPE_AI_MSG_MAGIC  0x58504541u

/* ==========================================================================
 * Message Types
 * -------------------------------------------------------------------------- */

/**
 * @brief IPC message type discriminator.
 */
typedef enum XpeAiMessageType {
    /* --- Lifecycle --- */
    XPE_AI_MSG_INIT            = 1,   /**< Worker: initialize ONNX runtime */
    XPE_AI_MSG_INIT_RESPONSE   = 2,   /**< Worker: init result */
    XPE_AI_MSG_SHUTDOWN        = 3,   /**< Worker: graceful shutdown */
    XPE_AI_MSG_HEARTBEAT       = 4,   /**< Worker: heartbeat ping */
    XPE_AI_MSG_HEARTBEAT_ACK   = 5,   /**< DLL: heartbeat acknowledgement */

    /* --- Inference --- */
    XPE_AI_MSG_BODYPART_RECOGNIZE       = 10,  /**< Body part classification */
    XPE_AI_MSG_BODYPART_RECOGNIZE_RESP  = 11,
    XPE_AI_MSG_STITCH_IMAGES            = 12,  /**< Image stitching */
    XPE_AI_MSG_STITCH_IMAGES_RESP       = 13,
    XPE_AI_MSG_BONE_SUPPRESS            = 14,  /**< Bone suppression */
    XPE_AI_MSG_BONE_SUPPRESS_RESP       = 15,
    XPE_AI_MSG_DL_DENOISE               = 16,  /**< DL denoising */
    XPE_AI_MSG_DL_DENOISE_RESP          = 17,

    /* --- Model Management --- */
    XPE_AI_MSG_GET_MODEL_CARD           = 20,  /**< Model card query */
    XPE_AI_MSG_GET_MODEL_CARD_RESP      = 21,
    XPE_AI_MSG_LIST_MODELS              = 22,  /**< List loaded models */
    XPE_AI_MSG_LIST_MODELS_RESP         = 23,

    /* --- Error --- */
    XPE_AI_MSG_ERROR                    = 99   /**< Generic error response */
} XpeAiMessageType;

/* ==========================================================================
 * Message Flags
 * -------------------------------------------------------------------------- */

/** Flag: this message carries binary (image) payload after JSON payload. */
#define XPE_AI_FLAG_HAS_BINARY_PAYLOAD  0x00000001u

/** Flag: inference timed out; worker sends partial result. */
#define XPE_AI_FLAG_TIMEOUT             0x00000002u

/** Flag: low confidence result; caller should use fallback. */
#define XPE_AI_FLAG_LOW_CONFIDENCE      0x00000004u

/** Flag: fallback mode is active; worker skips retries. */
#define XPE_AI_FLAG_FALLBACK_MODE       0x00000008u

/* ==========================================================================
 * Worker State Codes
 * -------------------------------------------------------------------------- */

/**
 * @brief Worker process state reported in heartbeat responses.
 */
typedef enum XpeAiWorkerState {
    XPE_AI_WORKER_IDLE        = 0,  /**< Worker is ready for requests */
    XPE_AI_WORKER_BUSY        = 1,  /**< Worker is processing a request */
    XPE_AI_WORKER_LOADING     = 2,  /**< Worker is loading a model */
    XPE_AI_WORKER_ERROR       = 3,  /**< Worker encountered an error */
    XPE_AI_WORKER_SHUTTING_DOWN = 4 /**< Worker is shutting down */
} XpeAiWorkerState;

/* ==========================================================================
 * Execution Provider Types (REQ-AI-006)
 * -------------------------------------------------------------------------- */

/**
 * @brief ONNX Runtime Execution Provider selector.
 */
typedef enum XpeAiExecutionProvider {
    XPE_AI_EP_CPU       = 0,  /**< CPU (always available) */
    XPE_AI_EP_CUDA      = 1,  /**< NVIDIA CUDA (x86 GPU) */
    XPE_AI_EP_TENSORRT  = 2,  /**< NVIDIA TensorRT */
    XPE_AI_EP_DIRECTML  = 3,  /**< DirectML (Windows GPU) */
    XPE_AI_EP_AUTO      = 4   /**< Auto-select best available EP */
} XpeAiExecutionProvider;

/* ==========================================================================
 * JSON Payload Schemas (documented, not enforced at compile time)
 * -------------------------------------------------------------------------- */

/**
 * Init request payload (JSON):
 * {
 *   "model_dir": "C:\\data\\models\\",
 *   "execution_provider": "cpu",       // cpu|cuda|tensorrt|directml|auto
 *   "timeout_ms": 5000,
 *   "confidence_threshold": 0.6,
 *   "fallback_mode": true
 * }
 *
 * Init response payload (JSON):
 * {
 *   "success": true,
 *   "loaded_models": ["bodypart_v1", "bone_suppress_v1", ...],
 *   "execution_provider": "cpu",
 *   "worker_pid": 12345
 * }
 *
 * Bodypart recognize response payload (JSON):
 * {
 *   "body_part": "CHEST",
 *   "confidence": 0.95,
 *   "model_id": "bodypart_cnn_v1",
 *   "inference_ms": 42,
 *   "sidecar": { ... }               // REQ-AI-004: sidecar metadata
 * }
 *
 * Error response payload (JSON):
 * {
 *   "error_code": -3,
 *   "error_message": "ONNX session creation failed",
 *   "fallback_recommended": true
 * }
 *
 * Model card response payload (JSON, REQ-AI-010/011):
 * {
 *   "model_id": "bone_suppress_v1",
 *   "model_version": "1.0.0",
 *   "intended_use": "...",
 *   "training_data_summary": "...",
 *   "demographic_performance": { ... },
 *   "limitations": "...",
 *   "pccp_status": "within_boundary",
 *   "published_date": "2026-04-01",
 *   "training_data_hash": "sha256:...",
 *   "validation_metrics": { "psnr": 42.1, "ssim": 0.98 }
 * }
 */

#ifdef __cplusplus
}
#endif

#endif /* XPE_AI_WORKER_PROTOCOL_H */
