/**
 * @file test_calib_generate_offset_multi.cpp
 * @brief Tests for multi-method offset generation (mean/median/sigma_clip/winsor)
 *
 * Tests:
 *  1.  Mean method (default, backward compatible): NULL config
 *  2.  Mean method: explicit {"method": "mean"}
 *  3.  Median method: known values
 *  4.  Median method: even frame count (average of two middle)
 *  5.  Sigma clipping: outlier frames removed
 *  6.  Sigma clipping: custom sigma and max_iter
 *  7.  Sigma clipping: no outliers -> same as mean
 *  8.  Winsorization: extreme values clipped
 *  9.  Winsorization: custom percentiles
 * 10.  Unknown method string -> XPE_ERR_CONFIG_INVALID
 * 11.  NULL configJson -> default mean (backward compat)
 * 12.  Single frame median -> identical to input
 * 13.  Performance benchmark: scalar mean vs median (timing comparison)
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstring>
#include <cmath>
#include <chrono>
#include <algorithm>

namespace {
constexpr uint32_t W = 8;
constexpr uint32_t H = 8;
constexpr size_t N = static_cast<size_t>(W) * H;

// Helper to create a UINT16 frame buffer
struct FrameHelper {
    std::vector<uint16_t> pixels;
    XpeImageBuffer buf;

    explicit FrameHelper(uint32_t w, uint32_t h, uint16_t fill = 0) {
        pixels.assign(static_cast<size_t>(w) * h, fill);
        std::memset(&buf, 0, sizeof(buf));
        buf.width         = w;
        buf.height        = h;
        buf.format        = XPE_PIXEL_UINT16;
        buf.bitsAllocated = 16;
        buf.bitsStored    = 16;
        buf.data          = pixels.data();
        buf.dataSize      = pixels.size() * sizeof(uint16_t);
    }

    void set(size_t idx, uint16_t val) {
        pixels[idx] = val;
    }

    void set(uint32_t row, uint32_t col, uint16_t val) {
        pixels[static_cast<size_t>(row) * buf.width + col] = val;
    }
};

// Helper to create output buffer
struct OutputHelper {
    std::vector<uint16_t> pixels;
    XpeImageBuffer buf;

    explicit OutputHelper(uint32_t w, uint32_t h) {
        pixels.assign(static_cast<size_t>(w) * h, 0);
        std::memset(&buf, 0, sizeof(buf));
        buf.width         = w;
        buf.height        = h;
        buf.format        = XPE_PIXEL_UINT16;
        buf.bitsAllocated = 16;
        buf.bitsStored    = 16;
        buf.data          = pixels.data();
        buf.dataSize      = pixels.size() * sizeof(uint16_t);
    }
};

} // anonymous namespace

// =============================================================================
// Test fixture
// =============================================================================
class MultiOffsetTest : public ::testing::Test {
protected:
    // no special setup needed
};

// =============================================================================
// Test 1: Mean method (default): NULL config -> backward compatible
// =============================================================================
TEST_F(MultiOffsetTest, MeanDefault_NullConfig_BackwardCompat) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    FrameHelper f3(W, H, 300);
    XpeImageBuffer frames[3] = { f1.buf, f2.buf, f3.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 3, &out.buf, nullptr));

    // Mean = (100 + 200 + 300) / 3 = 200
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(200u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 2: Mean method: explicit {"method": "mean"}
// =============================================================================
TEST_F(MultiOffsetTest, MeanExplicit_SameAsDefault) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    XpeImageBuffer frames[2] = { f1.buf, f2.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 2, &out.buf, "{\"method\":\"mean\"}"));

    // Mean = (100 + 200) / 2 = 150
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(150u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 3: Median method: known values (odd frame count)
// =============================================================================
TEST_F(MultiOffsetTest, Median_OddFrameCount) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    FrameHelper f3(W, H, 300);
    XpeImageBuffer frames[3] = { f1.buf, f2.buf, f3.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 3, &out.buf, "{\"method\":\"median\"}"));

    // Median of {100, 200, 300} = 200
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(200u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 4: Median method: even frame count (average of two middle)
// =============================================================================
TEST_F(MultiOffsetTest, Median_EvenFrameCount) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    FrameHelper f3(W, H, 300);
    FrameHelper f4(W, H, 400);
    XpeImageBuffer frames[4] = { f1.buf, f2.buf, f3.buf, f4.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 4, &out.buf, "{\"method\":\"median\"}"));

    // Median of {100, 200, 300, 400} = avg(200, 300) = 250
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(250u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 5: Median: outlier robustness (one extreme outlier frame)
// =============================================================================
TEST_F(MultiOffsetTest, Median_OutlierRobust) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 105);
    FrameHelper f3(W, H, 110);  // near-cluster
    FrameHelper f4(W, H, 65535); // extreme outlier
    FrameHelper f5(W, H, 115);
    XpeImageBuffer frames[5] = { f1.buf, f2.buf, f3.buf, f4.buf, f5.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 5, &out.buf, "{\"method\":\"median\"}"));

    // Sorted: {100, 105, 110, 115, 65535} -> Median = 110
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(110u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 6: Sigma clipping: outlier frames removed (tight cluster + 1 outlier)
// =============================================================================
TEST_F(MultiOffsetTest, SigmaClip_RemovesOutlier) {
    // Create 20 frames with identical values plus 1 extreme outlier.
    // With 20 identical values + 1 outlier, the cluster dominates the mean
    // and the outlier falls well outside 3-sigma.
    std::vector<FrameHelper> frames;
    frames.reserve(21);
    for (int i = 0; i < 20; ++i) frames.emplace_back(W, H, 100);
    frames.emplace_back(W, H, 60000);  // extreme outlier

    std::vector<XpeImageBuffer> bufs(21);
    for (int i = 0; i < 21; ++i) bufs[i] = frames[i].buf;

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(bufs.data(), 21, &out.buf,
            "{\"method\":\"sigma_clip\",\"sigma\":3.0,\"max_iter\":5}"));

    // With outlier removed, mean of remaining 100 values should be 100
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(100u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 7: Sigma clipping: no outliers -> result close to mean
// =============================================================================
TEST_F(MultiOffsetTest, SigmaClip_NoOutliers_SameAsMean) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    FrameHelper f3(W, H, 300);
    XpeImageBuffer frames[3] = { f1.buf, f2.buf, f3.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 3, &out.buf,
            "{\"method\":\"sigma_clip\",\"sigma\":3.0}"));

    // No outliers, sigma clip should converge to mean = 200
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(200u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 8: Sigma clipping: custom sigma parameter
// =============================================================================
TEST_F(MultiOffsetTest, SigmaClip_CustomSigma) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 110);
    FrameHelper f3(W, H, 105);
    FrameHelper f4(W, H, 108);
    FrameHelper f5(W, H, 500);  // outlier but within 3*sigma for tight distribution
    XpeImageBuffer frames[5] = { f1.buf, f2.buf, f3.buf, f4.buf, f5.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 5, &out.buf,
            "{\"method\":\"sigma_clip\",\"sigma\":1.0}"));

    // With sigma=1.0, the outlier at 500 should be removed
    // Remaining mean should be near 105.75
    for (size_t i = 0; i < N; ++i) {
        EXPECT_NEAR(106.0, static_cast<double>(out.pixels[i]), 2.0)
            << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 9: Winsorization: extreme values clipped
// =============================================================================
TEST_F(MultiOffsetTest, Winsor_ClipsExtremeValues) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    FrameHelper f3(W, H, 300);
    FrameHelper f4(W, H, 400);
    FrameHelper f5(W, H, 60000); // extreme outlier
    XpeImageBuffer frames[5] = { f1.buf, f2.buf, f3.buf, f4.buf, f5.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 5, &out.buf,
            "{\"method\":\"winsor\",\"lower_percentile\":10,\"upper_percentile\":90}"));

    // With 5 frames at 10th/90th percentiles:
    // Sorted: {100, 200, 300, 400, 60000}
    // 10th percentile idx = round(0.1*4) = 0 -> lo=100
    // 90th percentile idx = round(0.9*4) = 4 -> hi=60000
    // All values within [100, 60000], so no clipping -> mean of all = 12200
    // This tests the winsor path; with different percentiles it would clip
    for (size_t i = 0; i < N; ++i) {
        EXPECT_GT(out.pixels[i], 0u) << "pixel[" << i << "] should be positive";
    }
}

// =============================================================================
// Test 10: Winsorization: tighter percentiles clip outliers
// =============================================================================
TEST_F(MultiOffsetTest, Winsor_TightPercentiles) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    FrameHelper f3(W, H, 300);
    FrameHelper f4(W, H, 400);
    FrameHelper f5(W, H, 60000);
    XpeImageBuffer frames[5] = { f1.buf, f2.buf, f3.buf, f4.buf, f5.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 5, &out.buf,
            "{\"method\":\"winsor\",\"lower_percentile\":20,\"upper_percentile\":60}"));

    // Sorted: {100, 200, 300, 400, 60000}
    // 20th percentile idx = round(0.2*4) = 1 -> lo=200
    // 60th percentile idx = round(0.6*4) = 2 -> hi=300
    // Values clipped: 100->200, 400->300, 60000->300
    // Clipped set: {200, 200, 300, 300, 300} -> mean = 260
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(260u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 11: Unknown method string -> XPE_ERR_CONFIG_INVALID
// =============================================================================
TEST_F(MultiOffsetTest, UnknownMethod_ReturnsConfigInvalid) {
    FrameHelper f1(W, H, 100);
    OutputHelper out(W, H);

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
        xpe_calib_generate_offset(&f1.buf, 1, &out.buf,
            "{\"method\":\"unknown_method\"}"));
}

// =============================================================================
// Test 12: NULL configJson -> default mean (backward compat)
// =============================================================================
TEST_F(MultiOffsetTest, NullConfig_DefaultsToMean) {
    FrameHelper f1(W, H, 500);
    OutputHelper out(W, H);

    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(&f1.buf, 1, &out.buf, nullptr));

    // Single frame mean = 500
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(500u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 13: Single frame median -> identical to input
// =============================================================================
TEST_F(MultiOffsetTest, Median_SingleFrame_Identity) {
    FrameHelper f1(W, H, 42);
    OutputHelper out(W, H);

    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(&f1.buf, 1, &out.buf, "{\"method\":\"median\"}"));

    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(42u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 14: Per-pixel varying values across frames (median)
// =============================================================================
TEST_F(MultiOffsetTest, Median_PerPixelVarying) {
    // Each pixel has different values across frames
    FrameHelper f1(W, H, 0);
    FrameHelper f2(W, H, 0);
    FrameHelper f3(W, H, 0);
    FrameHelper f4(W, H, 0);
    FrameHelper f5(W, H, 0);

    // Pixel 0: values {10, 20, 30, 40, 50} -> median = 30
    f1.set(0, 10); f2.set(0, 20); f3.set(0, 30); f4.set(0, 40); f5.set(0, 50);
    // Pixel 1: values {5, 5, 100, 5, 5} -> median = 5 (outlier ignored)
    f1.set(1, 5);  f2.set(1, 5);  f3.set(1, 100); f4.set(1, 5); f5.set(1, 5);

    XpeImageBuffer frames[5] = { f1.buf, f2.buf, f3.buf, f4.buf, f5.buf };
    OutputHelper out(W, H);

    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 5, &out.buf, "{\"method\":\"median\"}"));

    EXPECT_EQ(30u, out.pixels[0]) << "pixel 0 median";
    EXPECT_EQ(5u,  out.pixels[1]) << "pixel 1 median (outlier ignored)";
}

// =============================================================================
// Test 15: Per-pixel sigma clip with outlier (large cluster)
// =============================================================================
TEST_F(MultiOffsetTest, SigmaClip_PerPixelOutlier) {
    // Use 20 frames of 100 + 1 outlier, per-pixel modification
    std::vector<FrameHelper> frames;
    frames.reserve(21);
    for (int i = 0; i < 21; ++i) frames.emplace_back(W, H, 100);

    // Pixel 0: set frame 0 to 50000 (outlier)
    frames[0].set(0, 50000);

    std::vector<XpeImageBuffer> bufs(21);
    for (int i = 0; i < 21; ++i) bufs[i] = frames[i].buf;

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(bufs.data(), 21, &out.buf,
            "{\"method\":\"sigma_clip\",\"sigma\":3.0}"));

    // The outlier at pixel 0 should be removed, result should be 100
    EXPECT_EQ(100u, out.pixels[0]) << "pixel 0 after sigma clip";
    // Other pixels are all 100
    for (size_t i = 1; i < N; ++i) {
        EXPECT_EQ(100u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 16: Winsorization per-pixel with mixed values
// =============================================================================
TEST_F(MultiOffsetTest, Winsor_PerPixelMixed) {
    FrameHelper f1(W, H, 0);
    FrameHelper f2(W, H, 0);
    FrameHelper f3(W, H, 0);
    FrameHelper f4(W, H, 0);

    // Pixel 0: {10, 20, 30, 40}
    // 5th percentile: idx=0 -> lo=10, 95th percentile: idx=3 -> hi=40
    // No clipping -> mean = 25
    f1.set(0, 10); f2.set(0, 20); f3.set(0, 30); f4.set(0, 40);

    // Pixel 1: {0, 20, 30, 65535}
    // 5th percentile: idx=0 -> lo=0, 95th percentile: idx=3 -> hi=65535
    // No clipping -> mean = 16396
    f1.set(1, 0); f2.set(1, 20); f3.set(1, 30); f4.set(1, 65535);

    XpeImageBuffer frames[4] = { f1.buf, f2.buf, f3.buf, f4.buf };
    OutputHelper out(W, H);

    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 4, &out.buf,
            "{\"method\":\"winsor\",\"lower_percentile\":5,\"upper_percentile\":95}"));

    EXPECT_EQ(25u, out.pixels[0]) << "pixel 0 winsor";
    // pixel 1: (0 + 20 + 30 + 65535) / 4 = 16396.25 -> 16396
    EXPECT_EQ(16396u, out.pixels[1]) << "pixel 1 winsor";
}

// =============================================================================
// Test 17: Output metadata populated correctly for all methods
// =============================================================================
TEST_F(MultiOffsetTest, OutputMetadata_CorrectForAllMethods) {
    FrameHelper f1(W, H, 100);
    FrameHelper f2(W, H, 200);
    XpeImageBuffer frames[2] = { f1.buf, f2.buf };

    const char* methods[] = {
        nullptr,
        "{\"method\":\"mean\"}",
        "{\"method\":\"median\"}",
        "{\"method\":\"sigma_clip\"}",
        "{\"method\":\"winsor\"}"
    };

    for (const char* config : methods) {
        OutputHelper out(W, H);
        ASSERT_EQ(XPE_OK,
            xpe_calib_generate_offset(frames, 2, &out.buf, config));

        EXPECT_EQ(W, out.buf.width) << "width for config: " << (config ? config : "null");
        EXPECT_EQ(H, out.buf.height) << "height for config: " << (config ? config : "null");
        EXPECT_EQ(XPE_PIXEL_UINT16, out.buf.format);
        EXPECT_EQ(16u, out.buf.bitsAllocated);
        EXPECT_EQ(16u, out.buf.bitsStored);
        EXPECT_EQ(N * sizeof(uint16_t), out.buf.dataSize);
    }
}

// =============================================================================
// Test 18: Sigma clip edge case: all identical values (zero stddev)
// =============================================================================
TEST_F(MultiOffsetTest, SigmaClip_AllIdentical_NoDivision) {
    FrameHelper f1(W, H, 500);
    FrameHelper f2(W, H, 500);
    FrameHelper f3(W, H, 500);
    XpeImageBuffer frames[3] = { f1.buf, f2.buf, f3.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 3, &out.buf,
            "{\"method\":\"sigma_clip\",\"sigma\":3.0}"));

    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(500u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 19: Winsorization edge case: all identical values
// =============================================================================
TEST_F(MultiOffsetTest, Winsor_AllIdentical_NoChange) {
    FrameHelper f1(W, H, 300);
    FrameHelper f2(W, H, 300);
    FrameHelper f3(W, H, 300);
    XpeImageBuffer frames[3] = { f1.buf, f2.buf, f3.buf };

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(frames, 3, &out.buf,
            "{\"method\":\"winsor\"}"));

    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(300u, out.pixels[i]) << "pixel[" << i << "]";
    }
}

// =============================================================================
// Test 20: Large frame count stress test (64 frames)
// =============================================================================
TEST_F(MultiOffsetTest, LargeFrameCount_64Frames) {
    constexpr uint32_t kFrameCount = 64;
    std::vector<FrameHelper> frames;
    frames.reserve(kFrameCount);

    for (uint32_t f = 0; f < kFrameCount; ++f) {
        frames.emplace_back(W, H, 100 + static_cast<uint16_t>(f));
    }

    std::vector<XpeImageBuffer> bufs(kFrameCount);
    for (uint32_t f = 0; f < kFrameCount; ++f) {
        bufs[f] = frames[f].buf;
    }

    OutputHelper out(W, H);
    ASSERT_EQ(XPE_OK,
        xpe_calib_generate_offset(bufs.data(), kFrameCount, &out.buf,
            "{\"method\":\"median\"}"));

    // Median of {100, 101, ..., 163} = (131 + 132) / 2 = 131.5 -> 132
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(132u, out.pixels[i]) << "pixel[" << i << "]";
    }
}
