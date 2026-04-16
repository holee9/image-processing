# IEC 62304 Class B — Complete Document Package
# X-ray Post-Processing Engine (XPE)

**Package ID:** XPE-62304-PKG-001 v1.0  
**Software Safety Classification:** Class B (Non-serious injury possible)  
**Date:** 2026-04-03  
**Applicable Standard:** IEC 62304:2006+AMD1:2015  
**Companion Standards:** ISO 14971:2019, IEC 62366-1:2015, ISO 13485:2016  

---

## IEC 62304 Class B 적용 범위 (Clause Applicability Matrix)

Class B는 Class A의 모든 요구사항 + 추가 요구사항을 포함한다.

| IEC 62304 Clause | Description | Class A | **Class B** | Class C |
|-------------------|-------------|:-------:|:----------:|:-------:|
| **5.1** Software Development Planning | | | | |
| 5.1.1 | Software development plan | ✓ | **✓** | ✓ |
| 5.1.2 | Keep plan updated | ✓ | **✓** | ✓ |
| 5.1.3 | Reference to SDP or plans | ✓ | **✓** | ✓ |
| 5.1.4 | Standards, methods, tools planning | — | — | ✓ |
| 5.1.5 | Software integration & integration test planning | — | **✓** | ✓ |
| 5.1.6 | Software verification planning | ✓ | **✓** | ✓ |
| 5.1.7 | Software risk management planning | ✓ | **✓** | ✓ |
| 5.1.8 | Documentation planning | ✓ | **✓** | ✓ |
| 5.1.9 | Software CM planning | ✓ | **✓** | ✓ |
| 5.1.10 | Supporting items to be controlled | — | **✓** | ✓ |
| 5.1.11 | Software config item control before verification | — | **✓** | ✓ |
| **5.2** Software Requirements Analysis | | | | |
| 5.2.1 | Define & document SW requirements | ✓ | **✓** | ✓ |
| 5.2.2 | Content of SW requirements | ✓ | **✓** | ✓ |
| 5.2.3 | Include risk control in requirements | ✓ | **✓** | ✓ |
| 5.2.4 | Re-evaluate risk analysis | ✓ | **✓** | ✓ |
| 5.2.5 | Update requirements | ✓ | **✓** | ✓ |
| 5.2.6 | Verify SW requirements | ✓ | **✓** | ✓ |
| **5.3** Software Architecture Design | | | | |
| 5.3.1 | Transform requirements into architecture | — | **✓** | ✓ |
| 5.3.2 | Develop architecture for interfaces | — | **✓** | ✓ |
| 5.3.3 | Specify functional & performance requirements of SOUP | — | **✓** | ✓ |
| 5.3.4 | Specify system hardware & software required by SOUP | — | **✓** | ✓ |
| 5.3.5 | Identify segregation for risk control | — | **✓** | ✓ |
| 5.3.6 | Verify SW architecture | — | **✓** | ✓ |
| **5.4** Software Detailed Design | | | | |
| 5.4.1 | Subdivide into software units | — | **✓** (identify only) | ✓ |
| 5.4.2 | Develop detailed design for each unit | — | — | ✓ |
| 5.4.3 | Develop detailed design for interfaces | — | — | ✓ |
| 5.4.4 | Verify detailed design | — | — | ✓ |
| **5.5** Software Unit Implementation & Verification | | | | |
| 5.5.1 | Implement software unit | ✓ | **✓** | ✓ |
| 5.5.2 | Establish software unit verification process | — | **✓** | ✓ |
| 5.5.3 | Software unit acceptance criteria | — | **✓** | ✓ |
| 5.5.4 | Additional unit acceptance criteria | — | — | ✓ |
| 5.5.5 | Software unit verification | — | **✓** | ✓ |
| **5.6** Software Integration & Integration Testing | | | | |
| 5.6.1 | Integrate software units | — | **✓** | ✓ |
| 5.6.2 | Verify software integration | — | **✓** | ✓ |
| 5.6.3 | Integration test content | — | **✓** | ✓ |
| 5.6.4 | Regression testing | — | **✓** | ✓ |
| 5.6.5 | Integration test record contents | — | **✓** | ✓ |
| 5.6.6 | Use problem resolution process | — | **✓** | ✓ |
| 5.6.7 | Verify integration test procedures | — | **✓** | ✓ |
| **5.7** Software System Testing | | | | |
| 5.7.1 | Establish tests for SW requirements | ✓ | **✓** | ✓ |
| 5.7.2 | Use problem resolution process | ✓ | **✓** | ✓ |
| 5.7.3 | Retest after change | ✓ | **✓** | ✓ |
| 5.7.4 | Verify test procedures | ✓ | **✓** | ✓ |
| 5.7.5 | System test record content | ✓ | **✓** | ✓ |
| **5.8** Software Release | | | | |
| 5.8.1 | Ensure completeness | — | **✓** | ✓ |
| 5.8.2 | Ensure known anomalies documented | — | **✓** | ✓ |
| 5.8.3 | Evaluate known residual anomalies | — | **✓** | ✓ |
| 5.8.4 | Document version of released software | ✓ | **✓** | ✓ |
| 5.8.5 | Document how SW was created | — | **✓** | ✓ |
| 5.8.6 | Ensure repeatable activities | — | **✓** | ✓ |
| 5.8.7 | Ensure release verified | — | **✓** | ✓ |
| 5.8.8 | Archive software | — | **✓** | ✓ |
| **Clause 6** Software Maintenance | ✓ | **✓** | ✓ |
| 6.2.3 | Analyze for risk | — | **✓** | ✓ |
| **Clause 7** SW Risk Management | | | | |
| 7.1 | Identify hazardous situations | — | **✓** | ✓ |
| 7.2 | Risk control for SW | — | **✓** | ✓ |
| 7.3 | Verify risk control measures | — | **✓** | ✓ |
| 7.4 | Risk management of SOUP | ✓ | **✓** | ✓ |
| **Clause 8** CM | ✓ | **✓** | ✓ |
| **Clause 9** Problem Resolution | ✓ | **✓** | ✓ |

---

## Document Inventory (Class B Required Deliverables)

| Doc ID | Document Name | IEC 62304 Clause | Status |
|--------|--------------|-----------------|--------|
| XPE-SDP-001 | Software Development Plan | 5.1 | This package |
| XPE-SRS-001 | Software Requirements Specification | 5.2 | This package |
| XPE-SAD-001 | Software Architecture Document | 5.3 | This package |
| XPE-SDD-001 | Software Unit Identification | 5.4.1 | This package |
| XPE-VVP-001 | Software Verification & Validation Plan | 5.5-5.7 | This package |
| XPE-ITP-001 | Integration Test Plan | 5.6 | This package |
| XPE-STP-001 | System Test Plan | 5.7 | This package |
| XPE-RTM-001 | Requirements Traceability Matrix | 5.1.1c | This package |
| XPE-SRP-001 | Software Release Procedure | 5.8 | This package |
| XPE-SMP-001 | Software Maintenance Plan | 6 | This package |
| XPE-SRM-001 | Software Risk Management File | 7 | This package |
| XPE-SCM-001 | Software Configuration Management Plan | 8 | This package |
| XPE-SPR-001 | Software Problem Resolution Process | 9 | This package |
| XPE-SOUP-001 | SOUP List & Analysis | 5.3.3, 7.4 | This package |

---

# DOCUMENT 1: Software Development Plan (XPE-SDP-001)

**Clause Coverage:** 5.1.1 — 5.1.11

## 1. Purpose

본 계획은 X-ray Post-Processing Engine(XPE)의 소프트웨어 개발 생명주기를 IEC 62304:2006+AMD1:2015 Class B 요구사항에 따라 정의한다.

## 2. Scope

| Item | Description |
|------|-------------|
| Product Name | X-ray Post-Processing Engine (XPE) |
| Software System | Image processing pipeline for digital radiography |
| Safety Classification | **Class B** — Non-serious injury possible |
| Classification Rationale | SW 오류 시 영상 품질 저하로 진단 지연/오류 가능. 외부 risk control(방사선사 확인, 재촬영 protocol)이 심각한 상해를 방지. |
| Intended Use | FPD raw 이미지를 진단용 DICOM 영상으로 변환 |
| Operating Environment | Windows 11 (x86-64), embedded Linux (ARM) |

## 3. Software Development Life Cycle Model

**선택 모델:** Iterative Incremental (3-Phase)

```
Phase 1 (Foundation) → Phase 2 (Clinical) → Phase 3 (Intelligence)
각 Phase 내부: Sprint 단위 (4주) iterative 개발
```

| Activity | Phase 1 | Phase 2 | Phase 3 |
|----------|---------|---------|---------|
| Requirements Analysis | ✓ | ✓ (delta) | ✓ (delta) |
| Architecture Design | ✓ | ✓ (extension) | ✓ (extension) |
| Unit Identification | ✓ | ✓ | ✓ |
| Implementation | ✓ | ✓ | ✓ |
| Unit Verification | ✓ | ✓ | ✓ |
| Integration Testing | ✓ | ✓ | ✓ |
| System Testing | ✓ | ✓ | ✓ |
| Release | ✓ (v1.0) | ✓ (v2.0) | ✓ (v3.0) |

## 4. Software Development Planning (5.1.1 — 5.1.3)

### 4.1 Processes to be used

| Process | Reference Document |
|---------|-------------------|
| Requirements Analysis | XPE-SRS-001 |
| Architecture Design | XPE-SAD-001 |
| Unit Identification | XPE-SDD-001 |
| Verification & Validation | XPE-VVP-001 |
| Integration Testing | XPE-ITP-001 |
| System Testing | XPE-STP-001 |
| Risk Management | XPE-SRM-001 (→ ISO 14971) |
| Configuration Management | XPE-SCM-001 |
| Problem Resolution | XPE-SPR-001 |
| Release | XPE-SRP-001 |
| Maintenance | XPE-SMP-001 |

### 4.2 Deliverables per Activity

| Activity | Deliverables | Verification Method |
|----------|-------------|-------------------|
| Requirements | SRS document | Formal review (sign-off) |
| Architecture | SAD document + diagrams | Formal review |
| Unit identification | Software unit list | Review against architecture |
| Implementation | Source code, build scripts | Code review + unit test |
| Unit verification | Unit test reports | Test execution |
| Integration test | Integration test report | Test execution |
| System test | System test report | Test execution |
| Release | Release note, archive | Release checklist |

## 5. Integration & Integration Testing Planning (5.1.5)

### 5.1 Integration Strategy

**방법:** Bottom-up integration

```
Level 1: Individual algorithm modules (Offset, Gain, etc.)
Level 2: Processing stage groups (Pre-Processing, Core, Display)
Level 3: Full pipeline integration
Level 4: System integration (SW + HW detector interface)
```

### 5.2 Integration Test Scope

| Integration Level | Test Focus |
|-------------------|-----------|
| L1 → L2 | Data flow between sequential algorithms, buffer format |
| L2 → L3 | Pipeline throughput, memory management, error propagation |
| L3 → L4 | DICOM I/O, detector interface, timing constraints |

## 6. Software Verification Planning (5.1.6)

| Verification Activity | Method | Acceptance Criteria | Tools |
|----------------------|--------|-------------------|-------|
| Requirements review | Formal review | 100% requirements reviewed, sign-off | Gitea Issues |
| Architecture review | Formal review | Traceability to SRS verified | Manual |
| Code review | Peer review | Coding standard compliance, no critical issues | Gitea PR review |
| Unit testing | Automated test | ≥ 80% statement coverage, all tests pass | Google Test, gcov |
| Integration testing | Automated + manual | All interfaces verified | CTest |
| System testing | Test execution | All SRS requirements verified | Custom test harness |
| Regression testing | Automated | No previously passing tests fail | CI pipeline |

## 7. Software Risk Management Planning (5.1.7)

| Item | Description |
|------|-------------|
| Risk management standard | ISO 14971:2019 |
| Risk management file | XPE-SRM-001 |
| Hazard identification method | FMEA + FTA (software-specific) |
| Risk acceptability criteria | Per ISO 14971 Annex C (probability × severity matrix) |
| Risk control implementation | Documented in SRS as safety requirements (REQ-SAFE-xxx) |
| Residual risk | Evaluated per ISO 14971 clause 7 |

## 8. Documentation Planning (5.1.8)

| Document | Format | Location | Review Cycle |
|----------|--------|----------|-------------|
| All technical docs | Markdown → PDF (via Pandoc) | Gitea repository `/docs/` | Per Sprint |
| Source code | C++ 17 / C# | Gitea repository `/src/` | Per commit (PR) |
| Test results | JUnit XML + HTML report | CI artifacts | Per build |
| Risk management file | Markdown + Excel | Gitea `/risk/` | Per Phase |

## 9. Software Configuration Management Planning (5.1.9 — 5.1.11)

| Item | Description |
|------|-------------|
| SCM tool | Gitea (self-hosted, DS224+) |
| Branching strategy | GitFlow (main/develop/feature/release/hotfix) |
| Configuration items | Source code, test code, documents, build scripts, SOUP libraries |
| Version scheme | Semantic versioning (MAJOR.MINOR.PATCH) |
| Build reproducibility | Docker-based build environment, pinned toolchain versions |
| Baseline | Tagged release on `main` branch |
| Change control | Gitea PR + mandatory reviewer approval |

### 9.1 Supporting Items (5.1.10)

| Item | Version Control | Justification |
|------|----------------|--------------|
| Compiler (GCC/MSVC) | Pinned in Dockerfile | Reproducibility |
| CMake | Pinned version | Build consistency |
| SOUP libraries | Lock file (conan.lock / vcpkg.json) | Dependency tracking |
| Test framework | Pinned version | Test reproducibility |

### 9.2 Config Items Under Control Before Verification (5.1.11)

모든 software item은 unit/integration test 수행 전 Gitea에 commit되어야 하며, test는 committed version에 대해서만 실행한다.

---

# DOCUMENT 2: Software Requirements Specification (XPE-SRS-001)

**Clause Coverage:** 5.2.1 — 5.2.6

## 1. Purpose

XPE 소프트웨어 시스템의 기능, 성능, 인터페이스 및 안전 요구사항을 정의한다.

## 2. Requirements Content (5.2.2)

본 SRS는 다음 카테고리의 요구사항을 포함한다:

- **(a)** Functional and capability requirements
- **(b)** Inputs and outputs of the software system
- **(c)** Interfaces between software system and other systems
- **(d)** Software-driven alarms, warnings, operator messages
- **(e)** Security requirements
- **(f)** Usability engineering requirements (→ IEC 62366-1)
- **(g)** Data definition and database requirements
- **(h)** Installation and acceptance requirements
- **(i)** Operation and maintenance requirements
- **(j)** Networking requirements
- **(k)** User maintenance requirements
- **(l)** Regulatory requirements

## 3. Functional Requirements

### 3.1 Pre-Processing

| Req ID | Requirement | Category | Priority | Risk Control |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-001 | 시스템은 raw detector data에서 dark offset을 감산하여 고유 신호를 제거해야 한다 | (a) | Must | — |
| SRS-FUNC-002 | 시스템은 gain map을 적용하여 pixel 간 감도 차이를 보정해야 한다 | (a) | Must | — |
| SRS-FUNC-003 | 시스템은 bad pixel map 기반으로 불량 화소를 검출하고 인접 pixel 보간으로 보정해야 한다 | (a) | Must | SRS-SAFE-003 |
| SRS-FUNC-004 | 시스템은 이전 exposure의 잔류 신호(ghost/lag)를 multi-exponential decay model로 보정해야 한다 | (a) | Must | SRS-SAFE-004 |

### 3.2 Core Processing

| Req ID | Requirement | Category | Priority | Risk Control |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-010 | 시스템은 선형 detector response를 logarithmic domain으로 변환해야 한다 | (a) | Must | — |
| SRS-FUNC-011 | 시스템은 edge-preserving noise reduction(최소 bilateral filter)을 제공해야 한다 | (a) | Must | — |
| SRS-FUNC-012 | 시스템은 CLAHE 기반 adaptive contrast enhancement를 제공해야 한다 | (a) | Must | — |
| SRS-FUNC-013 | 시스템은 frequency-selective edge enhancement를 제공해야 한다 | (a) | Must | SRS-SAFE-005 |
| SRS-FUNC-014 | 시스템은 multiscale frequency processing(Laplacian pyramid, ≥ 8 level)을 제공해야 한다 (Phase 2) | (a) | Should | — |

### 3.3 Display Processing

| Req ID | Requirement | Category | Priority | Risk Control |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-020 | 시스템은 DICOM Modality LUT (Rescale Slope/Intercept)를 적용해야 한다 | (a) | Must | — |
| SRS-FUNC-021 | 시스템은 VOI LUT (LINEAR, LINEAR_EXACT, SIGMOID)를 지원해야 한다 | (a) | Must | SRS-SAFE-006 |
| SRS-FUNC-022 | 시스템은 DICOM PS3.14 GSDF에 따른 Presentation LUT를 적용해야 한다 | (a) | Must | SRS-SAFE-007 |
| SRS-FUNC-023 | 시스템은 Photometric Interpretation MONOCHROME1/MONOCHROME2를 올바르게 처리해야 한다 | (a) | Must | — |

### 3.4 DICOM I/O

| Req ID | Requirement | Category | Priority | Risk Control |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-030 | 시스템은 DX IOD (1.2.840.10008.5.1.4.1.1.1.1) FOR PROCESSING / FOR PRESENTATION을 읽고 쓸 수 있어야 한다 | (b)(c) | Must | — |
| SRS-FUNC-031 | 시스템은 Grayscale Softcopy Presentation State를 생성 및 적용할 수 있어야 한다 | (b)(c) | Must | — |
| SRS-FUNC-032 | 시스템은 JPEG 2000 Lossless 및 Explicit VR Little Endian transfer syntax를 지원해야 한다 | (b) | Must | — |

## 4. Input / Output Requirements (5.2.2.b)

| Req ID | Requirement |
|--------|------------|
| SRS-IO-001 | **Input:** 14-16 bit raw detector data (binary format, detector-specific protocol) |
| SRS-IO-002 | **Input:** Calibration data (offset map, gain map, bad pixel map) — binary format |
| SRS-IO-003 | **Input:** DICOM image files (DX IOD) |
| SRS-IO-004 | **Output:** Processed DICOM image (FOR PRESENTATION) — all mandatory Type 1/2 tags |
| SRS-IO-005 | **Output:** Processed DICOM image (FOR PROCESSING) — full bit-depth preservation |
| SRS-IO-006 | **Output:** DICOM Grayscale Softcopy Presentation State |

## 5. Interface Requirements (5.2.2.c)

| Req ID | Interface | Protocol | Direction |
|--------|-----------|----------|-----------|
| SRS-IF-001 | Detector Interface | USB 3.x / Ethernet (detector-specific SDK) | Input |
| SRS-IF-002 | PACS Interface | DICOM C-STORE SCU | Output |
| SRS-IF-003 | Worklist Interface | DICOM C-FIND SCU (MWL) | Input |
| SRS-IF-004 | GUI Interface | C ABI (DLL export) → C# P/Invoke | Bidirectional |
| SRS-IF-005 | CAD Plugin Interface | REST API + ONNX Runtime | Bidirectional |

## 6. Safety Requirements (5.2.3 — Risk Control Measures)

| Req ID | Safety Requirement | Hazard Reference | Control Type |
|--------|-------------------|-----------------|-------------|
| SRS-SAFE-001 | 시스템은 processing 중 원본 raw data를 보존해야 한다 (비파괴 처리) | HAZ-001 | Design |
| SRS-SAFE-002 | 시스템은 모든 processing parameter의 기본값을 body-part별 validated preset으로 설정해야 한다 | HAZ-002 | Design |
| SRS-SAFE-003 | 시스템은 bad pixel correction 실패 시 해당 영역을 시각적으로 표시하고 operator에게 경고해야 한다 | HAZ-003 | Alert (5.2.2.d) |
| SRS-SAFE-004 | 시스템은 ghost correction이 적용되었는지 여부를 DICOM tag에 기록해야 한다 | HAZ-004 | Traceability |
| SRS-SAFE-005 | 시스템은 edge enhancement gain을 body-part별 safe range로 제한해야 한다 (overshoot에 의한 artifact 방지) | HAZ-005 | Design |
| SRS-SAFE-006 | 시스템은 W/L이 유효 범위를 벗어나면 operator에게 경고해야 한다 | HAZ-006 | Alert |
| SRS-SAFE-007 | 시스템은 GSDF 미보정 display에서 진단 영상을 표시할 때 경고를 표시해야 한다 | HAZ-007 | Alert |
| SRS-SAFE-008 | 시스템은 DL 기반 처리(bone suppression 등) 결과에 "AI-processed" label을 표시해야 한다 | HAZ-008 | Alert |
| SRS-SAFE-009 | 시스템은 원본 영상과 처리 영상 간 즉시 전환 기능을 제공해야 한다 | HAZ-009 | Design |

## 7. Performance Requirements

| Req ID | Requirement | Acceptance Criteria |
|--------|------------|-------------------|
| SRS-PERF-001 | Pre-processing pipeline latency | ≤ 500ms (3072×3072, single-thread CPU) |
| SRS-PERF-002 | Full pipeline latency (Phase 1) | ≤ 3s |
| SRS-PERF-003 | VOI LUT interactive adjustment latency | ≤ 16ms (60fps) |
| SRS-PERF-004 | Peak memory usage per image | ≤ 2 GB |
| SRS-PERF-005 | DICOM file write time | ≤ 1s (uncompressed), ≤ 3s (JPEG 2000) |
| SRS-PERF-006 | Concurrent processing capacity | ≥ 2 images simultaneously |

## 8. Requirements Verification (5.2.6)

모든 요구사항은 다음 기준으로 검증한다:

- **Testable:** 각 요구사항은 pass/fail 판정 가능한 acceptance criteria를 가져야 한다
- **Traceable:** RTM(XPE-RTM-001)에서 architecture → test case까지 추적 가능
- **Unique:** 각 요구사항은 고유 ID를 가지며 중복 없음
- **Consistent:** 상호 모순 없음 (formal review에서 확인)

---

# DOCUMENT 3: Software Architecture Document (XPE-SAD-001)

**Clause Coverage:** 5.3.1 — 5.3.6

## 1. Architecture Overview

XPE는 **Pipeline Architecture** 패턴을 사용하며, 4개의 주요 Software Item으로 구성된다.

```
┌─────────────────────────────────────────────────────┐
│                XPE Software System                   │
│                                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────┐│
│  │   Pre-   │→│   Core   │→│ Display  │→│DICOM││
│  │Processing│  │Processing│  │Processing│  │ I/O ││
│  │  (SWI-1) │  │  (SWI-2) │  │  (SWI-3) │  │(SWI-4)│
│  └──────────┘  └──────────┘  └──────────┘  └─────┘│
│       ↑              ↑              ↑          ↑    │
│  ┌──────────────────────────────────────────────┐  │
│  │         Common Infrastructure (SWI-5)         │  │
│  │   Memory Pool │ Thread Pool │ Error Handler   │  │
│  └──────────────────────────────────────────────┘  │
│       ↑                                             │
│  ┌──────────────────────────────────────────────┐  │
│  │         SOUP Components (SWI-6)               │  │
│  │   OpenCV │ dcmtk │ ONNX Runtime │ spdlog     │  │
│  └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

## 2. Software Items (5.3.1)

| SW Item ID | Name | Safety Class | Description |
|-----------|------|:----------:|-------------|
| SWI-1 | Pre-Processing Module | B | Offset, gain, defect pixel, ghost correction |
| SWI-2 | Core Processing Module | B | Noise reduction, contrast/edge enhancement, MFP |
| SWI-3 | Display Processing Module | B | Modality LUT, VOI LUT, GSDF |
| SWI-4 | DICOM I/O Module | B | DICOM read/write, Presentation State |
| SWI-5 | Common Infrastructure | B | Memory management, threading, logging, error handling |
| SWI-6 | SOUP Components | — | Third-party libraries (see XPE-SOUP-001) |

## 3. Interfaces (5.3.2)

### 3.1 Inter-Module Interfaces

| Interface | From → To | Data Type | Mechanism |
|-----------|-----------|-----------|-----------|
| IF-INT-001 | SWI-1 → SWI-2 | ImageBuffer (float32, M×N) | Shared memory pointer |
| IF-INT-002 | SWI-2 → SWI-3 | ImageBuffer (float32, M×N) | Shared memory pointer |
| IF-INT-003 | SWI-3 → SWI-4 | ImageBuffer (uint16, M×N) + metadata | Shared memory + struct |
| IF-INT-004 | SWI-4 → External | DICOM file / network stream | File I/O / DICOM protocol |

### 3.2 ImageBuffer Specification

```cpp
struct ImageBuffer {
    uint32_t width;          // pixels
    uint32_t height;         // pixels
    uint32_t bitsAllocated;  // 16 or 32
    uint32_t bitsStored;     // 14, 16, or 32
    PixelFormat format;      // UINT16, FLOAT32
    void* data;              // pixel data pointer
    size_t dataSize;         // bytes
    ImageMetadata metadata;  // exposure params, body part, etc.
};
```

### 3.3 External Interfaces

| Interface | Protocol | Error Handling |
|-----------|----------|---------------|
| Detector SDK | Vendor-specific C API | Timeout + retry (3×) → error state |
| PACS (C-STORE) | DICOM v3.0 SCU | Association failure → queue + retry |
| GUI | C ABI DLL export | Exception → error code return |

## 4. SOUP Specification (5.3.3, 5.3.4)

→ See XPE-SOUP-001 for complete analysis

## 5. Segregation for Risk Control (5.3.5)

| Risk Control | Segregation Method |
|-------------|-------------------|
| Original data preservation | SWI-1은 input buffer를 read-only로 접근, 별도 output buffer에 기록 |
| Processing error isolation | 각 SWI는 독립 error domain — 한 module의 exception이 다른 module에 전파되지 않음 |
| DL processing separation | Phase 3 AI 모듈은 별도 process(sandbox)에서 실행, IPC로 결과 전달 |
| Parameter validation | 모든 processing parameter는 SWI-5 내 validator를 거쳐 safe range 내에서만 적용 |

## 6. Architecture Verification (5.3.6)

| Verification Item | Method | Criteria |
|-------------------|--------|----------|
| 모든 SRS 요구사항이 architecture에 매핑됨 | RTM review | 100% coverage |
| Interface 정의 완전성 | Formal review | 모든 data flow 정의됨 |
| SOUP 요구사항 충족 | SOUP analysis review | 모든 SOUP 적합성 확인 |
| Risk control 구현 가능성 | Design review | 모든 SRS-SAFE-xxx가 architecture에 반영 |
| 기존 system 호환성 | Review | RadiConsole™ GUI(WPF) 연동 확인 |

---

# DOCUMENT 4: Software Unit Identification (XPE-SDD-001)

**Clause Coverage:** 5.4.1 (Class B: identification only — detailed design not required)

## 1. Software Unit Decomposition

### SWI-1: Pre-Processing Module

| Unit ID | Unit Name | Function |
|---------|-----------|----------|
| SWU-1.1 | OffsetCorrector | Dark/offset subtraction with saturation arithmetic |
| SWU-1.2 | GainCorrector | Flat-field gain multiplication |
| SWU-1.3 | DefectPixelCorrector | Bad pixel detection & interpolation |
| SWU-1.4 | GhostCorrector | Multi-exponential lag correction |
| SWU-1.5 | CalibrationManager | Calibration data load/store/validation |

### SWI-2: Core Processing Module

| Unit ID | Unit Name | Function |
|---------|-----------|----------|
| SWU-2.1 | LogTransform | Logarithmic domain conversion |
| SWU-2.2 | NoiseReducer | Bilateral filter, NLM |
| SWU-2.3 | ContrastEnhancer | CLAHE implementation |
| SWU-2.4 | EdgeEnhancer | Unsharp masking, frequency-selective |
| SWU-2.5 | MultiscaleProcessor | Laplacian pyramid MFP (Phase 2) |
| SWU-2.6 | BodyPartRecognizer | CNN classifier (Phase 2) |
| SWU-2.7 | ImageStitcher | Panoramic stitching (Phase 2) |
| SWU-2.8 | BoneSuppressionEngine | DL U-Net inference (Phase 3) |

### SWI-3: Display Processing Module

| Unit ID | Unit Name | Function |
|---------|-----------|----------|
| SWU-3.1 | ModalityLUT | Rescale Slope/Intercept application |
| SWU-3.2 | VoiLUT | W/L Linear, Sigmoid, LUT Sequence |
| SWU-3.3 | PresentationLUT | GSDF, photometric interpretation handling |
| SWU-3.4 | LUTManager | Preset storage, custom LUT management |

### SWI-4: DICOM I/O Module

| Unit ID | Unit Name | Function |
|---------|-----------|----------|
| SWU-4.1 | DicomReader | DICOM file parsing, pixel data extraction |
| SWU-4.2 | DicomWriter | DICOM file creation, tag population |
| SWU-4.3 | PresentationStateIO | GSPS create/apply |
| SWU-4.4 | DicomNetworkSCU | C-STORE, C-FIND SCU |

### SWI-5: Common Infrastructure

| Unit ID | Unit Name | Function |
|---------|-----------|----------|
| SWU-5.1 | MemoryPool | Pre-allocated image buffer pool |
| SWU-5.2 | ThreadPool | Task-based parallel execution |
| SWU-5.3 | ErrorHandler | Centralized error/exception management |
| SWU-5.4 | Logger | spdlog wrapper, audit trail |
| SWU-5.5 | ParameterValidator | Safe-range enforcement for all parameters |
| SWU-5.6 | ConfigManager | System/user configuration persistence |

---

# DOCUMENT 5: Software Verification & Validation Plan (XPE-VVP-001)

**Clause Coverage:** 5.5.1 — 5.5.5, 5.6.1 — 5.6.7, 5.7.1 — 5.7.5

## 1. Unit Verification (5.5)

### 1.1 Unit Verification Process (5.5.2)

| Item | Description |
|------|-------------|
| Framework | Google Test (C++), NUnit (C#) |
| Coverage Tool | gcov + lcov (C++), dotCover (C#) |
| Execution | CI pipeline (Gitea Actions) — every commit to develop/feature |
| Review | Unit tests reviewed as part of PR review |

### 1.2 Unit Acceptance Criteria (5.5.3)

| Criterion | Target |
|-----------|--------|
| Statement coverage | ≥ 80% per software unit |
| Branch coverage | ≥ 70% per software unit |
| All tests pass | 100% (zero failures) |
| Coding standard compliance | Zero critical violations (MISRA C++ subset) |
| No memory leaks | Verified by AddressSanitizer |

### 1.3 Unit Verification Execution (5.5.5)

각 software unit(SWU-x.y)에 대해:

1. Unit test suite 작성 (test case ID: UT-{unit_id}-{seq})
2. CI에서 자동 실행
3. Coverage report 생성
4. Test report를 Gitea artifact로 보관
5. Acceptance criteria 미달 시 merge 차단

## 2. Integration Testing (5.6)

### 2.1 Integration Strategy (5.6.1)

| Phase | Integration Scope | Pre-condition |
|-------|------------------|---------------|
| I-1 | SWU-1.1 → SWU-1.4 (Pre-Processing chain) | All Phase 1 units pass unit test |
| I-2 | SWI-1 → SWI-2 (Pre → Core chain) | I-1 pass |
| I-3 | SWI-2 → SWI-3 (Core → Display chain) | I-2 pass |
| I-4 | SWI-1 → SWI-4 (Full pipeline) | I-3 pass |
| I-5 | SWI-4 ↔ External (DICOM network) | I-4 pass |

### 2.2 Integration Test Content (5.6.3)

| Test ID | Test Description | Input | Expected Output |
|---------|-----------------|-------|-----------------|
| IT-001 | Offset → Gain chain 데이터 정합성 | Known synthetic raw + calibration | Pre-calculated reference (PSNR ≥ 60dB) |
| IT-002 | Full pre-processing → core 흐름 | Phantom image (CDRAD 2.0) | Visual IQ ≥ 3.5/5 (expert review) |
| IT-003 | Pipeline → DICOM output | Full pipeline input | DICOM conformance (DVTk validation pass) |
| IT-004 | W/L interactive response | User W/L drag event | Display update ≤ 16ms |
| IT-005 | Error propagation: SWI-1 failure | Corrupted calibration data | Graceful error, no crash, alert displayed |
| IT-006 | Memory stability | 100 images sequential processing | No memory growth > 5%, no leak |

### 2.3 Regression Testing (5.6.4)

- 모든 integration test는 regression suite에 포함
- Release branch merge 전 full regression 실행 필수
- Regression failure → release 차단

### 2.4 Integration Test Records (5.6.5)

각 테스트 실행에 대해 기록:

- Test ID, date, executor
- SW version (Git commit hash)
- Test environment (OS, hardware, compiler)
- Pass/fail result
- Anomalies found (→ XPE-SPR-001 연계)

## 3. System Testing (5.7)

### 3.1 System Test Plan (5.7.1)

모든 SRS 요구사항에 대해 최소 1개 system test case를 정의한다.

| SRS Req ID | System Test ID | Test Method | Pass Criteria |
|-----------|---------------|-------------|---------------|
| SRS-FUNC-001 | ST-001 | Synthetic data + reference comparison | PSNR ≥ 60dB vs reference |
| SRS-FUNC-003 | ST-003 | Known bad pixel injection | All injected defects corrected |
| SRS-FUNC-012 | ST-012 | Clinical image set (N=50) | Reader IQ score ≥ 3.5/5 |
| SRS-FUNC-021 | ST-021 | W/L preset application | Pixel value match reference ± 1 |
| SRS-FUNC-030 | ST-030 | DICOM conformance test | DVTk full validation pass |
| SRS-SAFE-001 | ST-SAFE-001 | Process image then verify raw | Raw data byte-identical |
| SRS-SAFE-003 | ST-SAFE-003 | Force defect correction failure | Warning displayed within 2s |
| SRS-PERF-001 | ST-PERF-001 | Timing measurement (3072×3072) | ≤ 500ms |
| SRS-PERF-002 | ST-PERF-002 | End-to-end timing | ≤ 3s |

### 3.2 System Test Record Content (5.7.5)

| Field | Description |
|-------|-------------|
| Test ID | Unique identifier |
| SW Version | Release candidate version + Git tag |
| Test Environment | Full HW/SW specification |
| Test Data | Input dataset identifier |
| Results | Pass/Fail + measured values |
| Anomalies | Reference to problem report (if any) |
| Tester | Name + signature |
| Date | Execution date |

---

# DOCUMENT 6: Requirements Traceability Matrix (XPE-RTM-001)

**Clause Coverage:** 5.1.1c, 5.3.6, 7.3.3

| SRS Req ID | Architecture (SAD) | SW Unit (SDD) | Unit Test | Integration Test | System Test | Risk (SRM) |
|-----------|-------------------|--------------|-----------|-----------------|-------------|-----------|
| SRS-FUNC-001 | SWI-1 | SWU-1.1 | UT-1.1-001..005 | IT-001 | ST-001 | — |
| SRS-FUNC-002 | SWI-1 | SWU-1.2 | UT-1.2-001..004 | IT-001 | ST-002 | — |
| SRS-FUNC-003 | SWI-1 | SWU-1.3 | UT-1.3-001..008 | IT-001 | ST-003 | HAZ-003 |
| SRS-FUNC-004 | SWI-1 | SWU-1.4 | UT-1.4-001..006 | IT-001 | ST-004 | HAZ-004 |
| SRS-FUNC-010 | SWI-2 | SWU-2.1 | UT-2.1-001..003 | IT-002 | ST-010 | — |
| SRS-FUNC-011 | SWI-2 | SWU-2.2 | UT-2.2-001..005 | IT-002 | ST-011 | — |
| SRS-FUNC-012 | SWI-2 | SWU-2.3 | UT-2.3-001..006 | IT-002 | ST-012 | — |
| SRS-FUNC-013 | SWI-2 | SWU-2.4 | UT-2.4-001..004 | IT-002 | ST-013 | HAZ-005 |
| SRS-FUNC-020 | SWI-3 | SWU-3.1 | UT-3.1-001..003 | IT-003 | ST-020 | — |
| SRS-FUNC-021 | SWI-3 | SWU-3.2 | UT-3.2-001..005 | IT-003 | ST-021 | HAZ-006 |
| SRS-FUNC-022 | SWI-3 | SWU-3.3 | UT-3.3-001..004 | IT-003 | ST-022 | HAZ-007 |
| SRS-FUNC-030 | SWI-4 | SWU-4.1, SWU-4.2 | UT-4.1/4.2-001..008 | IT-003 | ST-030 | — |
| SRS-SAFE-001 | SWI-1, SWI-5 | SWU-5.1 | UT-5.1-001..003 | IT-005 | ST-SAFE-001 | HAZ-001 |
| SRS-SAFE-003 | SWI-1, SWI-5 | SWU-1.3, SWU-5.3 | UT-1.3-008, UT-5.3-001 | IT-005 | ST-SAFE-003 | HAZ-003 |
| SRS-PERF-001 | SWI-1 | All SWU-1.x | UT-PERF-001 | IT-001 | ST-PERF-001 | — |
| SRS-PERF-002 | All SWI | All SWU | — | IT-004 | ST-PERF-002 | — |

---

# DOCUMENT 7: Software Risk Management File (XPE-SRM-001)

**Clause Coverage:** 7.1 — 7.4

## 1. Hazard Identification (7.1)

| Hazard ID | Hazardous Situation | Severity | Probability | Risk Level | SW Cause |
|-----------|-------------------|----------|------------|-----------|----------|
| HAZ-001 | 원본 영상 손실로 재처리 불가 → 재촬영(추가 피폭) | Medium | Low | Medium | Processing에서 원본 overwrite |
| HAZ-002 | 부적절한 processing parameter로 진단 정보 손실 | Medium | Medium | Medium | Default preset 누락 또는 오류 |
| HAZ-003 | 보정되지 않은 bad pixel이 병변으로 오인 | Medium | Low | Medium | Bad pixel map 미갱신 |
| HAZ-004 | Ghost artifact가 실제 병변으로 오인 | Medium | Medium | Medium | Lag correction 미적용 또는 부적절 |
| HAZ-005 | 과도한 edge enhancement으로 허위 구조물 생성 | Medium | Medium | Medium | Gain parameter 과다 |
| HAZ-006 | 부적절한 W/L 설정으로 미묘한 병변 비가시 | Medium | Medium | Medium | W/L preset 오류 |
| HAZ-007 | GSDF 미준수 display에서 contrast 왜곡 | Medium | Low | Low | Display 미보정 |
| HAZ-008 | AI processing이 병변 제거/생성 | Medium | Medium | Medium | DL model artifact |
| HAZ-009 | Processing 상태 혼동(원본 vs 처리 영상) | Low | Medium | Low | UI 표시 부재 |

## 2. Risk Control Measures (7.2)

| Hazard ID | Risk Control | Implementation | SRS Req |
|-----------|-------------|---------------|---------|
| HAZ-001 | Non-destructive processing (원본 보존) | Read-only input buffer + separate output | SRS-SAFE-001 |
| HAZ-002 | Validated body-part preset 적용 | Auto-selection + safe-range validator | SRS-SAFE-002 |
| HAZ-003 | Bad pixel correction 실패 시 경고 | ErrorHandler → UI alert | SRS-SAFE-003 |
| HAZ-004 | Ghost correction 적용 여부 DICOM 기록 | Custom DICOM tag 기록 | SRS-SAFE-004 |
| HAZ-005 | Enhancement gain 범위 제한 | ParameterValidator safe-range check | SRS-SAFE-005 |
| HAZ-006 | W/L 유효 범위 경고 | Range check + UI warning | SRS-SAFE-006 |
| HAZ-007 | 미보정 display 경고 | GSDF compliance check on startup | SRS-SAFE-007 |
| HAZ-008 | AI-processed label 표시 | Overlay text + DICOM annotation | SRS-SAFE-008 |
| HAZ-009 | 원본/처리 전환 기능 | UI toggle + state indicator | SRS-SAFE-009 |

## 3. Risk Control Verification (7.3)

| Hazard ID | Verification Method | Test ID | Status |
|-----------|-------------------|---------|--------|
| HAZ-001 | Raw data byte comparison after processing | ST-SAFE-001 | Planned |
| HAZ-002 | Preset validation test (all body-parts) | ST-SAFE-002 | Planned |
| HAZ-003 | Forced defect correction failure injection | ST-SAFE-003 | Planned |
| HAZ-004 | DICOM tag presence verification | ST-004 | Planned |
| HAZ-005 | Max gain application + visual artifact check | ST-013 | Planned |
| HAZ-006 | W/L out-of-range injection | ST-SAFE-006 | Planned |
| HAZ-007 | Non-GSDF display simulation | ST-SAFE-007 | Planned |
| HAZ-008 | AI output label presence test | ST-SAFE-008 | Planned |
| HAZ-009 | Toggle function test | ST-SAFE-009 | Planned |

## 4. SOUP Risk Management (7.4)

→ See XPE-SOUP-001

---

# DOCUMENT 8: SOUP List & Analysis (XPE-SOUP-001)

**Clause Coverage:** 5.3.3, 5.3.4, 7.4.1 — 7.4.3

| SOUP ID | Name | Version | Purpose | License | Safety Class |
|---------|------|---------|---------|---------|:----------:|
| SOUP-001 | OpenCV | 4.9.x | Image processing primitives (filter, transform) | Apache 2.0 | B |
| SOUP-002 | dcmtk | 3.6.8 | DICOM read/write/network | BSD-3 | B |
| SOUP-003 | ONNX Runtime | 1.17.x | DL model inference (Phase 3) | MIT | B |
| SOUP-004 | spdlog | 1.13.x | Logging framework | MIT | A |
| SOUP-005 | nlohmann/json | 3.11.x | JSON configuration parsing | MIT | A |
| SOUP-006 | Google Test | 1.14.x | Unit testing (dev only) | BSD-3 | N/A |
| SOUP-007 | fmt | 10.x | String formatting | MIT | A |
| SOUP-008 | Eigen | 3.4.x | Matrix operations (MFP) | MPL-2.0 | B |

### SOUP Functional & Performance Requirements (5.3.3)

| SOUP ID | Functional Requirement | Performance Requirement |
|---------|----------------------|------------------------|
| SOUP-001 | cv::bilateralFilter, cv::CLAHE, pyramid ops 정상 동작 | 3072×3072 bilateral ≤ 200ms |
| SOUP-002 | DX IOD read/write, C-STORE SCU, JPEG 2000 codec | DICOM file write ≤ 1s |
| SOUP-003 | ONNX model load + inference (GPU/CPU) | Inference ≤ 2s (GPU) |
| SOUP-008 | Matrix decomposition, FFT | Laplacian pyramid 12-level ≤ 500ms |

### SOUP System Requirements (5.3.4)

| SOUP ID | OS | Hardware | Dependencies |
|---------|----|---------|----|
| SOUP-001 | Windows 11, Linux | x86-64 (AVX2), ARM (NEON) | — |
| SOUP-002 | Windows 11, Linux | — | OpenSSL (TLS) |
| SOUP-003 | Windows 11, Linux | NVIDIA GPU (CUDA 12) optional | CUDA Toolkit (optional) |
| SOUP-008 | Cross-platform | — | — |

### SOUP Risk Analysis (7.4)

| SOUP ID | Potential Failure | Impact | Mitigation |
|---------|------------------|--------|-----------|
| SOUP-001 | Filter produces incorrect output | Degraded image quality | Output validation (PSNR check vs reference) |
| SOUP-002 | DICOM tag mishandling | Non-conformant output | DVTk conformance test in CI |
| SOUP-003 | Model inference produces NaN/Inf | AI module crash or wrong output | Output range validation + fallback to non-AI pipeline |
| SOUP-008 | Numerical instability in decomposition | MFP artifact | Condition number check, fallback to simpler decomposition |

---

# DOCUMENT 9: Software Configuration Management Plan (XPE-SCM-001)

**Clause Coverage:** 8.1 — 8.3

## 1. Configuration Identification (8.1)

| Config Item Type | Naming Convention | Location |
|-----------------|-------------------|----------|
| Source code | `src/{module}/{file}.cpp/.h` | Gitea `xpe-engine` repo |
| Test code | `test/{module}/{file}_test.cpp` | Same repo `/test/` |
| Documents | `docs/{doc-id}.md` | Same repo `/docs/` |
| Build scripts | `CMakeLists.txt`, `Dockerfile` | Root |
| SOUP lockfile | `vcpkg.json` + `vcpkg-configuration.json` | Root |
| Calibration data | `cal/{panel-id}/` | Separate `xpe-calibration` repo |

## 2. Change Control (8.2)

### 2.1 Change Request Process

```
1. Issue 생성 (Gitea Issue)
   ↓
2. Impact analysis (affected SWI, test scope, risk impact)
   ↓
3. Feature branch 생성 (feature/{issue-id}-{desc})
   ↓
4. Implementation + unit test
   ↓
5. Pull Request (PR) 생성
   ↓
6. Code review (≥ 1 reviewer approval)
   ↓
7. CI pass (build + unit test + static analysis)
   ↓
8. Merge to develop
   ↓
9. Integration test (develop branch)
```

### 2.2 Traceability (8.2.4)

- 모든 change request는 Gitea Issue에 기록
- PR은 관련 Issue를 참조 (`Fixes #123`)
- Commit message는 Issue ID 포함
- Release tag는 포함된 Issue 목록 기록

## 3. Configuration Status Accounting (8.3)

| Report | Frequency | Content |
|--------|-----------|---------|
| CI Build Report | Per commit | Build status, test results, coverage |
| Release Note | Per release | Version, changes, known anomalies, SOUP versions |
| Configuration Baseline | Per release | Complete list of all config items + versions |

---

# DOCUMENT 10: Software Release Procedure (XPE-SRP-001)

**Clause Coverage:** 5.8.1 — 5.8.8

## Release Checklist

| Step | Clause | Activity | Evidence |
|------|--------|----------|----------|
| 1 | 5.8.1 | Verify all planned activities complete | RTM 100% pass verification |
| 2 | 5.8.1 | Verify SRS ↔ System Test traceability complete | XPE-RTM-001 sign-off |
| 3 | 5.8.2 | Document all known anomalies | Known Anomalies List (in release note) |
| 4 | 5.8.3 | Evaluate each residual anomaly for risk acceptability | Risk evaluation per ISO 14971 |
| 5 | 5.8.4 | Document released SW version | Git tag + version string |
| 6 | 5.8.5 | Document build environment & procedure | Dockerfile + build script |
| 7 | 5.8.6 | Verify build reproducibility | Rebuild from tag → binary diff |
| 8 | 5.8.7 | Verify release activities complete | Release checklist sign-off |
| 9 | 5.8.8 | Archive to configuration management system | Gitea tag + artifact archive |

### Release Note Template

```
═══════════════════════════════════════════
XPE Release Note
Version: x.y.z
Date: YYYY-MM-DD
Git Tag: vx.y.z
Git Commit: {full SHA}
═══════════════════════════════════════════

1. Released Software Items
   - SWI-1 Pre-Processing v{x.y}
   - SWI-2 Core Processing v{x.y}
   - ...

2. SOUP Component Versions
   - OpenCV {version}
   - dcmtk {version}
   - ...

3. Changes Since Previous Release
   - {Issue #} - {description}
   - ...

4. Known Residual Anomalies
   | ID | Description | Severity | Risk Evaluation |
   |----|-------------|----------|-----------------|

5. Build Environment
   - OS: {Ubuntu 24.04 / Windows 11}
   - Compiler: {GCC 13.2 / MSVC 17.9}
   - CMake: {3.28}
   - Docker Image: {tag}

6. Verification Summary
   - Unit Tests: {X}/{Y} pass ({Z}% coverage)
   - Integration Tests: {X}/{Y} pass
   - System Tests: {X}/{Y} pass
   - DICOM Conformance: PASS

Approved by: ____________________  Date: ________
```

---

# DOCUMENT 11: Software Maintenance Plan (XPE-SMP-001)

**Clause Coverage:** 6.1 — 6.3

## 1. Maintenance Activities

| Activity | Trigger | Process |
|----------|---------|---------|
| Corrective | Problem report (field issue) | SPR-001 → fix → regression test → release |
| Adaptive | OS/SOUP update | Impact analysis → modify → full V&V → release |
| Perfective | Feature request | SRS update → full development cycle |
| Preventive | Scheduled review | SOUP vulnerability scan (quarterly) |

## 2. Problem Report Analysis (6.2.3)

각 post-market problem report에 대해:

1. Safety impact 평가 (ISO 14971 risk matrix)
2. 영향받는 SW Item / Unit 식별
3. Regulatory reporting 필요 여부 판단
4. 수정 우선순위 결정
5. Regression test 범위 결정

## 3. Feedback → Development Cycle

Maintenance에서 발견된 문제가 design change를 요구하면, Clause 5 (Software Development Process)의 해당 단계부터 재진입한다.

---

# DOCUMENT 12: Software Problem Resolution Process (XPE-SPR-001)

**Clause Coverage:** 9.1 — 9.8

## Problem Resolution Workflow

```
1. Problem Detection
   ├─ Internal (testing, code review)
   └─ External (field report, customer feedback)
        ↓
2. Problem Report 생성 (Gitea Issue, label: "problem-report")
   - Description, reproduction steps, severity, SW version
        ↓
3. Investigation & Impact Analysis
   - Root cause identification
   - Affected SW items / requirements
   - Safety impact assessment (→ XPE-SRM-001)
        ↓
4. Disposition Decision
   ├─ Fix → Change request → Development cycle (Clause 5)
   ├─ Defer → Risk evaluation + justification + known anomaly list
   └─ No action → Justification documented
        ↓
5. Verification
   - Fix verified by unit/integration/system test
   - Regression test pass
        ↓
6. Closure
   - Problem report closed with resolution
   - Trend analysis (quarterly)
```

## Severity Classification

| Level | Definition | Response Time |
|-------|-----------|--------------|
| Critical | Safety-related, potential patient harm | 24h investigation start |
| Major | Functional failure, workaround available | 5 business days |
| Minor | Cosmetic or minor usability issue | Next planned release |

---

# DOCUMENT 13: IEC 62304 Compliance Matrix (Summary)

| Clause | Title | Class B Required | Document Reference | Status |
|--------|-------|:---------------:|-------------------|--------|
| 5.1 | Development Planning | ✓ | XPE-SDP-001 | ✓ |
| 5.2 | Requirements Analysis | ✓ | XPE-SRS-001 | ✓ |
| 5.3 | Architecture Design | ✓ | XPE-SAD-001 | ✓ |
| 5.4.1 | Unit Identification | ✓ | XPE-SDD-001 | ✓ |
| 5.4.2-4 | Detailed Design | — (Class C only) | N/A | N/A |
| 5.5 | Unit Implementation & Verification | ✓ | XPE-VVP-001 | ✓ |
| 5.6 | Integration Testing | ✓ | XPE-VVP-001 (§2) | ✓ |
| 5.7 | System Testing | ✓ | XPE-VVP-001 (§3) | ✓ |
| 5.8 | Software Release | ✓ | XPE-SRP-001 | ✓ |
| 6 | Maintenance | ✓ | XPE-SMP-001 | ✓ |
| 7 | Risk Management | ✓ | XPE-SRM-001 | ✓ |
| 8 | Configuration Management | ✓ | XPE-SCM-001 | ✓ |
| 9 | Problem Resolution | ✓ | XPE-SPR-001 | ✓ |
| — | SOUP Analysis | ✓ | XPE-SOUP-001 | ✓ |
| — | Traceability Matrix | ✓ | XPE-RTM-001 | ✓ |

---

*Package End — XPE-62304-PKG-001 v1.0*  
*IEC 62304:2006+AMD1:2015 Class B Complete Document Set*
