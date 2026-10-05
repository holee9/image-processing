/**
 * @file test_zz_a241_measure.cpp
 * @brief QA-A-241 (#245): measurements behind the proposal for the out-of-range gain fraction limit (SRS-CALIB-FUNC-002).
 *        DISABLED_ (run with --gtest_also_run_disabled_tests); prints `[a241]` lines, asserts only what is a fact of the run.
 *
 *   B2_LimitBoundary      the rule at the limit: below it a map loads (classified + warning), above it it is refused
 *   B2_FractionCost       frame time and the defect stage's behaviour as the classified fraction grows, on the real 3072x3072
 *                         maps (XPE_A240_OUT/maps, made by test_zz_a240_checklist) with random extra defects
 *   B6_HeldOut            (QA-A-241b) the stage-1 checklist item B6 with the method fixed in
 *                         .moai/reports/lane-pre/QA-A-241b/evidence/00_b6_method_and_predictions_before_measuring.md:
 *                         gain from two flats of ONE acquisition condition, measured on the third, shipping path
 *                         (xpe_preprocess_pipeline_out), after the defect stage, defect pixels left out.
 *                         Needs XPE_A240_CAL (the CalData_6 folder) and XPE_A241_OUT (a scratch folder).
 *   B6_Protocol53         (QA-A-241c) B6 exactly as the approved evaluation protocol section 5.3 defines it; the method is fixed in
 *                         .moai/reports/lane-pre/QA-A-241c/evidence/00_b6_method_protocol_5_3.md (committed before any result):
 *                         ROI = every pixel whose gain value is finite and > 0 (defect pixels included, no crop, no ADU floor), three
 *                         held-out combinations of flats 4,5,6, all three reported, shipping path final output (after the defect stage)
 *                         and, for reference, the output before the defect stage. Needs XPE_A240_CAL and XPE_A241_OUT.
 *   B6_Acceptance         (QA-A-241e) the other section 5.3 acceptance indicators, FPN_Reduction_dB and LineArtifactScore, on the same held-out
 *                         flats; the method is fixed in .moai/reports/lane-pre/QA-A-241e/evidence/00_b6_acceptance_method.md (committed before
 *                         any result). Starts with a hand-computed formula check. Needs XPE_A240_CAL and XPE_A241_OUT.
 *   B6_StripeInvestigation (QA-A-241f) why LineArtifactScore exceeds its limit at large tiles: numerator and denominator apart, profiles, noise
 *                         controls. Measurement only; method fixed in .moai/reports/lane-pre/QA-A-241f/evidence/00_stripe_investigation_method.md
 *                         (committed before any result). Needs XPE_A240_CAL and XPE_A241_OUT.
 *   B10v2_Baseline        (QA-A-243) the stage-2 input reference image: the real frame wrist_lat_3072x3072.raw through the shipping
 *                         path (xpe_preprocess_pipeline_out) with the gain made from ALL THREE flats of the one acquisition
 *                         condition (flats 4,5,6). Writes the float32 output. Needs XPE_A240_CAL, XPE_A240_WRIST, XPE_A243_OUT.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "xcal_writer.hpp"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/preprocess_api.h"

namespace fs = std::filesystem;

namespace {

std::string envOr(const char* n, const char* d) {
    char* v = nullptr;
    size_t len = 0;
    std::string r = (_dupenv_s(&v, &len, n) == 0 && v) ? std::string(v) : std::string(d);
    std::free(v);
    return r;
}

std::vector<unsigned char> readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

struct XCal {
    XCalFileHeader hdr{};
    std::vector<uint8_t> cfg, payload;
};

bool readXCal(const std::string& path, XCal* out) {
    const auto b = readFile(path);
    if (b.size() < sizeof(XCalFileHeader)) return false;
    std::memcpy(&out->hdr, b.data(), sizeof(XCalFileHeader));
    const size_t cfgAt = sizeof(XCalFileHeader), payAt = cfgAt + out->hdr.config_json_len;
    if (b.size() < payAt + out->hdr.payload_len) return false;
    out->cfg.assign(b.begin() + static_cast<std::ptrdiff_t>(cfgAt), b.begin() + static_cast<std::ptrdiff_t>(payAt));
    out->payload.assign(b.begin() + static_cast<std::ptrdiff_t>(payAt), b.begin() + static_cast<std::ptrdiff_t>(payAt + out->hdr.payload_len));
    return true;
}

XpeErrorCode writeXCal(const std::string& path, const XCal& x) {
    return write_xcal_file(path.c_str(), x.hdr, x.cfg.empty() ? nullptr : x.cfg.data(), x.cfg.size(), x.payload.data(), x.payload.size());
}

int countAlerts(const char* needle, std::string* first = nullptr) {
    int hits = 0;
    char msg[600];
    int32_t sev = 0;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i)
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK && std::strstr(msg, needle)) {
            if (!hits && first) *first = msg;
            ++hits;
        }
    return hits;
}

const char* name(int rc) {
    switch (rc) {
        case XPE_OK: return "XPE_OK";
        case XPE_ERR_INVALID_CALIB_DATA: return "XPE_ERR_INVALID_CALIB_DATA";
        case XPE_ERR_CONFIG_INVALID: return "XPE_ERR_CONFIG_INVALID";
        default: return "other";
    }
}

}  // namespace

// The limit is `count > 0.05 * total`. 256x256 = 65536 pixels: 3276 out-of-range pixels (4.9988 %) load, 3277 (5.0003 %) are refused.
TEST(A241Measure, DISABLED_B2_LimitBoundary) {
    const uint32_t S = 256;
    const size_t n = static_cast<size_t>(S) * S;
    const fs::path dir = fs::temp_directory_path() / "xpe_a241_limit";
    fs::create_directories(dir);
    struct Case { size_t bad; float value; const char* what; } cases[] = {
        {0, 0.f, "no out-of-range pixel"},
        {292, 0.0f, "0.445 % (the real CalData_6 gain map's fraction), value 0.0"},
        {3276, 0.05f, "3276 px = 4.9988 %, value 0.05"},
        {3277, 0.05f, "3277 px = 5.0003 %, value 0.05"},
        {3277, 20.0f, "3277 px, value 20.0 (above 10)"},
        {3277, std::numeric_limits<float>::quiet_NaN(), "3277 px, NaN"},
        {19661, 0.0f, "30 %, value 0.0"},
    };
    for (const auto& c : cases) {
        std::vector<float> g(n, 1.0f);
        for (size_t i = 0; i < c.bad; ++i) g[(i * 7919) % n] = c.value;   // 7919 is prime: distinct positions for i < n
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, "a241", 5);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_GAIN;
        hdr.pixel_format = XCAL_FMT_FLOAT32;
        hdr.width = S;
        hdr.height = S;
        hdr.payload_len = n * sizeof(float);
        const std::string p = (dir / "gain.xcal").string();
        ASSERT_EQ(XPE_OK, write_xcal_file(p.c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(g.data()), n * sizeof(float)));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_clear_alerts();
        const int rc = xpe_calib_load_gain(p.c_str());
        std::string first;
        const int cls = countAlerts("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT", &first);
        const int over = countAlerts("XPE_WARN_GAIN_PIXELS_OVER_LIMIT", cls ? nullptr : &first);
        std::printf("[a241] B2 %-62s load rc=%d %-28s classified-warning %d, over-limit-error %d\n", c.what, rc, name(rc), cls, over);
        if (over) std::printf("[a241]    alert: %.230s\n", first.c_str());
    }
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

// What a larger classified fraction costs: the frame time on the real maps with extra random defects added to the defect map.
TEST(A241Measure, DISABLED_B2_FractionCost) {
    const std::string maps = envOr("XPE_A240_OUT", "build/a240") + "/maps";
    XCal off, gain, def;
    ASSERT_TRUE(readXCal(maps + "/offset.xcal", &off));
    ASSERT_TRUE(readXCal(maps + "/gain.xcal", &gain));
    ASSERT_TRUE(readXCal(maps + "/defect.xcal", &def));
    const uint32_t W = 3072, H = 3072;
    const size_t N = static_cast<size_t>(W) * H;
    const fs::path dir = fs::temp_directory_path() / "xpe_a241_cost";
    fs::create_directories(dir);
    ASSERT_EQ(XPE_OK, writeXCal((dir / "offset.xcal").string(), off));
    ASSERT_EQ(XPE_OK, writeXCal((dir / "gain.xcal").string(), gain));

    const std::string wrist = envOr("XPE_A240_WRIST", "");
    std::vector<uint16_t> raw(N);
    {
        const auto b = readFile(wrist);
        ASSERT_EQ(N * sizeof(uint16_t), b.size());
        std::memcpy(raw.data(), b.data(), b.size());
    }
    const char* cfg = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
    for (const double extraPct : {0.0, 0.5, 1.0, 2.0, 5.0, 10.0}) {
        XCal d = def;
        uint64_t s = 88172645463325252ull;
        const size_t extra = static_cast<size_t>(extraPct / 100.0 * static_cast<double>(N));
        for (size_t i = 0; i < extra; ++i) {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            d.payload[static_cast<size_t>(s % N)] = 255;
        }
        size_t set = 0;
        for (const uint8_t v : d.payload) set += (v != 0);
        ASSERT_EQ(XPE_OK, writeXCal((dir / "defect.xcal").string(), d));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset((dir / "offset.xcal").string().c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain((dir / "gain.xcal").string().c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map((dir / "defect.xcal").string().c_str()));
        std::vector<double> ms;
        int rc = XPE_OK;
        xpe_clear_alerts();
        for (int rep = 0; rep < 5; ++rep) {
            std::vector<float> frame(N);
            std::memcpy(frame.data(), raw.data(), N * sizeof(uint16_t));
            XpeImageBuffer img{};
            img.data = frame.data();
            img.dataSize = N * sizeof(float);
            img.width = W;
            img.height = H;
            img.format = XPE_PIXEL_UINT16;
            img.bitsAllocated = 16;
            img.bitsStored = 16;
            XpeImageMetadata meta{};
            const auto t0 = std::chrono::steady_clock::now();
            rc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, cfg);
            ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            if (rc != XPE_OK) break;
        }
        std::sort(ms.begin(), ms.end());
        std::string first;
        const int unionWarn = countAlerts("XPE_WARN_DEFECT_UNION_OVER_LIMIT", &first);
        std::printf("[a241] B2 cost: defect map %zu px (%.3f %%) + gain-classified 42026 px: frame rc=%d, median %.1f ms (min %.1f, max %.1f), union-over-limit alerts %d\n",
                    set, 100.0 * static_cast<double>(set) / static_cast<double>(N), rc, ms[ms.size() / 2], ms.front(), ms.back(), unionWarn);
        if (unionWarn) std::printf("[a241]    alert: %.200s\n", first.c_str());
    }
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

namespace {
std::vector<uint16_t> readRaw16(const std::string& path, size_t n) {
    const auto b = readFile(path);
    std::vector<uint16_t> v;
    if (b.size() != n * sizeof(uint16_t)) return v;
    v.resize(n);
    std::memcpy(v.data(), b.data(), b.size());
    return v;
}
}  // namespace

// B6, held-out. Group = flats 4,5,6 (QA-A-242: the same large-scale shape). For each flat j of the group: gain from the other two
// (product generator, no dark reference), frame = dark + flat_j through the shipping path, residual after the defect stage over
// every pixel that is neither in the defect map nor classified defective by the gain map.
TEST(A241Measure, DISABLED_B6_HeldOut) {
    const std::string cal = envOr("XPE_A240_CAL", "");
    const std::string work = envOr("XPE_A241_OUT", "build/a241");
    ASSERT_FALSE(cal.empty());
    const uint32_t W = 3072, H = 3072;
    const size_t N = static_cast<size_t>(W) * H;
    const fs::path dir = fs::path(work) / "b6";
    fs::create_directories(dir);

    const std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw", N);
    ASSERT_FALSE(dark.empty());
    std::vector<std::vector<uint16_t>> flat(7);
    for (int k = 1; k <= 6; ++k) {
        char nm[32];
        std::snprintf(nm, sizeof nm, "/bright%02d.raw", k);
        flat[static_cast<size_t>(k)] = readRaw16(cal + nm, N);
        ASSERT_FALSE(flat[static_cast<size_t>(k)].empty()) << nm;
    }
    const auto bpBytes = readFile(cal + "/BPMap.map");
    ASSERT_EQ(N, bpBytes.size());

    auto buf = [&](const void* d, size_t bytes, XpePixelFormat f) {
        XpeImageBuffer b{};
        b.data = const_cast<void*>(d);
        b.dataSize = bytes;
        b.width = W;
        b.height = H;
        b.format = f;
        b.bitsAllocated = f == XPE_PIXEL_FLOAT32 ? 32u : 16u;
        b.bitsStored = b.bitsAllocated;
        return b;
    };

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string offPath = (dir / "offset.xcal").string(), defPath = (dir / "defect.xcal").string();
    const XpeImageBuffer darkBuf = buf(dark.data(), N * 2, XPE_PIXEL_UINT16);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&darkBuf, 1, 100.0f, 25.0f, offPath.c_str(), nullptr));
    {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        // the generated offset file's session id, so the three maps agree
        XCal o;
        ASSERT_TRUE(readXCal(offPath, &o));
        std::memcpy(hdr.session_id, o.hdr.session_id, sizeof hdr.session_id);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file(defPath.c_str(), hdr, nullptr, 0, bpBytes.data(), N));
    }
    const char* cfg = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";

    struct Held { int j, a, b; };
    const Held cases[] = {{6, 4, 5}, {5, 4, 6}, {4, 5, 6}};
    for (const Held& c : cases) {
        const std::string gainPath = (dir / ("gain_from_" + std::to_string(c.a) + "_" + std::to_string(c.b) + ".xcal")).string();
        const XpeImageBuffer two[2] = {buf(flat[static_cast<size_t>(c.a)].data(), N * 2, XPE_PIXEL_UINT16),
                                       buf(flat[static_cast<size_t>(c.b)].data(), N * 2, XPE_PIXEL_UINT16)};
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(two, 2, nullptr, gainPath.c_str(), nullptr));
        XCal g;
        ASSERT_TRUE(readXCal(gainPath, &g));
        const float* gv = reinterpret_cast<const float*>(g.payload.data());
        // the session id the generator gave the gain must agree with the other two: rewrite the file with the offset's id
        {
            XCal o;
            ASSERT_TRUE(readXCal(offPath, &o));
            std::memcpy(g.hdr.session_id, o.hdr.session_id, sizeof g.hdr.session_id);
            ASSERT_EQ(XPE_OK, writeXCal(gainPath, g));
        }
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
        xpe_clear_alerts();

        // the held-out flat as a raw frame: dark + flat (the flat files are already offset-corrected)
        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(dark[i] + flat[static_cast<size_t>(c.j)][i]);
        std::vector<float> out(N, 0.0f);
        const XpeImageBuffer in = buf(raw.data(), N * 2, XPE_PIXEL_UINT16);
        XpeImageBuffer ob = buf(out.data(), N * 4, XPE_PIXEL_FLOAT32);
        XpeImageMetadata meta{};
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &ob, &meta, nullptr, nullptr, cfg));

        // ROI: not in the defect map, not classified defective by this gain map
        std::vector<uint8_t> roi(N, 1);
        size_t masked = 0;
        for (size_t i = 0; i < N; ++i) {
            const bool cls = !(gv[i] >= 0.1f && gv[i] <= 10.0f);
            if (bpBytes[i] != 0 || cls) { roi[i] = 0; ++masked; }
        }
        double s1 = 0, s2 = 0;
        size_t n = 0;
        double inMean = 0;
        for (size_t i = 0; i < N; ++i) {
            if (!roi[i]) continue;
            s1 += out[i];
            s2 += static_cast<double>(out[i]) * out[i];
            inMean += flat[static_cast<size_t>(c.j)][i];
            ++n;
        }
        const double mean = s1 / static_cast<double>(n);
        const double sd = std::sqrt(std::max(0.0, s2 / static_cast<double>(n) - mean * mean));
        // the large-scale part: the spread of 16x16 block means (blocks with more than half their pixels in the ROI)
        double b1 = 0, b2 = 0;
        size_t nb = 0;
        for (uint32_t by = 0; by + 16 <= H; by += 16)
            for (uint32_t bx = 0; bx + 16 <= W; bx += 16) {
                double sum = 0;
                int cnt = 0;
                for (uint32_t y = 0; y < 16; ++y)
                    for (uint32_t x = 0; x < 16; ++x) {
                        const size_t i = static_cast<size_t>(by + y) * W + bx + x;
                        if (roi[i]) { sum += out[i]; ++cnt; }
                    }
                if (cnt > 128) { const double m = sum / cnt; b1 += m; b2 += m * m; ++nb; }
            }
        const double bm = b1 / static_cast<double>(nb);
        const double bsd = std::sqrt(std::max(0.0, b2 / static_cast<double>(nb) - bm * bm));
        const double flatMean = inMean / static_cast<double>(n);
        std::printf("[a241] B6 held out flat%d, gain from flat%d+flat%d: flat mean %.1f ADU%s | masked %zu px (%.3f %%) | FlatResidualPct %.3f %% (output mean %.1f) | spread of 16x16 block means %.3f %%\n",
                    c.j, c.a, c.b, flatMean, flatMean >= 2000.0 ? " (judged)" : " (below 2000 ADU: reference only)", masked,
                    100.0 * static_cast<double>(masked) / static_cast<double>(N), 100.0 * sd / mean, mean, 100.0 * bsd / bm);
        xpe_clear_alerts();
    }
    xpe_preprocess_shutdown();
}

// B10 v2: the reference image the stage-2 lane receives. Everything from scratch: maps regenerated, one process, one frame.
TEST(A241Measure, DISABLED_B10v2_Baseline) {
    const std::string cal = envOr("XPE_A240_CAL", "");
    const std::string wristPath = envOr("XPE_A240_WRIST", "");
    const std::string outDir = envOr("XPE_A243_OUT", "");
    ASSERT_FALSE(cal.empty());
    ASSERT_FALSE(wristPath.empty());
    ASSERT_FALSE(outDir.empty());
    const uint32_t W = 3072, H = 3072;
    const size_t N = static_cast<size_t>(W) * H;
    const fs::path dir = fs::path(outDir);
    fs::create_directories(dir / "maps");

    const std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw", N);
    const std::vector<uint16_t> wrist = readRaw16(wristPath, N);
    ASSERT_FALSE(dark.empty());
    ASSERT_FALSE(wrist.empty());
    std::vector<std::vector<uint16_t>> flat(7);
    for (int k = 4; k <= 6; ++k) {
        char nm[32];
        std::snprintf(nm, sizeof nm, "/bright%02d.raw", k);
        flat[static_cast<size_t>(k)] = readRaw16(cal + nm, N);
        ASSERT_FALSE(flat[static_cast<size_t>(k)].empty()) << nm;
    }
    const auto bpBytes = readFile(cal + "/BPMap.map");
    ASSERT_EQ(N, bpBytes.size());

    auto buf = [&](const void* d, size_t bytes, XpePixelFormat f) {
        XpeImageBuffer b{};
        b.data = const_cast<void*>(d);
        b.dataSize = bytes;
        b.width = W;
        b.height = H;
        b.format = f;
        b.bitsAllocated = f == XPE_PIXEL_FLOAT32 ? 32u : 16u;
        b.bitsStored = b.bitsAllocated;
        return b;
    };

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string offPath = (dir / "maps" / "offset.xcal").string(), gainPath = (dir / "maps" / "gain_from_4_5_6.xcal").string(),
                      defPath = (dir / "maps" / "defect.xcal").string();
    const XpeImageBuffer darkBuf = buf(dark.data(), N * 2, XPE_PIXEL_UINT16);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&darkBuf, 1, 100.0f, 25.0f, offPath.c_str(), nullptr));
    const XpeImageBuffer three[3] = {buf(flat[4].data(), N * 2, XPE_PIXEL_UINT16), buf(flat[5].data(), N * 2, XPE_PIXEL_UINT16),
                                     buf(flat[6].data(), N * 2, XPE_PIXEL_UINT16)};
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(three, 3, nullptr, gainPath.c_str(), nullptr));
    XCal o;
    ASSERT_TRUE(readXCal(offPath, &o));
    {   // the three maps must carry one session id: the gain takes the offset's, the defect map is written with it
        XCal g;
        ASSERT_TRUE(readXCal(gainPath, &g));
        std::memcpy(g.hdr.session_id, o.hdr.session_id, sizeof g.hdr.session_id);
        ASSERT_EQ(XPE_OK, writeXCal(gainPath, g));
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, o.hdr.session_id, sizeof hdr.session_id);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file(defPath.c_str(), hdr, nullptr, 0, bpBytes.data(), N));
    }
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offPath.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
    xpe_clear_alerts();

    std::vector<float> out(N, 0.0f);
    const XpeImageBuffer in = buf(wrist.data(), N * 2, XPE_PIXEL_UINT16);
    XpeImageBuffer ob = buf(out.data(), N * 4, XPE_PIXEL_FLOAT32);
    XpeImageMetadata meta{};
    const char* cfg = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &ob, &meta, nullptr, nullptr, cfg));
    const std::string outFile = (dir / "wrist_lat_3072x3072_corrected_v2_f32le.raw").string();
    {
        std::ofstream f(outFile, std::ios::binary);
        f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(N * sizeof(float)));
    }
    uint64_t h = 1469598103934665603ull;
    for (const float v : out) {
        uint32_t b;
        std::memcpy(&b, &v, 4);
        for (int k = 0; k < 4; ++k) h = (h ^ ((b >> (8 * k)) & 0xFF)) * 1099511628211ull;
    }
    std::printf("[a241] B10v2 wrote %s (%zu bytes), FNV-64 %016llx, format %d, %ux%u, flags 0x%x\n", outFile.c_str(), N * sizeof(float),
                static_cast<unsigned long long>(h), static_cast<int>(ob.format), ob.width, ob.height, meta.flags);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

// B6 per protocol section 5.3 (QA-A-241c). See the method file named in the header of this file; nothing is excluded from the ROI
// except pixels whose gain value is not finite or not > 0, and no flat is left out of the report.
TEST(A241Measure, DISABLED_B6_Protocol53) {
    const std::string cal = envOr("XPE_A240_CAL", "");
    const std::string work = envOr("XPE_A241_OUT", "build/a241");
    ASSERT_FALSE(cal.empty());
    const uint32_t W = 3072, H = 3072;
    const size_t N = static_cast<size_t>(W) * H;
    const fs::path dir = fs::path(work) / "b6p53";
    fs::create_directories(dir);

    const std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw", N);
    ASSERT_FALSE(dark.empty());
    std::vector<std::vector<uint16_t>> flat(7);
    for (int k = 4; k <= 6; ++k) {
        char nm[32];
        std::snprintf(nm, sizeof nm, "/bright%02d.raw", k);
        flat[static_cast<size_t>(k)] = readRaw16(cal + nm, N);
        ASSERT_FALSE(flat[static_cast<size_t>(k)].empty()) << nm;
    }
    const auto bpBytes = readFile(cal + "/BPMap.map");
    ASSERT_EQ(N, bpBytes.size());

    auto buf = [&](const void* d, size_t bytes, XpePixelFormat f) {
        XpeImageBuffer b{};
        b.data = const_cast<void*>(d);
        b.dataSize = bytes;
        b.width = W;
        b.height = H;
        b.format = f;
        b.bitsAllocated = f == XPE_PIXEL_FLOAT32 ? 32u : 16u;
        b.bitsStored = b.bitsAllocated;
        return b;
    };

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string offPath = (dir / "offset.xcal").string(), defPath = (dir / "defect.xcal").string();
    const XpeImageBuffer darkBuf = buf(dark.data(), N * 2, XPE_PIXEL_UINT16);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&darkBuf, 1, 100.0f, 25.0f, offPath.c_str(), nullptr));
    XCal o;
    ASSERT_TRUE(readXCal(offPath, &o));
    {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, o.hdr.session_id, sizeof hdr.session_id);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file(defPath.c_str(), hdr, nullptr, 0, bpBytes.data(), N));
    }
    const char* cfgFinal = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
    const char* cfgBeforeDefect = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}";

    struct Held { int j, a, b; };
    const Held cases[] = {{4, 5, 6}, {5, 4, 6}, {6, 4, 5}};
    for (const Held& c : cases) {
        const std::string gainPath = (dir / ("gain_from_" + std::to_string(c.a) + "_" + std::to_string(c.b) + ".xcal")).string();
        const XpeImageBuffer two[2] = {buf(flat[static_cast<size_t>(c.a)].data(), N * 2, XPE_PIXEL_UINT16),
                                       buf(flat[static_cast<size_t>(c.b)].data(), N * 2, XPE_PIXEL_UINT16)};
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(two, 2, nullptr, gainPath.c_str(), nullptr));
        XCal g;
        ASSERT_TRUE(readXCal(gainPath, &g));
        std::memcpy(g.hdr.session_id, o.hdr.session_id, sizeof g.hdr.session_id);
        ASSERT_EQ(XPE_OK, writeXCal(gainPath, g));
        const float* gv = reinterpret_cast<const float*>(g.payload.data());

        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
        xpe_clear_alerts();

        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(dark[i] + flat[static_cast<size_t>(c.j)][i]);
        std::vector<float> outFinal(N, 0.0f), outPre(N, 0.0f);
        const XpeImageBuffer in = buf(raw.data(), N * 2, XPE_PIXEL_UINT16);
        XpeImageBuffer obF = buf(outFinal.data(), N * 4, XPE_PIXEL_FLOAT32), obP = buf(outPre.data(), N * 4, XPE_PIXEL_FLOAT32);
        XpeImageMetadata metaF{}, metaP{};
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &obF, &metaF, nullptr, nullptr, cfgFinal));
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &obP, &metaP, nullptr, nullptr, cfgBeforeDefect));
        xpe_clear_alerts();

        // ROI per protocol 5.3: the gain value (the file's value) is finite and > 0
        size_t roiN = 0, excluded = 0, inDefectMap = 0, gainOutOfRange = 0;
        double sF = 0, sF2 = 0, sP = 0, sP2 = 0, inMean = 0;
        for (size_t i = 0; i < N; ++i) {
            if (!(std::isfinite(gv[i]) && gv[i] > 0.0f)) { ++excluded; continue; }
            ++roiN;
            if (bpBytes[i] != 0) ++inDefectMap;
            if (!(gv[i] >= 0.1f && gv[i] <= 10.0f)) ++gainOutOfRange;
            sF += outFinal[i];
            sF2 += static_cast<double>(outFinal[i]) * outFinal[i];
            sP += outPre[i];
            sP2 += static_cast<double>(outPre[i]) * outPre[i];
            inMean += flat[static_cast<size_t>(c.j)][i];
        }
        auto cv = [&](double s1, double s2, double* mean) {
            const double m = s1 / static_cast<double>(roiN);
            *mean = m;
            return 100.0 * std::sqrt(std::max(0.0, s2 / static_cast<double>(roiN) - m * m)) / m;
        };
        double meanF = 0, meanP = 0;
        const double pctF = cv(sF, sF2, &meanF), pctP = cv(sP, sP2, &meanP);
        // the large-scale part of the final output: spread of 16x16 block means over the ROI pixels
        double b1 = 0, b2 = 0;
        size_t nb = 0;
        for (uint32_t by = 0; by + 16 <= H; by += 16)
            for (uint32_t bx = 0; bx + 16 <= W; bx += 16) {
                double sum = 0;
                int cnt = 0;
                for (uint32_t y = 0; y < 16; ++y)
                    for (uint32_t x = 0; x < 16; ++x) {
                        const size_t i = static_cast<size_t>(by + y) * W + bx + x;
                        if (std::isfinite(gv[i]) && gv[i] > 0.0f) { sum += outFinal[i]; ++cnt; }
                    }
                if (cnt > 128) { const double m = sum / cnt; b1 += m; b2 += m * m; ++nb; }
            }
        const double bm = b1 / static_cast<double>(nb);
        const double bsd = std::sqrt(std::max(0.0, b2 / static_cast<double>(nb) - bm * bm));
        std::printf("[a241c] B6 protocol 5.3, held out flat%d, gain from flat%d+flat%d | ROI %zu px (excluded: gain not finite or <= 0: %zu; in the ROI: %zu defect-map px, %zu gain-out-of-range px) | "
                    "FlatResidualPct final output %.3f %% (mean %.1f ADU) | before the defect stage %.3f %% (mean %.1f ADU) | 16x16 block-mean spread of the final output %.3f %% | flat mean %.1f ADU | verdict for this flat: %s\n",
                    c.j, c.a, c.b, roiN, excluded, inDefectMap, gainOutOfRange, pctF, meanF, pctP, meanP, 100.0 * bsd / bm,
                    inMean / static_cast<double>(roiN), pctF <= 1.0 ? "<= 1.0 %" : "ABOVE 1.0 %");
    }
    xpe_preprocess_shutdown();
}

// B6 acceptance indicators of protocol section 5.3 (QA-A-241e). The method is fixed in
// .moai/reports/lane-pre/QA-A-241e/evidence/00_b6_acceptance_method.md (committed before any result): FPN_Reduction_dB with R = the raw
// input frame and Y = the shipping-path final output, LineArtifactScore of the final output against the same path with only the gain
// stage bypassed, both over the section 5.3 ROI (gain finite and > 0), three held-out combinations of flats 4,5,6, all reported.
// The first part of the test checks the formulas against hand-computed synthetic cases; if one is wrong nothing is measured.
namespace {

// QA-A-241f: when both the gain and the defect stage are bypassed the pipeline's result is still the uint16 frame (final_result_is_float is
// false), written into the first width*height*2 bytes of the caller's buffer with out->format = UINT16. Reading that buffer as float32 (what
// the first QA-A-241e run of this harness did for its "offset only" image) gives garbage. This widens a uint16 result to float32 in place.
void widenIfUint16(std::vector<float>& v, const XpeImageBuffer& ob) {
    if (ob.format != XPE_PIXEL_UINT16) return;
    const uint16_t* u = reinterpret_cast<const uint16_t*>(v.data());
    for (size_t i = v.size(); i-- > 0;) v[i] = static_cast<float>(u[i]);   // descending: a float at i never overwrites a uint16 still to be read
}

struct RoiStat {
    double mean{0}, sd{0};
};

// population mean and standard deviation of img over the pixels where roi[i] != 0
RoiStat roiStat(const float* img, const std::vector<uint8_t>& roi) {
    double s1 = 0, s2 = 0;
    size_t n = 0;
    for (size_t i = 0; i < roi.size(); ++i)
        if (roi[i]) { s1 += img[i]; s2 += static_cast<double>(img[i]) * img[i]; ++n; }
    RoiStat r;
    if (n == 0) return r;
    r.mean = s1 / static_cast<double>(n);
    r.sd = std::sqrt(std::max(0.0, s2 / static_cast<double>(n) - r.mean * r.mean));
    return r;
}

double popStd(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double s1 = 0, s2 = 0;
    for (double x : v) { s1 += x; s2 += x * x; }
    const double m = s1 / static_cast<double>(v.size());
    return std::sqrt(std::max(0.0, s2 / static_cast<double>(v.size()) - m * m));
}

// LineArtifactScore = max(std(row_mean(Y)), std(col_mean(Y))) / max(std(tile_mean(Y)), epsilon); means over the ROI pixels only; a row,
// column or tile with no ROI pixel is left out; tiles do not overlap and are tile x tile pixels (the partial ones at the border count).
double lineArtifactScore(const float* img, const std::vector<uint8_t>& roi, uint32_t W, uint32_t H, uint32_t tile) {
    std::vector<double> rowS(H, 0.0), colS(W, 0.0);
    std::vector<size_t> rowN(H, 0), colN(W, 0);
    const uint32_t TX = (W + tile - 1) / tile, TY = (H + tile - 1) / tile;
    std::vector<double> tS(static_cast<size_t>(TX) * TY, 0.0);
    std::vector<size_t> tN(static_cast<size_t>(TX) * TY, 0);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (!roi[i]) continue;
            rowS[y] += img[i]; ++rowN[y];
            colS[x] += img[i]; ++colN[x];
            const size_t t = static_cast<size_t>(y / tile) * TX + x / tile;
            tS[t] += img[i]; ++tN[t];
        }
    auto means = [](const std::vector<double>& s, const std::vector<size_t>& n) {
        std::vector<double> m;
        for (size_t k = 0; k < s.size(); ++k)
            if (n[k]) m.push_back(s[k] / static_cast<double>(n[k]));
        return m;
    };
    const double sr = popStd(means(rowS, rowN)), sc = popStd(means(colS, colN)), st = popStd(means(tS, tN));
    return std::max(sr, sc) / std::max(st, 1e-12);
}

double fpnReductionDb(const RoiStat& r, const RoiStat& y) { return 20.0 * std::log10(r.sd / std::max(y.sd, 1e-12)); }

}  // namespace

TEST(A241Measure, DISABLED_B6_Acceptance) {
    // ---- formula check on synthetic frames (hand-computed): nothing is measured if one of these is wrong
    {
        const uint32_t S = 8;
        const std::vector<uint8_t> all(static_cast<size_t>(S) * S, 1);
        std::vector<float> rowImg(static_cast<size_t>(S) * S, 0.0f), colImg(static_cast<size_t>(S) * S, 0.0f), flatImg(static_cast<size_t>(S) * S, 5.0f);
        for (uint32_t x = 0; x < S; ++x) rowImg[x] = 1.0f;               // row 0 is +1
        for (uint32_t y = 0; y < S; ++y) colImg[static_cast<size_t>(y) * S] = 1.0f;   // column 0 is +1
        // row means: one 1 and seven 0 -> std sqrt(7)/8; col means all 1/8 -> 0; 4x4 tile means: two 0.25 and two 0 -> std 0.125; score sqrt(7) = 2.6458
        EXPECT_NEAR(std::sqrt(7.0), lineArtifactScore(rowImg.data(), all, S, S, 4), 1e-9) << "row artifact score";
        EXPECT_NEAR(std::sqrt(7.0), lineArtifactScore(colImg.data(), all, S, S, 4), 1e-9) << "column artifact score";
        EXPECT_NEAR(0.0, lineArtifactScore(flatImg.data(), all, S, S, 4), 1e-6) << "a flat frame has no line artifact";
        std::vector<float> a(static_cast<size_t>(S) * S), b(static_cast<size_t>(S) * S);
        for (size_t i = 0; i < a.size(); ++i) { a[i] = (i % 2 ? 10.0f : -10.0f); b[i] = a[i] / 2.0f; }   // b has half the spread of a
        EXPECT_NEAR(20.0 * std::log10(2.0), fpnReductionDb(roiStat(a.data(), all), roiStat(b.data(), all)), 1e-9);
        if (::testing::Test::HasFailure()) { GTEST_SKIP() << "formula check failed: no measurement is valid"; }
        std::printf("[a241e] formula check on synthetic frames passed (row/column score sqrt(7)=2.6458, flat 0, 2x spread = 6.0206 dB)\n");
    }

    const std::string cal = envOr("XPE_A240_CAL", "");
    const std::string work = envOr("XPE_A241_OUT", "build/a241");
    ASSERT_FALSE(cal.empty());
    const uint32_t W = 3072, H = 3072;
    const size_t N = static_cast<size_t>(W) * H;
    const fs::path dir = fs::path(work) / "b6acc";
    fs::create_directories(dir);

    const std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw", N);
    ASSERT_FALSE(dark.empty());
    std::vector<std::vector<uint16_t>> flat(7);
    for (int k = 4; k <= 6; ++k) {
        char nm[32];
        std::snprintf(nm, sizeof nm, "/bright%02d.raw", k);
        flat[static_cast<size_t>(k)] = readRaw16(cal + nm, N);
        ASSERT_FALSE(flat[static_cast<size_t>(k)].empty()) << nm;
    }
    const auto bpBytes = readFile(cal + "/BPMap.map");
    ASSERT_EQ(N, bpBytes.size());

    auto buf = [&](const void* d, size_t bytes, XpePixelFormat f) {
        XpeImageBuffer b{};
        b.data = const_cast<void*>(d);
        b.dataSize = bytes;
        b.width = W;
        b.height = H;
        b.format = f;
        b.bitsAllocated = f == XPE_PIXEL_FLOAT32 ? 32u : 16u;
        b.bitsStored = b.bitsAllocated;
        return b;
    };

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string offPath = (dir / "offset.xcal").string(), defPath = (dir / "defect.xcal").string();
    const XpeImageBuffer darkBuf = buf(dark.data(), N * 2, XPE_PIXEL_UINT16);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&darkBuf, 1, 100.0f, 25.0f, offPath.c_str(), nullptr));
    XCal o;
    ASSERT_TRUE(readXCal(offPath, &o));
    {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, o.hdr.session_id, sizeof hdr.session_id);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file(defPath.c_str(), hdr, nullptr, 0, bpBytes.data(), N));
    }
    const char* cfgFinal = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
    const char* cfgNoGain = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGain\":true}";
    const char* cfgOffsetOnly = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGain\":true,\"bypassDefect\":true}";

    struct Held { int j, a, b; };
    const Held cases[] = {{4, 5, 6}, {5, 4, 6}, {6, 4, 5}};
    for (const Held& c : cases) {
        const std::string gainPath = (dir / ("gain_from_" + std::to_string(c.a) + "_" + std::to_string(c.b) + ".xcal")).string();
        const XpeImageBuffer two[2] = {buf(flat[static_cast<size_t>(c.a)].data(), N * 2, XPE_PIXEL_UINT16),
                                       buf(flat[static_cast<size_t>(c.b)].data(), N * 2, XPE_PIXEL_UINT16)};
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(two, 2, nullptr, gainPath.c_str(), nullptr));
        XCal g;
        ASSERT_TRUE(readXCal(gainPath, &g));
        std::memcpy(g.hdr.session_id, o.hdr.session_id, sizeof g.hdr.session_id);
        ASSERT_EQ(XPE_OK, writeXCal(gainPath, g));
        const float* gv = reinterpret_cast<const float*>(g.payload.data());

        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
        xpe_clear_alerts();

        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(dark[i] + flat[static_cast<size_t>(c.j)][i]);
        std::vector<float> rawF(N), outFinal(N, 0.0f), outNoGain(N, 0.0f), outOffset(N, 0.0f);
        for (size_t i = 0; i < N; ++i) rawF[i] = static_cast<float>(raw[i]);
        const XpeImageBuffer in = buf(raw.data(), N * 2, XPE_PIXEL_UINT16);
        XpeImageBuffer o1 = buf(outFinal.data(), N * 4, XPE_PIXEL_FLOAT32), o2 = buf(outNoGain.data(), N * 4, XPE_PIXEL_FLOAT32),
                       o3 = buf(outOffset.data(), N * 4, XPE_PIXEL_FLOAT32);
        XpeImageMetadata m1{}, m2{}, m3{};
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &o1, &m1, nullptr, nullptr, cfgFinal));
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &o2, &m2, nullptr, nullptr, cfgNoGain));
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &o3, &m3, nullptr, nullptr, cfgOffsetOnly));
        widenIfUint16(outOffset, o3);   // QA-A-241f: see widenIfUint16
        xpe_clear_alerts();

        // ROI per protocol 5.3: the gain value (the file's value) is finite and > 0
        std::vector<uint8_t> roi(N, 0);
        size_t roiN = 0;
        double flatMean = 0;
        for (size_t i = 0; i < N; ++i)
            if (std::isfinite(gv[i]) && gv[i] > 0.0f) { roi[i] = 1; ++roiN; flatMean += flat[static_cast<size_t>(c.j)][i]; }
        flatMean /= static_cast<double>(roiN);

        const RoiStat sR = roiStat(rawF.data(), roi), sY = roiStat(outFinal.data(), roi), sOff = roiStat(outOffset.data(), roi);
        const double flatResidual = 100.0 * sY.sd / sY.mean;
        const double fpn = fpnReductionDb(sR, sY);                 // judged: R = the raw input frame
        const double fpnAlt = fpnReductionDb(sOff, sY);            // reported only: R = offset-corrected, before gain and defect
        double las[3][3];                                          // [tile 32/64/128][final / gain bypassed / offset only]
        const uint32_t tiles[3] = {32, 64, 128};
        for (int t = 0; t < 3; ++t) {
            las[t][0] = lineArtifactScore(outFinal.data(), roi, W, H, tiles[t]);
            las[t][1] = lineArtifactScore(outNoGain.data(), roi, W, H, tiles[t]);
            las[t][2] = lineArtifactScore(outOffset.data(), roi, W, H, tiles[t]);
        }
        const double lasRatio = las[1][0] / las[1][1];             // judged: tile 64, final / gain bypassed
        const bool okRes = flatResidual <= 1.0, okFpn = fpn >= 10.0, okLas = lasRatio <= 1.10;
        std::printf("[a241e] held out flat%d, gain from flat%d+flat%d | ROI %zu px | flat mean %.1f ADU (%s 2000) | "
                    "FlatResidualPct %.3f %% [%s 1.0] | std(R) %.2f ADU std(Y) %.2f ADU | FPN_Reduction_dB %.2f [%s 10] (R=offset-corrected: %.2f) | "
                    "LineArtifactScore tile64: final %.4f, gain bypassed %.4f, offset only %.4f, ratio final/gain-bypassed %.4f [%s 1.10] | "
                    "tile32: final %.4f / gain bypassed %.4f / offset only %.4f | tile128: final %.4f / gain bypassed %.4f / offset only %.4f | "
                    "ratio vs offset only (tile64) %.4f | all three accepted: %s\n",
                    c.j, c.a, c.b, roiN, flatMean, flatMean >= 2000.0 ? ">=" : "<", flatResidual, okRes ? "<=" : ">", sR.sd, sY.sd, fpn,
                    okFpn ? ">=" : "<", fpnAlt, las[1][0], las[1][1], las[1][2], lasRatio, okLas ? "<=" : ">", las[0][0], las[0][1], las[0][2],
                    las[2][0], las[2][1], las[2][2], las[1][0] / las[1][2], (okRes && okFpn && okLas) ? "YES" : "NO");
    }
    xpe_preprocess_shutdown();
}

// Stripe-metric investigation (QA-A-241f). Measurement only, no verdict. The method and the conclusion criteria are fixed in
// .moai/reports/lane-pre/QA-A-241f/evidence/00_stripe_investigation_method.md (committed before any result). It splits
// LineArtifactScore into its numerator (std of row means, std of column means) and its denominator (std of tile means) at tile sizes
// 16..256, on four images and two ROIs, shows the row/column mean profiles, compares the row/column structure with what the pixel noise
// alone would give, and runs two synthetic controls first.
namespace {

struct StripeParts {
    double rowStd{0}, colStd{0}, tileStd{0};
    size_t tiles{0};
    double num() const { return std::max(rowStd, colStd); }
    double score() const { return num() / std::max(tileStd, 1e-12); }
};

// row means and column means over the ROI pixels (rows/columns with no ROI pixel are left out)
void rowColMeans(const float* img, const std::vector<uint8_t>& roi, uint32_t W, uint32_t H, std::vector<double>* rows, std::vector<double>* cols,
                 double* meanRowCount, double* meanColCount) {
    std::vector<double> rs(H, 0.0), cs(W, 0.0);
    std::vector<size_t> rn(H, 0), cn(W, 0);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (!roi[i]) continue;
            rs[y] += img[i]; ++rn[y];
            cs[x] += img[i]; ++cn[x];
        }
    rows->clear();
    cols->clear();
    double sr = 0, sc = 0;
    for (uint32_t y = 0; y < H; ++y) if (rn[y]) { rows->push_back(rs[y] / static_cast<double>(rn[y])); sr += static_cast<double>(rn[y]); }
    for (uint32_t x = 0; x < W; ++x) if (cn[x]) { cols->push_back(cs[x] / static_cast<double>(cn[x])); sc += static_cast<double>(cn[x]); }
    *meanRowCount = rows->empty() ? 0.0 : sr / static_cast<double>(rows->size());
    *meanColCount = cols->empty() ? 0.0 : sc / static_cast<double>(cols->size());
}

StripeParts stripeParts(const float* img, const std::vector<uint8_t>& roi, uint32_t W, uint32_t H, uint32_t tile) {
    std::vector<double> rows, cols;
    double nr = 0, nc = 0;
    rowColMeans(img, roi, W, H, &rows, &cols, &nr, &nc);
    StripeParts p;
    p.rowStd = popStd(rows);
    p.colStd = popStd(cols);
    const uint32_t TX = (W + tile - 1) / tile, TY = (H + tile - 1) / tile;
    std::vector<double> tS(static_cast<size_t>(TX) * TY, 0.0);
    std::vector<size_t> tN(static_cast<size_t>(TX) * TY, 0);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (!roi[i]) continue;
            const size_t t = static_cast<size_t>(y / tile) * TX + x / tile;
            tS[t] += img[i]; ++tN[t];
        }
    std::vector<double> tm;
    for (size_t k = 0; k < tS.size(); ++k) if (tN[k]) tm.push_back(tS[k] / static_cast<double>(tN[k]));
    p.tileStd = popStd(tm);
    p.tiles = tm.size();
    return p;
}

// pixel noise from horizontal neighbour differences over pairs that are both in the ROI: sigma = std(x[i] - x[i+1]) / sqrt(2)
double pairSigma(const float* img, const std::vector<uint8_t>& roi, uint32_t W, uint32_t H) {
    double s1 = 0, s2 = 0;
    size_t n = 0;
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x + 1 < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (!roi[i] || !roi[i + 1]) continue;
            const double d = static_cast<double>(img[i]) - img[i + 1];
            s1 += d; s2 += d * d; ++n;
        }
    if (!n) return 0.0;
    const double m = s1 / static_cast<double>(n);
    return std::sqrt(std::max(0.0, s2 / static_cast<double>(n) - m * m)) / std::sqrt(2.0);
}

// std of a profile after subtracting its centred 33-sample moving average (the high-frequency part)
double highPassStd(const std::vector<double>& v) {
    const int n = static_cast<int>(v.size()), half = 16;
    std::vector<double> hp(v.size(), 0.0);
    for (int i = 0; i < n; ++i) {
        const int a = std::max(0, i - half), b = std::min(n - 1, i + half);
        double s = 0;
        for (int k = a; k <= b; ++k) s += v[static_cast<size_t>(k)];
        hp[static_cast<size_t>(i)] = v[static_cast<size_t>(i)] - s / static_cast<double>(b - a + 1);
    }
    return popStd(hp);
}

// std of a profile after removing its least-squares straight line
double detrendedStd(const std::vector<double>& v) {
    const double n = static_cast<double>(v.size());
    if (v.size() < 3) return 0.0;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < v.size(); ++i) { const double x = static_cast<double>(i); sx += x; sy += v[i]; sxx += x * x; sxy += x * v[i]; }
    const double b = (n * sxy - sx * sy) / (n * sxx - sx * sx), a = (sy - b * sx) / n;
    std::vector<double> r(v.size());
    for (size_t i = 0; i < v.size(); ++i) r[i] = v[i] - (a + b * static_cast<double>(i));
    return popStd(r);
}

const uint32_t kStripeTiles[5] = {16, 32, 64, 128, 256};

}  // namespace

TEST(A241Measure, DISABLED_B6_StripeInvestigation) {
    const uint32_t W = 3072, H = 3072;
    const size_t N = static_cast<size_t>(W) * H;
    const std::string cal = envOr("XPE_A240_CAL", "");
    const std::string work = envOr("XPE_A241_OUT", "build/a241");
    const fs::path dir = fs::path(work) / "stripe";
    fs::create_directories(dir);
    std::FILE* tab = nullptr;
    fopen_s(&tab, (dir / "tables.txt").string().c_str(), "wb");
    ASSERT_NE(nullptr, tab);
    auto out = [&](const char* fmt, auto... a) {
        std::printf(fmt, a...);
        std::fprintf(tab, fmt, a...);
    };

    // ---- controls first: nothing else is used if they are not what they should be
    {
        const std::vector<uint8_t> all(N, 1);
        std::vector<float> noise(N), striped(N);
        std::mt19937_64 rng(20261005ull);
        std::normal_distribution<double> nd(0.0, 1.0);
        const double sigma = 18.5;   // about the pixel noise of the held-out flat 6 output; the value only sets the scale
        for (size_t i = 0; i < N; ++i) noise[i] = static_cast<float>(2000.0 + sigma * nd(rng));
        std::vector<double> rowOff(H);
        for (auto& r : rowOff) r = 3.0 * sigma / std::sqrt(static_cast<double>(W)) * nd(rng);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) striped[static_cast<size_t>(y) * W + x] = noise[static_cast<size_t>(y) * W + x] + static_cast<float>(rowOff[y]);
        bool controlOk = true;
        for (uint32_t t : kStripeTiles) {
            const StripeParts c1 = stripeParts(noise.data(), all, W, H, t), c2 = stripeParts(striped.data(), all, W, H, t);
            const double expected = static_cast<double>(t) / std::sqrt(static_cast<double>(W));   // tile / 55.4
            const double numRatio = c2.num() / c1.num();
            out("[a241f] control tile %3u | white noise: num %.4f den %.4f score %.4f (expected tile/sqrt(W) = %.4f) | + row stripes of 3x the noise expectation: num %.4f score %.4f, numerator ratio %.2f\n",
                t, c1.num(), c1.tileStd, c1.score(), expected, c2.num(), c2.score(), numRatio);
            // sampling error of a standard deviation over n samples is about 1/sqrt(2(n-1)); the score divides two of them
            const double tol = 3.0 * std::sqrt(1.0 / (2.0 * (static_cast<double>(H) - 1.0)) + 1.0 / (2.0 * (static_cast<double>(c1.tiles) - 1.0)));
            if (std::fabs(c1.score() / expected - 1.0) > tol) controlOk = false;
            if (!(numRatio >= 2.5 && numRatio <= 3.5)) controlOk = false;
        }
        out("[a241f] controls %s\n", controlOk ? "as expected: the instrument is valid" : "NOT as expected: no measurement below is valid");
        if (!controlOk) { std::fclose(tab); GTEST_SKIP() << "control failed"; }
    }

    ASSERT_FALSE(cal.empty());
    const std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw", N);
    ASSERT_FALSE(dark.empty());
    std::vector<std::vector<uint16_t>> flat(7);
    for (int k = 4; k <= 6; ++k) {
        char nm[32];
        std::snprintf(nm, sizeof nm, "/bright%02d.raw", k);
        flat[static_cast<size_t>(k)] = readRaw16(cal + nm, N);
        ASSERT_FALSE(flat[static_cast<size_t>(k)].empty()) << nm;
    }
    const auto bpBytes = readFile(cal + "/BPMap.map");
    ASSERT_EQ(N, bpBytes.size());
    auto buf = [&](const void* d, size_t bytes, XpePixelFormat f) {
        XpeImageBuffer b{};
        b.data = const_cast<void*>(d);
        b.dataSize = bytes;
        b.width = W;
        b.height = H;
        b.format = f;
        b.bitsAllocated = f == XPE_PIXEL_FLOAT32 ? 32u : 16u;
        b.bitsStored = b.bitsAllocated;
        return b;
    };
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string offPath = (dir / "offset.xcal").string(), defPath = (dir / "defect.xcal").string();
    const XpeImageBuffer darkBuf = buf(dark.data(), N * 2, XPE_PIXEL_UINT16);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&darkBuf, 1, 100.0f, 25.0f, offPath.c_str(), nullptr));
    XCal o;
    ASSERT_TRUE(readXCal(offPath, &o));
    {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, o.hdr.session_id, sizeof hdr.session_id);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file(defPath.c_str(), hdr, nullptr, 0, bpBytes.data(), N));
    }
    const char* cfgs[4] = {
        "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}",
        "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGain\":true}",
        "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGain\":true,\"bypassDefect\":true}"};
    const char* imgName[4] = {"final", "gain bypassed", "offset only", "offset only, defect px out"};

    struct Held { int j, a, b; };
    const Held cases[] = {{6, 4, 5}, {4, 5, 6}, {5, 4, 6}};   // the primary flat first
    for (const Held& c : cases) {
        const std::string gainPath = (dir / ("gain_from_" + std::to_string(c.a) + "_" + std::to_string(c.b) + ".xcal")).string();
        const XpeImageBuffer two[2] = {buf(flat[static_cast<size_t>(c.a)].data(), N * 2, XPE_PIXEL_UINT16),
                                       buf(flat[static_cast<size_t>(c.b)].data(), N * 2, XPE_PIXEL_UINT16)};
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(two, 2, nullptr, gainPath.c_str(), nullptr));
        XCal g;
        ASSERT_TRUE(readXCal(gainPath, &g));
        std::memcpy(g.hdr.session_id, o.hdr.session_id, sizeof g.hdr.session_id);
        ASSERT_EQ(XPE_OK, writeXCal(gainPath, g));
        const float* gv = reinterpret_cast<const float*>(g.payload.data());
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
        xpe_clear_alerts();

        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(dark[i] + flat[static_cast<size_t>(c.j)][i]);
        std::vector<float> im[3];
        for (int k = 0; k < 3; ++k) {
            im[k].assign(N, 0.0f);
            const XpeImageBuffer in = buf(raw.data(), N * 2, XPE_PIXEL_UINT16);
            XpeImageBuffer ob = buf(im[k].data(), N * 4, XPE_PIXEL_FLOAT32);
            XpeImageMetadata md{};
            ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &ob, &md, nullptr, nullptr, cfgs[k]));
            widenIfUint16(im[k], ob);   // QA-A-241f: the offset-only result is uint16
        }
        xpe_clear_alerts();
        // images: 0 final, 1 gain bypassed, 2 offset only; the fourth "offset only, defect px out" is image 2 under ROI B
        std::vector<uint8_t> roiA(N, 0), roiB(N, 0);
        size_t nA = 0, nB = 0;
        for (size_t i = 0; i < N; ++i)
            if (std::isfinite(gv[i]) && gv[i] > 0.0f) {
                roiA[i] = 1; ++nA;
                if (bpBytes[i] == 0) { roiB[i] = 1; ++nB; }
            }
        out("[a241f] ==== held out flat%d (gain from flat%d+flat%d) | ROI A %zu px, ROI B (defect-map pixels removed) %zu px\n", c.j, c.a, c.b, nA, nB);

        // 1. numerator and denominator at every tile size
        for (int roiK = 0; roiK < 2; ++roiK) {
            const std::vector<uint8_t>& roi = roiK == 0 ? roiA : roiB;
            for (int k = 0; k < 3; ++k) {
                for (uint32_t t : kStripeTiles) {
                    const StripeParts p = stripeParts(im[k].data(), roi, W, H, t);
                    out("[a241f] flat%d ROI %c image=%-14s tile %3u | std(row_mean) %.4f std(col_mean) %.4f numerator %.4f | std(tile_mean) %.4f (%zu tiles) | score %.4f\n",
                        c.j, roiK == 0 ? 'A' : 'B', imgName[k], t, p.rowStd, p.colStd, p.num(), p.tileStd, p.tiles, p.score());
                }
            }
        }
        // ratio decomposition against both reference images, ROI A and ROI B
        for (int roiK = 0; roiK < 2; ++roiK) {
            const std::vector<uint8_t>& roi = roiK == 0 ? roiA : roiB;
            for (int ref = 1; ref <= 2; ++ref) {
                for (uint32_t t : kStripeTiles) {
                    const StripeParts f = stripeParts(im[0].data(), roi, W, H, t), r = stripeParts(im[ref].data(), roi, W, H, t);
                    out("[a241f] flat%d ROI %c final vs %-14s tile %3u | score ratio %.4f = numerator ratio %.4f / denominator ratio %.4f\n", c.j,
                        roiK == 0 ? 'A' : 'B', imgName[ref], t, f.score() / r.score(), f.num() / r.num(), f.tileStd / r.tileStd);
                }
            }
        }
        // 3. row/column structure against what the pixel noise alone gives
        for (int roiK = 0; roiK < 2; ++roiK) {
            const std::vector<uint8_t>& roi = roiK == 0 ? roiA : roiB;
            for (int k = 0; k < 3; ++k) {
                std::vector<double> rows, cols;
                double nr = 0, nc = 0;
                rowColMeans(im[k].data(), roi, W, H, &rows, &cols, &nr, &nc);
                const double sig = pairSigma(im[k].data(), roi, W, H);
                const double expRow = sig / std::sqrt(nr), expCol = sig / std::sqrt(nc);
                out("[a241f] flat%d ROI %c image=%-14s | pixel noise (neighbour differences) %.3f ADU | rows: std %.4f, detrended %.4f, high-pass %.4f, noise expectation %.4f -> high-pass/expected %.2f | "
                    "cols: std %.4f, detrended %.4f, high-pass %.4f, noise expectation %.4f -> high-pass/expected %.2f\n",
                    c.j, roiK == 0 ? 'A' : 'B', imgName[k], sig, popStd(rows), detrendedStd(rows), highPassStd(rows), expRow, highPassStd(rows) / expRow,
                    popStd(cols), detrendedStd(cols), highPassStd(cols), expCol, highPassStd(cols) / expCol);
            }
        }
        // 2. the profiles themselves (ROI A, final and gain bypassed), as text
        {
            std::vector<double> rF, cF, rG, cG;
            double a1 = 0, a2 = 0;
            rowColMeans(im[0].data(), roiA, W, H, &rF, &cF, &a1, &a2);
            rowColMeans(im[1].data(), roiA, W, H, &rG, &cG, &a1, &a2);
            std::FILE* pf = nullptr;
            fopen_s(&pf, (dir / ("profiles_flat" + std::to_string(c.j) + ".txt")).string().c_str(), "wb");
            ASSERT_NE(nullptr, pf);
            std::fprintf(pf, "# index row_mean_final row_mean_gain_bypassed col_mean_final col_mean_gain_bypassed (ROI A, held-out flat %d)\n", c.j);
            for (size_t i = 0; i < rF.size() && i < cF.size(); ++i) std::fprintf(pf, "%zu %.5f %.5f %.5f %.5f\n", i, rF[i], rG[i], cF[i], cG[i]);
            std::fclose(pf);
        }
    }
    std::fclose(tab);
    xpe_preprocess_shutdown();
}
