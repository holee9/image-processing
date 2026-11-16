/**
 * @file xpe_calib_mode.cpp
 * @brief XPE Calibration Mode Selection API (FUNC-031~033)
 *
 * SPEC: SAD-CALIB-001 SWU-1.12 (FUNC-031~033)
 * IEC 62304 Class B
 *
 * Implementation:
 * - FUNC-031: Calibration mode selection (QA-A-336: SINGLE_POINT only)
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

// The only calibration mode (QA-A-336): one dose level, a constant (degree 0) gain. The multi-point modes went with the
// gain polynomial; the single-dose generator used to record MULTI_POINT_8 here, which said something false about its file.
XpeCalibrationMode g_calib_mode = XPE_CALIB_SINGLE_POINT;

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
    g_calib_mode = XPE_CALIB_SINGLE_POINT;   // the only mode (QA-A-336)
}

/* =============================================================================
 * FUNC-031 (QA-A-336): one mode. A generation of one dose level and degree 0 resolves to SINGLE_POINT; anything else
 * is XPE_ERR_INVALID_INPUT (no generator of this module asks for more since the gain polynomial was removed).
 * ============================================================================ */

XpeErrorCode xpe_calib_resolve_mode(int32_t num_levels, int32_t degree,
                                    XpeCalibrationMode* resolved) noexcept
{
    if (resolved == nullptr || num_levels != 1 || degree != 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    *resolved = XPE_CALIB_SINGLE_POINT;
    return XPE_OK;
}

/* =============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Set calibration mode (only XPE_CALIB_SINGLE_POINT is accepted, QA-A-336)
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
    if (mode != XPE_CALIB_SINGLE_POINT) {   // QA-A-336: the multi-point modes and AUTO were removed
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
 * @return Current calibration mode (always XPE_CALIB_SINGLE_POINT, QA-A-336)
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
#ifdef XPE_CACHE_TEST_HOOKS
void (*xpe_calib_quality_before_lock_hook)() = nullptr;
#endif

XpeErrorCode xpe_calib_get_quality_meta(XpeCalibQualityMeta* meta) {
    if (!meta) {
        return XPE_ERR_INVALID_INPUT;
    }
#ifdef XPE_CACHE_TEST_HOOKS
    if (xpe_calib_quality_before_lock_hook) xpe_calib_quality_before_lock_hook();
#endif

    // Copy current metadata to output, under the lock the maps are moved under (QA-A-202d): the record is
    // never read half-way through a replacement, and never beside maps it does not belong to.
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    std::memcpy(meta, &g_calib.quality_meta, sizeof(XpeCalibQualityMeta));
    return XPE_OK;
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

/**
 * The R2 history after the record `replaced` is replaced: its own R2 when it HAS one -- the record is valid and
 * has_r_squared is set, whatever the value (a real R2 may be negative, and -1.0 is a possible one) -- otherwise the
 * history it carried. A record with other quality fields but no fit_r_squared, and the "no quality" record, therefore
 * pass the last known R2 on (QA-A-202f, Codex #41). With nothing known the value is the fill -1.0 and the flag 0
 * (QA-A-208d: the flag, not a marker value, says whether there is one).
 */
static void r2_history_after(const XpeCalibQualityMeta& replaced, double* value, uint8_t* has) noexcept
{
    if (replaced.valid != 0 && replaced.has_r_squared != 0) {
        *value = replaced.r_squared;
        *has = 1;
    } else {
        *value = replaced.previous_r_squared;
        *has = replaced.has_previous_r_squared;
    }
}

bool xpe_calib_record_quality_meta(const XpeCalibQualityMeta& meta) noexcept
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    XpeCalibQualityMeta& qm = g_calib.quality_meta;
    // The history: the R2 of the record being replaced when it has one, otherwise the history it carried.
    double previous = XPE_R_SQUARED_NOT_GIVEN;
    uint8_t hasPrevious = 0;
    r2_history_after(qm, &previous, &hasPrevious);

    qm = meta;
    qm.valid = 1;
    qm.has_r_squared = 1;              // a generation always produces an R2, whatever its value
    // calibration_mode is the mode the generator resolved: SINGLE_POINT (QA-A-336).
    qm.previous_r_squared = previous;
    qm.has_previous_r_squared = hasPrevious;

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
 * integer in [lo, hi] -- the range the field has by definition -- so an empty value, a value that is not a scalar,
 * a malformed number and a number outside the range are all refusals (QA-A-204, #233: atoi turned a malformed
 * value into 0, or truncated it, without a word; QA-A-205b, Codex #29 B1: an empty value was read as "not given";
 * QA-A-208c: a value no generation can write is a defect of the signed data, not a quality). The key is a TOP-LEVEL
 * member of the config object; a nested object's key of the same name is not it (QA-A-208, Codex #34 B2).
 */
static bool read_u8_field(const XpeConfigDoc& doc, const char* key, int32_t lo, int32_t hi, uint8_t* dst,
                          bool* present)
{
    const XpeConfigEntry* e = doc.find(std::string(key));
    if (e == nullptr) return true;
    if (!e->scalar) return false;                  // an object or an array
    int32_t n = 0;
    if (!xpe_strict::parse_int(e->text, &n) || n < lo || n > hi) return false;
    *dst = static_cast<uint8_t>(n);
    *present = true;
    return true;
}

XpeErrorCode xpe_calib_parse_quality_meta(const XpeConfigDoc& doc, XpeCalibQualityMeta* out, bool* found)
{
    if (found != nullptr) *found = false;
    if (out == nullptr || found == nullptr) return XPE_ERR_INVALID_INPUT;

    // A file from before QA-A-35 has none of these keys. Each absent field
    // keeps its no-data value instead of failing the load.
    XpeCalibQualityMeta meta{};
    meta.r_squared          = -1.0;
    meta.previous_r_squared = -1.0;

    bool anyPresent = false;

    // The range of each field is what no generation can step outside of (xpe_calib_generate_gain.cpp):
    //   fit_r_squared      <= 1  (1 - SS_res / SS_tot, so never above 1. NOT bounded below: the generator reports a
    //                             negative value when the fit is worse than the mean -- -0.0766 for the poly-fixture
    //                             ladder, and exactly -1.0 is possible -- and a file it wrote must load. Whether the
    //                             file gave one is has_r_squared (the key is there), never the value)
    //   polynomial_degree  0..4  (the fitted degree; files from the removed gain polynomial carried up to 4)
    //   actual_dose_levels 1..10 (the dose points; files from the removed multi-point modes carried up to 10)
    //   calibration_mode   0..4  (QA-A-336: read as a RECORD only. Generators write 0 now; a single-map gain file
    //                             written before carries the mode that was active then -- 3, the old default, and
    //                             1, 2 or 4 when set -- and must still load. The value never selects anything.)
    if (const XpeConfigEntry* r2 = doc.find(std::string("fit_r_squared"))) {
        if (!r2->scalar) return XPE_ERR_CONFIG_INVALID;
        if (!xpe_strict::parse_double(r2->text, &meta.r_squared)) return XPE_ERR_CONFIG_INVALID;
        if (!(meta.r_squared <= 1.0)) return XPE_ERR_CONFIG_INVALID;
        meta.has_r_squared = 1;
        anyPresent = true;
    }
    if (!read_u8_field(doc, "polynomial_degree", 0, 4, &meta.polynomial_degree, &anyPresent) ||
        !read_u8_field(doc, "actual_dose_levels", 1, 10, &meta.num_points, &anyPresent) ||
        !read_u8_field(doc, "calibration_mode", 0, 4, &meta.calibration_mode, &anyPresent)) {
        return XPE_ERR_CONFIG_INVALID;
    }

    if (!anyPresent) return XPE_OK;   // none of the fields: *found stays false

    // The gate verdict is derived, never read from the file: a file claiming it
    // passed does not make it so.
    meta.calibration_pass =
        (meta.has_r_squared != 0 && meta.r_squared >= XPE_CALIB_R_SQUARED_GATE) ? 1u : 0u;

    meta.valid = 1;
    *out = meta;
    *found = true;
    return XPE_OK;
}

void xpe_calib_commit_quality_meta_locked(const XpeCalibQualityMeta& parsed) noexcept
{
    XpeCalibQualityMeta& qm = g_calib.quality_meta;
    double previous = XPE_R_SQUARED_NOT_GIVEN;
    uint8_t hasPrevious = 0;
    r2_history_after(qm, &previous, &hasPrevious);
    qm = parsed;
    qm.valid = 1;
    qm.previous_r_squared = previous;
    qm.has_previous_r_squared = hasPrevious;
}

void xpe_calib_commit_no_quality_locked() noexcept
{
    XpeCalibQualityMeta& qm = g_calib.quality_meta;
    double previous = XPE_R_SQUARED_NOT_GIVEN;
    uint8_t hasPrevious = 0;
    r2_history_after(qm, &previous, &hasPrevious);
    qm = XpeCalibQualityMeta{};          // valid = 0 and every field zero: nothing is known about the current gain
    qm.previous_r_squared = previous;    // ...except the history, which stays apart from the current record
    qm.has_previous_r_squared = hasPrevious;
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
 * (QA-A-336: the public exports `xpe_calib_get_max_points` / `xpe_calib_get_poly_degree` were removed with the
 * multi-point modes.)
 * ============================================================================ */
