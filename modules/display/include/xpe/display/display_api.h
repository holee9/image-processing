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
 * these checks apply uniformly and are not repeated per function below:
 * a NULL img or img->data is XPE_ERR_INVALID_INPUT, a zero width or height is
 * XPE_ERR_INVALID_INPUT (#142 -- an empty image is an error, not a no-op
 * success), a non-FLOAT32 format is XPE_ERR_UNSUPPORTED_FORMAT, and a dataSize
 * inconsistent with the declared dimensions (#123) is XPE_ERR_INVALID_INPUT.
 * Note the ordering: dimensions are judged before the format, so an empty
 * UINT16 buffer reports INVALID_INPUT rather than UNSUPPORTED_FORMAT, while a
 * correctly sized UINT16 buffer reports UNSUPPORTED_FORMAT even when its
 * dataSize is also wrong.
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
 * Used by xpe_voi_preset_create() to populate XpeVoiLutParams with a
 * window in the detector DN domain (REQ-DISP-017, revised 2026-09-17).
 *
 * Until per-body-part DN windows are derived from real detector data (#151),
 * every body part yields the same full 16-bit window, center=32768 and
 * width=65535: selecting a body part does not change the output (#177).
 */
typedef enum XpeBodyPart {
    XPE_BODY_BONE    = 0, /**< Bone: center=32768, width=65535 (provisional, #151) */
    XPE_BODY_LUNG    = 1, /**< Lung: center=32768, width=65535 (provisional, #151) */
    XPE_BODY_ABDOMEN = 2, /**< Abdomen: center=32768, width=65535 (provisional, #151) */
    XPE_BODY_HEAD    = 3  /**< Head: center=32768, width=65535 (provisional, #151) */
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

/**
 * @brief Display polarity of the Presentation LUT stage (QA-B-214, user decision on #251).
 *
 * The pipeline carries "a larger value is brighter" (MONOCHROME2 sense) from the reader to the writer and no earlier stage
 * inverts. A detector's linear signal rises with dose, so the unexposed background (air) is the HIGH end of the data and,
 * shown as is, is white while bone is dark -- the reverse of a reading display. The polarity is therefore decided here, at
 * the last display stage: INVERTED reads the table backwards (output = lutData[1023 - index]), so the high end of the data
 * is shown dark. Reading the table backwards is the DICOM "INVERSE" shape of PS3.3 C.11.6 applied to the P-Values; for a
 * GSDF table it keeps the perceptual linearity, which "65535 - value" would not.
 * Pixel data written to DICOM afterwards is MONOCHROME2 with Presentation LUT Shape IDENTITY and already carries this
 * inversion; the meaning of the values of every earlier stage is unchanged.
 */
typedef enum XpePresentationPolarity {
    XPE_PRESENTATION_INVERTED = 0, /**< output = lutData[1023 - index]: high input shown dark (the default) */
    XPE_PRESENTATION_AS_IS    = 1  /**< output = lutData[index]: high input shown bright (behaviour before QA-B-214) */
} XpePresentationPolarity;

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
 *               width must be > 0.0f for LINEAR_EXACT and SIGMOID and >= 1.0f
 *               for LINEAR (PS3.3 C.11.2.1.2: "Window Width shall always be
 *               greater than or equal to 1"; C.11.2.1.3: "greater than 0" for the
 *               others). minOut must be below maxOut. All parameters must be finite.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if img or params is NULL, if img->dataSize is
 *         inconsistent with its dimensions, if width <= 0.0f, if mode is LINEAR
 *         and width < 1.0f, if minOut >= maxOut (QA-B-207 D3, D8), or if params->mode
 *         is not one of LINEAR / LINEAR_EXACT / SIGMOID. The image is untouched.
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
 * @brief Populate XpeVoiLutParams with a body-part preset in the detector DN domain.
 *
 * The window is expressed in raw detector DN, as VOI receives it (modality
 * LUT identity: slope 1, intercept 0). Until per-body-part DN values are
 * derived from real detector data (#151), all four body parts return the
 * same full 16-bit window, center=32768 and width=65535 (REQ-DISP-017,
 * #177). These are not CT HU windows.
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

/**
 * @brief Choose a VOI window from the anatomy in the image itself (QA-B-214, user decision on #251).
 *
 * The window is derived from the values that actually reach the VOI stage, whatever their domain (the log domain after
 * xpe_log_transform, detector DN, ...), so it needs no per-body-part table:
 *   1. every kAutoWindowSampleStride-th pixel of every kAutoWindowSampleStride-th row is histogrammed (1024 bins between
 *      the image's minimum and maximum);
 *   2. an Otsu split of that histogram separates two classes: the HIGH class is the background (unattenuated beam, air) and
 *      the low class is the anatomy;
 *   3. window low  = the 0.5 % quantile of the anatomy class (the densest bone is not clipped away),
 *      window high = the 5 % quantile of the background class (so the tissue-to-air transition at the skin line stays
 *      inside the window instead of being cut at the Otsu threshold).
 * The result is mode XPE_VOI_LINEAR_EXACT, center = (low + high) / 2, width = high - low, minOut 0, maxOut 1 -- the output
 * range xpe_apply_presentation_lut expects. The numeric constants, their values and the measurements behind them are the
 * named constants in voi_auto_window.cpp.
 *
 * Fallback: when the histogram does not show two classes -- either class holds under 2 % of the sampled pixels or the Otsu
 * separability (between-class variance / total variance) is under 0.75 -- there is no anatomy to isolate (a flat-field
 * frame, an image of one tissue) and the window is the 1 % .. 99 % quantile range of the whole image, with one Info alert
 * posted. A flat image (maximum == minimum) gets center = that value and width 1.0, also with the Info alert.
 *
 * Contract: the background is the HIGH end of the data. Data whose background is the low end (MONOCHROME1 normalised to
 * MONOCHROME2 sense) has it the wrong way round and the window would isolate the wrong class. Image content outside the
 * anatomy and the beam (a collimator shadow, which is darker than the anatomy) is counted into the anatomy class.
 *
 * @param img       Float32 image (read-only). NULL, empty, wrong format or inconsistent dataSize are rejected as for the other
 *                  display functions; a non-finite pixel is XPE_ERR_INVALID_INPUT.
 * @param outParams [out] Receives the window. Untouched on any error. Must not be NULL.
 * @return XPE_OK, XPE_ERR_INVALID_INPUT (NULL img/outParams/data, empty image, inconsistent dataSize, non-finite pixel) or
 *         XPE_ERR_UNSUPPORTED_FORMAT (img is not float32).
 *
 * @note Performance: one pass over every pixel for the finite check and extremes plus a 1/16 sample for the histogram;
 *       measured in the QA-B-214 report (3072x3072).
 * @note Thread-safe when called with independent buffers.
 */
XPE_API XpeErrorCode xpe_voi_auto_window(const XpeImageBuffer* img,
                                           XpeVoiLutParams*      outParams);

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
 * @note POLARITY (QA-B-214): this function shows the high end of the data DARK -- it is
 *       xpe_apply_presentation_lut_ex(img, params, XPE_PRESENTATION_INVERTED). Before QA-B-214 it was the ascending mapping
 *       output = lutData[index]; that mapping is xpe_apply_presentation_lut_ex(.., XPE_PRESENTATION_AS_IS). A caller that
 *       already inverts (a viewer's own "invert" switch) or whose data already has the background at the LOW end
 *       (MONOCHROME1 data normalised by xpe_dicom_read_image) must not invert a second time.
 * @note Performance target: <= 25 ms for 3072x3072 image (REQ-DISP-025).
 * @note Thread-safe when called with independent buffers (REQ-DISP-033).
 */
XPE_API XpeErrorCode xpe_apply_presentation_lut(XpeImageBuffer*                  img,
                                                  const XpePresentationLutParams*  params);

/**
 * @brief xpe_apply_presentation_lut with the polarity chosen by the caller (QA-B-214).
 *
 * Identical to xpe_apply_presentation_lut in every other respect: float32 in, uint16 out, input clamped to [0, 1], index =
 * round(input * 1023), non-finite pixels refused, the image untouched on any error.
 *
 * @param img      [in/out] Float32 image; converted to uint16 in place on success (as xpe_apply_presentation_lut).
 * @param params   [in]     Presentation LUT parameters. Must not be NULL.
 * @param polarity XPE_PRESENTATION_INVERTED or XPE_PRESENTATION_AS_IS. Any other value is XPE_ERR_INVALID_INPUT and the
 *                 image is untouched.
 * @return As xpe_apply_presentation_lut, plus XPE_ERR_INVALID_INPUT for an unknown polarity.
 */
XPE_API XpeErrorCode xpe_apply_presentation_lut_ex(XpeImageBuffer*                  img,
                                                     const XpePresentationLutParams*  params,
                                                     XpePresentationPolarity          polarity);

/**
 * @brief Compute a DICOM GSDF-compliant Presentation LUT from luminance measurements.
 *
 * @anchor xpe_gsdf_calibrate
 * @MX:ANCHOR: [AUTO] Public API boundary — P/Invoke entry point from C# host
 * @MX:REASON: All callers (xpe_display.dll consumers) depend on this ABI contract
 * @MX:SPEC: SPEC-XPE-P1B-DISP
 * @MX:NOTE: [AUTO] DICOM PS3.14 GSDF — Equation 7-2 (j from L) and 7-1 (L from j)
 * @MX:WARN: [AUTO] Numerical precision sensitive — validate with DICOM PS3.14 test vectors
 * @MX:REASON: Coefficients are transcribed from the standard and checked against Table B-1
 *
 * Applies the DICOM PS3.14 Grayscale Standard Display Function: 1024 P-Values
 * spaced equally in JND index across the measured luminance range, each mapped
 * to the driving level whose MEASURED luminance meets the standard's
 * requirement. Produces a monotonically non-decreasing 1024-entry uint16 LUT.
 * Sets outParams->gsdfEnabled = 1 on success.
 *
 * @param luminanceValues [in]  The display's measured characteristic curve
 *                              (cd/m^2), count >= 2. Element i is the luminance
 *                              measured at the EQUALLY SPACED driving level
 *                              DDL_i = i/(count-1) * 65535, and the values must
 *                              ASCEND. The whole array is used: the interior
 *                              samples are what the standard's required
 *                              luminances are inverted against.
 *                              (#155, QA-B-145 — this narrows an earlier
 *                              contract under which only the minimum and
 *                              maximum were read.)
 * @param count           [in]  Number of entries in luminanceValues (must be >= 2).
 * @param outParams       [out] Populated with GSDF LUT; gsdfEnabled set to 1.
 *                              Requires xpe_apply_presentation_lut to be called
 *                              separately -- this function computes the LUT only.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if luminanceValues or outParams is NULL,
 *         count < 2, any entry is NaN or infinite (QA-B-181d), or the array is not
 *         non-decreasing (REQ-DISP-029; equal neighbours are allowed, a fall is
 *         not), or the first entry -- the black level -- is below 0.05 cd/m^2
 *         (QA-B-207 D1: the lower end of the range PS3.14 specifies the grayscale
 *         standard display function on; zero and negative included; 0.05 itself is
 *         accepted). outParams is untouched.
 *
 * @note Only half of the REQ-DISP-029 contract is checked. The ordering is; the
 *       "measured at equally spaced driving levels" half is NOT, because no
 *       driving level reaches this function. An ascending but log-spaced ladder
 *       is accepted and yields an incorrect LUT.
 *
 * @note A maximum not above the minimum is still silently coerced to minimum + 1.0
 *       (the black level, by contrast, is no longer coerced: below 0.05 it is
 *       refused, QA-B-207 D1). The upper end of the standard's range, 4000 cd/m^2,
 *       is not enforced: a curve above it is accepted and its LUT follows the
 *       standard's polynomial outside the range it is specified on.
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
