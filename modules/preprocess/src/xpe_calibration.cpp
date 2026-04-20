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

// @MX:ANCHOR: [AUTO] Global calibration data shared across preprocessing algorithms
// xpe_validate_readout_artifact is defined in readout_validate.cpp (legacy 4-arg API)
