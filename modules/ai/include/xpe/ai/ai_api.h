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

/**
 * @defgroup xpe_ai XPE AI
 * @brief Deep-learning inference proxy -- C API (ai_api.h), worker IPC protocol
 *        (ai_worker_protocol.h) and ONNX session (ai_onnx_session.h).
 *
 * Defined here, as xpe_dicom is in dicom_api.h and xpe_common in xpe_types.h,
 * so the group references from this module's headers resolve to a real group
 * instead of being ignored (Doxygen reports a reference to an undefined group
 * as an error under FAIL_ON_WARNINGS).
 */

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
 * @return Non-NULL version string. Thread-safe (reads no module state, but see the LIFECYCLE CONTRACT at
 *         xpe_ai_shutdown(): it is covered too).
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
 * The boolean key "use_worker" (default false) routes xpe_bone_suppress through
 * the worker process (xpe_ai_worker.exe, found in the directory of xpe_ai.dll
 * and nowhere else). When that path fails -- the time budget ("timeout_ms") is
 * exceeded, the worker dies or goes silent, or it answers wrongly -- the call
 * copies the INPUT image to the output unchanged, raises one Warning alert and
 * returns a non-OK code; it does not re-run the inference in this process
 * (REQ-AI-003 keeps the model out of the host).
 *
 * After 3 CONSECUTIVE failures the worker is switched off for the rest of the
 * session (until xpe_ai_shutdown): the alert of the 3rd failure says so, its
 * process is ended, and later calls return the input at once with
 * XPE_ERR_PROCESSING_FAILED, start no worker and raise no further alert.
 * A success resets the count, so the limit of 3 alerts applies only to failures
 * that follow one another: a worker that fails intermittently (fail, fail,
 * succeed, repeat) is never switched off and raises one alert for EVERY failure.
 * A full alert queue is SRS-ALERT-007's concern, not this policy's. The
 * ceiling of 3 and the alert rule are values the user approved on 2026-10-01
 * (docs/project/REQ-CHANGE-LOG-P3-AI.md rows 2 and 3), not ones the requirements
 * state. The alert cites REQ-AI-002 and REQ-AI-092; the SRS table has no row for
 * the failure itself.
 *
 * The 3 count CONSECUTIVE worker and transport faults. A non-OK result of the
 * worker path counts, including an error reply that a worker sends on purpose
 * because the model refused the request (a missing or unloadable model, an input
 * length it rejects): a model that refuses three times in a row means AI is
 * unusable for the session, so that worker is switched off too;
 * xpe_ai_shutdown() followed by xpe_ai_init() recovers it. ONE result is not a
 * fault: a reply with a VALID success envelope (success true, the request's
 * width and height, float32) whose pixels are non-finite -- the model could not
 * give a result for this image. It is refused (see xpe_bone_suppress), raises
 * its own alert and, like any valid response, ENDS a run of faults; it never
 * counts, however many come in a row. A reply with a broken envelope (missing or
 * unparseable JSON, success not true, another size or format) is a protocol
 * fault and counts. There is no per-session total. A valid envelope does not
 * prove the worker healthy -- it only keeps a broken one from passing as an answer.
 *
 * Every image the module accepts, up to 4096 x 4096 float32 (64 MiB), travels in one
 * worker message; the worker path has no size limit of its own, and the contract
 * above (output = input on failure, also once the worker is switched off) holds for
 * the largest image exactly as for a small one.
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
 * LIFECYCLE CONTRACT: see xpe_ai_shutdown(). It applies to this call too.
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
 *
 * LIFECYCLE CONTRACT (applies to xpe_ai_init() and xpe_ai_shutdown()): each of them frees or creates the
 * module state that the other functions of this header read. Neither may therefore run concurrently with the
 * other, with itself, or with ANY other function declared in THIS header -- the boundary is the header, not
 * the name prefix. That is: xpe_ai_version, xpe_bodypart_recognize, xpe_stitch_images,
 * xpe_stitch_estimate_size, xpe_bone_suppress, xpe_dl_denoise, xpe_ai_get_model_card,
 * xpe_ai_set_fallback_mode and xpe_ai_worker_state (some of them do not read the state today; they are
 * covered anyway, so the contract does not depend on an implementation detail). This includes the functions
 * documented as "Reentrant" or "Thread-safe": those words describe concurrency among those functions, not
 * against init/shutdown. The caller serialises the lifecycle (for example a reader/writer lock held shared around
 * every call and exclusive around init/shutdown). A call that races them is a use-after-free, not a stale
 * answer. The module deliberately does not guard against it: a guard would make shutdown wait behind the
 * longest in-flight call (a worker call can run to its deadline) and would put a lock on
 * xpe_ai_worker_state(), which is documented lock-free so that a UI thread never waits.
 *
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
 *                       Must not be NULL.
 * @param bufLen         Size of @p bodyPartOut in bytes. Recommended >= 64.
 * @param confidenceOut  Output: confidence score [0, 1]. May be NULL. On the
 *                       stub path it is set to 0.0 before returning.
 * @return XPE_OK on success -- ONNX build only; not reachable in a stub build.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or bodyPartOut is NULL, the image
 *         buffer is invalid, or bufLen is 0 -- a zero-length output buffer is a
 *         missing argument, not a small one (#142).
 * @return XPE_ERR_BUFFER_TOO_SMALL if the buffer is real but cannot hold the
 *         label and its terminator. The label is never truncated silently.
 * @return XPE_ERR_PROCESSING_FAILED if inference fails (use fallback). In a
 *         stub build this is the unconditional outcome, with "UNKNOWN" written
 *         to @p bodyPartOut.
 *
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
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
 *         any element of @p parts is an invalid image buffer, or stitchedOut
 *         has a NULL data pointer or a dataSize of 0 (#142 -- a missing output
 *         argument, the same answer xpe_bone_suppress gives).
 * @return XPE_ERR_BUFFER_TOO_SMALL if the output buffer is real but cannot hold
 *         the stitched result.
 * @return XPE_ERR_PROCESSING_FAILED if stitching fails. In a stub build this is
 *         the unconditional outcome once validation passes.
 *
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
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
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
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
 *
 * PIXEL SCALE (QA-B-173). The CALLER supplies pixels in the scale the model was
 * trained for; this module does not normalise. The float values of @p img reach
 * the model exactly as given, on the in-process path and on the worker path,
 * and the model's output is copied to @p softTissueOut exactly as it comes
 * back: nothing is scaled, clamped, shifted or range-checked on the way in or
 * out. A value of 65535.0 or -3.0 is passed on as such, and the values are not
 * checked at all, not even for NaN. XPE-SDD-002 says "Preprocess:
 * normalize input to [0, 1]", but no code in this module does that, so an
 * image that must be in [0, 1] has to be put there by the caller before the
 * call, and the output is in whatever scale the model produces. Which scale a
 * given model expects is a property of the model; the module does not know it.
 *
 * MODEL AND SESSION OWNERSHIP (QA-B-161, #130, FUNC-038).
 * The model is read from `<modelDir>/bone_suppress.onnx`, where `<modelDir>`
 * is the path given to xpe_ai_init. The session built from it is owned by this
 * module: it is created on the FIRST call that needs it (not by xpe_ai_init),
 * reused by later calls, rebuilt if xpe_ai_init is called again with a
 * different directory, and released by xpe_ai_shutdown.
 * THE CALLER NEVER RECEIVES, STORES, OR FREES IT. No session handle crosses
 * this ABI, and nothing here returns a pointer the caller must manage --
 * @p softTissueOut is the caller's own buffer and stays the caller's.
 *
 * @return XPE_OK on success -- ONNX build only; not reachable in a stub build.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_INVALID_INPUT if img or softTissueOut is NULL, either buffer
 *         is invalid, the two differ in width or height, a buffer is too small
 *         for width*height floats, or the model rejected the input length.
 * @return XPE_ERR_UNSUPPORTED_FORMAT if either image is not XPE_PIXEL_FLOAT32.
 *         The session speaks float32; reinterpreting 16-bit pixels as floats
 *         would return numbers instead of an error.
 * @return XPE_ERR_IO_FAILED if `<modelDir>/bone_suppress.onnx` is not there.
 * @return XPE_ERR_CONFIG_INVALID if that file exists but is not a loadable
 *         model. Distinct from IO_FAILED on purpose: "install the model" and
 *         "the model you installed is broken" need different actions.
 * @return XPE_ERR_PROCESSING_FAILED if inference itself fails, the model
 *         returns a different number of values than the image has pixels, or
 *         the result contains a non-finite value (+/-inf, NaN) -- a finite input
 *         can overflow, e.g. a x2 model on a pixel above FLT_MAX / 2 (QA-B-181h,
 *         181i). The input was valid, so this is not INVALID_INPUT. The whole
 *         result is judged before it is copied; in-process, @p softTissueOut is
 *         left as the caller passed it. One Warning alert names the cause. With
 *         "use_worker" the same code is returned and @p softTissueOut holds the
 *         input, but this refusal is NOT a worker failure: it neither counts
 *         toward the ceiling below nor alerts as one, and it ends a run of worker
 *         faults. Its alert, on both paths, reads exactly "AI model output was
 *         non-finite (inf/NaN); this image was not AI-processed" and says nothing
 *         of the worker (its state is xpe_ai_worker_state()'s). In a stub build
 *         PROCESSING_FAILED is the unconditional outcome once validation passes.
 *
 * With "use_worker" set at xpe_ai_init, a failure of the worker path returns
 * the worker's or the transport's error code (never XPE_OK), raises one Warning
 * alert and leaves @p softTissueOut equal to @p img byte for byte. Once the
 * worker has been switched off (3 consecutive failures, see xpe_ai_init) every
 * call returns XPE_ERR_PROCESSING_FAILED with the same output, without trying
 * the worker and without an alert.
 *
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
 * Thread safety: Reentrant. Calls are serialised on the module mutex, so
 * concurrent callers do not race the lazy session load.
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
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
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
 * @return XPE_ERR_INVALID_INPUT if modelId or buf is NULL, or bufSize is 0
 *         (#142).
 * @return XPE_ERR_BUFFER_TOO_SMALL if the card does not fit; @p buf still holds
 *         the truncated JSON in that case.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init not called.
 * @return XPE_ERR_IO_FAILED if model is not found or not loaded.
 *
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
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
 * Lifecycle: the label below does NOT cover concurrency with xpe_ai_init / xpe_ai_shutdown -- see the
 * LIFECYCLE CONTRACT at xpe_ai_shutdown().
 * Thread safety: Thread-safe (atomic flag).
 */
XPE_API XpeErrorCode xpe_ai_set_fallback_mode(int32_t enable);

/** xpe_ai_worker_state(): "use_worker" was not set at xpe_ai_init; there is no worker path. */
#define XPE_AI_WORKER_NOT_USED  0
/** xpe_ai_worker_state(): the worker path is in use and has not been switched off. */
#define XPE_AI_WORKER_ACTIVE    1
/**
 * xpe_ai_worker_state(): the worker path was switched off for the rest of this session after
 * the ceiling of consecutive failures. xpe_bone_suppress now returns its input with
 * XPE_ERR_PROCESSING_FAILED at once. Recover with xpe_ai_shutdown() then xpe_ai_init().
 */
#define XPE_AI_WORKER_DISABLED  2

/**
 * @brief Reads the worker path's status for this session, without changing anything.
 *
 * The source of truth for "the AI worker is off for this session", which the
 * alert queue cannot be: alerts are drained by the reader and can overflow
 * (SRS-ALERT-007), the status stays. A client shows it, and offers recovery
 * (xpe_ai_shutdown() then xpe_ai_init(), which starts a new session with a
 * clean count) when it reads XPE_AI_WORKER_DISABLED.
 *
 * Read-only: it starts no worker, raises no alert, counts as no call and
 * changes no state. It does NOT wait for a call that is in progress -- a call
 * can hold the module for its whole time budget on a silent worker, and a
 * status query that waited would freeze a UI thread for that long. Called
 * while a call is running, it reports the state as of the last COMPLETED call.
 *
 * @param stateOut                One of XPE_AI_WORKER_NOT_USED,
 *                                XPE_AI_WORKER_ACTIVE, XPE_AI_WORKER_DISABLED.
 *                                Must not be NULL.
 * @param consecutiveFailuresOut  Optional (may be NULL): failures in a row since
 *                                the last success; 0 when the worker path is
 *                                not used; equal to the ceiling once DISABLED.
 * @param ceilingOut              Optional (may be NULL): the number of
 *                                consecutive failures that switches the worker
 *                                off (3), so a client need not hard-code it.
 * @return XPE_OK on success.
 * @return XPE_ERR_NOT_INITIALIZED if xpe_ai_init is not in effect (never
 *         called, or xpe_ai_shutdown since); the outputs are left untouched.
 * @return XPE_ERR_INVALID_INPUT if stateOut is NULL.
 *
 * The state it reports is published as ONE snapshot (the flag and the count
 * together) only when a call has finished everything it does, including ending
 * the worker process and raising the alert. While the third failure is still in
 * progress it therefore keeps reporting the previous completed call (active, 2),
 * not DISABLED.
 *
 * Thread safety: Thread-safe and lock-free against running calls to
 * xpe_bone_suppress and the other inference functions. It MUST NOT run
 * concurrently with xpe_ai_init or xpe_ai_shutdown, which the module documents
 * as not thread-safe (they free or create the state this reads): a call that
 * races them is a use-after-free, not a stale answer. A client serialises it
 * with its own init / shutdown / recover logic.
 * REQ-AI-002, REQ-AI-092.
 */
XPE_API XpeErrorCode xpe_ai_worker_state(int32_t* stateOut,
                                          uint32_t* consecutiveFailuresOut,
                                          uint32_t* ceilingOut);

#ifdef __cplusplus
}
#endif

#endif /* XPE_AI_API_H */
