# VVP Addendum: P1B Post-Processing Modules (ENH/DISP/DICOM) Verification & Validation Plan

**Document ID**: VVP-P1B-001
**Version**: 1.0.0
**Date**: 2026-04-22
**Parent**: XPE-VVP-001 v1.1 (docs/post-processing/xpe/)
**Grandparent**: XPE-SVVP-001 v1.4.0 (docs/project/)
**Scope**: P1B Post-Processing modules — xpe_enhance_basic, xpe_display, xpe_dicom
**IEC 62304 Clause**: 5.5.1–5.5.5 (L1 Unit), 5.6.1–5.6.7 (L2 Integration), 5.7.1–5.7.5 (L3 System)
**Safety Classification**: Class B
**Author**: main (governance)

---

## HISTORY

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-22 | main | Initial P1B VVP addendum covering ENH (67/67), DISP (48/48), DICOM (35/35) modules. |

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

| Module | SPEC | SWUs | API Functions | Test Count | Status |
|--------|------|------|:-------------:|:----------:|--------|
| xpe_enhance_basic.dll | SPEC-XPE-P1B-ENH v1.1 | SWU-2.1~2.4, 2.10 | 7 | 67/67 PASS | Implemented |
| xpe_display.dll | SPEC-XPE-P1B-DISP v1.0 | SWU-3.1~3.3 | 5 | 48/48 PASS | Implemented |
| xpe_dicom.dll | SPEC-XPE-P1B-DICOM v1.0 | SWU-4.1~4.4 | 10 | 35/35 PASS | Released |

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

| REQ ID | Test File | Target Count |
|--------|-----------|:------------:|
| REQ-ENH-001~006 | test_log_transform.cpp | 12+ |
| REQ-ENH-007~012 | test_noise_reduce.cpp | 15+ |
| REQ-ENH-013~017 | test_contrast_enhance.cpp | 10+ |
| REQ-ENH-018~022 | test_edge_enhance.cpp | 10+ |
| REQ-ENH-023~030 | test_exposure_index.cpp | 12+ |
| REQ-ENH-CC-001~005 | test_enhance_integration.cpp | 8+ |
| **Total** | | **67/67 PASS** |

#### xpe_display (modules/display/tests/)

| REQ ID | Test File | Target Count |
|--------|-----------|:------------:|
| REQ-DISP-001~008 | test_modality_lut.cpp | 10+ |
| REQ-DISP-009~018 | test_voi_lut.cpp | 15+ |
| REQ-DISP-019~028 | test_presentation_lut.cpp | 15+ |
| REQ-DISP-029~035 | test_display_integration.cpp | 8+ |
| **Total** | | **48/48 PASS** |

#### xpe_dicom (modules/dicom/tests/)

| REQ ID | Test File | Target Count |
|--------|-----------|:------------:|
| REQ-DICOM-001~012 | test_dicom_read.cpp | 10+ |
| REQ-DICOM-013~022 | test_dicom_write.cpp | 10+ |
| REQ-DICOM-023~028 | test_dicom_validate.cpp | 6+ |
| REQ-DICOM-029~046 | test_dicom_network.cpp + test_dicom_integration.cpp | 9+ |
| **Total** | | **35/35 PASS** |

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

| Module | SPEC Requirements | L1 Tests | L2 Tests | L3 Tests | L4 Tests | Total Verified |
|--------|:-----------------:|:--------:|:--------:|:--------:|:--------:|:--------------:|
| xpe_enhance_basic | 35 (REQ-ENH) | 67 | P/Invoke 5 + chain 2 | Pipeline 10 steps | BP-07/08/09 | ✅ |
| xpe_display | 35 (REQ-DISP) | 48 | P/Invoke 3 + chain 2 | Pipeline 3 stages | BP-08 | ✅ |
| xpe_dicom | 46 (REQ-DICOM) | 35 | P/Invoke 2 + chain 2 | Pipeline 1 stage | BP-10 ready | ✅ |
| **P1B Total** | **116** | **150** | **16** | **14** | **3+1** | **✅** |

---

## 9. Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| Author | main (governance) | 2026-04-22 | |
| Reviewer | | | |
| Approver | | | |
