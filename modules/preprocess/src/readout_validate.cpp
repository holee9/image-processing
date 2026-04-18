/**
 * @file readout_validate.cpp
 * @brief SWU-1.9: Readout artifact validation (PRE-01)
 *        REQ-P1A-001 to REQ-P1A-004
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstring>
#include <cstdio>

// @MX:ANCHOR: [AUTO] xpe_validate_readout_artifact — first stage in pipeline
// @MX:REASON: Must be called before any correction stage; gate for entire pipeline
// @MX:SPEC: REQ-P1A-001
XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* rawImg,
                                            int32_t* artifactScoreOut,
                                            char* msgOut,
                                            size_t msgLen)
{
    if (!rawImg || !artifactScoreOut) return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(rawImg, XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-001: validate raw uint16 image for readout artifacts
    // REQ-P1A-002: line noise — rows where mean > 0.9 * UINT16_MAX
    // REQ-P1A-003: ADC saturation — fraction of pixels at max value (65535)
    // REQ-P1A-004: score > 80 posts WARNING but still returns XPE_OK

    const uint32_t W  = rawImg->width;
    const uint32_t H  = rawImg->height;
    const auto*    px = static_cast<const uint16_t*>(rawImg->data);

    // ADC saturation detection
    size_t saturated = 0;
    for (size_t i = 0; i < n; ++i)
        if (px[i] == 65535u) ++saturated;

    // Line noise detection: flag rows where mean > 0.9 * 65535
    static constexpr double kLineNoiseFrac = 0.9;
    uint32_t noisyRows = 0;
    for (uint32_t y = 0; y < H; ++y) {
        double rowSum = 0.0;
        for (uint32_t x = 0; x < W; ++x)
            rowSum += px[static_cast<size_t>(y) * W + x];
        if (W > 0 && (rowSum / W) > kLineNoiseFrac * 65535.0)
            ++noisyRows;
    }

    // Score: combine saturation and line noise contributions, clamp to [0, 100]
    const double satFrac   = (n > 0)  ? static_cast<double>(saturated) / static_cast<double>(n)  : 0.0;
    const double noiseFrac = (H > 0)  ? static_cast<double>(noisyRows) / static_cast<double>(H)  : 0.0;
    int32_t score = static_cast<int32_t>((satFrac + noiseFrac) * 50.0);
    if (score > 100) score = 100;
    if (score < 0)   score = 0;

    *artifactScoreOut = score;

    if (msgOut && msgLen > 0) {
        std::snprintf(msgOut, msgLen,
                      "score=%d sat=%.1f%% noisy_rows=%u",
                      score,
                      satFrac * 100.0,
                      noisyRows);
    }

    // REQ-P1A-004: always XPE_OK regardless of score
    return XPE_OK;
}
