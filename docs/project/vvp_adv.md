# Validation and Verification Plan for xpe_enhance_advanced.dll

**Document ID**: XPE-VVP-P2ADV-001  
**Version**: 1.1.0  
**Date**: 2026-09-10  
**Status**: Controlled Draft  
**Classification**: Internal / IEC 62304 Compliance  
**Safety Classification**: IEC 62304 Class B  
**Module**: xpe_enhance_advanced.dll  
**Parent Specification**: SPEC-XPE-P2-ADV v1.0.0  
**Related Documents**:
  - Software Requirements Specification (SRS): `docs/project/srs_adv.md`
  - System V&V Plan: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md`
  - Architecture Reference: `docs/project/tech.md`
  - Requirements Traceability Matrix (RTM): `docs/project/rtm_adv.md` (RTM-ADV-001)
  - Parent V&V Plan: `docs/post-processing/xpe/XPE-VVP-001_Verification_Validation_Plan.md`

---

## 1. Introduction

### 1.1 Purpose

This document defines the verification and validation (V&V) strategy for `xpe_enhance_advanced.dll`, the XPE Advanced Post-Processing Module (Phase 2, Layer 1). The V&V plan ensures that:

1. All software requirements are correctly implemented (verification)
2. The system meets intended clinical use cases and performance objectives (validation)
3. IEC 62304 Class B software lifecycle compliance is maintained
4. Quality and safety expectations align with the XPE system baseline

### 1.2 Scope

This plan covers:

- **Module**: xpe_enhance_advanced.dll (native C++ DLL)
- **Software Units**: SWU-2.5 (Multiscale Frequency Processing), SWU-2.6 (Fractional-Order Edge Enhancement), SWU-2.8 (Collimation ROI Detection), SWU-2.10 (Exposure Index Refinement)
- **API Functions**: 4 primary processing entry points plus lifecycle/config API
- **Unit Test Inventory**: 196 `TEST`/`TEST_F` cases across 15 files in `modules/enhance_advanced/tests/` (counted 2026-09-10; see §3.1)
- **Test Coverage Target**: `XPE_COVERAGE_MIN` = 0.85 line-rate per DLL (`cmake/XpeCoverage.cmake:22`, REQ-P0-006)
- **Classification**: IEC 62304 Class B medical device software

### 1.3 Referenced Documents

| Document ID | Title | Version | Status |
|-------------|-------|---------|--------|
| SPEC-XPE-P2-ADV | Advanced Post-Processing Module Specification | 1.0.0 | Approved |
| SRS-ADV-001 | Software Requirements Specification (Advanced) | 1.2.0 | Released |
| XPE-SVVP-001 | System Verification and Validation Plan | 1.4.0 | Controlled Draft |
| XPE-API-SPEC-001 | XPE API Specification | 1.3.0 | Approved |
| IEC 62304:2006 | Medical Device Software Lifecycle Processes | +A1:2015 | Normative |
| IEC 62494-1:2008 | Exposure Index Standard | 2008 | Normative |
| ALG-SPEC-001 | Algorithm Specification (Advanced Algorithms) | 3.0.0 | Approved |

---

## 2. V&V Strategy

### 2.1 Six-Level Verification and Validation Hierarchy

The module is verified and validated across six complementary levels, aligned with XPE-SVVP-001 framework:

| Level | Scope | Primary Evidence | Responsibility |
|-------|-------|------------------|-----------------|
| **L1** | Unit Verification | Unit tests (196 cases, §3.1), statement/branch coverage, static analysis, scalar-to-SIMD parity | Developer + QA |
| **L2** | Integration Verification | API contract tests, P/Invoke marshalling compatibility, binary loading, dependency validation | Integration QA |
| **L3** | System Verification | End-to-end pipeline tests, benchmark processing, performance measurement, error recovery | System QA |
| **L4** | Feature Verification | Algorithm validation (MFP accuracy, fractional derivative validation, ROI detection precision) | Algorithm QA |
| **L5** | Validation (Clinical) | Clinical benchmark evidence, exposure index accuracy per IEC 62494-1, diagnostic usability | Clinical Review |
| **L6** | Field Performance | Long-term DI drift analysis, reject-analysis telemetry, maintenance evidence | Field QA |

### 2.2 V&V Principles

1. **Detector-domain measurement first**: All accuracy metrics (MFP, fractional edge, ROI bounds) are measured before presentation LUT application.
2. **Algorithm-specific evidence**: Non-linear algorithms (MFP, fractional derivative) require task-based or observer evidence in addition to scalar metrics.
3. **Graceful degradation**: Module functions correctly in absence mode (disabled features bypass gracefully).
4. **Determinism validation**: Processing of identical images produces identical output (required for calibration workflows).
5. **Benchmark integrity**: Frozen benchmark manifests and hashes prevent accidental test data corruption.

### 2.3 IEC 62304 Clause Mapping of the Six-Level Hierarchy

The six V&V levels of §2.1 are a project-internal decomposition. This subsection maps them onto the
normative IEC 62304:2006+A1:2015 clauses so that each level has an identified regulatory home.
Class B requires 5.5, 5.6 and 5.7 in full; 5.8 (release) and 6.x (maintenance) are referenced for the
post-release levels.

| Level (§2.1) | IEC 62304 Clause | Clause Title | Evidence Produced by This Plan |
|---|---|---|---|
| **L1** Unit Verification | **5.5.2 / 5.5.3 / 5.5.5** | Software unit verification process, acceptance criteria, unit verification | §3.1 unit-test inventory, §3.3 coverage metrics, §4.x.1 unit-test suites |
| **L2** Integration Verification | **5.6.1 / 5.6.2 / 5.6.3** | Integrate software units, verify integration, test integrated software | §4.1.3 integration tests, P/Invoke ABI and dependency checks |
| **L3** System Verification | **5.7.1 / 5.7.4 / 5.7.5** | Establish tests for software requirements, verify test procedures, test record contents | §4.1.3 pipeline tests, §7 performance validation, §8 error handling |
| **L4** Feature Verification | **5.7.1** (algorithm-level system test) | Establish tests for software requirements | §4.x.2 algorithm-validation tables |
| **L5** Validation (Clinical) | **5.7.1 + 5.8.x** | System testing feeding software release | §4.x.4 benchmark evidence, §5.3 clinical validation gate |
| **L6** Field Performance | **6.1 / 6.2 (maintenance), 5.6.4 (regression)** | Software maintenance process, regression testing | §11 post-release validation |

Cross-cutting clauses applied at every level: **5.5.4** (unit acceptance criteria applied before
integration), **5.6.5** (test record contents), **5.6.6 / 5.7.2** (problem resolution via
`XPE-SPR-001`), **5.6.7** (test-procedure verification), **5.7.3** (retest after change), and
**7.3.3** (risk-control verification traceability, recorded in `docs/project/rtm_adv.md`).

---

## 3. Test Coverage Requirements

### 3.1 Unit Test Inventory

Counts below are the actual `TEST` / `TEST_F` macro counts in `modules/enhance_advanced/tests/`,
enumerated on 2026-09-10. They supersede the planning estimates carried in v1.0.0.

| SWU | Function(s) | Purpose | Test Files | Test Count | Coverage Target |
|-----|-----------|---------|------------|------------|-----------------|
| SWU-2.5 | `xpe_mfp_process` | Multiscale frequency processing | `test_mfp_scalar.cpp`, `test_mfp_scalar_ext.cpp` | 31 | 85%+ |
| SWU-2.6 | `xpe_edge_enhancement` (fractional-order) | Fractional-order edge enhancement | `test_edge_enhancement.cpp`, `test_edge_enhancement_ext.cpp` | 32 | 85%+ |
| SWU-2.8 | `xpe_collimation_detect` | Collimation ROI detection | `test_collimation_detect.cpp`, `test_collimation_detect_ext.cpp` | 26 | 85%+ |
| SWU-2.10 | `xpe_calc_exposure_index` (ROI variant) | EI calculation with ROI refinement | `test_exposure_index.cpp`, `test_exposure_index_ext.cpp` | 24 | 85%+ |
| **Cross-cutting** | Lifecycle, config, ABI header, integration | Multi-function validation | `test_lifecycle*.cpp`, `test_coverage_ext.cpp`, `test_api_header*.cpp`, `test_integration*.cpp` | 83 | 85%+ |
| **Total** | — | — | **15 files** | **196 cases** | **85%+** |

### 3.2 Test Organization

Actual file inventory with per-file case counts (15 files, 196 cases):

```
modules/enhance_advanced/tests/
  test_mfp_scalar.cpp                 -- SWU-2.5   (13)  MfpScalarTest
  test_mfp_scalar_ext.cpp             -- SWU-2.5   (18)  MfpScalarExtTest
  test_edge_enhancement.cpp           -- SWU-2.6   (11)  EdgeEnhancementTest
  test_edge_enhancement_ext.cpp       -- SWU-2.6   (21)  EdgeEnhancementExtTest
  test_collimation_detect.cpp         -- SWU-2.8   (12)  CollimationDetectTest
  test_collimation_detect_ext.cpp     -- SWU-2.8   (14)  CollimationDetectExtTest
  test_exposure_index.cpp             -- SWU-2.10  (9)   ExposureIndexTest
  test_exposure_index_ext.cpp         -- SWU-2.10  (15)  ExposureIndexExtTest
  test_lifecycle.cpp                  -- X-cutting (8)   EnhanceAdvancedLifecycleTest
  test_lifecycle_ext.cpp              -- X-cutting (15)  LifecycleTest, NotInitializedGuardTest
  test_coverage_ext.cpp               -- X-cutting (23)  EnhanceAdvancedConfigTest
  test_api_header.cpp                 -- X-cutting (3)   EnhanceAdvancedApiHeaderTest
  test_api_header_ext.cpp             -- X-cutting (12)  ApiHeaderTest
  test_integration.cpp                -- X-cutting (12)  IntegrationTest
  test_integration_ext.cpp            -- X-cutting (10)  IntegrationPipelineTest, ErrorPrecedenceTest

Total: 196 TEST/TEST_F cases
```

The `*_ext.cpp` files are coverage-extension suites added after the initial release; they exercise
the same SWU through additional boundary, error-precedence and configuration paths.

### 3.3 Coverage Metrics

#### 3.3.1 Targets

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Line Coverage (gate) | >= 85% | `XPE_COVERAGE_MIN` = 0.85 per DLL, checked by the `coverage_check` target (`cmake/XpeCoverage.cmake:22`, REQ-P0-006) |
| Statement Coverage | >= 85% | gcov/llvm-cov per function |
| Branch Coverage | >= 80% | gcov/llvm-cov per decision point |
| Loop Coverage | >= 80% | Minimum 2x loop iterations in tests |
| Error Path Coverage | 100% | All error return codes exercised |
| Boundary Conditions | 100% | Min/max pixel values, image dimensions, parameters |

#### 3.3.2 Measured Coverage State (2026-09-10)

The following are measurements, not targets. Source: CI `workflow_dispatch` run `34414537575`,
executed 2026-09-10.

| Preset | DLLs in scope | Measured line-rate | Gate result vs. 0.85 |
|--------|---------------|--------------------|----------------------|
| `coverage` | xpe_common, xpe_preprocess | 0.649 | **FAIL** |
| `coverage-post` | xpe_common, xpe_gsvg, xpe_enhance_basic, **xpe_enhance_advanced**, xpe_display | 0.898 | **PASS** |

`xpe_enhance_advanced` is measured inside the `coverage-post` preset only; no per-DLL line-rate is
broken out by the current tooling, so the 0.898 figure is the aggregate for that preset and is the
only measured coverage evidence available for this module. A per-DLL breakdown remains an open item.

#### 3.3.3 Timing-Budget Test Exclusion

Coverage runs deliberately exclude timing-budget tests, because wall-clock assertions are not
reproducible under an instrumented build. The exclusion is a ctest `-E` regex
(`cmake/XpeCoverage.cmake:29`, `XPE_COVERAGE_EXCLUDE_TESTS`):

```
Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance
```

Consequence for this plan: the performance budgets of §7 are **not** verified by the coverage run
and must be verified by a separate non-instrumented Release execution. Coverage figures in §3.3.2
therefore describe a test subset, not the full suite.

#### 3.3.4 Memory-Leak Gate (G3)

Gate G3 (issue #105, closed 2026-09-10) applies to all 7 modules, `xpe_enhance_advanced` included:

| Step | Definition |
|------|------------|
| Warm-up | 100 process cycles executed and discarded |
| Baseline | Working-set size sampled after warm-up |
| Measurement | 1000 further process cycles |
| Pass criterion | Working-set growth over the 1000 cycles < 1 MB |
| Sensitivity probe | A deliberate 4096 B/cycle leak is injected and must be detected by the same harness |

AddressSanitizer (`/fsanitize=address`) is used as an **ad-hoc verification method** in QA scratch
builds to localise leaks found by G3. It is not a CMake build option of this project and is not part
of the standard build configuration.

---

## 4. Verification Methods per SWU

### 4.1 SWU-2.5: Multiscale Frequency Processing (MFP)

**Purpose**: Enhance diagnostic visibility via multiscale decomposition and adaptive recombination.

**Requirements Addressed**: SRS-ADV-MFP-001..008, SPEC REQ-ADV-MFP-001..006

#### Verification Method V4.1.1: Unit Test Suite

**Test Category**: L1 Unit Verification

| Test ID | Test Case | Assertion | Acceptance |
|---------|-----------|-----------|-----------|
| TC-MFP-001 | Gaussian pyramid decomposition | Pyramid levels match OpenCV reference | Output within 0.1% of reference |
| TC-MFP-002 | Level reconstruction | Reconstructed image = original (within numeric error) | Relative error < 1e-4 |
| TC-MFP-003 | Contrast enhancement per level | Enhancement amplitude >= 20% | Verified per visual inspection |
| TC-MFP-004 | Adaptive weighting | Weights apply correctly per frequency band | Weight values within [0, 1] |
| TC-MFP-005 | Parameter range validation | Invalid params (negative scale, zero bands) rejected | Returns XPE_ERR_INVALID_INPUT |
| TC-MFP-006 | Null pointer handling | NULL image, NULL params rejected | Returns XPE_ERR_INVALID_INPUT |
| TC-MFP-007 | Format validation | Non-float32 image rejected | Returns XPE_ERR_UNSUPPORTED_FORMAT |
| TC-MFP-008 | Boundary image sizes | 1x1, 64x64, 3072x3072 processed correctly | No segfault, valid output |

#### Verification Method V4.1.2: Algorithm Validation

**Test Category**: L4 Feature Verification

| Test ID | Test Scenario | Evidence Type | Acceptance Criteria |
|---------|-------------|---|---|
| TC-MFP-ALG-001 | Multiscale contrast recovery | Benchmark phantom image | Local contrast improvement >= 25% |
| TC-MFP-ALG-002 | Edge preservation at each scale | Gradient magnitude per Gaussian level | Edges preserved within 90% gradient magnitude |
| TC-MFP-ALG-003 | Noise suppression balance | SNR measurement on synthetic noisy image | SNR improvement >= 5 dB without over-smoothing |
| TC-MFP-ALG-004 | Frequency band separation | Spectral analysis of decomposed bands | Negligible overlap between adjacent levels |
| TC-MFP-ALG-005 | Robustness to extreme values | Min/max pixel test images | No NaN or Inf propagation |

#### Verification Method V4.1.3: Integration Test

**Test Category**: L3 System Verification

| Test ID | Scenario | Evidence | Acceptance |
|---------|----------|----------|-----------|
| TC-MFP-INT-001 | Pipeline integration | xpe_enhance_basic → MFP → xpe_display | No API errors, deterministic output |
| TC-MFP-INT-002 | Concurrent processing | Multiple threads on independent images | Identical results to serial execution |
| TC-MFP-INT-003 | Performance budget | 3072x3072 processing time | <= 80ms per SPEC budget |

#### Verification Method V4.1.4: Benchmark Processing

**Test Category**: L5 Validation

| Benchmark Set | Image Type | Purpose | Pass Criterion |
|---|---|---|---|
| BP-06-MFP | Standard radiograph (3072x3072) | Reference output baseline | Output matches frozen manifest hash |
| BP-06-MFP-noisy | Synthetic noisy image | Noise resilience | SNR improvement verified |

#### RTM Cross-Reference (SWU-2.5)

Traced in `docs/project/rtm_adv.md` (RTM-ADV-001) **§4 SWU-2.5: Multiscale Frequency Processing**.
Requirement rows covered by this section: REQ-ADV-010, REQ-ADV-050, REQ-ADV-032, REQ-ADV-022,
REQ-ADV-070, REQ-ADV-100, REQ-ADV-071, REQ-ADV-090, REQ-ADV-031. Those rows carry
`XPE-VVP-P2ADV-001 §4.1` in their **VVP Ref** column; the link is bidirectional and any change to
this section requires the matching RTM rows to be re-checked.

---

### 4.2 SWU-2.6: Fractional-Order Edge Enhancement

**Purpose**: Enhance edges via fractional-order derivatives for diagnostic detail visibility.

**Requirements Addressed**: SRS-ADV-FDE-001..006, SPEC REQ-ADV-FDE-001..005

#### Verification Method V4.2.1: Unit Test Suite

**Test Category**: L1 Unit Verification

| Test ID | Test Case | Assertion | Acceptance |
|---------|-----------|-----------|-----------|
| TC-FDE-001 | Fractional order validation | Order in [0.5, 2.0] accepted, outside rejected | Boundary order values processed correctly |
| TC-FDE-002 | Derivative computation | Fractional derivative matches reference implementation | Error < 1% vs. analytical fractional differential |
| TC-FDE-003 | Edge detection accuracy | Strong edges detected, weak noise-like edges rejected | Precision >= 90% on synthetic test image |
| TC-FDE-004 | Threshold application | Pixels below magnitude threshold unmodified | Zero gain below threshold confirmed |
| TC-FDE-005 | Gain amplitude bounds | Gain clamped to [0, gain_max] | No pixel exceeds clamp value |
| TC-FDE-006 | Parameter validation | Invalid orders, negative gain rejected | Error codes returned correctly |
| TC-FDE-007 | Memory safety | Large images processed without overflow | No heap corruption detected |
| TC-FDE-008 | Determinism | Identical images produce identical output | Byte-for-byte reproducibility verified |

#### Verification Method V4.2.2: Algorithm Validation

**Test Category**: L4 Feature Verification

| Test ID | Scenario | Evidence | Acceptance Criteria |
|---------|----------|----------|---|
| TC-FDE-ALG-001 | Fractional order vs. integer | Compare order=1.0 (1st derivative) vs. standard edge detection | Order 1.0 matches Sobel edge detector within 5% |
| TC-FDE-ALG-002 | Fractional order clinical value | Observer assessment of fractional vs. integer | Fractional shows improved diagnostic detail |
| TC-FDE-ALG-003 | Artifact prevention | Halo/ringing detection on synthetic sharp edges | No visible artifacts per visual inspection |
| TC-FDE-ALG-004 | Noise immunity | Denoised image enhancement (SNR >= 25 dB) | Edge detection stable across noise levels |

#### Verification Method V4.2.3: Benchmark Evidence

**Test Category**: L5 Validation

| Benchmark Set | Purpose | Pass Criterion |
|---|---|---|
| BP-06-FDE | Fractional edge enhancement | Output matches frozen manifest |
| BP-06-FDE-artifact | Synthetic edge (sharp transition) | No halo/ringing artifacts detected |

#### RTM Cross-Reference (SWU-2.6)

Traced in `docs/project/rtm_adv.md` **§5 SWU-2.6: Fractional-Order Edge Enhancement**.
Requirement rows covered: REQ-ADV-011, REQ-ADV-021, REQ-ADV-051, REQ-ADV-032, REQ-ADV-022,
REQ-ADV-100, REQ-ADV-071, REQ-ADV-090. Those rows carry `XPE-VVP-P2ADV-001 §4.2` in their
**VVP Ref** column.

---

### 4.3 SWU-2.8: Collimation ROI Detection

**Purpose**: Automatically detect collimation boundaries and refine exposure index calculation to ROI area.

**Requirements Addressed**: SRS-ADV-ROI-001..008, SPEC REQ-ADV-ROI-001..007

#### Verification Method V4.3.1: Unit Test Suite

**Test Category**: L1 Unit Verification

| Test ID | Test Case | Assertion | Acceptance |
|---------|-----------|-----------|-----------|
| TC-ROI-001 | ROI boundary detection | Detects 4-edge collimation correctly | Boundary precision within 2 pixels |
| TC-ROI-002 | Edge refinement | Gradient-based edge localization | Edges localized to pixel-level accuracy |
| TC-ROI-003 | ROI validation | ROI area is reasonable (>= 10% of image) | No spurious ROIs accepted |
| TC-ROI-004 | Partial collimation | Detects L/R/T/B edges present, others absent | Correctly handles edge-aligned collimation |
| TC-ROI-005 | No collimation case | Full-frame image returns full bounds | Graceful handling of borderless image |
| TC-ROI-006 | Parameter validation | Sensitivity threshold validation | Invalid sensitivity rejected |
| TC-ROI-007 | Null pointer handling | NULL image, NULL bounds rejected | Error codes returned |
| TC-ROI-008 | Edge cases | 1x1, 2x2 images processed | No segfault on minimal dimensions |

#### Verification Method V4.3.2: Algorithm Validation

**Test Category**: L4 Feature Verification

| Test ID | Scenario | Evidence | Acceptance Criteria |
|---------|----------|----------|---|
| TC-ROI-ALG-001 | Standard collimator | Tapered X-ray collimator (circular vignette) | Detection accuracy >= 95% |
| TC-ROI-ALG-002 | Rectangular field | Square collimator frame | All 4 edges detected within 1 pixel |
| TC-ROI-ALG-003 | Noise robustness | Noisy detector data (SNR=20 dB) | ROI bounds stable (variance < 1 pixel) |
| TC-ROI-ALG-004 | Collimation types | Multiple collimator shapes (circular, rectangular, slits) | Correctly identifies shape-specific boundaries |
| TC-ROI-ALG-005 | Penumbra handling | Soft collimation edge (gradient transition) | Locates 50% penumbra point consistently |

#### Verification Method V4.3.3: Benchmark Evidence

**Test Category**: L5 Validation

| Benchmark Set | Purpose | Pass Criterion |
|---|---|---|
| BP-06-ROI | Standard collimator image | ROI bounds match reference within 2 pixels |
| BP-06-ROI-edge | Marginal collimation (edge-aligned) | Correctly identifies which edges present |

#### RTM Cross-Reference (SWU-2.8)

Traced in `docs/project/rtm_adv.md` **§6 SWU-2.8: Collimation ROI Detection**.
Requirement rows covered: REQ-ADV-052, REQ-ADV-041, REQ-ADV-022, REQ-ADV-100, REQ-ADV-071,
REQ-ADV-012, REQ-ADV-090. Those rows carry `XPE-VVP-P2ADV-001 §4.3` in their **VVP Ref** column.

---

### 4.4 SWU-2.10: Exposure Index Refinement (ROI Variant)

**Purpose**: Refine EI/DI calculation to collimation ROI for improved exposure accuracy.

**Requirements Addressed**: SRS-ADV-EI-001..005, SPEC REQ-ADV-EI-001..004

#### Verification Method V4.4.1: Unit Test Suite

**Test Category**: L1 Unit Verification

| Test ID | Test Case | Assertion | Acceptance |
|---------|-----------|-----------|-----------|
| TC-EI-ROI-001 | ROI mean signal | Mean computed from ROI area only | Verification against numpy/OpenCV mean |
| TC-EI-ROI-002 | EI calculation | `EI = EIT * (mean_roi / S0_ref)` | Matches IEC 62494-1 formula |
| TC-EI-ROI-003 | DI calculation | `DI = 10 * log10(EI / EIT)` | Matches IEC 62494-1 formula |
| TC-EI-ROI-004 | ROI vs. whole-image | ROI EI differs from whole-image EI | Difference correlates with collimation area |
| TC-EI-ROI-005 | Parameter validation | Invalid ROI bounds rejected | Returns XPE_ERR_INVALID_INPUT |
| TC-EI-ROI-006 | Out-of-bounds ROI | ROI clipped to image bounds | Processing continues without error |
| TC-EI-ROI-007 | Empty ROI | ROI area = 0 returns error | XPE_ERR_INVALID_INPUT returned |
| TC-EI-ROI-008 | Warning alert on DI | DI outside [-3, +3] posts warning | Alert queue receives DI_DEVIATION alert |

#### Verification Method V4.4.2: Algorithm Validation

**Test Category**: L4 Feature Verification

| Test ID | Scenario | Evidence | Acceptance Criteria |
|---------|----------|----------|---|
| TC-EI-ALG-001 | IEC 62494-1 compliance | Reference phantom at multiple exposures | EI/DI within 0.1% of reference values |
| TC-EI-ALG-002 | Exposure accuracy post-ROI | Clinical phantom image with collimation | DI improvement >= 20% over whole-image EI |
| TC-EI-ALG-003 | EIT selection | Multiple body parts per metadata | Correct EIT lookup verified per DICOM tag |
| TC-EI-ALG-004 | Calibration stability | Repeated processing of same image | EI variance < 0.5% across runs |

#### Verification Method V4.4.3: Benchmark Evidence

**Test Category**: L5 Validation

| Benchmark Set | Purpose | Pass Criterion |
|---|---|---|
| BP-07 (Phase 2) | Standard phantom with collimation | EI/DI match reference within 0.1% |
| BP-08 (EI baseline) | Full range exposure tests | DI within [-3, +3] for valid exposures |
| BP-09 (EI edge cases) | Over/under-exposure scenarios | Correct warning/reject logic applied |

#### RTM Cross-Reference (SWU-2.10)

Traced in `docs/project/rtm_adv.md` **§7 SWU-2.10: Exposure Index Calculation**.
Requirement rows covered: REQ-ADV-013, REQ-ADV-022, REQ-ADV-071, REQ-ADV-070. Those rows carry
`XPE-VVP-P2ADV-001 §4.4` in their **VVP Ref** column.

Cross-SWU rows in `rtm_adv.md` §8 map to §4.1.3 (pipeline integration) and §8 (error handling and
graceful degradation) of this plan; RTM §2 (lifecycle) and §3 (not-initialized guard) have **no**
covering section in this plan and are recorded as `—` pending a lifecycle verification section.

---

## 5. Validation Evidence

### 5.1 Test Execution and Results

Upon completion of Run phase (implementation), validation evidence includes:

| Evidence Type | Location | Acceptance Criteria |
|---|---|---|
| Google Test output | `build/test_results/` | All 196 unit-test cases pass (0 failures) |
| Code coverage report | `build/coverage/` | >= 85% statement and branch coverage |
| Benchmark output hashes | `.moai/specs/SPEC-XPE-P2-ADV/benchmarks/` | Match frozen manifest values |
| Performance log | `build/perf_results/` | Each SWU within performance budgets |
| Integration test report | `build/integration_report.json` | P/Invoke compatibility verified |

### 5.2 Algorithm Validation Evidence

| SWU | Validation Type | Evidence Artifact | Acceptance Criteria |
|-----|---|---|---|
| SWU-2.5 (MFP) | Benchmark processing | BP-06-MFP output hash, spectral analysis | Output deterministic, contrast improvement quantified |
| SWU-2.6 (Fractional) | Observer task evidence | Clinical assessment document, artifact analysis | Observer confirms diagnostic improvement, no artifacts |
| SWU-2.8 (ROI) | Detection accuracy | ROI bounds on multiple collimator types, precision report | >= 95% detection accuracy across types |
| SWU-2.10 (EI) | IEC 62494-1 compliance | Reference phantom test results, EI error report | <= 0.1% error vs. reference values |

### 5.3 Clinical Validation (Phase 2 Gate)

Prior to product release, SWU-2.5 and SWU-2.6 require clinical evidence:

- **Observer task evidence**: Radiologists review enhanced images and assess diagnostic improvement
- **Task-based validation**: Lesion detection / classification improvement metrics
- **Comparative analysis**: Baseline vs. enhanced diagnostic performance

---

## 6. IEC 62304 Traceability Matrix

### 6.1 Requirements-to-Test Traceability

| SRS Requirement | SPEC Requirement | Test Case(s) | Verification Level |
|---|---|---|---|
| SRS-ADV-MFP-001 | REQ-ADV-MFP-001 | TC-MFP-001, TC-MFP-002, TC-MFP-ALG-001 | L1, L4 |
| SRS-ADV-MFP-002 | REQ-ADV-MFP-002 | TC-MFP-003, TC-MFP-ALG-003 | L1, L4 |
| SRS-ADV-MFP-003 | REQ-ADV-MFP-003 | TC-MFP-005, TC-MFP-006, TC-MFP-007 | L1 |
| SRS-ADV-FDE-001 | REQ-ADV-FDE-001 | TC-FDE-002, TC-FDE-ALG-001, TC-FDE-ALG-002 | L1, L4, L5 |
| SRS-ADV-FDE-002 | REQ-ADV-FDE-002 | TC-FDE-003, TC-FDE-ALG-003 | L1, L4 |
| SRS-ADV-FDE-003 | REQ-ADV-FDE-003 | TC-FDE-004, TC-FDE-005 | L1 |
| SRS-ADV-ROI-001 | REQ-ADV-ROI-001 | TC-ROI-001, TC-ROI-ALG-001 | L1, L4 |
| SRS-ADV-ROI-002 | REQ-ADV-ROI-002 | TC-ROI-002, TC-ROI-ALG-005 | L1, L4 |
| SRS-ADV-ROI-003 | REQ-ADV-ROI-003 | TC-ROI-003, TC-ROI-ALG-003 | L1, L4 |
| SRS-ADV-EI-001 | REQ-ADV-EI-001 | TC-EI-ROI-001, TC-EI-ROI-002, TC-EI-ALG-001 | L1, L4, L5 |
| SRS-ADV-EI-002 | REQ-ADV-EI-002 | TC-EI-ROI-003, TC-EI-ALG-001 | L1, L4 |
| SRS-ADV-EI-003 | REQ-ADV-EI-003 | TC-EI-ROI-004, TC-EI-ALG-002 | L1, L4 |

(Abbreviated for space; full matrix available in SPEC-XPE-P2-ADV Appendix C)

The authoritative requirement→implementation→test matrix for this module is
`docs/project/rtm_adv.md` (RTM-ADV-001). Since RTM-ADV-001 v1.4.0 each requirement row carries a
**VVP Ref** column naming the section of this document that verifies it, which closes the
IEC 62304 §5.7.4 SRS→test mapping loop in both directions.

### 6.2 Quality Attributes Traceability

| Quality Attribute | TRUST 5 Pillar | Verification Method |
|---|---|---|
| Correctness | Tested | 196 unit-test cases (§3.1), benchmark processing, algorithm validation |
| Clarity | Readable | Code review, Doxygen documentation generation |
| Consistency | Unified | clang-format compliance, naming conventions |
| Security | Secured | Input validation testing, buffer boundary checks, static analysis |
| Traceability | Trackable | Conventional commits, this V&V document, SPEC cross-reference |

---

## 7. Performance Validation

### 7.1 Performance Budgets

| SWU | Function | 3072x3072 Budget | Measurement Method |
|---|---|---|---|
| SWU-2.5 | `xpe_mfp_process` | <= 80ms | Wall-clock time, single thread |
| SWU-2.6 | `xpe_fractional_edge_enhance` | <= 50ms | Wall-clock time, single thread |
| SWU-2.8 | `xpe_roi_detect_collimation` | <= 30ms | Wall-clock time, single thread |
| SWU-2.10 | `xpe_calc_exposure_index` (ROI) | <= 5ms | Wall-clock time, single thread |
| **Total Pipeline** | **All SWUs sequenced** | **<= 200ms** | **Sum of stages** |

### 7.2 Performance Measurement Procedure

1. Load 3072x3072 reference image into memory
2. Repeat 10 times (warm-up cache):
   - Call SWU function with standard parameters
   - Record elapsed time via `std::chrono::high_resolution_clock`
3. Report median, mean, min, max elapsed time
4. Verify median <= budget value
5. Log detailed results to `build/perf_results/perf_log.txt`

### 7.3 Acceptance Criteria

- All individual SWUs meet budget within 10% tolerance
- Total pipeline within 200ms for standard 3-phase run (MFP → FDE → ROI+EI)
- No performance regression vs. previous sprint baseline

---

## 8. Error Handling and Robustness

### 8.1 Error Code Verification

| Error Condition | Expected Return Code | Test Case |
|---|---|---|
| NULL image pointer | XPE_ERR_INVALID_INPUT | TC-*-006 |
| NULL params pointer | XPE_ERR_INVALID_INPUT | TC-*-006 |
| Non-float32 format | XPE_ERR_UNSUPPORTED_FORMAT | TC-*-007 |
| Invalid parameter values | XPE_ERR_INVALID_INPUT | TC-*-005 |
| Zero/negative mean signal (EI) | XPE_ERR_PROCESSING_FAILED | TC-EI-* |
| Empty ROI area | XPE_ERR_INVALID_INPUT | TC-ROI-007 |
| Out-of-memory condition | XPE_ERR_OUT_OF_MEMORY | (OS-dependent, integration test) |

### 8.2 Graceful Degradation

| Failure Scenario | Expected Behavior | Test Case |
|---|---|---|
| MFP disabled | Process image without MFP stage, continue pipeline | Integration test |
| ROI detection fails | Use whole-image EI, post warning | TC-ROI-ALG-003 |
| Fractional order out of range | Reject invalid order, return error | TC-FDE-006 |

---

## 9. Test Case Reference

### 9.1 Test File Locations

See §3.2 for the authoritative per-file inventory. Summary by SWU:

| SWU | Files | Cases |
|-----|-------|------:|
| SWU-2.5 | `test_mfp_scalar.cpp`, `test_mfp_scalar_ext.cpp` | 31 |
| SWU-2.6 | `test_edge_enhancement.cpp`, `test_edge_enhancement_ext.cpp` | 32 |
| SWU-2.8 | `test_collimation_detect.cpp`, `test_collimation_detect_ext.cpp` | 26 |
| SWU-2.10 | `test_exposure_index.cpp`, `test_exposure_index_ext.cpp` | 24 |
| Cross-cutting | `test_lifecycle*.cpp`, `test_coverage_ext.cpp`, `test_api_header*.cpp`, `test_integration*.cpp` | 83 |
| **Total** | **15 files** | **196** |

### 9.2 Test Execution Command

```bash
cd build
ctest --output-on-failure --verbose
# or
cmake --build . --config Release --target test
```

### 9.3 Coverage Report Generation

```bash
# Post-processing coverage preset (includes xpe_enhance_advanced)
cmake --preset coverage-post
cmake --build --preset coverage-post --target coverage
cmake --build --preset coverage-post --target coverage_check   # fails below XPE_COVERAGE_MIN
```

`coverage_check` compares the `coverage.xml` line-rate against `XPE_COVERAGE_MIN` (0.85). The
coverage run excludes timing-budget tests via `XPE_COVERAGE_EXCLUDE_TESTS` — see §3.3.3.

---

## 10. V&V Documentation Requirements

### 10.1 Deliverables Upon Sync Phase Completion

| Deliverable | Format | Location | Due |
|---|---|---|---|
| Test Results Report | JSON + HTML | `build/test_results/report.html` | After Run phase |
| Code Coverage Report | HTML (lcov) | `build/coverage/index.html` | After Run phase |
| Performance Report | CSV + analysis | `build/perf_results/perf_summary.csv` | After Run phase |
| Algorithm Validation Summary | Markdown | `docs/project/validation_adv.md` | Sync phase |
| V&V Completion Checklist | Markdown | `.moai/specs/SPEC-XPE-P2-ADV/vvp_checklist.md` | Sync phase |

### 10.2 Sign-Off Criteria

Verification and validation is complete when:

1. ✓ All 196 unit-test cases pass (zero failures)
2. ✓ Code coverage >= 85% (`XPE_COVERAGE_MIN`) — measured state as of 2026-09-10 in §3.3.2
3. ✓ Performance budgets met for all functions
4. ✓ No critical defects in code review
5. ✓ IEC 62304 traceability matrix complete and verified
6. ✓ Benchmark processing results match frozen manifest
7. ✓ Algorithm validation evidence collected (L4 tests + benchmark data)
8. ✓ V&V documentation complete and reviewed
9. ✓ Memory-leak gate G3 passed for this module (§3.3.4)

---

## 11. Post-Release Validation

### 11.1 Long-Term Field Validation (L6)

After product release, validation continues via:

- **DI Drift Analysis**: Monthly review of clinical exposure index distribution; alert if drift > 0.5 DI units
- **Reject Analysis**: Collation of rejected images; analysis for systematic biases
- **Performance Monitoring**: Ongoing latency tracking; alert if > 10% regression
- **Regression Testing**: Quarterly re-execution of benchmark suite to detect unintended changes

### 11.2 Maintenance and Lifecycle

- Changes to algorithm parameters: Re-run affected benchmark suite (BP-06, BP-07, BP-08, BP-09)
- Security patches: Re-run full unit test suite + integration tests
- Major version bumps: Full V&V cycle including clinical validation for SWU-2.5 and SWU-2.6

---

## Document History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-21 | XPE Documentation | Initial V&V Plan for xpe_enhance_advanced.dll, 65 tests, IEC 62304 Class B alignment |
| 1.1.0 | 2026-09-10 | xpe-docs (issue #59) | §3.1/§3.2 corrected to the actual test inventory (15 files, 196 TEST/TEST_F cases) — the v1.0.0 "65 tests / 5 files" figures and filenames did not exist. Added §2.3 IEC 62304 §5.5/5.6/5.7 clause mapping of the six-level hierarchy. Added §3.3.2 measured coverage state (CI run 34414537575, `coverage` 0.649 FAIL / `coverage-post` 0.898 PASS), §3.3.3 timing-budget exclusion regex, §3.3.4 memory-leak gate G3. Added per-SWU RTM cross-reference subsections (§4.1–§4.4) and an RTM link in §6.1. Corrected test totals in §1.2, §2.1, §5.1, §6.2, §9.1, §10.2. |

---

## Appendix A: Benchmark Pack References

### A.1 Mandatory Benchmark Sets

| Benchmark ID | Purpose | Test Category | SPEC Section |
|---|---|---|---|
| BP-06 | MFP and Fractional Edge reference output | L4, L5 | SPEC-XPE-P2-ADV Sec. 7 |
| BP-07 | Phase 2 Advanced validation (ROI + EI refinement) | L4, L5 | SPEC-XPE-P2-ADV Sec. 7 |
| BP-08 | EI baseline / exposure index full range | L4, L5 | SPEC-XPE-P2-ADV Sec. 7 |
| BP-09 | EI edge cases (over/under-exposure, rejection) | L4, L5 | SPEC-XPE-P2-ADV Sec. 7 |

---

**End of Document — XPE-VVP-P2ADV-001 v1.1.0**
