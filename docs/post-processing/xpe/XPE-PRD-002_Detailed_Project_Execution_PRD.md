# PRD: X-Ray Image Processing Engine Detailed Project Execution

**Document ID**: XPE-PRD-002  
**Version**: 1.1.0  
**Date**: 2026-04-13  
**Status**: Working Draft (Cross-Verified)  
**Classification**: Internal / Execution Baseline  
**Regulatory Position**: XPE core package is currently planned as an IEC 62304 Class B package. Final software safety class and package boundaries must be confirmed by system hazard analysis and system-level decomposition. **ACTION REQUIRED**: System hazard analysis must be completed and signed before Phase 1a gate (see Section 14.1).  
**Supersedes for execution planning**: `xray-postprocessing-prd.md` (XPE-PRD-001)  
**Cross-Verification**: XPE-XVER-001 v1.0.0 (2026-04-13) — 3-round verified  

---

## 1. Executive Summary

본 문서는 X-Ray Image Processing Engine(XPE) 프로젝트를 실제로 추진하기 위한 상세 실행형 PRD다. 목적은 단순 요구사항 나열이 아니라, 다음 네 가지를 한 문서로 묶는 것이다.

1. 무엇을 만들 것인가
2. 어떤 순서로 만들 것인가
3. 각 단계가 끝났다고 판단하는 기준이 무엇인가
4. 규제 문서, 알고리즘 사양, 구현 계획을 어떻게 동기화할 것인가

현재 저장소는 문서 중심 상태이며, 구현 소스는 스캐폴딩 단계다. 따라서 본 PRD는 "이미 존재하는 제품의 개선"이 아니라 "문서에서 실행 가능한 제품 개발 프로그램으로 전환"하는 기준 문서다.

### 1.1 제품 목표

- FPD raw frame를 진단 가능한 DICOM 영상으로 변환하는 X-ray 영상처리 엔진 구축
- DLL 단위의 독립 모듈화로 스파게티 의존성 방지
- C# WPF `ImageProcTest`에서 각 DLL을 로드해 통합 테스트, 벤치마크, QA를 수행
- Must-Have 기능과 차별화 기능을 분리하여 점진적으로 출시 가능한 구조 확보

### 1.2 현재 기준선

- 제품 개요: `.moai/project/product.md`
- 실행 계획: `.moai/plans/memoized-conjuring-aurora.md`
- 파이프라인 계약: `.moai/project/pipeline-spec.md`
- C ABI: `.moai/project/api-spec.md`
- 알고리즘 DeepSync 기준: `.moai/specs/xpe-algorithm-spec-deepsync.md`
- 기술 분류: `docs/xray_fpd_tech_classification_final.md`

### 1.3 이 PRD의 산출물 정의

이 문서는 다음 산출물의 상위 기준이다.

- 구현 backlog
- phase roadmap
- module deliverable checklist
- release gate
- verification gate
- 문서 동기화 의무

세부 backlog 분해는 `XPE-PRD-003_PRD_Decomposition_and_Backlog.md`를 따른다.

---

## 2. Problem Statement

현재 프로젝트의 핵심 문제는 "알고리즘 연구는 많지만, 구현 우선순위와 실행 경계가 흔들린다"는 점이다.

### 2.1 현재 문제

- 기술 분류 문서, Moai plan, pipeline spec, 공식 XPE 패키지 문서 간 phase 정의가 일치하지 않는다.
- 문서 기준 SWU 분해는 존재하지만, 구현 우선순위와 release gate가 일관되게 정의되지 않았다.
- EI/DI, collimation, stitching, AI feature처럼 임상적으로 중요한 기능이 데이터 도메인과 runtime packaging 기준으로 고정되지 않았다.
- 규제 문서 세트와 실행 계획 문서가 분리되어 있어, 개발 진행 시 추적성이 쉽게 깨질 수 있다.

### 2.2 해결해야 하는 질문

- 어떤 기능이 출시 필수인가
- 어떤 기능이 차별화인가
- 어떤 기능이 detector-domain에서 검증되어야 하는가
- 어떤 기능이 classical path만으로 출시 가능하고, 어떤 기능이 AI worker를 필요로 하는가
- 각 phase의 종료 조건은 무엇인가

---

## 3. Product Vision and Success Criteria

### 3.1 Product Vision

XPE는 "Raw detector data에서 DICOM delivery까지 연결되는 모듈형 X-ray 영상처리 엔진"이어야 한다. 핵심은 화려한 AI보다 먼저, 규제와 성능을 버티는 deterministic baseline을 완성하는 것이다.

### 3.2 Success Criteria

프로젝트는 아래 조건을 만족할 때 성공으로 본다.

- Phase 1만으로 raw-to-DICOM 기본 경로가 독립적으로 동작한다.
- detector-domain correction과 display-domain processing이 명확히 분리된다.
- C ABI가 안정적이며 C#에서 재현 가능하게 호출된다.
- Phase 2 기능은 baseline path를 깨지 않고 점진적으로 얹힌다.
- Phase 3 AI 기능은 worker crash나 timeout 시에도 primary image delivery를 막지 않는다.
- 공식 IEC 62304 패키지와 실행 문서의 추적성이 release 전에는 반드시 일치한다.

---

## 4. Users and Stakeholders

### 4.1 Primary Users

| User | 목적 | 성공 조건 |
|------|------|-----------|
| 영상처리 엔지니어 | 알고리즘 구현, 튜닝, benchmark | 모듈 독립 구현 가능, golden dataset 기반 회귀 가능 |
| QA/RA 엔지니어 | 검증, traceability, release gate 확인 | 문서-테스트-산출물 추적 가능 |
| 시스템 통합자 | GUI/제품 소프트웨어에 DLL 연동 | ABI 안정성, 오류 처리, predictable fallback |
| 응용 GUI 개발자 | `ImageProcTest`와 상용 GUI 연동 | P/Invoke 안정성, 명확한 lifecycle API |

### 4.2 Secondary Stakeholders

| Stakeholder | 관심사 |
|-------------|--------|
| HW/FPGA 팀 | PRE-01, PRE-02/03/06/08/09의 HW offload 가능성 |
| 임상 자문 / 도메인 전문가 | 영상 품질, ALARA, body-part별 튜닝 타당성 |
| PM / Tech Lead | phase scope, 일정, risk burn-down |
| 규제 담당 | IEC 62304, FDA design control, 문서 일관성 |

---

## 5. Scope Definition

### 5.1 In Scope

#### Detector-domain Pre-Processing

- Readout validation
- Temperature compensation
- Offset correction
- Nonlinearity correction
- Gain correction
- Binning correction
- Defect correction
- Lag correction
- Gain ghosting correction

#### Enhancement and Clinical Processing

- Log transform
- Noise reduction
- Contrast enhancement
- Edge enhancement
- Baseline collimation
- ROI-aware EI refinement
- Multiscale processing
- Fractional processing
- GSVG / Virtual Grid

#### Display and I/O

- Modality LUT
- VOI LUT
- Presentation LUT / GSDF
- synchronized source-vs-processed comparison viewer
- large-image viewport interaction for 4096x4096 16-bit review
- DICOM Reader/Writer
- Presentation state support

#### AI and Premium Features

- Body-part recognition
- AI collimation refinement
- Stitching
- Bone suppression
- DL denoising

#### Project Infrastructure

- C ABI and memory ownership model
- alert polling and diagnostics
- test harness and regression dataset contract
- WPF integration host
- QA constancy test workflow

### 5.2 Out of Scope

- detector firmware/FPGA implementation 자체 개발
- acquisition control 전체 시스템
- CAD diagnosis decision support
- PACS/RIS full product workflow
- mammography, dental, tomosynthesis 전용 pipeline
- cloud inference serving

### 5.3 Constraints

- DLL 간 lateral dependency 금지
- `gsvg.dll`은 XPE 패키지와 독립 유지
- AI는 worker process 기반 선택 기능이어야 함
- detector-domain metric과 display-domain metric 혼용 금지
- source code가 아직 없는 상태를 전제로 문서-먼저 개발 진행

---

## 6. Product Principles

### 6.1 Architecture Principles

- 계산은 C/C++ DLL에 둔다.
- 제어, orchestration, viewer, QA workflow는 C#에 둔다.
- 공통 기능은 `xpe_common.dll`만 통해 공유한다.
- 모듈 사이 데이터 전달은 stable C ABI struct와 sidecar/result object로만 한다.

### 6.2 Quality Principles

- Must-Have path는 AI 없이 완성 가능해야 한다.
- fallback은 조용히 품질을 떨어뜨리는 방식이 아니라 명시적 alert를 남기는 방식이어야 한다.
- metadata flags는 상태만 표현한다. 오류 상세는 alert/result 채널로 보낸다.
- 문서와 코드의 분해 단위는 release 전에 반드시 일치시킨다.

### 6.3 Clinical Principles

- EI/DI는 detector-domain에서만 계산한다.
- GSDF는 display path에서만 적용한다.
- stitched/multi-irradiation image는 normative EI 대상으로 취급하지 않는다.
- primary image는 항상 보존하고, bone suppression 등 derived image는 별도 취급한다.

---

## 7. Runtime Packaging and Language Split

### 7.1 Language Allocation

| Layer | Language | 책임 |
|------|----------|------|
| Common / Algorithm DLLs | C/C++ | 연산, 성능 최적화, calibration, DICOM core |
| Host Orchestrator / QA / Viewer | C# WPF | DLL 로딩, 순서 제어, UI, benchmark, QA constancy |

현재 계획 기준 XPE SWU는 총 38개이며, `C/C++ 36개`, `C# 2개`다.

### 7.2 Runtime Packaging

| Package | Components | 목적 |
|---------|------------|------|
| Phase 1 required | `xpe_common.dll`, `xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` | raw-to-DICOM 기본 경로 |
| Phase 2 optional | `xpe_enhance_advanced.dll`, `gsvg.dll` | 임상 품질 향상, ROI-aware processing |
| Phase 3 optional | `xpe_ai.dll`, `xpe_ai_worker.exe` | AI 기반 premium feature |

### 7.3 Hard Decisions

- Body-part recognition은 현재 packaging 기준 Phase 3다.
- Stitching도 현재 packaging 기준 Phase 3다.
- Baseline collimation은 Phase 2 deterministic path다.
- AI collimation refinement는 Phase 3 선택 기능이다.

---

## 8. Detailed Product Requirements

### 8.1 Program-Level Requirements

| ID | Requirement | Priority | Acceptance |
|----|-------------|----------|------------|
| PRD-PROG-001 | 모든 개발 산출물은 phase별 release gate와 연결되어야 한다 | Must | phase 종료 시 checkable gate 존재 |
| PRD-PROG-002 | plan, API, pipeline, PRD, 알고리즘 사양은 동일한 phase ownership을 사용해야 한다 | Must | 문서 교차검토 시 phase mismatch 0건 |
| PRD-PROG-003 | 공식 IEC package와 실행 문서 간 traceability gap을 release 전 0으로 줄여야 한다 | Must | RTM/VVP/SDD sync 완료 |
| PRD-PROG-004 | 모든 기능은 deterministic baseline path를 먼저 정의해야 한다 | Must | AI 의존 Must-Have 없음 |
| PRD-PROG-005 | 각 phase는 code, test, docs, benchmark를 함께 종료해야 한다 | Must | phase checklist 전부 충족 |

### 8.2 Phase 0 Foundation Requirements

| ID | Requirement | Priority | Deliverable |
|----|-------------|----------|-------------|
| PRD-P0-001 | repo 내 공통 타입, 에러 코드, 메모리/alert API skeleton 생성 | Must | `xpe_common.dll` stub + header |
| PRD-P0-002 | `XpeImageBuffer`, `XpeImageMetadata` ABI packing 검증 | Must | C/C# size parity test |
| PRD-P0-003 | DLL lifecycle, memory ownership, alert polling 규칙 문서화 | Must | API spec + host sample |
| PRD-P0-004 | `ImageProcTest` 최소 host shell 작성 | Must | DLL load/unload smoke test |
| PRD-P0-005 | golden dataset, phantom dataset, regression manifest 포맷 정의 | Must | dataset manifest template |

### 8.3 Phase 1a Preprocess Requirements

| ID | Requirement | Priority | Acceptance |
|----|-------------|----------|------------|
| PRD-P1A-001 | readout validation 구현 | Must | synthetic fault injection 회귀 통과 |
| PRD-P1A-002 | temperature compensation 구현 | Must | 온도 sweep set 안정성 확보 |
| PRD-P1A-003 | offset correction 구현 | Must | dark residual 기준 충족 |
| PRD-P1A-004 | nonlinearity correction 구현 | Must | monotonicity 검증 통과 |
| PRD-P1A-005 | gain correction 구현 | Must | uniform phantom residual 기준 충족 |
| PRD-P1A-006 | binning correction 구현 | Should | binning mode regression 통과 |
| PRD-P1A-007 | defect correction 구현 | Must | isolated/cluster defect set 통과 |
| PRD-P1A-008 | lag/ghost tier baseline 구현 | Must | local ghost SRS latency/quality gate 충족 |
| PRD-P1A-009 | preprocess subset `(0.5-4)` 500ms/frame 이내 | Must | benchmark report |

### 8.4 Phase 1b Basic Post, Display, DICOM Requirements

| ID | Requirement | Priority | Acceptance |
|----|-------------|----------|------------|
| PRD-P1B-001 | log transform 구현 | Must | epsilon floor + monotonicity test |
| PRD-P1B-002 | basic noise reduction 구현 | Must | baseline quality review 통과 |
| PRD-P1B-003 | contrast enhancement 구현 | Must | CLAHE preset regression 통과 |
| PRD-P1B-004 | edge enhancement 구현 | Must | overshoot guard test 통과 |
| PRD-P1B-005 | whole-image EI baseline 구현 | Must | IEC formula/target lookup test |
| PRD-P1B-006 | display LUT stack 구현 | Must | GSDF/VOI smoke set 통과 |
| PRD-P1B-007 | DICOM write/read baseline 구현 | Must | DX IOD validation 통과 |
| PRD-P1B-008 | `ImageProcTest` viewer + W/L fast path 구현 | Must | UI interaction latency 기준 충족 |
| PRD-P1B-009 | `QaConstancyTest` baseline workflow 구현 | Must | QA run result artifact 생성 |
| PRD-P1B-010 | `ImageProcTest` source-vs-processed comparison viewport 구현 | Must | swipe/split/overlay/diff mode 검증 |
| PRD-P1B-011 | 4096x4096 16-bit RAW comparison E2E 검증 | Must | synchronized zoom/pan + evidence export 통과 |

### 8.5 Phase 2 Clinical Requirements

| ID | Requirement | Priority | Acceptance |
|----|-------------|----------|------------|
| PRD-P2-001 | deterministic baseline collimation 구현 | Must | low-confidence fallback 포함 |
| PRD-P2-002 | ROI-aware EI refinement 구현 | Must | whole-image fallback 포함 |
| PRD-P2-003 | multiscale processing 구현 | Should | artifact monitor 통과 |
| PRD-P2-004 | fractional processing 구현 | Should | texture preservation review |
| PRD-P2-005 | `gsvg.dll` 연동 구현 | Conditional | original buffer preservation fallback 확인 |
| PRD-P2-006 | clinical parameter preset 체계 수립 | Must | exam/body-part preset table |
| PRD-P2-007 | body-region validation set 기반 평가 | Must | review report 저장 |

### 8.6 Phase 3 AI Requirements

| ID | Requirement | Priority | Acceptance |
|----|-------------|----------|------------|
| PRD-P3-001 | `xpe_ai.dll` + `xpe_ai_worker.exe` sandbox 실행 | Must | launch/heartbeat/recovery test |
| PRD-P3-002 | body-part recognition 구현 | Should | locked validation set Top-1 accuracy 기준 충족 |
| PRD-P3-003 | AI collimation refinement 구현 | Could | baseline 대비 개선 지표 확보 |
| PRD-P3-004 | stitching 구현 | Conditional | overlap/non-overlap set 통과 |
| PRD-P3-005 | bone suppression 구현 | Could | derived image only, task gain report 확보 |
| PRD-P3-006 | DL denoiser 구현 | Could | classical fallback 유지 |
| PRD-P3-007 | model provenance, config immutability, dataset separation 문서화 | Must | model card + eval report |

### 8.7 Documentation and Compliance Requirements

| ID | Requirement | Priority | Acceptance |
|----|-------------|----------|------------|
| PRD-DOC-001 | phase 종료 시 plan/API/pipeline/PRD 동기화 | Must | cross-review checklist 완료 |
| PRD-DOC-002 | source 변경 전후 공식 IEC package 영향 분석 | Must | impact note 작성 |
| PRD-DOC-003 | formal XPE package는 38 SWU 기준으로 개정 | Must | SRS/SDD/RTM/VVP sync |
| PRD-DOC-004 | GSVG는 독립 package 유지 | Must | dependency review 통과 |

---

## 9. Non-Functional Requirements

### 9.1 Performance

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-PERF-001 | preprocess subset `(0.5-4)` | `<= 500 ms/frame` |
| NFR-PERF-002 | Phase 1 total | `<= 3000 ms/frame` |
| NFR-PERF-003 | full pipeline with optional modules | `<= 5000 ms/frame` target |
| NFR-PERF-004 | W/L interaction latency | `<= 16 ms` |
| NFR-PERF-005 | comparison viewport interaction | synchronized zoom/pan/swipe without pipeline reprocessing |
| NFR-PERF-006 | large-image viewer comfort envelope | 4096x4096 UInt16 source + processed in one viewport |

### 9.2 Image Quality

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-IQ-001 | lag/ghost residual | local ghost SRS baseline 충족 |
| NFR-IQ-002 | flat-field residual | detector acceptance band 내 |
| NFR-IQ-003 | sharpening overshoot | halo artifact review gate 통과 |
| NFR-IQ-004 | EI/DI correctness | formula and EIT mapping test 통과 |
| NFR-IQ-005 | GSVG/Virtual Grid | body-region validation set에서 clinical value 확인 |

### 9.3 Software Quality

| ID | Requirement | Target |
|----|-------------|--------|
| NFR-SQ-001 | preprocess coverage | statement `>= 90%`, branch `>= 80%` |
| NFR-SQ-002 | Phase 1b coverage | statement `>= 85%`, branch `>= 75%` |
| NFR-SQ-003 | deterministic modules hash reproducibility | same input/config -> same output |
| NFR-SQ-004 | hot path allocation control | unbounded allocation 금지 |
| NFR-SQ-005 | null/bounds validation | public API 전 함수 |

---

## 10. Roadmap and Milestones

본 PRD는 2주 sprint 기준 상대 일정으로 관리한다.

### 10.1 Phase Plan

| Phase | Sprint | 목적 | 종료 조건 |
|------|--------|------|-----------|
| Phase 0 | S0-S1 | ABI, host shell, common infra, dataset contract | DLL skeleton + C#/C size parity + smoke test |
| Phase 1a | S2-S4 | detector-domain preprocess baseline | preprocess 500ms gate + regression set |
| Phase 1b | S5-S7 | basic enhancement, EI baseline, display, DICOM, QA | raw-to-DICOM baseline demo |
| Phase 2 | S8-S11 | collimation, ROI-aware EI, multiscale, GSVG | clinical validation report |
| Phase 3 | S12-S15 | AI worker, body-part, stitching, bone suppression | AI fallback-safe demo |
| Release Hardening | S16-S18 | packaging, V&V, formal doc sync | release candidate + formal package sync |

### 10.2 Milestones

| Milestone | Description | Evidence |
|-----------|-------------|----------|
| M0 | PRD/plan/spec freeze | approved docs set |
| M1 | ABI freeze v1 | header + P/Invoke smoke |
| M2 | Preprocess baseline | benchmark + golden result |
| M3 | Phase 1 user demo | raw -> viewer -> DICOM export |
| M4 | Clinical feature pack | collimation/EI/GSVG review report |
| M5 | AI pack | worker lifecycle + AI eval report |
| M6 | Release candidate | formal verification pack |

---

## 11. Workstreams and Deliverables

### 11.1 Workstream Breakdown

| Workstream | 주요 책임 | 대표 산출물 |
|------------|-----------|-------------|
| WS-01 Common/ABI | 타입, 메모리, 오류, alert, config | header, common DLL, ABI tests |
| WS-02 Preprocess | detector-domain correction | preprocess DLL, phantom regression |
| WS-03 Basic Enhance/Display | log/noise/contrast/edge/LUT | enhance basic DLL, display DLL |
| WS-04 DICOM | IO, DX IOD, GSPS | dicom DLL, validation report |
| WS-05 Clinical Advanced | collimation, EI refinement, GSVG | advanced DLL, clinical review set |
| WS-06 AI | worker, models, inference path | ai DLL, worker, eval pack |
| WS-07 Host GUI | orchestrator, comparison viewer, benchmark, QA | `ImageProcTest` |
| WS-08 QA/RA | traceability, verification, release gate | RTM/VVP sync, gate checklist |

### 11.2 Mandatory Deliverables Per Phase

| Deliverable | P0 | P1a | P1b | P2 | P3 | RH |
|-------------|:--:|:---:|:---:|:--:|:--:|:--:|
| code skeleton | Y | Y | Y | Y | Y | Y |
| unit tests | Y | Y | Y | Y | Y | Y |
| benchmark report | - | Y | Y | Y | Y | Y |
| dataset manifest | Y | Y | Y | Y | Y | Y |
| docs sync | Y | Y | Y | Y | Y | Y |
| user demo | - | - | Y | Y | Y | Y |
| formal verification artifact | - | - | - | partial | partial | Y |

---

## 12. Definition of Done

### 12.1 Module DoD

한 모듈은 아래 조건을 모두 만족해야 완료로 본다.

1. public API와 config schema가 문서화되어 있다.
2. golden dataset 또는 synthetic dataset 기준 회귀 테스트가 있다.
3. 음수 입력, null pointer, format mismatch, oversized image 등 negative path test가 있다.
4. benchmark 결과가 기록되어 있다.
5. alert/fallback 동작이 정의되어 있다.
6. plan, pipeline, API 문서와 이름/phase가 일치한다.

### 12.2 Phase DoD

한 phase는 아래 조건을 모두 만족해야 종료된다.

1. 해당 phase의 Must 기능이 구현되었다.
2. 성능 게이트를 통과했다.
3. 문서 동기화 review에서 critical mismatch가 0건이다.
4. `ImageProcTest`에서 end-to-end 데모가 가능하다.
5. 차기 phase가 의존할 API/ABI가 안정화되었다.

---

## 13. Dependencies

### 13.1 Technical Dependencies

- detector raw sample and calibration data
- phantom image set
- body-region clinical evaluation set
- ONNX Runtime and model packaging
- DICOM validation toolchain
- display GSDF calibration reference

### 13.2 Organizational Dependencies

- HW/FPGA 팀의 PRE-01 및 calibration interface 정의
- 임상/도메인 검토자 확보
- QA/RA의 phase gate 참여
- dataset 사용 승인 및 관리 체계

---

## 14. Risks and Mitigations

### 14.1 Cross-Verification Identified Risks (XPE-XVER-001)

| ID | Risk | Severity | Mitigation | Gate |
|----|------|----------|------------|------|
| XVER-C1 | Safety Class C->B transition without signed hazard analysis | **CRITICAL** | System hazard analysis 수행 및 서명. SRS 안전 등급 갱신 | Phase 0 |
| XVER-C2 | Ghost correction Tier 2/3 latency exceeds pipeline stage budget | **CRITICAL** | Pipeline-spec v1.2.0에서 stage (4) Tier별 세부 예산 명시. Tier 3는 pre-processing 500ms 재분배 | Phase 1a |
| XVER-H1 | Body-Part/Stitching phase assignment conflict (PRD-001 Phase 2 vs PRD-002 Phase 3) | HIGH | PRD-001에 deprecation notice 추가. **Phase 3이 normative** | Phase 0 |
| XVER-H2 | IEC 62304 formal package not synced with PRD-002/DeepSync | HIGH | Phase gate 전 SRS/SDD/RTM/VVP 동기화 | Each gate |
| XVER-H3 | GSVG IEC 62304 package missing 6 documents (SCM/SRM/SMP/SPR/SRP/Compliance) | HIGH | Phase 2 gate 전 작성 완료 | Phase 2 |
| XVER-H4 | RTM orphan: SRS-PERF-005, SRS-PERF-006 unmapped | HIGH | RTM에 즉시 추가 | Phase 0 |
| XVER-M1 | PRD-001 missing DeepSync reference | MEDIUM | PRD-001에 cross-reference 추가 | Phase 0 |
| XVER-M2 | Test coverage target definition inconsistency | MEDIUM | Statement/Branch 분리 정의를 프로젝트 표준으로 채택 (Section 9.3 참조) | Phase 0 |

### 14.2 Original Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| 공식 XPE 패키지와 실행 문서 불일치 | release blocker | phase 종료마다 문서 sync review 의무화 |
| calibration/dataset 확보 지연 | preprocess 검증 지연 | synthetic set + placeholder manifest 먼저 생성 |
| AI 기능 scope creep | 일정 지연, baseline 흔들림 | AI는 Phase 3 optional로 고정 |
| GSVG licensing / packaging 문제 | 배포 리스크 | 독립 패키지 유지, dynamic dependency 검토 |
| stitching의 product-line 불명확 | phase 혼선 | conditional feature로 유지, 필요 시 deterministic 재패키징 |
| EI/DI 정의 혼선 | 규제/품질 리스크 | detector-domain only rule 고정 |
| C/C# ABI mismatch | integration blocker | pack/size CI and smoke tests |

---

## 15. Document Synchronization Rules

이 PRD를 기준으로 아래 문서는 함께 유지되어야 한다.

| Document | Sync Rule |
|----------|-----------|
| `.moai/plans/memoized-conjuring-aurora.md` | phase, SWU ownership, deliverable 범위 동일 |
| `.moai/project/pipeline-spec.md` | runtime stage ownership 동일 |
| `.moai/project/api-spec.md` | ABI와 data-domain 규칙 동일 |
| `.moai/specs/xpe-algorithm-spec-deepsync.md` | algorithm gate, EI/DI, fallback 규칙 동일 |
| `docs/xray_fpd_tech_classification_final.md` | product strategy phase와 runtime packaging phase를 구분 표기 |
| `XPE-SRS/SDD/RTM/VVP` | release 전까지 execution baseline과 traceability 일치 |

---

## 16. Approval Gates

| Gate | Reviewer | 통과 조건 |
|------|----------|-----------|
| G1 Architecture | Tech Lead | DLL boundary, ABI, phase ownership 승인 |
| G2 Algorithm | Algorithm Lead | detector-domain / enhancement-domain 기준 승인 |
| G3 QA/RA | QA/RA Lead | traceability and verification approach 승인 |
| G4 Integration | GUI/System Lead | host integration path 승인 |
| G5 Release | Product/QA/RA | phase deliverables and formal package sync 승인 |

---

## 17. Immediate Next Actions

이 PRD 승인 직후 바로 해야 할 일은 아래 순서다.

### 17.1 Cross-Verification Resolution (XVER-001 대응, 최우선)

1. **[CRITICAL]** System hazard analysis 수행 및 서명 → Class B 공식 확정 (XVER-C1)
2. **[CRITICAL]** Pipeline-spec v1.2.0 작성: stage (4) Tier별 세부 예산 명시 (XVER-C2)
3. **[HIGH]** PRD-001(xray-postprocessing-prd.md)에 deprecation notice 추가 (XVER-H1, M1, M3)
4. **[HIGH]** XPE-RTM-001에 SRS-PERF-005/006 매핑 추가 (XVER-H4)
5. **[MEDIUM]** quality.yaml에 Statement/Branch 분리 커버리지 기준 반영 (XVER-M2)

### 17.2 Phase 0 Execution (원래 계획)

1. Phase 0 backlog를 issue 단위로 분해 (XPE-PRD-003 기준)
2. ABI header와 C# marshaling smoke test 작성
3. preprocess golden dataset manifest 작성
4. `ImageProcTest` 최소 shell 및 DLL loader 작성
5. 공식 XPE package 개정 범위와 우선순위 문서화

---

## 18. Appendix: Release Decision Policy

### Release Candidate 진입 조건

- Phase 1 required path가 제품 수준 데모 가능
- Phase 2 optional 기능은 absent 상태에서도 시스템이 안정 동작
- Phase 3 optional 기능은 crash/timeout 시 fail-closed
- critical document mismatch 0건

### 출시 보류 조건

- EI/DI, GSDF, DICOM, alert/fallback 중 하나라도 정의와 구현이 다름
- 공식 XPE package와 plan/spec 간 traceability gap 존재
- preprocess 성능 게이트 미달
- C#/C ABI mismatch 존재
