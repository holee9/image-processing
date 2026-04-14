# IEC 62304 Class B — Complete Document Package
# X-ray Post-Processing Engine (XPE)

**Package ID:** XPE-62304-PKG-001 v1.0  
**Software Safety Classification:** Class B (Non-serious injury possible)  
**Date:** 2026-04-03  
**Applicable Standard:** IEC 62304:2006+AMD1:2015  
**Companion Standards:** ISO 14971:2019, IEC 62366-1:2015, ISO 13485:2016  

---

## IEC 62304 Class B 적용 범위 (Clause Applicability Matrix)

Class B는 Class A의 모든 요구사항과 추가 요구사항을 포함한다.

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

## 1. 목적

본 계획은 X-ray Post-Processing Engine(XPE)의 소프트웨어 개발 생명주기를 IEC 62304:2006+AMD1:2015 Class B 요구사항에 따라 정의한다.

## 2. 범위

| 항목 | 설명 |
|------|-------------|
| 제품명 | X-ray Post-Processing Engine (XPE) |
| 소프트웨어 시스템 | 디지털 방사선 촬영을 위한 영상 처리 파이프라인 |
| 안전 분류 | **Class B** — 경미한 상해 가능성 |
| 분류 근거 | SW 오류 시 영상 품질 저하로 진단 지연/오류 가능. 외부 risk control(방사선사 확인, 재촬영 protocol)이 심각한 상해를 방지. |
| 의도된 사용 | FPD raw 이미지를 진단용 DICOM 영상으로 변환 |
| 작동 환경 | Windows 11 (x86-64), embedded Linux (ARM) |

## 3. 소프트웨어 개발 생명주기 모델

**선택 모델:** Iterative Incremental (3-Phase)

```
Phase 1 (Foundation) → Phase 2 (Clinical) → Phase 3 (Intelligence)
각 Phase 내부: Sprint 단위 (4주) iterative 개발
```

| 활동 | Phase 1 | Phase 2 | Phase 3 |
|----------|---------|---------|---------|
| 요구사항 분석 | ✓ | ✓ (delta) | ✓ (delta) |
| 아키텍처 설계 | ✓ | ✓ (extension) | ✓ (extension) |
| 단위 식별 | ✓ | ✓ | ✓ |
| 구현 | ✓ | ✓ | ✓ |
| 단위 검증 | ✓ | ✓ | ✓ |
| 통합 테스트 | ✓ | ✓ | ✓ |
| 시스템 테스트 | ✓ | ✓ | ✓ |
| 릴리스 | ✓ (v1.0) | ✓ (v2.0) | ✓ (v3.0) |

## 4. 소프트웨어 개발 계획 (5.1.1 — 5.1.3)

### 4.1 사용할 프로세스

| 프로세스 | 참조 문서 |
|---------|-------------------|
| 요구사항 분석 | XPE-SRS-001 |
| 아키텍처 설계 | XPE-SAD-001 |
| 단위 식별 | XPE-SDD-001 |
| 검증 & 밸리데이션 | XPE-VVP-001 |
| 통합 테스트 | XPE-ITP-001 |
| 시스템 테스트 | XPE-STP-001 |
| Risk 관리 | XPE-SRM-001 (→ ISO 14971) |
| 구성 관리 | XPE-SCM-001 |
| 문제 해결 | XPE-SPR-001 |
| 릴리스 | XPE-SRP-001 |
| 유지보수 | XPE-SMP-001 |

### 4.2 활동별 산출물

| 활동 | 산출물 | 검증 방법 |
|----------|-------------|-------------------|
| 요구사항 | SRS 문서 | 형식적 검토 (승인) |
| 아키텍처 | SAD 문서 + 다이어그램 | 형식적 검토 |
| 단위 식별 | 소프트웨어 단위 목록 | 아키텍처 대비 검토 |
| 구현 | 소스 코드, 빌드 스크립트 | 코드 검토 + 단위 테스트 |
| 단위 검증 | 단위 테스트 보고서 | 테스트 실행 |
| 통합 테스트 | 통합 테스트 보고서 | 테스트 실행 |
| 시스템 테스트 | 시스템 테스트 보고서 | 테스트 실행 |
| 릴리스 | 릴리스 노트, 아카이브 | 릴리스 체크리스트 |

## 5. 통합 및 통합 테스트 계획 (5.1.5)

### 5.1 통합 전략

**방법:** Bottom-up integration

```
Level 1: 개별 알고리즘 모듈 (Offset, Gain 등)
Level 2: 처리 단계 그룹 (Pre-Processing, Core, Display)
Level 3: 전체 파이프라인 통합
Level 4: 시스템 통합 (SW + HW detector 인터페이스)
```

### 5.2 통합 테스트 범위

| 통합 레벨 | 테스트 초점 |
|-------------------|-----------|
| L1 → L2 | 순차 알고리즘 간 데이터 흐름, 버퍼 형식 |
| L2 → L3 | 파이프라인 처리량, 메모리 관리, 오류 전파 |
| L3 → L4 | DICOM I/O, detector 인터페이스, 시간 제약 |

## 6. 소프트웨어 검증 계획 (5.1.6)

| 검증 활동 | 방법 | 승인 기준 | 도구 |
|----------------------|--------|-------------------|-------|
| 요구사항 검토 | 형식적 검토 | 100% 요구사항 검토, 승인 | Gitea Issues |
| 아키텍처 검토 | 형식적 검토 | SRS와의 추적성 검증 | Manual |
| 코드 검토 | 동료 검토 | 코딩 표준 준수, 심각한 문제 없음 | Gitea PR review |
| 단위 테스트 | 자동화된 테스트 | ≥ 80% 명령문 커버리지, 모든 테스트 통과 | Google Test, gcov |
| 통합 테스트 | 자동화 + 수동 | 모든 인터페이스 검증 | CTest |
| 시스템 테스트 | 테스트 실행 | 모든 SRS 요구사항 검증 | Custom test harness |
| 회귀 테스트 | 자동화된 테스트 | 이전 통과 테스트 실패 없음 | CI pipeline |

## 7. 소프트웨어 Risk 관리 계획 (5.1.7)

| 항목 | 설명 |
|------|-------------|
| Risk 관리 표준 | ISO 14971:2019 |
| Risk 관리 파일 | XPE-SRM-001 |
| 위험 식별 방법 | FMEA + FTA (소프트웨어 특화) |
| Risk 수용 기준 | ISO 14971 Annex C에 따른 (확률 × 심각도 매트릭스) |
| Risk 제어 구현 | SRS에서 안전 요구사항으로 문서화 (REQ-SAFE-xxx) |
| 잔존 Risk | ISO 14971 clause 7에 따라 평가 |

## 8. 문서 계획 (5.1.8)

| 문서 | 형식 | 위치 | 검토 주기 |
|----------|--------|----------|-------------|
| 모든 기술 문서 | Markdown → PDF (Pandoc 사용) | Gitea repository `/docs/` | Sprint 단위 |
| 소스 코드 | C++ 17 / C# | Gitea repository `/src/` | Commit 단위 (PR) |
| 테스트 결과 | JUnit XML + HTML report | CI artifacts | Build 단위 |
| Risk 관리 파일 | Markdown + Excel | Gitea `/risk/` | Phase 단위 |

## 9. 소프트웨어 구성 관리 계획 (5.1.9 — 5.1.11)

| 항목 | 설명 |
|------|-------------|
| SCM 도구 | Gitea (self-hosted, DS224+) |
| Branching 전략 | GitFlow (main/develop/feature/release/hotfix) |
| 구성 항목 | 소스 코드, 테스트 코드, 문서, 빌드 스크립트, SOUP 라이브러리 |
| 버전 체계 | Semantic versioning (MAJOR.MINOR.PATCH) |
| 빌드 재현성 | Docker 기반 빌드 환경, 고정된 도구체인 버전 |
| Baseline | `main` 브랜치의 tagged 릴리스 |
| 변경 제어 | Gitea PR + 필수 reviewer 승인 |

### 9.1 지원 항목 (5.1.10)

| 항목 | 버전 제어 | 정당성 |
|------|----------------|--------------|
| Compiler (GCC/MSVC) | Dockerfile에서 고정 | 재현성 |
| CMake | 고정 버전 | 빌드 일관성 |
| SOUP 라이브러리 | Lock file (conan.lock / vcpkg.json) | 의존성 추적 |
| 테스트 프레임워크 | 고정 버전 | 테스트 재현성 |

### 9.2 검증 전 제어 대상 구성 항목 (5.1.11)

모든 software item은 단위/통합 테스트 수행 전 Gitea에 commit되어야 하며, 테스트는 committed version에 대해서만 실행된다.

---

# 문서 2: 소프트웨어 요구사항 명세 (XPE-SRS-001)

**Clause 커버리지:** 5.2.1 — 5.2.6

## 1. 목적

XPE 소프트웨어 시스템의 기능, 성능, 인터페이스 및 안전 요구사항을 정의한다.

## 2. 요구사항 내용 (5.2.2)

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

## 3. 기능 요구사항

### 3.1 Pre-Processing

| Req ID | 요구사항 | 카테고리 | 우선순위 | Risk 제어 |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-001 | 시스템은 raw detector data에서 dark offset을 감산하여 고유 신호를 제거해야 한다 | (a) | Must | — |
| SRS-FUNC-002 | 시스템은 gain map을 적용하여 pixel 간 감도 차이를 보정해야 한다 | (a) | Must | — |
| SRS-FUNC-003 | 시스템은 bad pixel map 기반으로 불량 화소를 검출하고 인접 pixel 보간으로 보정해야 한다 | (a) | Must | SRS-SAFE-003 |
| SRS-FUNC-004 | 시스템은 이전 exposure의 잔류 신호(ghost/lag)를 multi-exponential decay model로 보정해야 한다 | (a) | Must | SRS-SAFE-004 |

### 3.2 Core Processing

| Req ID | 요구사항 | 카테고리 | 우선순위 | Risk 제어 |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-010 | 시스템은 선형 detector response를 logarithmic domain으로 변환해야 한다 | (a) | Must | — |
| SRS-FUNC-011 | 시스템은 edge-preserving noise reduction(최소 bilateral filter)을 제공해야 한다 | (a) | Must | — |
| SRS-FUNC-012 | 시스템은 CLAHE 기반 adaptive contrast enhancement를 제공해야 한다 | (a) | Must | — |
| SRS-FUNC-013 | 시스템은 frequency-selective edge enhancement를 제공해야 한다 | (a) | Must | SRS-SAFE-005 |
| SRS-FUNC-014 | 시스템은 multiscale frequency processing(Laplacian pyramid, ≥ 8 level)을 제공해야 한다 (Phase 2) | (a) | Should | — |

### 3.3 Display Processing

| Req ID | 요구사항 | 카테고리 | 우선순위 | Risk 제어 |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-020 | 시스템은 DICOM Modality LUT (Rescale Slope/Intercept)를 적용해야 한다 | (a) | Must | — |
| SRS-FUNC-021 | 시스템은 VOI LUT (LINEAR, LINEAR_EXACT, SIGMOID)를 지원해야 한다 | (a) | Must | SRS-SAFE-006 |
| SRS-FUNC-022 | 시스템은 DICOM PS3.14 GSDF에 따른 Presentation LUT를 적용해야 한다 | (a) | Must | SRS-SAFE-007 |
| SRS-FUNC-023 | 시스템은 Photometric Interpretation MONOCHROME1/MONOCHROME2를 올바르게 처리해야 한다 | (a) | Must | — |

### 3.4 DICOM I/O

| Req ID | 요구사항 | 카테고리 | 우선순위 | Risk 제어 |
|--------|------------|----------|----------|-------------|
| SRS-FUNC-030 | 시스템은 DX IOD (1.2.840.10008.5.1.4.1.1.1.1) FOR PROCESSING / FOR PRESENTATION을 읽고 쓸 수 있어야 한다 | (b)(c) | Must | — |
| SRS-FUNC-031 | 시스템은 Grayscale Softcopy Presentation State를 생성 및 적용할 수 있어야 한다 | (b)(c) | Must | — |
| SRS-FUNC-032 | 시스템은 JPEG 2000 Lossless 및 Explicit VR Little Endian transfer syntax를 지원해야 한다 | (b) | Must | — |

## 4. 입력/출력 요구사항 (5.2.2.b)

| Req ID | 요구사항 |
|--------|------------|
| SRS-IO-001 | **입력:** 14-16 bit raw detector data (binary format, detector-specific protocol) |
| SRS-IO-002 | **입력:** Calibration data (offset map, gain map, bad pixel map) — binary format |
| SRS-IO-003 | **입력:** DICOM image files (DX IOD) |
| SRS-IO-004 | **출력:** 처리된 DICOM 이미지 (FOR PRESENTATION) — 모든 필수 Type 1/2 태그 |
| SRS-IO-005 | **출력:** 처리된 DICOM 이미지 (FOR PROCESSING) — 전체 bit-depth 보존 |
| SRS-IO-006 | **출력:** DICOM Grayscale Softcopy Presentation State |

## 5. 인터페이스 요구사항 (5.2.2.c)

| Req ID | 인터페이스 | Protocol | 방향 |
|--------|-----------|----------|-----------|
| SRS-IF-001 | Detector Interface | USB 3.x / Ethernet (detector-specific SDK) | 입력 |
| SRS-IF-002 | PACS Interface | DICOM C-STORE SCU | 출력 |
| SRS-IF-003 | Worklist Interface | DICOM C-FIND SCU (MWL) | 입력 |
| SRS-IF-004 | GUI Interface | C ABI (DLL export) → C# P/Invoke | 양방향 |
| SRS-IF-005 | CAD Plugin Interface | REST API + ONNX Runtime | 양방향 |

## 6. 안전 요구사항 (5.2.3 — Risk 제어 조치)

| Req ID | 안전 요구사항 | 위험 참조 | 제어 타입 |
|--------|-------------------|-----------------|-------------|
| SRS-SAFE-001 | 시스템은 processing 중 원본 raw data를 보존해야 한다 (비파괴 처리) | HAZ-001 | Design |
| SRS-SAFE-002 | 시스템은 모든 processing parameter의 기본값을 body-part별 validated preset으로 설정해야 한다 | HAZ-002 | Design |
| SRS-SAFE-003 | 시스템은 bad pixel correction 실패 시 해당 영역을 시각적으로 표시하고 operator에게 경고해야 한다 | HAZ-003 | Alert (5.2.2.d) |
| SRS-SAFE-004 | 시스템은 ghost correction이 적용되었는지 여부를 DICOM tag에 기록해야 한다 | HAZ-004 | 추적성 |
| SRS-SAFE-005 | 시스템은 edge enhancement gain을 body-part별 safe range로 제한해야 한다 (overshoot에 의한 artifact 방지) | HAZ-005 | Design |
| SRS-SAFE-006 | 시스템은 W/L이 유효 범위를 벗어나면 operator에게 경고해야 한다 | HAZ-006 | Alert |
| SRS-SAFE-007 | 시스템은 GSDF 미보정 display에서 진단 영상을 표시할 때 경고를 표시해야 한다 | HAZ-007 | Alert |
| SRS-SAFE-008 | 시스템은 DL 기반 처리(bone suppression 등) 결과에 "AI-processed" label을 표시해야 한다 | HAZ-008 | Alert |
| SRS-SAFE-009 | 시스템은 원본 영상과 처리 영상 간 즉시 전환 기능을 제공해야 한다 | HAZ-009 | Design |

## 7. 성능 요구사항

| Req ID | 요구사항 | 승인 기준 |
|--------|------------|-------------------|
| SRS-PERF-001 | Pre-processing pipeline latency | ≤ 500ms (3072×3072, single-thread CPU) |
| SRS-PERF-002 | 전체 파이프라인 latency (Phase 1) | ≤ 3s |
| SRS-PERF-003 | VOI LUT 대화형 조정 latency | ≤ 16ms (60fps) |
| SRS-PERF-004 | 이미지당 피크 메모리 사용량 | ≤ 2 GB |
| SRS-PERF-005 | DICOM 파일 쓰기 시간 | ≤ 1s (uncompressed), ≤ 3s (JPEG 2000) |
| SRS-PERF-006 | 동시 처리 용량 | ≥ 2 이미지 동시 처리 |

## 8. 요구사항 검증 (5.2.6)

모든 요구사항은 다음 기준으로 검증된다:

- **검사 가능:** 각 요구사항은 pass/fail 판정 가능한 승인 기준을 가져야 한다
- **추적 가능:** RTM(XPE-RTM-001)에서 architecture → test case까지 추적 가능
- **고유성:** 각 요구사항은 고유 ID를 가지며 중복 없음
- **일관성:** 상호 모순 없음 (형식적 검토에서 확인)

---

# 문서 3: 소프트웨어 아키텍처 문서 (XPE-SAD-001)

**Clause 커버리지:** 5.3.1 — 5.3.6

## 1. 아키텍처 개요

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

| SW Item ID | 이름 | 안전 클래스 | 설명 |
|-----------|------|:----------:|-------------|
| SWI-1 | Pre-Processing Module | B | Offset, gain, defect pixel, ghost correction |
| SWI-2 | Core Processing Module | B | Noise reduction, contrast/edge enhancement, MFP |
| SWI-3 | Display Processing Module | B | Modality LUT, VOI LUT, GSDF |
| SWI-4 | DICOM I/O Module | B | DICOM read/write, Presentation State |
| SWI-5 | Common Infrastructure | B | Memory management, threading, logging, error handling |
| SWI-6 | SOUP Components | — | Third-party libraries (XPE-SOUP-001 참조) |

## 3. 인터페이스 (5.3.2)

### 3.1 모듈간 인터페이스

| 인터페이스 | From → To | 데이터 타입 | 메커니즘 |
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

### 3.3 외부 인터페이스

| 인터페이스 | Protocol | 오류 처리 |
|-----------|----------|---------------|
| Detector SDK | Vendor-specific C API | Timeout + retry (3×) → error state |
| PACS (C-STORE) | DICOM v3.0 SCU | Association failure → queue + retry |
| GUI | C ABI DLL export | Exception → error code return |

## 4. SOUP 명세 (5.3.3, 5.3.4)

→ 완전한 분석은 XPE-SOUP-001 참조

## 5. Risk 제어를 위한 분리 (5.3.5)

| Risk 제어 | 분리 방법 |
|-------------|-------------------|
| 원본 데이터 보존 | SWI-1은 input buffer를 read-only로 접근, 별도 output buffer에 기록 |
| 처리 오류 격리 | 각 SWI는 독립 error domain — 한 module의 exception이 다른 module에 전파되지 않음 |
| DL 처리 분리 | Phase 3 AI 모듈은 별도 process(sandbox)에서 실행, IPC로 결과 전달 |
| 파라미터 검증 | 모든 processing parameter는 SWI-5 내 validator를 거쳐 safe range 내에서만 적용 |

## 6. 아키텍처 검증 (5.3.6)

| 검증 항목 | 방법 | 기준 |
|-------------------|--------|----------|
| 모든 SRS 요구사항이 architecture에 매핑됨 | RTM 검토 | 100% 커버리지 |
| Interface 정의 완전성 | 형식적 검토 | 모든 data flow 정의됨 |
| SOUP 요구사항 충족 | SOUP analysis 검토 | 모든 SOUP 적합성 확인 |
| Risk 제어 구현 가능성 | Design 검토 | 모든 SRS-SAFE-xxx가 architecture에 반영 |
| 기존 system 호환성 | 검토 | RadiConsole™ GUI(WPF) 연동 확인 |

---

# 문서 4: 소프트웨어 단위 식별 (XPE-SDD-001)

**Clause 커버리지:** 5.4.1 (Class B: 식별만 — 상세 설계 불필요)

## 1. Software Unit 분해

### SWI-1: Pre-Processing Module

| Unit ID | Unit 이름 | 기능 |
|---------|-----------|----------|
| SWU-1.1 | OffsetCorrector | Dark/offset 감산 (saturation 산술 포함) |
| SWU-1.2 | GainCorrector | Flat-field gain 곱셈 |
| SWU-1.3 | DefectPixelCorrector | Bad pixel 검출 & 보간 |
| SWU-1.4 | GhostCorrector | Multi-exponential lag 보정 |
| SWU-1.5 | CalibrationManager | Calibration data 로드/저장/검증 |

### SWI-2: Core Processing Module

| Unit ID | Unit 이름 | 기능 |
|---------|-----------|----------|
| SWU-2.1 | LogTransform | Logarithmic domain 변환 |
| SWU-2.2 | NoiseReducer | Bilateral filter, NLM |
| SWU-2.3 | ContrastEnhancer | CLAHE 구현 |
| SWU-2.4 | EdgeEnhancer | Unsharp masking, frequency-selective |
| SWU-2.5 | MultiscaleProcessor | Laplacian pyramid MFP (Phase 2) |
| SWU-2.6 | BodyPartRecognizer | CNN classifier (Phase 2) |
| SWU-2.7 | ImageStitcher | Panoramic stitching (Phase 2) |
| SWU-2.8 | BoneSuppressionEngine | DL U-Net inference (Phase 3) |

### SWI-3: Display Processing Module

| Unit ID | Unit 이름 | 기능 |
|---------|-----------|----------|
| SWU-3.1 | ModalityLUT | Rescale Slope/Intercept 적용 |
| SWU-3.2 | VoiLUT | W/L Linear, Sigmoid, LUT Sequence |
| SWU-3.3 | PresentationLUT | GSDF, photometric interpretation 처리 |
| SWU-3.4 | LUTManager | Preset 저장, custom LUT 관리 |

### SWI-4: DICOM I/O Module

| Unit ID | Unit 이름 | 기능 |
|---------|-----------|----------|
| SWU-4.1 | DicomReader | DICOM 파일 파싱, pixel data 추출 |
| SWU-4.2 | DicomWriter | DICOM 파일 생성, tag 채우기 |
| SWU-4.3 | PresentationStateIO | GSPS 생성/적용 |
| SWU-4.4 | DicomNetworkSCU | C-STORE, C-FIND SCU |

### SWI-5: Common Infrastructure

| Unit ID | Unit 이름 | 기능 |
|---------|-----------|----------|
| SWU-5.1 | MemoryPool | 사전 할당된 이미지 버퍼 풀 |
| SWU-5.2 | ThreadPool | Task 기반 병렬 실행 |
| SWU-5.3 | ErrorHandler | 중앙 집중식 오류/예외 관리 |
| SWU-5.4 | Logger | spdlog wrapper, audit trail |
| SWU-5.5 | ParameterValidator | 모든 파라미터에 대한 safe-range 강제 |
| SWU-5.6 | ConfigManager | System/user configuration 지속성 |

---

# 문서 5: 소프트웨어 검증 및 밸리데이션 계획 (XPE-VVP-001)

**Clause 커버리지:** 5.5.1 — 5.5.5, 5.6.1 — 5.6.7, 5.7.1 — 5.7.5

## 1. Unit 검증 (5.5)

### 1.1 Unit 검증 프로세스 (5.5.2)

| 항목 | 설명 |
|------|-------------|
| 프레임워크 | Google Test (C++), NUnit (C#) |
| Coverage 도구 | gcov + lcov (C++), dotCover (C#) |
| 실행 | CI pipeline (Gitea Actions) — develop/feature의 모든 commit에서 |
| 검토 | Unit test는 PR 검토의 일부로 검토됨 |

### 1.2 Unit 승인 기준 (5.5.3)

| 기준 | 목표 |
|-----------|--------|
| Statement coverage | software unit당 ≥ 80% |
| Branch coverage | software unit당 ≥ 70% |
| 모든 테스트 통과 | 100% (실패 없음) |
| 코딩 표준 준수 | 심각한 위반 없음 (MISRA C++ subset) |
| 메모리 누수 없음 | AddressSanitizer로 검증 |

### 1.3 Unit 검증 실행 (5.5.5)

각 software unit(SWU-x.y)에 대해:

1. Unit test suite 작성 (test case ID: UT-{unit_id}-{seq})
2. CI에서 자동 실행
3. Coverage report 생성
4. Test report를 Gitea artifact로 보관
5. 승인 기준 미달 시 merge 차단

## 2. 통합 테스트 (5.6)

### 2.1 통합 전략 (5.6.1)

| Phase | 통합 범위 | 사전 조건 |
|-------|------------------|---------------|
| I-1 | SWU-1.1 → SWU-1.4 (Pre-Processing chain) | 모든 Phase 1 unit이 unit test 통과 |
| I-2 | SWI-1 → SWI-2 (Pre → Core chain) | I-1 통과 |
| I-3 | SWI-2 → SWI-3 (Core → Display chain) | I-2 통과 |
| I-4 | SWI-1 → SWI-4 (전체 파이프라인) | I-3 통과 |
| I-5 | SWI-4 ↔ External (DICOM network) | I-4 통과 |

### 2.2 통합 테스트 내용 (5.6.3)

| Test ID | 테스트 설명 | 입력 | 예상 출력 |
|---------|-----------------|-------|-----------------|
| IT-001 | Offset → Gain chain 데이터 정합성 | Known synthetic raw + calibration | 사전 계산된 reference (PSNR ≥ 60dB) |
| IT-002 | 전체 pre-processing → core 흐름 | Phantom image (CDRAD 2.0) | Visual IQ ≥ 3.5/5 (expert review) |
| IT-003 | Pipeline → DICOM output | 전체 pipeline 입력 | DICOM 적합성 (DVTk validation 통과) |
| IT-004 | W/L 대화형 응답 | User W/L drag event | Display 업데이트 ≤ 16ms |
| IT-005 | 오류 전파: SWI-1 실패 | Corrupted calibration data | Graceful error, no crash, alert displayed |
| IT-006 | 메모리 안정성 | 100개 이미지 순차 처리 | 메모리 증가 > 5% 없음, 누수 없음 |

### 2.3 회귀 테스트 (5.6.4)

- 모든 통합 테스트는 regression suite에 포함
- Release branch merge 전 전체 regression 실행 필수
- Regression 실패 → release 차단

### 2.4 통합 테스트 기록 (5.6.5)

각 테스트 실행에 대해 기록:

- Test ID, date, executor
- SW version (Git commit hash)
- Test environment (OS, hardware, compiler)
- Pass/fail result
- 발견된 이상 (→ XPE-SPR-001 연계)

## 3. 시스템 테스트 (5.7)

### 3.1 시스템 테스트 계획 (5.7.1)

모든 SRS 요구사항에 대해 최소 1개의 system test case를 정의한다.

| SRS Req ID | System Test ID | 테스트 방법 | 통과 기준 |
|-----------|---------------|-------------|---------------|
| SRS-FUNC-001 | ST-001 | Synthetic data + reference comparison | PSNR ≥ 60dB vs reference |
| SRS-FUNC-003 | ST-003 | Known bad pixel injection | 주입된 모든 결함 보정됨 |
| SRS-FUNC-012 | ST-012 | 임상 이미지 세트 (N=50) | Reader IQ score ≥ 3.5/5 |
| SRS-FUNC-021 | ST-021 | W/L preset 적용 | Pixel value는 reference ± 1 일치 |
| SRS-FUNC-030 | ST-030 | DICOM 적합성 테스트 | DVTk 전체 validation 통과 |
| SRS-SAFE-001 | ST-SAFE-001 | 이미지 처리 후 raw 검증 | Raw data byte-identical |
| SRS-SAFE-003 | ST-SAFE-003 | 결함 보정 실패 강제 | 경고가 2초 내에 표시됨 |
| SRS-PERF-001 | ST-PERF-001 | Timing 측정 (3072×3072) | ≤ 500ms |
| SRS-PERF-002 | ST-PERF-002 | End-to-end timing | ≤ 3s |

### 3.2 시스템 테스트 기록 내용 (5.7.5)

| 필드 | 설명 |
|-------|-------------|
| Test ID | 고유 식별자 |
| SW Version | Release candidate 버전 + Git tag |
| Test Environment | 전체 HW/SW 명세 |
| Test Data | 입력 데이터셋 식별자 |
| Results | Pass/Fail + 측정값 |
| Anomalies | 문제 보고서 참조 (있는 경우) |
| Tester | 이름 + 서명 |
| Date | 실행 날짜 |

---

# 문서 6: 요구사항 추적성 매트릭스 (XPE-RTM-001)

**Clause 커버리지:** 5.1.1c, 5.3.6, 7.3.3

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

# 문서 7: 소프트웨어 Risk 관리 파일 (XPE-SRM-001)

**Clause 커버리지:** 7.1 — 7.4

## 1. 위험 식별 (7.1)

| 위험 ID | 위험 상황 | 심각도 | 확률 | Risk 수준 | SW 원인 |
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

## 2. Risk 제어 조치 (7.2)

| 위험 ID | Risk 제어 | 구현 | SRS Req |
|-----------|-------------|---------------|---------|
| HAZ-001 | Non-destructive processing (원본 보존) | Read-only input buffer + 별도 output | SRS-SAFE-001 |
| HAZ-002 | Validated body-part preset 적용 | Auto-selection + safe-range validator | SRS-SAFE-002 |
| HAZ-003 | Bad pixel correction 실패 시 경고 | ErrorHandler → UI alert | SRS-SAFE-003 |
| HAZ-004 | Ghost correction 적용 여부 DICOM 기록 | Custom DICOM tag 기록 | SRS-SAFE-004 |
| HAZ-005 | Enhancement gain 범위 제한 | ParameterValidator safe-range check | SRS-SAFE-005 |
| HAZ-006 | W/L 유효 범위 경고 | Range check + UI warning | SRS-SAFE-006 |
| HAZ-007 | 미보정 display 경고 | GSDF compliance check on startup | SRS-SAFE-007 |
| HAZ-008 | AI-processed label 표시 | Overlay text + DICOM annotation | SRS-SAFE-008 |
| HAZ-009 | 원본/처리 전환 기능 | UI toggle + state indicator | SRS-SAFE-009 |

## 3. Risk 제어 검증 (7.3)

| 위험 ID | 검증 방법 | Test ID | 상태 |
|-----------|-------------------|---------|--------|
| HAZ-001 | Processing 후 raw data byte 비교 | ST-SAFE-001 | 계획됨 |
| HAZ-002 | Preset 검증 테스트 (모든 body-part) | ST-SAFE-002 | 계획됨 |
| HAZ-003 | 강제 결함 보정 실패 주입 | ST-SAFE-003 | 계획됨 |
| HAZ-004 | DICOM tag 존재 검증 | ST-004 | 계획됨 |
| HAZ-005 | 최대 gain 적용 + visual artifact 확인 | ST-013 | 계획됨 |
| HAZ-006 | W/L out-of-range 주입 | ST-SAFE-006 | 계획됨 |
| HAZ-007 | Non-GSDF display 시뮬레이션 | ST-SAFE-007 | 계획됨 |
| HAZ-008 | AI output label 존재 테스트 | ST-SAFE-008 | 계획됨 |
| HAZ-009 | Toggle 기능 테스트 | ST-SAFE-009 | 계획됨 |

## 4. SOUP Risk 관리 (7.4)

→ XPE-SOUP-001 참조

---

# 문서 8: SOUP 목록 & 분석 (XPE-SOUP-001)

**Clause 커버리지:** 5.3.3, 5.3.4, 7.4.1 — 7.4.3

| SOUP ID | 이름 | 버전 | 목적 | 라이선스 | 안전 클래스 |
|---------|------|---------|---------|---------|:----------:|
| SOUP-001 | OpenCV | 4.9.x | Image processing primitives (filter, transform) | Apache 2.0 | B |
| SOUP-002 | dcmtk | 3.6.8 | DICOM read/write/network | BSD-3 | B |
| SOUP-003 | ONNX Runtime | 1.17.x | DL model inference (Phase 3) | MIT | B |
| SOUP-004 | spdlog | 1.13.x | Logging framework | MIT | A |
| SOUP-005 | nlohmann/json | 3.11.x | JSON configuration parsing | MIT | A |
| SOUP-006 | Google Test | 1.14.x | Unit testing (dev only) | BSD-3 | N/A |
| SOUP-007 | fmt | 10.x | String formatting | MIT | A |
| SOUP-008 | Eigen | 3.4.x | Matrix operations (MFP) | MPL-2.0 | B |

### SOUP 기능 및 성능 요구사항 (5.3.3)

| SOUP ID | 기능 요구사항 | 성능 요구사항 |
|---------|----------------------|------------------------|
| SOUP-001 | cv::bilateralFilter, cv::CLAHE, pyramid ops 정상 동작 | 3072×3072 bilateral ≤ 200ms |
| SOUP-002 | DX IOD read/write, C-STORE SCU, JPEG 2000 codec | DICOM file write ≤ 1s |
| SOUP-003 | ONNX model load + inference (GPU/CPU) | Inference ≤ 2s (GPU) |
| SOUP-008 | Matrix decomposition, FFT | Laplacian pyramid 12-level ≤ 500ms |

### SOUP 시스템 요구사항 (5.3.4)

| SOUP ID | OS | 하드웨어 | 의존성 |
|---------|----|---------|----|
| SOUP-001 | Windows 11, Linux | x86-64 (AVX2), ARM (NEON) | — |
| SOUP-002 | Windows 11, Linux | — | OpenSSL (TLS) |
| SOUP-003 | Windows 11, Linux | NVIDIA GPU (CUDA 12) optional | CUDA Toolkit (optional) |
| SOUP-008 | Cross-platform | — | — |

### SOUP Risk 분석 (7.4)

| SOUP ID | 가능한 실패 | 영향 | 완화 |
|---------|------------------|--------|-----------|
| SOUP-001 | Filter가 잘못된 출력 생성 | 영상 품질 저하 | 출력 검증 (PSNR check vs reference) |
| SOUP-002 | DICOM tag 오처리 | Non-conformant 출력 | DVTk conformance 테스트 in CI |
| SOUP-003 | Model inference가 NaN/Inf 생성 | AI module crash 또는 오류 출력 | 출력 범위 검증 + fallback to non-AI pipeline |
| SOUP-008 | 분해에서 수치적 불안정성 | MFP artifact | Condition number check, fallback to simpler decomposition |

---

# 문서 9: 소프트웨어 구성 관리 계획 (XPE-SCM-001)

**Clause 커버리지:** 8.1 — 8.3

## 1. 구성 식별 (8.1)

| 구성 항목 유형 | 명명 규칙 | 위치 |
|-----------------|-------------------|----------|
| 소스 코드 | `src/{module}/{file}.cpp/.h` | Gitea `xpe-engine` repo |
| 테스트 코드 | `test/{module}/{file}_test.cpp` | 동일 repo `/test/` |
| 문서 | `docs/{doc-id}.md` | 동일 repo `/docs/` |
| 빌드 스크립트 | `CMakeLists.txt`, `Dockerfile` | Root |
| SOUP lockfile | `vcpkg.json` + `vcpkg-configuration.json` | Root |
| Calibration data | `cal/{panel-id}/` | 별도 `xpe-calibration` repo |

## 2. 변경 제어 (8.2)

### 2.1 변경 요청 프로세스

```
1. Issue 생성 (Gitea Issue)
   ↓
2. 영향 분석 (영향받는 SWI, 테스트 범위, risk 영향)
   ↓
3. Feature branch 생성 (feature/{issue-id}-{desc})
   ↓
4. 구현 + 단위 테스트
   ↓
5. Pull Request (PR) 생성
   ↓
6. 코드 검토 (≥ 1 reviewer 승인)
   ↓
7. CI 통과 (빌드 + 단위 테스트 + 정적 분석)
   ↓
8. develop으로 merge
   ↓
9. 통합 테스트 (develop branch)
```

### 2.2 추적성 (8.2.4)

- 모든 변경 요청은 Gitea Issue에 기록
- PR은 관련 Issue를 참조 (`Fixes #123`)
- Commit message는 Issue ID 포함
- Release tag는 포함된 Issue 목록 기록

## 3. 구성 상태 회계 (8.3)

| 보고서 | 빈도 | 내용 |
|--------|-----------|---------|
| CI Build Report | Commit마다 | 빌드 상태, 테스트 결과, coverage |
| Release Note | Release마다 | 버전, 변경사항, known anomalies, SOUP 버전 |
| Configuration Baseline | Release마다 | 모든 구성 항목 + 버전의 완전한 목록 |

---

# 문서 10: 소프트웨어 릴리스 절차 (XPE-SRP-001)

**Clause 커버리지:** 5.8.1 — 5.8.8

## 릴리스 체크리스트

| 단계 | Clause | 활동 | 증거 |
|------|--------|----------|----------|
| 1 | 5.8.1 | 계획된 모든 활동 완료 검증 | RTM 100% 통과 검증 |
| 2 | 5.8.1 | SRS ↔ System Test 추적성 완료 검증 | XPE-RTM-001 승인 |
| 3 | 5.8.2 | 알려진 모든 이상 문서화 | Known Anomalies List (release note에) |
| 4 | 5.8.3 | 각 잔존 이상에 대한 risk 수용성 평가 | ISO 14971에 따른 Risk 평가 |
| 5 | 5.8.4 | 릴리스된 SW 버전 문서화 | Git tag + 버전 문자열 |
| 6 | 5.8.5 | 빌드 환경 & 절차 문서화 | Dockerfile + 빌드 스크립트 |
| 7 | 5.8.6 | 빌드 재현성 검증 | tag에서 재빌드 → 이진 비교 |
| 8 | 5.8.7 | 릴리스 활동 완료 검증 | Release 체크리스트 승인 |
| 9 | 5.8.8 | 구성 관리 시스템에 아카이브 | Gitea tag + artifact archive |

### 릴리스 노트 템플릿

```
═══════════════════════════════════════════
XPE 릴리스 노트
버전: x.y.z
날짜: YYYY-MM-DD
Git Tag: vx.y.z
Git Commit: {full SHA}
═══════════════════════════════════════════

1. 릴리스된 Software Items
   - SWI-1 Pre-Processing v{x.y}
   - SWI-2 Core Processing v{x.y}
   - ...

2. SOUP Component 버전
   - OpenCV {version}
   - dcmtk {version}
   - ...

3. 이전 릴리스 이후 변경사항
   - {Issue #} - {설명}
   - ...

4. Known 잔존 이상
   | ID | 설명 | 심각도 | Risk 평가 |
   |----|-------------|----------|-----------------|

5. 빌드 환경
   - OS: {Ubuntu 24.04 / Windows 11}
   - Compiler: {GCC 13.2 / MSVC 17.9}
   - CMake: {3.28}
   - Docker Image: {tag}

6. 검증 요약
   - 단위 테스트: {X}/{Y} 통과 ({Z}% coverage)
   - 통합 테스트: {X}/{Y} 통과
   - 시스템 테스트: {X}/{Y} 통과
   - DICOM 적합성: PASS

승인자: ____________________  날짜: ________
```

---

# 문서 11: 소프트웨어 유지보수 계획 (XPE-SMP-001)

**Clause 커버리지:** 6.1 — 6.3

## 1. 유지보수 활동

| 활동 | 트리거 | 프로세스 |
|----------|---------|---------|
| Corrective | Problem report (현장 이슈) | SPR-001 → 수정 → 회귀 테스트 → 릴리스 |
| Adaptive | OS/SOUP 업데이트 | 영향 분석 → 수정 → 전체 V&V → 릴리스 |
| Perfective | Feature request | SRS 업데이트 → 전체 개발 주기 |
| Preventive | 정기 검토 | SOUP 취약점 스캔 (분기별) |

## 2. Problem Report 분석 (6.2.3)

각 post-market problem report에 대해:

1. Safety 영향 평가 (ISO 14971 risk 매트릭스)
2. 영향받는 SW Item / Unit 식별
3. Regulatory 보고 필요 여부 판단
4. 수정 우선순위 결정
5. 회귀 테스트 범위 결정

## 3. 피드백 → 개발 주기

유지보수에서 발견된 문제가 design 변경을 요구하면, Clause 5 (Software Development Process)의 해당 단계부터 재진입한다.

---

# 문서 12: 소프트웨어 문제 해결 프로세스 (XPE-SPR-001)

**Clause 커버리지:** 9.1 — 9.8

## 문제 해결 워크플로우

```
1. 문제 감지
   ├─ 내부 (테스팅, 코드 검토)
   └─ 외부 (현장 보고, 고객 피드백)
        ↓
2. Problem Report 생성 (Gitea Issue, label: "problem-report")
   - 설명, 재현 단계, 심각도, SW 버전
        ↓
3. 조사 & 영향 분석
   - 근본 원인 식별
   - 영향받는 SW item / 요구사항
   - Safety 영향 평가 (→ XPE-SRM-001)
        ↓
4. 처리 결정
   ├─ 수정 → 변경 요청 → 개발 주기 (Clause 5)
   ├─ 연기 → Risk 평가 + 정당성 + known anomaly list
   └─ 조치 없음 → 정당성 문서화
        ↓
5. 검증
   - 수정은 단위/통합/시스템 테스트로 검증
   - 회귀 테스트 통과
        ↓
6. 종료
   - Problem report는 해결과 함께 종료
   - 트렌드 분석 (분기별)
```

## 심각도 분류

| 수준 | 정의 | 응답 시간 |
|-------|-----------|--------------|
| 심각 | Safety 관련, 환자 상해 가능성 | 24시간 내 조사 시작 |
| 주요 | 기능 실패, 회피책 가능 | 5 business days |
| 경미 | 미용적 또는 경미한 usability 이슈 | 다음 계획된 릴리스 |

---

# 문서 13: IEC 62304 준수 매트릭스 (요약)

| Clause | 제목 | Class B 필수 | 문서 참조 | 상태 |
|--------|-------|:---------------:|-------------------|--------|
| 5.1 | 개발 계획 | ✓ | XPE-SDP-001 | ✓ |
| 5.2 | 요구사항 분석 | ✓ | XPE-SRS-001 | ✓ |
| 5.3 | 아키텍처 설계 | ✓ | XPE-SAD-001 | ✓ |
| 5.4.1 | Unit 식별 | ✓ | XPE-SDD-001 | ✓ |
| 5.4.2-4 | 상세 설계 | — (Class C만) | N/A | N/A |
| 5.5 | Unit 구현 & 검증 | ✓ | XPE-VVP-001 | ✓ |
| 5.6 | 통합 테스트 | ✓ | XPE-VVP-001 (§2) | ✓ |
| 5.7 | 시스템 테스트 | ✓ | XPE-VVP-001 (§3) | ✓ |
| 5.8 | 소프트웨어 릴리스 | ✓ | XPE-SRP-001 | ✓ |
| 6 | 유지보수 | ✓ | XPE-SMP-001 | ✓ |
| 7 | Risk 관리 | ✓ | XPE-SRM-001 | ✓ |
| 8 | 구성 관리 | ✓ | XPE-SCM-001 | ✓ |
| 9 | 문제 해결 | ✓ | XPE-SPR-001 | ✓ |
| — | SOUP 분석 | ✓ | XPE-SOUP-001 | ✓ |
| — | 추적성 매트릭스 | ✓ | XPE-RTM-001 | ✓ |

---

*패키지 종료 — XPE-62304-PKG-001 v1.0*  
*IEC 62304:2006+AMD1:2015 Class B 완전 문서 세트*
