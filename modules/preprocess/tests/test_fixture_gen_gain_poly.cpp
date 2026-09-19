/**
 * @file test_fixture_gen_gain_poly.cpp
 * @brief The fixture generator's --gain-poly path produces a file that is
 *        actually XCAL_TYPE_GAIN_POLY, loads, and drives the polynomial
 *        alert into the queue (QA-A-136, #198).
 *
 * WHY THIS FILE EXISTS. #198 says three alerts added today do not reach the
 * user; two were traced to the screen. The third -- the gain polynomial one --
 * could not be reproduced at all, because nothing in the repository could
 * produce a XCAL_TYPE_GAIN_POLY file: the generator called
 * xpe_calib_generate_gain() (scalar XCAL_TYPE_GAIN) and the polynomial
 * generator was called only from module tests.
 *
 * WHAT IS ASSERTED, AND WHAT IS NOT. "A file appeared" is not the point.
 * The point is the alert moving. So this file asserts the queue, not the
 * filesystem:
 *
 *   1. the generated file's header type byte reads XCAL_TYPE_GAIN_POLY (3)
 *   2. xpe_calib_load_gain() accepts it
 *   3. the polynomial alert is actually pushed
 *   4. CONTROL: the default (scalar) run writes XCAL_TYPE_GAIN (1) and that
 *      alert does NOT appear -- an assertion that fires for both files would
 *      say nothing about either
 *
 * ONE MEASURED CORRECTION TO THE PREMISE. The card states the alert fires on
 * load alone. It does not, for a file this generator writes. The loader's
 * polynomial alert (xpe_calib_load_gain.cpp:156) fires only when the file
 * carries NO fitted dose range -- and the generator records dose_min/dose_max
 * (QA-A-123), so loading a freshly generated file is silent. The polynomial
 * alert that a current file can raise is the out-of-range clamp count
 * (gain_correct.cpp), and it needs exactly one xpe_gain_correct() call with a
 * pixel above the fitted range -- no pipeline, no image processing beyond
 * that single call. LoadAloneIsSilent below pins that silence so the next
 * reader does not rediscover it, and PolyAlertIsPushed pins the alert that
 * does fire.
 *
 * FALSIFICATION. Point the generator's polynomial path back at the scalar
 * generator and TypeByteIsGainPoly fails on the header byte -- which is why
 * that assertion reads the byte rather than the file name.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "preprocess_state_fixture.h"
#include "xpe_fixture_gen_path.h"  // generated: XPE_FIXTURE_GEN_EXE

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 32;
constexpr uint32_t kH = 32;

uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

/** Runs the generator into @p dir; @p extra carries --gain-poly when wanted. */
int runGenerator(const fs::path& dir, const std::string& extra) {
    std::string cmd;
    cmd += "\"";
    cmd += XPE_FIXTURE_GEN_EXE;
    cmd += "\" --out \"" + dir.string() + "\"";
    cmd += " --width " + std::to_string(kW);
    cmd += " --height " + std::to_string(kH);
    cmd += " --seed 7";
    if (!extra.empty()) cmd += " " + extra;
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";
#endif
    return std::system(cmd.c_str());
}

/** Reads the XCal header's type field (offset 8, uint32 LE). */
uint32_t typeByteOf(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return 0xFFFFFFFFu;
    char buf[12]{};
    f.read(buf, sizeof(buf));
    if (f.gcount() < static_cast<std::streamsize>(sizeof(buf))) return 0xFFFFFFFFu;
    uint32_t t = 0;
    std::memcpy(&t, buf + 8, sizeof(t));
    return t;
}

class FixtureGenGainPolyTest : public XpePreprocessStateFixture {
protected:
    fs::path root;

    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        // QA-A-137 (#198). The alert queue is a common-module global and is
        // deliberately NOT owned by xpe_preprocess_shutdown(): clearing it
        // when one module goes down would discard other modules' undelivered
        // alerts. Draining it is the consumer's job, through this public API
        // -- the same reset path the application uses, and the one every
        // other alert-asserting test here already calls
        // (test_gain_poly_dose_range.cpp:250, test_alert_queue_overflow.cpp:87).
        // Measured: without this, PolyAlertIsPushed's alert survived into
        // ScalarGainRaisesNoPolyAlert and made its negative assertion fail in
        // a single-process run. A test-only reset hook would have hidden the
        // fact that the product has a reset path at all.
        xpe_clear_alerts();
        root = fs::temp_directory_path() /
               ("xpe_fixpoly_" + std::to_string(nowMs()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::create_directories(root);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root, ec);
        xpe_clear_alerts();  // restore only what this test raised
        XpePreprocessStateFixture::TearDown();
    }

    static bool alertContains(const char* needle) {
        char msg[512];
        int32_t sev = -1;
        const int32_t count = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < count; ++i) {
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
            if (std::string(msg).find(needle) != std::string::npos) return true;
        }
        return false;
    }

    /** One correction call on an image whose single pixel sits above the
     *  fitted dose range. Not a pipeline -- one call. */
    XpeErrorCode correctOneBrightPixel() {
        std::vector<uint16_t> in(static_cast<size_t>(kW) * kH, 65535u);
        std::vector<float>    out(in.size(), -1.0f);
        XpeImageBuffer ib{}, ob{};
        ib.data = in.data(); ib.width = kW; ib.height = kH;
        ib.bitsAllocated = 16; ib.bitsStored = 16; ib.format = XPE_PIXEL_UINT16;
        ib.dataSize = static_cast<uint32_t>(in.size() * sizeof(uint16_t));
        ob.data = out.data(); ob.width = kW; ob.height = kH;
        ob.bitsAllocated = 32; ob.bitsStored = 32; ob.format = XPE_PIXEL_FLOAT32;
        ob.dataSize = static_cast<uint32_t>(out.size() * sizeof(float));
        XpeImageMetadata meta{};
        return xpe_gain_correct(&ib, &ob, &meta);
    }
};

// The header byte, not the file name, is what a reader dispatches on.
TEST_F(FixtureGenGainPolyTest, TypeByteIsGainPoly) {
    ASSERT_EQ(0, runGenerator(root, "--gain-poly"));
    const fs::path poly = root / "gain_poly.xcal";
    ASSERT_TRUE(fs::exists(poly)) << "generator did not write gain_poly.xcal";
    EXPECT_EQ(static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY), typeByteOf(poly));
}

// CONTROL: the default run is untouched and produces the scalar type.
TEST_F(FixtureGenGainPolyTest, DefaultRunStaysScalarAndWritesNoPolyFile) {
    ASSERT_EQ(0, runGenerator(root, ""));
    EXPECT_FALSE(fs::exists(root / "gain_poly.xcal"));
    EXPECT_EQ(static_cast<uint32_t>(XCAL_TYPE_GAIN), typeByteOf(root / "gain.xcal"));
}

TEST_F(FixtureGenGainPolyTest, GeneratedPolyFileLoads) {
    ASSERT_EQ(0, runGenerator(root, "--gain-poly"));
    const std::string poly = (root / "gain_poly.xcal").string();
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
}

// MEASURED, and contrary to the card's premise: a file this generator writes
// carries a dose range, so the loader's "no dose range" warning does not fire.
// Pinned so the next reader does not chase an alert that cannot appear here.
TEST_F(FixtureGenGainPolyTest, LoadAloneIsSilent) {
    ASSERT_EQ(0, runGenerator(root, "--gain-poly"));
    const std::string poly = (root / "gain_poly.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
    EXPECT_FALSE(alertContains("gain polynomial loaded without a dose range"));
    EXPECT_FALSE(alertContains("inverted dose range"));
}

// THE CARD'S ASSERTION: the polynomial alert actually reaches the queue.
TEST_F(FixtureGenGainPolyTest, PolyAlertIsPushed) {
    ASSERT_EQ(0, runGenerator(root, "--gain-poly"));
    const std::string poly = (root / "gain_poly.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
    ASSERT_EQ(XPE_OK, correctOneBrightPixel());
    EXPECT_TRUE(alertContains("gain polynomial's fitted dose range"))
        << "loading a generated XCAL_TYPE_GAIN_POLY file and correcting one "
           "out-of-range pixel raised no polynomial alert";
}

// CONTROL for the alert: the scalar file drives no polynomial alert at all.
TEST_F(FixtureGenGainPolyTest, ScalarGainRaisesNoPolyAlert) {
    ASSERT_EQ(0, runGenerator(root, ""));
    const std::string gain = (root / "gain.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gain.c_str()));
    ASSERT_EQ(XPE_OK, correctOneBrightPixel());
    EXPECT_FALSE(alertContains("gain polynomial's fitted dose range"));
}

}  // namespace
