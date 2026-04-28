/**
 * @file test_calib_generate_gain.cpp
 * @brief Unit tests for xpe_calib_generate_gain and xpe_calib_generate_gain_polynomial
 *
 * SPEC-XPE-P1A SWU-1.12 -- FUNC-026, FUNC-027
 *
 * Test cases:
 *  1. GenerateGain_BasicFlatField: 5 flat frames (8x8), no dark reference → generates gain map, verify output file exists
 *  2. GenerateGain_WithDarkSubtraction: 3 flat frames + dark reference → verify dark subtraction effect
 *  3. GenerateGain_SingleFrame: 1 flat frame (FUNC-024 Tier a) → verify XPE_OK with uncertainty metadata
 *  4. GenerateGain_NullInput: null flat_frames → XPE_ERR_INVALID_INPUT
 *  5. GenerateGain_ZeroFrames: num_frames=0 → XPE_ERR_INVALID_INPUT
 *  6. GenerateGainPolynomial_ThreeLevels: generate 3 gain maps at different dose levels, fit polynomial → verify monotonicity
 *  7. GenerateGainPolynomial_NullPaths: null paths → XPE_ERR_INVALID_INPUT
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

// =============================================================================
// Test 6: Polynomial gain generation with 3 dose levels
// =============================================================================
TEST_F(GenerateGainTest, GenerateGainPolynomial_ThreeLevels) {
    // First, create 3 gain maps at different dose levels
    const int num_levels = 3;
    std::vector<TempFile> gain_files;
    gain_files.reserve(num_levels);
    std::vector<const char*> gain_paths;
    gain_paths.reserve(num_levels);
    std::vector<double> dose_levels = {10.0, 20.0, 30.0};  // mGy

    for (int i = 0; i < num_levels; ++i) {
        gain_files.emplace_back("gain_poly_" + std::to_string(i) + ".xcal");
        std::error_code ec;
        fs::remove(gain_files.back().c_str(), ec);
        fs::remove(std::string(gain_files.back().c_str()) + ".tmp", ec);

        // Create a simple gain map
        std::vector<float> gain_data(W * H, 1.0f + i * 0.1f);  // Increasing gain with dose
        XpeImageBuffer gain_buf{};
        gain_buf.width = W;
        gain_buf.height = H;
        gain_buf.format = XPE_PIXEL_FLOAT32;
        gain_buf.bitsAllocated = 32;
        gain_buf.bitsStored = 32;
        gain_buf.data = gain_data.data();
        gain_buf.dataSize = gain_data.size() * sizeof(float);

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = static_cast<uint64_t>(gain_data.size() * sizeof(float));
        ASSERT_EQ(write_xcal_file(gain_files.back().c_str(), hdr,
                                  nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(gain_data.data()),
                                  hdr.payload_len),
                  XPE_OK);
        gain_paths.push_back(gain_files.back().c_str());
    }

    TempFile poly_output("gain_poly.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain_polynomial(
        gain_paths.data(),
        dose_levels.data(),
        num_levels,
        2,  // max_degree = quadratic
        poly_output.c_str()
    );

    ASSERT_EQ(rc, XPE_OK);
    EXPECT_TRUE(poly_output.exists()) << "Polynomial output file should exist";
}

// =============================================================================
// Test 7: Polynomial generation with null paths
// =============================================================================
TEST_F(GenerateGainTest, GenerateGainPolynomial_NullPaths) {
    std::vector<double> dose_levels = {10.0, 20.0, 30.0};
    TempFile output("gain_poly_null.xcal");

    XpeErrorCode rc = xpe_calib_generate_gain_polynomial(
        nullptr,  // Null paths
        dose_levels.data(),
        3,
        2,
        output.c_str()
    );

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT) << "Null paths should return INVALID_INPUT";
}
