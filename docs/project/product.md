# 제품 개요: X-ray 영상 처리 엔진

## 제품 정보

- **이름**: X-ray Image Processing Engine
- **저장소**: https://github.com/holee9/image-processing
- **분야**: Medical Device Software (X-ray Flat Panel Detector)
- **규제**: IEC 62304 Class B/C, FDA 21 CFR 820.30, EU MDR 2017/745

## 목적

X-ray Flat Panel Detector(FPD)에서 획득한 raw 영상 데이터를 진단 가능한 의료 영상으로 변환하는 영상처리 엔진입니다. Detector raw data의 보정(Pre-Processing)부터 영상 향상 및 DICOM 표시(Post-Processing)까지 전체 파이프라인을 포함합니다.

## 제품 구성

### 1. 네이티브 처리 모듈 (C/C++ DLLs)

각 알고리즘 모듈은 독립적인 DLL로 빌드되며, 최소 종속성 원칙에 따라 모듈화되었습니다.

**전처리 (Raw → Clean Image) — 9개 기술 (PRE-01~09)**

| Research ID | 기술 | 분류 |
|-------------|------|------|
| PRE-01 | Readout Artifact Correction | 필수 (HW-only FPGA, SW는 validation) |
| PRE-02 | Offset/Dark Correction | 필수 (SW-first → HW) |
| PRE-03 | Gain/Flat-Field Correction | 필수 (SW-first → HW) |
| PRE-04 | Lag Correction (LTI 기본) | 필수 (SW-only) |
| PRE-04 | Lag Correction (NLCSC 비선형) | 차별화 — 14-50x 업계 우위 |
| PRE-05 | Ghost/Gain Ghosting Correction | 필수 (SW-only) |
| PRE-06 | Defective Pixel Correction (기본 보간) | 필수 (SW-first → HW) |
| PRE-06 | Defective Pixel Correction (ML/ViT AE) | 차별화 — 14.2x NMSE 우위 |
| PRE-07 | Temperature Compensation | 필수 (SW-first → MCU) |
| PRE-08 | Non-linearity Correction | 필수 (SW-first → HW) |
| PRE-09 | Pixel Binning Correction | 조건부 필수 (형광투시/CBCT 시) |

**지원 기술 — 5개 기술 (SUP-01~05)**

| Research ID | 기술 | SWU 매핑 | 분류 |
|-------------|------|-----------|------|
| SUP-01 | Calibration Parameter Management | SWU-1.5, SWU-5.6 | 필수 |
| SUP-02 | Exposure Detection (AED) | SWU-5.8 AedEventInterface | 필수 |
| SUP-03 | Exposure Index (IEC 62494-1) | SWU-2.10 ExposureIndexCalc | 필수 |
| SUP-04 | DICOM Conformance | SWU-4.1~4.4 | 필수 |
| SUP-05 | Quality Assurance / Constancy Test | SWU-6.1 QaConstancyTest (C#) | 필수 |

**후처리 (Clean → Diagnostic-Ready Image)**
- Log Transform, Noise Reduction, Contrast Enhancement (CLAHE), Edge Enhancement
- Multiscale Frequency Processing, Collimation Detection, Exposure Index
- Body-Part Recognition (CNN), Bone Suppression (U-Net), DL Denoiser
- Grid Suppression Virtual Grid (GSVG) - 독립 모듈
- DICOM Grayscale Display Pipeline (Modality/VOI/Presentation LUT)

**인프라**
- DICOM I/O (Reader/Writer/Network SCU)
- Common Library (Memory Pool, Thread Pool, Logger, Config, Parameter Validator)

### 2. 통합 테스트 GUI (C# WPF)

- **이름**: ImageProcTest
- **목적**: 알고리즘 DLL 모듈을 P/Invoke로 로드하여 통합 테스트 수행
- **기능**: 파이프라인 빌더, DICOM 영상 뷰어, W/L 조절, 벤치마크

## 필수 기술 vs 차별화 기술 전략

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, 2026-04-13)

- **필수 (Must-Have)**: 미구현 시 IEC/FDA 규제 인증 불가능. Pre-Processing 전체(PRE-01~09), Support(SUP-01~05), Post-Processing 기본 기술(POST-01~04, POST-07 기본, POST-12) 포함.
- **차별화 (Differentiator)**: Phase 2/3에서 경쟁 우위 확보. PRE-04 NLCSC(14-50x), PRE-06 ML(14.2x), POST-05 MFP, POST-09 Bone Suppression, POST-11 Virtual Grid.
- **HW/SW 경계**: PRE-01은 FPGA 전담(HW팀). PRE-02,03,06,08,09는 SW-first 후 FPGA 이관 가능 설계. PRE-07은 SW-first 후 MCU 이관 가능 설계. PRE-04,05는 SW-only.

## 아키텍처 원칙

**Anti-Spaghetti 3-Layer Design**:
- Layer 0: 공통 타입/메모리 (xpe_common.dll)
- Layer 1: 알고리즘 DLLs (상호 의존 금지, Layer 0에만 의존)
- Layer 1-G: GSVG (독립 IEC 62304 패키지, xpe_common 비의존)
- Layer 2: C# GUI Orchestrator (P/Invoke로 모든 DLL 호출)

**SWU 카운팅 범위**:
- DLL 직접 매핑 기준: 38개 (C/C++ 36개 + C# 2개)
- SPEC-XPE-MASTER v2.0.0 전체 기준: 43개 (Infrastructure 7 + 전처리 9 + 핵심처리 12 + 디스플레이 4 + DICOM 4 + GSVG 4 + C# GUI 2 + QA 1)
- 차이 설명: xpe_common.dll 내부 서브유닛(MemoryPool, ThreadPool, ErrorHandler, Logger, ParameterValidator, ConfigManager, AedEventInterface)은 DLL 1개로 매핑되지만 개별 SWU로 계수

## 대상 사용자

- 영상처리 엔지니어: 알고리즘 개발 및 튜닝
- QA 엔지니어: IEC 62304 검증/확인
- 시스템 통합자: DLL을 RadiConsole 등 프로덕션 GUI에 연동

## 배포 바이너리 구성 (전체 10개)

| 번호 | 바이너리 | 유형 | Phase | 함수 수 |
|:---:|---|---|:---:|:---:|
| 1 | `xpe_common.dll` | C/C++ DLL | 0 | 18 |
| 2 | `xpe_preprocess.dll` | C/C++ DLL | 1a | 18 |
| 3 | `xpe_enhance_basic.dll` | C/C++ DLL | 1b | 7 |
| 4 | `xpe_display.dll` | C/C++ DLL | 1b | 11 |
| 5 | `xpe_dicom.dll` | C/C++ DLL | 1b | 10 |
| 6 | `xpe_enhance_advanced.dll` | C/C++ DLL | 2 | 3 |
| 7 | `gsvg.dll` | C/C++ DLL (독립) | 2 | 8 |
| 8 | `xpe_ai.dll` | C/C++ DLL | 3 | 7 |
| 9 | `ImageProcTest.exe` | C# WPF GUI | 1b | — |
| 10 | `xpe_ai_worker.exe` | C++ 프로세스 | 3 | — (IPC) |
| — | `xpe_common_infra.lib` | 정적 공통 인프라 | 0 | 내부 |

**합계**: DLL 8개 + 실행파일 2개 = **10개 배포 바이너리**

### SWU 카운팅 (SPEC-XPE-MASTER v2.0.0 기준)

| 범주 | SWU 수 | 대표 유닛 |
|---|:---:|---|
| Infrastructure (xpe_common) | **7** | MemoryPool, ThreadPool, ErrorHandler, Logger, ParameterValidator, ConfigManager, AedEventInterface |
| 전처리 (xpe_preprocess) | 9 | CalibManager, OffsetCorrector, GainCorrector, DefectCorrector, GhostCorrector, ReadoutValidator, TempCompensator, NonlinearityCorrector, BinningCorrector |
| 핵심처리 (enhance_basic/advanced) | 12 | LogTransform, NoiseReducer, ContrastEnhancer, EdgeEnhancer, MultiscaleProcessor, FractionalProcessor, CollimationDetector, ExposureIndexCalc, BodyPartRecognizer, Stitcher, BoneSuppressor, DLDenoiser |
| 디스플레이 (xpe_display) | 4 | ModalityLUT, VOILUT, PresentationLUT, LUTManager |
| DICOM I/O (xpe_dicom) | 4 | DicomReader, DicomWriter, DicomNetworkSCU, DicomValidator |
| GSVG (독립 IEC 62304) | 4 | GridDetector, GridSuppressor, VirtualGridGenerator, ScatterLUTManager |
| C# GUI | 2 | PipelineOrchestrator, QaConstancyTest |
| **합계** | **43** | |

## 개발 상태

- IEC 62304 규정 문서: 완비 (XPE, GSVG, Ghost Correction, Panel Defect, **Calibration v1.0**)
- 소스 코드: 스캐폴딩 단계 (xpe_common 기초 구현 중)
- Phase 0 (Foundation) → Phase 1 (Pre/Post Basic) → Phase 2 (Clinical) → Phase 3 (AI)
