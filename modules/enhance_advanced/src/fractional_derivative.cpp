/**
 * @file fractional_derivative.cpp
 * @brief Fractional-order differentiation implementation
 *
 * Implements Gruenwald-Letnikov fractional derivative with mandatory
 * overshoot limiting (SAF-100).
 *
 * REQ-ADV-011: Fractional-order process execution
 * REQ-ADV-032: No NaN/Inf in output
 * REQ-ADV-051: Mandatory overshoot limiting (SAF-100)
 */

#include "detail/fractional_derivative.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_common_api.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <limits>
#include <mutex>

// @MX:ANCHOR: [AUTO] Fractional binomial coefficient computation
// @MX:REASON: Core mathematical function used in mask generation and image convolution
// @MX:SPEC: REQ-ADV-011, REQ-ADV-032

namespace xpe {
namespace enhance_advanced {

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

namespace {
    /**
     * @brief Compute binomial coefficient for fractional order
     *
     * C(alpha, k) = gamma(alpha + 1) / (gamma(k + 1) * gamma(alpha - k + 1))
     *
     * For computational efficiency, we use the recursive formula:
     * C(alpha, 0) = 1
     * C(alpha, k) = C(alpha, k-1) * (alpha - k + 1) / k
     *
     * @param alpha Fractional order
     * @param k Coefficient index
     * @return Binomial coefficient
     */
    float fractionalBinomial(float alpha, int k) {
        if (k == 0) {
            return 1.0f;
        }

        float result = 1.0f;
        for (int i = 1; i <= k; ++i) {
            result *= (alpha - static_cast<float>(i) + 1.0f) / static_cast<float>(i);
        }

        return result;
    }

    /**
     * @brief Check if value is valid (not NaN or Inf)
     * @param value Value to check
     * @return true if value is valid
     */
    bool isValid(float value) {
        return !std::isnan(value) && !std::isinf(value);
    }

    /**
     * @brief Clamp value to valid range
     * @param value Value to clamp
     * @param min Minimum value
     * @param max Maximum value
     * @return Clamped value
     */
    float clamp(float value, float min, float max) {
        return std::max(min, std::min(max, value));
    }

    /**
     * @brief Safe float addition with overflow check
     * @param a First operand
     * @param b Second operand
     * @return a + b, or +/-inf if overflow would occur
     */
    float safeAdd(float a, float b) {
        if (a > 0 && b > std::numeric_limits<float>::max() - a) {
            return std::numeric_limits<float>::infinity();
        }
        if (a < 0 && b < -std::numeric_limits<float>::max() - a) {
            return -std::numeric_limits<float>::infinity();
        }
        return a + b;
    }
} // anonymous namespace

/* ============================================================================
 * FractionalConfig Implementation
 * ============================================================================ */

FractionalConfig FractionalConfig::fromJson(const char* configJsonOrNull) {
    FractionalConfig config = defaultConfig();

    if (configJsonOrNull == nullptr) {
        return config;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(configJsonOrNull);

        // Parse order parameter
        if (j.contains("order")) {
            config.order = j["order"];
        }

        // SAF-100: Reject attempts to disable overshoot limiting
        const std::vector<const char*> forbiddenKeys = {
            "overshoot_limiting",
            "overshoot_limit",
            "overshoot_factor",
            "disable_overshoot_limit"
        };

        for (const char* key : forbiddenKeys) {
            if (j.contains(key)) {
                // Attempting to configure overshoot limiting is a safety violation
                throw std::runtime_error("SAF-100: Overshoot limiting cannot be configured");
            }

            // Check nested in "safety" object
            if (j.contains("safety") && j["safety"].is_object()) {
                if (j["safety"].contains(key)) {
                    throw std::runtime_error("SAF-100: Overshoot limiting cannot be configured");
                }
            }
        }

    } catch (const nlohmann::json::exception&) {
        // JSON parse error - return defaults
        // In production, might want to log this
    } catch (const std::runtime_error&) {
        // SAF-100 violation - will be caught by caller and converted to error code
        throw;
    }

    return config;
}

/* ============================================================================
 * Mask Generation
 * ============================================================================ */

std::vector<float> computeFractionalMask(float order, size_t maskSize) {
    std::vector<float> mask(maskSize);

    // Generate Gruenwald-Letnikov coefficients
    // For alpha in [0, 2], we use a symmetric mask around center
    // Mask coefficients: C(alpha, 0), -C(alpha, 1), C(alpha, 2), ...

    for (size_t k = 0; k < maskSize; ++k) {
        float coeff = fractionalBinomial(order, static_cast<int>(k));

        // Alternate signs for derivative
        if (k % 2 == 1) {
            coeff = -coeff;
        }

        mask[k] = coeff;
    }

    return mask;
}

/* ============================================================================
 * Local Statistics
 * ============================================================================ */

float calculateLocalStdDev(const float* data, int width, int height, int x, int y) {
    float sum = 0.0f;
    float sumSq = 0.0f;
    int count = 0;

    // 3x3 neighborhood
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx;
            int ny = y + dy;

            // Boundary check: clamp to image bounds (int overload)
            nx = std::clamp(nx, 0, width - 1);
            ny = std::clamp(ny, 0, height - 1);

            float val = data[ny * static_cast<size_t>(width) + nx];

            // Skip invalid values
            if (!isValid(val)) {
                continue;
            }

            sum = safeAdd(sum, val);
            sumSq = safeAdd(sumSq, val * val);
            ++count;
        }
    }

    if (count == 0) {
        return 0.0f;  // No valid neighbors
    }

    float mean = sum / static_cast<float>(count);
    float variance = (sumSq / static_cast<float>(count)) - (mean * mean);

    // Guard against negative variance due to numerical errors
    variance = std::max(0.0f, variance);

    return std::sqrt(variance);
}

/* ============================================================================
 * Overshoot Limiting (SAF-100 - MANDATORY)
 * ============================================================================ */

void applyOvershootLimiting(const float* original, float* enhanced,
                            int width, int height) {
    const float overshootFactor = 3.0f;  // SAF-100: Fixed at 3*sigma

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = y * static_cast<size_t>(width) + x;

            float baseValue = original[idx];
            float enhancedValue = enhanced[idx];
            float boost = enhancedValue - baseValue;

            // Calculate local standard deviation
            float sigmaLocal = calculateLocalStdDev(original, width, height, x, y);

            // SAF-100: Clip enhancement boost to +-3*sigma_local
            float limit = overshootFactor * sigmaLocal;

            // Handle zero sigma (uniform region)
            if (sigmaLocal < 1e-6f) {
                // In uniform regions, allow minimal enhancement
                limit = 0.1f;  // Small fixed limit
            }

            // Apply clipping
            if (boost > limit) {
                enhanced[idx] = baseValue + limit;
            } else if (boost < -limit) {
                enhanced[idx] = baseValue - limit;
            }

            // Final sanity check: ensure output is valid
            if (!isValid(enhanced[idx])) {
                enhanced[idx] = baseValue;  // Fallback to original
            }
        }
    }
}

/* ============================================================================
 * Fractional Derivative Application
 * ============================================================================ */

XpeErrorCode applyFractionalDerivative(XpeImageBuffer* img, const FractionalConfig& config) {
    if (img == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    if (img->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (img->width == 0 || img->height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Order validation [0.0, 2.0]
    if (config.order < 0.0f || config.order > 2.0f) {
        return XPE_ERR_INVALID_INPUT;
    }

    int width = static_cast<int>(img->width);
    int height = static_cast<int>(img->height);
    float* data = static_cast<float*>(img->data);

    // Store original for overshoot limiting
    std::vector<float> original(data, data + width * height);

    // For order = 0, no enhancement (identity)
    if (config.order < 1e-6f) {
        return XPE_OK;
    }

    // Compute fractional derivative mask
    // Mask size: 5 for order <= 1.0, 7 for order > 1.0
    size_t maskSize = (config.order <= 1.0f) ? 5 : 7;
    std::vector<float> mask = computeFractionalMask(config.order, maskSize);

    // Apply fractional derivative convolution
    // For simplicity, we apply 1D convolution horizontally and vertically
    // Full 2D implementation would use separable filters

    std::vector<float> temp(width * height);

    // Horizontal pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;

            for (size_t k = 0; k < maskSize; ++k) {
                int nx = x - static_cast<int>(k) + static_cast<int>(maskSize) / 2;

                // Boundary handling: clamp (int overload)
                nx = std::clamp(nx, 0, width - 1);

                size_t srcIdx = y * static_cast<size_t>(width) + nx;
                float val = original[srcIdx];

                if (!isValid(val)) {
                    val = 0.0f;  // Treat invalid as zero
                }

                sum += val * mask[k];
            }

            temp[y * static_cast<size_t>(width) + x] = sum;
        }
    }

    // Vertical pass
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;

            for (size_t k = 0; k < maskSize; ++k) {
                int ny = y - static_cast<int>(k) + static_cast<int>(maskSize) / 2;

                // Boundary handling: clamp (int overload)
                ny = std::clamp(ny, 0, height - 1);

                size_t srcIdx = ny * static_cast<size_t>(width) + x;
                float val = temp[srcIdx];

                if (!isValid(val)) {
                    val = 0.0f;
                }

                sum += val * mask[k];
            }

            data[y * static_cast<size_t>(width) + x] = sum;
        }
    }

    // SAF-100: Apply overshoot limiting (MANDATORY)
    applyOvershootLimiting(original.data(), data, width, height);

    return XPE_OK;
}

} // namespace enhance_advanced
} // namespace xpe
