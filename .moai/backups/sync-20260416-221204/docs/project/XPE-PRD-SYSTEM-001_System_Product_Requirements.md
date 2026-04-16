# System Product Requirements Document

**Document ID**: XPE-PRD-SYSTEM-001  
**Version**: 1.1.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Classification**: Internal / Execution Baseline  
**Author**: XPE Program Management  
**Safety Classification**: IEC 62304 Class B  
**Canonical Scope**: `docs/project/`  
**Parent**: `XPE-MRD-001_Market_Requirements_Document.md` v1.1.0  
**System Tie-Breaker**: `SPEC-XPE-MASTER.md`  
**Cross-checked with**: `product.md`, `pipeline-spec.md`, `api-spec.md`, `xpe-algorithm-spec-deepsync.md`

---

## 1. Purpose

This document converts market requirements into system-level product requirements for XPE.

It defines:

- the system boundary,
- the required binaries and phases,
- the product requirements that must be implemented and verified,
- the release gates that separate deterministic baseline, deterministic premium, and assistive AI.

---

## 2. Product Definition

XPE is a modular X-ray image-processing engine for flat-panel detector systems. It transforms detector-domain raw frames into diagnostic-ready DICOM images while preserving a controlled boundary between:

- detector correction,
- deterministic enhancement,
- presentation and export,
- optional deterministic premium processing,
- optional assistive AI.

### 2.1 Canonical executable-unit count

| Category | Count |
|---|---:|
| XPE software units | 38 SWU |
| GSVG software items | 4 SI |
| **Total** | **42 executable units** |

### 2.2 Canonical binary map

| Binary | Phase | Role |
|---|:---:|---|
| `xpe_common.dll` | 0 | ABI types, lifecycle, alerts, logging, Auto Exposure Detection event interface |
| `xpe_preprocess.dll` | 1a | detector correction and calibration application |
| `xpe_enhance_basic.dll` | 1b | deterministic enhancement and whole-image EI |
| `xpe_display.dll` | 1b | LUT and GSDF-aligned presentation |
| `xpe_dicom.dll` | 1b | DICOM IO and network export |
| `xpe_enhance_advanced.dll` | 2 | deterministic premium enhancement |
| `gsvg.dll` | 2 | independent virtual-grid / grid-suppression package |
| `xpe_ai.dll` | 3 | in-process assistive proxy |
| `xpe_ai_worker.exe` | 3 | sandboxed assistive worker |
| `ImageProcTest.exe` | 0+ | orchestration, QA, integration harness |

### 2.3 Product rules that are fixed by this PRD

1. `SWU-2.10` is the only canonical EI unit identifier.
2. EI baseline occurs in Phase 1b on detector-domain, single-irradiation images.
3. ROI-aware EI refinement is a Phase 2 reinvocation of the same API on detector-domain ROI input.
4. Body-part recognition and stitching are Phase 3 only.
5. `XPE_FLAG_*` values are state bits only. Error details go to alerts or diagnostic JSON.
6. GSVG failure may degrade open, but the skip state and reason must be recorded explicitly.
7. AI features are assistive and shall never block deterministic image delivery.

---

## 3. Phase Definition

| Phase | Name | Required outcome |
|---|---|---|
| Phase 0 | foundation | common ABI, orchestration, QA scaffolding |
| Phase 1a | detector correction | deterministic detector-domain correction complete |
| Phase 1b | deterministic baseline | enhancement, display, DICOM, whole-image EI complete |
| Phase 2 | deterministic premium | advanced deterministic features optional but benchmarked |
| Phase 3 | assistive AI | worker-isolated assistive features optional and degradable |

---

## 4. System Requirements

### 4.1 Functional requirements

| PR ID | Requirement | Source MR |
|---|---|---|
| `PR-FUNC-001` | The system shall convert detector-domain raw input into a diagnostic-ready deterministic baseline image using Phases 1a and 1b only. | `MR-MKT-001`, `MR-IMG-001` |
| `PR-FUNC-002` | The system shall perform detector-domain correction in canonical order: readout validation, temperature compensation when applicable, offset, nonlinearity when applicable, gain, binning when applicable, defect correction, and lag/ghost correction. | `MR-IMG-001`, `MR-IMG-002` |
| `PR-FUNC-003` | The system shall compute whole-image EI/DI using `SWU-2.10` after detector correction and before presentation processing. | `MR-IMG-003` |
| `PR-FUNC-004` | The system shall allow ROI-aware EI refinement only by re-invoking the canonical EI API on detector-domain ROI input. | `MR-IMG-003`, `MR-IMG-005` |
| `PR-FUNC-005` | The system shall provide GSDF-aligned presentation processing and diagnostic DICOM export in the deterministic baseline. | `MR-REG-002` |
| `PR-FUNC-006` | The system shall support optional deterministic premium processing, including baseline collimation, multiscale, fractional processing, and GSVG, without changing the deterministic baseline contract. | `MR-IMG-005`, `MR-MKT-003` |
| `PR-FUNC-007` | The system shall support QA and constancy workflows through a host-facing orchestration and test harness. | `MR-OPS-002` |
| `PR-FUNC-008` | The system shall emit standardized event and metadata records sufficient for site-level reject-analysis and repeat-rate review. | `MR-OPS-001` |

### 4.2 Safety and degraded-mode requirements

| PR ID | Requirement | Source MR |
|---|---|---|
| `PR-SAFE-001` | The system shall preserve the original raw frame or a byte-identical reference path for forensic and validation use. | `MR-REG-001` |
| `PR-SAFE-002` | Missing Phase 2 or Phase 3 binaries shall not prevent deterministic Phase 1 delivery. | `MR-MKT-001`, `MR-IMG-005`, `MR-AI-001` |
| `PR-SAFE-003` | The system shall record optional-stage skip states and reasons explicitly through flags plus alerts or diagnostic JSON. | `MR-MKT-004`, `MR-IMG-005` |
| `PR-SAFE-004` | EI/DI shall reject or explicitly flag stitched and other multi-irradiation inputs as non-normative. | `MR-IMG-003` |
| `PR-SAFE-005` | Premium suppressive or nonlinear outputs shall never silently overwrite the deterministic baseline image. | `MR-IMG-004`, `MR-AI-001` |
| `PR-SAFE-006` | Benchmark manifests, calibration manifests, and model artifacts shall be integrity-checked before use. | `MR-OPS-003`, `MR-SEC-002` |

### 4.3 Performance and resource requirements

| PR ID | Requirement | Source MR |
|---|---|---|
| `PR-PERF-001` | Phase 1a detector correction shall complete within 500 ms for the canonical 3072x3072 target input. | `MR-IMG-002` |
| `PR-PERF-002` | Full deterministic Phase 1 output shall complete within 3000 ms. | `MR-MKT-001`, `MR-IMG-002` |
| `PR-PERF-003` | Peak memory shall remain inside the phase budgets defined by the pipeline specification. | `MR-MKT-003` |
| `PR-PERF-004` | Optional premium and AI stages shall expose bounded incremental latency and must not create unbounded steady-state growth. | `MR-IMG-005`, `MR-AI-001` |

### 4.4 AI and transparency requirements

| PR ID | Requirement | Source MR |
|---|---|---|
| `PR-AI-001` | AI functions shall run through a worker-isolated architecture using `xpe_ai.dll` plus `xpe_ai_worker.exe`. | `MR-AI-001`, `MR-SEC-002` |
| `PR-AI-002` | AI outputs shall expose model version, confidence, and degraded-mode behavior to the host or reviewer. | `MR-AI-002`, `MR-AI-003` |
| `PR-AI-003` | AI outputs shall be labelled assistive and operator-overridable. | `MR-AI-001`, `MR-AI-002` |
| `PR-AI-004` | No assistive AI feature shall be promoted to release claim status without degraded-mode proof and task-based or observer-centered evaluation where clinically relevant. | `MR-IMG-004`, `MR-AI-003` |

### 4.5 Cybersecurity and supply-chain requirements

| PR ID | Requirement | Source MR |
|---|---|---|
| `PR-SEC-001` | The shipped product shall maintain an SBOM covering native binaries, managed host components, models, and third-party libraries. | `MR-SEC-001` |
| `PR-SEC-002` | The product shall define artifact-integrity checks for configuration, calibration, benchmark-manifest, and AI-model files. | `MR-SEC-002` |
| `PR-SEC-003` | The program shall maintain a documented vulnerability-response and field-update path for software components and models. | `MR-SEC-003` |

### 4.6 Verification and operations requirements

| PR ID | Requirement | Source MR |
|---|---|---|
| `PR-OPS-001` | Release claims shall be backed by frozen benchmark manifests and reproducible result bundles. | `MR-OPS-003` |
| `PR-OPS-002` | The product shall support site-configurable DI review bands and DI drift monitoring. | `MR-OPS-002` |
| `PR-OPS-003` | Reject-analysis records shall use a stable schema suitable for vendor-neutral aggregation. | `MR-OPS-001` |
| `PR-OPS-004` | The product shall separate release-safe, research-gated, and regulatory-hold features in documentation, deployment, and verification artifacts. | `MR-REG-003` |

---

## 5. Phase Exit Gates

### 5.1 Phase 1a exit

- detector-correction pipeline implemented in canonical order,
- detector-domain benchmarks for temperature, gain, defect, and lag completed,
- `PR-FUNC-002`, `PR-SAFE-001`, `PR-PERF-001` satisfied.

### 5.2 Phase 1b exit

- deterministic baseline image delivery complete,
- EI baseline complete,
- GSDF-aligned presentation and DICOM export complete,
- `PR-FUNC-001`, `PR-FUNC-003`, `PR-FUNC-005`, `PR-PERF-002` satisfied.

### 5.3 Phase 2 exit

- premium deterministic stages are optional and benchmarked,
- ROI-aware EI refinement uses the canonical EI API,
- GSVG degrade-open behavior and diagnostics verified,
- `PR-FUNC-004`, `PR-FUNC-006`, `PR-SAFE-003`, `PR-SAFE-005` satisfied.

### 5.4 Phase 3 exit

- assistive AI is worker-isolated,
- confidence, version, and fallback visible,
- task-based or observer-centered evidence present for suppressive features,
- `PR-AI-001` through `PR-AI-004` satisfied.

---

## 6. Out of Scope for This PRD

The following remain outside the current release claim boundary:

- pathology-aware enhancement claims,
- patient-dose recommendation or ALARA advisory logic,
- repeat or reject workflow optimization claims,
- diagnosis or triage claims,
- fully automatic clinical optimization.

These remain governed as research-gated or regulatory-hold topics in `Regulatory-Feature-Boundary-Matrix.md`.
