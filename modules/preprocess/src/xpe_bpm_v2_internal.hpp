/**
 * @file xpe_bpm_v2_internal.hpp
 * @brief The two local-median helpers of the second-generation detector, exposed for the unit tests (QA-A-252j)
 *
 * The pixel noise check forms its local expectation with a separable median (rows, then columns). Since QA-A-252i the pixels of the detector's own line mask are left out of it
 * (separableMedianExcluding). These wrappers carry the helpers across the translation unit so that the exceptions of the exclusion can be tested directly:
 *   - nothing excluded: the result is bit-identical to the plain median;
 *   - a window with no admissible pixel: the pixel takes the value of the plain median (`plain`).
 * They are not part of the public API.
 */

#ifndef XPE_BPM_V2_INTERNAL_HPP
#define XPE_BPM_V2_INTERNAL_HPP

#include "xpe/preprocess_api.h"

#include <cstdint>

namespace xpe_bpm_v2_internal {

/** One median job: the planes (w x h floats, `excl` bytes with 1 = left out) and the window. `plain` is read by the excluding median as the fallback value. */
struct MedianPlanes {
    const float*   in;
    float*         tmp;
    float*         out;
    const float*   plain;
    const uint8_t* excl;
    int            w;
    int            h;
    int            win;
};

/** The plain separable median of `in` into `out` (uses `tmp`). */
XPE_API void median_plain(const MedianPlanes& m);

/** The separable median of `in` without the pixels where `excl` is 1; a pixel whose window holds none takes `plain` at that pixel. */
XPE_API void median_excluding(const MedianPlanes& m);

/** The weighted sums of one pixel over the dose groups: w_g = m_g / v0_g (m_g frames, v0_g the nominal temporal variance), x_g the group signal, n_g the group mean. */
struct LofSums {
    double w0;    //!< sum of w_g
    double w1;    //!< sum of w_g x_g
    double w2;    //!< sum of w_g x_g^2
    double wn;    //!< sum of w_g n_g
    double wxn;   //!< sum of w_g x_g n_g
    double wnn;   //!< sum of w_g n_g^2
};

/**
 * The weighted residual sum of squares Q' of the straight line n = a x + b through the group means (QA-A-258, the lack of fit of the line detector's LINEARITY cause), from the weighted sums
 * alone (normal equations). `ok` is false, and the result 0, when the doses are degenerate (the determinant is not positive) or a sum is not finite. A negative round-off result is cut to 0.
 */
XPE_API double weighted_lack_of_fit(const LofSums& s, bool* ok);

/**
 * The feature of a pixel: ln(Q' / (gEff - 2) + 1e-9) - ln(max(phi, phiMin)), Q' the weighted residual sum of squares, gEff the number of groups with a pure error, phi the pixel's pooled temporal
 * variance in units of the nominal variance. 0 (no evidence of curvature) when phi is not finite or not positive, or Q' is not finite.
 */
XPE_API float lack_of_fit_feature(double qprime, uint32_t gEff, double phi, double phiMin);

}  // namespace xpe_bpm_v2_internal

#endif
