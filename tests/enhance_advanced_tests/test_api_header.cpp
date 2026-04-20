/**
 * @file test_api_header.cpp
 * @brief API header compilation and P/Invoke ABI compliance tests
 *
 * Verifies that:
 *   - Public headers compile without errors
 *   - XpeImageBuffer/XpeImageMetadata struct layout matches C# expectations
 *   - All 4 API function signatures are declared with extern "C" linkage
 *   - Struct sizes match P/Invoke Pack=8 layout
 *
 * AC-PIN-001: C# Struct Marshaling
 * REQ-ADV-002: P/Invoke ABI Compliance
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// ============================================================================
// Header Compilation Tests
// ============================================================================

TEST(ApiHeaderTest, HeadersCompileWithoutError) {
    // If this test compiles, all headers are valid
    SUCCEED();
}

TEST(ApiHeaderTest, XpeImageBufferDefaultConstruction) {
    XpeImageBuffer img{};
    EXPECT_EQ(img.width, 0u);
    EXPECT_EQ(img.height, 0u);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    EXPECT_EQ(img.data, nullptr);
    EXPECT_EQ(img.dataSize, static_cast<size_t>(0));
}

TEST(ApiHeaderTest, XpeImageMetadataDefaultConstruction) {
    XpeImageMetadata meta{};
    EXPECT_EQ(meta.kVp, 0.0f);
    EXPECT_EQ(meta.mAs, 0.0f);
    EXPECT_EQ(meta.flags, 0u);
}

// ============================================================================
// P/Invoke Struct Size Verification (AC-PIN-001)
// ============================================================================

TEST(ApiHeaderTest, XpeImageBufferSizeIs40Bytes) {
    // Pack=8 layout: 5 scalar fields (20) + 4 padding + data(8) + dataSize(8) = 40
    EXPECT_EQ(sizeof(XpeImageBuffer), static_cast<size_t>(40));
}

TEST(ApiHeaderTest, XpeImageMetadataSizeIs96Bytes) {
    // Pack=8 layout: bodyPart(64) + 4 floats (16) + acquisitionTime(8) + flags(4) + tail padding = 96
    EXPECT_EQ(sizeof(XpeImageMetadata), static_cast<size_t>(96));
}

TEST(ApiHeaderTest, XpeImageBufferFieldOffsets) {
    EXPECT_EQ(offsetof(XpeImageBuffer, width), static_cast<size_t>(0));
    EXPECT_EQ(offsetof(XpeImageBuffer, height), static_cast<size_t>(4));
    EXPECT_EQ(offsetof(XpeImageBuffer, data), static_cast<size_t>(24));
}

TEST(ApiHeaderTest, XpeImageMetadataFieldOffsets) {
    EXPECT_EQ(offsetof(XpeImageMetadata, bodyPart), static_cast<size_t>(0));
    EXPECT_EQ(offsetof(XpeImageMetadata, acquisitionTime), static_cast<size_t>(80));
}

TEST(ApiHeaderTest, XpePixelFormatSizeIs4Bytes) {
    EXPECT_EQ(sizeof(XpePixelFormat), static_cast<size_t>(4));
}

// ============================================================================
// Function Pointer Type Verification (compile-time)
// ============================================================================

TEST(ApiHeaderTest, AllFunctionSignaturesDeclared) {
    // These typedefs will fail to compile if the functions are not declared
    using FnInit = XpeErrorCode (*)(const char*);
    using FnShutdown = void (*)(void);
    using FnVersion = const char* (*)(void);
    using FnMultiscaleProcess = XpeErrorCode (*)(XpeImageBuffer*, const XpeImageMetadata*, const char*);
    using FnFractionalProcess = XpeErrorCode (*)(XpeImageBuffer*, float, const char*);
    using FnDetectCollimation = XpeErrorCode (*)(const XpeImageBuffer*, int32_t*, int32_t*, int32_t*, int32_t*, const char*);
    using FnCalcExposureIndex = XpeErrorCode (*)(const XpeImageBuffer*, const XpeImageMetadata*, float*, float*);

    // Verify function pointer types are usable
    FnInit pInit = &xpe_enhance_advanced_init;
    FnShutdown pShutdown = &xpe_enhance_advanced_shutdown;
    FnVersion pVersion = &xpe_enhance_advanced_version;
    FnMultiscaleProcess pMfp = &xpe_multiscale_process;
    FnFractionalProcess pFrac = &xpe_fractional_process;
    FnDetectCollimation pCol = &xpe_detect_collimation;
    FnCalcExposureIndex pEI = &xpe_calc_exposure_index;

    // Suppress unused warnings
    (void)pInit;
    (void)pShutdown;
    (void)pVersion;
    (void)pMfp;
    (void)pFrac;
    (void)pCol;
    (void)pEI;

    SUCCEED();
}

// ============================================================================
// Error Code Constants Verification
// ============================================================================

TEST(ApiHeaderTest, ErrorCodeConstantsDefined) {
    EXPECT_EQ(XPE_OK, 0);
    EXPECT_LT(XPE_ERR_INVALID_INPUT, 0);
    EXPECT_LT(XPE_ERR_NOT_INITIALIZED, 0);
    EXPECT_LT(XPE_ERR_UNSUPPORTED_FORMAT, 0);
    EXPECT_LT(XPE_ERR_SAFETY_VIOLATION, 0);
    EXPECT_LT(XPE_ERR_CONFIG_INVALID, 0);
    EXPECT_LT(XPE_ERR_INTERNAL, 0);
}

// ============================================================================
// Pixel Format Enum Verification
// ============================================================================

TEST(ApiHeaderTest, PixelFormatValues) {
    EXPECT_EQ(XPE_PIXEL_UINT16, 0);
    EXPECT_EQ(XPE_PIXEL_FLOAT32, 1);
}

// ============================================================================
// Processing Flag Constants Verification
// ============================================================================

TEST(ApiHeaderTest, ProcessingFlagsDefined) {
    EXPECT_NE(XPE_FLAG_GHOST_CORRECTED, 0u);
    EXPECT_NE(XPE_FLAG_GAIN_CORRECTED, 0u);
    EXPECT_NE(XPE_FLAG_COLLIMATION_DETECTED, 0u);
}
