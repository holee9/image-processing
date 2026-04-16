/**
 * @file preprocess_api.h
 * @brief XPE Preprocessing Module API (SPEC-XPE-P1A)
 *
 * Phase 1: Lifecycle (2 functions)
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
 * Phase 4: Calibration Management (4 functions)
 * - xpe_calib_generate_offset()
 * - xpe_calib_check_expiry()
 * - xpe_calib_save()
 * - xpe_validate_readout_artifact()
 *
 * Phase 5: Utilities (2 functions)
 * - xpe_defect_detect_runtime()
 * - xpe_preprocess_get_param_range()
 *
 * Total: 14 API functions
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

/**
 * @brief Validate and mask readout artifacts
 *
 * REQ-P1A-041: Validate readout artifacts before correction
 *
 * @param image Image buffer to validate
 * @param metadata Image metadata
 * @param has_dropped_columns Output: true if dropped columns detected
 * @param has_nonuniform_gain Output: true if gain nonuniformity detected
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if NULL pointers
 */
XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* image,
                                                   const XpeImageMetadata* metadata,
                                                   bool* has_dropped_columns,
                                                   bool* has_nonuniform_gain);

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

#ifdef __cplusplus
}
#endif

#endif /* XPE_PREPROCESS_API_H */
