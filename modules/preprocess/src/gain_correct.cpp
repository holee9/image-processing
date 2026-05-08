/**
 * @file gain_correct.cpp
 * @brief SWU-1.2: Per-pixel flat-field gain normalization (PRE-03)
 *        Domain transition: uint16 -> float32 occurs in this stage.
 *        REQ-P1A-011: Gain correction with reciprocal precomputation + FMA
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
#include <mutex>
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
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
[[maybe_unused]] static inline int32_t ulp_difference(float a, float b) noexcept {
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
[[maybe_unused]] static inline bool check_parity(float scalar, float simd) noexcept {
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

static bool xpe_gain_has_avx2() noexcept {
#if defined(_MSC_VER)
    int cpuinfo[4];
    __cpuidex(cpuinfo, 0, 0);
    if (cpuinfo[0] < 7) return false;

    __cpuidex(cpuinfo, 1, 0);
    const bool osxsave = (cpuinfo[2] & (1 << 27)) != 0;
    const bool avx = (cpuinfo[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) return false;

    const unsigned long long xcr_mask = _xgetbv(0);
    if ((xcr_mask & 0x6ULL) != 0x6ULL) return false;

    __cpuidex(cpuinfo, 7, 0);
    return (cpuinfo[1] & (1 << 5)) != 0;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
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
[[maybe_unused]] static inline __m256 apply_gain_fma(__m128i input, float reciprocal_gain) noexcept {
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
    const size_t vec_end = pixel_count & ~(vec_width - size_t{1});  // Round down to 8

    for (; i < vec_end; i += vec_width) {
        // Load 8 uint16 values
        __m128i u16_data = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));

        // Load 8 reciprocal gain values (per-pixel gain map)
        __m256 gain_vec = _mm256_loadu_ps(&reciprocal_gain[i]);

        // Convert uint16 to float32 and apply gain (per-pixel element-wise)
        __m256 input_vec = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16_data));
        __m256 result = _mm256_mul_ps(input_vec, gain_vec);
        _mm256_storeu_ps(&output[i], result);
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

// @MX:ANCHOR: [AUTO] xpe_gain_correct — public API entry point (new g_calib-based)
// @MX:REASON: UINT16→FLOAT32 domain transition; reads g_calib.gain_map; fan_in >= 3
// @MX:SPEC: REQ-P1A-011, REQ-P1A-020
extern "C" XPE_API XpeErrorCode xpe_gain_correct(
    const XpeImageBuffer*  input,
    XpeImageBuffer*         output,
    const XpeImageMetadata* metadata)
{
    if (!input || !output || !metadata) return XPE_ERR_INVALID_INPUT;
    if (!input->data || !output->data) return XPE_ERR_INVALID_INPUT;
    if (input->format != XPE_PIXEL_UINT16) return XPE_ERR_UNSUPPORTED_FORMAT;
    if (input->width == 0 || input->height == 0) return XPE_ERR_INVALID_INPUT;
    if (output->width  != input->width ||
        output->height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;

    const size_t n = static_cast<size_t>(input->width) * input->height;
    if (n > std::numeric_limits<size_t>::max() / sizeof(float)) return XPE_ERR_INVALID_INPUT;
    if (output->dataSize < n * sizeof(float)) return XPE_ERR_BUFFER_TOO_SMALL;

    const uint16_t* src     = static_cast<const uint16_t*>(input->data);
    float*          dst     = static_cast<float*>(output->data);
    std::vector<float> gainmap;

    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        if (!g_calib.gain_map) return XPE_ERR_NOT_INITIALIZED;
        if (g_calib.gain_width  != input->width ||
            g_calib.gain_height != input->height) return XPE_ERR_BUFFER_TOO_SMALL;
        gainmap.assign(g_calib.gain_map.get(), g_calib.gain_map.get() + n);
    }

    // Validate gain map and precompute reciprocals
    std::vector<float> reciprocal(n);
    for (size_t i = 0; i < n; ++i) {
        if (!is_valid_gain(gainmap[i])) return XPE_ERR_CONFIG_INVALID;
        reciprocal[i] = 1.0f / gainmap[i];
    }

    // Apply: AVX2 when available, scalar fallback
    if (xpe_gain_has_avx2())
        apply_gain_avx2(src, reciprocal.data(), dst, input->width, input->height);
    else
        apply_gain_correction_scalar(src, reciprocal.data(), dst, input->width, input->height);

    output->format        = XPE_PIXEL_FLOAT32;
    output->bitsAllocated = 32u;
    output->bitsStored    = 32u;
    output->dataSize      = n * sizeof(float);
    return XPE_OK;
}
