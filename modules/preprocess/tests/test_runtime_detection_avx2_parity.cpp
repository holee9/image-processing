/**
 * @file test_runtime_detection_avx2_parity.cpp
 * @brief QA-A-65 (#144): the AVX2 per-pixel path is BITWISE identical to the scalar one.
 *
 * "Bitwise", not "within a tolerance". QA-A-61 recorded that this rule contains
 * no floating-point non-associativity, and QA-A-65 re-checked that claim for
 * SIMD rather than carrying it over -- horizontal sums, FMA contraction and
 * reciprocal approximation are exactly where such a claim usually stops being
 * true, and none of the three is used. So identity is the design, and a
 * tolerance would hide a defect instead of measuring one.
 *
 * EXPECT_FLOAT_EQ IS THE WRONG ASSERTION HERE -- it allows 4 ulp. Where a float
 * is compared below it is compared as raw bits.
 *
 * THE CASE THAT DECIDES THE OPERAND ORDER. The scalar compare-exchange is
 *
 *     lo = (b < a) ? b : a;   hi = (b < a) ? a : b
 *
 * and with a = +0.0, b = -0.0 the comparison is false, so hi = b = -0.0. The
 * vector form must therefore be _mm256_min_ps(b, a) and _mm256_max_ps(a, b);
 * _mm256_max_ps(b, a) would return +0.0 for the same input. Both bit patterns
 * mean zero and compare equal, so EVERY comparison-based assertion passes on the
 * wrong one -- only the bits separate them. SignedZeroOrderingMatches is the
 * test that does, and it is the reason this file exists rather than a frame-level
 * comparison alone. (#156 is the sibling lesson: a threshold that reads `> 0`
 * called a 1-ulp difference "reached".)
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013.  Refs #144 #143
 */

#include <gtest/gtest.h>

#include "runtime_detection.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess_api.h"

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {

using xpe::preprocess::internal::DetectDefectivePixel;
using xpe::preprocess::internal::DetectRowRange;
using xpe::preprocess::internal::MedianOfEight;

bool SameBits(float a, float b) {
    uint32_t ba = 0, bb = 0;
    std::memcpy(&ba, &a, sizeof(ba));
    std::memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}

uint32_t Bits(float f) {
    uint32_t b = 0;
    std::memcpy(&b, &f, sizeof(b));
    return b;
}

XpeImageBuffer Wrap(std::vector<float>& px, uint32_t w, uint32_t h) {
    XpeImageBuffer img{};
    img.data = px.data();
    img.width = w;
    img.height = h;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = px.size() * sizeof(float);
    return img;
}

/** kind: 0 noise, 1 many exact ties, 2 signed zeros, 3 flat, 4 integer grid. */
std::vector<float> MakeFrame(uint32_t w, uint32_t h, int kind, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(3000.0f, 10.0f);
    std::vector<float> f(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < f.size(); ++i) {
        switch (kind) {
            case 1: f[i] = static_cast<float>(i % 3u);          break;  // ties everywhere
            case 2: f[i] = ((i % 2u) == 0u) ? 0.0f : -0.0f;     break;  // +0 / -0 mixture
            case 3: f[i] = 3000.0f;                             break;
            case 4: f[i] = std::floor(noise(rng));              break;
            default: f[i] = noise(rng);                         break;
        }
    }
    if (kind == 0 || kind == 4) {
        for (size_t i = 97; i < f.size(); i += 523) f[i] += 120.0f;   // real outliers
    }
    return f;
}

/** The scalar rule over a whole frame, with no vector path anywhere. */
std::vector<uint8_t> ScalarMap(const XpeImageBuffer* img, const RuntimeDetectionConfig& cfg) {
    const uint32_t w = img->width, h = img->height;
    std::vector<uint8_t> map(static_cast<size_t>(w) * h, 0u);
    std::vector<float> a, b;
    a.reserve(64); b.reserve(64);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            if (DetectDefectivePixel(img, x, y, cfg, a, b))
                map[static_cast<size_t>(y) * w + x] = 1u;
    return map;
}

RuntimeDetectionConfig ResolvedConfig(const XpeImageBuffer* img) {
    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    const float sg = xpe::preprocess::internal::ComputeGlobalSigma(img);
    cfg.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sg;
    cfg.globalSigmaCap = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sg;
    return cfg;
}

}  // namespace

#if XPE_DETECT_HAS_AVX2

// ---------------------------------------------------------------------------
// QA-A-66: the four named selections, tested one level below the network.
//
// QA-A-65 left SignedZeroOrderingMatchesTheScalarNetwork as the only thing
// standing between a swapped operand order and a wrong build. The order now
// lives in four one-line wrappers (SelectCeLower / SelectCeUpper /
// SelectGreaterOf / SelectLesserOf) and nothing else in the header calls the
// intrinsics directly -- but a structure still needs a test that fails when the
// structure is edited wrongly, and this one names the exact function rather than
// asking the reader to infer it from a median.
// ---------------------------------------------------------------------------
TEST(Avx2ParityTest, NamedSelectionsMatchTheirScalarTernaries) {
    using xpe::preprocess::internal::SelectCeLower;
    using xpe::preprocess::internal::SelectCeUpper;
    using xpe::preprocess::internal::SelectGreaterOf;
    using xpe::preprocess::internal::SelectLesserOf;

    const float pool[6] = {0.0f, -0.0f, 1.0f, -1.0f, 3.5f, -2.25f};

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            const float a = pool[i], b = pool[j];

            // The scalar forms, copied from MedianSortCE and DetectDefectivePixel.
            const float ceLo = (b < a) ? b : a;
            const float ceHi = (b < a) ? a : b;
            const float greater = (a > b) ? a : b;
            const float lesser = (a < b) ? a : b;

            float got[4][8];
            _mm256_storeu_ps(got[0], SelectCeLower(_mm256_set1_ps(a), _mm256_set1_ps(b)));
            _mm256_storeu_ps(got[1], SelectCeUpper(_mm256_set1_ps(a), _mm256_set1_ps(b)));
            _mm256_storeu_ps(got[2], SelectGreaterOf(_mm256_set1_ps(a), _mm256_set1_ps(b)));
            _mm256_storeu_ps(got[3], SelectLesserOf(_mm256_set1_ps(a), _mm256_set1_ps(b)));

            EXPECT_TRUE(SameBits(ceLo, got[0][0]))
                << "SelectCeLower(" << a << ", " << b << "): scalar 0x" << std::hex
                << Bits(ceLo) << " vector 0x" << Bits(got[0][0]) << std::dec;
            EXPECT_TRUE(SameBits(ceHi, got[1][0]))
                << "SelectCeUpper(" << a << ", " << b << "): scalar 0x" << std::hex
                << Bits(ceHi) << " vector 0x" << Bits(got[1][0]) << std::dec
                << " -- this is the +0.0 / -0.0 case if both print as 0";
            EXPECT_TRUE(SameBits(greater, got[2][0]))
                << "SelectGreaterOf(" << a << ", " << b << ")";
            EXPECT_TRUE(SameBits(lesser, got[3][0]))
                << "SelectLesserOf(" << a << ", " << b << ")";
        }
    }
}

// ---------------------------------------------------------------------------
// The narrow case that decides the operand order. Everything else in this file
// would still pass if min/max were written the other way round.
// ---------------------------------------------------------------------------
TEST(Avx2ParityTest, SignedZeroOrderingMatchesTheScalarNetwork) {
    // Eight values built so the two middle order statistics are a +0.0 and a
    // -0.0: the median is then (+0.0 + -0.0) * 0.5 or (-0.0 + -0.0) * 0.5
    // depending on WHICH zero the network puts where, and those differ in bits.
    const float pos = 0.0f;
    const float neg = -0.0f;
    const float src[8] = {neg, pos, neg, pos, neg, pos, neg, pos};

    const float scalar = MedianOfEight(src);

    __m256 v[8];
    for (int k = 0; k < 8; ++k) v[k] = _mm256_set1_ps(src[k]);
    float lanes[8];
    _mm256_storeu_ps(lanes, xpe::preprocess::internal::MedianOfEight8(v));

    for (int lane = 0; lane < 8; ++lane) {
        EXPECT_TRUE(SameBits(scalar, lanes[lane]))
            << "lane " << lane << ": scalar bits 0x" << std::hex << Bits(scalar)
            << " vector bits 0x" << Bits(lanes[lane]) << std::dec
            << " -- both are zero and compare equal, so only the bits catch this."
               " Check the min/max operand order in MedianSortCE8.";
    }
}

// Same question, one input per lane rather than a broadcast, so a lane-crossing
// mistake cannot hide behind identical lanes.
TEST(Avx2ParityTest, PerLaneInputsAgreeBitForBit) {
    std::mt19937 rng(20260924u);
    std::uniform_int_distribution<int> pick(0, 3);
    const float pool[4] = {0.0f, -0.0f, 1.0f, -1.0f};

    for (int trial = 0; trial < 512; ++trial) {
        float src[8][8];        // [lane][neighbour]
        for (int lane = 0; lane < 8; ++lane)
            for (int k = 0; k < 8; ++k)
                src[lane][k] = pool[pick(rng)];

        __m256 v[8];
        for (int k = 0; k < 8; ++k) {
            float col[8];
            for (int lane = 0; lane < 8; ++lane) col[lane] = src[lane][k];
            v[k] = _mm256_loadu_ps(col);
        }
        float lanes[8];
        _mm256_storeu_ps(lanes, xpe::preprocess::internal::MedianOfEight8(v));

        for (int lane = 0; lane < 8; ++lane) {
            const float expected = MedianOfEight(src[lane]);
            ASSERT_TRUE(SameBits(expected, lanes[lane]))
                << "trial " << trial << " lane " << lane
                << ": scalar 0x" << std::hex << Bits(expected)
                << " vector 0x" << Bits(lanes[lane]) << std::dec;
        }
    }
}

// ---------------------------------------------------------------------------
// Frame level: the map the shared row loop produces must equal the map the
// scalar rule produces, pixel for pixel, on every frame shape.
// ---------------------------------------------------------------------------
TEST(Avx2ParityTest, FrameMapsAreIdenticalPixelForPixel) {
    struct Case { uint32_t w, h; int kind; const char* name; };
    const Case cases[] = {
        {  64u,  48u, 0, "noise" },
        {  64u,  48u, 1, "ties" },
        {  64u,  48u, 2, "signed zeros" },
        {  64u,  48u, 3, "flat" },
        {  64u,  48u, 4, "integer grid" },
        {  10u,  10u, 0, "narrowest frame with one vector run" },
        {   9u,  10u, 0, "one column too narrow -- all scalar" },
        {  17u,  17u, 0, "odd width: vector run plus scalar tail" },
        { 640u, 480u, 0, "realistic" },
    };

    for (const Case& c : cases) {
        std::vector<float> frame = MakeFrame(c.w, c.h, c.kind, 20260924u);
        XpeImageBuffer img = Wrap(frame, c.w, c.h);
        const RuntimeDetectionConfig cfg = ResolvedConfig(&img);

        const std::vector<uint8_t> expected = ScalarMap(&img, cfg);

        std::vector<uint8_t> actual(frame.size(), 0u);
        std::vector<float> a, b;
        a.reserve(64); b.reserve(64);
        DetectRowRange(&img, cfg, actual.data(), 0u, c.h, a, b);

        size_t mismatches = 0, firstBad = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
            if (expected[i] != actual[i]) {
                if (mismatches == 0) firstBad = i;
                ++mismatches;
            }
        }
        EXPECT_EQ(0u, mismatches)
            << c.name << " (" << c.w << "x" << c.h << "): " << mismatches
            << " pixels differ, first at row " << (firstBad / c.w)
            << " col " << (firstBad % c.w);
    }
}

// ---------------------------------------------------------------------------
// QA-A-69: the overlapping tail run.
//
// The forward walk starts runs at 1, 9, 17, ... while the run fits inside the
// interior columns, which leaves seven columns per row to the scalar path at
// EVERY width -- the remainder is a property of the step, not of the frame. One
// more run placed at w-9 ends exactly on the last interior column and overlaps
// the previous run by up to seven columns.
//
// WHAT THE PARITY TESTS ABOVE CAN AND CANNOT SEE. They compare the map against
// the scalar rule, so they catch an overlap that lands on the wrong columns or
// reaches into the border. They CANNOT catch the run being removed again: the
// scalar path computes the same values, so deleting the optimisation changes no
// map. That is the QA-A-67 lesson applied here -- check whether bit parity is
// enough before relying on it -- and the honest answer is that it is not, for
// this particular change. What is pinned below instead is the ARITHMETIC that
// makes the placement correct, which is the part that can be wrong while still
// being present.
// ---------------------------------------------------------------------------
TEST(Avx2ParityTest, LastRunStartLandsExactlyOnTheFinalInteriorColumn) {
    using xpe::preprocess::internal::DetectRowLastRunStart;

    for (uint32_t w = 10u; w <= 4096u; ++w) {
        const uint32_t start = DetectRowLastRunStart(w);
        EXPECT_GE(start, 1u) << "w=" << w << ": the run would read column " << (start - 1)
                             << ", which is outside the frame";
        EXPECT_EQ(w - 2u, start + 7u)
            << "w=" << w << ": the run ends on column " << (start + 7)
            << " but the last interior column is " << (w - 2);
    }
}

// The verdict is deterministic and the write is a whole byte, so judging a pixel
// twice must store what it stored the first time. QA-A-61 measured that on the
// ROW axis (duplicated boundary rows changed nothing). This is the same question
// on the COLUMN axis, which is the axis the overlap actually uses -- asked rather
// than inherited, because "it held there" is not "it holds here".
TEST(Avx2ParityTest, JudgingAColumnTwiceStoresTheSameByte) {
    const uint32_t w = 64u, h = 48u;
    std::vector<float> frame = MakeFrame(w, h, 0, 20260928u);
    XpeImageBuffer img = Wrap(frame, w, h);
    const RuntimeDetectionConfig cfg = ResolvedConfig(&img);

    std::vector<uint8_t> once(frame.size(), 0u);
    std::vector<float> a, b;
    a.reserve(64); b.reserve(64);
    DetectRowRange(&img, cfg, once.data(), 0u, h, a, b);

    // Re-judge a band of interior columns directly, exactly as an overlapping
    // run does, and require every byte to be unchanged.
    std::vector<uint8_t> twice = once;
    for (uint32_t y = 1u; y + 1u < h; ++y) {
        for (uint32_t x = 1u; x + 8u <= w - 1u; x += 3u) {   // step 3: deliberate overlap
            xpe::preprocess::internal::DetectEightPixelsAvx2(frame.data(), w, x, y, cfg,
                                                             twice.data());
        }
    }

    size_t changed = 0;
    for (size_t i = 0; i < once.size(); ++i) if (once[i] != twice[i]) ++changed;
    EXPECT_EQ(0u, changed)
        << changed << " bytes changed when interior columns were judged again; the"
           " overlapping tail run relies on that not happening";
}

// Widths chosen around the overlap boundary rather than at round numbers: w=10
// makes the tail run start at 1, i.e. exactly on top of the first run (full
// overlap); w=17 makes it start one column before the second run; w=18 makes the
// walk end flush so the tail run is a complete duplicate of the previous one.
TEST(Avx2ParityTest, OverlapBoundaryWidthsStillMatchTheScalarRule) {
    for (uint32_t w : {10u, 11u, 16u, 17u, 18u, 19u, 25u, 26u, 33u}) {
        const uint32_t h = 12u;
        std::vector<float> frame = MakeFrame(w, h, 0, 20260928u);
        XpeImageBuffer img = Wrap(frame, w, h);
        const RuntimeDetectionConfig cfg = ResolvedConfig(&img);

        const std::vector<uint8_t> expected = ScalarMap(&img, cfg);

        std::vector<uint8_t> actual(frame.size(), 0u);
        std::vector<float> a, b;
        a.reserve(64); b.reserve(64);
        DetectRowRange(&img, cfg, actual.data(), 0u, h, a, b);

        size_t mismatches = 0, firstBad = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
            if (expected[i] != actual[i]) {
                if (mismatches == 0) firstBad = i;
                ++mismatches;
            }
        }
        EXPECT_EQ(0u, mismatches)
            << "w=" << w << ": " << mismatches << " pixels differ, first at row "
            << (firstBad / w) << " col " << (firstBad % w)
            << " (tail run starts at " << (w - 9u) << ")";
    }
}

// ---------------------------------------------------------------------------
// QA-A-69: the vector runs never read outside the frame.
//
// WHY A MAP COMPARISON IS NOT ENOUGH HERE, measured rather than argued. Moving
// the tail run one column right (w-8 instead of w-9) makes it judge column w-1,
// whose right neighbour is off the end of the row. Every frame-level parity test
// above STILL PASSES: the run writes a wrong byte at w-1, and the scalar border
// pass that follows overwrites it with the right one. The map is correct and the
// read was not -- on the last interior row the load reaches one float past the
// end of the buffer.
//
// So the map cannot see it, and this test does not look at the map. The frame is
// placed so its last float ends exactly against a PAGE_NOACCESS page: a read one
// element past the end raises an access violation instead of returning a
// plausible number. This is the QA-A-63 fixture pointed at reads rather than
// writes, and it is the assertion the QA-A-67 question asks for -- bit parity is
// not enough, so something else has to watch.
// ---------------------------------------------------------------------------
#if defined(_WIN32)

namespace {

class GuardedFrame {
public:
    explicit GuardedFrame(size_t bytes) {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        const size_t page = si.dwPageSize;
        const size_t usable = ((bytes + page - 1) / page) * page;
        m_base = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, usable + page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (m_base == nullptr) return;
        DWORD old = 0;
        VirtualProtect(m_base + usable, page, PAGE_NOACCESS, &old);
        m_data = m_base + (usable - bytes);
    }
    ~GuardedFrame() { if (m_base) VirtualFree(m_base, 0, MEM_RELEASE); }
    GuardedFrame(const GuardedFrame&) = delete;
    GuardedFrame& operator=(const GuardedFrame&) = delete;

    float* data() const { return reinterpret_cast<float*>(m_data); }
    bool   valid() const { return m_base != nullptr; }

private:
    uint8_t* m_base = nullptr;
    uint8_t* m_data = nullptr;
};

bool RowRangeFaulted(const XpeImageBuffer* img,
                     RuntimeDetectionConfig cfg,
                     uint8_t* map,
                     uint32_t h,
                     std::vector<float>* a,
                     std::vector<float>* b) {
    __try {
        DetectRowRange(img, cfg, map, 0u, h, *a, *b);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return true;
    }
    return false;
}

}  // namespace

TEST(Avx2ParityTest, VectorRunsNeverReadPastTheFrame) {
    // Several widths: the tail run's placement is width-dependent, and w=10 is
    // the narrowest frame that vectorises at all.
    for (uint32_t w : {10u, 17u, 24u, 64u, 129u}) {
        const uint32_t h = 9u;
        const size_t n = static_cast<size_t>(w) * h;

        GuardedFrame frame(n * sizeof(float));
        ASSERT_TRUE(frame.valid());
        std::mt19937 rng(20260928u);
        std::normal_distribution<float> noise(3000.0f, 10.0f);
        for (size_t i = 0; i < n; ++i) frame.data()[i] = noise(rng);

        XpeImageBuffer img{};
        img.data = frame.data();
        img.width = w;
        img.height = h;
        img.bitsAllocated = 32;
        img.bitsStored = 32;
        img.format = XPE_PIXEL_FLOAT32;
        img.dataSize = n * sizeof(float);

        RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
        cfg.globalSigmaFloor = 0.0f;
        cfg.globalSigmaCap = 0.0f;

        std::vector<uint8_t> map(n, 0u);
        std::vector<float> a, b;
        a.reserve(64); b.reserve(64);

        EXPECT_FALSE(RowRangeFaulted(&img, cfg, map.data(), h, &a, &b))
            << "w=" << w << ": a load reached past the end of the frame (tail run"
               " starts at " << (w - 9u) << ")";
    }
}

#endif  // _WIN32

// A map that is entirely zero would make the test above pass without exercising
// anything. This is the control the other test needs.
TEST(Avx2ParityTest, TheComparedMapsAreNotTriviallyEmpty) {
    std::vector<float> frame = MakeFrame(640u, 480u, 0, 20260924u);
    XpeImageBuffer img = Wrap(frame, 640u, 480u);
    const RuntimeDetectionConfig cfg = ResolvedConfig(&img);

    const std::vector<uint8_t> map = ScalarMap(&img, cfg);
    size_t flagged = 0;
    for (uint8_t v : map) if (v) ++flagged;

    EXPECT_GT(flagged, 0u) << "no pixel flagged: the parity comparison would be vacuous";
    EXPECT_LT(flagged, map.size()) << "every pixel flagged: equally vacuous";
}

// The shipped entry point goes through the same row loop, so its map must match
// too. Without this, the vector path could be correct in the header and unreached
// in the product -- the shape QA-A-62 met when a tool measured a path the change
// did not touch.
TEST(Avx2ParityTest, ShippedEntryPointAgreesWithTheScalarRule) {
    const uint32_t w = 256u, h = 192u;
    std::vector<float> frame = MakeFrame(w, h, 0, 20260925u);
    XpeImageBuffer img = Wrap(frame, w, h);

    std::vector<uint8_t> mapOut(frame.size(), 0xCDu);
    XpeImageBuffer out{};
    out.data = mapOut.data();
    out.width = w; out.height = h;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = mapOut.size();

    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&img, &meta, &out));

    const std::vector<uint8_t> expected = ScalarMap(&img, ResolvedConfig(&img));

    size_t mismatches = 0;
    for (size_t i = 0; i < expected.size(); ++i) if (expected[i] != mapOut[i]) ++mismatches;
    EXPECT_EQ(0u, mismatches) << mismatches << " of " << expected.size()
                              << " pixels differ from the scalar rule";
}

#else

TEST(Avx2ParityTest, NoAvx2PathCompiledIn) {
    GTEST_SKIP() << "XPE_DETECT_HAS_AVX2 is 0; the scalar path is the whole implementation";
}

#endif  // XPE_DETECT_HAS_AVX2

// ===========================================================================
// QA-A-71 (#160): compiled in BOTH builds, on purpose.
//
// Everything above this line lives inside #if XPE_DETECT_HAS_AVX2, so a scalar
// build skips all of it -- which means the scalar build had no parity check of
// its own. QA-A-66 established that the scalar build COMPILES and QA-A-67
// re-checked that; neither established that it COMPUTES THE RIGHT ANSWER, and
// those are different claims.
//
// They are different for a concrete reason. QA-A-65 asserted bitwise parity
// BETWEEN TWO PATHS INSIDE ONE BUILD. A scalar build is a different compilation
// of the same source: different macro state, and -- where a project chooses to
// separate them -- potentially different flags, so the compiler may emit
// different code for the very functions the vector path was compared against.
// A claim about one build is not a claim about the other.
//
// This test therefore does two things, and the SECOND ONE ONLY WORKS ACROSS
// RUNS:
//
//   1. In whichever build it is compiled, it requires DetectRowRange to agree
//      with a direct scalar loop over DetectDefectivePixel, pixel for pixel, at
//      3072x3072 -- the size QA-A-62 opened as the one that matters. In the AVX2
//      build that compares the vector path against the rule; in the scalar build
//      it compares the row-range plumbing (border spans, row splitting) against
//      the rule, which is the part that can still be wrong there.
//
//   2. It PRINTS a digest of the map and the flagged count. Those two numbers
//      are what carries the cross-build claim: run the suite in both builds and
//      compare the printed digests. A test cannot do that by itself -- the two
//      builds are two binaries -- so the comparison is performed by whoever runs
//      them, and the QA-A-71 report records the pair that was observed.
//
// A hardcoded expected digest was deliberately NOT used. It would turn this into
// the constant comparison this repository has been bitten by (#140, QA-A-34):
// the number would have to come from a run, and then it asserts that the code
// still does what it did rather than that it does the right thing.
// ===========================================================================

namespace {

/** FNV-1a over the map bytes. Not cryptographic -- a compact run identity. */
uint64_t MapDigest(const std::vector<uint8_t>& map) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t v : map) {
        h ^= static_cast<uint64_t>(v);
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace

TEST(DetectBuildParityTest, RowRangeMatchesTheScalarRuleAtRealFrameSize) {
    constexpr uint32_t kW = 3072u;
    constexpr uint32_t kH = 3072u;
    const size_t n = static_cast<size_t>(kW) * kH;

    // Fixed seed: the digests printed below are only comparable across builds if
    // both builds see the same frame.
    std::vector<float> frame = MakeFrame(kW, kH, 0, 20260929u);
    ASSERT_EQ(n, frame.size());
    XpeImageBuffer img = Wrap(frame, kW, kH);
    const RuntimeDetectionConfig cfg = ResolvedConfig(&img);

    std::vector<uint8_t> viaRowRange(n, 0u);
    std::vector<float> a, b;
    a.reserve(64); b.reserve(64);
    DetectRowRange(&img, cfg, viaRowRange.data(), 0u, kH, a, b);

    const std::vector<uint8_t> viaScalarRule = ScalarMap(&img, cfg);

    size_t flagged = 0;
    for (uint8_t v : viaRowRange) if (v) ++flagged;

    size_t mismatches = 0, firstBad = 0;
    for (size_t i = 0; i < n; ++i) {
        if (viaRowRange[i] != viaScalarRule[i]) {
            if (mismatches == 0) firstBad = i;
            ++mismatches;
        }
    }

    // Printed unconditionally: this line IS the cross-build evidence.
    std::printf("[build-parity] XPE_DETECT_HAS_AVX2=%d  %ux%u  flagged=%zu  digest=%016llx\n",
                XPE_DETECT_HAS_AVX2, kW, kH, flagged,
                static_cast<unsigned long long>(MapDigest(viaRowRange)));
    std::fflush(stdout);

    EXPECT_GT(flagged, 0u) << "an all-zero map would agree trivially";
    EXPECT_LT(flagged, n) << "an all-one map would too";
    EXPECT_EQ(0u, mismatches)
        << mismatches << " of " << n << " pixels differ, first at row "
        << (firstBad / kW) << " col " << (firstBad % kW);
}
