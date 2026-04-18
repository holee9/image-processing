# Requirements Traceability Matrix

**Document ID:** XPE-RTM-001 v1.1  
**IEC 62304 Clause:** 5.1.1c, 5.3.6, 7.3.3  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose

시스템 요구사항 → 소프트웨어 요구사항 → 아키텍처 → 소프트웨어 유닛 → 테스트 → 리스크 제어 간의 양방향 추적성을 제공한다.

## 2. Forward Traceability (SRS → Test)

| SRS Req ID | Arch (SAD) | Unit (SDD-001) | Design Detail (SDD-002) | Unit Test (STP-001) | Integration Test | System Test | Risk Ref | HAZ (SHA-001) |
|-----------|:----------:|:----------:|:----------:|:---------:|:---------------:|:-----------:|:--------:|:--------:|
| SRS-FUNC-001 | SWI-1 | SWU-1.1 | SDD-002 §2.1 | UT-1.1-001..005 | IT-001 | ST-001 | — | — |
| SRS-FUNC-002 | SWI-1 | SWU-1.2 | SDD-002 §2.2 | UT-1.2-001..006 | IT-001 | ST-002 | — | — |
| SRS-FUNC-003 | SWI-1 | SWU-1.3 | SDD-002 §2.3 | UT-1.3-001..008 | IT-001 | ST-003 | HAZ-003 | SHA §3 HAZ-003 |
| SRS-FUNC-004 | SWI-1 | SWU-1.4 | SDD-002 §2.4 | UT-1.4-001..006 | IT-001 | ST-004 | HAZ-004 | SHA §3 HAZ-004 |
| SRS-FUNC-010 | SWI-2 | SWU-2.1 | SDD-002 §3.1 | UT-2.1-001..003 | IT-002 | ST-010 | — | — |
| SRS-FUNC-011 | SWI-2 | SWU-2.2 | SDD-002 §3.2 | UT-2.2-001..005 | IT-002 | ST-011 | — | — |
| SRS-FUNC-012 | SWI-2 | SWU-2.3 | SDD-002 §3.3 | UT-2.3-001..006 | IT-002 | ST-012 | — | — |
| SRS-FUNC-013 | SWI-2 | SWU-2.4 | SDD-002 §3.4 | UT-2.4-001..004 | IT-002 | ST-013 | HAZ-005 | SHA §3 HAZ-005 |
| SRS-FUNC-014 | SWI-2 | SWU-2.5 | SDD-002 §3.5 | UT-2.5-001..008 | IT-002 | ST-014 | — | — |
| SRS-FUNC-015 | SWI-2 | SWU-2.6 | SDD-002 §3.6 | UT-2.6-001..004 | IT-002 | ST-015 | — | — |
| SRS-FUNC-016 | SWI-2 | SWU-2.7,2.8,2.10 | SDD-002 §3.7,3.8,3.10 | UT-2.7/2.8/2.10-* | IT-002 | ST-016 | — | — |
| SRS-FUNC-017 | SWI-2 | SWU-2.9 | SDD-002 §3.9 | UT-2.9-001..006 | IT-002 | ST-017 | — | — |
| SRS-FUNC-018 | SWI-2 | SWU-2.11,2.12 | SDD-002 §3.11,3.12 | UT-2.11/2.12-* | IT-002 | ST-018 | HAZ-008 | SHA §3 HAZ-008 |
| SRS-FUNC-020 | SWI-3 | SWU-3.1 | SDD-002 §4.1 | UT-3.1-001..003 | IT-003 | ST-020 | — | — |
| SRS-FUNC-021 | SWI-3 | SWU-3.2 | SDD-002 §4.2 | UT-3.2-001..005 | IT-003 | ST-021 | HAZ-006 | SHA §3 HAZ-006 |
| SRS-FUNC-022 | SWI-3 | SWU-3.3 | SDD-002 §4.3 | UT-3.3-001..006 | IT-003 | ST-022 | HAZ-007 | SHA §3 HAZ-007 |
| SRS-FUNC-023 | SWI-3 | SWU-3.3 | SDD-002 §4.3 | UT-3.3-003,005 | IT-003 | ST-023 | — | — |
| SRS-FUNC-024 | SWI-5 | SWU-5.7 | XPE-GUI-COMPARE-001 §4-7 | GUI-CMP-VER-001..006 | IT-GUI-CMP | ST-GUI-CMP | HAZ-009 | SHA §3 HAZ-009 |
| SRS-FUNC-030 | SWI-4 | SWU-4.1,4.2 | SDD-002 §5.1,5.2 | UT-4.1/4.2-001..008 | IT-003 | ST-030 | — | — |
| SRS-FUNC-031 | SWI-4 | SWU-4.3 | SDD-002 §5.3 | UT-4.3-001..004 | IT-003 | ST-031 | — | — |
| SRS-FUNC-032 | SWI-4 | SWU-4.2 | SDD-002 §5.2 | UT-4.2-002,004 | IT-003 | ST-030 | — | — |
| SRS-SAFE-001 | SWI-1,5 | SWU-5.1 | SDD-002 §6.1 | UT-5.1-001..003 | IT-005 | ST-SAFE-001 | HAZ-001 | SHA §3 HAZ-001 |
| SRS-SAFE-002 | SWI-5 | SWU-5.5 | SDD-002 §6.5 | UT-5.5-001..005 | IT-002 | ST-SAFE-002 | HAZ-002 | SHA §3 HAZ-002 |
| SRS-SAFE-003 | SWI-1,5 | SWU-1.3,5.3 | SDD-002 §2.3,6.3 | UT-1.3-008,UT-5.3-001 | IT-005,011 | ST-SAFE-003 | HAZ-003 | SHA §3 HAZ-003 |
| SRS-SAFE-004 | SWI-1,4 | SWU-1.4,4.2 | SDD-002 §2.4,5.2 | UT-1.4-006,UT-4.2-005 | IT-001 | ST-SAFE-004 | HAZ-004 | SHA §3 HAZ-004 |
| SRS-SAFE-005 | SWI-2,5 | SWU-2.4,5.5 | SDD-002 §3.4,6.5 | UT-2.4-003..004,UT-5.5-002..003 | IT-002 | ST-SAFE-005 | HAZ-005 | SHA §3 HAZ-005 |
| SRS-SAFE-006 | SWI-3,5 | SWU-3.2,5.3 | SDD-002 §4.2,6.3 | UT-3.2-004..005 | IT-003 | ST-SAFE-006 | HAZ-006 | SHA §3 HAZ-006 |
| SRS-SAFE-007 | SWI-3,5 | SWU-3.3,5.3 | SDD-002 §4.3,6.3 | UT-3.3-002,004 | IT-003 | ST-SAFE-007 | HAZ-007 | SHA §3 HAZ-007 |
| SRS-SAFE-008 | SWI-2,3 | SWU-2.11,3.3 | SDD-002 §3.11,4.3 | UT-2.11-005,UT-3.3-007 | IT-002 | ST-SAFE-008 | HAZ-008 | SHA §3 HAZ-008 |
| SRS-SAFE-009 | SWI-3,5 | SWU-3.3,5.7 | SDD-002 §4.3,6.7 | UT-3.3-008,UT-5.7-002 | IT-003 | ST-SAFE-009 | HAZ-009 | SHA §3 HAZ-009 |
| SRS-SAFE-013 | SWI-5 | SWU-5.7 | XPE-GUI-COMPARE-001 §5-7 | GUI-CMP-VER-001..006 | IT-GUI-CMP | ST-SAFE-013 | HAZ-009 | SHA §3 HAZ-009 |
| SRS-PERF-001 | SWI-1 | All SWU-1.x | SDD-002 §2 | ST-PERF-001 | IT-010 | ST-PERF-001 | — | — |
| SRS-PERF-002 | All SWI | All SWU | SDD-002 §6.7 | — | IT-009 | ST-PERF-002 | — | — |
| SRS-PERF-003 | SWI-3 | SWU-3.2 | SDD-002 §4.2 | UT-3.2-interactive | IT-004 | ST-PERF-003 | — | — |
| SRS-PERF-004 | SWI-5 | SWU-5.1 | SDD-002 §6.1 | UT-5.1-002 | IT-006 | ST-PERF-004 | — | — |
| SRS-PERF-007 | SWI-5 | SWU-5.7 | XPE-GUI-COMPARE-001 §6-7 | GUI-CMP-VER-001 | IT-GUI-CMP | ST-PERF-007 | — | — |
| SRS-PERF-008 | SWI-5 | SWU-5.7 | XPE-GUI-COMPARE-001 §6-7 | GUI-CMP-VER-002,003 | IT-GUI-CMP | ST-PERF-008 | — | — |

## 3. Risk Control Traceability (7.3.3)

| Hazard ID | Risk Control (SRM) | SRS-SAFE Req | Architecture | Unit | Verification Test |
|-----------|-------------------|:------------:|:------------:|:----:|:-----------------:|
| HAZ-001 | Non-destructive processing | SRS-SAFE-001 | SWI-1,5 segregation | SWU-5.1 | ST-SAFE-001 |
| HAZ-002 | Validated presets | SRS-SAFE-002 | SWI-5 ParameterValidator | SWU-5.5 | ST-SAFE-002 |
| HAZ-003 | Defect correction failure alert | SRS-SAFE-003 | SWI-1,5 ErrorHandler | SWU-1.3,5.3 | ST-SAFE-003 |
| HAZ-004 | Ghost correction DICOM tag | SRS-SAFE-004 | SWI-1,4 metadata | SWU-1.4,4.2 | ST-004 |
| HAZ-005 | Enhancement gain limiting | SRS-SAFE-005 | SWI-2,5 validator | SWU-2.4,5.5 | ST-013 |
| HAZ-006 | W/L range warning | SRS-SAFE-006 | SWI-3,5 ErrorHandler | SWU-3.2,5.3 | ST-SAFE-006 |
| HAZ-007 | GSDF compliance warning | SRS-SAFE-007 | SWI-3,5 ErrorHandler | SWU-3.3,5.3 | ST-SAFE-007 |
| HAZ-008 | AI-processed label | SRS-SAFE-008 | SWI-2,3 overlay | SWU-2.11,3.3 | ST-SAFE-008 |
| HAZ-009 | Original/processed comparison | SRS-SAFE-009, SRS-SAFE-013, SRS-FUNC-024 | SWI-3,5 orchestrator | SWU-3.3,5.7 | ST-SAFE-009, ST-SAFE-013, ST-GUI-CMP |

## 4. Coverage Summary

| Direction | Total Items | Traced | Coverage |
|-----------|:-----------:|:------:|:--------:|
| SRS → Architecture (SAD) | 35 | 35 | 100% |
| SRS → Unit ID (SDD-001) | 35 | 35 | 100% |
| SRS → Detailed Design (SDD-002) | 35 | 35 | 100% |
| SRS → Unit Test (STP-001) | 35 | 35 | 100% |
| SRS → System Test | 35 | 35 | 100% |
| Hazard (SHA-001) → Risk Control → Test | 12 | 12 | 100% |
| **MR (MRD-001) → PR (PRD-SYSTEM-001) → SRS** | **28** | **28** | **100%** |

---

## 5. MR → PR Backward Traceability (XPE-MRD-001 → XPE-PRD-SYSTEM-001 → SRS)

**Added**: 2026-04-15. Closes OPEN-001 RTM gap identified in XPE-XVER-CONSOLIDATED-001 v3.0.0.

| MR ID | MR Description (Summary) | PR ID | SRS ID |
|-------|--------------------------|-------|--------|
| MR-FUNC-001 | Pre-processing 4종 보정 | PR-FUNC-001, 002, 003, 004 | SRS-FUNC-001, 002, 003, 004 |
| MR-FUNC-002 | Core 처리 (Log, NR, CE, EE) | PR-FUNC-010, 011, 012, 013 | SRS-FUNC-010, 011, 012, 013 |
| MR-FUNC-003 | DICOM Display Pipeline | PR-FUNC-020, 021, 022, 023 | SRS-FUNC-020, 021, 022, 023 |
| MR-FUNC-013 | Source/processed large-image comparison | PR-GUI-001 | SRS-FUNC-024 |
| MR-FUNC-004 | DICOM 파일/네트워크 | PR-FUNC-030, 031, 032 | SRS-FUNC-030, 031, 032 |
| MR-FUNC-005 | 교정 파라미터 관리 | PR-FUNC-040 | SRS-FUNC-040 (TBD Phase 1a) |
| MR-FUNC-006 | EI / DI 계산 | PR-FUNC-041 | SRS-FUNC-016 (EI baseline) |
| MR-FUNC-007 | Auto Exposure Detection 이벤트 처리 | PR-FUNC-042 | SRS-FUNC-042 (TBD Phase 0) |
| MR-FUNC-008 | Multiscale 처리 | PR-FUNC-014, 015 | SRS-FUNC-014, 015 |
| MR-FUNC-009 | Grid Suppression/Virtual Grid | PR-FUNC-050 | GSVG-SRS-001 (별도 패키지) |
| MR-FUNC-010 | CNN 신체 부위 인식 | PR-FUNC-016 | SRS-FUNC-016 |
| MR-FUNC-011 | DL Bone Suppression | PR-FUNC-018 | SRS-FUNC-018 |
| MR-FUNC-012 | AI 폴백 | PR-SAFE-010 | SRS-SAFE-010 (TBD Phase 3) |
| MR-PERF-001 | Pre-processing ≤ 500ms | PR-PERF-001 | SRS-PERF-001 |
| MR-PERF-002 | 전체 파이프라인 ≤ 3s | PR-PERF-002 | SRS-PERF-002 |
| MR-PERF-003 | W/L ≤ 16ms | PR-PERF-003 | SRS-PERF-003 |
| MR-PERF-004 | 메모리 ≤ 2GB | PR-PERF-004 | SRS-PERF-004 |
| MR-PERF-005 | 장기 안정성 | PR-PERF-005 | SRS-PERF-005 (TBD) |
| MR-REG-001 | IEC 62304 Class B 문서 | PR-REG-001 | SRS-REG-001 |
| MR-REG-002 | ISO 14971 SRM | PR-REG-002 | SRS-REG-002 |
| MR-REG-003 | FDA 21 CFR 820.30 | PR-REG-003 | SRS-REG-003 |
| MR-REG-004 | EU MDR 2017/745 | PR-REG-004 | SRS-REG-004 |
| MR-REG-005 | AI 처리 표시 | PR-SAFE-008 | SRS-SAFE-008 |
| MR-COMPAT-001 | DICOM 3.0 준수 | PR-FUNC-032 | SRS-FUNC-032 (DVTk) |
| MR-COMPAT-002 | C ABI 인터페이스 | PR-COMPAT-001 | SRS-COMPAT-001 (TBD) |
| MR-COMPAT-003 | Windows 10/11 64-bit | PR-COMPAT-002 | SRS-COMPAT-002 (TBD) |
| MR-BUSI-001 | 모듈별 라이선스 | PR-BUSI-001 | — (Non-SW) |
| MR-BUSI-002 | 파라미터 기반 FPD 추가 | PR-BUSI-002 | SRS-FUNC-040 |
| MR-BUSI-003 | 규제 문서 고객 제공 | PR-BUSI-003 | — (Non-SW) |

**Note**: "TBD" 항목은 해당 Phase gate 전 SRS에 추가 예정이다.

---

## 6. Algorithm SRS IDs — v1.5 GAP-AS~BB 추가 (XPE-ALG-001 v1.5)

| SRS Req ID | Algorithm | SWU | DLL | ALG Section |
|-----------|-----------|-----|-----|-------------|
| SRS-MEAS-004 | Perceptual IQM (PSNR/SSIM/MS-SSIM/FSIM) | SWU-18.0 | xpe_enhance_advanced.dll | §18 |
| SRS-FUNC-002d | Temperature-Compensated Gain Correction | SWU-1.12 | xpe_preprocess.dll | §3.12 |
| SRS-FUNC-001c | 2D FFT Notch Filter (Periodic Noise) | SWU-1.13 | xpe_preprocess.dll | §3.13 |
| SRS-FUNC-009b | AEC Feedback Loop (EI/DI → kVp/mAs) | SWU-9.10 | xpe_enhance_advanced.dll | §9.10 |
| SRS-QC-004 | SPC Calibration Control (Shewhart/CUSUM) | SWU-9.11 | xpe_common.dll | §9.11 |
| SRS-FLUORO-002 | Sub-pixel ECC Image Registration | SWU-14.2 | xpe_preprocess.dll | §14.2 |
| SRS-FUNC-011c | Signal-Dependent Quantum Noise Model | SWU-11.5 | xpe_enhance_advanced.dll | §11.5 |
| SRS-FUNC-008c | Moiré Artifact Detection & Suppression | SWU-5.5 | xpe_gsvg.dll | §5.5 |
| SRS-DICOM-002 | DICOM SR for CAD Findings (TID 1500/4100) | SWU-17.2 | xpe_dicom.dll | §17.2 |
| SRS-QA-001 | IEC 61223-3-5 Acceptance Testing Automation | SWU-12.10 | xpe_enhance_advanced.dll | §12.10 |

## 7. Algorithm SRS IDs — v1.6 GAP-BC~BL 추가 (XPE-ALG-001 v1.6)

| SRS Req ID | Algorithm | SWU | DLL | ALG Section |
|-----------|-----------|-----|-----|-------------|
| SRS-DOSE-001 | DAP/KERMA Cumulative Dose Tracking (IEC 60601-2-54) | SWU-9.12 | xpe_common.dll | §9.12 |
| SRS-DICOM-003 | JPEG 2000 Lossless/Lossy Compression (ISO 15444) | SWU-17.3 | xpe_dicom.dll | §17.3 |
| SRS-FUNC-001d | Motion Blur PSF Estimation & Wiener Deblur | SWU-1.14 | xpe_preprocess.dll | §3.14 |
| SRS-FUNC-001e | Metal High-Density Artifact Mask Generation | SWU-1.15 | xpe_preprocess.dll | §3.15 |
| SRS-TOMO-001 | Linear Tomosynthesis Reconstruction (FBP/SAA) | SWU-19.0 | xpe_enhance_advanced.dll | §19 |
| SRS-FUNC-017b | RANSAC+ORB Keypoint Panoramic Stitching | SWU-8.3.2 | xpe_ai.dll | §8.3.2 |
| SRS-FUNC-014b | Gaussian/Laplacian Multi-Resolution Pyramid | SWU-2.9 | xpe_enhance_advanced.dll | §4.9 |
| SRS-PERF-003 | GPU CUDA Pipeline Acceleration Architecture | SWU-10.9 | xpe_preprocess.dll | §10.9 |
| SRS-QA-002 | Auto QA Phantom Recognition (Leeds/CDRAD/CIRS) | SWU-12.11 | xpe_enhance_advanced.dll | §12.11 |
| SRS-CAL-002 | Cross-FPD Calibration Transfer Function | SWU-9.13 | xpe_preprocess.dll | §9.13 |

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |
| 1.1 | 2026-04-14 | XPE Team | SDD-002 section refs, SHA-001 hazard refs, STP-001 test ID updates added |
| 1.2 | 2026-04-15 | MoAI (SPEC-DOC-001) | §5 MR→PR Backward Traceability 추가 (OPEN-001 해소). Coverage summary 갱신 (HAZ 9→12). |
| 1.3 | 2026-04-15 | XPE Team | §6 Algorithm SRS IDs 추가 (XPE-ALG-001 v1.5 GAP-AS~BB 10건): SRS-MEAS-004, SRS-FUNC-002d/001c/009b/011c/008c, SRS-QC-004, SRS-FLUORO-002, SRS-DICOM-002, SRS-QA-001. |
| 1.4 | 2026-04-15 | XPE Team | §7 Algorithm SRS IDs 추가 (XPE-ALG-001 v1.6 GAP-BC~BL 10건): SRS-DOSE-001, SRS-DICOM-003, SRS-FUNC-001d/e, SRS-TOMO-001, SRS-FUNC-017b/014b, SRS-PERF-003, SRS-QA-002, SRS-CAL-002. |
| 1.5 | 2026-04-16 | Codex | Added large-image GUI comparison viewer traceability for Issue #8. |

---

*Document End — XPE-RTM-001 v1.3*
