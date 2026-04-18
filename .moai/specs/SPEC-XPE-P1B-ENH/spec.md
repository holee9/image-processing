# SPEC-XPE-P1B-ENH: Phase 1b Basic Enhancement + EI Baseline

**Document ID**: SPEC-XPE-P1B-ENH
**Version**: 1.0.0
**Date**: 2026-04-16
**Status**: Implemented
**Parent**: SPEC-XPE-MASTER v2.0.0
**Classification**: IEC 62304 Class B
**Sprint**: S1-B (xpe_enhance_basic.dll)
**EARS Requirement Count**: 30
**SWU Count**: 5
**API Count**: 7

---

## HISTORY

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI (manager-spec) | Initial EARS requirements from SPEC-XPE-MASTER v2.0.0 and ALG-SPEC-001 v3.0.0-ds2 |
| 1.1.0 | 2026-04-16 | MoAI (sync) | Implementation complete — 67/67 tests passing, all 5 SWUs delivered |

---

## 1. Scope

**Purpose**: Implement the xpe_enhance_basic.dll module providing 5 Software Units (SWU-2.1, SWU-2.2, SWU-2.3, SWU-2.4, SWU-2.10) with 7 exported C API functions covering log transform, noise reduction, contrast enhancement, edge enhancement, and exposure index baseline computation.

**Pipeline Position**: Receives float32 images from xpe_preprocess.dll (post-gain-correct). Output feeds into xpe_display.dll (Modality/VOI LUT) and provides EI/DI metrics to the C# orchestrator.

### 1.1 In Scope

- SWU-2.1: Log Transform and Inverse (POST-01)
- SWU-2.2: Noise Reduction -- bilateral filter and NLM (POST-02 basic)
- SWU-2.3: Contrast Enhancement -- CLAHE (POST-03 basic)
- SWU-2.4: Edge Enhancement -- Unsharp Masking (POST-04)
- SWU-2.10: Exposure Index / Deviation Index baseline (SUP-03, IEC 62494-1)
- Parameter structs: XpeNoiseReduceParams, XpeClaheParams, XpeUsmParams
- Google Test suite for all 7 API functions
- CMake target: xpe_enhance_basic (SHARED library)

### 1.2 Out of Scope (Exclusions -- What NOT to Build)

- Multiscale frequency processing (SWU-2.5, Phase 2)
- Fractional processing (SWU-2.6, Phase 2)
- Collimation detection and ROI-aware EI refinement (SWU-2.8 + SWU-2.10 Phase 2 reuse)
- DL-based denoising (SWU-2.12, Phase 3)
- Body part recognition for EIT auto-selection (SWU-2.7, Phase 3)
- GPU/SIMD acceleration (future optimization, not Phase 1b)
- Wavelet-based noise reduction modes
- Custom CLAHE interpolation schemes beyond bilinear tile blending

### 1.3 Dependencies

| Dependency | Type | Status |
|-----------|------|--------|
| xpe_common.dll (xpe_types.h, xpe_error.h) | Build-time + Runtime | Complete (SPEC-XPE-P0) |
| xpe_preprocess.dll | Runtime (pipeline predecessor) | Complete (SPEC-XPE-P1A) |
| SPEC-XPE-MASTER v2.0.0 | Specification | Approved |
| ALG-SPEC-001 v3.0.0-ds2 | Algorithm Reference | Approved |
| IEC 62494-1 | Normative Standard (EI/DI) | External |

---

## 2. Architecture

### 2.1 Module Structure

```
modules/enhance_basic/
  include/xpe/enhance_basic/
    enhance_basic_api.h        -- 7 function declarations + 3 param structs
    enhance_basic_internal.h   -- internal helpers (not exported)
  src/
    log_transform.cpp          -- SWU-2.1
    noise_reduce.cpp           -- SWU-2.2
    contrast_enhance.cpp       -- SWU-2.3
    edge_enhance.cpp           -- SWU-2.4
    exposure_index.cpp         -- SWU-2.10
  tests/
    test_log_transform.cpp
    test_noise_reduce.cpp
    test_contrast_enhance.cpp
    test_edge_enhance.cpp
    test_exposure_index.cpp
    test_enhance_integration.cpp
  CMakeLists.txt
```

### 2.2 DLL Exports

7 extern "C" functions with `XPE_API` macro, `__cdecl` calling convention, blittable types for P/Invoke:

```c
XPE_API XpeErrorCode xpe_log_transform(XpeImageBuffer* img, float normFactor);
XPE_API XpeErrorCode xpe_log_inverse(XpeImageBuffer* img, float normFactor);
XPE_API XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img, const XpeNoiseReduceParams* params);
XPE_API XpeErrorCode xpe_noise_estimate_sigma(const XpeImageBuffer* img, float* outSigma);
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img, const XpeClaheParams* params);
XPE_API XpeErrorCode xpe_edge_enhance(XpeImageBuffer* img, const XpeUsmParams* params);
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img, const XpeImageMetadata* meta, float* outEI, float* outDI);
```

### 2.3 Parameter Structs

```c
typedef enum XpeNoiseReduceMode {
    XPE_NOISE_BILATERAL = 0,
    XPE_NOISE_NLM       = 1
} XpeNoiseReduceMode;

typedef struct XpeNoiseReduceParams {
    XpeNoiseReduceMode mode;          /* bilateral or NLM */
    float              sigma_space;   /* bilateral: spatial sigma (default 3.0) */
    float              sigma_range;   /* bilateral: range sigma (default 50.0) */
    int32_t            search_window; /* NLM: search window size (default 21) */
    int32_t            patch_size;    /* NLM: patch size (default 7) */
    float              h_param;       /* NLM: filtering strength (default 10.0) */
} XpeNoiseReduceParams;

typedef struct XpeClaheParams {
    float   clip_limit;   /* contrast clip limit (default 3.0) */
    int32_t tile_width;   /* tile grid width (default 8) */
    int32_t tile_height;  /* tile grid height (default 8) */
} XpeClaheParams;

typedef struct XpeUsmParams {
    float amount;     /* sharpening amount (default 0.5) */
    float radius;     /* blur radius in pixels (default 2.0) */
    float threshold;  /* minimum edge magnitude to sharpen (default 10.0) */
} XpeUsmParams;
```

### 2.4 CMake Target

```cmake
add_library(xpe_enhance_basic SHARED
    src/log_transform.cpp
    src/noise_reduce.cpp
    src/contrast_enhance.cpp
    src/edge_enhance.cpp
    src/exposure_index.cpp
)
target_link_libraries(xpe_enhance_basic PUBLIC xpe_common)
target_compile_definitions(xpe_enhance_basic PRIVATE XPE_DLL_EXPORT)
```

---

## 3. Software Units

### 3.1 SWU-2.1: LogTransform (POST-01)

**Purpose**: Convert float32 detector-domain pixel values to logarithmic scale for perceptual uniformity. Provide inverse transform for EI computation on detector-domain data.

**Algorithm**:
- Forward: `output[i] = normFactor * log10(input[i] + 1.0)`
- Inverse: `output[i] = pow(10.0, input[i] / normFactor) - 1.0`

**Input**: float32 image (post-gain-correct from xpe_preprocess.dll)
**Output**: float32 image (in-place modification)

**Constraints**:
- Must handle zero pixels: `log10(0 + 1) = 0`
- Must handle negative pixels (clamp to 0 before log)
- normFactor must be positive; zero/negative returns XPE_ERR_INVALID_INPUT
- Performance: <= 15ms for 3072x3072 float32

### 3.2 SWU-2.2: NoiseReducer (POST-02 basic)

**Purpose**: Reduce quantum noise and electronic noise while preserving diagnostically relevant edges.

**Algorithm**:
- Mode 1 -- Bilateral filter: weighted spatial-range Gaussian filter
  - `sigma_space` (default 3.0): spatial domain sigma
  - `sigma_range` (default 50.0): intensity range sigma
- Mode 2 -- NLM (Non-Local Means): patch-based denoising
  - `search_window` (default 21): search neighborhood size
  - `patch_size` (default 7): comparison patch size
  - `h_param` (default 10.0): filtering strength

**Sigma Estimation**: `xpe_noise_estimate_sigma` computes noise standard deviation via Median Absolute Deviation (MAD) on a uniform ROI. Formula: `sigma = 1.4826 * MAD(pixel_values)`

**Input/Output**: float32 (in-place)

**Constraints**:
- sigma_space and sigma_range must be positive
- search_window and patch_size must be odd and positive
- Performance: <= 100ms for 3072x3072 float32

### 3.3 SWU-2.3: ContrastEnhancer (POST-03 basic)

**Purpose**: Improve local contrast for diagnostic visibility using CLAHE.

**Algorithm**: Contrast Limited Adaptive Histogram Equalization (CLAHE)
- Divide image into tiles (default 8x8 grid)
- Build histogram per tile, clip at `clip_limit`
- Redistribute clipped counts uniformly
- Bilinear interpolation between tile boundaries

**Input/Output**: float32 (in-place)

**Constraints**:
- clip_limit must be >= 1.0; values < 1.0 return XPE_ERR_INVALID_INPUT
- tile_width and tile_height must be >= 2
- Image dimensions must be >= 2 * tile dimension
- Performance: <= 50ms for 3072x3072 float32

### 3.4 SWU-2.4: EdgeEnhancer (POST-04)

**Purpose**: Enhance edge sharpness for improved diagnostic detail using Unsharp Masking (USM).

**Algorithm**: `output[i] = input[i] + amount * (input[i] - blur(input)[i])` where `abs(input[i] - blur(input)[i]) >= threshold`
- `amount` (default 0.5): sharpening gain
- `radius` (default 2.0): Gaussian blur sigma for unsharp mask
- `threshold` (default 10.0): edge magnitude threshold to prevent noise amplification

**Input/Output**: float32 (in-place)

**Constraints**:
- amount must be in [0.0, 5.0]; outside range returns XPE_ERR_INVALID_INPUT
- radius must be in [0.5, 10.0]
- threshold must be >= 0.0
- Performance: <= 20ms for 3072x3072 float32

### 3.5 SWU-2.10: ExposureIndexCalc (SUP-03 / IEC 62494-1)

**Purpose**: Compute Exposure Index (EI) and Deviation Index (DI) from detector-domain image data per IEC 62494-1.

**Algorithm**:
- `EI = EIT * (mean_detector_signal / S0_reference)`
- `DI = 10.0 * log10(EI / EIT)`
- `EIT` (Exposure Index Target) selected from lookup table by `meta->bodyPart`
- `S0_reference` is a system-specific calibration constant (hardcoded per detector model)
- `mean_detector_signal` computed as arithmetic mean of all pixels in the input buffer

**Input**: float32 image (detector-domain, pre-log-transform), XpeImageMetadata
**Output**: `outEI` (float), `outDI` (float) via out-parameters

**Constraints**:
- Input must be detector-domain (pre-presentation) data
- Applicable to single irradiation event images only (IEC 62494-1 scope)
- Must be callable on whole image (Phase 1b) AND ROI-cropped image (Phase 2 reuse)
- Unknown bodyPart uses default EIT (general radiography)
- DI preferred band: [-1, +1]; acceptable: [-3, +3]; outside +/-3: post WARNING alert
- Empty image (width==0 or height==0) returns XPE_ERR_INVALID_INPUT

---

## 4. EARS Format Requirements

### 4.1 Log Transform (SWU-2.1 / POST-01)

**REQ-ENH-001**: WHEN `xpe_log_transform` is called with a valid float32 image and positive `normFactor`, the system SHALL apply `output[i] = normFactor * log10(input[i] + 1.0)` to every pixel in-place.

**REQ-ENH-002**: IF any input pixel value is negative, THEN the system SHALL clamp it to zero before applying the logarithm, ensuring `log10(0 + 1) = 0`.

**REQ-ENH-003**: IF `normFactor` is zero or negative, THEN the system SHALL return `XPE_ERR_INVALID_INPUT` without modifying the image.

**REQ-ENH-004**: WHEN `xpe_log_inverse` is called with a valid float32 image and positive `normFactor`, the system SHALL apply `output[i] = pow(10.0, input[i] / normFactor) - 1.0` to every pixel in-place.

**REQ-ENH-005**: IF `normFactor` is zero or negative for `xpe_log_inverse`, THEN the system SHALL return `XPE_ERR_INVALID_INPUT` without modifying the image.

**REQ-ENH-006**: WHILE processing a 3072x3072 float32 image, the system SHALL complete `xpe_log_transform` or `xpe_log_inverse` within 15 milliseconds.

### 4.2 Noise Reduction (SWU-2.2 / POST-02 basic)

**REQ-ENH-007**: WHEN `xpe_noise_reduce` is called with `params->mode == XPE_NOISE_BILATERAL`, the system SHALL apply bilateral filtering with the specified `sigma_space` and `sigma_range` parameters in-place.

**REQ-ENH-008**: WHEN `xpe_noise_reduce` is called with `params->mode == XPE_NOISE_NLM`, the system SHALL apply Non-Local Means denoising with the specified `search_window`, `patch_size`, and `h_param` parameters in-place.

**REQ-ENH-009**: IF `params` is NULL, THEN the system SHALL return `XPE_ERR_INVALID_INPUT` without modifying the image.

**REQ-ENH-010**: IF `sigma_space` or `sigma_range` is non-positive (bilateral mode), THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-ENH-011**: WHEN `xpe_noise_estimate_sigma` is called with a valid float32 image, the system SHALL compute the noise standard deviation via `sigma = 1.4826 * MAD(pixel_values)` and write the result to `outSigma`.

**REQ-ENH-012**: WHILE processing a 3072x3072 float32 image, the system SHALL complete `xpe_noise_reduce` within 100 milliseconds.

### 4.3 Contrast Enhancement (SWU-2.3 / POST-03 basic)

**REQ-ENH-013**: WHEN `xpe_contrast_enhance` is called with valid parameters, the system SHALL apply CLAHE with the specified `clip_limit`, `tile_width`, and `tile_height` in-place.

**REQ-ENH-014**: IF `params` is NULL, THEN the system SHALL use default parameters (clip_limit=3.0, tile_width=8, tile_height=8).

**REQ-ENH-015**: IF `clip_limit` is less than 1.0, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-ENH-016**: IF `tile_width` or `tile_height` is less than 2, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-ENH-017**: WHILE processing a 3072x3072 float32 image, the system SHALL complete `xpe_contrast_enhance` within 50 milliseconds.

### 4.4 Edge Enhancement (SWU-2.4 / POST-04)

**REQ-ENH-018**: WHEN `xpe_edge_enhance` is called with valid parameters, the system SHALL apply Unsharp Masking: `output[i] = input[i] + amount * (input[i] - blur(input)[i])` only where `abs(input[i] - blur(input)[i]) >= threshold`.

**REQ-ENH-019**: IF `params` is NULL, THEN the system SHALL use default parameters (amount=0.5, radius=2.0, threshold=10.0).

**REQ-ENH-020**: IF `amount` is outside [0.0, 5.0] or `radius` is outside [0.5, 10.0] or `threshold` is negative, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-ENH-021**: The system SHALL NOT introduce clinically misleading halo or ringing artifacts. Pixel overshoot SHALL be clamped to `max(original * 2.0, original + amount * threshold)`.

**REQ-ENH-022**: WHILE processing a 3072x3072 float32 image, the system SHALL complete `xpe_edge_enhance` within 20 milliseconds.

### 4.5 Exposure Index (SWU-2.10 / SUP-03)

**REQ-ENH-023**: WHEN `xpe_calc_exposure_index` is called with a valid float32 detector-domain image and metadata, the system SHALL compute `EI = EIT * (mean_pixel_value / S0_reference)` and write the result to `outEI`.

**REQ-ENH-024**: WHEN `xpe_calc_exposure_index` computes EI, the system SHALL also compute `DI = 10.0 * log10(EI / EIT)` and write the result to `outDI`.

**REQ-ENH-025**: The system SHALL select `EIT` (Exposure Index Target) from an internal lookup table keyed by `meta->bodyPart`. WHERE `meta->bodyPart` is unknown or empty, the system SHALL use the default general radiography EIT value.

**REQ-ENH-026**: IF the computed `DI` is outside the range [-3.0, +3.0], THEN the system SHALL post a WARNING-level alert via the alert queue indicating exposure deviation.

**REQ-ENH-027**: IF `img` is NULL, `meta` is NULL, `outEI` is NULL, or `outDI` is NULL, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-ENH-028**: IF `img->width == 0` or `img->height == 0`, THEN the system SHALL return `XPE_ERR_INVALID_INPUT` without computing EI/DI.

**REQ-ENH-029**: The system SHALL accept both whole-image buffers and ROI-cropped sub-buffers as input, enabling Phase 2 ROI-aware EI refinement without API signature changes.

**REQ-ENH-030**: IF `mean_pixel_value` is zero or negative (indicating invalid detector data), THEN the system SHALL return `XPE_ERR_PROCESSING_FAILED` and set `*outEI = 0.0f` and `*outDI = 0.0f`.

### 4.6 Cross-Cutting Requirements

**REQ-ENH-CC-001**: The system SHALL export all 7 API functions with C linkage (`extern "C"`), `__cdecl` calling convention, and blittable parameter types for .NET P/Invoke compatibility.

**REQ-ENH-CC-002**: IF any API function receives a NULL `img` pointer or an image with `format != XPE_PIXEL_FLOAT32`, THEN the system SHALL return `XPE_ERR_INVALID_INPUT`.

**REQ-ENH-CC-003**: The system SHALL NOT allocate heap memory that outlives a single function call. All processing uses the caller-provided buffer (in-place modification).

**REQ-ENH-CC-004**: The system SHALL be thread-safe for concurrent calls on independent image buffers. No global mutable state is permitted.

**REQ-ENH-CC-005**: WHILE the enhance_basic pipeline processes a 3072x3072 float32 image through all 5 stages (log + noise + contrast + edge + EI), the total elapsed time SHALL be <= 200ms.

---

## 5. Performance Budgets

| Function | 3072x3072 float32 Budget | Measurement |
|----------|-------------------------|-------------|
| `xpe_log_transform` | <= 15ms | Wall-clock, single thread |
| `xpe_log_inverse` | <= 15ms | Wall-clock, single thread |
| `xpe_noise_reduce` (bilateral) | <= 100ms | Wall-clock, single thread |
| `xpe_noise_reduce` (NLM) | <= 100ms | Wall-clock, single thread |
| `xpe_noise_estimate_sigma` | <= 20ms | Wall-clock, single thread |
| `xpe_contrast_enhance` | <= 50ms | Wall-clock, single thread |
| `xpe_edge_enhance` | <= 20ms | Wall-clock, single thread |
| `xpe_calc_exposure_index` | <= 5ms | Wall-clock, single thread |
| **Total pipeline** | **<= 200ms** | **Sum of individual stages** |

Reference image: 3072 x 3072 x float32 = 36 MB.

---

## 6. Error Codes

All error codes reuse existing definitions from `xpe_error.h`. No new error codes required:

| Code | Usage in enhance_basic |
|------|----------------------|
| `XPE_OK` (0) | Success |
| `XPE_ERR_INVALID_INPUT` (-1) | NULL pointer, wrong format, invalid params |
| `XPE_ERR_PROCESSING_FAILED` (-3) | EI computation failure (zero/negative mean) |
| `XPE_ERR_UNSUPPORTED_FORMAT` (-7) | Non-float32 input to enhancement functions |

---

## 7. Acceptance Criteria

| AC-ID | Description | Pass Condition |
|-------|-------------|----------------|
| AC-01 | Log transform round-trip fidelity | `xpe_log_inverse(xpe_log_transform(img))` restores pixel values within 1e-4 relative error |
| AC-02 | Bilateral filter noise reduction | SNR improvement >= 3 dB on synthetic noisy image |
| AC-03 | NLM denoising edge preservation | Edge gradient magnitude preserved within 90% after denoising |
| AC-04 | CLAHE local contrast improvement | Local contrast ratio increases by >= 20% in low-contrast regions |
| AC-05 | USM no-artifact guarantee | No pixel exceeds overshoot clamp bound |
| AC-06 | EI/DI IEC 62494-1 compliance | EI/DI values match reference implementation within 0.1% for standard test phantom |
| AC-07 | EI DI warning alert | DI outside [-3, +3] triggers WARNING alert |
| AC-08 | P/Invoke compatibility | All 7 functions callable from C# without marshalling exceptions |
| AC-09 | Performance budget | All functions meet individual and total pipeline budgets |
| AC-10 | Thread safety | Concurrent execution on independent buffers produces identical results to serial execution |

---

## 8. IEC 62304 Traceability

### 8.1 SWU-to-REQ Mapping

| SWU | Requirements | Test File |
|-----|-------------|-----------|
| SWU-2.1 LogTransform | REQ-ENH-001..006 | test_log_transform.cpp |
| SWU-2.2 NoiseReducer | REQ-ENH-007..012 | test_noise_reduce.cpp |
| SWU-2.3 ContrastEnhancer | REQ-ENH-013..017 | test_contrast_enhance.cpp |
| SWU-2.4 EdgeEnhancer | REQ-ENH-018..022 | test_edge_enhance.cpp |
| SWU-2.10 ExposureIndexCalc | REQ-ENH-023..030 | test_exposure_index.cpp |
| Cross-Cutting | REQ-ENH-CC-001..005 | test_enhance_integration.cpp |

### 8.2 Upstream Traceability (SPEC-XPE-MASTER)

| Master Deliverable | SPEC-XPE-P1B-ENH SWU | Status |
|-------------------|----------------------|--------|
| P1b-01 Log Transform / Inverse | SWU-2.1 | Draft |
| P1b-02 Noise Reduction (Bilateral + NLM) | SWU-2.2 | Draft |
| P1b-03 Contrast Enhancement (CLAHE) | SWU-2.3 | Draft |
| P1b-04 Edge Enhancement (USM) | SWU-2.4 | Draft |
| P1b-04a Exposure Index Baseline | SWU-2.10 | Draft |

### 8.3 Algorithm Specification Traceability

| SWU | Research ID | ALG-SPEC Section | Normative Standard |
|-----|-------------|------------------|--------------------|
| SWU-2.1 | POST-01 | Section 6.1 row "Log transform" | -- |
| SWU-2.2 | POST-02 basic | Section 6.1 row "Noise reduction" | -- |
| SWU-2.3 | POST-03 basic | Section 6.1 row "Contrast enhancement" | -- |
| SWU-2.4 | POST-04 | Section 6.1 row "Edge enhancement" | -- |
| SWU-2.10 | SUP-03 | Section 6.3 | IEC 62494-1, AAPM TG-232 |

---
