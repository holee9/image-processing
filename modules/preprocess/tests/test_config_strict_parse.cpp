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
    return cfg("\"" + field + "\":\"" + value + "\"");
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
        const std::string c = "{\"" + f + "\":\"" + v + "\"}";
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
std::string withField(const char* tmpl, const char* field) {
    std::string out;
    for (const char* p = tmpl; *p; ++p) {
        if (*p == '@') out += field; else out += *p;
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

    // '@' stands for the field's name. A good value is 7 (valid for all four fields, and different from the
    // store's 0.5 / 1 / 3 / 2, so a read value shows as a change); a bad one is "bad".
    const TopRow rows[] = {
        {"only inside a nested object: not given (the nested value is not read)",
         "{\"nested\":{\"@\":7},\"other\":2}", false, false},
        {"only inside a nested object, malformed: not given (the nested value does not poison the load)",
         "{\"nested\":{\"@\":\"bad\"}}", false, false},
        {"valid in a nested object, malformed at the top level: refused",
         "{\"nested\":{\"@\":7},\"@\":\"bad\"}", true, false},
        {"malformed at the top level, valid nested after: refused",
         "{\"@\":\"bad\",\"nested\":{\"@\":7}}", true, false},
        {"in a nested array of objects only: not given",
         "{\"list\":[{\"@\":7},{\"@\":8}]}", false, false},
        {"the top-level key given twice, both valid: refused",
         "{\"@\":7,\"@\":7}", true, false},
        {"the top-level key given twice, the first valid: refused",
         "{\"@\":7,\"x\":0,\"@\":\"bad\"}", true, false},
        {"the top-level key given twice, the second valid: refused",
         "{\"@\":\"bad\",\"@\":7}", true, false},
        // In "see \"@" the string's own closing quote follows the key name, so the raw text contains "@" with a
        // quote on each side -- which is all a first-occurrence search looks for.
        {"the key's name at the end of a string value: not a key",
         "{\"note\":\"see \\\"@\"}", false, false},
        {"the key's name at the end of a string value, then the real key: the real key is read",
         "{\"note\":\"see \\\"@\",\"@\":7}", false, true},
        {"braces and quotes inside a string value are skipped, not counted",
         "{\"note\":\"} { [ \\\" ]\",\"@\":7}", false, true},
        {"the real key after a nested object with the same key: the top-level value is the one read",
         "{\"nested\":{\"@\":\"bad\"},\"@\":7}", false, true},
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
                    EXPECT_DOUBLE_EQ(7.0, got) << "the value read is the top-level one";
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
