/**
 * @file mode_selector.h
 * @brief SWU-1.12 ModeSelector - Calibration mode selection and validation (FUNC-031)
 *
 * Implements the calibration mode selection logic for multi-point gain calibration.
 * This SWU validates mode requests, enforces runtime change blocking, and manages
 * the current mode state.
 *
 * Mode characteristics:
 * - SINGLE_POINT (1): Fastest, no nonlinearity correction
 * - DUAL_POINT (2): Linear correction only
 * - MULTI_POINT_5 (5): Quadratic correction, typical use case
 * - MULTI_POINT_8 (8): Cubic correction, recommended default
 * - MULTI_POINT_10 (10): Quartic correction, maximum accuracy
 * - AUTO (0): Automatic selection based on input data
 *
 * @ingroup xpe_preprocess
 */
#ifndef XPE_PREPROCESS_MODE_SELECTOR_H
#define XPE_PREPROCESS_MODE_SELECTOR_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the mode selector with default mode (MULTI_POINT_8).
 *
 * Must be called once during CalibManager initialization before any other
 * mode selector functions.
 *
 * @return XPE_OK on success, error code on failure
 */
int xpe_mode_selector_init(void);

/**
 * @brief Shutdown the mode selector and release resources.
 *
 * Called during CalibManager shutdown.
 */
void xpe_mode_selector_shutdown(void);

/**
 * @brief Set the calibration mode.
 *
 * Validates the requested mode and updates the current mode if valid.
 * Blocks runtime mode changes during clinical operation.
 *
 * @param mode Requested calibration mode
 * @return XPE_OK on success, XPE_ERR_INVALID_PARAM if mode is invalid,
 *         XPE_ERR_INVALID_STATE if called during runtime
 */
int xpe_mode_selector_set(XpeCalibrationMode mode);

/**
 * @brief Get the current calibration mode.
 *
 * Returns the currently configured mode. If set() was never called,
 * returns MULTI_POINT_8 (default).
 *
 * @param out_mode Pointer to receive current mode
 * @return XPE_OK on success, XPE_ERR_INVALID_PARAM if out_mode is NULL
 */
int xpe_mode_selector_get(XpeCalibrationMode* out_mode);

/**
 * @brief Check if mode changes are allowed (factory calibration only).
 *
 * Runtime mode changes are blocked during clinical operation to prevent
 * accidental misconfiguration.
 *
 * @return 1 if mode changes allowed, 0 if blocked
 */
int xpe_mode_selector_is_change_allowed(void);

/**
 * @brief Block mode changes (called when calibration is loaded for clinical use).
 */
void xpe_mode_selector_block_changes(void);

/**
 * @brief Allow mode changes (called during factory calibration workflow).
 */
void xpe_mode_selector_allow_changes(void);

/**
 * @brief Get the maximum polynomial degree for a given mode.
 *
 * Returns the maximum polynomial degree allowed for the specified mode:
 * - SINGLE_POINT: 0 (constant)
 * - DUAL_POINT: 1 (linear)
 * - MULTI_POINT_5: 2 (quadratic)
 * - MULTI_POINT_8: 3 (cubic)
 * - MULTI_POINT_10: 4 (quartic)
 * - AUTO: 4 (maximum, resolved during fitting)
 *
 * @param mode Calibration mode
 * @return Maximum polynomial degree (0-4), or -1 if mode is invalid
 */
int xpe_mode_selector_get_max_degree(XpeCalibrationMode mode);

/**
 * @brief Get the recommended point count for AUTO mode based on input data.
 *
 * Analyzes the input data range and SNR to recommend an appropriate mode.
 * This is a heuristic; the caller may override the recommendation.
 *
 * @param num_points Number of dose levels available in input data
 * @param snr_db Estimated signal-to-noise ratio in dB (0 if unknown)
 * @return Recommended mode (MULTI_POINT_5, MULTI_POINT_8, or MULTI_POINT_10)
 */
XpeCalibrationMode xpe_mode_selector_auto_select(int num_points, float snr_db);

#ifdef __cplusplus
}
#endif

#endif /* XPE_PREPROCESS_MODE_SELECTOR_H */
