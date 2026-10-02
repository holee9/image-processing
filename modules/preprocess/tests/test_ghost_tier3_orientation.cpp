/**
 * @file test_ghost_tier3_orientation.cpp
 * @brief Ghost tier 3's 3x3 neighbour mean reads the incoming frame, so the result does not depend on the scan
 *        direction (QA-A-218b, #233).
 *
 * The tier updates the frame in place pixel by pixel. Its 3x3 mean used to read the frame being overwritten, so the
 * pixels above and to the left counted with their corrected, zero-clamped values and the rest with their original ones:
 * a frame flipped left-right, up-down, both, or transposed, corrected and flipped back differed from the unflipped
 * result in 41-91 % of its pixels (QA-A-218). The mean now reads the incoming frame (the copy kept for the call).
 *
 * Tolerance: flipping changes the order of the float32 sums (the 3x3 mean, and the double sum behind the frame mean
 * that is cast to float), so a flipped run is not bit identical to the unflipped one. 0.1 ADU absolute covers that
 * (nine values of at most 3e4 in float32 give about 0.02; the measured difference of an orientation-independent
 * reference was 2e-3) and is three orders of magnitude below the differences of the old behaviour (hundreds to thousands).
 */

#include <gtest/gtest.h>
#include "ghost_legacy_lag.h"

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace {

using Grid = std::vector<float>;   // S x S, row-major

Grid flipLR(const Grid& g, uint32_t S) { Grid o(g.size()); for (uint32_t y = 0; y < S; ++y) for (uint32_t x = 0; x < S; ++x) o[y * S + x] = g[y * S + (S - 1 - x)]; return o; }
Grid flipUD(const Grid& g, uint32_t S) { Grid o(g.size()); for (uint32_t y = 0; y < S; ++y) for (uint32_t x = 0; x < S; ++x) o[y * S + x] = g[(S - 1 - y) * S + x]; return o; }
Grid rot180(const Grid& g, uint32_t S) { return flipUD(flipLR(g, S), S); }
Grid transpose(const Grid& g, uint32_t S) { Grid o(g.size()); for (uint32_t y = 0; y < S; ++y) for (uint32_t x = 0; x < S; ++x) o[y * S + x] = g[x * S + y]; return o; }

struct Transform { const char* name; Grid (*fn)(const Grid&, uint32_t); };
const Transform kTransforms[] = {{"left-right", flipLR}, {"up-down", flipUD}, {"rot180", rot180}, {"transpose", transpose}};
// every one of the four is its own inverse

Grid randomGrid(uint32_t S, unsigned seed) {
    std::mt19937 g(seed);
    std::uniform_real_distribution<float> d(1000.0f, 30000.0f);
    Grid v(static_cast<size_t>(S) * S);
    for (auto& x : v) x = d(g);
    return v;
}
Grid smoothGrid(uint32_t S, unsigned seed) {   // neighbours alike: a small corrected/original difference reaches every pixel
    std::mt19937 g(seed);
    std::normal_distribution<float> noise(0.0f, 60.0f);
    Grid v(static_cast<size_t>(S) * S);
    for (uint32_t y = 0; y < S; ++y)
        for (uint32_t x = 0; x < S; ++x) {
            const float bump = ((x - S * 0.6f) * (x - S * 0.6f) + (y - S * 0.4f) * (y - S * 0.4f) < (S * 0.2f) * (S * 0.2f)) ? 8000.0f : 0.0f;
            v[y * S + x] = 9000.0f + 6000.0f * std::sin(x / 7.0f) * std::cos(y / 9.0f) + bump + noise(g);
        }
    return v;
}

// Two frames through a fresh tier-N handle, as the module runs them (acquisition times 1 and 2); returns the second.
Grid run(int tier, uint32_t S, const Grid& f1, const Grid& f2) {
    void* h = nullptr;
    const std::string cfg = "{\"tier\":\"" + std::to_string(tier) + "\"}";
    EXPECT_EQ(XPE_OK, xpe_ghost_create(S, S, withLegacyLag(cfg.c_str()).c_str(), &h));
    Grid out;
    uint64_t t = 1;
    for (const Grid* src : {&f1, &f2}) {
        Grid a = *src;
        XpeImageBuffer b{};
        b.data = a.data(); b.width = S; b.height = S; b.bitsAllocated = 32; b.bitsStored = 32; b.format = XPE_PIXEL_FLOAT32; b.dataSize = a.size() * sizeof(float);
        XpeImageMetadata m{};
        m.acquisitionTime = t++;
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &m));
        out = a;
    }
    xpe_ghost_destroy(h);
    return out;
}

// An independent derivation of tier 3 in double, the 3x3 mean read from the INCOMING frame; the history of the first
// frame is decay*0 + raw (both planes), the second frame is one time unit later.
Grid refTier3OriginalNeighbours(uint32_t S, const Grid& f1, const Grid& f2) {
    const size_t n = f2.size();
    const double a1b = 0.9, a2b = 0.05, tau1 = 1.0, tau2 = 20.0, beta = 0.1;
    (void)f1; (void)tau1; (void)tau2;
    const std::vector<float>& h = f1;                       // history planes after frame 1 (zero history, decay * 0 + raw)
    double sum = 0.0;
    for (float v : f2) sum += v;
    const double mean = sum / static_cast<double>(n);
    const double ew = 1.0 + (mean / 32768.0) * 0.5;
    Grid out(n);
    for (uint32_t y = 0; y < S; ++y)
        for (uint32_t x = 0; x < S; ++x) {
            const size_t i = static_cast<size_t>(y) * S + x;
            const double raw = f2[i];
            const double sd = 1.0 + beta * (raw / 32768.0);
            double c = raw - a1b * ew * sd * h[i] - a2b * ew * sd * h[i];
            if (x > 0 && x < S - 1 && y > 0 && y < S - 1) {
                double lm = 0.0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) lm += f2[(y + dy) * S + (x + dx)];
                c = 0.7 * c + 0.3 * (lm / 9.0);
            }
            out[i] = static_cast<float>(c > 0 ? c : 0.0);
        }
    return out;
}

double maxAbsDiff(const Grid& a, const Grid& b) {
    double m = 0.0;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i])));
    return m;
}

class GhostTier3Orientation : public XpePreprocessStateFixture {};

}  // namespace

TEST_F(GhostTier3Orientation, AFrameFlippedProcessedAndFlippedBackComesOutAsTheUnflippedOne) {
    for (const uint32_t S : {16u, 24u}) {
        for (const bool smooth : {false, true}) {
            const Grid f1 = smooth ? smoothGrid(S, 1) : randomGrid(S, 1);
            const Grid f2 = smooth ? smoothGrid(S, 2) : randomGrid(S, 2);
            const Grid base = run(3, S, f1, f2);
            for (const Transform& t : kTransforms) {
                SCOPED_TRACE(std::to_string(S) + "x" + std::to_string(S) + (smooth ? " smooth" : " random") + ", " + t.name);
                const Grid back = t.fn(run(3, S, t.fn(f1, S), t.fn(f2, S)), S);
                EXPECT_LT(maxAbsDiff(back, base), 0.1) << "the output must not depend on the direction the frame is scanned";
            }
        }
    }
}

TEST_F(GhostTier3Orientation, TheOutputIsTheTierThreeFormulaWithTheNeighboursReadFromTheIncomingFrame) {
    // an independent derivation in double (not the code's own loop): the module agrees to float32 rounding
    for (const uint32_t S : {16u, 24u}) {
        const Grid f1 = smoothGrid(S, 5), f2 = smoothGrid(S, 6);
        const Grid got = run(3, S, f1, f2);
        const Grid want = refTier3OriginalNeighbours(S, f1, f2);
        EXPECT_LT(maxAbsDiff(got, want), 0.1) << S << "x" << S;
    }
}

TEST_F(GhostTier3Orientation, TiersOneAndTwoWereAlwaysDirectionIndependentAndStillAre) {
    // their pixels never read a neighbour; the control that this test would notice a direction dependence is the
    // tier-3 test above (it is red with the old reading)
    for (const int tier : {1, 2}) {
        const uint32_t S = 16;
        const Grid f1 = smoothGrid(S, 3), f2 = smoothGrid(S, 4);
        const Grid base = run(tier, S, f1, f2);
        for (const Transform& t : kTransforms) {
            SCOPED_TRACE("tier " + std::to_string(tier) + ", " + t.name);
            EXPECT_LT(maxAbsDiff(t.fn(run(tier, S, t.fn(f1, S), t.fn(f2, S)), S), base), 0.1);
        }
    }
}
