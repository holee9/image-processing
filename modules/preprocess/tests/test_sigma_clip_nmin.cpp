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
 *     is already there.
 *
 * Observation boundary (measured, not assumed): this TU compiles
 * xpe_calib_generate_offset_methods.cpp directly, because the generation
 * functions are internal and unexported. `g_calib` and `g_calib_mutex` live in
 * the DLL and are likewise unexported, so a merge that touches them cannot be
 * linked here -- an earlier draft that put the merge in the methods TU failed
 * with LNK2001 on both symbols. The merge is therefore split: the bit-level OR
 * (`or_merge_defect_bits`) stays testable here, and the store-side wrapper
 * (`merge_static_defect_mask`) lives in the DLL. The store-side half is
 * exercised only through the public entry point; see the last case and the
 * report's Gaps section.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe_calib_generate_offset_methods.hpp"

#include <cstdint>
#include <filesystem>
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
     * Runs one generation over @p values -- one frame per value, every pixel of
     * a frame holding that number -- and returns the static-defect mask.
     */
    std::vector<uint8_t> maskFor(const std::vector<uint16_t>& values, double kappa,
                                 const char* method = "sigma_clip") {
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

        xpe::preprocess::OffsetGenerationConfig config;
        config.method = (std::string(method) == "mean")
            ? xpe::preprocess::OffsetGenerationMethod::Mean
            : xpe::preprocess::OffsetGenerationMethod::SigmaClip;
        config.sigma = kappa;

        std::vector<float>   result;
        std::vector<uint8_t> mask;
        uint32_t w = 0, h = 0;
        EXPECT_EQ(XPE_OK, xpe::preprocess::generate_offset_values(
            bufs.data(), static_cast<int32_t>(bufs.size()), config,
            &result, &w, &h, &mask));
        lastResult = result;
        return mask;
    }

    std::vector<std::vector<uint16_t>> frameStore;
    std::vector<float>                 lastResult;
};

// The A-26 characterization, inverted. {100,110,105,108,500} at kappa = 1.0
// converges on |S| = 2 (the set {105,108}); N = 5 gives
// N_min = max(3, floor(5/4)) = 3, so 2 < 3 and §9.8.2.1 requires the mark.
TEST_F(SigmaClipNMinTest, PixelBelowTheFloorIsMarkedDefective) {
    const std::vector<uint8_t> mask = maskFor({100, 110, 105, 108, 500}, 1.0);

    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(1u, mask[i]) << "pixel " << i << ": |S| = 2 < N_min = 3";
    }
}

// A set that keeps every frame stays unmarked. Without this, "mark everything"
// would pass the case above.
TEST_F(SigmaClipNMinTest, PixelAboveTheFloorIsNotMarked) {
    const std::vector<uint8_t> mask = maskFor({100, 102, 104, 106, 108}, 3.0);

    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(0u, mask[i]) << "pixel " << i << ": nothing was clipped, |S| = 5";
    }
}

// The marked pixel's offset value is unchanged: §9.8.3 computes cal_map for
// every pixel with max(valid_count, 1) and never substitutes a sentinel.
// 106.5 is the mean of {105, 108}.
TEST_F(SigmaClipNMinTest, MarkingDoesNotChangeTheOffsetValue) {
    const std::vector<uint8_t> mask = maskFor({100, 110, 105, 108, 500}, 1.0);

    ASSERT_EQ(1u, mask[0]) << "precondition: this pixel is marked";
    ASSERT_EQ(N, lastResult.size());
    EXPECT_DOUBLE_EQ(106.5, static_cast<double>(lastResult[0]))
        << "the clipped mean survives the mark; marking is a separate output";
}

// The formula itself, at the three counts the card names plus the §9.8.5 edge.
//
//   N = 8   -> max(3, 2)  = 3
//   N = 16  -> max(3, 4)  = 4
//   N = 40  -> max(3, 10) = 10
//
// §9.8.5: "N < 4 -> min_frames = N". At N = 3 that is already what the formula
// gives (max(3, 0) = 3), so the clamp is observable only below 3.
TEST_F(SigmaClipNMinTest, NMinFormulaMatchesTheSpec) {
    EXPECT_EQ(3u,  xpe::preprocess::sigma_clip_min_frames(8));
    EXPECT_EQ(4u,  xpe::preprocess::sigma_clip_min_frames(16));
    EXPECT_EQ(10u, xpe::preprocess::sigma_clip_min_frames(40));

    EXPECT_EQ(3u, xpe::preprocess::sigma_clip_min_frames(3)) << "max(3, 0) = 3 = N";
    EXPECT_EQ(2u, xpe::preprocess::sigma_clip_min_frames(2)) << "9.8.5 clamp";
    EXPECT_EQ(1u, xpe::preprocess::sigma_clip_min_frames(1)) << "9.8.5 clamp";
}

// The same formula driven through the shipped generation path, above and below
// the floor at each of the three counts. Each series is `survivors` copies of
// one value plus far-apart outliers that kappa = 1.0 strips.
//
// The survivor counts are chosen from measurement, not prediction: an earlier
// draft used {16, 6} on the reasoning that six 1000s would survive ten spread
// outliers, and the run marked the pixel anyway -- with the outliers in the
// majority the mean sits among them and the 1000s are the ones clipped out.
// Where a row is meant to stay above the floor, the survivors are the clear
// majority.
TEST_F(SigmaClipNMinTest, NMinAppliedAtThreeFrameCounts) {
    struct Case { size_t n; size_t survivors; bool marked; const char* why; };
    const Case cases[] = {
        {8,  2,  true,  "N=8  -> N_min=3, |S|=2 is below"},
        {8,  4,  false, "N=8  -> N_min=3, |S|=4 is above"},
        {16, 3,  true,  "N=16 -> N_min=4, |S|=3 is below"},
        {16, 12, false, "N=16 -> N_min=4, |S|=12 is above"},
        {40, 9,  true,  "N=40 -> N_min=10, |S|=9 is below"},
        {40, 20, false, "N=40 -> N_min=10, |S|=20 is above"},
    };

    for (const Case& c : cases) {
        SCOPED_TRACE(c.why);
        std::vector<uint16_t> values(c.survivors, 1000);
        for (size_t k = c.survivors; k < c.n; ++k) {
            values.push_back(static_cast<uint16_t>(20000 + 3000 * (k - c.survivors)));
        }
        const std::vector<uint8_t> mask = maskFor(values, 1.0);
        ASSERT_EQ(N, mask.size());
        EXPECT_EQ(c.marked ? 1u : 0u, mask[0]) << c.why;
    }
}

// Methods other than sigma clipping have no N_min clause and must not produce
// marks -- the mask comes back zero-filled, never absent and never populated.
TEST_F(SigmaClipNMinTest, MeanMethodMarksNothing) {
    const std::vector<uint8_t> mask = maskFor({100, 110, 500}, 3.0, "mean");

    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) EXPECT_EQ(0u, mask[i]);
}

// --- the OR half of decision #138 (a) --------------------------------------

TEST_F(SigmaClipNMinTest, OrMergeAddsBitsWithoutClearingAnyone) {
    std::vector<uint8_t> dst = {1, 0, 1, 0};
    const std::vector<uint8_t> mask = {0, 1, 1, 0};

    const size_t newly = xpe::preprocess::or_merge_defect_bits(dst.data(), mask, 4);

    EXPECT_EQ(1u, newly) << "only pixel 1 was not already set";
    EXPECT_EQ(1u, dst[0]) << "pre-existing bit kept";
    EXPECT_EQ(1u, dst[1]) << "new bit added";
    EXPECT_EQ(1u, dst[2]) << "already set, and also in the mask";
    EXPECT_EQ(0u, dst[3]) << "in neither";
}

TEST_F(SigmaClipNMinTest, OrMergeOfAnEmptyMaskChangesNothing) {
    std::vector<uint8_t> dst = {1, 0, 1, 0};
    const std::vector<uint8_t> mask(4, 0);

    EXPECT_EQ(0u, xpe::preprocess::or_merge_defect_bits(dst.data(), mask, 4));
    EXPECT_EQ(std::vector<uint8_t>({1, 0, 1, 0}), dst);
}

TEST_F(SigmaClipNMinTest, OrMergeRejectsASizeMismatch) {
    std::vector<uint8_t> dst(4, 0);
    const std::vector<uint8_t> mask(3, 1);

    EXPECT_EQ(0u, xpe::preprocess::or_merge_defect_bits(dst.data(), mask, 4));
    EXPECT_EQ(0u, dst[0]) << "a mismatched mask must not be applied partially";
    EXPECT_EQ(0u, xpe::preprocess::or_merge_defect_bits(nullptr, mask, 3));
}

// --- the store-side half, through the public entry point --------------------

// xpe_calib_generate_offset parses a null config, so it runs the Mean method
// and marks nothing. The observable consequence is that it creates no defect
// map -- a generation run must not invent one. (Sigma clipping is not reachable
// from any shipped entry point today; recorded as a Gap.)
TEST_F(SigmaClipNMinTest, PublicEntryPointCreatesNoDefectMap) {
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

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(
        bufs.data(), static_cast<int32_t>(bufs.size()), 100.0f, 25.0f,
        (tmpDir / "offset.xcal").string().c_str(), nullptr));

    // No defect map exists, so there is nothing to save. The shipped code
    // reports XPE_ERR_INVALID_INPUT for that (xpe_calib_save.cpp:84), not
    // CALIB_NOT_LOADED -- measured, not assumed.
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save((tmpDir / "none.xcal").string().c_str(), "defect", 0));
}

} // namespace
