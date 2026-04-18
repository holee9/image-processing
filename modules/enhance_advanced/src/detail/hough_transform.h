/**
 * @file hough_transform.h
 * @brief Hough transform for line detection in collimation boundaries
 *
 * Provides Hough accumulator construction and peak detection
 * for axis-aligned collimation border detection.
 * Part of SWU-2.8 Collimation ROI Detection (SPEC-XPE-P2-ADV).
 */

#ifndef XPE_ENHANCE_ADVANCED_HOUGH_TRANSFORM_H
#define XPE_ENHANCE_ADVANCED_HOUGH_TRANSFORM_H

#include "edge_detection.h"
#include <Eigen/Dense>
#include <vector>
#include <cstdint>

namespace xpe {
namespace enhance_advanced {
namespace detail {

/**
 * @struct HoughLine
 * @brief Detected line in Hough space
 *
 * Represents a line detected via Hough transform.
 * REQ-ADV-012: Hough line detection for collimation borders
 */
struct HoughLine {
    float theta;    ///< Line angle in radians [0, pi)
    float rho;      ///< Line distance from origin in pixels
    float strength; ///< Accumulator peak value (line strength)

    HoughLine(float t, float r, float s)
        : theta(t), rho(r), strength(s) {}
};

/**
 * @struct CollimationRectangle
 * @brief Detected collimation ROI rectangle
 *
 * Axis-aligned bounding rectangle for collimation area.
 * REQ-ADV-012, REQ-ADV-052: Collimation boundary extraction
 */
struct CollimationRectangle {
    int x0;      ///< Left boundary (pixel coordinate)
    int y0;      ///< Top boundary (pixel coordinate)
    int x1;      ///< Right boundary (pixel coordinate)
    int y1;      ///< Bottom boundary (pixel coordinate)
    float confidence;  ///< Detection confidence [0.0, 1.0]

    CollimationRectangle()
        : x0(0), y0(0), x1(0), y1(0), confidence(0.0f) {}

    CollimationRectangle(int x0_, int y0_, int x1_, int y1_, float conf)
        : x0(x0_), y0(y0_), x1(x1_), y1(y1_), confidence(conf) {}
};

/**
 * @class HoughTransform
 * @brief Hough transform for line detection
 *
 * Detects lines using Hough transform with focus on axis-aligned lines.
 * REQ-ADV-012: Hough transform for collimation boundary detection
 * AC-COL-001: Synthetic collimation detection with +-3 pixel accuracy
 */
class HoughTransform {
public:
    /**
     * @brief Construct Hough transform with specified resolution
     * @param thetaStep Angle resolution in degrees (default: 1 degree)
     * @param rhoStep Distance resolution in pixels (default: 1 pixel)
     *
     * @note Default values provide sufficient accuracy for collimation detection
     */
    explicit HoughTransform(float thetaStep = 1.0f, float rhoStep = 1.0f);

    /**
     * @brief Build Hough accumulator from edge gradient map
     * @param edgeMagnitude Gradient magnitude from Sobel operator
     * @return Hough accumulator matrix (theta x rho)
     *
     * REQ-ADV-012: Hough accumulator construction
     *
     * @note Accumulator dimensions: [0, 180) degrees x [0, maxRho] pixels
     */
    Eigen::MatrixXi buildAccumulator(const Eigen::MatrixXf& edgeMagnitude);

    /**
     * @brief Detect axis-aligned lines from Hough accumulator
     * @param accumulator Hough accumulator matrix
     * @param numLines Number of strong lines to detect per orientation
     * @return Vector of detected lines sorted by strength
     *
     * REQ-ADV-012: Axis-aligned line filtering (theta within +-5 degrees of 0 or 90)
     *
     * @note Filters for horizontal (theta ~ 0 or 180) and vertical (theta ~ 90) lines
     */
    std::vector<HoughLine> detectAxisAlignedLines(
        const Eigen::MatrixXi& accumulator,
        size_t numLines = 4);

    /**
     * @brief Extract collimation rectangle from detected lines
     * @param horizontalLines Detected horizontal lines (sorted by strength)
     * @param verticalLines Detected vertical lines (sorted by strength)
     * @param imageWidth Image width for boundary validation
     * @param imageHeight Image height for boundary validation
     * @return Collimation rectangle with confidence score
     *
     * REQ-ADV-041: Confidence-based fallback (< 0.7 -> full extent)
     * REQ-ADV-052: Boundary extraction with +-3 pixel accuracy
     *
     * @note Clips coordinates to image bounds [0, width) and [0, height)
     */
    CollimationRectangle extractCollimationRectangle(
        const std::vector<HoughLine>& horizontalLines,
        const std::vector<HoughLine>& verticalLines,
        int imageWidth,
        int imageHeight);

private:
    float thetaStep_;    ///< Angle resolution in radians
    float rhoStep_;      ///< Distance resolution in pixels
    int thetaBins_;      ///< Number of angle bins (180 degrees / thetaStep)
    int maxRho_;         ///< Maximum rho value (diagonal of image)

    /**
     * @brief Find local peaks in accumulator using non-maximum suppression
     * @param accumulator Hough accumulator matrix
     * @param threshold Minimum peak threshold
     * @param windowSize Size of suppression window
     * @return Vector of (theta, rho, value) peak tuples
     */
    std::vector<HoughLine> findPeaks(
        const Eigen::MatrixXi& accumulator,
        int threshold = 0,
        int windowSize = 5);

    /**
     * @brief Check if line is axis-aligned (horizontal or vertical)
     * @param theta Line angle in radians
     * @return true if theta within +-5 degrees of 0, 90, or 180 degrees
     *
     * REQ-ADV-012: Axis-aligned line filtering
     */
    bool isAxisAligned(float theta) const;
};

} // namespace detail
} // namespace enhance_advanced
} // namespace xpe

#endif /* XPE_ENHANCE_ADVANCED_HOUGH_TRANSFORM_H */
