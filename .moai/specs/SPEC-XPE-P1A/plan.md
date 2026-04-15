# SPEC-XPE-P1A: Implementation Plan

**SPEC**: SPEC-XPE-P1A v1.0.0
**Sprint**: S1-A
**DLL**: xpe_preprocess.dll
**Dependency**: S0-B (xpe_common.dll) must be complete

---

## 1. Implementation Strategy

TDD methodology (RED-GREEN-REFACTOR) per project quality.yaml. Each SWU implemented as an independent compilation unit with its own test file.

### 1.1 File Structure

```
modules/preprocess/
  include/xpe/preprocess/
    xpe_preprocess_api.h      -- 18 function declarations
    xpe_preprocess_internal.h  -- internal helpers (not exported)
  src/
    offset_correct.cpp         -- SWU-1.1
    gain_correct.cpp           -- SWU-1.2
    defect_correct.cpp         -- SWU-1.3
    ghost_correct.cpp          -- SWU-1.4
    calibration_manager.cpp    -- SWU-1.5
    temp_compensate.cpp        -- SWU-1.6
    nonlinearity_correct.cpp   -- SWU-1.7
    binning_correct.cpp        -- SWU-1.8
    readout_validate.cpp       -- SWU-1.9
    pipeline.cpp               -- Pipeline orchestration (optional)
  tests/
    test_offset_correct.cpp
    test_gain_correct.cpp
    test_defect_correct.cpp
    test_ghost_correct.cpp
    test_calibration_manager.cpp
    test_temp_compensate.cpp
    test_nonlinearity_correct.cpp
    test_binning_correct.cpp
    test_readout_validate.cpp
    test_pipeline_integration.cpp
    test_boundary_conditions.cpp
  CMakeLists.txt
```

---

## 2. Milestones (Priority-Based)

### Milestone 1: Core Corrections (Priority High)

- SWU-1.1: Offset correction (simplest, validates infrastructure)
- SWU-1.2: Gain correction (uint16-to-float32 domain transition)
- SWU-1.9: Readout artifact validation (pipeline entry point)
- Unit tests for all three SWUs

### Milestone 2: Extended Corrections (Priority High)

- SWU-1.6: Temperature compensation
- SWU-1.7: Nonlinearity correction
- SWU-1.8: Binning correction (conditional)
- SWU-1.3: Defect pixel correction (edge-aware interpolation)
- Unit tests for all four SWUs

### Milestone 3: Ghost Correction + Calibration (Priority High)

- SWU-1.4: Ghost/Lag Tier 1 (LTI deconvolution, 4 functions)
- SWU-1.5: Calibration Manager (load/save/generate/expiry, 6 functions)
- Unit tests for both SWUs

### Milestone 4: Integration + Quality (Priority High)

- Pipeline ordering enforcement and integration tests
- P/Invoke compatibility test (C# round-trip)
- Performance benchmark suite
- Memory leak testing (ASan, 1000-frame cycle)
- Coverage verification (>= 90% statement, >= 80% branch)

---

## 3. Technical Approach

### 3.1 Algorithm References

| SWU | Algorithm | Reference |
|-----|-----------|-----------|
| SWU-1.1 | Per-pixel subtraction with clamp-to-zero | IEC 62220-1-1 dark field subtraction |
| SWU-1.2 | Per-pixel multiplication + format conversion | IEC 62220-1-1 flat-field normalization |
| SWU-1.3 | Edge-aware bilinear interpolation | PMC7930811 baseline method |
| SWU-1.4 | Dual-exponential LTI deconvolution | PMC3465354 Starman et al. 2012 |
| SWU-1.5 | CRC-32 file integrity + expiry timestamp | Standard practice |
| SWU-1.6 | Exponential dark current model | EP2148500A1 patent |
| SWU-1.7 | Piecewise linear / polynomial correction | NUC literature (TPC baseline) |
| SWU-1.8 | Mode-specific gain/uniformity compensation | Detector vendor calibration |
| SWU-1.9 | Statistical pattern detection (line noise, dropped columns) | PRE-01 internal spec |

### 3.2 Data Domain Transition

- Stages 0.5 through 1.5: operate on uint16 raw pixel data
- Stage 2 (gain correction): converts uint16 to float32
- Stages 2.5 through 4: operate on float32 corrected data

### 3.3 Ghost Corrector Handle Design

- Opaque `void*` handle for C ABI compatibility
- Internal C++ struct holding: frame history buffer, IRF coefficients, state
- Handle-per-detector design (not shared across threads)
- Memory lifecycle: create -> correct (N frames) -> reset -> correct -> destroy

---

## 4. Risks

| Risk | Impact | Likelihood | Mitigation |
|------|--------|:----------:|-----------|
| Ghost Tier 1 exceeds 150ms budget | Performance gate failure | Medium | Profile early; consider SIMD optimization for inner loop |
| Calibration file format not finalized | Load/save implementation blocked | Low | Use minimal raw binary format; add DICOM support later |
| Edge-aware interpolation complexity | Defect correction exceeds 80ms | Medium | Limit neighborhood to 5x5; profile and optimize |
| NaN propagation in float32 pipeline | Silent data corruption | Low | Add NaN checks at gain correction output |
| uint16 underflow in offset correction | Data corruption | Low | Clamp-to-zero implemented per REQ-P1A-011 |

---

*Plan End -- SPEC-XPE-P1A v1.0.0*
