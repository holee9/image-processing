/**
 * @file fractional_derivative.h
 * @brief Fractional-order differentiation for edge enhancement
 *
 * Implements the Gruenwald-Letnikov fractional derivative operator
 * for edge enhancement with mandatory overshoot limiting (SAF-100).
 *
 * REQ-ADV-011: Fractional-order process execution
 * REQ-ADV-032: No NaN/Inf in output
 * REQ-ADV-051: Mandatory overshoot limiting (SAF-100)
 */

#ifndef XPE_ENHANCE_ADVANCED_FRACTIONAL_DERIVATIVE_H
#define XPE_ENHANCE_ADVANCED_FRACTIONAL_DERIVATIVE_H

#include "xpe/common/xpe_types.h"
#include <vector>
#include <cstddef>

namespace xpe {
namespace enhance_advanced {

/**
 * @brief Configuration for fractional-order enhancement
 *
 * Contains parameters for fractional derivative computation.
 * Note: Overshoot limiting is always enabled (SAF-100) and
 * cannot be disabled via configuration.
 */
struct FractionalConfig {
    float order;  ///< Fractional derivative order [0.0, 2.0]

    /**
     * @brief Create default config
     * @return Config with order = 1.0 (first derivative)
     */
    static FractionalConfig defaultConfig() {
        return FractionalConfig{1.0f};
    }

    /**
     * @brief Parse from JSON string
     * @param configJsonOrNull JSON configuration or nullptr
     * @return Parsed configuration
     *
     * Rejects attempts to disable overshoot limiting (SAF-100).
     */
    static FractionalConfig fromJson(const char* configJsonOrNull);
};

/**
 * @brief Apply fractional-order differentiation with overshoot limiting
 *
 * Implements Gruenwald-Letnikov fractional derivative operator:
 * D^alpha f(x) = sum_{k=0}^{n} (-1)^k * C(alpha, k) * f(x - k*h)
 *
 * where C(alpha, k) = gamma(alpha + 1) / (gamma(k + 1) * gamma(alpha - k + 1))
 *
 * SAF-100: Enhancement boost is clipped to +-3*sigma_local for every pixel.
 * This is mandatory and non-configurable.
 *
 * @param img Input/output image buffer (FLOAT32 format)
 * @param config Configuration including fractional order
 * @return XPE_OK on success, error code on failure
 *
 * REQ-ADV-011: Fractional-order process execution
 * REQ-ADV-021: Invalid order parameter guard
 * REQ-ADV-032: No NaN/Inf in output
 * REQ-ADV-051: Overshoot limiting enforcement
 */
XpeErrorCode applyFractionalDerivative(XpeImageBuffer* img, const FractionalConfig& config);

/**
 * @brief Compute fractional derivative mask coefficients
 *
 * Generates Gruenwald-Letnikov coefficients for a given order and mask size.
 * Coefficients follow the binomial pattern for fractional derivatives.
 *
 * @param order Fractional derivative order [0.0, 2.0]
 * @param maskSize Number of coefficients (should be odd for symmetric mask)
 * @return Vector of mask coefficients
 *
 * @pre order in [0.0, 2.0]
 * @pre maskSize > 0
 */
std::vector<float> computeFractionalMask(float order, size_t maskSize);

/**
 * @brief Apply overshoot limiting to enhanced image
 *
 * Clips enhancement boost to +-3*sigma_local for each pixel.
 * This is a mandatory safety feature (SAF-100).
 *
 * @param original Original image before enhancement
 * @param enhanced Enhanced image (will be modified in-place)
 * @param width Image width
 * @param height Image height
 *
 * REQ-ADV-051: Overshoot limiting enforcement
 * SAF-100: Mandatory safeguard
 */
void applyOvershootLimiting(const float* original, float* enhanced,
                            int width, int height);

/**
 * @brief Calculate local standard deviation (3x3 neighborhood)
 *
 * Computes sigma_local for overshoot limiting.
 *
 * @param data Image data
 * @param width Image width
 * @param height Image height
 * @param x Center pixel X coordinate
 * @param y Center pixel Y coordinate
 * @return Local standard deviation
 */
float calculateLocalStdDev(const float* data, int width, int height, int x, int y);

} // namespace enhance_advanced
} // namespace xpe

#endif // XPE_ENHANCE_ADVANCED_FRACTIONAL_DERIVATIVE_H
