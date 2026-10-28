/**
 * @file test_bpm_v2_periodic_dark.cpp
 * @brief QA-A-319 (#273 #278): a periodic dark structure of the readout (a row band x column parity interaction) is not flagged as defects by the dark-mean feature.
 *
 * Reproduction of QA-A-318: on the FT tool set (20250501_12) the dark mean of the detector is lower by about 20 on the odd columns of the rows with row mod 32 = 24..31,
 * and the dark-mean feature of xpe_bpm_generate_v2 flagged 98.6 % of those pixels (QA-A-319 evidence/11_stage_dump_out.txt). The stage dump showed why:
 *   - the row component removed before the local background is one number per row, so a level that differs between the even and odd columns of a row stays;
 *   - the column component is the median over all rows, and the band is only a quarter of the rows, so it stays as well;
 *   - the separable median (17 x 17) follows the column parity along x, but along y the band is 8 rows of the 17 of the window, so the background of a band pixel
 *     comes from the rows outside the band, and the odd band pixels are left with a residual of about -13 (z about -9).
 * The synthetic frames carry the same interaction (8 rows of 32, odd columns, -20). Controls with the same amplitude: a pure row component (the same rows, every column)
 * and a pure column component (every odd column, every row), which the row and column removal takes out. Four planted dark defects must be flagged in every variant.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

// 260 rows: the last band (rows 248..255) is followed by 4 ordinary rows before the bottom edge, as on the FT tool frame cut to its imaging area (band rows
// 2456..2463, then rows 2464..2467, then the cut). A first fix (one row component per column parity, one pass) removed the band in the interior but shifted
// these 4 rows: the reflected 17-row window there is mostly band rows (QA-A-319 evidence/34_stage_dump_fix1_vc_crop_rows2452_2465.txt). These synthetic frames
// do NOT reproduce that shift (they pass under the first fix too, evidence/22_test_h260_under_fix1_parity_only.txt); its guard is the FT tool measurement.
constexpr uint32_t kW = 256, kH = 260;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr double   kAmp = -20.0;   // the level of the structure (QA-A-318: odd 2476.66 against even 2497.25 in the band rows)

enum class Pattern { None, Interaction, RowOnly, ColumnOnly };

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

bool inBand(uint32_t y) { return (y % 32u) >= 24u; }

double structure(Pattern p, uint32_t x, uint32_t y) {
    switch (p) {
        case Pattern::Interaction: return (inBand(y) && (x % 2u) == 1u) ? kAmp : 0.0;
        case Pattern::RowOnly: return inBand(y) ? kAmp : 0.0;
        case Pattern::ColumnOnly: return (x % 2u) == 1u ? kAmp : 0.0;
        case Pattern::None: break;
    }
    return 0.0;
}

// Four planted dark defects (+60 in every dark and bright frame), two of them in band rows on odd columns.
const uint32_t kPlant[4][2] = {{41, 27}, {100, 60}, {157, 155}, {200, 222}};
bool planted(uint32_t x, uint32_t y) {
    for (const auto& p : kPlant)
        if (p[0] == x && p[1] == y) return true;
    return false;
}

XpeImageBuffer buf16(std::vector<uint16_t>& v) {
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

XpeImageBuffer buf8(std::vector<uint8_t>& v) {
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

struct Result {
    XpeErrorCode         rc;
    std::vector<uint8_t> feat;
    XpeBpmReportV2       rep;
    uint32_t             darkMeanOutsidePlants;   // pixels with the dark-mean bit that are not planted
    uint32_t             plantsFound;             // planted pixels with the dark-mean bit
};

// Seven dark frames (the first is skipped) and four dose groups of three bright frames, gain 1, the structure in the dark level of every frame.
Result run(Pattern p, uint64_t seed) {
    Normal noise(seed);
    auto frame = [&](double net) {
        std::vector<uint16_t> f(kN);
        for (uint32_t y = 0; y < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x) {
                const double d = 2500.0 + structure(p, x, y) + (planted(x, y) ? 60.0 : 0.0);
                const double v = d + net + (net > 0.0 ? 0.5 * std::sqrt(net) * noise.next() : 0.0) + 3.0 * noise.next();
                f[static_cast<size_t>(y) * kW + x] = static_cast<uint16_t>(std::lround(std::min(65535.0, std::max(0.0, v))));
            }
        return f;
    };
    std::vector<std::vector<uint16_t>> dark, bright;
    for (int k = 0; k < 7; ++k) dark.push_back(frame(0.0));
    for (double d : {2000.0, 4500.0, 9000.0, 18000.0})
        for (int k = 0; k < 3; ++k) bright.push_back(frame(d));
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : dark) db.push_back(buf16(v));
    for (auto& v : bright) bb.push_back(buf16(v));
    Result r{};
    std::vector<uint8_t> map(kN, 0);
    r.feat.assign(kN, 0);
    XpeImageBuffer mb = buf8(map), fb = buf8(r.feat);
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    r.rc = xpe_bpm_generate_v2(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), nullptr, &mb, &fb, nullptr, &r.rep);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    for (uint32_t y = 0; y < kH; ++y)
        for (uint32_t x = 0; x < kW; ++x) {
            const bool dm = (r.feat[static_cast<size_t>(y) * kW + x] & XPE_BPM_V2_FEATURE_DARK_MEAN) != 0;
            if (!dm) continue;
            if (planted(x, y)) ++r.plantsFound;
            else ++r.darkMeanOutsidePlants;
        }
    return r;
}

std::string describe(const Result& r) {
    return "dark_mean flagged " + std::to_string(r.rep.flagged[0]) + " (outside the plants " + std::to_string(r.darkMeanOutsidePlants) + "), threshold " +
           std::to_string(r.rep.threshold[0]) + ", scale " + std::to_string(r.rep.scale_median[0]) + ", union " + std::to_string(r.rep.flagged_union);
}

// The target is one expected false alarm per feature over the frame; a handful leaves room for the noise of one seed without hiding a structure of 8192 pixels.
constexpr uint32_t kFalseAlarmRoom = 5;

}  // namespace

TEST(BpmV2PeriodicDarkTest, ARowBandTimesColumnParityDarkStructureIsNotFlaggedAsDefects) {
    const Result r = run(Pattern::Interaction, 319);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_LE(r.darkMeanOutsidePlants, kFalseAlarmRoom) << describe(r) << " -- the odd columns of the band rows are a readout structure of the whole frame, not defects";
    EXPECT_EQ(4u, r.plantsFound) << describe(r);
}

TEST(BpmV2PeriodicDarkTest, ControlAPureRowComponentOfTheSameAmplitudeIsNotFlagged) {
    const Result r = run(Pattern::RowOnly, 319);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_LE(r.darkMeanOutsidePlants, kFalseAlarmRoom) << describe(r);
    EXPECT_EQ(4u, r.plantsFound) << describe(r);
}

TEST(BpmV2PeriodicDarkTest, ControlAPureColumnComponentOfTheSameAmplitudeIsNotFlagged) {
    const Result r = run(Pattern::ColumnOnly, 319);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_LE(r.darkMeanOutsidePlants, kFalseAlarmRoom) << describe(r);
    EXPECT_EQ(4u, r.plantsFound) << describe(r);
}

TEST(BpmV2PeriodicDarkTest, ControlNoStructure) {
    const Result r = run(Pattern::None, 319);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_LE(r.darkMeanOutsidePlants, kFalseAlarmRoom) << describe(r);
    EXPECT_EQ(4u, r.plantsFound) << describe(r);
}
