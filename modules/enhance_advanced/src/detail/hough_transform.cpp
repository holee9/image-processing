/**
 * @file hough_transform.cpp
 * @brief Hough transform implementation for line detection
 */

#include "hough_transform.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace xpe {
namespace enhance_advanced {
namespace detail {

HoughTransform::HoughTransform(float thetaStep, float rhoStep)
    : thetaStep_(thetaStep * static_cast<float>(M_PI) / 180.0f)  // Convert degrees to radians
    , rhoStep_(rhoStep)
    , thetaBins_(static_cast<int>(180.0f / thetaStep))
    , maxRho_(0) {
}

Eigen::MatrixXi HoughTransform::buildAccumulator(const Eigen::MatrixXf& edgeMagnitude) {
    const int rows = static_cast<int>(edgeMagnitude.rows());
    const int cols = static_cast<int>(edgeMagnitude.cols());

    // Calculate maximum rho (diagonal of image) — cast sqrt (double) back to int via static_cast
    const double diag = std::sqrt(static_cast<double>(rows) * rows
                                + static_cast<double>(cols) * cols);
    maxRho_ = static_cast<int>(diag / rhoStep_) + 1;

    // Initialize accumulator: [theta bins, rho bins]
    Eigen::MatrixXi accumulator(thetaBins_, 2 * maxRho_);
    accumulator.setZero();

    // Build accumulator by voting
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float magnitude = edgeMagnitude(y, x);

            // Skip weak edges (below threshold)
            if (magnitude < 10.0f) {
                continue;
            }

            // Vote for all theta bins
            for (int t = 0; t < thetaBins_; ++t) {
                const float theta = static_cast<float>(t) * thetaStep_;
                const float rho = static_cast<float>(x) * std::cos(theta)
                                + static_cast<float>(y) * std::sin(theta);

                // Convert rho to bin index (offset by maxRho for negative values)
                int rhoBin = static_cast<int>(rho / rhoStep_) + maxRho_;

                // Clamp to valid range
                rhoBin = std::max(0, std::min(rhoBin, 2 * maxRho_ - 1));

                // Add vote
                accumulator(t, rhoBin) += static_cast<int>(magnitude);
            }
        }
    }

    return accumulator;
}

std::vector<HoughLine> HoughTransform::detectAxisAlignedLines(
    const Eigen::MatrixXi& accumulator,
    size_t numLines) {

    // Find all peaks
    std::vector<HoughLine> allPeaks = findPeaks(accumulator, 50, 5);

    // Filter for axis-aligned lines (horizontal: theta ~ 0 or 180, vertical: theta ~ 90)
    std::vector<HoughLine> axisAlignedPeaks;
    for (const auto& line : allPeaks) {
        if (isAxisAligned(line.theta)) {
            axisAlignedPeaks.push_back(line);
        }
    }

    // Sort by strength (descending)
    std::sort(axisAlignedPeaks.begin(), axisAlignedPeaks.end(),
        [](const HoughLine& a, const HoughLine& b) {
            return a.strength > b.strength;
        });

    // Return top N lines
    size_t count = std::min(numLines, axisAlignedPeaks.size());
    return std::vector<HoughLine>(axisAlignedPeaks.begin(), axisAlignedPeaks.begin() + count);
}

CollimationRectangle HoughTransform::extractCollimationRectangle(
    const std::vector<HoughLine>& horizontalLines,
    const std::vector<HoughLine>& verticalLines,
    int imageWidth,
    int imageHeight) {

    // @MX:NOTE: [AUTO] Collimation rectangle extraction -- REQ-ADV-041, REQ-ADV-052
    // @MX:REASON: Axis-aligned line filtering and confidence scoring

    CollimationRectangle result;

    // If insufficient lines detected, return full extent with low confidence
    if (horizontalLines.size() < 2 || verticalLines.size() < 2) {
        result.x0 = 0;
        result.y0 = 0;
        result.x1 = imageWidth - 1;
        result.y1 = imageHeight - 1;
        result.confidence = 0.0f;
        return result;
    }

    // Extract top 2 horizontal and vertical lines
    // Horizontal lines: top (min y) and bottom (max y)
    // Vertical lines: left (min x) and right (max x)

    // Convert polar (theta, rho) to Cartesian (x, y)
    // For horizontal lines (theta ~ 0 or 180): y = rho / sin(theta)
    // For vertical lines (theta ~ 90): x = rho / cos(theta)

    std::vector<float> horizontalY;
    for (const auto& line : horizontalLines) {
        float y = line.rho / std::sin(line.theta + 1e-6f);  // Avoid division by zero
        horizontalY.push_back(y);
    }

    std::vector<float> verticalX;
    for (const auto& line : verticalLines) {
        // Cast M_PI_2 (double) to float to avoid C4244 narrowing warning
        const float adjustedTheta = line.theta - static_cast<float>(M_PI_2) + 1e-6f;
        float x = line.rho / std::cos(adjustedTheta);  // Adjust for vertical
        verticalX.push_back(x);
    }

    // Sort to find top/bottom and left/right
    std::sort(horizontalY.begin(), horizontalY.end());
    std::sort(verticalX.begin(), verticalX.end());

    // Extract boundaries (using top 2 lines)
    float y0 = horizontalY[0];
    float y1 = horizontalY[1];
    float x0 = verticalX[0];
    float x1 = verticalX[1];

    // Swap if needed to ensure y0 < y1 and x0 < x1
    if (y0 > y1) std::swap(y0, y1);
    if (x0 > x1) std::swap(x0, x1);

    // Clip to image bounds
    // REQ-ADV-052: Boundary clipping to [0, width) and [0, height)
    result.x0 = std::max(0, static_cast<int>(std::round(x0)));
    result.y0 = std::max(0, static_cast<int>(std::round(y0)));
    result.x1 = std::min(imageWidth - 1, static_cast<int>(std::round(x1)));
    result.y1 = std::min(imageHeight - 1, static_cast<int>(std::round(y1)));

    // Calculate confidence score
    // REQ-ADV-041: Confidence = sum(4 peak values) / (4 * max_accumulator_value)
    float totalStrength = 0.0f;
    if (horizontalLines.size() >= 2) {
        totalStrength += horizontalLines[0].strength;
        totalStrength += horizontalLines[1].strength;
    }
    if (verticalLines.size() >= 2) {
        totalStrength += verticalLines[0].strength;
        totalStrength += verticalLines[1].strength;
    }

    // Normalize by maximum possible strength (heuristic)
    float maxExpectedStrength = 4.0f * 1000.0f;  // Adjust based on accumulator size
    result.confidence = totalStrength / maxExpectedStrength;
    result.confidence = std::min(1.0f, std::max(0.0f, result.confidence));

    return result;
}

std::vector<HoughLine> HoughTransform::findPeaks(
    const Eigen::MatrixXi& accumulator,
    int threshold,
    int windowSize) {

    std::vector<HoughLine> peaks;

    const int rows = static_cast<int>(accumulator.rows());
    const int cols = static_cast<int>(accumulator.cols());

    // Non-maximum suppression
    for (int t = 0; t < rows; ++t) {
        for (int r = 0; r < cols; ++r) {
            int value = accumulator(t, r);

            if (value < threshold) {
                continue;
            }

            // Check if this is a local maximum
            bool isMax = true;
            for (int dt = -windowSize / 2; dt <= windowSize / 2 && isMax; ++dt) {
                for (int dr = -windowSize / 2; dr <= windowSize / 2 && isMax; ++dr) {
                    int nt = t + dt;
                    int nr = r + dr;

                    if (nt >= 0 && nt < rows && nr >= 0 && nr < cols) {
                        if (accumulator(nt, nr) > value) {
                            isMax = false;
                        }
                    }
                }
            }

            if (isMax) {
                const float theta = static_cast<float>(t) * thetaStep_;
                const float rho = static_cast<float>(r - maxRho_) * rhoStep_;
                peaks.emplace_back(theta, rho, static_cast<float>(value));
            }
        }
    }

    return peaks;
}

bool HoughTransform::isAxisAligned(float theta) const {
    // Normalize theta to [0, 180) degrees
    float degrees = theta * 180.0f / static_cast<float>(M_PI);
    while (degrees < 0.0f) degrees += 180.0f;
    while (degrees >= 180.0f) degrees -= 180.0f;

    // Check if within +-5 degrees of horizontal (0 or 180) or vertical (90)
    const float tolerance = 5.0f;

    bool isHorizontal = (degrees <= tolerance) || (degrees >= 180.0f - tolerance);
    bool isVertical = (std::abs(degrees - 90.0f) <= tolerance);

    return isHorizontal || isVertical;
}

} // namespace detail
} // namespace enhance_advanced
} // namespace xpe
