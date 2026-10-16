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
double varianceThreshold(uint32_t nu, double p) {
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

/** Separable median filter: window x 1 along the rows, then 1 x window along the columns; reflected borders. */
void separableMedian(const float* in, float* tmp, float* out, int w, int h, int win) {
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

/** Median of @p win values along the columns (alongY) or along the rows, reflected borders. */
void slidingMedian(const float* in, float* out, int w, int h, int win, bool alongY) {
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
inline float scaleAt(const std::vector<float>& sig, int nbx, int nby, int block, int x, int y) {
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
float flagVarianceRatio(Work& wk, const Settings& st, uint32_t nu, double tau, uint8_t bit, std::vector<uint8_t>* flags, uint32_t* count) {
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
enum LineKind { kKindResponse = 0, kKindLinearity = 1, kKindNoise = 2 };

struct Seg {
    int      orient;   // 0 row, 1 column
    int      idx;      // the row or column
    int      a, b;     // [a, b) along the line
    uint32_t cause;
    float    z;
    float    persist{-1.0f};   // fraction of the dark frames in which the stretch is large; -1 = not tested
};

struct LineJob {
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
void rowStretches(const float* f, int w, int h, const Settings& st, LineJob* job, int kind, int orient, int nullSlot, std::vector<Seg>* out) {
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
        for (int x = 0; x < nw; ++x) zw[static_cast<size_t>(y) * static_cast<size_t>(nw) + static_cast<size_t>(x)] = static_cast<float>((cs[static_cast<size_t>(x + L)] - cs[static_cast<size_t>(x)]) / std::sqrt(static_cast<double>(L)));
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
    const double lo = 1.0 / std::sqrt(job->tol), hi = std::sqrt(job->tol);
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
            // trim the ends: a stretch ends where eight pixels in a row no longer reach half of its mean
            auto meanOf = [&](int p, int q) {
                double s = 0.0;
                for (int k = p; k < q; ++k) s += static_cast<double>(er[k]);
                return s / std::max(1, q - p);
            };
            const double m = meanOf(a, b);
            auto holds = [&](int p, int q) { return sign * meanOf(p, q) >= 0.5 * sign * m && sign * m > 0.0; };
            while (b - a > L && !holds(a, std::min(b, a + 8))) ++a;
            while (b - a > L && !holds(std::max(a, b - 8), b)) --b;
            x = x2 + 1;
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
                if (g < lo || g > hi) cause = XPE_BPM_LINE_CAUSE_RESPONSE;
            } else if (kind == kKindNoise) {
                const double g = std::fabs(sr) > 1e-30 ? sf / sr : 1.0;
                if (g >= job->tol) cause = XPE_BPM_LINE_CAUSE_NOISE;
            } else {
                cause = XPE_BPM_LINE_CAUSE_LINEARITY;
            }
            if (cause == 0) continue;
            float persist = -1.0f;
            if (cause == XPE_BPM_LINE_CAUSE_NOISE && job->persistence) {
                persist = job->persistence(orient, y, a, b);
                if (!(persist > 0.5f)) continue;   // large in half of the frames or fewer: noise that moves, not a defect of a place
            }
            out->push_back(Seg{orient, y, a, b, cause, static_cast<float>(zmax), persist});
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
    int      orient, idx, width, a, b;
    uint32_t causes, flags;
    float    z;
    float    persist;
};

/** Merges the stretches of one line, then groups adjacent lines whose stretches overlap by half. */
std::vector<Group> groupSegments(std::vector<Seg> segs, int w, int h) {
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
            Group g{orient, lines[i].idx, 1, lines[i].a, lines[i].b, lines[i].cause, 0, lines[i].z, lines[i].persist};
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
                ++g.width;
                last = lines[j].idx;
                used[j] = true;
            }
            const int length = orient == 0 ? w : h;
            if ((g.b - g.a) * 100 >= length * 95) g.flags |= XPE_BPM_LINE_FULL;
            if (g.width >= 3) g.flags |= XPE_BPM_LINE_REJECT;
            groups.push_back(g);
        }
    }
    // crossings
    for (Group& r : groups) {
        if (r.orient != 0) continue;
        for (Group& c : groups) {
            if (c.orient != 1) continue;
            const bool hit = c.idx + c.width > r.a && c.idx < r.b && r.idx + r.width > c.a && r.idx < c.b;
            if (hit) {
                r.flags |= XPE_BPM_LINE_CROSSES;
                c.flags |= XPE_BPM_LINE_CROSSES;
            }
        }
    }
    return groups;
}

}  // namespace

/** The detection on frames that hold the imaging area only (no margins). */
static XpeErrorCode generateV2Core(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                   const XpeBpmConfigV2* cfg, XpeImageBuffer* bpm_out, XpeImageBuffer* feature_out, XpeBpmReportV2* report, LineJob* job) try {
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

    // ---- per-pixel least-squares sums over the groups: n_g = a * S_g + b
    std::vector<double> Sn(n, 0.0), Sxn(n, 0.0), Snn(n, 0.0);
    double Sx = 0.0, Sxx = 0.0;
    std::vector<double> groupSignal(G, 0.0);
    // bright-frame temporal noise inside the dose groups (QA-A-252b, F2b): per group the variance of the frames about the group mean (the row and column means of every frame
    // taken out, as for the dark frames), relative to its own local median; the groups are pooled as a sum of chi-square variables
    std::vector<float> noiseSum, refSum;
    uint32_t           nuBright = 0, noiseGroups = 0;
    {
        std::vector<float> acc(n);
        std::vector<float> vg, vgTmp, vgBg;
        for (uint32_t g = 0; g < G; ++g) {
            std::fill(acc.begin(), acc.end(), 0.0f);
            double sg = 0.0;
            for (uint32_t k : groups[g]) {
                const uint16_t* p = px(bright_frames[k]);
                for (size_t i = 0; i < n; ++i) acc[i] += static_cast<float>(p[i]);
                sg += signal[k];
            }
            sg /= static_cast<double>(groups[g].size());
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
                    vg.resize(n);
                    vgTmp.resize(n);
                    vgBg.resize(n);
                }
                std::fill(vg.begin(), vg.end(), 0.0f);
                std::vector<double> rm(static_cast<size_t>(height)), cm(static_cast<size_t>(width));
                for (uint32_t k : groups[g]) {
                    const uint16_t* p = px(bright_frames[k]);
                    for (uint32_t y = 0; y < height; ++y) {
                        double a = 0.0;
                        for (uint32_t x = 0; x < width; ++x) {
                            const size_t i = static_cast<size_t>(y) * width + x;
                            a += static_cast<double>(p[i]) - static_cast<double>(acc[i] * inv);
                        }
                        rm[y] = a / width;
                    }
                    std::fill(cm.begin(), cm.end(), 0.0);
                    for (uint32_t y = 0; y < height; ++y)
                        for (uint32_t x = 0; x < width; ++x) {
                            const size_t i = static_cast<size_t>(y) * width + x;
                            cm[x] += static_cast<double>(p[i]) - static_cast<double>(acc[i] * inv) - rm[y];
                        }
                    for (uint32_t x = 0; x < width; ++x) cm[x] /= height;
                    for (uint32_t y = 0; y < height; ++y)
                        for (uint32_t x = 0; x < width; ++x) {
                            const size_t i = static_cast<size_t>(y) * width + x;
                            const double e = static_cast<double>(p[i]) - static_cast<double>(acc[i] * inv) - rm[y] - cm[x];
                            vg[i] += static_cast<float>(e * e);
                        }
                }
                for (size_t i = 0; i < n; ++i) vg[i] /= static_cast<float>(mg - 1);
                separableMedian(vg.data(), vgTmp.data(), vgBg.data(), w, h, static_cast<int>(st.window));
                const double mnu = varianceMedian(mg - 1);
                for (size_t i = 0; i < n; ++i) {
                    const double ref = std::max(static_cast<double>(vgBg[i]) / mnu, kReferenceFloor);
                    noiseSum[i] += static_cast<float>((mg - 1) * (static_cast<double>(vg[i]) / ref));
                    refSum[i] += static_cast<float>(ref);
                }
                nuBright += mg - 1;
                ++noiseGroups;
            }
        }
    }

    if (report) std::memset(report, 0, sizeof(*report));
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
        job->persistence = [&](int orient, int idx, int a, int b) -> float {
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
                        const double e = static_cast<double>(p[i]) - static_cast<double>(D[i]) - rowMeanK[k][static_cast<size_t>(y)] - colMeanK[k][static_cast<size_t>(x)];
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
        active[2] = 1;
        const double dgx = static_cast<double>(G) * Sxx - Sx * Sx;
        for (size_t i = 0; i < n; ++i) wk.feat[i] = static_cast<float>((static_cast<double>(G) * Sxn[i] - Sx * Sn[i]) / dgx);
        threshold[2] = lambdaTwoSided;
        if (job) analyzeFeature(wk.feat.data(), w, h, st, job, kKindResponse);
        if (!(job && job->linesOnly)) scaleMed[2] = flagRobustZ(wk, st, lambdaTwoSided, false, true, XPE_BPM_V2_FEATURE_DOSE_SLOPE, &flags, &counts[2]);
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
            if (job) analyzeFeature(wk.feat.data(), w, h, st, job, kKindLinearity);
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
            if (static_cast<double>(noiseSum[i]) / static_cast<double>(nuBright) > threshold[5]) {
                flags[i] = static_cast<uint8_t>(flags[i] | XPE_BPM_V2_FEATURE_BRIGHT_NOISE);
                ++counts[5];
            }
            if (i % 16 == 0) sample.push_back(refSum[i] / static_cast<float>(noiseGroups));
        }
        scaleMed[5] = medianOf(sample.data(), sample.size());
        if (job) {   // lines: the pooled variance ratio of the bright frames
            std::vector<float> t(n);
            for (size_t i = 0; i < n; ++i) t[i] = noiseSum[i] / static_cast<float>(nuBright);
            analyzeFeature(t.data(), w, h, st, job, kKindNoise);
        }
    }

    // F4 relative flat-field response
    for (size_t i = 0; i < n; ++i) wk.feat[i] = static_cast<float>(Sn[i] / Sx);
    threshold[4] = lambdaTwoSided;
    if (job) analyzeFeature(wk.feat.data(), w, h, st, job, kKindResponse);
    if (!(job && job->linesOnly)) scaleMed[4] = flagRobustZ(wk, st, lambdaTwoSided, false, true, XPE_BPM_V2_FEATURE_FLAT, &flags, &counts[4]);

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
                                   LineJob* job) try {
    const uint32_t ml = cfg ? cfg->margin_left : 0u, mt = cfg ? cfg->margin_top : 0u, mr = cfg ? cfg->margin_right : 0u, mb = cfg ? cfg->margin_bottom : 0u;
    if (ml == 0 && mt == 0 && mr == 0 && mb == 0) {
        if (area_out && bpm_out) {
            if (!area_out->data) return XPE_ERR_INVALID_INPUT;
            if (area_out->format != XPE_PIXEL_UINT8) return XPE_ERR_UNSUPPORTED_FORMAT;
            if (area_out->width != bpm_out->width || area_out->height != bpm_out->height || area_out->dataSize < static_cast<size_t>(bpm_out->width) * bpm_out->height) return XPE_ERR_BUFFER_TOO_SMALL;
        }
        const XpeErrorCode rc = generateV2Core(dark_frames, num_dark, bright_frames, num_bright, cfg, bpm_out, feature_out, report, job);
        if (rc == XPE_OK) {
            if (report) report->out_of_area_pixels = 0;
            if (area_out) std::memset(area_out->data, 1, static_cast<size_t>(bpm_out->width) * bpm_out->height);
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
    const XpeErrorCode rc = generateV2Core(cdb.data(), num_dark, cbb.data(), num_bright, cfg, &outMap, feature_out ? &outFeat : nullptr, report, job);
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
    return runWithMargins(dark_frames, num_dark, bright_frames, num_bright, cfg, bpm_out, feature_out, area_out, report, nullptr);
} catch (const std::bad_alloc&) {
    return XPE_ERR_OUT_OF_MEMORY;
} catch (...) {
    return XPE_ERR_INTERNAL;
}

extern "C" XPE_API XpeErrorCode xpe_bpm_find_lines(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                                   const XpeBpmConfigV2* cfg, const XpeBpmLineConfig* lcfg, XpeImageBuffer* line_mask_out, XpeBpmLine* lines_out,
                                                   uint32_t max_lines, XpeBpmLineReport* report) try {
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
    const std::vector<Group> groups = groupSegments(job.segs, vw, vh);
    if (line_mask_out) std::memset(line_mask_out->data, 0, n);
    XpeBpmLineReport rep{};
    rep.pixels = static_cast<uint32_t>(static_cast<size_t>(vw) * static_cast<size_t>(vh));
    for (int k = 0; k < 6; ++k) rep.null_width[k] = job.nullWidth[k];
    rep.lines = static_cast<uint32_t>(groups.size());
    uint32_t written = 0;
    for (const Group& g : groups) {
        if (g.orient == 0) ++rep.row_groups;
        else ++rep.column_groups;
        if (g.flags & XPE_BPM_LINE_FULL) ++rep.full_lines;
        else ++rep.partial_lines;
        if (g.width == 2) ++rep.double_lines;
        rep.max_adjacent = std::max(rep.max_adjacent, static_cast<uint32_t>(g.width));
        if (g.flags & XPE_BPM_LINE_REJECT) rep.panel_reject = 1;
        rep.line_pixels += static_cast<uint32_t>((g.b - g.a) * g.width);
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
            for (int k = 0; k < g.width; ++k)
                for (int t = g.a; t < g.b; ++t) {
                    const size_t x = g.orient == 0 ? static_cast<size_t>(t) : static_cast<size_t>(g.idx + k);
                    const size_t y = g.orient == 0 ? static_cast<size_t>(g.idx + k) : static_cast<size_t>(t);
                    if (x < width && y < height) m[y * width + x] = 1;
                }
        }
    }
    // crossings are pairs of a row group and a column group whose stretches meet
    for (const Group& r : groups) {
        if (r.orient != 0) continue;
        for (const Group& c : groups)
            if (c.orient == 1 && c.idx + c.width > r.a && c.idx < r.b && r.idx + r.width > c.a && r.idx < c.b) ++rep.crossings;
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
