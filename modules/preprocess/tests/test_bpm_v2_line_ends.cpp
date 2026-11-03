/**
 * @file test_bpm_v2_line_ends.cpp
 * @brief QA-A-314 (#273): the ends of a strong line stretch are not carried onto the slightly weak pixels next to it.
 *
 * Reproduction of the line-end overflow of the B16 measurement (QA-A-313: 8 of the 48 new false alarms lie 3 to 5 pixels beyond the true end of a planted line).
 * The end of a stretch is the change point of the maximum-sum run of (z - m/2) over the cut z terms. The terms were cut at +-3, so the level m of a strong line
 * (tens of noise widths) was taken as about 3 and the change point sat at 1.5 noise widths: two or three ordinary pixels of the panel beyond the end, a little weak
 * (2 to 2.5 widths), were joined to the line. Here the panel has a 1 % gain pattern and the three pixels after the end of a row stretch of response 0.30 have a
 * response of 0.977 (about 2.3 widths): normal pixels. The stretch must end where the planted line ends.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t kW = 256, kH = 256;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr int      kRow = 128, kFrom = 64, kTo = 192;   // the planted stretch [kFrom, kTo) of row kRow
constexpr int      kWeak = 3;                            // pixels after the end with a response of kWeakResponse
constexpr double   kWeakResponse = 0.977;

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

struct Found {
    XpeErrorCode            rc;
    std::vector<XpeBpmLine> lines;
    std::vector<uint8_t>    mask;
};

// Seven dark frames (the first is skipped) and four dose groups of three bright frames. The gain pattern has a 1 % spread; the weak pixels and the line have no pattern.
Found run(uint64_t seed, bool weakPixels) {
    Normal rng(seed);
    std::vector<double> G(kN);
    for (size_t i = 0; i < kN; ++i) G[i] = 1.0 + 0.01 * rng.next();
    for (int x = kFrom; x < kTo; ++x) G[static_cast<size_t>(kRow) * kW + static_cast<size_t>(x)] = 0.30;
    for (int k = 0; k < kWeak; ++k) G[static_cast<size_t>(kRow) * kW + static_cast<size_t>(kTo + k)] = weakPixels ? kWeakResponse : 1.0;
    Normal noise(seed + 11);
    auto frame = [&](double net) {
        std::vector<uint16_t> f(kN);
        for (size_t i = 0; i < kN; ++i) f[i] = static_cast<uint16_t>(std::lround(1000.0 + net * G[i] + 3.0 * noise.next()));
        return f;
    };
    std::vector<std::vector<uint16_t>> dark, bright;
    for (int k = 0; k < 7; ++k) dark.push_back(frame(0.0));
    for (double d : {2000.0, 4500.0, 9000.0, 18000.0})
        for (int k = 0; k < 3; ++k) bright.push_back(frame(d));
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : dark) db.push_back(buf(v));
    for (auto& v : bright) bb.push_back(buf(v));
    Found r{};
    r.mask.assign(kN, 0);
    XpeImageBuffer mb{};
    mb.data = r.mask.data();
    mb.width = kW;
    mb.height = kH;
    mb.bitsAllocated = 8;
    mb.bitsStored = 8;
    mb.format = XPE_PIXEL_UINT8;
    mb.dataSize = r.mask.size();
    r.lines.assign(64, XpeBpmLine{});
    XpeBpmLineReport rep{};
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    r.rc = xpe_bpm_find_lines(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), nullptr, nullptr, &mb, r.lines.data(), 64, nullptr, 0, &rep);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    r.lines.resize(std::min<size_t>(rep.lines, 64));
    return r;
}

const XpeBpmLine* rowLine(const Found& r) {
    for (const XpeBpmLine& l : r.lines)
        if (l.orientation == XPE_BPM_LINE_ROW && l.index <= static_cast<uint32_t>(kRow) && static_cast<uint32_t>(kRow) < l.index + l.width) return &l;
    return nullptr;
}

int paintedEnd(const Found& r) {   // one past the last painted pixel of row kRow at or after kFrom
    int x = kFrom;
    while (x < static_cast<int>(kW) && r.mask[static_cast<size_t>(kRow) * kW + static_cast<size_t>(x)] != 0) ++x;
    return x;
}

}  // namespace

TEST(BpmV2LineEndsTest, AStrongLineStretchEndsWhereTheLineEndsAndLeavesTheSlightlyWeakNeighboursOut) {
    const Found r = run(314, true);
    ASSERT_EQ(XPE_OK, r.rc);
    const XpeBpmLine* l = rowLine(r);
    ASSERT_NE(nullptr, l) << "the planted row stretch of response 0.30 is found";
    EXPECT_EQ(static_cast<uint32_t>(kFrom), l->start);
    EXPECT_EQ(static_cast<uint32_t>(kTo), l->end) << "the three pixels of response 0.977 after the end are normal pixels, not part of the line";
    EXPECT_EQ(kTo, paintedEnd(r)) << "the mask paints the line and nothing after it";
}

TEST(BpmV2LineEndsTest, WithoutTheWeakNeighboursTheSameLineEndsInTheSamePlace) {
    // the control of the test above: the same frames with the three pixels at the nominal response
    const Found r = run(314, false);
    ASSERT_EQ(XPE_OK, r.rc);
    const XpeBpmLine* l = rowLine(r);
    ASSERT_NE(nullptr, l);
    EXPECT_EQ(static_cast<uint32_t>(kFrom), l->start);
    EXPECT_EQ(static_cast<uint32_t>(kTo), l->end);
    EXPECT_EQ(kTo, paintedEnd(r));
}
