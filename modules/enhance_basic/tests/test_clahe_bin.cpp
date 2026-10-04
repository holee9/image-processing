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
    size_t fast_rows = 0, exact_rows = 0;
    for (const BinVector& r : kVectors) {
        const float vmin = F(r.vmin), vmax = F(r.vmax), v = F(r.v);
        ASSERT_LT(vmin, vmax);
        const xpe_clahe::Binner b(vmin, vmax, MinLsb(vmin, vmax, v));
        const int exact = b.exact_bin(v);
        ASSERT_EQ(r.bin, exact) << "EXACT path, vmin=" << vmin << " vmax=" << vmax << " v=" << v;
        ASSERT_GE(r.bin, 0);
        ASSERT_LE(r.bin, 4095);
        if (b.fast()) {
            ++fast_rows;
            ASSERT_EQ(r.bin, b.fast_bin(v)) << "FAST path (allowed by fast_path_is_exact), vmin=" << vmin << " vmax=" << vmax << " v=" << v;
        } else {
            ++exact_rows;
        }
        ASSERT_EQ(r.bin, b.bin(v));
    }
    // controls: both paths are really exercised by the vectors
    EXPECT_GT(fast_rows, 2000u);
    EXPECT_GT(exact_rows, 2000u);
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
