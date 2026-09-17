/**
 * @file test_pipeline_stage_values.cpp
 * @brief Pipeline stage flags and values (QA-A-104, #184)
 *
 * test_pipeline_stages.cpp checks that stages RUN (the flag appears). These
 * cases check what the flag and the output SAY:
 *  - XPE_FLAG_NONLINEARITY_CORRECTED is set only when pixels were corrected.
 *    xpe_nonlinearity_correct changes no pixel today (no LUT/polynomial is
 *    implemented, SRS-CALIB-FUNC-006), so the flag must stay clear (#184).
 *  - With offset, nonlinearity and gain enabled, the frame that reaches the
 *    gain stage is the offset-corrected frame, and the pipeline returns
 *    (raw - offset) / gain. Expected values are computed here by hand, not by
 *    another run of the pipeline.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;
constexpr float    kRaw = 1000.0f, kOffset = 100.0f, kGain = 2.0f;

class PipelineStageValueTest : public ::testing::Test {
protected:
    // Room for the float32 result: the pipeline writes it back into img.
    std::vector<float>  storage;
    XpeImageBuffer      img{};
    XpeImageMetadata    meta{};
    fs::path            dir;

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        dir = fs::temp_directory_path() / "xpe_pipeline_stage_values";
        fs::remove_all(dir);
        fs::create_directories(dir);
        writeMap("offset.xcal", XCAL_TYPE_OFFSET, kOffset);
        writeMap("gain.xcal", XCAL_TYPE_GAIN, kGain);
        {   // the pipeline loads defect.xcal with the other two; no defects
            const std::vector<uint8_t> mask(N, 0);
            XCalFileHeader hdr{};
            std::memcpy(hdr.magic, XCAL_MAGIC, 4);
            hdr.version = XCAL_VERSION;
            hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
            hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
            hdr.width = W; hdr.height = H;
            hdr.payload_len = N;
            ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr,
                                              nullptr, 0, mask.data(), N));
        }

        storage.assign(N, 0.0f);
        auto* u = reinterpret_cast<uint16_t*>(storage.data());
        for (size_t i = 0; i < N; ++i) u[i] = static_cast<uint16_t>(kRaw);
        img.data = storage.data();
        img.width = W; img.height = H;
        img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16;
        img.dataSize = N * sizeof(float);
        meta = XpeImageMetadata{};
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    void writeMap(const char* name, XCalType type, float value) {
        const std::vector<float> data(N, value);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / name).string().c_str(), hdr, nullptr, 0,
                                          reinterpret_cast<const uint8_t*>(data.data()),
                                          hdr.payload_len));
    }

    std::string calib() const { return dir.string(); }
};

// Readout and temperature are bypassed: the temperature stage copies
// img->dataSize bytes into a W*H uint16 buffer, and this img is float-sized.
constexpr const char* kBase =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassBinning\":true,"
    "\"bypassDefect\":true,\"bypassGhost\":true";

}  // namespace

TEST_F(PipelineStageValueTest, NonlinearityWithoutCorrectionLeavesTheFlagClear) {
    const std::string cfg = std::string(kBase) +
        ",\"bypassOffset\":true,\"bypassGain\":true}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, cfg.c_str()));
    EXPECT_FALSE(meta.flags & XPE_FLAG_NONLINEARITY_CORRECTED)
        << "no pixel was corrected, so the frame must not be marked corrected (#184)";
}

TEST_F(PipelineStageValueTest, OffsetThenNonlinearityThenGainReturnsTheCorrectedFrame) {
    const std::string cfg = std::string(kBase) + "}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, calib().c_str(), nullptr, cfg.c_str()));
    EXPECT_TRUE(meta.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GAIN_CORRECTED);
    EXPECT_FALSE(meta.flags & XPE_FLAG_NONLINEARITY_CORRECTED);
    ASSERT_EQ(XPE_PIXEL_FLOAT32, img.format);
    const float expected = (kRaw - kOffset) / kGain;   // 450
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FLOAT_EQ(expected, storage[i]) << "pixel " << i;
    }
}

TEST_F(PipelineStageValueTest, BypassingNonlinearityGivesTheSameFrame) {
    const std::string cfg = std::string(kBase) + ",\"bypassNonlinearity\":true}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, calib().c_str(), nullptr, cfg.c_str()));
    const float expected = (kRaw - kOffset) / kGain;
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FLOAT_EQ(expected, storage[i]) << "pixel " << i;
    }
}

// Ghost correction runs in place on the stage-6 frame. A handle that has seen
// no previous frame has no lag to remove, so the frame must come back with the
// same values (and must not be replaced by an empty buffer).
TEST_F(PipelineStageValueTest, GhostStageReturnsTheFrameItCorrected) {
    void* gh = nullptr;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &gh));
    const std::string cfg =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassBinning\":true,"
        "\"bypassDefect\":true,\"bypassNonlinearity\":true}";
    meta.acquisitionTime = 1700000000;
    const XpeErrorCode rc =
        xpe_preprocess_pipeline(&img, &meta, calib().c_str(), gh, cfg.c_str());
    xpe_ghost_destroy(gh);
    ASSERT_EQ(XPE_OK, rc);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GHOST_CORRECTED);
    const float expected = (kRaw - kOffset) / kGain;
    for (size_t i = 0; i < N; ++i) {
        ASSERT_NEAR(expected, storage[i], 1e-3f) << "pixel " << i;
    }
}

// Binning mode 2 scales float32 pixels by 1/mode^2 (binning_correct.cpp:14);
// the defect stage must receive that frame.
TEST_F(PipelineStageValueTest, BinningStageReturnsTheBinnedFrame) {
    const std::string cfg =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassNonlinearity\":true,"
        "\"bypassGhost\":true,\"binningMode\":2}";
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, calib().c_str(), nullptr, cfg.c_str()));
    EXPECT_TRUE(meta.flags & XPE_FLAG_BINNING_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_DEFECT_CORRECTED);
    const float expected = (kRaw - kOffset) / kGain / 4.0f;   // 112.5
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FLOAT_EQ(expected, storage[i]) << "pixel " << i;
    }
}

// ---------------------------------------------------------------------------
// #185: every combination of nonlinearity on/off x ghost handle yes/no x defect
// map with/without a defect, on a NON-uniform frame so a zeroed or shifted
// buffer cannot pass by accident.
//
// Why the existing value checks missed #185: PipelineExTest.PipelineExWithState
// (test_pipeline_ex.cpp:209-223) and CalibFixtureGenTest.GeneratedSetDrivesThe-
// Pipeline (test_calib_fixture_gen.cpp:180-187) are the only pipeline cases that
// look at output, and both pass "bypassNonlinearity":true and
// "bypassGhost":true -- the two stages whose buffers were broken.
// test_pipeline_stages.cpp checks flags only.
//
// Expected values are computed from the formulas, not from another pipeline
// run: v(i) = (raw(i) - offset) / gain; an isolated defect is replaced by the
// mean of its four neighbours (helpers.cpp:22-36); a ghost handle that has seen
// no earlier frame leaves the frame unchanged.
// ---------------------------------------------------------------------------
namespace {

struct Combo { bool nonlin; bool ghost; bool defect; };

std::string ComboName(const ::testing::TestParamInfo<Combo>& info) {
    return std::string(info.param.nonlin ? "NonlinOn" : "NonlinOff") +
           (info.param.ghost ? "_Ghost" : "_NoGhost") +
           (info.param.defect ? "_OneDefect" : "_NoDefect");
}

class PipelineComboTest : public PipelineStageValueTest,
                          public ::testing::WithParamInterface<Combo> {};

constexpr size_t kDefectIdx = 3 * W + 3;   // interior pixel (3,3)

float RawAt(size_t i) { return 1000.0f + 10.0f * static_cast<float>(i); }
float CorrectedAt(size_t i) { return (RawAt(i) - kOffset) / kGain; }

}  // namespace

TEST_P(PipelineComboTest, OutputMatchesTheFormula) {
    const Combo c = GetParam();
    auto* u = reinterpret_cast<uint16_t*>(storage.data());
    for (size_t i = 0; i < N; ++i) u[i] = static_cast<uint16_t>(RawAt(i));
    if (c.defect) {   // replace the defect-free map written by SetUp
        std::vector<uint8_t> mask(N, 0);
        mask[kDefectIdx] = 1;
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr,
                                          nullptr, 0, mask.data(), N));
    }
    void* gh = nullptr;
    if (c.ghost) ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &gh));
    std::string cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassBinning\":true";
    if (!c.nonlin) cfg += ",\"bypassNonlinearity\":true";
    cfg += "}";
    meta.acquisitionTime = 1700000000;
    const XpeErrorCode rc =
        xpe_preprocess_pipeline(&img, &meta, calib().c_str(), gh, cfg.c_str());
    if (gh) xpe_ghost_destroy(gh);
    ASSERT_EQ(XPE_OK, rc);

    EXPECT_EQ(c.ghost, (meta.flags & XPE_FLAG_GHOST_CORRECTED) != 0);
    EXPECT_TRUE(meta.flags & XPE_FLAG_DEFECT_CORRECTED);
    EXPECT_FALSE(meta.flags & XPE_FLAG_NONLINEARITY_CORRECTED);

    const float tol = c.ghost ? 1e-3f : 0.0f;
    for (size_t i = 0; i < N; ++i) {
        float expected = CorrectedAt(i);
        if (c.defect && i == kDefectIdx) {
            expected = (CorrectedAt(i - 1) + CorrectedAt(i + 1) +
                        CorrectedAt(i - W) + CorrectedAt(i + W)) / 4.0f;
        }
        ASSERT_NEAR(expected, storage[i], tol) << "pixel " << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations, PipelineComboTest,
    ::testing::Values(Combo{false, false, false}, Combo{false, false, true},
                      Combo{false, true, false},  Combo{false, true, true},
                      Combo{true, false, false},  Combo{true, false, true},
                      Combo{true, true, false},   Combo{true, true, true}),
    ComboName);

// The first frame on a fresh handle is not changed by ghost correction, so it
// cannot tell a corrected buffer from an uncorrected copy. Two frames can: the
// second carries lag from the first. The reference is xpe_ghost_correct called
// directly on hand-computed stage-6 frames with a second handle -- not another
// pipeline run.
TEST_F(PipelineStageValueTest, GhostCorrectionOfTheSecondFrameReachesTheOutput) {
    void* gh = nullptr;
    void* ref = nullptr;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &gh));
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &ref));
    const std::string cfg =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassBinning\":true,"
        "\"bypassDefect\":true,\"bypassNonlinearity\":true}";
    const uint16_t raws[2] = {3000, 1000};   // bright frame, then a dimmer one
    std::vector<float> expected(N);
    for (int f = 0; f < 2; ++f) {
        auto* u = reinterpret_cast<uint16_t*>(storage.data());
        for (size_t i = 0; i < N; ++i) u[i] = raws[f];
        img.format = XPE_PIXEL_UINT16; img.bitsAllocated = 16; img.bitsStored = 16;
        meta = XpeImageMetadata{};
        meta.acquisitionTime = 1700000000 + static_cast<uint64_t>(f);
        ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, calib().c_str(), gh, cfg.c_str()));

        // reference: stage-6 value by formula, then the stage function itself
        std::fill(expected.begin(), expected.end(), (raws[f] - kOffset) / kGain);
        XpeImageBuffer rb{};
        rb.width = W; rb.height = H; rb.format = XPE_PIXEL_FLOAT32;
        rb.bitsAllocated = 32; rb.bitsStored = 32;
        rb.data = expected.data(); rb.dataSize = N * sizeof(float);
        XpeImageMetadata rm{};
        rm.acquisitionTime = meta.acquisitionTime;
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(ref, &rb, &rm));
    }
    xpe_ghost_destroy(gh);
    xpe_ghost_destroy(ref);
    // control: the correction really changed the second frame
    ASSERT_NE((raws[1] - kOffset) / kGain, expected[0])
        << "ghost correction did not change the reference frame; the case proves nothing";
    for (size_t i = 0; i < N; ++i) {
        ASSERT_FLOAT_EQ(expected[i], storage[i]) << "pixel " << i;
    }
}
