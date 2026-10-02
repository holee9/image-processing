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
 *   2. every stored polynomial is monotone over the WHOLE measured dose range, in either direction -- decided analytically
 *      (QA-A-210d: a 100-point sample misses an extremum between two samples)
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
#include <limits>
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

/** Real roots of q0 + q1 s + q2 s^2 + q3 s^3 (any leading coefficient may be exactly zero). Closed forms: linear, quadratic, Cardano. */
std::vector<long double> realRoots(const long double q[4]) {
    std::vector<long double> r;
    int n = 3;
    while (n > 0 && q[n] == 0.0L) --n;
    if (n == 1) {
        r.push_back(-q[0] / q[1]);
    } else if (n == 2) {
        const long double d = q[1] * q[1] - 4.0L * q[2] * q[0];
        if (d >= 0.0L) {
            const long double sq = sqrtl(d);
            r.push_back((-q[1] + sq) / (2.0L * q[2]));
            r.push_back((-q[1] - sq) / (2.0L * q[2]));
        }
    } else if (n == 3) {
        const long double a = q[3], b = q[2], c = q[1], d = q[0];
        const long double p = (3.0L * a * c - b * b) / (3.0L * a * a);
        const long double qq = (2.0L * b * b * b - 9.0L * a * b * c + 27.0L * a * a * d) / (27.0L * a * a * a);
        const long double shift = b / (3.0L * a);
        const long double disc = qq * qq / 4.0L + p * p * p / 27.0L;
        if (disc > 1e-24L) {
            const long double sd = sqrtl(disc);
            r.push_back(cbrtl(-qq / 2.0L + sd) + cbrtl(-qq / 2.0L - sd) - shift);
        } else if (disc >= -1e-24L) {
            if (p == 0.0L && qq == 0.0L) {
                r.push_back(-shift);
            } else {
                const long double u = cbrtl(-qq / 2.0L);
                r.push_back(2.0L * u - shift);
                r.push_back(-u - shift);
            }
        } else {
            const long double rad = 2.0L * sqrtl(-p / 3.0L);
            long double arg = (3.0L * qq / (2.0L * p)) * sqrtl(-3.0L / p);
            arg = std::max(-1.0L, std::min(1.0L, arg));
            const long double phi = acosl(arg) / 3.0L;
            const long double kTwoPi3 = 2.0943951023931954923L;
            for (int k = 0; k < 3; ++k) r.push_back(rad * cosl(phi - k * kTwoPi3) - shift);
        }
    }
    return r;
}

/**
 * Whether the polynomial sum c[j] x^j is monotone over the WHOLE closed interval [x0, x1] -- decided analytically, with no
 * sampling: the derivative is shifted to s = (x - x0) / (x1 - x0) in [0, 1], its real roots (at most three) are found in
 * closed form, and the sign of the derivative is read at the midpoint of every sub-interval the roots cut. A root that
 * touches zero without crossing leaves the same sign on both sides, so it is correctly not a turning point.
 * Written independently of the product (which finds the critical points of the derivative instead).
 */
bool monotoneOverInterval(const std::vector<long double>& c, double x0, double x1) {
    const long double L = static_cast<long double>(x1) - x0;
    long double d[4] = {0, 0, 0, 0};                       // P'(x) = d0 + d1 x + d2 x^2 + d3 x^3
    for (size_t j = 1; j < c.size() && j <= 4; ++j) d[j - 1] = static_cast<long double>(j) * c[j];
    long double q[4] = {0, 0, 0, 0};                       // Q(s) = P'(x0 + L s)
    for (int k = 0; k < 4; ++k) {
        long double sum = 0.0L;
        for (int j = k; j < 4; ++j) {
            long double binom = 1.0L;
            for (int i = 0; i < k; ++i) binom = binom * (j - i) / (i + 1);
            sum += d[j] * binom * powl(static_cast<long double>(x0), j - k);
        }
        q[k] = sum * powl(L, k);
    }
    long double scale = 0.0L;
    for (int k = 0; k < 4; ++k) scale += fabsl(q[k]);
    const long double tol = 1e-12L * scale;
    std::vector<long double> pts{0.0L, 1.0L};
    for (const long double root : realRoots(q)) if (root > 0.0L && root < 1.0L) pts.push_back(root);
    std::sort(pts.begin(), pts.end());
    bool rising = false, falling = false;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        const long double s = 0.5L * (pts[i] + pts[i + 1]);
        const long double v = ((q[3] * s + q[2]) * s + q[1]) * s + q[0];
        if (v > tol) rising = true;
        if (v < -tol) falling = true;
    }
    return !(rising && falling);
}

/** The coefficients as the file stores them: rounded to float32. */
std::vector<long double> asStored(const std::vector<long double>& c) {
    std::vector<long double> out;
    for (const long double v : c) out.push_back(static_cast<long double>(static_cast<float>(v)));
    return out;
}

/** The degree this policy must pick for a pixel -- written independently of the product, on the coefficients AS STORED. */
int expectedDegree(const std::vector<double>& x, const std::vector<double>& y, int maxDegree) {
    for (int deg = maxDegree; deg >= 1; --deg) {
        std::vector<long double> c;
        if (!lsFit(x, y, deg, &c)) continue;
        if (deg == 1 || monotoneOverInterval(asStored(c), x.front(), x.back())) return deg;
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
                if (!monotoneOverInterval(c, cs.doses.front(), cs.doses.back())) ++bad;
            }
            EXPECT_EQ(0u, bad) << "stored polynomials that turn inside the measured range (decided analytically, on the stored float32 coefficients)";
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

/* ---------------------------------------------------------------------------
 * 5. QA-A-210d (Codex #58): the monotone test is analytic and applies to the stored coefficients;
 *    non-finite doses and gain values are refused; close doses are fitted.
 * ------------------------------------------------------------------------- */

TEST_F(GainPolyPolicyTest, TheOracleItselfSeesATurnBetweenTwoSamples) {
    // control for the analytic oracle: G(E) = (E - 1001)^2 / 1e6 over [1000, 2000] turns at 1001, inside the first
    // sample gap (~10 ADU) of a 100-point rule
    std::vector<long double> c = {1001.0L * 1001.0L / 1e6L, -2.0L * 1001.0L / 1e6L, 1.0L / 1e6L};
    EXPECT_FALSE(monotoneOverInterval(c, 1000.0, 2000.0));
    // the same curve over [1001, 2000] and over [1002, 2000] does not turn there
    EXPECT_TRUE(monotoneOverInterval(c, 1001.0, 2000.0));
    EXPECT_TRUE(monotoneOverInterval(c, 1002.0, 2000.0));
    // a cubic with two turns inside, a cubic with none, a decreasing quartic
    EXPECT_FALSE(monotoneOverInterval({0.0L, 6.0L, -9.0L, 2.0L}, 0.0, 3.0));        // 2x^3 - 9x^2 + 6x: turns at 0.79 and 2.21
    EXPECT_TRUE(monotoneOverInterval({0.0L, 1.0L, 0.0L, 1.0L}, -5.0, 5.0));          // x + x^3
    EXPECT_TRUE(monotoneOverInterval({10.0L, -1.0L, -0.1L, -0.01L, -0.001L}, 0.0, 4.0));
    EXPECT_TRUE(monotoneOverInterval({0.0L, 0.0L, 1.0L, 0.0L, 0.0L}, 0.0, 5.0));     // x^2 on [0, 5]: derivative zero only at the end
    EXPECT_TRUE(monotoneOverInterval({0.0L, 0.0L, 0.0L, 1.0L}, -1.0, 1.0));          // x^3: a double root of the derivative, no turn
}

TEST_F(GainPolyPolicyTest, ACurveThatTurnsBetweenTwoSamplesOfTheOldRuleIsNotMonotoneAndGetsTheLeastSquaresLine) {
    // Codex #58 reproduction: doses [1000,1250,1500,1750,2000], G(E) = (E - 1001)^2 / 1e6. The quadratic fits exactly and
    // falls from 1000 to 1001 before it rises; the old rule sampled [1000, 1010.1, ...] and saw only the rise.
    const std::vector<double> doses{1000.0, 1250.0, 1500.0, 1750.0, 2000.0};
    const uint32_t w = 3, h = 1;
    std::vector<std::vector<float>> levels(5, std::vector<float>(3));
    for (size_t l = 0; l < 5; ++l) {
        const double e = doses[l];
        // +0.5 keeps every gain above the applier's floor of 0.001 (QA-A-210e); the offset changes no turning point
        levels[l][0] = static_cast<float>(0.5 + (e - 1001.0) * (e - 1001.0) / 1e6);    // turns at 1001, inside the range
        levels[l][1] = static_cast<float>(0.5 + (e - 500.0) * (e - 500.0) / 1e6);      // turns at 500, outside: monotone
        levels[l][2] = static_cast<float>(1.0 - (e - 1001.0) * (e - 1001.0) / 4e6);    // the mirror: a maximum at 1001
    }
    const Fit fit = generate(levels, w, h, doses, 2);
    EXPECT_EQ(1, fit.degree(0)) << "the derivative changes sign at 1001: not monotone, so the line";
    EXPECT_EQ(2, fit.degree(1)) << "control: a vertex outside the range is monotone and the quadratic is kept";
    EXPECT_EQ(1, fit.degree(2)) << "the mirror turns too";
    for (size_t p = 0; p < 3; ++p) {
        std::vector<long double> c;
        for (int j = 0; j <= 2; ++j) c.push_back(static_cast<long double>(fit.c(p, j)));
        EXPECT_TRUE(monotoneOverInterval(c, doses.front(), doses.back())) << "pixel " << p << " is stored non-monotone";
    }
}

TEST_F(GainPolyPolicyTest, TheDegreeIsDecidedOnTheCoefficientsAsStoredNotOnTheDoubleOnes) {
    // parabolas whose vertex sits a hair outside the first dose: monotone in double arithmetic, and whether the float32
    // coefficients still are depends on the rounding. The oracle decides on the float32 values; the product must agree.
    const std::vector<double> doses{8000.0, 12000.0, 16000.0, 20000.0};
    constexpr uint32_t kScan = 240, kPix = kScan + 2;
    std::vector<std::vector<float>> levels(4, std::vector<float>(kPix));
    size_t differ = 0;
    for (uint32_t i = 0; i < kScan; ++i) {
        const double eps = 1e-4 * std::pow(1.05, static_cast<double>(i));             // 1e-4 .. ~1.2e1 ADU past the first dose
        const double vertex = 8000.0 - eps;
        for (size_t l = 0; l < 4; ++l) {
            const double d = doses[l] - vertex;
            levels[l][i] = static_cast<float>(1.0 + 4e-9 * d * d);
        }
    }
    // the two pixels whose vertex is exactly the first dose and whose curvature makes the float32 rounding of the
    // coefficients reach the derivative at that dose (measured: the derivative there is a hair of one sign in double
    // arithmetic and of the other once the coefficients are float32) -- the control that the stored form matters
    const double kCurv[2] = {1e-6, 2e-6};
    for (uint32_t k = 0; k < 2; ++k) {
        for (size_t l = 0; l < 4; ++l) {
            const double d = doses[l] - 8000.0;
            // base 100: every gain AND the least-squares line stay inside the applier's range [0.001, 1000] (QA-A-210e),
            // so the generation is accepted; the flip survives the base (measured)
            levels[l][kScan + k] = static_cast<float>(100.0 + kCurv[k] * d * d);
        }
    }
    const Fit fit = generate(levels, kPix, 1, doses, 2);
    for (uint32_t i = 0; i < kPix; ++i) {
        const auto y = pixelSeries(levels, i);
        const int want = expectedDegree(doses, y, 2);
        EXPECT_EQ(want, fit.degree(i)) << "pixel " << i;
        std::vector<long double> c;
        if (lsFit(doses, y, 2, &c) && monotoneOverInterval(c, doses.front(), doses.back()) !=
                                            monotoneOverInterval(asStored(c), doses.front(), doses.back())) ++differ;
    }
    EXPECT_GT(differ, 0u) << "control: at least one pixel is monotone in double and not in float32, so the stored form matters";
}

TEST_F(GainPolyPolicyTest, ANonFiniteDoseIsRefusedBeforeAnyFileIsReadOrAnyStateChanges) {
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<std::vector<double>> bad = {
        {1.0, 2.0, 3.0, inf},        // passes a bare "strictly ascending" test: inf > 3
        {1.0, 2.0, inf, inf},
        {-inf, 2.0, 3.0, 4.0},       // -inf < 2
        {1.0, 2.0, nan, 4.0},
        {nan, 2.0, 3.0, 4.0},
        {1.0, 2.0, 3.0, nan},
    };
    // paths that do not exist: a refusal for the dose means the files were never opened (IO_FAILED would say they were)
    const char* paths[4] = {"no_such_gain_0.xcal", "no_such_gain_1.xcal", "no_such_gain_2.xcal", "no_such_gain_3.xcal"};
    XpeCalibQualityMeta before{};
    (void)xpe_calib_get_quality_meta(&before);
    for (const auto& doses : bad) {
        SCOPED_TRACE("doses " + std::to_string(doses[0]) + " " + std::to_string(doses[1]) + " " + std::to_string(doses[2]) + " " + std::to_string(doses[3]));
        xpe_clear_alerts();
        const std::string out = (root / "never.xcal").string();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_generate_gain_polynomial(paths, doses.data(), 4, 2, out.c_str()));
        EXPECT_FALSE(fs::exists(out));
        EXPECT_EQ(0, xpe_get_pending_alert_count()) << "no mode alert, nothing";
    }
    XpeCalibQualityMeta after{};
    (void)xpe_calib_get_quality_meta(&after);
    EXPECT_EQ(0, std::memcmp(&before, &after, sizeof(before))) << "the quality metadata changed";
}

TEST_F(GainPolyPolicyTest, ANonFiniteGainValueIsRefusedAsBadCalibrationDataAndNeverStoredAsNanCoefficients) {
    const std::vector<double> doses{8000.0, 12000.0, 16000.0, 20000.0};
    for (const float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity()}) {
        std::vector<std::vector<float>> levels = noiseLevels(4, 4, 4);
        levels[2][5] = bad;
        std::vector<std::string> paths;
        for (size_t l = 0; l < levels.size(); ++l) paths.push_back(writeLevel(("lvl" + std::to_string(l) + ".xcal").c_str(), 4, 4, levels[l].data()));
        std::vector<const char*> cp;
        for (const auto& pth : paths) cp.push_back(pth.c_str());
        const std::string out = (root / "nan.xcal").string();
        std::remove(out.c_str());
        EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), 4, 2, out.c_str()))
            << "a gain value of " << bad;
        EXPECT_FALSE(fs::exists(out));
    }
}

TEST_F(GainPolyPolicyTest, CloseDosesAreFittedAccuratelyAndNothingBecomesNonFinite) {
    // doses 1000 .. 1000.00004: a span of 4e-5 ADU around a large offset. The gain is exactly linear in the dose, so a
    // least-squares line must recover the slope. The normal equations in the raw dose lose it to cancellation (the
    // condition number is (x/span)^2 ~ 6e14, the double precision is 1e-16); centred and scaled they do not.
    // The file stores float32 coefficients in the raw dose, and an intercept of |c0| ~ 5e3 has a float32 resolution of
    // ~3e-4 -- far above the 2e-4 gain range of this ladder. The SLOPE is what the fit decides; what the file can DO with
    // it is what the applier computes, and the R^2 the file records is that (QA-A-210e, see the next test).
    const std::vector<double> doses{1000.0, 1000.00001, 1000.00002, 1000.00003, 1000.00004};
    const uint32_t w = 2, h = 1;
    std::vector<std::vector<float>> levels(5, std::vector<float>(2));
    for (size_t l = 0; l < 5; ++l) {
        levels[l][0] = static_cast<float>(1.0 + 5.0 * (doses[l] - 1000.0));                // rising, slope 5
        levels[l][1] = static_cast<float>(1.0 - 3.0 * (doses[l] - 1000.0));                // falling, slope -3
    }
    const Fit fit = generate(levels, w, h, doses, 1);
    ASSERT_EQ(2u, fit.pixels()) << "the generation itself must succeed";
    for (size_t p = 0; p < 2; ++p) {
        for (int j = 0; j <= 1; ++j) EXPECT_TRUE(std::isfinite(fit.c(p, j))) << "pixel " << p << " coefficient " << j;
        const double slope = (p == 0) ? 5.0 : -3.0;
        EXPECT_NEAR(slope, fit.c(p, 1), 0.01 * std::fabs(slope)) << "pixel " << p << ": the slope is recovered";
    }
}

/* ---------------------------------------------------------------------------
 * 6. QA-A-210e (Codex #61): what the generator reports and stores is what the APPLIER computes.
 *    The applier evaluates the stored float32 coefficients in float32 Horner at the pixel value (as a float), clamped to
 *    the fitted range, and refuses a gain outside [0.001, 1000].
 * ------------------------------------------------------------------------- */

namespace {

/** The applier's arithmetic: float32 Horner from the highest coefficient down (gain_correct.cpp). */
float applierGain(const Fit& fit, size_t pix, double dose) {
    const int n = fit.maxDegree + 1;
    float x = static_cast<float>(dose);
    float acc = static_cast<float>(fit.c(pix, n - 1));
    for (int j = n - 1; j > 0; --j) acc = acc * x + static_cast<float>(fit.c(pix, j - 1));
    return acc;
}

/** The R^2 of the whole file, recomputed in the applier's arithmetic from the stored coefficients. */
double appliedR2(const Fit& fit, const std::vector<std::vector<float>>& levels, const std::vector<double>& doses) {
    double ssRes = 0.0, ssTot = 0.0;
    for (size_t p = 0; p < fit.pixels(); ++p) {
        double mean = 0.0;
        for (const auto& l : levels) mean += l[p];
        mean /= static_cast<double>(levels.size());
        for (size_t i = 0; i < levels.size(); ++i) {
            const double y = levels[i][p];
            const double r = y - static_cast<double>(applierGain(fit, p, doses[i]));
            ssRes += r * r;
            ssTot += (y - mean) * (y - mean);
        }
    }
    return ssTot > 0.0 ? 1.0 - ssRes / ssTot : 1.0;
}

}  // namespace

TEST_F(GainPolyPolicyTest, ACodexReproductionWhoseStoredCoefficientsApplyBelowTheGainFloorIsRefusedNotWrittenAsSuccess) {
    // Codex #61: doses 1000 + k*1e-5, gain 0.001 + 5 (dose - 1000), degree 1. The double fit is exact (R^2 = 1); the stored
    // float32 coefficients are [-5000, 5.000000953674316]; the applier's float32 Horner at pixel 1000 gives 0.0009765625,
    // below its floor of 0.001, and the frame is refused with CONFIG_INVALID. A generator that reports success for such a
    // file is wrong; it now refuses the calibration.
    const std::vector<double> doses{1000.0, 1000.00001, 1000.00002, 1000.00003, 1000.00004};
    std::vector<std::vector<float>> levels(5, std::vector<float>(1));
    for (size_t l = 0; l < 5; ++l) levels[l][0] = static_cast<float>(0.001 + 5.0 * (doses[l] - 1000.0));
    std::vector<std::string> paths;
    for (size_t l = 0; l < 5; ++l) paths.push_back(writeLevel(("lvl" + std::to_string(l) + ".xcal").c_str(), 1, 1, levels[l].data()));
    std::vector<const char*> cp;
    for (const auto& pth : paths) cp.push_back(pth.c_str());
    const std::string out = (root / "repro.xcal").string();
    XpeCalibQualityMeta before{};
    (void)xpe_calib_get_quality_meta(&before);
    xpe_clear_alerts();
    const XpeErrorCode rc = xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), 5, 1, out.c_str());
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, rc) << "the calibration cannot be applied by the applier's arithmetic";
    EXPECT_FALSE(fs::exists(out)) << "no file is written for a calibration the applier would refuse";
    XpeCalibQualityMeta after{};
    (void)xpe_calib_get_quality_meta(&after);
    EXPECT_EQ(0, std::memcmp(&before, &after, sizeof(before))) << "the quality record is not changed by a refused generation";
    // the refusal says what happened, and for how many pixels
    bool said = false;
    char msg[512];
    int32_t sev = -1;
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK && std::string(msg).find("float32") != std::string::npos) said = true;
    }
    EXPECT_TRUE(said) << "an alert names the cause";
}

TEST_F(GainPolyPolicyTest, WhateverTheGeneratorWritesTheApplierCanApplyAtEveryMeasuredDose) {
    // The invariant, end to end: generate -> load -> xpe_gain_correct on a frame whose pixels ARE the measured doses.
    // Cases: the real crop (benign), noise-only, and a ladder of doses close together around a large offset with gains
    // near 1 (the stored coefficients are inexact but the gain stays in the applier's range).
    struct Case { const char* name; std::vector<std::vector<float>> levels; std::vector<double> doses; uint32_t w, h; int maxDegree; };
    const uint32_t n = gain_poly_real::kCropSide;
    std::vector<std::vector<float>> close(5, std::vector<float>(2));
    const std::vector<double> closeDoses{1000.0, 1000.00001, 1000.00002, 1000.00003, 1000.00004};
    for (size_t l = 0; l < 5; ++l) {
        close[l][0] = static_cast<float>(1.0 + 5.0 * (closeDoses[l] - 1000.0));
        close[l][1] = static_cast<float>(1.0 - 3.0 * (closeDoses[l] - 1000.0));
    }
    const std::vector<Case> cases = {
        {"real crop", realLevels(), realDoses(), n, n, 2},
        {"noise only", noiseLevels(32, 32, 4), {8000.0, 12000.0, 16000.0, 20000.0}, 32, 32, 2},
        {"close doses", close, closeDoses, 2, 1, 1},
    };
    for (const Case& cs : cases) {
        SCOPED_TRACE(cs.name);
        std::vector<std::string> paths;
        for (size_t l = 0; l < cs.levels.size(); ++l) paths.push_back(writeLevel(("c" + std::to_string(l) + ".xcal").c_str(), cs.w, cs.h, cs.levels[l].data()));
        std::vector<const char*> cp;
        for (const auto& pth : paths) cp.push_back(pth.c_str());
        const std::string out = (root / "inv.xcal").string();
        std::remove(out.c_str());
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(cp.data(), cs.doses.data(), static_cast<int32_t>(cs.doses.size()), cs.maxDegree, out.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(out.c_str()));
        for (size_t l = 0; l < cs.doses.size(); ++l) {
            std::vector<uint16_t> src(static_cast<size_t>(cs.w) * cs.h, static_cast<uint16_t>(std::lround(cs.doses[l])));
            std::vector<float> dst(src.size(), 0.0f);
            XpeImageBuffer in{}, o{};
            in.width = cs.w; in.height = cs.h; in.bitsAllocated = 16; in.bitsStored = 16; in.format = XPE_PIXEL_UINT16;
            in.data = src.data(); in.dataSize = src.size() * sizeof(uint16_t);
            o.width = cs.w; o.height = cs.h; o.bitsAllocated = 32; o.bitsStored = 32; o.format = XPE_PIXEL_FLOAT32;
            o.data = dst.data(); o.dataSize = dst.size() * sizeof(float);
            XpeImageMetadata meta{};
            EXPECT_EQ(XPE_OK, xpe_gain_correct(&in, &o, &meta)) << "dose level " << l << ": the applier refused a file the generator wrote";
            for (const float v : dst) EXPECT_TRUE(std::isfinite(v));
        }
    }
}

TEST_F(GainPolyPolicyTest, TheRSquaredTheFileRecordsIsTheOneOfTheApplierArithmeticOnTheStoredCoefficients) {
    // R^2 used to be computed from the double coefficients and doubles; the applier uses the stored float32 ones. They
    // agree on well-conditioned data and part where the float32 intercept cannot carry the fit.
    struct Case { const char* name; std::vector<std::vector<float>> levels; std::vector<double> doses; uint32_t w, h; int maxDegree; };
    const uint32_t n = gain_poly_real::kCropSide;
    std::vector<std::vector<float>> close(5, std::vector<float>(2));
    const std::vector<double> closeDoses{1000.0, 1000.00001, 1000.00002, 1000.00003, 1000.00004};
    for (size_t l = 0; l < 5; ++l) {
        close[l][0] = static_cast<float>(1.0 + 5.0 * (closeDoses[l] - 1000.0));
        close[l][1] = static_cast<float>(1.0 - 3.0 * (closeDoses[l] - 1000.0));
    }
    const std::vector<Case> cases = {
        {"real crop, degree 2", realLevels(), realDoses(), n, n, 2},
        {"real crop, degree 3", realLevels(), realDoses(), n, n, 3},
        {"close doses (float32 intercept cannot carry the slope)", close, closeDoses, 2, 1, 1},
    };
    double closeR2 = 1.0;
    for (const Case& cs : cases) {
        SCOPED_TRACE(cs.name);
        const Fit fit = generate(cs.levels, cs.w, cs.h, cs.doses, cs.maxDegree);
        ASSERT_EQ(static_cast<size_t>(cs.w) * cs.h, fit.pixels());
        const double want = appliedR2(fit, cs.levels, cs.doses);
        EXPECT_NEAR(want, fit.fileR2, 1e-8) << "the file's R^2 must be the applied-arithmetic R^2";
        if (std::string(cs.name).find("close") != std::string::npos) closeR2 = fit.fileR2;
    }
    EXPECT_LT(closeR2, 0.99) << "control: where float32 cannot carry the fit, the recorded R^2 says so (it was ~1 from doubles)";
}

TEST_F(GainPolyPolicyTest, AFitWhoseFloat32EvaluationDeviatesFromItBeyondTheToleranceIsRefusedEvenWithValidGains) {
    // gains 1 .. 1.002, slope 50 per ADU, doses 1000 .. 1000.00004: the stored intercept is ~ -5e4 and its float32 resolution
    // (~4e-3) is larger than the whole gain range, so the applier's evaluation lands up to ~4e-3 from the fit at a dose --
    // above the 0.1% the generator accepts. Every gain here is in the applier's range, so only the tolerance refuses it
    // (the file would record R^2 ~ 0 for what is, in double, an exact line).
    const std::vector<double> doses{1000.0, 1000.00001, 1000.00002, 1000.00003, 1000.00004};
    std::vector<std::vector<float>> levels(5, std::vector<float>(1));
    for (size_t l = 0; l < 5; ++l) levels[l][0] = static_cast<float>(1.0 + 50.0 * (doses[l] - 1000.0));
    std::vector<std::string> paths;
    for (size_t l = 0; l < 5; ++l) paths.push_back(writeLevel(("lvl" + std::to_string(l) + ".xcal").c_str(), 1, 1, levels[l].data()));
    std::vector<const char*> cp;
    for (const auto& pth : paths) cp.push_back(pth.c_str());
    const std::string out = (root / "tol.xcal").string();
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), 5, 1, out.c_str()));
    EXPECT_FALSE(fs::exists(out));
}

TEST_F(GainPolyPolicyTest, AGainOutsideTheAppliersRangeIsRefusedEvenWhenTheFitIsExact) {
    // well-conditioned doses, an exactly linear gain -- accurate in float32 -- but the VALUES are outside the range the
    // applier accepts ([0.001, 1000]): it would refuse every frame with CONFIG_INVALID. The generator refuses the
    // calibration instead of writing it. (Only the range check refuses these; the tolerance is satisfied.)
    const std::vector<double> doses{8000.0, 12000.0, 16000.0, 20000.0};
    for (const double base : {5e-4, 2000.0}) {
        SCOPED_TRACE("gain level " + std::to_string(base));
        std::vector<std::vector<float>> levels(4, std::vector<float>(1));
        for (size_t l = 0; l < 4; ++l) levels[l][0] = static_cast<float>(base * (1.0 + 1e-3 * static_cast<double>(l)));
        std::vector<std::string> paths;
        for (size_t l = 0; l < 4; ++l) paths.push_back(writeLevel(("r" + std::to_string(l) + ".xcal").c_str(), 1, 1, levels[l].data()));
        std::vector<const char*> cp;
        for (const auto& pth : paths) cp.push_back(pth.c_str());
        const std::string out = (root / "range.xcal").string();
        std::remove(out.c_str());
        EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), 4, 1, out.c_str()));
        EXPECT_FALSE(fs::exists(out));
    }
}
