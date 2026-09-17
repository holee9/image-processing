/**
 * @file test_e2e_collimation_mask.cpp
 * @brief #180 (QA-B-97): collimation detection -> field mask -> virtual grid.
 *
 * gsvg depends on no other XPE module (SPEC-XPE-GSVG section 1), so the
 * rectangle from enhance_advanced is turned into gsvg's byte mask here, on the
 * pipeline side.
 *
 * Rectangle convention, read from the implementation
 * (modules/enhance_advanced/src/collimation_detect.cpp): x0..x1 and y0..y1
 * are INCLUSIVE pixel indices -- the low-confidence fallback returns
 * [0, width-1] x [0, height-1], and the area check uses x1 - x0 + 1. The
 * header does not say so.
 *
 * Also measured here (QA-B-97):
 *  - what 1000 unmasked calls do to the shared alert queue (64 entries,
 *    xpe_common.cpp);
 *  - the brightness step across the mask edge on the MC step phantom (log only).
 */

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/gsvg/gsvg_api.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// XPE_GSVG_TEST_DATA: modules/gsvg/tests/data (set by CMake).
std::string DataPath(const char* rel) { return std::string(XPE_GSVG_TEST_DATA) + "/" + rel; }

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Rect { int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0; };

// The conversion helper: inclusive rectangle -> row-major byte mask.
std::vector<uint8_t> MaskFromRect(const Rect& r, int width, int height) {
    std::vector<uint8_t> m(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    const int x0 = std::max(0, r.x0), x1 = std::min(width - 1, r.x1);
    const int y0 = std::max(0, r.y0), y1 = std::min(height - 1, r.y1);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            m[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = 1;
    return m;
}

std::string WriteConfig(const std::string& tableText, const std::string& tag, double pitchMm,
                        double airDn, double ratio) {
    const auto dir = std::filesystem::temp_directory_path() / ("xpe_e2e_qa_b97_" + tag);
    std::filesystem::create_directories(dir);
    const auto path = dir / "table.csv";
    std::ofstream(path, std::ios::binary) << tableText;
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "{\"virtual_grid\": true, \"vg_table_path\": \"%s\", \"vg_kvp\": 80, "
                  "\"vg_grid_ratio\": %g, \"vg_pixel_pitch_mm\": %g, \"vg_air_signal\": %g, "
                  "\"vg_iterations\": 5}",
                  path.generic_string().c_str(), ratio, pitchMm, airDn);
    return buf;
}

// Synthetic collimated scene: a 256 x 256 frame whose field is the inclusive
// rectangle kField. Inside: an object (bright left, darker right, a dense
// disk). Outside: scatter only, falling off away from the field edge.
constexpr int kW = 256, kH = 256;
constexpr Rect kField{40, 30, 215, 225};

// Hand-made truth: the scene's own in-field predicate, not the helper.
bool InField(int x, int y) {
    return x >= kField.x0 && x <= kField.x1 && y >= kField.y0 && y <= kField.y1;
}

std::vector<uint8_t> HandMadeMask() {
    std::vector<uint8_t> m(static_cast<size_t>(kW) * kH);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x)
            m[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = InField(x, y) ? 1 : 0;
    return m;
}

// stepInside: the object carries a vertical brightness step at x = 128 (a
// strong interior edge). Without it the object is uniform apart from a disk.
std::vector<float> SyntheticScene(bool stepInside) {
    std::vector<float> img(static_cast<size_t>(kW) * kH);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            const bool in = InField(x, y);
            double v;
            if (in) {
                v = (stepInside && x >= 128) ? 12000.0 : 30000.0;
                const double dx = x - 160.0, dy = y - 128.0;
                if (dx * dx + dy * dy < 400.0) v *= 0.6;
            } else {
                const double dxo = std::max({0, kField.x0 - x, x - kField.x1});
                const double dyo = std::max({0, kField.y0 - y, y - kField.y1});
                v = 2500.0 * std::exp(-std::sqrt(dxo * dxo + dyo * dyo) / 30.0);
            }
            img[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = static_cast<float>(v);
        }
    return img;
}

Rect Detect(std::vector<float>& img, int w, int h) {
    XpeImageBuffer buf{};
    buf.width = static_cast<uint32_t>(w);
    buf.height = static_cast<uint32_t>(h);
    buf.bitsAllocated = 32;
    buf.bitsStored = 32;
    buf.format = XPE_PIXEL_FLOAT32;
    buf.data = img.data();
    buf.dataSize = img.size() * sizeof(float);
    Rect r;
    EXPECT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK);
    EXPECT_EQ(xpe_detect_collimation(&buf, &r.x0, &r.y0, &r.x1, &r.y1, nullptr), XPE_OK);
    xpe_enhance_advanced_shutdown();
    return r;
}

std::vector<uint16_t> ToU16(const std::vector<float>& v) {
    std::vector<uint16_t> o(v.size());
    for (size_t i = 0; i < v.size(); ++i)
        o[i] = static_cast<uint16_t>(std::lround(std::clamp(static_cast<double>(v[i]), 0.0, 65535.0)));
    return o;
}

std::vector<uint16_t> ProcessMasked(const std::string& cfg, const std::vector<uint16_t>& src,
                                    int w, int h, const std::vector<uint8_t>* mask) {
    void* handle = nullptr;
    EXPECT_EQ(xpe_gsvg_init(&handle, cfg.c_str()), XPE_OK) << cfg;
    std::vector<uint16_t> dst(src.size());
    EXPECT_EQ(xpe_gsvg_process_masked(handle, src.data(), src.size(), dst.data(), dst.size(), w, h,
                                      nullptr, 0, mask ? mask->data() : nullptr,
                                      mask ? mask->size() : 0), XPE_OK);
    xpe_gsvg_shutdown(handle);
    return dst;
}

}  // namespace

// The helper's boundary: every corner of the inclusive rectangle is inside,
// the line just beyond each side is outside.
TEST(CollimationMaskE2E, HelperIsInclusive)
{
    const Rect r{3, 5, 10, 12};
    const auto m = MaskFromRect(r, 16, 20);
    auto at = [&](int x, int y) { return m[static_cast<size_t>(y) * 16 + static_cast<size_t>(x)]; };
    EXPECT_EQ(at(3, 5), 1);   EXPECT_EQ(at(10, 12), 1);
    EXPECT_EQ(at(3, 12), 1);  EXPECT_EQ(at(10, 5), 1);
    EXPECT_EQ(at(2, 8), 0);   EXPECT_EQ(at(11, 8), 0);
    EXPECT_EQ(at(6, 4), 0);   EXPECT_EQ(at(6, 13), 0);
    size_t n = 0;
    for (uint8_t v : m) n += v;
    EXPECT_EQ(n, static_cast<size_t>((10 - 3 + 1) * (12 - 5 + 1)));
    // the detector's own fallback, [0, w-1] x [0, h-1], covers the whole frame
    const auto full = MaskFromRect(Rect{0, 0, 15, 19}, 16, 20);
    EXPECT_EQ(std::count(full.begin(), full.end(), uint8_t{1}), 16 * 20);
}

namespace {

struct ChainResult { Rect det; size_t maskDiff = 0, outDiff = 0, noneDiff = 0; };

// Detection -> helper mask -> virtual grid, against the hand-made mask.
ChainResult RunChain(bool stepInside) {
    std::vector<float> scene = SyntheticScene(stepInside);
    const std::vector<uint16_t> src = ToU16(scene);
    ChainResult r;
    r.det = Detect(scene, kW, kH);
    const std::string cfg = WriteConfig(ReadText(DataPath("virtual_grid_synthetic_table.csv")),
                                        "synthetic", 1.0, 60000.0, 100.0);
    const auto truthMask = HandMadeMask();
    const auto detMask = MaskFromRect(r.det, kW, kH);
    for (size_t i = 0; i < truthMask.size(); ++i) r.maskDiff += truthMask[i] != detMask[i];
    const auto withTruth = ProcessMasked(cfg, src, kW, kH, &truthMask);
    const auto withDet = ProcessMasked(cfg, src, kW, kH, &detMask);
    const auto withNone = ProcessMasked(cfg, src, kW, kH, nullptr);
    int maxDiff = 0;
    double sumDiff = 0;
    for (size_t i = 0; i < src.size(); ++i) {
        r.outDiff += withTruth[i] != withDet[i];
        r.noneDiff += withTruth[i] != withNone[i];
        const int d = std::abs(static_cast<int>(withTruth[i]) - static_cast<int>(withDet[i]));
        maxDiff = std::max(maxDiff, d);
        sumDiff += d;
    }
    std::printf("COLLMASK scene=%s truth x0=%d y0=%d x1=%d y1=%d detected x0=%d y0=%d x1=%d y1=%d "
                "mask pixels differing=%zu output pixels differing=%zu (no mask: %zu) "
                "|diff| max=%d mean=%.2f DN\n",
                stepInside ? "step-inside" : "uniform", kField.x0, kField.y0, kField.x1, kField.y1,
                r.det.x0, r.det.y0, r.det.x1, r.det.y1, r.maskDiff, r.outDiff, r.noneDiff,
                maxDiff, sumDiff / static_cast<double>(src.size()));
    return r;
}

}  // namespace

// The conversion: the helper's mask for the true rectangle is the hand-made
// mask, and the virtual grid gives the same image with either.
TEST(CollimationMaskE2E, HelperMaskEqualsHandMadeMask)
{
    const std::vector<float> scene = SyntheticScene(false);
    const std::vector<uint16_t> src = ToU16(scene);
    const auto hand = HandMadeMask();
    const auto helper = MaskFromRect(kField, kW, kH);
    ASSERT_EQ(helper, hand);
    const std::string cfg = WriteConfig(ReadText(DataPath("virtual_grid_synthetic_table.csv")),
                                        "synthetic", 1.0, 60000.0, 100.0);
    const auto a = ProcessMasked(cfg, src, kW, kH, &hand);
    const auto b = ProcessMasked(cfg, src, kW, kH, &helper);
    const auto none = ProcessMasked(cfg, src, kW, kH, nullptr);
    EXPECT_EQ(a, b);
    EXPECT_NE(a, none);   // control: the mask matters on this scene
}

// Recorded, not accepted (QA-B-97): on the uniform scene the detector finds
// three sides exactly and puts the TOP side one line outside (y0 = 29 for a
// field starting at row 30). Shifting the field by 1..3 px gave y0 one line
// outside three times and two lines once (_probe.log); the other sides were
// exact every time. The values pin today's behaviour.
TEST(CollimationMaskE2E, KnownDivergence_DetectorTopSideOneLineOutside)
{
    const ChainResult r = RunChain(false);
    EXPECT_EQ(r.det.x0, kField.x0);
    EXPECT_EQ(r.det.x1, kField.x1);
    EXPECT_EQ(r.det.y1, kField.y1);
    EXPECT_EQ(r.det.y0, kField.y0 - 1);
    // exactly the row y = 29 across the field width
    EXPECT_EQ(r.maskDiff, static_cast<size_t>(kField.x1 - kField.x0 + 1));
    EXPECT_GT(r.outDiff, 0u);
    EXPECT_GT(r.noneDiff, r.outDiff);   // still far closer than no mask
}

// Recorded, not accepted (QA-B-97): with a strong edge INSIDE the object the
// detector takes that edge for a field side, and the top side lands one line
// outside. The values pin today's behaviour so that a fix shows up here.
TEST(CollimationMaskE2E, KnownDivergence_InteriorEdgeTakenForFieldSide)
{
    const ChainResult r = RunChain(true);
    EXPECT_EQ(r.det.x0, 40);
    EXPECT_EQ(r.det.y0, 29);    // truth 30
    EXPECT_EQ(r.det.x1, 127);   // truth 215: the interior step at x = 128
    EXPECT_EQ(r.det.y1, 225);
    EXPECT_GT(r.maskDiff, 0u);
    EXPECT_GT(r.outDiff, 0u);
}

// 1000 unmasked calls with the virtual grid on: what the 64-entry queue keeps.
TEST(CollimationMaskE2E, UnmaskedWarningFlood)
{
    const std::string cfg = WriteConfig(ReadText(DataPath("virtual_grid_synthetic_table.csv")),
                                        "flood", 1.0, 60000.0, 100.0);
    std::vector<uint16_t> src(64 * 64, 30000), dst(src.size());
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, cfg.c_str()), XPE_OK);

    xpe_clear_alerts();
    xpe_alert_push("other module: info before the flood", XPE_ALERT_INFO);
    xpe_alert_push("other module: warning before the flood", XPE_ALERT_WARNING);
    xpe_alert_push("other module: error before the flood", XPE_ALERT_ERROR);
    constexpr int kCalls = 1000;
    for (int i = 0; i < kCalls; ++i)
        ASSERT_EQ(xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), 64, 64,
                                   nullptr, 0), XPE_OK);
    xpe_gsvg_shutdown(handle);

    int total = 0, gsvgWarn = 0, info = 0, otherWarn = 0, otherErr = 0, loss = 0;
    std::string lossText;
    for (int i = 0; i < 128; ++i) {
        char msg[512];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) break;
        ++total;
        const std::string s(msg);
        if (s.find("no collimation field mask") != std::string::npos) ++gsvgWarn;
        else if (s.find("info before") != std::string::npos) ++info;
        else if (s.find("warning before") != std::string::npos) ++otherWarn;
        else if (s.find("error before") != std::string::npos) ++otherErr;
        else if (s.find("alert queue overflow") != std::string::npos) { ++loss; lossText = s; }
    }
    std::printf("COLLMASK flood calls=%d queued=%d gsvgWarnings=%d info=%d otherWarning=%d "
                "otherError=%d lossAlerts=%d '%s' pending_count=%d\n",
                kCalls, total, gsvgWarn, info, otherWarn, otherErr, loss, lossText.c_str(),
                xpe_get_pending_alert_count());
    // Current behaviour, recorded (api-spec 5.17 eviction order).
    EXPECT_EQ(total, 64);
    EXPECT_EQ(otherErr, 1);      // errors are evicted last
    EXPECT_EQ(otherWarn, 0);     // an older warning from another module is gone
    EXPECT_EQ(info, 0);
    EXPECT_EQ(loss, 1);
    EXPECT_EQ(gsvgWarn, 62);
    xpe_clear_alerts();
}

// Brightness across the mask edge on the MC step phantom (log only).
TEST(CollimationMaskE2E, EdgeStepOnMcPhantom)
{
    constexpr int n = 80;
    constexpr double scale = 6955.33056938879;
    auto readF32 = [](const std::string& p) {
        const std::string raw = ReadText(p);
        std::vector<float> v(raw.size() / 4);
        std::memcpy(v.data(), raw.data(), v.size() * 4);
        return v;
    };
    std::vector<float> total = readF32(DataPath("mc/step_80kVp_total.f32"));
    const std::vector<float> air = readF32(DataPath("mc/step_80kVp_air.f32"));
    ASSERT_EQ(total.size(), static_cast<size_t>(n * n));
    for (float& v : total) v = static_cast<float>(v * scale);

    // Mask from the collimation detector on the phantom itself.
    std::vector<float> forDetect = total;
    const Rect det = Detect(forDetect, n, n);
    const auto mask = MaskFromRect(det, n, n);
    int airIn = 0;
    for (size_t i = 0; i < air.size(); ++i) airIn += air[i] * scale >= 25000.0;
    std::printf("COLLMASK mc detected x0=%d y0=%d x1=%d y1=%d (air-based field pixels=%d, mask pixels=%d)\n",
                det.x0, det.y0, det.x1, det.y1, airIn,
                static_cast<int>(std::count(mask.begin(), mask.end(), uint8_t{1})));

    std::vector<uint8_t> airMask(air.size());
    for (size_t i = 0; i < air.size(); ++i) airMask[i] = air[i] * scale >= 25000.0 ? 1 : 0;

    std::string table = "[kernels]\n" + ReadText(DataPath("mc/scatter_kernels_water_csi600.csv")) +
                        "\n[wet]\n" + ReadText(DataPath("mc/wet_water_csi600.csv")) +
                        "\n[grid]\nratio,tp,ts\n100,1,0\n";
    const std::string cfg = WriteConfig(table, "mc", 4.0, 50000.0, 100.0);
    const std::vector<uint16_t> src = ToU16(total);
    const auto out = ProcessMasked(cfg, src, n, n, &mask);
    const auto outAir = ProcessMasked(cfg, src, n, n, &airMask);

    // Mean of the last row/column inside the field and the first outside it,
    // along each side, rows/cols 20..59 (away from the corners).
    auto meanAt = [&](const std::vector<uint16_t>& img, bool vertical, int line) {
        double s = 0;
        for (int k = 20; k < 60; ++k)
            s += vertical ? img[static_cast<size_t>(k) * n + static_cast<size_t>(line)]
                          : img[static_cast<size_t>(line) * n + static_cast<size_t>(k)];
        return s / 40.0;
    };
    struct Side { const char* name; bool vertical; int inside, outside; };
    auto logEdges = [&](const char* which, const std::vector<uint16_t>& img, const Rect& f) {
        const Side sides[] = {{"left", true, f.x0, f.x0 - 1}, {"right", true, f.x1, f.x1 + 1},
                              {"top", false, f.y0, f.y0 - 1}, {"bottom", false, f.y1, f.y1 + 1}};
        for (const Side& s : sides) {
            if (s.outside < 0 || s.outside >= n) {
                std::printf("COLLMASK mc %s edge %s: no pixel outside the mask\n", which, s.name);
                continue;
            }
            std::printf("COLLMASK mc %s edge %s inside=%.1f outside=%.1f (input inside=%.1f outside=%.1f)\n",
                        which, s.name, meanAt(img, s.vertical, s.inside), meanAt(img, s.vertical, s.outside),
                        meanAt(src, s.vertical, s.inside), meanAt(src, s.vertical, s.outside));
        }
    };
    logEdges("detected-mask", out, det);
    // air-based field: rows/cols 3..76
    int ax0 = n, ay0 = n, ax1 = -1, ay1 = -1;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            if (airMask[static_cast<size_t>(y) * n + static_cast<size_t>(x)]) {
                ax0 = std::min(ax0, x); ax1 = std::max(ax1, x);
                ay0 = std::min(ay0, y); ay1 = std::max(ay1, y);
            }
    std::printf("COLLMASK mc air-mask x0=%d y0=%d x1=%d y1=%d\n", ax0, ay0, ax1, ay1);
    logEdges("air-mask", outAir, Rect{ax0, ay0, ax1, ay1});
    SUCCEED();
}
