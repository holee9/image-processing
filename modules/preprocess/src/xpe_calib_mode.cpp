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

#include "xpe_strict_parse.hpp"
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <mutex>

/* =============================================================================
 * Internal State
 * ============================================================================ */

namespace {

// Default calibration mode: MULTI_POINT_8 (8 points, cubic)
// Per Schmidgunst 2007 industry standard
XpeCalibrationMode g_calib_mode = XPE_CALIB_MULTI_POINT_8;

// (The quality metadata of the last calibration is g_calib.quality_meta -- in the store, under g_calib_mutex.)

// R² quality gate threshold (0.999 = 99.9% fit quality required)

// 10-point hard cap to prevent excessive calibration points

}  // namespace

/**
 * Restores the module global this file owns to its start-up value.
 *
 * QA-A-120 (#176), lead decision: xpe_preprocess_shutdown() clears ALL module
 * globals. It used to clear g_calib only and leave the calibration mode and the
 * quality metadata, which made the name a lie. The mode lives in an anonymous
 * namespace here, so the lifecycle code cannot reach it directly and calls this
 * instead. The quality metadata is part of g_calib now (QA-A-202d) and is cleared
 * with it; this runs while shutdown holds g_calib_mutex, so it must not take it.
 */
void xpe_calib_mode_reset_globals() noexcept {
    g_calib_mode = XPE_CALIB_MULTI_POINT_8;   // the FUNC-031 (7) default
}

namespace {

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
    /* XPE_CALIB_MULTI_POINT_10 */ {10, 4 },  // Quartic ceiling (SRS FUNC-031: degree <= 4)
    /* XPE_CALIB_AUTO           */ {10, 4 }   // Ceiling only; the mode used is resolved per call (#169)
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
 * FUNC-031 (3)(4)(5)(8): mode enforcement and AUTO resolution (QA-A-101, #169)
 * ============================================================================ */

XpeErrorCode xpe_calib_resolve_mode(int32_t num_levels, int32_t degree,
                                    XpeCalibrationMode* resolved) noexcept
{
    if (resolved == nullptr || num_levels < 1 || degree < 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    const auto fits = [&](XpeCalibrationMode m) {
        const ModeParams p = get_mode_params(m);
        return static_cast<uint32_t>(num_levels) <= p.max_points &&
               static_cast<uint32_t>(degree) <= p.poly_degree;
    };

    const XpeCalibrationMode active = g_calib_mode;
    if (active != XPE_CALIB_AUTO) {
        // (3)(4): an explicit mode caps the level count (and its degree).
        if (!fits(active)) {
            return XPE_ERR_INVALID_INPUT;
        }
        *resolved = active;
        return XPE_OK;
    }

    // (5): the smallest explicit mode that accepts this request. The table is
    // ordered by max_points, so the first fit is the smallest.
    for (int m = XPE_CALIB_SINGLE_POINT; m < XPE_CALIB_AUTO; ++m) {
        const auto mode = static_cast<XpeCalibrationMode>(m);
        if (fits(mode)) {
            *resolved = mode;
            return XPE_OK;
        }
    }
    return XPE_ERR_INVALID_INPUT;   // beyond MULTI_POINT_10, the hard cap
}

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

    // Copy current metadata to output, under the lock the maps are moved under (QA-A-202d): the record is
    // never read half-way through a replacement, and never beside maps it does not belong to.
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    std::memcpy(meta, &g_calib.quality_meta, sizeof(XpeCalibQualityMeta));
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
 * @return Polynomial degree ceiling (0, 1, 2, 3, or 4)
 */
uint32_t xpe_calib_get_poly_degree(void) {
    ModeParams params = get_mode_params(g_calib_mode);
    return params.poly_degree;
}

/* =============================================================================
 * Internal API for Calibration Generation
 * ============================================================================ */

/* =============================================================================
 * FUNC-033 quality metadata recording (QA-A-35, #140)
 *
 * Declared in xpe_preprocess_internal.h. This is the live wiring that replaces
 * the dead xpe::calib::mode namespace QA-A-34 deleted: same requirement, a path
 * that is actually reachable.
 * ============================================================================ */

bool xpe_calib_record_quality_meta(const XpeCalibQualityMeta& meta) noexcept
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    XpeCalibQualityMeta& qm = g_calib.quality_meta;
    const double previous = qm.r_squared;

    qm = meta;
    // calibration_mode is the mode the generator resolved (never AUTO); the
    // caller sets it from xpe_calib_resolve_mode (#169).
    qm.previous_r_squared =
        (qm.calibration_timestamp == 0 && previous == 0.0) ? -1.0 : previous;

    using namespace std::chrono;
    qm.calibration_timestamp = static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

    // FUNC-033 (2): the gate is R2 >= 0.999 (SRS-CALIB-001 SRS-CALIB-FUNC-033).
    const bool passed = (qm.r_squared >= XPE_CALIB_R_SQUARED_GATE);
    qm.calibration_pass = passed ? 1u : 0u;
    return passed;
}

/**
 * Reads one FUNC-033 field into a uint8: an ABSENT key keeps the default; a key that is present must hold an
 * integer in [0, 255], so an empty value, a value that is not a scalar and a malformed number are all refusals
 * (QA-A-204, #233: atoi turned a malformed value into 0, or truncated it, without a word; QA-A-205b, Codex #29
 * B1: an empty value was read as "not given").
 */
static bool read_u8_field(const char* json, const char* key, uint8_t* dst, bool* present)
{
    std::string v;
    const XpeJsonKey state = xpe_json_find_scalar(json, key, &v);
    if (state == XpeJsonKey::Absent) return true;
    if (state != XpeJsonKey::Scalar) return false;
    int32_t n = 0;
    if (!xpe_strict::parse_int(v, &n) || n < 0 || n > 255) return false;
    *dst = static_cast<uint8_t>(n);
    *present = true;
    return true;
}

XpeErrorCode xpe_calib_parse_quality_meta_json(const char* configJson, XpeCalibQualityMeta* out, bool* found)
{
    if (found != nullptr) *found = false;
    if (configJson == nullptr || out == nullptr || found == nullptr) return XPE_ERR_INVALID_INPUT;

    // A file from before QA-A-35 has none of these keys. Each absent field
    // keeps its no-data value instead of failing the load.
    XpeCalibQualityMeta meta{};
    meta.r_squared          = -1.0;
    meta.previous_r_squared = -1.0;

    bool anyPresent = false;

    std::string r2;
    const XpeJsonKey r2State = xpe_json_find_scalar(configJson, "fit_r_squared", &r2);
    if (r2State == XpeJsonKey::NotScalar) return XPE_ERR_CONFIG_INVALID;
    if (r2State == XpeJsonKey::Scalar) {
        if (!xpe_strict::parse_double(r2, &meta.r_squared)) return XPE_ERR_CONFIG_INVALID;
        anyPresent = true;
    }
    if (!read_u8_field(configJson, "polynomial_degree", &meta.polynomial_degree, &anyPresent) ||
        !read_u8_field(configJson, "actual_dose_levels", &meta.num_points, &anyPresent) ||
        !read_u8_field(configJson, "calibration_mode", &meta.calibration_mode, &anyPresent)) {
        return XPE_ERR_CONFIG_INVALID;
    }

    if (!anyPresent) return XPE_OK;   // none of the fields: *found stays false

    // The gate verdict is derived, never read from the file: a file claiming it
    // passed does not make it so.
    meta.calibration_pass =
        (meta.r_squared >= XPE_CALIB_R_SQUARED_GATE) ? 1u : 0u;

    *out = meta;
    *found = true;
    return XPE_OK;
}

void xpe_calib_commit_quality_meta_locked(const XpeCalibQualityMeta& parsed) noexcept
{
    XpeCalibQualityMeta& qm = g_calib.quality_meta;
    const double previous = qm.r_squared;
    qm = parsed;
    qm.previous_r_squared = previous;
}

void xpe_calib_commit_quality_meta(const XpeCalibQualityMeta& parsed) noexcept
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    xpe_calib_commit_quality_meta_locked(parsed);
}

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
