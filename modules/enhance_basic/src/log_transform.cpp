// SWU-2.1: Log Transform and Inverse
// SPEC-XPE-P1B-ENH  REQ-ENH-001..006

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

#include <cmath>

extern "C" {

// @MX:ANCHOR: xpe_log_transform converts detector-domain pixels to log scale.
// @MX:REASON: [AUTO] Public API boundary, called by pipeline and C# orchestrator. REQ-ENH-001..003.
XPE_API XpeErrorCode xpe_log_transform(XpeImageBuffer* img, float normFactor)
{
    // REQ-ENH-003: normFactor must be positive (validate before image)
    // QA-B-181f: and finite, by a bit test. `normFactor <= 0` is false for +inf in every floating-point mode and for
    // NaN under /fp:precise, so those used to be accepted.
    if (!xpe_float_is_finite(normFactor) || normFactor <= 0.0f) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    float* px = float_pixels(img);
    const uint64_t n = static_cast<uint64_t>(img->width) * img->height;

    // REQ-ENH-001: output[i] = normFactor * log10(input[i] + 1.0)
    // REQ-ENH-002: clamp negative pixels to 0 before log
    // Perf: log10(x) = log(x) * (1/ln10). Precompute scale = normFactor/ln(10).
    const float scale_fwd = normFactor / std::log(10.0f);
    // QA-B-181f (#233): a non-finite pixel is refused, and so is a request whose result would leave float (a finite
    // but enormous normFactor); nothing is written. The result is largest at the largest pixel, so one test of that
    // pixel covers the image. Zeroing a bad pixel was not chosen: it would make an upstream fault look like data.
    {
        float lo, hi;
        if (!xpe_scan_finite(px, n, &lo, &hi)) return XPE_ERR_INVALID_INPUT;
        const float top = scale_fwd * std::log((hi < 0.0f ? 0.0f : hi) + 1.0f);
        if (!xpe_float_is_finite(scale_fwd) || !xpe_float_is_finite(top)) return XPE_ERR_INVALID_INPUT;
    }
    for (uint64_t i = 0; i < n; ++i) {
        float v = px[i] < 0.0f ? 0.0f : px[i];
        px[i] = scale_fwd * std::log(v + 1.0f);
    }

    return XPE_OK;
}

// @MX:ANCHOR: xpe_log_inverse converts log-domain pixels back to detector domain.
// @MX:REASON: [AUTO] Public API boundary, inverse of xpe_log_transform. REQ-ENH-004..005.
XPE_API XpeErrorCode xpe_log_inverse(XpeImageBuffer* img, float normFactor)
{
    // REQ-ENH-005: normFactor must be positive
    // QA-B-181f: and finite -- see xpe_log_transform. +inf made scale_inv 0, so the whole image became exp(0) - 1 = 0
    // with rc=0.
    if (!xpe_float_is_finite(normFactor) || normFactor <= 0.0f) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    float* px = float_pixels(img);
    const uint64_t n = static_cast<uint64_t>(img->width) * img->height;

    // REQ-ENH-004: output[i] = pow(10.0, input[i] / normFactor) - 1.0
    // Perf: 10^x = exp(x*ln10). exp() is ~4x faster than pow() on MSVC.
    const float scale_inv = std::log(10.0f) / normFactor;
    // QA-B-181f (#233): exp(pixel * scale_inv) leaves float above about 88.7, so ONE finite pixel of 100 at
    // normFactor 1 became +inf (QA-B-181e), which the next stage then spread over the image with rc=0. A non-finite
    // pixel, or a request that would overflow, is refused before anything is written. exp is increasing, so the
    // largest pixel decides.
    {
        float lo, hi;
        if (!xpe_scan_finite(px, n, &lo, &hi)) return XPE_ERR_INVALID_INPUT;
        if (!xpe_float_is_finite(scale_inv) || !xpe_float_is_finite(std::exp(hi * scale_inv) - 1.0f)) {
            return XPE_ERR_INVALID_INPUT;
        }
    }
    for (uint64_t i = 0; i < n; ++i) {
        px[i] = std::exp(px[i] * scale_inv) - 1.0f;
    }

    return XPE_OK;
}

} // extern "C"
