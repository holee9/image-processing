# Software Development Plan

**Document ID:** XPE-SDP-001 v1.0  
**IEC 62304 Clause:** 5.1.1 — 5.1.11  
**Safety Classification:** Class B  
**Date:** 2026-04-03  
**Author:** XPE Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose & Scope

본 문서는 X-ray Post-Processing Engine(XPE) 소프트웨어의 개발 생명주기를 IEC 62304:2006+AMD1:2015 Class B 요구사항에 따라 정의한다.

| Item | Description |
|------|-------------|
| Product | X-ray Post-Processing Engine (XPE) |
| Function | FPD raw image → 진단용 DICOM 영상 변환 |
| Safety Class | **Class B** — Non-serious injury possible |
| Classification Rationale | SW 오류 시 영상 품질 저하로 진단 지연 가능. 외부 risk control(방사선사 확인, 재촬영)이 심각한 상해를 방지 |
| Target Platform | Windows 11 (x86-64), Embedded Linux (ARM) |
| Companion Standards | ISO 14971:2019, IEC 62366-1:2015, ISO 13485:2016 |

## 2. Software Development Life Cycle Model (5.1.1)

**선택 모델:** Iterative Incremental (3-Phase)

| Phase | Name | Duration | Scope |
|-------|------|----------|-------|
| Phase 1 | Foundation | 24주 | 최소 필수 파이프라인 |
| Phase 2 | Clinical | 20주 | MFP, Body-Part Adaptive, Stitching |
| Phase 3 | Intelligence | 16주 | DL Bone Suppression, CAD Framework |

각 Phase 내부는 4주 Sprint 단위 iterative 개발을 수행한다.

### 2.1 Phase별 활동 매트릭스

| Activity | Phase 1 | Phase 2 | Phase 3 |
|----------|:-------:|:-------:|:-------:|
| Requirements Analysis (5.2) | ✓ | ✓ (delta) | ✓ (delta) |
| Architecture Design (5.3) | ✓ | ✓ (extension) | ✓ (extension) |
| Unit Identification (5.4.1) | ✓ | ✓ | ✓ |
| Implementation & Unit Verification (5.5) | ✓ | ✓ | ✓ |
| Integration Testing (5.6) | ✓ | ✓ | ✓ |
| System Testing (5.7) | ✓ | ✓ | ✓ |
| Release (5.8) | ✓ (v1.0) | ✓ (v2.0) | ✓ (v3.0) |

## 3. Deliverables & Verification (5.1.1b, 5.1.3)

| Deliverable | Document ID | Verification Method |
|-------------|-------------|-------------------|
| Software Requirements Specification | XPE-SRS-001 | Formal review + sign-off |
| Software Architecture Document | XPE-SAD-001 | Formal review + sign-off |
| Software Unit Identification | XPE-SDD-001 | Review against architecture |
| V&V Plan | XPE-VVP-001 | Formal review |
| Integration Test Plan | XPE-ITP-001 | Formal review |
| System Test Plan | XPE-STP-001 | Formal review |
| Requirements Traceability Matrix | XPE-RTM-001 | Completeness check |
| Software Risk Management File | XPE-SRM-001 | ISO 14971 compliance review |
| SOUP Analysis | XPE-SOUP-001 | Technical review |
| Configuration Management Plan | XPE-SCM-001 | Process audit |
| Release Procedure | XPE-SRP-001 | Formal review |
| Maintenance Plan | XPE-SMP-001 | Formal review |
| Problem Resolution Process | XPE-SPR-001 | Process audit |

## 4. Integration & Integration Test Planning (5.1.5)

### 4.1 Integration Strategy

Bottom-up integration을 적용한다.

```
Level 1: Individual algorithm units (OffsetCorrector, GainCorrector, etc.)
Level 2: Processing stage groups (Pre-Processing, Core, Display)
Level 3: Full pipeline integration (SWI-1 → SWI-2 → SWI-3 → SWI-4)
Level 4: System integration (SW ↔ HW detector interface)
```

### 4.2 Integration Test Scope

| Level | Test Focus | Reference |
|-------|-----------|-----------|
| L1→L2 | Data flow, buffer format, boundary values | XPE-ITP-001 |
| L2→L3 | Pipeline throughput, memory, error propagation | XPE-ITP-001 |
| L3→L4 | DICOM I/O, detector interface, timing | XPE-ITP-001 |

## 5. Software Verification Planning (5.1.6)

| Activity | Method | Criteria | Tool |
|----------|--------|----------|------|
| Requirements review | Formal review | 100% reviewed, sign-off | Gitea Issues |
| Architecture review | Formal review | Traceability to SRS | Manual |
| Code review | Peer review | Coding standard, no critical issues | Gitea PR |
| Unit testing | Automated | ≥ 80% statement coverage, 0 failures | Google Test, gcov |
| Integration testing | Automated + manual | All interfaces verified | CTest |
| System testing | Test execution | All SRS requirements verified | Custom harness |
| Regression testing | Automated | No previously passing tests fail | CI pipeline |

## 6. Software Risk Management Planning (5.1.7)

| Item | Description |
|------|-------------|
| Standard | ISO 14971:2019 |
| Risk file | XPE-SRM-001 |
| Method | FMEA + FTA (software-specific) |
| Acceptability | Per ISO 14971 Annex C (probability × severity) |
| Implementation | Safety requirements (SRS-SAFE-xxx) |
| Residual risk | Evaluated per ISO 14971 clause 7 |

## 7. Documentation Planning (5.1.8)

| Document Type | Format | Location | Review |
|---------------|--------|----------|--------|
| Technical docs | Markdown → PDF (Pandoc) | Gitea `/docs/` | Per Sprint |
| Source code | C++ 17 / C# | Gitea `/src/` | Per commit (PR) |
| Test results | JUnit XML + HTML | CI artifacts | Per build |
| Risk file | Markdown + Excel | Gitea `/risk/` | Per Phase |

## 8. Configuration Management Planning (5.1.9 — 5.1.11)

| Item | Description |
|------|-------------|
| SCM tool | Gitea (self-hosted, Synology DS224+) |
| Branching | GitFlow (main / develop / feature / release / hotfix) |
| Config items | Source, tests, docs, build scripts, SOUP libraries |
| Versioning | Semantic versioning (MAJOR.MINOR.PATCH) |
| Build reproducibility | Docker-based, pinned toolchain |
| Baseline | Tagged release on `main` branch |
| Change control | Gitea PR + ≥ 1 reviewer approval |

### 8.1 Supporting Items Under Control (5.1.10)

| Item | Version Control | Justification |
|------|----------------|--------------|
| Compiler (GCC/MSVC) | Pinned in Dockerfile | Reproducibility |
| CMake | Pinned version | Build consistency |
| SOUP libraries | Lock file (vcpkg.json) | Dependency tracking |
| Test framework | Pinned version | Test reproducibility |

### 8.2 Config Items Controlled Before Verification (5.1.11)

모든 software item은 unit/integration test 수행 전 Gitea에 commit되어야 한다. 테스트는 committed version에 대해서만 실행한다.

## 9. Plan Update Process (5.1.2)

본 SDP는 다음 상황에서 갱신한다:

- Phase 전환 시
- Safety classification 변경 시
- 개발 프로세스 중대 변경 시
- 외부 audit 지적사항 반영 시

변경 시 formal review + approval을 거친다.

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-03 | XPE Team | Initial release |

---

*Document End — XPE-SDP-001 v1.0*
