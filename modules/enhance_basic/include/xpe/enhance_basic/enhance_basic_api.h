#ifndef XPE_ENHANCE_BASIC_API_H
#define XPE_ENHANCE_BASIC_API_H

/**
 * @file enhance_basic_api.h
 * @brief XPE Basic Enhancement Module API (SPEC-XPE-P1B-ENH)
 *
 * Provides 7 exported C API functions for log transform, noise reduction,
 * contrast enhancement (CLAHE), edge enhancement (USM), and exposure index
 * computation per IEC 62494-1.
 *
 * All functions operate in-place on float32 images (XPE_PIXEL_FLOAT32).
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
    float              sigma_space;    /**< Bilateral: spatial sigma (default 3.0) */
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

/** Returns the xpe_enhance_basic module version string. */
XPE_API const char* xpe_enhance_basic_version(void);

/* ============================================================================
 * SWU-2.1: Log Transform (REQ-ENH-001..006)
 * ============================================================================ */

/**
 * @brief Apply logarithmic transform to a float32 image in-place.
 *
 * output[i] = normFactor * log10(input[i] + 1.0)
 * Negative pixels are clamped to 0 before log. (REQ-ENH-002)
 *
 * @param img        Float32 image buffer (modified in-place).
 * @param normFactor Normalization factor; must be positive. (REQ-ENH-003)
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT if normFactor <= 0 or img invalid.
 */
XPE_API XpeErrorCode xpe_log_transform(XpeImageBuffer* img, float normFactor);

/**
 * @brief Apply inverse logarithmic transform to a float32 image in-place.
 *
 * output[i] = pow(10.0, input[i] / normFactor) - 1.0
 *
 * @param img        Float32 image buffer (modified in-place).
 * @param normFactor Normalization factor; must be positive. (REQ-ENH-005)
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT if normFactor <= 0 or img invalid.
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
 * @param img    Float32 image buffer (modified in-place).
 * @param params Noise reduction parameters. NULL returns XPE_ERR_INVALID_INPUT. (REQ-ENH-009)
 * @return XPE_OK on success.
 */
XPE_API XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img, const XpeNoiseReduceParams* params);

/**
 * @brief Estimate noise standard deviation via Median Absolute Deviation.
 *
 * sigma = 1.4826 * MAD(pixel_values) on a center ROI. (REQ-ENH-011)
 *
 * @param img      Float32 image buffer (read-only).
 * @param outSigma Output: estimated noise sigma.
 * @return XPE_OK on success.
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
 * @param img    Float32 image buffer (modified in-place).
 * @param params CLAHE parameters, or NULL for defaults.
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT if clip_limit < 1.0 or tiles < 2.
 */
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img, const XpeClaheParams* params);

/* ============================================================================
 * SWU-2.4: Edge Enhancement — USM (REQ-ENH-018..022)
 * ============================================================================ */

/**
 * @brief Apply Unsharp Masking edge enhancement to a float32 image in-place.
 *
 * output[i] = input[i] + amount * (input[i] - blur[i]) where |diff| >= threshold.
 * Overshoot is clamped per REQ-ENH-021.
 * If params is NULL, defaults are used (amount=0.5, radius=2.0, threshold=10.0). (REQ-ENH-019)
 *
 * @param img    Float32 image buffer (modified in-place).
 * @param params USM parameters, or NULL for defaults.
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT if params out of range. (REQ-ENH-020)
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
 * @param img   Float32 detector-domain image (read-only). (REQ-ENH-027, REQ-ENH-028)
 * @param meta  Image metadata with bodyPart for EIT lookup. (REQ-ENH-025)
 * @param outEI Output: computed Exposure Index.
 * @param outDI Output: computed Deviation Index.
 * @return XPE_OK on success, XPE_ERR_PROCESSING_FAILED if mean <= 0. (REQ-ENH-030)
 */
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img,
                                              const XpeImageMetadata* meta,
                                              float* outEI,
                                              float* outDI);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ENHANCE_BASIC_API_H */
