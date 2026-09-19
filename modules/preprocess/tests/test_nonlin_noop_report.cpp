/**
 * @file test_nonlin_noop_report.cpp
 * @brief The no-op report reaches the caller that passes no config, and does
 *        so once per condition rather than once per frame (QA-A-140, #196).
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
 * WHY THE FREQUENCY IS ASSERTED WITH A NUMBER. The message carries no
 * per-frame data, and the alert queue holds 64 entries with FIFO eviction
 * (xpe_common.cpp:59). One alert per frame would evict every other alert
 * within 65 frames -- including the #194 clamp count that the same operator
 * needs. So the report is latched per condition and re-armed when a LUT is
 * loaded or unloaded, and ManyFramesRaiseOneAlert pins that with a count
 * rather than with a claim.
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
    xpe_clear_alerts();  // the load path re-arms the latch; start from empty

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

// FREQUENCY, as a number. 200 frames is past the queue's 64-entry cap, so a
// per-frame alert would both flood and evict.
TEST_F(NonlinNoopReportTest, ManyFramesRaiseOneAlert) {
    for (int i = 0; i < 200; ++i) {
        XpeImageBuffer b = buffer();
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&b, nullptr));
    }
    EXPECT_EQ(1, alertsContaining(kNoopNeedle))
        << "200 no-op frames should report once, not once per frame";
    EXPECT_LE(xpe_get_pending_alert_count(), 2)
        << "the queue should not have been flooded";
}

// The latch is a report suppressor, not a permanent mute: once the situation
// changes, a later no-op is new information.
TEST_F(NonlinNoopReportTest, LoadingThenUnloadingALutReArmsTheReport) {
    XpeImageBuffer b1 = buffer();
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&b1, nullptr));
    ASSERT_EQ(1, alertsContaining(kNoopNeedle));

    ASSERT_EQ(XPE_OK, WriteHalvingLut(lutPath()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(lutPath().c_str()));
    xpe_calib_unload_nonlin_lut();

    XpeImageBuffer b2 = buffer();
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&b2, nullptr));
    EXPECT_EQ(2, alertsContaining(kNoopNeedle))
        << "the LUT came and went; the next no-op is a new fact";
}

}  // namespace
