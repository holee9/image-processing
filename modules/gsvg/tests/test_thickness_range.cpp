// REQ-GSVG-017 (#180, QA-B-116): the output across the required 10..30 cm.
//
// THIS IS NOT AN ACCURACY TEST. The scene is synthetic and its scatter is built
// with the SAME kernels the chain then uses to remove it, so the comparison is
// self-referential: a model that is consistently wrong passes. What the floors
// below catch is a REGRESSION — today's numbers getting worse. Accuracy is
// judged against the MC phantoms, where the answer comes from other code and
// the grid row is ideal (tp 1, ts 0), so the truth does not contain our table
// (QA-B-116 lead decision).
//
// The target is not the primary: the product table carries real grids
// (ratio 6..12, Ts/Tp about 0.1), so a correct chain returns
//     target = P + (Ts/Tp)(t) * S_true,   S_true = measured - P
// Comparing with P instead reads the residual scatter the grid is supposed to
// let through as an error (QA-B-115: it looked like 101 % at 30 cm).
//
// 실제 장비 영상 확보 시 재설정 (#151).
//
// Bound to the SYNTHETIC scene this file builds, not to the MC phantoms, so
// replacing those does not touch these floors (QA-B-118). The median is a
// robust statistic and p95 is only mildly extreme; the scene carries no noise
// and the values do not move between runs.
#include <gtest/gtest.h>

#include "virtual_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace vg = xpe_gsvg_detail;

namespace {

// 512 px at 0.8 mm is 41 cm of detector, so the scatter kernels (several cm
// wide) fit inside the image. The QA-B-115 survey used 1024 px at 0.4 mm --
// the same field, four times the pixels; the floors here were measured in
// THIS configuration.
constexpr int    kN = 512;
constexpr double kPitchMm = 0.8;
constexpr double kKvp = 80.0;
constexpr double kI0 = 60000.0;
constexpr double kRatio = 10.0;
constexpr double kFreqPerCm = 40.0;

const vg::ParamTable& ProductTable() {
    static vg::ParamTable t;
    static const std::string err = vg::LoadParamTable("data/vg_table_water_csi600_victre.csv", t);
    EXPECT_EQ(err, "");
    return t;
}

void WetAt80(double& w0, double& a, double& b) {
    const vg::ParamTable& t = ProductTable();
    size_t i = 0;
    while (i + 1 < t.wetKvp.size() && t.wetKvp[i] < kKvp) ++i;
    w0 = t.wetW0[i]; a = t.wetA[i]; b = t.wetB[i];
}

struct Scene { std::vector<double> primary, thickness, measured; };

// A scene is never a single uniform slab (#148). The details are THINNER than
// the base so no pixel leaves the table at the 30 cm row.
Scene MakeAt(double baseCm) {
    double w0 = 0, a = 0, b = 0;
    WetAt80(w0, a, b);
    Scene s;
    s.primary.assign(static_cast<size_t>(kN) * kN, 0.0);
    s.thickness.assign(s.primary.size(), 0.0);
    for (int y = 0; y < kN; ++y)
        for (int x = 0; x < kN; ++x) {
            double T = baseCm;
            const double dx = (x % 96) - 48.0, dy = (y % 96) - 48.0;
            if (dx * dx + dy * dy < 225.0) T -= 2.0;
            if (y > 200 && y < 208 && x > 40 && x < kN - 40) T -= 1.0;
            T = std::max(T, 1.0);
            const double mu = w0 - a * T / (1 + b * T);
            s.primary[static_cast<size_t>(y) * kN + x] = kI0 * std::exp(-mu * T);
        }
    for (size_t i = 0; i < s.primary.size(); ++i) {
        const double tt = vg::ThicknessFromLogAtten(-std::log(s.primary[i] / kI0), w0, a, b, 30.0);
        s.thickness[i] = tt < 0 ? 30.0 : tt;
    }
    s.measured = vg::ForwardScatter(s.primary, s.thickness, kN, kN, ProductTable(), kKvp, kPitchMm);
    return s;
}

}  // namespace

TEST(GsvgVirtualGridRange, ProvisionalFloor_RecoveryAcrossTheRequiredThickness_REQ_GSVG_017)
{
    // Floors: the value measured in this configuration x 1.5 (lead decision,
    // QA-B-116). The margin is not derived from a spread -- the scene is
    // deterministic -- it is headroom, and the numbers are a regression line,
    // not a quality claim.
    // measured here: 0.0021 0.0038 0.0059 0.0076 0.0091 (QA-B-116). The
    // QA-B-115 survey at 1024 px / 0.4 mm read 0.0018 0.0035 0.0056 0.0074
    // 0.0091 -- the same field at four times the pixels, so the two agree to
    // about 3 parts in 10000. The floors are this configuration's numbers.
    struct Case { double cm; double medianFloor; };
    const Case cases[] = {
        {10.0, 0.0032},
        {15.0, 0.0057},
        {20.0, 0.0089},
        {25.0, 0.0114},
        {30.0, 0.0137},
    };
    // p95 ceiling: the worst case (30 cm, 0.0468) x 1.5, applied to every row.
    constexpr double kP95Floor = 0.0702;

    for (const Case& c : cases) {
        const Scene s = MakeAt(c.cm);
        std::vector<double> img = s.measured;
        vg::VgSettings st;
        st.kvp = kKvp;
        st.gridRatio = kRatio;
        st.gridFreqPerCm = kFreqPerCm;
        st.pixelPitchMm = kPitchMm;
        st.airSignal = kI0;
        st.iterations = 5;
        // Post-steps off: the pyramid changes values for reasons that have
        // nothing to do with thickness (QA-B-111).
        st.pyramidLevels = 0; st.pyramidGain = 1.0; st.denoiseK = 0.0;
        const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, ProductTable(), st, vg::VgSwitches{});
        ASSERT_EQ(rep.error, "") << c.cm;

        vg::GridAtKvp grid;
        ASSERT_EQ(vg::SelectGrid(ProductTable(), kRatio, kFreqPerCm, kKvp, grid), "");

        std::vector<double> e;
        for (int y = kN / 4; y < 3 * kN / 4; ++y)
            for (int x = kN / 4; x < 3 * kN / 4; ++x) {
                const size_t i = static_cast<size_t>(y) * kN + x;
                if (s.primary[i] <= 0) continue;
                const double target = s.primary[i] +
                    grid.ResidualAt(s.thickness[i]) * (s.measured[i] - s.primary[i]);
                e.push_back(std::fabs(img[i] - target) / target);
            }
        std::sort(e.begin(), e.end());
        const double median = e[e.size() / 2], p95 = e[e.size() * 95 / 100], worst = e.back();
        std::printf("VGRANGE t=%.0f cm median=%.4f p95=%.4f max=%.4f aboveTable=%.4f\n",
                    c.cm, median, p95, worst, rep.aboveTableFullRes);
        EXPECT_LT(median, c.medianFloor) << c.cm;
        EXPECT_LT(p95, kP95Floor) << c.cm;
        // Recorded, not a floor: at 30 cm the required range's top is the
        // table's top, so a third of the pixels read above it and are held at
        // the table maximum (QA-B-93 lead decision). Below 30 cm it is 0.
        if (c.cm < 30.0) EXPECT_EQ(rep.aboveTableFullRes, 0.0) << c.cm;
        else EXPECT_GT(rep.aboveTableFullRes, 0.0);
        (void)worst;
    }
}
