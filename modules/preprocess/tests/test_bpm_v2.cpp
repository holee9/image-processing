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
    bool strongShading{false};
    bool respPixels{false};       // QA-A-252d: single pixels with a response of 0.40, 0.50, 0.60, 0.75, 1.45, 1.60 (row 220)
    bool respRowPixels{false};    // QA-A-252f: a row of gain 1.2 (row 100) and a row of gain 0.8 (row 150), each with one pixel of nominal response 0.46 and 0.50 (x 60)
    double   burstAmp{14.0};      // QA-A-253e: amplitude of the single-frame bursts / the noise of the some-frames stretches (see kLn*One, kLn*Frames)
    uint32_t frameMask{0};        // QA-A-253e: the frames (bit = frame index in acquisition order, dark or bright) that carry the noise of kLnDarkNoisyFrames / kLnBrightNoisyFrames
    uint32_t lines{0};            // QA-A-253: kLn* bits, whole rows and columns that are defective  // +-20 % gain shading along both directions (a heel-effect-like flat field): a line component taken from a partly defective line moves by many noise widths
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
constexpr int kVarCol = 70;                  // a column whose offset is a new random value in every frame (sd 60)
// line defects of the QA-A-253 tests (256 x 256 frames)
constexpr uint32_t kLnDeadRow = 1, kLnDeadCol = 2, kLnHalfRow = 4, kLnPartialRow = 8, kLnPairRows = 16, kLnTripleRows = 32, kLnNoisyRow = 64, kLnGainRow = 128, kLnMovingBurst = 256, kLnGainCol = 512, kLnBrightNoisyRow = 1024, kLnBrightBurst = 2048,
                 kLnBrightBurstMask = 2097152, kLnLowRow = 262144, kLnHighRow = 524288, kLnWeakRow = 1048576, kLnDarkBurstOne = 4096, kLnDarkNoisyFrames = 8192, kLnBrightBurstOne = 16384, kLnBrightNoisyFrames = 32768, kLnStagger2 = 65536, kLnStagger3 = 131072;
constexpr int kLnDeadRowY = 120, kLnDeadColX = 60, kLnHalfRowY = 80, kLnPartialRowY = 40, kLnPartialFrom = 96, kLnPartialTo = 192, kLnPairY = 170, kLnTripleY = 200, kLnNoisyY = 30, kLnGainY = 150;
// QA-A-253c: noise that moves. In dark frame k the stretch [96, 192) of row kBurstY0 + k gets an offset of +-14 (a different row in every frame)
constexpr int kBurstY0 = 100, kBurstFrom = 96, kBurstTo = 192;
// QA-A-253d: (the bright burst hits one row of a dose group in two of its three frames, with opposite signs, so that the dose response is untouched) a column with response 1.35 (a band that gain correction removes; 1.2 in the evaluation, the larger factor makes the shift of the feature show on 256 rows), a row that is noisy in every bright frame, and a burst that moves from row to row between the bright frames
constexpr int kLnGainColX = 180, kLnBrightNoisyY = 60, kBrightBurstY0 = 100;
// QA-A-253e: a burst in one frame only (row 140), noise in some frames only (row 160), adjacent dead rows whose ends differ (rows 210..212)
constexpr int kSingleY = 140, kSomeFramesY = 160, kStagY = 210;
// QA-A-252d: response rows (0.3 and 1.7 are defects, 0.6 is weak) and the response pixels
constexpr int kLnLowRowY = 70, kLnHighRowY = 190, kLnWeakRowY = 50, kRespY = 220;
constexpr int kGainRowHi = 100, kGainRowLo = 150, kRowPixX = 60;   // QA-A-252f
constexpr int kRespX[6] = {30, 40, 50, 60, 70, 80};
constexpr double kRespR[6] = {0.40, 0.50, 0.60, 0.75, 1.45, 1.60};
constexpr int kStagFrom[3] = {96, 120, 140}, kStagTo[3] = {192, 216, 236};

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
    auto put = [&](double net, bool bright, bool firstDark, int darkIdx, int brightIdx) {
        std::vector<uint16_t> fr(kN);
        const double columnOffset = plant.varyingColumn ? 60.0 * frameRng.next() : 0.0;
        for (size_t i = 0; i < kN; ++i) {
            const int x = static_cast<int>(i % kW), y = static_cast<int>(i / kW);
            double d = D0[i], sig = net * G[i];
            double noiseSd = 3.0;
            if (plant.darkPartial) {
                d += 0.3 * x;
                if (y == kDarkRow && x >= kDarkFrom && x < kDarkFrom + kPartLen) d += 300.0;
            }
            if (plant.lines) {
                if ((plant.lines & kLnDeadRow) && y == kLnDeadRowY) sig = 0.0;
                if ((plant.lines & kLnDeadCol) && x == kLnDeadColX) sig = 0.0;
                if ((plant.lines & kLnHalfRow) && y == kLnHalfRowY) sig *= 0.5;
                if ((plant.lines & kLnPartialRow) && y == kLnPartialRowY && x >= kLnPartialFrom && x < kLnPartialTo) sig = 0.0;
                if ((plant.lines & kLnPairRows) && (y == kLnPairY || y == kLnPairY + 1)) sig = 0.0;
                if ((plant.lines & kLnTripleRows) && y >= kLnTripleY && y < kLnTripleY + 3) sig = 0.0;
                if ((plant.lines & kLnNoisyRow) && y == kLnNoisyY) noiseSd = 9.0;
                if ((plant.lines & kLnGainRow) && y == kLnGainY) sig *= 1.2;
                if ((plant.lines & kLnLowRow) && y == kLnLowRowY) sig *= 0.3;
                if ((plant.lines & kLnHighRow) && y == kLnHighRowY) sig *= 1.7;
                if ((plant.lines & kLnWeakRow) && y == kLnWeakRowY) sig *= 0.6;
                if ((plant.lines & kLnDarkBurstOne) && darkIdx == 3 && y == kSingleY && x >= kBurstFrom && x < kBurstTo) d += plant.burstAmp;
                if ((plant.lines & kLnDarkNoisyFrames) && darkIdx >= 0 && ((plant.frameMask >> darkIdx) & 1u) && y == kSomeFramesY && x >= kBurstFrom && x < kBurstTo) noiseSd = plant.burstAmp;
                if ((plant.lines & kLnBrightBurstOne) && bright && brightIdx == 1 && y == kSingleY && x >= kBurstFrom && x < kBurstTo) d += plant.burstAmp * 0.5 * std::sqrt(std::max(sig, 1.0));
                if ((plant.lines & kLnBrightBurstMask) && bright && brightIdx >= 0 && ((plant.frameMask >> brightIdx) & 1u) && y == kSingleY && x >= kBurstFrom && x < kBurstTo)
                    d += plant.burstAmp * 0.5 * std::sqrt(std::max(sig, 1.0));
                if ((plant.lines & kLnBrightNoisyFrames) && bright && brightIdx >= 0 && ((plant.frameMask >> brightIdx) & 1u) && y == kSomeFramesY && x >= kBurstFrom && x < kBurstTo) noiseSd = plant.burstAmp;
                for (int r = 0; r < 3; ++r) {
                    const bool on = (r < 2 && (plant.lines & kLnStagger2)) || (plant.lines & kLnStagger3);
                    if (on && y == kStagY + r && x >= kStagFrom[r] && x < kStagTo[r]) sig = 0.0;
                }
                if ((plant.lines & kLnGainCol) && x == kLnGainColX) sig *= 1.35;
                if ((plant.lines & kLnBrightNoisyRow) && bright && y == kLnBrightNoisyY) noiseSd = 90.0;
                if ((plant.lines & kLnBrightBurst) && bright && brightIdx >= 0 && brightIdx % 3 != 2 && y == kBrightBurstY0 + brightIdx / 3 && x >= kBurstFrom && x < kBurstTo)
                    d += (brightIdx % 3 == 0 ? 3.0 : -3.0) * std::sqrt(std::max(sig, 1.0));   // +-6 quantum sd in two frames of a dose group (the group mean stays), a different row for every group
                if ((plant.lines & kLnMovingBurst) && darkIdx >= 0 && y == kBurstY0 + darkIdx && x >= kBurstFrom && x < kBurstTo) d += (darkIdx % 2 ? -14.0 : 14.0);
            }
            if (plant.respPixels && y == kRespY)
                for (int q = 0; q < 6; ++q)
                    if (x == kRespX[q]) sig *= kRespR[q];
            if (plant.respRowPixels) {
                if (y == kGainRowHi) sig *= x == kRowPixX ? 1.2 * 0.46 : 1.2;
                if (y == kGainRowLo) sig *= x == kRowPixX ? 0.8 * 0.50 : 0.8;
            }
            if (plant.yGradient) sig *= 0.93 + 0.14 * y / (kH - 1);
            if (plant.strongShading) sig *= (0.8 + 0.4 * x / (kW - 1)) * (0.8 + 0.4 * y / (kH - 1));
            if (plant.varyingColumn && x == kVarCol) d += columnOffset;
            if (plant.partialLines) {
                if ((y == kPartRow || y == kPartRowPair || y == kPartRowPair + 1) && x >= kPartRowFrom && x < kPartRowFrom + kPartLen) sig = 0.0;
                if (x == kPartCol && y < kPartLen) sig = 0.0;
            }
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
    for (int k = 0; k < nDark; ++k) f.dark.push_back(put(0.0, false, transientFirstDark && k == 0, k, -1));
    for (double d : doses)
        for (int k = 0; k < framesPerDose; ++k) {
            f.bright.push_back(put(d, true, false, -1, static_cast<int>(f.bright.size())));
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
    // a response of 1.10 is inside 45 %..150 % of the local median: not a defect (SRS FUNC-022), and the slope feature no longer flags pixels
    EXPECT_EQ(0, r.feat[at(kSlopeX, kSlopeY)] & (XPE_BPM_V2_FEATURE_FLAT | XPE_BPM_V2_FEATURE_DOSE_SLOPE));
    EXPECT_EQ(0, r.map[at(kSlopeX, kSlopeY)]);
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

// ---------------------------------------------------------------- QA-A-253: line defects (SRS FUNC-023) and blobs
namespace {

struct LinesResult {
    XpeErrorCode               rc;
    std::vector<XpeBpmLine>    lines, unconfirmed;
    std::vector<uint8_t>       mask;
    XpeBpmLineReport           rep;
};

LinesResult findLines(Frames& f, const XpeBpmConfigV2* cfg, const XpeBpmLineConfig* lcfg) {
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : f.dark) db.push_back(buf(v));
    for (auto& v : f.bright) bb.push_back(buf(v));
    LinesResult r{};
    r.mask.assign(kN, 77);
    r.lines.assign(64, XpeBpmLine{});
    r.unconfirmed.assign(64, XpeBpmLine{});
    XpeImageBuffer mb = outBuf(r.mask);
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    r.rc = xpe_bpm_find_lines(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), cfg, lcfg, &mb, r.lines.data(), 64, r.unconfirmed.data(), 64, &r.rep);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    r.lines.resize(std::min<size_t>(r.rep.lines, 64));
    r.unconfirmed.resize(std::min<size_t>(r.rep.unconfirmed, 64));
    return r;
}

XpeBpmLineConfig shortWindows() {
    XpeBpmLineConfig c{};
    c.min_segment = 32;   // the test frames are 256 pixels wide
    return c;
}

Plant onlyLines(uint32_t bits) {
    Plant p{};
    p.stripes = false;
    p.defects = false;
    p.lines = bits;
    return p;
}

const XpeBpmLine* lineAt(const LinesResult& r, uint32_t orientation, uint32_t index) {
    for (const XpeBpmLine& l : r.lines)
        if (l.orientation == orientation && l.index <= index && index < l.index + l.width) return &l;
    return nullptr;
}

}  // namespace

TEST(BpmV2LinesTest, AFullDeadRowAndColumnAreFoundAsFullLinesAndTheyCross) {
    Frames f = makeFrames(21, kDoses, 3, 7, onlyLines(kLnDeadRow | kLnDeadCol));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    ASSERT_EQ(2u, r.rep.lines) << "one row and one column, nothing else";
    const XpeBpmLine* row = lineAt(r, XPE_BPM_LINE_ROW, kLnDeadRowY);
    const XpeBpmLine* col = lineAt(r, XPE_BPM_LINE_COLUMN, kLnDeadColX);
    ASSERT_NE(nullptr, row);
    ASSERT_NE(nullptr, col);
    EXPECT_EQ(static_cast<uint32_t>(kLnDeadRowY), row->index);
    EXPECT_EQ(1u, row->width);
    EXPECT_NE(0u, row->flags & XPE_BPM_LINE_FULL);
    EXPECT_NE(0u, row->causes & XPE_BPM_LINE_CAUSE_RESPONSE);
    EXPECT_NE(0u, col->flags & XPE_BPM_LINE_FULL);
    EXPECT_NE(0u, row->flags & XPE_BPM_LINE_CROSSES) << "the dead column crosses the dead row";
    EXPECT_NE(0u, col->flags & XPE_BPM_LINE_CROSSES);
    EXPECT_EQ(1, r.mask[static_cast<size_t>(kLnDeadRowY) * kW + 5]);
    EXPECT_EQ(1, r.mask[static_cast<size_t>(5) * kW + kLnDeadColX]);
    EXPECT_EQ(0, r.mask[static_cast<size_t>(5) * kW + 5]);
    EXPECT_EQ(0u, r.rep.panel_reject);
    EXPECT_EQ(1u, r.rep.crossings);
}

TEST(BpmV2LinesTest, ALineWhoseCorrectionWouldRaiseTheNoiseByMoreThanTheToleranceIsADefectButAGainBandIsNot) {
    // QA-A-252d: a response below 45 % or above 150 % is a defect; 0.5 (row 80), 0.6 (row 50), 1.2 (row 150) and the 0.8 column are not; 0.3 (row 70) and 1.7 (row 190) are
    Frames f = makeFrames(22, kDoses, 3, 7, onlyLines(kLnHalfRow | kLnGainRow | kLnLowRow | kLnHighRow | kLnWeakRow));
    Frames g = makeFrames(22, kDoses, 3, 7, Plant{true, false});   // the fixed row band of +60 and the gain column band of 0.8
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc), bands = findLines(g, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    ASSERT_EQ(XPE_OK, bands.rc);
    EXPECT_NE(nullptr, lineAt(r, XPE_BPM_LINE_ROW, kLnLowRowY)) << "response 0.3: found";
    if (const XpeBpmLine* low = lineAt(r, XPE_BPM_LINE_ROW, kLnLowRowY)) {   // QA-A-252f: the response of the offset-corrected image and the slope proxy are recorded apart
        EXPECT_NE(0u, low->causes & XPE_BPM_LINE_CAUSE_RESPONSE) << "the ratio of the offset-corrected image";
        EXPECT_NE(0u, low->causes & XPE_BPM_LINE_CAUSE_SLOPE) << "the slope proxy has its own cause bit";
    }
    EXPECT_NE(nullptr, lineAt(r, XPE_BPM_LINE_ROW, kLnHighRowY)) << "response 1.7: found";
    EXPECT_EQ(nullptr, lineAt(r, XPE_BPM_LINE_ROW, kLnHalfRowY)) << "response 0.5: inside 45 %..150 %, normal";
    EXPECT_EQ(nullptr, lineAt(r, XPE_BPM_LINE_ROW, kLnWeakRowY)) << "response 0.6: weak, counted only";
    EXPECT_EQ(nullptr, lineAt(r, XPE_BPM_LINE_ROW, kLnGainY)) << "response 1.2: a band that gain correction removes";
    EXPECT_EQ(2u, r.rep.lines);
    EXPECT_GE(r.rep.weak_response_stretches, 2u) << "the rows at 0.5 and 0.6 are weak responses";
    EXPECT_EQ(0u, bands.rep.lines) << "the fixed offset band and the 0.8 gain band are not defects";
}

TEST(BpmV2LinesTest, APartialLineIsFoundWithItsStartAndEnd) {
    Frames f = makeFrames(23, kDoses, 3, 7, onlyLines(kLnPartialRow));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    ASSERT_EQ(1u, r.rep.lines);
    const XpeBpmLine& l = r.lines[0];
    EXPECT_EQ(XPE_BPM_LINE_ROW, l.orientation);
    EXPECT_EQ(static_cast<uint32_t>(kLnPartialRowY), l.index);
    EXPECT_EQ(0u, l.flags & XPE_BPM_LINE_FULL);
    EXPECT_NEAR(kLnPartialFrom, static_cast<double>(l.start), 2.0) << "the start is the change point, not the edge of a coarse window";
    EXPECT_NEAR(kLnPartialTo, static_cast<double>(l.end), 2.0);
    EXPECT_EQ(1u, r.rep.partial_lines);
    EXPECT_EQ(0u, r.rep.full_lines);
}

TEST(BpmV2LinesTest, TwoAdjacentLinesAreADoubleLineAndThreeSetThePanelRejection) {
    Frames f = makeFrames(24, kDoses, 3, 7, onlyLines(kLnPairRows));
    Frames g = makeFrames(24, kDoses, 3, 7, onlyLines(kLnTripleRows));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult two = findLines(f, nullptr, &lc), three = findLines(g, nullptr, &lc);
    ASSERT_EQ(XPE_OK, two.rc);
    ASSERT_EQ(XPE_OK, three.rc);
    ASSERT_EQ(1u, two.rep.lines);
    EXPECT_EQ(2u, two.lines[0].width);
    EXPECT_EQ(static_cast<uint32_t>(kLnPairY), two.lines[0].index);
    EXPECT_EQ(1u, two.rep.double_lines);
    EXPECT_EQ(0u, two.rep.panel_reject) << "two adjacent lines are a correction task (the line-direction interpolation handles them)";
    ASSERT_EQ(1u, three.rep.lines);
    EXPECT_EQ(3u, three.lines[0].width);
    EXPECT_EQ(1u, three.rep.panel_reject) << "three adjacent lines: the panel is rejected, not corrected";
    EXPECT_NE(0u, three.lines[0].flags & XPE_BPM_LINE_REJECT);
    EXPECT_EQ(3u, three.rep.max_adjacent);
}

TEST(BpmV2LinesTest, ARowWithMoreTemporalNoiseIsFoundAndCleanFramesGiveNoLine) {
    Frames f = makeFrames(25, kDoses, 3, 7, onlyLines(kLnNoisyRow));
    Frames c = makeFrames(25, kDoses, 3, 7, onlyLines(0));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult noisy = findLines(f, nullptr, &lc), clean = findLines(c, nullptr, &lc);
    ASSERT_EQ(XPE_OK, noisy.rc);
    ASSERT_EQ(XPE_OK, clean.rc);
    const XpeBpmLine* l = lineAt(noisy, XPE_BPM_LINE_ROW, kLnNoisyY);
    ASSERT_NE(nullptr, l);
    EXPECT_NE(0u, l->causes & XPE_BPM_LINE_CAUSE_NOISE);
    EXPECT_GE(l->persistence, 0.8f) << "a row that is noisy at its place is large in (nearly) every dark frame";
    EXPECT_EQ(0u, clean.rep.lines) << "clean frames: no line (the false-alarm target is one stretch over the whole frame)";
}

TEST(BpmV2LinesTest, NoiseThatHitsADifferentRowInEveryFrameIsNoDefectOfAPlace) {
    // a stretch of 96 pixels gets an offset of +-14 in one dark frame, a different row each time: large in the aggregate over the frames, large in one frame only
    Frames f = makeFrames(29, kDoses, 3, 7, onlyLines(kLnMovingBurst));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0u, r.rep.lines) << "persistence: the same stretch has to be large in more than half of the dark frames";
}

TEST(BpmV2LinesTest, AColumnWithResponse1Point35IsABandThatGainCorrectionRemovesNotADefect) {
    // the linearity feature is log(sse); sse grows with the square of the response, so the column stands 0.37 above its neighbours in that feature unless it is made relative
    Frames f = makeFrames(30, kDoses, 3, 7, onlyLines(kLnGainCol));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0u, r.rep.lines);
}

TEST(BpmV2LinesTest, ARowThatIsNoisyInEveryBrightFrameIsFoundWithFullPersistence) {
    Frames f = makeFrames(31, kDoses, 3, 7, onlyLines(kLnBrightNoisyRow));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    const XpeBpmLine* l = lineAt(r, XPE_BPM_LINE_ROW, kLnBrightNoisyY);
    ASSERT_NE(nullptr, l);
    EXPECT_NE(0u, l->causes & XPE_BPM_LINE_CAUSE_NOISE);
    EXPECT_GE(l->persistence, 0.8f);
}

TEST(BpmV2LinesTest, NoiseThatHitsADifferentRowInEveryBrightFrameIsNoDefectOfAPlace) {
    Frames f = makeFrames(32, kDoses, 3, 7, onlyLines(kLnBrightBurst));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0u, r.rep.lines) << (r.lines.empty() ? std::string() : "first group: orientation " + std::to_string(r.lines[0].orientation) + " index " + std::to_string(r.lines[0].index) + " causes " + std::to_string(r.lines[0].causes) + " persistence " + std::to_string(r.lines[0].persistence));
}

// ---------------------------------------------------------------- QA-A-253e: persistence against a robust reference, and the mask
namespace {
Plant lonePlant(uint32_t bits, double amp, uint32_t mask = 0) {
    Plant p = onlyLines(bits);
    p.burstAmp = amp;
    p.frameMask = mask;
    return p;
}
const XpeBpmLine* noiseGroupAt(const LinesResult& r, uint32_t index) {
    for (const XpeBpmLine& l : r.lines)
        if (l.orientation == XPE_BPM_LINE_ROW && (l.causes & XPE_BPM_LINE_CAUSE_NOISE) && l.index <= index && index < l.index + l.width) return &l;
    return nullptr;
}
}  // namespace

TEST(BpmV2LinesTest, ABurstInOneDarkFrameIsNoDefectOfAPlaceWhateverItsSizeAndTheNumberOfFrames) {
    // a burst of +40 and +100 in one dark frame: the mean of the stretch moves with it, so against the MEAN every other frame looks large in the opposite direction
    for (int rawDark : {4, 8})        // 3 and 7 frames after the first is skipped
        for (double amp : {40.0, 100.0}) {
            Frames f = makeFrames(40, kDoses, 3, rawDark, lonePlant(kLnDarkBurstOne, amp));
            XpeBpmConfigV2 cfg{};
            cfg.min_frames_dark = 3;
            const XpeBpmLineConfig lc = shortWindows();
            const LinesResult r = findLines(f, &cfg, &lc);
            ASSERT_EQ(XPE_OK, r.rc) << rawDark << " " << amp;
            EXPECT_EQ(nullptr, noiseGroupAt(r, kSingleY)) << "raw dark frames " << rawDark << ", burst " << amp << ": one frame is not persistence";
        }
}

TEST(BpmV2LinesTest, NoiseInMostDarkFramesIsADefectAndNoiseInAFewIsNot) {
    const XpeBpmLineConfig lc = shortWindows();
    // seven used frames (raw 8, the first skipped: raw indices 1..7); the stretch is noisy (sd 20) in frames 2,3,4,5 = 4 of 7 (a majority) or in 2,3 = 2 of 7
    Frames most = makeFrames(41, kDoses, 3, 8, lonePlant(kLnDarkNoisyFrames, 20.0, (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5)));
    Frames few = makeFrames(41, kDoses, 3, 8, lonePlant(kLnDarkNoisyFrames, 20.0, (1u << 2) | (1u << 3)));
    const LinesResult a = findLines(most, nullptr, &lc), b = findLines(few, nullptr, &lc);
    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);
    const XpeBpmLine* l = noiseGroupAt(a, kSomeFramesY);
    ASSERT_NE(nullptr, l) << "4 of 7 frames";
    EXPECT_GT(l->persistence, 0.5f);
    EXPECT_EQ(nullptr, noiseGroupAt(b, kSomeFramesY)) << "2 of 7 frames";
}

TEST(BpmV2LinesTest, ABurstInOneBrightFrameIsNoNoiseDefectWhateverTheSizeOfTheDoseGroup) {
    // the burst also shifts the dose-group mean and can look like a local nonlinearity (cause linearity); the noise cause must not report it.
    // A single dose group of 3 or 5 frames (the mean of the group moves with the burst; the median does not), and four groups of two frames (a pair has no median: the judgement
    // pools the frames of all groups, so the pair that carries the burst is 2 of 8)
    struct Case { std::vector<double> doses; int perDose; };
    for (const Case& c : {Case{{9000.0}, 3}, Case{{9000.0}, 5}, Case{kDoses, 2}})
        for (double amp : {8.0, 20.0}) {
            Frames f = makeFrames(42, c.doses, c.perDose, 7, lonePlant(kLnBrightBurstOne, amp));
            XpeBpmConfigV2 cfg{};
            cfg.min_frames_bright = 2;
            const XpeBpmLineConfig lc = shortWindows();
            const LinesResult r = findLines(f, &cfg, &lc);
            ASSERT_EQ(XPE_OK, r.rc);
            EXPECT_EQ(nullptr, noiseGroupAt(r, kSingleY)) << c.doses.size() << " dose(s), " << c.perDose << " frames per dose, burst of " << amp << " quantum sd";
        }
}

TEST(BpmV2LinesTest, NoiseInMostBrightFramesIsADefectAndNoiseInAFewIsNot) {
    const XpeBpmLineConfig lc = shortWindows();
    uint32_t four = 0, two = 0;
    for (int j = 0; j < 4; ++j) four |= 1u << j;
    for (int j = 0; j < 2; ++j) two |= 1u << j;
    XpeBpmConfigV2 cfg{};
    cfg.min_frames_bright = 2;
    // one dose group of 7 frames: noise in 4 of them (a majority) or in 2
    Frames most = makeFrames(43, {9000.0}, 7, 7, lonePlant(kLnBrightNoisyFrames, 90.0, four));
    Frames few = makeFrames(43, {9000.0}, 7, 7, lonePlant(kLnBrightNoisyFrames, 90.0, two));
    const LinesResult a = findLines(most, &cfg, &lc), b = findLines(few, &cfg, &lc);
    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);
    const XpeBpmLine* l = noiseGroupAt(a, kSomeFramesY);
    ASSERT_NE(nullptr, l) << "4 of 7 frames";
    EXPECT_GT(l->persistence, 0.5f);
    EXPECT_EQ(nullptr, noiseGroupAt(b, kSomeFramesY)) << "2 of 7 frames";
}

TEST(BpmV2LinesTest, ANoisyRowThatCannotBeJudgedFrameByFrameIsListedApartAndNeverPainted) {
    // two bright frames in all: no majority is possible; the stretch found in the bright frames is an unconfirmed candidate, not a defect
    Frames f = makeFrames(45, {9000.0}, 2, 7, lonePlant(kLnBrightNoisyFrames, 400.0, 3u));
    XpeBpmConfigV2 cfg{};
    cfg.min_frames_bright = 2;
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, &cfg, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(nullptr, noiseGroupAt(r, kSomeFramesY)) << "not confirmed";
    ASSERT_GE(r.rep.unconfirmed, 1u);
    const XpeBpmLine* u = nullptr;
    for (const XpeBpmLine& l : r.unconfirmed)
        if (l.orientation == XPE_BPM_LINE_ROW && l.index == static_cast<uint32_t>(kSomeFramesY)) u = &l;
    ASSERT_NE(nullptr, u) << "it is on the list of unconfirmed candidates, with its place";
    EXPECT_EQ(XPE_BPM_LINE_REASON_TOO_FEW_FRAMES, u->reason);
    EXPECT_FLOAT_EQ(-1.0f, u->persistence);
    for (uint32_t x = u->start; x < u->end; ++x) EXPECT_EQ(0, r.mask[static_cast<size_t>(kSomeFramesY) * kW + x]) << "an unconfirmed candidate is never painted: x " << x;
    EXPECT_EQ(2u, r.rep.bright_frames_raw);
    EXPECT_EQ(0u, r.rep.bright_frames_valid) << "a pair is not persistence evidence";
    EXPECT_EQ(7u, r.rep.dark_frames_raw);
    EXPECT_EQ(6u, r.rep.dark_frames_valid) << "the first dark frame is skipped";
}

TEST(BpmV2LinesTest, ABurstInOneFramePerPairIsNoPersistenceEvidence) {
    // Codex #211: [100,0] [100,0] and [100,0] [100,0] [0,0]. A pair cannot tell which of its frames is odd: its frames count neither for nor against persistence
    for (const std::vector<double>& doses : {std::vector<double>{2000.0, 4500.0}, std::vector<double>{2000.0, 4500.0, 9000.0}}) {
        Frames f = makeFrames(48, doses, 2, 7, lonePlant(kLnBrightBurstMask, 20.0, 5u));   // frames 0 and 2: the first frame of the first and of the second pair
        XpeBpmConfigV2 cfg{};
        cfg.min_frames_bright = 2;
        const XpeBpmLineConfig lc = shortWindows();
        const LinesResult r = findLines(f, &cfg, &lc);
        ASSERT_EQ(XPE_OK, r.rc);
        EXPECT_EQ(nullptr, noiseGroupAt(r, kSingleY)) << doses.size() << " pairs: not confirmed";
        // with three dose groups the burst also shifts a group mean and can look like a local nonlinearity (cause linearity, a known limit that persistence does not judge);
        // with two there is no linearity feature, so nothing at all may be painted
        if (doses.size() == 2)
            for (uint32_t x = static_cast<uint32_t>(kBurstFrom); x < static_cast<uint32_t>(kBurstTo); ++x)
                EXPECT_EQ(0, r.mask[static_cast<size_t>(kSingleY) * kW + x]) << "nothing painted at x " << x;
        EXPECT_EQ(0u, r.rep.bright_frames_valid);
        EXPECT_EQ(static_cast<uint32_t>(doses.size() * 2), r.rep.bright_frames_raw);
    }
}

TEST(BpmV2LinesTest, TheMaskPaintsTheUnionOfTheStretchesNotTheBoundingBoxOfTheGroup) {
    const XpeBpmLineConfig lc = shortWindows();
    for (int rows : {2, 3}) {
        Frames f = makeFrames(44, kDoses, 3, 7, onlyLines(rows == 2 ? kLnStagger2 : kLnStagger3));
        const LinesResult r = findLines(f, nullptr, &lc);
        ASSERT_EQ(XPE_OK, r.rc);
        ASSERT_EQ(1u, r.rep.lines) << rows << " adjacent rows with different ends are one group";
        EXPECT_EQ(static_cast<uint32_t>(rows), r.lines[0].width);
        // the ends are change-point estimates (a pixel or two off); what matters is that a stretch is not widened to the bounding box of the group (which would paint 24 to 44
        // more pixels per row here)
        constexpr int tol = 3;
        size_t painted = 0, expected = 0;
        for (int y = 0; y < static_cast<int>(kH); ++y)
            for (int x = 0; x < static_cast<int>(kW); ++x) {
                const int  rr = y - kStagY;
                const bool inRow = rr >= 0 && rr < rows;
                const bool inside = inRow && x >= kStagFrom[rr] && x < kStagTo[rr];
                const bool wellInside = inRow && x >= kStagFrom[rr] + tol && x < kStagTo[rr] - tol;
                const bool nearIt = inRow && x >= kStagFrom[rr] - tol && x < kStagTo[rr] + tol;
                const uint8_t m = r.mask[static_cast<size_t>(y) * kW + x];
                expected += inside ? 1 : 0;
                if (wellInside) EXPECT_EQ(1, m) << x << "," << y;
                if (!nearIt) EXPECT_EQ(0, m) << "a pixel outside every stretch is painted: " << x << "," << y;
                painted += m;
            }
        EXPECT_NEAR(static_cast<double>(expected), static_cast<double>(painted), 2.0 * tol * rows) << "the mask is the union of the stretches";
        EXPECT_EQ(static_cast<uint32_t>(painted), r.rep.line_pixels) << "the report counts the stretches too, not the box";
    }
}

TEST(BpmV2LinesTest, ASingleDeadPixelOrASmallBlockIsNotALine) {
    Frames f = makeFrames(26, kDoses, 3, 7, Plant{false, true});   // a hot, a dead, a slope and a noisy pixel
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0u, r.rep.lines) << "pixels are the pixel detector's job; a stretch needs min_segment pixels in a row";
}

TEST(BpmV2LinesTest, LinesInsideTheMarginsAreNotCounted) {
    Frames f = makeFrames(27, kDoses, 3, 7, onlyLines(kLnDeadRow));
    XpeBpmConfigV2 cfg{};
    cfg.margin_top = kLnDeadRowY - 2;   // the dead row is the third line outside the area counted from the margin's inner edge... it lies inside: the first control
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult inside = findLines(f, &cfg, &lc);
    ASSERT_EQ(XPE_OK, inside.rc);
    ASSERT_EQ(1u, inside.rep.lines) << "inside the imaging area";
    EXPECT_EQ(static_cast<uint32_t>(kLnDeadRowY), inside.lines[0].index) << "frame coordinates, not coordinates of the cut area";
    cfg.margin_top = kLnDeadRowY + 5;   // now the dead row is in the margin
    const LinesResult outside = findLines(f, &cfg, &lc);
    ASSERT_EQ(XPE_OK, outside.rc);
    EXPECT_EQ(0u, outside.rep.lines) << "a row in the margin takes no part";
    EXPECT_EQ(0, outside.mask[static_cast<size_t>(kLnDeadRowY) * kW + 5]);
}

TEST(BpmV2LinesTest, ConfigurationAndBuffersAreChecked) {
    Frames f = makeFrames(28, kDoses, 3, 7, onlyLines(0));
    XpeBpmLineConfig bad{};
    bad.reserved = 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, findLines(f, nullptr, &bad).rc);
    bad = XpeBpmLineConfig{};
    bad.min_segment = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, findLines(f, nullptr, &bad).rc);
    bad = XpeBpmLineConfig{};
    bad.noise_tolerance_db = 30.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, findLines(f, nullptr, &bad).rc);
    bad = XpeBpmLineConfig{};
    bad.target_false_alarms = 5000.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, findLines(f, nullptr, &bad).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bpm_find_lines(nullptr, 7, nullptr, 12, nullptr, nullptr, nullptr, nullptr, 0, nullptr, 0, nullptr));
}

TEST(BpmV2LinesTest, BlobsGroupNeighbouringDefectivePixelsWithTheirSize) {
    std::vector<uint8_t> m(64 * 64, 0);
    auto set = [&](int x, int y) { m[static_cast<size_t>(y) * 64 + static_cast<size_t>(x)] = 3; };
    set(5, 5);                                                         // a single pixel
    set(20, 20); set(21, 20); set(20, 21); set(21, 21);                // 2 x 2
    set(40, 40); set(41, 41); set(42, 42);                             // a diagonal: 8-neighbourhood joins it
    set(10, 50); set(10, 51); set(10, 52); set(11, 52); set(12, 52);   // an L of five
    XpeImageBuffer b{};
    b.data = m.data();
    b.width = 64;
    b.height = 64;
    b.format = XPE_PIXEL_UINT8;
    b.dataSize = m.size();
    uint32_t count = 0;
    XpeBpmBlob out[8] = {};
    ASSERT_EQ(XPE_OK, xpe_bpm_blobs(&b, out, 8, &count));
    ASSERT_EQ(4u, count);
    EXPECT_EQ(5u, out[0].size) << "largest first";
    EXPECT_EQ(10u, out[0].x0);
    EXPECT_EQ(12u, out[0].x1);
    EXPECT_EQ(50u, out[0].y0);
    EXPECT_EQ(52u, out[0].y1);
    EXPECT_EQ(4u, out[1].size);
    EXPECT_EQ(3u, out[2].size);
    EXPECT_EQ(1u, out[3].size);
    // counting without an array, a small array, bad input
    uint32_t only = 0;
    ASSERT_EQ(XPE_OK, xpe_bpm_blobs(&b, nullptr, 0, &only));
    EXPECT_EQ(4u, only);
    XpeBpmBlob two[2] = {};
    ASSERT_EQ(XPE_OK, xpe_bpm_blobs(&b, two, 2, &only));
    EXPECT_EQ(4u, only) << "the count is the number found, not the number stored";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bpm_blobs(nullptr, out, 8, &count));
    b.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_bpm_blobs(&b, out, 8, &count));
}

TEST(BpmV2Test, TheResponseRuleFlagsBelow45PercentAndAbove150PercentAndCountsTheWeakOnes) {
    Plant p{};
    p.stripes = false;
    p.defects = false;
    p.respPixels = true;
    Frames f = makeFrames(46, kDoses, 3, 7, p);
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    for (int q = 0; q < 6; ++q) {
        const bool defect = kRespR[q] < 0.45 || kRespR[q] > 1.5;
        const bool flagged = (r.feat[static_cast<size_t>(kRespY) * kW + kRespX[q]] & XPE_BPM_V2_FEATURE_FLAT) != 0;
        EXPECT_EQ(defect, flagged) << "response " << kRespR[q];
    }
    EXPECT_EQ(2u, r.rep.weak_response_pixels) << "0.50 and 0.60 are from 45 % up to 70 %: weak, counted, not flagged";
}

TEST(BpmV2LinesTest, AGainBandIsNotALinearityDefectWhateverTheNumberOfDoseGroups) {
    // eight dose groups make the linearity statistic sharp enough for the shift of log(sse) by a gain band (x1.35: +0.6) to stand out when it is not made relative
    const std::vector<double> doses8 = {1000.0, 2000.0, 3000.0, 4500.0, 6000.0, 9000.0, 13000.0, 18000.0};
    Frames f = makeFrames(47, doses8, 3, 7, onlyLines(kLnGainCol));
    const XpeBpmLineConfig lc = shortWindows();
    const LinesResult r = findLines(f, nullptr, &lc);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0u, r.rep.lines);
}

TEST(BpmV2Test, TheResponseIsJudgedAgainstTheLocalMedianOfTheOffsetCorrectedImage) {
    // QA-A-252f (Codex #213 finding 1): the row factor is not taken out before the judgement. A pixel of nominal 0.46 on a row of gain 1.2 reads 0.55 of its surroundings
    // (normal); a pixel of nominal 0.50 on a row of gain 0.8 reads 0.40 of its surroundings (a defect), although it is 0.50 of its own row.
    Plant p{};
    p.stripes = false;
    p.defects = false;
    p.respRowPixels = true;
    Frames f = makeFrames(48, kDoses, 3, 7, p);
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_EQ(0, r.feat[static_cast<size_t>(kGainRowHi) * kW + kRowPixX] & XPE_BPM_V2_FEATURE_FLAT) << "0.46 on a row of gain 1.2: 0.55 of the local median";
    EXPECT_NE(0, r.feat[static_cast<size_t>(kGainRowLo) * kW + kRowPixX] & XPE_BPM_V2_FEATURE_FLAT) << "0.50 on a row of gain 0.8: 0.40 of the local median";
}

TEST(BpmV2Test, TheResponseThresholdsAreExactAndTheInequalitiesAreFixed) {
    // noise-free frames of integer pixel values: the offset-corrected response of every pixel is its planted ratio to within the float resolution. Exactly 0.45 and 1.5 are
    // normal (the rule is "below 45 %" and "above 150 %"); 0.4499 and 1.5001 are defects
    Frames f;
    const std::vector<double> nets = {10000.0, 20000.0};
    const double              ratios[6] = {0.4499, 0.45, 0.50, 1.4999, 1.5, 1.5001};
    auto make = [&](double net, bool bright) {
        std::vector<uint16_t> fr(kN, 1000);
        if (bright)
            for (size_t i = 0; i < kN; ++i) fr[i] = static_cast<uint16_t>(1000 + net);
        if (bright)
            for (int q = 0; q < 6; ++q) fr[static_cast<size_t>(30) * kW + static_cast<size_t>(20 + 10 * q)] = static_cast<uint16_t>(1000 + std::floor(net * ratios[q] + 0.5));
        return fr;
    };
    for (int k = 0; k < 7; ++k) f.dark.push_back(make(0.0, false));
    for (double net : nets)
        for (int k = 0; k < 3; ++k) {
            f.bright.push_back(make(net, true));
            f.signal.push_back(net);
        }
    const Result r = detect(f, nullptr);
    ASSERT_EQ(XPE_OK, r.rc);
    const bool expected[6] = {true, false, false, false, false, true};
    for (int q = 0; q < 6; ++q)
        EXPECT_EQ(expected[q], (r.feat[static_cast<size_t>(30) * kW + static_cast<size_t>(20 + 10 * q)] & XPE_BPM_V2_FEATURE_FLAT) != 0) << "response " << ratios[q];
}

TEST(BpmV2Test, ThresholdsFollowFromTheTargetNumberOfFalseAlarms) {
    Frames f = makeFrames(3, kDoses, 3, 7, Plant{false, false});
    XpeBpmConfigV2 one{}, hundred{};
    hundred.target_false_alarms = 100.0f;
    const Result a = detect(f, &one), b = detect(f, &hundred);
    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);
    ASSERT_EQ(static_cast<uint32_t>(kN), a.rep.pixels);
    EXPECT_FLOAT_EQ(0.45f, a.rep.threshold[4]) << "the response rule: 45 % of the local median, whatever the false-alarm target";
    EXPECT_FLOAT_EQ(0.45f, a.rep.response_ratio_low);
    EXPECT_FLOAT_EQ(1.5f, a.rep.response_ratio_high);
    for (int k : {0}) {   // the z feature (the dark mean): erfc(lambda / sqrt 2) * N is the target
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
    EXPECT_EQ(0u, r.rep.feature_active[2]) << "the slope feature is retired as a pixel feature (it stays a line feature)";
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
