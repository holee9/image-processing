/**
 * @file offset_correct.cpp
 * @brief SWU-1.1: Per-pixel dark offset subtraction (PRE-02)
 *        REQ-P1A-009 to REQ-P1A-011
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <cstdint>

// @MX:NOTE: [AUTO] AVX2 intrinsics header — conditional include based on _MSC_VER
#if defined(_MSC_VER)
#include <intrin.h>
#include <immintrin.h>
#else
#include <immintrin.h>
#endif

/* =========================================================================
 * REQ-P1A-010: Scalar fallback implementation (portable, always available)
 * corrected[i] = max(raw[i] - offset[i], 0) — saturating subtraction
 * ========================================================================= */

namespace {
// @MX:ANCHOR: [AUTO] offset_correct_scalar — saturating subtraction kernel
// @MX:REASON: Core algorithm called by both scalar and AVX2 dispatch paths; fan_in = 2
// @MX:SPEC: REQ-P1A-009
void offset_correct_scalar(uint16_t* dst, const uint16_t* off, size_t n) noexcept
{
    for (size_t i = 0; i < n; ++i)
        dst[i] = (dst[i] > off[i]) ? static_cast<uint16_t>(dst[i] - off[i]) : uint16_t{0};
}

/* =========================================================================
 * REQ-P1A-010: AVX2 implementation (branch-free saturating subtract)
 * Uses _mm256_subs_epu16 for unsigned 16-bit saturating subtraction.
 * Bit-identical to scalar version — verified by test suite.
 * ========================================================================= */

#if defined(__AVX2__) || defined(__clang__) || defined(_MSC_VER)

/* =========================================================================
 * AVX-512 Implementation (512-bit vectors, 32x uint16 per iteration)
 * REQ-P1A-010: Bit-identical to scalar baseline
 * ========================================================================= */

#if defined(__AVX512F__) || defined(_MSC_VER)
// @MX:ANCHOR: [AUTO] offset_correct_avx512 — AVX-512 saturating subtraction kernel
// @MX:REASON: Highest performance on Skylake-X/Ice Lake; processes 32 pixels per iteration; fan_in = 2
// @MX:SPEC: REQ-P1A-010
// @MX:WARN: Requires 64-byte alignment for optimal performance; falls back to scalar for tails
void offset_correct_avx512(uint16_t* dst, const uint16_t* off, size_t n) noexcept
{
    // @MX:NOTE: [AUTO] AVX-512 processes 32 uint16_t values (512 bits / 16 bits per element)
    constexpr size_t kAVX512Stride = 32;

    size_t i = 0;

    // Main loop: 32 pixels per iteration using _mm512_subs_epu16
    // @MX:NOTE: [AUTO] _mm512_subs_epu16: unsigned saturating subtraction (a-b) saturated to 0
    // This intrinsic is branch-free and generates a single vpsubusw instruction
    const size_t avx512_limit = n - (n % kAVX512Stride);

    for (; i < avx512_limit; i += kAVX512Stride) {
        // Load 32 uint16_t values from dst and off
        __m512i dst_vec = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(dst + i));
        __m512i off_vec = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(off + i));

        // Saturating subtraction: dst_vec = max(dst_vec - off_vec, 0)
        // _mm512_subs_epu16 automatically clamps underflow to 0
        __m512i result = _mm512_subs_epu16(dst_vec, off_vec);

        // Store result back to dst
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(dst + i), result);
    }

    // Tail loop: handle remaining pixels (0-31 pixels) with scalar code
    for (; i < n; ++i) {
        dst[i] = (dst[i] > off[i]) ? static_cast<uint16_t>(dst[i] - off[i]) : uint16_t{0};
    }
}
#endif // __AVX512F__

/* =========================================================================
 * AVX2 Implementation (256-bit vectors, 16x uint16 per iteration)
 * REQ-P1A-010: Bit-identical to scalar baseline
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] offset_correct_avx2 — AVX2 saturating subtraction kernel
// @MX:REASON: Performance-critical hot path; processes 16 pixels per iteration; fan_in = 2
// @MX:SPEC: REQ-P1A-010
// @MX:WARN: Requires 32-byte alignment for optimal performance; falls back to scalar for tails
void offset_correct_avx2(uint16_t* dst, const uint16_t* off, size_t n) noexcept
{
    // @MX:NOTE: [AUTO] AVX2 processes 16 uint16_t values (256 bits / 16 bits per element)
    constexpr size_t kAVX2Stride = 16;

    size_t i = 0;

    // Main loop: 16 pixels per iteration using _mm256_subs_epu16
    // @MX:NOTE: [AUTO] _mm256_subs_epu16: unsigned saturating subtraction (a-b) saturated to 0
    // This intrinsic is branch-free and generates a single vpsubusw instruction
    const size_t avx2_limit = n - (n % kAVX2Stride);

    for (; i < avx2_limit; i += kAVX2Stride) {
        // Load 16 uint16_t values from dst and off
        __m256i dst_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst + i));
        __m256i off_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(off + i));

        // Saturating subtraction: dst_vec = max(dst_vec - off_vec, 0)
        // _mm256_subs_epu16 automatically clamps underflow to 0
        __m256i result = _mm256_subs_epu16(dst_vec, off_vec);

        // Store result back to dst
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), result);
    }

    // Tail loop: handle remaining pixels (0-15 pixels) with scalar code
    for (; i < n; ++i) {
        dst[i] = (dst[i] > off[i]) ? static_cast<uint16_t>(dst[i] - off[i]) : uint16_t{0};
    }
}

// @MX:ANCHOR: [AUTO] xpe_has_avx2 — runtime AVX2 detection
// @MX:REASON: CPU feature detection required for safe dispatch; fan_in = 1
// @MX:SPEC: REQ-P1A-010
bool xpe_has_avx2() noexcept
{
#if defined(__AVX2__)
    // Compile-time known: AVX2 is available
    return true;
#elif defined(_MSC_VER)
    // MSVC: use __cpuidex for runtime detection
    int cpuInfo[4];
    __cpuidex(cpuInfo, 0, 0);  // Get max leaf
    if (cpuInfo[0] < 7) return false;

    __cpuidex(cpuInfo, 7, 0);  // Leaf 7, subleaf 0
    return (cpuInfo[1] & (1 << 5)) != 0;  // EBX bit 5 = AVX2
#else
    // GCC/Clang: use __builtin_cpu_supports
    return __builtin_cpu_supports("avx2");
#endif
}

// @MX:ANCHOR: [AUTO] xpe_has_avx512f — runtime AVX-512F detection
// @MX:REASON: CPU feature detection required for safe AVX-512 dispatch; fan_in = 1
// @MX:SPEC: REQ-P1A-010
bool xpe_has_avx512f() noexcept
{
#if defined(__AVX512F__)
    // Compile-time known: AVX-512F is available
    return true;
#elif defined(_MSC_VER)
    // MSVC: use __cpuidex for runtime detection
    int cpuInfo[4];
    __cpuidex(cpuInfo, 0, 0);  // Get max leaf
    if (cpuInfo[0] < 7) return false;

    // Check OSXSAVE (leaf 1, ECX bit 27)
    __cpuidex(cpuInfo, 1, 0);
    if ((cpuInfo[2] & (1 << 27)) == 0) return false;

    // Check AVX-512F (leaf 7, EBX bit 16)
    __cpuidex(cpuInfo, 7, 0);
    if ((cpuInfo[1] & (1 << 16)) == 0) return false;

    // Check ZMM/YMM/XMM state saved by OS (XGETBV bits 7:5, 2:1, 0)
    unsigned long long xcr_mask = _xgetbv(0);
    return (xcr_mask & 0xE6) == 0xE6;
#else
    // GCC/Clang: use __builtin_cpu_supports
    return __builtin_cpu_supports("avx512f");
#endif
}

#endif // __AVX2__ || __clang__ || _MSC_VER

/* =========================================================================
 * NEON Implementation (ARM64, 128-bit vectors, 8x uint16 per iteration)
 * REQ-P1A-010: Bit-identical to scalar baseline
 * ========================================================================= */

#if defined(__aarch64__)
#include <arm_neon.h>

// @MX:ANCHOR: [AUTO] offset_correct_neon — NEON saturating subtraction kernel
// @MX:REASON: ARM64 primary SIMD path; processes 8 pixels per iteration; fan_in = 2
// @MX:SPEC: REQ-P1A-010
// @MX:WARN: Requires 16-byte alignment for optimal performance; falls back to scalar for tails
void offset_correct_neon(uint16_t* dst, const uint16_t* off, size_t n) noexcept
{
    // @MX:NOTE: [AUTO] NEON processes 8 uint16_t values (128 bits / 16 bits per element)
    constexpr size_t kNEONStride = 8;

    size_t i = 0;

    // Main loop: 8 pixels per iteration using vqsubq_u16
    // @MX:NOTE: [AUTO] vqsubq_u16: unsigned saturating subtraction (a-b) saturated to 0
    // This intrinsic is branch-free and generates a single uqsub instruction
    const size_t neon_limit = n - (n % kNEONStride);

    for (; i < neon_limit; i += kNEONStride) {
        // Load 8 uint16_t values from dst and off
        uint16x8_t dst_vec = vld1q_u16(dst + i);
        uint16x8_t off_vec = vld1q_u16(off + i);

        // Saturating subtraction: dst_vec = max(dst_vec - off_vec, 0)
        // vqsubq_u16 automatically clamps underflow to 0
        uint16x8_t result = vqsubq_u16(dst_vec, off_vec);

        // Store result back to dst
        vst1q_u16(dst + i, result);
    }

    // Tail loop: handle remaining pixels (0-7 pixels) with scalar code
    for (; i < n; ++i) {
        dst[i] = (dst[i] > off[i]) ? static_cast<uint16_t>(dst[i] - off[i]) : uint16_t{0};
    }
}

#endif // __aarch64__

} // namespace

/* =========================================================================
 * Runtime Dispatch Implementation
 * Hierarchical fallback: AVX-512F → AVX2 → NEON → Scalar
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_offset_correct_dispatch — runtime dispatch implementation
// @MX:REASON: Public API entry point; selects optimal implementation at runtime; fan_in >= 3
// @MX:SPEC: REQ-P1A-009
static XpeErrorCode xpe_offset_correct_dispatch(XpeImageBuffer* img,
                                                 const XpeImageBuffer* offsetMap) noexcept
{
    if (!img || !offsetMap) return XPE_ERR_INVALID_INPUT;
    if (!xpe_dims_match(img, offsetMap)) return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(offsetMap, XPE_PIXEL_UINT16)) return XPE_ERR_INVALID_INPUT;

    auto* dst = static_cast<uint16_t*>(img->data);
    const auto* off = static_cast<const uint16_t*>(offsetMap->data);

    // REQ-P1A-010: Hierarchical SIMD dispatch
    // Priority: AVX-512F > AVX2 > NEON > Scalar
    // All implementations produce bit-identical results

#if defined(__aarch64__)
    // ARM64 path: NEON → Scalar
    if (n >= 8) {
        offset_correct_neon(dst, off, n);
    } else {
        offset_correct_scalar(dst, off, n);
    }
#else
    // x86-64 path: AVX-512F → AVX2 → Scalar
    #if defined(__AVX512F__) || defined(_MSC_VER)
    if (xpe_has_avx512f() && n >= 32) {
        // @MX:NOTE: [AUTO] AVX-512 threshold: 32 pixels minimum for vectorization benefit
        offset_correct_avx512(dst, off, n);
    } else
    #endif
    #if defined(__AVX2__) || defined(_MSC_VER)
    if (xpe_has_avx2() && n >= 16) {
        // @MX:NOTE: [AUTO] AVX2 threshold: 16 pixels minimum for vectorization benefit
        offset_correct_avx2(dst, off, n);
    } else {
        offset_correct_scalar(dst, off, n);
    }
    #else
    {
        // No SIMD support available
        offset_correct_scalar(dst, off, n);
    }
    #endif
#endif

    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] xpe_offset_correct — public API entry point
// @MX:REASON: Called by pipeline and directly by calibration manager; fan_in >= 3
// @MX:SPEC: REQ-P1A-009
XpeErrorCode xpe_offset_correct(XpeImageBuffer* img,
                                 const XpeImageBuffer* offsetMap)
{
    return xpe_offset_correct_dispatch(img, offsetMap);
}
