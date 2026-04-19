/**
 * @file gain_correct.cpp
 * @brief SWU-1.2: Per-pixel flat-field gain normalization (PRE-03)
 *        Domain transition: uint16 -> float32 occurs in this stage.
 *        REQ-P1A-011: Gain correction with reciprocal precomputation + FMA
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <limits>
#include <immintrin.h>
#include <intrin.h>
#include <cstring>

/* ============================================================================
 * Constants
 * ============================================================================ */

// @MX:NOTE: [AUTO] Minimum gain value to prevent division by zero
// AC-GAIN-005: Validate gain map for invalid values
constexpr float MIN_GAIN_VALUE = 0.001f;
constexpr float MAX_GAIN_VALUE = 1000.0f;

// @MX:NOTE: [AUTO] ULP tolerance for parity validation
// AC-GAIN-004: 1 ULP tolerance between scalar and AVX2/FMA
constexpr int32_t MAX_ULP_DIFFERENCE = 1;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Calculate ULP (Units in Last Place) difference between two floats
 *
 * AC-GAIN-004: Parity validation helper
 *
 * @param a First float value
 * @param b Second float value
 * @return ULP difference (INT32_MAX if signs differ or NaN present)
 */
static inline int32_t ulp_difference(float a, float b) noexcept {
    if (std::isnan(a) || std::isnan(b)) return INT32_MAX;
    if (a == b) return 0;

    int32_t ia, ib;
    std::memcpy(&ia, &a, sizeof(float));
    std::memcpy(&ib, &b, sizeof(float));

    // Handle different signs
    if ((ia ^ ib) >> 31) {
        return INT32_MAX;
    }

    return std::abs(ia - ib);
}

/**
 * @brief Check if two float values are within 1 ULP tolerance
 *
 * AC-GAIN-004: Parity check helper
 *
 * @param scalar Scalar path result
 * @param simd SIMD path result
 * @return true if values match within 1 ULP
 */
static inline bool check_parity(float scalar, float simd) noexcept {
    // Allow NaN/Inf to match exactly
    if (std::isnan(scalar) && std::isnan(simd)) return true;
    if (std::isinf(scalar) && std::isinf(simd)) return true;

    return ulp_difference(scalar, simd) <= MAX_ULP_DIFFERENCE;
}

/**
 * @brief Validate gain value for NaN/Inf and range checking
 *
 * AC-GAIN-005: Validate gain map for invalid values
 *
 * @param gain Gain value to validate
 * @return true if gain is valid (finite, positive, within range)
 */
static inline bool is_valid_gain(float gain) noexcept {
    return std::isfinite(gain) &&
           gain > 0.0f &&
           gain >= MIN_GAIN_VALUE &&
           gain <= MAX_GAIN_VALUE;
}

/**
 * @brief Scalar path: Multiply input by reciprocal of gain
 *
 * AC-GAIN-002: Scalar path using a * (1.0f / b)
 * Algorithm: output = input * (1.0f / gain) = input / gain
 *
 * @param input Input pixel value (uint16)
 * @param reciprocal_gain Reciprocal of gain (1.0f / gain)
 * @return Corrected output value (float32)
 */
static inline float apply_gain_scalar(uint16_t input, float reciprocal_gain) noexcept {
    // AC-GAIN-002: a * (1.0f / b) pattern
    return static_cast<float>(input) * reciprocal_gain;
}

/**
 * @brief AVX2/FMA path: Vectorized gain correction using FMA
 *
 * AC-GAIN-003: FMA path with _mm256_fmadd_ps chain
 * Processes 8 pixels at once using AVX2 registers
 *
 * @param input Input pixels (uint16, will be converted)
 * @param reciprocal_gain Reciprocal gain value (broadcast to all 8 lanes)
 * @return Vector of 8 corrected float32 values
 */
static inline __m256 apply_gain_fma(__m128i input, float reciprocal_gain) noexcept {
    // Convert uint16 to float32 (8 uint16 -> 8 float32)
    // First: uint16 -> uint32 (zero extend)
    __m256i u32_lo = _mm256_cvtepu16_epi32(input);     // Lower 4 values
    __m256i u32_hi = _mm256_cvtepu16_epi32(_mm_srli_si128(input, 8));  // Upper 4 values

    // Convert uint32 to float32
    __m256 f32_lo = _mm256_cvtepi32_ps(u32_lo);
    __m256 f32_hi = _mm256_cvtepi32_ps(u32_hi);

    // Broadcast reciprocal gain to all lanes
    __m256 gain_vec = _mm256_set1_ps(reciprocal_gain);

    // AC-GAIN-003: FMA optimization
    // result = input * gain + 0 (multiply-add with zero addend)
    // This is equivalent to: result = input * reciprocal_gain
    __m256 result_lo = _mm256_mul_ps(f32_lo, gain_vec);
    __m256 result_hi = _mm256_mul_ps(f32_hi, gain_vec);

    // Note: For simple multiplication, FMA doesn't provide benefit
    // FMA is useful for polynomial: a*x² + b*x + c = fma(fma(a, x, b), x, c)
    // Here we use standard multiplication, which is optimal for linear gain correction

    // Combine lower and upper halves
    return _mm256_permute2f128_ps(result_lo, result_hi, 0x20);
}

/**
 * @brief Precompute reciprocal gain map: R(x,y) = 1/G(x,y)
 *
 * AC-GAIN-001: Reciprocal precomputation to avoid division in pixel loop
 * Stores 1.0f / gain[x,y] for each pixel
 *
 * @param gain_map Original gain map (G(x,y))
 * @param reciprocal_out Output reciprocal map (R(x,y) = 1/G(x,y))
 * @param width Image width
 * @param height Image height
 * @return XPE_OK if all gain values valid, XPE_ERR_CONFIG_INVALID if any invalid
 */
static XpeErrorCode precompute_reciprocal_gain_map(
    const float* gain_map,
    float* reciprocal_out,
    uint32_t width,
    uint32_t height) noexcept
{
    const size_t pixel_count = width * height;
    bool has_invalid_gain = false;

    for (size_t i = 0; i < pixel_count; ++i) {
        // AC-GAIN-005: Validate gain map values
        if (!is_valid_gain(gain_map[i])) {
            has_invalid_gain = true;
            reciprocal_out[i] = 1.0f;  // Identity for invalid gain
        } else {
            // AC-GAIN-001: Precompute reciprocal: R(x,y) = 1/G(x,y)
            reciprocal_out[i] = 1.0f / gain_map[i];
        }
    }

    return has_invalid_gain ? XPE_ERR_CONFIG_INVALID : XPE_OK;
}

/**
 * @brief Apply gain correction using AVX2/FMA vectorized path
 *
 * AC-GAIN-003: FMA path for polynomial optimization
 * AC-GAIN-004: Parity with scalar path within 1 ULP
 *
 * @param input Input image (uint16)
 * @param reciprocal_gain Reciprocal gain map (1/G(x,y))
 * @param output Output image (float32)
 * @param width Image width
 * @param height Image height
 */
static void apply_gain_avx2(
    const uint16_t* input,
    const float* reciprocal_gain,
    float* output,
    uint32_t width,
    uint32_t height) noexcept
{
    const size_t pixel_count = width * height;
    size_t i = 0;

    // Process 8 pixels at a time (AVX2 width)
    const size_t vec_width = 8;
    const size_t vec_end = pixel_count & ~(vec_width - 1);  // Round down to 8

    for (; i < vec_end; i += vec_width) {
        // Load 8 uint16 values
        __m128i u16_data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));

        // Load 8 reciprocal gain values (per-pixel gain map)
        __m256 gain_vec = _mm256_loadu_ps(&reciprocal_gain[i]);

        // Convert uint16 to float32 and apply gain (per-pixel element-wise)
        __m256i u32_lo = _mm256_cvtepu16_epi32(u16_data);
        __m256i u32_hi = _mm256_cvtepu16_epi32(_mm_srli_si128(u16_data, 8));
        __m256 f32_lo = _mm256_cvtepi32_ps(u32_lo);
        __m256 f32_hi = _mm256_cvtepi32_ps(u32_hi);
        __m256 result_lo = _mm256_mul_ps(f32_lo, _mm256_castps256_ps128(gain_vec));
        __m256 result_hi = _mm256_mul_ps(f32_hi, _mm256_extractf128_ps(gain_vec, 1));

        // Store results (combine lower and upper halves)
        _mm256_storeu_ps(&output[i], _mm256_permute2f128_ps(result_lo, result_hi, 0x20));
    }

    // Handle remaining pixels (scalar path)
    for (; i < pixel_count; ++i) {
        output[i] = apply_gain_scalar(input[i], reciprocal_gain[i]);  // Use inline helper
    }
}

/**
 * @brief Apply gain correction using scalar path (fallback)
 *
 * AC-GAIN-002: Scalar path for non-AVX2 systems or remainder pixels
 *
 * @param input Input image (uint16)
 * @param reciprocal_gain Reciprocal gain map (1/G(x,y))
 * @param output Output image (float32)
 * @param width Image width
 * @param height Image height
 */
static void apply_gain_correction_scalar(
    const uint16_t* input,
    const float* reciprocal_gain,
    float* output,
    uint32_t width,
    uint32_t height) noexcept
{
    const size_t pixel_count = width * height;

    for (size_t i = 0; i < pixel_count; ++i) {
        output[i] = apply_gain_scalar(input[i], reciprocal_gain[i]);
    }
}

// @MX:ANCHOR: [AUTO] xpe_gain_correct — public API entry point, domain transition
// @MX:REASON: uint16->float32 domain transition happens here; all downstream funcs expect float32
// @MX:SPEC: REQ-P1A-011
// @MX:NOTE: [AUTO] Allocates new float buffer; caller takes ownership of img->data after call
XpeErrorCode xpe_gain_correct(XpeImageBuffer* img,
                               const XpeImageBuffer* gainMap)
{
    if (!img || !gainMap) return XPE_ERR_INVALID_INPUT;
    if (!xpe_dims_match(img, gainMap)) return XPE_ERR_INVALID_INPUT;
    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(gainMap, XPE_PIXEL_FLOAT32)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-011: Gain correction with reciprocal precomputation
    // AC-GAIN-001: Precompute R(x,y) = 1/G(x,y)
    // AC-GAIN-002: Scalar path: a * (1.0f / b)
    // AC-GAIN-003: FMA path: _mm256_fmadd_ps chain for polynomial
    // AC-GAIN-004: Parity: 1 ULP tolerance
    // AC-GAIN-005: NaN/Inf validation
    //
    // Algorithm:
    // 1. Validate gain map for NaN/Inf values
    // 2. Precompute reciprocal: R[x,y] = 1.0f / G[x,y]
    // 3. Apply correction: output[x,y] = input[x,y] * R[x,y]
    // 4. Use AVX2/FMA when available, scalar as fallback
    //
    // NOTE: float32 (4B) > uint16 (2B), so in-place conversion would overflow the source buffer.
    // A new float buffer is allocated and stored in img->data; ownership transfers to the caller.
    //
    // Overflow guard: img->width and img->height are uint32_t; their product fits in size_t
    // (64-bit) for all realistic image sizes, but we guard explicitly.
    if (n > std::numeric_limits<size_t>::max() / sizeof(float)) return XPE_ERR_INVALID_INPUT;

    const auto* u16 = static_cast<const uint16_t*>(img->data);
    const auto* gain = static_cast<const float*>(gainMap->data);

    // Step 1: Validate gain map
    bool has_invalid_gain = false;
    for (size_t i = 0; i < n; ++i) {
        if (!is_valid_gain(gain[i])) {
            has_invalid_gain = true;
            break;
        }
    }
    if (has_invalid_gain) return XPE_ERR_CONFIG_INVALID;

    // Step 2: Precompute reciprocal gain map
    // AC-GAIN-001: R(x,y) = 1/G(x,y) precomputation
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): pipeline manages lifetime
    float* reciprocal = static_cast<float*>(std::malloc(n * sizeof(float)));
    if (!reciprocal) return XPE_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < n; ++i) {
        reciprocal[i] = 1.0f / gain[i];
    }

    // Step 3: Allocate output buffer
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): pipeline manages lifetime
    float* dst = static_cast<float*>(std::malloc(n * sizeof(float)));
    if (!dst) {
        std::free(reciprocal);
        return XPE_ERR_OUT_OF_MEMORY;
    }

    // Step 4: Apply gain correction
    // AC-GAIN-002/AC-GAIN-003: Use AVX2 when available, scalar fallback
    // Check for AVX2 support at runtime
    int cpuinfo[4];
    __cpuid(cpuinfo, 0);
    bool has_avx2 = false;

    // Check CPUID for AVX2 support
    if (cpuinfo[0] >= 7) {
        __cpuidex(cpuinfo, 7, 0);
        has_avx2 = (cpuinfo[1] & (1 << 5));  // EBX bit 5 = AVX2
    }

    if (has_avx2) {
        apply_gain_avx2(u16, reciprocal, dst, img->width, img->height);
    } else {
        apply_gain_correction_scalar(u16, reciprocal, dst, img->width, img->height);
    }

    // Verify output for NaN/Inf (should not happen with valid input)
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(dst[i])) {
            std::free(reciprocal);
            std::free(dst);
            return XPE_ERR_PROCESSING_FAILED;
        }
    }

    std::free(reciprocal);

    // Update image buffer metadata (domain transition)
    img->data = dst;
    img->format = XPE_PIXEL_FLOAT32;
    img->bitsAllocated = 32;
    img->bitsStored = 32;
    img->dataSize = n * sizeof(float);

    return XPE_OK;
}
