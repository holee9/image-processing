/**
 * @file test_calib_gain_poly_load.cpp
 * @brief QA-A-37 (#140): XCAL_TYPE_GAIN_POLY is readable, not just writable.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * xpe_calib_generate_gain_polynomial() writes XCAL_TYPE_GAIN_POLY (= 3) with a
 * payload of (degree+1) x W x H floats. Nothing could read it back:
 *
 *   - xcal_validator.cpp Check 3 rejected every type above XCAL_TYPE_DEFECT
 *     (= 2), so a POLY file failed validation as CONFIG_INVALID before any
 *     loader was reached;
 *   - Check 8 required payload_len == W*H*bpp exactly, which a coefficient
 *     array never satisfies;
 *   - xpe_calib_load_gain() asked the reader for XCAL_TYPE_GAIN only.
 *
 * The A-35 metadata round trip therefore only ever ran over scalar GAIN files.
 * These cases run it over the file the API actually produces.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdint>
#include <cstring>
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

    /**
     * Generates a polynomial gain file through the public API.
     *
     * Level files are tagged per invocation: write_xcal_file renames a .tmp
     * into place and a leftover from a previous call blocks it with -9
     * (IO_FAILED). Same hazard recorded in test_calib_quality_meta.cpp.
     */
    XpeErrorCode generatePoly(const std::vector<float>& values,
                              const std::string& outName,
                              int32_t maxDegree = 2) {
        const std::string tag = "g" + std::to_string(generation++) + "_";
        std::vector<std::string> paths;
        std::vector<const char*> pathPtrs;
        std::vector<double>      doses;
        for (size_t i = 0; i < values.size(); ++i) {
            paths.push_back(writeGainLevel(tag + "lvl" + std::to_string(i) + ".xcal",
                                           values[i]));
            doses.push_back(static_cast<double>(i + 1));
        }
        for (const auto& s : paths) pathPtrs.push_back(s.c_str());

        return xpe_calib_generate_gain_polynomial(
            pathPtrs.data(), doses.data(),
            static_cast<int32_t>(values.size()), maxDegree,
            (tmpDir / outName).string().c_str());
    }

    /** A hand-built POLY file with an arbitrary payload length. */
    std::string writePolyRaw(const std::string& name, uint64_t payloadLen,
                             const std::string& json = std::string()) {
        const std::string path = (tmpDir / name).string();
        std::vector<uint8_t> payload(static_cast<size_t>(payloadLen), 0);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version         = XCAL_VERSION;
        hdr.type            = static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY);
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

// The defect this card exists for: the file the API writes must be loadable.
TEST_F(GainPolyLoadTest, PolyFileWrittenByTheApiLoads) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "poly.xcal"));

    EXPECT_EQ(XPE_OK, xpe_calib_load_gain((tmpDir / "poly.xcal").string().c_str()))
        << "XCAL_TYPE_GAIN_POLY is produced by the public generator; a loader "
           "that cannot read it makes the file unreachable";
}

// The A-35 round trip, run over a POLY file instead of a scalar GAIN file.
// A perfectly linear series fits exactly, so R2 is 1 and the gate passes.
TEST_F(GainPolyLoadTest, PolyMetadataSurvivesTheRoundTrip) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "meta.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain((tmpDir / "meta.xcal").string().c_str()));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));
    EXPECT_NEAR(1.0, meta.r_squared, 1e-6);
    EXPECT_EQ(4u, meta.num_points);
    EXPECT_EQ(2u, meta.polynomial_degree);
    EXPECT_EQ(1u, meta.calibration_pass);
}

// A polynomial calibration is not a scalar map. Loading one must not leave a
// stale scalar gain map behind for xpe_gain_correct to silently apply.
TEST_F(GainPolyLoadTest, LoadingPolyClearsTheScalarGainMap) {
    const std::string scalar = writeGainLevel("scalar.xcal", 2.0f);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(scalar.c_str()));

    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "after.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain((tmpDir / "after.xcal").string().c_str()));

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
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, xpe_gain_correct(&in, &out, &meta))
        << "the scalar map from the earlier load must not survive a POLY load";
}

// The existing type-mismatch contract is unchanged: a POLY file is still not an
// offset map, and a defect map is still not a gain file.
TEST_F(GainPolyLoadTest, TypeMismatchStillReportsConfigInvalid) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "mismatch.xcal"));

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              xpe_calib_load_offset((tmpDir / "mismatch.xcal").string().c_str()));

    const std::string scalar = writeGainLevel("as_defect.xcal", 1.0f);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_map(scalar.c_str()));
}

// The payload of a POLY file is a whole number of coefficient planes. Anything
// else is a malformed file, not a degree the loader should guess at.
TEST_F(GainPolyLoadTest, PayloadThatIsNotAWholeNumberOfPlanesIsRejected) {
    const uint64_t plane = static_cast<uint64_t>(N) * sizeof(float);

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              xpe_calib_load_gain(writePolyRaw("ragged.xcal", plane + 4).c_str()));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              xpe_calib_load_gain(writePolyRaw("empty.xcal", 0).c_str()));

    // One plane is degree 0 -- a constant term only, which is well formed.
    EXPECT_EQ(XPE_OK,
              xpe_calib_load_gain(writePolyRaw("one_plane.xcal", plane).c_str()));
}

} // namespace
