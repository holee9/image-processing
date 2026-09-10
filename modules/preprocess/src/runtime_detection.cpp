/**
 * @file runtime_detection.cpp
 * @brief Runtime defective pixel detection implementation.
 *
 * Implements REQ-P1A-013: Hampel 5-sigma outlier detection for transient defects.
 * TDD methodology: RED-GREEN-REFACTOR cycle.
 *
 * @MX:NOTE: [AUTO] Runtime defect detection -- SPEC-XPE-P1A-REQ-P1A-013
 *          TPR >= 99.9%, FPR < 0.001% with 5x5 sliding window
 */

#include "runtime_detection.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include <cstring>
#include <cstdlib>

// JSON parsing is minimal for this implementation
// Full nlohmann/json integration can be added if needed

/**
 * @brief Validate runtime detection configuration.
 *
 * @param config Configuration to validate
 * @return XPE_OK if valid, XPE_ERR_INVALID_INPUT otherwise
 */
static XpeErrorCode ValidateConfig(const RuntimeDetectionConfig& config) {
    // Window size must be positive and odd
    if (config.windowSize <= 0) return XPE_ERR_INVALID_INPUT;
    if (config.windowSize % 2 == 0) return XPE_ERR_INVALID_INPUT;  // Must be odd

    // Sigma threshold must be positive
    if (config.sigmaThreshold <= 0.0f) return XPE_ERR_INVALID_INPUT;

    return XPE_OK;
}

/* =========================================================================
 * Public API Implementation
 * ========================================================================= */

extern "C" {

/**
 * @brief Detect transient defect pixels via Hampel 5-sigma outlier detection.
 *
 * @MX:ANCHOR: [AUTO] xpe_defect_detect_runtime -- REQ-P1A-013
 * @MX:REASON: Public API entry point; called by defect correction pipeline
 *
 * @param img Input image (float32 format required)
 * @param defectMapOut Output defect map (non-zero = defective, must match img dimensions)
 * @param defectMapOut Output defect map (non-zero = defective)
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on parameter errors
 *
 * @note Window size and sigma threshold are fixed at their defaults; this entry
 *       point takes no configuration (QA-A-34, #120).
 *
 * @note Algorithm: Hampel identifier with sliding window
 *       1. Collect values in window (default 5x5)
 *       2. Compute median of window
 *       3. Compute MAD (Median Absolute Deviation)
 *       4. Flag if: |value - median| > 5 * (1.4826 * MAD)
 *
 * @note TPR >= 99.9% for synthetic defects (outliers > 5-sigma)
 * @note FPR < 0.001% for clean Gaussian noise images
 */
XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* img,
                                                const XpeImageMetadata* metadata,
                                                XpeImageBuffer* defectMapOut) {
    (void)metadata;
    // Validate input parameters
    if (img == nullptr) return XPE_ERR_INVALID_INPUT;
    if (defectMapOut == nullptr) return XPE_ERR_INVALID_INPUT;
    if (img->data == nullptr) return XPE_ERR_INVALID_INPUT;
    if (defectMapOut->data == nullptr) return XPE_ERR_INVALID_INPUT;

    // Validate dimensions match
    if (img->width != defectMapOut->width) return XPE_ERR_INVALID_INPUT;
    if (img->height != defectMapOut->height) return XPE_ERR_INVALID_INPUT;

    // Validate format (float32 input, uint8 output)
    if (img->format != XPE_PIXEL_FLOAT32) return XPE_ERR_INVALID_INPUT;
    if (defectMapOut->format != XPE_PIXEL_UINT8) return XPE_ERR_INVALID_INPUT;

    size_t pixelCount = 0;
    if (!xpe_pixel_count(img, &pixelCount)) return XPE_ERR_INVALID_INPUT;

    size_t outPixelCount = 0;
    if (!xpe_pixel_count(defectMapOut, &outPixelCount)) return XPE_ERR_INVALID_INPUT;
    if (outPixelCount != pixelCount) return XPE_ERR_INVALID_INPUT;

    size_t inputBytes = 0;
    if (!xpe_required_bytes(img, sizeof(float), &inputBytes)) return XPE_ERR_INVALID_INPUT;
    if (img->dataSize < inputBytes) return XPE_ERR_INVALID_INPUT;

    size_t outputBytes = 0;
    if (!xpe_required_bytes(defectMapOut, sizeof(uint8_t), &outputBytes)) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (defectMapOut->dataSize < outputBytes) return XPE_ERR_BUFFER_TOO_SMALL;

    // Detection parameters.
    //
    // QA-A-34 (#120): this used to read
    //     config.windowSize     = ParseWindowSize(nullptr, config.windowSize);
    //     config.sigmaThreshold = ParseSigmaThreshold(nullptr, config.sigmaThreshold);
    // Both parsers were handed a literal nullptr, so they returned the default
    // they were given and their JSON-scanning bodies never executed -- dead code
    // that made the parameters look configurable. This entry point has no config
    // argument (api-spec 6.x), and no SPEC requires window size or sigma
    // threshold to be set from outside, so the parsers were removed rather than
    // wired up. Callers that need other values use the internal
    // DetectDefectivePixel(img, x, y, config) directly.
    const RuntimeDetectionConfig config = RuntimeDetection_DefaultConfig();

    // Validate configuration
    XpeErrorCode err = ValidateConfig(config);
    if (err != XPE_OK) return err;

    // Clear output defect map
    std::memset(defectMapOut->data, 0, outputBytes);

    // Detect defective pixels
    uint8_t* defectMap = static_cast<uint8_t*>(defectMapOut->data);

    for (uint32_t y = 0; y < img->height; ++y) {
        for (uint32_t x = 0; x < img->width; ++x) {
            bool isDefective = xpe::preprocess::internal::DetectDefectivePixel(
                img, x, y, config);

            if (isDefective) {
                defectMap[y * img->width + x] = 1;  // Mark as defective
            }
        }
    }

    return XPE_OK;
}

} // extern "C"
