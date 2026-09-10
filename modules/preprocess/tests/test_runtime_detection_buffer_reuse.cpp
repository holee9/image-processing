/**
 * @file test_runtime_detection_buffer_reuse.cpp
 * @brief QA-A-42 (#144): the buffer-reusing detector is bit-identical.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * QA-A-42 removed a per-pixel allocation from the runtime detection loop: the
 * two working vectors are now hoisted out and passed in. That is a performance
 * change and must be nothing else, so these cases assert equality of OUTPUT
 * rather than of timing.
 *
 * The reference side is the allocating overload -- the shape the code had
 * before the change, kept in the header for one-off callers -- driven pixel by
 * pixel from this TU. The subject side is the shipped entry point
 * xpe_defect_detect_runtime, which uses the hoisted buffers. If reuse ever
 * leaked state between pixels (a stale element, a wrong size), the two maps
 * would part company here.
 *
 * On the selection half of the #144 item, one premise correction: the median
 * was ALREADY a partial selection. ComputeMedian calls std::nth_element (with a
 * max_element over the lower half for the even case) and has done since before
 * this card -- there was no full sort to replace. The measured 42% was
 * allocator traffic alone.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "runtime_detection.h"

#include <cstdint>
#include <random>
#include <vector>

namespace {

class BufferReuseParityTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }

    /** The allocating form, run over the whole frame from this TU. */
    static std::vector<uint8_t> referenceMap(const XpeImageBuffer& img,
                                             const RuntimeDetectionConfig& cfg) {
        std::vector<uint8_t> map(static_cast<size_t>(img.width) * img.height, 0);
        for (uint32_t y = 0; y < img.height; ++y) {
            for (uint32_t x = 0; x < img.width; ++x) {
                if (xpe::preprocess::internal::DetectDefectivePixel(&img, x, y, cfg)) {
                    map[static_cast<size_t>(y) * img.width + x] = 1;
                }
            }
        }
        return map;
    }

    /** The shipped entry point, which uses the hoisted buffers. */
    static std::vector<uint8_t> shippedMap(XpeImageBuffer& img) {
        std::vector<uint8_t> map(static_cast<size_t>(img.width) * img.height, 0);
        XpeImageBuffer out{};
        out.data = map.data();
        out.width = img.width; out.height = img.height;
        out.bitsAllocated = 8; out.bitsStored = 8;
        out.format = XPE_PIXEL_UINT8;
        out.dataSize = static_cast<uint32_t>(map.size());

        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));
        return map;
    }

    static void compare(std::vector<float>& pixels, uint32_t w, uint32_t h) {
        XpeImageBuffer img{};
        img.data = pixels.data();
        img.width = w; img.height = h;
        img.bitsAllocated = 32; img.bitsStored = 32;
        img.format = XPE_PIXEL_FLOAT32;
        img.dataSize = static_cast<uint32_t>(pixels.size() * sizeof(float));

        // QA-A-43 (#143): the reference must use the SAME configuration as the
        // entry point, which now fills in a frame-wide sigma floor. The default
        // config leaves it at 0 (no floor) because only a caller that has seen
        // the whole frame can compute it -- so the reference computes it the
        // same way the entry point does. Without this the two sides would
        // differ for a reason that has nothing to do with buffer reuse.
        RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
        const float sigmaGlobal = xpe::preprocess::internal::ComputeGlobalSigma(&img);
        cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sigmaGlobal;
        cfg.globalSigmaCap   = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sigmaGlobal;

        const std::vector<uint8_t> ref = referenceMap(img, cfg);
        const std::vector<uint8_t> got = shippedMap(img);

        ASSERT_EQ(ref.size(), got.size());
        size_t mismatches = 0;
        size_t firstAt = 0;
        for (size_t i = 0; i < ref.size(); ++i) {
            if (ref[i] != got[i]) {
                if (mismatches == 0) firstAt = i;
                ++mismatches;
            }
        }
        EXPECT_EQ(0u, mismatches)
            << "first mismatch at index " << firstAt
            << " (x=" << (firstAt % w) << ", y=" << (firstAt / w) << ")";
    }
};

// Noise only: every pixel exercises a full neighbourhood, and the flagged set
// is large enough that a stale-buffer bug would show up somewhere.
TEST_F(BufferReuseParityTest, IdenticalOnAGaussianFrame) {
    constexpr uint32_t W = 256, H = 256;
    std::mt19937 rng(4242u);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    std::vector<float> pixels(static_cast<size_t>(W) * H);
    for (auto& p : pixels) p = noise(rng);

    compare(pixels, W, H);
}

// Injected transients: the flagged pixels are the ones whose buffers hold the
// most extreme values, so a leaked element would be most visible here.
TEST_F(BufferReuseParityTest, IdenticalWithInjectedTransients) {
    constexpr uint32_t W = 256, H = 256;
    std::mt19937 rng(7u);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    std::vector<float> pixels(static_cast<size_t>(W) * H);
    for (auto& p : pixels) p = noise(rng);
    for (uint32_t y = 8; y < H - 8; y += 16) {
        for (uint32_t x = 8; x < W - 8; x += 16) {
            pixels[static_cast<size_t>(y) * W + x] += 200.0f;   // 20 sigma
        }
    }

    compare(pixels, W, H);
}

// A flat field drives the MAD == 0 branch, and the borders drive the
// minimum-neighbour skip. Both are the paths where a reused buffer of the wrong
// SIZE (rather than the wrong contents) would diverge.
TEST_F(BufferReuseParityTest, IdenticalOnAFlatFieldWithBorders) {
    constexpr uint32_t W = 64, H = 64;
    std::vector<float> pixels(static_cast<size_t>(W) * H, 1000.0f);
    pixels[0] = 9000.0f;                                  // corner: skipped
    pixels[static_cast<size_t>(31) * W] = 9000.0f;        // left edge: judged
    pixels[static_cast<size_t>(31) * W + 31] = 9000.0f;   // interior: judged

    compare(pixels, W, H);
}

// A frame whose rows differ in width of neighbourhood use: the first and last
// rows exercise the truncated-window path immediately after full-window rows,
// which is exactly where a buffer that was not cleared would carry over.
TEST_F(BufferReuseParityTest, IdenticalOnANarrowFrame) {
    constexpr uint32_t W = 5, H = 400;
    std::mt19937 rng(99u);
    std::normal_distribution<float> noise(2000.0f, 5.0f);
    std::vector<float> pixels(static_cast<size_t>(W) * H);
    for (auto& p : pixels) p = noise(rng);

    compare(pixels, W, H);
}

} // namespace
