/**
 * @file test_runtime_detection_thread_parity.cpp
 * @brief QA-A-61 (#144): N threads produce bit-identical results to 1 thread.
 *
 * QA-A-58 measured that the global sigma stage splits, and supported "splitting
 * gives the same value" with nothing stronger than five runs agreeing. It said
 * so at the time: adopting the split would need bitwise parity. This is that.
 *
 * "Identical" means BITWISE, on both outputs:
 *   - the sigma, because it sets the global floor and the floor decides flags at
 *     the boundary; and
 *   - the defect map, pixel for pixel, because a count matching is not the map
 *     matching.
 * EXPECT_FLOAT_EQ tolerates 4 ulp and is the wrong assertion for either.
 *
 * WHY IDENTITY IS EXPECTED, so a failure is read as a defect and not as "threads
 * are like that":
 *   - Each pixel's verdict reads only the frame and the config; no pixel reads
 *     another's verdict; each writes its own map byte. Row splits cannot change
 *     a value.
 *   - The sigma is computed once over the whole frame BEFORE any split. Its own
 *     parallel form sums integer histogram counts, and integer addition is exact
 *     and associative, so the merged table equals the single-threaded one.
 * Floating-point non-associativity -- the usual reason threaded numerics drift --
 * never enters: nothing here sums floats across threads.
 *
 * Re-entrancy (REQ-P1A-003): the thread count is a config field, i.e. an
 * argument. Nothing in this path is static, thread_local, or remembered between
 * calls; ConcurrentCallsWithDifferentThreadCounts checks that two callers with
 * different counts running at once still each get the single-threaded answer.
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013, REQ-P1A-003.  Refs #144 #143
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"

#include <cstring>
#include <future>
#include <random>
#include <vector>

namespace {

using xpe::preprocess::internal::ComputeGlobalSigma;
using xpe::preprocess::internal::ComputeGlobalSigmaThreaded;
using xpe::preprocess::internal::DetectFrame;

bool SameBits(float a, float b) {
    uint32_t ba = 0, bb = 0;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
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
    // Real outliers, so the map is not trivially all zero.
    for (size_t i = 97; i < f.size(); i += 523) f[i] += 120.0f;
    return f;
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

const int32_t kThreadCounts[] = {1, 2, 8, 12};

}  // namespace

TEST(ThreadParityTest, GlobalSigmaIsIdenticalAtEveryThreadCount) {
    struct Shape { uint32_t w, h; };
    const Shape shapes[] = {{512u, 512u}, {640u, 480u}, {129u, 257u}, {3u, 1024u}};

    for (const Shape& sh : shapes) {
        for (int kind = 0; kind <= 4; ++kind) {
            std::vector<float> frame = MakeFrame(sh.w, sh.h, kind, 20260916u + kind);
            XpeImageBuffer img = Wrap(frame, sh.w, sh.h);
            const float single = ComputeGlobalSigma(&img);

            for (int32_t T : kThreadCounts) {
                const float threaded = ComputeGlobalSigmaThreaded(&img, T);
                ASSERT_TRUE(SameBits(single, threaded))
                    << sh.w << "x" << sh.h << " kind " << kind << " threads " << T
                    << ": single " << single << " vs threaded " << threaded;
            }
            // Counts below 1 must behave as 1, not as "no work".
            EXPECT_TRUE(SameBits(single, ComputeGlobalSigmaThreaded(&img, 0)));
            EXPECT_TRUE(SameBits(single, ComputeGlobalSigmaThreaded(&img, -4)));
        }
    }
}

TEST(ThreadParityTest, DefectMapIsIdenticalPixelForPixelAtEveryThreadCount) {
    constexpr uint32_t kW = 640u;
    constexpr uint32_t kH = 480u;      // non-square: row splits land unevenly
    const size_t n = static_cast<size_t>(kW) * kH;

    for (int kind = 0; kind <= 4; ++kind) {
        std::vector<float> frame = MakeFrame(kW, kH, kind, 20260917u + kind);
        XpeImageBuffer img = Wrap(frame, kW, kH);

        RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
        cfg.threadCount = 1;
        std::vector<uint8_t> single(n, 0u);
        DetectFrame(&img, cfg, single.data());

        size_t flagged = 0;
        for (uint8_t v : single) if (v) ++flagged;
        ASSERT_GT(flagged, 0u) << "kind " << kind
            << ": an all-zero map would agree trivially";

        for (int32_t T : kThreadCounts) {
            cfg.threadCount = T;
            std::vector<uint8_t> threaded(n, 0u);
            DetectFrame(&img, cfg, threaded.data());

            size_t mismatches = 0;
            size_t firstBad = 0;
            for (size_t i = 0; i < n; ++i) {
                if (single[i] != threaded[i]) {
                    if (mismatches == 0) firstBad = i;
                    ++mismatches;
                }
            }
            ASSERT_EQ(0u, mismatches)
                << "kind " << kind << " threads " << T << ": " << mismatches
                << " pixels differ, first at index " << firstBad
                << " (row " << (firstBad / kW) << ", col " << (firstBad % kW) << ")"
                << "; " << flagged << " pixels flagged single-threaded";
        }
    }
}

/**
 * The thread count is an argument, not module state (REQ-P1A-003). Two callers
 * running different counts at the same time must each get the answer they would
 * have got alone -- if anything were remembered in the module, this is where it
 * would show.
 */
TEST(ThreadParityTest, ConcurrentCallsWithDifferentThreadCountsAreIndependent) {
    constexpr uint32_t kW = 320u;
    constexpr uint32_t kH = 240u;
    const size_t n = static_cast<size_t>(kW) * kH;

    std::vector<float> frameA = MakeFrame(kW, kH, 0, 20260918u);
    std::vector<float> frameB = MakeFrame(kW, kH, 2, 20260919u);
    XpeImageBuffer imgA = Wrap(frameA, kW, kH);
    XpeImageBuffer imgB = Wrap(frameB, kW, kH);

    RuntimeDetectionConfig one = RuntimeDetection_DefaultConfig();
    one.threadCount = 1;
    std::vector<uint8_t> expectA(n, 0u), expectB(n, 0u);
    DetectFrame(&imgA, one, expectA.data());
    DetectFrame(&imgB, one, expectB.data());

    auto run = [n](const XpeImageBuffer* img, int32_t threads) {
        RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
        cfg.threadCount = threads;
        std::vector<uint8_t> map(n, 0u);
        for (int rep = 0; rep < 4; ++rep) {
            std::fill(map.begin(), map.end(), static_cast<uint8_t>(0));
            DetectFrame(img, cfg, map.data());
        }
        return map;
    };

    auto futureA = std::async(std::launch::async, run, &imgA, 8);
    auto futureB = std::async(std::launch::async, run, &imgB, 3);
    const std::vector<uint8_t> gotA = futureA.get();
    const std::vector<uint8_t> gotB = futureB.get();

    EXPECT_EQ(expectA, gotA) << "8-thread caller was disturbed by the 3-thread one";
    EXPECT_EQ(expectB, gotB) << "3-thread caller was disturbed by the 8-thread one";
}

/**
 * Determinism across repeats at one thread count. A race that only sometimes
 * changes a byte would pass a single comparison; this runs the same split
 * repeatedly and requires every run to agree with the first.
 */
TEST(ThreadParityTest, RepeatedThreadedRunsAreDeterministic) {
    constexpr uint32_t kW = 512u;
    constexpr uint32_t kH = 384u;
    const size_t n = static_cast<size_t>(kW) * kH;

    std::vector<float> frame = MakeFrame(kW, kH, 1, 20260920u);
    XpeImageBuffer img = Wrap(frame, kW, kH);

    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.threadCount = 12;

    std::vector<uint8_t> first(n, 0u);
    DetectFrame(&img, cfg, first.data());
    const float firstSigma = ComputeGlobalSigmaThreaded(&img, 12);

    for (int rep = 0; rep < 8; ++rep) {
        std::vector<uint8_t> again(n, 0u);
        DetectFrame(&img, cfg, again.data());
        ASSERT_EQ(first, again) << "run " << rep << " differs from the first";
        ASSERT_TRUE(SameBits(firstSigma, ComputeGlobalSigmaThreaded(&img, 12)))
            << "sigma differed on run " << rep;
    }
}
