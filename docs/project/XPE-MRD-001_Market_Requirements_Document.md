# Market Requirements Document

**Document ID**: XPE-MRD-001  
**Version**: 1.2.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Classification**: Internal / Confidential  
**Author**: XPE Program Management  
**Regulatory Scope**: IEC 62304, ISO 14971, FDA 21 CFR 820.30, EU MDR 2017/745  
**Canonical Scope**: `docs/project/`  
**Derived from**: `docs/project/product.md`, `docs/project/SPEC-XPE-MASTER.md`, `docs/xray_fpd_tech_classification_final.md`

---

## 1. Purpose

This document defines the market-level needs for XPE, a modular X-ray flat-panel detector image-processing engine.

It is the top business and product-need baseline for:

- why the product exists,
- which customers it is built for,
- which market outcomes matter,
- which constraints are non-negotiable before system requirements are written.

This document does not define detailed algorithms or APIs. Those belong to the system PRD and lower-level design documents.

---

## 2. Market Problem Statement

FPD manufacturers and imaging-system integrators face the same structural problem:

1. they need detector correction, enhancement, DICOM export, and optional premium processing to ship complete X-ray systems,
2. they need this with regulated traceability and predictable integration cost,
3. they cannot afford long custom-development cycles for every detector family or every OEM program.

The market gap is not only image quality. It is the lack of a reusable, regulated, modular processing engine that can:

- preserve deterministic baseline image delivery,
- absorb detector-specific calibration complexity,
- add premium processing without destabilizing the baseline,
- keep optional AI transparent and degradable,
- support field quality-control operations after release.

---

## 3. Target Customer Segments

### 3.1 FPD manufacturers

These customers need:

- fast time to market,
- stable C ABI integration into existing stacks,
- detector-specific calibration and correction logic,
- a path to IEC 62304 Class B documentation without writing everything from scratch.

### 3.2 Imaging-system OEMs and integrators

These customers need:

- stable DICOM behavior,
- configurable deployment by module,
- predictable degraded behavior when premium modules are absent,
- strong technical documentation and field diagnostics,
- version-matched offline help that can be opened directly from the host QA or integration tool.

### 3.3 QA, service, and regulatory teams

These customers need:

- auditability,
- benchmarked release evidence,
- exposure-management support,
- post-market quality signals such as reject analysis and drift monitoring,
- versioned user and developer manuals that match the shipped build.

---

## 4. Market Requirements

### 4.1 Integration and time-to-market requirements

| ID | Market requirement | Rationale |
|---|---|---|
| `MR-MKT-001` | XPE shall provide a deterministic release baseline that works without optional Phase 2 or Phase 3 binaries. | OEM integration must not depend on premium features. |
| `MR-MKT-002` | XPE shall expose a stable C ABI suitable for C# P/Invoke and native host integration. | Integrators need low-friction adoption into existing consoles. |
| `MR-MKT-003` | XPE shall support modular deployment by binary so that OEMs can ship baseline, premium, and assistive variants without forking the architecture. | Product-line flexibility is a market requirement. |
| `MR-MKT-004` | XPE documentation shall define a single canonical document root and tie-breaker document for conflicts. | Integration programs fail when specs drift. |
| `MR-MKT-005` | XPE shall provide version-matched offline help and integration guidance that can be opened from the host application or QA tool. | Integrators and field users need low-friction onboarding and troubleshooting without relying on external portals. |

### 4.2 Image-quality and detector-correction requirements

| ID | Market requirement | Rationale |
|---|---|---|
| `MR-IMG-001` | XPE shall prioritize detector-domain correctness before display-oriented enhancement. | Market differentiation depends on robust raw-to-corrected conversion. |
| `MR-IMG-002` | XPE shall remain stable across temperature, gain mode, lag history, and detector variation. | Field performance matters more than one-time demo quality. |
| `MR-IMG-003` | XPE shall support Exposure Index / Deviation Index for detector-exposure management, while explicitly avoiding patient-dose claims. | EI/DI are operationally valuable but are not dose surrogates. |
| `MR-IMG-004` | Nonlinear or suppressive algorithms shall not be promoted using only scalar fidelity metrics; task-based or observer-centered evidence shall be required where clinically relevant. | Published imaging literature shows scalar metrics alone are not enough for nonlinear processing decisions. |
| `MR-IMG-005` | Premium deterministic features such as virtual grid, ROI-aware EI refinement, and advanced enhancement shall degrade safely when absent or bypassed. | Optional modules must not create unusable systems. |

### 4.3 Operational quality-control requirements

| ID | Market requirement | Rationale |
|---|---|---|
| `MR-OPS-001` | XPE shall emit enough standardized event and metadata information to support vendor-neutral reject analysis and repeat-rate review. | Reject analysis is part of ongoing DR quality control. |
| `MR-OPS-002` | XPE shall support DI trend monitoring and site-configurable operational review bands. | Exposure management is a field-operations requirement, not just a lab metric. |
| `MR-OPS-003` | XPE shall preserve benchmark-pack manifests and content hashes for repeatable release comparison. | Customers need reproducible evidence, not one-off tuning. |
| `MR-OPS-004` | XPE developer and user documentation shall be generated from version-controlled Markdown and structured code comments so that help stays synchronized with the shipped build. | Manually maintained side documents drift too easily for regulated and integration-heavy programs. |

### 4.4 AI transparency and human-factors requirements

| ID | Market requirement | Rationale |
|---|---|---|
| `MR-AI-001` | Optional AI features shall be assistive only and shall never be the only path to a delivered image. | Current product boundary must remain inside a controlled assistive scope. |
| `MR-AI-002` | AI outputs shall expose confidence, failure mode, and fallback behavior to the operator or reviewer. | Transparency is required for trust and regulatory defensibility. |
| `MR-AI-003` | AI scope, training-data boundaries, and out-of-distribution handling shall be documented and testable. | Hidden model assumptions create field risk. |

### 4.5 Cybersecurity and software-supply-chain requirements

| ID | Market requirement | Rationale |
|---|---|---|
| `MR-SEC-001` | XPE shall maintain a software bill of materials for shipped binaries and third-party dependencies. | FDA cyber-device expectations increasingly require supply-chain visibility. |
| `MR-SEC-002` | XPE shall support signed or integrity-checked configuration, calibration, and model artifacts. | Field tampering and accidental corruption must be detectable. |
| `MR-SEC-003` | XPE shall define a vulnerability-response and update path for deployed software components. | Cyber maintenance is part of product quality, not an afterthought. |

### 4.6 Interoperability and regulatory requirements

| ID | Market requirement | Rationale |
|---|---|---|
| `MR-REG-001` | XPE shall stay inside the current Class B deterministic-plus-assistive boundary unless an explicit regulatory reclassification decision is made. | Scope creep would change the evidence burden materially. |
| `MR-REG-002` | XPE shall support GSDF-aligned presentation behavior and diagnostic DICOM export suitable for system integration and conformance testing. | Interoperability is mandatory for OEM adoption. |
| `MR-REG-003` | XPE shall separate release-safe, research-gated, and regulatory-hold features in product documentation and release governance. | Market claims must stay aligned with evidence. |

---

## 5. Non-Goals for the Current Program

The current program does not target:

- pathology-detection or triage claims,
- patient-dose recommendation or ALARA advisory logic,
- one-click fully automatic clinical optimization,
- workflow-optimization claims based on reject analytics,
- AI as the sole image-delivery path.

These may be researched, but they are outside the current market claim boundary.

---

## 6. Market Success Criteria

| Area | Success criterion |
|---|---|
| OEM adoption | baseline integration into a host system without requiring Phase 2 or Phase 3 binaries |
| delivery speed | meaningful reduction in time-to-market versus custom, from-scratch image-processing development |
| image quality | stable detector correction across temperature, gain, and lag-history benchmark families |
| field quality | DI trend review and reject-analysis data available for site QA workflows |
| trust | explicit AI labeling, fallback, and transparency in operator-facing workflows |
| usability | offline help and integration guidance accessible from the host UI and matched to the shipped version |
| compliance readiness | SBOM, benchmark freeze, and cross-document traceability available for review |

---

## 7. Traceability into System PRD

This MRD is translated into system-level requirements primarily in:

- `XPE-PRD-SYSTEM-001_System_Product_Requirements.md`
- `XPE-SVVP-001_System_Verification_Validation_Plan.md`
- `Regulatory-Feature-Boundary-Matrix.md`

All detailed product and verification requirements shall trace back to one or more `MR-*` identifiers from this document.
