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
#include <filesystem>
#include <fstream>
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
    img.dataSize      = static_cast<uint32_t>(static_cast<size_t>(kW) * kH * sizeof(uint16_t));

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

}  // namespace
