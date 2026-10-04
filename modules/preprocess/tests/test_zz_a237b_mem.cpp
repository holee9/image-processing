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
 * Environment: XPE_A237B_TIER (ghost tier 1..3, default 1), XPE_A237B_FRAMES (default 3).
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
#include <chrono>
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
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(offPath.c_str(), W, H, 200.0f));
    ASSERT_EQ(XPE_OK, MakeGainXCal(gainPath.c_str(), W, H, 1.25f));
    {
        std::vector<uint8_t> payload(N, 0);
        for (uint32_t k = 0; k < 16; ++k) payload[static_cast<size_t>(150 + 170 * k) * W + (100 + 180 * k)] = 1;
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

    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offPath.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defPath.c_str()));
    checkpoint("B maps loaded");

    void* ghost = nullptr;
    const std::string cfg = withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str());
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, cfg.c_str(), &ghost));
    checkpoint("C ghost handle created");
    std::printf("[a237b] config tier %d, frames %d, N %zu pixels\n", tier, frames, N);

#ifdef XPE_CACHE_TEST_HOOKS
    xpe_pipeline_after_stage_hook = &onStage;
#endif
    for (int f = 0; f < frames; ++f) {
#ifdef XPE_CACHE_TEST_HOOKS
        g_frame = f;
#endif
        uint16_t* raw = reinterpret_cast<uint16_t*>(frame.data());
        const uint16_t base = static_cast<uint16_t>(2000 + 600 * (f % 2));
        for (size_t i = 0; i < N; ++i) raw[i] = static_cast<uint16_t>(base + (i * 37u) % 300u);
        for (uint32_t k = 0; k < 16; ++k) raw[static_cast<size_t>(150 + 170 * k) * W + (100 + 180 * k)] = 60000;
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
