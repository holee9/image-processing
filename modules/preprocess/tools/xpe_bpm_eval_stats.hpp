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

}  // namespace xpe_bpm_eval
