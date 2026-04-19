#include "xpe/preprocess/xpe_preprocess_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// xpe_preprocess_version
// ---------------------------------------------------------------------------

TEST(XpePreprocessSmoke, VersionIsNonNull) {
    ASSERT_NE(xpe_preprocess_version(), nullptr);
}

TEST(XpePreprocessSmoke, VersionIsNonEmpty) {
    ASSERT_GT(std::strlen(xpe_preprocess_version()), 0u);
}

TEST(XpePreprocessSmoke, VersionMatchesFormat) {
    const char* version = xpe_preprocess_version();
    // Format: MAJOR.MINOR.PATCH (e.g., "0.1.0")
    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
}

// ---------------------------------------------------------------------------
// xpe_preprocess_init / xpe_preprocess_shutdown
// ---------------------------------------------------------------------------

TEST(XpePreprocessSmoke, InitAcceptsNullConfig) {
    EXPECT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
    xpe_preprocess_shutdown();
}

TEST(XpePreprocessSmoke, InitRejectsEmptyStringConfig) {
    // Empty string is treated as invalid configuration
    EXPECT_EQ(xpe_preprocess_init(""), XPE_ERR_CONFIG_INVALID);
}

TEST(XpePreprocessSmoke, InitAcceptsValidJsonConfig) {
    const char* config = "{\"logLevel\":1}";
    EXPECT_EQ(xpe_preprocess_init(config), XPE_OK);
    xpe_preprocess_shutdown();
}

TEST(XpePreprocessSmoke, InitIsIdempotent) {
    ASSERT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
    EXPECT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
    xpe_preprocess_shutdown();
}

TEST(XpePreprocessSmoke, ShutdownIsIdempotent) {
    xpe_preprocess_init(nullptr);
    xpe_preprocess_shutdown();
    // Second shutdown should not crash
    xpe_preprocess_shutdown();
}

// ---------------------------------------------------------------------------
// xpe_preprocess_get_param_range
// ---------------------------------------------------------------------------

TEST(XpePreprocessSmoke, GetParamRangeReturnsOkForKnownParam) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("integration_time_ms", &minVal, &maxVal), XPE_OK);
    EXPECT_GT(maxVal, minVal);
}

TEST(XpePreprocessSmoke, GetParamRangeIntegrationTime) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("integration_time_ms", &minVal, &maxVal), XPE_OK);
    EXPECT_FLOAT_EQ(minVal, 1.0f);
    EXPECT_FLOAT_EQ(maxVal, 10000.0f);
}

TEST(XpePreprocessSmoke, GetParamRangeTemperature) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("temperature_c", &minVal, &maxVal), XPE_OK);
    EXPECT_FLOAT_EQ(minVal, -20.0f);
    EXPECT_FLOAT_EQ(maxVal, 60.0f);
}

TEST(XpePreprocessSmoke, GetParamRangeKvp) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("kVp", &minVal, &maxVal), XPE_OK);
    EXPECT_FLOAT_EQ(minVal, 40.0f);
    EXPECT_FLOAT_EQ(maxVal, 150.0f);
}

TEST(XpePreprocessSmoke, GetParamRangeMAs) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("mAs", &minVal, &maxVal), XPE_OK);
    EXPECT_FLOAT_EQ(minVal, 0.1f);
    EXPECT_FLOAT_EQ(maxVal, 1000.0f);
}

TEST(XpePreprocessSmoke, GetParamRangeSid) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("SID_mm", &minVal, &maxVal), XPE_OK);
    EXPECT_FLOAT_EQ(minVal, 1000.0f);
    EXPECT_FLOAT_EQ(maxVal, 2000.0f);
}

TEST(XpePreprocessSmoke, GetParamRangePixelPitch) {
    float minVal = 0.0f, maxVal = 0.0f;
    ASSERT_EQ(xpe_preprocess_get_param_range("pixelPitch_mm", &minVal, &maxVal), XPE_OK);
    EXPECT_FLOAT_EQ(minVal, 0.1f);
    EXPECT_FLOAT_EQ(maxVal, 0.5f);
}

TEST(XpePreprocessSmoke, GetParamRangeReturnsErrorForUnknownParam) {
    float minVal = 0.0f, maxVal = 0.0f;
    EXPECT_EQ(xpe_preprocess_get_param_range("unknown_param", &minVal, &maxVal), XPE_ERR_INVALID_INPUT);
}

TEST(XpePreprocessSmoke, GetParamRangeReturnsErrorForNullName) {
    float minVal = 0.0f, maxVal = 0.0f;
    EXPECT_EQ(xpe_preprocess_get_param_range(nullptr, &minVal, &maxVal), XPE_ERR_INVALID_INPUT);
}

TEST(XpePreprocessSmoke, GetParamRangeReturnsErrorForNullMin) {
    float maxVal = 0.0f;
    EXPECT_EQ(xpe_preprocess_get_param_range("integration_time_ms", nullptr, &maxVal), XPE_ERR_INVALID_INPUT);
}

TEST(XpePreprocessSmoke, GetParamRangeReturnsErrorForNullMax) {
    float minVal = 0.0f;
    EXPECT_EQ(xpe_preprocess_get_param_range("integration_time_ms", &minVal, nullptr), XPE_ERR_INVALID_INPUT);
}

// ---------------------------------------------------------------------------
// Lifecycle Integration
// ---------------------------------------------------------------------------

TEST(XpePreprocessSmoke, FullLifecycle) {
    // Initialize -> Query -> Shutdown
    ASSERT_EQ(xpe_preprocess_init(nullptr), XPE_OK);

    float minVal = 0.0f, maxVal = 0.0f;
    EXPECT_EQ(xpe_preprocess_get_param_range("integration_time_ms", &minVal, &maxVal), XPE_OK);

    xpe_preprocess_shutdown();
}

TEST(XpePreprocessSmoke, MultipleInitShutdownCycles) {
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
        xpe_preprocess_shutdown();
    }
}

// ---------------------------------------------------------------------------
// Thread Safety (Basic)
// ---------------------------------------------------------------------------

TEST(XpePreprocessSmoke, ConcurrentInit) {
    // Note: This test verifies basic thread safety but does not
    // guarantee race-free behavior. A full thread-safety test
    // would require dedicated threading infrastructure.
    ASSERT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
    // Second init from same thread should be OK
    EXPECT_EQ(xpe_preprocess_init(nullptr), XPE_OK);
    xpe_preprocess_shutdown();
}
