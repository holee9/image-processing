# Sprint Execution Roadmap v2.0.0

**Document ID**: SPRINT-ROADMAP-001
**Version**: 2.0.0
**Date**: 2026-04-14
**Status**: Active
**Source**: SPEC-XPE-MASTER v2.0.0, Cross-Validation v5.0.0 (8-round)
**Classification**: IEC 62304 Class B

---

## Changelog (v1.0.0 -> v2.0.0)

| Change | Detail |
|--------|--------|
| EARS SPEC gating added | Each sprint now requires EARS SPEC as entry criterion |
| R8-01~R8-04 resolutions | Architecture issues from Round 8 integrated |
| Pre-sprint checklist added | Mandatory verification steps before each sprint |
| Coverage harmonized | Core preprocess 90%, others 85%, AI 80% |

---

## 1. Sprint Methodology (Enhanced)

### 1.1 Sprint Cycle

```
EARS-SPEC -> PLAN -> IMPLEMENT -> REVIEW -> EVALUATE -> FIX -> (repeat)
```

**v2.0 Enhancement**: EARS-SPEC 단계 추가. 각 Phase의 구현 전에 EARS 형식 요구사항 문서가 반드시 작성되어야 함.

| Phase | Entry Criteria | Exit Criteria |
|-------|----------------|---------------|
| EARS-SPEC | Sprint backlog + api-spec available | EARS requirements reviewed and approved |
| PLAN | EARS SPEC approved | Task breakdown + acceptance criteria confirmed |
| IMPLEMENT | Plan approved | All deliverables coded + unit tests written |
| REVIEW | Implementation complete | Code review passed, static analysis clean |
| EVALUATE | Review passed | All acceptance criteria met, quality gates green |
| FIX | Evaluation failed | All defects resolved, re-evaluate |

### 1.2 Quality Gates per Sprint (v2.0 Harmonized)

| Gate | Phase 0 | Phase 1a | Phase 1b | Phase 2 | Phase 3 |
|------|---------|----------|----------|---------|---------|
| Unit Test Coverage | >= 85% | **>= 90%** | >= 85% | >= 85% | >= 80% |
| Branch Coverage | >= 70% | **>= 80%** | >= 70% | >= 70% | >= 60% |
| Static Analysis | 0 warnings | 0 warnings | 0 warnings | 0 warnings | 0 warnings |
| Memory Leak (1000 frames) | Pass | Pass | Pass | Pass | Pass |
| Performance Budget | N/A | < 500ms | < 3000ms | < 2500ms | < 3000ms |
| EARS Traceability | 33 REQ | ~45 REQ | ~70 REQ | ~35 REQ | ~20 REQ |
| P/Invoke ABI Test | Pass | Pass | Pass | Pass | Pass |

### 1.3 Iteration Rules (GAN Loop Inspired)

- Maximum 5 iterations per sprint
- Escalation after 3 iterations without >= 5% improvement
- Stagnation: 2 consecutive iterations < 5% -> user escalation
- Acceptance criteria completion tracked in progress.md

---

## 2. Sprint Structure (v2.0)

### 2.1 Sprint Overview

| Sprint | SPEC | Phase | DLL | SWU | API | EARS REQ | Priority | Dependency |
|--------|------|-------|-----|:---:|:---:|:--------:|:--------:|:----------:|
| S0-A | SPEC-XPE-P0 | 0 | Build/CMake | -- | -- | 8 | Must | None |
| S0-B | SPEC-XPE-P0 | 0 | xpe_common | 7 | 18 | 25 | Must | S0-A |
| S0-C | SPEC-XPE-P0 | 0 | ImageProcTest | 1 | N/A | 3 | Must | S0-A |
| S1-A | **SPEC-XPE-P1A** | 1a | xpe_preprocess | 9 | 18 | **~45** | Must | S0-B |
| S1-B1 | **SPEC-XPE-P1B-ENH** | 1b | xpe_enhance_basic | 5 | 7 | **~28** | Must | S1-A |
| S1-B2 | **SPEC-XPE-P1B-DISP** | 1b | xpe_display | 4 | 11 | **~22** | Must | S1-A |
| S1-B3 | **SPEC-XPE-P1B-DICOM** | 1b | xpe_dicom | 4 | 10 | **~22** | Must | S1-A |
| S1-B4 | **SPEC-XPE-P1B-GUI** | 1b | ImageProcTest | 1+1 | N/A | **~12** | Should | S1-B1 |
| S2-A | **SPEC-XPE-P2-ADV** | 2 | xpe_enhance_advanced | 3 | 3 | **~18** | Must | S1-B1 |
| S2-B | **SPEC-XPE-P2-GSVG** | 2 | gsvg | 4 | 8 | **~22** | Must | S1-A |
| S3 | **SPEC-XPE-P3-AI** | 3 | xpe_ai + worker | 4 | 7 | **~18** | Should | S2-A, S1-B4 |

**Total**: 11 sprints, **82 APIs**, **43 SWUs**, **~223 EARS requirements**

### 2.2 Dependency Graph (Verified)

```
S0-A ──────────────┐
                    ├── S0-C ────────────┐
S0-B ──────────────┘                     │
                                         ├── S1-B1 ──────┐
S1-A ─────────────────────────────────────┤               ├── S2-A ──── S3
                                         ├── S1-B2 ──────┤
                                         ├── S1-B3 ──────┤
                                         └── S1-B4 ──────┘
S2-B ──────────────────────────────────── (parallel with S2-A)
```

**Critical Path**: S0-A -> S0-B -> S1-A -> S1-B1 -> S2-A -> S3

---

## 3. Pre-Sprint Checklists (NEW in v2.0)

### 3.1 Before S0-B (xpe_common.dll)

- [ ] api-spec.md v1.2.0 published (BLOCKER R6-01)
- [ ] SPEC-XPE-P0 REQ-P0-026~028 corrected to match api-spec signatures (R8-01)
- [ ] XPE_STATUS_NO_EVENT = 1 defined in xpe_error.h (R8-02)
- [ ] S0-A acceptance criteria all met

### 3.2 Before S1-A (xpe_preprocess.dll)

- [ ] **SPEC-XPE-P1A** EARS document created (~45 requirements)
- [ ] Ghost Tier 2/3 phase decision documented (R6-02)
- [ ] ALG-SPEC-001 v3.1 published (SWU-2.0 -> SWU-2.10 fix) (R6-03)
- [ ] Coverage target 90% confirmed for core preprocess (R6-07)
- [ ] Binning-Gain map interaction documented (R8-03)
- [ ] XPE-SDD-001 v1.1 published (6 missing SWU documented)
- [ ] S0-B acceptance criteria all met

### 3.3 Before S1-B1 (xpe_enhance_basic.dll)

- [ ] **SPEC-XPE-P1B-ENH** EARS document created (~28 requirements)
- [ ] EI/DI IEC 62494-1 compliance requirements formalized
- [ ] NLM performance mitigation documented (AVX2 SIMD plan) (R8-04)
- [ ] S1-A acceptance criteria all met

### 3.4 Before S1-B2 (xpe_display.dll)

- [ ] **SPEC-XPE-P1B-DISP** EARS document created (~22 requirements)
- [ ] DICOM PS3.3/PS3.14 GSDF compliance requirements formalized
- [ ] S1-A acceptance criteria all met

### 3.5 Before S1-B3 (xpe_dicom.dll)

- [ ] **SPEC-XPE-P1B-DICOM** EARS document created (~22 requirements)
- [ ] DICOM network timeout handling specified
- [ ] S1-A acceptance criteria all met

### 3.6 Before S2-A (xpe_enhance_advanced.dll)

- [ ] **SPEC-XPE-P2-ADV** EARS document created (~18 requirements)
- [ ] S1-B1 acceptance criteria all met

### 3.7 Before S2-B (gsvg.dll)

- [ ] **SPEC-XPE-P2-GSVG** EARS document created (~22 requirements)
- [ ] FFTW3 dynamic linking isolation verified
- [ ] S1-A acceptance criteria all met

### 3.8 Before S3 (xpe_ai.dll)

- [ ] **SPEC-XPE-P3-AI** EARS document created (~18 requirements)
- [ ] MVP scope clarified (S3-01 + S3-02 Must, others deferred) (R6-06)
- [ ] Defect ML DLL ownership resolved (R6-05)
- [ ] S2-A + S1-B4 acceptance criteria all met

---

## 4. Sprint S0: Foundation (Phase 0) -- DETAILED

### S0-A: Build Infrastructure (8 tasks)

| Task ID | Deliverable | Priority | Status | Sessions |
|---------|------------|:--------:|:------:|:--------:|
| S0A-01 | CMake root + modules/ CMakeLists.txt | Must | DONE | -- |
| S0A-02 | CMakePresets.json (4 presets) | Must | DONE | -- |
| S0A-03 | vcpkg.json SOUP manifest | Must | DONE | -- |
| S0A-04 | cmake/ helpers (Warnings, Platform, Dependencies) | Must | DONE | -- |
| S0A-05 | Google Test + CTest integration | Must | TODO | 1 |
| S0A-06 | Module directory scaffolding (8 modules) | Must | TODO | 1 |
| S0A-07 | CI pipeline (CMake + CTest + coverage) | Should | TODO | 1 |
| S0A-08 | Coverage reporting (gcov/lcov) | Should | TODO | 1 |

**Acceptance**: cmake --preset release succeeds, 8 module dirs exist, CTest works

### S0-B: xpe_common.dll (7 tasks)

| Task ID | Deliverable | SWU | Priority | Status | Sessions |
|---------|------------|-----|:--------:|:------:|:--------:|
| S0B-01 | Complete xpe_common_api.h (15 declarations) | -- | Must | PARTIAL | 1 |
| S0B-02 | Logging subsystem (set_level/set_file/flush) | SWU-5.4 | Must | TODO | 1 |
| S0B-04 | Memory pool hardening (alloc/free/copy) | SWU-5.1 | Must | PARTIAL | 1 |
| S0B-05 | Error handler + Alert queue | SWU-5.3 | Must | PARTIAL | 1 |
| S0B-06 | Config manager (init/shutdown/configure) | SWU-5.6 | Must | PARTIAL | 1 |
| S0B-07 | Unit tests (>= 85% coverage, >= 45 test cases) | -- | Must | TODO | 2 |
| S0B-08 | P/Invoke compatibility test | -- | Must | TODO | 1 |

**v2.0 수정**: API count 15.

**Acceptance**:
- [ ] dumpbin /exports: exactly 15 functions
- [ ] >= 85% statement coverage (>= 45 test cases)
- [ ] P/Invoke round-trip test passes
- [ ] Logging: file output + level filtering verified
- [ ] No memory leaks (ASan clean, 1000-cycle test)

### S0-C: C# GUI Scaffolding (4 tasks)

| Task ID | Deliverable | SWU | Priority | Sessions |
|---------|------------|-----|:--------:|:--------:|
| S0C-01 | ImageProcTest WPF project (.csproj) | SWU-5.7 | Must | 1 |
| S0C-02 | P/Invoke wrapper (18 functions) | SWU-5.7 | Must | 1 |
| S0C-03 | Main window + DLL load test | SWU-5.7 | Must | 1 |
| S0C-04 | PipelineOrchestrator skeleton | SWU-5.7 | Should | 1 |

**Acceptance**: ImageProcTest.exe builds, DLL loads, xpe_version() displays

---

## 5. Sprint S1-A: Pre-Processing (Phase 1a) -- DETAILED

### Prerequisite: SPEC-XPE-P1A EARS Document

SPEC-XPE-P1A must be created with ~45 EARS requirements covering:
- 9 algorithm correctness requirements (1 per SWU)
- 9 pipeline ordering requirements
- 6 calibration lifecycle requirements
- 6 performance budget requirements
- 5 error handling requirements
- 4 memory management requirements
- 4 cross-cutting requirements (thread safety, logging, flags, P/Invoke)
- 2+ boundary condition requirements

### S1-A Tasks (11 tasks)

| Task ID | Deliverable | SWU | Research ID | Priority | Sessions |
|---------|------------|-----|-------------|:--------:|:--------:|
| S1A-01 | Offset Correction | SWU-1.1 | PRE-02 | Must | 1 |
| S1A-02 | Gain Correction (+ uint16->float32) | SWU-1.2 | PRE-03 | Must | 1-2 |
| S1A-03 | Defect Pixel Correction (edge-aware) | SWU-1.3 | PRE-06 | Must | 1-2 |
| S1A-04 | Ghost/Lag Correction Tier 1 (LTI) | SWU-1.4 | PRE-04 | Must | 2 |
| S1A-05 | Calibration Manager (load/save/validate/expiry) | SWU-1.5 | SUP-01 | Must | 2 |
| S1A-06 | Temperature Compensator | SWU-1.6 | PRE-07 | Must | 1 |
| S1A-07 | Nonlinearity Corrector | SWU-1.7 | PRE-08 | Must | 1 |
| S1A-08 | Binning Corrector (conditional) | SWU-1.8 | PRE-09 | Should | 1 |
| S1A-09 | Readout Artifact Validator | SWU-1.9 | PRE-01 | Must | 1 |
| S1A-10 | Ghost Tier 2 (exposure-weighted) | SWU-1.4 | PRE-04 | **TBD** | 1 |
| S1A-11 | Ghost Tier 3 (NLCSC) | SWU-1.4 | PRE-04 | **TBD** | 2 |

**Note**: S1A-10/11 phase assignment awaiting Tech Lead decision (R6-02).

**Acceptance**:
- [ ] 18 API functions exported
- [ ] >= 90% statement coverage, >= 80% branch coverage
- [ ] Ghost Tier 1 <= 150ms (3072x3072)
- [ ] Pre-processing total <= 500ms
- [ ] Pipeline order: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost
- [ ] Calibration CRC verification working
- [ ] P/Invoke integration test passed
- [ ] All SPEC-XPE-P1A EARS requirements met

---

## 6-8. Sprints S1-B, S2, S3

Phase 1b, 2, 3의 상세 스프린트 내용은 Sprint Roadmap v1.0.0과 동일하되, 다음 변경사항 적용:

1. **EARS SPEC gating**: 각 스프린트 시작 전 해당 EARS SPEC 문서가 필수
2. **Coverage harmonization**: Phase 1a=90%, Phase 1b/2=85%, Phase 3=80%
3. **Pre-sprint checklist**: Section 3의 체크리스트 충족 필수
4. **API total**: 15 functions
5. **P2-ADV API**: 3 (not 4)

---

## 9. EARS SPEC Production Schedule

| SPEC Document | Target Sprint | EARS Count | Production Sprint |
|--------------|:------------:|:----------:|:-----------------:|
| SPEC-XPE-P0 (existing) | S0-A/B/C | 33 | Done |
| **SPEC-XPE-P1A** | S1-A | ~45 | **Before S1-A** |
| **SPEC-XPE-P1B-ENH** | S1-B1 | ~28 | **Before S1-B1** |
| **SPEC-XPE-P1B-DISP** | S1-B2 | ~22 | **Before S1-B2** |
| **SPEC-XPE-P1B-DICOM** | S1-B3 | ~22 | **Before S1-B3** |
| **SPEC-XPE-P1B-GUI** | S1-B4 | ~12 | **Before S1-B4** |
| **SPEC-XPE-P2-ADV** | S2-A | ~18 | **Before S2-A** |
| **SPEC-XPE-P2-GSVG** | S2-B | ~22 | **Before S2-B** |
| **SPEC-XPE-P3-AI** | S3 | ~18 | **Before S3** |
| **SPEC-XPE-COMMON-CROSS** | All | ~60 | **Before S1-A** |

각 SPEC 문서는 `/moai plan` 워크플로우로 생성. EARS 형식, acceptance criteria, test plan 포함.

---

## 10. Cross-Validation Resolutions (v2.0 Updated)

### Resolved in v2.0

| ID | Issue | Resolution |
|----|-------|-----------|
| R8-02 | XPE_ERR_NO_DATA undefined | Add XPE_STATUS_NO_EVENT = 1 (positive, non-error) |
| R7-SYS | ~217 EARS missing | EARS SPEC production schedule added (Section 9) |
| R6-07 | Coverage discrepancy | Harmonized: Phase 1a=90%, 1b/2=85%, 3=80% |

### Still Open

| ID | Issue | Owner | Sprint Impact |
|----|-------|-------|:------------:|
| R6-01 | api-spec.md v1.2.0 publication | Tech Lead | Blocks S0-B |
| R6-02 | Ghost Tier 2/3 phase decision | Tech Lead | S1-A scope |
| R6-03 | ALG-SPEC SWU-2.0 naming fix | Dev | S1-A clarity |
| R8-03 | Binning-Gain interaction | Dev | S1-A docs |
| R8-04 | NLM performance risk | Dev | S1-B1 mitigation |
| R6-04/05/06 | S3 scope/ownership | Tech Lead | S3 |

---

## 11. Completion Score Tracking (SCORE-PLAN-001 v2.0)

**Reference**: `.moai/specs/SPEC-XPE-MASTER/score-improvement-plan-85.md`
**Cross-reference**: `docs/project/XPE-Implementation-Analysis-Report.md` (Framework B)

### 11.1 Dual-Framework Baseline (2026-04-15)

| Framework | Basis | Current | At Phase 1b | Target |
|-----------|-------|:-------:|:-----------:|:------:|
| **A** (Process/Compliance) | EARS, IEC 62304, Cross-Validation | **61** | ~76 | **85** |
| **B** (Product/Delivery) | Functional scope, benchmark evidence | ~50 | **66** | **85** |

Framework A 영역별 현황:

| Area | Score | Max |
|------|:-----:|:---:|
| Requirements Completeness | 13 | 25 |
| Documentation Quality | 17 | 20 |
| Architecture Design | 17 | 20 |
| Implementation Progress | 6 | 20 |
| Quality Assurance | 8 | 15 |
| **TOTAL** | **61** | **100** |

### 11.2 Sprint-to-Score Mapping (Codex + Cross-Validation 통합)

| Sprint / Action | Framework A | Framework B | Cumulative (A) |
|----------------|:-----------:|:-----------:|:--------------:|
| 블로커 해소 (AED removal) | +1 (now scope-clearer) | +0 | 63 |
| S0-A 완료 (GTest + 스캐폴딩 + CI + Coverage) | +5 | +1 | 68 |
| S0-B 완료 (xpe_common 18/18 + 45 tests) | +5 | +1 | 73 |
| S0-C 완료 (WPF skeleton + P/Invoke) | +2 | +0.5 | 75 |
| SPEC-P1A + S1-A 구현 (xpe_preprocess 9 SWU) | +4 | +5 | 79 |
| S1-B 완료 (enhance_basic + display + dicom) | +3 | +8 | 82 → **66 (Fw B)** |
| Benchmark freeze BP-01~10 + SIMD parity | +2 | +5 | 84 → 71 (Fw B) |
| IEC sync (SRS/SDD/RTM/VVP) | +2 | +3 | 86 → 74 (Fw B) |
| Baseline collimation + ROI-aware EI | +1 | +3 | 87 → 77 (Fw B) |
| Reject-analysis + DI drift telemetry | +1 | +2 | **88 → 79** |
| **(Fw B 85 도달: collimation+EI+reject 완료)** | | **+8** | | **85 (Fw B)** |

### 11.3 85점 달성 핵심 원칙

`docs/project/XPE-Module-Reinforcement-Plan.md §6.3` 발췌:

- benchmark freeze 없는 AI 조기 투입 → 비효율 (0점 또는 음수)
- parity harness 없는 optimized 커널 → 침묵 회귀 위험
- reference 없는 premium path → correctness 증명 불가
- display-side 개선으로 detector-side 회귀를 숨기는 것 → 점수 하락

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | MoAI | Initial sprint roadmap |
| **2.0.0** | **2026-04-14** | **MoAI** | **8-round cross-validation integration. EARS gating. AED signature fix. Coverage harmonization. Pre-sprint checklists.** |
| **2.1.0** | **2026-04-15** | **MoAI** | **Section 11 추가: 완성도 점수 추적 (61→85 개선 계획 연동). SCORE-PLAN-001 참조.** |
| **2.2.0** | **2026-04-15** | **MoAI** | **Section 11 전면 갱신: Codex 분석 3문서(XPE-IMPL-ANALYSIS, BRAINSTORM, REINFORCE) 통합. 이중 프레임워크(A:61, B:66) + 85점 경로 정렬.** |

---

*Document End -- Sprint Execution Roadmap v2.0.0*
