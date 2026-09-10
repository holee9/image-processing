/**
 * @file test_json_helpers.cpp
 * @brief Pipeline bypass-flag parsing (#126).
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * The pipeline reads every bypass flag through xpe_json_get_string(), which
 * required the value to be quoted. A config written the ordinary way --
 * {"bypassOffset": true} -- was therefore ignored, and only the string form
 * {"bypassOffset": "true"} took effect. Nothing at the call site reported it:
 * the stage simply ran and failed later on missing calibration.
 *
 * The extractor is internal to the DLL and not exported, so these cases drive
 * it through the public entry point, which is also where the defect is visible
 * to a caller.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>

namespace {

class PipelineBypassTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4, H = 4;

    std::vector<uint16_t> pixels;
    XpeImageBuffer   img{};
    XpeImageMetadata meta{};

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

        pixels.assign(W * H, 1000);
        img.data          = pixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = pixels.size() * sizeof(uint16_t);
    }

    void TearDown() override { xpe_preprocess_shutdown(); }
};

// Every stage bypassed. With no calibration loaded, the run can only succeed if
// the flags were understood; otherwise offset correction runs and reports
// XPE_ERR_CALIB_NOT_LOADED.
TEST_F(PipelineBypassTest, JsonBooleanBypassFlagsAreHonoured) {
    const char* config =
        R"({"bypassReadout":true,"bypassTemp":true,"bypassOffset":true,)"
        R"("bypassNonlinearity":true,"bypassGain":true,"bypassBinning":true,)"
        R"("bypassDefect":true,"bypassGhost":true})";

    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
}

// The quoted form predates the fix and must keep working.
TEST_F(PipelineBypassTest, QuotedBypassFlagsStillHonoured) {
    const char* config =
        R"({"bypassReadout":"true","bypassTemp":"true","bypassOffset":"true",)"
        R"("bypassNonlinearity":"true","bypassGain":"true","bypassBinning":"true",)"
        R"("bypassDefect":"true","bypassGhost":"true"})";

    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
}

// Spacing after the colon must not change the outcome.
TEST_F(PipelineBypassTest, WhitespaceAroundBooleanIsTolerated) {
    const char* config =
        R"({"bypassReadout" : true , "bypassTemp" : true , "bypassOffset" : true ,)"
        R"("bypassNonlinearity" : true , "bypassGain" : true , "bypassBinning" : true ,)"
        R"("bypassDefect" : true , "bypassGhost" : true })";

    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
}

// false must not be read as "set"; offset correction then runs and reports the
// missing calibration rather than silently passing the image through.
TEST_F(PipelineBypassTest, JsonBooleanFalseDoesNotBypass) {
    const char* config =
        R"({"bypassReadout":true,"bypassTemp":true,"bypassOffset":false,)"
        R"("bypassNonlinearity":true,"bypassGain":true,"bypassBinning":true,)"
        R"("bypassDefect":true,"bypassGhost":true})";

    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
              xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
}

} // namespace
