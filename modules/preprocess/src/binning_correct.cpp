/**
 * @file binning_correct.cpp
 * @brief SWU-1.8: Binning correction for gain/uniformity differences (PRE-09)
 *        No-op when binningMode == 1 (1x1, no binning).
 *        REQ-P1A-090, REQ-P1A-091
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cmath>

// @MX:NOTE: [AUTO] No-op for 1x1 binning mode; valid modes: 1, 2, 4. Scales float32 pixels by 1/mode^2
// @MX:SPEC: REQ-P1A-090, REQ-P1A-091
XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                  int32_t binningMode,
                                  const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, &n)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-091: no-op for binningMode == 1
    if (binningMode == 1) return XPE_OK;

    // REQ-P1A-091: XPE_ERR_CONFIG_INVALID for a mode other than 1, 2 or 4
    // (the FLOAT32-only check is above: REQ-P1A-090; the stage runs after
    // gain correction: REQ-P1A-095)
    if (binningMode != 2 && binningMode != 4)
        return XPE_ERR_CONFIG_INVALID;

    // REQ-P1A-090: normalize by 1/binningMode^2 to compensate for summed charge
    // (FLOAT32 only; the non-finite handling below is REQ-P1A-091)
    auto* px = static_cast<float*>(img->data);

    // QA-A-215 (#233): A NON-FINITE FRAME IS REFUSED BEFORE ANYTHING IS WRITTEN. The scaling used to be checked after
    // each pixel was multiplied, so a NaN or an infinity in the middle of the frame answered XPE_ERR_PROCESSING_FAILED
    // with every pixel before it already divided -- a failure code and a half-changed buffer. The check is on the INPUT
    // because that is the only way this stage can produce a non-finite value: the factor is 1/4 or 1/16, which can
    // lower a finite float (to a denormal or to zero, both finite) but never raise it past the largest float. So a
    // non-finite result needs a non-finite input, and an input a consumer cannot process is INVALID_INPUT, with the
    // buffer left as it was (QA-A-214b's rule for the defect stage). The pipeline never hands this stage such a frame
    // (its input is the gain stage's finite output), so the pipeline's results are unchanged.
    {
        size_t count = 0, first = 0;
        if (xpe_find_nonfinite(px, n, &count, &first)) {
            xpe_alert_nonfinite("XPE_WARN_BINNING_INPUT_NOT_FINITE:", count, first, img->width,
                                "the frame was not binned and the buffer was not changed");
            return XPE_ERR_INVALID_INPUT;
        }
    }
    const float norm = 1.0f / static_cast<float>(binningMode * binningMode);
    for (size_t i = 0; i < n; ++i) px[i] *= norm;

    (void)configJsonOrNull;
    return XPE_OK;
}
