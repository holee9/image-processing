/**
 * @file test_parallel_determinism.cpp
 * @brief The pipeline's output does not depend on how many threads it uses (QA-A-296, Refs #278).
 *
 * The gain stage, the saturation step and the defect stage split their per-pixel work over threads (parallel.hpp). The split
 * must never change a pixel: the key `maxThreads` (1..16, default as many as the machine has) limits it, and the output with
 * 1, 2, 3, 8 and 16 threads and with the default must be the same bytes. The frame is sized so that every parallel path really
 * splits (a band of ~130,000 masked pixels, 1.2 million pixels), and a falsification arm shows the comparison can see a difference.
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

constexpr uint32_t W = 1200, H = 1000;
constexpr size_t   N = static_cast<size_t>(W) * H;
constexpr uint32_t kBand = 55;   // columns masked on each side: 2 * 55 * 1000 = 110,000 pixels

class ParallelDeterminismTest : public ::testing::Test {
protected:
    fs::path dir;
    size_t masked = 0;

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        dir = fs::temp_directory_path() / "xpe_parallel_determinism_test";
        fs::remove_all(dir);
        fs::create_directories(dir);
        std::vector<float> offset(N, 0.0f), gain(N);
        for (size_t i = 0; i < N; ++i) {
            const uint32_t x = static_cast<uint32_t>(i % W), y = static_cast<uint32_t>(i / W);
            gain[i] = 0.6f + 1.2f * static_cast<float>((x * 7u + y * 13u) % 101u) / 100.0f;
            if (i % 977 == 0) gain[i] = 0.01f;   // classified defective by the gain calibration: joins the mask
        }
        write("offset.xcal", XCAL_TYPE_OFFSET, offset);
        write("gain.xcal", XCAL_TYPE_GAIN, gain);
        std::vector<uint8_t> mask(N, 0);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x)
                if (x < kBand || x >= W - kBand) mask[y * W + x] = 1;
        for (uint32_t x = 100; x < 1100; ++x) { mask[300u * W + x] = 1; mask[600u * W + x] = 1; mask[601u * W + x] = 1; }   // lines (1 and 2 rows)
        for (uint32_t k = 0; k < 400; ++k) mask[(50u + (k * 2u) % 900u) * W + 200u + (k * 37u) % 800u] = 1;               // scattered pixels
        for (size_t i = 0; i < N; ++i) masked += mask[i] ? 1u : 0u;
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr, nullptr, 0, mask.data(), N));
        xpe_clear_alerts();
    }

    void TearDown() override {
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    void write(const char* name, XCalType type, const std::vector<float>& data) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / name).string().c_str(), hdr, nullptr, 0,
                                          reinterpret_cast<const uint8_t*>(data.data()), hdr.payload_len));
    }

    // one pipeline call; the float frame comes back in `out`
    XpeErrorCode run(const std::string& extra, std::vector<float>* out) {
        std::vector<float> storage(N, 0.0f);
        auto* u = reinterpret_cast<uint16_t*>(storage.data());
        for (size_t i = 0; i < N; ++i) {
            const uint32_t x = static_cast<uint32_t>(i % W), y = static_cast<uint32_t>(i / W);
            u[i] = (i % 11 == 0) ? static_cast<uint16_t>(12800 + (i % 22 == 0 ? 24 : 0))
                                 : static_cast<uint16_t>(1500 + (x * 5u + y * 3u) % 9000u);
        }
        XpeImageBuffer img{};
        img.data = storage.data(); img.width = W; img.height = H;
        img.bitsAllocated = 16; img.bitsStored = 16; img.format = XPE_PIXEL_UINT16; img.dataSize = N * sizeof(float);
        XpeImageMetadata meta{};
        const std::string cfg = std::string("{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,"
                                            "\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGhost\":true") + extra + "}";
        const std::string d = dir.string();
        const XpeErrorCode rc = xpe_preprocess_pipeline(&img, &meta, d.c_str(), nullptr, cfg.c_str());
        if (rc == XPE_OK) *out = storage;
        xpe_clear_alerts();
        return rc;
    }
};

bool sameBytes(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

}  // namespace

TEST_F(ParallelDeterminismTest, TheFrameReallyHasAMaskLargeEnoughToSplit) {
    EXPECT_GE(masked, 100000u);   // 20,000 masked pixels a thread: five or more threads
}

TEST_F(ParallelDeterminismTest, OneThreadAndManyThreadsGiveTheSameBytes) {
    std::vector<float> base;
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"rows\",\"maxThreads\":1", &base));
    for (const char* t : {"2", "3", "8", "16"}) {
        std::vector<float> got;
        ASSERT_EQ(XPE_OK, run(std::string(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"rows\",\"maxThreads\":") + t, &got));
        EXPECT_TRUE(sameBytes(base, got)) << "maxThreads " << t;
    }
    std::vector<float> dflt;
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"rows\"", &dflt));
    EXPECT_TRUE(sameBytes(base, dflt)) << "default thread count";
}

TEST_F(ParallelDeterminismTest, TheSameHoldsWithTheLineInterpolationOnColumnsAndOff) {
    for (const char* mode : {"off", "rows", "rows_cols"}) {
        std::vector<float> one, many;
        ASSERT_EQ(XPE_OK, run(std::string(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"") + mode + "\",\"maxThreads\":1", &one)) << mode;
        ASSERT_EQ(XPE_OK, run(std::string(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"") + mode + "\",\"maxThreads\":16", &many)) << mode;
        EXPECT_TRUE(sameBytes(one, many)) << mode;
    }
}

TEST_F(ParallelDeterminismTest, FalsificationTheComparisonSeesADifference) {
    std::vector<float> a, b;
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"rows\",\"maxThreads\":1", &a));
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":\"off\",\"defectLineInterpolation\":\"rows\",\"maxThreads\":1", &b));
    EXPECT_FALSE(sameBytes(a, b));
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800,\"defectLineInterpolation\":\"off\",\"maxThreads\":1", &b));
    EXPECT_FALSE(sameBytes(a, b));
}

TEST_F(ParallelDeterminismTest, ABadThreadLimitIsARefusal) {
    std::vector<float> out;
    for (const char* bad : {",\"maxThreads\":0", ",\"maxThreads\":17", ",\"maxThreads\":-1", ",\"maxThreads\":\"many\"", ",\"maxThreads\":2.5"})
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, run(std::string(",\"saturationLevel\":12800") + bad, &out)) << bad;
}
