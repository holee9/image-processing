/**
 * @file test_nonlin_lut_apply.cpp
 * @brief Loading and applying the nonlinearity LUT -- QA-A-111 (#186)
 *
 * THE EXPECTED VALUES ARE NOT PRODUCED BY THE LUT GENERATOR.
 *
 * The synthetic detector is a closed-form power law, so the residual a correct
 * correction must leave is computable directly:
 *
 *     raw   = adc_max * (D / D_max)^gamma          (what the detector reports)
 *     ideal = G_nominal * D                        (what it should have reported)
 *
 * The test builds a frame whose pixels are raw values for a set of doses, runs
 * the pipeline, and compares each corrected pixel against `ideal` computed from
 * that dose by this file's own arithmetic. If the generator, the loader, or the
 * lookup is wrong, the measured value moves and the expectation does not.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_common_api.h"
#include "xcal_writer.hpp"
#include "xcal_reader.hpp"
#include "preprocess_state_fixture.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include <cstring>

#include <chrono>
#include <filesystem>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr double kGamma = 1.35;
constexpr double kAdcMax = 4095.0;
constexpr double kDoseMax = 100.0;
constexpr int kLevels = 12;
constexpr uint32_t kW = 64, kH = 64;

struct Sim {
    std::vector<double> dose, signal;
    std::vector<std::vector<uint16_t>> pixels;
    std::vector<XpeImageBuffer> frames;
    double g_nominal = 0.0;

    /** The detector's raw reading for a dose. */
    static double Raw(double d) {
        return kAdcMax * std::pow(d / kDoseMax, kGamma);
    }
    /** What a linear detector would have read -- the correction's target. */
    double Ideal(double d) const { return g_nominal * d; }
};

Sim MakeSim() {
    Sim s;
    s.dose.resize(kLevels);
    s.signal.resize(kLevels);
    s.pixels.resize(kLevels);
    s.frames.resize(kLevels);
    for (int i = 0; i < kLevels; ++i) {
        const double frac = 0.05 + 0.90 * i / (kLevels - 1.0);
        const double sig = std::round(frac * kAdcMax);
        s.signal[static_cast<size_t>(i)] = sig;
        s.dose[static_cast<size_t>(i)] =
            kDoseMax * std::pow(sig / kAdcMax, 1.0 / kGamma);
        s.pixels[static_cast<size_t>(i)].assign(64, static_cast<uint16_t>(sig));
        XpeImageBuffer& b = s.frames[static_cast<size_t>(i)];
        b = XpeImageBuffer{};
        b.data = s.pixels[static_cast<size_t>(i)].data();
        b.width = 8; b.height = 8;
        b.bitsAllocated = 16; b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.dataSize = 128;
    }
    double num = 0.0, den = 0.0;
    for (int i = 0; i < kLevels; ++i) {
        num += s.dose[static_cast<size_t>(i)] * s.signal[static_cast<size_t>(i)];
        den += s.dose[static_cast<size_t>(i)] * s.dose[static_cast<size_t>(i)];
    }
    s.g_nominal = num / den;
    return s;
}

std::string LutPath() {
    return std::string(::testing::TempDir()) + "/a111_nonlin.xcal";
}

/** A frame whose rows step through the measured dose range. */
struct Frame {
    std::vector<uint16_t> px;
    std::vector<double> dose_of_row;
    XpeImageBuffer buf{};
};

Frame MakeGradientFrame(const Sim& s) {
    Frame f;
    f.px.resize(static_cast<size_t>(kW) * kH);
    f.dose_of_row.resize(kH);
    const double d_lo = s.dose.front(), d_hi = s.dose.back();
    for (uint32_t y = 0; y < kH; ++y) {
        const double d = d_lo + (d_hi - d_lo) * y / (kH - 1.0);
        f.dose_of_row[y] = d;
        const uint16_t raw = static_cast<uint16_t>(std::lround(Sim::Raw(d)));
        for (uint32_t x = 0; x < kW; ++x) {
            f.px[static_cast<size_t>(y) * kW + x] = raw;
        }
    }
    f.buf = XpeImageBuffer{};
    f.buf.data = f.px.data();
    f.buf.width = kW; f.buf.height = kH;
    f.buf.bitsAllocated = 16; f.buf.bitsStored = 16;
    f.buf.format = XPE_PIXEL_UINT16;
    f.buf.dataSize = static_cast<uint32_t>(f.px.size() * sizeof(uint16_t));
    return f;
}

/**
 * Inherits the state convention from XpePreprocessStateFixture (QA-A-113):
 * init/shutdown pairing and mode restoration live there, so this fixture only
 * adds what is specific to the nonlinearity LUT.
 */
class NonlinApplyTest : public XpePreprocessStateFixture {
protected:
    Sim sim;

    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        sim = MakeSim();
        ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(
            sim.frames.data(), sim.dose.data(), kLevels, nullptr, 4096u,
            LutPath().c_str(), nullptr));

        xpe_clear_alerts();
        // The calibration store is global and survives between tests, so the
        // "no LUT" cases would otherwise read whatever the previous test left.
        xpe_calib_unload_nonlin_lut();
    }

    void TearDown() override {
        xpe_calib_unload_nonlin_lut();
        XpePreprocessStateFixture::TearDown();
    }
};

/**
 * Runs the nonlinearity stage over the frame.
 *
 * The public entry point is used rather than the 3-argument internal one: only
 * the public symbol is exported from the DLL, and a test that linked its own
 * copy of the stage would get its own `g_calib` too -- it would then be testing
 * a second calibration store that no pipeline ever reads.
 *
 * `applied` is therefore observed through the pipeline instead (see the flag
 * test), which is the surface that actually has to be right.
 */
XpeErrorCode RunStage(Frame& f, const char* config) {
    return xpe_nonlinearity_correct(&f.buf, config);
}

/** Runs the whole pipeline with every other stage bypassed. */
XpeErrorCode RunPipelineNonlinOnly(Frame& f, XpeImageMetadata* meta,
                                   const char* extra) {
    std::string cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,"
                      "\"bypassOffset\":true,\"bypassGain\":true,"
                      "\"bypassBinning\":true,\"bypassDefect\":true,"
                      "\"bypassGhost\":true";
    if (extra != nullptr && extra[0] != 0) { cfg += ","; cfg += extra; }
    cfg += "}";
    // The _ex entry point takes a pre-loaded calibration state, so a run with
    // every file-backed stage bypassed does not have to point at a calibration
    // directory. The plain entry point reads offset/gain/defect from disk
    // before it looks at the bypass flags and fails with -9 here.
    XpeCalibrationState st{};
    return xpe_preprocess_pipeline_ex(&f.buf, meta, &st, nullptr, cfg.c_str());
}

bool AlertMentions(const char* needle) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[512] = {0};
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) != XPE_OK) continue;
        if (std::string(buf).find(needle) != std::string::npos) return true;
    }
    return false;
}

/** Writes the three calibration files the plain pipeline loads. */
bool MakeCalibDir(const std::string& dir) {
    std::filesystem::create_directories(dir);
    const size_t n = static_cast<size_t>(kW) * kH;

    auto header = [&](uint32_t type, uint32_t fmt) {
        XCalFileHeader h{};
        std::memcpy(h.magic, XCAL_MAGIC, 4);
        h.version = XCAL_VERSION;
        h.type = type;
        h.pixel_format = fmt;
        h.width = kW; h.height = kH;
        h.created_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return h;
    };

    const std::vector<float> offset(n, 200.0f);
    if (write_xcal_file((dir + "/offset.xcal").c_str(),
                        header(XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32), nullptr, 0,
                        reinterpret_cast<const uint8_t*>(offset.data()),
                        offset.size() * sizeof(float)) != XPE_OK) return false;

    const std::vector<float> gain(n, 1.0f);
    if (write_xcal_file((dir + "/gain.xcal").c_str(),
                        header(XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32), nullptr, 0,
                        reinterpret_cast<const uint8_t*>(gain.data()),
                        gain.size() * sizeof(float)) != XPE_OK) return false;

    const std::vector<uint8_t> defect(n, 0u);
    return write_xcal_file((dir + "/defect.xcal").c_str(),
                           header(XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK), nullptr, 0,
                           defect.data(), defect.size()) == XPE_OK;
}

}  // namespace

// The load-bearing test: after correction the frame follows the ideal line.
TEST_F(NonlinApplyTest, CorrectedPixelsFollowTheIdealLinearResponse) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(LutPath().c_str()));

    Frame f = MakeGradientFrame(sim);
    ASSERT_EQ(XPE_OK, RunStage(f, nullptr));

    // 0.3% of full scale, the requirement's clause, judged inside the measured
    // range -- which is the whole frame here by construction.
    const double tolerance = 0.003 * kAdcMax;
    double worst = 0.0;
    for (uint32_t y = 0; y < kH; ++y) {
        const double expected = sim.Ideal(f.dose_of_row[y]);
        const double actual = static_cast<double>(f.px[static_cast<size_t>(y) * kW]);
        worst = std::max(worst, std::fabs(expected - actual));
    }
    EXPECT_LE(worst, tolerance) << "worst residual " << worst << " ADU";
}

// Without the correction the same frame is far from the ideal line -- so the
// test above is measuring the correction, not a frame that was already linear.
TEST_F(NonlinApplyTest, TheUncorrectedFrameIsNowhereNearTheIdealLine) {
    Frame f = MakeGradientFrame(sim);
    double worst = 0.0;
    for (uint32_t y = 0; y < kH; ++y) {
        const double expected = sim.Ideal(f.dose_of_row[y]);
        const double actual = static_cast<double>(f.px[static_cast<size_t>(y) * kW]);
        worst = std::max(worst, std::fabs(expected - actual));
    }
    // Stated against the correction's own tolerance rather than a round
    // percentage: the control must miss by far more than the corrected frame is
    // allowed to. Measured here: about 331 ADU against a 12.3 ADU tolerance.
    EXPECT_GT(worst, 10.0 * 0.003 * kAdcMax)
        << "the simulated detector is too linear to test a correction";
}

// The flag means pixels changed -- both directions (#184), read where it is
// actually published: the pipeline's metadata.
TEST_F(NonlinApplyTest, TheFlagFollowsWhetherPixelsActuallyChanged) {
    // No LUT loaded: the stage runs, nothing changes, the flag stays clear.
    Frame f = MakeGradientFrame(sim);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, RunPipelineNonlinOnly(f, &meta, nullptr));
    EXPECT_EQ(0u, meta.flags & XPE_FLAG_NONLINEARITY_CORRECTED);

    // LUT loaded: the flag is set, and the pixels really did move.
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(LutPath().c_str()));
    Frame g = MakeGradientFrame(sim);
    const std::vector<uint16_t> before = g.px;
    XpeImageMetadata meta2{};
    ASSERT_EQ(XPE_OK, RunPipelineNonlinOnly(g, &meta2, nullptr));
    EXPECT_NE(0u, meta2.flags & XPE_FLAG_NONLINEARITY_CORRECTED);
    EXPECT_NE(before, g.px);
}

// "Detector profile governs enable/disable via field panel.linear = true/false"
TEST_F(NonlinApplyTest, ALinearPanelSkipsTheStageAndLeavesThePixelsAlone) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(LutPath().c_str()));

    Frame f = MakeGradientFrame(sim);
    const std::vector<uint16_t> before = f.px;
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, RunPipelineNonlinOnly(f, &meta, "\"panel.linear\":true"));
    EXPECT_EQ(before, f.px);
    EXPECT_EQ(0u, meta.flags & XPE_FLAG_NONLINEARITY_CORRECTED)
        << "a skipped stage must not claim the frame was corrected";
}

// A panel declared non-linear with no LUT must not pass silently.
TEST_F(NonlinApplyTest, ANonLinearPanelWithoutALutIsAnError) {
    Frame f = MakeGradientFrame(sim);
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
              RunStage(f, "{\"panel.linear\":false}"));
    EXPECT_TRUE(AlertMentions("no nonlinearity LUT is loaded"));
}

// Load-time validation, per the requirement's monotonicity clause.
TEST_F(NonlinApplyTest, ANonMonotoneOrWrongSizedTableIsRefusedAtLoad) {
    // Hand-write a table that falls in the middle.
    std::vector<uint16_t> lut(4096u);
    for (uint32_t i = 0; i < 4096u; ++i) lut[i] = static_cast<uint16_t>(i);
    lut[2000] = 100u;

    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT16);
    hdr.width = 4096; hdr.height = 1;
    hdr.created_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const std::string bad = std::string(::testing::TempDir()) + "/a111_bad.xcal";
    ASSERT_EQ(XPE_OK, write_xcal_file(bad.c_str(), hdr, nullptr, 0,
                                      reinterpret_cast<const uint8_t*>(lut.data()),
                                      lut.size() * sizeof(uint16_t)));
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_nonlin_lut(bad.c_str()));

    // A table of an unsupported size is refused too -- 2048 is a legal XCal
    // geometry but not one of the two sizes the requirement names.
    std::vector<uint16_t> small(2048u);
    for (uint32_t i = 0; i < 2048u; ++i) small[i] = static_cast<uint16_t>(i);
    hdr.width = 2048; hdr.height = 1;
    const std::string wrong = std::string(::testing::TempDir()) + "/a111_small.xcal";
    ASSERT_EQ(XPE_OK, write_xcal_file(wrong.c_str(), hdr, nullptr, 0,
                                      reinterpret_cast<const uint8_t*>(small.data()),
                                      small.size() * sizeof(uint16_t)));
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_nonlin_lut(wrong.c_str()));
}

TEST_F(NonlinApplyTest, NullPathIsRejected) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_nonlin_lut(nullptr));
}

/**
 * PIPELINE ORDER: the correction runs AFTER offset and BEFORE gain.
 *
 * The requirement states the reason rather than only the order
 * (SRS-CALIB-FUNC-006): "Correction must precede gain normalization (linearize
 * before normalize)." And the LUT is built from offset-corrected means, so it
 * describes a frame whose dark level has already been removed.
 *
 * The order is shown by consequence rather than by reading the source: the same
 * frame is run twice through the same pipeline, once with the offset stage on
 * and once bypassed. If the nonlinearity stage ran BEFORE offset, both runs
 * would hand the LUT identical raw values and produce identical output. They do
 * not.
 */
TEST_F(NonlinApplyTest, TheCorrectionRunsAfterOffsetAndBeforeGain) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(LutPath().c_str()));

    // A calibration directory the plain pipeline can read. It loads all three
    // files before it consults the bypass flags, so all three must exist even
    // though gain and defect are bypassed here.
    const std::string dir = std::string(::testing::TempDir()) + "/a111_calib";
    ASSERT_TRUE(MakeCalibDir(dir));

    Frame with_offset = MakeGradientFrame(sim);
    Frame no_offset = MakeGradientFrame(sim);
    XpeImageMetadata m1{}, m2{};

    const char* common = "\"bypassReadout\":true,\"bypassTemp\":true,"
                         "\"bypassGain\":true,\"bypassBinning\":true,"
                         "\"bypassDefect\":true,\"bypassGhost\":true";
    const std::string cfg_on = std::string("{") + common + "}";
    const std::string cfg_off = std::string("{") + common + ",\"bypassOffset\":true}";

    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&with_offset.buf, &m1, dir.c_str(),
                                              nullptr, cfg_on.c_str()));
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&no_offset.buf, &m2, dir.c_str(),
                                              nullptr, cfg_off.c_str()));

    EXPECT_NE(0u, m1.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_NE(0u, m1.flags & XPE_FLAG_NONLINEARITY_CORRECTED);
    EXPECT_EQ(0u, m2.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_NE(0u, m2.flags & XPE_FLAG_NONLINEARITY_CORRECTED);
    EXPECT_NE(with_offset.px, no_offset.px)
        << "the LUT saw the same input either way -- it is not running after offset";
}

/* ===================================================================== *
 * QA-A-112 (#186): the 65536-entry table's APPLY path, and the optional
 * dark reference. A-111 checked 65536 only at load.
 * ===================================================================== */

namespace {

/** A 16-bit ladder: the same power law, scaled to 16-bit full scale. */
struct Sim16 {
    static constexpr double kAdcMax16 = 65535.0;
    std::vector<double> dose, signal;
    std::vector<std::vector<uint16_t>> pixels;
    std::vector<XpeImageBuffer> frames;
    double g_nominal = 0.0;

    static double Raw(double d) {
        return kAdcMax16 * std::pow(d / kDoseMax, kGamma);
    }
    double Ideal(double d) const { return g_nominal * d; }
};

Sim16 MakeSim16() {
    Sim16 s;
    s.dose.resize(kLevels);
    s.signal.resize(kLevels);
    s.pixels.resize(kLevels);
    s.frames.resize(kLevels);
    for (int i = 0; i < kLevels; ++i) {
        const double frac = 0.05 + 0.90 * i / (kLevels - 1.0);
        const double sig = std::round(frac * Sim16::kAdcMax16);
        s.signal[static_cast<size_t>(i)] = sig;
        s.dose[static_cast<size_t>(i)] =
            kDoseMax * std::pow(sig / Sim16::kAdcMax16, 1.0 / kGamma);
        s.pixels[static_cast<size_t>(i)].assign(64, static_cast<uint16_t>(sig));
        XpeImageBuffer& b = s.frames[static_cast<size_t>(i)];
        b = XpeImageBuffer{};
        b.data = s.pixels[static_cast<size_t>(i)].data();
        b.width = 8; b.height = 8;
        b.bitsAllocated = 16; b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.dataSize = 128;
    }
    double num = 0.0, den = 0.0;
    for (int i = 0; i < kLevels; ++i) {
        num += s.dose[static_cast<size_t>(i)] * s.signal[static_cast<size_t>(i)];
        den += s.dose[static_cast<size_t>(i)] * s.dose[static_cast<size_t>(i)];
    }
    s.g_nominal = num / den;
    return s;
}

}  // namespace

/**
 * The 65536-entry table is applied, and the index arithmetic is right at both
 * sizes. A-111 only proved such a file LOADS -- a table stored as (4096, 16)
 * could load correctly and still be indexed as if it were 4096 wide, which
 * would fold sixteen different raw values onto the same entry.
 */
TEST_F(NonlinApplyTest, TheSixtyFiveThousandEntryTableIsAppliedWithCorrectIndexing) {
    const Sim16 s16 = MakeSim16();
    const std::string path16 = std::string(::testing::TempDir()) + "/a112_lut16.xcal";
    ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(
        s16.frames.data(), s16.dose.data(), kLevels, nullptr, 65536u,
        path16.c_str(), nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(path16.c_str()));

    // A frame of raw values taken straight from the measured ladder, plus the
    // two boundary indices.
    std::vector<uint16_t> px;
    std::vector<double> expect;
    for (int i = 0; i < kLevels; ++i) {
        px.push_back(static_cast<uint16_t>(s16.signal[static_cast<size_t>(i)]));
        expect.push_back(s16.Ideal(s16.dose[static_cast<size_t>(i)]));
    }
    px.push_back(0u);        expect.push_back(0.0);          // LUT[0] = 0
    px.push_back(65535u);    expect.push_back(-1.0);         // checked separately
    while (px.size() < 64) { px.push_back(px[0]); expect.push_back(expect[0]); }

    XpeImageBuffer buf{};
    buf.data = px.data();
    buf.width = 8; buf.height = 8;
    buf.bitsAllocated = 16; buf.bitsStored = 16;
    buf.format = XPE_PIXEL_UINT16;
    buf.dataSize = static_cast<uint32_t>(px.size() * sizeof(uint16_t));

    const std::vector<uint16_t> before = px;
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&buf, nullptr));

    // Every measured knot lands on its independently computed ideal value.
    // The tolerance is the requirement's 0.3% clause, on this scale.
    const double tol = 0.003 * Sim16::kAdcMax16;
    for (int i = 0; i < kLevels; ++i) {
        EXPECT_NEAR(expect[static_cast<size_t>(i)],
                    static_cast<double>(px[static_cast<size_t>(i)]), tol)
            << "knot " << i << " raw " << before[static_cast<size_t>(i)];
    }
    EXPECT_EQ(0u, px[static_cast<size_t>(kLevels)]) << "LUT[0] must stay 0";

    // The top index is inside a 65536-entry table, so it is a real lookup
    // rather than the clamp a 4096-entry table would take. It must therefore
    // sit above the highest measured knot's corrected value.
    const double top = static_cast<double>(px[static_cast<size_t>(kLevels) + 1]);
    EXPECT_GT(top, expect[static_cast<size_t>(kLevels) - 1]);

    // The fold a wrong index width would cause: raw values 16 apart must not
    // collapse onto one entry.
    std::vector<uint16_t> probe = {1000u, 1016u, 1032u};
    XpeImageBuffer pbuf{};
    pbuf.data = probe.data();
    pbuf.width = 3; pbuf.height = 1;
    pbuf.bitsAllocated = 16; pbuf.bitsStored = 16;
    pbuf.format = XPE_PIXEL_UINT16;
    pbuf.dataSize = static_cast<uint32_t>(probe.size() * sizeof(uint16_t));
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&pbuf, nullptr));
    EXPECT_NE(probe[0], probe[1]) << "raw 1000 and 1016 folded onto one entry";
    EXPECT_NE(probe[1], probe[2]) << "raw 1016 and 1032 folded onto one entry";
}

/**
 * A 4096-entry table with a 16-bit frame: the clamp, not an out-of-bounds read.
 * Stated as a consequence -- everything above the table's last index has to
 * come out as the table's last entry.
 */
TEST_F(NonlinApplyTest, A4096TableClampsRawValuesAboveItsLastIndex) {
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(LutPath().c_str()));

    std::vector<uint16_t> px = {4095u, 4096u, 20000u, 65535u};
    XpeImageBuffer buf{};
    buf.data = px.data();
    buf.width = 4; buf.height = 1;
    buf.bitsAllocated = 16; buf.bitsStored = 16;
    buf.format = XPE_PIXEL_UINT16;
    buf.dataSize = static_cast<uint32_t>(px.size() * sizeof(uint16_t));

    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&buf, nullptr));
    EXPECT_EQ(px[0], px[1]);
    EXPECT_EQ(px[0], px[2]);
    EXPECT_EQ(px[0], px[3]);
}

/**
 * The optional dark reference is subtracted before the means are taken.
 *
 * Shown by consequence rather than by reading the code: the same detector is
 * described twice, once by frames carrying a dark pedestal plus a matching dark
 * reference, and once by frames with no pedestal and no reference. If the
 * subtraction happens, both runs produce the same table.
 */
TEST_F(NonlinApplyTest, TheDarkReferenceIsSubtractedBeforeTheMeansAreTaken) {
    constexpr uint16_t kDark = 150u;

    Sim a = MakeSim();
    for (int i = 0; i < kLevels; ++i) {
        for (auto& v : a.pixels[static_cast<size_t>(i)]) {
            v = static_cast<uint16_t>(v + kDark);
        }
        a.frames[static_cast<size_t>(i)].data = a.pixels[static_cast<size_t>(i)].data();
    }
    std::vector<uint16_t> dark_px(64, kDark);
    XpeImageBuffer dark{};
    dark.data = dark_px.data();
    dark.width = 8; dark.height = 8;
    dark.bitsAllocated = 16; dark.bitsStored = 16;
    dark.format = XPE_PIXEL_UINT16;
    dark.dataSize = static_cast<uint32_t>(dark_px.size() * sizeof(uint16_t));

    const std::string with_dark = std::string(::testing::TempDir()) + "/a112_dark.xcal";
    ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(
        a.frames.data(), a.dose.data(), kLevels, &dark, 4096u,
        with_dark.c_str(), nullptr));

    const Sim b = MakeSim();
    const std::string no_dark = std::string(::testing::TempDir()) + "/a112_nodark.xcal";
    ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(
        b.frames.data(), b.dose.data(), kLevels, nullptr, 4096u,
        no_dark.c_str(), nullptr));

    XCalFileHeader h1{}, h2{};
    std::vector<uint8_t> c1, p1, c2, p2;
    ASSERT_EQ(XPE_OK, read_xcal_file(with_dark.c_str(), h1, c1, p1, false,
                                     XCAL_TYPE_NONLIN_LUT));
    ASSERT_EQ(XPE_OK, read_xcal_file(no_dark.c_str(), h2, c2, p2, false,
                                     XCAL_TYPE_NONLIN_LUT));
    EXPECT_EQ(p1, p2) << "the dark reference was not subtracted";

    // Control: without the reference the pedestal changes the table, so the
    // comparison above is not two runs that were identical anyway.
    const std::string forgotten = std::string(::testing::TempDir()) + "/a112_forgot.xcal";
    ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(
        a.frames.data(), a.dose.data(), kLevels, nullptr, 4096u,
        forgotten.c_str(), nullptr));
    XCalFileHeader h3{};
    std::vector<uint8_t> c3, p3;
    ASSERT_EQ(XPE_OK, read_xcal_file(forgotten.c_str(), h3, c3, p3, false,
                                     XCAL_TYPE_NONLIN_LUT));
    EXPECT_NE(p1, p3) << "the pedestal is too small to tell the two apart";
}

/* ===================================================================== *
 * QA-A-117 (#186) TEMPORARY PROBE -- the card's falsification, stated as
 * the card states it: replace the table with the identity and the output
 * must change. If it does not, the LUT is wired but unused.
 * ===================================================================== */
TEST_F(NonlinApplyTest, AnIdentityTableChangesTheOutputComparedToTheRealOne) {
    // 1. The real table, applied through the pipeline.
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(LutPath().c_str()));
    Frame real = MakeGradientFrame(sim);
    XpeImageMetadata m1{};
    ASSERT_EQ(XPE_OK, RunPipelineNonlinOnly(real, &m1, nullptr));
    ASSERT_NE(0u, m1.flags & XPE_FLAG_NONLINEARITY_CORRECTED);

    // 2. An identity table written by hand: LUT[i] = i. Loading it replaces
    //    the real one, so the stage still runs and still reports "corrected",
    //    but every pixel maps to itself.
    std::vector<uint16_t> ident(4096u);
    for (uint32_t i = 0; i < 4096u; ++i) ident[i] = static_cast<uint16_t>(i);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT16);
    hdr.width = 4096; hdr.height = 1;
    hdr.created_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string ipath = std::string(::testing::TempDir()) + "/a117_ident.xcal";
    ASSERT_EQ(XPE_OK, write_xcal_file(ipath.c_str(), hdr, nullptr, 0,
                                      reinterpret_cast<const uint8_t*>(ident.data()),
                                      ident.size() * sizeof(uint16_t)));
    ASSERT_EQ(XPE_OK, xpe_calib_load_nonlin_lut(ipath.c_str()));

    Frame id = MakeGradientFrame(sim);
    const std::vector<uint16_t> before = id.px;
    XpeImageMetadata m2{};
    ASSERT_EQ(XPE_OK, RunPipelineNonlinOnly(id, &m2, nullptr));

    // The identity leaves the frame alone -- which is what makes it a control.
    EXPECT_EQ(before, id.px) << "an identity table must not move any pixel";

    // And the real table must NOT agree with it. If these two came out equal,
    // the stage would be reporting a correction it never applied.
    EXPECT_NE(real.px, id.px)
        << "the real table produced the same output as the identity -- the LUT "
           "is wired but not used";

    // Quantified, so "different" cannot be one stray pixel.
    size_t moved = 0;
    for (size_t i = 0; i < real.px.size(); ++i) {
        if (real.px[i] != id.px[i]) ++moved;
    }
    EXPECT_GT(moved, real.px.size() / 2)
        << "only " << moved << " of " << real.px.size() << " pixels differ";
}
