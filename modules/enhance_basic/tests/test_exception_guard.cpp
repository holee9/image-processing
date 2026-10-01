/**
 * @file test_exception_guard.cpp
 * @brief No exception leaves xpe_noise_reduce / xpe_contrast_enhance / xpe_edge_enhance (QA-B-181, QA-B-179, #233).
 *
 * QA-B-179 measured inputs that made these three exported functions throw out of the C ABI with no memory
 * pressure at all:
 *   - xpe_noise_reduce (NLM): a declared 2^20 x 2^20 image with dataSize 0 (accepted: "unspecified") -> bad_alloc
 *   - xpe_noise_reduce (bilateral) and xpe_edge_enhance: width 0x80000000 became a negative int -> length_error
 *   - xpe_contrast_enhance: tile_width 0x40000000 overflowed `tile_width * 2` (undefined behaviour) -> length_error
 * Each is now an error code, the image descriptor and pixels are untouched, and the module keeps working.
 *
 * The guard itself is xpe_guarded_call (enhance_basic_internal.h); it is header-inline, so these tests run it
 * directly with each exception kind. The exported functions are wired to it, which the size-driven allocation
 * failures below exercise end to end. Those need a request larger than the machine's commit limit; where the
 * machine could in principle satisfy it the test skips rather than allocate.
 */
#include <gtest/gtest.h>

#include <windows.h>

#include <cstdint>
#include <new>
#include <stdexcept>
#include <vector>

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

namespace {

XpeImageBuffer Declared(uint32_t w, uint32_t h, float* data, size_t bytes) {
    XpeImageBuffer b{};
    b.width = w;
    b.height = h;
    b.bitsAllocated = 32;
    b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.data = data;
    b.dataSize = bytes;
    return b;
}

bool SameDescriptor(const XpeImageBuffer& a, const XpeImageBuffer& b) {
    return a.width == b.width && a.height == b.height && a.data == b.data && a.dataSize == b.dataSize &&
           a.format == b.format && a.bitsAllocated == b.bitsAllocated && a.bitsStored == b.bitsStored;
}

/** True when `bytes` is more than twice what the machine could ever commit: the allocation cannot succeed. */
bool CannotBeCommitted(double bytes) {
    MEMORYSTATUSEX m{};
    m.dwLength = sizeof(m);
    GlobalMemoryStatusEx(&m);
    return bytes > 2.0 * static_cast<double>(m.ullTotalPageFile);
}

template <class F>
bool Escaped(F fn, XpeErrorCode* rc) {
    try {
        *rc = fn();
        return false;
    } catch (...) {
        return true;
    }
}

}  // namespace

// ---- the guard ---------------------------------------------------------------------------------------------

TEST(ExceptionGuard, BadAllocBecomesOutOfMemory) {
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, xpe_guarded_call([]() -> XpeErrorCode { throw std::bad_alloc(); }));
}

TEST(ExceptionGuard, LengthErrorAndAnyStdExceptionBecomeProcessingFailed) {
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_guarded_call([]() -> XpeErrorCode { throw std::length_error("x"); }));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_guarded_call([]() -> XpeErrorCode { throw std::runtime_error("x"); }));
}

TEST(ExceptionGuard, ANonStdExceptionBecomesProcessingFailed) {
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_guarded_call([]() -> XpeErrorCode { throw 42; }));
}

TEST(ExceptionGuard, ANormalReturnPassesThroughUnchanged) {
    EXPECT_EQ(XPE_OK, xpe_guarded_call([]() -> XpeErrorCode { return XPE_OK; }));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_guarded_call([]() -> XpeErrorCode { return XPE_ERR_INVALID_INPUT; }));
}

// ---- the three exported functions, with the inputs QA-B-179 measured ---------------------------------------

TEST(ExceptionGuard, NoiseReduceNlmWithAnImpossibleDeclaredSizeIsOutOfMemoryNotAnException) {
    const double bytes = 4.0 * static_cast<double>(1u << 20) * static_cast<double>(1u << 20);   // output(n) floats
    if (!CannotBeCommitted(bytes)) GTEST_SKIP() << "this machine's commit limit could satisfy a " << bytes << " byte request";
    float one = 1.0f;
    XpeImageBuffer img = Declared(1u << 20, 1u << 20, &one, 0);   // dataSize 0 = unspecified: accepted
    const XpeImageBuffer before = img;
    XpeNoiseReduceParams p{};
    p.mode = XPE_NOISE_NLM;
    p.search_window = 3;
    p.patch_size = 3;
    p.h_param = 1.0f;
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_noise_reduce(&img, &p); }, &rc)) << "an exception left xpe_noise_reduce";
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    EXPECT_TRUE(SameDescriptor(before, img));
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, NoiseReduceBilateralWithAWidthAboveIntMaxIsInvalidInput) {
    float one = 1.0f;
    XpeImageBuffer img = Declared(0x80000000u, 1, &one, 0);
    XpeNoiseReduceParams p{};
    p.mode = XPE_NOISE_BILATERAL;
    p.sigma_space = 3.0f;
    p.sigma_range = 10.0f;
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_noise_reduce(&img, &p); }, &rc)) << "an exception left xpe_noise_reduce";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, EdgeEnhanceWithAWidthAboveIntMaxIsInvalidInput) {
    float one = 1.0f;
    XpeImageBuffer img = Declared(0x80000000u, 1, &one, 0);
    XpeUsmParams p{0.5f, 2.0f, 10.0f};
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_edge_enhance(&img, &p); }, &rc)) << "an exception left xpe_edge_enhance";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, EdgeEnhanceWithAnImpossibleRowBufferIsOutOfMemoryNotAnException) {
    // The widest accepted image: the blur's row ring needs ksize * width floats (ksize <= 41 at radius 10).
    const double bytes = 41.0 * 4.0 * 2147483647.0;
    if (!CannotBeCommitted(bytes)) GTEST_SKIP() << "this machine's commit limit could satisfy a " << bytes << " byte request";
    float one = 1.0f;
    XpeImageBuffer img = Declared(0x7FFFFFFFu, 1, &one, 0);
    const XpeImageBuffer before = img;
    XpeUsmParams p{0.5f, 10.0f, 10.0f};
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_edge_enhance(&img, &p); }, &rc)) << "an exception left xpe_edge_enhance";
    EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc);
    EXPECT_TRUE(SameDescriptor(before, img));
    EXPECT_EQ(1.0f, one);
}

TEST(ExceptionGuard, ContrastEnhanceWithATileCountThatOverflowsIntIsInvalidInput) {
    std::vector<float> px(64);
    for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<float>(i % 17);
    const std::vector<float> original = px;
    XpeImageBuffer img = Declared(8, 8, px.data(), px.size() * sizeof(float));
    XpeClaheParams p{3.0f, 0x40000000, 2};   // tile_width * 2 wrapped to INT_MIN in int and passed the window check
    XpeErrorCode rc = XPE_OK;
    EXPECT_FALSE(Escaped([&] { return xpe_contrast_enhance(&img, &p); }, &rc)) << "an exception left xpe_contrast_enhance";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(original, px);
}

// The window check still admits exactly the grids that fit: the boundary stays valid, one past it is rejected.
TEST(ExceptionGuard, ContrastEnhanceKeepsItsWindowBoundary) {
    std::vector<float> px(64);
    for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<float>(i % 17);
    XpeImageBuffer img = Declared(8, 8, px.data(), px.size() * sizeof(float));
    XpeClaheParams fits{3.0f, 4, 4};        // 4 * 2 == 8: the largest grid an 8 x 8 image allows
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, &fits));
    XpeClaheParams tooBig{3.0f, 5, 4};      // 5 * 2 > 8
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &tooBig));
}

// The size rule is module-wide (it sits in validate_float32_image), so a function that never allocated is
// consistent with the rest: a dimension above INT32_MAX is invalid input, not an index that wraps negative.
TEST(ExceptionGuard, EveryEntryPointRejectsADimensionAboveIntMax) {
    float one = 1.0f;
    XpeImageBuffer wide = Declared(0x80000000u, 1, &one, 0);
    XpeImageBuffer tall = Declared(1, 0x80000000u, &one, 0);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, validate_float32_image(&wide));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, validate_float32_image(&tall));
    XpeImageBuffer widest = Declared(0x7FFFFFFFu, 1, &one, 0);
    EXPECT_EQ(XPE_OK, validate_float32_image(&widest)) << "INT32_MAX itself stays valid";
}
