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
#include <cstring>
#include <cstdlib>

// JSON parsing is minimal for this implementation
// Full nlohmann/json integration can be added if needed

/**
 * @brief Parse window size from JSON config string.
 *
 * @param configJson JSON configuration string (can be nullptr)
 * @param defaultValue Default value if parsing fails
 * @return Parsed window size or default
 */
static int32_t ParseWindowSize(const char* configJson, int32_t defaultValue) {
    if (configJson == nullptr) return defaultValue;

    // Minimal JSON parsing for windowSize
    const char* windowKey = "\"windowSize\"";
    const char* pos = strstr(configJson, windowKey);
    if (pos == nullptr) return defaultValue;

    pos = strchr(pos, ':');
    if (pos == nullptr) return defaultValue;

    // Skip whitespace
    ++pos;
    while (*pos == ' ' || *pos == '\t') ++pos;

    // Parse integer value
    return static_cast<int32_t>(atoi(pos));
}

/**
 * @brief Parse sigma threshold from JSON config string.
 *
 * @param configJson JSON configuration string (can be nullptr)
 * @param defaultValue Default value if parsing fails
 * @return Parsed sigma threshold or default
 */
static float ParseSigmaThreshold(const char* configJson, float defaultValue) {
    if (configJson == nullptr) return defaultValue;

    // Minimal JSON parsing for sigmaThreshold
    const char* sigmaKey = "\"sigmaThreshold\"";
    const char* pos = strstr(configJson, sigmaKey);
    if (pos == nullptr) return defaultValue;

    pos = strchr(pos, ':');
    if (pos == nullptr) return defaultValue;

    // Skip whitespace
    ++pos;
    while (*pos == ' ' || *pos == '\t') ++pos;

    // Parse float value
    #ifdef _WIN32
        return static_cast<float>(atof(pos));
    #else
        return strtof(pos, nullptr);
    #endif
}

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
 * @param configJsonOrNull Optional JSON config with windowSize and sigmaThreshold
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on parameter errors
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
                                                XpeImageBuffer* defectMapOut,
                                                const char* configJsonOrNull) {
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

    // Parse configuration
    RuntimeDetectionConfig config = RuntimeDetection_DefaultConfig();
    config.windowSize = ParseWindowSize(configJsonOrNull, config.windowSize);
    config.sigmaThreshold = ParseSigmaThreshold(configJsonOrNull, config.sigmaThreshold);

    // Validate configuration
    XpeErrorCode err = ValidateConfig(config);
    if (err != XPE_OK) return err;

    // Clear output defect map
    std::memset(defectMapOut->data, 0, defectMapOut->dataSize);

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
