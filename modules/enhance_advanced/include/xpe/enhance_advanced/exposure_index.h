/**
 * @file exposure_index.h
 * @brief IEC 62494-1 Exposure Index and Deviation Index calculation
 *
 * Implements IEC 62494-1 standard for Exposure Index (EI) and Deviation Index (DI).
 *
 * REQ-ADV-013: Exposure index calculation execution
 * REQ-ADV-022: NULL pointer input guard
 * REQ-ADV-032: No NaN/Inf in output
 *
 * IEC 62494-1 Formulas:
 * - EI = c1 * g * mean(pixel_values_roi) + c2
 * - DI = 10 * log10(EI / EI_target)
 */

#ifndef XPE_ENHANCE_ADVANCED_EXPOSURE_INDEX_H
#define XPE_ENHANCE_ADVANCED_EXPOSURE_INDEX_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <cstdint>
#include <string>

namespace xpe {
namespace enhance_advanced {

/**
 * @brief EI target lookup table for different body parts
 *
 * IEC 62494-1 typical EI_target values (Reference: Table B.1)
 *
 * @MX:ANCHOR: [AUTO] Body-part EI target lookup
 * @MX:REASON: Core lookup table used by xpe_calc_exposure_index, high fan_in expected
 * @MX:SPEC: REQ-ADV-013
 */
struct EITargetTable {
    static constexpr float CHEST = 250.0f;      // Chest PA
    static constexpr float CHEST_LAT = 200.0f;  // Chest Lateral
    static constexpr float ABDOMEN = 400.0f;    // Abdomen
    static constexpr float PELVIS = 350.0f;     // Pelvis
    static constexpr float SKULL = 500.0f;      // Skull
    static constexpr float EXTREMITY = 100.0f;  // Extremity
    static constexpr float SPINE = 300.0f;      // Spine
    static constexpr float DEFAULT = 250.0f;    // Default (chest)

    /**
     * @brief Get EI target for body part string
     * @param bodyPartStr Body part string (e.g., "CHEST", "ABDOMEN")
     * @return EI target value
     */
    static float getTarget(const char* bodyPartStr);
};

/**
 * @brief IEC 62494-1 EI calculator
 *
 * Computes Exposure Index (EI) and Deviation Index (DI) according to
 * IEC 62494-1 standard.
 *
 * Performance target: < 50ms for 3072x3072 FLOAT32 frame (scalar)
 */
class ExposureIndexCalculator {
public:
    /**
     * @brief System constants for EI calculation
     *
     * These constants are system-specific and typically calibrated
     * during system installation. Default values are provided for
     * generic FPD systems.
     *
     * c1: Sensitivity coefficient (EI per pixel value per unit gain)
     * c2: Offset coefficient (accounts for dark current, scatter)
     */
    struct Constants {
        float c1 = 100.0f;  // Sensitivity coefficient
        float c2 = 0.0f;    // Offset coefficient

        // QC Alert threshold
        float diAlertThreshold = 3.0f;  // |DI| > 3 triggers QC alert
    };

    /**
     * @brief Calculate EI and DI for image
     *
     * @param img Input image buffer (FLOAT32, detector domain)
     * @param meta Image metadata (body part string, kVp, mAs)
     * @param eiOut Output: Calculated Exposure Index
     * @param diOut Output: Calculated Deviation Index
     * @return XPE_OK on success, error code on failure
     *
     * @note Gain is estimated from kVp and mAs if not directly available
     * @note ROIs are not yet implemented (future: integrate with collimation detection)
     */
    static XpeErrorCode calculate(
        const XpeImageBuffer* img,
        const XpeImageMetadata* meta,
        float* eiOut,
        float* diOut);

private:
    /**
     * @brief Estimate gain from acquisition parameters
     * @param meta Image metadata
     * @return Estimated gain (dimensionless)
     */
    static float estimateGain(const XpeImageMetadata* meta);

private:
    /**
     * @brief Validate input parameters
     * @return XPE_OK if valid, error code otherwise
     */
    static XpeErrorCode validateInputs(
        const XpeImageBuffer* img,
        const XpeImageMetadata* meta,
        const float* eiOut,
        const float* diOut);

    /**
     * @brief Calculate mean pixel value of image (ROI-aware in future)
     * @param img Input image
     * @return Mean pixel value, or 0 if image is empty/invalid
     */
    static float calculateMean(const XpeImageBuffer* img);

    /**
     * @brief Calculate EI using IEC 62494-1 formula
     * @param meanPixel Mean pixel value in ROI
     * @param gain System gain
     * @param constants System constants
     * @return Calculated EI
     */
    static float calculateEI(float meanPixel, float gain, const Constants& constants);

    /**
     * @brief Calculate DI using IEC 62494-1 formula
     * @param ei Calculated EI
     * @param eiTarget Target EI for body part
     * @return Calculated DI
     */
    static float calculateDI(float ei, float eiTarget);
};

} // namespace enhance_advanced
} // namespace xpe

#endif // XPE_ENHANCE_ADVANCED_EXPOSURE_INDEX_H
