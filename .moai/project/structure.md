# 프로젝트 구조

## 저장소 레이아웃

```
image-processing/
├── CMakeLists.txt                 # Root CMake (C++17, Ninja)
├── CMakePresets.json              # Debug/Release/CI presets
├── cmake/                         # CMake helper modules
│   ├── CompilerWarnings.cmake
│   ├── Platform.cmake             # AVX2 detection
│   └── DependencyRules.cmake      # lateral dep 검증
│
├── modules/                       # C/C++ 네이티브 DLL 모듈 (XPE)
│   ├── common/                    → xpe_common.dll (Layer 0)
│   ├── preprocess/                → xpe_preprocess.dll (Layer 1)
│   ├── enhance_basic/             → xpe_enhance_basic.dll (Layer 1)
│   ├── enhance_advanced/          → xpe_enhance_advanced.dll (Layer 1, Phase 2)
│   ├── ai/                        → xpe_ai.dll (Layer 1, Phase 3)
│   ├── display/                   → xpe_display.dll (Layer 1)
│   └── dicom/                     → xpe_dicom.dll (Layer 1)
│
├── gsvg/                          → gsvg.dll (Layer 1-G, 독립)
│
├── tests/                         # Google Test (SWU ID 기반 파일명)
│   ├── unit/                      # 단위 테스트
│   ├── integration/               # 통합 테스트
│   └── test_data/                 # Phantom 이미지, 보정 데이터
│
├── clients/                       # C# 클라이언트 응용프로그램
│   ├── ImageProcTest/             # Main WPF application
│   ├── ImageProcTest.IntegrationTests/ # xUnit 통합 테스트 (NEW, SPEC-XPE-GUI-IT)
│   │   ├── Fixtures/              # NativeLibraryFixture, DllStagingFixture
│   │   ├── Smoke/                 # AbiLayoutTests, DllResolutionTests
│   │   ├── Functional/            # LifecycleTests, ConfigureTests, MemoryTests, etc.
│   │   ├── Safety/                # LeakEnduranceTests, NoManagedExceptionTests
│   │   ├── ErrorMapping/          # EnumParityTests
│   │   ├── Optional/              # PreprocessOptionalTests, SyntheticAdapterChainTests
│   │   ├── Resources/             # expected-versions.json, requirement-matrix.json
│   │   └── PInvoke/               # XpeCommonNative.cs (P/Invoke wrapper)
│   └── ImageProcTest.slnx         # Modern solution format
│
├── third_party/                   # Third-party libraries (vendored + SOUP)
│   ├── vcpkg.json                 # SOUP 의존성 매니페스트
│   └── picosha2/                  # PicoSHA2 (header-only, MIT-0, NEW)
│       └── picosha2.h             # SHA-256 implementation for XCal integrity
│
├── scripts/                       # 빌드/검증 스크립트
├── data/                          # 런타임 데이터 (config, LUT, ONNX models)
│
├── docs/                          # IEC 62304 규정 문서 (기존)
│   ├── ghost-correction/          # Lag/Ghost 보정 SRS/SAD/SDD
│   ├── panel-defect-algorithm/    # 불량 픽셀 보정
│   ├── post-processing/gsvg/      # GSVG IEC 62304 Class B 패키지
│   ├── post-processing/xpe/       # XPE IEC 62304 Class B 패키지
│   └── xray-fpd-research/         # 연구/캘리브레이션
│
├── .moai/                         # MoAI 프레임워크 설정 및 관리
│   ├── specs/                     # SPEC 문서들
│   ├── project/                   # 프로젝트 문서
│   ├── config/                    # MoAI 설정
│   └── agents/                    # 에이전트 정의
│
├── CLAUDE.md                      # MoAI 실행 지침
├── AGENTS.md                      # 저장소 가이드라인
└── README.md                      # 프로젝트 개요
```

## 모듈-DLL 매핑

| 디렉토리 | 출력 | 레이어 | 단계 | SWU/SI 개수 |
|----------|------|--------|------|-------------|
| modules/common/ | xpe_common.dll | 0 | 0 | 7 (SWU-5.1~5.6, SWU-5.8) |
| modules/preprocess/ | xpe_preprocess.dll | 1 | 1a | 9 (SWU-1.1~1.9) |
| modules/enhance_basic/ | xpe_enhance_basic.dll | 1 | 1b | 4 (SWU-2.1~2.4) |
| modules/enhance_advanced/ | xpe_enhance_advanced.dll | 1 | 2 | 4 (SWU-2.5,2.6,2.8,2.10) |
| modules/ai/ | xpe_ai.dll | 1 | 3 | 4 (SWU-2.7,2.9,2.11,2.12) |
| modules/display/ | xpe_display.dll | 1 | 1b | 3 implemented (SWU-3.1~3.3); SWU-3.4 deferred |
| modules/dicom/ | xpe_dicom.dll | 1 | 1b | 4 (SWU-4.1~4.4) |
| gsvg/ | gsvg.dll | 1-G | 2 | 4 (SI-001~004) |
| clients/ImageProcTest/ | ImageProcTest.exe | 2 | 0+ | 2 (SWU-5.7 PipelineOrchestrator, SWU-6.1 QaConstancyTest) |
| clients/ImageProcTest.IntegrationTests/ | (xUnit test project) | 2 | CIT | 78 tests covering 18 P/Invoke symbols (NEW, SPEC-XPE-GUI-IT) |

**총 SWU: 38개 (C/C++ 36개 + C# 2개)**  
`SWU-5.7`, `SWU-6.1`은 Layer 2 C# 구현이며, 나머지 36개만 네이티브 DLL SWU입니다.

**참고**: SWU-6.1 QaConstancyTest는 C# ImageProcTest 내에 구현 (AAPM TG-151, IEC 61223 준수). 테스트 파일: `gui/ImageProcTest.Tests/QaConstancyTests.cs`

## 의존성 방향성 (Architecture Boundaries)

### 핵심 아키텍처 원칙
- **레이어 간 의존성**: Layer 1 → Layer 0만 허용 (측면 의존성 금지)
- **독립성**: Layer 1-G (GSVG) 완전 독립, xpe_common 무의존
- **통합**: Layer 2 (C# GUI) → 모든 레이어 P/Invoke로 통합

### 상세 의존성 규칙
```
Layer 0 (xpe_common.dll)
├── 타입 정의, 메모리 관리, 로깅, AED 시스템
└── 모든 Layer 1 모듈의 기반

Layer 1 (7개 알고리즘 DLL)
├── preprocess (xpe_preprocess.dll): PRE-01~09 알고리즘
├── enhance_basic (xpe_enhance_basic.dll): POST-01~04, POST-07
├── enhance_advanced (xpe_enhance_advanced.dll): POST-05~06, POST-08~09
├── ai (xpe_ai.dll): POST-02, POST-07 AI 알고리즘
├── display (xpe_display.dll): POST-12 DICOM 표시
├── dicom (xpe_dicom.dll): SUP-04 DICOM I/O
└── 상호 의존성 완전 금지

Layer 1-G (gsvg.dll)
├── 독립 IEC 62304 패키지
├── xpe_common.dll과 무관
└── 자체적인 의존성 관리

Layer 2 (ImageProcTest.exe)
├── C# WPF GUI 통합 테스트
├── P/Invoke로 모든 DLL 호출
└── SWU-5.7 (PipelineOrchestrator), SWU-6.1 (QaConstancyTest)
```

### 특수 구성 요소
- **SWU-5.7**: C# Layer 2에서 구현, DLL 모듈에 없음
- **SWU-6.1**: ImageProcTest 내 품질 보장 테스트
- **P/Invoke 인터페이스**: 모든 DLL 호출 통일 인터페이스

### 모듈 경계 (Module Boundaries)
- **강력한 경계**: 각 DLL은 독립적이며 공통 인터페이스만 공유
- **상태 관리**: 모든 모듈은 무상태(stateless)로 설계
- **오류 처리**: 각 DLL 자체적 오류 처리 및 로깅
- **스레드 안전성**: 모든 내보내기 함수 재진입 가능

---

## modules/preprocess/ 상세 구조 (SPEC-XPE-P1A, v1.1.0 이후)

### 헤더 파일 (include/xpe/preprocess/)

| 파일 | 목적 | 상태 |
|------|------|------|
| `preprocess_api.h` | 공개 API 정의 (P/Invoke 호출) | Existing |
| `xcal_format.h` | XCal v1 포맷 정의 (magic, header, SHA-256 slot) | NEW |

### 소스 파일 (src/)

#### 보정 알고리즘 (기존)
| 파일 | SWU | 함수 | 상태 |
|------|-----|------|------|
| `xpe_offset.cpp` | SWU-1.1 | xpe_offset_correct() | Existing |
| `xpe_gain.cpp` | SWU-1.2 | xpe_gain_correct() | Existing |
| `xpe_defect.cpp` | SWU-1.3 | xpe_defect_correct(), xpe_defect_detect_runtime() | Existing |
| `xpe_preprocess.cpp` | SWU-1.x | 초기화, 종료, 메인 제어 로직 | Existing (slimmed) |

#### 보정 데이터 관리 (NEW, SUP-01)
| 파일 | 함수 | 목적 | 테스트 수 |
|------|------|------|----------|
| `xcal_reader.cpp` | read_xcal_file() | XCal 파일 파싱, 검증 및 데이터 추출 | 8 |
| `xcal_writer.cpp` | write_xcal_file() | XCal 파일 생성, SHA-256 서명 | 8 |
| `xcal_validator.cpp` | validate_xcal_header(), validate_sha256() | XCal 포맷 및 무결성 검증 | 18 |
| `xpe_calib_load_offset.cpp` | xpe_calib_load_offset() | Offset 맵 로딩 | 6 |
| `xpe_calib_load_gain.cpp` | xpe_calib_load_gain() | Gain 맵 로딩 | 6 |
| `xpe_calib_load_defect_map.cpp` | xpe_calib_load_defect_map() | Defect 맵 로딩 | 6 |
| `xpe_calib_save.cpp` | xpe_calib_save() | 보정 데이터 저장 (atomic write) | 8 |
| `xpe_calib_generate_offset.cpp` | xpe_calib_generate_offset() | 다중 프레임에서 offset 생성 | 8 |
| `xpe_calib_check_expiry.cpp` | xpe_calib_check_expiry() | 보정 데이터 유효기간 검증 | 8 |
| `xpe_calibration.cpp` | 로컬 상태 관리 | 전역 calibration 상태 (slimmed to bare minimum) | Refactored |

#### 유틸리티 (NEW, SUP-01)
| 파일 | 함수 | 목적 |
|------|------|------|
| `xpe_sha256.hpp` | compute_sha256_hex(), compute_sha256_binary() | SHA-256 래퍼 (PicoSHA2 기반) |

### 테스트 파일 (tests/)

#### 기존 테스트
| 파일 | 테스트 수 | 커버리지 |
|------|----------|---------|
| `test_xpe_preprocess_calibration.cpp` | 8 | Existing correction functions (offset, gain, defect) |

#### NEW 테스트 (SPEC-XPE-P1A SUP-01)
| 파일 | 테스트 수 | 요구사항 |
|------|----------|---------|
| `test_xcal_validator.cpp` | 18 | REQ-P1A-014~019 (XCal format validation) |
| `test_xcal_reader.cpp` | 8 | REQ-P1A-014~016 (calibration loading) |
| `test_xcal_writer.cpp` | 8 | REQ-P1A-019 (calibration save) |
| `test_xpe_sha256.cpp` | 8 | SHA-256 integrity, hex/binary encoding |
| `test_xpe_calib_load.cpp` | 6 | REQ-P1A-014~016 (round-trip) |
| `test_xpe_calib_save.cpp` | 6 | REQ-P1A-019 (atomic write contract) |
| `test_xpe_calib_generate_offset.cpp` | 8 | REQ-P1A-017 (offset generation, NaN guard) |
| `test_xpe_calib_check_expiry.cpp` | 8 | REQ-P1A-018 (date handling, epoch boundary) |
| `test_xpe_calib_endurance.cpp` | 1 | Leak detection (1000 cycles) |

#### Fixtures
| 파일 | 목적 |
|------|------|
| `fixtures/make_xcal.hpp` | 테스트용 XCal 파일 생성 헬퍼 |

### 빌드 아티팩트

| 출력 | 타입 | 대상 |
|------|------|------|
| `xpe_preprocess.dll` | DLL (x64) | P/Invoke 호출 (C#) |
| `test_xpe_preprocess` | 실행파일 | Google Test 스위트 |

### 의존성 (Dependency Graph)

```
xpe_preprocess.dll
├── xpe_common.dll (Layer 0)
│   └── xpe_types.h, xpe_error.h, xpe_log.h, xpe_memory.h
├── third_party/picosha2/picosha2.h (header-only, no build artifact)
└── standard C++ library (std::vector, std::string, std::algorithm)
```

### 통합 테스트 (SPEC-XPE-GUI-IT)

C# 레이어에서 P/Invoke를 통해 calibration loading 함수 호출:
- `clients/ImageProcTest.IntegrationTests/Optional/CalibLoadOptionalTests.cs`
- REQ-GUI-IT-062: xpe_calib_load_offset(nonexistentPath) → XPE_ERR_IO_FAILED 검증

---
