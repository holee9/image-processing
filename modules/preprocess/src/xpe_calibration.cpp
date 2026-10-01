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

CalibSnapshot xpe_calib_snapshot_locked() noexcept
{
    CalibSnapshot s;
    s.initialized          = xpe_preprocess_is_initialized();
    s.offset_map           = g_calib.offset_map;
    s.offset_width         = g_calib.offset_width;
    s.offset_height        = g_calib.offset_height;
    s.gain_map             = g_calib.gain_map;
    s.gain_poly_coeffs     = g_calib.gain_poly_coeffs;
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
