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
 * - \#pragma pack(push, 8) for C# P/Invoke compatibility
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
 *
 * @return Null-terminated version string. Lifetime: process. Never NULL.
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
 *         XPE_ERR_CONFIG_INVALID if the text is not one valid JSON object (empty or white space only, an array,
 *                                text after the object, a top-level key given twice -- the reading rule of
 *                                xpe_preprocess_pipeline; NULL is the way to say "no configuration")
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 */
XPE_API XpeErrorCode xpe_preprocess_init(const char* config);

/**
 * @brief Shutdown preprocessing module and release resources
 *
 * REQ-P1A-031: No memory leak after shutdown
 * REQ-P1A-003: Thread-safe for concurrent shutdown
 *
 * Safe to call multiple times. After shutdown, the module is in the
 * uninitialized state and can be re-initialized.
 *
 * Releases the loaded offset map, gain map or polynomial gain coefficients,
 * and defect map, whether or not the module
 * was initialized. The calibration loaders do not require initialization, so
 * maps may be loaded before xpe_preprocess_init(); a shutdown at that point
 * releases them too (it is not a no-op).
 *
 * Clears EVERY module global: the calibration maps, the calibration mode set by
 * xpe_calib_set_mode() (back to the FUNC-031 default XPE_CALIB_MULTI_POINT_8),
 * and the quality metadata returned by xpe_calib_get_quality_meta().
 *
 * Until QA-A-120 (#176) it cleared only the maps and left the other two alive
 * across shutdown and re-initialization. That was not a contract but an
 * omission the header had been updated to describe (QA-A-90 aligned the text to
 * the behaviour rather than the other way round): a caller reads "shutdown" and
 * is entitled to a module in its start-up state, and a function that clears one
 * of three globals has a name that describes less than it does.
 */
XPE_API void xpe_preprocess_shutdown(void);

/**
 * @brief Report whether the module is currently initialized
 *
 * Read-only: it changes nothing, which is what separates it from calling
 * xpe_preprocess_init() and reading its error code.
 *
 * @return true while the module is up (between a successful
 *         xpe_preprocess_init() and the matching xpe_preprocess_shutdown()).
 */
XPE_API bool xpe_preprocess_is_initialized(void);

/* =============================================================================
 * Phase 2: Calibration Loading Functions (REQ-P1A-014~016, AC-CAL-001~003)
 * ============================================================================ */

/**
 * @brief Load offset calibration map from XCal file
 *
 * REQ-P1A-014: Load XCal format offset maps
 * AC-CAL-001: Validate SHA-256, check session consistency with the other loaded maps, verify expiry
 *
 * Session consistency (SRS-CALIB-FUNC-011, QA-A-229 M4): the offset, gain and defect maps in the store must come
 * from the same session. Each file carries a session_id; of two maps that both carry one (non-empty, not the
 * generator's literal "generated"), a different id is XPE_ERR_CONFIG_INVALID. The map that arrives second is the
 * one refused: the loaded maps stay and nothing else changes (a cached loader's hit gives the same verdict). A map
 * with no session id is left out of the comparison -- every generated file and every file written before this
 * check is in that case -- and ONE warning "XPE_WARN_CALIB_SESSION_UNSPECIFIED: ..." (XPE_ALERT_WARNING) is
 * raised per mixed state, not per load. What this does NOT do: compare against a session the caller names (no
 * such call exists) -- two maps from the same wrong detector pass -- and xpe_calib_session_create (the other half
 * of FUNC-011) is not implemented (#245). To switch to another session, clear the store first
 * (xpe_preprocess_shutdown, then xpe_preprocess_init): loading a map of the new session while maps of the old one
 * are loaded is refused.
 *
 * @param filepath Path to XCal format offset file
 * @return XPE_OK on success
 *         (never XPE_ERR_NOT_INITIALIZED: a map may be loaded before xpe_preprocess_init; the processing
 *          functions are what refuse an uninitialized module -- pinned by CalibLoadTest.LoadBeforeInit_AllThreeLoadersAcceptValidFiles)
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 *         XPE_ERR_CONFIG_INVALID if the file's session id conflicts with a loaded gain or defect map
 *                                (see "Session consistency" above)
 */
XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath);

/**
 * @brief Load gain calibration map from XCal file
 *
 * REQ-P1A-015: kVp-/SID-specific gain with interpolation -- NOT IMPLEMENTED (REQ-015, #245). The loader
 *              reads ONE gain map (or one gain polynomial in dose); there is no table indexed by kVp or SID.
 * AC-CAL-002: not implemented, same reason.
 * What kVp does today: nothing. metadata.kVp and metadata.SID_mm are not read when a gain map is chosen or
 * applied, so the same frame under different kVp/SID gives identical pixels (pinned by
 * GainPolyNotAppliedTest.KvpAndSidDoNotChangeTheCorrection).
 *
 * Quality metadata (FUNC-033). The file's config block is read as ONE valid JSON object, to its stored length
 * (a NUL byte, a byte-order mark that is not complete, malformed UTF-8, text after the object, a key given twice at
 * the top level: all XPE_ERR_CONFIG_INVALID), and its fit_r_squared, polynomial_degree, actual_dose_levels and
 * calibration_mode are TOP-LEVEL keys (a key inside a nested object is not one; a polynomial file's dose_min and dose_max are read the same way, and
 * given twice they are refused). A key that is ABSENT means "not
 * given" (a file from before QA-A-35 has none of them; for fit_r_squared the record then has has_r_squared = 0).
 * A key that is PRESENT must hold a number in the range the field has -- fit_r_squared a finite real of at most 1.0
 * (negative values are real: a fit worse than the mean; exactly -1.0 included), polynomial_degree an integer in
 * 0..4, actual_dose_levels 1..10, calibration_mode 0..4 (notation: optional leading white space, an optional
 * single '+', then a decimal number that fills the value) -- and an empty value ("fit_r_squared":""), a value that
 * is not a scalar (an object, an array), an unterminated string, a malformed number and a number outside its range
 * are all refused with XPE_ERR_CONFIG_INVALID, leaving the gain map and the quality metadata as they were. A file
 * that carries no quality field at all loads, and makes the quality record "none" (valid = 0; see
 * xpe_calib_get_quality_meta) instead of leaving the previous file's values in place. (The pipeline
 * CONFIGURATION is the other rule: there an empty value is an absent one, because a GUI sends an unset option
 * as "" -- an XCal file is signed data a generator produced, where an empty field is a defect, not an unset
 * option.) Compatibility: the values "2x", "3 " and "1e2" (for an integer field) were once read as 2, 3 and 1
 * by atoi/atof and now refuse the file -- an intended policy (QA-A-204, QA-A-205b): a quality field that is not
 * a number is not data. The generator (xpe_calib_generate_gain) writes plain numbers.
 *
 * Gain range and failed pixels (SRS-CALIB-FUNC-002, QA-A-211): a scalar map's values shall be in [0.1, 10.0]. A pixel
 * outside it -- a failed pixel, or the low-sensitivity edge band some detectors have -- does NOT refuse the map: it is
 * CLASSIFIED DEFECTIVE. Its gain is replaced by 1.0, its index is kept in the calibration store, and the defect
 * correction stage corrects it from its neighbours together with the pixels of the loaded defect map (the stage reads
 * the union of the two). The count is reported on the alert queue (XPE_ALERT_WARNING,
 * "XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: ..."), and a map with MORE than 5% of its pixels classified (the defect
 * density SRS-CALIB-FUNC-003 tolerates) is refused with XPE_ERR_INVALID_CALIB_DATA and an alert
 * "XPE_WARN_GAIN_PIXELS_OVER_LIMIT: ..." (XPE_ALERT_ERROR), the store unchanged. The same range and rule apply to a
 * gain polynomial, evaluated at each pixel value when a frame is corrected (see xpe_gain_correct). Pixels classified
 * by the gain calibration are corrected only by a defect stage that follows: with the defect stage bypassed, or with
 * xpe_gain_correct called on its own, they carry the uncorrected value (gain 1.0) and the frame says so
 * ("XPE_WARN_GAIN_PIXELS_UNCORRECTED: ..."); the pipeline refuses such a frame when binning is on.
 *
 * Session consistency: as for xpe_calib_load_offset (a conflict with a loaded offset or defect map is
 * XPE_ERR_CONFIG_INVALID, this map refused).
 *
 * @param filepath Path to XCal format gain file
 * @return XPE_OK on success
 *         (never XPE_ERR_NOT_INITIALIZED: a map may be loaded before xpe_preprocess_init; the processing
 *          functions are what refuse an uninitialized module -- pinned by CalibLoadTest.LoadBeforeInit_AllThreeLoadersAcceptValidFiles)
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CALIBRATION_EXPIRED if calibration expired
 *         XPE_ERR_CONFIG_INVALID if the config block is not one valid JSON object, or a present quality field is
 *                                not a number in its range
 *         XPE_ERR_INVALID_CALIB_DATA if more than 5% of a scalar map's pixels are outside [0.1, 10.0]
 */
XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath);

/**
 * @brief Load defect map (BPM) from XCal file
 *
 * REQ-P1A-016: Load XCal format defect maps (BPM)
 * AC-CAL-003: Validate defect locations and integrity
 *
 * Session consistency: as for xpe_calib_load_offset (a conflict with a loaded offset or gain map is
 * XPE_ERR_CONFIG_INVALID, this map refused).
 *
 * @param filepath Path to XCal format defect map file
 * @return XPE_OK on success
 *         (never XPE_ERR_NOT_INITIALIZED: a map may be loaded before xpe_preprocess_init; the processing
 *          functions are what refuse an uninitialized module -- pinned by CalibLoadTest.LoadBeforeInit_AllThreeLoadersAcceptValidFiles)
 *         XPE_ERR_IO_FAILED on file read error
 *         XPE_ERR_CONFIG_INVALID if the file's session id conflicts with a loaded offset or gain map
 */
XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath);

/* =============================================================================
 * Phase 3: Correction Algorithms (REQ-P1A-010~013, AC-OFF/GAIN/DEF-001~003)
 * ============================================================================ */

/**
 * @brief Execute offset correction: I_offset = max(I_raw - I_dark, 0)
 *
 * REQ-P1A-010: Offset correction (one offset map; temperature interpolation is NOT IMPLEMENTED -- #245)
 * AC-OFF-001: Basic offset correction with floor-at-zero
 * AC-OFF-002: Temperature interpolation between two offset maps -- NOT IMPLEMENTED (#245); XpeImageMetadata has
 *             no temperature field and one offset map is applied
 * AC-OFF-003: PREP-time exponential decay model -- NOT IMPLEMENTED (#245); acquisitionTime is not read
 * What metadata does today: nothing. kVp, SID_mm and acquisitionTime do not change the result (pinned by
 * PreprocessCorrectionTest.OffsetCorrect_MetadataDoesNotChangeTheCorrection).
 * REQ-P1A-020: Return XPE_ERR_NOT_INITIALIZED if not initialized
 * REQ-P1A-021: Validate dimension mismatch
 *
 * @param input Input image buffer (raw X-ray data, UINT16)
 * @param output Output image buffer (offset-corrected, UINT16)
 * @param metadata Image metadata including temperature and acquisition time
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers, or if the loaded calibration
 *                               map's dimensions differ from the input's (REQ-P1A-021)
 *         XPE_ERR_BUFFER_TOO_SMALL if the output's dimensions differ from the input's
 *
 * @note The map this function reads is the one in the calibration store when it is CALLED (it takes its own
 *       snapshot under the store's lock, then reads it in place; a load that lands during the call does not
 *       reach it). A caller that runs several stages by hand and loads calibration in between gets whatever
 *       was loaded at each call; the guarantee that one frame is processed with ONE calibration set belongs to
 *       the pipeline entry points (see xpe_preprocess_pipeline).
 */
XPE_API XpeErrorCode xpe_offset_correct(const XpeImageBuffer* input,
                                        XpeImageBuffer* output,
                                        const XpeImageMetadata* metadata);

/**
 * @brief Execute gain correction with UINT16→FLOAT32 conversion
 *
 * REQ-P1A-011: Gain correction with format conversion
 * AC-GAIN-001: UINT16 to FLOAT32 conversion, divide by gain map
 * AC-GAIN-002: Multi-SID gain interpolation -- NOT IMPLEMENTED (REQ-015, #245); metadata kVp / SID_mm are not used
 * AC-GAIN-003: Validate NaN/Inf values
 * REQ-P1A-021: Validate dimension mismatch
 * REQ-P1A-022: Validate format mismatch
 *
 * @param input Input image buffer (offset-corrected, UINT16)
 * @param output Output image buffer (gain-corrected, FLOAT32)
 * @param metadata Image metadata; must be non-NULL, its kVp and SID_mm do not change the result (see above)
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers, or if the loaded calibration
 *                               map's dimensions differ from the input's (REQ-P1A-021)
 *         XPE_ERR_BUFFER_TOO_SMALL if the output's dimensions differ from the input's
 *         XPE_ERR_UNSUPPORTED_FORMAT if format mismatch
 *         XPE_ERR_CONFIG_INVALID if the gain map contains invalid values, or (polynomial gain) if more than 5% of the
 *                               frame's pixels evaluate to a gain outside [0.1, 10.0] (QA-A-211)
 *
 * @note A pixel whose polynomial gain, evaluated in float32 at its own value, is outside [0.1, 10.0] (or not finite) is
 *       classified defective for this frame: gain 1.0, and the pipeline hands its index to the defect stage of the same
 *       frame. Called on its own this function cannot do that -- the pixel keeps gain 1.0 and an alert
 *       "XPE_WARN_GAIN_PIXELS_UNCORRECTED: ..." says no defect stage follows. A scalar map was classified when it was
 *       loaded (see xpe_calib_load_gain), and xpe_defect_correct reads that classification from the store.
 *
 * @note The map this function reads is the one in the calibration store when it is CALLED (it takes its own
 *       snapshot under the store's lock, then reads it in place; a load that lands during the call does not
 *       reach it). A caller that runs several stages by hand and loads calibration in between gets whatever
 *       was loaded at each call; the guarantee that one frame is processed with ONE calibration set belongs to
 *       the pipeline entry points (see xpe_preprocess_pipeline).
 */
XPE_API XpeErrorCode xpe_gain_correct(const XpeImageBuffer* input,
                                      XpeImageBuffer* output,
                                      const XpeImageMetadata* metadata);

/**
 * @brief Execute defect correction from the loaded defect map
 *
 * REQ-P1A-012: Defect correction. The description below is the shipped
 * behaviour (#125); the earlier "edge-aware bilinear, 5x5 neighbourhood"
 * wording described an algorithm this module does not implement.
 *  - Isolated defect: mean of the valid 4-connected neighbours (N/S/E/W). When
 *    all four are defective, the nearest complete Chebyshev ring (r = 1..3)
 *    supplies the neighbours instead (helpers.cpp:22-48).
 *  - Cluster (2+ pixels, 4-connectivity): median of the valid neighbours in the
 *    3x3 window, excluding the centre and other defective pixels
 *    (defect_correct.cpp:70-72).
 * AC-DEF-002: Static BPM priority over runtime detection
 * AC-DEF-003: Runtime transient defect detection
 * REQ-P1A-021: Validate dimension mismatch
 *
 * The defect map is not a parameter: it is loaded into the global calibration
 * by xpe_calib_load_defect_map() (#117 decision B).
 *
 * BUFFER ALIASING CONTRACT (SPEC-XPE-P1A REQ-P1A-012, #209 / QA-A-146):
 * input->data and output->data must be either EXACTLY THE SAME or FULLY
 * DISJOINT.
 *  - input->data == output->data (in-place): ALLOWED. The result is
 *    BIT-IDENTICAL to the same call with separate buffers.
 *  - Ranges that do not overlap: allowed (the ordinary path).
 *  - PARTIAL OVERLAP: XPE_ERR_INVALID_INPUT, returned before anything is
 *    written.
 * The comparison is over the n * sizeof(float) byte range this function
 * actually touches; dataSize is not the basis, because per the #123 contract
 * 0 means *unspecified* and is not a reliable length.
 *
 * In-place is safe because WRITES AND READS NEVER TOUCH THE SAME PIXEL: this
 * function writes only pixels the defect map marks (dm[idx] != 0), and both
 * kernels read only unmarked ones (defect_correct.cpp:96, helpers.cpp:30 on
 * the 4-neighbour path and the r=1..3 ring fallback alike). The two sets are
 * disjoint, so a read can never see an already-corrected value.
 *
 * NON-FINITE INPUT (QA-A-214b): a frame holding a NaN or an infinity -- masked pixels included -- is refused at the
 * entrance with XPE_ERR_INVALID_INPUT and one XPE_ALERT_ERROR "XPE_WARN_DEFECT_INPUT_NOT_FINITE: ..." (the count and the
 * first pixel); nothing is written (the output buffer is untouched, and called in place the input is). The check
 * follows the argument, format, size, aliasing, initialisation and map checks, so those errors keep their precedence.
 * The pipeline never hands this stage a non-finite value; the check is for callers of this function.
 *
 * FILL VALUE (QA-A-211b): a marked pixel takes the mean of its unmarked 4-neighbours (a lone defect), or -- inside a
 * cluster of adjacent marked pixels -- the median of the unmarked pixels of its 3x3; when that 3x3 holds none (the
 * interior of a cluster), the search widens ring by ring to Chebyshev radius 16 and the median of the NEAREST ring
 * holding an unmarked pixel is used. A pixel with no unmarked pixel within 16 pixels keeps its input value and one
 * alert "XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR: ..." (XPE_ALERT_WARNING) gives the count; it is never written as 0.
 * (Before QA-A-211b a cluster pixel with an empty 3x3 was written as 0.0f -- on CalData_6, 66.7% of the defect map.)
 * That invariant is a property of the CURRENT kernels, not a structural
 * guarantee -- a kernel that read a defective neighbour would break in-place
 * silently, so what holds it is a test, not this comment:
 * DefectCorrectTest.InPlaceMatchesOutOfPlace.
 *
 * Partial overlap is refused not because it is known to be wrong, but because
 * no caller does it, so nothing measures whether the result would be right;
 * widening the contract would guarantee behaviour no test observes (the shape
 * of #207). An error code makes the violation observable instead of letting
 * it run into UB in silence.
 *
 * ARGUMENT-CHECK ORDER: this check precedes REQ-P1A-020 (uninitialized ->
 * XPE_ERR_NOT_INITIALIZED). It sits with the other argument checks (NULL,
 * dimensions, buffer size), following the existing rule that a bad argument
 * is answered before module state -- so a partially overlapping call made
 * while uninitialized returns XPE_ERR_INVALID_INPUT.
 *
 * @param input Input image buffer (gain-corrected, FLOAT32)
 * @param output Output image buffer (defect-corrected, FLOAT32)
 * @param metadata Image metadata for dose-dependent threshold
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if xpe_preprocess_init() has not been called
 *         XPE_ERR_CALIB_NOT_LOADED if initialized but no defect map is loaded
 *         XPE_ERR_INVALID_INPUT if NULL pointers, if the input and output
 *                               buffers overlap partially (see above), or if the
 *                               loaded defect map's dimensions differ from the
 *                               input's (REQ-P1A-021)
 *         XPE_ERR_BUFFER_TOO_SMALL if the output's dimensions differ from the input's
 *
 * @note The map this function reads is the one in the calibration store when it is CALLED (it takes its own
 *       snapshot under the store's lock, then reads it in place; a load that lands during the call does not
 *       reach it). A caller that runs several stages by hand and loads calibration in between gets whatever
 *       was loaded at each call; the guarantee that one frame is processed with ONE calibration set belongs to
 *       the pipeline entry points (see xpe_preprocess_pipeline).
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
 * @param config_json_or_null Generation parameters as JSON, or NULL for the
 *        defaults. NULL behaves exactly as this function did before the
 *        parameter existed (QA-A-39, #138), so an existing caller that passes
 *        NULL sees no change.
 *
 *        Recognised keys (values are the defaults):
 *          "method"            "mean" | "median" | "sigma_clip" | "winsor"
 *          "sigma"             3.0    kappa for sigma_clip, must be > 0
 *          "max_iter"          5      sigma_clip iteration cap, 1..100
 *          "lower_percentile"  5.0    winsor lower bound, 0..100
 *          "upper_percentile"  95.0   winsor upper bound, >= lower
 *
 *        An unrecognised "method", or a value outside the ranges above, is
 *        reported as XPE_ERR_CONFIG_INVALID rather than silently defaulted.
 *
 *        With "sigma_clip", XPE-ALG-001 9.8.2.1 also applies: a pixel whose
 *        surviving frame count falls below N_min = max(3, floor(N/4)) is marked
 *        a static defect and OR-merged into the global defect map (#138
 *        decision (a)). Its offset value is unaffected.
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_INVALID_INPUT if NULL pointers or invalid parameters
 *         XPE_ERR_CONFIG_INVALID if config_json_or_null is not one valid JSON object, gives a key twice at
 *         the top level (keys are read from the top level -- see xpe_preprocess_pipeline), or holds a
 *         malformed or out-of-range value, or if a
 *         "sigma_clip" run marks at least one pixel while a defect map of a
 *         different width or height is already loaded (the marks are not
 *         merged; the map is left unchanged and output_path is not written)
 *         XPE_ERR_IO_FAILED on file write error
 */
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* dark_frames,
                                               int32_t num_frames,
                                               float integration_time_ms,
                                               float temperature_c,
                                               const char* output_path,
                                               const char* config_json_or_null);

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
 * @note One dose level, degree 0: accepted by every calibration mode; under
 *       XPE_CALIB_AUTO the mode recorded is XPE_CALIB_SINGLE_POINT.
 */
XPE_API XpeErrorCode xpe_calib_generate_gain(const XpeImageBuffer* flat_frames,
                                             int32_t num_frames,
                                             const XpeImageBuffer* dark_reference,
                                             const char* output_path,
                                             const char* metadata_json);

/**
 * @brief Generate a nonlinearity correction LUT (SRS-CALIB-FUNC-006-EXT 6a)
 *
 * Writes an XCAL_TYPE_NONLIN_LUT file: a flat uint16 table where the index is
 * the raw ADU value and the entry is the linearized ADU value.
 *
 * Procedure, per the requirement as corrected on 2026-09-18 (#186): the mean signal
 * of each flat frame is measured in ADU, an ideal response `S_ideal = G_nominal * D`
 * is fitted through the origin, the pairs `(S_meas, S_ideal)` become knots together
 * with the boundary condition `LUT[0] = 0`, and the entries between knots are filled
 * by monotone cubic interpolation (Fritsch-Carlson 1980). There is NO upper knot: the
 * `LUT[ADC_max] = ADC_max` identity pin was withdrawn. Above the highest measured
 * signal the table continues along the last measured interval's secant, saturating at
 * 65535, and the first entry of that extension is recorded in the file
 * (`xcal_nonlin_extension_start`); the accuracy clause is judged inside the measured
 * range only.
 *
 * The flat frames are taken rather than gain maps because
 * xpe_calib_generate_gain() normalizes each map to unit mean, which discards the
 * absolute signal level the fit needs.
 *
 * @param flat_frames    Array of `num_levels` flat-field frames (UINT16), one per dose.
 * @param dose_levels    Array of `num_levels` reference doses, strictly increasing, > 0.
 * @param num_levels     Number of dose levels; the requirement asks for >= 10.
 * @param dark_reference Optional dark frame subtracted before averaging (NULL to skip).
 * @param lut_entries    4096 (12-bit) or 65536 (16-bit full scale).
 * @param output_path    Destination .xcal path.
 * @param metadata_json  Optional detector-identifying JSON object embedded in the
 *                       file's config blob (NULL for none).
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT for a null argument, fewer than 10 levels, an
 *                       unsupported entry count, or non-increasing dose values
 *         XPE_ERR_INVALID_CALIB_DATA when the measured response is not strictly
 *                       increasing, its lowest value is not above zero, its highest
 *                       value reaches `lut_entries - 1`, or the fit gives a table that
 *                       is not finite or not non-decreasing
 *         XPE_ERR_IO_FAILED on write failure
 */
XPE_API XpeErrorCode xpe_calib_generate_nonlin_lut(const XpeImageBuffer* flat_frames,
                                                   const double* dose_levels,
                                                   int32_t num_levels,
                                                   const XpeImageBuffer* dark_reference,
                                                   uint32_t lut_entries,
                                                   const char* output_path,
                                                   const char* metadata_json);

/**
 * @brief Load a nonlinearity LUT into the calibration store (FUNC-006-EXT 6a)
 *
 * Reads an XCAL_TYPE_NONLIN_LUT file and makes it the active nonlinearity
 * calibration. The pipeline's nonlinearity stage applies it when `panel.linear`
 * is not "true" in the pipeline config.
 *
 * @param filepath Path to the .xcal file written by xpe_calib_generate_nonlin_lut().
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if filepath is NULL
 *         XPE_ERR_IO_FAILED / XPE_ERR_CONFIG_INVALID from the file reader; XPE_ERR_CONFIG_INVALID also when the
 *                       table's config block is not one valid JSON object or gives xcal_nonlin_extension_start
 *                       twice at the top level (a key inside a nested object is not it)
 *         XPE_ERR_INVALID_CALIB_DATA if the entry count is not 4096 or 65536,
 *                       the table is not non-decreasing, or the recorded
 *                       extension boundary lies outside the table
 */
XPE_API XpeErrorCode xpe_calib_load_nonlin_lut(const char* filepath);

/**
 * @brief Drop the loaded nonlinearity LUT (FUNC-006-EXT 6a)
 *
 * After this call the nonlinearity stage has no table to apply. Use it when
 * switching detector profiles, so a previous panel's table is never applied to
 * another detector's frames.
 */
XPE_API void xpe_calib_unload_nonlin_lut(void);

/**
 * @brief Generate dose-dependent gain polynomial (FUNC-027)
 *
 * SWU-1.12: Generate gain polynomial G(x,y,E) = c0 + c1*E + c2*E² + ...
 *
 * Algorithm:
 *   1. Load N gain maps from FUNC-026 output files
 *   2. For each pixel: fit polynomial via least-squares
 *   3. Validate monotonicity in [dose_min, dose_max]: the curve must be non-decreasing OR non-increasing
 *      (SRS-CALIB-FUNC-027 says "monotone", not "increasing"; QA-A-210c) over the WHOLE range, decided
 *      analytically -- from the real roots of the derivative's derivative -- on the coefficients as the file
 *      stores them (float32), not by sampling (QA-A-210d: a quadratic can turn between two samples)
 *   4. Reduce degree if non-monotone; degree 1 is the LEAST-SQUARES line, which is monotone whichever way it
 *      points, so the descent always ends there (min degree = 1). No pixel is stored with a worse-than-its-own-mean
 *      model: the straight line through the first and last measurement that earlier stood at the end of the
 *      descent is gone (QA-A-210c)
 *   5. Store coefficient array: (d+1) × W × H
 *
 * UNITS OF `dose_levels`: PIXEL VALUES (ADU). Not mGy.
 *
 * QA-A-141 (#194 item 2, lead decision 2026-09-26). This line used to read
 * "mGy or relative units", which permitted a unit the implementation cannot
 * accept. The applier indexes the fitted curve with the PIXEL'S OWN VALUE
 * (gain_correct.cpp), and the recorded [dose_min, dose_max] clamp is compared
 * against pixel values too, so the abscissa is in ADU by construction. The
 * reference dataset agrees -- tests/test_data/cyan_test names its CalSet
 * levels by ADU.
 *
 * WHAT THIS DOES NOT BUY. Writing "ADU" here does not make a wrong unit
 * detectable: the XCal file carries no dose-unit field, so a file fitted in
 * mGy loads, applies, and produces a WRONG IMAGE WITHOUT ANY ERROR. The
 * failure is silent. Pinning the documented unit removes the half of the
 * problem that was the documentation contradicting the code; the other half
 * needs a format change and is deferred -- see the DEBT marker below.
 *
 * @MX:DEBT: dose unit is documented, not enforced
 * @MX:CEILING: no dose-unit field exists in the XCal header; every file is
 *              assumed ADU and a mis-united file cannot be distinguished
 * @MX:UPGRADE: when #151 supplies real-detector calibration files, check what
 *              unit they actually carry; if any is not ADU, add the header
 *              dose-unit field (#194 option 1) and decide how pre-field files
 *              are read
 *
 * @param gain_file_paths Array of N gain file paths (from FUNC-026)
 * @param dose_levels Array of N dose levels, in PIXEL VALUES (ADU) -- see the
 *                    UNITS note above; not enforced, and not detectable if wrong
 * @param num_levels Number of dose levels (≥ 3)
 * @param max_degree Maximum polynomial degree (1 ≤ max_degree ≤ 4)
 * @param output_path Output XCal file path for gain polynomial
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if NULL pointers or invalid parameters (a dose level that is not finite -- NaN or
 *         +-infinity -- is one, checked before any file is read, any mode is resolved or the quality metadata can change;
 *         QA-A-210d), or
 *         XPE_ERR_INVALID_CALIB_DATA if a gain map holds a value that is not finite (QA-A-210d), or if
 *         some pixel has no polynomial the APPLIER can use (QA-A-210e): the coefficients are stored as float32 in the
 *         raw dose and applied in float32 Horner at the pixel value, and a fit whose float32 evaluation at a measured
 *         dose is more than 0.1% from the fit (typically doses so close together that the float32 intercept cannot
 *         carry the slope) lowers the degree like a non-monotone one; if even the least-squares line fails, no quality
 *         record is made and no file is written, and an alert "XPE_WARN_GAIN_POLY_NOT_APPLICABLE: ..."
 *         (XPE_ALERT_ERROR) gives the pixel count and the first pixel. A pixel whose gain is outside the gain range
 *         [0.1, 10.0] (SRS-CALIB-FUNC-002) is not refused but CLASSIFIED DEFECTIVE (QA-A-211): when a measured gain of
 *         it is outside the range, or the least-squares line leaves the range at a measured dose, its coefficients are
 *         stored as 0 (the applier classifies it again at every frame), it is not fitted and not in the quality figures
 *         below, and the count is reported ("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: ..."); more than 5% of the frame
 *         classified refuses the generation (XPE_ERR_INVALID_CALIB_DATA, alert "XPE_WARN_GAIN_PIXELS_OVER_LIMIT: ...",
 *         nothing recorded, no file). The
 *         fit_r_squared the file records is computed in the applier's arithmetic from the stored coefficients, so it is
 *         the quality of the correction that will be performed, or
 *         when num_levels / max_degree exceed the active calibration mode
 *         (SRS-CALIB-FUNC-031 (3)(4); under AUTO, more than 10 levels)
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
 * Decision #132 (api-spec.md 6.14): the previous two-argument form always wrote
 * expiry_epoch_ms = 0, so no API path could produce an expiring calibration file
 * and SRS-ALERT-005 / REQ-P1A-018 were unreachable. The expiry policy itself
 * (30 days, and so on) belongs to the caller; this function records the value.
 *
 * @param filepath Output XCal file path
 * @param calib_type Calibration type (offset, gain, defect)
 * @param expiry_epoch_ms Expiry timestamp in Unix milliseconds; 0 = never expires
 * @return XPE_OK on success
 *         XPE_ERR_NOT_INITIALIZED if module not initialized
 *         XPE_ERR_IO_FAILED on file write error. A failed save raises an XPE_ALERT_ERROR naming the step and
 *                           the system error (XPE_WARN_XCAL_TEMP_OPEN_FAILED, _TEMP_WRITE_FAILED,
 *                           _REPLACE_FAILED) only as long as the alert text can be built: building it can itself
 *                           run out of memory, and the alert is then dropped -- the return code is the one
 *                           signal that always arrives (QA-A-221c)
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 */
XPE_API XpeErrorCode xpe_calib_save(const char* filepath,
                                    const char* calib_type,
                                    uint64_t expiry_epoch_ms);

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
 *         XPE_ERR_INVALID_INPUT if NULL pointers, or if the image holds a NaN or
 *         an infinity: refused at the entrance (QA-A-215), the defect map is
 *         not written, and one Error alert
 *         XPE_WARN_RUNTIME_DETECT_INPUT_NOT_FINITE names the count and the
 *         first pixel
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
 * Phase 6: Ghost/Lag Correction (REQ-P1A-085 to REQ-P1A-087)
 * SWU-1.4: Ghost/Lag Correction Tier 1/2/3 — LTI/NLCSC deconvolution (PRE-04)
 * ============================================================================ */

/**
 * @brief Allocate an opaque ghost corrector handle with frame history buffer
 *
 * REQ-P1A-085: Allocate handle with frame history buffer
 * REQ-P1A-030: Config JSON for IRF coefficient override
 * REQ-P1A-031: Return XPE_ERR_OUT_OF_MEMORY on allocation failure
 *
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param configJsonOrNull Optional JSON configuration for tier/IRF coefficients (reading rule: see
 *        xpe_preprocess_pipeline -- one valid JSON object, top-level keys, a key given twice or a malformed text
 *        is XPE_ERR_CONFIG_INVALID and no handle is handed back)
 * @param handleOut Output: Opaque handle pointer (caller must call xpe_ghost_destroy)
 * @return XPE_OK on success
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure
 *         XPE_ERR_INVALID_INPUT on NULL handleOut or zero dimensions
 *         XPE_ERR_CONFIG_INVALID if a numeric value in the configuration (tier, alpha1, tau1,
 *                  alpha2, tau2, tier2Threshold, nlcscBeta) is not one finite number in range (notation:
 *                  see xpe_preprocess_pipeline); no handle is handed back and nothing is left allocated.
 *                  Also, when all four lag parameters are given (QA-A-226b, QA-A-226c): an alpha below 0 (it
 *                  would ADD signal), a tau that is 0 or negative (the history would grow), or a steady-state
 *                  gain S = alpha1/(1-exp(-1/tau1)) + alpha2/(1-exp(-1/tau2)) >= 1 (a term with alpha 0 counts
 *                  as 0): such a set is a forward system that cannot exist, and a constant input would come
 *                  out as input*(1-S) clamped at 0. alpha 0 is allowed; so is a very small positive tau. An
 *                  alpha of 1 or more can never pass (each term is >= its alpha). Only this weight-free S is
 *                  checked; the tier 2/3 exposure weight is not.
 *
 * @note TAU IS IN FRAMES (QA-A-226b). tau1 and tau2 are measured in frames: every successful
 *       xpe_ghost_correct call is one step. The acquisition time in the metadata is not used by the ghost
 *       corrector; a break in the sequence (another patient or study, a pause) is xpe_ghost_reset().
 *
 * @note CALIBRATED OR NOT (QA-A-226, #241). A handle corrects only when the configuration gave all four lag
 *       parameters -- alpha1, tau1, alpha2 and tau2, each present and non-empty (an empty value keeps the built-in
 *       default, so it does not count). The built-in defaults are NOT a calibration: the forward system they imply
 *       has a gain of about 2.45 and a constant exposure is "corrected" to nothing. A handle made without all four
 *       (a NULL configuration, `{}`, only tier/tier2Threshold/nlcscBeta, or three of the four) therefore passes
 *       frames through unchanged (see xpe_ghost_correct) and raises ONE alert at creation, severity
 *       XPE_ALERT_WARNING, with this text (clients may match on the prefix):
 *       "XPE_WARN_GHOST_NOT_CALIBRATED: ghost correction is not calibrated: the handle passes frames through
 *       unchanged until lag parameters are configured (alpha1, tau1, alpha2, tau2)". The warning is per handle, not
 *       per frame, and a creation that fails raises none. "Calibrated" says the parameters were GIVEN, not that they
 *       are right: whether a given set is usable is the caller's calibration.
 *
 * @note The handle holds the frame history twice (width*height floats, four planes) plus one more plane
 *       with the frame as it came in: a frame writes its new history into the second pair and the pairs are
 *       swapped only when the frame succeeded, and a frame that fails gets its pixels put back from the fifth
 *       plane (see xpe_ghost_correct). At 3072x3072 that is about 189 MB per handle.
 *
 * @note SRS-CALIB-NFR-003: one handle may be shared by several threads. Calls to
 *       xpe_ghost_correct() and xpe_ghost_reset() on the same handle are serialised
 *       inside the handle (one mutex per handle), so no history update is lost.
 *       Calls on different handles do not block each other. Not guaranteed: the
 *       caller reading or writing the same image buffer from several threads
 *       without its own synchronisation, and xpe_ghost_destroy() (see there).
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
 * Serialised per handle with xpe_ghost_reset() and other xpe_ghost_correct() calls
 * on the same handle (see xpe_ghost_create). The image buffer is not protected: the
 * caller must not access the same buffer concurrently without its own synchronisation.
 *
 * @param handle Ghost corrector handle (from xpe_ghost_create)
 * @param img [in/out] Image to correct (float32 format)
 * @param meta Image metadata; must not be NULL. Its acquisitionTime is NOT used (tau is in frames, one step per
 *        successful call -- see xpe_ghost_create; QA-A-226b)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL/invalid handle, dimension mismatch, or a frame holding a NaN or an
 *         infinity: refused at the entrance (QA-A-217), nothing is written and one Error alert
 *         XPE_WARN_GHOST_INPUT_NOT_FINITE names the count and the first pixel
 *         XPE_ERR_PROCESSING_FAILED when a corrected value or the new history overflows float (finite input
 *         at the extremes of the range)
 *
 * @note Tier 3 blends the corrected value of EVERY pixel with the mean of its 3x3 neighbourhood in the incoming frame
 *       (0.7 corrected + 0.3 mean). The mean is taken over the neighbours that exist: nine inside the frame, six on an edge,
 *       four in a corner (QA-A-227: the border pixels used to be left out, so a uniform frame came out with a one-pixel ring
 *       0.6 % to 8.4 % off). A frame smaller than 3x3 has no spatial context and is not blended. Tiers 1 and 2 read no neighbour.
 *
 * @note A handle without calibrated lag parameters (see xpe_ghost_create) does not correct: after the checks above
 *       the call returns XPE_OK with the image and the history untouched, and no
 *       alert (the one warning was raised at creation). The entrance checks -- handle, size, float32, non-finite
 *       pixels -- apply to such a handle exactly as to a calibrated one.
 *
 * @note A frame that fails leaves the handle exactly as it found it: the frame history and the exposure
 *       estimate (a failed call is not a step: the next frame continues from the last frame that SUCCEEDED).
 *       Only a successful frame changes them. Since QA-A-217 a failed frame also leaves
 *       `img` as it was (REQ-P1A-032): pixels already corrected when the failure was found are put back.
 *       Before QA-A-217 they were not, and a caller had to keep its own copy; before QA-A-202c a failure
 *       part-way through left the history of the pixels already processed updated too.
 */
XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                        const XpeImageMetadata* meta);

/**
 * @brief Clear accumulated frame history without destroying the handle
 *
 * REQ-P1A-088: Clear accumulated frame history
 * Call between patient acquisitions or after detector power cycle.
 * Serialised per handle with xpe_ghost_correct(): a concurrent correct sees the history
 * either entirely before or entirely after the reset (see xpe_ghost_create).
 *
 * @param handle Ghost corrector handle
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL/invalid handle
 */
XPE_API XpeErrorCode xpe_ghost_reset(void* handle);

/**
 * @brief Free all resources associated with a ghost corrector handle
 *
 * After this call the handle is invalid (do not pass to any other function). A pointer that is not a live handle
 * -- destroyed already, never returned by xpe_ghost_create, or not a handle at all -- is recognised from a
 * registry of live handles, WITHOUT being read (REQ-P1A-086): every ghost function refuses it
 * (XPE_ERR_INVALID_INPUT) and a second destroy returns without effect, also when several threads destroy the same
 * handle at once. Limit (ABA): once a handle is destroyed its address may be returned by a later xpe_ghost_create,
 * and a stale pointer to the old handle then reads as that new, live one -- do not keep the pointer after destroy.
 * Must not run concurrently with any call on the same handle, whether that call is
 * already in progress or starts meanwhile: the handle's mutex is freed with it, so
 * the caller must stop all other threads using the handle first.
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
 * REQ-P1A-081: NaN -> use 25.0C fallback
 * REQ-P1A-081: Temp out of [-20, +60] range -> XPE_ERR_INVALID_INPUT
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
 * Requirement: SRS-CALIB-FUNC-006 / -006-EXT (SRS-CALIB-001), not a REQ-P1A-
 * number. SPEC-XPE-P1A puts nonlinearity out of its scope (spec.md:55, PRE-08)
 * and the SPEC-XPE-P1D it names does not exist.
 *   - Apply the loaded LUT (6a) or polynomial (6b): SRS-CALIB-FUNC-006-EXT.
 *   - No LUT and no coefficients: no-op with an alert, XPE_OK (QA-A-127,
 *     QA-A-140, #196). No requirement states this sentence; the nearest text
 *     is SRS-CALIB-FUNC-006 (the profile supplies f_nonlin) and
 *     SRS-CALIB-SAFE-001 (nonlinearity is optional and conditional).
 *   - An unrecognised "mode" is not an error: the stage reads no meaning
 *     from it (QA-A-127, #196).
 * The numbers REQ-P1A-012..015 this block used to cite are the pre-bc22093
 * ones and now name other requirements. Its "identity polynomial for
 * baseline" line has no counterpart in SRS 6b or in the implementation.
 *
 * @param img [in/out] Image to correct (uint16 format)
 * @param configJsonOrNull Optional detector mode/coefficient override JSON (reading rule: see
 *        xpe_preprocess_pipeline -- one valid JSON object, top-level keys; panel.linear, panel.nonlinearity_mode,
 *        panel.target_platform, panel.nonlin_poly_c0..c4 and panel.adc_max are the keys read)
 * @return XPE_OK on success
 *         XPE_ERR_CONFIG_INVALID if a key is given twice at the top level or the text is not one valid JSON
 *         object (the frame is untouched)
 *         XPE_ERR_CALIB_NOT_LOADED if the panel is declared non-linear and no
 *         LUT is loaded
 *         XPE_ERR_INVALID_INPUT if NULL img
 */
XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                               const char* configJsonOrNull);

/**
 * @brief Apply per-mode binning correction for gain/uniformity differences
 *
 * REQ-P1A-090: binningMode 2 or 4 -> normalise each pixel by 1/binningMode^2,
 *               in place, FLOAT32 buffers only (a non-FLOAT32 buffer returns
 *               XPE_ERR_INVALID_INPUT -- implementation behaviour; the SPEC
 *               text says only "FLOAT32")
 * REQ-P1A-091: binningMode == 1 -> XPE_OK, image untouched; a mode other than
 *               1, 2 or 4 -> XPE_ERR_CONFIG_INVALID; a frame holding a NaN or
 *               an infinity -> XPE_ERR_INVALID_INPUT, checked BEFORE any pixel
 *               is written (QA-A-215), so the buffer keeps its bytes and one
 *               Error alert XPE_WARN_BINNING_INPUT_NOT_FINITE names the count
 *               and the first pixel. (Scaling by 1/4 or 1/16 cannot make a
 *               finite value non-finite, so the input is the only source.)
 * REQ-P1A-095: this stage runs after gain correction in the pipeline
 * The numbers REQ-P1A-020/021/022 this block used to cite are the
 * pre-bc22093 ones. They now name the not-initialized, dimension-mismatch
 * and format-mismatch guards, which this function does not implement (it
 * has no initialisation check and takes no map buffer).
 *
 * @param img [in/out] Image to correct (float32 format)
 * @param binningMode Binning factor (1 = no-op, 2 = 2x2, 4 = 4x4)
 * @param configJsonOrNull Unused: no correction profile is read from it
 * @return XPE_OK on success
 *         XPE_ERR_CONFIG_INVALID if unknown binning mode
 *         XPE_ERR_INVALID_INPUT if NULL img
 */
XPE_API XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                          int32_t binningMode,
                                          const char* configJsonOrNull);

/* =============================================================================
 * Phase 8: Full Pre-Processing Pipeline (REQ-P1A-095 to REQ-P1A-099)
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
 * @param has_nonuniform_gain Output: true if any row mean > 0.9 * UINT16_MAX (a bright-row
 *        check; the name is historical -- it does not detect line noise, see #232)
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
 * REQ-P1A-095 to REQ-P1A-099: Full pipeline integration
 *
 * One frame, one calibration set (QA-A-202d). The three maps (offset, gain, defect) with their dimensions are
 * captured together, under one lock, when the frame starts -- for this function and the batch, the set they have
 * just loaded from @p calibPath, captured in the same critical section that stored it -- and every stage runs
 * on that capture. Another thread loading a different set while the frame runs cannot give the frame an offset
 * from one set and a gain from another. Calibration loaded by other calls (xpe_calib_load_*, the cached loaders)
 * during the call takes effect from the next frame. The nonlinearity table is not part of the set: it is read
 * from the store when its stage runs.
 *
 * A bypassed stage is skipped, with one exception that is not a skip: the gain stage is the uint16 -> float32
 * boundary, so a bypassed gain stage means "gain = 1" -- when a float stage follows (binning with binningMode > 1,
 * defect, or ghost with a handle) the frame is converted to float32 explicitly and the result is the one a gain
 * map of ones gives (QA-A-208). With no float stage the frame stays uint16.
 *
 * A call that fails AFTER some stages have run -- a stage's own error, or one of the internal consistency checks
 * that return XPE_ERR_PROCESSING_FAILED -- leaves the frame in img as the caller gave it (the result is written
 * back only after the last stage). It does not undo the stage-completion flags in the metadata: the flags of the
 * stages that finished stay set, as for every error other than XPE_ERR_OUT_OF_MEMORY. (A refusal before any stage
 * runs -- the capacity and dimension checks -- sets none.)
 * Pipeline: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost
 *
 * @warning This function RE-READS offset.xcal, gain.xcal and defect.xcal from
 *          @p calibPath on every call. At 3072x3072 that is about 475 ms per
 *          frame (measured, QA-A-105) -- more than the 500 ms budget for the
 *          whole pipeline (SRS-CALIB-PERF-001), and SRS-CALIB-PERF-003 budgets
 *          200 ms for loading all three files ONCE at startup, not per frame.
 *          The intended per-frame path is:
 *              XpeCalibrationState st = {};
 *              xpe_calib_state_load(&st, calibPath);   // once
 *              for each frame: xpe_preprocess_pipeline_ex(img, meta, &st, ...);
 *              xpe_calib_state_release(&st);
 *          Use this function for a one-off frame or a smoke test.
 *
 * @param img [in/out] Image to process (uint16 in, float32 out after Gain). The result is written back into
 *        img->data, so img->dataSize is both the room for the input and the room for the result:
 *          - the frame the pipeline hands back is float32 (width*height*4 bytes) when any stage from the gain stage
 *            on runs (gain, binning with binningMode > 1, defect, or ghost with a handle), and the uint16 frame
 *            (width*height*2 bytes) otherwise;
 *          - dataSize 0 is NOT "size not given" here (the rule for input-only buffers): there is no room for a
 *            result, and the call returns XPE_ERR_BUFFER_TOO_SMALL; a dataSize smaller than that result,
 *            with at least width*height*2, is XPE_ERR_BUFFER_TOO_SMALL too; 0 < dataSize < width*height*2
 *            (the buffer does not even hold the input) is XPE_ERR_INVALID_INPUT;
 *          - a refused call reads nothing, writes nothing and sets no flag; a larger dataSize is accepted and the
 *            bytes beyond the result are not written.
 *        A width or height of 0, or dimensions whose byte count does not fit in a size_t, is
 *        XPE_ERR_INVALID_INPUT. This used to be accepted for a short buffer: the result was copied truncated and
 *        the call returned XPE_OK with a float32 format on a half-written frame (QA-A-205b, Codex #29).
 * @param meta [in/out] Image metadata (updated with processing flags)
 * @param calibPath Calibration data directory path
 * @param ghostHandle Ghost corrector handle (NULL = skip ghost correction). A handle without calibrated lag
 *        parameters (see xpe_ghost_create) passes the frame through: the output is still float32 (the format depends
 *        on the handle being given), but XPE_FLAG_GHOST_CORRECTED is not set (QA-A-226)
 * @param configJsonOrNull Pipeline configuration JSON (bypass flags, temperature, etc.), NUL-terminated.
 *        Reading rule (every configuration text of this module -- this one, xpe_ghost_create's, the nonlinearity
 *        stage's, the offset generation's): the text is ONE valid JSON object and its keys are read from the TOP
 *        LEVEL. A key that appears only inside a nested object or array is not given (the default applies); a
 *        top-level key beside a nested one of the same name is the top-level one; a top-level key given twice, or
 *        a text that is not one JSON object (not closed, an array, text after the object, a raw
 *        control character in a string, single quotes, a trailing comma), is XPE_ERR_CONFIG_INVALID and nothing
 *        is changed. A top-level key given twice is refused WHATEVER its name -- an unknown key included
 *        ({"future":1,"future":2}): the object is ambiguous, and which keys this module reads must not decide
 *        whether it is noticed (QA-A-209b). A text that is empty or only white space is not a configuration and is
 *        XPE_ERR_CONFIG_INVALID; a NULL text means every default. (The config block STORED in an XCal file is the
 *        one exception: a block of length 0 is a file without one and loads; a block that is there is read by this
 *        rule, so white space only is refused.) An empty string value ("") and a value that is an object or an
 *        array are "not given" (a GUI sends an unset option as ""). The text is parsed ONCE per call and every key
 *        is read from that parse, the nonlinearity stage's included (panel.linear, panel.nonlinearity_mode,
 *        panel.target_platform, panel.nonlin_poly_c0..c4, panel.adc_max), so a refusal happens before the first
 *        stage runs. An XCal file's config block is parsed ONCE per load, by the file reader, and the loader reads its
 *        keys from that parse (QA-A-209c); the block of a compressed DEFECT file written by an older writer, whose
 *        closing braces were doubled, is accepted after a repair with a warning alert (xcal_format.h).
 *
 *        Two ways a key's value is read, and they differ on purpose. (1) A STRING-OR-TOKEN key -- the bypass flags,
 *        detectorTempC, binningMode, the ghost keys (tier, alpha1, ...), panel.linear, panel.nonlinearity_mode,
 *        panel.target_platform, method -- accepts a JSON string or a bare token, and its text is then converted
 *        strictly (notation as below: "25.5" and 25.5 are both 25.5). (2) A NUMBER key -- the polynomial
 *        coefficients and panel.adc_max, the offset generation's sigma / max_iter / lower_percentile /
 *        upper_percentile, an XCal config block's dose_min, dose_max, xcal_nonlin_extension_start -- is given only as
 *        a BARE JSON number; a string ("2.0") is "not given", and a bare token that is not a JSON number (+2, .5, NaN)
 *        is not JSON at all and is XPE_ERR_CONFIG_INVALID. Compatibility: a text such as {"sigma":+2} used to be
 *        read as a number by strtod or silently ignored; it is now refused (QA-A-209b).
 * @return XPE_OK on success
 *         XPE_ERR_CONFIG_INVALID if the configuration is not one valid JSON object, a top-level key is given
 *                  twice, or a numeric value in the configuration (detectorTempC,
 *                  binningMode) is not one finite number in range -- "abc", "1e999", "2x" -- nothing
 *                  is loaded and no image or metadata is touched (the configuration is read first).
 *                  Notation of a configuration number: optional leading white space, an optional single
 *                  '+' (not followed by another sign), then a decimal number that fills the rest of the
 *                  value -- an integer for an integer field, a finite number (decimal point and exponent
 *                  allowed) for a real field. "25 ", "2x", "+-1", "nan", "inf", hexadecimal and
 *                  out-of-range values are refused; an empty value is an absent one (the default).
 *         Any error while the three calibration files are read (a missing, corrupt or expired file, a
 *                  wrong type, a malformed quality field, an allocation failure) leaves the calibration
 *                  store and the quality metadata exactly as the call found them: offset.xcal, gain.xcal
 *                  and defect.xcal are read as a SET and replace the stored maps -- and the quality
 *                  record xpe_calib_get_quality_meta() serves (the gain file's quality, or "none" when it
 *                  carries none) -- together, or not at all.
 *                  Once the set has loaded it stays loaded even if processing the frame then fails
 *                  (QA-A-221b): the maps left in the store are whole and verified -- never a half-replaced
 *                  set; the replacement of the set is atomic -- and the next call loads them again.
 *         XPE_ERR_BUFFER_TOO_SMALL if img->dataSize is smaller than the frame the pipeline writes back (see @p img)
 *         XPE_ERR_INVALID_INPUT on a NULL img / meta / img->data, an empty or overflowing frame, or a dataSize
 *                  that does not hold the input (see @p img)
 *         XPE_ERR_OUT_OF_MEMORY if an allocation fails; no exception leaves the function, the image
 *                  is untouched and the metadata is as it was
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
 * This is the recommended per-frame entry point: the calibration files are
 * read once by xpe_calib_state_load() (SRS-CALIB-PERF-003: "Clinical workflows
 * load calibration once at startup, not per-frame"), so a frame costs about
 * 124 ms instead of about 590 ms at 3072x3072 (measured, QA-A-105).
 *
 * @param img [in/out] Image to process; img->dataSize is the room for the result as well as the input,
 *        exactly as for xpe_preprocess_pipeline()
 * @param meta [in/out] Image metadata
 * @param calibState Pre-loaded calibration state (from xpe_calib_state_load)
 * @param ghostHandle Ghost corrector handle (NULL = skip ghost)
 * @param configJsonOrNull Pipeline configuration JSON (reading rule: see xpe_preprocess_pipeline -- one valid JSON
 *        object, top-level keys, a key given twice or a malformed text is XPE_ERR_CONFIG_INVALID)
 * @return XPE_OK on success
 *         XPE_ERR_CONFIG_INVALID if a numeric value in the configuration (detectorTempC,
 *                  binningMode) is not one finite number in range -- "abc", "1e999", "2x" -- no image or metadata is touched (the configuration is read first)
 *         XPE_ERR_BUFFER_TOO_SMALL, XPE_ERR_INVALID_INPUT for the size and dimension rules of
 *                  xpe_preprocess_pipeline() (see its @p img)
 *         XPE_ERR_OUT_OF_MEMORY if an allocation fails (image untouched, metadata as it was)
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
 * @param configJsonOrNull Pipeline configuration JSON (reading rule: see xpe_preprocess_pipeline -- one valid JSON
 *        object, top-level keys, a key given twice or a malformed text is XPE_ERR_CONFIG_INVALID)
 * @return XPE_OK if all images processed successfully
 *         XPE_ERR_INVALID_INPUT on null/invalid parameters
 *         XPE_ERR_CONFIG_INVALID if a numeric value in the configuration is not one finite
 *                  number in range (see xpe_preprocess_pipeline); no frame is touched
 *         A call that fails after the calibration set was loaded leaves that set in the store (QA-A-221b): whole
 *                  and verified, never half replaced -- the same rule as xpe_preprocess_pipeline().
 *         first error code if any individual frame fails -- including a frame whose dataSize is too small for
 *                  its result (XPE_ERR_BUFFER_TOO_SMALL, rules of xpe_preprocess_pipeline()): each frame is
 *                  checked as its turn comes, not all before the first, so a refused frame is left untouched
 *                  and the batch carries on with the next one; a frame that runs out of memory is
 *         XPE_ERR_OUT_OF_MEMORY, is left untouched with its metadata as it was, and the batch
 *         carries on with the next frame
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
 * Returns a cache-owned view of the map: XPE_PIXEL_FLOAT32, 32 bits, dataSize = width * height * 4.
 *
 * - Cache key: @p filePath compared as a string, exactly. An entry also records which of the three
 *   cached loaders made it (a file holds one kind of map): asking for the same path through a loader
 *   of another kind is refused with the code the plain loader of that kind returns for the file
 *   (wrong XCal type), leaves the module-global store alone, and leaves the entry for its own loader.
 * - Hit: no file read. The module-global calibration store is set to this map, as after a miss, so a
 *   correction called right after a successful load works, and the call reaches the verdict a miss
 *   would reach:
 *     file change - the file's size and last-write time are compared with the ones recorded when it
 *                  was read; if either differs, or the file cannot be examined, the hit is cancelled
 *                  and the call loads the file like a miss (so its SHA-256 is checked again and a
 *                  tampered file is refused with the loader's code);
 *     readable   - the file is opened for reading once (opening only, nothing is read): a file whose
 *                  attributes are visible but whose content cannot be opened is refused with
 *                  XPE_ERR_IO_FAILED, as a miss would, and the store and the entry are left as they
 *                  were. This comes BEFORE the expiry, in the order the file reader judges a file: a
 *                  file that is both expired and unreadable is XPE_ERR_IO_FAILED;
 *     expiry     - then re-checked from the file's expiry kept in the entry; an expired map is
 *                  refused with XPE_ERR_CALIBRATION_EXPIRED, the entry is dropped, and the store is
 *                  left as it was.
 *   A hit does NOT re-hash the file: a change that keeps both the size and the last-write time is not
 *   noticed. Call xpe_calib_cache_clear() (or shut the module down) to force the next call to read the
 *   file. The session check is not repeated on a hit.
 * - Concurrent writers are not supported: do not write or replace the calibration file while a load
 *   of it is in progress. The attributes are looked at once, before the lookup; a second look just
 *   before the install would not close every such race, so none is made.
 * - Miss: loads through xpe_calib_load_offset(), copies the map into the cache.
 * - Ownership: the data pointer belongs to the cache on hit and miss. Do NOT free it. It stays valid
 *   until xpe_calib_cache_clear(), eviction (a full cache, or xpe_calib_cache_set_max_size()),
 *   xpe_preprocess_shutdown(), or the entry being dropped because its file changed or expired (see
 *   Hit). Take a copy with xpe_copy_image() to keep it longer.
 *
 * @param filePath Path to calibration file
 * @param offsetMapOut Output: the cache-owned view (see Ownership)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if @p filePath or the output pointer is NULL
 *         on a miss or a cancelled hit, the code of the plain loader (XPE_ERR_IO_FAILED,
 *                               XPE_ERR_CALIBRATION_EXPIRED, a SHA-256 failure, ...)
 *         XPE_ERR_CALIBRATION_EXPIRED on a hit whose entry has expired
 *         XPE_ERR_NOT_INITIALIZED if the load left no offset map in the store
 *         XPE_ERR_IO_FAILED on a hit whose file cannot be opened for reading
 *         XPE_ERR_OUT_OF_MEMORY, XPE_ERR_PROCESSING_FAILED if the entry could not be cached or an
 *                               allocation failed (no exception leaves this function). On a miss the
 *                               module-global store may already hold the file's map when the entry
 *                               could not be cached -- a whole, verified map, never a half-loaded one
 *                               (QA-A-221b); on a hit that fails, the store is unchanged.
 */
XPE_API XpeErrorCode xpe_calib_load_offset_cached(const char* filePath,
                                                    XpeImageBuffer* offsetMapOut);

/**
 * @brief Load gain calibration map with LRU caching
 *
 * Returns a cache-owned view of the scalar gain map: XPE_PIXEL_FLOAT32, 32 bits,
 * dataSize = width * height * 4.
 *
 * - Cache key: @p filePath compared as a string, exactly. An entry also records which of the three
 *   cached loaders made it (a file holds one kind of map): asking for the same path through a loader
 *   of another kind is refused with the code the plain loader of that kind returns for the file
 *   (wrong XCal type), leaves the module-global store alone, and leaves the entry for its own loader.
 * - Hit: no file read. The module-global calibration store is set to this map, as after a miss, so a
 *   correction called right after a successful load works, and the call reaches the verdict a miss
 *   would reach:
 *     file change - the file's size and last-write time are compared with the ones recorded when it
 *                  was read; if either differs, or the file cannot be examined, the hit is cancelled
 *                  and the call loads the file like a miss (so its SHA-256 is checked again and a
 *                  tampered file is refused with the loader's code);
 *     readable   - the file is opened for reading once (opening only, nothing is read): a file whose
 *                  attributes are visible but whose content cannot be opened is refused with
 *                  XPE_ERR_IO_FAILED, as a miss would, and the store and the entry are left as they
 *                  were. This comes BEFORE the expiry, in the order the file reader judges a file: a
 *                  file that is both expired and unreadable is XPE_ERR_IO_FAILED;
 *     expiry     - then re-checked from the file's expiry kept in the entry; an expired map is
 *                  refused with XPE_ERR_CALIBRATION_EXPIRED, the entry is dropped, and the store is
 *                  left as it was.
 *   A hit does NOT re-hash the file: a change that keeps both the size and the last-write time is not
 *   noticed. Call xpe_calib_cache_clear() (or shut the module down) to force the next call to read the
 *   file. The session check is not repeated on a hit.
 * - Concurrent writers are not supported: do not write or replace the calibration file while a load
 *   of it is in progress. The attributes are looked at once, before the lookup; a second look just
 *   before the install would not close every such race, so none is made.
 * - Miss: loads through xpe_calib_load_gain(), copies the map into the cache.
 * - Ownership: the data pointer belongs to the cache on hit and miss. Do NOT free it. It stays valid
 *   until xpe_calib_cache_clear(), eviction (a full cache, or xpe_calib_cache_set_max_size()),
 *   xpe_preprocess_shutdown(), or the entry being dropped because its file changed or expired (see
 *   Hit). Take a copy with xpe_copy_image() to keep it longer.
 * - Quality metadata: a hit applies the entry's stored config JSON again, so
 *   xpe_calib_get_quality_meta() reports what a miss would report.
 * - A gain POLYNOMIAL file (XCAL_TYPE_GAIN_POLY) loads into the store, where xpe_gain_correct() uses it.
 *   There is no scalar map to hand back, so this function returns XPE_OK with @p gainMapOut zeroed
 *   (data NULL, dataSize 0) and caches nothing: every call reads the file again. Use xpe_gain_correct()
 *   rather than the returned buffer for such a file.
 *
 * @param filePath Path to calibration file
 * @param gainMapOut Output: the cache-owned view (see Ownership)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if @p filePath or the output pointer is NULL
 *         on a miss or a cancelled hit, the code of the plain loader (XPE_ERR_IO_FAILED,
 *                               XPE_ERR_CALIBRATION_EXPIRED, a SHA-256 failure, ...)
 *         XPE_ERR_CALIBRATION_EXPIRED on a hit whose entry has expired
 *         XPE_ERR_NOT_INITIALIZED if the load left neither a scalar gain map nor a polynomial in the store
 *         XPE_ERR_IO_FAILED on a hit whose file cannot be opened for reading
 *         XPE_ERR_OUT_OF_MEMORY, XPE_ERR_PROCESSING_FAILED if the entry could not be cached or an
 *                               allocation failed (no exception leaves this function). On a miss the
 *                               module-global store may already hold the file's map when the entry
 *                               could not be cached -- a whole, verified map, never a half-loaded one
 *                               (QA-A-221b); on a hit that fails, the store is unchanged.
 */
XPE_API XpeErrorCode xpe_calib_load_gain_cached(const char* filePath,
                                                  XpeImageBuffer* gainMapOut);

/**
 * @brief Load defect map with LRU caching
 *
 * Returns a cache-owned view of the map: XPE_PIXEL_UINT8, 8 bits, dataSize = width * height.
 *
 * - Cache key: @p filePath compared as a string, exactly. An entry also records which of the three
 *   cached loaders made it (a file holds one kind of map): asking for the same path through a loader
 *   of another kind is refused with the code the plain loader of that kind returns for the file
 *   (wrong XCal type), leaves the module-global store alone, and leaves the entry for its own loader.
 * - Hit: no file read. The module-global calibration store is set to this map, as after a miss, so a
 *   correction called right after a successful load works, and the call reaches the verdict a miss
 *   would reach:
 *     file change - the file's size and last-write time are compared with the ones recorded when it
 *                  was read; if either differs, or the file cannot be examined, the hit is cancelled
 *                  and the call loads the file like a miss (so its SHA-256 is checked again and a
 *                  tampered file is refused with the loader's code);
 *     readable   - the file is opened for reading once (opening only, nothing is read): a file whose
 *                  attributes are visible but whose content cannot be opened is refused with
 *                  XPE_ERR_IO_FAILED, as a miss would, and the store and the entry are left as they
 *                  were. This comes BEFORE the expiry, in the order the file reader judges a file: a
 *                  file that is both expired and unreadable is XPE_ERR_IO_FAILED;
 *     expiry     - then re-checked from the file's expiry kept in the entry; an expired map is
 *                  refused with XPE_ERR_CALIBRATION_EXPIRED, the entry is dropped, and the store is
 *                  left as it was.
 *   A hit does NOT re-hash the file: a change that keeps both the size and the last-write time is not
 *   noticed. Call xpe_calib_cache_clear() (or shut the module down) to force the next call to read the
 *   file. The session check is not repeated on a hit.
 * - Concurrent writers are not supported: do not write or replace the calibration file while a load
 *   of it is in progress. The attributes are looked at once, before the lookup; a second look just
 *   before the install would not close every such race, so none is made.
 * - Miss: loads through xpe_calib_load_defect_map(), copies the map into the cache.
 * - Ownership: the data pointer belongs to the cache on hit and miss. Do NOT free it. It stays valid
 *   until xpe_calib_cache_clear(), eviction (a full cache, or xpe_calib_cache_set_max_size()),
 *   xpe_preprocess_shutdown(), or the entry being dropped because its file changed or expired (see
 *   Hit). Take a copy with xpe_copy_image() to keep it longer.

 *
 * @param filePath Path to defect map file
 * @param defectMapOut Output: the cache-owned view (see Ownership)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if @p filePath or the output pointer is NULL
 *         on a miss or a cancelled hit, the code of the plain loader (XPE_ERR_IO_FAILED,
 *                               XPE_ERR_CALIBRATION_EXPIRED, a SHA-256 failure, ...)
 *         XPE_ERR_CALIBRATION_EXPIRED on a hit whose entry has expired
 *         XPE_ERR_NOT_INITIALIZED if the load left no defect map in the store
 *         XPE_ERR_IO_FAILED on a hit whose file cannot be opened for reading
 *         XPE_ERR_OUT_OF_MEMORY, XPE_ERR_PROCESSING_FAILED if the entry could not be cached or an
 *                               allocation failed (no exception leaves this function). On a miss the
 *                               module-global store may already hold the file's map when the entry
 *                               could not be cached -- a whole, verified map, never a half-loaded one
 *                               (QA-A-221b); on a hit that fails, the store is unchanged.
 */
XPE_API XpeErrorCode xpe_calib_load_defect_cached(const char* filePath,
                                                    XpeImageBuffer* defectMapOut);

/**
 * @brief Clear all entries from the calibration cache, freeing memory
 *
 * Also done by xpe_preprocess_shutdown(). Does not touch the module-global calibration store.
 * Views returned by the *_cached loaders become invalid.
 */
XPE_API void xpe_calib_cache_clear(void);

/**
 * @brief Set the maximum number of calibration maps retained in cache
 *
 * Default is 4. Excess entries are evicted (LRU first). The setting is not reset by
 * xpe_preprocess_shutdown().
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
 * Call this ONCE at startup (or whenever the calibration set changes) and pass
 * the state to xpe_preprocess_pipeline_ex() for every frame. Loading all three
 * files at 3072x3072 takes about 475 ms (measured, QA-A-105); the budget in
 * SRS-CALIB-PERF-003 is 200 ms.
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
    double prnu_after;          ///< PRNU after gain correction [%].
                                ///< THIS IS `FlatResidualPct` -- QA-A-154 (#218).
                                ///< Preprocessing-E2E-Automated-Evaluation-Protocol.md:218-219
                                ///< defines `PRNU_CV = std/mean` (a FRACTION) and
                                ///< `FlatResidualPct = 100 * PRNU_CV` (a PERCENT). This field is
                                ///< computed as std/mean*100, so it is the percent one, and it is
                                ///< what SRS-CALIB-FUNC-017 gates at <= 1.0%. The field keeps its
                                ///< name because renaming a public struct member is an ABI change;
                                ///< what was missing was the gate, not the number.
    double flatness_pct;        ///< Histogram flatness percentage (higher = more uniform)
    double gain_coverage;       ///< % of valid gain values (1.0 = all valid)
    uint32_t invalid_gain_count;///< Number of invalid gain pixels

    // Defect/BPM metrics
    uint32_t defect_count;      ///< Number of defective pixels found
    double   defect_density;    ///< defects / total pixels × 100 [%]
    double   correction_error;  ///< Mean absolute difference between corrected pixels and neighbor mean

    // Overall
    /// Improvement in dB, finite for finite input (QA-A-224, #242). The name is kept (ABI lock,
    /// SRS-CALIB-FUNC-037); the two functions that write it fill it with the same quantity:
    ///  - xpe_verify_gain: 20*log10(prnu_before/prnu_after), the PRNU improvement. A residual of
    ///    exactly zero is an infinite improvement and is reported as 200, which passes the gate.
    ///  - xpe_verify_pipeline: SNR_final - SNR_raw with SNR = 20*log10(mean/std). A flat frame
    ///    (std exactly 0) has an infinite SNR and counts as 200, so a noisy raw frame processed
    ///    to a flat one reports 200 minus the raw SNR.
    /// 200 is a reporting convention, not a bound: a nonzero residual is reported as measured. It is a
    /// value on the same axis as any finite improvement, not a flag: whether a correction was perfect is
    /// `prnu_after == 0` of a frame that was MEASURED (see `measured_mask`); an unmeasurable corrected
    /// frame also leaves `prnu_after` at 0 but reports 0.0 here and fails (QA-A-224b).
    /// Non-finite pixels are outside this guarantee (the field may then be NaN and the verdict
    /// is a failure).
    double snr_improvement_db;
    bool   overall_pass;        ///< Overall pass/fail based on thresholds

    /* ---------------------------------------------------------------------
     * APPENDED BY QA-A-159 (#223) -- SRS-CALIB-FUNC-037.
     *
     * Everything above keeps its name, type, and OFFSET. Three holes were
     * closed in ONE widening rather than three, because they are one ABI
     * decision about one struct:
     *   (i)   dark_reduction_db -- the gate computed it and threw it away
     *   (ii)  dsnu_adu          -- the canonical name resolved to the wrong value
     *   (iii) measured_mask     -- "could not be measured" had no field
     * ------------------------------------------------------------------- */

    /// `DarkReduction_dB` -- Preprocessing-E2E-Automated-Evaluation-Protocol.md:205
    /// `20*log10(std(R_dark_roi) / max(std(Y_dark_roi), epsilon))`, over the SAME
    /// dark ROI as dark_bias. SRS-CALIB-FUNC-016 gates on this as the `or`
    /// alternative to `abs(DarkBias) <= 5 ADU`; until #223 the value existed only
    /// inside the gate, so a caller could not see WHICH clause passed it.
    double dark_reduction_db;

    /// `DSNU_ADU` -- Protocol.md:204 `DSNU_ADU = std(Y_dark_roi)`, in ADU.
    ///
    /// THIS DUPLICATES `residual_noise` ON PURPOSE, and the duplication is the
    /// point. The canonical quantity was already computed and stored, but under
    /// a name nobody searching for DSNU would find -- while the field actually
    /// NAMED `dsnu` holds a coefficient of variation in percent, a different
    /// quantity. QA-A-158 (#222) measured what that costs: on a real dark frame
    /// a 0.51 ADU residual (a GOOD correction, 10x inside the 5 ADU budget)
    /// produced dsnu = 129%, because a CV diverges as its mean approaches zero.
    /// Reading the name instead of the formula produced a gate that rejected
    /// good corrections. FUNC-037 (c): add the canonical name, change no
    /// existing meaning. Both fields are written from the same variable at the
    /// same place so they cannot drift.
    double dsnu_adu;

    /// Which metrics in this struct were actually MEASURED -- SRS-CALIB-FUNC-036.
    /// A bitwise OR of XpeMetricMeasured flags; a clear bit means the verification
    /// could not measure that metric and the corresponding field holds a
    /// placeholder, NOT a measurement.
    ///
    /// Per-metric rather than one struct-wide flag, because the requirement asks
    /// which VALUE is trustworthy, and one call can measure some and not others.
    /// A bitmask over per-field booleans: it reserves 32 slots inside one field,
    /// so the next metric needs no further ABI change.
    ///
    /// `overall_pass = false` on an unmeasurable input REMAINS -- see FUNC-036.
    /// The two live together deliberately: this mask serves the caller who asks
    /// what happened, and the false serves the caller who never reads the mask.
    /// Removing the false would silently re-open #219 for every such caller.
    uint32_t measured_mask;
} XpeCalibrationMetrics;

/**
 * @brief What a gain map means, as SRS-CALIB-FUNC-018 records it.
 *
 * "System shall record calibration gain semantics as `normalized_gain`,
 * `reciprocal_gain`, or `unknown`. Unknown semantics may run exploratory
 * validation but shall not pass release gates."
 *
 * Values are the int32 the C ABI carries for a C enum; any other value is
 * rejected by xpe_verify_gain with XPE_ERR_INVALID_INPUT.
 */
typedef enum {
    XPE_GAIN_SEMANTICS_UNKNOWN    = 0,  ///< `unknown`
    XPE_GAIN_SEMANTICS_NORMALIZED = 1,  ///< `normalized_gain`
    XPE_GAIN_SEMANTICS_RECIPROCAL = 2   ///< `reciprocal_gain`
} XpeGainSemantics;

/**
 * @brief Per-metric "was this actually measured" flags for `measured_mask`
 *
 * SRS-CALIB-FUNC-036. A set bit means the field was computed from data; a clear
 * bit means it holds a placeholder.
 */
typedef enum {
    XPE_METRIC_DARK_BIAS       = 1u << 0,  ///< dark_bias, dsnu, dsnu_adu, residual_noise
    XPE_METRIC_DARK_REDUCTION  = 1u << 1,  ///< dark_reduction_db
    XPE_METRIC_PRNU            = 1u << 2,  ///< prnu_before, prnu_after, flatness_pct
    XPE_METRIC_GAIN_COVERAGE   = 1u << 3,  ///< gain_coverage, invalid_gain_count
    XPE_METRIC_DEFECT          = 1u << 4,  ///< defect_count, defect_density, correction_error
    XPE_METRIC_SNR             = 1u << 5   ///< snr_improvement_db
} XpeMetricMeasured;

/**
 * @brief Verify offset correction quality
 *
 * Computes dark bias, DSNU, and residual noise metrics for offset-corrected images.
 * Dark bias measures how well offset correction removes the dark current pedestal.
 * DSNU measures residual non-uniformity in dark regions.
 *
 * SRS-CALIB-FUNC-016: Offset correction verification.
 * No REQ-P1A- requirement covers xpe_verify_* (spec.md does not mention them); the
 * REQ-P1A-010 this line used to cite is "Offset Correction Execution" (xpe_offset_correct),
 * not verification.
 * SRS-CALIB-FUNC-036 states the measured/unmeasured reporting contract for every
 * xpe_verify_*.
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
 * SRS-CALIB-FUNC-017: Gain correction verification.
 * No REQ-P1A- requirement covers xpe_verify_* (spec.md does not mention them); the
 * REQ-P1A-011 this line used to cite is "Gain Correction Execution" (xpe_gain_correct),
 * not verification.
 * SRS-CALIB-FUNC-036 states the measured/unmeasured reporting contract for every
 * xpe_verify_*.
 *
 * @param before_gain Offset-corrected image (UINT16)
 * @param after_gain Gain-corrected image (FLOAT32)
 * @param gain_map Gain map used (FLOAT32)
 * @param gain_semantics What the gain map means (SRS-CALIB-FUNC-018). It selects the
 *        FlatResidualPct line that `overall_pass` is held to, and changes no measured value:
 *        `XPE_GAIN_SEMANTICS_NORMALIZED` or `XPE_GAIN_SEMANTICS_RECIPROCAL` (known) -> 0.5%;
 *        `XPE_GAIN_SEMANTICS_UNKNOWN` -> 1.0%. SRS-CALIB-FUNC-017: "Phase 1 acceptance shall
 *        require `FlatResidualPct <= 1.0%` and target `<= 0.5%` for release-hardening fixtures
 *        where gain semantics are known."
 * @param metrics Output metrics
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers, a `gain_semantics` value outside the
 *                               enumeration, or a width or height of zero (checked before
 *                               the format, after the dimension match)
 *         XPE_ERR_BUFFER_TOO_SMALL on dimension mismatch
 *         XPE_ERR_UNSUPPORTED_FORMAT on format mismatch of a non-empty frame
 *
 * @note `overall_pass` is the Phase 1 verdict. It does not on its own mean a release is
 *       approved: the release gate separately requires `gain_semantics != UNKNOWN`
 *       (SRS-CALIB-FUNC-018: unknown semantics "shall not pass release gates").
 *
 * @note PROVISIONAL thresholds (QA-A-223, #242): the 3.0 dB improvement line and the 0.99
 *       coverage line have no requirement basis; they are kept, not derived. `FlatResidualPct`
 *       (SRS-CALIB-FUNC-017) is the criterion the requirement states.
 * @note A perfectly corrected frame passes: a corrected frame that can be measured (finite, positive
 *       mean) with a spread of exactly zero reports `snr_improvement_db` as 200 dB, a reporting
 *       convention for an infinite improvement (QA-A-224). A corrected frame that cannot be measured
 *       (mean <= 0 or NaN, or a non-finite pixel) never passes: it reports no improvement (0.0), and
 *       `measured_mask` has neither `XPE_METRIC_PRNU` nor `XPE_METRIC_SNR` (QA-A-224b). The same holds
 *       for a raw frame that cannot be measured -- all zeros, a dead sensor's frame; it is UINT16, so
 *       zero is the only way its mean fails the condition (QA-A-224c).
 * @note A raw frame and a corrected frame that are BOTH flat (PRNU < 0.01%) pass without any improvement:
 *       an already flat panel has nothing to improve, and the requirement (SRS-CALIB-FUNC-017) sets a
 *       residual limit, not an improvement. `xpe_verify_pipeline` differs: its field is an SNR difference
 *       and two flat frames improve nothing, so it does not pass. Decided in QA-A-224b; revisited when #242
 *       is decided.
 *
 * @warning ABI break (QA-A-192, #220): the 5-argument form replaced the 4-argument form under
 *       the SAME exported name, so a binary built against the old header still links and
 *       loads, and then calls with four arguments: `metrics` is read from the wrong register
 *       or stack slot and the call misbehaves, with no error at link or load time. Rebuild
 *       every caller and deploy them together with this library. No caller outside the tests
 *       was found in the repository search range (QA-A-189); callers elsewhere were not
 *       searched. `gain_semantics` sits before `metrics`.
 */
XPE_API XpeErrorCode xpe_verify_gain(
    const XpeImageBuffer* before_gain,
    const XpeImageBuffer* after_gain,
    const XpeImageBuffer* gain_map,
    XpeGainSemantics gain_semantics,
    XpeCalibrationMetrics* metrics);

/**
 * @brief Verify defect correction quality
 *
 * Computes defect count, density, and correction error metrics.
 * Correction error measures how well defective pixels are interpolated from neighbors.
 *
 * SRS-CALIB-FUNC-019: Defect correction verification.
 * No REQ-P1A- requirement covers xpe_verify_* (spec.md does not mention them); the
 * REQ-P1A-012 this line used to cite is "Defect Correction Execution" (xpe_defect_correct),
 * not verification.
 * SRS-CALIB-FUNC-036 states the measured/unmeasured reporting contract for every
 * xpe_verify_*.
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
 * SRS-CALIB-FUNC-015 / SRS-CALIB-FUNC-021: Pipeline verification. No REQ-P1A- requirement
 * covers xpe_verify_* (spec.md does not mention them); the REQ-P1A-041..047 this line
 * used to cite were the pre-bc22093 pipeline-stage requirements, now REQ-P1A-095..101.
 *
 * PROVISIONAL threshold (QA-A-223, #242): the 2.0 dB line has no requirement basis (neither the
 * evaluation protocol nor the SRS defines it) and is kept, not derived. `snr_improvement_db` is
 * finite for finite input: a flat frame (std exactly 0) counts as an SNR of 200 dB, so a noisy
 * raw frame processed to a perfectly flat one passes and two flat frames do not (nothing
 * improved).
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
 * @brief (withdrawn) xpe_crc32 -- QA-A-152 (#216)
 *
 * This export is gone. It had no caller in clients/, gui/, modules/, tools/
 * or tests/ (build outputs excluded), and the integrity mechanism it served
 * was replaced by SHA-256 (SRS-CALIB-001:43-44). The doc comment said "used
 * internally by calibration manager", which had stopped being true.
 *
 * ABI NOTE: removing an export changes the DLL surface. No caller existed, so
 * nothing in this repository breaks; a binary built against an older header
 * that resolved this symbol would not.
 */

#ifdef __cplusplus
}
#endif

/* =============================================================================
 * Phase 12: BPM (Bad Pixel Map) Generation (SWU-1.11)
 * FUNC-022: Dark BPM generation using RMM (Robust Mask Maker)
 * FUNC-023: Bright BPM generation using local mean deviation
 * BPM merging (dark U bright) and reflect padding at the borders are implementation
 * behaviour, not requirements: SRS-CALIB-FUNC-024 and -025 are the gain frame-count tiers
 * and the post-BPM LineArtifactScore limit (QA-A-224, #216).
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
 * BPM Merging (implementation behaviour -- no SRS text states it; SRS-CALIB-FUNC-024 is
 * the gain frame-count tiers; QA-A-224, #216)
 *   - final_bpm[x,y] = max(dark_bpm[x,y], bright_bpm[x,y])
 *   - Values: 0=good, 1=dead/stuck, 2=hot/noisy, 3=both
 *
 * Reflect Padding (implementation behaviour -- no SRS text states it; SRS-CALIB-FUNC-025
 * is the post-BPM LineArtifactScore limit)
 *   - Window extraction uses reflect mode at boundaries
 *   - Prevents false detections at image borders
 *
 * SRS-CALIB-FUNC-022 and -023: the detection parameters of BPM generation for FPD calibration
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
 * - MULTI_POINT_10: 10 points, degree <= 4 (hard cap)
 * - AUTO: each generation uses the smallest mode above whose point count and
 *   degree accept the request (SRS-CALIB-FUNC-031 (5), issue \#169). The
 *   mode used is written to the XCal config JSON as `calibration_mode`
 *   (the requested one as `requested_calibration_mode`), to
 *   XpeCalibQualityMeta::calibration_mode, and as an XPE_ALERT_INFO entry.
 *
 * Enforcement (SRS-CALIB-FUNC-031 (3)(4)(8)): xpe_calib_generate_gain() and
 * xpe_calib_generate_gain_polynomial() return XPE_ERR_INVALID_INPUT when the
 * dose level count exceeds the active mode's points or the requested degree
 * exceeds its degree; under AUTO, when no mode accepts the request.
 */
typedef enum XpeCalibrationMode {
    XPE_CALIB_SINGLE_POINT   = 0,  ///< 1 point, constant fit
    XPE_CALIB_DUAL_POINT     = 1,  ///< 2 points, linear fit
    XPE_CALIB_MULTI_POINT_5  = 2,  ///< 5 points, quadratic fit
    XPE_CALIB_MULTI_POINT_8  = 3,  ///< 8 points, cubic fit (DEFAULT)
    XPE_CALIB_MULTI_POINT_10 = 4,  ///< 10 points, degree <= 4
    XPE_CALIB_AUTO           = 5   ///< Smallest fitting mode per generation (issue \#169)
} XpeCalibrationMode;

/**
 * @brief Quality metadata for calibration output
 *
 * Populated by calibration generation functions to enable quality assessment
 * and historical comparison.
 *
 * Fields:
 * - calibration_mode: Active calibration mode (XpeCalibrationMode)
 * - polynomial_degree: Fitted polynomial degree (0-4; 4 is MULTI_POINT_10)
 * - num_points: Number of dose points used (1-10)
 * - r_squared: Coefficient of determination: at most 1.0, and NEGATIVE for a fit worse than the mean (the
 *   generator reports such values). Any finite value up to 1.0 is a real one, -1.0 included; whether there
 *   is one is has_r_squared, not the value. When has_r_squared is 0, r_squared holds the fill value -1.0.
 * - has_r_squared: 1 when r_squared is a value (the file gave fit_r_squared, or a generation produced one);
 *   0 when it was not given (a file with other quality fields but no fit_r_squared)
 * - calibration_timestamp: Unix epoch milliseconds
 * - detector_serial: Detector identifier (null-terminated)
 * - firmware_version: Firmware version string (null-terminated)
 * - calibration_pass: Quality gate result (0=fail, 1=pass)
 * - previous_r_squared: R² of the last earlier record that had one; records without an R² (valid = 0, or
 *   has_r_squared = 0) are skipped. Whether there is one is has_previous_r_squared; with none it holds -1.0
 * - has_previous_r_squared: 1 when previous_r_squared is a value, 0 when no earlier record had an R²
 * - valid: 1 when the record describes a calibration; 0 when there is no quality metadata for the
 *   current one (every field describing the current record -- has_r_squared included -- is then zero; only the
 *   history, previous_r_squared and has_previous_r_squared, is kept from the record it replaced)
 *
 * has_r_squared and has_previous_r_squared are the presence of an R2, whatever its value; r_squared and
 * previous_r_squared hold the fill value -1.0 when there is none, which a real R2 can also equal.
 *
 * BUILD-MATCHED USE (QA-A-208e). valid, has_r_squared and has_previous_r_squared sit in what used to be padding
 * (offsets 3, 4 and 73); sizeof and every other offset are unchanged. A caller built against a header that has a
 * flag must be run against a module build that writes it: a module built BEFORE the flag existed leaves that byte
 * as padding, whose value is not defined, so the flag reads as noise. The other direction is safe -- a caller built
 * against an older header never looks at the byte. The module has no way to tell the generations apart at run time:
 * xpe_preprocess_version() returns a source constant ("0.1.0") that the changes which added these bytes did not
 * bump, so it cannot serve as the check. Ship the header and the module as one build; do not read a flag out of a
 * module of unknown build.
 */
typedef struct XpeCalibQualityMeta {
    uint8_t  calibration_mode;      ///< XpeCalibrationMode used by the generation (never AUTO)
    uint8_t  polynomial_degree;     ///< 0=constant, 1=linear, 2=quadratic, 3=cubic, 4=quartic (MULTI_POINT_10)
    uint8_t  num_points;            ///< Number of dose levels (1-10)
    uint8_t  valid;                 ///< 1 = describes a calibration; 0 = no quality metadata for the current one.
                                    ///< Sits in what was padding: sizeof and every other offset are unchanged.
    uint8_t  has_r_squared;         ///< 1 = r_squared is a value (any finite value <= 1.0, -1.0 included); 0 = not given.
                                    ///< Offset 4, in what was padding: sizeof and every other offset are unchanged.
    double   r_squared;             ///< Coefficient of determination: at most 1.0, negative for a fit worse than the mean;
                                    ///< meaningful when has_r_squared is 1 (the fill value is -1.0)
    uint64_t calibration_timestamp; ///< Unix epoch milliseconds
    char     detector_serial[32];   ///< Detector serial number (null-terminated)
    char     firmware_version[16];  ///< Firmware version (null-terminated)
    uint8_t  calibration_pass;      ///< 0=failed R² gate, 1=passed
    uint8_t  has_previous_r_squared;///< 1 = previous_r_squared is a value; 0 = no earlier record had an R².
                                    ///< Offset 73, in what was padding: sizeof and every other offset are unchanged.
    double   previous_r_squared;    ///< R² of the last earlier record that had one (-1.0 fill when none)
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
 * @brief Get quality metadata of the current gain calibration
 *
 * FUNC-033: Quality Metadata API
 *
 * Returns the record of the most recent gain calibration the module GENERATED (xpe_calib_generate_gain...) or
 * LOADED (xpe_calib_load_gain, the cached loader, the pipeline's calibration set). Every gain load replaces it
 * (QA-A-202e): with the file's quality metadata when the file carries it, and when the file carries NONE with the
 * "no quality" record -- `valid` is 0 and every field describing the current record, has_r_squared included, is
 * zero; the history (previous_r_squared and has_previous_r_squared) is kept from the record it replaced. A record
 * is never left from an earlier file as if it described the gain that is current. Check `valid` before using the
 * other fields, and has_r_squared before using r_squared: a file with quality fields but no fit_r_squared has
 * valid = 1 and has_r_squared = 0, and r_squared then holds the fill value -1.0 -- which is also a possible real
 * value, so it is the flag, never the value, that says whether there is an R2.
 * previous_r_squared is the history: the R2 of the last record that HAD one -- a valid record with
 * has_r_squared = 1 -- kept apart from the current record; has_previous_r_squared says whether there is one
 * (with none, previous_r_squared holds -1.0). A record that has no R2 -- no quality at all, or other quality
 * fields but no fit_r_squared -- does not interrupt it: the history passes through such records, with its flag.
 * After a generation the record describes the generated calibration (FUNC-033), which is not necessarily the map
 * in the store; the next gain load replaces it.
 *
 * The two has_* flags are valid only against a module build that writes them (see XpeCalibQualityMeta,
 * "BUILD-MATCHED USE"): a header that has them with an older module leaves them undefined.
 *
 * Thread-safe, and consistent with the calibration store (QA-A-202d): the record lives in the store and is read
 * under the lock the maps are replaced under, so a gain load makes the maps and this record current in one step --
 * a reader sees the previous file's record or the new one's, never the new maps beside the old quality, and
 * never a record half-way through being replaced.
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
 * @return Polynomial degree ceiling (0, 1, 2, 3, or 4); for AUTO, the
 *         largest ceiling (4) -- the degree used is resolved per generation
 */
XPE_API uint32_t xpe_calib_get_poly_degree(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_PREPROCESS_API_H */
