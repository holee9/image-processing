/**
 * @file test_defect_fill_equivalence.cpp
 * @brief The one-pass distance fill of the defect stage gives, bit for bit, what the ring-by-ring search of QA-A-211b
 *        gave (QA-A-213, #233).
 *
 * QA-A-211b filled a masked pixel whose 3x3 holds no valid pixel from the nearest Chebyshev ring that holds one,
 * found by trying ring 2, 3, ... 16 per pixel: 638 ms for a 686x686 block (4.99% of a 3072x3072 frame). QA-A-213
 * computes the distance of every masked pixel to the nearest valid pixel in one breadth-first pass and reads only
 * that ring. The DEFINITION is unchanged, so the output must be identical, bit for bit -- and this file holds that
 * with an independent reference: the 0e8ed0fd algorithm written out here naively (4-connected clusters of >= 2 take
 * the median of the 3x3 valid pixels, else of the nearest ring up to radius 16, else keep their input value; a
 * lone defect takes the mean of its valid 4-neighbours, else of the first ring of radius 1..3 that holds any, else
 * keeps its value), compared by memcmp with xpe_defect_correct over the four frame shapes of the QA-A-212b time
 * table (scaled down) and a few hundred random masks, edges included.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "preprocess_state_fixture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <queue>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kRadius = 16;   // the search radius of QA-A-211b

// ---- the reference: the QA-A-211b definition, naively ------------------------------------------------------------
struct Ref {
    uint32_t W, H;
    const std::vector<float>& px;
    const std::vector<uint8_t>& mask;
    size_t unfilled = 0;

    bool valid(int x, int y) const {
        return x >= 0 && y >= 0 && static_cast<uint32_t>(x) < W && static_cast<uint32_t>(y) < H &&
               mask[static_cast<size_t>(y) * W + static_cast<uint32_t>(x)] == 0;
    }
    float at(int x, int y) const { return px[static_cast<size_t>(y) * W + static_cast<uint32_t>(x)]; }

    float single(int x, int y) const {
        float sum = 0.0f; int count = 0;
        auto add = [&](int nx, int ny) { if (valid(nx, ny)) { sum += at(nx, ny); ++count; } };
        add(x - 1, y); add(x + 1, y); add(x, y - 1); add(x, y + 1);
        if (count == 0) {
            for (int radius = 1; radius <= 3 && count == 0; ++radius)
                for (int dy = -radius; dy <= radius; ++dy)
                    for (int dx = -radius; dx <= radius; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
                        add(x + dx, y + dy);
                    }
        }
        return count > 0 ? sum / static_cast<float>(count) : at(x, y);
    }

    float cluster(int x, int y) {
        std::vector<float> values;
        for (int radius = 1; radius <= kRadius && values.empty(); ++radius)
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
                    if (valid(x + dx, y + dy)) values.push_back(at(x + dx, y + dy));
                }
        if (values.empty()) { ++unfilled; return at(x, y); }
        std::sort(values.begin(), values.end());
        return values[values.size() / 2u];
    }

    std::vector<float> run() {
        std::vector<float> out = px;
        std::vector<int> comp(static_cast<size_t>(W) * H, -1);
        std::vector<int> compSize;
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                const size_t i = static_cast<size_t>(y) * W + x;
                if (mask[i] == 0 || comp[i] >= 0) continue;
                const int id = static_cast<int>(compSize.size());
                int size = 0;
                std::queue<size_t> q; q.push(i); comp[i] = id;
                while (!q.empty()) {
                    const size_t c = q.front(); q.pop(); ++size;
                    const int cx = static_cast<int>(c % W), cy = static_cast<int>(c / W);
                    const int dxs[4] = {-1, 1, 0, 0}, dys[4] = {0, 0, -1, 1};
                    for (int k = 0; k < 4; ++k) {
                        const int nx = cx + dxs[k], ny = cy + dys[k];
                        if (nx < 0 || ny < 0 || static_cast<uint32_t>(nx) >= W || static_cast<uint32_t>(ny) >= H) continue;
                        const size_t ni = static_cast<size_t>(ny) * W + static_cast<uint32_t>(nx);
                        if (mask[ni] != 0 && comp[ni] < 0) { comp[ni] = id; q.push(ni); }
                    }
                }
                compSize.push_back(size);
            }
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                const size_t i = static_cast<size_t>(y) * W + x;
                if (mask[i] == 0) continue;
                out[i] = compSize[static_cast<size_t>(comp[i])] >= 2 ? cluster(static_cast<int>(x), static_cast<int>(y))
                                                                    : single(static_cast<int>(x), static_cast<int>(y));
            }
        return out;
    }
};

uint32_t g_seed = 1;
uint32_t rnd() { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }

class DefectFillEquivalenceTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir_ = (fs::temp_directory_path() / ("xpe_dfe_" + std::to_string(counter_++))).string();
        fs::create_directories(dir_);
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_clear_alerts();
        std::error_code ec;
        fs::remove_all(dir_, ec);
        XpePreprocessStateFixture::TearDown();
    }

    void loadMask(uint32_t w, uint32_t h, const std::vector<uint8_t>& m) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = w; hdr.height = h; hdr.payload_len = m.size();
        const std::string path = (fs::path(dir_) / "m.xcal").string();
        ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, m.data(), m.size()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path.c_str()));
    }

    /** product vs reference on one frame; returns the number of masked pixels the reference could not fill */
    size_t compare(const char* label, uint32_t w, uint32_t h, const std::vector<uint8_t>& mask, bool signedZeros = false) {
        std::vector<float> src(static_cast<size_t>(w) * h);
        for (size_t i = 0; i < src.size(); ++i) src[i] = static_cast<float>(rnd() % 20000) * 0.37f + 1.0f;   // all different, so the median is decided by the ring
        if (signedZeros) {   // a handful of values, half of the zeros negative: equal under < and ==, different bits
            const float pool[6] = {-0.0f, 0.0f, -0.0f, 0.0f, 1.0f, -1.0f};
            for (size_t i = 0; i < src.size(); ++i) src[i] = pool[rnd() % 6u];
        }
        for (size_t i = 0; i < src.size(); ++i) if (mask[i]) src[i] = 9.0e6f + static_cast<float>(i);           // a masked pixel's own value must never leak in
        Ref ref{w, h, src, mask};
        const std::vector<float> want = ref.run();

        loadMask(w, h, mask);
        std::vector<float> in = src, out(src.size(), -1.0f);
        XpeImageBuffer ib{}, ob{};
        ib.data = in.data(); ib.width = w; ib.height = h; ib.bitsAllocated = 32; ib.bitsStored = 32; ib.format = XPE_PIXEL_FLOAT32; ib.dataSize = in.size() * 4;
        ob = ib; ob.data = out.data();
        XpeImageMetadata meta{};
        xpe_clear_alerts();
        const XpeErrorCode rc = xpe_defect_correct(&ib, &ob, &meta);
        EXPECT_EQ(XPE_OK, rc) << label;
        if (signedZeros) {   // -0.0 == +0.0: see WhenTheRingMixesNegativeAndPositiveZero...
            bool same = true;
            for (size_t i = 0; i < want.size(); ++i) if (!(want[i] == out[i])) { same = false; break; }
            EXPECT_TRUE(same) << label << ": a filled value differs from the reference";
        } else {
            EXPECT_EQ(0, std::memcmp(want.data(), out.data(), want.size() * sizeof(float))) << label << ": output differs from the reference (bit for bit)";
        }
        // the same frame in place
        std::vector<float> ip = src;
        XpeImageBuffer pb = ib; pb.data = ip.data();
        EXPECT_EQ(XPE_OK, xpe_defect_correct(&pb, &pb, &meta));
        if (!signedZeros) EXPECT_EQ(0, std::memcmp(want.data(), ip.data(), want.size() * sizeof(float))) << label << ": in place differs";
        else { bool same = true; for (size_t i = 0; i < want.size(); ++i) if (!(want[i] == ip[i])) { same = false; break; } EXPECT_TRUE(same) << label << ": in place differs"; }
        // and the alert about pixels with no valid pixel within the radius says the reference's count
        const int32_t n = xpe_get_pending_alert_count();
        std::string found;
        for (int32_t i = 0; i < n; ++i) {
            char b[512]; int32_t sev = 0;
            if (xpe_get_pending_alert(i, b, sizeof(b), &sev) == XPE_OK && std::string(b).rfind("XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR:", 0) == 0) { found = b; break; }
        }
        if (ref.unfilled == 0) {
            EXPECT_TRUE(found.empty()) << label;
        } else {
            EXPECT_NE(std::string::npos, found.find(": " + std::to_string(ref.unfilled) + " masked pixel(s)")) << label << ": " << found;
        }
        return ref.unfilled;
    }

    std::string dir_;
    static inline int counter_ = 0;
};

std::vector<uint8_t> blank(uint32_t w, uint32_t h) { return std::vector<uint8_t>(static_cast<size_t>(w) * h, 0); }
void rect(std::vector<uint8_t>& m, uint32_t w, uint32_t r0, uint32_t c0, uint32_t rows, uint32_t cols, uint32_t h) {
    for (uint32_t r = r0; r < r0 + rows && r < h; ++r) for (uint32_t c = c0; c < c0 + cols && c < w; ++c) m[static_cast<size_t>(r) * w + c] = 1;
}

}  // namespace

// the four frame shapes of the QA-A-212b time table, scaled to 240x240
TEST_F(DefectFillEquivalenceTest, OneBigBlockWhoseDeepInteriorIsBeyondTheSearchRadius) {
    auto m = blank(240, 240); rect(m, 240, 60, 60, 100, 100, 240);
    EXPECT_GT(compare("one 100x100 block", 240, 240, m), 0u) << "control: the deep interior (distance > 16) exists, so the unfilled path is covered";
}
TEST_F(DefectFillEquivalenceTest, ABlockGridWhoseInteriorIsWithinTenPixels) {
    auto m = blank(240, 240);
    for (uint32_t r = 10; r + 20 <= 240; r += 40) for (uint32_t c = 10; c + 20 <= 240; c += 40) rect(m, 240, r, c, 20, 20, 240);
    EXPECT_EQ(0u, compare("20x20 blocks on a 40 grid", 240, 240, m));
}
TEST_F(DefectFillEquivalenceTest, ABlockGridWhoseInteriorReachesTwentyPixels) {
    auto m = blank(240, 240);
    for (uint32_t r = 10; r + 40 <= 240; r += 70) for (uint32_t c = 10; c + 40 <= 240; c += 70) rect(m, 240, r, c, 40, 40, 240);
    EXPECT_GT(compare("40x40 blocks on a 70 grid", 240, 240, m), 0u);
}
TEST_F(DefectFillEquivalenceTest, ASparseRealisticMaskOfLonePixelsAndSmallClusters) {
    auto m = blank(240, 240);
    for (int k = 0; k < 400; ++k) m[rnd() % m.size()] = 1;
    for (int k = 0; k < 40; ++k) rect(m, 240, rnd() % 230, rnd() % 230, 1 + rnd() % 5, 1 + rnd() % 5, 240);
    compare("sparse lone pixels + small clusters", 240, 240, m);
}
TEST_F(DefectFillEquivalenceTest, BlocksTouchingTheFrameEdgesAndCorners) {
    auto m = blank(60, 50);
    rect(m, 60, 0, 0, 9, 9, 50); rect(m, 60, 41, 51, 9, 9, 50); rect(m, 60, 0, 40, 4, 20, 50); rect(m, 60, 20, 0, 12, 3, 50);
    compare("edge and corner blocks", 60, 50, m);
}
TEST_F(DefectFillEquivalenceTest, AFrameWhollyMasked) {
    auto m = blank(30, 20); rect(m, 30, 0, 0, 20, 30, 20);
    EXPECT_EQ(30u * 20u, compare("every pixel masked", 30, 20, m)) << "no valid pixel anywhere: every one keeps its value";
}
TEST_F(DefectFillEquivalenceTest, ThinLinesAndDiagonals) {
    auto m = blank(80, 80);
    for (uint32_t i = 0; i < 80; ++i) { m[static_cast<size_t>(i) * 80 + i] = 1; m[static_cast<size_t>(40) * 80 + i] = 1; m[static_cast<size_t>(i) * 80 + 79 - i] = 1; }
    rect(m, 80, 10, 55, 25, 2, 80);
    compare("lines and diagonals (8-connected but not 4-connected)", 80, 80, m);
}
// QA-A-214: signed zeros. -0.0 and +0.0 are EQUAL under < and ==, so std::sort (the QA-A-211b definition) and
// std::nth_element may leave either of two equal-valued zeros at the median's place: the standard does not say which.
// QA-A-213 switched the median from a full sort to nth_element, so whether the output BITS of a filled pixel can differ
// from the old ones when the ring mixes -0.0 and +0.0 had to be measured, not assumed. Measured (QA-A-214 evidence,
// 10_diag_...txt): 300 frames of 60-90 pixels per side with big blocks (rings of up to ~100 values) whose valid pixels are
// drawn from {-0.0, +0.0, -0.0, +0.0, 1, -1}: 372,134 masked pixels, 348,053 of them filled with a zero, 172,934 of
// those -0.0 in the reference AND 172,934 in the product, and not one differing bit. That is what this STL does, not
// something the standard promises, so THIS test holds the VALUE (==, which treats the two zeros as equal) and the other
// tests in this file, whose data has no zeros, hold the bits. If a future library leaves a different zero at the
// median's place, the output is still the same number; only its sign bit may differ.
TEST_F(DefectFillEquivalenceTest, WhenTheRingMixesNegativeAndPositiveZeroTheFilledValueIsTheReferenceValue) {
    for (int t = 0; t < 120; ++t) {
        g_seed = 5000u + static_cast<uint32_t>(t);
        const uint32_t w = 60 + rnd() % 30, h = 60 + rnd() % 30;
        auto m = blank(w, h);
        for (int k = 0; k < 3; ++k) rect(m, w, rnd() % h, rnd() % w, 12 + rnd() % 30, 12 + rnd() % 30, h);
        compare(("signed zeros, frame " + std::to_string(t)).c_str(), w, h, m, true);
        if (::testing::Test::HasFailure()) break;
    }
}

// QA-A-214 (item 1): through the pipeline the defect stage cannot be handed a NaN or an infinity. Its input is the gain
// stage's output (uint16 x a reciprocal of a gain that the stage has refused unless it is finite and within [0.1, 10]:
// at most 65535 / 0.1) or, with the gain bypassed, a uint16 converted to float -- and binning refuses a non-finite
// result of its own scaling. The extreme frame below holds that to the numbers: the smallest allowed gain on the
// largest pixel value.
TEST_F(DefectFillEquivalenceTest, ThroughThePipelineTheDefectStageIsOnlyEverGivenFiniteValues) {
    constexpr uint32_t W = 20, H = 20;
    std::vector<float> g(static_cast<size_t>(W) * H, 0.1f);   // the smallest gain the range allows
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H; hdr.payload_len = g.size() * sizeof(float);
    const std::string gp = (fs::path(dir_) / "g.xcal").string();
    ASSERT_EQ(XPE_OK, write_xcal_file(gp.c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(g.data()), hdr.payload_len));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gp.c_str()));
    auto m = blank(W, H); rect(m, W, 8, 8, 3, 3, H); m[2u * W + 2u] = 1;
    loadMask(W, H, m);
    std::vector<uint8_t> bytes(static_cast<size_t>(W) * H * sizeof(float), 0);
    auto* px = reinterpret_cast<uint16_t*>(bytes.data());
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) px[i] = 65535;
    XpeImageBuffer img{};
    img.data = bytes.data(); img.width = W; img.height = H; img.bitsAllocated = 16; img.bitsStored = 16; img.format = XPE_PIXEL_UINT16; img.dataSize = bytes.size();
    XpeImageMetadata meta{};
    const char* cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGhost\":true}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, cfg));
    const float* out = reinterpret_cast<const float*>(bytes.data());
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        ASSERT_TRUE(std::isfinite(out[i])) << "pixel " << i;
        EXPECT_FLOAT_EQ(655350.0f, out[i]) << "pixel " << i << " (65535 / 0.1, the largest value the stage can see)";
    }
}

TEST_F(DefectFillEquivalenceTest, ThreeHundredRandomMasks) {
    size_t anyUnfilled = 0;
    for (int t = 0; t < 300; ++t) {
        g_seed = 1000u + static_cast<uint32_t>(t);
        const uint32_t w = 1 + rnd() % 70, h = 1 + rnd() % 70;
        auto m = blank(w, h);
        const int blobs = static_cast<int>(rnd() % 12), dots = static_cast<int>(rnd() % (w * h / 4 + 1));
        for (int k = 0; k < blobs; ++k) rect(m, w, rnd() % h, rnd() % w, 1 + rnd() % 40, 1 + rnd() % 40, h);
        for (int k = 0; k < dots; ++k) m[rnd() % m.size()] = 1;
        const std::string label = "random mask " + std::to_string(t) + " (" + std::to_string(w) + "x" + std::to_string(h) + ")";
        anyUnfilled += compare(label.c_str(), w, h, m);
        if (::testing::Test::HasFailure()) break;
    }
    EXPECT_GT(anyUnfilled, 0u) << "control: the random masks reach the beyond-radius case at least once";
}
