# XPE (X-ray Processing Engine) — 모듈 개요

**Package Version**: 1.6.0  
**IEC 62304 Classification**: Class B  
**Last Updated**: 2026-04-15  
**Document Count**: 23개  
**Status**: Controlled Draft  

---

## 1. 모듈 목적

XPE는 X선 영상의 전처리, 향상, 표시, EI 계산, AI 추론을 담당하는 주요 소프트웨어 항목이다. 8개 DLL과 1개 독립 실행 파일로 구성되며, IEC 62304 Class B 수명 주기 요구사항을 완전히 준수한다.

---

## 2. 소프트웨어 아키텍처 요약

```
Phase 0  :  xpe_common.dll          — 공통 인프라 (메모리 풀, ABI, 오류 처리)
Phase 1a :  xpe_preprocess.dll      — Offset/Gain/Defect/Ghost 보정
Phase 1b :  xpe_enhance_basic.dll   — Log Transform, CLAHE, W/L, EI Baseline
            xpe_display.dll         — Modality LUT, VOI LUT, GSDF
            xpe_dicom.dll           — DICOM Reader/Writer, J2K, GSPS
Phase 2  :  xpe_enhance_advanced.dll — 4계층 노이즈 감소, 엣지, GSVG, EI ROI
            xpe_gsvg.dll            — Grid Suppression / Virtual Grid
Phase 3  :  xpe_ai.dll (proxy)      — AI 모델 인터페이스
            xpe_ai_worker.exe       — 격리 AI 워커 프로세스
```

---

## 3. 문서 목록 (23개)

### 3.1 PRD 및 계획 문서

| ID | Document | IEC 62304 |
|----|----------|-----------|
| PRD-001 | [xray-postprocessing-prd.md](xray-postprocessing-prd.md) | — |
| PRD-002 | [XPE-PRD-002_Detailed_Project_Execution_PRD.md](XPE-PRD-002_Detailed_Project_Execution_PRD.md) | — |
| PRD-003 | [XPE-PRD-003_PRD_Decomposition_and_Backlog.md](XPE-PRD-003_PRD_Decomposition_and_Backlog.md) | — |
| PLAN-001 | [XPE-PLAN-001_Consolidated_Execution_Plan.md](XPE-PLAN-001_Consolidated_Execution_Plan.md) | — |

### 3.2 IEC 62304 수명 주기 문서

| ID | Document | IEC 62304 Clause |
|----|----------|-----------------|
| SDP-001 | [XPE-SDP-001_Software_Development_Plan.md](XPE-SDP-001_Software_Development_Plan.md) | 5.1 |
| SRS-001 | [XPE-SRS-001_Software_Requirements_Specification.md](XPE-SRS-001_Software_Requirements_Specification.md) | 5.2 |
| SAD-001 | [XPE-SAD-001_Software_Architecture_Document.md](XPE-SAD-001_Software_Architecture_Document.md) | 5.3 |
| SDD-001 | [XPE-SDD-001_Software_Unit_Identification.md](XPE-SDD-001_Software_Unit_Identification.md) | 5.4 |
| SDD-002 | [XPE-SDD-002_Software_Detailed_Design.md](XPE-SDD-002_Software_Detailed_Design.md) | 5.4 |
| **ALG-001** | **[XPE-ALG-001_Unified_Algorithm_Development_Specification.md](XPE-ALG-001_Unified_Algorithm_Development_Specification.md)** | **5.4** |  v1.7 |
| ITP-001 | [XPE-ITP-001_Integration_Test_Plan.md](XPE-ITP-001_Integration_Test_Plan.md) | 5.6 |
| STP-001 | [XPE-STP-001_Software_Test_Plan_and_Cases.md](XPE-STP-001_Software_Test_Plan_and_Cases.md) | 5.5 |
| VVP-001 | [XPE-VVP-001_Verification_Validation_Plan.md](XPE-VVP-001_Verification_Validation_Plan.md) | 5.7 |
| RTM-001 | [XPE-RTM-001_Requirements_Traceability_Matrix.md](XPE-RTM-001_Requirements_Traceability_Matrix.md) | 5.8 |
| SCM-001 | [XPE-SCM-001_Configuration_Management_Plan.md](XPE-SCM-001_Configuration_Management_Plan.md) | 6.1 |
| SRM-001 | [XPE-SRM-001_Software_Risk_Management_File.md](XPE-SRM-001_Software_Risk_Management_File.md) | 7 |
| SHA-001 | [XPE-SHA-001_Software_Hazard_Analysis.md](XPE-SHA-001_Software_Hazard_Analysis.md) | 7 |
| SOUP-001 | [XPE-SOUP-001_SOUP_Analysis.md](XPE-SOUP-001_SOUP_Analysis.md) | 8 |
| SMP-001 | [XPE-SMP-001_Software_Maintenance_Plan.md](XPE-SMP-001_Software_Maintenance_Plan.md) | 12 |
| SPR-001 | [XPE-SPR-001_Problem_Resolution_Process.md](XPE-SPR-001_Problem_Resolution_Process.md) | 9 |

### 3.3 규제 준수 패키지

| ID | Document |
|----|----------|
| MAP-001 | [XPE-62304-MAP-001_Compliance_Matrix.md](XPE-62304-MAP-001_Compliance_Matrix.md) |
| PKG-001 | [xpe-iec62304-class-b-package.md](xpe-iec62304-class-b-package.md) |
| SRP-001 | [XPE-SRP-001_Software_Release_Procedure.md](XPE-SRP-001_Software_Release_Procedure.md) |

---

## 4. XPE-ALG-001 알고리즘 명세 — 빠른 참조

`XPE-ALG-001`은 IEC 62304 §5.4 Detailed Design 문서이며, 시스템의 모든 알고리즘을 수학적 공식, C++ 의사코드, SIMD 최적화 전략, 검증 기준으로 명세한다.

### 4.1 해소된 알고리즘 공백 (v1.7 기준 — 총 80건)

**v1.0 Round 1 (GAP-01~10):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §2 | Python↔C++ 아키텍처 브리지 | GAP-01 |
| §4 | Core Processing 알고리즘 | GAP-02 |
| §5 | Grid Suppression | GAP-03 |
| §8 | AI/DL 알고리즘 | GAP-04 |
| §6 | Display Processing | GAP-05 |
| §10 | SIMD 최적화 파이프라인 | GAP-06 |
| §8.3 | 파노라마 스티칭 | GAP-07 |
| §5.2 | Virtual Grid / Scatter Correction | GAP-08 |
| §7 | Exposure Index (IEC 62494-1) | GAP-09 |
| §9 | 교정 맵 생성↔런타임 연결 | GAP-10 |

**v1.1 Round 2 (GAP-D~N):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §3.0 | Readout Validation (SWU-1.0) | GAP-I |
| §3.0.5 | Non-linearity Correction (monotonic LUT) | GAP-H |
| §3.3.4 | `update_defect_map_runtime()` — AVX2 구현 | GAP-E |
| §4.1 | `avx2_log_ps()` — Cephes 다항식 근사 | GAP-G |
| §5.1.3 | `nsct_grid_suppression()` — 4단계 NSCT | GAP-D |
| §7.2 | EI ROI Central Method 수학 수정 (√0.1 계수) | GAP-F |
| §9.4 | AED-0 Automatic Exposure Detection | GAP-J |
| §12.3 | NPS 계산 (IEC 62220-1 준수) | GAP-L |
| §12.4 | DQE 계산 | GAP-M |
| §12.5 | Collimation Mask Detection / CollimatorMask 클래스 | GAP-N |

**v1.2 Round 3 (GAP-O~X):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §3.5 | Heel Effect Compensation (Wang 2013 Duo-SID) | GAP-O |
| §3.2.5 | Multi-SID Gain 보간 및 kVp 선택 | GAP-P |
| §2.4 | 교정 세션 잠금 및 매니페스트 해시 체인 | GAP-Q |
| §13 | 품질 상태 벡터 사이드카 (XpeQualityState) | GAP-R |
| §11.4 | 스칼라 참조 + SIMD 패리티 하네스 | GAP-S |
| §12.6 | MTF 슬랜트 에지 ESF 완전 구현 (IEC 62220-1-1) | GAP-T |
| §3.4.5 | Lag 잔류 기반 결정론적 티어링 | GAP-U |
| §5.3 | 해부 부위별 Virtual Grid 프리셋 (15개 부위) | GAP-V |
| §8.4 | AI Worker 격리 아키텍처 (ONNX + 폴백) | GAP-W |
| §9.5 | 교정 드리프트 모니터링 | GAP-X |

**v1.3 Round 4 (GAP-Y~AH):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §14 | Fluoroscopy 시간적 재귀 IIR 필터 (AVX2 FMA, α 적응형) | GAP-Y |
| §3.9 | Beam Hardening Correction (PMMA 팬텀, OD 도메인 다항식) | GAP-Z |
| §3.10 | Geometric Distortion Correction (Brown-Conrady, 역 LUT) | GAP-AA |
| §9.7 | Pixel Binning Mode 교정 보간 (gain 블록 평균, defect 전파) | GAP-AB |
| §10.7 | Memory Arena Zero-Copy 아키텍처 (8-슬롯 링 버퍼, CAS) | GAP-AC |
| §10.8 | Multi-Channel SPSC Thread Safety (lock-free, CPU affinity) | GAP-AD |
| §12.8 | Automatic CNR Auto-Assessment (히스토그램 IQI) | GAP-AE |
| §6.4 | Anatomy-Adaptive Auto Window/Level (5종 퍼센타일 테이블) | GAP-AF |
| §9.8 | Multi-Frame Sigma-Clipping 교정 (Python NumPy, κ=3.0) | GAP-AG |
| §15 | Error Code Taxonomy (32개 코드, xpe_error_string, C# 핸들러) | GAP-AH |

**v1.4 Round 5 (GAP-AI~AR):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §3.4.6 | Real-Time GCR Estimator (슬라이딩 윈도우 EMA, 0.2% 임계값) | GAP-AI |
| §3.4.7 | NLCSC State Machine (비선형 전하 누적 4차 다항식, kVp 의존) | GAP-AJ |
| §3.3.5 | Dose-Dependent Dynamic Defect Detection (4선량 z-score, R²) | GAP-AM |
| §3.11 | Row/Column FPN Correction (3패스 반복, AVX2 중앙값) | GAP-AK |
| §4.8 | Wavelet BayesShrink Denoising (db4 3레벨, MAD σ_n) | GAP-AP |
| §5.4 | Scatter SPR Boone-Seibert Model (Beer-Lambert 두께 역산) | GAP-AO |
| §9.9 | Multi-Exponential Lag Parameter Fitting (LM 최소제곱, Python) | GAP-AN |
| §12.9 | Allan Variance Stability Characterization (잡음 유형 분류) | GAP-AL |
| §16 | Dual-Energy Subtraction (위상 상관 모션 보정, 로그 차감) | GAP-AQ |
| §17 | DICOM IOD Conformance Validation (Type 1/2/3 + 픽셀 수치) | GAP-AR |

**v1.5 Round 6 (GAP-AS~BB):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §18 | 지각적 화질 품질 지표 (PSNR/SSIM/MS-SSIM/FSIM) | GAP-AS |
| §3.12 | 온도 보상 이득 보정 (a-Si:H α_T = 0.0015/°C) | GAP-AT |
| §3.13 | 2D FFT 노치 필터 (주기적 구조 잡음 제거) | GAP-AU |
| §9.10 | AEC 피드백 루프 (EI/DI → kVp/mAs 자동 조정) | GAP-AV |
| §9.11 | 교정 SPC (Shewhart + CUSUM 제어 차트) | GAP-AW |
| §14.2 | 서브픽셀 ECC 영상 정합 (DSA/형광투시 용) | GAP-AX |
| §11.5 | 양자 잡음 모델 (Poisson+Gaussian, Anscombe 변환) | GAP-AY |
| §5.5 | 무아레 아티팩트 검출 및 방향성 대역 제거 | GAP-AZ |
| §17.2 | DICOM SR CAD 소견 출력 (TID 1500/4100) | GAP-BA |
| §12.10 | IEC 61223-3-5 인수 시험 자동화 (T1~T6) | GAP-BB |

**v1.6 Round 7 (GAP-BC~BL):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §9.12 | 방사선량-면적곱(DAP) 및 KERMA 누적 추적 (IEC 60601-2-54) | GAP-BC |
| §17.3 | JPEG 2000 무손실/근손실 압축 (OpenJPEG 2.5) | GAP-BD |
| §3.14 | 모션 블러 PSF 추정 및 위너 역필터 | GAP-BE |
| §3.15 | 금속 고밀도 아티팩트 마스크 생성 | GAP-BF |
| §19 | 선형 토모합성 FBP/SAA 재구성 | GAP-BG |
| §8.3.2 | RANSAC+ORB 키포인트 파노라마 스티칭 개선 | GAP-BH |
| §4.9 | 가우시안/라플라시안 다중 해상도 피라미드 | GAP-BI |
| §10.9 | GPU CUDA 파이프라인 가속 아키텍처 | GAP-BJ |
| §12.11 | 자동 QA 팬텀 인식 알고리즘 (Leeds/CDRAD/CIRS) | GAP-BK |
| §9.13 | 교정 전달 함수 (Cross-FPD 패널 정규화) | GAP-BL |

**v1.7 Round 8 (GAP-BM~BV):**

| 섹션 | 알고리즘 | 공백 |
|------|---------|------|
| §6.5 | DICOM GSDF 그레이스케일 표준 디스플레이 함수 (PS 3.14, JND 보정) | GAP-BM |
| §6.6 | Multi-Scale Retinex 로컬 톤 매핑 (σ={15,80,250}px) | GAP-BN |
| §8.5 | U-Net 폐 영역 자동 분할 (IoU≥0.92, Non-SaMD) | GAP-BO |
| §8.6 | DLIR CNN 저선량 화질 복원 (RDN 16블록, PSNR≥38dB) | GAP-BP |
| §8.7 | 흉부 늑골 억제 (Hessian Frangi 능선 척도, 억제 지수≥80%) | GAP-BQ |
| §8.8 | 자동 해부학적 부위 인식 CNN (MobileNetV3, 10클래스, 정확도≥95%) | GAP-BR |
| §14.3 | 피라미달 Lucas-Kanade 광학 흐름 (3레벨, AVX2, <5ms/프레임) | GAP-BS |
| §3.16 | 통합 시간 선형성 보정 (a-Si:H TFT, 16-노드 PWL, ε<0.1%) | GAP-BT |
| §20 | TV-최소화 반복적 영상 복원 (ADMM, FFT 가속, PSNR+3dB) | GAP-BU |
| §20.1 | 골밀도 정량화 DXA-proxy (DES 기반, r²>0.85, Non-SaMD) | GAP-BV |

### 4.2 문서 관계

```
XPE-SRS-001 (요구사항)
    ↓ 추적
XPE-SAD-001 (아키텍처)
    ↓ 추적
XPE-SDD-001 (단위 식별) + XPE-SDD-002 (상세 설계)
    ↓ 알고리즘 상세
XPE-ALG-001 (통합 알고리즘 명세)  ←→  xpe-algorithm-spec-deepsync.md (계약 수준 명세)
    ↓ 검증
XPE-STP-001 (테스트 계획)
```

### 4.3 알고리즘-SRS 요구사항 추적

| 알고리즘 | SRS ID | DLL |
|---------|--------|-----|
| Readout Validation | SRS-QC-001 | xpe_preprocess.dll |
| Non-linearity Correction | SRS-FUNC-001b | xpe_preprocess.dll |
| Offset Correction | SRS-FUNC-001 | xpe_preprocess.dll |
| Gain Correction | SRS-FUNC-002 | xpe_preprocess.dll |
| Multi-SID Gain Interpolation | SRS-FUNC-002 ext | xpe_preprocess.dll |
| Heel Effect Compensation | SRS-FUNC-002b | xpe_preprocess.dll |
| Defect Correction | SRS-FUNC-003 | xpe_preprocess.dll |
| Ghost/Lag Correction | SRS-FUNC-004 | xpe_preprocess.dll |
| Lag Residual Tiering | SRS-FUNC-004 ext | xpe_preprocess.dll |
| Calibration Session Lock | SRS-SEC-002 ext | xpe_common.dll |
| Calibration Drift Monitor | SRS-QC-002 | xpe_common.dll |
| Log Transform | SRS-FUNC-010 | xpe_enhance_basic.dll |
| Bilateral Filter | SRS-FUNC-011 | xpe_enhance_basic.dll |
| CLAHE | SRS-FUNC-012 | xpe_enhance_basic.dll |
| Edge Enhancement | SRS-FUNC-013 | xpe_enhance_advanced.dll |
| Grid Suppression (NSCT) | Phase 2 SRS TBD | xpe_gsvg.dll |
| Virtual Grid | Phase 2 SRS TBD | xpe_gsvg.dll |
| VG Anatomy Presets | SRS-FUNC-008b | xpe_gsvg.dll |
| EI / DI | SRS-FUNC-009 | xpe_enhance_advanced.dll |
| NPS / DQE | SRS-MEAS-001 | xpe_enhance_advanced.dll |
| MTF ESF Pipeline | SRS-MEAS-002 | xpe_enhance_advanced.dll |
| Collimation Mask | Phase 2 SRS TBD | xpe_enhance_advanced.dll |
| Quality State Sidecar | SRS-QC-003 | xpe_common.dll |
| Scalar Parity Harness | SRS-TEST-001 | (test framework) |
| AI Worker Isolation | SRS-AI-001 | xpe_ai.dll |
| ONNX Model Manifest | SRS-AI-002 | xpe_ai_worker.exe |
| Panoramic Stitch | SRS-FUNC-017 | xpe_ai.dll |
| Bone Suppression | SRS-FUNC-018 | xpe_ai.dll |
| Temporal IIR Filter | SRS-FLUORO-001 | xpe_preprocess.dll |
| Beam Hardening Correction | SRS-FUNC-003b | xpe_preprocess.dll |
| Geometric Distortion Correction | SRS-FUNC-005b | xpe_preprocess.dll |
| Binning Mode Calibration | SRS-FUNC-002c | xpe_preprocess.dll |
| Sigma-Clipping Calibration | SRS-CAL-001b | (offline, Python) |
| Memory Arena Architecture | SRS-PERF-001 | xpe_common.dll |
| Pipeline Thread Safety | SRS-PERF-002 | xpe_common.dll |
| Auto CNR Assessment | SRS-MEAS-003 | xpe_enhance_advanced.dll |
| Auto Window/Level (Anatomy) | SRS-FUNC-021b | xpe_display.dll |
| Error Code Taxonomy | SRS-ERR-001 | xpe_common.dll |
| GCR Estimator | SRS-FUNC-004 ext | xpe_preprocess.dll |
| NLCSC State Machine | SRS-FUNC-004 ext | xpe_preprocess.dll |
| Row/Column FPN Correction | SRS-FUNC-001 ext | xpe_preprocess.dll |
| Allan Variance Stability | SRS-QC-002 ext | (offline, Python) |
| Dose-Dependent Defect Detection | SRS-FUNC-003 ext | (offline, Python) |
| Multi-Exponential Lag Fitting | SRS-FUNC-004 | (offline, Python) |
| Scatter SPR Model | SRS-FUNC-008 | xpe_gsvg.dll |
| Wavelet BayesShrink Denoising | SRS-FUNC-011b | xpe_enhance_advanced.dll |
| Dual-Energy Subtraction | SRS-FUNC-019 | xpe_enhance_advanced.dll |
| DICOM IOD Conformance Validation | SRS-DICOM-001 | xpe_dicom.dll |
| Perceptual IQM (PSNR/SSIM/MS-SSIM/FSIM) | SRS-MEAS-004 | xpe_enhance_advanced.dll |
| Temperature-Compensated Gain | SRS-FUNC-002d | xpe_preprocess.dll |
| 2D FFT Notch Filter | SRS-FUNC-001c | xpe_preprocess.dll |
| AEC Feedback Loop | SRS-FUNC-009b | xpe_enhance_advanced.dll |
| SPC Calibration Control (Shewhart/CUSUM) | SRS-QC-004 | xpe_common.dll |
| Sub-pixel ECC Registration | SRS-FLUORO-002 | xpe_preprocess.dll |
| Quantum Noise Model (Poisson+Anscombe) | SRS-FUNC-011c | xpe_enhance_advanced.dll |
| Moiré Artifact Suppression | SRS-FUNC-008c | xpe_gsvg.dll |
| DICOM SR for CAD Findings | SRS-DICOM-002 | xpe_dicom.dll |
| IEC 61223 Acceptance Testing | SRS-QA-001 | xpe_enhance_advanced.dll |
| DAP/KERMA Dose Tracking | SRS-DOSE-001 | xpe_common.dll |
| JPEG 2000 Compression | SRS-DICOM-003 | xpe_dicom.dll |
| Motion Blur Wiener Deblur | SRS-FUNC-001d | xpe_preprocess.dll |
| Metal Artifact Mask | SRS-FUNC-001e | xpe_preprocess.dll |
| Linear Tomosynthesis FBP/SAA | SRS-TOMO-001 | xpe_enhance_advanced.dll |
| RANSAC+ORB Panoramic Stitch | SRS-FUNC-017b | xpe_ai.dll |
| Gaussian/Laplacian Pyramid | SRS-FUNC-014b | xpe_enhance_advanced.dll |
| GPU CUDA Pipeline Acceleration | SRS-PERF-003 | xpe_preprocess.dll (CUDA) |
| Auto QA Phantom Recognition | SRS-QA-002 | xpe_enhance_advanced.dll |
| Cross-FPD Calibration Transfer | SRS-CAL-002 | xpe_preprocess.dll |
| DICOM GSDF Display Calibration | SRS-DISP-005 | xpe_display.dll |
| Multi-Scale Retinex Local Tone Mapping | SRS-DISP-004 | xpe_display.dll |
| U-Net Lung Field Segmentation | SRS-SEG-001 | xpe_ai_worker.dll |
| DLIR CNN Low-Dose Denoising | SRS-DLIR-001 | xpe_ai_worker.dll |
| Rib Suppression (Hessian) | SRS-RIB-001 | xpe_ai_worker.dll |
| Body Part Recognition CNN | SRS-ANAT-001 | xpe_ai_worker.dll |
| Lucas-Kanade Optical Flow | SRS-FLUORO-003 | xpe_fluoroscopy.dll |
| Integration Nonlinearity Correction | SRS-FUNC-001f | xpe_preprocess.dll |
| TV-ADMM Iterative Denoising | SRS-ITER-001 | xpe_enhance_advanced.dll |
| BMD DXA-proxy Estimation | SRS-BMD-001 | xpe_enhance_advanced.dll |

---

## 5. IEC 62304 적용 범위 요약

| Clause | Description | 담당 문서 |
|--------|-------------|---------|
| 5.1 | Software Development Planning | SDP-001 |
| 5.2 | Software Requirements | SRS-001 |
| 5.3 | Software Architecture | SAD-001 |
| 5.4 | Software Detailed Design | SDD-001, SDD-002, **ALG-001** |
| 5.5 | Software Unit Implementation & Testing | STP-001 |
| 5.6 | Software Integration & Testing | ITP-001 |
| 5.7 | Software System Testing | VVP-001 |
| 5.8 | Software Release | RTM-001, SRP-001 |
| 6.1 | Configuration Management | SCM-001 |
| 7 | Risk Management | SRM-001, SHA-001 |
| 8 | SOUP Controls | SOUP-001 |
| 9 | Problem Resolution | SPR-001 |
| 12 | Software Maintenance | SMP-001 |

---

## 6. 관련 모듈

| 모듈 | 관계 | 위치 |
|------|------|------|
| GSVG | XPE Phase 2 서브시스템 (Grid/Virtual Grid) | [../gsvg/](../gsvg/) |
| Ghost Correction | XPE SWU-1.4 상세 규격 | [../ghost-correction/](../ghost-correction/) |
| Calibration | XPE 교정 파이프라인 (§9) | [../calibration/](../calibration/) |
| Panel Defect | XPE SWU-1.3 상세 규격 | [../panel-defect/](../panel-defect/) |
| Enhance Basic | XPE Phase 1b 향상 | [../enhance-basic/](../enhance-basic/) |
| Enhance Advanced | XPE Phase 2 향상 | [../enhance-advanced/](../enhance-advanced/) |
| AI Module | XPE Phase 3 AI | [../ai-module/](../ai-module/) |
| Display | XPE 표시 처리 | [../display/](../display/) |
| DICOM | XPE DICOM I/O | [../dicom/](../dicom/) |

---

## Change Log

| Date | Version | Changes |
|------|---------|---------|
| 2026-04-15 | 1.5.0 | XPE-ALG-001 v1.6 반영. Round 7 GAP-BC~BL 해소 반영 (10건). §19 토모합성 신설, §17.3 JPEG2000, §10.9 GPU CUDA, §12.11 팬텀 인식 등 추가. 알고리즘 빠른 참조 테이블 확장 (70건). SRS ID 추가 (SRS-DOSE-001, SRS-DICOM-003, SRS-FUNC-001d/e, SRS-TOMO-001, SRS-FUNC-017b/014b, SRS-PERF-003, SRS-QA-002, SRS-CAL-002). |
| 2026-04-15 | 1.4.0 | XPE-ALG-001 v1.5 반영. Round 6 GAP-AS~BB 해소 반영 (10건). §18 지각적 IQM, §12.10 IEC61223 인수 시험, §17.2 DICOM SR 신설. 알고리즘 빠른 참조 테이블 확장 (60건). SRS ID 추가 (SRS-MEAS-004, SRS-FUNC-002d/001c/009b/011c/008c, SRS-QC-004, SRS-FLUORO-002, SRS-DICOM-002, SRS-QA-001). |
| 2026-04-15 | 1.3.0 | XPE-ALG-001 v1.4 반영. Round 5 GAP-AI~AR 해소 반영 (10건). §16 DES, §17 DICOM IOD 신설. 알고리즘 빠른 참조 테이블 확장 (50건). SRS ID 추가 (SRS-FUNC-019, SRS-DICOM-001, SRS-FUNC-001/003/004/008/011b ext, SRS-QC-002 ext 등). |
| 2026-04-15 | 1.2.0 | XPE-ALG-001 v1.3 반영. Round 4 GAP-Y~AH 해소 반영 (10건). §14 Fluoroscopy IIR, §15 Error Code Taxonomy 신설. 알고리즘 빠른 참조 테이블 확장 (40건). SRS ID 추가 (SRS-FLUORO-001, SRS-PERF-001/002, SRS-MEAS-003, SRS-ERR-001 등). |
| 2026-04-15 | 1.1.0 | XPE-ALG-001 v1.2 반영. Round 3 GAP-O~X 해소 반영. 알고리즘 빠른 참조 테이블 확장 (30건). SRS ID 추가 (SRS-FUNC-002b, SRS-QC-002/003, SRS-AI-001/002, SRS-MEAS-002, SRS-TEST-001 등). |
| 2026-04-15 | 1.0.0 | 신규 생성. XPE-ALG-001 v1.1 통합 반영. 23개 문서 목록 완성. |
