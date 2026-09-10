/**
 * @file display_api.h
 * @brief XPE Phase 1b Display DLL public C API (6 exported functions)
 *
 * All functions use C linkage (__cdecl) and blittable types for P/Invoke compatibility.
 * SPEC: SPEC-XPE-P1B-DISP v1.0.0
 * IEC 62304 Class B
 *
 * Processing pipeline (in order):
 *   xpe_apply_modality_lut  ->  xpe_apply_voi_lut  ->  xpe_apply_presentation_lut
 *
 * Input format requirement: all processing functions require XPE_PIXEL_FLOAT32.
 * Domain transition: xpe_apply_presentation_lut converts float32 -> uint16.
 *
 * Every processing function routes its image through one shared validator, so
 * three checks apply uniformly and are not repeated per function below:
 * a NULL img is XPE_ERR_INVALID_INPUT, a non-FLOAT32 format is
 * XPE_ERR_UNSUPPORTED_FORMAT, and a dataSize inconsistent with the declared
 * dimensions (#123) is XPE_ERR_INVALID_INPUT. Note the ordering: format is
 * judged before dataSize, so a UINT16 buffer reports UNSUPPORTED_FORMAT even
 * when its dataSize is also wrong.
 */

#ifndef XPE_DISPLAY_API_H
#define XPE_DISPLAY_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * SWU-3.1: Modality LUT Types
 * REQ-DISP-001 to REQ-DISP-008
 * ========================================================================= */

/**
 * @brief Selects the Modality LUT mapping mode.
 *
 * XPE_MODALITY_LUT_LINEAR applies a linear rescale formula.
 * XPE_MODALITY_LUT_TABLE  applies a lookup table mapping.
 */
typedef enum XpeModalityLutMode {
    XPE_MODALITY_LUT_LINEAR = 0, /**< output[i] = input[i] * slope + intercept */
    XPE_MODALITY_LUT_TABLE  = 1  /**< output[i] = lutData[clamp(round(input[i]) - firstMapped, 0, len-1)] */
} XpeModalityLutMode;

/**
 * @brief Parameters for the Modality LUT transformation (SWU-3.1).
 *
 * When mode == XPE_MODALITY_LUT_LINEAR:
 *   - rescaleSlope and rescaleIntercept are used; lutData/lutLength/lutFirstMapped/lutBitsStored ignored.
 *   - rescaleSlope must not be 0.0f.
 *
 * When mode == XPE_MODALITY_LUT_TABLE:
 *   - lutData, lutLength, lutFirstMapped, lutBitsStored are used.
 *   - lutData must not be NULL; lutLength must be > 0.
 *   - Input pixel value is rounded, shifted by lutFirstMapped, and clamped to [0, lutLength-1].
 */
typedef struct XpeModalityLutParams {
    XpeModalityLutMode mode;          /**< Mapping mode selector */
    float              rescaleSlope;  /**< LINEAR: multiplier (must != 0.0f) */
    float              rescaleIntercept; /**< LINEAR: additive offset */
    const uint16_t*    lutData;       /**< TABLE: pointer to LUT entries (owned by caller) */
    uint32_t           lutLength;     /**< TABLE: number of LUT entries (must > 0) */
    int32_t            lutFirstMapped; /**< TABLE: input value that maps to index 0 */
    uint32_t           lutBitsStored; /**< TABLE: bit depth of LUT output values */
} XpeModalityLutParams;

/* =========================================================================
 * SWU-3.2: VOI LUT Types
 * REQ-DISP-009 to REQ-DISP-018
 * ========================================================================= */

/**
 * @brief Selects the VOI LUT windowing algorithm (DICOM PS3.3 C.11.2.1).
 */
typedef enum XpeVoiLutMode {
    XPE_VOI_LINEAR       = 0, /**< Standard linear windowing with half-value offset */
    XPE_VOI_LINEAR_EXACT = 1, /**< DICOM PS3.3 C.11.2.1.3 exact linear mapping */
    XPE_VOI_SIGMOID      = 2  /**< Sigmoid / S-curve windowing */
} XpeVoiLutMode;

/**
 * @brief Body part presets for VOI LUT parameters.
 *
 * Used by xpe_voi_preset_create() to populate XpeVoiLutParams with
 * clinically validated window center/width values.
 */
typedef enum XpeBodyPart {
    XPE_BODY_BONE    = 0, /**< Bone: center=500,  width=2000 */
    XPE_BODY_LUNG    = 1, /**< Lung: center=-600, width=1600 */
    XPE_BODY_ABDOMEN = 2, /**< Abdomen: center=40, width=400 */
    XPE_BODY_HEAD    = 3  /**< Head: center=40,   width=80   */
} XpeBodyPart;

/**
 * @brief Parameters for the VOI LUT windowing transformation (SWU-3.2).
 *
 * width must be > 0.0f. Output pixel values are clamped to [minOut, maxOut].
 *
 * LINEAR formula:
 *   output[i] = clamp((input[i] - (center - width/2)) / width * (maxOut - minOut) + minOut,
 *                     minOut, maxOut)
 *
 * LINEAR_EXACT formula (DICOM PS3.3 C.11.2.1.3):
 *   output[i] = clamp(((input[i] - center) / width + 0.5f) * (maxOut - minOut) + minOut,
 *                     minOut, maxOut)
 *
 * SIGMOID formula:
 *   output[i] = (maxOut - minOut) / (1 + exp(-4 * (input[i] - center) / width)) + minOut
 */
typedef struct XpeVoiLutParams {
    XpeVoiLutMode mode;   /**< Windowing algorithm selector */
    float         center; /**< Window center (Hounsfield units or raw pixel value) */
    float         width;  /**< Window width (must be > 0.0f) */
    float         minOut; /**< Minimum output pixel value */
    float         maxOut; /**< Maximum output pixel value */
} XpeVoiLutParams;

/* =========================================================================
 * SWU-3.3: Presentation LUT Types
 * REQ-DISP-019 to REQ-DISP-028
 * ========================================================================= */

/**
 * @brief Parameters for the Presentation LUT and GSDF calibration (SWU-3.3).
 *
 * lutData: 1024-entry lookup table mapping [0.0, 1.0] float input to uint16 output.
 *   - Index = clamp(round(input[i] * 1023), 0, 1023)
 *   - output[i] = lutData[index]
 *
 * gsdfEnabled: non-zero if the LUT was generated by xpe_gsdf_calibrate().
 *
 * @note xpe_apply_presentation_lut performs a domain transition:
 *       float32 image -> uint16 image (allocates new buffer, frees old).
 */
typedef struct XpePresentationLutParams {
    uint16_t lutData[1024]; /**< 1024-entry presentation LUT (uint16 output values) */
    int32_t  gsdfEnabled;   /**< Non-zero if LUT is GSDF-calibrated */
} XpePresentationLutParams;

/* =========================================================================
 * Version
 * ========================================================================= */

/**
 * @brief Returns the xpe_display module version string (e.g. "1.0.0").
 * @return Null-terminated version string. Lifetime: process. Never NULL.
 */
XPE_API const char* xpe_display_version(void);

/* =========================================================================
 * SWU-3.1: Modality LUT API
 * REQ-DISP-001 to REQ-DISP-008
 * ========================================================================= */

/**
 * @brief Apply Modality LUT (rescale or table lookup) to a float32 image in-place.
 *
 * @anchor xpe_apply_modality_lut
 * @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
 * @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
 * @MX:SPEC: SPEC-XPE-P1B-DISP
 *
 * @param img    [in/out] Float32 image to transform. Must not be NULL.
 *               format must be XPE_PIXEL_FLOAT32.
 * @param params [in]     Modality LUT parameters. Must not be NULL.
 *               For TABLE mode: lutData must not be NULL, lutLength must be > 0.
 *               For LINEAR mode: rescaleSlope must not be 0.0f.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if img or params is NULL; if img->dataSize is
 *         inconsistent with its dimensions; if TABLE mode validation fails
 *         (lutData NULL or lutLength == 0); if LINEAR mode rescaleSlope == 0.0f;
 *         or if params->mode is neither LINEAR nor TABLE.
 * @return XPE_ERR_UNSUPPORTED_FORMAT if img->format != XPE_PIXEL_FLOAT32.
 *
 * @note Mode-specific validation happens AFTER the image is accepted, so an
 *       invalid slope or LUT is reported even for a zero-pixel image.
 *
 * @note Performance target: <= 20 ms for 3072x3072 image (REQ-DISP-008).
 * @note Thread-safe when called with independent buffers (REQ-DISP-033).
 */
XPE_API XpeErrorCode xpe_apply_modality_lut(XpeImageBuffer*            img,
                                              const XpeModalityLutParams* params);

/* =========================================================================
 * SWU-3.2: VOI LUT API
 * REQ-DISP-009 to REQ-DISP-018
 * ========================================================================= */

/**
 * @brief Apply VOI LUT windowing to a float32 image in-place.
 *
 * @anchor xpe_apply_voi_lut
 * @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
 * @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
 * @MX:SPEC: SPEC-XPE-P1B-DISP
 *
 * @param img    [in/out] Float32 image to window. Must not be NULL.
 * @param params [in]     VOI LUT parameters. Must not be NULL.
 *               width must be > 0.0f.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if img or params is NULL, if img->dataSize is
 *         inconsistent with its dimensions, if width <= 0.0f, or if params->mode
 *         is not one of LINEAR / LINEAR_EXACT / SIGMOID.
 * @return XPE_ERR_UNSUPPORTED_FORMAT if img->format != XPE_PIXEL_FLOAT32.
 *
 * @note An unknown mode is detected inside the per-pixel dispatch, so pixels are
 *       not modified before the error is returned.
 *
 * @note Performance target: <= 16 ms for 3072x3072 image (REQ-DISP-016).
 * @note Thread-safe when called with independent buffers (REQ-DISP-033).
 */
XPE_API XpeErrorCode xpe_apply_voi_lut(XpeImageBuffer*          img,
                                         const XpeVoiLutParams*   params);

/**
 * @brief Populate XpeVoiLutParams with clinically validated preset values.
 *
 * Every field of @p params is overwritten, not just center and width: mode is
 * set to XPE_VOI_LINEAR, minOut to 0.0f and maxOut to 255.0f for all four
 * presets. A caller that set mode or an output range before calling loses it.
 *
 * @param params   [out] Params struct to populate. Must not be NULL. Left
 *                       untouched when bodyPart is invalid.
 * @param bodyPart [in]  Body part selector (XPE_BODY_BONE, XPE_BODY_LUNG, etc.)
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if params is NULL or bodyPart is not a valid enum value.
 *
 * @note Does not modify img; only fills the params struct.
 */
XPE_API XpeErrorCode xpe_voi_preset_create(XpeVoiLutParams* params,
                                             XpeBodyPart      bodyPart);

/* =========================================================================
 * SWU-3.3: Presentation LUT + GSDF API
 * REQ-DISP-019 to REQ-DISP-028
 * ========================================================================= */

/**
 * @brief Apply Presentation LUT to a float32 image, producing a uint16 image.
 *
 * @anchor xpe_apply_presentation_lut
 * @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
 * @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract; domain transition float32->uint16
 * @MX:SPEC: SPEC-XPE-P1B-DISP
 *
 * Domain transition: allocates a new uint16 buffer, maps float32 pixels through
 * the 1024-entry LUT, frees the old float32 buffer, and updates img->format,
 * bitsAllocated, bitsStored, and dataSize.
 *
 * The old buffer is released with the same allocator xpe_alloc_image() used, so
 * the converted image stays valid input for xpe_free_image(). Input pixels are
 * clamped to [0.0, 1.0] before indexing, so out-of-range values saturate rather
 * than being rejected.
 *
 * @param img    [in/out] Float32 image. On success, converted to uint16 in-place.
 *                        On ANY error the image is left exactly as it was --
 *                        allocation happens before the old buffer is freed.
 * @param params [in]     Presentation LUT parameters. Must not be NULL. Its
 *                        lutData is a fixed 1024-entry array, so there is no
 *                        length to validate and no LUT-shape error to report.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if img or params is NULL, or img->dataSize is
 *         inconsistent with its dimensions.
 * @return XPE_ERR_UNSUPPORTED_FORMAT if img->format != XPE_PIXEL_FLOAT32.
 * @return XPE_ERR_OUT_OF_MEMORY if uint16 buffer allocation fails.
 *
 * @note Performance target: <= 25 ms for 3072x3072 image (REQ-DISP-025).
 * @note Thread-safe when called with independent buffers (REQ-DISP-033).
 */
XPE_API XpeErrorCode xpe_apply_presentation_lut(XpeImageBuffer*                  img,
                                                  const XpePresentationLutParams*  params);

/**
 * @brief Compute a DICOM GSDF-compliant Presentation LUT from luminance measurements.
 *
 * @anchor xpe_gsdf_calibrate
 * @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
 * @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
 * @MX:SPEC: SPEC-XPE-P1B-DISP
 * @MX:NOTE: [AUTO] GSDF Barten model approximation — simplified log-linear JND model
 * @MX:WARN: [AUTO] Numerical precision sensitive — validate with DICOM PS3.14 test vectors
 * @MX:REASON: Barten model uses empirical constants; different calibration data may require tuning
 *
 * Uses a simplified Barten model approximation to compute JND indices from
 * luminance values. Produces a monotonically non-decreasing 1024-entry uint16 LUT.
 * Sets outParams->gsdfEnabled = 1 on success.
 *
 * @param luminanceValues [in]  Array of measured luminance values (cd/m^2), count >= 2.
 *                              Only the minimum and maximum of the array are
 *                              used; intermediate measurements do not affect
 *                              the resulting LUT.
 * @param count           [in]  Number of entries in luminanceValues (must be >= 2).
 * @param outParams       [out] Populated with GSDF LUT; gsdfEnabled set to 1.
 *                              Requires xpe_apply_presentation_lut to be called
 *                              separately -- this function computes the LUT only.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if luminanceValues or outParams is NULL, or count < 2.
 *
 * @note Degenerate luminance input is silently coerced, not rejected: a
 *       non-positive minimum becomes 0.01 cd/m^2, and a maximum not above the
 *       minimum becomes minimum + 1.0. A caller passing measurement garbage
 *       therefore receives XPE_OK and a plausible-looking LUT.
 *
 * @note count >= 2 is required to define a luminance range (REQ-DISP-027).
 */
XPE_API XpeErrorCode xpe_gsdf_calibrate(const float*             luminanceValues,
                                          uint32_t                 count,
                                          XpePresentationLutParams* outParams);

#ifdef __cplusplus
}
#endif

#endif /* XPE_DISPLAY_API_H */
