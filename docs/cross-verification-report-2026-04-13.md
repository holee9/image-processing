# XPE Document Cross-Verification Report

**Document ID**: XPE-XVER-001  
**Version**: 1.0.0  
**Date**: 2026-04-13  
**Method**: 3-Round parallel cross-verification (Consistency / Completeness / Technical Accuracy)  
**Scope**: All PRDs, IEC 62304 packages, MoAI project specs, research documents

---

## 1. Executive Summary

XPE 프로젝트 문서 체계에 대한 3회 교차검증을 수행하였다. 검증 대상은 PRD 3건, IEC 62304 패키지 3세트(XPE/GSVG/Ghost), MoAI 프로젝트 문서 5건, 기술 분류 문서 1건, 알고리즘 DeepSync 사양서 1건이다.

**결과 요약:**

| 등급 | 건수 | 주요 내용 |
|------|------|----------|
| CRITICAL | 2 | 안전 등급 불일치, Ghost 지연시간 예산 충돌 |
| HIGH | 4 | Phase 배정 불일치, 문서 동기화, GSVG 패키지 미비, RTM 누락 |
| MEDIUM | 3 | DeepSync 참조 부재, 커버리지 정의 불일치, Phase 구조 미deprecation |
| MATCH | 12 | Pipeline 순서, DLL 패키징, API 호환성 등 일관성 확인 |

---

## 2. CRITICAL Findings

### C1. Safety Class Inconsistency (Class C vs Class B)

| Document | Safety Class | Date |
|----------|-------------|------|
| XPE-PRD-001 (xray-postprocessing-prd.md) | **Class C** | 2026-04-02 |
| XPE-PRD-002 (Detailed Execution PRD) | **Class B** (currently planned) | 2026-04-13 |
| XPE-SAD-001 (Architecture) | **Class B** | 2026-04-03 |
| Ghost Correction PRD v2 | **Class B** | 2026-04-02 |

**Problem:** PRD-002 states "Final software safety class must be confirmed by system hazard analysis" but no signed hazard analysis document exists. Class B claim is not yet formally approved.

**Resolution Required:**
- [ ] System hazard analysis 수행 및 서명
- [ ] PRD-001에 deprecation notice 추가 (Class C → Class B 전환 사유)
- [ ] XPE-SRS-001에 안전 등급 공식 갱신

### C2. Ghost Correction Latency Budget Conflict

| Source | Stage (4) Budget | Tier 2 Total |
|--------|-----------------|-------------|
| Pipeline-spec (PIPE-SPEC-001 v1.1.0) | 150ms | 190ms |
| Ghost PRD v2 (sw_lag_correction_prd_v2.md) | — | < 200ms |

**Problem:** Pipeline-spec은 ghost correction stage에 150ms를 할당하지만, Ghost PRD Tier 2는 총 200ms를 요구한다. 10ms 부족은 아니지만, Tier 3(NLCSC)는 pipeline 예산을 초과할 가능성이 높다.

**Resolution Required:**
- [ ] Pipeline-spec stage (4)에 Tier별 세부 예산 명시
- [ ] Tier 3 적용 시 추가 시간 허용 (auto-escalation 조건 포함)
- [ ] Pre-processing 총 500ms 예산 내 Tier 3 재분배 방안

---

## 3. HIGH Findings

### H1. Phase Assignment Inconsistency (Body-Part Recognition / Stitching)

| Feature | PRD-001 | PRD-002 | Pipeline-Spec | DeepSync | Tech-Classification |
|---------|---------|---------|---------------|----------|---------------------|
| Body-Part Recognition | Phase 2 | Phase 3 | Phase 3 (5a) | Phase 3 | Differentiator |
| Stitching | Phase 2 | Phase 3 | Phase 3 (12) | Phase 3 | Conditional |

**Decision:** PRD-002가 실행 계획 기준으로 PRD-001을 supersede하므로, **Phase 3이 normative**. PRD-001에 deprecation notice 필요.

### H2. IEC 62304 Document Synchronization

PRD-002 Section 15에서 6개 문서의 동기화 의무를 명시하였으나, 공식 IEC 62304 패키지(SRS/SDD/RTM/VVP)는 아직 PRD-002/DeepSync 내용을 반영하지 않음.

**Action:** Phase gate 전 SRS/SDD/RTM/VVP 동기화 완료 필수.

### H3. GSVG IEC 62304 Package Incomplete

| Document | Status |
|----------|--------|
| SDP, SRS, SAD, SDD, SVP, RTM, SOUP, SHA | **Exists** |
| SCM, SRM, SMP, SPR, SRP, Compliance Matrix | **Missing** |

**Action:** GSVG 패키지 누락 6개 문서 작성 필요.

### H4. RTM Orphan Requirements

XPE-SRS-001에 정의된 SRS-PERF-005(DICOM write time)와 SRS-PERF-006(Concurrent processing)이 XPE-RTM-001에 매핑되지 않음.

**Action:** RTM에 두 항목 추가.

---

## 4. MEDIUM Findings

### M1. PRD-001 Missing DeepSync Reference

PRD-001(2026-04-02)은 DeepSync(2026-04-13) 이전에 작성되어 Tier 에스컬레이션 규칙, EI 도메인 제한, collimation sidecar 패턴이 누락됨. 구현팀이 PRD-001만 참조 시 혼란 가능.

**Action:** PRD-001에 "For normative algorithm behavior, refer to ALG-SPEC-001 (DeepSync)" 참조 추가.

### M2. Test Coverage Target Misalignment

- PRD-001: 80-90% per module (범위)
- Ghost PRD v2: Statement >= 80%, Branch >= 70% (명시적)

**Action:** Ghost PRD v2의 Statement/Branch 분리 정의를 프로젝트 표준으로 채택.

### M3. Phase Structure Redefinition Without Deprecation

PRD-001의 3-Phase(Foundation/Clinical/Intelligence) → PRD-002의 6-Phase(P0/P1a/P1b/P2/P3/RH) 전환이 명시적 deprecation 없이 진행됨.

**Action:** PRD-001 header에 "Superseded by XPE-PRD-002 for phase structure and execution planning" 명시.

---

## 5. MATCH Findings (Consistency Confirmed)

| # | Dimension | Documents Verified |
|---|-----------|-------------------|
| 1 | Core 17-stage pipeline order | PRD-001, Pipeline-Spec, DeepSync |
| 2 | Enhanced pre-processing order (stages 0-4) | Pipeline-Spec, DeepSync, product.md |
| 3 | DLL packaging (Layer 0/1/1-G/2) | product.md, SAD-001, PRD-002 |
| 4 | SAD-001 SWI partitions vs runtime DLLs | SAD-001, PRD-002 |
| 5 | API type compatibility (XpeImageBuffer vs Frame) | api-spec.md, Ghost PRD v2 |
| 6 | Calibration data structure compatibility | Ghost PRD v2, api-spec.md, pipeline-spec |
| 7 | Pixel format transition (uint16 → float32 at gain) | Pipeline-Spec, SAD-001 |
| 8 | GSVG pipeline position (stage 9) | Pipeline-Spec, SAD-001 |
| 9 | EI/DI phase split (P1b baseline, P2 ROI-aware) | PRD-002, Pipeline-Spec, DeepSync |
| 10 | XPE IEC 62304 package (13 docs complete) | File system verification |
| 11 | Ghost Correction package (5 docs complete) | File system verification |
| 12 | Backlog decomposition (9 Epics, 109 items) | XPE-PRD-003 |

---

## 6. Action Plan

### Immediate (Before Phase 0 Gate)

| Action | Owner | Deliverable |
|--------|-------|-------------|
| System hazard analysis for Class B confirmation | QA-RA | Signed hazard analysis document |
| PRD-001 deprecation notice | Tech Lead | Header update in xray-postprocessing-prd.md |
| Pipeline-spec stage (4) Tier budget clarification | Algorithm | PIPE-SPEC-001 v1.2.0 |
| RTM orphan fix (PERF-005, PERF-006) | QA | XPE-RTM-001 update |

### Before Phase 1a Gate

| Action | Owner | Deliverable |
|--------|-------|-------------|
| IEC 62304 package sync with PRD-002/DeepSync | QA-RA | SRS/SDD/RTM/VVP revision |
| Test coverage standard adoption | Tech Lead | quality.yaml update |
| DeepSync reference in PRD-001 | Tech Lead | Cross-reference note |

### Before Phase 2 Gate

| Action | Owner | Deliverable |
|--------|-------|-------------|
| GSVG package completion (6 missing docs) | QA-RA | SCM/SRM/SMP/SPR/SRP/Compliance |

---

*Report End — XPE-XVER-001 v1.0.0*
