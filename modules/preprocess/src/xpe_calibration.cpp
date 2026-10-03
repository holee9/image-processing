/**
 * @file xpe_calibration.cpp
 * @brief XPE Calibration Global State definitions + xpe_validate_readout_artifact
 *
 * Defines global g_calib + g_calib_mutex (shared across all calibration functions).
 * All per-function implementations have been split into separate translation units:
 *   T-006: xpe_calib_load_offset/gain/defect_map.cpp
 *   T-007: xpe_calib_save.cpp
 *   T-008: xpe_calib_generate_offset.cpp
 *   T-009: xpe_calib_check_expiry.cpp
 *
 * REQ-P1A-031: RAII via unique_ptr for automatic cleanup
 * REQ-P1A-041: xpe_validate_readout_artifact remains here (Phase 2 misc)
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include <mutex>
#include <cstring>

// =============================================================================
// Internal Calibration State
// =============================================================================

// Global calibration data and mutex definitions
CalibrationData g_calib;
std::mutex g_calib_mutex;

// QA-A-229 M4 (#245): session consistency between the loaded maps -- see xpe_preprocess_internal.h.
namespace {
struct SessionSlot { const char* id; bool loaded; };

XpeErrorCode session_check(const SessionSlot (&others)[2], const char* incoming64, bool* shouldWarn) noexcept
{
    for (const SessionSlot& o : others) {
        if (o.loaded && xpe_session_conflict(incoming64, o.id)) return XPE_ERR_CONFIG_INVALID;
    }
    int loadedCount = 1;
    bool anyUnspecified = !xpe_session_specified(incoming64);
    for (const SessionSlot& o : others) {
        if (!o.loaded) continue;
        ++loadedCount;
        if (!xpe_session_specified(o.id)) anyUnspecified = true;
    }
    // The state AFTER this commit, and the transition of the warning flag, under the caller's lock.
    const bool shouldPush = xpe_calib_session_transition_locked((loadedCount >= 2) && anyUnspecified);
    if (shouldWarn) *shouldWarn = shouldPush;
    return XPE_OK;
}
} // namespace

bool xpe_calib_session_transition_locked(bool mixed) noexcept
{
    if (!mixed) { g_calib.session_warned = false; return false; }
    if (g_calib.session_warned) return false;
    g_calib.session_warned = true;
    return true;
}

XpeErrorCode xpe_calib_session_check_locked(CalibMapKind kind, const char* incoming64, bool* unspecifiedMixed) noexcept
{
    const SessionSlot off{g_calib.offset_session_id, g_calib.offset_map != nullptr};
    const SessionSlot gain{g_calib.gain_session_id, g_calib.gain_map != nullptr || g_calib.gain_poly_coeffs != nullptr};
    const SessionSlot def{g_calib.defect_session_id, g_calib.defect_map != nullptr};
    switch (kind) {
        case CalibMapKind::Offset: { const SessionSlot o[2] = {gain, def}; return session_check(o, incoming64, unspecifiedMixed); }
        case CalibMapKind::Gain:   { const SessionSlot o[2] = {off, def};  return session_check(o, incoming64, unspecifiedMixed); }
        case CalibMapKind::Defect: { const SessionSlot o[2] = {off, gain}; return session_check(o, incoming64, unspecifiedMixed); }
    }
    return XPE_ERR_INVALID_INPUT;
}

XpeErrorCode xpe_calib_session_check_set(const char* offset64, const char* gain64, const char* defect64,
                                         bool* unspecifiedMixed) noexcept
{
    if (xpe_session_conflict(offset64, gain64) || xpe_session_conflict(offset64, defect64) ||
        xpe_session_conflict(gain64, defect64)) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (unspecifiedMixed) {
        *unspecifiedMixed = !xpe_session_specified(offset64) || !xpe_session_specified(gain64) ||
                            !xpe_session_specified(defect64);
    }
    return XPE_OK;
}

#ifdef XPE_CACHE_TEST_HOOKS
void (*xpe_session_after_commit_hook)() = nullptr;
#endif

void xpe_calib_session_warn(bool shouldWarn) noexcept
{
#ifdef XPE_CACHE_TEST_HOOKS
    if (xpe_session_after_commit_hook) xpe_session_after_commit_hook();
#endif
    if (!shouldWarn) return;
    try {
        xpe_alert_push("XPE_WARN_CALIB_SESSION_UNSPECIFIED: calibration maps are loaded together and at least one carries "
                       "no session id (empty or generated); session consistency is checked only between maps "
                       "that carry one",
                       XPE_ALERT_WARNING);
    } catch (...) {
        // advisory: lost under memory pressure
    }
}

CalibSnapshot xpe_calib_snapshot_locked() noexcept
{
    CalibSnapshot s;
    s.initialized          = xpe_preprocess_is_initialized();
    s.offset_map           = g_calib.offset_map;
    s.offset_width         = g_calib.offset_width;
    s.offset_height        = g_calib.offset_height;
    s.gain_map             = g_calib.gain_map;
    s.gain_poly_coeffs     = g_calib.gain_poly_coeffs;
    s.gain_defect_idx      = g_calib.gain_defect_idx;
    s.gain_defect_count    = g_calib.gain_defect_count;
    s.gain_poly_num_coeffs = g_calib.gain_poly_num_coeffs;
    s.gain_poly_has_range  = g_calib.gain_poly_has_range;
    s.gain_poly_dose_min   = g_calib.gain_poly_dose_min;
    s.gain_poly_dose_max   = g_calib.gain_poly_dose_max;
    s.gain_width           = g_calib.gain_width;
    s.gain_height          = g_calib.gain_height;
    s.defect_map           = g_calib.defect_map;
    s.defect_width         = g_calib.defect_width;
    s.defect_height        = g_calib.defect_height;
    return s;
}

CalibSnapshot xpe_calib_snapshot() noexcept
{
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    return xpe_calib_snapshot_locked();
}

// @MX:ANCHOR: [AUTO] Global calibration data shared across preprocessing algorithms
// xpe_validate_readout_artifact is defined in readout_validate.cpp (legacy 4-arg API)
