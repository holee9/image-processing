/**
 * @file test_ghost_tier3_border.cpp
 * @brief Tier 3 blends EVERY pixel with the mean of its neighbours, the border included (QA-A-227, #244).
 *
 * Measured before the change (QA-A-225 M3/M4): tier 3 blended `0.7*corrected + 0.3*(3x3 mean of the incoming frame)` only for
 * pixels with all eight neighbours, so on a perfectly uniform frame the interior sat above the one-pixel border by
 * `0.3*(raw - border value)` (0.6 % to 8.4 % of the level, up to 1193 ADU): a ring artifact, and the blend's own cost
 * (it hands back 30 % of the removed lag) paid by the interior only. `git blame` puts the condition in 31d38a57
 * (2026-04-19, "Ghost Correction 고도화"); neither that commit, the SRS texts nor the verification guide give a reason for
 * excluding the border -- the guide says "3x3 spatial context blend" without an exception, and the index arithmetic
 * `(y+dy)*W + (x+dx)` wraps to the neighbouring row at x = 0, which the guard avoids. It reads as a safety guard, not a design.
 * Whether the blend should exist at all is #238, not this file.
 *
 * The rule now: the mean is taken over the neighbours that EXIST in the frame (9 inside, 6 on an edge, 4 in a corner), the
 * module's convention elsewhere (the defect stage's "valid neighbours"). The 3x3 gate of the whole blend (a frame smaller than
 * 3x3 has no spatial context and is not blended) is unchanged.
 */

#include <gtest/gtest.h>
#include "ghost_stable_lag.h"
#include "ghost_oracle_harness.h"

#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

using Grid = std::vector<float>;

class GhostTier3Border : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }
};

XpeImageBuffer bufOf(Grid& g, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = g.data(); b.width = w; b.height = h; b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32; b.dataSize = g.size() * sizeof(float);
    return b;
}

// the lag set of ghost_stable_lag.h, tier given
std::string cfg(int tier) { return withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str()); }

/** Runs `frames` through a fresh tier-`tier` handle of size w x h; returns the last output. */
Grid run(int tier, uint32_t w, uint32_t h, const std::vector<Grid>& frames) {
    void* hnd = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(w, h, cfg(tier).c_str(), &hnd));
    Grid out;
    for (const Grid& f : frames) {
        out = f;
        XpeImageBuffer b = bufOf(out, w, h);
        XpeImageMetadata m{};
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(hnd, &b, &m));
    }
    xpe_ghost_destroy(hnd);
    return out;
}

Grid randomGrid(uint32_t w, uint32_t h, unsigned seed) {
    std::mt19937 g(seed);
    std::uniform_real_distribution<float> d(3000.0f, 20000.0f);
    Grid v(static_cast<size_t>(w) * h);
    for (auto& x : v) x = d(g);
    return v;
}

/** An independent derivation of tier 3 in double for the SECOND frame of a two-frame sequence (the history of the first frame is
 *  decay*0 + raw in both planes): the blend over the neighbours that exist, a frame below 3x3 not blended at all. */
Grid refSecondFrame(uint32_t w, uint32_t h, const Grid& f1, const Grid& f2) {
    const double a1b = 0.1, a2b = 0.01, beta = 0.1;   // the lag set of ghost_stable_lag.h, nlcscBeta default
    double sum = 0.0;
    for (float v : f2) sum += v;
    const double mean = sum / static_cast<double>(f2.size());
    const double ew = 1.0 + (mean / 32768.0) * 0.5;
    Grid out(f2.size());
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            const double raw = f2[i];
            const double sd = 1.0 + beta * (raw / 32768.0);
            double c = raw - a1b * ew * sd * f1[i] - a2b * ew * sd * f1[i];
            if (w >= 3 && h >= 3) {
                double lm = 0.0;
                int cnt = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int yy = static_cast<int>(y) + dy, xx = static_cast<int>(x) + dx;
                        if (yy < 0 || xx < 0 || yy >= static_cast<int>(h) || xx >= static_cast<int>(w)) continue;
                        lm += f2[static_cast<size_t>(yy) * w + xx];
                        ++cnt;
                    }
                c = 0.7 * c + 0.3 * (lm / cnt);
            }
            out[i] = static_cast<float>(c > 0.0 ? c : 0.0);
        }
    return out;
}

double maxAbsDiff(const Grid& a, const Grid& b) {
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i])));
    return m;
}

const struct { uint32_t w, h; } kSizes[] = {{3, 3}, {4, 4}, {5, 7}, {7, 5}, {9, 9}, {16, 16}, {3, 16}, {16, 3}, {2, 2}, {1, 1}, {2, 9}, {9, 2}};

}  // namespace

TEST_F(GhostTier3Border, AUniformFrameComesOutUniformAtEverySize) {
    // two uniform frames of different levels: the second is corrected against a uniform history, so every pixel must get the same
    // value -- before QA-A-227 the interior sat above the border by 0.3 * (raw - corrected value)
    for (const auto& s : kSizes) {
        const size_t n = static_cast<size_t>(s.w) * s.h;
        const Grid f1(n, 12000.0f), f2(n, 15000.0f);
        const Grid out = run(3, s.w, s.h, {f1, f2});
        const auto mm = std::minmax_element(out.begin(), out.end());
        EXPECT_LT(static_cast<double>(*mm.second) - static_cast<double>(*mm.first), 1e-2)
            << s.w << "x" << s.h << ": min " << *mm.first << " max " << *mm.second;
    }
}

TEST_F(GhostTier3Border, EveryPixelFollowsTheBlendOverTheNeighboursThatExist) {
    for (const auto& s : kSizes) {
        const Grid f1 = randomGrid(s.w, s.h, 1), f2 = randomGrid(s.w, s.h, 2);
        const Grid got = run(3, s.w, s.h, {f1, f2});
        const Grid want = refSecondFrame(s.w, s.h, f1, f2);
        EXPECT_LT(maxAbsDiff(got, want), 0.05) << s.w << "x" << s.h;
    }
}

TEST_F(GhostTier3Border, ATierOneOrTwoFrameIsNotBlendedAnywhere) {
    // tiers 1 and 2 never read a neighbour: a frame whose pixels are all different keeps its per-pixel correction exactly, border and
    // interior alike (the pixel value times a factor; no pixel depends on another)
    for (const int tier : {1, 2}) {
        const uint32_t w = 6, h = 5;
        const Grid f1 = randomGrid(w, h, 3);
        Grid f2 = f1;                       // the same frame again: every pixel's correction is the same function of its own history
        const Grid a = run(tier, w, h, {f1, f2});
        // shuffle the frame's pixels spatially and run again: a per-pixel corrector gives the shuffled result, whatever the spatial order
        Grid p1 = f1, p2 = f2;
        std::reverse(p1.begin(), p1.end());
        std::reverse(p2.begin(), p2.end());
        Grid b = run(tier, w, h, {p1, p2});
        std::reverse(b.begin(), b.end());
        // tier 2 uses the frame MEAN, which a reversal does not change
        EXPECT_EQ(0.0, maxAbsDiff(a, b)) << "tier " << tier;
    }
}

// Evidence for "tiers 1 and 2 are unchanged": the digest of each tier on the noisy T1 27 % exposure sequence. Run on the code before
// and after the QA-A-227 change: tiers 1 and 2 print the same value, tier 3 differs (the border pixels are now blended).
TEST_F(GhostTier3Border, PrintsTheTierDigestsForTheBeforeAfterComparison) {
    using namespace ghost_oracle;
    const Seq s = exposureSeq(0.27);
    const auto y = makeFrames(truthLti(s.x), 5.0, 7);
    for (const int tier : {1, 2, 3}) {
        int failures = 0;
        const Frames out = runModule(lagCfg(tier, 0.02, 3.0, 0.003, 30.0).c_str(), y, &failures);
        EXPECT_EQ(0, failures);
        std::printf("[ghost-oracle] tier%d-digest 0x%016llx\n", tier, static_cast<unsigned long long>(digestOf(out)));
    }
}

// ---- QA-A-227b (#244, Codex #89): the neighbour span in unsigned arithmetic, tested at axis lengths above INT_MAX ------------
//
// The first border blend cast the pixel coordinate and the frame width/height to int. A handle may have an axis above INT_MAX
// (xpe_ghost_create does not cap it), where static_cast<int>(W) is negative and every neighbour counted as outside the frame. A
// frame that size cannot be allocated in a test, so the bound computation is a function of two numbers
// (xpe_ghost_neighbour_span) and is tested on the numbers.

namespace {

/** The span the way the first version computed it: signed int, as a count of in-frame neighbours along one axis. */
int firstVersionNeighbourCount(size_t coord, size_t extent) {
    const int c = static_cast<int>(coord), e = static_cast<int>(extent);
    int n = 0;
    for (int d = -1; d <= 1; ++d) {
        const int v = c + d;
        if (v < 0 || v >= e) continue;
        ++n;
    }
    return n;
}

const uint64_t kExtents[] = {1, 2, 3, 4, 5, 1000, 0x7FFFFFFEull, 0x7FFFFFFFull, 0x80000000ull, 0x80000001ull, 0xFFFFFFFEull, 0xFFFFFFFFull};

}  // namespace

TEST(GhostTier3NeighbourSpan, MatchesAnIndependentWideSignedComputationAtEveryAxisLength) {
    for (const uint64_t ext : kExtents) {
        const uint64_t coords[] = {0, 1, 2, ext / 2, ext >= 3 ? ext - 3 : 0, ext >= 2 ? ext - 2 : 0, ext - 1};
        for (const uint64_t c : coords) {
            if (c >= ext) continue;
            size_t lo = 99, hi = 99;
            xpe_ghost_neighbour_span(static_cast<size_t>(c), static_cast<size_t>(ext), &lo, &hi);
            // the expectation: 64-bit signed arithmetic, written from the definition "the cells c-1..c+1 that exist"
            const int64_t wantLo = std::max<int64_t>(static_cast<int64_t>(c) - 1, 0);
            const int64_t wantHi = std::min<int64_t>(static_cast<int64_t>(c) + 1, static_cast<int64_t>(ext) - 1);
            EXPECT_EQ(static_cast<uint64_t>(wantLo), static_cast<uint64_t>(lo)) << "extent " << ext << " coord " << c;
            EXPECT_EQ(static_cast<uint64_t>(wantHi), static_cast<uint64_t>(hi)) << "extent " << ext << " coord " << c;
            // and the count the blend divides by: 3 inside, 2 on an edge of a longer axis, 1 when the axis is one cell
            const uint64_t count = static_cast<uint64_t>(hi) - static_cast<uint64_t>(lo) + 1;
            const bool edge = (c == 0 || c == ext - 1);
            const uint64_t wantCount = ext == 1 ? 1 : (edge ? 2 : 3);
            EXPECT_EQ(wantCount, count) << "extent " << ext << " coord " << c;
        }
    }
}

TEST(GhostTier3NeighbourSpan, TheFirstVersionLostEveryNeighbourAboveIntMaxAndTheSpanDoesNot) {
    // the control that makes the test above able to see the defect: at extent 0x80000000 the int cast is INT_MIN, so the first
    // version found 0 neighbours for an interior coordinate where the span finds 3
    const size_t ext = 0x80000000u, c = 1000;
    EXPECT_EQ(0, firstVersionNeighbourCount(c, ext)) << "the defect, reproduced on the numbers";
    size_t lo = 0, hi = 0;
    xpe_ghost_neighbour_span(c, ext, &lo, &hi);
    EXPECT_EQ(3u, hi - lo + 1);
    // and the same two functions agree where the first version was right (an ordinary axis)
    for (size_t coord = 0; coord < 7; ++coord) {
        xpe_ghost_neighbour_span(coord, 7, &lo, &hi);
        EXPECT_EQ(static_cast<size_t>(firstVersionNeighbourCount(coord, 7)), hi - lo + 1) << "coord " << coord;
    }
}
