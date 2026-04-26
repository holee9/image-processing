/**
 * @file test_defect_gen.cpp
 * @brief Unit tests for xpe_bpm_generate (Bad Pixel Map generation)
 *
 * SPEC-XPE-P1A SWU-1.11 -- FUNC-022, FUNC-023, FUNC-024, FUNC-025
 *
 * Test cases:
 *  1. BpmGenerate_BasicDetection: 5 uniform dark frames + 5 uniform bright frames (16x16) with 1 injected hot pixel and 1 dead pixel → verify detection
 *  2. BpmGenerate_DefaultConfig: pass NULL config → verify defaults work (lambda=8.0, mask_dark=32, tolerance=0.07)
 *  3. BpmGenerate_NullInput: null dark_frames → XPE_ERR_INVALID_INPUT
 *  4. BpmGenerate_InsufficientFrames: num_dark=0 → XPE_ERR_INVALID_INPUT
 *  5. BpmGenerate_OutputFormatCheck: verify output is UINT8 with values in {0,1,2,3}
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <random>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

namespace {

constexpr uint32_t W = 16;
constexpr uint32_t H = 16;

// Build a UINT16 XpeImageBuffer from a flat vector.
struct FrameHelper {
    std::vector<uint16_t> pixels;
    XpeImageBuffer buf;

    explicit FrameHelper(uint32_t w, uint32_t h, uint16_t fill = 0) {
        pixels.assign(static_cast<size_t>(w) * h, fill);
        std::memset(&buf, 0, sizeof(buf));
        buf.width       = w;
        buf.height      = h;
        buf.format      = XPE_PIXEL_UINT16;
        buf.bitsAllocated = 16;
        buf.bitsStored    = 16;
        buf.data        = pixels.data();
        buf.dataSize    = pixels.size() * sizeof(uint16_t);
    }

    // Set individual pixel
    void set(uint32_t row, uint32_t col, uint16_t val) {
        pixels[row * buf.width + col] = val;
    }

    // Get individual pixel
    uint16_t get(uint32_t row, uint32_t col) const {
        return pixels[row * buf.width + col];
    }
};

} // anonymous namespace

class BpmGenerateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize module
        xpe_preprocess_init(nullptr);
    }

    void TearDown() override {
        // Cleanup
        xpe_preprocess_shutdown();
    }

    // Helper to inject a hot pixel (very high value)
    void injectHotPixel(FrameHelper& frame, uint32_t row, uint32_t col) {
        frame.set(row, col, 30000); // Very high value for hot pixel
    }

    // Helper to inject a dead pixel (very low value)
    void injectDeadPixel(FrameHelper& frame, uint32_t row, uint32_t col) {
        frame.set(row, col, 50); // Very low value for dead pixel
    }
};

// =============================================================================
// Test 1: Basic BPM detection with injected defects
// =============================================================================
TEST_F(BpmGenerateTest, BpmGenerate_BasicDetection) {
    const uint32_t num_dark = 5;
    const uint32_t num_bright = 5;

    // Create uniform dark frames
    std::vector<FrameHelper> dark_frames;
    for (uint32_t i = 0; i < num_dark; ++i) {
        dark_frames.emplace_back(W, H, 500); // Uniform dark level
    }

    // Create uniform bright frames with injected defects
    std::vector<FrameHelper> bright_frames;
    for (uint32_t i = 0; i < num_bright; ++i) {
        bright_frames.emplace_back(W, H, 8000); // Uniform bright level
    }

    // Inject one hot pixel in all bright frames at same location
    const uint32_t hot_row = 5, hot_col = 5;
    for (uint32_t i = 0; i < num_bright; ++i) {
        injectHotPixel(bright_frames[i], hot_row, hot_col);
    }

    // Inject one dead pixel in all bright frames at same location
    const uint32_t dead_row = 8, dead_col = 8;
    for (uint32_t i = 0; i < num_bright; ++i) {
        injectDeadPixel(bright_frames[i], dead_row, dead_col);
    }

    // Prepare frame buffers
    std::vector<XpeImageBuffer> dark_bufs, bright_bufs;
    for (auto& f : dark_frames) dark_bufs.push_back(f.buf);
    for (auto& f : bright_frames) bright_bufs.push_back(f.buf);

    // Output BPM buffer
    std::vector<uint8_t> bpm_data(W * H, 0);
    XpeImageBuffer bpm_out{};
    bpm_out.width = W;
    bpm_out.height = H;
    bpm_out.format = XPE_PIXEL_UINT8;
    bpm_out.bitsAllocated = 8;
    bpm_out.bitsStored = 8;
    bpm_out.data = bpm_data.data();
    bpm_out.dataSize = bpm_data.size();

    // Generate BPM with default config (NULL = use defaults)
    XpeErrorCode rc = xpe_bpm_generate(
        dark_bufs.data(),
        num_dark,
        bright_bufs.data(),
        num_bright,
        nullptr, // Use default config
        &bpm_out
    );

    ASSERT_EQ(rc, XPE_OK) << "BPM generation should succeed";

    // Verify hot pixel detected (value should be 2 or 3 for hot/noisy)
    uint8_t hot_pixel_value = bpm_data[hot_row * W + hot_col];
    EXPECT_GE(hot_pixel_value, 2) << "Hot pixel should be detected (value >= 2)";

    // Verify dead pixel detected (value should be 1 or 3 for dead/stuck)
    uint8_t dead_pixel_value = bpm_data[dead_row * W + dead_col];
    EXPECT_GE(dead_pixel_value, 1) << "Dead pixel should be detected (value >= 1)";

    // Verify most pixels are good (value = 0)
    uint32_t good_pixels = 0;
    for (uint8_t v : bpm_data) {
        if (v == 0) good_pixels++;
    }
    float good_ratio = static_cast<float>(good_pixels) / static_cast<float>(W * H);
    EXPECT_GT(good_ratio, 0.95f) << "At least 95% of pixels should be good (0)";
}

// =============================================================================
// Test 2: Default config (NULL) should work
// =============================================================================
TEST_F(BpmGenerateTest, BpmGenerate_DefaultConfig) {
    const uint32_t num_dark = 5;
    const uint32_t num_bright = 10;

    // Create uniform frames
    std::vector<FrameHelper> dark_frames, bright_frames;
    for (uint32_t i = 0; i < num_dark; ++i) {
        dark_frames.emplace_back(W, H, 500);
    }
    for (uint32_t i = 0; i < num_bright; ++i) {
        bright_frames.emplace_back(W, H, 8000);
    }

    std::vector<XpeImageBuffer> dark_bufs, bright_bufs;
    for (auto& f : dark_frames) dark_bufs.push_back(f.buf);
    for (auto& f : bright_frames) bright_bufs.push_back(f.buf);

    std::vector<uint8_t> bpm_data(W * H, 0);
    XpeImageBuffer bpm_out{};
    bpm_out.width = W;
    bpm_out.height = H;
    bpm_out.format = XPE_PIXEL_UINT8;
    bpm_out.bitsAllocated = 8;
    bpm_out.bitsStored = 8;
    bpm_out.data = bpm_data.data();
    bpm_out.dataSize = bpm_data.size();

    // Pass NULL config → should use defaults
    XpeErrorCode rc = xpe_bpm_generate(
        dark_bufs.data(),
        num_dark,
        bright_bufs.data(),
        num_bright,
        nullptr, // NULL = default config
        &bpm_out
    );

    EXPECT_EQ(rc, XPE_OK) << "Default config (NULL) should work";

    // All pixels should be good in uniform image
    for (uint8_t v : bpm_data) {
        EXPECT_EQ(v, 0) << "Uniform image should have no defects";
    }
}

// =============================================================================
// Test 3: Null input validation
// =============================================================================
TEST_F(BpmGenerateTest, BpmGenerate_NullInput) {
    const uint32_t num_bright = 10;

    std::vector<FrameHelper> bright_frames;
    for (uint32_t i = 0; i < num_bright; ++i) {
        bright_frames.emplace_back(W, H, 8000);
    }
    std::vector<XpeImageBuffer> bright_bufs;
    for (auto& f : bright_frames) bright_bufs.push_back(f.buf);

    std::vector<uint8_t> bpm_data(W * H, 0);
    XpeImageBuffer bpm_out{};
    bpm_out.width = W;
    bpm_out.height = H;
    bpm_out.format = XPE_PIXEL_UINT8;
    bpm_out.bitsAllocated = 8;
    bpm_out.bitsStored = 8;
    bpm_out.data = bpm_data.data();
    bpm_out.dataSize = bpm_data.size();

    XpeErrorCode rc = xpe_bpm_generate(
        nullptr, // Null dark_frames
        0,
        bright_bufs.data(),
        num_bright,
        nullptr,
        &bpm_out
    );

    // Should accept null dark_frames if num_dark == 0
    // But if num_dark > 0 with null pointer, should fail
    rc = xpe_bpm_generate(
        nullptr, // Null dark_frames
        5,       // But num_dark > 0
        bright_bufs.data(),
        num_bright,
        nullptr,
        &bpm_out
    );

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT) << "Null dark_frames with num_dark > 0 should return INVALID_INPUT";
}

// =============================================================================
// Test 4: Insufficient frames validation
// =============================================================================
TEST_F(BpmGenerateTest, BpmGenerate_InsufficientFrames) {
    std::vector<FrameHelper> dark_frames, bright_frames;
    std::vector<XpeImageBuffer> dark_bufs, bright_bufs;

    // Create 0 frames
    std::vector<uint8_t> bpm_data(W * H, 0);
    XpeImageBuffer bpm_out{};
    bpm_out.width = W;
    bpm_out.height = H;
    bpm_out.format = XPE_PIXEL_UINT8;
    bpm_out.bitsAllocated = 8;
    bpm_out.bitsStored = 8;
    bpm_out.data = bpm_data.data();
    bpm_out.dataSize = bpm_data.size();

    // Test with 0 dark frames
    XpeErrorCode rc = xpe_bpm_generate(
        nullptr,
        0, // 0 dark frames
        bright_bufs.data(),
        0, // 0 bright frames
        nullptr,
        &bpm_out
    );

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT) << "0 frames should return INVALID_INPUT";
}

// =============================================================================
// Test 5: Output format validation
// =============================================================================
TEST_F(BpmGenerateTest, BpmGenerate_OutputFormatCheck) {
    const uint32_t num_dark = 5;
    const uint32_t num_bright = 10;

    std::vector<FrameHelper> dark_frames, bright_frames;
    for (uint32_t i = 0; i < num_dark; ++i) {
        dark_frames.emplace_back(W, H, 500);
    }
    for (uint32_t i = 0; i < num_bright; ++i) {
        bright_frames.emplace_back(W, H, 8000);
    }

    std::vector<XpeImageBuffer> dark_bufs, bright_bufs;
    for (auto& f : dark_frames) dark_bufs.push_back(f.buf);
    for (auto& f : bright_frames) bright_bufs.push_back(f.buf);

    std::vector<uint8_t> bpm_data(W * H, 0);
    XpeImageBuffer bpm_out{};
    bpm_out.width = W;
    bpm_out.height = H;
    bpm_out.format = XPE_PIXEL_UINT8;
    bpm_out.bitsAllocated = 8;
    bpm_out.bitsStored = 8;
    bpm_out.data = bpm_data.data();
    bpm_out.dataSize = bpm_data.size();

    XpeErrorCode rc = xpe_bpm_generate(
        dark_bufs.data(),
        num_dark,
        bright_bufs.data(),
        num_bright,
        nullptr,
        &bpm_out
    );

    ASSERT_EQ(rc, XPE_OK) << "BPM generation should succeed";

    // Verify all output values are in valid range {0, 1, 2, 3}
    for (uint8_t v : bpm_data) {
        EXPECT_TRUE(v == 0 || v == 1 || v == 2 || v == 3)
            << "BPM values must be in {0,1,2,3}, got " << static_cast<int>(v);
    }

    // Verify format is UINT8
    EXPECT_EQ(bpm_out.format, XPE_PIXEL_UINT8);
}

// =============================================================================
// Test 6: Custom config parameters
// =============================================================================
TEST_F(BpmGenerateTest, BpmGenerate_CustomConfig) {
    const uint32_t num_dark = 5;
    const uint32_t num_bright = 10;

    std::vector<FrameHelper> dark_frames, bright_frames;
    for (uint32_t i = 0; i < num_dark; ++i) {
        dark_frames.emplace_back(W, H, 500);
    }
    for (uint32_t i = 0; i < num_bright; ++i) {
        bright_frames.emplace_back(W, H, 8000);
    }

    std::vector<XpeImageBuffer> dark_bufs, bright_bufs;
    for (auto& f : dark_frames) dark_bufs.push_back(f.buf);
    for (auto& f : bright_frames) bright_bufs.push_back(f.buf);

    std::vector<uint8_t> bpm_data(W * H, 0);
    XpeImageBuffer bpm_out{};
    bpm_out.width = W;
    bpm_out.height = H;
    bpm_out.format = XPE_PIXEL_UINT8;
    bpm_out.bitsAllocated = 8;
    bpm_out.bitsStored = 8;
    bpm_out.data = bpm_data.data();
    bpm_out.dataSize = bpm_data.size();

    // Custom config with non-default values
    XpeBpmConfig cfg{};
    cfg.lambda_dark = 10.0f;         // Higher than default (8.0)
    cfg.mask_size_dark = 64;         // Larger than default (32)
    cfg.tolerance_pct = 0.08f;       // Higher than default (0.07)
    cfg.mask_size_bright = 256;      // Larger than default (128)
    cfg.min_frames_dark = 3;
    cfg.min_frames_bright = 5;

    XpeErrorCode rc = xpe_bpm_generate(
        dark_bufs.data(),
        num_dark,
        bright_bufs.data(),
        num_bright,
        &cfg, // Custom config
        &bpm_out
    );

    EXPECT_EQ(rc, XPE_OK) << "Custom config should work";

    // Uniform image should still produce all-good BPM
    for (uint8_t v : bpm_data) {
        EXPECT_EQ(v, 0) << "Uniform image with custom config should have no defects";
    }
}
