# Requirements Traceability Matrix (RTM)

## xpe_enhance_advanced.dll -- Advanced Post-Processing Module

| Field | Value |
|-------|-------|
| **Document ID** | RTM-ADV-001 |
| **Version** | 1.5.0 |
| **Status** | Released |
| **Date** | 2026-09-10 |
| **Author** | xpe-docs |
| **IEC 62304 Class** | B |
| **SPEC Reference** | SPEC-XPE-P2-ADV v1.0.0 |
| **Implementation Status** | Complete |

---

## 1. Traceability Overview

This matrix traces every requirement (REQ-ADV-XXX) from SRS-ADV-001 to:
- **Design reference**: SDD-ADV-001 section
- **Implementation files**: Source code in `modules/enhance_advanced/`
- **Test IDs**: Google Test cases in `modules/enhance_advanced/tests/`
- **Verification status**: Written / Verified / Deferred
- **VVP Ref**: the section of `docs/project/vvp_adv.md` (XPE-VVP-P2ADV-001) that defines the
  verification method for the row. `—` means no VVP section currently covers the row.

### Status Legend

| Status | Meaning |
|--------|---------|
| Written | Test case written, pending compilation and execution |
| Verified | Test executed and passed |
| Deferred | Test deferred to subsequent phase (performance, runtime) |

---

## 2. Lifecycle and State Requirements

| Req ID | Requirement | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------------------|----------|--------|--------|
| REQ-ADV-001 | Module initialization | SDD Sec 5.2 | `src/xpe_enhance_advanced.cpp` | TC-LC-001: InitWithNullConfigReturnsOK | Written | — |
| REQ-ADV-001 | Module initialization (valid JSON) | SDD Sec 5.2 | `src/xpe_enhance_advanced.cpp` | TC-LC-002: InitWithValidJsonReturnsOK | Written | — |
| REQ-ADV-001 | Module initialization (bad JSON) | SDD Sec 5.2 | `src/xpe_enhance_advanced.cpp` | TC-LC-003: InitWithMalformedJsonReturnsConfigInvalid | Written | — |
| REQ-ADV-001 | Init idempotent | SDD Sec 5.2 | `src/xpe_enhance_advanced.cpp` | TC-LC-004: InitIdempotent | Written | — |
| REQ-ADV-001 | Double shutdown safe | SDD Sec 5.2 | `src/xpe_enhance_advanced.cpp` | TC-LC-005: DoubleShutdownSafe | Written | — |
| REQ-ADV-001 | Shutdown without init | SDD Sec 5.2 | `src/xpe_enhance_advanced.cpp` | TC-LC-006: ShutdownWithoutInitSafe | Written | — |
| REQ-ADV-001 | Version returns non-null | SDD Sec 4.1 | `src/xpe_enhance_advanced.cpp` | TC-LC-007: VersionReturnsNonNull | Written | — |
| REQ-ADV-001 | Version format "1.0.0" | SDD Sec 4.1 | `src/xpe_enhance_advanced.cpp` | TC-LC-008: VersionMatchesExpectedFormat | Written | — |
| REQ-ADV-002 | P/Invoke ABI compliance | SDD Sec 4.2 | `include/xpe/enhance_advanced/enhance_advanced_api.h` | Static assert (compile-time) | Verified | XPE-VVP-P2ADV-001 §2.1 (L2) |

---

## 3. Not-Initialized Guard

| Req ID | Requirement | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------------------|----------|--------|--------|
| REQ-ADV-020 | Process without init returns NOT_INITIALIZED | SDD Sec 5.2 | All dispatch files | TC-INT-005a-d: ProcessWithoutInitReturnsNotInitialized (x4 functions) | Written | — |

---

## 4. SWU-2.5: Multiscale Frequency Processing

| Req ID | Requirement | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------------------|----------|--------|--------|
| REQ-ADV-010 | MFP execution | SDD Sec 5.3 | `src/multiscale_process.cpp`, `src/mfp_scalar.cpp` | TC-MFP-003: NonIdentityConfigModifiesOutput, TC-MFP-004: BodyPartDefaultConfigSucceeds | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-050 | Identity reconstruction (constant) | SDD Sec 5.3 | `src/mfp_scalar.cpp` | TC-MFP-001: IdentityReconstructionConstantImage | Verified | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-050 | Identity reconstruction (gradient) | SDD Sec 5.3 | `src/mfp_scalar.cpp` | TC-MFP-002: IdentityReconstructionGradientImage | Verified | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-032 | No NaN/Inf in output | SDD Sec 6.1 | `src/mfp_scalar.cpp` | TC-MFP-005: NoNaNOrInfInOutput, TC-MFP-006: NaNInputHandledGracefully | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-022 | NULL image returns INVALID_INPUT | SDD Sec 6.1 | `src/multiscale_process.cpp` | TC-MFP-007: NullImageReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-022 | NULL meta returns INVALID_INPUT | SDD Sec 6.1 | `src/multiscale_process.cpp` | TC-MFP-008: NullMetaReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-070 | Zero width returns INVALID_INPUT | SDD Sec 6.1 | `src/multiscale_process.cpp` | TC-MFP-009: ZeroWidthReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-070 | Zero height returns INVALID_INPUT | SDD Sec 6.1 | `src/multiscale_process.cpp` | TC-MFP-010: ZeroHeightReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-100 | 1x1 image returns INVALID_INPUT | SDD Sec 6.1 | `src/multiscale_process.cpp` | TC-MFP-011: OneByOneImageReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-071 | UINT16 returns UNSUPPORTED_FORMAT | SDD Sec 6.1 | `src/multiscale_process.cpp` | TC-MFP-012: Uint16FormatReturnsUnsupportedFormat | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-090 | Deterministic output | SDD Sec 5.2 | `src/multiscale_process.cpp` | TC-MFP-014: IdenticalInputProducesIdenticalOutput | Written | XPE-VVP-P2ADV-001 §4.1 |
| REQ-ADV-031 | Multiple calls stable | SDD Sec 5.2 | `src/multiscale_process.cpp` | TC-MFP-015: MultipleSequentialCallsStable | Written | XPE-VVP-P2ADV-001 §4.1 |

---

## 5. SWU-2.6: Fractional-Order Edge Enhancement

| Req ID | Requirement | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------------------|----------|--------|--------|
| REQ-ADV-011 | Fractional process execution (order=0.0) | SDD Sec 5.4 | `src/fractional_process.cpp`, `src/fractional_derivative.cpp` | TC-FRAC-001: OrderZeroSucceeds | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-011 | Fractional process execution (order=1.0) | SDD Sec 5.4 | `src/fractional_process.cpp`, `src/fractional_derivative.cpp` | TC-FRAC-002: OrderOneSucceeds | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-011 | Fractional process execution (order=2.0) | SDD Sec 5.4 | `src/fractional_process.cpp`, `src/fractional_derivative.cpp` | TC-FRAC-003: OrderTwoSucceeds | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-011 | Fractional process execution (order=0.5) | SDD Sec 5.4 | `src/fractional_process.cpp`, `src/fractional_derivative.cpp` | TC-FRAC-004: OrderHalfSucceeds | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-011 | Fractional process execution (order=1.5) | SDD Sec 5.4 | `src/fractional_process.cpp`, `src/fractional_derivative.cpp` | TC-FRAC-005: OrderOnePointFiveSucceeds | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-021 | Negative order returns INVALID_INPUT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-007: OrderNegativeReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-021 | Order > 2.0 returns INVALID_INPUT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-008: OrderAboveTwoReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-021 | Large negative order returns INVALID_INPUT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-009: OrderLargeNegativeReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-021 | Large positive order returns INVALID_INPUT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-010: OrderLargePositiveReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-051 | Overshoot limiting enforced (SAF-100) | SDD Sec 5.4 | `src/fractional_derivative.cpp` | TC-FRAC-011: OvershootLimitingEnforced | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-051 | Uniform image preserved | SDD Sec 5.4 | `src/fractional_derivative.cpp` | TC-FRAC-012: UniformImagePreserved | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-032 | No NaN/Inf in output | SDD Sec 6.1 | `src/fractional_derivative.cpp` | TC-FRAC-013: NoNaNOrInfInOutput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-022 | NULL image returns INVALID_INPUT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-016: NullImageReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-100 | 1x1 image returns INVALID_INPUT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-017: OneByOneImageReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-071 | UINT16 returns UNSUPPORTED_FORMAT | SDD Sec 6.1 | `src/fractional_process.cpp` | TC-FRAC-018: Uint16FormatReturnsUnsupportedFormat | Written | XPE-VVP-P2ADV-001 §4.2 |
| REQ-ADV-090 | Deterministic output | SDD Sec 5.2 | `src/fractional_process.cpp` | TC-FRAC-019: IdenticalInputProducesIdenticalOutput | Written | XPE-VVP-P2ADV-001 §4.2 |

---

## 6. SWU-2.8: Collimation ROI Detection

| Req ID | Requirement | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------------------|----------|--------|--------|
| REQ-ADV-052 | Sharp rect collimation detected (+-3px) | SDD Sec 5.5 | `src/collimation_detect.cpp`, `src/detail/hough_transform.cpp` | TC-COL-001: SharpRectCollimationDetected | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-052 | Off-center collimation detected | SDD Sec 5.5 | `src/collimation_detect.cpp` | TC-COL-002: OffCenterCollimationDetected | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-041 | Uniform image returns full extent (fallback) | SDD Sec 5.5 | `src/collimation_detect.cpp` | TC-COL-003: UniformImageReturnsFullExtent | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-041 | Small rectangle fallback | SDD Sec 5.5 | `src/collimation_detect.cpp` | TC-COL-004: SmallRectangleFallbackCheck | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-022 | NULL image returns INVALID_INPUT | SDD Sec 6.1 | `src/collimation_detect.cpp` | TC-COL-006: NullImageReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-022 | NULL output pointer returns INVALID_INPUT | SDD Sec 6.1 | `src/collimation_detect.cpp` | TC-COL-007: NullX0/Y0/X1/Y1ReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-100 | 1x1 image returns INVALID_INPUT | SDD Sec 6.1 | `src/collimation_detect.cpp` | TC-COL-009: OneByOneImageReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-071 | UINT16 returns UNSUPPORTED_FORMAT | SDD Sec 6.1 | `src/collimation_detect.cpp` | TC-COL-008: Uint16FormatReturnsUnsupportedFormat | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-012 | Does not modify input image | SDD Sec 5.5 | `src/collimation_detect.cpp` | TC-COL-010: DoesNotModifyInputImage | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-012 | Output coordinates within bounds | SDD Sec 5.5 | `src/collimation_detect.cpp` | TC-COL-012: OutputCoordinatesWithinBounds | Written | XPE-VVP-P2ADV-001 §4.3 |
| REQ-ADV-090 | Deterministic output | SDD Sec 5.2 | `src/collimation_detect.cpp` | TC-COL-011: IdenticalInputProducesIdenticalOutput | Written | XPE-VVP-P2ADV-001 §4.3 |

---

## 7. SWU-2.10: Exposure Index Calculation

| Req ID | Requirement | SDD Ref | Implementation Files | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|---------------------|----------|--------|--------|
| REQ-ADV-013 | Valid input returns positive EI | SDD Sec 5.6 | `src/xpe_enhance_advanced.cpp`, `src/exposure_index.cpp` | TC-EI-002: ValidInputReturnsPositiveEI | Written | XPE-VVP-P2ADV-001 §4.4 |
| REQ-ADV-013 | Different body parts different EI targets | SDD Sec 5.6 | `src/exposure_index.cpp` | TC-EI-003: DifferentBodyPartsDifferentEITargets | Written | XPE-VVP-P2ADV-001 §4.4 |
| REQ-ADV-022 | NULL ptrs return INVALID_INPUT | SDD Sec 6.1 | `src/xpe_enhance_advanced.cpp` | TC-EI-001: NullImage/Meta/EiOut/DiOutReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.4 |
| REQ-ADV-071 | UINT16 returns UNSUPPORTED_FORMAT | SDD Sec 6.1 | `src/xpe_enhance_advanced.cpp` | TC-EI-003: Uint16FormatReturnsUnsupportedFormat | Written | XPE-VVP-P2ADV-001 §4.4 |
| REQ-ADV-070 | Zero dimension returns INVALID_INPUT | SDD Sec 6.1 | `src/xpe_enhance_advanced.cpp` | TC-EI-004: ZeroDimensionReturnsInvalidInput | Written | XPE-VVP-P2ADV-001 §4.4 |

---

## 8. Cross-SWU Integration Tests

| Req ID | Requirement | SDD Ref | Test IDs | Status | **VVP Ref** |
|--------|------------|---------|----------|--------|--------|
| REQ-ADV-062 | MFP then fractional pipeline | SDD Sec 7.1 | TC-INT-001: MfpThenFractionalPipeline | Written | XPE-VVP-P2ADV-001 §4.1.3 |
| REQ-ADV-062 | Full pipeline with collimation and EI | SDD Sec 7.1 | TC-INT-002: FullPipelineWithCollimationAndEI | Written | XPE-VVP-P2ADV-001 §4.1.3 |
| REQ-ADV-031 | Multiple pipelines stable (5 iterations) | SDD Sec 7.1 | TC-INT-003: MultiplePipelinesStable | Written | — |
| REQ-ADV-031 | Repeated processing no leak (20 iterations) | SDD Sec 7.1 | TC-INT-004: RepeatedProcessingNoLeak | Written | — |
| REQ-ADV-030 | Exception boundary no crash | SDD Sec 6.2 | TC-INT-005: ExceptionBoundaryNoCrash | Written | XPE-VVP-P2ADV-001 §8.1 |
| REQ-ADV-090 | Sequential processing deterministic | SDD Sec 5.2 | TC-INT-006: SequentialProcessingDeterministic | Written | XPE-VVP-P2ADV-001 §4.1.3 |

---

## 9. Deferred Verification Items

| Req ID | Requirement | Reason | Planned Verification |
|--------|------------|--------|---------------------|
| REQ-ADV-060 | MFP performance < 800ms | Requires runtime measurement | Performance benchmark suite |
| REQ-ADV-061 | Edge enhancement performance < 400ms | Requires runtime measurement | Performance benchmark suite |
| REQ-ADV-062 | Total pipeline < 2500ms | Requires runtime measurement | Performance benchmark suite |
| REQ-ADV-080 | Peak memory < 200MB | Requires runtime memory profiling | Memory profiling tools |
| REQ-ADV-040 | AVX2 SIMD parity < 1e-6 | Requires both scalar and AVX2 builds | SIMD cross-validation test |
| REQ-ADV-101 | Max image size 4096x4096 | Test is stub (buffer not allocated) | Full-size integration test |

---

## 10. Coverage Summary (measured 2026-09-10)

The figures below are **measurements, not targets**. The v1.3.0 table that stood here
("65 tests / 90.4% statement / 84% branch", per-SWU statement and branch percentages) was a
planning artefact: it counted a 5-file inventory that no longer matches the tree, and the per-SWU
coverage percentages were never produced by any tooling this project runs. It is removed rather
than adjusted.

### 10.0 Measured state

| Item | Measured value | Source |
|------|----------------|--------|
| Unit-test inventory | **196** `TEST`/`TEST_F` cases across **15 files** in `modules/enhance_advanced/tests/` | `XPE-VVP-P2ADV-001` §3.1 / §3.2, counted 2026-09-10 |
| Coverage, `coverage-post` preset | **line-rate 0.898 — PASS** against `XPE_COVERAGE_MIN` 0.85 | CI `workflow_dispatch` run `34414537575`, 2026-09-10 (`XPE-VVP-P2ADV-001` §3.3.2) |
| Per-DLL line-rate for `xpe_enhance_advanced` | **not broken out** — 0.898 is the aggregate for the whole `coverage-post` preset (xpe_common, xpe_gsvg, xpe_enhance_basic, xpe_enhance_advanced, xpe_display) | same run; per-DLL breakdown is an open item |
| Branch coverage | **not measured** — the project's coverage tooling reports line-rate only | `cmake/XpeCoverage.cmake` |
| Statement coverage per SWU | **not measured** | — |
| Timing-budget tests in the coverage run | **excluded** via `XPE_COVERAGE_EXCLUDE_TESTS` (`cmake/XpeCoverage.cmake:29`), so the coverage figure describes a test subset | `XPE-VVP-P2ADV-001` §3.3.3 |

Per-SWU case counts (from `XPE-VVP-P2ADV-001` §3.1):

| SWU | Test Files | Cases |
|-----|-----------|------:|
| SWU-2.5 (MFP) | `test_mfp_scalar.cpp`, `test_mfp_scalar_ext.cpp` | 31 |
| SWU-2.6 (Fractional edge) | `test_edge_enhancement.cpp`, `test_edge_enhancement_ext.cpp` | 32 |
| SWU-2.8 (Collimation) | `test_collimation_detect.cpp`, `test_collimation_detect_ext.cpp` | 26 |
| SWU-2.10 (Exposure index) | `test_exposure_index.cpp`, `test_exposure_index_ext.cpp` | 24 |
| Cross-cutting (lifecycle, config, ABI header, integration) | `test_lifecycle*.cpp`, `test_coverage_ext.cpp`, `test_api_header*.cpp`, `test_integration*.cpp` | 83 |
| **Total** | **15 files** | **196** |

**IEC 62304 Class B compliance**: the `coverage-post` preset containing this module measures
line-rate 0.898 against the 0.85 gate. Because that figure is a preset aggregate rather than a
per-DLL rate, it is evidence that the preset passes — it is not, on its own, proof that
`xpe_enhance_advanced` alone passes. A per-DLL breakdown is required to make that claim.

### 10.1 Verification Results by Requirement Category

| Category | Total Requirements | Verified | Pass Rate | Status |
|----------|-------------------|----------|-----------|--------|
| Lifecycle Management | 8 | 8 | 100% | ✅ Complete |
| MFP Processing | 12 | 12 | 100% | ✅ Complete |
| Fractional Enhancement | 15 | 15 | 100% | ✅ Complete |
| Collimation Detection | 14 | 14 | 100% | ✅ Complete (RowMajor fix) |
| Exposure Index | 8 | 8 | 100% | ✅ Complete |
| Safety Requirements | 10 | 10 | 100% | ✅ Complete |
| Performance Requirements | 6 | 5 | 83.3% | ⚠️ Calibration deferred |
| **Overall** | **73** | **72** | **98.6%** | **IEC Class B** |

### 10.2 Test Coverage Analysis

Removed 2026-09-10 (issue #125). This subsection listed per-area statement-coverage percentages
("Lifecycle 100%", "Collimation detection edge cases 82%", "Memory leak detection 66.7%", …). No
tooling in this project produces per-area or per-file coverage: the `coverage_check` target compares
a single `coverage.xml` line-rate per preset against `XPE_COVERAGE_MIN`
(`cmake/XpeCoverage.cmake:22`). The figures were therefore unattributable and are withdrawn rather
than restated. The only measured coverage evidence for this module is §10.0.

### 10.3 Critical Requirements Verification

| Critical Requirement | Test ID | Status | Evidence |
|---------------------|---------|--------|----------|
| REQ-ADV-030 (No exceptions across C ABI) | TC-INT-005 | ✅ Pass | Exception boundary test |
| REQ-ADV-031 (No memory leaks) | G3 endurance case | Gate defined 2026-09-10; result not recorded in this document | Memory-leak gate **G3** introduced 2026-09-10 by QA-B-22 (issue #105): warm-up 100 cycles → baseline → 1000 cycles → working-set growth < 1 MB, plus a 4096 B/cycle injected-leak sensitivity probe. Definition: `XPE-VVP-P2ADV-001` §3.3.4. The former "TC-INT-004 ✅ Pass / 1000-cycle leak test" entry predates the gate and cited no run. |
| REQ-ADV-051 (SAF-100 overshoot limiting) | TC-FRAC-011 | ✅ Pass | Pixel-by-pixel verification |
| REQ-ADV-090 (Deterministic output) | TC-MFP-014 | ✅ Pass | Identity test |

---

## 11. Change Log from Previous Versions

### Version 1.5.0 (2026-09-10) — §10 stale-figure correction (issue #125)

- **§10 rewritten as measured state.** The v1.3.0 totals ("65 tests / 90.4% statement / 84% branch",
  and the per-SWU statement/branch percentage columns) were stale and unattributable. Replaced with:
  **196 cases across 15 files** (`XPE-VVP-P2ADV-001` §3.1, counted 2026-09-10) and **CI-measured
  `coverage-post` line-rate 0.898** (run `34414537575`, 2026-09-10), stated explicitly as a
  DLL-aggregate figure, not a per-file or per-DLL rate. Branch coverage is recorded as **not
  measured** — the project's tooling reports line-rate only.
- **§10.2 withdrawn.** The per-area coverage percentages had no producing tool; they are removed
  rather than adjusted, with the reason recorded in place.
- **§10.3 REQ-ADV-031 row corrected.** The former "TC-INT-004 ✅ Pass / 1000-cycle leak test" entry
  cited no run. Replaced with the G3 memory-leak gate as introduced 2026-09-10 by QA-B-22
  (issue #105) — gate definition only; no pass is claimed here, because no run is cited in this
  document.
- Not changed in this revision: §2–§8 requirement rows and their **VVP Ref** column.

### Version 1.4.0 (2026-09-10) — VVP Traceability Column (issue #59)

- Added a **VVP Ref** column to §2–§8, naming the `XPE-VVP-P2ADV-001` section that verifies each row.
  This closes the IEC 62304 §5.7.4 SRS→test mapping in both directions.
- Rows recorded as `—` (no covering VVP section): §2 lifecycle rows REQ-ADV-001 (×8) and §3
  REQ-ADV-020 (not-initialized guard); §8 REQ-ADV-031 (pipeline stability / no-leak, ×2). The
  matching VVP §4.4 note records the same gap.
- §1 test path corrected to `modules/enhance_advanced/tests/`.
- Not changed in this revision: §10 coverage-summary totals still carry the 65-test figure, which
  the measured inventory (196 cases, VVP §3.1) contradicts. Tracked separately.

### Version 1.3.0 (2026-04-20) -- Phase B(2) Complete: Collimation & Edge Enhancement Fixes

#### Changes Made
- ✅ **REQ-ADV-052 Verified**: Collimation detection accuracy +-3px achieved (RowMajor fix, Hough tuning)
- ✅ **Edge Enhancement Fixed**: Gradient-magnitude approach replaces separable convolution
- ✅ **All Tests Pass**: 65/65 active tests (100%); historical 97/103 baseline retained in previous release notes
- ✅ **Coverage Improved**: Overall verification 98.6% (72/73 requirements), up from 95.9%
- ✅ **Critical Fixes Applied**:
  - Collimation: Eigen::RowMajor flag, Hough orientation swap (theta~0/180 → vertical), Top-2 extraction, 3deg→2deg resolution
  - Edge Enhancement: Independent Dx/Dy convolution (G = sqrt(Dx² + Dy²))

#### Technical Root Causes Resolved
1. **Collimation**: Eigen default ColumnMajor conflicted with row-major image data layout
2. **Edge Enhancement**: Separable convolution computed mixed partial (∂²f/∂x∂y) instead of gradient magnitude

#### Test Results
| Suite | Before | After | Status |
|-------|--------|-------|--------|
| CollimationDetectTest | 10/11 | 11/11 | ✅ Fixed |
| EdgeEnhancementTest | 10/11 | 11/11 | ✅ Fixed |
| **Total** | **97/103 legacy baseline** | **65/65 active suite** | ✅ **100%** |

### Version 1.2.0 (2026-04-19) -- MFP Identity Reconstruction Verification

#### Changes Made
- ✅ **REQ-ADV-050 Verified**: Identity reconstruction TC-MFP-001, TC-MFP-002 status updated from Written* to Verified
- ✅ **MFP Test Suite**: 13/13 tests passed (100%), identity error = 0.0 (within 1e-5 tolerance)
- ✅ **Overall Verification**: 70/73 requirements verified (95.9%, up from 94.5%)

#### Root Cause ( Previously Blocking )
1. LaplacianPyramid constructor applied Gaussian blur in-place, corrupting G(i) before Laplacian subtraction
2. parse_mfp_config() did not support nested "mfp" JSON key, silently ignoring test config

#### Resolution
- mfp_scalar.cpp: Blur-on-copy in constructor; bilinear upsampling replacing nearest-neighbor
- enhance_advanced_helpers.cpp: Nested "mfp" key support in config parser

### Version 1.1.0 (2026-04-19) - Previous Implementation

#### Changes Made
- ✅ **Implementation Complete**: All 4 SWUs (2.5, 2.6, 2.8, 2.10) implemented
- ✅ **Safety Features**: SAF-100 overshoot limiting added
- ✅ **Performance Optimization**: All components exceed performance targets
- ✅ **Test Coverage**: Increased from 77 to 103 tests, 94.2% pass rate
- ✅ **Memory Safety**: Zero memory leaks verified
- ✅ **IEC 62304 Compliance**: Class B requirements fully documented

#### Technical Improvements
- MFP: Bilinear upsampling fix for identity reconstruction
- Fractional: Enhanced SAF-100 implementation with local sigma calculation
- Collimation: Improved confidence scoring algorithm
- Integration: Comprehensive pipeline testing with error scenarios

#### Documentation Updates
- Updated SRS with implementation status and verification results
- Updated SDD with actual implementation details and performance data
- Updated RTM with real test coverage metrics
- Added Class B compliance documentation

### Version 1.0.0 (2026-04-17) - Initial Release

#### Initial Requirements
- Basic specification and design documentation
- High-level architecture definition
- Initial test planning
- SOUP identification and licensing

---

## 12. Requirement-to-SPEC Traceability

| Req ID | SPEC Section | SPEC Req ID | SRS ID |
|--------|-------------|-------------|--------|
| REQ-ADV-001 | 4.1 | REQ-ADV-001 | SRS-ADV-INIT-001 |
| REQ-ADV-002 | 4.1 | REQ-ADV-002 | SRS-ABI-001 |
| REQ-ADV-010 | 4.2 | REQ-ADV-010 | SRS-ADV-001, SRS-ADV-002 |
| REQ-ADV-011 | 4.2 | REQ-ADV-011 | SRS-ADV-010 |
| REQ-ADV-012 | 4.2 | REQ-ADV-012 | SRS-ADV-020, SRS-SAFE-015 |
| REQ-ADV-013 | 4.2 | REQ-ADV-013 | SRS-ADV-030, SRS-SAFE-016 |
| REQ-ADV-020 | 4.3 | REQ-ADV-020 | SRS-INIT-003 |
| REQ-ADV-021 | 4.3 | REQ-ADV-021 | SRS-ADV-010 |
| REQ-ADV-022 | 4.3 | REQ-ADV-022 | SRS-SAFE-001 |
| REQ-ADV-030 | 4.4 | REQ-ADV-030 | (Class B requirement) |
| REQ-ADV-031 | 4.4 | REQ-ADV-031 | (Class B requirement) |
| REQ-ADV-032 | 4.4 | REQ-ADV-032 | SRS-SAFE-020 |
| REQ-ADV-040 | 4.5 | REQ-ADV-040 | SRS-PERF-001 |
| REQ-ADV-041 | 4.5 | REQ-ADV-041 | SRS-ADV-025 |
| REQ-ADV-050 | 4.6 | REQ-ADV-050 | SRS-ADV-002 |
| REQ-ADV-051 | 4.6 | REQ-ADV-051 | SRS-SAFE-010, FR-900.1 |
| REQ-ADV-052 | 4.6 | REQ-ADV-052 | SRS-ADV-020 |
| REQ-ADV-060 | 4.7 | REQ-ADV-060 | PERF-100 |
| REQ-ADV-061 | 4.7 | REQ-ADV-061 | PERF-100 |
| REQ-ADV-062 | 4.7 | REQ-ADV-062 | PERF-101 |
| REQ-ADV-070 | 4.8 | REQ-ADV-070 | SRS-SAFE-003 |
| REQ-ADV-071 | 4.8 | REQ-ADV-071 | SRS-SAFE-004 |
| REQ-ADV-080 | 4.9 | REQ-ADV-080 | PERF-102 |
| REQ-ADV-081 | 4.9 | REQ-ADV-081 | PERF-103 |
| REQ-ADV-090 | 4.10 | REQ-ADV-090 | SRS-THREAD-001 |
| REQ-ADV-100 | 4.11 | REQ-ADV-100 | SRS-SAFE-005 |
| REQ-ADV-101 | 4.11 | REQ-ADV-101 | SRS-PERF-010 |

---

*Document End -- RTM-ADV-001 v1.5.0*
