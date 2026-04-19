/**
 * @file xpe_preprocess_api.h
 * @brief XPE Phase 1a Pre-Processing DLL public C API (18 exported functions)
 *
 * All functions use C linkage (__cdecl) and blittable types for P/Invoke compatibility.
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#ifndef XPE_PREPROCESS_API_H_NEW
#define XPE_PREPROCESS_API_H_NEW

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Module Lifecycle / GUI Readiness
 * ========================================================================= */

XPE_API const char* xpe_preprocess_version(void);

XPE_API XpeErrorCode xpe_preprocess_init(const char* configJsonOrNull);

XPE_API void xpe_preprocess_shutdown(void);

XPE_API XpeErrorCode xpe_preprocess_get_param_range(const char* paramName,
                                                     float* minValue,
                                                     float* maxValue);

/* =========================================================================
 * SWU-1.1: Offset Correction (PRE-02)
 * REQ-P1A-009 to REQ-P1A-011
 * ========================================================================= */

/**
 * @brief Apply per-pixel dark offset subtraction in-place.
 *        corrected[i] = clamp(raw[i] - offsetMap[i], 0)
 * @param img       [in/out] Image to correct (uint16 format)
 * @param offsetMap [in]     Per-pixel offset map (must match img dimensions)
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on dimension mismatch or NULL input
 */
XPE_API XpeErrorCode xpe_offset_correct(XpeImageBuffer* img,
                                         const XpeImageBuffer* offsetMap);

/* =========================================================================
 * SWU-1.2: Gain Correction (PRE-03) + uint16->float32 domain transition
 * REQ-P1A-016 to REQ-P1A-019
 * ========================================================================= */

/**
 * @brief Apply per-pixel flat-field gain normalization.
 *        corrected[i] = img[i] * gainMap[i]  (output: float32)
 *        Domain transition: uint16 input -> float32 output occurs here (stage 2).
 *
 * @par Ownership Transfer
 *   This function allocates a new float32 pixel buffer (via malloc) and stores
 *   the pointer in img->data. The caller takes ownership of the new buffer and
 *   must call free(img->data) when done. The original uint16 buffer is NOT freed
 *   by this function — the caller retains ownership of the original buffer.
 *
 * @par Buffer Layout
 *   Both img and gainMap must be contiguous (stride == width * element_size).
 *   Non-contiguous (row-padded) buffers return XPE_ERR_INVALID_INPUT.
 *
 * @param img     [in/out] Image to correct (uint16 in, float32 out); data ptr replaced
 * @param gainMap [in]     Per-pixel gain map (float32, must match img dimensions)
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT (NULL/dim mismatch/non-contiguous),
 *         XPE_ERR_OUT_OF_MEMORY if allocation fails
 */
XPE_API XpeErrorCode xpe_gain_correct(XpeImageBuffer* img,
                                       const XpeImageBuffer* gainMap);

/* =========================================================================
 * SWU-1.3: Defect Pixel Correction (PRE-06)
 * REQ-P1A-024 to REQ-P1A-028
 * ========================================================================= */

/**
 * @brief Replace defect pixels using edge-aware bilinear interpolation.
 * @param img           [in/out] Image to correct (float32 format)
 * @param defectMap     [in]     Non-zero pixels indicate defect locations
 * @param configJsonOrNull [in]  Optional JSON: {"mode": "nearest"|"bilinear"|"median"}
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on NULL required input
 */
XPE_API XpeErrorCode xpe_defect_correct(XpeImageBuffer* img,
                                         const XpeImageBuffer* defectMap,
                                         const char* configJsonOrNull);

/**
 * @brief Detect transient defect pixels via statistical outlier analysis.
 * @param img          [in]  Input image frame
 * @param defectMapOut [out] Boolean defect map (non-zero = defect)
 * @param configJsonOrNull [in] Optional detection config JSON
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on NULL required input
 */
XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* img,
                                                XpeImageBuffer* defectMapOut,
                                                const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.4: Ghost/Lag Correction Tier 1 -- LTI deconvolution (PRE-04)
 * REQ-P1A-029 to REQ-P1A-034
 * ========================================================================= */

/**
 * @brief Allocate an opaque ghost corrector handle with frame history buffer.
 * @param width         [in]  Image width
 * @param height        [in]  Image height
 * @param configJsonOrNull [in] Optional IRF configuration JSON
 * @param handleOut     [out] Opaque handle pointer; caller must call xpe_ghost_destroy
 * @return XPE_OK on success, XPE_ERR_OUT_OF_MEMORY on allocation failure
 *
 * @note Handle is NOT thread-safe; do not share across threads (REQ-P1A-066)
 */
XPE_API XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                                       const char* configJsonOrNull,
                                       void** handleOut);

/**
 * @brief Apply LTI lag correction using dual-exponential IRF model.
 * @param handle [in]     Ghost corrector handle (from xpe_ghost_create)
 * @param img    [in/out] Image to correct (float32 format)
 * @param meta   [in]     Image metadata (acquisitionTime used for IRF timing)
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on NULL/invalid handle
 */
XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                        const XpeImageMetadata* meta);

/**
 * @brief Clear accumulated frame history without destroying the handle.
 *        Call between patient acquisitions or after detector power cycle.
 * @param handle [in] Ghost corrector handle
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on NULL/invalid handle
 */
XPE_API XpeErrorCode xpe_ghost_reset(void* handle);

/**
 * @brief Free all resources associated with a ghost corrector handle.
 *        After this call the handle is invalid (do not pass to any other function).
 * @param handle [in] Ghost corrector handle to destroy (may be NULL, no-op)
 */
XPE_API void xpe_ghost_destroy(void* handle);

/* =========================================================================
 * SWU-1.5: Calibration Manager (SUP-01)
 * REQ-P1A-035 to REQ-P1A-040
 * ========================================================================= */

/**
 * @brief Load offset calibration map from file, verify CRC-32, populate offsetMapOut.
 * @param filePath     [in]  Path to calibration file
 * @param offsetMapOut [out] Populated by this function; caller owns the buffer
 * @return XPE_OK, XPE_ERR_IO_FAILED (CRC mismatch), XPE_ERR_CALIBRATION_EXPIRED
 */
XPE_API XpeErrorCode xpe_calib_load_offset(const char* filePath,
                                            XpeImageBuffer* offsetMapOut);

/**
 * @brief Load gain calibration map from file, verify CRC-32, populate gainMapOut.
 * @param filePath  [in]  Path to calibration file
 * @param gainMapOut [out] Populated by this function; caller owns the buffer
 * @return XPE_OK, XPE_ERR_IO_FAILED (CRC mismatch), XPE_ERR_CALIBRATION_EXPIRED
 */
XPE_API XpeErrorCode xpe_calib_load_gain(const char* filePath,
                                          XpeImageBuffer* gainMapOut);

/**
 * @brief Load static defect map from file.
 * @param filePath     [in]  Path to defect map file
 * @param defectMapOut [out] Non-zero pixels indicate defect locations
 * @return XPE_OK, XPE_ERR_IO_FAILED on read/CRC error
 */
XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filePath,
                                                XpeImageBuffer* defectMapOut);

/**
 * @brief Compute per-pixel mean across frameCount dark-field frames.
 * @param frames       [in]  Array of frameCount XpeImageBuffers
 * @param frameCount   [in]  Number of dark frames (>= 1)
 * @param offsetMapOut [out] Computed mean offset map
 * @param configJsonOrNull [in] Optional generation config JSON
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on NULL or zero frameCount
 */
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* frames,
                                                uint32_t frameCount,
                                                XpeImageBuffer* offsetMapOut,
                                                const char* configJsonOrNull);

/**
 * @brief Read embedded expiry timestamp from calibration file.
 * @param filePath        [in]  Path to calibration file
 * @param expiryEpochMsOut [out] Expiry timestamp in milliseconds since epoch
 * @return XPE_OK if not expired, XPE_ERR_CALIBRATION_EXPIRED if past system clock
 */
XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filePath,
                                             uint64_t* expiryEpochMsOut);

/**
 * @brief Write calibration map to file with CRC-32 checksum and expiry timestamp.
 * @param calibMap       [in] Calibration map to save
 * @param filePath       [in] Destination file path
 * @param expiryEpochMs  [in] Expiry timestamp in milliseconds since epoch
 * @param configJsonOrNull [in] Optional save config JSON
 * @return XPE_OK on success, XPE_ERR_IO_FAILED on write error
 */
XPE_API XpeErrorCode xpe_calib_save(const XpeImageBuffer* calibMap,
                                     const char* filePath,
                                     uint64_t expiryEpochMs,
                                     const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.6: Temperature Compensation (PRE-07)
 * REQ-P1A-005 to REQ-P1A-008
 * ========================================================================= */

/**
 * @brief Adjust pixel values for dark current temperature dependence.
 *        Model: I_dark(T) = I_0 * exp(-E_g / (2 * k_B * T))
 * @param img            [in/out] Image to correct (uint16 format)
 * @param detectorTempC  [in]     Detector temperature in Celsius; NaN -> use 25.0C fallback
 * @param configJsonOrNull [in]   Optional calibration coefficient override JSON
 * @return XPE_OK, XPE_ERR_INVALID_INPUT (temp out of [-20, +60] range or NULL img)
 */
XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
                                          float detectorTempC,
                                          const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.7: Nonlinearity Correction (PRE-08)
 * REQ-P1A-012 to REQ-P1A-015
 * ========================================================================= */

/**
 * @brief Apply piecewise linear or polynomial correction to linearize detector response.
 *        No-op if no nonlinearity coefficients are loaded for this panel profile.
 * @param img            [in/out] Image to correct (uint16 format)
 * @param configJsonOrNull [in]   Optional detector mode/coefficient override JSON
 * @return XPE_OK, XPE_ERR_CONFIG_INVALID (unknown detector mode), XPE_ERR_INVALID_INPUT
 */
XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                               const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.8: Binning Correction (PRE-09)
 * REQ-P1A-020 to REQ-P1A-023
 * ========================================================================= */

/**
 * @brief Apply per-mode binning correction for gain/uniformity differences.
 *        No-op when binningMode == 1 (1x1, no binning).
 * @param img          [in/out] Image to correct (float32 format)
 * @param binningMode  [in]     Binning factor (1 = no-op, 2 = 2x2, 4 = 4x4, etc.)
 * @param configJsonOrNull [in] Optional correction profile JSON
 * @return XPE_OK, XPE_ERR_CONFIG_INVALID (unknown binning mode), XPE_ERR_INVALID_INPUT
 */
XPE_API XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                          int32_t binningMode,
                                          const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.9: Readout Artifact Validation (PRE-01)
 * REQ-P1A-001 to REQ-P1A-004
 * ========================================================================= */

/**
 * @brief Validate raw uint16 image for readout artifacts (line noise, dropped columns,
 *        ADC saturation). Call BEFORE any correction stage.
 * @param rawImg         [in]  Raw uint16 image to validate
 * @param artifactScoreOut [out] Normalized score: 0 = clean, 100 = severely corrupted
 * @param msgOut         [out] Operator-readable summary message buffer
 * @param msgLen         [in]  Size of msgOut buffer in bytes
 * @return XPE_OK on success (score > 80 posts WARNING alert but returns XPE_OK)
 */
XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* rawImg,
                                                    int32_t* artifactScoreOut,
                                                    char* msgOut,
                                                    size_t msgLen);

/* =========================================================================
 * Full Pre-Processing Pipeline (stages 0.5-4)
 * REQ-P1A-041 to REQ-P1A-047
 * ========================================================================= */

/**
 * @brief Execute full pre-processing pipeline with bypass logic.
 *        Pipeline: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost
 * @param img           [in/out] Image to process (uint16 in, float32 out after Gain)
 * @param meta          [in/out] Image metadata (updated with processing flags)
 * @param calibPath     [in]     Calibration data directory path
 * @param ghostHandle   [in]     Ghost corrector handle (NULL = skip ghost correction)
 * @param configJsonOrNull [in]  Pipeline configuration JSON (bypass flags, etc.)
 * @return XPE_OK on success, XPE_ERR_* on failure
 */
XPE_API XpeErrorCode xpe_preprocess_pipeline(XpeImageBuffer* img,
                                              XpeImageMetadata* meta,
                                              const char* calibPath,
                                              void* ghostHandle,
                                              const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.10: Calibration Data Caching (LRU)
 * Eliminates repeated file I/O for calibration maps.
 * ========================================================================= */

/**
 * @brief Load offset calibration map with LRU caching.
 *        On cache hit, returns cached data without file I/O.
 *        On miss, loads from file and inserts into cache.
 * @param filePath     [in]  Path to calibration file
 * @param offsetMapOut [out] Populated by this function; caller owns the buffer.
 *                           Data pointer is shared with cache — do NOT free.
 * @return XPE_OK, XPE_ERR_IO_FAILED, XPE_ERR_CALIBRATION_EXPIRED
 */
XPE_API XpeErrorCode xpe_calib_load_offset_cached(const char* filePath,
                                                    XpeImageBuffer* offsetMapOut);

/**
 * @brief Load gain calibration map with LRU caching.
 * @param filePath  [in]  Path to calibration file
 * @param gainMapOut [out] Populated on hit or miss; data shared with cache.
 * @return XPE_OK, XPE_ERR_IO_FAILED, XPE_ERR_CALIBRATION_EXPIRED
 */
XPE_API XpeErrorCode xpe_calib_load_gain_cached(const char* filePath,
                                                  XpeImageBuffer* gainMapOut);

/**
 * @brief Load defect map with LRU caching.
 * @param filePath     [in]  Path to defect map file
 * @param defectMapOut [out] Populated on hit or miss; data shared with cache.
 * @return XPE_OK, XPE_ERR_IO_FAILED, XPE_ERR_CALIBRATION_EXPIRED
 */
XPE_API XpeErrorCode xpe_calib_load_defect_cached(const char* filePath,
                                                    XpeImageBuffer* defectMapOut);

/**
 * @brief Clear all entries from the calibration cache, freeing memory.
 */
XPE_API void xpe_calib_cache_clear(void);

/**
 * @brief Set the maximum number of calibration maps retained in cache.
 *        Default is 4. Excess entries are evicted (LRU first).
 * @param maxMaps Maximum cache entries (minimum 1)
 */
XPE_API void xpe_calib_cache_set_max_size(uint32_t maxMaps);

/* =========================================================================
 * SWU-1.11: Pre-loaded Calibration State (Pipeline Optimization)
 * Load calibration maps once, reuse across multiple pipeline invocations.
 * ========================================================================= */

/**
 * @brief Load all calibration maps from a directory into a state struct.
 *        Files expected: offset.xcal, gain.xcal, defect.xcal
 *        Missing files are silently skipped (corresponding *Loaded flag = false).
 * @param state     [out] Zero-initialized state to populate
 * @param calibPath [in]  Calibration data directory path
 * @return XPE_OK on success (at least one map loaded),
 *         XPE_ERR_INVALID_INPUT on null parameters
 */
XPE_API XpeErrorCode xpe_calib_state_load(void* state, const char* calibPath);

/**
 * @brief Free all resources held by a calibration state struct.
 *        Safe to call on zero-initialized or already-released state.
 * @param state [in/out] Calibration state to release
 */
XPE_API void xpe_calib_state_release(void* state);

/* =========================================================================
 * Optimized Pipeline with Pre-loaded Calibration State
 * REQ-P1A-041 to REQ-P1A-047 (extended)
 * ========================================================================= */

/**
 * @brief Execute full pre-processing pipeline using pre-loaded calibration maps.
 *        Identical to xpe_preprocess_pipeline() but skips file I/O by using
 *        calibration data from a pre-loaded XpeCalibrationState.
 * @param img           [in/out] Image to process
 * @param meta          [in/out] Image metadata
 * @param calibState    [in]     Pre-loaded calibration state (from xpe_calib_state_load)
 * @param ghostHandle   [in]     Ghost corrector handle (NULL = skip ghost)
 * @param configJsonOrNull [in]  Pipeline configuration JSON
 * @return XPE_OK on success, XPE_ERR_* on failure
 */
XPE_API XpeErrorCode xpe_preprocess_pipeline_ex(XpeImageBuffer* img,
                                                  XpeImageMetadata* meta,
                                                  const void* calibState,
                                                  void* ghostHandle,
                                                  const char* configJsonOrNull);

/* =========================================================================
 * SWU-1.12: Batch Processing
 * Apply identical calibration to multiple frames with SIMD parallelism.
 * ========================================================================= */

/**
 * @brief Process multiple frames with identical calibration in batch.
 *        All frames share the same calibration maps (offset/gain/defect).
 *        Optimized for AVX2 parallel processing of frames.
 *
 * @param images        [in/out] Array of imageCount XpeImageBuffer to process.
 *                             Each image undergoes the full pipeline.
 * @param imageCount    [in]    Number of images in the array (must be >= 1)
 * @param metas         [in/out] Array of imageCount XpeImageMetadata
 * @param calibPath     [in]    Calibration data directory path
 * @param ghostHandle   [in]    Ghost corrector handle (NULL = skip ghost)
 * @param configJsonOrNull [in] Pipeline configuration JSON
 * @return XPE_OK if all images processed successfully,
 *         XPE_ERR_INVALID_INPUT on null/invalid parameters,
 *         first error code if any individual frame fails
 */
XPE_API XpeErrorCode xpe_preprocess_pipeline_batch(
    XpeImageBuffer* images,
    uint32_t imageCount,
    XpeImageMetadata* metas,
    const char* calibPath,
    void* ghostHandle,
    const char* configJsonOrNull);

#ifdef __cplusplus
}
#endif

#endif /* XPE_PREPROCESS_API_H_NEW */
