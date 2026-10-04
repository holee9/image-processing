#ifndef XPE_ENHANCE_BASIC_API_H
#define XPE_ENHANCE_BASIC_API_H

/**
 * @file enhance_basic_api.h
 * @brief XPE Basic Enhancement Module API (SPEC-XPE-P1B-ENH)
 *
 * Provides 8 exported C API functions for log transform, noise reduction,
 * contrast enhancement (CLAHE), edge enhancement (USM), exposure index
 * computation per IEC 62494-1, and the module version string.
 *
 * The five processing functions operate in-place on float32 images
 * (XPE_PIXEL_FLOAT32); xpe_noise_estimate_sigma() reads without modifying and
 * xpe_enhance_basic_version() takes no image. Every function that takes an
 * image returns XPE_ERR_UNSUPPORTED_FORMAT when the buffer is not FLOAT32, and
 * XPE_ERR_INVALID_INPUT when img or img->data is NULL, when width or height is
 * 0, or when dataSize is inconsistent with width * height * bytes-per-pixel
 * (#123).
 *
 * An empty image is an error everywhere, not a no-op (#142). Three functions
 * used to accept a zero-sized image and return XPE_OK while two others
 * rejected it; the answer is now XPE_ERR_INVALID_INPUT from all of them. The
 * dimension is judged before the pixel format, so an empty UINT16 buffer is
 * reported as empty rather than as the wrong format.
 *
 * Thread-safe for concurrent calls on independent image buffers.
 */

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)

/* ============================================================================
 * Parameter Structs
 * ============================================================================ */

/**
 * @brief Noise reduction mode selector.
 */
typedef enum XpeNoiseReduceMode {
    XPE_NOISE_BILATERAL = 0,  /**< Bilateral filter (spatial + range Gaussian) */
    XPE_NOISE_NLM       = 1   /**< Non-Local Means denoising */
} XpeNoiseReduceMode;

/**
 * @brief Parameters for xpe_noise_reduce().
 *
 * For bilateral mode: sigma_space, sigma_range are used.
 * For NLM mode: search_window, patch_size, h_param are used.
 * (REQ-ENH-007..012)
 */
typedef struct XpeNoiseReduceParams {
    XpeNoiseReduceMode mode;           /**< Bilateral or NLM */
    float              sigma_space;    /**< Bilateral: spatial sigma, 0 < s <= 7.5 (default 3.0) */
    float              sigma_range;    /**< Bilateral: range sigma (default 50.0) */
    int32_t            search_window;  /**< NLM: search window size, must be odd positive (default 21) */
    int32_t            patch_size;     /**< NLM: patch size, must be odd positive (default 7) */
    float              h_param;        /**< NLM: filtering strength (default 10.0) */
} XpeNoiseReduceParams;

/**
 * @brief Parameters for xpe_contrast_enhance() (CLAHE).
 * (REQ-ENH-013..017)
 */
typedef struct XpeClaheParams {
    float   clip_limit;    /**< Contrast clip limit, must be >= 1.0 (default 3.0) */
    int32_t tile_width;    /**< Number of horizontal tiles, must be >= 2 (default 8) */
    int32_t tile_height;   /**< Number of vertical tiles, must be >= 2 (default 8) */
} XpeClaheParams;

/**
 * @brief Parameters for xpe_edge_enhance() (Unsharp Masking).
 * (REQ-ENH-018..022)
 */
typedef struct XpeUsmParams {
    float amount;      /**< Sharpening gain, range [0.0, 5.0] (default 0.5) */
    float radius;      /**< Gaussian blur sigma, range [0.5, 10.0] (default 2.0) */
    float threshold;   /**< Edge magnitude threshold, must be >= 0.0 (default 10.0) */
} XpeUsmParams;

#pragma pack(pop)

/* ============================================================================
 * Module Version
 * ============================================================================ */

/**
 * @brief Returns the xpe_enhance_basic module version string.
 * @return Null-terminated version string. Lifetime: process. Never NULL.
 */
XPE_API const char* xpe_enhance_basic_version(void);

/**
 * @brief Set how many threads the per-pixel passes of this module may use.
 *
 * Add-only entry point (#179, QA-B-103). The image passes split the image into
 * contiguous row bands; every band runs the same arithmetic on the same values
 * in the same order, so the OUTPUT IS IDENTICAL for any thread count -- the
 * setting changes only how long the call takes.
 *
 * @param threads 0 (default): automatic -- half the logical processors, at most
 *                4. The cap is an oversubscription guard: several XPE modules
 *                and a GUI run in one process, and past 4 threads the measured
 *                gain flattens (QA-B-103). 1 runs everything on the calling
 *                thread. Negative values are treated as 0.
 * @return XPE_OK always.
 *
 * Process-wide and not per handle; a caller that changes it while another
 * thread is inside a process call gets the old or the new value for that call.
 */
XPE_API XpeErrorCode xpe_enhance_basic_set_max_threads(int32_t threads);

/**
 * @brief The value set by xpe_enhance_basic_set_max_threads (0 = automatic).
 * @return The stored request, not the resolved count.
 */
XPE_API int32_t xpe_enhance_basic_get_max_threads(void);

/* ============================================================================
 * SWU-2.1: Log Transform (REQ-ENH-001..006)
 * ============================================================================ */

/**
 * @brief Apply logarithmic transform to a float32 image in-place.
 *
 * output[i] = normFactor * log10(input[i] + 1.0)
 * Negative pixels are clamped to 0 before log. (REQ-ENH-002)
 *
 * @param img        Float32 image buffer (modified in-place). NULL is rejected.
 * @param normFactor Normalization factor; must be positive. (REQ-ENH-003)
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if normFactor <= 0, img or
 *         img->data is NULL, or dataSize is inconsistent;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32.
 */
XPE_API XpeErrorCode xpe_log_transform(XpeImageBuffer* img, float normFactor);

/**
 * @brief Apply inverse logarithmic transform to a float32 image in-place.
 *
 * output[i] = pow(10.0, input[i] / normFactor) - 1.0
 *
 * @param img        Float32 image buffer (modified in-place). NULL is rejected.
 * @param normFactor Normalization factor; must be positive. (REQ-ENH-005)
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if normFactor <= 0, img or
 *         img->data is NULL, or dataSize is inconsistent;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32.
 */
XPE_API XpeErrorCode xpe_log_inverse(XpeImageBuffer* img, float normFactor);

/* ============================================================================
 * SWU-2.2: Noise Reduction (REQ-ENH-007..012)
 * ============================================================================ */

/**
 * @brief Apply noise reduction to a float32 image in-place.
 *
 * Supports bilateral filter (XPE_NOISE_BILATERAL) and Non-Local Means
 * (XPE_NOISE_NLM). (REQ-ENH-007, REQ-ENH-008)
 *
 * @param img    Float32 image buffer (modified in-place). A zero-sized image is
 *               rejected (#142).
 * @param params Noise reduction parameters. NULL returns XPE_ERR_INVALID_INPUT. (REQ-ENH-009)
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if params is NULL, the image
 *         is invalid, mode is neither BILATERAL nor NLM, or the mode's own
 *         parameters are out of range (bilateral: sigma_space/sigma_range <= 0, NaN or
 *         infinite -- QA-B-181d; sigma_space above 7.5 -- QA-B-210: 7.5 is the
 *         largest value whose 2-sigma extent fits the kernel radius cap of 15;
 *         a larger one would be cut off before 2 sigma, so it is refused;
 *         NLM: search_window/patch_size not odd-positive, h_param <= 0);
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32. (REQ-ENH-010)
 */
XPE_API XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img, const XpeNoiseReduceParams* params);

/**
 * @brief Estimate noise standard deviation via Median Absolute Deviation.
 *
 * sigma = 1.4826 * MAD(pixel_values) on a center ROI. (REQ-ENH-011)
 *
 * @param img      Float32 image buffer (read-only). NULL or zero-sized is
 *                 rejected.
 * @param outSigma Output: estimated noise sigma. NULL is rejected.
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if outSigma is NULL, the
 *         image is invalid, or width/height is 0;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32.
 */
XPE_API XpeErrorCode xpe_noise_estimate_sigma(const XpeImageBuffer* img, float* outSigma);

/* ============================================================================
 * SWU-2.3: Contrast Enhancement — CLAHE (REQ-ENH-013..017)
 * ============================================================================ */

/**
 * @brief Apply CLAHE contrast enhancement to a float32 image in-place.
 *
 * If params is NULL, defaults are used (clip_limit=3.0, tile_width=8,
 * tile_height=8). (REQ-ENH-014)
 *
 * This is the standard CLAHE (QA-B-207 M2): one intensity scale for the whole
 * image (4096 bins between its minimum and maximum); per tile, a histogram
 * clipped at clip_limit * tile_area / 4096 pixels per bin (at least 1) with the
 * excess spread evenly over the scale, turned into a cumulative table; and per
 * pixel, a bilinear blend of the tables of the up to four tiles whose centres
 * surround it, so there is no step at a tile border. The result is mapped back
 * onto the image's own [minimum, maximum]: equalization redistributes values
 * inside that range, it does not widen it, so an image that is already evenly
 * spread over its range comes out about as it went in. With a tile area below
 * 4096 / clip_limit pixels every clip_limit up to that bound clips alike (the
 * per-bin cap bottoms out at 1).
 *
 * Numeric boundary (QA-B-207b): the scale and the bin of a pixel are computed in
 * double, so any positive range is processed, down to a single denormal step
 * (1.4e-45) -- 4095 / 1.4e-45 = 2.9e48 is far below DBL_MAX. Only an image whose
 * minimum equals its maximum is "flat". (A float scale 4095.0f / range overflows
 * below a range of 4095 / FLT_MAX = 1.2034e-35, which is the bound this replaces.)
 * The other end is unchanged: a range that overflows float (max - min > FLT_MAX,
 * e.g. -3e38 and +3e38) is XPE_ERR_INVALID_INPUT (QA-B-181f).
 *
 * @param img    Float32 image buffer (modified in-place). A zero-sized image is
 *               rejected (#142); a flat image (no value range) is accepted and
 *               returns XPE_OK unchanged.
 * @param params CLAHE parameters, or NULL for defaults.
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if clip_limit < 1.0, NaN or
 *         infinite (QA-B-181d; a large finite value is accepted and never clips), either
 *         tile count < 2, the image is invalid, or the image is smaller than
 *         twice the tile grid (width < tile_width * 2 or height < tile_height * 2);
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32.
 */
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img, const XpeClaheParams* params);

/* ============================================================================
 * SWU-2.4: Edge Enhancement — USM (REQ-ENH-018..022)
 * ============================================================================ */

/**
 * @brief Apply Unsharp Masking edge enhancement to a float32 image in-place.
 *
 * output[i] = input[i] + amount * (input[i] - blur[i]) where |diff| >= threshold.
 * Overshoot is clamped per REQ-ENH-021 to max(original * 2, original + amount * threshold),
 * and no output pixel is below 0 (QA-B-201 M3/M3b): the undershoot beside a dark region is cut at 0.
 * A pixel that is not sharpened keeps its input value, except that a negative input is returned as 0.
 * If params is NULL, defaults are used (amount=0.5, radius=2.0, threshold=10.0). (REQ-ENH-019)
 *
 * @param img    Float32 image buffer (modified in-place). A zero-sized image is
 *               rejected (#142). amount == 0.0 builds no blur; a negative pixel
 *               is returned as 0 and a non-finite pixel is refused, as for every
 *               other amount (QA-B-205), and a finite image with no negative pixel
 *               is returned unchanged.
 * @param params USM parameters, or NULL for defaults.
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if amount is outside
 *         [0.0, 5.0] or NaN, radius outside [0.5, 10.0] or NaN, threshold < 0.0 or NaN
 *         (QA-B-181d; +infinity is a valid threshold), or the image
 *         is invalid or holds a non-finite pixel (for every amount, 0 included);
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32.
 *         (REQ-ENH-020)
 */
XPE_API XpeErrorCode xpe_edge_enhance(XpeImageBuffer* img, const XpeUsmParams* params);

/* ============================================================================
 * SWU-2.10: Exposure Index (REQ-ENH-023..030)
 * ============================================================================ */

/**
 * @brief Compute Exposure Index (EI) and Deviation Index (DI) per IEC 62494-1.
 *
 * EI = EIT * (mean_pixel_value / S0_reference)
 * DI = 10.0 * log10(EI / EIT)
 * Posts WARNING alert if |DI| > 3.0. (REQ-ENH-026)
 *
 * @param img   Float32 detector-domain image (read-only). NULL or zero-sized is
 *              rejected. (REQ-ENH-027, REQ-ENH-028)
 * @param meta  Image metadata with bodyPart for EIT lookup. NULL is rejected.
 *              An unknown or empty bodyPart falls back to the default EIT
 *              (200.0), it is not an error. (REQ-ENH-025)
 * @param outEI Output: computed Exposure Index. NULL is rejected.
 * @param outDI Output: computed Deviation Index. NULL is rejected.
 * @return XPE_OK on success; XPE_ERR_INVALID_INPUT if any pointer is NULL, the
 *         image is zero-sized, or the buffer is invalid;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32;
 *         XPE_ERR_PROCESSING_FAILED if the mean pixel value is <= 0 or the
 *         image holds a non-finite pixel (+inf, -inf, NaN: all three give the
 *         same answer, QA-B-206), in which case *outEI and *outDI are set to
 *         0.0. (REQ-ENH-030)
 */
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img,
                                              const XpeImageMetadata* meta,
                                              float* outEI,
                                              float* outDI);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ENHANCE_BASIC_API_H */
