/**
 * @file multiscale_process.cpp
 * @brief Public API entry point for Multiscale Frequency Processing (MFP).
 *
 * Validates inputs, parses config via internal.h parser, then delegates to
 * the LaplacianPyramid implementation in mfp_scalar.cpp.
 *
 * SPEC: SPEC-XPE-P2-ADV, SWU-2.5
 * REQ-ADV-010: MFP execution
 * REQ-ADV-050: Identity reconstruction fidelity
 */

#include "xpe/enhance_advanced/enhance_advanced_api.h"
#include "xpe/enhance_advanced/internal.h"
#include "mfp_scalar.h"

#include <spdlog/spdlog.h>
#include <mutex>

// Access the module state defined in enhance_advanced.cpp
// Declared as extern in internal.h
extern bool          g_initialized;
extern std::mutex    g_initMutex;

extern "C" {

XPE_API XpeErrorCode xpe_multiscale_process(
    XpeImageBuffer*         img,
    const XpeImageMetadata* meta,
    const char*             configJsonOrNull)
{
    // @MX:ANCHOR: [AUTO] MFP public API entry -- REQ-ADV-010, REQ-ADV-050
    // @MX:REASON: High fan_in expected; validates inputs and dispatches to LaplacianPyramid

    // REQ-ADV-020: Not-initialized guard
    {
        std::lock_guard<std::mutex> lock(g_initMutex);
        if (!g_initialized) {
            return XPE_ERR_NOT_INITIALIZED;
        }
    }

    // REQ-ADV-022: NULL pointer guard
    if (img == nullptr || meta == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-ADV-071: Format validation (FLOAT32 only)
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
        int   levels;
        float edgeGain, textureGain, flatGain, noiseThreshold;

        if (!xpe::enhance_advanced::config::parse_mfp_config(
                configJsonOrNull, levels, edgeGain, textureGain, flatGain, noiseThreshold)) {
            spdlog::error("xpe_multiscale_process: invalid config JSON");
            return XPE_ERR_CONFIG_INVALID;
        }

        // Build MfpConfig from parsed values
        xpe::enhance_advanced::MfpConfig mfpConfig;
        mfpConfig.numLevels      = levels;
        mfpConfig.edgeGain       = edgeGain;
        mfpConfig.textureGain    = textureGain;
        mfpConfig.flatGain       = flatGain;
        mfpConfig.noiseThreshold = noiseThreshold;

        // Delegate to LaplacianPyramid implementation
        XpeErrorCode result = xpe::enhance_advanced::applyMfpScalar(img, mfpConfig);

        if (result != XPE_OK) {
            spdlog::warn("xpe_multiscale_process: MFP failed with code {}", result);
        } else {
            spdlog::debug("xpe_multiscale_process: completed (levels={}, edge={:.2f}, "
                          "texture={:.2f}, flat={:.2f})",
                          levels, edgeGain, textureGain, flatGain);
        }

        return result;

    } catch (const std::exception& e) {
        // REQ-ADV-030: No exceptions across C ABI boundary
        spdlog::error("xpe_multiscale_process: exception: {}", e.what());
        return XPE_ERR_INTERNAL;
    } catch (...) {
        spdlog::error("xpe_multiscale_process: unknown exception");
        return XPE_ERR_INTERNAL;
    }
}

} // extern "C"
