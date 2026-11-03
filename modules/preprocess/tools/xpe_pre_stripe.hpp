#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace xpe::pre::e2e {
// Protocol 5.3: final output, original finite-positive gain ROI, population statistics.
struct StripeMeasurement {
    std::optional<double> ratio, rowRatio, columnRatio;
    double rowStd = 0.0, columnStd = 0.0, rowExpected = 0.0, columnExpected = 0.0;
    double sigmaHorizontal = 0.0, sigmaVertical = 0.0;
    size_t roiPixels = 0, rows = 0, columns = 0, horizontalPairs = 0, verticalPairs = 0;
};

inline StripeMeasurement measureStripe(const std::vector<float>& image, const std::vector<float>& gain,
                                       uint32_t width, uint32_t height) {
    StripeMeasurement result;
    const auto count = static_cast<uint64_t>(width) * height;
    if (!width || !height || count != image.size() || image.size() != gain.size()) return result;
    struct Population {
        size_t n = 0;
        double mean = 0.0, m2 = 0.0;
        void add(double value) {
            ++n;
            const double delta = value - mean;
            mean += delta / static_cast<double>(n);
            m2 += delta * (value - mean);
        }
        double stddev() const { return n ? std::sqrt(std::max(0.0, m2 / static_cast<double>(n))) : 0.0; }
    };
    const auto valid = [&](size_t i) { return std::isfinite(gain[i]) && gain[i] > 0.0f; };
    std::vector<double> rowSums(height), columnSums(width);
    std::vector<size_t> rowCounts(height), columnCounts(width);
    Population horizontal, vertical;
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            const size_t i = y * width + x;
            if (!valid(i)) continue;
            if (!std::isfinite(image[i])) return result;
            ++result.roiPixels;
            rowSums[y] += image[i]; ++rowCounts[y];
            columnSums[x] += image[i]; ++columnCounts[x];
            if (x + 1 < width && valid(i + 1)) {
                if (!std::isfinite(image[i + 1])) return result;
                horizontal.add(static_cast<double>(image[i]) - image[i + 1]);
            }
            if (y + 1 < height && valid(i + width)) {
                if (!std::isfinite(image[i + width])) return result;
                vertical.add(static_cast<double>(image[i]) - image[i + width]);
            }
        }
    }
    std::vector<double> rows, columns;
    for (size_t i = 0; i < height; ++i)
        if (rowCounts[i]) rows.push_back(rowSums[i] / static_cast<double>(rowCounts[i]));
    for (size_t i = 0; i < width; ++i)
        if (columnCounts[i]) columns.push_back(columnSums[i] / static_cast<double>(columnCounts[i]));
    result.rows = rows.size(); result.columns = columns.size();
    result.horizontalPairs = horizontal.n; result.verticalPairs = vertical.n;
    result.sigmaHorizontal = horizontal.stddev() / std::sqrt(2.0);
    result.sigmaVertical = vertical.stddev() / std::sqrt(2.0);
    const auto profile = [&](const std::vector<double>& values, double sigma, double& stddev, double& expected) {
        if (values.size() < 2 || !result.roiPixels) return;
        std::vector<double> prefix(values.size() + 1);
        for (size_t i = 0; i < values.size(); ++i) prefix[i + 1] = prefix[i] + values[i];
        Population highPass;
        double g = 0.0;
        for (size_t k = 0; k < values.size(); ++k) {
            const size_t a = k > 16 ? k - 16 : 0;
            const size_t b = k + std::min<size_t>(16, values.size() - 1 - k);
            const double m = static_cast<double>(b - a + 1);
            highPass.add(values[k] - (prefix[b + 1] - prefix[a]) / m);
            g += 1.0 - 1.0 / m;
        }
        stddev = highPass.stddev();
        g /= static_cast<double>(values.size());
        const double meanPixels = static_cast<double>(result.roiPixels) / static_cast<double>(values.size());
        expected = sigma * std::sqrt(g / meanPixels);
    };
    profile(rows, result.sigmaHorizontal, result.rowStd, result.rowExpected);
    profile(columns, result.sigmaVertical, result.columnStd, result.columnExpected);
    if (horizontal.n && result.rowExpected > 0.0 && std::isfinite(result.rowExpected))
        result.rowRatio = result.rowStd / result.rowExpected;
    if (vertical.n && result.columnExpected > 0.0 && std::isfinite(result.columnExpected))
        result.columnRatio = result.columnStd / result.columnExpected;
    if (result.rowRatio && result.columnRatio && std::isfinite(*result.rowRatio) && std::isfinite(*result.columnRatio))
        result.ratio = std::max(*result.rowRatio, *result.columnRatio);
    return result;
}
} // namespace xpe::pre::e2e
