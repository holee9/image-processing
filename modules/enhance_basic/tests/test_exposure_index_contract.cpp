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
// RESOLVED by #154 / QA-B-148. The decision was taken: the defect was in the
// SPEC, not in this file -- REQ-ENH-023 itself said EI = EIT * (mean / S0), and
// the code implemented it faithfully. The SPEC was corrected to
// EI = K_cal * (mean / S0) with K_cal independent of the body part, and the
// code followed.
//
// EVERY CASE BELOW IS THEREFORE INVERTED, NOT DELETED. Each one said "say how,
// and retire this case" in its own failure message; the diff is what shows the
// cancellation leaving. What they now assert is the property the standard
// wants: DI moves with the body part, by an amount derived from the TABLE
// rather than from the code's own expression.

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

// INVERTED (#154). EI used to scale with the target -- that WAS the defect:
// the measurement carried its own reference inside it. EI must now be the same
// for every view, because it measures the dose that reached the detector and
// nothing else. The body-part table reaches DI instead, which is the next case.
TEST(ExposureIndexContractTest, Stage2_EiIsIndependentOfTheTarget_154) {
    const EiResult chest   = Measure("CHEST",   1000.0f);   // EIT 200
    const EiResult skull   = Measure("SKULL",   1000.0f);   // EIT 320

    ASSERT_EQ(XPE_OK, chest.rc);
    ASSERT_EQ(XPE_OK, skull.rc);
    GTEST_LOG_(INFO) << "CHEST EI=" << chest.ei << " DI=" << chest.di;
    GTEST_LOG_(INFO) << "SKULL EI=" << skull.ei << " DI=" << skull.di;

    EXPECT_FLOAT_EQ(chest.ei, skull.ei)
        << "EI still moves with the body part -- the target is back inside the "
           "measurement, which is #154";
    // EI = K_cal * (mean/S0) with mean == S0 == 1000, so EI lands on K_cal.
    EXPECT_FLOAT_EQ(100.0f, chest.ei);
    EXPECT_FLOAT_EQ(100.0f, skull.ei);
}

// INVERTED (#154). The finding was: same exposure, different views, identical
// DI. Now DI must differ, and by a quantity that does NOT come from the code:
// DI_a - DI_b = 10*log10(EIT_b / EIT_a), in which K_cal and mean/S0 cancel.
// That identity is the independent control -- it holds whatever K_cal is.
TEST(ExposureIndexContractTest, Stage2_DiSeparatesBodyPartsByTheTableRatio_154) {
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

    // Derived from the TABLE, not from the module: EIT CHEST 200, SKULL 320,
    // HAND 100.
    EXPECT_NEAR(10.0f * std::log10(320.0f / 200.0f), chest.di - skull.di, 1e-4f)
        << "CHEST-SKULL separation is not the table ratio -- either the "
           "cancellation is back, or the table changed";
    EXPECT_NEAR(10.0f * std::log10(100.0f / 200.0f), chest.di - hand.di, 1e-4f);

    // And the absolute value still follows K_cal = 100: DI(CHEST) =
    // 10*log10(100*(1234/1000) / 200).
    EXPECT_NEAR(10.0f * std::log10(100.0f * (1234.0f / 1000.0f) / 200.0f),
                chest.di, 1e-4f);
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

TEST(ExposureIndexContractTest, Stage2_DiSpreadsAcrossEveryBodyPart_154) {
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
    float diLo = ref.di, diHi = ref.di;
    int diffBits = 0;
    for (const char* p : parts) {
        const EiResult r = Measure(p, kFill);
        ASSERT_EQ(XPE_OK, r.rc) << p;
        eiLo = std::min(eiLo, r.ei);
        eiHi = std::max(eiHi, r.ei);
        const bool same = (Bits(r.di) == Bits(ref.di));
        if (!same) ++diffBits;
        diLo = std::min(diLo, r.di);
        diHi = std::max(diHi, r.di);
        GTEST_LOG_(INFO) << "  " << (p[0] ? p : "(empty)")
                         << "  EI=" << r.ei << "  DI=" << r.di
                         << "  DI bits " << (same ? "identical to CHEST" : "differ");
    }
    const float spread = diHi - diLo;
    GTEST_LOG_(INFO) << "  EI spans " << eiLo << ".." << eiHi << " (x"
                     << (eiHi / eiLo) << "), DI spread = " << spread
                     << " dB, entries differing from CHEST: " << diffBits
                     << " of " << (sizeof(parts) / sizeof(parts[0]));

    // G1 (#154, QA-B-148). INVERTED: this asserted bit-identity across every
    // body part, which was the cancellation. EI must now be flat and DI must
    // spread, and the spread is derived from the TABLE -- widest ratio
    // SKULL 320 : HAND 100 -- not from anything the module computes.
    EXPECT_FLOAT_EQ(eiLo, eiHi)
        << "EI still varies by body part -- the target is inside the "
           "measurement again (#154)";
    EXPECT_NEAR(10.0f * std::log10(320.0f / 100.0f), spread, 1e-4f)
        << "DI spread is " << spread << " dB; the shipped table's widest ratio "
           "requires " << (10.0f * std::log10(320.0f / 100.0f))
        << " dB. 0.00 means the cancellation is back.";

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

// INVERTED (#154). REQ-ENH-026's |DI| > 3 alert used to inherit the
// cancellation: the same exposure alerted for every view or for none. It must
// now SPLIT, and the exposure below is chosen so that it does -- with
// EI = 100*(6000/1000) = 600, DI is 7.78 for HAND (EIT 100) and 2.73 for SKULL
// (EIT 320), so one alerts and the other does not at the very same dose.
//
// That split is the clinical point of DI: the same exposure is too much for a
// hand and acceptable for a skull.
TEST(ExposureIndexContractTest, Stage2_TheAlertThresholdSeparatesBodyParts_154) {
    const char* parts[] = {"HAND", "CHEST", "SKULL"};   // EIT 100, 200, 320
    const float kFill = 6000.0f;

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

    // A SPLIT is now required. All-three or none is what the cancellation
    // produced, and either would mean the table stopped reaching the threshold.
    EXPECT_EQ(2, alerted)
        << alerted << " of 3 body parts alerted. HAND (7.78) and CHEST (4.77) "
           "must alert and SKULL (2.73) must not -- if all three or none did, "
           "the threshold is body-part independent again (#154)";
}

// G2 (#154, QA-B-148): the table reaches the output, measured as a RATIO
// between two views rather than as an absolute value.
//
// This is the case that could not exist before: with EI = EIT*(mean/S0) the
// difference below was identically zero for every pair, whatever the table
// said. It is written against the table constants -- 10*log10(EIT_b/EIT_a) --
// which is independent of K_cal, of S0_REFERENCE, and of the exposure, so it
// stays true when any of those are recalibrated (#151).
TEST(ExposureIndexContractTest, Stage2_EveryTableEntryReachesDi_154) {
    struct Entry { const char* part; float eit; };
    const Entry entries[] = {
        {"CHEST",   200.0f},
        {"HAND",    100.0f},
        {"FOOT",    100.0f},
        {"ABDOMEN", 250.0f},
        {"PELVIS",  250.0f},
        {"SPINE",   300.0f},
        {"SKULL",   320.0f},
    };

    const float kFill = 1234.0f;
    const EiResult ref = Measure(entries[0].part, kFill);   // CHEST
    ASSERT_EQ(XPE_OK, ref.rc);

    GTEST_LOG_(INFO) << "#154 G2 -- DI offset from CHEST vs the table ratio:";
    for (const Entry& e : entries) {
        const EiResult r = Measure(e.part, kFill);
        ASSERT_EQ(XPE_OK, r.rc) << e.part;

        // Independently derived: K_cal and mean/S0 cancel in the difference.
        const float expected = 10.0f * std::log10(entries[0].eit / e.eit);
        GTEST_LOG_(INFO) << "  " << e.part << " (EIT " << e.eit << ")  measured "
                         << (r.di - ref.di) << "  expected " << expected;
        EXPECT_NEAR(expected, r.di - ref.di, 1e-4f)
            << e.part << ": the table value is not reaching DI with the right "
                         "weight -- a zero offset means the cancellation is back";
    }

    // G3, kept in the same case so a change that kills the exposure axis while
    // restoring the body-part axis cannot pass. Doubling the dose must add
    // 10*log10(2) = 3.01 dB, for every view.
    const EiResult brighter = Measure(entries[0].part, kFill * 2.0f);
    ASSERT_EQ(XPE_OK, brighter.rc);
    GTEST_LOG_(INFO) << "  exposure x2: " << ref.di << " -> " << brighter.di
                     << " (delta " << (brighter.di - ref.di) << ")";
    EXPECT_NEAR(10.0f * std::log10(2.0f), brighter.di - ref.di, 1e-4f)
        << "doubling the exposure no longer moves DI by 3.01 dB -- the dose "
           "axis is broken, whatever the body-part axis does";
}
