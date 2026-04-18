# SPEC-XPE-P1B-DISP: Acceptance Criteria

**SPEC ID**: SPEC-XPE-P1B-DISP
**Version**: 1.0.0
**Date**: 2026-04-16

---

## 1. Modality LUT Correctness

**Test Approach**: Unit test with known input/output pairs

**Given** a 3072x3072 float32 image with pixel values in range [0, 4095]
**When** `xpe_apply_modality_lut` is called with `mode=LINEAR`, `rescaleSlope=1.0`, `rescaleIntercept=-1024.0`
**Then** each output pixel SHALL equal `input * 1.0 + (-1024.0)` with float32 precision (relative error < 1e-6)
**And** the image format SHALL remain `XPE_PIXEL_FLOAT32`

**Pass Condition**: All pixels match expected values within tolerance. Return code is `XPE_OK`.

---

## 2. Modality LUT Table Mode

**Test Approach**: Unit test with DICOM Modality LUT Sequence simulation

**Given** a float32 image and a 256-entry Modality LUT with `lutFirstMapped=0` and monotonically increasing entries
**When** `xpe_apply_modality_lut` is called with `mode=TABLE`
**Then** each pixel SHALL be mapped through the LUT: `output = lutData[clamp(round(input) - lutFirstMapped, 0, lutLength-1)]`
**And** input values below `lutFirstMapped` SHALL map to `lutData[0]`
**And** input values above `lutFirstMapped + lutLength - 1` SHALL map to `lutData[lutLength-1]`

**Pass Condition**: Boundary clamping verified for out-of-range inputs. Return code is `XPE_OK`.

---

## 3. VOI LUT Windowing (All 3 Modes)

**Test Approach**: Unit test with analytical verification

**Given** a float32 image post-Modality-LUT with known pixel distribution
**When** `xpe_apply_voi_lut` is called with `mode=LINEAR`, `center=40.0`, `width=400.0`, `minOut=0.0`, `maxOut=1.0`
**Then** pixel at value `center` SHALL map to approximately 0.5
**And** pixel at value `center - width/2` SHALL map to `minOut` (0.0)
**And** pixel at value `center + width/2` SHALL map to `maxOut` (1.0)
**And** all output values SHALL be in `[minOut, maxOut]`

**When** the same test is repeated with `mode=SIGMOID`
**Then** pixel at value `center` SHALL map to approximately 0.5 (sigmoid midpoint)
**And** transition SHALL be smooth (no discontinuities)

**When** the same test is repeated with `mode=LINEAR_EXACT`
**Then** the full window range SHALL map exactly from `minOut` to `maxOut` without half-value offset

**Pass Condition**: All 3 modes produce mathematically correct output within float32 precision. Return code is `XPE_OK`.

---

## 4. VOI LUT Interactive Performance

**Test Approach**: Performance benchmark with timing measurement

**Given** a 3072x3072 float32 image (approximately 36MB)
**When** `xpe_apply_voi_lut` is called in any mode (LINEAR, LINEAR_EXACT, SIGMOID)
**Then** execution SHALL complete within 16ms on reference hardware

**Pass Condition**: Median of 100 runs <= 16ms. P95 <= 20ms.

---

## 5. VOI Preset Factory

**Test Approach**: Unit test for all body part presets

**Given** a valid `XpeVoiLutParams` struct
**When** `xpe_voi_preset_create` is called with `XPE_BODY_BONE`
**Then** `params->center` SHALL be 500.0 and `params->width` SHALL be 2000.0

**When** called with `XPE_BODY_LUNG`
**Then** `params->center` SHALL be -600.0 and `params->width` SHALL be 1600.0

**When** called with `XPE_BODY_ABDOMEN`
**Then** `params->center` SHALL be 40.0 and `params->width` SHALL be 400.0

**When** called with `XPE_BODY_HEAD`
**Then** `params->center` SHALL be 40.0 and `params->width` SHALL be 80.0

**When** called with an invalid body part value (e.g., -1 or 999)
**Then** return code SHALL be `XPE_ERR_INVALID_INPUT`

**Pass Condition**: All 4 presets return correct values. Invalid input returns error. `minOut` and `maxOut` are set to 0.0 and 1.0 respectively.

---

## 6. Presentation LUT with Domain Transition

**Test Approach**: Unit test verifying float32 -> uint16 conversion

**Given** a 3072x3072 float32 image with all pixels in [0.0, 1.0] range
**And** a `XpePresentationLutParams` with a linear identity LUT (`lutData[i] = i * 64` for i in 0..1023)
**When** `xpe_apply_presentation_lut` is called
**Then** the image SHALL be converted from `XPE_PIXEL_FLOAT32` to `XPE_PIXEL_UINT16`
**And** `img->format` SHALL be `XPE_PIXEL_UINT16`
**And** `img->bitsAllocated` SHALL be 16
**And** `img->bitsStored` SHALL be 16
**And** `img->dataSize` SHALL be `width * height * 2` bytes
**And** each output pixel SHALL equal `lutData[round(input * 1023)]`

**Pass Condition**: Format transition complete, all pixel values correct, no memory leaks.

---

## 7. GSDF Calibration

**Test Approach**: Unit test with known luminance values

**Given** an array of 256 luminance values representing display measurements from L_min to L_max
**When** `xpe_gsdf_calibrate` is called
**Then** `outParams->lutData` SHALL contain a monotonically non-decreasing 1024-entry LUT
**And** `outParams->gsdfEnabled` SHALL be 1
**And** the LUT SHALL produce perceptually uniform steps (JND-linearized) per DICOM PS3.14

**Pass Condition**: LUT is monotonic, gsdfEnabled is set, return code is `XPE_OK`.

---

## 8. Full Display Pipeline Integration

**Test Approach**: Integration test running all 3 stages sequentially

**Given** a 3072x3072 float32 image (post-enhancement, pixel values in detector range)
**When** `xpe_apply_modality_lut` is called (linear, slope=1.0, intercept=-1024.0)
**And** `xpe_apply_voi_lut` is called (LINEAR, center=40, width=400, minOut=0, maxOut=1)
**And** `xpe_apply_presentation_lut` is called (identity LUT)
**Then** the final image SHALL be uint16 format
**And** the total pipeline execution time SHALL be <= 65ms
**And** no memory leaks SHALL be detected

**Pass Condition**: Complete pipeline produces valid uint16 output within performance budget.

---

## 9. Error Handling Robustness

**Test Approach**: Negative test cases for all error paths

**Given** various invalid inputs:
- NULL image pointer
- NULL params pointer
- Image with `format == XPE_PIXEL_UINT16` (wrong format for display functions)
- VOI LUT with `width == 0.0`
- VOI LUT with `width < 0.0`
- Modality LUT with `rescaleSlope == 0.0`
- Modality LUT table mode with `lutData == NULL`
- Modality LUT table mode with `lutLength == 0`
- GSDF calibrate with `count < 2`

**When** the corresponding function is called
**Then** each SHALL return the appropriate `XPE_ERR_*` code
**And** the image SHALL NOT be modified
**And** no memory SHALL be leaked

**Pass Condition**: All error codes match specification. Image data unchanged after error.

---

## 10. Boundary Conditions

**Test Approach**: Edge case testing with extreme image sizes

**Given** a 1x1 float32 image (single pixel)
**When** all 3 display stages are applied sequentially
**Then** the single pixel SHALL be correctly transformed through all stages
**And** the final output SHALL be a 1x1 uint16 image

**Given** a 4096x4096 float32 image (maximum supported size)
**When** all 3 display stages are applied
**Then** processing SHALL complete within memory budget (dataSize <= 64MB per XpeImageBuffer)
**And** no buffer overflow SHALL occur

**Pass Condition**: Both minimum and maximum image sizes processed correctly.

---

## Quality Gate Criteria

| Gate | Target | REQ Trace |
|------|--------|-----------|
| Unit test coverage (statement) | >= 90% | TRUST 5 - Tested |
| Unit test coverage (branch) | >= 80% | TRUST 5 - Tested |
| Static analysis (cppcheck) | 0 warnings | TRUST 5 - Unified |
| Static analysis (clang-tidy) | 0 warnings | TRUST 5 - Unified |
| Memory leak (ASan, 1000 frames) | 0 leaks | REQ-DISP-033 |
| DLL export count | Exactly 5 | REQ-DISP-029 |
| P/Invoke compatibility | Round-trip pass | REQ-DISP-029 |
| Performance: Modality LUT | <= 20ms | REQ-DISP-008 |
| Performance: VOI LUT | <= 16ms | REQ-DISP-016 |
| Performance: Presentation LUT | <= 25ms | REQ-DISP-028 |

---

## Definition of Done

- [ ] All 35 EARS requirements (REQ-DISP-001 through REQ-DISP-035) have corresponding test cases
- [ ] All tests pass (>= 38 test cases across 5 test files)
- [ ] `dumpbin /exports xpe_display.dll` lists exactly 5 functions
- [ ] P/Invoke round-trip test passes (C# struct layout compatibility)
- [ ] Statement coverage >= 90%, branch coverage >= 80%
- [ ] cppcheck --std=c++17 reports 0 warnings
- [ ] clang-tidy (modernize-*, performance-*, bugprone-*) reports 0 warnings
- [ ] ASan clean over 1000-frame processing cycle
- [ ] VOI LUT interactive latency <= 16ms (median of 100 runs, 3072x3072)
- [ ] Full display pipeline (Modality + VOI + Presentation) <= 65ms
- [ ] float32 -> uint16 domain transition verified (format, bits, dataSize correct)
- [ ] IEC 62304 traceability: all REQs traceable to SWU, test, and git commit

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI (manager-spec) | Initial acceptance criteria (10 scenarios + quality gates) |

---

*Document End -- SPEC-XPE-P1B-DISP acceptance.md v1.0.0*
