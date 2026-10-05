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
