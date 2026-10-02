/**
 * @file test_gain_poly_policy.cpp
 * @brief The per-pixel degree policy of xpe_calib_generate_gain_polynomial (QA-A-210c, #233).
 *
 * THE DECISION (lead, from QA-A-210 and QA-A-210b): the generator tried the requested degree and lowered it until the
 * fitted curve was NON-DECREASING over the measured dose range, and a pixel for which no degree passed got a straight
 * line through its first and last measurement. SRS-CALIB-FUNC-027 says "monotone", not "increasing"; and on real
 * calibration data (cyan_test, 3072x3072, five dose levels) 48.9% of the pixels went to that line -- every one of them
 * with a NEGATIVE least-squares slope of the size of the level-to-level scatter -- and the pooled R^2 came out at
 * -0.035. The policy is now:
 *   - the curve of a degree is accepted when it is monotone in EITHER direction (non-decreasing or non-increasing);
 *   - the degrees are tried from the requested one down to 1, and degree 1 -- a straight line is monotone whichever way
 *     it points -- is the LEAST-SQUARES line, so no pixel is ever stored with a worse-than-its-own-mean model;
 *   - the endpoint line is gone.
 * What these tests hold:
 *   1. per-pixel R^2 >= 0 and pooled R^2 >= 0 on real data and on noise-only data (least squares scored on its own data)
 *   2. every stored polynomial is monotone over the measured dose range, in either direction
 *   3. the choice of degree equals an INDEPENDENT least-squares-and-monotone cascade written in this file (long double,
 *      Gauss-Jordan), pixel by pixel -- so an accepted decreasing curve, a rejected non-monotone one, and the line are all
 *      pinned by what they must be, not by what the product happens to print
 *   3b. the pixels the old policy already fitted at degree >= 2 are stored BIT FOR BIT as before
 *   4. the synthetic ladder of QA-A-210 (fixture generator, seed 7, 32x32) no longer reports a negative R^2
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "preprocess_state_fixture.h"
#include "xpe_fixture_gen_path.h"  // generated: XPE_FIXTURE_GEN_EXE
#include "gain_poly_real_crop.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

/** One generated polynomial file, read back. */
struct Fit {
    uint32_t width = 0, height = 0;
    int maxDegree = 0;
    std::vector<double> coeffs;       // pixel-major: [pixel * (maxDegree + 1) + j]
    double fileR2 = 0.0;
    int fileHighestDegree = -1;

    size_t pixels() const { return static_cast<size_t>(width) * height; }
    double c(size_t pix, int j) const { return coeffs[pix * static_cast<size_t>(maxDegree + 1) + static_cast<size_t>(j)]; }
    uint32_t bits(size_t pix, int j) const {
        const float f = static_cast<float>(c(pix, j));
        uint32_t u;
        std::memcpy(&u, &f, sizeof(u));
        return u;
    }
    /** The stored degree of a pixel: its highest non-zero coefficient. */
    int degree(size_t pix) const {
        for (int j = maxDegree; j >= 1; --j) {
            if (c(pix, j) != 0.0) return j;
        }
        return 0;
    }
    double eval(size_t pix, double x) const {
        double y = 0.0;
        for (int j = maxDegree; j >= 0; --j) y = y * x + c(pix, j);
        return y;
    }
};

/** @return the number following `"key":` in a flat JSON text; NaN when absent. */
double jsonNumber(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const size_t at = json.find(needle);
    if (at == std::string::npos) return std::nan("");
    return std::strtod(json.c_str() + at + needle.size(), nullptr);
}

Fit readPoly(const fs::path& path, int maxDegree) {
    std::ifstream f(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Fit fit;
    fit.maxDegree = maxDegree;
    if (bytes.size() < sizeof(XCalFileHeader)) return fit;
    XCalFileHeader hdr{};
    std::memcpy(&hdr, bytes.data(), sizeof(hdr));
    fit.width = hdr.width;
    fit.height = hdr.height;
    const std::string cfg = bytes.substr(sizeof(hdr), static_cast<size_t>(hdr.config_json_len));
    fit.fileR2 = jsonNumber(cfg, "fit_r_squared");
    fit.fileHighestDegree = static_cast<int>(jsonNumber(cfg, "polynomial_degree"));
    const size_t n = static_cast<size_t>(hdr.width) * hdr.height * static_cast<size_t>(maxDegree + 1);
    const size_t at = sizeof(hdr) + static_cast<size_t>(hdr.config_json_len);
    fit.coeffs.resize(n);
    for (size_t i = 0; i < n; ++i) {
        float v;
        std::memcpy(&v, bytes.data() + at + i * sizeof(float), sizeof(float));
        fit.coeffs[i] = static_cast<double>(v);
    }
    return fit;
}

/** A least-squares polynomial of `degree` through (x, y), long double normal equations with partial pivoting. */
bool lsFit(const std::vector<double>& x, const std::vector<double>& y, int degree, std::vector<long double>* c) {
    const int m = degree + 1;
    std::vector<std::vector<long double>> a(static_cast<size_t>(m), std::vector<long double>(static_cast<size_t>(m + 1), 0.0L));
    for (size_t i = 0; i < x.size(); ++i) {
        for (int j = 0; j < m; ++j) {
            for (int k = 0; k < m; ++k) a[j][k] += powl(static_cast<long double>(x[i]), j + k);
            a[j][m] += powl(static_cast<long double>(x[i]), j) * static_cast<long double>(y[i]);
        }
    }
    for (int col = 0; col < m; ++col) {
        int piv = col;
        for (int r = col + 1; r < m; ++r) if (fabsl(a[r][col]) > fabsl(a[piv][col])) piv = r;
        if (fabsl(a[piv][col]) < 1e-30L) return false;
        std::swap(a[col], a[piv]);
        for (int r = 0; r < m; ++r) {
            if (r == col) continue;
            const long double f = a[r][col] / a[col][col];
            for (int k = col; k <= m; ++k) a[r][k] -= f * a[col][k];
        }
    }
    c->assign(static_cast<size_t>(m), 0.0L);
    for (int j = 0; j < m; ++j) (*c)[static_cast<size_t>(j)] = a[j][m] / a[j][j];
    return true;
}

/** Monotone (either direction) over 100 samples of [x0, x1], with a slack that absorbs the float32 rounding of stored coefficients. */
bool monotoneEitherWay(const std::vector<long double>& c, double x0, double x1, long double slack) {
    bool rising = true, falling = true;
    long double prev = 0.0L;
    for (int i = 0; i < 100; ++i) {
        const long double x = static_cast<long double>(x0) + (static_cast<long double>(x1) - x0) * i / 99.0L;
        long double y = 0.0L;
        for (size_t j = c.size(); j-- > 0;) y = y * x + c[j];
        if (i > 0) {
            if (y < prev - slack) rising = false;
            if (y > prev + slack) falling = false;
        }
        prev = y;
    }
    return rising || falling;
}

/** The degree this policy must pick for a pixel -- written independently of the product. */
int expectedDegree(const std::vector<double>& x, const std::vector<double>& y, int maxDegree) {
    for (int deg = maxDegree; deg >= 1; --deg) {
        std::vector<long double> c;
        if (!lsFit(x, y, deg, &c)) continue;
        if (deg == 1 || monotoneEitherWay(c, x.front(), x.back(), 0.0L)) return deg;
    }
    return -1;
}

class GainPolyPolicyTest : public XpePreprocessStateFixture {
protected:
    fs::path root;

    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        xpe_clear_alerts();
        root = fs::temp_directory_path() / ("xpe_gpp_" + std::to_string(std::rand()) + "_" +
                                            ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::create_directories(root);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root, ec);
        xpe_clear_alerts();
        XpePreprocessStateFixture::TearDown();
    }

    std::string writeLevel(const char* name, uint32_t w, uint32_t h, const float* values) {
        const std::string path = (root / name).string();
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = w;
        hdr.height = h;
        hdr.payload_len = static_cast<uint64_t>(w) * h * sizeof(float);
        EXPECT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(values), hdr.payload_len));
        return path;
    }

    /** Generates the polynomial of `levels` (one float map per dose) and reads it back. */
    Fit generate(const std::vector<std::vector<float>>& levels, uint32_t w, uint32_t h,
                 const std::vector<double>& doses, int maxDegree) {
        std::vector<std::string> paths;
        for (size_t l = 0; l < levels.size(); ++l) {
            paths.push_back(writeLevel(("lvl" + std::to_string(l) + ".xcal").c_str(), w, h, levels[l].data()));
        }
        std::vector<const char*> cpaths;
        for (const auto& p : paths) cpaths.push_back(p.c_str());
        const std::string out = (root / ("poly" + std::to_string(maxDegree) + ".xcal")).string();
        EXPECT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(cpaths.data(), doses.data(), static_cast<int32_t>(doses.size()),
                                                             maxDegree, out.c_str()));
        return readPoly(out, maxDegree);
    }

    static std::vector<std::vector<float>> realLevels() {
        std::vector<std::vector<float>> v;
        for (unsigned l = 0; l < gain_poly_real::kLevels; ++l) {
            v.emplace_back(gain_poly_real::kMaps[l], gain_poly_real::kMaps[l] + gain_poly_real::kCropSide * gain_poly_real::kCropSide);
        }
        return v;
    }
    static std::vector<double> realDoses() {
        return std::vector<double>(gain_poly_real::kDoses, gain_poly_real::kDoses + gain_poly_real::kLevels);
    }

    /** Noise-only gain maps: unit mean per level, a small deterministic scatter, no dose dependence (the shape of the real data). */
    static std::vector<std::vector<float>> noiseLevels(uint32_t w, uint32_t h, size_t levels) {
        std::vector<std::vector<float>> out(levels, std::vector<float>(static_cast<size_t>(w) * h));
        uint32_t s = 12345u;
        auto next = [&s]() { s = s * 1664525u + 1013904223u; return (static_cast<double>(s >> 8) / 16777216.0) - 0.5; };
        for (auto& lvl : out) {
            double sum = 0.0;
            for (auto& v : lvl) { v = static_cast<float>(1.0 + 0.006 * next()); sum += v; }
            const double mean = sum / static_cast<double>(lvl.size());
            for (auto& v : lvl) v = static_cast<float>(v / mean);
        }
        return out;
    }

    static double pixelR2(const Fit& fit, size_t pix, const std::vector<std::vector<float>>& levels, const std::vector<double>& doses) {
        double mean = 0.0;
        for (const auto& l : levels) mean += l[pix];
        mean /= static_cast<double>(levels.size());
        double ssTot = 0.0, ssRes = 0.0;
        for (size_t i = 0; i < levels.size(); ++i) {
            const double y = levels[i][pix];
            ssTot += (y - mean) * (y - mean);
            const double r = y - fit.eval(pix, doses[i]);
            ssRes += r * r;
        }
        return ssTot > 1e-18 ? 1.0 - ssRes / ssTot : 1.0;
    }

    std::vector<double> pixelSeries(const std::vector<std::vector<float>>& levels, size_t pix) const {
        std::vector<double> y;
        for (const auto& l : levels) y.push_back(l[pix]);
        return y;
    }
};

}  // namespace

/* ---------------------------------------------------------------------------
 * 1. A least-squares fit scored on its own data is never worse than the mean.
 * ------------------------------------------------------------------------- */

TEST_F(GainPolyPolicyTest, OnRealDataEveryPixelFitAndTheFileAreAtLeastAsGoodAsTheMean) {
    const auto levels = realLevels();
    const auto doses = realDoses();
    const uint32_t n = gain_poly_real::kCropSide;
    for (int maxDegree : {2, 3}) {
        SCOPED_TRACE("max degree " + std::to_string(maxDegree));
        const Fit fit = generate(levels, n, n, doses, maxDegree);
        ASSERT_EQ(static_cast<size_t>(n) * n, fit.pixels());
        size_t negative = 0;
        double worst = 1.0;
        for (size_t p = 0; p < fit.pixels(); ++p) {
            const double r2 = pixelR2(fit, p, levels, doses);
            worst = std::min(worst, r2);
            if (r2 < -1e-6) ++negative;
        }
        EXPECT_EQ(0u, negative) << "pixels scored below their own mean (worst R^2 " << worst << ")";
        EXPECT_GE(fit.fileR2, 0.0) << "the pooled R^2 the file records";
    }
}

TEST_F(GainPolyPolicyTest, OnNoiseOnlyDataTheFileRSquaredIsNotNegativeEither) {
    const auto levels = noiseLevels(32, 32, 4);
    const std::vector<double> doses{8000.0, 12000.0, 16000.0, 20000.0};
    const Fit fit = generate(levels, 32, 32, doses, 2);
    EXPECT_GE(fit.fileR2, 0.0);
    for (size_t p = 0; p < fit.pixels(); ++p) EXPECT_GE(pixelR2(fit, p, levels, doses), -1e-6) << "pixel " << p;
}

/* ---------------------------------------------------------------------------
 * 2. Every stored polynomial is monotone over the measured range, in either direction.
 * ------------------------------------------------------------------------- */

TEST_F(GainPolyPolicyTest, EveryStoredPolynomialIsMonotoneOverTheMeasuredRangeInEitherDirection) {
    struct Case { const char* name; std::vector<std::vector<float>> levels; std::vector<double> doses; uint32_t w, h; };
    const uint32_t n = gain_poly_real::kCropSide;
    const std::vector<Case> cases = {
        {"real crop", realLevels(), realDoses(), n, n},
        {"noise only", noiseLevels(32, 32, 4), {8000.0, 12000.0, 16000.0, 20000.0}, 32, 32},
    };
    for (const Case& cs : cases) {
        for (int maxDegree : {2, 3}) {
            SCOPED_TRACE(std::string(cs.name) + ", max degree " + std::to_string(maxDegree));
            const Fit fit = generate(cs.levels, cs.w, cs.h, cs.doses, maxDegree);
            size_t bad = 0;
            for (size_t p = 0; p < fit.pixels(); ++p) {
                std::vector<long double> c;
                for (int j = 0; j <= maxDegree; ++j) c.push_back(static_cast<long double>(fit.c(p, j)));
                if (!monotoneEitherWay(c, cs.doses.front(), cs.doses.back(), 2e-7L)) ++bad;
            }
            EXPECT_EQ(0u, bad) << "stored polynomials that are neither non-decreasing nor non-increasing";
        }
    }
}

/* ---------------------------------------------------------------------------
 * 3. The degree each pixel gets is what an independent least-squares + monotone cascade says.
 * ------------------------------------------------------------------------- */

TEST_F(GainPolyPolicyTest, TheDegreeOfEveryPixelIsThatOfAnIndependentLeastSquaresAndMonotoneCascade) {
    const auto levels = realLevels();
    const auto doses = realDoses();
    const uint32_t n = gain_poly_real::kCropSide;
    for (int maxDegree : {2, 3}) {
        SCOPED_TRACE("max degree " + std::to_string(maxDegree));
        const Fit fit = generate(levels, n, n, doses, maxDegree);
        size_t mismatches = 0, atMax = 0, linear = 0;
        for (size_t p = 0; p < fit.pixels(); ++p) {
            const int want = expectedDegree(doses, pixelSeries(levels, p), maxDegree);
            const int got = fit.degree(p);
            if (got != want) ++mismatches;
            if (want == maxDegree) ++atMax;
            if (want == 1) ++linear;
        }
        EXPECT_EQ(0u, mismatches) << "pixels whose stored degree differs from the independent cascade's";
        // the control: the two classes both exist in this crop, so a policy that sends everything one way cannot pass
        EXPECT_GT(atMax, 0u);
        EXPECT_GT(linear, 0u);
    }
}

TEST_F(GainPolyPolicyTest, ADecreasingCurveIsKeptAndAFallingThenRisingOneIsReplacedByTheLeastSquaresLine) {
    const uint32_t w = 4, h = 1;
    const std::vector<double> doses{8000.0, 12000.0, 16000.0, 20000.0};
    // pixel 0: a convex curve that FALLS over the whole range (derivative -2e-5 + 1.6e-9 (x-8000) < 0 up to 20000)
    // pixel 1: the same curve mirrored: it RISES over the whole range
    // pixel 2: rises then falls: no monotone quadratic fits it
    // pixel 3: a clear straight decline
    static const double kInterior[4] = {1.0, 1.2, 1.25, 1.1};   // rises, then falls: its quadratic has an interior maximum
    std::vector<std::vector<float>> levels(4, std::vector<float>(4));
    for (size_t l = 0; l < 4; ++l) {
        const double t = doses[l] - 8000.0;
        levels[l][0] = static_cast<float>(1.0 - 2e-5 * t + 8e-10 * t * t);
        levels[l][1] = static_cast<float>(1.0 + 2e-5 * t - 8e-10 * t * t);
        levels[l][2] = static_cast<float>(kInterior[l]);
        levels[l][3] = static_cast<float>(1.05 - 8e-6 * t);
    }
    const Fit fit = generate(levels, w, h, doses, 2);
    EXPECT_EQ(2, fit.degree(0)) << "a falling convex curve is monotone: it is kept, not replaced";
    EXPECT_GT(pixelR2(fit, 0, levels, doses), 0.999999) << "and it explains its data";
    EXPECT_EQ(2, fit.degree(1)) << "the rising mirror was accepted before and still is";
    EXPECT_EQ(1, fit.degree(2)) << "an interior maximum is not monotone: the line is the least-squares line";
    // an exact straight line: the quadratic term of its least-squares quadratic is rounding noise (it may or may not be zero)
    EXPECT_LT(std::fabs(fit.c(3, 2)) * 12000.0 * 12000.0, 1e-6) << "a straight decline stays a straight line";
    EXPECT_NEAR(-8e-6, fit.c(3, 1), 1e-9);
    // the line of pixel 2 is THE least-squares line, not a line through the end measurements
    std::vector<long double> ls;
    ASSERT_TRUE(lsFit(doses, pixelSeries(levels, 2), 1, &ls));
    EXPECT_NEAR(static_cast<double>(ls[1]), fit.c(2, 1), 1e-9);
    EXPECT_NEAR(static_cast<double>(ls[0]), fit.c(2, 0), 1e-5);
    const double endpoint = (levels[3][2] - levels[0][2]) / (doses[3] - doses[0]);
    EXPECT_GT(std::fabs(endpoint - fit.c(2, 1)), 1e-9) << "control: the least-squares slope differs from the endpoint slope here";
    EXPECT_GE(pixelR2(fit, 2, levels, doses), 0.0);
}

/* ---------------------------------------------------------------------------
 * 3b. Pixels that were fitted at degree >= 2 before are stored bit for bit as before.
 *     The table is the output of the OLD generator (QA-A-210c capture, DISABLED_PrintGolden below).
 * ------------------------------------------------------------------------- */

#include "gain_poly_policy_golden.inc"

TEST_F(GainPolyPolicyTest, PixelsTheOldPolicyFittedAtDegreeTwoOrMoreAreStoredBitForBit) {
    const auto levels = realLevels();
    const auto doses = realDoses();
    const uint32_t n = gain_poly_real::kCropSide;
    const Fit fit2 = generate(levels, n, n, doses, 2);
    ASSERT_GT(sizeof(kGoldenMax2) / sizeof(kGoldenMax2[0]), 0u);
    for (const auto& g : kGoldenMax2) {
        SCOPED_TRACE("max degree 2, pixel " + std::to_string(g.pix));
        EXPECT_EQ(g.c0, fit2.bits(g.pix, 0));
        EXPECT_EQ(g.c1, fit2.bits(g.pix, 1));
        EXPECT_EQ(g.c2, fit2.bits(g.pix, 2));
    }
}

/** Prints kGoldenMax2 for gain_poly_policy_golden.inc. Run ONCE against the generator whose output is to be preserved. */
TEST_F(GainPolyPolicyTest, DISABLED_PrintGolden) {
    const auto levels = realLevels();
    const auto doses = realDoses();
    const uint32_t n = gain_poly_real::kCropSide;
    const Fit fit2 = generate(levels, n, n, doses, 2);
    std::printf("// pixels fitted at degree 2 by the old policy (max degree 2): pixel, bits of c0 c1 c2\n");
    for (size_t p = 0; p < fit2.pixels(); ++p) {
        if (fit2.degree(p) >= 2) std::printf("    {%zuu, 0x%08Xu, 0x%08Xu, 0x%08Xu},\n", p, fit2.bits(p, 0), fit2.bits(p, 1), fit2.bits(p, 2));
    }
}

/* ---------------------------------------------------------------------------
 * 4. The synthetic ladder of QA-A-210 (fixture generator, seed 7, 32x32, --gain-poly): R^2 was -0.0766.
 * ------------------------------------------------------------------------- */

TEST_F(GainPolyPolicyTest, TheSyntheticLadderOfTheFixtureGeneratorNoLongerReportsANegativeRSquared) {
    std::string cmd = std::string("\"") + XPE_FIXTURE_GEN_EXE + "\" --out \"" + root.string() +
                      "\" --width 32 --height 32 --seed 7 --gain-poly";
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";
#endif
    ASSERT_EQ(0, std::system(cmd.c_str()));
    const fs::path poly = root / "gain_poly.xcal";
    ASSERT_TRUE(fs::exists(poly));
    std::ifstream f(poly, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    XCalFileHeader hdr{};
    ASSERT_GE(bytes.size(), sizeof(hdr));
    std::memcpy(&hdr, bytes.data(), sizeof(hdr));
    const std::string cfg = bytes.substr(sizeof(hdr), static_cast<size_t>(hdr.config_json_len));
    const double r2 = jsonNumber(cfg, "fit_r_squared");
    EXPECT_FALSE(std::isnan(r2)) << "the file records its R^2";
    EXPECT_GE(r2, 0.0) << "was -0.0766 under the endpoint-line policy";
}
