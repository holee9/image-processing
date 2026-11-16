/**
 * @file test_fixture_gen_gain_poly.cpp
 * @brief The fixture generator writes the scalar set only; its --gain-poly option is gone (QA-A-336).
 *
 * QA-A-136 (#198) added --gain-poly, which wrote gain_poly.xcal (XCAL type 3) so the GUI lane could reproduce the
 * polynomial alerts. The gain polynomial was removed by user decision (QA-A-336) and the option with it: the tool now
 * refuses it as an unknown argument and writes nothing, and the default run still writes the scalar gain type.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "preprocess_state_fixture.h"
#include "xpe_fixture_gen_path.h"  // generated: XPE_FIXTURE_GEN_EXE

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 32;
constexpr uint32_t kH = 32;

uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

/** Runs the generator into @p dir with @p extra appended to the arguments. */
int runGenerator(const fs::path& dir, const std::string& extra) {
    std::string cmd;
    cmd += "\"";
    cmd += XPE_FIXTURE_GEN_EXE;
    cmd += "\" --out \"" + dir.string() + "\"";
    cmd += " --width " + std::to_string(kW);
    cmd += " --height " + std::to_string(kH);
    cmd += " --seed 7";
    if (!extra.empty()) cmd += " " + extra;
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";
#endif
    return std::system(cmd.c_str());
}

/** Reads the XCal header's type field (offset 8, uint32 LE). */
uint32_t typeByteOf(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return 0xFFFFFFFFu;
    char buf[12]{};
    f.read(buf, sizeof(buf));
    if (f.gcount() < static_cast<std::streamsize>(sizeof(buf))) return 0xFFFFFFFFu;
    uint32_t t = 0;
    std::memcpy(&t, buf + 8, sizeof(t));
    return t;
}

class FixtureGenGainPolyTest : public XpePreprocessStateFixture {
protected:
    fs::path root;

    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        // The alert queue is a common-module global (QA-A-137, #198): drained through the public call.
        xpe_clear_alerts();
        root = fs::temp_directory_path() /
               ("xpe_fixpoly_" + std::to_string(nowMs()) + "_" +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::create_directories(root);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root, ec);
        xpe_clear_alerts();  // restore only what this test raised
        XpePreprocessStateFixture::TearDown();
    }
};

// CONTROL: the default run produces the scalar type and no polynomial file.
TEST_F(FixtureGenGainPolyTest, DefaultRunStaysScalarAndWritesNoPolyFile) {
    ASSERT_EQ(0, runGenerator(root, ""));
    EXPECT_FALSE(fs::exists(root / "gain_poly.xcal"));
    EXPECT_EQ(static_cast<uint32_t>(XCAL_TYPE_GAIN), typeByteOf(root / "gain.xcal"));
}

// QA-A-336: --gain-poly (and its --poly-degree / --poly-levels) is an unknown argument now -- the run fails and writes
// no file at all, so a script that still asks for a polynomial fixture learns it instead of getting a scalar set.
TEST_F(FixtureGenGainPolyTest, TheGainPolyOptionIsRefusedAndNothingIsWritten) {
    const char* const extras[] = {"--gain-poly", "--poly-degree 2", "--poly-levels 4"};
    for (size_t k = 0; k < 3; ++k) {
        const char* const extra = extras[k];
        const fs::path dir = root / ("run_" + std::to_string(k));
        fs::create_directories(dir);
        EXPECT_NE(0, runGenerator(dir, extra)) << extra;
        EXPECT_TRUE(fs::is_empty(dir)) << extra << ": the refused run wrote files";
    }
}

}  // namespace
