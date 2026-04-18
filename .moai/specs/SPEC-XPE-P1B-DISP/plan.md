# SPEC-XPE-P1B-DISP: Implementation Plan

**SPEC ID**: SPEC-XPE-P1B-DISP
**Version**: 1.0.0
**Date**: 2026-04-16
**Methodology**: TDD (RED-GREEN-REFACTOR)

---

## 1. Implementation Strategy

### 1.1 TDD Approach

Each SWU follows the RED-GREEN-REFACTOR cycle:
1. **RED**: Write failing Google Test cases for EARS requirements
2. **GREEN**: Implement minimal code to pass all tests
3. **REFACTOR**: Optimize, clean up, ensure TRUST 5 compliance

### 1.2 SWU Execution Order

```
SWU-3.1 (ModalityLUT)  -->  SWU-3.2 (VOILUT)  -->  SWU-3.3 (PresentationLUT + GSDF)
                                                           |
                                                     Integration Tests
```

Rationale: Pipeline stages are sequential (Modality -> VOI -> Presentation). Implementing in pipeline order enables integration testing at each step.

---

## 2. File Structure

### 2.1 Headers

| File | Purpose |
|------|---------|
| `modules/display/include/xpe/display/xpe_display_api.h` | Public API (5 functions + param structs + enums) |
| `modules/display/include/xpe/display/xpe_display_internal.h` | Internal helpers (LUT math, clamping) |

### 2.2 Implementation

| File | SWU | Functions |
|------|-----|-----------|
| `modules/display/src/modality_lut.cpp` | SWU-3.1 | `xpe_apply_modality_lut` |
| `modules/display/src/voi_lut.cpp` | SWU-3.2 | `xpe_apply_voi_lut`, `xpe_voi_preset_create` |
| `modules/display/src/presentation_lut.cpp` | SWU-3.3 | `xpe_apply_presentation_lut`, `xpe_gsdf_calibrate` |
| `modules/display/src/display_helpers.cpp` | -- | Shared utilities (clamping, format validation) |

### 2.3 Tests

| File | SWU | Min Test Count | REQ Trace |
|------|-----|:--------------:|-----------|
| `modules/display/tests/test_modality_lut.cpp` | SWU-3.1 | >= 8 | REQ-DISP-001..008 |
| `modules/display/tests/test_voi_lut.cpp` | SWU-3.2 | >= 10 | REQ-DISP-009..018 |
| `modules/display/tests/test_presentation_lut.cpp` | SWU-3.3 | >= 10 | REQ-DISP-019..028 |
| `modules/display/tests/test_display_integration.cpp` | All | >= 6 | REQ-DISP-029..035 |
| `modules/display/tests/test_display_boundary.cpp` | All | >= 4 | REQ-DISP-034..035 |

### 2.4 Build

| File | Purpose |
|------|---------|
| `modules/display/CMakeLists.txt` | Library target + GTest integration |

---

## 3. Task Breakdown

### Milestone 1: Project Setup (Priority High)

| Task | Description |
|------|-------------|
| M1-01 | Create `modules/display/` directory structure (include/, src/, tests/) |
| M1-02 | Create `CMakeLists.txt` with xpe_display shared library target |
| M1-03 | Define `xpe_display_api.h` with all enums, structs, and 5 function declarations |
| M1-04 | Create `xpe_display_internal.h` with helper function declarations |

### Milestone 2: SWU-3.1 ModalityLUT (Priority High)

| Task | Description | REQ Trace |
|------|-------------|-----------|
| M2-01 | RED: Write `test_modality_lut.cpp` -- linear rescale tests (slope/intercept variations) | REQ-DISP-001 |
| M2-02 | RED: Write LUT-table mode tests (boundary clamping, index offset) | REQ-DISP-002 |
| M2-03 | RED: Write error handling tests (NULL, wrong format, zero slope, empty LUT) | REQ-DISP-003..007 |
| M2-04 | GREEN: Implement `xpe_apply_modality_lut` in `modality_lut.cpp` | REQ-DISP-001..007 |
| M2-05 | GREEN: Implement shared helpers in `display_helpers.cpp` | -- |
| M2-06 | REFACTOR: Performance optimization, verify <= 20ms budget | REQ-DISP-008 |

### Milestone 3: SWU-3.2 VOILUT (Priority High)

| Task | Description | REQ Trace |
|------|-------------|-----------|
| M3-01 | RED: Write `test_voi_lut.cpp` -- LINEAR mode windowing tests | REQ-DISP-009 |
| M3-02 | RED: Write LINEAR_EXACT mode tests | REQ-DISP-010 |
| M3-03 | RED: Write SIGMOID mode tests | REQ-DISP-011 |
| M3-04 | RED: Write output clamping and error handling tests | REQ-DISP-012..015 |
| M3-05 | RED: Write `xpe_voi_preset_create` tests (all 4 body parts + invalid) | REQ-DISP-017..018 |
| M3-06 | GREEN: Implement `xpe_apply_voi_lut` in `voi_lut.cpp` | REQ-DISP-009..016 |
| M3-07 | GREEN: Implement `xpe_voi_preset_create` in `voi_lut.cpp` | REQ-DISP-017..018 |
| M3-08 | REFACTOR: Verify <= 16ms interactive latency budget | REQ-DISP-016 |

### Milestone 4: SWU-3.3 PresentationLUT + GSDF (Priority High)

| Task | Description | REQ Trace |
|------|-------------|-----------|
| M4-01 | RED: Write `test_presentation_lut.cpp` -- LUT lookup tests (index mapping, clamping) | REQ-DISP-019..021 |
| M4-02 | RED: Write float32->uint16 domain transition tests (format, bits, dataSize) | REQ-DISP-020 |
| M4-03 | RED: Write error handling tests (NULL, wrong format) | REQ-DISP-022..023 |
| M4-04 | RED: Write GSDF calibration tests (luminance -> JND mapping) | REQ-DISP-025..027 |
| M4-05 | RED: Write GSDF error handling tests (NULL, count < 2) | REQ-DISP-026 |
| M4-06 | GREEN: Implement `xpe_apply_presentation_lut` in `presentation_lut.cpp` | REQ-DISP-019..024 |
| M4-07 | GREEN: Implement `xpe_gsdf_calibrate` in `presentation_lut.cpp` | REQ-DISP-025..027 |
| M4-08 | REFACTOR: Verify <= 25ms budget, optimize memory reallocation | REQ-DISP-028 |

### Milestone 5: Integration and Quality (Priority High)

| Task | Description | REQ Trace |
|------|-------------|-----------|
| M5-01 | Write `test_display_integration.cpp` -- full Modality->VOI->Presentation pipeline | REQ-DISP-029..033 |
| M5-02 | Write `test_display_boundary.cpp` -- 1x1 and 4096x4096 edge cases | REQ-DISP-034..035 |
| M5-03 | Verify `dumpbin /exports xpe_display.dll` lists exactly 5 functions | REQ-DISP-029 |
| M5-04 | P/Invoke round-trip test (C# struct layout compatibility) | REQ-DISP-029 |
| M5-05 | Memory leak check (ASan, 1000-frame cycle) | REQ-DISP-033 |
| M5-06 | Performance benchmark: all 3 stages within budget | REQ-DISP-008,016,028 |
| M5-07 | Static analysis: cppcheck + clang-tidy 0 warnings | TRUST 5 |

---

## 4. Technical Approach

### 4.1 Modality LUT Algorithm

**Linear mode**: Simple per-pixel multiply-add. Highly parallelizable, candidate for SIMD optimization if needed for performance.

**Table mode**: Index computation from float input requires rounding to nearest integer, subtracting `lutFirstMapped`, and clamping to `[0, lutLength-1]`. Out-of-range values map to boundary entries per DICOM PS3.3 C.11.1.

### 4.2 VOI LUT Algorithm

**LINEAR**: Standard DICOM windowing formula with half-value offset convention. Output clamped to `[minOut, maxOut]`.

**LINEAR_EXACT**: No half-value offset -- maps the entire window range exactly. Per DICOM PS3.3 C.11.2.1.3.

**SIGMOID**: Logistic function providing smooth contrast transition. The factor `-4/width` provides the standard sigmoid shape where ~98% of values fall within the window.

**Presets**: Static lookup table mapping `XpeBodyPart` to clinically standard center/width values. Values sourced from standard radiology practice.

### 4.3 Presentation LUT + GSDF

**Presentation LUT**: 1024-entry lookup table maps normalized [0,1] input to uint16 output. Index = round(input * 1023). This stage also performs the float32->uint16 buffer conversion (reallocate or in-place overwrite if buffer is large enough).

**GSDF Calibration** (DICOM PS3.14): Given display luminance measurements at multiple digital driving levels, compute a LUT that linearizes perceptual contrast. The algorithm:
1. Convert luminance values to JND index using DICOM PS3.14 Barten model
2. Create monotonic mapping from linear p-value to JND space
3. Invert the mapping to produce digital driving levels for uniform JND steps
4. Populate 1024-entry LUT with resulting values

### 4.4 Memory Management for Domain Transition

The Presentation LUT stage converts float32 (4 bytes/pixel) to uint16 (2 bytes/pixel). Strategy:
- Allocate new uint16 buffer (half the size)
- Apply LUT lookup during conversion
- Free old float32 buffer
- Update XpeImageBuffer fields (format, bitsAllocated, bitsStored, dataSize, data pointer)

---

## 5. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| GSDF Barten model numerical instability | Medium | Use well-established reference implementation; validate against DICOM PS3.14 test vectors |
| VOI LUT 16ms budget tight for large images | Medium | Profile early; use SIMD (SSE2/AVX2) if needed for exp() in sigmoid mode |
| float32->uint16 reallocation failure | Low | Return `XPE_ERR_OUT_OF_MEMORY`; caller retries or handles gracefully |
| Precision loss in Modality LUT table mode | Low | Use nearest-neighbor interpolation per DICOM spec; document precision characteristics |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI (manager-spec) | Initial implementation plan |

---

*Document End -- SPEC-XPE-P1B-DISP plan.md v1.0.0*
