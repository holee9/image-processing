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
    if (normFactor <= 0.0f) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    float* px = float_pixels(img);
    const uint64_t n = static_cast<uint64_t>(img->width) * img->height;

    // REQ-ENH-001: output[i] = normFactor * log10(input[i] + 1.0)
    // REQ-ENH-002: clamp negative pixels to 0 before log
    // Perf: log10(x) = log(x) * (1/ln10). Precompute scale = normFactor/ln(10).
    const float scale_fwd = normFactor / std::log(10.0f);
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
    if (normFactor <= 0.0f) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    float* px = float_pixels(img);
    const uint64_t n = static_cast<uint64_t>(img->width) * img->height;

    // REQ-ENH-004: output[i] = pow(10.0, input[i] / normFactor) - 1.0
    // Perf: 10^x = exp(x*ln10). exp() is ~4x faster than pow() on MSVC.
    const float scale_inv = std::log(10.0f) / normFactor;
    for (uint64_t i = 0; i < n; ++i) {
        px[i] = std::exp(px[i] * scale_inv) - 1.0f;
    }

    return XPE_OK;
}

} // extern "C"
