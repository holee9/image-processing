/**
 * @file test_sigma_clip_nmin.cpp
 * @brief QA-A-38 (#138 #97): XPE-ALG-001 §9.8.2.1 N_min static-defect marking.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * §9.8.2.1, verbatim:
 *
 *     최소 프레임 수 제약:
 *     N_min = max(3, floor(N/4))
 *     유효 프레임 수 |S| < N_min 이면 해당 픽셀을 정적 결함으로 마킹.
 *
 * §9.8.3's reference implementation returns two arrays and computes them
 * independently:
 *
 *     cal_map     = masked_final.sum(axis=0) / np.maximum(valid_count, 1)
 *     defect_mask = valid_count < min_frames
 *
 * Two consequences this suite pins:
 *
 *  1. The marked pixel's calibration value is still the clipped mean. The
 *     reference divides by max(valid_count, 1) for EVERY pixel and never
 *     substitutes 0 for a defective one -- marking and averaging are separate
 *     outputs, so implementing the mark must not change the offset value.
 *  2. The mark is an output channel of its own. Leader decision #138 (a) routes
 *     it to the global calibration store's defect map, OR-merged with whatever
 *     is already there, and readable back through the shipped
 *     xpe_calib_save(path, "defect", expiry).
 *
 * The mask is observed through that public save path rather than through
 * g_calib, which is internal to the DLL and not exported.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "xpe_calib_generate_offset_methods.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 2, H = 2;
constexpr size_t   N = static_cast<size_t>(W) * H;

class SigmaClipNMinTest : public ::testing::Test {
protected:
    fs::path tmpDir;

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        tmpDir = fs::temp_directory_path() / "xpe_nmin";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    /**
     * Runs sigma clipping over @p values (one frame per value, every pixel of a
     * frame holding the same number) through the shipped generation path.
     */
    XpeErrorCode runSigmaClip(const std::vector<uint16_t>& values, double kappa) {
        frameStore.clear();
        frameStore.reserve(values.size());
        for (uint16_t v : values) frameStore.emplace_back(N, v);

        std::vector<XpeImageBuffer> bufs(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            XpeImageBuffer& b = bufs[i];
            b.data = frameStore[i].data();
            b.width = W; b.height = H;
            b.bitsAllocated = 16; b.bitsStored = 16;
            b.format = XPE_PIXEL_UINT16;
            b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
        }

        outPixels.assign(N, 0);
        XpeImageBuffer out{};
        out.data = outPixels.data();
        out.width = W; out.height = H;
        out.bitsAllocated = 16; out.bitsStored = 16;
        out.format = XPE_PIXEL_UINT16;
        out.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));

        const std::string cfg =
            "{\"method\":\"sigma_clip\",\"sigma\":" + std::to_string(kappa) + "}";
        return xpe::preprocess::generate_offset_to_uint16_buffer(
            bufs.data(), static_cast<int32_t>(bufs.size()), &out, cfg.c_str());
    }

    /** Saves the global defect map and returns its payload, one byte per pixel. */
    std::vector<uint8_t> savedDefectMask(const std::string& name) {
        const std::string path = (tmpDir / name).string();
        EXPECT_EQ(XPE_OK, xpe_calib_save(path.c_str(), "defect", 0));

        std::ifstream f(path, std::ios::binary);
        EXPECT_TRUE(f.is_open());
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        f.seekg(static_cast<std::streamoff>(hdr.config_json_len), std::ios::cur);
        std::vector<uint8_t> mask(static_cast<size_t>(hdr.payload_len), 0);
        if (!mask.empty()) {
            f.read(reinterpret_cast<char*>(mask.data()),
                   static_cast<std::streamsize>(mask.size()));
        }
        return mask;
    }

    /** Loads a hand-built defect map into the global store. */
    void loadDefectMap(const std::string& name, const std::vector<uint8_t>& mask) {
        const std::string path = (tmpDir / name).string();
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len  = mask.size();
        ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                          mask.data(), hdr.payload_len));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path.c_str()));
    }

    std::vector<std::vector<uint16_t>> frameStore;
    std::vector<uint16_t>              outPixels;
};

// The A-26 characterization, inverted. {100,110,105,108,500} at kappa = 1.0
// converges on |S| = 2 (the set {105,108}); N = 5 gives
// N_min = max(3, floor(5/4)) = 3, so 2 < 3 and §9.8.2.1 requires the mark.
TEST_F(SigmaClipNMinTest, PixelBelowTheFloorIsMarkedDefective) {
    ASSERT_EQ(XPE_OK, runSigmaClip({100, 110, 105, 108, 500}, 1.0));

    const std::vector<uint8_t> mask = savedDefectMask("below.xcal");
    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(1u, mask[i]) << "pixel " << i << ": |S| = 2 < N_min = 3";
    }
}

// A set that keeps every frame stays unmarked. Without this, "mark everything"
// would pass the case above.
TEST_F(SigmaClipNMinTest, PixelAboveTheFloorIsNotMarked) {
    // An all-zero map is pre-loaded so there is something to read back: a run
    // that marks nothing creates no defect map, and xpe_calib_save would have
    // nothing to write.
    loadDefectMap("zero.xcal", {0, 0, 0, 0});
    ASSERT_EQ(XPE_OK, runSigmaClip({100, 102, 104, 106, 108}, 3.0));

    const std::vector<uint8_t> mask = savedDefectMask("above.xcal");
    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(0u, mask[i]) << "pixel " << i << ": nothing was clipped, |S| = 5";
    }
}

// The marked pixel's offset value is unchanged: §9.8.3 computes cal_map for
// every pixel with max(valid_count, 1) and never substitutes a sentinel.
// 106.5 is the mean of {105, 108}; the uint16 entry point rounds it to 107.
TEST_F(SigmaClipNMinTest, MarkingDoesNotChangeTheOffsetValue) {
    ASSERT_EQ(XPE_OK, runSigmaClip({100, 110, 105, 108, 500}, 1.0));

    ASSERT_EQ(N, outPixels.size());
    EXPECT_EQ(107u, outPixels[0])
        << "the clipped mean 106.5 rounds to 107; marking is a separate output";
}

// N_min = max(3, floor(N/4)) -- three points on the formula, each driven
// through the shipped path rather than recomputed in the test.
//
//   N = 8   -> max(3, 2)  = 3
//   N = 16  -> max(3, 4)  = 4
//   N = 40  -> max(3, 10) = 10
//
// Each series is built so clipping converges on exactly `survivors` frames:
// `survivors` copies of one value plus (N - survivors) copies of a spread of
// far-apart outliers, which kappa = 1.0 strips.
TEST_F(SigmaClipNMinTest, NMinFormulaAtThreeFrameCounts) {
    struct Case { size_t n; size_t survivors; bool expectMarked; const char* why; };
    const Case cases[] = {
        {8,  2,  true,  "N=8  -> N_min=3, |S|=2 is below"},
        {8,  4,  false, "N=8  -> N_min=3, |S|=4 is above"},
        {16, 3,  true,  "N=16 -> N_min=4, |S|=3 is below"},
        {16, 6,  false, "N=16 -> N_min=4, |S|=6 is above"},
        {40, 9,  true,  "N=40 -> N_min=10, |S|=9 is below"},
        {40, 20, false, "N=40 -> N_min=10, |S|=20 is above"},
    };

    int idx = 0;
    for (const Case& c : cases) {
        SCOPED_TRACE(c.why);
        loadDefectMap("nmin_base" + std::to_string(idx) + ".xcal", {0, 0, 0, 0});
        std::vector<uint16_t> values(c.survivors, 1000);
        for (size_t k = c.survivors; k < c.n; ++k) {
            // Outliers spread far apart so no two of them cluster into a
            // surviving group of their own.
            values.push_back(static_cast<uint16_t>(20000 + 3000 * (k - c.survivors)));
        }
        ASSERT_EQ(XPE_OK, runSigmaClip(values, 1.0));

        const std::vector<uint8_t> mask =
            savedDefectMask("nmin" + std::to_string(idx++) + ".xcal");
        ASSERT_EQ(N, mask.size());
        EXPECT_EQ(c.expectMarked ? 1u : 0u, mask[0]) << c.why;
    }
}

// OR merge: a defect map already in the store keeps its bits, and the N_min
// pixels are added to it. Decision #138 (a) says merge, not replace.
TEST_F(SigmaClipNMinTest, ExistingDefectMapIsOrMergedNotReplaced) {
    // Pixel 0 already known bad; the run marks all four.
    loadDefectMap("existing.xcal", {1, 0, 0, 0});
    ASSERT_EQ(XPE_OK, runSigmaClip({100, 110, 105, 108, 500}, 1.0));

    const std::vector<uint8_t> mask = savedDefectMask("merged.xcal");
    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(1u, mask[i]) << "pixel " << i;
    }
}

// The pre-existing bit survives even when the run marks nothing -- the merge
// must never clear a bit it did not set.
TEST_F(SigmaClipNMinTest, ExistingBitsSurviveARunThatMarksNothing) {
    loadDefectMap("keep.xcal", {1, 0, 1, 0});
    ASSERT_EQ(XPE_OK, runSigmaClip({100, 102, 104, 106, 108}, 3.0));

    const std::vector<uint8_t> mask = savedDefectMask("kept.xcal");
    ASSERT_EQ(N, mask.size());
    EXPECT_EQ(1u, mask[0]);
    EXPECT_EQ(0u, mask[1]);
    EXPECT_EQ(1u, mask[2]);
    EXPECT_EQ(0u, mask[3]);
}

// Methods other than sigma clipping have no N_min clause and must not create a
// defect map out of nothing.
TEST_F(SigmaClipNMinTest, MeanMethodMarksNothing) {
    frameStore.clear();
    for (uint16_t v : {uint16_t(100), uint16_t(110), uint16_t(500)}) {
        frameStore.emplace_back(N, v);
    }
    std::vector<XpeImageBuffer> bufs(frameStore.size());
    for (size_t i = 0; i < frameStore.size(); ++i) {
        XpeImageBuffer& b = bufs[i];
        b.data = frameStore[i].data();
        b.width = W; b.height = H;
        b.bitsAllocated = 16; b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
    }
    outPixels.assign(N, 0);
    XpeImageBuffer out{};
    out.data = outPixels.data();
    out.width = W; out.height = H;
    out.bitsAllocated = 16; out.bitsStored = 16;
    out.format = XPE_PIXEL_UINT16;
    out.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));

    ASSERT_EQ(XPE_OK, xpe::preprocess::generate_offset_to_uint16_buffer(
        bufs.data(), static_cast<int32_t>(bufs.size()), &out,
        "{\"method\":\"mean\"}"));

    // Nothing was marked, so there is no defect map to save. The shipped code
    // reports XPE_ERR_INVALID_INPUT for that (xpe_calib_save.cpp:84), not
    // CALIB_NOT_LOADED -- measured, not assumed.
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save((tmpDir / "none.xcal").string().c_str(), "defect", 0));
}

} // namespace
