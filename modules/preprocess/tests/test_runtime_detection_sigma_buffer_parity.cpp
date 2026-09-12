/**
 * @file test_runtime_detection_sigma_buffer_parity.cpp
 * @brief QA-A-59 (#144): the pre-sized, index-written difference buffer produces
 *        the identical sigma the push_back version produced.
 *
 * The change this pins is a data-structure change, not an algorithm change:
 * ComputeGlobalSigma used to reserve() one std::vector and push_back ~9.4 million
 * differences per direction; it now allocates once and writes by index. The
 * values, their order, and the two selections over them are untouched.
 *
 * WHY A SEPARATE FILE FROM THE RADIX PARITY TEST. That one compares against the
 * pre-QA-A-55 implementation (std::nth_element) and runs on 512x512 SQUARE frames.
 * The two directions produce h*(w-1) and (h-1)*w differences, which are EQUAL
 * when w == h and different otherwise -- so a class of sizing mistakes is
 * invisible on square frames by construction.
 *
 * That claim was tested rather than assumed (QA-A-59 falsification, measured):
 *   - Writing the horizontal pass with stride w instead of w-1 fails BOTH suites.
 *     Square frames do catch that one; it corrupts the data on any shape.
 *   - Passing vCount where hCount belongs fails ONLY this suite. The square-only
 *     suite passes 4/4 because the two counts are the same number there.
 * So the separation earns its place, but for the second shape of mistake, not
 * for every sizing mistake -- which is why the first bullet is written down
 * instead of quietly implying this file is strictly stronger.
 *
 * "Identical" means BITWISE. The sigma sets the global floor, and the floor
 * decides flags at the boundary; EXPECT_FLOAT_EQ allows 4 ulp and would not be
 * checking the claim.
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013.  Refs #144 #143
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

namespace {

using xpe::preprocess::internal::ComputeGlobalSigma;
using xpe::preprocess::internal::SelectKthSmallest;

bool SameBits(float a, float b) {
    uint32_t ba = 0, bb = 0;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

/**
 * The implementation as it stood before QA-A-59: one std::vector grown by
 * push_back, cleared and re-grown for the second direction. Same selection
 * (QA-A-55's radix), so any difference this test finds is the buffer change.
 */
float PushBackImplementation(const std::vector<float>& px, uint32_t w, uint32_t h) {
    if (w < 2u && h < 2u) return 0.0f;
    const float* pixels = px.data();

    auto madSigma = [](std::vector<float>& d) -> float {
        if (d.empty()) return 0.0f;
        const size_t mid = d.size() / 2u;
        const float median = SelectKthSmallest(d, mid);
        for (size_t i = 0; i < d.size(); ++i) d[i] = std::abs(d[i] - median);
        return SelectKthSmallest(d, mid) * RUNTIME_DETECTION_MAD_SCALE * 0.70710678f;
    };

    std::vector<float> diff;
    diff.reserve(static_cast<size_t>(w) * h);

    float sigmaH = 0.0f;
    if (w >= 2u) {
        for (size_t y = 0; y < h; ++y) {
            const float* row = pixels + y * w;
            for (size_t x = 0; x + 1u < w; ++x) diff.push_back(row[x + 1u] - row[x]);
        }
        sigmaH = madSigma(diff);
    }

    float sigmaV = 0.0f;
    if (h >= 2u) {
        diff.clear();
        for (size_t y = 0; y + 1u < h; ++y) {
            const float* row = pixels + y * w;
            for (size_t x = 0; x < w; ++x) diff.push_back(row[x + w] - row[x]);
        }
        sigmaV = madSigma(diff);
    }

    if (sigmaH <= 0.0f) return sigmaV;
    if (sigmaV <= 0.0f) return sigmaH;
    return (sigmaH < sigmaV) ? sigmaH : sigmaV;
}

XpeImageBuffer Wrap(std::vector<float>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer img{};
    img.data = px.data();
    img.width = w;
    img.height = h;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

/** kind: 0 noise, 1 vertical step, 2 row stripes, 3 flat, 4 integer grid. */
std::vector<float> MakeFrame(uint32_t w, uint32_t h, int kind, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    std::vector<float> f(static_cast<size_t>(w) * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float v = noise(rng);
            if (kind == 1) v += (x >= w / 2u) ? 1500.0f : 0.0f;
            if (kind == 2) v += ((y % 8u) == 0u) ? 240.0f : 0.0f;
            if (kind == 3) v = 3000.0f;
            if (kind == 4) v = std::floor(v);
            f[static_cast<size_t>(y) * w + x] = v;
        }
    }
    return f;
}

}  // namespace

/**
 * NON-SQUARE is the point. h*(w-1) != (h-1)*w unless w == h, so the two
 * directions need different element counts out of one buffer -- exactly what a
 * stride or sizing mistake gets wrong, and exactly what square frames hide.
 */
TEST(SigmaBufferParityTest, NonSquareFramesMatchThePushBackImplementation) {
    struct Shape { uint32_t w, h; };
    const Shape shapes[] = {
        {640u, 480u},    // wide
        {480u, 640u},    // tall
        {257u, 129u},    // odd both, wide
        {129u, 257u},    // odd both, tall
        {1024u, 3u},     // extremely wide
        {3u, 1024u},     // extremely tall
        {512u, 512u},    // square, for contrast
    };

    for (const Shape& sh : shapes) {
        for (int kind = 0; kind <= 4; ++kind) {
            std::vector<float> frame = MakeFrame(sh.w, sh.h, kind, 20260912u + kind);
            XpeImageBuffer img = Wrap(frame, sh.w, sh.h);

            const float shipped = ComputeGlobalSigma(&img);
            const float previous = PushBackImplementation(frame, sh.w, sh.h);

            ASSERT_TRUE(SameBits(shipped, previous))
                << sh.w << "x" << sh.h << " kind " << kind
                << ": shipped " << shipped << " vs push_back " << previous;
        }
    }
}

/** One direction produces no differences at all. The buffer must still be right. */
TEST(SigmaBufferParityTest, DegenerateDimensionsMatchThePushBackImplementation) {
    struct Shape { uint32_t w, h; };
    const Shape shapes[] = {
        {1u, 1u},      // nothing in either direction
        {2u, 1u},      // horizontal only
        {1u, 2u},      // vertical only
        {2u, 2u},      // smallest with both
        {1u, 4096u},   // vertical only, long
        {4096u, 1u},   // horizontal only, long
    };

    for (const Shape& sh : shapes) {
        std::vector<float> frame = MakeFrame(sh.w, sh.h, 0, 20260913u);
        XpeImageBuffer img = Wrap(frame, sh.w, sh.h);

        const float shipped = ComputeGlobalSigma(&img);
        const float previous = PushBackImplementation(frame, sh.w, sh.h);

        EXPECT_TRUE(SameBits(shipped, previous))
            << sh.w << "x" << sh.h << ": shipped " << shipped
            << " vs push_back " << previous;
    }
}

/**
 * The buffer is reused between the two directions. If the second pass ever read
 * a value the first pass left behind, a frame whose two directions carry very
 * different noise would expose it -- the horizontal sigma would leak into the
 * vertical one. This builds exactly that frame.
 */
TEST(SigmaBufferParityTest, BufferReuseDoesNotLeakBetweenDirections) {
    constexpr uint32_t w = 600u, h = 400u;   // non-square on purpose
    const size_t n = static_cast<size_t>(w) * h;

    std::mt19937 rng(20260914u);
    std::normal_distribution<float> fine(0.0f, 1.0f);
    std::vector<float> frame(n);
    // Columns vary a lot; rows barely vary. Horizontal and vertical differences
    // then have sigmas an order of magnitude apart.
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            frame[static_cast<size_t>(y) * w + x] =
                3000.0f + 200.0f * static_cast<float>((x % 2u)) + fine(rng);
        }
    }

    XpeImageBuffer img = Wrap(frame, w, h);
    const float shipped = ComputeGlobalSigma(&img);
    const float previous = PushBackImplementation(frame, w, h);

    EXPECT_TRUE(SameBits(shipped, previous))
        << "shipped " << shipped << " vs push_back " << previous;
    // min(h, v) must pick the quiet direction; a leak would inflate it.
    EXPECT_LT(shipped, 10.0f) << "sigma " << shipped
        << " is far above the row-to-row noise -- the horizontal pass may be "
           "leaking into the vertical one";
}
