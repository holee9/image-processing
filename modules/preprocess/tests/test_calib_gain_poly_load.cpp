/**
 * @file test_calib_gain_poly_load.cpp
 * @brief QA-A-336: a gain POLYNOMIAL file is refused with an explicit error; the single-dose gain file's FUNC-033 fields.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * The dose-dependent gain polynomial (XCAL type 3, written by xpe_calib_generate_gain_polynomial) was removed by user
 * decision (QA-A-336). A type-3 file that still exists on disk must not load -- and must never be read as a scalar map:
 * the loader refuses it with XPE_ERR_CONFIG_INVALID and an alert "XPE_ERR_XCAL_TYPE: ..." naming the type, and the store
 * keeps what it held. Until QA-A-336 this file proved the opposite (that such a file loads, QA-A-37 #140); the cases
 * below build the file by hand the way those cases did, since no generator writes it any more.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 4;
constexpr uint32_t H = 4;
constexpr size_t   N = static_cast<size_t>(W) * H;

class GainPolyLoadTest : public ::testing::Test {
protected:
    fs::path tmpDir;
    int      generation = 0;

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        tmpDir = fs::temp_directory_path() / "xpe_gain_poly_load";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
    }

    void TearDown() override {
        // QA-A-138 (#198): leave the pending alert queue as this test found
        // it. The product drains it through this public call; nothing in
        // xpe_preprocess_shutdown() touches the common-module queue.
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    /** One scalar GAIN level file, every pixel = @p value. */
    std::string writeGainLevel(const std::string& name, float value) {
        const std::string path = (tmpDir / name).string();
        const std::vector<float> data(N, value);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len  = data.size() * sizeof(float);
        EXPECT_EQ(XPE_OK,
                  write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));
        return path;
    }

    /** Reads the config JSON block back out of an XCal file. */
    std::string readConfigJson(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        EXPECT_TRUE(f.is_open());
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        std::string json(static_cast<size_t>(hdr.config_json_len), '\0');
        if (hdr.config_json_len > 0) f.read(&json[0], hdr.config_json_len);
        return json;
    }

    /** Runs the single-dose generator over @p count identical flat frames. */
    XpeErrorCode generateSingleDose(const std::string& outName,
                                    const char* metadataJson = nullptr,
                                    int32_t count = 4) {
        flatStore.assign(static_cast<size_t>(count), std::vector<uint16_t>(N, 3000));
        std::vector<XpeImageBuffer> frames(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i) {
            XpeImageBuffer& b = frames[static_cast<size_t>(i)];
            b.data = flatStore[static_cast<size_t>(i)].data();
            b.width = W; b.height = H;
            b.bitsAllocated = 16; b.bitsStored = 16;
            b.format = XPE_PIXEL_UINT16;
            b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
        }
        return xpe_calib_generate_gain(frames.data(), count, nullptr,
                                       (tmpDir / outName).string().c_str(),
                                       metadataJson);
    }
    std::vector<std::vector<uint16_t>> flatStore;

    /** A hand-built file of the removed polynomial type (3) with an arbitrary payload length. */
    std::string writePolyRaw(const std::string& name, uint64_t payloadLen,
                             const std::string& json = std::string()) {
        const std::string path = (tmpDir / name).string();
        std::vector<uint8_t> payload(static_cast<size_t>(payloadLen), 0);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version         = XCAL_VERSION;
        hdr.type            = 3u;   // the removed XCAL_TYPE_GAIN_POLY (QA-A-336: the number stays retired)
        hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len     = payloadLen;
        hdr.config_json_len = json.size();
        EXPECT_EQ(XPE_OK,
                  write_xcal_file(path.c_str(), hdr,
                                  json.empty() ? nullptr
                                      : reinterpret_cast<const uint8_t*>(json.data()),
                                  json.size(),
                                  payload.data(), payloadLen));
        return path;
    }
};

/** The pending alerts that contain @p needle. */
static int countAlerts(const char* needle) {
    int found = 0;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t k = 0; k < n; ++k) {
        char msg[512] = {};
        int32_t severity = 0;
        if (xpe_get_pending_alert(k, msg, sizeof(msg), &severity) == XPE_OK && std::strstr(msg, needle) != nullptr) ++found;
    }
    return found;
}

/** One frame through xpe_gain_correct with the store as it is; the code, and the first output pixel through @p first. */
static XpeErrorCode correctOnce(float* first) {
    std::vector<uint16_t> inData(N, 1000);
    std::vector<float>    outData(N, 0.0f);
    XpeImageBuffer in{}, out{};
    in.data = inData.data();  in.width = W;  in.height = H;
    in.bitsAllocated = 16; in.bitsStored = 16;
    in.format = XPE_PIXEL_UINT16;
    in.dataSize = static_cast<uint32_t>(inData.size() * sizeof(uint16_t));
    out.data = outData.data(); out.width = W; out.height = H;
    out.bitsAllocated = 32; out.bitsStored = 32;
    out.format = XPE_PIXEL_FLOAT32;
    out.dataSize = static_cast<uint32_t>(outData.size() * sizeof(float));
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_gain_correct(&in, &out, &meta);
    *first = outData[0];
    return rc;
}

// QA-A-336: a polynomial gain file -- one plane, three planes, with and without the dose range it used to carry -- is
// refused with an explicit error, never loaded and never read as a scalar map. Into an empty store: nothing is loaded
// afterwards. Over a loaded scalar map: that map stays and is the one applied.
TEST_F(GainPolyLoadTest, APolynomialGainFileIsRefusedWithAnExplicitErrorAndNothingIsLoaded) {
    const uint64_t plane = static_cast<uint64_t>(N) * sizeof(float);
    const std::string onePlane = writePolyRaw("poly1.xcal", plane);
    const std::string threePlanes = writePolyRaw("poly3.xcal", 3 * plane,
                                                 "{\"polynomial_degree\":2,\"dose_min\":14037.0,\"dose_max\":42677.0}");

    // Control: the same writer, the scalar type, loads -- so a refusal below is the type, not the file.
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(writeGainLevel("control.xcal", 1.0f).c_str()));
    xpe_preprocess_shutdown();   // empty store again
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    xpe_clear_alerts();

    float first = 0.0f;
    for (const std::string& path : {onePlane, threePlanes}) {
        xpe_clear_alerts();
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain(path.c_str())) << path;
        EXPECT_EQ(1, countAlerts("XPE_ERR_XCAL_TYPE: calibration file type 3 (gain polynomial")) << path;
        EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, correctOnce(&first)) << "a refused file left a gain in the store: " << path;
    }

    // Over a loaded scalar map of 2.0: refused, and the scalar map is still the one applied (1000 / 2.0 = 500).
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(writeGainLevel("scalar.xcal", 2.0f).c_str()));
    for (const std::string& path : {onePlane, threePlanes}) {
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain(path.c_str())) << path;
        ASSERT_EQ(XPE_OK, correctOnce(&first));
        EXPECT_FLOAT_EQ(500.0f, first) << "the refused polynomial file changed the loaded scalar map: " << path;
    }

    // The cached loader refuses it the same way.
    XpeImageBuffer view{};
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain_cached(threePlanes.c_str(), &view));
}

// A type-3 file is not an offset map either, and a scalar gain file is still not a defect map.
TEST_F(GainPolyLoadTest, TypeMismatchStillReportsConfigInvalid) {
    const uint64_t plane = static_cast<uint64_t>(N) * sizeof(float);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              xpe_calib_load_offset(writePolyRaw("mismatch.xcal", plane).c_str()));

    const std::string scalar = writeGainLevel("as_defect.xcal", 1.0f);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_map(scalar.c_str()));
}

// --- FUNC-033 (1) for the single-dose generator ---------------------------
//
// SRS-CALIB-FUNC-033 (1): "Every generated XCal gain file shall include:
// calibration_mode, actual_dose_levels, polynomial_degree (0 for single-point),
// fit_r_squared (1.0 for single-point by definition), max_residual_pct,
// mean_residual_pct, ...".

TEST_F(GainPolyLoadTest, SingleDoseGainFileCarriesTheMandatoryFields) {
    ASSERT_EQ(XPE_OK, generateSingleDose("single.xcal"));

    const std::string json = readConfigJson((tmpDir / "single.xcal").string());
    for (const char* key : {"calibration_mode", "actual_dose_levels",
                            "polynomial_degree", "fit_r_squared",
                            "max_residual_pct", "mean_residual_pct",
                            "calibration_pass"}) {
        EXPECT_NE(std::string::npos, json.find(key))
            << "SRS-CALIB-FUNC-033 (1) field missing from the file: " << key;
    }
    EXPECT_NE(std::string::npos, json.find("\"polynomial_degree\":0"));
    EXPECT_NE(std::string::npos, json.find("\"actual_dose_levels\":1"));
}

TEST_F(GainPolyLoadTest, SingleDoseMetadataSurvivesTheRoundTrip) {
    ASSERT_EQ(XPE_OK, generateSingleDose("single_rt.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain((tmpDir / "single_rt.xcal").string().c_str()));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));
    EXPECT_NEAR(1.0, meta.r_squared, 1e-9) << "1.0 for single-point by definition";
    EXPECT_EQ(1u, meta.num_points);
    EXPECT_EQ(0u, meta.polynomial_degree);
    EXPECT_EQ(1u, meta.calibration_pass) << "1.0 >= the 0.999 gate";
}

// The caller's own metadata used to be the whole config JSON. It must survive
// the FUNC-033 fields being added around it, or callers lose data.
TEST_F(GainPolyLoadTest, CallerMetadataIsNotDiscarded) {
    ASSERT_EQ(XPE_OK, generateSingleDose("with_meta.xcal",
                                         "{\"kVp\":80,\"mAs\":2.5}"));

    const std::string json = readConfigJson((tmpDir / "with_meta.xcal").string());
    EXPECT_NE(std::string::npos, json.find("kVp"));
    EXPECT_NE(std::string::npos, json.find("fit_r_squared"));
}

} // namespace
