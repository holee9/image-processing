/**
 * @file test_a246_stage_diagnostics.cpp
 * @brief QA-A-246 (#245): xpe_preprocess_pipeline_out_ex reports per-stage non-finite counts.
 *
 * Why the counts exist: the defect stage replaces the pixels it corrects with finite values, so a gain stage that produced NaN or
 * infinity in a pixel the defect stage then fills leaves no trace in the final image. The caller that judges only the output cannot
 * tell the gain stage failed. pipeline_out_ex counts the gain stage's own output and the final frame.
 *
 * The current gain stage cannot produce a non-finite value from finite input (every bad gain is classified defective and replaced by
 * 1.0), so the tests that need non-finite values poison the gain stage's output through a test-only seam (xpe_pipeline_gain_frame_hook).
 * That seam exists only in builds that define XPE_CACHE_TEST_HOOKS (xpe_preprocess_clock_tests); in xpe_preprocess_tests, which links the
 * shipped DLL, the tests that do not need the seam still run.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "xcal_writer.hpp"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/preprocess_api.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 64, kH = 48;
constexpr size_t kN = static_cast<size_t>(kW) * kH;
constexpr const char* kCfg = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
constexpr const char* kCfgNoDefect = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}";
constexpr const char* kCfgUint16 = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGain\":true,\"bypassDefect\":true}";
const size_t kDefectPixels[] = {100, 500, 1200};   // the defect map marks these; the poison tests aim at them

XpeImageBuffer bufferOf(void* data, size_t bytes, XpePixelFormat fmt) {
    XpeImageBuffer b{};
    b.data = data;
    b.dataSize = bytes;
    b.width = kW;
    b.height = kH;
    b.format = fmt;
    b.bitsAllocated = fmt == XPE_PIXEL_FLOAT32 ? 32u : 16u;
    b.bitsStored = b.bitsAllocated;
    return b;
}

class A246Diagnostics : public ::testing::Test {
protected:
    fs::path dir_;
    std::vector<uint16_t> raw_;

    void SetUp() override {
        dir_ = fs::temp_directory_path() / "xpe_a246_diag";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        write("offset.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, std::vector<float>(kN, 100.0f).data(), kN * sizeof(float));
        write("gain.xcal", XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, std::vector<float>(kN, 1.0f).data(), kN * sizeof(float));
        std::vector<uint8_t> map(kN, 0);
        for (size_t i : kDefectPixels) map[i] = 1;
        write("defect.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, map.data(), kN);
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset((dir_ / "offset.xcal").string().c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain((dir_ / "gain.xcal").string().c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map((dir_ / "defect.xcal").string().c_str()));
        xpe_clear_alerts();
        raw_.resize(kN);
        for (size_t i = 0; i < kN; ++i) raw_[i] = static_cast<uint16_t>(1000 + i % 50);
    }
    void TearDown() override {
#ifdef XPE_CACHE_TEST_HOOKS
        xpe_pipeline_gain_frame_hook = nullptr;
#endif
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    void write(const char* name, XCalType type, XCalPixelFormat fmt, const void* data, size_t bytes) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, "a246", 5);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(fmt);
        hdr.width = kW;
        hdr.height = kH;
        hdr.payload_len = bytes;
        ASSERT_EQ(XPE_OK, write_xcal_file((dir_ / name).string().c_str(), hdr, nullptr, 0, static_cast<const uint8_t*>(data), bytes));
    }

    // runs pipeline_out (diag == nullptr and plain = true) or pipeline_out_ex into a float32 output; returns the result code
    XpeErrorCode run(const char* cfg, std::vector<float>* outPixels, XpeStageDiagnostics* diag, bool plain) {
        std::vector<uint16_t> in(raw_);
        outPixels->assign(kN, -1.0f);
        const XpeImageBuffer ib = bufferOf(in.data(), kN * 2, XPE_PIXEL_UINT16);
        XpeImageBuffer ob = bufferOf(outPixels->data(), kN * 4, XPE_PIXEL_FLOAT32);
        XpeImageMetadata meta{};
        return plain ? xpe_preprocess_pipeline_out(&ib, &ob, &meta, nullptr, nullptr, cfg)
                     : xpe_preprocess_pipeline_out_ex(&ib, &ob, &meta, nullptr, nullptr, cfg, diag);
    }
    static XpeStageDiagnostics freshDiag() {
        XpeStageDiagnostics d{};
        d.structSize = sizeof(XpeStageDiagnostics);
        return d;
    }
};

}  // namespace

TEST_F(A246Diagnostics, WithANullDiagPointerTheOutputIsByteIdenticalToPipelineOut) {
    std::vector<float> a, b;
    ASSERT_EQ(XPE_OK, run(kCfg, &a, nullptr, true));
    ASSERT_EQ(XPE_OK, run(kCfg, &b, nullptr, false));
    EXPECT_EQ(0, std::memcmp(a.data(), b.data(), kN * sizeof(float)));
}

TEST_F(A246Diagnostics, CountingDoesNotChangeTheOutputAndACleanFrameCountsZero) {
    std::vector<float> a, b;
    ASSERT_EQ(XPE_OK, run(kCfg, &a, nullptr, true));
    XpeStageDiagnostics d = freshDiag();
    ASSERT_EQ(XPE_OK, run(kCfg, &b, &d, false));
    EXPECT_EQ(0, std::memcmp(a.data(), b.data(), kN * sizeof(float))) << "the output with diagnostics is the output without";
    EXPECT_EQ(XPE_DIAG_GAIN_NONFINITE | XPE_DIAG_FINAL_NONFINITE, d.validMask);
    EXPECT_EQ(kN, d.pixelCount);
    EXPECT_EQ(0u, d.gainStageNonFinite);
    EXPECT_EQ(0u, d.finalNonFinite);
    EXPECT_EQ(sizeof(XpeStageDiagnostics), d.structSize) << "the caller's structSize is left as it was";
}

TEST_F(A246Diagnostics, ATooSmallStructIsRefusedAndNothingIsWritten) {
    std::vector<float> out;
    XpeStageDiagnostics d = freshDiag();
    d.structSize = 8;   // smaller than the struct this library knows
    d.finalNonFinite = 77;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(kCfg, &out, &d, false));
    EXPECT_EQ(-1.0f, out[0]) << "the output buffer was not written";
    EXPECT_EQ(77u, d.finalNonFinite) << "an unusable struct is not touched";
}

TEST_F(A246Diagnostics, AFailedCallLeavesValidMaskZeroAndTheCountsZero) {
    std::vector<uint16_t> in(raw_);
    XpeStageDiagnostics d = freshDiag();
    d.validMask = 0xFF;
    d.gainStageNonFinite = 5;
    d.finalNonFinite = 6;
    const XpeImageBuffer ib = bufferOf(in.data(), kN * 4, XPE_PIXEL_UINT16);
    XpeImageBuffer ob = ib;   // the same buffer: refused (SAFE-004)
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_pipeline_out_ex(&ib, &ob, &meta, nullptr, nullptr, kCfg, &d));
    EXPECT_EQ(0u, d.validMask);
    EXPECT_EQ(0u, d.pixelCount);
    EXPECT_EQ(0u, d.gainStageNonFinite);
    EXPECT_EQ(0u, d.finalNonFinite);
}

TEST_F(A246Diagnostics, AUint16FinalFrameCountsZeroForBothStages) {
    std::vector<uint16_t> in(raw_), out16(kN, 0);
    XpeStageDiagnostics d = freshDiag();
    const XpeImageBuffer ib = bufferOf(in.data(), kN * 2, XPE_PIXEL_UINT16);
    XpeImageBuffer ob = bufferOf(out16.data(), kN * 2, XPE_PIXEL_UINT16);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out_ex(&ib, &ob, &meta, nullptr, nullptr, kCfgUint16, &d));
    EXPECT_EQ(static_cast<uint32_t>(XPE_PIXEL_UINT16), static_cast<uint32_t>(ob.format));
    EXPECT_EQ(XPE_DIAG_GAIN_NONFINITE | XPE_DIAG_FINAL_NONFINITE, d.validMask) << "measured, and an integer frame has none";
    EXPECT_EQ(0u, d.gainStageNonFinite);
    EXPECT_EQ(0u, d.finalNonFinite);
}

#ifdef XPE_CACHE_TEST_HOOKS
namespace {
std::vector<std::pair<size_t, float>> g_poison;
void poisonGainOutput(void* data, size_t pixels, bool isFloat) {
    if (!isFloat) return;
    float* f = static_cast<float*>(data);
    for (const auto& p : g_poison)
        if (p.first < pixels) f[p.first] = p.second;
}
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();
}  // namespace

// What the card expected and the product does not do: the defect stage REFUSES a frame with a non-finite pixel (QA-A-214b: "THE FRAME
// MUST BE FINITE"; it does not fill NaN / infinity), and the pipeline passes that refusal on. So a non-finite value from the gain stage cannot
// be hidden by the defect stage: the call fails, loudly, with an alert, whether or not the pixel is in the defect map. The diagnostics are
// therefore not what catches the "defect fills what gain broke" case in the current pipeline; they report the counts when the defect stage
// does not run (bypassed) and are defence in depth if that refusal is ever relaxed.
TEST_F(A246Diagnostics, ANonFinitePixelFromTheGainStageStopsTheFrameAtTheDefectStageWhetherOrNotTheMapMarksIt) {
    const size_t cases[] = {kDefectPixels[0], 2000};   // a pixel the defect map marks, and one it does not
    for (size_t idx : cases) {
        SCOPED_TRACE(idx);
        g_poison = {{idx, kNaN}};
        xpe_pipeline_gain_frame_hook = &poisonGainOutput;
        xpe_clear_alerts();
        std::vector<float> out;
        XpeStageDiagnostics d = freshDiag();
        d.gainStageNonFinite = 9;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(kCfg, &out, &d, false)) << "the defect stage refuses a non-finite input";
        EXPECT_EQ(-1.0f, out[0]) << "the output buffer was not written";
        EXPECT_EQ(0u, d.validMask) << "a failed call reports no counts";
        EXPECT_EQ(0u, d.gainStageNonFinite);
        char msg[400];
        int32_t sev = 0;
        bool alerted = false;
        for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i)
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK && std::strstr(msg, "XPE_WARN_DEFECT_INPUT_NOT_FINITE")) alerted = true;
        EXPECT_TRUE(alerted) << "and says why";
        // the same call without a diagnostics struct fails the same way
        std::vector<float> out2;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(kCfg, &out2, nullptr, true));
    }
}

// With the defect stage bypassed nothing refuses or fills, and the counts are the only record: NaN, +inf and -inf, on the gain stage's
// output and on the final frame.
TEST_F(A246Diagnostics, WithTheDefectStageBypassedTheCountsSeeNaNAndBothInfinities) {
    g_poison = {{kDefectPixels[0], kNaN}, {kDefectPixels[1], kInf}, {2000, -kInf}};
    xpe_pipeline_gain_frame_hook = &poisonGainOutput;
    std::vector<float> out;
    XpeStageDiagnostics d = freshDiag();
    ASSERT_EQ(XPE_OK, run(kCfgNoDefect, &out, &d, false));
    EXPECT_EQ(XPE_DIAG_GAIN_NONFINITE | XPE_DIAG_FINAL_NONFINITE, d.validMask);
    EXPECT_EQ(3u, d.gainStageNonFinite);
    EXPECT_EQ(3u, d.finalNonFinite);
    EXPECT_TRUE(std::isnan(out[kDefectPixels[0]]));
    EXPECT_TRUE(std::isinf(out[kDefectPixels[1]]));
    EXPECT_TRUE(std::isinf(out[2000]));
}

TEST_F(A246Diagnostics, CountingDoesNotChangeAPoisonedOutputEither) {
    g_poison = {{kDefectPixels[0], kNaN}, {2000, kInf}};
    xpe_pipeline_gain_frame_hook = &poisonGainOutput;
    std::vector<float> a, b;
    ASSERT_EQ(XPE_OK, run(kCfgNoDefect, &a, nullptr, true));
    XpeStageDiagnostics d = freshDiag();
    ASSERT_EQ(XPE_OK, run(kCfgNoDefect, &b, &d, false));
    EXPECT_EQ(0, std::memcmp(a.data(), b.data(), kN * sizeof(float))) << "counting reads the frame; it never changes it";
    EXPECT_EQ(2u, d.gainStageNonFinite);
}
#endif
