/**
 * @file xpe_collimation_detect.cpp
 * @brief Collimation ROI detection implementation -- SPEC-XPE-P2-ADV
 *
 * Implements SWU-2.8: ROI-Aware Collimation Detection using Hough transform.
 * REQ-ADV-012, REQ-ADV-041, REQ-ADV-052
 */

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"
#include "detail/hough_transform.h"
#include "detail/edge_detection.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <Eigen/Dense>
#include <cstring>
#include <chrono>
#include <mutex>

namespace {

/**
 * @brief Configuration for collimation detection
 *
 * Parsed from optional JSON config parameter.
 */
struct CollimationConfig {
    float edgeThreshold;      ///< Minimum edge magnitude for Hough voting
    float confidenceThreshold; ///< Minimum confidence for valid detection (default: 0.7)
    int thetaStep;            ///< Hough theta step in degrees
    int rhoStep;              ///< Hough rho step in pixels

    CollimationConfig()
        : edgeThreshold(10.0f)
        , confidenceThreshold(0.7f)
        , thetaStep(1)
        , rhoStep(1) {}

    /**
     * @brief Parse configuration from JSON string
     * @param configJsonOrNull JSON string or nullptr for defaults
     * @return Parsed configuration
     */
    static CollimationConfig fromJson(const char* configJsonOrNull) {
        CollimationConfig config;

        if (configJsonOrNull == nullptr) {
            return config;
        }

        try {
            nlohmann::json json = nlohmann::json::parse(configJsonOrNull);

            if (json.contains("edge_threshold")) {
                config.edgeThreshold = json["edge_threshold"];
            }
            if (json.contains("confidence_threshold")) {
                config.confidenceThreshold = json["confidence_threshold"];
            }
            if (json.contains("theta_step")) {
                config.thetaStep = json["theta_step"];
            }
            if (json.contains("rho_step")) {
                config.rhoStep = json["rho_step"];
            }
        } catch (const nlohmann::json::exception& e) {
            spdlog::warn("Failed to parse collimation config JSON: {}", e.what());
            // Return default config on parse error
        }

        return config;
    }
};

} // anonymous namespace

/* ============================================================================
 * Collimation ROI Detection (SWU-2.8, REQ-ADV-012)
 * ============================================================================ */

extern "C" {

XPE_API XpeErrorCode xpe_detect_collimation(
    const XpeImageBuffer* img,
    int32_t* x0Out,
    int32_t* y0Out,
    int32_t* x1Out,
    int32_t* y1Out,
    const char* configJsonOrNull) {

    // @MX:NOTE: [AUTO] Collimation detection lifecycle -- REQ-ADV-012, REQ-ADV-041
    // @MX:REASON: Hough transform pipeline with confidence-based fallback

    try {
        // REQ-ADV-020: Not-initialized guard
        // Simple check: if version function returns nullptr, not initialized
        const char* version = xpe_enhance_advanced_version();
        if (version == nullptr || strlen(version) == 0) {
            return XPE_ERR_NOT_INITIALIZED;
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

        // Parse configuration
        CollimationConfig config = CollimationConfig::fromJson(configJsonOrNull);

        // Convert image buffer to Eigen matrix
        const int width = static_cast<int>(img->width);
        const int height = static_cast<int>(img->height);
        const float* data = static_cast<const float*>(img->data);

        Eigen::Map<const Eigen::MatrixXf> image(data, height, width);

        // Step 1: Compute edge gradients using Sobel operator
        auto startTime = std::chrono::high_resolution_clock::now();

        xpe::enhance_advanced::detail::EdgeGradientResult gradients =
            xpe::enhance_advanced::detail::computeSobelGradients(image);

        auto edgeTime = std::chrono::high_resolution_clock::now();
        auto edgeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            edgeTime - startTime);

        // Step 2: Build Hough accumulator from edge map
        xpe::enhance_advanced::detail::HoughTransform hough(
            static_cast<float>(config.thetaStep),
            static_cast<float>(config.rhoStep)
        );

        Eigen::MatrixXi accumulator = hough.buildAccumulator(gradients.magnitude);

        auto houghTime = std::chrono::high_resolution_clock::now();
        auto houghDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            houghTime - edgeTime);

        // Step 3: Detect axis-aligned lines
        std::vector<xpe::enhance_advanced::detail::HoughLine> lines =
            hough.detectAxisAlignedLines(accumulator, 4);

        // Separate horizontal and vertical lines
        std::vector<xpe::enhance_advanced::detail::HoughLine> horizontalLines;
        std::vector<xpe::enhance_advanced::detail::HoughLine> verticalLines;

        for (const auto& line : lines) {
            float degrees = line.theta * 180.0f / static_cast<float>(M_PI);
            while (degrees < 0.0f) degrees += 180.0f;
            while (degrees >= 180.0f) degrees -= 180.0f;

            // Horizontal: theta ~ 0 or 180, Vertical: theta ~ 90
            if (degrees < 45.0f || degrees > 135.0f) {
                horizontalLines.push_back(line);
            } else {
                verticalLines.push_back(line);
            }
        }

        auto detectTime = std::chrono::high_resolution_clock::now();
        auto detectDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            detectTime - houghTime);

        // Step 4: Extract collimation rectangle
        xpe::enhance_advanced::detail::CollimationRectangle rect =
            hough.extractCollimationRectangle(horizontalLines, verticalLines, width, height);

        // REQ-ADV-041: Confidence-based fallback
        // If confidence < 0.7, return full image extent with warning
        if (rect.confidence < config.confidenceThreshold) {
            spdlog::warn(
                "ROI confidence ({:.2f}) below threshold ({:.2f}). Using full-image extent.",
                rect.confidence,
                config.confidenceThreshold
            );

            *x0Out = 0;
            *y0Out = 0;
            *x1Out = width - 1;
            *y1Out = height - 1;
        } else {
            // Use detected rectangle
            // REQ-ADV-052: +-3 pixel accuracy enforced by Hough resolution
            *x0Out = rect.x0;
            *y0Out = rect.y0;
            *x1Out = rect.x1;
            *y1Out = rect.y1;
        }

        auto totalTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            totalTime - startTime);

        // REQ-ADV-091: Diagnostic logging
        spdlog::info(
            "{{\"stage\":\"COLLIMATION_DETECTION\","
            "\"edge_time_ms\":{},"
            "\"hough_time_ms\":{},"
            "\"detect_time_ms\":{},"
            "\"total_time_ms\":{},"
            "\"confidence\":{:.2f},"
            "\"roi\":[{},{},{},{}]}}",
            edgeDuration.count(),
            houghDuration.count(),
            detectDuration.count(),
            totalDuration.count(),
            rect.confidence,
            *x0Out, *y0Out, *x1Out, *y1Out
        );

        return XPE_OK;

    } catch (const std::exception& e) {
        // REQ-ADV-030: No exceptions across C ABI
        spdlog::error("Exception in xpe_detect_collimation: {}", e.what());
        return XPE_ERR_INTERNAL;
    } catch (...) {
        spdlog::error("Unknown exception in xpe_detect_collimation");
        return XPE_ERR_INTERNAL;
    }
}

} // extern "C"
