// QA-B-207d (Codex #120, #141, #143): the bin of a pixel in CLAHE must be EXACTLY floor((v - vmin) * 4095 / (vmax - vmin)).
//
// The third hold on this requirement was found by neither of the earlier test sets: both built images with vmin == 0, where a
// double subtraction never rounds. The tests here close that gap with an oracle that is not a formula: clahe_bin_vectors.inc
// holds {vmin, vmax, v, bin} rows computed with fractions.Fraction (gen_clahe_bin_vectors.py), covering every regime of the
// derivation in clahe_bin_internal.h, with the pixels ON and next to the exact bin boundaries.
#include <gtest/gtest.h>

#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/enhance_basic/clahe_bin_internal.h"
#include "xpe/enhance_basic/enhance_basic_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace {

struct BinVector {
    uint32_t vmin, vmax, v;
    int bin;
};

const BinVector kVectors[] = {
#include "clahe_bin_vectors.inc"
};

float F(uint32_t bits) {
    float x;
    std::memcpy(&x, &bits, sizeof x);
    return x;
}

// The lowest last-bit exponent over the nonzero pixels of the three-pixel image {vmin, vmax, v}.
int MinLsb(float a, float b, float c) {
    int m = 1000;
    for (float x : {a, b, c}) {
        if (x != 0.0f) m = std::min(m, xpe_clahe::lsb_exp(x));
    }
    return m;
}

}  // namespace

// Every row: the exact path equals the rational bin always; the fast path equals it wherever it is allowed; the result is in
// [0, 4095] (no clamp or mask in the implementation -- this is where the invariant is checked).
TEST(ClaheBin, EveryVectorMatchesTheExactRationalBin) {
    size_t fast_rows = 0, exact_rows = 0, filter_to_exact = 0, int64_rows = 0;
    for (const BinVector& r : kVectors) {
        const float vmin = F(r.vmin), vmax = F(r.vmax), v = F(r.v);
        ASSERT_LT(vmin, vmax);
        const xpe_clahe::Binner b(vmin, vmax, MinLsb(vmin, vmax, v));
        const int exact = b.exact_bin(v);
        ASSERT_EQ(r.bin, exact) << "EXACT path, vmin=" << vmin << " vmax=" << vmax << " v=" << v;
        ASSERT_GE(r.bin, 0);
        ASSERT_LE(r.bin, 4095);
        if (b.int64_path()) {
            ++int64_rows;
            ASSERT_EQ(r.bin, b.int64_bin(v)) << "INT64 path (41 < S <= 52), vmin=" << vmin << " vmax=" << vmax << " v=" << v;
        }
        if (b.fast()) {
            ++fast_rows;
            ASSERT_EQ(r.bin, b.fast_bin(v)) << "FAST path (allowed by fast_path_is_exact), vmin=" << vmin << " vmax=" << vmax << " v=" << v;
        } else {
            ++exact_rows;
        }
        ASSERT_EQ(r.bin, b.bin(v));
        // QA-B-207e: the tier-2 filter is valid for ANY image, so it must reproduce the rational bin on every row, including the
        // rows the image-level proof rejects. Rows it sends to the 320-bit path are counted (a control: the filter is not idle).
        bool exact_used = false;
        ASSERT_EQ(r.bin, xpe_clahe::filtered_bin(b, v, &exact_used))
            << "FILTER path, vmin=" << vmin << " vmax=" << vmax << " v=" << v;
        if (exact_used) ++filter_to_exact;
    }
    // controls: both paths are really exercised by the vectors
    EXPECT_GT(fast_rows, 2000u);
    EXPECT_GT(exact_rows, 1000u);
    EXPECT_GT(int64_rows, 500u) << "the vectors cover the 64-bit integer tier (span 42..52)";
    EXPECT_GT(filter_to_exact, 20u) << "the vectors sit on bin boundaries: the filter must hand some of them to the 320-bit path";
    EXPECT_LT(filter_to_exact, sizeof(kVectors) / sizeof(kVectors[0]) / 2u) << "...but most rows are certified by the filter itself";
}

// The decision itself, just inside and just outside the proven span (c). vmin = 1 has its last bit at 2^-23, so
// S = ilogb(vmax) + 1 + 23 and the fast path ends at ilogb(vmax) = 17 (vmax < 2^18).
TEST(ClaheBin, TheFastPathEndsExactlyWhereTheDerivationEndsIt) {
    using xpe_clahe::fast_path_is_exact;
    // (A) a zero minimum is always fast
    EXPECT_TRUE(fast_path_is_exact(0.0f, 3.0e38f, 0));
    EXPECT_TRUE(fast_path_is_exact(-0.0f, 1.0e-40f, 0));
    // (B) positive minimum
    EXPECT_TRUE(fast_path_is_exact(1.0f, 262143.0f, 0)) << "S = 17 + 1 + 23 = 41";
    EXPECT_FALSE(fast_path_is_exact(1.0f, 262144.0f, 0)) << "S = 18 + 1 + 23 = 42";
    EXPECT_FALSE(fast_path_is_exact(0.0123f, 4095.0f, 0)) << "a small positive minimum next to a large maximum";
    EXPECT_FALSE(fast_path_is_exact(std::ldexp(1.0f, -100), 4095.0f, 0));
    // (C) negative minimum: the lowest pixel last-bit exponent decides. Pixels of 1.0 -> -23: H + 2 + 23 <= 41 <=> H <= 16.
    EXPECT_TRUE(fast_path_is_exact(-1.0f, 65535.0f, -23)) << "H = 15";
    EXPECT_TRUE(fast_path_is_exact(-1.0f, 131071.0f, -23)) << "H = 16, S = 41";
    EXPECT_FALSE(fast_path_is_exact(-1.0f, 131072.0f, -23)) << "H = 17, S = 42";
    EXPECT_FALSE(fast_path_is_exact(-1.0f, 4095.0f, -149)) << "a subnormal pixel in the image";
    // tier 1b: the 64-bit integer path takes over for 41 < S <= 52 (vmin = 1: S = ilogb(vmax) + 24)
    using xpe_clahe::Binner;
    EXPECT_FALSE(Binner(1.0f, 262143.0f, 0).int64_path()) << "S = 41 is tier 1 (double), not 1b";
    EXPECT_TRUE(Binner(1.0f, 262144.0f, 0).int64_path()) << "S = 42";
    EXPECT_TRUE(Binner(1.0f, std::ldexp(1.0f, 28), 0).int64_path()) << "S = 28 + 24 = 52";
    EXPECT_FALSE(Binner(1.0f, std::ldexp(1.0f, 29), 0).int64_path()) << "S = 53: beyond 4095 * 2^52 < 2^64";
    EXPECT_FALSE(Binner(1.0f, std::ldexp(1.0f, 29), 0).fast());
    EXPECT_TRUE(fast_path_is_exact(-1.0f, 4095.0f, -24)) << "H = 11, S = 11 + 2 + 24 = 37: control of the previous line";
}

// min_lsb_exp: zero is ignored, a subnormal counts as 2^-149, the lowest nonzero pixel wins.
TEST(ClaheBin, MinLsbExpReadsTheLowestNonzeroPixel) {
    const float px[] = {0.0f, 1.0f, 4095.0f, -0.0f};
    EXPECT_EQ(-23, xpe_clahe::min_lsb_exp(px, 4));
    const float sub[] = {1.0f, std::ldexp(1.0f, -140), 4095.0f};
    EXPECT_EQ(-149, xpe_clahe::min_lsb_exp(sub, 3));   // a subnormal-range value: its exponent field is 0 -> -149
    const float tiny[] = {std::ldexp(1.0f, -100), 1.0f};
    EXPECT_EQ(-123, xpe_clahe::min_lsb_exp(tiny, 2));
    const float zeros[] = {0.0f, 0.0f};
    EXPECT_GT(xpe_clahe::min_lsb_exp(zeros, 2), 0) << "no nonzero pixel: no constraint";
}

// Codex #143 through the real library (built with /fp:fast): the 8x8 image made of the 4x4 block
// [2^-100, 4095, 1,1,1,1, 1,1,1,1, 2,3,4,5,6,7] four times, 2x2 tiles, clip 3. Every pixel against the exact-rational CLAHE
// computed by the Python reference (clahe_ref_exact.py + print_codex_d_expected.py in the QA-B-207 evidence): the value 1 is 511.875,
// the 207c library gave 767.8125 for 56 of the 64 pixels.
TEST(ClaheBin, CodexEightByEight_EveryPixelMatchesTheExactReference) {
    const float block[16] = {std::ldexp(1.0f, -100), 4095.0f, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 6, 7};
    static const double kExpected[64] = {
        511.875, 4095.0, 511.875, 511.875, 511.875, 4095.0, 511.875, 511.875,
        511.875, 511.875, 511.875, 511.875, 511.875, 511.875, 511.875, 511.875,
        511.875, 511.875, 767.8125, 1023.75, 511.875, 511.875, 767.8125, 1023.75,
        1279.6875, 1535.625, 1791.5625, 2047.5, 1279.6875, 1535.625, 1791.5625, 2047.5,
        511.875, 4095.0, 511.875, 511.875, 511.875, 4095.0, 511.875, 511.875,
        511.875, 511.875, 511.875, 511.875, 511.875, 511.875, 511.875, 511.875,
        511.875, 511.875, 767.8125, 1023.75, 511.875, 511.875, 767.8125, 1023.75,
        1279.6875, 1535.625, 1791.5625, 2047.5, 1279.6875, 1535.625, 1791.5625, 2047.5};
    std::vector<float> px(64);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) px[static_cast<size_t>(y) * 8 + x] = block[(y % 4) * 4 + (x % 4)];
    XpeImageBuffer img{};
    img.width = 8;
    img.height = 8;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = px.size() * sizeof(float);
    img.data = px.data();
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 2;
    params.tile_height = 2;
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    for (size_t i = 0; i < 64; ++i) {
        EXPECT_NEAR(kExpected[i], px[i], 1e-5 * 4095.0) << "pixel " << i;
    }
}

// QA-B-207e: the tier-2 filter. A pixel is certified only if frac(t^) is farther than kFilterMargin from an integer.
TEST(ClaheBin, TheFilterCertifiesExactlyOutsideItsMargin) {
    using xpe_clahe::filter_certifies;
    using xpe_clahe::kFilterMargin;
    int k = -1;
    // the margin dominates the derived error bound by a wide factor (also a static_assert in the header)
    EXPECT_GE(kFilterMargin, 100.0 * xpe_clahe::kFilterAbsErr);
    EXPECT_FALSE(filter_certifies(1365.0, &k)) << "exactly on a boundary";
    EXPECT_FALSE(filter_certifies(1365.0 + 0.5 * kFilterMargin, &k)) << "inside the margin above";
    EXPECT_FALSE(filter_certifies(1366.0 - 0.5 * kFilterMargin, &k)) << "inside the margin below";
    EXPECT_TRUE(filter_certifies(1365.0 + 2.0 * kFilterMargin, &k));
    EXPECT_EQ(1365, k);
    EXPECT_TRUE(filter_certifies(1366.0 - 2.0 * kFilterMargin, &k));
    EXPECT_EQ(1365, k);
    EXPECT_TRUE(filter_certifies(2047.5, &k));
    EXPECT_EQ(2047, k);
    EXPECT_FALSE(filter_certifies(4095.0, &k)) << "the maximum is exactly on the top boundary";
    EXPECT_FALSE(filter_certifies(0.0, &k)) << "the minimum is exactly on the bottom boundary";
}

// The same classification through filtered_bin: a pixel at t = 0.5e-9 (inside the margin above bin 0's lower boundary) takes the
// 320-bit path and is bin 0; at t = 2e-9 the filter certifies it. Pixels exactly ON a boundary (integer data on a range of 4095)
// take the 320-bit path too. vmin == 0 keeps the arithmetic simple: t = v * (4095 / 4095) = v.
TEST(ClaheBin, PixelsNearOrOnABoundaryGoToTheExactPath) {
    const xpe_clahe::Binner b(0.0f, 4095.0f, 0);
    bool exact = false;
    EXPECT_EQ(0, xpe_clahe::filtered_bin(b, 0.5e-9f, &exact));
    EXPECT_TRUE(exact) << "t = 0.5e-9 is inside the margin";
    exact = false;
    EXPECT_EQ(0, xpe_clahe::filtered_bin(b, 2.0e-9f, &exact));
    EXPECT_FALSE(exact) << "t = 2e-9 is certified by the filter";
    exact = false;
    EXPECT_EQ(1000, xpe_clahe::filtered_bin(b, 1000.0f, &exact));
    EXPECT_TRUE(exact) << "an integer on a boundary";
    exact = false;
    EXPECT_EQ(1000, xpe_clahe::filtered_bin(b, 1000.5f, &exact));
    EXPECT_FALSE(exact);
    exact = false;
    EXPECT_EQ(0, xpe_clahe::filtered_bin(b, 0.0f, &exact));
    EXPECT_FALSE(exact) << "v == vmin is bin 0 without the 320-bit path";
    EXPECT_EQ(4095, xpe_clahe::filtered_bin(b, 4095.0f, &exact));
    EXPECT_FALSE(exact) << "v == vmax is bin 4095 without the 320-bit path";
}

// Measure-only, with a regression bound: the share of pixels the filter hands to the 320-bit path on 3072x3072 images of the kinds
// in the QA-B-207 report. A continuous distribution should send about 2e-9 of its pixels; the bound below is far looser.
TEST(ClaheBin, MeasureTheShareOfPixelsOnTheExactPath) {
    const int n = 3072 * 3072;
    struct Case { const char* what; float lo; float hi; bool plateau; bool one_tiny; bool integers; };
    const Case cases[] = {
        {"uniform decimals 0.0123..4000", 0.0123f, 4000.0f, false, false, false},
        {"decimals 10..4000 with ONE pixel of 0.0123", 10.0f, 4000.0f, false, true, false},
        {"decimals 0.0123..4000, 30 % of the pixels AT the minimum", 0.0123f, 4000.0f, true, false, false},
        {"decimals -3.7..4000 (negative minimum)", -3.7f, 4000.0f, false, false, false},
        // every pixel is an integer on a range of 4095 (t is an integer for EVERY pixel), plus one pixel of 0.001: S = 46
        {"integer HU -1024..3071 with ONE pixel of 0.001", -1024.0f, 3071.0f, false, true, true},
    };
    for (const Case& c : cases) {
        std::vector<float> px(static_cast<size_t>(n));
        uint32_t s = 12345u;
        for (int i = 0; i < n; ++i) {
            s = s * 1664525u + 1013904223u;
            const double u = (s >> 8) * (1.0 / 16777216.0);
            px[static_cast<size_t>(i)] = static_cast<float>(c.lo + u * (c.hi - c.lo));
            if (c.plateau && (s & 0xFF) < 77) px[static_cast<size_t>(i)] = c.lo;
            if (c.integers) px[static_cast<size_t>(i)] = std::floor(px[static_cast<size_t>(i)]);
        }
        if (c.integers) { px[2] = c.lo; px[3] = c.hi; }   // the range is exactly 4095, so t is an integer for every pixel
        if (c.one_tiny) px[100] = c.integers ? 0.001f : 0.0123f;
        px[0] = *std::min_element(px.begin(), px.end());
        px[1] = *std::max_element(px.begin(), px.end());
        const float vmin = *std::min_element(px.begin(), px.end());
        const float vmax = *std::max_element(px.begin(), px.end());
        const xpe_clahe::Binner b(vmin, vmax, vmin < 0.0f ? xpe_clahe::min_lsb_exp(px.data(), px.size()) : 0);
        std::vector<uint16_t> out(static_cast<size_t>(n));
        const size_t exact = xpe_clahe::fill_bins_filtered(b, px.data(), out.data(), n);
        GTEST_LOG_(INFO) << c.what << ": image-level fast path " << (b.fast() ? "ALLOWED" : "not allowed") << "; filter sent " << exact
                         << " of " << n << " pixels to an integer path (64-bit where proven, else 320-bit) (" << (100.0 * static_cast<double>(exact) / n) << " %)";
        if (c.integers) {
            // every pixel is on a boundary: the filter hands (nearly) all of them over, and the 64-bit integer tier takes them
            EXPECT_TRUE(b.int64_path()) << c.what << ": S = 46 is inside the 64-bit tier";
            EXPECT_GT(exact, static_cast<size_t>(n) / 2u) << c.what;
        } else {
            EXPECT_LT(exact, static_cast<size_t>(n) / 100000u) << c.what;
        }
    }
}
