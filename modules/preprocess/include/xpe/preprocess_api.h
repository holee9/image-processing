/**
 * @file preprocess_api.h
 * @brief XPE Preprocessing Module API (SPEC-XPE-P1A)
 *
 * Phase 1: Lifecycle (3 functions)
 * - xpe_preprocess_version()
 * - xpe_preprocess_init()
 * - xpe_preprocess_shutdown()
 *
 * Phase 2: Calibration Loading (3 functions)
 * - xpe_calib_load_offset()
 * - xpe_calib_load_gain()
 * - xpe_calib_load_defect_map()
 *
 * Phase 3: Correction Algorithms (3 functions)
 * - xpe_offset_correct()
 * - xpe_gain_correct()
 * - xpe_defect_correct()
 *
 * Phase 4: Calibration Management (3 functions)
 * - xpe_calib_generate_offset()
 * - xpe_calib_check_expiry()
 * - xpe_calib_save()
 *
 * Phase 5: Utilities (2 functions)
 * - xpe_defect_detect_runtime()
 * - xpe_preprocess_get_param_range()
 *
 * Total: 15 API functions
 *
 * ABI Compliance:
 * - extern "C" linkage
 * - __cdecl calling convention (Windows)
 * - #pragma pack(push, 8) for C# P/Invoke compatibility
 */

#ifndef XPE_PREPROCESS_API_H
#define XPE_PREPROCESS_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Phase 1: Lifecycle Functions (REQ-P1A-001, AC-LC-001~003)
 * ============================================================================ */

/**
 * @brief Return preprocessing module version string.
 *
 * The returned pointer is owned by the module and remains valid for the process
 * lifetime. GUI readiness probes use this export for R1 binary health only.
 */
XPE_API const char* xpe_preprocess_version(void);

/**
 * @brief Initialize preprocessing module with configuration
 *
 * REQ-P1A-001: Module Initialization
 * AC-LC-001: Accept NULL config for default settings
 * AC-LC-002: Accept JSON config string for custom settings
 * AC-LC-003: Return XPE_ERR_INVALID_INPUT on double-init
 * REQ-P1A-003: Thread-safe for concurrent initialization attempts
 *
 * @param config JSON configuration string (NULL for default)
 *               Example: "{\"mode\":\"clinical\",\"log_level\":1}"
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on double-init
 *         XPE_ERR_CONFIG_INVALID on invalid JSON
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 */
XPE_API XpeErrorCode xpe_preprocess_init(const char* config);

/**
 * @brief Shutdown preprocessing module and release resources
 *
 * REQ-P1A-031: No memory leak after shutdown
 * REQ-P1A-003: Thread-safe for concurrent shutdown
 *
 * Safe to call multiple times. If module is not initialized, this is a no-op.
 * After shutdown, module returns to uninitialized state and can be re-initialized.
 */
XPE_API void xpe_preprocess_shutdown(void);

/* =============================================================================
 * Phase 2: Calibration Loading Functions (REQ-P1A-014~016, AC-CAL-001~003)
 * ============================================================================ */

/**
 * @brief Load offset calibration map from XCal file
 *
 * REQ-P1A-014: Load XCal format offset maps
 * AC-CAL-001: Validate SHA-256, check session matching, verify expiry
 *
 * @param filepath Path to XCal format offset file
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 *         XPE_ERR_CONFIG_INVALID if session mismatch
 */
XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath);

/**
 * @brief Load gain calibration map from XCal file
 *
 * REQ-P1A-015: Load XCal format gain maps with multi-SID interpolation
 * AC-CAL-002: Load with interpolation table for kVp-specific gain
 *
 * @param filepath Path to XCal format gain file
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 */
XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath);

/**
 * @brief Load defect map (BPM) from XCal file
 *
 * REQ-P1A-016: Load XCal format defect maps (BPM)
 * AC-CAL-003: Validate defect locations and integrity
 *
 * @param filepath Path to XCal format defect map file
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_IO_FAILED on file read error
 */
XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath);

/* =============================================================================
 * Phase 3: Correction Algorithms (REQ-P1A-010~013, AC-OFF/GAIN/DEF-001~003)
 * ============================================================================ */

/**
 * @brief Execute offset correction: I_offset = max(I_raw - I_dark, 0)
 *
 * REQ-P1A-010: Offset correction with temperature interpolation
 * AC-OFF-001: Basic offset correction with floor-at-zero
 * AC-OFF-002: Temperature interpolation between two offset maps
 * AC-OFF-003: PREP-time exponential decay model
 * REQ-P1A-020: Return XPE_ERR_NOT_INITIALIZED if not initialized
 * REQ-P1A-021: Validate dimension mismatch
 *
 * @param input Input image buffer (raw X-ray data, UINT16)
 * @param output Output image buffer (offset-corrected, UINT16)
 * @param metadata Image metadata including temperature and acquisition time
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers
 *         XPE_ERR_BUFFER_TOO_SMALL if dimension mismatch
 */
XPE_API XpeErrorCode xpe_offset_correct(const XpeImageBuffer* input,
                                        XpeImageBuffer* output,
                                        const XpeImageMetadata* metadata);

/**
 * @brief Execute gain correction with UINT16→FLOAT32 conversion
 *
 * REQ-P1A-011: Gain correction with format conversion
 * AC-GAIN-001: UINT16 to FLOAT32 conversion, divide by gain map
 * AC-GAIN-002: Multi-SID gain interpolation
 * AC-GAIN-003: Validate NaN/Inf values
 * REQ-P1A-021: Validate dimension mismatch
 * REQ-P1A-022: Validate format mismatch
 *
 * @param input Input image buffer (offset-corrected, UINT16)
 * @param output Output image buffer (gain-corrected, FLOAT32)
 * @param metadata Image metadata including kVp and SID
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers
 *         XPE_ERR_BUFFER_TOO_SMALL if dimension mismatch
 *         XPE_ERR_UNSUPPORTED_FORMAT if format mismatch
 *         XPE_ERR_CONFIG_INVALID if gain map contains invalid values
 */
XPE_API XpeErrorCode xpe_gain_correct(const XpeImageBuffer* input,
                                      XpeImageBuffer* output,
                                      const XpeImageMetadata* metadata);

/**
 * @brief Execute defect correction using edge-aware bilinear interpolation
 *
 * REQ-P1A-012: Defect correction with 5x5 neighborhood
 * AC-DEF-001: Edge-aware bilinear interpolation excluding center
 * AC-DEF-002: Static BPM priority over runtime detection
 * AC-DEF-003: Runtime transient defect detection
 * REQ-P1A-021: Validate dimension mismatch
 *
 * @param input Input image buffer (gain-corrected, FLOAT32)
 * @param output Output image buffer (defect-corrected, FLOAT32)
 * @param metadata Image metadata for dose-dependent threshold
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers
 *         XPE_ERR_BUFFER_TOO_SMALL if dimension mismatch
 */
XPE_API XpeErrorCode xpe_defect_correct(const XpeImageBuffer* input,
                                        XpeImageBuffer* output,
                                        const XpeImageMetadata* metadata);

/* =============================================================================
 * Phase 4: Calibration Management (REQ-P1A-017~019, AC-CAL-004~005)
 * ============================================================================ */

/**
 * @brief Generate offset calibration map from dark frames
 *
 * REQ-P1A-017: Generate dark frames with configurable parameters
 *
 * @param dark_frames Array of dark frame images
 * @param num_frames Number of dark frames to average
 * @param integration_time_ms Integration time in milliseconds
 * @param temperature_c Temperature in Celsius
 * @param output_path Output XCal file path for generated offset map
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers or invalid parameters
 *         XPE_ERR_IO_FAILED on file write error
 */
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* dark_frames,
                                               int32_t num_frames,
                                               float integration_time_ms,
                                               float temperature_c,
                                               const char* output_path);

/**
 * @brief Generate flat-field gain map from flat frames (FUNC-026)
 *
 * SWU-1.12: Generate gain calibration map with dark subtraction and normalization
 *
 * Algorithm:
 *   1. Dark subtraction: flat_corr[i] = flat_frames[i] - dark_reference
 *   2. Pixel-wise mean: G_raw(x,y) = mean(flat_corr[0..N-1][x,y])
 *   3. Normalize: G(x,y) = G_raw(x,y) / mean(G_raw)
 *   4. Single-frame mode: compute uncertainty and store in metadata
 *
 * @param flat_frames Array of flat-field images (N × W × H)
 * @param num_frames Number of flat-field frames (≥ 1)
 * @param dark_reference Dark frame for subtraction (may be NULL for no subtraction)
 * @param output_path Output XCal file path for generated gain map
 * @param metadata_json Optional metadata JSON (kVp, mAs, SID, etc.)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if NULL pointers or invalid parameters
 *         XPE_ERR_IO_FAILED on file write error
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 */
XPE_API XpeErrorCode xpe_calib_generate_gain(const XpeImageBuffer* flat_frames,
                                             int32_t num_frames,
                                             const XpeImageBuffer* dark_reference,
                                             const char* output_path,
                                             const char* metadata_json);

/**
 * @brief Generate dose-dependent gain polynomial (FUNC-027)
 *
 * SWU-1.12: Generate gain polynomial G(x,y,E) = c0 + c1*E + c2*E² + ...
 *
 * Algorithm:
 *   1. Load N gain maps from FUNC-026 output files
 *   2. For each pixel: fit polynomial via least-squares
 *   3. Validate monotonicity in [dose_min, dose_max]
 *   4. Reduce degree if non-monotone (min degree = 1)
 *   5. Store coefficient array: (d+1) × W × H
 *
 * @param gain_file_paths Array of N gain file paths (from FUNC-026)
 * @param dose_levels Array of N dose levels (mGy or relative units)
 * @param num_levels Number of dose levels (≥ 3)
 * @param max_degree Maximum polynomial degree (1 ≤ max_degree ≤ 4)
 * @param output_path Output XCal file path for gain polynomial
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if NULL pointers or invalid parameters
 *         XPE_ERR_IO_FAILED on file read/write error
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 *         XPE_ERR_PROCESSING_FAILED on polynomial fitting failure
 */
XPE_API XpeErrorCode xpe_calib_generate_gain_polynomial(const char** gain_file_paths,
                                                        const double* dose_levels,
                                                        int32_t num_levels,
                                                        int32_t max_degree,
                                                        const char* output_path);

/**
 * @brief Check calibration expiry status
 *
 * REQ-P1A-018: Check expiry based on timestamp and drift scoring
 * AC-CAL-004: Compare current time to expires_at
 *
 * @param filepath Path to calibration file to check
 * @param is_expired Output: true if expired, false otherwise
 * @param remaining_days Output: Days until expiry (negative if expired)
 * @return XPE_OK on success
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CONFIG_INVALID if file format invalid
 */
XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filepath,
                                            bool* is_expired,
                                            int32_t* remaining_days);

/**
 * @brief Save current calibration state to XCal format
 *
 * REQ-P1A-019: Save calibration with SHA-256 integrity
 *
 * @param filepath Output XCal file path
 * @param calib_type Calibration type (offset, gain, defect)
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_IO_FAILED on file write error
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 */
XPE_API XpeErrorCode xpe_calib_save(const char* filepath,
                                    const char* calib_type);

/* =============================================================================
 * Phase 5: Utilities (REQ-P1A-013, REQ-P1A-042)
 * ============================================================================ */

/**
 * @brief Detect transient defects at runtime
 *
 * REQ-P1A-013: Runtime defect detection with dose-dependent threshold
 * AC-DEF-003: Merge with static BPM
 *
 * @param image Image buffer to analyze
 * @param metadata Image metadata for dose information
 * @param defect_map_output Output defect map (merged with static BPM)
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers
 */
XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* image,
                                               const XpeImageMetadata* metadata,
                                               XpeImageBuffer* defect_map_output);

/**
 * @brief Query valid parameter ranges for calibration
 *
 * REQ-P1A-042: Return valid ranges for calibration parameters
 *
 * @param param_name Parameter name (e.g., "integration_time_ms", "temperature_c")
 * @param min_value Output: Minimum valid value
 * @param max_value Output: Maximum valid value
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if param_name not recognized
 */
XPE_API XpeErrorCode xpe_preprocess_get_param_range(const char* param_name,
                                                    float* min_value,
                                                    float* max_value);

/* =============================================================================
 * Phase 6: Ghost/Lag Correction (REQ-P1A-029 to REQ-P1A-034)
 * SWU-1.4: Ghost/Lag Correction Tier 1/2/3 — LTI/NLCSC deconvolution (PRE-04)
 * ============================================================================ */

/**
 * @brief Allocate an opaque ghost corrector handle with frame history buffer
 *
 * REQ-P1A-029: Allocate handle with frame history buffer
 * REQ-P1A-030: Config JSON for IRF coefficient override
 * REQ-P1A-031: Return XPE_ERR_OUT_OF_MEMORY on allocation failure
 *
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param configJsonOrNull Optional JSON configuration for tier/IRF coefficients
 * @param handleOut Output: Opaque handle pointer (caller must call xpe_ghost_destroy)
 * @return XPE_OK on success
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 *         XPE_ERR_INVALID_INPUT on NULL handleOut or zero dimensions
 *
 * @note Handle is NOT thread-safe; do not share across threads
 */
XPE_API XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                                       const char* configJsonOrNull,
                                       void** handleOut);

/**
 * @brief Apply LTI lag correction using dual-exponential IRF model
 *
 * REQ-P1A-032: Apply LTI deconvolution (Tier 1/2/3)
 * REQ-P1A-033: Compute time delta in units of frames
 *
 * @param handle Ghost corrector handle (from xpe_ghost_create)
 * @param img [in/out] Image to correct (float32 format)
 * @param meta Image metadata (acquisitionTime used for IRF timing)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL/invalid handle or dimension mismatch
 *         XPE_ERR_PROCESSING_FAILED on numerical errors
 */
XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                        const XpeImageMetadata* meta);

/**
 * @brief Clear accumulated frame history without destroying the handle
 *
 * REQ-P1A-034: Clear accumulated frame history
 * Call between patient acquisitions or after detector power cycle.
 *
 * @param handle Ghost corrector handle
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL/invalid handle
 */
XPE_API XpeErrorCode xpe_ghost_reset(void* handle);

/**
 * @brief Free all resources associated with a ghost corrector handle
 *
 * After this call the handle is invalid (do not pass to any other function).
 *
 * @param handle Ghost corrector handle to destroy (may be NULL, no-op)
 */
XPE_API void xpe_ghost_destroy(void* handle);

/* =============================================================================
 * Phase 7: Temperature, Nonlinearity, and Binning Correction
 * SWU-1.6: Temperature Compensation (PRE-07)
 * SWU-1.7: Nonlinearity Correction (PRE-08)
 * SWU-1.8: Binning Correction (PRE-09)
 * ============================================================================ */

/**
 * @brief Adjust pixel values for dark current temperature dependence
 *
 * REQ-P1A-005: Apply temperature-dependent dark current scaling
 * REQ-P1A-007: NaN -> use 25.0C fallback
 * REQ-P1A-008: Temp out of [-20, +60] range -> XPE_ERR_INVALID_INPUT
 * Model: I_dark(T) = I_0 * exp(-E_g / (2 * k_B * T))
 *
 * @param img [in/out] Image to correct (uint16 format)
 * @param detectorTempC Detector temperature in Celsius; NaN -> use 25.0C fallback
 * @param configJsonOrNull Optional calibration coefficient override JSON
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if NULL img or temp out of range
 */
XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
                                          float detectorTempC,
                                          const char* configJsonOrNull);

/**
 * @brief Apply piecewise linear or polynomial correction to linearize detector response
 *
 * REQ-P1A-012: Apply nonlinearity correction
 * REQ-P1A-013: No-op when no config supplied
 * REQ-P1A-014: Unknown mode -> XPE_ERR_CONFIG_INVALID
 * REQ-P1A-015: Identity polynomial for baseline
 *
 * @param img [in/out] Image to correct (uint16 format)
 * @param configJsonOrNull Optional detector mode/coefficient override JSON
 * @return XPE_OK on success
 *         XPE_ERR_CONFIG_INVALID if unknown detector mode
 *         XPE_ERR_INVALID_INPUT if NULL img
 */
XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                               const char* configJsonOrNull);

/**
 * @brief Apply per-mode binning correction for gain/uniformity differences
 *
 * REQ-P1A-020: No-op for binningMode == 1
 * REQ-P1A-021: XPE_ERR_CONFIG_INVALID for unknown binning mode
 * REQ-P1A-022: Float32 format (post-gain-correct stage)
 * REQ-P1A-023: Per-mode correction profile
 *
 * @param img [in/out] Image to correct (float32 format)
 * @param binningMode Binning factor (1 = no-op, 2 = 2x2, 4 = 4x4)
 * @param configJsonOrNull Optional correction profile JSON
 * @return XPE_OK on success
 *         XPE_ERR_CONFIG_INVALID if unknown binning mode
 *         XPE_ERR_INVALID_INPUT if NULL img
 */
XPE_API XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                          int32_t binningMode,
                                          const char* configJsonOrNull);

/* =============================================================================
 * Phase 8: Full Pre-Processing Pipeline (REQ-P1A-041 to REQ-P1A-047)
 * SWU-1.9: Readout Artifact Validation (PRE-01)
 * Pipeline stages: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost
 * ============================================================================ */

/**
 * @brief Validate raw uint16 image for readout artifacts
 *
 * REQ-P1A-041: Validate readout artifacts before correction
 * Call BEFORE any correction stage.
 *
 * @param image Raw uint16 image to validate
 * @param metadata Image metadata (acquisition context)
 * @param has_dropped_columns Output: true if any all-zero column detected
 * @param has_nonuniform_gain Output: true if any row mean > 0.9 * UINT16_MAX
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers
 */
XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* image,
                                                   const XpeImageMetadata* metadata,
                                                   bool* has_dropped_columns,
                                                   bool* has_nonuniform_gain);

/**
 * @brief Execute full pre-processing pipeline with bypass logic
 *
 * REQ-P1A-041 to REQ-P1A-047: Full pipeline integration
 * Pipeline: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost
 *
 * @param img [in/out] Image to process (uint16 in, float32 out after Gain)
 * @param meta [in/out] Image metadata (updated with processing flags)
 * @param calibPath Calibration data directory path
 * @param ghostHandle Ghost corrector handle (NULL = skip ghost correction)
 * @param configJsonOrNull Pipeline configuration JSON (bypass flags, temperature, etc.)
 * @return XPE_OK on success
 *         XPE_ERR_* on failure
 */
XPE_API XpeErrorCode xpe_preprocess_pipeline(XpeImageBuffer* img,
                                              XpeImageMetadata* meta,
                                              const char* calibPath,
                                              void* ghostHandle,
                                              const char* configJsonOrNull);

/**
 * @brief Execute full pre-processing pipeline using pre-loaded calibration maps
 *
 * Extended version of xpe_preprocess_pipeline() that skips file I/O by using
 * calibration data from a pre-loaded XpeCalibrationState.
 *
 * @param img [in/out] Image to process
 * @param meta [in/out] Image metadata
 * @param calibState Pre-loaded calibration state (from xpe_calib_state_load)
 * @param ghostHandle Ghost corrector handle (NULL = skip ghost)
 * @param configJsonOrNull Pipeline configuration JSON
 * @return XPE_OK on success
 *         XPE_ERR_* on failure
 */
XPE_API XpeErrorCode xpe_preprocess_pipeline_ex(XpeImageBuffer* img,
                                                  XpeImageMetadata* meta,
                                                  const void* calibState,
                                                  void* ghostHandle,
                                                  const char* configJsonOrNull);

/**
 * @brief Process multiple frames with identical calibration in batch
 *
 * All frames share the same calibration maps (offset/gain/defect).
 * Optimized for AVX2 parallel processing of frames.
 *
 * @param images [in/out] Array of imageCount XpeImageBuffer to process
 * @param imageCount Number of images in the array (must be >= 1)
 * @param metas [in/out] Array of imageCount XpeImageMetadata
 * @param calibPath Calibration data directory path
 * @param ghostHandle Ghost corrector handle (NULL = skip ghost)
 * @param configJsonOrNull Pipeline configuration JSON
 * @return XPE_OK if all images processed successfully
 *         XPE_ERR_INVALID_INPUT on null/invalid parameters
 *         first error code if any individual frame fails
 */
XPE_API XpeErrorCode xpe_preprocess_pipeline_batch(
    XpeImageBuffer* images,
    uint32_t imageCount,
    XpeImageMetadata* metas,
    const char* calibPath,
    void* ghostHandle,
    const char* configJsonOrNull);

/* =============================================================================
 * Phase 9: Calibration Data Caching and State Management
 * SWU-1.10: Calibration Data Caching (LRU)
 * SWU-1.11: Pre-loaded Calibration State (Pipeline Optimization)
 * ============================================================================ */

/**
 * @brief Load offset calibration map with LRU caching
 *
 * On cache hit, returns cached data without file I/O.
 * On miss, loads from file and inserts into cache.
 *
 * @param filePath Path to calibration file
 * @param offsetMapOut Output: Populated by this function; caller owns the buffer.
 *                     Data pointer is shared with cache — do NOT free.
 * @return XPE_OK on success
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 */
XPE_API XpeErrorCode xpe_calib_load_offset_cached(const char* filePath,
                                                    XpeImageBuffer* offsetMapOut);

/**
 * @brief Load gain calibration map with LRU caching
 *
 * @param filePath Path to calibration file
 * @param gainMapOut Output: Populated on hit or miss; data shared with cache.
 * @return XPE_OK on success
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 */
XPE_API XpeErrorCode xpe_calib_load_gain_cached(const char* filePath,
                                                  XpeImageBuffer* gainMapOut);

/**
 * @brief Load defect map with LRU caching
 *
 * @param filePath Path to defect map file
 * @param defectMapOut Output: Populated on hit or miss; data shared with cache.
 * @return XPE_OK on success
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 */
XPE_API XpeErrorCode xpe_calib_load_defect_cached(const char* filePath,
                                                    XpeImageBuffer* defectMapOut);

/**
 * @brief Clear all entries from the calibration cache, freeing memory
 */
XPE_API void xpe_calib_cache_clear(void);

/**
 * @brief Set the maximum number of calibration maps retained in cache
 *
 * Default is 4. Excess entries are evicted (LRU first).
 *
 * @param maxMaps Maximum cache entries (minimum 1)
 */
XPE_API void xpe_calib_cache_set_max_size(uint32_t maxMaps);

/**
 * @brief Load all calibration maps from a directory into a state struct
 *
 * Files expected: offset.xcal, gain.xcal, defect.xcal
 * Missing files are silently skipped (corresponding *Loaded flag = false).
 *
 * @param state [out] Zero-initialized state to populate
 * @param calibPath Calibration data directory path
 * @return XPE_OK on success (at least one map loaded)
 *         XPE_ERR_INVALID_INPUT on null parameters
 */
XPE_API XpeErrorCode xpe_calib_state_load(void* state, const char* calibPath);

/**
 * @brief Free all resources held by a calibration state struct
 *
 * Safe to call on zero-initialized or already-released state.
 *
 * @param state [in/out] Calibration state to release
 */
XPE_API void xpe_calib_state_release(void* state);

/* =============================================================================
 * Phase 11: Calibration Verification Metrics (Quality Assessment)
 * SWU-1.12: Verification Metrics API (PRE-11)
 * ============================================================================ */

/**
 * @brief Quantitative metrics for calibration quality assessment
 *
 * Populated by verification functions to enable automated pass/fail determination.
 */
typedef struct {
    // Offset metrics
    double dark_bias;           ///< Mean of corrected dark region (ADU, should → 0)
    double dsnu;                ///< Dark Signal Non-Uniformity: std/mean × 100 [%]
    double residual_noise;      ///< Standard deviation of corrected dark pixels

    // Gain metrics
    double prnu_before;         ///< Photo Response Non-Uniformity before gain [%]
    double prnu_after;          ///< PRNU after gain correction [%]
    double flatness_pct;        ///< Histogram flatness percentage (higher = more uniform)
    double gain_coverage;       ///< % of valid gain values (1.0 = all valid)
    uint32_t invalid_gain_count;///< Number of invalid gain pixels

    // Defect/BPM metrics
    uint32_t defect_count;      ///< Number of defective pixels found
    double   defect_density;    ///< defects / total pixels × 100 [%]
    double   correction_error;  ///< Mean absolute difference between corrected pixels and neighbor mean

    // Overall
    double snr_improvement_db;  ///< SNR improvement in dB (before vs after)
    bool   overall_pass;        ///< Overall pass/fail based on thresholds
} XpeCalibrationMetrics;

/**
 * @brief Verify offset correction quality
 *
 * Computes dark bias, DSNU, and residual noise metrics for offset-corrected images.
 * Dark bias measures how well offset correction removes the dark current pedestal.
 * DSNU measures residual non-uniformity in dark regions.
 *
 * REQ-P1A-XXX: Offset correction verification
 *
 * @param raw_image Original raw image (UINT16)
 * @param corrected_image Offset-corrected image (UINT16)
 * @param metadata Image metadata
 * @param metrics Output metrics (populated by this function)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 *         XPE_ERR_UNSUPPORTED_FORMAT on format mismatch
 */
XPE_API XpeErrorCode xpe_verify_offset(
    const XpeImageBuffer* raw_image,
    const XpeImageBuffer* corrected_image,
    const XpeImageMetadata* metadata,
    XpeCalibrationMetrics* metrics);

/**
 * @brief Verify gain correction quality
 *
 * Computes PRNU before/after, flatness, gain coverage, and SNR improvement.
 * PRNU (Photo Response Non-Uniformity) measures pixel-to-pixel gain variation.
 * Flatness measures histogram uniformity (ideal flat-field response).
 *
 * REQ-P1A-XXX: Gain correction verification
 *
 * @param before_gain Offset-corrected image (UINT16)
 * @param after_gain Gain-corrected image (FLOAT32)
 * @param gain_map Gain map used (FLOAT32)
 * @param metrics Output metrics
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 *         XPE_ERR_UNSUPPORTED_FORMAT on format mismatch
 */
XPE_API XpeErrorCode xpe_verify_gain(
    const XpeImageBuffer* before_gain,
    const XpeImageBuffer* after_gain,
    const XpeImageBuffer* gain_map,
    XpeCalibrationMetrics* metrics);

/**
 * @brief Verify defect correction quality
 *
 * Computes defect count, density, and correction error metrics.
 * Correction error measures how well defective pixels are interpolated from neighbors.
 *
 * REQ-P1A-XXX: Defect correction verification
 *
 * @param corrected_image Defect-corrected image (FLOAT32)
 * @param defect_map BPM used (UINT8)
 * @param metrics Output metrics
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 *         XPE_ERR_UNSUPPORTED_FORMAT on format mismatch
 */
XPE_API XpeErrorCode xpe_verify_defect(
    const XpeImageBuffer* corrected_image,
    const XpeImageBuffer* defect_map,
    XpeCalibrationMetrics* metrics);

/**
 * @brief Verify full pipeline quality
 *
 * Computes overall SNR improvement between raw and final processed images.
 * Provides end-to-end quality assessment for the entire preprocessing pipeline.
 *
 * REQ-P1A-XXX: Pipeline verification
 *
 * @param raw_image Original raw image (UINT16)
 * @param final_image Final processed image (FLOAT32)
 * @param metadata Image metadata
 * @param metrics Combined metrics (snr_improvement_db and overall_pass populated)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 *         XPE_ERR_UNSUPPORTED_FORMAT on format mismatch
 */
XPE_API XpeErrorCode xpe_verify_pipeline(
    const XpeImageBuffer* raw_image,
    const XpeImageBuffer* final_image,
    const XpeImageMetadata* metadata,
    XpeCalibrationMetrics* metrics);

/* =============================================================================
 * CRC-32 Utility (for calibration integrity)
 * ============================================================================ */

/**
 * @brief Compute CRC-32 checksum using ISO-HDLC polynomial (0xEDB88320)
 *
 * Used internally by calibration manager for file integrity verification.
 *
 * @param data Data buffer to checksum
 * @param len Length of data in bytes
 * @return CRC-32 checksum value
 */
XPE_API uint32_t xpe_crc32(const uint8_t* data, size_t len) noexcept;

#ifdef __cplusplus
}
#endif

/* =============================================================================
 * Phase 12: BPM (Bad Pixel Map) Generation (SWU-1.11)
 * FUNC-022: Dark BPM generation using RMM (Robust Mask Maker)
 * FUNC-023: Bright BPM generation using local mean deviation
 * FUNC-024: BPM merging (dark U bright)
 * FUNC-025: Reflect padding for boundary handling
 * ============================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BPM generation configuration
 *
 * Default values:
 * - lambda_dark: 8.0 (RMM threshold multiplier)
 * - mask_size_dark: 32 (local window for dark detection)
 * - tolerance_pct: 0.07 (7% tolerance for bright detection)
 * - mask_size_bright: 128 (local window for bright detection)
 * - min_frames_dark: 5 (minimum dark frames required)
 * - min_frames_bright: 10 (minimum bright frames required)
 *
 * Validation limits:
 * - lambda_dark: must be > 0
 * - mask_size_dark: minimum 32
 * - tolerance_pct: range [0.05, 0.09]
 * - mask_size_bright: minimum 128
 * - min_frames_dark/bright: must be > 0
 */
typedef struct {
    float    lambda_dark;       ///< RMM lambda for dark detection (default: 8.0)
    uint32_t mask_size_dark;    ///< Local window side length (default: 32, min: 32)
    float    tolerance_pct;     ///< Flat-field tolerance fraction (default: 0.07, range: 0.05~0.09)
    uint32_t mask_size_bright;  ///< Local window side length (default: 128, min: 128)
    uint32_t min_frames_dark;   ///< Minimum dark frames required (default: 5)
    uint32_t min_frames_bright; ///< Minimum bright frames required (default: 10)
} XpeBpmConfig;

/**
 * @brief Generate BPM (Bad Pixel Map) from dark and bright frames
 *
 * FUNC-022: Dark BPM Generation
 *   Uses RMM (Robust Mask Maker) with adaptive local statistics:
 *   - Compute dark_mean = average(dark_frames)
 *   - For each pixel: extract mask_size_dark × mask_size_dark window
 *   - Compute local median M and MAD σ_r = 1.4826 × median(|w_i - M|)
 *   - Flag if |dark_mean[x,y] - M| > lambda_dark × σ_r
 *
 * FUNC-023: Bright BPM Generation
 *   - Compute bright_mean = average(bright_frames)
 *   - For each pixel: extract mask_size_bright × mask_size_bright window
 *   - Compute maskAvg = mean(window)
 *   - Flag if |bright_mean[x,y] - maskAvg| > maskAvg × tolerance_pct
 *
 * FUNC-024: BPM Merging
 *   - final_bpm[x,y] = max(dark_bpm[x,y], bright_bpm[x,y])
 *   - Values: 0=good, 1=dead/stuck, 2=hot/noisy, 3=both
 *
 * FUNC-025: Reflect Padding
 *   - Window extraction uses reflect mode at boundaries
 *   - Prevents false detections at image borders
 *
 * REQ-P1A-XXX: BPM generation for FPD calibration
 *
 * @param dark_frames Array of dark frames (UINT16, offset-uncorrected)
 * @param num_dark Number of dark frames (≥ min_frames_dark)
 * @param bright_frames Array of bright/flat-field frames (UINT16)
 * @param num_bright Number of bright frames (≥ min_frames_bright)
 * @param cfg Algorithm configuration (NULL = use defaults)
 * @param bpm_out Output BPM (UINT8, 0=good, 1=dead/stuck, 2=hot/noisy, 3=both)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or invalid parameters
 *         XPE_ERR_BUFFER_TOO_SMALL if output buffer too small
 *         XPE_ERR_UNSUPPORTED_FORMAT if output format is not UINT8
 */
XPE_API XpeErrorCode xpe_bpm_generate(
    const XpeImageBuffer* dark_frames,
    uint32_t num_dark,
    const XpeImageBuffer* bright_frames,
    uint32_t num_bright,
    const XpeBpmConfig* cfg,
    XpeImageBuffer* bpm_out);

#ifdef __cplusplus
}
#endif

/* =============================================================================
 * Phase 13: Calibration Mode Selection (FUNC-031~033)
 * SWU-1.12: Calibration Mode Selection API (PRE-13)
 * ============================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calibration mode selection for polynomial fitting
 *
 * Defines the number of dose points and polynomial degree for gain calibration.
 *
 * Mode specifications:
 * - SINGLE_POINT: 1 point, degree 0 (constant)
 * - DUAL_POINT: 2 points, degree 1 (linear)
 * - MULTI_POINT_5: 5 points, degree 2 (quadratic)
 * - MULTI_POINT_8: 8 points, degree 3 (cubic) — DEFAULT per Schmidgunst 2007
 * - MULTI_POINT_10: 10 points, degree 3 (cubic)
 * - AUTO: Adaptive selection (max 10 points, degree 3)
 */
typedef enum XpeCalibrationMode {
    XPE_CALIB_SINGLE_POINT   = 0,  ///< 1 point, constant fit
    XPE_CALIB_DUAL_POINT     = 1,  ///< 2 points, linear fit
    XPE_CALIB_MULTI_POINT_5  = 2,  ///< 5 points, quadratic fit
    XPE_CALIB_MULTI_POINT_8  = 3,  ///< 8 points, cubic fit (DEFAULT)
    XPE_CALIB_MULTI_POINT_10 = 4,  ///< 10 points, cubic fit
    XPE_CALIB_AUTO           = 5   ///< Adaptive mode (max 10 points)
} XpeCalibrationMode;

/**
 * @brief Quality metadata for calibration output
 *
 * Populated by calibration generation functions to enable quality assessment
 * and historical comparison.
 *
 * Fields:
 * - calibration_mode: Active calibration mode (XpeCalibrationMode)
 * - polynomial_degree: Fitted polynomial degree (0-3)
 * - num_points: Number of dose points used (1-10)
 * - r_squared: Coefficient of determination (0.0 to 1.0)
 * - calibration_timestamp: Unix epoch milliseconds
 * - detector_serial: Detector identifier (null-terminated)
 * - firmware_version: Firmware version string (null-terminated)
 * - calibration_pass: Quality gate result (0=fail, 1=pass)
 * - previous_r_squared: R² from previous calibration (-1.0 if none)
 */
typedef struct XpeCalibQualityMeta {
    uint8_t  calibration_mode;      ///< XpeCalibrationMode value
    uint8_t  polynomial_degree;     ///< 0=constant, 1=linear, 2=quadratic, 3=cubic
    uint8_t  num_points;            ///< Number of dose levels (1-10)
    double   r_squared;             ///< Coefficient of determination (0.0 to 1.0)
    uint64_t calibration_timestamp; ///< Unix epoch milliseconds
    char     detector_serial[32];   ///< Detector serial number (null-terminated)
    char     firmware_version[16];  ///< Firmware version (null-terminated)
    uint8_t  calibration_pass;      ///< 0=failed R² gate, 1=passed
    double   previous_r_squared;    ///< Previous calibration R² (-1.0 if none)
} XpeCalibQualityMeta;

/**
 * @brief Set calibration mode for polynomial fitting
 *
 * FUNC-031: Mode Selection API
 *
 * Default mode: XPE_CALIB_MULTI_POINT_8 (8 points, cubic)
 *
 * @param mode Calibration mode to set
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if mode value is invalid
 */
XPE_API XpeErrorCode xpe_calib_set_mode(XpeCalibrationMode mode);

/**
 * @brief Get current calibration mode
 *
 * FUNC-031: Mode Selection API
 *
 * @return Current calibration mode (default: XPE_CALIB_MULTI_POINT_8)
 */
XPE_API XpeCalibrationMode xpe_calib_get_mode(void);

/**
 * @brief Get quality metadata from last calibration
 *
 * FUNC-033: Quality Metadata API
 *
 * Returns the quality metadata structure populated during the last
 * calibration generation operation.
 *
 * @param meta Output: Quality metadata (caller-owned)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if meta is NULL
 */
XPE_API XpeErrorCode xpe_calib_get_quality_meta(XpeCalibQualityMeta* meta);

/**
 * @brief Get maximum number of dose points for current mode
 *
 * FUNC-031: Mode-to-params mapping
 *
 * @return Maximum dose points (1, 2, 5, 8, or 10)
 */
XPE_API uint32_t xpe_calib_get_max_points(void);

/**
 * @brief Get polynomial degree for current mode
 *
 * FUNC-031: Mode-to-params mapping
 *
 * @return Polynomial degree (0, 1, 2, or 3)
 */
XPE_API uint32_t xpe_calib_get_poly_degree(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_PREPROCESS_API_H */
