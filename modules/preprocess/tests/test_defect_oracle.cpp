/**
 * @file test_defect_oracle.cpp
 * @brief SRS-CALIB-FUNC-019 synthetic BPM oracle (QA-A-188, #218).
 *
 * FUNC-019 (SRS-CALIB-001:150): "Synthetic BPM oracle cases shall require 100% defect recall and
 * false-positive rate below 0.001%." The metric definitions and the pass lines are the canonical
 * ones in Preprocessing-E2E-Automated-Evaluation-Protocol.md section 5.5:
 *
 *   DefectRecall      = TP / max(TP + FN, 1)                    required = 100%
 *   DefectFPR         = FP / max(FP + TN, 1)                    required < 0.001%
 *   GoodPixelDeltaP99 = percentile99(|Y(good) - Y_no_defect_stage(good)|)   required <= 1 ADU
 *
 * The truth map is known because the frames are synthetic: defects are injected at known
 * positions. The chain under test is the one the GUI runs: xpe_bpm_generate() predicts a bad pixel
 * map from dark and bright frames, then xpe_defect_correct() applies it. Recall and FPR compare the
 * predicted map with the truth map; GoodPixelDeltaP99 is taken over the pixels the truth map calls
 * good, between the frame entering the defect stage and the frame leaving it.
 *
 * Layouts are mixed on purpose: isolated pixels, 3x3 clusters, line segments, frame corners and
 * edges, in five defect kinds (dead, hot, stuck, low response, high response). The pass lines are
 * asserted as the canon states them. They are never loosened here: a miss is a defect finding.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {

constexpr uint32_t W = 256, H = 256;
constexpr size_t N = static_cast<size_t>(W) * H;
constexpr int kDark = 8, kBright = 12;

enum Kind { Dead, Hot, Stuck, LowResp, HighResp };

struct Oracle {
    std::vector<uint8_t> truth;                    // 1 = defective
    std::vector<std::vector<uint16_t>> dark, bright;
    std::vector<float> cleanBright;                // what a defect-free pixel would read in bright[0]
    int layouts[4] = {0, 0, 0, 0};                 // isolated, cluster pixels, line pixels, edge pixels
};

Oracle makeOracle(uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    Oracle o;
    o.truth.assign(N, 0);

    std::vector<double> fpnDark(N), fpnGain(N);
    for (size_t i = 0; i < N; ++i) { fpnDark[i] = 3.0 * g(rng); fpnGain[i] = 0.005 * g(rng); }

    auto apply = [&](size_t i, Kind k, bool inBright, double& v, bool isDark) {
        (void)inBright;
        switch (k) {
            case Dead:     v = 0.0; break;
            case Hot:      v += 3000.0; break;
            case Stuck:    v = 30000.0; break;
            case LowResp:  if (!isDark) v = (v - 500.0) * 0.88 + 500.0; break;
            case HighResp: if (!isDark) v = (v - 500.0) * 1.12 + 500.0; break;
        }
        (void)i;
    };

    // defect list: (index, kind)
    std::vector<std::pair<size_t, Kind>> defects;
    auto at = [&](uint32_t x, uint32_t y) { return static_cast<size_t>(y) * W + x; };
    auto add = [&](uint32_t x, uint32_t y, Kind k, int layout) {
        if (x >= W || y >= H || o.truth[at(x, y)]) return;
        o.truth[at(x, y)] = 1; defects.emplace_back(at(x, y), k); ++o.layouts[layout];
    };
    // isolated: 5 kinds x 12 positions, kept away from each other and from the frame edge
    std::uniform_int_distribution<uint32_t> pos(8, W - 9);
    for (int k = 0; k < 5; ++k)
        for (int n = 0; n < 12; ++n) {
            uint32_t x = pos(rng), y = pos(rng);
            // keep isolated pixels isolated: skip if any of the 5x5 neighbourhood is already defective
            bool free = true;
            for (int dy = -2; dy <= 2 && free; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    if (o.truth[at(x + dx, y + dy)]) { free = false; break; }
            if (free) add(x, y, static_cast<Kind>(k), 0);
        }
    // 3x3 clusters, one per kind that varies across a cluster
    const uint32_t cx[5] = {40, 100, 160, 210, 70}, cy[5] = {200, 60, 150, 220, 120};
    for (int k = 0; k < 5; ++k)
        for (uint32_t dy = 0; dy < 3; ++dy)
            for (uint32_t dx = 0; dx < 3; ++dx) add(cx[k] + dx, cy[k] + dy, static_cast<Kind>(k), 1);
    // line segments 1x8 (a dead column piece, a hot row piece)
    for (uint32_t i = 0; i < 8; ++i) { add(128, 20 + i, Dead, 2); add(30 + i, 100, Hot, 2); }
    // frame corners and edges, every kind
    add(0, 0, Dead, 3); add(W - 1, H - 1, Hot, 3); add(0, H - 1, Stuck, 3); add(W - 1, 0, LowResp, 3);
    add(0, 100, HighResp, 3); add(W - 1, 90, Dead, 3); add(100, 0, Hot, 3); add(120, H - 1, LowResp, 3);
    add(1, 1, Stuck, 3); add(W - 2, H - 2, HighResp, 3);

    std::vector<Kind> kindAt(N, Dead);
    for (auto& d : defects) kindAt[d.first] = d.second;

    auto frame = [&](bool isDark, double sigma) {
        std::vector<uint16_t> f(N);
        for (size_t i = 0; i < N; ++i) {
            double v = isDark ? 500.0 + fpnDark[i] : 500.0 + fpnDark[i] + 20000.0 * (1.0 + fpnGain[i]);
            v += sigma * g(rng);
            if (o.truth[i]) apply(i, kindAt[i], !isDark, v, isDark);
            f[i] = static_cast<uint16_t>(std::lround(std::min(65535.0, std::max(0.0, v))));
        }
        return f;
    };
    for (int i = 0; i < kDark; ++i) o.dark.push_back(frame(true, 4.0));
    for (int i = 0; i < kBright; ++i) o.bright.push_back(frame(false, 60.0));
    o.cleanBright.resize(N);
    for (size_t i = 0; i < N; ++i) o.cleanBright[i] = static_cast<float>(500.0 + fpnDark[i] + 20000.0 * (1.0 + fpnGain[i]));
    return o;
}

struct Metrics { size_t tp = 0, fp = 0, fn = 0, tn = 0; double recall = 0, fpr = 0; };

Metrics score(const std::vector<uint8_t>& truth, const std::vector<uint8_t>& predicted) {
    Metrics m;
    for (size_t i = 0; i < truth.size(); ++i) {
        const bool t = truth[i] != 0, p = predicted[i] != 0;
        if (t && p) ++m.tp; else if (!t && p) ++m.fp; else if (t) ++m.fn; else ++m.tn;
    }
    m.recall = static_cast<double>(m.tp) / static_cast<double>(std::max<size_t>(m.tp + m.fn, 1));
    m.fpr = static_cast<double>(m.fp) / static_cast<double>(std::max<size_t>(m.fp + m.tn, 1));
    return m;
}

double percentile99(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const double rank = 0.99 * static_cast<double>(v.size() - 1);
    const size_t lo = static_cast<size_t>(rank);
    const size_t hi = std::min(lo + 1, v.size() - 1);
    return v[lo] + (rank - static_cast<double>(lo)) * (v[hi] - v[lo]);
}

XpeImageBuffer u16(std::vector<uint16_t>& p) {
    XpeImageBuffer b{};
    b.data = p.data(); b.width = W; b.height = H; b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16; b.dataSize = static_cast<uint32_t>(p.size() * 2);
    return b;
}
XpeImageBuffer f32(std::vector<float>& p) {
    XpeImageBuffer b{};
    b.data = p.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32; b.dataSize = static_cast<uint32_t>(p.size() * 4);
    return b;
}

std::vector<uint8_t> predictBpm(Oracle& o) {
    std::vector<XpeImageBuffer> d, b;
    for (auto& f : o.dark) d.push_back(u16(f));
    for (auto& f : o.bright) b.push_back(u16(f));
    std::vector<uint8_t> bpm(N, 0);
    XpeImageBuffer out{};
    out.data = bpm.data(); out.width = W; out.height = H; out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8; out.dataSize = static_cast<uint32_t>(bpm.size());
    EXPECT_EQ(XPE_OK, xpe_bpm_generate(d.data(), kDark, b.data(), kBright, nullptr, &out));
    return bpm;
}

class DefectOracle : public ::testing::Test {
protected:
    const char* path = "test_defect_oracle_map.xcal";
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        std::remove(path); std::remove("test_defect_oracle_map.xcal.tmp");
        xpe_clear_alerts(); xpe_preprocess_shutdown();
    }
    /** Loads `bpm` as the defect map and returns GoodPixelDeltaP99 over the truth-good pixels, plus the corrected frame. */
    double goodPixelDeltaP99(const Oracle& o, const std::vector<uint8_t>& bpm, std::vector<float>* correctedOut = nullptr,
                             std::vector<float>* inputOut = nullptr) {
        std::remove(path);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = XCAL_TYPE_DEFECT; hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W; hdr.height = H; hdr.payload_len = bpm.size();
        EXPECT_EQ(XPE_OK, write_xcal_file(path, hdr, nullptr, 0, bpm.data(), hdr.payload_len));
        EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(path));

        std::vector<float> in(N), out(N, -1.0f);
        for (size_t i = 0; i < N; ++i) in[i] = static_cast<float>(o.bright[0][i]);
        XpeImageBuffer ib = f32(in), ob = f32(out);
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_defect_correct(&ib, &ob, &meta));
        std::vector<double> deltas;
        for (size_t i = 0; i < N; ++i)
            if (!o.truth[i]) deltas.push_back(std::fabs(static_cast<double>(out[i]) - static_cast<double>(in[i])));
        if (correctedOut) *correctedOut = out;
        if (inputOut) *inputOut = in;
        return percentile99(deltas);
    }
};

} // namespace

// FUNC-019: the canonical pass lines, as the canon states them. Three seeds, so one lucky frame cannot pass.
TEST_F(DefectOracle, RecallFprAndGoodPixelDeltaMeetTheCanonicalLines) {
    for (uint32_t seed : {1u, 2u, 3u}) {
        Oracle o = makeOracle(seed);
        const std::vector<uint8_t> predicted = predictBpm(o);
        const Metrics m = score(o.truth, predicted);
        const double p99 = goodPixelDeltaP99(o, predicted);
        std::printf("[oracle] seed=%u truth=%zu (isolated %d, cluster %d, line %d, edge %d) TP=%zu FN=%zu FP=%zu "
                    "recall=%.4f%% FPR=%.6f%% GoodPixelDeltaP99=%.4f ADU\n",
                    seed, m.tp + m.fn, o.layouts[0], o.layouts[1], o.layouts[2], o.layouts[3], m.tp, m.fn, m.fp,
                    100.0 * m.recall, 100.0 * m.fpr, p99);
        EXPECT_EQ(m.fn, 0u) << "seed " << seed << ": DefectRecall must be 100% (FN=" << m.fn << ", recall=" << 100.0 * m.recall << "%)";
        EXPECT_DOUBLE_EQ(1.0, m.recall) << "seed " << seed;
        EXPECT_LT(100.0 * m.fpr, 0.001) << "seed " << seed << ": DefectFPR must be < 0.001% (FP=" << m.fp << ")";
        EXPECT_LE(p99, 1.0) << "seed " << seed << ": GoodPixelDeltaP99 must be <= 1 ADU";
    }
}

// Control for the metric: one defect missed from a perfect prediction must break the recall line.
TEST_F(DefectOracle, ControlOneMissedDefectBreaksRecall) {
    Oracle o = makeOracle(1);
    std::vector<uint8_t> predicted = o.truth;                  // a perfect prediction
    EXPECT_DOUBLE_EQ(1.0, score(o.truth, predicted).recall);
    size_t first = 0;
    while (!predicted[first]) ++first;
    predicted[first] = 0;                                      // miss exactly one defect
    const Metrics m = score(o.truth, predicted);
    EXPECT_EQ(1u, m.fn);
    EXPECT_LT(m.recall, 1.0);
}

// Control for the metric: one good pixel flagged must show up in FPR.
TEST_F(DefectOracle, ControlOneFalseAlarmShowsInFpr) {
    Oracle o = makeOracle(1);
    std::vector<uint8_t> predicted = o.truth;
    size_t good = 0;
    while (o.truth[good]) ++good;
    predicted[good] = 1;
    const Metrics m = score(o.truth, predicted);
    EXPECT_EQ(1u, m.fp);
    EXPECT_GT(100.0 * m.fpr, 0.0);
    // FPR < 0.001% on a 65536-pixel frame means no false alarm at all: one is 0.0015%.
    EXPECT_GE(100.0 * m.fpr, 0.001);
}

// Control for GoodPixelDeltaP99: a map that flags many good pixels does alter them, and the line catches it.
TEST_F(DefectOracle, ControlFlaggingManyGoodPixelsBreaksGoodPixelDelta) {
    Oracle o = makeOracle(1);
    std::vector<uint8_t> predicted = o.truth;
    // flag every 7th good pixel (~14% of the frame): the correction replaces them with a neighbour model
    size_t k = 0;
    for (size_t i = 0; i < N; ++i)
        if (!o.truth[i] && (k++ % 7u) == 0u) predicted[i] = 1;
    EXPECT_GT(goodPixelDeltaP99(o, predicted), 1.0);
}
