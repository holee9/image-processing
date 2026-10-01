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
    // (FLOAT32 only; the non-finite check below is REQ-P1A-091)
    auto* px = static_cast<float*>(img->data);
    const float norm = 1.0f / static_cast<float>(binningMode * binningMode);
    for (size_t i = 0; i < n; ++i) {
        px[i] *= norm;
        if (!std::isfinite(px[i])) return XPE_ERR_PROCESSING_FAILED;
    }

    (void)configJsonOrNull;
    return XPE_OK;
}
