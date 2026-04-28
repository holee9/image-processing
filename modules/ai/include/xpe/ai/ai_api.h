/**
 * @file ai_api.h
 * @brief xpe_ai.dll public C API -- deep-learning inference proxy.
 *
 * Provides body-part recognition, image stitching, bone suppression,
 * DL-based denoising, model card transparency, and deterministic fallback
 * routing. Actual inference runs in a sandboxed worker process
 * (xpe_ai_worker.exe) over IPC (named pipe).
 *
 * Dependency: xpe_common.dll only (Layer 1, no lateral DLL deps).
 * Execution model: xpe_ai.dll is an in-process C ABI proxy. The worker
 * process provides crash isolation for GPU/native ONNX Runtime calls.
 *
 * REQ-AI-001 through REQ-AI-012 (SPEC-XPE-P3-AI v1.1).
 * API contract: docs/project/api-spec.md Section 9 (7 exported functions).
 *
 * @ingroup xpe_ai
 */
#ifndef XPE_AI_API_H
#define XPE_AI_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * xpe_ai.dll -- Exported API (7 functions + 2 extended)
 *
 * REQ-AI-001: Layer 1 dependency (xpe_common only)
 * REQ-AI-002: Deterministic fallback for all AI functions
 * REQ-AI-003: Worker-isolated architecture (IPC)
 * REQ-AI-005: Opt-in activation (default off)
 * REQ-AI-006: ONNX Runtime 1.20+ multi-EP
 * -------------------------------------------------------------------------- */

/**
 * @brief Returns the xpe_ai module version string.
 *
 * Format: "X.Y.Z". DLL-owned static storage; do NOT free.
 *
 * @return Non-NULL version string. Thread-safe.
 */
XPE_API const char* xpe_ai_version(void);

/**
 * @brief Initialises the AI inference subsystem.
 *
 * Launches or attaches to the sandboxed AI worker process, loads ONNX models
 * from @p modelDirPath, and initialises the inference runtime. Must be called
 * before any other xpe_ai function.
 *
 * @p configJsonOrNull selects device (CPU/CUDA/TensorRT/DirectML), IPC timeout,
 * batch settings, and confidence thresholds. Pass NULL for defaults (CPU EP,
 * 5 s timeout, 0.6 confidence threshold).
 *
 * REQ-AI-001: Only xpe_common dependency.
 * REQ-AI-003: Worker process isolation.
 * REQ-AI-006: ONNX Runtime 1.20+ integration.
 *
 * @param modelDirPath      Directory containing signed .onnx model files.
 *                          Must not be NULL.
 * @param configJsonOrNull  UTF-8 JSON configuration, or NULL for defaults.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if modelDirPath is NULL.
 * @return XPE_ERR_IO_FAILED if worker process cannot be launched.
 * @return XPE_ERR_CONFIG_INVALID if config JSON is malformed.
 * @return XPE_ERR_OUT_OF_MEMORY if IPC resources cannot be allocated.
 *
 * Thread safety: Not thread-safe; call from single thread at startup.
 * SRS: SRS-AI-001, SRS-AI-002
 */
XPE_API XpeErrorCode xpe_ai_init(const char* modelDirPath,
                                  const char* configJsonOrNull);

/**
 * @brief Shuts down the AI inference subsystem.
 *
 * Stops the sandboxed worker process, unloads all models, and releases IPC
 * resources. No xpe_ai function may be called after this returns.
 *
 * REQ-AI-003: IPC cleanup.
 *
 * Thread safety: Not thread-safe; call from single thread at shutdown.
 * SRS: SRS-AI-003
 */
XPE_API void xpe_ai_shutdown(void);

/**
 * @brief Classifies the anatomical body part in an image using CNN.
 *
 * Writes a body-part label (e.g., "CHEST", "HAND") to @p bodyPartOut and a
 * confidence score [0,1] to @p confidenceOut.
 *
 * REQ-AI-002: If AI fails or confidence < threshold, returns
 *             XPE_ERR_PROCESSING_FAILED. Caller should use deterministic
 *             body-part lookup as fallback.
 *
 * @param img            Input image. Must not be NULL.
 * @param bodyPartOut    Caller-allocated buffer for the label string.
 *                       Must not be NULL.
 * @param bufLen         Size of @p bodyPartOut in bytes. Recommended >= 64.
 * @param confidenceOut  Output: confidence score [0, 1]. May be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or bodyPartOut is NULL.
 * @return XPE_ERR_BUFFER_TOO_SMALL if bufLen is insufficient.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails (use fallback).
 *
 * Thread safety: Reentrant.
 * SRS: SRS-AI-010
 */
XPE_API XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img,
                                             char* bodyPartOut, size_t bufLen,
                                             float* confidenceOut);

/**
 * @brief Stitches overlapping partial images into a single wide-field image.
 *
 * Uses AI-based feature matching for alignment. @p stitchedOut must be
 * pre-allocated via dimensions from xpe_stitch_estimate_size().
 *
 * REQ-AI-002: On failure, caller should fall back to deterministic
 *             translation-only stitching.
 *
 * @param parts          Array of partial images. Must not be NULL.
 * @param partCount      Number of images in @p parts. Must be >= 2.
 * @param stitchedOut    Pre-allocated output buffer. Must not be NULL.
 * @param configJsonOrNull  Optional stitch configuration (overlap estimate,
 *                          blend mode). NULL for defaults.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if parts or stitchedOut is NULL, or partCount < 2.
 * @return XPE_ERR_PROCESSING_FAILED if stitching fails.
 * @return XPE_ERR_BUFFER_TOO_SMALL if stitchedOut is too small.
 *
 * Thread safety: Reentrant.
 * SRS: SRS-AI-020, SRS-AI-021
 */
XPE_API XpeErrorCode xpe_stitch_images(const XpeImageBuffer* parts,
                                        uint32_t partCount,
                                        XpeImageBuffer* stitchedOut,
                                        const char* configJsonOrNull);

/**
 * @brief Estimates output dimensions for stitching without performing it.
 *
 * Use returned dimensions to pre-allocate the buffer for xpe_stitch_images().
 *
 * @param parts          Array of partial images. Must not be NULL.
 * @param partCount      Number of images. Must be >= 2.
 * @param widthOut       Output: estimated width in pixels. Must not be NULL.
 * @param heightOut      Output: estimated height in pixels. Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if any parameter is NULL or partCount < 2.
 * @return XPE_ERR_PROCESSING_FAILED if dimensions cannot be estimated.
 *
 * Thread safety: Reentrant.
 * SRS: SRS-AI-020
 */
XPE_API XpeErrorCode xpe_stitch_estimate_size(const XpeImageBuffer* parts,
                                               uint32_t partCount,
                                               uint32_t* widthOut,
                                               uint32_t* heightOut);

/**
 * @brief Produces a soft-tissue-only image by suppressing bony structures.
 *
 * Uses a U-Net style model (REQ-AI-050). @p softTissueOut must be
 * pre-allocated with the same dimensions as @p img.
 *
 * REQ-AI-002: On failure, caller should use the original image unchanged
 *             (bone suppression is optional enhancement).
 *
 * @param img              Input image. Must not be NULL.
 * @param softTissueOut    Output soft-tissue image (pre-allocated, same dims).
 *                          Must not be NULL.
 * @param configJsonOrNull Optional configuration (model variant, strength).
 * @return XPE_OK on success.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or softTissueOut is NULL.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails.
 *
 * Thread safety: Reentrant.
 * SRS: SRS-AI-030
 */
XPE_API XpeErrorCode xpe_bone_suppress(const XpeImageBuffer* img,
                                        XpeImageBuffer* softTissueOut,
                                        const char* configJsonOrNull);

/**
 * @brief Applies deep-learning denoising to an image in-place.
 *
 * Selects model variant based on bodyPart and mAs from @p meta.
 * Complements the classical xpe_noise_reduce (enhance_basic).
 *
 * REQ-AI-002: On failure, caller should fall back to classical
 *             xpe_noise_reduce.
 *
 * @param img              Image to denoise (modified in-place). Must not be NULL.
 * @param meta             Acquisition metadata for model selection. Must not be NULL.
 * @param configJsonOrNull Optional configuration (model variant, strength).
 * @return XPE_OK on success.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or meta is NULL.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails.
 *
 * Thread safety: Reentrant.
 * SRS: SRS-AI-040
 */
XPE_API XpeErrorCode xpe_dl_denoise(XpeImageBuffer* img,
                                     const XpeImageMetadata* meta,
                                     const char* configJsonOrNull);

/**
 * @brief Retrieves the Model Card for a loaded AI model.
 *
 * Returns JSON conforming to schemas/model-card.schema.json containing:
 * intended_use, training_data_summary, demographic_performance, limitations,
 * model_version, pccp_status, published_date.
 *
 * REQ-AI-010: Model Card transparency API.
 * REQ-AI-011: JSON schema conformance.
 * REQ-AI-008: Model metadata (model_id, version, pccp_scope, etc.).
 *
 * @param modelId    Model identifier string (e.g., "bone_suppress_v1").
 *                    Must not be NULL.
 * @param buf        Caller-allocated buffer for JSON output. Must not be NULL.
 * @param bufSize    Size of @p buf in bytes. Recommended >= 4096.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if modelId or buf is NULL.
 * @return XPE_ERR_BUFFER_TOO_SMALL if bufSize is insufficient.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_IO_FAILED if model is not found or not loaded.
 *
 * Thread safety: Thread-safe (read-only model metadata).
 */
XPE_API XpeErrorCode xpe_ai_get_model_card(const char* modelId,
                                             char* buf, size_t bufSize);

/**
 * @brief Controls the deterministic fallback mode for AI functions.
 *
 * When fallback mode is enabled (default), all AI functions will return
 * XPE_ERR_PROCESSING_FAILED when confidence is below threshold, allowing
 * the caller to use deterministic alternatives. When disabled, AI functions
 * will attempt retries before failing.
 *
 * REQ-AI-002: Deterministic fallback router.
 * REQ-AI-012: Low-confidence event triggers fallback.
 *
 * @param enable  Non-zero to enable fallback mode (default), zero to disable.
 * @return XPE_OK on success.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 *
 * Thread safety: Thread-safe (atomic flag).
 */
XPE_API XpeErrorCode xpe_ai_set_fallback_mode(int32_t enable);

#ifdef __cplusplus
}
#endif

#endif /* XPE_AI_API_H */
