# GSVG-SDP-001: Software Development Plan

**Document ID:** GSVG-SDP-001  
**Version:** 1.0 | **Date:** 2026-04-03  
**IEC 62304 Clause:** 5.1  
**Safety Classification:** Class B

---

## 1. Purpose & Scope

본 문서는 X-ray FPD 시스템의 Grid Suppression(GS) 및 Virtual Grid(VG) 소프트웨어 모듈 개발에 대한 IEC 62304 Class B 준수 개발 계획을 정의한다.

**소프트웨어 명칭:** GSVG (Grid Suppression & Virtual Grid)  
**Intended Use:** X-ray 진단 영상의 grid artifact 제거 및 gridless 촬영 시 scatter radiation 소프트웨어 보정  
**운영 환경:** RadiConsole™ 진단 콘솔 내 영상처리 모듈로 통합

---

## 2. Software Safety Classification

**Class B** — 소프트웨어 오동작 시 non-serious injury 가능.

| 항목 | 분석 |
|------|------|
| GS 실패 | Grid artifact 잔류 → 진단 정보 일부 가려짐 → 재촬영 필요 |
| VG 실패 | Scatter 미보정 → contrast 저하 → 미세 병변 가시성 감소 |
| 방사선 위험 | 없음 — 영상 후처리 전용, 촬영 파라미터 제어 불가 |
| 하드웨어 mitigation | Exposure interlock이 독립적으로 존재 |
| **결론** | Non-serious injury possible → **Class B** |

---

## 3. Lifecycle Model

V-Model 적용.

```mermaid
graph LR
    A[System Requirements] --> B[Software Requirements<br/>GSVG-SRS-001]
    B --> C[Architecture Design<br/>GSVG-SAD-001]
    C --> D[Detailed Design<br/>GSVG-SDD-001]
    D --> E[Implementation]
    E --> F[Unit Testing]
    F --> G[Integration Testing]
    G --> H[System Testing]
    H --> I[Release]
    
    B -.->|verified by| H
    C -.->|verified by| G
    D -.->|verified by| F
```

---

## 4. Class B Required Activities

```mermaid
graph TD
    subgraph "Mandatory (Class B)"
        P[5.1 Development Planning ✓]
        R[5.2 Requirements Analysis ✓]
        A[5.3 Architecture Design ✓]
        INT[5.5 Integration Testing ✓]
        SYS[5.7 System Testing ✓]
        REL[5.8 Release ✓]
    end
    
    subgraph "Voluntary (applied for quality)"
        DD[5.4 Detailed Design]
        UT[5.5 Unit Implementation & Verification]
    end
    
    P --> R --> A --> DD --> UT --> INT --> SYS --> REL
```

---

## 5. Development Tools & Environment

| Category | Tool | Version | Purpose |
|----------|------|---------|---------|
| Language | C++ | C++17 | Core algorithm |
| Language | Python | 3.11+ | Prototyping, test automation |
| Build | CMake | 3.25+ | Cross-platform build |
| VCS | Git / Gitea | Latest | Configuration management |
| CI/CD | Gitea Actions | Latest | Automated build & test |
| Testing | Google Test | 1.14+ | Unit & integration testing |
| Testing | pytest | 8.0+ | Python test automation |
| Static Analysis | cppcheck, clang-tidy | Latest | Code quality enforcement |
| Coverage | gcov + lcov | Latest | Code coverage measurement |
| Memory | Valgrind | Latest | Memory leak detection |
| Image Processing | OpenCV 4.9 | SOUP | Image I/O, basic ops |
| FFT | FFTW3 3.3.10 | SOUP | Frequency domain ops |
| Math | Eigen 3.4 | SOUP | Linear algebra |
| DICOM | DCMTK 3.6.8 | SOUP | DICOM read/write |

---

## 6. Configuration Management

| 항목 | 정책 |
|------|------|
| Branch strategy | `main` (release) / `develop` (integration) / `feature/*` (개발) |
| Commit convention | Conventional Commits: `feat:`, `fix:`, `test:`, `docs:` |
| Versioning | Semantic versioning `vMAJOR.MINOR.PATCH` |
| Code review | `develop` merge 시 1+ reviewer approval 필수 |
| Baseline | 각 milestone에서 Git tag 생성 + 문서 동결 |
| Build reproducibility | `CMakeLists.txt` + dependency lock file로 재현 가능 |

---

## 7. Problem Resolution Process

- **추적 도구:** Gitea Issues
- **Severity:** Critical / Major / Minor / Cosmetic
- **Safety-related issue:** GSVG-SHA-001과 cross-reference 필수
- **미해결 anomaly:** Release 시 잔여 위험으로 문서화 (GSVG-SVP-001 Appendix)
- **Regression:** 모든 fix에 대해 regression test suite 실행

---

## 8. Schedule

```mermaid
gantt
    title GSVG Development Schedule
    dateFormat  YYYY-MM-DD
    
    section Phase 1: Grid Suppression
    SRS & Architecture           :p1a, 2026-04-07, 14d
    Core Algorithm (DWT+BandStop):p1b, after p1a, 21d
    Unit & Integration Test      :p1c, after p1b, 14d
    System Test & Validation     :p1d, after p1c, 7d
    
    section Phase 2: Virtual Grid
    SRS & Architecture           :p2a, 2026-04-14, 14d
    Scatter Model Implementation :p2b, after p2a, 28d
    Laplacian Pyramid Pipeline   :p2c, after p2b, 14d
    Unit & Integration Test      :p2d, after p2c, 14d
    System Test & Validation     :p2e, after p2d, 7d
    
    section Phase 3: Integration & Release
    Combined Pipeline            :p3a, after p2e, 14d
    Final System Test            :p3b, after p3a, 7d
    Release Documentation        :p3c, after p3b, 7d
    Release                      :milestone, after p3c, 0d
```

---

## 9. Deliverables Checklist

| Deliverable | Document ID | Phase |
|-------------|-------------|-------|
| Software Development Plan | GSVG-SDP-001 (본 문서) | Phase 1 |
| Software Requirements Specification | GSVG-SRS-001 | Phase 1 |
| Software Architecture Design | GSVG-SAD-001 | Phase 1 |
| Software Detailed Design | GSVG-SDD-001 | Phase 1–2 |
| SOUP Analysis | GSVG-SOUP-001 | Phase 1 |
| Software Hazard Analysis | GSVG-SHA-001 | Phase 1 |
| Software Verification Plan & Records | GSVG-SVP-001 | Phase 1–3 |
| Requirements Traceability Matrix | GSVG-RTM-001 | Phase 3 |
| Release Notes | GSVG-REL-001 | Phase 3 |

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-04-03 | — | Initial release |
