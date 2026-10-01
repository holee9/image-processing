/**
 * @file test_ghost_failed_frame.cpp
 * @brief A ghost-correction frame that fails changes nothing the next frame can see (QA-A-202c, #233).
 *
 * xpe_ghost_correct used to update the handle's history pixel by pixel and only afterwards check that the
 * corrected value was finite, so a frame that failed half way left the first half of its history behind --
 * and a batch, which carries on past a failed frame, handed the next frame a history that included frames
 * that were never delivered. The history, the time of the last frame and the exposure estimate are now
 * committed only when the whole frame succeeded.
 *
 * How a frame is made to fail half way: tier 3 scales its coefficient by the pixel value, and a huge
 * nlcscBeta makes the product coefficient * history overflow float for one bright pixel and for no
 * other -- but only once the history is non-zero, so every scenario starts with a seed frame.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 16, H = 16;
constexpr size_t N = static_cast<size_t>(W) * H;
constexpr size_t kBright = N / 2;                 // the pixel that overflows, in the middle of the frame
const char* const kGhostConfig = "{\"tier\":\"3\",\"nlcscBeta\":\"1e37\"}";

XpeImageBuffer floatBuf(float* d) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32; b.dataSize = N * sizeof(float);
    return b;
}

struct GhostState {
    std::vector<float> h1, h2;
    double lastAcq{0.0};
    double lastMean{0.0};
    double weight{0.0};
    bool operator==(const GhostState& o) const {
        return h1 == o.h1 && h2 == o.h2 && lastAcq == o.lastAcq && lastMean == o.lastMean && weight == o.weight;
    }
};
GhostState stateOf(void* handle) {
    auto* gh = static_cast<GhostCorrectorHandle*>(handle);
    return GhostState{gh->hist1, gh->hist2, gh->lastAcqTimeSec, gh->lastFrameMean, gh->exposureWeight};
}

class GhostFailedFrame : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        if (ghost_) xpe_ghost_destroy(ghost_);
        std::remove("gff_gain.xcal");
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }
    void newGhost() {
        if (ghost_) xpe_ghost_destroy(ghost_);
        ghost_ = nullptr;
        ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, kGhostConfig, &ghost_));
    }
    void* ghost_{nullptr};
};

}  // namespace

TEST_F(GhostFailedFrame, AStandaloneCallThatFailsLeavesTheHandleAsItWas) {
    newGhost();
    const GhostState fresh = stateOf(ghost_);

    std::vector<float> seed(N, 450.0f);
    XpeImageBuffer s = floatBuf(seed.data());
    XpeImageMetadata m{};
    m.acquisitionTime = 100;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(ghost_, &s, &m)) << "control: the seed frame is corrected";
    const GhostState seeded = stateOf(ghost_);
    ASSERT_FALSE(seeded == fresh) << "control: a successful frame changes the handle";

    std::vector<float> bad(N, 450.0f);
    bad[kBright] = 29950.0f;
    XpeImageBuffer b = floatBuf(bad.data());
    m.acquisitionTime = 101;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_ghost_correct(ghost_, &b, &m))
        << "control: the bright pixel overflows, in the middle of the frame";
    EXPECT_TRUE(stateOf(ghost_) == seeded)
        << "a failed frame must leave the history, the time of the last frame and the exposure estimate as they were";

    // And the handle still works as if the failed frame had never been offered.
    std::vector<float> next(N, 450.0f);
    XpeImageBuffer n2 = floatBuf(next.data());
    m.acquisitionTime = 102;
    EXPECT_EQ(XPE_OK, xpe_ghost_correct(ghost_, &n2, &m));
}

namespace {

void writeGain(const char* path, float v) {
    std::vector<float> g(N, v);
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = XCAL_TYPE_GAIN; hdr.pixel_format = XCAL_FMT_FLOAT32;
    hdr.width = W; hdr.height = H; hdr.payload_len = g.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>("{}"), 2,
                                      reinterpret_cast<const uint8_t*>(g.data()), g.size() * sizeof(float)));
}

// Only the gain stage and the ghost stage run: the frame is uint16 in, float32 out of the gain stage
// (a gain of 2 halves it), and the ghost stage takes it from there.
const char* const kPipelineConfig =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassBinning\":true,\"bypassDefect\":true}";

struct Frame {
    std::vector<uint8_t> bytes;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    // `px` is the uint16 pixel value of every pixel except `bright`, which is `brightPx` (0 = none).
    Frame(uint16_t px, uint64_t acq, uint16_t brightPx = 0) : bytes(N * sizeof(float), 0) {
        auto* p = reinterpret_cast<uint16_t*>(bytes.data());
        for (size_t i = 0; i < N; ++i) p[i] = px;
        if (brightPx != 0) p[kBright] = brightPx;
        img.data = bytes.data();
        img.width = W; img.height = H; img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16;
        img.dataSize = N * sizeof(float);   // room for the float result (QA-A-205)
        meta.acquisitionTime = acq;
    }
};

}  // namespace

TEST_F(GhostFailedFrame, ABatchCarriesOnPastAFailedFrameWithoutItsHistory) {
    writeGain("gff_gain.xcal", 2.0f);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("gff_gain.xcal"));

    // Reference: a seed frame, then the second frame alone -- the history the second frame should see.
    newGhost();
    Frame seedRef(900, 100);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_batch(&seedRef.img, 1, &seedRef.meta, nullptr, ghost_, kPipelineConfig));
    Frame secondRef(900, 102);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_batch(&secondRef.img, 1, &secondRef.meta, nullptr, ghost_, kPipelineConfig))
        << "control: the second frame is fine on its own";
    const GhostState after_reference = stateOf(ghost_);

    // The scenario: the same seed, then a batch of [a frame that fails half way, the same second frame].
    newGhost();
    Frame seed(900, 100);
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_batch(&seed.img, 1, &seed.meta, nullptr, ghost_, kPipelineConfig));
    Frame failing(900, 101, 59900);
    Frame second(900, 102);
    XpeImageBuffer imgs[2] = {failing.img, second.img};   // each descriptor points at its own frame's bytes
    XpeImageMetadata metas[2] = {failing.meta, second.meta};
    const XpeErrorCode rc = xpe_preprocess_pipeline_batch(imgs, 2, metas, nullptr, ghost_, kPipelineConfig);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc) << "control: the first frame fails (the batch reports the first error)";
    EXPECT_EQ(0u, metas[0].flags & XPE_FLAG_GHOST_CORRECTED) << "control: the failed frame was not ghost-corrected";
    ASSERT_NE(0u, metas[1].flags & XPE_FLAG_GHOST_CORRECTED) << "control: the batch carried on and corrected the second frame";

    EXPECT_EQ(secondRef.bytes, second.bytes)
        << "the second frame must come out as it does without the failed frame before it";
    EXPECT_TRUE(stateOf(ghost_) == after_reference)
        << "and the handle must hold the same history as the run without the failed frame";
}
