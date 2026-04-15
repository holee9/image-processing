# System Verification and Validation Plan

**Document ID**: XPE-SVVP-001  
**Version**: 1.1.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Classification**: Internal / IEC 62304 Compliance  
**Author**: XPE QA Team  
**Safety Classification**: IEC 62304 Class B  
**Canonical Scope**: `docs/project/`  
**Parent**: `XPE-PRD-SYSTEM-001_System_Product_Requirements.md` v1.1.0  
**Cross-reference**: `Algorithm-Benchmark-Pack-Spec.md`, `Algorithm-Evaluation-Protocol.md`, `Regulatory-Feature-Boundary-Matrix.md`

---

## 1. Purpose

This document defines the verification and validation strategy for the full XPE system across:

- deterministic baseline processing,
- deterministic premium processing,
- assistive AI,
- field-quality operations,
- cybersecurity and software-supply-chain controls.

The goal is to prove not only that the system was built correctly, but that it remains promotable, auditable, and degradable within the current claim boundary.

---

## 2. V and V Structure

| Level | Scope | Primary evidence |
|---|---|---|
| L1 | unit verification | unit tests, static analysis, scalar-vs-SIMD parity, memory discipline |
| L2 | integration verification | API contract tests, binary loading, sidecar and alert flows |
| L3 | system verification | end-to-end deterministic pipeline, DICOM, performance, degraded mode |
| L4 | premium and assistive feature verification | Phase 2 and Phase 3 option-gating, benchmark evidence, observer or task-based evidence where required |
| L5 | validation and usability | operator-visible behavior, labeling, fallback comprehension, intended-use validation |
| L6 | field performance and maintenance | DI drift review, reject-analysis telemetry, cyber maintenance, regression reproducibility |

---

## 3. Verification Principles

1. Detector-domain metrics shall be measured before presentation LUT application.
2. Suppressive or nonlinear algorithms shall not be approved with scalar fidelity metrics alone where task-based evidence is relevant.
3. Optional features shall be validated both in enabled mode and in absent or failed mode.
4. Frozen benchmark manifests and hashes are required for release claims.
5. AI evidence is incomplete unless degraded-mode and transparency checks also pass.

---

## 4. Requirement Coverage Matrix

| PR ID | Verification / validation method | Main evidence |
|---|---|---|
| `PR-FUNC-001` | L3 end-to-end deterministic pipeline test | baseline output bundle and DICOM export |
| `PR-FUNC-002` | L1 plus L3 detector-chain tests | detector benchmark packs `BP-01` to `BP-05` |
| `PR-FUNC-003` | L3 EI baseline tests | `BP-08`, detector-domain EI error results |
| `PR-FUNC-004` | L3 Phase 2 refinement tests | `BP-07`, ROI reinvocation evidence |
| `PR-FUNC-005` | L3 presentation and DICOM tests | GSDF checks, DICOM conformance logs |
| `PR-FUNC-006` | L4 premium-option tests | `BP-06`, premium feature regression bundle |
| `PR-FUNC-007` | L3 and L5 QA workflow tests | orchestration and constancy workflow evidence |
| `PR-FUNC-008` | L3 plus L6 operational telemetry tests | reject-event schema validation and sample exports |
| `PR-SAFE-001` | L2 raw-preservation test | byte-identical raw reference evidence |
| `PR-SAFE-002` | L3 degraded-mode tests | missing-binary and worker-failure scenarios |
| `PR-SAFE-003` | L2 / L3 alert and diagnostic tests | flags plus external reason capture |
| `PR-SAFE-004` | L3 EI rejection tests | `BP-09`, flagged non-normative EI cases |
| `PR-SAFE-005` | L4 assistive-output handling tests | baseline and assistive output separation |
| `PR-SAFE-006` | L2 integrity and artifact-check tests | signature or hash verification logs |
| `PR-PERF-001` | L3 performance test | detector-chain latency measurement |
| `PR-PERF-002` | L3 performance test | full deterministic latency measurement |
| `PR-PERF-003` | L3 memory test | peak-memory and steady-state profiling |
| `PR-PERF-004` | L4 option-cost profiling | incremental Phase 2 and Phase 3 timing |
| `PR-AI-001` | L2 / L4 worker-isolation tests | launch, crash, restart, timeout evidence |
| `PR-AI-002` | L4 and L5 reporting tests | model version, confidence, fallback visibility |
| `PR-AI-003` | L5 usability and labeling tests | assistive labeling and operator override evidence |
| `PR-AI-004` | L4 evidence review | degraded-mode proof plus task-based or observer evidence |
| `PR-SEC-001` | L2 documentation audit | SBOM presence and packaging checks |
| `PR-SEC-002` | L2 integrity tests | configuration, model, and manifest tamper detection |
| `PR-SEC-003` | L6 maintenance review | update path and vulnerability handling evidence |
| `PR-OPS-001` | L6 reproducibility audit | frozen benchmark manifest and result replay |
| `PR-OPS-002` | L6 operational review | DI distribution and drift dashboards |
| `PR-OPS-003` | L6 telemetry validation | standardized reject-analysis schema checks |
| `PR-OPS-004` | document review | release-safe / research-gated / hold separation confirmed |

---

## 5. Benchmark and Evaluation Binding

### 5.1 Mandatory benchmark packs

The following benchmark families are mandatory for release-relevant claims:

- `BP-01` temperature sweep,
- `BP-02` multi-gain linearity,
- `BP-03` heel-effect SID variation,
- `BP-04` sparse and cluster defect,
- `BP-05` lag history sequence,
- `BP-06` grid and no-grid,
- `BP-07` collimation ROI,
- `BP-08` single-irradiation EI reference,
- `BP-09` stitched and multi-irradiation exclusion,
- `BP-10` degraded-mode stress.

### 5.2 Additional evidence for advanced or nonlinear features

The following evidence is additionally required when relevant:

- `BP-11` task-based or observer-centered feature pack for nonlinear or suppressive processing,
- `BP-12` operational QC and reject-analysis telemetry pack for field-quality claims.

### 5.3 Promotion rule

No feature is promotable to release-safe unless:

1. its benchmark-pack version and hashes are frozen,
2. its evaluation metrics satisfy the applicable protocol,
3. its degraded-mode behavior is verified,
4. its documentation is synchronized across product, pipeline, API, and regulatory boundary documents.

---

## 6. Task-Based and Observer-Centered Validation

Task-based or observer-centered validation is mandatory for:

- virtual-grid default promotion,
- bone suppression,
- DL denoising when marketed as more than a research artifact,
- any nonlinear enhancement that may hide, suppress, or reshape clinically meaningful structure.

Minimum evidence bundle:

- objective image-quality metric,
- artifact review,
- expert or observer review,
- parameter-sensitivity sweep,
- degraded-mode comparison to the deterministic baseline.

---

## 7. AI Validation Requirements

### 7.1 Required AI tests

| Test class | Minimum expectation |
|---|---|
| model load and startup | worker launches with validated artifact set |
| confidence and transparency | confidence and version surfaced to host |
| fallback | deterministic output still delivered when AI is unavailable |
| worker resilience | timeout, crash, restart, and corrupt-model cases handled |
| out-of-distribution behavior | unknown or low-confidence cases handled explicitly |
| operator labeling | assistive labeling and original-view access always available |

### 7.2 AI validation rule

AI outputs shall not be treated as release-complete unless both of the following are true:

- model-performance evidence is acceptable for the intended assistive use,
- degraded-mode and operator-transparency evidence is also acceptable.

---

## 8. Cybersecurity and Supply-Chain Verification

The following checks are part of system verification:

- SBOM generation and packaging review,
- artifact-integrity verification for configuration, calibration, model, and benchmark-manifest inputs,
- corrupted-artifact negative tests,
- documented vulnerability-response workflow review,
- release bundle inspection for shipped dependencies and versions.

These checks do not replace a dedicated security program, but they are required system evidence for this product line.

---

## 9. Field Performance Validation

Post-release validation shall monitor:

- DI distribution and drift by site or detector family,
- reject-rate and repeat-rate telemetry where enabled,
- override rates for assistive AI features,
- GSVG skip frequency and reason distribution,
- vulnerability and update events affecting released software artifacts.

Field monitoring may trigger:

- threshold review,
- benchmark refresh planning,
- documentation updates,
- promotion hold or rollback decisions.

---

## 10. Entry and Exit Criteria

### 10.1 Entry criteria

- parent PRD approved or accepted as current execution baseline,
- benchmark schema version defined,
- test environment and artifacts versioned,
- traceability IDs assigned.

### 10.2 Exit criteria

- all release-safe requirements have passing evidence,
- all open deviations are dispositioned,
- research-gated and regulatory-hold items are not misrepresented as release claims,
- benchmark manifests and result bundles are archived and reproducible.
