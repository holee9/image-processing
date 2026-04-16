# PRD Decomposition and Delivery Backlog

**Document ID**: XPE-PRD-003  
**Version**: 1.0.0  
**Date**: 2026-04-13  
**Status**: Working Draft  
**Parent Document**: `XPE-PRD-002_Detailed_Project_Execution_PRD.md`  
**Purpose**: 실행형 PRD를 issue/backlog/sprint 단위로 세분화한 delivery baseline

---

## 1. Purpose

본 문서는 `XPE-PRD-002`를 실제 실행 가능한 backlog로 분해한 하위 문서다. 목적은 다음과 같다.

1. PRD 요구를 `Epic -> Feature -> Story/Backlog Item`으로 분해
2. 각 항목의 선행 의존성과 산출물을 명확화
3. Sprint 단위 우선순위와 critical path 정의
4. 구현, 검증, 문서 동기화를 같은 backlog 시스템으로 관리

이 문서는 일정표가 아니라 "무엇을 어떤 순서로 issue화할지"를 결정하는 기준 문서다.

---

## 2. Decomposition Model

### 2.1 Hierarchy

| Level | Prefix | Meaning |
|------|--------|---------|
| Epic | `EP-xx` | 하나의 workstream 또는 릴리스 단위 |
| Feature | `FT-xx.yy` | 하나의 모듈/기능 묶음 |
| Backlog Item | `BI-xx.yy.zz` | 실제 구현/문서/검증 단위 |

### 2.2 Recommended Issue Split

- `Epic`: milestone 또는 project board column에 대응
- `Feature`: GitHub issue 1개 또는 issue 묶음
- `Backlog Item`: 실제 작업 task 또는 sub-issue

### 2.3 Status Model

| Status | Meaning |
|--------|---------|
| `Ready` | 선행 의존성 해결, 즉시 착수 가능 |
| `Blocked` | 상위 산출물 또는 타 팀 입력 필요 |
| `In Progress` | 구현/문서/검증 중 |
| `Review` | 코드/문서/benchmark/traceability 리뷰 중 |
| `Done` | 정의된 evidence까지 확보 완료 |

### 2.4 Evidence Rule

모든 backlog item은 아래 중 최소 1개 이상을 evidence로 남겨야 한다.

- code or header
- test file
- benchmark report
- review report
- dataset manifest
- document sync note
- screenshot or demo artifact

---

## 3. Epic Map

| Epic | Name | PRD Reference | Primary Owner | Target Phase |
|------|------|---------------|---------------|--------------|
| `EP-00` | Program Control and Doc Sync | `PRD-PROG-*`, `PRD-DOC-*` | PM / QA-RA / Tech Lead | 전 phase |
| `EP-01` | Common ABI and Runtime Core | `PRD-P0-001~003` | C/C++ Core | Phase 0 |
| `EP-02` | Host Shell and Tooling | `PRD-P0-004`, `PRD-P1B-008`, `PRD-P1B-009` | C# Host | Phase 0-1b |
| `EP-03` | Dataset and Verification Infrastructure | `PRD-P0-005`, `PRD-PROG-005` | QA / Algorithm | Phase 0-RH |
| `EP-04` | Detector-Domain Preprocess | `PRD-P1A-*` | Algorithm Core | Phase 1a |
| `EP-05` | Basic Post, Display, DICOM | `PRD-P1B-*` | Algorithm + Host + DICOM | Phase 1b |
| `EP-06` | Clinical Advanced Processing | `PRD-P2-*` | Algorithm Advanced | Phase 2 |
| `EP-07` | AI Worker and Premium Features | `PRD-P3-*` | AI / Platform | Phase 3 |
| `EP-08` | Release Hardening and Formal Package Sync | `PRD-DOC-*`, Release Hardening | QA-RA / Tech Lead | RH |

---

## 4. Epic Decomposition

## 4.1 EP-00 Program Control and Doc Sync

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-00.01` | Baseline freeze | none | approved baseline note |
| `FT-00.02` | Cross-review checklist | `FT-00.01` | checklist template |
| `FT-00.03` | Formal package gap register | `FT-00.02` | gap register |
| `FT-00.04` | Phase gate review ritual | `FT-00.02` | gate agenda and sign-off template |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Status Seed |
|----|---------|---------|----------|-------------|
| `BI-00.01.01` | Freeze baseline document set and version markers | `PRD-PROG-001`, `PRD-PROG-002` | Must | Ready |
| `BI-00.02.01` | Create document cross-review checklist for PRD/plan/pipeline/API/spec | `PRD-DOC-001` | Must | Ready |
| `BI-00.03.01` | Build formal XPE package gap register against 38 SWU baseline | `PRD-DOC-003` | Must | Ready |
| `BI-00.03.02` | Add release blocker severity rules to gap register | `PRD-PROG-003` | Must | Ready |
| `BI-00.04.01` | Define phase gate review cadence and reviewer roles | `PRD-PROG-005` | Must | Ready |

---

## 4.2 EP-01 Common ABI and Runtime Core

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-01.01` | Stable headers and enums | `EP-00` baseline freeze | header files |
| `FT-01.02` | Error and alert model | `FT-01.01` | error header + smoke tests |
| `FT-01.03` | Memory ownership and allocation API | `FT-01.01` | alloc/free API + tests |
| `FT-01.04` | Config and parameter query API | `FT-01.01` | config schema |
| `FT-01.05` | ABI parity and dependency rules | `FT-01.01` | C/C# parity test + dep rule script |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-01.01.01` | Define `xpe_types.h` canonical structs and pack rules | `PRD-P0-001`, `PRD-P0-002` | Must | `BI-00.01.01` |
| `BI-01.01.02` | Define pixel format, flag, lifecycle enums | `PRD-P0-001` | Must | `BI-01.01.01` |
| `BI-01.02.01` | Define `xpe_error.h` code taxonomy | `PRD-P0-001` | Must | `BI-01.01.01` |
| `BI-01.02.02` | Define alert severity and polling contract | `PRD-P0-003` | Must | `BI-01.02.01` |
| `BI-01.02.03` | Implement alert queue skeleton in common runtime | `PRD-P0-003` | Must | `BI-01.02.02` |
| `BI-01.03.01` | Implement `xpe_alloc_image` / `xpe_free_image` skeleton | `PRD-P0-001` | Must | `BI-01.01.01` |
| `BI-01.03.02` | Add null/bounds validation to common entrypoints | `PRD-P0-003`, `NFR-SQ-005` | Must | `BI-01.03.01` |
| `BI-01.04.01` | Define JSON config schema and parameter range query format | `PRD-P0-003` | Must | `BI-01.01.01` |
| `BI-01.04.02` | Add config load/save stub and schema validation | `PRD-P0-003` | Should | `BI-01.04.01` |
| `BI-01.05.01` | Build C/C# `Marshal.SizeOf` parity test | `PRD-P0-002` | Must | `BI-01.01.01` |
| `BI-01.05.02` | Create dependency rules check script for DLL graph | `PRD-PROG-002` | Must | `BI-01.01.01` |
| `BI-01.05.03` | Create minimal common DLL smoke build target | `PRD-P0-001` | Must | `BI-01.03.01` |

---

## 4.3 EP-02 Host Shell and Tooling

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-02.01` | WPF shell bootstrap | `EP-01` ABI | app shell |
| `FT-02.02` | Native DLL loader and lifecycle manager | `FT-02.01` | loader smoke test |
| `FT-02.03` | Pipeline executor skeleton | `FT-02.02` | execution trace log |
| `FT-02.04` | Viewer and W/L interaction | `EP-05` display path | viewer demo |
| `FT-02.05` | Benchmark and QA harness | `FT-02.03` | benchmark output / QA artifact |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-02.01.01` | Create `ImageProcTest` solution and shell window | `PRD-P0-004` | Must | `BI-01.05.01` |
| `BI-02.02.01` | Implement native library load/unload manager | `PRD-P0-004` | Must | `BI-02.01.01` |
| `BI-02.02.02` | Add per-DLL availability state and graceful degradation UI | `PRD-PROG-004` | Must | `BI-02.02.01` |
| `BI-02.03.01` | Implement pipeline executor skeleton with ordered stage graph | `PRD-P0-004` | Must | `BI-02.02.01` |
| `BI-02.03.02` | Add alert polling and display console | `PRD-P0-003` | Must | `BI-01.02.03`, `BI-02.03.01` |
| `BI-02.04.01` | Add viewer canvas and basic image load/display path | `PRD-P1B-008` | Must | `BI-02.03.01` |
| `BI-02.04.02` | Add W/L fast-path interaction layer | `PRD-P1B-008`, `NFR-PERF-004` | Must | `BI-02.04.01`, `BI-05.03.02` |
| `BI-02.05.01` | Add benchmark runner UI and export format | `PRD-PROG-005` | Should | `BI-02.03.01` |
| `BI-02.05.02` | Add QA constancy test host workflow | `PRD-P1B-009` | Must | `BI-02.04.01`, `BI-03.03.01` |

---

## 4.4 EP-03 Dataset and Verification Infrastructure

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-03.01` | Dataset manifest schema | baseline freeze | manifest schema |
| `FT-03.02` | Golden dataset layout | `FT-03.01` | dataset directory contract |
| `FT-03.03` | Benchmark/report templates | `FT-03.01` | templates |
| `FT-03.04` | Verification pack tracking | `FT-03.02` | checklist |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-03.01.01` | Define dataset manifest fields and hash policy | `PRD-P0-005` | Must | `BI-00.01.01` |
| `BI-03.01.02` | Define phantom/clinical/synthetic dataset categories | `PRD-P0-005` | Must | `BI-03.01.01` |
| `BI-03.02.01` | Create golden dataset directory layout contract | `PRD-P0-005` | Must | `BI-03.01.01` |
| `BI-03.02.02` | Create defect-mask synthetic injection format | `PRD-P1A-007` | Must | `BI-03.01.02` |
| `BI-03.02.03` | Create temperature sweep dataset contract | `PRD-P1A-002` | Must | `BI-03.01.02` |
| `BI-03.02.04` | Create EI/DI validation case table | `PRD-P1B-005` | Must | `BI-03.01.02` |
| `BI-03.03.01` | Create benchmark report template | `PRD-PROG-005` | Must | `BI-03.01.01` |
| `BI-03.03.02` | Create image-quality review report template | `NFR-IQ-*` | Must | `BI-03.03.01` |
| `BI-03.04.01` | Create verification pack checklist mapped to DeepSync gates | `PRD-PROG-005` | Must | `BI-03.03.02` |

---

## 4.5 EP-04 Detector-Domain Preprocess

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-04.01` | Calibration manager and load path | `EP-01`, `EP-03` | calibration load tests |
| `FT-04.02` | Readout and temperature path | `FT-04.01` | synthetic and sweep tests |
| `FT-04.03` | Offset / nonlinearity / gain path | `FT-04.01` | phantom residual report |
| `FT-04.04` | Binning and defect path | `FT-04.03` | defect regression |
| `FT-04.05` | Lag / ghost path | `FT-04.01`, `EP-03` | lag benchmark and residual report |
| `FT-04.06` | Preprocess integration and performance | `FT-04.02~05` | 500 ms benchmark |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-04.01.01` | Implement calibration manager stub and file load contract | `PRD-P1A-008`, `SUP-01` | Must | `BI-01.04.02`, `BI-03.01.01` |
| `BI-04.01.02` | Define panel profile and mode selection structure | `PRD-P1A-004`, `PRD-P1A-006` | Must | `BI-04.01.01` |
| `BI-04.02.01` | Implement readout validator skeleton and fault classes | `PRD-P1A-001` | Must | `BI-04.01.01` |
| `BI-04.02.02` | Create synthetic fault injection tests for readout validator | `PRD-P1A-001` | Must | `BI-03.02.02`, `BI-04.02.01` |
| `BI-04.02.03` | Implement temperature compensation baseline | `PRD-P1A-002` | Must | `BI-04.01.01`, `BI-03.02.03` |
| `BI-04.02.04` | Create temperature sweep regression and stability report | `PRD-P1A-002` | Must | `BI-04.02.03` |
| `BI-04.03.01` | Implement offset correction baseline | `PRD-P1A-003` | Must | `BI-04.01.01` |
| `BI-04.03.02` | Implement nonlinearity correction baseline | `PRD-P1A-004` | Must | `BI-04.01.02` |
| `BI-04.03.03` | Implement gain correction baseline | `PRD-P1A-005` | Must | `BI-04.01.01` |
| `BI-04.03.04` | Run flat-field residual benchmark and report | `PRD-P1A-005`, `NFR-IQ-002` | Must | `BI-04.03.03` |
| `BI-04.04.01` | Implement binning correction baseline | `PRD-P1A-006` | Should | `BI-04.01.02` |
| `BI-04.04.02` | Implement defect interpolation baseline with cluster fallback | `PRD-P1A-007` | Must | `BI-03.02.02`, `BI-04.03.03` |
| `BI-04.04.03` | Build isolated/cluster defect regression set | `PRD-P1A-007` | Must | `BI-04.04.02` |
| `BI-04.05.01` | Implement Tier 1 lag correction baseline | `PRD-P1A-008` | Must | `BI-04.01.01` |
| `BI-04.05.02` | Implement Tier 2 escalation path and diagnostics | `PRD-P1A-008` | Must | `BI-04.05.01` |
| `BI-04.05.03` | Implement gain ghosting baseline | `PRD-P1A-008` | Must | `BI-04.05.02` |
| `BI-04.05.04` | Execute lag/ghost phantom benchmark and residual report | `PRD-P1A-008`, `NFR-IQ-001` | Must | `BI-04.05.03` |
| `BI-04.06.01` | Integrate preprocess stage order `(0.5-4)` in one path | `PRD-P1A-009` | Must | `BI-04.02.03`, `BI-04.03.03`, `BI-04.04.02`, `BI-04.05.03` |
| `BI-04.06.02` | Produce preprocess subset performance report (`<= 500 ms`) | `PRD-P1A-009`, `NFR-PERF-001` | Must | `BI-04.06.01` |

---

## 4.6 EP-05 Basic Post, Display, DICOM

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-05.01` | Basic enhancement chain | `EP-04` | image quality report |
| `FT-05.02` | EI baseline | `FT-05.01`, `EP-03` | EI/DI test set |
| `FT-05.03` | Display LUT stack | `FT-05.01` | GSDF/VOI smoke test |
| `FT-05.04` | DICOM baseline | `FT-05.03` | DX validation result |
| `FT-05.05` | Viewer and QA host integration | `EP-02`, `FT-05.03` | user demo |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-05.01.01` | Implement log transform baseline | `PRD-P1B-001` | Must | `BI-04.06.01` |
| `BI-05.01.02` | Implement baseline noise reducer | `PRD-P1B-002` | Must | `BI-05.01.01` |
| `BI-05.01.03` | Implement CLAHE baseline | `PRD-P1B-003` | Must | `BI-05.01.02` |
| `BI-05.01.04` | Implement edge enhancement with overshoot guard | `PRD-P1B-004` | Must | `BI-05.01.03` |
| `BI-05.01.05` | Create Phase 1b image-quality review set | `PRD-P1B-002~004` | Must | `BI-03.03.02`, `BI-05.01.04` |
| `BI-05.02.01` | Implement whole-image EI baseline API and math | `PRD-P1B-005` | Must | `BI-03.02.04`, `BI-04.06.01` |
| `BI-05.02.02` | Implement EIT exam/view lookup source | `PRD-P1B-005` | Must | `BI-05.02.01` |
| `BI-05.02.03` | Create DI formula and target mapping tests | `PRD-P1B-005`, `NFR-IQ-004` | Must | `BI-05.02.02` |
| `BI-05.03.01` | Implement Modality LUT | `PRD-P1B-006` | Must | `BI-05.01.04` |
| `BI-05.03.02` | Implement VOI LUT and fast-path entrypoint | `PRD-P1B-006`, `PRD-P1B-008` | Must | `BI-05.03.01` |
| `BI-05.03.03` | Implement Presentation LUT / GSDF baseline | `PRD-P1B-006` | Must | `BI-05.03.02` |
| `BI-05.03.04` | Run GSDF/VOI smoke set and output review | `PRD-P1B-006` | Must | `BI-05.03.03` |
| `BI-05.04.01` | Implement DICOM writer baseline | `PRD-P1B-007` | Must | `BI-05.03.03` |
| `BI-05.04.02` | Implement DICOM reader baseline | `PRD-P1B-007` | Must | `BI-05.04.01` |
| `BI-05.04.03` | Validate DX IOD output against toolchain | `PRD-P1B-007` | Must | `BI-05.04.02` |
| `BI-05.05.01` | Connect viewer to display DLL output | `PRD-P1B-008` | Must | `BI-02.04.01`, `BI-05.03.03` |
| `BI-05.05.02` | Wire W/L fast path from host to display DLL | `PRD-P1B-008`, `NFR-PERF-004` | Must | `BI-02.04.02`, `BI-05.03.02` |
| `BI-05.05.03` | Implement QA constancy baseline workflow and result artifact | `PRD-P1B-009` | Must | `BI-02.05.02`, `BI-05.04.03` |
| `BI-05.05.04` | Produce raw-to-DICOM Phase 1 demo package | `M3` | Must | `BI-05.05.03` |

---

## 4.7 EP-06 Clinical Advanced Processing

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-06.01` | Baseline collimation | `EP-05` | ROI fallback report |
| `FT-06.02` | ROI-aware EI refinement | `FT-06.01`, `FT-05.02` | EI comparison report |
| `FT-06.03` | Multiscale and fractional modules | `EP-05` | artifact review |
| `FT-06.04` | GSVG integration | `FT-06.01`, package boundary rules | integration test |
| `FT-06.05` | Clinical presets and validation | `FT-06.01~04` | clinical report |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-06.01.01` | Implement deterministic baseline collimation detector | `PRD-P2-001` | Must | `BI-05.01.04` |
| `BI-06.01.02` | Add low-confidence fallback to whole-image handling | `PRD-P2-001` | Must | `BI-06.01.01` |
| `BI-06.01.03` | Create ROI correctness review set | `PRD-P2-001` | Must | `BI-03.03.02`, `BI-06.01.02` |
| `BI-06.02.01` | Implement ROI-aware EI refinement path | `PRD-P2-002` | Must | `BI-06.01.02`, `BI-05.02.02` |
| `BI-06.02.02` | Add fallback to whole-image EI baseline | `PRD-P2-002` | Must | `BI-06.02.01` |
| `BI-06.02.03` | Create ROI EI regression cases and compare to baseline | `PRD-P2-002` | Must | `BI-06.02.02` |
| `BI-06.03.01` | Implement multiscale processor baseline | `PRD-P2-003` | Should | `BI-05.01.04` |
| `BI-06.03.02` | Implement fractional processor baseline | `PRD-P2-004` | Should | `BI-06.03.01` |
| `BI-06.03.03` | Add artifact monitor thresholds and disable path | `PRD-P2-003`, `PRD-P2-004` | Should | `BI-06.03.02` |
| `BI-06.04.01` | Define XPE to GSVG integration contract and handoff object | `PRD-P2-005`, `PRD-DOC-004` | Must | `BI-00.02.01`, `BI-05.01.04` |
| `BI-06.04.02` | Implement optional `gsvg.dll` load and fallback path | `PRD-P2-005` | Conditional | `BI-02.02.02`, `BI-06.04.01` |
| `BI-06.04.03` | Verify original-buffer preservation and alert behavior | `PRD-P2-005` | Conditional | `BI-06.04.02` |
| `BI-06.05.01` | Build exam/body-part preset table for clinical advanced modules | `PRD-P2-006` | Must | `BI-06.01.02`, `BI-06.03.03` |
| `BI-06.05.02` | Run body-region validation set and produce review report | `PRD-P2-007`, `NFR-IQ-005` | Must | `BI-06.05.01`, `BI-06.04.03` |

---

## 4.8 EP-07 AI Worker and Premium Features

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-07.01` | Worker runtime and IPC | `EP-01`, `EP-02` | worker lifecycle tests |
| `FT-07.02` | AI proxy ABI | `FT-07.01` | proxy API tests |
| `FT-07.03` | Body-part recognition | `FT-07.02`, dataset availability | eval report |
| `FT-07.04` | AI collimation refinement | `FT-07.03`, `EP-06` | improvement report |
| `FT-07.05` | Stitching | `FT-07.02`, overlap dataset | merge report |
| `FT-07.06` | Bone suppression / DL denoiser | `FT-07.02`, chest dataset | model eval |
| `FT-07.07` | Model governance and failure handling | `FT-07.01~06` | model card + recovery tests |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-07.01.01` | Define worker process lifecycle and IPC contract | `PRD-P3-001` | Must | `BI-01.02.03`, `BI-02.03.01` |
| `BI-07.01.02` | Implement worker launch, heartbeat, shutdown skeleton | `PRD-P3-001` | Must | `BI-07.01.01` |
| `BI-07.01.03` | Implement worker timeout and crash recovery tests | `PRD-P3-001` | Must | `BI-07.01.02` |
| `BI-07.02.01` | Implement `xpe_ai.dll` proxy ABI and request marshaling | `PRD-P3-001` | Must | `BI-07.01.02` |
| `BI-07.03.01` | Define body-part model IO, label set, confidence policy | `PRD-P3-002` | Should | `BI-07.02.01` |
| `BI-07.03.02` | Integrate body-part recognition into worker | `PRD-P3-002` | Should | `BI-07.03.01` |
| `BI-07.03.03` | Run locked validation set and produce accuracy report | `PRD-P3-002` | Should | `BI-07.03.02` |
| `BI-07.04.01` | Define AI collimation refinement input/output contract | `PRD-P3-003` | Could | `BI-06.01.02`, `BI-07.03.02` |
| `BI-07.04.02` | Implement refinement path with safe fallback to baseline ROI | `PRD-P3-003` | Could | `BI-07.04.01` |
| `BI-07.05.01` | Define stitching request format and overlap classification rules | `PRD-P3-004` | Conditional | `BI-07.02.01`, `BI-03.02.01` |
| `BI-07.05.02` | Implement stitching baseline and transform report output | `PRD-P3-004` | Conditional | `BI-07.05.01` |
| `BI-07.05.03` | Run overlap/non-overlap regression and runtime comparison | `PRD-P3-004` | Conditional | `BI-07.05.02` |
| `BI-07.06.01` | Define bone suppression output as derived-image only | `PRD-P3-005` | Could | `BI-07.02.01` |
| `BI-07.06.02` | Integrate bone suppression model path | `PRD-P3-005` | Could | `BI-07.06.01` |
| `BI-07.06.03` | Define DL denoiser fallback policy to classical denoiser | `PRD-P3-006` | Could | `BI-07.02.01` |
| `BI-07.06.04` | Integrate DL denoiser path | `PRD-P3-006` | Could | `BI-07.06.03` |
| `BI-07.07.01` | Write model card template and immutable config policy | `PRD-P3-007` | Must | `BI-07.01.01` |
| `BI-07.07.02` | Create dataset separation and provenance checklist | `PRD-P3-007` | Must | `BI-03.01.01`, `BI-07.07.01` |
| `BI-07.07.03` | Produce AI evaluation pack for release review | `PRD-P3-007` | Must | `BI-07.03.03`, `BI-07.05.03`, `BI-07.06.02` |

---

## 4.9 EP-08 Release Hardening and Formal Package Sync

### Feature Map

| Feature | Scope | Depends On | Done Evidence |
|---------|-------|------------|---------------|
| `FT-08.01` | Formal package restructuring | prior epics | updated package docs |
| `FT-08.02` | RTM / VVP / gap close | `FT-08.01` | sync report |
| `FT-08.03` | Release checklist and RC evidence | all prior features | release packet |

### Backlog Items

| ID | Summary | PRD Ref | Priority | Depends On |
|----|---------|---------|----------|------------|
| `BI-08.01.01` | Update XPE-SRS to reflect current 38 SWU baseline | `PRD-DOC-003` | Must | `BI-00.03.01` |
| `BI-08.01.02` | Update XPE-SDD to reflect runtime packaging and SWU mapping | `PRD-DOC-003` | Must | `BI-08.01.01` |
| `BI-08.01.03` | Update XPE-SAD if package partition changes | `PRD-DOC-002` | Must | `BI-08.01.02` |
| `BI-08.02.01` | Update RTM to match PRD/plan/API/pipeline naming | `PRD-DOC-003` | Must | `BI-08.01.02` |
| `BI-08.02.02` | Update VVP with actual verification assets and gates | `PRD-DOC-003` | Must | `BI-03.04.01`, `BI-08.02.01` |
| `BI-08.02.03` | Close gap register critical items to zero | `PRD-PROG-003` | Must | `BI-08.02.02` |
| `BI-08.03.01` | Build release candidate evidence pack | `M6` | Must | `BI-08.02.03` |
| `BI-08.03.02` | Run final phase gate and sign-off review | `PRD-PROG-005` | Must | `BI-08.03.01` |

---

## 5. Critical Path

다음 항목은 선행되지 않으면 전체 일정이 멈춘다.

1. `BI-00.01.01` baseline freeze
2. `BI-01.01.01` canonical ABI structs
3. `BI-01.05.01` C/C# ABI parity test
4. `BI-02.02.01` native DLL loader
5. `BI-03.01.01` dataset manifest schema
6. `BI-04.01.01` calibration manager load contract
7. `BI-04.06.02` preprocess 500 ms benchmark
8. `BI-05.04.03` DX IOD validation
9. `BI-06.01.02` baseline collimation fallback
10. `BI-07.01.03` worker recovery tests
11. `BI-08.02.03` gap register critical 0건

---

## 6. Sprint-Ready Breakdown

## 6.1 Sprint S0

목표: baseline freeze, ABI skeleton, host shell 착수

| Sprint Item | Backlog IDs |
|-------------|-------------|
| 문서 기준선 확정 | `BI-00.01.01`, `BI-00.02.01`, `BI-00.03.01` |
| ABI skeleton | `BI-01.01.01`, `BI-01.01.02`, `BI-01.02.01`, `BI-01.03.01` |
| C# shell | `BI-02.01.01`, `BI-02.02.01` |
| dataset 규격 | `BI-03.01.01`, `BI-03.02.01`, `BI-03.03.01` |

## 6.2 Sprint S1

목표: common runtime usable 상태, host smoke test

| Sprint Item | Backlog IDs |
|-------------|-------------|
| alert/config/runtime | `BI-01.02.02`, `BI-01.02.03`, `BI-01.04.01`, `BI-01.04.02` |
| ABI 검증 | `BI-01.05.01`, `BI-01.05.02`, `BI-01.05.03` |
| host execution skeleton | `BI-02.02.02`, `BI-02.03.01`, `BI-02.03.02` |
| verification infra | `BI-03.01.02`, `BI-03.04.01` |

## 6.3 Sprint S2

목표: preprocess 착수, calibration/load path 확보

| Sprint Item | Backlog IDs |
|-------------|-------------|
| calibration/profile | `BI-04.01.01`, `BI-04.01.02` |
| readout/temp | `BI-04.02.01`, `BI-04.02.02`, `BI-04.02.03` |
| offset/nonlinearity | `BI-04.03.01`, `BI-04.03.02` |
| dataset prep | `BI-03.02.02`, `BI-03.02.03` |

## 6.4 Sprint S3

목표: preprocess baseline complete

| Sprint Item | Backlog IDs |
|-------------|-------------|
| gain/defect/binning | `BI-04.03.03`, `BI-04.04.01`, `BI-04.04.02`, `BI-04.04.03` |
| lag/ghost | `BI-04.05.01`, `BI-04.05.02`, `BI-04.05.03` |
| 품질 증빙 | `BI-04.02.04`, `BI-04.03.04`, `BI-04.05.04` |

## 6.5 Sprint S4

목표: Phase 1a 종료

| Sprint Item | Backlog IDs |
|-------------|-------------|
| preprocess 통합 | `BI-04.06.01`, `BI-04.06.02` |
| phase gate | `BI-00.04.01` |
| package 영향분석 | `BI-08.01.01` 준비용 impact note |

---

## 7. Issue Creation Order

GitHub issue를 만든다면 아래 순서가 가장 안전하다.

1. `EP-00`, `EP-01`, `EP-03`
2. `EP-02`
3. `EP-04`
4. `EP-05`
5. `EP-06`
6. `EP-07`
7. `EP-08`

이 순서를 깨면 host, dataset, ABI가 없는 상태에서 알고리즘 구현 이슈만 먼저 늘어나서 추적성이 무너진다.

---

## 8. Ready / Done Criteria

### 8.1 Definition of Ready

Backlog item은 아래 조건을 만족해야 `Ready`다.

1. 상위 `PRD Ref`가 지정되어 있다.
2. 선행 의존성이 닫혔다.
3. 입력 dataset 또는 mock input이 정의되어 있다.
4. 산출물 종류가 정해져 있다.
5. phase ownership이 확정되어 있다.

### 8.2 Definition of Done

Backlog item은 아래 조건을 만족해야 `Done`이다.

1. 코드나 문서가 반영되었다.
2. 테스트 또는 리뷰 evidence가 있다.
3. 관련 spec/plan/API naming과 충돌하지 않는다.
4. alert/fallback behavior가 필요한 경우 명시돼 있다.
5. 다음 의존 item이 착수 가능한 상태다.

---

## 9. Immediate Next Actions

이 문서를 기준으로 바로 할 일은 아래 5개다.

1. `EP-00`과 `EP-01`의 backlog item을 issue로 먼저 생성
2. `S0`, `S1` 항목만 별도 board column으로 분리
3. `BI-03.01.01` dataset manifest schema를 우선 문서화
4. `BI-01.05.01` ABI parity test를 가장 먼저 자동화
5. `BI-04.01.01` calibration load contract에 필요한 placeholder file format 정의
