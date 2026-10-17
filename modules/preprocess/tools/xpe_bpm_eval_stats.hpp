/**
 * @file xpe_bpm_eval_stats.hpp
 * @brief The numeric parts of the defect-map evaluation tool xpe_bpm_eval, apart from the file and module handling so the tests can call them directly.
 *
 * Definitions: .moai/reports/lane-pre/QA-A-251/evidence/30_method_fixed_before_measurement.md (section 3) and, for the corrections of Codex #204,
 * .moai/reports/lane-pre/QA-A-251b/evidence/30_method_fixed_before_the_fix.md (section 1).
 *
 * Nothing in here turns a value that cannot be computed into a number: a sample whose denominator is zero or whose value is not finite is counted as
 * invalid with its reason, and a group or a line without a valid value is reported as uncomputable (never as 0).
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xpe_bpm_eval {

struct Rect {
    int x, y, w, h;
};

/** Median of the values: v[floor(m / 2)] of the sorted values. @p v must not be empty. */
inline double medianOf(std::vector<double> v) {
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
    return v[v.size() / 2];
}

/** The q percentile (0..1): v[min(m - 1, floor(q * (m - 1) + 0.5))] of the sorted values. @p v must not be empty. */
inline double percentileOf(std::vector<double> v, double q) {
    const size_t k = std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size() - 1) + 0.5));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

// ------------------------------------------------------------------------------------------------------------- correction error
enum class ZStatus { Valid, ZeroSigmaEqual, ZeroSigmaError, NonFinite };

/**
 * One correction-error sample: |measured - reference| / sigma. Invalid when a value is not finite (NonFinite), or sigma is zero (or negative): ZeroSigmaEqual
 * when the numerator is zero too (0 / 0), ZeroSigmaError when it is not (a real error that cannot be measured against the noise).
 */
inline ZStatus classifyZ(double measured, double reference, double sigma, double* z) {
    if (!std::isfinite(measured) || !std::isfinite(reference) || !std::isfinite(sigma)) return ZStatus::NonFinite;
    const double d = std::fabs(measured - reference);
    if (!(sigma > 0.0)) return d == 0.0 ? ZStatus::ZeroSigmaEqual : ZStatus::ZeroSigmaError;
    *z = d / sigma;
    return ZStatus::Valid;
}

/** The samples of one group on one side (corrected or uncorrected output). */
struct ZSide {
    std::vector<double> z;
    size_t              zeroSigmaEqual{0}, zeroSigmaError{0}, nonFinite{0};

    void add(ZStatus s, double value) {
        switch (s) {
            case ZStatus::Valid: z.push_back(value); break;
            case ZStatus::ZeroSigmaEqual: ++zeroSigmaEqual; break;
            case ZStatus::ZeroSigmaError: ++zeroSigmaError; break;
            case ZStatus::NonFinite: ++nonFinite; break;
        }
    }
    size_t valid() const { return z.size(); }
    size_t invalid() const { return zeroSigmaEqual + zeroSigmaError + nonFinite; }
    bool   computable() const { return !z.empty(); }
    double median() const { return medianOf(z); }          // only when computable()
    double p95() const { return percentileOf(z, 0.95); }   // only when computable()
};

// ------------------------------------------------------------------------------------------------------------- line index D_n
struct LineIndex {
    bool   skipped{false};        ///< the line is at the frame edge or too short: not evaluated, not counted
    bool   computable{false};     ///< ratio is a finite number (standard-deviation basis, the one that decides)
    bool   madComputable{false};  ///< ratioMad is a finite number
    double dn{0.0};               ///< D_n in pixel values
    double sigmaPix{0.0};         ///< standard deviation of the front neighbour's difference residuals / sqrt 1.5
    double sigmaPred{0.0};        ///< predicted standard deviation of D_n for uncorrelated noise
    double ratio{0.0};            ///< dn / sigmaPred
    double sigmaPixMad{0.0};      ///< 1.4826 * MAD / sqrt 1.5 (auxiliary, never used for a verdict)
    double ratioMad{0.0};         ///< dn / (sigmaPixMad-based prediction)
};

/**
 * The line index D_n (AAPM TG-150 4.3.9, method A) of a row-type (w > h) or column-type rectangle of the image @p s (w x h), with the prediction for
 * uncorrelated noise. P is the mean along the rectangle's extent; D = P_item - (P_before + P_after) / 2 with the lines just before and just after the
 * rectangle; the item profile averages its k lines. sigmaPix is the sample standard deviation of d_i = S_i - (S_{i-1} + S_{i+1}) / 2 along the line
 * before, divided by sqrt 1.5; the prediction is sigmaPix * sqrt(1 / (k L) + 1 / (2 L)) with L the extent (k = 1: sigmaPix * sqrt 1.5 / sqrt L).
 */
inline LineIndex lineIndex(const float* s, uint32_t w, uint32_t h, const Rect& q) {
    LineIndex r;
    const bool row = q.w > q.h;
    const int  k = row ? q.h : q.w, len = row ? q.w : q.h;
    const int  first = row ? q.y : q.x;
    const int  beforeIdx = first - 1, afterIdx = first + k;
    if (beforeIdx < 0 || afterIdx >= static_cast<int>(row ? h : w) || len < 8) {
        r.skipped = true;
        return r;
    }
    auto at = [&](int line, int pos) {
        return row ? static_cast<double>(s[static_cast<size_t>(line) * w + static_cast<size_t>(pos)]) : static_cast<double>(s[static_cast<size_t>(pos) * w + static_cast<size_t>(line)]);
    };
    const int p0 = row ? q.x : q.y;
    double pItem = 0, pBefore = 0, pAfter = 0;
    for (int p = p0; p < p0 + len; ++p) {
        for (int t = 0; t < k; ++t) pItem += at(first + t, p);
        pBefore += at(beforeIdx, p);
        pAfter += at(afterIdx, p);
    }
    pItem /= static_cast<double>(k) * len;
    pBefore /= len;
    pAfter /= len;
    r.dn = pItem - 0.5 * (pBefore + pAfter);

    std::vector<double> res;
    res.reserve(static_cast<size_t>(len));
    for (int p = p0 + 1; p < p0 + len - 1; ++p) res.push_back(at(beforeIdx, p) - 0.5 * (at(beforeIdx, p - 1) + at(beforeIdx, p + 1)));
    double mean = 0;
    for (double v : res) mean += v;
    mean /= static_cast<double>(res.size());
    double ss = 0;
    for (double v : res) ss += (v - mean) * (v - mean);
    r.sigmaPix = std::sqrt(ss / static_cast<double>(res.size() - 1)) / std::sqrt(1.5);
    const double factor = std::sqrt(1.0 / (static_cast<double>(k) * len) + 0.5 / len);
    r.sigmaPred = r.sigmaPix * factor;
    r.computable = std::isfinite(r.dn) && std::isfinite(r.sigmaPred) && r.sigmaPred > 0.0;
    if (r.computable) r.ratio = r.dn / r.sigmaPred;

    const double med = medianOf(res);
    std::vector<double> dev;
    dev.reserve(res.size());
    for (double v : res) dev.push_back(std::fabs(v - med));
    r.sigmaPixMad = 1.4826 * medianOf(dev) / std::sqrt(1.5);
    const double predMad = r.sigmaPixMad * factor;
    r.madComputable = std::isfinite(r.dn) && std::isfinite(predMad) && predMad > 0.0;
    if (r.madComputable) r.ratioMad = r.dn / predMad;
    return r;
}

// ------------------------------------------------------------------------------------------------------------- imaging-area contract of the line index (QA-A-265, Codex #234 R234-1)

/** The imaging area: columns [x0, x1), rows [y0, y1). A frame without margins has the whole frame as its area. */
struct Area {
    int x0, y0, x1, y1;
};

/** Why a line part could (not) be measured inside the imaging area. */
enum class AreaStatus {
    Full,              ///< the whole rectangle lies inside the area
    Clipped,           ///< the lines are inside, the reference lines are inside, the extent was cut to the area (still at least kMinAreaExtent long)
    ItemOutside,       ///< a line of the part itself lies outside the area (across the line)
    ReferenceOutside,  ///< the line before or the line after the part lies outside the area
    TooShort,          ///< less than kMinAreaExtent pixels of the extent are inside the area
    OutsideArea        ///< none of the extent is inside the area
};

constexpr int kMinAreaExtent = 8;   ///< the same minimum extent the plain line index asks for

/** The line index of a part under the area contract: where it was measured, from which rectangle, and why not when it was not. */
struct AreaLineIndex {
    AreaStatus status{AreaStatus::OutsideArea};
    Rect       used{0, 0, 0, 0};   ///< the rectangle the index was computed on (the part cut to the area)
    LineIndex  li;                 ///< valid only when measured()
    bool       measured() const { return status == AreaStatus::Full || status == AreaStatus::Clipped; }
};

/**
 * The line index D_n of a row- or column-type rectangle, measured only where everything it reads lies inside the imaging area: the lines of the part, the line before and the line after
 * (the references of D_n), and the extent of the part cut to the area, which is also the support of the standard-deviation estimate (the residuals of the front reference line use the same
 * positions and their neighbours). A part whose extent leaves the area is measured on the part of its extent inside the area (status Clipped) when at least kMinAreaExtent pixels remain.
 * Anything else is not measured and says why. Nothing here changes lineIndex: a part wholly inside the area gives bit for bit the value lineIndex gives.
 */
inline AreaLineIndex lineIndexInArea(const float* s, uint32_t w, uint32_t h, const Rect& q, const Area& area) {
    AreaLineIndex r;
    const bool row = q.w > q.h;
    const int  k = row ? q.h : q.w;
    const int  first = row ? q.y : q.x;
    const int  acrossLo = row ? area.y0 : area.x0, acrossHi = row ? area.y1 : area.x1;       // across the lines
    const int  alongLo = row ? area.x0 : area.y0, alongHi = row ? area.x1 : area.y1;         // along the lines
    const int  p0 = row ? q.x : q.y, len = row ? q.w : q.h;
    if (first < acrossLo || first + k > acrossHi) {
        r.status = AreaStatus::ItemOutside;
        return r;
    }
    if (first - 1 < acrossLo || first + k >= acrossHi) {
        r.status = AreaStatus::ReferenceOutside;
        return r;
    }
    const int lo = std::max(p0, alongLo), hi = std::min(p0 + len, alongHi);
    if (hi <= lo) {
        r.status = AreaStatus::OutsideArea;
        return r;
    }
    if (hi - lo < kMinAreaExtent) {
        r.status = AreaStatus::TooShort;
        return r;
    }
    r.used = row ? Rect{lo, q.y, hi - lo, q.h} : Rect{q.x, lo, q.w, hi - lo};
    r.status = (lo == p0 && hi == p0 + len) ? AreaStatus::Full : AreaStatus::Clipped;
    r.li = lineIndex(s, w, h, r.used);
    return r;
}

// ------------------------------------------------------------------------------------------------------------- chi-square, quantum gain, noise cost (QA-A-252f)

/** Regularised lower incomplete gamma function P(a, x): the chi-square distribution function is P(nu / 2, x / 2). */
inline double gammaP(double a, double x) {
    if (!(x > 0.0)) return 0.0;
    const double gln = std::lgamma(a);
    if (x < a + 1.0) {   // series
        double ap = a, sum = 1.0 / a, del = sum;
        for (int n = 0; n < 5000; ++n) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (std::fabs(del) < std::fabs(sum) * 1e-15) break;
        }
        return sum * std::exp(-x + a * std::log(x) - gln);
    }
    double b = x + 1.0 - a, c = 1e300, d = 1.0 / b, h = d;   // continued fraction for the upper function
    for (int i = 1; i < 5000; ++i) {
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
    return 1.0 - std::exp(-x + a * std::log(x) - gln) * h;
}

/** The value x with P(chi-square_nu <= x) = p, by bisection. */
inline double chiSquareQuantile(double nu, double p) {
    double lo = 0.0, hi = std::max(10.0, nu * 4.0 + 40.0);
    while (gammaP(0.5 * nu, 0.5 * hi) < p) hi *= 2.0;
    for (int it = 0; it < 300; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (gammaP(0.5 * nu, 0.5 * mid) < p) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

/**
 * The median of chi-square_nu / nu: the factor by which the MEDIAN of unbiased sample variances (nu degrees of freedom) is too small (nu = 1: 0.455, nu = 2: 0.693,
 * nu = 5: 0.870, the same bias as in the sample-variance median of #208). Dividing a median of sample variances by it makes it the variance.
 */
inline double chiSquareMedianOverNu(double nu) { return chiSquareQuantile(nu, 0.5) / nu; }

/** One dose group of the quantum gain fit: its net signal, its variance (already divided by chiSquareMedianOverNu), its degrees of freedom and the number of pixels. */
struct QuantumGainGroup {
    double signal{0}, variance{0}, dof{0}, pixels{0};
};

/** The fit variance = v0 + kappa * signal over the dose groups, weighted by the inverse variance of each group variance (2 variance^2 / (dof * pixels)). */
struct QuantumGainFit {
    bool   ok{false};
    double kappa{0}, v0{0}, kappaSe{0}, v0Se{0}, chiSquare{0};
    int    groups{0};
};

inline QuantumGainFit fitQuantumGain(const std::vector<QuantumGainGroup>& g) {
    QuantumGainFit r;
    double S = 0, Sx = 0, Sy = 0, Sxx = 0, Sxy = 0;
    int    k = 0;
    for (const QuantumGainGroup& q : g) {
        if (!(q.variance > 0.0) || !(q.dof > 0.0) || !(q.pixels > 0.0) || !(q.signal > 0.0)) continue;
        const double w = q.dof * q.pixels / (2.0 * q.variance * q.variance);
        S += w;
        Sx += w * q.signal;
        Sy += w * q.variance;
        Sxx += w * q.signal * q.signal;
        Sxy += w * q.signal * q.variance;
        ++k;
    }
    r.groups = k;
    const double delta = S * Sxx - Sx * Sx;
    if (k < 2 || !(delta > 0.0)) return r;
    r.kappa = (S * Sxy - Sx * Sy) / delta;
    r.v0 = (Sxx * Sy - Sx * Sxy) / delta;
    for (const QuantumGainGroup& q : g) {
        if (!(q.variance > 0.0) || !(q.dof > 0.0) || !(q.pixels > 0.0) || !(q.signal > 0.0)) continue;
        const double w = q.dof * q.pixels / (2.0 * q.variance * q.variance), e = q.variance - r.v0 - r.kappa * q.signal;
        r.chiSquare += w * e * e;
    }
    const double scale = k > 2 ? std::max(1.0, r.chiSquare / static_cast<double>(k - 2)) : 1.0;   // the scatter about the line widens the errors, never narrows them
    r.kappaSe = std::sqrt(S / delta * scale);
    r.v0Se = std::sqrt(Sxx / delta * scale);
    r.ok = std::isfinite(r.kappa) && r.kappa > 0.0;
    return r;
}

/**
 * Relative standard error of the median of @p n samples of chi-square_nu / nu (the asymptotic variance of a sample median, 1 / (4 n f(m)^2), with f the density of
 * chi-square_nu / nu at its median m): 2.33 / sqrt(n) for nu = 1, 1.2 / sqrt(n) for nu = 5.
 */
inline double medianRelativeSe(double nu, double n) {
    const double m = chiSquareMedianOverNu(nu), x = nu * m;
    const double pdf = std::exp((0.5 * nu - 1.0) * std::log(x) - 0.5 * x - 0.5 * nu * std::log(2.0) - std::lgamma(0.5 * nu));
    const double f = nu * pdf;
    return 1.0 / (2.0 * std::sqrt(n) * f * m);
}

/** The mean of the per-pixel variance ratios of an item, the lag-1 autocorrelation along its pixels and the effective degrees of freedom of the mean from blocks of consecutive pixels (0 = not determinable). */
struct BlockDof {
    double mean{0}, lag1{0}, dofEff{0};
    size_t blocks{0};
};

inline BlockDof blockEffectiveDof(const std::vector<double>& c, size_t block) {
    BlockDof r;
    const size_t n = c.size();
    if (n == 0 || block == 0) return r;
    double s = 0.0;
    for (double v : c) s += v;
    r.mean = s / static_cast<double>(n);
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < n; ++i) den += (c[i] - r.mean) * (c[i] - r.mean);
    for (size_t i = 0; i + 1 < n; ++i) num += (c[i] - r.mean) * (c[i + 1] - r.mean);
    r.lag1 = den > 0.0 ? num / den : 0.0;
    const size_t nb = n / block;
    r.blocks = nb;
    if (nb < 4) return r;
    std::vector<double> bm(nb, 0.0);
    for (size_t j = 0; j < nb; ++j) {
        for (size_t i = j * block; i < (j + 1) * block; ++i) bm[j] += c[i];
        bm[j] /= static_cast<double>(block);
    }
    double mb = 0.0;
    for (double v : bm) mb += v;
    mb /= static_cast<double>(nb);
    double vb = 0.0;
    for (double v : bm) vb += (v - mb) * (v - mb);
    vb /= static_cast<double>(nb - 1);
    const double se2 = vb / static_cast<double>(nb);
    r.dofEff = se2 > 0.0 ? 2.0 * mb * mb / se2 : 0.0;
    return r;
}

/** The verdict on the noise cost of an item (the variance after gain correction against the local expectation; 2 is the SRS limit). */
enum class NoiseLabel { Defect, Normal, Undetermined };

struct NoiseVerdict {
    double     cost{0}, lo{0}, hi{0};
    NoiseLabel label{NoiseLabel::Undetermined};
};

/**
 * The cost @p cost of an item measured with @p dof independent degrees of freedom of its variance estimate (pixels x (frames - 1)); @p extraLogSe is the standard error
 * (natural log) of the local expectation it is compared with. The interval is the chi-square interval at 95 % widened by the error of the expectation. Defect: the
 * interval lies at or above @p limit. Normal: it lies below. Undetermined: it contains @p limit (the item is counted neither as a hit nor as a false alarm).
 */
inline NoiseVerdict classifyNoiseCost(double cost, double dof, double extraLogSe, double limit = 2.0) {
    NoiseVerdict v;
    v.cost = cost;
    const double widen = std::exp(1.96 * extraLogSe);
    v.lo = dof * cost / chiSquareQuantile(dof, 0.975) / widen;
    v.hi = dof * cost / chiSquareQuantile(dof, 0.025) * widen;
    v.label = v.lo >= limit ? NoiseLabel::Defect : (v.hi < limit ? NoiseLabel::Normal : NoiseLabel::Undetermined);
    return v;
}

}  // namespace xpe_bpm_eval
