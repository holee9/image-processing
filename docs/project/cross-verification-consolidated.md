# XPE Cross-Verification Consolidated Register

**Document ID**: XPE-XVER-CONSOLIDATED-001  
**Version**: 2.0.0  
**Date**: 2026-04-14  
**Status**: Living register  
**Canonical Scope**: `docs/project/`

---

## 1. Purpose

This file is a living issue register, not a historical scorecard. It tracks the architectural cross-verification items that still matter after the current document refresh.

---

## 2. Closed by This Revision

| ID | Item | Status |
|---|---|---|
| `XVER-001` | duplicate EI identifier (`SWU-2.0` vs `SWU-2.10`) | Closed |
| `XVER-002` | `.moai/project` treated as normative in DeepSync doc | Closed |
| `XVER-003` | pipeline body lacked EI baseline stage | Closed |
| `XVER-004` | pipeline Phase 2 / Phase 3 body-part contradiction | Closed |
| `XVER-005` | GSVG flag semantics mixed state bits with error codes | Closed |
| `XVER-006` | milestone UAT plan used pre-filled pass language | Closed |
| `XVER-007` | implementation analysis used stale API count and source version | Closed |
| `XVER-008` | product and structure docs diverged on unit counting | Closed |

---

## 2.1 Brainstorming Resolution

`XPE-Brainstorming-DeepSync-Execution.md` (v1.0.0) was added as a result of this verification pass.

Key decisions formalized:
- benchmark-first promotion rule (no premium claim without BP-01~BP-10 evidence)
- scalar reference before SIMD (parity harness required)
- sidecar contracts for ROI / quality-state / AI confidence (not XpeImageMetadata mutation)
- deterministic router before assistive AI worker

These decisions are now also reflected in `xpe-algorithm-spec-deepsync.md` §8.1 and `sprint-plan.md` Brainstorm-Derived Non-Negotiables.

---

## 3. Open Blocking Items

| ID | Severity | Blocking item | Owner class |
|---|---|---|---|
| `OPEN-001` | Critical | `docs/post-processing/xpe/` IEC package still needs SRS, SDD, RTM, and VVP synchronization with the current canonical architecture | documentation |
| `OPEN-002` | High | benchmark pack manifests and dataset hashes are not yet frozen in code or data | algorithm / QA |
| `OPEN-003` | High | source modules beyond `modules/common/` are not yet implemented | engineering |
| `OPEN-004` | Medium | regulatory boundary decisions for assistive AI must be adopted by release management and labels | regulatory |
