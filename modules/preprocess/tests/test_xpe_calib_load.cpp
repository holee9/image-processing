/**
 * @file test_xpe_calib_load.cpp
 * @brief Unit tests for xpe_calib_load_offset / load_gain / load_defect_map (T-006)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-014, REQ-P1A-015, REQ-P1A-016
 *
 * Test cases (per function, total >= 6 each):
 *  LoadOffset:
 *   1. Happy path: valid XCal v1 OFFSET file -> XPE_OK, g_calib populated
 *   2. Null path -> XPE_ERR_INVALID_INPUT
 *   3. Non-existent file -> XPE_ERR_IO_FAILED
 *   4. Bad magic file -> XPE_ERR_IO_FAILED or XPE_ERR_CONFIG_INVALID
 *   5. Wrong type (GAIN file loaded as offset) -> XPE_ERR_CONFIG_INVALID
 *   6. SHA-256 mismatch (tampered payload) -> XPE_ERR_CONFIG_INVALID
 *   7. Expired file -> XPE_ERR_CALIBRATION_EXPIRED
 *
 *  LoadGain:
 *   8. Happy path: valid GAIN file -> XPE_OK
 *   9. Null path -> XPE_ERR_INVALID_INPUT
 *  10. Non-existent file -> XPE_ERR_IO_FAILED
 *  11. Wrong type (OFFSET file) -> XPE_ERR_CONFIG_INVALID
 *  12. SHA-256 mismatch -> XPE_ERR_CONFIG_INVALID
 *  13. Expired file -> XPE_ERR_CALIBRATION_EXPIRED
 *
 *  LoadDefectMap:
 *  14. Happy path: valid DEFECT file -> XPE_OK
 *  15. Null path -> XPE_ERR_INVALID_INPUT
 *  16. Non-existent file -> XPE_ERR_IO_FAILED
 *  17. Wrong type (OFFSET file) -> XPE_ERR_CONFIG_INVALID
 *  18. SHA-256 mismatch -> XPE_ERR_CONFIG_INVALID
 *  19. Expired file with check_expiry=false -> XPE_OK (defect maps ignore expiry)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <chrono>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

namespace {
constexpr uint32_t W = 64;
constexpr uint32_t H = 64;
constexpr float    OVAL = 2.5f;
constexpr float    GVAL = 0.8f;
constexpr uint8_t  DVAL = 0;
} // anonymous namespace

// =============================================================================
// Fixture
// =============================================================================
class CalibLoadTest : public ::testing::Test {
protected:
    const char* offset_path  = "t006_offset.xcal";
    const char* gain_path    = "t006_gain.xcal";
    const char* defect_path  = "t006_defect.xcal";
    const char* bad_path     = "t006_bad.xcal";
    const char* tamper_path  = "t006_tamper.xcal";

    void TearDown() override {
        std::remove(offset_path);
        std::remove(gain_path);
        std::remove(defect_path);
        std::remove(bad_path);
        std::remove(tamper_path);
        std::remove((std::string(offset_path) + ".tmp").c_str());
        std::remove((std::string(gain_path)   + ".tmp").c_str());
        std::remove((std::string(defect_path) + ".tmp").c_str());
    }

    /**
     * @brief Write a valid XCal OFFSET file and then flip one byte in the payload.
     */
    bool MakeTamperedOffset(const char* path) {
        if (MakeOffsetXCal(path, W, H, OVAL) != XPE_OK) return false;
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!f) return false;
        // Payload starts immediately after header
        f.seekp(static_cast<std::streamoff>(sizeof(XCalFileHeader)));
        char byte = 0;
        f.read(&byte, 1);
        f.seekp(-1, std::ios::cur);
        byte ^= 0xFF;
        f.write(&byte, 1);
        return f.good();
    }

    /**
     * @brief Write a valid XCal GAIN file and then flip one byte in the payload.
     */
    bool MakeTamperedGain(const char* path) {
        if (MakeGainXCal(path, W, H, GVAL) != XPE_OK) return false;
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!f) return false;
        f.seekp(static_cast<std::streamoff>(sizeof(XCalFileHeader)));
        char byte = 0;
        f.read(&byte, 1);
        f.seekp(-1, std::ios::cur);
        byte ^= 0xFF;
        f.write(&byte, 1);
        return f.good();
    }
};

// =============================================================================
// xpe_calib_load_offset tests
// =============================================================================

// Test 1: Happy path – OFFSET file loads into g_calib
TEST_F(CalibLoadTest, LoadOffset_HappyPath_ReturnsOk) {
    ASSERT_EQ(MakeOffsetXCal(offset_path, W, H, OVAL), XPE_OK);
    XpeErrorCode rc = xpe_calib_load_offset(offset_path);
    EXPECT_EQ(rc, XPE_OK);
}

// Test 2: Null path -> INVALID_INPUT
TEST_F(CalibLoadTest, LoadOffset_NullPath_ReturnsInvalidInput) {
    EXPECT_EQ(xpe_calib_load_offset(nullptr), XPE_ERR_INVALID_INPUT);
}

// Test 3: Non-existent file -> IO_FAILED
TEST_F(CalibLoadTest, LoadOffset_NonExistentFile_ReturnsIoFailed) {
    EXPECT_EQ(xpe_calib_load_offset("does_not_exist_t006.xcal"), XPE_ERR_IO_FAILED);
}

// Test 4: Bad magic (4-byte file) -> IO_FAILED or CONFIG_INVALID
TEST_F(CalibLoadTest, LoadOffset_BadMagic_ReturnsError) {
    ASSERT_TRUE(MakeBadMagicFile(bad_path));
    XpeErrorCode rc = xpe_calib_load_offset(bad_path);
    EXPECT_TRUE(rc == XPE_ERR_IO_FAILED || rc == XPE_ERR_CONFIG_INVALID)
        << "Expected IO_FAILED or CONFIG_INVALID, got " << rc;
}

// Test 5: Wrong type – GAIN file passed to load_offset -> CONFIG_INVALID
TEST_F(CalibLoadTest, LoadOffset_WrongType_ReturnsConfigInvalid) {
    ASSERT_EQ(MakeGainXCal(gain_path, W, H, GVAL), XPE_OK);
    EXPECT_EQ(xpe_calib_load_offset(gain_path), XPE_ERR_CONFIG_INVALID);
}

// Test 6: Tampered payload -> SHA-256 mismatch -> CONFIG_INVALID
TEST_F(CalibLoadTest, LoadOffset_TamperedPayload_ReturnsConfigInvalid) {
    ASSERT_TRUE(MakeTamperedOffset(tamper_path));
    EXPECT_EQ(xpe_calib_load_offset(tamper_path), XPE_ERR_CONFIG_INVALID);
}

// Test 7: Expired file -> CALIBRATION_EXPIRED
TEST_F(CalibLoadTest, LoadOffset_ExpiredFile_ReturnsCalibrationExpired) {
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    int64_t expiry_ms = now_ms - 3600000LL;  // 1 hour ago

    ASSERT_EQ(MakeOffsetXCal(offset_path, W, H, OVAL, expiry_ms), XPE_OK);
    EXPECT_EQ(xpe_calib_load_offset(offset_path), XPE_ERR_CALIBRATION_EXPIRED);
}

// =============================================================================
// xpe_calib_load_gain tests
// =============================================================================

// Test 8: Happy path – GAIN file loads into g_calib
TEST_F(CalibLoadTest, LoadGain_HappyPath_ReturnsOk) {
    ASSERT_EQ(MakeGainXCal(gain_path, W, H, GVAL), XPE_OK);
    EXPECT_EQ(xpe_calib_load_gain(gain_path), XPE_OK);
}

// Test 9: Null path -> INVALID_INPUT
TEST_F(CalibLoadTest, LoadGain_NullPath_ReturnsInvalidInput) {
    EXPECT_EQ(xpe_calib_load_gain(nullptr), XPE_ERR_INVALID_INPUT);
}

// Test 10: Non-existent file -> IO_FAILED
TEST_F(CalibLoadTest, LoadGain_NonExistentFile_ReturnsIoFailed) {
    EXPECT_EQ(xpe_calib_load_gain("no_file_t006.xcal"), XPE_ERR_IO_FAILED);
}

// Test 11: Wrong type – OFFSET file passed to load_gain -> CONFIG_INVALID
TEST_F(CalibLoadTest, LoadGain_WrongType_ReturnsConfigInvalid) {
    ASSERT_EQ(MakeOffsetXCal(offset_path, W, H, OVAL), XPE_OK);
    EXPECT_EQ(xpe_calib_load_gain(offset_path), XPE_ERR_CONFIG_INVALID);
}

// Test 12: Tampered payload -> SHA-256 mismatch -> CONFIG_INVALID
TEST_F(CalibLoadTest, LoadGain_TamperedPayload_ReturnsConfigInvalid) {
    ASSERT_TRUE(MakeTamperedGain(tamper_path));
    EXPECT_EQ(xpe_calib_load_gain(tamper_path), XPE_ERR_CONFIG_INVALID);
}

// Test 13: Expired file -> CALIBRATION_EXPIRED
TEST_F(CalibLoadTest, LoadGain_ExpiredFile_ReturnsCalibrationExpired) {
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    int64_t expiry_ms = now_ms - 3600000LL;

    ASSERT_EQ(MakeGainXCal(gain_path, W, H, GVAL, expiry_ms), XPE_OK);
    EXPECT_EQ(xpe_calib_load_gain(gain_path), XPE_ERR_CALIBRATION_EXPIRED);
}

// =============================================================================
// xpe_calib_load_defect_map tests
// =============================================================================

// Test 14: Happy path – DEFECT file loads into g_calib
TEST_F(CalibLoadTest, LoadDefect_HappyPath_ReturnsOk) {
    ASSERT_EQ(MakeDefectXCal(defect_path, W, H, DVAL), XPE_OK);
    EXPECT_EQ(xpe_calib_load_defect_map(defect_path), XPE_OK);
}

// Test 15: Null path -> INVALID_INPUT
TEST_F(CalibLoadTest, LoadDefect_NullPath_ReturnsInvalidInput) {
    EXPECT_EQ(xpe_calib_load_defect_map(nullptr), XPE_ERR_INVALID_INPUT);
}

// Test 16: Non-existent file -> IO_FAILED
TEST_F(CalibLoadTest, LoadDefect_NonExistentFile_ReturnsIoFailed) {
    EXPECT_EQ(xpe_calib_load_defect_map("no_file_t006d.xcal"), XPE_ERR_IO_FAILED);
}

// Test 17: Wrong type – OFFSET file passed to load_defect_map -> CONFIG_INVALID
TEST_F(CalibLoadTest, LoadDefect_WrongType_ReturnsConfigInvalid) {
    ASSERT_EQ(MakeOffsetXCal(offset_path, W, H, OVAL), XPE_OK);
    EXPECT_EQ(xpe_calib_load_defect_map(offset_path), XPE_ERR_CONFIG_INVALID);
}

// Test 18: Tampered DEFECT payload -> SHA-256 mismatch -> CONFIG_INVALID
TEST_F(CalibLoadTest, LoadDefect_TamperedPayload_ReturnsConfigInvalid) {
    ASSERT_EQ(MakeDefectXCal(defect_path, W, H, DVAL), XPE_OK);
    {
        std::fstream f(defect_path, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(f.is_open());
        f.seekp(static_cast<std::streamoff>(sizeof(XCalFileHeader)));
        char byte = 0;
        f.read(&byte, 1);
        f.seekp(-1, std::ios::cur);
        byte ^= 0xFF;
        f.write(&byte, 1);
    }
    EXPECT_EQ(xpe_calib_load_defect_map(defect_path), XPE_ERR_CONFIG_INVALID);
}

// Test 19: DEFECT map with past expiry -> still XPE_OK (defect maps ignore expiry)
TEST_F(CalibLoadTest, LoadDefect_ExpiredTimestamp_ReturnsOkBecauseNoExpiry) {
    // MakeDefectXCal always writes expiry_epoch_ms=0 (never expires),
    // and xpe_calib_load_defect_map passes check_expiry=false.
    // So even if we manually set an old expiry we should still get XPE_OK.
    ASSERT_EQ(MakeDefectXCal(defect_path, W, H, DVAL), XPE_OK);
    // Manually overwrite expiry to 1 ms (far in the past)
    {
        std::fstream f(defect_path, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(f.is_open());
        // expiry_epoch_ms is at offset 32 in XCalFileHeader
        f.seekp(static_cast<std::streamoff>(offsetof(XCalFileHeader, expiry_epoch_ms)));
        int64_t past_expiry = 1LL;  // 1 ms since epoch = expired
        f.write(reinterpret_cast<const char*>(&past_expiry), sizeof(past_expiry));
        // SHA-256 now mismatches! To make test valid we just check that
        // defect load does NOT reject on expiry (check_expiry=false).
        // The SHA mismatch will cause CONFIG_INVALID -- which is correct
        // because we corrupted the file. So test only validates check_expiry logic
        // is bypass via the implementation.
    }
    // After patching expiry the sha256 now covers original payload
    // but header changed => sha256 still matches (sha256 covers payload, not header).
    // So we expect XPE_OK because SHA-256 is over payload only.
    XpeErrorCode rc = xpe_calib_load_defect_map(defect_path);
    // SHA-256 covers payload only (header excluded), so patching expiry
    // in the header does NOT invalidate the SHA. Defect map load ignores expiry.
    EXPECT_EQ(rc, XPE_OK);
}
