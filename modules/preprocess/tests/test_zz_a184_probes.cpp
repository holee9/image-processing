/**
 * @file test_zz_a184_probes.cpp
 * @brief QA-A-184 (#232) -- decision material, two probes. Observation only.
 *
 * All DISABLED_ on purpose. Run with:
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A184Probes.*
 *
 * (2) REQ-P1A-041 line noise: what does xpe_validate_readout_artifact report on
 *     frames that DO contain row- or column-correlated noise, and do any frames
 *     in the repository contain such noise? A "line-noise index" is computed from
 *     the frame itself, so the claim "this frame has line noise" is measured.
 *
 * (3) NFR-003 shared ghost handle: two threads call xpe_ghost_correct on ONE
 *     handle. With identical frames and a constant acquisition time every atomic
 *     interleaving of the calls ends in the same history state as the serial
 *     replay, so a different final state is lost-update evidence, not noise.
 *     Two controls guard the method: one handle per thread, and one shared handle
 *     serialised by an external mutex. Both must match their serial replay.
 *
 * Nothing here is part of the product or of any requirement.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// (2) line noise
// ---------------------------------------------------------------------------

XpeImageBuffer u16Buffer(std::vector<uint16_t>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = px.data(); b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = static_cast<uint32_t>(px.size() * sizeof(uint16_t));
    return b;
}

double medianOf(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2u];
}

/** Robust spread (MAD * 1.4826) of v. */
double robustSpread(const std::vector<double>& v) {
    const double m = medianOf(v);
    std::vector<double> d(v.size());
    for (size_t i = 0; i < v.size(); ++i) d[i] = std::fabs(v[i] - m);
    return 1.4826 * medianOf(d);
}

/**
 * Line-noise index. Pixel noise sigma comes from neighbour differences along the
 * line (robust, so edges barely count). The first difference of the line means
 * has spread sigma*sqrt(2/len) when only pixel noise is present; the index is
 * observed / expected. ~1 = no line-correlated noise; >> 1 = line noise (or a
 * strong gradient perpendicular to the lines).
 */
double lineNoiseIndex(const uint16_t* px, uint32_t w, uint32_t h, bool rows, int order) {
    const uint32_t lines = rows ? h : w;
    const uint32_t len   = rows ? w : h;
    auto at = [&](uint32_t line, uint32_t i) -> double {
        return rows ? px[static_cast<size_t>(line) * w + i] : px[static_cast<size_t>(i) * w + line];
    };
    std::vector<double> nd;
    nd.reserve(static_cast<size_t>(lines) * (len - 1u) / 8u + 1u);
    for (uint32_t l = 0; l < lines; l += 4u)              // every 4th line is plenty
        for (uint32_t i = 0; i + 1u < len; ++i) nd.push_back(at(l, i + 1u) - at(l, i));
    const double sigmaPx = robustSpread(nd) / std::sqrt(2.0);

    std::vector<double> means(lines, 0.0);
    for (uint32_t l = 0; l < lines; ++l) {
        double s = 0.0;
        for (uint32_t i = 0; i < len; ++i) s += at(l, i);
        means[l] = s / len;
    }
    // order 1: first difference of the line means (variance 2*s^2/len for white noise);
    // order 2: second difference (variance 6*s^2/len), which cancels a smooth gradient --
    // anatomy changes the first difference of the means but barely the second.
    std::vector<double> e;
    e.reserve(lines);
    if (order == 1) {
        for (uint32_t l = 0; l + 1u < lines; ++l) e.push_back(means[l + 1u] - means[l]);
    } else {
        for (uint32_t l = 0; l + 2u < lines; ++l) e.push_back(means[l + 2u] - 2.0 * means[l + 1u] + means[l]);
    }
    const double observed = robustSpread(e);
    const double expected = sigmaPx * std::sqrt((order == 1 ? 2.0 : 6.0) / len);
    return expected > 0.0 ? observed / expected : 0.0;
}

struct FrameStats {
    double maxRowMean = 0.0; int rowsOver = 0;
    double rowIdx1 = 0.0, colIdx1 = 0.0, rowIdx2 = 0.0, colIdx2 = 0.0;
};

FrameStats statsOf(const uint16_t* px, uint32_t w, uint32_t h) {
    FrameStats s;
    const double lim = 0.9 * 65535.0;
    for (uint32_t y = 0; y < h; ++y) {
        double sum = 0.0;
        for (uint32_t x = 0; x < w; ++x) sum += px[static_cast<size_t>(y) * w + x];
        const double m = sum / w;
        s.maxRowMean = std::max(s.maxRowMean, m);
        if (m > lim) ++s.rowsOver;
    }
    s.rowIdx1 = lineNoiseIndex(px, w, h, true, 1);
    s.colIdx1 = lineNoiseIndex(px, w, h, false, 1);
    s.rowIdx2 = lineNoiseIndex(px, w, h, true, 2);
    s.colIdx2 = lineNoiseIndex(px, w, h, false, 2);
    return s;
}

void reportFrame(const char* label, std::vector<uint16_t>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer img = u16Buffer(px, w, h);
    XpeImageMetadata meta{};
    bool dropped = false, nonuni = false;
    const XpeErrorCode rc = xpe_validate_readout_artifact(&img, &meta, &dropped, &nonuni);
    const FrameStats s = statsOf(px.data(), w, h);
    std::printf("[a184] %-44s rc=%d dropped=%d nonuniform_gain=%d | rowsOver0.9FS=%d maxRowMean=%.0f | "
                "LNI1 rows=%.2f cols=%.2f | LNI2 rows=%.2f cols=%.2f\n",
                label, static_cast<int>(rc), dropped ? 1 : 0, nonuni ? 1 : 0, s.rowsOver, s.maxRowMean,
                s.rowIdx1, s.colIdx1, s.rowIdx2, s.colIdx2);
}

std::vector<uint16_t> synth(uint32_t w, uint32_t h, double base, double sigmaPx,
                            double rowSigma, double colSigma, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<double> rowOff(h, 0.0), colOff(w, 0.0);
    for (auto& v : rowOff) v = rowSigma * g(rng);
    for (auto& v : colOff) v = colSigma * g(rng);
    std::vector<uint16_t> px(static_cast<size_t>(w) * h);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const double v = base + sigmaPx * g(rng) + rowOff[y] + colOff[x];
            px[static_cast<size_t>(y) * w + x] =
                static_cast<uint16_t>(std::min(65535.0, std::max(0.0, std::round(v))));
        }
    return px;
}

bool readRaw16(const std::string& path, size_t count, std::vector<uint16_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.resize(count);
    f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(count * sizeof(uint16_t)));
    return static_cast<size_t>(f.gcount()) == count * sizeof(uint16_t);
}

/** Repository root, from this file's compile-time path (.../modules/preprocess/tests/<file>). */
std::string repoRoot() {
    std::string p = __FILE__;
    for (char& c : p) if (c == '\\') c = '/';
    for (int i = 0; i < 4; ++i) {
        const size_t k = p.find_last_of('/');
        if (k == std::string::npos) return std::string();
        p.resize(k);
    }
    return p;
}

// ---------------------------------------------------------------------------
// (3) shared ghost handle
// ---------------------------------------------------------------------------

XpeImageBuffer f32Buffer(std::vector<float>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = px.data(); b.width = w; b.height = h;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
    return b;
}

constexpr float    kFrameValue = 1.0f;
constexpr uint64_t kAcqTime    = 10u;     // constant: dt is 1.0 on every call
// tau1 = tau2 = 1e12 frames: decay = exp(-1/tau) rounds to exactly 1.0f, so the history never
// forgets and hist1[i] == the number of updates applied to element i. A lost update therefore
// stays visible forever (with the default taus the dynamics contract and hide it -- the first
// version of this probe measured exactly that blind spot: 0 mismatches with the default taus).
const char* const kCfg = R"({"tau1":"1e12","tau2":"1e12"})";
const char* g_cfg = kCfg;   // nullptr = the product default time constants (tau1 = 1, tau2 = 20)

struct HandleState { std::vector<float> h1, h2; };

HandleState snapshot(void* handle) {
    const auto* gh = static_cast<const GhostCorrectorHandle*>(handle);
    return HandleState{gh->hist1, gh->hist2};
}

bool sameBits(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

/** K serial calls on a fresh private handle -- the replay every other arm is compared with. */
HandleState serialReplay(uint32_t side, int calls) {
    void* h = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(side, side, g_cfg, &h));
    std::vector<float> px(static_cast<size_t>(side) * side);
    XpeImageMetadata meta{};
    meta.acquisitionTime = kAcqTime;
    for (int i = 0; i < calls; ++i) {
        std::fill(px.begin(), px.end(), kFrameValue);
        XpeImageBuffer img = f32Buffer(px, side, side);
        EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &img, &meta));
    }
    HandleState s = snapshot(h);
    xpe_ghost_destroy(h);
    return s;
}

struct ArmResult {
    int runs = 0, mismatched = 0, runsWithErrors = 0, runsWithNonFinite = 0;
    double maxAbsDiff = 0.0;          // over every element of every mismatched run
    double lostTotal = 0.0, lostMaxRun = 0.0;   // sum over elements of (expected - observed) updates
    double maxAbsDiff2 = 0.0, meanFracElements2 = 0.0;   // same for the slow accumulator hist2
    double meanFracElements = 0.0;    // mean fraction of differing elements in mismatched runs
};

enum class Arm { SharedUnsynchronised, SharedMutex, OwnHandles };

ArmResult runArm(Arm arm, uint32_t side, int callsPerThread, int runs, const HandleState& ref2K,
                 const HandleState& refK) {
    ArmResult r;
    r.runs = runs;
    double fracSum = 0.0, fracSum2 = 0.0;
    for (int run = 0; run < runs; ++run) {
        void* shared = nullptr;
        void* own[2] = {nullptr, nullptr};
        if (arm == Arm::OwnHandles) {
            EXPECT_EQ(XPE_OK, xpe_ghost_create(side, side, g_cfg, &own[0]));
            EXPECT_EQ(XPE_OK, xpe_ghost_create(side, side, g_cfg, &own[1]));
        } else {
            EXPECT_EQ(XPE_OK, xpe_ghost_create(side, side, g_cfg, &shared));
        }

        std::atomic<bool> go{false};
        std::atomic<int> errors{0}, nonFinite{0};
        std::mutex callMutex;
        auto worker = [&](int t) {
            std::vector<float> px(static_cast<size_t>(side) * side);
            XpeImageMetadata meta{};
            meta.acquisitionTime = kAcqTime;
            void* h = (arm == Arm::OwnHandles) ? own[t] : shared;
            while (!go.load(std::memory_order_acquire)) { /* spin: start both threads together */ }
            for (int i = 0; i < callsPerThread; ++i) {
                std::fill(px.begin(), px.end(), kFrameValue);
                XpeImageBuffer img = f32Buffer(px, side, side);
                XpeErrorCode rc;
                if (arm == Arm::SharedMutex) {
                    std::lock_guard<std::mutex> lk(callMutex);
                    rc = xpe_ghost_correct(h, &img, &meta);
                } else {
                    rc = xpe_ghost_correct(h, &img, &meta);
                }
                if (rc != XPE_OK) ++errors;
                for (float v : px) if (!std::isfinite(v)) { ++nonFinite; break; }
            }
        };
        std::thread a(worker, 0), b(worker, 1);
        go.store(true, std::memory_order_release);
        a.join(); b.join();

        bool mismatch = false;
        double runMax = 0.0, frac = 0.0, runLost = 0.0, runMax2 = 0.0, frac2 = 0.0;
        auto compare = [&](void* h, const HandleState& ref) {
            const HandleState s = snapshot(h);
            if (!sameBits(s.h1, ref.h1) || !sameBits(s.h2, ref.h2)) {
                mismatch = true;
                size_t diff = 0;
                for (size_t i = 0; i < s.h1.size(); ++i) {
                    const double d = std::fabs(static_cast<double>(s.h1[i]) - ref.h1[i]);
                    runLost += static_cast<double>(ref.h1[i]) - static_cast<double>(s.h1[i]);
                    if (s.h1[i] != ref.h1[i]) ++diff;
                    runMax = std::max(runMax, d);
                }
                frac = std::max(frac, static_cast<double>(diff) / static_cast<double>(s.h1.size()));
                size_t diff2 = 0;
                for (size_t i = 0; i < s.h2.size(); ++i) {
                    if (s.h2[i] != ref.h2[i]) ++diff2;
                    runMax2 = std::max(runMax2, std::fabs(static_cast<double>(s.h2[i]) - ref.h2[i]));
                }
                frac2 = std::max(frac2, static_cast<double>(diff2) / static_cast<double>(s.h2.size()));
            }
        };
        if (arm == Arm::OwnHandles) { compare(own[0], refK); compare(own[1], refK); }
        else                        { compare(shared, ref2K); }

        if (mismatch) {
            ++r.mismatched; r.maxAbsDiff = std::max(r.maxAbsDiff, runMax); fracSum += frac;
            r.maxAbsDiff2 = std::max(r.maxAbsDiff2, runMax2); fracSum2 += frac2;
            r.lostTotal += runLost; r.lostMaxRun = std::max(r.lostMaxRun, runLost);
        }
        if (errors.load() > 0) ++r.runsWithErrors;
        if (nonFinite.load() > 0) ++r.runsWithNonFinite;

        if (arm == Arm::OwnHandles) { xpe_ghost_destroy(own[0]); xpe_ghost_destroy(own[1]); }
        else xpe_ghost_destroy(shared);
    }
    r.meanFracElements = r.mismatched ? fracSum / r.mismatched : 0.0;
    r.meanFracElements2 = r.mismatched ? fracSum2 / r.mismatched : 0.0;
    return r;
}

void printArm(const char* name, uint32_t side, int k, const ArmResult& r) {
    std::printf("[a184] side=%-4u calls/thread=%-6d %-34s runs=%-4d runs-with-a-different-final-state=%-4d (%.0f%%) "
                "error-returns(runs)=%d non-finite(runs)=%d | hist1: lost-update total=%.0f, worst run=%.0f, "
                "max|dh1|=%.4g, differing elements ~%.1f%% | hist2: max|dh2|=%.4g, differing elements ~%.1f%% "
                "(run = %d updates x %u elements)\n",
                side, k, name, r.runs, r.mismatched, 100.0 * r.mismatched / r.runs, r.runsWithErrors,
                r.runsWithNonFinite, r.lostTotal, r.lostMaxRun, r.maxAbsDiff, 100.0 * r.meanFracElements,
                r.maxAbsDiff2, 100.0 * r.meanFracElements2, 2 * k, side * side);
}

} // namespace

class A184Probes : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

// (2) -----------------------------------------------------------------------
TEST_F(A184Probes, DISABLED_LineNoiseVersusTheReadoutCheck) {
    constexpr uint32_t W = 1024, H = 1024;
    std::printf("\n[a184] (2) xpe_validate_readout_artifact vs frames with and without line noise (%ux%u UINT16)\n", W, H);
    std::printf("[a184] LNI1/LNI2 = spread of the first/second difference of the line means / expected from pixel noise alone"
                " (~1 = no line-correlated noise); LNI2 cancels a smooth gradient such as anatomy\n");

    // synthetic cases: base, pixel sigma, row sigma, col sigma
    { auto f = synth(W, H, 20000, 20, 0,   0,   1u); reportFrame("synthetic clean (base 20000, px 20)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 300, 0,   2u); reportFrame("synthetic ROW line noise (sigma 300)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 0,   300, 3u); reportFrame("synthetic COLUMN line noise (sigma 300)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 1500, 0,  4u); reportFrame("synthetic ROW line noise (sigma 1500)", f, W, H); }
    { auto f = synth(W, H, 58000, 20, 3000, 0,  5u); reportFrame("synthetic ROW noise 3000 near full scale", f, W, H); }
    { auto f = synth(W, H, 62000, 20, 0,   0,   8u); reportFrame("control: CLEAN but bright (base 62000)", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 0,   0,   6u);
      for (uint32_t y = 0; y < H; ++y) f[static_cast<size_t>(y) * W + 500] = 0;
      reportFrame("control: one all-zero column", f, W, H); }
    { auto f = synth(W, H, 20000, 20, 0,   0,   7u);
      for (uint32_t x = 0; x < W; ++x) f[static_cast<size_t>(300) * W + x] = 65535;
      reportFrame("control: one saturated row", f, W, H); }

    // frames that exist in the repository
    const std::string root = repoRoot();
    std::printf("[a184] repository frames (root=%s)\n", root.c_str());
    struct Item { const char* rel; uint32_t w, h; };
    const Item items[] = {
        {"gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw", 3072, 3072},
        {"gui/ImageProcTest/fixtures/gui-s0/raw/synthetic_1024x1024.raw", 1024, 1024},
        {"test_data/synthetic_512x512.raw", 512, 512},
        {".moai/reports/lane-pre/QA-A-51/frames/uniform.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/lines.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/scatter.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/edge.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/checker8.raw", 1024, 1024},
        {".moai/reports/lane-pre/QA-A-51/frames/diag.raw", 1024, 1024},
    };
    for (const Item& it : items) {
        std::vector<uint16_t> px;
        if (!readRaw16(root + "/" + it.rel, static_cast<size_t>(it.w) * it.h, px)) {
            std::printf("[a184] %-44s NOT READ (%s)\n", it.rel, "missing or short");
            continue;
        }
        reportFrame(it.rel, px, it.w, it.h);
    }
}

// (3) -----------------------------------------------------------------------
TEST_F(A184Probes, DISABLED_SharedGhostHandleTwoThreads) {
    struct Cfg { uint32_t side; int callsPerThread; int runs; };
    const Cfg cfgs[] = { {16u, 20000, 50}, {64u, 2000, 50}, {512u, 200, 30} };
    std::printf("\n[a184] (3) two threads, ONE ghost handle; identical frames, constant acquisition time\n");
    std::printf("[a184] method: tau1=tau2=1e12 so the history never forgets; hist1[i] must equal the number of updates;"
                " final hist compared bit-for-bit with a serial replay\n");
    for (const Cfg& c : cfgs) {
        const HandleState ref2K = serialReplay(c.side, 2 * c.callsPerThread);
        const HandleState refK  = serialReplay(c.side, c.callsPerThread);
        printArm("SHARED, no synchronisation", c.side, c.callsPerThread,
                 runArm(Arm::SharedUnsynchronised, c.side, c.callsPerThread, c.runs, ref2K, refK));
        printArm("control: shared + external mutex", c.side, c.callsPerThread,
                 runArm(Arm::SharedMutex, c.side, c.callsPerThread, c.runs, ref2K, refK));
        printArm("control: one handle per thread", c.side, c.callsPerThread,
                 runArm(Arm::OwnHandles, c.side, c.callsPerThread, c.runs, ref2K, refK));
    }
}

// (3b) The same race with the PRODUCT DEFAULT time constants (tau1 = 1, tau2 = 20). The history
// forgets (decay 0.37 and 0.95 per frame), so a lost update fades; only short runs show it.
// "lost" below is the summed hist1 deficit in frame-value units, not an update count.
TEST_F(A184Probes, DISABLED_SharedGhostHandleDefaultTaus) {
    struct Cfg { uint32_t side; int callsPerThread; int runs; };
    const Cfg cfgs[] = { {16u, 8, 3000}, {64u, 20, 1000} };
    g_cfg = nullptr;
    std::printf("\n[a184] (3b) same race, DEFAULT taus (tau1=1, tau2=20); short runs; lost = hist1 deficit in frame units\n");
    for (const Cfg& c : cfgs) {
        const HandleState ref2K = serialReplay(c.side, 2 * c.callsPerThread);
        const HandleState refK  = serialReplay(c.side, c.callsPerThread);
        printArm("SHARED, no synchronisation", c.side, c.callsPerThread,
                 runArm(Arm::SharedUnsynchronised, c.side, c.callsPerThread, c.runs, ref2K, refK));
        printArm("control: shared + external mutex", c.side, c.callsPerThread,
                 runArm(Arm::SharedMutex, c.side, c.callsPerThread, c.runs, ref2K, refK));
        printArm("control: one handle per thread", c.side, c.callsPerThread,
                 runArm(Arm::OwnHandles, c.side, c.callsPerThread, c.runs, ref2K, refK));
    }
    g_cfg = kCfg;
}

// (1) -----------------------------------------------------------------------
// Which code do offset/gain/defect return when sizes disagree? Two different disagreements:
//   map-vs-input    : the loaded calibration map is MW x MH, the input (and output) is IW x IH
//   output-vs-input : the map matches the input, the OUTPUT buffer is one pixel wider
namespace dim {

constexpr uint32_t MW = 4, MH = 4;     // calibration map
constexpr uint32_t IW = 5, IH = 4;     // input for the map-vs-input case

void loadMap(uint32_t type, uint32_t fmt, const void* data, size_t bytes, const char* path) {
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = MW; hdr.height = MH; hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, nullptr, 0, static_cast<const uint8_t*>(data), bytes));
}

XpeImageBuffer buf(void* d, uint32_t w, uint32_t h, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = w; b.height = h; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(static_cast<size_t>(w) * h * (bits / 8));
    return b;
}

void line(const char* what, XpeErrorCode rc) {
    std::printf("[a184]   %-52s -> %d (%s)\n", what, static_cast<int>(rc),
                rc == XPE_ERR_BUFFER_TOO_SMALL ? "BUFFER_TOO_SMALL"
                : rc == XPE_ERR_INVALID_INPUT  ? "INVALID_INPUT"
                : rc == XPE_OK                 ? "OK" : "other");
}

} // namespace dim

TEST_F(A184Probes, DISABLED_DimensionMismatchReturnCodes) {
    using namespace dim;
    std::printf("\n[a184] (1) return code per kind of size disagreement (BUFFER_TOO_SMALL=%d INVALID_INPUT=%d)\n",
                static_cast<int>(XPE_ERR_BUFFER_TOO_SMALL), static_cast<int>(XPE_ERR_INVALID_INPUT));
    XpeImageMetadata meta{};

    { // offset
        const std::vector<float> m(MW * MH, 100.0f);
        loadMap(XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), "a184_offset.xcal");
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset("a184_offset.xcal"));
        std::vector<uint16_t> in(IW * IH, 1000), out(IW * IH, 0), in4(MW * MH, 1000), out5(IW * MH, 0);
        XpeImageBuffer i1 = buf(in.data(), IW, IH, XPE_PIXEL_UINT16, 16), o1 = buf(out.data(), IW, IH, XPE_PIXEL_UINT16, 16);
        line("offset  map 4x4  vs input 5x4 (output 5x4)", xpe_offset_correct(&i1, &o1, &meta));
        XpeImageBuffer i2 = buf(in4.data(), MW, MH, XPE_PIXEL_UINT16, 16), o2 = buf(out5.data(), IW, MH, XPE_PIXEL_UINT16, 16);
        line("offset  map 4x4 = input 4x4, output 5x4", xpe_offset_correct(&i2, &o2, &meta));
        std::remove("a184_offset.xcal");
    }
    { // gain
        const std::vector<float> m(MW * MH, 1.5f);
        loadMap(XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), "a184_gain.xcal");
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain("a184_gain.xcal"));
        // gain contract: UINT16 input, FLOAT32 output
        std::vector<uint16_t> in(IW * IH, 1000), in4(MW * MH, 1000);
        std::vector<float> out(IW * IH, 0.0f), out5(IW * MH, 0.0f);
        XpeImageBuffer i1 = buf(in.data(), IW, IH, XPE_PIXEL_UINT16, 16), o1 = buf(out.data(), IW, IH, XPE_PIXEL_FLOAT32, 32);
        line("gain    map 4x4  vs input 5x4 (output 5x4)", xpe_gain_correct(&i1, &o1, &meta));
        XpeImageBuffer i2 = buf(in4.data(), MW, MH, XPE_PIXEL_UINT16, 16), o2 = buf(out5.data(), IW, MH, XPE_PIXEL_FLOAT32, 32);
        line("gain    map 4x4 = input 4x4, output 5x4", xpe_gain_correct(&i2, &o2, &meta));
        std::remove("a184_gain.xcal");
    }
    { // defect
        const std::vector<uint8_t> m(MW * MH, 0);
        loadMap(XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size(), "a184_defect.xcal");
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("a184_defect.xcal"));
        std::vector<float> in(IW * IH, 1000.0f), out(IW * IH, 0.0f), in4(MW * MH, 1000.0f), out5(IW * MH, 0.0f);
        XpeImageBuffer i1 = buf(in.data(), IW, IH, XPE_PIXEL_FLOAT32, 32), o1 = buf(out.data(), IW, IH, XPE_PIXEL_FLOAT32, 32);
        line("defect  map 4x4  vs input 5x4 (output 5x4)", xpe_defect_correct(&i1, &o1, &meta));
        XpeImageBuffer i2 = buf(in4.data(), MW, MH, XPE_PIXEL_FLOAT32, 32), o2 = buf(out5.data(), IW, MH, XPE_PIXEL_FLOAT32, 32);
        line("defect  map 4x4 = input 4x4, output 5x4", xpe_defect_correct(&i2, &o2, &meta));
        std::remove("a184_defect.xcal");
    }
}
