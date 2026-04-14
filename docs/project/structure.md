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
├── modules/                       # C/C++ native DLL modules (XPE)
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
├── gui/                           # C# WPF (ImageProcTest)
│   ├── ImageProcTest.sln
│   ├── ImageProcTest/             # Main WPF application
│   └── ImageProcTest.Tests/       # xUnit 테스트
│
├── third_party/
│   └── vcpkg.json                 # SOUP 의존성 매니페스트
│
├── scripts/                       # 빌드/검증 스크립트
├── data/                          # 런타임 데이터 (config, LUT, ONNX models)
│
├── docs/                          # IEC 62304 규정 문서
│   ├── ghost-correction/          # Lag/Ghost 보정 SRS/SAD/SDD
│   ├── panel-defect-algorithm/    # 불량 픽셀 보정
│   ├── post-processing/gsvg/      # GSVG IEC 62304 Class B 패키지
│   ├── post-processing/xpe/       # XPE IEC 62304 Class B 패키지
│   └── xray-fpd-research/         # 연구/캘리브레이션
│
├── CLAUDE.md                      # MoAI execution directive
├── AGENTS.md                      # Repository guidelines
└── .moai/                         # MoAI framework config
```

## 모듈-DLL 매핑

| Directory | DLL Output | Layer | Phase | SWU/SI Count |
|-----------|-----------|-------|-------|--------------|
| modules/common/ | xpe_common.dll | 0 | 0 | 7 (SWU-5.1~5.6, SWU-5.8) |
| modules/preprocess/ | xpe_preprocess.dll | 1 | 1a | 9 (SWU-1.1~1.9) |
| modules/enhance_basic/ | xpe_enhance_basic.dll | 1 | 1b | 5 (SWU-2.1~2.4, SWU-2.10 EI baseline) |
| modules/enhance_advanced/ | xpe_enhance_advanced.dll | 1 | 2 | 4 (SWU-2.5,2.6,2.8,2.10) |
| modules/ai/ | xpe_ai.dll | 1 | 3 | 4 (SWU-2.7,2.9,2.11,2.12) |
| modules/display/ | xpe_display.dll | 1 | 1b | 4 (SWU-3.1~3.4) |
| modules/dicom/ | xpe_dicom.dll | 1 | 1b | 4 (SWU-4.1~4.4) |
| gsvg/ | gsvg.dll | 1-G | 2 | 4 (SI-001~004) |
| gui/ | ImageProcTest.exe | 2 | 0+ | 2 (SWU-5.7 PipelineOrchestrator, SWU-6.1 QaConstancyTest) |

**총 SWU: 38개 (C/C++ 36개 + C# 2개)** — DLL 직접 매핑 기준
> 참고: SPEC-XPE-MASTER v2.0.0에서는 Infrastructure 포함 전체 SWU를 **43개**로 계수 (7 Infrastructure + 9 Pre-Processing + 12 Core Processing + 4 Display + 4 DICOM + 4 GSVG + 2 C# GUI + 1 QA). 본 테이블은 DLL에 직접 매핑되는 38개만 표시. 차이 5개는 xpe_common.dll Infrastructure SWU-5.1~5.6, 5.8의 내부 서브유닛입니다.
`SWU-5.7`, `SWU-6.1`은 Layer 2 C# 구현이며, 나머지 36개만 native DLL SWU입니다.

**참고**: SWU-6.1 QaConstancyTest는 C# ImageProcTest 내에 구현 (AAPM TG-151, IEC 61223 준수). 테스트 파일: `gui/ImageProcTest.Tests/QaConstancyTests.cs`

## 의존성 방향

- Layer 1 → Layer 0 only (횡방향 의존성 없음)
- Layer 1-G (GSVG): 완전히 독립적이고 자급자족
- Layer 2 (C# GUI) → Layer 0 + Layer 1 + Layer 1-G via P/Invoke
- SWU-5.7 (PipelineOrchestrator): C# Layer 2에 구현, DLL에 포함되지 않음
