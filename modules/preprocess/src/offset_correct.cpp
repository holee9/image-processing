/**
 * @file offset_correct.cpp
 * @brief SWU-1.1: Per-pixel dark offset subtraction (PRE-02)
 *        REQ-P1A-009 to REQ-P1A-011
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>

// @MX:NOTE: [AUTO] AVX2 intrinsics header — conditional include based on _MSC_VER
#if defined(_MSC_VER)
#include <intrin.h>
#include <immintrin.h>
#else
#include <immintrin.h>
#endif

/* =========================================================================
 * QA-A-74 (#160): four kernels were removed from this file --
 *   offset_correct_scalar, offset_correct_avx2, offset_correct_avx512,
 *   offset_correct_neon
 * -- and none of them was a portability asset.
 *
 * THEY WERE A DIFFERENT OPERATION, not older versions of the live one. Their
 * shared signature was (uint16_t* dst, const uint16_t* off, size_t n): an
 * IN-PLACE integer saturating subtract over a uint16 offset map. The path this
 * module actually takes is (const uint16_t* src, const float* off, uint16_t*
 * dst, size_t n): a FLOAT subtract over a float offset map, with a 0 floor, a
 * 65535 ceiling and a +0.5 rounding step -- see xpe_offset_apply_scalar_reference
 * in xpe_preprocess_internal.h and offset_correct_float_avx2 below. Different
 * inputs, different arithmetic, different rounding.
 *
 * So the NEON and AVX-512 members were not ports of live code; they were ports
 * of dead code. Deleting them loses no work a future aarch64 port could use,
 * because what such a port has to translate is the FLOAT path. Keeping them is
 * the harm: they are the wrong starting point, sitting in the file, looking
 * finished.
 *
 * That is the ground QA-A-73 removed xpe_has_avx512f() on -- something with no
 * consumer is something a later reader adopts as-is.
 *
 * MEASURED, NOT ASSUMED: all four had zero callers across modules/, counted per
 * function with definitions and calls tallied separately. Every one of them
 * carried `@MX:REASON: ... fan_in = 2`. The annotation was wrong for all four.
 * ========================================================================= */

namespace {

// QA-A-73 (#160): the scalar float path moved to xpe_preprocess_internal.h as
// xpe_offset_apply_scalar_reference, an inline definition, so the parity test
// compiles the same source this file does without the module exporting a symbol
// to make a test possible (the QA-A-61 boundary). See the note there for what it
// is now FOR -- it is the comparand, not a fallback.

/* =========================================================================
 * REQ-P1A-010: the vector path this module takes.
 *
 * offset_correct_float_avx2 below is the whole of it. Its scalar twin lives in
 * xpe_preprocess_internal.h as xpe_offset_apply_scalar_reference, and
 * REQ-P1A-010's "bit-identical to scalar version -- verified by test suite" is
 * checked by test_offset_correct_avx2_parity.cpp. QA-A-73 made the second half
 * of that sentence true: before it the suite compared repeated calls with each
 * other, which a consistently wrong kernel passes.
 * ========================================================================= */

#if defined(__AVX2__) || defined(__clang__) || defined(_MSC_VER)



void offset_correct_float_avx2(const uint16_t* src, const float* off, uint16_t* dst, size_t n) noexcept
{
    constexpr size_t kAVX2Stride = 8;
    const __m256 zero = _mm256_setzero_ps();
    const __m256 max_u16 = _mm256_set1_ps(65535.0f);
    const __m256 round_bias = _mm256_set1_ps(0.5f);
    const size_t avx2_limit = n - (n % kAVX2Stride);

    size_t i = 0;
    for (; i < avx2_limit; i += kAVX2Stride) {
        __m128i raw_u16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        __m256 raw = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(raw_u16));
        __m256 offset = _mm256_loadu_ps(off + i);
        __m256 corrected = _mm256_sub_ps(raw, offset);
        corrected = _mm256_max_ps(corrected, zero);
        corrected = _mm256_min_ps(corrected, max_u16);
        corrected = _mm256_add_ps(corrected, round_bias);

        __m256i u32 = _mm256_cvttps_epi32(corrected);
        __m128i lo = _mm256_castsi256_si128(u32);
        __m128i hi = _mm256_extracti128_si256(u32, 1);
        __m128i u16 = _mm_packus_epi32(lo, hi);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), u16);
    }

    xpe_offset_apply_scalar_reference(src + i, off + i, dst + i, n - i);
}

// QA-A-73 (#160): xpe_has_avx2() and xpe_has_avx512f() were here. Both are gone,
// for two DIFFERENT reasons, and the difference is the point.
//
// xpe_has_avx2() could not protect anything. This module is compiled with
// /arch:AVX2 (modules/preprocess/CMakeLists.txt), so the compiler may emit AVX2
// anywhere in it, including in the code that reaches the check -- a machine
// without AVX2 faults before the probe can answer. Its body even began with
// `#if defined(__AVX2__) return true;`, which is exactly what that build
// defines, so in the shipped configuration it was a compile-time `true`. A guard
// whose arrival depends on the thing it guards against is not a guard, and a
// reader who sees one stops looking. AVX2 is a stated minimum requirement
// (SPEC-XPE-P1A section 4.6, user decision 2026-09-16), so the vector path is
// the only path and is selected at compile time below.
//
// xpe_has_avx512f() is a different case and was removed on a different ground.
// AVX-512 is NOT implied by /arch:AVX2, so that probe could in principle have
// protected something -- but it had NO CALLER, so it protected nothing in fact.
// It is removed because it is unused, not because the idea is wrong: a detection
// function with no consumer is one a future AVX-512 path would find and trust
// as-is, when what that path will need is a probe written against the
// requirement in force at the time. Same standard QA-A-61 applied to exports --
// nothing survives on the strength of a consumer that does not exist.
//
// Neither was `static`, so the symbols had external linkage; both were unused
// outside this file (checked across modules/ in QA-A-72 and re-checked here), so
// removing them narrows nothing a caller could reach.

#endif // __AVX2__ || __clang__ || _MSC_VER


} // namespace

/* =========================================================================
 * Runtime Dispatch Implementation
 * Hierarchical fallback: AVX-512F → AVX2 → NEON → Scalar
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_offset_correct — public API entry point (new g_calib-based)
// @MX:REASON: Called by pipeline; reads g_calib.offset_map (float32); fan_in >= 3
// @MX:SPEC: REQ-P1A-009, REQ-P1A-020
extern "C" XPE_API XpeErrorCode xpe_offset_correct(
    const XpeImageBuffer*  input,
    XpeImageBuffer*         output,
    const XpeImageMetadata* metadata)
{
    if (!input || !output || !metadata) return XPE_ERR_INVALID_INPUT;
    if (!input->data || !output->data) return XPE_ERR_INVALID_INPUT;
    if (input->format != XPE_PIXEL_UINT16) return XPE_ERR_UNSUPPORTED_FORMAT;
    if (input->width == 0 || input->height == 0) return XPE_ERR_INVALID_INPUT;
    if (input->width > std::numeric_limits<size_t>::max() / input->height) return XPE_ERR_INVALID_INPUT;
    if (output->width  != input->width ||
        output->height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;

    const size_t n = static_cast<size_t>(input->width) * input->height;

    // #123 dataSize input contract (docs/project/api-spec.md, 2026-09-10):
    // 0 means *unspecified* -- trust the dimensions. A non-zero value smaller
    // than the dimensions require is refused here, before the kernel reads
    // width*height pixels past the end of the allocation (QA-B-18).
    if (input->dataSize != 0 && input->dataSize < n * sizeof(uint16_t))
        return XPE_ERR_INVALID_INPUT;
    if (output->dataSize < n * sizeof(uint16_t)) return XPE_ERR_BUFFER_TOO_SMALL;

    const uint16_t* src = static_cast<const uint16_t*>(input->data);
    uint16_t* dst = static_cast<uint16_t*>(output->data);
    std::vector<float> offmap;

    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        // SPEC-XPE-P1A REQ-P1A-020: while the module is not initialized, every
        // processing function returns XPE_ERR_NOT_INITIALIZED. Checked explicitly --
        // before #117 decision B the missing calibration map stood in for this, which
        // is why the two states could not be told apart.
        if (!xpe_preprocess_is_initialized()) return XPE_ERR_NOT_INITIALIZED;

        // #117 decision B: the module is initialized -- what is missing is the
        // calibration map. XPE_ERR_NOT_INITIALIZED is reserved for
        // xpe_preprocess_init() not called / after shutdown (SPEC-XPE-P1A
        // REQ-P1A-020), so the caller can tell the two apart.
        if (!g_calib.offset_map) return XPE_ERR_CALIB_NOT_LOADED;
        if (g_calib.offset_width  != input->width ||
            g_calib.offset_height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;

        offmap.assign(g_calib.offset_map.get(), g_calib.offset_map.get() + n);
    }

    // One path per platform, chosen at compile time. The two arms of the old
    // #if defined(__aarch64__) split called the same function, so the split said
    // nothing and is gone with the probe.
#if defined(__AVX2__) || defined(_MSC_VER)
    offset_correct_float_avx2(src, offmap.data(), dst, n);
#else
    xpe_offset_apply_scalar_reference(src, offmap.data(), dst, n);
#endif

    output->format        = XPE_PIXEL_UINT16;
    output->bitsAllocated = 16u;
    output->bitsStored    = 16u;
    output->dataSize      = n * sizeof(uint16_t);
    return XPE_OK;
}
