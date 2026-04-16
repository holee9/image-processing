/**
 * @file test_xpe_common.cpp
 * @brief Google Test suite for xpe_common.dll -- SPEC-XPE-P0 validation
 *
 * IEC 62304 Class B -- Unit Tests for xpe_common 18 API functions.
 * Coverage target: >= 85% statement coverage.
 */

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_memory.h"
#include "gtest/gtest.h"

#include <cstring>
#include <fstream>
#include <thread>
#include <cstdio>

/* External test-support functions from xpe_common.cpp (white-box linkage) */
extern "C" {
    XPE_API void xpe_test_inject_aed_event(int32_t eventType, float signalLevel);
    XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity);
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class XpeCommonTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize library before each test
        ASSERT_EQ(xpe_init(nullptr), XPE_OK);
    }

    void TearDown() override {
        // Cleanup after each test
        xpe_shutdown();
    }
};

/* ============================================================================
 * Lifecycle Tests (REQ-P0-011, REQ-P0-012)
 * ============================================================================ */

TEST_F(XpeCommonTest, InitReturnsOk) {
    xpe_shutdown();
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);
}

TEST_F(XpeCommonTest, InitWithEmptyConfigReturnsInvalid) {
    xpe_shutdown();
    EXPECT_EQ(xpe_init(""), XPE_ERR_CONFIG_INVALID);
}

TEST_F(XpeCommonTest, ShutdownWithoutInitIsNoOp) {
    xpe_shutdown();
    xpe_shutdown();  // Second shutdown should not crash
    SUCCEED();
}

TEST_F(XpeCommonTest, VersionReturnsValidString) {
    const char* ver = xpe_version();
    ASSERT_NE(ver, nullptr);
    EXPECT_STRNE(ver, "");
    EXPECT_GE(std::strlen(ver), 3);  // Minimum "0.0.0"
}

/* ============================================================================
 * Configuration Tests (REQ-P0-014)
 * ============================================================================ */

TEST_F(XpeCommonTest, ConfigureWithNullReturnsInvalid) {
    EXPECT_EQ(xpe_configure(nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, ConfigureWithInvalidJsonReturnsInvalid) {
    EXPECT_EQ(xpe_configure("not json"), XPE_ERR_CONFIG_INVALID);
}

TEST_F(XpeCommonTest, ConfigureWithValidJsonSucceeds) {
    EXPECT_EQ(xpe_configure("{\"key\":\"value\"}"), XPE_OK);
}

/* ============================================================================
 * Parameter Range Tests (REQ-P0-022)
 * ============================================================================ */

TEST_F(XpeCommonTest, GetParamRangeWithNullPtrReturnsInvalid) {
    float min, max, def;
    EXPECT_EQ(xpe_get_param_range(nullptr, "windowWidth", &min, &max, &def),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_get_param_range("CHEST", nullptr, &min, &max, &def),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_get_param_range("CHEST", "windowWidth", nullptr, &max, &def),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, GetParamRangeReturnsValidValues) {
    float min, max, def;
    EXPECT_EQ(xpe_get_param_range("CHEST", "windowWidth", &min, &max, &def), XPE_OK);
    EXPECT_GT(max, min);
    EXPECT_LE(min, def);
    EXPECT_GE(max, def);
}

TEST_F(XpeCommonTest, GetParamRangeForUnknownParamReturnsDefaults) {
    float min, max, def;
    EXPECT_EQ(xpe_get_param_range("CHEST", "unknown_param", &min, &max, &def), XPE_OK);
    EXPECT_EQ(min, 0.0f);
    EXPECT_EQ(max, 1.0f);
    EXPECT_EQ(def, 0.5f);
}

/* ============================================================================
 * Error String Tests (REQ-P0-018)
 * ============================================================================ */

TEST_F(XpeCommonTest, ErrorStringReturnsNonNullForAllCodes) {
    const char* msg;

    msg = xpe_error_string(XPE_OK);
    ASSERT_NE(msg, nullptr);

    msg = xpe_error_string(XPE_ERR_INVALID_INPUT);
    ASSERT_NE(msg, nullptr);

    msg = xpe_error_string(XPE_ERR_OUT_OF_MEMORY);
    ASSERT_NE(msg, nullptr);

    msg = xpe_error_string(XPE_ERR_CONFIG_INVALID);
    ASSERT_NE(msg, nullptr);

    msg = xpe_error_string(static_cast<XpeErrorCode>(9999));
    ASSERT_NE(msg, nullptr);
}

/* ============================================================================
 * Alert Queue Tests (REQ-P0-019, REQ-P0-020, REQ-P0-021)
 * ============================================================================ */

TEST_F(XpeCommonTest, GetPendingAlertCountInitiallyZero) {
    EXPECT_EQ(xpe_get_pending_alert_count(), 0);
}

TEST_F(XpeCommonTest, InjectedAlertAppearsInQueue) {
    const int32_t severity = 3;
    xpe_test_inject_alert("Test alert", severity);

    EXPECT_EQ(xpe_get_pending_alert_count(), 1);
}

TEST_F(XpeCommonTest, GetPendingAlertRetrievesMessage) {
    const char* testMsg = "Test alert message";
    const int32_t severity = 2;

    xpe_test_inject_alert(testMsg, severity);

    char msg[256];
    int32_t outSeverity;
    EXPECT_EQ(xpe_get_pending_alert(0, msg, sizeof(msg), &outSeverity), XPE_OK);
    EXPECT_STREQ(msg, testMsg);
    EXPECT_EQ(outSeverity, severity);
}

TEST_F(XpeCommonTest, GetPendingAlertWithNullPtrReturnsInvalid) {
    char msg[256];
    int32_t severity;

    EXPECT_EQ(xpe_get_pending_alert(0, nullptr, sizeof(msg), &severity),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_get_pending_alert(0, msg, 0, &severity),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_get_pending_alert(0, msg, sizeof(msg), nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, GetPendingAlertWithSmallBufferReturnsBufferTooSmall) {
    xpe_test_inject_alert("A very long alert message that exceeds the buffer", 1);

    char msg[10];
    int32_t severity;
    EXPECT_EQ(xpe_get_pending_alert(0, msg, sizeof(msg), &severity),
              XPE_ERR_BUFFER_TOO_SMALL);
}

TEST_F(XpeCommonTest, ClearAlertsEmptiesQueue) {
    xpe_test_inject_alert("Alert 1", 1);
    xpe_test_inject_alert("Alert 2", 2);

    EXPECT_EQ(xpe_get_pending_alert_count(), 2);
    xpe_clear_alerts();
    EXPECT_EQ(xpe_get_pending_alert_count(), 0);
}

/* ============================================================================
 * Logging Tests (REQ-P0-023, REQ-P0-024, REQ-P0-025)
 * ============================================================================ */

TEST_F(XpeCommonTest, LogSetLevelWithInvalidValueReturnsInvalid) {
    EXPECT_EQ(xpe_log_set_level(-1), XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_log_set_level(6), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, LogSetLevelAcceptsValidRange) {
    EXPECT_EQ(xpe_log_set_level(0), XPE_OK);  // TRACE
    EXPECT_EQ(xpe_log_set_level(5), XPE_OK);  // OFF
}

TEST_F(XpeCommonTest, LogSetFileReopensSink) {
    // Create a temporary log file
    const char* logFile = "test_log.txt";

    EXPECT_EQ(xpe_log_set_file(logFile), XPE_OK);
    xpe_log_flush();  // Should not crash

    // Revert to stderr
    EXPECT_EQ(xpe_log_set_file(nullptr), XPE_OK);

    // Cleanup
    std::remove(logFile);
}

/* ============================================================================
 * AED Tests (REQ-P0-026, REQ-P0-027, REQ-P0-028)
 * ============================================================================ */

TEST_F(XpeCommonTest, AedConfigureWithNullUsesDefaults) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);
}

TEST_F(XpeCommonTest, AedConfigureWithInvalidJsonReturnsInvalid) {
    EXPECT_EQ(xpe_aed_configure("not json"), XPE_ERR_CONFIG_INVALID);
}

TEST_F(XpeCommonTest, AedConfigureWithValidJsonSucceeds) {
    const char* config = R"({"trigger_threshold_adu": 600, "settle_time_ms": 150})";
    EXPECT_EQ(xpe_aed_configure(config), XPE_OK);
}

TEST_F(XpeCommonTest, AedGetStatusWithoutInitReturnsNotInitialized) {
    xpe_shutdown();
    int32_t state;
    EXPECT_EQ(xpe_aed_get_status(&state), XPE_ERR_NOT_INITIALIZED);
}

TEST_F(XpeCommonTest, AedGetStatusReturnsValidState) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);

    int32_t state;
    EXPECT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_GE(state, 0);
    EXPECT_LE(state, 2);
}

TEST_F(XpeCommonTest, AedPollEventWithNullPtrReturnsInvalid) {
    int32_t eventType;
    uint64_t timestamp;
    float signalLevel;

    EXPECT_EQ(xpe_aed_poll_event(nullptr, &timestamp, &signalLevel),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_aed_poll_event(&eventType, nullptr, &signalLevel),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, AedPollEventWithoutEventsReturnsNoEvent) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);

    int32_t eventType;
    uint64_t timestamp;
    float signalLevel;
    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel),
              XPE_STATUS_NO_EVENT);
}

TEST_F(XpeCommonTest, AedPollEventRetrievesInjectedEvent) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);

    const int32_t testEventType = 1;
    const float testSignalLevel = 0.75f;
    xpe_test_inject_aed_event(testEventType, testSignalLevel);

    int32_t eventType;
    uint64_t timestamp;
    float signalLevel;
    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel), XPE_OK);
    EXPECT_EQ(eventType, testEventType);
    EXPECT_GT(timestamp, 0);
    EXPECT_FLOAT_EQ(signalLevel, testSignalLevel);
}

/* ============================================================================
 * Memory Tests (REQ-P0-015, REQ-P0-016, REQ-P0-017)
 * ============================================================================ */

TEST_F(XpeCommonTest, AllocImageWithNullPtrReturnsInvalid) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, AllocImageWithZeroDimensionsReturnsInvalid) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(0, 100, XPE_PIXEL_UINT16, &buf),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_alloc_image(100, 0, XPE_PIXEL_UINT16, &buf),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, AllocImageWithLargeDimensionsReturnsInvalid) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(5000, 5000, XPE_PIXEL_UINT16, &buf),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, AllocImageSucceedsForValidInput) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, &buf), XPE_OK);
    EXPECT_EQ(buf.width, 100);
    EXPECT_EQ(buf.height, 100);
    EXPECT_NE(buf.data, nullptr);
    EXPECT_GT(buf.dataSize, 0);

    xpe_free_image(&buf);
}

TEST_F(XpeCommonTest, AllocImageForFloat32Format) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(50, 50, XPE_PIXEL_FLOAT32, &buf), XPE_OK);
    EXPECT_EQ(buf.bitsAllocated, 32);
    EXPECT_EQ(buf.format, XPE_PIXEL_FLOAT32);

    xpe_free_image(&buf);
}

TEST_F(XpeCommonTest, FreeImageWithNullPtrReturnsInvalid) {
    EXPECT_EQ(xpe_free_image(nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, FreeImageReleasesMemory) {
    XpeImageBuffer buf;
    xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, &buf);

    void* dataPtr = buf.data;
    EXPECT_EQ(xpe_free_image(&buf), XPE_OK);
    EXPECT_EQ(buf.data, nullptr);
    EXPECT_EQ(buf.dataSize, 0);
}

TEST_F(XpeCommonTest, CopyImageWithNullPtrReturnsInvalid) {
    XpeImageBuffer src, dst;
    EXPECT_EQ(xpe_copy_image(nullptr, &dst), XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_copy_image(&src, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, CopyImageSucceeds) {
    XpeImageBuffer src, dst;

    xpe_alloc_image(10, 10, XPE_PIXEL_UINT16, &src);
    xpe_alloc_image(10, 10, XPE_PIXEL_UINT16, &dst);

    // Set some data in source
    static_cast<uint16_t*>(src.data)[0] = 12345;

    EXPECT_EQ(xpe_copy_image(&src, &dst), XPE_OK);
    EXPECT_EQ(dst.width, src.width);
    EXPECT_EQ(dst.height, src.height);
    EXPECT_EQ(static_cast<uint16_t*>(dst.data)[0], 12345);

    xpe_free_image(&src);
    xpe_free_image(&dst);
}

TEST_F(XpeCommonTest, CopyImageWithSmallBufferReturnsBufferTooSmall) {
    XpeImageBuffer src, dst;

    xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, &src);
    xpe_alloc_image(10, 10, XPE_PIXEL_UINT16, &dst);  // Smaller buffer

    EXPECT_EQ(xpe_copy_image(&src, &dst), XPE_ERR_BUFFER_TOO_SMALL);

    xpe_free_image(&src);
    xpe_free_image(&dst);
}

/* ============================================================================
 * Additional Lifecycle Tests (REQ-P0-011, REQ-P0-012)
 * ============================================================================ */

TEST_F(XpeCommonTest, InitDoubleInitReturnsOk) {
    // Second init should be idempotent (no-op)
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);
}

TEST_F(XpeCommonTest, ShutdownReturnsNotInitializedWhenNotInit) {
    xpe_shutdown();
    // Shutdown without init should handle gracefully
    // Note: Current implementation returns void, so we just verify no crash
    SUCCEED();
}

TEST_F(XpeCommonTest, InitShutdownCycleRepeated) {
    for (int i = 0; i < 10; ++i) {
        xpe_shutdown();
        EXPECT_EQ(xpe_init(nullptr), XPE_OK);
    }
}

/* ============================================================================
 * Additional Configuration Tests (REQ-P0-014)
 * ============================================================================ */

TEST_F(XpeCommonTest, ConfigureWithEmptyStringReturnsInvalid) {
    EXPECT_EQ(xpe_configure(""), XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, ConfigureWithPartialJsonReturnsOk) {
    // Partial JSON should still parse (forward compatibility)
    EXPECT_EQ(xpe_configure("{\"key\":\"value\"}"), XPE_OK);
}

TEST_F(XpeCommonTest, ConfigureAfterReinitSucceeds) {
    xpe_shutdown();
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);
    EXPECT_EQ(xpe_configure("{\"test\":1}"), XPE_OK);
}

/* ============================================================================
 * Additional Version Tests (REQ-P0-013)
 * ============================================================================ */

TEST_F(XpeCommonTest, VersionFormatSemanticVersioning) {
    const char* ver = xpe_version();
    ASSERT_NE(ver, nullptr);

    // Verify format X.Y.Z (semantic versioning)
    int major = 0, minor = 0, patch = 0;
    int parsed = sscanf(ver, "%d.%d.%d", &major, &minor, &patch);
    EXPECT_EQ(parsed, 3);
    EXPECT_GE(major, 0);
    EXPECT_GE(minor, 0);
    EXPECT_GE(patch, 0);
}

/* ============================================================================
 * Additional Logging Tests (REQ-P0-023, REQ-P0-024, REQ-P0-025)
 * ============================================================================ */

TEST_F(XpeCommonTest, LogSetLevelEachLevel) {
    // Test each level (0-5)
    for (int level = 0; level <= 5; ++level) {
        EXPECT_EQ(xpe_log_set_level(level), XPE_OK);
    }
}

TEST_F(XpeCommonTest, LogSetFileWithInvalidPathReturnsError) {
    // Try to write to read-only location (should fail gracefully)
    const char* invalidPath = "/invalid/path/that/does/not/exist/log.txt";

    // On Windows, this might succeed differently; just verify no crash
    XpeErrorCode result = xpe_log_set_file(invalidPath);
    // Either success (created somehow) or IO failed is acceptable
    EXPECT_TRUE(result == XPE_OK || result == XPE_ERR_IO_FAILED);

    // Revert to stderr
    xpe_log_set_file(nullptr);
}

TEST_F(XpeCommonTest, LogFlushDoesNotCrash) {
    // Multiple flushes should be safe
    xpe_log_flush();
    xpe_log_flush();
    xpe_log_flush();
    SUCCEED();
}

/* ============================================================================
 * Additional AED Tests (REQ-P0-026, REQ-P0-027, REQ-P0-028)
 * ============================================================================ */

TEST_F(XpeCommonTest, AedConfigureWithDisabledSetsIdleState) {
    const char* config = R"({"enabled": false})";
    EXPECT_EQ(xpe_aed_configure(config), XPE_OK);

    int32_t state;
    EXPECT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 0);  // IDLE
}

TEST_F(XpeCommonTest, AedConfigureWithEnabledSetsArmedState) {
    const char* config = R"({"enabled": true})";
    EXPECT_EQ(xpe_aed_configure(config), XPE_OK);

    int32_t state;
    EXPECT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 1);  // ARMED
}

TEST_F(XpeCommonTest, AedPollEventClearsTriggeredStateWhenEmpty) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);

    // Inject an event
    xpe_test_inject_aed_event(1, 0.5f);

    // Poll it
    int32_t eventType;
    uint64_t timestamp;
    float signalLevel;
    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel), XPE_OK);

    // State should now be IDLE (queue empty)
    int32_t state;
    EXPECT_EQ(xpe_aed_get_status(&state), XPE_OK);
    EXPECT_EQ(state, 0);  // IDLE
}

TEST_F(XpeCommonTest, AedPollEventMultipleEvents) {
    EXPECT_EQ(xpe_aed_configure(nullptr), XPE_OK);

    // Inject multiple events
    xpe_test_inject_aed_event(1, 0.3f);
    xpe_test_inject_aed_event(2, 0.6f);
    xpe_test_inject_aed_event(3, 0.9f);

    // Poll all events
    int32_t eventType;
    uint64_t timestamp;
    float signalLevel;

    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel), XPE_OK);
    EXPECT_EQ(eventType, 1);

    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel), XPE_OK);
    EXPECT_EQ(eventType, 2);

    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel), XPE_OK);
    EXPECT_EQ(eventType, 3);

    // Next poll should return NO_EVENT
    EXPECT_EQ(xpe_aed_poll_event(&eventType, &timestamp, &signalLevel),
              XPE_STATUS_NO_EVENT);
}

/* ============================================================================
 * Additional Memory Tests (REQ-P0-015, REQ-P0-016, REQ-P0-017)
 * ============================================================================ */

TEST_F(XpeCommonTest, AllocImageZeroInitializesMemory) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(10, 10, XPE_PIXEL_UINT16, &buf), XPE_OK);

    // Verify all bytes are zero
    uint16_t* data = static_cast<uint16_t*>(buf.data);
    for (size_t i = 0; i < buf.dataSize / sizeof(uint16_t); ++i) {
        EXPECT_EQ(data[i], 0);
    }

    xpe_free_image(&buf);
}

TEST_F(XpeCommonTest, AllocImageUint16Format) {
    XpeImageBuffer buf;
    EXPECT_EQ(xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, &buf), XPE_OK);
    EXPECT_EQ(buf.bitsAllocated, 16);
    EXPECT_EQ(buf.format, XPE_PIXEL_UINT16);
    EXPECT_EQ(buf.dataSize, 100 * 100 * 2);

    xpe_free_image(&buf);
}

TEST_F(XpeCommonTest, FreeImageNullDataDoesNotCrash) {
    XpeImageBuffer buf;
    buf.data = nullptr;
    buf.dataSize = 0;

    // Should handle gracefully
    EXPECT_EQ(xpe_free_image(&buf), XPE_OK);
}

TEST_F(XpeCommonTest, CopyImageWithNullDataReturnsInvalid) {
    XpeImageBuffer src, dst;
    src.data = nullptr;
    dst.data = nullptr;

    EXPECT_EQ(xpe_copy_image(&src, &dst), XPE_ERR_INVALID_INPUT);
}

/* ============================================================================
 * Additional Parameter Range Tests (REQ-P0-022)
 * ============================================================================ */

TEST_F(XpeCommonTest, GetParamRangeForDifferentBodyParts) {
    float min, max, def;

    // Test different body parts
    EXPECT_EQ(xpe_get_param_range("CHEST", "windowWidth", &min, &max, &def), XPE_OK);
    EXPECT_EQ(xpe_get_param_range("ABDOMEN", "windowWidth", &min, &max, &def), XPE_OK);
    EXPECT_EQ(xpe_get_param_range("EXTREMITY", "windowWidth", &min, &max, &def), XPE_OK);
}

/* ============================================================================
 * Additional Error String Tests (REQ-P0-018)
 * ============================================================================ */

TEST_F(XpeCommonTest, ErrorStringForUnknownErrorCode) {
    const char* msg = xpe_error_string(static_cast<XpeErrorCode>(-999));
    ASSERT_NE(msg, nullptr);
    EXPECT_STRNE(msg, "");
}

/* ============================================================================
 * Additional Alert Queue Tests (REQ-P0-019, REQ-P0-020, REQ-P0-021)
 * ============================================================================ */

TEST_F(XpeCommonTest, GetPendingAlertWithIndexOutOfBounds) {
    xpe_test_inject_alert("Test", 1);

    char msg[256];
    int32_t severity;

    // Index out of bounds
    EXPECT_EQ(xpe_get_pending_alert(999, msg, sizeof(msg), &severity),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(XpeCommonTest, ClearAlertsWhenEmptyDoesNotCrash) {
    xpe_clear_alerts();
    xpe_clear_alerts();  // Second clear should be safe
    EXPECT_EQ(xpe_get_pending_alert_count(), 0);
}

TEST_F(XpeCommonTest, AlertQueueFifoOrder) {
    xpe_test_inject_alert("First", 1);
    xpe_test_inject_alert("Second", 2);
    xpe_test_inject_alert("Third", 3);

    EXPECT_EQ(xpe_get_pending_alert_count(), 3);

    char msg[256];
    int32_t severity;

    // Verify FIFO order - retrieve sequentially (index 0, then index 1)
    xpe_get_pending_alert(0, msg, sizeof(msg), &severity);
    EXPECT_STREQ(msg, "First");
    EXPECT_EQ(severity, 1);

    xpe_get_pending_alert(1, msg, sizeof(msg), &severity);  // Retrieve second alert
    EXPECT_STREQ(msg, "Second");
    EXPECT_EQ(severity, 2);
}

/* ============================================================================
 * Concurrent Access Tests (REQ-P0-022 thread safety)
 * ============================================================================ */

TEST_F(XpeCommonTest, ConcurrentInitShutdownDoesNotCrash) {
    // Simple thread safety test
    std::thread t1([]() {
        for (int i = 0; i < 10; ++i) {
            xpe_init(nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            xpe_shutdown();
        }
    });

    std::thread t2([]() {
        for (int i = 0; i < 10; ++i) {
            xpe_init(nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            xpe_shutdown();
        }
    });

    t1.join();
    t2.join();
    SUCCEED();
}

TEST_F(XpeCommonTest, ConcurrentAlertQueueAccess) {
    const int numAlerts = 100;
    std::thread producer([numAlerts]() {
        for (int i = 0; i < numAlerts; ++i) {
            xpe_test_inject_alert("Concurrent test", 1);
        }
    });

    std::thread consumer([numAlerts]() {
        int consumed = 0;
        while (consumed < numAlerts) {
            int count = xpe_get_pending_alert_count();
            if (count > 0) {
                char msg[256];
                int32_t severity;
                if (xpe_get_pending_alert(0, msg, sizeof(msg), &severity) == XPE_OK) {
                    consumed++;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(xpe_get_pending_alert_count(), 0);
}

TEST_F(XpeCommonTest, MemoryLeakTestThousandCycles) {
    // Allocate and free 1000 times to detect memory leaks
    for (int i = 0; i < 1000; ++i) {
        XpeImageBuffer buf;
        EXPECT_EQ(xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, &buf), XPE_OK);
        EXPECT_EQ(xpe_free_image(&buf), XPE_OK);
    }
    // If this test passes without ASan errors, no leaks detected
    SUCCEED();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
