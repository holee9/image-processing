/**
 * @file voi_auto_window.cpp
 * @brief Anatomy-based automatic VOI window (SWU-3.2, QA-B-214 / QA-B-214b, user decision on #251).
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
#include <cstring>

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// Named parameters. Every value was chosen on the real wrist frame of the stage-1 reference image (m1-baseline-v2, 3072 x
// 3072, after log -> bilateral -> CLAHE -> USM, 1000 * log10(ADU + 1) domain) and the other real frames of the repository;
// the numbers behind each are in .moai/reports/lane-post/QA-B-214/report.md and QA-B-214b/report.md. One anatomy frame is a
// small base: a second body part may move them, which is why they are constants here and not literals in the code.
// ---------------------------------------------------------------------------------------------------------------------

// Every 4th pixel of every 4th row (1/16 of the image) feeds the histogram of a large image. Measured against the full
// image, the window ends moved by 0.02 % and 0.04 % of the window width at stride 4 (0.11 % / 0.05 % at 8, 0.12 % / 0.30 %
// at 16): the quantiles are insensitive, and a full histogram costs several times the sampled one (QA-B-214b report).
constexpr size_t kAutoWindowSampleStride = 4;

// QA-B-214b (Codex #167 finding 3): an image with fewer pixels than this is histogrammed in full, not sampled. A 1/16 sample of
// a small image is a handful of points (an 8 x 8 image gives four) and can miss a class altogether. 2^20 pixels is 1024 x 1024:
// the full histogram of a 1000 x 1000 image costs about 1.2 ms (QA-B-214b report), and above the limit the 1/16 sample still
// holds 65536 points.
constexpr size_t kAutoWindowFullPassPixelLimit = static_cast<size_t>(1) << 20;

// QA-B-214b (Codex #167 finding 3): the sample is trusted only if the pixels OUTSIDE the value range the sample itself spans
// are fewer than this fraction of the image. A class the sample missed shows up as a large mass outside that range (the
// 8 x 8 and the off-grid cases of Codex #167: 60 of 64 pixels, 80 % of the image). It must stay under the lowest quantile the
// method reads (0.5 %), or the unseen mass could move that quantile; real frames have 11..37 stray pixels (0.0001..0.0004 %,
// hot and dead pixels), five hundred times under the bound, so a real frame never pays for a second histogram.
constexpr double kAutoWindowMaxUnsampledMass = 0.002;

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

// Fallback (no two classes): a class holding less than this fraction of the sampled pixels is not a class. The wrist frame
// anatomy is 11.7 %; flat-field frames split off 0.8 .. 1.3 % of dead or outlier pixels.
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

// An integer that orders like the float (the sign bit folded into the low bits): -0.0 sorts below +0.0, which is harmless here.
inline int32_t float_sort_key(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    const int32_t s = static_cast<int32_t>(u);
    return s ^ ((s >> 31) & 0x7FFFFFFF);
}

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

// The result of one histogram analysis of an image.
struct Analysis {
    double      winLo       = 0.0;
    double      winHi       = 0.0;
    const char* fallbackWhy = nullptr;  // non-null: no two classes, the window is the 1 %..99 % quantile range
};

// Histogram of every `stride`-th pixel of every `stride`-th row between `lo` and `lo + range`, then the Otsu split and the
// window (or the fallback window).
Analysis analyze(const float* px, size_t width, size_t height, double lo, double range, size_t stride) {
    constexpr int32_t kBins = kAutoWindowHistogramBins;
    uint32_t hist[kBins] = {};
    const double binWidth = range / static_cast<double>(kBins);
    const double toBin    = static_cast<double>(kBins) / range;
    uint64_t sampled = 0;
    for (size_t y = 0; y < height; y += stride) {
        const float* row = px + y * width;
        for (size_t x = 0; x < width; x += stride) {
            int32_t b = static_cast<int32_t>((static_cast<double>(row[x]) - lo) * toBin);
            b = b < 0 ? 0 : (b >= kBins ? kBins - 1 : b);
            ++hist[b];
            ++sampled;
        }
    }

    // Otsu on the histogram; the bin index stands for the value (the criterion is unchanged by a scale and a shift).
    Analysis a;
    const double total = static_cast<double>(sampled);
    double sumAll = 0.0;
    for (int32_t i = 0; i < kBins; ++i) sumAll += static_cast<double>(i) * static_cast<double>(hist[i]);
    const double mean = sumAll / total;
    double variance = 0.0;
    for (int32_t i = 0; i < kBins; ++i) {
        const double d = static_cast<double>(i) - mean;
        variance += static_cast<double>(hist[i]) * d * d;
    }
    variance /= total;

    double wBelow = 0.0, sumBelow = 0.0, best = -1.0, countBelowAtBest = 0.0;
    int32_t bestBin = 0;
    for (int32_t i = 0; i < kBins; ++i) {
        wBelow += static_cast<double>(hist[i]);
        if (wBelow == 0.0) continue;
        const double wAbove = total - wBelow;
        if (wAbove <= 0.0) break;
        sumBelow += static_cast<double>(i) * static_cast<double>(hist[i]);
        const double mBelow = sumBelow / wBelow;
        const double mAbove = (sumAll - sumBelow) / wAbove;
        const double between = (wBelow / total) * (wAbove / total) * (mBelow - mAbove) * (mBelow - mAbove);
        if (between > best) {
            best = between;
            bestBin = i;
            countBelowAtBest = wBelow;
        }
    }
    const double fracBelow    = countBelowAtBest / total;
    const double fracAbove    = 1.0 - fracBelow;
    const double separability = (variance > 0.0 && best > 0.0) ? best / variance : 0.0;

    if (best < 0.0 || fracBelow < kAutoWindowMinClassFraction || fracAbove < kAutoWindowMinClassFraction) {
        a.fallbackWhy = "one class holds under 2% of the sampled pixels";
    } else if (separability < kAutoWindowMinSeparability) {
        a.fallbackWhy = "the histogram is not two-class (Otsu separability under 0.75)";
    }
    if (a.fallbackWhy) {
        a.winLo = histogram_quantile(hist, 0, kBins - 1, kAutoWindowFallbackLowQuantile, lo, binWidth);
        a.winHi = histogram_quantile(hist, 0, kBins - 1, kAutoWindowFallbackHighQuantile, lo, binWidth);
    } else {
        a.winLo = histogram_quantile(hist, 0, bestBin, kAutoWindowAnatomyLowQuantile, lo, binWidth);
        a.winHi = histogram_quantile(hist, bestBin + 1, kBins - 1, kAutoWindowBackgroundLowQuantile, lo, binWidth);
    }
    if (!(a.winHi - a.winLo >= binWidth)) {  // a degenerate window (also catches NaN): one bin wide, around the middle
        const double mid = 0.5 * (a.winLo + a.winHi);
        a.winLo = mid - 0.5 * binWidth;
        a.winHi = mid + 0.5 * binWidth;
    }
    return a;
}

// One full pass over the image: every pixel must be finite (as xpe_apply_voi_lut requires), the extremes of the whole image,
// and how many pixels lie outside [keyLo, keyLo + keySpan] in the sort-key order (the value range of the sample). Integer keys
// that sort like the floats make this one vectorizing loop, like xpe_scan_finite; a float compare with a 64-bit counter did
// not vectorize and cost 7 of the 10 ms of the call. The 32-bit counter is emptied into the 64-bit total every 2^24 pixels,
// so it cannot overflow. Returns false if a pixel is NaN or infinite.
bool scan_full(const float* px, size_t count, uint32_t keyLo, uint32_t keySpan, float* fullLo, float* fullHi, uint64_t* outside) {
    int32_t mn = INT32_MAX, mx = INT32_MIN;
    uint32_t bad = 0;
    uint64_t out64 = 0;
    for (size_t base = 0; base < count; base += (static_cast<size_t>(1) << 24)) {
        const size_t end = std::min(count, base + (static_cast<size_t>(1) << 24));
        uint32_t chunk = 0;
        for (size_t i = base; i < end; ++i) {
            uint32_t u;
            std::memcpy(&u, px + i, sizeof u);
            bad |= static_cast<uint32_t>((u & 0x7F800000u) == 0x7F800000u);
            const int32_t s = static_cast<int32_t>(u);
            const int32_t key = s ^ ((s >> 31) & 0x7FFFFFFF);
            mn = key < mn ? key : mn;
            mx = key > mx ? key : mx;
            // one unsigned compare: a key below keyLo wraps to a huge difference, a key above the span end exceeds keySpan
            chunk += static_cast<uint32_t>((static_cast<uint32_t>(key) - keyLo) > keySpan);
        }
        out64 += chunk;
    }
    auto decode = [](int32_t k) {
        const uint32_t u = static_cast<uint32_t>(k ^ ((k >> 31) & 0x7FFFFFFF));
        float f;
        std::memcpy(&f, &u, sizeof f);
        return f;
    };
    *fullLo = count ? decode(mn) : 0.0f;
    *fullHi = count ? decode(mx) : 0.0f;
    *outside = out64;
    return bad == 0;
}

XpeErrorCode voi_auto_window_impl(const XpeImageBuffer* img, XpeVoiLutParams* outParams) {
    if (!outParams) return XPE_ERR_INVALID_INPUT;
    const XpeErrorCode fmt = xpe_validate_float32(img);
    if (fmt != XPE_OK) return fmt;

    const size_t count  = xpe_pixel_count(img);
    const size_t width  = img->width;
    const size_t height = img->height;
    const float* px = static_cast<const float*>(img->data);

    // QA-B-214b (Codex #167 finding 3): a small image is histogrammed in full; a large one is sampled.
    size_t stride = (count < kAutoWindowFullPassPixelLimit) ? static_cast<size_t>(1) : kAutoWindowSampleStride;

    // Pass 1, the sample: its own value range. (A non-finite pixel makes these numbers meaningless; the full pass below
    // refuses the image before they are used.)
    float sampleLo = FLT_MAX, sampleHi = -FLT_MAX;
    if (stride > 1) {
        for (size_t y = 0; y < height; y += stride) {
            const float* row = px + y * width;
            for (size_t x = 0; x < width; x += stride) {
                sampleLo = row[x] < sampleLo ? row[x] : sampleLo;
                sampleHi = row[x] > sampleHi ? row[x] : sampleHi;
            }
        }
    }

    // Pass 2, the whole image, once: finite check, the real extremes, and the mass outside the sample value range. (Without
    // a sample every pixel is "in the sample": the key range covers everything and the count is not read.)
    const uint32_t keyLo   = (stride > 1) ? static_cast<uint32_t>(float_sort_key(sampleLo)) : 0u;
    const uint32_t keySpan = (stride > 1) ? static_cast<uint32_t>(float_sort_key(sampleHi)) - keyLo : UINT32_MAX;
    float lo = 0.0f, hi = 0.0f;
    uint64_t outside = 0;
    if (!scan_full(px, count, keyLo, keySpan, &lo, &hi, &outside)) return XPE_ERR_INVALID_INPUT;
    if (stride == 1) {
        sampleLo = lo;
        sampleHi = hi;
    }

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

    // The sample is trusted only if it spans a value range at all and the pixels outside that range are fewer than
    // kAutoWindowMaxUnsampledMass of the image; otherwise the whole image is histogrammed.
    const bool trusted = (stride == 1) ||
                         (sampleHi > sampleLo &&
                          static_cast<double>(outside) <= kAutoWindowMaxUnsampledMass * static_cast<double>(count));
    if (!trusted) stride = 1;

    // The histogram covers the value range of the pixels it is built from: the sample own range (the extremes of the whole
    // image may be single hot or dead pixels no sample took), or the range of the whole image when everything is histogrammed.
    double lod   = (stride == 1) ? static_cast<double>(lo) : static_cast<double>(sampleLo);
    double range = ((stride == 1) ? static_cast<double>(hi) : static_cast<double>(sampleHi)) - lod;  // finite: two finite floats
    Analysis a = analyze(px, width, height, lod, range, stride);

    // A fallback decided on a sample is confirmed on the whole image, so the fallback window is always made of the real
    // whole-image quantiles and a class the sample missed is never lost.
    if (stride > 1 && a.fallbackWhy) {
        lod   = static_cast<double>(lo);
        range = static_cast<double>(hi) - lod;
        a = analyze(px, width, height, lod, range, 1);
    }

    // QA-B-214b (Codex #167 finding 4): the window is computed in double and must still be a window in float. Two finite
    // classes at -FLT_MAX and +FLT_MAX span 2 * FLT_MAX, whose float width is infinity; narrowing without the check returned
    // XPE_OK with an infinite width. A window that float cannot hold (center or width beyond FLT_MAX, a width that
    // underflows to zero) is refused with the output untouched. The comparisons come before the casts: the conversion of an
    // out-of-range double to float is undefined.
    const double center  = 0.5 * (a.winLo + a.winHi);
    const double winSpan = a.winHi - a.winLo;
    if (!(std::fabs(center) <= static_cast<double>(FLT_MAX)) || !(winSpan > 0.0) || !(winSpan <= static_cast<double>(FLT_MAX))) {
        return XPE_ERR_INVALID_INPUT;
    }
    res.center = static_cast<float>(center);
    res.width  = static_cast<float>(winSpan);
    if (!xpe_float_is_finite(res.center) || !xpe_float_is_finite(res.width) || !(res.width > 0.0f)) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (a.fallbackWhy) post_fallback_alert(a.fallbackWhy);
    *outParams = res;
    return XPE_OK;
}

}  // namespace

extern "C" XpeErrorCode xpe_voi_auto_window(const XpeImageBuffer* img, XpeVoiLutParams* outParams) {
    xpe_display_log_enter("xpe_voi_auto_window");
    return xpe_display_log_exit("xpe_voi_auto_window", voi_auto_window_impl(img, outParams));
}
