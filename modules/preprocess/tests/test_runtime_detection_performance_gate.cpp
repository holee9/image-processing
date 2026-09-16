/**
 * @file test_runtime_detection_performance_gate.cpp
 * @brief QA-A-60 (#144): machine-relative regression gate for the runtime detector.
 *
 * WHAT THIS GATE IS FOR. Not an improvement target -- a REGRESSION catcher: work
 * that silently makes the detector slower than it is today.
 *
 * WHY IT IS NO LONGER AN ABSOLUTE MILLISECOND BUDGET. QA-A-57 set 810 ms from
 * local measurements and QA-A-59 recorded, as an explicit unverified item, that
 * the CI machine had never been measured. The next CI run answered:
 *
 *     3072x3072   local 702.3 ms   CI 1340.9 ms   (1.91x)
 *     1024x1024   local  82.8 ms   CI  148.2 ms   (1.79x)
 *
 * CI samples were 1360.9 / 1363.3 / 1340.9 -- spread 1.7%, all three above the
 * gate. That is a machine, not noise. Raising the constant to 1400 would have
 * made the gate useless on a developer machine, where a 2x regression would then
 * pass. So the gate divides by a measurement of the machine taken in the same
 * process, and asserts on the RATIO.
 *
 * WHY THE REFERENCE IS A FROZEN KERNEL AND NOT THE 1024 DETECTION PATH.
 * The obvious reference is the small-frame run of the same detector, and it was
 * tried and rejected on measurements already on record:
 *
 *     3072/1024 ratio   normal (local)        702.3 /  82.8 = 8.48
 *                       same code on CI      1340.9 / 148.2 = 9.05
 *                       REAL code regression 1201.2 / 189.4 = 6.34   (QA-A-57)
 *
 * It does absorb the machine well (8.48 -> 9.05). But a genuine regression --
 * QA-A-57 bypassed the median fast path -- moves the ratio DOWN, because the
 * 1024 frame fits in L3 and is hurt relatively more by extra compute. A
 * `ratio <= R` gate would have waved that regression through. A reference that
 * shares the code under test cancels the very thing being watched.
 *
 * The reference below is therefore frozen IN THIS FILE: it touches memory and
 * does float arithmetic and min/max, like the detector, but it calls none of the
 * detector's code and must never be edited to track it. Machine slower -> both
 * grow, ratio steady. Code slower -> only the detector grows, ratio rises, gate
 * fires. That is the property the gate needs and the other reference lacked.
 *
 * THE CONTEXT IS STILL PART OF THE NUMBER (QA-A-55/A-56/A-57): one warm-up run
 * discarded, minimum of three, single thread. Timing noise is one-sided -- the
 * scheduler and other processes can only make a run slower -- so the minimum is
 * the least contaminated estimate. The reference is measured the same way, in
 * the same process, so both sides see the same machine state.
 *
 * READING A FAILURE. The message prints absolute milliseconds, the reference,
 * and the ratio, for exactly one reason: so the next person can tell a slow
 * machine from slower code without re-running anything. A slow machine raises
 * the absolutes and leaves the ratio alone; a regression raises the ratio.
 *
 * COVERAGE. The names match XPE_COVERAGE_EXCLUDE_TESTS in cmake/XpeCoverage.cmake
 * ("Performance|Within[0-9]+ms|..."), so the coverage build skips them -- a
 * timing assertion under instrumentation measures the instrumentation.
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013 (budget re-defined 92f8f1e, CI
 * measurement recorded 8a611a0). Refs #144 #143
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

/* ------------------------------------------------------- reference work */

/**
 * FROZEN. Do not edit this to match the detector, ever -- the moment it tracks
 * the code under test, the gate stops seeing regressions (see the file header).
 * It exists only to answer "how fast is this machine, right now, in this
 * process", and its character is chosen to resemble the detector's: streaming
 * reads over a buffer far larger than L2, a small neighbourhood per element,
 * float subtract/abs, and min/max.
 */
constexpr size_t kReferenceElems = 4u * 1024u * 1024u;   // 16 MB of float
// QA-A-60: sized so one measurement lasts long enough that start-up transients
// and scheduler slices amortise. At 3 sweeps the reference measured 24.8-46.6 ms
// across runs -- a near 2x jitter that made the RATIO noisier than the absolute
// it was meant to stabilise, which would have been a worse gate, not a better one.
constexpr int kReferenceSweeps = 12;
constexpr int kReferenceReps = 5;

double MeasureReferenceMs() {
    static const std::vector<float> buffer = [] {
        std::vector<float> b(kReferenceElems);
        std::mt19937 rng(20260912u);
        std::normal_distribution<float> noise(1000.0f, 25.0f);
        for (size_t i = 0; i < b.size(); ++i) b[i] = noise(rng);
        return b;
    }();

    volatile float sink = 0.0f;
    auto once = [&]() {
        float acc = 0.0f;
        for (int sweep = 0; sweep < kReferenceSweeps; ++sweep) {
            for (size_t i = 4; i + 4 < buffer.size(); ++i) {
                const float c = buffer[i];
                float lo = c, hi = c;
                for (int d = 1; d <= 4; ++d) {
                    const float a = buffer[i - static_cast<size_t>(d)];
                    const float b = buffer[i + static_cast<size_t>(d)];
                    lo = (a < lo) ? a : lo;
                    lo = (b < lo) ? b : lo;
                    hi = (a > hi) ? a : hi;
                    hi = (b > hi) ? b : hi;
                }
                acc += std::fabs(c - (lo + hi) * 0.5f);
            }
        }
        sink = sink + acc;
    };

    once();   // warm-up, discarded -- same treatment as the measured work
    double best = 1e30;
    for (int rep = 0; rep < kReferenceReps; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        once();
        const auto t1 = std::chrono::steady_clock::now();
        const double v = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (v < best) best = v;
    }
    return best;
}

/* ------------------------------------------------------------- the gates */
//
// Every run prints a grep-able line:
//
//     [perf-gate-ratio] <label> ratio=<x> limit=<y>
//
// QA-A-66 RE-DERIVED THE LIMIT. QA-A-65 made the detector 4.2x faster, which
// moved the clean ratio from ~7.2 to ~1.6 and left the old limit of 10.00
// passing a six-fold regression. A limit that cannot fail is a gate that is off.
//
// IT ALSO WIDENED THE CLEAN BAND, and that had to be explained before the limit
// could be set. The two machines' ratios, which had agreed to 5.4% before A-65,
// then differed by 24% AND swapped order (dev 1.644, CI 1.323). QA-A-66 found
// the reason by pinning the same work to the two core types of this machine --
// an i7-12700 has P (Golden Cove) and E (Gracemont) cores, which is a second
// microarchitecture without a second computer:
//
//     P-core   detect 166.7   reference  96.9   ratio 1.720
//     E-core   detect 237.2   reference 181.4   ratio 1.308
//     CI                                        ratio 1.323
//
// The E-core reproduces the CI ratio to within 1.2%. So the CI value is not an
// anomaly to be explained away -- the clean band is genuinely this wide now, and
// the reason is a change in what the ratio measures. Going from P to E the
// reference slows by 1.87x while the detector slows by only 1.42x, because the
// detector's time is now dominated by the global sigma stage (1.40x) rather than
// by the per-pixel loop (2.40x, the AVX2 part, which E-cores punish hardest).
//
// FIVE CANDIDATE REFERENCES WERE MEASURED against that spread, and none tracked
// the detector better than the one already here (P->E factor, detector 1.42):
//
//     A  this kernel, streaming min/max + float arithmetic     1.87
//     B  histogram over 16 MB, integer scatter                 1.78
//     C  streaming difference + histogram over 36 MB           1.89
//     D  A and C together                                      1.86
//     E  four-accumulator streaming sum, bandwidth-bound       1.75
//
// Everything written in a test TU lands at 1.75-1.89 while the detector lands at
// 1.42, so a single reference will not close the gap -- the detector is compiled
// with /arch:AVX2 and a test TU is not, and its dominant stage is memory-shaped
// in a way none of these reproduce. The conclusion is therefore NOT "find a
// better kernel": it is that the clean band is ~1.30 to ~1.72 and the limit must
// be set above it rather than pretending the band is narrow. The kernel below is
// unchanged, and stays frozen.
//
// MEASURED BANDS (QA-A-66, fresh build verified for each):
//
//     clean        P-core        1.636 .. 1.720   (4 runs)
//                  E-core        1.298 .. 1.308   (4 runs)
//                  CI runner     1.323            (post-A-65 run)
//     regression   P-core        7.060            (AVX2 path compiled out)
//                  E-core        7.154            (same)
//
// The regression band is the natural one for this code: XPE_DETECT_HAS_AVX2
// forced to 0, which is what any change that silently loses the vector path
// would produce. It is 4.1x above the worst clean run, and -- worth noting --
// the two core types agree to 1.3% there, because with the AVX2 path gone the
// detector is scalar again and tracks the scalar reference again.
//
// 2.20 sits 27.9% above the worst clean run across both microarchitectures (the
// same margin QA-A-60 used) and 69% below the best regression run. It catches a
// 1.28x regression on a P-core and a 1.69x one on an E-core or CI.
//
// WHY NOT TIGHTER: 1.72 is a measured clean run, not a bound, and the CI runner
// hardware has already changed once (its reference moved 172.3 -> 158.1 ms
// between runs). A margin narrower than the observed machine-to-machine spread
// buys sensitivity with false failures.
//
// Reading the value from CI: ctest prints test output only on failure, so a
// passing run has no `perf-gate-ratio` line in the job log. It is in the
// xpe-preprocess-test-results artifact, under Temporary/LastTest.log. That is
// ctest behaving normally -- do not "fix" it, or every passing run grows a log.
constexpr double kRatio3072Limit = 2.20;

/** Reported, never asserted: the SPEC improvement target we are not near yet. */
constexpr double kImprovementTargetMs = 60.0;

// QA-A-60: five repeats, not three. The small-frame ratio moved 0.739..1.123
// across runs at three (52% spread), which is wider than the gap between a clean
// run and a real regression -- a gate cannot live inside its own noise. More
// repeats cost under a second and narrow the minimum's distribution.
constexpr int kDetectionReps = 5;

struct Timing {
    double best = 0.0;
    double samples[kDetectionReps] = {0.0, 0.0, 0.0, 0.0, 0.0};
};

Timing TimeDetection(uint32_t w, uint32_t h) {
    const size_t n = static_cast<size_t>(w) * h;

    std::mt19937 rng(20260912u);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    std::vector<float> frame(n);
    for (size_t i = 0; i < n; ++i) frame[i] = noise(rng);
    std::vector<uint8_t> map(n, 0u);

    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = w;
    img.height = h;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(n * sizeof(float));

    XpeImageBuffer out{};
    out.data = map.data();
    out.width = w;
    out.height = h;
    out.bitsAllocated = 8;
    out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(n);

    XpeImageMetadata meta{};
    Timing t;

    // One warm-up, discarded: page faults on the fresh buffers and cold caches
    // are real costs but are not what the gate is about.
    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));

    t.best = 1e30;
    for (int rep = 0; rep < kDetectionReps; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = xpe_defect_detect_runtime(&img, &meta, &out);
        const auto t1 = std::chrono::steady_clock::now();
        EXPECT_EQ(XPE_OK, rc);
        t.samples[rep] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (t.samples[rep] < t.best) t.best = t.samples[rep];
    }
    return t;
}

/** One place that builds the failure text, so both gates explain themselves alike. */
std::string Explain(const char* label, const Timing& t, double reference,
                    double ratio, double limit) {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "%s: ratio %.3f exceeds limit %.3f.\n"
        "  detection  %.1f ms (samples %.1f / %.1f / %.1f / %.1f / %.1f)\n"
        "  reference  %.1f ms  (frozen kernel, same process, same warm-up)\n"
        "  HOW TO READ THIS: the ratio is detection / reference, so a slow\n"
        "  machine raises BOTH and leaves the ratio alone. A raised ratio means\n"
        "  the DETECTOR got slower relative to this machine -- that is a code\n"
        "  regression, and re-running will not clear it.\n"
        "  If instead the absolutes look high but the ratio is near its usual\n"
        "  value, the machine is slow and the code is fine.\n"
        "  (Do NOT read three high samples as proof of a regression: on a slow\n"
        "  machine all three are high and consistent -- QA-A-59's CI run showed\n"
        "  exactly that, 1360.9 / 1363.3 / 1340.9 with 1.7%% spread.)",
        label, ratio, limit, t.best, t.samples[0], t.samples[1], t.samples[2],
        t.samples[3], t.samples[4], reference);
    return std::string(buf);
}

}  // namespace

TEST(RuntimeDetectionPerformanceGateTest, Frame3072SquaredWithinMachineRatio) {
    const double reference = MeasureReferenceMs();
    const Timing t = TimeDetection(3072u, 3072u);
    const double ratio = t.best / reference;

    std::printf("[perf-gate] 3072x3072 single thread, warm, min of %d: %.1f ms"
                "  (samples %.1f / %.1f / %.1f / %.1f / %.1f)\n",
                kDetectionReps, t.best, t.samples[0], t.samples[1],
                t.samples[2], t.samples[3], t.samples[4]);
    std::printf("[perf-gate] reference kernel: %.1f ms\n", reference);
    std::printf("[perf-gate-ratio] 3072 ratio=%.3f limit=%.3f\n", ratio, kRatio3072Limit);
    std::printf("[perf-gate] SPEC improvement target %.0f ms (AVX2, single thread)"
                " -- currently %.1fx the target on this machine\n",
                kImprovementTargetMs, t.best / kImprovementTargetMs);

    EXPECT_LE(ratio, kRatio3072Limit)
        << Explain("3072x3072 runtime detection", t, reference, ratio, kRatio3072Limit);
}

/**
 * DIAGNOSTIC, NOT A GATE -- and that is a measured decision, not an omission.
 *
 * The small frame was given exactly the same treatment as the large one, and the
 * treatment did not take:
 *
 *     1024 ratio   clean       0.629 .. 0.969   (5 runs, spread 54%)
 *                  regression  1.140 .. 1.359   (3 runs, same injected regression)
 *
 * The worst clean run and the best regression run are only 1.18x apart while the
 * clean runs alone span 54%. Every candidate limit therefore sits inside the
 * clean band: it would either fire on healthy code or never fire at all. The
 * frame is small enough to live in L3, so its cost is dominated by scheduling
 * and turbo transients rather than by the work -- more repeats narrowed it (three
 * repeats spanned 0.739..1.123) but not far enough.
 *
 * A gate that lives inside its own noise is worse than no gate: it trains people
 * to re-run until it passes, which is exactly how a real regression gets waved
 * through. So the number is measured and PRINTED -- the CI log will carry it, and
 * a trend is still readable -- but nothing is asserted on it. The 3072 gate is
 * the one that separates, and it is the one that decides.
 */
TEST(RuntimeDetectionPerformanceGateTest, Frame1024SquaredMachineRatioDiagnostic) {
    const double reference = MeasureReferenceMs();
    const Timing t = TimeDetection(1024u, 1024u);
    const double ratio = t.best / reference;

    std::printf("[perf-gate] 1024x1024 single thread, warm, min of %d: %.1f ms"
                "  (samples %.1f / %.1f / %.1f / %.1f / %.1f)\n",
                kDetectionReps, t.best, t.samples[0], t.samples[1],
                t.samples[2], t.samples[3], t.samples[4]);
    std::printf("[perf-gate] reference kernel: %.1f ms\n", reference);
    std::printf("[perf-gate-ratio] 1024 ratio=%.3f limit=none (diagnostic only,"
                " see the comment above this test)\n", ratio);

    // The only assertion here is that the work ran at all. See the comment above.
    EXPECT_GT(ratio, 0.0);
}
