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

}  // namespace
