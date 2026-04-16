# Software Hazard Analysis - Calibration Module

> **Document ID**: SHA-CALIB-001 | **Version**: 1.1 | **Date**: 2026-04-14
>
> **IEC 62304 Clause**: 7 (ISO 14971 integration)
>
> **Safety Classification**: Class B
>
> **Trace Source**: XPE-SRS-001, SRM (Software Risk Management), SAD-001  
> **Acquisition Reference**: IAP-CALIB-001 — 캘리브레이션 영상 취득 절차 (위험 통제 검증에 사용되는 실제 영상의 취득 조건 정의)  
> **Test Verification**: TDS-CALIB-001 — 위험 통제 효과를 검증하는 테스트 케이스의 입력 데이터 명세

---

## 1. Purpose & Scope

This document identifies and assesses hazardous situations arising from the XPE Calibration module (`xpe_preprocess.dll`, Phase 1a) according to ISO 14971:2019 Risk Management for medical devices. The calibration module manages loading and validation of offset maps, gain maps, defect pixel maps (BPM), nonlinearity correction coefficients, and temperature compensation data from persistent storage.

Hazardous situations addressed include:
- Corrupted or missing calibration files
- CRC/integrity check failures
- Expired calibration data
- Incomplete defect pixel detection
- Temperature sensor failures
- Session ID collisions in multi-detector systems
- File access race conditions

This document serves as the formal hazard identification source for risk control traceability (XPE-RTM-001 §3).

### Related Documents

| Document | Relationship |
|----------|-------------|
| **SRS-CALIB-001** | Safety requirements (SRS-CALIB-SAFE-001 through SAFE-005) implement hazard risk controls identified here |
| **SAD-CALIB-001** | Architectural units (SWU-1.5, SWU-1.1–1.4) implementing risk controls |
| **RTM-CALIB-001** | Bidirectional traceability: HAZ-CALIB-XXX ↔ SRS-SAFE ↔ test cases |
| **IAP-CALIB-001** | Acquisition protocol whose adherence reduces HAZ-CALIB-004 (BPM outdated) and HAZ-CALIB-005 (Nonlinearity mismatch) probability; Section 6.3 BPM protocol and §6.4 Nonlinearity protocol are preventive controls |
| **TDS-CALIB-001** | Test datasets used to verify effectiveness of all 7 risk controls; synthetic data covers failure injection (HAZ-CALIB-001 corrupted offset, HAZ-CALIB-003 expired timestamp); real data via IAP §6 |

---

## 2. Risk Acceptability Matrix

ISO 14971 Annex C severity × probability framework:

| Probability | Negligible | Minor | Serious | Critical | Catastrophic |
|---|:-:|:-:|:-:|:-:|:-:|
| **Frequent** | Low | Medium | **High** | **Unacceptable** | **Unacceptable** |
| **Probable** | Low | Medium | **High** | **High** | **Unacceptable** |
| **Occasional** | Low | Low | Medium | **High** | **High** |
| **Remote** | Low | Low | Low | Medium | **High** |
| **Improbable** | Low | Low | Low | Low | Medium |

**Acceptable Risk**: Low or Medium (after control)  
**Unacceptable Risk**: High or greater requires risk mitigation

---

## 3. Hazard Identification Table

### HAZ-CALIB-001: Missing or Corrupted Offset Map

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-001 |
| **Hazardous Situation** | Offset map file absent, unreadable, or binary corrupted → xpe_calib_load_offset() returns XPE_ERR_NOT_INITIALIZED or XPE_ERR_FILE_CORRUPTED → dark current offset data unavailable → Pipeline fails or uses stale cached offset → Underexposure or overexposure in corrected image |
| **Cause (Software)** | File system corruption, storage media failure, incomplete write during calibration update, permission errors, file moved/deleted by operator, version mismatch in binary format |
| **Affected SWU** | SWU-1.5 (CalibDataManager), SWU-1.1 (OffsetCorrection) |
| **Sequence** | (1) Startup: CalibManager attempts to load offset map from persistent storage (2) File system returns error (not found, I/O error, bad CRC) (3) xpe_calib_load_offset() fails, returns error code (4) Pipeline initialization blocked OR outdated offset cached from previous session (5) Offset correction stage executes with incorrect dark current model (6) Resulting image has systematic bias (too bright or too dark) (7) Radiologist sees uncalibrated image, misdiagnoses lesion presence/size |
| **Harm** | Diagnostic error (misdiagnosis, missed lesion, or false positive), potential patient harm (Serious) |
| **Severity** | Serious (image uncalibrated; diagnostic errors possible) |
| **Probability (Pre-Control)** | Remote (storage corruption is infrequent with modern HDDs/SSDs) |
| **Risk Level (Pre)** | **Low** |

### HAZ-CALIB-002: Gain Map File Corruption (CRC Failure)

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-002 |
| **Hazardous Situation** | Gain map file passes existence check but binary data corrupted (bit flips, incomplete transfer, media error) → CRC validation catches error but correction not applied → Pixel normalization uses corrupted gain coefficients → Incorrect brightness/contrast in output |
| **Cause (Software)** | Partial file write (power loss during save, interrupted transfer), bit flips in flash storage (SEU - Single Event Upset in long-term storage), network transfer corruption, malware/tampering |
| **Affected SWU** | SWU-1.5 (CalibDataManager CRC validation), SWU-1.2 (GainCorrection applies coefficients) |
| **Sequence** | (1) Gain map loaded and CRC computed (2) CRC mismatch detected → function returns XPE_ERR_CALIBRATION_CORRUPTED (3) Old/cached gain map substituted if available, or pipeline halted (4) If fallback occurs: old calibration from previous detector session applied (5) Subtle gain distortion undetectable to operator (6) Image appears slightly too bright/dark in regions with non-uniform gain error (7) Radiologist may attribute artifact to pathology |
| **Harm** | Diagnostic error due to subtle gain non-uniformity; potentially missed subtle findings (Serious) |
| **Severity** | Serious (localized gain artifacts can mask/mimic pathology) |
| **Probability (Pre-Control)** | Improbable (modern storage with ECC, but possible in extreme conditions) |
| **Risk Level (Pre)** | **Low** |

### HAZ-CALIB-003: Expired Calibration Data

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-003 |
| **Hazardous Situation** | Calibration timestamp exceeds maximum age (default: 30 days per xpe_calib_check_expiry()) → Function returns XPE_ERR_CALIBRATION_EXPIRED → Pipeline refuses to start OR expires warning issued but processing continues with stale coefficients → Systematic gain drift not accounted for → Pixel values increasingly inaccurate over time |
| **Cause (Software)** | Time synchronization failure on system clock (NTP service stopped), deliberate override of expiry check by technician without re-calibration, failure to schedule periodic recalibration, undetected detector gain drift |
| **Affected SWU** | SWU-1.5 (CalibDataManager expiry check function), all downstream SWU depend on valid calibration |
| **Sequence** | (1) Calibration file timestamp loaded (2) Current system time compared to calibration_timestamp + max_age (3) If exceeded: xpe_calib_check_expiry() returns error or warning (4) Override flag ignored → processing continues with expired data (5) Over subsequent hours, gain drift accumulates (e.g., temperature-dependent sensitivity drift) (6) Pixel values diverge from true corrected values by 2-5% (7) Subtle lesions become hard to visualize |
| **Harm** | Insidious reduction in image quality; delayed diagnosis or missed subtle findings (Serious) |
| **Severity** | Serious (time-dependent drift; effect cumulative) |
| **Probability (Pre-Control)** | Occasional (depends on operational discipline) |
| **Risk Level (Pre)** | **Medium** |

### HAZ-CALIB-004: Defect Pixel Map Incomplete or Outdated

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-004 |
| **Hazardous Situation** | Bad Pixel Map (BPM) missing newly appeared defects (hot/dead pixels) or is empty when new defects develop → DefectCorrection stage skips interpolation for unknown defects → Defect artifacts (bright or dark spots) remain in image → Radiologist may misinterpret bright spots as calcifications or dark spots as cysts |
| **Cause (Software)** | BPM not updated after factory calibration when detector develops new defects in field, defect detection algorithm fails to identify transient defects, BPM file overwritten with incorrect version, defect detection skipped in diagnostic mode |
| **Affected SWU** | SWU-1.5 (CalibDataManager loads BPM), SWU-1.3 (DefectCorrection applies map) |
| **Sequence** | (1) Detector develops a hot pixel due to manufacturing defect or aging (2) Factory BPM does not include this pixel (3) During preprocessing, DefectCorrection stage checks BPM—pixel is unmarked (4) No interpolation applied; hot pixel value remains in image (5) Bright spot visible in output (6) Radiologist sees spot, suspects micro-calcification (7) Unnecessary follow-up biopsy ordered |
| **Harm** | False positive diagnosis; unnecessary procedural intervention (Serious) |
| **Severity** | Serious (false pathology detected) |
| **Probability (Pre-Control)** | Occasional (detector defects develop over time) |
| **Risk Level (Pre)** | **Low** |

### HAZ-CALIB-005: Nonlinearity Correction Mismatch

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-005 |
| **Hazardous Situation** | Detector response curve correction coefficients (NLCSC - Nonlinearity Signal-Dependent Coefficients) do not match actual detector model or are incorrect from calibration error → Nonlinearity stage applies wrong linearization → Pixel values deviate from true detector response → Output image has non-uniform brightness/contrast |
| **Cause (Software)** | Calibration tool generates incorrect polynomial or LUT coefficients due to calibration software bug, wrong detector profile selected (profile mismatch), temperature-dependent coefficients loaded at wrong temperature, old coefficients not cleared when detector replaced |
| **Affected SWU** | SWU-1.5 (CalibDataManager loads NLCSC coefficients), SWU-1.8 (NonlinearityCorrection applies) |
| **Sequence** | (1) NLCSC coefficients loaded from configuration file (2) Detector response curve has a different polynomial order or offset than expected (3) Nonlinearity stage applies wrong correction formula (4) Pixel value transformation inaccurate (5) High-intensity regions over-corrected, low-intensity regions under-corrected (6) Output lacks proper dynamic range (7) Radiologist cannot confidently assess lesion density |
| **Harm** | Diagnostic difficulty; image not suitable for confident interpretation (Serious) |
| **Severity** | Serious (image quality degradation) |
| **Probability (Pre-Control)** | Remote (calibration tool validation catches most errors) |
| **Risk Level (Pre)** | **Low** |

### HAZ-CALIB-006: Temperature Compensation Failure

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-006 |
| **Hazardous Situation** | Temperature sensor unavailable, fails, or returns invalid value → Temp compensation stage skipped or uses nominal temperature → Dark current correction does not account for actual thermal drift → Subtle bias in corrected image varies with detector temperature |
| **Cause (Software)** | Temperature sensor I/O interface fails (hardware issue), sensor data unavailable from detector firmware, thermistor open/short circuit, calibration LUT incomplete (temperatures outside calibration range), sensor driver timeout |
| **Affected SWU** | SWU-1.5 (CalibDataManager supplies temperature LUT), SWU-1.7 (TempCompensation reads sensor) |
| **Sequence** | (1) TempCompensation stage queries detector temperature (2) Sensor returns null or error status (3) Function falls back to nominal temperature (25 C per default) (4) Actual detector is warmer (e.g., 35 C after warm-up) (5) Dark current not fully corrected (under-compensation by ~2-3%) (6) Noise floor in dark regions elevated (7) Subtle lesions in low-dose lung imaging harder to see |
| **Harm** | Image quality degradation in temperature-sensitive scenarios; potential missed findings (Minor-Serious) |
| **Severity** | Serious (depends on thermal drift magnitude; in tropical/high-load scenarios, harm increases) |
| **Probability (Pre-Control)** | Remote (temperature sensors generally reliable; failure rates <0.1% per year in service) |
| **Risk Level (Pre)** | **Low** |

### HAZ-CALIB-007: Session ID Collision in Multi-Detector Systems

| Field | Value |
|-------|-------|
| **Hazard ID** | HAZ-CALIB-007 |
| **Hazardous Situation** | Multi-detector imaging system with shared calibration storage → CalibDataManager uses session_id to segregate calibration data per detector → session_id collision or reuse → Wrong detector's calibration applied to another detector's image → Systematic error: offset/gain mismatch → Uncalibrated image output |
| **Cause (Software)** | Session ID generation algorithm reuses IDs before detectors stop using them, session ID not unique per detector (e.g., global counter not atomic), concurrent initialization races, no lock mechanism on session_id table |
| **Affected SWU** | SWU-1.5 (CalibDataManager session_id lookup), shared calibration index |
| **Sequence** | (1) Detector A initializes, session_id = 001, loads offset_A, gain_A (2) Detector B initializes, session_id = 002, loads offset_B, gain_B (3) Detector B finishes acquisition, session resource released (4) Detector A acquires new frame, session_id = 002 (reuse) (5) CalibDataManager lookup returns offset_B, gain_B (wrong!) (6) Frame from Detector A processed with Detector B's calibration (7) Systematic error undetectable without audit log (8) Radiologist sees wrong calibration artifact |
| **Harm** | Systematic processing error; if undetected, patient data corrupted (Serious) |
| **Severity** | Serious (wrong calibration applied systematically) |
| **Probability (Pre-Control)** | Improbable (session_id management is typically well-tested) |
| **Risk Level (Pre)** | **Low** |

---

## 4. Risk Assessment Matrix

### Pre-Control Assessment

| HAZ ID | Severity | Probability | Risk Level | Control Needed |
|--------|:--------:|:-----------:|:----------:|:---------:|
| HAZ-CALIB-001 | Serious | Remote | Low | Low priority |
| HAZ-CALIB-002 | Serious | Improbable | Low | Low priority |
| HAZ-CALIB-003 | Serious | Occasional | **Medium** | **Yes** |
| HAZ-CALIB-004 | Serious | Occasional | Low | Low priority |
| HAZ-CALIB-005 | Serious | Remote | Low | Low priority |
| HAZ-CALIB-006 | Serious | Remote | Low | Low priority |
| HAZ-CALIB-007 | Serious | Improbable | Low | Low priority |

---

## 5. Risk Controls per Hazard

### Control for HAZ-CALIB-001 (Missing/Corrupted Offset Map)

**Risk Control Strategy**: Fail-safe detection + graceful degradation

| Control | Implementation | SRS-SAFE Requirement | Test Case |
|---------|----------------|---------------------|-----------|
| File existence check | xpe_calib_load_offset() checks file path before read | SRS-SAFE-001 | UT-5.1-001 |
| Null pointer validation | Return XPE_ERR_NOT_INITIALIZED if offset == NULL | SRS-SAFE-001 | UT-1.5-001 |
| CRC validation | Compute & verify CRC32 on loaded buffer | SRS-SAFE-001 | UT-1.5-002 |
| Hard fail on missing data | Pipeline refuses to start if offset absent | SRS-SAFE-001 | IT-CALIB-001 |
| Operator alert | Error message logged + UI alert issued | SRS-SAFE-001 | ST-SAFE-001 |

**Residual Risk (Post-Control)**: Very Low (fail-stop prevents silent errors)

---

### Control for HAZ-CALIB-003 (Expired Calibration Data)

**Risk Control Strategy**: Time-based expiry enforcement + automated alert

| Control | Implementation | SRS-SAFE Requirement | Test Case |
|---------|----------------|---------------------|-----------|
| Timestamp recording | Store calibration generation timestamp in file metadata | SRS-SAFE-002 | UT-1.5-003 |
| Expiry check function | xpe_calib_check_expiry() compares current time to timestamp | SRS-SAFE-002 | UT-1.5-004 |
| Hard fail on expiry | Return XPE_ERR_CALIBRATION_EXPIRED; block pipeline start | SRS-SAFE-002 | IT-CALIB-002 |
| Configurable max_age | Operator can adjust max_age (default 30 days) | SRS-SAFE-002 | UT-1.5-005 |
| Recalibration reminder | Alert issued at 80% of max_age | SRS-SAFE-002 | ST-SAFE-002 |

**Residual Risk (Post-Control)**: Low (expiry check enforced; operator override prevents silent degradation)

---

### Control for HAZ-CALIB-004 (Incomplete Defect Pixel Map)

**Risk Control Strategy**: Runtime detection + alert-on-skip

| Control | Implementation | SRS-SAFE Requirement | Test Case |
|---------|----------------|---------------------|-----------|
| BPM validation | Check BPM is non-empty before DefectCorrection | SRS-SAFE-003 | UT-1.3-008 |
| Skip notification | If BPM empty, DefectCorrection skipped + alert issued | SRS-SAFE-003 | UT-1.3-008 |
| Runtime defect detection | Optional: xpe_defect_detect_runtime() scans frame for new defects | SRS-SAFE-003 | IT-CALIB-003 |
| Merge with factory map | Runtime detections added to factory BPM (non-destructive merge) | SRS-SAFE-003 | UT-1.5-006 |
| QA tracking | New defects logged for technician review | SRS-SAFE-003 | ST-SAFE-003 |

**Residual Risk (Post-Control)**: Low (alert on skip; optional runtime detection available)

---

### Control for HAZ-CALIB-007 (Session ID Collision)

**Risk Control Strategy**: Unique session ID generation + atomic operations

| Control | Implementation | SRS-SAFE Requirement | Test Case |
|---------|----------------|---------------------|-----------|
| Unique ID generation | session_id = UUID v4 (not simple counter) | SRS-SAFE-007 | UT-1.5-007 |
| Atomic lookup/insert | Use spinlock or atomic CAS for session_id table updates | SRS-SAFE-007 | UT-1.5-008 |
| Session timeout | Cleanup aged sessions > 1 hour old (prevent reuse) | SRS-SAFE-007 | IT-CALIB-005 |
| Collision detection | Log warning if duplicate session_id detected | SRS-SAFE-007 | UT-1.5-008 |

**Residual Risk (Post-Control)**: Very Low (UUID collision probability negligible)

---

## 6. Residual Risk Evaluation

After risk controls applied:

| HAZ ID | Pre-Control Risk | Control | Post-Control Risk | Acceptable |
|--------|:----------------:|---------|:-----------------:|:----------:|
| HAZ-CALIB-001 | Low | Fail-safe + alert | Very Low | ✓ |
| HAZ-CALIB-002 | Low | CRC validation | Low | ✓ |
| HAZ-CALIB-003 | Medium | Expiry enforcement | Low | ✓ |
| HAZ-CALIB-004 | Low | Runtime detection + alert | Low | ✓ |
| HAZ-CALIB-005 | Low | Profile validation | Low | ✓ |
| HAZ-CALIB-006 | Low | Fallback to nominal | Low | ✓ |
| HAZ-CALIB-007 | Low | UUID + atomic ops | Very Low | ✓ |

**Overall Assessment**: All residual risks acceptable (Low or Very Low)

---

## 7. Traceability to Requirements & Tests

| HAZ ID | Related SRS-SAFE | RTM Reference | Test Case | Verification |
|--------|-----------------|---------------|-----------|----|
| HAZ-CALIB-001 | SRS-SAFE-001 | XPE-RTM-001 §3 | UT-1.5-001..003, IT-CALIB-001, ST-SAFE-001 | Code review + unit test |
| HAZ-CALIB-002 | SRS-SAFE-002 | XPE-RTM-001 §3 | UT-1.5-002, ST-SAFE-002 | CRC validation test |
| HAZ-CALIB-003 | SRS-SAFE-002 | XPE-RTM-001 §3 | UT-1.5-004..005, IT-CALIB-002, ST-SAFE-002 | Integration test |
| HAZ-CALIB-004 | SRS-SAFE-003 | XPE-RTM-001 §3 | UT-1.3-008, IT-CALIB-003, ST-SAFE-003 | Runtime detection test |
| HAZ-CALIB-005 | SRS-SAFE-002 | XPE-RTM-001 §3 | UT-1.8-001..003 | Profile validation |
| HAZ-CALIB-006 | SRS-SAFE-002 | XPE-RTM-001 §3 | UT-1.7-001..002 | Sensor fallback test |
| HAZ-CALIB-007 | SRS-SAFE-007 | XPE-RTM-001 §3 | UT-1.5-007..008, IT-CALIB-005 | Concurrent init test |

---

## 8. Summary

The XPE Calibration module (Phase 1a preprocessing) manages critical calibration data (offset, gain, defect maps) that directly affect image quality and diagnostic accuracy. Seven hazardous situations identified; all assessed as Low or Medium pre-control risk. Seven risk control measures implemented spanning data validation, expiry enforcement, runtime detection, and unique session ID management. All residual risks assessed as acceptable (Low or Very Low).

**Document Status**: Ready for formal review and sign-off per IEC 62304 §7.4

**Review Checklist**:
- [ ] Hazard identification complete (all 7 identified)
- [ ] Risk assessment matrix reviewed
- [ ] Control measures traced to SRS-SAFE requirements
- [ ] Test cases assigned to all controls (inputs per TDS-CALIB-001)
- [ ] Real-image test data acquired per IAP-CALIB-001 protocols
- [ ] Residual risk acceptable
- [ ] Formal sign-off by Quality/Risk Manager

---

**Document End**

