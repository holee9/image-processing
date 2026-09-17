// #180 (QA-B-91): virtual grid (virtual_grid.cpp).
//
// What these cases can show: the algorithm recovers the primary of images
// built with ITS OWN forward model and ITS OWN (synthetic) table. That says
// nothing about real scatter -- a model that is consistently wrong passes a
// self-consistency test. Agreement with physics waits for the MC tables
// (QA-A-94..97) and a real detector (#151).
//
// Scenes are never a single uniform slab (#148): a thickness step and a smooth
// gradient, each with small high-attenuation details.
//
// Table: tests/data/virtual_grid_synthetic_table.csv (invented numbers).

#include <gtest/gtest.h>

#include "virtual_grid.h"
#include "perf_measure.h"

#include "xpe/gsvg/gsvg_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace vg = xpe_gsvg_detail;

namespace {

constexpr const char* kTablePath = "tests/data/virtual_grid_synthetic_table.csv";
constexpr int    kN = 256;
constexpr double kPitchMm = 1.0;
constexpr double kKvp = 80.0;
constexpr double kI0 = 60000.0;
constexpr double kIdealRatio = 100.0;   // tp 1, ts 0 in the synthetic table

std::string ReadFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

const vg::ParamTable& Table() {
    static vg::ParamTable t;
    static const std::string err = vg::LoadParamTable(kTablePath, t);
    EXPECT_EQ(err, "");
    return t;
}

// Replace every kernel amplitude by factor * amplitude (an over-estimating table).
vg::ParamTable ScaledKernels(double factor) {
    vg::ParamTable t = Table();
    for (auto& n : t.kernels)
        for (int i = 0; i < n.terms; ++i) n.a[i] *= factor;
    return t;
}

enum class Shape { Step, Gradient, ThinStep, ThickNextToAir, ThinNextToThick };

struct Scene {
    std::vector<double> primary, thickness, measured;
};

// Primary from a thickness layout plus details, then the true thickness is
// re-derived from that primary so the forward model is exactly the one the
// algorithm assumes.
// thickPatch: a 36 x 36 px square (about 2 % of the image) of 40 cm water in
// the thin half -- above the 30 cm table. Its forward scatter uses the 30 cm
// kernel, the same limit the chain applies.
constexpr int kPatchX0 = 60, kPatchY0 = 150, kPatch = 36;

bool InPatch(int x, int y, int margin = 0) {
    return x >= kPatchX0 - margin && x < kPatchX0 + kPatch + margin &&
           y >= kPatchY0 - margin && y < kPatchY0 + kPatch + margin;
}

Scene MakeScene(Shape shape, const vg::ParamTable& forwardTable = Table(), bool thickPatch = false) {
    const vg::ParamTable& t = Table();
    // wet coefficients at 80 kVp (a node of the synthetic table)
    const double w0 = t.wetW0[1], a = t.wetA[1], b = t.wetB[1];
    Scene s;
    s.primary.resize(kN * kN);
    s.thickness.resize(kN * kN);
    for (int y = 0; y < kN; ++y)
        for (int x = 0; x < kN; ++x) {
            double T = shape == Shape::Step     ? (x < kN / 2 ? 8.0 : 24.0)
                     : shape == Shape::ThinStep ? (x < kN / 2 ? 4.0 : 10.0)
                     : shape == Shape::ThickNextToAir ? (x < kN / 2 ? 0.3 : 25.0)
                     : shape == Shape::ThinNextToThick ? (x < kN / 2 ? 5.0 : 27.0)
                                                : 6.0 + 20.0 * x / (kN - 1.0);
            // details: small dense disks and a thin bar
            const double dx = (x % 48) - 24.0, dy = (y % 48) - 24.0;
            if (dx * dx + dy * dy < 16.0) T += 2.0;
            if (y > 100 && y < 104 && x > 20 && x < 236) T += 1.5;
            T = std::min(T, 29.0);
            if (thickPatch && InPatch(x, y)) T = 40.0;
            const double mu = w0 - a * T / (1 + b * T);
            s.primary[y * kN + x] = kI0 * std::exp(-mu * T);
        }
    for (size_t i = 0; i < s.primary.size(); ++i)
    {
        const double tt = vg::ThicknessFromLogAtten(-std::log(s.primary[i] / kI0), w0, a, b, 30.0);
        s.thickness[i] = tt < 0 ? 30.0 : tt;
    }
    s.measured = vg::ForwardScatter(s.primary, s.thickness, kN, kN, forwardTable, kKvp, kPitchMm);
    return s;
}

vg::VgSettings Settings(int iterations, double ratio = kIdealRatio) {
    vg::VgSettings st;
    st.kvp = kKvp;
    st.gridRatio = ratio;
    st.pixelPitchMm = kPitchMm;
    st.airSignal = kI0;
    st.iterations = iterations;
    return st;
}

struct ErrStats { double median = 0, p95 = 0, max = 0; };

ErrStats RelErr(const std::vector<double>& got, const std::vector<double>& want,
                int x0 = 0, int x1 = kN) {
    std::vector<double> e;
    for (int y = 0; y < kN; ++y)
        for (int x = x0; x < x1; ++x) {
            const size_t i = static_cast<size_t>(y) * kN + x;
            e.push_back(std::fabs(got[i] - want[i]) / want[i]);
        }
    std::sort(e.begin(), e.end());
    return {e[e.size() / 2], e[e.size() * 95 / 100], e.back()};
}

void Log(const char* what, const ErrStats& s) {
    std::printf("VGMEASURE %s median=%.5f p95=%.5f max=%.5f\n", what, s.median, s.p95, s.max);
}

}  // namespace

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------
TEST(GsvgVirtualGridTable, SyntheticTableLoads)
{
    const vg::ParamTable& t = Table();
    EXPECT_EQ(t.kThick, (std::vector<double>{5, 10, 20, 30}));
    EXPECT_EQ(t.kKvp, (std::vector<double>{60, 80, 100}));
    ASSERT_EQ(t.kernels.size(), 12u);
    EXPECT_EQ(t.kernels[1 * 3 + 1].terms, 2);
    EXPECT_DOUBLE_EQ(t.kernels[1 * 3 + 1].a[0], 0.32);   // 10 cm, 80 kVp
    EXPECT_DOUBLE_EQ(t.kernels[1 * 3 + 1].s[1], 4.7);
    EXPECT_EQ(t.gridRatio.size(), 5u);
    EXPECT_EQ(t.capSpr.size(), 12u);
}

// [spr_cap] is optional since QA-B-93 (see GsvgVirtualGridCap).
TEST(GsvgVirtualGridTable, RequiredSectionsAreRequired)
{
    const std::string full = ReadFile(kTablePath);
    for (const char* sec : {"\n[kernels]\n", "\n[wet]\n", "\n[grid]\n"}) {
        std::string text = full;
        const size_t p = text.find(sec);   // the header line, not the comment
        ASSERT_NE(p, std::string::npos);
        text.replace(p, std::string(sec).size(), "\n[unused]\n");
        vg::ParamTable t;
        EXPECT_NE(vg::ParseParamTable(text, t), "") << sec;
    }
    vg::ParamTable t;
    EXPECT_EQ(vg::ParseParamTable(full, t), "");   // control: the untouched file parses
}

TEST(GsvgVirtualGridTable, IncompleteGridAndBadRowsAreRejected)
{
    const std::string full = ReadFile(kTablePath);
    auto without = [&](const std::string& line) {
        std::string text = full;
        const size_t p = text.find(line);
        EXPECT_NE(p, std::string::npos) << line;
        text.erase(p, line.size() + 1);
        vg::ParamTable t;
        return vg::ParseParamTable(text, t);
    };
    EXPECT_NE(without("20,80,gauss2,0.50,1.2,2.00,5.2,0,0,0,0"), "");
    EXPECT_NE(without("20,80,3.8"), "");

    auto replaced = [&](const std::string& from, const std::string& to) {
        std::string text = full;
        const size_t p = text.find(from);
        EXPECT_NE(p, std::string::npos) << from;
        text.replace(p, from.size(), to);
        vg::ParamTable t;
        return vg::ParseParamTable(text, t);
    };
    EXPECT_NE(replaced("10,80,gauss2,0.32", "10,80,gauss2,x"), "");       // non-numeric
    EXPECT_NE(replaced("12,0.66,0.07", "12,0.00,0.07"), "");              // tp = 0
    EXPECT_NE(replaced("80,0.22,0.015,0.10", "80,0.22,0.050,0.00"), "");  // mu(t)*t turns down
    vg::ParamTable empty;
    EXPECT_NE(vg::ParseParamTable("", empty), "");
}

TEST(GsvgVirtualGridTable, Gauss4RowsWinOverGauss2)
{
    // Same form as tools/mcsim/tables: both models for every node.
    std::string text = ReadFile(kTablePath);
    const size_t k = text.find("\n[kernels]\n") + 1;
    const size_t w = text.find("\n[wet]\n") + 1;
    std::string kernels = "[kernels]\nthickness_cm,kvp,model,a1,s1,a2,s2,a3,s3,a4,s4\n";
    for (int t : {5, 10, 20, 30})
        for (int kv : {60, 80, 100}) {
            kernels += std::to_string(t) + "," + std::to_string(kv) + ",gauss2,0.1,1,0.2,4,,,,\n";
            kernels += std::to_string(t) + "," + std::to_string(kv) + ",gauss4,0.1,1,0.2,2,0.3,4,0.4,8\n";
        }
    text.replace(k, w - k, kernels + "\n");
    vg::ParamTable t;
    ASSERT_EQ(vg::ParseParamTable(text, t), "");
    EXPECT_EQ(t.kernels[0].terms, 4);
    EXPECT_DOUBLE_EQ(t.kernels[0].s[3], 8.0);
}

// ---------------------------------------------------------------------------
// Kernel and thickness
// ---------------------------------------------------------------------------
TEST(GsvgVirtualGridKernel, NodesBlendsAndBounds)
{
    const vg::ParamTable& t = Table();
    vg::BlendedKernel k;
    auto total = [&] { double s = 0; for (double a : k.a) s += a; return s; };

    ASSERT_TRUE(vg::KernelAt(t, 10.0, 80.0, k));
    EXPECT_NEAR(total(), 0.32 + 0.80, 1e-12);

    ASSERT_TRUE(vg::KernelAt(t, 15.0, 70.0, k));   // centre of four nodes
    const double expect = 0.25 * ((0.35 + 0.75) + (0.32 + 0.80) + (0.55 + 1.85) + (0.50 + 2.00));
    EXPECT_NEAR(total(), expect, 1e-12);
    EXPECT_EQ(k.a.size(), 8u);   // four node kernels, blended, not refitted

    ASSERT_TRUE(vg::KernelAt(t, 2.5, 80.0, k));    // half way to the first node
    EXPECT_NEAR(total(), 0.5 * (0.18 + 0.32), 1e-12);
    ASSERT_TRUE(vg::KernelAt(t, 0.0, 80.0, k));
    EXPECT_EQ(total(), 0.0);

    EXPECT_FALSE(vg::KernelAt(t, 30.5, 80.0, k));
    EXPECT_FALSE(vg::KernelAt(t, 10.0, 59.0, k));
    EXPECT_FALSE(vg::KernelAt(t, 10.0, 101.0, k));
}

TEST(GsvgVirtualGridKernel, ThicknessInversionRoundTrips)
{
    const double w0 = 0.22, a = 0.015, b = 0.1;
    for (double T : {0.0, 0.5, 5.0, 12.3, 29.9}) {
        const double L = (w0 - a * T / (1 + b * T)) * T;
        EXPECT_NEAR(vg::ThicknessFromLogAtten(L, w0, a, b, 30.0), T, 1e-9) << T;
    }
    EXPECT_LT(vg::ThicknessFromLogAtten(100.0, w0, a, b, 30.0), 0.0);
}

// The scatter of a uniform, very wide primary equals sum(a) * P away from the
// edges: the discrete kernels keep the table's normalisation.
TEST(GsvgVirtualGridKernel, ConvolutionKeepsTheTableNormalisation)
{
    const int n = 400;
    std::vector<double> p(n * n, 1000.0), t(n * n, 10.0);
    const auto s = vg::ScatterEstimate(p, t, n, n, Table(), 80.0, 0.1);
    ASSERT_EQ(s.size(), p.size());
    EXPECT_NEAR(s[200 * n + 200] / 1000.0, 0.32 + 0.80, 0.005);
    // collimated field: the corner sees about a quarter of the wide tail
    EXPECT_LT(s[0], 0.4 * s[200 * n + 200]);
}

// ---------------------------------------------------------------------------
// Recovery on self-consistent synthetic images
// ---------------------------------------------------------------------------
class GsvgVirtualGridRecovery : public ::testing::TestWithParam<Shape> {};

TEST_P(GsvgVirtualGridRecovery, RecoversThePrimary)
{
    const Scene s = MakeScene(GetParam());
    const ErrStats before = RelErr(s.measured, s.primary);
    std::vector<double> img = s.measured;
    const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, Table(), Settings(5));
    ASSERT_EQ(rep.error, "");
    const ErrStats after = RelErr(img, s.primary);
    Log(GetParam() == Shape::Step ? "step.before" : "gradient.before", before);
    Log(GetParam() == Shape::Step ? "step.after5" : "gradient.after5", after);
    std::printf("VGMEASURE factor=%d coarse=%dx%d maxT=%.2f meanSpr=%.3f\n",
                rep.factor, rep.coarseW, rep.coarseH, rep.maxThicknessCm, rep.meanSpr);
    EXPECT_EQ(rep.factor, 5);   // floor(0.5 * 1.0 cm narrowest term at 80 kVp / 0.1 cm)
    EXPECT_LT(after.median, 0.1 * before.median);
    EXPECT_LT(after.p95, 0.2 * before.p95);
    EXPECT_EQ(rep.negativePrimary, 0u);
    EXPECT_EQ(rep.clampedHighFraction, 0.0);
}

// Falsification: one iteration leaves more error than five.
TEST_P(GsvgVirtualGridRecovery, OneIterationIsWorse)
{
    const Scene s = MakeScene(GetParam());
    std::vector<double> one = s.measured, five = s.measured;
    ASSERT_EQ(vg::RunVirtualGrid(one, kN, kN, Table(), Settings(1)).error, "");
    ASSERT_EQ(vg::RunVirtualGrid(five, kN, kN, Table(), Settings(5)).error, "");
    const ErrStats e1 = RelErr(one, s.primary), e5 = RelErr(five, s.primary);
    Log(GetParam() == Shape::Step ? "step.after1" : "gradient.after1", e1);
    EXPECT_GT(e1.median, 2.0 * e5.median);
}

INSTANTIATE_TEST_SUITE_P(Shapes, GsvgVirtualGridRecovery,
                         ::testing::Values(Shape::Step, Shape::Gradient),
                         [](const auto& info) { return info.param == Shape::Step ? "Step" : "Gradient"; });

// Falsification: one global thickness instead of the per-pixel index raises
// the error next to the thickness step.
TEST(GsvgVirtualGridFalsify, GlobalThicknessIsWorseAtTheStep)
{
    const Scene s = MakeScene(Shape::Step);
    std::vector<double> indexed = s.measured, global = s.measured;
    vg::VgSwitches off;
    off.thicknessIndex = false;
    ASSERT_EQ(vg::RunVirtualGrid(indexed, kN, kN, Table(), Settings(5)).error, "");
    ASSERT_EQ(vg::RunVirtualGrid(global, kN, kN, Table(), Settings(5), off).error, "");
    // +-3 cm around the step at x = 128
    const ErrStats ei = RelErr(indexed, s.primary, 98, 158);
    const ErrStats eg = RelErr(global, s.primary, 98, 158);
    Log("step.edge.indexed", ei);
    Log("step.edge.global", eg);
    EXPECT_GT(eg.median, 3.0 * ei.median);
}

// Falsification: an over-estimating table (kernels x3) against data made with
// the true one. With the cap the SPR is limited and no primary goes negative.
// Without it, the full-resolution primary I - S goes negative where the
// up-sampled coarse scatter exceeds a dark fine detail (QA-B-93).
TEST(GsvgVirtualGridFalsify, SprCapPreventsOvercorrection)
{
    const Scene s = MakeScene(Shape::ThinStep);
    const vg::ParamTable over = ScaledKernels(3.0);
    std::vector<double> capped = s.measured, uncapped = s.measured;
    vg::VgSwitches off;
    off.cap = vg::CapMode::None;
    const vg::VgReport rc = vg::RunVirtualGrid(capped, kN, kN, over, Settings(5));
    const vg::VgReport ru = vg::RunVirtualGrid(uncapped, kN, kN, over, Settings(5), off);
    std::printf("VGMEASURE overcorrect capped: err='%s' negative=%zu capped=%.4f high=%.4f meanSpr=%.3f\n",
                rc.error.c_str(), rc.negativePrimary, rc.cappedFraction, rc.clampedHighFraction, rc.meanSpr);
    std::printf("VGMEASURE overcorrect uncapped: err='%s' negative=%zu capped=%.4f high=%.4f meanSpr=%.3f\n",
                ru.error.c_str(), ru.negativePrimary, ru.cappedFraction, ru.clampedHighFraction, ru.meanSpr);
    ASSERT_EQ(rc.error, "");
    EXPECT_GT(rc.cappedFraction, 0.0);
    EXPECT_EQ(rc.negativePrimary, 0u);
    double minCapped = 1e300;
    for (double v : capped) minCapped = std::min(minCapped, v);
    EXPECT_GT(minCapped, 0.0);
    ASSERT_EQ(ru.error, "");
    EXPECT_GT(ru.negativePrimary, 0u);
}

// QA-B-93 (1): without [spr_cap] the cap is sum(a_i) of the kernel rows.
namespace {
vg::ParamTable TableWithoutCapSection() {
    std::string text = ReadFile(kTablePath);
    const size_t p = text.find("\n[spr_cap]\n");
    EXPECT_NE(p, std::string::npos);
    text.erase(p + 1);
    vg::ParamTable t;
    EXPECT_EQ(vg::ParseParamTable(text, t), "");
    return t;
}
}  // namespace

TEST(GsvgVirtualGridCap, KernelSumIsTheDefaultCap)
{
    const vg::ParamTable t = TableWithoutCapSection();
    ASSERT_TRUE(t.capFromKernels);
    EXPECT_FALSE(Table().capFromKernels);   // control: the full file keeps its section
    EXPECT_NEAR(vg::SprCapAt(t, 10.0, 80.0), 0.32 + 0.80, 1e-12);
    EXPECT_NEAR(vg::SprCapAt(t, 20.0, 100.0), 0.45 + 2.15, 1e-12);
    const double mid = 0.25 * ((0.35 + 0.75) + (0.32 + 0.80) + (0.55 + 1.85) + (0.50 + 2.00));
    EXPECT_NEAR(vg::SprCapAt(t, 15.0, 70.0), mid, 1e-12);
    EXPECT_NEAR(vg::SprCapAt(t, 2.5, 80.0), 0.5 * (0.18 + 0.32), 1e-12);
    EXPECT_NEAR(vg::SprCapAt(t, 45.0, 80.0), 0.60 + 3.30, 1e-12);   // above the table: its maximum
    EXPECT_LT(vg::SprCapAt(t, 10.0, 120.0), 0.0);
    // the [spr_cap] section, when present, is what is used
    EXPECT_NEAR(vg::SprCapAt(Table(), 10.0, 80.0), 1.7, 1e-12);
    // gauss4 rows: the cap comes from the same (gauss4) rows as the kernel
    std::string text = ReadFile(kTablePath);
    const size_t k = text.find("\n[kernels]\n") + 1;
    const size_t w = text.find("\n[wet]\n") + 1;
    std::string kernels = "[kernels]\nthickness_cm,kvp,model,a1,s1,a2,s2,a3,s3,a4,s4\n";
    for (int th : {5, 10, 20, 30})
        for (int kv : {60, 80, 100}) {
            kernels += std::to_string(th) + "," + std::to_string(kv) + ",gauss2,9,1,9,4,,,,\n";
            kernels += std::to_string(th) + "," + std::to_string(kv) + ",gauss4,0.1,1,0.2,2,0.3,4,0.4,8\n";
        }
    text.replace(k, w - k, kernels + "\n");
    text.erase(text.find("\n[spr_cap]\n") + 1);
    vg::ParamTable g4;
    ASSERT_EQ(vg::ParseParamTable(text, g4), "");
    EXPECT_NEAR(vg::SprCapAt(g4, 10.0, 80.0), 1.0, 1e-12);
}

// C1 (local sum(a_i)), kept as the record of why it was not chosen (QA-B-94).
TEST(GsvgVirtualGridCap, LocalKernelSumCapBindsOnCorrectDataToo)
{
    vg::VgSwitches c1;
    c1.cap = vg::CapMode::LocalSum;
    const Scene s = MakeScene(Shape::Step);
    const vg::ParamTable t = TableWithoutCapSection();
    vg::ParamTable over = t;
    for (auto& n : over.kernels)
        for (int i = 0; i < n.terms; ++i) n.a[i] *= 3.0;
    // Over-estimating kernels scale their own cap too, so the cap alone
    // cannot catch them; the check here is that the default cap is live.
    std::vector<double> a = s.measured, b = s.measured;
    const vg::VgReport ra = vg::RunVirtualGrid(a, kN, kN, t, Settings(5), c1);
    // Scaling the kernels but not the cap (cap taken from the true table).
    vg::ParamTable overTrueCap = over;
    overTrueCap.capFromKernels = false;
    overTrueCap.capThick = t.kThick;
    overTrueCap.capKvp = t.kKvp;
    overTrueCap.capSpr.clear();
    for (const auto& n : t.kernels) overTrueCap.capSpr.push_back(n.a[0] + n.a[1]);
    const vg::VgReport rb = vg::RunVirtualGrid(b, kN, kN, overTrueCap, Settings(5), c1);
    // Reference: the same data with the generous synthetic [spr_cap] section.
    std::vector<double> r = s.measured;
    const vg::VgReport rr = vg::RunVirtualGrid(r, kN, kN, Table(), Settings(5), c1);
    const ErrStats eSum = RelErr(a, s.primary), eSec = RelErr(r, s.primary);
    std::printf("VGMEASURE kernel-sum cap: true data capped=%.4f; x3 kernels capped=%.4f negative=%zu\n",
                ra.cappedFraction, rb.cappedFraction, rb.negativePrimary);
    Log("step.after5.kernelSumCap", eSum);
    Log("step.after5.sectionCap", eSec);
    ASSERT_EQ(ra.error, "");
    ASSERT_EQ(rb.error, "");
    ASSERT_EQ(rr.error, "");
    // Even on data made with these kernels the local SPR can exceed the local
    // sum(a_i): a thin pixel next to a thick region receives that region's wider
    // scatter. The cap then binds on a few percent of the reduced grid.
    EXPECT_LT(ra.cappedFraction, 0.10);
    EXPECT_EQ(rr.cappedFraction, 0.0);
    EXPECT_GT(rb.cappedFraction, 0.5);
    EXPECT_EQ(rb.negativePrimary, 0u);
}

// QA-B-93 (2): a region thicker than the table is limited, not refused.
TEST(GsvgVirtualGridRange, ThickRegionIsLimitedAndReported)
{
    const Scene plain = MakeScene(Shape::Step);
    const Scene patched = MakeScene(Shape::Step, Table(), true);
    std::vector<double> a = plain.measured, b = patched.measured;
    const vg::VgReport ra = vg::RunVirtualGrid(a, kN, kN, Table(), Settings(5));
    const vg::VgReport rb = vg::RunVirtualGrid(b, kN, kN, Table(), Settings(5));
    ASSERT_EQ(ra.error, "");
    ASSERT_EQ(rb.error, "");
    // error away from the patch (3 cm margin), same pixels in both scenes
    auto outside = [&](const std::vector<double>& got, const std::vector<double>& want) {
        std::vector<double> e;
        for (int y = 0; y < kN; ++y)
            for (int x = 0; x < kN; ++x)
                if (!InPatch(x, y, 30)) e.push_back(std::fabs(got[y * kN + x] - want[y * kN + x]) / want[y * kN + x]);
        std::sort(e.begin(), e.end());
        return ErrStats{e[e.size() / 2], e[e.size() * 95 / 100], e.back()};
    };
    const ErrStats ea = outside(a, plain.primary), eb = outside(b, patched.primary);
    Log("patch.none.outside", ea);
    Log("patch.40cm.outside", eb);
    const double area = double(kPatch * kPatch) / (kN * kN);
    std::printf("VGMEASURE patch area=%.4f aboveFullRes=%.4f clampedHigh(reduced)=%.4f below=%.4f\n",
                area, rb.aboveTableFullRes, rb.clampedHighFraction, rb.belowTableFraction);
    std::printf("VGMEASURE no patch: aboveFullRes=%.4f clampedHigh=%.4f\n",
                ra.aboveTableFullRes, ra.clampedHighFraction);
    EXPECT_EQ(ra.clampedHighFraction, 0.0);
    EXPECT_EQ(ra.aboveTableFullRes, 0.0);
    EXPECT_GT(rb.clampedHighFraction, 0.0);
    EXPECT_LE(rb.clampedHighFraction, area);
    EXPECT_NEAR(rb.aboveTableFullRes, area, 0.25 * area);
    EXPECT_LT(eb.median, 1.5 * ea.median);
    EXPECT_LT(eb.p95, 1.5 * ea.p95);

    // Falsification: without the limit the image is refused, as before QA-B-93.
    std::vector<double> c = patched.measured;
    vg::VgSwitches off;
    off.clampThickness = false;
    const vg::VgReport rc = vg::RunVirtualGrid(c, kN, kN, Table(), Settings(5), off);
    EXPECT_NE(rc.error, "");
    EXPECT_EQ(c, patched.measured);
}

// Thin regions (below the first kernel node) were never refused: the kernel
// fades to no scatter at 0 cm. They are counted, not limited.
TEST(GsvgVirtualGridRange, ThinRegionIsCountedNotRefused)
{
    const Scene s = MakeScene(Shape::ThinStep);   // 4 cm | 10 cm
    std::vector<double> img = s.measured;
    const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, Table(), Settings(5));
    ASSERT_EQ(rep.error, "");
    std::printf("VGMEASURE thin below=%.4f high=%.4f\n", rep.belowTableFraction, rep.clampedHighFraction);
    EXPECT_GT(rep.belowTableFraction, 0.3);
    EXPECT_LT(RelErr(img, s.primary).median, 0.1 * RelErr(s.measured, s.primary).median);
}

// ---------------------------------------------------------------------------
// Grid ratio
// ---------------------------------------------------------------------------
TEST(GsvgVirtualGridRatio, ResidualSprFallsWithRatio)
{
    const Scene s = MakeScene(Shape::Gradient);
    double prev = 1e300;
    for (double ratio : {6.0, 8.0, 10.0, 12.0}) {
        std::vector<double> img = s.measured;
        ASSERT_EQ(vg::RunVirtualGrid(img, kN, kN, Table(), Settings(5, ratio)).error, "");
        double spr = 0;
        for (size_t i = 0; i < img.size(); ++i) spr += (img[i] - s.primary[i]) / s.primary[i];
        spr /= static_cast<double>(img.size());
        std::printf("VGMEASURE ratio=%g residualSpr=%.4f\n", ratio, spr);
        EXPECT_LT(spr, prev) << ratio;
        EXPECT_GT(spr, 0.0);
        prev = spr;
    }
}

TEST(GsvgVirtualGridRatio, UnknownRatioIsRefusedAndImageKept)
{
    const Scene s = MakeScene(Shape::Gradient);
    std::vector<double> img = s.measured;
    const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, Table(), Settings(5, 7.0));
    EXPECT_NE(rep.error, "");
    EXPECT_EQ(img, s.measured);
}

// ---------------------------------------------------------------------------
// Pyramid and de-noise
// ---------------------------------------------------------------------------
TEST(GsvgVirtualGridPyramid, UnitGainIsIdentityAndGainRaisesDetail)
{
    const Scene s = MakeScene(Shape::Step);
    std::vector<double> plain = s.measured, unit = s.measured, boosted = s.measured;
    vg::VgSettings st = Settings(3);
    ASSERT_EQ(vg::RunVirtualGrid(plain, kN, kN, Table(), st).error, "");
    st.pyramidLevels = 5;
    st.pyramidGain = 1.0;
    ASSERT_EQ(vg::RunVirtualGrid(unit, kN, kN, Table(), st).error, "");
    double maxDiff = 0;
    for (size_t i = 0; i < plain.size(); ++i) maxDiff = std::max(maxDiff, std::fabs(plain[i] - unit[i]));
    EXPECT_LT(maxDiff, 1e-6);

    st.pyramidGain = 1.5;
    ASSERT_EQ(vg::RunVirtualGrid(boosted, kN, kN, Table(), st).error, "");
    // contrast of a detail disk against its surroundings (disk centre at 24,24)
    auto contrast = [&](const std::vector<double>& im) { return im[24 * kN + 30] - im[24 * kN + 24]; };
    std::printf("VGMEASURE disk contrast plain=%.1f boosted=%.1f\n", contrast(plain), contrast(boosted));
    EXPECT_GT(contrast(boosted), 1.2 * contrast(plain));
}

TEST(GsvgVirtualGridPyramid, DenoiseLowersFlatRegionNoise)
{
    // Flat region with deterministic pseudo-noise. Noise in the output is read
    // as (output of noisy input) - (output of the same input without noise),
    // so the scatter fall-off towards the edges does not count as noise.
    std::vector<double> clean(kN * kN, 20000.0), noisy(kN * kN);
    uint32_t state = 12345;
    for (size_t i = 0; i < noisy.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        noisy[i] = clean[i] + static_cast<double>((state >> 8) % 2001) - 1000.0;
    }
    auto run = [&](std::vector<double> img, double k) {
        vg::VgSettings st = Settings(3);
        st.pyramidLevels = 4;
        st.denoiseK = k;
        EXPECT_EQ(vg::RunVirtualGrid(img, kN, kN, Table(), st).error, "");
        return img;
    };
    auto noiseSd = [&](double k) {
        const auto a = run(noisy, k), b = run(clean, k);
        double q = 0;
        for (size_t i = 0; i < a.size(); ++i) q += (a[i] - b[i]) * (a[i] - b[i]);
        return std::sqrt(q / static_cast<double>(a.size()));
    };
    const double off = noiseSd(0.0), on = noiseSd(3.0);
    std::printf("VGMEASURE flat noise sd input=577.4 k0=%.1f k3=%.1f\n", off, on);
    EXPECT_LT(on, 0.8 * off);
}

TEST(GsvgVirtualGridPyramid, BadPostSettingsAreRefused)
{
    std::vector<double> img(kN * kN, 20000.0);
    const std::vector<double> orig = img;
    vg::VgSettings st = Settings(3);
    st.pyramidLevels = 3;
    EXPECT_NE(vg::RunVirtualGrid(img, kN, kN, Table(), st).error, "");
    st.pyramidLevels = 0;
    st.denoiseK = 2.0;
    EXPECT_NE(vg::RunVirtualGrid(img, kN, kN, Table(), st).error, "");
    st = Settings(3);
    st.pyramidLevels = 8;   // needs >= 256 px: exactly fits
    EXPECT_EQ(vg::RunVirtualGrid(img, kN, kN, Table(), st).error, "");
    std::vector<double> small(100 * 100, 20000.0);
    EXPECT_NE(vg::RunVirtualGrid(small, 100, 100, Table(), st).error, "");
    EXPECT_EQ(orig.size(), img.size());
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
namespace {

std::string Config(const std::string& extra = "") {
    return std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + kTablePath +
           "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 1.0,"
           " \"vg_air_signal\": 60000, \"vg_iterations\": 5" + extra + "}";
}

std::string Without(std::string cfg, const std::string& key) {
    const size_t p = cfg.find("\"" + key + "\"");
    EXPECT_NE(p, std::string::npos) << key;
    const size_t e = cfg.find(',', p);
    cfg.erase(p, e - p + 1);
    return cfg;
}

std::vector<uint16_t> ToU16(const std::vector<double>& v) {
    std::vector<uint16_t> o(v.size());
    for (size_t i = 0; i < v.size(); ++i) o[i] = static_cast<uint16_t>(std::clamp(std::round(v[i]), 0.0, 65535.0));
    return o;
}

}  // namespace

TEST(GsvgVirtualGridApi, InitNeedsEveryValue)
{
    void* h = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&h, Config().c_str()), XPE_OK);   // control
    xpe_gsvg_shutdown(h);
    for (const char* key : {"vg_table_path", "vg_kvp", "vg_grid_ratio", "vg_pixel_pitch_mm",
                            "vg_air_signal"}) {
        h = nullptr;
        EXPECT_EQ(xpe_gsvg_init(&h, Without(Config(), key).c_str()), XPE_ERR_CONFIG_INVALID) << key;
        EXPECT_EQ(h, nullptr);
    }
    const std::string noIter = std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + kTablePath +
        "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 1.0, \"vg_air_signal\": 60000}";
    EXPECT_EQ(xpe_gsvg_init(&h, noIter.c_str()), XPE_ERR_CONFIG_INVALID);

    std::string missingFile = Config();
    missingFile.replace(missingFile.find(kTablePath), std::string(kTablePath).size(), "tests/data/no_such_table.csv");
    EXPECT_EQ(xpe_gsvg_init(&h, missingFile.c_str()), XPE_ERR_CONFIG_INVALID);

    std::string both = Config(", \"grid_suppression\": true");
    EXPECT_EQ(xpe_gsvg_init(&h, both.c_str()), XPE_ERR_CONFIG_INVALID);

    EXPECT_EQ(xpe_gsvg_init(&h, Config(", \"vg_iterations2\": 1").c_str()), XPE_OK);   // unknown key: warning only
    xpe_gsvg_shutdown(h);
}

TEST(GsvgVirtualGridApi, ProcessesAndKeepsTheOriginalOnFailure)
{
    const Scene s = MakeScene(Shape::Step);
    const std::vector<uint16_t> src = ToU16(s.measured);

    void* h = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&h, Config().c_str()), XPE_OK);
    std::vector<uint16_t> dst(src.size(), 0);
    ASSERT_EQ(xpe_gsvg_process(h, src.data(), src.size(), dst.data(), dst.size(), kN, kN, nullptr, 0), XPE_OK);
    size_t changed = 0;
    for (size_t i = 0; i < src.size(); ++i) changed += dst[i] != src[i];
    EXPECT_GT(changed, src.size() / 2);
    xpe_gsvg_shutdown(h);

    // kVp outside the table: configuration error, dst = src.
    std::string cfg = Config();
    cfg.replace(cfg.find("\"vg_kvp\": 80"), 12, "\"vg_kvp\": 140");
    ASSERT_EQ(xpe_gsvg_init(&h, cfg.c_str()), XPE_OK);
    std::fill(dst.begin(), dst.end(), 0);
    EXPECT_EQ(xpe_gsvg_process(h, src.data(), src.size(), dst.data(), dst.size(), kN, kN, nullptr, 0),
              XPE_ERR_CONFIG_INVALID);
    EXPECT_EQ(dst, src);
    xpe_gsvg_shutdown(h);

    // Thickness beyond the table everywhere (very dark image): processed, with a
    // warning that names the limited share (QA-B-93).
    ASSERT_EQ(xpe_gsvg_init(&h, Config().c_str()), XPE_OK);
    xpe_clear_alerts();
    std::vector<uint16_t> dark(src.size(), 5);
    EXPECT_EQ(xpe_gsvg_process(h, dark.data(), dark.size(), dark.data(), dark.size(), kN, kN, nullptr, 0),
              XPE_OK);
    bool warned = false;
    for (int i = 0; i < 32; ++i) {
        char msg[512];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) break;
        if (sev == XPE_ALERT_WARNING && std::string(msg).find("100.00%") != std::string::npos) warned = true;
    }
    EXPECT_TRUE(warned);
    xpe_clear_alerts();
    xpe_gsvg_shutdown(h);

    // Refusal with the vignette step on and dst aliasing src: still the original.
    ASSERT_EQ(xpe_gsvg_init(&h, (cfg.substr(0, cfg.size() - 1) + ", \"vignette_correction\": true}").c_str()), XPE_OK);
    std::vector<uint16_t> inPlace = src;
    const std::vector<float> gain(src.size(), 2.0f);
    EXPECT_EQ(xpe_gsvg_process(h, inPlace.data(), inPlace.size(), inPlace.data(), inPlace.size(), kN, kN,
                               gain.data(), gain.size()), XPE_ERR_CONFIG_INVALID);
    EXPECT_EQ(inPlace, src);
    xpe_gsvg_shutdown(h);
}

// ---------------------------------------------------------------------------
// REQ-GSVG-019: time only. XPE_VG_TABLE may point at another table (e.g. one
// built around the tools/mcsim kernels) for a local measurement.
// ---------------------------------------------------------------------------
TEST(GsvgVirtualGridBench, BenchmarkFreeze_Performance_REQ_GSVG_019_VirtualGrid3072)
{
    const int n = 3072;
    std::string table = kTablePath;
#ifdef _WIN32
    char* env = nullptr;
    size_t envLen = 0;
    if (_dupenv_s(&env, &envLen, "XPE_VG_TABLE") == 0 && env != nullptr) table = env;
    std::free(env);
#else
    if (const char* env = std::getenv("XPE_VG_TABLE")) table = env;
#endif
    const std::string cfg = std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + table +
        "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 0.139,"
        " \"vg_air_signal\": 60000, \"vg_iterations\": 3, \"vg_pyramid_levels\": 6,"
        " \"vg_pyramid_gain\": 1.3, \"vg_denoise_k\": 2}";
    void* h = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&h, cfg.c_str()), XPE_OK) << table;

    // Input built with the model itself (scatter computed on a 36x reduced
    // grid, repeated back up), so the chain runs its normal path: an input
    // whose scatter disagrees with the table can drive the thickness out of
    // the table and be refused.
    vg::ParamTable t;
    ASSERT_EQ(vg::LoadParamTable(table, t), "");
    size_t wi = 0;
    while (wi + 1 < t.wetKvp.size() && t.wetKvp[wi] < 80.0) ++wi;
    const double w0 = t.wetW0[wi], a = t.wetA[wi], b = t.wetB[wi];
    const int f = 36, c = n / f;
    std::vector<double> pc(static_cast<size_t>(c) * c), tc(pc.size());
    for (int y = 0; y < c; ++y)
        for (int x = 0; x < c; ++x) {
            const double T = 6.0 + 14.0 * x / (c - 1.0) + ((x / 7 + y / 7) % 2 ? 2.0 : 0.0);
            tc[static_cast<size_t>(y) * c + x] = T;
            pc[static_cast<size_t>(y) * c + x] = 60000.0 * std::exp(-(w0 - a * T / (1 + b * T)) * T);
        }
    const std::vector<double> ic = vg::ForwardScatter(pc, tc, c, c, t, 80.0, 0.139 * f);
    ASSERT_EQ(ic.size(), pc.size());
    std::vector<uint16_t> src(static_cast<size_t>(n) * n), dst(src.size());
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            src[static_cast<size_t>(y) * n + x] = static_cast<uint16_t>(std::min(
                65535.0, ic[static_cast<size_t>(std::min(y / f, c - 1)) * c + std::min(x / f, c - 1)]));
    perf_measure::Measure("REQ-GSVG-019/virtual_grid", "3072x3072",
        [&] { std::fill(dst.begin(), dst.end(), 0); },
        [&] { return xpe_gsvg_process(h, src.data(), src.size(), dst.data(), dst.size(), n, n, nullptr, 0); });
    xpe_gsvg_shutdown(h);
}

// ---------------------------------------------------------------------------
// QA-B-94: which over-correction guard. Every candidate on every scene, with
// the algorithm's kernels scaled x1 (right), x2 / x3 (over-estimated) and
// x0.5 (under-estimated) against data made with the x1 kernels. The table has
// no [spr_cap] section, so C1 is the local sum(a_i).
//
// Same forward-model code as the chain: a comparison inside the model, to be
// repeated on the MC images (QA-A-98).
// ---------------------------------------------------------------------------
TEST(GsvgVirtualGridMinFilter, MatchesBruteForce)
{
    const int w = 37, h = 23;
    std::vector<double> img(w * h);
    uint32_t state = 7;
    for (double& v : img) { state = state * 1664525u + 1013904223u; v = (state >> 8) % 1000; }
    for (int r : {0, 1, 3, 5, 30}) {
        const auto got = vg::MinFilter2D(img, w, h, r);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                double m = 1e300;
                for (int yy = std::max(0, y - r); yy <= std::min(h - 1, y + r); ++yy)
                    for (int xx = std::max(0, x - r); xx <= std::min(w - 1, x + r); ++xx)
                        m = std::min(m, img[yy * w + xx]);
                ASSERT_EQ(got[y * w + x], m) << "r=" << r << " x=" << x << " y=" << y;
            }
    }
}

namespace {

struct Cand { std::string name; vg::CapMode mode; double eps; };

const std::vector<Cand>& Candidates() {
    static const std::vector<Cand> c = {
        {"C0", vg::CapMode::None, 0},
        {"C1", vg::CapMode::LocalSum, 0},
        {"C2", vg::CapMode::GlobalSum, 0},
        {"C3e0.05", vg::CapMode::PrimaryFloor, 0.05},
        {"C3e0.10", vg::CapMode::PrimaryFloor, 0.10},
        {"C3e0.20", vg::CapMode::PrimaryFloor, 0.20},
        {"C3e0.30", vg::CapMode::PrimaryFloor, 0.30},
        {"C4e0.02", vg::CapMode::SmoothFloor, 0.02},
        {"C4e0.05", vg::CapMode::SmoothFloor, 0.05},
        {"C4e0.10", vg::CapMode::SmoothFloor, 0.10},
        {"C4e0.20", vg::CapMode::SmoothFloor, 0.20},
    };
    return c;
}

struct CapResult {
    bool refused = false;
    double median = 0, p95 = 0, max = 0;   // |out - P| / P
    double capped = 0;                      // reduced-grid share
    double negative = 0;                    // share of I - S < 0
    double nearZero = 0;                    // share of out < 0.1 P
    double maxOver = 0;                     // max (P - out) / P, 1 = zero
};

CapResult RunCandidate(const Scene& s, const vg::ParamTable& t, const Cand& c) {
    std::vector<double> img = s.measured;
    vg::VgSwitches sw;
    sw.cap = c.mode;
    sw.capEps = c.eps;
    const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, t, Settings(5), sw);
    CapResult r;
    if (!rep.error.empty()) { r.refused = true; return r; }
    const ErrStats e = RelErr(img, s.primary);
    r.median = e.median; r.p95 = e.p95; r.max = e.max;
    r.capped = rep.cappedFraction;
    r.negative = double(rep.negativePrimary) / img.size();
    size_t nz = 0;
    for (size_t i = 0; i < img.size(); ++i) {
        if (img[i] < 0.1 * s.primary[i]) ++nz;
        r.maxOver = std::max(r.maxOver, (s.primary[i] - img[i]) / s.primary[i]);
    }
    r.nearZero = double(nz) / img.size();
    return r;
}

vg::ParamTable Scaled(const vg::ParamTable& t, double k) {
    vg::ParamTable o = t;
    for (auto& n : o.kernels)
        for (int i = 0; i < n.terms; ++i) n.a[i] *= k;
    return o;
}

}  // namespace

TEST(GsvgVirtualGridCapChoice, CompareCandidates)
{
    const vg::ParamTable base = TableWithoutCapSection();
    std::map<std::string, CapResult> res;   // scene|factor|candidate
    const std::vector<std::pair<const char*, Shape>> scenes = {
        {"step", Shape::Step}, {"gradient", Shape::Gradient}, {"thick+air", Shape::ThickNextToAir},
        {"thin+thick", Shape::ThinNextToThick}};
    std::printf("VGCAP scene,factor,cand,refused,median,p95,max,capped,negative,nearZero,maxOver\n");
    for (const auto& [sname, shape] : scenes) {
        const Scene s = MakeScene(shape, base);
        double trueMax = 0;
        for (size_t i = 0; i < s.primary.size(); ++i)
            trueMax = std::max(trueMax, (s.measured[i] - s.primary[i]) / s.primary[i]);
        std::printf("VGCAPSCENE %s trueMaxSpr=%.3f globalSum=%.3f\n", sname, trueMax,
                    vg::SprCapAt(base, 30.0, kKvp));
        for (double k : {1.0, 2.0, 3.0, 0.5}) {
            const vg::ParamTable t = Scaled(base, k);
            for (const Cand& c : Candidates()) {
                const CapResult r = RunCandidate(s, t, c);
                char key[64];
                std::snprintf(key, sizeof(key), "%s|%.1f|%s", sname, k, c.name.c_str());
                res[key] = r;
                std::printf("VGCAP %s,%.1f,%s,%d,%.5f,%.5f,%.5f,%.4f,%.5f,%.5f,%.4f\n", sname, k, c.name.c_str(),
                            r.refused ? 1 : 0, r.median, r.p95, r.max, r.capped, r.negative, r.nearZero, r.maxOver);
            }
        }
    }

    // The choice (C2, the default) against the two conditions, per scene.
    auto at = [&](const char* sc, double k, const char* c) {
        char key[64];
        std::snprintf(key, sizeof(key), "%s|%.1f|%s", sc, k, c);
        return res.at(key);
    };
    for (const auto& [sname, shape] : scenes) {
        (void)shape;
        SCOPED_TRACE(sname);
        for (double k : {1.0, 0.5}) {   // correct and under-estimated: C2 changes nothing
            const CapResult c0 = at(sname, k, "C0"), c2 = at(sname, k, "C2");
            EXPECT_FALSE(c2.refused);
            EXPECT_NEAR(c2.median, c0.median, 1e-4) << k;
            EXPECT_NEAR(c2.max, c0.max, 1e-3) << k;
        }
        for (double k : {2.0, 3.0}) {   // over-estimated: no negative or near-zero primary
            const CapResult c0 = at(sname, k, "C0"), c2 = at(sname, k, "C2");
            EXPECT_EQ(c2.negative, 0.0) << k;
            EXPECT_EQ(c2.nearZero, 0.0) << k;
            EXPECT_LT(c2.maxOver, 0.9) << k;
            EXPECT_LE(c2.maxOver, c0.maxOver) << k;
        }
    }
    // Falsification: without the guard (C0), x3 kernels drive the primary to
    // zero on at least three of the four scenes (gradient: 0.92).
    int zeroScenes = 0;
    for (const auto& [sname, shape] : scenes) { (void)shape; zeroScenes += at(sname, 3.0, "C0").maxOver >= 1.0; }
    EXPECT_GE(zeroScenes, 3);
    // Why not C1: it binds on the under-estimated table.
    EXPECT_GT(at("step", 0.5, "C1").capped, 0.0);
}
