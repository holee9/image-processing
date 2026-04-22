# XPE Cross-Verification Consolidated Register

**Document ID**: XPE-XVER-CONSOLIDATED-001  
**Version**: 5.0.0  
**Date**: 2026-04-20  
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

## 2.2 Cross-Verification Round 5 (2026-04-15) Resolutions

**Scope**: SPEC-DOC-001 implementation — MRD, System PRD, System V&V Plan creation.

### Closed by Round 5

| ID | Item | Resolution |
|---|---|---|
| `CV-002` | Safety Class B/C 미확정 | **CLOSED** — Class B confirmed in XPE-PRD-SYSTEM-001 §3. Working assumption pending ISO 14971 Hazard Analysis signature. |
| `CV-005` | Ghost latency budget conflict (Pipeline-spec 150ms vs Ghost PRD 200ms) | **CLOSED** — Tier-based budget normative in XPE-PRD-SYSTEM-001 §7: Tier 1 ≤100ms, Tier 2 ≤200ms, Tier 3 ≤400ms (extended mode). |

### New documents created (2026-04-15)

| Document | Path | Purpose |
|---|---|---|
| XPE-MRD-001 v1.0.0 | `docs/project/XPE-MRD-001_Market_Requirements_Document.md` | Market requirements (top of hierarchy) |
| XPE-PRD-SYSTEM-001 v1.0.0 | `docs/project/XPE-PRD-SYSTEM-001_System_Product_Requirements.md` | System PRD, integrates product.md |
| XPE-SVVP-001 v1.0.0 | `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` | System V&V Plan (6-level hierarchy) |

---

## 2.3 Cross-Verification Round 6 (2026-04-15 continued) Resolutions

**Scope**: SPEC-DOC-002 partial — OPEN-001/004/005 close-out via document updates.

### Closed by Round 6

| ID | Item | Resolution |
|---|---|---|
| `OPEN-004` | AI regulatory boundary decisions not formalized | **CLOSED** — XPE-AI-REG-001 v1.0.0 created at `docs/project/XPE-AI-REG-001_AI_Regulatory_Strategy.md`. Defines: AI feature classification (assistive/Non-SaMD justification), PICOS per AI function, training data provenance requirements, labeling claim statements, FDA/EU MDR/MFDS regulatory pathways, PMS plan. 4 residual open items tracked within XPE-AI-REG-001 §9. |
| `OPEN-005` | ISO 14971 System Hazard Analysis signature missing; HAZ-010/011/012 absent | **CLOSED (document)** — XPE-SHA-001 updated to v2.0 (2026-04-15): HAZ-010 (AI confidence misuse), HAZ-011 (multi-package cascade failure), HAZ-012 (clinical misread scenario) added. All sections (§4~§8) updated for 12 hazards. §9 ISO 14971 formal approval sign-off block added with 4 signature lines. **Signature action still required** before Phase 1a gate — see §9 ACTION REQUIRED. |

### Partially Closed by Round 5+6

| ID | Item | Status |
|---|---|---|
| `OPEN-001` | IEC 62304 package sync | **PARTIALLY CLOSED** — RTM v1.2: §5 MR→PR backward traceability (28 rows) added. SHA v2.0: HAZ-010/011/012 and sign-off block added. VVP v1.1: parent reference to XPE-SVVP-001 added. **Remaining**: SRS/SDD full synchronization with XPE-PRD-SYSTEM-001 (tracked as SPEC-DOC-002). |

### New documents created (2026-04-15, Round 6)

| Document | Path | Purpose |
|---|---|---|
| XPE-AI-REG-001 v1.0.0 | `docs/project/XPE-AI-REG-001_AI_Regulatory_Strategy.md` | AI regulatory strategy — closes OPEN-004 |

### Updated documents (2026-04-15, Round 6)

| Document | Version | Change Summary |
|---|---|---|
| XPE-SHA-001 | 1.0 → 2.0 | HAZ-010/011/012 added; ISO 14971 sign-off block added |
| XPE-VVP-001 | 1.0 → 1.1 | Parent reference to XPE-SVVP-001 added in header |
| XPE-RTM-001 | 1.1 → 1.2 | §5 MR→PR backward traceability (28 rows) added |

---

## 2.4 Cross-Verification Round 7 (2026-04-20) — 3-Lane Integration

**Scope**: 3개 Lane (pre/post/gui) → main squash merge 통합 후 상태 검증

### Closed by Round 7

| ID | Item | Resolution |
|---|---|---|
| `OPEN-003-partial` | source modules beyond `modules/common/` not implemented | **PARTIALLY CLOSED** — xpe_preprocess (SPEC-XPE-P1A M2 완료, 202 tests), xpe_enhance_advanced (SPEC-XPE-P2-ADV 완료, 97/103 tests). 잔존: enhance_basic(done), gsvg/ai/display/dicom 미착수. |

### Integration Status (2026-04-20)

| Lane | Branch | SPEC | Tests | Status |
|---|---|---|---|---|
| A (Pre) | dev/preprocess | SPEC-XPE-P1A M2 | 202/202 | ✅ main merged (e9b8ed4) |
| B (Post) | dev/postprocess | SPEC-XPE-P2-ADV | 97/103 | ✅ main merged (f057d9e) |
| C (GUI) | dev/gui | SPEC-XPE-GUI-CALIB-001 | — | ✅ main merged (c746e20) |

### New issues created (2026-04-20)

| ID | Title | Status |
|---|---|---|
| GitHub #46 | GUI viewer brightness/contrast/histogram | Open — TASK-GUI-VIEWER-001 문서 추가, 구현 예정 |
| GitHub #47 | GUI evaluation viewer 탭 구조 모듈화 | Open — 계획 수립 중 |

### Artifact cleanup (2026-04-20)

- build_test2/, build_test3/, lint_results.json .gitignore에 추가 (dev/postprocess 빌드 아티팩트 오염 수정)

---

## 3. Open Blocking Items

| ID | Severity | Blocking item | Owner class |
|---|---|---|---|
| `OPEN-001` | High | SRS and SDD full synchronization with XPE-PRD-SYSTEM-001 canonical architecture. RTM/SHA/VVP partially fixed (see §2.3). Track as SPEC-DOC-002. | documentation |
| `OPEN-002` | High | benchmark pack manifests and dataset hashes are not yet frozen in code or data | algorithm / QA |
| `OPEN-003` | Medium | GSVG/AI/display/dicom 모듈 미착수 (pre/post/enhance_advanced는 완료) | engineering |
| `OPEN-004-ADV` | Medium | xpe_enhance_advanced 테스트 6건 미통과 (97/103, 94.17% → 목표 95%+) | QA |
| `OPEN-005-ACTION` | Critical | ISO 14971 formal signature of XPE-SHA-001 §9 required before Phase 1a gate. Document is complete (v2.0); 4 human signatures pending. | regulatory / QA |
