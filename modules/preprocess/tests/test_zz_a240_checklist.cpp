/**
 * @file test_zz_a240_checklist.cpp
 * @brief QA-A-240: the stage-1 checklist B1..B10 (.moai/project/milestone-1-pre-basic.md), measured on real data.
 *
 * Not a gate. Every test here is DISABLED_ and only PRINTS what it observed ("[a240] ..." lines), so that a person
 * reads the facts and judges pass / fail against the requirement text. Nothing is asserted except what a test needs to
 * go on (a map it just made loads).
 *
 * Inputs (environment): XPE_A240_CAL   the CalData_6 directory (dark.raw, bright01..06.raw, BPMap.map)
 *                       XPE_A240_WRIST the real frame (wrist_lat_3072x3072.raw)
 *                       XPE_A240_OUT   a directory for the maps this makes and for the baseline file
 * The offset and gain maps are made by the product's own generators from the real dark and flat frames; the defect map is
 * BPMap.map written into an XCal file with its type values (1..4) kept.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "fixtures/make_xcal.hpp"

namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

constexpr uint32_t W = 3072, H = 3072;
constexpr size_t N = static_cast<size_t>(W) * H;

std::string env(const char* name) {
    char* v = nullptr;
    size_t len = 0;
    std::string out;
    if (_dupenv_s(&v, &len, name) == 0 && v != nullptr) {
        out = v;
        std::free(v);
    }
    return out;
}

double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

const char* rcName(int rc) {
    switch (rc) {
        case 0: return "XPE_OK";
        case -1: return "XPE_ERR_INVALID_INPUT";
        case -2: return "XPE_ERR_OUT_OF_MEMORY";
        case -3: return "XPE_ERR_PROCESSING_FAILED";
        case -4: return "XPE_ERR_CONFIG_INVALID";
        case -5: return "XPE_ERR_CALIBRATION_EXPIRED";
        case -6: return "XPE_ERR_NOT_INITIALIZED";
        case -7: return "XPE_ERR_UNSUPPORTED_FORMAT";
        case -8: return "XPE_ERR_BUFFER_TOO_SMALL";
        case -9: return "XPE_ERR_IO_FAILED";
        case -15: return "XPE_ERR_NOT_IMPLEMENTED";
        case -16: return "XPE_ERR_CALIB_NOT_LOADED";
        case -17: return "XPE_ERR_INVALID_CALIB_DATA";
        default: return "(other)";
    }
}

void say(const char* label, int rc, const char* extra = "") {
    std::printf("[a240] %-58s rc=%d %s %s\n", label, rc, rcName(rc), extra);
    std::fflush(stdout);
}

/** Prints and clears the alerts the module raised since the last call. Returns how many. */
bool g_alertSeen = false;   // set by drainAlerts when an alert contained g_alertNeedle
std::string g_alertNeedle;
int drainAlerts(const char* label) {
    char msg[600];
    int32_t sev = -1;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i) {
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) {
            std::printf("[a240]     alert (%s, severity %d): %.190s\n", label, static_cast<int>(sev), msg);
            if (!g_alertNeedle.empty() && std::strstr(msg, g_alertNeedle.c_str())) g_alertSeen = true;
        }
    }
    xpe_clear_alerts();
    return static_cast<int>(count);
}

std::vector<uint16_t> readRaw16(const std::string& path, size_t n = N) {
    std::vector<uint16_t> v(n);
    std::ifstream f(path, std::ios::binary);
    f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(n * sizeof(uint16_t)));
    if (static_cast<size_t>(f.gcount()) != n * sizeof(uint16_t)) return {};
    return v;
}

std::vector<unsigned char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const std::streamsize size = f.tellg();
    std::vector<unsigned char> v(static_cast<size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(v.data()), size);
    return v;
}

void writeFileBytes(const std::string& path, const std::vector<unsigned char>& v) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size()));
}

struct XCal {
    XCalFileHeader hdr{};
    std::vector<uint8_t> cfg;
    std::vector<uint8_t> payload;
};

bool readXCal(const std::string& path, XCal* out) {
    const std::vector<unsigned char> b = readFile(path);
    if (b.size() < sizeof(XCalFileHeader)) return false;
    std::memcpy(&out->hdr, b.data(), sizeof(XCalFileHeader));
    const size_t cfgAt = sizeof(XCalFileHeader);
    const size_t payAt = cfgAt + static_cast<size_t>(out->hdr.config_json_len);
    if (b.size() < payAt + out->hdr.payload_len) return false;
    out->cfg.assign(b.begin() + static_cast<std::ptrdiff_t>(cfgAt), b.begin() + static_cast<std::ptrdiff_t>(payAt));
    out->payload.assign(b.begin() + static_cast<std::ptrdiff_t>(payAt), b.begin() + static_cast<std::ptrdiff_t>(payAt + out->hdr.payload_len));
    return true;
}

XpeErrorCode writeXCal(const std::string& path, const XCal& x) {
    return write_xcal_file(path.c_str(), x.hdr, x.cfg.empty() ? nullptr : x.cfg.data(), x.cfg.size(), x.payload.data(), x.payload.size());
}

XCalFileHeader headerFor(XCalType type, XCalPixelFormat fmt, uint32_t w, uint32_t h) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    std::memcpy(hdr.session_id, "a240", 5);
    hdr.version = XCAL_VERSION;
    hdr.type = type;
    hdr.pixel_format = fmt;
    hdr.width = w;
    hdr.height = h;
    hdr.created_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return hdr;
}

XpeErrorCode writeMap(const std::string& path, XCalType type, XCalPixelFormat fmt, uint32_t w, uint32_t h, const void* data, size_t bytes,
                      int64_t expiryMs = 0) {
    XCalFileHeader hdr = headerFor(type, fmt, w, h);
    hdr.expiry_epoch_ms = expiryMs;
    hdr.payload_len = bytes;
    return write_xcal_file(path.c_str(), hdr, nullptr, 0, static_cast<const uint8_t*>(data), bytes);
}

XpeImageBuffer bufferOf(void* data, uint32_t w, uint32_t h, XpePixelFormat fmt) {
    XpeImageBuffer b{};
    b.width = w;
    b.height = h;
    b.format = fmt;
    b.bitsAllocated = (fmt == XPE_PIXEL_UINT16) ? 16 : 32;
    b.bitsStored = b.bitsAllocated;
    b.data = data;
    b.dataSize = static_cast<size_t>(w) * h * (b.bitsAllocated / 8);
    return b;
}

struct Maps {
    std::string dir, offset, gain, defect;
};

std::string outDir() {
    const std::string out = env("XPE_A240_OUT");
    fs::create_directories(out);
    return out;
}

/** Makes (once) the three real maps in $XPE_A240_OUT/maps and returns their paths. The module is initialized afterwards. */
Maps realMaps() {
    Maps m;
    m.dir = (fs::path(outDir()) / "maps").string();
    m.offset = (fs::path(m.dir) / "offset.xcal").string();
    m.gain = (fs::path(m.dir) / "gain.xcal").string();
    m.defect = (fs::path(m.dir) / "defect.xcal").string();
    xpe_preprocess_shutdown();
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    if (fs::exists(m.offset) && fs::exists(m.gain) && fs::exists(m.defect)) return m;
    fs::create_directories(m.dir);
    const std::string cal = env("XPE_A240_CAL");
    std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw");
    EXPECT_FALSE(dark.empty());
    XpeImageBuffer d = bufferOf(dark.data(), W, H, XPE_PIXEL_UINT16);
    EXPECT_EQ(XPE_OK, xpe_calib_generate_offset(&d, 1, 100.0f, 25.0f, m.offset.c_str(), nullptr));
    std::vector<std::vector<uint16_t>> flats;
    std::vector<XpeImageBuffer> buffers;
    for (int k = 1; k <= 6; ++k) {
        char name[32];
        std::snprintf(name, sizeof(name), "/bright%02d.raw", k);
        flats.push_back(readRaw16(cal + name));
        EXPECT_FALSE(flats.back().empty());
    }
    for (auto& v : flats) buffers.push_back(bufferOf(v.data(), W, H, XPE_PIXEL_UINT16));
    EXPECT_EQ(XPE_OK, xpe_calib_generate_gain(buffers.data(), 6, nullptr, m.gain.c_str(), nullptr));
    std::vector<unsigned char> bp = readFile(cal + "/BPMap.map");
    EXPECT_EQ(N, bp.size());
    EXPECT_EQ(XPE_OK, writeMap(m.defect, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, W, H, bp.data(), bp.size()));
    xpe_clear_alerts();
    return m;
}

struct Loaded {
    int rcOffset, rcGain, rcDefect;
    double msOffset, msGain, msDefect;
};

Loaded loadAll(const Maps& m) {
    Loaded r{};
    auto t0 = Clock::now();
    r.rcOffset = xpe_calib_load_offset(m.offset.c_str());
    auto t1 = Clock::now();
    r.rcGain = xpe_calib_load_gain(m.gain.c_str());
    auto t2 = Clock::now();
    r.rcDefect = xpe_calib_load_defect_map(m.defect.c_str());
    auto t3 = Clock::now();
    r.msOffset = ms(t0, t1);
    r.msGain = ms(t1, t2);
    r.msDefect = ms(t2, t3);
    return r;
}

/** A frame buffer big enough for the float result, with the uint16 raw in its first half. */
struct Frame {
    std::vector<float> data;
    XpeImageBuffer img;
    XpeImageMetadata meta{};
    explicit Frame(const std::vector<uint16_t>& raw) : data(N) {
        std::memcpy(data.data(), raw.data(), N * sizeof(uint16_t));
        img = bufferOf(data.data(), W, H, XPE_PIXEL_UINT16);
        img.dataSize = N * sizeof(float);   // room for the float32 result
        meta.pixelPitch_mm = 0.14f;
    }
};

uint64_t fnv(const void* p, size_t bytes) {
    uint64_t h = 1469598103934665603ull;
    const unsigned char* c = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < bytes; ++i) {
        h ^= c[i];
        h *= 1099511628211ull;
    }
    return h;
}

// The configuration that leaves ghost out by passing no ghost handle, and the one that also switches off every stage the
// checklist leaves out of stage 1.
const char* kBasicCfg = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";

}  // namespace

// ------------------------------------------------------------------------------------------------------------------
// B1  offset map: load and dimension check
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B1_OffsetLoadAndDimension) {
    const Maps m = realMaps();
    std::printf("[a240] B1 real offset map %s: %llu bytes on disk\n", m.offset.c_str(), static_cast<unsigned long long>(fs::file_size(m.offset)));
    XCal x;
    ASSERT_TRUE(readXCal(m.offset, &x));
    std::printf("[a240] B1 header: version %u type %u pixel_format %u %ux%u payload %llu bytes (%.2f bytes per pixel) config %llu bytes\n", x.hdr.version,
                x.hdr.type, x.hdr.pixel_format, x.hdr.width, x.hdr.height, static_cast<unsigned long long>(x.hdr.payload_len),
                static_cast<double>(x.hdr.payload_len) / N, static_cast<unsigned long long>(x.hdr.config_json_len));

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const auto t0 = Clock::now();
    const int rc = xpe_calib_load_offset(m.offset.c_str());
    char extra[64];
    std::snprintf(extra, sizeof extra, "(%.1f ms)", ms(t0, Clock::now()));
    say("load the real offset map", rc, extra);
    drainAlerts("after load");

    // its content: the product's own offset stage on the dark frame it was made from
    std::vector<uint16_t> dark = readRaw16(env("XPE_A240_CAL") + "/dark.raw");
    std::vector<uint16_t> out(N);
    XpeImageBuffer in = bufferOf(dark.data(), W, H, XPE_PIXEL_UINT16), ob = bufferOf(out.data(), W, H, XPE_PIXEL_UINT16);
    XpeImageMetadata meta{};
    say("xpe_offset_correct on dark.raw (the frame the map was made from)", xpe_offset_correct(&in, &ob, &meta));
    double sum = 0;
    for (size_t i = 0; i < N; ++i) sum += out[i];
    std::printf("[a240] B1 mean of the corrected dark: %.4f ADU (circular: the map IS this frame)\n", sum / N);

    // dimension: a 1024x1024 map and a 3072x3072 frame
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string small = (fs::path(m.dir) / "offset_1024.xcal").string();
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(small.c_str(), 1024, 1024, 200.0f));
    say("load a 1024x1024 offset map", xpe_calib_load_offset(small.c_str()));
    say("xpe_offset_correct 3072x3072 frame with the 1024x1024 map loaded", xpe_offset_correct(&in, &ob, &meta));
    drainAlerts("dimension");
    // wrong file type and missing file
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    say("load the GAIN file with the offset loader", xpe_calib_load_offset(m.gain.c_str()));
    say("load the DEFECT file with the offset loader", xpe_calib_load_offset(m.defect.c_str()));
    say("load a path that does not exist", xpe_calib_load_offset((fs::path(m.dir) / "nope.xcal").string().c_str()));
    say("load a null path", xpe_calib_load_offset(nullptr));
    drainAlerts("wrong type");
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B2  gain map: load, dimension, value range
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B2_GainLoadAndRange) {
    const Maps m = realMaps();
    XCal x;
    ASSERT_TRUE(readXCal(m.gain, &x));
    const float* g = reinterpret_cast<const float*>(x.payload.data());
    size_t below = 0, above = 0, nonfinite = 0;
    double sum = 0, mn = 1e30, mx = -1e30;
    for (size_t i = 0; i < N; ++i) {
        if (!std::isfinite(g[i])) { ++nonfinite; continue; }
        if (g[i] < 0.1f) ++below;
        if (g[i] > 10.0f) ++above;
        sum += g[i];
        mn = std::min<double>(mn, g[i]);
        mx = std::max<double>(mx, g[i]);
    }
    std::printf("[a240] B2 real gain map: %llu bytes on disk, %ux%u, payload %.2f bytes per pixel; mean %.4f min %.4f max %.4f; below 0.1: %zu above 10: %zu non-finite: %zu\n",
                static_cast<unsigned long long>(fs::file_size(m.gain)), x.hdr.width, x.hdr.height, static_cast<double>(x.hdr.payload_len) / N, sum / N, mn, mx, below, above, nonfinite);
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const auto t0 = Clock::now();
    const int rc = xpe_calib_load_gain(m.gain.c_str());
    char extra[64];
    std::snprintf(extra, sizeof extra, "(%.1f ms)", ms(t0, Clock::now()));
    say("load the real gain map", rc, extra);
    drainAlerts("after load of the real gain map");

    // value range on small maps (64x64): out of [0.1, 10.0]
    const uint32_t S = 64;
    const size_t n = static_cast<size_t>(S) * S;
    struct Case { const char* name; float v; };
    for (const Case c : {Case{"one pixel 0.05 (below 0.1)", 0.05f}, Case{"one pixel 20.0 (above 10.0)", 20.0f}, Case{"one pixel 0.0 (zero)", 0.0f},
                         Case{"one pixel NaN", std::nanf("")}, Case{"one pixel -1.0 (negative)", -1.0f}}) {
        std::vector<float> map(n, 1.0f);
        map[100] = c.v;
        const std::string p = (fs::path(m.dir) / "gain_small.xcal").string();
        ASSERT_EQ(XPE_OK, writeMap(p, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, S, S, map.data(), n * sizeof(float)));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        char label[96];
        std::snprintf(label, sizeof label, "load a 64x64 gain map with %s", c.name);
        say(label, xpe_calib_load_gain(p.c_str()));
        drainAlerts("range");
        // what the gain stage does with that pixel
        std::vector<uint16_t> raw(n, 1000), dummy(n);
        std::vector<float> outf(n, -7.0f);
        XpeImageBuffer in = bufferOf(raw.data(), S, S, XPE_PIXEL_UINT16), ob = bufferOf(outf.data(), S, S, XPE_PIXEL_FLOAT32);
        XpeImageMetadata meta{};
        const int grc = xpe_gain_correct(&in, &ob, &meta);
        std::snprintf(label, sizeof label, "xpe_gain_correct with that map (pixel 100 -> %.3f, pixel 101 -> %.3f)", outf[100], outf[101]);
        say(label, grc);
        drainAlerts("gain stage");
    }
    // dimension: a 1024x1024 gain map, then a frame of another size
    {
        const std::string p = (fs::path(m.dir) / "gain_1024.xcal").string();
        ASSERT_EQ(XPE_OK, MakeGainXCal(p.c_str(), 1024, 1024, 1.0f));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        say("load a 1024x1024 gain map", xpe_calib_load_gain(p.c_str()));
        std::vector<uint16_t> raw(N, 1000);
        std::vector<float> outf(N);
        XpeImageBuffer in = bufferOf(raw.data(), W, H, XPE_PIXEL_UINT16), ob = bufferOf(outf.data(), W, H, XPE_PIXEL_FLOAT32);
        XpeImageMetadata meta{};
        say("xpe_gain_correct 3072x3072 frame with the 1024x1024 map loaded", xpe_gain_correct(&in, &ob, &meta));
        drainAlerts("gain dimension");
    }
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B3  defect map: load and integrity
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B3_DefectLoadAndIntegrity) {
    const Maps m = realMaps();
    XCal x;
    ASSERT_TRUE(readXCal(m.defect, &x));
    size_t counts[256] = {};
    for (const uint8_t v : x.payload) ++counts[v];
    size_t masked = N - counts[0];
    std::printf("[a240] B3 real defect map (BPMap.map in an XCal file): %llu bytes on disk, %ux%u, masked pixels %zu = %.3f %%; values: ", static_cast<unsigned long long>(fs::file_size(m.defect)), x.hdr.width, x.hdr.height, masked, 100.0 * masked / N);
    for (int v = 1; v < 256; ++v) if (counts[v]) std::printf("%d:%zu ", v, counts[v]);
    std::printf("\n");
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const auto t0 = Clock::now();
    const int rc = xpe_calib_load_defect_map(m.defect.c_str());
    char extra[64];
    std::snprintf(extra, sizeof extra, "(%.1f ms)", ms(t0, Clock::now()));
    say("load the real defect map", rc, extra);
    drainAlerts("after load");

    // integrity: flip one payload byte / one header byte / cut the file / flip a byte of the stored hash
    const std::vector<unsigned char> good = readFile(m.defect);
    const auto tryFile = [&](const char* label, std::vector<unsigned char> bytes) {
        const std::string p = (fs::path(m.dir) / "defect_damaged.xcal").string();
        writeFileBytes(p, bytes);
        xpe_preprocess_shutdown();
        EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        say(label, xpe_calib_load_defect_map(p.c_str()));
        drainAlerts("damaged");
    };
    { auto b = good; b[sizeof(XCalFileHeader) + 5000] ^= 0x01; tryFile("one payload bit flipped", b); }
    { auto b = good; b[16] ^= 0x01; tryFile("one header byte flipped (width)", b); }
    { auto b = good; b[130] ^= 0xFF; tryFile("the stored SHA-256 changed", b); }
    { auto b = good; b.resize(b.size() / 2); tryFile("file cut in half", b); }
    { auto b = good; b[0] = 'Z'; tryFile("magic changed", b); }
    { std::vector<unsigned char> b; tryFile("empty file", b); }
    // RLE
    {
        const std::string p = (fs::path(m.dir) / "defect_rle.xcal").string();
        XCal y = x;
        y.hdr.sha256[0] = 0;
        const XpeErrorCode wrc = write_xcal_file_ex(p.c_str(), y.hdr, nullptr, 0, y.payload.data(), y.payload.size(), true);
        say("write the real defect map with RLE", wrc, (std::string("size ") + std::to_string(fs::exists(p) ? fs::file_size(p) : 0) + " bytes").c_str());
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        say("load the RLE defect map", xpe_calib_load_defect_map(p.c_str()));
        drainAlerts("rle");
    }
    // density above 5 %
    {
        const uint32_t S = 64;
        const size_t n = static_cast<size_t>(S) * S;
        for (const int pct : {4, 6, 30}) {
            std::vector<uint8_t> map(n, 0);
            for (size_t i = 0; i < n * static_cast<size_t>(pct) / 100; ++i) map[(i * 37) % n] = 1;
            size_t set = 0;
            for (const uint8_t v : map) set += (v != 0);
            const std::string p = (fs::path(m.dir) / "defect_density.xcal").string();
            ASSERT_EQ(XPE_OK, writeMap(p, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, S, S, map.data(), n));
            xpe_preprocess_shutdown();
            ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
            char label[96];
            std::snprintf(label, sizeof label, "load a 64x64 defect map with %.1f %% masked", 100.0 * set / n);
            say(label, xpe_calib_load_defect_map(p.c_str()));
            drainAlerts("density");
        }
    }
    // dimension
    {
        const uint32_t S = 1024;
        std::vector<uint8_t> map(static_cast<size_t>(S) * S, 0);
        const std::string p = (fs::path(m.dir) / "defect_1024.xcal").string();
        ASSERT_EQ(XPE_OK, writeMap(p, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, S, S, map.data(), map.size()));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        say("load a 1024x1024 defect map", xpe_calib_load_defect_map(p.c_str()));
        std::vector<float> frame(N, 100.0f), out(N);
        XpeImageBuffer in = bufferOf(frame.data(), W, H, XPE_PIXEL_FLOAT32), ob = bufferOf(out.data(), W, H, XPE_PIXEL_FLOAT32);
        XpeImageMetadata meta{};
        say("xpe_defect_correct 3072x3072 frame with the 1024x1024 map loaded", xpe_defect_correct(&in, &ob, &meta));
        drainAlerts("defect dimension");
    }
    xpe_preprocess_shutdown();
}

// B3 (SRS-CALIB-FUNC-003 "maximum 5 % defect density tolerance"): does anything say so when the density is above it?
TEST(A240, DISABLED_B3b_DensityAboveFivePercent) {
    const Maps m = realMaps();
    const uint32_t S = 128;
    const size_t n = static_cast<size_t>(S) * S;
    for (const int pct : {4, 6, 30}) {
        std::vector<uint8_t> map(n, 0);
        for (size_t i = 0; i < n * static_cast<size_t>(pct) / 100; ++i) map[(i * 37) % n] = 1;
        size_t set = 0;
        for (const uint8_t v : map) set += (v != 0);
        const std::string po = (fs::path(m.dir) / "small_offset.xcal").string(), pg = (fs::path(m.dir) / "small_gain.xcal").string(), pd = (fs::path(m.dir) / "small_defect.xcal").string();
        // the three maps from one writer so that they carry one session id (maps with different session ids are refused together)
        std::vector<float> offv(n, 100.0f), gainv(n, 1.0f);
        ASSERT_EQ(XPE_OK, writeMap(po, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, S, S, offv.data(), n * sizeof(float)));
        ASSERT_EQ(XPE_OK, writeMap(pg, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, S, S, gainv.data(), n * sizeof(float)));
        ASSERT_EQ(XPE_OK, writeMap(pd, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, S, S, map.data(), n));
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset(po.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(pg.c_str()));
        const int lrc = xpe_calib_load_defect_map(pd.c_str());
        char label[128];
        std::snprintf(label, sizeof label, "%dx%d frame, defect map %.1f %% masked: load", S, S, 100.0 * set / n);
        say(label, lrc);
        const int loadAlerts = drainAlerts("load");
        std::vector<float> frame(n, 0.0f);
        uint16_t* raw = reinterpret_cast<uint16_t*>(frame.data());
        for (size_t i = 0; i < n; ++i) raw[i] = static_cast<uint16_t>(1000 + i % 50);
        XpeImageBuffer img = bufferOf(frame.data(), S, S, XPE_PIXEL_UINT16);
        img.dataSize = n * sizeof(float);
        XpeImageMetadata meta{};
        g_alertSeen = false;
        g_alertNeedle = "DENSITY";
        const int prc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, kBasicCfg);
        std::snprintf(label, sizeof label, "%dx%d frame, defect map %.1f %% masked: pipeline", S, S, 100.0 * set / n);
        say(label, prc, loadAlerts ? "[the load raised alerts]" : "[the load raised no alert]");
        g_alertNeedle = "OVER_LIMIT";
        const int a2 = drainAlerts("pipeline");
        std::printf("[a240] B3b   pipeline raised %d alert(s); one naming a density/limit: %s\n", a2, g_alertSeen ? "yes" : "see the alert lines above");
        g_alertNeedle.clear();
    }
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B4  safe behaviour: no map, damaged map, expired map; the input buffer
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B4_SafeBehaviour) {
    const Maps m = realMaps();
    std::vector<uint16_t> wrist = readRaw16(env("XPE_A240_WRIST"));
    ASSERT_FALSE(wrist.empty());
    const auto run = [&](const char* label, const char* cfg = nullptr) {
        Frame f(wrist);
        const std::vector<float> before = f.data;
        const int rc = xpe_preprocess_pipeline_ex(&f.img, &f.meta, nullptr, nullptr, cfg);
        const bool changed = std::memcmp(before.data(), f.data.data(), N * sizeof(float)) != 0;
        say(label, rc, changed ? "[the caller's buffer CHANGED]" : "[the caller's buffer unchanged]");
        drainAlerts(label);
        return rc;
    };
    // (a) nothing loaded
    xpe_preprocess_shutdown();
    say("pipeline with the module not initialized", [&] { Frame f(wrist); return xpe_preprocess_pipeline_ex(&f.img, &f.meta, nullptr, nullptr, nullptr); }());
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    run("pipeline, module initialized, NO map loaded");
    // (b) one map missing
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(m.offset.c_str()));
    run("pipeline, only the offset map loaded");
    xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(m.gain.c_str()));
    run("pipeline, only the gain map loaded");
    xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(m.offset.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(m.gain.c_str()));
    run("pipeline, offset and gain loaded, NO defect map (defect stage on)");
    run("pipeline, offset and gain loaded, defect stage bypassed", "{\"bypassDefect\":true}");
    run("pipeline, offset and gain loaded, offset stage bypassed", "{\"bypassOffset\":true}");
    run("pipeline, offset and gain loaded, gain stage bypassed", "{\"bypassGain\":true}");

    // (c) a damaged map while a good set is loaded, then the pipeline
    xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    Loaded l = loadAll(m);
    ASSERT_EQ(XPE_OK, l.rcOffset); ASSERT_EQ(XPE_OK, l.rcGain); ASSERT_EQ(XPE_OK, l.rcDefect);
    const int goodRc = run("pipeline with the good set (reference)", kBasicCfg);
    {
        auto bytes = readFile(m.offset);
        bytes[sizeof(XCalFileHeader) + 123456] ^= 0x10;
        const std::string p = (fs::path(m.dir) / "offset_damaged.xcal").string();
        writeFileBytes(p, bytes);
        say("load a DAMAGED offset map while the good set is loaded", xpe_calib_load_offset(p.c_str()));
        drainAlerts("damaged load");
        const int after = run("pipeline right after that failed load", kBasicCfg);
        std::printf("[a240] B4 after the failed load the pipeline %s the good set (rc %d, before %d)\n", after == goodRc ? "still runs on" : "does NOT run on", after, goodRc);
    }
    // (d) a damaged map as the FIRST load
    {
        xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const std::string p = (fs::path(m.dir) / "offset_damaged.xcal").string();
        say("load the damaged offset map first", xpe_calib_load_offset(p.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(m.gain.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(m.defect.c_str()));
        run("pipeline with a damaged offset, good gain and defect", kBasicCfg);
    }
    // (e) expired
    {
        XCal x;
        ASSERT_TRUE(readXCal(m.offset, &x));
        const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (const int64_t exp : {now - 86400000ll, now + 86400000ll * 30, int64_t{0}}) {
            XCal y = x;
            y.hdr.expiry_epoch_ms = exp;
            const std::string p = (fs::path(m.dir) / "offset_expiry.xcal").string();
            ASSERT_EQ(XPE_OK, writeXCal(p, y));
            xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
            char label[128];
            std::snprintf(label, sizeof label, "load an offset map whose expiry is %s", exp == 0 ? "0 (no expiry)" : (exp < now ? "YESTERDAY" : "in 30 days"));
            g_alertSeen = false;
            g_alertNeedle = "XPE_WARN_NO_EXPIRY";
            say(label, xpe_calib_load_offset(p.c_str()));
            drainAlerts("expiry load");
            std::printf("[a240] B4   an alert containing XPE_WARN_NO_EXPIRY was raised by that load: %s\n", g_alertSeen ? "yes" : "no");
            g_alertNeedle.clear();
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain(m.gain.c_str()));
            ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(m.defect.c_str()));
            std::snprintf(label, sizeof label, "pipeline with that offset map (expiry %s)", exp == 0 ? "0" : (exp < now ? "yesterday" : "in 30 days"));
            run(label, kBasicCfg);
            bool isExpired = false;
            int32_t remaining = 0;
            const int ex = xpe_calib_check_expiry(p.c_str(), &isExpired, &remaining);
            char extra2[96];
            std::snprintf(extra2, sizeof extra2, "[is_expired %d, remaining_days %d]", static_cast<int>(isExpired), static_cast<int>(remaining));
            say("xpe_calib_check_expiry on that file", ex, extra2);
            drainAlerts("check_expiry");
        }
    }
    // (e2) the bypass flags with the full set loaded (SAFE-001: offset and gain correction non-bypassable)
    {
        xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        loadAll(m);
        drainAlerts("full set");
        run("full set loaded, bypassOffset=true", "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassOffset\":true}");
        run("full set loaded, bypassGain=true", "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGain\":true}");
        run("full set loaded, bypassOffset and bypassGain", "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassOffset\":true,\"bypassGain\":true}");
    }
    // (e3) an expiry that passes WHILE the map is loaded
    {
        XCal x;
        ASSERT_TRUE(readXCal(m.offset, &x));
        const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        x.hdr.expiry_epoch_ms = now + 6000;
        const std::string p = (fs::path(m.dir) / "offset_expiry_soon.xcal").string();
        ASSERT_EQ(XPE_OK, writeXCal(p, x));
        xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        say("load an offset map that expires in 6 s", xpe_calib_load_offset(p.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(m.gain.c_str()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(m.defect.c_str()));
        run("pipeline right after loading it", kBasicCfg);
        std::this_thread::sleep_for(std::chrono::seconds(8));
        run("pipeline 8 s later (the map has expired while loaded)", kBasicCfg);
    }
    // (f) the input buffer and the stage APIs
    {
        xpe_preprocess_shutdown(); ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        loadAll(m);
        std::vector<uint16_t> raw = wrist, out(N);
        XpeImageBuffer in = bufferOf(raw.data(), W, H, XPE_PIXEL_UINT16), ob = bufferOf(out.data(), W, H, XPE_PIXEL_UINT16);
        XpeImageMetadata meta{};
        say("xpe_offset_correct (separate output buffer)", xpe_offset_correct(&in, &ob, &meta));
        std::printf("[a240] B4 xpe_offset_correct left the input buffer %s\n", std::memcmp(raw.data(), wrist.data(), N * 2) == 0 ? "unchanged" : "CHANGED");
        std::vector<float> outf(N);
        XpeImageBuffer ib2 = bufferOf(raw.data(), W, H, XPE_PIXEL_UINT16), ob2 = bufferOf(outf.data(), W, H, XPE_PIXEL_FLOAT32);
        say("xpe_gain_correct (separate output buffer)", xpe_gain_correct(&ib2, &ob2, &meta));
        std::printf("[a240] B4 xpe_gain_correct left the input buffer %s\n", std::memcmp(raw.data(), wrist.data(), N * 2) == 0 ? "unchanged" : "CHANGED");
        drainAlerts("stage apis");
    }
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B5  the real frame, offset -> gain -> defect
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B5_RealFrameOrder) {
    const Maps m = realMaps();
    std::vector<uint16_t> wrist = readRaw16(env("XPE_A240_WRIST"));
    ASSERT_FALSE(wrist.empty());
    XCal xo, xg, xd;
    ASSERT_TRUE(readXCal(m.offset, &xo));
    ASSERT_TRUE(readXCal(m.gain, &xg));
    ASSERT_TRUE(readXCal(m.defect, &xd));
    const float* off = reinterpret_cast<const float*>(xo.payload.data());
    const float* gain = reinterpret_cast<const float*>(xg.payload.data());
    const uint8_t* bp = xd.payload.data();

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const Loaded l = loadAll(m);
    ASSERT_EQ(XPE_OK, l.rcOffset); ASSERT_EQ(XPE_OK, l.rcGain); ASSERT_EQ(XPE_OK, l.rcDefect);
    drainAlerts("load");

    Frame f(wrist);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&f.img, &f.meta, nullptr, nullptr, kBasicCfg));
    std::printf("[a240] B5 pipeline (basic config): format %d flags 0x%x\n", static_cast<int>(f.img.format), static_cast<unsigned>(f.meta.flags));
    drainAlerts("pipeline basic");
    Frame d(wrist);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&d.img, &d.meta, nullptr, nullptr, nullptr));
    std::printf("[a240] B5 pipeline (default config) equals the basic config byte for byte: %s; flags 0x%x\n", std::memcmp(f.data.data(), d.data.data(), N * 4) == 0 ? "yes" : "NO", static_cast<unsigned>(d.meta.flags));
    drainAlerts("pipeline default");
    Frame nd(wrist);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&nd.img, &nd.meta, nullptr, nullptr, "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}"));
    drainAlerts("pipeline no defect");

    // the expectation, worked out here from the map files and the formulas of the requirement
    size_t gainBad = 0;
    std::vector<uint8_t> masked(N);
    size_t nMasked = 0;
    for (size_t i = 0; i < N; ++i) {
        const bool bad = !(std::isfinite(gain[i]) && gain[i] >= 0.1f && gain[i] <= 10.0f);
        gainBad += bad;
        masked[i] = (bp[i] != 0 || bad) ? 1 : 0;
        nMasked += masked[i];
    }
    std::printf("[a240] B5 masked pixels: map %zu, gain outside [0.1,10] %zu, union %zu\n", N - static_cast<size_t>(std::count(bp, bp + N, uint8_t{0})), gainBad, nMasked);
    std::vector<float> expect(N);   // offset (+0.5 rounding, clamped to uint16) then gain (x * 1/g; a classified pixel keeps gain 1.0)
    for (size_t i = 0; i < N; ++i) {
        float c = static_cast<float>(wrist[i]) - off[i];
        c = std::min(std::max(c, 0.0f), 65535.0f);
        const uint16_t u = static_cast<uint16_t>(c + 0.5f);
        const float g = (std::isfinite(gain[i]) && gain[i] >= 0.1f && gain[i] <= 10.0f) ? gain[i] : 1.0f;
        expect[i] = static_cast<float>(u) * (1.0f / g);
    }
    // (1) every pixel the defect stage does not touch must equal that expectation
    size_t good = 0, goodEqual = 0, goodWithin = 0;
    double goodMax = 0;
    for (size_t i = 0; i < N; ++i) {
        if (masked[i]) continue;
        ++good;
        const double dd = std::fabs(static_cast<double>(f.data[i]) - expect[i]);
        goodEqual += (f.data[i] == expect[i]);
        goodWithin += (dd <= 1e-3 * std::max(1.0, std::fabs(static_cast<double>(expect[i]))) * 1e-3);
        goodMax = std::max(goodMax, dd);
    }
    std::printf("[a240] B5 unmasked pixels %zu: equal to offset->gain expectation %zu, max |diff| %.6g\n", good, goodEqual, goodMax);
    // the stages without the defect stage give the same numbers
    size_t ndEqual = 0;
    for (size_t i = 0; i < N; ++i) ndEqual += (nd.data[i] == expect[i]);
    std::printf("[a240] B5 pipeline without the defect stage equals the expectation on %zu of %zu pixels\n", ndEqual, N);
    // (2) the order: gain before offset gives something else
    double orderDiff = 0, orderRel = 0;
    size_t orderDiffer = 0;
    for (size_t i = 0; i < N; ++i) {
        if (masked[i]) continue;
        const float g = gain[i];
        const double alt = std::max(0.0, static_cast<double>(wrist[i]) / g - off[i]);   // gain first, then offset
        const double dd = std::fabs(alt - f.data[i]);
        if (dd > 0.5) ++orderDiffer;
        orderDiff = std::max(orderDiff, dd);
        orderRel += dd;
    }
    std::printf("[a240] B5 if the order were gain->offset instead: %zu unmasked pixels would differ by > 0.5 ADU, max %.2f, mean %.3f ADU\n", orderDiffer, orderDiff, orderRel / good);
    // (3) the masked pixels: isolated ones equal the mean of their valid 4 neighbours; clusters stay inside the range of their valid neighbours
    size_t iso = 0, isoEqual = 0, cluster = 0, clusterInRange = 0, changed = 0, unchanged = 0;
    double isoMax = 0;
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (!masked[i]) continue;
            if (f.data[i] != nd.data[i]) ++changed; else ++unchanged;
            bool hasMaskedNeighbour = false;
            const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, -1, 1};
            float sum = 0; int cnt = 0; float lo = 1e30f, hi = -1e30f;
            for (int k = 0; k < 4; ++k) {
                const int nx = static_cast<int>(x) + dx[k], ny = static_cast<int>(y) + dy[k];
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(W) || ny >= static_cast<int>(H)) continue;
                const size_t j = static_cast<size_t>(ny) * W + nx;
                if (masked[j]) { hasMaskedNeighbour = true; continue; }
                sum += expect[j]; ++cnt; lo = std::min(lo, expect[j]); hi = std::max(hi, expect[j]);
            }
            if (!hasMaskedNeighbour && cnt > 0) {
                ++iso;
                const double dd = std::fabs(static_cast<double>(f.data[i]) - sum / static_cast<float>(cnt));
                isoMax = std::max(isoMax, dd);
                isoEqual += (dd < 1e-3);
            } else {
                ++cluster;
                // the 3x3 valid neighbours' range (a cluster pixel is filled from the median of the valid pixels around it)
                float l3 = 1e30f, h3 = -1e30f; int c3 = 0;
                for (int yy = -3; yy <= 3 && c3 == 0; ++yy) {}
                for (int r = 1; r <= 16 && c3 == 0; ++r) {
                    for (int yy = -r; yy <= r; ++yy) for (int xx = -r; xx <= r; ++xx) {
                        if (std::max(std::abs(xx), std::abs(yy)) != r) continue;
                        const int nx = static_cast<int>(x) + xx, ny = static_cast<int>(y) + yy;
                        if (nx < 0 || ny < 0 || nx >= static_cast<int>(W) || ny >= static_cast<int>(H)) continue;
                        const size_t j = static_cast<size_t>(ny) * W + nx;
                        if (masked[j]) continue;
                        l3 = std::min(l3, expect[j]); h3 = std::max(h3, expect[j]); ++c3;
                    }
                }
                if (c3 > 0 && f.data[i] >= l3 - 1e-3f && f.data[i] <= h3 + 1e-3f) ++clusterInRange;
            }
        }
    }
    std::printf("[a240] B5 masked pixels %zu: changed by the defect stage %zu, left as they were %zu\n", nMasked, changed, unchanged);
    std::printf("[a240] B5 isolated masked pixels %zu: equal to the mean of their valid 4-neighbours (|diff| < 1e-3) %zu, max |diff| %.6g\n", iso, isoEqual, isoMax);
    std::printf("[a240] B5 clustered masked pixels %zu: inside the value range of the nearest ring of valid pixels %zu\n", cluster, clusterInRange);
    size_t nan = 0;
    for (size_t i = 0; i < N; ++i) nan += !std::isfinite(f.data[i]);
    double mean = 0;
    for (size_t i = 0; i < N; ++i) mean += f.data[i];
    std::printf("[a240] B5 output: NaN/Inf %zu, mean %.3f (raw mean %.3f)\n", nan, mean / N, [&] { double s = 0; for (size_t i = 0; i < N; ++i) s += wrist[i]; return s / N; }());
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B6  quality: dark residual, flat residual, defect residual
// ------------------------------------------------------------------------------------------------------------------
namespace {
/** The pixel noise of a nearly flat frame, as a coefficient of variation: pixel minus the mean of its 3x3 has variance (8/9) sigma^2 for
 *  independent noise, so sigma = std(pixel - mean3x3) / sqrt(8/9). Interior pixels only. */
double noiseCv(const float* v) {
    double s = 0, s2 = 0, sm = 0;
    size_t c = 0;
    for (uint32_t y = 1; y + 1 < H; ++y) {
        for (uint32_t x = 1; x + 1 < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            double m3 = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) m3 += v[i + static_cast<size_t>(dy) * W + static_cast<size_t>(dx)];
            m3 /= 9.0;
            const double d = v[i] - m3;
            s += d; s2 += d * d; sm += v[i]; ++c;
        }
    }
    const double n = static_cast<double>(c);
    const double sd = std::sqrt(std::max(0.0, s2 / n - (s / n) * (s / n)) / (8.0 / 9.0));
    return sd / (sm / n);
}
/** The coefficient of variation of the means of 64x64 tiles: what is left of the non-uniformity once the pixel noise averages out. */
double tileCv(const float* v) {
    std::vector<double> t;
    for (uint32_t ty = 0; ty + 64 <= H; ty += 64) {
        for (uint32_t tx = 0; tx + 64 <= W; tx += 64) {
            double s = 0;
            for (uint32_t y = 0; y < 64; ++y) for (uint32_t x = 0; x < 64; ++x) s += v[static_cast<size_t>(ty + y) * W + tx + x];
            t.push_back(s / 4096.0);
        }
    }
    double m = 0, m2 = 0;
    for (const double x : t) { m += x; m2 += x * x; }
    m /= static_cast<double>(t.size());
    return std::sqrt(std::max(0.0, m2 / static_cast<double>(t.size()) - m * m)) / m;
}
struct Stat { double mean, sd; };
Stat statOf(const float* v, size_t n, const uint8_t* skip) {
    double s = 0, s2 = 0; size_t c = 0;
    for (size_t i = 0; i < n; ++i) { if (skip && skip[i]) continue; s += v[i]; s2 += static_cast<double>(v[i]) * v[i]; ++c; }
    const double mean = s / static_cast<double>(c);
    return {mean, std::sqrt(std::max(0.0, s2 / static_cast<double>(c) - mean * mean))};
}
}  // namespace

TEST(A240, DISABLED_B6_Quality) {
    const Maps m = realMaps();
    const std::string cal = env("XPE_A240_CAL");
    XCal xg, xd;
    ASSERT_TRUE(readXCal(m.gain, &xg));
    ASSERT_TRUE(readXCal(m.defect, &xd));
    const float* gain = reinterpret_cast<const float*>(xg.payload.data());
    const uint8_t* bp = xd.payload.data();
    std::vector<uint8_t> masked(N);
    for (size_t i = 0; i < N; ++i) masked[i] = (bp[i] != 0 || !(std::isfinite(gain[i]) && gain[i] >= 0.1f && gain[i] <= 10.0f)) ? 1 : 0;

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_EQ(XPE_OK, loadAll(m).rcOffset);
    drainAlerts("load");
    std::vector<uint16_t> dark = readRaw16(cal + "/dark.raw");
    std::vector<std::vector<uint16_t>> bright(6);
    for (int k = 0; k < 6; ++k) { char nm[32]; std::snprintf(nm, sizeof nm, "/bright%02d.raw", k + 1); bright[static_cast<size_t>(k)] = readRaw16(cal + nm); }

    // ---- dark (SRS-CALIB-FUNC-016): the offset stage on the dark frame (the map IS this frame: circular)
    {
        std::vector<uint16_t> out(N);
        XpeImageBuffer in = bufferOf(dark.data(), W, H, XPE_PIXEL_UINT16), ob = bufferOf(out.data(), W, H, XPE_PIXEL_UINT16);
        XpeImageMetadata meta{};
        ASSERT_EQ(XPE_OK, xpe_offset_correct(&in, &ob, &meta));
        XCal xo;
        ASSERT_TRUE(readXCal(m.offset, &xo));
        const float* offmap = reinterpret_cast<const float*>(xo.payload.data());
        double sr = 0, sr2 = 0, sy = 0, sy2 = 0; size_t clamped = 0, zeros = 0;
        for (size_t i = 0; i < N; ++i) { sr += dark[i]; sr2 += static_cast<double>(dark[i]) * dark[i]; sy += out[i]; sy2 += static_cast<double>(out[i]) * out[i]; clamped += (static_cast<float>(dark[i]) - offmap[i] < 0.0f); zeros += (out[i] == 0); }
        const double mr = sr / N, my = sy / N, stdR = std::sqrt(sr2 / N - mr * mr), stdY = std::sqrt(std::max(0.0, sy2 / N - my * my));
        std::printf("[a240] B6 dark (all pixels as the ROI; circular): DarkBias %.4f ADU, DSNU_ADU %.4f, DarkReduction_dB %.2f (std raw %.3f -> %.3f), ClampRate (R - D < 0) %.4f %%, pixels that came out 0: %.4f %%\n", my, stdY, 20.0 * std::log10(stdR / std::max(stdY, 1e-12)), stdR, stdY, 100.0 * static_cast<double>(clamped) / N, 100.0 * static_cast<double>(zeros) / N);
        XpeCalibrationMetrics met{};
        const int vrc = xpe_verify_offset(&in, &ob, &meta, &met);
        std::printf("[a240] B6 xpe_verify_offset rc=%d: dark_bias %.4f dsnu_adu %.4f dark_reduction_db %.2f overall_pass %d\n", vrc, met.dark_bias, met.dsnu_adu, met.dark_reduction_db, static_cast<int>(met.overall_pass));
    }
    // ---- flat (SRS-CALIB-FUNC-017): for each real flat, raw = dark + bright; offset, gain, then defect
    std::printf("[a240] B6 flat: FlatResidualPct = 100 * std/mean over EVERY pixel whose gain is valid (protocol 5.3), with the map's own gain from all 6 flats\n");
    for (int k = 0; k < 6; ++k) {
        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(std::min<uint32_t>(65535u, static_cast<uint32_t>(dark[i]) + bright[static_cast<size_t>(k)][i]));
        Frame withDefect(raw), noDefect(raw);
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&withDefect.img, &withDefect.meta, nullptr, nullptr, kBasicCfg));
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&noDefect.img, &noDefect.meta, nullptr, nullptr, "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}"));
        drainAlerts("flat pipeline");
        std::vector<float> pre(N);   // offset-corrected, before gain
        for (size_t i = 0; i < N; ++i) pre[i] = static_cast<float>(bright[static_cast<size_t>(k)][i]);
        const Stat before = statOf(pre.data(), N, nullptr);
        const Stat afterAll = statOf(noDefect.data.data(), N, nullptr);
        const Stat afterAllMasked = statOf(noDefect.data.data(), N, masked.data());
        const Stat beforeMasked = statOf(pre.data(), N, masked.data());
        const Stat finalAll = statOf(withDefect.data.data(), N, nullptr);
        std::printf("[a240] B6 flat %d (mean %.1f ADU): FlatResidualPct before gain %.3f | after gain, all pixels %.3f | after gain, masked pixels left out %.3f (before, same pixels %.3f) | after the defect stage, all pixels %.3f | FPN_Reduction_dB (masked out) %.2f\n",
                    k + 1, before.mean, 100 * before.sd / before.mean, 100 * afterAll.sd / afterAll.mean, 100 * afterAllMasked.sd / afterAllMasked.mean, 100 * beforeMasked.sd / beforeMasked.mean, 100 * finalAll.sd / finalAll.mean,
                    20.0 * std::log10(beforeMasked.sd / std::max(afterAllMasked.sd, 1e-12)));
        {
            const double nz = noiseCv(withDefect.data.data()), tl = tileCv(withDefect.data.data());
            const double nzRaw = noiseCv(pre.data());
            std::printf("[a240] B6 flat %d split (after the defect stage): pixel-noise CV %.3f %% (before gain %.3f %%) | 64x64-tile-mean CV %.3f %% | residual beyond pixel noise sqrt(total^2 - noise^2) %.3f %%\n",
                        k + 1, 100 * nz, 100 * nzRaw, 100 * tl, 100 * std::sqrt(std::max(0.0, finalAll.sd * finalAll.sd / (finalAll.mean * finalAll.mean) - nz * nz)));
        }
        if (k == 1) {   // the product's own verification on one flat
            std::vector<uint16_t> offc(N);
            XpeImageBuffer in = bufferOf(raw.data(), W, H, XPE_PIXEL_UINT16), ob = bufferOf(offc.data(), W, H, XPE_PIXEL_UINT16);
            XpeImageMetadata meta{};
            ASSERT_EQ(XPE_OK, xpe_offset_correct(&in, &ob, &meta));
            XpeImageBuffer gb = bufferOf(const_cast<float*>(gain), W, H, XPE_PIXEL_FLOAT32);
            XpeImageBuffer after = bufferOf(noDefect.data.data(), W, H, XPE_PIXEL_FLOAT32);
            XpeCalibrationMetrics met{};
            const int vrc = xpe_verify_gain(&ob, &after, &gb, XPE_GAIN_SEMANTICS_NORMALIZED, &met);
            std::printf("[a240] B6 xpe_verify_gain on flat 2 rc=%d: prnu_before %.4f %% prnu_after %.4f %% gain_coverage %.6f invalid_gain_count %u snr_improvement_db %.2f overall_pass %d measured_mask 0x%x\n", vrc, met.prnu_before, met.prnu_after, met.gain_coverage, met.invalid_gain_count, met.snr_improvement_db, static_cast<int>(met.overall_pass), met.measured_mask);
        }
    }
    // ---- flat, held out: the gain made from the OTHER five flats applied to the sixth
    std::printf("[a240] B6 flat held out: the gain map made from the other five flats, applied to the sixth\n");
    for (int k = 0; k < 6; ++k) {
        std::vector<XpeImageBuffer> bufs;
        for (int j = 0; j < 6; ++j) if (j != k) bufs.push_back(bufferOf(bright[static_cast<size_t>(j)].data(), W, H, XPE_PIXEL_UINT16));
        const std::string gp = (fs::path(m.dir) / "gain_loo.xcal").string();
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(bufs.data(), 5, nullptr, gp.c_str(), nullptr));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gp.c_str()));
        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(std::min<uint32_t>(65535u, static_cast<uint32_t>(dark[i]) + bright[static_cast<size_t>(k)][i]));
        Frame f(raw);
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&f.img, &f.meta, nullptr, nullptr, "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}"));
        XCal xl; ASSERT_TRUE(readXCal(gp, &xl));
        const float* gl = reinterpret_cast<const float*>(xl.payload.data());
        std::vector<uint8_t> mk(N);
        for (size_t i = 0; i < N; ++i) mk[i] = (bp[i] != 0 || !(std::isfinite(gl[i]) && gl[i] >= 0.1f && gl[i] <= 10.0f)) ? 1 : 0;
        const Stat a = statOf(f.data.data(), N, mk.data());
        std::printf("[a240] B6 flat %d held out: FlatResidualPct after gain (masked pixels left out) %.3f\n", k + 1, 100 * a.sd / a.mean);
        drainAlerts("loo");
    }
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(m.gain.c_str()));
    // ---- defect (SRS-CALIB-FUNC-019): the real frames have no ground truth; the metrics that do not need one
    {
        std::vector<uint16_t> raw(N);
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(std::min<uint32_t>(65535u, static_cast<uint32_t>(dark[i]) + bright[1][i]));
        Frame withDefect(raw), noDefect(raw);
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&withDefect.img, &withDefect.meta, nullptr, nullptr, kBasicCfg));
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&noDefect.img, &noDefect.meta, nullptr, nullptr, "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}"));
        std::vector<double> deltas;
        deltas.reserve(N);
        size_t goodChanged = 0;
        for (size_t i = 0; i < N; ++i) {
            if (masked[i]) continue;
            const double dd = std::fabs(static_cast<double>(withDefect.data[i]) - noDefect.data[i]);
            deltas.push_back(dd);
            goodChanged += (dd != 0.0);
        }
        std::sort(deltas.begin(), deltas.end());
        std::printf("[a240] B6 GoodPixelDeltaP99 (unmasked pixels, defect stage on vs off, flat 2): %.6f ADU; unmasked pixels changed: %zu of %zu\n", deltas[static_cast<size_t>(0.99 * static_cast<double>(deltas.size()))], goodChanged, deltas.size());
        // DefectResidualADU = mean |Y(defect pixels) - neighbour model|, the neighbour model being the mean of the valid 4-neighbours' corrected values
        double res = 0; size_t cnt = 0, before = 0; double resBefore = 0;
        for (uint32_t y = 1; y + 1 < H; ++y) for (uint32_t x = 1; x + 1 < W; ++x) {
            const size_t i = static_cast<size_t>(y) * W + x;
            if (!masked[i]) continue;
            float s = 0; int c = 0;
            const size_t nb[4] = {i - 1, i + 1, i - W, i + W};
            for (const size_t j : nb) if (!masked[j]) { s += withDefect.data[j]; ++c; }
            if (c == 0) continue;
            res += std::fabs(static_cast<double>(withDefect.data[i]) - s / static_cast<float>(c)); ++cnt;
            resBefore += std::fabs(static_cast<double>(noDefect.data[i]) - s / static_cast<float>(c)); ++before;
        }
        std::printf("[a240] B6 DefectResidualADU (masked pixels with a valid 4-neighbour, flat 2): after the defect stage %.4f ADU, before it %.4f ADU (%zu pixels)\n", res / static_cast<double>(cnt), resBefore / static_cast<double>(before), cnt);
        XpeImageBuffer cb = bufferOf(withDefect.data.data(), W, H, XPE_PIXEL_FLOAT32);
        std::vector<uint8_t> bpc(bp, bp + N);
        XpeImageBuffer db = bufferOf(bpc.data(), W, H, XPE_PIXEL_UINT8);
        XpeCalibrationMetrics met{};
        const int vrc = xpe_verify_defect(&cb, &db, &met);
        std::printf("[a240] B6 xpe_verify_defect rc=%d: defect_count %u defect_density %.4f %% correction_error %.4f overall_pass %d\n", vrc, met.defect_count, met.defect_density, met.correction_error, static_cast<int>(met.overall_pass));
        drainAlerts("defect metrics");
    }
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B7  frame time
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B7_FrameTime) {
    const Maps m = realMaps();
    std::vector<uint16_t> wrist = readRaw16(env("XPE_A240_WRIST"));
    ASSERT_FALSE(wrist.empty());
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const Loaded l = loadAll(m);
    ASSERT_EQ(XPE_OK, l.rcOffset); ASSERT_EQ(XPE_OK, l.rcGain); ASSERT_EQ(XPE_OK, l.rcDefect);
    drainAlerts("load");
    for (const char* label : {"default config", "basic config"}) {
        const char* cfg = (std::strcmp(label, "basic config") == 0) ? kBasicCfg : nullptr;
        std::vector<double> t;
        for (int i = 0; i < 40; ++i) {
            Frame f(wrist);
            const auto t0 = Clock::now();
            const int rc = xpe_preprocess_pipeline_ex(&f.img, &f.meta, nullptr, nullptr, cfg);
            const auto t1 = Clock::now();
            ASSERT_EQ(XPE_OK, rc);
            t.push_back(ms(t0, t1));
        }
        const double cold = t[0];
        std::vector<double> warm(t.begin() + 1, t.end());
        std::sort(warm.begin(), warm.end());
        std::printf("[a240] B7 %s, real frame, ghost off: first frame %.1f ms; 39 warm frames min %.1f median %.1f p95 %.1f max %.1f ms (limit 500 ms)\n", label, cold, warm.front(), warm[warm.size() / 2], warm[static_cast<size_t>(0.95 * static_cast<double>(warm.size()))], warm.back());
        drainAlerts("timing");
    }
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B9  map load time
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B9_LoadTime) {
    const Maps m = realMaps();
    std::printf("[a240] B9 files: offset %llu B, gain %llu B, defect %llu B\n", static_cast<unsigned long long>(fs::file_size(m.offset)), static_cast<unsigned long long>(fs::file_size(m.gain)), static_cast<unsigned long long>(fs::file_size(m.defect)));
    std::vector<double> total, o, g, d;
    for (int i = 0; i < 12; ++i) {
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const Loaded l = loadAll(m);
        ASSERT_EQ(XPE_OK, l.rcOffset); ASSERT_EQ(XPE_OK, l.rcGain); ASSERT_EQ(XPE_OK, l.rcDefect);
        o.push_back(l.msOffset); g.push_back(l.msGain); d.push_back(l.msDefect); total.push_back(l.msOffset + l.msGain + l.msDefect);
        std::printf("[a240] B9 load %2d: offset %.1f gain %.1f defect %.1f total %.1f ms\n", i + 1, l.msOffset, l.msGain, l.msDefect, total.back());
    }
    drainAlerts("load");
    const auto summary = [](const char* n, std::vector<double> v) {
        const double first = v[0];
        std::sort(v.begin(), v.end());
        std::printf("[a240] B9 %-7s first %.1f ms; over 12 loads min %.1f median %.1f max %.1f ms\n", n, first, v.front(), v[v.size() / 2], v.back());
    };
    summary("offset", o); summary("gain", g); summary("defect", d); summary("total", total);
    xpe_preprocess_shutdown();
}

// ------------------------------------------------------------------------------------------------------------------
// B10  the baseline image
// ------------------------------------------------------------------------------------------------------------------
TEST(A240, DISABLED_B10_Baseline) {
    const Maps m = realMaps();
    std::vector<uint16_t> wrist = readRaw16(env("XPE_A240_WRIST"));
    ASSERT_FALSE(wrist.empty());
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_EQ(XPE_OK, loadAll(m).rcOffset);
    drainAlerts("load");
    Frame a(wrist), b(wrist);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&a.img, &a.meta, nullptr, nullptr, kBasicCfg));
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&b.img, &b.meta, nullptr, nullptr, kBasicCfg));
    drainAlerts("pipeline");
    const bool same = std::memcmp(a.data.data(), b.data.data(), N * sizeof(float)) == 0;
    std::printf("[a240] B10 two runs in one process are byte-identical: %s\n", same ? "yes" : "NO");
    std::printf("[a240] B10 output FNV-64 %016llx, format %d, %ux%u, flags 0x%x\n", static_cast<unsigned long long>(fnv(a.data.data(), N * sizeof(float))), static_cast<int>(a.img.format), a.img.width, a.img.height, static_cast<unsigned>(a.meta.flags));
    const std::string dir = (fs::path(outDir()) / "baseline").string();
    fs::create_directories(dir);
    const std::string p = (fs::path(dir) / "wrist_lat_3072x3072_corrected_f32le.raw").string();
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(a.data.data()), static_cast<std::streamsize>(N * sizeof(float)));
    f.close();
    std::printf("[a240] B10 wrote %s (%llu bytes)\n", p.c_str(), static_cast<unsigned long long>(fs::file_size(p)));
    xpe_preprocess_shutdown();
}
