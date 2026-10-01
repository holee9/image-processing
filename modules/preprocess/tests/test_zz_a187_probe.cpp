/**
 * @file test_zz_a187_probe.cpp
 * @brief QA-A-187 (#218) -- xpe_verify_pipeline snr_improvement_db: median centre (today) versus
 *        the arithmetic mean. Observation only; DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A187Probe.*
 *
 * xpe_verify_pipeline computes, for raw and final frame each, 20*log10(centre/std) with the
 * centre = MEDIAN and std = RMS about that median, and gates overall_pass on
 * (snr_final - snr_raw) >= 2.0 dB. For one (raw, final) pair this prints:
 *   today  : snr_improvement_db returned by xpe_verify_pipeline
 *   median : recomputed here with the median centre -- must equal `today` (control)
 *   mean   : the same formula with the arithmetic mean as the centre (what QA-A-156 adopted for
 *            the gain and offset metrics)
 * and whether the 2.0 dB gate disagrees between `today` and `mean`.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace {

constexpr double kGateDb = 2.0;

double medianOf(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return n % 2u == 0u ? (v[n / 2u - 1u] + v[n / 2u]) / 2.0 : v[n / 2u];
}
double meanOf(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}
double stdAbout(const std::vector<double>& v, double c) {
    double s = 0.0;
    for (double x : v) s += (x - c) * (x - c);
    return std::sqrt(s / static_cast<double>(v.size() - 1u));
}
double snrDb(const std::vector<double>& v, bool useMedian) {
    const double c = useMedian ? medianOf(v) : meanOf(v);
    return c > 0.0 ? 20.0 * std::log10(c / stdAbout(v, c)) : 0.0;
}

struct Result { double today, median, mean; };

Result run(const std::vector<uint16_t>& raw, const std::vector<float>& fin, uint32_t w, uint32_t h) {
    std::vector<uint16_t> r = raw;
    std::vector<float> f = fin;
    XpeImageBuffer rb{}, fb{};
    rb.data = r.data(); rb.width = w; rb.height = h; rb.bitsAllocated = 16; rb.bitsStored = 16;
    rb.format = XPE_PIXEL_UINT16; rb.dataSize = static_cast<uint32_t>(r.size() * 2);
    fb.data = f.data(); fb.width = w; fb.height = h; fb.bitsAllocated = 32; fb.bitsStored = 32;
    fb.format = XPE_PIXEL_FLOAT32; fb.dataSize = static_cast<uint32_t>(f.size() * 4);
    XpeImageMetadata meta{};
    XpeCalibrationMetrics m{};
    EXPECT_EQ(XPE_OK, xpe_verify_pipeline(&rb, &fb, &meta, &m));
    std::vector<double> rv(raw.begin(), raw.end()), fv(fin.begin(), fin.end());
    Result res{};
    res.today = m.snr_improvement_db;
    res.median = snrDb(fv, true) - snrDb(rv, true);
    res.mean = snrDb(fv, false) - snrDb(rv, false);
    return res;
}

void print(const char* label, const Result& r) {
    const bool split = (r.today >= kGateDb) != (r.mean >= kGateDb);
    std::printf("[a187] %-52s today=%8.4f dB %s | median=%8.4f (%s) | mean=%8.4f dB %s%s\n", label, r.today,
                r.today >= kGateDb ? "PASS" : "FAIL", r.median,
                std::fabs(r.median - r.today) < 1e-9 * std::max(1.0, std::fabs(r.today)) ? "= today" : "!= TODAY",
                r.mean, r.mean >= kGateDb ? "PASS" : "FAIL", split ? "  SPLIT" : "");
}

} // namespace

class A187Probe : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_clear_alerts(); xpe_preprocess_shutdown(); }
};

TEST_F(A187Probe, DISABLED_PipelineSnrMedianVersusMean) {
    constexpr uint32_t W = 128, H = 128;
    constexpr size_t N = static_cast<size_t>(W) * H;
    std::mt19937 rng(18);
    std::normal_distribution<double> g(0.0, 1.0);
    std::printf("\n[a187] xpe_verify_pipeline snr_improvement_db, gate >= %.1f dB\n", kGateDb);

    auto rawFrame = [&](double base, double sigma) {
        std::vector<uint16_t> v(N);
        for (auto& x : v) x = static_cast<uint16_t>(std::lround(std::clamp(base + sigma * g(rng), 0.0, 65535.0)));
        return v;
    };
    auto finFrame = [&](double base, double sigma) {
        std::vector<float> v(N);
        for (auto& x : v) x = static_cast<float>(base + sigma * g(rng));
        return v;
    };

    // symmetric noise reduced by a factor: the improvement is 20*log10(factor)
    for (double factor : {1.2, 1.26, 1.3, 2.0}) {
        char l[64]; std::snprintf(l, sizeof l, "symmetric noise, reduced %.2fx", factor);
        print(l, run(rawFrame(20000, 200), finFrame(20000, 200 / factor), W, H));
    }
    // raw with a bright skew (a fraction of pixels raised), final clean: the skew moves mean and median differently
    for (double frac : {0.02, 0.05, 0.10, 0.20}) {
        std::vector<uint16_t> raw = rawFrame(20000, 100);
        for (size_t i = 0; i < N; ++i)
            if (static_cast<double>((i * 2654435761u) % 1000u) / 1000.0 < frac) raw[i] = static_cast<uint16_t>(raw[i] + 1500);
        char l[64]; std::snprintf(l, sizeof l, "raw skewed (%.0f%% of pixels +1500), final clean", frac * 100);
        print(l, run(raw, finFrame(20000, 60), W, H));
    }
    // raw with dead pixels, final repaired
    for (int dead : {8, 40, 160}) {
        std::vector<uint16_t> raw = rawFrame(20000, 100);
        for (int i = 0; i < dead; ++i) raw[(i * 7919u) % N] = 0;
        char l[64]; std::snprintf(l, sizeof l, "raw with %d dead pixels, final repaired", dead);
        print(l, run(raw, finFrame(20000, 100), W, H));
    }
    // final skewed (the correction itself leaves a bright tail), raw clean -- coarse, then a sweep across the gate
    for (double amp : {300.0, 600.0, 900.0}) {
        std::vector<float> fin = finFrame(20000, 100);
        for (size_t i = 0; i < N; i += 10) fin[i] += static_cast<float>(amp);
        char l[64]; std::snprintf(l, sizeof l, "final skewed (10%% of pixels +%.0f), raw sigma 200", amp);
        print(l, run(rawFrame(20000, 200), fin, W, H));
    }
    for (double amp = 360.0; amp <= 460.0; amp += 10.0) {
        std::vector<float> fin = finFrame(20000, 100);
        for (size_t i = 0; i < N; i += 10) fin[i] += static_cast<float>(amp);
        char l[64]; std::snprintf(l, sizeof l, "sweep: final skewed (10%% of pixels +%.0f)", amp);
        print(l, run(rawFrame(20000, 200), fin, W, H));
    }
}
