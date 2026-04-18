/**
 * @file test_xpe_calib_check_expiry.cpp
 * @brief Unit tests for xpe_calib_check_expiry (T-009)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-018
 *
 * Test cases:
 *  1. Never-expires file (expiry_epoch_ms == 0): is_expired=false, remaining_days=INT32_MAX
 *  2. Valid (non-expired) file: is_expired=false, remaining_days > 0
 *  3. Expired file (expiry in past): is_expired=true, remaining_days <= 0
 *  4. Null filepath -> XPE_ERR_INVALID_INPUT
 *  5. Null is_expired -> XPE_ERR_INVALID_INPUT
 *  6. Null remaining_days -> XPE_ERR_INVALID_INPUT
 *  7. Non-existent file -> XPE_ERR_IO_FAILED
 *  8. Bad magic file -> XPE_ERR_IO_FAILED or XPE_ERR_CONFIG_INVALID
 */

#include <gtest/gtest.h>
#include <cstring>
#include <climits>
#include <chrono>
#include <fstream>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

namespace {
constexpr uint32_t W = 16;
constexpr uint32_t H = 16;
} // anonymous namespace

class CheckExpiryTest : public ::testing::Test {
protected:
    const char* valid_path   = "t009_valid.xcal";
    const char* expired_path = "t009_expired.xcal";
    const char* never_path   = "t009_never.xcal";
    const char* bad_path     = "t009_bad.xcal";

    void TearDown() override {
        std::remove(valid_path);
        std::remove(expired_path);
        std::remove(never_path);
        std::remove(bad_path);
        std::remove((std::string(valid_path)   + ".tmp").c_str());
        std::remove((std::string(expired_path) + ".tmp").c_str());
        std::remove((std::string(never_path)   + ".tmp").c_str());
    }
};

// =============================================================================
// Test 1: Never-expires (expiry_epoch_ms == 0) -> is_expired=false, remaining=INT32_MAX
// =============================================================================
TEST_F(CheckExpiryTest, NeverExpires_IsExpiredFalse_RemainingMax) {
    ASSERT_EQ(MakeOffsetXCal(never_path, W, H, 1.0f, /*expiry_ms=*/0), XPE_OK);

    bool is_expired = true;
    int32_t remaining = 0;
    ASSERT_EQ(xpe_calib_check_expiry(never_path, &is_expired, &remaining), XPE_OK);
    EXPECT_FALSE(is_expired);
    EXPECT_EQ(remaining, INT32_MAX);
}

// =============================================================================
// Test 2: Future expiry -> is_expired=false, remaining_days > 0
// =============================================================================
TEST_F(CheckExpiryTest, FutureExpiry_IsExpiredFalse) {
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    // Expires in 30 days
    int64_t future_expiry = now_ms + 30LL * 24LL * 3600LL * 1000LL;

    ASSERT_EQ(MakeOffsetXCal(valid_path, W, H, 1.0f, future_expiry), XPE_OK);

    bool is_expired = true;
    int32_t remaining = 0;
    ASSERT_EQ(xpe_calib_check_expiry(valid_path, &is_expired, &remaining), XPE_OK);
    EXPECT_FALSE(is_expired);
    EXPECT_GE(remaining, 29);  // at least 29 days remaining
}

// =============================================================================
// Test 3: Past expiry -> is_expired=true, remaining_days <= 0
// =============================================================================
TEST_F(CheckExpiryTest, PastExpiry_IsExpiredTrue) {
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    // Expired 2 days ago
    int64_t past_expiry = now_ms - 2LL * 24LL * 3600LL * 1000LL;

    ASSERT_EQ(MakeOffsetXCal(expired_path, W, H, 1.0f, past_expiry), XPE_OK);

    bool is_expired = false;
    int32_t remaining = 99;
    ASSERT_EQ(xpe_calib_check_expiry(expired_path, &is_expired, &remaining), XPE_OK);
    EXPECT_TRUE(is_expired);
    EXPECT_LE(remaining, 0);
}

// =============================================================================
// Test 4: Null filepath -> INVALID_INPUT
// =============================================================================
TEST_F(CheckExpiryTest, NullFilepath_ReturnsInvalidInput) {
    bool expired = false;
    int32_t days = 0;
    EXPECT_EQ(xpe_calib_check_expiry(nullptr, &expired, &days), XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 5: Null is_expired -> INVALID_INPUT
// =============================================================================
TEST_F(CheckExpiryTest, NullIsExpired_ReturnsInvalidInput) {
    int32_t days = 0;
    EXPECT_EQ(xpe_calib_check_expiry(never_path, nullptr, &days), XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 6: Null remaining_days -> INVALID_INPUT
// =============================================================================
TEST_F(CheckExpiryTest, NullRemainingDays_ReturnsInvalidInput) {
    bool expired = false;
    EXPECT_EQ(xpe_calib_check_expiry(never_path, &expired, nullptr), XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 7: Non-existent file -> IO_FAILED
// =============================================================================
TEST_F(CheckExpiryTest, NonExistentFile_ReturnsIoFailed) {
    bool expired = false;
    int32_t days = 0;
    EXPECT_EQ(xpe_calib_check_expiry("no_file_t009.xcal", &expired, &days),
              XPE_ERR_IO_FAILED);
}

// =============================================================================
// Test 8: Bad magic -> IO_FAILED or CONFIG_INVALID
// =============================================================================
TEST_F(CheckExpiryTest, BadMagic_ReturnsError) {
    ASSERT_TRUE(MakeBadMagicFile(bad_path));
    bool expired = false;
    int32_t days = 0;
    XpeErrorCode rc = xpe_calib_check_expiry(bad_path, &expired, &days);
    EXPECT_TRUE(rc == XPE_ERR_IO_FAILED || rc == XPE_ERR_CONFIG_INVALID)
        << "Expected IO_FAILED or CONFIG_INVALID, got " << rc;
}
