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
#include <cstdio>
#include <cstring>

/* ============================================================================
 * Constants
 * ============================================================================ */

// @MX:NOTE: [AUTO] Minimum gain value to prevent division by zero
// Gain map value guard. The range constants below are NOT the requirement's:
// SRS-CALIB-001 FUNC-002 sets [0.1, 10.0] and xpe_calib_load_gain enforces it
// at load (QA-A-107, #188), so a map that reaches this function has already
// passed the requirement's range. 0.001/1000 had no source -- the AC-GAIN-005
// this comment used to cite does not exist in the SPEC (searched .moai/specs
// and docs: only AC-GAIN-001..003). Kept as a second line of defence for maps
// that do not come through the loader (none today).
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
 * Second line of defence; the requirement's range is enforced at load
 * (SRS-CALIB-001 FUNC-002, xpe_calib_load_gain).
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

// QA-A-72 (#160): xpe_gain_has_avx2() was here, and it was a correct probe --
// CPUID leaves 1 and 7 for the AVX and AVX2 bits, OSXSAVE plus XGETBV for the
// operating system actually saving YMM state. It still could not protect
// anything, and the user decision of 2026-09-16 settled what to do about that.
//
// It could not protect anything because this whole module is compiled with
// /arch:AVX2 (modules/preprocess/CMakeLists.txt). The compiler is free to emit
// AVX2 instructions anywhere in it, INCLUDING IN THE CODE THAT REACHES THIS
// CHECK, so a machine without AVX2 faults before the probe can return false. A
// guard whose own arrival depends on the thing it guards against is not a
// guard.
//
// And the damage of keeping it is not neutral: a reader who sees a runtime
// check concludes the case is handled and stops looking. A guard that cannot
// protect is worse than no guard.
//
// AVX2 is now a stated minimum requirement (SPEC-XPE-P1A section 4.6), so the
// vector path is the only path and is called unconditionally below. The scalar
// form is NOT deleted -- section 4.6 names it as the reference implementation,
// and it now lives as an inline function in xpe_preprocess_internal.h with a
// test that compares the shipped path against it. It stopped being a fallback
// and became a check.

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
// QA-A-72 (#160): the per-pixel rule and the whole-buffer reference moved to
// xpe_preprocess_internal.h as inline definitions, so the parity test compiles
// the same source this file does. See the note on
// xpe_gain_apply_scalar_reference there.

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
        output[i] = xpe_gain_apply_scalar_pixel(input[i], reciprocal_gain[i]);  // Use inline helper
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
// (definition moved to xpe_preprocess_internal.h -- QA-A-72)

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
    if (n > std::numeric_limits<size_t>::max() / sizeof(float)) return XPE_ERR_INVALID_INPUT;
    if (output->dataSize < n * sizeof(float)) return XPE_ERR_BUFFER_TOO_SMALL;

    const uint16_t* src = static_cast<const uint16_t*>(input->data);
    float*          dst = static_cast<float*>(output->data);

    try {
        std::vector<float> gainmap;
        std::vector<float> poly;          // QA-A-121: per-pixel coefficients
        uint32_t poly_coeffs = 0;
        bool   poly_has_range = false;    // QA-A-123 (#194): fitted dose range
        double poly_dose_min  = 0.0;
        double poly_dose_max  = 0.0;
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
            // QA-A-107 (#187): a loaded gain POLYNOMIAL is not "no calibration".
            // No API applies G(x,y,E) yet, so this call cannot run -- but it says
            // so, instead of reporting the state as an empty calibration and
            // leaving the operator to guess.
            // QA-A-121 (#187): a loaded polynomial is now APPLIED here.
            //
            // FUNC-027 fits, per pixel, gain as a function of dose. At
            // correction time the value that says where this pixel sits on its
            // own curve is the pixel itself -- the curve is walked backwards,
            // not handed a dose from outside. A single dose per frame would be
            // the wrong shape anyway: pixels in one frame receive different
            // amounts, which is the reason the fit is per pixel at all.
            //
            // UNITS: the abscissa is whatever `dose_levels` held when the file
            // was generated ("mGy or relative units"). Indexing by the pixel's
            // own value is consistent when those levels were expressed in
            // pixel-value units, which is what the reference dataset does
            // (tests/test_data/cyan_test: CalSet levels named by ADU).
            if (!g_calib.gain_map && g_calib.gain_poly_coeffs) {
                if (g_calib.gain_width  != input->width ||
                    g_calib.gain_height != input->height) {
                    return XPE_ERR_BUFFER_TOO_SMALL;
                }
                poly_coeffs = g_calib.gain_poly_num_coeffs;
                if (poly_coeffs == 0) return XPE_ERR_INVALID_CALIB_DATA;
                poly.assign(g_calib.gain_poly_coeffs.get(),
                            g_calib.gain_poly_coeffs.get() + n * poly_coeffs);
                poly_has_range = g_calib.gain_poly_has_range;
                poly_dose_min  = g_calib.gain_poly_dose_min;
                poly_dose_max  = g_calib.gain_poly_dose_max;
            } else if (!g_calib.gain_map) {
                return XPE_ERR_CALIB_NOT_LOADED;
            }
            // The polynomial path copied its coefficients above and builds the
            // map below, outside the lock, because it reads the input frame.
            // The scalar path copies its map here.
            if (poly.empty()) {
                if (g_calib.gain_width  != input->width ||
                    g_calib.gain_height != input->height) {
                    return XPE_ERR_BUFFER_TOO_SMALL;
                }
                gainmap.assign(g_calib.gain_map.get(), g_calib.gain_map.get() + n);
            }
        }

        // QA-A-121: evaluate the per-pixel polynomial at the pixel's own value.
        // Horner from the highest coefficient down, so the layout
        // [p * poly_coeffs + j] is read once per pixel in order.
        //
        // QA-A-123 (#194): the fit says nothing outside [dose_min, dose_max],
        // so the abscissa is clamped into it first. Measured on a steeply
        // curved ladder, a saturated pixel (65535) was otherwise evaluated at
        // 2.78x the top-knot gain and came out DARKER than a D_max pixel --
        // monotonicity inverted exactly where direct-exposure, metal and
        // saturation live. Clamping pins those pixels to the edge gain, which
        // is the nearest value the calibration actually measured.
        //
        // Rejecting the frame instead was considered and declined: a handful
        // of saturated pixels would discard the whole image, which is the
        // heavier failure. Clamp + one alert keeps the frame and still says
        // what happened.
        size_t clamped_count = 0;
        if (!poly.empty()) {
            gainmap.resize(n);
            for (size_t i = 0; i < n; ++i) {
                float x = static_cast<float>(src[i]);
                if (poly_has_range) {
                    if (x < static_cast<float>(poly_dose_min)) {
                        x = static_cast<float>(poly_dose_min);
                        ++clamped_count;
                    } else if (x > static_cast<float>(poly_dose_max)) {
                        x = static_cast<float>(poly_dose_max);
                        ++clamped_count;
                    }
                }
                const float* c = poly.data() + i * poly_coeffs;
                float acc = c[poly_coeffs - 1];
                for (uint32_t j = poly_coeffs - 1; j > 0; --j) {
                    acc = acc * x + c[j - 1];
                }
                gainmap[i] = acc;
            }

            // One alert for the frame, carrying the count. Pushing per pixel
            // would put tens of thousands of identical lines in the queue and
            // make the queue itself useless.
            if (clamped_count > 0) {
                char msg[256];
                std::snprintf(msg, sizeof(msg),
                    "%zu pixel(s) fell outside the gain polynomial's fitted "
                    "dose range [%.1f, %.1f] and were evaluated at the range "
                    "edge; values beyond the calibrated levels are not "
                    "extrapolated (issue #194)",
                    clamped_count, poly_dose_min, poly_dose_max);
                xpe_alert_push(msg, XPE_ALERT_WARNING);
            }
        }

        // Validate gain map and precompute reciprocals
        std::vector<float> reciprocal(n);
        for (size_t i = 0; i < n; ++i) {
            if (!is_valid_gain(gainmap[i])) return XPE_ERR_CONFIG_INVALID;
            reciprocal[i] = 1.0f / gainmap[i];
        }

        // Apply. One path: see the QA-A-72 note where the runtime probe used to be.
        apply_gain_avx2(src, reciprocal.data(), dst, input->width, input->height);

        output->format        = XPE_PIXEL_FLOAT32;
        output->bitsAllocated = 32u;
        output->bitsStored    = 32u;
        output->dataSize      = n * sizeof(float);
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    }
}
