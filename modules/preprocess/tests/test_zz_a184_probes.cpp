/**
 * @file test_zz_a184_probes.cpp
 * @brief QA-A-184 (#232) -- REQ-P1A-041 line-noise probe. Observation only.
 *
 * All DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A184Probes.*
 *
 * (2) REQ-P1A-041 line noise: what does xpe_validate_readout_artifact report on
 *     frames that DO contain row- or column-correlated noise, and do any frames
 *     in the repository contain such noise? A "line-noise index" is computed from
 *     the frame itself, so the claim "this frame has line noise" is measured.
 *
 * The other QA-A-184 probes were promoted or superseded in QA-A-185 (#232): the shared ghost
 * handle race is now GhostThreadSafety.* (test_ghost_thread_safety.cpp) and the dimension
 * mismatch return codes are CalibDimMismatch.* (test_calib_dim_mismatch.cpp). This one stays
 * because REQ-P1A-041's line-noise check is unimplemented and the measurement below is the
 * evidence for that statement.
 *
 * Nothing here is part of the product or of any requirement.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// (2) line noise
// ---------------------------------------------------------------------------

XpeImageBuffer u16Buffer(std::vector<uint16_t>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = px.data(); b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = static_cast<uint32_t>(px.size() * sizeof(uint16_t));
    return b;
}

double medianOf(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2u];
}

/** Robust spread (MAD * 1.4826) of v. */
double robustSpread(const std::vector<double>& v) {
    const double m = medianOf(v);
    std::vector<double> d(v.size());
    for (size_t i = 0; i < v.size(); ++i) d[i] = std::fabs(v[i] - m);
    return 1.4826 * medianOf(d);
}

/**
 * Line-noise index. Pixel noise sigma comes from neighbour differences along the
 * line (robust, so edges barely count). The first difference of the line means
 * has spread sigma*sqrt(2/len) when only pixel noise is present; the index is
 * observed / expected. ~1 = no line-correlated noise; >> 1 = line noise (or a
 * strong gradient perpendicular to the lines).
 */
double lineNoiseIndex(const uint16_t* px, uint32_t w, uint32_t h, bool rows, int order) {
    const uint32_t lines = rows ? h : w;
    const uint32_t len   = rows ? w : h;
    auto at = [&](uint32_t line, uint32_t i) -> double {
        return rows ? px[static_cast<size_t>(line) * w + i] : px[static_cast<size_t>(i) * w + line];
    };
    std::vector<double> nd;
    nd.reserve(static_cast<size_t>(lines) * (len - 1u) / 8u + 1u);
    for (uint32_t l = 0; l < lines; l += 4u)              // every 4th line is plenty
        for (uint32_t i = 0; i + 1u < len; ++i) nd.push_back(at(l, i + 1u) - at(l, i));
    const double sigmaPx = robustSpread(nd) / std::sqrt(2.0);

    std::vector<double> means(lines, 0.0);
    for (uint32_t l = 0; l < lines; ++l) {
        double s = 0.0;
        for (uint32_t i = 0; i < len; ++i) s += at(l, i);
        means[l] = s / len;
    }
    // order 1: first difference of the line means (variance 2*s^2/len for white noise);
    // order 2: second difference (variance 6*s^2/len), which cancels a smooth gradient --
    // anatomy changes the first difference of the means but barely the second.
    std::vector<double> e;
    e.reserve(lines);
    if (order == 1) {
        for (uint32_t l = 0; l + 1u < lines; ++l) e.push_back(means[l + 1u] - means[l]);
    } else {
        for (uint32_t l = 0; l + 2u < lines; ++l) e.push_back(means[l + 2u] - 2.0 * means[l + 1u] + means[l]);
    }
    const double observed = robustSpread(e);
    const double expected = sigmaPx * std::sqrt((order == 1 ? 2.0 : 6.0) / len);
    return expected > 0.0 ? observed / expected : 0.0;
}

struct FrameStats {
    double maxRowMean = 0.0; int rowsOver = 0;
    double rowIdx1 = 0.0, colIdx1 = 0.0, rowIdx2 = 0.0, colIdx2 = 0.0;
};

FrameStats statsOf(const uint16_t* px, uint32_t w, uint32_t h) {
    FrameStats s;
    const double lim = 0.9 * 65535.0;
    for (uint32_t y = 0; y < h; ++y) {
        double sum = 0.0;
        for (uint32_t x = 0; x < w; ++x) sum += px[static_cast<size_t>(y) * w + x];
        const double m = sum / w;
        s.maxRowMean = std::max(s.maxRowMean, m);
        if (m > lim) ++s.rowsOver;
    }
    s.rowIdx1 = lineNoiseIndex(px, w, h, true, 1);
    s.colIdx1 = lineNoiseIndex(px, w, h, false, 1);
    s.rowIdx2 = lineNoiseIndex(px, w, h, true, 2);
    s.colIdx2 = lineNoiseIndex(px, w, h, false, 2);
    return s;
}

void reportFrame(const char* label, std::vector<uint16_t>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer img = u16Buffer(px, w, h);
    XpeImageMetadata meta{};
    bool dropped = false, nonuni = false;
    const XpeErrorCode rc = xpe_validate_readout_artifact(&img, &meta, &dropped, &nonuni);
    const FrameStats s = statsOf(px.data(), w, h);
    std::printf("[a184] %-44s rc=%d dropped=%d nonuniform_gain=%d | rowsOver0.9FS=%d maxRowMean=%.0f | "
                "LNI1 rows=%.2f cols=%.2f | LNI2 rows=%.2f cols=%.2f\n",
                label, static_cast<int>(rc), dropped ? 1 : 0, nonuni ? 1 : 0, s.rowsOver, s.maxRowMean,
                s.rowIdx1, s.colIdx1, s.rowIdx2, s.colIdx2);
}

std::vector<uint16_t> synth(uint32_t w, uint32_t h, double base, double sigmaPx,
                            double rowSigma, double colSigma, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> rowOff(h, 0.0), colOff(w, 0.0);
    for (auto& v : rowOff) v = rowSigma * g(rng);
    for (auto& v : colOff) v = colSigma * g(rng);
    std::vector<uint16_t> px(static_cast<size_t>(w) * h);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const double v = base + sigmaPx * g(rng) + rowOff[y] + colOff[x];
            px[static_cast<size_t>(y) * w + x] =
                static_cast<uint16_t>(std::min(65535.0, std::max(0.0, std::round(v))));
        }
    return px;
}

bool readRaw16(const std::string& path, size_t count, std::vector<uint16_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.resize(count);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(count * sizeof(uint16_t)));
    return static_cast<size_t>(f.gcount()) == count * sizeof(uint16_t);
}

/** Repository root, from this file's compile-time path (.../modules/preprocess/tests/<file>). */
std::string repoRoot() {
    std::string p = __FILE__;
    for (char& c : p) if (c == '\\') c = '/';
    for (int i = 0; i < 4; ++i) {
        const size_t k = p.find_last_of('/');
        if (k == std::string::npos) return std::string();
        p.resize(k);
    }
    return p;
}

} // namespace

class A184Probes : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

// (2) -----------------------------------------------------------------------
TEST_F(A184Probes, DISABLED_LineNoiseVersusTheReadoutCheck) {
    constexpr uint32_t W = 1024, H = 1024;
    std::printf("\n[a184] (2) xpe_validate_readout_artifact vs frames with and without line noise (%ux%u UINT16)\n", W, H);
    std::printf("[a184] LNI1/LNI2 = spread of the first/second difference of the line means / expected from pixel noise alone"
                " (~1 = no line-correlated noise); LNI2 cancels a smooth gradient such as anatomy\n");

    // synthetic cases: base, pixel sigma, row sigma, col sigma
    { auto f = synth(W, H, 20000, 20, 0,   0,   1u); reportFrame("synthetic clean (base 20000, px 20)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 300, 0,   2u); reportFrame("synthetic ROW line noise (sigma 300)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 0,   300, 3u); reportFrame("synthetic COLUMN line noise (sigma 300)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 1500, 0,  4u); reportFrame("synthetic ROW line noise (sigma 1500)", f, W, H); }
    { auto f = synth(W, H, 58000, 20, 3000, 0,  5u); reportFrame("synthetic ROW noise 3000 near full scale", f, W, H); }
    { auto f = synth(W, H, 62000, 20, 0,   0,   8u); reportFrame("control: CLEAN but bright (base 62000)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 0,   0,   6u);
      for (uint32_t y = 0; y < H; ++y) f[static_cast<size_t>(y) * W + 500] = 0;
      reportFrame("control: one all-zero column", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 0,   0,   7u);
      for (uint32_t x = 0; x < W; ++x) f[static_cast<size_t>(300) * W + x] = 65535;
      reportFrame("control: one saturated row", f, W, H); }

    // frames that exist in the repository
    const std::string root = repoRoot();
    std::printf("[a184] repository frames (root=%s)\n", root.c_str());
    struct Item { const char* rel; uint32_t w, h; };
    const Item items[] = {
        {"gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw", 3072, 3072},
        {"gui/ImageProcTest/fixtures/gui-s0/raw/synthetic_1024x1024.raw", 1024, 1024},
        {"test_data/synthetic_512x512.raw", 512, 512},
        {".moai/reports/lane-pre/QA-A-51/frames/uniform.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/lines.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/scatter.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/edge.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/checker8.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/diag.raw", 1024, 1024},
    };
    for (const Item& it : items) {
        std::vector<uint16_t> px;
        if (!readRaw16(root + "/" + it.rel, static_cast<size_t>(it.w) * it.h, px)) {
            std::printf("[a184] %-44s NOT READ (%s)\n", it.rel, "missing or short");
            continue;
        }
        reportFrame(it.rel, px, it.w, it.h);
    }
}

// QA-A-185: cost of the per-handle mutex on the single-threaded path. Run the SAME probe from a build
// with and from a build without the mutex, alternating processes (see the QA-A-185 evidence).
// Prints one line: median wall time of one xpe_ghost_correct call, ms, over kFrames frames.
TEST_F(A184Probes, DISABLED_GhostSingleThreadTime) {
    for (uint32_t side : {64u, 1024u}) {
        const int frames = side == 64u ? 20000 : 150;
        void* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_ghost_create(side, side, nullptr, &h));
        std::vector<float> px(static_cast<size_t>(side) * side, 1000.0f);
        XpeImageBuffer img{};
        img.data = px.data(); img.width = side; img.height = side;
        img.bitsAllocated = 32; img.bitsStored = 32; img.format = XPE_PIXEL_FLOAT32;
        img.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
        XpeImageMetadata meta{};
        std::vector<double> ms;
        for (int i = 0; i < frames + 10; ++i) {
            std::fill(px.begin(), px.end(), 1000.0f);
            meta.acquisitionTime = static_cast<uint64_t>(i + 1);
            const auto t0 = std::chrono::steady_clock::now();
            ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &img, &meta));
            const auto t1 = std::chrono::steady_clock::now();
            if (i >= 10) ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        std::sort(ms.begin(), ms.end());
        std::printf("[a185] ghost_correct %ux%u frames=%d median_ms=%.6f p10_ms=%.6f\n", side, side, frames,
                    ms[ms.size() / 2], ms[ms.size() / 10]);
        xpe_ghost_destroy(h);
    }
}
