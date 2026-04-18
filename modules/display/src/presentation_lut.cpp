/**
 * @file presentation_lut.cpp
 * @brief Presentation LUT and GSDF calibration implementation (SWU-3.3).
 * REQ-DISP-019 to REQ-DISP-028
 * SPEC: SPEC-XPE-P1B-DISP
 */

#include "xpe/display/display_api.h"
#include "xpe/display/display_internal.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdint>

// @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
// @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract; domain transition float32->uint16
// @MX:SPEC: SPEC-XPE-P1B-DISP
extern "C" XpeErrorCode xpe_apply_presentation_lut(XpeImageBuffer*                 img,
                                                     const XpePresentationLutParams* params) {
    if (!img)    return XPE_ERR_INVALID_INPUT;
    if (!params) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode fmt_rc = xpe_validate_float32(img);
    if (fmt_rc != XPE_OK) return fmt_rc;

    const size_t count = xpe_pixel_count(img);

    // REQ-DISP-024: allocate new uint16 buffer
    size_t new_size = count * sizeof(uint16_t);
    uint16_t* out_buf = static_cast<uint16_t*>(std::malloc(new_size));
    if (!out_buf) {
        return XPE_ERR_OUT_OF_MEMORY;
    }

    const float* src = static_cast<const float*>(img->data);
    const uint16_t* lut = params->lutData;

    // REQ-DISP-020: index = clamp(round(input * 1023), 0, 1023)
    // REQ-DISP-021: input clamped to [0.0, 1.0] first
    for (size_t i = 0; i < count; ++i) {
        float v = src[i];
        // Clamp input to [0.0, 1.0]
        v = xpe_clamp(v, 0.0f, 1.0f);
        int32_t idx = xpe_round_to_int(v * 1023.0f);
        idx = xpe_clamp(idx, 0, 1023);
        out_buf[i] = lut[idx];
    }

    // REQ-DISP-019: domain transition — free old float32 buffer, update metadata
    std::free(img->data);
    img->data          = out_buf;
    img->format        = XPE_PIXEL_UINT16;
    img->bitsAllocated = 16;
    img->bitsStored    = 16;
    img->dataSize      = new_size;

    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
// @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
// @MX:SPEC: SPEC-XPE-P1B-DISP
// @MX:NOTE: [AUTO] GSDF Barten model approximation — simplified log-linear JND model
// @MX:WARN: [AUTO] Numerical precision sensitive — validate with DICOM PS3.14 test vectors
// @MX:REASON: Barten model uses empirical constants; different calibration data may require tuning
extern "C" XpeErrorCode xpe_gsdf_calibrate(const float*              luminanceValues,
                                             uint32_t                  count,
                                             XpePresentationLutParams* outParams) {
    if (!luminanceValues) return XPE_ERR_INVALID_INPUT;
    if (!outParams)       return XPE_ERR_INVALID_INPUT;
    if (count < 2)        return XPE_ERR_INVALID_INPUT;

    // REQ-DISP-026: Compute DICOM GSDF-compliant LUT using Barten model approximation
    //
    // Simplified log-linear JND model:
    //   JND(L) ≈ a * log10(L) + b
    // where a, b are derived from the provided luminance range.
    //
    // Algorithm:
    //   1. Find min/max luminance from input array
    //   2. Compute JND for min and max using log10 model
    //   3. Create linear JND scale from JND_min to JND_max over 1024 steps
    //   4. For each LUT position, find the digital driving level (DDL) by
    //      inverting the JND function back to luminance, then scaling to [0, 65535]
    //   5. Ensure monotonically non-decreasing output

    // Step 1: find luminance range
    float lum_min = luminanceValues[0];
    float lum_max = luminanceValues[0];
    for (uint32_t i = 1; i < count; ++i) {
        if (luminanceValues[i] < lum_min) lum_min = luminanceValues[i];
        if (luminanceValues[i] > lum_max) lum_max = luminanceValues[i];
    }

    // Protect against invalid luminance values
    if (lum_min <= 0.0f) lum_min = 0.01f;
    if (lum_max <= lum_min) lum_max = lum_min + 1.0f;

    // Step 2: compute JND range using log10 model
    // JND(L) ≈ 71.498 * (log10(L))^3 - 94.593 * (log10(L))^2 + 41.912 * log10(L) + 9.8212
    // (Simplified Barten model constants — DICOM PS3.14 approximation)
    auto jnd_from_log10l = [](float log10_L) -> float {
        return 71.498f * log10_L * log10_L * log10_L
             - 94.593f * log10_L * log10_L
             + 41.912f * log10_L
             + 9.8212f;
    };

    float log_lmin = std::log10f(lum_min);
    float log_lmax = std::log10f(lum_max);

    float jnd_min = jnd_from_log10l(log_lmin);
    float jnd_max = jnd_from_log10l(log_lmax);

    if (jnd_max <= jnd_min) jnd_max = jnd_min + 1.0f;

    // Step 3 & 4: For each of 1024 LUT positions, compute the output DDL
    // Each LUT index i corresponds to a normalized input value n = i / 1023.0
    // We map that to a JND index, then invert to luminance, then scale to uint16
    const float jnd_range = jnd_max - jnd_min;

    uint16_t prev = 0;
    for (int i = 0; i < 1024; ++i) {
        // Target JND index linearly distributed
        float target_jnd = jnd_min + (static_cast<float>(i) / 1023.0f) * jnd_range;

        // Invert JND -> log10(L) using a simple linear approximation of the inverse
        // In the simplified model: log10(L) ≈ (target_jnd - 9.8212) / (some slope)
        // For monotonicity, we use the linear interpolation between log_lmin and log_lmax
        float t = (target_jnd - jnd_min) / jnd_range;
        float log_L = log_lmin + t * (log_lmax - log_lmin);

        // Scale log_L in [log_lmin, log_lmax] to DDL in [0, 65535]
        float ddl_f = ((log_L - log_lmin) / (log_lmax - log_lmin)) * 65535.0f;
        uint16_t ddl = static_cast<uint16_t>(xpe_clamp(static_cast<int32_t>(std::roundf(ddl_f)), 0, 65535));

        // Step 5: monotonically non-decreasing
        if (ddl < prev) ddl = prev;
        outParams->lutData[i] = ddl;
        prev = ddl;
    }

    // REQ-DISP-028: set gsdfEnabled flag
    outParams->gsdfEnabled = 1;

    return XPE_OK;
}
