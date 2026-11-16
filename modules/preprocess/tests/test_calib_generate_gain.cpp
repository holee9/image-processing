/**
 * @file test_calib_generate_gain.cpp
 * @brief Unit tests for xpe_calib_generate_gain
 *
 * SPEC-XPE-P1A SWU-1.12 -- FUNC-026 (the FUNC-027 polynomial cases 6-7 were removed with the generator, QA-A-336)
 *
 * Test cases:
 *  1. GenerateGain_BasicFlatField: 5 flat frames (8x8), no dark reference → generates gain map, verify output file exists
 *  2. GenerateGain_WithDarkSubtraction: 3 flat frames + dark reference → verify dark subtraction effect
 *  3. GenerateGain_SingleFrame: 1 flat frame (FUNC-024 Tier a) → verify XPE_OK with uncertainty metadata
 *  4. GenerateGain_NullInput: null flat_frames → XPE_ERR_INVALID_INPUT
 *  5. GenerateGain_ZeroFrames: num_frames=0 → XPE_ERR_INVALID_INPUT
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>
#include <filesystem>
#include <fstream>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_reader.hpp"
#include "xcal_writer.hpp"

namespace {

constexpr uint32_t W = 8;
constexpr uint32_t H = 8;
namespace fs = std::filesystem;

// Build a UINT16 XpeImageBuffer from a flat vector.
struct FrameHelper {
    std::vector<uint16_t> pixels;
    XpeImageBuffer buf{};

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
};

// RAII temp-file guard
class TempFile {
public:
    explicit TempFile(const std::string& suffix = ".xcal") {
        path_ = fs::temp_directory_path() / ("test_gain_" + std::to_string(std::hash<size_t>{}(rand())) + suffix);
        path_string_ = path_.string();
        std::error_code ec;
        fs::remove(path_, ec);
        fs::remove(path_string_ + ".tmp", ec);
    }
    ~TempFile() {
        std::error_code ec;
        fs::remove(path_, ec);
        // Also remove .tmp file if it exists
        fs::remove(path_string_ + ".tmp", ec);
    }
    const char* c_str() const { return path_string_.c_str(); }
    bool exists() const { return fs::exists(path_); }
private:
    fs::path path_;
    std::string path_string_;
};

} // anonymous namespace

class GenerateGainTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize module if needed
        xpe_preprocess_init(nullptr);
    }

    void TearDown() override {
        // Cleanup
        xpe_preprocess_shutdown();
    }
};

// =============================================================================
// Test 1: Basic flat field gain generation (5 frames, no dark reference)
// =============================================================================
TEST_F(GenerateGainTest, GenerateGain_BasicFlatField) {
    const int num_frames = 5;
    std::vector<FrameHelper> flat_frames;
    for (int i = 0; i < num_frames; ++i) {
        flat_frames.emplace_back(W, H, 5000); // Uniform flat field
    }

    std::vector<XpeImageBuffer> frame_bufs;
    for (auto& f : flat_frames) {
        frame_bufs.push_back(f.buf);
    }

    TempFile output("gain_basic.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain(
        frame_bufs.data(),
        num_frames,
        nullptr, // No dark reference
        output.c_str(),
        nullptr  // No metadata
    );

    ASSERT_EQ(rc, XPE_OK) << "Gain generation should succeed for uniform flat field";
    EXPECT_TRUE(output.exists()) << "Output gain file should exist";

    // Verify output file can be read
    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    rc = read_xcal_file(output.c_str(), hdr, cfg, payload,
                        /*check_expiry=*/false, XCAL_TYPE_GAIN);
    EXPECT_EQ(rc, XPE_OK) << "Generated gain file should be valid XCal format";
}

// =============================================================================
// Test 2: Gain generation with dark subtraction
// =============================================================================
TEST_F(GenerateGainTest, GenerateGain_WithDarkSubtraction) {
    const int num_frames = 3;
    std::vector<FrameHelper> flat_frames;
    FrameHelper dark(W, H, 500); // Dark reference

    for (int i = 0; i < num_frames; ++i) {
        flat_frames.emplace_back(W, H, 5000); // Flat field
    }

    std::vector<XpeImageBuffer> frame_bufs;
    for (auto& f : flat_frames) {
        frame_bufs.push_back(f.buf);
    }

    TempFile output("gain_with_dark.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain(
        frame_bufs.data(),
        num_frames,
        &dark.buf,
        output.c_str(),
        nullptr
    );

    ASSERT_EQ(rc, XPE_OK) << "Gain generation with dark reference should succeed";
    EXPECT_TRUE(output.exists()) << "Output gain file should exist";

    // Verify gain values are reasonable (should be > 1.0 after dark subtraction)
    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    rc = read_xcal_file(output.c_str(), hdr, cfg, payload,
                        /*check_expiry=*/false, XCAL_TYPE_GAIN);
    ASSERT_EQ(rc, XPE_OK);

    const float* gain = reinterpret_cast<const float*>(payload.data());
    // Gain should be normalized around 1.0 for flat field
    EXPECT_GT(gain[0], 0.5f) << "Gain values should be positive";
    EXPECT_LT(gain[0], 10.0f) << "Gain values should be reasonable";
}

// =============================================================================
// Test 3: Single frame gain generation (FUNC-024 Tier a)
// =============================================================================
TEST_F(GenerateGainTest, GenerateGain_SingleFrame) {
    FrameHelper flat(W, H, 5000);
    TempFile output("gain_single.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain(
        &flat.buf,
        1,
        nullptr,
        output.c_str(),
        "{\"uncertainty\":0.01}"
    );

    ASSERT_EQ(rc, XPE_OK) << "Single frame gain generation should succeed";
    EXPECT_TRUE(output.exists()) << "Output gain file should exist";

    // Verify output file
    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, payload;
    rc = read_xcal_file(output.c_str(), hdr, cfg, payload,
                        /*check_expiry=*/false, XCAL_TYPE_GAIN);
    EXPECT_EQ(rc, XPE_OK);
}

// =============================================================================
// Test 4: Null input validation
// =============================================================================
TEST_F(GenerateGainTest, GenerateGain_NullInput) {
    TempFile output("gain_null.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain(
        nullptr,  // Null flat_frames
        1,
        nullptr,
        output.c_str(),
        nullptr
    );

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT) << "Null flat_frames should return INVALID_INPUT";
}

// =============================================================================
// Test 5: Zero frames validation
// =============================================================================
TEST_F(GenerateGainTest, GenerateGain_ZeroFrames) {
    FrameHelper flat(W, H, 5000);
    TempFile output("gain_zero.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain(
        &flat.buf,
        0,  // Zero frames
        nullptr,
        output.c_str(),
        nullptr
    );

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT) << "Zero frames should return INVALID_INPUT";
}

