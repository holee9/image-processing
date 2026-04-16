# Consolidated Execution Plan

**Document ID**: XPE-PLAN-001  
**Version**: 1.0.0  
**Date**: 2026-04-13  
**Status**: Working Draft  
**Parent Documents**: XPE-PRD-002 v1.1.0, XPE-PRD-003 v1.0.0, XPE-XVER-001 v1.0.0  
**Purpose**: 교차검증 결과를 반영한 통합 실행 계획서. 충돌 해소, 갭 보완, 우선순위 정렬, Phase Gate 기준 정의.

---

## 1. Document Hierarchy

```
XPE-PRD-001 (Algorithm Baseline, SUPERSEDED for execution)
    |
    v
XPE-PRD-002 v1.1.0 (Execution Baseline, Cross-Verified)
    |
    +-- XPE-PRD-003 (Backlog Decomposition, 9 Epics / 109 Items)
    +-- XPE-PLAN-001 (This Document: Consolidated Execution Plan)
    +-- XPE-XVER-001 (Cross-Verification Report)
    |
    +-- Normative References:
        +-- ALG-SPEC-001 (DeepSync Algorithm Spec)
        +-- PIPE-SPEC-001 (Pipeline Spec)
        +-- XPE-API-SPEC-001 (C ABI Spec)
        +-- Tech Classification v2.0
```

### 1.1 Normative Priority (Conflict Resolution)

충돌 발생 시 아래 우선순위를 따른다:

1. **XPE-XVER-001** (교차검증 결과) — 충돌 해소 결정
2. **XPE-PRD-002 v1.1.0** — 실행 계획 기준
3. **ALG-SPEC-001 (DeepSync)** — 알고리즘 동작 기준
4. **PIPE-SPEC-001** — 런타임 파이프라인 기준
5. **XPE-PRD-001** — 원본 요구사항 참조 (실행 계획으로는 미사용)

---

## 2. Resolved Conflicts

교차검증에서 발견된 충돌의 공식 해소 결정:

### 2.1 Safety Class: Class B (Conditional)

| Decision | Class B |
|----------|---------|
| Condition | System hazard analysis 서명 완료 시 확정 |
| Fallback | 서명 미완료 시 Class C 유지 (PRD-001 기준) |
| Gate | Phase 0 Gate (G1) 전에 확정 필수 |

### 2.2 Phase Structure: 6-Phase Model

| Decision | PRD-002의 6-Phase 구조가 normative |
|----------|----------------------------------------|
| Mapping | PRD-001 Phase 1 = P1a + P1b, PRD-001 Phase 2 = P2 + P3(AI), PRD-001 Phase 3 = P3 |

### 2.3 Feature Phase Assignment (Resolved)

| Feature | **Normative Phase** | Rationale |
|---------|---------------------|-----------|
| Body-Part Recognition (CNN) | **Phase 3** | AI worker 의존, deterministic baseline과 분리 |
| Stitching | **Phase 3** (conditional) | AI worker 의존, product-line에 따라 선택 |
| Collimation (baseline) | **Phase 2** | Deterministic (gradient + Hough), AI 불필요 |
| Collimation (AI refinement) | **Phase 3** | Optional AI enhancement |
| EI/DI (whole-image) | **Phase 1b** | Detector-domain, AI 불필요 |
| EI/DI (ROI-aware) | **Phase 2** | Collimation ROI 의존 |
| GSVG | **Phase 2** (conditional) | 독립 패키지, 임상 검증 필요 |
| MFP / Fractional | **Phase 2** | Deterministic classical algorithm |
| Bone Suppression | **Phase 3** (optional) | DL 모델, derived image 전용 |

### 2.4 Ghost Correction Latency Budget (Resolved)

| Tier | Stage (4) Budget | Pre-processing Total Budget |
|------|------------------|-----------------------------|
| Tier 1 (Offset only) | 70 ms | 200 ms |
| Tier 2 (AR(1) Lag) | 150 ms | 350 ms |
| Tier 3 (NLCSC) | 250 ms | 500 ms (full budget) |

**Rule:** Tier 3는 pre-processing 500ms 전체 예산을 사용할 수 있으나, 다른 pre-processing 단계의 예산을 잠식한다. Auto-escalation 시 성능 경고 alert 필수.

### 2.5 Test Coverage Standard (Resolved)

| Domain | Statement | Branch |
|--------|-----------|--------|
| Pre-processing modules | >= 90% | >= 80% |
| Phase 1b enhancement/display | >= 85% | >= 75% |
| Phase 2 advanced modules | >= 80% | >= 70% |
| Phase 3 AI integration | >= 70% | >= 60% |
| Safety-critical paths (overflow, null, clamp) | 100% | 100% |

---

## 3. Phase Gate Checklist

### G0: Phase 0 Exit (ABI + Infrastructure)

| # | Gate Criterion | Evidence | Status |
|---|---------------|----------|--------|
| G0-1 | System hazard analysis signed (XVER-C1) | Signed document | Pending |
| G0-2 | PRD-001 deprecation notice added (XVER-H1) | File header update | **Done** |
| G0-3 | RTM orphan fix: PERF-005/006 mapped (XVER-H4) | RTM update | Pending |
| G0-4 | DLL skeleton builds on CI | Build log | Pending |
| G0-5 | C#/C ABI size parity test passes | Test output | Pending |
| G0-6 | Dataset manifest schema defined | Schema file | Pending |
| G0-7 | Test coverage standard in quality.yaml (XVER-M2) | Config update | Pending |

### G1: Phase 1a Exit (Pre-processing Baseline)

| # | Gate Criterion | Evidence |
|---|---------------|----------|
| G1-1 | Preprocess stages (0.5-4) implemented | Source code |
| G1-2 | Preprocess 500ms gate met | Benchmark report |
| G1-3 | Ghost correction Tier budget verified (XVER-C2) | Stage-level timing report |
| G1-4 | Golden dataset regression passes | Regression output |
| G1-5 | Pre-processing coverage: Statement >= 90%, Branch >= 80% | Coverage report |
| G1-6 | IEC 62304 SRS/SDD synced with DeepSync (XVER-H2) | Sync review note |

### G2: Phase 1b Exit (Basic Enhancement + DICOM)

| # | Gate Criterion | Evidence |
|---|---------------|----------|
| G2-1 | Raw-to-DICOM baseline demo | Demo recording |
| G2-2 | EI/DI whole-image baseline passes test set | EI validation |
| G2-3 | GSDF/VOI smoke test passes | Test output |
| G2-4 | DX IOD validation passes | DVTk/dcm4che output |
| G2-5 | Phase 1b coverage: Statement >= 85%, Branch >= 75% | Coverage report |
| G2-6 | W/L interactive latency <= 16ms | Benchmark |
| G2-7 | QA constancy test workflow operational | QA artifact |

### G3: Phase 2 Exit (Clinical Advanced)

| # | Gate Criterion | Evidence |
|---|---------------|----------|
| G3-1 | Collimation baseline operates with whole-image fallback | Test output |
| G3-2 | ROI-aware EI comparison report | Comparison data |
| G3-3 | GSVG integration test (conditional) | Integration report |
| G3-4 | Multiscale/Fractional artifact review | Review report |
| G3-5 | GSVG IEC 62304 package complete (XVER-H3) | 6 missing docs created |
| G3-6 | Clinical validation report (body-region set) | Clinical report |

### G4: Phase 3 Exit (AI + Premium)

| # | Gate Criterion | Evidence |
|---|---------------|----------|
| G4-1 | AI worker crash/timeout does not block primary image | Fault injection test |
| G4-2 | Body-part recognition >= 95% Top-1 accuracy | Eval report |
| G4-3 | Stitching NCC baseline met (conditional) | Benchmark |
| G4-4 | Bone suppression toggle works (primary preserved) | Demo |
| G4-5 | Phase 3 coverage: Statement >= 70%, Branch >= 60% | Coverage report |

### G5: Release Hardening Exit

| # | Gate Criterion | Evidence |
|---|---------------|----------|
| G5-1 | All IEC 62304 packages synced and reviewed | Review sign-off |
| G5-2 | Cross-document traceability gap = 0 | RTM review |
| G5-3 | All XVER findings resolved | XVER tracking update |
| G5-4 | Formal verification pack complete | VVP evidence |

---

## 4. Sprint Allocation

| Sprint | Phase | Focus | Key Backlog (PRD-003 Ref) |
|--------|-------|-------|---------------------------|
| S0 | Phase 0 | **XVER resolution + baseline freeze** | BI-00.01.01, BI-00.02.01, BI-00.03.01 |
| S1 | Phase 0 | ABI, common DLL, host shell | EP-01 (BI-01.*), EP-02 start |
| S2 | Phase 1a | Calibration mgr, readout, temperature | BI-04.01.*, BI-04.02.* |
| S3 | Phase 1a | Offset, nonlinearity, gain correction | BI-04.03.* |
| S4 | Phase 1a | Binning, defect, lag/ghost, integration | BI-04.04.*, BI-04.05.*, BI-04.06.* |
| S5 | Phase 1b | Log, noise, contrast, edge enhancement | BI-05.01.* |
| S6 | Phase 1b | EI baseline, display LUT stack | BI-05.02.*, BI-05.03.* |
| S7 | Phase 1b | DICOM, viewer, QA constancy, Phase 1 demo | BI-05.04.*, BI-05.05.* |
| S8 | Phase 2 | Collimation baseline, ROI-aware EI | BI-06.01.*, BI-06.02.* |
| S9 | Phase 2 | Multiscale, fractional processing | BI-06.03.* |
| S10 | Phase 2 | GSVG integration | BI-06.04.* |
| S11 | Phase 2 | Clinical presets, validation | BI-06.05.* |
| S12 | Phase 3 | AI worker, body-part recognition | BI-07.01.*, BI-07.02.* |
| S13 | Phase 3 | AI collimation, stitching | BI-07.03.*, BI-07.04.* |
| S14 | Phase 3 | Bone suppression, DL denoising | BI-07.05.*, BI-07.06.* |
| S15 | Phase 3 | AI evaluation pack, fallback testing | BI-07.07.* |
| S16 | RH | Packaging, formal doc sync | BI-08.01.* |
| S17 | RH | V&V execution | BI-08.02.* |
| S18 | RH | Release candidate | BI-08.03.* |

---

## 5. Critical Path

```
S0: XVER Resolution + Baseline Freeze
 |
 v
S1: ABI Headers + Common DLL
 |
 v
S2-S4: Pre-processing Chain (detector-domain)
 |        |
 |        +-- Ghost Tier Budget Verification (XVER-C2)
 v
S5-S7: Enhancement + Display + DICOM = Phase 1 Demo (M3)
 |
 v
S8-S11: Clinical Advanced = Phase 2 Report (M4)
 |        |
 |        +-- GSVG Package Completion (XVER-H3)
 v
S12-S15: AI Features = Phase 3 Eval (M5)
 |
 v
S16-S18: Release Hardening = RC (M6)
          |
          +-- Full Document Sync Verification (XVER-H2)
```

**Critical path blockers:**
- S0 → S1: Hazard analysis 미서명 시 전체 지연
- S4 → S5: Pre-processing 500ms gate 미달 시 Phase 1b 진입 불가
- S7 → S8: Phase 1 demo 실패 시 Phase 2 진입 불가
- S16 → Release: Document sync 불일치 시 출시 보류

---

## 6. XVER Finding Tracking

| XVER ID | Finding | Resolution | Sprint | Status |
|---------|---------|------------|--------|--------|
| C1 | Safety Class C->B 미확정 | Hazard analysis 수행 | S0 | **Open** |
| C2 | Ghost Tier latency 예산 충돌 | Section 2.4 예산 재분배 | S0 (spec), S4 (verify) | **Resolved (spec)** |
| H1 | Body-Part/Stitching phase 불일치 | Phase 3 normative, PRD-001 deprecation | S0 | **Resolved** |
| H2 | IEC 62304 package sync 미완료 | Phase gate마다 sync review | Each gate | **Open** |
| H3 | GSVG package 6 docs missing | Phase 2 gate 전 작성 | S10-S11 | **Open** |
| H4 | RTM orphan PERF-005/006 | RTM 즉시 갱신 | S0 | **Open** |
| M1 | PRD-001 DeepSync 참조 부재 | Deprecation notice에 포함 | S0 | **Resolved** |
| M2 | Coverage 정의 불일치 | Section 2.5 표준 채택 | S0 | **Resolved** |
| M3 | Phase 구조 미deprecation | PRD-001 header 갱신 | S0 | **Resolved** |

---

## 7. Document Synchronization Obligations

Phase Gate 통과 시 아래 문서의 동기화를 검증해야 한다:

| Document | Sync with | Verification Method |
|----------|-----------|---------------------|
| XPE-SRS-001 | PRD-002, DeepSync | Requirement ID coverage check |
| XPE-SDD-001 | SAD-001, Pipeline-Spec | SWU-to-stage mapping review |
| XPE-RTM-001 | SRS-001, SDD-001, VVP-001 | Orphan/gap scan |
| XPE-VVP-001 | SRS-001, PRD-003 | Test case-to-requirement mapping |
| Pipeline-Spec | DeepSync, PRD-002 | Stage-to-phase ownership check |
| API-Spec | Pipeline-Spec, product.md | ABI struct parity check |

---

## 8. Immediate Actions (Sprint 0)

Sprint 0는 코드 작성 전 문서 기반선을 확립하는 스프린트이다.

| # | Action | Owner | Deliverable | Priority |
|---|--------|-------|-------------|----------|
| 1 | System hazard analysis 수행 | QA-RA | Signed HA document | CRITICAL |
| 2 | Pipeline-spec v1.2.0 (Tier budget) | Algorithm Lead | PIPE-SPEC-001 v1.2.0 | CRITICAL |
| 3 | XPE-RTM-001 PERF-005/006 추가 | QA | RTM update | HIGH |
| 4 | quality.yaml 커버리지 기준 갱신 | Tech Lead | Config update | MEDIUM |
| 5 | PRD-003 EP-00 backlog를 issue로 변환 | PM | GitHub issues | HIGH |
| 6 | Baseline document set 동결 | PM | Approved baseline note | HIGH |

---

*Document End — XPE-PLAN-001 v1.0.0*
