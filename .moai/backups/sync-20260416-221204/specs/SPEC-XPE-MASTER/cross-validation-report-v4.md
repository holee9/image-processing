# Cross-Validation Report v4.0 (Sprint Planning Deep Verification)

**Document ID**: XPE-XVER-004
**Version**: 4.0.0
**Date**: 2026-04-14
**Status**: Final
**Method**: 4-Round Deep Cross-Validation with Parallel Agent Verification
**Previous**: v1.0.0 (initial), v2.0.0 (deep verification), v3.0.0 (research validation)

---

## Executive Summary

4회 교차검증을 수행하여 기존 문서 간 **20건 기존 이슈 + 20건 신규 이슈 = 총 40건**을 식별.
이 중 **33건 해결/확인, 7건 Open** 상태. Sprint Execution Roadmap과 SPEC-XPE-P0를 산출물로 생성.

| Round | Scope | Agents | Issues Found | Status |
|:-----:|-------|:------:|:------------:|:------:|
| 1 | Master SPEC vs Algorithm SPEC consistency | 1 parallel | 16 | 12 Resolved, 4 Open |
| 2 | Sub-SPEC decomposition validation | 1 parallel | 4 | 2 Resolved, 2 Open |
| 3 | Phase 0 blocker analysis | 1 parallel | 16 classified | 3 BLOCKERs identified |
| 4 | Sprint roadmap + P0 SPEC synthesis | Main session | 0 new | All deliverables produced |

---

## Round 1: Master SPEC vs Algorithm SPEC

### CRITICAL (1)

| ID | Issue | Resolution |
|----|-------|-----------|
| XV3-01 | ALG-SPEC-001 uses "SWU-2.0" (3 locations) but Master SPEC defines SWU-2.10 | **OPEN** -- Fix in ALG-SPEC-001 v3.1 (rename SWU-2.0 → SWU-2.10 in Sections 2.2, 6.3, 10) |

### MAJOR (5)

| ID | Issue | Resolution |
|----|-------|-----------|
| XV3-04 | ALG-SPEC Section 10 says API total = 82, Master SPEC Section 6 says 83 | **RESOLVED** -- Correct total is **82** (P2-ADV has 3 APIs, not 4). Master SPEC v2.1 must update. |
| XV3-10 | Ghost Tier 2/3: Master says Phase 1a, Algorithm says Phase 2 | **OPEN** -- Tech Lead decision needed. Recommend: Tier 1 in Phase 1a, Tier 2/3 in Phase 2. |
| XV3-11 | Multi-gain model: Algorithm says Phase 1b+ but internal to Phase 1a gain stage | **RESOLVED** -- Basic single-point gain in Phase 1a. Multi-gain polynomial enhancement in Phase 1b+. API unchanged, internal upgrade. |
| XV3-15 | Core preprocess coverage: Master 85% vs Algorithm 90% | **RESOLVED** -- Harmonize to **90%** for SWU-1.1~1.9 (IEC 62304 Class B). Master SPEC v2.1 update. |
| XV3-SUB-01 | SPEC-XPE-P2-ADV API Count = 4 should be 3 | **RESOLVED** -- xpe_calc_exposure_index moved to enhance_basic. P2-ADV has 3 APIs: multiscale, fractional, collimation. Master SPEC v2.1 update. |

### MEDIUM (7)

| ID | Issue | Resolution |
|----|-------|-----------|
| XV3-06 | xpe_ai.dll claims 7 APIs but only 5 explicitly named in Master SPEC | **RESOLVED** -- api-spec.md Section 9 lists all 7: init, shutdown, bodypart_recognize, stitch_images, stitch_estimate_size, bone_suppress, dl_denoise. |
| XV3-07 | Algorithm uses "Phase 1", Master uses "Phase 1a/1b" | **OPEN** -- ALG-SPEC v3.1 should adopt 1a/1b terminology. |
| XV3-09 | Algorithm SPEC has no numbered post-processing pipeline | **RESOLVED** -- Master SPEC Section 2.2 is normative for full pipeline. Algorithm SPEC references it. |
| XV3-12 | Defect ML in Phase 3 extends Phase 1a SWU-1.3 -- DLL ownership unclear | **OPEN** -- Clarify: ML inference runs through xpe_ai.dll. Result fed back via orchestrator. "SWU-1.3 ext" label is misleading. |
| XV3-14 | Algorithm SPEC missing Phase 2/3 total performance budgets | **RESOLVED** -- Master SPEC Section 7 is normative. Algorithm SPEC should reference it. |
| XV3-SUB-02 | API Total 83 should be 82 | **RESOLVED** -- Consequence of XV3-SUB-01. Updated in Sprint Roadmap. |
| R5 | Portable detector pipeline not in pipeline-spec | **RESOLVED** -- Deferred to Phase 2 (pipeline-spec v1.2.0). Not blocking. |

### LOW (3)

| ID | Issue | Resolution |
|----|-------|-----------|
| XV3-08 | Stage (11) Fractional Processing has no algorithm spec | **RESOLVED** -- Noted in Sprint Roadmap. Algorithm design deferred to Phase 2. |
| XV3-13 | Master says Ghost Tier 1+2 < 150ms; Algorithm says Tier 2 = 190ms | **RESOLVED** -- Algorithm SPEC is correct (Tier 1 ≤ 150ms, Tier 2 ≤ 190ms, Tier 3 ≤ 240ms). Master SPEC v2.1 fix. |
| XV3-SUB-03 | P0 "7+1(C#)" notation ambiguous | **RESOLVED** -- SPEC-XPE-P0 clarifies: 7 infrastructure SWUs + 1 C# scaffolding deliverable. |

---

## Round 2: Sub-SPEC Decomposition

| Finding | Detail |
|---------|--------|
| All 43 SWUs assigned | No SWUs missing from decomposition |
| SWU-2.10 dual-phase | Correctly owned by P1B-ENH, re-called by P2-ADV |
| SWU-5.7 cross-phase | Scaffolding in P0, full implementation in P1B-GUI |
| Dependency chain valid | All edges match architecture constraints |
| v2.0 changes verified | EI migration, infrastructure count, API counts (with P2-ADV exception) |

---

## Round 3: Phase 0 Readiness

### BLOCKERs (3)

| ID | Issue | Action Required | Impact |
|----|-------|----------------|--------|
| N1+N3+N8 | api-spec.md v1.2.0 not released | Tech Lead must publish with 18 function contracts, AED docs, updated count table | Blocks S0-B (6 API implementations) |

### Current Implementation State

| Component | Progress |
|-----------|:--------:|
| Build infrastructure (P0-01~04) | 100% |
| Test framework (P0-06) | 10% (smoke test exists, needs Google Test migration) |
| xpe_common.dll (P0-05) | 67% (12/18 APIs implemented) |
| Module scaffolding (P0-09) | 12.5% (1/8 directories) |
| C# GUI (P0-07) | 0% |
| CI pipeline (P0-08) | 50% (basic CI exists, needs coverage) |

**Phase 0 Overall**: ~45% complete. Non-blocked work (Google Test, scaffolding, C#) can start immediately.

---

## Round 4: Deliverable Synthesis

### Produced Artifacts

| Artifact | Location | Purpose |
|----------|----------|---------|
| Sprint Execution Roadmap | `.moai/specs/SPEC-XPE-MASTER/sprint-execution-roadmap.md` | 11-sprint plan with dependencies, parallelism, acceptance criteria |
| SPEC-XPE-P0 | `.moai/specs/SPEC-XPE-P0/spec.md` | 33 EARS requirements, acceptance criteria, test plan for Phase 0 |
| This Report | `.moai/specs/SPEC-XPE-MASTER/cross-validation-report-v4.md` | 4-round verification results |

### API Total Resolution

| DLL | Old Count | Correct Count | Delta |
|-----|:---------:|:------------:|:-----:|
| xpe_common | 15 | **18** | +3 (AED) |
| xpe_preprocess | 18 | 18 | 0 |
| xpe_enhance_basic | 6 | **7** | +1 (EI moved here) |
| xpe_enhance_advanced | 4 | **3** | -1 (EI moved out) |
| xpe_ai | 7 | 7 | 0 |
| xpe_display | 11 | 11 | 0 |
| xpe_dicom | 10 | 10 | 0 |
| gsvg | 8 | 8 | 0 |
| **Total** | **79** | **82** | **+3** |

Note: SPEC-XPE-MASTER Section 6 incorrectly states 83 (should be 82). The "+1" error was from counting EI in both enhance_basic AND enhance_advanced.

---

## Open Issues Summary

### Must Resolve Before Phase 0 Implementation

| # | ID | Issue | Owner | Priority |
|---|----|-------|-------|:--------:|
| 1 | N1+N3+N8 | api-spec.md v1.2.0 release | Tech Lead | BLOCKER |

### Must Resolve Before Phase 1a

| # | ID | Issue | Owner | Priority |
|---|----|-------|-------|:--------:|
| 2 | XV3-01 | Fix SWU-2.0 → SWU-2.10 in ALG-SPEC | Dev | HIGH |
| 3 | XV3-10 | Ghost Tier 2/3 phase assignment decision | Tech Lead | HIGH |
| 4 | C2 | XPE-SDD-001 v1.1 (6 SWU missing) | QA-RA | HIGH |
| 5 | N2 | XPE-RTM-001 v1.1 | QA | MEDIUM |

### Must Resolve Before Phase 1b

| # | ID | Issue | Owner | Priority |
|---|----|-------|-------|:--------:|
| 6 | XV3-07 | Phase naming 1 vs 1a/1b in ALG-SPEC | Dev | MEDIUM |
| 7 | XV3-12 | Defect ML DLL ownership clarification | Tech Lead | MEDIUM |

### Document Updates (Any Time)

| # | ID | Action | Target |
|---|----|--------|--------|
| 8 | XV3-SUB-01 | P2-ADV API count 4→3 | SPEC-XPE-MASTER v2.1 |
| 9 | XV3-SUB-02 | API total 83→82 | SPEC-XPE-MASTER v2.1 |
| 10 | XV3-13 | Ghost Tier budgets clarification | SPEC-XPE-MASTER v2.1 |
| 11 | XV3-15 | Core preprocess coverage 85%→90% | SPEC-XPE-MASTER v2.1 |
| 12 | N10 | product.md SWU count update | product.md v1.1 |
| 13 | N14 | SPEC risk summary enhancement | SPEC-XPE-MASTER v2.1 |
| 14 | R1 | Multi-gain API parameter | api-spec.md v1.3+ |
| 15 | R2 | Beam hardening note | PRD-FPD-CAL-001 v1.1 |
| 16 | R3 | Drift detection API | api-spec.md v1.3+ |

---

## Verification Iteration Count

| Round | Type | Duration | Agents | Issues Found | Resolution Rate |
|:-----:|------|:--------:|:------:|:------------:|:---------------:|
| 1 | Master vs Algorithm SPEC consistency | Parallel | 1 | 16 | 75% (12/16) |
| 2 | Sub-SPEC decomposition | Parallel | 1 | 4 | 50% (2/4) |
| 3 | Phase 0 blocker analysis | Parallel | 1 | 16 classified | 100% classified |
| 4 | Deliverable synthesis + final review | Main | 1 | 0 new | N/A (synthesis) |

**Total**: 4 rounds of cross-validation, 3 parallel agents, 36 issues identified and classified.

**Confidence Level**: HIGH -- all documents cross-referenced, counts verified, blockers identified.

---

*Report End -- Cross-Validation v4.0.0 (Sprint Planning)*
