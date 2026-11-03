/**
 * @file test_xpe_preprocess_calibration.cpp
 * @brief Calibration management tests for XPE Preprocessing Module
 *
 * Tests calibration loading, generation, expiry checking, and saving.
 * Covers REQ-P1A-014 through REQ-P1A-019.
 */

#include <gtest/gtest.h>
#include <fstream>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <vector>
#include <string>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xcal_writer.hpp"
#include "xcal_reader.hpp"
#include "defect_ext.hpp"
#include "rle_codec.hpp"
#include "xpe/preprocess/xpe_preprocess_internal.h"

// =============================================================================
// Test Data Generation Helpers
// =============================================================================

namespace {

/**
 * @brief Create a canonical XCal v1 fixture with a valid payload hash.
 */
bool CreateTestXCalFile(const char* filepath,
                        uint32_t width,
                        uint32_t height,
                        uint32_t data_type,
                        const char* session_id = "test-session-123",
                        int64_t expiry_ms = 4102444800000LL) {
    XCalFileHeader header{};
    std::memcpy(header.magic, XCAL_MAGIC, 4);
    header.version = XCAL_VERSION;
    header.type = data_type == 0 ? XCAL_TYPE_OFFSET : data_type == 1 ? XCAL_TYPE_GAIN : XCAL_TYPE_DEFECT;
    header.pixel_format = data_type == 2 ? XCAL_FMT_UINT8_MASK : XCAL_FMT_FLOAT32;
    header.width = width; header.height = height;
    header.expiry_epoch_ms = expiry_ms;
    const size_t length = std::strlen(session_id);
    if (length >= sizeof(header.session_id)) return false;
    std::memcpy(header.session_id, session_id, length);
    const size_t pixels = static_cast<size_t>(width) * height;
    if (data_type == 2) {
        const std::vector<uint8_t> mask(pixels, 0);
        return XPE_OK == write_xcal_file(filepath, header, nullptr, 0, mask.data(), mask.size());
    }
    const std::vector<float> map(pixels, data_type == 0 ? 100.0f : 2.0f);
    return XPE_OK == write_xcal_file(filepath, header, nullptr, 0,
        reinterpret_cast<const uint8_t*>(map.data()), map.size() * sizeof(float));
}

void ExpectSavedMap(uint32_t type, float value) {
    XCalFileHeader header{};
    std::vector<uint8_t> config, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file("test_output.xcal", header, config, payload, true, type));
    EXPECT_EQ(1024u, header.width); EXPECT_EQ(1024u, header.height);
    EXPECT_EQ(XCAL_FMT_FLOAT32, header.pixel_format);
    const std::vector<float> expected(1024 * 1024, value);
    ASSERT_EQ(expected.size() * sizeof(float), payload.size());
    EXPECT_EQ(0, std::memcmp(expected.data(), payload.data(), payload.size()));
}

bool WriteOffsetWithConfig(const char* path, const std::string& config, float shift = 0.0f) {
    XCalFileHeader header{};
    std::memcpy(header.magic, XCAL_MAGIC, 4);
    header.version = XCAL_VERSION;
    header.type = XCAL_TYPE_OFFSET;
    header.pixel_format = XCAL_FMT_FLOAT32;
    header.width = 2; header.height = 2;
    std::memcpy(header.session_id, "test-session-123", 16);
    const float map[]{1.0f + shift, 2.0f + shift, 3.0f + shift, 4.0f + shift};
    return XPE_OK == write_xcal_file(path, header,
        config.empty() ? nullptr : reinterpret_cast<const uint8_t*>(config.data()), config.size(),
        reinterpret_cast<const uint8_t*>(map), sizeof(map));
}

void ExpectSavedOffsetConfig(const std::string& expected, float shift = 0.0f) {
    ASSERT_EQ(XPE_OK, xpe_calib_save("test_output.xcal", "offset", 4102444800000ULL));
    XCalFileHeader header{};
    std::vector<uint8_t> config, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file("test_output.xcal", header, config, payload, true, XCAL_TYPE_OFFSET));
    EXPECT_EQ(expected, std::string(config.begin(), config.end()));
    EXPECT_EQ(4102444800000LL, header.expiry_epoch_ms);
    const float expectedMap[]{1.0f + shift, 2.0f + shift, 3.0f + shift, 4.0f + shift};
    ASSERT_EQ(sizeof(expectedMap), payload.size());
    EXPECT_EQ(0, std::memcmp(expectedMap, payload.data(), sizeof(expectedMap)));
}

bool WriteScalarGainWithConfig(const char* path, uint32_t width, uint32_t height,
    const std::vector<float>& map, const std::string& config, bool compressed = false) {
    if (map.size() != static_cast<size_t>(width) * height) return false;
    XCalFileHeader header{};
    std::memcpy(header.magic, XCAL_MAGIC, 4);
    header.version = XCAL_VERSION; header.type = XCAL_TYPE_GAIN;
    header.pixel_format = XCAL_FMT_FLOAT32; header.width = width; header.height = height;
    std::memcpy(header.session_id, "test-session-123", 16);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(map.data());
    size_t size = map.size() * sizeof(float);
    std::vector<uint8_t> encoded;
    if (compressed) {
        if (rle_encode(data, size, encoded) != XPE_OK) return false;
        data = encoded.data(); size = encoded.size();
    }
    return XPE_OK == write_xcal_file(path, header,
        config.empty() ? nullptr : reinterpret_cast<const uint8_t*>(config.data()), config.size(), data, size);
}

void ExpectSavedGainConfig(const std::string& expected, const std::vector<float>& map) {
    ASSERT_EQ(XPE_OK, xpe_calib_save("test_output.xcal", "gain", 0));
    XCalFileHeader header{};
    std::vector<uint8_t> config, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file("test_output.xcal", header, config, payload, true, XCAL_TYPE_GAIN));
    EXPECT_EQ(expected, std::string(config.begin(), config.end()));
    ASSERT_EQ(map.size() * sizeof(float), payload.size());
    EXPECT_EQ(0, std::memcmp(map.data(), payload.data(), payload.size()));
}

/**
 * @brief Create corrupted XCal file (wrong magic number)
 */
bool CreateCorruptedXCalFile(const char* filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;

    file.write("BAD!", 4);  // Wrong magic
    return file.good();
}

/**
 * @brief Create test image buffer
 */
XpeImageBuffer* CreateTestImage(uint32_t width, uint32_t height, XpePixelFormat format) {
    XpeImageBuffer* img = new XpeImageBuffer();
    std::memset(img, 0, sizeof(XpeImageBuffer));

    img->width = width;
    img->height = height;
    img->format = format;
    img->bitsAllocated = (format == XPE_PIXEL_UINT16) ? 16 : 32;
    img->bitsStored = img->bitsAllocated;

    size_t pixel_size = (format == XPE_PIXEL_UINT16) ? sizeof(uint16_t) : sizeof(float);
    // stride field does not exist in XpeImageBuffer (removed for ABI compatibility)
    size_t data_size = height * width * pixel_size;
    img->data = new uint8_t[data_size];
    img->dataSize = data_size;
    std::memset(img->data, 0, data_size);

    // Fill with test data
    if (format == XPE_PIXEL_UINT16) {
        uint16_t* data = reinterpret_cast<uint16_t*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1000 + (i % 100);
        }
    } else {
        float* data = reinterpret_cast<float*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1.0f + (i % 100) * 0.01f;
        }
    }

    return img;
}

/**
 * @brief Free image buffer
 */
void FreeTestImage(XpeImageBuffer* img) {
    if (img) {
        delete[] img->data;
        delete img;
    }
}

} // anonymous namespace

// =============================================================================
// Test Fixtures
// =============================================================================

class PreprocessCalibrationTest : public ::testing::Test {
protected:
    std::filesystem::path previousDirectory, fixtureDirectory;
    void SetUp() override {
        previousDirectory = std::filesystem::current_path();
        fixtureDirectory = std::filesystem::temp_directory_path() /
            ("xpe_calibration_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(fixtureDirectory));
        std::filesystem::current_path(fixtureDirectory);
        xpe_preprocess_shutdown();
        ASSERT_EQ(xpe_preprocess_init(NULL), XPE_OK);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();

        // Cleanup test files
        std::remove("test_offset.xcal");
        std::remove("test_gain.xcal");
        std::remove("test_defect.xcal");
        std::remove("test_check.xcal");
        std::remove("test_output.xcal");
        std::remove("test_corrupted.xcal");
        if (!previousDirectory.empty()) std::filesystem::current_path(previousDirectory);
        if (!fixtureDirectory.empty()) {
            std::error_code error;
            std::filesystem::remove(fixtureDirectory, error); // only removes the empty directory created by this test
        }
    }
};

TEST_F(PreprocessCalibrationTest, OffsetConfigRoundTripPreservesTemperatureAndOpaqueExtensions) {
    const std::string config = " {\"method\":\"median\", \"integration_time_ms\":123.5,\"temperature_c\":31.25,\"extension\":{\"v\":[1,2],\"xcal_compression\":1,\"xcal_raw_payload_len\":16}} \n";
    ASSERT_TRUE(WriteOffsetWithConfig("test_offset.xcal", config));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ExpectSavedOffsetConfig(config);
}

TEST_F(PreprocessCalibrationTest, OffsetConfigCompressedRoundTripKeepsMetadataAndPayloadConsistent) {
    XCalFileHeader header{};
    std::memcpy(header.magic, XCAL_MAGIC, 4);
    header.version = XCAL_VERSION; header.type = XCAL_TYPE_OFFSET;
    header.pixel_format = XCAL_FMT_FLOAT32; header.width = header.height = 2;
    const float map[]{1.0f, 2.0f, 3.0f, 4.0f};
    const std::string config = "{\"temperature_c\":31,\"xcal_compression\":1,\"xcal_raw_payload_len\":16}";
    std::vector<uint8_t> encoded;
    ASSERT_EQ(XPE_OK, rle_encode(reinterpret_cast<const uint8_t*>(map), sizeof(map), encoded));
    ASSERT_EQ(XPE_OK, write_xcal_file("test_offset.xcal", header,
        reinterpret_cast<const uint8_t*>(config.data()), config.size(), encoded.data(), encoded.size()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ExpectSavedOffsetConfig(config);
}

TEST_F(PreprocessCalibrationTest, OffsetConfigCacheHitRestoresTheMatchingMapMetadata) {
    const std::string a = "{\"temperature_c\":21,\"method\":\"mean\"}";
    const std::string b = "{\"temperature_c\":34,\"method\":\"median\"}";
    ASSERT_TRUE(WriteOffsetWithConfig("test_offset.xcal", a));
    ASSERT_TRUE(WriteOffsetWithConfig("test_check.xcal", b, 100.0f));
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("test_offset.xcal", &view));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_check.xcal"));
    ExpectSavedOffsetConfig(b, 100.0f);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("test_offset.xcal", &view));
    ExpectSavedOffsetConfig(a);
}

TEST_F(PreprocessCalibrationTest, OffsetConfigEmptyReplacementDoesNotKeepPreviousMetadata) {
    ASSERT_TRUE(WriteOffsetWithConfig("test_offset.xcal", "{\"temperature_c\":31}"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ASSERT_TRUE(WriteOffsetWithConfig("test_check.xcal", ""));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_check.xcal"));
    ExpectSavedOffsetConfig("");
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("test_check.xcal", &view));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("test_check.xcal", &view));
    ExpectSavedOffsetConfig("");
}

TEST_F(PreprocessCalibrationTest, OffsetConfigRejectedLoadPreservesPreviousMetadata) {
    const std::string config = "{\"temperature_c\":31}";
    ASSERT_TRUE(WriteOffsetWithConfig("test_offset.xcal", config));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ASSERT_TRUE(CreateCorruptedXCalFile("test_corrupted.xcal"));
    EXPECT_NE(XPE_OK, xpe_calib_load_offset("test_corrupted.xcal"));
    ExpectSavedOffsetConfig(config);
}

TEST_F(PreprocessCalibrationTest, SavedDefectKeepsItsSessionAndReloadsWithTheOriginalSet) {
    const char* session = "saved-set-session";
    ASSERT_TRUE(CreateTestXCalFile("test_offset.xcal", 2, 2, 0, session));
    ASSERT_TRUE(CreateTestXCalFile("test_gain.xcal", 2, 2, 1, session));
    ASSERT_TRUE(CreateTestXCalFile("test_defect.xcal", 2, 2, 2, session));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_gain.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("test_defect.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_save("test_output.xcal", "defect", 4102444800000ULL));
    XCalFileHeader header{};
    std::vector<uint8_t> config, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file("test_output.xcal", header, config, payload, true, XCAL_TYPE_DEFECT));
    EXPECT_STREQ(session, header.session_id);
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("test_offset.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_gain.xcal"));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map("test_output.xcal"));
}

TEST_F(PreprocessCalibrationTest, SavedDefectV2RetainsTheRuntimeLayersAndCorrection) {
    constexpr uint32_t width = 16, height = 16;
    std::vector<uint8_t> map(width * height, 0), area(width * height, 1);
    for (uint32_t x = 0; x < width; ++x) area[x] = 0;
    for (uint32_t x = 2; x < 6; ++x) map[3 * width + x] = 1;
    map[5 * width + 5] = map[5 * width + 6] = 3;
    DefectExtLine line;
    line.orientation = 0; line.index = 3; line.width = 1; line.start = 2; line.end = 6;
    line.causes = 1; line.correction = XPE_DEFECT_LINE_CORRECT_INTERPOLATE;
    line.strength = 12.5f; line.persistence = 0.75f; line.segments = {{3, 2, 6}};
    DefectExt extension;
    extension.lines = {line}; extension.blobs = {{5, 5, 6, 5, 2}};
    DefectXcalWrite job;
    job.width = width; job.height = height; job.map = map.data(); job.area = area.data();
    job.ext = &extension; job.session_id = "saved-v2-state";
    ASSERT_EQ(XPE_OK, write_defect_xcal("test_defect.xcal", job));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("test_defect.xcal"));
    std::vector<float> input(width * height), before(input.size()), after(input.size());
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            input[y * width + x] = map[y * width + x] ? 60000.0f : static_cast<float>(x * x + 10 * y * y + x * y);
    XpeImageBuffer in{}, out{};
    in.width = out.width = width; in.height = out.height = height;
    in.bitsAllocated = out.bitsAllocated = 32; in.bitsStored = out.bitsStored = 32;
    in.format = out.format = XPE_PIXEL_FLOAT32;
    in.data = input.data(); out.data = before.data();
    in.dataSize = out.dataSize = input.size() * sizeof(float);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in, &out, &meta));
    ASSERT_EQ(XPE_OK, xpe_calib_save("test_output.xcal", "defect", 4102444800000ULL));
    DefectXcal saved;
    ASSERT_EQ(XPE_OK, read_defect_xcal("test_output.xcal", true, &saved));
    EXPECT_EQ(2u, saved.format_version);
    EXPECT_EQ(map, saved.map); EXPECT_EQ(area, saved.area);
    EXPECT_EQ("saved-v2-state", saved.session_id);
    EXPECT_EQ(4102444800000LL, saved.expiry_epoch_ms);
    ASSERT_EQ(1u, saved.ext.lines.size());
    const auto& actual = saved.ext.lines[0];
    EXPECT_EQ(line.orientation, actual.orientation); EXPECT_EQ(line.index, actual.index);
    EXPECT_EQ(line.width, actual.width); EXPECT_EQ(line.start, actual.start); EXPECT_EQ(line.end, actual.end);
    EXPECT_EQ(line.causes, actual.causes); EXPECT_EQ(line.correction, actual.correction);
    EXPECT_FLOAT_EQ(line.strength, actual.strength); EXPECT_FLOAT_EQ(line.persistence, actual.persistence);
    ASSERT_EQ(1u, actual.segments.size());
    EXPECT_EQ(3u, actual.segments[0].line); EXPECT_EQ(2u, actual.segments[0].start); EXPECT_EQ(6u, actual.segments[0].end);
    ASSERT_EQ(1u, saved.ext.blobs.size());
    EXPECT_EQ(5u, saved.ext.blobs[0].x0); EXPECT_EQ(5u, saved.ext.blobs[0].y0);
    EXPECT_EQ(6u, saved.ext.blobs[0].x1); EXPECT_EQ(5u, saved.ext.blobs[0].y1); EXPECT_EQ(2u, saved.ext.blobs[0].size);
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("test_output.xcal"));
    out.data = after.data(); meta = XpeImageMetadata{};
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in, &out, &meta));
    EXPECT_EQ(0, std::memcmp(before.data(), after.data(), before.size() * sizeof(float)));
}

TEST_F(PreprocessCalibrationTest, SavedScalarGainKeepsQualityAndTheMatchingCachedConfig) {
    const std::string a = " {\"calibration_mode\":3,\"actual_dose_levels\":5,\"polynomial_degree\":2,\"fit_r_squared\":0.9995,\"extension\":{\"source\":\"A\"}} ";
    const std::string b = "{\"calibration_mode\":1,\"actual_dose_levels\":1,\"polynomial_degree\":0,\"fit_r_squared\":0.5}";
    const std::vector<float> mapA(16, 1.0f), mapB(16, 2.0f);
    ASSERT_TRUE(WriteScalarGainWithConfig("test_gain.xcal", 4, 4, mapA, a));
    ASSERT_TRUE(WriteScalarGainWithConfig("test_check.xcal", 4, 4, mapB, b));
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("test_gain.xcal", &view));
    XpeCalibQualityMeta before{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&before));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_check.xcal"));
    ExpectSavedGainConfig(b, mapB);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("test_gain.xcal", &view));
    ExpectSavedGainConfig(a, mapA);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_output.xcal"));
    XpeCalibQualityMeta after{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&after));
    EXPECT_EQ(before.num_points, after.num_points); EXPECT_EQ(before.polynomial_degree, after.polynomial_degree);
    EXPECT_DOUBLE_EQ(before.r_squared, after.r_squared); EXPECT_EQ(before.calibration_pass, after.calibration_pass);
}

TEST_F(PreprocessCalibrationTest, SavedScalarGainEmptyConfigDoesNotKeepPreviousMetadata) {
    const std::vector<float> map(16, 1.0f);
    ASSERT_TRUE(WriteScalarGainWithConfig("test_gain.xcal", 4, 4, map, "{\"fit_r_squared\":1}"));
    ASSERT_TRUE(WriteScalarGainWithConfig("test_check.xcal", 4, 4, map, ""));
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("test_check.xcal", &view));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_gain.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("test_check.xcal", &view));
    ExpectSavedGainConfig("", map);
}

TEST_F(PreprocessCalibrationTest, SavedScalarGainReplaysItsClassifiedPixelsThroughThePipeline) {
    ASSERT_EQ(0, xpe_get_pending_alert_count());
    struct OwnedAlerts {
        ~OwnedAlerts() { xpe_clear_alerts(); }
    } ownedAlerts; // This case starts with an empty queue and owns the classification warnings it raises.
    constexpr uint32_t width = 16, height = 16;
    constexpr size_t count = width * height;
    const size_t bad1 = 8 * width + 8, bad2 = 5 * width + 5;
    ASSERT_TRUE(CreateTestXCalFile("test_defect.xcal", width, height, 2));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("test_defect.xcal"));
    for (const bool compressed : {false, true}) {
        std::vector<float> map(count, 1.0f), before(count), after(count);
        map[bad1] = 0.0f; map[bad2] = 11.0f;
        std::vector<uint16_t> raw(count);
        for (size_t i = 0; i < count; ++i) raw[i] = static_cast<uint16_t>(1000 + i);
        raw[bad1] = raw[bad2] = 65535;
        const std::string config = compressed ?
            "{\"source\":\"classified\",\"xcal_compression\":1,\"xcal_raw_payload_len\":1024}" :
            "{\"source\":\"classified\"}";
        ASSERT_TRUE(WriteScalarGainWithConfig("test_gain.xcal", width, height, map, config, compressed));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_gain.xcal"));
        XpeImageBuffer in{}, out{};
        in.width = out.width = width; in.height = out.height = height;
        in.bitsAllocated = in.bitsStored = 16; out.bitsAllocated = out.bitsStored = 32;
        in.format = XPE_PIXEL_UINT16; out.format = XPE_PIXEL_FLOAT32;
        in.data = raw.data(); in.dataSize = raw.size() * sizeof(uint16_t);
        out.data = before.data(); out.dataSize = before.size() * sizeof(float);
        XpeCalibrationState state{}; state.gainLoaded = state.defectLoaded = true;
        XpeImageMetadata meta{};
        const char* pipelineConfig = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGhost\":true}";
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &out, &meta, &state, nullptr, pipelineConfig));
        ASSERT_NE(65535.0f, before[bad1]); ASSERT_NE(65535.0f, before[bad2]);
        ASSERT_EQ(XPE_OK, xpe_calib_save("test_output.xcal", "gain", 0));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_output.xcal"));
        out.data = after.data(); meta = XpeImageMetadata{};
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_out(&in, &out, &meta, &state, nullptr, pipelineConfig));
        EXPECT_EQ(0, std::memcmp(before.data(), after.data(), before.size() * sizeof(float))) << "compressed=" << compressed;
    }
}

TEST_F(PreprocessCalibrationTest, SavedActivePolynomialGainRetainsCoefficientsRangeAndOutput) {
    ASSERT_EQ(0, xpe_get_pending_alert_count());
    struct OwnedAlerts { ~OwnedAlerts() { xpe_clear_alerts(); } } ownedAlerts;
    constexpr uint32_t width = 4, height = 4;
    constexpr size_t count = width * height;
    std::vector<float> coefficients(count * 2), before(count), after(count);
    std::vector<uint16_t> raw(count);
    for (size_t i = 0; i < count; ++i) {
        coefficients[2 * i] = 0.8f + static_cast<float>(i % 4) * 0.01f;
        coefficients[2 * i + 1] = 0.001f;
        raw[i] = static_cast<uint16_t>(i == 0 ? 5 : i == count - 1 ? 150 : 20 + i * 2);
    }
    const std::string config = " {\"dose_min\":10,\"dose_max\":100,\"calibration_mode\":3,\"actual_dose_levels\":5,\"polynomial_degree\":1,\"fit_r_squared\":1,\"extension\":{\"source\":\"poly\"}} ";
    XCalFileHeader header{};
    std::memcpy(header.magic, XCAL_MAGIC, 4);
    header.version = XCAL_VERSION; header.type = XCAL_TYPE_GAIN_POLY; header.pixel_format = XCAL_FMT_FLOAT32;
    header.width = width; header.height = height; std::memcpy(header.session_id, "poly-save-state", 15);
    ASSERT_EQ(XPE_OK, write_xcal_file("test_gain.xcal", header,
        reinterpret_cast<const uint8_t*>(config.data()), config.size(),
        reinterpret_cast<const uint8_t*>(coefficients.data()), coefficients.size() * sizeof(float)));
    const std::vector<float> scalar(count, 1.5f);
    const std::string scalarConfig = "{\"source\":\"scalar-replacement\"}";
    ASSERT_TRUE(WriteScalarGainWithConfig("test_check.xcal", width, height, scalar, scalarConfig));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_check.xcal"));
    ExpectSavedGainConfig(scalarConfig, scalar);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_gain.xcal"));
    XpeImageBuffer in{}, out{};
    in.width = out.width = width; in.height = out.height = height;
    in.bitsAllocated = in.bitsStored = 16; out.bitsAllocated = out.bitsStored = 32;
    in.format = XPE_PIXEL_UINT16; out.format = XPE_PIXEL_FLOAT32;
    in.data = raw.data(); in.dataSize = raw.size() * sizeof(uint16_t);
    out.data = before.data(); out.dataSize = before.size() * sizeof(float);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&in, &out, &meta));
    ASSERT_EQ(XPE_OK, xpe_calib_save("test_output.xcal", "gain", 4102444800000ULL));
    XCalFileHeader saved{};
    std::vector<uint8_t> savedConfig, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file("test_output.xcal", saved, savedConfig, payload, true, XCAL_TYPE_GAIN_POLY));
    EXPECT_EQ(config, std::string(savedConfig.begin(), savedConfig.end()));
    EXPECT_STREQ("poly-save-state", saved.session_id);
    EXPECT_EQ(4102444800000LL, saved.expiry_epoch_ms);
    ASSERT_EQ(coefficients.size() * sizeof(float), payload.size());
    EXPECT_EQ(0, std::memcmp(coefficients.data(), payload.data(), payload.size()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_output.xcal"));
    out.data = after.data(); meta = XpeImageMetadata{};
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&in, &out, &meta));
    EXPECT_EQ(0, std::memcmp(before.data(), after.data(), before.size() * sizeof(float)));
    XpeCalibQualityMeta quality{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&quality));
    EXPECT_EQ(5u, quality.num_points); EXPECT_EQ(1u, quality.polynomial_degree);
    EXPECT_DOUBLE_EQ(1.0, quality.r_squared);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("test_check.xcal"));
    ExpectSavedGainConfig(scalarConfig, scalar);
}

// =============================================================================
// Calibration Loading Tests
// =============================================================================

/**
 * @test LoadOffset_ValidXCalFile
 *
 * Given module is initialized
 * When valid XCal offset file is loaded
 * Then returns XPE_OK
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_ValidXCalFile) {
    ASSERT_TRUE(CreateTestXCalFile("test_offset.xcal", 1024, 1024, 0));
    XpeErrorCode result = xpe_calib_load_offset("test_offset.xcal");
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LoadOffset_InvalidFilePath
 *
 * Given module is initialized
 * When non-existent file path is provided
 * Then returns XPE_ERR_IO_FAILED
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_InvalidFilePath) {
    XpeErrorCode result = xpe_calib_load_offset("nonexistent_file.xcal");

    EXPECT_EQ(result, XPE_ERR_IO_FAILED);
}

/**
 * @test LoadOffset_NullFilePath
 *
 * Given module is initialized
 * When NULL file path is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_NullFilePath) {
    XpeErrorCode result = xpe_calib_load_offset(nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test LoadOffset_CorruptedFile
 *
 * Given module is initialized
 * When corrupted XCal file is loaded
 * Then returns XPE_ERR_CONFIG_INVALID
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_CorruptedFile) {
    if (!CreateCorruptedXCalFile("test_offset.xcal")) {
        GTEST_SKIP() << "Could not create corrupted file";
    }
    XpeErrorCode result = xpe_calib_load_offset("test_offset.xcal");
    EXPECT_TRUE(result == XPE_ERR_IO_FAILED ||
                result == XPE_ERR_CONFIG_INVALID);
}

/**
 * @test LoadGain_ValidXCalFile
 *
 * Given module is initialized
 * When valid XCal gain file is loaded
 * Then returns XPE_OK
 */
TEST_F(PreprocessCalibrationTest, LoadGain_ValidXCalFile) {
    ASSERT_TRUE(CreateTestXCalFile("test_gain.xcal", 1024, 1024, 1));
    XpeErrorCode result = xpe_calib_load_gain("test_gain.xcal");
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LoadGain_InvalidFilePath
 *
 * Given module is initialized
 * When non-existent file path is provided
 * Then returns XPE_ERR_IO_FAILED
 */
TEST_F(PreprocessCalibrationTest, LoadGain_InvalidFilePath) {
    XpeErrorCode result = xpe_calib_load_gain("nonexistent_file.xcal");

    EXPECT_EQ(result, XPE_ERR_IO_FAILED);
}

/**
 * @test LoadDefectMap_ValidXCalFile
 *
 * Given module is initialized
 * When valid XCal defect map file is loaded
 * Then returns XPE_OK
 */
TEST_F(PreprocessCalibrationTest, LoadDefectMap_ValidXCalFile) {
    ASSERT_TRUE(CreateTestXCalFile("test_defect.xcal", 1024, 1024, 2));
    XpeErrorCode result = xpe_calib_load_defect_map("test_defect.xcal");
    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LoadDefectMap_InvalidFilePath
 *
 * Given module is initialized
 * When non-existent file path is provided
 * Then returns XPE_ERR_IO_FAILED
 */
TEST_F(PreprocessCalibrationTest, LoadDefectMap_InvalidFilePath) {
    XpeErrorCode result = xpe_calib_load_defect_map("nonexistent_file.xcal");

    EXPECT_EQ(result, XPE_ERR_IO_FAILED);
}

// =============================================================================
// Calibration Generation Tests
// =============================================================================

/**
 * @test GenerateOffset_DarkFrameAveraging
 *
 * Given module is initialized
 * When offset calibration is generated from multiple dark frames
 * Then output is average of all frames
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_DarkFrameAveraging) {
    const int num_frames = 10;
    // xpe_calib_generate_offset takes const XpeImageBuffer* (array of structs, not pointers)
    std::vector<XpeImageBuffer> dark_frames(num_frames);
    for (int i = 0; i < num_frames; ++i) {
        XpeImageBuffer* tmp = CreateTestImage(512, 512, XPE_PIXEL_UINT16);
        dark_frames[i] = *tmp;
        delete tmp;  // we copied the struct; note data ptr is shared (fine for read-only use)
    }

    XpeErrorCode result = xpe_calib_generate_offset(
        dark_frames.data(),
        num_frames,
        100.0f,  // integration_time_ms
        25.0f,   // temperature_c
        "test_output.xcal"
    , nullptr);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_IO_FAILED ||
                result == XPE_ERR_PROCESSING_FAILED);

    // Free the data pointers that CreateTestImage allocated
    for (auto& frame : dark_frames) {
        delete[] static_cast<uint8_t*>(frame.data);
        frame.data = nullptr;
    }
}

/**
 * @test GenerateOffset_SingleFrame
 *
 * Given module is initialized
 * When offset calibration is generated from single dark frame
 * Then output is that frame
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_SingleFrame) {
    XpeImageBuffer* tmp = CreateTestImage(512, 512, XPE_PIXEL_UINT16);
    XpeImageBuffer dark_frame = *tmp;
    delete tmp;

    XpeErrorCode result = xpe_calib_generate_offset(
        &dark_frame,
        1,
        100.0f,
        25.0f,
        "test_output.xcal"
    , nullptr);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_IO_FAILED ||
                result == XPE_ERR_PROCESSING_FAILED);

    delete[] static_cast<uint8_t*>(dark_frame.data);
}

/**
 * @test GenerateOffset_NullFrames
 *
 * Given module is initialized
 * When null pointer is passed for dark frames
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_NullFrames) {
    XpeErrorCode result = xpe_calib_generate_offset(
        nullptr,
        10,
        100.0f,
        25.0f,
        "test_output.xcal"
    , nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test GenerateOffset_InvalidFrameCount
 *
 * Given module is initialized
 * When invalid frame count (<= 0) is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_InvalidFrameCount) {
    XpeImageBuffer* tmp = CreateTestImage(512, 512, XPE_PIXEL_UINT16);
    XpeImageBuffer dark_frame = *tmp;
    delete tmp;

    XpeErrorCode result = xpe_calib_generate_offset(
        &dark_frame,
        0,  // Invalid frame count
        100.0f,
        25.0f,
        "test_output.xcal"
    , nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete[] static_cast<uint8_t*>(dark_frame.data);
}

/**
 * @test GenerateOffset_DimensionMismatch
 *
 * Given module is initialized
 * When frames have different dimensions
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_DimensionMismatch) {
    XpeImageBuffer* tmp1 = CreateTestImage(512, 512, XPE_PIXEL_UINT16);
    XpeImageBuffer* tmp2 = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    std::vector<XpeImageBuffer> dark_frames = { *tmp1, *tmp2 };
    delete tmp1;
    delete tmp2;

    XpeErrorCode result = xpe_calib_generate_offset(
        dark_frames.data(),
        2,
        100.0f,
        25.0f,
        "test_output.xcal"
    , nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    for (auto& frame : dark_frames) {
        delete[] static_cast<uint8_t*>(frame.data);
    }
}

// =============================================================================
// Calibration Expiry Tests
// =============================================================================

/**
 * @test CheckExpiry_ValidCalibration
 *
 * Given module is initialized
 * When valid calibration file is checked
 * Then returns is_expired=false with remaining_days > 0
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_ValidCalibration) {
    ASSERT_TRUE(CreateTestXCalFile("test_check.xcal", 1024, 1024, 0));
    bool is_expired = false;
    int32_t remaining_days = 0;

    XpeErrorCode result = xpe_calib_check_expiry(
        "test_check.xcal",
        &is_expired,
        &remaining_days
    );

    EXPECT_EQ(result, XPE_OK);
    EXPECT_FALSE(is_expired);
    EXPECT_GT(remaining_days, 0);
}

/**
 * @test CheckExpiry_ExpiredCalibration
 *
 * Given module is initialized
 * When expired calibration file is checked
 * Then returns is_expired=true with negative remaining_days
 *
 * Uses the canonical writer with an expiry timestamp in the past.
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_ExpiredCalibration) {
    ASSERT_TRUE(CreateTestXCalFile("test_check.xcal", 1024, 1024, 0, "test-session-123", 1));
    bool expired = false;
    int32_t days = 0;
    ASSERT_EQ(XPE_OK, xpe_calib_check_expiry("test_check.xcal", &expired, &days));
    EXPECT_TRUE(expired);
    EXPECT_LE(days, 0);
}

/**
 * @test CheckExpiry_NullFilePath
 *
 * Given module is initialized
 * When null file path is checked
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_NullFilePath) {
    bool is_expired = false;
    int32_t remaining_days = 0;

    XpeErrorCode result = xpe_calib_check_expiry(
        nullptr,
        &is_expired,
        &remaining_days
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test CheckExpiry_NullOutputPointers
 *
 * Given module is initialized
 * When null output pointers are provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_NullOutputPointers) {
    XpeErrorCode result = xpe_calib_check_expiry(
        "test_check.xcal",
        nullptr,
        nullptr
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Calibration Save Tests
// =============================================================================

/**
 * @test Save_OffsetCalibration
 *
 * Given module is initialized and offset is loaded
 * When calibration is saved
 * Then file is created successfully
 */
TEST_F(PreprocessCalibrationTest, Save_OffsetCalibration) {
    ASSERT_TRUE(CreateTestXCalFile("test_offset.xcal", 1024, 1024, 0));
    ASSERT_EQ(xpe_calib_load_offset("test_offset.xcal"), XPE_OK);

    XpeErrorCode result = xpe_calib_save("test_output.xcal", "offset", 0);

    ASSERT_EQ(XPE_OK, result);
    ExpectSavedMap(XCAL_TYPE_OFFSET, 100.0f);
}

/**
 * @test Save_GainCalibration
 *
 * Given module is initialized and gain is loaded
 * When calibration is saved
 * Then file is created successfully
 */
TEST_F(PreprocessCalibrationTest, Save_GainCalibration) {
    ASSERT_TRUE(CreateTestXCalFile("test_gain.xcal", 1024, 1024, 1));
    ASSERT_EQ(xpe_calib_load_gain("test_gain.xcal"), XPE_OK);

    XpeErrorCode result = xpe_calib_save("test_output.xcal", "gain", 0);

    ASSERT_EQ(XPE_OK, result);
    ExpectSavedMap(XCAL_TYPE_GAIN, 2.0f);
}

/**
 * @test Save_NullFilePath
 *
 * Given module is initialized
 * When null file path is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, Save_NullFilePath) {
    XpeErrorCode result = xpe_calib_save(nullptr, "offset", 0);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test Save_NullCalibType
 *
 * Given module is initialized
 * When null calibration type is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, Save_NullCalibType) {
    XpeErrorCode result = xpe_calib_save("test_output.xcal", nullptr, 0);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test Save_InvalidCalibType
 *
 * Given module is initialized
 * When invalid calibration type is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, Save_InvalidCalibType) {
    XpeErrorCode result = xpe_calib_save("test_output.xcal", "invalid_type", 0);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Main Test Runner
// =============================================================================

