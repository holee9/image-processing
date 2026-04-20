# XPE 엔진 아키텍처 개요

**문서 ID**: XPE-CODEMAP-001  
**버전**: 1.1.0  
**날짜**: 2026-04-20  
**상태**: 완료  
**분류**: 내부 / 아키텍처 기준 문서

---

## 1. 아키텍처 개요

XPE(X-ray Processing Engine)는 의료 영상 장비 소프트웨어로, X-ray Flat Panel Detector(FPD)의 원시 raw 프레임을 진단 가능한 DICOM 영상으로 변환하는 모듈형 이미지 처리 엔진입니다. 3-Lane 개발 전략을 따르며 IEC 62304 Class B 규정 준수를 목표로 하는 엔터프라이즈급 솔루션입니다.

### 핵심 설계 원칙

#### 1.1 3-Lane 개발 전략

**Lane A (Preprocess)**: `dev/preprocess` 브랜치
- `modules/preprocess/` 소유권
- SIMD 최적화 알고리즘 구현
- 고속 이미지 전처리 파이프라인

**Lane B (Postprocess)**: `dev/postprocess` 브랜치  
- `modules/enhance_**/`, `modules/ai/`, `modules/display/`, `modules/dicom/`, `modules/gsvg/` 소유권
- 고급 영상 향상 및 AI 처리
- DICOM 통합 및 표준 준수

**Lane C (GUI)**: `dev/gui` 브랜치
- `clients/` 소유권
- 사용자 인터페이스 및 시각화
- P/Invoke 통합

**Main (통합)**: `main` 브랜치
- 프로젝트 루트 공유 자산 관리
- 통합 빌드 및 배포 관리
- 거버넌스 및 문서 관리

#### 1.2 모듈형 DLL 아키텍처

- DLL 간 laterl dependency 금지
- C ABI 인터페이스를 통한 안정적인 경계
- 선택적 로딩을 통한 점진적 기능 추가
- 각 DLL은 독립적으로 개발/테스트 가능
- **신규**: 전처리 파이프라인 통합 모듈 (pipeline.cpp)

#### 1.3 상태 비설계 원칙 (Stateless Design)

- 모든 처리 함수는 reentrant 특성 보장
- 호출자 할당 메모리 모델
- 스레드 안전성 기본 제공
- 상태 관리는 오케스트레이터 담당

### 1.4 시스템 계층 구조 (System Layer Architecture)

```
[Layer 3] GUI Layer (C# WPF) 
    └── ImageProcTest.exe (사용자 인터페이스)

[Layer 2] Application Layer (C# P/Invoke)
    └── NativeDependencyLoader (DLL 로딩 관리)

[Layer 1] Core Processing Layer (C++ DLL)
    ├── xpe_common (공통 런타임, 15개 C API)
    ├── xpe_preprocess (SIMD 최적화 전처리)
    ├── xpe_enhance_basic (기본 영상 향상)
    ├── xpe_enhance_advanced (고급 영상 향상, Eigen3)
    ├── xpe_display (디스플레이 처리)
    ├── xpe_ai (AI 기반 처리)
    ├── xpe_dicom (DICOM 통합, DCMTK)
    └── gsvg (독립적 벡터 그래픽)

[Layer 0] Foundation Layer
    ├── spdlog (로깅)
    ├── fmt (형식화)
    ├── nlohmann_json (JSON 처리)
    ├── OpenCV4 (컴퓨터 비전)
    ├── Eigen3 (선형 대수)
    ├── DCMTK (DICOM 통합)
    └── Google Test (테스팅 프레임워크)
```

---

## 2. 주요 아키텍처 결정

### 2.1 모듈 독립성 (Module Independence)

**원칙**: 각 XPE 모듈은 `xpe_common`에만 의존하며 다른 XPE 모듈과의 횡적 의존성을 허용하지 않음

**이점**:
- 독립적 배포 가능성
- 버전 격리 및 충돌 방지
- 모듈별 테스트 용이성
- 점진적 업그레이드 지원

### 2.2 ABI 인터페이스 (C API)

**설계**: 15개 표준 C API 함수를 통한 모듈 간 통합
- `XpeInit()`, `XpeProcess()`, `XpeDestroy()` 등
- GUI 통합을 위한 안정적인 인터페이스
- 언어에 독립적인 바이너리 호환성

### 2.3 안전 로딩 (Graceful Loading)

**메커니즘**: `NativeDependencyLoader`를 통한 DLL 존재감 확인
- P/Invoke 호출은 `try/catch`로 감싸기
- 부재하는 DLL 시 에러 없이 계속 실행
- GUI에서 준비 상태 수준 표시 (R0-R3)

### 2.4 품질 보장 (Quality Assurance)

**전략**:
- 94.17% 테스트 커버리지 (목표 95%)
- Google Test 기반 통합 테스트
- 3-Lane 별 독립적 테스트 실행기
- IEC 62304 Class B 문서화

---

## 3. 기술 스택

| 분류 | 기술 | 버전 | 목적 |
|------|------|------|------|
| **프로그래밍** | C++17 | | 고성능 영상 처리 |
| **빌드 시스템** | CMake 3.20+ | | 크로스 플랫폼 빌드 |
| **로그** | spdlog | v1.14.1 | 고성능 로깅 |
| **형식화** | fmt | v11.0.2 | 안전한 문자열 형식화 |
| **JSON** | nlohmann_json | v3.11.3 | JSON 처리 |
| **선형 대수** | Eigen3 | v3.4.0 | 고급 영상 처리 |
| **컴퓨터 비전** | OpenCV4 | v4.9.0 | 영상 처리 유틸리티 |
| **DICOM** | DCMTK | v3.6.8 | 의료 영상 표준 |
| **테스팅** | Google Test | v1.14.0 | 단위 테스트 프레임워크 |

---

## 4. 런타임 패키징

| Phase | 필수 DLL | 선택 DLL | 목적 |
|-------|----------|----------|------|
| Phase 1 | `xpe_common.dll`, `xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` | 없음 | Raw-to-DICOM 기본 경로 |
| Phase 2 | - | `xpe_enhance_advanced.dll`, `gsvg.dll` | 임상 품질 향상 기능 |
| Phase 3 | - | `xpe_ai.dll`, `xpe_ai_worker.exe` | AI 기반 프리미엄 기능 |

---

## 5. 아키텍처 장점

### 5.1 모듈성과 유지보수성

- DLL 단위로 독립 개발 가능
- 명확한 인터페이스 경계
- 상호 의존성 제거로 단위 테스트 용이
- 버전 관리 독립성

### 5.2 성능과 확장성

- C/C++으로 성능 최적화
- 선택적 로딩으로 메모리 관리
- 병렬 처리 지원
- GPU 가속 가능 (ONNX)

### 5.3 규제 준수성

- IEC 62304 Class B 준비 구조
- 추적성 확보 아키텍처
- 안전한 실패 메커니즘
- 검증 가능한 모듈 경계

---

## 6. 아키텍처 진화

### 현재 상태 (v3.0)
- 14개 스프린트 완료
- 7개 XPE 모듈 전체 구현
- IEC 62304 Class B 준수 도입
- 3-Lane 개발 전략 적용

### 차기 계획
- GSVG 모듈 독립화 완료
- 성능 벤치마크 최적화
- 클라우드 배포 지원
- AI 처리 성능 향상

---

## 7. 참고 문서

- `.moai/project/pipeline-spec.md` - 파이프라인 상세 명세
- `.moai/project/api-spec.md` - C ABI 참조 문서  
- `.moai/specs/xpe-algorithm-spec-deepsync.md` - 알고리즘 심화 명세
- `docs/post-processing/xpe/XPE-PRD-002_*.md` - 실행형 PRD

---

*최종 업데이트: 2026-04-20*