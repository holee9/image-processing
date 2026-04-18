/**
 * @file test_xpe_common.cpp
 * @brief Google Test unit tests for xpe_common.dll
 *
 * Coverage target: >= 85% statement coverage (REQ-P0-006)
 * Test count: >= 45 cases (REQ-P0-007)
 *
 * Test categories:
 *   - Lifecycle       (xpe_init, xpe_shutdown, xpe_version)
 *   - Config          (xpe_configure)
 *   - ParamRange      (xpe_get_param_range)
 *   - ErrorString     (xpe_error_string)
 *   - AlertQueue      (xpe_get_pending_alert_count, xpe_get_pending_alert, xpe_clear_alerts)
 *   - Logging         (xpe_log_set_level, xpe_log_set_file, xpe_log_flush)
 *   - AED             (xpe_aed_configure, xpe_aed_poll_event, xpe_aed_get_status)
 *   - ImageMemory     (xpe_alloc_image, xpe_free_image, xpe_copy_image)
 *   - StructLayout    (P/Invoke struct-size static assertions)
 */

#include "xpe/common/xpe_common_api.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <thread>

// ---------------------------------------------------------------------------
// White-box helpers -- non-exported internal functions for test injection
// (linked from xpe_common.cpp via same target)
// ---------------------------------------------------------------------------
extern "C" {
void xpe_test_inject_aed_event(int32_t eventType, float signalLevel);
void xpe_test_inject_alert(const char* msg, int32_t severity);
}

// ===========================================================================
// Test fixture: ensures init/shutdown symmetry for all test cases
// ===========================================================================
class XpeCommonFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(xpe_init(nullptr), XPE_OK);
    }
    void TearDown() override {
        xpe_clear_alerts();
        xpe_shutdown();
    }
};

// ===========================================================================
// 1. LIFECYCLE TESTS
// ===========================================================================

TEST(XpeLifecycle, InitNullConfigReturnsOk) {
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);
    xpe_shutdown();
}

TEST(XpeLifecycle, InitWithEmptyJsonReturnsConfigInvalid) {
    // Empty string is not valid JSON
    EXPECT_NE(xpe_init(""), XPE_OK);
    // Shutdown is still safe after failed init
    xpe_shutdown();
}

TEST(XpeLifecycle, InitWithValidJsonReturnsOk) {
    EXPECT_EQ(xpe_init("{}"), XPE_OK);
    xpe_shutdown();
}

TEST(XpeLifecycle, DoubleInitIsIdempotent) {
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);  // second call replaces state
    xpe_shutdown();
}

TEST(XpeLifecycle, ShutdownWithoutInitIsNoOp) {
    // No prior init -- should not crash
    xpe_shutdown();
    xpe_shutdown();  // double shutdown also safe
}

// ===========================================================================
// 2. VERSION TESTS
// ===========================================================================

TEST(XpeVersion, VersionIsNonNull) {
    ASSERT_NE(xpe_version(), nullptr);
}

TEST(XpeVersion, VersionIsNonEmpty) {
    ASSERT_GT(std::strlen(xpe_version()), 0u);
}

TEST(XpeVersion, VersionContainsDot) {
    const char* v = xpe_version();
    EXPECT_NE(std::strchr(v, '.'), nullptr) << "Expected semantic version with dot";
}

// ===========================================================================
// 3. CONFIGURE TESTS
// ===========================================================================

TEST_F(XpeCommonFixture, ConfigureNullReturnsInvalidInput) {
    EXPECT_EQ(xpe_configure(nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, ConfigureValidJsonReturnsOk) {
    EXPECT_EQ(xpe_configure("{}"), XPE_OK);
}

TEST_F(XpeCommonFixture, ConfigureInvalidJsonReturnsCfgInvalid) {
    EXPECT_EQ(xpe_configure("not-json"), XPE_ERR_CONFIG_INVALID);
}

TEST_F(XpeCommonFixture, ConfigureWithFieldsReturnsOk) {
    const char* json = R"({"logLevel": 1, "maxWorkers": 4})";
    EXPECT_EQ(xpe_configure(json), XPE_OK);
}

// ===========================================================================
// 4. PARAM RANGE TESTS
// ===========================================================================

TEST_F(XpeCommonFixture, GetParamRangeNullBodyPartFails) {
    float mn, mx, def;
    EXPECT_EQ(xpe_get_param_range(nullptr, "windowWidth", &mn, &mx, &def),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, GetParamRangeNullParamNameFails) {
    float mn, mx, def;
    EXPECT_EQ(xpe_get_param_range("CHEST", nullptr, &mn, &mx, &def),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, GetParamRangeNullOutputPtrFails) {
    float mn, mx;
    EXPECT_EQ(xpe_get_param_range("CHEST", "windowWidth", &mn, &mx, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, GetParamRangeKnownParamReturnsOk) {
    float mn = 0.0f, mx = 0.0f, def = 0.0f;
    EXPECT_EQ(xpe_get_param_range("CHEST", "windowWidth", &mn, &mx, &def), XPE_OK);
}

TEST_F(XpeCommonFixture, GetParamRangeOrdering) {
    float mn = 0.0f, mx = 0.0f, def = 0.0f;
    ASSERT_EQ(xpe_get_param_range("CHEST", "windowWidth", &mn, &mx, &def), XPE_OK);
    EXPECT_LE(mn, def);
    EXPECT_LE(def, mx);
}

TEST_F(XpeCommonFixture, GetParamRangeGammaOrdering) {
    float mn, mx, def;
    ASSERT_EQ(xpe_get_param_range("HAND", "gamma", &mn, &mx, &def), XPE_OK);
    EXPECT_GT(mx, mn);
}

TEST_F(XpeCommonFixture, GetParamRangeUnknownReturnsOkWithDefaults) {
    float mn, mx, def;
    EXPECT_EQ(xpe_get_param_range("CHEST", "unknown_param_xyz", &mn, &mx, &def), XPE_OK);
    EXPECT_LT(mn, mx);
}

// ===========================================================================
// 5. ERROR STRING TESTS
// ===========================================================================

TEST(XpeErrorString, OkReturnsSuccess) {
    EXPECT_STREQ(xpe_error_string(XPE_OK), "Success");
}

TEST(XpeErrorString, InvalidInputReturnsExpectedString) {
    EXPECT_STREQ(xpe_error_string(XPE_ERR_INVALID_INPUT), "Invalid input parameter");
}

TEST(XpeErrorString, OutOfMemoryMapped) {
    const char* s = xpe_error_string(XPE_ERR_OUT_OF_MEMORY);
    ASSERT_NE(s, nullptr);
    EXPECT_GT(std::strlen(s), 0u);
}

TEST(XpeErrorString, AllDefinedCodesNonNull) {
    XpeErrorCode codes[] = {
        XPE_OK,
        XPE_STATUS_NO_EVENT,
        XPE_ERR_INVALID_INPUT,
        XPE_ERR_OUT_OF_MEMORY,
        XPE_ERR_PROCESSING_FAILED,
        XPE_ERR_CONFIG_INVALID,
        XPE_ERR_CALIBRATION_EXPIRED,
        XPE_ERR_NOT_INITIALIZED,
        XPE_ERR_UNSUPPORTED_FORMAT,
        XPE_ERR_BUFFER_TOO_SMALL,
        XPE_ERR_IO_FAILED,
        XPE_ERR_NETWORK_FAILED
    };
    for (XpeErrorCode c : codes) {
        const char* s = xpe_error_string(c);
        EXPECT_NE(s, nullptr) << "Error string is NULL for code " << c;
        EXPECT_GT(std::strlen(s), 0u) << "Error string is empty for code " << c;
    }
}

TEST(XpeErrorString, UnknownCodeReturnsUnknown) {
    EXPECT_STREQ(xpe_error_string(-9999), "Unknown error");
}

TEST(XpeErrorString, StatusNoEventMapped) {
    EXPECT_STREQ(xpe_error_string(XPE_STATUS_NO_EVENT), "No pending event");
}

// ===========================================================================
// 6. ALERT QUEUE TESTS
// ===========================================================================

TEST_F(XpeCommonFixture, FreshInitHasNoAlerts) {
    EXPECT_EQ(xpe_get_pending_alert_count(), 0);
}

TEST_F(XpeCommonFixture, InjectOneAlertCount) {
    xpe_test_inject_alert("test alert", XPE_ALERT_WARNING);
    EXPECT_EQ(xpe_get_pending_alert_count(), 1);
}

TEST_F(XpeCommonFixture, InjectMultipleAlertsCounted) {
    xpe_test_inject_alert("a1", XPE_ALERT_INFO);
    xpe_test_inject_alert("a2", XPE_ALERT_WARNING);
    xpe_test_inject_alert("a3", XPE_ALERT_ERROR);
    EXPECT_EQ(xpe_get_pending_alert_count(), 3);
}

TEST_F(XpeCommonFixture, GetAlertNullMsgFails) {
    xpe_test_inject_alert("x", XPE_ALERT_INFO);
    int32_t sev = 0;
    EXPECT_EQ(xpe_get_pending_alert(0, nullptr, 128, &sev), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, GetAlertNullSeverityFails) {
    xpe_test_inject_alert("x", XPE_ALERT_INFO);
    char buf[128];
    EXPECT_EQ(xpe_get_pending_alert(0, buf, sizeof(buf), nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, GetAlertBufferTooSmall) {
    xpe_test_inject_alert("hello", XPE_ALERT_INFO);
    char buf[2];  // too small for "hello"
    int32_t sev = 0;
    EXPECT_EQ(xpe_get_pending_alert(0, buf, sizeof(buf), &sev), XPE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(XpeCommonFixture, GetAlertReturnsCorrectMessage) {
    xpe_test_inject_alert("calibration-warning", XPE_ALERT_WARNING);
    char buf[256];
    int32_t sev = 0;
    ASSERT_EQ(xpe_get_pending_alert(0, buf, sizeof(buf), &sev), XPE_OK);
    EXPECT_STREQ(buf, "calibration-warning");
    EXPECT_EQ(sev, static_cast<int32_t>(XPE_ALERT_WARNING));
}

TEST_F(XpeCommonFixture, GetAlertOutOfRangeFails) {
    char buf[128];
    int32_t sev = 0;
    EXPECT_EQ(xpe_get_pending_alert(99, buf, sizeof(buf), &sev), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, ClearAlertsEmptiesQueue) {
    xpe_test_inject_alert("a", XPE_ALERT_INFO);
    xpe_test_inject_alert("b", XPE_ALERT_INFO);
    xpe_clear_alerts();
    EXPECT_EQ(xpe_get_pending_alert_count(), 0);
}

TEST_F(XpeCommonFixture, GetAlertDoesNotConsume) {
    // xpe_get_pending_alert reads by index but does not pop
    xpe_test_inject_alert("persistent", XPE_ALERT_INFO);
    char buf[128]; int32_t sev = 0;
    ASSERT_EQ(xpe_get_pending_alert(0, buf, sizeof(buf), &sev), XPE_OK);
    ASSERT_EQ(xpe_get_pending_alert(0, buf, sizeof(buf), &sev), XPE_OK);
    EXPECT_EQ(xpe_get_pending_alert_count(), 1);  // still there
}

// ===========================================================================
// 7. LOGGING TESTS
// ===========================================================================

TEST_F(XpeCommonFixture, SetLogLevelValidRange) {
    for (int32_t lvl = 0; lvl <= 5; ++lvl) {
        EXPECT_EQ(xpe_log_set_level(lvl), XPE_OK) << "level=" << lvl;
    }
    // Restore to INFO
    EXPECT_EQ(xpe_log_set_level(2), XPE_OK);
}

TEST_F(XpeCommonFixture, SetLogLevelNegativeFails) {
    EXPECT_EQ(xpe_log_set_level(-1), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, SetLogLevelAbove5Fails) {
    EXPECT_EQ(xpe_log_set_level(6), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, SetLogFileNullRevertsToStderr) {
    EXPECT_EQ(xpe_log_set_file(nullptr), XPE_OK);
}

TEST_F(XpeCommonFixture, SetLogFileValidPathSucceeds) {
    const char* tmpPath = "xpe_unit_test_log.txt";
    XpeErrorCode rc = xpe_log_set_file(tmpPath);
    EXPECT_EQ(rc, XPE_OK);
    xpe_log_flush();
    xpe_log_set_file(nullptr);  // revert
    std::remove(tmpPath);
}

TEST_F(XpeCommonFixture, SetLogFileInvalidPathFails) {
    EXPECT_EQ(xpe_log_set_file("/nonexistent/path/that/cannot/be/created/log.txt"),
              XPE_ERR_IO_FAILED);
}

TEST_F(XpeCommonFixture, LogFlushDoesNotCrash) {
    xpe_log_flush();  // must not crash regardless of state
}

// ===========================================================================
// 8. AED SUBSYSTEM TESTS
// ===========================================================================

TEST_F(XpeCommonFixture, AedGetStatusBeforeConfigureReturnsIdle) {
    // After init, AED is not yet configured -- state should be IDLE (0)
    int32_t state = -1;
    ASSERT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 0);  // IDLE
}

TEST_F(XpeCommonFixture, AedConfigureNullAcceptsDefaults) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);
}

TEST_F(XpeCommonFixture, AedConfigureValidJsonReturnsOk) {
    const char* json = R"({
        "aed": {
            "trigger_threshold_adu": 200,
            "settle_time_ms": 50,
            "min_exposure_ms": 10,
            "max_exposure_ms": 3000
        }
    })";
    EXPECT_EQ(xpe_aed_configure(json), XPE_OK);
}

TEST_F(XpeCommonFixture, AedConfigureInvalidJsonFails) {
    EXPECT_EQ(xpe_aed_configure("not-json-at-all"), XPE_ERR_CONFIG_INVALID);
}

TEST_F(XpeCommonFixture, AedConfigureArmsSubsystem) {
    ASSERT_EQ(xpe_aed_configure(nullptr), XPE_OK);
    int32_t state = -1;
    ASSERT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 1);  // ARMED
}

TEST_F(XpeCommonFixture, AedGetStatusNullFails) {
    EXPECT_EQ(xpe_aed_get_status(nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, AedPollEventNullPtrsFail) {
    ASSERT_EQ(xpe_aed_configure(nullptr), XPE_OK);
    uint64_t ts; float sig;
    EXPECT_EQ(xpe_aed_poll_event(nullptr, &ts, &sig), XPE_ERR_INVALID_INPUT);

    int32_t et; float sig2;
    EXPECT_EQ(xpe_aed_poll_event(&et, nullptr, &sig2), XPE_ERR_INVALID_INPUT);

    int32_t et2; uint64_t ts2;
    EXPECT_EQ(xpe_aed_poll_event(&et2, &ts2, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonFixture, AedPollEventEmptyQueueReturnsNoEvent) {
    ASSERT_EQ(xpe_aed_configure(nullptr), XPE_OK);
    int32_t et = -1; uint64_t ts = 0; float sig = 0.0f;
    EXPECT_EQ(xpe_aed_poll_event(&et, &ts, &sig), XPE_STATUS_NO_EVENT);
}

TEST_F(XpeCommonFixture, AedPollEventConsumesInjectedEvent) {
    ASSERT_EQ(xpe_aed_configure(nullptr), XPE_OK);
    xpe_test_inject_aed_event(1 /*exposure_end*/, 0.85f);

    int32_t et = -1; uint64_t ts = 0; float sig = 0.0f;
    ASSERT_EQ(xpe_aed_poll_event(&et, &ts, &sig), XPE_OK);
    EXPECT_EQ(et, 1);
    EXPECT_FLOAT_EQ(sig, 0.85f);
    EXPECT_GT(ts, 0u);
}

TEST_F(XpeCommonFixture, AedPollEventTriggeredStateTransition) {
    ASSERT_EQ(xpe_aed_configure(nullptr), XPE_OK);
    xpe_test_inject_aed_event(2 /*exposure_trigger*/, 1.0f);

    int32_t state = -1;
    ASSERT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 2);  // TRIGGERED

    int32_t et; uint64_t ts; float sig;
    ASSERT_EQ(xpe_aed_poll_event(&et, &ts, &sig), XPE_OK);

    // After consuming, should return to ARMED
    ASSERT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 1);  // ARMED
}

TEST_F(XpeCommonFixture, AedPollMultipleEvents) {
    ASSERT_EQ(xpe_aed_configure(nullptr), XPE_OK);
    xpe_test_inject_aed_event(0, 0.3f);
    xpe_test_inject_aed_event(1, 0.7f);

    int32_t et; uint64_t ts; float sig;
    ASSERT_EQ(xpe_aed_poll_event(&et, &ts, &sig), XPE_OK);
    EXPECT_EQ(et, 0);
    ASSERT_EQ(xpe_aed_poll_event(&et, &ts, &sig), XPE_OK);
    EXPECT_EQ(et, 1);
    EXPECT_EQ(xpe_aed_poll_event(&et, &ts, &sig), XPE_STATUS_NO_EVENT);
}

TEST_F(XpeCommonFixture, AedNotInitializedAfterShutdown) {
    xpe_shutdown();
    int32_t state = -1;
    EXPECT_EQ(xpe_aed_get_status(&state), XPE_ERR_NOT_INITIALIZED);
    // Re-initialize for TearDown
    xpe_init(nullptr);
}

// ===========================================================================
// 9. IMAGE MEMORY TESTS
// ===========================================================================

class XpeImageFixture : public ::testing::Test {
protected:
    XpeImageBuffer src{};
    XpeImageBuffer dst{};

    void SetUp() override {
        ASSERT_EQ(xpe_init(nullptr), XPE_OK);
        ASSERT_EQ(xpe_alloc_image(8, 8, XPE_PIXEL_UINT16, &src), XPE_OK);
        ASSERT_EQ(xpe_alloc_image(8, 8, XPE_PIXEL_UINT16, &dst), XPE_OK);

        auto* px = static_cast<uint16_t*>(src.data);
        for (std::size_t i = 0; i < 64; ++i) px[i] = static_cast<uint16_t>(i);
    }
    void TearDown() override {
        xpe_free_image(&src);
        xpe_free_image(&dst);
        xpe_shutdown();
    }
};

TEST_F(XpeImageFixture, AllocUint16HasBackingBuffer) {
    EXPECT_NE(src.data, nullptr);
}

TEST_F(XpeImageFixture, AllocUint16DataSizeCorrect) {
    // 8 * 8 * 2 = 128
    EXPECT_EQ(src.dataSize, 128u);
}

TEST_F(XpeImageFixture, AllocFloat32DataSizeCorrect) {
    XpeImageBuffer f32{};
    ASSERT_EQ(xpe_alloc_image(4, 4, XPE_PIXEL_FLOAT32, &f32), XPE_OK);
    EXPECT_EQ(f32.dataSize, 64u);  // 4*4*4
    xpe_free_image(&f32);
}

TEST_F(XpeImageFixture, AllocZeroWidthFails) {
    XpeImageBuffer buf{};
    EXPECT_EQ(xpe_alloc_image(0, 8, XPE_PIXEL_UINT16, &buf), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, AllocZeroHeightFails) {
    XpeImageBuffer buf{};
    EXPECT_EQ(xpe_alloc_image(8, 0, XPE_PIXEL_UINT16, &buf), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, AllocNullOutFails) {
    EXPECT_EQ(xpe_alloc_image(8, 8, XPE_PIXEL_UINT16, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, AllocExceedsMaxDimensionFails) {
    XpeImageBuffer buf{};
    EXPECT_EQ(xpe_alloc_image(4097, 4097, XPE_PIXEL_UINT16, &buf), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, FreeNullFails) {
    EXPECT_EQ(xpe_free_image(nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, FreeNullsDataPointer) {
    XpeImageBuffer buf{};
    ASSERT_EQ(xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, &buf), XPE_OK);
    ASSERT_EQ(xpe_free_image(&buf), XPE_OK);
    EXPECT_EQ(buf.data, nullptr);
    EXPECT_EQ(buf.dataSize, 0u);
}

TEST_F(XpeImageFixture, DoubleFreeSafeNoOp) {
    XpeImageBuffer buf{};
    ASSERT_EQ(xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, &buf), XPE_OK);
    ASSERT_EQ(xpe_free_image(&buf), XPE_OK);
    // Second free: buf.data is nullptr -- should return OK (no-op per spec)
    EXPECT_EQ(xpe_free_image(&buf), XPE_OK);
}

TEST_F(XpeImageFixture, CopyPreservesPixelData) {
    ASSERT_EQ(xpe_copy_image(&src, &dst), XPE_OK);
    EXPECT_EQ(std::memcmp(src.data, dst.data, src.dataSize), 0);
}

TEST_F(XpeImageFixture, CopyPreservesDimensions) {
    ASSERT_EQ(xpe_copy_image(&src, &dst), XPE_OK);
    EXPECT_EQ(dst.width, src.width);
    EXPECT_EQ(dst.height, src.height);
    EXPECT_EQ(dst.format, src.format);
}

TEST_F(XpeImageFixture, CopyNullSrcFails) {
    EXPECT_EQ(xpe_copy_image(nullptr, &dst), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, CopyNullDstFails) {
    EXPECT_EQ(xpe_copy_image(&src, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeImageFixture, CopyDstTooSmallFails) {
    XpeImageBuffer small{};
    ASSERT_EQ(xpe_alloc_image(2, 2, XPE_PIXEL_UINT16, &small), XPE_OK);
    EXPECT_EQ(xpe_copy_image(&src, &small), XPE_ERR_BUFFER_TOO_SMALL);
    xpe_free_image(&small);
}

// ===========================================================================
// 10. P/INVOKE STRUCT LAYOUT TESTS (static assertions + runtime verification)
// ===========================================================================

TEST(XpeStructLayout, ImageBufferSizeOnX64) {
    // width(4) + height(4) + bitsAllocated(4) + bitsStored(4)
    // + format(4) + pad(4) + data(8) + dataSize(8) = 40 bytes
    // Verify actual size matches expected P/Invoke layout
    static_assert(sizeof(XpeImageBuffer) >= 36,
                  "XpeImageBuffer is smaller than expected for P/Invoke");
    // Runtime check
    EXPECT_GE(sizeof(XpeImageBuffer), 36u);
}

TEST(XpeStructLayout, ImageMetadataSizeOnX64) {
    // bodyPart(64) + kVp(4) + mAs(4) + SID_mm(4) + pixelPitch_mm(4)
    // + acquisitionTime(8) + flags(4) + pad(4) = 96 bytes
    static_assert(sizeof(XpeImageMetadata) >= 88,
                  "XpeImageMetadata is smaller than expected for P/Invoke");
    EXPECT_GE(sizeof(XpeImageMetadata), 88u);
}

TEST(XpeStructLayout, XpeImageBufferDataOffsetAligned) {
    // 'data' field (void*) should be 8-byte aligned within the struct
    std::size_t offset = offsetof(XpeImageBuffer, data);
    EXPECT_EQ(offset % 8, 0u) << "XpeImageBuffer::data is not 8-byte aligned";
}

TEST(XpeStructLayout, XpeImageMetadataAcquisitionTimeAligned) {
    std::size_t offset = offsetof(XpeImageMetadata, acquisitionTime);
    EXPECT_EQ(offset % 8, 0u) << "XpeImageMetadata::acquisitionTime is not 8-byte aligned";
}

// ===========================================================================
// 11. THREAD SAFETY SMOKE TEST
// ===========================================================================

TEST(XpeThreadSafety, ConcurrentAlertInjectAndRead) {
    ASSERT_EQ(xpe_init(nullptr), XPE_OK);

    std::thread writer([] {
        for (int i = 0; i < 20; ++i) {
            xpe_test_inject_alert("concurrent-alert", XPE_ALERT_INFO);
        }
    });

    std::thread reader([] {
        for (int i = 0; i < 20; ++i) {
            (void)xpe_get_pending_alert_count();
        }
    });

    writer.join();
    reader.join();

    xpe_clear_alerts();
    xpe_shutdown();
}

// ===========================================================================
// main() is provided by GTest::gtest_main
// ===========================================================================
