/**
 * @file edge_detection.h
 * @brief Edge detection using Sobel gradients for collimation detection
 *
 * Provides gradient computation for edge map generation.
 * Part of SWU-2.8 Collimation ROI Detection (SPEC-XPE-P2-ADV).
 */

#ifndef XPE_ENHANCE_ADVANCED_EDGE_DETECTION_H
#define XPE_ENHANCE_ADVANCED_EDGE_DETECTION_H

#include <Eigen/Dense>
#include <cstdint>

namespace xpe {
namespace enhance_advanced {
namespace detail {

/**
 * @struct EdgeGradientResult
 * @brief Output structure for Sobel gradient computation
 *
 * Contains gradient magnitude and direction maps.
 * REQ-ADV-012: Edge detection for collimation boundary detection
 */
struct EdgeGradientResult {
    Eigen::MatrixXf magnitude;  ///< Gradient magnitude at each pixel
    Eigen::MatrixXf direction;  ///< Gradient direction in radians [-pi, pi]

    /**
     * @brief Construct empty result for zero-sized image
     */
    EdgeGradientResult() = default;

    /**
     * @brief Construct result with pre-allocated matrices
     * @param rows Image height
     * @param cols Image width
     */
    EdgeGradientResult(int rows, int cols)
        : magnitude(rows, cols)
        , direction(rows, cols) {}

    /**
     * @brief Construct with the direction map left empty (QA-B-102, #179)
     * @param rows Image height
     * @param cols Image width
     * @param withDirection false: direction stays 0 x 0 and is not computed
     */
    EdgeGradientResult(int rows, int cols, bool withDirection)
        : magnitude(rows, cols)
        , direction(withDirection ? rows : 0, withDirection ? cols : 0) {}
};

/**
 * @brief Compute Sobel gradient magnitude and direction
 *
 * Applies 3x3 Sobel kernels to compute image gradients:
 * - Gx: Horizontal gradient (detects vertical edges)
 * - Gy: Vertical gradient (detects horizontal edges)
 *
 * Gradient magnitude: sqrt(Gx^2 + Gy^2)
 * Gradient direction: atan2(Gy, Gx)
 *
 * @param image Input image as Eigen matrix (float32 pixel values)
 * @param withDirection false: skip the per-pixel atan2 and leave
 *        EdgeGradientResult::direction empty (0 x 0). The magnitude map is
 *        identical either way. Collimation detection reads only the magnitude,
 *        and the atan2 was 3072x3072 calls it never used (QA-B-102, #179).
 * @return EdgeGradientResult containing the magnitude map, and the direction
 *         map when @p withDirection is true
 *
 * REQ-ADV-012: Edge detection for Hough transform input
 * AC-COL-001: Synthetic collimation border detection accuracy
 *
 * @note Input image must have at least 3x3 dimensions for Sobel kernels
 * @note Output matrices are same size as input
 */
EdgeGradientResult computeSobelGradients(const Eigen::MatrixXf& image,
                                         bool withDirection = true);

} // namespace detail
} // namespace enhance_advanced
} // namespace xpe

#endif /* XPE_ENHANCE_ADVANCED_EDGE_DETECTION_H */
