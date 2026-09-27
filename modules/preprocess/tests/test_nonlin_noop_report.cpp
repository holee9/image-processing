/**
 * @file test_nonlin_noop_report.cpp
 * @brief The no-op report reaches the caller that passes no config, and is
 *        raised once per call (QA-A-140 #196; frequency revised by QA-A-142 #199).
 *
 * WHAT WAS WRONG. `xpe_nonlinearity_apply` returned early on
 * `if (!configJsonOrNull) return XPE_OK;`, which sat AHEAD of the no-op alert
 * QA-A-127 put at the end of the function. The GUI wires this stage into its
 * chain and passes nullptr for config (GUI-C-128), so the #196 alert could
 * never appear on the one path that needed it.
 *
 * The guard was sound for what originally followed it -- a "mode" parse, which
 * has nothing to read without a config -- and stopped being sound when
 * QA-A-127 replaced that parse with the report. Its cited requirement,
 * REQ-P1A-013, is a renumbering orphan: in the current set that number is
 * defect correction.
 *
 * WHY THE FREQUENCY IS ASSERTED WITH A NUMBER, AND WHY THE NUMBER CHANGED.
 * QA-A-140 latched this report to once per condition, on the argument that a
 * per-frame alert would evict everything else from a 64-entry queue.
 * QA-A-142 (#199) measured that argument and it failed: the host drains the
 * queue after every native call, so nothing is evicted, and -- measured over
 * 20 runs -- the latch suppressed 20->1 only when the module stayed up and
 * 20->20 in the host that actually exists, which tears the module down per
 * run. The latch is gone. EveryNoopFrameReportsOnce pins the frequency that
 * replaced it, still with a count rather than a claim.
 *
 * THE CONTROL THAT MAKES THE REST MEAN SOMETHING. A test that only checks
 * "alert present" in three no-op cases would pass equally against a function
 * that alerts unconditionally. LutLoadedIsSilentAndActuallyChangesTheFrame
 * asserts BOTH halves of the other side: no alert, and pixels that actually
 * moved. Without the second half it would also pass against a stage that does
 * nothing at all and says nothing.
 *
 * Falsification: put the early return back (behind a runtime-false condition,
 * per the note in test_global_state_hygiene.cpp) and NullConfigStillReports
 * goes red while the control stays green.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"
#include "preprocess_state_fixture.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;
constexpr const char* kNoopNeedle = "nonlinearity correction did nothing";
constexpr const char* kNonLinearNeedle = "panel.linear is false but no nonlinearity LUT";

/** A monotone 4096-entry LUT that halves every input -- visibly applied. */
XpeErrorCode WriteHalvingLut(const std::string& path) {
    using namespace std::chrono;
    const int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    std::vector<uint16_t> table(4096);
    for (size_t i = 0; i < table.size(); ++i) {
        table[i] = static_cast<uint16_t>(i / 2);
    }

    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
    hdr.pixel_format     = XCAL_FMT_UINT16;
    hdr.width            = static_cast<uint32_t>(table.size());
    hdr.height           = 1;
    hdr.created_epoch_ms = now_ms;
    hdr.expiry_epoch_ms  = 0;
    hdr.config_json_len  = 0;
    hdr.payload_len      = table.size() * sizeof(uint16_t);

    return write_xcal_file(path.c_str(), hdr, nullptr, 0,
                           reinterpret_cast<const uint8_t*>(table.data()),
                           table.size() * sizeof(uint16_t));
}

class NonlinNoopReportTest : public XpePreprocessStateFixture {
protected:
    std::vector<uint16_t> px;

    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        // The calibration store is global; a LUT left by another test would
        // silence every no-op case here for the wrong reason.
        xpe_calib_unload_nonlin_lut();
        xpe_clear_alerts();
        px.assign(N, 1000u);
    }

    void TearDown() override {
        xpe_calib_unload_nonlin_lut();
        // QA-A-138 axis 4: leave the queue as this test found it, through the
        // product's own drain.
        xpe_clear_alerts();
        XpePreprocessStateFixture::TearDown();
    }

    XpeImageBuffer buffer() {
        XpeImageBuffer b{};
        b.data = px.data(); b.width = W; b.height = H;
        b.bitsAllocated = 16; b.bitsStored = 16; b.format = XPE_PIXEL_UINT16;
        b.dataSize = static_cast<uint32_t>(px.size() * sizeof(uint16_t));
        return b;
    }

    static int32_t alertsContaining(const char* needle) {
        char msg[512];
        int32_t sev = -1;
        int32_t hits = 0;
        const int32_t count = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < count; ++i) {
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
            if (std::string(msg).find(needle) != std::string::npos) ++hits;
        }
        return hits;
    }

    std::string lutPath() const {
        return std::string(::testing::TempDir()) + "/a140_halving_nonlin.xcal";
    }
};

// THE GUI'S ACTUAL CALL SHAPE: config is nullptr, no LUT is loaded.
TEST_F(NonlinNoopReportTest, NullConfigStillReports) {
    XpeImageBuffer b = buffer();
    EXPECT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, nullptr));
    EXPECT_EQ(1, alertsContaining(kNoopNeedle))
        << "the no-op report did not reach a caller that passes no config";
}

// The pre-existing path keeps working -- this change adds a case, it does not
// move one.
TEST_F(NonlinNoopReportTest, ConfigPresentStillReports) {
    XpeImageBuffer b = buffer();
    EXPECT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, R"({"mode":"clinical"})"));
    EXPECT_EQ(1, alertsContaining(kNoopNeedle));
}

// CONTROL, both halves: with a LUT loaded the stage says nothing AND the
// pixels actually move. The second half is what separates this from a stage
// that is silent because it does nothing.
TEST_F(NonlinNoopReportTest, LutLoadedIsSilentAndActuallyChangesTheFrame) {
    ASSERT_EQ(XPE_OK, WriteHalvingLut(lutPath()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(lutPath().c_str()));
    xpe_clear_alerts();  // the load itself may report; start from an empty queue

    XpeImageBuffer b = buffer();
    EXPECT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, nullptr));

    EXPECT_EQ(0, alertsContaining(kNoopNeedle)) << "a LUT was loaded and applied";
    EXPECT_EQ(500u, px[0]) << "the LUT halves every input; the frame did not change";
}

// The explicit non-linear declaration keeps its error and its return code.
TEST_F(NonlinNoopReportTest, NonLinearPanelWithoutLutIsStillAnError) {
    XpeImageBuffer b = buffer();
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
              xpe_nonlinearity_correct(&b, R"({"panel.linear":"false"})"));
    EXPECT_EQ(1, alertsContaining(kNonLinearNeedle));
    EXPECT_EQ(0, alertsContaining(kNoopNeedle)) << "this path is an error, not a no-op";
}

// FREQUENCY, as a number. One report per no-op call -- QA-A-142 (#199).
// Asserted rather than left implicit because the previous number was 1, and a
// test that stopped asserting the count would leave the change invisible.
// 40 rather than 200: the queue caps at 64 (xpe_common.cpp:59), so a run past
// it would measure the cap instead of the producer.
TEST_F(NonlinNoopReportTest, EveryNoopFrameReportsOnce) {
    constexpr int kFrames = 40;
    for (int i = 0; i < kFrames; ++i) {
        XpeImageBuffer b = buffer();
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, nullptr));
    }
    EXPECT_EQ(kFrames, alertsContaining(kNoopNeedle))
        << "each no-op call reports; suppression belongs to the consumer";
}

// The host this module actually has drains after every call, which is the
// condition under which one-per-call is the right frequency: the operator sees
// the line once per frame rendered, not a queue filling up behind them.
// Modelled here by draining between calls, as NativeAlertDrain.cs:109 does.
TEST_F(NonlinNoopReportTest, WithAHostThatDrainsEachCallTheQueueNeverGrows) {
    for (int i = 0; i < 40; ++i) {
        XpeImageBuffer b = buffer();
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, nullptr));
        EXPECT_EQ(1, alertsContaining(kNoopNeedle)) << "at frame " << i;
        xpe_clear_alerts();
    }
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

// CONTROL for the removal -- QA-A-142 (#199) §4. Taking suppression away must
// not take anything else with it: the LUT path has to stay silent, and the
// non-linear-panel path has to keep its own ERROR. A removal that also
// silenced those would pass EveryNoopFrameReportsOnce and still be wrong.
TEST_F(NonlinNoopReportTest, RemovingSuppressionDidNotSilenceTheOtherPaths) {
    ASSERT_EQ(XPE_OK, WriteHalvingLut(lutPath()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(lutPath().c_str()));
    xpe_clear_alerts();

    for (int i = 0; i < 5; ++i) {
        XpeImageBuffer b = buffer();
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, nullptr));
    }
    EXPECT_EQ(0, alertsContaining(kNoopNeedle)) << "a LUT is loaded and applied";

    xpe_calib_unload_nonlin_lut();
    xpe_clear_alerts();

    XpeImageBuffer b = buffer();
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
              xpe_nonlinearity_correct(&b, R"({"panel.linear":"false"})"));
    EXPECT_EQ(1, alertsContaining(kNonLinearNeedle))
        << "the explicit non-linear error must survive the removal";
}

}  // namespace
