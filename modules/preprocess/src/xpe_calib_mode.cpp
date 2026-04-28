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
constexpr double R_SQUARED_QUALITY_GATE = 0.999;

// 10-point hard cap to prevent excessive calibration points
constexpr uint32_t MAX_POINTS_HARD_CAP = 10;

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

/**
 * @brief Initialize quality metadata with defaults
 *
 * @param meta Metadata structure to initialize
 */
inline void init_quality_meta(XpeCalibQualityMeta* meta) noexcept {
    if (!meta) return;

    std::memset(meta, 0, sizeof(XpeCalibQualityMeta));
    meta->previous_r_squared = -1.0;  // Indicates no previous calibration
}

inline void copy_cstr(char* dst, size_t dst_size, const char* src) noexcept {
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;

    const size_t len = std::min(std::strlen(src), dst_size - 1);
    std::memcpy(dst, src, len);
    dst[len] = '\0';
}

inline void log_quality_regression(double previous_r_squared,
                                   double r_squared) noexcept {
    std::fprintf(stderr,
                 "pre: calibration R2 regression detected: previous=%.6f current=%.6f\n",
                 previous_r_squared,
                 r_squared);
}

/**
 * @brief Update quality metadata after calibration
 *
 * @param meta Metadata to update
 * @param mode Calibration mode used
 * @param degree Polynomial degree fitted
 * @param num_points Number of dose points
 * @param r_squared Coefficient of determination
 */
inline void update_quality_meta(XpeCalibQualityMeta* meta,
                                XpeCalibrationMode mode,
                                uint32_t degree,
                                uint32_t num_points,
                                double r_squared) noexcept {
    if (!meta) return;

    meta->calibration_mode = static_cast<uint8_t>(mode);
    meta->polynomial_degree = static_cast<uint8_t>(degree);
    meta->num_points = static_cast<uint8_t>(num_points);
    meta->r_squared = r_squared;

    // Set timestamp (unix epoch milliseconds)
    using namespace std::chrono;
    auto now = system_clock::now();
    auto duration = now.time_since_epoch();
    meta->calibration_timestamp = static_cast<uint64_t>(
        duration_cast<milliseconds>(duration).count());

    // R² quality gate: pass if R² >= 0.999
    meta->calibration_pass = (r_squared >= R_SQUARED_QUALITY_GATE) ? 1 : 0;

    // Previous R² is preserved from last calibration (already in meta)
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

namespace xpe::calib::mode {

/**
 * @brief Initialize quality metadata before calibration
 *
 * Called by calibration generation functions to prepare metadata.
 *
 * @param detector_serial Detector serial number (may be NULL)
 * @param firmware_version Firmware version (may be NULL)
 */
void init_metadata(const char* detector_serial,
                   const char* firmware_version) noexcept {
    init_quality_meta(&g_quality_meta);

    copy_cstr(g_quality_meta.detector_serial,
              sizeof(g_quality_meta.detector_serial),
              detector_serial);

    copy_cstr(g_quality_meta.firmware_version,
              sizeof(g_quality_meta.firmware_version),
              firmware_version);
}

/**
 * @brief Update metadata after polynomial fitting
 *
 * Called by calibration generation functions after fitting completes.
 *
 * @param degree Polynomial degree fitted
 * @param num_points Number of dose points used
 * @param r_squared Coefficient of determination
 * @return XPE_OK on success, XPE_ERR_CALIBRATION_POOR_QUALITY if R² < 0.999
 */
XpeErrorCode update_metadata(uint32_t degree,
                             uint32_t num_points,
                             double r_squared) noexcept {
    // Preserve previous R² for comparison
    double previous_r_squared = g_quality_meta.r_squared;

    // Update metadata with new calibration results
    update_quality_meta(&g_quality_meta, g_calib_mode, degree,
                        num_points, r_squared);

    // Restore previous R² for comparison
    g_quality_meta.previous_r_squared = previous_r_squared;

    // R² quality gate: fail if R² < 0.999
    if (r_squared < R_SQUARED_QUALITY_GATE) {
        return XPE_ERR_PROCESSING_FAILED;  // Calibration quality below threshold
    }

    // Warn if new R² is significantly worse than previous (regression detection)
    if (previous_r_squared >= 0.0 && r_squared < previous_r_squared - 0.01) {
        log_quality_regression(previous_r_squared, r_squared);
        // TODO: Log warning: calibration quality regression detected
        // For now, this is just a warning; calibration still passes R² gate
    }

    return XPE_OK;
}

/**
 * @brief Get max points for current mode (internal)
 *
 * @return Maximum dose points (enforces 10-point hard cap)
 */
uint32_t get_max_points(void) noexcept {
    uint32_t max_points = xpe_calib_get_max_points();

    // Enforce 10-point hard cap (FUNC-032 requirement)
    if (max_points > MAX_POINTS_HARD_CAP) {
        max_points = MAX_POINTS_HARD_CAP;
    }

    return max_points;
}

/**
 * @brief Get polynomial degree for current mode (internal)
 *
 * @return Polynomial degree for fitting
 */
uint32_t get_poly_degree(void) noexcept {
    return xpe_calib_get_poly_degree();
}

} // namespace xpe::calib::mode
