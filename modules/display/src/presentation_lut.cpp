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
// @MX:NOTE: [AUTO] DICOM PS3.14 GSDF — Eq 7-2 (j from L) and Eq 7-1 (L from j)
// @MX:WARN: [AUTO] Numerical precision sensitive — validate with DICOM PS3.14 test vectors
// @MX:REASON: Coefficients are transcribed from the standard and checked against Table B-1
extern "C" XpeErrorCode xpe_gsdf_calibrate(const float*              luminanceValues,
                                             uint32_t                  count,
                                             XpePresentationLutParams* outParams) {
    if (!luminanceValues) return XPE_ERR_INVALID_INPUT;
    if (!outParams)       return XPE_ERR_INVALID_INPUT;
    if (count < 2)        return XPE_ERR_INVALID_INPUT;

    // REQ-DISP-026 + REQ-DISP-029 (#155, QA-B-146): the array is the display's
    // characteristic curve, so it must be non-decreasing. Until QA-B-145 the
    // order carried no meaning -- only the minimum and maximum were read -- and
    // an unordered array was a legitimate input. It is now a contract
    // violation, and this returns the error REQ-DISP-026 already defines rather
    // than inventing a new code.
    //
    // ONLY HALF OF REQ-DISP-029 IS CHECKABLE HERE, and the half that is not
    // must stay written down rather than assumed:
    //   - non-decreasing        -- checkable, it is a property of the array;
    //   - measured at EQUALLY SPACED driving levels -- NOT checkable, because
    //     no driving level reaches this function. A log-spaced ladder like
    //     {0.05, 1, 10, 100, 400} ascends and passes here while still being
    //     the wrong input.
    // Do not read this guard as "the contract is now enforced".
    //
    // non-decreasing, NOT strictly increasing: a real panel can have a flat
    // stretch, so values[i] == values[i+1] is allowed (<=, not <). What the
    // inversion does on such a plateau is decided and stated below.
    for (uint32_t i = 1; i < count; ++i) {
        if (luminanceValues[i] < luminanceValues[i - 1]) {
            return XPE_ERR_INVALID_INPUT;
        }
    }

    // REQ-DISP-025/026: compute a GSDF-compliant Presentation LUT.
    //
    // #155 (QA-B-145, stage 2). THE ARRAY IS NOW A CURVE, NOT A PAIR OF
    // ENDPOINTS. luminanceValues[i] is the luminance measured at the driving
    // level DDL_i = i/(count-1) * 65535, ascending. That horizontal axis is the
    // contract this function needs and did not previously have: stage 1 left
    // the standard's model computed and then cancelled out of the answer
    // because there was nothing to invert it against.
    //
    // Algorithm, with the step that was missing named:
    //   1. Range = the two ends of the measured curve.
    //   2. j_min = j(L_min), j_max = j(L_max)          -- Equation 7-2.
    //   3. Equal steps in JND index across [j_min, j_max], 1024 of them.
    //      That is what "P-Value steps are perceptually equal" means.
    //   4. L_target = L(j)                             -- Equation 7-1. THIS IS
    //      THE LINK THAT WAS ABSENT; without it step 3 undoes itself.
    //   5. DDL = the driving level whose MEASURED luminance is L_target, found
    //      by inverting the input array (linear interpolation between samples).
    //   6. Monotonically non-decreasing output.
    //
    // Verified without hardware: feeding a synthetic characteristic curve whose
    // answer is known analytically (gamma 2.2 and gamma 1.8 in
    // test_gsdf_characterization.cpp) and checking the module reproduces it.
    // A real measurement is not required to know the right answer -- an
    // independently derived control is (QA-B-142 used Table B-1 the same way).

    // Step 1: the ends of the measured curve. The contract says ascending, so
    // these are element 0 and element count-1; a caller that violates it is
    // caught here rather than silently producing an inverted ramp.
    float lum_min = luminanceValues[0];
    float lum_max = luminanceValues[count - 1];

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
    // STAGE 1 CHANGED NO OUTPUT: this expression cancelled out of the LUT
    // (#155). Stage 2 below makes it reach the answer, by inverting it through
    // Equation 7-1 and the measured curve instead of through itself. The test
    // that asserted the ramp is now inverted rather than deleted
    // (Stage2_LutIsNoLongerTheStraightRamp_155), so the diff reads "was a ramp
    // -> is not" in one line.
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

    // DICOM PS3.14 Equation 7-1, L(j): the inverse of the expression above, and
    // the step stage 1 did not have. Rational function in ln(j), evaluated in
    // double precision as the standard recommends. Its agreement with all ten
    // published Table B-1 points, and its round trip against Equation 7-2, are
    // asserted in test_gsdf_characterization.cpp -- the coefficients are not
    // trusted because they were transcribed, they are checked.
    auto luminance_from_jnd = [](double j) -> double {
        const double x  = std::log(j);
        const double x2 = x * x, x3 = x2 * x, x4 = x3 * x, x5 = x4 * x;
        const double num = -1.3011877 + 8.0242636e-2 * x + 1.3646699e-1 * x2
                         + -2.5468404e-2 * x3 + 1.3635334e-3 * x4;
        const double den = 1.0 + -2.5840191e-2 * x + -1.0320229e-1 * x2
                         + 2.8745620e-2 * x3 + -3.1978977e-3 * x4
                         + 1.2992634e-4 * x5;
        return std::pow(10.0, num / den);
    };

    float jnd_min = jnd_from_log10l(std::log10f(lum_min));
    float jnd_max = jnd_from_log10l(std::log10f(lum_max));

    if (jnd_max <= jnd_min) jnd_max = jnd_min + 1.0f;

    // Steps 3-5: equal JND steps -> required luminance -> the driving level the
    // MEASURED curve says produces it.
    const float jnd_range = jnd_max - jnd_min;

    uint16_t prev = 0;
    for (int i = 0; i < 1024; ++i) {
        // Step 3: target JND index, linearly distributed across the range.
        const double target_jnd = static_cast<double>(jnd_min)
            + (static_cast<double>(i) / 1023.0) * static_cast<double>(jnd_range);

        // Step 4: the luminance the standard requires at this P-Value.
        double target_lum = luminance_from_jnd(target_jnd);
        if (target_lum < lum_min) target_lum = lum_min;
        if (target_lum > lum_max) target_lum = lum_max;

        // Step 5: invert the measured curve. luminanceValues[k] was measured at
        // DDL_k = k/(count-1) * 65535, so locating target_lum between two
        // samples locates the driving level between their two DDLs.
        //
        // PLATEAU RULE (#155, QA-B-146). The curve is non-decreasing, not
        // strictly increasing, so several driving levels can share one
        // luminance and the inverse is a range rather than a point. This picks
        // the LOWEST driving level of that range: the search stops at the first
        // sample whose successor reaches target_lum, and a zero-width interval
        // contributes frac = 0. Lowest is the deterministic choice and the
        // conservative one -- it is the least drive that meets the luminance
        // the standard asks for. It is a CHOICE, not a consequence: highest, or
        // the midpoint, would satisfy the standard equally well, which is why
        // it is written here instead of being left to whoever reads the loop.
        double ddl_f;
        if (target_lum <= static_cast<double>(luminanceValues[0])) {
            ddl_f = 0.0;
        } else if (target_lum >= static_cast<double>(luminanceValues[count - 1])) {
            ddl_f = 65535.0;
        } else {
            uint32_t k = 0;
            while (k + 2 < count &&
                   static_cast<double>(luminanceValues[k + 1]) < target_lum) {
                ++k;
            }
            const double lo = static_cast<double>(luminanceValues[k]);
            const double hi = static_cast<double>(luminanceValues[k + 1]);
            const double frac = (hi > lo) ? (target_lum - lo) / (hi - lo) : 0.0;
            ddl_f = ((static_cast<double>(k) + frac) /
                     static_cast<double>(count - 1)) * 65535.0;
        }

        uint16_t ddl = static_cast<uint16_t>(
            xpe_clamp(static_cast<int32_t>(std::lround(ddl_f)), 0, 65535));

        // Step 6: monotonically non-decreasing
        if (ddl < prev) ddl = prev;
        outParams->lutData[i] = ddl;
        prev = ddl;
    }

    // REQ-DISP-028: set gsdfEnabled flag
    outParams->gsdfEnabled = 1;

    return XPE_OK;
}
