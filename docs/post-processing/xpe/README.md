# XPE (X-ray Processing Engine) — 모듈 개요

**Package Version**: 1.0.0  
**IEC 62304 Classification**: Class B  
**Last Updated**: 2026-04-15  
**Document Count**: 23개  
**Status**: Controlled Draft  

---

## 1. 모듈 목적

XPE는 X선 영상의 전처리, 향상, 표시, EI 계산, AI 추론을 담당하는 주요 소프트웨어 항목이다. 8개 DLL과 1개 독립 실행 파일로 구성되며, IEC 62304 Class B 수명 주기 요구사항을 완전히 준수한다.

---

## 2. 소프트웨어 아키텍처 요약

```
Phase 0  :  xpe_common.dll          — 공통 인프라 (메모리 풀, ABI, 오류 처리)
Phase 1a :  xpe_preprocess.dll      — Offset/Gain/Defect/Ghost 보정
Phase 1b :  xpe_enhance_basic.dll   — Log Transform, CLAHE, W/L, EI Baseline
            xpe_display.dll         — Modality LUT, VOI LUT, GSDF
            xpe_dicom.dll           — DICOM Reader/Writer, J2K, GSPS
Phase 2  :  xpe_enhance_advanced.dll — 4계층 노이즈 감소, 엣지, GSVG, EI ROI
            xpe_gsvg.dll            — Grid Suppression / Virtual Grid
Phase 3  :  xpe_ai.dll (proxy)      — AI 모델 인터페이스
            xpe_ai_worker.exe       — 격리 AI 워커 프로세스
```

---

## 3. 문서 목록 (23개)

### 3.1 PRD 및 계획 문서

| ID | Document | IEC 62304 |
|----|----------|-----------|
| PRD-001 | [xray-postprocessing-prd.md](xray-postprocessing-prd.md) | — |
| PRD-002 | [XPE-PRD-002_Detailed_Project_Execution_PRD.md](XPE-PRD-002_Detailed_Project_Execution_PRD.md) | — |
| PRD-003 | [XPE-PRD-003_PRD_Decomposition_and_Backlog.md](XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | — |
| PLAN-001 | [XPE-PLAN-001_Consolidated_Execution_Plan.md](XPE-PLAN-001_Consolidated_Execution_Plan.md) | — |

### 3.2 IEC 62304 수명 주기 문서

| ID | Document | IEC 62304 Clause |
|----|----------|-----------------|
| SDP-001 | [XPE-SDP-001_Software_Development_Plan.md](XPE-SDP-001_Software_Development_Plan.md) | 5.1 |
| SRS-001 | [XPE-SRS-001_Software_Requirements_Specification.md](XPE-SRS-001_Software_Requirements_Specification.md) | 5.2 |
| SAD-001 | [XPE-SAD-001_Software_Architecture_Document.md](XPE-SAD-001_Software_Architecture_Document.md) | 5.3 |
| SDD-001 | [XPE-SDD-001_Software_Unit_Identification.md](XPE-SDD-001_Software_Unit_Identification.md) | 5.4 |
| SDD-002 | [XPE-SDD-002_Software_Detailed_Design.md](XPE-SDD-002_Software_Detailed_Design.md) | 5.4 |
| **ALG-001** | **[XPE-ALG-001_Unified_Algorithm_Development_Specification.md](XPE-ALG-001_Unified_Algorithm_Development_Specification.md)** | **5.4** |
| ITP-001 | [XPE-ITP-001_Integration_Test_Plan.md](XPE-ITP-001_Integration_Test_Plan.md) | 5.6 |
| STP-001 | [XPE-STP-001_Software_Test_Plan_and_Cases.md](XPE-STP-001_Software_Test_Plan_and_Cases.md) | 5.5 |
| VVP-001 | [XPE-VVP-001_Verification_Validation_Plan.md](XPE-VVP-001_Verification_Validation_Plan.md) | 5.7 |
| RTM-001 | [XPE-RTM-001_Requirements_Traceability_Matrix.md](XPE-RTM-001_Requirements_Traceability_Matrix.md) | 5.8 |
| SCM-001 | [XPE-SCM-001_Configuration_Management_Plan.md](XPE-SCM-001_Configuration_Management_Plan.md) | 6.1 |
| SRM-001 | [XPE-SRM-001_Software_Risk_Management_File.md](XPE-SRM-001_Software_Risk_Management_File.md) | 7 |
| SHA-001 | [XPE-SHA-001_Software_Hazard_Analysis.md](XPE-SHA-001_Software_Hazard_Analysis.md) | 7 |
| SOUP-001 | [XPE-SOUP-001_SOUP_Analysis.md](XPE-SOUP-001_SOUP_Analysis.md) | 8 |
| SMP-001 | [XPE-SMP-001_Software_Maintenance_Plan.md](XPE-SMP-001_Software_Maintenance_Plan.md) | 12 |
| SPR-001 | [XPE-SPR-001_Problem_Resolution_Process.md](XPE-SPR-001_Problem_Resolution_Process.md) | 9 |

### 3.3 규제 준수 패키지

| ID | Document |
|----|----------|
| MAP-001 | [XPE-62304-MAP-001_Compliance_Matrix.md](XPE-62304-MAP-001_Compliance_Matrix.md) |
| PKG-001 | [xpe-iec62304-class-b-package.md](xpe-iec62304-class-b-package.md) |
| SRP-001 | [XPE-SRP-001_Software_Release_Procedure.md](XPE-SRP-001_Software_Release_Procedure.md) |

---

## 4. XPE-ALG-001 알고리즘 명세 — 빠른 참조

`XPE-ALG-001`은 IEC 62304 §5.4 Detailed Design 문서이며, 시스템의 모든 알고리즘을 수학적 공식, C++ 의사코드, SIMD 최적화 전략, 검증 기준으로 명세한다.

### 4.1 해소된 알고리즘 공백

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §3.0 | Readout Validation (SWU-1.0) | GAP-I |
| §3.0.5 | Non-linearity Correction (monotonic LUT) | GAP-H |
| §3.3.4 | `update_defect_map_runtime()` — AVX2 구현 | GAP-E |
| §4.1 | `avx2_log_ps()` — Cephes 다항식 근사 | GAP-G |
| §5.1.3 | `nsct_grid_suppression()` — 4단계 NSCT | GAP-D |
| §7.2 | EI ROI Central Method 수학 수정 (√0.1 계수) | GAP-F |
| §9.4 | AED-0 Automatic Exposure Detection | GAP-J |
| §12.3 | NPS 계산 (IEC 62220-1 준수) | GAP-L |
| §12.4 | DQE 계산 | GAP-M |
| §12.5 | Collimation Mask Detection / CollimatorMask 클래스 | GAP-N |

### 4.2 문서 관계

```
XPE-SRS-001 (요구사항)
    ↓ 추적
XPE-SAD-001 (아키텍처)
    ↓ 추적
XPE-SDD-001 (단위 식별) + XPE-SDD-002 (상세 설계)
    ↓ 알고리즘 상세
XPE-ALG-001 (통합 알고리즘 명세)  ←→  xpe-algorithm-spec-deepsync.md (계약 수준 명세)
    ↓ 검증
XPE-STP-001 (테스트 계획)
```

### 4.3 알고리즘-SRS 요구사항 추적

| 알고리즘 | SRS ID | DLL |
|---------|--------|-----|
| Readout Validation | SRS-QC-001 (신규) | xpe_preprocess.dll |
| Non-linearity Correction | SRS-FUNC-001b (신규) | xpe_preprocess.dll |
| Offset Correction | SRS-FUNC-001 | xpe_preprocess.dll |
| Gain Correction | SRS-FUNC-002 | xpe_preprocess.dll |
| Defect Correction | SRS-FUNC-003 | xpe_preprocess.dll |
| Ghost/Lag Correction | SRS-FUNC-004 | xpe_preprocess.dll |
| Log Transform | SRS-FUNC-010 | xpe_enhance_basic.dll |
| Bilateral Filter | SRS-FUNC-011 | xpe_enhance_basic.dll |
| CLAHE | SRS-FUNC-012 | xpe_enhance_basic.dll |
| Edge Enhancement | SRS-FUNC-013 | xpe_enhance_advanced.dll |
| Grid Suppression (NSCT) | Phase 2 SRS TBD | xpe_gsvg.dll |
| Virtual Grid | Phase 2 SRS TBD | xpe_gsvg.dll |
| EI / DI | SRS-FUNC-009 | xpe_enhance_advanced.dll |
| NPS / DQE | SRS-MEAS-001 (신규) | xpe_enhance_advanced.dll |
| Collimation Mask | Phase 2 SRS TBD | xpe_enhance_advanced.dll |
| Panoramic Stitch | SRS-FUNC-017 | xpe_ai.dll |
| Bone Suppression | SRS-FUNC-018 | xpe_ai.dll |

---

## 5. IEC 62304 적용 범위 요약

| Clause | Description | 담당 문서 |
|--------|-------------|---------|
| 5.1 | Software Development Planning | SDP-001 |
| 5.2 | Software Requirements | SRS-001 |
| 5.3 | Software Architecture | SAD-001 |
| 5.4 | Software Detailed Design | SDD-001, SDD-002, **ALG-001** |
| 5.5 | Software Unit Implementation & Testing | STP-001 |
| 5.6 | Software Integration & Testing | ITP-001 |
| 5.7 | Software System Testing | VVP-001 |
| 5.8 | Software Release | RTM-001, SRP-001 |
| 6.1 | Configuration Management | SCM-001 |
| 7 | Risk Management | SRM-001, SHA-001 |
| 8 | SOUP Controls | SOUP-001 |
| 9 | Problem Resolution | SPR-001 |
| 12 | Software Maintenance | SMP-001 |

---

## 6. 관련 모듈

| 모듈 | 관계 | 위치 |
|------|------|------|
| GSVG | XPE Phase 2 서브시스템 (Grid/Virtual Grid) | [../gsvg/](../gsvg/) |
| Ghost Correction | XPE SWU-1.4 상세 규격 | [../ghost-correction/](../ghost-correction/) |
| Calibration | XPE 교정 파이프라인 (§9) | [../calibration/](../calibration/) |
| Panel Defect | XPE SWU-1.3 상세 규격 | [../panel-defect/](../panel-defect/) |
| Enhance Basic | XPE Phase 1b 향상 | [../enhance-basic/](../enhance-basic/) |
| Enhance Advanced | XPE Phase 2 향상 | [../enhance-advanced/](../enhance-advanced/) |
| AI Module | XPE Phase 3 AI | [../ai-module/](../ai-module/) |
| Display | XPE 표시 처리 | [../display/](../display/) |
| DICOM | XPE DICOM I/O | [../dicom/](../dicom/) |

---

## Change Log

| Date | Version | Changes |
|------|---------|---------|
| 2026-04-15 | 1.0.0 | 신규 생성. XPE-ALG-001 v1.1 통합 반영. 23개 문서 목록 완성. |
