// #180 (QA-B-95): virtual grid against MC-GPU phantoms.
//
// The first comparison against an answer made by OTHER code: the phantom's
// `total` image (MC-GPU, primary + scatter) goes through the virtual grid and
// the recovered primary is compared with the phantom's `primary` image.
// Tables: the MC kernel and [wet] tables (simulation-based, not calibrated).
// There is no [grid] table yet, so the comparison uses an ideal grid row
// (tp 1, ts 0): "remove all scatter", which is what `primary` is.
//
// This is MC-to-MC agreement, not agreement with a real detector.
//
// Values are logged (VGMC lines). Only loose bounds are asserted: the
// correction must bring the image closer to the primary than no correction.
// Thresholds follow once the lead has read these numbers.
//
// Data: tests/data/mc (copied from tools/mcsim, see its README).

#include <gtest/gtest.h>

#include "virtual_grid.h"

#include "xpe/gsvg/gsvg_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace vg = xpe_gsvg_detail;

namespace {

constexpr const char* kDir = "tests/data/mc/";
constexpr int    kN = 80;
constexpr double kPitchMm = 4.0;       // json pixel_pitch_mm
constexpr double kKvp = 80.0;          // json kvp
constexpr double kDnScale = 6955.33056938879;   // json dn_scale
constexpr double kAirDn = 50000.0;     // json dn_air
// Field 30 x 30 cm on a 32 cm detector: rows/cols 3..76 carry the beam. The
// metrics use rows/cols [8, 72): 2 cm inside the field edge.
constexpr int kLo = 8, kHi = 72;

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<double> ReadF32(const std::string& path) {
    const std::string raw = ReadText(path);
    EXPECT_EQ(raw.size(), static_cast<size_t>(kN * kN * 4)) << path;
    std::vector<double> out(kN * kN, 0.0);
    for (size_t i = 0; i < out.size() && (i + 1) * 4 <= raw.size(); ++i) {
        float v = 0;
        std::memcpy(&v, raw.data() + i * 4, 4);
        out[i] = v;
    }
    return out;
}

// [kernels] and [wet] from the MC tables, plus an ideal-grid row.
vg::ParamTable McTable(bool gauss2Only) {
    std::string kernels = ReadText(std::string(kDir) + "scatter_kernels_water_csi600.csv");
    if (gauss2Only) {
        std::istringstream is(kernels);
        std::string line, kept;
        while (std::getline(is, line))
            if (line.find(",gauss4,") == std::string::npos) kept += line + "\n";
        kernels = kept;
    }
    const std::string text = "[kernels]\n" + kernels + "\n[wet]\n" +
        ReadText(std::string(kDir) + "wet_water_csi600.csv") +
        "\n[grid]\nratio,tp,ts\n# ideal grid: removes all scatter (no [grid] table exists yet)\n100,1,0\n";
    vg::ParamTable t;
    const std::string err = vg::ParseParamTable(text, t);
    EXPECT_EQ(err, "");
    return t;
}

struct Phantom {
    std::vector<double> total, primary, thickness, air;   // DN (thickness in cm)
};

Phantom Load(const char* name) {
    const std::string base = std::string(kDir) + name + "_80kVp_";
    Phantom p;
    p.total = ReadF32(base + "total.f32");
    p.primary = ReadF32(base + "primary.f32");
    p.thickness = ReadF32(base + "thickness.f32");
    p.air = ReadF32(base + "air.f32");
    for (double& v : p.total) v *= kDnScale;
    for (double& v : p.primary) v *= kDnScale;
    for (double& v : p.air) v *= kDnScale;
    return p;
}

// Outside the collimated field `total` still carries scatter (about 3000 DN
// next to the field). The virtual grid has no collimation mask, so the
// caller zeroes those pixels -- here from the air image.
std::vector<double> Collimated(const Phantom& p) {
    const double centre = p.air[40 * kN + 40];
    std::vector<double> out = p.total;
    for (size_t i = 0; i < out.size(); ++i)
        if (p.air[i] < 0.5 * centre) out[i] = 0.0;
    return out;
}

bool InRegion(int i) {
    const int r = i / kN, c = i % kN;
    return r >= kLo && r < kHi && c >= kLo && c < kHi;
}

struct Metrics {
    std::string error;
    int factor = 0;
    double median = 0, lo = 0, hi = 0;      // recovered / primary
    double absMedian = 0;                    // median |ratio - 1|
    double capped = 0;
};

Metrics Measure(const std::vector<double>& img, const Phantom& p) {
    Metrics m;
    std::vector<double> r, a;
    for (int i = 0; i < kN * kN; ++i) {
        if (!InRegion(i) || p.primary[i] <= 0) continue;
        const double q = img[static_cast<size_t>(i)] / p.primary[static_cast<size_t>(i)];
        r.push_back(q);
        a.push_back(std::fabs(q - 1.0));
    }
    std::sort(r.begin(), r.end());
    std::sort(a.begin(), a.end());
    m.median = r[r.size() / 2];
    m.lo = r.front();
    m.hi = r.back();
    m.absMedian = a[a.size() / 2];
    return m;
}

struct Config {
    const char* name;
    vg::CapMode cap;
    int iterations;
    bool gauss2;
    int factor = 0;   // 0: derived (gauss4 -> 1, gauss2 -> 4 on this grid)
};

Metrics RunChain(const Phantom& p, const Config& c, std::vector<double>* outImg = nullptr,
            bool collimate = true) {
    const vg::ParamTable t = McTable(c.gauss2);
    std::vector<double> img = collimate ? Collimated(p) : p.total;
    vg::VgSettings st;
    st.kvp = kKvp;
    st.gridRatio = 100;
    st.pixelPitchMm = kPitchMm;
    st.airSignal = kAirDn;
    st.iterations = c.iterations;
    vg::VgSwitches sw;
    sw.cap = c.cap;
    sw.reductionFactor = c.factor;
    const vg::VgReport rep = vg::RunVirtualGrid(img, kN, kN, t, st, sw);
    Metrics m;
    if (!rep.error.empty()) { m.error = rep.error; return m; }
    m = Measure(img, p);
    m.factor = rep.factor;
    m.capped = rep.cappedFraction;
    if (outImg) *outImg = img;
    return m;
}

// Mean ratio per thickness band: nominal +-0.5 cm for the step, 5 cm bins for
// the wedge.
void LogBands(const char* phantom, const char* what, const std::vector<double>& img, const Phantom& p,
              bool step) {
    std::string line = std::string("VGMC ") + phantom + " bands " + what + ":";
    for (int nominal = 5; nominal <= 25; nominal += 5) {
        double sum = 0;
        int n = 0;
        for (int i = 0; i < kN * kN; ++i) {
            if (!InRegion(i) || p.primary[static_cast<size_t>(i)] <= 0) continue;
            const double t = p.thickness[static_cast<size_t>(i)];
            const bool in = step ? std::fabs(t - nominal) < 0.5 : (t >= nominal && t < nominal + 5);
            if (!in) continue;
            sum += img[static_cast<size_t>(i)] / p.primary[static_cast<size_t>(i)];
            ++n;
        }
        char buf[64];
        if (n) std::snprintf(buf, sizeof(buf), " %s%d=%.3f(n%d)", step ? "" : ">=", nominal, sum / n, n);
        else std::snprintf(buf, sizeof(buf), " %s%d=-", step ? "" : ">=", nominal);
        line += buf;
    }
    std::printf("%s\n", line.c_str());
}

void LogProfile(const char* phantom, const char* what, const std::vector<double>& img, const Phantom& p) {
    std::string line = std::string("VGMC ") + phantom + " profile " + what + " (cols " +
                       std::to_string(kLo) + ".." + std::to_string(kHi - 1) + ", row mean):";
    for (int c = kLo; c < kHi; ++c) {
        double sum = 0;
        int n = 0;
        for (int r = kLo; r < kHi; ++r) {
            const size_t i = static_cast<size_t>(r) * kN + static_cast<size_t>(c);
            if (p.primary[i] <= 0) continue;
            sum += img[i] / p.primary[i];
            ++n;
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), " %.3f", n ? sum / n : 0.0);
        line += buf;
    }
    std::printf("%s\n", line.c_str());
}

void LogMetrics(const char* phantom, const char* what, const Metrics& m) {
    if (!m.error.empty()) {
        std::printf("VGMC %s %s error='%s'\n", phantom, what, m.error.c_str());
        return;
    }
    std::printf("VGMC %s %s factor=%d ratio median=%.4f min=%.4f max=%.4f |r-1| median=%.4f capped=%.4f\n",
                phantom, what, m.factor, m.median, m.lo, m.hi, m.absMedian, m.capped);
}

const Config kBase{"C2.it5.g4", vg::CapMode::GlobalSum, 5, false};

}  // namespace

TEST(GsvgVirtualGridMc, DataAndConditions)
{
    const Phantom s = Load("step");
    // Orientation: thickness changes along columns (x), not along rows (z).
    EXPECT_LT(std::fabs(s.thickness[8 * kN + 40] - s.thickness[72 * kN + 40]), 0.5);
    EXPECT_GT(std::fabs(s.thickness[40 * kN + 8] - s.thickness[40 * kN + 71]), 15.0);
    // Field: rows/cols 3..76; the metric region lies inside it.
    const double centre = s.air[40 * kN + 40];
    EXPECT_NEAR(centre, kAirDn, 1e-3 * kAirDn);   // json dn_air is the centre-region mean
    for (int k : {kLo, kHi - 1}) {
        EXPECT_GT(s.air[static_cast<size_t>(k) * kN + 40], 0.9 * centre);
        EXPECT_GT(s.air[40 * kN + static_cast<size_t>(k)], 0.9 * centre);
    }
    EXPECT_LT(s.air[40 * kN + 1], 0.5 * centre);
    // Visible step thicknesses: 5..25 cm; 30 cm lies outside the field.
    double tMax = 0;
    for (int i = 0; i < kN * kN; ++i)
        if (InRegion(i)) tMax = std::max(tMax, s.thickness[static_cast<size_t>(i)]);
    std::printf("VGMC step region max thickness=%.2f air edge/centre=%.4f\n", tMax,
                s.air[40 * kN + static_cast<size_t>(kLo)] / centre);
    EXPECT_LT(tMax, 27.0);
    // Scatter outside the field in `total`.
    std::printf("VGMC step total outside field (row 40, col 0)=%.0f DN\n", s.total[40 * kN]);
    // 80 x 80 at 4 mm reduces by factor 1: the chain runs on the full grid.
    Metrics m = RunChain(s, kBase);
    EXPECT_EQ(m.error, "");
    EXPECT_EQ(m.factor, 1);
}

TEST(GsvgVirtualGridMc, CompareToPrimary)
{
    const Config configs[] = {
        kBase,
        {"C0.it5.g4", vg::CapMode::None, 5, false},
        {"C2.it1.g4", vg::CapMode::GlobalSum, 1, false},
        {"C2.it3.g4", vg::CapMode::GlobalSum, 3, false},
        {"C2.it10.g4", vg::CapMode::GlobalSum, 10, false},
        {"C2.it5.g2", vg::CapMode::GlobalSum, 5, true},
        // gauss2 changes the derived reduction factor as well (1 -> 4); these
        // two separate the kernel model from the grid.
        {"C2.it5.g2.f1", vg::CapMode::GlobalSum, 5, true, 1},
        {"C2.it5.g4.f4", vg::CapMode::GlobalSum, 5, false, 4},
    };
    for (const char* name : {"step", "wedge"}) {
        SCOPED_TRACE(name);
        const Phantom p = Load(name);
        const bool step = std::string(name) == "step";

        // No correction: total / primary.
        const Metrics none = Measure(Collimated(p), p);
        LogMetrics(name, "none", none);
        LogBands(name, "none", Collimated(p), p, step);

        for (const Config& c : configs) {
            std::vector<double> img;
            const Metrics m = RunChain(p, c, &img);
            LogMetrics(name, c.name, m);
            ASSERT_EQ(m.error, "") << c.name;
            LogBands(name, c.name, img, p, step);
            if (&c == &configs[0]) LogProfile(name, c.name, img, p);
            // Loose bound: every configuration is closer to the primary than
            // no correction at all.
            EXPECT_LT(m.absMedian, none.absMedian) << c.name;
        }

        // Without the collimation mask (log only).
        std::vector<double> img;
        const Metrics raw = RunChain(p, kBase, &img, false);
        LogMetrics(name, "C2.it5.g4.nomask", raw);
    }
}

// ---------------------------------------------------------------------------
// QA-B-96: the caller's collimation field mask.
// ---------------------------------------------------------------------------
namespace {

std::vector<uint8_t> FieldMask(const Phantom& p) {
    const double centre = p.air[40 * kN + 40];
    std::vector<uint8_t> m(p.air.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = p.air[i] >= 0.5 * centre ? 1 : 0;
    return m;
}

vg::VgSettings McSettings() {
    vg::VgSettings st;
    st.kvp = kKvp;
    st.gridRatio = 100;
    st.pixelPitchMm = kPitchMm;
    st.airSignal = kAirDn;
    st.iterations = 5;
    return st;
}

}  // namespace

// With the mask, the chain gives inside the field exactly what the QA-B-95
// zeroed input gave, and leaves the outside untouched.
TEST(GsvgVirtualGridMcMask, MaskEqualsZeroedInput)
{
    for (const char* name : {"step", "wedge"}) {
        SCOPED_TRACE(name);
        const Phantom p = Load(name);
        const vg::ParamTable t = McTable(false);
        const std::vector<uint8_t> mask = FieldMask(p);

        std::vector<double> zeroed = Collimated(p);
        ASSERT_EQ(vg::RunVirtualGrid(zeroed, kN, kN, t, McSettings()).error, "");
        std::vector<double> masked = p.total;
        ASSERT_EQ(vg::RunVirtualGrid(masked, kN, kN, t, McSettings(), vg::VgSwitches{}, mask.data()).error, "");

        size_t inside = 0, outside = 0;
        for (size_t i = 0; i < masked.size(); ++i) {
            if (mask[i]) { ASSERT_EQ(masked[i], zeroed[i]) << i; ++inside; }
            else { ASSERT_EQ(masked[i], p.total[i]) << i; ++outside; }
        }
        const Metrics mm = Measure(masked, p), mz = Measure(zeroed, p);
        std::printf("VGMC %s mask inside=%zu outside=%zu median masked=%.4f zeroed=%.4f\n",
                    name, inside, outside, mm.median, mz.median);
        EXPECT_EQ(mm.median, mz.median);
        EXPECT_GT(outside, 0u);
    }
}

// QA-B-95 observation, recorded: without a mask the scatter outside the
// field is read as object and too much is subtracted near it.
TEST(GsvgVirtualGridMcMask, NoMaskOverSubtracts)
{
    const struct { const char* name; double min; } expected[] = {{"step", 0.7275}, {"wedge", 0.7694}};
    for (const auto& e : expected) {
        SCOPED_TRACE(e.name);
        const Phantom p = Load(e.name);
        std::vector<double> img = p.total;
        ASSERT_EQ(vg::RunVirtualGrid(img, kN, kN, McTable(false), McSettings()).error, "");
        const Metrics m = Measure(img, p);
        std::printf("VGMC %s nomask min=%.4f median=%.4f\n", e.name, m.lo, m.median);
        EXPECT_NEAR(m.lo, e.min, 5e-4);
        EXPECT_LT(m.lo, Measure(Collimated(p), p).lo);   // below anything the masked run can reach
    }
}

// Falsification: a chain that receives the mask but ignores it is the
// no-mask chain, and fails the equality of MaskEqualsZeroedInput.
TEST(GsvgVirtualGridMcMask, IgnoringTheMaskBreaksTheEquality)
{
    const Phantom p = Load("step");
    const vg::ParamTable t = McTable(false);
    const std::vector<uint8_t> mask = FieldMask(p);
    std::vector<double> zeroed = Collimated(p);
    ASSERT_EQ(vg::RunVirtualGrid(zeroed, kN, kN, t, McSettings()).error, "");
    vg::VgSwitches ignore;
    ignore.useFieldMask = false;
    std::vector<double> ignored = p.total;
    ASSERT_EQ(vg::RunVirtualGrid(ignored, kN, kN, t, McSettings(), ignore, mask.data()).error, "");
    std::vector<double> none = p.total;
    ASSERT_EQ(vg::RunVirtualGrid(none, kN, kN, t, McSettings()).error, "");
    EXPECT_EQ(ignored, none);
    size_t differ = 0;
    for (size_t i = 0; i < ignored.size(); ++i)
        if (mask[i] && ignored[i] != zeroed[i]) ++differ;
    std::printf("VGMC step ignored-mask pixels differing from zeroed (inside field)=%zu\n", differ);
    EXPECT_GT(differ, 1000u);
    EXPECT_NE(Measure(ignored, p).median, Measure(zeroed, p).median);
}

// Public ABI: xpe_gsvg_process_masked on the uint16 image.
TEST(GsvgVirtualGridMcMask, PublicEntryPoint)
{
    const Phantom p = Load("step");
    const std::vector<uint8_t> mask = FieldMask(p);

    // table file: MC kernels + [wet] + ideal grid
    const auto dir = std::filesystem::temp_directory_path() / "xpe_gsvg_qa_b96";
    std::filesystem::create_directories(dir);
    const auto tablePath = dir / "mc_table.csv";
    {
        std::ofstream f(tablePath, std::ios::binary);
        f << "[kernels]\n" << ReadText(std::string(kDir) + "scatter_kernels_water_csi600.csv")
          << "\n[wet]\n" << ReadText(std::string(kDir) + "wet_water_csi600.csv")
          << "\n[grid]\nratio,tp,ts\n100,1,0\n";
    }
    std::string path = tablePath.generic_string();
    const std::string cfg = "{\"virtual_grid\": true, \"vg_table_path\": \"" + path +
        "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 100, \"vg_pixel_pitch_mm\": 4.0,"
        " \"vg_air_signal\": 50000, \"vg_iterations\": 5}";

    std::vector<uint16_t> src(p.total.size()), srcZeroed(p.total.size());
    for (size_t i = 0; i < src.size(); ++i) {
        src[i] = static_cast<uint16_t>(std::lround(std::min(p.total[i], 65535.0)));
        srcZeroed[i] = mask[i] ? src[i] : 0;
    }
    const size_t n = src.size();

    auto warnings = [] {
        int count = 0;
        for (int i = 0; i < 64; ++i) {
            char msg[512];
            int32_t sev = 0;
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) break;
            if (std::string(msg).find("no collimation field mask") != std::string::npos) ++count;
        }
        return count;
    };

    void* h = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&h, cfg.c_str()), XPE_OK) << cfg;

    xpe_clear_alerts();
    std::vector<uint16_t> masked(n), zeroed(n);
    ASSERT_EQ(xpe_gsvg_process_masked(h, src.data(), n, masked.data(), n, kN, kN, nullptr, 0,
                                      mask.data(), mask.size()), XPE_OK);
    EXPECT_EQ(warnings(), 0);
    ASSERT_EQ(xpe_gsvg_process(h, srcZeroed.data(), n, zeroed.data(), n, kN, kN, nullptr, 0), XPE_OK);
    EXPECT_EQ(warnings(), 1);   // the unmasked entry point warns
    size_t changed = 0;
    for (size_t i = 0; i < n; ++i) {
        if (mask[i]) { ASSERT_EQ(masked[i], zeroed[i]) << i; changed += masked[i] != src[i]; }
        else ASSERT_EQ(masked[i], src[i]) << i;
    }
    EXPECT_GT(changed, n / 2);

    // A NULL mask through the new entry point: same as xpe_gsvg_process. This
    // handle has warned already, so neither call warns again (QA-B-98).
    xpe_clear_alerts();
    std::vector<uint16_t> viaMasked(n), viaPlain(n);
    ASSERT_EQ(xpe_gsvg_process_masked(h, src.data(), n, viaMasked.data(), n, kN, kN, nullptr, 0, nullptr, 0), XPE_OK);
    ASSERT_EQ(xpe_gsvg_process(h, src.data(), n, viaPlain.data(), n, kN, kN, nullptr, 0), XPE_OK);
    EXPECT_EQ(viaMasked, viaPlain);
    EXPECT_EQ(warnings(), 0);

    // A mask shorter than the image.
    EXPECT_EQ(xpe_gsvg_process_masked(h, src.data(), n, viaMasked.data(), n, kN, kN, nullptr, 0,
                                      mask.data(), n - 1), XPE_ERR_INVALID_INPUT);
    xpe_gsvg_shutdown(h);

    // A new handle (re-init with the same configuration) warns once more.
    ASSERT_EQ(xpe_gsvg_init(&h, cfg.c_str()), XPE_OK);
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(xpe_gsvg_process_masked(h, src.data(), n, viaMasked.data(), n, kN, kN, nullptr, 0, nullptr, 0), XPE_OK);
    EXPECT_EQ(warnings(), 1);
    xpe_gsvg_shutdown(h);

    // Virtual grid off: no warning, the mask changes nothing.
    xpe_clear_alerts();
    ASSERT_EQ(xpe_gsvg_init(&h, nullptr), XPE_OK);
    std::vector<uint16_t> off(n);
    ASSERT_EQ(xpe_gsvg_process_masked(h, src.data(), n, off.data(), n, kN, kN, nullptr, 0, mask.data(), n), XPE_OK);
    EXPECT_EQ(off, src);
    EXPECT_EQ(warnings(), 0);
    xpe_gsvg_shutdown(h);
    xpe_clear_alerts();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
