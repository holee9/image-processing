/**
 * @file test_calib_fixture_gen.cpp
 * @brief QA-A-36 (#141): xpe_calib_fixture_gen — determinism and acceptance.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * The repository ships no XCal fixtures, so GUI, E2E and CI build the set at
 * run time with this tool. Two properties are what make that usable, and both
 * are checked here against the tool as a subprocess rather than against its
 * internals:
 *
 *   1. Determinism — the same --seed must produce byte-identical files, or a
 *      catalog manifest recording their SHA-256 is worthless. The generators
 *      stamp a wall clock into the header, so the tool rewrites the artifact
 *      with created_epoch_ms = 0; a regression there would show up here as two
 *      runs differing.
 *   2. Acceptance — the produced directory must satisfy the pipeline's own
 *      layout expectation (offset.xcal / gain.xcal / defect.xcal under one
 *      directory, pipeline.cpp:311-323). A set that loads but does not drive a
 *      frame is not a fixture.
 *
 * Frames are 256x256 rather than the tool's 1024x1024 default: the bright-pixel
 * detector's mask_size_bright floor is 128, so 256 still exercises the real
 * xpe_bpm_generate path while keeping the suite fast.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include "xpe_fixture_gen_path.h"  // generated: XPE_FIXTURE_GEN_EXE
#include "defect_ext.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 256;
constexpr uint32_t kH = 256;

/** Runs the generator into @p dir and returns its exit code. */
int runGenerator(const fs::path& dir, unsigned seed, uint64_t expiryMs) {
    std::string cmd;
    cmd += "\"";
    cmd += XPE_FIXTURE_GEN_EXE;
    cmd += "\" --out \"" + dir.string() + "\"";
    cmd += " --width " + std::to_string(kW);
    cmd += " --height " + std::to_string(kH);
    cmd += " --seed " + std::to_string(seed);
    cmd += " --expiry-ms " + std::to_string(expiryMs);
#ifdef _WIN32
    // cmd.exe strips the outer pair of quotes from the whole command line, so a
    // quoted executable plus a quoted argument needs one more pair around both.
    cmd = "\"" + cmd + "\"";
#endif
    return std::system(cmd.c_str());
}

/** Runs the generator with a free-form argument string and returns its exit code. */
int runTool(const std::string& arguments) {
    std::string cmd = "\"";
    cmd += XPE_FIXTURE_GEN_EXE;
    cmd += "\" " + arguments;
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";   // see runGenerator
#endif
    return std::system(cmd.c_str());
}

std::vector<char> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
}

uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

class CalibFixtureGenTest : public ::testing::Test {
protected:
    fs::path root;

    void SetUp() override {
        root = fs::temp_directory_path() /
               ("xpe_fixgen_" + std::to_string(nowMs()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::create_directories(root);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root, ec);  // best effort; a leftover temp dir is not a failure
    }

    fs::path caseDir(const char* name) {
        const fs::path d = root / name;
        fs::create_directories(d);
        return d;
    }
};

// The three files plus the manifest are the contract Lane C's catalog reads.
TEST_F(CalibFixtureGenTest, EmitsThreeXCalFilesAndManifest) {
    const fs::path d = caseDir("set");
    ASSERT_EQ(0, runGenerator(d, 0, 0));

    EXPECT_TRUE(fs::exists(d / "offset.xcal"));
    EXPECT_TRUE(fs::exists(d / "gain.xcal"));
    EXPECT_TRUE(fs::exists(d / "defect.xcal"));
    EXPECT_TRUE(fs::exists(d / "manifest.json"));

    // No scratch file survives the run -- a leftover .tmp is what made
    // write_xcal_file's rename fail with IO_FAILED on the next invocation.
    for (const auto& e : fs::directory_iterator(d)) {
        EXPECT_EQ(std::string::npos, e.path().filename().string().find("_scratch"))
            << "scratch file left behind: " << e.path().string();
        EXPECT_NE(".tmp", e.path().extension().string());
    }
}

// The load-bearing property: same seed, same bytes. If this fails, the
// manifest's SHA-256 entries cannot be reproduced on another machine.
TEST_F(CalibFixtureGenTest, SameSeedProducesIdenticalBytes) {
    const fs::path a = caseDir("a");
    const fs::path b = caseDir("b");
    ASSERT_EQ(0, runGenerator(a, 7, 0));
    ASSERT_EQ(0, runGenerator(b, 7, 0));

    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal"}) {
        const std::vector<char> lhs = readAll(a / name);
        const std::vector<char> rhs = readAll(b / name);
        ASSERT_FALSE(lhs.empty()) << name << " is empty";
        EXPECT_TRUE(lhs == rhs) << name << " differs between two runs with the same seed";
    }
}

// The seed has to actually reach the synthetic frames; identical output for
// different seeds would mean the noise is constant and the fixture degenerate.
TEST_F(CalibFixtureGenTest, DifferentSeedChangesOffsetAndGain) {
    const fs::path a = caseDir("s1");
    const fs::path b = caseDir("s2");
    ASSERT_EQ(0, runGenerator(a, 1, 0));
    ASSERT_EQ(0, runGenerator(b, 2, 0));

    EXPECT_FALSE(readAll(a / "offset.xcal") == readAll(b / "offset.xcal"));
    EXPECT_FALSE(readAll(a / "gain.xcal") == readAll(b / "gain.xcal"));
    // defect.xcal is deliberately seed-independent: the defective coordinates
    // are injected at fixed positions so a consumer can assert against them.
    EXPECT_TRUE(readAll(a / "defect.xcal") == readAll(b / "defect.xcal"));
}

// Acceptance: the generated directory is exactly what xpe_preprocess_pipeline
// expects as its calibPath, and one frame runs through it end to end.
TEST_F(CalibFixtureGenTest, GeneratedSetDrivesThePipeline) {
    const fs::path d = caseDir("pipe");
    ASSERT_EQ(0, runGenerator(d, 0, 0));

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

    // Sized for float32: gain correction widens the frame in place, so a
    // uint16-sized buffer would be overrun.
    std::vector<uint8_t> storage(static_cast<size_t>(kW) * kH * sizeof(float), 0);
    uint16_t* pixels = reinterpret_cast<uint16_t*>(storage.data());
    for (size_t i = 0; i < static_cast<size_t>(kW) * kH; ++i) {
        pixels[i] = static_cast<uint16_t>(2000);
    }

    XpeImageBuffer img{};
    img.data          = storage.data();
    img.width         = kW;
    img.height        = kH;
    img.bitsAllocated = 16;
    img.bitsStored    = 16;
    img.format        = XPE_PIXEL_UINT16;
    // The room the buffer really has -- its float32 result is written back into it (QA-A-205b: this claimed
    // the uint16 size, half of what the result needs, and the pipeline accepted that and truncated).
    img.dataSize      = static_cast<uint32_t>(static_cast<size_t>(kW) * kH * sizeof(float));

    XpeImageMetadata meta{};
    // Only the three stages the fixture supplies calibration for are left on.
    const char* config =
        R"({"bypassReadout":true,"bypassTemp":true,"bypassNonlinearity":true,)"
        R"("bypassBinning":true,"bypassGhost":true})";

    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, d.string().c_str(),
                                              nullptr, config));
    EXPECT_TRUE(meta.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GAIN_CORRECTED);
    EXPECT_EQ(XPE_PIXEL_FLOAT32, img.format);

    xpe_preprocess_shutdown();
}

// --expiry-ms lands in the header where xpe_calib_check_expiry reads it, in
// both directions. Written as one case because the two runs only mean something
// compared against each other.
TEST_F(CalibFixtureGenTest, ExpiryTimestampRoundTrips) {
    const uint64_t oneDayMs = 24ull * 60 * 60 * 1000;
    const fs::path past   = caseDir("expired");
    const fs::path future = caseDir("valid");
    ASSERT_EQ(0, runGenerator(past,   0, nowMs() - 2 * oneDayMs));
    ASSERT_EQ(0, runGenerator(future, 0, nowMs() + 30 * oneDayMs));

    bool    expired = false;
    int32_t remaining = 0;

    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry((past / "offset.xcal").string().c_str(),
                                             &expired, &remaining));
    EXPECT_TRUE(expired);
    EXPECT_LT(remaining, 0);

    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry((future / "offset.xcal").string().c_str(),
                                             &expired, &remaining));
    EXPECT_FALSE(expired);
    EXPECT_GT(remaining, 0);
}

// ---------------------------------------------------------------- QA-A-248 (#262): --caldata
// The mode builds the set from real frames. The repository holds no CalData_6 frames (gitignored), so these tests write a small stand-in
// for that folder - the five files the mode reads - and check the mode against it.
constexpr uint64_t kFarExpiryMs = 4102444800000ull;   // 2100-01-01

void writeBytes(const fs::path& p, const void* data, size_t bytes) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
}

/** dark.raw, bright04..06.raw (uint16, kW x kH) and BPMap.map (uint8, three marked pixels); returns the map. */
std::vector<uint8_t> writeCalData(const fs::path& dir) {
    fs::create_directories(dir);
    const size_t n = static_cast<size_t>(kW) * kH;
    std::vector<uint16_t> frame(n);
    for (uint32_t y = 0; y < kH; ++y) {
        for (uint32_t x = 0; x < kW; ++x) {
            frame[y * kW + x] = static_cast<uint16_t>(300 + ((x * 7 + y * 13) % 11));
        }
    }
    writeBytes(dir / "dark.raw", frame.data(), n * 2);
    for (int k = 4; k <= 6; ++k) {
        const double level = 4000.0 + 1000.0 * (k - 4);
        for (uint32_t y = 0; y < kH; ++y) {
            for (uint32_t x = 0; x < kW; ++x) {
                frame[y * kW + x] = static_cast<uint16_t>(level * (0.9 + 0.2 * x / (kW - 1)) + (y % 5));
            }
        }
        const std::string name = "bright0" + std::to_string(k) + ".raw";
        writeBytes(dir / name, frame.data(), n * 2);
    }
    std::vector<uint8_t> map(n, 0u);
    map[10 * kW + 10] = 1u;
    map[200 * kW + 100] = 1u;
    map[255 * kW + 255] = 1u;
    writeBytes(dir / "BPMap.map", map.data(), n);
    return map;
}

std::string asText(const std::vector<char>& v) { return std::string(v.begin(), v.end()); }

// What the mode promises: the three files, byte-identical between runs, defect.xcal carrying BPMap.map as it is, the inputs' hashes in the
// manifest, and a set the module loads.
TEST_F(CalibFixtureGenTest, CalDataModeBuildsTheSetFromTheGivenFramesAndIsByteIdenticalBetweenRuns) {
    const std::vector<uint8_t> map = writeCalData(root / "in");
    const std::string args = "--caldata \"" + (root / "in").string() + "\" --width " + std::to_string(kW) + " --height " + std::to_string(kH) +
                             " --expiry-ms " + std::to_string(kFarExpiryMs);
    const fs::path a = caseDir("a"), b = caseDir("b");
    ASSERT_EQ(0, runTool("--out \"" + a.string() + "\" " + args));
    ASSERT_EQ(0, runTool("--out \"" + b.string() + "\" " + args));

    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal", "manifest.json"}) {
        const std::vector<char> lhs = readAll(a / name);
        ASSERT_FALSE(lhs.empty()) << name << " is empty";
        EXPECT_TRUE(lhs == readAll(b / name)) << name << " differs between two runs on the same frames";
    }

    // The defect map is the file's own bytes: the last kW*kH bytes of defect.xcal are BPMap.map.
    const std::vector<char> defect = readAll(a / "defect.xcal");
    ASSERT_GT(defect.size(), map.size());
    EXPECT_EQ(0, std::memcmp(defect.data() + (defect.size() - map.size()), map.data(), map.size())) << "defect.xcal does not carry BPMap.map unchanged";

    const std::string manifest = asText(readAll(a / "manifest.json"));
    EXPECT_NE(std::string::npos, manifest.find("\"source\": \"caldata\""));
    EXPECT_NE(std::string::npos, manifest.find("\"defect_pixels\": 3"));
    for (const char* name : {"dark.raw", "bright04.raw", "bright05.raw", "bright06.raw", "BPMap.map"}) {
        EXPECT_NE(std::string::npos, manifest.find(std::string("\"") + name + "\": \"")) << "manifest does not name input " << name;
    }

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset((a / "offset.xcal").string().c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain((a / "gain.xcal").string().c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map((a / "defect.xcal").string().c_str()));
    xpe_preprocess_shutdown();
}

// An input that is missing or the wrong size ends the run before anything is written: no half-made set for a later step to pick up.
TEST_F(CalibFixtureGenTest, CalDataModeRefusesAMissingOrWronglySizedInputAndWritesNoMap) {
    struct Case {
        const char* what;
        std::function<void(const fs::path&)> spoil;
        bool giveSize;
        const char* stderrHas;   // text the refusal must name, or nullptr
    };
    const std::vector<Case> cases = {
        {"a missing flat frame", [](const fs::path& in) { fs::remove(in / "bright06.raw"); }, true, "cannot open"},
        {"a dark frame one byte too long",
         [](const fs::path& in) {
             std::ofstream f(in / "dark.raw", std::ios::binary | std::ios::app);
             f.put('\0');
         },
         true, "expected"},
        {"a defect map that is too short", [](const fs::path& in) { fs::resize_file(in / "BPMap.map", 100); }, true, "expected"},
        // 18874368 = 3072 * 3072 * 2: the size the mode expects when no --width/--height is given. A default of 1024 would refuse these
        // frames too, so the refusal alone would not show the default; the number in the message does.
        {"frames that are not the default 3072 x 3072 (no --width/--height given)", [](const fs::path&) {}, false, "expected 18874368"},
    };
    int i = 0;
    for (const Case& c : cases) {
        const fs::path in = root / ("in" + std::to_string(i));
        const fs::path out = caseDir(("out" + std::to_string(i)).c_str());
        writeCalData(in);
        c.spoil(in);
        std::string args = "--out \"" + out.string() + "\" --caldata \"" + in.string() + "\"";
        if (c.giveSize) args += " --width " + std::to_string(kW) + " --height " + std::to_string(kH);
        const fs::path err = root / ("err" + std::to_string(i) + ".txt");
        EXPECT_NE(0, runTool(args + " 2> \"" + err.string() + "\"")) << c.what << " was accepted";
        EXPECT_NE(std::string::npos, asText(readAll(err)).find(c.stderrHas)) << c.what << ": the refusal does not say why (stderr: " << asText(readAll(err)) << ")";
        for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal"}) {
            EXPECT_FALSE(fs::exists(out / name)) << c.what << ": " << name << " was written";
        }
        ++i;
    }
}

// The polynomial gain has no real-frame source in this mode; asking for both is refused up front rather than ignoring one.
TEST_F(CalibFixtureGenTest, CalDataModeCannotBeCombinedWithAGainPolynomial) {
    writeCalData(root / "in");
    const fs::path out = caseDir("out");
    EXPECT_NE(0, runTool("--out \"" + out.string() + "\" --caldata \"" + (root / "in").string() + "\" --gain-poly --width " + std::to_string(kW) + " --height " +
                         std::to_string(kH)));
    EXPECT_FALSE(fs::exists(out / "offset.xcal"));
}

// The synthetic manifest is what it always was: the extra keys belong to --caldata runs only.
TEST_F(CalibFixtureGenTest, TheSyntheticManifestCarriesNoCalDataKeys) {
    const fs::path d = caseDir("set");
    ASSERT_EQ(0, runGenerator(d, 0, 0));
    const std::string manifest = asText(readAll(d / "manifest.json"));
    EXPECT_EQ(std::string::npos, manifest.find("\"source\""));
    EXPECT_EQ(std::string::npos, manifest.find("\"inputs\""));
}

// ---------------------------------------------------------------- QA-A-250: --dark / --bright (any number of real frames)
// Small stand-in frames (256 x 256, the size the older tests use): offset-uncorrected, a pedestal under every frame, a smooth sensitivity ramp in the
// bright ones, one hot pixel in every dark frame, one dead and one hot pixel in every bright frame.
struct FrameSet {
    std::vector<std::string> darks, brights;
};

uint32_t nextNoise(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

void writeU16(const fs::path& path, const std::vector<uint16_t>& v) { writeBytes(path, v.data(), v.size() * sizeof(uint16_t)); }

FrameSet writeFrameSet(const fs::path& dir, size_t darkCount, size_t brightCount, const std::vector<double>& brightDose = {}, int pedestal = 1000) {
    fs::create_directories(dir);
    FrameSet set;
    uint32_t state = 12345u;
    const size_t n = static_cast<size_t>(kW) * kH;
    for (size_t k = 0; k < darkCount; ++k) {
        std::vector<uint16_t> f(n);
        for (uint32_t y = 0; y < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x) f[y * kW + x] = static_cast<uint16_t>(pedestal + ((x * 7 + y * 13) % 11) + (nextNoise(state) % 7));
        f[40 * kW + 40] = 30000;   // hot in every dark frame
        const fs::path path = dir / ("dark_" + std::to_string(k) + ".raw");
        writeU16(path, f);
        set.darks.push_back(path.string());
    }
    for (size_t k = 0; k < brightCount; ++k) {
        std::vector<uint16_t> f(n);
        const double dose = brightDose.empty() ? 1.0 : brightDose[k % brightDose.size()];
        for (uint32_t y = 0; y < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x)
                f[y * kW + x] = static_cast<uint16_t>(pedestal + 20000.0 * dose * (0.9 + 0.2 * x / (kW - 1)) + (nextNoise(state) % 41));
        f[100 * kW + 100] = 0;       // dead in every bright frame
        f[50 * kW + 200] = 60000;    // hot in every bright frame
        const fs::path path = dir / ("bright_" + std::to_string(k) + ".raw");
        writeU16(path, f);
        set.brights.push_back(path.string());
    }
    return set;
}

std::string framesArgs(const FrameSet& set) {
    std::string a = " --width " + std::to_string(kW) + " --height " + std::to_string(kH) + " --expiry-ms " + std::to_string(kFarExpiryMs);
    for (const auto& d : set.darks) a += " --dark \"" + d + "\"";
    for (const auto& b : set.brights) a += " --bright \"" + b + "\"";
    return a;
}

/** Runs the tool in frames mode into @p out, capturing stderr in <root>/<tag>.err; returns the exit code. */
int runFrames(const fs::path& root, const fs::path& out, const std::string& args, const std::string& tag, std::string* err = nullptr) {
    const fs::path errFile = root / (tag + ".err");
    const int rc = runTool("--out \"" + out.string() + "\"" + args + " 2> \"" + errFile.string() + "\"");
    if (err) *err = asText(readAll(errFile));
    return rc;
}

bool hasNoMaps(const fs::path& out) {
    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal"})
        if (fs::exists(out / name)) return false;
    return true;
}

// What the mode promises: the three files from the given frames, byte-identical between runs, the manifest naming every input with its hash and mean, a
// defect map that finds the planted pixels, and a set the module loads.
TEST_F(CalibFixtureGenTest, FramesModeBuildsTheSetFromTheGivenFramesAndIsByteIdenticalBetweenRuns) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path a = caseDir("a"), b = caseDir("b");
    ASSERT_EQ(0, runFrames(root, a, framesArgs(set), "a"));
    ASSERT_EQ(0, runFrames(root, b, framesArgs(set), "b"));
    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal", "manifest.json"}) {
        const std::vector<char> lhs = readAll(a / name);
        ASSERT_FALSE(lhs.empty()) << name << " is empty";
        EXPECT_TRUE(lhs == readAll(b / name)) << name << " differs between two runs on the same frames";
    }
    const std::string manifest = asText(readAll(a / "manifest.json"));
    EXPECT_NE(std::string::npos, manifest.find("\"mode\": \"frames\""));
    EXPECT_NE(std::string::npos, manifest.find("\"frames\": {\"dark\": 6, \"bright\": 12}"));
    size_t sha = 0, mean = 0;
    for (size_t pos = manifest.find("\"sha256\""); pos != std::string::npos; pos = manifest.find("\"sha256\"", pos + 1)) ++sha;
    for (size_t pos = manifest.find("\"mean\": "); pos != std::string::npos; pos = manifest.find("\"mean\": ", pos + 1)) ++mean;
    EXPECT_EQ(18u, sha) << "every input needs its hash in the manifest";
    EXPECT_EQ(18u, mean) << "every input needs its mean in the manifest";
    EXPECT_NE(std::string::npos, manifest.find("\"gain_dark_reference\": \"mean\""));

    // The generated defect map finds what was planted: a hot pixel in every dark, a dead and a hot pixel in every bright.
    const std::vector<char> defect = readAll(a / "defect.xcal");
    const size_t n = static_cast<size_t>(kW) * kH;
    ASSERT_GT(defect.size(), n);
    const char* mask = defect.data() + (defect.size() - n);
    EXPECT_NE(0, mask[40 * kW + 40]) << "hot pixel of the dark frames";
    EXPECT_NE(0, mask[100 * kW + 100]) << "dead pixel of the bright frames";
    EXPECT_NE(0, mask[50 * kW + 200]) << "hot pixel of the bright frames";

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset((a / "offset.xcal").string().c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain((a / "gain.xcal").string().c_str()));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map((a / "defect.xcal").string().c_str()));
    xpe_clear_alerts();   // loading raised an alert (these stand-in maps are not a real set); the hygiene check wants none left behind
    xpe_preprocess_shutdown();
}

// A list file gives the same set as repeating the option; its paths may be relative to the list.
TEST_F(CalibFixtureGenTest, FramesModeListFilesGiveTheSameBytesAsRepeatedOptions) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    {
        std::ofstream d(root / "in" / "darks.txt"), b(root / "in" / "brights.txt");
        d << "# darks, relative to this file\n\n";
        for (const auto& p : set.darks) d << fs::path(p).filename().string() << "\r\n";
        for (const auto& p : set.brights) b << p << "\n";   // absolute
    }
    const fs::path viaOptions = caseDir("opt"), viaLists = caseDir("lst");
    ASSERT_EQ(0, runFrames(root, viaOptions, framesArgs(set), "opt"));
    const std::string listArgs = " --width " + std::to_string(kW) + " --height " + std::to_string(kH) + " --expiry-ms " + std::to_string(kFarExpiryMs) + " --dark-list \"" +
                                 (root / "in" / "darks.txt").string() + "\" --bright-list \"" + (root / "in" / "brights.txt").string() + "\"";
    ASSERT_EQ(0, runFrames(root, viaLists, listArgs, "lst"));
    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal"}) EXPECT_TRUE(readAll(viaOptions / name) == readAll(viaLists / name)) << name;
}

// The dark reference of the gain stage is a choice: the mean of the darks (default), nothing, or one file. Nothing and the default give different maps
// (the pedestal is in the bright frames); a missing reference file is refused.
TEST_F(CalibFixtureGenTest, FramesModeGainDarkReferenceIsAChoice) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path byDefault = caseDir("default"), none = caseDir("none"), file = caseDir("file"), missing = caseDir("missing");
    ASSERT_EQ(0, runFrames(root, byDefault, framesArgs(set), "default"));
    ASSERT_EQ(0, runFrames(root, none, framesArgs(set) + " --gain-dark-ref none", "none"));
    ASSERT_EQ(0, runFrames(root, file, framesArgs(set) + " --gain-dark-ref \"" + set.darks[0] + "\"", "file"));
    EXPECT_FALSE(readAll(byDefault / "gain.xcal") == readAll(none / "gain.xcal")) << "with and without the pedestal subtracted the gain maps cannot be equal";
    EXPECT_TRUE(readAll(byDefault / "offset.xcal") == readAll(none / "offset.xcal")) << "the offset map does not depend on the gain reference";
    EXPECT_NE(std::string::npos, asText(readAll(none / "manifest.json")).find("\"gain_dark_reference\": \"none\""));
    EXPECT_NE(std::string::npos, asText(readAll(file / "manifest.json")).find("\"role\": \"gain_dark_reference\""));
    std::string err;
    EXPECT_NE(0, runFrames(root, missing, framesArgs(set) + " --gain-dark-ref \"" + (root / "no_such_dark.raw").string() + "\"", "missing", &err));
    EXPECT_NE(std::string::npos, err.find("cannot open")) << err;
    EXPECT_TRUE(hasNoMaps(missing));
}

// Someone else's defect map is compared and reported, never used: the three files are byte-identical with and without it. uint8 and uint16 forms agree.
TEST_F(CalibFixtureGenTest, FramesModeReportsAgreementWithAReferenceMapAndDoesNotUseIt) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const size_t n = static_cast<size_t>(kW) * kH;
    std::vector<uint8_t> ref8(n, 0u);
    ref8[40 * kW + 40] = 127u;
    ref8[100 * kW + 100] = 255u;
    ref8[50 * kW + 200] = 255u;
    ref8[10 * kW + 10] = 127u;   // a pixel the generator has no reason to flag
    std::vector<uint16_t> ref16(n);
    for (size_t i = 0; i < n; ++i) ref16[i] = ref8[i];
    writeBytes(root / "ref8.raw", ref8.data(), n);
    writeU16(root / "ref16.raw", ref16);
    const fs::path plain = caseDir("plain"), with8 = caseDir("with8"), with16 = caseDir("with16");
    ASSERT_EQ(0, runFrames(root, plain, framesArgs(set), "plain"));
    ASSERT_EQ(0, runFrames(root, with8, framesArgs(set) + " --compare-bpm \"" + (root / "ref8.raw").string() + "\"", "with8"));
    ASSERT_EQ(0, runFrames(root, with16, framesArgs(set) + " --compare-bpm \"" + (root / "ref16.raw").string() + "\"", "with16"));
    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal"}) {
        EXPECT_TRUE(readAll(plain / name) == readAll(with8 / name)) << name << ": the reference map must not change the set";
        EXPECT_TRUE(readAll(plain / name) == readAll(with16 / name)) << name;
    }
    const std::string m8 = asText(readAll(with8 / "manifest.json")), m16 = asText(readAll(with16 / "manifest.json"));
    EXPECT_EQ(std::string::npos, asText(readAll(plain / "manifest.json")).find("bpm_comparison"));
    EXPECT_NE(std::string::npos, m8.find("\"bpm_comparison\""));
    EXPECT_NE(std::string::npos, m8.find("\"reference\": 4,")) << m8;
    auto field = [](const std::string& m, const char* key) {
        const size_t at = m.find(std::string("\"") + key + "\": ");
        return at == std::string::npos ? std::string() : m.substr(at, m.find_first_of(",\n", at) - at);
    };
    for (const char* key : {"generated", "reference", "both", "recall", "precision", "false_positive_rate"})
        EXPECT_EQ(field(m8, key), field(m16, key)) << key << ": uint8 and uint16 forms of the same map must agree";
    // recall = both / reference, written with 6 decimals
    const size_t both = std::stoul(field(m8, "both").substr(field(m8, "both").find(": ") + 2));
    char expected[64];
    std::snprintf(expected, sizeof expected, "\"recall\": %.6f", static_cast<double>(both) / 4.0);
    EXPECT_EQ(std::string(expected), field(m8, "recall"));
    EXPECT_GE(both, 3u) << "the three planted pixels are in the reference map and in the generated one";
}

// Each refusal ends the run before anything is written, and says why.
TEST_F(CalibFixtureGenTest, FramesModeRefusesTooFewDarkFrames) {
    const FrameSet set = writeFrameSet(root / "in", 4, 12);
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(10, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("too few dark frames: 4")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesTooFewBrightFramesForTheGainMap) {
    const FrameSet set = writeFrameSet(root / "in", 6, 4);
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(10, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("too few bright frames: 4")) << err;
    EXPECT_NE(std::string::npos, err.find("gain map")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesTooFewBrightFramesForTheDefectMap) {
    const FrameSet set = writeFrameSet(root / "in", 6, 7);   // enough for the gain map (5), not for the defect map (10)
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(10, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("too few bright frames: 7")) << err;
    EXPECT_NE(std::string::npos, err.find("defect map")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesAFrameThatIsNotWidthTimesHeight) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    {
        std::ofstream f(set.brights[3], std::ios::binary | std::ios::app);
        f.put('\0');
        f.put('\0');   // two bytes too long: one pixel more
    }
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(9, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("expected")) << err;
    EXPECT_NE(std::string::npos, err.find("bright_3.raw")) << "the message names the file: " << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesASaturatedFrame) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    {
        std::vector<uint16_t> f(static_cast<size_t>(kW) * kH);
        std::ifstream in(set.darks[2], std::ios::binary);
        in.read(reinterpret_cast<char*>(f.data()), static_cast<std::streamsize>(f.size() * 2));
        in.close();
        for (size_t i = 0; i < f.size() / 20; ++i) f[i] = 65535u;   // 5 % of the pixels
        writeU16(set.darks[2], f);
    }
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(11, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("65535")) << err;
    EXPECT_NE(std::string::npos, err.find("dark_2.raw")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesAMissingFrame) {
    FrameSet set = writeFrameSet(root / "in", 6, 12);
    fs::remove(set.brights[5]);
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(9, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("cannot open")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesAReferenceMapOfTheWrongSize) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    std::vector<uint8_t> ref(1000, 0u);
    writeBytes(root / "ref.raw", ref.data(), ref.size());
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(9, runFrames(root, out, framesArgs(set) + " --compare-bpm \"" + (root / "ref.raw").string() + "\"", "x", &err));
    EXPECT_NE(std::string::npos, err.find("expected")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeCannotBeCombinedWithCalDataOrAGainPolynomial) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path a = caseDir("a"), b = caseDir("b");
    std::string err;
    EXPECT_NE(0, runFrames(root, a, framesArgs(set) + " --caldata \"" + (root / "in").string() + "\"", "a", &err));
    EXPECT_NE(std::string::npos, err.find("--caldata")) << err;
    EXPECT_NE(0, runFrames(root, b, framesArgs(set) + " --gain-poly", "b", &err));
    EXPECT_NE(std::string::npos, err.find("--gain-poly")) << err;
    EXPECT_TRUE(hasNoMaps(a));
    EXPECT_TRUE(hasNoMaps(b));
}

// ---------------------------------------------------------------- QA-A-250b: the gain map takes one dose; the defect map may take several
// The gain generator averages its frames and assumes one dose. Bright frames of the gain map whose offset-subtracted means differ by more than 1.25
// (max/min) are refused before anything is written; the limit sits between the largest spread inside one dose level (1.0712) and the smallest step
// between two levels (1.5593) of the cyan_test frames (evidence 10_dose_spread_basis.txt).
TEST_F(CalibFixtureGenTest, FramesModeRefusesGainFramesOfMoreThanOneDose) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12, {1.0, 1.6});
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(12, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("more than one dose")) << err;
    EXPECT_NE(std::string::npos, err.find("limit 1.25")) << err;
    EXPECT_NE(std::string::npos, err.find("bright_")) << "the message names the lowest and the highest frame: " << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, FramesModeAcceptsGainFramesWithinTheDoseLimit) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12, {1.0, 1.15});
    const fs::path out = caseDir("out");
    ASSERT_EQ(0, runFrames(root, out, framesArgs(set), "x"));
    const std::string manifest = asText(readAll(out / "manifest.json"));
    EXPECT_NE(std::string::npos, manifest.find("\"gain_dose_ratio_limit\": 1.25"));
    EXPECT_NE(std::string::npos, manifest.find("\"gain_dose_ratio\": 1.1")) << "the measured ratio is written: " << manifest;
}

// The ratio is taken after the offset is subtracted. With a pedestal of 15000 ADU a net dose ratio of 1.4 is a raw-mean ratio of only 1.23: judged on the
// raw means the frames would pass, judged on the offset-subtracted means they are refused.
TEST_F(CalibFixtureGenTest, FramesModeJudgesTheDoseAfterSubtractingTheOffset) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12, {1.0, 1.4}, 15000);
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(12, runFrames(root, out, framesArgs(set), "x", &err));
    EXPECT_NE(std::string::npos, err.find("ratio 1.4")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

// The defect map has its own bright frames, of any doses; frames named in both lists are read once and marked so in the manifest.
TEST_F(CalibFixtureGenTest, FramesModeTakesTheDefectMapFromItsOwnBrightFramesAndAllowsDoseMixingThere) {
    const FrameSet gain = writeFrameSet(root / "in", 6, 6);                  // one dose, enough for the gain map, too few for the defect map
    const FrameSet more = writeFrameSet(root / "more", 0, 6, {1.6});         // another dose
    std::string args = framesArgs(gain);
    for (const auto& b : gain.brights) args += " --bpm-bright \"" + b + "\"";
    for (const auto& b : more.brights) args += " --bpm-bright \"" + b + "\"";
    const fs::path out = caseDir("out");
    ASSERT_EQ(0, runFrames(root, out, args, "x"));
    const std::string manifest = asText(readAll(out / "manifest.json"));
    EXPECT_NE(std::string::npos, manifest.find("\"frames\": {\"dark\": 6, \"bright\": 6, \"bpm_bright\": 12}")) << manifest;
    auto count = [&](const std::string& what) {
        size_t n = 0;
        for (size_t pos = manifest.find(what); pos != std::string::npos; pos = manifest.find(what, pos + 1)) ++n;
        return n;
    };
    EXPECT_EQ(6u, count("\"role\": \"bright_and_bpm_bright\""));
    EXPECT_EQ(6u, count("\"role\": \"bpm_bright\""));
    EXPECT_EQ(18u, count("\"sha256\"")) << "each frame once: 6 darks + 6 shared + 6 defect-map-only";
    const std::vector<char> defect = readAll(out / "defect.xcal");
    const size_t n = static_cast<size_t>(kW) * kH;
    ASSERT_GT(defect.size(), n);
    const char* mask = defect.data() + (defect.size() - n);
    EXPECT_NE(0, mask[100 * kW + 100]) << "the dead pixel of the bright frames is found from the defect map's own frames";
}

// QA-A-254: --bpm-v2 writes defect.xcal as format version 2 from the second-generation chain: the pixel map with the line mask merged in, the imaging area of --bpm-margins, the line records
// with their individual stretches, a blob mark per group of defective pixels, the unconfirmed candidates apart; the manifest names it and states its limits.
TEST_F(CalibFixtureGenTest, FramesModeBpmV2WritesFormatVersion2WithTheAreaTheLinesAndTheBlobs) {
    const FrameSet set = writeFrameSet(root / "in", 8, 6);                   // one dose: the gain map
    const FrameSet more = writeFrameSet(root / "more", 0, 6, {1.6, 2.2});    // other doses, for the defect map only
    // a double row defect in every bright frame: the response of rows 120 and 121 is 30 % (a defect: below 45 %), over x = 10..119 and x = 60..245
    std::vector<std::string> all = set.brights;
    all.insert(all.end(), more.brights.begin(), more.brights.end());
    for (const auto& b : all) {
        std::vector<char> raw = readAll(b);
        auto* px = reinterpret_cast<uint16_t*>(raw.data());
        for (uint32_t x = 10; x < 120; ++x) px[120 * kW + x] = static_cast<uint16_t>(1000 + (px[120 * kW + x] - 1000) * 0.30);   // two adjacent rows with different stretches:
        for (uint32_t x = 60; x < 246; ++x) px[121 * kW + x] = static_cast<uint16_t>(1000 + (px[121 * kW + x] - 1000) * 0.30);   // one group of width 2, two individual stretches
        std::ofstream(b, std::ios::binary | std::ios::trunc).write(raw.data(), static_cast<std::streamsize>(raw.size()));
    }
    const fs::path a = caseDir("a"), b = caseDir("b");
    std::string args = framesArgs(set) + " --bpm-v2 --bpm-margins 3,2,1,1";
    for (const auto& f : all) args += " --bpm-bright \"" + f + "\"";
    std::string err;
    ASSERT_EQ(0, runFrames(root, a, args, "a", &err)) << err;
    ASSERT_EQ(0, runFrames(root, b, args, "b"));
    for (const char* name : {"offset.xcal", "gain.xcal", "defect.xcal", "manifest.json"})
        EXPECT_TRUE(readAll(a / name) == readAll(b / name)) << name << " differs between two runs on the same frames";
    DefectXcal d;
    ASSERT_EQ(XPE_OK, read_defect_xcal((a / "defect.xcal").string().c_str(), false, &d));
    EXPECT_EQ(2u, d.format_version);
    ASSERT_TRUE(d.has_area);
    size_t outside = 0;
    for (uint8_t v : d.area) outside += v == 0;
    EXPECT_EQ(static_cast<size_t>(3) * kH + static_cast<size_t>(1) * kH + static_cast<size_t>(2 + 1) * (kW - 4) , outside) << "margins 3,2,1,1: three columns left, one right, two rows top, one bottom";
    EXPECT_EQ(0, d.area[100 * kW + 1]) << "inside the left margin";
    EXPECT_EQ(1, d.area[100 * kW + 3]) << "just inside the area";
    EXPECT_NE(0, d.map[100 * kW + 100]) << "the dead pixel of the bright frames";
    bool rowFound = false;
    for (const DefectExtLine& L : d.ext.lines) {
        if (L.orientation == XPE_BPM_LINE_ROW && L.index <= 121 && L.index + L.width > 120) {
            rowFound = true;
            EXPECT_FALSE(L.segments.empty());
            for (const DefectExtSegment& g : L.segments) {
                EXPECT_GE(g.start, L.start);
                EXPECT_LE(g.end, L.end);
                for (uint32_t x = g.start; x < g.end; ++x) EXPECT_NE(0, d.map[static_cast<size_t>(g.line) * kW + x]) << "a recorded stretch is painted in the defect plane";
            }
        }
    }
    EXPECT_TRUE(rowFound) << "the planted row defect is a line record (lines: " << d.ext.lines.size() << ")";
    EXPECT_EQ(0, d.map[120 * kW + 200]) << "row 120 is good there: the record's bounding range must not be painted as one stretch";
    EXPECT_FALSE(d.ext.blobs.empty());
    const std::string manifest = asText(readAll(a / "manifest.json"));
    EXPECT_NE(std::string::npos, manifest.find("\"defect_format\"")) << manifest;
    EXPECT_NE(std::string::npos, manifest.find("\"xcal_version\": 2"));
    EXPECT_NE(std::string::npos, manifest.find("\"margins_ltrb\": [3, 2, 1, 1]"));
    EXPECT_NE(std::string::npos, manifest.find("not evidence for a line pixel")) << "the limit of the pixel bit near a line is stated";
    EXPECT_NE(std::string::npos, manifest.find("diagnostic appendix"));
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map((a / "defect.xcal").string().c_str())) << "the runtime loader takes the version-2 file";
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

TEST_F(CalibFixtureGenTest, TheBpmV2OptionsBelongToTheFramesModeAndTakeFourMargins) {
    std::string err;
    EXPECT_NE(0, runFrames(root, caseDir("o"), " --bpm-v2", "o", &err));
    EXPECT_NE(std::string::npos, err.find("--bpm-v2")) << err;
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    EXPECT_NE(0, runFrames(root, caseDir("m"), framesArgs(set) + " --bpm-v2 --bpm-margins 1,2,3", "m", &err));
    EXPECT_NE(std::string::npos, err.find("L,T,R,B")) << err;
    EXPECT_TRUE(hasNoMaps(caseDir("m")));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesTooFewDefectMapFramesEvenWhenTheGainFramesAreEnough) {
    const FrameSet gain = writeFrameSet(root / "in", 6, 12);
    std::string args = framesArgs(gain);
    for (size_t i = 0; i < 7; ++i) args += " --bpm-bright \"" + gain.brights[i] + "\"";
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_EQ(10, runFrames(root, out, args, "x", &err));
    EXPECT_NE(std::string::npos, err.find("too few bright frames: 7")) << err;
    EXPECT_NE(std::string::npos, err.find("defect map")) << err;
    EXPECT_TRUE(hasNoMaps(out));
}

TEST_F(CalibFixtureGenTest, TheBpmBrightOptionsBelongToTheFramesMode) {
    const fs::path out = caseDir("out");
    std::string err;
    EXPECT_NE(0, runFrames(root, out, " --bpm-bright \"x.raw\"", "x", &err));
    EXPECT_NE(std::string::npos, err.find("--bpm-bright")) << err;
}

// ---------------------------------------------------------------- QA-A-250c: Codex #200 findings 2, 3 and 4
// Finding 2: the dose check sees the signal the gain generator sees (mean over pixels of max(0, bright - gain dark reference), the reference being
// --gain-dark-ref). Finding 3: a frame is given once per role. Finding 4: the set is built aside and moved into --out only when all four files exist.
class ScopedEnv {
public:
    ScopedEnv(const char* key, const char* value) : key_(key) {
#ifdef _WIN32
        _putenv_s(key, value);
#else
        setenv(key, value, 1);
#endif
    }
    ~ScopedEnv() {
#ifdef _WIN32
        _putenv_s(key_, "");
#else
        unsetenv(key_);
#endif
    }

private:
    const char* key_;
};

fs::path writeFlatReference(const fs::path& path, uint16_t value) {
    writeU16(path, std::vector<uint16_t>(static_cast<size_t>(kW) * kH, value));
    return path;
}

// Anything named *.building left next to the output is a leftover work directory.
bool hasWorkDirLeftover(const fs::path& root) {
    for (const auto& e : fs::recursive_directory_iterator(root))
        if (e.path().filename().string().find(".building") != std::string::npos) return true;
    return false;
}

// Codex #200 finding 2, first reproduction: dark mean 15000, bright means 35000 and 43000, --gain-dark-ref none. The generator sees 35000 and 43000
// (ratio 1.23): accepted. The old check subtracted the dark mean anyway (20000 and 28000, ratio 1.4) and refused with exit 12.
TEST_F(CalibFixtureGenTest, FramesModeDoseCheckUsesTheGeneratorsSignalWhenThereIsNoDarkReference) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12, {1.0, 1.4}, 15000);
    const fs::path none = root / "none", mean = root / "mean";
    ASSERT_EQ(0, runFrames(root, none, framesArgs(set) + " --gain-dark-ref none", "none"));
    const std::string manifest = asText(readAll(none / "manifest.json"));
    EXPECT_NE(std::string::npos, manifest.find("\"gain_dose_ratio\": 1.22")) << manifest;
    EXPECT_NE(std::string::npos, manifest.find("\"gain_dose_basis\": \"mean over pixels of max(0, bright - gain dark reference); reference: none\"")) << manifest;
    // the same frames with the default reference (the dark mean) have signals 20000 and 28000: refused
    std::string err;
    EXPECT_EQ(12, runFrames(root, mean, framesArgs(set), "mean", &err));
    EXPECT_NE(std::string::npos, err.find("ratio 1.4")) << err;
}

// Codex #200 finding 2, second reproduction: dark mean 1000, bright means 20000 and 24000, external reference mean 15000. The old check saw 19000 and 23000
// (ratio 1.21, accepted); the generator sees 5000 and 9000 (ratio 1.8).
TEST_F(CalibFixtureGenTest, FramesModeDoseCheckUsesTheExternalDarkReferenceOfTheGenerator) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12, {0.95, 1.15}, 1000);
    const fs::path ref = writeFlatReference(root / "ref15000.raw", 15000);
    std::string err;
    EXPECT_EQ(12, runFrames(root, root / "external", framesArgs(set) + " --gain-dark-ref \"" + ref.string() + "\"", "external", &err));
    EXPECT_NE(std::string::npos, err.find("ratio 1.79")) << err;
    EXPECT_NE(std::string::npos, err.find("ref15000.raw")) << "the message names the reference: " << err;
    EXPECT_FALSE(fs::exists(root / "external"));
    EXPECT_EQ(0, runFrames(root, root / "mean", framesArgs(set), "mean")) << "the same frames with the dark mean as reference are one dose";
}

// The generator floors every pixel at zero before it averages. A reference of 21500 is above the mean (21020) of the first frame: without the floor that
// frame would have no signal left ("no signal"); with it about 280 remain, against about 1555 for the 23020-mean frame (ratio 5.5, refused as a dose spread).
TEST_F(CalibFixtureGenTest, FramesModeDoseCheckFloorsEachPixelAtZeroLikeTheGenerator) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12, {1.0, 1.1}, 1000);
    const fs::path ref = writeFlatReference(root / "ref21500.raw", 21500);
    std::string err;
    EXPECT_EQ(12, runFrames(root, root / "out", framesArgs(set) + " --gain-dark-ref \"" + ref.string() + "\"", "x", &err));
    EXPECT_NE(std::string::npos, err.find("ratio")) << err;
    EXPECT_EQ(std::string::npos, err.find("no signal")) << "the floored signal is positive: " << err;
}

// Codex #200 finding 4 reproduction (the card names a reference of 30000; the frames here carry a 60000 hot pixel, so the reference is 65000 to leave
// nothing): a reference above every bright frame leaves no signal. The old tool failed in the gain stage, after offset.xcal was
// written, and left it behind. Now the frames are refused before anything is made, and --out does not exist afterwards.
TEST_F(CalibFixtureGenTest, FramesModeRefusesFramesWithNoSignalAfterTheReferenceAndLeavesNothing) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path ref = writeFlatReference(root / "ref65000.raw", 65000);
    const fs::path out = root / "deep" / "new" / "out";
    std::string err;
    EXPECT_EQ(12, runFrames(root, out, framesArgs(set) + " --gain-dark-ref \"" + ref.string() + "\"", "x", &err));
    EXPECT_NE(std::string::npos, err.find("no signal")) << err;
    EXPECT_FALSE(fs::exists(root / "deep")) << "a refusal creates no directory";
}

// Boundary on both sides of 1.25 for the references that matter: none (signal = frame mean) and the dark mean.
TEST_F(CalibFixtureGenTest, FramesModeDoseLimitBoundaryForEachReferenceKind) {
    const FrameSet below = writeFrameSet(root / "below", 6, 12, {1.0, 1.2}, 1000), above = writeFrameSet(root / "above", 6, 12, {1.0, 1.3}, 1000);
    EXPECT_EQ(0, runFrames(root, root / "none_below", framesArgs(below) + " --gain-dark-ref none", "a"));   // 21000 and 25000: 1.19
    EXPECT_EQ(12, runFrames(root, root / "none_above", framesArgs(above) + " --gain-dark-ref none", "b"));  // 21000 and 27000: 1.29
    EXPECT_EQ(0, runFrames(root, root / "mean_below", framesArgs(below), "c"));                              // 20000 and 24000: 1.2
    EXPECT_EQ(12, runFrames(root, root / "mean_above", framesArgs(above), "d"));                             // 20000 and 26000: 1.3
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesTheSameFileTwiceInOneRole) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    std::string err;
    const fs::path out = root / "out";

    // repeated option
    EXPECT_EQ(13, runFrames(root, out, framesArgs(set) + " --dark \"" + set.darks[0] + "\"", "opt", &err));
    EXPECT_NE(std::string::npos, err.find("duplicate dark frame")) << err;
    EXPECT_NE(std::string::npos, err.find("are the same file")) << err;

    // a list that names a frame twice
    std::string listed;
    for (const auto& b : set.brights) listed += b + "\n";
    listed += set.brights[3] + "\n";
    const fs::path list = root / "bright.lst";
    writeBytes(list, listed.data(), listed.size());
    std::string args = " --width " + std::to_string(kW) + " --height " + std::to_string(kH) + " --expiry-ms " + std::to_string(kFarExpiryMs) + " --bright-list \"" + list.string() + "\"";
    for (const auto& d : set.darks) args += " --dark \"" + d + "\"";
    EXPECT_EQ(13, runFrames(root, out, args, "list", &err));
    EXPECT_NE(std::string::npos, err.find("duplicate bright frame")) << err;

    // the same file under another spelling of its path
    const std::string alias = (root / "in" / ".." / "in" / "bright_0.raw").string();
    EXPECT_EQ(13, runFrames(root, out, framesArgs(set) + " --bright \"" + alias + "\"", "alias", &err));
    EXPECT_NE(std::string::npos, err.find("are the same file")) << err;

    // the defect map's own list
    std::string bpm = framesArgs(set);
    for (size_t i = 0; i < 10; ++i) bpm += " --bpm-bright \"" + set.brights[i] + "\"";
    bpm += " --bpm-bright \"" + set.brights[2] + "\"";
    EXPECT_EQ(13, runFrames(root, out, bpm, "bpm", &err));
    EXPECT_NE(std::string::npos, err.find("duplicate bpm_bright frame")) << err;

    EXPECT_FALSE(fs::exists(out));
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesTwoFilesWithTheSameContentInOneRole) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path copy = root / "in" / "bright_copy.raw";
    fs::copy_file(set.brights[4], copy);
    std::string err;
    EXPECT_EQ(13, runFrames(root, root / "out", framesArgs(set) + " --bright \"" + copy.string() + "\"", "x", &err));
    EXPECT_NE(std::string::npos, err.find("same content")) << err;
    EXPECT_FALSE(fs::exists(root / "out"));
    // a copy does not make up a missing frame: 9 distinct frames and a copy (10 paths) are refused, not counted as 10
    const FrameSet few = writeFrameSet(root / "few", 6, 9);
    const fs::path copy2 = root / "few" / "bright_copy.raw";
    fs::copy_file(few.brights[0], copy2);
    EXPECT_EQ(13, runFrames(root, root / "out2", framesArgs(few) + " --bright \"" + copy2.string() + "\"", "y", &err));
}

// A refusal makes no directory, however deep --out is.
TEST_F(CalibFixtureGenTest, FramesModeRefusalsLeaveNoOutputDirectory) {
    const FrameSet few = writeFrameSet(root / "few", 3, 12);   // too few darks
    const fs::path out = root / "deep" / "new" / "out";
    EXPECT_EQ(10, runFrames(root, out, framesArgs(few), "x"));
    EXPECT_FALSE(fs::exists(root / "deep"));
    const FrameSet ok = writeFrameSet(root / "in", 6, 12);
    EXPECT_EQ(9, runFrames(root, out, framesArgs(ok) + " --bright \"" + (root / "missing.raw").string() + "\"", "y"));
    EXPECT_FALSE(fs::exists(root / "deep"));
}

// ---------------------------------------------------------------- QA-A-250d: no overwrite, a work directory of the run's own, one rename
// --out must not exist or be an EMPTY directory (an empty one is accepted and is replaced by the finished set). A non-empty --out is refused with exit 14
// before anything is read or written. The set is made in <out>.building-<pid>-<ms> and published by one directory rename.
bool hasOwnWorkDir(const fs::path& root) {   // a work directory of the form *.building-<pid>-<ms>
    for (const auto& e : fs::recursive_directory_iterator(root))
        if (e.is_directory() && e.path().filename().string().find(".building-") != std::string::npos) return true;
    return false;
}

TEST_F(CalibFixtureGenTest, FramesModeRefusesANonEmptyOutAndLeavesItUntouched) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path out = root / "taken";
    ASSERT_EQ(0, runFrames(root, out, framesArgs(set), "first"));
    std::vector<std::vector<char>> before;
    for (const char* n : {"offset.xcal", "gain.xcal", "defect.xcal", "manifest.json"}) before.push_back(readAll(out / n));
    const FrameSet other = writeFrameSet(root / "other", 6, 12, {1.0}, 3000);   // other maps: a run that wrote would change the bytes
    std::string err;
    EXPECT_EQ(14, runFrames(root, out, framesArgs(other), "again", &err));
    EXPECT_NE(std::string::npos, err.find("is not empty")) << err;
    size_t i = 0;
    for (const char* n : {"offset.xcal", "gain.xcal", "defect.xcal", "manifest.json"}) EXPECT_TRUE(before[i++] == readAll(out / n)) << n << " changed";
    EXPECT_FALSE(hasWorkDirLeftover(root));

    // not only a finished set: any file in --out makes it non-empty
    const fs::path stray = caseDir("stray");
    writeBytes(stray / "notes.txt", "x", 1);
    EXPECT_EQ(14, runFrames(root, stray, framesArgs(set), "stray"));
    EXPECT_TRUE(fs::exists(stray / "notes.txt"));
    EXPECT_TRUE(hasNoMaps(stray));
}

TEST_F(CalibFixtureGenTest, FramesModeAcceptsAnEmptyOutDirectory) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path out = caseDir("empty");   // exists, empty
    ASSERT_EQ(0, runFrames(root, out, framesArgs(set), "x"));
    for (const char* n : {"offset.xcal", "gain.xcal", "defect.xcal", "manifest.json"}) EXPECT_TRUE(fs::exists(out / n)) << n;
    EXPECT_FALSE(hasWorkDirLeftover(root));
}

// Each stage is made to fail in turn (the seam XPE_FIXTURE_GEN_FAIL): --out is not created, nor its parents, and the run's own work directory is gone.
// The publish stage is the one rename; its failure is a failure (exit 8), not a success. An empty --out that was there is still there.
TEST_F(CalibFixtureGenTest, FramesModeFailureInAnyStageLeavesNothingBehind) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    struct Stage { const char* name; int code; };
    for (const Stage& st : {Stage{"gain", 6}, Stage{"bpm", 7}, Stage{"manifest", 8}, Stage{"commit", 8}}) {
        const fs::path fresh = root / (std::string("fresh_") + st.name) / "out";
        {
            ScopedEnv fail("XPE_FIXTURE_GEN_FAIL", st.name);
            EXPECT_EQ(st.code, runFrames(root, fresh, framesArgs(set), std::string("f_") + st.name)) << st.name;
        }
        EXPECT_FALSE(fs::exists(fresh)) << st.name << ": --out was created";
        EXPECT_FALSE(fs::exists(fresh.parent_path())) << st.name << ": a parent directory was left";

        const fs::path empty = caseDir((std::string("empty_") + st.name).c_str());
        {
            ScopedEnv fail("XPE_FIXTURE_GEN_FAIL", st.name);
            EXPECT_EQ(st.code, runFrames(root, empty, framesArgs(set), std::string("e_") + st.name)) << st.name;
        }
        EXPECT_TRUE(fs::is_directory(empty)) << st.name << ": the empty --out is gone";
        EXPECT_TRUE(fs::is_empty(empty)) << st.name << ": the empty --out is not empty any more";
    }
    EXPECT_FALSE(hasOwnWorkDir(root));
}

// A work directory of another run (a killed one, or one on the same --out right now) is not this run's to touch, whatever its name: an old-style fixed
// name, a name with another pid, and the run's own pattern with a foreign suffix.
TEST_F(CalibFixtureGenTest, FramesModeDoesNotTouchAnotherRunsWorkDirectory) {
    const FrameSet set = writeFrameSet(root / "in", 6, 12);
    const fs::path out = root / "out";
    std::vector<fs::path> foreign;
    for (const char* name : {"out.building", "out.building-1-2", "out.building-99999-0"}) {
        const fs::path d = root / name;
        fs::create_directories(d);
        writeBytes(d / "old_offset.xcal", "precious", 8);
        foreign.push_back(d);
    }
    ASSERT_EQ(0, runFrames(root, out, framesArgs(set), "ok"));
    {
        ScopedEnv fail("XPE_FIXTURE_GEN_FAIL", "gain");
        EXPECT_EQ(6, runFrames(root, root / "out2", framesArgs(set), "fail"));
    }
    for (const fs::path& d : foreign) {
        ASSERT_TRUE(fs::exists(d / "old_offset.xcal")) << d << " was removed";
        EXPECT_EQ(std::string("precious"), asText(readAll(d / "old_offset.xcal")));
    }
    EXPECT_TRUE(fs::exists(out / "offset.xcal"));
}

}  // namespace
