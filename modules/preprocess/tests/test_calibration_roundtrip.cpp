/**
 * @file test_calibration_roundtrip.cpp
 * @brief Calibration Pipeline Round-trip Verification Tests
 *
 * Tests verify the full calibration data lifecycle for pixel-accurate results:
 *   generate_offset → save → load → apply_offset
 *
 * Each stage verifies the formula matches the specification, not just
 * that I/O succeeds.
 *
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 * REQ coverage: REQ-P1A-035 to REQ-P1A-040
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace {

namespace fs = std::filesystem;

// RAII temp-file guard (auto-removes on destruction)
class TempFile {
public:
    explicit TempFile(std::string suffix = ".xpec") {
        path_ = fs::temp_directory_path() /
                ("xpe_rt_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) + suffix);
    }
    ~TempFile() { std::error_code ec; fs::remove(path_, ec); }
    const std::string& str() const { return path_string_; }
    const char* c_str() {
        path_string_ = path_.string();
        return path_string_.c_str();
    }
private:
    fs::path    path_;
    std::string path_string_;
};

static uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

static XpeImageBuffer makeU16Buf(std::vector<uint16_t>& v, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

static XpeImageBuffer makeF32Buf(std::vector<float>& v, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = v.size() * sizeof(float);
    return b;
}

// ==========================================================================
// Stage 1: xpe_calib_generate_offset — per-pixel mean computation
// REQ-P1A-039
// ==========================================================================
class GenerateOffsetTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8, H = 8, N = W * H;
};

// Single-frame mean = frame itself
TEST_F(GenerateOffsetTest, SingleFrameMeanEqualsFrame) {
    std::vector<uint16_t> framePixels(N, 1234u);
    std::vector<uint16_t> outPixels(N, 0u);

    XpeImageBuffer frame = makeU16Buf(framePixels, W, H);
    XpeImageBuffer out   = makeU16Buf(outPixels, W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&frame, 1, &out, nullptr));

    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(1234u, outPixels[i]) << "pixel[" << i << "]";
}

// Mean of N identical frames equals that value
TEST_F(GenerateOffsetTest, MultiFrameSameValueMeanEqualsValue) {
    const uint16_t val = 500u;
    const uint32_t frameCount = 5;

    std::vector<std::vector<uint16_t>> fd(frameCount, std::vector<uint16_t>(N, val));
    std::vector<XpeImageBuffer> frames(frameCount);
    for (uint32_t f = 0; f < frameCount; ++f)
        frames[f] = makeU16Buf(fd[f], W, H);

    std::vector<uint16_t> outPixels(N, 0u);
    XpeImageBuffer out = makeU16Buf(outPixels, W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(frames.data(), frameCount, &out, nullptr));

    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(val, outPixels[i]) << "pixel[" << i << "] identical-frame mean";
}

// Per-pixel mean across 3 frames with known values
TEST_F(GenerateOffsetTest, ThreeFramesMeanIsCorrect) {
    // 3 single-pixel images: [100], [200], [300] → mean = 200
    static constexpr uint32_t SW = 1, SH = 1, SN = 1;
    std::vector<uint16_t> f0 = {100}, f1 = {200}, f2 = {300};
    std::vector<uint16_t> out = {0};

    XpeImageBuffer frames[3] = {makeU16Buf(f0, SW, SH),
                                  makeU16Buf(f1, SW, SH),
                                  makeU16Buf(f2, SW, SH)};
    XpeImageBuffer outBuf = makeU16Buf(out, SW, SH);

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(frames, 3, &outBuf, nullptr));
    EXPECT_NEAR(200, static_cast<int>(out[0]), 1)
        << "mean of [100, 200, 300] must be ~200";
}

// Zero frameCount → XPE_ERR_INVALID_INPUT
TEST_F(GenerateOffsetTest, ZeroFrameCountReturnsError) {
    std::vector<uint16_t> dummy(N, 0u);
    XpeImageBuffer frame = makeU16Buf(dummy, W, H);
    XpeImageBuffer out   = makeU16Buf(dummy, W, H);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_offset(&frame, 0, &out, nullptr));
}

// ==========================================================================
// Stages 2-3: xpe_calib_save → xpe_calib_load_* round-trip
// REQ-P1A-035 to REQ-P1A-038
// ==========================================================================
class SaveLoadRoundtripTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8, H = 8, N = W * H;
    const uint64_t kFutureExpiry = nowMs() + 365ULL * 24 * 3600 * 1000;
};

// Offset (uint16) save → load_offset preserves pixel values
TEST_F(SaveLoadRoundtripTest, OffsetUint16RoundtripPreservesPixels) {
    TempFile tmp;

    std::vector<uint16_t> original(N);
    for (uint32_t i = 0; i < N; ++i)
        original[i] = static_cast<uint16_t>((i * 257) % 65535);

    std::vector<uint16_t> loaded(N, 0u);
    XpeImageBuffer saveMap   = makeU16Buf(original, W, H);
    XpeImageBuffer loadedMap = makeU16Buf(loaded, W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, tmp.c_str(), kFutureExpiry, nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(tmp.c_str(), &loadedMap));

    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(original[i], loaded[i]) << "pixel[" << i << "] offset round-trip";
}

// Gain (float32) save → load_gain preserves pixel values within float epsilon
TEST_F(SaveLoadRoundtripTest, GainFloat32RoundtripPreservesPixels) {
    TempFile tmp;

    std::vector<float> original(N);
    for (uint32_t i = 0; i < N; ++i)
        original[i] = 0.5f + static_cast<float>(i) * 0.01f;

    std::vector<float> loaded(N, 0.0f);
    XpeImageBuffer saveMap   = makeF32Buf(original, W, H);
    XpeImageBuffer loadedMap = makeF32Buf(loaded, W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, tmp.c_str(), kFutureExpiry, nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(tmp.c_str(), &loadedMap));

    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(original[i], loaded[i]) << "pixel[" << i << "] gain round-trip";
}

// Corrupted payload (byte flip) → CRC failure → XPE_ERR_IO_FAILED
TEST_F(SaveLoadRoundtripTest, CorruptedPayloadFailsCrcCheck) {
    TempFile tmp;

    std::vector<float> pixels(N, 1.0f);
    XpeImageBuffer saveMap = makeF32Buf(pixels, W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, tmp.c_str(), kFutureExpiry, nullptr));

    // Flip a byte in the payload (after 64-byte CalibFileHeader)
    FILE* f = std::fopen(tmp.c_str(), "r+b");
    ASSERT_NE(nullptr, f);
    std::fseek(f, 64, SEEK_SET);
    uint8_t bad = 0xAA;
    std::fwrite(&bad, 1, 1, f);
    std::fclose(f);

    std::vector<float> loaded(N, 0.0f);
    XpeImageBuffer loadedMap = makeF32Buf(loaded, W, H);
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_calib_load_gain(tmp.c_str(), &loadedMap));
}

// Expired calibration → XPE_ERR_CALIBRATION_EXPIRED
TEST_F(SaveLoadRoundtripTest, ExpiredCalibrationReturnsExpiryError) {
    TempFile tmp;

    std::vector<uint16_t> pixels(N, 100u);
    XpeImageBuffer saveMap = makeU16Buf(pixels, W, H);

    const uint64_t pastExpiry = 1000ULL; // epoch+1s = definitely expired
    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, tmp.c_str(), pastExpiry, nullptr));

    std::vector<uint16_t> loaded(N, 0u);
    XpeImageBuffer loadedMap = makeU16Buf(loaded, W, H);
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED,
              xpe_calib_load_offset(tmp.c_str(), &loadedMap));
}

// ==========================================================================
// xpe_calib_check_expiry round-trip
// REQ-P1A-040
// ==========================================================================
class ExpiryRoundtripTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4, H = 4, N = W * H;
};

// save with known expiry → check_expiry reads back same value
TEST_F(ExpiryRoundtripTest, ExpiryTimestampPreservedRoundtrip) {
    TempFile tmp;

    std::vector<uint16_t> pixels(N, 200u);
    XpeImageBuffer saveMap = makeU16Buf(pixels, W, H);

    const uint64_t expiryMs = nowMs() + 90ULL * 24 * 3600 * 1000; // 90 days
    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, tmp.c_str(), expiryMs, nullptr));

    uint64_t readback = 0;
    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry(tmp.c_str(), &readback));
    EXPECT_EQ(expiryMs, readback) << "expiry timestamp must survive save→check_expiry";
}

// Future expiry → XPE_OK; past expiry → XPE_ERR_CALIBRATION_EXPIRED
TEST_F(ExpiryRoundtripTest, ExpiryClassificationIsCorrect) {
    std::vector<uint16_t> pixels(N, 100u);
    XpeImageBuffer saveMap = makeU16Buf(pixels, W, H);

    TempFile validFile, expiredFile;

    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, validFile.c_str(),
                                     nowMs() + 3600000ULL, nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_save(&saveMap, expiredFile.c_str(),
                                     1000ULL, nullptr));

    uint64_t dummy = 0;
    EXPECT_EQ(XPE_OK, xpe_calib_check_expiry(validFile.c_str(), &dummy));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED,
              xpe_calib_check_expiry(expiredFile.c_str(), &dummy));
}

// ==========================================================================
// Full Pipeline: generate_offset → save → load → apply_offset
// End-to-end verification of pixel-accurate calibration correction
// REQ-P1A-035 to REQ-P1A-040
// ==========================================================================
class FullPipelineTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8, H = 8, N = W * H;
    const uint64_t kFutureExpiry = nowMs() + 365ULL * 24 * 3600 * 1000;
};

// Step 1→4: generate→save→load→apply produces corrected = raw - dark
TEST_F(FullPipelineTest, DarkSubtractionEndToEnd) {
    TempFile tmp;

    const uint16_t kDark = 300u;
    const uint16_t kRaw  = 1000u;

    // Step 1: Generate offset map from 4 identical dark frames
    const uint32_t frameCount = 4;
    std::vector<std::vector<uint16_t>> fd(frameCount, std::vector<uint16_t>(N, kDark));
    std::vector<XpeImageBuffer> frames(frameCount);
    for (uint32_t f = 0; f < frameCount; ++f)
        frames[f] = makeU16Buf(fd[f], W, H);

    std::vector<uint16_t> offsetPixels(N, 0u);
    XpeImageBuffer offsetMap = makeU16Buf(offsetPixels, W, H);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(frames.data(), frameCount,
                                                  &offsetMap, nullptr));

    // Verify generated offset = kDark
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(kDark, offsetPixels[i]) << "generated offset pixel[" << i << "]";

    // Step 2: Save offset map
    ASSERT_EQ(XPE_OK, xpe_calib_save(&offsetMap, tmp.c_str(), kFutureExpiry, nullptr));

    // Step 3: Load offset map back
    std::vector<uint16_t> loadedOffset(N, 0u);
    XpeImageBuffer loaded = makeU16Buf(loadedOffset, W, H);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(tmp.c_str(), &loaded));

    // Step 4: Apply offset correction to raw image
    std::vector<uint16_t> rawPixels(N, kRaw);
    XpeImageBuffer rawImg = makeU16Buf(rawPixels, W, H);
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&rawImg, &loaded));

    // Verify: corrected = raw - dark = 1000 - 300 = 700
    const uint16_t expected = kRaw - kDark;
    const auto* result = static_cast<const uint16_t*>(rawImg.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(expected, result[i])
            << "pixel[" << i << "] end-to-end: expected " << expected;
}

// Dark > raw: corrected saturates to 0 (no underflow)
TEST_F(FullPipelineTest, OffsetExceedsRawClampsToZero) {
    TempFile tmp;

    const uint16_t kDark = 500u;
    const uint16_t kRaw  = 200u; // raw < dark

    std::vector<uint16_t> darkPixels(N, kDark);
    XpeImageBuffer offsetMap = makeU16Buf(darkPixels, W, H);
    ASSERT_EQ(XPE_OK, xpe_calib_save(&offsetMap, tmp.c_str(), kFutureExpiry, nullptr));

    std::vector<uint16_t> loadedPx(N, 0u);
    XpeImageBuffer loaded = makeU16Buf(loadedPx, W, H);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(tmp.c_str(), &loaded));

    std::vector<uint16_t> rawPx(N, kRaw);
    XpeImageBuffer rawImg = makeU16Buf(rawPx, W, H);
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&rawImg, &loaded));

    const auto* result = static_cast<const uint16_t*>(rawImg.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(0u, result[i])
            << "pixel[" << i << "] dark > raw must clamp to 0";
}

} // namespace
