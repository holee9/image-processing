/**
 * @file exposure_index.cpp
 * @brief IEC 62494-1 Exposure Index and Deviation Index calculation implementation
 *
 * REQ-ADV-013: Exposure index calculation execution
 * REQ-ADV-022: NULL pointer input guard
 * REQ-ADV-032: No NaN/Inf in output
 */

#include "xpe/enhance_advanced/exposure_index.h"
#include "xpe/common/xpe_common_api.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>
#include <string>

namespace xpe {
namespace enhance_advanced {

/* ============================================================================
 * EITargetTable Implementation
 * ============================================================================ */

// @MX:ANCHOR: [AUTO] Body-part EI target lookup implementation
// @MX:REASON: Core lookup function, called by xpe_calc_exposure_index
// @MX:SPEC: REQ-ADV-013
float EITargetTable::getTarget(const char* bodyPartStr) {
    if (bodyPartStr == nullptr || strlen(bodyPartStr) == 0) {
        return DEFAULT;
    }

    // Convert to uppercase for case-insensitive comparison
    std::string bp(bodyPartStr);
    std::transform(bp.begin(), bp.end(), bp.begin(), ::toupper);

    // Match body part string to EI target
    if (bp.find("CHEST") != std::string::npos) {
        if (bp.find("LAT") != std::string::npos) {
            return CHEST_LAT;  // Chest Lateral
        }
        return CHEST;  // Chest PA (default)
    }
    if (bp.find("ABDOMEN") != std::string::npos) return ABDOMEN;
    if (bp.find("PELVIS") != std::string::npos) return PELVIS;
    if (bp.find("SKULL") != std::string::npos) return SKULL;
    if (bp.find("EXTREM") != std::string::npos) return EXTREMITY;
    if (bp.find("SPINE") != std::string::npos) return SPINE;

    // Default for unknown body parts
    return DEFAULT;
}

/* ============================================================================
 * ExposureIndexCalculator Implementation
 * ============================================================================ */

XpeErrorCode ExposureIndexCalculator::calculate(
    const XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    float* eiOut,
    float* diOut)
{
    // Validate inputs (REQ-ADV-022)
    XpeErrorCode validateResult = validateInputs(img, meta, eiOut, diOut);
    if (validateResult != XPE_OK) {
        return validateResult;
    }

    // Use default system constants
    Constants constants;

    // Estimate gain from acquisition parameters
    float gain = estimateGain(meta);

    // Calculate mean pixel value (full image for now, ROI in future)
    float meanPixel = calculateMean(img);

    // Guard against zero mean (edge case)
    if (meanPixel <= 0.0f) {
        meanPixel = 1e-6f;  // Small positive value to avoid log10(0)
    }

    // Calculate EI using IEC 62494-1 formula: EI = c1 * g * mean + c2
    float ei = calculateEI(meanPixel, gain, constants);

    // Guard against non-finite EI
    if (!std::isfinite(ei)) {
        ei = constants.c2;  // Fallback to offset value
    }

    // Get EI target for body part
    float eiTarget = EITargetTable::getTarget(meta->bodyPart);

    // Calculate DI: DI = 10 * log10(EI / EI_target)
    float di = calculateDI(ei, eiTarget);

    // Guard against non-finite DI
    if (!std::isfinite(di)) {
        di = 0.0f;  // Neutral DI if calculation fails
    }

    // Write outputs (REQ-ADV-032: No NaN/Inf)
    *eiOut = ei;
    *diOut = di;

    return XPE_OK;
}

XpeErrorCode ExposureIndexCalculator::validateInputs(
    const XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    const float* eiOut,
    const float* diOut)
{
    // NULL pointer checks (REQ-ADV-022, AC-EI-003)
    if (img == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (meta == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (eiOut == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (diOut == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Format validation (must be FLOAT32)
    if (img->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    // Dimension validation (REQ-ADV-070)
    if (img->width == 0 || img->height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    return XPE_OK;
}

float ExposureIndexCalculator::estimateGain(const XpeImageMetadata* meta) {
    // Estimate system gain from acquisition parameters
    // This is a simplified model - real systems may have calibration data
    //
    // Gain model: g = (kVp^2 * mAs) / reference
    // This approximates the X-ray fluence reaching the detector
    //
    // Reference: 80 kVp, 10 mAs → gain = 1.0 (normalized)

    if (meta == nullptr) {
        return 1.0f;  // Default gain
    }

    // Guard against invalid parameters
    if (!std::isfinite(meta->kVp) || !std::isfinite(meta->mAs)) {
        return 1.0f;
    }

    if (meta->kVp <= 0.0f || meta->mAs <= 0.0f) {
        return 1.0f;
    }

    // Simplified gain estimation model
    // In production, this would use detector-specific calibration
    float kvp = meta->kVp;
    float mas = meta->mAs;

    // Normalize to reference (80 kVp, 10 mAs)
    float referenceKvp = 80.0f;
    float referenceMas = 10.0f;

    // X-ray intensity ∝ kVp^2 * mAs (approximate)
    float gain = (kvp * kvp / (referenceKvp * referenceKvp)) * (mas / referenceMas);

    // Clamp to reasonable range [0.1, 10.0]
    gain = std::max(0.1f, std::min(10.0f, gain));

    return gain;
}

float ExposureIndexCalculator::calculateMean(const XpeImageBuffer* img) {
    if (img->data == nullptr || img->width == 0 || img->height == 0) {
        return 0.0f;
    }

    const float* data = static_cast<const float*>(img->data);
    size_t totalPixels = img->width * img->height;

    // Calculate mean with NaN/Inf filtering (REQ-ADV-032)
    double sum = 0.0;
    size_t validCount = 0;

    for (size_t i = 0; i < totalPixels; ++i) {
        float val = data[i];
        if (std::isfinite(val)) {
            sum += val;
            ++validCount;
        }
    }

    if (validCount == 0) {
        return 0.0f;  // No valid pixels
    }

    return static_cast<float>(sum / validCount);
}

float ExposureIndexCalculator::calculateEI(
    float meanPixel,
    float gain,
    const Constants& constants)
{
    // IEC 62494-1 formula: EI = c1 * g * mean + c2
    // Guard against invalid inputs
    if (!std::isfinite(meanPixel) || !std::isfinite(gain)) {
        return constants.c2;  // Fallback to offset
    }

    if (meanPixel <= 0.0f) {
        return constants.c2;
    }

    // Calculate EI
    float ei = constants.c1 * gain * meanPixel + constants.c2;

    // Ensure EI is positive (physical constraint)
    if (ei <= 0.0f) {
        ei = 1e-3f;  // Small positive EI
    }

    return ei;
}

float ExposureIndexCalculator::calculateDI(float ei, float eiTarget) {
    // IEC 62494-1 formula: DI = 10 * log10(EI / EI_target)

    // Guard against invalid inputs
    if (!std::isfinite(ei) || !std::isfinite(eiTarget)) {
        return 0.0f;  // Neutral DI
    }

    if (ei <= 0.0f || eiTarget <= 0.0f) {
        return 0.0f;  // Cannot compute log10 of non-positive
    }

    // Calculate ratio
    float ratio = ei / eiTarget;

    // Guard against overflow/underflow in log10
    if (ratio <= 0.0f || !std::isfinite(ratio)) {
        return 0.0f;
    }

    // Calculate DI
    float di = 10.0f * std::log10(ratio);

    // Ensure DI is finite
    if (!std::isfinite(di)) {
        return 0.0f;
    }

    return di;
}

} // namespace enhance_advanced
} // namespace xpe
