# XPE Milestone UAT Plan

**Document ID**: XPE-MILESTONE-UAT-001  
**Version**: 1.1.0  
**Date**: 2026-04-14  
**Status**: Planned  
**Canonical Scope**: `docs/project/`

---

## 1. Policy

This document is a milestone evidence plan, not a completion report.

No milestone is marked `PASS` in advance. Each gate moves through:

- `Planned`
- `Evidence Collected`
- `Reviewed`
- `Approved`

Every approval requires both automated evidence and human sign-off.

---

## 2. Required Evidence Types

| Evidence type | Examples |
|---|---|
| Build evidence | CI run URL, local build log, export list |
| Functional evidence | unit tests, integration tests, golden dataset outputs |
| Performance evidence | timing report, memory report, degraded-mode latency |
| Safety evidence | alert log, failure-mode report, fallback behavior |
| Human review evidence | operator checklist, reviewer signature, defect log |

---

## 3. Milestone Overview

| Milestone | Scope | Status |
|---|---|---|
| `M1` Foundation | `xpe_common`, build/test scaffolding, ABI smoke | Planned |
| `M2` Detector correction | Phase 1a detector pipeline | Planned |
| `M3` Deterministic release baseline | Phase 1b enhancement, display, DICOM | Planned |
| `M4` Deterministic premium layer | Phase 2 advanced enhancement and GSVG | Planned |
| `M5` Assistive AI layer | Phase 3 worker-isolated AI | Planned |

---

## 4. Gate Requirements

### 4.1 M1 Foundation

Required evidence:

- successful native build and smoke test
- verified common ABI export list
- alert subsystem exercised
- AED subsystem exercised
- memory ownership and struct packing checks

### 4.2 M2 Detector correction

Required evidence:

- detector-domain benchmark pack subset executed
- pre-processing latency <= 500 ms
- offset/gain/nonlinearity/defect/lag outputs stored and comparable
- first-frame and empty-history degraded modes validated

### 4.3 M3 Deterministic release baseline

Required evidence:

- end-to-end raw-to-DICOM path
- whole-image EI baseline values validated against reference calculations
- GSDF-aligned presentation checks
- full deterministic path <= 3000 ms
- DICOM export and round-trip checks

### 4.4 M4 Deterministic premium layer

Required evidence:

- collimation baseline accuracy on benchmark pack
- ROI-aware EI refinement on bounded cases
- GSVG skip, apply, and degraded cases
- Phase 2 incremental latency and memory budgets

### 4.5 M5 Assistive AI layer

Required evidence:

- worker startup, shutdown, timeout, and crash recovery tests
- confidence and fallback behavior recorded
- assistive outputs tagged and separable from baseline output
- Phase 3 incremental latency and memory budgets

---

## 5. Sign-Off Roles

| Role | Responsibility |
|---|---|
| Development owner | confirms implementation scope and known limitations |
| QA owner | confirms evidence completeness and repeatability |
| Algorithm reviewer | confirms metric interpretation and domain correctness |
| Regulatory / documentation owner | confirms claims stay inside approved boundary |

---

## 6. Anti-Pattern Rules

The following are prohibited:

- pre-filling milestone states as `PASS`
- counting implementation intent as evidence
- using presentation-domain screenshots to prove detector-domain quality
- approving AI features without degraded-mode evidence
