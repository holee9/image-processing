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

}  // namespace xpe_bpm_v2_internal

#endif
