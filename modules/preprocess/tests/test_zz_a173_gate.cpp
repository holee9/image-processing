/**
 * @file test_zz_a173_gate.cpp
 * @brief QA-A-173 (#230) -- the merge gate for the tile-straddle change:
 *        the defect map must be BIT-IDENTICAL before and after, on every accuracy
 *        fixture. One flipped pixel means the change does not merge.
 *
 * All DISABLED_ on purpose. This file is a HARNESS, not a regression test: it
 * compiles against both the old and the new runtime_detection.h and writes every
 * map it computes to a directory, so two builds can be compared byte for byte.
 *
 *   set XPE_A173_DIR=<dir>
 *   xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter=A173Gate.DISABLED_DumpCorpus
 *
 * WHAT "ACCURACY FIXTURE" MEANS HERE (the card says "all of them"):
 *   1. the frame families the accuracy suite and the QA-A-161 probe build --
 *      uniform, scatter, edge, lines(8), lines(13) -- 5 seeds, clean and with
 *      defects injected at 5 and 10 sigma (rates test: test_runtime_detection_rates)
 *   2. the real frames in CalData_6 (dark + bright01..06, 3072x3072), when the
 *      primary checkout is present -- they are gitignored, so a lane worktree
 *      does not have them
 *   3. BOUNDARY frames built for this card: pixels placed at the exact decision
 *      threshold of the SCALAR rule, plus/minus two ulp, at columns that straddle a
 *      tile boundary. This is the hole the card names: scalar and AVX2 can be
 *      mathematically equal and still differ in float association, so a pixel at
 *      epsilon distance from the threshold can flip. Random frames almost never
 *      contain such a pixel; these do.
 *   4. configuration variants that reach the changed code: tile sizes
 *      {4,7,8,9,16,100,128,129,256,1024,3072}, blend weights, no tile table,
 *      the historical rule (blendWeight == 0), odd frame sizes.
 *
 * The scalar rule (DetectDefectivePixel for every pixel) is the reference: it is
 * exactly what the OLD code used for the pixels that straddled a tile boundary.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "runtime_detection.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

using namespace xpe::preprocess::internal;

/* ------------------------------------------------------------------ helpers */

std::string outDir() {
    char buf[1024];
    size_t len = 0;
    if (getenv_s(&len, buf, sizeof(buf), "XPE_A173_DIR") != 0 || len == 0) return {};
    return std::string(buf);
}

uint64_t fnv1a(const std::vector<uint8_t>& v) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : v) { h ^= b; h *= 1099511628211ull; }
    return h;
}

struct Fixture {
    std::string name;
    uint32_t w = 0, h = 0;
    std::vector<float> px;
};

XpeImageBuffer imageOf(std::vector<float>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer img{};
    img.data = px.data(); img.width = w; img.height = h;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

/** Every pixel through the scalar rule -- the reference the old straddle path used. */
std::vector<uint8_t> scalarReference(const XpeImageBuffer& img, const RuntimeDetectionConfig& cfg) {
    std::vector<uint8_t> map(static_cast<size_t>(img.width) * img.height, 0u);
    std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);
    for (uint32_t y = 0; y < img.height; ++y)
        for (uint32_t x = 0; x < img.width; ++x)
            if (DetectDefectivePixel(&img, x, y, cfg, wv, dev))
                map[static_cast<size_t>(y) * img.width + x] = 1u;
    return map;
}

std::vector<uint8_t> rowsMap(const XpeImageBuffer& img, const RuntimeDetectionConfig& cfg) {
    std::vector<uint8_t> map(static_cast<size_t>(img.width) * img.height, 0u);
    std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);
    DetectRowRange(&img, cfg, map.data(), 0u, img.height, wv, dev);
    return map;
}

size_t countDiff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    size_t d = 0;
    for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) ++d;
    return d;
}
size_t countSet(const std::vector<uint8_t>& a) {
    size_t n = 0;
    for (uint8_t v : a) if (v) ++n;
    return n;
}

/** Writes one map and one manifest line; returns the flagged count. */
size_t emit(const std::string& dir, const std::string& name, const std::vector<uint8_t>& map,
            size_t mismatchVsScalar, const char* note) {
    const size_t flagged = countSet(map);
    std::printf("[a173] %-64s flagged %7zu  fnv %016llx  vs-scalar %zu %s\n", name.c_str(), flagged,
                static_cast<unsigned long long>(fnv1a(map)), mismatchVsScalar, note);
    if (!dir.empty()) {
        std::ofstream f(std::filesystem::path(dir) / (name + ".map"), std::ios::binary);
        f.write(reinterpret_cast<const char*>(map.data()), static_cast<std::streamsize>(map.size()));
        std::ofstream m(std::filesystem::path(dir) / "manifest.txt", std::ios::app);
        m << name << " " << flagged << " " << std::hex << fnv1a(map) << std::dec << "\n";
    }
    return flagged;
}

/* -------------------------------------------------------- synthetic families */

enum class Family { Uniform, Scatter, Edge, Lines8, Lines13 };
const char* familyName(Family f) {
    switch (f) {
        case Family::Uniform: return "uniform"; case Family::Scatter: return "scatter";
        case Family::Edge: return "edge"; case Family::Lines8: return "lines8";
        case Family::Lines13: return "lines13";
    }
    return "?";
}

/** Same construction as the QA-A-161 probe, sized by (w, h). Injects at a 32-px
 *  lattice (16 px margin) when @p k > 0: value += k * localSigma. */
Fixture synth(Family fam, uint32_t w, uint32_t h, uint32_t seed, float k, float uniformSigma = 10.0f) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    Fixture f; f.w = w; f.h = h;
    f.px.resize(static_cast<size_t>(w) * h);
    std::vector<float> sig(f.px.size());
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            float mean = 3000.0f, s = uniformSigma;
            switch (fam) {
                case Family::Uniform: break;
                case Family::Scatter: {
                    mean = 2000.0f + 1200.0f * (static_cast<float>(x) / static_cast<float>(w - 1u));
                    s = 0.35f * std::sqrt(mean);
                    break;
                }
                case Family::Edge:
                    mean = (x >= w / 2u) ? 3000.0f : 1500.0f;
                    s = (x >= w / 2u) ? 25.0f : 12.0f;
                    break;
                case Family::Lines8:
                    mean = 3000.0f + (((x / 8u) % 2u) ? 400.0f : -400.0f); s = 12.0f; break;
                case Family::Lines13:
                    mean = 3000.0f + (((x / 13u) % 2u) ? 400.0f : -400.0f); s = 12.0f; break;
            }
            sig[i] = s;
            f.px[i] = mean + s * g(rng);
        }
    }
    if (k > 0.0f) {
        for (uint32_t y = 16; y + 16 < h; y += 32)
            for (uint32_t x = 16; x + 16 < w; x += 32) {
                const size_t i = static_cast<size_t>(y) * w + x;
                f.px[i] += k * sig[i];
            }
    }
    return f;
}

/* --------------------------------------------------------------- real frames */

bool loadRaw16(const std::string& path, Fixture* out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize bytes = f.tellg();
    const size_t n = static_cast<size_t>(bytes) / 2u;
    if (n != 3072u * 3072u) return false;
    f.seekg(0);
    std::vector<uint16_t> q(n);
    f.read(reinterpret_cast<char*>(q.data()), bytes);
    out->w = 3072u; out->h = 3072u;
    out->px.resize(n);
    for (size_t i = 0; i < n; ++i) out->px[i] = static_cast<float>(q[i]);
    return true;
}

/* ------------------------------------------------- boundary-pixel fixtures --
 * A fixed tile table (independent of the frame, so modifying pixels cannot move
 * it) and, at columns that straddle a tile boundary, pixels bisected to the exact
 * decision threshold of the SCALAR rule, then offset by -2..+2 ulp. */

struct Boundary {
    Fixture fx;
    std::vector<float> table;
    RuntimeDetectionConfig cfg;
    size_t sites = 0;
};

Boundary makeBoundary(uint32_t w, uint32_t h, uint32_t tile, uint32_t seed, uint32_t rowStep) {
    Boundary b;
    b.fx.w = w; b.fx.h = h;
    b.fx.px.resize(static_cast<size_t>(w) * h);
    std::mt19937 rng(seed);
    std::normal_distribution<float> g(0.0f, 1.0f);
    const uint32_t stepX = std::max<uint32_t>(2u, w / 3u);       // noise level steps across the frame
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            b.fx.px[static_cast<size_t>(y) * w + x] = 3000.0f + ((x < stepX) ? 10.0f : 40.0f) * g(rng);

    const uint32_t tilesX = (w + tile - 1u) / tile, tilesY = (h + tile - 1u) / tile;
    b.table.resize(static_cast<size_t>(tilesX) * tilesY);
    for (uint32_t ty = 0; ty < tilesY; ++ty)
        for (uint32_t tx = 0; tx < tilesX; ++tx)
            b.table[static_cast<size_t>(ty) * tilesX + tx] = 8.0f + static_cast<float>((tx * 7u + ty * 3u) % 40u);

    b.cfg = RuntimeDetection_DefaultConfig();
    b.cfg.blendWeight = RUNTIME_DETECTION_BLEND_WEIGHT;
    b.cfg.tileSigma = b.table.data();
    b.cfg.tileSize = tile;
    b.cfg.tilesX = tilesX;
    b.cfg.blendReference = 0.0f;

    XpeImageBuffer img = imageOf(b.fx.px, w, h);
    std::vector<float> wv, dev; wv.reserve(64); dev.reserve(64);
    size_t counter = 0;
    for (uint32_t y = 4; y + 4 < h; y += rowStep) {
        // the straddle columns of the forward walk, one site per boundary
        for (uint32_t x0 = 1u; x0 + 8u <= w - 1u; x0 += 8u) {
            if ((x0 / tile) == ((x0 + 7u) / tile)) continue;
            const uint32_t x = x0 + static_cast<uint32_t>((y / rowStep + x0 / tile) % 8u);   // all 8 lanes get used
            if (x < 2u || x + 2u >= w) continue;
            const size_t idx = static_cast<size_t>(y) * w + x;
            CollectNeighborValues(&img, x, y, 3, wv);
            std::vector<float> nb = wv;
            std::sort(nb.begin(), nb.end());
            const float med = 0.5f * (nb[3] + nb[4]);
            b.fx.px[idx] = med;
            if (DetectDefectivePixel(&img, x, y, b.cfg, wv, dev)) continue;        // not usable
            b.fx.px[idx] = med + 2000.0f;
            if (!DetectDefectivePixel(&img, x, y, b.cfg, wv, dev)) continue;       // not usable
            uint32_t lo = 0, hi = 0;
            float flo = med, fhi = med + 2000.0f;
            std::memcpy(&lo, &flo, 4); std::memcpy(&hi, &fhi, 4);
            while (hi - lo > 1u) {                                                // adjacent floats
                const uint32_t mid = lo + (hi - lo) / 2u;
                float fm; std::memcpy(&fm, &mid, 4);
                b.fx.px[idx] = fm;
                if (DetectDefectivePixel(&img, x, y, b.cfg, wv, dev)) hi = mid; else lo = mid;
            }
            const int k = static_cast<int>(counter % 5u) - 2;                     // -2..+2 ulp around the threshold
            const uint32_t bits = static_cast<uint32_t>(static_cast<int64_t>(hi) + k);
            float v; std::memcpy(&v, &bits, 4);
            b.fx.px[idx] = v;
            ++counter;
        }
    }
    b.sites = counter;
    return b;
}

/* ------------------------------------------------------------ the corpus run */

struct Totals {
    size_t maps = 0, mismatchMaps = 0, vsScalarTotal = 0, flaggedTotal = 0;
};

void runShip(const std::string& dir, Fixture& fx, Totals& t, bool dll) {
    XpeImageBuffer img = imageOf(fx.px, fx.w, fx.h);
    std::vector<float> ts;
    RuntimeDetectionConfig cfg = BuildFrameConfig(&img, ts);
    const std::vector<uint8_t> rows = rowsMap(img, cfg);
    const std::vector<uint8_t> ref = scalarReference(img, cfg);
    const size_t d = countDiff(rows, ref);
    t.flaggedTotal += emit(dir, fx.name + "__ship_rows", rows, d, d ? "  <<<< DIFFERS FROM SCALAR" : "");
    ++t.maps; t.vsScalarTotal += d; if (d) ++t.mismatchMaps;

    if (dll) {
        std::vector<uint8_t> out(rows.size(), 0u);
        XpeImageBuffer o{};
        o.data = out.data(); o.width = fx.w; o.height = fx.h;
        o.bitsAllocated = 8; o.bitsStored = 8; o.format = XPE_PIXEL_UINT8;
        o.dataSize = static_cast<uint32_t>(out.size());
        XpeImageMetadata meta{};
        const XpeErrorCode rc = xpe_defect_detect_runtime(&img, &meta, &o);
        EXPECT_EQ(XPE_OK, rc);
        const size_t dd = countDiff(out, rows);
        emit(dir, fx.name + "__ship_dll", out, dd, dd ? "  <<<< DLL != rows" : "");
        ++t.maps; if (dd) { ++t.mismatchMaps; t.vsScalarTotal += dd; }
    }

    // threaded DetectFrame with the tile configuration (cap > 0 is set by DetectFrame itself)
    RuntimeDetectionConfig tcfg = cfg;
    tcfg.threadCount = 1;
    std::vector<uint8_t> t1(rows.size(), 0u);
    DetectFrame(&img, tcfg, t1.data(), t1.size());
    emit(dir, fx.name + "__detectframe_t1", t1, 0, "");
    ++t.maps;
    for (uint32_t T : {2u, 3u, 5u, 20u}) {
        tcfg.threadCount = T;
        std::vector<uint8_t> tn(rows.size(), 0u);
        DetectFrame(&img, tcfg, tn.data(), tn.size());
        const size_t dt = countDiff(tn, t1);
        std::printf("[a173]   %-62s threads %2u vs 1: %zu different\n", fx.name.c_str(), T, dt);
        t.vsScalarTotal += dt; if (dt) ++t.mismatchMaps;
        EXPECT_EQ(0u, dt) << fx.name << " threads=" << T;
    }
}

}  // namespace

class A173Gate : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

TEST_F(A173Gate, DISABLED_DumpCorpus) {
    const std::string dir = outDir();
    if (!dir.empty()) std::filesystem::create_directories(dir);
    std::printf("\n[a173] output directory: %s\n", dir.empty() ? "(none: comparison against scalar only)" : dir.c_str());
    Totals t;

    // 1. accuracy-suite families, 1024x1024, 5 seeds, clean and injected
    const Family fams[] = {Family::Uniform, Family::Scatter, Family::Edge, Family::Lines8, Family::Lines13};
    const uint32_t seeds[] = {20260911u, 7u, 1234u, 99991u, 424242u};
    for (Family fam : fams)
        for (uint32_t s : seeds)
            for (float k : {0.0f, 5.0f, 10.0f}) {
                Fixture fx = synth(fam, 1024u, 1024u, s, k);
                fx.name = std::string("acc_") + familyName(fam) + "_s" + std::to_string(s) + "_k" + std::to_string(static_cast<int>(k));
                runShip(dir, fx, t, false);
            }
    // noise scaling (the rates test's "invariant under noise scaling")
    for (float sg : {0.5f, 2.0f, 10.0f, 50.0f}) {
        Fixture fx = synth(Family::Uniform, 1024u, 1024u, 20260911u, 10.0f, sg);
        fx.name = "acc_noisescale_" + std::to_string(static_cast<int>(sg * 10.0f));
        runShip(dir, fx, t, true);
    }

    // 2. real frames
    const char* realDir = "D:/workspace-github/image-processing/tests/test_data/CalData_6/";
    const char* realNames[] = {"dark", "bright01", "bright02", "bright03", "bright04", "bright05", "bright06"};
    size_t realLoaded = 0;
    for (const char* n : realNames) {
        Fixture fx;
        if (loadRaw16(std::string(realDir) + n + ".raw", &fx)) {
            fx.name = std::string("real_") + n;
            runShip(dir, fx, t, true);
            ++realLoaded;
        }
    }
    std::printf("[a173] real frames loaded: %zu of 7\n", realLoaded);

    // 3. boundary frames -- pixels at the scalar decision threshold +-2 ulp, at straddle columns
    struct BSpec { const char* name; uint32_t w, h, tile, seed, rowStep; };
    const BSpec bspecs[] = {
        {"boundary_3072_t128", 3072u, 3072u, 128u, 20260930u, 7u},
        {"boundary_3072_t100", 3072u, 3072u, 100u, 20260931u, 7u},
        {"boundary_1024_t128", 1024u, 1024u, 128u, 20260932u, 3u},
        {"boundary_517x203_t64", 517u, 203u, 64u, 20260933u, 3u},
        {"boundary_640x480_t16", 640u, 480u, 16u, 20260934u, 3u},
    };
    for (const BSpec& bs : bspecs) {
        Boundary b = makeBoundary(bs.w, bs.h, bs.tile, bs.seed, bs.rowStep);
        XpeImageBuffer img = imageOf(b.fx.px, b.fx.w, b.fx.h);
        const std::vector<uint8_t> rows = rowsMap(img, b.cfg);
        const std::vector<uint8_t> ref = scalarReference(img, b.cfg);
        const size_t d = countDiff(rows, ref);
        std::printf("[a173]   %s: %zu boundary sites placed\n", bs.name, b.sites);
        t.flaggedTotal += emit(dir, std::string(bs.name) + "__rows", rows, d, d ? "  <<<< DIFFERS FROM SCALAR" : "");
        ++t.maps; t.vsScalarTotal += d; if (d) ++t.mismatchMaps;
        // threads with this table
        RuntimeDetectionConfig tcfg = b.cfg;
        tcfg.threadCount = 1;
        std::vector<uint8_t> t1(rows.size(), 0u);
        DetectFrame(&img, tcfg, t1.data(), t1.size());
        emit(dir, std::string(bs.name) + "__detectframe_t1", t1, 0, "");
        ++t.maps;
        for (uint32_t T : {2u, 3u, 5u, 20u}) {
            tcfg.threadCount = T;
            std::vector<uint8_t> tn(rows.size(), 0u);
            DetectFrame(&img, tcfg, tn.data(), tn.size());
            const size_t dt = countDiff(tn, t1);
            std::printf("[a173]   %-62s threads %2u vs 1: %zu different\n", bs.name, T, dt);
            t.vsScalarTotal += dt; if (dt) ++t.mismatchMaps;
            EXPECT_EQ(0u, dt) << bs.name << " threads=" << T;
        }
    }

    // 4. configuration variants on a few frames
    struct VF { const char* name; Fixture fx; };
    std::vector<VF> vfs;
    vfs.push_back({"var_uniform1024_k10", synth(Family::Uniform, 1024u, 1024u, 20260911u, 10.0f)});
    vfs.push_back({"var_edge1024_k5", synth(Family::Edge, 1024u, 1024u, 7u, 5.0f)});
    vfs.push_back({"var_lines13_1024_k10", synth(Family::Lines13, 1024u, 1024u, 1234u, 10.0f)});
    vfs.push_back({"var_odd517x203_k10", synth(Family::Edge, 517u, 203u, 99991u, 10.0f)});
    vfs.push_back({"var_640x480_k5", synth(Family::Scatter, 640u, 480u, 424242u, 5.0f)});
    vfs.push_back({"var_w10x10", synth(Family::Uniform, 10u, 10u, 3u, 0.0f)});
    vfs.push_back({"var_w9x9_novec", synth(Family::Uniform, 9u, 9u, 3u, 0.0f)});
    for (VF& vf : vfs) {
        vf.fx.name = vf.name;
        XpeImageBuffer img = imageOf(vf.fx.px, vf.fx.w, vf.fx.h);
        const float sg = ComputeGlobalSigma(&img);
        for (uint32_t tile : {4u, 7u, 8u, 9u, 16u, 100u, 128u, 129u, 256u, 1024u, 3072u}) {
            for (float bw : {0.10f, 0.5f, 1.0f}) {
                if (bw != 0.10f && tile != 128u && tile != 7u) continue;
                RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
                uint32_t tilesX = 0;
                std::vector<float> table = ComputeTileSigmas(&img, tile, &tilesX);
                cfg.blendWeight = bw;
                cfg.tileSigma = table.empty() ? nullptr : table.data();
                cfg.tileSize = tile; cfg.tilesX = tilesX;
                cfg.blendReference = sg;
                const std::vector<uint8_t> rows = rowsMap(img, cfg);
                const std::vector<uint8_t> ref = scalarReference(img, cfg);
                const size_t d = countDiff(rows, ref);
                t.flaggedTotal += emit(dir, std::string(vf.name) + "__tile" + std::to_string(tile) + "_w" +
                                            std::to_string(static_cast<int>(bw * 100.0f)), rows, d,
                                       d ? "  <<<< DIFFERS FROM SCALAR" : "");
                ++t.maps; t.vsScalarTotal += d; if (d) ++t.mismatchMaps;
            }
        }
        {   // blend weight > 0 but NO tile table: the frame-wide reference only
            RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
            cfg.blendWeight = RUNTIME_DETECTION_BLEND_WEIGHT; cfg.blendReference = sg;
            const std::vector<uint8_t> rows = rowsMap(img, cfg);
            const std::vector<uint8_t> ref = scalarReference(img, cfg);
            const size_t d = countDiff(rows, ref);
            t.flaggedTotal += emit(dir, std::string(vf.name) + "__notable", rows, d, d ? "  <<<< DIFFERS FROM SCALAR" : "");
            ++t.maps; t.vsScalarTotal += d; if (d) ++t.mismatchMaps;
        }
        {   // the historical rule: blendWeight == 0, floor and cap from the global sigma
            RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
            cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sg;
            cfg.globalSigmaCap = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sg;
            const std::vector<uint8_t> rows = rowsMap(img, cfg);
            const std::vector<uint8_t> ref = scalarReference(img, cfg);
            const size_t d = countDiff(rows, ref);
            t.flaggedTotal += emit(dir, std::string(vf.name) + "__historical", rows, d, d ? "  <<<< DIFFERS FROM SCALAR" : "");
            ++t.maps; t.vsScalarTotal += d; if (d) ++t.mismatchMaps;
        }
    }

    std::printf("\n[a173] SUMMARY  maps %zu | maps differing from the scalar reference or across threads %zu |"
                " total differing pixels %zu | total flagged pixels %zu\n\n",
                t.maps, t.mismatchMaps, t.vsScalarTotal, t.flaggedTotal);
    EXPECT_EQ(0u, t.mismatchMaps);
}

// Timing of the SHIPPED entry point. Run from two copies of the executable (built
// before and after the change) in alternation -- a process cannot hold both.
TEST_F(A173Gate, DISABLED_ShippedWhole) {
    constexpr uint32_t W = 3072u, H = 3072u;
    std::mt19937 rng(20260911u);
    std::normal_distribution<float> g(0.0f, 1.0f);
    std::vector<float> px(static_cast<size_t>(W) * H);
    for (float& v : px) v = 3000.0f + 10.0f * g(rng);
    XpeImageBuffer img = imageOf(px, W, H);
    std::vector<uint8_t> map(px.size(), 0u);
    XpeImageBuffer out{};
    out.data = map.data(); out.width = W; out.height = H;
    out.bitsAllocated = 8; out.bitsStored = 8; out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(map.size());
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));   // warm-up
    double best = 1e18;
    std::printf("[a173perf] obs:");
    for (int r = 0; r < 12; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::printf(" %.1f", ms);
        best = std::min(best, ms);
    }
    std::printf("\n[a173perf] min %.1f ms  flagged %zu  fnv %016llx\n", best, countSet(map),
                static_cast<unsigned long long>(fnv1a(map)));
}

#if XPE_DETECT_HAS_AVX2
// How far apart can the scalar and the vector sigma expressions be? The scalar rule
// evaluates ((w*mad)*mad) + refSq; the vector code evaluates (w*(mad*mad)) + refSq.
// Those differ in association, so they CAN differ by an ulp. This counts how often,
// which is the size of the exposure the card names -- independent of any fixture.
TEST_F(A173Gate, DISABLED_SigmaExpressionExposure) {
    std::mt19937 rng(20260930u);
    std::uniform_real_distribution<float> madD(1.0f, 200.0f), refD(1.0f, 200.0f);
    const float w = RUNTIME_DETECTION_BLEND_WEIGHT;
    constexpr size_t kBatches = 6250000u;                 // x 8 lanes = 50M samples
    size_t vecOldDiff = 0, vecScalarOrderDiff = 0, total = 0;
    for (size_t b = 0; b < kBatches; ++b) {
        alignas(32) float mad[8], ref[8], sScalar[8], sOld[8], sOrder[8];
        for (int i = 0; i < 8; ++i) {
            mad[i] = madD(rng); ref[i] = refD(rng);
            RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
            cfg.blendWeight = w; cfg.blendReference = ref[i];
            sScalar[i] = ResolveSigma(mad[i], cfg, 0u, 0u);
        }
        __m256 m = _mm256_load_ps(mad);
        alignas(32) float refSqArr[8];
        for (int i = 0; i < 8; ++i) refSqArr[i] = (1.0f - w) * ref[i] * ref[i];
        __m256 refSq = _mm256_load_ps(refSqArr);
        __m256 wv = _mm256_set1_ps(w);
        _mm256_store_ps(sOld, _mm256_sqrt_ps(_mm256_add_ps(_mm256_mul_ps(wv, _mm256_mul_ps(m, m)), refSq)));
        _mm256_store_ps(sOrder, _mm256_sqrt_ps(_mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(wv, m), m), refSq)));
        for (int i = 0; i < 8; ++i) {
            uint32_t a, bo, c;
            std::memcpy(&a, &sScalar[i], 4); std::memcpy(&bo, &sOld[i], 4); std::memcpy(&c, &sOrder[i], 4);
            if (a != bo) ++vecOldDiff;
            if (a != c) ++vecScalarOrderDiff;
            ++total;
        }
    }
    std::printf("\n[a173] sigma expression exposure over %zu random (mad, ref) pairs, w = %.2f\n", total, static_cast<double>(w));
    std::printf("[a173]   vector expression as shipped   w*(mad*mad)+refSq  differs from scalar ResolveSigma in %zu (%.6f%%)\n",
                vecOldDiff, 100.0 * static_cast<double>(vecOldDiff) / static_cast<double>(total));
    std::printf("[a173]   vector, scalar association     (w*mad)*mad+refSq  differs from scalar ResolveSigma in %zu (%.6f%%)\n\n",
                vecScalarOrderDiff, 100.0 * static_cast<double>(vecScalarOrderDiff) / static_cast<double>(total));
}
#endif
