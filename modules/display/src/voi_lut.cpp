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
static XpeErrorCode apply_voi_lut_impl(XpeImageBuffer*        img,
                                       const XpeVoiLutParams* params) {
    if (!img)    return XPE_ERR_INVALID_INPUT;
    if (!params) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode fmt_rc = xpe_validate_float32(img);
    if (fmt_rc != XPE_OK) return fmt_rc;

    // REQ-DISP-015: width must be > 0
    // QA-B-181f (#233): and every parameter finite, by a bit test. `width <= 0` is false for +inf in every mode and
    // for NaN under /fp:precise, and center / minOut / maxOut had no test at all, so a NaN anywhere in the window
    // made the whole output NaN with rc=0. The output range must not overflow either (-3e38 .. 3e38).
    if (!xpe_float_is_finite(params->center) || !xpe_float_is_finite(params->width) ||
        !xpe_float_is_finite(params->minOut) || !xpe_float_is_finite(params->maxOut) ||
        !xpe_float_is_finite(params->maxOut - params->minOut)) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (params->width <= 0.0f) return XPE_ERR_INVALID_INPUT;

    // QA-B-207 D3 (user decision on #251): PS3.3 C.11.2.1.2 says of the default LINEAR function "Window Width (0028,1051)
    // shall always be greater than or equal to 1", where LINEAR_EXACT (C.11.2.1.3.2) and SIGMOID (C.11.2.1.3.1) say "shall
    // always be greater than 0". A LINEAR width in (0, 1) makes `width - 1` negative, the lower threshold exceeds the upper
    // one and the function collapses to a step (QA-B-204: the SPEC formula runs the ramp backwards there). It is refused;
    // exactly 1 is legal and gives the standard's threshold at `center - 0.5` (no division happens: the continuous
    // segment is never reached, C.11.2.1.2 note 2).
    if (params->mode == XPE_VOI_LINEAR && params->width < 1.0f) return XPE_ERR_INVALID_INPUT;

    // QA-B-207 D8 (user decision on #251): the output window must be non-empty and ascending, minOut < maxOut. With
    // minOut > maxOut `clamp(x, minOut, maxOut)` has its bounds crossed and returned garbage with rc = 0 for all three modes
    // (QA-B-204: LINEAR_EXACT (1, 0) gave 0,0,1,1,1 instead of an inverted ramp; SIGMOID (1, 0) gave 1.0 everywhere), and
    // minOut == maxOut was a special case that wrote minOut everywhere. Both are refused. Nothing has been written yet.
    if (!(params->minOut < params->maxOut)) return XPE_ERR_INVALID_INPUT;

    const size_t count  = xpe_pixel_count(img);
    float* px           = static_cast<float*>(img->data);
    const float center  = params->center;
    const float width   = params->width;
    const float minOut  = params->minOut;
    const float maxOut  = params->maxOut;
    const float range   = maxOut - minOut;

    // QA-B-181f (#233): a non-finite pixel is refused, as in the other two LUT functions; nothing is written.
    if (!xpe_all_finite(px, count)) return XPE_ERR_INVALID_INPUT;

    // (QA-B-207 D8: minOut == maxOut, which QA-B-181f handled here as a special case so that `inf * 0` could not make a NaN,
    // is refused above, so `range` is positive from here on.)

    switch (params->mode) {
        case XPE_VOI_LINEAR: {
            // REQ-DISP-009: DICOM PS3.3 C.11.2.1.2.1 Default LINEAR. Windows
            // about `center - 0.5` over a width of `width - 1`:
            //
            //   clamp( ((x - (center - 0.5)) / (width - 1) + 0.5) * range + minOut, ... )
            //
            // #156 (QA-B-149). WHAT WAS HERE BEFORE, and why it was wrong:
            //
            //     const float lo = center - width * 0.5f;
            //     float val = (px[i] - lo) / width * range + minOut;
            //
            // That expands to ((x - center)/width + 0.5) * range + minOut --
            // character for character the LINEAR_EXACT branch below. The two
            // modes were algebraically identical (measured: 5.96e-08 apart on a
            // [0,1] output, one ulp), so selecting LINEAR_EXACT did nothing.
            //
            // THE MISSING PIECE IS THE WINDOW-PLACEMENT ADJUSTMENT, NOT THE
            // `+ 0.5`. #156's own text read the `+ 0.5` as the forbidden
            // "half-value offset", but that term is in BOTH standard functions
            // and only re-centres the normalized window onto [0,1]; removing it
            // would map the window to [-0.5, +0.5] and REQ-DISP-010's "maps
            // exactly from minOut to maxOut" would break. What separates the
            // two is `center - 0.5` / `width - 1`, which LINEAR had lost.
            // REQ-DISP-010a now says this in the SPEC so the next reader does
            // not have to re-derive it.
            //
            // EXACT was already correct and is NOT touched by this change.
            //
            // Three branches rather than one clamped expression, because
            // `width - 1` is zero at width == 1: the standard's thresholds
            // partition the line there (everything maps to minOut or maxOut)
            // and the interior division is never reached. REQ-DISP-015 only
            // rejects width <= 0, so width == 1 is a legal input and must not
            // divide by zero.
            const float c = center - 0.5f;
            const float w = width - 1.0f;
            const float lo = c - w * 0.5f;
            const float hi = c + w * 0.5f;
            for (size_t i = 0; i < count; ++i) {
                const float v = px[i];
                if (v <= lo) {
                    px[i] = minOut;
                } else if (v > hi) {
                    px[i] = maxOut;
                } else {
                    px[i] = xpe_clamp(((v - c) / w + 0.5f) * range + minOut, minOut, maxOut);
                }
            }
            break;
        }
        case XPE_VOI_LINEAR_EXACT: {
            // REQ-DISP-010: DICOM PS3.3 C.11.2.1.3
            // output = clamp(((input - center) / width + 0.5) * range + minOut, minOut, maxOut)
            for (size_t i = 0; i < count; ++i) {
                float val = ((px[i] - center) / width + 0.5f) * range + minOut;
                px[i] = xpe_clamp(val, minOut, maxOut);
            }
            break;
        }
        case XPE_VOI_SIGMOID: {
            // REQ-DISP-011: output[i] = range / (1 + exp(-4*(input[i]-center)/width)) + minOut
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
static XpeErrorCode voi_preset_create_impl(XpeVoiLutParams* params,
                                           XpeBodyPart      bodyPart) {
    if (!params) return XPE_ERR_INVALID_INPUT;

    // REQ-DISP-017 (revised 2026-09-17, #177): the presets act on detector DN,
    // not HU -- this product applies VOI to raw DN with an identity modality
    // LUT. Until per-body-part DN windows are derived from real detector data
    // (#151), every body part uses the full 16-bit window. The former CT HU
    // windows (e.g. Abdomen 40/400) crushed DN input to a single output level
    // (GUI-C-84). Do not pick per-part values from synthetic data (#148).
    switch (bodyPart) {
        case XPE_BODY_BONE:
        case XPE_BODY_LUNG:
        case XPE_BODY_ABDOMEN:
        case XPE_BODY_HEAD:
            params->mode   = XPE_VOI_LINEAR;
            params->center = 32768.0f;
            params->width  = 65535.0f;
            params->minOut = 0.0f;
            params->maxOut = 255.0f;
            break;
        default:
            // REQ-DISP-018: invalid bodyPart
            return XPE_ERR_INVALID_INPUT;
    }

    return XPE_OK;
}

// REQ-DISP-031 (QA-B-200 M2a): entry, outcome and error conditions are logged around the functions above.
extern "C" XpeErrorCode xpe_apply_voi_lut(XpeImageBuffer*        img,
                                            const XpeVoiLutParams* params) {
    xpe_display_log_enter("xpe_apply_voi_lut");
    return xpe_display_log_exit("xpe_apply_voi_lut", apply_voi_lut_impl(img, params));
}

extern "C" XpeErrorCode xpe_voi_preset_create(XpeVoiLutParams* params,
                                                XpeBodyPart      bodyPart) {
    xpe_display_log_enter("xpe_voi_preset_create");
    return xpe_display_log_exit("xpe_voi_preset_create", voi_preset_create_impl(params, bodyPart));
}
