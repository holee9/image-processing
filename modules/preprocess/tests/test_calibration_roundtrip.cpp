/**
 * @file test_calibration_roundtrip.cpp
 * @brief Calibration Pipeline Round-trip Verification Tests
 *
 * Tests verify the full calibration data lifecycle for pixel-accurate results:
 *   generate_offset -> load -> save -> apply_offset
 *
 * Each stage verifies the formula matches the specification, not just
 * that I/O succeeds.
 *
 * QA-A-25 (#120) migration note. This suite was written against the retired
 * map-as-argument API family and had never been compiled. The shipped API
 * (issue #117 decision B) is file- and global-store based:
 *   - xpe_calib_generate_offset(frames, n, integration_ms, temp_c, out_path)
 *     writes an XCal v1 OFFSET file (FLOAT32 payload) instead of filling a buffer
 *   - xpe_calib_load_offset/gain(path) load into the global calibration store
 *   - xpe_calib_save(path, "offset"|"gain"|"defect") serialises that store and
 *     always writes expiry_epoch_ms = 0 (xpe_calib_save.cpp:56)
 *   - xpe_calib_check_expiry(path, &is_expired, &remaining_days) returns XPE_OK
 *     for an expired file and reports expiry through its out-parameters
 *   - xpe_offset_correct(input, output, metadata) writes into a caller buffer
 * Expiry therefore has to be set by writing the XCal file directly; the
 * assertions below follow the shipped contract, not the retired one.
 *
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 * REQ coverage: REQ-P1A-035 to REQ-P1A-040
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <string>

namespace {

namespace fs = std::filesystem;

// RAII temp-file guard (auto-removes on destruction, .tmp sibling included --
// write_xcal_file writes path+".tmp" first and renames)
class TempFile {
public:
    explicit TempFile(std::string suffix = ".xcal") {
        path_ = fs::temp_directory_path() /
                ("xpe_rt_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) + suffix);
        path_string_ = path_.string();
    }
    ~TempFile() {
        std::error_code ec;
        fs::remove(path_, ec);
        fs::remove(path_string_ + ".tmp", ec);
    }
    const char* c_str() const { return path_string_.c_str(); }
    const std::string& str() const { return path_string_; }
private:
    fs::path    path_;
    std::string path_string_;
};

static int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static XpeImageBuffer makeU16Buf(std::vector<uint16_t>& v, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

// Writes an XCal v1 file with a FLOAT32 payload and a caller-chosen expiry.
// `expiry_ms` 0 means "never expires" (xcal_format.h 0x20).
static void writeXCal(const char* path, XCalType type,
                      const std::vector<float>& values,
                      uint32_t w, uint32_t h, int64_t expiry_ms) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = static_cast<uint32_t>(type);
    hdr.pixel_format     = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = nowMs();
    hdr.expiry_epoch_ms  = expiry_ms;
    hdr.payload_len      = static_cast<uint64_t>(values.size() * sizeof(float));

    ASSERT_EQ(XPE_OK,
              write_xcal_file(path, hdr, nullptr, 0,
                              reinterpret_cast<const uint8_t*>(values.data()),
                              hdr.payload_len));
}

// Reads the FLOAT32 payload back out of an XCal v1 file (header is 152 bytes,
// then config_json_len bytes, then the payload).
static void readXCalPayload(const char* path, std::vector<float>* out) {
    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f.is_open()) << "cannot open " << path;

    XCalFileHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    ASSERT_EQ(static_cast<std::streamsize>(sizeof(hdr)), f.gcount());
    ASSERT_EQ(0, std::memcmp(hdr.magic, XCAL_MAGIC, 4));
    ASSERT_EQ(static_cast<uint32_t>(XCAL_FMT_FLOAT32), hdr.pixel_format);

    f.seekg(static_cast<std::streamoff>(hdr.config_json_len), std::ios::cur);
    out->assign(static_cast<size_t>(hdr.payload_len / sizeof(float)), 0.0f);
    f.read(reinterpret_cast<char*>(out->data()),
           static_cast<std::streamsize>(hdr.payload_len));
    ASSERT_EQ(static_cast<std::streamsize>(hdr.payload_len), f.gcount());
}

// Every suite here drives the global calibration store, so each needs the
// module initialised (offset/gain correction return XPE_ERR_NOT_INITIALIZED
// otherwise) and torn back down so the next suite starts from an empty store.
class CalibFixture : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

// ==========================================================================
// Stage 1: xpe_calib_generate_offset -- per-pixel mean computation
// REQ-P1A-039
// ==========================================================================
class RoundtripGenerateOffsetTest : public CalibFixture {
protected:
    static constexpr uint32_t W = 8, H = 8, N = W * H;
    static constexpr float kIntegrationMs = 100.0f;
    static constexpr float kTempC         = 25.0f;
};

// Single-frame mean = frame itself
TEST_F(RoundtripGenerateOffsetTest, SingleFrameMeanEqualsFrame) {
    TempFile tmp;
    std::vector<uint16_t> framePixels(N, 1234u);
    XpeImageBuffer frame = makeU16Buf(framePixels, W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&frame, 1, kIntegrationMs,
                                                kTempC, tmp.c_str()));

    std::vector<float> generated;
    ASSERT_NO_FATAL_FAILURE(readXCalPayload(tmp.c_str(), &generated));
    ASSERT_EQ(N, generated.size());
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(1234.0f, generated[i]) << "pixel[" << i << "]";
}

// Mean of N identical frames equals that value
TEST_F(RoundtripGenerateOffsetTest, MultiFrameSameValueMeanEqualsValue) {
    TempFile tmp;
    const uint16_t val = 500u;
    const int32_t frameCount = 5;

    std::vector<std::vector<uint16_t>> fd(frameCount, std::vector<uint16_t>(N, val));
    std::vector<XpeImageBuffer> frames(frameCount);
    for (int32_t f = 0; f < frameCount; ++f)
        frames[f] = makeU16Buf(fd[f], W, H);

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(frames.data(), frameCount,
                                                kIntegrationMs, kTempC, tmp.c_str()));

    std::vector<float> generated;
    ASSERT_NO_FATAL_FAILURE(readXCalPayload(tmp.c_str(), &generated));
    ASSERT_EQ(N, generated.size());
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(static_cast<float>(val), generated[i])
            << "pixel[" << i << "] identical-frame mean";
}

// Per-pixel mean across 3 frames with known values
TEST_F(RoundtripGenerateOffsetTest, ThreeFramesMeanIsCorrect) {
    TempFile tmp;
    // 3 single-pixel images: [100], [200], [300] -> mean = 200
    static constexpr uint32_t SW = 1, SH = 1;
    std::vector<uint16_t> f0 = {100}, f1 = {200}, f2 = {300};

    XpeImageBuffer frames[3] = {makeU16Buf(f0, SW, SH),
                                makeU16Buf(f1, SW, SH),
                                makeU16Buf(f2, SW, SH)};

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(frames, 3, kIntegrationMs,
                                                kTempC, tmp.c_str()));

    std::vector<float> generated;
    ASSERT_NO_FATAL_FAILURE(readXCalPayload(tmp.c_str(), &generated));
    ASSERT_EQ(1u, generated.size());
    EXPECT_NEAR(200.0f, generated[0], 1.0f)
        << "mean of [100, 200, 300] must be ~200";
}

// Zero frameCount -> XPE_ERR_INVALID_INPUT
TEST_F(RoundtripGenerateOffsetTest, ZeroFrameCountReturnsError) {
    TempFile tmp;
    std::vector<uint16_t> dummy(N, 0u);
    XpeImageBuffer frame = makeU16Buf(dummy, W, H);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_offset(&frame, 0, kIntegrationMs,
                                        kTempC, tmp.c_str()));
}

// ==========================================================================
// Stages 2-3: xpe_calib_load_* -> xpe_calib_save round-trip
// REQ-P1A-035 to REQ-P1A-038
// ==========================================================================
class SaveLoadRoundtripTest : public CalibFixture {
protected:
    static constexpr uint32_t W = 8, H = 8, N = W * H;
};

// Offset map survives load -> save unchanged
TEST_F(SaveLoadRoundtripTest, OffsetRoundtripPreservesPixels) {
    TempFile src, dst;

    std::vector<float> original(N);
    for (uint32_t i = 0; i < N; ++i)
        original[i] = static_cast<float>((i * 257) % 65535);

    ASSERT_NO_FATAL_FAILURE(writeXCal(src.c_str(), XCAL_TYPE_OFFSET, original, W, H, 0));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(src.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_save(dst.c_str(), "offset"));

    std::vector<float> loaded;
    ASSERT_NO_FATAL_FAILURE(readXCalPayload(dst.c_str(), &loaded));
    ASSERT_EQ(N, loaded.size());
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(original[i], loaded[i]) << "pixel[" << i << "] offset round-trip";
}

// Gain map survives load -> save within float epsilon
TEST_F(SaveLoadRoundtripTest, GainFloat32RoundtripPreservesPixels) {
    TempFile src, dst;

    std::vector<float> original(N);
    for (uint32_t i = 0; i < N; ++i)
        original[i] = 0.5f + static_cast<float>(i) * 0.01f;

    ASSERT_NO_FATAL_FAILURE(writeXCal(src.c_str(), XCAL_TYPE_GAIN, original, W, H, 0));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(src.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_save(dst.c_str(), "gain"));

    std::vector<float> loaded;
    ASSERT_NO_FATAL_FAILURE(readXCalPayload(dst.c_str(), &loaded));
    ASSERT_EQ(N, loaded.size());
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(original[i], loaded[i]) << "pixel[" << i << "] gain round-trip";
}

// Corrupted payload -> SHA-256 mismatch. The reader reports that as
// XPE_ERR_CONFIG_INVALID (xcal_reader.cpp:210-211), not IO_FAILED: the read
// itself succeeded, the contents failed validation.
TEST_F(SaveLoadRoundtripTest, CorruptedPayloadFailsHashCheck) {
    TempFile tmp;

    std::vector<float> pixels(N, 1.0f);
    ASSERT_NO_FATAL_FAILURE(writeXCal(tmp.c_str(), XCAL_TYPE_GAIN, pixels, W, H, 0));

    // Flip a byte in the payload (immediately after the 152-byte XCal header;
    // this file carries no config JSON)
    FILE* f = nullptr;
    ASSERT_EQ(0, fopen_s(&f, tmp.c_str(), "r+b"));
    ASSERT_NE(nullptr, f);
    std::fseek(f, static_cast<long>(sizeof(XCalFileHeader)), SEEK_SET);
    uint8_t bad = 0xAA;
    std::fwrite(&bad, 1, 1, f);
    std::fclose(f);

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain(tmp.c_str()));
}

// Expired calibration -> XPE_ERR_CALIBRATION_EXPIRED (xcal_reader.cpp:227-232)
TEST_F(SaveLoadRoundtripTest, ExpiredCalibrationReturnsExpiryError) {
    TempFile tmp;

    std::vector<float> pixels(N, 100.0f);
    const int64_t pastExpiry = 1000;  // epoch+1s = definitely expired
    ASSERT_NO_FATAL_FAILURE(
        writeXCal(tmp.c_str(), XCAL_TYPE_OFFSET, pixels, W, H, pastExpiry));

    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_offset(tmp.c_str()));
}

// ==========================================================================
// xpe_calib_check_expiry
// REQ-P1A-040
// ==========================================================================
class ExpiryRoundtripTest : public CalibFixture {
protected:
    static constexpr uint32_t W = 4, H = 4, N = W * H;
};

// A known future expiry is readable back as a remaining-day count
TEST_F(ExpiryRoundtripTest, ExpiryTimestampPreservedRoundtrip) {
    TempFile tmp;

    std::vector<float> pixels(N, 200.0f);
    const int64_t expiryMs = nowMs() + 90LL * 24 * 3600 * 1000;  // 90 days
    ASSERT_NO_FATAL_FAILURE(
        writeXCal(tmp.c_str(), XCAL_TYPE_OFFSET, pixels, W, H, expiryMs));

    bool    isExpired     = true;
    int32_t remainingDays = -1;
    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry(tmp.c_str(), &isExpired, &remainingDays));
    EXPECT_FALSE(isExpired);
    // Truncating division to days can land on 89 when the write straddles a
    // millisecond boundary, so accept 89 or 90.
    EXPECT_GE(remainingDays, 89);
    EXPECT_LE(remainingDays, 90);
}

// Future expiry -> not expired; past expiry -> expired with negative days.
// Both calls return XPE_OK: expiry travels through the out-parameters.
TEST_F(ExpiryRoundtripTest, ExpiryClassificationIsCorrect) {
    std::vector<float> pixels(N, 100.0f);
    TempFile validFile, expiredFile;

    ASSERT_NO_FATAL_FAILURE(writeXCal(validFile.c_str(), XCAL_TYPE_OFFSET,
                                      pixels, W, H, nowMs() + 3600000LL));
    ASSERT_NO_FATAL_FAILURE(writeXCal(expiredFile.c_str(), XCAL_TYPE_OFFSET,
                                      pixels, W, H, 1000));

    bool    isExpired     = true;
    int32_t remainingDays = 0;
    EXPECT_EQ(XPE_OK, xpe_calib_check_expiry(validFile.c_str(),
                                             &isExpired, &remainingDays));
    EXPECT_FALSE(isExpired);

    EXPECT_EQ(XPE_OK, xpe_calib_check_expiry(expiredFile.c_str(),
                                             &isExpired, &remainingDays));
    EXPECT_TRUE(isExpired);
    EXPECT_LT(remainingDays, 0);
}

// ==========================================================================
// Full Pipeline: generate_offset -> load -> apply_offset
// End-to-end verification of pixel-accurate calibration correction
// REQ-P1A-035 to REQ-P1A-040
// ==========================================================================
class FullPipelineTest : public CalibFixture {
protected:
    static constexpr uint32_t W = 8, H = 8, N = W * H;

    // Generates an offset map of `dark` from 4 identical dark frames, loads it,
    // then corrects a uniform `raw` image through it.
    void runPipeline(const char* path, uint16_t dark, uint16_t raw,
                     std::vector<uint16_t>* out) {
        const int32_t frameCount = 4;
        std::vector<std::vector<uint16_t>> fd(frameCount,
                                              std::vector<uint16_t>(N, dark));
        std::vector<XpeImageBuffer> frames(frameCount);
        for (int32_t f = 0; f < frameCount; ++f)
            frames[f] = makeU16Buf(fd[f], W, H);

        ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(frames.data(), frameCount,
                                                    100.0f, 25.0f, path));

        std::vector<float> generated;
        ASSERT_NO_FATAL_FAILURE(readXCalPayload(path, &generated));
        for (uint32_t i = 0; i < N; ++i)
            ASSERT_FLOAT_EQ(static_cast<float>(dark), generated[i])
                << "generated offset pixel[" << i << "]";

        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(path));

        std::vector<uint16_t> rawPixels(N, raw);
        out->assign(N, 0u);
        XpeImageBuffer rawImg = makeU16Buf(rawPixels, W, H);
        XpeImageBuffer outImg = makeU16Buf(*out, W, H);
        XpeImageMetadata metadata{};
        ASSERT_EQ(XPE_OK, xpe_offset_correct(&rawImg, &outImg, &metadata));
    }
};

// generate -> load -> apply produces corrected = raw - dark
TEST_F(FullPipelineTest, DarkSubtractionEndToEnd) {
    TempFile tmp;
    const uint16_t kDark = 300u;
    const uint16_t kRaw  = 1000u;

    std::vector<uint16_t> result;
    ASSERT_NO_FATAL_FAILURE(runPipeline(tmp.c_str(), kDark, kRaw, &result));

    const uint16_t expected = kRaw - kDark;
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(expected, result[i])
            << "pixel[" << i << "] end-to-end: expected " << expected;
}

// Dark > raw: corrected saturates to 0 (no underflow)
TEST_F(FullPipelineTest, OffsetExceedsRawClampsToZero) {
    TempFile tmp;
    const uint16_t kDark = 500u;
    const uint16_t kRaw  = 200u;  // raw < dark

    std::vector<uint16_t> result;
    ASSERT_NO_FATAL_FAILURE(runPipeline(tmp.c_str(), kDark, kRaw, &result));

    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(0u, result[i]) << "pixel[" << i << "] dark > raw must clamp to 0";
}

} // namespace
