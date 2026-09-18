// #180 (QA-B-123): the 512 x 512 MC phantom set.
//
// The 80 x 80 set (test_virtual_grid_mc.cpp) has 4 mm pixels, so a six-level
// pyramid's 128-pixel reach covers the whole image and no mask-outside
// comparison is possible there (QA-B-108/110). This set is 512 x 512 at
// 0.140 mm with a 358-pixel field whose boundary lies inside the image, which
// is what the checklist asked for:
//   .moai/reports/lane-post/PHANTOM-SWAP-CHECKLIST.md
//
// Basis, unchanged from QA-B-116: an IDEAL grid row (ratio 100, tp 1, ts 0),
// so the answer is the phantom's own `primary` image. Comparing against the
// product table would put our own Ts/Tp into the answer and make the
// measurement self-referential.
//
// The 80 x 80 file keeps its own tests; this file is the 512 measurement and
// carries the REQ-GSVG-018 floor, which is bound to the phantom rather than to
// the code (QA-B-117).

#include <gtest/gtest.h>

#include "virtual_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace vg = xpe_gsvg_detail;

namespace {

// The tables live with the 80 x 80 copies; the 512 images are read from the
// producing directory rather than copied, because four 1 MB images per phantom
// would put 8 MB of duplicated binaries into the repository.
constexpr const char* kDir = "tests/data/mc/";
constexpr const char* kPhantomDir = "../../tools/mcsim/phantoms/";
constexpr int    kN = 512;
constexpr double kPitchMm = 0.14;                 // json pixel_pitch_mm
constexpr double kKvp = 80.0;
constexpr double kDnScale = 160989.63650775206;   // json dn_scale
constexpr double kAirDn = 50000.0;                // json dn_air

// Field measured from the air image at 50 % of its maximum: rows/cols 77..434.
// The metric region is inset 32 px (4.5 mm) from that boundary.
constexpr int kFieldLo = 77, kFieldHi = 435;      // [lo, hi)
constexpr int kLo = 109, kHi = 403;               // [lo, hi)

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<double> ReadF32(const std::string& path) {
    const std::string raw = ReadText(path);
    EXPECT_EQ(raw.size(), static_cast<size_t>(kN) * kN * 4) << path;
    std::vector<double> out(static_cast<size_t>(kN) * kN, 0.0);
    for (size_t i = 0; i < out.size() && (i + 1) * 4 <= raw.size(); ++i) {
        float v = 0;
        std::memcpy(&v, raw.data() + i * 4, 4);
        out[i] = v;
    }
    return out;
}

vg::ParamTable McTable() {
    const std::string text =
        "[kernels]\n" + ReadText(std::string(kDir) + "scatter_kernels_water_csi600.csv") +
        "\n[wet]\n" + ReadText(std::string(kDir) + "wet_water_csi600.csv") +
        "\n[grid]\nratio,tp,ts\n# ideal grid: removes all scatter, so `primary` is the answer\n100,1,0\n";
    vg::ParamTable t;
    const std::string err = vg::ParseParamTable(text, t);
    EXPECT_EQ(err, "");
    return t;
}

struct Phantom {
    std::vector<double> total, primary, thickness, air;   // DN (thickness in cm)
    std::vector<uint8_t> mask;                            // 1 = inside the field
};

Phantom Load(const char* name) {
    const std::string base = std::string(kPhantomDir) + name + "_80kVp_512_";
    Phantom p;
    p.total = ReadF32(base + "total.f32");
    p.primary = ReadF32(base + "primary.f32");
    p.thickness = ReadF32(base + "thickness.f32");
    p.air = ReadF32(base + "air.f32");
    double airMax = 0;
    for (double v : p.air) airMax = std::max(airMax, v);
    p.mask.assign(p.air.size(), 0);
    for (size_t i = 0; i < p.air.size(); ++i) p.mask[i] = p.air[i] > 0.5 * airMax ? 1 : 0;
    for (double& v : p.total) v *= kDnScale;
    for (double& v : p.primary) v *= kDnScale;
    for (double& v : p.air) v *= kDnScale;
    return p;
}

bool InRegion(size_t i) {
    const int r = static_cast<int>(i) / kN, c = static_cast<int>(i) % kN;
    return r >= kLo && r < kHi && c >= kLo && c < kHi;
}

vg::VgSettings BaseSettings() {
    vg::VgSettings st;
    st.kvp = kKvp;
    st.gridRatio = 100;
    st.pixelPitchMm = kPitchMm;
    st.airSignal = kAirDn;
    st.iterations = 5;
    return st;
}

struct Metrics {
    double median = 0, hi = 0, absMedian = 0;
};

Metrics Measure(const std::vector<double>& img, const Phantom& p) {
    std::vector<double> r, a;
    for (size_t i = 0; i < img.size(); ++i) {
        if (!InRegion(i) || p.primary[i] <= 0) continue;
        const double q = img[i] / p.primary[i];
        r.push_back(q);
        a.push_back(std::fabs(q - 1.0));
    }
    std::sort(r.begin(), r.end());
    std::sort(a.begin(), a.end());
    Metrics m;
    m.median = r[r.size() / 2];
    m.hi = r.back();
    m.absMedian = a[a.size() / 2];
    return m;
}

// The chain on the collimated image, with the field mask passed in.
Metrics RunChain(const Phantom& p, std::vector<double>* outImg = nullptr,
                 const vg::VgSettings* settings = nullptr,
                 const vg::VgSwitches* switches = nullptr) {
    const vg::ParamTable t = McTable();
    std::vector<double> img = p.total;
    const vg::VgSettings st = settings ? *settings : BaseSettings();
    const vg::VgSwitches sw = switches ? *switches : vg::VgSwitches{};
    const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, t, st, sw, p.mask.data());
    EXPECT_EQ(rep.error, "");
    const Metrics m = Measure(img, p);
    if (outImg) *outImg = img;
    return m;
}

}  // namespace

// ===========================================================================
// (a) Receipt check, run here rather than taken on trust from the producing
// lane: the checklist's six items, read by this code from the files.
// ===========================================================================
TEST(GsvgVirtualGridMc512, DataAndConditions)
{
    const Phantom s = Load("step");

    // Field: boundary inside the image on all four sides, and wide enough that a
    // six-level pyramid's 128-pixel reach does not span it.
    int lo = kN, hi = -1;
    for (int c = 0; c < kN; ++c)
        for (int r = 0; r < kN; ++r)
            if (s.mask[static_cast<size_t>(r) * kN + c]) { lo = std::min(lo, c); hi = std::max(hi, c); }
    std::printf("VGMC512 field cols %d..%d (%d px) pitch=%.3f mm\n", lo, hi, hi - lo + 1, kPitchMm);
    EXPECT_EQ(lo, kFieldLo);
    EXPECT_EQ(hi, kFieldHi - 1);
    EXPECT_GT(hi - lo + 1, 320) << "field narrower than the pyramid reach demands";
    EXPECT_GT(lo, 0);
    EXPECT_LT(hi, kN - 1) << "the field boundary must lie inside the image";

    // Outside the field `total` is low scatter, not zero -- the fact the
    // edge-replicate default was chosen for (#189, QA-B-110).
    size_t zeros = 0;
    double outSum = 0, inSum = 0;
    size_t nOut = 0, nIn = 0;
    for (size_t i = 0; i < s.total.size(); ++i) {
        if (s.total[i] == 0.0) ++zeros;
        if (s.mask[i]) { inSum += s.total[i]; ++nIn; } else { outSum += s.total[i]; ++nOut; }
    }
    std::printf("VGMC512 total zeros=%zu outside mean=%.1f inside mean=%.1f DN\n",
                zeros, outSum / static_cast<double>(nOut), inSum / static_cast<double>(nIn));
    EXPECT_EQ(zeros, 0u) << "a zero pixel outside the field would hide the mask question";

    // Thickness: what is actually reachable inside the field.
    double tMin = 1e9, tMax = 0;
    for (size_t i = 0; i < s.thickness.size(); ++i)
        if (s.mask[i]) { tMin = std::min(tMin, s.thickness[i]); tMax = std::max(tMax, s.thickness[i]); }
    std::printf("VGMC512 in-field thickness %.2f..%.2f cm\n", tMin, tMax);
    EXPECT_LT(tMin, 11.0);
    // 30 cm is NOT reachable in the field on this set either (the 80 x 80 set
    // had the same gap). REQ-GSVG-017's 30 cm point therefore still has no MC
    // basis -- reported, not asserted away.
    EXPECT_LT(tMax, 28.0);

    // Air centre matches the DN scaling the json declares.
    const double centre = s.air[static_cast<size_t>(kN / 2) * kN + kN / 2];
    std::printf("VGMC512 air centre=%.0f DN (json dn_air=%.0f)\n", centre, kAirDn);
    EXPECT_NEAR(centre, kAirDn, 0.05 * kAirDn);
}

// ===========================================================================
// (c) REQ-GSVG-018 provisional floor, re-measured on this phantom.
//
// NOT a clinical pass mark -- it only says "no worse than today", and the value
// is bound to THIS phantom's noise realisation (QA-B-117). The peak is a
// maximum statistic, so noise pushes it one way only.
// ===========================================================================
TEST(GsvgVirtualGridMc512, ProvisionalFloor_StepEdgeUnderSubtraction_REQ_GSVG_018)
{
    const Phantom p = Load("step");
    std::vector<double> img;
    const Metrics m = RunChain(p, &img);

    double peak = 0;
    for (int c = kLo; c < kHi; ++c) {
        double sum = 0;
        int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + static_cast<size_t>(c);
            if (p.primary[i] <= 0) continue;
            sum += img[i] / p.primary[i];
            ++n;
        }
        if (n) peak = std::max(peak, sum / n);
    }
    std::printf("VGMC512 floor018 step peak=%.4f max=%.4f |r-1| median=%.4f\n",
                peak, m.hi, m.absMedian);

    // Measured on this phantom (QA-B-123): 1.0504 / 1.1928 / 0.0342. The floors
    // are those values plus 5 %, the same rule QA-B-95 used on the 80 x 80 set.
    // Not widened further: the noise test below moves the peak by 0.0001 at
    // 0.3 % relative input noise, which is 0.2 % of this margin -- the 80 x 80
    // set's 58..83 % margin consumption does NOT carry over, because the peak is
    // a column mean over 294 rows here against 64 there.
    EXPECT_LT(peak, 1.103) << "column-mean peak after a step";   // 1.0504 x 1.05
    EXPECT_LT(m.hi, 1.253) << "worst pixel";                     // 1.1928 x 1.05
    EXPECT_LT(m.absMedian, 0.036) << "median |ratio - 1|";       // 0.0342 x 1.05
}

// The floor above is a maximum statistic, so noise pushes it one way only.
// QA-B-117 measured that on the 80 x 80 set 0.1 % of relative input noise ate
// 58..83 % of the margin and 0.3 % crossed it. The same measurement is repeated
// here, because a new phantom is a new noise realisation and the old answer
// does not carry over.
TEST(GsvgVirtualGridMc512, NoiseSensitivityOfTheFloor)
{
    const Phantom p = Load("step");

    for (const double rel : {0.0, 0.001, 0.003}) {
        Phantom q = p;
        // Deterministic pseudo-noise: a fixed LCG, so the number is reproducible
        // and the comparison is against the same realisation every run.
        uint32_t seed = 123456789u;
        for (size_t i = 0; i < q.total.size(); ++i) {
            seed = seed * 1664525u + 1013904223u;
            const double u = static_cast<double>(seed >> 8) / 16777216.0 - 0.5;   // [-0.5, 0.5)
            q.total[i] *= 1.0 + 2.0 * rel * u;
        }
        std::vector<double> img;
        const Metrics m = RunChain(q, &img);

        double peak = 0;
        for (int c = kLo; c < kHi; ++c) {
            double sum = 0;
            int n = 0;
            for (int r = kLo; r < kHi; ++r) {
                const size_t i = static_cast<size_t>(r) * kN + static_cast<size_t>(c);
                if (q.primary[i] <= 0) continue;
                sum += img[i] / q.primary[i];
                ++n;
            }
            if (n) peak = std::max(peak, sum / n);
        }
        std::printf("VGMC512 noise rel=%.3f%% peak=%.4f max=%.4f |r-1| median=%.4f\n",
                    rel * 100.0, peak, m.hi, m.absMedian);
    }
    SUCCEED();
}

// ===========================================================================
// (b) REQ-GSVG-017 by thickness, on the MC basis. Values only -- the pass mark
// is the lead's to set. The synthetic measurement (QA-B-115) is printed next to
// each row by the report, not here: this test must not compare against a number
// derived from our own table.
// ===========================================================================
TEST(GsvgVirtualGridMc512, ThicknessAccuracy_REQ_GSVG_017)
{
    const Phantom p = Load("wedge");
    std::vector<double> img;
    RunChain(p, &img);

    std::printf("VGMC512 REQ-017 (wedge, ideal-grid basis): thickness, n, median r, median |r-1|, p95 |r-1|\n");
    for (const int t : {10, 15, 20, 25, 30}) {
        std::vector<double> a, r;
        for (size_t i = 0; i < img.size(); ++i) {
            if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) continue;
            if (std::fabs(p.thickness[i] - t) >= 0.5) continue;
            const double q = img[i] / p.primary[i];
            r.push_back(q);
            a.push_back(std::fabs(q - 1.0));
        }
        if (a.empty()) {
            std::printf("VGMC512 REQ-017 t=%2d cm: NOT PRESENT in the field on this phantom\n", t);
            continue;
        }
        std::sort(a.begin(), a.end());
        std::sort(r.begin(), r.end());
        std::printf("VGMC512 REQ-017 t=%2d cm: n=%zu median r=%.4f median|r-1|=%.4f p95|r-1|=%.4f\n",
                    t, a.size(), r[r.size() / 2], a[a.size() / 2], a[static_cast<size_t>(0.95 * a.size())]);
    }
    SUCCEED();
}

// ===========================================================================
// (d) #189 mask-outside, WITH the post-steps on -- the comparison the 80 x 80
// set could not carry. The artificial step at the field boundary is measured
// the same way as the synthetic test: a ring one pixel inside the boundary
// against the deep interior.
// ===========================================================================
TEST(GsvgVirtualGridMc512, MaskOutsideVariantsWithPostStepsOn)
{
    const Phantom p = Load("step");

    struct Variant { const char* name; vg::MaskOutside mode; };
    const Variant variants[] = {
        {"Zero",      vg::MaskOutside::Zero},
        {"Replicate", vg::MaskOutside::Replicate},
        {"Keep",      vg::MaskOutside::Keep},
        {"RectOnly",  vg::MaskOutside::RectOnly},
    };

    for (const int levels : {4, 6}) {
        for (const Variant& v : variants) {
            vg::VgSettings st = BaseSettings();
            st.pyramidLevels = levels;
            vg::VgSwitches sw;
            sw.maskOutside = v.mode;

            std::vector<double> img;
            const Metrics m = RunChain(p, &img, &st, &sw);

            // The step phantom's own thickness gradient spans a factor of ~6 in
            // signal, so a raw ring-vs-interior mean measures the phantom, not
            // the boundary. Normalising by `primary` removes the gradient and
            // leaves the artificial step the mask choice introduces.
            double ring = 0, deep = 0;
            size_t nRing = 0, nDeep = 0;
            for (int y = kFieldLo; y < kFieldHi; ++y)
                for (int x = kFieldLo; x < kFieldHi; ++x) {
                    const size_t i = static_cast<size_t>(y) * kN + x;
                    if (p.primary[i] <= 0) continue;
                    const double q = img[i] / p.primary[i];
                    const int d = std::min(std::min(x - kFieldLo, kFieldHi - 1 - x),
                                           std::min(y - kFieldLo, kFieldHi - 1 - y));
                    if (d == 1) { ring += q; ++nRing; }
                    else if (d >= 140) { deep += q; ++nDeep; }
                }
            ring /= static_cast<double>(nRing);
            deep /= static_cast<double>(nDeep);
            std::printf("VGMC512 mask levels=%d %-9s step=%.4f  median r=%.4f |r-1| median=%.4f\n",
                        levels, v.name, (ring - deep) / deep, m.median, m.absMedian);
        }
    }
    SUCCEED();
}
