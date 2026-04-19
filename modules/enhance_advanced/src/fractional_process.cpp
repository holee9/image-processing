/**
 * @file fractional_process.cpp
 * @brief Public API entry point for Fractional-order Edge Enhancement.
 *
 * Validates inputs, parses config via internal.h parser, then delegates to
 * the Gruenwald-Letnikov fractional derivative implementation.
 *
 * SPEC: SPEC-XPE-P2-ADV, SWU-2.6
 * REQ-ADV-011: Fractional-order process execution
 * REQ-ADV-021: Invalid order parameter guard
 * REQ-ADV-051: Mandatory overshoot limiting (SAF-100)
 */

#include "xpe/enhance_advanced/enhance_advanced_api.h"
#include "xpe/enhance_advanced/internal.h"
#include "detail/fractional_derivative.h"

#include <spdlog/spdlog.h>
#include <mutex>

// Access the module state defined in enhance_advanced.cpp
extern bool          g_initialized;
extern std::mutex    g_initMutex;

extern "C" {

XPE_API XpeErrorCode xpe_fractional_process(
    XpeImageBuffer* img,
    float           order,
    const char*     configJsonOrNull)
{
    // @MX:ANCHOR: [AUTO] Fractional process public API entry -- REQ-ADV-011
    // @MX:REASON: High fan_in expected; validates inputs and dispatches to GL derivative

    // REQ-ADV-020: Not-initialized guard
    {
        std::lock_guard<std::mutex> lock(g_initMutex);
        if (!g_initialized) {
            return XPE_ERR_NOT_INITIALIZED;
        }
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

    // Data pointer validation
    if (img->data == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-ADV-021: Order parameter validation [0.0, 2.0]
    if (order < XPE_FRAC_MIN_ORDER || order > XPE_FRAC_MAX_ORDER) {
        spdlog::error("xpe_fractional_process: order {:.3f} out of range [{:.1f}, {:.1f}]",
                      order, XPE_FRAC_MIN_ORDER, XPE_FRAC_MAX_ORDER);
        return XPE_ERR_INVALID_INPUT;
    }

    try {
        // Parse config via internal.h parser
        int   iterations;
        float stepSize;

        if (!xpe::enhance_advanced::config::parse_fractional_config(
                configJsonOrNull, iterations, stepSize)) {
            spdlog::error("xpe_fractional_process: invalid config JSON");
            return XPE_ERR_CONFIG_INVALID;
        }

        // Build FractionalConfig
        xpe::enhance_advanced::FractionalConfig fracConfig;
        fracConfig.order = order;

        // Apply fractional derivative with SAF-100 overshoot limiting
        // Iterative application: apply the derivative 'iterations' times
        XpeErrorCode result = XPE_OK;
        for (int iter = 0; iter < iterations; ++iter) {
            result = xpe::enhance_advanced::applyFractionalDerivative(img, fracConfig);
            if (result != XPE_OK) {
                break;
            }
        }

        if (result == XPE_ERR_SAFETY_VIOLATION) {
            spdlog::error("xpe_fractional_process: SAF-100 overshoot violation detected");
        } else if (result != XPE_OK) {
            spdlog::warn("xpe_fractional_process: failed with code {}", result);
        } else {
            spdlog::debug("xpe_fractional_process: completed (order={:.2f}, iters={}, step={:.2f})",
                          order, iterations, stepSize);
        }

        return result;

    } catch (const std::runtime_error& e) {
        // SAF-100 violation from config parsing
        std::string msg = e.what();
        if (msg.find("SAF-100") != std::string::npos) {
            spdlog::error("xpe_fractional_process: SAF-100 violation: {}", msg);
            return XPE_ERR_SAFETY_VIOLATION;
        }
        spdlog::error("xpe_fractional_process: runtime error: {}", msg);
        return XPE_ERR_INTERNAL;
    } catch (const std::exception& e) {
        // REQ-ADV-030: No exceptions across C ABI boundary
        spdlog::error("xpe_fractional_process: exception: {}", e.what());
        return XPE_ERR_INTERNAL;
    } catch (...) {
        spdlog::error("xpe_fractional_process: unknown exception");
        return XPE_ERR_INTERNAL;
    }
}

} // extern "C"
