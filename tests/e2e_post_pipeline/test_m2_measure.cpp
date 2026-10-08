/**
 * @file test_m2_measure.cpp
 * @brief QA-B-213: stage-2 measurement harness (checklist P1..P7) on the stage-1 reference image.
 *
 * This is a MEASUREMENT harness, not a pass/fail test of the product: every test is DISABLED_ and is run by name with
 * --gtest_also_run_disabled_tests. It reads a real frame (XPE_M2_IN: float32 little endian, 3072 x 3072, the output of
 * the shipping preprocess path) and writes its observations to stdout and to files in XPE_M2_OUT. It changes no product code.
 *
 * Chain (milestone-2-post-basic.md P1): log -> noise (bilateral) -> CLAHE -> USM -> Modality -> VOI -> Presentation (uint16).
 * Every function takes the default parameters its header documents. Where the SPEC gives no default, the choice is named
 * in kLogNorm / the VOI window and repeated in the report.
 */

#ifndef NOMINMAX
#  define NOMINMAX  // windows.h min/max macros would break std::min/std::max
#endif

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/display/display_api.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  pragma comment(lib, "Psapi.lib")
#endif

namespace {

constexpr uint32_t kDim = 3072;
constexpr size_t kPixels = static_cast<size_t>(kDim) * kDim;

// The SPEC (REQ-ENH-001) fixes the formula but names no default for normFactor; the existing E2E test uses 1000.
constexpr float kLogNorm = 1000.0f;

std::string env_or_empty(const char* name) {
#ifdef _MSC_VER
    char* v = nullptr;
    size_t len = 0;
    if (_dupenv_s(&v, &len, name) != 0 || v == nullptr) return std::string();
    std::string out(v);
    std::free(v);
    return out;
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

FILE* open_file(const std::string& path, const char* mode) {
#ifdef _MSC_VER
    FILE* f = nullptr;
    return fopen_s(&f, path.c_str(), mode) == 0 ? f : nullptr;
#else
    return std::fopen(path.c_str(), mode);
#endif
}

std::vector<float> load_input(const std::string& path) {
    std::vector<float> px(kPixels);
    FILE* f = open_file(path, "rb");
    if (!f) return {};
    const size_t got = std::fread(px.data(), sizeof(float), kPixels, f);
    std::fclose(f);
    if (got != kPixels) return {};
    return px;
}

bool write_file(const std::string& path, const void* data, size_t bytes) {
    FILE* f = open_file(path, "wb");
    if (!f) return false;
    const size_t put = std::fwrite(data, 1, bytes, f);
    std::fclose(f);
    return put == bytes;
}

struct Stats {
    double minv = 0, maxv = 0, mean = 0, p01 = 0, p99 = 0;
    size_t nonFinite = 0;
};

Stats compute_stats(const float* px, size_t n) {
    Stats s;
    double lo = 1e300, hi = -1e300, sum = 0;
    size_t bad = 0;
    for (size_t i = 0; i < n; ++i) {
        const float v = px[i];
        if (!std::isfinite(v)) { ++bad; continue; }
        lo = std::min(lo, static_cast<double>(v));
        hi = std::max(hi, static_cast<double>(v));
        sum += static_cast<double>(v);
    }
    s.minv = lo;
    s.maxv = hi;
    s.nonFinite = bad;
    s.mean = (n > bad) ? sum / static_cast<double>(n - bad) : 0.0;
    std::vector<float> c(px, px + n);
    const size_t i1 = static_cast<size_t>(0.01 * static_cast<double>(n));
    const size_t i99 = static_cast<size_t>(0.99 * static_cast<double>(n));
    std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(i1), c.end());
    s.p01 = static_cast<double>(c[i1]);
    std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(i99), c.end());
    s.p99 = static_cast<double>(c[i99]);
    return s;
}

void print_stats(const char* label, const Stats& s) {
    std::printf("  %-22s min=%.4f max=%.4f mean=%.4f p01=%.4f p99=%.4f nonFinite=%zu\n", label, s.minv, s.maxv, s.mean,
                s.p01, s.p99, s.nonFinite);
}

XpeImageBuffer alloc_f32() {
    XpeImageBuffer img{};
    EXPECT_EQ(XPE_OK, xpe_alloc_image(kDim, kDim, XPE_PIXEL_FLOAT32, &img));
    return img;
}

float* f32(XpeImageBuffer& img) { return static_cast<float*>(img.data); }

double ms_since(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// Parameters of the chain. VOI is filled in after the window is chosen.
struct ChainParams {
    XpeNoiseReduceParams noise{XPE_NOISE_BILATERAL, 3.0f, 50.0f, 21, 7, 10.0f};
    XpeModalityLutParams modality{XPE_MODALITY_LUT_LINEAR, 1.0f, 0.0f, nullptr, 0u, 0, 0u};
    XpeVoiLutParams voi{XPE_VOI_LINEAR, 0.0f, 1.0f, 0.0f, 1.0f};
    XpePresentationLutParams pres{};
};

ChainParams make_params() {
    ChainParams p;
    for (int i = 0; i < 1024; ++i) {
        p.pres.lutData[i] = static_cast<uint16_t>(std::lround(static_cast<double>(i) * 65535.0 / 1023.0));
    }
    p.pres.gsdfEnabled = 0;
    return p;
}

// stage index: 0 log, 1 noise, 2 clahe, 3 usm, 4 modality, 5 voi, 6 presentation
const char* const kStageName[7] = {"log", "noise_bilateral", "clahe", "usm", "modality", "voi", "presentation"};

XpeErrorCode run_stage(int stage, XpeImageBuffer& img, const ChainParams& p) {
    switch (stage) {
        case 0: return xpe_log_transform(&img, kLogNorm);
        case 1: return xpe_noise_reduce(&img, &p.noise);
        case 2: return xpe_contrast_enhance(&img, nullptr);
        case 3: return xpe_edge_enhance(&img, nullptr);
        case 4: return xpe_apply_modality_lut(&img, &p.modality);
        case 5: return xpe_apply_voi_lut(&img, &p.voi);
        default: return xpe_apply_presentation_lut(&img, &p.pres);
    }
}

// Linear window p01..p99 -> 0..65535 for LOOKING at a float stage image (display only).
std::vector<uint16_t> display_window(const float* px, size_t n, double lo, double hi) {
    std::vector<uint16_t> out(n);
    const double span = (hi > lo) ? hi - lo : 1.0;
    for (size_t i = 0; i < n; ++i) {
        double t = (static_cast<double>(px[i]) - lo) / span;
        t = std::min(1.0, std::max(0.0, t));
        out[i] = static_cast<uint16_t>(std::lround(t * 65535.0));
    }
    return out;
}

void drain_alerts(const char* label) {
    const int32_t n = xpe_get_pending_alert_count();
    std::printf("  alerts after %s: %d\n", label, n);
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) {
            std::printf("    [%d] severity=%d \"%s\"\n", i, sev, msg);
        }
    }
    xpe_clear_alerts();
}

void report_ei(const char* label, const XpeImageBuffer& img, const char* bodyPart) {
    XpeImageMetadata meta{};
#ifdef _MSC_VER
    strncpy_s(meta.bodyPart, sizeof(meta.bodyPart), bodyPart, _TRUNCATE);
#else
    std::strncpy(meta.bodyPart, bodyPart, sizeof(meta.bodyPart) - 1);
#endif
    xpe_clear_alerts();
    float ei = -1.0f, di = -1.0f;
    const XpeErrorCode rc = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    std::printf("  EI/DI %-30s bodyPart=\"%s\" rc=%d (%s) EI=%.6f DI=%.6f\n", label, bodyPart, static_cast<int>(rc),
                xpe_error_string(rc), static_cast<double>(ei), static_cast<double>(di));
    drain_alerts(label);
}

#ifdef _WIN32
struct MemSample {
    double privateMiB = 0, workingMiB = 0, peakMiB = 0;
};
MemSample sample_memory() {
    PROCESS_MEMORY_COUNTERS_EX c{};
    c.cb = sizeof(c);
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&c), sizeof(c));
    constexpr double kMiB = 1024.0 * 1024.0;
    MemSample m;
    m.privateMiB = static_cast<double>(c.PrivateUsage) / kMiB;
    m.workingMiB = static_cast<double>(c.WorkingSetSize) / kMiB;
    m.peakMiB = static_cast<double>(c.PeakWorkingSetSize) / kMiB;
    return m;
}
#else
struct MemSample {
    double privateMiB = 0, workingMiB = 0, peakMiB = 0;
};
MemSample sample_memory() { return {}; }
#endif

double median_of(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.empty() ? 0.0 : v[v.size() / 2];
}

// Least-squares slope (MiB per frame) of v[first..last-1] against the frame index.
double slope_of(const std::vector<double>& v, size_t first, size_t last) {
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const double n = static_cast<double>(last - first);
    for (size_t i = first; i < last; ++i) {
        const double x = static_cast<double>(i);
        sx += x;
        sy += v[i];
        sxx += x * x;
        sxy += x * v[i];
    }
    const double den = n * sxx - sx * sx;
    return den != 0.0 ? (n * sxy - sx * sy) / den : 0.0;
}

// The detector shared by the real chain and its positive control. 1-based frames 11..20 = baseline, 91..100 = end.
void leak_verdict(const char* label, const std::vector<double>& priv) {
    const size_t n = priv.size();
    const double base = median_of(std::vector<double>(priv.begin() + 10, priv.begin() + 20));
    const double end = median_of(std::vector<double>(priv.begin() + static_cast<std::ptrdiff_t>(n - 10), priv.end()));
    const double slope = slope_of(priv, 20, n);
    std::printf("  [%s] private MiB: first=%.2f frames11-20 median=%.2f frames91-100 median=%.2f  end-base=%.3f MiB  "
                "slope(frames21-100)=%.5f MiB/frame\n",
                label, priv[0], base, end, end - base, slope);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// P1 + P3 + P4 + P5 + P7: one pass of the chain over the real frame, with stage-by-stage observations and display files.
// ---------------------------------------------------------------------------------------------------------------------
TEST(M2Measure, DISABLED_Chain_P1_P3_P4_P5_P7) {
    const std::string inPath = env_or_empty("XPE_M2_IN");
    const std::string outDir = env_or_empty("XPE_M2_OUT");
    ASSERT_FALSE(inPath.empty());
    ASSERT_FALSE(outDir.empty());
    std::vector<float> input = load_input(inPath);
    ASSERT_EQ(kPixels, input.size()) << "input file missing or wrong size";

    std::printf("== QA-B-213 chain ==\n  module versions: enhance_basic=%s display=%s\n  hardware threads=%u enhance_basic max_threads(request)=%d\n",
                xpe_enhance_basic_version(), xpe_display_version(), std::thread::hardware_concurrency(),
                static_cast<int>(xpe_enhance_basic_get_max_threads()));
    const Stats sIn = compute_stats(input.data(), kPixels);
    print_stats("input (ADU)", sIn);

    // ---- P3: EI / DI on the detector-domain image (the corrected ADU, before any chain stage)
    std::printf("== P3 EI/DI ==\n");
    XpeImageBuffer ein = alloc_f32();
    std::memcpy(ein.data, input.data(), kPixels * sizeof(float));
    report_ei("real frame", ein, "HAND");
    report_ei("real frame", ein, "");
    report_ei("real frame", ein, "CHEST");
    for (const float scale : {0.25f, 4.0f, 0.5f, 2.0f}) {
        float* d = f32(ein);
        for (size_t i = 0; i < kPixels; ++i) d[i] = input[i] * scale;
        char label[64];
        std::snprintf(label, sizeof(label), "real frame x %.2f", static_cast<double>(scale));
        report_ei(label, ein, "HAND");
    }
    xpe_free_image(&ein);

    // ---- P1: the chain, stage by stage
    std::printf("== P1 chain ==\n");
    ChainParams p = make_params();
    XpeImageBuffer img = alloc_f32();
    std::memcpy(img.data, input.data(), kPixels * sizeof(float));
    std::vector<float> preUsm, postUsm, postVoi;
    std::vector<uint16_t> win;
    {
        const auto w = display_window(input.data(), kPixels, sIn.p01, sIn.p99);
        ASSERT_TRUE(write_file(outDir + "/00_input_adu.u16", w.data(), w.size() * sizeof(uint16_t)));
    }
    double firstRunMs[7] = {};
    for (int s = 0; s < 6; ++s) {
        if (s == 3) preUsm.assign(f32(img), f32(img) + kPixels);
        if (s == 5) {
            // VOI window: linear, 1st..99th percentile of the image that reaches the VOI stage (after Modality).
            const Stats sv = compute_stats(f32(img), kPixels);
            p.voi.center = static_cast<float>((sv.p01 + sv.p99) * 0.5);
            p.voi.width = static_cast<float>(sv.p99 - sv.p01);
            std::printf("  VOI window (LINEAR, p01..p99 of the Modality output): p01=%.4f p99=%.4f -> center=%.4f width=%.4f out=[0,1]\n",
                        sv.p01, sv.p99, static_cast<double>(p.voi.center), static_cast<double>(p.voi.width));
        }
        const auto t0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = run_stage(s, img, p);
        firstRunMs[s] = ms_since(t0);
        ASSERT_EQ(XPE_OK, rc) << kStageName[s];
        if (s == 3) postUsm.assign(f32(img), f32(img) + kPixels);
        const Stats st = compute_stats(f32(img), kPixels);
        char label[64];
        std::snprintf(label, sizeof(label), "after %s", kStageName[s]);
        print_stats(label, st);
        const auto w = display_window(f32(img), kPixels, st.p01, st.p99);
        char name[96];
        std::snprintf(name, sizeof(name), "/%02d_%s_p01p99.u16", s + 1, kStageName[s]);
        ASSERT_TRUE(write_file(outDir + name, w.data(), w.size() * sizeof(uint16_t)));
        if (s == 5) postVoi.assign(f32(img), f32(img) + kPixels);
    }

    // ---- P5: VOI output range (before the Presentation stage)
    std::printf("== P5 VOI / Presentation ==\n");
    {
        size_t below = 0, above = 0;
        for (size_t i = 0; i < kPixels; ++i) {
            if (postVoi[i] < p.voi.minOut) ++below;
            if (postVoi[i] > p.voi.maxOut) ++above;
        }
        const Stats sv = compute_stats(postVoi.data(), kPixels);
        std::printf("  VOI output range [%.1f,%.1f]: min=%.6f max=%.6f below=%zu above=%zu nonFinite=%zu\n",
                    static_cast<double>(p.voi.minOut), static_cast<double>(p.voi.maxOut), sv.minv, sv.maxv, below,
                    above, sv.nonFinite);
    }
    {
        const auto t0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = run_stage(6, img, p);
        firstRunMs[6] = ms_since(t0);
        ASSERT_EQ(XPE_OK, rc);
        ASSERT_EQ(XPE_PIXEL_UINT16, img.format);
        const uint16_t* u = static_cast<const uint16_t*>(img.data);
        uint16_t lo = 65535, hi = 0;
        size_t atZero = 0, atFull = 0;
        for (size_t i = 0; i < kPixels; ++i) {
            lo = std::min(lo, u[i]);
            hi = std::max(hi, u[i]);
            if (u[i] == 0) ++atZero;
            if (u[i] == 65535) ++atFull;
        }
        std::printf("  Presentation output uint16: min=%u max=%u pixels at 0=%zu pixels at 65535=%zu bitsAllocated=%u dataSize=%zu\n",
                    static_cast<unsigned>(lo), static_cast<unsigned>(hi), atZero, atFull,
                    static_cast<unsigned>(img.bitsAllocated), img.dataSize);
        ASSERT_TRUE(write_file(outDir + "/07_presentation_uint16.u16", u, kPixels * sizeof(uint16_t)));
    }
    xpe_free_image(&img);
    std::printf("  first-run stage times (ms): ");
    for (int s = 0; s < 7; ++s) std::printf("%s=%.2f ", kStageName[s], firstRunMs[s]);
    std::printf("\n");

    // P5 edge arms on the real post-VOI image with injected pixels (declared as injected).
    {
        XpeImageBuffer e = alloc_f32();
        std::memcpy(e.data, postVoi.data(), kPixels * sizeof(float));
        f32(e)[0] = std::nanf("");
        std::vector<uint8_t> before(static_cast<uint8_t*>(e.data), static_cast<uint8_t*>(e.data) + e.dataSize);
        const XpeErrorCode rc = xpe_apply_presentation_lut(&e, &p.pres);
        const bool same = std::memcmp(before.data(), e.data, e.dataSize) == 0;
        std::printf("  arm NaN pixel injected at [0]: rc=%d (%s) buffer unchanged=%d format still float32=%d\n",
                    static_cast<int>(rc), xpe_error_string(rc), same ? 1 : 0, e.format == XPE_PIXEL_FLOAT32 ? 1 : 0);
        f32(e)[0] = -5.0f;
        f32(e)[1] = 7.0f;
        f32(e)[2] = std::numeric_limits<float>::infinity();
        const XpeErrorCode rcInf = xpe_apply_presentation_lut(&e, &p.pres);
        std::printf("  arm +inf pixel injected at [2]: rc=%d (%s)\n", static_cast<int>(rcInf), xpe_error_string(rcInf));
        f32(e)[2] = 0.5f;
        const XpeErrorCode rc2 = xpe_apply_presentation_lut(&e, &p.pres);
        if (rc2 == XPE_OK) {
            const uint16_t* u = static_cast<const uint16_t*>(e.data);
            std::printf("  arm finite out-of-range injected (-5.0, 7.0): rc=%d out[0]=%u (lut[0]=%u) out[1]=%u (lut[1023]=%u)\n",
                        static_cast<int>(rc2), static_cast<unsigned>(u[0]), static_cast<unsigned>(p.pres.lutData[0]),
                        static_cast<unsigned>(u[1]), static_cast<unsigned>(p.pres.lutData[1023]));
        } else {
            std::printf("  arm finite out-of-range injected: rc=%d (%s)\n", static_cast<int>(rc2), xpe_error_string(rc2));
        }
        xpe_free_image(&e);
    }

    // ---- P4: USM overshoot bound (REQ-ENH-021) on the real pre-USM image
    std::printf("== P4 USM ==\n");
    auto usm_bound_report = [&](const char* label, float amount) {
        std::vector<float> work = preUsm;
        XpeImageBuffer u = alloc_f32();
        std::memcpy(u.data, work.data(), kPixels * sizeof(float));
        XpeUsmParams up{amount, 2.0f, 10.0f};
        const XpeErrorCode rc = xpe_edge_enhance(&u, &up);
        const float* o = f32(u);
        size_t violate = 0, negative = 0, changed = 0, atBound = 0;
        double maxDiff = 0, maxRatio = 0;
        size_t maxAt = 0;
        for (size_t i = 0; i < kPixels; ++i) {
            const float in = preUsm[i];
            const float out = o[i];
            const float bound = std::max(in * 2.0f, in + amount * 10.0f);
            if (out > bound * (1.0f + 1e-6f) + 1e-3f) ++violate;
            if (out < 0.0f) ++negative;
            if (out != in) ++changed;
            if (out > 0 && std::fabs(out - bound) <= 1e-4f * std::fabs(bound) + 1e-3f) ++atBound;
            const double d = std::fabs(static_cast<double>(out) - static_cast<double>(in));
            if (d > maxDiff) { maxDiff = d; maxAt = i; }
            if (in > 0) maxRatio = std::max(maxRatio, static_cast<double>(out) / static_cast<double>(in));
        }
        std::printf("  [%s] amount=%.1f radius=2.0 threshold=10.0 rc=%d: changed=%zu (%.2f%%) bound-violations=%zu "
                    "negative-outputs=%zu pixels-at-the-clamp=%zu max|out-in|=%.4f at index %zu max(out/in)=%.6f\n",
                    label, static_cast<double>(amount), static_cast<int>(rc), changed,
                    100.0 * static_cast<double>(changed) / static_cast<double>(kPixels), violate, negative, atBound,
                    maxDiff, maxAt, maxRatio);
        if (amount == 0.5f) {
            // Observation: the row profile around the pixel USM changed most (in -> out, 21 samples each side).
            const size_t row = maxAt / kDim, col = maxAt % kDim;
            const size_t c0 = (col >= 20) ? col - 20 : 0;
            const size_t c1 = std::min<size_t>(col + 20, kDim - 1);
            std::printf("  largest-change profile row %zu cols %zu..%zu (in/out):", row, c0, c1);
            for (size_t c = c0; c <= c1; ++c) {
                std::printf(" %.1f/%.1f", static_cast<double>(preUsm[row * kDim + c]),
                            static_cast<double>(o[row * kDim + c]));
            }
            std::printf("\n");
        }
        xpe_free_image(&u);
    };
    usm_bound_report("default amount", 0.5f);
    usm_bound_report("stress amount (max of range)", 5.0f);
    {
        double maxDiffDefault = 0;
        for (size_t i = 0; i < kPixels; ++i) {
            maxDiffDefault = std::max(maxDiffDefault, std::fabs(static_cast<double>(postUsm[i]) - static_cast<double>(preUsm[i])));
        }
        std::printf("  chain-pass default USM max|out-in|=%.4f (cross-check of the stage-by-stage pass)\n", maxDiffDefault);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// P2: per-stage time, repeated in one process on the real frame. Each repetition starts from the stage's own input.
// ---------------------------------------------------------------------------------------------------------------------
TEST(M2Measure, DISABLED_StageTimes_P2) {
    const std::string inPath = env_or_empty("XPE_M2_IN");
    ASSERT_FALSE(inPath.empty());
    std::vector<float> input = load_input(inPath);
    ASSERT_EQ(kPixels, input.size());
    constexpr int kReps = 21;

    ChainParams p = make_params();
    std::vector<std::vector<float>> stageIn(7);
    XpeImageBuffer img = alloc_f32();
    std::memcpy(img.data, input.data(), kPixels * sizeof(float));
    for (int s = 0; s < 6; ++s) {
        if (s == 5) {
            const Stats sv = compute_stats(f32(img), kPixels);
            p.voi.center = static_cast<float>((sv.p01 + sv.p99) * 0.5);
            p.voi.width = static_cast<float>(sv.p99 - sv.p01);
        }
        stageIn[static_cast<size_t>(s)].assign(f32(img), f32(img) + kPixels);
        ASSERT_EQ(XPE_OK, run_stage(s, img, p));
    }
    stageIn[6].assign(f32(img), f32(img) + kPixels);  // VOI output feeds Presentation
    xpe_free_image(&img);

    std::printf("== P2 stage times (ms), %d repetitions per stage, 3072x3072, same process ==\n  hardware threads=%u max_threads(request)=%d\n",
                kReps, std::thread::hardware_concurrency(), static_cast<int>(xpe_enhance_basic_get_max_threads()));
    const double limit[7] = {15, 100, 50, 20, 20, 16, 25};
    for (int s = 0; s < 7; ++s) {
        std::vector<double> t;
        for (int r = 0; r < kReps; ++r) {
            XpeImageBuffer w = alloc_f32();
            std::memcpy(w.data, stageIn[static_cast<size_t>(s)].data(), kPixels * sizeof(float));
            const auto t0 = std::chrono::steady_clock::now();
            const XpeErrorCode rc = run_stage(s, w, p);
            const double ms = ms_since(t0);
            EXPECT_EQ(XPE_OK, rc);
            t.push_back(ms);
            xpe_free_image(&w);
        }
        const double first = t[0];
        std::vector<double> rest(t.begin() + 1, t.end());
        std::sort(rest.begin(), rest.end());
        std::printf("  %-16s limit=%5.0f first=%8.2f min=%8.2f median=%8.2f max=%8.2f (reps 2..%d)\n", kStageName[s],
                    limit[s], first, rest.front(), rest[rest.size() / 2], rest.back(), kReps);
        size_t over = 0;
        for (const double v : t) over += (v > limit[s]) ? 1u : 0u;
        std::printf("      repetitions over the limit: %zu of %zu; all (ms, run order):", over, t.size());
        for (const double v : t) std::printf(" %.2f", v);
        std::printf("\n");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// P6: memory across 100 frames of the whole chain, with a positive control (a real 1 MiB/frame leak) for the detector.
// ---------------------------------------------------------------------------------------------------------------------
TEST(M2Measure, DISABLED_Leak100_P6) {
    const std::string inPath = env_or_empty("XPE_M2_IN");
    const std::string outDir = env_or_empty("XPE_M2_OUT");
    ASSERT_FALSE(inPath.empty());
    std::vector<float> input = load_input(inPath);
    ASSERT_EQ(kPixels, input.size());
    constexpr int kFrames = 100;

    ChainParams p = make_params();
    {
        // The VOI window is fixed from the first frame so the memory series measures the chain, not the percentile sort.
        XpeImageBuffer w = alloc_f32();
        std::memcpy(w.data, input.data(), kPixels * sizeof(float));
        for (int s = 0; s < 5; ++s) ASSERT_EQ(XPE_OK, run_stage(s, w, p));
        const Stats sv = compute_stats(f32(w), kPixels);
        p.voi.center = static_cast<float>((sv.p01 + sv.p99) * 0.5);
        p.voi.width = static_cast<float>(sv.p99 - sv.p01);
        xpe_free_image(&w);
    }

    std::printf("== P6 leak: %d frames of the full chain ==\n", kFrames);
    std::vector<double> priv, work, peak;
    std::string csv = "frame,private_MiB,working_set_MiB,peak_working_set_MiB\n";
    for (int f = 0; f < kFrames; ++f) {
        XpeImageBuffer w = alloc_f32();
        std::memcpy(w.data, input.data(), kPixels * sizeof(float));
        for (int s = 0; s < 7; ++s) ASSERT_EQ(XPE_OK, run_stage(s, w, p)) << "frame " << f << " stage " << s;
        xpe_free_image(&w);
        const MemSample m = sample_memory();
        priv.push_back(m.privateMiB);
        work.push_back(m.workingMiB);
        peak.push_back(m.peakMiB);
        char line[128];
        std::snprintf(line, sizeof(line), "%d,%.3f,%.3f,%.3f\n", f + 1, m.privateMiB, m.workingMiB, m.peakMiB);
        csv += line;
    }
    ASSERT_TRUE(write_file(outDir + "/p6_memory_100_frames.csv", csv.data(), csv.size()));
    leak_verdict("real chain", priv);
    std::printf("  peak working set over the run: %.1f MiB (observation; the SPEC states no post limit)\n", peak.back());

    // Positive control: the same detector on a series that really leaks 1 MiB per frame (touched pages, kept alive).
    std::vector<std::vector<char>> kept;
    std::vector<double> ctl;
    for (int f = 0; f < kFrames; ++f) {
        XpeImageBuffer w = alloc_f32();
        std::memcpy(w.data, input.data(), kPixels * sizeof(float));
        ASSERT_EQ(XPE_OK, run_stage(4, w, p));
        xpe_free_image(&w);
        kept.emplace_back(1024 * 1024, static_cast<char>(f + 1));
        ctl.push_back(sample_memory().privateMiB);
    }
    leak_verdict("control: 1 MiB/frame injected", ctl);
}
