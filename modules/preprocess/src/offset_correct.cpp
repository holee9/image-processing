/**
 * @file offset_correct.cpp
 * @brief SWU-1.1: Per-pixel dark offset subtraction (PRE-02)
 *        REQ-P1A-009 to REQ-P1A-011
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <cstdint>

// @MX:ANCHOR: [AUTO] xpe_offset_correct — public API entry point
// @MX:REASON: Called by pipeline and directly by calibration manager; fan_in >= 3
// @MX:SPEC: REQ-P1A-009
XpeErrorCode xpe_offset_correct(XpeImageBuffer* img,
                                 const XpeImageBuffer* offsetMap)
{
    if (!img || !offsetMap) return XPE_ERR_INVALID_INPUT;
    if (!xpe_dims_match(img, offsetMap)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-009: corrected[i] = clamp(raw[i] - offsetMap[i], 0)
    // REQ-P1A-010: in-place, uint16 format
    // REQ-P1A-011: no pixel shall underflow below 0 — saturating subtract
    const size_t n = static_cast<size_t>(img->width) * img->height;
    auto*       dst = static_cast<uint16_t*>(img->pixels);
    const auto* off = static_cast<const uint16_t*>(offsetMap->pixels);
    for (size_t i = 0; i < n; ++i)
        dst[i] = (dst[i] > off[i]) ? static_cast<uint16_t>(dst[i] - off[i]) : uint16_t{0};
    return XPE_OK;
}
