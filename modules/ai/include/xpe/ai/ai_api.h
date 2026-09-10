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
 * API contract: docs/project/api-spec.md Section 9. This header declares 10
 * exported functions.
 *
 * BUILD-DEPENDENT BEHAVIOUR -- read before relying on any return value below.
 * The default build is a stub: XPE_AI_USE_ONNXRUNTIME defaults to OFF
 * (modules/ai/CMakeLists.txt:26) and ONNX Runtime is not linked
 * (ai.cpp:9). In that build no inference runs and no worker process is
 * launched, so every inference entry point -- xpe_bodypart_recognize,
 * xpe_stitch_images, xpe_bone_suppress, xpe_dl_denoise -- validates its
 * arguments and then returns XPE_ERR_PROCESSING_FAILED unconditionally. That
 * is the documented fallback signal (REQ-AI-002), so a caller written against
 * the fallback contract is correct either way; a caller expecting XPE_OK from
 * a stub build is not. Per-function notes below mark which returns are
 * currently unreachable.
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
 * xpe_ai.dll -- Exported API (10 functions)
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
 * Calling this again while already initialised is not an error: the call is
 * ignored and XPE_OK is returned (ai.cpp:272-275).
 *
 * @param modelDirPath      Directory containing signed .onnx model files.
 *                          Must not be NULL. In the stub build the path is
 *                          recorded but never opened, so a non-existent
 *                          directory does NOT fail here.
 * @param configJsonOrNull  UTF-8 JSON configuration, or NULL for defaults.
 *                          Malformed JSON is logged and ignored -- defaults are
 *                          used and the call still succeeds (ai.cpp parseConfig).
 * @return XPE_OK on success, and also when already initialised.
 * @return XPE_ERR_INVALID_INPUT if modelDirPath is NULL.
 * @return XPE_ERR_OUT_OF_MEMORY if module state cannot be allocated.
 * @return XPE_ERR_IO_FAILED -- documented for the ONNX build, where the worker
 *         process is launched. Not reachable in the stub build.
 * @return XPE_ERR_CONFIG_INVALID -- documented for the ONNX build. NOT returned
 *         by the current implementation for any input: malformed config is a
 *         warning, not an error.
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
 * resources. Calling it when the module was never initialised is a no-op.
 *
 * After it returns the module is back to the uninitialised state: further calls
 * return XPE_ERR_NOT_INITIALIZED rather than being undefined, and
 * xpe_ai_init() may be called again.
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
 * @param img            Input image. Must not be NULL; zero dimensions, a NULL
 *                       data pointer, a dataSize above 64 MB, or a non-zero
 *                       dataSize smaller than the declared dimensions (#123)
 *                       are all rejected.
 * @param bodyPartOut    Caller-allocated buffer for the label string.
 *                       Must not be NULL. Only bufLen < 1 is treated as too
 *                       small; a larger-but-still-short buffer receives a
 *                       TRUNCATED, null-terminated label without an error.
 * @param bufLen         Size of @p bodyPartOut in bytes. Recommended >= 64.
 * @param confidenceOut  Output: confidence score [0, 1]. May be NULL. On the
 *                       stub path it is set to 0.0 before returning.
 * @return XPE_OK on success -- ONNX build only; not reachable in a stub build.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or bodyPartOut is NULL, or the image
 *         buffer is invalid.
 * @return XPE_ERR_BUFFER_TOO_SMALL if bufLen < 1.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails (use fallback). In a
 *         stub build this is the unconditional outcome, with "UNKNOWN" written
 *         to @p bodyPartOut.
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
 * @param parts          Array of partial images. Must not be NULL. Every
 *                       element is validated, not just the first.
 * @param partCount      Number of images in @p parts. Must be >= 2.
 * @param stitchedOut    Pre-allocated output buffer. Must not be NULL, and must
 *                       carry a non-NULL data pointer and a non-zero dataSize.
 * @param configJsonOrNull  Optional stitch configuration (overlap estimate,
 *                          blend mode). NULL for defaults. Currently ignored.
 * @return XPE_OK on success -- ONNX build only; not reachable in a stub build.
 * @return XPE_ERR_INVALID_INPUT if parts or stitchedOut is NULL, partCount < 2,
 *         or any element of @p parts is an invalid image buffer.
 * @return XPE_ERR_BUFFER_TOO_SMALL if stitchedOut->data is NULL or its
 *         dataSize is 0.
 * @return XPE_ERR_PROCESSING_FAILED if stitching fails. In a stub build this is
 *         the unconditional outcome once validation passes.
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
 * Unlike the inference entry points, this function does NOT require
 * xpe_ai_init() and works in a stub build: the estimate is arithmetic over the
 * part dimensions, not a model call. Both outputs are clamped to 4096.
 *
 * @param parts          Array of partial images. Must not be NULL. Every
 *                       element is validated.
 * @param partCount      Number of images. Must be >= 2.
 * @param widthOut       Output: estimated width in pixels, clamped to 4096.
 *                       Must not be NULL.
 * @param heightOut      Output: estimated height in pixels, clamped to 4096.
 *                       Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if any parameter is NULL, partCount < 2, or any
 *         element of @p parts is an invalid image buffer.
 * @return XPE_ERR_PROCESSING_FAILED -- documented for the ONNX build. NOT
 *         returned by the current implementation for any input.
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
 *                          Must not be NULL. It is validated as a full image
 *                          buffer, so a NULL data pointer or zero dimensions
 *                          are rejected here too.
 * @param configJsonOrNull Optional configuration (model variant, strength).
 *                         Currently ignored.
 * @return XPE_OK on success -- ONNX build only; not reachable in a stub build.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or softTissueOut is NULL, either buffer
 *         is invalid, or the two differ in width or height.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails. In a stub build this is
 *         the unconditional outcome once validation passes.
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
 * @param meta             Acquisition metadata for model selection. Must not be
 *                         NULL. Its contents are not inspected in a stub build.
 * @param configJsonOrNull Optional configuration (model variant, strength).
 *                         Currently ignored.
 * @return XPE_OK on success -- ONNX build only; not reachable in a stub build.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or meta is NULL, or the image buffer is
 *         invalid.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails. In a stub build this is
 *         the unconditional outcome once validation passes; the image is left
 *         unmodified.
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
 * In a stub build the card is a placeholder: model_version is "0.1.0-stub" and
 * the limitations field states that ONNX Runtime is not linked. The loaded-model
 * list is fixed at init time (bodypart_cnn_v1, stitch_feature_match_v1,
 * bone_suppress_unet_v1, dl_denoise_ssl_v1); no directory is scanned.
 *
 * On every path that reaches the copy step, @p buf receives a null-terminated
 * JSON document -- including the not-found path, which writes a card carrying
 * "error":"model_not_loaded" and then returns XPE_ERR_IO_FAILED. A caller must
 * therefore check the return code rather than the presence of output.
 *
 * @param modelId    Model identifier string (e.g., "bone_suppress_v1").
 *                    Must not be NULL.
 * @param buf        Caller-allocated buffer for JSON output. Must not be NULL.
 * @param bufSize    Size of @p buf in bytes. Recommended >= 4096.
 * @return XPE_OK if the model is known and the card fits.
 * @return XPE_ERR_INVALID_INPUT if modelId or buf is NULL.
 * @return XPE_ERR_BUFFER_TOO_SMALL if bufSize < 1, or if the card does not fit;
 *         in the latter case @p buf still holds the truncated JSON.
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
 * NOTE: that description states the intended ONNX-build behaviour. In the
 * current implementation the call only stores the flag (ai.cpp:595); no code
 * path reads it back, so toggling it changes nothing observable. The flag is
 * also settable at init through the "fallback_mode" config key.
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
