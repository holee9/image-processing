// #180 (QA-B-123): the 512 x 512 MC phantom set.
//
// The 80 x 80 set (test_virtual_grid_mc.cpp) has 4 mm pixels, so the pyramid's
// reach covers the whole image and no mask-outside comparison is possible there
// (QA-B-108/110). QA-B-137 CORRECTION: this header used to say "a six-level
// pyramid's 128-pixel reach". Both numbers were wrong and they did not even
// agree with each other -- the default is FOUR levels (virtual_grid.h:115) and
// PyramidContrast reduces `levels` times (virtual_grid.cpp:603), so the reach is
// 2^levels = 16 px, not 128. The wrong premise was copied out of this comment
// into the QA-B-136 report and #191 before anyone read the code. This set is
// 512 x 512 at
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

// ===========================================================================
// QA-B-124: why the 018 numbers improved. The floor moved 1.4521 -> 1.0504
// between the 80 x 80 set and this one, and "the code got better" would be the
// wrong reading: the SAME code runs on both, so the difference has to come from
// the input. The candidate was pixel size (4 mm against 0.140 mm).
//
// The controlled form: bin THIS phantom down by 2/4/8, which changes the pixel
// size and nothing else -- same scene, same statistics, same geometry. If the
// peak climbs with the pitch, the pixel-size reading holds; if it stays put,
// the difference is something else about the old phantom and the improvement
// does not travel with the code.
// ===========================================================================
TEST(GsvgVirtualGridMc512, WhyTheFloorImproved_PixelSize)
{
    const Phantom full = Load("step");

    std::printf("VGMC512 bin: factor, pitch_mm, n, peak, worst, |r-1| median\n");
    for (const int f : {1, 2, 4, 8}) {
        const int n = kN / f;
        Phantom b;
        b.total.assign(static_cast<size_t>(n) * n, 0.0);
        b.primary = b.total;
        b.thickness = b.total;
        b.air = b.total;
        b.mask.assign(b.total.size(), 0);

        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                double t = 0, p = 0, th = 0, a = 0;
                for (int dy = 0; dy < f; ++dy)
                    for (int dx = 0; dx < f; ++dx) {
                        const size_t s = static_cast<size_t>(y * f + dy) * kN + (x * f + dx);
                        t += full.total[s]; p += full.primary[s];
                        th += full.thickness[s]; a += full.air[s];
                    }
                const size_t d = static_cast<size_t>(y) * n + x;
                const double inv = 1.0 / (f * f);
                // Signals are per-pixel means: binning a detector adds the
                // quanta but the DN scale is per pixel, so the mean keeps the
                // chain's airSignal meaningful.
                b.total[d] = t * inv; b.primary[d] = p * inv;
                b.thickness[d] = th * inv; b.air[d] = a * inv;
            }
        double airMax = 0;
        for (double v : b.air) airMax = std::max(airMax, v);
        for (size_t i = 0; i < b.air.size(); ++i) b.mask[i] = b.air[i] > 0.5 * airMax ? 1 : 0;

        vg::VgSettings st = BaseSettings();
        st.pixelPitchMm = kPitchMm * f;

        const vg::ParamTable tbl = McTable();
        std::vector<double> img = b.total;
        const vg::VgReport rep = vg::RunVirtualGrid(img, n, n, tbl, st, vg::VgSwitches{}, b.mask.data());
        ASSERT_EQ(rep.error, "") << "factor " << f;

        // Same metric region, scaled: inset 32/f px from the field boundary.
        const int lo = kLo / f, hi = kHi / f;
        std::vector<double> a;
        double peak = 0, worst = 0;
        for (int c = lo; c < hi; ++c) {
            double sum = 0;
            int cnt = 0;
            for (int r = lo; r < hi; ++r) {
                const size_t i = static_cast<size_t>(r) * n + c;
                if (b.primary[i] <= 0) continue;
                const double q = img[i] / b.primary[i];
                sum += q; ++cnt;
                worst = std::max(worst, q);
                a.push_back(std::fabs(q - 1.0));
            }
            if (cnt) peak = std::max(peak, sum / cnt);
        }
        std::sort(a.begin(), a.end());
        std::printf("VGMC512 bin f=%d pitch=%.3f mm n=%d peak=%.4f worst=%.4f |r-1| median=%.4f\n",
                    f, st.pixelPitchMm, n, peak, worst, a[a.size() / 2]);
    }
    SUCCEED();
}

// QA-B-124: do the two phantoms agree where their thicknesses overlap?
// The step set carries 8 / 10.5 / 13.5 / 16 / 19 / 21.5 / 24.5 cm; the wedge is
// continuous over 6..26. A disagreement at the same thickness would mean the
// measurement depends on the scene rather than on the thickness, which is what
// REQ-GSVG-017 is about.
TEST(GsvgVirtualGridMc512, StepAndWedgeAgreeAtTheSameThickness)
{
    struct Row { double t; double step; double wedge; size_t nStep, nWedge; };
    std::vector<Row> rows;

    std::vector<double> imgStep, imgWedge;
    const Phantom s = Load("step");
    const Phantom w = Load("wedge");
    RunChain(s, &imgStep);
    RunChain(w, &imgWedge);

    auto medianAt = [](const std::vector<double>& img, const Phantom& p, double t, size_t& n) {
        std::vector<double> a;
        for (size_t i = 0; i < img.size(); ++i) {
            if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) continue;
            if (std::fabs(p.thickness[i] - t) >= 0.25) continue;
            a.push_back(std::fabs(img[i] / p.primary[i] - 1.0));
        }
        n = a.size();
        if (a.empty()) return -1.0;
        std::sort(a.begin(), a.end());
        return a[a.size() / 2];
    };

    std::printf("VGMC512 step-vs-wedge: thickness, step median|r-1| (n), wedge median|r-1| (n), ratio\n");
    for (const double t : {8.0, 10.5, 13.5, 16.0, 19.0, 21.5, 24.5}) {
        size_t ns = 0, nw = 0;
        const double a = medianAt(imgStep, s, t, ns);
        const double b = medianAt(imgWedge, w, t, nw);
        if (a < 0 || b < 0) {
            std::printf("VGMC512 step-vs-wedge t=%.1f cm: missing (step n=%zu, wedge n=%zu)\n", t, ns, nw);
            continue;
        }
        std::printf("VGMC512 step-vs-wedge t=%.1f cm: step=%.4f (n=%zu) wedge=%.4f (n=%zu) ratio=%.2f\n",
                    t, a, ns, b, nw, a / b);
        rows.push_back({t, a, b, ns, nw});
    }
    EXPECT_FALSE(rows.empty()) << "no overlapping thickness was measurable";
    SUCCEED();
}

// ===========================================================================
// QA-B-125: is the step phantom worse because of its BOUNDARIES?
//
// QA-B-124 measured the step at 1.2..3.0x the wedge error at the same
// thickness and read it as "the boundaries dominate". That was an explanation,
// not a measurement. Here the error is split by distance to the nearest
// thickness jump, and the median is re-taken with a ring of +-N pixels around
// every jump removed.
//
// Choosing N: the gauss4 kernel terms at 80 kVp have sigma 0.92..12.03 mm over
// 5..30 cm of water (scatter_kernels_water_csi600.csv), which at 0.140 mm is
// 6.6..86 px. A single N cannot cover that range -- the widest term alone would
// eat the whole 358 px field -- so N is swept instead, and the question becomes
// where the step value converges rather than what one exclusion gives.
// ===========================================================================
TEST(GsvgVirtualGridMc512, StepErrorAgainstDistanceToTheBoundary)
{
    const Phantom s = Load("step");
    const Phantom w = Load("wedge");
    std::vector<double> imgStep, imgWedge;
    RunChain(s, &imgStep);
    RunChain(w, &imgWedge);

    // Thickness runs along columns, so a jump is a column-to-column change.
    std::vector<double> colT(kN, 0.0);
    for (int c = 0; c < kN; ++c) {
        double sum = 0;
        int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + c;
            if (!s.mask[i]) continue;
            sum += s.thickness[i]; ++n;
        }
        colT[c] = n ? sum / n : 0.0;
    }
    std::vector<int> isJump(kN, 0);
    int jumps = 0;
    for (int c = kLo; c + 1 < kHi; ++c)
        if (std::fabs(colT[c + 1] - colT[c]) > 0.2) { isJump[c] = 1; ++jumps; }

    // Distance of each column to the nearest jump.
    std::vector<int> dist(kN, kN);
    for (int c = kLo; c < kHi; ++c)
        for (int j = kLo; j < kHi; ++j)
            if (isJump[j]) dist[c] = std::min(dist[c], std::abs(c - j));

    auto median = [](std::vector<double> a) {
        if (a.empty()) return -1.0;
        std::sort(a.begin(), a.end());
        return a[a.size() / 2];
    };

    // Wedge reference over the same region: no jumps exist there (the wedge
    // changes by 0.056 cm per column, far below the 0.2 cm jump criterion).
    std::vector<double> wref;
    for (size_t i = 0; i < imgWedge.size(); ++i) {
        if (!InRegion(i) || !w.mask[i] || w.primary[i] <= 0) continue;
        wref.push_back(std::fabs(imgWedge[i] / w.primary[i] - 1.0));
    }
    std::printf("VGMC125 jumps=%d in the metric region; wedge reference median|r-1|=%.4f\n",
                jumps, median(wref));

    // (1) Error against distance to the nearest jump.
    std::printf("VGMC125 profile: distance_px, distance_mm, n, median|r-1|\n");
    const int edges[] = {0, 1, 2, 4, 8, 16, 32, 64, 128};
    for (size_t b = 0; b + 1 < sizeof(edges) / sizeof(edges[0]); ++b) {
        std::vector<double> a;
        for (size_t i = 0; i < imgStep.size(); ++i) {
            if (!InRegion(i) || !s.mask[i] || s.primary[i] <= 0) continue;
            const int c = static_cast<int>(i) % kN;
            if (dist[c] < edges[b] || dist[c] >= edges[b + 1]) continue;
            a.push_back(std::fabs(imgStep[i] / s.primary[i] - 1.0));
        }
        std::printf("VGMC125 profile d=[%3d,%3d) = [%.2f,%.2f) mm n=%6zu median=%.4f\n",
                    edges[b], edges[b + 1], edges[b] * kPitchMm, edges[b + 1] * kPitchMm,
                    a.size(), median(a));
    }

    // (2) The median with a +-N ring around every jump removed.
    std::printf("VGMC125 exclusion: N_px, N_mm, n_left, median|r-1| (wedge reference %.4f)\n",
                median(wref));
    for (const int n : {0, 2, 5, 10, 20, 40, 80}) {
        std::vector<double> a;
        for (size_t i = 0; i < imgStep.size(); ++i) {
            if (!InRegion(i) || !s.mask[i] || s.primary[i] <= 0) continue;
            const int c = static_cast<int>(i) % kN;
            if (dist[c] <= n) continue;
            a.push_back(std::fabs(imgStep[i] / s.primary[i] - 1.0));
        }
        std::printf("VGMC125 exclude N=%2d (%.2f mm) n=%6zu median=%.4f\n",
                    n, n * kPitchMm, a.size(), median(a));
    }
    SUCCEED();
}

// ===========================================================================
// QA-B-125 (b): REQ-GSVG-011 and REQ-GSVG-025 against MC.
//
// REQ-011 asks that the scatter DISTRIBUTION be estimated from the kernel LUT.
// The phantom carries the true distribution (total - primary), so the estimate
// the chain actually subtracts can be compared against it pixel by pixel --
// which no synthetic scene can do, because there the scatter was made with the
// same kernels that remove it.
//
// REQ-025 asks that the correction be clamped at the physical limit. What MC
// can say here is whether the cap BINDS on real scatter, and what it costs when
// it does; the clamp logic itself is covered by the synthetic tests.
//
// Post-steps are off for both: they run after the subtraction and would mix a
// second effect into the comparison.
// ===========================================================================
TEST(GsvgVirtualGridMc512, ScatterEstimateAgainstTruth_REQ_GSVG_011_025)
{
    const Phantom p = Load("step");
    const vg::ParamTable tbl = McTable();

    vg::VgSettings st = BaseSettings();
    st.pyramidLevels = 0;         // isolate the subtraction
    st.pyramidGain = 1.0;
    st.denoiseK = 0.0;

    struct Case { const char* name; vg::CapMode cap; };
    const Case cases[] = {
        {"None",        vg::CapMode::None},
        {"GlobalSum",   vg::CapMode::GlobalSum},   // the default
        {"LocalSum",    vg::CapMode::LocalSum},
    };

    std::printf("VGMC125 REQ-011/025: cap, capped fraction, negativePrimary, median S_est/S_true, p05, p95\n");
    for (const Case& c : cases) {
        vg::VgSwitches sw;
        sw.cap = c.cap;
        std::vector<double> img = p.total;
        const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, tbl, st, sw, p.mask.data());
        ASSERT_EQ(rep.error, "") << c.name;

        std::vector<double> ratio;
        for (size_t i = 0; i < img.size(); ++i) {
            if (!InRegion(i) || !p.mask[i]) continue;
            const double sTrue = p.total[i] - p.primary[i];
            const double sEst = p.total[i] - img[i];
            if (sTrue <= 0) continue;
            ratio.push_back(sEst / sTrue);
        }
        std::sort(ratio.begin(), ratio.end());
        ASSERT_FALSE(ratio.empty());
        std::printf("VGMC125 REQ-011 cap=%-9s capped=%.4f negPrimary=%.0f n=%zu "
                    "median=%.4f p05=%.4f p95=%.4f\n",
                    c.name, rep.cappedFraction, static_cast<double>(rep.negativePrimary), ratio.size(),
                    ratio[ratio.size() / 2],
                    ratio[static_cast<size_t>(0.05 * ratio.size())],
                    ratio[static_cast<size_t>(0.95 * ratio.size())]);
    }
    SUCCEED();
}

// ===========================================================================
// #191 item 2 (QA-B-134): does the error peak sit at the narrowest kernel
// term's width?
//
// FIRST, A CORRECTION TO THE CLAIM BEING TESTED. QA-B-125 read sigma_1 as
// 0.92..1.21 mm and said the peak (0.28..1.12 mm) matched its order. The table
// stores sigma in CENTIMETRES -- the kernel CSV header says "r in cm at the
// detector plane" and virtual_grid.cpp:505 divides by the pitch in cm. So
// sigma_1 = 9.19..12.15 mm, and the peak sits at 0.03..0.09 of it, two orders
// away. The claim was wrong on units before any experiment was run.
//
// The experiment still earns its place, because it separates "wrong by a
// factor" from "unrelated": scaling sigma_1 by k has a falsifiable prediction.
// If the peak is set by the narrowest term it moves roughly with k and
// peak/sigma_1 stays put; if it does not move, the narrowest term does not set
// it -- and that is the more useful answer.
//
// Only the narrowest term is scaled, and only its WIDTH, so the comparison has
// one moving part. Amplitudes are untouched, so the total scatter is not what
// changes (scaling a whole kernel moves the answer and the question with it --
// the design QA-B-134 rejected).
// ===========================================================================
TEST(GsvgVirtualGridMc512, NarrowestTermWidthVsErrorPeakLocation_191)
{
    const Phantom p = Load("step");

    // Column-to-column thickness jumps, as in QA-B-125.
    std::vector<double> colT(kN, 0.0);
    for (int c = 0; c < kN; ++c) {
        double sum = 0;
        int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + c;
            if (!p.mask[i]) continue;
            sum += p.thickness[i];
            ++n;
        }
        colT[c] = n ? sum / n : 0.0;
    }
    std::vector<int> dist(kN, kN);
    for (int c = kLo; c < kHi; ++c)
        for (int j = kLo; j + 1 < kHi; ++j)
            if (std::fabs(colT[j + 1] - colT[j]) > 0.2) dist[c] = std::min(dist[c], std::abs(c - j));

    std::printf("VGMC134 sigma1 sweep: k, sigma1_mm, factor, peak_at_mm, peak_median, peak/sigma1\n");
    for (const double k : {1.0, 1.5, 2.0, 3.0}) {
        vg::ParamTable t = McTable();
        double s1mm = 0;
        for (auto& node : t.kernels) {
            int narrow = 0;
            for (int i = 1; i < node.terms; ++i)
                if (node.s[i] < node.s[narrow]) narrow = i;
            node.s[narrow] *= k;                   // width only, narrowest term only
            if (s1mm == 0) s1mm = node.s[narrow] * 10.0;
        }

        vg::VgSettings st = BaseSettings();
        std::vector<double> img = p.total;
        const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, t, st, vg::VgSwitches{}, p.mask.data());
        ASSERT_EQ(rep.error, "") << k;

        const int edges[] = {0, 1, 2, 4, 8, 16, 32, 64, 128};
        double best = -1, bestMm = -1;
        for (size_t b = 0; b + 1 < sizeof(edges) / sizeof(edges[0]); ++b) {
            std::vector<double> a;
            for (size_t i = 0; i < img.size(); ++i) {
                if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) continue;
                const int c = static_cast<int>(i) % kN;
                if (dist[c] < edges[b] || dist[c] >= edges[b + 1]) continue;
                a.push_back(std::fabs(img[i] / p.primary[i] - 1.0));
            }
            if (a.empty()) continue;
            std::sort(a.begin(), a.end());
            const double m = a[a.size() / 2];
            if (m > best) { best = m; bestMm = 0.5 * (edges[b] + edges[b + 1]) * kPitchMm; }
        }
        std::printf("VGMC134 sigma1 k=%.1f sigma1=%.2f mm factor=%d peak_at=%.2f mm median=%.4f peak/sigma1=%.4f\n",
                    k, s1mm, rep.factor, bestMm, best, bestMm / s1mm);
    }
    SUCCEED();
}

// ===========================================================================
// #191 item 4 (QA-B-134): can this phantom make the case the cap protects
// against -- a negative primary before clamping?
//
// QA-B-125 found the cap BINDS on 6.25 % of pixels but never had to SAVE
// anything: negativePrimary was 0 in every configuration. That is "it fires",
// not "it protects". Here the kernel amplitudes are over-estimated (the
// ScaledKernels idea from test_virtual_grid.cpp:77) until the subtraction would
// go negative WITHOUT the cap, and the cap is then switched on at the same
// factor.
//
// CapMode::None is the control and is not optional: QA-B-112 measured only at
// the default cap and missed a floor defect that the None case exposed.
// ===========================================================================
TEST(GsvgVirtualGridMc512, OverEstimatedKernelsMakeTheCapProtect_191)
{
    const Phantom p = Load("step");

    std::printf("VGMC134 cap: factor, cap, negativePrimary, cappedFraction, median r\n");
    for (const double factor : {1.0, 2.0, 3.0, 5.0, 10.0}) {
        for (const vg::CapMode cap : {vg::CapMode::None, vg::CapMode::GlobalSum}) {
            vg::ParamTable t = McTable();
            for (auto& node : t.kernels)
                for (int i = 0; i < node.terms; ++i) node.a[i] *= factor;

            vg::VgSettings st = BaseSettings();
            st.pyramidLevels = 0;
            st.pyramidGain = 1.0;
            st.denoiseK = 0.0;
            vg::VgSwitches sw;
            sw.cap = cap;

            std::vector<double> img = p.total;
            const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, t, st, sw, p.mask.data());
            ASSERT_EQ(rep.error, "") << factor;
            const Metrics m = Measure(img, p);
            std::printf("VGMC134 cap factor=%5.1f cap=%-9s negativePrimary=%zu capped=%.4f median=%.4f\n",
                        factor, cap == vg::CapMode::None ? "None" : "GlobalSum",
                        rep.negativePrimary, rep.cappedFraction, m.median);
        }
    }
    SUCCEED();
}

// ===========================================================================
// #191 item 1 (QA-B-136): is the error peak bound to MILLIMETRES or to PIXELS?
//
// FIRST, A CORRECTION TO THE CARD. QA-B-136 (a) asks for the existing profile
// to be "re-binned in pixels". It already is: the bin edges in
// StepErrorAgainstDistanceToTheBoundary are pixel counts {0,1,2,4,8,...} and
// the mm column is those counts times the pitch, printed for reading. At one
// pixel size the two axes differ by a constant, so the same table reads both
// ways and (a) cannot separate them. Only (b) can.
//
// (b) changes ONE axis. The phantom is binned k x k, which is physically a
// coarser detector over the SAME scene: the object, its step boundaries and the
// scatter physics are untouched; only the sampling changes. pixelPitchMm moves
// with k, so the kernel taps (sigma is in cm) keep their physical width and
// shrink in pixels.
//
// The prediction is falsifiable either way:
//   peak at the same MILLIMETRES across k -> physical scale; a 2 cm-step
//     phantom is the right instrument and #191 item 1 stands as written.
//   peak at the same PIXEL COUNT across k -> scene/sampling scale; a coarser
//     step phantom would put the peak a few pixels from its boundary again and
//     would not answer the question. #191 item 1 needs rewriting, not running.
//
// No grid is present in these MC phantoms, so the #192 aliasing that also moves
// with pixel size cannot mix into this comparison.
//
// LIMIT, stated rather than assumed away: binning models a larger pixel's
// aperture by averaging. A real detector at 0.28 mm also has its own MTF and
// noise; this experiment holds those fixed on purpose, because the question is
// about the sampling grid, not about detector physics.
// ===========================================================================
namespace {

struct Binned {
    int n = 0;
    double pitchMm = 0;
    std::vector<double> total, primary, thickness;
    std::vector<uint8_t> mask;
    int lo = 0, hi = 0;          // metric region [lo, hi) in binned pixels
};

Binned BinPhantom(const Phantom& p, int k) {
    Binned b;
    b.n = kN / k;
    b.pitchMm = kPitchMm * k;
    const size_t m = static_cast<size_t>(b.n) * b.n;
    b.total.assign(m, 0.0);
    b.primary.assign(m, 0.0);
    b.thickness.assign(m, 0.0);
    std::vector<double> air(m, 0.0);
    for (int r = 0; r < b.n; ++r)
        for (int c = 0; c < b.n; ++c) {
            double t = 0, pr = 0, th = 0, ai = 0;
            for (int dr = 0; dr < k; ++dr)
                for (int dc = 0; dc < k; ++dc) {
                    const size_t i = static_cast<size_t>(r * k + dr) * kN + (c * k + dc);
                    t += p.total[i]; pr += p.primary[i]; th += p.thickness[i]; ai += p.air[i];
                }
            const double inv = 1.0 / (k * k);
            const size_t j = static_cast<size_t>(r) * b.n + c;
            b.total[j] = t * inv; b.primary[j] = pr * inv; b.thickness[j] = th * inv; air[j] = ai * inv;
        }
    double airMax = 0;
    for (double v : air) airMax = std::max(airMax, v);
    b.mask.assign(m, 0);
    for (size_t i = 0; i < m; ++i) b.mask[i] = air[i] > 0.5 * airMax ? 1 : 0;
    // Same PHYSICAL metric region as the 512 case (cols 109..403 at 0.14 mm).
    b.lo = kLo / k;
    b.hi = kHi / k;
    return b;
}

}  // namespace

TEST(GsvgVirtualGridMc512, ErrorPeakFollowsPixelsOrMillimetres_191)
{
    const Phantom p = Load("step");

    // Boundary columns, found once on the 512 phantom (the QA-B-125 criterion).
    std::vector<double> colT0(kN, 0.0);
    for (int c = 0; c < kN; ++c) {
        double sum = 0; int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + c;
            if (!p.mask[i]) continue;
            sum += p.thickness[i]; ++n;
        }
        colT0[c] = n ? sum / n : 0.0;
    }
    std::vector<int> jumpCols;
    for (int j = kLo; j + 1 < kHi; ++j)
        if (std::fabs(colT0[j + 1] - colT0[j]) > 0.2) jumpCols.push_back(j);

    // Two arms. The post-steps (pyramid, denoise) are specified in PIXELS, so
    // their physical reach grows with k -- a second moving part that the first
    // run of this test did not hold still (the six-level pyramid reaches 128 px
    // = 17.9 mm at k=1 but 71.7 mm at k=4). The `subtract-only` arm switches
    // them off, leaving only the kernel subtraction, whose sigma is physical.
    // If the two arms disagree, the post-steps are what moved, not the peak.
    for (const bool postSteps : {true, false}) {
    std::printf("VGMC136 pixel-size sweep (post-steps %s): k, pitch_mm, N, peak_px, peak_mm\n",
                postSteps ? "ON" : "OFF");
    for (const int k : {1, 2, 4}) {
        const Binned b = BinPhantom(p, k);

        vg::VgSettings st = BaseSettings();
        st.pixelPitchMm = b.pitchMm;           // the one axis that moves
        if (!postSteps) { st.pyramidLevels = 0; st.pyramidGain = 1.0; st.denoiseK = 0.0; }
        std::vector<double> img = b.total;
        const vg::VgReport rep =
            vg::RunVirtualGrid(img, b.n, b.n, McTable(), st, vg::VgSwitches{}, b.mask.data());
        ASSERT_EQ(rep.error, "") << k;

        // The boundary set is taken from the UNBINNED phantom and mapped into
        // this rendering's columns, so all three k measure distance to the SAME
        // physical boundaries. Detecting jumps per-k instead lost boundaries as
        // binning blended them (18 -> 12 -> 11 in the first run of this test),
        // which changed the scene and not only the sampling -- two moving parts.
        std::vector<int> dist(b.n, b.n);
        for (int c = b.lo; c < b.hi; ++c)
            for (int j : jumpCols) dist[c] = std::min(dist[c], std::abs(c - j / k));
        const int jumps = static_cast<int>(jumpCols.size());

        // Bins in PIXELS of this rendering. The mm column is derived, so the
        // same rows can be read on either axis and compared across k.
        const int edges[] = {0, 1, 2, 4, 8, 16, 32, 64};
        double best = -1, bestPx = -1, last = -1;
        std::printf("VGMC136 --- k=%d pitch=%.3f mm N=%d jumps=%d factor=%d\n",
                    k, b.pitchMm, b.n, jumps, rep.factor);
        for (size_t e = 0; e + 1 < sizeof(edges) / sizeof(edges[0]); ++e) {
            std::vector<double> a;
            for (size_t i = 0; i < img.size(); ++i) {
                const int r = static_cast<int>(i) / b.n, c = static_cast<int>(i) % b.n;
                if (r < b.lo || r >= b.hi || c < b.lo || c >= b.hi) continue;
                if (!b.mask[i] || b.primary[i] <= 0) continue;
                if (dist[c] < edges[e] || dist[c] >= edges[e + 1]) continue;
                a.push_back(std::fabs(img[i] / b.primary[i] - 1.0));
            }
            if (a.empty()) continue;
            std::sort(a.begin(), a.end());
            const double med = a[a.size() / 2];
            const double midPx = 0.5 * (edges[e] + edges[e + 1]);
            std::printf("VGMC136 k=%d d=[%2d,%2d) px = [%.2f,%.2f) mm n=%6zu median=%.4f\n",
                        k, edges[e], edges[e + 1], edges[e] * b.pitchMm, edges[e + 1] * b.pitchMm,
                        a.size(), med);
            if (med > best) { best = med; bestPx = midPx; }
            last = med;
        }
        // The level moves with k as well as the shape, so the peak is also
        // reported relative to this rendering's own far-field bin. A shape
        // comparison across k needs the level divided out.
        std::printf("VGMC136 PEAK k=%d peak_px=%.1f peak_mm=%.3f median=%.4f far=%.4f peak/far=%.3f\n",
                    k, bestPx, bestPx * b.pitchMm, best, last, last > 0 ? best / last : -1.0);
    }
    }
    SUCCEED();
}

// ===========================================================================
// #191 item 1 (QA-B-137): does the boundary excess follow the PYRAMID REACH?
//
// QA-B-136 showed the excess is made by the post-steps, not by the scatter
// subtraction. This sweeps the post-step reach with everything else fixed --
// same phantom, same 0.140 mm pixel, same boundary set -- so one axis moves.
//
// TWO CORRECTIONS TO THE CARD, both read from the code rather than assumed.
//
// 1. The default is FOUR levels, not six. `VgSettings::pyramidLevels = 4`
//    (virtual_grid.h:115), and BaseSettings() does not override it, so every
//    QA-B-125/134/136 measurement ran at 4. The "six-level 128-pixel reach"
//    QA-B-136 reported (and this lane wrote into its own report) was wrong.
//
// 2. The sweep cannot go DOWN. RunVirtualGrid rejects `pyramidLevels < 4`
//    outright (virtual_grid.cpp:695, "0 = off, otherwise 4..8"), so 3 and
//    below are not reachable settings; the reach can only be made LARGER.
//    The card's shape survives -- one axis, a falsifiable prediction -- but
//    the prediction has to be stated in the direction the code allows.
//
// REACH. PyramidContrast reduces `levels` times (virtual_grid.cpp:603), so the
// coarsest Gaussian sits at 2^levels pixels: 16 px (2.24 mm) at 4 levels,
// 256 px (35.8 mm) at 8. Each level DOUBLES it.
//
// PREDICTION, falsifiable either way:
//   the excess reaches ~2^levels px -> the reach is the cause, and #191 item 1
//     is answerable on THIS phantom by moving the reach instead of the scene.
//   it does not move with 2^levels -> something else inside the post-steps
//     makes it, and that is the next question.
//
// WHAT IS LOST, reported alongside (the card asks for this explicitly): levels
// is a FEATURE, not a speed knob. Each row carries the corrected error over the
// whole region against the uncorrected one, so a later reader cannot mistake
// "fewer levels" for an improvement. This is a cause experiment, NOT a settings
// recommendation.
// ===========================================================================
TEST(GsvgVirtualGridMc512, BoundaryExcessAgainstPyramidReach_191)
{
    const Phantom p = Load("step");

    // Boundary columns, found once (the QA-B-125 criterion).
    std::vector<double> colT(kN, 0.0);
    for (int c = 0; c < kN; ++c) {
        double sum = 0; int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + c;
            if (!p.mask[i]) continue;
            sum += p.thickness[i]; ++n;
        }
        colT[c] = n ? sum / n : 0.0;
    }
    std::vector<int> jumpCols;
    for (int j = kLo; j + 1 < kHi; ++j)
        if (std::fabs(colT[j + 1] - colT[j]) > 0.2) jumpCols.push_back(j);
    std::vector<int> dist(kN, kN);
    for (int c = kLo; c < kHi; ++c)
        for (int j : jumpCols) dist[c] = std::min(dist[c], std::abs(c - j));

    auto median = [](std::vector<double> a) {
        if (a.empty()) return -1.0;
        std::sort(a.begin(), a.end());
        return a[a.size() / 2];
    };

    // Uncorrected reference: what the chain starts from.
    std::vector<double> before;
    for (size_t i = 0; i < p.total.size(); ++i) {
        if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) continue;
        before.push_back(std::fabs(p.total[i] / p.primary[i] - 1.0));
    }
    const double beforeMed = median(before);
    std::printf("VGMC137 uncorrected median|total/primary-1| = %.4f (%zu px)\n",
                beforeMed, before.size());
    std::printf("VGMC137 jumps=%zu, spacing ~%.1f px\n",
                jumpCols.size(),
                jumpCols.size() > 1
                    ? static_cast<double>(jumpCols.back() - jumpCols.front()) /
                          static_cast<double>(jumpCols.size() - 1)
                    : -1.0);

    // Finer bins than QA-B-136 used: a narrower peak would fall inside one of
    // its six bins and read as "no peak" (the gap that report listed).
    const int edges[] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48};
    constexpr size_t kBins = sizeof(edges) / sizeof(edges[0]) - 1;

    std::printf("VGMC137 levels, reach_px, reach_mm, profile..., far, peak_px, peak/far, overall, after/before\n");
    for (const int levels : {0, 4, 5, 6, 7, 8}) {
        vg::VgSettings st = BaseSettings();
        st.pyramidLevels = levels;
        if (levels == 0) { st.pyramidGain = 1.0; st.denoiseK = 0.0; }

        std::vector<double> img = p.total;
        const vg::VgReport rep =
            vg::RunVirtualGrid(img, kN, kN, McTable(), st, vg::VgSwitches{}, p.mask.data());
        ASSERT_EQ(rep.error, "") << levels;

        std::vector<double> med(kBins, -1.0);
        std::vector<size_t> cnt(kBins, 0);
        for (size_t b = 0; b < kBins; ++b) {
            std::vector<double> a;
            for (size_t i = 0; i < img.size(); ++i) {
                if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) continue;
                const int c = static_cast<int>(i) % kN;
                if (dist[c] < edges[b] || dist[c] >= edges[b + 1]) continue;
                a.push_back(std::fabs(img[i] / p.primary[i] - 1.0));
            }
            cnt[b] = a.size();
            med[b] = median(a);
        }

        // Far reference = the outermost populated bin; peak = the largest bin.
        double far = -1;
        for (size_t b = kBins; b-- > 0;) if (med[b] >= 0) { far = med[b]; break; }
        double best = -1, bestPx = -1;
        for (size_t b = 0; b < kBins; ++b)
            if (med[b] > best) { best = med[b]; bestPx = 0.5 * (edges[b] + edges[b + 1]); }

        // How far out the excess actually reaches: the outermost bin still
        // above halfway between the far level and the peak.
        double reachedPx = -1;
        const double half = far + 0.5 * (best - far);
        for (size_t b = kBins; b-- > 0;)
            if (med[b] >= 0 && med[b] >= half) { reachedPx = static_cast<double>(edges[b + 1]); break; }

        const Metrics m = Measure(img, p);
        const double reachPx = levels ? static_cast<double>(1 << levels) : 0.0;

        std::printf("VGMC137 --- levels=%d reach=%.0f px (%.2f mm) factor=%d\n",
                    levels, reachPx, reachPx * kPitchMm, rep.factor);
        for (size_t b = 0; b < kBins; ++b)
            if (med[b] >= 0)
                std::printf("VGMC137 L=%d d=[%2d,%2d) px = [%.2f,%.2f) mm n=%6zu median=%.4f\n",
                            levels, edges[b], edges[b + 1],
                            edges[b] * kPitchMm, edges[b + 1] * kPitchMm, cnt[b], med[b]);
        std::printf("VGMC137 SUMMARY levels=%d reach=%.0f px peak_px=%.1f peak=%.4f far=%.4f "
                    "peak/far=%.3f excess_reaches=%.0f px overall=%.4f after/before=%.3f\n",
                    levels, reachPx, bestPx, best, far, far > 0 ? best / far : -1.0,
                    reachedPx, m.absMedian,
                    beforeMed > 0 ? m.absMedian / beforeMed : -1.0);
    }
    SUCCEED();
}

// ===========================================================================
// #191 follow-up (2) (QA-B-138): what does raising `pyramidLevels` BUY?
//
// QA-B-137 measured only the scatter residual and found 8 levels leaves 72 % of
// the pre-correction error. 8 is a legal setting (virtual_grid.cpp:695 allows
// 0 or 4..8). Either the upper half of that range buys something the residual
// cannot see, or it buys nothing and the ceiling has no basis.
//
// WHICH AXIS, chosen by reading what the pyramid actually does
// (PyramidContrast, virtual_grid.cpp:603):
//
//   - De-noising is a soft threshold on lap[0] -- the FINEST band ONLY
//     (`if (k > 0) { std::vector<double>& b = lap[0].v; ... }`, :620). It does
//     not touch any other band, so it is the SAME work at 4 levels and at 8.
//     Raising the count cannot buy more de-noising.
//   - `gain` multiplies EVERY band on reconstruction (:629). Adding a level
//     adds one more, COARSER band to that multiplication.
//
// So the only thing `levels` buys is detail gain carried out to coarser scales.
// The axis that sees it is the contrast of a real structure against the noise
// it must be seen through: CNR across a step boundary.
//
//   contrast = |mean(plateau A) - mean(plateau B)| for adjacent plateaus,
//              interior pixels only (>= 4 px from any boundary)
//   sigma    = pixel-scale noise, std of (img - 3x3 box mean) over the same
//              pixels. The high-pass isolates noise from the low-frequency
//              correction error, which would otherwise be counted as noise.
//   CNR      = contrast / sigma, scale-free, so the DN level cancels.
//
// `primary` (the scatter-free truth) is measured the same way as the reference
// a reader needs to tell "restored" from "amplified past the truth".
//
// PREDICTION, falsifiable either way:
//   CNR rises with levels -> 4..8 is a trade (residual for detectability) and
//     the range has a basis; the knee says where.
//   CNR does NOT rise (8 no better than 4) -> the upper half buys nothing and
//     only switches the correction off. A separate card would narrow it.
//
// NOT A RECOMMENDATION, and no default is changed here.
// ===========================================================================
TEST(GsvgVirtualGridMc512, WhatRaisingPyramidLevelsBuys_191)
{
    const Phantom p = Load("step");

    // Boundaries and plateau ids, found once on the phantom.
    std::vector<double> colT(kN, 0.0);
    for (int c = 0; c < kN; ++c) {
        double sum = 0; int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + c;
            if (!p.mask[i]) continue;
            sum += p.thickness[i]; ++n;
        }
        colT[c] = n ? sum / n : 0.0;
    }
    std::vector<int> jumpCols;
    for (int j = kLo; j + 1 < kHi; ++j)
        if (std::fabs(colT[j + 1] - colT[j]) > 0.2) jumpCols.push_back(j);
    std::vector<int> dist(kN, kN), plateau(kN, -1);
    for (int c = kLo; c < kHi; ++c) {
        for (int j : jumpCols) dist[c] = std::min(dist[c], std::abs(c - j));
        int id = 0;
        for (int j : jumpCols) if (c > j) ++id;
        plateau[c] = id;
    }
    const int plateaus = static_cast<int>(jumpCols.size()) + 1;

    // Interior pixels only: >= 4 px from any boundary, so the boundary excess
    // QA-B-137 measured is not what moves this number.
    auto interior = [&](size_t i) {
        if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) return false;
        return dist[static_cast<int>(i) % kN] >= 4;
    };

    // contrast / sigma / CNR over the adjacent-plateau pairs.
    // `sep` = how many plateaus apart the two regions are, i.e. the SCALE of
    // the structure whose contrast is measured. sep=1 is ~16 px (2.3 mm); sep=4
    // is ~63 px (8.9 mm). This is the axis's own blind spot made measurable: a
    // coarse band cannot tell two plateaus apart when its reach spans both, so
    // a result at sep=1 says nothing about coarser structure.
    auto measureCnr = [&](const std::vector<double>& img, int sep,
                          double* outContrast, double* outSigma) {
        std::vector<double> sum(static_cast<size_t>(plateaus), 0.0);
        std::vector<size_t> cnt(static_cast<size_t>(plateaus), 0);
        std::vector<double> hp;                     // pixel-scale residual
        for (int r = kLo + 1; r + 1 < kHi; ++r)
            for (int c = kLo + 1; c + 1 < kHi; ++c) {
                const size_t i = static_cast<size_t>(r) * kN + c;
                if (!interior(i)) continue;
                sum[static_cast<size_t>(plateau[c])] += img[i];
                ++cnt[static_cast<size_t>(plateau[c])];
                double box = 0;
                for (int dr = -1; dr <= 1; ++dr)
                    for (int dc = -1; dc <= 1; ++dc)
                        box += img[static_cast<size_t>(r + dr) * kN + (c + dc)];
                hp.push_back(img[i] - box / 9.0);
            }
        double hpMean = 0;
        for (double v : hp) hpMean += v;
        hpMean /= static_cast<double>(hp.size());
        double var = 0;
        for (double v : hp) var += (v - hpMean) * (v - hpMean);
        // The 3x3 high pass keeps 8/9 of a white-noise sample's variance.
        const double sigma = std::sqrt(var / static_cast<double>(hp.size()) * 9.0 / 8.0);

        std::vector<double> gaps;
        for (int a = 0; a + sep < plateaus; ++a) {
            const size_t ia = static_cast<size_t>(a), ib = static_cast<size_t>(a + sep);
            if (cnt[ia] < 100 || cnt[ib] < 100) continue;
            const double ma = sum[ia] / static_cast<double>(cnt[ia]);
            const double mb = sum[ib] / static_cast<double>(cnt[ib]);
            gaps.push_back(std::fabs(ma - mb));
        }
        std::sort(gaps.begin(), gaps.end());
        const double contrast = gaps.empty() ? -1.0 : gaps[gaps.size() / 2];
        if (outContrast) *outContrast = contrast;
        if (outSigma) *outSigma = sigma;
        return sigma > 0 ? contrast / sigma : -1.0;
    };

    double cTruth = 0, sTruth = 0;
    double cTruth4 = 0, sT4 = 0;
    const double cnrTruth = measureCnr(p.primary, 1, &cTruth, &sTruth);
    const double cnrTruth4 = measureCnr(p.primary, 4, &cTruth4, &sT4);
    double cRaw = 0, sRaw = 0;
    const double cnrRaw = measureCnr(p.total, 1, &cRaw, &sRaw);

    std::vector<double> beforeAbs;
    for (size_t i = 0; i < p.total.size(); ++i) {
        if (!InRegion(i) || !p.mask[i] || p.primary[i] <= 0) continue;
        beforeAbs.push_back(std::fabs(p.total[i] / p.primary[i] - 1.0));
    }
    std::sort(beforeAbs.begin(), beforeAbs.end());
    const double beforeMed = beforeAbs[beforeAbs.size() / 2];

    std::printf("VGMC138 plateaus=%d boundaries=%zu; interior = dist >= 4 px\n",
                plateaus, jumpCols.size());
    std::printf("VGMC138 TRUTH   (primary) contrast=%.1f sigma=%.1f CNR=%.2f\n",
                cTruth, sTruth, cnrTruth);
    std::printf("VGMC138 TRUTH   (primary) sep=4 contrast=%.1f CNR=%.2f\n",
                cTruth4, cnrTruth4);
    std::printf("VGMC138 RAW     (total)   contrast=%.1f sigma=%.1f CNR=%.2f residual=%.4f\n",
                cRaw, sRaw, cnrRaw, beforeMed);

    std::printf("VGMC138 levels, reach_px, residual after/before, contrast, sigma, CNR, CNR/truth\n");
    for (const int levels : {0, 4, 5, 6, 7, 8}) {
        vg::VgSettings st = BaseSettings();
        st.pyramidLevels = levels;
        if (levels == 0) { st.pyramidGain = 1.0; st.denoiseK = 0.0; }

        std::vector<double> img = p.total;
        const vg::VgReport rep =
            vg::RunVirtualGrid(img, kN, kN, McTable(), st, vg::VgSwitches{}, p.mask.data());
        ASSERT_EQ(rep.error, "") << levels;

        double c = 0, s = 0;
        const double cnr = measureCnr(img, 1, &c, &s);
        double c4 = 0, s4 = 0;
        const double cnr4 = measureCnr(img, 4, &c4, &s4);
        const Metrics m = Measure(img, p);
        std::printf("VGMC138 levels=%d reach=%4d px residual=%.4f after/before=%.3f "
                    "contrast=%.1f sigma=%.1f CNR=%.2f CNR/truth=%.3f\n",
                    levels, levels ? (1 << levels) : 0, m.absMedian,
                    beforeMed > 0 ? m.absMedian / beforeMed : -1.0,
                    c, s, cnr, cnrTruth > 0 ? cnr / cnrTruth : -1.0);
        std::printf("VGMC138 levels=%d sep=4 contrast=%.1f CNR=%.2f CNR/truth=%.3f\n",
                    levels, c4, cnr4, cnrTruth4 > 0 ? cnr4 / cnrTruth4 : -1.0);
    }
    SUCCEED();
}
