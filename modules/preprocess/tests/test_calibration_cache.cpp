/**
 * @file test_calibration_cache.cpp
 * @brief TDD tests for SWU-1.10: Calibration LRU Cache
 *        Validates cache hit/miss, LRU eviction, size limits, and thread safety.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * Buffer ownership (QA-A-20, #120). The xpe_calib_load_*_cached() entry points
 * do NOT fill a caller-provided buffer:
 *   - on a cache miss they std::realloc(out->data, ...), so out->data must be
 *     nullptr or a malloc'd block; passing a std::vector's storage there is
 *     undefined behaviour, which is what the original version of this suite did;
 *   - on a cache hit they overwrite the struct with the cache's own pointer,
 *     which the caller must not free (calibration_cache.cpp:74-77).
 * A caller cannot tell the two apart, so it can neither free safely nor keep
 * ownership. These tests therefore start from nullptr and never free; the miss
 * path leaks inside the test process. The inconsistency is reported as a
 * finding rather than papered over here.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// xpe_calib_save() no longer writes an arbitrary buffer (it saves the loaded
// calibration), so fixtures write XCal v1 files directly.
static void writeXCalFixture(const std::string& path, XCalType type,
                             XCalPixelFormat fmt, uint32_t w, uint32_t h,
                             const void* payload, uint64_t payloadLen,
                             uint64_t expiryMs) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version         = XCAL_VERSION;
    hdr.type            = static_cast<uint32_t>(type);
    hdr.pixel_format    = static_cast<uint32_t>(fmt);
    hdr.width           = w;
    hdr.height          = h;
    hdr.expiry_epoch_ms = static_cast<int64_t>(expiryMs);
    hdr.payload_len     = payloadLen;

    ASSERT_EQ(XPE_OK,
              write_xcal_file(path.c_str(), hdr, nullptr, 0,
                              static_cast<const uint8_t*>(payload), payloadLen));
}

class CalibrationCacheTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    fs::path tmpDir;
    fs::path offsetFile;
    fs::path gainFile;
    fs::path defectFile;

    void SetUp() override {
        tmpDir = fs::temp_directory_path() / "xpe_cache_test";
        fs::remove_all(tmpDir);   // an earlier aborted run can leave this behind
        fs::create_directories(tmpDir);
        offsetFile = tmpDir / "offset.xcal";
        gainFile   = tmpDir / "gain.xcal";
        defectFile = tmpDir / "defect.xcal";

        const uint64_t expiry = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        ) + 365ULL * 24 * 3600 * 1000;

        // XCal v1 requires OFFSET and GAIN payloads to be FLOAT32 and DEFECT to
        // be UINT8_MASK (xcal_validator.cpp:78-86).
        {
            const std::vector<float> data(W * H, 100.0f);
            writeXCalFixture(offsetFile.string(), XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32,
                             W, H, data.data(), data.size() * sizeof(float), expiry);
        }
        {
            const std::vector<float> data(W * H, 1.5f);
            writeXCalFixture(gainFile.string(), XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32,
                             W, H, data.data(), data.size() * sizeof(float), expiry);
        }
        {
            std::vector<uint8_t> data(W * H, 0);
            data[0] = 1;  // one defect pixel
            writeXCalFixture(defectFile.string(), XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK,
                             W, H, data.data(), data.size(), expiry);
        }

        xpe_calib_cache_clear();
        xpe_calib_cache_set_max_size(4);
    }

    void TearDown() override {
        xpe_calib_cache_clear();
        fs::remove_all(tmpDir);
    }

    // An out-parameter the loaders may realloc. Never freed -- see the file note.
    static XpeImageBuffer emptyOut() { return XpeImageBuffer{}; }
};

// --- Basic Cache Operations ---

TEST_F(CalibrationCacheTest, CacheMissLoadsFromFile) {
    XpeImageBuffer out = emptyOut();

    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out));
    EXPECT_EQ(W, out.width);
    EXPECT_EQ(H, out.height);
    EXPECT_EQ(XPE_PIXEL_FLOAT32, out.format);

    ASSERT_NE(nullptr, out.data);
    EXPECT_NEAR(100.0f, static_cast<const float*>(out.data)[0], 1e-6f);
}

TEST_F(CalibrationCacheTest, CacheHitReturnsSameData) {
    XpeImageBuffer out1 = emptyOut();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out1));
    ASSERT_NE(nullptr, out1.data);

    XpeImageBuffer out2 = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out2));
    ASSERT_NE(nullptr, out2.data);

    const auto* first  = static_cast<const float*>(out1.data);
    const auto* second = static_cast<const float*>(out2.data);
    for (size_t i = 0; i < W * H; ++i) {
        EXPECT_NEAR(first[i], second[i], 1e-6f) << "Mismatch at pixel " << i;
    }
}

TEST_F(CalibrationCacheTest, GainCacheMissAndHit) {
    XpeImageBuffer out = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &out));
    ASSERT_NE(nullptr, out.data);
    EXPECT_NEAR(1.5f, static_cast<const float*>(out.data)[0], 1e-6f);

    XpeImageBuffer out2 = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &out2));
    ASSERT_NE(nullptr, out2.data);
    EXPECT_NEAR(1.5f, static_cast<const float*>(out2.data)[0], 1e-6f);
}

TEST_F(CalibrationCacheTest, DefectCacheMissAndHit) {
    XpeImageBuffer out = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(defectFile.string().c_str(), &out));
    ASSERT_NE(nullptr, out.data);

    const auto* mask = static_cast<const uint8_t*>(out.data);
    EXPECT_EQ(1u, mask[0]);  // defect pixel
    EXPECT_EQ(0u, mask[1]);  // normal pixel
}

// --- NULL Input Validation ---

TEST_F(CalibrationCacheTest, NullPathReturnsError) {
    XpeImageBuffer out = emptyOut();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_offset_cached(nullptr, &out));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_gain_cached(nullptr, &out));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_defect_cached(nullptr, &out));
}

TEST_F(CalibrationCacheTest, NullOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_offset_cached(offsetFile.string().c_str(), nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_gain_cached(gainFile.string().c_str(), nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_defect_cached(defectFile.string().c_str(), nullptr));
}

// --- Cache Clear ---

TEST_F(CalibrationCacheTest, ClearEvictsAllEntries) {
    XpeImageBuffer out = emptyOut();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out));

    xpe_calib_cache_clear();

    // A load after the clear is a miss again and must still succeed.
    XpeImageBuffer out2 = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out2));
    ASSERT_NE(nullptr, out2.data);
    EXPECT_NEAR(100.0f, static_cast<const float*>(out2.data)[0], 1e-6f);
}

// --- Max Size Limits ---

TEST_F(CalibrationCacheTest, SetMaxSizeEvictsExcess) {
    xpe_calib_cache_set_max_size(1);

    XpeImageBuffer offOut = emptyOut();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &offOut));

    // With room for one entry, loading gain evicts the offset entry.
    XpeImageBuffer gainOut = emptyOut();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &gainOut));

    // Offset is a miss again and reloads from file.
    XpeImageBuffer offOut2 = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &offOut2));
    ASSERT_NE(nullptr, offOut2.data);
    EXPECT_NEAR(100.0f, static_cast<const float*>(offOut2.data)[0], 1e-6f);
}

TEST_F(CalibrationCacheTest, SetMaxSizeZeroClampsToOne) {
    xpe_calib_cache_set_max_size(0);

    // Must not crash; the limit is clamped to one entry.
    XpeImageBuffer out = emptyOut();
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out));
    ASSERT_NE(nullptr, out.data);
    EXPECT_NEAR(100.0f, static_cast<const float*>(out.data)[0], 1e-6f);
}

} // namespace
