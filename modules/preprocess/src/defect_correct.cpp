/**
 * @file defect_correct.cpp
 * @brief SWU-1.3: Defect pixel correction and runtime detection (PRE-06)
 *        REQ-P1A-024 to REQ-P1A-028
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cmath>
#include <cstring>
#include <vector>

// @MX:ANCHOR: [AUTO] xpe_defect_correct — float32 in-place defect replacement
// @MX:REASON: Called in main pipeline after gain correction; fan_in >= 3
// @MX:SPEC: REQ-P1A-024
XpeErrorCode xpe_defect_correct(XpeImageBuffer* img,
                                 const XpeImageBuffer* defectMap,
                                 const char* configJsonOrNull)
{
    if (!img || !defectMap) return XPE_ERR_INVALID_INPUT;
    if (!xpe_dims_match(img, defectMap)) return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, &n)) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(defectMap, XPE_PIXEL_UINT8)) return XPE_ERR_INVALID_INPUT;

    const uint32_t W = img->width;
    const uint32_t H = img->height;
    auto*       px = static_cast<float*>(img->data);
    const auto* dm = static_cast<const uint8_t*>(defectMap->data);

    bool hasDefects = false;
    for (size_t i = 0; i < n; ++i) {
        if (dm[i] != 0) {
            hasDefects = true;
            break;
        }
    }
    if (!hasDefects) return XPE_OK;

    std::vector<float> source;
    try {
        source.assign(px, px + n);
    } catch (...) {
        return XPE_ERR_OUT_OF_MEMORY;
    }

    // REQ-P1A-024/025/027/028: iterate defect map and replace defective pixels
    // using edge-aware neighbor interpolation (xpe_interpolate_pixel handles
    // fallback to diagonals when all 4-connected neighbors are also defective)
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            if (dm[y * W + x] != 0) {
                px[y * W + x] = xpe_interpolate_pixel(source.data(), dm, x, y, W, H);
            }
        }
    }

    (void)configJsonOrNull;
    return XPE_OK;
}

// @MX:NOTE: [AUTO] Runtime transient defect detection via mean+3sigma statistical outlier analysis
// @MX:SPEC: REQ-P1A-028
XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* img,
                                        XpeImageBuffer* defectMapOut,
                                        const char* configJsonOrNull)
{
    if (!img || !defectMapOut) return XPE_ERR_INVALID_INPUT;
    if (!xpe_dims_match(img, defectMapOut)) return XPE_ERR_INVALID_INPUT;

    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, &n)) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(defectMapOut, XPE_PIXEL_UINT8)) return XPE_ERR_INVALID_INPUT;
    const auto*  px = static_cast<const float*>(img->data);
    auto*        out = static_cast<uint8_t*>(defectMapOut->data);
    std::memset(out, 0, n);

    // Compute mean
    double mean = 0.0;
    size_t finiteCount = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::isfinite(px[i])) {
            mean += px[i];
            ++finiteCount;
        } else {
            out[i] = 1u;
        }
    }
    if (finiteCount == 0) return XPE_ERR_PROCESSING_FAILED;
    mean /= static_cast<double>(finiteCount);

    // Compute standard deviation
    double var = 0.0;
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(px[i])) continue;
        const double d = static_cast<double>(px[i]) - mean;
        var += d * d;
    }
    const double sigma = std::sqrt(var / static_cast<double>(finiteCount));
    if (sigma == 0.0) return XPE_OK;

    // Flag pixels beyond mean ± 3*sigma as defective
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(px[i])) continue;
        out[i] = (std::abs(static_cast<double>(px[i]) - mean) > 3.0 * sigma) ? 1u : 0u;
    }

    (void)configJsonOrNull;
    return XPE_OK;
}
