/**
 * @file test_zz_a237b_mem.cpp
 * @brief QA-A-237b (#245, SRS-CALIB-PERF-002): memory, time and output digest of the SHIPPED per-frame path
 *        (xpe_preprocess_pipeline_ex -> pipeline_core) at 3072x3072 with a ghost handle. DISABLED_ probe, not a gate.
 *
 * QA-A-236 measured a hand-chained stage sequence; this runs the entry point a client calls. Process counters are
 * read with GetProcessMemoryInfo at named checkpoints. PeakPagefileUsage only grows, so the increase between two
 * checkpoints is attributed to what ran between them; when the build defines XPE_CACHE_TEST_HOOKS (the allocation-failure
 * executable) pipeline_core reports every stage boundary through xpe_pipeline_after_stage_hook, which gives the
 * per-stage peaks.
 *
 * Run (one fresh process per measurement, the peak counter cannot be reset):
 *   xpe_preprocess_tests --gtest_also_run_disabled_tests --gtest_filter=A237bMem.DISABLED_ShippedPath
 * Environment: XPE_A237B_TIER (ghost tier 1..3, default 1), XPE_A237B_FRAMES (default 3),
 * XPE_A237B_RAW_DIR (a directory with bright01..06.raw: real frames instead of the synthetic ones),
 * XPE_A237C_BETA (ghost nlcscBeta, tier 3: a value large enough to send the frames down the ghost stage's scratch route).
 * Output lines start with "[a237b]". Every number carries its unit; MiB = 2^20 bytes.
 */

#include <gtest/gtest.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN   // without it windows.h defines `small`, which xpe_preprocess_internal.h uses as a name
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "fixtures/make_xcal.hpp"
#include "ghost_stable_lag.h"

extern "C" XPE_API void xpe_ghost_destroy(void* handle);

namespace {

constexpr uint32_t W = 3072, H = 3072;
constexpr size_t N = static_cast<size_t>(W) * H;

struct Mem { double privateMiB, peakCommitMiB, peakWsMiB; };

Mem readMem() {
    PROCESS_MEMORY_COUNTERS_EX c{};
    c.cb = sizeof(c);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&c), sizeof(c));
    const double mib = 1048576.0;
    return {static_cast<double>(c.PrivateUsage) / mib, static_cast<double>(c.PeakPagefileUsage) / mib,
            static_cast<double>(c.PeakWorkingSetSize) / mib};
}

void checkpoint(const char* name) {
    const Mem m = readMem();
    std::printf("[a237b] chk %-34s private %7.1f MiB  peakCommit %7.1f MiB  peakWs %7.1f MiB\n", name, m.privateMiB,
                m.peakCommitMiB, m.peakWsMiB);
    std::fflush(stdout);
}

// FNV-1a over 32-bit words: a digest of the output bytes, printed so two builds can be compared by eye and by script.
uint64_t digest(const void* data, size_t bytes) {
    uint64_t h = 1469598103934665603ull;
    const uint32_t* p = static_cast<const uint32_t*>(data);
    const size_t words = bytes / sizeof(uint32_t);
    for (size_t i = 0; i < words; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

int envInt(const char* name, int fallback) {
    char* v = nullptr;
    size_t len = 0;
    int out = fallback;
    if (_dupenv_s(&v, &len, name) == 0 && v != nullptr) {
        if (*v) out = std::atoi(v);
        std::free(v);
    }
    return out;
}

std::string envStr(const char* name) {
    char* v = nullptr;
    size_t len = 0;
    std::string out;
    if (_dupenv_s(&v, &len, name) == 0 && v != nullptr) {
        out = v;
        std::free(v);
    }
    return out;
}

// A raw 3072x3072 uint16 frame without a header (the CalData_6 files); empty on any problem.
std::vector<uint16_t> readRaw(const std::filesystem::path& file) {
    std::vector<uint16_t> v(N);
    FILE* f = nullptr;
    if (fopen_s(&f, file.string().c_str(), "rb") != 0 || f == nullptr) return {};
    const size_t got = std::fread(v.data(), sizeof(uint16_t), N, f);
    std::fclose(f);
    return got == N ? v : std::vector<uint16_t>{};
}

#ifdef XPE_CACHE_TEST_HOOKS
int g_frame = 0;
void onStage(int stage) {
    static const char* const names[] = {"readout", "temp", "offset", "nonlinearity", "gain",
                                        "binning", "defect", "ghost", "?", "final copy"};
    char label[64];
    std::snprintf(label, sizeof(label), "frame %d after %s", g_frame, names[(stage >= 0 && stage <= 9) ? stage : 8]);
    checkpoint(label);
}
#endif

}  // namespace

TEST(A237bMem, DISABLED_ShippedPath) {
    const int tier = envInt("XPE_A237B_TIER", 1);
    const int frames = envInt("XPE_A237B_FRAMES", 3);

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "qa_a_237b_mem";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string offPath = (dir / "offset.xcal").string();
    const std::string gainPath = (dir / "gain.xcal").string();
    const std::string defPath = (dir / "defect.xcal").string();

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    // XPE_A237B_RAW_DIR: real frames (CalData_6 layout: bright01..06.raw). Those exposures are already offset-corrected
    // (the dataset README), so the offset map is a small constant with a fractional part and the real structure survives
    // the subtraction; the gain and defect maps stay synthetic. Without the variable everything is synthetic.
    const std::string rawDir = envStr("XPE_A237B_RAW_DIR");
    // XPE_A237G_MAPS (QA-A-237g): a directory holding offset.xcal and gain.xcal made by A237gMaps from the CalData_6 files (the
    // product's own generators run on the real dark and flat frames), so that all three maps -- these two and BPMap.map -- are real.
    // The frames are then dark + bright (the bright files are already offset-corrected, see the dataset README).
    const std::string realMaps = envStr("XPE_A237G_MAPS");
    std::string offUse = offPath, gainUse = gainPath;
    if (!realMaps.empty()) {
        offUse = (std::filesystem::path(realMaps) / "offset.xcal").string();
        gainUse = (std::filesystem::path(realMaps) / "gain.xcal").string();
    } else {
        ASSERT_EQ(XPE_OK, MakeOffsetXCal(offPath.c_str(), W, H, rawDir.empty() ? 200.0f : 0.3f));
        ASSERT_EQ(XPE_OK, MakeGainXCal(gainPath.c_str(), W, H, 1.25f));
    }
    {
        std::vector<uint8_t> payload(N, 0);
        for (uint32_t k = 0; k < 16; ++k) payload[static_cast<size_t>(150 + 170 * k) * W + (100 + 180 * k)] = 1;
        // XPE_A237F_BPMAP (QA-A-237f): a real defect map (CalData_6 BPMap.map, uint8 W*H, 0 = good) in place of the 16 isolated
        // fixture pixels -- those never reach the defect stage's fill-distance table (a masked pixel with no valid pixel in
        // its 3x3), which a real map does.
        const std::string bpmap = envStr("XPE_A237F_BPMAP");
        if (!bpmap.empty()) {
            std::FILE* f = nullptr;
            ASSERT_EQ(0, fopen_s(&f, bpmap.c_str(), "rb"));
            ASSERT_EQ(payload.size(), std::fread(payload.data(), 1, payload.size(), f));
            std::fclose(f);
            size_t masked = 0;
            for (uint8_t& v : payload) { if (v != 0) { v = 1; ++masked; } }
            std::printf("[a237f] defect map %s: %zu masked pixels (%.3f %%)\n", bpmap.c_str(), masked, 100.0 * masked / payload.size());
        }
        XCalFileHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        std::memcpy(hdr.session_id, "fixture", 8);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = W;
        hdr.height = H;
        hdr.created_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch()).count();
        hdr.payload_len = payload.size();
        ASSERT_EQ(XPE_OK, write_xcal_file(defPath.c_str(), hdr, nullptr, 0, payload.data(), payload.size()));
    }

    // The caller's frame buffer: float32-sized, the uint16 input sits in its first half (the result is written back).
    std::vector<float> frame(N, 0.0f);
    XpeImageBuffer img{};
    img.width = W;
    img.height = H;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = img.bitsStored = 16;
    img.data = frame.data();
    img.dataSize = frame.size() * sizeof(float);
    XpeImageMetadata meta{};
    meta.pixelPitch_mm = 0.14f;

    checkpoint("A harness (frame buffer, fixtures written)");

    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offUse.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainUse.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
    checkpoint("B maps loaded");
#ifdef XPE_CACHE_TEST_HOOKS
    {
        const CalibSnapshot snap = xpe_calib_snapshot();
        size_t masked = 0;
        if (snap.defect_map) for (size_t i = 0; i < N; ++i) masked += (snap.defect_map[i] != 0);
        std::printf("[a237g] loaded maps: gain_map %s, gain_poly %s, gain-classified pixels %u, defect-map pixels %zu -> union_mask path %s\n",
                    snap.gain_map ? "yes" : "no", snap.gain_poly_coeffs ? "yes" : "no", static_cast<unsigned>(snap.gain_defect_count), masked,
                    snap.gain_defect_count > 0 ? "TAKEN" : "not taken");
    }
#endif

    void* ghost = nullptr;
    // XPE_A237C_BETA (QA-A-237c): a signal-dependence beta so large that the ghost stage cannot prove its frames stay inside the float
    // range (QA-A-237b/d): the frames then take the scratch route (three more planes for the call). Only tier 3 reads beta.
    const std::string beta = envStr("XPE_A237C_BETA");
    const std::string cfg = withStableLag(("{\"tier\":" + std::to_string(tier) + (beta.empty() ? "" : ",\"nlcscBeta\":" + beta) + "}").c_str());
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, cfg.c_str(), &ghost));
    checkpoint("C ghost handle created");
    std::printf("[a237b] config tier %d, frames %d, N %zu pixels\n", tier, frames, N);

#ifdef XPE_CACHE_TEST_HOOKS
    xpe_pipeline_after_stage_hook = &onStage;
    xpe_ghost_in_place_frames = 0;
#endif
    for (int f = 0; f < frames; ++f) {
#ifdef XPE_CACHE_TEST_HOOKS
        g_frame = f;
#endif
        uint16_t* raw = reinterpret_cast<uint16_t*>(frame.data());
        if (!realMaps.empty()) {
            // dark + bright, streamed in 64 KiB pieces: two whole frames read at once would raise the process's peak commit above the
            // pipeline's own and hide it (the peak is a watermark for the whole process).
            char name[24];
            std::snprintf(name, sizeof(name), "bright%02d.raw", 1 + (f % 6));
            std::FILE* fd = nullptr;
            std::FILE* fb = nullptr;
            const std::string darkPath = (std::filesystem::path(rawDir) / "dark.raw").string();
            const std::string brightPath = (std::filesystem::path(rawDir) / name).string();
            ASSERT_EQ(0, fopen_s(&fd, darkPath.c_str(), "rb"));
            ASSERT_EQ(0, fopen_s(&fb, brightPath.c_str(), "rb"));
            uint16_t pd[32768], pb[32768];
            for (size_t at = 0; at < N; at += 32768) {
                const size_t len = std::min<size_t>(32768, N - at);
                ASSERT_EQ(len, std::fread(pd, sizeof(uint16_t), len, fd));
                ASSERT_EQ(len, std::fread(pb, sizeof(uint16_t), len, fb));
                for (size_t i = 0; i < len; ++i) raw[at + i] = static_cast<uint16_t>(std::min<uint32_t>(65535u, static_cast<uint32_t>(pd[i]) + pb[i]));
            }
            std::fclose(fd);
            std::fclose(fb);
        } else if (!rawDir.empty()) {
            char name[24];
            std::snprintf(name, sizeof(name), "bright%02d.raw", 1 + (f % 6));
            const std::vector<uint16_t> real = readRaw(std::filesystem::path(rawDir) / name);
            ASSERT_FALSE(real.empty()) << "cannot read " << name;
            std::memcpy(raw, real.data(), N * sizeof(uint16_t));
        } else {
            const uint16_t base = static_cast<uint16_t>(2000 + 600 * (f % 2));
            for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(base + (i * 37u) % 300u);
            for (uint32_t k = 0; k < 16; ++k) raw[static_cast<size_t>(150 + 170 * k) * W + (100 + 180 * k)] = 60000;
        }
        img.format = XPE_PIXEL_UINT16;
        img.bitsAllocated = img.bitsStored = 16;

        char label[48];
        std::snprintf(label, sizeof(label), "frame %d before", f);
        checkpoint(label);
        const auto t0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, ghost, nullptr);
        const auto t1 = std::chrono::steady_clock::now();
        ASSERT_EQ(XPE_OK, rc) << "frame " << f;
        std::snprintf(label, sizeof(label), "frame %d after", f);
        checkpoint(label);
        std::printf("[a237b] frame %d total %.1f ms, output digest %016llx\n", f,
                    std::chrono::duration<double, std::milli>(t1 - t0).count(),
                    static_cast<unsigned long long>(digest(frame.data(), N * sizeof(float))));
    }
#ifdef XPE_CACHE_TEST_HOOKS
    xpe_pipeline_after_stage_hook = nullptr;
    std::printf("[a237b] ghost frames processed in place: %lu of %d\n", xpe_ghost_in_place_frames, frames);
#endif
    {
        // What the frames pushed: printed, then drained (the suite's environment reports alerts left behind as a failure).
        char msg[400];
        int32_t sev = -1;
        const int32_t count = xpe_get_pending_alert_count();
        std::printf("[a237b] alerts pending after %d frame(s): %d\n", frames, static_cast<int>(count));
        for (int32_t i = 0; i < count; ++i)
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK)
                std::printf("[a237b]   alert %d (severity %d): %.160s\n", static_cast<int>(i), static_cast<int>(sev), msg);
        xpe_clear_alerts();
    }
    xpe_ghost_destroy(ghost);
    checkpoint("Z ghost destroyed");
    xpe_preprocess_shutdown();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// QA-A-237g: writes offset.xcal and gain.xcal into $XPE_A237G_OUT from the CalData_6 files in $XPE_A237B_RAW_DIR with the product's own
// generators (dark.raw: one dark frame; bright01..06.raw: the six flat frames, no dark reference because they are already offset
// corrected). A process of its own, so that the generators' transient memory is not in the peak of a measurement run.
TEST(A237gMaps, DISABLED_GenerateRealMaps) {
    const std::string outDir = envStr("XPE_A237G_OUT");
    const std::string rawDir = envStr("XPE_A237B_RAW_DIR");
    ASSERT_FALSE(outDir.empty());
    ASSERT_FALSE(rawDir.empty());
    std::filesystem::create_directories(outDir);
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const auto frameOf = [](std::vector<uint16_t>& v) {
        XpeImageBuffer b{};
        b.data = v.data();
        b.width = W;
        b.height = H;
        b.bitsAllocated = b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
        return b;
    };
    std::vector<uint16_t> dark = readRaw(std::filesystem::path(rawDir) / "dark.raw");
    ASSERT_FALSE(dark.empty());
    XpeImageBuffer d = frameOf(dark);
    const std::string offPath = (std::filesystem::path(outDir) / "offset.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&d, 1, 100.0f, 25.0f, offPath.c_str(), nullptr));
    std::vector<std::vector<uint16_t>> flats;
    std::vector<XpeImageBuffer> buffers;
    for (int k = 1; k <= 6; ++k) {
        char name[24];
        std::snprintf(name, sizeof(name), "bright%02d.raw", k);
        flats.push_back(readRaw(std::filesystem::path(rawDir) / name));
        ASSERT_FALSE(flats.back().empty()) << name;
    }
    for (auto& v : flats) buffers.push_back(frameOf(v));
    const std::string gainPath = (std::filesystem::path(outDir) / "gain.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(buffers.data(), 6, nullptr, gainPath.c_str(), nullptr));
    std::printf("[a237g] wrote %s and %s\n", offPath.c_str(), gainPath.c_str());
    // XPE_A237G_POLY_OUT: a polynomial gain (degree 2) fitted over the six flat levels, one gain map made of each level on its own
    // (the doses are not in the dataset, the README says so: they are 1000..6000 here, which fixes the fit, not the memory it takes).
    const std::string polyDir = envStr("XPE_A237G_POLY_OUT");
    if (!polyDir.empty()) {
        std::filesystem::create_directories(polyDir);
        std::vector<std::string> levelPaths;
        std::vector<double> doses;
        for (int k = 0; k < 6; ++k) {
            const std::string lp = (std::filesystem::path(polyDir) / ("level" + std::to_string(k + 1) + ".xcal")).string();
            ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(&buffers[static_cast<size_t>(k)], 1, nullptr, lp.c_str(), nullptr)) << lp;
            levelPaths.push_back(lp);
            doses.push_back(1000.0 * (k + 1));
        }
        std::vector<const char*> pointers;
        for (const std::string& lp : levelPaths) pointers.push_back(lp.c_str());
        const std::string polyPath = (std::filesystem::path(polyDir) / "gain.xcal").string();
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(pointers.data(), doses.data(), 6, 2, polyPath.c_str()));
        std::filesystem::copy_file(offPath, std::filesystem::path(polyDir) / "offset.xcal", std::filesystem::copy_options::overwrite_existing);
        std::printf("[a237g] wrote %s\n", polyPath.c_str());
    }
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

#ifdef XPE_CACHE_TEST_HOOKS
// QA-A-237c M1: which frames take the ghost stage's scratch route. For each configuration and each decade of the frame's magnitude, a
// handle is fed 150 identical frames (long enough for the history bound to settle: 0.951^150 is below 1e-3) and the LAST frame is
// classified with the real decision (the in-place counter): "in place", "scratch" (the route with three extra planes), or "fails".
// Constant frames of one value are the least favourable ordinary case for the bound (it is a maximum); the table says where each
// configuration leaves the in-place route, not what a particular exposure does.
TEST(A237cRoute, DISABLED_WhichMagnitudesTakeTheScratchRoute) {
    struct Cfg { const char* name; const char* beta; };
    const Cfg cfgs[] = {{"default beta (0.1)", ""}, {"beta 1e3", "1e3"}, {"beta 1e10", "1e10"}, {"beta 1e20", "1e20"}, {"beta 1e30", "1e30"}, {"beta 1e35", "1e35"}, {"beta 1e36", "1e36"}};
    std::printf("[a237c] route of the last of 150 constant frames, 8x8, lag alpha1 0.1 tau1 1 alpha2 0.01 tau2 20 (S = 0.363)\n");
    for (int tier = 1; tier <= 3; ++tier) {
        for (const Cfg& c : cfgs) {
            if (tier != 3 && c.beta[0] != '\0') continue;   // beta is read by tier 3 only
            std::string line;
            for (int decade = 3; decade <= 38; ++decade) {
                const float value = std::pow(10.0f, static_cast<float>(decade));
                if (!std::isfinite(value)) break;
                void* ghost = nullptr;
                const std::string cfg = withStableLag(("{\"tier\":" + std::to_string(tier) + (c.beta[0] ? ",\"nlcscBeta\":" + std::string(c.beta) : std::string()) + "}").c_str());
                if (xpe_ghost_create(8, 8, cfg.c_str(), &ghost) != XPE_OK) { line += "?"; continue; }
                std::vector<float> px(64);
                XpeImageBuffer img{};
                img.width = 8;
                img.height = 8;
                img.format = XPE_PIXEL_FLOAT32;
                img.bitsAllocated = img.bitsStored = 32;
                img.data = px.data();
                img.dataSize = px.size() * sizeof(float);
                XpeImageMetadata meta{};
                char mark = '.';
                for (int f = 0; f < 150; ++f) {
                    std::fill(px.begin(), px.end(), value);
                    const unsigned long before = xpe_ghost_in_place_frames;
                    const XpeErrorCode rc = xpe_ghost_correct(ghost, &img, &meta);
                    mark = (rc != XPE_OK) ? 'F' : (xpe_ghost_in_place_frames != before ? '.' : 'S');
                    if (rc != XPE_OK) break;   // a failed frame leaves the history as it was; the next would fail again
                }
                line += mark;
                xpe_ghost_destroy(ghost);
                xpe_clear_alerts();
            }
            std::printf("[a237c] tier %d %-20s decades 1e3..1e38: %s\n", tier, c.name, line.c_str());
        }
    }
    std::printf("[a237c] legend: . in place, S scratch route (succeeds), F the frame fails (the call returns PROCESSING_FAILED)\n");
}
#endif  // XPE_CACHE_TEST_HOOKS
