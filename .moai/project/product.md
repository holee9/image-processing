# Product Overview: X-ray Image Processing Engine

## Product Identity

- **Name**: X-ray Image Processing Engine
- **Repository**: https://github.com/holee9/image-processing
- **Domain**: Medical Device Software (X-ray Flat Panel Detector)
- **Regulatory**: IEC 62304 Class B/C, FDA 21 CFR 820.30, EU MDR 2017/745

## Purpose

X-ray Flat Panel Detector(FPD)에서 획득한 Raw 영상 데이터를 진단 가능한 의료 영상으로 변환하는 영상처리 엔진. Detector raw data의 보정(Pre-Processing)부터 영상 향상 및 DICOM 표시(Post-Processing)까지 전체 파이프라인을 커버한다.

## Product Components

### 1. Native Processing Modules (C/C++ DLLs)

각 알고리즘 모듈은 독립 DLL로 빌드되며, 최소 종속성 원칙으로 모듈화.

**Pre-Processing (Raw → Clean Image) — 9개 기술 (PRE-01~09)**

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

**Support Technologies — 5개 기술 (SUP-01~05)**

| Research ID | 기술 | SWU 매핑 | 분류 |
|-------------|------|-----------|------|
| SUP-01 | Calibration Parameter Management | SWU-1.5, SWU-5.6 | 필수 |
| SUP-02 | Exposure Detection (AED) | SWU-5.8 AedEventInterface | 필수 |
| SUP-03 | Exposure Index (IEC 62494-1) | SWU-2.10 ExposureIndexCalc | 필수 |
| SUP-04 | DICOM Conformance | SWU-4.1~4.4 | 필수 |
| SUP-05 | Quality Assurance / Constancy Test | SWU-6.1 QaConstancyTest (C#) | 필수 |

**Post-Processing (Clean → Diagnostic-Ready Image)**
- Log Transform, Noise Reduction, Contrast Enhancement (CLAHE), Edge Enhancement
- Multiscale Frequency Processing, Collimation Detection, Exposure Index
- Body-Part Recognition (CNN), Bone Suppression (U-Net), DL Denoiser
- Grid Suppression Virtual Grid (GSVG) - 독립 모듈
- DICOM Grayscale Display Pipeline (Modality/VOI/Presentation LUT)

**Infrastructure**
- DICOM I/O (Reader/Writer/Network SCU)
- Common Library (Memory Pool, Thread Pool, Logger, Config, Parameter Validator)

### 2. Integration Test GUI (C# WPF)

- **Name**: ImageProcTest
- **Purpose**: 알고리즘 DLL 모듈을 P/Invoke로 로드하여 통합 테스트
- **Features**: 파이프라인 빌더, DICOM 영상 뷰어, W/L 조절, 벤치마크

## Must-Have vs Differentiator Strategy

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, 2026-04-13)

- **필수 (Must-Have)**: 미구현 시 IEC/FDA 규제 인증 불가. Pre-Processing 전체(PRE-01~09), Support(SUP-01~05), Post-Processing 기반(POST-01~04, POST-07 기본, POST-12) 해당.
- **차별화 (Differentiator)**: Phase 2/3에서 경쟁 우위 확보. PRE-04 NLCSC(14-50x), PRE-06 ML(14.2x), POST-05 MFP, POST-09 Bone Suppression, POST-11 Virtual Grid.
- **HW/SW 경계**: PRE-01은 FPGA 전담(HW팀). PRE-02,03,06,08,09는 SW-first 후 FPGA 이관 가능 설계. PRE-07은 SW-first 후 MCU 이관 가능 설계. PRE-04,05는 SW-only.

## Architecture Principle

**Anti-Spaghetti 3-Layer Design**:
- Layer 0: 공통 타입/메모리 (xpe_common.dll)
- Layer 1: 알고리즘 DLLs (상호 의존 금지, Layer 0에만 의존)
- Layer 1-G: GSVG (독립 IEC 62304 패키지, xpe_common 비의존)
- Layer 2: C# GUI Orchestrator (P/Invoke로 모든 DLL 호출)

## Target Users

- 영상처리 엔지니어: 알고리즘 개발 및 튜닝
- QA 엔지니어: IEC 62304 검증/확인
- 시스템 통합자: DLL을 RadiConsole 등 프로덕션 GUI에 연동

## Development Status

- IEC 62304 규정 문서: 완비 (XPE, GSVG, Ghost Correction, Panel Defect)
- 소스 코드: 스캐폴딩 단계
- Phase 0 (Foundation) → Phase 1 (Pre/Post Basic) → Phase 2 (Clinical) → Phase 3 (AI)
