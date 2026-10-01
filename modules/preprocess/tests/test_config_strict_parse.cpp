/**
 * @file test_config_strict_parse.cpp
 * @brief A malformed number in a configuration JSON is XPE_ERR_CONFIG_INVALID, not an exception (QA-A-202, #233).
 *
 * The pipeline entry points and xpe_ghost_create used std::stof / std::stoi / std::stod on values taken from
 * the caller's configuration JSON. A value such as "abc" or "1e999" made them throw out of a C ABI function
 * (QA-A-201 measured it for every one of these inputs). The conversion is now strict -- the whole value must be
 * a finite number in range -- and a failure is XPE_ERR_CONFIG_INVALID with nothing changed: no image, no
 * metadata, no loaded calibration, no handle.
 *
 * The calls go through std::function on purpose: MSVC /EHsc treats an extern "C" call as non-throwing, so a
 * try/catch around a direct call does not reliably see the exception it is meant to observe.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t N = static_cast<size_t>(W) * H;

XpeImageBuffer buf(void* d, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}

// Every stage bypassed: only the configuration decides the outcome.
const std::string kAllBypass =
    "\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassGain\":true,\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true";

std::string cfg(const std::string& extra) {
    return "{" + kAllBypass + (extra.empty() ? "" : "," + extra) + "}";
}

/** Runs `f`; an exception that reaches here is reported through `threw` instead of ending the test. */
XpeErrorCode callSafely(const std::function<XpeErrorCode()>& f, bool* threw) {
    *threw = false;
    try {
        return f();
    } catch (...) {
        *threw = true;
        return XPE_OK;
    }
}

const char* kBadNumbers[] = {
    "\"detectorTempC\":\"abc\"",                        // not a number
    "\"binningMode\":\"x\"",                            // not a number
    "\"detectorTempC\":\"1e999\"",                      // out of range for any float
    "\"binningMode\":\"99999999999999999999\"",         // out of range for int
    "\"detectorTempC\":\"25.5x\"",                      // a number followed by garbage
    "\"binningMode\":\"2x\"",                           // an integer followed by garbage
};

void writeFile(const fs::path& path, uint32_t type, uint32_t fmt, const void* data, size_t bytes) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = W; hdr.height = H; hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path.string().c_str(), hdr, reinterpret_cast<const uint8_t*>("{}"), 2,
                                      static_cast<const uint8_t*>(data), bytes));
}

class ConfigStrictParse : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all("csp_calib", ec);
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }
};

}  // namespace

// ---- the pipeline entry points --------------------------------------------------------------------

TEST_F(ConfigStrictParse, PipelineExRefusesAMalformedNumberAndChangesNothing) {
    for (const char* bad : kBadNumbers) {
        SCOPED_TRACE(bad);
        std::vector<uint16_t> pixels(N, 1000);
        XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata meta{};
        const std::string c = cfg(bad);
        bool threw = false;
        const XpeErrorCode rc = callSafely([&] {
            return xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, c.c_str());
        }, &threw);
        EXPECT_FALSE(threw) << "an exception left xpe_preprocess_pipeline_ex";
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
        EXPECT_EQ(0u, meta.flags) << "metadata must be untouched";
        for (size_t i = 0; i < N; ++i) ASSERT_EQ(1000u, pixels[i]) << "pixel " << i << " changed";
    }
}

TEST_F(ConfigStrictParse, PipelineExAcceptsOrdinaryNumbers) {
    // Control: the same call shape with well-formed values still succeeds, so the refusals above are
    // about the value and not about the call.
    const char* good[] = {
        "\"detectorTempC\":\"25.5\",\"binningMode\":\"1\"",
        "\"detectorTempC\":-5.5,\"binningMode\":2",
        "\"detectorTempC\":\"0\",\"binningMode\":\"4\"",
        "\"detectorTempC\":\"3.0e1\"",
        "\"binningMode\":\"-1\"",
    };
    for (const char* g : good) {
        SCOPED_TRACE(g);
        std::vector<uint16_t> pixels(N, 1000);
        XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata meta{};
        const std::string c = cfg(g);
        bool threw = false;
        const XpeErrorCode rc = callSafely([&] {
            return xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, c.c_str());
        }, &threw);
        EXPECT_FALSE(threw);
        EXPECT_EQ(XPE_OK, rc);
    }
}

TEST_F(ConfigStrictParse, PipelineWithAMalformedConfigLoadsNoCalibration) {
    std::vector<float> off(N, 100.0f), gain(N, 2.0f);
    std::vector<uint8_t> def(N, 0);
    fs::create_directories("csp_calib");
    writeFile("csp_calib/offset.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, off.data(), off.size() * 4);
    writeFile("csp_calib/gain.xcal", XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, gain.data(), gain.size() * 4);
    writeFile("csp_calib/defect.xcal", XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, def.data(), def.size());

    auto offsetLoaded = [] {
        std::vector<uint16_t> in(N, 1000), out(N, 0);
        XpeImageBuffer i = buf(in.data(), XPE_PIXEL_UINT16, 16), o = buf(out.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata m{};
        return xpe_offset_correct(&i, &o, &m) == XPE_OK;
    };
    ASSERT_FALSE(offsetLoaded()) << "precondition: no calibration is loaded";

    std::vector<uint16_t> pixels(N, 1000);
    XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
    XpeImageMetadata meta{};
    const std::string bad = cfg("\"detectorTempC\":\"abc\"");
    bool threw = false;
    XpeErrorCode rc = callSafely([&] {
        return xpe_preprocess_pipeline(&img, &meta, "csp_calib", nullptr, bad.c_str());
    }, &threw);
    EXPECT_FALSE(threw);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
    EXPECT_FALSE(offsetLoaded()) << "a refused configuration must not have loaded the calibration files";

    // Control: with a good configuration the same call does load them.
    const std::string good = cfg("\"detectorTempC\":\"25\"");
    rc = callSafely([&] { return xpe_preprocess_pipeline(&img, &meta, "csp_calib", nullptr, good.c_str()); }, &threw);
    EXPECT_FALSE(threw);
    EXPECT_EQ(XPE_OK, rc);
    EXPECT_TRUE(offsetLoaded()) << "control: a good configuration loads the calibration files";
}

TEST_F(ConfigStrictParse, PipelineBatchRefusesAMalformedNumberAndLeavesEveryImageUntouched) {
    for (const char* bad : kBadNumbers) {
        SCOPED_TRACE(bad);
        std::vector<uint16_t> p0(N, 1000), p1(N, 1000);
        XpeImageBuffer imgs[2] = {buf(p0.data(), XPE_PIXEL_UINT16, 16), buf(p1.data(), XPE_PIXEL_UINT16, 16)};
        XpeImageMetadata metas[2] = {};
        const std::string c = cfg(bad);
        bool threw = false;
        const XpeErrorCode rc = callSafely([&] {
            return xpe_preprocess_pipeline_batch(imgs, 2, metas, nullptr, nullptr, c.c_str());
        }, &threw);
        EXPECT_FALSE(threw);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
        EXPECT_EQ(0u, metas[0].flags);
        EXPECT_EQ(0u, metas[1].flags);
        for (size_t i = 0; i < N; ++i) { ASSERT_EQ(1000u, p0[i]); ASSERT_EQ(1000u, p1[i]); }
    }
}

// ---- xpe_ghost_create ------------------------------------------------------------------------------

TEST_F(ConfigStrictParse, GhostCreateRefusesAMalformedNumberAndHandsBackNoHandle) {
    const char* bad[] = {
        "{\"alpha1\":\"abc\"}",
        "{\"tier\":\"x\"}",
        "{\"tau2\":\"1e999\"}",
        "{\"tier\":\"99999999999999999999\"}",            // out of range for int
        "{\"tier2Threshold\":\"1e999\"}",
        "{\"nlcscBeta\":\"1.5.2\"}",
        "{\"alpha1\":\"0.5x\"}",
        "{\"tier\":\"2x\"}",
    };
    for (const char* c : bad) {
        SCOPED_TRACE(c);
        void* handle = nullptr;
        bool threw = false;
        const XpeErrorCode rc = callSafely([&] { return xpe_ghost_create(W, H, c, &handle); }, &threw);
        EXPECT_FALSE(threw) << "an exception left xpe_ghost_create";
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
        EXPECT_EQ(nullptr, handle) << "no handle may be handed back on a refusal";
    }
}

TEST_F(ConfigStrictParse, GhostCreateAcceptsOrdinaryNumbers) {
    const char* good =
        "{\"tier\":\"2\",\"alpha1\":\"0.5\",\"tau1\":1.5,\"alpha2\":\"0.25\",\"tau2\":\"10\","
        "\"tier2Threshold\":\"3000\",\"nlcscBeta\":\"0.1\"}";
    void* handle = nullptr;
    bool threw = false;
    const XpeErrorCode rc = callSafely([&] { return xpe_ghost_create(W, H, good, &handle); }, &threw);
    EXPECT_FALSE(threw);
    EXPECT_EQ(XPE_OK, rc);
    ASSERT_NE(nullptr, handle);
    xpe_ghost_destroy(handle);
}
