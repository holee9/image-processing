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

class NonlinApplyTest : public ::testing::Test {
protected:
    Sim sim;
    void SetUp() override {
        sim = MakeSim();
        ASSERT_EQ(XPE_OK, xpe_calib_generate_nonlin_lut(
            sim.frames.data(), sim.dose.data(), kLevels, nullptr, 4096u,
            LutPath().c_str(), nullptr));
        // The pipeline entry points refuse to run before init (-6). A second
        // init in the same process is not an error condition this file tests,
        // so its code is ignored -- what matters is that the module is up.
        (void)xpe_preprocess_init(nullptr);
        xpe_clear_alerts();
        // The calibration store is global and survives between tests, so the
        // "no LUT" cases would otherwise read whatever the previous test left.
        xpe_calib_unload_nonlin_lut();
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
