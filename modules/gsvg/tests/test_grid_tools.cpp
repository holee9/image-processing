// #180 (QA-B-89): checks for the test-side grid tools in grid_test_tools.h.
//
// Each tool is run on inputs whose answer is known before the run, and each
// metric is paired with a falsification case showing it can miss when it is
// misused -- so a later "residual energy is low" or "MTF is preserved" means
// something. No product code is exercised here; the tools are for the gsvg
// implementation that #180 is still designing.
//
// Size: 1024x1024 for the grid cases (the profile DFT is O(N^2) per image),
// 200x64 for the edge cases.

#include <gtest/gtest.h>

#include "grid_test_tools.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace gsvg_test;

namespace {

constexpr int    kN = 1024;
constexpr double kPitch = 0.139;
constexpr unsigned kSeed = 180u;
constexpr double kNoise = 30.0;

const Image& Background() {
    static const Image bg = MakeAnatomyBackground(kN, kN, kSeed, kNoise);
    return bg;
}

double BinWidthPerMm() { return 1.0 / (kN * kPitch); }

}  // namespace

// ---------------------------------------------------------------------------
// (1) generator
// ---------------------------------------------------------------------------

// Values worked by hand, not by the helper: fs = 1/0.139 = 7.194245 c/mm.
//   60/25.4  = 2.362205 (below fs/2 = 3.597122, unchanged)
//   103/25.4 = 4.055118 -> |4.055118 - 7.194245| = 3.139127
//   200/25.4 = 7.874016 -> |7.874016 - 7.194245| = 0.679771
TEST(GsvgGridTools, AliasedFrequencyMatchesHandComputedValues) {
    EXPECT_NEAR(AliasedFrequencyPerMm(60.0, kPitch), 2.362205, 1e-5);
    EXPECT_NEAR(AliasedFrequencyPerMm(103.0, kPitch), 3.139127, 1e-5);
    EXPECT_NEAR(AliasedFrequencyPerMm(200.0, kPitch), 0.679771, 1e-5);
}

TEST(GsvgGridTools, BackgroundIsNotUniform) {
    const Image& bg = Background();
    const auto [mn, mx] = std::minmax_element(bg.px.begin(), bg.px.end());
    EXPECT_GT(*mx - *mn, 8000.0);
    // the sharp oblique edge: a jump of ~5000 between neighbours somewhere on row 512
    double maxStep = 0.0;
    for (int x = 0; x + 1 < kN; ++x)
        maxStep = std::max(maxStep, std::fabs(Background().at(x + 1, 512) - Background().at(x, 512)));
    EXPECT_GT(maxStep, 4000.0);
}

// The generator's output frequency, read back by a spectrum of the image itself.
TEST(GsvgGridTools, GeneratedGridAppearsAtTheAliasedFrequency) {
    for (GridAxis axis : {GridAxis::Rows, GridAxis::Columns}) {
        for (double lpi : {60.0, 103.0, 200.0}) {
            GridSpec g;
            g.linesPerInch = lpi; g.pitchMm = kPitch; g.axis = axis; g.depth = 0.05;
            const Image img = ApplyGrid(Background(), g);
            const double measured = PeakFrequencyPerMm(img, axis, kPitch);
            const double expected = AliasedFrequencyPerMm(lpi, kPitch);
            std::printf("GRIDTOOL peak axis=%s lpi=%.0f expected=%.5f measured=%.5f diff_bins=%.3f\n",
                        axis == GridAxis::Rows ? "rows" : "cols", lpi, expected, measured,
                        (measured - expected) / BinWidthPerMm());
            EXPECT_NEAR(measured, expected, 0.1 * BinWidthPerMm()) << "lpi " << lpi;
            // control: the un-aliased frequency is not what the image contains
            if (lpi > 0.5 * 25.4 / kPitch)
                EXPECT_GT(std::fabs(measured - GridFrequencyPerMm(lpi)), 10.0 * BinWidthPerMm());
        }
    }
}

// ---------------------------------------------------------------------------
// (2) residual grid energy
// ---------------------------------------------------------------------------

TEST(GsvgGridTools, ResidualEnergyRisesWithDepthAndReturnsToBaselineWhenRemoved) {
    for (double lpi : {60.0, 103.0, 200.0}) {
        const double f = AliasedFrequencyPerMm(lpi, kPitch);
        const GridEnergy base = ResidualGridEnergy(Background(), GridAxis::Rows, f, kPitch);
        double prev = base.ratio;
        for (double d : {0.0005, 0.001, 0.005, 0.02, 0.05, 0.1, 0.2}) {
            GridSpec g;
            g.linesPerInch = lpi; g.pitchMm = kPitch; g.axis = GridAxis::Rows; g.depth = d;
            const Image img = ApplyGrid(Background(), g);
            const GridEnergy e = ResidualGridEnergy(img, GridAxis::Rows, f, kPitch);
            std::printf("GRIDTOOL energy lpi=%.0f d=%.3f ratio=%.4g (baseline %.4g)\n", lpi, d, e.ratio, base.ratio);
            EXPECT_GT(e.ratio, prev) << "lpi " << lpi << " d " << d;
            prev = e.ratio;

            const GridEnergy fixed = ResidualGridEnergy(RemoveGridExactly(img, g), GridAxis::Rows, f, kPitch);
            EXPECT_NEAR(fixed.ratio, base.ratio, 1e-6 * std::max(1.0, base.ratio)) << "lpi " << lpi << " d " << d;
        }
    }
}

// Falsification: a band placed away from the grid does not see the grid.
// Not exactly zero: the grid multiplies the background, so background content
// is copied to +-f_grid and a little of it lands in any band. Measured at d=0.1
// that moves the wrong-band ratio by about 10 % (QA-B-89 run), against a
// right-band rise of ~1e7. The bound below is 25 %.
TEST(GsvgGridTools, ResidualEnergyAtAWrongBandIgnoresTheGrid) {
    for (double lpi : {60.0, 103.0, 200.0}) {
        const double f = AliasedFrequencyPerMm(lpi, kPitch);
        const double wrong = f * 0.6;
        GridSpec g;
        g.linesPerInch = lpi; g.pitchMm = kPitch; g.axis = GridAxis::Rows; g.depth = 0.1;
        const Image img = ApplyGrid(Background(), g);
        const double w0 = ResidualGridEnergy(Background(), GridAxis::Rows, wrong, kPitch).ratio;
        const double w1 = ResidualGridEnergy(img, GridAxis::Rows, wrong, kPitch).ratio;
        const double r0 = ResidualGridEnergy(Background(), GridAxis::Rows, f, kPitch).ratio;
        const double r1 = ResidualGridEnergy(img, GridAxis::Rows, f, kPitch).ratio;
        std::printf("GRIDTOOL wrongband lpi=%.0f wrong: %.4g -> %.4g  right: %.4g -> %.4g\n", lpi, w0, w1, r0, r1);
        EXPECT_LT(std::fabs(w1 - w0) / w0, 0.25) << "lpi " << lpi;
        EXPECT_GT(r1 / r0, 1e6) << "lpi " << lpi;
    }
}

// Falsification: measuring along the wrong axis does not see the grid either.
TEST(GsvgGridTools, ResidualEnergyOnTheOtherAxisIgnoresTheGrid) {
    const double f = AliasedFrequencyPerMm(103.0, kPitch);
    GridSpec g;
    g.axis = GridAxis::Rows; g.depth = 0.1;
    const Image img = ApplyGrid(Background(), g);
    const double c0 = ResidualGridEnergy(Background(), GridAxis::Columns, f, kPitch).ratio;
    const double c1 = ResidualGridEnergy(img, GridAxis::Columns, f, kPitch).ratio;
    std::printf("GRIDTOOL otheraxis cols: %.4g -> %.4g\n", c0, c1);
    EXPECT_LT(std::fabs(c1 - c0) / c0, 0.25);
}

// ---------------------------------------------------------------------------
// (3) slanted-edge MTF
// ---------------------------------------------------------------------------

namespace {
constexpr int kEdgeW = 64, kEdgeH = 200;
constexpr double kMtfTol = 0.01;   // set from the measured maxima printed below

double MaxDeviationFromGaussian(const MtfCurve& c, double sigma, double fLimit = 0.5) {
    double worst = 0.0;
    for (size_t i = 0; i < c.freq.size(); ++i)
        if (c.freq[i] <= fLimit + 1e-12)
            worst = std::max(worst, std::fabs(c.mtf[i] - GaussianMtf(sigma, c.freq[i])));
    return worst;
}
}  // namespace

TEST(GsvgGridTools, EdgeFitRecoversTheTilt) {
    for (double deg : {2.0, 3.0, 5.0}) {
        const Image img = MakeSlantedEdge(kEdgeW, kEdgeH, deg, 1.0);
        const EdgeLine e = FitEdge(img);
        const double got = std::atan(e.slope) * 180.0 / kPi;
        std::printf("GRIDTOOL tilt set=%.1f fitted=%.4f\n", deg, got);
        EXPECT_NEAR(got, deg, 0.02);
    }
}

TEST(GsvgGridTools, EdgeMtfMatchesTheGaussianBlurFormula) {
    for (double deg : {2.0, 3.0, 5.0}) {
        for (double sigma : {0.5, 1.0, 1.5, 2.0}) {
            const MtfCurve c = SlantedEdgeMtf(MakeSlantedEdge(kEdgeW, kEdgeH, deg, sigma), TiltCorrection::On);
            const double dev = MaxDeviationFromGaussian(c, sigma);
            std::printf("GRIDTOOL mtf tilt=%.1f sigma=%.1f maxdev(f<=0.5)=%.5f\n", deg, sigma, dev);
            EXPECT_LE(dev, kMtfTol) << "tilt " << deg << " sigma " << sigma;
        }
    }
}

TEST(GsvgGridTools, EdgeMtf50FallsAsBlurGrows) {
    double prev = 1e9;
    for (double sigma : {0.0, 0.5, 1.0, 1.5, 2.0}) {
        const double m50 = Mtf50(SlantedEdgeMtf(MakeSlantedEdge(kEdgeW, kEdgeH, 3.0, sigma), TiltCorrection::On));
        const double analytic = sigma > 0.0 ? std::sqrt(std::log(2.0) / (2.0 * kPi * kPi)) / sigma : -1.0;
        std::printf("GRIDTOOL mtf50 sigma=%.1f measured=%.4f analytic=%.4f\n", sigma, m50, analytic);
        EXPECT_LT(m50, prev) << "sigma " << sigma;
        if (sigma > 0.0) EXPECT_NEAR(m50, analytic, 0.01) << "sigma " << sigma;
        prev = m50;
    }
}

// Falsification: without projecting onto the edge normal, a tilted edge smears
// the ESF and the MTF no longer matches the formula.
TEST(GsvgGridTools, EdgeMtfWithoutTiltCorrectionMissesTheFormula) {
    for (double deg : {2.0, 3.0, 5.0}) {
        const Image img = MakeSlantedEdge(kEdgeW, kEdgeH, deg, 1.0);
        const double on = MaxDeviationFromGaussian(SlantedEdgeMtf(img, TiltCorrection::On), 1.0);
        const double off = MaxDeviationFromGaussian(SlantedEdgeMtf(img, TiltCorrection::Off), 1.0);
        std::printf("GRIDTOOL notilt tilt=%.1f maxdev on=%.5f off=%.5f\n", deg, on, off);
        EXPECT_GT(off, 0.2) << "tilt " << deg;
        EXPECT_LE(on, kMtfTol) << "tilt " << deg;
    }
}
