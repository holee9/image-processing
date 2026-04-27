/**
 * @file xpe_defect_gen.cpp
 * @brief BPM (Bad Pixel Map) Generation API (SWU-1.11)
 *
 * SPEC: SAD-CALIB-001 SWU-1.11 (FUNC-022~025)
 * IEC 62304 Class B
 *
 * Algorithms:
 * - FUNC-022: Dark BPM generation using RMM (Robust Mask Maker)
 * - FUNC-023: Bright BPM generation using local mean deviation
 * - FUNC-024: BPM merging (dark U bright)
 * - FUNC-025: Reflect padding for boundary handling
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>

/* =============================================================================
 * Internal Constants and Defaults
 * ============================================================================ */

namespace {

// Default configuration values
constexpr float    kDefaultLambdaDark      = 8.0f;
constexpr uint32_t kDefaultMaskSizeDark    = 32;
constexpr float    kDefaultTolerancePct    = 0.07f;
constexpr uint32_t kDefaultMaskSizeBright  = 128;
constexpr uint32_t kDefaultMinFramesDark   = 5;
constexpr uint32_t kDefaultMinFramesBright = 10;

// Configuration validation limits
constexpr float    kMinTolerancePct        = 0.05f;
constexpr float    kMaxTolerancePct        = 0.09f;
constexpr uint32_t kMinMaskSizeDark        = 32;
constexpr uint32_t kMinMaskSizeBright      = 128;

// MAD-to-sigma conversion constant (1 / (sqrt(2) * erfc_inv(3/2)))
constexpr float kMadToSigma = 1.4826f;

// BPM pixel value definitions
constexpr uint8_t BPM_PIXEL_GOOD      = 0;  // Normal pixel
constexpr uint8_t BPM_PIXEL_DEAD      = 1;  // Dead/stuck pixel (dark BPM)
constexpr uint8_t BPM_PIXEL_HOT       = 2;  // Hot/noisy pixel (bright BPM)
constexpr uint8_t BPM_PIXEL_BOTH      = 3;  // Both dark and bright defects

/* =============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Validate XpeBpmConfig parameters
 *
 * @param cfg Configuration to validate (NULL = use defaults)
 * @param configOut Output: Validated configuration with defaults filled
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT on invalid parameters
 */
XpeErrorCode validate_bpm_config(const XpeBpmConfig* cfg, XpeBpmConfig* configOut) noexcept {
    if (!configOut) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Start with defaults
    configOut->lambda_dark       = kDefaultLambdaDark;
    configOut->mask_size_dark    = kDefaultMaskSizeDark;
    configOut->tolerance_pct     = kDefaultTolerancePct;
    configOut->mask_size_bright  = kDefaultMaskSizeBright;
    configOut->min_frames_dark   = kDefaultMinFramesDark;
    configOut->min_frames_bright = kDefaultMinFramesBright;

    if (!cfg) {
        return XPE_OK;  // Use defaults
    }

    // Validate and apply user-provided values
    if (cfg->lambda_dark <= 0.0f) {
        return XPE_ERR_INVALID_INPUT;
    }
    configOut->lambda_dark = cfg->lambda_dark;

    if (cfg->mask_size_dark < kMinMaskSizeDark) {
        return XPE_ERR_INVALID_INPUT;
    }
    configOut->mask_size_dark = cfg->mask_size_dark;

    if (cfg->tolerance_pct < kMinTolerancePct || cfg->tolerance_pct > kMaxTolerancePct) {
        return XPE_ERR_INVALID_INPUT;
    }
    configOut->tolerance_pct = cfg->tolerance_pct;

    if (cfg->mask_size_bright < kMinMaskSizeBright) {
        return XPE_ERR_INVALID_INPUT;
    }
    configOut->mask_size_bright = cfg->mask_size_bright;

    if (cfg->min_frames_dark == 0 || cfg->min_frames_bright == 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    configOut->min_frames_dark   = cfg->min_frames_dark;
    configOut->min_frames_bright = cfg->min_frames_bright;

    return XPE_OK;
}

/**
 * @brief Compute pixel-wise mean across multiple frames
 *
 * @param frames Array of image frames
 * @param num_frames Number of frames
 * @param width Image width
 * @param height Image height
 * @param mean_out Output: Mean image (float32)
 * @return XPE_OK on success
 */
XpeErrorCode compute_frame_mean(const XpeImageBuffer* frames,
                                uint32_t num_frames,
                                uint32_t width,
                                uint32_t height,
                                std::vector<float>& mean_out) noexcept {
    const size_t num_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    mean_out.resize(num_pixels, 0.0f);

    // Accumulate sum across all frames
    for (uint32_t f = 0; f < num_frames; ++f) {
        const XpeImageBuffer& frame = frames[f];

        if (!xpe_buffer_has_format(&frame, XPE_PIXEL_UINT16)) {
            return XPE_ERR_INVALID_INPUT;
        }

        const uint16_t* data = reinterpret_cast<const uint16_t*>(frame.data);
        for (size_t i = 0; i < num_pixels; ++i) {
            mean_out[i] += static_cast<float>(data[i]);
        }
    }

    // Divide by number of frames
    const float inv_frames = 1.0f / static_cast<float>(num_frames);
    for (size_t i = 0; i < num_pixels; ++i) {
        mean_out[i] *= inv_frames;
    }

    return XPE_OK;
}

/**
 * @brief Extract local window with reflect padding
 *
 * FUNC-025: Reflect padding at boundaries
 *
 * @param image Source image (float32)
 * @param width Image width
 * @param height Image height
 * @param cx Center pixel X
 * @param cy Center pixel Y
 * @param mask_size Window size (must be odd)
 * @param window_out Output: Window values (mask_size * mask_size)
 */
void extract_window_reflect(const float* image,
                            uint32_t width, uint32_t height,
                            uint32_t cx, uint32_t cy,
                            uint32_t mask_size,
                            std::vector<float>& window_out) noexcept {
    const int half_size = static_cast<int>(mask_size / 2);
    const int actual_size = 2 * half_size + 1;
    window_out.resize(static_cast<size_t>(actual_size) * static_cast<size_t>(actual_size));

    size_t win_idx = 0;
    for (int dy = -half_size; dy <= half_size; ++dy) {
        // Reflect Y coordinate
        int ry = static_cast<int>(cy) + dy;
        if (ry < 0) ry = -ry;                          // Mirror at top
        else if (ry >= static_cast<int>(height)) {
            ry = 2 * static_cast<int>(height) - ry - 1;  // Mirror at bottom
        }

        for (int dx = -half_size; dx <= half_size; ++dx) {
            // Reflect X coordinate
            int rx = static_cast<int>(cx) + dx;
            if (rx < 0) rx = -rx;                          // Mirror at left
            else if (rx >= static_cast<int>(width)) {
                rx = 2 * static_cast<int>(width) - rx - 1;  // Mirror at right
            }

            const size_t img_idx = static_cast<size_t>(ry) * static_cast<size_t>(width) +
                                   static_cast<size_t>(rx);
            window_out[win_idx++] = image[img_idx];
        }
    }
}

/**
 * @brief Compute median of values (using nth_element for O(n) performance)
 *
 * @param values Vector of values (modified during computation)
 * @return Median value
 */
float compute_median(std::vector<float>& values) noexcept {
    if (values.empty()) return 0.0f;

    const size_t n = values.size();
    const size_t mid = n / 2;

    if (n % 2 == 0) {
        // Even number: average of two middle values
        std::nth_element(values.begin(), values.begin() + mid - 1, values.end());
        const float v1 = values[mid - 1];
        std::nth_element(values.begin(), values.begin() + mid, values.end());
        const float v2 = values[mid];
        return (v1 + v2) * 0.5f;
    } else {
        // Odd number: middle value
        std::nth_element(values.begin(), values.begin() + mid, values.end());
        return values[mid];
    }
}

/**
 * @brief Generate dark BPM using RMM (Robust Mask Maker)
 *
 * FUNC-022: Dark BPM generation with adaptive local statistics
 *
 * Algorithm:
 * 1. Compute dark_mean (average across dark frames)
 * 2. For each pixel:
 *    a. Extract mask_size_dark × mask_size_dark window with reflect padding
 *    b. Compute local median M
 *    c. Compute MAD σ_r = 1.4826 × median(|w_i - M|)
 *    d. Flag if |dark_mean[x,y] - M| > λ × σ_r
 *
 * @param dark_mean Mean dark image (float32)
 * @param width Image width
 * @param height Image height
 * @param config BPM configuration
 * @param dark_bpm_out Output: Dark BPM (uint8, 0=good, 1=bad)
 * @return XPE_OK on success
 */
XpeErrorCode generate_dark_bpm(const float* dark_mean,
                               uint32_t width, uint32_t height,
                               const XpeBpmConfig& config,
                               std::vector<uint8_t>& dark_bpm_out) noexcept {
    const size_t num_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    dark_bpm_out.resize(num_pixels, BPM_PIXEL_GOOD);

    const uint32_t mask_size = config.mask_size_dark;
    const float lambda = config.lambda_dark;

    // Temporary buffer for window values
    std::vector<float> window;
    std::vector<float> abs_deviations;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t pixel_idx = static_cast<size_t>(y) * static_cast<size_t>(width) +
                                     static_cast<size_t>(x);

            // Extract window with reflect padding
            extract_window_reflect(dark_mean, width, height, x, y, mask_size, window);

            // Compute local median
            std::vector<float> window_copy = window;  // Copy for median computation
            const float M = compute_median(window_copy);

            // Compute MAD (Median Absolute Deviation)
            abs_deviations.resize(window.size());
            for (size_t i = 0; i < window.size(); ++i) {
                abs_deviations[i] = std::abs(window[i] - M);
            }
            const float sigma_r = kMadToSigma * compute_median(abs_deviations);

            // Flag pixel if deviation exceeds threshold
            const float deviation = std::abs(dark_mean[pixel_idx] - M);
            if (deviation > lambda * sigma_r) {
                dark_bpm_out[pixel_idx] = BPM_PIXEL_DEAD;
            }
        }
    }

    return XPE_OK;
}

/**
 * @brief Generate bright BPM using local mean deviation
 *
 * FUNC-023: Bright BPM generation for hot/noisy pixels
 *
 * Algorithm:
 * 1. Compute bright_mean (average across bright frames)
 * 2. For each pixel:
 *    a. Extract mask_size_bright × mask_size_bright window from bright_mean
 *    b. Compute maskAvg = mean(window)
 *    c. Compute Bright_Tol = maskAvg × tolerance_pct
 *    d. Flag if |bright_mean[x,y] - maskAvg| > Bright_Tol
 *
 * @param bright_mean Mean bright image (float32)
 * @param width Image width
 * @param height Image height
 * @param config BPM configuration
 * @param bright_bpm_out Output: Bright BPM (uint8, 0=good, 2=bad)
 * @return XPE_OK on success
 */
XpeErrorCode generate_bright_bpm(const float* bright_mean,
                                 uint32_t width, uint32_t height,
                                 const XpeBpmConfig& config,
                                 std::vector<uint8_t>& bright_bpm_out) noexcept {
    const size_t num_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    bright_bpm_out.resize(num_pixels, BPM_PIXEL_GOOD);

    const uint32_t mask_size = config.mask_size_bright;
    const float tolerance_pct = config.tolerance_pct;

    // Temporary buffer for window values
    std::vector<float> window;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t pixel_idx = static_cast<size_t>(y) * static_cast<size_t>(width) +
                                     static_cast<size_t>(x);

            // Extract window with reflect padding
            extract_window_reflect(bright_mean, width, height, x, y, mask_size, window);

            // Compute local mean
            float maskAvg = 0.0f;
            for (const float val : window) {
                maskAvg += val;
            }
            maskAvg /= static_cast<float>(window.size());

            // Compute tolerance and flag pixel
            const float bright_tol = maskAvg * tolerance_pct;
            const float deviation = std::abs(bright_mean[pixel_idx] - maskAvg);

            if (deviation > bright_tol) {
                bright_bpm_out[pixel_idx] = BPM_PIXEL_HOT;
            }
        }
    }

    return XPE_OK;
}

/**
 * @brief Merge dark and bright BPM using logical OR
 *
 * FUNC-024: BPM merging (dark U bright)
 *
 * @param dark_bpm Dark BPM (0=good, 1=dead)
 * @param bright_bpm Bright BPM (0=good, 2=hot)
 * @param bpm_out Output: Merged BPM (0=good, 1=dead, 2=hot, 3=both)
 */
void merge_bpm(const std::vector<uint8_t>& dark_bpm,
               const std::vector<uint8_t>& bright_bpm,
               std::vector<uint8_t>& bpm_out) noexcept {
    const size_t num_pixels = dark_bpm.size();
    bpm_out.resize(num_pixels);

    for (size_t i = 0; i < num_pixels; ++i) {
        // Merge using logical OR: 0, 1, 2, or 3 (both)
        bpm_out[i] = dark_bpm[i] | bright_bpm[i];
    }
}

} // anonymous namespace

/* =============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Generate BPM (Bad Pixel Map) from dark and bright frames
 *
 * SWU-1.11: BPM Generation API (FUNC-022~025)
 *
 * @param dark_frames Array of dark frames (UINT16)
 * @param num_dark Number of dark frames (≥ min_frames_dark)
 * @param bright_frames Array of bright/flat-field frames (UINT16)
 * @param num_bright Number of bright frames (≥ min_frames_bright)
 * @param cfg Algorithm configuration (NULL = defaults)
 * @param bpm_out Output BPM (UINT8, 0=good, 1=dead/stuck, 2=hot/noisy, 3=both)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or invalid parameters
 *         XPE_ERR_BUFFER_TOO_SMALL if output buffer too small
 */
extern "C" XPE_API XpeErrorCode xpe_bpm_generate(
    const XpeImageBuffer* dark_frames,
    uint32_t num_dark,
    const XpeImageBuffer* bright_frames,
    uint32_t num_bright,
    const XpeBpmConfig* cfg,
    XpeImageBuffer* bpm_out)
{
    // Validate inputs
    if (!dark_frames || num_dark == 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (!bright_frames || num_bright == 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (!bpm_out || !bpm_out->data) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Validate and apply configuration
    XpeBpmConfig config;
    XpeErrorCode err = validate_bpm_config(cfg, &config);
    if (err != XPE_OK) {
        return err;
    }

    // Check minimum frame requirements
    if (num_dark < config.min_frames_dark) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (num_bright < config.min_frames_bright) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Validate all frames have matching dimensions
    const uint32_t width = dark_frames[0].width;
    const uint32_t height = dark_frames[0].height;

    for (uint32_t i = 0; i < num_dark; ++i) {
        if (!xpe_dims_match(&dark_frames[0], &dark_frames[i])) {
            return XPE_ERR_INVALID_INPUT;
        }
    }
    for (uint32_t i = 0; i < num_bright; ++i) {
        if (!xpe_dims_match(&bright_frames[0], &bright_frames[i])) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (bright_frames[i].width != width || bright_frames[i].height != height) {
            return XPE_ERR_INVALID_INPUT;
        }
    }

    // Validate output buffer format and size
    if (bpm_out->format != XPE_PIXEL_UINT8) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }
    if (bpm_out->width != width || bpm_out->height != height) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    const size_t num_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (bpm_out->dataSize < num_pixels) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    // Compute mean images
    std::vector<float> dark_mean;
    err = compute_frame_mean(dark_frames, num_dark, width, height, dark_mean);
    if (err != XPE_OK) {
        return err;
    }

    std::vector<float> bright_mean;
    err = compute_frame_mean(bright_frames, num_bright, width, height, bright_mean);
    if (err != XPE_OK) {
        return err;
    }

    // Generate dark BPM (FUNC-022)
    std::vector<uint8_t> dark_bpm;
    err = generate_dark_bpm(dark_mean.data(), width, height, config, dark_bpm);
    if (err != XPE_OK) {
        return err;
    }

    // Generate bright BPM (FUNC-023)
    std::vector<uint8_t> bright_bpm;
    err = generate_bright_bpm(bright_mean.data(), width, height, config, bright_bpm);
    if (err != XPE_OK) {
        return err;
    }

    // Merge BPMs (FUNC-024)
    std::vector<uint8_t> merged_bpm;
    merge_bpm(dark_bpm, bright_bpm, merged_bpm);

    // Copy to output buffer
    std::memcpy(bpm_out->data, merged_bpm.data(), num_pixels);

    return XPE_OK;
}
