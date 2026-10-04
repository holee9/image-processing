/**
 * @file test_contrast_enhance.cpp
 * @brief TDD RED tests for SWU-2.3: Contrast Enhancement / CLAHE (REQ-ENH-013..017)
 * SPEC: SPEC-XPE-P1B-ENH v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>
#include "perf_measure.h"

namespace {

// Helper: create float32 image filled with a value
static XpeImageBuffer make_f32(uint32_t w, uint32_t h, float fill = 0.0f) {
    XpeImageBuffer img{};
    img.width = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = (size_t)w * h * sizeof(float);
    img.data = malloc(img.dataSize);
    if (img.data) {
        float* px = static_cast<float*>(img.data);
        std::fill(px, px + (size_t)w * h, fill);
    }
    return img;
}

static void free_img(XpeImageBuffer& img) {
    free(img.data);
    img.data = nullptr;
}

// Helper: compute standard deviation of a region
static float compute_stddev(const float* data, int n) {
    if (n <= 1) return 0.0f;
    double sum = 0, sum_sq = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
        sum_sq += (double)data[i] * data[i];
    }
    double mean = sum / n;
    double var = (sum_sq / n) - (mean * mean);
    return (float)std::sqrt(std::max(0.0, var));
}

// REQ-ENH-CC-002: Null image returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, NullImage_ReturnsInvalidInput) {
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(nullptr, &params));
}

// REQ-ENH-013: Valid params returns XPE_OK
TEST(ContrastEnhance, ValidParams_ReturnsOk) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-014: Null params uses defaults and returns XPE_OK (AC Scenario 7)
TEST(ContrastEnhance, NullParams_UsesDefaults_ReturnsOk) {
    auto img = make_f32(64, 64, 500.0f);
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, nullptr));
    free_img(img);
}

// REQ-ENH-015: clip_limit < 1.0 returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, ClipLimitBelow1_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 0.5f;
    params.tile_width = 8;
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-016: tile_width < 2 returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, TileWidthBelow2_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 1;
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-016: tile_height < 2 returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, TileHeightBelow2_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-016: Image smaller than 2*tile grid returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, ImageSmallerThanTileGrid_ReturnsInvalidInput) {
    auto img = make_f32(16, 16, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 16;  // image 16 wide, need >= 2*16 = 32
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// ---------------------------------------------------------------------------
// QA-B-207 M2 (E6): the standard CLAHE -- one intensity scale for the whole image, a clipped histogram table per tile, and a
// bilinear blend of the tables of the four tiles around each pixel. The value tests below were rewritten for it; see
// .moai/reports/lane-post/QA-B-207/report_m2_e6.md for what each one replaced and why.
// ---------------------------------------------------------------------------

// Deterministic noise in [-0.5, 0.5): a linear congruential generator, so a failure reproduces on every machine.
static float lcg_noise(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>((state >> 8) & 0xFFFFu) / 65535.0f - 0.5f;
}

// The picture AC-04 talks about: a LOW-CONTRAST REGION inside an image that also has a wide value range. A wide gradient across
// the whole image; noise of about +-60 on the left half; and on the right half a 192x192 region (it crosses the borders of the
// 4x4 tile grid) flattened to one level with noise of only about +-6.
struct LowContrastScene {
    static constexpr int kSize = 512;
    static constexpr int kRowBegin = 160, kRowEnd = 352, kColBegin = 288, kColEnd = 480;
    std::vector<float> px;

    LowContrastScene() : px(static_cast<size_t>(kSize) * kSize) {
        uint32_t s1 = 1u, s2 = 2u;
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                float v = 200.0f + 3000.0f * static_cast<float>(x) / static_cast<float>(kSize - 1);
                if (x < kSize / 2) v += 120.0f * lcg_noise(s1);
                px[static_cast<size_t>(y) * kSize + x] = v;
            }
        }
        for (int y = kRowBegin; y < kRowEnd; ++y)
            for (int x = kColBegin; x < kColEnd; ++x)
                px[static_cast<size_t>(y) * kSize + x] = 2200.0f + 12.0f * lcg_noise(s2);
    }

    // Standard deviation of the low-contrast region in @p data (an image of the same layout).
    float region_stddev(const float* data) const {
        std::vector<float> r;
        for (int y = kRowBegin; y < kRowEnd; ++y)
            for (int x = kColBegin; x < kColEnd; ++x) r.push_back(data[static_cast<size_t>(y) * kSize + x]);
        return compute_stddev(r.data(), static_cast<int>(r.size()));
    }

    // Enhances a copy of the scene and returns the region's contrast gain (stddev after / stddev before).
    float gain(float clip_limit, int tiles) const {
        auto img = make_f32(kSize, kSize, 0.0f);
        std::copy(px.begin(), px.end(), static_cast<float*>(img.data));
        XpeClaheParams params{};
        params.clip_limit = clip_limit;
        params.tile_width = tiles;
        params.tile_height = tiles;
        const XpeErrorCode rc = xpe_contrast_enhance(&img, &params);
        const float after = region_stddev(static_cast<const float*>(img.data));
        free_img(img);
        return rc == XPE_OK ? after / region_stddev(px.data()) : -1.0f;
    }
};

// AC-04, as the criterion words it: "Local contrast ratio increases by >= 20% in low-contrast regions".
// What this replaced: a whole-image ramp 495..505 whose centre stddev had to grow by 20%. That passed only because each tile
// was stretched onto the full value range on its own. A ramp is already evenly spread over the image's range, so an equalization
// that keeps the image's range has nothing to add to it (AlreadyEvenlySpreadImage_IsLeftNearlyAsItIs below states that); the
// regions the criterion is about are low-contrast parts of an image whose range is set by other parts.
TEST(ContrastEnhance, LowContrastRegion_ContrastImproves) {
    LowContrastScene scene;
    const float gain = scene.gain(3.0f, 4);
    EXPECT_GE(gain, 1.2f) << "the region's stddev changed by x" << gain;
}

// The clip limit is what bounds the contrast a tile may add: a higher limit lets the region's contrast grow more, and with no
// clipping at all it grows far beyond the default. (Measured at 512x512 / 4x4: x9.6, x18.3, x27.2, x68.7, x188.)
TEST(ContrastEnhance, ClipLimit_BoundsTheContrastGain) {
    LowContrastScene scene;
    const float clips[] = {1.0f, 2.0f, 3.0f, 10.0f, 1.0e6f};
    float gains[5];
    for (int i = 0; i < 5; ++i) gains[i] = scene.gain(clips[i], 4);
    for (int i = 1; i < 5; ++i)
        EXPECT_GT(gains[i], gains[i - 1]) << "clip " << clips[i] << " gave x" << gains[i] << ", clip " << clips[i - 1] << " x" << gains[i - 1];
    EXPECT_GT(gains[4], 3.0f * gains[2]) << "no clipping must give far more than the default limit";
}

// A smooth ramp must stay smooth. The step between two neighbouring pixels across a tile border has to be about the step
// anywhere else. (The tile-by-tile code this replaced stepped by x1022 here: 1018 across the border against 1 elsewhere.)
TEST(ContrastEnhance, SmoothRamp_HasNoStepAtTileBoundaries) {
    const int W = 256, H = 256, T = 8, tile = W / T;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) px[y * W + x] = 100.0f + 4.0f * static_cast<float>(x);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = T;
    params.tile_height = T;
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));

    const float* row = px + 100 * W;
    std::vector<float> inside, boundary;
    for (int k = 0; k + 1 < W; ++k) {
        const float step = std::fabs(row[k + 1] - row[k]);
        ((k + 1) % tile == 0 ? boundary : inside).push_back(step);
    }
    std::vector<float> sorted = inside;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const float median = sorted[sorted.size() / 2];
    ASSERT_GT(median, 0.0f);
    float boundarySum = 0.0f, largest = 0.0f;
    for (float b : boundary) { boundarySum += b; largest = std::max(largest, b); }
    for (float v : inside) largest = std::max(largest, v);
    EXPECT_LE(boundarySum / static_cast<float>(boundary.size()), 1.5f * median) << "median step " << median;
    EXPECT_LE(largest, 2.0f * median) << "the largest step on the row, against a median of " << median;
    free_img(img);
}

// The other half of the AC-04 replacement: an image that is already evenly spread over its range (the ramp 495..505 the old test
// used) comes out about as it went in. Equalization redistributes values within the image's own range; it cannot widen it.
TEST(ContrastEnhance, AlreadyEvenlySpreadImage_IsLeftNearlyAsItIs) {
    const uint32_t W = 256, H = 256;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);
    for (uint32_t y = 0; y < H; y++)
        for (uint32_t x = 0; x < W; x++) px[y * W + x] = 495.0f + 10.0f * ((float)(x + y) / (float)(W + H));
    std::vector<float> before(128 * 128), after(128 * 128);
    for (int y = 64; y < 192; y++)
        for (int x = 64; x < 192; x++) before[(y - 64) * 128 + (x - 64)] = px[y * W + x];
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    for (int y = 64; y < 192; y++)
        for (int x = 64; x < 192; x++) after[(y - 64) * 128 + (x - 64)] = px[y * W + x];
    const float ratio = compute_stddev(after.data(), 128 * 128) / compute_stddev(before.data(), 128 * 128);
    EXPECT_GT(ratio, 0.9f);
    EXPECT_LT(ratio, 1.1f) << "measured x1.001";
    free_img(img);
}

// An implementation of the definition (Zuiderveld, Graphics Gems IV), written separately from contrast_enhance.cpp and in double
// precision: global scale, per-tile clipped histogram with the excess spread evenly (the part that does not divide evenly at a
// fixed stride over the whole scale), cumulative distribution, bilinear blend of the four tiles whose CENTRES surround the pixel
// (a border pixel sees two tiles, a corner one), mapped back onto [min, max]. Tiles with no pixel (ceil-division can leave one
// past the edge) do not take part.
//
// QA-B-207b (Codex #120): the bin was `(v - vmin) * (4095 / range)` here too, i.e. the formula under test copied, so this
// function could not see a failure of that formula (the float scale overflows below a range of 1.2e-35). It is now
// `floor((v - vmin) * 4095 / range)`: multiply first, divide last. That is NOT the same arithmetic as the implementation
// (which multiplies by a precomputed scale), and for float inputs it is exact: v - vmin and range are exact in double, the
// product with 4095 needs at most 36 bits, and a quotient of two such numbers is either an integer or at least 2^-24 from
// one, so the correctly rounded division cannot cross an integer. Independence beyond that rests on
// .moai/reports/lane-post/QA-B-207/clahe_ref_exact.py.txt, which takes the bin from exact rational arithmetic (Fraction) and
// agrees with the DLL on the same narrow-range cases (b_e6_green.txt).
static std::vector<double> ReferenceClahe(const std::vector<float>& in, int w, int h, float clip, int tw, int th) {
    const int kBins = 4096;
    const float vmin = *std::min_element(in.begin(), in.end());
    const float vmax = *std::max_element(in.begin(), in.end());
    const double range = static_cast<double>(vmax) - static_cast<double>(vmin);
    std::vector<double> out(in.begin(), in.end());
    if (!(range > 0.0)) return out;
    std::vector<int> bin(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        const double b = std::floor((static_cast<double>(in[i]) - static_cast<double>(vmin)) * (kBins - 1) / range);
        bin[i] = static_cast<int>(std::min(std::max(b, 0.0), static_cast<double>(kBins - 1)));
    }
    struct Span { int begin, end; double centre; };
    auto spans = [](int size, int tile, int count) {
        std::vector<Span> v;
        for (int t = 0; t < count; ++t) {
            const int b = std::min(t * tile, size), e = std::min(b + tile, size);
            if (b < e) v.push_back({b, e, 0.5 * (b + e - 1)});
        }
        return v;
    };
    const std::vector<Span> xs = spans(w, (w + tw - 1) / tw, tw), ys = spans(h, (h + th - 1) / th, th);
    std::vector<std::vector<double>> lut(xs.size() * ys.size(), std::vector<double>(kBins));
    for (size_t j = 0; j < ys.size(); ++j) {
        for (size_t i = 0; i < xs.size(); ++i) {
            std::vector<long long> hist(kBins, 0);
            for (int y = ys[j].begin; y < ys[j].end; ++y)
                for (int x = xs[i].begin; x < xs[i].end; ++x) ++hist[bin[static_cast<size_t>(y) * w + x]];
            const long long area = static_cast<long long>(xs[i].end - xs[i].begin) * (ys[j].end - ys[j].begin);
            const double clipF = static_cast<double>(clip) * static_cast<double>(area) / kBins;
            long long clipCount = clipF >= static_cast<double>(area) ? area : static_cast<long long>(clipF);
            if (clipCount < 1) clipCount = 1;
            long long excess = 0;
            for (auto& c : hist) {
                if (c > clipCount) { excess += c - clipCount; c = clipCount; }
            }
            const long long per = excess / kBins;
            long long rem = excess % kBins;
            for (auto& c : hist) c += per;
            if (rem > 0) {
                const int stride = static_cast<int>(std::max<long long>(kBins / rem, 1));
                for (int b = 0; b < kBins && rem > 0; b += stride, --rem) ++hist[b];
            }
            long long cum = 0;
            for (int b = 0; b < kBins; ++b) {
                cum += hist[b];
                lut[j * xs.size() + i][b] = static_cast<double>(cum) / static_cast<double>(area);
            }
        }
    }
    auto locate = [](const std::vector<Span>& s, int p, size_t& lo, size_t& hi, double& wt) {
        const double fp = p;
        if (fp <= s.front().centre) { lo = hi = 0; wt = 0.0; return; }
        if (fp >= s.back().centre) { lo = hi = s.size() - 1; wt = 0.0; return; }
        size_t k = 0;
        while (s[k + 1].centre <= fp) ++k;
        lo = k;
        hi = k + 1;
        wt = (fp - s[k].centre) / (s[k + 1].centre - s[k].centre);
    };
    for (int y = 0; y < h; ++y) {
        size_t j0, j1;
        double wy;
        locate(ys, y, j0, j1, wy);
        for (int x = 0; x < w; ++x) {
            size_t i0, i1;
            double wx;
            locate(xs, x, i0, i1, wx);
            const int b = bin[static_cast<size_t>(y) * w + x];
            const double top = lut[j0 * xs.size() + i0][b] * (1 - wx) + lut[j0 * xs.size() + i1][b] * wx;
            const double bot = lut[j1 * xs.size() + i0][b] * (1 - wx) + lut[j1 * xs.size() + i1][b] * wx;
            out[static_cast<size_t>(y) * w + x] = static_cast<double>(vmin) + (top * (1 - wy) + bot * wy) * static_cast<double>(range);
        }
    }
    return out;
}

// Every output pixel against the independent reference, on shapes that exercise the edges: sizes not divisible by the tile
// counts, a tile past the edge (11x11 in 5x5), a window of two tiles, tiny and large clip limits. The pixels are integers and
// the value range is 4095 (scale 1) or 8190 (scale 0.5), so the bin of a pixel is exact in any floating-point mode and the two
// implementations cannot disagree on a bin edge. The minimum is not always 0 (positive and negative offsets): the output is
// mapped back as min + fraction * range, and with a minimum of 0 the "+ min" would never be seen.
TEST(ContrastEnhance, MatchesAnIndependentReference) {
    struct Case { int w, h, tw, th; float clip; int mult; int offset; };   // offset: the image minimum (not always 0)
    const Case cases[] = {
        {101, 67, 8, 8, 3.0f, 1, 1000},   {64, 64, 4, 4, 3.0f, 1, -2000},   {17, 9, 2, 2, 3.0f, 2, 500},      {5, 8, 2, 4, 3.0f, 1, 0},
        {11, 11, 5, 5, 3.0f, 1, 100},     {256, 256, 8, 8, 1.0f, 1, 3000},   {256, 256, 8, 8, 40.0f, 1, 0},   {200, 150, 3, 7, 2.0f, 2, -500},
        {512, 512, 4, 4, 3.0f, 1, 0},
    };
    for (const Case& c : cases) {
        std::vector<float> in(static_cast<size_t>(c.w) * c.h);
        uint32_t seed = 7u;
        for (int y = 0; y < c.h; ++y) {
            for (int x = 0; x < c.w; ++x) {
                double v = 2048.0 + 1400.0 * std::sin(x * 0.11) * std::cos(y * 0.09);
                if (x > c.w / 4 && x < c.w / 2 && y > c.h / 4 && y < c.h / 2) v += 1500.0;                 // a bright block
                if (y >= c.h / 2 && x < c.w / 3) v = 300.0;                                               // a flat dark region
                int q = static_cast<int>(v) + static_cast<int>(lcg_noise(seed) * 48.0f);
                in[static_cast<size_t>(y) * c.w + x] = static_cast<float>(std::min(std::max(q, 1), 4094) * c.mult + c.offset);
            }
        }
        in[0] = static_cast<float>(c.offset);
        in[1] = static_cast<float>(4095 * c.mult + c.offset);   // pin the range: min = offset, max = offset + 4095 (x mult)
        const std::vector<double> ref = ReferenceClahe(in, c.w, c.h, c.clip, c.tw, c.th);

        std::vector<float> got = in;
        XpeImageBuffer img = make_f32(static_cast<uint32_t>(c.w), static_cast<uint32_t>(c.h), 0.0f);
        std::copy(got.begin(), got.end(), static_cast<float*>(img.data));
        XpeClaheParams params{};
        params.clip_limit = c.clip;
        params.tile_width = c.tw;
        params.tile_height = c.th;
        ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params)) << c.w << "x" << c.h;
        const float* o = static_cast<const float*>(img.data);
        const double range = 4095.0 * c.mult;
        double worst = 0.0;
        size_t worstAt = 0;
        for (size_t i = 0; i < in.size(); ++i) {
            const double d = std::fabs(static_cast<double>(o[i]) - ref[i]);
            if (d > worst) { worst = d; worstAt = i; }
        }
        EXPECT_LE(worst, 2.0e-4 * range) << c.w << "x" << c.h << " tiles " << c.tw << "x" << c.th << " clip " << c.clip
                                         << ": worst difference " << worst << " at pixel " << worstAt;
        free_img(img);
    }
}

// QA-B-207b E6 (Codex #120): a NARROW but finite value range. The scale `4095.0f / range` overflowed float for every range
// below 4095 / FLT_MAX = 1.2034e-35; the int conversion of the resulting Inf/NaN product is undefined and the call returned
// XPE_OK with pixels off by 97 % of the value range (measured before the fix: .moai/reports/lane-post/QA-B-207/b_e6_red.txt).
// The pixels sit at the CENTRES of bins, so no value is on a bin edge in any arithmetic; the minimum and the maximum are pinned.
// Compared with ReferenceClahe, whose bin is floor(d * 4095 / range) (not the implementation's multiply-by-scale), and the same
// cases against the Fraction-based Python reference in b_e6_green.txt.
TEST(ContrastEnhance, NarrowFiniteRange_MatchesTheReferenceAndIsNotTreatedAsBroken) {
    const double kFltMax = static_cast<double>(std::numeric_limits<float>::max());
    const double edge = 4095.0 / kFltMax;   // 1.2034e-35: the range below which a float scale overflows
    struct Case { const char* what; double vmin, vmax; int w, h, tw, th; float clip; };
    const Case cases[] = {
        {"0 and 1e-37",                              0.0,   1e-37,                                 32, 32, 4, 4, 3.0f},
        {"0 and FLT_MIN",                            0.0,   static_cast<double>(std::numeric_limits<float>::min()), 32, 32, 4, 4, 3.0f},
        {"-5e-38 and +5e-38 (minimum not 0)",        -5e-38, 5e-38,                                32, 32, 4, 4, 3.0f},
        {"range just below where a float scale is finite", 0.0, edge * 0.99,                       32, 32, 4, 4, 3.0f},
        {"range just above where a float scale is finite", 0.0, edge * 1.01,                       32, 32, 4, 4, 3.0f},
        {"0 and 1e-30",                              0.0,   1e-30,                                 33, 29, 3, 5, 2.0f},
        {"control: 0 and 1",                         0.0,   1.0,                                   32, 32, 4, 4, 3.0f},
    };
    for (const Case& c : cases) {
        const float lo = static_cast<float>(c.vmin), hi = static_cast<float>(c.vmax);
        const double span = static_cast<double>(hi) - static_cast<double>(lo);
        std::vector<float> in(static_cast<size_t>(c.w) * c.h);
        uint32_t s = 12345u;
        for (int y = 0; y < c.h; ++y) {
            for (int x = 0; x < c.w; ++x) {
                s = s * 1664525u + 1013904223u;
                uint32_t m = (s >> 8) % 4096u;
                if (y < c.h / 2 && x < c.w / 3) m %= 40u;                       // a dark, low-contrast corner
                in[static_cast<size_t>(y) * c.w + x] = static_cast<float>(static_cast<double>(lo) + span * (m + 0.5) / 4096.0);
            }
        }
        in[0] = lo;
        in[1] = hi;
        const std::vector<double> ref = ReferenceClahe(in, c.w, c.h, c.clip, c.tw, c.th);

        XpeImageBuffer img = make_f32(static_cast<uint32_t>(c.w), static_cast<uint32_t>(c.h), 0.0f);
        std::copy(in.begin(), in.end(), static_cast<float*>(img.data));
        XpeClaheParams params{};
        params.clip_limit = c.clip;
        params.tile_width = c.tw;
        params.tile_height = c.th;
        ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params)) << c.what;
        const float* o = static_cast<const float*>(img.data);
        double worst = 0.0;
        size_t worstAt = 0;
        bool finite = true;
        for (size_t i = 0; i < in.size(); ++i) {
            if (!std::isfinite(o[i])) finite = false;
            const double d = std::fabs(static_cast<double>(o[i]) - ref[i]) / span;
            if (d > worst) { worst = d; worstAt = i; }
        }
        EXPECT_TRUE(finite) << c.what;
        EXPECT_LE(worst, 2.0e-4) << c.what << ": worst difference " << worst << " of the value range at pixel " << worstAt;
        free_img(img);
    }
}

// QA-B-207c (Codex #141): the bin must be floor((v - vmin) * 4095 / range) EXACTLY. Multiplying by a precomputed
// 4095 / range rounds the reciprocal: for 0..123, v = 41 is bin 1365 (41 * 4095 / 123 = 1365 exactly) but 41 * (4095 / 123)
// evaluates to 1364.9999999999998 and truncates to 1364. A bin one off moves the output only through the clipped-excess
// spread (every bin gets its share), so these inputs clip hard (clip_limit 1) and the comparison is tight (1e-5 of range).
static void RunClahe(const std::vector<float>& in, int w, int h, float clip, int tw, int th, std::vector<float>& out) {
    XpeImageBuffer img = make_f32(static_cast<uint32_t>(w), static_cast<uint32_t>(h), 0.0f);
    std::copy(in.begin(), in.end(), static_cast<float*>(img.data));
    XpeClaheParams params{};
    params.clip_limit = clip;
    params.tile_width = tw;
    params.tile_height = th;
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    const float* o = static_cast<const float*>(img.data);
    out.assign(o, o + in.size());
    free_img(img);
}

// Codex's case: the 4x4 block [0,123,41,41, 41,41,1,2, 3,4,5,6, 7,8,9,10] four times (8x8, 2x2 tiles, clip 3). The value 41
// comes out 107.625 with the exact bin (1365) and 99.9375 with the rounded one (1364).
TEST(ContrastEnhance, BinOnAnExactBoundary_CodexCase_41In0To123) {
    const float block[16] = {0, 123, 41, 41, 41, 41, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<float> in(64);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) in[static_cast<size_t>(y) * 8 + x] = block[(y % 4) * 4 + (x % 4)];
    const std::vector<double> ref = ReferenceClahe(in, 8, 8, 3.0f, 2, 2);
    std::vector<float> got;
    RunClahe(in, 8, 8, 3.0f, 2, 2, got);
    // 107.625 is the exact-rational value (Python fractions, clahe_ref_exact.py case "codex 41 in 0..123"), taken from
    // Codex's independent calculation as well; the reference function must reproduce it before it is used as a judge.
    EXPECT_NEAR(107.625, ref[2], 1e-9) << "ReferenceClahe disagrees with the exact-rational value";
    for (size_t i = 0; i < in.size(); ++i)
        EXPECT_NEAR(ref[i], got[i], 1e-5 * 123.0) << "pixel " << i << " value " << in[i];
}

// Every bin boundary: for k = 0..4095 the smallest value of bin k, v = ceil(k * range / 4095), and the value just below it,
// for several ranges. Integer ranges are exact in float; the narrow float ranges put v on the float grid, where the
// neighbours are one ulp apart. All of them against ReferenceClahe (exact floor of the multiply-first quotient).
TEST(ContrastEnhance, BinOnEveryBoundary_MatchesTheExactQuotient) {
    struct Case { const char* what; double range; };
    const Case cases[] = {
        {"range 123", 123.0}, {"range 4095", 4095.0}, {"range 65535", 65535.0}, {"range 1000", 1000.0},
        {"range 1e-37", 1e-37}, {"range 1.21e-35", 1.21e-35}, {"range 7.3", 7.3},
    };
    for (const Case& c : cases) {
        const float hi = static_cast<float>(c.range);
        std::vector<float> in;
        in.push_back(0.0f);
        in.push_back(hi);
        const bool integral = (c.range == std::floor(c.range));
        for (int k = 1; k < 4095; ++k) {
            // the real boundary k * range / 4095 rounded to float, and its two float neighbours
            const float b = static_cast<float>(static_cast<double>(k) * static_cast<double>(hi) / 4095.0);
            in.push_back(std::nextafter(b, 0.0f));
            in.push_back(b);
            in.push_back(std::nextafter(b, hi * 2.0f));
            if (integral) {   // the first integer of bin k and the last one of bin k-1
                const float first = std::ceil(b);
                in.push_back(first);
                in.push_back(first - 1.0f);
            }
        }
        while (in.size() % 128) in.push_back(hi * 0.5f);
        const int w = 128, h = static_cast<int>(in.size()) / 128;
        const std::vector<double> ref = ReferenceClahe(in, w, h, 1.0f, 2, 2);
        std::vector<float> got;
        RunClahe(in, w, h, 1.0f, 2, 2, got);
        double worst = 0.0;
        size_t at = 0;
        for (size_t i = 0; i < in.size(); ++i) {
            const double d = std::fabs(static_cast<double>(got[i]) - ref[i]) / static_cast<double>(hi);
            if (d > worst) { worst = d; at = i; }
        }
        EXPECT_LE(worst, 1e-5) << c.what << ": worst " << worst << " of the range at pixel " << at << " (value " << in[at] << ")";
    }
}

// Edge case: Flat image should produce no artifacts
TEST(ContrastEnhance, FlatImage_NoArtifacts) {
    const uint32_t W = 64, H = 64;
    auto img = make_f32(W, H, 500.0f);

    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;

    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));

    float* px = static_cast<float*>(img.data);
    float first = px[0];
    for (uint32_t i = 1; i < W * H; i++) {
        EXPECT_NEAR(first, px[i], 1.0f) << "Flat image diverged at pixel " << i;
    }
    free_img(img);
}

// REQ-ENH-017: Performance <= 50ms for 3072x3072
TEST(ContrastEnhance, Performance_3072x3072_Within50ms) {
    // QA-B-207 M2 (E6): this used to fill the image with ONE value, and a flat image returns before any tile work, so it
    // timed nothing (9 ms). The data is the benchmark's: pseudo-random 1..4096, every tile fully populated.
    auto img = make_f32(3072, 3072, 0.0f);
    {
        float* px = static_cast<float*>(img.data);
        const size_t n = static_cast<size_t>(3072) * 3072;
        for (size_t i = 0; i < n; ++i) px[i] = 1.0f + static_cast<float>((i * 2654435761u) % 4096u);
    }
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;

    auto t0 = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - t0).count();
    EXPECT_LE(ms, 50) << "CLAHE 3072x3072 took " << ms << "ms, budget 50ms";
    free_img(img);
}

} // namespace

// ---------------------------------------------------------------------------
// #179 (QA-B-86): measure-only benchmark at the SPEC size -- no time assertion.
// The name carries BenchmarkFreeze (selected by benchmark-regression.yml -R)
// and Performance (excluded by ci.yml -E, which runs on shared runners).
// ---------------------------------------------------------------------------
TEST(ContrastEnhance, BenchmarkFreeze_Performance_REQ_ENH_017_Clahe3072) {
    constexpr uint32_t kSize = 3072;
    const size_t n = static_cast<size_t>(kSize) * kSize;
    std::vector<float> pristine(n);
    for (size_t i = 0; i < n; ++i)
        pristine[i] = 1.0f + static_cast<float>((i * 2654435761u) % 4096u);
    auto img = make_f32(kSize, kSize, 0.0f);
    float* px = static_cast<float*>(img.data);
    auto reset = [&] { std::copy(pristine.begin(), pristine.end(), px); };
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;
    perf_measure::Measure("REQ-ENH-017/xpe_contrast_enhance", "3072x3072", reset,
                          [&] { return xpe_contrast_enhance(&img, &params); });
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(px[i])) { ADD_FAILURE() << "non-finite output at " << i; break; }
    }
    free_img(img);
}
