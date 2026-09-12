// #153 (QA-B-56): what REQ-ENH-023 actually computes, measured.
//
// REQ-ENH-023 specifies  EI = EIT * (mean / S0_reference)
// REQ-ENH-024 specifies  DI = 10 * log10(EI / EIT)
//
// Substituting the first into the second, EIT cancels:
//
//     DI = 10 * log10( EIT * (mean/S0) / EIT ) = 10 * log10(mean / S0)
//
// So the body-part lookup REQ-ENH-025 mandates has no effect on DI at all, and
// EI scales with the TARGET rather than being compared against it. In
// IEC 62494-1 -- which both this module and enhance_advanced name -- EI measures
// the exposure reaching the detector and EI_T is the separate target for the
// view; DI exists precisely to compare two independent quantities.
//
// That is algebra. These cases are the measurement, because algebra about code
// is still a claim about code: the same uniform image is read twice with
// different body parts, and the outputs are compared.
//
// NOTHING IS FIXED HERE. Changing the formula changes a reported clinical
// number, which is a decision, not a test (QA-B-56 card). These cases pin what
// the code does today so the decision arrives as a visible change.

#include <gtest/gtest.h>

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

namespace {

struct EiResult {
    XpeErrorCode rc = XPE_OK;
    float ei = 0.0f;
    float di = 0.0f;
};

EiResult Measure(const char* bodyPart, float fill) {
    constexpr uint32_t kW = 64, kH = 64;
    std::vector<float> pixels(static_cast<size_t>(kW) * kH, fill);

    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data          = pixels.data();
    img.dataSize      = static_cast<uint32_t>(pixels.size() * sizeof(float));

    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", bodyPart);

    EiResult r;
    r.rc = xpe_calc_exposure_index(&img, &meta, &r.ei, &r.di);
    return r;
}

}  // namespace

// The body-part lookup moves EI. Recorded so the next case is unambiguous: the
// table IS consulted, so a DI that does not move cannot be blamed on the lookup
// being skipped.
TEST(ExposureIndexContractTest, KnownDivergence_EiScalesWithTheTarget) {
    const EiResult chest   = Measure("CHEST",   1000.0f);   // EIT 200
    const EiResult skull   = Measure("SKULL",   1000.0f);   // EIT 320

    ASSERT_EQ(XPE_OK, chest.rc);
    ASSERT_EQ(XPE_OK, skull.rc);
    GTEST_LOG_(INFO) << "CHEST EI=" << chest.ei << " DI=" << chest.di;
    GTEST_LOG_(INFO) << "SKULL EI=" << skull.ei << " DI=" << skull.di;

    EXPECT_NE(chest.ei, skull.ei)
        << "the EIT table is not being consulted at all, which would make the "
           "next case say nothing";
    // EI = EIT * (mean/S0) with mean == S0 == 1000, so EI lands exactly on EIT.
    EXPECT_FLOAT_EQ(200.0f, chest.ei);
    EXPECT_FLOAT_EQ(320.0f, skull.ei);
}

// The finding. Same exposure, two body parts, identical DI -- because the
// target cancels out of the formula pair.
TEST(ExposureIndexContractTest, KnownDivergence_DiIsIndependentOfBodyPart) {
    const EiResult chest = Measure("CHEST", 1234.0f);
    const EiResult skull = Measure("SKULL", 1234.0f);
    const EiResult hand  = Measure("HAND",  1234.0f);   // EIT 100, the extreme

    ASSERT_EQ(XPE_OK, chest.rc);
    ASSERT_EQ(XPE_OK, skull.rc);
    ASSERT_EQ(XPE_OK, hand.rc);
    GTEST_LOG_(INFO) << "same exposure, DI by body part:"
                     << " CHEST=" << chest.di
                     << " SKULL=" << skull.di
                     << " HAND="  << hand.di;

    EXPECT_FLOAT_EQ(chest.di, skull.di)
        << "DI now differs by body part -- the EIT cancellation has been fixed; "
           "say how, and retire this case";
    EXPECT_FLOAT_EQ(chest.di, hand.di);

    // And it equals the body-part-free value, which is what the cancellation
    // leaves behind: DI = 10 * log10(mean / S0_REFERENCE), S0_REFERENCE = 1000.
    EXPECT_NEAR(10.0f * std::log10(1234.0f / 1000.0f), chest.di, 1e-4f);
}
