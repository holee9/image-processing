/**
 * @file test_calib_save_expiry.cpp
 * @brief xpe_calib_save embedded expiry (api-spec.md §6.14, REQ-P1A-019, #132)
 *
 * QA-A-29 (#132). QA-A-25 measured that `xpe_calib_save` wrote
 * `expiry_epoch_ms = 0` unconditionally (`xpe_calib_save.cpp:57/77/97`), so no
 * public API path could produce an expiring calibration file and both
 * SRS-ALERT-005 (Error alert, acquisition blocked) and REQ-P1A-018 were
 * unreachable through the API. Decision #132 adds a third parameter
 * `uint64_t expiryEpochMs` (Unix milliseconds, 0 = never expires); the expiry
 * policy itself stays with the caller and the native side only records it.
 *
 * The read-back contract these cases assert was itself measured in QA-A-25:
 * `xpe_calib_check_expiry` returns XPE_OK even for an expired file and reports
 * expiry through its out-parameters (`xpe_calib_check_expiry.cpp:70-80`), while
 * `xpe_calib_load_offset` refuses an expired file with
 * XPE_ERR_CALIBRATION_EXPIRED (`xcal_reader.cpp:227-232`).
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

#include <chrono>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace {

constexpr uint32_t W = 16, H = 16;

static int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class CalibSaveExpiryTest : public ::testing::Test {
protected:
    const char* srcPath = "a29_src_offset.xcal";
    const char* dstPath = "a29_dst_offset.xcal";

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, MakeOffsetXCal(srcPath, W, H, 1.5f));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(srcPath));
    }

    void TearDown() override {
        std::remove(srcPath);
        std::remove(dstPath);
        std::remove((std::string(dstPath) + ".tmp").c_str());
        xpe_preprocess_shutdown();
    }

    // Reads expiry_epoch_ms straight out of the 152-byte XCal header, so the
    // assertion does not depend on how check_expiry interprets it.
    int64_t readHeaderExpiry(const char* path) {
        std::ifstream f(path, std::ios::binary);
        EXPECT_TRUE(f.is_open()) << "cannot open " << path;
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        EXPECT_EQ(static_cast<std::streamsize>(sizeof(hdr)), f.gcount());
        EXPECT_EQ(0, std::memcmp(hdr.magic, XCAL_MAGIC, 4));
        return hdr.expiry_epoch_ms;
    }
};

// (a) A future expiry is written to the file and read back as not-yet-expired.
TEST_F(CalibSaveExpiryTest, FutureExpiryIsRecordedAndReadsAsNotExpired) {
    const uint64_t expiry = static_cast<uint64_t>(nowMs() + 30LL * 24 * 3600 * 1000);

    ASSERT_EQ(XPE_OK, xpe_calib_save(dstPath, "offset", expiry));
    EXPECT_EQ(static_cast<int64_t>(expiry), readHeaderExpiry(dstPath))
        << "the caller's value must be recorded verbatim";

    bool    isExpired     = true;
    int32_t remainingDays = -1;
    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry(dstPath, &isExpired, &remainingDays));
    EXPECT_FALSE(isExpired);
    EXPECT_GT(remainingDays, 0);
    EXPECT_LE(remainingDays, 30);
}

// (b) A past expiry makes the file expired. check_expiry still returns XPE_OK
// and reports it through the out-parameters (contract measured in QA-A-25).
TEST_F(CalibSaveExpiryTest, PastExpiryReadsAsExpired) {
    const uint64_t pastExpiry = 1000;  // epoch + 1s

    ASSERT_EQ(XPE_OK, xpe_calib_save(dstPath, "offset", pastExpiry));
    EXPECT_EQ(1000, readHeaderExpiry(dstPath));

    bool    isExpired     = false;
    int32_t remainingDays = 0;
    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry(dstPath, &isExpired, &remainingDays));
    EXPECT_TRUE(isExpired);
    EXPECT_LT(remainingDays, 0);
}

// (b2) The loaders enforce it: an expired file written through the API is
// refused. This is the path SRS-ALERT-005 depends on, and it was unreachable
// before #132.
TEST_F(CalibSaveExpiryTest, ExpiredFileIsRefusedByLoader) {
    ASSERT_EQ(XPE_OK, xpe_calib_save(dstPath, "offset", 1000));

    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_offset(dstPath));
}

// (c) 0 keeps the "never expires" behaviour the two-argument form always had.
TEST_F(CalibSaveExpiryTest, ZeroMeansNeverExpires) {
    ASSERT_EQ(XPE_OK, xpe_calib_save(dstPath, "offset", 0));
    EXPECT_EQ(0, readHeaderExpiry(dstPath));

    bool    isExpired     = true;
    int32_t remainingDays = 0;
    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry(dstPath, &isExpired, &remainingDays));
    EXPECT_FALSE(isExpired);
    EXPECT_EQ(INT32_MAX, remainingDays)
        << "never-expires is reported as the saturated day count";

    EXPECT_EQ(XPE_OK, xpe_calib_load_offset(dstPath))
        << "a never-expiring file must still load";
}

// The expiry argument does not disturb the existing validation order: a bad
// calib_type is still rejected, whatever expiry is passed.
TEST_F(CalibSaveExpiryTest, InvalidTypeStillRejectedWithExpiryArgument) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save(dstPath, "bogus_type",
                             static_cast<uint64_t>(nowMs() + 100000)));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_save(nullptr, "offset", 0));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_save(dstPath, nullptr, 0));
}

} // namespace
