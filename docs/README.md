# X-ray Image Processing Engine - Documentation System

**Version**: 2.0.0  
**Last Updated**: 2026-04-14  
**Organization**: Hybrid 3-Tier (Normative/Informational/Archive) + IEC 62304 Traceability  

---

## 이 인덱스 사용 방법

- **Normative** 문서는 단일 정보 출처(SSoT)입니다. 정보가 충돌할 때는 정규 문서가 우선합니다.
- **Informational** 문서는 맥락, 분석 또는 구현 지침을 제공합니다. 정규 문서를 참고하지만 절대 이를 무시하지 않습니다.
- **Archive** 문서는 대체되었거나 역사적입니다. 감시 추적을 위해서만 보관됩니다.
- **IEC 62304** 패키지는 소프트웨어 항목(XPE, GSVG, Ghost Correction)별로 정리된 규제 전달물입니다.

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
| [api-spec.md](project/api-spec.md) | XPE-API-SPEC-001 | v1.2.0 | 1,469 | 8개 DLL에 걸친 82개 내보낸 C ABI 함수, 일반적인 타입, P/Invoke 정렬 |
| [xpe-algorithm-spec-deepsync.md](project/xpe-algorithm-spec-deepsync.md) | ALG-SPEC-001 | v3.0.0-ds2 | 573 | 알고리즘 계약: DeepSync 결정, 연구 검증 모델, 품질 게이트, EI-0 해결 |

### 1.3 구현 계획

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [SPEC-XPE-MASTER.md](project/SPEC-XPE-MASTER.md) | SPEC-XPE-MASTER | v2.0.0 | 495 | 마스터 계획: 43개 SWU, Phase 0-3, 교차 검증 요약, 문서 업데이트 매트릭스 |
| [sprint-plan.md](project/sprint-plan.md) | XPE-SPRINT-PLAN-001 | v1.2.0 | 1,430 | 28개 sprint, 종속성 그래프, sprint별 범위/API/테스트 대상, 로깅/Alert 검증 기준 |
| [xpe-implementation-reference.md](project/xpe-implementation-reference.md) | XPE-IMPL-REF-001 | v1.1.0 | 950 | Calibration 바이너리, 로깅/Alert JSON(§9), LUT 형식(§10), GSDF(§11), IPC(§12), 양자화(§13), session_id(§14) |

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

### 3.1 XPE (X-ray Processing Engine) — 22개 문서

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

### 3.2 GSVG (Grid Suppression Virtual Grid) — 10개 문서

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

### 3.3 Calibration (전처리 보정 모듈) — 6개 문서

PRE-02~09 Calibration 보정 알고리즘의 완전한 IEC 62304 Class B 수명 주기입니다.

| IEC 62304 Clause | Document Type | Document | ID |
|----------------:|:-------------|---------|-----|
| — | PRD (Calibration) | [xray-detector-calibration-prd.md](calibration/xray-detector-calibration-prd.md) | PRD |
| — | Architecture Reference | [README.md](calibration/README.md) | REF |
| 5.2 | Requirements Specification | [SRS-CALIB-001](calibration/SRS-CALIB-001_Software_Requirements_Specification.md) | SRS-001 |
| 5.3 | Architecture Document | [SAD-CALIB-001](calibration/SAD-CALIB-001_Software_Architecture_Document.md) | SAD-001 |
| 7 | Hazard Analysis | [SHA-CALIB-001](calibration/SHA-CALIB-001_Software_Hazard_Analysis.md) | SHA-001 |
| 5.8 | Traceability Matrix | [RTM-CALIB-001](calibration/RTM-CALIB-001_Requirements_Traceability_Matrix.md) | RTM-001 |

### 3.4 Ghost Correction (Lag/Ghost) — 6개 문서

PRE-04/05 Lag/Ghost 보정에 대한 완전한 IEC 62304 Class B 수명 주기입니다.

| IEC 62304 Clause | Document Type | Document |
|----------------:|:-------------|---------|
| — | Product Requirements v2 | [sw_lag_correction_prd_v2.md](ghost-correction/sw_lag_correction_prd_v2.md) |
| 5.2 | Requirements Specification | [srs_ghost_correction.md](ghost-correction/srs_ghost_correction.md) |
| 5.3 | Architecture Document | [sad_ghost_correction.md](ghost-correction/sad_ghost_correction.md) |
| 5.4 | Detailed Design | [sdd_ghost_correction.md](ghost-correction/sdd_ghost_correction.md) |
| 5.5 | Test Plan & Cases (50+) | [stp_stc_ghost_correction.md](ghost-correction/stp_stc_ghost_correction.md) |
| 5.8 | Traceability Matrix (30+) | [rtm_ghost_correction.md](ghost-correction/rtm_ghost_correction.md) |

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

| Document | Lines | Description |
|----------|:-----:|-------------|
| [plan.md](panel-defect-algorithm/plan.md) | 542 | 나쁜 픽셀/클러스터 감지, 선 결함, 3가지 보정 프로필 |

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

| IEC 62304 Clause | Description | XPE | GSVG | Ghost | Calibration |
|:-----------------:|-------------|:---:|:----:|:-----:|:-----------:|
| 5.1 | Development Plan | SDP-001 | SDP-001 | — | — |
| 5.2 | Requirements | SRS-001 | SRS-001 | SRS | SRS-001 |
| 5.3 | Architecture | SAD-001 | SAD-001 | SAD | SAD-001 |
| 5.4 | Detailed Design | SDD-001/002 | SDD-001 | SDD | — |
| 5.5 | Unit Implementation | (code) | (code) | (code) | (code) |
| 5.5 | Test Plan | STP-001 | — | STP/STC | — |
| 5.6 | Integration | ITP-001, VVP-001 | SVP-001 | — | — |
| 5.7 | Verification | VVP-001 | SVP-001 | — | — |
| 5.8 | Traceability | RTM-001 | RTM-001 | RTM | RTM-001 |
| 6.1 | Configuration Mgmt | SCM-001 | — | — | — |
| 7 | Risk Management | SRM-001, SHA-001 | SHA-001 | — | SHA-001 |
| 8 | SOUP | SOUP-001 | SOUP-001 | — | — |
| 9 | Problem Resolution | SPR-001 | — | — | — |
| 12 | Maintenance | SMP-001 | — | — | — |

### 적용 범위 요약

| Software Item | Documents | IEC 62304 Coverage |
|--------------|:---------:|:------------------:|
| XPE | 21 | Complete (Class B full package) |
| GSVG | 10 | Complete (Class B full package) |
| Ghost Correction | 6 | Complete (PRD-SRS-SAD-SDD-STP-RTM) |
| Calibration | 2 | Partial (PRD + Architecture) |
| Panel Defect | 1 | R&D Plan only |
| Quality Evaluation | 4 | Pending (methodology ready, SPEC needed) |

---

## 7. 문서 통계

| Category | Count | Total Lines |
|----------|:-----:|:-----------:|
| Normative (Section 1) | 9 | ~5,680 |
| Informational (Section 2) | 3 | ~900 |
| IEC 62304 XPE (Section 3.1) | 21 | ~7,805 |
| IEC 62304 GSVG (Section 3.2) | 10 | ~1,441 |
| IEC 62304 Ghost (Section 3.3) | 6 | ~2,126 |
| Research (Section 4) | 11 | ~14,310 |
| Archive (Section 5) | 4 | ~1,158 |
| **Total** | **64** | **~33,420** |

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
| 2026-04-14 | v2.0.0 | Hybrid 3-Tier + IEC 62304 재편성. Archive 분리. 검증 보고서 통합. Normative Authority Table 추가 |
| 2026-04-14 | v1.0.0 | 초기 문서 인덱스 |
