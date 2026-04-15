/**
 * @file binning_correct.cpp
 * @brief SWU-1.8: Binning correction for gain/uniformity differences (PRE-09)
 *        No-op when binningMode == 1 (1x1, no binning).
 *        REQ-P1A-020 to REQ-P1A-023
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

// @MX:NOTE: [AUTO] No-op for 1x1 binning mode; valid modes: 1, 2, 4. Scales float32 pixels by 1/mode^2
// @MX:SPEC: REQ-P1A-020
XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                  int32_t binningMode,
                                  const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-020: no-op for binningMode == 1
    if (binningMode == 1) return XPE_OK;

    // REQ-P1A-021: XPE_ERR_CONFIG_INVALID for unknown binning mode
    // REQ-P1A-022: float32 format (post-gain-correct stage)
    // REQ-P1A-023: per-mode correction profile
    if (binningMode != 2 && binningMode != 4)
        return XPE_ERR_CONFIG_INVALID;

    // REQ-P1A-022/023: normalize by binningMode^2 to compensate for summed charge
    // float32 format (post-gain-correct stage)
    const size_t n = static_cast<size_t>(img->width) * img->height;
    auto* px = static_cast<float*>(img->pixels);
    const float norm = 1.0f / static_cast<float>(binningMode * binningMode);
    for (size_t i = 0; i < n; ++i) px[i] *= norm;

    (void)configJsonOrNull;
    return XPE_OK;
}
