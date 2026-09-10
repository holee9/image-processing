# VVP Addendum: P1B Post-Processing Modules (ENH/DISP/DICOM) Verification & Validation Plan

**Document ID**: VVP-P1B-001
**Version**: 1.1.0
**Date**: 2026-09-10
**Parent**: XPE-VVP-001 v1.1 (docs/post-processing/xpe/)
**Grandparent**: XPE-SVVP-001 v1.4.0 (docs/project/)
**Scope**: P1B Post-Processing modules — xpe_enhance_basic, xpe_display, xpe_dicom
**IEC 62304 Clause**: 5.5.1–5.5.5 (L1 Unit), 5.6.1–5.6.7 (L2 Integration), 5.7.1–5.7.5 (L3 System)
**Safety Classification**: Class B
**Author**: main (governance)
**Supersedes**: `docs/project/vvp-p1b-001-addendum.md` (Korean draft, superseded 2026-09-10 — its
§6 P/Invoke ABI boundary verification and §9 completion criteria are merged into §5.4 and §8.2 here)

---

## HISTORY

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-22 | main | Initial P1B VVP addendum covering ENH (67/67), DISP (48/48), DICOM (35/35) modules. |
| 1.1.0 | 2026-09-10 | xpe-docs (issue #59) | Test counts and filenames corrected against the source tree (ENH 92 / DISP 63 / DICOM 47 = 202 cases; `test_dicom_network_scu.cpp` replaces the non-existent `test_dicom_network.cpp` / `test_dicom_integration.cpp`). Added §2.1 IEC 62304 §5.5/5.6/5.7 clause mapping, §5.4 DLL-loading / P-Invoke boundary matrix and §8.2 completion criteria (both absorbed from the now-superseded Korean addendum), §8.3 RTM cross-reference, §6.5 measured coverage and leak-gate state. |

---

## 1. Purpose

This addendum operationalises `XPE-VVP-001` for the P1B Post-Processing scope. It maps each P1B requirement to concrete verification activities at Levels L1 through L4 as defined in `XPE-SVVP-001` Section 2.

Out of scope (covered elsewhere):
- Pre-processing modules — VVP-PREPROCESS-001
- Advanced post-processing (P2-ADV) — XPE-VVP-P2ADV-001
- GSVG module — separate VVP when R2 achieved
- AI module (Phase 3) — future VVP addendum
- GUI-specific tests — Lane C

---

## 2. Module Overview

| Module | SPEC | SWUs | API Functions | Test Files | Test Cases | Status |
|--------|------|------|:-------------:|:----------:|:----------:|--------|
| xpe_enhance_basic.dll | SPEC-XPE-P1B-ENH v1.1 | SWU-2.1~2.4, 2.10 | 7 | 8 | 92 | Implemented |
| xpe_display.dll | SPEC-XPE-P1B-DISP v1.0 | SWU-3.1~3.3 | 5 | 4 | 63 | Implemented |
| xpe_dicom.dll | SPEC-XPE-P1B-DICOM v1.0 | SWU-4.1~4.4 | 10 | 4 | 47 | Released |
| **P1B total** | | | | **16** | **202** | |

Test-case counts are `TEST` / `TEST_F` macro counts enumerated from `modules/*/tests/` on
2026-09-10. They replace the v1.0.0 figures (67 / 48 / 35 = 150), which were planning targets and
did not match the tree. This document records the **inventory**; per-run pass/fail results belong in
the CI test report, not here.

### 2.1 IEC 62304 Clause Mapping

| Level (XPE-SVVP-001 §2) | IEC 62304 Clause | Section of this document |
|---|---|---|
| L1 Unit Verification | **5.5.2 / 5.5.3 / 5.5.5** — unit verification process, acceptance criteria, verification | §4 |
| L2 Integration Verification | **5.6.1 / 5.6.2 / 5.6.3** — integrate units, verify integration, test integrated software | §5 |
| L3 System Verification | **5.7.1 / 5.7.4 / 5.7.5** — establish system tests, verify procedures, record contents | §6 |
| L4 Feature Verification | **5.7.1** (benchmark-level system test) | §7 |
| Regression after change | **5.6.4 / 5.7.3** | §6.4, parent XPE-VVP-001 §3.3 |
| Problem resolution | **5.6.6 / 5.7.2** (via XPE-SPR-001) | parent XPE-VVP-001 §3.5, §4.2 |
| Risk-control verification | **7.3.3** | XPE-RTM-001 §3, XPE-SHA-001 |

---

## 3. Requirement → VV Level Mapping

### 3.1 xpe_enhance_basic (SPEC-XPE-P1B-ENH)

| REQ ID | Description | L1 Unit | L2 Integration | L3 System | L4 Feature |
|--------|-------------|:-------:|:--------------:|:---------:|:----------:|
| REQ-ENH-001~006 | Log Transform (SWU-2.1) | ✓ | ✓ | ✓ | |
| REQ-ENH-007~012 | Noise Reduction (SWU-2.2) | ✓ | ✓ | ✓ | |
| REQ-ENH-013~017 | Contrast Enhancement / CLAHE (SWU-2.3) | ✓ | ✓ | ✓ | ✓ (BP-07) |
| REQ-ENH-018~022 | Edge Enhancement (SWU-2.4) | ✓ | ✓ | ✓ | |
| REQ-ENH-023~030 | Exposure Index Calculation (SWU-2.10) | ✓ | ✓ | ✓ | ✓ (BP-08, BP-09) |
| REQ-ENH-CC-001~005 | Cross-Cutting (ABI, threads, perf) | ✓ | ✓ | ✓ | |

### 3.2 xpe_display (SPEC-XPE-P1B-DISP)

| REQ ID | Description | L1 Unit | L2 Integration | L3 System | L4 Feature |
|--------|-------------|:-------:|:--------------:|:---------:|:----------:|
| REQ-DISP-001~008 | Modality LUT (SWU-3.1) | ✓ | ✓ | ✓ | |
| REQ-DISP-009~018 | VOI LUT + Presets (SWU-3.2) | ✓ | ✓ | ✓ | ✓ (BP-08) |
| REQ-DISP-019~028 | Presentation LUT + GSDF (SWU-3.3) | ✓ | ✓ | ✓ | ✓ (BP-08) |
| REQ-DISP-029~035 | Cross-Cutting (ABI, threads, perf) | ✓ | ✓ | ✓ | |

### 3.3 xpe_dicom (SPEC-XPE-P1B-DICOM)

| REQ ID | Description | L1 Unit | L2 Integration | L3 System | L4 Feature |
|--------|-------------|:-------:|:--------------:|:---------:|:----------:|
| REQ-DICOM-001~005 | DICOM Read / Parse (SWU-4.1) | ✓ | ✓ | ✓ | |
| REQ-DICOM-006~012 | Pixel Extraction / Metadata (SWU-4.1) | ✓ | ✓ | ✓ | |
| REQ-DICOM-013~022 | DICOM Write / J2K (SWU-4.2) | ✓ | ✓ | ✓ | |
| REQ-DICOM-023~028 | DICOM Validate (SWU-4.3) | ✓ | ✓ | ✓ | ✓ (BP-10) |
| REQ-DICOM-029~040 | DICOM Network SCU (SWU-4.4) | ✓ | ✓ | ✓ | ✓ (integration) |
| REQ-DICOM-041~046 | Cross-Cutting (ABI, memory, threads) | ✓ | ✓ | ✓ | |

---

## 4. Level 1 (Unit Verification) — P1B Specifics

### 4.1 Test Suite Mapping

#### xpe_enhance_basic (modules/enhance_basic/tests/)

| REQ ID | Test File | Cases (2026-09-10) |
|--------|-----------|:------------------:|
| REQ-ENH-001~006 | test_log_transform.cpp | 14 |
| REQ-ENH-007~012 | test_noise_reduce.cpp | 13 |
| REQ-ENH-013~017 | test_contrast_enhance.cpp | 10 |
| REQ-ENH-018~022 | test_edge_enhance.cpp | 11 |
| REQ-ENH-023~030 | test_exposure_index.cpp | 15 |
| REQ-ENH-001~012 (log + noise regression) | test_enh01_log_noise.cpp | 5 |
| REQ-ENH-CC-001~005 | test_enhance_integration.cpp | 7 |
| REQ-ENH-CC-002 (buffer/size guards) | test_datasize_guard.cpp | 17 |
| **Total** | **8 files** | **92** |

#### xpe_display (modules/display/tests/)

| REQ ID | Test File | Cases (2026-09-10) |
|--------|-----------|:------------------:|
| REQ-DISP-001~008 | test_modality_lut.cpp | 13 |
| REQ-DISP-009~018 | test_voi_lut.cpp | 18 |
| REQ-DISP-019~028 | test_presentation_lut.cpp | 15 |
| REQ-DISP-029~035 | test_display_integration.cpp | 17 |
| **Total** | **4 files** | **63** |

#### xpe_dicom (modules/dicom/tests/)

| REQ ID | Test File | Cases (2026-09-10) |
|--------|-----------|:------------------:|
| REQ-DICOM-001~012 | test_dicom_reader.cpp | 16 |
| REQ-DICOM-013~022 | test_dicom_writer.cpp | 15 |
| REQ-DICOM-023~028 | test_dicom_validator.cpp | 7 |
| REQ-DICOM-029~040 | test_dicom_network_scu.cpp | 9 |
| **Total** | **4 files** | **47** |

> **Filename correction (v1.1.0).** v1.0.0 named `test_dicom_read.cpp`, `test_dicom_write.cpp`,
> `test_dicom_validate.cpp`, `test_dicom_network.cpp` and `test_dicom_integration.cpp`. None of those
> paths exist. The four files above are the actual contents of `modules/dicom/tests/`; there is no
> dedicated DICOM integration test file, so **REQ-DICOM-041~046 (cross-cutting: ABI, memory, threads)
> has no dedicated L1 file** and is verified only at L2/L3 (§5, §6). This is an open coverage gap.

### 4.2 Acceptance Criteria

- Statement coverage ≥ 80% per unit (P1B target: ≥ 85%)
- Branch coverage ≥ 70% per unit
- Zero test failures
- Zero memory leaks (ASan clean)
- Zero critical static analysis findings

### 4.3 P1B-Specific L1 Pass/Fail Criteria

| Module | Criterion | Target | Verification |
|--------|-----------|--------|--------------|
| enhance_basic | CLAHE clip_limit boundaries | No overflow/underflow | Edge-case with extreme clip values |
| enhance_basic | EI/DI computation accuracy | ±0.1 EI, ±0.01 DI | IEC 62494-1 reference values |
| enhance_basic | No NaN/Inf in output | Zero violations | isfinite() check on all output pixels |
| display | VOI LINEAR_EXACT center accuracy | ±0.5 value | DICOM PS3.3 C.11.2.1.3 compliance |
| display | GSDF JND linearity | r² ≥ 0.99 | 1024-entry LUT regression |
| display | Float32→UINT16 format conversion | Zero data loss outside clamp | Range boundary test |
| dicom | DICOM Part 10 preamble validation | "DICM" magic check | Malformed file rejection test |
| dicom | J2K lossless round-trip | Bit-identical reconstruction | Compress→decompress pixel compare |
| dicom | C-STORE timeout handling | XPE_ERR_NETWORK_FAILED within timeoutMs | Network timeout simulation |

---

## 5. Level 2 (Integration Verification) — P1B Specifics

### 5.1 P/Invoke Integration Tests

Location: `clients/ImageProcTest/` (C# test project).

| Test | Module | REQ | Pass Criteria |
|------|--------|-----|---------------|
| XpeImageBuffer marshaling | All P1B | ABI | sizeof matches C++ (36 bytes), Pack=8 alignment |
| xpe_log_transform via P/Invoke | enhance_basic | REQ-ENH-CC-001 | Same output as direct C++ call |
| xpe_noise_reduce via P/Invoke | enhance_basic | REQ-ENH-CC-001 | Same output as direct C++ call |
| xpe_contrast_enhance via P/Invoke | enhance_basic | REQ-ENH-CC-001 | Same output as direct C++ call |
| xpe_edge_enhance via P/Invoke | enhance_basic | REQ-ENH-CC-001 | Same output as direct C++ call |
| xpe_calc_exposure_index via P/Invoke | enhance_basic | REQ-ENH-CC-001 | EI/DI values within tolerance |
| xpe_apply_modality_lut via P/Invoke | display | REQ-DISP-029 | Same output as direct C++ call |
| xpe_apply_voi_lut via P/Invoke | display | REQ-DISP-029 | Same output as direct C++ call |
| xpe_apply_presentation_lut via P/Invoke | display | REQ-DISP-029 | Same output as direct C++ call |
| xpe_dicom_read_image via P/Invoke | dicom | REQ-DICOM-041 | Pixel-exact extraction |
| xpe_dicom_write via P/Invoke | dicom | REQ-DICOM-041 | DICOM conformance validation |

### 5.2 Module Interconnection Tests

| Test | Scope | Pass Criteria |
|------|-------|---------------|
| xpe_common ↔ enhance_basic linkage | Alert queue, log routing, memory allocation | Alerts surface through xpe_common, logs appear with correct severity |
| xpe_common ↔ display linkage | Alert queue, log routing | Same as above |
| xpe_common ↔ dicom linkage | Alert queue, log routing, DCMTK SOUP interface | Same as above, DCMTK exceptions caught and converted |
| enhance_basic → display chain | Pipeline output | Float32 output from enhance_basic accepted by display input |
| display → dicom chain | Pipeline output | UINT16 output from display accepted by dicom write |

### 5.3 Dependency Verification

Per xpe-module-principles.md Rule 1:

| Module | dumpbin /dependents Expected | Forbidden |
|--------|-----------------------------|-----------|
| xpe_enhance_basic.dll | xpe_common.dll, fmt.dll, spdlog.dll | Any xpe_*.dll |
| xpe_display.dll | xpe_common.dll, fmt.dll, spdlog.dll | Any xpe_*.dll |
| xpe_dicom.dll | xpe_common.dll, fmt.dll, spdlog.dll, dcmtk*.dll | Any xpe_*.dll |

### 5.4 P/Invoke ABI Boundary and DLL-Loading Verification

Absorbed from the superseded Korean addendum §6. Test host: `ImageProcTest.exe` (C#) plus the
P/Invoke wrapper layer.

#### 5.4.1 Marshalling Matrix

| Module | Functions under test | Marshalled types | Pass criterion |
|--------|----------------------|------------------|----------------|
| xpe_enhance_basic | all 7 exported functions | `int[]`, `float[]`, struct-by-ref | Managed call result identical to the direct C++ call |
| xpe_display | all 5 exported functions | `byte[]`, `int[]`, struct-by-ref | Managed call result identical to the direct C++ call |
| xpe_dicom | all 10 exported functions | `string`, `int[]`, `byte[]` | Managed call result identical to the direct C++ call |

Procedure: (1) invoke each function 100 times from the managed host; (2) compare input/output value
integrity against the native reference; (3) exercise the DLL-absent path; (4) exercise concurrent
invocation for thread safety. `XpeImageBuffer` marshalling size is verified in §5.1.

#### 5.4.2 DLL-Loading Scenarios

| Scenario | Expected behaviour |
|----------|--------------------|
| only `xpe_enhance_basic.dll` present | module loads, other modules bypassed without error |
| only `xpe_display.dll` present | module loads, other modules bypassed without error |
| only `xpe_dicom.dll` present | module loads, other modules bypassed without error |
| no XPE DLL present | host reports "module unavailable"; no crash |

> Provenance note: the superseded addendum recorded these scenarios as passing on 2026-04-22. That
> result predates the current tree and is **not** carried forward as evidence here; the table above
> defines the verification, and the result of record is whatever the current CI/manual run reports.

---

## 6. Level 3 (System Verification) — P1B Specifics

### 6.1 Full P1B Pipeline Test

| Step | Operation | Module | REQ |
|------|-----------|--------|-----|
| 1 | Load preprocessed float32 image | (test harness) | - |
| 2 | xpe_log_transform | enhance_basic | REQ-ENH-001 |
| 3 | xpe_noise_reduce (bilateral) | enhance_basic | REQ-ENH-007 |
| 4 | xpe_contrast_enhance (CLAHE) | enhance_basic | REQ-ENH-013 |
| 5 | xpe_edge_enhance (unsharp mask) | enhance_basic | REQ-ENH-018 |
| 6 | xpe_calc_exposure_index | enhance_basic | REQ-ENH-023 |
| 7 | xpe_apply_modality_lut | display | REQ-DISP-001 |
| 8 | xpe_apply_voi_lut (LINEAR) | display | REQ-DISP-009 |
| 9 | xpe_apply_presentation_lut | display | REQ-DISP-019 |
| 10 | xpe_dicom_write | dicom | REQ-DICOM-013 |

### 6.2 Performance Budget (3072×3072)

| Stage | Module | Budget | REQ |
|--------|--------|:------:|-----|
| Log Transform | enhance_basic | ≤ 15ms | REQ-ENH-006 |
| Noise Reduce (bilateral) | enhance_basic | ≤ 100ms | REQ-ENH-012 |
| Contrast Enhance (CLAHE) | enhance_basic | ≤ 50ms | REQ-ENH-017 |
| Edge Enhance (unsharp) | enhance_basic | ≤ 20ms | REQ-ENH-022 |
| Full enhance_basic pipeline | enhance_basic | ≤ 200ms | REQ-ENH-CC-005 |
| Modality LUT | display | ≤ 20ms | REQ-DISP-008 |
| VOI LUT | display | ≤ 16ms | REQ-DISP-016 |
| Presentation LUT | display | ≤ 25ms | REQ-DISP-028 |
| DICOM Write (uncompressed) | dicom | ≤ 100ms | REQ-DICOM-013 |
| **Total P1B Pipeline** | | **≤ 500ms** | |

### 6.3 Memory Discipline

- Peak memory ≤ 190MB for full Phase 1 pipeline (including preprocess)
- Zero heap growth over 1000-frame continuous processing
- No memory leaks on any code path (ASan clean)

### 6.4 Error Recovery Tests

| Scenario | Module | Expected Behavior |
|----------|--------|-------------------|
| NULL image pointer | All P1B | XPE_ERR_INVALID_INPUT, no crash |
| Wrong pixel format (UINT16 input to float32-only API) | enhance_basic, display | XPE_ERR_UNSUPPORTED_FORMAT |
| Corrupted DICOM file | dicom | XPE_ERR_DICOM_INVALID, clean resource release |
| Network timeout during C-STORE | dicom | XPE_ERR_NETWORK_FAILED, no resource leak |
| Zero-size image | All P1B | XPE_ERR_INVALID_INPUT |
| Out-of-range parameter (clip_limit < 1.0) | enhance_basic | XPE_ERR_INVALID_INPUT |

### 6.5 Measured Coverage and Leak-Gate State (2026-09-10)

Measurements, not targets. Coverage gate: `XPE_COVERAGE_MIN` = 0.85 line-rate per DLL
(`cmake/XpeCoverage.cmake:22`, REQ-P0-006).

| Preset | DLLs in scope | Measured line-rate | Gate vs. 0.85 |
|--------|---------------|--------------------|---------------|
| `coverage` | xpe_common, xpe_preprocess | 0.725 | **FAIL** |
| `coverage-post` | xpe_common, xpe_gsvg, xpe_enhance_basic, xpe_enhance_advanced, xpe_display | 0.898 | **PASS** |
| `coverage-dicom` | xpe_common, xpe_dicom | 0.696 | **FAIL** |

Source: CI `workflow_dispatch` run `34444576614`, 2026-09-10 (earlier run `34414537575` measured
`coverage` 0.649 before test_xcal_compression was registered). `coverage-dicom` was added on
2026-09-10 (#124, vcpkg DCMTK/OpenJPEG); its first run skipped 24 of 111 ctest cases on the CI
runner (DegradedMode 10, DicomNetwork 8, Validator 4, Reader 2), which is the leading suspect for
the low rate — QA-B-25 tracks the cause.

Timing-budget tests are excluded from the coverage run by the ctest `-E` regex
`XPE_COVERAGE_EXCLUDE_TESTS` (`cmake/XpeCoverage.cmake:29`):

```
Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance
```

Consequently the §6.2 performance budgets are **not** verified by the coverage run and require a
separate non-instrumented Release execution.

**Memory-leak gate G3** (issue #105, closed 2026-09-10) applies to all 7 modules including the three
P1B modules: 100 warm-up cycles → working-set baseline → 1000 measured cycles → growth < 1 MB, with a
deliberate 4096 B/cycle injected-leak probe confirming harness sensitivity. This supersedes the
looser "zero heap growth over 1000-frame continuous processing" wording of §6.3.
AddressSanitizer (`/fsanitize=address`) is used as an ad-hoc localisation method in QA scratch
builds; it is not a CMake build option of this project.

---

## 7. Level 4 (Feature Verification) — P1B Specifics

### 7.1 Benchmark Pack Coverage

| BP | Module | Feature | Status |
|----|--------|---------|--------|
| BP-07 | enhance_basic | CLAHE quality regression | ✅ Frozen (Post-B) |
| BP-08 | enhance_basic + display | EI/DI accuracy + VOI+Presentation pipeline | ✅ Frozen (Post-B) |
| BP-09 | enhance_basic | EI rejection (non-normative cases) | ✅ Frozen (Post-B) |
| BP-10 | cross-lane | Degraded-mode stress | CI workflow ready |

### 7.2 Degraded Mode Verification

Each P1B module must function correctly when:
- Input data is partially invalid (NaN, Inf, out-of-range values)
- Upstream module output is degraded
- Optional features (GSDF, J2K compression, network SCU) are unavailable

---

## 8. Traceability Summary

### 8.1 Verification Inventory

| Module | SPEC Requirements | L1 Cases | L2 Tests | L3 Tests | L4 Benchmarks |
|--------|:-----------------:|:--------:|:--------:|:--------:|:-------------:|
| xpe_enhance_basic | 35 (REQ-ENH) | 92 | P/Invoke 5 + chain 2 | Pipeline 10 steps | BP-07 / BP-08 / BP-09 |
| xpe_display | 35 (REQ-DISP) | 63 | P/Invoke 3 + chain 2 | Pipeline 3 stages | BP-08 |
| xpe_dicom | 46 (REQ-DICOM) | 47 | P/Invoke 2 + chain 2 | Pipeline 1 stage | BP-10 (workflow ready) |
| **P1B Total** | **116** | **202** | **16** | **14** | **3 frozen + 1 ready** |

This table counts planned and existing verification items. Execution outcome per release is recorded
in the CI test report, not in this plan.

### 8.2 V&V Completion Criteria

Absorbed and corrected from the superseded Korean addendum §9. P1B V&V is complete when **all** of
the following hold:

1. All 202 L1 unit-test cases (§4.1) execute with zero failures.
2. Line coverage ≥ `XPE_COVERAGE_MIN` (0.85) for each P1B DLL — `xpe_dicom` is measured by
   `coverage-dicom` since 2026-09-10 and currently fails at 0.696 (§6.5).
3. Performance budgets of §6.2 met in a non-instrumented Release run (the coverage run excludes
   timing tests — §6.5).
4. Memory-leak gate G3 passed for all three P1B modules (§6.5).
5. P/Invoke ABI boundary and DLL-loading scenarios of §5.4 verified.
6. Dependency rule of §5.3 satisfied (no `xpe_*.dll` → `xpe_*.dll` edge except `xpe_common`).
7. Benchmark packs BP-07 / BP-08 / BP-09 match their frozen manifests; BP-10 executed.
8. No critical findings open from code review or static analysis.
9. IEC 62304 traceability complete: every REQ row in `XPE-RTM-001` carries a VVP Ref (§8.3).
10. This document reviewed and approved (§9).

### 8.3 RTM Cross-Reference

| RTM Document | Rows Covered by This Plan | VVP Ref Value Used |
|---|---|---|
| `docs/post-processing/xpe/XPE-RTM-001_Requirements_Traceability_Matrix.md` §2 | SWU-2.1~2.4, 2.10 rows | `VVP-P1B-001 §4.1` |
| same, §2 | SWU-3.1~3.3 rows | `VVP-P1B-001 §4.1` |
| same, §2 | SWU-4.1~4.3 rows | `VVP-P1B-001 §4.1` |
| same, §2 | integration / system rows | `VVP-P1B-001 §5`, `§6` |

Since XPE-RTM-001 v1.7 each forward-traceability row carries a **VVP Ref** column, closing the
IEC 62304 §5.7.4 SRS→test mapping in both directions.

---

## 9. Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | main (governance) | 2026-04-22 | |
| Reviewer | | | |
| Approver | | | |
