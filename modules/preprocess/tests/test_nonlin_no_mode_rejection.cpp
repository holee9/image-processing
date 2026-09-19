/**
 * @file test_nonlin_no_mode_rejection.cpp
 * @brief The nonlinearity stage decides nothing from "mode" (QA-A-127, #196).
 *
 * WHAT WAS MEASURED, AND WHY THE LIST WENT AWAY.
 *
 * The stage used to reject any "mode" outside {"standard","high_gain",
 * "low_dose"} with XPE_ERR_CONFIG_INVALID, which fails the whole pipeline.
 * Three measurements retired it (QA-A-126):
 *
 *  - REACHED: a pipeline config carrying "mode":"clinical" returned -4 while
 *    the same config without the key returned 0.
 *  - NO SOURCE: the three names are defined as detector modes nowhere in
 *    docs/ or .moai/specs/. "high_gain" is a defect-detection pixel mask
 *    elsewhere, "low_dose" a display LUT name.
 *  - WRONG REQUIREMENT: the cited REQ-P1A-014 is "Calibration File Loading
 *    (Offset)", and SPEC-XPE-P1A puts nonlinearity out of its own scope.
 *
 * The root is two vocabularies under one JSON key: that list held DETECTOR
 * modes while the repository puts OPERATING modes ("clinical", "research",
 * "production", "test") under the same "mode".
 *
 * WHY THE ALERT IS ASSERTED AND NOT JUST THE RETURN CODE. A silent pass and a
 * correction that ran are indistinguishable from outside: the frame is
 * byte-identical either way and XPE_FLAG_NONLINEARITY_CORRECTED is absent in
 * both. The error at least said something; trading it for silence would
 * remove the caller's only signal. So these tests assert the alert -- a test
 * checking only `rc` would still pass with the alert deleted, which is
 * precisely the falsification this file has to fail.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

// Every stage except nonlinearity is bypassed. Without this the pipeline stops
// at offset correction with XPE_ERR_CALIB_NOT_LOADED before stage 3 is ever
// reached -- the first QA-A-126 probe measured exactly that, and for a moment
// it looked like "the mode does not matter".
constexpr const char* kBypassAllButNonlin =
    "\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,"
    "\"bypassGain\":true,\"bypassBinning\":true,\"bypassDefect\":true,"
    "\"bypassGhost\":true";

class NonlinModeTest : public XpePreprocessStateFixture {
protected:
    XpeErrorCode runPipeline(const std::string& extra) {
        std::vector<uint16_t> in(N, 1000u);
        XpeImageBuffer ib{};
        ib.data = in.data(); ib.width = W; ib.height = H;
        ib.bitsAllocated = 16; ib.bitsStored = 16; ib.format = XPE_PIXEL_UINT16;
        ib.dataSize = static_cast<uint32_t>(in.size() * sizeof(uint16_t));
        XpeImageMetadata meta{};
        const std::string cfg = "{" + std::string(kBypassAllButNonlin) + extra + "}";
        return xpe_preprocess_pipeline(&ib, &meta, nullptr, nullptr, cfg.c_str());
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
};

}  // namespace

/** THE CONTROL. With no "mode" key the pipeline succeeds. If this ever fails
 *  the cases below prove nothing -- they would be measuring a pipeline that
 *  rejects everything, which is what the first probe accidentally did. */
TEST_F(NonlinModeTest, ControlAPipelineWithNoModeKeySucceeds) {
    EXPECT_EQ(XPE_OK, runPipeline(""));
}

/** The operating modes the repository actually uses. "clinical" returned
 *  XPE_ERR_CONFIG_INVALID (-4) before QA-A-127 and failed the whole frame. */
TEST_F(NonlinModeTest, AnOperatingModeNoLongerFailsThePipeline) {
    EXPECT_EQ(XPE_OK, runPipeline(",\"mode\":\"clinical\""))
        << "\"clinical\" is an operating mode the repository uses; the "
           "nonlinearity stage must not reject it";
    EXPECT_EQ(XPE_OK, runPipeline(",\"mode\":\"research\""));
    EXPECT_EQ(XPE_OK, runPipeline(",\"mode\":\"production\""));
}

/** A name nobody uses fares the same. The point is not that the old list was
 *  missing entries -- it is that the stage reads no meaning from "mode" at
 *  all, so no list can be wrong again. */
TEST_F(NonlinModeTest, AnArbitraryModeNameIsAlsoAccepted) {
    EXPECT_EQ(XPE_OK, runPipeline(",\"mode\":\"zzz_not_a_mode\""));
}

/** THE FALSIFICATION. Passing silently is not the fix -- the caller cannot
 *  tell "corrected" from "nothing happened", because the frame is unchanged
 *  and the corrected flag is absent in both cases. Delete the alert and this
 *  fails; a test asserting only `rc` would not. */
TEST_F(NonlinModeTest, DoingNothingIsReported) {
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, runPipeline(",\"mode\":\"clinical\""));

    EXPECT_TRUE(alertContains("nonlinearity correction did nothing"))
        << "the stage passed the frame through unchanged and said nothing; "
           "silence is indistinguishable from a correction that ran";
    EXPECT_TRUE(alertContains("xpe_calib_load_nonlin_lut"))
        << "the alert must name what to do about it, not only that it happened";
}

/** The same is reported with no "mode" key at all -- the report is about the
 *  missing LUT, not about the mode. */
TEST_F(NonlinModeTest, DoingNothingIsReportedWithNoModeKeyEither) {
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, runPipeline(""));
    EXPECT_TRUE(alertContains("nonlinearity correction did nothing"));
}
