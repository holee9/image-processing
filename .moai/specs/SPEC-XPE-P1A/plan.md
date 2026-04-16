# Implementation Plan: SPEC-XPE-P1A

---
spec_id: SPEC-XPE-P1A
version: 1.0.0
status: Planned
created: 2026-04-16
updated: 2026-04-16
author: manager-spec (MoAI)
---

## 1. Implementation Overview

### 1.1 Architecture Position

```
Layer 0: xpe_common.dll (완료)
    ↓ (의존)
Layer 1: xpe_preprocess.dll (본 SPEC 구현 대상)
    ↓ (P/Invoke)
Layer 2: C# ImageProcTest GUI
```

xpe_preprocess.dll은 xpe_common.dll(Layer 0)에만 의존한다. 다른 Layer 1 DLL과는 상호 의존하지 않는다(Anti-Spaghetti 원칙).

### 1.2 Technology Stack

| Component        | Version     | Purpose                           |
|------------------|-------------|-----------------------------------|
| C++ Standard     | C++17       | 모듈 구현 언어                     |
| C ABI            | C11         | DLL export boundary               |
| CMake            | >= 3.25     | 빌드 시스템                        |
| Google Test      | 1.14.x      | 단위/통합 테스트 프레임워크         |
| spdlog           | 1.13.x      | 비동기 로깅 (xpe_common 경유)       |
| nlohmann/json    | 3.11.x      | JSON config 파싱                   |
| fmt              | 10.x        | 문자열 포매팅                      |
| AVX2 Intrinsics  | immintrin.h | SIMD 최적화                        |
| vcpkg            | manifest    | SOUP 의존성 관리                    |

### 1.3 File Structure Plan

```
modules/preprocess/
    CMakeLists.txt                              # 빌드 설정
    include/xpe/preprocess/
        xpe_preprocess_api.h                    # API 선언 (18개 함수)
    src/
        xpe_preprocess.cpp                      # Lifecycle, utility 구현
        xpe_offset_correction.cpp               # Offset correction (SWU-1.1)
        xpe_gain_correction.cpp                 # Gain correction (SWU-1.2)
        xpe_defect_correction.cpp               # Defect correction (SWU-1.3)
        xpe_calibration.cpp                     # Calibration file I/O (SUP-01)
        xpe_readout_validation.cpp              # Readout artifact validation
        simd/
            xpe_offset_avx2.cpp                 # AVX2 offset correction
            xpe_gain_avx2.cpp                   # AVX2 gain correction
            xpe_defect_avx2.cpp                 # AVX2 defect correction
            xpe_simd_dispatch.cpp               # Runtime SIMD feature detection
        detail/
            xcal_parser.h                       # XCal format parser
            xcal_parser.cpp                     # XCal format parser impl
            interpolation.h                     # Interpolation algorithms
            interpolation.cpp                   # Bilinear/median interpolation
    tests/
        CMakeLists.txt                          # 테스트 빌드 설정
        test_offset_correction.cpp              # Offset correction 테스트
        test_gain_correction.cpp                # Gain correction 테스트
        test_defect_correction.cpp              # Defect correction 테스트
        test_calibration.cpp                    # Calibration I/O 테스트
        test_simd_parity.cpp                    # Scalar vs SIMD 동등성 테스트
        test_preprocess_integration.cpp         # 파이프라인 통합 테스트
        test_readout_validation.cpp             # Readout validation 테스트
        test_data/                              # 테스트 데이터 (synthetic)
            offset_map_512x512.raw
            gain_map_512x512.raw
            defect_map_512x512.raw
            sample_dark_512x512.raw
            sample_flat_512x512.raw
```

---

## 2. Milestone Decomposition

### Milestone M1: Foundation (Priority: High)

**목표**: 모듈 스캐폴딩, 빌드 통합, API 헤더, lifecycle 함수

| Task | Description                                                | Dependency  |
|------|------------------------------------------------------------|-------------|
| M1-1 | `CMakeLists.txt` 작성 (xpe_common 링크, vcpkg 의존성)        | None        |
| M1-2 | `xpe_preprocess_api.h` 헤더 작성 (14개 함수 선언)            | None        |
| M1-3 | `xpe_preprocess.cpp` lifecycle 구현 (init/shutdown/version) | M1-1, M1-2  |
| M1-4 | 빌드 통합 테스트 (root CMakeLists.txt 인식 확인)              | M1-3        |
| M1-5 | `test_preprocess_integration.cpp` 스캐폴딩                   | M1-4        |

**산출물**: 빌드 가능한 xpe_preprocess.dll (빈 함수들), C# P/Invoke 로드 가능

### Milestone M2: Scalar Reference Implementation (Priority: High)

**목표**: SIMD 없이 순수 C++로 모든 알고리즘의 기준 구현을 완성

| Task | Description                                                         | Dependency |
|------|---------------------------------------------------------------------|------------|
| M2-1 | `xpe_offset_correction.cpp` 구현 (saturating subtraction, floor-at-zero) | M1     |
| M2-2 | `xpe_gain_correction.cpp` 구현 (per-pixel multiplication, NaN/Inf clamping) | M1 |
| M2-3 | `xpe_defect_correction.cpp` 구현 (bilinear interpolation, edge-aware) | M1         |
| M2-4 | `xpe_defect_correction.cpp` nearest/median 모드 추가                | M2-3       |
| M2-5 | `xpe_defect_detect_runtime()` transient defect detection 구현       | M2-3       |
| M2-6 | Input validation 및 dimension/format mismatch guard                | M2-1~M2-5  |

**알고리즘 참조** (XPE-ALG-001):

- Offset: `I_offset(x,y) = max(I_raw(x,y) - I_dark(x,y), 0)` (research.md line 82)
- Gain: `G(x,y) = mean(I_flat) / (I_flat(x,y) - I_dark(x,y))` (research.md line 87)
- Defect: Edge-aware bilinear interpolation (research.md line 91)

### Milestone M3: Calibration Data Management (Priority: High)

**목표**: XCal 포맷 파싱, 무결성 검증, calibration lifecycle 관리

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M3-1 | `xcal_parser.h/cpp` XCal 포맷 헤더 파싱 (magic, version, type)     | M1         |
| M3-2 | SHA-256 무결성 검증 구현                                           | M3-1       |
| M3-3 | `xpe_calib_load_offset()` 구현                                     | M3-1, M2-1 |
| M3-4 | `xpe_calib_load_gain()` 구현                                       | M3-1, M2-2 |
| M3-5 | `xpe_calib_load_defect_map()` 구현                                 | M3-1, M2-3 |
| M3-6 | `xpe_calib_generate_offset()` 다중 프레임 평균 구현                  | M2-1       |
| M3-7 | `xpe_calib_check_expiry()` 만료 확인 구현                           | M3-1       |
| M3-8 | `xpe_calib_save()` XCal 포맷 저장 구현                              | M3-1       |
| M3-9 | Session matching 로직 (offset/gain/BPM 동일 session_id 검증)        | M3-3~M3-5  |

**XCal 포맷 참조** (research.md line 105-124):
- Magic: "XCal", fields: version, type(0-5), detector_serial, session_id, timestamps, kVp, mAs, temperature, pixel_format, compression, payload_size, SHA-256 checksums

### Milestone M4: Test Infrastructure (Priority: High)

**목표**: TDD 기반 포괄적 테스트, 85% coverage 달성

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M4-1 | `test_offset_correction.cpp` 작성 (golden reference, edge cases)    | M2-1       |
| M4-2 | `test_gain_correction.cpp` 작성                                    | M2-2       |
| M4-3 | `test_defect_correction.cpp` 작성                                  | M2-3, M2-4 |
| M4-4 | `test_calibration.cpp` 작성 (load/save/validate lifecycle)          | M3         |
| M4-5 | `test_preprocess_integration.cpp` 파이프라인 통합 테스트             | M2, M3     |
| M4-6 | Synthetic test data 생성 스크립트                                   | M4-1       |
| M4-7 | SIMD parity 테스트 harness 준비 (scalar 결과를 golden reference로)  | M2         |
| M4-8 | 1000-cycle 메모리 누수 테스트 (xpe_common 패턴 참조)                | M2         |
| M4-9 | Coverage 측정 및 85% 달성 검증                                     | M4-1~M4-8  |

**테스트 패턴 참조** (research.md line 129-148):
- Golden reference datasets (synthetic test cases with known outputs)
- Edge case validation (extreme temperatures, large defects, corrupted data)
- Performance regression (continuous benchmarking against targets)
- Concurrency testing (multi-threaded access validation)

### Milestone M5: SIMD Optimization (Priority: Medium)

**목표**: AVX2 최적화, scalar와의 bit-exact parity 검증

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M5-1 | `xpe_simd_dispatch.cpp` CPUID 기반 AVX2 runtime detection          | M1         |
| M5-2 | `xpe_offset_avx2.cpp` 구현 (`_mm256_subs_epu16` 활용)              | M2-1, M5-1 |
| M5-3 | `xpe_gain_avx2.cpp` 구현 (FMA chains, uint16->float32 변환)        | M2-2, M5-1 |
| M5-4 | `xpe_defect_avx2.cpp` 구현 (SIMD gather/scatter interpolation)     | M2-3, M5-1 |
| M5-5 | `test_simd_parity.cpp` Scalar vs AVX2 bit-exact equivalence 테스트 | M5-2~M5-4  |
| M5-6 | Performance benchmark (3072x3072 목표 달성 검증)                    | M5-5       |

**SIMD 전략 참조** (research.md line 97-102):
- Saturating subtraction: `_mm256_subs_epu16`
- Type conversion: `_mm256_cvtepu16_epi32` -> `_mm256_cvtepi32_ps`
- FMA chains: polynomial evaluations in gain correction
- Parallel reductions: min/max, sum/variance calculations

### Milestone M6: Readout Validation + Utility (Priority: Low)

**목표**: Readout artifact validation, parameter range query

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M6-1 | `xpe_readout_validation.cpp` line noise/dropped column/ADC 검출    | M2         |
| M6-2 | `xpe_preprocess_get_param_range()` body-part parameter range 구현   | M1         |
| M6-3 | `test_readout_validation.cpp` 작성                                 | M6-1       |

---

## 3. API Function Signatures

api-spec.md v1.3.0 Section 6 기준, 본 SPEC 범위 14개 함수의 상세 시그니처:

### 3.1 Lifecycle

```c
// P1A 범위 커스텀 init/shutdown (xpe_common의 init과 별개)
XPE_API XpeErrorCode xpe_preprocess_init(const char* configJsonOrNull);
XPE_API void         xpe_preprocess_shutdown(void);
```

### 3.2 Correction Processing

```c
// SWU-1.1: Offset Correction (REQ-P1A-010)
XPE_API XpeErrorCode xpe_offset_correct(XpeImageBuffer* img,
                                         const XpeImageBuffer* offsetMap);

// SWU-1.2: Gain Correction (REQ-P1A-011)
XPE_API XpeErrorCode xpe_gain_correct(XpeImageBuffer* img,
                                       const XpeImageBuffer* gainMap);

// SWU-1.3: Defect Correction (REQ-P1A-012)
XPE_API XpeErrorCode xpe_defect_correct(XpeImageBuffer* img,
                                         const XpeImageBuffer* defectMap,
                                         const char* configJsonOrNull);

// SWU-1.3: Runtime Defect Detection (REQ-P1A-013)
XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* img,
                                                XpeImageBuffer* defectMapOut,
                                                const char* configJsonOrNull);
```

### 3.3 Calibration I/O

```c
// Calibration Loading (REQ-P1A-014~016)
XPE_API XpeErrorCode xpe_calib_load_offset(const char* filePath,
                                            XpeImageBuffer* offsetMapOut);
XPE_API XpeErrorCode xpe_calib_load_gain(const char* filePath,
                                          XpeImageBuffer* gainMapOut);
XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filePath,
                                                XpeImageBuffer* defectMapOut);

// Calibration Generation (REQ-P1A-017)
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* frames,
                                                uint32_t frameCount,
                                                XpeImageBuffer* offsetMapOut,
                                                const char* configJsonOrNull);

// Calibration Validation (REQ-P1A-018)
XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filePath,
                                             uint64_t* expiryEpochMsOut);

// Calibration Save (REQ-P1A-019)
XPE_API XpeErrorCode xpe_calib_save(const XpeImageBuffer* calibMap,
                                     const char* filePath,
                                     uint64_t expiryEpochMs,
                                     const char* configJsonOrNull);
```

### 3.4 Utility

```c
// Readout Artifact Validation (REQ-P1A-041)
XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* rawImg,
                                                    int32_t* artifactScoreOut,
                                                    char* msgOut,
                                                    size_t msgLen);

// Parameter Range Query (REQ-P1A-042)
XPE_API XpeErrorCode xpe_preprocess_get_param_range(const char* bodyPart,
                                                     const char* paramName,
                                                     float* minVal,
                                                     float* maxVal,
                                                     float* defaultVal);
```

---

## 4. Reference Implementations

### 4.1 xpe_common.dll Pattern (복제 대상)

**파일**: `modules/common/src/xpe_common.cpp`

| 패턴                           | 코드 위치       | 설명                                    |
|-------------------------------|----------------|----------------------------------------|
| extern "C" + XPE_API          | Line 48        | C ABI export 래핑                       |
| g_initialized flag            | Line 36        | init/shutdown 상태 관리                  |
| std::mutex + std::lock_guard  | Line 34-35     | Thread-safe singleton                   |
| nlohmann/json::parse try-catch | Line 87-91    | JSON config 검증 (C++ exception → error code) |
| xpe_test_inject_alert         | Line 171       | White-box test 지원 함수                  |

### 4.2 Pack=8 Struct Pattern (반드시 준수)

**파일**: `modules/common/include/xpe/common/xpe_types.h`

```cpp
#pragma pack(push, 8)
// struct definition
#pragma pack(pop)
static_assert(sizeof(StructName) == EXPECTED, "P/Invoke compatibility");
static_assert(offsetof(StructName, field) == OFFSET, "Field offset check");
```

### 4.3 Error Handling Pattern

```cpp
XPE_API XpeErrorCode function_name(/* params */) {
    // 1. NULL pointer checks
    if (!ptr) return XPE_ERR_INVALID_INPUT;
    // 2. Initialization check
    if (!g_initialized) return XPE_ERR_NOT_INITIALIZED;
    // 3. Dimension/format validation
    // 4. Algorithm execution (try-catch for IEC 62304)
    try {
        // processing
    } catch (const std::exception&) {
        return XPE_ERR_PROCESSING_FAILED;
    }
    return XPE_OK;
}
```

---

## 5. Technical Constraints

### 5.1 IEC 62304 Class B Compliance

| 항목              | 요구사항                                            | 검증 방법          |
|------------------|----------------------------------------------------|-------------------|
| Exception safety | C++ 예외가 C ABI 경계를 넘지 않음                    | Code review, test |
| Memory safety    | 할당/해제 쌍 보장, leak 없음                         | 1000-cycle test   |
| Thread safety    | 전역 가변 상태는 mutex 보호                           | Concurrency test  |
| Input validation | 모든 포인터/차원 검증                                 | Negative test     |
| Traceability     | REQ → SWU → DLL function → Test 1:1 매핑            | Test matrix       |

### 5.2 Dependency Constraints

- **허용 의존성**: xpe_common.dll, spdlog, nlohmann_json, fmt, Google Test
- **금지 의존성**: OpenCV (enhance_basic에서만 사용), Eigen, ONNX Runtime, DCMTK
- **이유**: 모듈 간 결합도 최소화 (Anti-Spaghetti 원칙)

### 5.3 Build Constraints

- CMake >= 3.25, C++17 표준
- Warnings as errors 옵션 지원 (`XPE_WARNINGS_AS_ERRORS`)
- vcpkg manifest mode로 SOUP 버전 고정
- BUILD_SHARED_LIBS=ON (DLL 빌드)

---

## 6. Risk Analysis

### 6.1 Technical Risks

| Risk ID | Risk                                | Probability | Impact | Mitigation                                              |
|---------|-------------------------------------|-------------|--------|---------------------------------------------------------|
| R-01    | XCal 포맷 명세 불일치                | Medium      | High   | XPE-ALG-001 공식 포맷 정의 확인, 파서 단위 테스트 강화    |
| R-02    | SIMD parity 불일치 (scalar != AVX2)  | Medium      | High   | M5-5 parity 테스트, 온라인 검증 로깅                      |
| R-03    | Performance target 미달              | Low         | High   | M5-6 benchmark, scalar fallback 보장                      |
| R-04    | P/Invoke struct alignment 불일치     | Low         | High   | static_assert 강제, C# 통합 테스트                        |
| R-05    | 대형 defect cluster 처리 한계         | Medium      | Medium | Edge-aware interpolation, cluster size limit config      |
| R-06    | Calibration file corruption          | Low         | Medium | SHA-256 무결성 검증, graceful error reporting             |

### 6.2 Schedule Risks

| Risk ID | Risk                                  | Mitigation                                  |
|---------|---------------------------------------|---------------------------------------------|
| S-01    | M2~M3 간 알고리즘 복잡도 과소평가       | Scalar reference 먼저 완성, SIMD은 별도 milestone |
| S-02    | XCal 포맷 상세 명세 누락               | M3-1에서 포맷 검증 우선, stub 데이터로 테스트     |

---

## 7. Testing Strategy

### 7.1 Test Categories

| Category              | Test Count (Est.) | Coverage Target |
|-----------------------|-------------------|-----------------|
| Unit - Offset         | 15+               | 90%             |
| Unit - Gain           | 15+               | 90%             |
| Unit - Defect         | 20+               | 90%             |
| Unit - Calibration    | 20+               | 85%             |
| Integration - Pipeline| 10+               | 80%             |
| SIMD Parity           | 15+               | 100% path       |
| Performance           | 6+                | N/A             |
| Memory Safety         | 4+                | N/A             |
| **Total**             | **105+**          | **>= 85%**      |

### 7.2 Test Data Strategy

- **Synthetic data**: 프로그래밍 방식 생성 (512x512, 1024x1024, 3072x3072)
- **Known-answer tests**: 수학적 공식으로 기대값 계산 가능한 케이스
- **Edge cases**: Zero-filled, max-valued, single-pixel, boundary pixels
- **Negative tests**: NULL input, dimension mismatch, format mismatch, expired calibration

### 7.3 SIMD Parity Harness

```
For each correction function:
  1. Generate random input image
  2. Execute scalar implementation -> result_scalar
  3. Execute AVX2 implementation -> result_avx2
  4. Assert bit-exact equality: result_scalar == result_avx2
  5. Repeat for 100 random inputs
```

---

## 8. Milestone Execution Order

```
M1 (Foundation)
    ↓
M2 (Scalar Reference) ← TDD RED-GREEN-REFACTOR
    ↓
M4 (Test Infrastructure) ← M2와 병행, TDD 사이클 내
    ↓
M3 (Calibration Management) ← M2 이후, calibration lifecycle
    ↓
M5 (SIMD Optimization) ← Scalar 검증 후
    ↓
M6 (Readout Validation + Utility) ← 마지막
```

**참고**: TDD 방법론에 따라 M2의 각 task는 RED(실패 테스트 작성) -> GREEN(최소 구현) -> REFACTOR(개선) 사이클로 수행한다. M4는 M2와 병행하여 수행된다.

---

## 9. Acceptance Criteria Summary

| #  | Criterion                                       | Verification Method        |
|----|-------------------------------------------------|----------------------------|
| A1 | 14개 함수 모두 빌드 및 export 확인                | `dumpbin /exports`         |
| A2 | Scalar 구현이 golden reference와 일치             | Unit test                  |
| A3 | AVX2 구현이 scalar와 bit-exact 동일              | Parity test                |
| A4 | Performance target 달성 (< 55ms offset/gain)     | Benchmark test             |
| A5 | Test coverage >= 85%                             | gcov/llvm-cov              |
| A6 | P/Invoke 호환 (C#에서 DLL 로드 성공)             | Integration test           |
| A7 | IEC 62304 Class B 준수 (no exceptions, no leaks) | Code review + 1000-cycle   |
| A8 | XCal 파일 load/save round-trip 성공              | Integration test           |

---

*Document End - SPEC-XPE-P1A Plan v1.0.0*
