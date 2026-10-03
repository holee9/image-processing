# Implementation Plan: SPEC-XPE-P1A

> **수치를 읽는 법 (2026-09-17, QA-A-85 전수).** 이 파일에는 **유도 근거를 찾지 못한 성능 목표가 1줄** 있습니다(55/15 · 95/30 · 500/100 ms 계열 등). 탐색 범위: `.moai/reports/lane-pre/`, `.moai/specs/SPEC-XPE-P1A/`, `docs/` 전체. **기계도 적혀 있지 않습니다.** 성능 판정의 근거로 인용하지 마십시오 — 현행 목표는 `spec.md` Performance 절입니다. 줄 목록은 QA-A-85 보고서에 있습니다.
>
> 측정값을 문서에 적을 때는 **측정일·명령·기계**를 함께 적습니다(`lane-sessions.md` §3.5.5).


---
spec_id: SPEC-XPE-P1A
version: 1.3.0
status: M2 Complete (SUP-01 + M2 algorithms implemented)
created: 2026-04-16
updated: 2026-10-03
author: manager-spec (MoAI)
---

## HISTORY

| Version | Date       | Author       | Changes |
|---------|------------|--------------|---------|
| 1.3.0   | 2026-10-03 | xpe-leader | #245: QA-A-228 정합 대조 반영(D14·B-1~B-6·A-4). frontmatter 상태를 `spec.md` 와 같은 문장으로 맞춤(옛 값 "In Progress (SUP-01 complete, M2 pending)" 은 2026-04-19 M2 완료와 모순). §1.3 파일 구조를 실제 파일 이름으로 정정(계획 이름 24개 중 23개가 존재하지 않음), 작업표에 REQ 열 추가(리더 추정 매핑), M1-2 함수 수 정정, M2-3/M2-4 문구 정정, M4-9·M1-5·M5-6 상태 기록. M3-9 세션 일치는 상태 주석만(구현 예정 — QA-A-229 M3, #245). |
| 1.2.0   | 2026-04-18 | manager-spec (Pre Lane upgrade) | Align with spec.md v1.2.0. Cross-reference simd-parity-harness.md and benchmark manifest. M2 scope strengthened with Hampel runtime detection recipe. |
| 1.0.0   | 2026-04-16 | manager-spec | Initial implementation plan |

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

> **정정 2026-10-03** (`#245` / `QA-A-228` B-2·D14): 이 블록이 적던 계획 이름 24개 중 실제로 있는 것은 `xpe_calibration.cpp` 하나였다. 아래는 실제 파일 이름이고, 각 줄 끝 주석에 계획 때의 이름을 남겼다. 전체 목록이 아니라 계획 항목에 대응하는 파일만 적는다.

```
modules/preprocess/
    CMakeLists.txt                              # 빌드 설정
    include/xpe/
        preprocess_api.h                        # API 선언 (XPE_API 선언 48개; 계획: preprocess/xpe_preprocess_api.h, "18개 함수")
    src/
        preprocess.cpp                          # Lifecycle, utility 구현 (계획: xpe_preprocess.cpp)
        offset_correct.cpp                      # Offset correction, AVX2 커널 인라인 (SWU-1.1; 계획: xpe_offset_correction.cpp)
        gain_correct.cpp                        # Gain correction, AVX2 커널 인라인 (SWU-1.2; 계획: xpe_gain_correction.cpp)
        defect_correct.cpp                      # Defect correction, AVX2 없음 (SWU-1.3; 계획: xpe_defect_correction.cpp)
        xpe_calibration.cpp                     # Calibration file I/O (SUP-01) — 계획 이름 그대로 존재
        readout_validate.cpp                    # Readout artifact validation (계획: xpe_readout_validation.cpp)
        simd/                                   # PLANNED, NOT BUILT — see the M5 note below
            xpe_offset_avx2.cpp                 # never created; kernel lives inline in offset_correct.cpp
            xpe_gain_avx2.cpp                   # never created; kernel lives inline in gain_correct.cpp
            xpe_defect_avx2.cpp                 # never created; defect correction has no AVX2 path
            xpe_simd_dispatch.cpp               # never created; the nearest thing, simd_dispatch.cpp, was deleted in QA-A-76
        xcal_reader.cpp / xcal_validator.cpp / xcal_writer.cpp   # XCal 포맷 (계획: detail/xcal_parser.h/.cpp)
        helpers.cpp                             # 결함 보간 도우미 (계획: detail/interpolation.h/.cpp)
    tests/
        CMakeLists.txt                          # 테스트 빌드 설정
        test_offset_correct.cpp                 # Offset correction 테스트 (계획: test_offset_correction.cpp)
        test_gain_correct.cpp                   # Gain correction 테스트 (계획: test_gain_correction.cpp)
        test_defect_correct.cpp                 # Defect correction 테스트 (계획: test_defect_correction.cpp)
        test_calibration_manager.cpp 외         # Calibration I/O 테스트 — test_calibration_roundtrip.cpp, test_xpe_calib_*.cpp 등 (계획: test_calibration.cpp)
        test_*_avx2_parity.cpp                  # Scalar vs SIMD 동등성 테스트 (계획: test_simd_parity.cpp — M5 정정 참조)
        test_integration.cpp                    # 파이프라인 통합 테스트 (계획: test_preprocess_integration.cpp)
        test_readout_validate.cpp               # Readout validation 테스트 (계획: test_readout_validation.cpp)
        (test_data/ 없음)                       # 계획한 .raw 고정 데이터는 만들어진 적이 없다 — 시험이 합성 데이터를 직접 만든다 (tests/fixtures/ 는 XCal 생성 도우미 헤더뿐)
```

---

## 2. Milestone Decomposition

### Milestone M1: Foundation (Priority: High)

**목표**: 모듈 스캐폴딩, 빌드 통합, API 헤더, lifecycle 함수

| Task | Description                                                | Dependency  |
|------|------------------------------------------------------------|-------------|
| M1-1 | `CMakeLists.txt` 작성 (xpe_common 링크, vcpkg 의존성)        | None        |
| M1-2 | `include/xpe/preprocess_api.h` 헤더 작성 — 현재 `XPE_API` 선언 48개 *(정정 2026-10-03, `#245` / `QA-A-228`: 옛 문구는 `xpe_preprocess_api.h`, "14개 함수 선언")* | None        |
| M1-3 | `xpe_preprocess.cpp` lifecycle 구현 (init/shutdown/version) | M1-1, M1-2  |
| M1-4 | 빌드 통합 테스트 (root CMakeLists.txt 인식 확인)              | M1-3        |
| M1-5 | `test_integration.cpp` 스캐폴딩 — **상태(2026-10-03, `QA-A-228` B-6): 파이프라인 500 ms 성능 시험 `DISABLED_PipelinePerformance3072x3072` 은 꺼져 있다(#245)** | M1-4        |

**산출물**: 빌드 가능한 xpe_preprocess.dll (빈 함수들), C# P/Invoke 로드 가능

### Milestone M2: Scalar Reference Implementation (Priority: High)

**목표**: SIMD 없이 순수 C++로 모든 알고리즘의 기준 구현을 완성

> **REQ 열 추가 2026-10-03** (`#245` / `QA-A-228` B-1): 이전에는 작업 행 38개 중 REQ 를 적은 행이 0개였다. M2·M3·M6 표의 REQ 열은 `QA-A-228` §1 의 **추정 매핑**이다. 파일 이름은 §1.3 의 실제 이름으로 고쳤다.

| Task | REQ | Description                                                         | Dependency |
|------|-----|---------------------------------------------------------------------|------------|
| M2-1 | 010 | `offset_correct.cpp` 구현 (floor-at-zero offset subtraction) | M1     |
| M2-2 | 011 | `gain_correct.cpp` 구현 (per-pixel multiplication, NaN/Inf clamping) | M1 |
| M2-3 | 012 | `defect_correct.cpp` 구현 — 고립 결함은 유효한 4이웃 평균(없으면 체비셰프 링 r=1..3), 군집은 3×3 중앙값 *(정정 2026-10-03, `QA-A-228` B-4: 옛 문구 "bilinear interpolation, edge-aware")* | M1         |
| M2-4 | 012 | ~~`xpe_defect_correction.cpp` nearest/median 모드 추가~~ — **삭제 2026-10-03** (`QA-A-228` B-4): 선택 가능한 모드는 없다. 군집만 중앙값을 쓰고, `REQ-P1A-012` 는 호출 단위 설정을 받지 않는다 | M2-3       |
| M2-5 | 013 | `xpe_defect_detect_runtime()` transient defect detection 구현       | M2-3       |
| M2-6 | 005·021·022 | Input validation 및 dimension/format mismatch guard                | M2-1~M2-5  |

**알고리즘 참조** (XPE-ALG-001):

- Offset: `I_offset(x,y) = max(I_raw(x,y) - I_dark(x,y), 0)` (research.md line 82)
- Gain: `G(x,y) = mean(I_flat) / (I_flat(x,y) - I_dark(x,y))` (research.md line 87)
- Defect: ~~Edge-aware bilinear interpolation~~ (research.md line 91) — 실제 구현은 4이웃 평균 + 군집 3×3 중앙값(`spec.md` `REQ-P1A-012`, 정정 2026-10-03 `QA-A-228` B-4)

### Milestone M3: Calibration Data Management (Priority: High)

**목표**: XCal 포맷 파싱, 무결성 검증, calibration lifecycle 관리

| Task | REQ | Description                                                        | Dependency |
|------|-----|--------------------------------------------------------------------|------------|
| M3-1 | — | `xcal_reader.cpp`·`xcal_validator.cpp`·`xcal_writer.cpp` XCal 포맷 헤더 파싱 (magic, version, type) *(계획 이름: `xcal_parser.h/cpp`)* | M1         |
| M3-2 | — | SHA-256 무결성 검증 구현                                           | M3-1       |
| M3-3 | 014 | `xpe_calib_load_offset()` 구현                                     | M3-1, M2-1 |
| M3-4 | 015 | `xpe_calib_load_gain()` 구현                                       | M3-1, M2-2 |
| M3-5 | 016 | `xpe_calib_load_defect_map()` 구현                                 | M3-1, M2-3 |
| M3-6 | 017 | `xpe_calib_generate_offset()` 다중 프레임 평균 구현                  | M2-1       |
| M3-7 | 018 | `xpe_calib_check_expiry()` 만료 확인 구현                           | M3-1       |
| M3-8 | 019 | `xpe_calib_save()` XCal 포맷 저장 구현                              | M3-1       |
| M3-9 | 014 | Session matching 로직 (offset/gain/BPM 동일 session_id 검증) — **상태(2026-10-03, `QA-A-228` B-3·D1): 구현 없음, 구현 예정 — QA-A-229 M3, #245** | M3-3~M3-5  |

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
| M4-9 | Coverage 측정 및 85% 달성 검증 — **상태(2026-10-03, `QA-A-228` B-5): 미측정.** preprocess 의 측정값을 찾지 못했다(QA-A-08: 커버리지 타깃 없음) | M4-1~M4-8  |

**테스트 패턴 참조** (research.md line 129-148):
- Golden reference datasets (synthetic test cases with known outputs)
- Edge case validation (extreme temperatures, large defects, corrupted data)
- Performance regression (continuous benchmarking against targets)
- Concurrency testing (multi-threaded access validation)

### Milestone M5: SIMD Optimization (Priority: Medium)

**목표**: AVX2 최적화, scalar와의 bit-exact parity 검증

| Task | Description                                                        | Dependency |
|------|--------------------------------------------------------------------|------------|
| M5-1 | ~~`xpe_simd_dispatch.cpp` CPUID 기반 AVX2 runtime detection~~ **철회** | M1         |
| M5-2 | ~~`xpe_offset_avx2.cpp` 구현 (`_mm256_subs_epu16` 활용)~~ **설계 변경** | M2-1, M5-1 |
| M5-3 | ~~`xpe_gain_avx2.cpp` 구현~~ **설계 변경** — 커널은 `gain_correct.cpp` 안에 | M2-2, M5-1 |
| M5-4 | ~~`xpe_defect_avx2.cpp` 구현~~ **설계 변경** — 커널은 `defect_correct.cpp` 안에 | M2-3, M5-1 |
| M5-5 | ~~`test_simd_parity.cpp`~~ → `test_*_avx2_parity.cpp` 4개로 대체 (20 TEST) | M5-2~M5-4  |

> **M5 정정 2026-09-16 (QA-A-75, #160).** 이 표는 출하된 설계가 아니라 **옛 설계**를 적고 있었고,
> 그 사실이 코드 쪽에 흔적을 남겼습니다.
>
> - **M5-1 은 철회됩니다.** §4.6 이 AVX2 를 최소 플랫폼으로 확정했으므로(사용자 결정 2026-09-16)
>   런타임 검출이 고를 것이 없습니다. `modules/preprocess/src/simd_dispatch.cpp` 는 CMake 소스
>   목록에 없고 **컴파일되지 않습니다**(`XPE_EXPORT` 정의가 저장소에 0건, 첫 사용에서 `error C2143`).
> - **M5-2~M5-4 는 설계가 바뀌었습니다.** AVX2 커널은 별도 `simd/` 파일이 아니라 각 보정 `.cpp`
>   안에 인라인으로 있습니다. 계획된 `simd/` 디렉터리는 존재한 적이 없습니다.
> - **M5-2 의 `_mm256_subs_epu16` 이 죽은 커널 넷의 출처였습니다.** 그 명령을 쓰던
>   `offset_correct_avx2` 와 그 스칼라·AVX-512·NEON 짝은 호출자가 0인 채 남아 있다가
>   QA-A-74 에서 제거됐습니다. 그것들은 버려진 코드가 아니라 **이 문서가 아직 적고 있던 설계의
>   구현체**였습니다 — 문서가 옛 설계를 계속 말하면, 그 설계의 잔해를 지워도 될지 판단하기가
>   어려워집니다. 이 정정이 그 연결을 끊습니다.
> - **M5-5 는 다른 형태로 달성됐습니다.** 단일 하네스 대신 연산별 파리티 파일 4개이고,
>   런타임 스위치 없이 **같은 소스에서 컴파일된 인라인 스칼라 기준**과 비교합니다.
>   케이스 수는 계획과 다릅니다(계획 100입력x4 → 실제 TEST 20개, 전 프레임 비교).
| M5-6 | Performance benchmark (3072x3072 목표 달성 검증) — **상태(2026-10-03, `QA-A-228` B-6): 파이프라인 시험 `DISABLED_PipelinePerformance3072x3072` 꺼짐(#245).** 검출만 절대 400 ms 게이트가 있고 보정 세 함수에는 회귀 게이트가 없다 | M5-5       |

**SIMD 전략 참조** (research.md line 97-102):
- Saturating subtraction: `_mm256_subs_epu16`
- Type conversion: `_mm256_cvtepu16_epi32` -> `_mm256_cvtepi32_ps`
- FMA chains: polynomial evaluations in gain correction
- Parallel reductions: min/max, sum/variance calculations

### Milestone M6: Readout Validation + Utility (Priority: Low)

**목표**: Readout artifact validation, parameter range query

| Task | REQ | Description                                                        | Dependency |
|------|-----|--------------------------------------------------------------------|------------|
| M6-1 | 041 | `readout_validate.cpp` line noise/dropped column/ADC 검출 — 상태: 선 잡음 미구현(#232) | M2         |
| M6-2 | 042 | `xpe_preprocess_get_param_range()` body-part parameter range 구현 — 상태: 부위별 한계 미구현(#245) | M1         |
| M6-3 | 041 | `test_readout_validate.cpp` 작성                                 | M6-1       |

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

See `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md` v1.0.0 for the complete protocol. Summary:

- Deterministic RNG seed: CRC32("XPE-SIMD-PARITY-v1") derived per operation
- 100 random inputs per (operation, shape) pair — 3 shapes (512, 1024, 3072) × 6 operations = 1800 cases
- Plus 30 edge-case tests (zero, max, hotspot, checkerboard, ramp) per operation
- Parity rules:
  - Integer operations (Offset, Defect, Runtime Detect): **bit-identical** via memcmp
  - Float operations (Gain reciprocal, Gain polynomial): **1 ULP** tolerance via `fabsf(s-a) <= ULP(max(|s|,|a|))`
- Dispatch override: `XPE_FORCE_SCALAR=1` env or `{"force_scalar": true}` config
- Harness location: `modules/preprocess/tests/test_simd_parity.cpp` (new file, M5 deliverable)

### 7.4 Pixel-Accuracy Benchmarks (BP-01~05)

See `benchmark/BP-01-05-preprocess-manifest.md` v1.0.0. Pre Lane M2 release gate requires:

- BP-01 Temperature sweep dataset captured and passing (REQ-P1A-010)
- BP-02 Multi-gain linearity dataset captured and passing (REQ-P1A-011)
- BP-03 Heel-effect SID dataset captured and passing (REQ-P1A-011)
- BP-04 Defect density dataset captured and passing (REQ-P1A-012, 013)
- ~~BP-SIMD addendum (1830/1830 parity) passing (REQ-P1A-040)~~ — **철회 2026-09-17 (QA-A-85).** `acceptance.md` 에서는 먼저 걷어냈는데 이 줄이 남아 있었습니다. `1830` 은 존재한 적 없는 하네스의 수치입니다. 현재 파리티는 `test_*_avx2_parity.cpp` 4개 파일, TEST 20건입니다.

Freeze protocol: SHA-256 hashes locked before release; any replacement requires version bump per parent spec Section 7.

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

*Document End - SPEC-XPE-P1A Plan v1.3.0*
