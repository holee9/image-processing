# SPEC-XPE-MASTER: X-ray Image Processing Engine Master Specification

**Document ID**: SPEC-XPE-MASTER  
**Version**: 2.5.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

**Source Set**: `product.md v1.4.0`, `structure.md v1.3.0`, `pipeline-spec.md v1.5.0`, `api-spec.md v1.3.0`, `xpe-algorithm-spec-deepsync.md v3.2.0-ds4`, `XPE-MRD-001 v1.2.0`, `XPE-PRD-SYSTEM-001 v1.3.0`, `XPE-SVVP-001 v1.3.0`, `XPE-AI-REG-001 v1.1.0`, `XPE-DOC-HELP-001 v2.1.0`, `XPE-GUI-MENU-001 v1.0.0`, `XPE-Module-Reinforcement-Plan.md`, `Algorithm-Benchmark-Pack-Spec.md v1.3.0`, `Algorithm-Evaluation-Protocol.md v1.3.0`, `Regulatory-Feature-Boundary-Matrix.md`, `XPE-ALG-001 v1.8`

---

## 1. Purpose

This document is the master tie-breaker for the XPE project document set.

If two project documents disagree, this document wins until the lower-level documents are synchronized.

It fixes the canonical answers for:

- document authority,
- product boundary,
- executable-unit totals,
- phase ownership,
- pipeline invariants,
- algorithm promotion rules,
- remaining synchronization debt.

---

## 2. Canonical Decisions

| Topic | Canonical decision |
|---|---|
| Canonical document root | `docs/project/` only |
| XPE software units | 38 SWU |
| GSVG software items | 4 SI |
| Executable-unit total | 42 |
| Native exported APIs | 82 |
| Phase 0 | common ABI, orchestration, QA scaffolding |
| Phase 1a | detector-domain deterministic correction |
| Phase 1b | deterministic enhancement, display, DICOM, whole-image EI |
| Phase 2 | deterministic premium processing and GSVG |
| Phase 3 | assistive AI with worker isolation |
| EI unit identifier | `SWU-2.10` only |
| EI baseline | Phase 1b, whole-image, detector-domain |
| EI refinement | Phase 2, same API re-invoked on ROI-cropped detector image |
| Body-part recognition | Phase 3 only |
| Image stitching | Phase 3 only |
| Flags | state-only bitfield |
| GSVG failure reason | alert or diagnostic JSON, never embedded in flags |
| AI dependency model | `xpe_ai.dll` proxy plus `xpe_ai_worker.exe` sandboxed worker |
| Documentation generation model | DocFX for conceptual plus managed API docs, Doxygen for native exported headers |
| In-app help model | version-matched offline Help entry point from the host application, with packaged local bundle |
| GUI menu model | top-level menu bar with `File`, `Backend`, `View`, `Pipeline`, `Tools`, and `Help`; toolbar remains shortcut surface |

---

## 3. Document Authority Stack

Use the following order when reconciling project documents:

1. `SPEC-XPE-MASTER.md`
2. `product.md`
3. `structure.md`
4. `pipeline-spec.md`
5. `api-spec.md`
6. `xpe-algorithm-spec-deepsync.md`
7. `XPE-MRD-001_Market_Requirements_Document.md`
8. `XPE-PRD-SYSTEM-001_System_Product_Requirements.md`
9. `XPE-SVVP-001_System_Verification_Validation_Plan.md`
10. `XPE-AI-REG-001_AI_Regulatory_Strategy.md`
11. `XPE-DOC-HELP-001_Documentation_and_Help_Strategy.md`
12. `XPE-GUI-MENU-001_Menu_and_Command_Strategy.md`
13. `docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md` as detailed design reference only
14. `sprint-plan.md`
15. analysis, reinforcement, benchmark, and review logs

Notes on XPE-ALG-001 authority:

- XPE-ALG-001 is a detailed design reference for implementation work.
- It is subordinate to the normative decisions in items 1 through 10 above.
- It may guide implementation detail only when it does not contradict higher-priority canonical documents.
- It does not override the master specification, product boundary, system PRD, or system V and V plan.

Raw JSON research transcripts and `.moai/` working copies are not normative.

---

## 4. Product Boundary

### 4.1 Mandatory release baseline

The minimum releasable product consists of:

- `xpe_common.dll`
- `xpe_preprocess.dll`
- `xpe_enhance_basic.dll`
- `xpe_display.dll`
- `xpe_dicom.dll`
- `ImageProcTest.exe`

This baseline must deliver:

- detector-domain correction,
- whole-image EI and DI,
- display-ready presentation LUT path,
- DICOM export,
- deterministic operation without AI or GSVG.

### 4.2 Optional deterministic premium layer

Phase 2 is optional at deployment time but deterministic in behavior:

- `xpe_enhance_advanced.dll`
- `gsvg.dll`

Phase 2 adds:

- baseline collimation detection,
- ROI-aware EI refinement,
- multiscale processing,
- fractional processing,
- grid suppression and virtual grid.

### 4.3 Optional assistive AI layer

Phase 3 is assistive only:

- `xpe_ai.dll`
- `xpe_ai_worker.exe`

Phase 3 may add:

- body-part recognition,
- AI collimation refinement,
- image stitching,
- bone suppression,
- DL denoising.

If Phase 3 fails, the deterministic baseline remains the delivery path.

---

## 5. Canonical Phase Semantics

### 5.1 Phase 1a

Detector-domain correction only:

- readout validation,
- temperature compensation,
- offset,
- nonlinearity,
- gain,
- binning correction,
- defect correction,
- lag and ghost correction.

### 5.2 Phase 1b

Deterministic enhancement and delivery:

- whole-image EI baseline,
- log,
- noise reduction,
- contrast enhancement,
- edge enhancement,
- display LUT path,
- DICOM export.

### 5.3 Phase 2

Deterministic premium functions:

- baseline collimation,
- ROI-aware EI refinement,
- multiscale processing,
- fractional processing,
- GSVG.

### 5.4 Phase 3

Assistive AI only:

- body-part recognition,
- AI collimation refinement,
- stitching,
- bone suppression,
- DL denoising.

---

## 6. Algorithm and Promotion Rules

1. Detector-domain correctness has priority over display-oriented visual appeal.
2. Premium features do not become release-safe merely because they look better on sample images.
3. Nonlinear or suppressive algorithms require task-based or observer-centered evidence when clinically relevant.
4. AI cannot be promoted without degraded-mode proof, labeling, and transparency.
5. Benchmark manifests and hashes must be frozen before release claims are accepted.
6. Reject-analysis telemetry export is release-safe; reject-workflow optimization claims are not.
7. EI and DI are operational detector-exposure signals, not patient-dose estimates.

---

## 7. Remaining Synchronization Debt

The following still require downstream synchronization outside this document:

- `docs/post-processing/xpe/` IEC package documents,
- detailed algorithm design references under `docs/post-processing/xpe/`,
- implementation status documents if code ownership or progress changes materially.

**v1.8 sync debt (added 2026-04-15)**:

- §21 PCD Spectral Binning (GAP-BW) and §22 CS-Tomo Sparse-View Reconstruction (GAP-CB) require new SWU entries in XPE-SDD-001 and SRS entries in XPE-SRS-001.
- BP-13 benchmark family (PCD spectral binning, ring correction, CS-Tomo, RDSR compliance) requires formal definition in Algorithm-Benchmark-Pack-Spec.md.
- xpe-algorithm-spec-deepsync.md requires sync for §21, §22, §9.14, §4.10, §3.17, §8.9, §12.12, §10.10, §17.4, §11.6 new entries.

Until those documents are synchronized, this master document defines the canonical answer.
