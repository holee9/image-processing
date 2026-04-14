# X-ray Image Processing - Documentation Index

## Overview

This directory contains all development documentation for the X-ray FPD Image Processing project.
Documents are organized by module and purpose for continuous review.

---

## Directory Structure

```
docs/
├── project/              # Project-wide specs, plans, architecture
├── calibration/          # Calibration pre-processing module (xpe_preprocess.dll)
├── ghost-correction/     # Lag/Ghost correction module (PRE-05)
├── panel-defect-algorithm/ # Panel defect detection & correction
├── post-processing/      # Post-processing modules
│   ├── gsvg/             #   Grid Suppression Virtual Grid (gsvg.dll)
│   └── xpe/              #   X-ray Processing Engine (xpe_*.dll)
├── quality-eval/         # Quality evaluation module (xpe_quality_eval.dll) - NEW
├── development/          # CI/CD, build, operational runbooks
└── references/           # External references, research surveys, proposals
```

---

## 1. Project Foundation (`project/`)

| Document | Type | Description |
|----------|------|-------------|
| [product.md](project/product.md) | Product Overview | Product identity, purpose, components, regulatory context |
| [structure.md](project/structure.md) | Architecture | Repository layout, module organization, SWU structure |
| [tech.md](project/tech.md) | Tech Stack | Technology choices, regulatory framework, deployment targets |
| [api-spec.md](project/api-spec.md) | API Spec | XPE API specification with SWU mappings (1,409 lines) |
| [pipeline-spec.md](project/pipeline-spec.md) | Pipeline Spec | Pre/post-processing pipeline specification |
| [SPEC-XPE-MASTER.md](project/SPEC-XPE-MASTER.md) | Master SPEC | Master implementation plan, Phase 0-3 deliverables |
| [SPEC-XPE-MASTER-verification.md](project/SPEC-XPE-MASTER-verification.md) | Verification | 3-round cross-verification, 12 issues found |
| [xpe-algorithm-spec-deepsync.md](project/xpe-algorithm-spec-deepsync.md) | Algorithm SPEC | Algorithm-level specification with deep sync verification |
| [cross-verification-report-2026-04-13.md](project/cross-verification-report-2026-04-13.md) | Report | XPE document cross-verification findings |

> Source: Copied from `.moai/project/` and `.moai/specs/` for review convenience.

---

## 2. Calibration Module (`calibration/`)

| Document | Type | Description |
|----------|------|-------------|
| [README.md](calibration/README.md) | Architecture | xpe_preprocess.dll - 9-stage pipeline, 18 C ABI functions |
| [xray-detector-calibration-prd.md](calibration/xray-detector-calibration-prd.md) | PRD | Calibration algorithm requirements (offset, gain, defect, lag, scatter) |

---

## 3. Ghost Correction Module (`ghost-correction/`)

IEC 62304 Class B complete lifecycle package for Lag/Ghost correction.

| Document | Type | Description |
|----------|------|-------------|
| [sw_lag_correction_prd_v2.md](ghost-correction/sw_lag_correction_prd_v2.md) | PRD v2 | Product requirements with NLCSC algorithm |
| [srs_ghost_correction.md](ghost-correction/srs_ghost_correction.md) | SRS | Software Requirements Specification |
| [sad_ghost_correction.md](ghost-correction/sad_ghost_correction.md) | SAD | Software Architecture Document |
| [sdd_ghost_correction.md](ghost-correction/sdd_ghost_correction.md) | SDD | Software Detailed Design |
| [stp_stc_ghost_correction.md](ghost-correction/stp_stc_ghost_correction.md) | STP/STC | Test Plan & Test Cases (50+ cases) |
| [rtm_ghost_correction.md](ghost-correction/rtm_ghost_correction.md) | RTM | Requirements Traceability Matrix (30+ requirements) |

---

## 4. Panel Defect Algorithm (`panel-defect-algorithm/`)

| Document | Type | Description |
|----------|------|-------------|
| [plan.md](panel-defect-algorithm/plan.md) | R&D Plan | Bad pixel/cluster detection, line defects, 3 correction profiles |

---

## 5. Post-Processing Modules (`post-processing/`)

### 5.1 GSVG - Grid Suppression Virtual Grid (`post-processing/gsvg/`)

IEC 62304 Class B documentation package (10 documents).

| Document | Type | Description |
|----------|------|-------------|
| [GSVG_IEC62304_ClassB_Document_Package.md](post-processing/gsvg/GSVG_IEC62304_ClassB_Document_Package.md) | Package | Master document with IEC 62304 clause mapping |
| [GSVG-PKG-001_Document_Index.md](post-processing/gsvg/GSVG-PKG-001_Document_Index.md) | Index | Document listing and version control |
| [GSVG-SDP-001_Development_Plan.md](post-processing/gsvg/GSVG-SDP-001_Development_Plan.md) | SDP | Software Development Plan |
| [GSVG-SRS-001_Requirements.md](post-processing/gsvg/GSVG-SRS-001_Requirements.md) | SRS | Software Requirements Specification |
| [GSVG-SAD-001_Architecture.md](post-processing/gsvg/GSVG-SAD-001_Architecture.md) | SAD | Software Architecture Document |
| [GSVG-SDD-001_Detailed_Design.md](post-processing/gsvg/GSVG-SDD-001_Detailed_Design.md) | SDD | Software Detailed Design |
| [GSVG-SVP-001_Verification_Plan.md](post-processing/gsvg/GSVG-SVP-001_Verification_Plan.md) | SVP | Software Verification Plan |
| [GSVG-SHA-001_Hazard_Analysis.md](post-processing/gsvg/GSVG-SHA-001_Hazard_Analysis.md) | SHA | Safety/Hazard Analysis |
| [GSVG-SOUP-001_SOUP_Analysis.md](post-processing/gsvg/GSVG-SOUP-001_SOUP_Analysis.md) | SOUP | Software of Unknown Provenance |
| [GSVG-RTM-001_Traceability.md](post-processing/gsvg/GSVG-RTM-001_Traceability.md) | RTM | Requirements Traceability Matrix |

### 5.2 XPE - X-ray Processing Engine (`post-processing/xpe/`)

IEC 62304 Class B documentation package (18 documents).

| Document | Type | Description |
|----------|------|-------------|
| [xpe-iec62304-class-b-package.md](post-processing/xpe/xpe-iec62304-class-b-package.md) | Package | Applicability matrix for Class A/B/C |
| [xray-postprocessing-prd.md](post-processing/xpe/xray-postprocessing-prd.md) | PRD | X-ray post-processing product requirements |
| [XPE-PRD-002_Detailed_Project_Execution_PRD.md](post-processing/xpe/XPE-PRD-002_Detailed_Project_Execution_PRD.md) | PRD | Detailed execution PRD with scope/timeline |
| [XPE-PRD-003_PRD_Decomposition_and_Backlog.md](post-processing/xpe/XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | PRD | 8 epics, 109 backlog items |
| [XPE-PLAN-001_Consolidated_Execution_Plan.md](post-processing/xpe/XPE-PLAN-001_Consolidated_Execution_Plan.md) | Plan | 6-phase execution roadmap |
| [XPE-SDP-001_Software_Development_Plan.md](post-processing/xpe/XPE-SDP-001_Software_Development_Plan.md) | SDP | Software Development Plan |
| [XPE-SRS-001_Software_Requirements_Specification.md](post-processing/xpe/XPE-SRS-001_Software_Requirements_Specification.md) | SRS | Software Requirements Specification |
| [XPE-SAD-001_Software_Architecture_Document.md](post-processing/xpe/XPE-SAD-001_Software_Architecture_Document.md) | SAD | Software Architecture Document |
| [XPE-SDD-001_Software_Unit_Identification.md](post-processing/xpe/XPE-SDD-001_Software_Unit_Identification.md) | SDD | Software Unit Identification |
| [XPE-RTM-001_Requirements_Traceability_Matrix.md](post-processing/xpe/XPE-RTM-001_Requirements_Traceability_Matrix.md) | RTM | Requirements Traceability Matrix |
| [XPE-VVP-001_Verification_Validation_Plan.md](post-processing/xpe/XPE-VVP-001_Verification_Validation_Plan.md) | V&V | Verification and Validation Plan |
| [XPE-62304-MAP-001_Compliance_Matrix.md](post-processing/xpe/XPE-62304-MAP-001_Compliance_Matrix.md) | Compliance | IEC 62304 compliance mapping |
| [XPE-SCM-001_Configuration_Management_Plan.md](post-processing/xpe/XPE-SCM-001_Configuration_Management_Plan.md) | SCM | Configuration Management Plan |
| [XPE-SRM-001_Software_Risk_Management_File.md](post-processing/xpe/XPE-SRM-001_Software_Risk_Management_File.md) | Risk | Software Risk Management File |
| [XPE-SOUP-001_SOUP_Analysis.md](post-processing/xpe/XPE-SOUP-001_SOUP_Analysis.md) | SOUP | Software of Unknown Provenance |
| [XPE-SMP-001_Software_Maintenance_Plan.md](post-processing/xpe/XPE-SMP-001_Software_Maintenance_Plan.md) | SMP | Software Maintenance Plan |
| [XPE-SPR-001_Problem_Resolution_Process.md](post-processing/xpe/XPE-SPR-001_Problem_Resolution_Process.md) | SPR | Problem Resolution Process |
| [XPE-SRP-001_Software_Release_Procedure.md](post-processing/xpe/XPE-SRP-001_Software_Release_Procedure.md) | SRP | Software Release Procedure |

---

## 6. Quality Evaluation Module (`quality-eval/`) - NEW

Development documentation for `xpe_quality_eval.dll` (proposed).
Three-layer measurement-analysis-management architecture.

| Document | Type | Description |
|----------|------|-------------|
| [01_Noise_...pplx.md](quality-eval/01_Noise_평가_방법론_종합보고서.pplx.md) | Methodology (WHY) | IEC 62220-1 based noise evaluation, 12 metrics (DQE/MTF/NPS/NEQ/SNR) |
| [02_양산라인_...pplx.md](quality-eval/02_양산라인_계측방법론_가이드.pplx.md) | Measurement (HOW) | Production line testing, 12 items, 5-level hierarchy |
| [03_측정_...pplx.md](quality-eval/03_측정_알고리즘_명세서.pplx.md) | Algorithm (WHAT) | NPS/MTF/DQE/Defect Python implementation specs |
| [05_체계적_...pplx.md](quality-eval/05_체계적_관리방안_문서.pplx.md) | Management (WHERE/WHEN) | SPC, KPI (Cpk>=1.33), CAPA framework |

> Status: Awaiting SPEC creation (`/moai plan`) for formal development kickoff.

---

## 7. Development Operations (`development/`)

| Document | Type | Description |
|----------|------|-------------|
| [XPE-CI-CD_LocalBuild_Runbook.md](development/XPE-CI-CD_LocalBuild_Runbook.md) | Runbook | CI/CD and local build execution guide |

---

## 8. References (`references/`)

External research, technology surveys, and proposals. Not direct development input.

| Document | Type | Description |
|----------|------|-------------|
| [xray_fpd_tech_classification_final.md](references/xray_fpd_tech_classification_final.md) | Tech Survey | 50+ papers, 30+ patents, 12 vendors classification |
| [04_...pplx.md](references/04_분석SW_Tool_비교_추천보고서.pplx.md) | Tool Survey | Open-source/commercial analysis tool comparison |
| [xray_grid_suppression_virtual_grid_research.md](references/xray_grid_suppression_virtual_grid_research.md) | Research | Grid artifact removal algorithm background |
| [panel-defect-proposals/](references/panel-defect-proposals/) | Proposals | TFT Panel defect correction proposals (2x .docx) |

---

## Document Statistics

| Category | Count | IEC 62304 Coverage |
|----------|-------|--------------------|
| Project Foundation | 9 | - |
| Calibration | 2 | Partial (PRD + Architecture) |
| Ghost Correction | 6 | Complete (PRD-SRS-SAD-SDD-STP-RTM) |
| GSVG | 10 | Complete (SDP-SRS-SAD-SDD-SVP-SHA-SOUP-RTM) |
| XPE | 18 | Complete (Full IEC 62304 Class B package) |
| Quality Evaluation | 4 | Pending (methodology ready, SPEC needed) |
| Panel Defect | 1 | R&D Plan only |
| Development Ops | 1 | - |
| References | 4 | - |
| **Total** | **55** | |

---

Last Updated: 2026-04-14
