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
#include <algorithm>
#include <cstdint>

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

// ===========================================================================
// #154 (QA-B-147): the sweep, the consequence, and the control.
//
// QA-B-56 measured two body parts. This widens it to EVERY entry of the
// shipped table plus the two default paths, compares DI BIT-FOR-BIT rather
// than with a float tolerance, and then measures the consequence the issue
// names: REQ-ENH-026's |DI| > 3 alert.
//
// Bit-for-bit matters here. The cancellation is exact in algebra, but the code
// computes ei = eit * (mean/S0) in float and then divides by eit again, so a
// rounding residue COULD survive and a tolerance-based comparison would hide
// whether it does. What survives is reported below, not assumed.
// ===========================================================================
namespace {

uint32_t Bits(float v) {
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    return u;
}

}  // namespace

TEST(ExposureIndexContractTest, KnownDivergence_DiIsBitIdenticalAcrossEveryBodyPart_154) {
    // Every entry of the shipped table, plus both routes to the default.
    const char* parts[] = {
        "CHEST", "HAND", "FOOT", "ABDOMEN", "PELVIS", "SPINE", "SKULL",
        "NOT_A_BODY_PART",   // unknown -> default 200
        "",                  // empty   -> default 200
    };

    const float kFill = 1234.0f;
    const EiResult ref = Measure(parts[0], kFill);
    ASSERT_EQ(XPE_OK, ref.rc);

    GTEST_LOG_(INFO) << "#154 body-part sweep, one axis, same 64x64 image (fill "
                     << kFill << "):";
    float eiLo = ref.ei, eiHi = ref.ei;
    int diffBits = 0;
    for (const char* p : parts) {
        const EiResult r = Measure(p, kFill);
        ASSERT_EQ(XPE_OK, r.rc) << p;
        eiLo = std::min(eiLo, r.ei);
        eiHi = std::max(eiHi, r.ei);
        const bool same = (Bits(r.di) == Bits(ref.di));
        if (!same) ++diffBits;
        GTEST_LOG_(INFO) << "  " << (p[0] ? p : "(empty)")
                         << "  EI=" << r.ei << "  DI=" << r.di
                         << "  DI bits " << (same ? "identical" : "DIFFER");
        EXPECT_EQ(Bits(ref.di), Bits(r.di))
            << p << ": DI now differs by body part -- the EIT cancellation has "
                    "been fixed; say how, and retire this case";
    }
    GTEST_LOG_(INFO) << "  EI spans " << eiLo << ".." << eiHi << " (x"
                     << (eiHi / eiLo) << "), DI differing entries: " << diffBits
                     << " of " << (sizeof(parts) / sizeof(parts[0]));

    // THE CONTROL. Without it "DI does not move" is an absence assertion with
    // nothing showing the measurement can see anything at all. The axis the
    // code DOES respond to is the exposure, so move that instead.
    const EiResult brighter = Measure(parts[0], kFill * 2.0f);
    ASSERT_EQ(XPE_OK, brighter.rc);
    GTEST_LOG_(INFO) << "  control -- same body part, exposure x2: DI "
                     << ref.di << " -> " << brighter.di;
    EXPECT_NE(Bits(ref.di), Bits(brighter.di))
        << "DI does not move when the EXPOSURE changes either -- then this "
           "measurement is blind and proves nothing about the body part";
}

// The consequence the issue names: REQ-ENH-026's alert threshold inherits the
// cancellation, so the same exposure alerts for every view or for none.
//
// The exposure below is chosen so that the answer WOULD differ if EIT reached
// DI: at mean 3000, DI = 10*log10(3000/1000) = 4.77 for every part, but
// against the targets themselves -- 10*log10(EI/EIT) with EI independent of
// EIT -- HAND (EIT 100) and SKULL (EIT 320) would sit half a decade apart.
TEST(ExposureIndexContractTest, KnownDivergence_TheAlertThresholdIsBodyPartIndependent_154) {
    const char* parts[] = {"HAND", "CHEST", "SKULL"};   // EIT 100, 200, 320
    const float kFill = 3000.0f;

    int alerted = 0;
    for (const char* p : parts) {
        xpe_clear_alerts();
        const EiResult r = Measure(p, kFill);
        ASSERT_EQ(XPE_OK, r.rc) << p;
        const int n = xpe_get_pending_alert_count();
        if (n > 0) ++alerted;
        GTEST_LOG_(INFO) << "  " << p << "  EI=" << r.ei << "  DI=" << r.di
                         << "  alerts=" << n;
    }
    xpe_clear_alerts();

    // All three or none -- never a split. A split is what a body-part-aware
    // threshold would produce, and it is exactly what #154 says cannot happen.
    EXPECT_TRUE(alerted == 0 || alerted == static_cast<int>(sizeof(parts) / sizeof(parts[0])))
        << alerted << " of 3 body parts alerted -- the threshold has become "
                      "body-part dependent; say how, and retire this case";
    EXPECT_EQ(3, alerted)
        << "at this exposure every view should alert, because DI = "
           "10*log10(mean/S0) = 4.77 regardless of the view";
}
