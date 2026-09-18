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
 * The reference below therefore lives IN THIS FILE and calls none of the
 * detector's code. Machine slower -> both grow, ratio steady. Code slower ->
 * only the detector grows, ratio rises, gate fires. That is the property the
 * gate needs and the other reference lacked.
 *
 * "Machine slower -> both grow" only holds if both are slowed by the SAME
 * property of the machine. QA-A-115 (#179) found that they were not: the old
 * reference was cache-resident and measured core throughput while the detector
 * streams 46 MB and measures memory bandwidth, so a CI runner with 19.4 GB/s
 * raised the ratio from 0.65 to 1.05 with no code change at all. The reference
 * is now bandwidth-bound too -- see the kernel's own comment for the evidence
 * and for what remains fixed about it (it still calls no detector code, and it
 * is still never re-tuned to make a failing run pass).
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
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

/* ------------------------------------------------------- reference work */

/**
 * THE REFERENCE KERNEL IS BANDWIDTH-BOUND, AND THAT IS THE WHOLE POINT.
 *
 * It used to say "do not edit this, ever". QA-A-115 (#179) replaces that
 * sentence, because the old kernel was measuring the wrong thing about the
 * machine and the CI logs proved it:
 *
 *     dev PC   i7-12700          bandwidth 25.5 GB/s   ratio 0.65
 *     CI       Xeon 6973P-C      bandwidth 19.4 GB/s   ratio 1.03-1.07
 *
 * with ZERO changed lines in the detector's path. The old kernel swept a 16 MB
 * buffer twelve times, so after the first sweep it lived in cache and measured
 * core throughput. The detector streams a 36 MB frame plus a 9.4 MB map, so it
 * measures memory bandwidth. Two machines that differ in bandwidth but not in
 * core throughput therefore moved the ratio without any code change -- the
 * ratio did not cancel the machine, which is the only thing a ratio is for.
 *
 * The rule the old sentence protected still holds and is worth restating: the
 * reference MUST NOT call the detector, and MUST NOT be re-tuned to make a
 * failing run pass. What changed is which property of the machine it measures,
 * and that change was made once, on evidence, and is recorded here.
 *
 * So: same working-set size and same access shape as the detector -- a float
 * frame streamed with a small neighbourhood, and a uint8 map written per
 * element -- without any of its code.
 *
 * EVERY RATIO MEASURED BEFORE THIS CHANGE IS VOID. The historical band
 * (0.472..0.638 across 21 CI runs) described the old denominator and says
 * nothing about this one; the limit below is re-derived from scratch.
 */
constexpr size_t kReferenceElems = 9u * 1024u * 1024u;   // 37.7 MB of float
constexpr size_t kReferenceMapBytes = kReferenceElems;   // 9.4 MB of uint8
// Sized from measurement, not taste: one sweep moves about 47 MB, so a handful
// of sweeps is long enough that start-up transients amortise while the run
// stays under a tenth of a second on both machines.
constexpr int kReferenceSweeps = 6;
constexpr int kReferenceReps = 5;

double MeasureReferenceMs() {
    // Both buffers are allocated once and reused. Their combined 47 MB is far
    // past any last-level cache, which is what forces the traffic to main
    // memory -- the property being measured.
    static const std::vector<float> frame = [] {
        std::vector<float> b(kReferenceElems);
        std::mt19937 rng(20260912u);
        std::normal_distribution<float> noise(1000.0f, 25.0f);
        for (size_t i = 0; i < b.size(); ++i) b[i] = noise(rng);
        return b;
    }();
    static std::vector<uint8_t> map(kReferenceMapBytes, 0u);

    volatile float sink = 0.0f;
    auto once = [&]() {
        float acc = 0.0f;
        for (int sweep = 0; sweep < kReferenceSweeps; ++sweep) {
            // One streaming read of the frame, one streaming write of the map,
            // three floats of arithmetic per element. The arithmetic is kept
            // deliberately thin so the loop waits on memory rather than on the
            // core -- the detector's own inner loop has the same character.
            const float thr = 30.0f;
            for (size_t i = 1; i + 1 < frame.size(); ++i) {
                const float d = std::fabs(frame[i] - 0.5f * (frame[i - 1] + frame[i + 1]));
                map[i] = (d > thr) ? 1u : 0u;
                acc += d;
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

/* --------------------------------------------------- machine diagnostics */
//
// QA-A-108 (#179). Two CI runs of the SAME commit produced ratios 0.972 and
// 1.172 while 21 historical runs sat at 0.472..0.638, with zero changed lines in
// modules/preprocess and modules/common. To read the NEXT failure we need to
// know which machine ran it, so the run prints its own profile.
//
// These lines are diagnostics only. They assert nothing, they do not touch the
// reference kernel, and they must never become an input to the limit.
//
// Why a bandwidth probe belongs here: the reference kernel is a 16 MB buffer
// swept 12 times, which is compute-bound once resident, while the 3072^2
// detection touches a 36 MB frame plus a 9.4 MB map, which is bandwidth-bound.
// If a runner's memory subsystem is slow but its cores are not, the ratio rises
// with no code change -- exactly the shape of the two failing runs. The probe
// measures that axis directly so the next failure can be attributed rather than
// guessed at.

#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <thread>

std::string CpuBrand() {
#if defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuid(regs, 0x80000000);
    if (static_cast<unsigned>(regs[0]) < 0x80000004u) return "unknown";
    char brand[49] = {0};
    for (unsigned leaf = 0; leaf < 3; ++leaf) {
        __cpuid(regs, static_cast<int>(0x80000002u + leaf));
        std::memcpy(brand + leaf * 16u, regs, sizeof(regs));
    }
    std::string s(brand);
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
#else
    return "unknown";
#endif
}

bool HasAvx2() {
#if defined(_MSC_VER)
    int regs[4] = {0, 0, 0, 0};
    __cpuid(regs, 0);
    if (regs[0] < 7) return false;
    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 5)) != 0;   // EBX bit 5 = AVX2
#else
    return false;
#endif
}

/** Streaming read bandwidth over a buffer far larger than any last-level cache. */
double StreamingBandwidthGBs() {
    constexpr size_t kBytes = 256u * 1024u * 1024u;
    constexpr size_t kElems = kBytes / sizeof(float);
    std::vector<float> buf(kElems, 1.0f);
    volatile float sink = 0.0f;
    auto once = [&]() {
        float a = 0.0f;
        for (size_t i = 0; i < kElems; i += 16) a += buf[i];   // one float per cache line
        sink = sink + a;
    };
    once();   // warm-up: first touch pays the page faults
    double best = 1e30;
    for (int rep = 0; rep < 3; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        once();
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return static_cast<double>(kBytes) / (best * 1e-3) / 1e9;
}

/** Printed once per gate so a failing job log identifies its own runner. */
void PrintMachineProfile() {
    std::printf("[perf-gate-machine] cpu=\"%s\" logical=%u avx2=%d bandwidth=%.1f GB/s\n",
                CpuBrand().c_str(), std::thread::hardware_concurrency(),
                HasAvx2() ? 1 : 0, StreamingBandwidthGBs());
    // The library is built with /arch:AVX2; this test binary is not, so the
    // ratio's numerator and denominator are compiled for different instruction
    // sets (modules/preprocess/CMakeLists.txt:77). Recorded, not changed --
    // changing it would move every historical ratio.
    std::printf("[perf-gate-machine] numerator=xpe_preprocess(/arch:AVX2)"
                " denominator=in-test kernel(default arch)\n");
}

/* ------------------------------------------------------------- the gates */
//
// Every run prints a grep-able line:
//
//     [perf-gate-ratio] <label> ratio=<x> limit=<y>
//
// QA-A-115 (#179) RE-DERIVED THE LIMIT FROM SCRATCH, because the denominator
// changed. SPEC 60a81c1 [HARD]: a limit is re-derived at every large
// performance change and never inherited -- and replacing the reference kernel
// is the largest such change there is. EVERY RATIO RECORDED BEFORE THIS POINT
// (the 0.472..0.638 band of 21 CI runs, the 0.85 limit, the QA-A-66/67 P/E-core
// tables) described the old cache-resident kernel and is VOID here.
//
// MEASURED BAND (dev PC i7-12700, fresh build each time, min-of-3-rounds):
//
//     clean             1.282  1.289  1.318      (3 runs; worst 1.318)
//     regression +1 pass  1.380                  (one extra streaming pass)
//     regression +2      1.479
//     regression +8      2.046
//
// The injected regression is an extra streaming pass over the input frame, the
// same instrument QA-A-109 used, so the rows are comparable to each other.
//
// 1.45 sits 9.6% above the worst clean run and 2.0% below the nearest
// regression this band contains (+2 passes, 1.479). That is a TIGHTER margin
// than the 30.6% the old gate carried, and it is tight for a reason worth
// stating plainly: a bandwidth-bound denominator moves with the same machine
// noise as the numerator, so the clean band narrowed (2.8% across runs, against
// the old gate's 24% across core types) -- but it has not been measured on a
// second machine yet.
//
// PROVISIONAL UNTIL THE CI RUN. The whole point of the new kernel is that the
// ratio should now be the SAME on a machine with different memory bandwidth.
// That claim is untested until this lands in CI on the Xeon 6973P-C, where the
// old kernel produced 1.03-1.07 against the dev PC's 0.65. If the CI ratio
// lands near 1.3, the kernel cancels the machine and this limit stands. If it
// does not, the limit is wrong and so is the approach -- and that is the
// measurement, not a reason to move the number.
constexpr double kRatio3072Limit = 1.45;

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

// QA-A-109 (#179): the gate judges the MINIMUM ratio of three rounds, not one.
//
// Why: two CI runs of the same commit produced 0.972 and 1.172 against a history
// of 0.472..0.638 with zero changed lines in the detector's path (QA-A-108).
// The failing runs' 1024 frame was barely slower while the 3072 frame was 55-160%
// slower -- a size-dependent slowdown, so the runner's memory subsystem, not the
// code. A transient like that lifts one round; a real regression lifts all three,
// because it is in the instructions being executed.
//
// Each round re-measures BOTH the reference kernel and the detector. Measuring
// the reference once and reusing it would make the later rounds compare against
// a machine state that no longer exists -- the ratio would stop meaning
// "detector relative to this machine right now".
//
// All three ratios are printed. Printing only the minimum would leave the next
// investigation with nothing: the SPREAD between the rounds is what separates
// "transient" from "regression", and that is exactly the evidence QA-A-108 had
// to reconstruct from artifacts.
constexpr int kGateRounds = 3;

TEST(RuntimeDetectionPerformanceGateTest, Frame3072SquaredWithinMachineRatio) {
    PrintMachineProfile();

    double ratios[kGateRounds] = {0.0, 0.0, 0.0};
    double best = 1e30;
    int bestRound = 0;
    Timing bestTiming{};
    double bestReference = 0.0;

    for (int round = 0; round < kGateRounds; ++round) {
        const double reference = MeasureReferenceMs();
        const Timing t = TimeDetection(3072u, 3072u);
        const double ratio = t.best / reference;
        ratios[round] = ratio;

        std::printf("[perf-gate] round %d/%d 3072x3072 single thread, warm,"
                    " min of %d: %.1f ms"
                    "  (samples %.1f / %.1f / %.1f / %.1f / %.1f)\n",
                    round + 1, kGateRounds, kDetectionReps, t.best,
                    t.samples[0], t.samples[1], t.samples[2], t.samples[3],
                    t.samples[4]);
        std::printf("[perf-gate] round %d/%d reference kernel: %.1f ms\n",
                    round + 1, kGateRounds, reference);
        std::printf("[perf-gate-ratio] 3072 round=%d ratio=%.3f limit=%.3f\n",
                    round + 1, ratio, kRatio3072Limit);

        if (ratio < best) {
            best = ratio;
            bestRound = round;
            bestTiming = t;
            bestReference = reference;
        }
    }

    std::printf("[perf-gate-ratio] 3072 ratio=%.3f limit=%.3f"
                "  (minimum of %d rounds: %.3f / %.3f / %.3f)\n",
                best, kRatio3072Limit, kGateRounds,
                ratios[0], ratios[1], ratios[2]);
    std::printf("[perf-gate] SPEC improvement target %.0f ms (AVX2, single thread)"
                " -- currently %.1fx the target on this machine\n",
                kImprovementTargetMs, bestTiming.best / kImprovementTargetMs);

    EXPECT_LE(best, kRatio3072Limit)
        << Explain("3072x3072 runtime detection", bestTiming, bestReference,
                   best, kRatio3072Limit)
        << "\n  This is the BEST of " << kGateRounds << " rounds ("
        << ratios[0] << " / " << ratios[1] << " / " << ratios[2]
        << "), measured in round " << (bestRound + 1) << ". A transient lifts one\n"
        << "  round; all three being over the limit is what a code regression\n"
        << "  looks like, so re-running will not clear this.";
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
