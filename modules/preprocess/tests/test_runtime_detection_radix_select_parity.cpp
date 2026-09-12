/**
 * @file test_runtime_detection_radix_select_parity.cpp
 * @brief QA-A-55 (#144): the radix selection returns the same value
 *        std::nth_element returns, and ComputeGlobalSigma is unchanged.
 *
 * The optimisation this pins replaces two std::nth_element calls inside
 * ComputeGlobalSigma with SelectKthSmallest, an exact two-pass radix histogram.
 * Both compute the same order statistic; neither samples nor approximates.
 *
 * "Same" means BITWISE. A sigma that differs in the last ulp moves the global
 * floor, and the floor decides flags at the boundary -- so a tolerance-based
 * assertion would not be checking the claim. EXPECT_FLOAT_EQ allows 4 ulp and is
 * the wrong tool here; these tests compare raw bits.
 *
 * The one place the two orders genuinely differ is -0.0 vs +0.0: they compare
 * equal as floats, so std::nth_element may return either, while the key order
 * always puts -0.0 first. That is checked head-on rather than excluded --
 * see ZeroSignChoiceCannotChangeTheSigma, which shows both call sites are
 * insensitive to it.
 *
 * NaN is out of scope: it breaks the strict weak ordering std::nth_element
 * requires, so the prior behaviour is undefined rather than merely different.
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013.  Refs #144 #143
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

namespace {

using xpe::preprocess::internal::SelectKthSmallest;
using xpe::preprocess::internal::FloatSortKey;
using xpe::preprocess::internal::SortKeyToFloat;
using xpe::preprocess::internal::ComputeGlobalSigma;

bool SameBits(float a, float b) {
    uint32_t ba = 0, bb = 0;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

/** What the shipped code used to do, kept here as the reference. */
float NthElementReference(std::vector<float> v, size_t k) {
    if (v.empty()) return 0.0f;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

}  // namespace

/** The key must be a total order that agrees with float `<` on non-NaN pairs. */
TEST(RadixSelectParityTest, SortKeyIsOrderPreservingAndInvertible) {
    std::mt19937 rng(20260912u);
    std::uniform_real_distribution<float> wide(-1.0e6f, 1.0e6f);

    for (int trial = 0; trial < 200000; ++trial) {
        const float a = wide(rng);
        const float b = wide(rng);
        if (a < b) {
            ASSERT_LT(FloatSortKey(a), FloatSortKey(b)) << a << " vs " << b;
        } else if (b < a) {
            ASSERT_LT(FloatSortKey(b), FloatSortKey(a)) << a << " vs " << b;
        }
        ASSERT_TRUE(SameBits(a, SortKeyToFloat(FloatSortKey(a))));
        ASSERT_TRUE(SameBits(b, SortKeyToFloat(FloatSortKey(b))));
    }

    for (float f : {0.0f, -0.0f, 1.0f, -1.0f, 1e-38f, -1e-38f, 3.4e38f, -3.4e38f}) {
        EXPECT_TRUE(SameBits(f, SortKeyToFloat(FloatSortKey(f)))) << f;
    }
    // The deliberate difference, stated as a test so it cannot drift unnoticed.
    EXPECT_LT(FloatSortKey(-0.0f), FloatSortKey(0.0f));
    EXPECT_TRUE(-0.0f == 0.0f);
}

/** Selection agrees with std::nth_element, bit for bit, at every rank tried. */
TEST(RadixSelectParityTest, MatchesNthElementBitForBit) {
    std::mt19937 rng(20260913u);
    std::normal_distribution<float> gauss(0.0f, 14.0f);
    std::uniform_int_distribution<int> tie(-4, 4);

    for (int trial = 0; trial < 3000; ++trial) {
        const size_t n = 1u + static_cast<size_t>(rng() % 2000u);
        std::vector<float> v(n);
        if (trial % 3 == 0) {
            for (float& f : v) f = static_cast<float>(tie(rng));   // heavy ties
        } else {
            for (float& f : v) f = gauss(rng);
        }
        const size_t k = static_cast<size_t>(rng() % n);
        ASSERT_TRUE(SameBits(SelectKthSmallest(v, k), NthElementReference(v, k)))
            << "trial " << trial << " n " << n << " k " << k;
    }

    // Ranks that are easy to get wrong by one.
    std::vector<float> v(1000);
    for (size_t i = 0; i < v.size(); ++i) v[i] = gauss(rng);
    for (size_t k : {size_t(0), size_t(1), size_t(499), size_t(500), size_t(998), size_t(999)}) {
        EXPECT_TRUE(SameBits(SelectKthSmallest(v, k), NthElementReference(v, k))) << "k " << k;
    }

    // Degenerate shapes.
    EXPECT_TRUE(SameBits(0.0f, SelectKthSmallest(std::vector<float>{}, 0)));
    EXPECT_TRUE(SameBits(7.0f, SelectKthSmallest(std::vector<float>{7.0f}, 0)));
    EXPECT_TRUE(SameBits(7.0f, SelectKthSmallest(std::vector<float>{7.0f}, 99)));  // k clamped
    const std::vector<float> allSame(500, -2.5f);
    EXPECT_TRUE(SameBits(-2.5f, SelectKthSmallest(allSame, 250)));
}

/**
 * The one genuine ordering difference, met head-on: the key puts -0.0 before
 * +0.0 where nth_element treats them as equal. Neither call site can see it.
 */
TEST(RadixSelectParityTest, ZeroSignChoiceCannotChangeTheSigma) {
    std::vector<float> zeros = {-0.0f, 0.0f, -0.0f, 0.0f, -0.0f};
    const float picked = SelectKthSmallest(zeros, 2);
    EXPECT_EQ(0.0f, picked);

    // Call site 1: the median only ever appears as `x - median`.
    for (float x : {-9.0f, -1e-7f, -0.0f, 0.0f, 1e-7f, 9.0f, 1234.5f}) {
        EXPECT_TRUE(SameBits(std::abs(x - (-0.0f)), std::abs(x - 0.0f))) << x;
    }

    // Call site 2 runs on absolute deviations, which cannot contain -0.0:
    // std::abs maps every zero to +0.0.
    for (float x : {-0.0f, 0.0f, -1.0f, 1.0f}) {
        const float d = std::abs(x - x);
        EXPECT_TRUE(SameBits(0.0f, d)) << "abs deviation carried a -0.0 for " << x;
    }
}

/**
 * End to end: the shipped ComputeGlobalSigma against a replica of the previous
 * implementation, on frames of the shapes QA-A-47..A-50 used.
 */
TEST(RadixSelectParityTest, GlobalSigmaMatchesThePreviousImplementation) {
    constexpr uint32_t kW = 512;
    constexpr uint32_t kH = 512;
    constexpr size_t kN = static_cast<size_t>(kW) * kH;

    auto previousImplementation = [](const std::vector<float>& px,
                                     uint32_t w, uint32_t h) -> float {
        auto madSigma = [](std::vector<float>& d) -> float {
            if (d.empty()) return 0.0f;
            const size_t mid = d.size() / 2u;
            std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
            const float median = d[mid];
            for (size_t i = 0; i < d.size(); ++i) d[i] = std::abs(d[i] - median);
            std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
            return d[mid] * RUNTIME_DETECTION_MAD_SCALE * 0.70710678f;
        };
        std::vector<float> diff;
        diff.reserve(static_cast<size_t>(w) * h);
        for (size_t y = 0; y < h; ++y) {
            const float* row = px.data() + y * w;
            for (size_t x = 0; x + 1u < w; ++x) diff.push_back(row[x + 1u] - row[x]);
        }
        const float sh = madSigma(diff);
        diff.clear();
        for (size_t y = 0; y + 1u < h; ++y) {
            const float* row = px.data() + y * w;
            for (size_t x = 0; x < w; ++x) diff.push_back(row[x + w] - row[x]);
        }
        const float sv = madSigma(diff);
        if (sh <= 0.0f) return sv;
        if (sv <= 0.0f) return sh;
        return (sh < sv) ? sh : sv;
    };

    std::mt19937 rng(20260914u);
    std::normal_distribution<float> noise(3000.0f, 10.0f);

    struct Shape { const char* name; int kind; };
    const Shape shapes[] = {
        {"uniform", 0}, {"ramp+edge", 1}, {"stripes", 2}, {"flat", 3}, {"quantised", 4},
    };

    for (const Shape& sh : shapes) {
        std::vector<float> frame(kN);
        for (uint32_t y = 0; y < kH; ++y) {
            for (uint32_t x = 0; x < kW; ++x) {
                const size_t i = static_cast<size_t>(y) * kW + x;
                float v = noise(rng);
                if (sh.kind == 1) v += (x >= kW / 2) ? 1500.0f : 0.0f;
                if (sh.kind == 2) v += ((y % 8u) == 0u) ? 240.0f : 0.0f;
                if (sh.kind == 3) v = 3000.0f;                       // MAD == 0
                if (sh.kind == 4) v = std::floor(v);                 // integer grid
                frame[i] = v;
            }
        }

        XpeImageBuffer img{};
        img.data = frame.data();
        img.width = kW;
        img.height = kH;
        img.bitsAllocated = 32;
        img.bitsStored = 32;
        img.format = XPE_PIXEL_FLOAT32;
        img.dataSize = static_cast<uint32_t>(kN * sizeof(float));

        const float shipped = ComputeGlobalSigma(&img);
        const float previous = previousImplementation(frame, kW, kH);
        EXPECT_TRUE(SameBits(shipped, previous))
            << sh.name << ": shipped " << shipped << " vs previous " << previous;
    }
}
