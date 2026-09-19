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
    //
    // #142 D5: this frees a buffer xpe_common allocated and installs one this
    // module allocated, so ownership crosses the DLL boundary in both
    // directions. Measured safe because both DLLs link the shared UCRT --
    // dumpbin shows VCRUNTIME140.dll + api-ms-win-crt-heap-l1-1-0.dll for
    // xpe_common.dll and xpe_display.dll alike (QA-B-40, _d5_crt.log), which is
    // CMake's MSVC default (MultiThreadedDLL) since no MSVC_RUNTIME_LIBRARY is
    // set anywhere in the project. A static-CRT switch would break this line
    // silently; PresentationLutCrossDllTest is the standing check.
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

    // Step 2: compute the JND range -- DICOM PS3.14 Equation 7-2, j(L).
    //
    //   j = A + B*y + C*y^2 + D*y^3 + E*y^4 + F*y^5 + G*y^6 + H*y^7 + I*y^8,
    //   y = log10(L)
    //
    // #155 (QA-B-143, stage 1). WHAT WAS HERE BEFORE, and why it was wrong:
    //
    //     71.498*y^3 - 94.593*y^2 + 41.912*y + 9.8212
    //
    // called "Simplified Barten model constants" in its own comment. It is not
    // a simplification. Those four numbers ARE Equation 7-2's A, B, C, D -- in
    // REVERSED positional order with the signs alternated. A is the standard's
    // CONSTANT term and the old code used it on y^3; D is the standard's y^3
    // term and the old code used it as the constant. The cleanest proof is
    // L = 1 cd/m^2, where y = 0 and every power term vanishes, so both reduce
    // to their constants: the standard gives j = A = 71.50, the old code gave
    // 9.82. Table B-1 has L(70) = 0.9640 and L(71) ~ 0.99, so 71.5 is right.
    //
    // NOT VERIFIED, and recorded rather than assumed away: the equations
    // themselves are IMAGES in the standard's HTML, so only the coefficients
    // could be read as text. This polynomial FORM is supported by three checks
    // in test_gsdf_characterization.cpp -- all ten published Table B-1 points,
    // a round trip against Equation 7-1, and the stated 4000 cd/m^2 ceiling --
    // NOT by having read the printed equation. Whether some source deliberately
    // uses the reversed arrangement was not researched; four coefficients
    // matching is not a coincidence, but intent was not established.
    //
    // https://dicom.nema.org/medical/dicom/current/output/chtml/part14/chapter_7.html
    //
    // STAGE 1 CHANGES NO OUTPUT. This expression cancels out of the LUT below
    // (#155), so the produced bytes are identical before and after; that
    // identity is asserted by Stage1_LutBytesAreUnchangedByTheCoefficientFix_155
    // and is the evidence that the expression really does not reach the answer.
    // Making it reach the answer is a separate, later change.
    auto jnd_from_log10l = [](float log10_L) -> float {
        const double y = static_cast<double>(log10_L);
        double p = -0.017046845;      // I, y^8
        p = p * y + 0.14710899;       // H, y^7
        p = p * y + -0.18014349;      // G, y^6
        p = p * y + -1.1878455;       // F, y^5
        p = p * y + 0.28175407;       // E, y^4
        p = p * y + 9.8247004;        // D, y^3
        p = p * y + 41.912053;        // C, y^2
        p = p * y + 94.593053;        // B, y^1
        p = p * y + 71.498068;        // A, constant
        return static_cast<float>(p);
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
