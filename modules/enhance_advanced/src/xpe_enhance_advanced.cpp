/**
 * @file xpe_enhance_advanced.cpp
 * @brief Core xpe_enhance_advanced.dll implementation -- SPEC-XPE-P2-ADV
 *
 * Provides advanced image processing algorithms:
 * - SWU-2.5: Multiscale Frequency Processing (MFP)
 * - SWU-2.6: Edge Enhancement with fractional-order differentiation
 * - SWU-2.8: Collimation ROI Detection
 * - SWU-2.10: Exposure Index Calculation
 *
 * IEC 62304 Class B -- No C++ exceptions across C ABI boundary.
 */

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"
#include "mfp_scalar.h"
#include "detail/fractional_derivative.h"
#include "detail/exposure_index.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

// @MX:ANCHOR: [AUTO] Initialization flag -- REQ-ADV-001, REQ-ADV-020
// @MX:REASON: Module lifecycle state; high fan-in from all processing functions

namespace {
    std::mutex g_initMutex;
    bool g_initialized = false;
    const char* XPE_ENHANCE_ADVANCED_VERSION = "1.0.0";
}

/* ============================================================================
 * Lifecycle Management (REQ-ADV-001, REQ-ADV-020)
 * ============================================================================ */

extern "C" {

XPE_API XpeErrorCode xpe_enhance_advanced_init(const char* configJsonOrNull) {
    std::lock_guard<std::mutex> lock(g_initMutex);

    // Validate config parameter (empty string is invalid)
    if (configJsonOrNull != nullptr && strlen(configJsonOrNull) == 0) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Parse config JSON if provided
    if (configJsonOrNull != nullptr) {
        try {
            nlohmann::json config = nlohmann::json::parse(configJsonOrNull);
            // TODO: Store configuration parameters for use in processing functions
            // For now, just validate JSON format
        } catch (const nlohmann::json::exception&) {
            return XPE_ERR_CONFIG_INVALID;
        }
    }

    // Initialize xpe_common if not already initialized
    XpeErrorCode commonResult = xpe_init(nullptr);
    if (commonResult != XPE_OK && commonResult != XPE_ERR_NOT_INITIALIZED) {
        return commonResult;
    }

    g_initialized = true;
    return XPE_OK;
}

XPE_API void xpe_enhance_advanced_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_initMutex);
    g_initialized = false;
}

XPE_API const char* xpe_enhance_advanced_version(void) {
    return XPE_ENHANCE_ADVANCED_VERSION;
}

/* ============================================================================
 * Multiscale Frequency Processing (SWU-2.5, REQ-ADV-010)
 * ============================================================================ */

XPE_API XpeErrorCode xpe_multiscale_process(
    XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    const char* configJsonOrNull) {

    std::lock_guard<std::mutex> lock(g_initMutex);

    // REQ-ADV-020: Not-initialized guard
    if (!g_initialized) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    // REQ-ADV-022: NULL pointer guard
    if (img == nullptr || meta == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-ADV-071: Format validation (advanced module only supports FLOAT32)
    if (img->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    // REQ-ADV-070: Dimension validation
    if (img->width == 0 || img->height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    // T-206, T-207: Parse configuration
    auto config = xpe::enhance_advanced::MfpConfig::fromJson(configJsonOrNull);

    // T-201: Apply MFP processing
    return xpe::enhance_advanced::applyMfpScalar(img, config);
}

/* ============================================================================
 * Fractional-Order Edge Enhancement (SWU-2.6, REQ-ADV-011)
 * ============================================================================ */

XPE_API XpeErrorCode xpe_fractional_process(
    XpeImageBuffer* img,
    float order,
    const char* configJsonOrNull) {

    std::lock_guard<std::mutex> lock(g_initMutex);

    // REQ-ADV-020: Not-initialized guard
    if (!g_initialized) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    // REQ-ADV-022: NULL pointer guard
    if (img == nullptr) {
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

    // REQ-ADV-021: Order parameter validation [0.0, 2.0]
    if (order < 0.0f || order > 2.0f) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Parse configuration
    auto config = xpe::enhance_advanced::FractionalConfig::fromJson(configJsonOrNull);
    config.order = order;  // Override with parameter

    // Apply fractional-order enhancement with overshoot limiting (SAF-100)
    try {
        return xpe::enhance_advanced::applyFractionalDerivative(img, config);
    } catch (const std::runtime_error& e) {
        // SAF-100: Catch attempts to disable overshoot limiting
        if (std::string(e.what()).find("SAF-100") != std::string::npos) {
            return XPE_ERR_SAFETY_VIOLATION;
        }
        return XPE_ERR_INTERNAL;
    } catch (...) {
        return XPE_ERR_INTERNAL;
    }
}

/* ============================================================================
 * Collimation ROI Detection (SWU-2.8, REQ-ADV-012)
 * ============================================================================ */

// Implemented in xpe_collimation_detect.cpp (T-045, T-046, T-047)
// See that file for implementation details

/* ============================================================================
 * Exposure Index Calculation (SWU-2.10, REQ-ADV-013)
 * ============================================================================ */

XPE_API XpeErrorCode xpe_calc_exposure_index(
    const XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    float* eiOut,
    float* deviationIndexOut) {

    std::lock_guard<std::mutex> lock(g_initMutex);

    // REQ-ADV-020: Not-initialized guard
    if (!g_initialized) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    // REQ-ADV-022: NULL pointer guard
    if (img == nullptr || meta == nullptr || eiOut == nullptr || deviationIndexOut == nullptr) {
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

    // REQ-ADV-013: Calculate IEC 62494-1 EI and DI
    // REQ-ADV-022: NULL pointer guard (already checked above)
    // REQ-ADV-032: No NaN/Inf in output
    try {
        return xpe::enhance_advanced::ExposureIndexCalculator::calculate(
            img, meta, eiOut, deviationIndexOut);
    } catch (const std::exception&) {
        // REQ-ADV-030: No exceptions across C ABI
        return XPE_ERR_INTERNAL;
    } catch (...) {
        return XPE_ERR_INTERNAL;
    }
}

} // extern "C"
