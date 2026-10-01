/**
 * @file test_zz_a186_probe.cpp
 * @brief QA-A-186 (#220) -- FlatResidualPct with the arithmetic mean (today) versus the median.
 *        Observation only; DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A186Probe.*
 *
 * For one `after` frame this prints four numbers, all percent:
 *   today   : prnu_after returned by xpe_verify_gain (what the 1.0% gate reads in this tree)
 *   arith   : std about the arithmetic mean / arithmetic mean  (Protocol.md:218) -- recomputed here;
 *             must equal `today`, which is the control that the recomputation is the product's formula
 *   median  : the pre-QA-A-156 code: RMS about the MEDIAN / median
 *   denom   : arithmetic-mean std / median -- the denominator swap alone
 * and whether each side of the 1.0% threshold (<= 1.0 passes) disagrees with `today`.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr double kGate = 1.0;

double meanOf(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}
double medianOf(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return n % 2u == 0u ? (v[n / 2u - 1u] + v[n / 2u]) / 2.0 : v[n / 2u];
}
double stdAbout(const std::vector<double>& v, double c) {
    double s = 0.0;
    for (double x : v) s += (x - c) * (x - c);
    return std::sqrt(s / static_cast<double>(v.size() - 1u));
}

struct Row { double today, arith, median, denom; };

Row measure(const std::vector<float>& after, uint32_t w, uint32_t h) {
    std::vector<uint16_t> before(after.size(), 1000);
    std::vector<float> gain(after.size(), 1.0f), a = after;
    XpeImageBuffer b{}, f{}, g{};
    b.data = before.data(); b.width = w; b.height = h; b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16; b.dataSize = static_cast<uint32_t>(before.size() * 2);
    f.data = a.data(); f.width = w; f.height = h; f.bitsAllocated = 32; f.bitsStored = 32;
    f.format = XPE_PIXEL_FLOAT32; f.dataSize = static_cast<uint32_t>(a.size() * 4);
    g.data = gain.data(); g.width = w; g.height = h; g.bitsAllocated = 32; g.bitsStored = 32;
    g.format = XPE_PIXEL_FLOAT32; g.dataSize = static_cast<uint32_t>(gain.size() * 4);
    XpeCalibrationMetrics m{};
    EXPECT_EQ(XPE_OK, xpe_verify_gain(&b, &f, &g, XPE_GAIN_SEMANTICS_UNKNOWN, &m));

    std::vector<double> v(after.begin(), after.end());
    const double mu = meanOf(v), md = medianOf(v);
    Row r{};
    r.today = m.prnu_after;
    r.arith = mu > 0.0 ? stdAbout(v, mu) / mu * 100.0 : 0.0;
    r.median = md > 0.0 ? stdAbout(v, md) / md * 100.0 : 0.0;
    r.denom = md > 0.0 ? stdAbout(v, mu) / md * 100.0 : 0.0;
    return r;
}

const char* verdict(double x) { return x <= kGate ? "PASS" : "FAIL"; }

void print(const char* label, const Row& r) {
    const bool splitMedian = (r.today <= kGate) != (r.median <= kGate);
    const bool splitDenom = (r.today <= kGate) != (r.denom <= kGate);
    std::printf("[a186] %-46s today=%9.4f %s | arith=%9.4f (%s) | median=%9.4f %s%s | denom=%9.4f %s%s\n",
                label, r.today, verdict(r.today), r.arith,
                std::fabs(r.arith - r.today) < 1e-9 * std::max(1.0, r.today) ? "= today" : "!= TODAY",
                r.median, verdict(r.median), splitMedian ? " SPLIT" : "",
                r.denom, verdict(r.denom), splitDenom ? " SPLIT" : "");
}

bool readU16(const std::string& path, size_t count, std::vector<uint16_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.resize(count);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(count * 2));
    return static_cast<size_t>(f.gcount()) == count * 2;
}

} // namespace

class A186Probe : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_clear_alerts(); xpe_preprocess_shutdown(); }
};

TEST_F(A186Probe, DISABLED_MeanVersusMedianFlatResidual) {
    constexpr uint32_t W = 128, H = 128;
    constexpr size_t N = static_cast<size_t>(W) * H;
    constexpr float kBase = 10000.0f;
    std::mt19937 rng(20220);
    std::normal_distribution<float> noise(0.0f, 1.0f);
    std::printf("\n[a186] FlatResidualPct, percent, gate <= %.1f. today = xpe_verify_gain\n", kGate);

    auto flat = [&](float sigmaPct) {
        std::vector<float> v(N);
        for (auto& x : v) x = kBase * (1.0f + sigmaPct / 100.0f * noise(rng));
        return v;
    };
    print("flat, noise 0.3%", measure(flat(0.3f), W, H));
    print("flat, noise 0.8%", measure(flat(0.8f), W, H));
    print("flat, noise 1.2%", measure(flat(1.2f), W, H));
    { // vignetting: smooth radial fall-off, corner drop `d` percent, plus 0.3% noise
        for (float d : {1.0f, 2.0f, 3.0f, 4.0f}) {
            std::vector<float> v = flat(0.3f);
            for (uint32_t y = 0; y < H; ++y)
                for (uint32_t x = 0; x < W; ++x) {
                    const float dx = (x - W / 2.0f) / (W / 2.0f), dy = (y - H / 2.0f) / (H / 2.0f);
                    const float r2 = std::min(1.0f, (dx * dx + dy * dy) / 2.0f);
                    v[y * W + x] *= 1.0f - d / 100.0f * r2;
                }
            char l[64]; std::snprintf(l, sizeof l, "vignetting corner drop %.0f%% + noise 0.3%%", d);
            print(l, measure(v, W, H));
        }
    }
    { // defect pixels (zero) in a flat 0.3% frame
        for (int count : {2, 8, 16, 24, 28, 40}) {
            std::vector<float> v = flat(0.3f);
            for (int i = 0; i < count; ++i) v[(i * 7919u) % N] = 0.0f;
            char l[64]; std::snprintf(l, sizeof l, "flat + %d dead pixels (of %zu)", count, N);
            print(l, measure(v, W, H));
        }
    }
    { // hot pixels
        for (int count : {1, 4, 16}) {
            std::vector<float> v = flat(0.3f);
            for (int i = 0; i < count; ++i) v[(i * 7919u) % N] = kBase * 3.0f;
            char l[64]; std::snprintf(l, sizeof l, "flat + %d hot pixels (3x)", count);
            print(l, measure(v, W, H));
        }
    }
    { // skewed: 10% of pixels raised by `amp` percent (the QA-A-155 boundary sweep)
        for (float amp : {2.8f, 3.0f, 3.1f, 3.2f, 3.3f, 3.4f, 3.6f}) {
            std::vector<float> v(N, kBase);
            for (size_t i = 0; i < N; i += 10) v[i] = kBase * (1.0f + amp / 100.0f);
            char l[64]; std::snprintf(l, sizeof l, "skewed: 10%% of pixels +%.1f%%", amp);
            print(l, measure(v, W, H));
        }
    }

    // real frames, if the fixture is reachable from this checkout
    std::printf("[a186] real fixture CalData_6 (3072x3072 uint16, read-only):\n");
    const std::string dir = "D:/workspace-github/image-processing/tests/test_data/CalData_6/";
    constexpr uint32_t RW = 3072, RH = 3072;
    constexpr size_t RN = static_cast<size_t>(RW) * RH;
    std::vector<std::vector<uint16_t>> bright(7);
    bool ok = true;
    for (int i = 1; i <= 6; ++i) {
        char name[32]; std::snprintf(name, sizeof name, "bright0%d.raw", i);
        if (!readU16(dir + name, RN, bright[i])) { std::printf("[a186]   NOT READ: %s%s\n", dir.c_str(), name); ok = false; }
    }
    if (ok) {
        for (int i = 1; i <= 6; ++i) {
            std::vector<float> v(bright[i].begin(), bright[i].end());
            char l[64]; std::snprintf(l, sizeof l, "bright0%d raw frame", i);
            print(l, measure(v, RW, RH));
        }
        // the operating point: flat-field bright05 with the gain taken from bright06
        double m6 = 0.0;
        for (uint16_t x : bright[6]) m6 += x;
        m6 /= static_cast<double>(RN);
        std::vector<float> corrected(RN);
        for (size_t i = 0; i < RN; ++i)
            corrected[i] = bright[6][i] > 0 ? static_cast<float>(bright[5][i] * (m6 / bright[6][i])) : 0.0f;
        print("bright05 flat-fielded with gain from bright06", measure(corrected, RW, RH));
    }
}
