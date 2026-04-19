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

} // namespace

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

    // REQ-P1A-010: Dispatch to AVX2 if available, otherwise scalar
    // Both implementations produce bit-identical results
    if (xpe_has_avx2() && n >= 16) {
        // @MX:NOTE: [AUTO] AVX2 threshold: 16 pixels minimum for vectorization benefit
        offset_correct_avx2(dst, off, n);
    } else {
        offset_correct_scalar(dst, off, n);
    }

    return XPE_OK;
}

#else // No AVX2 support available

// Fallback for compilers without AVX2 intrinsics support
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
    offset_correct_scalar(dst, off, n);
    return XPE_OK;
}

#endif // __AVX2__

// @MX:ANCHOR: [AUTO] xpe_offset_correct — public API entry point
// @MX:REASON: Called by pipeline and directly by calibration manager; fan_in >= 3
// @MX:SPEC: REQ-P1A-009
XpeErrorCode xpe_offset_correct(XpeImageBuffer* img,
                                 const XpeImageBuffer* offsetMap)
{
    return xpe_offset_correct_dispatch(img, offsetMap);
}
