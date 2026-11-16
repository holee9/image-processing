/**
 * @file xpe_dark_selection.hpp
 * @brief QA-A-337 (#278): which leading dark frames of a calibration list are left out.
 *
 * Header-only so the rule is unit-tested on doubles (layer U) without the tool, and the tool
 * (xpe_calib_fixture_gen) calls the same function on the per-frame means of its region.
 *
 * Rule (design 3 as amended, .moai/reports/lane-pre/QA-A-337/report.md):
 *   - frame 0 is always excluded;
 *   - R = median of the means of the last ceil(N/2) frames (N = frames given); an even count
 *     takes the mean of the middle two values;
 *   - from frame 1 on, the contiguous run with |m_k - R| > T is excluded as well (T = 20);
 *   - verdict unavailable when (c) a frame of the later half deviates, (a) a frame after the
 *     excluded run deviates, (b) fewer than max(5, ceil(N/2)) frames remain. Every true
 *     condition is reported, in the order (c), (a), (b).
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace xpe_dark_sel {

constexpr double kDeviationLimit = 20.0;   // T, pixel value: a candidate policy value fixed before measurement (QA-A-337)
constexpr size_t kMinUsedDarks = 5;         // the tool's minimum number of dark frames (kMinDarkFrames)

enum class Condition { LaterHalfDeviates /*(c)*/, DeviatesAfterRun /*(a)*/, TooFewLeft /*(b)*/ };

struct Selection {
    size_t                 excluded{0};   // the first `excluded` frames are left out (always >= 1 for N >= 1)
    double                 reference{0.0};  // R
    std::vector<Condition> conditions;      // empty = the selection is usable; otherwise in the order (c), (a), (b)
};

inline const char* conditionLabel(Condition c) {
    switch (c) {
    case Condition::LaterHalfDeviates: return "(c)";
    case Condition::DeviatesAfterRun: return "(a)";
    case Condition::TooFewLeft: return "(b)";
    }
    return "?";
}

/** Median with the even-count rule of this design (mean of the two middle values). Not the upper median used elsewhere. */
inline double medianMeanOfMiddleTwo(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

inline Selection selectLeadingDarks(const std::vector<double>& m, double limit = kDeviationLimit) {
    Selection    s;
    const size_t n = m.size();
    if (n == 0) return s;
    const size_t half = (n + 1) / 2;   // ceil(N/2)
    s.reference = medianMeanOfMiddleTwo(std::vector<double>(m.end() - static_cast<std::ptrdiff_t>(half), m.end()));
    auto deviates = [&](size_t k) { return std::fabs(m[k] - s.reference) > limit; };
    size_t k = 1;
    while (k < n && deviates(k)) ++k;
    s.excluded = k;
    bool laterHalf = false, afterRun = false;
    for (size_t j = n - half; j < n; ++j) laterHalf = laterHalf || deviates(j);
    for (size_t j = s.excluded; j < n; ++j) afterRun = afterRun || deviates(j);
    const bool tooFew = n - s.excluded < std::max(kMinUsedDarks, half);
    if (laterHalf) s.conditions.push_back(Condition::LaterHalfDeviates);
    if (afterRun) s.conditions.push_back(Condition::DeviatesAfterRun);
    if (tooFew) s.conditions.push_back(Condition::TooFewLeft);
    return s;
}

}  // namespace xpe_dark_sel
