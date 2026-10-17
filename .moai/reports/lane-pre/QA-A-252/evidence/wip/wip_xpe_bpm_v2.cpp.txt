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
    s->skip = 0;
    s->minDark = 5;
    s->minBright = 6;
    s->ratio = 1.25;
    if (!in) return XPE_OK;
    if (in->reserved != 0) return XPE_ERR_INVALID_INPUT;
    if (in->target_false_alarms != 0.0f) s->falseAlarms = in->target_false_alarms;
    if (in->window != 0) s->window = in->window;
    if (in->sigma_block != 0) s->block = in->sigma_block;
    s->skip = in->skip_first_dark != 0 ? 1u : 0u;
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

/** Subtracts the median of every row, then the median of every column of the result. */
void removeRowColumn(float* f, int w, int h) {
    std::vector<float> line(static_cast<size_t>(std::max(w, h)));
    for (int y = 0; y < h; ++y) {
        float* row = f + static_cast<size_t>(y) * static_cast<size_t>(w);
        std::copy(row, row + w, line.begin());
        const float m = medianOf(line.data(), static_cast<size_t>(w));
        for (int x = 0; x < w; ++x) row[x] -= m;
    }
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) line[static_cast<size_t>(y)] = f[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
        const float m = medianOf(line.data(), static_cast<size_t>(h));
        for (int y = 0; y < h; ++y) f[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] -= m;
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
float flagRobustZ(Work& wk, const Settings& st, double lambda, bool upperOnly, uint8_t bit, std::vector<uint8_t>* flags, uint32_t* count) {
    removeRowColumn(wk.feat.data(), wk.w, wk.h);
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

}  // namespace

extern "C" XPE_API XpeErrorCode xpe_bpm_generate_v2(const XpeImageBuffer* dark_frames, uint32_t num_dark, const XpeImageBuffer* bright_frames, uint32_t num_bright,
                                                    const XpeBpmConfigV2* cfg, XpeImageBuffer* bpm_out, XpeImageBuffer* feature_out, XpeBpmReportV2* report) try {
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
    {
        std::vector<uint32_t> sum(n, 0u);
        for (uint32_t k = first; k < num_dark; ++k) {
            const uint16_t* p = px(dark_frames[k]);
            for (size_t i = 0; i < n; ++i) sum[i] += p[i];
        }
        for (size_t i = 0; i < n; ++i) D[i] = static_cast<float>(sum[i]) / static_cast<float>(nd);
        std::vector<double> rowMean(static_cast<size_t>(h));
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
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                    const double e = static_cast<double>(p[i]) - static_cast<double>(D[i]) - rowMean[static_cast<size_t>(y)];
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
    {
        std::vector<float> acc(n);
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
        }
    }

    if (report) std::memset(report, 0, sizeof(*report));
    std::vector<uint8_t> flags(n, 0);
    Work wk{w, h, n, std::vector<float>(n), std::vector<float>(n), std::vector<float>(n)};
    uint32_t counts[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0};
    float    scaleMed[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0};
    double   threshold[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0};
    uint32_t nu[XPE_BPM_V2_FEATURE_COUNT] = {0, 0, 0, 0, 0};
    uint32_t active[XPE_BPM_V2_FEATURE_COUNT] = {1, 1, 0, 0, 1};
    const double p = st.falseAlarms / static_cast<double>(n);
    const double lambdaTwoSided = zThreshold(p);

    // F1 dark mean
    std::copy(D.begin(), D.end(), wk.feat.begin());
    threshold[0] = lambdaTwoSided;
    scaleMed[0] = flagRobustZ(wk, st, lambdaTwoSided, false, XPE_BPM_V2_FEATURE_DARK_MEAN, &flags, &counts[0]);

    // F2 dark temporal noise
    nu[1] = nd - 1;
    threshold[1] = varianceThreshold(nu[1], p);
    std::copy(s2.begin(), s2.end(), wk.feat.begin());
    scaleMed[1] = flagVarianceRatio(wk, st, nu[1], threshold[1], XPE_BPM_V2_FEATURE_DARK_NOISE, &flags, &counts[1]);

    // F3a slope and F3b linearity
    if (G >= 2) {
        active[2] = 1;
        const double dgx = static_cast<double>(G) * Sxx - Sx * Sx;
        for (size_t i = 0; i < n; ++i) wk.feat[i] = static_cast<float>((static_cast<double>(G) * Sxn[i] - Sx * Sn[i]) / dgx);
        threshold[2] = lambdaTwoSided;
        scaleMed[2] = flagRobustZ(wk, st, lambdaTwoSided, false, XPE_BPM_V2_FEATURE_DOSE_SLOPE, &flags, &counts[2]);
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
            scaleMed[3] = flagRobustZ(wk, st, threshold[3], true, XPE_BPM_V2_FEATURE_DOSE_LINEAR, &flags, &counts[3]);
        }
    }

    // F4 relative flat-field response
    for (size_t i = 0; i < n; ++i) wk.feat[i] = static_cast<float>(Sn[i] / Sx);
    threshold[4] = lambdaTwoSided;
    scaleMed[4] = flagRobustZ(wk, st, lambdaTwoSided, false, XPE_BPM_V2_FEATURE_FLAT, &flags, &counts[4]);

    // ---- output
    uint8_t* out = static_cast<uint8_t*>(bpm_out->data);
    uint32_t unionCount = 0;
    const uint8_t brightBits = XPE_BPM_V2_FEATURE_DOSE_SLOPE | XPE_BPM_V2_FEATURE_DOSE_LINEAR | XPE_BPM_V2_FEATURE_FLAT;
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
