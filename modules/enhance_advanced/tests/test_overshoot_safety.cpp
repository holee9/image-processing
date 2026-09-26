// T-068 / AC-EDGE-005 (REQ-ADV-051, SAF-100) -- #145, QA-B-150.
//
// WHY THIS FILE DID NOT EXIST, AND WHY THAT MATTERED.
//
// tasks.md:82 listed this file as `pending`. The requirement it covers names
// one config key verbatim:
//
//   acceptance.md:205, AC-EDGE-005
//     Given a 512x512 FLOAT32 image
//     And a config JSON with "overshoot_limit_enabled": false
//     When xpe_fractional_process(img, 1.0, config) is called
//     Then the function returns XPE_ERR_SAFETY_VIOLATION
//
// That key was NOT in the product's forbidden list. The list held five other
// names, none of which the SPEC mentions anywhere. nlohmann::json::contains is
// an exact match, so "overshoot_limit" never covered "overshoot_limit_enabled"
// -- the prefix only looks as though it would. The requirement, the
// implementation and the test were empty at the same name simultaneously, so
// reading any one of the three showed nothing.
//
// THE KEYS BELOW ARE TAKEN FROM THE AC TEXT, NOT FROM THE PRODUCT'S LIST.
// T305_AttemptDisableOvershootLimiting in test_edge_enhancement.cpp is written
// the other way round -- its five attempts are the five names the code already
// had -- which is why it was green throughout and could not see this. A test
// derived from the implementation asks "does the code do what the code does".
//
// The control matters as much as the assertion: an implementation that
// rejected EVERY config would satisfy every rejection case here. So a normal
// key must still be accepted, in the same run, through the same entry point.

#include <gtest/gtest.h>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 512;   // the size AC-EDGE-005 states
constexpr uint32_t kH = 512;

class OvershootSafety : public ::testing::Test {
protected:
    std::vector<float> pixels_;

    // Without this every call returns XPE_ERR_NOT_INITIALIZED (-6) and every
    // rejection case below would fail for a reason that has nothing to do with
    // the safety guard. Observed while writing this file.
    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_enhance_advanced_init(nullptr));
    }

    XpeImageBuffer Image() {
        pixels_.assign(static_cast<size_t>(kW) * kH, 0.5f);
        XpeImageBuffer img{};
        img.width         = kW;
        img.height        = kH;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.data          = pixels_.data();
        img.dataSize      = static_cast<uint32_t>(pixels_.size() * sizeof(float));
        return img;
    }

    XpeErrorCode Run(const std::string& config) {
        XpeImageBuffer img = Image();
        return xpe_fractional_process(&img, 1.0f, config.c_str());
    }
};

}  // namespace

// AC-EDGE-005 as written. One case, one key, quoted from the requirement.
TEST_F(OvershootSafety, AcEdge005_OvershootLimitEnabledFalseIsRejected_145) {
    EXPECT_EQ(XPE_ERR_SAFETY_VIOLATION, Run(R"({"overshoot_limit_enabled": false})"))
        << "AC-EDGE-005 names this key verbatim and the product must reject it. "
           "An exact-match list that holds \"overshoot_limit\" does NOT cover "
           "\"overshoot_limit_enabled\".";
}

// The value must not be the thing that decides. A caller who writes `true`
// is still configuring a setting the SPEC says is not configurable.
TEST_F(OvershootSafety, AcEdge005_TheKeyIsRejectedWhateverItsValue_145) {
    for (const char* cfg : {R"({"overshoot_limit_enabled": false})",
                            R"({"overshoot_limit_enabled": true})",
                            R"({"overshoot_limit_enabled": 0})"}) {
        EXPECT_EQ(XPE_ERR_SAFETY_VIOLATION, Run(cfg))
            << "presence of the key is the violation, not its value: " << cfg;
    }
}

// Nested "safety" object, the path the product also checks.
TEST_F(OvershootSafety, AcEdge005_KeyInsideTheSafetyObjectIsRejected_145) {
    EXPECT_EQ(XPE_ERR_SAFETY_VIOLATION,
              Run(R"({"safety": {"overshoot_limit_enabled": false}})"))
        << "the nested path must reject the AC's key too";
}

// Regression: the five names the implementation already carried keep working.
// Listed here as well so that a future tidy-up of the product list turns THIS
// file red -- the one written from the requirement -- and not only the file
// written from the list.
TEST_F(OvershootSafety, EveryForbiddenNameIsRejected_145) {
    const std::vector<std::string> configs = {
        R"({"overshoot_limit_enabled": false})",   // AC-EDGE-005
        R"({"overshoot_limiting": false})",
        R"({"overshoot_limit": false})",
        R"({"overshoot_factor": 10.0})",
        R"({"disable_overshoot_limit": true})",
        R"({"overshoot": false})",
        R"({"safety": {"overshoot": false}})",
    };
    for (const std::string& cfg : configs) {
        EXPECT_EQ(XPE_ERR_SAFETY_VIOLATION, Run(cfg)) << cfg;
    }
}

// THE CONTROL. Without it every assertion above is satisfied by a product that
// rejects all configuration, and "the safety guard works" would be
// indistinguishable from "nothing is configurable".
TEST_F(OvershootSafety, OrdinaryConfigStillPasses_145) {
    for (const char* cfg : {R"({"iterations": 3})",
                            R"({"safety": {"iterations": 3}})",
                            R"({})"}) {
        EXPECT_EQ(XPE_OK, Run(cfg))
            << "a config with no forbidden key was rejected: " << cfg;
    }

    // And a key that merely CONTAINS a forbidden name as a substring is not
    // itself forbidden -- the match is exact by design (prefix matching was
    // considered and rejected, see enhance_advanced_helpers.cpp). This pins
    // that decision so a later switch to prefix matching is visible here.
    EXPECT_EQ(XPE_OK, Run(R"({"overshoot_limit_enabled_note": "text"})"))
        << "exact matching is the contract; this key is not on the list";
}
