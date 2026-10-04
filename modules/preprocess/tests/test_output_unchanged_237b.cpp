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

PipelineRun runPipeline(Size sz, int tier, const char* cfg, int frames, bool ghostGiven) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "qa_a_237b_digest";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    xpe_preprocess_shutdown();
    PipelineRun out;
    if (xpe_preprocess_init(nullptr) != XPE_OK) { out.rc = XPE_ERR_PROCESSING_FAILED; return out; }
    loadMaps(dir, sz);
    void* ghost = nullptr;
    if (ghostGiven) {
        const std::string gcfg = withStableLag(("{\"tier\":" + std::to_string(tier) + "}").c_str());
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
        out.digest = fnvValue(out.digest, meta.flags);
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
        h = fnvValue(h, meta.flags);
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
    m[1] = 0.1f;
    m[2] = 10.0f;
    return m;
}

}  // namespace

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
    };
    return g;
}
}  // namespace
