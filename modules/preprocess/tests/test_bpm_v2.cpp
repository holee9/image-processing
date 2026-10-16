/**
 * @file test_bpm_v2.cpp
 * @brief Tests of the second-generation defective-pixel detection xpe_bpm_generate_v2 (SRS-CALIB-FUNC-022, QA-A-252, #273).
 *
 * The method is fixed in .moai/reports/lane-pre/QA-A-252/evidence/30_method_fixed_before_measurement.md. The frames are synthetic: a dark level with a
 * fixed pattern, temporal noise, and bright frames of four doses with a gain pattern and shot-like noise. Defects are planted the way the evaluation
 * tool plants them (the same places in every frame).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 256, kH = 256;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

// A deterministic normal generator (the standard library's is not the same on every platform).
class Normal {
public:
    explicit Normal(uint64_t seed) : s_(seed * 2862933555777941757ull + 3037000493ull) {}
    double next() {
        const double u1 = uniform(), u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }

private:
    double uniform() {
        s_ = s_ * 6364136223846793005ull + 1442695040888963407ull;
        return (static_cast<double>(s_ >> 11) + 0.5) / 9007199254740992.0;
    }
    uint64_t s_;
};

struct Frames {
    std::vector<std::vector<uint16_t>> dark, bright;
    std::vector<double>                signal;   // net signal of each bright frame
};

XpeImageBuffer buf(std::vector<uint16_t>& v) {
    XpeImageBuffer b{};
    b.data = v.data();
    b.width = kW;
    b.height = kH;
    b.bitsAllocated = 16;
    b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

XpeImageBuffer outBuf(std::vector<uint8_t>& v) {
    XpeImageBuffer b{};
    b.data = v.data();
    b.width = kW;
    b.height = kH;
    b.bitsAllocated = 8;
    b.bitsStored = 8;
    b.format = XPE_PIXEL_UINT8;
    b.dataSize = v.size();
    return b;
}

// What is planted, at fixed places. Applied to every frame like the evaluation tool does.
struct Plant {
    bool stripes{true};
    bool defects{true};
    // QA-A-252b: what the first measurement of the detector showed it cannot take (#273)
    bool partialLines{false};   // a quarter of a row / of a column, and of a two-row line, without response in the bright frames
    bool varyingColumn{false};  // a column whose offset changes from frame to frame
    bool yGradient{false};      // the gain also changes along the columns (vertical shading), so a multiplicative column band leaves a trace
    bool darkPartial{false};    // a steep dark-level gradient along the rows and a raised segment in the middle of one row (every frame): only the dark-mean feature sees it
    bool brightNoisyPixel{false};   // a pixel whose temporal noise is large in the bright frames only (the dark frames cannot show it)
    bool strongShading{false};  // +-20 % gain shading along both directions (a heel-effect-like flat field): a line component taken from a partly defective line moves by many noise widths
};

constexpr int kHotX = 40, kHotY = 50;        // +80 pixel values in every frame
constexpr int kDeadX = 100, kDeadY = 60;     // no signal in the bright frames
constexpr int kSlopeX = 150, kSlopeY = 120;  // response 1.10
constexpr int kNoisyX = 200, kNoisyY = 30;   // temporal noise sd 30 instead of 3
constexpr int kBandRow = 180;                // fixed +60 in every frame (a row band)
constexpr int kGainCol = 220;                // response 0.8 in the bright frames (a column band)
constexpr int kPartRow = 100, kPartRowPair = 140, kPartCol = 30;   // partial lines: a quarter of the length (64 of 256), from the frame start
constexpr int kPartLen = 64;
constexpr int kPartRowFrom = 96;             // the row segments sit in the middle of the row: the median of a row with a gradient moves most when the missing part is not at an end
constexpr int kBrightNoisyX = 120, kBrightNoisyY = 200;   // extra temporal noise sd 150 in every bright frame
constexpr int kDarkRow = 60, kDarkFrom = 96;   // the raised segment: kPartLen pixels, +300 in every frame
constexpr int kVarCol = 70;                  // a column offset that is a new random value in every frame (sd 15)

Frames makeFrames(uint64_t seed, const std::vector<double>& doses, int framesPerDose, int nDark, Plant plant, bool transientFirstDark = false) {
    Normal rng(seed);
    std::vector<double> D0(kN), G(kN);
    for (size_t i = 0; i < kN; ++i) {
        const int x = static_cast<int>(i % kW), y = static_cast<int>(i / kW);
        D0[i] = 1000.0 + 4.0 * rng.next() + 0.02 * x;                                         // fixed pattern and a gentle slope
        G[i] = (1.0 + 0.004 * rng.next()) * (0.95 + 0.1 * x / (kW - 1)) * (1.0 + 0.00005 * y);  // gain pattern with a shading
    }
    auto atPlant = [&](int x, int y, int px, int py) { return x == px && y == py; };
    Frames f;
    Normal pixelRng(seed + 7);   // used only for the bright-noise pixel
    Normal frameRng(seed + 99);   // separate from the pixel noise, so the planted extras do not change any other value
    auto put = [&](double net, bool bright, bool firstDark) {
        std::vector<uint16_t> fr(kN);
        const double columnOffset = plant.varyingColumn ? 60.0 * frameRng.next() : 0.0;
        for (size_t i = 0; i < kN; ++i) {
            const int x = static_cast<int>(i % kW), y = static_cast<int>(i / kW);
            double d = D0[i], sig = net * G[i];
            if (plant.darkPartial) {
                d += 0.3 * x;
                if (y == kDarkRow && x >= kDarkFrom && x < kDarkFrom + kPartLen) d += 300.0;
            }
            if (plant.yGradient) sig *= 0.93 + 0.14 * y / (kH - 1);
            if (plant.strongShading) sig *= (0.8 + 0.4 * x / (kW - 1)) * (0.8 + 0.4 * y / (kH - 1));
            if (plant.varyingColumn && x == kVarCol) d += columnOffset;
            if (plant.partialLines) {
                if ((y == kPartRow || y == kPartRowPair || y == kPartRowPair + 1) && x >= kPartRowFrom && x < kPartRowFrom + kPartLen) sig = 0.0;
                if (x == kPartCol && y < kPartLen) sig = 0.0;
            }
            double noiseSd = 3.0;
            if (plant.defects) {
                if (atPlant(x, y, kHotX, kHotY)) d += 80.0;
                if (atPlant(x, y, kDeadX, kDeadY)) sig = 0.0;
                if (atPlant(x, y, kSlopeX, kSlopeY)) sig *= 1.10;
                if (atPlant(x, y, kNoisyX, kNoisyY)) noiseSd = 30.0;
            }
            if (plant.stripes) {
                if (y == kBandRow) d += 60.0;
                if (x == kGainCol) sig *= 0.8;
            }
            double v = d + (bright ? sig + 0.5 * std::sqrt(std::max(sig, 1.0)) * rng.next() : 0.0) + noiseSd * rng.next();
            if (bright && plant.brightNoisyPixel && atPlant(x, y, kBrightNoisyX, kBrightNoisyY)) v += 150.0 * pixelRng.next();
            if (firstDark && x >= 20 && x < 24 && y >= 20 && y < 24) v += 200.0;   // a transient only in the first frame
            fr[i] = static_cast<uint16_t>(std::min(65535.0, std::max(0.0, std::floor(v + 0.5))));
        }
        return fr;
    };
    for (int k = 0; k < nDark; ++k) f.dark.push_back(put(0.0, false, transientFirstDark && k == 0));
    for (double d : doses)
        for (int k = 0; k < framesPerDose; ++k) {
            f.bright.push_back(put(d, true, false));
            f.signal.push_back(d);
        }
    return f;
}

struct Result {
    XpeErrorCode          rc;
    std::vector<uint8_t>  map, feat, area;
    XpeBpmReportV2        rep;
};

Result detect(Frames& f, const XpeBpmConfigV2* cfg) {
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : f.dark) db.push_back(buf(v));
    for (auto& v : f.bright) bb.push_back(buf(v));
    Result r{};
    r.map.assign(kN, 0);
    r.feat.assign(kN, 0);
    r.area.assign(kN, 77);   // an impossible value: the detector must write every pixel
    XpeImageBuffer mb = outBuf(r.map), fb = outBuf(r.feat), ab = outBuf(r.area);
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    r.rc = xpe_bpm_generate_v2(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), cfg, &mb, &fb, &ab, &r.rep);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    return r;
}

const std::vector<double> kDoses = {2000.0, 4500.0, 9000.0, 18000.0};

// The per-feature counts and thresholds, for the failure messages.
std::string describe(const XpeBpmReportV2& r) {
    static const char* const names[XPE_BPM_V2_FEATURE_COUNT] = {"dark_mean", "dark_noise", "dose_slope", "dose_linearity", "flat_field", "bright_noise"};
    std::string s = "union " + std::to_string(r.flagged_union);
    for (int k = 0; k < XPE_BPM_V2_FEATURE_COUNT; ++k)
        s += std::string("; ") + names[k] + " flagged " + std::to_string(r.flagged[k]) + " threshold " + std::to_string(r.threshold[k]) + " scale " + std::to_string(r.scale_median[k]);
    return s;
}

TEST(BpmV2Test, FlagsEachPlantedDefectWithTheFeatureThatSeesIt) {
    Frames f = makeFrames(1, kDoses, 3, 7, Plant{false, true});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    auto at = [&](int x, int y) { return static_cast<size_t>(y) * kW + static_cast<size_t>(x); };
    // hot: only the dark mean sees it (the dark is subtracted from the bright frames) -> code 2
    EXPECT_NE(0, r.feat[at(kHotX, kHotY)] & XPE_BPM_V2_FEATURE_DARK_MEAN);
    EXPECT_EQ(2, r.map[at(kHotX, kHotY)]);
    // dead and slope: the response features -> code 1
    EXPECT_NE(0, r.feat[at(kDeadX, kDeadY)] & XPE_BPM_V2_FEATURE_FLAT);
    EXPECT_EQ(1, r.map[at(kDeadX, kDeadY)]);
    EXPECT_NE(0, r.feat[at(kSlopeX, kSlopeY)] & (XPE_BPM_V2_FEATURE_FLAT | XPE_BPM_V2_FEATURE_DOSE_SLOPE));
    EXPECT_EQ(1, r.map[at(kSlopeX, kSlopeY)]);
    // noisy: only the dark temporal noise sees it -> code 2
    EXPECT_NE(0, r.feat[at(kNoisyX, kNoisyY)] & XPE_BPM_V2_FEATURE_DARK_NOISE);
    EXPECT_EQ(2, r.map[at(kNoisyX, kNoisyY)]);
    // the whole map has the four planted pixels and next to nothing else (about the target of 1 per feature)
    EXPECT_LE(r.rep.flagged_union, 4u + 8u) << describe(r.rep);
    EXPECT_GE(r.rep.flagged_union, 4u);
}

TEST(BpmV2Test, StripesThatOffsetAndGainRemoveAreNotDefects) {
    Frames f = makeFrames(2, kDoses, 3, 7, Plant{true, false});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    size_t inRow = 0, inCol = 0;
    for (uint32_t x = 0; x < kW; ++x) inRow += r.map[static_cast<size_t>(kBandRow) * kW + x] != 0;
    for (uint32_t y = 0; y < kH; ++y) inCol += r.map[static_cast<size_t>(y) * kW + kGainCol] != 0;
    EXPECT_EQ(0u, inRow) << "a fixed row band of +60 is removed with the row component";
    EXPECT_EQ(0u, inCol) << "a gain column band of 0.8 is removed with the column component";
    // Not an absolute count: about the target of 1 per feature flags a few pixels by chance (a bound of 8 was a guess and 9 came up). The stripes' own effect is what
    // is judged: the same noise (same seed) without stripes is the control, and the stripes may not add more than 2 flagged pixels to it.
    Frames c = makeFrames(2, kDoses, 3, 7, Plant{false, false});
    const Result control = detect(c, nullptr);
    ASSERT_EQ(XPE_OK, control.rc);
    EXPECT_LE(r.rep.flagged_union, control.rep.flagged_union + 2u) << "with stripes: " << describe(r.rep) << " | control: " << describe(control.rep);
}


// ---------------------------------------------------------------- QA-A-252b: the three failures of the first measurement (#273), on synthetic frames
// The shading of the synthetic gain is 10 % along the rows, like a real flat field; a row component taken as the median of the row itself is then moved by
// a defect that covers a part of the row.
size_t flaggedInRow(const Result& r, int y, int xFrom, int xTo) {
    size_t n = 0;
    for (int x = xFrom; x < xTo; ++x) n += r.map[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] != 0;
    return n;
}
size_t flaggedInColumn(const Result& r, int x, int yFrom, int yTo) {
    size_t n = 0;
    for (int y = yFrom; y < yTo; ++y) n += r.map[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] != 0;
    return n;
}

TEST(BpmV2Test, APartialLineDoesNotPushTheRestOfItsRowOrColumnIntoTheMap) {
    Frames f = makeFrames(5, kDoses, 3, 7, Plant{false, false, true, false, false, false, false, true});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    for (int y : {kPartRow, kPartRowPair, kPartRowPair + 1}) {
        EXPECT_GE(flaggedInRow(r, y, kPartRowFrom, kPartRowFrom + kPartLen), static_cast<size_t>(kPartLen) * 9 / 10) << "the defective part of row " << y << " is found";
        EXPECT_LE(flaggedInRow(r, y, 0, kPartRowFrom - 8) + flaggedInRow(r, y, kPartRowFrom + kPartLen + 8, static_cast<int>(kW)), 2u)
            << "the rest of row " << y << " is not defective: " << describe(r.rep);
    }
    EXPECT_GE(flaggedInColumn(r, kPartCol, 0, kPartLen), static_cast<size_t>(kPartLen) * 9 / 10);
    EXPECT_LE(flaggedInColumn(r, kPartCol, kPartLen + 8, static_cast<int>(kH)), 2u) << "the rest of column " << kPartCol << " is not defective: " << describe(r.rep);
}

TEST(BpmV2Test, NoiseThatOnlyTheBrightFramesShowIsSeenByTheBrightNoiseFeature) {
    Frames f = makeFrames(8, kDoses, 3, 7, Plant{false, false, false, false, false, false, true, false});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    ASSERT_EQ(1u, r.rep.feature_active[5]) << "the bright-noise feature runs when a dose group holds two frames or more";
    EXPECT_EQ(static_cast<uint32_t>(4 * 2), r.rep.degrees_of_freedom[5]) << "pooled degrees of freedom: four groups of three frames, two each";
    const size_t at = static_cast<size_t>(kBrightNoisyY) * kW + static_cast<size_t>(kBrightNoisyX);
    EXPECT_NE(0, r.feat[at] & XPE_BPM_V2_FEATURE_BRIGHT_NOISE) << describe(r.rep);
    EXPECT_EQ(0, r.feat[at] & XPE_BPM_V2_FEATURE_DARK_NOISE) << "the dark frames of this pixel are quiet";
    EXPECT_EQ(1, r.map[at]) << "a bright-frame feature gives code 1";
    EXPECT_LE(r.rep.flagged[5], 1u + 6u) << "about the target of 1, plus the pixel itself: " << describe(r.rep);
}

TEST(BpmV2Test, ARaisedSegmentOfARowDoesNotPushTheRestOfTheRowIntoTheDarkMeanFeature) {
    Frames f = makeFrames(9, kDoses, 3, 7, Plant{false, false, false, false, false, true, false, false});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    size_t segment = 0, rest = 0;
    for (int x = 0; x < static_cast<int>(kW); ++x) {
        const bool hit = (r.feat[static_cast<size_t>(kDarkRow) * kW + static_cast<size_t>(x)] & XPE_BPM_V2_FEATURE_DARK_MEAN) != 0;
        if (x >= kDarkFrom && x < kDarkFrom + kPartLen) segment += hit;
        else if (x < kDarkFrom - 8 || x >= kDarkFrom + kPartLen + 8) rest += hit;
    }
    EXPECT_GE(segment, static_cast<size_t>(kPartLen) * 9 / 10) << "the raised segment is found: " << describe(r.rep);
    EXPECT_LE(rest, 2u) << "the rest of the row is not defective (a steep gradient along the row, a quarter of the row raised): " << describe(r.rep);
}

TEST(BpmV2Test, AColumnOffsetThatChangesFromFrameToFrameIsNotADarkNoiseDefect) {
    Frames f = makeFrames(6, kDoses, 3, 7, Plant{false, false, false, true, false});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    size_t noise = 0, brightNoise = 0;
    for (uint32_t y = 0; y < kH; ++y) {
        noise += (r.feat[static_cast<size_t>(y) * kW + kVarCol] & XPE_BPM_V2_FEATURE_DARK_NOISE) != 0;
        brightNoise += (r.feat[static_cast<size_t>(y) * kW + kVarCol] & XPE_BPM_V2_FEATURE_BRIGHT_NOISE) != 0;
    }
    EXPECT_LE(noise, 2u) << "a shared offset of the column is not temporal noise of its pixels: " << describe(r.rep);
    EXPECT_LE(brightNoise, 2u) << "nor is it in the bright frames (the offset is in every frame): " << describe(r.rep);
}

TEST(BpmV2Test, AGainBandIsRemovedEvenWhenTheGainAlsoChangesAlongTheColumn) {
    Frames f = makeFrames(7, kDoses, 3, 7, Plant{true, false, false, false, false, false, false, true});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_LE(flaggedInColumn(r, kGainCol, 0, static_cast<int>(kH)), 2u) << "a column band of response 0.8 is not a defect: " << describe(r.rep);
}

// ---------------------------------------------------------------- QA-A-252b: lines outside the imaging area (shielded or dummy), a fact given by the user (#273)
namespace {

constexpr uint32_t kML = 6, kMT = 2, kMR = 1, kMB = 1;   // left columns 0..5, top rows 0..1, last column, last row (the margins of the cyan_test panel)

bool inMargin(uint32_t x, uint32_t y) { return x < kML || y < kMT || x >= kW - kMR || y >= kH - kMB; }

// Fills the margins of every frame with values that have nothing to do with the imaging area (different in every frame and pixel).
void scrambleMargins(Frames* f) {
    uint64_t h = 88172645463325252ull;
    auto fill = [&](std::vector<uint16_t>& fr) {
        for (uint32_t y = 0; y < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x)
                if (inMargin(x, y)) {
                    h ^= h << 13; h ^= h >> 7; h ^= h << 17;
                    fr[static_cast<size_t>(y) * kW + x] = static_cast<uint16_t>(h % 40000u);
                }
    };
    for (auto& v : f->dark) fill(v);
    for (auto& v : f->bright) fill(v);
}

XpeBpmConfigV2 marginConfig() {
    XpeBpmConfigV2 c{};
    c.margin_left = kML;
    c.margin_top = kMT;
    c.margin_right = kMR;
    c.margin_bottom = kMB;
    return c;
}

}  // namespace

TEST(BpmV2Test, WildValuesOutsideTheImagingAreaDisturbTheDetectionUnlessTheyAreDeclaredAsMargins) {
    Frames f = makeFrames(10, kDoses, 3, 7, Plant{false, false});
    Frames g = makeFrames(10, kDoses, 3, 7, Plant{false, false});
    scrambleMargins(&g);
    const Result clean = detect(f, nullptr), wildUndeclared = detect(g, nullptr);
    ASSERT_EQ(XPE_OK, clean.rc);
    ASSERT_EQ(XPE_OK, wildUndeclared.rc);
    EXPECT_GT(wildUndeclared.rep.flagged_union, clean.rep.flagged_union + 100u) << "without the declaration the wild lines flag themselves and their neighbours: " << describe(wildUndeclared.rep);
}

TEST(BpmV2Test, MarginsAreToldByTheAreaMaskAndTakeNoPartInTheDecisionOrAnyStatistic) {
    const XpeBpmConfigV2 cfg = marginConfig();
    Frames f = makeFrames(10, kDoses, 3, 7, Plant{false, false});
    Frames g = makeFrames(10, kDoses, 3, 7, Plant{false, false});
    scrambleMargins(&g);
    const Result ref = detect(f, &cfg), wild = detect(g, &cfg);
    ASSERT_EQ(XPE_OK, ref.rc);
    ASSERT_EQ(XPE_OK, wild.rc);
    for (uint32_t y = 0; y < kH; ++y)
        for (uint32_t x = 0; x < kW; ++x) {
            const size_t i = static_cast<size_t>(y) * kW + x;
            ASSERT_EQ(inMargin(x, y) ? 0 : 1, wild.area[i]) << "(" << x << "," << y << "): the area mask is 1 inside and 0 outside";
            if (inMargin(x, y)) {
                ASSERT_EQ(0, wild.map[i]) << "(" << x << "," << y << "): outside the area the map is good (0), the defect codes keep their meaning";
                ASSERT_EQ(0, wild.feat[i]);
            } else {
                ASSERT_LE(wild.map[i], 3) << "(" << x << "," << y << ")";
            }
        }
    // nothing of the margins reaches the detection: the map inside is the same bit for bit whatever the margins hold
    EXPECT_EQ(ref.map, wild.map);
    EXPECT_EQ(ref.feat, wild.feat);
    const uint32_t inside = (kW - kML - kMR) * (kH - kMT - kMB);
    EXPECT_EQ(inside, wild.rep.pixels) << "N of the false-alarm target is the imaging area";
    EXPECT_EQ(static_cast<uint32_t>(kN) - inside, wild.rep.out_of_area_pixels);
    // the thresholds follow from the imaging area: erfc(lambda / sqrt 2) * N_inside = target
    EXPECT_NEAR(1.0, std::erfc(static_cast<double>(wild.rep.threshold[0]) / std::sqrt(2.0)) * static_cast<double>(inside), 0.002);
}

TEST(BpmV2Test, WithoutMarginsTheWholeFrameIsTheAreaAndMarginsThatLeaveNoAreaAreRefused) {
    Frames f = makeFrames(10, kDoses, 3, 7, Plant{false, true});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0u, r.rep.out_of_area_pixels);
    EXPECT_EQ(static_cast<ptrdiff_t>(kN), std::count(r.area.begin(), r.area.end(), static_cast<uint8_t>(1))) << "no margins: every pixel is inside";
    XpeBpmConfigV2 big{};
    big.margin_left = kW;   // nothing left
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(f, &big).rc);
    XpeBpmConfigV2 wide{};
    wide.margin_left = kW / 2;
    wide.margin_right = kW / 2 - 8;   // 8 columns left: fewer than the 16 the detector needs
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(f, &wide).rc);
}

TEST(BpmV2Test, ThresholdsFollowFromTheTargetNumberOfFalseAlarms) {
    Frames f = makeFrames(3, kDoses, 3, 7, Plant{false, false});
    XpeBpmConfigV2 one{}, hundred{};
    hundred.target_false_alarms = 100.0f;
    const Result a = detect(f, &one), b = detect(f, &hundred);
    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);
    ASSERT_EQ(static_cast<uint32_t>(kN), a.rep.pixels);
    for (int k : {0, 2, 4}) {   // the z features: erfc(lambda / sqrt 2) * N is the target
        EXPECT_NEAR(1.0, std::erfc(static_cast<double>(a.rep.threshold[k]) / std::sqrt(2.0)) * static_cast<double>(kN), 0.002) << k;
        EXPECT_NEAR(100.0, std::erfc(static_cast<double>(b.rep.threshold[k]) / std::sqrt(2.0)) * static_cast<double>(kN), 0.2) << k;
        EXPECT_GT(a.rep.threshold[k], b.rep.threshold[k]);
    }
    // the variance-ratio features: the chi-square upper tail P(chi2_nu / nu > tau) * N is the target. Checked by numerical integration of the density.
    // the linearity feature is judged on one side: P(z > lambda) * N is the target
    EXPECT_NEAR(1.0, 0.5 * std::erfc(static_cast<double>(a.rep.threshold[3]) / std::sqrt(2.0)) * static_cast<double>(kN), 0.002);
    EXPECT_NEAR(100.0, 0.5 * std::erfc(static_cast<double>(b.rep.threshold[3]) / std::sqrt(2.0)) * static_cast<double>(kN), 0.2);
    for (int k : {1}) {
        const uint32_t nu = a.rep.degrees_of_freedom[k];
        ASSERT_GT(nu, 0u) << k;
        auto tail = [&](double tau) {
            // density of chi-square_nu / nu at t: (nu/2)^(nu/2) / Gamma(nu/2) * t^(nu/2 - 1) * exp(-nu t / 2); integrated by Simpson on [tau, tau + 60]
            const double h = 0.5 * nu;
            auto pdf = [&](double t) { return std::exp(h * std::log(h) - std::lgamma(h) + (h - 1.0) * std::log(t) - h * t); };
            const int steps = 200000;
            const double lo = tau, hi = tau + 80.0, dx = (hi - lo) / steps;
            double s = pdf(lo) + pdf(hi);
            for (int i = 1; i < steps; ++i) s += pdf(lo + i * dx) * (i % 2 ? 4.0 : 2.0);
            return s * dx / 3.0;
        };
        EXPECT_NEAR(1.0, tail(static_cast<double>(a.rep.threshold[k])) * static_cast<double>(kN), 0.02) << k;
        EXPECT_NEAR(100.0, tail(static_cast<double>(b.rep.threshold[k])) * static_cast<double>(kN), 2.0) << k;
    }
    EXPECT_EQ(6u, a.rep.degrees_of_freedom[1] + 1u) << "dark noise: 7 raw frames, the first skipped by default: 6 used, 5 degrees of freedom";
    EXPECT_EQ(0u, a.rep.degrees_of_freedom[3]) << "the linearity feature is a z feature, not a variance ratio";
    // with the stricter target there are at least as many flags
    EXPECT_GE(b.rep.flagged_union, a.rep.flagged_union);
}

TEST(BpmV2Test, TheFirstDarkFrameIsSkippedOnRequest) {
    Frames f = makeFrames(4, kDoses, 3, 8, Plant{false, false}, true);   // a transient of +200 in the first dark frame only
    XpeBpmConfigV2 keep{}, skip{};
    keep.skip_first_dark = XPE_BPM_V2_SKIP_NONE;   // use every frame of the list
    skip.skip_first_dark = XPE_BPM_V2_SKIP_FIRST;
    const Result withFirst = detect(f, &keep), without = detect(f, &skip);
    ASSERT_EQ(XPE_OK, withFirst.rc);
    ASSERT_EQ(XPE_OK, without.rc);
    EXPECT_EQ(8u, withFirst.rep.dark_frames_used);
    EXPECT_EQ(7u, without.rep.dark_frames_used);
    auto at = [&](int x, int y) { return static_cast<size_t>(y) * kW + static_cast<size_t>(x); };
    size_t seen = 0, seenSkipped = 0;
    for (int y = 20; y < 24; ++y)
        for (int x = 20; x < 24; ++x) {
            seen += withFirst.map[at(x, y)] != 0;
            seenSkipped += without.map[at(x, y)] != 0;
        }
    EXPECT_GE(seen, 8u) << "the transient (+25 in the mean of 8 frames) is flagged when the first frame is used";
    EXPECT_EQ(0u, seenSkipped) << "and it is not when the first frame is skipped";
}

TEST(BpmV2Test, DoseGroupsComeFromTheSignalsWhateverTheFrameOrder) {
    Frames f = makeFrames(5, kDoses, 3, 7, Plant{false, false});
    const Result a = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, a.rc);
    EXPECT_EQ(4u, a.rep.dose_groups);
    for (int g = 0; g < 4; ++g) EXPECT_EQ(3u, a.rep.group_frames[g]);
    EXPECT_NEAR(2000.0, a.rep.group_signal[0], 40.0);
    EXPECT_NEAR(18000.0, a.rep.group_signal[3], 400.0);
    std::reverse(f.bright.begin(), f.bright.end());
    const Result b = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, b.rc);
    EXPECT_EQ(4u, b.rep.dose_groups);
    EXPECT_EQ(a.map, b.map) << "the order of the bright frames does not matter";
    // a looser ratio merges neighbouring doses (4500 / 2000 = 2.25 and 9000 / 4500 = 2 stay apart below 2, merge above 2.3)
    XpeBpmConfigV2 loose{};
    loose.dose_group_ratio = 2.3f;
    const Result c = detect(f, &loose);
    ASSERT_EQ(XPE_OK, c.rc);
    EXPECT_LT(c.rep.dose_groups, 4u);
}

TEST(BpmV2Test, TheLinearityFeatureNeedsThreeDoseGroups) {
    Frames f = makeFrames(6, {3000.0, 12000.0}, 4, 7, Plant{false, false});
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(2u, r.rep.dose_groups);
    EXPECT_EQ(1u, r.rep.feature_active[2]);
    EXPECT_EQ(0u, r.rep.feature_active[3]);
    Frames one = makeFrames(7, {6000.0}, 8, 7, Plant{false, false});
    const Result o = detect(one, nullptr);
    ASSERT_EQ(XPE_OK, o.rc);
    EXPECT_EQ(1u, o.rep.dose_groups);
    EXPECT_EQ(0u, o.rep.feature_active[2]);
    EXPECT_EQ(1u, o.rep.feature_active[4]) << "the flat-field feature still runs with one dose";
}

// QA-A-252c: the input is the raw acquisition list and its first dark frame is skipped unless the caller says it was trimmed already (SRS FUNC-022, method 2.1).
TEST(BpmV2Test, TheFirstDarkFrameOfTheRawListIsSkippedByDefault) {
    Frames f = makeFrames(11, kDoses, 3, 5, Plant{false, false});
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(f, nullptr).rc) << "five raw dark frames leave four after the skip: fewer than the minimum of five";
    const XpeBpmConfigV2 zeroed{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(f, &zeroed).rc) << "a zero-initialised configuration takes the same default";
    XpeBpmConfigV2 first{};
    first.skip_first_dark = XPE_BPM_V2_SKIP_FIRST;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(f, &first).rc);
    XpeBpmConfigV2 none{};
    none.skip_first_dark = XPE_BPM_V2_SKIP_NONE;
    const Result r = detect(f, &none);
    ASSERT_EQ(XPE_OK, r.rc) << "a list that was trimmed already uses all five";
    EXPECT_EQ(5u, r.rep.dark_frames_used);
    Frames g = makeFrames(11, kDoses, 3, 7, Plant{false, false});
    EXPECT_EQ(6u, detect(g, nullptr).rep.dark_frames_used) << "seven raw frames, the first skipped";
    XpeBpmConfigV2 bad{};
    bad.skip_first_dark = 3;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(g, &bad).rc) << "only 0, 1 and 2 are values of skip_first_dark";
}

TEST(BpmV2Test, RefusesInputsItCannotJudge) {
    Frames f = makeFrames(8, kDoses, 3, 7, Plant{false, false});
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : f.dark) db.push_back(buf(v));
    for (auto& v : f.bright) bb.push_back(buf(v));
    std::vector<uint8_t> map(kN);
    XpeImageBuffer mb = outBuf(map);
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    auto run = [&](const XpeImageBuffer* d, uint32_t nd, const XpeImageBuffer* b, uint32_t nb, const XpeBpmConfigV2* c, XpeImageBuffer* o) {
        return xpe_bpm_generate_v2(d, nd, b, nb, c, o, nullptr, nullptr, nullptr);
    };
    EXPECT_EQ(XPE_OK, run(db.data(), 7, bb.data(), 12, nullptr, &mb));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(nullptr, 7, bb.data(), 12, nullptr, &mb));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, bb.data(), 12, nullptr, nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 4, bb.data(), 12, nullptr, &mb)) << "four dark frames are fewer than the minimum of five";
    XpeBpmConfigV2 skip{};
    skip.skip_first_dark = 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 5, bb.data(), 12, &skip, &mb)) << "five frames minus the skipped first is four";
    EXPECT_EQ(XPE_OK, run(db.data(), 6, bb.data(), 12, &skip, &mb));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, bb.data(), 5, nullptr, &mb)) << "five bright frames are fewer than the minimum of six";
    XpeBpmConfigV2 bad{};
    bad.reserved = 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, bb.data(), 12, &bad, &mb));
    bad = XpeBpmConfigV2{};
    bad.window = 16;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, bb.data(), 12, &bad, &mb)) << "the window is odd";
    bad = XpeBpmConfigV2{};
    bad.target_false_alarms = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, bb.data(), 12, &bad, &mb));
    // a frame of another size, another format, and an output that is too small or of the wrong format
    XpeImageBuffer odd = bb[0];
    odd.width = kW - 1;
    std::vector<XpeImageBuffer> mixed = bb;
    mixed[3] = odd;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, mixed.data(), 12, nullptr, &mb));
    mixed = bb;
    mixed[3].format = XPE_PIXEL_FLOAT32;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, run(db.data(), 7, mixed.data(), 12, nullptr, &mb));
    XpeImageBuffer small = mb;
    small.dataSize = kN - 1;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, run(db.data(), 7, bb.data(), 12, nullptr, &small));
    XpeImageBuffer wrong = mb;
    wrong.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, run(db.data(), 7, bb.data(), 12, nullptr, &wrong));
    // a bright frame with no signal above the dark level
    std::vector<uint16_t> dim(kN, 0);
    std::vector<XpeImageBuffer> withDim = bb;
    withDim[0] = buf(dim);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, run(db.data(), 7, withDim.data(), 12, nullptr, &mb));
    xpe_preprocess_shutdown();
}

TEST(BpmV2Test, GivesTheSameMapForTheSameInputAndLeavesTheInputAlone) {
    Frames f = makeFrames(9, kDoses, 3, 7, Plant{true, true});
    const Frames copy = f;
    const Result a = detect(f, nullptr), b = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, a.rc);
    EXPECT_EQ(a.map, b.map);
    EXPECT_EQ(a.feat, b.feat);
    EXPECT_TRUE(copy.dark == f.dark);
    EXPECT_TRUE(copy.bright == f.bright);
    EXPECT_EQ(0, std::memcmp(&a.rep, &b.rep, sizeof a.rep));
}

}  // namespace
