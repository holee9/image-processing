/**
 * @file edge_detection.cpp
 * @brief Sobel gradient computation implementation
 */

#include "edge_detection.h"
#include <cmath>
#include <algorithm>

namespace xpe {
namespace enhance_advanced {
namespace detail {

EdgeGradientResult computeSobelGradients(const Eigen::MatrixXf& image) {
    // @MX:NOTE: [AUTO] Sobel kernels for edge detection -- REQ-ADV-012
    // @MX:REASON: Standard 3x3 Sobel operators for gradient estimation

    const int rows = static_cast<int>(image.rows());
    const int cols = static_cast<int>(image.cols());

    EdgeGradientResult result(rows, cols);

    // Handle edge case: image too small for Sobel kernels
    if (rows < 3 || cols < 3) {
        // Return zero gradients for tiny images
        result.magnitude.setZero();
        result.direction.setZero();
        return result;
    }

    // Sobel kernels
    // Gx: Horizontal gradient (detects vertical edges)
    // Gy: Vertical gradient (detects horizontal edges)
    const int gx[3][3] = {
        {-1, 0, 1},
        {-2, 0, 2},
        {-1, 0, 1}
    };

    const int gy[3][3] = {
        {-1, -2, -1},
        { 0,  0,  0},
        { 1,  2,  1}
    };

    // Compute gradients for interior pixels (excluding 1-pixel border)
    for (int y = 1; y < rows - 1; ++y) {
        for (int x = 1; x < cols - 1; ++x) {
            // Apply Sobel kernels
            float gx_sum = 0.0f;
            float gy_sum = 0.0f;

            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    float pixel = image(y + ky, x + kx);
                    gx_sum += gx[ky + 1][kx + 1] * pixel;
                    gy_sum += gy[ky + 1][kx + 1] * pixel;
                }
            }

            // Compute gradient magnitude and direction
            result.magnitude(y, x) = std::sqrt(gx_sum * gx_sum + gy_sum * gy_sum);
            result.direction(y, x) = std::atan2(gy_sum, gx_sum);
        }
    }

    // Handle borders: replicate edge pixels (simple approach)
    // Top and bottom rows
    for (int x = 1; x < cols - 1; ++x) {
        result.magnitude(0, x) = result.magnitude(1, x);
        result.direction(0, x) = result.direction(1, x);
        result.magnitude(rows - 1, x) = result.magnitude(rows - 2, x);
        result.direction(rows - 1, x) = result.direction(rows - 2, x);
    }

    // Left and right columns
    for (int y = 0; y < rows; ++y) {
        result.magnitude(y, 0) = result.magnitude(y, 1);
        result.direction(y, 0) = result.direction(y, 1);
        result.magnitude(y, cols - 1) = result.magnitude(y, cols - 2);
        result.direction(y, cols - 1) = result.direction(y, cols - 2);
    }

    return result;
}

} // namespace detail
} // namespace enhance_advanced
} // namespace xpe
