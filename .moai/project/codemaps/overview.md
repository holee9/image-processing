# XPE 아키텍처 개요

**문서 ID**: XPE-CODEMAP-001  
**버전**: 1.0.0  
**날짜**: 2026-04-17  
**상태**: 작성 중  
**분류**: 내부 / 아키텍처 기준 문서

---

## 1. 아키텍처 개요

XPE(X-ray Processing Engine)는 의료 장비 소프트웨어로, X선 플랫 판 디텍터(FPD)의 원시 raw 프레임을 진단 가능한 DICOM 영상으로 변환하는 모듈형 이미지 처리 엔진입니다. 최신 업데이트로 고스트 보정 Tier 1/2/3 구현과 전처리 파이프라인 통합이 추가되었습니다.

### 핵심 설계 원칙

#### 1.1 3-Layer Anti-Spaghetti Architecture

- **Layer 0**: `xpe_common.dll` - 공통 기능 기반 계층
- **Layer 1**: 7개 알고리즘 DLL - 독립적 계산 모듈  
- **Layer 1-G**: `gsvg.dll` - 독립 IEC 62304 패키지
- **Layer 2**: `ImageProcTest` - C# WPF GUI 오케스트레이터

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

---

## 2. 시스템 경계

### 2.1 계층 구조

```
┌─────────────────────────────────────────────────────────────┐
│                    Layer 2: 오케스트레이터                     │
│                    (ImageProcTest - C# WPF)                  │
├─────────────────────────────────────────────────────────────┤
│  Layer 1-G: gsvg.dll (독립 패키지)                           │
│  Layer 1: 알고리즘 DLL 7개 (Phase 1/2/3 분할 로딩)             │
│  Layer 0: xpe_common.dll (기반 계층)                         │
├─────────────────────────────────────────────────────────────┤
│               외부 의존성 (OpenCV, DCMTK, ONNX 등)            │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 런타임 패키징

| Phase | 필수 DLL | 선택 DLL | 목적 |
|-------|----------|----------|------|
| Phase 1 | `xpe_common.dll`, `xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` | 없음 | Raw-to-DICOM 기본 경로 |
| Phase 2 | - | `xpe_enhance_advanced.dll`, `gsvg.dll` | 임상 품질 향상 기능 |
| Phase 3 | - | `xpe_ai.dll`, `xpe_ai_worker.exe` | AI 기반 프리미엄 기능 |

---

## 3. 주요 설계 패턴

### 3.1 C ABI 경계 패턴

```c
// 안정적인 ABI 경계
typedef struct XpeImageBuffer {
    uint32_t width;
    uint32_t height;
    XpePixelFormat format;
    void* data;
} XpeImageBuffer;

// 모든 함수는 __cdecl 호출 규칙
XPE_API XpeErrorCode xpe_process(XpeImageBuffer* img);

// **신규**: 전처리 파이프라인 통합 API
XPE_API XpeErrorCode xpe_preprocess_pipeline(XpeImageBuffer* img,
                                             XpeImageMetadata* meta,
                                             const char* calibPath,
                                             void* ghostHandle,
                                             const char* configJsonOrNull);
```

### 3.2 메모리 관리 패턴

```c
// 호출자 할당 모델
XpeImageBuffer img;
xpe_alloc_image(width, height, format, &img);  // DLL 할당
// ... 처리 ...
xpe_free_image(&img);  // 호출자 해제

// **신규**: 파이프라인 통합 시 메모리 흐름
// Stage 0.5~4: 단일 버퍼에서 모든 처리 수행
// 형식 변환: uint16 -> float32 (단방향 흐름)
```

### 3.3 선택적 로딩 패턴

```csharp
// C# 오케스트레이터에서 선택적 DLL 로딩
try {
    var ai = NativeLibrary.Load("xpe_ai.dll");
    phase3Available = true;
}
catch {
    phase3Available = false; // 그레이스풀 다운그레이드
}
```

---

## 4. 기술 스택

### 4.1 언어 및 플랫폼

| 계층 | 언어 | 플랫폼 | 목적 |
|------|------|--------|------|
| 알고리즘 DLL | C/C++17 | Windows DLL | 성능 최적화 연산 |
| 오케스트레이터 | C# 8.0 | .NET WPF | GUI, 제어, 통합 |
| 외부 의존 | 다양 | 크로스 플랫폼 | 표준 라이브러리 |
| **신규**: 파이프라인 모듈 | C++17 | Windows DLL | **전처리 단계 통합 (0.5-4)** |

### 4.2 핵심 의존성

| 의존성 | 용도 | 라이선스 |
|--------|------|----------|
| OpenCV 이미지 처리 | 이미지 연산 | Apache 2.0 |
| DCMTK | DICOM 처리 | GPL 3.0 (상업용 라이선스) |
| ONNX Runtime | AI 추론 | MIT |
| Eigen | 선형 대수 | MPL 2.0 |
| vcpkg | 패키지 관리 | MIT |

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

## 6. 품질 보증

### 6.1 테스트 전략

| 계층 | 테스트 방법 | 커버리지 목표 |
|------|-------------|---------------|
| DLL 단위 | Google Test | Statement 90%, Branch 80% |
| 통합 테스트 | ImageProcTest | End-to-end 검증 |
| 성능 테스트 | 벤치마크 프레임워크 | 3000ms/frame 이내 |

### 6.2 검증 점검점

- ABI 안정성 검증
- 메모리 누수 테스트
- 스레드 안전성 검증
- 예외 처리 검증

---

## 7. 미래 확장성

### 7.1 예상 확장 방향

1. **GPU 가속**: CUDA/Metal 지원 추가
2. **클라우드 통합**: 원격 처리 모듈
3. **실시간 처리**: 스트리밍 처리 지원
4. **AI 모델**: 추가 AI 기능 확장

### 7.2 아키텍처 적응성

- 모듈 추가 시 기존 API 변경 최소화
- 새로운 처리 스테이지 통합 용이
- 하드웨어 가속 플러그인 아키텍처
- 다중 플랫폼 지원 가능성

---

## 8. 참고 문서

- `.moai/project/pipeline-spec.md` - 파이프라인 상세 명세
- `.moai/project/api-spec.md` - C ABI 참조 문서  
- `.moai/specs/xpe-algorithm-spec-deepsync.md` - 알고리즘 심화 명세
- `docs/post-processing/xpe/XPE-PRD-002_*.md` - 실행형 PRD

---

*최종 업데이트: 2026-04-17*