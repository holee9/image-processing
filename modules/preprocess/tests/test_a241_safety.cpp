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

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

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

    void SetUp() override {
        dir_ = fs::temp_directory_path() / "xpe_a241_safety";
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    std::string write(const char* name, XCalType type, XCalPixelFormat fmt, const void* data, size_t bytes, int64_t expiryMs) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, "a241", 5);
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
    std::string gainFile(int64_t expiryMs) {
        const std::vector<float> v(kN, 1.0f);
        return write("gain.xcal", XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, v.data(), kN * sizeof(float), expiryMs);
    }
    std::string defectFile(int64_t expiryMs) {
        std::vector<uint8_t> v(kN, 0);
        v[100] = 1;
        return write("defect.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, v.data(), kN, expiryMs);
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

        std::this_thread::sleep_for(std::chrono::milliseconds(600));
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

TEST_F(A241Safety, ABypassedMapThatExpiredStillStopsTheFrame) {
    // SAFE-002 judges "any loaded calibration file", the bypassed one included.
    loadAll(nowMs() + 300, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::vector<float> frame(kN, 0.0f);
    XpeImageBuffer img = bufferOf(frame.data(), kN * sizeof(float));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED,
              xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr,
                                         "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassOffset\":true}"));
}

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
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
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

TEST_F(A241Safety, AnExpiredMapStopsAllFourPipelineEntryPoints) {
    const int64_t soon = nowMs() + 400;
    loadAll(soon, 0, 0);   // loaded through the three loaders: the store holds them
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
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

}  // namespace
