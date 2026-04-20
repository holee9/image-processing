/**
 * @file readout_validate.cpp
 * @brief SWU-1.9: Readout artifact validation (PRE-01)
 *        REQ-P1A-001 to REQ-P1A-004
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstring>
#include <cstdio>

// @MX:ANCHOR: [AUTO] xpe_validate_readout_artifact — first stage in pipeline
// @MX:REASON: Must be called before any correction stage; gate for entire pipeline
// @MX:SPEC: REQ-P1A-001
extern "C" XPE_API XpeErrorCode xpe_validate_readout_artifact(
    const XpeImageBuffer* image,
    const XpeImageMetadata* metadata,
    bool* has_dropped_columns,
    bool* has_nonuniform_gain)
{
    if (!image || !metadata || !has_dropped_columns || !has_nonuniform_gain)
        return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(image, XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;

    const uint32_t W  = image->width;
    const uint32_t H  = image->height;
    const auto*    px = static_cast<const uint16_t*>(image->data);

    // REQ-P1A-002: detect dropped columns (column where all pixels are 0)
    bool dropped = false;
    for (uint32_t x = 0; x < W && !dropped; ++x) {
        bool allZero = true;
        for (uint32_t y = 0; y < H && allZero; ++y) {
            if (px[static_cast<size_t>(y) * W + x] != 0) allZero = false;
        }
        if (allZero) dropped = true;
    }

    // REQ-P1A-003: detect gain nonuniformity (rows where mean > 0.9 * UINT16_MAX)
    static constexpr double kLineNoiseFrac = 0.9;
    bool nonuniform = false;
    for (uint32_t y = 0; y < H && !nonuniform; ++y) {
        double rowSum = 0.0;
        for (uint32_t x = 0; x < W; ++x)
            rowSum += px[static_cast<size_t>(y) * W + x];
        if (W > 0 && (rowSum / W) > kLineNoiseFrac * 65535.0)
            nonuniform = true;
    }

    *has_dropped_columns = dropped;
    *has_nonuniform_gain = nonuniform;

    // REQ-P1A-004: always XPE_OK
    return XPE_OK;
}
