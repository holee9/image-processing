# X-ray Image Processing Engine - Documentation System

**Version**: 3.5.0
**Last Updated**: 2026-04-22  
**Organization**: Hybrid 3-Tier (Normative/Informational/Archive) + IEC 62304 Traceability  

---

## 이 인덱스 사용 방법

- **Normative** 문서는 단일 정보 출처(SSoT)입니다. 정보가 충돌할 때는 정규 문서가 우선합니다.
- **Informational** 문서는 맥락, 분석 또는 구현 지침을 제공합니다. 정규 문서를 참고하지만 절대 이를 무시하지 않습니다.
- **Archive** 문서는 대체되었거나 역사적입니다. 감시 추적을 위해서만 보관됩니다.
- **IEC 62304** 패키지는 소프트웨어 항목(DLL 단위)별로 정리된 규제 전달물입니다.

### 정규 권한 테이블

| Topic | Normative Document | Scope |
|-------|-------------------|-------|
| 제품 정의, 컴포넌트, Phase | [product.md](project/product.md) | 우리가 무엇을 만드는가 |
| 저장소 구조, DLL 매핑 | [structure.md](project/structure.md) | 코드가 어디에 있는가 |
| 기술 스택, 종속성, ABI | [tech.md](project/tech.md) | 무엇으로 만드는가 |
| 파이프라인 단계 순서, 우회 규칙 | [pipeline-spec.md](project/pipeline-spec.md) | 단계가 어떻게 실행되는가 |
| C ABI 함수 서명 | [api-spec.md](project/api-spec.md) | API 계약 |
| 알고리즘 동작, 품질 게이트 | [xpe-algorithm-spec-deepsync.md](project/xpe-algorithm-spec-deepsync.md) | 알고리즘 계약 |
| SWU 카운트, Phase 할당 | [SPEC-XPE-MASTER.md](project/SPEC-XPE-MASTER.md) | 마스터 구현 계획 |
| Sprint 분해 | [sprint-plan.md](project/sprint-plan.md) | 실행 일정 |
| 구현 세부 사항 (바이너리 형식, JSON 스키마) | [xpe-implementation-reference.md](project/xpe-implementation-reference.md) | 개발자 참조 |

> 문서 간의 정보가 충돌할 때는 위의 정규 문서가 권위를 갖습니다.

---

## 1. 정규 문서 (단일 정보 출처)

프로젝트를 정의하는 핵심 명세입니다. 이 문서의 변경사항은 다운스트림 업데이트를 촉발합니다.

### 1.1 제품 기초

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [product.md](project/product.md) | — | v1.0 | 94 | 제품 정체성, 컴포넌트(Pre/Post/Support), Must-Have 대 Differentiator 전략, 대상 사용자 |
| [structure.md](project/structure.md) | — | v1.0 | 78 | 저장소 레이아웃, Module-to-DLL 매핑(38개 native SWU), 종속성 방향 규칙 |
| [tech.md](project/tech.md) | — | v1.0 | 136 | C++17/C#/.NET 8 스택, SOUP 종속성(8개 XPE + 5개 GSVG), 플랫폼 대상, C ABI 설계, HW/SW 전략 |

### 1.2 기술 명세

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [pipeline-spec.md](project/pipeline-spec.md) | PIPE-SPEC-001 | v1.3.0 | 666 | 17단계 파이프라인 시퀀스, 전처리 종속성 그래프, 우회 정책, 데이터 흐름 |
| [api-spec.md](project/api-spec.md) | XPE-API-SPEC-001 | v1.4.0 | 1,474 | 8개 DLL에 걸친 79개 내보낸 C ABI 함수 (AED 제거), 명시적 경로 API 패턴, P/Invoke 정렬 |
| [xpe-algorithm-spec-deepsync.md](project/xpe-algorithm-spec-deepsync.md) | ALG-SPEC-001 | v3.2.0-ds4 | 573 | 알고리즘 계약: DeepSync 결정, 연구 검증 모델, 품질 게이트, EI-0 해결. 상세 구현은 XPE-ALG-001 참조 |

### 1.3 구현 계획

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [SPEC-XPE-MASTER.md](project/SPEC-XPE-MASTER.md) | SPEC-XPE-MASTER | v3.0.0 | 495 | 마스터 계획: 43개 SWU, Phase 0-3, 교차 검증 요약, 문서 업데이트 매트릭스 |
| [sprint-plan.md](project/sprint-plan.md) | XPE-SPRINT-PLAN-001 | v1.2.0 | 1,430 | 28개 sprint, 종속성 그래프, sprint별 범위/API/테스트 대상, 로깅/Alert 검증 기준 |
| [xpe-implementation-reference.md](project/xpe-implementation-reference.md) | XPE-IMPL-REF-001 | v1.1.0 | 950 | Calibration 바이너리, 로깅/Alert JSON(§9), LUT 형식(§10), GSDF(§11), IPC(§12), 양자화(§13), session_id(§14) |

### 1.4 SPEC 문서 (.moai/specs/)

요구사항·인수기준·추적성을 정의하는 EARS 형식 SPEC 문서입니다.

| Document | Version | REQ Count | Description |
|----------|:-------:|:---------:|-------------|
| [SPEC-XPE-P0](../.moai/specs/SPEC-XPE-P0/spec.md) | — | 11 | Phase 0 Foundation |
| [SPEC-XPE-P1A](../.moai/specs/SPEC-XPE-P1A/spec.md) | v1.3.0 | 42 | Phase 1a 전처리 (Offset/Gain/Defect/Ghost) |
| [SPEC-XPE-P1B-ENH](../.moai/specs/SPEC-XPE-P1B-ENH/spec.md) | — | 30 | Phase 1b 기본 향상 |
| [SPEC-XPE-P1B-DISP](../.moai/specs/SPEC-XPE-P1B-DISP/spec.md) | — | 35 | Phase 1b 디스플레이 |
| [SPEC-XPE-P1B-DICOM](../.moai/specs/SPEC-XPE-P1B-DICOM/spec.md) | v1.1.0 | 40 | Phase 1b DICOM (Released) |
| [SPEC-XPE-P2-ADV](../.moai/specs/SPEC-XPE-P2-ADV/spec.md) | — | 65 tests | Phase 2 고급 향상 |
| [SPEC-XPE-GSVG](../.moai/specs/SPEC-XPE-GSVG/spec.md) | v1.0.0 | 26 | Grid Suppression + Virtual Grid (GS 8 + VG 10 + 성능 3 + 안전 5) |
| [SPEC-SIMD-001](../.moai/specs/SPEC-SIMD-001/spec.md) | v1.0.0 | 6 | SIMD Scalar Reference + Full-Operation Parity (+5점 임계경로) |
| [SPEC-BENCH-PRE](../.moai/specs/SPEC-BENCH-PRE/spec.md) | v1.0.0 | 7 | Preprocessing Benchmark Freeze (BP-01~05, DegradedMode 6/6 PASS) |
| [SPEC-BENCH-POST](../.moai/specs/SPEC-BENCH-POST/spec.md) | v1.0.0 | 6 | Post-Processing Benchmark Freeze (BP-06~09, 4/4 PASS) |
| [SPEC-XPE-P3-AI](../.moai/specs/SPEC-XPE-P3-AI/spec.md) | v1.0.0 | — | Phase 3 AI 모듈 (Should, 미착수) |
| [SPEC-XPE-REG](../.moai/specs/SPEC-XPE-REG/spec.md) | — | — | 규제 준수 |
| [SPEC-XPE-SEC](../.moai/specs/SPEC-XPE-SEC/spec.md) | — | — | 사이버보안 |
| [SPEC-XPE-IOP](../.moai/specs/SPEC-XPE-IOP/spec.md) | — | — | 상호운용성 |
| [SPEC-XPE-OPS](../.moai/specs/SPEC-XPE-OPS/spec.md) | — | — | 운영·PMS |

---

## 2. 정보성 문서 (맥락 및 지침)

개발을 지원하는 참고 자료입니다. 권위가 없으므로 항상 정규 문서를 우선합니다.

### 2.1 검증 및 분석

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [cross-verification-consolidated.md](project/cross-verification-consolidated.md) | XPE-XVER-CONSOLIDATED-001 | v1.0.0 | — | 4라운드 교차 검증의 **통합** 결과. 3개 별도 보고서로부터 병합 |
| [XPE-Module-Reinforcement-Plan.md](project/XPE-Module-Reinforcement-Plan.md) | XPE-REINFORCE-001 | v1.0.0 | 685 | 비정규 R&D 로드맵: 23개 전처리 + 35개 후처리 개선, 혁신 아이디어 |

### 2.2 운영

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [XPE-CI-CD_LocalBuild_Runbook.md](development/XPE-CI-CD_LocalBuild_Runbook.md) | — | v1.0 | 215 | CI/CD 파이프라인 및 로컬 빌드 실행 가이드 |

---

## 3. IEC 62304 규제 패키지

소프트웨어 항목별로 정리된 완전한 수명 주기 문서입니다. 각 패키지는 IEC 62304 Class B 규정 준수를 목표로 합니다.

### 3.1 XPE (X-ray Processing Engine) — 23개 문서

전처리, 향상, 표시 및 DICOM 모듈을 다루는 주요 소프트웨어 항목입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD (Product Requirements) | [xray-postprocessing-prd.md](post-processing/xpe/xray-postprocessing-prd.md) | PRD-001 |
| — | PRD (Execution Detail) | [XPE-PRD-002](post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md) | PRD-002 |
| — | PRD (Backlog) | [XPE-PRD-003](post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | PRD-003 |
| — | Execution Plan | [XPE-PLAN-001](post-processing/xpe/XPE-PLAN-001_Consolidated_Execution_Plan.md) | PLAN-001 |
| 5.1 | Software Development Plan | [XPE-SDP-001](post-processing/xpe/XPE-SDP-001_Software_Development_Plan.md) | SDP-001 |
| 5.2 | Requirements Specification | [XPE-SRS-001](post-processing/xpe/XPE-SRS-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [XPE-SAD-001](post-processing/xpe/XPE-SAD-001_Software_Architecture_Document.md) | SAD-001 |
| 5.4 | Unit Identification | [XPE-SDD-001](post-processing/xpe/XPE-SDD-001_Software_Unit_Identification.md) | SDD-001 |
| 5.4 | Detailed Design | [XPE-SDD-002](post-processing/xpe/XPE-SDD-002_Software_Detailed_Design.md) | SDD-002 |
| 5.5 | Test Plan & Cases | [XPE-STP-001](post-processing/xpe/XPE-STP-001_Software_Test_Plan_and_Cases.md) | STP-001 |
| 5.7 | Verification & Validation Plan | [XPE-VVP-001](post-processing/xpe/XPE-VVP-001_Verification_Validation_Plan.md) | VVP-001 |
| 5.8 | Requirements Traceability | [XPE-RTM-001](post-processing/xpe/XPE-RTM-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| 6.1 | Configuration Management | [XPE-SCM-001](post-processing/xpe/XPE-SCM-001_Configuration_Management_Plan.md) | SCM-001 |
| 7 | Risk Management | [XPE-SRM-001](post-processing/xpe/XPE-SRM-001_Software_Risk_Management_File.md) | SRM-001 |
| 7 | Hazard Analysis | [XPE-SHA-001](post-processing/xpe/XPE-SHA-001_Software_Hazard_Analysis.md) | SHA-001 |
| 8 | SOUP Analysis | [XPE-SOUP-001](post-processing/xpe/XPE-SOUP-001_SOUP_Analysis.md) | SOUP-001 |
| — | Compliance Matrix | [XPE-62304-MAP-001](post-processing/xpe/XPE-62304-MAP-001_Compliance_Matrix.md) | MAP-001 |
| — | Applicability Matrix | [xpe-iec62304-class-b-package.md](post-processing/xpe/xpe-iec62304-class-b-package.md) | PKG-001 |
| 12 | Maintenance Plan | [XPE-SMP-001](post-processing/xpe/XPE-SMP-001_Software_Maintenance_Plan.md) | SMP-001 |
| 9 | Problem Resolution | [XPE-SPR-001](post-processing/xpe/XPE-SPR-001_Problem_Resolution_Process.md) | SPR-001 |
| — | Release Procedure | [XPE-SRP-001](post-processing/xpe/XPE-SRP-001_Software_Release_Procedure.md) | SRP-001 |
| 5.6 | Integration Test Plan | [XPE-ITP-001](post-processing/xpe/XPE-ITP-001_Integration_Test_Plan.md) | ITP-001 |
| 5.4 | Unified Algorithm Spec | [XPE-ALG-001](post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md) | ALG-001 |

> **XPE-ALG-001 v1.7** (IEC 62304 §5.4 Detailed Design): 8 라운드 80회 Review-Evaluate-Fix를 통해 80개 알고리즘 공백(GAP-01~10, GAP-D~N, GAP-O~X, GAP-Y~AH, GAP-AI~AR, GAP-AS~BB, GAP-BC~BL, GAP-BM~BV) 전부 해소. v1.7 신규: DICOM GSDF(§6.5), Multi-Scale Retinex(§6.6), U-Net 폐 분할(§8.5), DLIR CNN(§8.6), 늑골 억제(§8.7), Body Part CNN(§8.8), Lucas-Kanade 광학 흐름(§14.3), 통합 비선형성(§3.16), TV-ADMM §20 신설, BMD proxy(§20.1). 총 11,561줄.

### 3.2 GSVG (Grid Suppression Virtual Grid) — 13개 문서

완전한 IEC 62304 Class B 패키지를 갖춘 독립 소프트웨어 항목입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | Master Package | [GSVG_IEC62304_ClassB_Document_Package.md](post-processing/gsvg/GSVG_IEC62304_ClassB_Document_Package.md) | PKG |
| — | Document Index | [GSVG-PKG-001](post-processing/gsvg/GSVG-PKG-001_Document_Index.md) | PKG-001 |
| 5.1 | Development Plan | [GSVG-SDP-001](post-processing/gsvg/GSVG-SDP-001_Development_Plan.md) | SDP-001 |
| 5.2 | Requirements | [GSVG-SRS-001](post-processing/gsvg/GSVG-SRS-001_Requirements.md) | SRS-001 |
| 5.3 | Architecture | [GSVG-SAD-001](post-processing/gsvg/GSVG-SAD-001_Architecture.md) | SAD-001 |
| 5.4 | Detailed Design | [GSVG-SDD-001](post-processing/gsvg/GSVG-SDD-001_Detailed_Design.md) | SDD-001 |
| 5.7 | Verification Plan | [GSVG-SVP-001](post-processing/gsvg/GSVG-SVP-001_Verification_Plan.md) | SVP-001 |
| 7 | Hazard Analysis | [GSVG-SHA-001](post-processing/gsvg/GSVG-SHA-001_Hazard_Analysis.md) | SHA-001 |
| 8 | SOUP Analysis | [GSVG-SOUP-001](post-processing/gsvg/GSVG-SOUP-001_SOUP_Analysis.md) | SOUP-001 |
| 5.8 | Traceability Matrix | [GSVG-RTM-001](post-processing/gsvg/GSVG-RTM-001_Traceability.md) | RTM-001 |
| — | Image Acquisition Protocol | [IAP-GSVG-001](post-processing/gsvg/IAP-GSVG-001_Image_Acquisition_Protocol.md) | IAP-001 |
| — | Test Dataset Specification | [TDS-GSVG-001](post-processing/gsvg/TDS-GSVG-001_Test_Dataset_Specification.md) | TDS-001 |
| — | Module README | [README.md](post-processing/gsvg/README.md) | REF |

### 3.3 Calibration (전처리 보정 모듈) — 9개 문서

PRE-02~09 Calibration 보정 알고리즘의 완전한 IEC 62304 Class B 수명 주기입니다.

> **빠른 진입**: [KNOWLEDGE-BASE.md](calibration/KNOWLEDGE-BASE.md) — 알고리즘·문서·코드·API 전체 교차 참조 허브  
> **알고리즘 시각화**: [CONCEPT-DIAGRAMS.md](calibration/CONCEPT-DIAGRAMS.md) — 12개 Mermaid 개념도, 처리 흐름도, 상태 기계

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD (Calibration) | [xray-detector-calibration-prd.md](calibration/xray-detector-calibration-prd.md) | PRD |
| — | Module README | [README.md](calibration/README.md) | REF |
| — | **Knowledge Base** | [**KNOWLEDGE-BASE.md**](calibration/KNOWLEDGE-BASE.md) | **KB-001** |
| — | **Concept Diagrams** | [**CONCEPT-DIAGRAMS.md**](calibration/CONCEPT-DIAGRAMS.md) | **DGM-001** |
| 5.2 | Requirements Specification | [SRS-CALIB-001](calibration/SRS-CALIB-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-CALIB-001](calibration/SAD-CALIB-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-CALIB-001](calibration/SHA-CALIB-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-CALIB-001](calibration/RTM-CALIB-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Image Acquisition Protocol | [IAP-CALIB-001](calibration/IAP-CALIB-001_Image_Acquisition_Protocol.md) | IAP-001 |
| — | Test Dataset Specification | [TDS-CALIB-001](calibration/TDS-CALIB-001_Test_Dataset_Specification.md) | TDS-001 |

### 3.4 Ghost Correction (Lag/Ghost 보정) — 9개 문서

PRE-04/05 Lag/Ghost 보정에 대한 완전한 IEC 62304 Class B 수명 주기입니다.

| IEC 62304 Clause | Document Type | Document |
|----------------:|:-------------|---------|
| — | Product Requirements v2 | [sw_lag_correction_prd_v2.md](ghost-correction/sw_lag_correction_prd_v2.md) |
| 5.2 | Requirements Specification | [srs_ghost_correction.md](ghost-correction/srs_ghost_correction.md) |
| 5.3 | Architecture Document | [sad_ghost_correction.md](ghost-correction/sad_ghost_correction.md) |
| 5.4 | Detailed Design | [sdd_ghost_correction.md](ghost-correction/sdd_ghost_correction.md) |
| 5.5 | Test Plan & Cases (50+) | [stp_stc_ghost_correction.md](ghost-correction/stp_stc_ghost_correction.md) |
| 5.8 | Traceability Matrix (30+) | [rtm_ghost_correction.md](ghost-correction/rtm_ghost_correction.md) |
| — | Image Acquisition Protocol | [IAP-GHOST-001](ghost-correction/IAP-GHOST-001_Image_Acquisition_Protocol.md) |
| — | Test Dataset Specification | [TDS-GHOST-001](ghost-correction/TDS-GHOST-001_Test_Dataset_Specification.md) |
| — | Module README | [README.md](ghost-correction/README.md) |

### 3.5 Panel Defect (패널 불량 보정) — 9개 문서

PRE-06 패널 불량 감지 및 보정 (RMM + ANN + 선 결함)의 완전한 IEC 62304 Class B 패키지입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xray-panel-defect-prd.md](panel-defect/xray-panel-defect-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-DEFECT-001](panel-defect/SRS-DEFECT-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-DEFECT-001](panel-defect/SAD-DEFECT-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-DEFECT-001](panel-defect/SHA-DEFECT-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-DEFECT-001](panel-defect/RTM-DEFECT-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Image Acquisition Protocol | [IAP-DEFECT-001](panel-defect/IAP-DEFECT-001_Image_Acquisition_Protocol.md) | IAP-001 |
| — | Test Dataset Specification | [TDS-DEFECT-001](panel-defect/TDS-DEFECT-001_Test_Dataset_Specification.md) | TDS-001 |
| — | Module README | [README.md](panel-defect/README.md) | REF |
| — | Document Index | [INDEX.md](panel-defect/INDEX.md) | IDX |

### 3.6 Enhance Basic (기본 향상 모듈) — 9개 문서

Phase 1b `xpe_enhance_basic.dll` (Log Transform, CLAHE, Window/Level, EI Baseline)의 완전한 IEC 62304 Class B 패키지입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xpe-enhance-basic-prd.md](enhance-basic/xpe-enhance-basic-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-ENHANCE-BASIC-001](enhance-basic/SRS-ENHANCE-BASIC-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-ENHANCE-BASIC-001](enhance-basic/SAD-ENHANCE-BASIC-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-ENHANCE-BASIC-001](enhance-basic/SHA-ENHANCE-BASIC-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-ENHANCE-BASIC-001](enhance-basic/RTM-ENHANCE-BASIC-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Image Acquisition Protocol | [IAP-ENHANCE-BASIC-001](enhance-basic/IAP-ENHANCE-BASIC-001_Image_Acquisition_Protocol.md) | IAP-001 |
| — | Test Dataset Specification | [TDS-ENHANCE-BASIC-001](enhance-basic/TDS-ENHANCE-BASIC-001_Test_Dataset_Specification.md) | TDS-001 |
| — | Module README | [README.md](enhance-basic/README.md) | REF |
| — | Document Manifest | [MANIFEST.md](enhance-basic/MANIFEST.md) | MNF |

### 3.7 Enhance Advanced (고급 향상 모듈) — 8개 문서

Phase 2 `xpe_enhance_advanced.dll` (4계층 노이즈 감소, 엣지 강조, Hough 조명 감지, EI ROI 보정)의 완전한 IEC 62304 Class B 패키지입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xpe-enhance-advanced-prd.md](enhance-advanced/xpe-enhance-advanced-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-ENHANCE-ADV-001](enhance-advanced/SRS-ENHANCE-ADV-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-ENHANCE-ADV-001](enhance-advanced/SAD-ENHANCE-ADV-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-ENHANCE-ADV-001](enhance-advanced/SHA-ENHANCE-ADV-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-ENHANCE-ADV-001](enhance-advanced/RTM-ENHANCE-ADV-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Image Acquisition Protocol | [IAP-ENHANCE-ADV-001](enhance-advanced/IAP-ENHANCE-ADV-001_Image_Acquisition_Protocol.md) | IAP-001 |
| — | Test Dataset Specification | [TDS-ENHANCE-ADV-001](enhance-advanced/TDS-ENHANCE-ADV-001_Test_Dataset_Specification.md) | TDS-001 |
| — | Module README | [README.md](enhance-advanced/README.md) | REF |

### 3.8 AI Module (AI 추론 모듈) — 6개 문서 ✨ NEW

Phase 3 `xpe_ai.dll` + `xpe_ai_worker.exe` (신체부위 인식, AI 조명 정제, 이미지 스티칭, 뼈 억제, DL 디노이저)의 완전한 IEC 62304 Class B 패키지입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xpe-ai-prd.md](ai-module/xpe-ai-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-AI-001](ai-module/SRS-AI-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-AI-001](ai-module/SAD-AI-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-AI-001](ai-module/SHA-AI-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-AI-001](ai-module/RTM-AI-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Module README | [README.md](ai-module/README.md) | REF |

### 3.9 Display (표시 처리 모듈) — 6개 문서 ✨ NEW

Phase 1b `xpe_display.dll` (Modality LUT, VOI LUT, GSDF/Presentation LUT, LUT 관리자)의 완전한 IEC 62304 Class B 패키지입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xpe-display-prd.md](display/xpe-display-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-DISPLAY-001](display/SRS-DISPLAY-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-DISPLAY-001](display/SAD-DISPLAY-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-DISPLAY-001](display/SHA-DISPLAY-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-DISPLAY-001](display/RTM-DISPLAY-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Module README | [README.md](display/README.md) | REF |

### 3.10 DICOM I/O (DICOM 입출력 모듈) — 6개 문서 ✨ NEW

Phase 1b `xpe_dicom.dll` (DICOM Reader/Writer, J2K, GSPS, 네트워크 SCU)의 완전한 IEC 62304 Class B 패키지입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xpe-dicom-prd.md](dicom/xpe-dicom-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-DICOM-001](dicom/SRS-DICOM-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-DICOM-001](dicom/SAD-DICOM-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-DICOM-001](dicom/SHA-DICOM-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-DICOM-001](dicom/RTM-DICOM-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Module README | [README.md](dicom/README.md) | REF |

### 3.11 Common Infrastructure (공통 인프라 모듈) — 6개 문서 ✨ NEW

Layer 0 `xpe_common.dll` (MemoryPool, Pack=8 TypeDef, ErrorHandler, XPE Event System, JsonConfig, ParameterValidator)의 완전한 IEC 62304 Class B 패키지입니다. `AED` 약어는 detector Auto Exposure Detection 용도로만 예약합니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD | [xpe-common-prd.md](common/xpe-common-prd.md) | PRD |
| 5.2 | Requirements Specification | [SRS-COMMON-001](common/SRS-COMMON-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-COMMON-001](common/SAD-COMMON-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-COMMON-001](common/SHA-COMMON-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-COMMON-001](common/RTM-COMMON-001_Requirements_Traceability_Matrix.md) | RTM-001 |
| — | Module README | [README.md](common/README.md) | REF |

---

## 4. 연구 및 사전 개발

도메인 연구, 방법론 연구 및 초기 단계 분석입니다. 아직 SPEC으로 공식화되지 않았습니다.

### 4.1 Calibration 모듈

> IEC 62304 Class B 패키지 (§3.3)로 승격되었습니다. SRS, SAD, SHA, RTM 문서가 완성되었습니다.

| Document | Lines | Description |
|----------|:-----:|-------------|
| [README.md](calibration/README.md) | 778 | xpe_preprocess.dll 아키텍처 — 9단계 파이프라인, 18개 C ABI 함수 |
| [xray-detector-calibration-prd.md](calibration/xray-detector-calibration-prd.md) | 3,588 | Calibration 알고리즘 요구사항(offset, gain, defect, lag, scatter) |

### 4.2 Panel Defect 알고리즘

> IEC 62304 Class B 패키지 (§3.5)로 승격되었습니다. 9개 문서 완성.

| Document | Lines | Description |
|----------|:-----:|-------------|
| [plan.md](panel-defect-algorithm/plan.md) | 542 | 나쁜 픽셀/클러스터 감지, 선 결함, 3가지 보정 프로필 (원본 R&D 계획) |

### 4.3 품질 평가 모듈 (제안)

`xpe_quality_eval.dll`을 위한 3계층 측정-분석-관리 아키텍처입니다.

| Document | Layer | Lines | Description |
|----------|-------|:-----:|-------------|
| [01_Noise_...pplx.md](quality-eval/01_Noise_평가_방법론_종합보고서.pplx.md) | WHY | 2,056 | IEC 62220-1 노이즈 평가, 12개 메트릭(DQE/MTF/NPS/NEQ/SNR) |
| [02_양산라인_...pplx.md](quality-eval/02_양산라인_계측방법론_가이드.pplx.md) | HOW | 2,051 | 생산 라인 테스트, 12개 항목, 5단계 계층 |
| [03_측정_...pplx.md](quality-eval/03_측정_알고리즘_명세서.pplx.md) | WHAT | 4,084 | NPS/MTF/DQE/Defect Python 구현 명세 |
| [05_체계적_...pplx.md](quality-eval/05_체계적_관리방안_문서.pplx.md) | WHERE/WHEN | 1,800 | SPC, KPI(Cpk>=1.33), CAPA 프레임워크 |

> 상태: 공식 개발 시작을 위한 SPEC 생성 대기(`/moai plan`).

### 4.4 외부 참고 자료

| Document | Lines | Description |
|----------|:-----:|-------------|
| [xray_fpd_tech_classification_final.md](references/xray_fpd_tech_classification_final.md) | 661 | 50개 이상의 논문, 30개 이상의 특허, 12개 공급업체 — Must-Have 대 Differentiator 분류 |
| [04_분석SW_Tool_비교_추천보고서.pplx.md](references/04_분석SW_Tool_비교_추천보고서.pplx.md) | 2,679 | 오픈소스/상용 분석 도구 비교 |
| [xray_grid_suppression_virtual_grid_research.md](references/xray_grid_suppression_virtual_grid_research.md) | 268 | Grid 아티팩트 제거 알고리즘 배경 |

---

## 5. Archive (대체됨 / 역사적)

여기로 이동된 문서는 통합 또는 업데이트된 버전으로 대체되었습니다. 감시 추적을 위해서만 보관됩니다.

| Document | Superseded By | Reason |
|----------|--------------|--------|
| [cross-verification-report-2026-04-13.md](archive/verification-history/cross-verification-report-2026-04-13.md) | cross-verification-consolidated.md | Round 1 결과가 통합 보고서로 병합 |
| [SPEC-XPE-MASTER-verification.md](archive/verification-history/SPEC-XPE-MASTER-verification.md) | cross-verification-consolidated.md | Round 2-3 결과가 통합 보고서로 병합 |
| [DEEP-QUANTITATIVE-CROSS-VERIFICATION-ROUND-4.md](archive/verification-history/DEEP-QUANTITATIVE-CROSS-VERIFICATION-ROUND-4.md) | cross-verification-consolidated.md | Round 4 결과가 통합 보고서로 병합 |
| [XPE-Implementation-Analysis-Report.md](archive/superseded/XPE-Implementation-Analysis-Report.md) | product.md + pipeline-spec.md + SPEC-XPE-MASTER.md | 파생 문서 — 정규 출처로부터 중복된 내용 |

---

## 6. IEC 62304 추적성 매트릭스 (교차 참고)

빠른 검색: 각 소프트웨어 항목에 대해 어느 IEC 62304 조항이 어느 문서로 다루어지는가.

| IEC 62304 Clause | Description | XPE | GSVG | Ghost | Calibration | Panel Defect | Enhance Basic | Enhance Adv | AI Module | Display | DICOM | Common |
|:-----------------:|-------------|:---:|:----:|:-----:|:-----------:|:------------:|:-------------:|:-----------:|:---------:|:-------:|:-----:|:------:|
| 5.1 | Development Plan | SDP-001 | SDP-001 | — | — | — | — | — | — | — | — | — |
| 5.2 | Requirements | SRS-001 | SRS-001 | SRS | SRS-001 | SRS-001 | SRS-001 | SRS-001 | SRS-001 | SRS-001 | SRS-001 | SRS-001 |
| 5.3 | Architecture | SAD-001 | SAD-001 | SAD | SAD-001 | SAD-001 | SAD-001 | SAD-001 | SAD-001 | SAD-001 | SAD-001 | SAD-001 |
| 5.4 | Detailed Design | SDD-001/002 | SDD-001 | SDD | — | — | — | — | — | — | — | — |
| 5.5 | Test Plan | STP-001 | — | STP/STC | — | — | — | — | — | — | — | — |
| 5.6 | Integration | ITP-001 | SVP-001 | — | — | — | — | — | — | — | — | — |
| 5.7 | Verification | VVP-001 | SVP-001 | — | — | — | — | — | — | — | — | — |
| 5.8 | Traceability | RTM-001 | RTM-001 | RTM | RTM-001 | RTM-001 | RTM-001 | RTM-001 | RTM-001 | RTM-001 | RTM-001 | RTM-001 |
| 6.1 | Config Mgmt | SCM-001 | — | — | — | — | — | — | — | — | — | — |
| 7 | Risk/Hazard | SRM+SHA | SHA-001 | — | SHA-001 | SHA-001 | SHA-001 | SHA-001 | SHA-001 | SHA-001 | SHA-001 | SHA-001 |
| 8 | SOUP | SOUP-001 | SOUP-001 | — | — | — | — | — | — | — | — | — |
| 9 | Problem Res. | SPR-001 | — | — | — | — | — | — | — | — | — | — |
| 12 | Maintenance | SMP-001 | — | — | — | — | — | — | — | — | — | — |
| — | IAP | — | IAP-001 | IAP-001 | IAP-001 | IAP-001 | IAP-001 | IAP-001 | — | — | — | — |
| — | TDS | — | TDS-001 | TDS-001 | TDS-001 | TDS-001 | TDS-001 | TDS-001 | — | — | — | — |

### 적용 범위 요약

| Software Item | Documents | IEC 62304 Coverage |
|--------------|:---------:|:------------------:|
| XPE (시스템 레벨) | 22 | Complete (Class B full package) |
| GSVG | 13 | Complete + IAP/TDS |
| Ghost Correction | 9 | Complete + IAP/TDS/README |
| Calibration | 8 | Complete + IAP/TDS |
| Panel Defect | 9 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README+INDEX) |
| Enhance Basic | 9 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README+MANIFEST) |
| Enhance Advanced | 8 | Complete (PRD+SRS+SAD+SHA+RTM+IAP+TDS+README) |
| AI Module | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) |
| Display | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) |
| DICOM I/O | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) |
| Common Infrastructure | 6 | Complete (PRD+SRS+SAD+SHA+RTM+README) |
| Quality Evaluation | 4 | Pending (methodology ready, SPEC needed) |

---

## 7. 문서 통계

| Category | Count | Total Lines (approx) |
|----------|:-----:|:--------------------:|
| Normative (Section 1) | 9 | ~5,680 |
| Informational (Section 2) | 3 | ~900 |
| IEC 62304 XPE System (Section 3.1) | 23 | ~10,005 |
| IEC 62304 GSVG (Section 3.2) | 13 | ~3,868 |
| IEC 62304 Ghost Correction (Section 3.3) | 9 | ~4,552 |
| IEC 62304 Calibration (Section 3.4) | 8 | ~5,200 |
| IEC 62304 Panel Defect (Section 3.5) | 9 | ~4,500 |
| IEC 62304 Enhance Basic (Section 3.6) | 9 | ~2,600 |
| IEC 62304 Enhance Advanced (Section 3.7) | 8 | ~2,300 |
| IEC 62304 AI Module (Section 3.8) | 6 | ~2,592 |
| IEC 62304 Display (Section 3.9) | 6 | ~2,704 |
| IEC 62304 DICOM I/O (Section 3.10) | 6 | ~2,700 |
| IEC 62304 Common Infra (Section 3.11) | 6 | ~3,930 |
| Research (Section 4) | 11 | ~14,310 |
| Archive (Section 5) | 4 | ~1,158 |
| **Total** | **~132** | **~67,699** |

---

## 8. AI 에이전트용

AI 에이전트가 이 프로젝트를 이해할 때 읽어야 할 문서 순서:

1. **이 README.md** — 문서 전체 구조 파악
2. **product.md** — 제품 정의, 컴포넌트, Phase 전략
3. **structure.md** — 코드 위치, DLL 매핑
4. **tech.md** — 기술 스택, ABI 규칙
5. **SPEC-XPE-MASTER.md** — 구현 계획, SWU 카운트
6. **pipeline-spec.md** — 파이프라인 순서 (필요 시)
7. **api-spec.md** — API 상세 (구현 시)

> `.moai/project/` 디렉토리에도 product.md, structure.md, tech.md, api-spec.md, pipeline-spec.md의 사본이 존재합니다.
> **정본(Normative)은 `docs/project/`** 입니다. `.moai/project/`는 MoAI 프레임워크의 작업 사본으로, 최신성이 보장되지 않습니다.

---

## 9. 문서 버전 관리 규칙

| Pattern | Example | Meaning |
|---------|---------|---------|
| Major.Minor.Patch | v2.0.0 | Major=구조 변경, Minor=내용 추가, Patch=오타 수정 |
| -dsN suffix | v3.0.0-ds2 | DeepSync 적용 회차 |
| vN.N.N-draft | v1.0.0-draft | 검토 대기 |

---

## Change Log

| Date | Version | Changes |
|------|---------|---------|
| 2026-04-22 | 3.5.0 | **SPEC-BENCH-PRE/POST + SPEC-XPE-GSVG v1.0.0 추가** (Section 1.4 신규). api-spec v1.4.0 (AED 제거, 79개 함수). SPEC-XPE-MASTER v3.0.0. SPEC 문서 15종 인덱스 Section 1.4 추가. |
| 2026-04-15 | 3.4.0 | XPE-ALG-001 v1.7 반영 (80건 GAP 해소). GAP-BM~BV: DICOM GSDF, Multi-Scale Retinex, U-Net 폐 분할, DLIR CNN, 늑골 억제, Body Part CNN, Lucas-Kanade 광학 흐름, 통합 비선형성, TV-ADMM §20 신설, BMD proxy. |
| 2026-04-15 | 3.3.0 | XPE-ALG-001 v1.6 반영 (70건 GAP 해소). GAP-BC~BL: DAP/KERMA, JPEG2000, 모션블러 위너, 금속 마스크, 토모합성 FBP/SAA, RANSAC 스티칭, 라플라시안 피라미드, GPU CUDA, 팬텀 인식, Cross-FPD 정규화 신설. |
| 2026-04-15 | 3.2.0 | XPE-ALG-001 v1.5 반영 (60건 GAP 해소). GAP-AS~BB: 지각적 IQM, 온도 보상, FFT 노치, AEC 피드백, SPC, ECC 정합, 양자 잡음 모델, 무아레 제거, DICOM SR, IEC61223 인수 시험 신설. |
| 2026-04-15 | v3.2.0 (old) | **XPE-ALG-001 v1.2 Round 3 완료** (GAP-O~X 해소 10건): Heel Effect, Multi-SID Gain, Session Lock, Quality State Sidecar(§13 신설), Parity Harness, MTF ESF 완전 구현, Lag Tiering, VG Anatomy Presets(15개 부위), AI Worker Isolation(ONNX), Drift Monitor. `post-processing/xpe/README.md` v1.1.0 반영. |
| 2026-04-15 | v3.1.0 | **XPE-ALG-001 통합 알고리즘 명세 추가** (ALG-001): GAP-D/E/F/G/H/I/J/L/M/N 해소, Readout Validation, Non-linearity Correction, Auto Exposure Detection (AED-0), NPS/DQE/Collimation 알고리즘 추가. xpe-algorithm-spec-deepsync 버전 참조 v3.0.0-ds2 → v3.2.0-ds4 수정. `post-processing/xpe/README.md` 신규. 총 ~132개 문서, ~67,699줄 |
| 2026-04-14 | v3.0.0 | **전체 모듈 문서 패키지 완성**: Panel Defect(9), Enhance Basic(9), Enhance Advanced(8), AI Module(6), Display(6), DICOM(6), Common(6) 추가. GSVG·Ghost·Calibration IAP/TDS/README 보강. 섹션 3.5~3.11 신규. IEC 62304 매트릭스 11개 항목으로 확장. 총 129개 문서, ~64,599줄 |
| 2026-04-14 | v2.0.0 | Hybrid 3-Tier + IEC 62304 재편성. Archive 분리. 검증 보고서 통합. Normative Authority Table 추가 |
| 2026-04-14 | v1.0.0 | 초기 문서 인덱스 |
