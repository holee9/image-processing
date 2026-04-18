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
│   │   ├── product.md             # 제품 개요 (v2.1 업데이트)
│   │   ├── structure.md           # 프로젝트 구조
│   │   ├── tech.md               # 기술 스택 (v2.0 업데이트)
│   │   └── codemaps/             # 아키텍처 코맵 (2026-04-19)
│   ├── config/                    # MoAI 설정
│   └── agents/                    # 에이전트 정의
│
├── CLAUDE.md                      # MoAI 실행 지침
├── AGENTS.md                      # 저장소 가이드라인
└── README.md                      # 프로젝트 개요
```

## 모듈-DLL 매핑

| 디렉토리 | 출력 | 레이어 | 단계 | SWU/SI 개수 | 구현 상태 |
|----------|------|--------|------|-------------|----------|
| modules/common/ | xpe_common.dll | 0 | 0 | 18 (SWU-5.1~5.6, SWU-5.8) | ✅ 완료 |
| modules/preprocess/ | xpe_preprocess.dll | 1 | 1a | 19 (PRE-01~09 + pipeline) | ✅ 완료 |
| modules/enhance_basic/ | xpe_enhance_basic.dll | 1 | 1b | 6 (POST-01~04, POST-07) | ✅ 완료 |
| modules/enhance_advanced/ | xpe_enhance_advanced.dll | 1 | 2 | 4 (POST-05,06,08,09) | 🔄 Phase 1b 예정 |
| modules/ai/ | xpe_ai.dll | 1 | 3 | 7 (POST-02 AI, POST-07 AI, etc.) | 🔄 Phase 3 예정 |
| modules/display/ | xpe_display.dll | 1 | 1b | 11 implemented (POST-12) | ✅ 완료 |
| modules/dicom/ | xpe_dicom.dll | 1 | 1b | 10 (SUP-04) | ✅ 완료 |
| gsvg/ | gsvg.dll | 1-G | 2 | 8 (GSVG-01~04) | 🔄 Phase 2 예정 |
| clients/ImageProcTest/ | ImageProcTest.exe | 2 | 0+ | 2 (SWU-5.7 PipelineOrchestrator, SWU-6.1 QaConstancyTest) | ✅ 완료 |
| clients/ImageProcTest.IntegrationTests/ | (xUnit test project) | 2 | CIT | 78 tests covering 18 P/Invoke symbols | ✅ 완료 |

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
├── 18개 API 함수: xpe_init(), xpe_alloc_image(), xpe_configure(), etc.
└── 모든 Layer 1 모듈의 기반

Layer 1 (7개 알고리즘 DLL)
├── preprocess (xpe_preprocess.dll): PRE-01~09 알고리즘 (19개 함수)
│   ├── 신규: xpe_preprocess_pipeline() 전처리 통합 함수
│   ├── Ghost Correction Tier 1/2/3 (NLCSC) 구현 완료
│   └── Temperature Compensation, Nonlinearity Correction 추가
├── enhance_basic (xpe_enhance_basic.dll): POST-01~04, POST-07 (6개 함수)
├── enhance_advanced (xpe_enhance_advanced.dll): POST-05~06, POST-08~09 (4개 함수)
├── ai (xpe_ai.dll): POST-02, POST-07 AI 알고리즘 (7개 함수)
├── display (xpe_display.dll): POST-12 DICOM 표시 (11개 함수)
├── dicom (xpe_dicom.dll): SUP-04 DICOM I/O (10개 함수)
└── 상호 의존성 완전 금지

Layer 1-G (gsvg.dll)
├── 독립 IEC 62304 패키지
├── xpe_common.dll과 무관
└── 자체적인 의존성 관리 (FFTW3, OpenCV, etc.)

Layer 2 (ImageProcTest.exe)
├── C# WPF GUI 통합 테스트
├── P/Invoke로 모든 DLL 호출
├── SWU-5.7 (PipelineOrchestrator): 전체 파이프라인 제어
├── SWU-6.1 (QaConstancyTest): 품질 보장 테스트
└── 78개 xUnit 통합 테스트 커버리지
```

### 특수 구성 요소
- **SWU-5.7**: C# Layer 2에서 구현, DLL 모듈에 없음
- **SWU-6.1**: ImageProcTest 내 품질 보장 테스트
- **P/Invoke 인터페이스**: 모든 DLL 호출 통일 인터페이스
- **신규**: 전처리 파이프라인 통합 (xpe_preprocess_pipeline)

### 모듈 경계 (Module Boundaries)
- **강력한 경계**: 각 DLL은 독립적이며 공통 인터페이스만 공유
- **상태 관리**: 모든 모듈은 무상태(stateless)로 설계
- **오류 처리**: 각 DLL 자체적 오류 처리 및 로깅
- **스레드 안전성**: 모든 내보내기 함수 재진입 가능

---

## modules/preprocess/ 상세 구조 (SPEC-XPE-P1A, v1.1.0 완료)

### 헤더 파일 (include/xpe/preprocess/)

| 파일 | 목적 | 상태 |
|------|------|------|
| `preprocess_api.h` | 공개 API 정의 (P/Invoke 호출) | Existing |
| `xcal_format.h` | XCal v1 포맷 정의 (magic, header, SHA-256 slot) | ✅ 완료 |
| `xpe_ghost.h` | Ghost Correction Tier 1/2/3 API | ✅ 완료 |
| `xpe_pipeline.h` | 전처리 파이프라인 통합 API | ✅ 신규 |

### 소스 파일 (src/)

#### 보정 알고리즘 (기존)
| 파일 | SWU | 함수 | 상태 |
|------|-----|------|------|
| `xpe_offset.cpp` | SWU-1.1 | xpe_offset_correct() | ✅ 완료 |
| `xpe_gain.cpp` | SWU-1.2 | xpe_gain_correct() | ✅ 완료 |
| `xpe_defect.cpp` | SWU-1.3 | xpe_defect_correct(), xpe_defect_detect_runtime() | ✅ 완료 |
| `xpe_preprocess.cpp` | SWU-1.x | 초기화, 종료, 메인 제어 로직 | ✅ 완료 (slimmed) |

#### Ghost Correction (신규, Tier 1/2/3)
| 파일 | SWU | 함수 | 상태 |
|------|-----|------|------|
| `xpe_ghost.cpp` | SWU-1.4 | xpe_ghost_correct() | ✅ 완료 |
| `ghost_lti.cpp` | Tier 1 | LTI 기반 고스트 보정 | ✅ 완료 |
| `ghost_exposure.cpp` | Tier 2 | 노출 가중치 고스트 보정 | ✅ 완료 |
| `ghost_nlcsc.cpp` | Tier 3 | NLCSC 비선형 고스트 보정 | ✅ 완료 (14-50x 성능 향상) |

#### 보정 데이터 관리 (NEW, SUP-01)
| 파일 | 함수 | 목적 | 테스트 수 | 상태 |
|------|------|------|----------|------|
| `xcal_reader.cpp` | read_xcal_file() | XCal 파일 파싱, 검증 및 데이터 추출 | 8 | ✅ 완료 |
| `xcal_writer.cpp` | write_xcal_file() | XCal 파일 생성, SHA-256 서명 | 8 | ✅ 완료 |
| `xcal_validator.cpp` | validate_xcal_header(), validate_sha256() | XCal 포맷 및 무결성 검증 | 18 | ✅ 완료 |
| `xpe_calib_load_offset.cpp` | xpe_calib_load_offset() | Offset 맵 로딩 | 6 | ✅ 완료 |
| `xpe_calib_load_gain.cpp` | xpe_calib_load_gain() | Gain 맵 로딩 | 6 | ✅ 완료 |
| `xpe_calib_load_defect_map.cpp` | xpe_calib_load_defect_map() | Defect 맵 로딩 | 6 | ✅ 완료 |
| `xpe_calib_save.cpp` | xpe_calib_save() | 보정 데이터 저장 (atomic write) | 8 | ✅ 완료 |
| `xpe_calib_generate_offset.cpp` | xpe_calib_generate_offset() | 다중 프레임에서 offset 생성 | 8 | ✅ 완료 |
| `xpe_calib_check_expiry.cpp` | xpe_calib_check_expiry() | 보정 데이터 유효기간 검증 | 8 | ✅ 완료 |
| `xpe_calibration.cpp` | 로컬 상태 관리 | 전역 calibration 상태 (slimmed to bare minimum) | ✅ Refactored |

#### 신규 보정 알고리즘 (Phase 1a 완료)
| 파일 | SWU | 함수 | 목적 | 상태 |
|------|-----|------|------|------|
| `xpe_temp_compensate.cpp` | SWU-1.5 | xpe_temp_compensate() | 온도 보상 (EP2148500A1) | ✅ 완료 |
| `xpe_nonlinearity.cpp` | SWU-1.7 | xpe_nonlinearity_correct() | 비선형 보정 (BEFORE gain) | ✅ 완료 |
| `xpe_binning.cpp` | SWU-1.8 | xpe_binning_correct() | 빈닝 보정 (조건부) | ✅ 완료 |
| `xpe_readout_validate.cpp` | SWU-1.9 | xpe_validate_readout_artifact() | 읽기 아티팩트 검증 | ✅ 완료 |

#### 유틸리티 (NEW, SUP-01)
| 파일 | 함수 | 목적 | 상태 |
|------|------|------|------|
| `xpe_sha256.hpp` | compute_sha256_hex(), compute_sha256_binary() | SHA-256 래퍼 (PicoSHA2 기반) | ✅ 완료 |

#### 전처리 파이프라인 통합 (신규)
| 파일 | 함수 | 목적 | 상태 |
|------|------|------|------|
| `pipeline.cpp` | xpe_preprocess_pipeline() | 단일 함수로 전처리 통합 (0.5~4 단계) | ✅ 신규 |

### 테스트 파일 (tests/)

#### 기존 테스트
| 파일 | 테스트 수 | 커버리지 | 상태 |
|------|----------|---------|------|
| `test_xpe_preprocess_calibration.cpp` | 8 | Existing correction functions (offset, gain, defect) | ✅ 완료 |

#### NEW 테스트 (SPEC-XPE-P1A SUP-01)
| 파일 | 테스트 수 | 요구사항 | 상태 |
|------|----------|---------|------|
| `test_xcal_validator.cpp` | 18 | REQ-P1A-014~019 (XCal format validation) | ✅ 완료 |
| `test_xcal_reader.cpp` | 8 | REQ-P1A-014~016 (calibration loading) | ✅ 완료 |
| `test_xcal_writer.cpp` | 8 | REQ-P1A-019 (calibration save) | ✅ 완료 |
| `test_xpe_sha256.cpp` | 8 | SHA-256 integrity, hex/binary encoding | ✅ 완료 |
| `test_xpe_calib_load.cpp` | 6 | REQ-P1A-014~016 (round-trip) | ✅ 완료 |
| `test_xpe_calib_save.cpp` | 6 | REQ-P1A-019 (atomic write contract) | ✅ 완료 |
| `test_xpe_calib_generate_offset.cpp` | 8 | REQ-P1A-017 (offset generation, NaN guard) | ✅ 완료 |
| `test_xpe_calib_check_expiry.cpp` | 8 | REQ-P1A-018 (date handling, epoch boundary) | ✅ 완료 |
| `test_xpe_calib_endurance.cpp` | 1 | Leak detection (1000 cycles) | ✅ 완료 |

#### Ghost Correction 테스트 (신규)
| 파일 | 테스트 수 | 목적 | 상태 |
|------|----------|------|------|
| `test_ghost_lti.cpp` | 6 | Tier 1 LTI 고스트 보정 검증 | ✅ 완료 |
| `test_ghost_exposure.cpp` | 8 | Tier 2 노출 가중치 검증 | ✅ 완료 |
| `test_ghost_nlcsc.cpp` | 12 | Tier 3 NLCSC 알고리즘 검증 | ✅ 완료 |
| `test_pipeline_integration.cpp` | 15 | 전체 파이프라인 통합 테스트 | ✅ 신규 |

#### Fixtures
| 파일 | 목적 | 상태 |
|------|------|------|
| `fixtures/make_xcal.hpp` | 테스트용 XCal 파일 생성 헬퍼 | ✅ 완료 |
| `fixtures/ghost_data.hpp` | 고스트 보정 테스트 데이터 | ✅ 신규 |

### 빌드 아티팩트

| 출력 | 타입 | 대상 | 상태 |
|------|------|------|------|
| `xpe_preprocess.dll` | DLL (x64) | P/Invoke 호출 (C#) | ✅ 완료 |
| `test_xpe_preprocess` | 실행파일 | Google Test 스위트 | ✅ 완료 |

### 의존성 (Dependency Graph)

```
xpe_preprocess.dll
├── xpe_common.dll (Layer 0)
│   ├── xpe_types.h, xpe_error.h, xpe_log.h, xpe_memory.h
│   └── 18개 API 함수
├── third_party/picosha2/picosha2.h (header-only, no build artifact)
├── third_party/opencv4 (이미지 처리)
└── standard C++ library (std::vector, std::string, std::algorithm)
```

### 통합 테스트 (SPEC-XPE-GUI-IT)

C# 레이어에서 P/Invoke를 통해 calibration loading 함수 호출:
- `clients/ImageProcTest.IntegrationTests/Optional/CalibLoadOptionalTests.cs`
- REQ-GUI-IT-062: xpe_calib_load_offset(nonexistentPath) → XPE_ERR_IO_FAILED 검증

---

## 통합 아키텍처 상태

### Phase 1a 완료 모듈 (2026-04-19)
- ✅ **xpe_common.dll**: 18개 API 함수 완성
- ✅ **xpe_preprocess.dll**: 19개 API 함수 (전처리 통합 포함)
- ✅ **xpe_enhance_basic.dll**: 6개 API 함수 (기본 후처리)
- ✅ **xpe_display.dll**: 11개 API 함수 (DICOM 표시)
- ✅ **xpe_dicom.dll**: 10개 API 함수 (DICOM I/O)
- ✅ **ImageProcTest.exe**: C# GUI 통합 테스트 환경

### Phase 1b 예정 모듈 (2026-Q2)
- 🔄 **xpe_enhance_advanced.dll**: 4개 API 함수 (고급 후처리)

### Phase 2 예정 모듈 (2026-H2)
- 🔄 **gsvg.dll**: 8개 API 함수 (그리드 처리)

### Phase 3 예정 모듈 (2027)
- 🔄 **xpe_ai.dll**: 7개 API 함수 (AI 처리)

### 테스트 커버리지 현황
- **단위 테스트**: 78개 테스트 커버리지 (Google Test)
- **통합 테스트**: 18개 P/Invoke 심볼 커버리지 (xUnit)
- **전체 커버리지**: 96% 목표 달성 (현재 85%+)

### 아키텍처 검증 완료 항목
- ✅ C ABI 인터페이스 안정성
- ✅ 메모리 관리 모델 (호출자 할당)
- ✅ 스레드 안전성 (재진입 가능)
- ✅ 오류 처리 체계
- ✅ 형식 변환 체크포인트 (uint16 ↔ float32)
- ✅ NLCSC 고스트 보정 Tier 1/2/3 구현
- ✅ 전처리 파이프라인 통합 (xpe_preprocess_pipeline)