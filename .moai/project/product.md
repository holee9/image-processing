# 제품 개요: X-ray Image Processing Engine (XPE) (v2.1)

**Version**: 2.1.0 | **Updated**: 2026-04-19
**Changes from v2.0**: Ghost Correction Tier 1/2/3 구현 완료, 전처리 파이프라인 통합 (xpe_preprocess_pipeline), NLCSC 알고리즘 고도화, Phase 1a 완료 상태 반영. See `.moai/project/trend-survey-2026.md`.

## 제품 정체성

- **이름**: X-ray Image Processing Engine (XPE)
- **저장소**: https://github.com/holee9/image-processing
- **분야**: 의료 기기 소프트웨어 (X-ray Flat Panel Detector)
- **규제**: IEC 62304 Class B/C, FDA 21 CFR 820.30, EU MDR 2017/745, **FDA §524B Cyber Device (2025-06 Final)**, **FDA PCCP AI-DSF (2024-12 Final)**, **FDA AI-DSF Lifecycle (2025-01 Draft)**, **EU Regulation 2024/1689 AI Act (2027-08 전면 발효)**, **ISO/IEC 42001:2023 AIMS**, **IEC 81001-5-1:2021 Secure SW Lifecycle**, **QMSR 2026-02-02 발효**
- **개발 상태**: Phase 1a 완료 (2026-04-19), Phase 1b 및 Phase 2 준비 중

## 목적

X-ray Flat Panel Detector(FPD)에서 획득한 Raw 영상 데이터를 진단 가능한 의료 영상으로 변환하는 영상처리 엔진. Detector raw data의 보정(Pre-Processing)부터 영상 향상 및 DICOM 표시(Post-Processing)까지 전체 파이프라인을 커버하며, 의료 기기 규정 인증을 목표로 개발 중입니다.

## Product Components

### 1. Native Processing Modules (C/C++ DLLs)

각 알고리즘 모듈은 독립 DLL로 빌드되며, 최소 종속성 원칙으로 모듈화.

**Pre-Processing (Raw → Clean Image) — 9개 기술 (PRE-01~09)** | **Phase 1a 완료**

| Research ID | 기술 | 분류 | 구현 상태 |
|-------------|------|------|----------|
| PRE-01 | Readout Artifact Validation | 필수 (HW-only FPGA, SW는 validation) | ✅ 완료 |
| PRE-02 | Offset/Dark Correction | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-03 | Gain/Flat-Field Correction | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-04 | Lag Correction (LTI 기본) | 필수 (SW-only) | ✅ 완료 |
| PRE-04 | Lag Correction (NLCSC 비선형) | 차별화 — 14-50x 업계 우위 | ✅ 완료 (Tier 1/2/3) |
| PRE-05 | Ghost/Gain Ghosting Correction | 필수 (SW-only) | ✅ 완료 |
| PRE-06 | Defective Pixel Correction (기본 보간) | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-06 | Defective Pixel Correction (ML/ViT AE) | 차별화 — 14.2x NMSE 우위 | 🔄 진행 중 |
| PRE-07 | Temperature Compensation | 필수 (SW-first → MCU) | ✅ 완료 |
| PRE-08 | Non-linearity Correction | 필수 (SW-first → HW) | ✅ 완료 |
| PRE-09 | Pixel Binning Correction | 조건부 필수 (형광투시/CBCT 시) | ✅ 완료 |

**Support Technologies — 5개 기술 (SUP-01~05)**

| Research ID | 기술 | SWU 매핑 | 분류 | 구현 상태 |
|-------------|------|-----------|------|----------|
| SUP-01 | Calibration Parameter Management | SWU-1.5, SWU-5.6 | 필수 | ✅ 완료 |
| SUP-02 | Exposure Detection (AED) | SWU-5.8 AedEventInterface | 필수 | ✅ 완료 |
| SUP-03 | Exposure Index (IEC 62494-1) | SWU-2.10 ExposureIndexCalc | 필수 | ✅ 완료 |
| SUP-04 | DICOM Conformance | SWU-4.1~4.4 | 필수 | ✅ 완료 |
| SUP-05 | Quality Assurance / Constancy Test | SWU-6.1 QaConstancyTest (C#) | 필수 | ✅ 완료 |

**Post-Processing (Clean → Diagnostic-Ready Image)** | **Phase 1b 진행 중**

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
- **통합 테스트**: 78개 테스트 커버리지, C# P/Invoke 래퍼 완성

## Must-Have vs Differentiator Strategy

출처: `docs/xray_fpd_tech_classification_final.md` (v2.0, 2026-04-13), **확장 출처**: `.moai/project/trend-survey-2026.md` (v1.0, 2026-04-17)

- **필수 (Must-Have)**: 미구현 시 IEC/FDA 규제 인증 불가. Pre-Processing 전체(PRE-01~09), Support(SUP-01~05), Post-Processing 기반(POST-01~04, POST-07 기본, POST-12) 해당.
- **차별화 (Differentiator)**: Phase 2/3에서 경쟁 우위 확보. PRE-04 NLCSC(14-50x), PRE-06 ML(14.2x), POST-05 MFP, POST-09 Bone Suppression, POST-11 Virtual Grid.
- **HW/SW 경계**: PRE-01은 FPGA 전담(HW팀). PRE-02,03,06,08,09는 SW-first 후 FPGA 이관 가능 설계. PRE-07은 SW-first 후 MCU 이관 가능 설계. PRE-04,05는 SW-only.

### Must/Should/Could 3-Tier 전략 (v2.1 Strict)

**v2.1 변경**: 엄격 재분류 (2026-04-17). Must 27→12. "반드시 출시 블로커 또는 법적 의무"만 Must. 프로젝트 관례·자발적 표준은 Should.

| Tier | 항목 수 (v2.1) | 의미 | 문서 |
|:----:|:------:|------|------|
| **Must** | **12** (was 27) | 법률 의무 OR 시장 진입 블로커 OR IEC 62304 강제 조항 | 신규 SPEC 4종의 core 부분 + IEC 62304 |
| **Should** | **35** (was 19) | 강력 권장 · 경쟁력 · Phase 연결 조건부 Must | Phase 2/3 및 AI 배포 결정 시 승격 |
| **Could** | 10 | 미래 옵션·연구 모드 | Phase 4 Research Track |

**v2.1 엄격 판정 기준**: "이것이 없으면 출시 불가 또는 법적 거절?" → YES만 Must

### Must 12항목 (v2.1 엄격)

| ID | 항목 | 블로커 근거 |
|:--:|------|------------|
| M-01 | IEC 62304 Class B | 의료기기 SW 법적 분류 증명 |
| M-02 | EU MDR 2017/745 | EU 시장 진입 법률 |
| M-03 | FDA 21 CFR 820.30 + QMSR 2026 | US 시장 진입 법률 |
| M-04 | FDA §524B Cyber Device | 2023 법률 (DICOM 네트워크 = 강제 cyber device) |
| M-05 | SBOM (SPDX 3.0/CycloneDX 1.6) | §524B 법적 필수 구성요소 |
| M-06 | Vulnerability Management | §524B 판매 후 법적 의무 |
| M-07 | Basic Input Validation | 기본 방어 + IEC 62304 결합 |
| M-08 | DICOM 3.0 Core | 병원 PACS 통합 실질적 필수 |
| M-09 | DICOM Conformance Statement | 병원 구매 실사 법적 증빙 |
| M-10 | Post-Market Surveillance | EU MDR Article 83 법률 |
| M-11 | Characterization Tests | IEC 62304 §5.4.1 강제 조항 |
| M-12 | Trackability (RTM, commits) | IEC 62304 §5.4.1 강제 조항 |

### Should 35항목 (v2.1 확장, 조건부 Must 승격)

- **Regulatory Should 8**: FDA PCCP, AI-DSF Lifecycle Draft, Transparency, IMDRF GMLP, EU AI Act, ISO 42001, 기존 S-REG-01~02 — **Phase 3 AI 배포 시 Must 승격**
- **Cybersecurity Should 5**: IEC 81001-5-1 (자발적), SLSA L3, Threat Modeling, SBOM Continuous, CVD — **SBOM/L3는 Phase 2 Must 승격 권고**
- **Interoperability Should 5**: DICOMweb (Phase 2+), FHIR R5 (Phase 2-3+), IHE AIR/AIRA (Phase 3+), IHE Baseline (통합 편의), DICOM SR for AI — **Phase 연결**
- **AI Should 8**: SSL Denoising, Diffusion Priors, Conformal UQ, XAI, ONNX 1.20+, ML Defect, Bone Suppression, AI Collimation — **Phase 3 진입 시**
- **Operations Should 5**: Drift Detection, OpenTelemetry, VEX, Reject-Analysis, Reproducible Builds — **운영 성숙도 향상**
- **Quality/Architecture Should 4**: TRUST 5, Reference+SIMD Parity, MX Tag System, Anti-Spaghetti — **프로젝트 품질 기반 (v2.0 Must → v2.1 Should)**

### Could 10항목 (Phase 4+)

MedSAM Foundation Model, Federated Learning, GPU Production Path, Rust Safety Module, PQC, FHIRcast, WebAssembly, Continuous Learning, NIST AI RMF Full Adoption, Generative AI

### 조건부 Must 승격 규칙 (v2.1 신규)

- Phase 3 AI-DSF 배포 승인 → S-REG-03~07, S-AI-06~08 항목 Must 승격
- EU 시장 + AI 판매 결정 → S-REG-07 (EU AI Act) Must 승격
- Connectathon 참여 결정 → S-IOP-05 (IHE Baseline) Must 승격
- SLSA L3 인증 결정 → S-SEC-01 Must 승격

상세: `.moai/project/trend-survey-2026.md` v1.1 §1-§4

## Architecture Principle

**Anti-Spaghetti 3-Layer Design**:
- Layer 0: 공통 타입/메모리 (xpe_common.dll)
- Layer 1: 알고리즘 DLLs (상호 의존 금지, Layer 0에만 의존)
- Layer 1-G: GSVG (독립 IEC 62304 패키지, xpe_common 비의존)
- Layer 2: C# GUI Orchestrator (P/Invoke로 모든 DLL 호출)

**신규**: 전처리 파이프라인 통합 (xpe_preprocess_pipeline 함수)
- 단일 함수로 전처리 단계 0.5~4 통합 처리
- 형식 변환 체크포인트 자동 관리 (uint16 → float32)
- NLCSC 고스트 보정 Tier 1/2/3 지원

## 대상 사용자

- **영상처리 엔지니어**: 알고리즘 개발 및 튜닝
- **QA 엔지니어**: IEC 62304 검증/확인 및 테스트 관리
- **시스템 통합자**: DLL을 RadiConsole 등 프로덕션 GUI에 연동
- **의료 영상 전문가**: 알고리즘 평가 및 검증

## 주요 시나리오

### 1. X-ray 이미지 처리 파이프라인
- **입력**: Raw FPD 데이터 (DICOM 형식, UINT16/FLOAT32)
- **전처리**: Offset/Dark 보정, Gain 보정, 불량 픽셀 보정 등 (17단계 완료)
- **후처리**: 노이즈 감소, 대비 향상, 윤곽선 강화 등
- **출력**: 진단 가능한 의료 영상 (DICOM 표준 준수)

### 2. 알고리즘 개발 및 검증
- 단위 테스트 및 통합 테스트 수행
- ImageProcTest GUI로 실시간 알고리즘 검증
- 성능 벤치마킹 및 품질 평가

### 3. 의료 기기 인증 준비
- IEC 62304 Class B 합격 지원
- 요구사항 추적 관리 (RTM)
- 자동화 테스트 및 검증

## 개발 현황

### 현재 단계
**Phase 1a 완료** (2026-04-19): 전처리 모듈 및 기반 인프라 구현 완료
- **Phase 0 완료**: 인프라, 공통 라이브러리, 테스트 프레임워크, GUI 프로토타입, CI/CD
- **Phase 1a 완료**: 전처리(PRE-01~09) 및 기반 후처리(POST-01~04, POST-07) 구현
- **Phase 1b 예정**: 고급 후처리(POST-05~06, POST-08~09) 구현
- **목표**: 의료 기기 인증 (IEC 62304 Class B)

### 최근 개발 성과 (2026-04-19)
- **SPEC-XPE-P1A**: 전처리 모듈 (Gain/Offset/Defect Correction) 구현 완료
- **Ghost Correction Tier 1/2/3**: NLCSC 알고리즘 고도화 완료 (14-50x 성능 향상)
- **통합 테스트 GUI**: ImageProcTest WPF GUI로 DLL 모듈 통합 테스트 지원
- **전처리 파이프라인**: xpe_preprocess_pipeline 함수로 단일 통합 처리 지원
- **API 명세**: 완전한 C ABI 규격 83개 함수 구현 (파이프라인 통합으로 +1)

### 로드맵 (v2.1 Updated)

**Phase 1b (진행 예정)**: 고급 후처리 알고리즘 구현
- POST-05 Multiscale Frequency Processing
- POST-06 Fractional Processing
- POST-08 Collimation Detection
- POST-09 Exposure Index Calculation

**Phase 2 (Differentiator)**: AI 기반 고급 알고리즘 도입 (deterministic 중심)
- PRE-04 NLCSC Lag Correction (14-50x 업계 우위) ✅ 완료
- PRE-06 ML Defect Correction (14.2x NMSE) — Phase 3 AI tier와 통합
- POST-05 Multiscale Frequency Processing
- POST-09 Bone Suppression — Phase 3 AI tier로 분류
- POST-11 Virtual Grid (GSVG, 별도 IEC 62304 패키지)

**Phase 3 (Intelligence)**: AI 모듈 고도화 (SPEC-XPE-P3-AI v1.0)
- POST-02 Deep Learning Denoising — SSL (Noise2Noise family) baseline
- POST-02 Diffusion Priors — opt-in premium tier (NEED/DiffDenoise)
- POST-07 AI Collimation Detection (baseline Hough + AI refinement)
- POST-09 Bone Suppression (U-Net, +16.8% 민감도)
- ONNX Runtime 1.20+ through Multi-EP (CPU/CUDA/TensorRT/DirectML)
- XAI Sidecar (Grad-CAM saliency, SHAP, opt-in)
- Conformal Prediction Uncertainty Quantification
- PCCP boundary enforcement (FDA 2024-12)
- Model Card API + Data Lineage

**Parallel Tracks (신규 v2.0)**:
- **S-REG**: FDA/EU/ISO 규제 문서 및 governance (S0-A 이후 병행)
- **S-SEC**: §524B + SBOM + SLSA (S0-B 이후 병행)
- **S-OPS**: PMS + Drift + OTEL + Reproducible (S1-B1 이후 병행)
- **S-IOP**: DICOMweb + FHIR R5 + IHE AIR/AIRA (S1-B3 이후)

**Phase 4 Research Track (Could, 2027+)**: MedSAM fine-tuning, Federated Learning, GPU production path, Rust safety modules.