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
#include "xpe/enhance_advanced/internal.h"
#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"
#include "detail/exposure_index.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <mutex>

// @MX:ANCHOR: [AUTO] Initialization flag -- REQ-ADV-001, REQ-ADV-020
// @MX:REASON: Module lifecycle state; high fan-in from all processing functions

// Module state (defined here, declared as extern in internal.h)
bool          g_initialized = false;
std::mutex    g_initMutex;

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
 * Processing Functions
 * ============================================================================ */

// xpe_multiscale_process: Implemented in multiscale_process.cpp
// xpe_fractional_process: Implemented in fractional_process.cpp
// xpe_detect_collimation: Implemented in collimation_detect.cpp

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
