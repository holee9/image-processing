/**
 * @file test_lifecycle.cpp
 * @brief Module lifecycle tests: init/shutdown/version, not-initialized guard
 *
 * Verifies:
 *   - AC-LC-001: Initialization with default config
 *   - AC-LC-002: Processing before init returns XPE_ERR_NOT_INITIALIZED
 *   - AC-LC-003: Shutdown after init
 *   - REQ-ADV-001: Module initialization
 *   - REQ-ADV-020: Not-initialized guard
 */

#include <gtest/gtest.h>

#include <cstring>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// ============================================================================
// REQ-ADV-001: Module Initialization
// ============================================================================

TEST(LifecycleTest, InitWithNullConfigReturnsOK) {
    // REQ-ADV-001: NULL config uses defaults
    XpeErrorCode err = xpe_enhance_advanced_init(nullptr);
    EXPECT_EQ(err, XPE_OK);
    xpe_enhance_advanced_shutdown();
}

TEST(LifecycleTest, InitWithValidJsonReturnsOK) {
    const char* config = "{\"log_level\":\"info\"}";
    XpeErrorCode err = xpe_enhance_advanced_init(config);
    EXPECT_EQ(err, XPE_OK);
    xpe_enhance_advanced_shutdown();
}

TEST(LifecycleTest, InitWithEmptyStringReturnsConfigInvalid) {
    // Empty string (not NULL) is invalid per implementation
    XpeErrorCode err = xpe_enhance_advanced_init("");
    EXPECT_EQ(err, XPE_ERR_CONFIG_INVALID);
}

TEST(LifecycleTest, InitWithMalformedJsonReturnsConfigInvalid) {
    XpeErrorCode err = xpe_enhance_advanced_init("{invalid json");
    EXPECT_EQ(err, XPE_ERR_CONFIG_INVALID);
}

TEST(LifecycleTest, DoubleInitIdempotent) {
    // Second init should succeed (idempotent)
    XpeErrorCode err1 = xpe_enhance_advanced_init(nullptr);
    ASSERT_EQ(err1, XPE_OK);

    XpeErrorCode err2 = xpe_enhance_advanced_init(nullptr);
    EXPECT_EQ(err2, XPE_OK);

    xpe_enhance_advanced_shutdown();
}

// ============================================================================
// Double-Shutdown Safety
// ============================================================================

TEST(LifecycleTest, DoubleShutdownDoesNotCrash) {
    xpe_enhance_advanced_init(nullptr);
    xpe_enhance_advanced_shutdown();

    // Second shutdown must be a no-op (no crash)
    EXPECT_NO_FATAL_FAILURE(xpe_enhance_advanced_shutdown());
}

TEST(LifecycleTest, ShutdownWithoutInitDoesNotCrash) {
    // Shutdown without init should be safe
    EXPECT_NO_FATAL_FAILURE(xpe_enhance_advanced_shutdown());
}

// ============================================================================
// Version Query
// ============================================================================

TEST(LifecycleTest, VersionReturnsNonNull) {
    const char* version = xpe_enhance_advanced_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
}

TEST(LifecycleTest, VersionMatchesExpectedFormat) {
    const char* version = xpe_enhance_advanced_version();
    ASSERT_NE(version, nullptr);
    // Version must be X.Y.Z format
    EXPECT_STREQ(version, "1.0.0");
}

TEST(LifecycleTest, VersionStringIsStable) {
    // Repeated calls must return the same pointer and content
    const char* v1 = xpe_enhance_advanced_version();
    const char* v2 = xpe_enhance_advanced_version();
    EXPECT_EQ(v1, v2) << "Version string address must be stable";
    EXPECT_STREQ(v1, v2);
}

// ============================================================================
// REQ-ADV-020: Not-Initialized Guard (AC-LC-002)
// ============================================================================

class NotInitializedGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure module is NOT initialized
        xpe_enhance_advanced_shutdown();
    }

    void TearDown() override {
        // Restore initialized state for subsequent tests
        xpe_enhance_advanced_init(nullptr);
    }
};

TEST_F(NotInitializedGuardTest, MultiscaleProcessReturnsNotInitialized) {
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    XpeErrorCode err = xpe_multiscale_process(&img, &meta, nullptr);
    EXPECT_EQ(err, XPE_ERR_NOT_INITIALIZED);
}

TEST_F(NotInitializedGuardTest, FractionalProcessReturnsNotInitialized) {
    XpeImageBuffer img{};
    XpeErrorCode err = xpe_fractional_process(&img, 1.0f, nullptr);
    EXPECT_EQ(err, XPE_ERR_NOT_INITIALIZED);
}

TEST_F(NotInitializedGuardTest, DetectCollimationReturnsNotInitialized) {
    XpeImageBuffer img{};
    int32_t x0, y0, x1, y1;
    XpeErrorCode err = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);
    EXPECT_EQ(err, XPE_ERR_NOT_INITIALIZED);
}

TEST_F(NotInitializedGuardTest, CalcExposureIndexReturnsNotInitialized) {
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    float ei = 0.0f, di = 0.0f;
    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    EXPECT_EQ(err, XPE_ERR_NOT_INITIALIZED);
}

// ============================================================================
// Init-Process-Shutdown Cycle
// ============================================================================

TEST(LifecycleTest, InitProcessShutdownCycle) {
    // Full cycle: init -> process -> shutdown
    XpeErrorCode err = xpe_enhance_advanced_init(nullptr);
    ASSERT_EQ(err, XPE_OK);

    // Create minimal valid image
    const uint32_t W = 32, H = 32;
    float data[W * H];
    for (size_t i = 0; i < W * H; ++i) data[i] = 500.0f;

    XpeImageBuffer img{};
    img.width = W;
    img.height = H;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.data = data;
    img.dataSize = W * H * sizeof(float);

    XpeImageMetadata meta{};
    std::memset(&meta, 0, sizeof(meta));
    strncpy_s(meta.bodyPart, sizeof(meta.bodyPart), "CHEST", _TRUNCATE);

    // Verify processing works
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);

    // Shutdown
    xpe_enhance_advanced_shutdown();

    // After shutdown, processing should fail
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_ERR_NOT_INITIALIZED);
}
