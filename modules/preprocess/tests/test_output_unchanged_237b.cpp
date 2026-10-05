/**
 * @file test_output_unchanged_237b.cpp
 * @brief QA-A-237b (#245): the memory reductions of the shipped path change no output byte.
 *
 * Every digest below was recorded on the tree BEFORE any reduction (55d781da) and is compared byte for byte with what
 * the tree produces now: the output frame, and for the ghost stage also the history it leaves behind and the state it
 * keeps (last frame mean, exposure weight). A change that is meant to leave the output alone must leave these alone;
 * a change that touches them is a change of behaviour and has to be decided, not slipped in. These are change
 * detectors, not accuracy checks -- the accuracy of the stages is the business of their own suites.
 *
 * What is covered:
 *  - the shipped entry (xpe_preprocess_pipeline_ex): tiers 1..3, two frame sizes (one whose pixel count is not a
 *    multiple of 8), and the stage bypass combinations that change which stage buffer aliases which;
 *  - the ghost stage on its own, frames of rising magnitude up to a finite overflow (a failed frame: the pixels, the
 *    history and the state must come back as they were), a single huge pixel in an ordinary frame, negative values;
 *  - (hook build) the gain stage on a hand-built snapshot: a scalar map, a polynomial map, a refused map (the output
 *    buffer must not be touched when a gain value is refused).
 *
 * Re-recording (only for a deliberate behaviour change): run with XPE_A237B_PRINT=1, paste the printed table.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "ghost_stable_lag.h"

extern "C" XPE_API void xpe_ghost_destroy(void* handle);

namespace {

// ---------------------------------------------------------------- digest helpers
uint64_t fnv(uint64_t h, const void* data, size_t bytes) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}
constexpr uint64_t kSeed = 1469598103934665603ull;

template <class T>
uint64_t fnvValue(uint64_t h, const T& v) { return fnv(h, &v, sizeof(T)); }

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }   // [0, 1)
};

std::map<std::string, uint64_t>& golden();   // defined at the end of the file

bool envSet(const char* name) {
#ifdef _WIN32
    char* v = nullptr;
    size_t len = 0;
    const bool set = (_dupenv_s(&v, &len, name) == 0 && v != nullptr);
    std::free(v);
    return set;
#else
    return std::getenv(name) != nullptr;
#endif
}

void expectDigest(const std::string& name, uint64_t value) {
    if (envSet("XPE_A237B_PRINT"))
        std::printf("[a237b-golden] {\"%s\", 0x%016llxull},\n", name.c_str(), static_cast<unsigned long long>(value));
    const auto it = golden().find(name);
    if (it == golden().end()) {
        ADD_FAILURE() << "no recorded digest for " << name << " (computed " << std::hex << value << ")";
        return;
    }
    EXPECT_EQ(it->second, value) << name << ": the output changed";
}

// ---------------------------------------------------------------- calibration fixtures
void writeMap(const std::string& path, XCalType type, XCalPixelFormat fmt, uint32_t w, uint32_t h, const void* data,
              size_t bytes) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    std::memcpy(hdr.session_id, "fixture", 8);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(type);
    hdr.pixel_format = static_cast<uint32_t>(fmt);
    hdr.width = w;
    hdr.height = h;
    hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, static_cast<const uint8_t*>(data), bytes));
}

struct Size { uint32_t w, h; };
constexpr Size kSizes[] = {{203, 157}, {256, 192}};   // 31871 pixels (not a multiple of 8) and 49152

void loadMaps(const std::filesystem::path& dir, Size sz) {
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<float> off(n), gain(n);
    std::vector<uint8_t> def(n, 0);
    for (uint32_t y = 0; y < sz.h; ++y) {
        for (uint32_t x = 0; x < sz.w; ++x) {
            const size_t i = static_cast<size_t>(y) * sz.w + x;
            off[i] = 150.0f + 0.37f * static_cast<float>(x % 7) + 0.011f * static_cast<float>(y % 13) + 0.5f * static_cast<float>((i * 2654435761u) % 997u) / 997.0f;
            gain[i] = 0.85f + 0.0003f * static_cast<float>(x % 97) + 0.0002f * static_cast<float>(y % 101);
        }
    }
    const uint32_t pts[][2] = {{5, 7}, {40, 100}, {sz.w - 1, 0}, {0, sz.h - 1}, {sz.w - 1, sz.h - 1}, {90, 20}};
    for (const auto& p : pts) def[static_cast<size_t>(p[1]) * sz.w + p[0]] = 1;
    for (uint32_t dy = 0; dy < 3; ++dy)
        for (uint32_t dx = 0; dx < 3; ++dx) def[static_cast<size_t>(60 + dy) * sz.w + (50 + dx)] = 1;   // a 3x3 cluster
    writeMap((dir / "offset.xcal").string(), XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, sz.w, sz.h, off.data(), n * sizeof(float));
    writeMap((dir / "gain.xcal").string(), XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, sz.w, sz.h, gain.data(), n * sizeof(float));
    writeMap((dir / "defect.xcal").string(), XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, sz.w, sz.h, def.data(), n);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset((dir / "offset.xcal").string().c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain((dir / "gain.xcal").string().c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map((dir / "defect.xcal").string().c_str()));
}

void fillRaw(uint16_t* raw, Size sz, int frame) {
    Rng rng(0x1234u + 77u * static_cast<uint32_t>(frame));
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    const float level = 1.0f + 0.35f * static_cast<float>(frame);
    for (uint32_t y = 0; y < sz.h; ++y) {
        for (uint32_t x = 0; x < sz.w; ++x) {
            const float v = (1500.0f + 8.0f * static_cast<float>(x) + 5.0f * static_cast<float>(y) + 600.0f * rng.unit()) * level;
            raw[static_cast<size_t>(y) * sz.w + x] = static_cast<uint16_t>(v > 65535.0f ? 65535.0f : v);
        }
    }
    raw[0] = 0;
    raw[n - 1] = 65535;
    raw[n / 2] = 65535;
}

// ---------------------------------------------------------------- the shipped entry
struct PipelineRun { uint64_t digest = kSeed; XpeErrorCode rc = XPE_OK; };

// ghostExtra: members added to the ghost configuration (for example `"nlcscBeta":1e36`); polyGain: the gain map is a polynomial
// (generated from four flat levels) instead of a scalar map.
PipelineRun runPipeline(Size sz, int tier, const char* cfg, int frames, bool ghostGiven, const char* ghostExtra = nullptr,
                        bool polyGain = false, bool classifiedGain = false) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "qa_a_237b_digest";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    xpe_preprocess_shutdown();
    PipelineRun out;
    if (xpe_preprocess_init(nullptr) != XPE_OK) { out.rc = XPE_ERR_PROCESSING_FAILED; return out; }
    loadMaps(dir, sz);
    if (classifiedGain) {
        // QA-A-237g: a scalar gain map with pixels outside [0.1, 10.0] -- classified defective at load, so the defect stage masks them
        // together with the defect map. Some sit beside the map's own pixels (the 3x3 cluster, the corners).
        const size_t np = static_cast<size_t>(sz.w) * sz.h;
        std::vector<float> gain(np);
        for (uint32_t y = 0; y < sz.h; ++y)
            for (uint32_t x = 0; x < sz.w; ++x)
                gain[static_cast<size_t>(y) * sz.w + x] = 0.85f + 0.0003f * static_cast<float>(x % 97) + 0.0002f * static_cast<float>(y % 101);
        const uint32_t odd[][2] = {{0, 0}, {1, 0}, {53, 62}, {54, 62}, {49, 60}, {sz.w - 2, 0}, {100, 100}, {101, 100}, {102, 100}, {20, 21}, {sz.w - 1, 1}, {7, 7}};
        float bad = 0.05f;
        for (const auto& o : odd) {
            gain[static_cast<size_t>(o[1]) * sz.w + o[0]] = bad;
            bad = (bad == 0.05f) ? 20.0f : ((bad == 20.0f) ? 0.0f : 0.05f);
        }
        writeMap((dir / "gain.xcal").string(), XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, sz.w, sz.h, gain.data(), np * sizeof(float));
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain((dir / "gain.xcal").string().c_str()));
        xpe_clear_alerts();
    }
    if (polyGain) {
        const size_t np = static_cast<size_t>(sz.w) * sz.h;
        const double doses[] = {1000.0, 2000.0, 3000.0, 4000.0};
        const float values[] = {1.00f, 1.10f, 1.18f, 1.25f};
        std::vector<std::string> paths;
        for (int i = 0; i < 4; ++i) {
            std::vector<float> level(np);
            for (size_t j = 0; j < np; ++j) level[j] = values[i] * (1.0f + 0.0005f * static_cast<float>(j % 11));
            paths.push_back((dir / ("level" + std::to_string(i) + ".xcal")).string());
            writeMap(paths.back(), XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, sz.w, sz.h, level.data(), np * sizeof(float));
        }
        std::vector<const char*> pointers;
        for (const std::string& pth : paths) pointers.push_back(pth.c_str());
        const std::string polyFile = (dir / "gain_poly.xcal").string();
        EXPECT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(pointers.data(), doses, 4, 2, polyFile.c_str()));
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain(polyFile.c_str()));
    }
    void* ghost = nullptr;
    if (ghostGiven) {
        const std::string gcfg = withStableLag(("{\"tier\":" + std::to_string(tier) + (ghostExtra ? std::string(",") + ghostExtra : std::string()) + "}").c_str());
        EXPECT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, gcfg.c_str(), &ghost));
    }
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<float> buf(n);
    XpeImageMetadata meta{};
    meta.pixelPitch_mm = 0.14f;
    for (int f = 0; f < frames; ++f) {
        fillRaw(reinterpret_cast<uint16_t*>(buf.data()), sz, f);
        XpeImageBuffer img{};
        img.width = sz.w;
        img.height = sz.h;
        img.format = XPE_PIXEL_UINT16;
        img.bitsAllocated = img.bitsStored = 16;
        img.data = buf.data();
        img.dataSize = n * sizeof(float);
        out.rc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, ghost, cfg);
        out.digest = fnvValue(out.digest, static_cast<int>(out.rc));
        out.digest = fnvValue(out.digest, static_cast<uint32_t>(img.format));
        out.digest = fnv(out.digest, buf.data(), n * (img.format == XPE_PIXEL_FLOAT32 ? sizeof(float) : sizeof(uint16_t)));
        out.digest = fnvValue(out.digest, meta.flags & ~XPE_FLAG_CORRECTION_BYPASSED);   // QA-A-241: the new bypass flag is asserted in test_a241_safety.cpp; the rest of the flags and every pixel keep their recorded digest
    }
    if (ghost) {
        const GhostCorrectorHandle* gh = static_cast<const GhostCorrectorHandle*>(ghost);
        out.digest = fnv(out.digest, gh->hist1.data(), gh->hist1.size() * sizeof(float));
        out.digest = fnv(out.digest, gh->hist2.data(), gh->hist2.size() * sizeof(float));
        xpe_ghost_destroy(ghost);
    }
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return out;
}

struct Combo { const char* name; const char* cfg; };
const Combo kCombos[] = {
    {"temp_off", "{\"bypassTemp\":true}"},
    {"offset_off", "{\"bypassOffset\":true}"},
    {"nonlin_off", "{\"bypassNonlinearity\":true}"},
    {"temp_offset_nonlin_off", "{\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true}"},
    {"gain_off", "{\"bypassGain\":true}"},
    {"defect_off", "{\"bypassDefect\":true}"},
    {"ghost_off", "{\"bypassGhost\":true}"},
    {"binning2", "{\"binningMode\":2}"},
    {"gain_defect_off", "{\"bypassGain\":true,\"bypassDefect\":true}"},
    {"uint16_out", "{\"bypassGain\":true,\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true}"},
    {"gain_binning_ghost_off", "{\"bypassGain\":true,\"bypassBinning\":true,\"bypassGhost\":true}"},
};

}  // namespace

TEST(OutputUnchanged237b, PipelineEntryDefaultsEveryTierAndSize) {
    for (const Size sz : kSizes) {
        for (int tier = 1; tier <= 3; ++tier) {
            const PipelineRun r = runPipeline(sz, tier, nullptr, 4, true);
            EXPECT_EQ(XPE_OK, r.rc);
            expectDigest("pipeline/default/" + std::to_string(sz.w) + "x" + std::to_string(sz.h) + "/tier" + std::to_string(tier), r.digest);
        }
    }
}

TEST(OutputUnchanged237b, PipelineEntryBypassCombinations) {
    for (const Combo& c : kCombos) {
        const PipelineRun r = runPipeline(kSizes[0], 2, c.cfg, 3, true);
        EXPECT_EQ(XPE_OK, r.rc) << c.name;
        expectDigest(std::string("pipeline/combo/") + c.name, r.digest);
    }
}

// ---------------------------------------------------------------- float32 input: the caller's own buffer goes through the float stages
namespace {

// The uint16 stages are bypassed and the caller hands a float32 frame, so the defect and ghost stages see the CALLER's buffer. A stage
// that fails must leave it as it was; the digests include the buffer after every call, a failed one included.
uint64_t runFloatInput(int tier, bool failing) {
    const Size sz = kSizes[0];
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "qa_a_237b_digest_f32";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    xpe_preprocess_shutdown();
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    loadMaps(dir, sz);
    void* ghost = nullptr;
    const std::string gcfg = withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str());
    EXPECT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, gcfg.c_str(), &ghost));
    // The failing run also bypasses the defect stage (it refuses frames of this size before the ghost stage is reached), so the
    // ghost stage is handed the caller's buffer itself.
    const char* cfg = failing ? "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassGain\":true,\"bypassDefect\":true}"
                              : "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassGain\":true}";
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<float> buf(n);
    XpeImageMetadata meta{};
    uint64_t h = kSeed;
    const float scales[] = {900.0f, 5000.0f, 2.0e38f, 2.0e38f, 700.0f};   // finite (at most 3e38); the second huge frame overflows the history
    const int count = failing ? 5 : 3;
    Rng rng(31337u + static_cast<uint32_t>(tier));
    for (int f = 0; f < count; ++f) {
        for (size_t i = 0; i < n; ++i) buf[i] = scales[f] * (0.5f + rng.unit());
        XpeImageBuffer img{};
        img.width = sz.w;
        img.height = sz.h;
        img.format = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = img.bitsStored = 32;
        img.data = buf.data();
        img.dataSize = n * sizeof(float);
        const std::vector<float> before = buf;
        const XpeErrorCode rc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, ghost, cfg);
        if (failing) {
            // Frame 3 is the failed one: the ghost stage cannot take it. The call says so and the caller's buffer is as given.
            EXPECT_EQ(f == 3 ? XPE_ERR_PROCESSING_FAILED : XPE_OK, rc) << "frame " << f;
            if (f == 3) EXPECT_EQ(0, std::memcmp(before.data(), buf.data(), n * sizeof(float))) << "the caller's buffer changed on a failed frame";
        }
        h = fnvValue(h, static_cast<int>(rc));
        h = fnvValue(h, static_cast<uint32_t>(img.format));
        h = fnv(h, buf.data(), n * sizeof(float));
        h = fnvValue(h, meta.flags & ~XPE_FLAG_CORRECTION_BYPASSED);   // QA-A-241: see above
    }
    const GhostCorrectorHandle* gh = static_cast<const GhostCorrectorHandle*>(ghost);
    h = fnv(h, gh->hist1.data(), gh->hist1.size() * sizeof(float));
    h = fnv(h, gh->hist2.data(), gh->hist2.size() * sizeof(float));
    xpe_ghost_destroy(ghost);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return h;
}

}  // namespace

TEST(OutputUnchanged237b, PipelineEntryFloatInputOnTheCallersOwnBuffer) {
    expectDigest("pipeline/float_input/tier1", runFloatInput(1, false));
    expectDigest("pipeline/float_input/tier3", runFloatInput(3, false));
    expectDigest("pipeline/float_input/failed_frame/tier2", runFloatInput(2, true));
}

// QA-A-237c: the polynomial gain keeps its own route through the pipeline, and a ghost whose frames cannot be proven safe (QA-A-237b/d)
// takes the scratch route there. Both are recorded before the point-wise stages were merged into one working buffer. The scratch route
// is reached through the signal-dependence beta (tier 3): 1e33 leaves the frames succeeding, 1e36 makes them fail (the call returns
// XPE_ERR_PROCESSING_FAILED and the caller's frame is left as it was).
TEST(OutputUnchanged237b, PipelineEntryPolynomialGainAndTheGhostScratchRoute) {
    const PipelineRun poly = runPipeline(kSizes[0], 2, nullptr, 3, true, nullptr, true);
    EXPECT_EQ(XPE_OK, poly.rc);
    expectDigest("pipeline/poly_gain/tier2", poly.digest);

    // QA-A-237g: the classified gain pixels reach the defect stage as the pixels added to the map (every tier).
    for (const Size sz : kSizes) {
        for (int tier = 1; tier <= 3; ++tier) {
            const PipelineRun cl = runPipeline(sz, tier, nullptr, 2, true, nullptr, false, true);
            EXPECT_EQ(XPE_OK, cl.rc);
            expectDigest("pipeline/gain_classified/" + std::to_string(sz.w) + "x" + std::to_string(sz.h) + "/tier" + std::to_string(tier), cl.digest);
        }
    }

#ifdef XPE_CACHE_TEST_HOOKS
    xpe_ghost_in_place_frames = 0;
#endif
    for (const Size sz : kSizes) {
        const PipelineRun ok = runPipeline(sz, 3, nullptr, 4, true, "\"nlcscBeta\":1e33", false);
        EXPECT_EQ(XPE_OK, ok.rc) << "the scratch route takes these frames";
        expectDigest("pipeline/slow_route_ok/" + std::to_string(sz.w) + "x" + std::to_string(sz.h) + "/tier3_beta1e33", ok.digest);
    }
    const PipelineRun failing = runPipeline(kSizes[0], 3, nullptr, 4, true, "\"nlcscBeta\":1e36", false);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, failing.rc) << "these frames fail on the scratch route";
    expectDigest("pipeline/slow_route_failing/203x157/tier3_beta1e36", failing.digest);
#ifdef XPE_CACHE_TEST_HOOKS
    EXPECT_LT(xpe_ghost_in_place_frames, 12ul) << "frames must have taken the scratch route (12 frames were run)";
    EXPECT_GT(xpe_ghost_in_place_frames, 0ul) << "the first frame of each run (history 0) goes in place";
#endif
}

// ---------------------------------------------------------------- the ghost stage on its own
namespace {

uint64_t ghostSequence(int tier, Size sz) {
    void* ghost = nullptr;
    const std::string cfg = withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str());
    EXPECT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg.c_str(), &ghost));
    GhostCorrectorHandle* gh = static_cast<GhostCorrectorHandle*>(ghost);
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<float> buf(n);
    XpeImageMetadata meta{};
    uint64_t h = kSeed;
    // Magnitude of the frame, a few pixels of a different sign, and (last two) a single huge pixel in an ordinary frame.
    const struct { float scale; bool oneHuge; } frames[] = {
        {100.0f, false}, {3000.0f, false}, {40000.0f, false}, {1e5f, false}, {1e10f, false}, {1e15f, false},
        {1e20f, false}, {50.0f, false},   {1e30f, false},     {80.0f, false}, {1e36f, false}, {120.0f, false},
        {3.0e38f, false}, {200.0f, false}, {3000.0f, true},   {3000.0f, true}, {-500.0f, false}, {250.0f, false},
    };
    int index = 0;
    Rng rng(99u + static_cast<uint32_t>(tier));
    for (const auto& f : frames) {
        for (size_t i = 0; i < n; ++i) {
            float v = f.scale * (0.5f + rng.unit());
            if ((i % 31u) == 0u) v = -v;   // some pixels of the other sign
            buf[i] = v;
        }
        if (f.oneHuge) buf[n / 3] = (index % 2 == 0) ? 1.0e36f : -2.0e37f;
        XpeImageBuffer img{};
        img.width = sz.w;
        img.height = sz.h;
        img.format = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = img.bitsStored = 32;
        img.data = buf.data();
        img.dataSize = n * sizeof(float);
        const XpeErrorCode rc = xpe_ghost_correct(ghost, &img, &meta);
        h = fnvValue(h, static_cast<int>(rc));
        h = fnv(h, buf.data(), n * sizeof(float));
        h = fnv(h, gh->hist1.data(), gh->hist1.size() * sizeof(float));
        h = fnv(h, gh->hist2.data(), gh->hist2.size() * sizeof(float));
        h = fnvValue(h, gh->lastFrameMean);
        h = fnvValue(h, gh->exposureWeight);
        ++index;
    }
    xpe_ghost_destroy(ghost);
    xpe_clear_alerts();
    return h;
}

}  // namespace

TEST(OutputUnchanged237b, GhostFramesOfRisingMagnitudeIncludingFailedOnes) {
    for (const Size sz : kSizes) {
        for (int tier = 1; tier <= 3; ++tier) {
            expectDigest("ghost/sequence/" + std::to_string(sz.w) + "x" + std::to_string(sz.h) + "/tier" + std::to_string(tier),
                         ghostSequence(tier, sz));
        }
    }
}

namespace {

// Finite frames whose history overflows: the first huge frame is taken, the second cannot be (its new history exceeds the float range).
// The failed frame must return PROCESSING_FAILED, leave the pixels as given and leave the history and the state as they were.
uint64_t ghostOverflow(int tier) {
    const Size sz = kSizes[1];
    void* ghost = nullptr;
    const std::string cfg = withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str());
    EXPECT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg.c_str(), &ghost));
    GhostCorrectorHandle* gh = static_cast<GhostCorrectorHandle*>(ghost);
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<float> buf(n);
    XpeImageMetadata meta{};
    uint64_t h = kSeed;
    const float scales[] = {1000.0f, 2.0e38f, 2.0e38f, 800.0f, 2.0e38f, 2.0e38f, 1200.0f};
    Rng rng(555u + static_cast<uint32_t>(tier));
    for (size_t f = 0; f < sizeof(scales) / sizeof(scales[0]); ++f) {
        for (size_t i = 0; i < n; ++i) buf[i] = scales[f] * (0.5f + rng.unit());
        const std::vector<float> given = buf;
        const std::vector<float> hist1 = gh->hist1, hist2 = gh->hist2;
        const double mean = gh->lastFrameMean, weight = gh->exposureWeight;
        XpeImageBuffer img{};
        img.width = sz.w;
        img.height = sz.h;
        img.format = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = img.bitsStored = 32;
        img.data = buf.data();
        img.dataSize = n * sizeof(float);
        const XpeErrorCode rc = xpe_ghost_correct(ghost, &img, &meta);
        if (rc != XPE_OK) {
            EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc) << "tier " << tier << " frame " << f;
            EXPECT_EQ(0, std::memcmp(given.data(), buf.data(), n * sizeof(float))) << "tier " << tier << " frame " << f << ": pixels changed";
            EXPECT_TRUE(hist1 == gh->hist1 && hist2 == gh->hist2) << "tier " << tier << " frame " << f << ": history changed";
            EXPECT_EQ(mean, gh->lastFrameMean);
            EXPECT_EQ(weight, gh->exposureWeight);
        }
        h = fnvValue(h, static_cast<int>(rc));
        h = fnv(h, buf.data(), n * sizeof(float));
        h = fnv(h, gh->hist1.data(), gh->hist1.size() * sizeof(float));
        h = fnv(h, gh->hist2.data(), gh->hist2.size() * sizeof(float));
        h = fnvValue(h, gh->lastFrameMean);
        h = fnvValue(h, gh->exposureWeight);
    }
    xpe_ghost_destroy(ghost);
    xpe_clear_alerts();
    return h;
}

}  // namespace

TEST(OutputUnchanged237b, GhostFailedFrameLeavesPixelsHistoryAndStateAsTheyWere) {
    for (int tier = 1; tier <= 3; ++tier) expectDigest("ghost/overflow/tier" + std::to_string(tier), ghostOverflow(tier));
}

// ---------------------------------------------------------------- Codex #142: a coefficient that overflows makes a FAILED frame, not an erased history
namespace {

struct KeptState { XpeErrorCode rc; bool pixelsKept; bool historyKept; bool statsKept; };

// Frame 1 (every pixel `first`) is taken; frame 2 (every pixel `second`) is fed. If frame 2 fails it must leave its pixels, the
// history and the statistics exactly as they were (REQ-P1A-032); the verdict of the in-place safety check must agree with the
// arithmetic of the tier, including its float intermediate coefficients (a1 = a1_base * exposureWeight * signalDependence can
// overflow to +Inf long before a1 * history does).
KeptState secondFrame(const char* cfgJson, Size sz, float first, float second) {
    void* ghost = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfgJson, &ghost));
    GhostCorrectorHandle* gh = static_cast<GhostCorrectorHandle*>(ghost);
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<float> buf(n, first);
    XpeImageMetadata meta{};
    XpeImageBuffer img{};
    img.width = sz.w;
    img.height = sz.h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = img.bitsStored = 32;
    img.data = buf.data();
    img.dataSize = n * sizeof(float);
    EXPECT_EQ(XPE_OK, xpe_ghost_correct(ghost, &img, &meta)) << "the first frame is taken";
    std::fill(buf.begin(), buf.end(), second);
    const std::vector<float> given = buf, hist1 = gh->hist1, hist2 = gh->hist2;
    const double mean = gh->lastFrameMean, weight = gh->exposureWeight;
    KeptState k{};
    k.rc = xpe_ghost_correct(ghost, &img, &meta);
    k.pixelsKept = (std::memcmp(given.data(), buf.data(), n * sizeof(float)) == 0);
    k.historyKept = (hist1 == gh->hist1 && hist2 == gh->hist2);
    k.statsKept = (mean == gh->lastFrameMean && weight == gh->exposureWeight);
    xpe_ghost_destroy(ghost);
    xpe_clear_alerts();
    return k;
}

}  // namespace

TEST(OutputUnchanged237b, GhostCoefficientOverflowIsAFailedFrameNotAnErasedHistory) {
    // The reproduction Codex sent (#142): 3x3, tier 3, a small non-zero history, then a frame whose a1 is +Inf in float.
    const char* codex = "{\"tier\":\"3\",\"alpha1\":0.1,\"tau1\":1,\"alpha2\":0,\"tau2\":1,\"nlcscBeta\":1}";
    const KeptState r = secondFrame(codex, Size{3, 3}, 1e-15f, 1e25f);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc) << "the frame cannot be taken: a1 is +Inf";
    EXPECT_TRUE(r.pixelsKept) << "pixels changed on a failed frame";
    EXPECT_TRUE(r.historyKept) << "the history of the earlier frame was erased or changed by a failed frame";
    EXPECT_TRUE(r.statsKept);

    // The same shape on other sizes, first values and second values, and with a signal-dependence beta that makes it fail earlier.
    const struct { const char* cfg; Size sz; float first, second; } cases[] = {
        {codex, {40, 30}, 1e-30f, 1e20f},
        {codex, {40, 30}, 1e-30f, 1e30f},
        {codex, {37, 29}, 1e-5f, 5e24f},
        {"{\"tier\":\"3\",\"alpha1\":0.1,\"tau1\":1,\"alpha2\":0.01,\"tau2\":20,\"nlcscBeta\":1000}", {40, 30}, 1e-10f, 1e24f},
        {"{\"tier\":\"3\",\"alpha1\":0.1,\"tau1\":1,\"alpha2\":0.01,\"tau2\":20,\"nlcscBeta\":0.1}", {40, 30}, 1e-20f, 1e27f},
        {"{\"tier\":\"2\",\"alpha1\":0.1,\"tau1\":1,\"alpha2\":0.01,\"tau2\":20}", {40, 30}, 1e-20f, 3e37f},
    };
    for (const auto& c : cases) {
        const KeptState k = secondFrame(c.cfg, c.sz, c.first, c.second);
        if (k.rc != XPE_OK) {   // a frame that fails must leave everything as it was; one that is taken is not this test's business
            EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, k.rc) << c.cfg << " " << c.first << " -> " << c.second;
            EXPECT_TRUE(k.pixelsKept && k.historyKept && k.statsKept)
                << c.cfg << " " << c.sz.w << "x" << c.sz.h << " " << c.first << " -> " << c.second << ": state not preserved on a failed frame";
        }
    }
}

// ---------------------------------------------------------------- Codex #142 boundary samples
// NaN and Inf in a frame are refused at the entrance, before anything is written, whatever the history holds.
TEST(OutputUnchanged237b, GhostNonFiniteFramesAreRefusedAndChangeNothing) {
    const Size sz = kSizes[0];
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    const float bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
    for (int tier = 1; tier <= 3; ++tier) {
        for (const bool withHistory : {false, true}) {
            for (const float v : bad) {
                void* ghost = nullptr;
                const std::string cfg = withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str());
                ASSERT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg.c_str(), &ghost));
                GhostCorrectorHandle* gh = static_cast<GhostCorrectorHandle*>(ghost);
                std::vector<float> buf(n, 1000.0f);
                XpeImageMetadata meta{};
                XpeImageBuffer img{};
                img.width = sz.w;
                img.height = sz.h;
                img.format = XPE_PIXEL_FLOAT32;
                img.bitsAllocated = img.bitsStored = 32;
                img.data = buf.data();
                img.dataSize = n * sizeof(float);
                if (withHistory) ASSERT_EQ(XPE_OK, xpe_ghost_correct(ghost, &img, &meta));
                std::fill(buf.begin(), buf.end(), 2000.0f);
                buf[n / 2] = v;
                const std::vector<float> given = buf, hist1 = gh->hist1, hist2 = gh->hist2;
                const double mean = gh->lastFrameMean, weight = gh->exposureWeight;
                EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ghost_correct(ghost, &img, &meta)) << "tier " << tier << " history " << withHistory;
                EXPECT_EQ(0, std::memcmp(given.data(), buf.data(), n * sizeof(float))) << "pixels changed";
                EXPECT_TRUE(hist1 == gh->hist1 && hist2 == gh->hist2) << "history changed";
                EXPECT_TRUE(mean == gh->lastFrameMean && weight == gh->exposureWeight) << "state changed";
                xpe_ghost_destroy(ghost);
                xpe_clear_alerts();
            }
        }
    }
}

#ifdef XPE_CACHE_TEST_HOOKS
// Around the point where a1 = a1_base * exposureWeight * signalDependence leaves the float range: signal-dependence betas in steps
// of 1.25 and second-frame magnitudes in steps of about 3, after a first frame that leaves a small, a unit and a large non-zero
// history. The in-place route must never be taken for a frame the scratch route fails, and where both succeed they must agree.
TEST(OutputUnchanged237b, GhostInPlaceVerdictAroundTheCoefficientOverflow) {
    const Size sz{9, 7};
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    const float firsts[] = {0.0f, 1e-30f, 1e-15f, 1.0f, 1e6f};
    unsigned long inPlaceFrames = 0, failedFrames = 0, comparisons = 0;
    for (int tier = 1; tier <= 3; ++tier) {
        for (double beta = 1e-3; beta < 1e6; beta *= 1.25) {
            for (const float first : firsts) {
                for (float second = 1e18f; second < 3.4e38f; second *= 3.0f) {
                    char cfg[200];
                    std::snprintf(cfg, sizeof(cfg), "{\"tier\":%d,\"alpha1\":0.1,\"tau1\":1,\"alpha2\":0.01,\"tau2\":20,\"nlcscBeta\":%.17g}", tier, beta);
                    void* a = nullptr;
                    void* b = nullptr;
                    ASSERT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg, &a));
                    ASSERT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg, &b));
                    GhostCorrectorHandle* ga = static_cast<GhostCorrectorHandle*>(a);
                    GhostCorrectorHandle* gb = static_cast<GhostCorrectorHandle*>(b);
                    std::vector<float> fa(n), fb(n);
                    XpeImageMetadata meta{};
                    XpeImageBuffer ia{};
                    ia.width = sz.w;
                    ia.height = sz.h;
                    ia.format = XPE_PIXEL_FLOAT32;
                    ia.bitsAllocated = ia.bitsStored = 32;
                    ia.dataSize = n * sizeof(float);
                    XpeImageBuffer ib = ia;
                    ia.data = fa.data();
                    ib.data = fb.data();
                    for (int f = 0; f < 2; ++f) {
                        std::fill(fa.begin(), fa.end(), f == 0 ? first : second);
                        fb = fa;
                        const unsigned long before = xpe_ghost_in_place_frames;
                        const XpeErrorCode rcA = xpe_ghost_correct(a, &ia, &meta);
                        const bool wentInPlace = (xpe_ghost_in_place_frames != before);
                        xpe_ghost_force_slow_path = true;
                        const XpeErrorCode rcB = xpe_ghost_correct(b, &ib, &meta);
                        xpe_ghost_force_slow_path = false;
                        ++comparisons;
                        if (wentInPlace) ++inPlaceFrames;
                        if (rcB != XPE_OK) ++failedFrames;
                        const std::string where = std::string("tier ") + std::to_string(tier) + " beta " + std::to_string(beta) + " " + std::to_string(first) + " -> " + std::to_string(second) + " frame " + std::to_string(f);
                        ASSERT_EQ(rcB, rcA) << where;
                        if (rcB != XPE_OK) ASSERT_FALSE(wentInPlace) << where << ": the in-place route was taken for a frame the scratch route fails";
                        ASSERT_EQ(0, std::memcmp(fa.data(), fb.data(), n * sizeof(float))) << where << ": pixels differ";
                        ASSERT_TRUE(ga->hist1 == gb->hist1 && ga->hist2 == gb->hist2) << where << ": history differs";
                        ASSERT_EQ(gb->lastFrameMean, ga->lastFrameMean) << where;
                        ASSERT_EQ(gb->exposureWeight, ga->exposureWeight) << where;
                    }
                    xpe_ghost_destroy(a);
                    xpe_ghost_destroy(b);
                }
            }
        }
    }
    xpe_clear_alerts();
    std::printf("[a237b] ghost coefficient-overflow boundary: %lu frames, %lu in place, %lu failed on the scratch route\n", comparisons, inPlaceFrames, failedFrames);
    EXPECT_GT(inPlaceFrames, comparisons / 20u);
    EXPECT_GT(failedFrames, comparisons / 20u);
}
#endif  // XPE_CACHE_TEST_HOOKS

// ---------------------------------------------------------------- the gain stage on a hand-built snapshot (hook build)
#ifdef XPE_CACHE_TEST_HOOKS
namespace {

CalibSnapshot gainSnapshot(Size sz, std::shared_ptr<float[]> scalar) {
    CalibSnapshot s;
    s.initialized = true;
    s.gain_map = std::move(scalar);
    s.gain_width = sz.w;
    s.gain_height = sz.h;
    return s;
}

struct GainRun { XpeErrorCode rc; uint64_t digestOut; uint64_t digestList; };

GainRun runGain(const CalibSnapshot& snap, Size sz, uint16_t fillOut, bool withList) {
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::vector<uint16_t> in(n);
    fillRaw(in.data(), sz, 0);
    std::vector<float> out(n);
    std::memset(out.data(), 0x5A, n * sizeof(float));   // a pattern a write would show
    (void)fillOut;
    XpeImageBuffer ib{}, ob{};
    ib.width = ob.width = sz.w;
    ib.height = ob.height = sz.h;
    ib.format = XPE_PIXEL_UINT16;
    ib.bitsAllocated = ib.bitsStored = 16;
    ib.data = in.data();
    ib.dataSize = n * sizeof(uint16_t);
    ob.format = XPE_PIXEL_FLOAT32;
    ob.bitsAllocated = ob.bitsStored = 32;
    ob.data = out.data();
    ob.dataSize = n * sizeof(float);
    XpeImageMetadata meta{};
    std::vector<uint32_t> list;
    GainRun r{};
    r.rc = xpe_gain_correct_in(snap, &ib, &ob, &meta, withList ? &list : nullptr);
    r.digestOut = fnv(kSeed, out.data(), n * sizeof(float));
    r.digestList = fnv(kSeed, list.data(), list.size() * sizeof(uint32_t));
    xpe_clear_alerts();
    return r;
}

std::shared_ptr<float[]> scalarGainMap(Size sz, int variant) {
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    std::shared_ptr<float[]> m(new float[n]);
    Rng rng(4242u + static_cast<uint32_t>(variant));
    for (size_t i = 0; i < n; ++i) m[i] = 0.1f + 9.9f * rng.unit();   // the whole accepted range [0.1, 10]
    if (n > 2) {   // the two ends of the accepted range, where the frame has room for them
        m[1] = 0.1f;
        m[2] = 10.0f;
    }
    return m;
}

}  // namespace

// QA-A-237c: the gain stage may write its float frame over the uint16 frame it reads (output and input the same storage, the
// pipeline's working buffer). The result must be bit for bit what separate buffers give, at every size where the blocks (8192
// pixels) and the vector groups (8) meet differently: below 8, a multiple of 8, one past it, one block, a block and a bit, several
// blocks, and a count that is neither; scalar and polynomial maps.
TEST(OutputUnchanged237b, GainWrittenOverItsOwnInputEqualsSeparateBuffers) {
    const Size sizes[] = {{1, 1}, {7, 1}, {8, 1}, {9, 1}, {15, 1}, {16, 1}, {17, 3}, {8192, 1}, {8193, 1}, {4096, 4}, {128, 129}, {203, 157}, {256, 192}, {16387, 1}};
    for (const bool poly : {false, true}) {
        for (const Size sz : sizes) {
            const size_t n = static_cast<size_t>(sz.w) * sz.h;
            CalibSnapshot snap;
            snap.initialized = true;
            snap.gain_width = sz.w;
            snap.gain_height = sz.h;
            if (!poly) {
                snap.gain_map = scalarGainMap(sz, 7);
            } else {
                const uint32_t coeffs = 2;
                std::shared_ptr<float[]> pc(new float[n * coeffs]);
                Rng rng(99u + sz.w);
                for (size_t i = 0; i < n; ++i) {
                    pc[i * coeffs + 0] = 0.8f + 0.4f * rng.unit();
                    pc[i * coeffs + 1] = 2.0e-6f * rng.unit();
                }
                snap.gain_poly_coeffs = pc;
                snap.gain_poly_num_coeffs = coeffs;
                snap.gain_poly_has_range = true;
                snap.gain_poly_dose_min = 1000.0;
                snap.gain_poly_dose_max = 60000.0;
            }
            std::vector<uint16_t> in(n);
            Rng rng(0xC0FFEEu + sz.w * 31u + sz.h);
            for (size_t i = 0; i < n; ++i) in[i] = static_cast<uint16_t>(rng.next());   // the whole uint16 range
            XpeImageMetadata meta{};

            // separate buffers
            std::vector<float> separate(n, 0.0f);
            XpeImageBuffer ib{}, ob{};
            ib.width = ob.width = sz.w;
            ib.height = ob.height = sz.h;
            ib.format = XPE_PIXEL_UINT16;
            ib.bitsAllocated = ib.bitsStored = 16;
            ib.data = in.data();
            ib.dataSize = n * sizeof(uint16_t);
            ob.format = XPE_PIXEL_FLOAT32;
            ob.bitsAllocated = ob.bitsStored = 32;
            ob.data = separate.data();
            ob.dataSize = n * sizeof(float);
            std::vector<uint32_t> listA;
            ASSERT_EQ(XPE_OK, xpe_gain_correct_in(snap, &ib, &ob, &meta, poly ? &listA : nullptr)) << "separate, " << sz.w << "x" << sz.h;

            // the same storage: the uint16 frame in the first half of a float-sized buffer
            std::unique_ptr<unsigned char[]> shared(new unsigned char[n * sizeof(float)]);
            std::memcpy(shared.get(), in.data(), n * sizeof(uint16_t));
            XpeImageBuffer sin = ib, sout = ob;
            sin.data = shared.get();
            sin.dataSize = n * sizeof(uint16_t);
            sout.data = shared.get();
            sout.dataSize = n * sizeof(float);
            std::vector<uint32_t> listB;
            ASSERT_EQ(XPE_OK, xpe_gain_correct_in(snap, &sin, &sout, &meta, poly ? &listB : nullptr)) << "over its own input, " << sz.w << "x" << sz.h;
            EXPECT_EQ(0, std::memcmp(separate.data(), shared.get(), n * sizeof(float)))
                << (poly ? "polynomial" : "scalar") << " map, " << sz.w << "x" << sz.h << ": the float frame written over its input differs";
            EXPECT_TRUE(listA == listB) << "the classified pixels differ";
            xpe_clear_alerts();
        }
    }
}

// QA-A-237e: the defect stage keeps no frame-sized "already corrected" plane of its own any more (it reuses the cluster search's
// visited plane). Its output is recorded before that change for every mask shape the two planes told apart: isolated pixels,
// 2x2 blocks, lines along the frame border, a blob wider than the fill radius, a dense random mask, an all-masked frame, and the
// union with a per-frame list of classified pixels (shape 6), and (QA-A-237g) shapes 8-11: the scalar map's list of classified pixels
// -- overlapping the map, next to it, alone on an empty map, on a dense map, with duplicates between the two lists and indices past
// the frame; each in place and out of place (the two must agree).
TEST(OutputUnchanged237b, DefectStageOutputForEveryMaskShape) {
    const Size sizes[] = {{1, 1}, {3, 3}, {17, 5}, {64, 64}, {203, 157}, {256, 192}};
    for (const Size sz : sizes) {
        const size_t n = static_cast<size_t>(sz.w) * sz.h;
        for (int shape = 0; shape < 12; ++shape) {
            std::shared_ptr<uint8_t[]> mask(new uint8_t[n]());
            Rng rng(7000u + shape * 131u + sz.w);
            auto at = [&](uint32_t x, uint32_t y) -> uint8_t& { return mask[static_cast<size_t>(y) * sz.w + x]; };
            switch (shape) {
                case 0:   // isolated pixels
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 97u == 0u) ? 1 : 0;
                    break;
                case 1:   // isolated pixels and 2x2 blocks
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 131u == 0u) ? 1 : 0;
                    for (uint32_t k = 0; k < 12; ++k) {
                        const uint32_t x = rng.next() % sz.w, y = rng.next() % sz.h;
                        for (uint32_t dy = 0; dy < 2 && y + dy < sz.h; ++dy)
                            for (uint32_t dx = 0; dx < 2 && x + dx < sz.w; ++dx) at(x + dx, y + dy) = 1;
                    }
                    break;
                case 2:   // a line along the top edge, one along the left edge, a column through the middle
                    for (uint32_t x = 0; x < sz.w; ++x) at(x, 0) = 1;
                    for (uint32_t y = 0; y < sz.h; ++y) { at(0, y) = 1; at(sz.w / 2, y) = 1; }
                    break;
                case 3:   // a blob wider than the fill radius, plus isolated pixels
                    for (uint32_t y = sz.h / 4; y < sz.h / 4 + 40 && y < sz.h; ++y)
                        for (uint32_t x = sz.w / 4; x < sz.w / 4 + 40 && x < sz.w; ++x) at(x, y) = 1;
                    for (size_t i = 0; i < n; ++i) if (rng.next() % 211u == 0u) mask[i] = 1;
                    break;
                case 4:   // dense random
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 10u < 3u) ? 1 : 0;
                    break;
                case 5:   // every pixel
                    for (size_t i = 0; i < n; ++i) mask[i] = 1;
                    break;
                case 7:   // solid blocks of 3x3 up to 33x33 (radius >= 2 fills, some deeper than the fill radius) among isolated pixels
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 173u == 0u) ? 1 : 0;
                    for (const uint32_t side : {3u, 4u, 5u, 7u, 9u, 12u, 17u, 25u, 33u}) {
                        const uint32_t x0 = rng.next() % sz.w, y0 = rng.next() % sz.h;
                        for (uint32_t dy = 0; dy < side && y0 + dy < sz.h; ++dy)
                            for (uint32_t dx = 0; dx < side && x0 + dx < sz.w; ++dx) at(x0 + dx, y0 + dy) = 1;
                    }
                    break;
                case 8:   // isolated pixels; the scalar map's list overlaps some and sits next to others (below)
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 151u == 0u) ? 1 : 0;
                    break;
                case 9:   // an empty map: only the lists mask pixels
                    break;
                case 10:  // a dense map (above the density where the mask is kept as a plane) with the list added
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 10u < 3u) ? 1 : 0;
                    break;
                case 11:  // 2x2 blocks; the lists extend them into larger clusters and repeat each other (below)
                    for (uint32_t k = 0; k < 10; ++k) {
                        const uint32_t x = rng.next() % sz.w, y = rng.next() % sz.h;
                        for (uint32_t dy = 0; dy < 2 && y + dy < sz.h; ++dy)
                            for (uint32_t dx = 0; dx < 2 && x + dx < sz.w; ++dx) at(x + dx, y + dy) = 1;
                    }
                    break;
                default:  // isolated pixels, completed by a per-frame list of classified pixels (below)
                    for (size_t i = 0; i < n; ++i) mask[i] = (rng.next() % 151u == 0u) ? 1 : 0;
                    break;
            }
            CalibSnapshot snap;
            snap.initialized = true;
            snap.defect_map = mask;
            snap.defect_width = sz.w;
            snap.defect_height = sz.h;
            std::vector<uint32_t> classified;
            if (shape == 6) {
                for (uint32_t k = 0; k < 40; ++k) classified.push_back(rng.next() % static_cast<uint32_t>(n));
                classified.push_back(0);
            }
            if (shape >= 8) {
                // The scalar map's list: ascending, without duplicates. Picks at random, plus the neighbours (right, below) of masked
                // pixels so that some join the map's pixels into clusters, plus pixels the map already masks.
                std::vector<uint32_t> scalar;
                const uint32_t picks = (shape == 10) ? 300u : 60u;
                for (uint32_t k = 0; k < picks; ++k) scalar.push_back(rng.next() % static_cast<uint32_t>(n));
                for (size_t i = 0; i + sz.w + 1 < n && scalar.size() < picks + 120u; i += 97) {
                    if (mask[i] != 0) { scalar.push_back(static_cast<uint32_t>(i + 1)); scalar.push_back(static_cast<uint32_t>(i + sz.w)); scalar.push_back(static_cast<uint32_t>(i)); }
                }
                scalar.push_back(0);
                scalar.push_back(static_cast<uint32_t>(n - 1));
                std::sort(scalar.begin(), scalar.end());
                scalar.erase(std::unique(scalar.begin(), scalar.end()), scalar.end());
                snap.gain_defect_idx.reset(new uint32_t[scalar.size()]);
                std::copy(scalar.begin(), scalar.end(), snap.gain_defect_idx.get());
                snap.gain_defect_count = static_cast<uint32_t>(scalar.size());
                if (shape == 9 || shape == 11) {
                    // This frame's list too: repeats of the scalar list, a few new pixels and indices past the frame (ignored).
                    for (size_t k = 0; k < scalar.size(); k += 3) classified.push_back(scalar[k]);
                    for (uint32_t k = 0; k < 8; ++k) classified.push_back(rng.next() % static_cast<uint32_t>(n));
                    classified.push_back(static_cast<uint32_t>(n));
                    classified.push_back(static_cast<uint32_t>(n) + 17u);
                    classified.push_back(0xFFFFFFFFu);
                }
            }
            std::vector<float> in(n);
            Rng vals(0xD0D0u + shape + sz.h);
            for (size_t i = 0; i < n; ++i) in[i] = 4000.0f * vals.unit();
            XpeImageMetadata meta{};

            auto run = [&](bool inPlace, std::vector<float>& result, XpeErrorCode& rc) {
                std::vector<float> src = in;
                result.assign(n, -1.0f);
                XpeImageBuffer ib{}, ob{};
                ib.width = ob.width = sz.w;
                ib.height = ob.height = sz.h;
                ib.format = ob.format = XPE_PIXEL_FLOAT32;
                ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
                ib.data = src.data();
                ib.dataSize = n * sizeof(float);
                ob.data = inPlace ? src.data() : result.data();
                ob.dataSize = n * sizeof(float);
                rc = xpe_defect_correct_in(snap, &ib, &ob, &meta, classified.empty() ? nullptr : &classified);
                if (inPlace) result = src;
                xpe_clear_alerts();
            };
            std::vector<float> outOfPlace, inPlace;
            XpeErrorCode rcA{}, rcB{};
            run(false, outOfPlace, rcA);
            run(true, inPlace, rcB);
            EXPECT_EQ(rcA, rcB);
            EXPECT_EQ(0, std::memcmp(outOfPlace.data(), inPlace.data(), n * sizeof(float))) << "shape " << shape << ", " << sz.w << "x" << sz.h;
            uint64_t h = fnvValue(1469598103934665603ull, static_cast<int>(rcA));
            h = fnv(h, outOfPlace.data(), n * sizeof(float));
            expectDigest("defect/shape" + std::to_string(shape) + "/" + std::to_string(sz.w) + "x" + std::to_string(sz.h), h);
        }
    }
}

TEST(OutputUnchanged237b, GainScalarMapOutputAndRefusedMaps) {
    const Size sz = kSizes[0];
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    const GainRun ok = runGain(gainSnapshot(sz, scalarGainMap(sz, 0)), sz, 0, false);
    EXPECT_EQ(XPE_OK, ok.rc);
    expectDigest("gain/scalar/valid", ok.digestOut);

    // A refused map: the call returns the same code and leaves the output buffer exactly as the caller gave it.
    std::vector<uint32_t> untouched(n, 0x5A5A5A5Au);
    const uint64_t untouchedDigest = fnv(kSeed, untouched.data(), n * sizeof(float));
    const struct { const char* name; size_t index; float value; } bad[] = {
        {"first_zero", 0, 0.0f}, {"middle_nan", n / 2, std::numeric_limits<float>::quiet_NaN()},
        {"last_too_big", n - 1, 11.0f}, {"row_tail_negative", 7, -1.0f}, {"too_small", n - 3, 0.05f},
    };
    for (const auto& b : bad) {
        std::shared_ptr<float[]> m = scalarGainMap(sz, 1);
        m[b.index] = b.value;
        const GainRun r = runGain(gainSnapshot(sz, m), sz, 0, false);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, r.rc) << b.name;
        EXPECT_EQ(untouchedDigest, r.digestOut) << b.name << ": a refused gain map must not touch the output buffer";
    }
}

TEST(OutputUnchanged237b, GainPolynomialMapOutputAndClassifiedPixels) {
    const Size sz = kSizes[0];
    const size_t n = static_cast<size_t>(sz.w) * sz.h;
    const uint32_t coeffs = 2;
    std::shared_ptr<float[]> poly(new float[n * coeffs]);
    Rng rng(777u);
    for (size_t i = 0; i < n; ++i) {
        poly[i * coeffs + 0] = 0.8f + 0.4f * rng.unit();   // c0
        poly[i * coeffs + 1] = 2.0e-6f * rng.unit();       // c1: gain = c0 + c1 * x
    }
    for (size_t i = 0; i < n; i += 997) poly[i * coeffs] = -1.0f;   // evaluates outside [0.1, 10]: classified defective
    CalibSnapshot s;
    s.initialized = true;
    s.gain_poly_coeffs = poly;
    s.gain_poly_num_coeffs = coeffs;
    s.gain_poly_has_range = true;
    s.gain_poly_dose_min = 1000.0;
    s.gain_poly_dose_max = 60000.0;
    s.gain_width = sz.w;
    s.gain_height = sz.h;
    const GainRun r = runGain(s, sz, 0, true);
    EXPECT_EQ(XPE_OK, r.rc);
    expectDigest("gain/poly/output", r.digestOut);
    expectDigest("gain/poly/classified_list", r.digestList);
}
#endif  // XPE_CACHE_TEST_HOOKS

namespace {
std::map<std::string, uint64_t>& golden() {
    static std::map<std::string, uint64_t> g = {
        // recorded on 55d781da (before any reduction); the three gain/ rows exist in the hook build only
        {"ghost/overflow/tier1", 0xde044ba757ec081cull},
        {"ghost/overflow/tier2", 0xa43465e22bd4b861ull},
        {"ghost/overflow/tier3", 0xd2cb41ffe6f22464ull},
        {"pipeline/float_input/failed_frame/tier2", 0x67c12f7153423f6cull},
        {"pipeline/float_input/tier1", 0xfeaa3ef3923603bfull},
        {"pipeline/float_input/tier3", 0x0e3ffaacec8b5233ull},
        // (the pipeline/float_input and ghost/overflow rows above were recorded on 966e0a8f with the old pipeline.cpp and ghost_correct.cpp: gain_correct.cpp is
        //  not involved in them)
        {"gain/poly/classified_list", 0x342f59b67d3b1150ull},
        {"gain/poly/output", 0xa2b80e46cc6b5e7bull},
        {"gain/scalar/valid", 0xcf90b52dd434a576ull},
        {"ghost/sequence/203x157/tier1", 0x222704c1a48b7a57ull},
        {"ghost/sequence/203x157/tier2", 0x0e4cb5cc730a8a29ull},
        {"ghost/sequence/203x157/tier3", 0xe302b3e9913a9f6eull},
        {"ghost/sequence/256x192/tier1", 0xdfa89cd32f1f043bull},
        {"ghost/sequence/256x192/tier2", 0x758ce723ee750df3ull},
        {"ghost/sequence/256x192/tier3", 0xf16481ebea1ad8f0ull},
        {"pipeline/combo/binning2", 0x74ec0d485930158eull},
        {"pipeline/combo/defect_off", 0xeaefd9693cc887f4ull},
        {"pipeline/combo/gain_binning_ghost_off", 0xd2c7c13340283354ull},
        {"pipeline/combo/gain_defect_off", 0x799fdd09f465b68dull},
        {"pipeline/combo/gain_off", 0x60ecccedf9d6ab47ull},
        {"pipeline/combo/ghost_off", 0x88767eb8bc7d8abeull},
        {"pipeline/combo/nonlin_off", 0xb3aca48d1b76d872ull},
        {"pipeline/combo/offset_off", 0x621b0af5f9adfa6aull},
        {"pipeline/combo/temp_off", 0x3230d95ce318e312ull},
        {"pipeline/combo/temp_offset_nonlin_off", 0x0680df6cecb33d0aull},
        {"pipeline/combo/uint16_out", 0xf593cb0e7bd53415ull},
        {"pipeline/default/203x157/tier1", 0x5937a758a6ed1341ull},
        {"pipeline/default/203x157/tier2", 0x1e76a035b3cedc88ull},
        {"pipeline/default/203x157/tier3", 0xf224ca30501e998eull},
        {"pipeline/default/256x192/tier1", 0xa75c9d9e8247a047ull},
        {"pipeline/default/256x192/tier2", 0x77cb58f9c64c6a7aull},
        {"pipeline/default/256x192/tier3", 0x9dafa625dc1ba994ull},
        // recorded on 416c3e3d (before the point-wise stages were merged, QA-A-237c): the polynomial gain and the ghost scratch route through the pipeline
        {"pipeline/poly_gain/tier2", 0x794cad8c2ee0dc20ull},
        {"pipeline/slow_route_failing/203x157/tier3_beta1e36", 0xf9189e72c1ffbc18ull},
        {"pipeline/slow_route_ok/203x157/tier3_beta1e33", 0xdf9b1db6da540934ull},
        {"pipeline/slow_route_ok/256x192/tier3_beta1e33", 0xeec4a5ac55567cceull},
        // recorded on c9d32e59 (before the defect stage dropped its own processed plane, QA-A-237e)
        {"defect/shape0/17x5", 0xdde3d6d2c831fc8aull},
        {"defect/shape0/1x1", 0x79a8a8f0b1d6d159ull},
        {"defect/shape0/203x157", 0xc3a4db5dbcc368f7ull},
        {"defect/shape0/256x192", 0x695d83560cddb205ull},
        {"defect/shape0/3x3", 0xfe3acccff4a9321dull},
        {"defect/shape0/64x64", 0x09d451ea41f21448ull},
        {"defect/shape1/17x5", 0x8c0677a0ee518a97ull},
        {"defect/shape1/1x1", 0x85abe58de3d25f75ull},
        {"defect/shape1/203x157", 0x4f93e620597752d1ull},
        {"defect/shape1/256x192", 0x00de0736174601ecull},
        {"defect/shape1/3x3", 0xf0ab77594867f8c9ull},
        {"defect/shape1/64x64", 0x26db5fb0f0626d53ull},
        {"defect/shape2/17x5", 0xa0a20f57adf3bffcull},
        {"defect/shape2/1x1", 0xffbfbc0359894313ull},
        {"defect/shape2/203x157", 0xd496369a363e2dd3ull},
        {"defect/shape2/256x192", 0x98622fe47d9d4a8aull},
        {"defect/shape2/3x3", 0x13422bb0275b2192ull},
        {"defect/shape2/64x64", 0x9a3d59066ffdccdfull},
        {"defect/shape3/17x5", 0xf68dffe5bee81062ull},
        {"defect/shape3/1x1", 0x502183460450a219ull},
        {"defect/shape3/203x157", 0xeb95795a2b4a7fceull},
        {"defect/shape3/256x192", 0x05b9956bc5b67d02ull},
        {"defect/shape3/3x3", 0x0b06e13c822a3e04ull},
        {"defect/shape3/64x64", 0xf645e17179d30f7full},
        {"defect/shape4/17x5", 0x825964beb2f64d60ull},
        {"defect/shape4/1x1", 0x19062eb11b965b2full},
        {"defect/shape4/203x157", 0x46c4d485bc07ec77ull},
        {"defect/shape4/256x192", 0x7b381e0d03d3960bull},
        {"defect/shape4/3x3", 0x883951dce67554a9ull},
        {"defect/shape4/64x64", 0x0eaba48ff378b7a9ull},
        {"defect/shape5/17x5", 0x694d300c0c9fc47dull},
        {"defect/shape5/1x1", 0xa7b6783e789bf504ull},
        {"defect/shape5/203x157", 0xbf19e5f152f285daull},
        {"defect/shape5/256x192", 0xd35d6fe5662004b7ull},
        {"defect/shape5/3x3", 0xc885589775c12656ull},
        {"defect/shape5/64x64", 0xec26bf643aa1257full},
        {"defect/shape6/17x5", 0x5d95d56b38b8613bull},
        {"defect/shape6/1x1", 0x5cc3c2ce65d29129ull},
        {"defect/shape6/203x157", 0x90f7a8a4a6e8f984ull},
        {"defect/shape6/256x192", 0x4081d7b130ad384aull},
        {"defect/shape6/3x3", 0xdce840b004d75517ull},
        {"defect/shape6/64x64", 0x5296484dad498ed3ull},
        // recorded on 760772e9 (before the defect stage's tables were indexed by masked pixel, QA-A-237f): shape 7, blocks of 3x3 up to 33x33
        {"defect/shape7/17x5", 0xf75209774d0b4ec7ull},
        {"defect/shape7/1x1", 0x870a8bd15bf84728ull},
        {"defect/shape7/203x157", 0xd3215e1ca8146c16ull},
        {"defect/shape7/256x192", 0x7312ef67845290f4ull},
        {"defect/shape7/3x3", 0x7f006fe5d0de67dbull},
        {"defect/shape7/64x64", 0x0d185c4806cca3dcull},
        // recorded on 458025a8 (before the defect stage's union_mask copy was replaced by a MaskSet, QA-A-237g): shapes 8-11 (the gain's classified pixels added to the map), gain-classified pipeline runs
        {"defect/shape10/17x5", 0x980e3c5f5f2a75aaull},
        {"defect/shape10/1x1", 0xb99900857e7830a9ull},
        {"defect/shape10/203x157", 0xc5ff2f627aa9160dull},
        {"defect/shape10/256x192", 0x20fc9c7dcd0453faull},
        {"defect/shape10/3x3", 0xf75941ba98471872ull},
        {"defect/shape10/64x64", 0x72d044447806655eull},
        {"defect/shape11/17x5", 0xf905624833f8ee0dull},
        {"defect/shape11/1x1", 0x42c9559388a58b0aull},
        {"defect/shape11/203x157", 0x3bd021a286cd80cbull},
        {"defect/shape11/256x192", 0x2f25116436e58085ull},
        {"defect/shape11/3x3", 0xe1cff404b1e57b23ull},
        {"defect/shape11/64x64", 0xcc6649df2fee7585ull},
        {"defect/shape8/17x5", 0x028ce60fd1f44475ull},
        {"defect/shape8/1x1", 0x6cff6465ed14b9f7ull},
        {"defect/shape8/203x157", 0xf20c3b2283598f79ull},
        {"defect/shape8/256x192", 0x4c1fb4700e70c24full},
        {"defect/shape8/3x3", 0x0823bde10d0eb45cull},
        {"defect/shape8/64x64", 0x46d18c7a35398aa1ull},
        {"defect/shape9/17x5", 0xa1af933eceda5439ull},
        {"defect/shape9/1x1", 0x486d55f00e6c9964ull},
        {"defect/shape9/203x157", 0x33e8a51cac8a22a3ull},
        {"defect/shape9/256x192", 0xea94eae748182770ull},
        {"defect/shape9/3x3", 0x8879d764ccb53a13ull},
        {"defect/shape9/64x64", 0x29b7a724d852a053ull},
        {"pipeline/gain_classified/203x157/tier1", 0x72a6fbded50cba3full},
        {"pipeline/gain_classified/203x157/tier2", 0x2746313d7704b09cull},
        {"pipeline/gain_classified/203x157/tier3", 0x987573b6753f85c1ull},
        {"pipeline/gain_classified/256x192/tier1", 0xca2d33d0452a704full},
        {"pipeline/gain_classified/256x192/tier2", 0x923805cfeb68e1e2ull},
        {"pipeline/gain_classified/256x192/tier3", 0xfd97f0b84a7b7e4dull},
    };
    return g;
}
}  // namespace

// ---------------------------------------------------------------- the in-place route of the ghost stage against the scratch route (hook build)
#ifdef XPE_CACHE_TEST_HOOKS
// A frame is processed in place only when its arithmetic is PROVEN to stay inside the float range (a frame that cannot fail has
// nothing to undo). That proof is the one thing the in-place route rests on, so it is checked against the route that does not rely on
// it: the same frames are fed to two handles, one normal and one forced onto the scratch route (which can fail and undo). They must
// agree bit for bit, and the normal handle must NEVER have gone in place for a frame the scratch route failed.
TEST(OutputUnchanged237b, GhostInPlaceRouteIsNeverTakenForAFrameTheScratchRouteFails) {
    struct Config { const char* name; const char* json; };
    const Config configs[] = {
        {"stable_lag", ""},
        {"large_alpha", "{\"alpha1\":0.4,\"tau1\":1,\"alpha2\":0.01,\"tau2\":20}"},
        {"huge_beta", "{\"nlcscBeta\":1e10}"},
        {"beta_one", "{\"nlcscBeta\":1}"},
        {"beta_thousand", "{\"nlcscBeta\":1000}"},
        {"slow_tau", "{\"alpha1\":0.1,\"tau1\":1,\"alpha2\":0.002,\"tau2\":400}"},
    };
    const float magnitudes[] = {1e-30f, 1e-15f, 1e-5f, 1.0f, 1e3f, 6e4f, 1e8f, 1e15f, 1e20f, 1e22f, 1e24f, 1e25f, 1e26f, 1e28f, 1e30f, 1e33f, 1e35f, 3e35f,
                               1e36f, 3e36f, 1e37f, 3e37f, 1e38f, 2e38f, 3e38f};
    const Size sizes[] = {{40, 30}, {37, 29}, {2, 5}};   // the last is below 3x3: no spatial blend
    unsigned long inPlaceFrames = 0, failedFrames = 0, frames = 0;
    for (const Config& c : configs) {
        for (int tier = 1; tier <= 3; ++tier) {
            for (const Size sz : sizes) {
                std::string base = c.json[0] ? c.json : "{}";
                const size_t close = base.rfind('}');
                const bool empty = base.find_first_not_of(" {\t", 0) >= close;
                base.insert(close, std::string(empty ? "" : ",") + "\"tier\":" + std::to_string(tier));
                const std::string cfg = (c.json[0] && std::string(c.json).find("alpha1") != std::string::npos) ? base : withStableLag(base.c_str());
                void* a = nullptr;
                void* b = nullptr;
                ASSERT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg.c_str(), &a)) << c.name << " " << cfg;
                ASSERT_EQ(XPE_OK, xpe_ghost_create(sz.w, sz.h, cfg.c_str(), &b));
                GhostCorrectorHandle* ga = static_cast<GhostCorrectorHandle*>(a);
                GhostCorrectorHandle* gb = static_cast<GhostCorrectorHandle*>(b);
                const size_t n = static_cast<size_t>(sz.w) * sz.h;
                Rng rng(0xBEEFu + static_cast<uint32_t>(tier) * 131u + sz.w);
                std::vector<float> fa(n), fb(n);
                XpeImageMetadata meta{};
                for (int f = 0; f < 70; ++f) {
                    const float scale = magnitudes[rng.next() % (sizeof(magnitudes) / sizeof(magnitudes[0]))];
                    const bool oneHuge = (rng.next() % 5u) == 0u;   // an ordinary frame with a single huge pixel
                    for (size_t i = 0; i < n; ++i) {
                        float v = (oneHuge ? 1000.0f : scale) * (0.5f + rng.unit());
                        if ((rng.next() % 7u) == 0u) v = -v;
                        fa[i] = v;
                    }
                    if (oneHuge) fa[rng.next() % n] = scale * (0.5f + rng.unit());
                    fb = fa;
                    XpeImageBuffer ia{};
                    ia.width = sz.w;
                    ia.height = sz.h;
                    ia.format = XPE_PIXEL_FLOAT32;
                    ia.bitsAllocated = ia.bitsStored = 32;
                    ia.dataSize = n * sizeof(float);
                    XpeImageBuffer ib = ia;
                    ia.data = fa.data();
                    ib.data = fb.data();

                    const unsigned long before = xpe_ghost_in_place_frames;
                    const XpeErrorCode rcA = xpe_ghost_correct(a, &ia, &meta);
                    const bool wentInPlace = (xpe_ghost_in_place_frames != before);
                    xpe_ghost_force_slow_path = true;
                    const XpeErrorCode rcB = xpe_ghost_correct(b, &ib, &meta);
                    xpe_ghost_force_slow_path = false;

                    ++frames;
                    if (wentInPlace) ++inPlaceFrames;
                    if (rcB != XPE_OK) ++failedFrames;
                    const std::string where = std::string(c.name) + " tier " + std::to_string(tier) + " " + std::to_string(sz.w) + "x" + std::to_string(sz.h) + " frame " + std::to_string(f);
                    ASSERT_EQ(rcB, rcA) << where;
                    if (rcB != XPE_OK) ASSERT_FALSE(wentInPlace) << where << ": the in-place route was taken for a frame the scratch route fails";
                    ASSERT_EQ(0, std::memcmp(fa.data(), fb.data(), n * sizeof(float))) << where << ": pixels differ";
                    ASSERT_TRUE(ga->hist1 == gb->hist1 && ga->hist2 == gb->hist2) << where << ": history differs";
                    ASSERT_EQ(gb->lastFrameMean, ga->lastFrameMean) << where;
                    ASSERT_EQ(gb->exposureWeight, ga->exposureWeight) << where;
                }
                xpe_ghost_destroy(a);
                xpe_ghost_destroy(b);
            }
        }
    }
    xpe_clear_alerts();
    // Not vacuous: both routes were exercised, and the bound was tested on frames that really fail.
    std::printf("[a237b] ghost differential: %lu frames, %lu in place, %lu failed on the scratch route\n", frames, inPlaceFrames, failedFrames);
    EXPECT_GT(inPlaceFrames, frames / 10u);
    EXPECT_GT(failedFrames, frames / 20u);
}
#endif  // XPE_CACHE_TEST_HOOKS
