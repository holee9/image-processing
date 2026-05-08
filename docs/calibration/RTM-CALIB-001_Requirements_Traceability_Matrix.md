# Requirements Traceability Matrix - Calibration Module

**Document ID:** RTM-CALIB-001 v1.3  
**IEC 62304 Clause:** 5.1.1c (backward traceability), 5.3.6 (design completeness), 7.3.3 (hazard control traceability)  
**Safety Classification:** Class B  
**Date:** 2026-04-24  
**Trace Source:** XPE-SRS-001, XPE-SAD-001 (Architecture), SHA-CALIB-001 (Hazards)  
**Test Input Source:** TDS-CALIB-001 (테스트 데이터셋 명세서) — 모든 테스트 케이스의 입력 데이터 규격 정의  
**Acquisition Reference:** IAP-CALIB-001 (영상 취득 프로토콜) — 실제 영상 기반 테스트의 취득 조건 명세  

---

## 1. Purpose

This document provides complete bidirectional traceability between:
- System Requirements (SRS) for calibration data management
- Software Architecture (SAD) units (SWI-1, SWI-5)
- Software Design Units (SDD - §3.1.5 Calibration loading)
- Test cases (Unit, Integration, System)
- Hazard identifications and risk controls (SHA-CALIB-001)

Ensures all requirements are designed, implemented, tested, and traceable to risk controls.

### Related Documents

| Document | Role in Traceability |
|----------|---------------------|
| **SRS-CALIB-001** | Source of all functional, safety, and performance requirements |
| **SAD-CALIB-001** | Maps requirements to SWU architectural units |
| **SHA-CALIB-001** | Provides HAZ-CALIB-001 through HAZ-CALIB-007 risk control traceability |
| **TDS-CALIB-001** | Defines test dataset specifications for all unit/integration test cases; test input data must conform to TDS-CALIB-001 §4–§7 |
| **IAP-CALIB-001** | Governs acquisition of real-image test inputs; all `real/` directory data must be acquired per IAP-CALIB-001 §6 protocols |

---

## 2. Traceability Matrix

| SRS Req ID | Requirement Summary | SAD SWU | Design Ref | Unit Test | Integ Test | System Test | HAZ Ref | Control |
|:-----------|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| **SRS-CALIB-FUNC-001** | Load offset map from persistent storage | SWU-1.5 | SAD §3.1.5 | UT-1.5-001 | IT-CALIB-001 | ST-001 | HAZ-CALIB-001 | CRC + null check |
| **SRS-CALIB-FUNC-002** | Load gain map from persistent storage | SWU-1.5 | SAD §3.1.5 | UT-1.5-002 | IT-CALIB-001 | ST-002 | HAZ-CALIB-002 | CRC + validation |
| **SRS-CALIB-FUNC-003** | Load bad pixel map (BPM) from file | SWU-1.5 | SAD §3.1.5 | UT-1.5-003 | IT-CALIB-001 | ST-003 | HAZ-CALIB-004 | Integrity check |
| **SRS-CALIB-FUNC-004** | Load nonlinearity correction coefficients | SWU-1.5 | SAD §3.1.5 | UT-1.5-004 | IT-CALIB-002 | ST-004 | HAZ-CALIB-005 | Profile validation |
| **SRS-CALIB-FUNC-005** | Load temperature compensation LUT | SWU-1.5 | SAD §3.1.5 | UT-1.5-005 | IT-CALIB-002 | ST-005 | HAZ-CALIB-006 | Fallback logic |
| **SRS-CALIB-FUNC-006** | Validate offset map dimensions (pitch, resolution) | SWU-1.5 | SAD §3.1.5 | UT-1.5-006 | IT-CALIB-001 | ST-006 | -- | Shape check |
| **SRS-CALIB-FUNC-007** | Validate gain map dimensions match offset | SWU-1.5 | SAD §3.1.5 | UT-1.5-007 | IT-CALIB-001 | ST-007 | -- | Dimension check |
| **SRS-CALIB-FUNC-008** | Validate gain map value ranges (0.5 to 2.0x typical) | SWU-1.5 | SAD §3.1.5 | UT-1.5-008 | IT-CALIB-001 | ST-008 | HAZ-CALIB-002 | Range bounds |
| **SRS-CALIB-FUNC-009** | Reject offset map with >5% dead zones (dark) | SWU-1.5 | SAD §3.1.5 | UT-1.5-009 | IT-CALIB-003 | ST-009 | -- | QA criterion |
| **SRS-CALIB-FUNC-010** | Apply CRC-32 checksum to all calibration maps | SWU-1.5 | SAD §3.1.5 | UT-1.5-010 | IT-CALIB-001 | ST-010 | HAZ-CALIB-001 | CRC validation |
| **SRS-CALIB-FUNC-011** | Persist calibration timestamp (generation time) | SWU-1.5 | SAD §3.1.5 | UT-1.5-011 | IT-CALIB-002 | ST-011 | HAZ-CALIB-003 | Timestamp record |
| **SRS-CALIB-FUNC-012** | Check expiry: block pipeline if age > max_age (30d) | SWU-1.5 | SAD §3.1.5 | UT-1.5-012 | IT-CALIB-002 | ST-012 | HAZ-CALIB-003 | Expiry enforce |
| **SRS-CALIB-FUNC-013** | Generate unique session_id per detector instance | SWU-1.5 | SAD §3.1.5 | UT-1.5-013 | IT-CALIB-005 | ST-013 | HAZ-CALIB-007 | UUID gen |
| **SRS-CALIB-FUNC-014** | Segregate calibration data per session_id | SWU-1.5 | SAD §3.1.5 | UT-1.5-014 | IT-CALIB-005 | ST-014 | HAZ-CALIB-007 | Session isolation |
| **SRS-CALIB-FUNC-015** | Support detector profile selection (detector_type) | SWU-1.5 | SAD §3.1.5 | UT-1.5-015 | IT-CALIB-002 | ST-015 | HAZ-CALIB-005 | Profile select |
| **SRS-CALIB-FUNC-016** | Version check: detect calibration format mismatch | SWU-1.5 | SAD §3.1.5 | UT-1.5-016 | IT-CALIB-001 | ST-016 | -- | Version check |
| **SRS-CALIB-FUNC-017** | Support field (on-site) calibration updates | SWU-1.5 | SAD §3.1.5 | UT-1.5-017 | IT-CALIB-004 | ST-017 | -- | Merge update |
| **SRS-CALIB-SAFE-001** | Fail-safe if offset absent: return error, do not proceed | SWU-1.5 | SAD §3.1.5 | UT-1.5-001 | IT-CALIB-001 | ST-SAFE-001 | HAZ-CALIB-001 | Hard fail |
| **SRS-CALIB-SAFE-002** | Fail-safe if gain absent: return error, do not proceed | SWU-1.5 | SAD §3.1.5 | UT-1.5-002 | IT-CALIB-001 | ST-SAFE-002 | HAZ-CALIB-002 | Hard fail |
| **SRS-CALIB-SAFE-003** | Alert operator if calibration corrupted (CRC fail) | SWU-1.5 | SAD §3.1.5 | UT-1.5-010 | IT-CALIB-001 | ST-SAFE-003 | HAZ-CALIB-001 | Alert + fail |
| **SRS-CALIB-SAFE-004** | Alert operator if calibration expired | SWU-1.5 | SAD §3.1.5 | UT-1.5-012 | IT-CALIB-002 | ST-SAFE-004 | HAZ-CALIB-003 | Expiry alert |
| **SRS-CALIB-SAFE-005** | Log all calibration load/unload events (audit trail) | SWU-1.5 | SAD §3.1.5 | UT-1.5-018 | IT-CALIB-004 | ST-SAFE-005 | -- | Audit log |
| **SRS-CALIB-PERF-001** | Load all calibration maps within 200 ms (startup) | SWU-1.5 | SAD §3.1.5 | ST-PERF-001 | IT-CALIB-006 | ST-PERF-001 | -- | Perf budget |
| **SRS-CALIB-PERF-002** | Support 3072 x 3072 detector resolution | SWU-1.5 | SAD §3.1.5 | UT-1.5-008 | IT-CALIB-001 | ST-PERF-002 | -- | Size support |
| **SRS-CALIB-PERF-003** | Support 4096 x 4096 detector (max) | SWU-1.5 | SAD §3.1.5 | UT-1.5-008 | IT-CALIB-001 | ST-PERF-003 | -- | Max size |
| **SRS-CALIB-FUNC-022** | BPM dark detection: min 32×32 adaptive window (replaces MC 256×7) | SWU-1.10 | SAD §3.4 | UT-BPM-001 | IT-BPM-001 | ST-BPM-001 | -- | Grid_abnormal |
| **SRS-CALIB-FUNC-023** | BPM bright detection: 128×128 window, tolerance 5~9% (replaces MC 60×60, 15%) | SWU-1.10 | SAD §3.4 | UT-BPM-002 | IT-BPM-001 | ST-BPM-002 | -- | CalData_6 |
| **SRS-CALIB-FUNC-024** | Multi-gain frame count: min 5~10, recommended 15~20 per dose level | SWU-1.10 | SAD §3.4 | UT-BPM-003 | IT-BPM-002 | ST-BPM-003 | -- | CalData_6 |
| **SRS-CALIB-FUNC-025** | Grid artifact robustness: LineArtifactScore < 10% (Blue target: < 5%) | SWU-1.10 | SAD §3.4 | UT-BPM-004 | IT-BPM-002 | ST-BPM-004 | -- | Grid_abnormal |
| **SRS-CALIB-FUNC-026** | Gain map generation from flat-field stack | SWU-1.12 | SAD §3.5 | UT-GAIN-GEN-001 | IT-GAIN-001 | ST-GAIN-001 | -- | CalData_6 |
| **SRS-CALIB-FUNC-027** | Multi-point gain polynomial fitting (N ≥ 3 dose levels) | SWU-1.12 | SAD §3.5 | UT-GAIN-GEN-002~003 | IT-GAIN-002 | ST-GAIN-002 | -- | CalData_6, cyan_test |
| **SRS-CALIB-FUNC-028** | Field calibration workflow (5 dark + 1 flat minimum) | SWU-1.13 | SAD §3.6 | UT-FIELD-001 | IT-FIELD-001 | ST-FIELD-001 | -- | CalData_6 subset |
| **SRS-CALIB-FUNC-029** | Calibration drift detection (Quick Check) | SWU-1.13 | SAD §3.6 | UT-FIELD-002~003 | IT-FIELD-002 | ST-FIELD-002 | -- | Simulated drift |
| **SRS-CALIB-FUNC-030** | Real-time offset adaptation for temperature changes | SWU-1.13 | SAD §3.6 | UT-DRIFT-001~002 | IT-FIELD-003 | ST-FIELD-003 | -- | Temp ramp simulation |
| **SRS-CALIB-FUNC-031** | Calibration mode selection API (XpeCalibrationMode enum, 6 modes) | SWU-1.12 | SAD §3.5 | UT-MODE-001~003 | IT-MODE-001 | ST-MODE-001 | -- | Mode enforcement |
| **SRS-CALIB-FUNC-032** | Multi-point calibration performance optimization (online fitting, SIMD, degree reduction) | SWU-1.12 | SAD §3.5 | UT-PERF-MP-001~003 | IT-MODE-002 | ST-MODE-002 | -- | Perf benchmark |
| **SRS-CALIB-FUNC-033** | Calibration quality metadata recording (R² gate, comparison metrics, mode-specific fields) | SWU-1.12 | SAD §3.5 | UT-META-001~003 | IT-MODE-003 | ST-MODE-003 | -- | Metadata validation |

---

## 3. Test Case Definitions (Brief)

> **Test Input Data**: All test cases requiring calibration map files (`.calib`, `.raw`) must use datasets prepared according to **TDS-CALIB-001**. Synthetic datasets are defined in TDS §4–§5; real image datasets in TDS §6. Golden reference comparison uses TDS §7 criteria (SSIM > 0.999).
>
> **Real Image Acquisition**: Real-image test inputs (`real/` datasets) must be acquired following **IAP-CALIB-001** §6 protocols. See IAP §6.1 (Dark), §6.2 (Flat-field), §6.3 (BPM), §6.4 (Nonlinearity), §6.5 (Lag/Ghost).

### Unit Tests (UT-1.5-001 through UT-1.5-018)

| Test ID | Requirement | Case Description | Input | Expected | Pass Crit |
|:---|:---|:---|:---|:---|:---|
| **UT-1.5-001** | SRS-CALIB-FUNC-001 | Load valid offset map | offset.calib file | XPE_OK, buffer loaded | Map loaded correctly |
| **UT-1.5-002** | SRS-CALIB-FUNC-002 | Load valid gain map | gain.calib file | XPE_OK, buffer loaded | Map loaded correctly |
| **UT-1.5-003** | SRS-CALIB-FUNC-003 | Load valid BPM | defects.calib file | XPE_OK, entries loaded | BPM entries parsed |
| **UT-1.5-004** | SRS-CALIB-FUNC-004 | Load NLCSC coefficients | nlcsc.json config | XPE_OK, poly parsed | Coefficients valid |
| **UT-1.5-005** | SRS-CALIB-FUNC-005 | Load temp LUT | temp_lut.json | XPE_OK, LUT table loaded | Table accessible |
| **UT-1.5-006** | SRS-CALIB-FUNC-006 | Validate offset dimensions | offset 3072x3072 | XPE_OK | Dimensions match |
| **UT-1.5-007** | SRS-CALIB-FUNC-007 | Gain/offset dimension match | gain 3072x3072, offset 3072x3072 | XPE_OK | Match verified |
| **UT-1.5-008** | SRS-CALIB-FUNC-008 | Reject gain out-of-range | gain=0.1 (< 0.5 min) | XPE_ERR_INVALID_DATA | Error returned |
| **UT-1.5-009** | SRS-CALIB-FUNC-009 | Detect dead zones in offset | offset with 10% black zone | XPE_ERR_CALIBRATION_QUALITY | QA rejected |
| **UT-1.5-010** | SRS-CALIB-FUNC-010 | CRC-32 computation & validation | offset buffer + CRC | CRC matches stored | CRC valid |
| **UT-1.5-011** | SRS-CALIB-FUNC-011 | Timestamp persistence | save with time=2026-04-14T10:00:00Z | timestamp stored | Time preserved |
| **UT-1.5-012** | SRS-CALIB-SAFE-004 | Expiry check: expired | cal_time + 31 days vs current | XPE_ERR_CALIBRATION_EXPIRED | Error returned |
| **UT-1.5-013** | SRS-CALIB-FUNC-013 | UUID generation uniqueness | call 1000x | all unique | No collisions |
| **UT-1.5-014** | SRS-CALIB-FUNC-014 | Session isolation | sessionA load offset_A, sessionB load offset_B | session lookups return correct maps | Isolation verified |
| **UT-1.5-015** | SRS-CALIB-FUNC-015 | Detector profile selection | detector_type="Varex XRD 4343N" | correct profile loaded | Profile matched |
| **UT-1.5-016** | SRS-CALIB-FUNC-016 | Version mismatch detection | offset v1.0, gain v2.0 | XPE_ERR_VERSION_MISMATCH | Error detected |
| **UT-1.5-017** | SRS-CALIB-FUNC-017 | Field update merge | factory BPM + 5 new defects | merged BPM with 10 entries total | Non-destructive merge |
| **UT-1.5-018** | SRS-CALIB-SAFE-005 | Audit log | load offset, save log | audit.log contains timestamp+action | Logged |

### Integration Tests (IT-CALIB-001 through IT-CALIB-005)

| Test ID | Requirement(s) | Case Description | Involvement |
|:---|:---|:---|:---|
| **IT-CALIB-001** | SRS-CALIB-FUNC-001..003, 010 | Load full calibration suite (offset, gain, BPM) at startup | SWI-1 + SWI-5 |
| **IT-CALIB-002** | SRS-CALIB-FUNC-004..005, 011..012 | Load & check expiry with temperature fallback | SWU-1.5 + SWU-1.7 |
| **IT-CALIB-003** | SRS-CALIB-FUNC-009 | QA validation: reject poor-quality offset | SWU-1.5 validation flow |
| **IT-CALIB-004** | SRS-CALIB-FUNC-017, SAFE-005 | Field calibration update merge + audit log | SWU-1.5 + logging |
| **IT-CALIB-005** | SRS-CALIB-FUNC-013..014 | Multi-session concurrent initialization (threading) | SWU-1.5 session table |
| **IT-BPM-001** | SRS-CALIB-FUNC-022..023 | BPM generation dark+bright with Grid_abnormal dataset | SWU-1.10 + Grid_abnormal fixture |
| **IT-BPM-002** | SRS-CALIB-FUNC-024..025 | Multi-gain frame count + LineArtifactScore validation | SWU-1.10 + CalData_6/Grid_abnormal |

---

## 4. Coverage Summary

### Forward Traceability (SRS → Test)

| Coverage Dimension | Total | Traced | % | Status |
|:---|:---:|:---:|:---:|:---|
| **Functional Req (SRS-CALIB-FUNC)** | 24 | 24 | **100%** | ✓ All traced |
| **Safety Req (SRS-CALIB-SAFE)** | 5 | 5 | **100%** | ✓ All traced |
| **Performance Req (SRS-CALIB-PERF)** | 3 | 3 | **100%** | ✓ All traced |
| **Total SRS Reqs** | **32** | **32** | **100%** | ✓ Complete |

### Backward Traceability (Test → SRS)

All test cases (23 UT + 4 UT-BPM + 9 UT-MODE/PERF-MP/META + 5 IT + 2 IT-BPM + 3 IT-MODE) reference at least one SRS requirement. No orphaned tests.

### Risk Control Traceability

| Hazard | Risk Control | SRS-SAFE Req | Test Case | Verification |
|:---|:---|:---:|:---:|:---|
| HAZ-CALIB-001 | Fail-safe on missing offset | SRS-CALIB-SAFE-001 | UT-1.5-001, IT-CALIB-001 | Code review + integration |
| HAZ-CALIB-002 | CRC validation on gain | SRS-CALIB-SAFE-002 | UT-1.5-010, IT-CALIB-001 | CRC test + integration |
| HAZ-CALIB-003 | Expiry enforcement | SRS-CALIB-SAFE-004 | UT-1.5-012, IT-CALIB-002 | Unit + integration |
| HAZ-CALIB-004 | BPM validation + runtime detection | SRS-CALIB-FUNC-003, SAFE-003 | UT-1.5-003, IT-CALIB-001 | Integrity check |
| HAZ-CALIB-005 | Profile validation | SRS-CALIB-FUNC-015 | UT-1.5-015 | Unit test |
| HAZ-CALIB-006 | Temperature fallback | SRS-CALIB-FUNC-005 | UT-1.5-005, IT-CALIB-002 | Unit + integration |
| HAZ-CALIB-007 | Session ID uniqueness + isolation | SRS-CALIB-FUNC-013..014 | UT-1.5-013..014, IT-CALIB-005 | Unit + threading test |

---

## 5. Design Completeness Check

Does every SRS requirement have a corresponding design section?

| SRS Req | SAD §3.1.5 Section | SDD-002 § | Status |
|:---|:---|:---|:---|
| SRS-CALIB-FUNC-001..005 | Overview | 3.1.5a | ✓ Designed |
| SRS-CALIB-FUNC-006..010 | Data validation | 3.1.5b | ✓ Designed |
| SRS-CALIB-FUNC-011..017 | Storage & updates | 3.1.5c | ✓ Designed |
| SRS-CALIB-SAFE-001..005 | Error handling | 3.1.5d | ✓ Designed |
| SRS-CALIB-PERF-001..003 | Performance budgets | 3.1.5e | ✓ Designed |
| SRS-CALIB-FUNC-022..025 | BPM generation algorithm | SAD §3.4 | ✓ Designed (2026-04-19) |
| SRS-CALIB-FUNC-026..027, 031..033 | Gain generation, mode selection, optimization, metadata | SAD §3.5 | ✓ Designed (2026-04-24 v1.2) |

---

### 5.1 Codex Traceability Addendum - 2026-04-28

Scope: `feat/preprocessing`, Issues #68, #69, #70.

Placeholder review: no `REQ-P1A-XXX` placeholder remains in the active preprocessing API and verification metric declarations after this update. The original RTM file itself did not contain `REQ-P1A-XXX` placeholders.

| Trace ID | Implementation Evidence | Verification Evidence | Status |
|:---|:---|:---|:---|
| SRS-CALIB-FUNC-016 / REQ-P1A-010 | `xpe_verify_offset`, `xpe_offset_correct` | Offset AVX2 parity tests; ctest 341/341 passed | Updated |
| SRS-CALIB-FUNC-017 / REQ-P1A-011 | `xpe_verify_gain`, `xpe_gain_correct` | Gain AVX2 parity tests; ctest 341/341 passed | Updated |
| SRS-CALIB-FUNC-019 / REQ-P1A-012 | `xpe_verify_defect`, `xpe_defect_correct` | Defect AVX2 parity tests; ctest 341/341 passed | Updated |
| SRS-CALIB-FUNC-015 / SRS-CALIB-FUNC-021 / REQ-P1A-041..047 | `xpe_verify_pipeline`, pipeline API comments | Pipeline verification metric tests; ctest 341/341 passed | Updated |
| SRS-CALIB-FUNC-022..025 | `xpe_bpm_generate` | BPM generation tests; ctest 341/341 passed | Updated |
| SRS-CALIB-FUNC-034 | `xpe_calib_generate_offset` file-writing path plus shared multi-method generation helper | `test_calib_generate_offset_multi.cpp`; ctest 341/341 passed | Added |
| SRS-CALIB-NFR-003-CACHE | `CalibrationLRUCache` mutex-protected list/index access | Code review plus preprocessing ctest 341/341 passed | Added |

---

## 6. Sign-Off & Approval

**Document Status**: Ready for formal review

**Review Checklist**:
- [ ] All 32 SRS requirements traced to architecture
- [ ] All 32 SRS requirements traced to test cases
- [ ] All 7 hazards have risk controls traced to SRS-SAFE
- [ ] Forward & backward traceability complete (100%)
- [ ] No orphaned requirements or tests
- [ ] Coverage summary reviewed

---

**Document End**
