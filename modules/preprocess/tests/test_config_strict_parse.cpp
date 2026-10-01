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

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
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
        std::remove("csp_gq.xcal");
        std::remove("csp_gx.xcal");
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

// ---- the accepted notation: a boundary table (QA-A-202b, Codex #23) -----------------------------------
//
// Leading white space and a single leading '+' were accepted by the std::stof / stoi / stod these
// conversions replaced, so they stay accepted. What does not stay is anything after the number: the whole
// value must be consumed. The rule, one paragraph: a configuration number is optional leading white space
// (space, tab, newline, vertical tab, form feed, carriage return), then an optional single '+' that is not
// followed by another sign, then a decimal number that fills the rest of the value -- an integer for an
// integer field, a finite number (decimal point and exponent allowed) for a real field; "nan", "inf",
// hexadecimal, a trailing character (including a trailing space) and an out-of-range value are refused.
// An empty value is an absent one and keeps the default.

namespace {

struct NumberRow { const char* value; bool accepted; };

const NumberRow kRealRows[] = {
    {" +25", true},  {"+2", true},   {" 0.5", true}, {"1e2", true},  {"-5.5", true}, {"	3", true},
    {"25 ", false},  {"+-1", false}, {"++1", false}, {"+ 1", false}, {"nan", false}, {"inf", false},
    {"-inf", false}, {"1e999", false}, {"0x10", false}, {"1,5", false}, {"1.5.2", false},
};
const NumberRow kIntRows[] = {
    {" +2", true},   {"+2", true},   {" 4", true},   {"-1", true},   {"	2", true},
    {"1e2", false},  {"2 ", false},  {"+-1", false}, {"++1", false}, {"+ 1", false}, {"2.0", false},
    {"nan", false},  {"inf", false}, {"99999999999999999999", false}, {"0x4", false}, {"1e", false},
};

/** `v` as the body of a JSON string literal: the quote, the backslash and every control character escaped. A raw
 *  tab inside a string is not JSON (QA-A-209: the configuration is parsed now); the value the number conversion
 *  sees is the same -- the parser undoes the escapes. */
std::string jsonStringBody(const std::string& v) {
    std::string out;
    for (const char ch : v) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (ch == '"') out += std::string(1, static_cast<char>(92)) + '"';
        else if (ch == static_cast<char>(92)) out += std::string(2, static_cast<char>(92));
        else if (u < 0x20) {
            char buf6[8];
            std::snprintf(buf6, sizeof(buf6), "\\u%04x", u);
            out += buf6;
        } else out += ch;
    }
    return out;
}

using EntryCall = std::function<XpeErrorCode(const std::string& field, const std::string& value)>;

void runTable(const char* entry, const char* realField, const char* intField, const EntryCall& call) {
    for (const NumberRow& r : kRealRows) {
        SCOPED_TRACE(std::string(entry) + " " + realField + " = \"" + r.value + "\"");
        bool threw = false;
        const XpeErrorCode rc = callSafely([&] { return call(realField, r.value); }, &threw);
        EXPECT_FALSE(threw);
        EXPECT_EQ(r.accepted ? XPE_OK : XPE_ERR_CONFIG_INVALID, rc);
    }
    for (const NumberRow& r : kIntRows) {
        SCOPED_TRACE(std::string(entry) + " " + intField + " = \"" + r.value + "\"");
        bool threw = false;
        const XpeErrorCode rc = callSafely([&] { return call(intField, r.value); }, &threw);
        EXPECT_FALSE(threw);
        EXPECT_EQ(r.accepted ? XPE_OK : XPE_ERR_CONFIG_INVALID, rc);
    }
}

std::string pipelineConfig(const std::string& field, const std::string& value) {
    return cfg("\"" + field + "\":\"" + jsonStringBody(value) + "\"");
}

}  // namespace

TEST_F(ConfigStrictParse, TheAcceptedNotationOfThePipelineEntryPointsIsFixedByATable) {
    runTable("pipeline_ex", "detectorTempC", "binningMode", [](const std::string& f, const std::string& v) {
        std::vector<uint16_t> pixels(N, 1000);
        XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata meta{};
        const std::string c = pipelineConfig(f, v);
        return xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, c.c_str());
    });
    runTable("pipeline", "detectorTempC", "binningMode", [](const std::string& f, const std::string& v) {
        std::vector<uint16_t> pixels(N, 1000);
        XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata meta{};
        const std::string c = pipelineConfig(f, v);
        return xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, c.c_str());
    });
    runTable("pipeline_batch", "detectorTempC", "binningMode", [](const std::string& f, const std::string& v) {
        std::vector<uint16_t> pixels(N, 1000);
        XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata meta{};
        const std::string c = pipelineConfig(f, v);
        return xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, c.c_str());
    });
}

TEST_F(ConfigStrictParse, TheAcceptedNotationOfGhostCreateIsFixedByATable) {
    runTable("xpe_ghost_create", "alpha1", "tier", [](const std::string& f, const std::string& v) {
        void* handle = nullptr;
        const std::string c = "{\"" + f + "\":\"" + jsonStringBody(v) + "\"}";
        const XpeErrorCode rc = xpe_ghost_create(W, H, c.c_str(), &handle);
        if (rc == XPE_OK) xpe_ghost_destroy(handle);
        return rc;
    });
}

// ---- the quality metadata a gain file carries (QA-A-204, #233 item 5) ----------------------------------
//
// A gain file's config block may carry fit_r_squared, polynomial_degree, actual_dose_levels and
// calibration_mode (FUNC-033). They were read with atof / atoi, which turn a malformed value into 0 without
// a word -- a file claiming "abc" for its degree loaded as a degree-0 calibration. The same strict
// conversion as the pipeline configuration now reads them: a field that is present and is not a number in
// range is XPE_ERR_CONFIG_INVALID, and the load changes nothing (the parse happens before the commit).

namespace {

void writeGainWithConfig(const char* path, float v, const std::string& json) {
    std::vector<float> m(N, v);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = XCAL_TYPE_GAIN; hdr.pixel_format = XCAL_FMT_FLOAT32;
    hdr.width = W; hdr.height = H; hdr.payload_len = m.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      reinterpret_cast<const uint8_t*>(m.data()), m.size() * sizeof(float)));
}

/** The first pixel of a gain correction of a frame of 1000 -- 1000 / gain. */
float gainResult() {
    std::vector<uint16_t> in(N, 1000);
    std::vector<float> out(N, -1.0f);
    XpeImageBuffer i = buf(in.data(), XPE_PIXEL_UINT16, 16), o = buf(out.data(), XPE_PIXEL_FLOAT32, 32);
    XpeImageMetadata meta{};
    return xpe_gain_correct(&i, &o, &meta) == XPE_OK ? out[0] : -1.0f;
}

std::string qualityJson(const std::string& field, const std::string& value) {
    return "{\"" + field + "\":\"" + value + "\"}";
}

}  // namespace

TEST_F(ConfigStrictParse, AMalformedQualityFieldInAGainFileRefusesTheLoadAndLeavesTheStore) {
    // The store holds a good gain map (gain 4) with its metadata.
    writeGainWithConfig("csp_gq.xcal", 4.0f,
        "{\"fit_r_squared\":\"0.5\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
    XpeCalibQualityMeta base{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));
    ASSERT_NEAR(250.0f, gainResult(), 0.01f) << "control: the good map is in the store";

    const std::pair<const char*, const char*> bad[] = {
        {"fit_r_squared", "abc"},       {"fit_r_squared", "1e999"},  {"fit_r_squared", "0.9x"},
        {"fit_r_squared", "nan"},       {"polynomial_degree", "x"},  {"polynomial_degree", "2x"},
        {"polynomial_degree", "256"},   {"polynomial_degree", "-1"}, {"actual_dose_levels", "abc"},
        {"actual_dose_levels", "1e2"},  {"calibration_mode", "abc"}, {"calibration_mode", "3 "},
    };
    for (const auto& b : bad) {
        SCOPED_TRACE(std::string(b.first) + " = \"" + b.second + "\"");
        writeGainWithConfig("csp_gx.xcal", 2.0f, qualityJson(b.first, b.second));
        bool threw = false;
        const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
        EXPECT_FALSE(threw);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);

        XpeCalibQualityMeta after{};
        ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
        EXPECT_EQ(base.r_squared, after.r_squared) << "the metadata must be as it was";
        EXPECT_EQ(base.polynomial_degree, after.polynomial_degree);
        EXPECT_EQ(base.num_points, after.num_points);
        EXPECT_EQ(base.calibration_mode, after.calibration_mode);
        EXPECT_NEAR(250.0f, gainResult(), 0.01f) << "the refused file must not have replaced the gain map";
    }
}

// QA-A-205b (Codex #29 B1): "the key is there" and "the key has a value" are different facts. The extractor
// returned the same empty string for both, so {"fit_r_squared":""} -- a field that is present and says nothing --
// read as "not given" and the load went through. In a calibration file a present quality field must be a number
// (the generator writes all four or none); only a missing key means "not given". The pipeline CONFIGURATION keeps
// its own rule, "an empty value is an absent one" (202b): the GUI sends an unset option as an empty string, while
// an XCal file is signed data a generator produced, where an empty field is a defect, not an unset option.
TEST_F(ConfigStrictParse, AQualityFieldThatIsPresentButEmptyOrNotAScalarRefusesTheLoadAndLeavesTheStore) {
    writeGainWithConfig("csp_gq.xcal", 4.0f,
        "{\"fit_r_squared\":\"0.5\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
    XpeCalibQualityMeta base{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));
    ASSERT_NEAR(250.0f, gainResult(), 0.01f) << "control: the good map is in the store";

    const char* const bad[] = {
        // quoted but empty, for each of the four fields
        "{\"fit_r_squared\":\"\"}",         "{\"polynomial_degree\":\"\"}",
        "{\"actual_dose_levels\":\"\"}",    "{\"calibration_mode\":\"\"}",
        // present beside valid ones: one bad field refuses the file
        "{\"fit_r_squared\":\"0.9\",\"polynomial_degree\":\"\"}",
        // key with no value at all
        "{\"polynomial_degree\":,\"fit_r_squared\":\"0.9\"}", "{\"fit_r_squared\":}",
        // a value that is not a scalar
        "{\"fit_r_squared\":{}}",           "{\"polynomial_degree\":[1]}",
        "{\"calibration_mode\":{\"a\":1}}", "{\"actual_dose_levels\":[]}",
        // an unterminated string
        "{\"fit_r_squared\":\"0.9",
    };
    for (const char* json : bad) {
        SCOPED_TRACE(json);
        writeGainWithConfig("csp_gx.xcal", 2.0f, json);
        bool threw = false;
        const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
        EXPECT_FALSE(threw);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);

        XpeCalibQualityMeta after{};
        ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
        EXPECT_EQ(base.r_squared, after.r_squared) << "the metadata must be as it was";
        EXPECT_EQ(base.polynomial_degree, after.polynomial_degree);
        EXPECT_EQ(base.num_points, after.num_points);
        EXPECT_EQ(base.calibration_mode, after.calibration_mode);
        EXPECT_NEAR(250.0f, gainResult(), 0.01f) << "the refused file must not have replaced the gain map";
    }
}

TEST_F(ConfigStrictParse, AQualityFieldThatIsAbsentIsStillNotGivenAndThePipelineConfigKeepsItsEmptyValueRule) {
    // Control 1: no quality keys at all (a file from before QA-A-35) loads, and the metadata is "none".
    writeGainWithConfig("csp_gx.xcal", 2.0f, "{}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gx.xcal"));
    EXPECT_NEAR(500.0f, gainResult(), 0.01f);
    // Control 2: a file with only some of the keys, each a number, loads.
    writeGainWithConfig("csp_gx.xcal", 4.0f, "{\"polynomial_degree\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gx.xcal"));
    XpeCalibQualityMeta m{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&m));
    EXPECT_EQ(2u, m.polynomial_degree);
    // The pipeline configuration is the other rule: an empty value is the default, through the same entry points.
    std::vector<uint16_t> data(N, 1000);
    XpeImageBuffer img = buf(data.data(), XPE_PIXEL_UINT16, 16);
    XpeImageMetadata meta{};
    const char* cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
                      "\"bypassGain\":true,\"bypassBinning\":true,\"bypassDefect\":true,\"bypassGhost\":true,"
                      "\"detectorTempC\":\"\",\"binningMode\":\"\"}";
    EXPECT_EQ(XPE_OK, xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, cfg))
        << "an empty configuration value is an absent one (QA-A-202b), unchanged by QA-A-205b";
}

// QA-A-208 (Codex #34 B2): a gain file's quality fields are the TOP-LEVEL keys of its config object. They were
// found with strstr, which reads the first occurrence of the key anywhere in the text -- inside a nested object,
// inside a string value -- and so could read a nested 0.9 and pass over a malformed top-level value. The config is
// now walked structurally (strings, nested objects and arrays are skipped), and a top-level key given twice is
// refused. The pipeline CONFIGURATION is read by a different function (xpe_json_get_string) and is NOT changed
// here: it has the same first-occurrence limit.
namespace {

struct TopRow { const char* what; const char* json; bool refused; bool read; };

/** The four fields, each substituted for FIELD in a row's template. */
/** The numbers the rows stand in with, per field, each inside the range that field has (QA-A-208c) and different from
 *  the store's 0.5 / 1 / 3 / 2 so that a value read shows as a change: `$` is the good value, `%` another good one. */
std::string goodValue(const std::string& field) {
    return field == "fit_r_squared" ? "0.75" : field == "polynomial_degree" ? "3" : field == "actual_dose_levels" ? "7" : "4";
}
std::string otherValue(const std::string& field) {
    return field == "fit_r_squared" ? "0.25" : field == "polynomial_degree" ? "2" : field == "actual_dose_levels" ? "5" : "1";
}

std::string withField(const char* tmpl, const char* field) {
    std::string out;
    for (const char* p = tmpl; *p; ++p) {
        if (*p == '@') out += field;
        else if (*p == '$') out += goodValue(field);
        else if (*p == '%') out += otherValue(field);
        else out += *p;
    }
    return out;
}

}  // namespace

TEST_F(ConfigStrictParse, AQualityFieldIsTakenFromTheTopLevelOfTheConfigOnly) {
    // The store holds a gain file with known quality; each row loads another file over it.
    writeGainWithConfig("csp_gq.xcal", 4.0f,
        "{\"fit_r_squared\":\"0.5\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
    XpeCalibQualityMeta base{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));

    // '@' stands for the field's name, '$' for a good value of that field (see goodValue: inside the field's range, and different from the
    // store's 0.5 / 1 / 3 / 2, so a read value shows as a change); a bad one is "bad".
    const TopRow rows[] = {
        {"only inside a nested object: not given (the nested value is not read)",
         "{\"nested\":{\"@\":$},\"other\":2}", false, false},
        {"only inside a nested object, malformed: not given (the nested value does not poison the load)",
         "{\"nested\":{\"@\":\"bad\"}}", false, false},
        {"valid in a nested object, malformed at the top level: refused",
         "{\"nested\":{\"@\":$},\"@\":\"bad\"}", true, false},
        {"malformed at the top level, valid nested after: refused",
         "{\"@\":\"bad\",\"nested\":{\"@\":$}}", true, false},
        {"in a nested array of objects only: not given",
         "{\"list\":[{\"@\":$},{\"@\":%}]}", false, false},
        {"the top-level key given twice, both valid: refused",
         "{\"@\":$,\"@\":$}", true, false},
        {"the top-level key given twice, the first valid: refused",
         "{\"@\":$,\"x\":0,\"@\":\"bad\"}", true, false},
        {"the top-level key given twice, the second valid: refused",
         "{\"@\":\"bad\",\"@\":$}", true, false},
        // In "see \"@" the string's own closing quote follows the key name, so the raw text contains "@" with a
        // quote on each side -- which is all a first-occurrence search looks for.
        {"the key's name at the end of a string value: not a key",
         "{\"note\":\"see \\\"@\"}", false, false},
        {"the key's name at the end of a string value, then the real key: the real key is read",
         "{\"note\":\"see \\\"@\",\"@\":$}", false, true},
        {"braces and quotes inside a string value are skipped, not counted",
         "{\"note\":\"} { [ \\\" ]\",\"@\":$}", false, true},
        {"the real key after a nested object with the same key: the top-level value is the one read",
         "{\"nested\":{\"@\":\"bad\"},\"@\":$}", false, true},
    };
    const char* fields[] = {"fit_r_squared", "polynomial_degree", "actual_dose_levels", "calibration_mode"};

    for (const char* field : fields) {
        for (const auto& row : rows) {
            const std::string json = withField(row.json, field);
            SCOPED_TRACE(std::string(field) + ": " + row.what + "  " + json);
            writeGainWithConfig("csp_gx.xcal", 2.0f, json);
            bool threw = false;
            const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
            EXPECT_FALSE(threw);
            XpeCalibQualityMeta after{};
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
            if (row.refused) {
                EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
                EXPECT_EQ(base.r_squared, after.r_squared) << "a refused file must leave the metadata as it was";
                EXPECT_EQ(base.polynomial_degree, after.polynomial_degree);
                EXPECT_EQ(base.num_points, after.num_points);
                EXPECT_EQ(base.calibration_mode, after.calibration_mode);
                EXPECT_NEAR(250.0f, gainResult(), 0.01f) << "a refused file must not have replaced the gain map";
            } else {
                ASSERT_EQ(XPE_OK, rc);
                EXPECT_NEAR(500.0f, gainResult(), 0.01f) << "the file loaded: its gain map (2) is in the store";
                // A file with no top-level quality key carries no quality: the record becomes "none" (valid = 0,
                // every field zero -- QA-A-202e), which is also what shows that a nested value was NOT read.
                if (row.read) {
                    EXPECT_EQ(1u, after.valid) << "the top-level value was not read";
                    const double got = (std::string(field) == "fit_r_squared")      ? after.r_squared
                                     : (std::string(field) == "polynomial_degree")  ? after.polynomial_degree
                                     : (std::string(field) == "actual_dose_levels") ? after.num_points
                                                                                    : after.calibration_mode;
                    EXPECT_DOUBLE_EQ(std::stod(goodValue(field)), got) << "the value read is the top-level one";
                } else {
                    EXPECT_EQ(0u, after.valid) << "a value that is not a top-level key was read";
                    EXPECT_DOUBLE_EQ(0.0, after.r_squared);
                    EXPECT_EQ(0u, after.polynomial_degree);
                    EXPECT_EQ(0u, after.num_points);
                    EXPECT_EQ(0u, after.calibration_mode);
                }
            }
            // Put set "base" back for the next row.
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));
        }
    }
}

// QA-A-208b (Codex #40): the quality fields are looked up in a JSON text that has been PARSED -- by nlohmann-json,
// the parser the repository already carries (third_party/common/vcpkg.json) -- not by a hand-written scanner. The
// scanner compared a key's raw bytes (so "fit\u005fr_squared", the same key written with an escape, was another
// key and a duplicate of it went unnoticed) and checked no grammar: bracket kinds, bare tokens, text after the
// closing brace, forbidden escapes and control characters all passed. Now the text must be one valid JSON object;
// keys are compared after the escapes are interpreted; a top-level key given twice is refused.
namespace {

/** The field's name with its first underscore written as the JSON escape \u005f: the same key, spelled differently. */
std::string escapedKey(const std::string& field) {
    const size_t i = field.find('_');
    return field.substr(0, i) + std::string(1, static_cast<char>(92)) + "u005f" + field.substr(i + 1);
}

std::string withKeys(const char* tmpl, const std::string& field) {
    std::string out;
    for (const char* p = tmpl; *p; ++p) {
        if (*p == '@') out += field;
        else if (*p == '#') out += escapedKey(field);
        else if (*p == '$') out += goodValue(field);
        else if (*p == '%') out += otherValue(field);
        else out += *p;
    }
    return out;
}

struct JsonRow { const char* what; const char* json; bool refused; bool read; };

}  // namespace

TEST_F(ConfigStrictParse, AQualityConfigMustBeOneValidJsonObjectAndItsKeysAreComparedAfterTheEscapesAreRead) {
    writeGainWithConfig("csp_gq.xcal", 4.0f,
        "{\"fit_r_squared\":\"0.5\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
    XpeCalibQualityMeta base{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));

    // '@' is the field's name, '#' the same name with its first underscore spelled as the escape \u005f.
    // '$' and '%' are good values of the field (goodValue / otherValue), different from the store's 0.5 / 1 / 3 / 2.
    // (\\ in the C++ below is one backslash in the JSON text.)
    const JsonRow rows[] = {
        // the escaped spelling is the same key (Codex #40 finding 1)
        {"escaped key, malformed value: the key is found and refused", "{\"#\":\"bad\"}", true, false},
        {"escaped key, valid value: read", "{\"#\":$}", false, true},
        {"the key twice, once escaped: a duplicate", "{\"@\":$,\"#\":%}", true, false},
        {"the key twice, escaped first: a duplicate", "{\"#\":$,\"@\":%}", true, false},
        {"the key twice, both escaped: a duplicate", "{\"#\":$,\"#\":%}", true, false},
        // broken JSON is not an object with keys (Codex #40 finding 2)
        {"a broken nested object before a valid key", "{\"nested\":{bad},\"@\":$}", true, false},
        {"a bracket of the wrong kind inside a nested value", "{\"nested\":{]}", true, false},
        {"text after the closing brace", "{\"@\":$} garbage", true, false},
        {"text after an empty object", "{} garbage", true, false},
        {"a forbidden escape in a string", "{\"note\":\"a\\qb\",\"@\":$}", true, false},
        {"a raw control character (a newline) inside a string", "{\"note\":\"a\nb\",\"@\":$}", true, false},
        {"a trailing comma", "{\"@\":$,}", true, false},
        {"single quotes", "{'@':$}", true, false},
        {"a bare plus sign is not a JSON number", "{\"@\":+$}", true, false},
        {"a missing colon", "{\"@\" $}", true, false},
        {"the object is not closed", "{\"@\":$", true, false},
        {"the top level is an array", "[{\"@\":$}]", true, false},
        {"the top level is a string", "\"@\"", true, false},
        // well-formed text of every shape that has to keep working
        {"an empty object: no quality given", "{}", false, false},
        {"ordinary spacing and a surrogate pair, escapes and a solidus in another string",
         "\n {\n  \"note\" : \"\\ud83d\\ude00 \\\" \\\\ \\/ \\b\\f\\n\\r\\t\" ,\n  \"@\" : $\n }\n", false, true},
        {"the key's name at the end of a string value is not a key",
         "{\"note\":\"see \\\"@\",\"@\":$}", false, true},
        {"a nested object with the same key is not it",
         "{\"nested\":{\"@\":\"bad\"},\"@\":$}", false, true},
    };
    const char* fields[] = {"fit_r_squared", "polynomial_degree", "actual_dose_levels", "calibration_mode"};

    for (const char* field : fields) {
        for (const auto& row : rows) {
            const std::string json = withKeys(row.json, field);
            SCOPED_TRACE(std::string(field) + ": " + row.what + "  " + json);
            writeGainWithConfig("csp_gx.xcal", 2.0f, json);
            bool threw = false;
            const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
            EXPECT_FALSE(threw);
            XpeCalibQualityMeta after{};
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
            if (row.refused) {
                EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
                EXPECT_EQ(base.r_squared, after.r_squared) << "a refused file must leave the metadata as it was";
                EXPECT_EQ(base.polynomial_degree, after.polynomial_degree);
                EXPECT_EQ(base.num_points, after.num_points);
                EXPECT_EQ(base.calibration_mode, after.calibration_mode);
                EXPECT_NEAR(250.0f, gainResult(), 0.01f) << "a refused file must not have replaced the gain map";
            } else {
                ASSERT_EQ(XPE_OK, rc);
                EXPECT_NEAR(500.0f, gainResult(), 0.01f) << "the file loaded: its gain map (2) is in the store";
                if (row.read) {
                    EXPECT_EQ(1u, after.valid);
                    const double got = (std::string(field) == "fit_r_squared")      ? after.r_squared
                                     : (std::string(field) == "polynomial_degree")  ? after.polynomial_degree
                                     : (std::string(field) == "actual_dose_levels") ? after.num_points
                                                                                    : after.calibration_mode;
                    EXPECT_DOUBLE_EQ(std::stod(goodValue(field)), got) << "the value read is the top-level one";
                } else {
                    EXPECT_EQ(0u, after.valid) << "no quality was given";
                }
            }
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));      // set "base" back for the next row
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));
        }
    }
}

// QA-A-208c (Codex #43): the config block of an XCal file is read to its stored LENGTH, and the quality parser must
// read all of it. It used to be handed over as a C string, so the first NUL byte ended the text: everything behind
// it went unchecked and "{}<NUL>{\"fit_r_squared\":\"bad\"}" hid a bad quality key (the file's SHA-256 covers the
// whole block, so the hash does not catch it). Passing the real length is NOT enough, and neither does nlohmann's strict
// mode help: its lexer reads a NUL outside a string as the END of the input (token end_of_input, lexer.hpp), which
// strict accepts after the value and never reads past -- so "{}<NUL>{bad}" parsed. Inside a string a NUL is already a
// control-character error. helpers.cpp therefore refuses any NUL byte before parsing (memchr); the rows below that
// put a NUL outside a string fail without it (QA-A-208c; the first version of this comment said the opposite, QA-A-208d).
// The same rows cover what was never run before: a byte-order mark and malformed UTF-8.
TEST_F(ConfigStrictParse, AQualityConfigIsParsedToItsRealLengthNotToTheFirstNulByte) {
    writeGainWithConfig("csp_gq.xcal", 4.0f,
        "{\"fit_r_squared\":\"0.5\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
    XpeCalibQualityMeta base{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));

    const std::string nul(1, '\0');
    struct LenRow { const char* what; std::string json; bool refused; bool read; };
    // '@' is the field's name, '$' a good value of it (goodValue).
    const LenRow rows[] = {
        // a NUL ends nothing: what is behind it is checked (Codex #43)
        {"a bad quality key hidden behind a NUL", std::string("{}") + nul + "{\"@\":\"bad\"}", true, false},
        {"garbage behind a NUL", std::string("{\"@\":$}") + nul + "garbage", true, false},
        {"one NUL after a valid object", std::string("{\"@\":$}") + nul, true, false},
        {"several NULs after a valid object", std::string("{\"@\":$}") + nul + nul + nul, true, false},
        {"a NUL before the object", nul + "{\"@\":$}", true, false},
        {"a NUL inside a string", std::string("{\"note\":\"a") + nul + "b\",\"@\":$}", true, false},
        {"a NUL between a key and its colon", std::string("{\"@\"") + nul + ":$}", true, false},
        {"a NUL as the whole config", nul, true, false},
        // a byte-order mark (never run before)
        {"a complete UTF-8 byte-order mark before a valid object is skipped",
         "\xEF\xBB\xBF{\"@\":$}", false, true},
        {"an incomplete byte-order mark (two bytes)", "\xEF\xBB{\"@\":$}", true, false},
        {"a lone first byte of a byte-order mark", "\xEF{\"@\":$}", true, false},
        // malformed UTF-8 (never run before)
        {"an invalid continuation byte inside a string", "{\"note\":\"\xC3\x28\",\"@\":$}", true, false},
        {"a byte that is never valid UTF-8 inside a string", "{\"note\":\"\xFF\",\"@\":$}", true, false},
        {"a lone surrogate escape", "{\"note\":\"\\ud800\",\"@\":$}", true, false},
        {"valid multi-byte UTF-8 in another string is fine",
         "{\"note\":\"\xC3\xA9 \xE2\x82\xAC\",\"@\":$}", false, true},
    };
    const char* fields[] = {"fit_r_squared", "polynomial_degree", "actual_dose_levels", "calibration_mode"};

    for (const char* field : fields) {
        for (const auto& row : rows) {
            std::string json;
            for (char ch : row.json)
                json += (ch == '@') ? std::string(field) : (ch == '$') ? goodValue(field)
                      : (ch == '%') ? otherValue(field) : std::string(1, ch);
            SCOPED_TRACE(std::string(field) + ": " + row.what);
            writeGainWithConfig("csp_gx.xcal", 2.0f, json);
            bool threw = false;
            const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
            EXPECT_FALSE(threw);
            XpeCalibQualityMeta after{};
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
            if (row.refused) {
                EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
                EXPECT_EQ(base.r_squared, after.r_squared) << "a refused file must leave the metadata as it was";
                EXPECT_EQ(base.polynomial_degree, after.polynomial_degree);
                EXPECT_EQ(base.num_points, after.num_points);
                EXPECT_EQ(base.calibration_mode, after.calibration_mode);
                EXPECT_NEAR(250.0f, gainResult(), 0.01f) << "a refused file must not have replaced the gain map";
            } else {
                ASSERT_EQ(XPE_OK, rc);
                EXPECT_NEAR(500.0f, gainResult(), 0.01f) << "the file loaded: its gain map (2) is in the store";
                EXPECT_EQ(1u, after.valid);
                const double got = (std::string(field) == "fit_r_squared")      ? after.r_squared
                                 : (std::string(field) == "polynomial_degree")  ? after.polynomial_degree
                                 : (std::string(field) == "actual_dose_levels") ? after.num_points
                                                                                : after.calibration_mode;
                EXPECT_DOUBLE_EQ(std::stod(goodValue(field)), got) << "the value read is the top-level one";
            }
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));      // set "base" back for the next row
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));
        }
    }
}

// QA-A-208c: a quality field outside the range it has by definition is refused, and the ends of the range are
// accepted. The ranges are what no generation can step outside of (xpe_calib_mode.cpp): fit_r_squared at most 1;
// polynomial_degree 0..4; actual_dose_levels 1..10; calibration_mode 0..4.
// fit_r_squared has no lower bound: the generator reports a NEGATIVE one when the fit is worse than the mean
// (-0.0766 for the ladder of the poly-fixture tests), and a file it wrote must load -- exactly -1.0 included
// (QA-A-208d: its presence is has_r_squared, not a marker value; the generator prints it as -1.000000000).
TEST_F(ConfigStrictParse, AQualityFieldOutsideItsRangeIsRefusedAndTheEndsOfTheRangeAreAccepted) {
    writeGainWithConfig("csp_gq.xcal", 4.0f,
        "{\"fit_r_squared\":\"0.5\",\"polynomial_degree\":\"1\",\"actual_dose_levels\":\"3\",\"calibration_mode\":\"2\"}");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
    XpeCalibQualityMeta base{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));

    struct Case { const char* field; const char* value; bool accepted; };
    const Case cases[] = {
        {"fit_r_squared", "1.0001", false}, {"fit_r_squared", "2", false}, {"fit_r_squared", "-1.0", true},
        {"fit_r_squared", "-1", true}, {"fit_r_squared", "-1.000000000", true}, {"fit_r_squared", "1", true},
        {"fit_r_squared", "1.0", true},
        {"fit_r_squared", "0", true}, {"fit_r_squared", "0.0", true}, {"fit_r_squared", "-0.01", true},
        {"fit_r_squared", "-0.076637433", true}, {"fit_r_squared", "-1.0001", true},
        {"polynomial_degree", "-1", false}, {"polynomial_degree", "5", false}, {"polynomial_degree", "0", true},
        {"polynomial_degree", "4", true},
        {"actual_dose_levels", "0", false}, {"actual_dose_levels", "11", false}, {"actual_dose_levels", "1", true},
        {"actual_dose_levels", "10", true},
        {"calibration_mode", "-1", false}, {"calibration_mode", "5", false}, {"calibration_mode", "0", true},
        {"calibration_mode", "4", true},
    };
    for (const auto& cs : cases) {
        for (const bool quoted : {false, true}) {
            const std::string v = quoted ? std::string("\"") + cs.value + "\"" : std::string(cs.value);
            const std::string json = std::string("{\"") + cs.field + "\":" + v + "}";
            SCOPED_TRACE(json);
            writeGainWithConfig("csp_gx.xcal", 2.0f, json);
            bool threw = false;
            const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
            EXPECT_FALSE(threw);
            XpeCalibQualityMeta after{};
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
            if (!cs.accepted) {
                EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
                EXPECT_EQ(base.r_squared, after.r_squared) << "a refused file must leave the metadata as it was";
                EXPECT_EQ(base.polynomial_degree, after.polynomial_degree);
                EXPECT_EQ(base.num_points, after.num_points);
                EXPECT_EQ(base.calibration_mode, after.calibration_mode);
                EXPECT_NEAR(250.0f, gainResult(), 0.01f) << "a refused file must not have replaced the gain map";
            } else {
                ASSERT_EQ(XPE_OK, rc);
                EXPECT_EQ(1u, after.valid);
                const std::string f = cs.field;
                EXPECT_EQ(f == "fit_r_squared" ? 1u : 0u, static_cast<unsigned>(after.has_r_squared))
                    << "the flag says whether the file gave an R2 -- the key is there, whatever the value";
                const double got = f == "fit_r_squared" ? after.r_squared : f == "polynomial_degree" ? after.polynomial_degree
                                 : f == "actual_dose_levels" ? after.num_points : after.calibration_mode;
                EXPECT_DOUBLE_EQ(std::stod(cs.value), got);
            }
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));      // set "base" back for the next row
            ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&base));
        }
    }
}

TEST_F(ConfigStrictParse, WellFormedQualityFieldsInAGainFileAreStillRead) {
    // Control: the same call shape with good values loads and the metadata is what the file says.
    writeGainWithConfig("csp_gx.xcal", 2.0f,
        "{\"fit_r_squared\":\" +0.95\",\"polynomial_degree\":\"+2\",\"actual_dose_levels\":\"4\",\"calibration_mode\":\" 3\"}");
    bool threw = false;
    const XpeErrorCode rc = callSafely([] { return xpe_calib_load_gain("csp_gx.xcal"); }, &threw);
    EXPECT_FALSE(threw);
    ASSERT_EQ(XPE_OK, rc);
    XpeCalibQualityMeta m{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&m));
    EXPECT_NEAR(0.95, m.r_squared, 1e-9);
    EXPECT_EQ(2u, m.polynomial_degree);
    EXPECT_EQ(4u, m.num_points);
    EXPECT_EQ(3u, m.calibration_mode);
    EXPECT_NEAR(500.0f, gainResult(), 0.01f) << "the good file's map (gain 2) is now in the store";
}

// =====================================================================================================
// QA-A-209: every configuration text is read through its TOP-LEVEL keys
// =====================================================================================================
//
// The configuration readers -- the pipeline, the ghost corrector, the nonlinearity stage, the offset generation,
// and the config block of a polynomial-gain or nonlinearity-LUT file -- looked for the first occurrence of the
// quoted key anywhere in the text. {"nested":{"bypassGain":true}} switched the gain stage off, a key given twice
// was whichever came first, and a text that was not JSON was read for whatever it happened to contain. They now
// read one valid JSON object and its top-level keys (xpe_config_get_string / xpe_config_get_double), and the same
// rows run through every entry point:
//   a key only inside a nested object                 -> not given (the default)
//   a top-level key beside a nested one               -> the top-level one
//   a top-level key given twice, or a text that is not one valid JSON object
//                                                     -> XPE_ERR_CONFIG_INVALID, and nothing has changed
//   an empty value ("")                               -> not given (the configuration's rule: a GUI sends an unset
//                                                        option as "")
// "Nothing has changed" is checked per entry point: the image and its metadata, the handle that was not handed
// back, the file that was not written, the table or gain map that is still the one loaded before.

namespace {

enum class Kind { Effect, NoEffect, Refused };
struct CfgRow { const char* what; const char* tmpl; Kind kind; };

// `@` is the key under test, `$` a literal that has an effect, `%` a literal that has none (both valid values), `~` the
// other members the entry point needs (the comma next to it goes with it when there are none).
const CfgRow kCfgRows[] = {
    {"only inside a nested object: not given", "{~,\"nested\":{\"@\":$}}", Kind::NoEffect},
    {"nested one with an effect, top-level one without: the top-level one", "{~,\"nested\":{\"@\":$},\"@\":%}", Kind::NoEffect},
    {"nested one without, top-level one with an effect: the top-level one", "{~,\"nested\":{\"@\":%},\"@\":$}", Kind::Effect},
    {"nested one in an array of objects only: not given", "{~,\"list\":[{\"@\":$}]}", Kind::NoEffect},
    {"given twice at the top level (two neutral values)", "{~,\"@\":%,\"@\":%}", Kind::Refused},
    {"given twice at the top level (effect, then neutral)", "{~,\"@\":$,\"@\":%}", Kind::Refused},
    {"given twice at the top level, a nested one between", "{~,\"@\":%,\"nested\":{\"@\":$},\"@\":%}", Kind::Refused},
    {"the object is not closed", "{~,\"@\":%", Kind::Refused},
    {"the top level is an array", "[{~,\"@\":%}]", Kind::Refused},
    {"text after the object", "{~,\"@\":%} x", Kind::Refused},
    {"not JSON at all", "not json", Kind::Refused},
    {"a broken nested object before the key", "{~,\"nested\":{bad},\"@\":%}", Kind::Refused},
    {"an empty value: not given", "{~,\"@\":\"\"}", Kind::NoEffect},
    {"an ordinary top-level key", "{~,\"@\":$}", Kind::Effect},
};

struct Observation { XpeErrorCode rc; bool effect; bool unchanged; };

struct Probe {
    std::string name, key, on, off, extra;
    XpeErrorCode rcEffect = XPE_OK, rcNoEffect = XPE_OK;
    bool checkEffect = false;      // the effect shows in something other than the return code
    std::function<Observation(const std::string& json)> run;
};

std::string expand(const char* tmpl, const Probe& p) {
    std::string out;
    for (const char* c = tmpl; *c; ++c) {
        if (*c == '@') out += p.key;
        else if (*c == '$') out += p.on;
        else if (*c == '%') out += p.off;
        else if (*c == '~') {
            if (!p.extra.empty()) out += p.extra;
            else if (c[1] == ',') ++c;                                   // "{~,x" without members: "{x"
            else if (!out.empty() && out.back() == ',') out.pop_back();
        } else out += *c;
    }
    return out;
}

void runTopRows(const Probe& p) {
    for (const CfgRow& row : kCfgRows) {
        const std::string json = expand(row.tmpl, p);
        SCOPED_TRACE(p.name + ": " + row.what + "  " + json);
        bool threw = false;
        Observation o{};
        callSafely([&] { o = p.run(json); return o.rc; }, &threw);
        EXPECT_FALSE(threw) << "an exception left the entry point";
        switch (row.kind) {
            case Kind::Effect:
                EXPECT_EQ(p.rcEffect, o.rc);
                if (p.checkEffect) EXPECT_TRUE(o.effect) << "the top-level value was not applied";
                break;
            case Kind::NoEffect:
                EXPECT_EQ(p.rcNoEffect, o.rc);
                if (p.checkEffect) EXPECT_FALSE(o.effect) << "a value that is not a top-level key was applied";
                break;
            case Kind::Refused:
                EXPECT_EQ(XPE_ERR_CONFIG_INVALID, o.rc);
                EXPECT_TRUE(o.unchanged) << "a refused configuration changed something";
                break;
        }
    }
}

std::string allBypassExcept(const std::string& key) {
    std::string out;
    for (const char* k : {"bypassReadout", "bypassTemp", "bypassOffset", "bypassNonlinearity", "bypassGain",
                          "bypassBinning", "bypassDefect", "bypassGhost"}) {
        if (key == k) continue;
        if (!out.empty()) out += ",";
        out += std::string("\"") + k + "\":true";
    }
    return out;
}

Probe pipelineProbe(const std::string& entry, const std::string& key, const std::string& on, const std::string& off,
                    XpeErrorCode rcEffect, XpeErrorCode rcNoEffect) {
    Probe p;
    p.name = entry + " " + key; p.key = key; p.on = on; p.off = off; p.extra = allBypassExcept(key);
    p.rcEffect = rcEffect; p.rcNoEffect = rcNoEffect;
    p.run = [entry](const std::string& json) {
        std::vector<uint16_t> pixels(N, 1000);
        XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
        XpeImageMetadata meta{};
        XpeErrorCode rc;
        if (entry == "pipeline") rc = xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, json.c_str());
        else if (entry == "pipeline_ex") rc = xpe_preprocess_pipeline_ex(&img, &meta, nullptr, nullptr, json.c_str());
        else rc = xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, json.c_str());
        XpeImageMetadata zero{};
        bool same = std::memcmp(&meta, &zero, sizeof(meta)) == 0;
        for (const uint16_t v : pixels) same = same && v == 1000;
        return Observation{rc, false, same};
    };
    return p;
}

Probe ghostProbe(const std::string& key, const std::string& on, const std::string& off) {
    Probe p;
    p.name = "xpe_ghost_create " + key; p.key = key; p.on = on; p.off = off;
    p.rcEffect = XPE_ERR_CONFIG_INVALID; p.rcNoEffect = XPE_OK;          // `on` is a malformed number: read => refused
    p.run = [](const std::string& json) {
        void* handle = nullptr;
        const XpeErrorCode rc = xpe_ghost_create(W, H, json.c_str(), &handle);
        const bool made = handle != nullptr;
        if (handle) xpe_ghost_destroy(handle);
        return Observation{rc, false, !made};
    };
    return p;
}

/** The nonlinearity stage on a frame of 1000: the polynomial 2*x doubles it; no LUT is loaded, so the other path leaves it. */
Probe nonlinProbe(const std::string& key, const std::string& on, const std::string& off, const std::string& extra,
                  bool effectIsDoubling) {
    Probe p;
    p.name = "xpe_nonlinearity_correct " + key; p.key = key; p.on = on; p.off = off; p.extra = extra;
    p.checkEffect = true;
    p.run = [effectIsDoubling](const std::string& json) {
        std::vector<uint16_t> px(N, 1000);
        XpeImageBuffer img = buf(px.data(), XPE_PIXEL_UINT16, 16);
        const XpeErrorCode rc = xpe_nonlinearity_correct(&img, json.c_str());
        bool same = true, doubled = true;
        for (const uint16_t v : px) { same = same && v == 1000; doubled = doubled && v == 2000; }
        return Observation{rc, effectIsDoubling ? doubled : same, same};
    };
    return p;
}

}  // namespace

TEST_F(ConfigStrictParse, ThePipelineEntryPointsReadTheirKeysFromTheTopLevelOfOneJsonObject) {
    for (const char* entry : {"pipeline", "pipeline_ex", "pipeline_batch"}) {
        // The offset stage runs unless bypassed, and with no calibration loaded it says so: the code shows whether the flag counted.
        runTopRows(pipelineProbe(entry, "bypassOffset", "true", "false", XPE_OK, XPE_ERR_CALIB_NOT_LOADED));
    }
    // The numbers: `on` is a malformed number, so reading it is a refusal.
    runTopRows(pipelineProbe("pipeline", "detectorTempC", "\"abc\"", "25.5", XPE_ERR_CONFIG_INVALID, XPE_OK));
    runTopRows(pipelineProbe("pipeline", "binningMode", "\"x\"", "1", XPE_ERR_CONFIG_INVALID, XPE_OK));
    // Keys only a LATER stage reads (the nonlinearity stage): a duplicate among them fails the call before any stage runs.
    for (const char* key : {"panel.linear", "panel.nonlinearity_mode", "panel.target_platform", "panel.nonlin_poly_c0",
                            "panel.nonlin_poly_c4", "panel.adc_max"}) {
        runTopRows(pipelineProbe("pipeline", key, "\"true\"", "\"false\"", XPE_OK, XPE_OK));
    }
}

TEST_F(ConfigStrictParse, ThePipelineWithARefusedTopLevelConfigLoadsNoCalibrationAndTheBatchLeavesEveryImage) {
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

    // The duplicate is in a key only the nonlinearity stage reads: the refusal must come before the calibration is loaded.
    const std::string dup = "{\"bypassNonlinearity\":true,\"panel.linear\":\"true\",\"panel.linear\":\"false\"}";
    std::vector<uint16_t> p0(N, 1000), p1(N, 1000);
    XpeImageBuffer imgs[2] = {buf(p0.data(), XPE_PIXEL_UINT16, 16), buf(p1.data(), XPE_PIXEL_UINT16, 16)};
    XpeImageMetadata metas[2] = {};
    bool threw = false;
    XpeErrorCode rc = callSafely([&] {
        return xpe_preprocess_pipeline(&imgs[0], &metas[0], "csp_calib", nullptr, dup.c_str());
    }, &threw);
    EXPECT_FALSE(threw);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
    EXPECT_FALSE(offsetLoaded()) << "a refused configuration must not have loaded the calibration files";
    rc = callSafely([&] {
        return xpe_preprocess_pipeline_batch(imgs, 2, metas, "csp_calib", nullptr, dup.c_str());
    }, &threw);
    EXPECT_FALSE(threw);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
    EXPECT_FALSE(offsetLoaded());
    EXPECT_EQ(0u, metas[0].flags);
    EXPECT_EQ(0u, metas[1].flags);
    for (size_t i = 0; i < N; ++i) { ASSERT_EQ(1000u, p0[i]); ASSERT_EQ(1000u, p1[i]); }
}

TEST_F(ConfigStrictParse, TheGhostCorrectorReadsItsKeysFromTheTopLevelOfOneJsonObject) {
    runTopRows(ghostProbe("alpha1", "\"abc\"", "0.5"));
    runTopRows(ghostProbe("tau2", "\"1e999\"", "10"));
    runTopRows(ghostProbe("tier", "\"x\"", "2"));
    runTopRows(ghostProbe("nlcscBeta", "\"1.5.2\"", "0.1"));
}

TEST_F(ConfigStrictParse, TheNonlinearityStageReadsItsKeysFromTheTopLevelOfOneJsonObject) {
    // The polynomial 2*x runs when the profile names POLY (or AUTO on an embedded target) and has coefficients.
    runTopRows(nonlinProbe("panel.nonlinearity_mode", "\"POLY\"", "\"LUT\"", "\"panel.nonlin_poly_c1\":2.0", true));
    runTopRows(nonlinProbe("panel.nonlin_poly_c1", "2.0", "0.0", "\"panel.nonlinearity_mode\":\"POLY\"", true));
    runTopRows(nonlinProbe("panel.target_platform", "\"MCU\"", "\"CPU\"",
                           "\"panel.nonlinearity_mode\":\"AUTO\",\"panel.nonlin_poly_c1\":2.0", true));
    // panel.linear = true skips the stage: the frame stays as it was.
    runTopRows(nonlinProbe("panel.linear", "\"true\"", "\"false\"",
                           "\"panel.nonlinearity_mode\":\"POLY\",\"panel.nonlin_poly_c1\":2.0", false));
}

TEST_F(ConfigStrictParse, TheOffsetGenerationReadsItsKeysFromTheTopLevelOfOneJsonObject) {
    const std::string out = "csp_offset_out.xcal";
    for (const auto& kv : std::vector<std::vector<std::string>>{
             {"sigma", "-1", "3.0"}, {"method", "\"bogus\"", "\"mean\""}, {"max_iter", "0", "5"},
             {"lower_percentile", "-5", "10.0"}}) {
        Probe p;
        p.name = "xpe_calib_generate_offset " + kv[0]; p.key = kv[0]; p.on = kv[1]; p.off = kv[2];
        p.rcEffect = XPE_ERR_CONFIG_INVALID; p.rcNoEffect = XPE_OK;       // `on` is out of range: read => refused
        p.run = [out](const std::string& json) {
            std::remove(out.c_str());
            std::vector<std::vector<uint16_t>> frames{std::vector<uint16_t>(N, 100), std::vector<uint16_t>(N, 110),
                                                     std::vector<uint16_t>(N, 105)};
            std::vector<XpeImageBuffer> bufs;
            for (auto& f : frames) bufs.push_back(buf(f.data(), XPE_PIXEL_UINT16, 16));
            const XpeErrorCode rc = xpe_calib_generate_offset(bufs.data(), 3, 100.0f, 25.0f, out.c_str(), json.c_str());
            const bool wrote = fs::exists(out);
            std::remove(out.c_str());
            return Observation{rc, false, !wrote};
        };
        runTopRows(p);
    }
}

namespace {

/** A polynomial gain file (degree 1, gain 1 in every pixel) with `json` as its config block. */
void writePolyGain(const char* path, const std::string& json) {
    std::vector<float> coeffs(N * 2);
    for (size_t i = 0; i < N; ++i) { coeffs[i * 2] = 1.0f; coeffs[i * 2 + 1] = 0.0f; }
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = XCAL_TYPE_GAIN_POLY; hdr.pixel_format = XCAL_FMT_FLOAT32;
    hdr.width = W; hdr.height = H; hdr.payload_len = coeffs.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      reinterpret_cast<const uint8_t*>(coeffs.data()), coeffs.size() * sizeof(float)));
}

/** A 4096-entry nonlinearity table (`entry(i)` for index i) with `json` as its config block. */
void writeLut(const char* path, bool halving, const std::string& json) {
    std::vector<uint16_t> lut(4096u);
    for (uint32_t i = 0; i < 4096u; ++i) lut[i] = static_cast<uint16_t>(halving ? i / 2u : i);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT16);
    hdr.width = 4096; hdr.height = 1; hdr.payload_len = lut.size() * sizeof(uint16_t);
    hdr.created_epoch_ms = 1700000000000ll;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      reinterpret_cast<const uint8_t*>(lut.data()), lut.size() * sizeof(uint16_t)));
}

bool pendingAlertContains(const char* needle) {
    char msg[512];
    int32_t sev = -1;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i) {
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
        if (std::string(msg).find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST_F(ConfigStrictParse, TheConfigBlockOfAPolynomialGainFileIsReadFromItsTopLevelKeys) {
    Probe p;
    p.name = "xpe_calib_load_gain (polynomial) dose_min"; p.key = "dose_min"; p.on = "10.0";
    p.off = "\"none\"";             // a string is not a number: the range is then not given
    p.extra = "\"dose_max\":100.0";
    p.checkEffect = true;                 // a polynomial file with no dose range loads, and says so in an alert
    p.run = [](const std::string& json) {
        writeGainWithConfig("csp_gq.xcal", 4.0f, "{}");
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain("csp_gq.xcal"));
        writePolyGain("csp_gx.xcal", json);
        xpe_clear_alerts();
        const XpeErrorCode rc = xpe_calib_load_gain("csp_gx.xcal");
        const bool rangeRead = !pendingAlertContains("without a dose range");
        return Observation{rc, rangeRead, std::fabs(gainResult() - 250.0f) < 0.01f};   // still the plain gain 4 map
    };
    runTopRows(p);
}

TEST_F(ConfigStrictParse, TheConfigBlockOfANonlinearityLutFileIsReadFromItsTopLevelKeys) {
    Probe p;
    p.name = "xpe_calib_load_nonlin_lut xcal_nonlin_extension_start"; p.key = "xcal_nonlin_extension_start";
    p.on = "99999"; p.off = "100";        // a start past the end of the table is a corrupt record
    p.rcEffect = XPE_ERR_INVALID_CALIB_DATA; p.rcNoEffect = XPE_OK;
    p.run = [](const std::string& json) {
        writeLut("csp_gq.xcal", true, "{}");
        EXPECT_EQ(XPE_OK, xpe_calib_load_nonlin_lut("csp_gq.xcal"));       // the table in the store: halves a frame
        writeLut("csp_gx.xcal", false, json);
        const XpeErrorCode rc = xpe_calib_load_nonlin_lut("csp_gx.xcal");
        std::vector<uint16_t> px(N, 1000);
        XpeImageBuffer img = buf(px.data(), XPE_PIXEL_UINT16, 16);
        EXPECT_EQ(XPE_OK, xpe_nonlinearity_correct(&img, nullptr));
        return Observation{rc, false, px[0] == 500u};                      // a failed load left the halving table in place
    };
    runTopRows(p);
}

TEST_F(ConfigStrictParse, ANullConfigPointerStillMeansTheDefaults) {
    void* handle = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));
    ASSERT_NE(nullptr, handle);
    xpe_ghost_destroy(handle);
    std::vector<uint16_t> pixels(N, 1000);
    XpeImageBuffer img = buf(pixels.data(), XPE_PIXEL_UINT16, 16);
    XpeImageMetadata meta{};
    // No configuration: nothing is bypassed, so every stage runs and the result is float32 -- which does not fit the
    // uint16 buffer, a refusal the call makes before any stage (not CONFIG_INVALID: a null text is not a malformed one).
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, nullptr));
}
