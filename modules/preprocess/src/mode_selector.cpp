/**
 * @file mode_selector.cpp
 * @brief SWU-1.12 ModeSelector implementation - Calibration mode selection (FUNC-031)
 *
 * Implements the calibration mode selection logic for multi-point gain calibration.
 * This component validates mode requests, enforces runtime change blocking, and
 * manages the current mode state.
 *
 * Thread safety: This component uses a mutex to protect the mode state.
 * All public functions are thread-safe.
 *
 * @ingroup xpe_preprocess
 */

#include "xpe/preprocess/mode_selector.h"

#include <memory>
#include <mutex>

// State management
namespace {
    struct ModeSelectorState {
        XpeCalibrationMode current_mode;
        bool changes_allowed;
        bool initialized;
        std::mutex mutex;

        ModeSelectorState()
            : current_mode(XPE_CALIB_MODE_MULTI_POINT_8)  // Default/recommended
            , changes_allowed(true)  // Initially allowed (factory calibration)
            , initialized(false)
        {}
    };

    std::unique_ptr<ModeSelectorState> g_state;
}

// Public API implementation

extern "C" {

int xpe_mode_selector_init(void) {
    if (g_state && g_state->initialized) {
        return XPE_OK;  // Already initialized
    }

    try {
        g_state = std::make_unique<ModeSelectorState>();
        g_state->initialized = true;
        return XPE_OK;
    } catch (...) {
        return XPE_ERR_NOT_INITIALIZED;
    }
}

void xpe_mode_selector_shutdown(void) {
    if (g_state) {
        std::lock_guard<std::mutex> lock(g_state->mutex);
        g_state->initialized = false;
    }
    g_state.reset();
}

int xpe_mode_selector_set(XpeCalibrationMode mode) {
    if (!g_state || !g_state->initialized) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    // Validate mode
    if (mode < XPE_CALIB_MODE_AUTO || mode > XPE_CALIB_MODE_MULTI_POINT_10) {
        if (mode != XPE_CALIB_MODE_SINGLE_POINT &&
            mode != XPE_CALIB_MODE_DUAL_POINT &&
            mode != XPE_CALIB_MODE_MULTI_POINT_5 &&
            mode != XPE_CALIB_MODE_MULTI_POINT_8 &&
            mode != XPE_CALIB_MODE_MULTI_POINT_10) {
            return XPE_ERR_INVALID_PARAM;
        }
    }

    std::lock_guard<std::mutex> lock(g_state->mutex);

    // Check if mode changes are allowed
    if (!g_state->changes_allowed) {
        return XPE_ERR_INVALID_STATE;
    }

    g_state->current_mode = mode;
    return XPE_OK;
}

int xpe_mode_selector_get(XpeCalibrationMode* out_mode) {
    if (!g_state || !g_state->initialized) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    if (!out_mode) {
        return XPE_ERR_INVALID_PARAM;
    }

    std::lock_guard<std::mutex> lock(g_state->mutex);
    *out_mode = g_state->current_mode;
    return XPE_OK;
}

int xpe_mode_selector_is_change_allowed(void) {
    if (!g_state || !g_state->initialized) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(g_state->mutex);
    return g_state->changes_allowed ? 1 : 0;
}

void xpe_mode_selector_block_changes(void) {
    if (g_state && g_state->initialized) {
        std::lock_guard<std::mutex> lock(g_state->mutex);
        g_state->changes_allowed = false;
    }
}

void xpe_mode_selector_allow_changes(void) {
    if (g_state && g_state->initialized) {
        std::lock_guard<std::mutex> lock(g_state->mutex);
        g_state->changes_allowed = true;
    }
}

int xpe_mode_selector_get_max_degree(XpeCalibrationMode mode) {
    switch (mode) {
        case XPE_CALIB_MODE_SINGLE_POINT:
            return 0;  // Constant
        case XPE_CALIB_MODE_DUAL_POINT:
            return 1;  // Linear
        case XPE_CALIB_MODE_MULTI_POINT_5:
            return 2;  // Quadratic
        case XPE_CALIB_MODE_MULTI_POINT_8:
            return 3;  // Cubic
        case XPE_CALIB_MODE_MULTI_POINT_10:
            return 4;  // Quartic
        case XPE_CALIB_MODE_AUTO:
            return 4;  // Maximum (resolved during fitting)
        default:
            return -1;  // Invalid mode
    }
}

XpeCalibrationMode xpe_mode_selector_auto_select(int num_points, float snr_db) {
    // Heuristic for AUTO mode selection based on input data characteristics

    // If insufficient points for multi-point, fall back to single/dual
    if (num_points < 2) {
        return XPE_CALIB_MODE_SINGLE_POINT;
    } else if (num_points < 5) {
        return XPE_CALIB_MODE_DUAL_POINT;
    }

    // High SNR (> 40 dB) allows maximum accuracy (10 points)
    if (snr_db > 40.0f && num_points >= 10) {
        return XPE_CALIB_MODE_MULTI_POINT_10;
    }

    // Good SNR (> 30 dB) uses recommended 8-point mode
    if (snr_db > 30.0f && num_points >= 8) {
        return XPE_CALIB_MODE_MULTI_POINT_8;
    }

    // Lower SNR or limited points uses 5-point mode
    if (num_points >= 5) {
        return XPE_CALIB_MODE_MULTI_POINT_5;
    }

    // Fallback to dual point if limited data
    return XPE_CALIB_MODE_DUAL_POINT;
}

}  // extern "C"
