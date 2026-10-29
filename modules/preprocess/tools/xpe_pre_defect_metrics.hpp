#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace xpe::pre::e2e {
struct DefectOracleMetrics {
    bool valid = false, predictionMeasured = false;
    uint64_t tp = 0, fp = 0, fn = 0, tn = 0, truthBad = 0, truthGood = 0;
    uint64_t residualPixels = 0, nonfinitePixels = 0;
    std::optional<double> recall, fpr, residual, p99Linear, p99Nearest;
};
// FUNC-019 / protocol 5.5. Prediction is mask selection, not proof of successful repair.
// An empty prediction supports stage statistics only (e.g. unsupported polynomial selection).
inline DefectOracleMetrics measureDefectOracle(const std::vector<float>& before,
    const std::vector<float>& after, const std::vector<uint8_t>& truth,
    const std::vector<uint8_t>& prediction, uint32_t width, uint32_t height) {
    DefectOracleMetrics m;
    const uint64_t n = static_cast<uint64_t>(width) * height;
    if (!n || before.size() != n || after.size() != n || truth.size() != n ||
        (!prediction.empty() && prediction.size() != n)) return m;
    m.valid = true;
    m.predictionMeasured = !prediction.empty();
    for (size_t i = 0; i < truth.size(); ++i) {
        const bool bad = truth[i] != 0;
        if (bad) ++m.truthBad; else ++m.truthGood;
        if (m.predictionMeasured) {
            const bool selected = prediction[i] != 0;
            if (bad && selected) ++m.tp;
            else if (bad) ++m.fn;
            else if (selected) ++m.fp;
            else ++m.tn;
        }
        if (!std::isfinite(before[i]) || !std::isfinite(after[i])) ++m.nonfinitePixels;
    }
    if (m.predictionMeasured) {
        if (m.truthBad) m.recall = static_cast<double>(m.tp) / static_cast<double>(m.truthBad);
        if (m.truthGood) m.fpr = static_cast<double>(m.fp) / static_cast<double>(m.truthGood);
    }
    // Do not omit nonfinite stage pixels and manufacture a passing subset.
    if (m.nonfinitePixels) return m;
    std::vector<double> deltas;
    deltas.reserve(static_cast<size_t>(m.truthGood));
    double residualSum = 0.0;
    for (size_t i = 0; i < truth.size(); ++i) {
        if (!truth[i]) {
            deltas.push_back(std::abs(static_cast<double>(after[i]) - before[i]));
            continue;
        }
        const size_t x = i % width, y = i / width;
        double neighbours = 0.0;
        size_t count = 0;
        const size_t y0 = y ? y - 1 : 0, x0 = x ? x - 1 : 0;
        for (size_t yy = y0; yy <= std::min(y + 1, static_cast<size_t>(height - 1)); ++yy)
            for (size_t xx = x0; xx <= std::min(x + 1, static_cast<size_t>(width - 1)); ++xx) {
                const size_t j = yy * width + xx;
                if (j != i && !truth[j]) { neighbours += after[j]; ++count; }
            }
        if (count) {
            residualSum += std::abs(static_cast<double>(after[i]) - neighbours / static_cast<double>(count));
            ++m.residualPixels;
        }
    }
    if (m.residualPixels) m.residual = residualSum / static_cast<double>(m.residualPixels);
    if (!deltas.empty()) {
        std::sort(deltas.begin(), deltas.end());
        const double rank = 0.99 * static_cast<double>(deltas.size() - 1);
        const size_t lo = static_cast<size_t>(rank), hi = std::min(lo + 1, deltas.size() - 1);
        m.p99Linear = deltas[lo] + (rank - static_cast<double>(lo)) * (deltas[hi] - deltas[lo]);
        const size_t nearest = static_cast<size_t>(std::ceil(0.99 * static_cast<double>(deltas.size()))) - 1;
        m.p99Nearest = deltas[nearest];
    }
    return m;
}
inline const char* defectSelectionVerdict(const DefectOracleMetrics& m, bool synthetic) {
    if (!m.recall || !m.fpr) return "unmeasured";
    if (!synthetic) return "reported"; // a caller-selected reference is not synthetic acceptance evidence
    return *m.recall == 1.0 && *m.fpr < 0.00001 ? "pass" : "fail";
}
inline const char* goodPixelVerdict(const DefectOracleMetrics& m) {
    if (!m.p99Linear || !m.p99Nearest) return "unmeasured";
    const bool linear = *m.p99Linear <= 1.0, nearest = *m.p99Nearest <= 1.0;
    if (linear != nearest) return "unmeasured"; // preserve the existing convention conflict
    return linear ? "pass" : "fail";
}
} // namespace xpe::pre::e2e
