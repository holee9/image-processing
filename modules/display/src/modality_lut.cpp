/**
 * @file modality_lut.cpp
 * @brief Modality LUT implementation (SWU-3.1).
 * REQ-DISP-001 to REQ-DISP-008
 * SPEC: SPEC-XPE-P1B-DISP
 */

#include "xpe/display/display_api.h"
#include "xpe/display/display_internal.h"

#include <cmath>
#include <algorithm>
#include <cstdint>

// @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
// @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
// @MX:SPEC: SPEC-XPE-P1B-DISP
extern "C" XpeErrorCode xpe_apply_modality_lut(XpeImageBuffer*             img,
                                                 const XpeModalityLutParams* params) {
    // Validate inputs
    if (!img) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (!params) {
        return XPE_ERR_INVALID_INPUT;
    }
    // REQ-DISP-004: must be FLOAT32
    XpeErrorCode fmt_rc = xpe_validate_float32(img);
    if (fmt_rc != XPE_OK) {
        return fmt_rc;
    }

    const size_t count = xpe_pixel_count(img);
    float* px = static_cast<float*>(img->data);

    if (params->mode == XPE_MODALITY_LUT_LINEAR) {
        // REQ-DISP-007: slope must not be 0
        if (params->rescaleSlope == 0.0f) {
            return XPE_ERR_INVALID_INPUT;
        }
        // REQ-DISP-001: output[i] = input[i] * slope + intercept
        const float slope     = params->rescaleSlope;
        const float intercept = params->rescaleIntercept;
        for (size_t i = 0; i < count; ++i) {
            px[i] = px[i] * slope + intercept;
        }
    } else if (params->mode == XPE_MODALITY_LUT_TABLE) {
        // REQ-DISP-005: lutData must not be NULL
        if (!params->lutData) {
            return XPE_ERR_INVALID_INPUT;
        }
        // REQ-DISP-006: lutLength must be > 0
        if (params->lutLength == 0) {
            return XPE_ERR_INVALID_INPUT;
        }
        // REQ-DISP-002: output[i] = lutData[clamp(round(input[i]) - firstMapped, 0, len-1)]
        const uint16_t* lut        = params->lutData;
        const int32_t   len        = static_cast<int32_t>(params->lutLength);
        const int32_t   firstMapped = params->lutFirstMapped;

        for (size_t i = 0; i < count; ++i) {
            int32_t idx = xpe_round_to_int(px[i]) - firstMapped;
            idx = xpe_clamp(idx, 0, len - 1);
            px[i] = static_cast<float>(lut[idx]);
        }
    } else {
        return XPE_ERR_INVALID_INPUT;
    }

    return XPE_OK;
}
