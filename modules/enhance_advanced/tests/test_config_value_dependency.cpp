// #145 (QA-B-62): config keys that are read but never reach the result.
//
// QA-B-60 and QA-B-61 made an UNKNOWN key audible. Both reports closed with the
// same residual risk, twice:
//
//     "no warning" does not mean "the setting was applied" -- a key whose name
//     and type are both correct can still be dropped, and the warning cannot
//     see that.
//
// This file is that residual risk turned into assertions. The question here is
// not "does the parser recognise the name" but "does changing the value change
// the output". It is the QA-B-58 dependency sweep with config keys as the
// subject.
//
// THE SHAPE OF THE ASSERTION (established in QA-B-58, unchanged here):
// change ONE key, hold everything else fixed, and require the output to MOVE.
// A characterization test -- one that pins the current output values -- passes
// identically after the input stops being read, which is exactly the defect
// class being hunted. So nothing here pins an output value.
//
// THE THRESHOLD, and why it is not `> 0`.
// #156 is the trap: a 1-ulp difference (5.96e-08 at unit scale) reads as
// "reached" under `> 0`, so a key that only perturbs rounding would pass. QA-B-58
// used an absolute 1e-4, and itself recorded the gap that an absolute figure does
// not transfer to a function with a different output scale. So the threshold here
// is RELATIVE to the measured baseline: 1e-4 x (max - min) of the baseline
// output. On the fixture below the baseline range is ~1e3, giving ~1e-1 --
// roughly four orders of magnitude above float rounding at that magnitude, so
// the two cannot be confused in either direction. Collimation is exempt: it
// returns integer pixel coordinates, where any change is a whole pixel and no
// rounding band exists.
//
// THE FIXTURE, and why it is not the 32x32 frame the other tests use.
// QA-B-58 was caught twice by a fixture that never reached the code under test
// (CLAHE's clip_limit had nothing to clip in a 64-pixel tile; collimation's ROI
// fell below the minimum area ratio and took the documented full-extent
// fallback). Both had said so in the log. So:
//   - multiscale needs detail at SEVERAL scales or a per-level gain has nothing
//     to multiply -- the frame carries a smooth ramp, hard edges, and fine
//     texture, at 256x256 so a 4-level pyramid is affordable;
//   - collimation needs a bright region inset far enough from the border to
//     clear the minimum-area gate, or detection takes the fallback path and
//     border_margin is never applied (the QA-B-58 finding, reproduced as a
//     deliberate contrast case below).
//
// NON-MOVEMENT IS ONLY A FINDING WITH PROOF OF ARRIVAL. Every KnownDivergence_
// case below carries evidence that the value reached the parser -- either the
// same key moving the output under a different neighbouring setting, or the
// QA-B-61 unknown-key warning staying SILENT, which is positive evidence that
// the parser recognises the name.

#include <gtest/gtest.h>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 256, kH = 256;

// A frame with content at several scales: a smooth ramp (coarse), hard steps
// (edges), and a high-frequency ripple (texture). A per-level gain has
// something to multiply at every level of a 4-level pyramid.
std::vector<float> StructuredFrame() {
    std::vector<float> px(static_cast<size_t>(kW) * kH);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            const float ramp    = 300.0f + 2.0f * static_cast<float>(x);
            const float step    = (x / 32u) % 2u ? 180.0f : 0.0f;
            const float texture = 40.0f * std::sin(static_cast<float>(x) * 1.7f)
                                        * std::cos(static_cast<float>(y) * 2.3f);
            px[static_cast<size_t>(y) * kW + x] = ramp + step + texture;
        }
    }
    return px;
}

// A bright rectangle inset 48 px on a dark ground. QA-B-58 measured that this
// shape clears the minimum-area gate and reaches the detection path, where
// border_margin is applied; a smaller inset falls back to the full extent.
std::vector<float> CollimatedFrame(uint32_t inset) {
    std::vector<float> px(static_cast<size_t>(kW) * kH, 20.0f);
    for (uint32_t y = inset; y + inset < kH; ++y) {
        for (uint32_t x = inset; x + inset < kW; ++x) {
            px[static_cast<size_t>(y) * kW + x] = 900.0f;
        }
    }
    return px;
}

XpeImageBuffer Wrap(std::vector<float>& px) {
    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data          = px.data();
    img.dataSize      = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

XpeImageMetadata Meta() {
    XpeImageMetadata m{};
    std::snprintf(m.bodyPart, sizeof(m.bodyPart), "%s", "CHEST");
    return m;
}

float Range(const std::vector<float>& v) {
    const auto mm = std::minmax_element(v.begin(), v.end());
    return *mm.second - *mm.first;
}

float MaxDiff(const std::vector<float>& a, const std::vector<float>& b) {
    float d = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        d = std::max(d, std::fabs(a[i] - b[i]));
    }
    return d;
}

// Run multiscale on a fresh copy of the fixture and return the processed pixels.
std::vector<float> Multiscale(const char* config) {
    std::vector<float> px = StructuredFrame();
    XpeImageBuffer img = Wrap(px);
    XpeImageMetadata meta = Meta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, config), XPE_OK) << "config: "
        << (config ? config : "(null)");
    return px;
}

std::vector<float> Fractional(float order, const char* config) {
    std::vector<float> px = StructuredFrame();
    XpeImageBuffer img = Wrap(px);
    EXPECT_EQ(xpe_fractional_process(&img, order, config), XPE_OK) << "config: "
        << (config ? config : "(null)");
    return px;
}

struct Roi { int32_t x0, y0, x1, y1; bool operator==(const Roi& o) const {
    return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1; } };

Roi Collimate(uint32_t inset, const char* config) {
    std::vector<float> px = CollimatedFrame(inset);
    XpeImageBuffer img = Wrap(px);
    Roi r{};
    EXPECT_EQ(xpe_detect_collimation(&img, &r.x0, &r.y0, &r.x1, &r.y1, config), XPE_OK);
    return r;
}

// Positive evidence that the parser RECOGNISES a key: the QA-B-61 unknown-key
// warning stays silent for it. A key the parser did not know would be named in
// an alert. Returns true when nothing was said about `key`.
bool ParserRecognises(const char* config, const char* key) {
    xpe_clear_alerts();
    std::vector<float> px = StructuredFrame();
    XpeImageBuffer img = Wrap(px);
    (void)xpe_fractional_process(&img, 1.0f, config);
    for (int i = 0; i < 256; ++i) {
        char msg[256] = {0};
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) break;
        if (msg[0] == '\0') break;
        if (std::string(msg).find(key) != std::string::npos) {
            xpe_clear_alerts();
            return false;
        }
    }
    xpe_clear_alerts();
    return true;
}

class ConfigValueDependency : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK);
        xpe_clear_alerts();
        // Threshold is derived from the fixture, not hard-coded: 1e-4 of the
        // baseline's own dynamic range. See the file header for why not `> 0`.
        baseline_ = Multiscale(R"({"num_levels":4,"edge_gain":1.5,"texture_gain":1.2,
                                   "flat_gain":1.0,"noise_threshold":0.02})");
        moveThreshold_ = 1e-4f * Range(baseline_);
        ASSERT_GT(moveThreshold_, 0.0f) << "fixture is flat -- nothing could move";
    }
    void TearDown() override { xpe_clear_alerts(); }

    std::vector<float> baseline_;
    float moveThreshold_ = 0.0f;
};

/* ==========================================================================
 * multiscale -- the keys that DO reach the result
 * ========================================================================== */

TEST_F(ConfigValueDependency, MultiscaleGainsAndThresholdAllMoveTheOutput) {
    struct Case { const char* name; const char* config; };
    const Case cases[] = {
        {"edge_gain",       R"({"num_levels":4,"edge_gain":3.0,"texture_gain":1.2,"flat_gain":1.0,"noise_threshold":0.02})"},
        {"texture_gain",    R"({"num_levels":4,"edge_gain":1.5,"texture_gain":3.0,"flat_gain":1.0,"noise_threshold":0.02})"},
        {"flat_gain",       R"({"num_levels":4,"edge_gain":1.5,"texture_gain":1.2,"flat_gain":2.5,"noise_threshold":0.02})"},
        {"noise_threshold", R"({"num_levels":4,"edge_gain":1.5,"texture_gain":1.2,"flat_gain":1.0,"noise_threshold":30.0})"},
        {"num_levels",      R"({"num_levels":5,"edge_gain":1.5,"texture_gain":1.2,"flat_gain":1.0,"noise_threshold":0.02})"},
    };
    for (const auto& c : cases) {
        const float d = MaxDiff(baseline_, Multiscale(c.config));
        EXPECT_GT(d, moveThreshold_)
            << c.name << " changed but the output did not move (maxdiff=" << d
            << ", threshold=" << moveThreshold_ << ")";
        std::printf("[  INFO ] multiscale %-16s maxdiff=%.6f (threshold %.6f)\n",
                    c.name, d, moveThreshold_);
    }
}

/* ==========================================================================
 * multiscale -- values that are read and then discarded
 * ========================================================================== */

// The legacy flat-schema key `levels` is parsed AFTER `num_levels` into the same
// output variable, so a config carrying both silently loses `num_levels`. This
// records the behaviour; it is not a statement that it is wrong.
TEST_F(ConfigValueDependency, KnownDivergence_LevelsSilentlyOverridesNumLevels) {
    const char* both  = R"({"num_levels":5,"levels":2,"edge_gain":1.5,"texture_gain":1.2,"flat_gain":1.0,"noise_threshold":0.02})";
    const char* only2 = R"({"levels":2,"edge_gain":1.5,"texture_gain":1.2,"flat_gain":1.0,"noise_threshold":0.02})";
    const char* only5 = R"({"num_levels":5,"edge_gain":1.5,"texture_gain":1.2,"flat_gain":1.0,"noise_threshold":0.02})";

    const std::vector<float> withBoth = Multiscale(both);

    // Proof of arrival: `num_levels` alone DOES move the output (it is read).
    EXPECT_GT(MaxDiff(baseline_, Multiscale(only5)), moveThreshold_)
        << "num_levels is not read at all -- the override claim would be moot";

    // The divergence: with both present, the result is the `levels` result.
    EXPECT_LT(MaxDiff(withBoth, Multiscale(only2)), moveThreshold_)
        << "levels did not win";
    EXPECT_GT(MaxDiff(withBoth, Multiscale(only5)), moveThreshold_)
        << "num_levels won -- the last-write-wins order has changed";
}

// mfp_scalar applies a per-level gain by branch: the coarsest detail level takes
// flat_gain, level 0 takes edge_gain, and everything between takes texture_gain.
// With few levels the branches collapse, and a gain that is parsed and clamped
// has nothing left to multiply.
TEST_F(ConfigValueDependency, KnownDivergence_LowLevelCountSilencesGains) {
    auto at = [](int levels, float edge, float texture) {
        char cfg[192];
        std::snprintf(cfg, sizeof(cfg),
                      R"({"num_levels":%d,"edge_gain":%.2f,"texture_gain":%.2f,"flat_gain":1.0,"noise_threshold":0.02})",
                      levels, edge, texture);
        return Multiscale(cfg);
    };

    // Proof of arrival: at 4 levels both gains move the output.
    EXPECT_GT(MaxDiff(at(4, 1.5f, 1.2f), at(4, 3.0f, 1.2f)), moveThreshold_)
        << "edge_gain is never read -- the collapse claim would be moot";
    EXPECT_GT(MaxDiff(at(4, 1.5f, 1.2f), at(4, 1.5f, 3.0f)), moveThreshold_)
        << "texture_gain is never read -- the collapse claim would be moot";

    // num_levels = 3: the middle branch is empty, so texture_gain does nothing.
    const float tex3 = MaxDiff(at(3, 1.5f, 1.2f), at(3, 1.5f, 3.0f));
    EXPECT_LT(tex3, moveThreshold_)
        << "texture_gain now moves at 3 levels (maxdiff=" << tex3 << ")";

    // num_levels = 2: the coarsest branch also covers level 0, so BOTH the edge
    // and texture gains are unreachable and only flat_gain survives.
    const float edge2 = MaxDiff(at(2, 1.5f, 1.2f), at(2, 3.0f, 1.2f));
    const float tex2  = MaxDiff(at(2, 1.5f, 1.2f), at(2, 1.5f, 3.0f));
    EXPECT_LT(edge2, moveThreshold_) << "edge_gain now moves at 2 levels (maxdiff=" << edge2 << ")";
    EXPECT_LT(tex2,  moveThreshold_) << "texture_gain now moves at 2 levels (maxdiff=" << tex2 << ")";

    std::printf("[  INFO ] gain reach by level count: tex@3=%.6f edge@2=%.6f tex@2=%.6f "
                "(threshold %.6f)\n", tex3, edge2, tex2, moveThreshold_);
}

/* ==========================================================================
 * fractional -- the finding this card was opened for
 * ========================================================================== */

TEST_F(ConfigValueDependency, FractionalIterationsMovesTheOutput) {
    const float d = MaxDiff(Fractional(1.0f, R"({"iterations":1,"step_size":0.5})"),
                            Fractional(1.0f, R"({"iterations":3,"step_size":0.5})"));
    EXPECT_GT(d, moveThreshold_) << "iterations changed but the output did not move";
    std::printf("[  INFO ] fractional iterations maxdiff=%.6f (threshold %.6f)\n",
                d, moveThreshold_);
}

// step_size is parsed and clamped to [0.01, 1.0] and then goes nowhere: the
// struct it would travel in, FractionalConfig, has a single member (`order`),
// and the call site fills only that. The value reaches a debug log line and
// stops there.
//
// This is the exact residual risk QA-B-60 and QA-B-61 both recorded: the
// unknown-key warning is SILENT here, because `step_size` IS a known key.
TEST_F(ConfigValueDependency, KnownDivergence_FractionalStepSizeIsReadAndDiscarded) {
    // Proof of arrival (1): the parser accepts the config rather than rejecting it.
    std::vector<float> px = StructuredFrame();
    XpeImageBuffer img = Wrap(px);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, R"({"step_size":0.99})"), XPE_OK);

    // Proof of arrival (2): the parser RECOGNISES the name -- the QA-B-61
    // warning names unknown keys, and stays silent for this one.
    EXPECT_TRUE(ParserRecognises(R"({"step_size":0.99})", "step_size"))
        << "step_size is reported as unknown -- it is not a known key after all";

    // The divergence: across the whole clamped range, the output is identical.
    const std::vector<float> lo = Fractional(1.0f, R"({"iterations":2,"step_size":0.01})");
    const std::vector<float> hi = Fractional(1.0f, R"({"iterations":2,"step_size":1.0})");
    const float d = MaxDiff(lo, hi);
    EXPECT_LT(d, moveThreshold_)
        << "step_size now moves the output (maxdiff=" << d << ") -- it has been wired";
    EXPECT_EQ(d, 0.0f) << "expected bit-identical output, not merely a small one";

    std::printf("[  INFO ] fractional step_size 0.01 vs 1.0 maxdiff=%.9f "
                "(threshold %.6f) -- read, clamped, discarded\n", d, moveThreshold_);
}

/* ==========================================================================
 * collimation -- integer coordinates, so no rounding band
 * ========================================================================== */

TEST_F(ConfigValueDependency, CollimationKeysMoveTheDetectedRoi) {
    // The fixture must clear the minimum-area gate first, or every case below
    // measures the documented full-extent fallback instead of the detector.
    const Roi base = Collimate(48, R"({"confidence_strictness":0.5,"min_area_ratio":0.05,"border_margin":0})");
    const Roi full{0, 0, static_cast<int32_t>(kW) - 1, static_cast<int32_t>(kH) - 1};
    ASSERT_FALSE(base == full)
        << "the fixture took the full-extent fallback -- this measures the fallback, "
           "not the keys (the QA-B-58 trap)";

    // border_margin is a CLAMP on the coordinates, not an inset:
    //   x0 = max(border_margin, rect.x0);  x1 = min(w-1-border_margin, rect.x1)
    // so a margin BELOW the detected edge is a no-op BY CONSTRUCTION. The first
    // attempt here used 16 against a rectangle starting at x=48, measured no
    // movement, and would have reported a defect that does not exist -- the
    // QA-B-58 fixture trap met a third time, caught by reading the code rather
    // than by trusting the measurement. 64 exceeds the detected edge, so it is
    // the value that actually exercises the key.
    const Roi belowEdge = Collimate(48, R"({"confidence_strictness":0.5,"min_area_ratio":0.05,"border_margin":16})");
    EXPECT_TRUE(base == belowEdge)
        << "a margin below the detected edge moved the ROI -- border_margin is no "
           "longer a clamp";

    const Roi aboveEdge = Collimate(48, R"({"confidence_strictness":0.5,"min_area_ratio":0.05,"border_margin":64})");
    EXPECT_FALSE(base == aboveEdge) << "border_margin did not move the ROI even above the edge";

    // min_area_ratio above the fixture's own ratio must force the fallback --
    // a different result, and one whose reason is in the log.
    const Roi gated = Collimate(48, R"({"confidence_strictness":0.5,"min_area_ratio":1.0,"border_margin":0})");
    EXPECT_TRUE(gated == full) << "min_area_ratio=1.0 did not force the fallback";

    std::printf("[  INFO ] collimation base=[%d,%d,%d,%d] margin16(below edge)=[%d,%d,%d,%d] "
                "margin64(above edge)=[%d,%d,%d,%d] gated=[%d,%d,%d,%d]\n",
                base.x0, base.y0, base.x1, base.y1,
                belowEdge.x0, belowEdge.y0, belowEdge.x1, belowEdge.y1,
                aboveEdge.x0, aboveEdge.y0, aboveEdge.x1, aboveEdge.y1,
                gated.x0, gated.y0, gated.x1, gated.y1);
}

// confidence_strictness feeds two things that pull opposite ways -- the Hough
// theta step (collimation_detect.cpp:134, higher = finer search) and the
// confidence a detection must reach (:181, higher = rejects more) -- so it is
// measured across its full clamped range rather than at a nearby pair.
//
// HISTORY, because it is the reason this case has the shape it has.
// QA-B-62 measured 0.0 against 1.0 on ONE fixture, saw the same integer ROI, and
// recorded it WITHOUT asserting: a single fixture cannot separate "the parameter
// does nothing" from "this fixture's edges are far past the point where it could
// matter". QA-B-63 separated them by sweeping the variable the fixture controls,
// and the answer was neither -- the parameter works, in a narrow band at the
// detector's own floor, and it ran OPPOSITE to the name it then carried
// (`sensitivity`): raising it lost a detection the low setting made.
//
// #164 resolved that by moving the NAME to match the arithmetic (2026-09-16,
// user decision). The arithmetic is untouched, so this case asserts exactly what
// it asserted before -- which is what makes it the check that the rename changed
// no behaviour. What changed is that the direction is now the one the name
// predicts, so this is a pinned property rather than a divergence.
//
// Same structure as the border_margin case above: there the threshold was swept
// past the detected boundary; here the fixture is swept past the detector's own
// floor.
TEST_F(ConfigValueDependency, CollimationConfidenceStrictnessRejectsMoreAsItRises) {
    // Contrast from barely-there to unmistakable, with the 21..25 band sampled
    // finely: the first run showed the detector itself flipping between those
    // two (21 falls back to the full extent, 25 detects), so that band IS the
    // detector's own floor and is exactly where this parameter would
    // have to act if it acts anywhere. The dark ground is 20.
    const float kForegrounds[] = {20.5f, 21.0f, 21.5f, 22.0f, 22.5f, 23.0f,
                                  24.0f, 25.0f, 40.0f, 80.0f, 200.0f, 900.0f};
    const int kN = static_cast<int>(sizeof(kForegrounds) / sizeof(kForegrounds[0]));

    int splits = 0;
    int lowDetectedHighFellBack = 0;
    const Roi full{0, 0, static_cast<int32_t>(kW) - 1, static_cast<int32_t>(kH) - 1};
    for (float fg : kForegrounds) {
        std::vector<float> pxLo(static_cast<size_t>(kW) * kH, 20.0f);
        for (uint32_t y = 48; y + 48 < kH; ++y)
            for (uint32_t x = 48; x + 48 < kW; ++x)
                pxLo[static_cast<size_t>(y) * kW + x] = fg;
        std::vector<float> pxHi = pxLo;

        Roi lo{}, hi{};
        XpeImageBuffer iLo = Wrap(pxLo);
        XpeImageBuffer iHi = Wrap(pxHi);
        EXPECT_EQ(xpe_detect_collimation(&iLo, &lo.x0, &lo.y0, &lo.x1, &lo.y1,
                  R"({"confidence_strictness":0.0,"min_area_ratio":0.05,"border_margin":0})"), XPE_OK);
        EXPECT_EQ(xpe_detect_collimation(&iHi, &hi.x0, &hi.y0, &hi.x1, &hi.y1,
                  R"({"confidence_strictness":1.0,"min_area_ratio":0.05,"border_margin":0})"), XPE_OK);

        const bool split = !(lo == hi);
        if (split) {
            ++splits;
            if (!(lo == full) && hi == full) ++lowDetectedHighFellBack;
        }
        std::printf("[  INFO ] strictness sweep fg=%-6.1f strict0.0=[%d,%d,%d,%d] "
                    "strict1.0=[%d,%d,%d,%d] %s\n", fg,
                    lo.x0, lo.y0, lo.x1, lo.y1, hi.x0, hi.y0, hi.x1, hi.y1,
                    split ? "SPLIT" : "same");
    }

    std::printf("[  INFO ] strictness sweep: %d of %d edge strengths split 0.0 from 1.0\n",
                splits, kN);

    // Result 1: the parameter is NOT inert. QA-B-62's single fixture sat far
    // above the detector's floor, which is why it showed nothing; the split
    // appears only in the narrow band where detection itself is marginal.
    EXPECT_GT(splits, 0)
        << "no edge strength splits confidence_strictness 0.0 from 1.0 across a ~45x "
           "contrast "
           "range that brackets the detector's own floor -- the parameter is inert";

    // Result 2, the direction: raising the value makes detection FAIL on an edge
    // a low setting finds, because the value is interpolated into the confidence
    // a detection must reach --
    //     confidenceThreshold = 0.7 + 0.3 * confidence_strictness   (:181)
    // -- so a higher setting demands more and rejects more. Since #164 the name
    // says that too; before it, the name said the opposite.
    //
    // Asserted as a direction rather than at a fixed contrast: the exact
    // strength where the band sits is a fixture property, but which side wins is
    // the behaviour. Pinned, not fixed -- changing it moves clinical output, and
    // it is what tells a later reader that the #164 rename touched the name only.
    EXPECT_EQ(lowDetectedHighFellBack, splits)
        << "a split ran the other way (a low setting fell back where a high one "
           "detected) -- the direction has changed";
}

/* ==========================================================================
 * #164 (QA-B-65): the old name is refused out loud, not accepted quietly
 * ========================================================================== */

// The rename would be worth little if `sensitivity` kept working: a caller would
// go on setting a key whose name says the opposite of what it does, and nothing
// would say so. It is not kept, and the mechanism that says so already exists --
// the QA-B-60/61 unknown-key warning. Dropping the key from the known list is
// what routes it there, so this case asserts that the routing actually happens
// rather than assuming it.
//
// No deprecation period: the leader's caller audit found no C#, XAML or deployed
// JSON setting this key, so there is nobody to ease across.
TEST_F(ConfigValueDependency, OldSensitivityNameIsReportedAsUnknownAndHasNoEffect) {
    xpe_clear_alerts();
    const Roi withOldName =
        Collimate(48, R"({"sensitivity":1.0,"min_area_ratio":0.05,"border_margin":0})");

    // It is named in an alert -- the warning identifies the key, so a caller
    // reading the log learns which setting was ignored rather than only that
    // something was.
    bool named = false;
    for (int i = 0; i < 256; ++i) {
        char msg[256] = {0};
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) break;
        if (msg[0] == '\0') break;
        if (std::string(msg).find("sensitivity") != std::string::npos) named = true;
    }
    xpe_clear_alerts();
    EXPECT_TRUE(named)
        << "the old key name was accepted silently -- it is still in the known-key "
           "list, or the unknown-key warning no longer routes it";

    // And it has no effect: the result is the DEFAULT, not the requested 1.0.
    // Measured against both ends so this cannot pass by coincidence.
    const Roi atDefault =
        Collimate(48, R"({"min_area_ratio":0.05,"border_margin":0})");
    const Roi atRequested =
        Collimate(48, R"({"confidence_strictness":1.0,"min_area_ratio":0.05,"border_margin":0})");

    EXPECT_TRUE(withOldName == atDefault)
        << "the old name still changes behaviour -- it is being read somewhere";

    std::printf("[  INFO ] old name: [%d,%d,%d,%d]  default: [%d,%d,%d,%d]  "
                "new name @1.0: [%d,%d,%d,%d]\n",
                withOldName.x0, withOldName.y0, withOldName.x1, withOldName.y1,
                atDefault.x0, atDefault.y0, atDefault.x1, atDefault.y1,
                atRequested.x0, atRequested.y0, atRequested.x1, atRequested.y1);
}

}  // namespace
