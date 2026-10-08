/**
 * @file voi_auto_window.cpp
 * @brief Anatomy-based automatic VOI window (SWU-3.2, QA-B-214 / 214b / 214c, user decision on #251).
 * SPEC: SPEC-XPE-P1B-DISP
 */

#include "xpe/display/display_api.h"
#include "xpe/display/display_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// Named parameters. Every value was chosen on the real wrist frame of the stage-1 reference image (m1-baseline-v2, 3072 x
// 3072, after log -> bilateral -> CLAHE -> USM, 1000 * log10(ADU + 1) domain) and the other real frames of the repository;
// the numbers behind each are in .moai/reports/lane-post/QA-B-214/report.md. One anatomy frame is a small base: a second
// body part may move them, which is why they are constants here and not literals in the code.
//
// QA-B-214c (Codex #168, the third hold on this function): EVERY pixel is histogrammed. The 1/16 sample of 214, and the
// representativeness test and re-confirmation 214b put around it, were removed: a fixed sampling grid cannot see a
// difference between the sampled pixels and the rest whose values stay inside the sample value range (a detector grid or a
// moire has a period of 4 pixels), and every defence added to the sample invited the next counter-example. What is read is
// what the window is made of.
// ---------------------------------------------------------------------------------------------------------------------

// 1024 bins between the minimum and the maximum. Bin width is 0.1 % of the range (1.6 log units on the wrist frame); the
// window ends changed by under 1 log unit between 256 and 4096 bins.
constexpr int32_t kAutoWindowHistogramBins = 1024;

// Window low end = this quantile of the anatomy class. 0.5 % leaves the densest bone inside the window: at 1 % the window
// start clipped 0.98 % of the anatomy pixels, at 0.5 % it clips 0.50 %, at 0.1 % 0.10 % (p05-p95 spread 0.648 against 0.662
// of the window).
constexpr double kAutoWindowAnatomyLowQuantile = 0.005;

// Window high end = this quantile of the BACKGROUND class (the high class of the Otsu split). The Otsu threshold sits in the
// valley between tissue and air, so cutting the window there clipped the whole tissue-to-air transition at the skin line
// (a ring of 44 px around the anatomy: 100 % of it above the window). At the 5 % quantile of the background class 9.9 % of
// that ring is above the window, at 1 % it is 53 %, at the median 1.4 %; the contrast of the anatomy (p05-p95 of the window)
// falls from 0.871 (threshold) through 0.672 (5 %) to 0.576 (median). 5 % is the point that keeps nearly all of the skin line
// and still leaves two thirds of the output range to the anatomy.
constexpr double kAutoWindowBackgroundLowQuantile = 0.05;

// Fallback (no two classes): a class holding less than this fraction of the pixels is not a class. The wrist frame anatomy
// is 11.7 %; flat-field frames split off 0.8 .. 1.3 % of dead or outlier pixels.
constexpr double kAutoWindowMinClassFraction = 0.02;

// Fallback: Otsu separability eta = between-class variance / total variance. The wrist frame is 0.85 .. 0.89 (linear and log
// domain); real flat-field frames are 0.60 .. 0.72 in the linear domain and the one-tissue synthetic fixture 0.67; a single
// Gaussian gives 2/pi = 0.64 whatever its shape. 0.75 lies between the highest value without anatomy (0.72) and the lowest
// with it (0.85).
constexpr double kAutoWindowMinSeparability = 0.75;

// Fallback window: the 1 % .. 99 % quantile range of the whole image, the window of the 213 baseline run.
constexpr double kAutoWindowFallbackLowQuantile  = 0.01;
constexpr double kAutoWindowFallbackHighQuantile = 0.99;

// Width of the window of a flat image (maximum == minimum). Any positive width gives the same all-equal output; 1.0 keeps
// the number readable.
constexpr float kAutoWindowFlatWidth = 1.0f;

// Quantile q of bins [first, last] of the histogram, linear inside a bin. `lo` is the value of the lower edge of bin 0 and
// `binWidth` the width of every bin.
double histogram_quantile(const uint32_t* hist, int32_t first, int32_t last, double q, double lo, double binWidth) {
    uint64_t total = 0;
    for (int32_t i = first; i <= last; ++i) total += hist[i];
    if (total == 0) return lo + binWidth * static_cast<double>(first);
    const double target = q * static_cast<double>(total);
    double cum = 0.0;
    for (int32_t i = first; i <= last; ++i) {
        const double next = cum + static_cast<double>(hist[i]);
        if (next > target || i == last) {
            const double frac = hist[i] > 0 ? (target - cum) / static_cast<double>(hist[i]) : 0.0;
            return lo + binWidth * (static_cast<double>(i) + std::min(1.0, std::max(0.0, frac)));
        }
        cum = next;
    }
    return lo + binWidth * static_cast<double>(last + 1);
}

void post_fallback_alert(const char* why) {
    char msg[200];
    std::snprintf(msg, sizeof msg,
                  "VOI auto window: %s -- the whole-image 1%%..99%% window was used instead of an anatomy window", why);
    xpe_alert_push(msg, XPE_ALERT_INFO);
}

XpeErrorCode voi_auto_window_impl(const XpeImageBuffer* img, XpeVoiLutParams* outParams) {
    if (!outParams) return XPE_ERR_INVALID_INPUT;
    const XpeErrorCode fmt = xpe_validate_float32(img);
    if (fmt != XPE_OK) return fmt;

    const size_t count = xpe_pixel_count(img);
    const float* px = static_cast<const float*>(img->data);

    // Pass 1: every pixel must be finite (as xpe_apply_voi_lut requires); the extremes scale the histogram.
    float lo = 0.0f, hi = 0.0f;
    if (!xpe_scan_finite(px, count, &lo, &hi)) return XPE_ERR_INVALID_INPUT;

    XpeVoiLutParams res{};
    res.mode   = XPE_VOI_LINEAR_EXACT;  // the window ends map exactly to minOut / maxOut; LINEAR half-value offsets are for integer data
    res.minOut = 0.0f;                  // the [0, 1] range xpe_apply_presentation_lut takes
    res.maxOut = 1.0f;

    if (!(hi > lo)) {
        res.center = lo;
        res.width  = kAutoWindowFlatWidth;
        post_fallback_alert("the image is flat (no value range)");
        *outParams = res;
        return XPE_OK;
    }

    // Pass 2: the histogram of EVERY pixel. Four interleaved sub-histograms break the dependency a repeated bin would put on
    // one counter; they are integer counts and are added up in a fixed order, so the result does not depend on how the
    // compiler schedules the loop. The bin index is computed in float when the value range is an ordinary one (one
    // subtraction and one multiplication, no conversion of every pixel to double: about 4 ms of the 9 ms of the double
    // version on 3072 x 3072), and in double at the extremes of the float range, where a float scale would overflow.
    constexpr int32_t kBins = kAutoWindowHistogramBins;
    const double lod      = static_cast<double>(lo);
    const double range    = static_cast<double>(hi) - lod;  // two finite floats: finite in double, up to 2 * FLT_MAX
    const double binWidth = range / static_cast<double>(kBins);
    uint32_t sub[4][kBins] = {};
    if (range >= 1e-30 && range <= 1e30) {
        const float scale = static_cast<float>(static_cast<double>(kBins) / range);
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            for (size_t k = 0; k < 4; ++k) {
                int32_t b = static_cast<int32_t>((px[i + k] - lo) * scale);
                b = b < 0 ? 0 : (b >= kBins ? kBins - 1 : b);
                ++sub[k][b];
            }
        }
        for (; i < count; ++i) {
            int32_t b = static_cast<int32_t>((px[i] - lo) * scale);
            b = b < 0 ? 0 : (b >= kBins ? kBins - 1 : b);
            ++sub[0][b];
        }
    } else {
        const double toBin = static_cast<double>(kBins) / range;
        for (size_t i = 0; i < count; ++i) {
            int32_t b = static_cast<int32_t>((static_cast<double>(px[i]) - lod) * toBin);
            b = b < 0 ? 0 : (b >= kBins ? kBins - 1 : b);
            ++sub[i & 3][b];
        }
    }
    uint32_t hist[kBins];
    for (int32_t b = 0; b < kBins; ++b) hist[b] = sub[0][b] + sub[1][b] + sub[2][b] + sub[3][b];

    // Otsu on the histogram; the bin index stands for the value (the criterion is unchanged by a scale and a shift).
    const double total = static_cast<double>(count);
    double sumAll = 0.0;
    for (int32_t b = 0; b < kBins; ++b) sumAll += static_cast<double>(b) * static_cast<double>(hist[b]);
    const double mean = sumAll / total;
    double variance = 0.0;
    for (int32_t b = 0; b < kBins; ++b) {
        const double d = static_cast<double>(b) - mean;
        variance += static_cast<double>(hist[b]) * d * d;
    }
    variance /= total;

    double wBelow = 0.0, sumBelow = 0.0, best = -1.0, countBelowAtBest = 0.0;
    int32_t bestBin = 0;
    for (int32_t b = 0; b < kBins; ++b) {
        wBelow += static_cast<double>(hist[b]);
        if (wBelow == 0.0) continue;
        const double wAbove = total - wBelow;
        if (wAbove <= 0.0) break;
        sumBelow += static_cast<double>(b) * static_cast<double>(hist[b]);
        const double mBelow = sumBelow / wBelow;
        const double mAbove = (sumAll - sumBelow) / wAbove;
        const double between = (wBelow / total) * (wAbove / total) * (mBelow - mAbove) * (mBelow - mAbove);
        if (between > best) {
            best = between;
            bestBin = b;
            countBelowAtBest = wBelow;
        }
    }
    const double fracBelow    = countBelowAtBest / total;
    const double fracAbove    = 1.0 - fracBelow;
    const double separability = (variance > 0.0 && best > 0.0) ? best / variance : 0.0;

    const char* fallbackWhy = nullptr;
    if (best < 0.0 || fracBelow < kAutoWindowMinClassFraction || fracAbove < kAutoWindowMinClassFraction) {
        fallbackWhy = "one class holds under 2% of the pixels";
    } else if (separability < kAutoWindowMinSeparability) {
        fallbackWhy = "the histogram is not two-class (Otsu separability under 0.75)";
    }
    double winLo = 0.0, winHi = 0.0;
    if (fallbackWhy) {
        winLo = histogram_quantile(hist, 0, kBins - 1, kAutoWindowFallbackLowQuantile, lod, binWidth);
        winHi = histogram_quantile(hist, 0, kBins - 1, kAutoWindowFallbackHighQuantile, lod, binWidth);
    } else {
        winLo = histogram_quantile(hist, 0, bestBin, kAutoWindowAnatomyLowQuantile, lod, binWidth);
        winHi = histogram_quantile(hist, bestBin + 1, kBins - 1, kAutoWindowBackgroundLowQuantile, lod, binWidth);
    }
    if (!(winHi - winLo >= binWidth)) {  // a degenerate window (also catches NaN): one bin wide, around the middle
        const double mid = 0.5 * (winLo + winHi);
        winLo = mid - 0.5 * binWidth;
        winHi = mid + 0.5 * binWidth;
    }

    // QA-B-214b (Codex #167 finding 4): the window is computed in double and must still be a window in float. Two finite
    // classes at -FLT_MAX and +FLT_MAX span 2 * FLT_MAX, whose float width is infinity; narrowing without the check returned
    // XPE_OK with an infinite width. A window that float cannot hold (center or width beyond FLT_MAX, a width that
    // underflows to zero) is refused with the output untouched. The comparisons come before the casts: the conversion of an
    // out-of-range double to float is undefined.
    const double center  = 0.5 * (winLo + winHi);
    const double winSpan = winHi - winLo;
    if (!(std::fabs(center) <= static_cast<double>(FLT_MAX)) || !(winSpan > 0.0) || !(winSpan <= static_cast<double>(FLT_MAX))) {
        return XPE_ERR_INVALID_INPUT;
    }
    res.center = static_cast<float>(center);
    res.width  = static_cast<float>(winSpan);
    if (!xpe_float_is_finite(res.center) || !xpe_float_is_finite(res.width) || !(res.width > 0.0f)) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (fallbackWhy) post_fallback_alert(fallbackWhy);
    *outParams = res;
    return XPE_OK;
}

}  // namespace

extern "C" XpeErrorCode xpe_voi_auto_window(const XpeImageBuffer* img, XpeVoiLutParams* outParams) {
    xpe_display_log_enter("xpe_voi_auto_window");
    return xpe_display_log_exit("xpe_voi_auto_window", voi_auto_window_impl(img, outParams));
}
