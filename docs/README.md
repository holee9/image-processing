# X-ray Image Processing Engine - Documentation System

**Version**: 2.0.0  
**Last Updated**: 2026-04-14  
**Organization**: Hybrid 3-Tier (Normative/Informational/Archive) + IEC 62304 Traceability  

---

## How to Use This Index

- **Normative** documents are the Single Source of Truth (SSoT). When information conflicts, normative documents win.
- **Informational** documents provide context, analysis, or implementation guidance. They reference normative documents but never override them.
- **Archive** documents are superseded or historical. Kept for audit trail only.
- **IEC 62304** packages are regulatory deliverables organized by software item (XPE, GSVG, Ghost Correction).

### Normative Authority Table

| Topic | Normative Document | Scope |
|-------|-------------------|-------|
| Product definition, components, phases | [product.md](project/product.md) | WHAT we build |
| Repository structure, DLL mapping | [structure.md](project/structure.md) | WHERE code lives |
| Technology stack, dependencies, ABI | [tech.md](project/tech.md) | WITH WHAT we build |
| Pipeline stage ordering, bypass rules | [pipeline-spec.md](project/pipeline-spec.md) | HOW stages execute |
| C ABI function signatures | [api-spec.md](project/api-spec.md) | API contract |
| Algorithm behavior, quality gates | [xpe-algorithm-spec-deepsync.md](project/xpe-algorithm-spec-deepsync.md) | Algorithm contract |
| SWU counts, Phase assignments | [SPEC-XPE-MASTER.md](project/SPEC-XPE-MASTER.md) | Master implementation plan |
| Sprint decomposition | [sprint-plan.md](project/sprint-plan.md) | Execution schedule |
| Implementation details (binary format, JSON schema) | [xpe-implementation-reference.md](project/xpe-implementation-reference.md) | Developer reference |

> When you find conflicting information across documents, the Normative Document listed above is authoritative.

---

## 1. Normative Documents (Single Source of Truth)

Core specifications that define the project. Changes to these documents trigger downstream updates.

### 1.1 Product Foundation

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [product.md](project/product.md) | — | v1.0 | 94 | Product identity, components (Pre/Post/Support), Must-Have vs Differentiator strategy, target users |
| [structure.md](project/structure.md) | — | v1.0 | 78 | Repository layout, Module-to-DLL mapping (38 native SWU), dependency direction rules |
| [tech.md](project/tech.md) | — | v1.0 | 136 | C++17/C#/.NET 8 stack, SOUP dependencies (8 XPE + 5 GSVG), platform targets, C ABI design, HW/SW strategy |

### 1.2 Technical Specifications

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [pipeline-spec.md](project/pipeline-spec.md) | PIPE-SPEC-001 | v1.3.0 | 666 | 17-stage pipeline sequence, pre-processing dependency graph, bypass policy, data flow |
| [api-spec.md](project/api-spec.md) | XPE-API-SPEC-001 | v1.2.0 | 1,469 | 82 exported C ABI functions across 8 DLLs, common types, P/Invoke alignment |
| [xpe-algorithm-spec-deepsync.md](project/xpe-algorithm-spec-deepsync.md) | ALG-SPEC-001 | v3.0.0-ds2 | 573 | Algorithm contract: DeepSync decisions, research-validated models, quality gates, EI-0 resolution |

### 1.3 Implementation Planning

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [SPEC-XPE-MASTER.md](project/SPEC-XPE-MASTER.md) | SPEC-XPE-MASTER | v2.0.0 | 495 | Master plan: 43 SWU, Phase 0-3, cross-verification summary, document update matrix |
| [sprint-plan.md](project/sprint-plan.md) | XPE-SPRINT-PLAN-001 | v1.1.0 | 1,410 | 28 sprints, dependency graph, per-sprint scope/API/test targets, rollback strategy |
| [xpe-implementation-reference.md](project/xpe-implementation-reference.md) | XPE-IMPL-REF-001 | v1.0.0 | 759 | Calibration binary format, JSON config schemas, body-part lookup tables, error codes |

---

## 2. Informational Documents (Context & Guidance)

Reference material that supports development. Not authoritative — always defer to normative documents.

### 2.1 Verification & Analysis

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [cross-verification-consolidated.md](project/cross-verification-consolidated.md) | XPE-XVER-CONSOLIDATED-001 | v1.0.0 | — | **Consolidated** findings from 4 rounds of cross-verification. Merged from 3 separate reports |
| [XPE-Module-Reinforcement-Plan.md](project/XPE-Module-Reinforcement-Plan.md) | XPE-REINFORCE-001 | v1.0.0 | 685 | Non-normative R&D roadmap: 23 pre-processing + 35 post-processing improvements, innovation ideas |

### 2.2 Operations

| Document | ID | Version | Lines | Description |
|----------|-----|---------|:-----:|-------------|
| [XPE-CI-CD_LocalBuild_Runbook.md](development/XPE-CI-CD_LocalBuild_Runbook.md) | — | v1.0 | 215 | CI/CD pipeline and local build execution guide |

---

## 3. IEC 62304 Regulatory Packages

Complete lifecycle documentation organized by software item. Each package targets IEC 62304 Class B compliance.

### 3.1 XPE (X-ray Processing Engine) — 21 Documents

Primary software item covering pre-processing, enhancement, display, and DICOM modules.

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

### 3.2 GSVG (Grid Suppression Virtual Grid) — 10 Documents

Independent software item with complete IEC 62304 Class B package.

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

### 3.3 Ghost Correction (Lag/Ghost) — 6 Documents

Complete IEC 62304 Class B lifecycle for PRE-04/05 Lag/Ghost correction.

| IEC 62304 Clause | Document Type | Document |
|----------------:|:-------------|---------|
| — | Product Requirements v2 | [sw_lag_correction_prd_v2.md](ghost-correction/sw_lag_correction_prd_v2.md) |
| 5.2 | Requirements Specification | [srs_ghost_correction.md](ghost-correction/srs_ghost_correction.md) |
| 5.3 | Architecture Document | [sad_ghost_correction.md](ghost-correction/sad_ghost_correction.md) |
| 5.4 | Detailed Design | [sdd_ghost_correction.md](ghost-correction/sdd_ghost_correction.md) |
| 5.5 | Test Plan & Cases (50+) | [stp_stc_ghost_correction.md](ghost-correction/stp_stc_ghost_correction.md) |
| 5.8 | Traceability Matrix (30+) | [rtm_ghost_correction.md](ghost-correction/rtm_ghost_correction.md) |

---

## 4. Research & Pre-Development

Domain research, methodology studies, and early-stage analysis. Not yet formalized into SPECs.

### 4.1 Calibration Module

| Document | Lines | Description |
|----------|:-----:|-------------|
| [README.md](calibration/README.md) | 778 | xpe_preprocess.dll architecture — 9-stage pipeline, 18 C ABI functions |
| [xray-detector-calibration-prd.md](calibration/xray-detector-calibration-prd.md) | 3,588 | Calibration algorithm requirements (offset, gain, defect, lag, scatter) |

### 4.2 Panel Defect Algorithm

| Document | Lines | Description |
|----------|:-----:|-------------|
| [plan.md](panel-defect-algorithm/plan.md) | 542 | Bad pixel/cluster detection, line defects, 3 correction profiles |

### 4.3 Quality Evaluation Module (Proposed)

Three-layer measurement-analysis-management architecture for `xpe_quality_eval.dll`.

| Document | Layer | Lines | Description |
|----------|-------|:-----:|-------------|
| [01_Noise_...pplx.md](quality-eval/01_Noise_평가_방법론_종합보고서.pplx.md) | WHY | 2,056 | IEC 62220-1 noise evaluation, 12 metrics (DQE/MTF/NPS/NEQ/SNR) |
| [02_양산라인_...pplx.md](quality-eval/02_양산라인_계측방법론_가이드.pplx.md) | HOW | 2,051 | Production line testing, 12 items, 5-level hierarchy |
| [03_측정_...pplx.md](quality-eval/03_측정_알고리즘_명세서.pplx.md) | WHAT | 4,084 | NPS/MTF/DQE/Defect Python implementation specs |
| [05_체계적_...pplx.md](quality-eval/05_체계적_관리방안_문서.pplx.md) | WHERE/WHEN | 1,800 | SPC, KPI (Cpk>=1.33), CAPA framework |

> Status: Awaiting SPEC creation (`/moai plan`) for formal development kickoff.

### 4.4 External References

| Document | Lines | Description |
|----------|:-----:|-------------|
| [xray_fpd_tech_classification_final.md](references/xray_fpd_tech_classification_final.md) | 661 | 50+ papers, 30+ patents, 12 vendors — Must-Have vs Differentiator classification |
| [04_분석SW_Tool_비교_추천보고서.pplx.md](references/04_분석SW_Tool_비교_추천보고서.pplx.md) | 2,679 | Open-source/commercial analysis tool comparison |
| [xray_grid_suppression_virtual_grid_research.md](references/xray_grid_suppression_virtual_grid_research.md) | 268 | Grid artifact removal algorithm background |

---

## 5. Archive (Superseded / Historical)

Documents moved here have been superseded by consolidated or updated versions. Kept for audit trail.

| Document | Superseded By | Reason |
|----------|--------------|--------|
| [cross-verification-report-2026-04-13.md](archive/verification-history/cross-verification-report-2026-04-13.md) | cross-verification-consolidated.md | Round 1 findings merged into consolidated report |
| [SPEC-XPE-MASTER-verification.md](archive/verification-history/SPEC-XPE-MASTER-verification.md) | cross-verification-consolidated.md | Round 2-3 findings merged into consolidated report |
| [DEEP-QUANTITATIVE-CROSS-VERIFICATION-ROUND-4.md](archive/verification-history/DEEP-QUANTITATIVE-CROSS-VERIFICATION-ROUND-4.md) | cross-verification-consolidated.md | Round 4 findings merged into consolidated report |
| [XPE-Implementation-Analysis-Report.md](archive/superseded/XPE-Implementation-Analysis-Report.md) | product.md + pipeline-spec.md + SPEC-XPE-MASTER.md | Derivative document — content duplicated from normative sources |

---

## 6. IEC 62304 Traceability Matrix (Cross-Reference)

Quick lookup: Which IEC 62304 clause is covered by which document, for each software item.

| IEC 62304 Clause | Description | XPE | GSVG | Ghost |
|:-----------------:|-------------|:---:|:----:|:-----:|
| 5.1 | Development Plan | SDP-001 | SDP-001 | — |
| 5.2 | Requirements | SRS-001 | SRS-001 | SRS |
| 5.3 | Architecture | SAD-001 | SAD-001 | SAD |
| 5.4 | Detailed Design | SDD-001/002 | SDD-001 | SDD |
| 5.5 | Unit Implementation | (code) | (code) | (code) |
| 5.5 | Test Plan | STP-001 | — | STP/STC |
| 5.6 | Integration | VVP-001 | SVP-001 | — |
| 5.7 | Verification | VVP-001 | SVP-001 | — |
| 5.8 | Traceability | RTM-001 | RTM-001 | RTM |
| 6.1 | Configuration Mgmt | SCM-001 | — | — |
| 7 | Risk Management | SRM-001, SHA-001 | SHA-001 | — |
| 8 | SOUP | SOUP-001 | SOUP-001 | — |
| 9 | Problem Resolution | SPR-001 | — | — |
| 12 | Maintenance | SMP-001 | — | — |

### Coverage Summary

| Software Item | Documents | IEC 62304 Coverage |
|--------------|:---------:|:------------------:|
| XPE | 21 | Complete (Class B full package) |
| GSVG | 10 | Complete (Class B full package) |
| Ghost Correction | 6 | Complete (PRD-SRS-SAD-SDD-STP-RTM) |
| Calibration | 2 | Partial (PRD + Architecture) |
| Panel Defect | 1 | R&D Plan only |
| Quality Evaluation | 4 | Pending (methodology ready, SPEC needed) |

---

## 7. Document Statistics

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

## 8. For AI Agents

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

## 9. Document Versioning Convention

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
| 2026-04-14 | v1.0.0 | Initial documentation index |
