/**
 * @file xpe_calib_mode.cpp
 * @brief XPE Calibration Mode Selection API (FUNC-031~033)
 *
 * SPEC: SAD-CALIB-001 SWU-1.12 (FUNC-031~033)
 * IEC 62304 Class B
 *
 * Implementation:
 * - FUNC-031: Calibration mode selection (6 modes)
 * - FUNC-032: Online accumulative fitting (O(W×H×degree) memory)
 * - FUNC-033: Quality metadata (8 mandatory fields)
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>

/* =============================================================================
 * Internal State
 * ============================================================================ */

namespace {

// Default calibration mode: MULTI_POINT_8 (8 points, cubic)
// Per Schmidgunst 2007 industry standard
XpeCalibrationMode g_calib_mode = XPE_CALIB_MULTI_POINT_8;

// Quality metadata from last calibration
XpeCalibQualityMeta g_quality_meta = []{
    XpeCalibQualityMeta m{};
    m.previous_r_squared = -1.0;
    return m;
}();

// R² quality gate threshold (0.999 = 99.9% fit quality required)

// 10-point hard cap to prevent excessive calibration points

/* =============================================================================
 * Mode-to-Parameters Mapping
 * ============================================================================ */

/**
 * @brief Calibration mode parameters
 *
 * Maps each mode to its max_points and poly_degree values.
 */
struct ModeParams {
    uint32_t max_points;   ///< Maximum number of dose points
    uint32_t poly_degree;  ///< Polynomial degree (0=constant, 1=linear, etc.)
};

// Mode-to-params mapping table
constexpr ModeParams kModeParams[] = {
    /* XPE_CALIB_SINGLE_POINT   */ { 1, 0 },  // Constant fit
    /* XPE_CALIB_DUAL_POINT     */ { 2, 1 },  // Linear fit
    /* XPE_CALIB_MULTI_POINT_5  */ { 5, 2 },  // Quadratic fit
    /* XPE_CALIB_MULTI_POINT_8  */ { 8, 3 },  // Cubic fit (DEFAULT)
    /* XPE_CALIB_MULTI_POINT_10 */ {10, 3 },  // Cubic fit
    /* XPE_CALIB_AUTO           */ {10, 3 }   // Adaptive (max 10, cubic)
};

static_assert(sizeof(kModeParams) / sizeof(kModeParams[0]) == 6u,
              "Mode params table must have 6 entries");

/* =============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Validate calibration mode value
 *
 * @param mode Mode to validate
 * @return true if valid, false otherwise
 */
inline bool is_valid_mode(XpeCalibrationMode mode) noexcept {
    return mode >= XPE_CALIB_SINGLE_POINT && mode <= XPE_CALIB_AUTO;
}

/**
 * @brief Get mode parameters for a given mode
 *
 * @param mode Calibration mode
 * @return Mode parameters (max_points, poly_degree)
 */
inline ModeParams get_mode_params(XpeCalibrationMode mode) noexcept {
    if (is_valid_mode(mode)) {
        return kModeParams[static_cast<size_t>(mode)];
    }
    // Invalid mode: return safest defaults (1 point, constant)
    return {1, 0};
}

} // anonymous namespace

/* =============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Set calibration mode for polynomial fitting
 *
 * FUNC-031: Mode Selection API
 *
 * Validates the mode value and updates the global calibration mode.
 *
 * @param mode Calibration mode to set
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if mode value is invalid
 */
XpeErrorCode xpe_calib_set_mode(XpeCalibrationMode mode) {
    if (!is_valid_mode(mode)) {
        return XPE_ERR_INVALID_INPUT;
    }

    g_calib_mode = mode;
    return XPE_OK;
}

/**
 * @brief Get current calibration mode
 *
 * FUNC-031: Mode Selection API
 *
 * @return Current calibration mode (default: XPE_CALIB_MULTI_POINT_8)
 */
XpeCalibrationMode xpe_calib_get_mode(void) {
    return g_calib_mode;
}

/**
 * @brief Get quality metadata from last calibration
 *
 * FUNC-033: Quality Metadata API
 *
 * Returns a copy of the quality metadata structure populated during
 * the last calibration generation operation.
 *
 * @param meta Output: Quality metadata (caller-owned)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT if meta is NULL
 */
XpeErrorCode xpe_calib_get_quality_meta(XpeCalibQualityMeta* meta) {
    if (!meta) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Copy current metadata to output
    std::memcpy(meta, &g_quality_meta, sizeof(XpeCalibQualityMeta));
    return XPE_OK;
}

/**
 * @brief Get maximum number of dose points for current mode
 *
 * FUNC-031: Mode-to-params mapping
 *
 * @return Maximum dose points (1, 2, 5, 8, or 10)
 */
uint32_t xpe_calib_get_max_points(void) {
    ModeParams params = get_mode_params(g_calib_mode);
    return params.max_points;
}

/**
 * @brief Get polynomial degree for current mode
 *
 * FUNC-031: Mode-to-params mapping
 *
 * @return Polynomial degree (0, 1, 2, or 3)
 */
uint32_t xpe_calib_get_poly_degree(void) {
    ModeParams params = get_mode_params(g_calib_mode);
    return params.poly_degree;
}

/* =============================================================================
 * Internal API for Calibration Generation
 * ============================================================================ */

/* =============================================================================
 * Internal API for Calibration Generation — REMOVED (QA-A-34, #120)
 *
 * `xpe::calib::mode::{init_metadata, update_metadata, get_max_points,
 * get_poly_degree}` lived here. QA-A-32 measured that nothing called them
 * (repo-wide grep: zero hits outside this file) and that they were declared in
 * no header, so no consumer could reach them; they accounted for 52 of this
 * file's 82 instrumented lines, all uncovered.
 *
 * Deleting them also retired the four static helpers they alone used
 * (`init_quality_meta`, `copy_cstr`, `log_quality_regression`,
 * `update_quality_meta`).
 *
 * The public exports `xpe_calib_get_max_points` / `xpe_calib_get_poly_degree`
 * are NOT affected: they are defined separately above and the dependency ran
 * the other way — the dead wrappers called them, not the reverse.
 * ============================================================================ */
