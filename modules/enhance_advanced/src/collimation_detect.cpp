/**
 * @file collimation_detect.cpp
 * @brief Public API entry point for Collimation ROI Detection.
 *
 * Validates inputs, parses config via internal.h parser, then delegates to
 * Hough transform pipeline (detail::edge_detection + detail::hough_transform).
 *
 * SPEC: SPEC-XPE-P2-ADV, SWU-2.8
 * REQ-ADV-012: Collimation detection execution
 * REQ-ADV-041: Confidence-based fallback
 * REQ-ADV-052: Collimation detection accuracy (+-3 px)
 */

#include "xpe/enhance_advanced/enhance_advanced_api.h"
#include "xpe/enhance_advanced/internal.h"
#include "detail/edge_detection.h"
#include "detail/hough_transform.h"

#include <spdlog/spdlog.h>
#include <Eigen/Dense>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

// Access the module state defined in enhance_advanced.cpp
extern bool          g_initialized;
extern std::mutex    g_initMutex;

extern "C" {

XPE_API XpeErrorCode xpe_detect_collimation(
    const XpeImageBuffer* img,
    int32_t*              x0Out,
    int32_t*              y0Out,
    int32_t*              x1Out,
    int32_t*              y1Out,
    const char*           configJsonOrNull)
{
    // @MX:ANCHOR: [AUTO] Collimation detection public API entry -- REQ-ADV-012
    // @MX:REASON: High fan_in; Hough pipeline with confidence-based fallback

    // REQ-ADV-020: Not-initialized guard
    {
        std::lock_guard<std::mutex> lock(g_initMutex);
        if (!g_initialized) {
            return XPE_ERR_NOT_INITIALIZED;
        }
    }

    // REQ-ADV-022: NULL pointer guard
    if (img == nullptr || x0Out == nullptr || y0Out == nullptr ||
        x1Out == nullptr || y1Out == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-ADV-071: Format validation
    if (img->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    // REQ-ADV-070: Dimension validation
    if (img->width == 0 || img->height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Data pointer validation
    if (img->data == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    try {
        // Parse config via internal.h parser
        float sensitivity;
        float minAreaRatio;
        int   borderMargin;

        if (!xpe::enhance_advanced::config::parse_collimation_config(
                configJsonOrNull, sensitivity, minAreaRatio, borderMargin)) {
            spdlog::error("xpe_detect_collimation: invalid config JSON");
            return XPE_ERR_CONFIG_INVALID;
        }

        const int width  = static_cast<int>(img->width);
        const int height = static_cast<int>(img->height);
        const float* data = static_cast<const float*>(img->data);

        auto startTime = std::chrono::high_resolution_clock::now();

        // Step 1: Map image to Eigen and compute Sobel gradients
        // @MX:NOTE: [AUTO] Row-major image data mapping -- image buffer is row-major (y*w+x)
        // @MX:REASON: Eigen defaults to column-major; must use RowMajor flag to match
        //   the image buffer layout p[y * width + x]. Without this, x/y axes are transposed.
        using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        Eigen::Map<const RowMajorMatrixXf> image(data, height, width);

        xpe::enhance_advanced::detail::EdgeGradientResult gradients =
            xpe::enhance_advanced::detail::computeSobelGradients(image);

        auto edgeTime = std::chrono::high_resolution_clock::now();

        // Step 2: Build Hough accumulator
        // @MX:NOTE: [AUTO] Hough theta resolution derived from sensitivity
        // @MX:REASON: Collimation requires fine angular resolution (1-2 deg) for accurate
        //   axis-aligned line detection. Coarser resolution (3+ deg) causes rho drift
        //   and misclassification of line positions.
        int thetaStep = std::max(1, static_cast<int>(2.0f * (1.0f - sensitivity) + 1.0f));
        int rhoStep   = 1;

        xpe::enhance_advanced::detail::HoughTransform hough(
            static_cast<float>(thetaStep),
            static_cast<float>(rhoStep));

        Eigen::MatrixXi accumulator = hough.buildAccumulator(gradients.magnitude);

        auto houghTime = std::chrono::high_resolution_clock::now();

        // Step 3: Detect axis-aligned lines
        // Increased threshold for better accuracy (REQ-ADV-052: +-3 pixel)
        std::vector<xpe::enhance_advanced::detail::HoughLine> lines =
            hough.detectAxisAlignedLines(accumulator, 8);

        // @MX:NOTE: [AUTO] Hough line orientation classification
        // @MX:REASON: In Hough space, theta=0 => normal along x-axis => line is vertical (x=const).
        //   theta=PI/2 (90 deg) => normal along y-axis => line is horizontal (y=const).
        //   We classify by which Cartesian axis the line runs parallel to:
        //   degrees near 0 or 180: vertical lines (x=const) => verticalLines
        //   degrees near 90: horizontal lines (y=const) => horizontalLines
        std::vector<xpe::enhance_advanced::detail::HoughLine> horizontalLines;
        std::vector<xpe::enhance_advanced::detail::HoughLine> verticalLines;

        for (const auto& line : lines) {
            float degrees = line.theta * 180.0f / static_cast<float>(M_PI);
            while (degrees < 0.0f)    degrees += 180.0f;
            while (degrees >= 180.0f) degrees -= 180.0f;

            // theta ~ 0 or 180: vertical lines (x = rho / cos(theta))
            // theta ~ 90: horizontal lines (y = rho / sin(theta))
            if (degrees < 45.0f || degrees > 135.0f) {
                verticalLines.push_back(line);
            } else {
                horizontalLines.push_back(line);
            }
        }

        auto detectTime = std::chrono::high_resolution_clock::now();

        // Step 4: Extract collimation rectangle
        xpe::enhance_advanced::detail::CollimationRectangle rect =
            hough.extractCollimationRectangle(horizontalLines, verticalLines, width, height);

        // REQ-ADV-041: Confidence-based fallback
        // Convert sensitivity to confidence threshold: sensitivity 0.5 -> threshold 0.7
        float confidenceThreshold = 0.7f + 0.3f * sensitivity;

        if (rect.confidence < confidenceThreshold) {
            spdlog::warn("ROI confidence ({:.2f}) below threshold ({:.2f}). "
                         "Using full-image extent.",
                         rect.confidence, confidenceThreshold);

            // @MX:NOTE: [FIX] Low confidence returns full image extent (no border margin)
            // @MX:REASON: REQ-ADV-041 requires actual full extent [0, width-1] when
            //   confidence is low, not margin-padded extent. Previous code incorrectly
            //   applied borderMargin in fallback path, causing 8-pixel insets.
            *x0Out = 0;
            *y0Out = 0;
            *x1Out = width - 1;
            *y1Out = height - 1;
        } else {
            // Use detected rectangle with border margin applied
            *x0Out = std::max(borderMargin, rect.x0);
            *y0Out = std::max(borderMargin, rect.y0);
            *x1Out = std::min(width  - 1 - borderMargin, rect.x1);
            *y1Out = std::min(height - 1 - borderMargin, rect.y1);
        }

        // REQ-ADV-052: Validate detected ROI meets minimum area ratio
        int roiWidth  = *x1Out - *x0Out + 1;
        int roiHeight = *y1Out - *y0Out + 1;
        float roiArea = static_cast<float>(roiWidth * roiHeight);
        float imgArea = static_cast<float>(width * height);

        if (roiArea / imgArea < minAreaRatio) {
            spdlog::warn("ROI area ratio ({:.3f}) below minimum ({:.3f}). "
                         "Using full-image extent.",
                         roiArea / imgArea, minAreaRatio);

            // @MX:NOTE: [FIX] Minimum area ratio fallback also returns full extent
            *x0Out = 0;
            *y0Out = 0;
            *x1Out = width - 1;
            *y1Out = height - 1;
        }

        auto totalTime = std::chrono::high_resolution_clock::now();

        // REQ-ADV-091: Diagnostic logging
        auto edgeMs  = std::chrono::duration_cast<std::chrono::milliseconds>(edgeTime - startTime);
        auto houghMs = std::chrono::duration_cast<std::chrono::milliseconds>(houghTime - edgeTime);
        auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(totalTime - startTime);

        spdlog::info("{{\"stage\":\"COLLIMATION_DETECTION\","
                     "\"edge_time_ms\":{},"
                     "\"hough_time_ms\":{},"
                     "\"total_time_ms\":{},"
                     "\"confidence\":{:.2f},"
                     "\"roi\":[{},{},{},{}]}}",
                     edgeMs.count(), houghMs.count(), totalMs.count(),
                     rect.confidence, *x0Out, *y0Out, *x1Out, *y1Out);

        return XPE_OK;

    } catch (const std::exception& e) {
        // REQ-ADV-030: No exceptions across C ABI boundary
        spdlog::error("xpe_detect_collimation: exception: {}", e.what());
        return XPE_ERR_INTERNAL;
    } catch (...) {
        spdlog::error("xpe_detect_collimation: unknown exception");
        return XPE_ERR_INTERNAL;
    }
}

} // extern "C"
