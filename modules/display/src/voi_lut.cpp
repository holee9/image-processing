/**
 * @file voi_lut.cpp
 * @brief VOI LUT windowing implementation (SWU-3.2).
 * REQ-DISP-009 to REQ-DISP-018
 * SPEC: SPEC-XPE-P1B-DISP
 */

#include "xpe/display/display_api.h"
#include "xpe/display/display_internal.h"

#include <cmath>
#include <algorithm>

// @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
// @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
// @MX:SPEC: SPEC-XPE-P1B-DISP
extern "C" XpeErrorCode xpe_apply_voi_lut(XpeImageBuffer*        img,
                                            const XpeVoiLutParams* params) {
    if (!img)    return XPE_ERR_INVALID_INPUT;
    if (!params) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode fmt_rc = xpe_validate_float32(img);
    if (fmt_rc != XPE_OK) return fmt_rc;

    // REQ-DISP-015: width must be > 0
    if (params->width <= 0.0f) return XPE_ERR_INVALID_INPUT;

    const size_t count  = xpe_pixel_count(img);
    float* px           = static_cast<float*>(img->data);
    const float center  = params->center;
    const float width   = params->width;
    const float minOut  = params->minOut;
    const float maxOut  = params->maxOut;
    const float range   = maxOut - minOut;

    switch (params->mode) {
        case XPE_VOI_LINEAR: {
            // REQ-DISP-009: output[i] = clamp((input[i] - (center - width/2)) / width * range + minOut, minOut, maxOut)
            const float lo = center - width * 0.5f;
            for (size_t i = 0; i < count; ++i) {
                float val = (px[i] - lo) / width * range + minOut;
                px[i] = xpe_clamp(val, minOut, maxOut);
            }
            break;
        }
        case XPE_VOI_LINEAR_EXACT: {
            // REQ-DISP-011: DICOM PS3.3 C.11.2.1.3
            // output = clamp(((input - center) / width + 0.5) * range + minOut, minOut, maxOut)
            for (size_t i = 0; i < count; ++i) {
                float val = ((px[i] - center) / width + 0.5f) * range + minOut;
                px[i] = xpe_clamp(val, minOut, maxOut);
            }
            break;
        }
        case XPE_VOI_SIGMOID: {
            // REQ-DISP-012: output[i] = range / (1 + exp(-4*(input[i]-center)/width)) + minOut
            for (size_t i = 0; i < count; ++i) {
                float val = range / (1.0f + std::expf(-4.0f * (px[i] - center) / width)) + minOut;
                px[i] = xpe_clamp(val, minOut, maxOut);
            }
            break;
        }
        default:
            return XPE_ERR_INVALID_INPUT;
    }

    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
// @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
// @MX:SPEC: SPEC-XPE-P1B-DISP
extern "C" XpeErrorCode xpe_voi_preset_create(XpeVoiLutParams* params,
                                                XpeBodyPart      bodyPart) {
    if (!params) return XPE_ERR_INVALID_INPUT;

    // REQ-DISP-017: preset center/width values
    switch (bodyPart) {
        case XPE_BODY_BONE:
            params->mode   = XPE_VOI_LINEAR;
            params->center = 500.0f;
            params->width  = 2000.0f;
            params->minOut = 0.0f;
            params->maxOut = 255.0f;
            break;
        case XPE_BODY_LUNG:
            params->mode   = XPE_VOI_LINEAR;
            params->center = -600.0f;
            params->width  = 1600.0f;
            params->minOut = 0.0f;
            params->maxOut = 255.0f;
            break;
        case XPE_BODY_ABDOMEN:
            params->mode   = XPE_VOI_LINEAR;
            params->center = 40.0f;
            params->width  = 400.0f;
            params->minOut = 0.0f;
            params->maxOut = 255.0f;
            break;
        case XPE_BODY_HEAD:
            params->mode   = XPE_VOI_LINEAR;
            params->center = 40.0f;
            params->width  = 80.0f;
            params->minOut = 0.0f;
            params->maxOut = 255.0f;
            break;
        default:
            // REQ-DISP-018: invalid bodyPart
            return XPE_ERR_INVALID_INPUT;
    }

    return XPE_OK;
}
