/**
 * @file test_runtime_detection_median8_parity.cpp
 * @brief QA-A-54 (#144): the 8-value median fast path returns the identical
 *        float the generic path returns, and the detector's map is unchanged
 *        pixel for pixel.
 *
 * The optimisation this pins is a pure dispatch: ComputeMedian sends the
 * 8-element case (every interior pixel of the 3x3 window) to a Batcher sorting
 * network instead of std::nth_element + std::max_element. The network fully
 * sorts, so it selects the same two order statistics and returns the same sum
 * times the same constant.
 *
 * "Same" here means BITWISE, not "close". A median that differs in the last
 * ulp changes a Hampel decision at the boundary, and a changed decision would
 * silently invalidate every table QA-A-42..A-50 measured. EXPECT_FLOAT_EQ
 * tolerates 4 ulp and is therefore the wrong assertion for this claim; these
 * tests compare the raw bits.
 *
 * ONE MEASURED EXCEPTION, and it is narrow enough to state exactly. When the
 * two selected order statistics are zeros of OPPOSITE SIGN, the two paths may
 * place them differently (-0.0 and +0.0 compare equal, so neither selection is
 * more correct), and the sum is then -0.0 on one side and +0.0 on the other.
 * The values are numerically equal; only the sign bit differs. It cannot change
 * a detection, because the median reaches the rule only through
 * |centre - median| and |x - median|, and std::abs maps both zeros to +0.0 --
 * ZeroSignDifferenceCannotChangeADecision below asserts that directly, and
 * DetectionMapIsIdenticalPixelForPixel demonstrates it end to end. The first
 * test therefore asserts "same bits, or both zero" rather than being relaxed to
 * a tolerance: a 1-ulp difference anywhere else still fails it.
 *
 * Scope note: NaN input is excluded. A NaN makes `<` inconsistent, which breaks
 * std::nth_element's strict-weak-ordering precondition -- the generic path is
 * undefined there, not merely different, so there is no "old behaviour" to be
 * identical to.
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013.  Refs #144 #143
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"

#include <cstring>
#include <random>
#include <vector>

namespace {

using xpe::preprocess::internal::ComputeMedian;
using xpe::preprocess::internal::ComputeMedianGeneric;
using xpe::preprocess::internal::CollectNeighborValues;
using xpe::preprocess::internal::ComputeMAD;
using xpe::preprocess::internal::DetectDefectivePixel;

/** Bitwise float equality: no tolerance, because the claim has none. */
bool SameBits(float a, float b) {
    uint32_t ba = 0, bb = 0;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

/**
 * Same bits, or both zero. The second clause covers exactly one measured case:
 * -0.0 vs +0.0 when the selected pair are zeros of opposite sign. Everything
 * else -- including a single-ulp difference in any non-zero value -- still fails.
 */
bool SameBitsOrBothZero(float a, float b) {
    if (SameBits(a, b)) return true;
    return (a == 0.0f && b == 0.0f);
}

}  // namespace

/**
 * The whole behavioural surface of the change: an 8-element vector in, one
 * float out. Random values, then the cases a random generator almost never
 * produces -- ties, all-equal, negatives, zero-crossing, extreme magnitudes.
 */
TEST(Median8ParityTest, FastPathMatchesGenericBitForBit) {
    std::mt19937 rng(20260912u);
    std::uniform_real_distribution<float> wide(-5.0e4f, 5.0e4f);
    std::uniform_int_distribution<int> small(-3, 3);

    size_t checked = 0;
    for (int trial = 0; trial < 200000; ++trial) {
        std::vector<float> v(8);
        if (trial % 3 == 0) {
            // Heavy ties: small integers repeat constantly.
            for (float& f : v) f = static_cast<float>(small(rng));
        } else {
            for (float& f : v) f = wide(rng);
        }
        std::vector<float> copy = v;

        const float fast = ComputeMedian(v);
        const float slow = ComputeMedianGeneric(copy);
        ASSERT_TRUE(SameBitsOrBothZero(fast, slow))
            << "trial " << trial << ": fast " << fast << " vs generic " << slow;
        ++checked;
    }
    EXPECT_EQ(200000u, checked);

    const std::vector<std::vector<float>> corners = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {-1, -1, -1, -1, 1, 1, 1, 1},
        {8, 7, 6, 5, 4, 3, 2, 1},
        {1, 2, 3, 4, 5, 6, 7, 8},
        {-0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f},
        {1e-30f, 1e30f, -1e30f, -1e-30f, 0.0f, 1.0f, -1.0f, 2.0f},
    };
    for (const std::vector<float>& c : corners) {
        std::vector<float> a = c, b = c;
        EXPECT_TRUE(SameBitsOrBothZero(ComputeMedian(a), ComputeMedianGeneric(b)));
    }
}

/**
 * The one exception, pinned rather than hidden: alternating -0.0 / +0.0 is the
 * case that produced it (QA-A-54, measured 2026-09-12). Both paths return a
 * zero; the signs may differ; and the quantity the Hampel rule actually consumes
 * is bit-identical regardless.
 */
TEST(Median8ParityTest, ZeroSignDifferenceCannotChangeADecision) {
    std::vector<float> a = {-0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f};
    std::vector<float> b = a;
    const float fast = ComputeMedian(a);
    const float slow = ComputeMedianGeneric(b);

    EXPECT_EQ(0.0f, fast);
    EXPECT_EQ(0.0f, slow);
    EXPECT_TRUE(SameBitsOrBothZero(fast, slow));

    // Whatever the sign, the rule sees the same number. Sample both sides of the
    // decision and a value far from it.
    for (float centre : {-7.5f, -1e-7f, 0.0f, 1e-7f, 7.5f, 1234.5f}) {
        EXPECT_TRUE(SameBits(std::abs(centre - fast), std::abs(centre - slow)))
            << "centre " << centre;
    }
}

/** The MAD is a second median over the same window; it must match too. */
TEST(Median8ParityTest, MadMatchesGenericBitForBit) {
    std::mt19937 rng(20260913u);
    std::uniform_real_distribution<float> wide(-1.0e3f, 1.0e3f);

    for (int trial = 0; trial < 50000; ++trial) {
        std::vector<float> v(8);
        for (float& f : v) f = wide(rng);
        std::vector<float> copy = v;
        const float median = ComputeMedian(v);

        std::vector<float> devFast = v, devSlow = copy;
        const float fast = ComputeMAD(devFast, median);

        // The generic MAD, re-derived here so the comparison does not depend on
        // which path ComputeMAD's internal ComputeMedian happens to take.
        for (size_t i = 0; i < devSlow.size(); ++i) {
            devSlow[i] = std::abs(devSlow[i] - median);
        }
        const float slow = ComputeMedianGeneric(devSlow) * RUNTIME_DETECTION_MAD_SCALE;

        ASSERT_TRUE(SameBits(fast, slow)) << "trial " << trial;
    }
}

/**
 * End to end, in the BufferReuseParityTest shape: the shipped detector against a
 * replica that is forced down the generic path. Every pixel of the map must
 * agree -- not a count, the map.
 */
TEST(Median8ParityTest, DetectionMapIsIdenticalPixelForPixel) {
    constexpr uint32_t kW = 256;
    constexpr uint32_t kH = 256;
    constexpr size_t kN = static_cast<size_t>(kW) * kH;

    std::mt19937 rng(20260914u);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    std::vector<float> frame(kN);
    for (size_t i = 0; i < kN; ++i) frame[i] = noise(rng);
    // Genuine outliers, so the two sides must agree on flags as well as on
    // clean pixels -- an all-zero map would agree trivially.
    for (size_t i = 137; i < kN; i += 811) frame[i] += 90.0f;

    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = kW;
    img.height = kH;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    const float sg = xpe::preprocess::internal::ComputeGlobalSigma(&img);
    cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sg;
    cfg.globalSigmaCap = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sg;

    std::vector<float> nbrs, dev;
    std::vector<uint8_t> shipped(kN, 0), generic(kN, 0);
    size_t flagged = 0;

    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            shipped[i] = DetectDefectivePixel(&img, x, y, cfg, nbrs, dev) ? 1u : 0u;
            if (shipped[i]) ++flagged;

            // The same rule, with every selection forced through the generic path.
            CollectNeighborValues(&img, x, y, cfg.windowSize, nbrs);
            if (nbrs.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) { generic[i] = 0u; continue; }
            const float median = ComputeMedianGeneric(nbrs);
            dev.assign(nbrs.begin(), nbrs.end());
            for (size_t k = 0; k < dev.size(); ++k) dev[k] = std::abs(dev[k] - median);
            const float mad = ComputeMedianGeneric(dev) * RUNTIME_DETECTION_MAD_SCALE;

            float sigmaEstimate = mad;
            if (cfg.globalSigmaFloor > sigmaEstimate) sigmaEstimate = cfg.globalSigmaFloor;
            if (cfg.globalSigmaCap > 0.0f && sigmaEstimate > cfg.globalSigmaCap) {
                sigmaEstimate = cfg.globalSigmaCap;
            }
            const float centre = frame[i];
            if (sigmaEstimate < 1e-6f) {
                generic[i] = (std::abs(centre - median) > 1e-6f) ? 1u : 0u;
            } else {
                generic[i] = (std::abs(centre - median) > cfg.sigmaThreshold * sigmaEstimate)
                             ? 1u : 0u;
            }
        }
    }

    ASSERT_GT(flagged, 0u) << "a map with no flags would agree trivially";
    size_t mismatches = 0;
    for (size_t i = 0; i < kN; ++i) {
        if (shipped[i] != generic[i]) ++mismatches;
    }
    EXPECT_EQ(0u, mismatches)
        << flagged << " pixels flagged, " << mismatches << " disagreed";
}
