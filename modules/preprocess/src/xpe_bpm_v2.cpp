/**
 * @file xpe_bpm_v2.cpp
 * @brief Second-generation defective-pixel detection (SRS-CALIB-FUNC-022, QA-A-252, #273)
 *
 * The method is fixed in .moai/reports/lane-pre/QA-A-252/evidence/30_method_fixed_before_measurement.md.
 *
 * Five per-pixel features are judged one by one and the union is flagged:
 *   F1 local deviation of the dark mean                 (robust z)
 *   F2 temporal noise of the dark frames                (variance ratio against its local median)
 *   F3a slope of the response over the dose groups      (robust z)
 *   F3b straight-line residual over the dose groups     (robust z of the log of the residual variance, upper side only)
 *   F4 local deviation of the relative flat-field response (robust z)
 *
 * Robust z: the row and column components are subtracted from the feature image (median per row, then per column), the local background
 * is a separable median (rows, then columns) of a window of odd side, the local scale is 1.4826 * median(|residual|) per block, interpolated
 * bilinearly between block centres. The thresholds follow from the target number of false alarms per feature over the whole frame.
 */

#include "xpe/preprocess_api.h"
#include "xpe_bpm_v2_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <vector>

namespace {

constexpr double kMadToSigma = 1.4826;
constexpr double kResponseLow = 0.45;       // SRS FUNC-022 (provisional, user decision): a response below 45 % of the local median is a defect
constexpr double kResponseHigh = 1.5;       // ... above 150 %
constexpr double kWeakResponseHigh = 0.7;   // 45 % up to 70 %: weak, counted only
constexpr double kNoiseCostLimit = 2.0;     // SRS FUNC-022 (leader decision, synthesis P2): a gain-corrected variance of twice the local expectation (3.01 dB) is a defect
constexpr double kRatioSlack = 1e-6;        // the ratio is a quotient of floats (about 6e-8 resolution): a response of exactly 0.45 or 1.5 is normal, whatever the rounding
constexpr double kScaleFloor = 1e-6;
constexpr double kReferenceFloor = 1e-9;

struct Settings {
    double   falseAlarms;
    uint32_t window, block, skip, minDark, minBright;
    double   ratio;
};

XpeErrorCode resolve(const XpeBpmConfigV2* in, Settings* s) noexcept {
    s->falseAlarms = 1.0;
    s->window = 17;
    s->block = 64;
    s->skip = 1;   // the dark list is the raw acquisition list: its first frame is skipped unless the caller says it was trimmed already
    s->minDark = 5;
    s->minBright = 6;
    s->ratio = 1.25;
    if (!in) return XPE_OK;
    if (in->reserved != 0) return XPE_ERR_INVALID_INPUT;
    if (in->target_false_alarms != 0.0f) s->falseAlarms = in->target_false_alarms;
    if (in->window != 0) s->window = in->window;
    if (in->sigma_block != 0) s->block = in->sigma_block;
    if (in->skip_first_dark > XPE_BPM_V2_SKIP_NONE) return XPE_ERR_INVALID_INPUT;
    s->skip = in->skip_first_dark == XPE_BPM_V2_SKIP_NONE ? 0u : 1u;
    if (in->min_frames_dark != 0) s->minDark = in->min_frames_dark;
    if (in->min_frames_bright != 0) s->minBright = in->min_frames_bright;
    if (in->dose_group_ratio != 0.0f) s->ratio = in->dose_group_ratio;
    if (!(s->falseAlarms > 0.0 && s->falseAlarms <= 1000.0)) return XPE_ERR_INVALID_INPUT;
    if (s->window < 9 || s->window > 65 || (s->window % 2) == 0) return XPE_ERR_INVALID_INPUT;
    if (s->block < 16 || s->block > 512) return XPE_ERR_INVALID_INPUT;
    if (s->minDark < 3 || s->minBright < 2) return XPE_ERR_INVALID_INPUT;
    if (!(s->ratio >= 1.01)) return XPE_ERR_INVALID_INPUT;
    return XPE_OK;
}

// ---------------------------------------------------------------------------------------------------------------- statistics
/** Two-sided z threshold: erfc(lambda / sqrt 2) = p. */
double zThreshold(double p) {
    double lo = 0.0, hi = 40.0;
    for (int it = 0; it < 200; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (std::erfc(mid / std::sqrt(2.0)) > p) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/** Regularised upper incomplete gamma Q(a, x) (series below a + 1, continued fraction above; Numerical Recipes). */
double gammaQ(double a, double x) {
    if (x <= 0.0) return 1.0;
    const double gln = std::lgamma(a);
    if (x < a + 1.0) {
        double ap = a, del = 1.0 / a, sum = del;
        for (int n = 0; n < 1000; ++n) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (std::fabs(del) < std::fabs(sum) * 1e-15) break;
        }
        return 1.0 - sum * std::exp(-x + a * std::log(x) - gln);
    }
    double b = x + 1.0 - a, c = 1.0 / 1e-300, d = 1.0 / b, h = d;
    for (int i = 1; i < 1000; ++i) {
        const double an = -static_cast<double>(i) * (static_cast<double>(i) - a);
        b += 2.0;
        d = an * d + b;
        if (std::fabs(d) < 1e-300) d = 1e-300;
        c = b + an / c;
        if (std::fabs(c) < 1e-300) c = 1e-300;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < 1e-15) break;
    }
    return std::exp(-x + a * std::log(x) - gln) * h;
}

/** tau with P(chi-square_nu / nu > tau) = p. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): nu is the degrees of freedom of a variance ratio and p the tail probability the threshold must keep; the two call sites pass (nu[k], p) and (nuBright, p)
double varianceThreshold(uint32_t nu, double p) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    const double a = 0.5 * nu;
    double lo = 1.0, hi = 1e6;
    for (int it = 0; it < 300; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (gammaQ(a, a * mid) > p) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/** Median of chi-square_nu / nu. */
double varianceMedian(uint32_t nu) {
    const double a = 0.5 * nu;
    double lo = 1e-6, hi = 1e3;
    for (int it = 0; it < 300; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (gammaQ(a, a * mid) > 0.5) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

inline int reflect(int i, int n) {
    if (n <= 1) return 0;
    while (i < 0 || i >= n) {
        if (i < 0) i = -i;
        if (i >= n) i = 2 * n - 2 - i;
    }
    return i;
}

float medianOf(float* a, size_t n) {
    std::nth_element(a, a + n / 2, a + n);
    return a[n / 2];
}

/** Median of a few values (the frames of one pixel): the middle one, the mean of the two middle ones for an even count (so that a pair gives its mean). */
float temporalMedian(std::vector<float>* v) {
    std::sort(v->begin(), v->end());
    const size_t m = v->size();
    return (m & 1u) ? (*v)[m / 2] : 0.5f * ((*v)[m / 2 - 1] + (*v)[m / 2]);
}

/** Separable median filter: window x 1 along the rows, then 1 x window along the columns; reflected borders. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): in is read only, tmp is the scratch of the horizontal pass and out receives the result; w and h are the image size, win the window; the six call sites pass (wk.feat or a local plane, tmp, out, w, h, window)
void separableMedian(const float* in, float* tmp, float* out, int w, int h, int win) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    const int r = win / 2;
    float buf[65];
    for (int y = 0; y < h; ++y) {
        const float* row = in + static_cast<size_t>(y) * static_cast<size_t>(w);
        float* o = tmp + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            for (int k = -r; k <= r; ++k) buf[k + r] = row[reflect(x + k, w)];
            std::nth_element(buf, buf + r, buf + win);
            o[x] = buf[r];
        }
    }
    for (int y = 0; y < h; ++y) {
        float* o = out + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            for (int k = -r; k <= r; ++k) buf[k + r] = tmp[static_cast<size_t>(reflect(y + k, h)) * static_cast<size_t>(w) + static_cast<size_t>(x)];
            std::nth_element(buf, buf + r, buf + win);
            o[x] = buf[r];
        }
    }
}

/**
 * The separable median with pixels left out (QA-A-252i): the median along x over the window pixels that are NOT excluded, then the median along y of those over the pixels that exist.
 * `excl` is 1 for an excluded pixel. A row window with no admissible pixel gives no value (NaN) to the second pass; a pixel whose second pass finds no value takes `plain`, the plain
 * separable median of the same image. Reflected borders as in separableMedian.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): same roles as separableMedian, plus plain (the fallback value of a pixel whose whole window is excluded) and excl (1 = left out); the one call site passes (vg, vgTmp, vgBg, plainRef, refExclude, w, h, window)
void separableMedianExcluding(const float* in, float* tmp, float* out, const float* plain, const uint8_t* excl, int w, int h, int win) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    const int r = win / 2;
    float     buf[65];
    const float nan = std::numeric_limits<float>::quiet_NaN();
    for (int y = 0; y < h; ++y) {
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            int cnt = 0;
            for (int k = -r; k <= r; ++k) {
                const size_t i = row + static_cast<size_t>(reflect(x + k, w));
                if (!excl[i]) buf[cnt++] = in[i];
            }
            if (cnt == 0) {
                tmp[row + static_cast<size_t>(x)] = nan;
            } else {
                std::nth_element(buf, buf + cnt / 2, buf + cnt);
                tmp[row + static_cast<size_t>(x)] = buf[cnt / 2];
            }
        }
    }
    for (int y = 0; y < h; ++y) {
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) {
            int cnt = 0;
            for (int k = -r; k <= r; ++k) {
                const float v = tmp[static_cast<size_t>(reflect(y + k, h)) * static_cast<size_t>(w) + static_cast<size_t>(x)];
                if (v == v) buf[cnt++] = v;
            }
            if (cnt == 0) {
                out[row + static_cast<size_t>(x)] = plain[row + static_cast<size_t>(x)];
            } else {
                std::nth_element(buf, buf + cnt / 2, buf + cnt);
                out[row + static_cast<size_t>(x)] = buf[cnt / 2];
            }
        }
    }
}

/** Median of @p win values along the columns (alongY) or along the rows, reflected borders. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): in is read and out written; w and h are the image size, win the window and alongY the direction; the callers pass (f, tmp, w, h, win, true) and (f, tmp, w, h, win, false)
void slidingMedian(const float* in, float* out, int w, int h, int win, bool alongY) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    const int r = win / 2;
    float buf[65];
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (alongY) {
                for (int k = -r; k <= r; ++k) buf[k + r] = in[static_cast<size_t>(reflect(y + k, h)) * static_cast<size_t>(w) + static_cast<size_t>(x)];
            } else {
                for (int k = -r; k <= r; ++k) buf[k + r] = in[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(reflect(x + k, w))];
            }
            std::nth_element(buf, buf + r, buf + win);
            out[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = buf[r];
        }
}

/**
 * Removes the row component, then the column component (QA-A-252b). The component of a row is the median over the row of the difference between the row and the
 * median of the neighbouring rows at the same column, so a gradient along the row cancels and a defect that covers a part of the row moves the median by no more than
 * a fraction of the noise. Taking the median of the row itself (the first version) moved it by a fraction of the whole gradient. With @p multiplicative the
 * component is a factor (the median of the ratio) instead of an offset. Constant over the whole line: removed. Not constant along the line: left for the later steps.
 */
void removeRowColumn(float* f, float* tmp, int w, int h, int win, bool multiplicative) {
    std::vector<float> line(static_cast<size_t>(std::max(w, h)));
    auto component = [&](float v, float ref) -> float { return multiplicative ? (std::fabs(ref) > 1e-30f ? v / ref : 1.0f) : v - ref; };
    auto apply = [&](float& v, float c) {
        if (multiplicative) {
            if (std::fabs(c) > 1e-30f) v /= c;
        } else {
            v -= c;
        }
    };
    slidingMedian(f, tmp, w, h, win, true);
    for (int y = 0; y < h; ++y) {
        float* row = f + static_cast<size_t>(y) * static_cast<size_t>(w);
        const float* ref = tmp + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x) line[static_cast<size_t>(x)] = component(row[x], ref[x]);
        const float c = medianOf(line.data(), static_cast<size_t>(w));
        for (int x = 0; x < w; ++x) apply(row[x], c);
    }
    slidingMedian(f, tmp, w, h, win, false);
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) line[static_cast<size_t>(y)] = component(f[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)], tmp[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]);
        const float c = medianOf(line.data(), static_cast<size_t>(h));
        for (int y = 0; y < h; ++y) apply(f[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)], c);
    }
}

/** Local scale: 1.4826 * median(|r|) per block, one value per block; returns the median over the blocks as a summary. */
float blockScale(const float* r, int w, int h, int block, std::vector<float>* sig, int* nbx, int* nby) {
    *nbx = (w + block - 1) / block;
    *nby = (h + block - 1) / block;
    sig->assign(static_cast<size_t>(*nbx) * static_cast<size_t>(*nby), 0.0f);
    std::vector<float> vals;
    vals.reserve(static_cast<size_t>(block) * static_cast<size_t>(block));
    for (int by = 0; by < *nby; ++by)
        for (int bx = 0; bx < *nbx; ++bx) {
            vals.clear();
            for (int y = by * block; y < std::min(h, (by + 1) * block); ++y)
                for (int x = bx * block; x < std::min(w, (bx + 1) * block); ++x) vals.push_back(std::fabs(r[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]));
            (*sig)[static_cast<size_t>(by) * static_cast<size_t>(*nbx) + static_cast<size_t>(bx)] = static_cast<float>(kMadToSigma * medianOf(vals.data(), vals.size()));
        }
    std::vector<float> copy(*sig);
    return medianOf(copy.data(), copy.size());
}

/** Bilinear value of the block scales at pixel (x, y). */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): nbx and nby are the size of the block grid, block the edge of a block in pixels and x and y the pixel; the three call sites pass (sig, nbx, nby, block, x, y)
inline float scaleAt(const std::vector<float>& sig, int nbx, int nby, int block, int x, int y) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    const float fx = std::min(std::max((static_cast<float>(x) + 0.5f) / static_cast<float>(block) - 0.5f, 0.0f), static_cast<float>(nbx - 1));
    const float fy = std::min(std::max((static_cast<float>(y) + 0.5f) / static_cast<float>(block) - 0.5f, 0.0f), static_cast<float>(nby - 1));
    const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, nbx - 1), y1 = std::min(y0 + 1, nby - 1);
    const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
    auto at = [&](int xx, int yy) { return sig[static_cast<size_t>(yy) * static_cast<size_t>(nbx) + static_cast<size_t>(xx)]; };
    return (1.0f - ty) * ((1.0f - tx) * at(x0, y0) + tx * at(x1, y0)) + ty * ((1.0f - tx) * at(x0, y1) + tx * at(x1, y1));
}

struct Work {
    int                w, h;
    size_t             n;
    std::vector<float> feat, tmp, bg;
};

/**
 * The response ratio rule (SRS FUNC-022; QA-A-252d, corrected in QA-A-252f): the feature is the relative response of the OFFSET-CORRECTED bright frames (summed over the
 * dose groups), and the background is its local median as it is: no row or column factor is taken out first (that normalisation belongs to the other features only).
 * The local median is the separable one of the other features: the median over @c window pixels along x, then the median of those over @c window pixels along y; it is
 * not a true two-dimensional median of the window. A pixel whose feature is below @p lo or above @p hi times the background (to within kRatioSlack) is defective.
 * @p weak counts the pixels from @p lo up to @p weakHi (not flagged). Returns the median ratio.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): lo, hi and weakHi are the lower limit, the upper limit and the upper limit of the weak band of the response, in ascending order, passed as the named constants kResponseLow, kResponseHigh, kWeakResponseHigh; count and weak are two output counters (flagged pixels, weak-response stretches)
float flagResponseRatio(Work& wk, const Settings& st, double lo, double hi, double weakHi, uint8_t bit, std::vector<uint8_t>* flags, uint32_t* count, uint32_t* weak) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    separableMedian(wk.feat.data(), wk.tmp.data(), wk.bg.data(), wk.w, wk.h, static_cast<int>(st.window));
    std::vector<float> sample;
    sample.reserve(wk.n / 16 + 1);
    for (size_t i = 0; i < wk.n; ++i) {
        const double bg = static_cast<double>(wk.bg[i]);
        if (!(std::fabs(bg) > 1e-12)) continue;
        const double ratio = static_cast<double>(wk.feat[i]) / bg;
        if (i % 16 == 0) sample.push_back(static_cast<float>(ratio));
        if (ratio < lo - kRatioSlack || ratio > hi + kRatioSlack) {
            (*flags)[i] = static_cast<uint8_t>((*flags)[i] | bit);
            ++*count;
        } else if (ratio < weakHi) {
            ++*weak;
        }
    }
    return sample.empty() ? 0.0f : medianOf(sample.data(), sample.size());
}

/** Robust z feature: subtracts rows and columns, estimates background and scale, flags |z| > lambda (z > lambda when @p upperOnly). Returns the median local scale. */
float flagRobustZ(Work& wk, const Settings& st, double lambda, bool upperOnly, bool multiplicative, uint8_t bit, std::vector<uint8_t>* flags, uint32_t* count) {
    removeRowColumn(wk.feat.data(), wk.tmp.data(), wk.w, wk.h, static_cast<int>(st.window), multiplicative);
    separableMedian(wk.feat.data(), wk.tmp.data(), wk.bg.data(), wk.w, wk.h, static_cast<int>(st.window));
    for (size_t i = 0; i < wk.n; ++i) wk.feat[i] -= wk.bg[i];   // residual
    std::vector<float> sig;
    int nbx = 0, nby = 0;
    const float med = blockScale(wk.feat.data(), wk.w, wk.h, static_cast<int>(st.block), &sig, &nbx, &nby);
    for (int y = 0; y < wk.h; ++y)
        for (int x = 0; x < wk.w; ++x) {
            const size_t i = static_cast<size_t>(y) * static_cast<size_t>(wk.w) + static_cast<size_t>(x);
            const double s = std::max(static_cast<double>(scaleAt(sig, nbx, nby, static_cast<int>(st.block), x, y)), kScaleFloor);
            const double r = static_cast<double>(wk.feat[i]);
            if ((upperOnly ? r : std::fabs(r)) > lambda * s) {
                (*flags)[i] = static_cast<uint8_t>((*flags)[i] | bit);
                ++*count;
            }
        }
    return med;
}

/** Variance-ratio feature: V against its local median divided by the median of chi-square_nu / nu; flags the ratio above tau. */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): nu is the degrees of freedom and tau the threshold ratio of the variance check; the one call site passes (nu[1], threshold[1]) from two arrays of the same index
float flagVarianceRatio(Work& wk, const Settings& st, uint32_t nu, double tau, uint8_t bit, std::vector<uint8_t>* flags, uint32_t* count) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    separableMedian(wk.feat.data(), wk.tmp.data(), wk.bg.data(), wk.w, wk.h, static_cast<int>(st.window));
    const double mnu = varianceMedian(nu);
    std::vector<float> sample;
    sample.reserve(wk.n / 16 + 1);
    for (size_t i = 0; i < wk.n; i += 16) sample.push_back(static_cast<float>(wk.bg[i] / mnu));
    const float med = medianOf(sample.data(), sample.size());
    for (size_t i = 0; i < wk.n; ++i) {
        const double ref = std::max(static_cast<double>(wk.bg[i]) / mnu, kReferenceFloor);
        if (static_cast<double>(wk.feat[i]) / ref > tau) {
            (*flags)[i] = static_cast<uint8_t>((*flags)[i] | bit);
            ++*count;
        }
    }
    return med;
}

// ---------------------------------------------------------------------------------------------------------------------------------------- lines (QA-A-253)
enum LineKind : std::uint8_t { kKindResponse = 0, kKindLinearity = 1, kKindNoise = 2 };

struct Seg {
    int      orient;   // 0 row, 1 column
    int      idx;      // the row or column
    int      a, b;     // [a, b) along the line
    uint32_t cause;
    float    z;
    float    persist{-1.0f};   // fraction of the frames in which the stretch is large; -1 = not tested
    bool     unconfirmed{false};   // the persistence could not be judged: a candidate, not a defect
};

struct LineJob {
    uint32_t         darkRaw{0}, darkValid{0}, brightRaw{0}, brightValid{0};   // the frames of the persistence judgement
    bool             countWeak{false};   // count the weak-response stretches of the response feature being analysed
    uint32_t         responseCause{XPE_BPM_LINE_CAUSE_RESPONSE};   // the cause a response stretch gets: RESPONSE for the offset-corrected image, SLOPE for the slope proxy
    uint32_t         weak{0};
    double           fa{1.0};
    int              minSeg{64};
    double           tol{1.9952623149688795};   // 3 dB as a power ratio
    bool             linesOnly{false};
    // set while the dark-frame noise feature is analysed: the fraction of the dark frames in which the stretch (orientation, line, [a, b)) is large
    std::function<float(int, int, int, int)> persistence;
    std::vector<Seg> segs;
    float            nullWidth[6]{1, 1, 1, 1, 1, 1};
};

void transposeImage(const float* in, float* out, int w, int h) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) out[static_cast<size_t>(x) * static_cast<size_t>(h) + static_cast<size_t>(y)] = in[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
}

/**
 * Stretches along the rows of f (w x h). e is the difference to the median of the neighbouring rows at the same column (for the response kind the ratio minus one);
 * the window statistic is the sum over min_segment pixels of e / local scale, each term cut at +-3 so that a single outlier (a dead pixel, a hot pixel) cannot make a
 * stretch, divided by the square root of the window length: the uncorrelated-noise prediction. Its robust width over all windows (the empirical null) rescales it.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters): w and h are the plane size; kind (LineKind), orient (0 row, 1 column) and nullSlot (the slot of the empirical null) are three small codes; the two call sites pass the plane as (w, h) with orient 0 and the transposed plane as (h, w) with orient 1, each with its own slot
void rowStretches(const float* f, int w, int h, const Settings& st, LineJob* job, int kind, int orient, int nullSlot, std::vector<Seg>* out) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    const int    L = std::min(job->minSeg, w);
    if (L < 8) return;
    std::vector<float> ref(n), e(n);
    slidingMedian(f, ref.data(), w, h, static_cast<int>(st.window), true);
    for (size_t i = 0; i < n; ++i) {
        if (kind == kKindResponse) e[i] = std::fabs(ref[i]) > 1e-30f ? f[i] / ref[i] - 1.0f : 0.0f;
        else e[i] = f[i] - ref[i];
    }
    std::vector<float> sig;
    int                nbx = 0, nby = 0;
    blockScale(e.data(), w, h, static_cast<int>(st.block), &sig, &nbx, &nby);
    const int nw = w - L + 1;
    std::vector<float> zw(static_cast<size_t>(h) * static_cast<size_t>(nw));
    std::vector<double> cs(static_cast<size_t>(w) + 1);
    for (int y = 0; y < h; ++y) {
        cs[0] = 0.0;
        for (int x = 0; x < w; ++x) {
            const double s = std::max(static_cast<double>(scaleAt(sig, nbx, nby, static_cast<int>(st.block), x, y)), 1e-9);
            double       t = static_cast<double>(e[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]) / s;
            t = std::min(3.0, std::max(-3.0, t));
            cs[static_cast<size_t>(x) + 1] = cs[static_cast<size_t>(x)] + t;
        }
        for (int x = 0; x < nw; ++x) zw[static_cast<size_t>(y) * static_cast<size_t>(nw) + static_cast<size_t>(x)] = static_cast<float>((cs[static_cast<size_t>(x) + static_cast<size_t>(L)] - cs[static_cast<size_t>(x)]) / std::sqrt(static_cast<double>(L)));
    }
    // the empirical null: robust width of the window statistic over all rows and windows
    std::vector<float> sample;
    sample.reserve(zw.size() / 8 + 1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < nw; x += 8) sample.push_back(std::fabs(zw[static_cast<size_t>(y) * static_cast<size_t>(nw) + static_cast<size_t>(x)]));
    const double kappa = std::max(1.0, static_cast<double>(kMadToSigma) * static_cast<double>(medianOf(sample.data(), sample.size())));
    job->nullWidth[nullSlot] = static_cast<float>(kappa);
    const double tests = std::max(1.0, static_cast<double>(h) * static_cast<double>(w) / static_cast<double>(L));
    const double lambda = zThreshold(job->fa / tests);
    const double lo = kResponseLow, hi = kResponseHigh;
    for (int y = 0; y < h; ++y) {
        const float* zr = zw.data() + static_cast<size_t>(y) * static_cast<size_t>(nw);
        const float* er = e.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        int          x = 0;
        while (x < nw) {
            const double z = static_cast<double>(zr[x]) / kappa;
            const bool   upperOnly = kind != kKindResponse;
            const int    sign = z > lambda ? 1 : ((!upperOnly && z < -lambda) ? -1 : 0);
            if (sign == 0) {
                ++x;
                continue;
            }
            int    x2 = x;
            double zmax = std::fabs(z);
            while (x2 + 1 < nw) {
                const double z2 = static_cast<double>(zr[x2 + 1]) / kappa;
                if (!(sign > 0 ? z2 > lambda : z2 < -lambda)) break;
                ++x2;
                zmax = std::max(zmax, std::fabs(z2));
            }
            int a = x, b = x2 + L;
            x = x2 + 1;
            // The ends: the windows give a coarse stretch (a window that only touches the stretch can still reach the threshold, so the coarse ends lie outside by up to a
            // window). The ends are the change points of the maximum-likelihood fit of one elevated stretch: with m the mean level of the stretch (taken from its core), the
            // stretch is the maximum-sum run of (z - m/2) in the cut z terms, searched within one window length beyond the coarse ends.
            // QA-A-314: the upper cut is the level of the stretch itself (the median of its uncut core terms), at least 3. With a fixed cut at 3 a strong line (tens of noise
            // widths) had m = 3 and its ends took in the ordinary pixels of 1.5 to 3 widths next to it (3 to 5 pixels beyond the true end in the B16 measurement). A line whose
            // core level is 3 or less is cut as before.
            {
                const int lo2 = std::max(0, a - L), hi2 = std::min(w, b + L);
                std::vector<double> zt(static_cast<size_t>(hi2 - lo2));
                for (int t = lo2; t < hi2; ++t) {
                    const double sc = std::max(static_cast<double>(scaleAt(sig, nbx, nby, static_cast<int>(st.block), t, y)), 1e-9);
                    zt[static_cast<size_t>(t - lo2)] = sign * static_cast<double>(er[t]) / sc;
                }
                const int c0 = (b - a >= 2 * L) ? a + L / 2 : a, c1 = (b - a >= 2 * L) ? b - L / 2 : b;
                std::vector<double> core(zt.begin() + (c0 - lo2), zt.begin() + (c1 - lo2));
                std::nth_element(core.begin(), core.begin() + static_cast<std::ptrdiff_t>(core.size() / 2), core.end());
                const double cutHi = std::max(3.0, core[core.size() / 2]);
                for (double& v : zt) v = std::min(cutHi, std::max(-3.0, v));
                double    m = 0.0;
                for (int t = c0; t < c1; ++t) m += zt[static_cast<size_t>(t - lo2)];
                m /= std::max(1, c1 - c0);
                if (!(m > 0.0)) continue;
                double best = -1e300, cur = 0.0;
                int    curStart = lo2, bestA = lo2, bestB = lo2;
                for (int t = lo2; t < hi2; ++t) {
                    const double v = zt[static_cast<size_t>(t - lo2)] - 0.5 * m;
                    if (cur <= 0.0) {
                        cur = v;
                        curStart = t;
                    } else {
                        cur += v;
                    }
                    if (cur > best) {
                        best = cur;
                        bestA = curStart;
                        bestB = t + 1;
                    }
                }
                a = bestA;
                b = bestB;
            }
            if (b - a < L) continue;
            // magnitude: a stretch that offset and gain correction removes is no defect
            auto segSum = [&](int yy) {
                double t = 0.0;
                for (int k = a; k < b; ++k) {
                    const size_t i = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(k);
                    double       v = static_cast<double>(f[i]);
                    // a variance ratio has a heavy tail (single defective pixels): each pixel counts at most twice the tolerance, so that a few of them cannot make a line
                    if (kind == kKindNoise) v = std::min(v, 2.0 * job->tol * std::max(static_cast<double>(ref[i]), 0.0));
                    t += v;
                }
                return t;
            };
            const double sf = segSum(y);
            std::vector<float> nbSums;   // the neighbouring rows, the same pixels: mean against mean (a median of single pixels is biased low for skewed features)
            const int half = static_cast<int>(st.window) / 2;
            for (int dy = -half; dy <= half; ++dy)
                if (dy != 0 && y + dy >= 0 && y + dy < h) nbSums.push_back(static_cast<float>(segSum(y + dy)));
            const double sr = nbSums.empty() ? sf : static_cast<double>(medianOf(nbSums.data(), nbSums.size()));
            uint32_t cause = 0;
            if (kind == kKindResponse) {
                const double g = std::fabs(sr) > 1e-30 ? sf / sr : 1.0;
                if (g < lo - kRatioSlack || g > hi + kRatioSlack) cause = job->responseCause;
                else if (job->countWeak && g < kWeakResponseHigh) ++job->weak;
            } else if (kind == kKindNoise) {
                const double g = std::fabs(sr) > 1e-30 ? sf / sr : 1.0;
                if (g >= job->tol) cause = XPE_BPM_LINE_CAUSE_NOISE;
            } else {
                cause = XPE_BPM_LINE_CAUSE_LINEARITY;
            }
            if (cause == 0) continue;
            float persist = -1.0f;
            bool  unconfirmed = false;
            if (cause == XPE_BPM_LINE_CAUSE_NOISE && job->persistence) {
                persist = job->persistence(orient, y, a, b);
                // large in half of the frames or fewer: noise that moves, not a defect of a place. A negative value says the test was not possible (fewer than three frames
                // to judge by): the stretch is an unconfirmed candidate, listed apart and never painted
                if (persist < 0.0f) unconfirmed = true;
                else if (!(persist > 0.5f)) continue;
            }
            out->push_back(Seg{orient, y, a, b, cause, static_cast<float>(zmax), persist, unconfirmed});
        }
    }
}

/** Examines one feature image along its rows and its columns. */
void analyzeFeature(const float* f, int w, int h, const Settings& st, LineJob* job, int kind) {
    const int slot = kind * 2;
    rowStretches(f, w, h, st, job, kind, 0, slot, &job->segs);
    std::vector<float> t(static_cast<size_t>(w) * static_cast<size_t>(h));
    transposeImage(f, t.data(), w, h);
    rowStretches(t.data(), h, w, st, job, kind, 1, slot + 1, &job->segs);
}

struct Group {
    int      orient, idx, width, a, b;   // a, b: the bounding range along the lines of the members (metadata only)
    uint32_t causes, flags;
    float    z;
    float    persist;                    // the largest persistence of the member stretches
    struct Member {
        int idx, a, b;
    };
    std::vector<Member> members;         // the individual stretches: what is painted, counted and crossed
};

/** Merges the stretches of one line, then groups adjacent lines whose stretches overlap by half. */
std::vector<Group> groupSegments(const std::vector<Seg>& segs, int w, int h) {
    std::vector<Group> groups;
    for (int orient = 0; orient < 2; ++orient) {
        std::vector<Seg> mine;
        for (const Seg& s : segs)
            if (s.orient == orient) mine.push_back(s);
        std::sort(mine.begin(), mine.end(), [](const Seg& p, const Seg& q) { return p.idx != q.idx ? p.idx < q.idx : p.a < q.a; });
        // one stretch per overlapping set on a line
        std::vector<Seg> lines;
        for (const Seg& s : mine) {
            if (!lines.empty() && lines.back().idx == s.idx && s.a <= lines.back().b) {
                lines.back().b = std::max(lines.back().b, s.b);
                lines.back().cause |= s.cause;
                lines.back().z = std::max(lines.back().z, s.z);
                lines.back().persist = std::max(lines.back().persist, s.persist);
            } else {
                lines.push_back(s);
            }
        }
        // adjacent lines with overlapping stretches form a group
        std::vector<bool> used(lines.size(), false);
        for (size_t i = 0; i < lines.size(); ++i) {
            if (used[i]) continue;
            Group g{orient, lines[i].idx, 1, lines[i].a, lines[i].b, lines[i].cause, 0, lines[i].z, lines[i].persist, {}};
            g.members.push_back({lines[i].idx, lines[i].a, lines[i].b});
            used[i] = true;
            int last = lines[i].idx;
            for (size_t j = i + 1; j < lines.size(); ++j) {
                if (used[j] || lines[j].idx != last + 1) continue;
                const int ov = std::min(g.b, lines[j].b) - std::max(g.a, lines[j].a);
                const int shorter = std::min(g.b - g.a, lines[j].b - lines[j].a);
                if (ov * 2 < shorter) continue;
                g.a = std::min(g.a, lines[j].a);
                g.b = std::max(g.b, lines[j].b);
                g.causes |= lines[j].cause;
                g.z = std::max(g.z, lines[j].z);
                g.persist = std::max(g.persist, lines[j].persist);
                g.members.push_back({lines[j].idx, lines[j].a, lines[j].b});
                ++g.width;
                last = lines[j].idx;
                used[j] = true;
            }
            const int length = orient == 0 ? w : h;
            bool full = true;   // full when every member covers the line
            for (const Group::Member& m : g.members) full = full && (m.b - m.a) * 100 >= length * 95;
            if (full) g.flags |= XPE_BPM_LINE_FULL;
            if (g.width >= 3) g.flags |= XPE_BPM_LINE_REJECT;
            groups.push_back(g);
        }
    }
    // crossings
    for (Group& r : groups) {
        if (r.orient != 0) continue;
        for (Group& c : groups) {
            if (c.orient != 1) continue;
            bool hit = false;   // a row stretch and a column stretch meet when the column lies inside the row stretch and the row inside the column stretch
            for (const Group::Member& rm : r.members)
                for (const Group::Member& cm : c.members) hit = hit || (cm.idx >= rm.a && cm.idx < rm.b && rm.idx >= cm.a && rm.idx < cm.b);
            if (hit) {
                r.flags |= XPE_BPM_LINE_CROSSES;
                c.flags |= XPE_BPM_LINE_CROSSES;
            }
        }
    }
    return groups;
}

}  // namespace

namespace xpe_bpm_v2_internal {
void median_plain(const MedianPlanes& m) { separableMedian(m.in, m.tmp, m.out, m.w, m.h, m.win); }
void median_excluding(const MedianPlanes& m) { separableMedianExcluding(m.in, m.tmp, m.out, m.plain, m.excl, m.w, m.h, m.win); }

double weighted_lack_of_fit(const LofSums& s, bool* ok) {
    const double det = s.w2 * s.w0 - s.w1 * s.w1;
    const bool   finite = std::isfinite(s.w0) && std::isfinite(s.w1) && std::isfinite(s.w2) && std::isfinite(s.wn) && std::isfinite(s.wxn) && std::isfinite(s.wnn);
    if (!finite || !(det > 1e-9 * s.w0 * s.w2)) {
        *ok = false;
        return 0.0;
    }
    const double a = (s.wxn * s.w0 - s.w1 * s.wn) / det;
    const double b = (s.w2 * s.wn - s.w1 * s.wxn) / det;
    const double q = s.wnn - a * s.wxn - b * s.wn;
    *ok = std::isfinite(q);
    return *ok ? std::max(0.0, q) : 0.0;
}

float lack_of_fit_feature(double qprime, uint32_t gEff, double phi, double phiMin) {
    if (!(phi > 0.0) || !std::isfinite(phi) || !std::isfinite(qprime) || gEff < 3u) return 0.0f;
    const double v = std::log(qprime / static_cast<double>(gEff - 2u) + 1e-9) - std::log(std::max(phi, phiMin));
    return std::isfinite(v) ? static_cast<float>(v) : 0.0f;
}
}  // namespace xpe_bpm_v2_internal

/** The detection on frames that hold the imaging area only (no margins). */
static XpeErrorCode generateV2Core(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                   const XpeBpmConfigV2* cfg, XpeImageBuffer* bpm_out, XpeImageBuffer* feature_out, XpeBpmReportV2* report, LineJob* job,
                                   const uint8_t* refExclude = nullptr) try {
    if (!dark_frames || !bright_frames || !bpm_out) return XPE_ERR_INVALID_INPUT;
    Settings st{};
    if (const XpeErrorCode rc = resolve(cfg, &st)) return rc;
    if (num_dark <= st.skip || num_dark - st.skip < st.minDark) return XPE_ERR_INVALID_INPUT;
    if (num_bright < st.minBright) return XPE_ERR_INVALID_INPUT;

    const uint32_t width = dark_frames[0].width, height = dark_frames[0].height;
    if (width < 16 || height < 16) return XPE_ERR_INVALID_INPUT;
    const size_t n = static_cast<size_t>(width) * height;
    if (n > std::numeric_limits<uint32_t>::max()) return XPE_ERR_INVALID_INPUT;
    auto checkInput = [&](const XpeImageBuffer& b) -> XpeErrorCode {
        if (!b.data) return XPE_ERR_INVALID_INPUT;
        if (b.width != width || b.height != height) return XPE_ERR_INVALID_INPUT;
        if (b.format != XPE_PIXEL_UINT16) return XPE_ERR_UNSUPPORTED_FORMAT;
        if (b.dataSize != 0 && b.dataSize < n * sizeof(uint16_t)) return XPE_ERR_INVALID_INPUT;
        return XPE_OK;
    };
    for (uint32_t k = 0; k < num_dark; ++k)
        if (const XpeErrorCode rc = checkInput(dark_frames[k])) return rc;
    for (uint32_t k = 0; k < num_bright; ++k)
        if (const XpeErrorCode rc = checkInput(bright_frames[k])) return rc;
    auto checkOutput = [&](const XpeImageBuffer* b) -> XpeErrorCode {
        if (!b->data) return XPE_ERR_INVALID_INPUT;
        if (b->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
        if (b->width != width || b->height != height || b->dataSize < n) return XPE_ERR_BUFFER_TOO_SMALL;
        return XPE_OK;
    };
    if (const XpeErrorCode rc = checkOutput(bpm_out)) return rc;
    if (feature_out)
        if (const XpeErrorCode rc = checkOutput(feature_out)) return rc;

    const int w = static_cast<int>(width), h = static_cast<int>(height);
    const uint32_t first = st.skip, nd = num_dark - st.skip;
    auto px = [](const XpeImageBuffer& b) { return static_cast<const uint16_t*>(b.data); };

    // ---- dark mean D and the temporal variance s2 (frame and row components removed per frame)
    std::vector<float> D(n), s2(n, 0.0f);
    std::vector<std::vector<double>> rowMeanK, colMeanK;   // per dark frame: the row means and the column means taken out (line persistence)
    std::vector<float>               darkMed;              // line persistence: the temporal median of the dark frames, which a burst in one frame cannot move
    {
        std::vector<uint32_t> sum(n, 0u);
        for (uint32_t k = first; k < num_dark; ++k) {
            const uint16_t* p = px(dark_frames[k]);
            for (size_t i = 0; i < n; ++i) sum[i] += p[i];
        }
        for (size_t i = 0; i < n; ++i) D[i] = static_cast<float>(sum[i]) / static_cast<float>(nd);
        std::vector<double> rowMean(static_cast<size_t>(h)), colMean(static_cast<size_t>(w));
        rowMeanK.resize(nd);
        colMeanK.resize(nd);
        for (uint32_t k = first; k < num_dark; ++k) {
            const uint16_t* p = px(dark_frames[k]);
            for (int y = 0; y < h; ++y) {
                double acc = 0.0;
                for (int x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                    acc += static_cast<double>(p[i]) - static_cast<double>(D[i]);
                }
                rowMean[static_cast<size_t>(y)] = acc / w;
            }
            // the column component of this frame: what is left of the column means after the row means are taken out (a shared offset of a column that changes from frame to frame)
            std::fill(colMean.begin(), colMean.end(), 0.0);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                    colMean[static_cast<size_t>(x)] += static_cast<double>(p[i]) - static_cast<double>(D[i]) - rowMean[static_cast<size_t>(y)];
                }
            for (int x = 0; x < w; ++x) colMean[static_cast<size_t>(x)] /= h;
            rowMeanK[k - first] = rowMean;
            colMeanK[k - first] = colMean;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                    const double e = static_cast<double>(p[i]) - static_cast<double>(D[i]) - rowMean[static_cast<size_t>(y)] - colMean[static_cast<size_t>(x)];
                    s2[i] += static_cast<float>(e * e);
                }
        }
        for (size_t i = 0; i < n; ++i) s2[i] /= static_cast<float>(nd - 1);
    }
    if (job) {
        // The persistence of a stretch is judged against the temporal MEDIAN of the dark frames, not their mean: a burst in one frame moves the mean of the stretch and so makes
        // every other frame look large in the opposite direction (one frame in seven would read as seven in seven). The row and column means taken out of every frame are
        // taken against the same median image.
        darkMed.resize(n);
        std::vector<float> vals(nd);
        for (size_t i = 0; i < n; ++i) {
            for (uint32_t k = 0; k < nd; ++k) vals[k] = static_cast<float>(px(dark_frames[first + k])[i]);
            darkMed[i] = temporalMedian(&vals);
        }
        for (uint32_t k = 0; k < nd; ++k) {
            const uint16_t*     p = px(dark_frames[first + k]);
            std::vector<double> rm(static_cast<size_t>(h), 0.0), cm(static_cast<size_t>(w), 0.0);
            for (int y = 0; y < h; ++y) {
                double acc = 0.0;
                for (int x = 0; x < w; ++x) acc += static_cast<double>(p[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]) - static_cast<double>(darkMed[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]);
                rm[static_cast<size_t>(y)] = acc / w;
            }
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                    cm[static_cast<size_t>(x)] += static_cast<double>(p[i]) - static_cast<double>(darkMed[i]) - rm[static_cast<size_t>(y)];
                }
            for (int x = 0; x < w; ++x) cm[static_cast<size_t>(x)] /= h;
            rowMeanK[k] = rm;
            colMeanK[k] = cm;
        }
    }
    double darkMeanAll = 0.0;
    for (size_t i = 0; i < n; ++i) darkMeanAll += D[i];
    darkMeanAll /= static_cast<double>(n);

    // ---- dose groups: bright frames sorted by their signal, a new group where the ratio exceeds the configured one
    std::vector<double> signal(num_bright);
    for (uint32_t k = 0; k < num_bright; ++k) {
        const uint16_t* p = px(bright_frames[k]);
        double acc = 0.0;
        for (size_t i = 0; i < n; ++i) acc += p[i];
        signal[k] = acc / static_cast<double>(n) - darkMeanAll;
        if (!(signal[k] > 0.0)) return XPE_ERR_INVALID_INPUT;
    }
    // the exposure scale of the noise-cost statistic uses a dose that defective pixels cannot pull: the median of the net signal over every fifth pixel (QA-A-252h)
    std::vector<double> dose(num_bright);
    {
        std::vector<float> smp;
        smp.reserve(n / 5 + 1);
        for (uint32_t k = 0; k < num_bright; ++k) {
            const uint16_t* p = px(bright_frames[k]);
            smp.clear();
            for (size_t i = 0; i < n; i += 5) smp.push_back(static_cast<float>(p[i]) - D[i]);
            std::nth_element(smp.begin(), smp.begin() + static_cast<std::ptrdiff_t>(smp.size() / 2), smp.end());
            dose[k] = static_cast<double>(smp[smp.size() / 2]);
            if (!(dose[k] > 0.0)) return XPE_ERR_INVALID_INPUT;
        }
    }
    std::vector<uint32_t> order(num_bright);
    for (uint32_t k = 0; k < num_bright; ++k) order[k] = k;
    std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return signal[a] < signal[b]; });
    std::vector<std::vector<uint32_t>> groups(1);
    for (uint32_t t = 0; t < num_bright; ++t) {
        if (t > 0 && signal[order[t]] / signal[order[t - 1]] > st.ratio) groups.emplace_back();
        groups.back().push_back(order[t]);
    }
    const uint32_t G = static_cast<uint32_t>(groups.size());
    if (G > XPE_BPM_V2_MAX_DOSE_GROUPS) return XPE_ERR_INVALID_INPUT;
    if (job) {   // lines: the frames of the persistence judgement (reported)
        job->darkRaw = num_dark;
        job->darkValid = nd;
        job->brightRaw = num_bright;
        job->brightValid = 0;
        for (const auto& grp : groups)
            if (grp.size() >= 3) job->brightValid += static_cast<uint32_t>(grp.size());
    }

    // ---- the gain correction of the noise-cost judgement (QA-A-252f, P2): the measured response of every pixel, the relative response of the offset-corrected bright frames against
    // its local median (the quantity of the response rule), squared and inverted. It is a measurement, not the model 1 / r: the variance of a pixel is divided by the square of what
    // the pixel actually passes on. The response is bounded to 0.2 .. 3 (a pixel below 0.45 is a response defect whatever its noise).
    std::vector<float> corrGain(n, 1.0f);
    {
        std::vector<float>  flat(n, 0.0f), tmpP(n), bgP(n);
        std::vector<double> accP(n);
        double              sxPre = 0.0;
        for (uint32_t g = 0; g < G; ++g) {
            std::fill(accP.begin(), accP.end(), 0.0);
            double sg = 0.0;
            for (uint32_t k : groups[g]) {
                const uint16_t* p = px(bright_frames[k]);
                for (size_t i = 0; i < n; ++i) accP[i] += p[i];
                sg += signal[k];
            }
            const double mgd = static_cast<double>(groups[g].size());
            sxPre += sg / mgd;
            for (size_t i = 0; i < n; ++i) flat[i] += static_cast<float>(accP[i] / mgd - static_cast<double>(D[i]));
        }
        for (size_t i = 0; i < n; ++i) flat[i] = static_cast<float>(static_cast<double>(flat[i]) / sxPre);
        separableMedian(flat.data(), tmpP.data(), bgP.data(), w, h, static_cast<int>(st.window));
        for (size_t i = 0; i < n; ++i) {
            const double b = static_cast<double>(bgP[i]);
            const double r = std::fabs(b) > 1e-12 ? static_cast<double>(flat[i]) / b : 1.0;
            const double rc = std::min(3.0, std::max(0.2, r));
            corrGain[i] = static_cast<float>(1.0 / (rc * rc));
        }
    }

    // ---- per-pixel least-squares sums over the groups: n_g = a * S_g + b
    std::vector<double> Sn(n, 0.0), Sxn(n, 0.0), Snn(n, 0.0);
    double Sx = 0.0, Sxx = 0.0;
    std::vector<double> groupSignal(G, 0.0);
    // bright-frame temporal noise inside the dose groups (QA-A-252b, F2b): per group the variance of the frames about the group mean (the row and column means of every frame
    // taken out, as for the dark frames), relative to its own local median; the groups are pooled as a sum of chi-square variables
    std::vector<float>   noiseSum, refSum;
    // QA-A-258: the lack of fit of the line detector's LINEARITY cause. Weights w_g = m_g / v0_g do not depend on the pixel, so only three sums per pixel are needed; the pixel's own pure error is
    // pooled over the dose groups against the nominal variance (own[i] = sum nu_g s2_g / v0_g). A pixel with a saturated group mean is marked with own < -1e29 and judged by no evidence.
    // @MX:NOTE: [AUTO] the pooling assumes the SHAPE of the variance over the doses is the nominal one and only its size is the pixel's own (definition file, section 3)
    constexpr float      kLofSaturated = -1e30f;
    constexpr float      kLofSaturationLevel = 65500.0f;
    std::vector<double>  lofWn, lofWxn, lofWnn;
    std::vector<float>   lofOwn;
    xpe_bpm_v2_internal::LofSums lofScalars{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    uint32_t             lofGroups = 0, lofNu = 0;
    if (job) {
        lofWn.assign(n, 0.0);
        lofWxn.assign(n, 0.0);
        lofWnn.assign(n, 0.0);
        lofOwn.assign(n, 0.0f);
    }
    std::vector<uint8_t> persistHits;   // in how many dose groups the gain-corrected variance of the pixel reaches the limit (P2: the position must persist)
    uint32_t           nuBright = 0, noiseGroups = 0;
    std::vector<double> frameScale(num_bright, 1.0);   // f_k = mean signal of the group / signal of frame k (QA-A-252g: the exposure variation is not a pixel noise)
    std::vector<std::vector<double>> brmK(num_bright), bcmK(num_bright);   // per bright frame: the row and column means taken out (line persistence)
    std::vector<double>              groupNom(groups.size(), 0.0);          // per dose group: the nominal temporal variance
    {
        std::vector<float> acc(n);
        std::vector<float> plainRef;   // QA-A-252i: the plain separable median, the fallback of a pixel whose whole window is excluded
        std::vector<float> vg, vgTmp, vgBg, gmean;   // gmean: the group mean of the frames scaled to the mean dose of the group
        for (uint32_t g = 0; g < G; ++g) {
            std::fill(acc.begin(), acc.end(), 0.0f);
            double sg = 0.0;
            for (uint32_t k : groups[g]) {
                const uint16_t* p = px(bright_frames[k]);
                for (size_t i = 0; i < n; ++i) acc[i] += static_cast<float>(p[i]);
                sg += signal[k];
            }
            sg /= static_cast<double>(groups[g].size());
            double dg = 0.0;
            for (uint32_t k : groups[g]) dg += dose[k];
            dg /= static_cast<double>(groups[g].size());
            for (uint32_t k : groups[g]) frameScale[k] = dg / dose[k];
            groupSignal[g] = sg;
            Sx += sg;
            Sxx += sg * sg;
            const float inv = 1.0f / static_cast<float>(groups[g].size());
            for (size_t i = 0; i < n; ++i) {
                const double ng = static_cast<double>(acc[i] * inv - D[i]);
                Sn[i] += ng;
                Sxn[i] += sg * ng;
                Snn[i] += ng * ng;
            }
            const uint32_t mg = static_cast<uint32_t>(groups[g].size());
            if (mg >= 2) {
                if (noiseSum.empty()) {
                    noiseSum.assign(n, 0.0f);
                    refSum.assign(n, 0.0f);
                    persistHits.assign(n, 0);
                    vg.resize(n);
                    vgTmp.resize(n);
                    vgBg.resize(n);
                    gmean.resize(n);
                }
                std::fill(vg.begin(), vg.end(), 0.0f);
                std::fill(gmean.begin(), gmean.end(), 0.0f);
                for (uint32_t k : groups[g]) {
                    const uint16_t* p = px(bright_frames[k]);
                    const double    fk = frameScale[k] / static_cast<double>(mg);
                    for (size_t i = 0; i < n; ++i) gmean[i] += static_cast<float>(fk * (static_cast<double>(p[i]) - static_cast<double>(D[i])));
                }
                std::vector<double> rm(static_cast<size_t>(height)), cm(static_cast<size_t>(width));
                for (uint32_t k : groups[g]) {
                    const uint16_t* p = px(bright_frames[k]);
                    const double    fk = frameScale[k];
                    for (uint32_t y = 0; y < height; ++y) {
                        double a = 0.0;
                        for (uint32_t x = 0; x < width; ++x) {
                            const size_t i = static_cast<size_t>(y) * width + x;
                            a += fk * (static_cast<double>(p[i]) - static_cast<double>(D[i])) - static_cast<double>(gmean[i]);
                        }
                        rm[y] = a / width;
                    }
                    std::fill(cm.begin(), cm.end(), 0.0);
                    for (uint32_t y = 0; y < height; ++y)
                        for (uint32_t x = 0; x < width; ++x) {
                            const size_t i = static_cast<size_t>(y) * width + x;
                            cm[x] += fk * (static_cast<double>(p[i]) - static_cast<double>(D[i])) - static_cast<double>(gmean[i]) - rm[y];
                        }
                    for (uint32_t x = 0; x < width; ++x) cm[x] /= height;
                    brmK[k] = rm;
                    bcmK[k] = cm;
                    for (uint32_t y = 0; y < height; ++y)
                        for (uint32_t x = 0; x < width; ++x) {
                            const size_t i = static_cast<size_t>(y) * width + x;
                            const double e = fk * (static_cast<double>(p[i]) - static_cast<double>(D[i])) - static_cast<double>(gmean[i]) - rm[y] - cm[x];
                            vg[i] += static_cast<float>(e * e);
                        }
                }
                for (size_t i = 0; i < n; ++i) vg[i] /= static_cast<float>(mg - 1);
                if (job && mg >= 3) {
                    // line persistence: the row and column means of every frame against the temporal median of the group (groups of three or more only)
                    std::vector<float> gmed(n), vals(mg);
                    for (size_t i = 0; i < n; ++i) {
                        for (uint32_t j = 0; j < mg; ++j) vals[j] = static_cast<float>(frameScale[groups[g][j]] * (static_cast<double>(px(bright_frames[groups[g][j]])[i]) - static_cast<double>(D[i])));
                        gmed[i] = temporalMedian(&vals);
                    }
                    for (uint32_t k : groups[g]) {
                        const uint16_t*     pk = px(bright_frames[k]);
                        std::vector<double> rm2(static_cast<size_t>(height), 0.0), cm2(static_cast<size_t>(width), 0.0);
                        for (uint32_t y = 0; y < height; ++y) {
                            double acc2 = 0.0;
                            for (uint32_t x = 0; x < width; ++x) {
                                const size_t ii = static_cast<size_t>(y) * width + x;
                                acc2 += frameScale[k] * (static_cast<double>(pk[ii]) - static_cast<double>(D[ii])) - static_cast<double>(gmed[ii]);
                            }
                            rm2[y] = acc2 / width;
                        }
                        for (uint32_t y = 0; y < height; ++y)
                            for (uint32_t x = 0; x < width; ++x) {
                                const size_t i = static_cast<size_t>(y) * width + x;
                                cm2[x] += frameScale[k] * (static_cast<double>(pk[i]) - static_cast<double>(D[i])) - static_cast<double>(gmed[i]) - rm2[y];
                            }
                        for (uint32_t x = 0; x < width; ++x) cm2[x] /= height;
                        brmK[k] = rm2;
                        bcmK[k] = cm2;
                    }
                }
                if (refExclude != nullptr) {   // QA-A-252i: the reference does not see the pixels of the detector's own line mask
                    separableMedian(vg.data(), vgTmp.data(), vgBg.data(), w, h, static_cast<int>(st.window));
                    if (plainRef.empty()) plainRef.resize(n);
                    std::copy(vgBg.begin(), vgBg.end(), plainRef.begin());
                    separableMedianExcluding(vg.data(), vgTmp.data(), vgBg.data(), plainRef.data(), refExclude, w, h, static_cast<int>(st.window));
                } else {
                    separableMedian(vg.data(), vgTmp.data(), vgBg.data(), w, h, static_cast<int>(st.window));
                }
                const double mnu = varianceMedian(mg - 1);
                std::vector<float> nomSample;
                nomSample.reserve(n / 16 + 1);
                for (size_t i = 0; i < n; ++i) {
                    const double ref = std::max(static_cast<double>(vgBg[i]) / mnu, kReferenceFloor);
                    const double ratioG = static_cast<double>(vg[i]) * static_cast<double>(corrGain[i]) / ref;   // gain-corrected variance against its local expectation (P2)
                    noiseSum[i] += static_cast<float>((mg - 1) * ratioG);
                    refSum[i] += static_cast<float>(ref);
                    if (ratioG >= kNoiseCostLimit) ++persistHits[i];
                    if (i % 16 == 0) nomSample.push_back(static_cast<float>(ref));
                }
                groupNom[g] = static_cast<double>(medianOf(nomSample.data(), nomSample.size()));
                if (job && groupNom[g] > 0.0) {   // QA-A-258: the weighted sums of the lack of fit (the weights need the nominal variance of this group)
                    const double wg = static_cast<double>(mg) / groupNom[g];
                    const double nuOverV0 = static_cast<double>(mg - 1) / groupNom[g];
                    for (size_t i = 0; i < n; ++i) {
                        const float mean = acc[i] * inv;
                        const double ng = static_cast<double>(mean - D[i]);
                        lofWn[i] += wg * ng;
                        lofWxn[i] += wg * sg * ng;
                        lofWnn[i] += wg * ng * ng;
                        if (mean >= kLofSaturationLevel) lofOwn[i] = kLofSaturated;
                        else if (lofOwn[i] > kLofSaturated * 0.5f) lofOwn[i] += static_cast<float>(nuOverV0 * static_cast<double>(vg[i]));
                    }
                    lofScalars.w0 += wg;
                    lofScalars.w1 += wg * sg;
                    lofScalars.w2 += wg * sg * sg;
                    ++lofGroups;
                    lofNu += mg - 1;
                }
                nuBright += mg - 1;
                ++noiseGroups;
            }
        }
    }

    if (report) *report = XpeBpmReportV2{};
    std::vector<uint8_t> flags(n, 0);
    Work wk{w, h, n, std::vector<float>(n), std::vector<float>(n), std::vector<float>(n)};
    uint32_t counts[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0, 0};
    float    scaleMed[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0, 0};
    double   threshold[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0, 0};
    uint32_t nu[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0, 0};
    uint32_t active[XPE_BPM_V2_FEATURE_COUNT] = {1, 1, 0, 0, 1, 0};
    const double p = st.falseAlarms / static_cast<double>(n);
    const double lambdaTwoSided = zThreshold(p);

    // F1 dark mean
    std::copy(D.begin(), D.end(), wk.feat.begin());
    threshold[0] = lambdaTwoSided;
    if (!(job && job->linesOnly)) scaleMed[0] = flagRobustZ(wk, st, lambdaTwoSided, false, false, XPE_BPM_V2_FEATURE_DARK_MEAN, &flags, &counts[0]);

    // F2 dark temporal noise
    nu[1] = nd - 1;
    threshold[1] = varianceThreshold(nu[1], p);
    std::copy(s2.begin(), s2.end(), wk.feat.begin());
    scaleMed[1] = flagVarianceRatio(wk, st, nu[1], threshold[1], XPE_BPM_V2_FEATURE_DARK_NOISE, &flags, &counts[1]);
    if (job) {   // lines: the variance ratio of the dark frames against its local median, before any component is removed
        const double       mnu = varianceMedian(nu[1]);
        std::vector<float> t(n);
        for (size_t i = 0; i < n; ++i) t[i] = static_cast<float>(static_cast<double>(wk.feat[i]) / std::max(static_cast<double>(wk.bg[i]) / mnu, kReferenceFloor));
        // persistence: per dark frame, the mean of the squared residual over the stretch (each pixel at most 16 times its local variance) against the median of the same
        // pixels in the neighbouring lines; the frame counts when the ratio reaches the tolerance. Returns the share of the dark frames that count.
        job->persistence = [&](int orient, int idx, int a, int b) -> float {   // NOLINT(bugprone-easily-swappable-parameters): orient is 0 (row) or 1 (column), idx the line, [a, b) the stretch; a lambda local to this function, called at one site as persistence(orient, y, a, b)
            const int half = static_cast<int>(st.window) / 2;
            const int lines = orient == 0 ? h : w;
            int       hits = 0;
            for (uint32_t k = 0; k < nd; ++k) {
                const uint16_t* p = px(dark_frames[first + k]);
                auto lineStat = [&](int line) {
                    double acc = 0.0;
                    for (int t2 = a; t2 < b; ++t2) {
                        const int    x = orient == 0 ? t2 : line, y = orient == 0 ? line : t2;
                        const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                        const double e = static_cast<double>(p[i]) - static_cast<double>(darkMed[i]) - rowMeanK[k][static_cast<size_t>(y)] - colMeanK[k][static_cast<size_t>(x)];
                        acc += std::min(e * e, 16.0 * static_cast<double>(wk.bg[i]));
                    }
                    return acc;
                };
                const double       sf = lineStat(idx);
                std::vector<float> nb;
                for (int d = -half; d <= half; ++d)
                    if (d != 0 && idx + d >= 0 && idx + d < lines) nb.push_back(static_cast<float>(lineStat(idx + d)));
                const double sr = nb.empty() ? sf : static_cast<double>(medianOf(nb.data(), nb.size()));
                if (sr > 0.0 && sf / sr >= job->tol) ++hits;
            }
            return static_cast<float>(hits) / static_cast<float>(nd);
        };
        analyzeFeature(t.data(), w, h, st, job, kKindNoise);
        job->persistence = nullptr;
    }

    // F3a slope and F3b linearity
    if (G >= 2) {
        // The slope feature is retired as a pixel feature (QA-A-252d): a response is judged by the ratio rule on the flat-field response (F4). It stays as a line feature.
        const double dgx = static_cast<double>(G) * Sxx - Sx * Sx;
        if (job) {
            for (size_t i = 0; i < n; ++i) wk.feat[i] = static_cast<float>((static_cast<double>(G) * Sxn[i] - Sx * Sn[i]) / dgx);
            job->responseCause = XPE_BPM_LINE_CAUSE_SLOPE;   // the slope is a proxy of the response (it is the regression slope, not the ratio of the offset-corrected image): its own cause
            analyzeFeature(wk.feat.data(), w, h, st, job, kKindResponse);
            job->responseCause = XPE_BPM_LINE_CAUSE_RESPONSE;
        }
        if (G >= 3) {
            // The noise of the group means differs from group to group (shot noise grows with the dose, the groups hold different numbers of frames),
            // so the residual of the unweighted fit is not chi-square distributed; the log of the residual variance is judged by the robust z machinery instead.
            active[3] = 1;
            threshold[3] = zThreshold(2.0 * p);   // one side: P(z > lambda) = p
            for (size_t i = 0; i < n; ++i) {
                const double a = (static_cast<double>(G) * Sxn[i] - Sx * Sn[i]) / dgx;
                const double b = (Sn[i] - a * Sx) / static_cast<double>(G);
                const double sse = std::max(0.0, Snn[i] - a * Sxn[i] - b * Sn[i]);
                wk.feat[i] = static_cast<float>(std::log(sse / static_cast<double>(G - 2) + 1e-9));
            }
            if (job) {   // lines (QA-A-258): the lack of fit of the straight line through the group means, weighted by the nominal variance of the means and divided by the pixel's own pooled temporal variance.
                // Not available (fewer than three groups with a pure error, pooled degrees of freedom below 5, degenerate doses): the LINEARITY cause is not evaluated at all; the old statistic is NOT used instead.
                constexpr double kLofPhiMin = 0.1;
                constexpr uint32_t kLofMinNu = 5;
                if (lofGroups >= 3 && lofNu >= kLofMinNu) {
                    bool        okSums = true;
                    std::vector<float> rel(n, 0.0f);
                    const double nuD = static_cast<double>(lofNu);
                    for (size_t i = 0; i < n; ++i) {
                        if (!(lofOwn[i] > kLofSaturated * 0.5f)) continue;   // saturated: no evidence
                        xpe_bpm_v2_internal::LofSums sums = lofScalars;
                        sums.wn = lofWn[i];
                        sums.wxn = lofWxn[i];
                        sums.wnn = lofWnn[i];
                        bool        ok = true;
                        const double q = xpe_bpm_v2_internal::weighted_lack_of_fit(sums, &ok);
                        if (!ok) {
                            okSums = false;
                            break;   // the doses are degenerate (the same for every pixel): nothing to evaluate
                        }
                        rel[i] = xpe_bpm_v2_internal::lack_of_fit_feature(q, lofGroups, static_cast<double>(lofOwn[i]) / nuD, kLofPhiMin);
                    }
                    if (okSums) analyzeFeature(rel.data(), w, h, st, job, kKindLinearity);
                }
            }
            if (!(job && job->linesOnly)) scaleMed[3] = flagRobustZ(wk, st, threshold[3], true, false, XPE_BPM_V2_FEATURE_DOSE_LINEAR, &flags, &counts[3]);
        }
    }

    // F2b bright temporal noise inside the dose groups (needs a group of two frames or more)
    if (nuBright > 0) {
        active[5] = 1;
        nu[5] = nuBright;
        threshold[5] = varianceThreshold(nuBright, p);
        std::vector<float> sample;
        sample.reserve(n / 16 + 1);
        for (size_t i = 0; i < n; ++i) {
            if (static_cast<double>(noiseSum[i]) / static_cast<double>(nuBright) > threshold[5] && 2u * persistHits[i] >= noiseGroups) {
                flags[i] = static_cast<uint8_t>(flags[i] | XPE_BPM_V2_FEATURE_BRIGHT_NOISE);
                ++counts[5];
            }
            if (i % 16 == 0) sample.push_back(refSum[i] / static_cast<float>(noiseGroups));
        }
        scaleMed[5] = medianOf(sample.data(), sample.size());
        if (job) {   // lines: the pooled variance ratio of the bright frames
            std::vector<float> t(n);
            for (size_t i = 0; i < n; ++i) t[i] = noiseSum[i] / static_cast<float>(nuBright);
            // persistence over the bright frames: per frame the squared residual about its dose-group mean (row and column means out, each pixel at most 16 times the nominal
            // variance of the group), averaged over the stretch, against the median of the same pixels in the neighbouring lines; the share of frames that reach the tolerance
            job->persistence = [&](int orient, int idx, int a, int b) -> float {   // NOLINT(bugprone-easily-swappable-parameters): orient is 0 (row) or 1 (column), idx the line, [a, b) the stretch; a lambda local to this function, called at one site as persistence(orient, y, a, b)
                const int half = static_cast<int>(st.window) / 2;
                const int lines = orient == 0 ? h : w;
                int       frames = 0, hits = 0;
                for (size_t g = 0; g < groups.size(); ++g) {
                    const uint32_t mg = static_cast<uint32_t>(groups[g].size());
                    // A pair has no median (its reference is its mean, so both frames read as large when one carries a burst) and cannot tell which frame is odd: the frames of
                    // groups of fewer than three count neither in the numerator nor in the denominator (QA-A-253f)
                    if (mg < 3) continue;
                    const double cap = 16.0 * groupNom[g];
                    // squared residual about the temporal median of the group, summed over the stretch, for the line and its neighbours, every frame at once
                    std::vector<std::vector<double>> stat(mg, std::vector<double>(static_cast<size_t>(2 * half + 1), 0.0));
                    std::vector<float>               vals(mg);
                    for (int d = -half; d <= half; ++d) {
                        const int line = idx + d;
                        if (line < 0 || line >= lines) continue;
                        for (int t2 = a; t2 < b; ++t2) {
                            const int    x = orient == 0 ? t2 : line, y = orient == 0 ? line : t2;
                            const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                            for (uint32_t j = 0; j < mg; ++j) vals[j] = static_cast<float>(frameScale[groups[g][j]] * (static_cast<double>(px(bright_frames[groups[g][j]])[i]) - static_cast<double>(D[i])));
                            std::vector<float> srt(vals);
                            const double       md = static_cast<double>(temporalMedian(&srt));
                            for (uint32_t j = 0; j < mg; ++j) {
                                const uint32_t k = groups[g][j];
                                const double   e = static_cast<double>(vals[j]) - md - brmK[k][static_cast<size_t>(y)] - bcmK[k][static_cast<size_t>(x)];
                                stat[j][static_cast<size_t>(d) + static_cast<size_t>(half)] += std::min(e * e * static_cast<double>(corrGain[i]), cap);
                            }
                        }
                    }
                    for (uint32_t j = 0; j < mg; ++j) {
                        const double       sf = stat[j][static_cast<size_t>(half)];
                        std::vector<float> nb;
                        for (int d = -half; d <= half; ++d)
                            if (d != 0 && idx + d >= 0 && idx + d < lines) nb.push_back(static_cast<float>(stat[j][static_cast<size_t>(d) + static_cast<size_t>(half)]));
                        const double sr = nb.empty() ? sf : static_cast<double>(medianOf(nb.data(), nb.size()));
                        ++frames;
                        if (sr > 0.0 && sf / sr >= job->tol) ++hits;
                    }
                }
                return frames >= 3 ? static_cast<float>(hits) / static_cast<float>(frames) : -1.0f;   // fewer than three frames cannot show a majority
            };
            analyzeFeature(t.data(), w, h, st, job, kKindNoise);
            job->persistence = nullptr;
        }
    }

    // F4 relative flat-field response
    for (size_t i = 0; i < n; ++i) wk.feat[i] = static_cast<float>(Sn[i] / Sx);
    threshold[4] = static_cast<float>(kResponseLow);
    if (job) {
        job->countWeak = true;   // the weak-response stretches are counted on this feature only (the slope feature sees the same lines)
        analyzeFeature(wk.feat.data(), w, h, st, job, kKindResponse);
        job->countWeak = false;
    }
    uint32_t weakResponse = 0;
    if (!(job && job->linesOnly)) scaleMed[4] = flagResponseRatio(wk, st, kResponseLow, kResponseHigh, kWeakResponseHigh, XPE_BPM_V2_FEATURE_FLAT, &flags, &counts[4], &weakResponse);

    // ---- output
    uint8_t* out = static_cast<uint8_t*>(bpm_out->data);
    uint32_t unionCount = 0;
    const uint8_t brightBits = XPE_BPM_V2_FEATURE_DOSE_SLOPE | XPE_BPM_V2_FEATURE_DOSE_LINEAR | XPE_BPM_V2_FEATURE_FLAT | XPE_BPM_V2_FEATURE_BRIGHT_NOISE;
    const uint8_t darkBits = XPE_BPM_V2_FEATURE_DARK_MEAN | XPE_BPM_V2_FEATURE_DARK_NOISE;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t f = flags[i];
        out[i] = static_cast<uint8_t>(((f & brightBits) ? 1 : 0) | ((f & darkBits) ? 2 : 0));
        unionCount += f != 0;
    }
    if (feature_out) std::memcpy(feature_out->data, flags.data(), n);
    if (report) {
        report->pixels = static_cast<uint32_t>(n);
        report->dark_frames_used = nd;
        report->bright_frames_used = num_bright;
        report->dose_groups = G;
        for (int f = 0; f < XPE_BPM_V2_FEATURE_COUNT; ++f) {
            report->feature_active[f] = active[f];
            report->degrees_of_freedom[f] = nu[f];
            report->threshold[f] = static_cast<float>(threshold[f]);
            report->scale_median[f] = scaleMed[f];
            report->flagged[f] = counts[f];
        }
        report->flagged_union = unionCount;
        report->weak_response_pixels = weakResponse;
        report->response_ratio_low = static_cast<float>(kResponseLow);
        report->response_ratio_high = static_cast<float>(kResponseHigh);
        for (uint32_t g = 0; g < G; ++g) {
            report->group_signal[g] = static_cast<float>(groupSignal[g]);
            report->group_frames[g] = static_cast<uint32_t>(groups[g].size());
        }
    }
    return XPE_OK;
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_INTERNAL;
}

/**
 * The entry with margins. Pixels in the margins (outside the imaging area) are cut away before anything is computed, so no statistic of the detection sees them;
 * outside the area the map and the feature bits are 0 and area_out tells. A line job (QA-A-253) collects the stretches with their coordinates shifted back.
 */
static XpeErrorCode runWithMargins(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                   const XpeBpmConfigV2* cfg, XpeImageBuffer* bpm_out, XpeImageBuffer* feature_out, XpeImageBuffer* area_out, XpeBpmReportV2* report,
                                   LineJob* job, const uint8_t* refExclude = nullptr) try {
    const uint32_t ml = cfg ? cfg->margin_left : 0u, mt = cfg ? cfg->margin_top : 0u, mr = cfg ? cfg->margin_right : 0u, mb = cfg ? cfg->margin_bottom : 0u;
    if (ml == 0 && mt == 0 && mr == 0 && mb == 0) {
        if (area_out && bpm_out) {
            if (!area_out->data) return XPE_ERR_INVALID_INPUT;
            if (area_out->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
            if (area_out->width != bpm_out->width || area_out->height != bpm_out->height || area_out->dataSize < static_cast<size_t>(bpm_out->width) * bpm_out->height) return XPE_ERR_BUFFER_TOO_SMALL;
        }
        const XpeErrorCode rc = generateV2Core(dark_frames, num_dark, bright_frames, num_bright, cfg, bpm_out, feature_out, report, job, refExclude);
        if (rc == XPE_OK) {
            if (report) report->out_of_area_pixels = 0;
            if (area_out && bpm_out) std::memset(area_out->data, 1, static_cast<size_t>(bpm_out->width) * bpm_out->height);
        }
        return rc;
    }
    if (!dark_frames || !bright_frames || !bpm_out || num_dark == 0 || num_bright == 0) return XPE_ERR_INVALID_INPUT;
    const uint32_t width = dark_frames[0].width, height = dark_frames[0].height;
    if (width < 16 || height < 16 || ml > width || mr > width || mt > height || mb > height) return XPE_ERR_INVALID_INPUT;
    if (width - ml < mr + 16 || height - mt < mb + 16) return XPE_ERR_INVALID_INPUT;
    const uint32_t cw = width - ml - mr, ch = height - mt - mb;
    const size_t   n = static_cast<size_t>(width) * height, cn = static_cast<size_t>(cw) * ch;
    auto check = [&](const XpeImageBuffer& b) -> XpeErrorCode {
        if (!b.data) return XPE_ERR_INVALID_INPUT;
        if (b.width != width || b.height != height) return XPE_ERR_INVALID_INPUT;
        if (b.format != XPE_PIXEL_UINT16) return XPE_ERR_UNSUPPORTED_FORMAT;
        if (b.dataSize != 0 && b.dataSize < n * sizeof(uint16_t)) return XPE_ERR_INVALID_INPUT;
        return XPE_OK;
    };
    for (uint32_t k = 0; k < num_dark; ++k)
        if (const XpeErrorCode rc = check(dark_frames[k])) return rc;
    for (uint32_t k = 0; k < num_bright; ++k)
        if (const XpeErrorCode rc = check(bright_frames[k])) return rc;
    auto checkOut = [&](const XpeImageBuffer* b) -> XpeErrorCode {
        if (!b->data) return XPE_ERR_INVALID_INPUT;
        if (b->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
        if (b->width != width || b->height != height || b->dataSize < n) return XPE_ERR_BUFFER_TOO_SMALL;
        return XPE_OK;
    };
    if (const XpeErrorCode rc = checkOut(bpm_out)) return rc;
    if (feature_out)
        if (const XpeErrorCode rc = checkOut(feature_out)) return rc;
    if (area_out)
        if (const XpeErrorCode rc = checkOut(area_out)) return rc;

    auto crop = [&](const XpeImageBuffer& src, std::vector<uint16_t>* dst) {
        dst->resize(cn);
        const uint16_t* p = static_cast<const uint16_t*>(src.data);
        for (uint32_t y = 0; y < ch; ++y)
            std::memcpy(dst->data() + static_cast<size_t>(y) * cw, p + static_cast<size_t>(y + mt) * width + ml, static_cast<size_t>(cw) * sizeof(uint16_t));
    };
    auto cutBuf = [&](std::vector<uint16_t>& v) {
        XpeImageBuffer b{};
        b.data = v.data();
        b.width = cw;
        b.height = ch;
        b.bitsAllocated = 16;
        b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.dataSize = v.size() * sizeof(uint16_t);
        return b;
    };
    std::vector<std::vector<uint16_t>> cd(num_dark), cb(num_bright);
    std::vector<XpeImageBuffer>        cdb(num_dark), cbb(num_bright);
    for (uint32_t k = 0; k < num_dark; ++k) {
        crop(dark_frames[k], &cd[k]);
        cdb[k] = cutBuf(cd[k]);
    }
    for (uint32_t k = 0; k < num_bright; ++k) {
        crop(bright_frames[k], &cb[k]);
        cbb[k] = cutBuf(cb[k]);
    }
    std::vector<uint8_t> cmap(cn, 0), cfeat(feature_out ? cn : 0, 0);
    XpeImageBuffer       outMap{}, outFeat{};
    outMap.data = cmap.data();
    outMap.width = cw;
    outMap.height = ch;
    outMap.bitsAllocated = 8;
    outMap.bitsStored = 8;
    outMap.format = XPE_PIXEL_UINT8;
    outMap.dataSize = cmap.size();
    if (feature_out) {
        outFeat = outMap;
        outFeat.data = cfeat.data();
    }
    std::vector<uint8_t> cexcl;
    if (refExclude != nullptr) {
        cexcl.resize(cn);
        for (uint32_t y = 0; y < ch; ++y) std::memcpy(cexcl.data() + static_cast<size_t>(y) * cw, refExclude + static_cast<size_t>(y + mt) * width + ml, cw);
    }
    const XpeErrorCode rc = generateV2Core(cdb.data(), num_dark, cbb.data(), num_bright, cfg, &outMap, feature_out ? &outFeat : nullptr, report, job, refExclude != nullptr ? cexcl.data() : nullptr);
    if (rc != XPE_OK) return rc;
    if (job)
        for (Seg& sg : job->segs) {   // back to frame coordinates
            const int along = sg.orient == 0 ? static_cast<int>(ml) : static_cast<int>(mt);
            const int across = sg.orient == 0 ? static_cast<int>(mt) : static_cast<int>(ml);
            sg.idx += across;
            sg.a += along;
            sg.b += along;
        }
    uint8_t* mapDst = static_cast<uint8_t*>(bpm_out->data);
    std::memset(mapDst, 0, n);   // outside the imaging area the map is 0 (good): the defect codes keep their meaning, the area has its own output
    for (uint32_t y = 0; y < ch; ++y) std::memcpy(mapDst + static_cast<size_t>(y + mt) * width + ml, cmap.data() + static_cast<size_t>(y) * cw, cw);
    if (feature_out) {
        uint8_t* featDst = static_cast<uint8_t*>(feature_out->data);
        std::memset(featDst, 0, n);
        for (uint32_t y = 0; y < ch; ++y) std::memcpy(featDst + static_cast<size_t>(y + mt) * width + ml, cfeat.data() + static_cast<size_t>(y) * cw, cw);
    }
    if (area_out) {
        uint8_t* areaDst = static_cast<uint8_t*>(area_out->data);
        std::memset(areaDst, 0, n);
        for (uint32_t y = 0; y < ch; ++y) std::memset(areaDst + static_cast<size_t>(y + mt) * width + ml, 1, cw);
    }
    if (report) report->out_of_area_pixels = static_cast<uint32_t>(n - cn);
    return XPE_OK;
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_INTERNAL;
}

extern "C" XPE_API XpeErrorCode xpe_bpm_generate_v2(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                                    const XpeBpmConfigV2* cfg, XpeImageBuffer* bpm_out, XpeImageBuffer* feature_out, XpeImageBuffer* area_out,
                                                    XpeBpmReportV2* report) try {
    // QA-A-252i: the line detection comes first and its mask (the painted union of the confirmed stretches) is kept out of the local expectation of the pixel noise check. The lines-only pass
    // does not use the exclusion, so the mask does not depend on it. A failure of that pass leaves the pixel pass as it was (it reports its own errors).
    std::vector<uint8_t> lineMask;
    if (dark_frames && bright_frames && num_dark > 0 && num_bright > 0 && dark_frames[0].width > 0 && dark_frames[0].height > 0) {
        const size_t n = static_cast<size_t>(dark_frames[0].width) * dark_frames[0].height;
        lineMask.assign(n, 0);
        XpeImageBuffer lb{};
        lb.data = lineMask.data();
        lb.width = dark_frames[0].width;
        lb.height = dark_frames[0].height;
        lb.bitsAllocated = 8;
        lb.bitsStored = 8;
        lb.format = XPE_PIXEL_UINT8;
        lb.dataSize = n;
        if (xpe_bpm_find_lines(dark_frames, num_dark, bright_frames, num_bright, cfg, nullptr, &lb, nullptr, 0, nullptr, 0, nullptr) != XPE_OK ||
            std::none_of(lineMask.begin(), lineMask.end(), [](uint8_t b) { return b != 0; }))
            lineMask.clear();
    }
    return runWithMargins(dark_frames, num_dark, bright_frames, num_bright, cfg, bpm_out, feature_out, area_out, report, nullptr, lineMask.empty() ? nullptr : lineMask.data());
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_INTERNAL;
}

extern "C" XPE_API XpeErrorCode xpe_bpm_find_lines(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                                   const XpeBpmConfigV2* cfg, const XpeBpmLineConfig* lcfg, XpeImageBuffer* line_mask_out, XpeBpmLine* lines_out,
                                                   uint32_t max_lines, XpeBpmLine* unconfirmed_out, uint32_t max_unconfirmed, XpeBpmLineReport* report) try {
    if (!dark_frames || !bright_frames || num_dark == 0 || num_bright == 0) return XPE_ERR_INVALID_INPUT;
    LineJob job;
    job.linesOnly = true;
    if (lcfg) {
        if (lcfg->reserved != 0) return XPE_ERR_INVALID_INPUT;
        if (lcfg->target_false_alarms != 0.0f) job.fa = lcfg->target_false_alarms;
        if (lcfg->min_segment != 0) job.minSeg = static_cast<int>(lcfg->min_segment);
        if (lcfg->noise_tolerance_db != 0.0f) job.tol = std::pow(10.0, static_cast<double>(lcfg->noise_tolerance_db) / 10.0);
        if (!(lcfg->noise_tolerance_db >= 0.0f && lcfg->noise_tolerance_db <= 20.0f)) return XPE_ERR_INVALID_INPUT;
    }
    if (!(job.fa > 0.0 && job.fa <= 1000.0) || job.minSeg < 16 || job.minSeg > 1024) return XPE_ERR_INVALID_INPUT;
    const uint32_t width = dark_frames[0].width, height = dark_frames[0].height;
    const size_t   n = static_cast<size_t>(width) * height;
    if (line_mask_out) {
        if (!line_mask_out->data) return XPE_ERR_INVALID_INPUT;
        if (line_mask_out->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
        if (line_mask_out->width != width || line_mask_out->height != height || line_mask_out->dataSize < n) return XPE_ERR_BUFFER_TOO_SMALL;
    }
    std::vector<uint8_t> scratchMap(n, 0), area(n, 0);
    XpeImageBuffer       mb{}, ab{};
    mb.data = scratchMap.data();
    mb.width = width;
    mb.height = height;
    mb.bitsAllocated = 8;
    mb.bitsStored = 8;
    mb.format = XPE_PIXEL_UINT8;
    mb.dataSize = n;
    ab = mb;
    ab.data = area.data();
    const XpeErrorCode rc = runWithMargins(dark_frames, num_dark, bright_frames, num_bright, cfg, &mb, nullptr, &ab, nullptr, &job);
    if (rc != XPE_OK) return rc;
    const uint32_t ml = cfg ? cfg->margin_left : 0u, mt = cfg ? cfg->margin_top : 0u, mr = cfg ? cfg->margin_right : 0u, mbm = cfg ? cfg->margin_bottom : 0u;
    const int      vw = static_cast<int>(width - ml - mr), vh = static_cast<int>(height - mt - mbm);
    // confirmed stretches become groups; the candidates whose persistence could not be judged are kept apart (never painted)
    std::vector<Seg> confirmedSegs, pendingSegs;
    for (const Seg& sg : job.segs) (sg.unconfirmed ? pendingSegs : confirmedSegs).push_back(sg);
    const std::vector<Group> groups = groupSegments(confirmedSegs, vw, vh);
    std::sort(pendingSegs.begin(), pendingSegs.end(), [](const Seg& p, const Seg& q) { return p.orient != q.orient ? p.orient < q.orient : (p.idx != q.idx ? p.idx < q.idx : p.a < q.a); });
    std::vector<Seg> pending;
    for (const Seg& sg : pendingSegs) {
        if (!pending.empty() && pending.back().orient == sg.orient && pending.back().idx == sg.idx && sg.a <= pending.back().b) {
            pending.back().b = std::max(pending.back().b, sg.b);
            pending.back().z = std::max(pending.back().z, sg.z);
        } else {
            pending.push_back(sg);
        }
    }
    if (line_mask_out) std::memset(line_mask_out->data, 0, n);
    XpeBpmLineReport rep{};
    rep.pixels = static_cast<uint32_t>(static_cast<size_t>(vw) * static_cast<size_t>(vh));
    for (int k = 0; k < 6; ++k) rep.null_width[k] = job.nullWidth[k];
    rep.lines = static_cast<uint32_t>(groups.size());
    rep.weak_response_stretches = job.weak;
    rep.unconfirmed = static_cast<uint32_t>(pending.size());
    rep.dark_frames_raw = job.darkRaw;
    rep.dark_frames_valid = job.darkValid;
    rep.bright_frames_raw = job.brightRaw;
    rep.bright_frames_valid = job.brightValid;
    if (unconfirmed_out)
        for (size_t k = 0; k < pending.size() && k < max_unconfirmed; ++k) {
            XpeBpmLine& o = unconfirmed_out[k];
            o = XpeBpmLine{};
            o.orientation = static_cast<uint32_t>(pending[k].orient);
            o.index = static_cast<uint32_t>(pending[k].idx);
            o.width = 1;
            o.start = static_cast<uint32_t>(pending[k].a);
            o.end = static_cast<uint32_t>(pending[k].b);
            o.causes = pending[k].cause;
            o.strength = pending[k].z;
            o.reason = XPE_BPM_LINE_REASON_TOO_FEW_FRAMES;
            o.persistence = -1.0f;
        }
    uint32_t written = 0;
    for (const Group& g : groups) {
        if (g.orient == 0) ++rep.row_groups;
        else ++rep.column_groups;
        if (g.flags & XPE_BPM_LINE_FULL) ++rep.full_lines;
        else ++rep.partial_lines;
        if (g.width == 2) ++rep.double_lines;
        rep.max_adjacent = std::max(rep.max_adjacent, static_cast<uint32_t>(g.width));
        if (g.flags & XPE_BPM_LINE_REJECT) rep.panel_reject = 1;
        for (const Group::Member& m : g.members) rep.line_pixels += static_cast<uint32_t>(m.b - m.a);
        if (lines_out && written < max_lines) {
            XpeBpmLine& o = lines_out[written];
            o.orientation = static_cast<uint32_t>(g.orient);
            o.index = static_cast<uint32_t>(g.idx);
            o.width = static_cast<uint32_t>(g.width);
            o.start = static_cast<uint32_t>(g.a);
            o.end = static_cast<uint32_t>(g.b);
            o.causes = g.causes;
            o.flags = g.flags;
            o.strength = g.z;
            o.persistence = g.persist;
            ++written;
        } else if (lines_out) {
            rep.truncated = 1;
        }
        if (line_mask_out) {
            uint8_t* m = static_cast<uint8_t*>(line_mask_out->data);
            for (const Group::Member& mem : g.members)   // the union of the detected stretches, not the bounding box of the group
                for (int t = mem.a; t < mem.b; ++t) {
                    const size_t x = g.orient == 0 ? static_cast<size_t>(t) : static_cast<size_t>(mem.idx);
                    const size_t y = g.orient == 0 ? static_cast<size_t>(mem.idx) : static_cast<size_t>(t);
                    if (x < width && y < height) m[y * width + x] = 1;
                }
        }
    }
    // crossings are pairs of a row group and a column group whose stretches meet
    for (const Group& r : groups) {
        if (r.orient != 0) continue;
        for (const Group& c : groups)
            if (c.orient == 1) {
                bool hit = false;
                for (const Group::Member& rm : r.members)
                    for (const Group::Member& cm : c.members) hit = hit || (cm.idx >= rm.a && cm.idx < rm.b && rm.idx >= cm.a && rm.idx < cm.b);
                if (hit) ++rep.crossings;
            }
    }
    // a pixel counted twice by a crossing is counted once in the mask; the report keeps the sum of the stretches
    if (report) *report = rep;
    return XPE_OK;
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_INTERNAL;
}

extern "C" XPE_API XpeErrorCode xpe_bpm_blobs(const XpeImageBuffer* map, XpeBpmBlob* blobs_out, uint32_t max_blobs, uint32_t* blob_count) try {
    if (blob_count) *blob_count = 0;
    if (!map || !map->data) return XPE_ERR_INVALID_INPUT;
    if (map->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
    const int    w = static_cast<int>(map->width), h = static_cast<int>(map->height);
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (w < 1 || h < 1 || (map->dataSize != 0 && map->dataSize < n)) return XPE_ERR_INVALID_INPUT;
    const uint8_t*       m = static_cast<const uint8_t*>(map->data);
    std::vector<uint8_t> seen(n, 0);
    std::vector<XpeBpmBlob> blobs;
    std::vector<size_t>  stack;
    for (size_t start = 0; start < n; ++start) {
        if (m[start] == 0 || seen[start]) continue;
        XpeBpmBlob b{static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0, 0, 0};
        stack.assign(1, start);
        seen[start] = 1;
        while (!stack.empty()) {
            const size_t i = stack.back();
            stack.pop_back();
            const int x = static_cast<int>(i % static_cast<size_t>(w)), y = static_cast<int>(i / static_cast<size_t>(w));
            ++b.size;
            b.x0 = std::min(b.x0, static_cast<uint32_t>(x));
            b.x1 = std::max(b.x1, static_cast<uint32_t>(x));
            b.y0 = std::min(b.y0, static_cast<uint32_t>(y));
            b.y1 = std::max(b.y1, static_cast<uint32_t>(y));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx, yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                    const size_t j = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
                    if (m[j] != 0 && !seen[j]) {
                        seen[j] = 1;
                        stack.push_back(j);
                    }
                }
        }
        blobs.push_back(b);
    }
    std::stable_sort(blobs.begin(), blobs.end(), [](const XpeBpmBlob& p, const XpeBpmBlob& q) { return p.size > q.size; });
    if (blob_count) *blob_count = static_cast<uint32_t>(blobs.size());
    if (blobs_out)
        for (uint32_t k = 0; k < max_blobs && k < blobs.size(); ++k) blobs_out[k] = blobs[k];
    return XPE_OK;
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_INTERNAL;
}
