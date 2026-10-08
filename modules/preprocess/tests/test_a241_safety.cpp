/**
 * @file test_a241_safety.cpp
 * @brief QA-A-241 (#245): the SRS safety rules the 1-stage checklist (B4) found missing.
 *   SRS-CALIB-SAFE-002 / FUNC-009  a map that expires while loaded stops the frame; expiry 0 is reported once (XPE_WARN_NO_EXPIRY)
 *   SRS-CALIB-SAFE-001             an explicit bypassOffset / bypassGain is allowed, flagged and logged
 *   SRS-CALIB-SAFE-004             xpe_preprocess_pipeline_out never writes the input buffer
 */
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
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

int64_t realNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

#ifdef XPE_CACHE_TEST_HOOKS
// QA-A-244: in the clock-test build (xpe_preprocess_clock_tests) every expiry decision reads this clock, which only the test moves. The
// tests that need time to pass used to set an expiry 400 ms ahead of the wall clock, load, run a frame and sleep 600 ms; on a slow CI
// runner the load and the first frame took longer than the window and the map had expired before its first frame. Now the test sets the
// time, loads, runs the frame, advances the clock past the expiry and runs the frame again: no wall-clock window, no race, no sleep.
int64_t g_testNowMs = 0;
int64_t testClock() { return g_testNowMs; }
int64_t nowMs() { return g_testNowMs; }
void advanceMs(int64_t ms) { g_testNowMs += ms; }
#else
int64_t nowMs() { return realNowMs(); }
#endif

int countAlerts(const char* needle) {
    int hits = 0;
    char msg[600];
    int32_t sev = 0;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i)
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK && std::strstr(msg, needle)) ++hits;
    return hits;
}

uint64_t digestOf(const void* p, size_t bytes) {
    uint64_t h = 1469598103934665603ull;
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < bytes; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}

class A241Safety : public ::testing::Test {
protected:
    fs::path dir_;
    const char* session_ = "a241";   // the session id the next written file carries

    void SetUp() override {
        dir_ = fs::temp_directory_path() / "xpe_a241_safety";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
#ifdef XPE_CACHE_TEST_HOOKS
        g_testNowMs = realNowMs();   // starts at the real time so file timestamps stay plausible; it moves only when a test advances it
        xpe_clock_now_ms_hook = &testClock;
#endif
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
    }
    void TearDown() override {
#ifdef XPE_CACHE_TEST_HOOKS
        xpe_clock_now_ms_hook = nullptr;
#endif
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    std::string write(const char* name, XCalType type, XCalPixelFormat fmt, const void* data, size_t bytes, int64_t expiryMs) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, session_, std::strlen(session_) + 1);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(fmt);
        hdr.width = kW;
        hdr.height = kH;
        hdr.payload_len = bytes;
        hdr.expiry_epoch_ms = expiryMs;
        const std::string path = (dir_ / name).string();
        EXPECT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, static_cast<const uint8_t*>(data), bytes));
        return path;
    }
    std::string offsetFile(int64_t expiryMs, const char* name = "offset.xcal") {
        const std::vector<float> v(kN, 100.0f);
        return write(name, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, v.data(), kN * sizeof(float), expiryMs);
    }
    std::string gainFile(int64_t expiryMs, const char* name = "gain.xcal") {
        const std::vector<float> v(kN, 1.0f);
        return write(name, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, v.data(), kN * sizeof(float), expiryMs);
    }
    std::string defectFile(int64_t expiryMs, const char* name = "defect.xcal") {
        std::vector<uint8_t> v(kN, 0);
        v[100] = 1;
        return write(name, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, v.data(), kN, expiryMs);
    }
    void loadAll(int64_t offsetExp, int64_t gainExp, int64_t defectExp) {
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetFile(offsetExp).c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainFile(gainExp).c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defectFile(defectExp).c_str()));
    }
    static void fillRaw(std::vector<uint16_t>& raw) {
        for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint16_t>(1000 + i % 50);
    }
    static XpeImageBuffer bufferOf(void* data, size_t bytes, XpePixelFormat fmt = XPE_PIXEL_UINT16) {
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
};

// ---------------------------------------------------------------- SAFE-002 / FUNC-009: expiry while loaded
#ifdef XPE_CACHE_TEST_HOOKS   // needs the injected clock (xpe_preprocess_clock_tests), see nowMs() above
TEST_F(A241Safety, AMapThatExpiresWhileLoadedStopsTheFrame) {
    const char* kinds[] = {"offset", "gain", "defect"};
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kinds[k]);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const int64_t soon = nowMs() + 400;
        loadAll(k == 0 ? soon : 0, k == 1 ? soon : 0, k == 2 ? soon : 0);

        std::vector<float> frame(kN);
        std::vector<uint16_t> raw(kN);
        fillRaw(raw);
        std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
        XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
        XpeImageMetadata meta{};
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg)) << "before it expires, the frame is corrected";

        advanceMs(600);
        xpe_clear_alerts();
        std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
        img = bufferOf(frame.data(), kN * sizeof(float));
        const uint64_t before = digestOf(frame.data(), kN * sizeof(float));
        XpeImageMetadata meta2{};
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_preprocess_pipeline_ex(&img, &meta2, nullptr, nullptr, kCfg));
        EXPECT_EQ(before, digestOf(frame.data(), kN * sizeof(float))) << "a refused frame leaves the caller's buffer as it was";
        EXPECT_EQ(0u, meta2.flags) << "no stage ran";
        EXPECT_EQ(1, countAlerts("XPE_ERR_CALIBRATION_EXPIRED:"));
        EXPECT_EQ(1, countAlerts(kinds[k])) << "the alert names the map that expired";
    }
}
#endif

#ifdef XPE_CACHE_TEST_HOOKS   // needs the injected clock (xpe_preprocess_clock_tests), see nowMs() above
TEST_F(A241Safety, ABypassedMapThatExpiredStillStopsTheFrame) {
    // SAFE-002 judges "any loaded calibration file", the bypassed one included.
    loadAll(nowMs() + 300, 0, 0);
    advanceMs(500);
    std::vector<float> frame(kN, 0.0f);
    XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED,
              xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr,
                                         "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassOffset\":true}"));
}
#endif

TEST_F(A241Safety, ANeverExpiringFileDoesNotStopTheFrameLater) {
    loadAll(0, 0, 0);
    std::vector<float> frame(kN);
    std::vector<uint16_t> raw(kN);
    fillRaw(raw);
    std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
    XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg));
}

TEST_F(A241Safety, ExpiryZeroIsReportedOncePerState) {
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetFile(0).c_str()));
    EXPECT_EQ(1, countAlerts("XPE_WARN_NO_EXPIRY:"));
    EXPECT_EQ(1, countAlerts("the offset calibration file"));
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetFile(0).c_str()));
    EXPECT_EQ(0, countAlerts("XPE_WARN_NO_EXPIRY:")) << "the same state is not reported again on every load";
    // a file that carries an expiry ends the state; the next never-expiring file is reported again
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetFile(nowMs() + 3600 * 1000).c_str()));
    EXPECT_EQ(0, countAlerts("XPE_WARN_NO_EXPIRY:"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetFile(0).c_str()));
    EXPECT_EQ(1, countAlerts("XPE_WARN_NO_EXPIRY:"));
    // each kind is reported on its own
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainFile(0).c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defectFile(0).c_str()));
    EXPECT_EQ(1, countAlerts("the gain calibration file"));
    EXPECT_EQ(1, countAlerts("the defect calibration file"));
}

TEST_F(A241Safety, AFileWithAnExpiryRaisesNoExpiryWarning) {
    xpe_clear_alerts();
    loadAll(nowMs() + 3600 * 1000, nowMs() + 3600 * 1000, nowMs() + 3600 * 1000);
    EXPECT_EQ(0, countAlerts("XPE_WARN_NO_EXPIRY"));
}

// ---------------------------------------------------------------- SAFE-001: an explicit bypass is flagged and logged
TEST_F(A241Safety, ABypassedOffsetOrGainIsFlaggedAndLogged) {
    loadAll(0, 0, 0);
    struct Case { const char* extra; bool flagged; const char* what; } cases[] = {
        {"", false, nullptr},
        {",\"bypassDefect\":true", false, nullptr},
        {",\"bypassOffset\":true", true, "offset correction is bypassed"},
        {",\"bypassGain\":true", true, "gain correction is bypassed"},
        {",\"bypassOffset\":true,\"bypassGain\":true", true, "offset and gain correction are bypassed"},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.extra);
        const std::string cfg = std::string("{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true") + c.extra + "}";
        std::vector<float> frame(kN);
        std::vector<uint16_t> raw(kN);
        fillRaw(raw);
        std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
        XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
        XpeImageMetadata meta{};
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, cfg.c_str()));
        EXPECT_EQ(c.flagged, (meta.flags & XPE_FLAG_CORRECTION_BYPASSED) != 0u);
        EXPECT_EQ(c.flagged ? 1 : 0, countAlerts("XPE_WARN_CORRECTION_BYPASSED:"));
        if (c.what) EXPECT_EQ(1, countAlerts(c.what));
        // the flag says "uncorrected": the matching stage flag is absent
        if (std::strstr(c.extra, "bypassOffset")) EXPECT_EQ(0u, meta.flags & XPE_FLAG_OFFSET_CORRECTED);
        if (std::strstr(c.extra, "bypassGain")) EXPECT_EQ(0u, meta.flags & XPE_FLAG_GAIN_CORRECTED);
    }
}

// ---------------------------------------------------------------- SAFE-004: the input buffer is never written
TEST_F(A241Safety, PipelineOutLeavesTheInputAndGivesTheInPlaceResult) {
    loadAll(0, 0, 0);
    std::vector<uint16_t> raw(kN);
    fillRaw(raw);

    // the in-place result, for comparison
    std::vector<float> inPlace(kN);
    std::memcpy(inPlace.data(), raw.data(), kN * sizeof(uint16_t));
    XpeImageBuffer a = bufferOf(inPlace.data(), kN * sizeof(float));
    XpeImageMetadata metaA{};
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&a, &metaA, nullptr, nullptr, kCfg));

    // the separate-output call on the same frame
    std::vector<uint16_t> in = raw;
    const uint64_t inBefore = digestOf(in.data(), in.size() * sizeof(uint16_t));
    std::vector<float> outBuf(kN, -1.0f);
    const XpeImageBuffer input = bufferOf(in.data(), kN * sizeof(uint16_t));
    XpeImageBuffer out = bufferOf(outBuf.data(), kN * sizeof(float));
    out.format = XPE_PIXEL_UINT16;   // whatever the caller had in it: set from the result
    XpeImageMetadata metaB{};
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&input, &out, &metaB, nullptr, nullptr, kCfg));
    EXPECT_EQ(inBefore, digestOf(in.data(), in.size() * sizeof(uint16_t))) << "the input frame is byte for byte as it was";
    EXPECT_EQ(XPE_PIXEL_FLOAT32, out.format);
    EXPECT_EQ(0, std::memcmp(inPlace.data(), outBuf.data(), kN * sizeof(float))) << "the result is the in-place result";
    EXPECT_EQ(metaA.flags, metaB.flags);
}

TEST_F(A241Safety, PipelineOutWithEveryStageBypassedGivesTheInputFrameAndLeavesTheInput) {
    loadAll(0, 0, 0);
    std::vector<uint16_t> in(kN);
    fillRaw(in);
    const uint64_t inBefore = digestOf(in.data(), in.size() * sizeof(uint16_t));
    std::vector<uint16_t> outBuf(kN, 0);
    const XpeImageBuffer input = bufferOf(in.data(), kN * sizeof(uint16_t));
    XpeImageBuffer out = bufferOf(outBuf.data(), kN * sizeof(uint16_t));
    XpeImageMetadata meta{};
    const char* cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassGain\":true,"
                      "\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&input, &out, &meta, nullptr, nullptr, cfg));
    EXPECT_EQ(XPE_PIXEL_UINT16, out.format);
    EXPECT_EQ(0, std::memcmp(in.data(), outBuf.data(), kN * sizeof(uint16_t)));
    EXPECT_EQ(inBefore, digestOf(in.data(), in.size() * sizeof(uint16_t)));
    EXPECT_NE(0u, meta.flags & XPE_FLAG_CORRECTION_BYPASSED);
}

TEST_F(A241Safety, PipelineOutRefusesWhatCannotHoldTheResultAndWritesNothing) {
    loadAll(0, 0, 0);
    std::vector<uint16_t> in(kN);
    fillRaw(in);
    const uint64_t inBefore = digestOf(in.data(), in.size() * sizeof(uint16_t));
    const XpeImageBuffer input = bufferOf(in.data(), kN * sizeof(uint16_t));
    XpeImageMetadata meta{};

    // an output room of a uint16 frame cannot hold the float result
    std::vector<uint8_t> small(kN * sizeof(uint16_t), 0xAB);
    XpeImageBuffer tooSmall = bufferOf(small.data(), small.size());
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_preprocess_pipeline_out(&input, &tooSmall, &meta, nullptr, nullptr, kCfg));
    for (const uint8_t b : small) ASSERT_EQ(0xAB, b) << "a refused call writes nothing";

    // out aliasing in is the in-place call, which this entry point does not offer
    XpeImageBuffer same = bufferOf(in.data(), kN * sizeof(float));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_pipeline_out(&input, &same, &meta, nullptr, nullptr, kCfg));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_pipeline_out(&input, nullptr, &meta, nullptr, nullptr, kCfg));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_pipeline_out(nullptr, &same, &meta, nullptr, nullptr, kCfg));
    EXPECT_EQ(inBefore, digestOf(in.data(), in.size() * sizeof(uint16_t)));
}

// ---------------------------------------------------------------- Codex #157 (1): the output may not share a byte with the input
TEST_F(A241Safety, PipelineOutRefusesAnyOverlapOfInputAndOutputAndLeavesBothAsTheyWere) {
    loadAll(0, 0, 0);
    // every stage bypassed: the result is a uint16 frame of the same size (kN * 2 bytes), so the ranges are easy to place
    const char* cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassGain\":true,"
                      "\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true}";
    const size_t bytes = kN * sizeof(uint16_t);
    struct Case { std::ptrdiff_t outAt; bool accepted; const char* what; };
    const Case cases[] = {
        {0, false, "the very same buffer"},
        {1, false, "one byte in"},
        {-1, false, "one byte before"},
        {static_cast<std::ptrdiff_t>(bytes) - 1, false, "overlapping by the last byte of the input"},
        {-static_cast<std::ptrdiff_t>(bytes) + 1, false, "overlapping by the first byte of the input"},
        {static_cast<std::ptrdiff_t>(bytes), true, "starting right after the input (adjacent, no shared byte)"},
        {-static_cast<std::ptrdiff_t>(bytes), true, "ending right before the input (adjacent, no shared byte)"},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.what);
        // one arena: the input in the middle, room on both sides
        std::vector<uint8_t> arena(bytes * 3, 0x5A);
        uint8_t* inPtr = arena.data() + bytes;
        std::vector<uint16_t> frame(kN);
        fillRaw(frame);
        std::memcpy(inPtr, frame.data(), bytes);
        const std::vector<uint8_t> before = arena;
        const XpeImageBuffer input = bufferOf(inPtr, bytes);
        XpeImageBuffer out = bufferOf(inPtr + c.outAt, bytes);
        XpeImageMetadata meta{};
        const int rc = xpe_preprocess_pipeline_out(&input, &out, &meta, nullptr, nullptr, cfg);
        if (c.accepted) {
            EXPECT_EQ(XPE_OK, rc);
            EXPECT_EQ(0, std::memcmp(inPtr, frame.data(), bytes)) << "the input is untouched";
            EXPECT_EQ(0, std::memcmp(inPtr + c.outAt, frame.data(), bytes)) << "the result is the frame";
        } else {
            EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc);
            EXPECT_EQ(before, arena) << "a refused call leaves the input AND the output bytes as they were";
            EXPECT_EQ(0u, meta.flags);
        }
    }
}

// ---------------------------------------------------------------- QA-A-246c (Codex #169): the order of the size and overlap refusals, at a FIXED layout
// The two refusals are different answers to different problems: an output room that cannot hold the result is BUFFER_TOO_SMALL; a result that
// would be written over the input is INVALID_INPUT. When both apply the size comes first (QA-A-246b). The earlier test of the size refusal
// put its buffers in two std::vectors and so depended on where the allocator placed them (5 of 60 runs failed before 246b); these two put
// the input and the output into ONE arena at offsets the test chooses, so the layout is the same in every run.
TEST_F(A241Safety, ATooSmallOutputJustBeforeTheInputWhoseRequiredRangeReachesTheInputIsBufferTooSmall) {
    loadAll(0, 0, 0);
    const size_t inBytes = kN * sizeof(uint16_t);    // the uint16 input frame
    const size_t needBytes = kN * sizeof(float);     // the float result kCfg produces
    // arena: [ output room (inBytes) | input (inBytes) | spare ]. The output declares inBytes (< needBytes); the bytes the RESULT would need,
    // [0, needBytes), run through the input at [inBytes, 2 * inBytes).
    std::vector<uint8_t> arena(needBytes + inBytes, 0xA7);
    uint8_t* const outPtr = arena.data();
    uint8_t* const inPtr = arena.data() + inBytes;
    std::vector<uint16_t> frame(kN);
    fillRaw(frame);
    std::memcpy(inPtr, frame.data(), inBytes);
    const std::vector<uint8_t> before = arena;
    const XpeImageBuffer input = bufferOf(inPtr, inBytes);
    XpeImageBuffer out = bufferOf(outPtr, inBytes);
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_preprocess_pipeline_out(&input, &out, &meta, nullptr, nullptr, kCfg))
        << "the output cannot hold the result: that is the answer, whatever the required range would have touched";
    EXPECT_EQ(before, arena) << "a refused call leaves every byte of the arena, input and output room, as it was";
    EXPECT_EQ(0u, meta.flags);
}

TEST_F(A241Safety, AnOutputBigEnoughForTheResultThatOverlapsTheInputIsInvalidInput) {
    loadAll(0, 0, 0);
    const size_t inBytes = kN * sizeof(uint16_t);
    const size_t needBytes = kN * sizeof(float);
    // arena: [ output (needBytes) over [0, needBytes) ] with the input at [inBytes, 2 * inBytes) inside it: the declared output is big enough,
    // and the result would be written over the input.
    std::vector<uint8_t> arena(needBytes + inBytes, 0xA7);
    uint8_t* const outPtr = arena.data();
    uint8_t* const inPtr = arena.data() + inBytes;
    std::vector<uint16_t> frame(kN);
    fillRaw(frame);
    std::memcpy(inPtr, frame.data(), inBytes);
    const std::vector<uint8_t> before = arena;
    const XpeImageBuffer input = bufferOf(inPtr, inBytes);
    XpeImageBuffer out = bufferOf(outPtr, needBytes);
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_pipeline_out(&input, &out, &meta, nullptr, nullptr, kCfg))
        << "room enough, but the result would destroy the input";
    EXPECT_EQ(before, arena) << "a refused call leaves every byte of the arena, input and output, as it was";
    EXPECT_EQ(0u, meta.flags);
}

TEST_F(A241Safety, PipelineOutOverlapCheckIsOverflowSafe) {
    loadAll(0, 0, 0);
    const char* cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassGain\":true,"
                      "\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true}";
    std::vector<uint8_t> arena(kN * sizeof(uint16_t) * 2, 0x33);
    std::vector<uint16_t> frame(kN);
    fillRaw(frame);
    std::memcpy(arena.data(), frame.data(), kN * sizeof(uint16_t));
    const std::vector<uint8_t> before = arena;
    XpeImageBuffer input = bufferOf(arena.data(), kN * sizeof(uint16_t));
    XpeImageBuffer out = bufferOf(arena.data() + kN * sizeof(uint16_t), kN * sizeof(uint16_t));
    XpeImageMetadata meta{};
    // a declared input size so large that start + size wraps the address space: the input claims everything above its start,
    // the output sits above it, so they overlap -- the wrapped sum must not make that look like "no overlap"
    input.dataSize = SIZE_MAX - 16;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_preprocess_pipeline_out(&input, &out, &meta, nullptr, nullptr, cfg));
    EXPECT_EQ(before, arena);
}

// ---------------------------------------------------------------- Codex #157 (2): the single-stage functions judge expiry too
TEST(A241Checker, JudgesExactlyAtAndAfterTheExpiry) {
    // a snapshot built by hand: the maps' contents do not matter to the checker, only that they are loaded and when they expire
    const int64_t expiry = 1'700'000'000'000;
    CalibSnapshot snap;
    snap.offset_map.reset(new float[1]);
    snap.gain_map.reset(new float[1]);
    snap.defect_map.reset(new uint8_t[1]);
    snap.offset_expiry_ms = snap.gain_expiry_ms = snap.defect_expiry_ms = expiry;
    for (const unsigned maps : {XPE_EXPIRY_OFFSET, XPE_EXPIRY_GAIN, XPE_EXPIRY_DEFECT, XPE_EXPIRY_ALL}) {
        SCOPED_TRACE(maps);
        xpe_clear_alerts();
        EXPECT_EQ(XPE_OK, xpe_calib_snapshot_expiry_check_at(snap, maps, expiry - 1)) << "before";
        EXPECT_EQ(XPE_OK, xpe_calib_snapshot_expiry_check_at(snap, maps, expiry)) << "at the expiry millisecond the map is still valid (now > expiry expires)";
        EXPECT_EQ(0, countAlerts("XPE_ERR_CALIBRATION_EXPIRED"));
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_snapshot_expiry_check_at(snap, maps, expiry + 1)) << "one millisecond after";
        EXPECT_EQ(1, countAlerts("XPE_ERR_CALIBRATION_EXPIRED"));
    }
    // each map on its own, and the alert names it
    const char* names[] = {"offset", "gain", "defect"};
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(names[k]);
        CalibSnapshot one = snap;
        (k == 0 ? one.offset_expiry_ms : k == 1 ? one.gain_expiry_ms : one.defect_expiry_ms) = expiry - 5;   // only this one is old
        xpe_clear_alerts();
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_snapshot_expiry_check_at(one, XPE_EXPIRY_ALL, expiry)) << "judged";
        EXPECT_EQ(1, countAlerts(names[k]));
        EXPECT_EQ(XPE_OK, xpe_calib_snapshot_expiry_check_at(one, XPE_EXPIRY_ALL & ~(1u << k), expiry)) << "outside the asked set: not judged";
    }
    // a map that is not loaded, and a never-expiring map, do not fail it
    xpe_clear_alerts();
    const CalibSnapshot none;
    EXPECT_EQ(XPE_OK, xpe_calib_snapshot_expiry_check_at(none, XPE_EXPIRY_ALL, INT64_MAX));
    CalibSnapshot forever = snap;
    forever.offset_expiry_ms = forever.gain_expiry_ms = forever.defect_expiry_ms = 0;
    EXPECT_EQ(XPE_OK, xpe_calib_snapshot_expiry_check_at(forever, XPE_EXPIRY_ALL, INT64_MAX));
    EXPECT_EQ(XPE_OK, xpe_calib_snapshot_expiry_check_at(snap, 0u, INT64_MAX));
    xpe_clear_alerts();
}

#ifdef XPE_CACHE_TEST_HOOKS   // needs the injected clock (xpe_preprocess_clock_tests), see nowMs() above
TEST_F(A241Safety, TheThreeSingleStageFunctionsRefuseAnExpiredMapAndWriteNothing) {
    struct Case { const char* name; int which; };
    const Case cases[] = {{"xpe_offset_correct", 0}, {"xpe_gain_correct", 1}, {"xpe_defect_correct", 2}};
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const int64_t soon = nowMs() + 400;
        loadAll(c.which == 0 ? soon : 0, c.which == 1 ? soon : 0, c.which == 2 ? soon : 0);
        std::vector<uint16_t> raw(kN);
        fillRaw(raw);
        std::vector<float> flt(kN, 1000.0f);
        std::vector<uint16_t> out16(kN, 0xBEEF);
        std::vector<float> outF(kN, -7.0f);
        XpeImageMetadata meta{};
        auto call = [&]() -> int {
            if (c.which == 0) {
                const XpeImageBuffer in = bufferOf(raw.data(), kN * sizeof(uint16_t));
                XpeImageBuffer out = bufferOf(out16.data(), kN * sizeof(uint16_t));
                return xpe_offset_correct(&in, &out, &meta);
            }
            if (c.which == 1) {
                const XpeImageBuffer in = bufferOf(raw.data(), kN * sizeof(uint16_t));
                XpeImageBuffer out = bufferOf(outF.data(), kN * sizeof(float), XPE_PIXEL_FLOAT32);
                return xpe_gain_correct(&in, &out, &meta);
            }
            const XpeImageBuffer in = bufferOf(flt.data(), kN * sizeof(float), XPE_PIXEL_FLOAT32);
            XpeImageBuffer out = bufferOf(outF.data(), kN * sizeof(float), XPE_PIXEL_FLOAT32);
            return xpe_defect_correct(&in, &out, &meta);
        };
        ASSERT_EQ(XPE_OK, call()) << "before it expires the stage works";
        advanceMs(600);
        xpe_clear_alerts();
        std::fill(out16.begin(), out16.end(), static_cast<uint16_t>(0xBEEF));
        std::fill(outF.begin(), outF.end(), -7.0f);
        const uint64_t before16 = digestOf(out16.data(), kN * 2), beforeF = digestOf(outF.data(), kN * 4);
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, call());
        EXPECT_EQ(before16, digestOf(out16.data(), kN * 2)) << "the output was not written";
        EXPECT_EQ(beforeF, digestOf(outF.data(), kN * 4)) << "the output was not written";
        EXPECT_EQ(1, countAlerts("XPE_ERR_CALIBRATION_EXPIRED:"));
    }
}
#endif

#ifdef XPE_CACHE_TEST_HOOKS   // needs the injected clock (xpe_preprocess_clock_tests), see nowMs() above
TEST_F(A241Safety, AnExpiredMapStopsAllFourPipelineEntryPoints) {
    const int64_t soon = nowMs() + 400;
    loadAll(soon, 0, 0);   // loaded through the three loaders: the store holds them
    advanceMs(600);
    std::vector<float> frame(kN, 0.0f);
    std::vector<uint16_t> raw(kN);
    fillRaw(raw);
    std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
    XpeImageMetadata meta{};

    XpeImageBuffer a = bufferOf(frame.data(), kN * sizeof(float));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_preprocess_pipeline_ex(&a, &meta, nullptr, nullptr, kCfg)) << "pipeline_ex";

    std::vector<float> outBuf(kN, -1.0f);
    const XpeImageBuffer in = bufferOf(raw.data(), kN * sizeof(uint16_t));
    XpeImageBuffer out = bufferOf(outBuf.data(), kN * sizeof(float));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_preprocess_pipeline_out(&in, &out, &meta, nullptr, nullptr, kCfg)) << "pipeline_out";
    for (const float v : outBuf) ASSERT_EQ(-1.0f, v);

    // the batch entry without a path judges the stored set
    XpeImageBuffer batch = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata metas[1] = {};
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_preprocess_pipeline_batch(&batch, 1, metas, nullptr, nullptr, kCfg)) << "pipeline_batch";

    // the entry that reads the three files itself refuses the expired FILE while loading it
    XpeImageBuffer viaPath = bufferOf(frame.data(), kN * sizeof(float));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_preprocess_pipeline(&viaPath, &meta, dir_.string().c_str(), nullptr, kCfg)) << "pipeline (calibPath)";
}
#endif

// ---------------------------------------------------------------- Codex #158 (1): a cache HIT installs the entry's expiry with the map
// Two files of one kind with different expiries (the same session, the same size), moved between the store and the cache.
#ifdef XPE_CACHE_TEST_HOOKS   // needs the injected clock (xpe_preprocess_clock_tests), see nowMs() above
TEST_F(A241Safety, ACacheHitInstallsTheExpiryOfTheMapItInstalls) {
    const char* kinds[] = {"offset", "gain", "defect"};
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(kinds[k]);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        // the other two maps never expire, so only this kind's expiry can stop a frame
        loadAll(0, 0, 0);
        const int64_t soon = nowMs() + 1500;
        std::string soonPath, foreverPath;
        auto cached = [&](const std::string& path) -> int {
            XpeImageBuffer view{};
            if (k == 0) return xpe_calib_load_offset_cached(path.c_str(), &view);
            if (k == 1) return xpe_calib_load_gain_cached(path.c_str(), &view);
            return xpe_calib_load_defect_cached(path.c_str(), &view);
        };
        auto plain = [&](const std::string& path) -> int {
            return k == 0 ? xpe_calib_load_offset(path.c_str()) : k == 1 ? xpe_calib_load_gain(path.c_str()) : xpe_calib_load_defect_map(path.c_str());
        };
        auto make = [&](int64_t expiry, const char* name) -> std::string {
            return k == 0 ? offsetFile(expiry, name) : k == 1 ? gainFile(expiry, name) : defectFile(expiry, name);
        };
        soonPath = make(soon, "soon.xcal");
        foreverPath = make(0, "forever.xcal");
        // the call that must judge expiry, for this kind and for the pipeline
        auto frameRc = [&]() -> int {
            std::vector<float> frame(kN, 1000.0f);
            std::vector<uint16_t> raw(kN, 1500);
            std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
            XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
            XpeImageMetadata meta{};
            return xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg);
        };
        auto stageRc = [&]() -> int {
            std::vector<uint16_t> raw(kN, 1500), o16(kN);
            std::vector<float> fin(kN, 1000.0f), outF(kN);
            XpeImageMetadata meta{};
            if (k == 0) {
                const XpeImageBuffer in = bufferOf(raw.data(), kN * 2);
                XpeImageBuffer out = bufferOf(o16.data(), kN * 2);
                return xpe_offset_correct(&in, &out, &meta);
            }
            if (k == 1) {
                const XpeImageBuffer in = bufferOf(raw.data(), kN * 2);
                XpeImageBuffer out = bufferOf(outF.data(), kN * 4, XPE_PIXEL_FLOAT32);
                return xpe_gain_correct(&in, &out, &meta);
            }
            const XpeImageBuffer in = bufferOf(fin.data(), kN * 4, XPE_PIXEL_FLOAT32);
            XpeImageBuffer out = bufferOf(outF.data(), kN * 4, XPE_PIXEL_FLOAT32);
            return xpe_defect_correct(&in, &out, &meta);
        };

        // (a) the entry that is about to expire goes into the cache, then the NEVER-expiring file replaces it in the store through the
        //     plain loader, then the cached entry is installed again by a HIT: the store must now hold ITS expiry
        ASSERT_EQ(XPE_OK, cached(soonPath));
        ASSERT_EQ(XPE_OK, plain(foreverPath));
        ASSERT_EQ(XPE_OK, cached(soonPath)) << "a hit";
        EXPECT_EQ(XPE_OK, frameRc()) << "before the entry expires";
        EXPECT_EQ(XPE_OK, stageRc());
        advanceMs(1700);
        xpe_clear_alerts();
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, frameRc()) << "after it: the pipeline refuses (before the fix the store still said 'never expires')";
        EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, stageRc()) << "and so does the single-stage function";
        EXPECT_EQ(2, countAlerts("XPE_ERR_CALIBRATION_EXPIRED:")) << "one alert per refused call";
        EXPECT_EQ(2, countAlerts(kinds[k])) << "each names the map";

        // (b) the other direction: the store holds the soon-to-expire file (plain load), a hit installs the NEVER-expiring cached entry:
        //     the store must now say 'never expires', not keep the other file's expiry
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        loadAll(0, 0, 0);
        const int64_t soon2 = nowMs() + 1200;
        const std::string soon2Path = make(soon2, "soon2.xcal");
        ASSERT_EQ(XPE_OK, cached(foreverPath));
        ASSERT_EQ(XPE_OK, plain(soon2Path));
        ASSERT_EQ(XPE_OK, cached(foreverPath)) << "a hit";
        advanceMs(1400);
        EXPECT_EQ(XPE_OK, frameRc()) << "the map the hit installed never expires: the other file's expiry must not stop the frame";
        EXPECT_EQ(XPE_OK, stageRc());
    }
}
#endif

TEST_F(A241Safety, ACacheHitReportsAndResetsTheNeverExpiresState) {
    // the never-expires warning state is part of what a hit installs, under the same lock
    xpe_clear_alerts();
    const std::string forever = offsetFile(0, "f.xcal");
    const std::string soon = offsetFile(nowMs() + 600000, "s.xcal");
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(forever.c_str(), &view));
    EXPECT_EQ(1, countAlerts("XPE_WARN_NO_EXPIRY:")) << "the load that fills the cache reports it once";
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(soon.c_str()));       // ends the never-expires state
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(forever.c_str(), &view));   // a hit: the store is a never-expiring map again
    EXPECT_EQ(1, countAlerts("XPE_WARN_NO_EXPIRY:")) << "the hit installed a never-expiring map: reported again, as a plain load would";
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(forever.c_str(), &view));   // a hit on the same state
    EXPECT_EQ(0, countAlerts("XPE_WARN_NO_EXPIRY:")) << "the same state is not reported again";
}

// ---------------------------------------------------------------- Codex #158 (3): the defect density tolerance of SRS-CALIB-FUNC-003
TEST_F(A241Safety, ADefectMapAboveFivePercentIsReportedAtLoadAndTheLimitItselfIsNot) {
    // SRS-CALIB-FUNC-003: "Maximum 5% defect density tolerance" (the SRS gives the tolerance, not the behaviour above it: the map is
    // loaded and reported, as the union with the gain-classified pixels already is). 100 x 100 = 10000 pixels: 500 is exactly 5 %.
    const uint32_t S = 100;
    const size_t n = static_cast<size_t>(S) * S;
    struct Case { size_t marked; bool rle; bool warns; };
    const Case cases[] = {{0, false, false}, {499, false, false}, {500, false, false}, {501, false, true}, {3000, false, true},
                          {500, true, false}, {501, true, true}, {3000, true, true}};
    for (const auto& c : cases) {
        SCOPED_TRACE(std::to_string(c.marked) + (c.rle ? " (RLE)" : ""));
        std::vector<uint8_t> v(n, 0);
        for (size_t i = 0; i < c.marked; ++i) v[(i * 7919) % n] = static_cast<uint8_t>(1 + i % 4);
        size_t marked = 0;
        for (const uint8_t b : v) marked += (b != 0);
        ASSERT_EQ(c.marked, marked) << "the fixture marks exactly the intended number of pixels";
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, "a241", 5);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = S;
        hdr.height = S;
        hdr.payload_len = n;
        const std::string path = (dir_ / "dense.xcal").string();
        ASSERT_EQ(XPE_OK, c.rle ? write_xcal_file_ex(path.c_str(), hdr, nullptr, 0, v.data(), n, true)
                                : write_xcal_file(path.c_str(), hdr, nullptr, 0, v.data(), n));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
        EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(path.c_str())) << "loaded either way";
        EXPECT_EQ(c.warns ? 1 : 0, countAlerts("XPE_WARN_DEFECT_MAP_OVER_LIMIT:"));
        // the cached loader: the load that fills the cache reports it, a hit on the cached map does not repeat it
        xpe_clear_alerts();
        xpe_calib_cache_clear();
        XpeImageBuffer view{};
        EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(path.c_str(), &view));
        EXPECT_EQ(c.warns ? 1 : 0, countAlerts("XPE_WARN_DEFECT_MAP_OVER_LIMIT:")) << "cache miss";
        xpe_clear_alerts();
        EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(path.c_str(), &view));
        EXPECT_EQ(0, countAlerts("XPE_WARN_DEFECT_MAP_OVER_LIMIT:")) << "cache hit: no second report";
    }
}

// ---------------------------------------------------------------- Codex #159: a REFUSED load raises nothing and changes nothing
TEST_F(A241Safety, ARefusedDefectLoadLeavesTheAlertQueueAndTheStoreAsItFoundThem) {
    loadAll(0, 0, 0);   // the good set, session "a241"
    auto frameDigest = [&]() -> uint64_t {
        std::vector<float> frame(kN);
        std::vector<uint16_t> raw(kN);
        fillRaw(raw);
        std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
        XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg));
        return digestOf(frame.data(), kN * sizeof(float));
    };
    const uint64_t before = frameDigest();
    xpe_clear_alerts();

    // a dense (over-limit) defect map of ANOTHER session
    session_ = "other";
    std::vector<uint8_t> dense(kN, 0);
    for (size_t i = 0; i < 400; ++i) dense[(i * 7) % kN] = 1;
    const std::string denseOther = write("dense_other.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, dense.data(), kN, 0);
    session_ = "a241";

    // (1) the plain loader
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_map(denseOther.c_str()));
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a refused plain load raises nothing (before: it said 'the map is loaded')";
    EXPECT_EQ(before, frameDigest()) << "and the stored defect map is the one from before";
    xpe_clear_alerts();

    // (2) the cached loader, a miss (it runs the plain loader), twice: a refused file is not cached
    XpeImageBuffer view{};
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_cached(denseOther.c_str(), &view));
    EXPECT_EQ(0, xpe_get_pending_alert_count());
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_cached(denseOther.c_str(), &view)) << "still refused: nothing was cached";
    EXPECT_EQ(0, xpe_get_pending_alert_count());
    EXPECT_EQ(before, frameDigest());
    xpe_clear_alerts();

    // (3) the set path: offset and gain of session "a241", the defect map of session "other" and over the limit
    const fs::path setDir = dir_ / "set";
    fs::create_directories(setDir);
    {
        const std::vector<float> off(kN, 100.0f), gn(kN, 1.0f);
        const fs::path keep = dir_;
        dir_ = setDir;
        write("offset.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, off.data(), kN * sizeof(float), 0);
        write("gain.xcal", XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, gn.data(), kN * sizeof(float), 0);
        session_ = "other";
        write("defect.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, dense.data(), kN, 0);
        session_ = "a241";
        dir_ = keep;
    }
    std::vector<float> frame(kN);
    std::vector<uint16_t> raw(kN);
    fillRaw(raw);
    std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
    XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_preprocess_pipeline(&img, &meta, setDir.string().c_str(), nullptr, kCfg)) << "the set is refused";
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "the set load contract: the alerts are as the call found them";
    EXPECT_EQ(before, frameDigest()) << "and the store still holds the set from before";
    xpe_clear_alerts();
}

TEST_F(A241Safety, AnOverLimitDefectMapThatIsInstalledIsReportedExactlyOnce) {
    // the success paths: the plain loader and the set load (same session)
    std::vector<uint8_t> dense(kN, 0);
    for (size_t i = 0; i < 400; ++i) dense[(i * 7) % kN] = 1;
    const std::string path = write("dense.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, dense.data(), kN, 0);
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(path.c_str()));
    EXPECT_EQ(1, countAlerts("XPE_WARN_DEFECT_MAP_OVER_LIMIT:"));
    xpe_clear_alerts();

    const fs::path setDir = dir_ / "set_ok";
    fs::create_directories(setDir);
    {
        const std::vector<float> off(kN, 100.0f), gn(kN, 1.0f);
        const fs::path keep = dir_;
        dir_ = setDir;
        write("offset.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, off.data(), kN * sizeof(float), 0);
        write("gain.xcal", XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, gn.data(), kN * sizeof(float), 0);
        write("defect.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, dense.data(), kN, 0);
        dir_ = keep;
    }
    std::vector<float> frame(kN);
    std::vector<uint16_t> raw(kN);
    fillRaw(raw);
    std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
    XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, setDir.string().c_str(), nullptr, kCfg));
    EXPECT_EQ(1, countAlerts("XPE_WARN_DEFECT_MAP_OVER_LIMIT:")) << "the set load reports the installed map once";
}

// ---------------------------------------------------------------- Codex #160: a cache HIT that re-activates an over-limit map reports it
TEST_F(A241Safety, ACacheHitThatTurnsAWithinToleranceMapIntoAnOverLimitOneReportsItAndARepeatDoesNot) {
    std::vector<uint8_t> dense(kN, 0), normal(kN, 0);
    for (size_t i = 0; i < 400; ++i) dense[(i * 7) % kN] = 1;          // ~13 %: over the limit
    for (size_t i = 0; i < 10; ++i) normal[(i * 7) % kN] = 1;          // well within tolerance
    const std::string denseA = write("dense_a.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, dense.data(), kN, 0);
    const std::string normalB = write("normal_b.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, normal.data(), kN, 0);
    const char* kWarn = "XPE_WARN_DEFECT_MAP_OVER_LIMIT:";
    XpeImageBuffer view{};

    xpe_calib_cache_clear();
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "A, cache miss: the load reports it";
    xpe_clear_alerts();

    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));
    EXPECT_EQ(0, countAlerts(kWarn)) << "A -> A, cache hit: the installed state does not change, no repeat";

    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(normalB.c_str()));
    EXPECT_EQ(0, countAlerts(kWarn)) << "B is within tolerance";
    xpe_clear_alerts();

    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "A -> B -> A, cache hit: the over-limit map is the one in use again (before: no warning)";
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));
    EXPECT_EQ(0, countAlerts(kWarn)) << "the repeat after that stays quiet";

    // a cached within-tolerance map never reports, hit or miss
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(normalB.c_str(), &view));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));   // a hit that turns B (within) into A (over): reported, cleared below
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(normalB.c_str(), &view));
    EXPECT_EQ(0, countAlerts(kWarn)) << "installing a within-tolerance map reports nothing";

    // invalidation: after the cache is cleared the next call is a miss and reports again
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(normalB.c_str()));
    xpe_calib_cache_clear();
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "after a cache clear: a miss, the load reports it";
}

TEST_F(A241Safety, ARefusedCacheHitOfAnOverLimitMapLeavesTheAlertQueueAndTheStoreAsItFoundThem) {
    std::vector<uint8_t> dense(kN, 0);
    for (size_t i = 0; i < 400; ++i) dense[(i * 7) % kN] = 1;
    const std::string denseA = write("dense_a2.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, dense.data(), kN, 0);   // session "a241"
    XpeImageBuffer view{};
    xpe_calib_cache_clear();
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached(denseA.c_str(), &view));   // A is in the cache now

    // a whole set of ANOTHER session replaces the store through the set path (offset, gain, a within-tolerance defect map)
    const fs::path setDir = dir_ / "set_other";
    fs::create_directories(setDir);
    {
        const std::vector<float> off(kN, 100.0f), gn(kN, 1.0f);
        const std::vector<uint8_t> none(kN, 0);
        const fs::path keep = dir_;
        dir_ = setDir;
        session_ = "other";
        write("offset.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, off.data(), kN * sizeof(float), 0);
        write("gain.xcal", XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, gn.data(), kN * sizeof(float), 0);
        write("defect.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, none.data(), kN, 0);
        session_ = "a241";
        dir_ = keep;
    }
    auto frameDigest = [&](const char* calibDir) -> uint64_t {
        std::vector<float> frame(kN);
        std::vector<uint16_t> raw(kN);
        fillRaw(raw);
        std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
        XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, calibDir, nullptr, kCfg));
        return digestOf(frame.data(), kN * sizeof(float));
    };
    const uint64_t before = frameDigest(setDir.string().c_str());   // loads the "other" set and processes a frame with it
    xpe_clear_alerts();

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_cached(denseA.c_str(), &view)) << "the hit is refused: A is session a241, the store holds 'other'";
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a refused hit raises nothing";
    EXPECT_EQ(before, frameDigest(nullptr)) << "and the store still holds the 'other' set";
}

// ---------------------------------------------------------------- Codex #161: which over-limit map is installed, and the generator's merge
TEST_F(A241Safety, ACacheHitThatReplacesOneOverLimitMapWithADifferentOneReportsIt) {
    std::vector<uint8_t> a(kN, 0), b(kN, 0);
    for (size_t i = 0; i < 400; ++i) { a[(i * 7) % kN] = 1; b[(i * 11 + 5) % kN] = 1; }   // two different masks, both ~13 %
    const std::string pa = write("over_a.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, a.data(), kN, 0);
    const std::string pb = write("over_b.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, b.data(), kN, 0);
    const char* kWarn = "XPE_WARN_DEFECT_MAP_OVER_LIMIT:";
    XpeImageBuffer view{};

    xpe_calib_cache_clear();
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pa.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "A, miss";
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pb.c_str(), &view));
    EXPECT_EQ(2, countAlerts(kWarn)) << "B, miss (a load of another over-limit map)";
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pa.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "B -> A, hit: another over-limit map is now in use (before: no warning)";
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pb.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "A -> B, hit";
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pb.c_str(), &view));
    EXPECT_EQ(0, countAlerts(kWarn)) << "B -> B, hit: the same map, no repeat";
}

TEST_F(A241Safety, TheGeneratorsMergeKeepsTheDensityStateOfTheInstalledMapCurrent) {
    // S = 400 marked pixels (over the limit); W = the first 150 of them (4.88 %, within it)
    std::vector<size_t> s;
    for (size_t i = 0; i < 400; ++i) s.push_back((i * 7) % kN);
    std::vector<uint8_t> full(kN, 0), part(kN, 0);
    for (size_t i = 0; i < s.size(); ++i) { full[s[i]] = 1; if (i < 150) part[s[i]] = 1; }
    const std::string pFull = write("gen_full.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, full.data(), kN, 0);
    const std::string pPart = write("gen_part.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, part.data(), kN, 0);
    const char* kWarn = "XPE_WARN_DEFECT_MAP_OVER_LIMIT:";
    XpeImageBuffer view{};

    xpe_calib_cache_clear();
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached(pFull.c_str(), &view));   // the full map is in the cache
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(pPart.c_str()));             // the installed map is within tolerance
    EXPECT_EQ(0, countAlerts(kWarn));

    // sigma-clip generation marks the other 250 pixels of S: five dark frames, a marked pixel reads {100,110,105,108,500}, the rest 104
    const uint16_t bad[5] = {100, 110, 105, 108, 500};
    std::vector<std::vector<uint16_t>> frames(5, std::vector<uint16_t>(kN, 104));
    for (size_t i = 150; i < s.size(); ++i)
        for (int f = 0; f < 5; ++f) frames[static_cast<size_t>(f)][s[i]] = bad[f];
    XpeImageBuffer bufs[5];
    for (int f = 0; f < 5; ++f) bufs[f] = bufferOf(frames[static_cast<size_t>(f)].data(), kN * sizeof(uint16_t));
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(bufs, 5, 100.0f, 25.0f, (dir_ / "gen_offset.xcal").string().c_str(),
                                                "{\"method\":\"sigma_clip\",\"sigma\":1.0}"));
    EXPECT_EQ(0, countAlerts(kWarn)) << "the generator raises no load warning of its own (documented)";

    // the installed map is now W union (250 new marks) = S, the same mask as the cached full map: re-activating it changes nothing
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pFull.c_str(), &view));
    EXPECT_EQ(0, countAlerts(kWarn)) << "same mask, state in step with the merge (a stale 'within tolerance' state reported it again)";

    // and a merge that leaves the store holding a mask the cache does not know: the cached map is a different over-limit map
    std::vector<std::vector<uint16_t>> more(5, std::vector<uint16_t>(kN, 104));
    for (int f = 0; f < 5; ++f) more[static_cast<size_t>(f)][(kN - 1)] = bad[f];   // one more marked pixel, outside S
    XpeImageBuffer bufs2[5];
    for (int f = 0; f < 5; ++f) bufs2[f] = bufferOf(more[static_cast<size_t>(f)].data(), kN * sizeof(uint16_t));
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(bufs2, 5, 100.0f, 25.0f, (dir_ / "gen_offset2.xcal").string().c_str(),
                                                "{\"method\":\"sigma_clip\",\"sigma\":1.0}"));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(pFull.c_str(), &view));
    EXPECT_EQ(1, countAlerts(kWarn)) << "the merged store differs from the cached full map: another over-limit map is re-activated";
}

#ifdef XPE_CACHE_TEST_HOOKS
// ---------------------------------------------------------------- QA-A-244: the injected clock decides, not the time that passed
TEST_F(A241Safety, TheExpiryBoundaryIsJudgedAtTheInjectedTime) {
    const int64_t expiry = nowMs() + 1000;
    loadAll(expiry, 0, 0);
    auto frameRc = [&]() -> int {
        std::vector<float> frame(kN);
        std::vector<uint16_t> raw(kN);
        fillRaw(raw);
        std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
        XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
        XpeImageMetadata meta{};
        return xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg);
    };
    EXPECT_EQ(XPE_OK, frameRc()) << "well before the expiry";
    g_testNowMs = expiry - 1;
    EXPECT_EQ(XPE_OK, frameRc()) << "1 ms before";
    g_testNowMs = expiry;
    EXPECT_EQ(XPE_OK, frameRc()) << "at the expiry the map is still valid (it expires when the time is GREATER than the expiry)";
    g_testNowMs = expiry + 1;
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, frameRc()) << "1 ms after";
    g_testNowMs = expiry - 500;
    EXPECT_EQ(XPE_OK, frameRc()) << "the clock is the only input: moved back, the same map is valid again";
}

TEST_F(A241Safety, ASlowRealClockDoesNotChangeTheOutcome) {
    // the failure this replaces: load + first frame took longer than a real-time window on a slow runner. Make this one slow on purpose.
    const int64_t expiry = nowMs() + 400;
    loadAll(expiry, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));   // a slow environment: a real second between the load and the frame
    std::vector<float> frame(kN);
    std::vector<uint16_t> raw(kN);
    fillRaw(raw);
    std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
    XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg)) << "the injected time did not move, so the map has not expired";
    advanceMs(401);
    std::memcpy(frame.data(), raw.data(), kN * sizeof(uint16_t));
    img = bufferOf(frame.data(), kN * sizeof(float));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kCfg)) << "and moving it past the expiry stops the frame";
}

TEST_F(A241Safety, TheLoadersJudgeExpiryAtTheInjectedTimeToo) {
    const int64_t expiry = nowMs() + 1000;
    const std::string path = offsetFile(expiry);
    g_testNowMs = expiry;
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset(path.c_str())) << "at the expiry: loads";
    g_testNowMs = expiry + 1;
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_offset(path.c_str())) << "1 ms after: the loader refuses an expired file";
    XpeImageBuffer view{};
    xpe_calib_cache_clear();
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_offset_cached(path.c_str(), &view)) << "the cached loader's miss path too";
}
#endif

}  // namespace
