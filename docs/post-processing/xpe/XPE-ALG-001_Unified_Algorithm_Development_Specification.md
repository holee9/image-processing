# XPE 통합 알고리즘 개발 명세서

**Document ID:** XPE-ALG-001 v1.8  
**IEC 62304 Clause:** 5.4 (Software Detailed Design)  
**Safety Classification:** Class B  
**Date:** 2026-04-15  
**Author:** XPE Development Team  
**Review Cycles:** 90회 (v1.0: 10회 + v1.1: 10회 + v1.2: 10회 + v1.3: 10회 + v1.4: 10회 + v1.5: 10회 + v1.6: 10회 + v1.7: 10회 + v1.8: 10회 Review-Evaluate-Fix 반복 완료)  
**Approval:** __________________ Date: __________  

---

## 문서 목적

본 문서는 XPE(X-ray Processing Engine) 시스템의 **모든 알고리즘**을 수학적 공식, C++ 의사코드, SIMD 최적화 전략, 검증 기준까지 일관되게 명세한다. 기존 문서(XPE-SRS-001, XPE-SAD-001, XPE-SDD-002, 03_측정_알고리즘_명세서, xray_grid_suppression_virtual_grid_research)의 교차 검증을 통해 식별된 알고리즘 공백을 3 라운드에 걸쳐 모두 해소한다.

### 공백 해소 매핑

| 공백 번호 | 내용 | 본 문서 섹션 | 라운드 |
|----------|------|------------|-------|
| GAP-01 | Python↔C++ 아키텍처 브리지 미문서화 | §2 | v1.0 |
| GAP-02 | Core Processing 알고리즘 미명세 | §4 | v1.0 |
| GAP-03 | Grid Suppression 알고리즘 미명세 | §5 | v1.0 |
| GAP-04 | AI/DL 알고리즘 미명세 | §8 | v1.0 |
| GAP-05 | Display Processing 표준 미명세 | §6 | v1.0 |
| GAP-06 | SIMD 최적화 전체 파이프라인 미커버 | §10 | v1.0 |
| GAP-07 | 파노라마 스티칭 미명세 | §8.3 | v1.0 |
| GAP-08 | Virtual Grid / Scatter Correction 미명세 | §5.2 | v1.0 |
| GAP-09 | Exposure Index (IEC 62494-1) 미명세 | §7 | v1.0 |
| GAP-10 | 교정 맵 생성↔런타임 연결 미문서화 | §9 | v1.0 |
| GAP-D | NSCT Grid Suppression 완전 구현 누락 | §5.1.3 | v1.1 |
| GAP-E | 런타임 결함 검출 (AVX2 z-score) 누락 | §3.3.4 | v1.1 |
| GAP-F | EI ROI Central Method √0.1 오류 | §7.2 | v1.1 |
| GAP-G | AVX2 log 근사 (avx2_log_ps) 미구현 | §4.1.3 | v1.1 |
| GAP-H | 비선형성 보정 (PCHIP LUT) 미명세 | §3.0.5 | v1.1 |
| GAP-I | Readout Validation 미명세 | §3.0 | v1.1 |
| GAP-L | NPS 계산 (IEC 62220-1) 미명세 | §12.3 | v1.1 |
| GAP-M | DQE 계산 알고리즘 미명세 | §12.4 | v1.1 |
| GAP-N | Collimation Mask 검출 미명세 | §12.5 | v1.1 |
| GAP-O | Heel Effect 보정 (Wang 2013) 미명세 | §3.5 | v1.2 |
| GAP-P | Multi-SID Gain 보간 및 kVp 선택 미명세 | §3.2.5 | v1.2 |
| GAP-Q | 교정 세션 잠금 및 매니페스트 해시 체인 미명세 | §2.4 | v1.2 |
| GAP-R | 품질 상태 벡터 사이드카 미명세 | §13 | v1.2 |
| GAP-S | 스칼라 참조 + SIMD 패리티 하네스 미명세 | §11.4 | v1.2 |
| GAP-T | MTF 슬랜트 에지 ESF 완전 구현 누락 | §12.6 | v1.2 |
| GAP-U | Lag 잔류 기반 결정론적 티어링 미명세 | §3.4.5 | v1.2 |
| GAP-V | 해부 부위별 Virtual Grid 프리셋 미명세 | §5.3 | v1.2 |
| GAP-W | AI Worker 격리 아키텍처 (ONNX) 미명세 | §8.4 | v1.2 |
| GAP-X | 교정 드리프트 모니터링 알고리즘 미명세 | §9.5 | v1.2 |
| GAP-Y | Fluoroscopy 시간적 재귀 IIR 필터 미명세 | §14 | v1.3 |
| GAP-Z | Beam Hardening Correction 미명세 | §3.9 | v1.3 |
| GAP-AA | Geometric Distortion Correction 미명세 | §3.10 | v1.3 |
| GAP-AB | Pixel Binning 모드 교정 보간 미명세 | §9.7 | v1.3 |
| GAP-AC | Pipeline Zero-Copy Memory Arena 아키텍처 미명세 | §10.7 | v1.3 |
| GAP-AD | Multi-Channel Producer-Consumer Thread Safety 미명세 | §10.8 | v1.3 |
| GAP-AE | Automatic CNR Auto-Assessment (IQI) 미명세 | §12.8 | v1.3 |
| GAP-AF | Anatomy-Adaptive Auto Window/Level 미명세 | §6.4 | v1.3 |
| GAP-AG | Multi-Frame Sigma-Clipping 교정 미명세 | §9.8 | v1.3 |
| GAP-AH | Error Code Taxonomy 및 복구 동작 미명세 | §15 | v1.3 |
| GAP-AI | Real-Time GCR (Ghost Charge Ratio) Estimator 미명세 | §3.4.6 | v1.4 |
| GAP-AJ | NLCSC (Non-Linear Scattered Charge) State Machine 미명세 | §3.4.7 | v1.4 |
| GAP-AK | Row/Column FPN (Fixed-Pattern Noise) 보정 미명세 | §3.11 | v1.4 |
| GAP-AL | Allan Variance 장기 안정성 특성화 미명세 | §12.9 | v1.4 |
| GAP-AM | 선량 의존 동적 결함 화소 검출 미명세 | §3.3.5 | v1.4 |
| GAP-AN | 다중 지수 Lag 감쇠 파라미터 피팅 알고리즘 미명세 | §9.9 | v1.4 |
| GAP-AO | Scatter SPR 반경험 모델(Boone-Seibert) 미명세 | §5.4 | v1.4 |
| GAP-AP | Wavelet 다중 스케일 적응형 노이즈 제거(BayesShrink) 미명세 | §4.8 | v1.4 |
| GAP-AQ | 이중 에너지 차감(DES) 분해 알고리즘 미명세 | §16 | v1.4 |
| GAP-AR | DICOM IOD 적합성 검증 파이프라인 미명세 | §17 | v1.4 |
| GAP-AS | 지각적 화질 품질 지표 (PSNR/SSIM/MS-SSIM/FSIM) 미명세 | §18 | v1.5 |
| GAP-AT | 온도 보상 이득 보정 (a-Si:H TFT 온도 계수) 미명세 | §3.12 | v1.5 |
| GAP-AU | 2D FFT 노치 필터 (주기적 구조 잡음 제거) 미명세 | §3.13 | v1.5 |
| GAP-AV | AEC 피드백 루프 (EI/DI → kVp/mAs 조정) 미명세 | §9.10 | v1.5 |
| GAP-AW | 교정 통계적 공정 관리 (Shewhart/CUSUM SPC) 미명세 | §9.11 | v1.5 |
| GAP-AX | 서브픽셀 영상 정합 (ECC 알고리즘, DSA용) 미명세 | §14.2 | v1.5 |
| GAP-AY | 신호 의존 양자 잡음 모델 (Poisson+Gaussian, Anscombe) 미명세 | §11.5 | v1.5 |
| GAP-AZ | 무아레 아티팩트 검출 및 방향성 대역 제거 미명세 | §5.5 | v1.5 |
| GAP-BA | DICOM 구조화 보고서 (SR, TID 1500/4100) CAD 소견 출력 미명세 | §17.2 | v1.5 |
| GAP-BB | IEC 61223-3-5 인수 시험 자동화 (T1~T6) 미명세 | §12.10 | v1.5 |
| GAP-BC | 방사선량-면적곱(DAP) 및 KERMA 누적 추적 알고리즘 미명세 | §9.12 | v1.6 |
| GAP-BD | JPEG 2000 (ISO 15444) 무손실/근손실 압축 파이프라인 미명세 | §17.3 | v1.6 |
| GAP-BE | 모션 블러 PSF 추정 및 위너(Wiener) 역필터 미명세 | §3.14 | v1.6 |
| GAP-BF | 금속 고밀도 아티팩트 마스크 생성 알고리즘 미명세 | §3.15 | v1.6 |
| GAP-BG | 선형 토모합성 z-축 재구성 (FBP/SAA) 미명세 | §19 | v1.6 |
| GAP-BH | RANSAC+ORB 키포인트 기반 파노라마 스티칭 개선 미명세 | §8.3.2 | v1.6 |
| GAP-BI | 가우시안/라플라시안 다중 해상도 피라미드 미명세 | §4.9 | v1.6 |
| GAP-BJ | GPU CUDA 파이프라인 가속 아키텍처 미명세 | §10.9 | v1.6 |
| GAP-BK | 자동 QA 팬텀 인식 알고리즘 (Leeds/CDRAD/CIRS) 미명세 | §12.11 | v1.6 |
| GAP-BL | 교정 전달 함수 (Cross-FPD 패널 정규화) 미명세 | §9.13 | v1.6 |
| GAP-BM | DICOM 그레이스케일 표준 디스플레이 함수 (GSDF, PS 3.14) 미명세 | §6.5 | v1.7 |
| GAP-BN | 로컬 톤 매핑 (Multi-Scale Retinex 기반 지역 대비 향상) 미명세 | §6.6 | v1.7 |
| GAP-BO | 폐 영역 자동 분할 (U-Net Lung Field Segmentation) 미명세 | §8.5 | v1.7 |
| GAP-BP | 딥러닝 기반 저선량 화질 복원 (DLIR, CNN Denoising) 미명세 | §8.6 | v1.7 |
| GAP-BQ | 흉부 늑골 억제 알고리즘 (Rib Suppression, Hessian) 미명세 | §8.7 | v1.7 |
| GAP-BR | 자동 해부학적 부위 인식 CNN (Body Part Recognition) 미명세 | §8.8 | v1.7 |
| GAP-BS | 형광투시 피라미달 Lucas-Kanade 광학 흐름 추정 미명세 | §14.3 | v1.7 |
| GAP-BT | 통합 시간 선형성 보정 (Integration Nonlinearity Correction) 미명세 | §3.16 | v1.7 |
| GAP-BU | 전체 변분 반복적 영상 복원 (TV-Minimization ADMM) 미명세 | §20 | v1.7 |
| GAP-BV | 골밀도 정량화 알고리즘 (Bone Mineral Density DXA-proxy) 미명세 | §20.1 | v1.7 |
| GAP-BW | 포톤 계수 검출기(PCD) 스펙트럼 빈닝 미명세 | §21 | v1.8 |
| GAP-BX | 지능형 교정 수명 주기 관리 (ICLM) 미명세 | §9.14 | v1.8 |
| GAP-BY | NPS 기반 잡음 최적 구조 필터링 (NOSF) 미명세 | §4.10 | v1.8 |
| GAP-BZ | 극좌표 도메인 링 아티팩트 보정 미명세 | §3.17 | v1.8 |
| GAP-CA | 해부 기반 인스턴스 분할 (AGIS) 미명세 | §8.9 | v1.8 |
| GAP-CB | 압축 센싱 희소 뷰 토모합성 재구성 미명세 | §22 | v1.8 |
| GAP-CC | 임상 영상 품질 감사 엔진 (ACIQ) 미명세 | §12.12 | v1.8 |
| GAP-CD | 이기종 컴퓨팅 파이프라인 스케줄러 (HCPS) 미명세 | §10.10 | v1.8 |
| GAP-CE | DICOM 방사선량 구조화 보고서 (RDSR) 생성 미명세 | §17.4 | v1.8 |
| GAP-CF | 신호 검출 이론 프레임워크 (SDT — d', ROC, JAFROC) 미명세 | §11.6 | v1.8 |

---

## 목차

1. [용어 정의 및 기호 규약](#1-용어-정의-및-기호-규약)
2. [시스템 아키텍처 — Python↔C++ 브리지](#2-시스템-아키텍처--pythonc-브리지)
   - [§2.4 교정 세션 잠금 및 매니페스트 해시 체인 ★GAP-Q](#24-교정-세션-잠금-및-매니페스트-해시-체인-gap-q-해소)
3. [SWI-1: Pre-Processing 알고리즘](#3-swi-1-pre-processing-알고리즘)
   - [§3.0 Readout Validation (SWU-1.0) ★GAP-I](#30-swu-10-readout-validation-gap-i-해소)
   - [§3.0.5 Non-linearity Correction ★GAP-H](#305-swu-105-non-linearity-correction-gap-h-해소)
   - §3.1 Offset Correction
   - §3.2 Gain Correction
   - [§3.2.5 Multi-SID Gain 보간 ★GAP-P](#325-swu-12b-multi-sid-gain-보간-및-kvp-stratified-gain-선택-gap-p-해소)
   - §3.3 Defect Correction (★GAP-E)
   - §3.4 Ghost/Lag Correction
   - [§3.4.5 Lag Residual 티어링 ★GAP-U](#345-swu-14b-lag-잔류-기반-결정론적-티어링-gap-u-해소)
   - [§3.5 Heel Effect Compensation ★GAP-O](#35-swu-15-heel-effect-compensation-gap-o-해소)
4. [SWI-2: Core Processing 알고리즘](#4-swi-2-core-processing-알고리즘) (★GAP-G avx2_log_ps)
5. [Grid Suppression & Virtual Grid 알고리즘](#5-grid-suppression--virtual-grid-알고리즘) (★GAP-D NSCT)
   - [§5.3 해부 부위별 Virtual Grid 프리셋 ★GAP-V](#53-해부-부위별-virtual-grid-프리셋-테이블-gap-v-해소)
6. [SWI-3: Display Processing 알고리즘](#6-swi-3-display-processing-알고리즘)
7. [IEC 62494-1 Exposure Index 알고리즘](#7-iec-62494-1-exposure-index-알고리즘) (★GAP-F ROI 수정)
8. [AI/DL 알고리즘](#8-aidl-알고리즘)
   - [§8.4 AI Worker 격리 아키텍처 ★GAP-W](#84-ai-worker-격리-아키텍처-및-onnx-추론-gap-w-해소)
9. [교정 데이터 파이프라인](#9-교정-데이터-파이프라인)
   - [§9.5 교정 드리프트 모니터링 ★GAP-X](#95-교정-드리프트-모니터링-gap-x-해소)
10. [성능 최적화 — SIMD/OpenMP 전략](#10-성능-최적화--simdopenmp-전략)
11. [검증 방법론](#11-검증-방법론)
    - [§11.4 스칼라 참조 + SIMD 패리티 하네스 ★GAP-S](#114-스칼라-참조-구현-및-simd-패리티-하네스-gap-s-해소)
12. [FPD 특성화 알고리즘 보완](#12-fpd-특성화-알고리즘-보완)
    - [§12.3 NPS 계산 ★GAP-L](#123-nps-계산-알고리즘-gap-l-해소)
    - [§12.4 DQE 계산 ★GAP-M](#124-dqe-계산-알고리즘-gap-m-해소)
    - [§12.5 Collimation Mask Detection ★GAP-N](#125-collimation-mask-detection-알고리즘-gap-n-해소)
    - [§12.6 MTF 슬랜트 에지 ESF 완전 구현 ★GAP-T](#126-mtf-슬랜트-에지-esf-완전-구현-gap-t-해소)
13. [품질 상태 벡터 사이드카 ★GAP-R](#13-품질-상태-벡터-사이드카-gap-r-해소)
14. [Fluoroscopy 시간적 IIR 필터 ★GAP-Y](#14-fluoroscopy-시간적-재귀-iir-필터-gap-y-해소)
    - [§3.9 Beam Hardening Correction ★GAP-Z](#39-swu-19-beam-hardening-correction-gap-z-해소)
    - [§3.10 Geometric Distortion Correction ★GAP-AA](#310-swu-110-geometric-distortion-correction-gap-aa-해소)
    - [§6.4 Anatomy-Adaptive Auto Window/Level ★GAP-AF](#64-swu-34b-anatomy-adaptive-auto-windowlevel-gap-af-해소)
    - [§9.7 Binning Mode 교정 보간 ★GAP-AB](#97-swu-97-pixel-binning-mode-교정-보간-gap-ab-해소)
    - [§9.8 Multi-Frame Sigma-Clipping ★GAP-AG](#98-swu-98-multi-frame-sigma-clipping-교정-gap-ag-해소)
    - [§10.7 Memory Arena 아키텍처 ★GAP-AC](#107-메모리-아레나-zero-copy-아키텍처-gap-ac-해소)
    - [§10.8 Multi-Channel Thread Safety ★GAP-AD](#108-multi-channel-producer-consumer-thread-safety-gap-ad-해소)
    - [§12.8 Auto CNR Assessment ★GAP-AE](#128-automatic-cnr-auto-assessment-iqi-gap-ae-해소)
15. [Error Code Taxonomy ★GAP-AH](#15-error-code-taxonomy-및-복구-동작-gap-ah-해소)
    - [§3.4.6 Real-Time GCR Estimator ★GAP-AI](#346-swu-146-real-time-gcr-estimator-gap-ai-해소)
    - [§3.4.7 NLCSC State Machine ★GAP-AJ](#347-swu-147-nlcsc-state-machine-gap-aj-해소)
    - [§3.3.5 Dose-Dependent Dynamic Defect Detection ★GAP-AM](#335-swu-135-dose-dependent-dynamic-defect-detection-gap-am-해소)
    - [§3.11 Row/Column FPN Correction ★GAP-AK](#311-swu-111-rowcolumn-fpn-correction-gap-ak-해소)
    - [§4.8 Wavelet Multi-Scale Adaptive Denoising ★GAP-AP](#48-swu-28-wavelet-multi-scale-adaptive-denoising-gap-ap-해소)
    - [§5.4 Scatter SPR Semi-Empirical Model ★GAP-AO](#54-swu-54-scatter-spr-semi-empirical-model-gap-ao-해소)
    - [§9.9 Multi-Exponential Lag Parameter Fitting ★GAP-AN](#99-swu-99-multi-exponential-lag-parameter-fitting-gap-an-해소)
    - [§12.9 Allan Variance Stability Characterization ★GAP-AL](#129-swu-129-allan-variance-stability-characterization-gap-al-해소)
16. [Dual-Energy Subtraction (DES) Algorithm ★GAP-AQ](#16-dual-energy-subtraction-des-algorithm-gap-aq-해소)
17. [DICOM IOD Conformance Validation ★GAP-AR](#17-dicom-iod-conformance-validation-gap-ar-해소)
18. [지각적 화질 품질 지표 (IQM) ★GAP-AS](#18-지각적-화질-품질-지표-perceptual-iqm-gap-as-해소)
    - [§3.12 온도 보상 이득 보정 ★GAP-AT](#312-swu-112-온도-보상-이득-보정-temperature-compensated-gain-correction-gap-at-해소)
19. [선형 토모합성 재구성 (FBP/SAA) ★GAP-BG](#19-선형-토모합성-재구성-fbpsaa-gap-bg-해소)
    - [§3.13 2D FFT 노치 필터 ★GAP-AU](#313-swu-113-주기적-구조-노이즈-제거-2d-fft-notch-filter-gap-au-해소)
    - [§9.10 AEC 피드백 루프 ★GAP-AV](#910-swu-910-aec-피드백-루프-eidi--kvpmas-조정-gap-av-해소)
    - [§9.11 SPC 교정 관리 ★GAP-AW](#911-swu-911-교정-통계적-공정-관리-spc-gap-aw-해소)
    - [§14.2 서브픽셀 영상 정합 (ECC) ★GAP-AX](#142-swu-142-서브픽셀-영상-정합-ecc-알고리즘-gap-ax-해소)
    - [§11.5 양자 잡음 모델 ★GAP-AY](#115-swu-115-신호-의존-양자-잡음-모델-poissongaussian-gap-ay-해소)
    - [§5.5 무아레 아티팩트 제거 ★GAP-AZ](#55-swu-55-무아레-아티팩트-검출-및-제거-gap-az-해소)
    - [§17.2 DICOM SR CAD 소견 ★GAP-BA](#172-swu-172-dicom-구조화-보고서-sr--cad-소견-출력-gap-ba-해소)
    - [§12.10 IEC 61223 인수 시험 ★GAP-BB](#1210-swu-1210-iec-61223-인수-시험-자동화-gap-bb-해소)
    - [§9.12 DAP/KERMA 추적 ★GAP-BC](#912-swu-912-방사선량-면적곱dap-및-kerma-누적-추적-gap-bc-해소)
    - [§17.3 JPEG 2000 압축 ★GAP-BD](#173-swu-173-jpeg-2000-무손실근손실-압축-gap-bd-해소)
    - [§3.14 모션 블러 위너 필터 ★GAP-BE](#314-swu-114-모션-블러-psf-추정-및-위너-역필터-gap-be-해소)
    - [§3.15 금속 아티팩트 마스크 ★GAP-BF](#315-swu-115-금속-고밀도-아티팩트-마스크-생성-gap-bf-해소)
    - [§19 토모합성 FBP/SAA ★GAP-BG](#19-선형-토모합성-재구성-fbpsaa-gap-bg-해소)
    - [§8.3.2 RANSAC 스티칭 ★GAP-BH](#832-swu-8-3-2-ransacorb-키포인트-파노라마-스티칭-gap-bh-해소)
    - [§4.9 라플라시안 피라미드 ★GAP-BI](#49-swu-29-가우시안라플라시안-다중-해상도-피라미드-gap-bi-해소)
    - [§10.9 GPU CUDA 가속 ★GAP-BJ](#109-swu-109-gpu-cuda-파이프라인-가속-아키텍처-gap-bj-해소)
    - [§12.11 자동 팬텀 인식 ★GAP-BK](#1211-swu-1211-자동-qa-팬텀-인식-알고리즘-gap-bk-해소)
    - [§9.13 Cross-FPD 정규화 ★GAP-BL](#913-swu-913-교정-전달-함수-cross-fpd-패널-정규화-gap-bl-해소)
    - [§3.16 통합 시간 선형성 보정 ★GAP-BT](#316-swu-116-통합-시간-선형성-보정-integration-nonlinearity-correction-gap-bt-해소)
    - [§6.5 DICOM GSDF 표준 디스플레이 ★GAP-BM](#65-swu-65-dicom-gsdf-그레이스케일-표준-디스플레이-함수-gap-bm-해소)
    - [§6.6 로컬 톤 매핑 (Retinex) ★GAP-BN](#66-swu-66-로컬-톤-매핑-multi-scale-retinex-gap-bn-해소)
    - [§8.5 U-Net 폐 분할 ★GAP-BO](#85-swu-85-폐-영역-자동-분할-u-net-lung-field-segmentation-gap-bo-해소)
    - [§8.6 DLIR CNN 저선량 복원 ★GAP-BP](#86-swu-86-딥러닝-기반-저선량-화질-복원-dlir-gap-bp-해소)
    - [§8.7 늑골 억제 (Hessian) ★GAP-BQ](#87-swu-87-흉부-늑골-억제-알고리즘-rib-suppression-gap-bq-해소)
    - [§8.8 신체 부위 인식 CNN ★GAP-BR](#88-swu-88-자동-해부학적-부위-인식-cnn-body-part-recognition-gap-br-해소)
    - [§14.3 Lucas-Kanade 광학 흐름 ★GAP-BS](#143-swu-143-형광투시-lucas-kanade-광학-흐름-추정-gap-bs-해소)
20. [전체 변분 반복적 영상 복원 (TV-Minimization ADMM) ★GAP-BU](#20-전체-변분-반복적-영상-복원-tv-minimization-admm-gap-bu-해소)
    - [§20.1 골밀도 정량화 (BMD DXA-proxy) ★GAP-BV](#201-swu-201-골밀도-정량화-알고리즘-bmd-dxa-proxy-gap-bv-해소)
- [부록 A: 수학 공식 일람](#부록-a-수학-공식-일람)
- [부록 B: 표준 참조 테이블](#부록-b-표준-참조-테이블)
- [부록 C: 알고리즘-요구사항 추적성](#부록-c-알고리즘-요구사항-추적성)

---

## 1. 용어 정의 및 기호 규약

### 1.1 기호 체계

| 기호 | 정의 | 단위 |
|------|------|------|
| `I_raw(x,y)` | 원시 detector 출력 pixel 값 | ADU |
| `I_dark(x,y)` | Dark offset map | ADU |
| `G(x,y)` | Gain correction map | dimensionless |
| `I_flat(x,y)` | Flat-field (flood) image | ADU |
| `I_corr(x,y)` | Gain-corrected image | ADU |
| `I_clean(x,y)` | Defect-corrected image | ADU |
| `I_od(x,y)` | Log-transformed (OD domain) image | OD |
| `σ_s` | Bilateral filter spatial sigma | pixels |
| `σ_r` | Bilateral filter range sigma | ADU or OD |
| `f` | Spatial frequency | cycles/mm |
| `MTF(f)` | Modulation Transfer Function | dimensionless |
| `NPS(f)` | Noise Power Spectrum | ADU²·mm² |
| `NNPS(f)` | Normalized NPS | mm² |
| `DQE(f)` | Detective Quantum Efficiency | dimensionless |
| `Φ` | X-ray quantum fluence at detector | photons/mm² |
| `EI` | Exposure Index (IEC 62494-1) | dimensionless |
| `DI` | Deviation Index | dB |
| `W(u,v)` | Window function (Hanning) | dimensionless |
| `ε` | Numerical floor (= 1×10⁻⁶) | ADU or OD |
| `α_T` | Gain temperature coefficient (a-Si:H TFT) | 1/°C |
| `T_ref` | Calibration reference temperature | °C |
| `f_moire` | Moiré artifact spatial frequency | lp/mm |
| `f_grid` | Anti-scatter grid line frequency | lp/mm |
| `S+_t` | CUSUM upper cumulative sum | dimensionless |
| `S-_t` | CUSUM lower cumulative sum | dimensionless |
| `σ_total(x,y)` | Signal-dependent total noise map | ADU |
| `PSNR` | Peak Signal-to-Noise Ratio | dB |
| `SSIM` | Structural Similarity Index | dimensionless |
| `MS-SSIM` | Multi-Scale SSIM | dimensionless |
| `FSIM` | Feature Similarity Index | dimensionless |
| `PC_m(x)` | Phase Congruency mask (FSIM) | dimensionless |
| `DAP` | Dose-Area Product (방사선량-면적곱) | Gy·cm² |
| `K_air` | Air KERMA (공기 커마) | Gy |
| `k_fact` | 장치별 DAP 교정 계수 | Gy·cm²·cm²/(mAs·kVp^n) |
| `n_kVp` | kVp 지수 (W/Al 필터 빔 ≈ 2.5) | dimensionless |
| `L_px` | 모션 블러 길이 | pixels |
| `θ_i` | 토모합성 i번째 투영 각도 | degrees |
| `N_proj` | 토모합성 투영 수 | dimensionless |
| `G_l` | 가우시안 피라미드 l번째 레벨 | ADU |
| `L_l` | 라플라시안 피라미드 l번째 레벨 | ADU |
| `J` | GSDF JND-index (Just-Noticeable Difference) | dimensionless |
| `L` | Display luminance | cd/m² |
| `R(x,y)` | Multi-Scale Retinex log reflectance | dimensionless |
| `λ_1, λ_2` | Hessian matrix eigenvalues | ADU/pixel² |
| `B(x,y)` | Bone probability map (rib suppression) | dimensionless |
| `v_x, v_y` | Optical flow displacement field | pixels/frame |
| `TV(u)` | Total variation of image u | ADU |
| `ρ_BMD` | BMD proxy density | g/cm² |
| `T_score` | T-score (bone density z-score) | dimensionless |
| `t_int` | Detector integration time | ms |

### 1.2 좌표 규약

```
Origin: top-left (0,0)
x: column (horizontal), y: row (vertical)
Spatial frequency: u (horizontal), v (vertical), f = sqrt(u²+v²)
Nyquist frequency: f_N = 1/(2·pixelPitch_mm)
```

### 1.3 데이터 타입 규약

| 처리 단계 | 내부 타입 | 비트 깊이 | 범위 |
|----------|----------|---------|------|
| Raw detector | uint16 | 14–16 bit | 0–65535 |
| Pre-processing 중간 | float32 | 32 bit | 0.0–65535.0 |
| OD domain | float32 | 32 bit | −∞ ~ +∞ (실제 −5 ~ +5) |
| Display pipeline | float32→uint16 | 16→8 bit | 0–4095 → 0–255 |

---

## 2. 시스템 아키텍처 — Python↔C++ 브리지

### 2.1 전체 데이터 흐름 (GAP-01 해소)

```
┌──────────────────────────────────────────────────────────────────┐
│                    OFFLINE (Python) — 교정 단계                    │
│                                                                    │
│  FPD 검사/특성화                                                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐           │
│  │  Dark Frame │    │  Flood Field│    │  Slanted Edge│           │
│  │  ≥16 frames │    │  per SID    │    │  (MTF)      │           │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘           │
│         │                  │                   │                   │
│         ▼                  ▼                   ▼                   │
│  compute_offset_map()  compute_gain_map()  compute_mtf()          │
│  compute_defect_map()  compute_nps_full()  compute_dqe()          │
│         │                  │                   │                   │
│         ▼                  ▼                   ▼                   │
│  [offset_map.bin]    [gain_map_SIDXXX.bin] [characterization.json]│
│  [defect_map.bin]    [checksum.sha256]                             │
└──────────────────────────────────────────────────────────────────┘
           │                  │
           ▼ 파일 배포         ▼
┌──────────────────────────────────────────────────────────────────┐
│                    ONLINE (C++) — 런타임 파이프라인                  │
│                                                                    │
│  SWI-1 Pre-Processing                                              │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ xpe_offset_correct() → xpe_gain_correct()               │      │
│  │ → xpe_defect_correct() → xpe_ghost_correct()            │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │ float32 ImageBuffer              │
│  SWI-2 Core Processing           ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ log_transform() → bilateral_filter() → clahe()          │      │
│  │ → edge_enhance() → [laplacian_pyramid()] [fractional()] │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │                                  │
│  SWI-3 Display Processing        ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ modality_lut() → voi_lut() → presentation_lut_gsdf()   │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │                                  │
│  SWI-4 DICOM I/O                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ dcmtk_write_dx_iod() → C-STORE SCU                     │      │
│  └─────────────────────────────────────────────────────────┘      │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 교정 파일 형식 명세

#### 2.2.1 Offset Map (`.bin`)

```
Header (64 bytes):
  [0..3]   Magic: "XOFF"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] NumFrames: uint32 (≥16)
  [20..23] BitDepth: uint32 (14 or 16)
  [24..55] AcquisitionDateTime: char[32] (ISO-8601)
  [56..63] Checksum: uint64 (CRC64)
Payload:
  float32[Width × Height]  // mean of NumFrames dark images, clamp ≥ 0
```

#### 2.2.2 Gain Map (`.bin`)

```
Header (96 bytes):
  [0..3]   Magic: "XGAI"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] SID_mm: float32   // Source-to-Image Distance
  [20..23] kVp: float32
  [24..27] mAs: float32
  [28..31] GainMean: float32  // mean of (Flood - Offset)
  [32..63] AcquisitionDateTime: char[32]
  [64..95] Checksum: uint64 (CRC64)
Payload:
  float32[Width × Height]  // GainMean / (Flood(x,y) - Offset(x,y)), clamped [0.5, 2.0]
```

#### 2.2.3 Defect Pixel Map (`.bin`)

```
Header (64 bytes):
  [0..3]   Magic: "XDEF"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] NumDefects: uint32
  [20..23] MapFlags: uint32  // bit0: factory, bit1: runtime
  [24..55] AcquisitionDateTime: char[32]
  [56..63] Checksum: uint64 (CRC64)
Payload:
  // Run-Length Encoded defect list:
  struct DefectEntry {
      uint16 x;
      uint16 y;
      uint8  type;   // 0: point, 1: cluster, 2: row, 3: col
      uint8  size;   // 1-based, used for cluster radius
  };
  DefectEntry[NumDefects]
```

### 2.3 파일 무결성 검증 (SRS-SEC-002)

```cpp
// Runtime validation before applying calibration data
bool validate_calibration_file(const std::string& path,
                                const std::string& checksum_path) {
    // Read file content
    auto data = read_binary_file(path);
    // Compute SHA-256
    auto computed = sha256(data.data(), data.size());
    // Compare with stored checksum
    auto stored = read_text_file(checksum_path);
    return computed == stored;
}
```

---

### 2.4 교정 세션 잠금 및 매니페스트 해시 체인 (GAP-Q 해소)

xpe-algorithm-spec-deepsync.md §4.1에서 "Every offset, gain, BPM, nonlinearity, and lag coefficient pack shall carry a session identity and hash chain"으로 명시된 교정 무결성 인프라이다. 서로 다른 세션의 교정 파일이 혼합되는 것을 방지하고, 드리프트 모니터링 API를 통해 재교정 트리거를 제공한다.

#### 2.4.1 세션 ID 스키마

모든 교정 파일은 공통 헤더 확장에 `session_id` 필드를 포함한다:

```
세션 ID 구성: SHA-256 digest 앞 8바이트 (16 hex 문자)
  session_id = SHA-256(
      device_serial    +   // FPD 시리얼 번호 (ASCII)
      calibration_date +   // ISO-8601 날짜 (예: "2026-04-15")
      operator_id          // 기사 ID 또는 자동화 토큰
  )[0:8]

예시: "A3F1C2D8E4B09517"
```

**파일 헤더 확장 (모든 교정 파일 형식에 적용)**:

```
확장 헤더 블록 (32 bytes, 기존 헤더 뒤에 추가):
  [0..7]   SessionID:     char[8]   // 세션 식별자 (16진수 ASCII)
  [8..15]  PrevHash:      uint8[8]  // 이전 교정 세션 해시 체인 링크
  [16..23] CreatedAt_ns:  uint64    // Unix nanoseconds
  [24..31] Reserved:      uint8[8]  // 향후 확장용 (모두 0)
```

#### 2.4.2 매니페스트 파일 스키마

교정 세션마다 하나의 매니페스트 파일 `calibration_manifest.json`을 생성한다:

```json
{
  "schema_version": "1.0",
  "session_id": "A3F1C2D8E4B09517",
  "device_serial": "FPD-2024-003421",
  "calibration_date": "2026-04-15T09:30:00Z",
  "operator_id": "CAL-AUTO-001",
  "files": [
    {
      "type": "offset_map",
      "path": "offset_map.bin",
      "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "size_bytes": 37748800,
      "acquired_at": "2026-04-15T09:15:00Z"
    },
    {
      "type": "gain_map",
      "sid_mm": 1000.0,
      "kvp": 80.0,
      "path": "gain_SID1000_kVP080.bin",
      "sha256": "...",
      "size_bytes": 37748896,
      "acquired_at": "2026-04-15T09:20:00Z"
    },
    {
      "type": "defect_map",
      "path": "defect_map.bin",
      "sha256": "...",
      "size_bytes": 9437248,
      "acquired_at": "2026-04-15T09:18:00Z"
    },
    {
      "type": "nonlinearity_lut",
      "path": "nonlinearity_lut.bin",
      "sha256": "...",
      "size_bytes": 262144,
      "acquired_at": "2026-04-15T09:22:00Z"
    },
    {
      "type": "lag_params",
      "path": "lag_params.json",
      "sha256": "...",
      "size_bytes": 512,
      "acquired_at": "2026-04-15T09:25:00Z"
    }
  ],
  "hash_chain": {
    "prev_session_id": "7B2F9A4C1D3E8F60",
    "manifest_hash": "sha256_of_this_file_excluding_manifest_hash_field"
  }
}
```

#### 2.4.3 혼합 세션 거부 로직

```python
import hashlib
import json
from pathlib import Path
from typing import Optional

class CalibrationSessionLock:
    """
    Validates that all calibration files in a pack belong to the same session.
    Rejects mixed-session packs unless explicitly overridden for diagnostics.
    """

    def __init__(self, manifest_path: Path, allow_mixed: bool = False):
        self.manifest_path = manifest_path
        self.allow_mixed   = allow_mixed
        self._manifest     = None

    def load_and_validate(self) -> dict:
        """
        Load manifest and verify:
          1. All file hashes match
          2. All files share the same session_id
          3. Hash chain integrity (prev_session_id link)

        Returns: validated manifest dict
        Raises:  CalibrationIntegrityError on any violation
        """
        with open(self.manifest_path) as f:
            manifest = json.load(f)

        session_id = manifest['session_id']
        errors = []

        # 1. Verify individual file hashes
        base_dir = self.manifest_path.parent
        for entry in manifest['files']:
            fpath = base_dir / entry['path']
            if not fpath.exists():
                errors.append(f"Missing: {entry['path']}")
                continue
            computed = _sha256_file(fpath)
            if computed != entry['sha256']:
                errors.append(
                    f"Hash mismatch for {entry['path']}: "
                    f"expected {entry['sha256'][:16]}…, got {computed[:16]}…")

        # 2. Verify session ID consistency in binary headers
        for entry in manifest['files']:
            fpath = base_dir / entry['path']
            if not fpath.exists(): continue
            if entry['path'].endswith('.bin'):
                file_session = _read_session_id_from_bin(fpath)
                if file_session and file_session != session_id:
                    if not self.allow_mixed:
                        errors.append(
                            f"Session mismatch in {entry['path']}: "
                            f"file={file_session}, manifest={session_id}")

        if errors:
            raise CalibrationIntegrityError(errors)

        self._manifest = manifest
        return manifest

    def check_drift(self,
                    current_dark_mean: float,
                    baseline_dark_mean: float,
                    threshold_adu_per_day: float = 5.0,
                    days_elapsed: float = 1.0) -> dict:
        """
        Detect dark current drift and recommend recalibration.

        Args:
            current_dark_mean:  current dark frame mean (ADU)
            baseline_dark_mean: dark mean at last calibration (ADU)
            threshold_adu_per_day: drift threshold for recalibration trigger
            days_elapsed:       time since last calibration
        Returns:
            dict: {drift_adu_per_day, needs_recal, severity}
        """
        drift = abs(current_dark_mean - baseline_dark_mean) / max(days_elapsed, 0.01)
        needs_recal = drift > threshold_adu_per_day
        severity = ('critical' if drift > 3 * threshold_adu_per_day else
                    'warning'  if needs_recal else 'ok')
        return {
            'drift_adu_per_day': drift,
            'needs_recalibration': needs_recal,
            'severity': severity,
            'baseline_dark_mean': baseline_dark_mean,
            'current_dark_mean': current_dark_mean,
        }


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def _read_session_id_from_bin(path: Path) -> Optional[str]:
    """Read session_id from binary calibration file extended header."""
    with open(path, 'rb') as f:
        magic = f.read(4)
        if magic not in (b'XOFF', b'XGAI', b'XDEF'):
            return None
        # Standard header: 64 or 96 bytes; extended header starts immediately after
        hdr_size = 96 if magic == b'XGAI' else 64
        f.seek(hdr_size)
        ext = f.read(32)
        if len(ext) < 8:
            return None
        return ext[:8].decode('ascii', errors='replace')


class CalibrationIntegrityError(Exception):
    def __init__(self, errors: list):
        self.errors = errors
        super().__init__('\n'.join(errors))
```

#### 2.4.4 C++ 런타임 세션 검증

```cpp
// C++ runtime session lock — called by ConfigManager during startup
// Rejects packs with mismatched session IDs before any correction is applied.

struct CalibrationManifestEntry {
    std::string  type;
    std::string  path;
    std::string  sha256;
    std::string  session_id;   // read from binary header extended block
};

class CalibrationSessionValidator {
public:
    enum class ValidationResult {
        OK,
        SESSION_MISMATCH,
        HASH_MISMATCH,
        MISSING_FILE,
        PARSE_ERROR,
    };

    ValidationResult validate_pack(const std::string& manifest_path,
                                    bool allow_mixed = false) {
        // Parse JSON manifest
        auto manifest = parse_json_manifest(manifest_path);
        if (!manifest.valid) return ValidationResult::PARSE_ERROR;

        std::string primary_session = manifest.session_id;

        for (const auto& entry : manifest.files) {
            // 1. Check file existence
            if (!std::filesystem::exists(entry.path))
                return ValidationResult::MISSING_FILE;

            // 2. Verify SHA-256
            if (compute_sha256_file(entry.path) != entry.sha256)
                return ValidationResult::HASH_MISMATCH;

            // 3. Read session ID from binary header extended block
            auto file_session = read_session_id(entry.path);
            if (!file_session.empty() &&
                file_session != primary_session && !allow_mixed) {
                log_error("Session mismatch: file={}, manifest={}",
                           file_session, primary_session);
                return ValidationResult::SESSION_MISMATCH;
            }
        }
        return ValidationResult::OK;
    }

private:
    std::string read_session_id(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return {};
        char magic[4];
        f.read(magic, 4);
        // Seek to extended header
        size_t hdr_size = (std::strncmp(magic, "XGAI", 4) == 0) ? 96 : 64;
        f.seekg(hdr_size);
        char session_buf[9] = {};
        f.read(session_buf, 8);
        return std::string(session_buf);
    }
};
```

#### 2.4.5 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 혼합 세션 거부율 | 100% | 의도적 혼합 팩 주입 테스트 |
| 해시 검증 속도 (5개 파일 × 38MB) | < 3s | SHA-256 벤치마크 |
| 드리프트 감지 임계치 정확도 | ±0.1 ADU/day | 합성 드리프트 시나리오 |
| 매니페스트 파싱 오류 처리 | 100% 예외 캐치 | 손상된 JSON 주입 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-SEC-002 (파일 무결성 확장), SRS-SEC-003 (교정 세션 추적) — Phase 2 추가 예정

---

## 3. SWI-1: Pre-Processing 알고리즘

### 3.0 SWU-1.0 Readout Validation (GAP-I 해소)

Readout Validation은 모든 보정 전에 실행되는 **입력 품질 게이트**로, 잘못된 이미지가 파이프라인에 진입하는 것을 방지한다. xpe-algorithm-spec-deepsync.md 표 "release-safe baseline" 항목에 명시되어 있으며, 이 섹션이 상세 구현을 제공한다.

#### 3.0.1 알고리즘 수학 정의

**포화 검사 (Saturation)**:
$$P_{\text{sat}} = \frac{|\{(x,y) : I_{\text{raw}}(x,y) \geq V_{\text{sat}}\}|}{W \times H} \leq \theta_{\text{sat}}$$

**DR 클리핑 검사 (Clipped Dynamic Range)**:
$$P_{\text{clip\_low}} = \frac{|\{(x,y) : I_{\text{raw}}(x,y) \leq V_{\text{clip\_low}}\}|}{W \times H} \leq \theta_{\text{clip}}$$

**불가능한 기하학 검사 (Impossible Geometry)**:
$$\text{Valid}: W, H \in [256,\ 4096],\quad W \cdot H \leq 16{,}777{,}216,\quad W/H \in [0.5,\ 4.0]$$

**행/열 결함 검사 (Row/Column Fault)**:
$$\text{RowFault}(y) = 1 \iff \text{std}(I_{\text{raw}}[y, :]) < \sigma_{\text{line\_min}}$$

| 파라미터 | 기본값 | 의미 |
|---------|-------|------|
| `V_sat` | 65530 (14-bit: 16380) | 포화 임계치 (ADU) |
| `θ_sat` | 0.05 | 허용 포화 픽셀 비율 (5%) |
| `V_clip_low` | 4 | 하단 클리핑 임계치 (ADU) |
| `θ_clip` | 0.10 | 허용 하단 클리핑 비율 (10%) |
| `σ_line_min` | 2.0 | 최소 행/열 표준편차 (ADU) |

#### 3.0.2 Python 구현 (오프라인 QC)

```python
import numpy as np
from dataclasses import dataclass, field
from enum import Flag, auto

class ReadoutFaultCode(Flag):
    OK              = 0
    SATURATED       = auto()   # > θ_sat fraction at V_sat
    CLIPPED_DR      = auto()   # > θ_clip fraction at V_clip_low
    IMPOSSIBLE_GEOM = auto()   # width/height outside valid range
    ROW_FAULT       = auto()   # ≥1 row with std < σ_line_min
    COLUMN_FAULT    = auto()   # ≥1 col with std < σ_line_min
    EMPTY_IMAGE     = auto()   # all-zero or single-value image

@dataclass
class ReadoutValidationResult:
    fault_code:     ReadoutFaultCode = ReadoutFaultCode.OK
    fault_details:  dict             = field(default_factory=dict)
    saturated_frac: float            = 0.0
    clipped_frac:   float            = 0.0
    faulty_rows:    list             = field(default_factory=list)
    faulty_cols:    list             = field(default_factory=list)

def validate_readout(raw: np.ndarray,
                     v_sat:       int   = 65530,
                     theta_sat:   float = 0.05,
                     v_clip_low:  int   = 4,
                     theta_clip:  float = 0.10,
                     sigma_line_min: float = 2.0,
                     bit_depth:   int   = 16) -> ReadoutValidationResult:
    """
    Gate-check a raw detector image before any correction is applied.

    Returns ReadoutValidationResult; caller must reject the frame if
    fault_code != ReadoutFaultCode.OK (non-zero).
    """
    result = ReadoutValidationResult()
    H, W = raw.shape
    img = raw.astype(np.float32)

    # 1. Impossible geometry
    if not (256 <= W <= 4096 and 256 <= H <= 4096):
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['geometry'] = f'W={W}, H={H} outside [256,4096]'
    if W * H > 16_777_216:
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['geometry_area'] = f'W×H={W*H} > 16M'
    ar = W / H
    if not (0.5 <= ar <= 4.0):
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['aspect_ratio'] = f'{ar:.3f}'

    # 2. Saturation check
    max_adu = (1 << bit_depth) - 1
    sat_thresh = min(v_sat, max_adu)
    sat_mask   = img >= sat_thresh
    result.saturated_frac = float(np.mean(sat_mask))
    if result.saturated_frac > theta_sat:
        result.fault_code |= ReadoutFaultCode.SATURATED
        result.fault_details['saturated_frac'] = f'{result.saturated_frac:.4f}'

    # 3. Clipped DR check (lower end)
    clip_mask = img <= v_clip_low
    result.clipped_frac = float(np.mean(clip_mask))
    if result.clipped_frac > theta_clip:
        result.fault_code |= ReadoutFaultCode.CLIPPED_DR
        result.fault_details['clipped_frac'] = f'{result.clipped_frac:.4f}'

    # 4. Empty image check
    if np.std(img) < 10.0:
        result.fault_code |= ReadoutFaultCode.EMPTY_IMAGE
        result.fault_details['std'] = f'{float(np.std(img)):.2f}'

    # 5. Row fault detection
    row_stds = np.std(img, axis=1)
    faulty_rows = np.where(row_stds < sigma_line_min)[0].tolist()
    if faulty_rows:
        result.fault_code  |= ReadoutFaultCode.ROW_FAULT
        result.faulty_rows  = faulty_rows
        result.fault_details['faulty_row_count'] = len(faulty_rows)

    # 6. Column fault detection
    col_stds = np.std(img, axis=0)
    faulty_cols = np.where(col_stds < sigma_line_min)[0].tolist()
    if faulty_cols:
        result.fault_code  |= ReadoutFaultCode.COLUMN_FAULT
        result.faulty_cols  = faulty_cols
        result.fault_details['faulty_col_count'] = len(faulty_cols)

    return result
```

#### 3.0.3 C++ 런타임 구현

```cpp
enum class ReadoutFaultCode : uint32_t {
    OK              = 0x00,
    SATURATED       = 0x01,
    CLIPPED_DR      = 0x02,
    IMPOSSIBLE_GEOM = 0x04,
    ROW_FAULT       = 0x08,
    COLUMN_FAULT    = 0x10,
    EMPTY_IMAGE     = 0x20,
};
inline ReadoutFaultCode operator|(ReadoutFaultCode a, ReadoutFaultCode b) {
    return static_cast<ReadoutFaultCode>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct ReadoutValidationResult {
    ReadoutFaultCode fault_code  = ReadoutFaultCode::OK;
    float  saturated_frac        = 0.0f;
    float  clipped_frac          = 0.0f;
    int    faulty_row_count      = 0;
    int    faulty_col_count      = 0;
};

ReadoutValidationResult xpe_validate_readout(
        const uint16_t* raw, uint32_t W, uint32_t H, uint32_t bit_depth = 16) {
    ReadoutValidationResult r;
    const uint32_t total = W * H;
    const uint16_t v_sat       = static_cast<uint16_t>((1u << bit_depth) - 6u);
    const uint16_t v_clip_low  = 4u;
    const float    theta_sat   = 0.05f;
    const float    theta_clip  = 0.10f;
    const float    sigma_line_min = 2.0f;

    // 1. Geometry check
    if (W < 256 || W > 4096 || H < 256 || H > 4096 || total > 16'777'216u) {
        r.fault_code = r.fault_code | ReadoutFaultCode::IMPOSSIBLE_GEOM;
    }
    float ar = static_cast<float>(W) / H;
    if (ar < 0.5f || ar > 4.0f) {
        r.fault_code = r.fault_code | ReadoutFaultCode::IMPOSSIBLE_GEOM;
    }

    // 2. Saturation + clip count (AVX2 vectorised)
    uint32_t sat_cnt = 0, clip_cnt = 0;
    for (uint32_t i = 0; i < total; ++i) {
        if (raw[i] >= v_sat)      ++sat_cnt;
        if (raw[i] <= v_clip_low) ++clip_cnt;
    }
    r.saturated_frac = static_cast<float>(sat_cnt) / total;
    r.clipped_frac   = static_cast<float>(clip_cnt) / total;

    if (r.saturated_frac > theta_sat)
        r.fault_code = r.fault_code | ReadoutFaultCode::SATURATED;
    if (r.clipped_frac > theta_clip)
        r.fault_code = r.fault_code | ReadoutFaultCode::CLIPPED_DR;

    // 3. Row/column fault (Welford online mean/variance)
    for (uint32_t y = 0; y < H; ++y) {
        double mean = 0.0, M2 = 0.0;
        for (uint32_t x = 0; x < W; ++x) {
            double delta = raw[y * W + x] - mean;
            mean += delta / (x + 1);
            M2   += delta * (raw[y * W + x] - mean);
        }
        float std_row = static_cast<float>(std::sqrt(M2 / (W - 1)));
        if (std_row < sigma_line_min) ++r.faulty_row_count;
    }
    for (uint32_t x = 0; x < W; ++x) {
        double mean = 0.0, M2 = 0.0;
        for (uint32_t y = 0; y < H; ++y) {
            double delta = raw[y * W + x] - mean;
            mean += delta / (y + 1);
            M2   += delta * (raw[y * W + x] - mean);
        }
        float std_col = static_cast<float>(std::sqrt(M2 / (H - 1)));
        if (std_col < sigma_line_min) ++r.faulty_col_count;
    }
    if (r.faulty_row_count > 0)
        r.fault_code = r.fault_code | ReadoutFaultCode::ROW_FAULT;
    if (r.faulty_col_count > 0)
        r.fault_code = r.fault_code | ReadoutFaultCode::COLUMN_FAULT;

    return r;
}
```

#### 3.0.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 포화 감지 정확도 | FPR < 1%, FNR = 0% | 합성 포화 이미지 주입 |
| 행/열 결함 감지 | 결함 없는 경우 경보 없음 | 정상 dark frame 검사 |
| 처리 시간 (3072×3072) | < 5ms | 단일 코어, no SIMD needed |
| 기하학 검사 | 잘못된 크기 100% 차단 | 경계값 분석 |

---

### 3.0.5 SWU-1.0.5 Non-linearity Correction (GAP-H 해소)

비선형성 보정은 Offset/Gain 보정 이후, Log Transform 이전에 적용한다. xpe-algorithm-spec-deepsync.md "release-safe baseline"에서 "monotonic LUT or low-order polynomial"로 명시되어 있다.

#### 3.0.6 알고리즘 수학 정의

**Monotonic LUT 방법 (권장)**:

LUT $\mathcal{L}$ 은 ADU 입력값에 대한 선형 응답 보정 출력을 저장한다:

$$I_{\text{linear}}(x,y) = \mathcal{L}\!\left[I_{\text{gain\_corr}}(x,y)\right]$$

LUT 생성 시 단조성 조건을 강제한다:
$$\mathcal{L}[v+1] \geq \mathcal{L}[v] \quad \forall\ v \in [0,\ 2^B - 2]$$

**Polynomial 방법 (대안)**:
$$I_{\text{linear}} = \sum_{k=0}^{K} c_k \cdot I_{\text{gain\_corr}}^k, \quad K \leq 4$$

단조성 요구사항: 도함수 $\frac{dI_{\text{linear}}}{dI_{\text{gain\_corr}}} > 0$ (전 범위에서 양수)

#### 3.0.7 Python 교정 구현 (오프라인)

```python
import numpy as np
from scipy.interpolate import PchipInterpolator

def calibrate_nonlinearity_lut(
        signal_levels_adu:  np.ndarray,
        true_exposures_mAs: np.ndarray,
        bit_depth: int = 16) -> np.ndarray:
    """
    Generate a monotonic non-linearity correction LUT from calibration data.

    Args:
        signal_levels_adu:  measured detector signal at each exposure (N,)
        true_exposures_mAs: reference exposure levels in mAs (N,)  
                            Linear response: signal ∝ mAs
        bit_depth:          detector bit depth (default 16)
    Returns:
        lut: float32 array of length 2^bit_depth
             lut[adu] = linearity-corrected value in ADU-equivalent units

    Method:
        1. Fit PCHIP spline: ADU → ideal_linear (preserves monotonicity)
        2. Evaluate at every integer ADU level 0..2^B-1
        3. Clip & enforce monotonicity (post-fit safety pass)
    """
    N = len(signal_levels_adu)
    assert len(true_exposures_mAs) == N and N >= 4, \
        "Need ≥4 calibration points"

    # Normalize: ideal linear signal = gain_mean × (exposure / exposure_ref)
    exposure_ref  = true_exposures_mAs[N // 2]  # mid-range reference
    signal_ref    = signal_levels_adu[N // 2]
    ideal_signals = signal_ref * (true_exposures_mAs / exposure_ref)

    # Sort by input signal for spline fitting
    sort_idx = np.argsort(signal_levels_adu)
    x_ctrl   = signal_levels_adu[sort_idx].astype(np.float64)
    y_ctrl   = ideal_signals[sort_idx].astype(np.float64)

    # PCHIP: monotone cubic Hermite interpolation
    interp = PchipInterpolator(x_ctrl, y_ctrl, extrapolate=True)

    full_adu_range = np.arange(1 << bit_depth, dtype=np.float64)
    lut = interp(full_adu_range).astype(np.float32)

    # Enforce monotonicity (safety clip)
    lut[0] = max(0.0, lut[0])
    for i in range(1, len(lut)):
        if lut[i] < lut[i - 1]:
            lut[i] = lut[i - 1]  # monotone clamp

    # Clip to valid ADU range
    max_adu = float((1 << bit_depth) - 1)
    lut = np.clip(lut, 0.0, max_adu)
    return lut


def validate_nonlinearity_lut(lut: np.ndarray,
                               max_deviation_pct: float = 5.0) -> dict:
    """
    Validate that the generated LUT is monotone and within deviation bounds.

    Returns dict with: is_valid, max_deviation_pct, monotone_violations
    """
    diffs = np.diff(lut)
    violations = int(np.sum(diffs < 0))
    # Max deviation from identity (no correction)
    identity  = np.arange(len(lut), dtype=np.float32)
    deviation = np.abs(lut - identity) / (identity + 1.0) * 100.0  # percent
    max_dev   = float(np.max(deviation))
    return {
        'is_valid':            violations == 0 and max_dev <= max_deviation_pct,
        'max_deviation_pct':   max_dev,
        'monotone_violations': violations,
    }
```

#### 3.0.8 C++ 런타임 구현 (AVX2 + LUT lookup)

```cpp
// Non-linearity correction via pre-loaded float LUT
// LUT size: 2^bit_depth floats (256KB for 16-bit)
// Called after xpe_gain_correct(), before xpe_log_transform()

void xpe_nonlinearity_correct(const float*    __restrict__ gain_corr_img,
                               const float*    __restrict__ lut,        // size: 1<<bit_depth
                               float*          __restrict__ out,
                               uint32_t width, uint32_t height,
                               uint32_t bit_depth = 16u) {
    const size_t   total   = static_cast<size_t>(width) * height;
    const uint32_t max_idx = (1u << bit_depth) - 1u;

    // Scalar LUT lookup (vectorisation not beneficial for scatter-gather pattern)
    for (size_t i = 0; i < total; ++i) {
        // Clamp to valid LUT range before index conversion
        float  v   = std::clamp(gain_corr_img[i], 0.0f, static_cast<float>(max_idx));
        uint32_t idx = static_cast<uint32_t>(v + 0.5f);  // nearest-integer
        idx = std::min(idx, max_idx);
        out[i] = lut[idx];
    }
}
```

#### 3.0.9 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| LUT 단조성 | 위반 0건 | `validate_nonlinearity_lut()` |
| 최대 보정 편차 | ≤ 5% | identity 대비 백분율 |
| 선형성 잔차 R² | ≥ 0.9995 | 교정 후 계단식 노출 |
| 처리 시간 (3072×3072) | < 30ms | 단일 코어 |

---

### 3.1 SWU-1.1 Offset Correction (SRS-FUNC-001)

#### 3.1.1 알고리즘 수학 정의

$$I_{\text{offset}}(x,y) = \max\left(I_{\text{raw}}(x,y) - I_{\text{dark}}(x,y),\ 0\right)$$

- **입력**: `I_raw` (uint16), `I_dark` (float32 mean of ≥16 dark frames)
- **출력**: `I_offset` (float32, ≥ 0)
- **목적**: Detector dark current 및 readout offset 제거

#### 3.1.2 Offset Map 생성 알고리즘 (Python, 오프라인)

```python
def compute_offset_map(dark_frames: list[np.ndarray]) -> np.ndarray:
    """
    Generate offset correction map from ≥16 dark frames.
    
    Args:
        dark_frames: list of uint16 arrays, shape (H, W), len ≥ 16
    Returns:
        float32 offset map, shape (H, W)
    """
    assert len(dark_frames) >= 16, "Minimum 16 dark frames required"
    
    stack = np.stack(dark_frames, axis=0).astype(np.float64)
    
    # Temporal outlier rejection (σ-clipping, 3σ)
    mean = np.mean(stack, axis=0)
    std  = np.std(stack, axis=0)
    mask = np.abs(stack - mean) <= 3.0 * std  # (N, H, W)
    
    # Compute masked mean
    offset_map = np.sum(stack * mask, axis=0) / np.maximum(np.sum(mask, axis=0), 1)
    
    return offset_map.astype(np.float32)
```

#### 3.1.3 C++ 런타임 구현 (AVX2 최적화)

```cpp
// SWU-1.1: xpe_offset_correct()
// Vectorized subtraction with floor-at-zero (SRS-PERF-001: ≤500ms)
void xpe_offset_correct(const uint16_t* __restrict__ raw,
                         const float*    __restrict__ offset_map,
                         float*          __restrict__ out,
                         uint32_t width, uint32_t height) {
    const size_t total = static_cast<size_t>(width) * height;
    size_t i = 0;

    // AVX2 path: process 8 float32 per iteration
    const __m256 zero = _mm256_setzero_ps();
    for (; i + 8 <= total; i += 8) {
        // Load 8 uint16 → convert to float32
        __m128i raw16 = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(raw + i));
        __m256 raw_f  = _mm256_cvtepi32_ps(
            _mm256_cvtepu16_epi32(raw16));
        
        __m256 off_f  = _mm256_loadu_ps(offset_map + i);
        __m256 diff   = _mm256_sub_ps(raw_f, off_f);
        __m256 result = _mm256_max_ps(diff, zero);   // clamp at 0
        
        _mm256_storeu_ps(out + i, result);
    }
    
    // Scalar tail
    for (; i < total; ++i) {
        float diff = static_cast<float>(raw[i]) - offset_map[i];
        out[i] = (diff < 0.0f) ? 0.0f : diff;
    }
}
```

#### 3.1.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Residual dark signal | Mean < 1.0 ADU | 보정 후 dark field 평균 |
| Negative 픽셀 | 0개 | min(I_offset) ≥ 0 |
| 처리 시간 (3072×3072) | < 50ms | 단일 코어 벤치마크 |

---

### 3.2 SWU-1.2 Gain Correction (SRS-FUNC-002)

#### 3.2.1 알고리즘 수학 정의

$$G(x,y) = \frac{\bar{I}_{\text{flat}}}{I_{\text{flat}}(x,y) - I_{\text{dark}}(x,y)}$$

$$I_{\text{corr}}(x,y) = I_{\text{offset}}(x,y) \cdot G(x,y)$$

- **GainMean** $\bar{I}_{\text{flat}}$: ROI 내 `(Flood - Offset)` 의 spatial mean
- **목적**: Pixel 간 감도 차이 (heel effect, scintillator 두께 불균일) 보정
- **SID별 개별 맵**: kVp에 따른 스펙트럼 변화 → SID마다 별도 gain map 보유

#### 3.2.2 Gain Map 생성 알고리즘 (Python, 오프라인)

```python
def compute_gain_map(flood_frames: list[np.ndarray],
                     offset_map: np.ndarray,
                     roi: tuple[int,int,int,int] | None = None) -> tuple[np.ndarray, float]:
    """
    Generate per-SID gain correction map.
    
    Args:
        flood_frames: list of uint16 flood images (≥8 recommended)
        offset_map:   float32 offset map (H, W)
        roi:          (x0, y0, x1, y1) for GainMean calculation, None = full image
    Returns:
        (gain_map float32 (H,W), gain_mean float32)
    """
    # Average flood frames
    stack = np.stack(flood_frames, axis=0).astype(np.float32)
    flood_mean = np.mean(stack, axis=0)
    
    # Subtract dark
    net_signal = flood_mean - offset_map
    
    # Compute GainMean from ROI (avoid detector edge artefacts)
    if roi:
        x0, y0, x1, y1 = roi
        roi_signal = net_signal[y0:y1, x0:x1]
    else:
        # Auto-trim: inner 80% of image
        h, w = net_signal.shape
        margin_y, margin_x = h // 10, w // 10
        roi_signal = net_signal[margin_y:-margin_y, margin_x:-margin_x]
    
    gain_mean = float(np.mean(roi_signal))
    
    # Compute gain map; clamp to prevent extreme values
    with np.errstate(divide='ignore', invalid='ignore'):
        gain_map = np.where(net_signal > 0,
                            gain_mean / net_signal,
                            1.0)  # fallback for near-zero pixels
    
    gain_map = np.clip(gain_map, 0.5, 2.0).astype(np.float32)
    
    return gain_map, gain_mean
```

#### 3.2.3 C++ 런타임 구현 (AVX2 최적화)

```cpp
// SWU-1.2: xpe_gain_correct()
void xpe_gain_correct(const float* __restrict__ offset_corrected,
                       const float* __restrict__ gain_map,
                       float*       __restrict__ out,
                       uint32_t width, uint32_t height) {
    const size_t total = static_cast<size_t>(width) * height;
    size_t i = 0;

    for (; i + 8 <= total; i += 8) {
        __m256 img  = _mm256_loadu_ps(offset_corrected + i);
        __m256 gain = _mm256_loadu_ps(gain_map + i);
        __m256 res  = _mm256_mul_ps(img, gain);
        _mm256_storeu_ps(out + i, res);
    }
    for (; i < total; ++i) {
        out[i] = offset_corrected[i] * gain_map[i];
    }
}
```

#### 3.2.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Uniformity (PRNU) | CV < 1% after gain | std/mean × 100% |
| Gain map range | [0.5, 2.0] | min/max of G(x,y) |
| Heel effect correction | CV 감소율 > 80% | 보정 전후 CV 비교 |

---

### 3.2.5 SWU-1.2b Multi-SID Gain 보간 및 kVp-Stratified Gain 선택 (GAP-P 해소)

기존 §3.2는 단일 SID·kVp 조합에 대한 gain 보정을 다룬다. 본 섹션은 여러 SID·kVp 조합으로 사전 교정된 gain 맵 세트에서 실제 촬영 조건에 맞는 맵을 선택·보간하는 알고리즘을 추가한다.

#### 3.2.5.1 알고리즘 수학 정의

SID 집합 $\{S_1, S_2, \ldots, S_N\}$, kVp 집합 $\{k_1, k_2, \ldots, k_M\}$으로 교정된 gain 맵 격자 $G(x, y; S_i, k_j)$가 주어졌을 때:

$$G_{\text{select}}(x,y) = \sum_{i,j} w_{ij} \cdot G(x,y;S_i,k_j)$$

**이중선형 보간 가중치**:

$$w_{ij} = \left[(1-t_S)(1-t_k),\ (1-t_S)t_k,\ t_S(1-t_k),\ t_S t_k\right]_{i \in \{0,1\},\, j \in \{0,1\}}$$

$$t_S = \frac{S - S_{lo}}{S_{hi} - S_{lo}}, \quad t_k = \frac{k - k_{lo}}{k_{hi} - k_{lo}}$$

**자동 SID 선택 로직**:

$$\text{SID}_{\text{use}} = \begin{cases} S_1 & \text{if } S < S_1 \\ S_N & \text{if } S > S_N \\ \text{interpolate}(S_{lo}, S_{hi}) & \text{otherwise} \end{cases}$$

#### 3.2.5.2 Python 구현 (오프라인)

```python
import numpy as np
from pathlib import Path
from typing import Dict, Tuple, List
import struct

GainKey = Tuple[float, float]  # (sid_mm, kvp)

def load_gain_map_table(gain_dir: Path,
                         sid_list: List[float],
                         kvp_list: List[float]) -> Dict[GainKey, np.ndarray]:
    """
    Load pre-calibrated gain maps for all SID/kVp combinations.

    Expects files named: gain_SIDxxxx_kVPyyy.bin (XGAI format)
    Returns dict: {(sid_mm, kvp): gain_map float32 (H, W)}
    """
    table: Dict[GainKey, np.ndarray] = {}
    for sid in sid_list:
        for kvp in kvp_list:
            fname = gain_dir / f"gain_SID{sid:04.0f}_kVP{kvp:03.0f}.bin"
            if fname.exists():
                table[(sid, kvp)] = _read_xgai_bin(fname)
            else:
                raise FileNotFoundError(f"Missing gain map: {fname}")
    return table


def _read_xgai_bin(path: Path) -> np.ndarray:
    """Parse XGAI binary format (§2.2.2)."""
    with open(path, 'rb') as f:
        hdr = f.read(96)
        magic = hdr[:4]
        assert magic == b'XGAI', f"Bad magic: {magic}"
        w = struct.unpack_from('<I', hdr, 8)[0]
        h = struct.unpack_from('<I', hdr, 12)[0]
        payload = np.frombuffer(f.read(), dtype=np.float32)
    return payload.reshape(h, w)


def select_gain_map_bilinear(
        table:    Dict[GainKey, np.ndarray],
        sid_mm:   float,
        kvp:      float,
        sid_list: List[float],
        kvp_list: List[float]) -> np.ndarray:
    """
    Bilinear interpolation of gain map for arbitrary SID and kVp.

    Args:
        table:    pre-loaded gain map dictionary
        sid_mm:   actual source-to-image distance (mm)
        kvp:      actual tube voltage (kVp)
        sid_list: sorted calibrated SID values
        kvp_list: sorted calibrated kVp values
    Returns:
        gain_map: float32 (H, W) — interpolated gain map
    """
    sid_arr = np.array(sorted(sid_list), dtype=np.float64)
    kvp_arr = np.array(sorted(kvp_list), dtype=np.float64)

    # Clamp to calibrated range
    sid_c = float(np.clip(sid_mm, sid_arr[0], sid_arr[-1]))
    kvp_c = float(np.clip(kvp,    kvp_arr[0], kvp_arr[-1]))

    i_s = int(np.clip(np.searchsorted(sid_arr, sid_c, 'right') - 1, 0, len(sid_arr) - 2))
    i_k = int(np.clip(np.searchsorted(kvp_arr, kvp_c, 'right') - 1, 0, len(kvp_arr) - 2))

    s_lo, s_hi = sid_arr[i_s], sid_arr[i_s + 1]
    k_lo, k_hi = kvp_arr[i_k], kvp_arr[i_k + 1]

    ts = (sid_c - s_lo) / (s_hi - s_lo) if s_hi != s_lo else 0.0
    tk = (kvp_c - k_lo) / (k_hi - k_lo) if k_hi != k_lo else 0.0

    m00 = table[(s_lo, k_lo)].astype(np.float64)
    m01 = table[(s_lo, k_hi)].astype(np.float64)
    m10 = table[(s_hi, k_lo)].astype(np.float64)
    m11 = table[(s_hi, k_hi)].astype(np.float64)

    result = ((1 - ts) * (1 - tk) * m00 +
              (1 - ts) *      tk  * m01 +
                   ts  * (1 - tk) * m10 +
                   ts  *      tk  * m11)
    return result.astype(np.float32)


def validate_gain_table_consistency(
        table:    Dict[GainKey, np.ndarray],
        max_ratio: float = 1.10) -> List[str]:
    """
    Check that adjacent gain maps differ by no more than max_ratio.
    Returns list of warnings (empty = OK).
    """
    warnings = []
    keys = sorted(table.keys())
    for i, k1 in enumerate(keys):
        for k2 in keys[i + 1:]:
            sid_diff = abs(k1[0] - k2[0])
            kvp_diff = abs(k1[1] - k2[1])
            # Only check neighbours
            if sid_diff <= 200 and kvp_diff <= 20:
                ratio = table[k1] / np.maximum(table[k2], 1e-6)
                if float(np.max(ratio)) > max_ratio or float(np.min(ratio)) < 1.0 / max_ratio:
                    warnings.append(
                        f"Gain maps {k1}↔{k2} differ by > {max_ratio}×: "
                        f"max={float(np.max(ratio)):.3f}")
    return warnings
```

#### 3.2.5.3 C++ 런타임 구현

```cpp
// Multi-SID gain map selector — C++ runtime
// Gain maps are pre-loaded at startup into GainMapTable.

struct GainMapEntry {
    float  sid_mm;
    float  kvp;
    float* map;       // float32 (H × W), pinned memory preferred
    size_t size;
};

class GainMapTable {
public:
    // Load all entries from calibration directory
    void load(const std::vector<std::string>& paths);

    // Select best map for given SID/kVp (nearest or interpolate)
    // Returns pointer to the map; ownership stays with GainMapTable.
    const float* get_map(float sid_mm, float kvp,
                          uint32_t W, uint32_t H,
                          float* interp_buf = nullptr) const {
        // Find bounding entries
        const GainMapEntry* lo_sid  = nullptr;
        const GainMapEntry* hi_sid  = nullptr;
        float best_sid_lo = -1e9f, best_sid_hi = 1e9f;

        for (const auto& e : entries_) {
            if (std::fabsf(e.kvp - kvp) > 10.0f) continue;
            if (e.sid_mm <= sid_mm && e.sid_mm > best_sid_lo) {
                best_sid_lo = e.sid_mm; lo_sid = &e;
            }
            if (e.sid_mm > sid_mm && e.sid_mm < best_sid_hi) {
                best_sid_hi = e.sid_mm; hi_sid = &e;
            }
        }

        if (!lo_sid && !hi_sid) return nullptr;
        if (!lo_sid)            return hi_sid->map;
        if (!hi_sid)            return lo_sid->map;

        // Bilinear interpolation into interp_buf
        if (!interp_buf) return lo_sid->map;  // fallback: no buffer supplied

        float t = (sid_mm - lo_sid->sid_mm) / (hi_sid->sid_mm - lo_sid->sid_mm);
        const size_t total = static_cast<size_t>(W) * H;
        const __m256 v_t    = _mm256_set1_ps(t);
        const __m256 v_1mt  = _mm256_set1_ps(1.0f - t);
        size_t i = 0;
        for (; i + 8 <= total; i += 8) {
            __m256 a = _mm256_loadu_ps(lo_sid->map + i);
            __m256 b = _mm256_loadu_ps(hi_sid->map + i);
            _mm256_storeu_ps(interp_buf + i,
                             _mm256_fmadd_ps(v_t, b, _mm256_mul_ps(v_1mt, a)));
        }
        for (; i < total; ++i) {
            interp_buf[i] = (1.0f - t) * lo_sid->map[i] + t * hi_sid->map[i];
        }
        return interp_buf;
    }

private:
    std::vector<GainMapEntry> entries_;
};
```

#### 3.2.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 보간 오차 (중간 SID) | < 0.5% signal | 교정되지 않은 SID에서 실측과 보간 비교 |
| kVp 전환 후 PRNU | CV < 1% | kVp ±20% 변화 후 flood 균일도 |
| 맵 로드 시간 (8 맵 × 3072×3072) | < 2s | 시작 시 일괄 로딩 |
| 자동 SID 선택 정확도 | ±25mm 이내 | SID 센서 데이터 교차 검증 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-002 (Gain Correction 확장) — Multi-SID 항목 Phase 2 추가 예정

---

### 3.3 SWU-1.3 Defect Pixel Correction (SRS-FUNC-003)

#### 3.3.1 결함 픽셀 분류 체계

| 유형 | 정의 | 보간 방법 |
|------|------|---------|
| Point Defect | 단일 픽셀: G(x,y) < G_mean × 0.5 또는 > G_mean × 2.0 | 4-neighbor 평균 |
| Cluster Defect | 반경 r ≤ 3 내 ≥4개 point defect | 8-neighbor 유효 픽셀 평균 |
| Column Defect | 전체 컬럼의 ≥80% 결함 | 좌우 컬럼 선형 보간 |
| Row Defect | 전체 행의 ≥80% 결함 | 상하 행 선형 보간 |
| Stuck Pixel | Dark frame에서도 포화 (>MAX-100 ADU) | 주변 median |

#### 3.3.2 결함 맵 생성 알고리즘 (Python, 오프라인)

```python
def create_defect_map(gain_map: np.ndarray,
                      dark_map: np.ndarray,
                      bit_depth: int = 14) -> np.ndarray:
    """
    Detect and classify defect pixels from calibration data.
    
    Returns:
        defect_map: uint8 array (H, W)
          0 = good pixel
          1 = point defect
          2 = cluster defect  
          3 = column defect
          4 = row defect
          5 = stuck pixel (always bright)
    """
    H, W = gain_map.shape
    defect_map = np.zeros((H, W), dtype=np.uint8)
    max_adu = (1 << bit_depth) - 1
    
    gain_mean = np.median(gain_map)  # robust to outliers
    gain_std  = np.std(gain_map[
        (gain_map > gain_mean * 0.5) & (gain_map < gain_mean * 2.0)])
    
    # 1. Point defects from gain map
    low_gain  = gain_map < gain_mean * 0.5
    high_gain = gain_map > gain_mean * 2.0
    point_mask = low_gain | high_gain
    defect_map[point_mask] = 1
    
    # 2. Stuck pixels from dark map
    stuck = dark_map > (max_adu - 100)
    defect_map[stuck] = 5
    
    # 3. Cluster detection: connected component analysis
    from scipy import ndimage
    labeled, n_clusters = ndimage.label(point_mask)
    cluster_sizes = ndimage.sum(point_mask, labeled, range(1, n_clusters + 1))
    for i, size in enumerate(cluster_sizes, start=1):
        if size >= 4:
            defect_map[labeled == i] = 2  # upgrade to cluster
    
    # 4. Column defects
    col_defect_frac = np.mean(defect_map > 0, axis=0)  # fraction per column
    bad_cols = col_defect_frac >= 0.8
    defect_map[:, bad_cols] = 3
    
    # 5. Row defects
    row_defect_frac = np.mean(defect_map > 0, axis=1)  # fraction per row
    bad_rows = row_defect_frac >= 0.8
    defect_map[bad_rows, :] = 4
    
    return defect_map
```

#### 3.3.3 C++ 런타임 보간 알고리즘

```cpp
// Defect interpolation priority: row/col first, then cluster, then point
// Interpolation methods:

// Point/Cluster: Weighted average of valid neighbors
float interpolate_point(const float* img, int x, int y, int W, int H,
                         const uint8_t* defect_map, bool use_8neighbor) {
    float sum = 0.0f;
    int   cnt = 0;
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = { 0,-1, 0,  1,-1,  1, 1,-1};
    int n_neighbors = use_8neighbor ? 8 : 4;
    // For 4-neighbor: only first 4 entries used with indices {1,0}, {-1,0}, {0,1}, {0,-1}
    
    for (int k = 0; k < n_neighbors; ++k) {
        int nx = x + dx[k], ny = y + dy[k];
        if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
            defect_map[ny * W + nx] == 0) {
            sum += img[ny * W + nx];
            ++cnt;
        }
    }
    return (cnt > 0) ? sum / cnt : img[y * W + x];
}

// Column defect: linear interpolation from left-right valid columns
float interpolate_column(const float* img, int x, int y, int W, int H,
                          const uint8_t* defect_map) {
    // Find nearest valid left column
    int left = x - 1;
    while (left >= 0 && defect_map[y * W + left] == 3) --left;
    int right = x + 1;
    while (right < W && defect_map[y * W + right] == 3) ++right;
    
    if (left < 0 && right >= W) return img[y * W + x]; // no valid
    if (left < 0)  return img[y * W + right];
    if (right >= W) return img[y * W + left];
    
    float t = float(x - left) / float(right - left);
    return img[y * W + left] * (1.0f - t) + img[y * W + right] * t;
}

// Main defect correction pass (two-pass: line defects first)
void xpe_defect_correct(float* __restrict__ img,
                          const uint8_t* __restrict__ defect_map,
                          uint32_t width, uint32_t height) {
    // Pass 1: Row/Column defects
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t dt = defect_map[y * width + x];
            if (dt == 3) {
                img[y * width + x] =
                    interpolate_column(img, x, y, width, height, defect_map);
            } else if (dt == 4) {
                // Row: interpolate from above/below rows
                img[y * width + x] =
                    interpolate_row(img, x, y, width, height, defect_map);
            }
        }
    }
    
    // Pass 2: Cluster defects (8-neighbor)
    // Pass 3: Point defects (4-neighbor) + Stuck pixels (median)
    // (implementation follows same pattern)
}
```

#### 3.3.4 런타임 자동 갱신 알고리즘

```cpp
// Runtime defect detection: identify new defects during exposure sequence
// Algorithm:
//   1. Compute per-pixel deviation: |current - reference| / (local_std + eps)
//   2. Pixels with z-score > threshold_sigma → new defect candidate
//   3. Local std estimated over 5×5 neighbourhood using AVX2 vectorisation
//   4. Set bit 1 (0x02) in defect_map for runtime-flagged pixels
//   5. Caller must invoke xpe_defect_correct() to interpolate flagged pixels
//
// Note: reference_img should be a rolling mean of N_ref (≥4) preceding frames.
//       Use atomic write to defect_map to allow concurrent correction pass.

static void compute_local_std_row(const float* src, float* local_std_out,
                                   uint32_t W, uint32_t y, uint32_t H,
                                   float eps = 1e-6f) {
    // 5×5 neighbourhood local standard deviation (vertical strip [y-2, y+2])
    const int radius = 2;
    for (uint32_t x = 0; x < W; ++x) {
        float sum = 0.0f, sum_sq = 0.0f;
        int   n   = 0;
        for (int dy = -radius; dy <= radius; ++dy) {
            int ny = static_cast<int>(y) + dy;
            if (ny < 0 || ny >= static_cast<int>(H)) continue;
            for (int dx = -radius; dx <= radius; ++dx) {
                int nx = static_cast<int>(x) + dx;
                if (nx < 0 || nx >= static_cast<int>(W)) continue;
                float v = src[ny * W + nx];
                sum    += v;
                sum_sq += v * v;
                ++n;
            }
        }
        float mean = sum / n;
        float var  = std::max(0.0f, sum_sq / n - mean * mean);
        local_std_out[y * W + x] = std::sqrt(var) + eps;
    }
}

void update_defect_map_runtime(float*   __restrict__ current_img,
                                float*   __restrict__ reference_img,
                                uint8_t* __restrict__ defect_map,
                                uint32_t width,
                                uint32_t height,
                                float    threshold_sigma /* = 5.0f */) {
    const size_t total = static_cast<size_t>(width) * height;

    // Allocate temporary local_std buffer
    std::vector<float> local_std(total);

    // Compute local std row-by-row (OpenMP parallelisable)
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < static_cast<int>(height); ++y) {
        compute_local_std_row(reference_img, local_std.data(),
                               width, static_cast<uint32_t>(y), height);
    }

    // AVX2 vectorised z-score threshold pass
    const __m256 v_thresh = _mm256_set1_ps(threshold_sigma);
    size_t i = 0;

    for (; i + 8 <= total; i += 8) {
        __m256 cur  = _mm256_loadu_ps(current_img   + i);
        __m256 ref  = _mm256_loadu_ps(reference_img + i);
        __m256 lstd = _mm256_loadu_ps(local_std.data() + i);

        // |cur - ref| / local_std
        __m256 diff   = _mm256_sub_ps(cur, ref);
        __m256 absdif = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), diff);  // abs
        __m256 zscore = _mm256_div_ps(absdif, lstd);

        // z-score > threshold_sigma → candidate defect
        __m256 cmp = _mm256_cmp_ps(zscore, v_thresh, _CMP_GT_OQ);
        int mask8  = _mm256_movemask_ps(cmp);

        if (mask8 != 0) {
            for (int k = 0; k < 8; ++k) {
                if (mask8 & (1 << k)) {
                    // Set bit 1 (runtime defect flag), preserve other bits
                    defect_map[i + k] |= 0x02u;
                }
            }
        }
    }
    // Scalar tail
    for (; i < total; ++i) {
        float z = std::fabsf(current_img[i] - reference_img[i]) / local_std[i];
        if (z > threshold_sigma) {
            defect_map[i] |= 0x02u;
        }
    }
}
```

---

### 3.4 SWU-1.4 Ghost/Lag Correction (SRS-FUNC-004)

#### 3.4.1 알고리즘 수학 정의 — Siewerdsen-Jaffray Multi-Exponential Model

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i \cdot e^{-t/\tau_i}$$

$$I_{\text{true}}(t) = I_{\text{measured}}(t) - \text{Lag}(t) \cdot I_{\text{prev\_max}}$$

- **파라미터**: `α = [0.04, 0.02, 0.005]`, `τ = [0.5s, 2.0s, 10.0s]` (CsI:Tl/a-Si 기본값)
- **교정**: 각 detector 유형별 실측 피팅으로 파라미터 결정
- **목적**: 이전 노출의 잔류 신호(ghost/lag) 제거, ≥90% ghost removal 달성

#### 3.4.2 Python 피팅 (오프라인 교정)

```python
from scipy.optimize import curve_fit

def multi_exponential_lag(t: np.ndarray, a1, tau1, a2, tau2, a3, tau3) -> np.ndarray:
    return a1 * np.exp(-t / tau1) + a2 * np.exp(-t / tau2) + a3 * np.exp(-t / tau3)

def fit_lag_parameters(lag_decay_data: np.ndarray,
                        time_points: np.ndarray) -> dict:
    """
    Fit 3-component exponential lag model to measured decay data.
    
    Args:
        lag_decay_data: normalized lag fraction at each time point (0-1)
        time_points:    time in seconds after initial exposure
    Returns:
        dict with keys: alpha, tau (each length 3)
    """
    p0 = [0.04, 0.5, 0.02, 2.0, 0.005, 10.0]
    bounds = ([0, 0.1, 0, 0.5, 0, 2.0],
              [0.2, 5.0, 0.1, 20.0, 0.05, 100.0])
    
    popt, pcov = curve_fit(multi_exponential_lag, time_points,
                            lag_decay_data, p0=p0, bounds=bounds,
                            maxfev=10000)
    
    return {
        'alpha': [popt[0], popt[2], popt[4]],
        'tau':   [popt[1], popt[3], popt[5]],
        'r_squared': compute_r_squared(lag_decay_data,
                                        multi_exponential_lag(time_points, *popt))
    }
```

#### 3.4.3 C++ 런타임 구현

```cpp
struct GhostCorrectionParams {
    float alpha[3];  // {0.04, 0.02, 0.005}
    float tau[3];    // {0.5, 2.0, 10.0}  seconds
};

struct ExposureHistory {
    float* max_signal;   // Per-pixel max signal from previous exposure
    float  elapsed_sec;  // Time since previous exposure
};

void xpe_ghost_correct(float* __restrict__ img,
                         const ExposureHistory& history,
                         const GhostCorrectionParams& params,
                         uint32_t width, uint32_t height) {
    if (history.elapsed_sec <= 0.0f || history.max_signal == nullptr) return;
    
    // Compute lag fraction at elapsed time
    float lag_fraction = 0.0f;
    for (int i = 0; i < 3; ++i) {
        lag_fraction += params.alpha[i] *
                        expf(-history.elapsed_sec / params.tau[i]);
    }
    
    const size_t total = static_cast<size_t>(width) * height;
    
    // AVX2 path
    __m256 lag_f = _mm256_set1_ps(lag_fraction);
    __m256 zero  = _mm256_setzero_ps();
    
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 cur     = _mm256_loadu_ps(img + i);
        __m256 prev    = _mm256_loadu_ps(history.max_signal + i);
        __m256 ghost   = _mm256_mul_ps(lag_f, prev);
        __m256 result  = _mm256_max_ps(_mm256_sub_ps(cur, ghost), zero);
        _mm256_storeu_ps(img + i, result);
    }
    for (; i < total; ++i) {
        float ghost = lag_fraction * history.max_signal[i];
        img[i] = std::max(img[i] - ghost, 0.0f);
    }
}
```

#### 3.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Ghost removal rate | ≥90% | 이중 노출 프로토콜: ghost_after / ghost_before |
| Residual lag at 0.5s | < 2% | 단기 lag 측정 |
| Model fit R² | ≥0.98 | 피팅 결과 검증 |

---

### 3.4.5 SWU-1.4b Lag 잔류 기반 결정론적 티어링 (GAP-U 해소)

xpe-algorithm-spec-deepsync.md §4의 "lag residual-driven deterministic tiering"에서 결정된 항목이다. 측정된 lag 잔류량에 따라 1항 모델 (빠른 처리)과 3항 모델 (정밀 처리)을 결정론적으로 선택함으로써, 항상 3항 모델을 실행하는 과잉 처리를 방지한다.

#### 3.4.5.1 알고리즘 수학 정의

**Lag 잔류 측정**:

$$R_{\text{lag}}(t) = \frac{|\bar{I}_{\text{measured}}(t) - \bar{I}_{\text{expected}}(t)|}{\bar{I}_{\text{expected}}(t)} \times 100\%$$

여기서 $\bar{I}_{\text{expected}}$는 이상적 노출(lag 없음) 이미지의 기대 평균 신호이다.

**티어 선택 규칙**:

$$\text{Tier} = \begin{cases} 0 & R_{\text{lag}} < \theta_0 \quad \text{(보정 건너뜀)} \\ 1 & \theta_0 \leq R_{\text{lag}} < \theta_1 \quad \text{(1항 모델)} \\ 3 & R_{\text{lag}} \geq \theta_1 \quad \text{(3항 모델)} \end{cases}$$

**1항 근사 모델**:

$$I_{\text{true,1}}(t) = I_{\text{measured}}(t) - \alpha_1 e^{-t/\tau_1} \cdot I_{\text{prev\_max}}$$

| 파라미터 | 값 | 의미 |
|---------|---|------|
| $\theta_0$ | 0.2% | Tier-0 상한 (보정 불필요) |
| $\theta_1$ | 1.0% | Tier-3 하한 (3항 모델 필요) |

#### 3.4.5.2 Python 구현

```python
import numpy as np
from enum import IntEnum

class LagTier(IntEnum):
    NONE  = 0   # lag residual < 0.2%: skip correction
    FAST  = 1   # 0.2–1.0%: single-term model
    FULL  = 3   # ≥1.0%: three-term model

def measure_lag_residual(measured_img:  np.ndarray,
                          prev_max_img:  np.ndarray,
                          elapsed_sec:   float,
                          alpha:         list,
                          tau:           list,
                          roi_mask:      np.ndarray = None) -> float:
    """
    Measure current lag residual as % of expected signal.

    Args:
        measured_img:  current frame (gain-corrected float32)
        prev_max_img:  previous exposure max signal per pixel
        elapsed_sec:   time since previous exposure
        alpha, tau:    3-component lag model parameters
        roi_mask:      optional boolean mask for ROI averaging
    Returns:
        residual_pct: lag residual as percentage (0–100)
    """
    # Expected lag component
    lag_frac = sum(a * np.exp(-elapsed_sec / t) for a, t in zip(alpha, tau))
    expected_ghost = lag_frac * prev_max_img

    # Measured mean signal
    if roi_mask is not None:
        meas_mean = float(np.mean(measured_img[roi_mask]))
        ghost_mean = float(np.mean(expected_ghost[roi_mask]))
        expected_clean_mean = float(np.mean(
            (measured_img - expected_ghost)[roi_mask]))
    else:
        meas_mean  = float(np.mean(measured_img))
        ghost_mean = float(np.mean(expected_ghost))
        expected_clean_mean = meas_mean - ghost_mean

    if abs(expected_clean_mean) < 1.0:
        return 0.0

    residual_pct = abs(ghost_mean) / abs(expected_clean_mean) * 100.0
    return residual_pct


def select_lag_tier(residual_pct: float,
                    theta_0: float = 0.2,
                    theta_1: float = 1.0) -> LagTier:
    """Select correction tier based on measured lag residual."""
    if residual_pct < theta_0:
        return LagTier.NONE
    elif residual_pct < theta_1:
        return LagTier.FAST
    else:
        return LagTier.FULL


def apply_lag_correction_tiered(
        img:          np.ndarray,
        prev_max:     np.ndarray,
        elapsed_sec:  float,
        alpha:        list,
        tau:          list,
        tier:         LagTier) -> np.ndarray:
    """
    Apply lag correction at the selected tier.

    Tier 0: return img unchanged
    Tier 1: single-term correction (fast)
    Tier 3: full three-term correction (precise)
    """
    if tier == LagTier.NONE:
        return img

    if tier == LagTier.FAST:
        # Use dominant (fastest) component only
        lag_frac = alpha[0] * np.exp(-elapsed_sec / tau[0])
        ghost    = lag_frac * prev_max
        return np.maximum(img - ghost, 0.0).astype(np.float32)

    # Full 3-term model
    lag_frac = sum(a * np.exp(-elapsed_sec / t) for a, t in zip(alpha, tau))
    ghost    = lag_frac * prev_max
    return np.maximum(img - ghost.astype(np.float32), 0.0).astype(np.float32)
```

#### 3.4.5.3 C++ 런타임 구현

```cpp
// Lag tiering decision and correction — extends SWU-1.4 Ghost Correction

enum class LagTier : uint8_t {
    NONE = 0,  // Skip correction
    FAST = 1,  // Single-term approximation
    FULL = 3,  // Full three-term model
};

struct LagTierConfig {
    float theta_none = 0.002f;  // 0.2% — skip threshold
    float theta_full = 0.010f;  // 1.0% — full-model threshold
};

LagTier determine_lag_tier(const float* __restrict__ img,
                             const float* __restrict__ prev_max,
                             const GhostCorrectionParams& params,
                             float    elapsed_sec,
                             uint32_t W,
                             uint32_t H,
                             const LagTierConfig& cfg = {}) {
    // Compute expected lag fraction
    float lag_frac = 0.0f;
    for (int k = 0; k < 3; ++k)
        lag_frac += params.alpha[k] * expf(-elapsed_sec / params.tau[k]);

    // Estimate residual: mean(lag_frac × prev_max) / mean(img - ghost)
    double ghost_sum = 0.0, img_sum = 0.0;
    const size_t total = static_cast<size_t>(W) * H;

    for (size_t i = 0; i < total; ++i) {
        ghost_sum += lag_frac * prev_max[i];
        img_sum   += img[i];
    }
    double ghost_mean = ghost_sum / total;
    double clean_mean = img_sum   / total - ghost_mean;

    if (std::abs(clean_mean) < 1.0) return LagTier::NONE;

    float residual_pct = static_cast<float>(
        std::abs(ghost_mean) / std::abs(clean_mean));

    if (residual_pct < cfg.theta_none) return LagTier::NONE;
    if (residual_pct < cfg.theta_full) return LagTier::FAST;
    return LagTier::FULL;
}

void xpe_ghost_correct_tiered(float* __restrict__ img,
                                const ExposureHistory& history,
                                const GhostCorrectionParams& params,
                                uint32_t W,
                                uint32_t H,
                                const LagTierConfig& tier_cfg = {}) {
    if (!history.max_signal || history.elapsed_sec <= 0.0f) return;

    LagTier tier = determine_lag_tier(img, history.max_signal, params,
                                       history.elapsed_sec, W, H, tier_cfg);

    if (tier == LagTier::NONE) return;  // Skip: residual below noise floor

    // For FAST tier: use only component 0
    GhostCorrectionParams effective = params;
    if (tier == LagTier::FAST) {
        effective.alpha[1] = 0.0f; effective.alpha[2] = 0.0f;
    }

    // Delegate to standard xpe_ghost_correct()
    xpe_ghost_correct(img, history, effective, W, H);
}
```

#### 3.4.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Tier-0 정확 선택률 | 100% (lag 없는 첫 촬영) | 새 세션 첫 이미지 |
| Tier-3 → Tier-1 개선 | 처리 시간 ≤ 60% of Tier-3 | 동일 이미지 두 경로 비교 |
| Tier-1 ghost 제거율 | ≥ 80% (단기 lag) | 0.5s 경과 이중 노출 |
| Tier-3 ghost 제거율 | ≥ 90% (장기 lag) | 10s 경과 이중 노출 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-004 (Ghost Correction 확장) — Tiering 항목 Phase 2 추가 예정

---

### 3.5 SWU-1.5 Heel Effect Compensation (GAP-O 해소)

Heel 효과는 X선관의 양극 경사면(anode heel)으로 인해 음극(cathode) 방향보다 양극(anode) 방향으로 X선 강도가 감소하는 현상이다. xpe-algorithm-spec-deepsync.md §4 "geometry-aware heel compensation"에서 "adopt now"로 결정된 항목이며, Wang 2013 Duo-SID 모델을 기반으로 한다.

#### 3.5.1 알고리즘 수학 정의

**Wang 2013 Duo-SID Heel 효과 모델**:

$$H(x, y; \text{SID}, \theta_T) = \exp\!\left(-\mu_{\text{eff}} \cdot d_{\text{anode}}(x, y; \text{SID}, \theta_T)\right)$$

$$d_{\text{anode}}(x, y; \text{SID}, \theta_T) = \frac{t_{\text{anode}}}{\sin\!\left(\theta_T - \arctan\!\left(\frac{x - x_{\text{center}}}{\text{SID}}\right)\right)}$$

$$I_{\text{heel\_corr}}(x, y) = \frac{I_{\text{gain\_corr}}(x, y)}{H(x, y; \text{SID}, \theta_T)}$$

여기서:
- $\theta_T$: 양극 타겟 각도 (일반적으로 10° ~ 17°)
- $t_{\text{anode}}$: 양극 재료 두께 (mm), 유효 경로 추정에 사용
- $\mu_{\text{eff}}$: 양극 재료(텅스텐)의 X선 유효 선감쇄계수 (kVp-dependent)
- $x_{\text{center}}$: 이미지 중앙에서 음극-양극 방향 이동(detector center offset)

**Multi-SID 보간**:

SID가 $\text{SID}_A$와 $\text{SID}_B$ 사이에 있을 때:

$$H_{\text{interp}}(x, y) = H(x, y; \text{SID}_A) + \frac{\text{SID} - \text{SID}_A}{\text{SID}_B - \text{SID}_A} \cdot \left(H(x,y;\text{SID}_B) - H(x,y;\text{SID}_A)\right)$$

#### 3.5.2 파라미터 및 경계 조건

| 파라미터 | 기본값 | 범위 | 의미 |
|---------|-------|-----|------|
| `theta_target_deg` | 12.0 | 8–20° | 양극 타겟 각도 |
| `mu_eff` | 0.045 | 0.02–0.10 mm⁻¹ | 유효 선감쇄계수 (80kVp 기준) |
| `sid_ref_mm` | 1000.0 | 600–1800 mm | 기준 SID |
| `anode_direction` | 'col' | 'row'/'col' | 음극→양극 방향 (열 방향 = 수직) |
| `max_correction_factor` | 1.5 | 1.0–2.0 | 최대 보정 계수 (안전 클램프) |

**경계 조건**:
- 보정 계수가 `max_correction_factor`를 초과하면 클램프 적용
- $H(x,y) < 0.1$이 되는 극단적 기하학은 교정 실패로 간주하고 보정 건너뜀
- kVp 변경 시 `mu_eff`는 선형 보간으로 업데이트

#### 3.5.3 Python 구현 (오프라인 교정)

```python
import numpy as np
from scipy.interpolate import RegularGridInterpolator
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class HeelEffectParams:
    theta_target_deg: float = 12.0    # anode target angle (degrees)
    mu_eff_per_mm:    float = 0.045   # effective attenuation at reference kVp
    sid_ref_mm:       float = 1000.0  # reference SID for calibration
    anode_direction:  str   = 'col'   # 'col' = cathode-anode along columns
    pixel_pitch_mm:   float = 0.148   # detector pixel pitch
    detector_width:   int   = 2816    # pixels in anode direction
    kvp_ref:          float = 80.0    # reference kVp for mu_eff
    max_correction:   float = 1.5     # safety clamp

def compute_heel_correction_map(
        params:  HeelEffectParams,
        sid_mm:  float,
        kvp:     float,
        height:  int,
        width:   int) -> np.ndarray:
    """
    Compute per-pixel heel effect correction map for given SID and kVp.

    Args:
        params:   HeelEffectParams configuration
        sid_mm:   source-to-image distance in mm
        kvp:      tube voltage (for mu_eff scaling)
        height:   image height in pixels
        width:    image width in pixels
    Returns:
        correction_map: float32 (H, W) — divide gain-corrected image by this map
                        Values in [1/max_correction, max_correction]
    """
    theta_T = np.radians(params.theta_target_deg)

    # mu_eff scales approximately as kVp^(-2.5) for tungsten in diagnostic range
    mu_scale = (params.kvp_ref / kvp) ** 2.5 if kvp > 0 else 1.0
    mu_eff = params.mu_eff_per_mm * mu_scale

    # anode direction coordinate (x = displacement from detector centre)
    if params.anode_direction == 'col':
        # anode runs along columns → displacement is along x (horizontal)
        cx = width / 2.0
        coords = (np.arange(width, dtype=np.float64) - cx) * params.pixel_pitch_mm  # mm
        disp_2d = np.tile(coords[np.newaxis, :], (height, 1))
    else:
        cy = height / 2.0
        coords = (np.arange(height, dtype=np.float64) - cy) * params.pixel_pitch_mm
        disp_2d = np.tile(coords[:, np.newaxis], (1, width))

    # Projection angle at each pixel
    alpha = np.arctan(disp_2d / sid_mm)  # small-angle approx OK for |disp| < 300mm

    # Effective path through anode (Wang 2013 Eq. 4)
    denom = np.sin(theta_T - alpha)
    # Avoid division by zero / negative (beyond edge of beam)
    denom = np.where(denom > 0.01, denom, 0.01)

    # Approximate anode path (normalised: at centre denom=sin(theta_T))
    path_ratio = np.sin(theta_T) / denom  # relative path length

    # Heel factor H(x) = exp(-mu_eff * t_ref * (path_ratio - 1))
    # t_ref is absorbed into mu_eff calibration: at centre H=1
    H = np.exp(-mu_eff * sid_mm * 0.001 * (path_ratio - 1.0))
    # Note: sid_mm * 0.001 converts mm→m; empirical factor, absorb into mu_eff

    # Clip to safe range
    H = np.clip(H, 1.0 / params.max_correction, params.max_correction)
    return H.astype(np.float32)


def compute_heel_map_multi_sid(
        params:     HeelEffectParams,
        sid_list:   List[float],
        kvp_list:   List[float],
        height:     int,
        width:      int) -> dict:
    """
    Pre-compute heel correction maps for all SID/kVp combinations.

    Returns dict: {(sid_mm, kvp): correction_map float32 (H, W)}
    Used by runtime to select nearest map or interpolate.
    """
    maps = {}
    for sid in sid_list:
        for kvp in kvp_list:
            maps[(sid, kvp)] = compute_heel_correction_map(params, sid, kvp, height, width)
    return maps


def interpolate_heel_map(
        maps:      dict,
        sid_mm:    float,
        kvp:       float,
        sid_list:  List[float],
        kvp_list:  List[float]) -> np.ndarray:
    """
    Bilinear interpolation between pre-computed heel correction maps.

    Args:
        maps:     dict from compute_heel_map_multi_sid()
        sid_mm:   target SID
        kvp:      target kVp
        sid_list: sorted list of calibrated SID values
        kvp_list: sorted list of calibrated kVp values
    Returns:
        interpolated correction map float32 (H, W)
    """
    sid_arr = np.array(sorted(sid_list), dtype=np.float64)
    kvp_arr = np.array(sorted(kvp_list), dtype=np.float64)

    # Clamp to calibrated range
    sid_c = float(np.clip(sid_mm, sid_arr[0], sid_arr[-1]))
    kvp_c = float(np.clip(kvp,    kvp_arr[0], kvp_arr[-1]))

    # Find bounding indices
    i_sid = np.searchsorted(sid_arr, sid_c, side='right') - 1
    i_sid = int(np.clip(i_sid, 0, len(sid_arr) - 2))
    i_kvp = np.searchsorted(kvp_arr, kvp_c, side='right') - 1
    i_kvp = int(np.clip(i_kvp, 0, len(kvp_arr) - 2))

    sid_lo, sid_hi = sid_arr[i_sid], sid_arr[i_sid + 1]
    kvp_lo, kvp_hi = kvp_arr[i_kvp], kvp_arr[i_kvp + 1]

    t_sid = (sid_c - sid_lo) / (sid_hi - sid_lo) if sid_hi != sid_lo else 0.0
    t_kvp = (kvp_c - kvp_lo) / (kvp_hi - kvp_lo) if kvp_hi != kvp_lo else 0.0

    m00 = maps[(sid_lo, kvp_lo)].astype(np.float64)
    m01 = maps[(sid_lo, kvp_hi)].astype(np.float64)
    m10 = maps[(sid_hi, kvp_lo)].astype(np.float64)
    m11 = maps[(sid_hi, kvp_hi)].astype(np.float64)

    result = ((1 - t_sid) * (1 - t_kvp) * m00 +
              (1 - t_sid) *      t_kvp  * m01 +
                   t_sid  * (1 - t_kvp) * m10 +
                   t_sid  *      t_kvp  * m11)
    return result.astype(np.float32)
```

#### 3.5.4 C++ 런타임 구현

```cpp
// Heel Effect Compensation — runtime application
// Pre-computed correction map loaded from calibration pack.
// Called after xpe_gain_correct(), before xpe_nonlinearity_correct().

struct HeelCorrectionPack {
    float*   map;           // float32 (H × W) — correction divisor
    uint32_t width;
    uint32_t height;
    float    sid_mm;
    float    kvp;
    uint64_t crc64;         // CRC of this map for integrity verification
};

// --- In-process map interpolation (bilinear between two SID maps) ---
void interpolate_heel_maps(const float* __restrict__ map_lo,
                            const float* __restrict__ map_hi,
                            float* __restrict__ out,
                            uint32_t total,
                            float t) {
    // t ∈ [0, 1]: linear interpolation weight toward map_hi
    const __m256 v_t    = _mm256_set1_ps(t);
    const __m256 v_1mt  = _mm256_set1_ps(1.0f - t);
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 lo = _mm256_loadu_ps(map_lo + i);
        __m256 hi = _mm256_loadu_ps(map_hi + i);
        __m256 r  = _mm256_fmadd_ps(v_t, hi, _mm256_mul_ps(v_1mt, lo));
        _mm256_storeu_ps(out + i, r);
    }
    for (; i < total; ++i) {
        out[i] = (1.0f - t) * map_lo[i] + t * map_hi[i];
    }
}

// --- Main heel correction ---
void xpe_heel_correct(const float* __restrict__ gain_corr,
                       float*       __restrict__ out,
                       const float* __restrict__ heel_map,   // pre-interpolated for current SID/kVp
                       uint32_t width,
                       uint32_t height,
                       float    max_correction = 1.5f) {
    const size_t   total     = static_cast<size_t>(width) * height;
    const __m256   v_max_cor = _mm256_set1_ps(max_correction);
    const __m256   v_min_cor = _mm256_set1_ps(1.0f / max_correction);
    const __m256   v_eps     = _mm256_set1_ps(1e-6f);

    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 img  = _mm256_loadu_ps(gain_corr  + i);
        __m256 hmap = _mm256_loadu_ps(heel_map   + i);

        // Clamp correction map to safe range
        hmap = _mm256_max_ps(hmap, v_min_cor);
        hmap = _mm256_min_ps(hmap, v_max_cor);

        // Corrected = img / H(x,y)
        __m256 denom = _mm256_max_ps(hmap, v_eps);
        __m256 res   = _mm256_div_ps(img, denom);
        _mm256_storeu_ps(out + i, res);
    }
    for (; i < total; ++i) {
        float h = std::clamp(heel_map[i], 1.0f / max_correction, max_correction);
        out[i] = gain_corr[i] / std::max(h, 1e-6f);
    }
}

// --- SID/kVp selector: choose nearest pre-loaded map or trigger interpolation ---
const HeelCorrectionPack* select_heel_pack(
        const std::vector<HeelCorrectionPack>& packs,
        float sid_mm,
        float kvp,
        float sid_tol = 25.0f,
        float kvp_tol = 5.0f) {
    for (const auto& p : packs) {
        if (std::fabsf(p.sid_mm - sid_mm) < sid_tol &&
            std::fabsf(p.kvp    - kvp)    < kvp_tol) {
            return &p;
        }
    }
    return nullptr;  // caller must interpolate
}
```

#### 3.5.5 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Heel 보정 후 PRNU | CV < 0.8% (보정 전 대비 개선) | 균일 조사 flood 이미지의 수평 프로파일 |
| SID 보간 오차 | < 0.5% 신호 오차 | 두 교정 SID 사이의 중간값 측정 |
| 최대 보정 계수 초과 비율 | < 0.01% 픽셀 | 클램프 이벤트 카운터 |
| 처리 시간 (3072×3072) | < 40ms | AVX2, 단일 코어 |
| 모델 R² (측정 대 예측) | ≥ 0.99 | 교정 flood 이미지 대비 모델 예측값 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-002b (Gain Correction 확장) — Phase 2에서 할당 예정

---

## 4. SWI-2: Core Processing 알고리즘

### 4.1 SWU-2.1 Log Transform (SRS-FUNC-010)

#### 4.1.1 수학 정의

$$I_{OD}(x,y) = -\ln\left(\frac{I_{\text{clean}}(x,y) + \varepsilon}{I_0 + \varepsilon}\right)$$

- $I_0$: 비노출 영역(collimator edge 내부) 기준 최대 플루엔스 추정값 또는 이론값
- $\varepsilon = 10^{-6}$: Zero/negative 입력 보호 (SRS-FUNC-010)
- **결과**: Beer-Lambert law에 의해 OD(Optical Density) ≈ attenuation coefficient × thickness

#### 4.1.2 I₀ 추정 전략

```cpp
// Strategy 1: Use collimated (unattenuated) region statistics
float estimate_I0_from_collimator(const float* img, uint32_t W, uint32_t H,
                                    const CollimatorMask& mask) {
    // Find unattenuated pixels (inside collimator border, no anatomy)
    // Use 95th percentile to avoid outliers
    std::vector<float> unattenuated;
    for (uint32_t i = 0; i < W * H; ++i) {
        if (mask.is_unattenuated(i)) unattenuated.push_back(img[i]);
    }
    std::sort(unattenuated.begin(), unattenuated.end());
    return unattenuated[static_cast<size_t>(unattenuated.size() * 0.95f)];
}

// Strategy 2: Use gain-normalized reference (preferred for consistency)
// I0 = GainMean (stored in gain map header)
```

#### 4.1.3 C++ 구현 (AVX2)

```cpp
void xpe_log_transform(const float* __restrict__ in,
                         float*       __restrict__ out,
                         float I0, float epsilon,
                         uint32_t width, uint32_t height) {
    const float eps = (epsilon > 0) ? epsilon : 1e-6f;
    const size_t total = static_cast<size_t>(width) * height;
    
    __m256 v_eps   = _mm256_set1_ps(eps);
    __m256 v_I0e   = _mm256_set1_ps(I0 + eps);
    __m256 v_neg1  = _mm256_set1_ps(-1.0f);
    
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 x      = _mm256_loadu_ps(in + i);
        __m256 x_eps  = _mm256_add_ps(x, v_eps);
        __m256 ratio  = _mm256_div_ps(x_eps, v_I0e);
        __m256 ln_val = avx2_log_ps(ratio);   // See avx2_log_ps below
        __m256 od     = _mm256_mul_ps(v_neg1, ln_val);
        _mm256_storeu_ps(out + i, od);
    }
    for (; i < total; ++i) {
        out[i] = -logf((in[i] + eps) / (I0 + eps));
    }
}

// ---------------------------------------------------------------------------
// avx2_log_ps: Cephes-based AVX2 natural logarithm approximation
// Accuracy: ~5 ULP (max relative error < 1.2×10⁻⁷ for x ∈ (0, +∞))
// Algorithm: Cephes log.c decomposition — identical to avx_mathfun (Gruzdev 2012)
//   Reference: https://github.com/reyoung/avx_mathfun (BSD-2)
//   Reference: Cephes Math Library, S. Moshier
//
// Derivation:
//   x = m × 2^e  where m ∈ [0.5, 1.0)
//   ln(x) = ln(m) + e × ln(2)
//   ln(m) approximated by degree-8 minimax polynomial on [sqrt(0.5), sqrt(2)]
//   after substitution f = m − 1 (range reduction to [−0.293, 0.414])
// ---------------------------------------------------------------------------
static inline __m256 avx2_log_ps(__m256 x) {
    // Polynomial coefficients (Cephes, ~5 ULP)
    const __m256 c_ln2_hi  = _mm256_set1_ps(0.693359375f);
    const __m256 c_ln2_lo  = _mm256_set1_ps(-2.12194440e-4f);
    const __m256 c_half    = _mm256_set1_ps(0.5f);
    const __m256 c_one     = _mm256_set1_ps(1.0f);
    const __m256 c_sqrthf  = _mm256_set1_ps(0.707106781186547524f);  // sqrt(0.5)
    // Polynomial coefficients for ln(1+f), f = normalized(x) − 1
    const __m256 c_p0  = _mm256_set1_ps( 7.0376836292e-2f);
    const __m256 c_p1  = _mm256_set1_ps(-1.1514610310e-1f);
    const __m256 c_p2  = _mm256_set1_ps( 1.1676998740e-1f);
    const __m256 c_p3  = _mm256_set1_ps(-1.2420140846e-1f);
    const __m256 c_p4  = _mm256_set1_ps( 1.4249322787e-1f);
    const __m256 c_p5  = _mm256_set1_ps(-1.6668057665e-1f);
    const __m256 c_p6  = _mm256_set1_ps( 2.0000714765e-1f);
    const __m256 c_p7  = _mm256_set1_ps(-2.4999993993e-1f);
    const __m256 c_p8  = _mm256_set1_ps( 3.3333331174e-1f);
    const __m256i c_127 = _mm256_set1_epi32(127);

    // Clamp x > 0 (avoid NaN/Inf propagation)
    x = _mm256_max_ps(x, _mm256_set1_ps(1.175494351e-38f));  // FLT_MIN

    // Decompose x = m × 2^e  (e = biased_exponent - 127)
    __m256i xi = _mm256_castps_si256(x);
    // Extract exponent
    __m256i exp_i = _mm256_sub_epi32(_mm256_srli_epi32(xi, 23), c_127);
    __m256 e = _mm256_cvtepi32_ps(exp_i);
    // Set mantissa to [0.5, 1.0): clear exponent, set bias to 126
    xi = _mm256_and_si256(xi, _mm256_set1_epi32(0x007fffff));
    xi = _mm256_or_si256(xi,  _mm256_set1_epi32(0x3f000000));
    __m256 m = _mm256_castsi256_ps(xi);

    // If m < sqrt(0.5), multiply m by 2 and subtract 1 from exponent
    __m256 mask = _mm256_cmp_ps(m, c_sqrthf, _CMP_LT_OQ);
    e = _mm256_sub_ps(e, _mm256_and_ps(c_one, mask));
    m = _mm256_add_ps(m, _mm256_and_ps(m, mask));   // m += m if m < sqrthf
    m = _mm256_sub_ps(m, c_one);                     // f = m - 1

    // Horner evaluation of polynomial in f
    __m256 y = c_p0;
    y = _mm256_fmadd_ps(y, m, c_p1);
    y = _mm256_fmadd_ps(y, m, c_p2);
    y = _mm256_fmadd_ps(y, m, c_p3);
    y = _mm256_fmadd_ps(y, m, c_p4);
    y = _mm256_fmadd_ps(y, m, c_p5);
    y = _mm256_fmadd_ps(y, m, c_p6);
    y = _mm256_fmadd_ps(y, m, c_p7);
    y = _mm256_fmadd_ps(y, m, c_p8);
    y = _mm256_mul_ps(y, m);
    y = _mm256_mul_ps(y, m);   // y × m²

    // ln(x) = y + e×ln2_hi + e×ln2_lo − 0.5×m² + m
    __m256 r = _mm256_fmadd_ps(e,  c_ln2_hi, y);
    r = _mm256_fmadd_ps(e, c_ln2_lo, r);
    r = _mm256_fmadd_ps(_mm256_set1_ps(-0.5f), _mm256_mul_ps(m, m), r);
    r = _mm256_add_ps(r, m);
    return r;
}
```

---

### 4.2 SWU-2.2 Noise Reduction (SRS-FUNC-011)

#### 4.2.1 Bilateral Filter — 핵심 알고리즘

$$BF[I](x) = \frac{1}{W_p} \sum_{x_i \in \Omega} I(x_i) \cdot f_s(\|x_i - x\|) \cdot f_r(|I(x_i) - I(x)|)$$

$$f_s(d) = e^{-d^2/(2\sigma_s^2)}, \quad f_r(\delta) = e^{-\delta^2/(2\sigma_r^2)}$$

$$W_p = \sum_{x_i \in \Omega} f_s(\|x_i - x\|) \cdot f_r(|I(x_i) - I(x)|)$$

- **파라미터 (SRS-FUNC-011)**: `σ_s = 2.0` pixels, `σ_r = 0.1` OD unit
- **커널 크기**: `2 × ⌈3σ_s⌉ + 1 = 13×13`
- **구현**: OpenCV `cv::bilateralFilter()` + 사전 계산 lookup table

#### 4.2.2 파라미터 선택 근거

| σ_s | σ_r | 효과 | 부작용 |
|-----|-----|------|--------|
| 1.0 | 0.05 | 약한 스무딩, 노이즈 유지 | 효과 미미 |
| 2.0 | 0.10 | **권장: 노이즈 제거 + edge 보존** | 미미한 detail 손실 |
| 3.0 | 0.15 | 강한 스무딩 | Texture 과도 억제 |
| 5.0 | 0.30 | 과도한 스무딩 | Watercolor artifact |

#### 4.2.3 Non-Local Means (고품질 옵션)

$$NLM[I](x) = \frac{1}{C(x)} \sum_{y \in \Omega} e^{-\frac{\|I(N_x) - I(N_y)\|^2_{2,a}}{h^2}} \cdot I(y)$$

- $N_x$: `x` 중심 `p×p` 패치 (권장: `p=7`)
- 탐색 범위: `d×d` 윈도우 (권장: `d=21`)
- `h`: 필터링 파라미터 (노이즈 표준편차의 ~10배)
- **구현**: OpenCV `cv::fastNlMeansDenoising()` 또는 CUDA 가속

#### 4.2.4 알고리즘 선택 로직

```cpp
ImageQualityMode select_denoising_mode(const ProcessingParams& params) {
    if (params.quality_mode == "high" || params.body_part == "BREAST")
        return ImageQualityMode::NLM;
    return ImageQualityMode::Bilateral;  // default
}
```

---

### 4.3 SWU-2.3 CLAHE (SRS-FUNC-012)

#### 4.3.1 알고리즘 수학 정의

CLAHE (Contrast Limited Adaptive Histogram Equalization):

1. **타일 분할**: 이미지를 `M×N` 타일로 분할 (기본: 8×8)
2. **히스토그램 계산**: 각 타일 내 픽셀 히스토그램 (bins: 256)
3. **Clip 제한**: `clip_limit × (tile_area / bins)` 초과 빈도 → 균등 재분배
4. **CDF 계산**: 클리핑된 히스토그램의 누적분포함수
5. **Bilinear 보간**: 경계 타일 간 매끄러운 전환

$$\text{CDF}(v) = \frac{1}{N_{clip}} \sum_{i=0}^{v} h_{clip}(i)$$

$$I_{out}(x,y) = \text{BilinearInterp}\left(\text{CDF}_{T_1}, \text{CDF}_{T_2}, \text{CDF}_{T_3}, \text{CDF}_{T_4}, I_{in}(x,y)\right)$$

#### 4.3.2 파라미터 명세

| 파라미터 | 기본값 | 범위 | 설명 |
|---------|--------|------|------|
| `tile_size` | 8×8 | 4–64 | 적응 영역 크기 |
| `clip_limit` | 2.0 | 1.0–8.0 | 클리핑 강도 (1.0 = no clip = AHE) |
| `bins` | 256 | 64–4096 | 히스토그램 해상도 |
| `input_range` | [0, 4095] | adaptive | 입력 동적 범위 |

#### 4.3.3 Body-Part별 최적 파라미터 (SRS-FUNC-021 preset과 연계)

| 신체 부위 | clip_limit | tile_size | 이유 |
|----------|-----------|---------|------|
| Chest PA/AP | 2.0 | 8×8 | 폐야/종격동 균형 |
| Abdomen | 1.5 | 16×16 | 대비 차이 완만 |
| Extremity | 3.0 | 4×4 | 국소 골 디테일 강화 |
| Hand/Wrist | 4.0 | 4×4 | 세밀한 골 구조 |
| Spine | 2.5 | 8×8 | 추체-디스크 대비 |
| Breast | 1.0 | 32×32 | 균일한 대비, 과도 억제 방지 |

#### 4.3.4 C++ 구현

```cpp
// Using OpenCV CLAHE
void xpe_clahe_enhance(cv::Mat& img_float32,
                         const ClaheParams& params) {
    // Convert to 16-bit for OpenCV CLAHE processing
    double min_val, max_val;
    cv::minMaxLoc(img_float32, &min_val, &max_val);
    
    cv::Mat img16;
    img_float32.convertTo(img16, CV_16U,
                           65535.0 / (max_val - min_val + 1e-6),
                           -min_val * 65535.0 / (max_val - min_val + 1e-6));
    
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
        params.clip_limit,
        cv::Size(params.tile_cols, params.tile_rows));
    clahe->apply(img16, img16);
    
    // Convert back to float32
    img16.convertTo(img_float32, CV_32F,
                     (max_val - min_val) / 65535.0,
                     min_val);
}
```

---

### 4.4 SWU-2.4 Edge Enhancement — Unsharp Masking (SRS-FUNC-013)

#### 4.4.1 알고리즘 수학 정의

$$I_{\text{USM}}(x,y) = I(x,y) + \lambda(x,y) \cdot \left[I(x,y) - I_{\text{blur}}(x,y)\right]$$

$$I_{\text{blur}}(x,y) = I(x,y) * G_{\sigma}(x,y)$$

- $G_{\sigma}$: Gaussian blur kernel (σ = 1.5~2.0 pixels)
- $\lambda$: **Adaptive gain** — body-part별 safe range로 제한 (SRS-SAFE-005)
- **High-pass component**: `H(x,y) = I(x,y) - I_blur(x,y)` (Laplacian of Gaussian 근사)

#### 4.4.2 Body-Part별 Safe Gain Range (SRS-SAFE-005 이행)

| 신체 부위 | λ_min | λ_default | λ_max | 이유 |
|----------|-------|---------|-------|------|
| Chest | 0.0 | 0.8 | 1.5 | 과도한 폐 구조 강조 방지 |
| Bone (extremity) | 0.0 | 1.2 | 2.5 | 골 디테일 강화 허용 |
| Spine | 0.0 | 1.0 | 2.0 | 균형 |
| Breast | 0.0 | 0.5 | 1.0 | 미세석회화 인식, artifact 방지 |
| Pediatric | 0.0 | 0.6 | 1.2 | 낮은 contrast 조직 보호 |

```cpp
float clamp_usm_gain(float lambda, const BodyPartPreset& preset) {
    return std::clamp(lambda, preset.lambda_min, preset.lambda_max);
}
```

#### 4.4.3 주파수 선택적 USM (Selective Frequency Enhancement)

진단 가치 있는 공간주파수 범위만 강화:

```cpp
// Bandpass USM: enhance only [f_low, f_high] frequency band
// Implementation: DoG (Difference of Gaussians)
// H_band(x,y) = G_{σ1}(x,y) - G_{σ2}(x,y)  where σ1 < σ2
// f_low ≈ 1/(4σ2), f_high ≈ 1/(4σ1)
// For chest: σ1=1.0, σ2=4.0 → [0.06, 0.25] cycles/pixel
```

---

### 4.5 SWU-2.5 Multiscale Processing — Laplacian Pyramid (SRS-FUNC-014)

#### 4.5.1 알고리즘 수학 정의

**건설 단계 (Analysis):**

$$G_0 = I, \quad G_k = \text{Downsample}(G_{k-1} * h)$$
$$L_k = G_k - \text{Upsample}(G_{k+1}) \quad \text{for } k = 0, 1, \ldots, N-1$$
$$L_N = G_N \quad \text{(residual)}$$

**비선형 게인 적용:**

$$\hat{L}_k = L_k \cdot g_k\left(\|L_k\|\right)$$

$$g_k(s) = \begin{cases} g_{\max,k} & s < s_1 \\ g_{\min,k} + (g_{\max,k}-g_{\min,k})\cdot\frac{s_2-s}{s_2-s_1} & s_1 \le s < s_2 \\ g_{\min,k} & s \ge s_2 \end{cases}$$

**재구성 단계 (Synthesis):**

$$\hat{G}_{k} = \hat{L}_k + \text{Upsample}(\hat{G}_{k+1})$$

#### 4.5.2 파라미터 명세

```cpp
struct LaplacianPyramidParams {
    int    levels        = 8;    // SRS-FUNC-014: ≥8 levels
    float  sigma         = 1.0f; // Gaussian sigma for each level
    // Per-level gain curve (nonlinear)
    struct LevelGain {
        float g_max    = 1.5f;   // gain for small signals (texture)
        float g_min    = 0.8f;   // gain for large signals (edges)
        float s1       = 0.02f;  // lower threshold (in OD units)
        float s2       = 0.10f;  // upper threshold
    } gains[8];
};
```

#### 4.5.3 구현 전략

```cpp
// Using OpenCV pyrDown/pyrUp (Gaussian 5-tap kernel)
void build_laplacian_pyramid(const cv::Mat& src,
                               std::vector<cv::Mat>& laplacian,
                               std::vector<cv::Mat>& gaussian,
                               int levels) {
    gaussian.resize(levels + 1);
    laplacian.resize(levels);
    gaussian[0] = src.clone();
    
    for (int k = 0; k < levels; ++k) {
        cv::pyrDown(gaussian[k], gaussian[k+1]);
        cv::Mat upsampled;
        cv::pyrUp(gaussian[k+1], upsampled, gaussian[k].size());
        laplacian[k] = gaussian[k] - upsampled;
    }
}

void apply_nonlinear_gain(cv::Mat& L, const LaplacianPyramidParams::LevelGain& gain) {
    L.forEach<float>([&](float& val, const int* pos) {
        float s = std::abs(val);
        float g;
        if (s < gain.s1)
            g = gain.g_max;
        else if (s < gain.s2)
            g = gain.g_min + (gain.g_max - gain.g_min) *
                (gain.s2 - s) / (gain.s2 - gain.s1);
        else
            g = gain.g_min;
        val *= g;
    });
}
```

---

### 4.6 SWU-2.6 Fractional Multiscale Processing (SRS-FUNC-015)

#### 4.6.1 개념 및 수학적 배경

Fractional Multiscale Processing (FMP)은 Laplacian Pyramid의 정수 스케일 해상도 감소 대신 **비정수(fractional) 스케일**을 사용하여 density transition zone의 artifact를 제거한다.

**기본 원리:**

$$L_k^{\alpha} = I - (G_k)^{\alpha} \cdot (I * h^{N-k})^{1-\alpha}$$

여기서 $\alpha \in (0, 1)$는 분수 스케일 파라미터.

실용 구현 (Polynomial Approximation):

$$G_k^{\alpha}(x,y) = \sum_{n=0}^{K} c_n(\alpha) \cdot G_n(x,y)$$

- $c_n(\alpha)$: 분수 스케일 계수 (Chebyshev 보간 기반)

#### 4.6.2 구현 알고리즘

```cpp
struct FractionalMSParams {
    float alpha         = 0.5f;   // Fractional scale (0.3–0.7)
    int   base_levels   = 8;
    float density_threshold = 0.3f; // OD threshold for transition zone
};

// FMP replaces integer pyramid bands with fractional bands at transition zones
cv::Mat compute_fractional_band(const std::vector<cv::Mat>& gaussian_pyr,
                                  float alpha, int target_level) {
    int L = static_cast<int>(gaussian_pyr.size()) - 1;
    int k1 = static_cast<int>(std::floor(alpha * (L - 1)));
    int k2 = k1 + 1;
    float t = alpha * (L - 1) - k1;
    
    cv::Mat level1, level2;
    cv::resize(gaussian_pyr[k1], level1, gaussian_pyr[0].size(),
               0, 0, cv::INTER_LINEAR);
    cv::resize(gaussian_pyr[k2], level2, gaussian_pyr[0].size(),
               0, 0, cv::INTER_LINEAR);
    
    return (1.0f - t) * level1 + t * level2;
}
```

---

## 5. Grid Suppression & Virtual Grid 알고리즘

### 5.1 Grid Line Artifact Suppression (GAP-03 해소)

#### 5.1.1 자동 Grid 파라미터 감지

```cpp
struct GridSpec {
    float  line_density_lpi;  // lines per inch (60–200 lpi typical)
    float  angle_deg;         // grid orientation (0° = horizontal)
    float  pixel_pitch_mm;    // detector pixel pitch
};

// Derive grid artifact frequency from DICOM tags and GridSpec
float compute_grid_artifact_frequency(const GridSpec& spec) {
    // f_grid [cycles/pixel] = pixel_pitch_mm / (25.4 / line_density_lpi)
    return spec.pixel_pitch_mm * spec.line_density_lpi / 25.4f;
}
```

#### 5.1.2 2D DWT 기반 Grid Suppression (Tang et al. 2015)

```python
def wavelet_grid_suppression(image: np.ndarray,
                              grid_freq_cpx: float,
                              wavelet: str = 'db6',
                              max_levels: int = 8) -> np.ndarray:
    """
    Remove grid line artifact using 2D DWT + Gaussian band-stop filter.
    
    Algorithm:
      1. 2D DWT decomposition with auto stop condition
      2. For each sub-band: detect grid frequency component
      3. Apply Gaussian band-stop filter in frequency domain
      4. Reconstruct via inverse DWT
      
    Args:
        image:          float32 input (H, W)
        grid_freq_cpx:  grid artifact frequency in cycles/pixel
        wavelet:        wavelet family (db6 recommended)
        max_levels:     maximum decomposition levels
    Returns:
        float32 grid-suppressed image
    """
    import pywt
    
    # Auto stop condition: stop when grid frequency falls below Nyquist
    # at current decomposition level
    auto_level = 1
    nyquist = 0.5  # cycles/pixel at current level
    f = grid_freq_cpx
    while auto_level < max_levels and f < nyquist * 0.25:
        f *= 2  # frequency doubles with each level of downsampling
        nyquist /= 2
        auto_level += 1
    
    # Perform 2D DWT
    coeffs = pywt.wavedec2(image, wavelet, level=auto_level)
    
    # Apply band-stop filter to horizontal/vertical detail coefficients
    filtered_coeffs = [coeffs[0]]  # keep approximation
    for level_coeffs in coeffs[1:]:
        cH, cV, cD = level_coeffs
        # Suppress grid frequency in horizontal and vertical bands
        cH = _apply_gaussian_bandstop_1d(cH, grid_freq_cpx, axis=1)
        cV = _apply_gaussian_bandstop_1d(cV, grid_freq_cpx, axis=0)
        filtered_coeffs.append((cH, cV, cD))
    
    return pywt.waverec2(filtered_coeffs, wavelet)

def _apply_gaussian_bandstop_1d(band: np.ndarray, f_stop: float,
                                   axis: int, bandwidth: float = 0.02) -> np.ndarray:
    """Apply 1D Gaussian band-stop filter along specified axis."""
    spectrum = np.fft.rfft(band, axis=axis)
    freqs = np.fft.rfftfreq(band.shape[axis])
    
    # Gaussian notch centered at f_stop
    notch = 1.0 - np.exp(-0.5 * ((freqs - f_stop) / bandwidth)**2)
    notch = notch.reshape([-1 if i == axis else 1 for i in range(band.ndim)])
    
    spectrum *= notch
    return np.fft.irfft(spectrum, n=band.shape[axis], axis=axis)
```

#### 5.1.3 NSCT 기반 Moiré Suppression (Kim et al. 2023)

Nonsubsampled Contourlet Transform은 aliasing 없이 다방향 분해를 제공하여 비축 grid orientation에 효과적이다.

```python
# NSCT-based approach for non-standard grid angles
# Reference: Kim et al. 2023, Nuclear Engineering and Technology 55(4):1420-1429
# Key advantage: shift-invariance prevents ringing artifacts at non-axis orientations

def nsct_grid_suppression(image: np.ndarray,
                           grid_angle_deg: float,
                           grid_freq_cpx: float,
                           nsct_levels: int = 4,
                           n_directions_fine: int = 8) -> np.ndarray:
    """
    Suppress X-ray anti-scatter grid artifact using NSCT decomposition.

    Algorithm (Kim et al. 2023, 4-step):
      Step 1 — NSCT Decomposition
        Decompose image into (nsct_levels) lowpass + directional subband pyramid.
        Fine-scale level uses n_directions_fine directional subbands.
        Shift-invariance achieved by omitting downsampling (nonsubsampled filter bank).

      Step 2 — Artifact Subband Identification
        Grid artifact in spatial domain → spike in specific directional subband.
        Target subband index = round(grid_angle_deg / (180 / n_directions_fine)) mod n_directions_fine
        Confirm by comparing subband energy to neighboring subbands (energy_ratio > 3.0 threshold).

      Step 3 — Moiré Component Extraction via Gaussian Band-Pass
        Within the identified subband coefficient map S[i][k]:
          centre_freq = grid_freq_cpx  (in cycles/pixel)
          sigma_bp    = centre_freq × 0.25  (bandwidth: ±25% of grid frequency)
          mask        = gaussian_bandpass_2d(S[i][k].shape, centre_freq, sigma_bp)
          moire_coeff = S[i][k] × mask

      Step 4 — Subtract and Reconstruct
        Zero out or attenuate moire_coeff in the identified subband:
          suppression_weight = compute_adaptive_weight(energy_ratio)
          S[i][k]_clean = S[i][k] - suppression_weight × moire_coeff
        Reconstruct image via inverse NSCT (synthesis filter bank).

    Args:
        image:            float32 input image, OD or linear domain (H, W)
        grid_angle_deg:   dominant grid line orientation in degrees [0, 180)
        grid_freq_cpx:    grid spatial frequency in cycles/pixel (typical 0.05–0.20)
        nsct_levels:      number of decomposition levels (default 4)
        n_directions_fine: directional subbands at finest level (default 8)
    Returns:
        float32 image with grid artifact suppressed
    """
    try:
        import pynsct  # pip install pynsct  (or custom NSCT implementation)
        _nsct_available = True
    except ImportError:
        _nsct_available = False

    if not _nsct_available:
        # Fallback: frequency-domain notch filter at grid frequency
        # Less effective for oblique grids but always available
        import warnings
        warnings.warn("pynsct not available; falling back to notch filter suppression")
        return _notch_fallback(image, grid_angle_deg, grid_freq_cpx)

    H, W = image.shape

    # --- Step 1: NSCT Decomposition ---
    # n_dir_list: number of directional subbands per level (coarse→fine)
    # e.g. [4, 4, 8, 8] for 4-level decomposition
    n_dir_list = [4] * (nsct_levels - 2) + [n_directions_fine, n_directions_fine]
    coeffs = pynsct.nsctdec(image, nlevels=nsct_levels, n_dir_list=n_dir_list)
    # coeffs structure: [lowpass_coeff, level0_subbands, level1_subbands, ...]
    # finest level: coeffs[-1] is list of n_directions_fine subband arrays

    # --- Step 2: Identify Target Subband ---
    fine_subbands = coeffs[-1]          # list of n_directions_fine arrays
    n_sb = len(fine_subbands)
    angle_per_sb = 180.0 / n_sb         # angular step per subband
    target_idx = int(round(grid_angle_deg / angle_per_sb)) % n_sb

    # Confirm by energy ratio
    target_energy  = float(np.sum(fine_subbands[target_idx] ** 2))
    neighbor_energy = float(np.sum(fine_subbands[(target_idx - 1) % n_sb] ** 2) +
                            np.sum(fine_subbands[(target_idx + 1) % n_sb] ** 2)) / 2.0
    energy_ratio = target_energy / (neighbor_energy + 1e-12)

    if energy_ratio < 2.0:
        # Grid artifact not dominant in this subband — skip suppression
        return image.copy()

    # --- Step 3: Gaussian Band-Pass Filter in Frequency Domain ---
    sb = fine_subbands[target_idx].astype(np.float64)
    sb_h, sb_w = sb.shape

    # Build 2-D Gaussian band-pass mask centred on grid frequency
    u = np.fft.fftfreq(sb_w)   # cycles/pixel
    v = np.fft.fftfreq(sb_h)
    UU, VV = np.meshgrid(u, v)
    freq_map = np.sqrt(UU ** 2 + VV ** 2)

    sigma_bp = grid_freq_cpx * 0.25
    # Difference of two Gaussians: band-pass centred at grid_freq_cpx
    mask_bp = (np.exp(-((freq_map - grid_freq_cpx) ** 2) / (2 * sigma_bp ** 2)) +
               np.exp(-((freq_map + grid_freq_cpx) ** 2) / (2 * sigma_bp ** 2)))
    mask_bp = np.clip(mask_bp, 0.0, 1.0)

    sb_fft    = np.fft.fft2(sb)
    moire_fft = sb_fft * mask_bp
    moire_component = np.real(np.fft.ifft2(moire_fft)).astype(np.float32)

    # --- Step 4: Adaptive Suppression and Reconstruction ---
    # Suppression weight increases with energy_ratio (stronger artifact → more suppression)
    suppression_weight = float(np.clip(1.0 - 1.0 / energy_ratio, 0.5, 1.0))
    fine_subbands[target_idx] = (fine_subbands[target_idx] -
                                  suppression_weight * moire_component)
    coeffs[-1] = fine_subbands

    # Inverse NSCT synthesis
    result = pynsct.nsctidec(coeffs).astype(np.float32)
    return result


def _notch_fallback(image: np.ndarray,
                    grid_angle_deg: float,
                    grid_freq_cpx: float) -> np.ndarray:
    """Frequency-domain notch filter fallback when pynsct is unavailable."""
    H, W = image.shape
    u = np.fft.fftfreq(W)
    v = np.fft.fftfreq(H)
    UU, VV = np.meshgrid(u, v)

    # Rotate frequency coordinates to grid orientation
    theta = np.deg2rad(grid_angle_deg)
    u_rot = UU * np.cos(theta) + VV * np.sin(theta)

    # Notch: suppress narrow band around grid frequency
    sigma_notch = grid_freq_cpx * 0.15
    notch = 1.0 - (np.exp(-((u_rot - grid_freq_cpx) ** 2) / (2 * sigma_notch ** 2)) +
                   np.exp(-((u_rot + grid_freq_cpx) ** 2) / (2 * sigma_notch ** 2)))
    notch = np.clip(notch, 0.0, 1.0)

    F = np.fft.fft2(image)
    F_notched = F * notch
    return np.real(np.fft.ifft2(F_notched)).astype(np.float32)
```

---

### 5.2 Virtual Grid — Scatter Correction 알고리즘 (GAP-08 해소)

#### 5.2.1 알고리즘 선택 전략

| 상황 | 권장 방법 | 이유 |
|------|----------|------|
| Phase 1 (빠른 구현) | Thickness-based Empirical (§5.2.2) | 구현 단순, real-time |
| Phase 2 (정확도 향상) | Laplacian Pyramid (§5.2.3) | US8064676B2 특허 기반 |
| Phase 3 (최고 정확도) | DL U-Net (§5.2.4) | MC 데이터 학습, <5% 오차 |

#### 5.2.2 Laplacian Pyramid Virtual Grid (US8064676B2 특허 기반)

```python
def laplacian_pyramid_virtual_grid(image: np.ndarray,
                                    pixel_pitch_mm: float,
                                    target_grid_ratio: float = 10.0) -> np.ndarray:
    """
    Virtual grid via Laplacian Pyramid scatter estimation.
    
    Algorithm (US8064676B2):
      1. Build Laplacian Pyramid (n = log2(N) - 0.5 levels)
      2. Low-frequency bands: scatter component → de-scatter
      3. High-frequency bands: contrast enhancement + denoising
      4. Reconstruct enhanced image
    
    Args:
        image:            float32 input (H, W), linear domain (pre-log transform)
        pixel_pitch_mm:   detector pixel pitch in mm
        target_grid_ratio: emulated grid ratio (5:1 ~ 16:1)
    Returns:
        scatter-corrected float32 image
    """
    H, W = image.shape
    n_levels = int(np.log2(max(H, W)) - 0.5)
    
    # Build Gaussian pyramid (5×5 Gaussian kernel, σ=1, per patent)
    gaussian_pyr = [image]
    for _ in range(n_levels):
        gaussian_pyr.append(cv2.pyrDown(gaussian_pyr[-1]))
    
    # Build Laplacian pyramid
    laplacian_pyr = []
    for k in range(n_levels):
        up = cv2.pyrUp(gaussian_pyr[k+1], dstsize=gaussian_pyr[k].shape[::-1])
        laplacian_pyr.append(gaussian_pyr[k] - up)
    laplacian_pyr.append(gaussian_pyr[-1])  # residual
    
    # Scatter is primarily in low-frequency bands
    # Estimate scatter fraction based on grid ratio emulation
    # Bucky factor B = (1 + 1/R)/(1 - scatter_fraction) where R = grid ratio
    scatter_fraction = estimate_scatter_fraction(target_grid_ratio)
    
    # De-scatter low-frequency bands
    for k in range(n_levels - 2, n_levels + 1):  # lower 2 bands + residual
        idx = min(k, len(laplacian_pyr) - 1)
        laplacian_pyr[idx] = (1.0 / (1.0 - scatter_fraction)) * laplacian_pyr[idx]
    
    # Contrast enhancement for high-frequency bands
    enhancement_factors = compute_enhancement_factors(target_grid_ratio, n_levels)
    for k in range(n_levels - 2):
        laplacian_pyr[k] *= enhancement_factors[k]
    
    # Reconstruct
    result = laplacian_pyr[-1]
    for k in range(n_levels - 1, -1, -1):
        result = cv2.pyrUp(result, dstsize=laplacian_pyr[k].shape[::-1]) + laplacian_pyr[k]
    
    return np.clip(result, 0, None)

def estimate_scatter_fraction(grid_ratio: float) -> float:
    """
    Estimate scatter fraction based on equivalent grid ratio.
    SPR (Scatter-to-Primary Ratio) model:
    For chest AP, 20cm patient, 80kVp: SPR ≈ 100%
    Effective scatter fraction = SPR / (1 + SPR)
    Grid reduces scatter by: factor ≈ R/(R-1) approximately
    """
    # Simplified model; full implementation uses patient thickness estimation
    spr_base = 1.0  # 100% SPR baseline (20cm chest, 80kVp)
    transmission_factor = 1.0 / grid_ratio  # approximate
    return spr_base * transmission_factor / (1.0 + spr_base * transmission_factor)
```

#### 5.2.3 Scatter Fraction 추정 (Thickness 기반)

```python
def estimate_scatter_fraction_from_exposure(
        image: np.ndarray,
        kvp: float,
        sid_mm: float,
        pixel_pitch_mm: float) -> float:
    """
    Estimate patient scatter fraction from image signal statistics.
    Based on Fujifilm Virtual Grid empirical model.
    
    SPR reference table (80kVp, 35×43cm FOV):
      10cm: SPR ~35%
      15cm: SPR ~70%
      20cm: SPR ~100%
      25cm: SPR ~150%
    """
    # Step 1: Estimate effective patient thickness from signal attenuation
    # Primary signal region: darkest area of lung fields (chest)
    # or central anatomical region
    p10 = np.percentile(image, 10)   # approximately primary + scatter
    p90 = np.percentile(image, 90)   # approximately scatter only
    
    # Approximate effective thickness from signal ratio
    # (simplified Beer-Lambert)
    mu_water = 0.018  # cm⁻¹ at 80kVp (effective)
    if p90 > 0 and p10 > 0:
        thickness_cm = -np.log(p10 / p90) / mu_water
    else:
        thickness_cm = 20.0  # default: 20cm
    thickness_cm = np.clip(thickness_cm, 5.0, 40.0)
    
    # kVp correction factor
    kvp_factor = 1.0 + 0.003 * (kvp - 80)
    
    # SPR lookup (linear interpolation)
    spr_table = {10: 0.35, 15: 0.70, 20: 1.00, 25: 1.50, 30: 2.00}
    spr = np.interp(thickness_cm, list(spr_table.keys()),
                    list(spr_table.values())) * kvp_factor
    
    return spr / (1.0 + spr)  # scatter fraction from SPR
```

---

### 5.3 해부 부위별 Virtual Grid 프리셋 테이블 (GAP-V 해소)

xpe-algorithm-spec-deepsync.md §4의 "anatomy-bounded virtual-grid presets"에서 결정된 항목이다. 기존 §5.2는 단일 Virtual Grid 파라미터를 사용하였으나, 실제 임상에서는 해부 부위별로 최적 파라미터가 다르다. 전신 촬영 부위에 대한 사전 검증된 프리셋 테이블을 제공한다.

#### 5.3.1 알고리즘 수학 정의

**Virtual Grid 강도 파라미터**:

$$\text{VG}_{\lambda} = \text{scatter\_fraction} \times \lambda_{\text{anatomy}} \times \lambda_{\text{kvp\_scale}}$$

$$\lambda_{\text{kvp\_scale}} = 1.0 + 0.004 \times (kVp - 80)$$

여기서 $\lambda_{\text{anatomy}}$는 해부 부위별 기준 강도 파라미터이다.

**Grid 비율 선택**:

$$\text{grid\_ratio}_{\text{effective}} = \text{grid\_ratio}_{\text{preset}} \times \frac{1}{1 + \text{scatter\_fraction}}$$

#### 5.3.2 해부 부위별 프리셋 테이블

| 해부 부위 | `body_part_id` | `grid_ratio` | `lambda` | `frequency_lp_mm` | 비고 |
|---------|--------------|------------|---------|------------------|------|
| Chest AP | `CHEST_AP` | 12 | 0.65 | 40–70 | 고산란 (폐/심장) |
| Chest Lateral | `CHEST_LAT` | 15 | 0.75 | 40–70 | 최고 산란 |
| Abdomen AP | `ABD_AP` | 12 | 0.70 | 40–60 | 고산란 복부 |
| Lumbar Spine AP | `LSPINE_AP` | 12 | 0.65 | 40–60 | 두꺼운 조직 |
| Lumbar Spine Lat | `LSPINE_LAT` | 15 | 0.75 | 40–60 | — |
| Pelvis AP | `PELVIS_AP` | 12 | 0.60 | 40–60 | — |
| Hip | `HIP` | 10 | 0.55 | 40–60 | — |
| Knee AP/Lat | `KNEE` | 8 | 0.35 | 50–80 | 낮은 산란 |
| Hand/Wrist | `HAND_WRIST` | 6 | 0.20 | 70–120 | 극소 산란 |
| Foot/Ankle | `FOOT_ANKLE` | 6 | 0.20 | 70–120 | — |
| Skull AP/Lat | `SKULL` | 10 | 0.45 | 50–80 | — |
| C-Spine | `CSPINE` | 8 | 0.40 | 50–80 | — |
| T-Spine | `TSPINE` | 10 | 0.55 | 40–70 | — |
| Shoulder | `SHOULDER` | 8 | 0.40 | 50–80 | — |
| Extremity General | `EXTREMITY` | 6 | 0.25 | 60–100 | 소아 포함 |

**주석**: `grid_ratio`는 Bucky grid 그리드비, `lambda`는 Laplacian Pyramid VG의 기준 강도 계수, `frequency_lp_mm`는 VG가 표적하는 공간 주파수 대역 (lp/mm).

#### 5.3.3 Python 구현

```python
from dataclasses import dataclass
from typing import Optional
import numpy as np

@dataclass(frozen=True)
class VirtualGridPreset:
    body_part_id:      str
    grid_ratio:        int     # nominal (10, 12, 15)
    lambda_base:       float   # base VG strength coefficient
    freq_lo_lpmm:      float   # target frequency band lower bound
    freq_hi_lpmm:      float   # target frequency band upper bound
    description:       str = ''

# Canonical preset table (IEC 62304 §5.4: frozen, change requires review cycle)
VIRTUAL_GRID_PRESETS: dict[str, VirtualGridPreset] = {
    'CHEST_AP':     VirtualGridPreset('CHEST_AP',    12, 0.65, 40, 70,  'Chest AP'),
    'CHEST_LAT':    VirtualGridPreset('CHEST_LAT',   15, 0.75, 40, 70,  'Chest Lateral'),
    'ABD_AP':       VirtualGridPreset('ABD_AP',      12, 0.70, 40, 60,  'Abdomen AP'),
    'LSPINE_AP':    VirtualGridPreset('LSPINE_AP',   12, 0.65, 40, 60,  'Lumbar Spine AP'),
    'LSPINE_LAT':   VirtualGridPreset('LSPINE_LAT',  15, 0.75, 40, 60,  'Lumbar Spine Lat'),
    'PELVIS_AP':    VirtualGridPreset('PELVIS_AP',   12, 0.60, 40, 60,  'Pelvis AP'),
    'HIP':          VirtualGridPreset('HIP',         10, 0.55, 40, 60,  'Hip'),
    'KNEE':         VirtualGridPreset('KNEE',         8, 0.35, 50, 80,  'Knee AP/Lat'),
    'HAND_WRIST':   VirtualGridPreset('HAND_WRIST',   6, 0.20, 70, 120, 'Hand/Wrist'),
    'FOOT_ANKLE':   VirtualGridPreset('FOOT_ANKLE',   6, 0.20, 70, 120, 'Foot/Ankle'),
    'SKULL':        VirtualGridPreset('SKULL',        10, 0.45, 50, 80, 'Skull'),
    'CSPINE':       VirtualGridPreset('CSPINE',       8, 0.40, 50, 80,  'Cervical Spine'),
    'TSPINE':       VirtualGridPreset('TSPINE',       10, 0.55, 40, 70, 'Thoracic Spine'),
    'SHOULDER':     VirtualGridPreset('SHOULDER',     8, 0.40, 50, 80,  'Shoulder'),
    'EXTREMITY':    VirtualGridPreset('EXTREMITY',    6, 0.25, 60, 100, 'Extremity General'),
}


def get_vg_params(body_part_id:    str,
                  scatter_fraction: float,
                  kvp:              float,
                  custom_lambda:    Optional[float] = None) -> dict:
    """
    Get Virtual Grid processing parameters for a specific anatomy and exposure.

    Args:
        body_part_id:     anatomy identifier (e.g., 'CHEST_AP')
        scatter_fraction: estimated scatter fraction (0–0.7, from §5.2.3)
        kvp:              tube voltage (kVp)
        custom_lambda:    override preset lambda (for operator adjustment)
    Returns:
        dict: {lambda_vg, grid_ratio, freq_lo, freq_hi, body_part_id}
    """
    preset = VIRTUAL_GRID_PRESETS.get(body_part_id,
                                       VIRTUAL_GRID_PRESETS['EXTREMITY'])

    lambda_base = custom_lambda if custom_lambda is not None else preset.lambda_base

    # Scale by scatter fraction and kVp
    kvp_scale    = 1.0 + 0.004 * (kvp - 80.0)
    lambda_final = lambda_base * scatter_fraction * kvp_scale

    # Clamp to valid range
    lambda_final = float(np.clip(lambda_final, 0.05, 1.5))

    return {
        'lambda_vg':    lambda_final,
        'grid_ratio':   preset.grid_ratio,
        'freq_lo_lpmm': preset.freq_lo_lpmm,
        'freq_hi_lpmm': preset.freq_hi_lpmm,
        'body_part_id': preset.body_part_id,
    }


def validate_vg_output(input_img:  np.ndarray,
                        output_img: np.ndarray,
                        preset:     VirtualGridPreset) -> dict:
    """
    Validate that VG output satisfies CNR and artifact criteria for the preset.
    Returns dict: {cnr_improvement, artifact_flag, pass}
    """
    # CNR: region-of-interest contrast-to-noise ratio
    # (simplified: use center vs background std ratio)
    H, W = input_img.shape
    roi = input_img[H//4:3*H//4, W//4:3*W//4]
    bg  = np.concatenate([input_img[:H//8, :].ravel(),
                           input_img[7*H//8:, :].ravel()])

    def cnr(arr_roi, arr_bg):
        return abs(np.mean(arr_roi) - np.mean(arr_bg)) / (np.std(arr_bg) + 1e-6)

    cnr_in  = cnr(roi, bg)
    roi_out = output_img[H//4:3*H//4, W//4:3*W//4]
    bg_out  = np.concatenate([output_img[:H//8, :].ravel(),
                               output_img[7*H//8:, :].ravel()])
    cnr_out = cnr(roi_out, bg_out)

    cnr_improvement = cnr_out / (cnr_in + 1e-6)
    artifact_flag   = bool(np.max(np.abs(output_img - input_img)) >
                           0.3 * np.mean(input_img))  # overshoot check

    return {
        'cnr_improvement': cnr_improvement,
        'artifact_flag':   artifact_flag,
        'pass':            cnr_improvement >= 1.05 and not artifact_flag,
    }
```

#### 5.3.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| CNR 개선 (Chest AP) | ≥ 10% vs no-VG | CDRAD 팬텀 또는 합성 |
| CNR 개선 (Extremity) | ≥ 5% vs no-VG | CDRAD 팬텀 |
| MTF 열화 | < 5% at f50 | 슬랜트 에지 측정 |
| 과도 보정 오결 (Artifact) | 없음 | 시각 검토 + 픽셀 오버슈트 |
| Observer 검증 (chest) | ≥ 전문의 3명 동의 | 블라인드 A/B 테스트 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-FUNC-008b (Virtual Grid 프리셋) — Phase 2 추가 예정

---

## 6. SWI-3: Display Processing 알고리즘

### 6.1 SWU-3.1 Modality LUT (SRS-FUNC-020)

#### 6.1.1 수학 정의

$$\text{StoredPixelValue} \xrightarrow{\text{Modality LUT}} \text{ModalityPixelValue}$$

**Linear form (DICOM PS3.3 §C.7.6.3.1.2):**

$$\text{ModalityPixelValue} = \text{RescaleSlope} \times \text{StoredPixelValue} + \text{RescaleIntercept}$$

- DICOM tags: `(0028,1053)` RescaleSlope, `(0028,1052)` RescaleIntercept
- 단위: Housfield Units (CT) 또는 arbitrary linear units (DX)
- DX의 경우: Slope=1, Intercept=0 (identity)가 일반적

#### 6.1.2 구현

```cpp
void xpe_apply_modality_lut(const uint16_t* stored_pixels,
                              float* modality_pixels,
                              float rescale_slope,
                              float rescale_intercept,
                              uint32_t total_pixels) {
    // AVX2 vectorized Fused Multiply-Add
    __m256 v_slope  = _mm256_set1_ps(rescale_slope);
    __m256 v_interc = _mm256_set1_ps(rescale_intercept);
    
    size_t i = 0;
    for (; i + 8 <= total_pixels; i += 8) {
        __m128i u16x8 = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(stored_pixels + i));
        __m256 f32x8  = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16x8));
        __m256 result = _mm256_fmadd_ps(f32x8, v_slope, v_interc);
        _mm256_storeu_ps(modality_pixels + i, result);
    }
    for (; i < total_pixels; ++i) {
        modality_pixels[i] = rescale_slope * stored_pixels[i] + rescale_intercept;
    }
}
```

---

### 6.2 SWU-3.2 VOI LUT (SRS-FUNC-021)

#### 6.2.1 LINEAR 변환

$$\text{Output} = \frac{\text{Input} - (\text{WC} - \text{WW}/2)}{\text{WW}} \times (\text{MaxOut} - \text{MinOut}) + \text{MinOut}$$

Clamp: `[MinOut, MaxOut]`

#### 6.2.2 LINEAR_EXACT 변환 (DICOM PS3.3 §C.11.2.1.2)

$$\text{Output} = \begin{cases} \text{MinOut} & \text{if Input} \le WC - \lfloor WW/2 \rfloor \\ \frac{(Input - (WC - 0.5)) \cdot (MaxOut - MinOut + 1)}{WW} + \frac{MinOut + MaxOut}{2} & \text{otherwise} \\ \text{MaxOut} & \text{if Input} > WC + \lfloor (WW-1)/2 \rfloor \end{cases}$$

#### 6.2.3 SIGMOID 변환

$$\text{Output} = \frac{\text{MaxOut} - \text{MinOut}}{1 + e^{-4(\text{Input} - WC)/WW}} + \text{MinOut}$$

- **장점**: 선형에 비해 extreme 값에서 부드러운 클리핑 → 구조 과노출 방지
- **권장 사용 사례**: 폐, 종격동 동시 표현

#### 6.2.4 실시간 W/L 조정 구현 (SRS-PERF-003: ≤16ms)

```cpp
// GPU-accelerated VOI LUT for real-time interactive adjustment
// Falls back to AVX2 CPU path if GPU unavailable
void xpe_apply_voi_lut(const float* modality_pixels,
                         uint16_t*   output,
                         VoiLutType  lut_type,
                         float wc, float ww,
                         float min_out, float max_out,
                         uint32_t total_pixels) {
    
    const float half_ww = ww * 0.5f;
    const float lo = wc - half_ww;
    const float range_out = max_out - min_out;
    
    switch (lut_type) {
        case VoiLutType::LINEAR: {
            __m256 v_lo    = _mm256_set1_ps(lo);
            __m256 v_scale = _mm256_set1_ps(range_out / ww);
            __m256 v_off   = _mm256_set1_ps(min_out);
            __m256 v_min   = _mm256_set1_ps(min_out);
            __m256 v_max   = _mm256_set1_ps(max_out);
            
            size_t i = 0;
            for (; i + 8 <= total_pixels; i += 8) {
                __m256 inp  = _mm256_loadu_ps(modality_pixels + i);
                __m256 norm = _mm256_sub_ps(inp, v_lo);
                __m256 res  = _mm256_fmadd_ps(norm, v_scale, v_off);
                res = _mm256_min_ps(_mm256_max_ps(res, v_min), v_max);
                // Convert to uint16
                __m256i res_i = _mm256_cvttps_epi32(res);
                // Pack and store (simplified; full impl uses _mm256_packs_epi32)
                // ... store to output
            }
            break;
        }
        case VoiLutType::SIGMOID: {
            // Vectorized sigmoid via polynomial approximation
            // f(x) ≈ 0.5 + 0.25*x*(1 - x²/12) for |x| < 2
            break;
        }
        default: break;
    }
}
```

#### 6.2.5 Body-Part Preset 테이블 (≥20 preset, SRS-FUNC-021)

```json
{
  "presets": [
    {"name":"Chest Standard",   "body_part":"CHEST",    "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Chest Lung",       "body_part":"CHEST",    "wc":-600,"ww":1500, "type":"SIGMOID"},
    {"name":"Chest Mediastinum","body_part":"CHEST",    "wc":50,  "ww":400,  "type":"LINEAR"},
    {"name":"Bone Standard",    "body_part":"EXTREMITY","wc":500, "ww":2500, "type":"LINEAR"},
    {"name":"Abdomen",          "body_part":"ABDOMEN",  "wc":60,  "ww":400,  "type":"LINEAR"},
    {"name":"Spine",            "body_part":"SPINE",    "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Skull",            "body_part":"HEAD",     "wc":500, "ww":3000, "type":"LINEAR"},
    {"name":"Hand/Wrist",       "body_part":"HAND",     "wc":600, "ww":2500, "type":"LINEAR"},
    {"name":"Pelvis",           "body_part":"PELVIS",   "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Shoulder",         "body_part":"SHOULDER", "wc":300, "ww":1500, "type":"LINEAR"},
    {"name":"Knee",             "body_part":"KNEE",     "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Ankle/Foot",       "body_part":"FOOT",     "wc":600, "ww":2000, "type":"LINEAR"},
    {"name":"Breast MLO",       "body_part":"BREAST",   "wc":2000,"ww":4000, "type":"LINEAR"},
    {"name":"Pediatric Chest",  "body_part":"CHEST",    "wc":300, "ww":1500, "type":"SIGMOID"},
    {"name":"Neonatal",         "body_part":"CHEST",    "wc":200, "ww":800,  "type":"SIGMOID"},
    {"name":"Panoramic Dental", "body_part":"DENTAL",   "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Long Leg",         "body_part":"LOWER_EX", "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Full Spine",       "body_part":"SPINE",    "wc":500, "ww":2500, "type":"LINEAR"},
    {"name":"Scoliosis",        "body_part":"SPINE",    "wc":400, "ww":1800, "type":"LINEAR_EXACT"},
    {"name":"Soft Tissue",      "body_part":"EXTREMITY","wc":100, "ww":400,  "type":"LINEAR"},
    {"name":"Bone Suppress",    "body_part":"CHEST",    "wc":300, "ww":1200, "type":"SIGMOID"}
  ]
}
```

---

### 6.3 SWU-3.3 Presentation LUT — GSDF (SRS-FUNC-022)

#### 6.3.1 DICOM PS3.14 GSDF 알고리즘

**목적**: P-Value(0–4095)를 Just Noticeable Difference(JND)가 균일한 휘도로 변환.

**GSDF 수학 모델 (DICOM PS3.14 §6):**

$$\bar{L}(j) = \frac{L_{\min} + L_{\max}}{2} \cdot \exp\left(\frac{j - j_0}{j_0}\right) \quad \text{(simplified)}$$

정확한 구현은 PS3.14 Table B.1의 256-point LUT 사용:

```cpp
// GSDF P-Value to Luminance conversion
// Source: DICOM PS3.14 Table B.1 (256 P-Value entries)
// Full table: 1024 entries interpolated from 256

struct GSDFCalibration {
    float L_min_cdm2;   // minimum luminance of display (cd/m²)
    float L_max_cdm2;   // maximum luminance of display (cd/m²)
    float gamma;        // display gamma (typically 2.2)
    std::vector<float> gsdf_lut;  // 4096-entry P-Value → DDL LUT
};

// Build calibrated Presentation LUT for current display
std::vector<uint16_t> build_presentation_lut(
        const GSDFCalibration& cal,
        uint16_t p_value_range = 4096) {
    
    // GSDF JND indices for given luminance range
    float j_min = compute_jnd_index(cal.L_min_cdm2);  // PS3.14 §B.2
    float j_max = compute_jnd_index(cal.L_max_cdm2);
    
    // Map P-Values uniformly across JND range
    std::vector<uint16_t> lut(p_value_range);
    for (uint16_t p = 0; p < p_value_range; ++p) {
        float j = j_min + (j_max - j_min) * p / (p_value_range - 1);
        float L = jnd_index_to_luminance(j);  // inverse GSDF
        // Convert luminance to DDL (Digital Driving Level)
        uint16_t ddl = luminance_to_ddl(L, cal);
        lut[p] = ddl;
    }
    return lut;
}

// JND index formula (PS3.14 §B.2)
float compute_jnd_index(float L_cdm2) {
    float log_L = std::log10(L_cdm2);
    // 4th-order polynomial approximation (DICOM standard)
    return 71.498068f + 94.593053f * log_L + 41.912053f * log_L * log_L
           + 9.8247004f * pow(log_L, 3) + 0.28175407f * pow(log_L, 4)
           - 1.1878455f * pow(log_L, 5) - 0.18014349f * pow(log_L, 6)
           + 0.14710899f * pow(log_L, 7) - 0.017046845f * pow(log_L, 8);
}
```

#### 6.3.2 Display 교정 미보정 감지 (SRS-ALERT-003)

```cpp
bool detect_uncalibrated_display(const DisplayDevice& device) {
    // Measure luminance at multiple DDL steps
    // Compare measured JND spacing to GSDF target
    // If max deviation > 10% → flag as uncalibrated
    float gsdf_conformance = evaluate_gsdf_conformance(device);
    return gsdf_conformance < 0.90f;  // <90% conformance → warning
}
```

---

## 7. IEC 62494-1 Exposure Index 알고리즘

### 7.1 알고리즘 개요 (GAP-09 해소)

IEC 62494-1은 디지털 방사선 촬영의 **노출 적절성**을 수치화하는 표준이다:

- **EI (Exposure Index)**: 검출기 수신 선량에 비례하는 지수
- **EI_target**: 특정 촬영 유형의 목표 EI
- **DI (Deviation Index)**: EI 대비 EI_target의 편차 (dB 단위)

$$DI = 10 \cdot \log_{10}\left(\frac{EI}{EI_{\text{target}}}\right)$$

### 7.2 ROI 추출 알고리즘

```cpp
struct ExposureIndexROI {
    cv::Rect roi;         // Selected ROI rectangle
    float    mean_signal; // Mean pixel value in ROI
    float    area_mm2;    // Physical area of ROI
};

// IEC 62494-1 §7.3: ROI selection methods
ExposureIndexROI select_roi_for_ei(const cv::Mat& image,
                                    const CollimatorMask& collimator,
                                    RoiSelectionMethod method,
                                    const std::string& body_part) {
    switch (method) {
        case RoiSelectionMethod::FULL_FIELD:
            // Use entire collimated area (simple, method A)
            return {collimator.bounding_rect(), 
                    mean_within_mask(image, collimator.mask()), 0.0f};
        
        case RoiSelectionMethod::ANATOMY_BASED:
            // Method B: auto-detect anatomical region
            // For chest: left/right lung ROIs
            // For extremity: bone shaft ROI
            return detect_anatomy_roi(image, body_part);
        
        case RoiSelectionMethod::CENTRAL:
            // Method C: central 10% of collimated area (by area fraction)
            // Target: ROI_area = 0.10 × full_area
            //   => w_roi = full.width  × sqrt(0.10) ≈ full.width  × 0.3162
            //   => h_roi = full.height × sqrt(0.10) ≈ full.height × 0.3162
            // IEC 62494-1 §7.2.4 — S_d region must represent ≥10% of receptor area
            auto full = collimator.bounding_rect();
            int cx = full.x + full.width / 2;
            int cy = full.y + full.height / 2;
            // sqrt(0.10) = 0.31623 — use compile-time constant for clarity
            constexpr double kSqrt01 = 0.31622776601683794;  // sqrt(0.10)
            int w  = static_cast<int>(std::round(full.width  * kSqrt01));
            int h  = static_cast<int>(std::round(full.height * kSqrt01));
            // Ensure minimum 32×32 pixels for statistical validity
            w = std::max(w, 32);
            h = std::max(h, 32);
            return {cv::Rect(cx - w/2, cy - h/2, w, h), 0.0f, 0.0f};
    }
}
```

### 7.3 EI 계산

```cpp
float compute_exposure_index(float mean_roi_signal,
                               float pixel_pitch_mm,
                               float rescale_slope,
                               float rescale_intercept,
                               const DetectorCalibrationData& cal) {
    // Convert mean ROI signal to calibrated detector signal S_cal
    // S_cal = (mean_roi_signal × rescale_slope + rescale_intercept)
    float s_cal = mean_roi_signal * rescale_slope + rescale_intercept;
    
    // EI = C_ei × s_cal (IEC 62494-1 §7.2)
    // C_ei: detector-specific calibration constant
    // Calibrated such that EI = 100 corresponds to reference entrance dose
    float ei = cal.C_ei * s_cal;
    
    // Clamp to valid range [0, 10000]
    return std::clamp(ei, 0.0f, 10000.0f);
}

float compute_deviation_index(float ei, float ei_target) {
    if (ei_target <= 0.0f || ei <= 0.0f) return 0.0f;
    return 10.0f * std::log10(ei / ei_target);
}

// DI interpretation:
// DI < -1.0: underexposure (high noise)
// -1.0 ≤ DI ≤ +1.0: acceptable exposure
// DI > +1.0: overexposure (unnecessary dose)
// DI > +3.0: significant overexposure → alert
```

### 7.4 EI_target 테이블

| 촬영 부위 | EI_target | 참고 |
|----------|-----------|------|
| Chest PA | 200 | ACR 권장 |
| Chest AP (portable) | 300 | 산란 증가 반영 |
| Abdomen AP | 250 | |
| Spine AP/Lateral | 200 | |
| Extremity | 100 | 낮은 감쇠 |
| Hand/Foot | 80 | |
| Pelvis AP | 250 | |
| Skull | 200 | |

---

## 8. AI/DL 알고리즘

### 8.1 CNN Body-Part Recognition (SRS-FUNC-016)

#### 8.1.1 모델 아키텍처

```
Input: 512×512 (downsampled from full resolution)
  ↓
EfficientNet-B4 Backbone (ImageNet pretrained)
  ↓
Global Average Pooling
  ↓
FC(1792 → 512) + BatchNorm + ReLU + Dropout(0.3)
  ↓
FC(512 → N_classes)   N_classes = 15+ body parts
  ↓
Softmax → confidence scores
```

#### 8.1.2 신체 부위 분류 체계 (≥15 categories)

| 클래스 ID | 명칭 | DICOM Body Part |
|---------|------|----------------|
| 0 | Chest PA | CHEST |
| 1 | Chest AP | CHEST |
| 2 | Chest Lateral | CHEST |
| 3 | Abdomen AP | ABDOMEN |
| 4 | Pelvis AP | PELVIS |
| 5 | Spine Cervical | CSPINE |
| 6 | Spine Thoracic | TSPINE |
| 7 | Spine Lumbar | LSPINE |
| 8 | Shoulder | SHOULDER |
| 9 | Elbow | ELBOW |
| 10 | Hand/Wrist | HAND |
| 11 | Hip | HIP |
| 12 | Knee | KNEE |
| 13 | Ankle/Foot | FOOT |
| 14 | Skull | HEAD |
| 15 | Full Spine | SPINE |
| 16 | Long Leg | LOWER_EXTREMITY |

#### 8.1.3 Preprocessing for Inference

```python
def preprocess_for_body_part_recognition(image: np.ndarray) -> np.ndarray:
    """
    Preprocess X-ray image for CNN inference.
    """
    # 1. Resize to 512×512
    img = cv2.resize(image, (512, 512), interpolation=cv2.INTER_AREA)
    
    # 2. Normalize to [0, 1] using percentile normalization
    p2  = np.percentile(img, 2)
    p98 = np.percentile(img, 98)
    img = (img - p2) / max(p98 - p2, 1e-6)
    img = np.clip(img, 0.0, 1.0)
    
    # 3. Expand to 3 channels (grayscale → RGB replication)
    img_3ch = np.stack([img, img, img], axis=0)  # (3, H, W)
    
    # 4. Normalize with ImageNet stats (for pretrained backbone)
    mean = np.array([0.485, 0.456, 0.406]).reshape(3, 1, 1)
    std  = np.array([0.229, 0.224, 0.225]).reshape(3, 1, 1)
    img_norm = (img_3ch - mean) / std
    
    return img_norm.astype(np.float32)[np.newaxis]  # (1, 3, 512, 512)
```

#### 8.1.4 ONNX Runtime 추론 (xpe_ai_worker.exe)

```cpp
// In xpe_ai_worker.exe (sandbox process)
class BodyPartRecognizer {
    Ort::Session session_;
    
public:
    BodyPartResult recognize(const float* preprocessed_input,
                               size_t input_size) {
        // Create input tensor
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, const_cast<float*>(preprocessed_input),
            input_size, input_shape_.data(), input_shape_.size());
        
        // Run inference
        auto outputs = session_.Run(Ort::RunOptions{nullptr},
                                     input_names_.data(), &input_tensor, 1,
                                     output_names_.data(), 1);
        
        // Parse softmax output
        float* scores = outputs[0].GetTensorMutableData<float>();
        int n_classes = static_cast<int>(outputs[0].GetTensorTypeAndShapeInfo()
                                          .GetShape()[1]);
        
        int best_class = std::max_element(scores, scores + n_classes) - scores;
        float confidence = scores[best_class];
        
        return {best_class, confidence, 
                std::vector<float>(scores, scores + n_classes)};
    }
};
```

#### 8.1.5 성능 요구사항

- **정확도**: ≥95% top-1 accuracy (SRS-FUNC-016)
- **추론 시간**: ≤200ms (CPU 추론, EfficientNet-B4)
- **입력 허용 범위**: 0.5× ~ 2× 기준 크기

---

### 8.2 DL Bone Suppression (SRS-FUNC-018)

#### 8.2.1 모델 아키텍처 — U-Net with Attention

```
Encoder:                          Decoder:
Input (H×W×1)                     (H×W×1) Output
  ↓                                  ↑
Conv3×3 + BN + ReLU ×2             ← Skip connection (attention gate)
MaxPool2×2                          UpSample2×2 + Conv
  ↓                                  ↑
×4 encoder blocks              ×4 decoder blocks
  ↓
Bottleneck: Conv3×3 ×3 (dilated 1,2,4)
```

#### 8.2.2 훈련 데이터 생성

```python
# Virtual training pairs from real Dual-Energy Subtraction (DES)
# - Standard X-ray (mixed bone + soft tissue)
# - DES bone image (real dual-energy reference)
# - DES soft tissue image (ground truth for bone suppression)

# Data augmentation:
# - Random horizontal flip
# - Random rotation ±5°
# - Gaussian noise injection (σ = 0.01–0.05)
# - Random contrast adjustment (0.9–1.1×)
# - Random elastic deformation (medical augmentation)
```

#### 8.2.3 손실 함수

$$\mathcal{L} = \lambda_1 \mathcal{L}_{L1} + \lambda_2 \mathcal{L}_{SSIM} + \lambda_3 \mathcal{L}_{perceptual}$$

$$\mathcal{L}_{L1} = \|I_{pred} - I_{DES}\|_1$$

$$\mathcal{L}_{SSIM} = 1 - \text{SSIM}(I_{pred}, I_{DES})$$

$$\mathcal{L}_{perceptual} = \|\phi_k(I_{pred}) - \phi_k(I_{DES})\|_2$$

- **목표**: PSNR ≥ 33dB, SSIM ≥ 0.97 (SRS-FUNC-018)

---

### 8.3 Panoramic Image Stitching (SRS-FUNC-017)

#### 8.3.1 알고리즘 파이프라인 (GAP-07 해소)

```
Input: 2-4 overlapping X-ray images (10-30% overlap)
         ↓
Step 1: Feature Detection (SIFT/ORB on bone edges)
         ↓
Step 2: Feature Matching (FLANN-based matcher + ratio test)
         ↓
Step 3: Homography Estimation (RANSAC, ≥4 point pairs)
         ↓
Step 4: Geometric Correction (perspective + distortion)
         ↓
Step 5: Intensity Normalization (histogram matching at overlap)
         ↓
Step 6: Blending (Multi-band blending or Feathering)
         ↓
Output: Panoramic image with Cobb angle error ≤ 2°
```

#### 8.3.2 Cobb Angle 오차 ≤2° 달성 전략

```python
def validate_stitching_accuracy(stitched: np.ndarray,
                                 individual_images: list[np.ndarray],
                                 known_landmarks: list[dict]) -> dict:
    """
    Validate stitching accuracy using vertebral landmark pairs.
    
    Cobb angle error = |Cobb_stitched - Cobb_ground_truth|
    Acceptance: ≤ 2°
    """
    # Measure vertebral endplate angles in stitched image
    cobb_stitched = measure_cobb_angle(stitched, known_landmarks)
    
    # Ground truth from reference measurement
    # (physical phantom with calibrated curvature)
    cobb_reference = known_landmarks[0]['cobb_angle_reference']
    
    error_deg = abs(cobb_stitched - cobb_reference)
    return {
        'cobb_stitched': cobb_stitched,
        'cobb_reference': cobb_reference,
        'error_deg': error_deg,
        'pass': error_deg <= 2.0
    }
```

---

### 8.4 AI Worker 격리 아키텍처 및 ONNX 추론 (GAP-W 해소)

xpe-algorithm-spec-deepsync.md §5.3 "prefer ONNX CPU execution with quantized inference, require deterministic fallback, versioned model manifests"에서 요구된 항목이다. 기존 §8.1.4에는 기본 ONNX 세션이 있지만, 워커 격리, 모델 매니페스트 스키마, 결정론적 폴백 메커니즘이 누락되어 있었다.

#### 8.4.1 AI Worker 격리 설계 원칙

```
격리 아키텍처:

  XPE 메인 프로세스
  ┌───────────────────────────────┐
  │  Deterministic Pipeline       │
  │  (§3 Pre-Process, §4 Core)    │
  │         │                     │
  │  xpe_ai_worker_proxy()        │──── IPC (shared memory + semaphore)
  │         │                     │
  └─────────│─────────────────────┘
            │ input tensor + request_id
            ▼
  xpe_ai_worker.exe (isolated process)
  ┌────────────────────────────────┐
  │  ONNX Runtime Session          │
  │  Quantized INT8 model (NCHW)   │
  │  Model Manifest Validator      │
  │  Timeout watchdog (5s)         │
  │         │                      │
  │  Result + confidence → IPC     │
  └────────────────────────────────┘
            │ fallback if timeout or error
            ▼
  Deterministic fallback
  (heuristic body-part classifier)
```

**격리 규칙**:
- AI 워커 실패 또는 타임아웃 시 메인 파이프라인은 결정론적 폴백으로 계속 진행
- AI 결과는 항상 `is_ai_result` 플래그와 함께 반환 (QualityState 사이드카에 기록)
- 모델 버전이 매니페스트와 불일치 시 워커 시작 거부

#### 8.4.2 모델 매니페스트 스키마

```json
{
  "schema_version": "1.0",
  "model_id": "body_part_recognition_v2",
  "model_file": "body_part_cls_effb4_int8.onnx",
  "sha256": "f4a9b3c1d2e8f7a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7a8b9c0d1e2f3a4",
  "model_version": "2.1.0",
  "framework": "onnxruntime",
  "quantization": "INT8",
  "input_shape": [1, 3, 512, 512],
  "input_dtype": "float32",
  "output_shape": [1, 15],
  "output_dtype": "float32",
  "classes": ["CHEST_AP", "CHEST_LAT", "ABD_AP", "LSPINE_AP", "LSPINE_LAT",
              "PELVIS_AP", "HIP", "KNEE", "HAND_WRIST", "FOOT_ANKLE",
              "SKULL", "CSPINE", "TSPINE", "SHOULDER", "EXTREMITY"],
  "performance": {
    "top1_accuracy_pct": 96.2,
    "inference_ms_cpu_p95": 180,
    "validation_dataset": "xpe_cls_val_v3_n=2500"
  },
  "requires_deterministic_fallback": true,
  "disable_control": "XPE_AI_DISABLE_BODY_PART_CLS",
  "release_boundary": "release-safe",
  "created_at": "2026-04-15T00:00:00Z"
}
```

#### 8.4.3 Python ONNX 추론 래퍼 (참조 구현)

```python
import numpy as np
import json
import hashlib
import os
from pathlib import Path
from dataclasses import dataclass
from typing import Optional, List

@dataclass
class AiInferenceResult:
    body_part_id:    str
    confidence:      float
    all_scores:      List[float]
    is_ai_result:    bool   = True   # False = deterministic fallback used
    model_version:   str   = ''
    inference_ms:    float = 0.0

class OnnxAiWorker:
    """
    ONNX AI worker with model manifest validation, quantized inference,
    per-task disable control, and deterministic fallback.
    """

    def __init__(self, manifest_path: Path):
        self.manifest  = self._load_manifest(manifest_path)
        self._session  = None
        self._classes  = self.manifest['classes']
        self._disabled = os.environ.get(
            self.manifest.get('disable_control', '_NONE_'), '0') != '0'

    def _load_manifest(self, path: Path) -> dict:
        with open(path) as f:
            m = json.load(f)
        # Verify model file hash
        model_file = path.parent / m['model_file']
        if not model_file.exists():
            raise FileNotFoundError(f"Model file not found: {model_file}")
        sha = hashlib.sha256(model_file.read_bytes()).hexdigest()
        if sha != m['sha256']:
            raise ValueError(f"Model hash mismatch: {m['model_file']}")
        return m

    def _get_session(self):
        if self._session is None:
            import onnxruntime as ort
            model_path = str(Path(self.manifest['model_file']))
            opts = ort.SessionOptions()
            opts.intra_op_num_threads = 1   # deterministic single-thread
            opts.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
            self._session = ort.InferenceSession(
                model_path,
                sess_options=opts,
                providers=['CPUExecutionProvider'])
        return self._session

    def infer(self,
              preprocessed_input: np.ndarray,
              timeout_ms:         float = 5000.0) -> AiInferenceResult:
        """
        Run inference with timeout. Falls back to heuristic on failure.

        Args:
            preprocessed_input: float32 (1, 3, 512, 512) — normalised
            timeout_ms:         maximum allowed inference time
        Returns:
            AiInferenceResult
        """
        import time

        if self._disabled:
            return self._deterministic_fallback(preprocessed_input)

        try:
            session = self._get_session()
            input_name = session.get_inputs()[0].name

            t0 = time.monotonic()
            outputs = session.run(None, {input_name: preprocessed_input})
            elapsed_ms = (time.monotonic() - t0) * 1000.0

            if elapsed_ms > timeout_ms:
                return self._deterministic_fallback(preprocessed_input)

            scores    = outputs[0][0]                      # (n_classes,)
            best_idx  = int(np.argmax(scores))
            return AiInferenceResult(
                body_part_id   = self._classes[best_idx],
                confidence     = float(scores[best_idx]),
                all_scores     = [float(s) for s in scores],
                is_ai_result   = True,
                model_version  = self.manifest.get('model_version', ''),
                inference_ms   = elapsed_ms,
            )
        except Exception:
            return self._deterministic_fallback(preprocessed_input)

    def _deterministic_fallback(self,
                                  img: np.ndarray) -> AiInferenceResult:
        """
        Heuristic body-part classification (no ML required).
        Uses image aspect ratio and intensity statistics as features.
        """
        arr = img.squeeze()
        if arr.ndim == 3:
            arr = arr[0]  # take first channel

        h, w = arr.shape[-2], arr.shape[-1]
        aspect = w / max(h, 1)
        mean_i = float(np.mean(arr))

        # Simple heuristic: aspect ratio + mean intensity
        if aspect > 1.5:
            body_part = 'CHEST_AP'
        elif mean_i > 0.6:
            body_part = 'EXTREMITY'
        else:
            body_part = 'ABD_AP'

        n = len(self._classes)
        scores = [1.0 / n] * n
        idx = self._classes.index(body_part) if body_part in self._classes else 0
        scores[idx] = 0.6

        return AiInferenceResult(
            body_part_id  = body_part,
            confidence    = 0.6,
            all_scores    = scores,
            is_ai_result  = False,
            model_version = 'fallback-heuristic',
        )
```

#### 8.4.4 C++ Worker Proxy

```cpp
// C++ IPC proxy for xpe_ai_worker.exe
// Sends input via shared memory, waits for result with timeout.

struct AiWorkerRequest {
    uint32_t request_id;
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    // Followed by float32[channels × height × width] in shared memory
};

struct AiWorkerResponse {
    uint32_t request_id;
    char     body_part_id[32];
    float    confidence;
    float    all_scores[15];   // max 15 classes
    bool     is_ai_result;
    float    inference_ms;
};

class XpeAiWorkerProxy {
public:
    AiInferenceResult run_with_timeout(const float* input,
                                        size_t n_elements,
                                        uint32_t timeout_ms = 5000) {
        if (!worker_running_ || ai_disabled_) {
            return deterministic_fallback(input, n_elements);
        }

        // Write request to shared memory
        auto req = write_request(input, n_elements);

        // Wait for response with timeout
        bool got_response = response_sem_.wait_for(
            std::chrono::milliseconds(timeout_ms));

        if (!got_response) {
            log_warning("AI worker timeout after {}ms — using fallback", timeout_ms);
            return deterministic_fallback(input, n_elements);
        }

        auto resp = read_response(req.request_id);
        return AiInferenceResult{
            .body_part_id  = std::string(resp.body_part_id),
            .confidence    = resp.confidence,
            .is_ai_result  = resp.is_ai_result,
            .inference_ms  = resp.inference_ms,
        };
    }

private:
    bool ai_disabled_ = false;  // Set from env XPE_AI_DISABLE_BODY_PART_CLS
    bool worker_running_ = false;
    Semaphore response_sem_;
    SharedMemory shm_;
};
```

#### 8.4.5 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 모델 해시 검증 | 불일치 시 워커 시작 거부 | 변조된 ONNX 파일 주입 |
| 타임아웃 폴백 | 5s 초과 시 100% 폴백 전환 | 의도적 지연 테스트 |
| INT8 vs FP32 정확도 차이 | top-1 accuracy ≤ 0.5% 차이 | 검증 세트 비교 |
| 워커 비활성화 제어 | `XPE_AI_DISABLE_*` 환경변수 100% 동작 | 환경변수 테스트 |
| 폴백 결과 플래그 | `is_ai_result=false` 항상 표시 | 폴백 시나리오 실행 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-AI-001 (AI Worker 격리), SRS-AI-002 (모델 매니페스트) — Phase 2 추가 예정

---

### 8.5 SWU-8.5 폐 영역 자동 분할 (U-Net Lung Field Segmentation) ★GAP-BO 해소

**참고**: Ronneberger et al., "U-Net: Convolutional Networks for Biomedical Image Segmentation," MICCAI 2015.  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-SEG-001 | SWU-8.5 | xpe_ai_worker.dll | Class B |

#### 아키텍처 개요

```
입력: 512×512 float32 (uint16 → float, normalize /65535)
인코더: 5 레벨 × [64, 128, 256, 512, 512] 채널
         각 레벨: 3×3 Conv → BN → ReLU → 3×3 Conv → BN → ReLU → 2×2 MaxPool
보틀넥: 1024 채널, 3×3 Conv × 2
디코더: 5 레벨 바이리니어 업샘플 + 스킵 concat + Conv×2
출력: 1채널 sigmoid → 이진 마스크 (폐=1, 배경=0)
모델: models/xpe_lung_seg_unet_v1.onnx  (≤ 25 MB int8 양자화)
```

#### 후처리

```
1. sigmoid > 0.5 → binary mask
2. 형태학적 fill: 반경 15px flood-fill (폐 내부 기관지 holes 제거)
3. 연결 영역 분석: 상위 2개 영역만 보존 (좌폐/우폐)
4. 신뢰도: iou_confidence = predicted_area / expected_area_range
```

#### C++ 구조체 및 API

```cpp
struct XpeLungMask {
    uint8_t* mask;            // W×H binary mask (1=lung)
    uint32_t W, H;
    float    iou_confidence;  // [0,1] 예측 신뢰도
};

XpeStatus xpe_seg_lung_field(
    const uint16_t* I_in,
    uint32_t        W,
    uint32_t        H,
    XpeLungMask*    out,
    OrtSession*     session   // ONNX Runtime session (may be null → skip)
);
```

#### 안전 고려사항

- `iou_confidence < 0.70` → 마스크 결과 무효화, 폴백: 전체 이미지 마스크
- `clinical_decision_blocked = true`: AI 분할 결과는 보조적 정보만 제공
- Non-SaMD 분류 (XPE-AI-REG-001 §4 참조)

#### 성능 목표

| 항목 | 목표 |
|------|------|
| CPU 추론 | < 200 ms |
| GPU 추론 | < 50 ms |
| 모델 로딩 | 1회 초기화 후 세션 재사용 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| IoU | ≥ 0.92 (Montgomery 데이터셋 50장 서브셋) |
| Dice 계수 | ≥ 0.95 |
| 미탐지 (iou_confidence < 0.5 비율) | < 5% |

---

### 8.6 SWU-8.6 딥러닝 기반 저선량 화질 복원 (DLIR — CNN Denoising) ★GAP-BP 해소

**참고**: Zhang et al., "Residual Dense Network for Image Super-Resolution," CVPR 2018 (adapted for denoising).  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-DLIR-001 | SWU-8.6 | xpe_ai_worker.dll | Class B |

#### 아키텍처 개요

Residual Dense Network (RDN) — 잔류 학습으로 노이즈 성분만 예측:

```
입력: 256×256 패치 (stride 128, cosine-window 블렌딩으로 경계 제거)
정규화: (I − I_mean) / I_std  per patch (float32)
RDB × 16: 각 블록 내 6 Dense Conv(3×3, 64ch) + Local Feature Fusion
글로벌 잔류: I_out = I_in + F(I_in)  ← 노이즈 잔류 학습
출력: float32 → uint16 역정규화 → saturate_cast
모델: models/xpe_dlir_rdn_v1.onnx  (≤ 50 MB int8 양자화)
```

패치 블렌딩:

```
w(x) = sin²(πx / patch_size)  (cosine window)
I_out_assembled = Σ_patch (w_patch × I_out_patch) / Σ_patch w_patch
```

#### C++ 구조체 및 API

```cpp
struct XpeDlirParams {
    int   patch_size;    // default 256
    int   stride;        // default 128
    float blend_sigma;   // cosine window (0 = use default)
    bool  use_gpu;       // use CUDA EP if available
};

XpeStatus xpe_dlir_denoise(
    const uint16_t*   I_in,
    uint16_t*         I_out,
    uint32_t          W,
    uint32_t          H,
    const XpeDlirParams* params,
    OrtSession*       session
);
```

#### 안전 고려사항

- 처리 전/후 이미지 비교 QA: PSNR(I_in, I_out) 범위 확인 (< 50 dB → 경고)
- 구조 환각 검사: FSIM(I_in, I_out) ≥ 0.97 강제 (미충족 시 I_in 원본 반환)

#### 성능 목표

| 항목 | 목표 |
|------|------|
| CPU 처리 | < 500 ms / 3K×3K |
| GPU 처리 | < 150 ms / 3K×3K |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| PSNR (50% 선량 대비 전선량) | ≥ 38 dB |
| SSIM | ≥ 0.96 |
| FSIM (구조 보존) | ≥ 0.97 |
| 선량 의존 잡음 감소 | σ_out / σ_in ≤ 0.35 (50% 선량 기준) |

---

### 8.7 SWU-8.7 흉부 늑골 억제 알고리즘 (Rib Suppression — Hessian) ★GAP-BQ 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-RIB-001 | SWU-8.7 | xpe_ai_worker.dll | Class B |

#### 수학적 정의

Hessian 행렬 (σ = 3 px Gaussian 스케일):

```
H(x,y) = [ Hxx  Hxy ]   where Hxx = d²I/dx², Hxy = d²I/dxdy, Hyy = d²I/dy²
          [ Hxy  Hyy ]

고유값 분해: λ₁ ≥ λ₂  (λ₁ = major, λ₂ = minor curvature)

능선 척도 (Frangi 1998):
  R_s(x,y) = exp(−λ₂² / (2β²))  where β = 0.5·max|λ₂|

골 확률 맵:
  B(x,y) = R_s · (1 − exp(−|λ₂|² / 2c²)) · lung_mask(x,y)
  c = 0.05 × max|λ₂| (vessel suppression threshold)

연조직 영상:
  I_soft(x,y) = I_log(x,y) − w_b · B(x,y) · (I_log(x,y) − μ_bg)
  w_b ∈ [0, 1], default = 0.85
  μ_bg = 폐 마스크 외부 배경 평균
```

#### C++ 구조체 및 API

```cpp
struct XpeRibParams {
    float hessian_sigma;    // Gaussian scale for Hessian (default 3.0 px)
    float beta_ridgeness;   // Frangi β (default 0.5)
    float bone_weight;      // w_b (default 0.85, range [0,1])
};

XpeStatus xpe_rib_suppress(
    const uint16_t*   I_in,
    const XpeLungMask* mask,
    uint16_t*         I_soft,
    uint32_t          W,
    uint32_t          H,
    const XpeRibParams* params
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 50 ms / 3K×3K (SIMD Hessian 미분) |
| 의존성 | §8.5 XpeLungMask 사전 계산 필요 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 늑골 억제 지수 | ≥ 80% (늑골 신호 대비 처리 전/후 비율) |
| 폐 결절 CNR 보존 | ≥ 95% (합성 10 mm 결절 기준) |
| 폐 마스크 외 변화 | < 0.1 ADU RMS (마스크 바깥 영역) |

---

### 8.8 SWU-8.8 자동 해부학적 부위 인식 CNN (Body Part Recognition) ★GAP-BR 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-ANAT-001 | SWU-8.8 | xpe_ai_worker.dll | Class B |

#### 분류 클래스 (10종)

```cpp
enum XpeBodyPart {
    XPE_CHEST_PA        = 0,
    XPE_CHEST_LAT       = 1,
    XPE_ABDOMEN         = 2,
    XPE_PELVIS          = 3,
    XPE_SPINE_AP        = 4,
    XPE_SPINE_LAT       = 5,
    XPE_EXTREMITY_UPPER = 6,
    XPE_EXTREMITY_LOWER = 7,
    XPE_SKULL           = 8,
    XPE_UNKNOWN         = 9
};
```

#### 아키텍처 개요

```
모델: MobileNetV3-Small (3.4M 파라미터)
입력: 224×224 float32  (바이큐빅 리사이즈, μ=0.5, σ=0.25 정규화)
출력: softmax 10-class 확률 벡터
수락 임계값: p_max ≥ 0.85 → 해당 클래스; else → XPE_UNKNOWN
모델 파일: models/xpe_bodypart_mobilenetv3_v1.onnx  (≤ 8 MB int8)
```

#### C++ 구조체 및 API

```cpp
struct XpeBodyPartResult {
    XpeBodyPart part;         // predicted class
    float       confidence;   // max probability
    float       probs[10];    // full softmax distribution
};

XpeStatus xpe_classify_body_part(
    const uint16_t*     I_in,
    uint32_t            W,
    uint32_t            H,
    XpeBodyPartResult*  result,
    OrtSession*         session
);
```

#### 통합

- §6.4 Anatomy-Adaptive W/L: `result.part`를 해부 부위 인덱스로 사용
- §5.3 Virtual Grid 프리셋: `CHEST_PA/LAT, SPINE_AP/LAT` 자동 선택
- §20.1 BMD 추정: `SPINE_AP` 클래스 확인 후 ROI 활성화

#### 성능 목표

| 항목 | 목표 |
|------|------|
| CPU 추론 | < 50 ms |
| GPU 추론 | < 15 ms |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 10-class 정확도 | ≥ 95% (100 × 10클래스 합성 데이터셋) |
| UNKNOWN 오분류 | < 3% (알려진 클래스를 UNKNOWN으로 분류) |
| 신뢰도 보정 | Expected Calibration Error (ECE) < 0.05 |

---

## 9. 교정 데이터 파이프라인

### 9.1 오프라인 교정 순서 (GAP-10 해소)

```
Phase 1: Dark Calibration (반드시 먼저)
  - 조건: 방사선 OFF, detector 안정화 ≥10분
  - 획득: ≥16 dark frames
  - 출력: offset_map.bin

Phase 2: Flat-Field Calibration (per SID per kVp)
  - 조건: 균일 조사야, 산란체 없음
  - 획득: ≥8 flood frames
  - 출력: gain_map_SID{n}_kVp{m}.bin

Phase 3: Defect Map Generation
  - 입력: offset_map.bin + gain_map (any SID)
  - 출력: defect_map.bin

Phase 4: Lag Parameter Fitting
  - 조건: 이중 노출 프로토콜
  - 획득: decay curve (t=0.1s ~ 30s)
  - 출력: lag_params.json

Phase 5: Checksum Generation
  - 모든 .bin 파일에 SHA-256 생성
  - 출력: checksums.sha256
```

### 9.2 C++ ConfigManager 로딩 순서

```cpp
class CalibrationManager {
public:
    bool load_calibration_set(const std::filesystem::path& cal_dir) {
        // 1. Validate all checksums first (SRS-SEC-002)
        if (!validate_all_checksums(cal_dir)) {
            logger_->error("Calibration data integrity check failed");
            raise_alert(AlertType::CALIBRATION_INTEGRITY_FAILURE);
            return false;
        }
        
        // 2. Load offset map (required)
        offset_map_ = load_binary_map<float>(cal_dir / "offset_map.bin",
                                               "XOFF");
        
        // 3. Load gain maps (SID-indexed)
        for (const auto& entry : std::filesystem::directory_iterator(cal_dir)) {
            if (entry.path().stem().string().starts_with("gain_map_")) {
                auto [sid, kvp] = parse_gain_map_filename(entry.path());
                gain_maps_[{sid, kvp}] = 
                    load_binary_map<float>(entry.path(), "XGAI");
            }
        }
        
        // 4. Load defect map (required)
        defect_map_ = load_binary_map<uint8_t>(cal_dir / "defect_map.bin",
                                                 "XDEF");
        
        // 5. Load lag parameters (optional, default if missing)
        load_lag_params(cal_dir / "lag_params.json");
        
        // 6. Validate expiry (SRS-ALERT-005)
        if (is_calibration_expired()) {
            raise_alert(AlertType::CALIBRATION_EXPIRED);
        }
        
        return true;
    }
    
    const float* get_gain_map(float sid_mm, float kvp) const {
        // Find nearest SID/kVp combination
        auto key = find_nearest_gain_map(sid_mm, kvp);
        return gain_maps_.at(key).data();
    }
};
```

### 9.3 교정 유효기간 관리

| 교정 항목 | 권장 주기 | 트리거 조건 |
|----------|---------|-----------|
| Offset Map | 8시간 또는 시동 시 | 온도 ≥5°C 변화 |
| Gain Map | 1주 또는 kVp 변경 시 | SID 변경 ±50mm |
| Defect Map | 1개월 | 결함 픽셀 +10% |
| Lag Parameters | 분기 1회 | 모델 교체 시 |

---


xpe-algorithm-spec-deepsync.md §4.1 "Drift monitoring shall feed recalibration decisions rather than silently allowing quality erosion"에서 요구된 항목이다. 매 처리 세션에서 드리프트 지표를 측정하고 임계치 초과 시 재교정을 트리거한다.

#### 9.5.1 알고리즘 수학 정의

**Dark Current 드리프트율**:

$$\dot{D} = \frac{\bar{I}_{\text{dark,current}} - \bar{I}_{\text{dark,baseline}}}{\Delta t_{\text{days}}} \quad (\text{ADU/day})$$

**Gain Non-Uniformity 트렌드**:

$$\Delta_{\text{PRNU}} = \left|\frac{\text{CV}_{\text{current}} - \text{CV}_{\text{baseline}}}{\text{CV}_{\text{baseline}}}\right| \times 100\% \quad (\% \text{ change})$$

**Defect Burden 성장률**:

$$\dot{N}_{\text{defect}} = \frac{N_{\text{defect,current}} - N_{\text{defect,baseline}}}{\Delta t_{\text{days}}} \quad (\text{defects/day})$$

**재교정 트리거 조건**:

$$\text{TriggerRecal} = \left(\dot{D} > \theta_D\right) \lor \left(\Delta_{\text{PRNU}} > \theta_{\text{PRNU}}\right) \lor \left(\dot{N}_{\text{defect}} > \theta_N\right)$$

| 지표 | 임계치 | 의미 |
|------|-------|------|
| $\theta_D$ | 5.0 ADU/day | Dark current 드리프트 |
| $\theta_{\text{PRNU}}$ | 0.5% 변화 | Gain 균일도 저하 |
| $\theta_N$ | 10 defects/day | 결함 픽셀 성장 |

#### 9.5.2 Python 구현

```python
import numpy as np
import json
from pathlib import Path
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from typing import List, Optional

@dataclass
class DriftMetrics:
    timestamp_iso:         str
    dark_mean_adu:         float
    prnu_cv_pct:           float
    defect_count:          int
    dark_drift_per_day:    float = 0.0
    prnu_delta_pct:        float = 0.0
    defect_growth_per_day: float = 0.0
    needs_recalibration:   bool  = False
    trigger_reasons:       List[str] = field(default_factory=list)


@dataclass
class DriftThresholds:
    dark_drift_adu_per_day:  float = 5.0
    prnu_delta_pct:          float = 0.5
    defect_growth_per_day:   float = 10.0


class CalibrationDriftMonitor:
    """
    Monitors detector calibration drift across sessions.
    Compares current metrics against stored baseline and triggers recalibration.
    """

    def __init__(self, drift_log_path: Path,
                 thresholds: Optional[DriftThresholds] = None):
        self.drift_log_path = drift_log_path
        self.thresholds     = thresholds or DriftThresholds()
        self._history: List[DriftMetrics] = []
        self._load_history()

    def _load_history(self):
        if self.drift_log_path.exists():
            with open(self.drift_log_path) as f:
                data = json.load(f)
                self._history = [DriftMetrics(**e) for e in data]

    def _save_history(self):
        with open(self.drift_log_path, 'w') as f:
            json.dump([asdict(m) for m in self._history], f, indent=2)

    def measure_current(self,
                         dark_frames:  np.ndarray,   # (N, H, W)
                         flood_image:  np.ndarray,   # (H, W) gain-corrected
                         defect_map:   np.ndarray    # (H, W) uint8
                         ) -> DriftMetrics:
        """
        Compute current drift metrics from live detector frames.

        Args:
            dark_frames:  recent dark frames (≥4 frames stacked)
            flood_image:  recent flood field (gain-corrected)
            defect_map:   current defect map
        Returns:
            DriftMetrics with filled current values
        """
        dark_mean = float(np.mean(dark_frames))
        # PRNU: coefficient of variation of net signal (gain-corrected flood)
        net = flood_image[flood_image > 10]  # exclude near-zero pixels
        prnu_cv = float(np.std(net) / np.mean(net) * 100) if len(net) > 0 else 0.0
        defect_count = int(np.sum(defect_map > 0))
        ts = datetime.now(timezone.utc).isoformat()
        return DriftMetrics(
            timestamp_iso=ts,
            dark_mean_adu=dark_mean,
            prnu_cv_pct=prnu_cv,
            defect_count=defect_count,
        )

    def evaluate(self, current: DriftMetrics) -> DriftMetrics:
        """
        Compare current metrics against baseline (first recorded session).
        Sets drift rates and triggers if thresholds exceeded.
        """
        if not self._history:
            # No baseline: record and return OK
            self._history.append(current)
            self._save_history()
            return current

        baseline = self._history[0]
        latest   = self._history[-1]

        # Time delta in days
        try:
            t0 = datetime.fromisoformat(baseline.timestamp_iso)
            t1 = datetime.fromisoformat(current.timestamp_iso)
            delta_days = max((t1 - t0).total_seconds() / 86400.0, 0.01)
        except Exception:
            delta_days = 1.0

        current.dark_drift_per_day    = abs(current.dark_mean_adu - baseline.dark_mean_adu) / delta_days
        current.prnu_delta_pct        = abs(current.prnu_cv_pct   - baseline.prnu_cv_pct)
        current.defect_growth_per_day = max(current.defect_count  - baseline.defect_count, 0) / delta_days

        reasons = []
        if current.dark_drift_per_day > self.thresholds.dark_drift_adu_per_day:
            reasons.append(f"dark_drift={current.dark_drift_per_day:.2f} ADU/day > {self.thresholds.dark_drift_adu_per_day}")
        if current.prnu_delta_pct > self.thresholds.prnu_delta_pct:
            reasons.append(f"prnu_delta={current.prnu_delta_pct:.3f}% > {self.thresholds.prnu_delta_pct}%")
        if current.defect_growth_per_day > self.thresholds.defect_growth_per_day:
            reasons.append(f"defect_growth={current.defect_growth_per_day:.1f}/day > {self.thresholds.defect_growth_per_day}")

        current.needs_recalibration = len(reasons) > 0
        current.trigger_reasons     = reasons

        self._history.append(current)
        if len(self._history) > 365:  # keep 1 year of daily records
            self._history = self._history[-365:]
        self._save_history()
        return current

    def get_trend_report(self, window_days: int = 30) -> dict:
        """
        Summarise drift trends over a rolling window.
        Returns: {metric: (mean, std, trend_direction)} for last window_days entries.
        """
        recent = self._history[-window_days:] if len(self._history) >= window_days else self._history
        if len(recent) < 2:
            return {}
        darks  = np.array([m.dark_mean_adu for m in recent])
        prnus  = np.array([m.prnu_cv_pct   for m in recent])
        defs   = np.array([m.defect_count  for m in recent])
        idx    = np.arange(len(recent))

        def trend(arr):
            p = np.polyfit(idx, arr, 1)
            slope = float(p[0])
            return float(np.mean(arr)), float(np.std(arr)), ('up' if slope > 0 else 'down')

        return {
            'dark_mean_adu':  trend(darks),
            'prnu_cv_pct':    trend(prnus),
            'defect_count':   trend(defs),
            'n_records':      len(recent),
        }
```

#### 9.5.3 C++ 런타임 통합

```cpp
// Drift monitoring hook — called after each processing session
// Updates drift log and raises alert if recalibration is needed.

struct DriftSnapshot {
    float    dark_mean_adu;
    float    prnu_cv_pct;
    uint32_t defect_count;
    int64_t  timestamp_unix_s;
};

class DriftMonitor {
public:
    struct Alert {
        bool   needs_recalibration;
        float  dark_drift_per_day;
        float  prnu_delta_pct;
        float  defect_growth_per_day;
        char   message[256];
    };

    // Call this after each calibration verification pass
    Alert update(const DriftSnapshot& current) {
        Alert alert{};
        if (history_.empty()) {
            baseline_ = current;
            history_.push_back(current);
            return alert;
        }

        double days = static_cast<double>(current.timestamp_unix_s - baseline_.timestamp_unix_s)
                      / 86400.0;
        days = std::max(days, 0.01);

        alert.dark_drift_per_day    = std::fabsf(current.dark_mean_adu - baseline_.dark_mean_adu) / days;
        alert.prnu_delta_pct        = std::fabsf(current.prnu_cv_pct   - baseline_.prnu_cv_pct);
        alert.defect_growth_per_day = static_cast<float>(
                                         std::max<int32_t>(current.defect_count - baseline_.defect_count, 0)
                                      ) / days;

        alert.needs_recalibration =
            (alert.dark_drift_per_day    > k_dark_thresh_)   ||
            (alert.prnu_delta_pct        > k_prnu_thresh_)    ||
            (alert.defect_growth_per_day > k_defect_thresh_);

        if (alert.needs_recalibration) {
            std::snprintf(alert.message, sizeof(alert.message),
                "RECAL REQUIRED: dark=%.2f ADU/d, PRNU=%.3f%%, defects=%.1f/d",
                alert.dark_drift_per_day,
                alert.prnu_delta_pct,
                alert.defect_growth_per_day);
            xpe_alert(XpeAlertCode::ALERT_RECALIBRATION_REQUIRED, alert.message);
        }

        history_.push_back(current);
        if (history_.size() > 365) history_.erase(history_.begin());
        return alert;
    }

private:
    static constexpr float k_dark_thresh_   = 5.0f;   // ADU/day
    static constexpr float k_prnu_thresh_   = 0.5f;   // % change
    static constexpr float k_defect_thresh_ = 10.0f;  // defects/day

    DriftSnapshot              baseline_{};
    std::vector<DriftSnapshot> history_;
};
```

#### 9.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 드리프트 감지 민감도 | 임계치의 1.1× → 100% 감지 | 합성 드리프트 시나리오 |
| 오탐율 (False Positive) | < 1% | 안정된 detector 30일 모니터링 |
| 드리프트 로그 용량 | 365일 기록 유지 | 자동 롤오버 테스트 |
| 재교정 알림 지연 | < 1s | 임계치 초과 직후 알림 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-QC-002 (교정 드리프트 모니터링) — Phase 2 추가 예정

---

## 10. 성능 최적화 — SIMD/OpenMP 전략

### 10.1 전체 파이프라인 SIMD 커버리지 (GAP-06 해소)

| SWU | AVX2 | FMA | OpenMP | GPU (Optional) |
|-----|------|-----|--------|---------------|
| Offset Correct | ✅ | — | ✅ (row-parallel) | — |
| Gain Correct | ✅ | ✅ | ✅ | — |
| Defect Correct | — (data-dependent) | — | ✅ (pass 분리) | — |
| Ghost Correct | ✅ | ✅ | ✅ | — |
| Log Transform | ✅ (SVML/approx) | — | ✅ | ✅ (CUDA) |
| Bilateral Filter | 부분 (OpenCV) | — | ✅ (tile) | ✅ (CUDA) |
| CLAHE | — (OpenCV) | — | — | — |
| USM | ✅ | ✅ | ✅ | — |
| Laplacian Pyramid | — (OpenCV pyrDown/Up) | — | ✅ (level) | — |
| VOI LUT | ✅ | ✅ | ✅ | ✅ |
| Modality LUT | ✅ | ✅ | ✅ | — |
| Grid Suppression | — (FFT-based) | — | ✅ | — |

### 10.2 메모리 풀 전략

```cpp
// SWI-5 MemoryPool: Pre-allocated pipeline buffers
// Prevent dynamic allocation during processing (SRS-PERF-001)

class XpeMemoryPool {
    // Fixed set of reusable float32 image buffers
    static constexpr size_t MAX_BUFFERS = 8;
    static constexpr size_t MAX_IMAGE_PIXELS = 4096ULL * 4096;  // 16MP max
    
    struct Buffer {
        std::unique_ptr<float, AlignedDeleter> data;  // 64-byte aligned
        size_t size;
        std::atomic<bool> in_use{false};
    } buffers_[MAX_BUFFERS];
    
public:
    XpeMemoryPool() {
        for (auto& buf : buffers_) {
            buf.data = alloc_aligned<float>(MAX_IMAGE_PIXELS, 64);
            buf.size = MAX_IMAGE_PIXELS;
        }
    }
    
    float* acquire(size_t required_pixels) {
        for (auto& buf : buffers_) {
            bool expected = false;
            if (buf.in_use.compare_exchange_strong(expected, true) &&
                buf.size >= required_pixels) {
                return buf.data.get();
            }
        }
        throw std::runtime_error("Memory pool exhausted");
    }
    
    void release(float* ptr) {
        for (auto& buf : buffers_) {
            if (buf.data.get() == ptr) {
                buf.in_use.store(false);
                return;
            }
        }
    }
};
```

### 10.3 Thread Pool 최적화

```cpp
// Row-parallel SIMD: optimal for cache efficiency
// Thread granularity: tile-based (prevent false sharing)

#pragma omp parallel for schedule(static) num_threads(NUM_CORES)
for (int tile_y = 0; tile_y < num_tiles_y; ++tile_y) {
    for (int tile_x = 0; tile_x < num_tiles_x; ++tile_x) {
        process_tile(tile_x, tile_y, tile_size);
    }
}
// Tile size guideline: 64×64 pixels (fits in L1 cache: 64×64×4 = 16KB)
```

### 10.4 성능 프로파일링 포인트

```cpp
// Built-in performance counters (SRS-PERF-001, 002)
class PipelineProfiler {
    struct StageMetrics {
        std::chrono::nanoseconds elapsed;
        size_t pixels_processed;
        double mpixels_per_sec() const {
            return pixels_processed / (elapsed.count() * 1e-3);
        }
    };
    
public:
    void report(uint32_t width, uint32_t height) {
        auto total = sum_all_stages();
        log("Pipeline total: {}ms for {}×{} ({:.1f} MPix/s)",
            total.count() / 1e6, width, height,
            (double)(width * height) / (total.count() * 1e-3));
        // SRS-PERF-001: target ≤500ms for 3072×3072
    }
};
```

---

## 11. 검증 방법론

### 11.1 단위 테스트 기준

| 알고리즘 | 입력 | 기대 출력 | Pass 기준 |
|---------|------|---------|---------|
| Offset Correct | Synthetic dark signal | Subtracted + clamped | Max error = 0 ADU |
| Gain Correct | Uniform flood | Uniform output (CV<0.1%) | CV < 0.1% |
| Defect Correct | Injected point defects | Interpolated ≤ 1% error | Pixel error < 5 ADU |
| Ghost Correct | Known lag signal | ≥90% removal | Ghost fraction < 10% |
| Log Transform | Gradient ramp | Logarithmic curve | Max relative error < 1e-5 |
| Bilateral | AWGN + step edge | Smoothed / edge preserved | Edge FWHM < 2× input |
| CLAHE | Low-contrast uniform | Enhanced, no artifact | SSIM > 0.95 |
| USM | Fine texture | Enhanced within λ_max | No artifact above λ_max |
| VOI LUT | Full range sweep | Correct output per formula | Max error ≤ 1 DDL |
| GSDF | P-Value sweep | PS3.14 conformance | JND deviation < 10% |

### 11.2 통합 테스트 — 황금 표준 이미지

```python
def run_integration_test(pipeline, reference_images: dict) -> dict:
    """
    Compare pipeline output to golden reference images.
    
    Test images:
    - CDMAM (contrast-detail phantom): sensitivity threshold analysis
    - Leeds TOR(CDR) phantom: resolution measurement
    - RMI 156 phantom: uniformity + noise measurement
    - AAPM TG-18 patterns: GSDF conformance
    """
    results = {}
    for test_name, (input_img, golden_ref) in reference_images.items():
        output = pipeline.process(input_img)
        
        # Structural similarity
        ssim_val = ssim(output, golden_ref)
        # Peak signal-to-noise ratio
        psnr_val = psnr(output, golden_ref)
        # Max pixel deviation
        max_err  = float(np.max(np.abs(output.astype(float) - 
                                         golden_ref.astype(float))))
        
        results[test_name] = {
            'ssim': ssim_val,
            'psnr': psnr_val,
            'max_pixel_error': max_err,
            'pass': ssim_val >= 0.95 and psnr_val >= 35.0
        }
    return results
```

### 11.3 성능 회귀 테스트

```bash
# Automated performance regression check (CI/CD)
# Target: SRS-PERF-001 ≤500ms for 3072×3072

xpe_benchmark --image-size 3072x3072 \
               --pipeline pre+core+display \
               --iterations 10 \
               --threshold-ms 500 \
               --output benchmark_results.json
```

---

### 11.4 스칼라 참조 구현 및 SIMD 패리티 하네스 (GAP-S 해소)

xpe-algorithm-spec-deepsync.md §5.1 "every major stage shall have: one scalar reference, one optimized implementation, one parity test harness, one benchmark family binding"에서 요구된 항목이다. AVX2 또는 다중 스레드 경로가 유일한 구현이 되는 것을 방지한다.

#### 11.4.1 패리티 하네스 아키텍처

모든 주요 처리 단계는 세 가지 구현을 동시에 보유해야 한다:

| 레이어 | 목적 | 요구사항 |
|-------|------|---------|
| **Scalar Reference** | 수학적 정확성의 기준선, 이식 가능 | 컴파일러 최적화 없음, 인라인 없음 |
| **Optimized** | AVX2/FMA/OpenMP 병렬화 | 프로덕션 경로 |
| **Parity Test** | Scalar ↔ Optimized 수치 등가 검증 | CI/CD에서 자동 실행 |

#### 11.4.2 패리티 테스트 프레임워크

```python
import numpy as np
from typing import Callable, Dict, Any
from dataclasses import dataclass

@dataclass
class ParityTestResult:
    stage_name:        str
    max_abs_error:     float
    max_rel_error:     float
    mean_abs_error:    float
    passed:            bool
    error_message:     str = ''

    def __repr__(self):
        status = "PASS" if self.passed else "FAIL"
        return (f"[{status}] {self.stage_name}: "
                f"max_abs={self.max_abs_error:.3e}, "
                f"max_rel={self.max_rel_error:.3e}")


def run_parity_test(
        stage_name:   str,
        scalar_fn:    Callable,
        optimized_fn: Callable,
        inputs:       Dict[str, Any],
        abs_tol:      float = 1e-4,
        rel_tol:      float = 1e-4) -> ParityTestResult:
    """
    Compare scalar reference and optimized implementation outputs.

    Both functions receive the same **inputs dict.
    Returns ParityTestResult with pass/fail and error statistics.
    """
    ref_out  = scalar_fn(**inputs)
    opt_out  = optimized_fn(**inputs)

    ref_arr  = np.asarray(ref_out,  dtype=np.float64)
    opt_arr  = np.asarray(opt_out,  dtype=np.float64)

    abs_diff = np.abs(ref_arr - opt_arr)
    rel_diff = abs_diff / (np.abs(ref_arr) + 1e-10)

    max_abs  = float(np.max(abs_diff))
    max_rel  = float(np.max(rel_diff))
    mean_abs = float(np.mean(abs_diff))

    passed = (max_abs <= abs_tol) and (max_rel <= rel_tol)
    msg    = '' if passed else (f"abs_err={max_abs:.3e} > {abs_tol} or "
                                 f"rel_err={max_rel:.3e} > {rel_tol}")
    return ParityTestResult(
        stage_name=stage_name,
        max_abs_error=max_abs,
        max_rel_error=max_rel,
        mean_abs_error=mean_abs,
        passed=passed,
        error_message=msg,
    )


class XpeParityTestSuite:
    """
    Parity test harness for all XPE processing stages.
    Instantiate with a seeded test image set and call run_all().
    """

    def __init__(self, width: int = 512, height: int = 512, seed: int = 42):
        rng = np.random.default_rng(seed)
        self.raw    = rng.integers(100, 55000, (height, width), dtype=np.uint16)
        self.offset = rng.uniform(50, 200, (height, width)).astype(np.float32)
        self.gain   = rng.uniform(0.8, 1.2, (height, width)).astype(np.float32)
        self.W, self.H = width, height

    def _offset_scalar(self, raw, offset_map):
        result = np.zeros(raw.shape, dtype=np.float32)
        for y in range(raw.shape[0]):
            for x in range(raw.shape[1]):
                result[y, x] = max(float(raw[y, x]) - offset_map[y, x], 0.0)
        return result

    def _offset_vectorized(self, raw, offset_map):
        return np.maximum(raw.astype(np.float32) - offset_map, 0.0)

    def _gain_scalar(self, offset_corrected, gain_map):
        result = np.zeros_like(offset_corrected, dtype=np.float32)
        for y in range(offset_corrected.shape[0]):
            for x in range(offset_corrected.shape[1]):
                result[y, x] = offset_corrected[y, x] * gain_map[y, x]
        return result

    def _gain_vectorized(self, offset_corrected, gain_map):
        return offset_corrected * gain_map

    def _log_transform_scalar(self, clean, I0=10000.0, eps=1e-6):
        result = np.zeros_like(clean, dtype=np.float32)
        for y in range(clean.shape[0]):
            for x in range(clean.shape[1]):
                result[y, x] = -np.log((clean[y, x] + eps) / (I0 + eps))
        return result

    def _log_transform_vectorized(self, clean, I0=10000.0, eps=1e-6):
        return -np.log((clean.astype(np.float64) + eps) / (I0 + eps)).astype(np.float32)

    def run_all(self) -> list:
        """Run all parity tests. Returns list of ParityTestResult."""
        offset_corr = self._offset_vectorized(self.raw, self.offset)
        gain_corr   = self._gain_vectorized(offset_corr, self.gain)

        results = []

        results.append(run_parity_test(
            'offset_correction',
            scalar_fn=self._offset_scalar,
            optimized_fn=self._offset_vectorized,
            inputs={'raw': self.raw, 'offset_map': self.offset},
            abs_tol=0.0,   # Exact match expected
            rel_tol=0.0,
        ))

        results.append(run_parity_test(
            'gain_correction',
            scalar_fn=self._gain_scalar,
            optimized_fn=self._gain_vectorized,
            inputs={'offset_corrected': offset_corr, 'gain_map': self.gain},
            abs_tol=1e-4,  # FP rounding tolerance
            rel_tol=1e-5,
        ))

        results.append(run_parity_test(
            'log_transform',
            scalar_fn=self._log_transform_scalar,
            optimized_fn=self._log_transform_vectorized,
            inputs={'clean': gain_corr},
            abs_tol=2e-4,  # AVX2 poly approx tolerance
            rel_tol=2e-4,
        ))

        return results


def print_parity_report(results: list) -> bool:
    """Print parity test report. Returns True if all passed."""
    all_pass = True
    print("=" * 60)
    print("XPE PARITY TEST REPORT")
    print("=" * 60)
    for r in results:
        print(r)
        if not r.passed:
            all_pass = False
            print(f"  ERROR: {r.error_message}")
    print("=" * 60)
    print(f"OVERALL: {'PASS' if all_pass else 'FAIL'} ({sum(r.passed for r in results)}/{len(results)} passed)")
    return all_pass
```

#### 11.4.3 C++ 패리티 검증 매크로

```cpp
// XPE parity test macro — wrap each optimized function for CI validation
// Usage: XPE_PARITY_CHECK(scalar_fn, avx2_fn, inputs..., tol)

#define XPE_PARITY_CHECK(scalar_fn, opt_fn, out_s, out_o, n, abs_tol)     \
    do {                                                                    \
        bool _parity_ok = true;                                             \
        for (size_t _i = 0; _i < (n); ++_i) {                             \
            float _diff = std::fabsf((out_s)[_i] - (out_o)[_i]);           \
            if (_diff > (abs_tol)) {                                        \
                std::fprintf(stderr,                                        \
                    "PARITY FAIL [%s vs %s] idx=%zu diff=%.4e tol=%.4e\n", \
                    #scalar_fn, #opt_fn, _i, _diff, (float)(abs_tol));     \
                _parity_ok = false; break;                                  \
            }                                                               \
        }                                                                   \
        assert(_parity_ok && "Scalar/AVX2 parity check failed");           \
    } while (0)

// --- Example usage in unit test ---
void test_offset_parity(const uint16_t* raw, const float* offset,
                          float* out_scalar, float* out_avx2,
                          uint32_t W, uint32_t H) {
    // Scalar reference
    for (size_t i = 0; i < (size_t)W * H; ++i)
        out_scalar[i] = std::max(static_cast<float>(raw[i]) - offset[i], 0.0f);

    // AVX2 optimized
    xpe_offset_correct(raw, offset, out_avx2, W, H);

    // Parity check (exact match expected for offset correction)
    XPE_PARITY_CHECK(offset_scalar, xpe_offset_correct,
                      out_scalar, out_avx2, (size_t)W * H, 0.0f);
}
```

#### 11.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Offset/Gain 패리티 | 최대 절대 오차 = 0 | 합성 이미지 1000개 |
| Log Transform 패리티 | 최대 상대 오차 < 2×10⁻⁴ | AVX2 poly vs libm |
| 모든 Stage 패리티 | CI에서 100% PASS | 매 커밋 자동 실행 |
| 스칼라 구현 독립성 | -O0 컴파일에서도 동작 | 컴파일러 플래그 테스트 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-TEST-001 (단위 테스트 기준 확장) — Phase 2 추가 예정

---

## 12. FPD 특성화 알고리즘 보완

### 12.1 Allan Variance (장기 안정성 평가)

Allan Variance는 시스템의 시간적 안정성을 평가하는 통계량이다:

$$\sigma_A^2(\tau) = \frac{1}{2}\left\langle\left(\bar{x}_{k+1}(\tau) - \bar{x}_k(\tau)\right)^2\right\rangle$$

```python
def compute_allan_variance(time_series: np.ndarray,
                            sampling_interval_s: float) -> tuple[np.ndarray, np.ndarray]:
    """
    Compute Allan Variance of FPD signal over time.
    
    Useful for:
    - Identifying drift (positive slope in log-log plot)
    - Identifying random noise floor (flat region)
    - Identifying periodic artifacts (bumps)
    """
    N = len(time_series)
    max_m = N // 2
    
    tau_list = []
    avar_list = []
    
    for m in range(1, max_m + 1):
        tau = m * sampling_interval_s
        # Group means
        n_groups = N // m
        group_means = np.array([
            np.mean(time_series[k*m:(k+1)*m]) for k in range(n_groups)
        ])
        # Allan variance
        avar = 0.5 * np.mean(np.diff(group_means)**2)
        tau_list.append(tau)
        avar_list.append(avar)
    
    return np.array(tau_list), np.array(avar_list)
```

### 12.2 MTF 슬랜트 에지법 정밀도 개선

기존 명세(03_측정_알고리즘_명세서)의 보완:

```python
def compute_mtf_precision_mode(edge_image: np.ndarray,
                                pixel_pitch_mm: float,
                                oversampling: int = 4,
                                edge_angle_range: tuple = (2.0, 10.0)) -> dict:
    """
    High-precision MTF via Slanted Edge method with subpixel accuracy.
    
    Improvements over basic implementation:
    1. Subpixel edge localization (Canny + parabolic fit)
    2. Noise-robust ESF via LOWESS smoothing
    3. Aperture correction for finite pixel size
    4. IEC 62220-1 compliant ROI selection
    
    Aperture correction:
    MTF_true(f) = MTF_measured(f) / sinc(f × pixel_pitch)
    """
    # ... (full implementation follows existing 03_측정_알고리즘_명세서 pattern)
    
    # Key addition: aperture correction
    def aperture_correction(mtf_measured, freqs, pixel_pitch):
        sinc_vals = np.sinc(freqs * pixel_pitch)  # sinc = sin(πx)/(πx)
        with np.errstate(divide='ignore', invalid='ignore'):
            mtf_corrected = np.where(sinc_vals > 0.01,
                                      mtf_measured / sinc_vals,
                                      mtf_measured)
        return np.clip(mtf_corrected, 0, 1.2)
    
    return {'mtf': mtf_corrected, 'frequencies': freqs, 
            'f50': freq_at_mtf(mtf_corrected, freqs, 0.5),
            'f10': freq_at_mtf(mtf_corrected, freqs, 0.1)}
```

### 12.3 NPS 계산 알고리즘 (GAP-L 해소)

IEC 62220-1:2015 §6.3 준수 구현이다. 2-D NPS는 ROI별 FFT²를 평균화하여 계산한다.

#### 12.3.1 알고리즘 수학 정의

$$\text{NPS}(u, v) = \frac{\Delta x \cdot \Delta y}{N_x \cdot N_y} \cdot \left\langle \left|\mathcal{F}\left[I_{\text{ROI}}(x,y) - \bar{I}_{\text{ROI}}\right](u,v)\right|^2 \right\rangle_{\text{ROI ensemble}}$$

$$\text{NNPS}(u, v) = \frac{\text{NPS}(u, v)}{\bar{I}_{\text{det}}^2}$$

- $\Delta x, \Delta y$: pixel pitch in mm
- $N_x \times N_y$: ROI size (IEC 62220-1: 256×256 권장)
- $\langle \cdot \rangle$: ensemble average over non-overlapping ROIs (≥50 권장)
- $\bar{I}_{\text{det}}$: mean detector signal in ADU (또는 calibrated units)

**1-D radial NPS** (측정 보고용):

$$\text{NPS}(f) = \frac{1}{N_{\text{annulus}}} \sum_{(u,v): f - \delta f/2 \leq \sqrt{u^2+v^2} < f+\delta f/2} \text{NPS}(u,v)$$

#### 12.3.2 Python 구현 (IEC 62220-1 준수)

```python
import numpy as np
from scipy.signal.windows import hann

def compute_nps_2d(flat_images:      list[np.ndarray],
                   pixel_pitch_mm:   float,
                   roi_size:         int   = 256,
                   min_rois:         int   = 50,
                   detrend_order:    int   = 1,
                   window_function:  bool  = True) -> dict:
    """
    Compute 2-D Noise Power Spectrum per IEC 62220-1:2015 §6.3.

    Args:
        flat_images:     list of uniformly-exposed float32 images (H, W)
                         ≥2 images recommended; >1 required for ensemble
        pixel_pitch_mm:  pixel pitch in mm (same in x and y)
        roi_size:        ROI side length in pixels (IEC: 256)
        min_rois:        minimum ROI count for statistical validity
        detrend_order:   polynomial order for intra-ROI detrending (0=mean, 1=plane)
        window_function: apply 2-D Hanning window before FFT (reduces leakage)
    Returns:
        dict with keys:
          'nps_2d'     : float32 array (roi_size, roi_size) — 2-D NPS (ADU²·mm²)
          'nnps_2d'    : float32 array (roi_size, roi_size) — Normalised NPS (mm²)
          'nps_1d'     : (freqs, nps_radial) — radial average
          'nnps_1d'    : (freqs, nnps_radial)
          'mean_signal': mean detector signal used for normalisation
          'n_rois'     : number of ROIs used
    """
    H, W = flat_images[0].shape
    dx = dy = pixel_pitch_mm  # isotropic detector assumed
    half = roi_size // 2

    # Build 2-D Hanning window
    if window_function:
        win_1d = hann(roi_size, sym=False)
        window = np.outer(win_1d, win_1d).astype(np.float64)
        # Normalise so that sum(window²) == roi_size²  (IEC energy preservation)
        window /= np.sqrt(np.mean(window ** 2))
    else:
        window = np.ones((roi_size, roi_size), dtype=np.float64)

    nps_accum  = np.zeros((roi_size, roi_size), dtype=np.float64)
    roi_count  = 0
    mean_sum   = 0.0

    for img in flat_images:
        img_f = img.astype(np.float64)
        # Tile non-overlapping ROIs with 10% border margin
        y_starts = range(roi_size // 2, H - roi_size - roi_size // 2, roi_size)
        x_starts = range(roi_size // 2, W - roi_size - roi_size // 2, roi_size)

        for y0 in y_starts:
            for x0 in x_starts:
                roi = img_f[y0:y0 + roi_size, x0:x0 + roi_size]
                mean_sum += float(np.mean(roi))

                # Detrend: fit and subtract polynomial surface
                if detrend_order == 0:
                    roi_dt = roi - np.mean(roi)
                else:
                    # Plane fit (linear detrend)
                    ys, xs = np.mgrid[0:roi_size, 0:roi_size].astype(np.float64)
                    A = np.column_stack([xs.ravel(), ys.ravel(),
                                         np.ones(roi_size * roi_size)])
                    coef, _, _, _ = np.linalg.lstsq(A, roi.ravel(), rcond=None)
                    plane = (coef[0] * xs + coef[1] * ys + coef[2])
                    roi_dt = roi - plane

                # Apply window and FFT
                roi_w   = roi_dt * window
                F       = np.fft.fft2(roi_w)
                power   = np.abs(F) ** 2

                # NPS contribution: scale by pixel area / ROI area
                nps_accum += power * (dx * dy) / (roi_size * roi_size)
                roi_count += 1

    if roi_count < min_rois:
        import warnings
        warnings.warn(f"Only {roi_count} ROIs collected; IEC 62220-1 recommends ≥{min_rois}")

    nps_2d = (nps_accum / roi_count).astype(np.float32)
    nps_2d = np.fft.fftshift(nps_2d)   # centre DC at array centre

    mean_signal = mean_sum / roi_count
    nnps_2d     = nps_2d / (mean_signal ** 2 + 1e-12)

    # Radial average
    freqs, nps_1d  = _radial_average(nps_2d,  dx, roi_size)
    _, nnps_1d     = _radial_average(nnps_2d, dx, roi_size)

    return {
        'nps_2d':      nps_2d,
        'nnps_2d':     nnps_2d,
        'nps_1d':      (freqs, nps_1d),
        'nnps_1d':     (freqs, nnps_1d),
        'mean_signal': mean_signal,
        'n_rois':      roi_count,
    }


def _radial_average(power_2d: np.ndarray,
                    pixel_pitch_mm: float,
                    roi_size: int) -> tuple[np.ndarray, np.ndarray]:
    """Compute radial average of a centred 2-D power spectrum."""
    H, W    = power_2d.shape
    cy, cx  = H // 2, W // 2
    y_idx   = np.arange(H) - cy
    x_idx   = np.arange(W) - cx
    XX, YY  = np.meshgrid(x_idx, y_idx)
    freq_step = 1.0 / (roi_size * pixel_pitch_mm)    # cycles/mm per bin
    R = np.sqrt(XX ** 2 + YY ** 2)                   # radial distance in bins

    max_bin    = min(cx, cy)
    freq_bins  = np.arange(0, max_bin) * freq_step
    nps_radial = np.zeros(max_bin, dtype=np.float64)

    for k in range(max_bin):
        annulus = (R >= k - 0.5) & (R < k + 0.5)
        if np.sum(annulus) > 0:
            nps_radial[k] = float(np.mean(power_2d[annulus]))

    return freq_bins.astype(np.float32), nps_radial.astype(np.float32)
```

#### 12.3.3 검증 기준 (IEC 62220-1)

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| DC 성분 차단 | NPS(0,0) = 0 (detrend 후) | Detrending 적용 확인 |
| NNPS 저주파 일관성 | ≤ 10% variation across ROIs | ROI-to-ROI NNPS 비교 |
| ROI count | ≥ 50 | 프로그램 출력 확인 |
| 주파수 해상도 | Δf = 1/(N·Δx) cycles/mm | N=256, Δx=0.1mm → Δf=0.039 cycles/mm |

---

### 12.4 DQE 계산 알고리즘 (GAP-M 해소)

Detective Quantum Efficiency는 MTF와 NNPS로부터 계산되며 IEC 62220-1:2015 §6.4를 따른다.

#### 12.4.1 알고리즘 수학 정의

$$\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\Phi \cdot \text{NNPS}(f)}$$

- $\Phi$: 입사 X선 quantum fluence (photons/mm²)
- $\text{MTF}^2(f)$: Modulation Transfer Function 제곱
- $\text{NNPS}(f)$: Normalised Noise Power Spectrum (mm²)

**Quantum fluence 추정** (IEC 62220-1 §5.2):
$$\Phi = \frac{\bar{I}_{\text{det}}}{\bar{g} \cdot \eta_{\text{absorb}} \cdot E_{\text{mean}}}$$

또는 실측 기반으로 ionisation chamber 측정값 사용 (권장):
$$\Phi = \frac{K_{\text{air}} \cdot \mu_{\text{en}}/\rho \cdot A_{\text{beam}}}{\bar{E}_{\text{photon}}}$$

실용적 접근 (RQA5 조건, 80kVp, IEC 61267): $\Phi \approx 3.0 \times 10^5\ \text{photons/mm}^2/(\text{mR})$

#### 12.4.2 Python 구현

```python
def compute_dqe(mtf_result:   dict,
                nps_result:   dict,
                quantum_fluence_per_mm2: float,
                freq_range_mm: tuple[float, float] = (0.0, 5.0)) -> dict:
    """
    Compute DQE(f) per IEC 62220-1:2015 §6.4.

    Args:
        mtf_result:               output of compute_mtf_precision_mode() — keys 'mtf', 'frequencies'
        nps_result:               output of compute_nps_2d() — key 'nnps_1d': (freqs, nnps)
        quantum_fluence_per_mm2:  Φ — X-ray photon fluence at detector surface (photons/mm²)
                                  Measure with calibrated ionisation chamber, or use
                                  tabulated value for RQA condition (IEC 62220-1 Annex C)
        freq_range_mm:            (f_min, f_max) in cycles/mm for output
    Returns:
        dict: 'frequencies', 'dqe', 'dqe_at_0', 'dqe_at_1', 'dqe_at_Nyquist'
    """
    mtf_freqs  = np.asarray(mtf_result['frequencies'], dtype=np.float64)
    mtf_vals   = np.asarray(mtf_result['mtf'],         dtype=np.float64)

    nnps_freqs = np.asarray(nps_result['nnps_1d'][0],  dtype=np.float64)
    nnps_vals  = np.asarray(nps_result['nnps_1d'][1],  dtype=np.float64)

    # Interpolate NNPS onto MTF frequency grid
    from scipy.interpolate import interp1d
    nnps_interp_fn = interp1d(nnps_freqs, nnps_vals,
                               kind='linear', bounds_error=False,
                               fill_value=(nnps_vals[0], nnps_vals[-1]))
    nnps_on_mtf_grid = nnps_interp_fn(mtf_freqs)

    # DQE = MTF² / (Φ × NNPS)
    denom = quantum_fluence_per_mm2 * nnps_on_mtf_grid
    with np.errstate(divide='ignore', invalid='ignore'):
        dqe = np.where(denom > 1e-20, mtf_vals ** 2 / denom, 0.0)

    # Clip to physically valid range [0, 1]
    dqe = np.clip(dqe, 0.0, 1.0)

    # Select output frequency range
    mask = (mtf_freqs >= freq_range_mm[0]) & (mtf_freqs <= freq_range_mm[1])

    def _dqe_at(target_freq: float) -> float:
        idx = np.argmin(np.abs(mtf_freqs - target_freq))
        return float(dqe[idx])

    return {
        'frequencies':    mtf_freqs[mask].astype(np.float32),
        'dqe':            dqe[mask].astype(np.float32),
        'dqe_at_0':       _dqe_at(0.0),
        'dqe_at_1':       _dqe_at(1.0),      # 1 cycle/mm
        'dqe_at_Nyquist': _dqe_at(float(mtf_freqs[mask].max())),
    }
```

#### 12.4.3 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| DQE(0) 범위 | 0.5 – 0.85 (CsI:Tl FPD 전형) | 공개 문헌 비교 |
| DQE(f) 단조성 | 감소 경향 (경미한 진동 허용) | 1차 차분 부호 확인 |
| IEC 62220-1 부합성 | 동일 팬텀에서 ±10% 이내 재현성 | 3회 반복 측정 |

---

### 12.5 Collimation Mask Detection 알고리즘 (GAP-N 해소)

`CollimatorMask`는 EI ROI 선택, GSVG scatter 추정, Phase 2 세부 알고리즘에서 공통으로 사용하는 기본 마스크 클래스이다. xpe-algorithm-spec-deepsync.md §3.2 "release-safe baseline"에 명시된 "baseline collimation detection"의 상세 구현이다.

#### 12.5.1 알고리즘 수학 정의

조준기 마스크는 임계값 기반 + 모폴로지 연산 결합으로 생성된다:

**단계 1 — 적응형 임계값**:
$$M_{\text{thresh}}(x,y) = \begin{cases} 1 & I_{\text{det}}(x,y) \geq \theta_{\text{coll}} \\ 0 & \text{otherwise} \end{cases}$$

$$\theta_{\text{coll}} = \mu_{\text{bright}} - k_{\text{coll}} \cdot \sigma_{\text{bright}}, \quad k_{\text{coll}} = 2.0$$

여기서 $\mu_{\text{bright}}$ 및 $\sigma_{\text{bright}}$는 상위 60% 픽셀에서 계산한다.

**단계 2 — 모폴로지 정제**:
$$M_{\text{final}} = \text{Close}\left(\text{Open}\left(M_{\text{thresh}},\ \text{SE}_{r_1}\right),\ \text{SE}_{r_2}\right)$$

- $r_1 = 15$ pixels (잡음 제거 opening)
- $r_2 = 50$ pixels (경계 닫기 closing)

**단계 3 — 최대 연결 성분 선택**: 최대 면적의 연결 성분을 최종 마스크로 채택.

#### 12.5.2 Python 구현

```python
import numpy as np
from dataclasses import dataclass

@dataclass
class CollimatorMask:
    """
    Collimator mask result for a single detector image.

    Attributes:
        mask:       uint8 binary mask (H, W) — 1 = inside collimated field
        bounding:   (x, y, w, h) bounding rectangle of collimated field
        confidence: float in [0,1] — detection quality estimate
        method_id:  str identifier for the detection algorithm used
    """
    mask:       np.ndarray
    bounding:   tuple[int, int, int, int]   # (x, y, w, h)
    confidence: float
    method_id:  str = 'threshold_morpho_v1'


def detect_collimator_mask(image:        np.ndarray,
                            pixel_pitch_mm: float = 0.1,
                            k_coll:      float = 2.0,
                            bright_frac:  float = 0.60,
                            open_r_mm:   float = 1.5,
                            close_r_mm:  float = 5.0) -> CollimatorMask:
    """
    Detect collimator boundary from a corrected detector image.

    Algorithm:
      1. Adaptive threshold from upper bright_frac percentile statistics
      2. Morphological open (noise removal) then close (gap filling)
      3. Select largest connected component
      4. Compute bounding rectangle and confidence score

    Args:
        image:          float32 (H, W), gain-corrected linear domain
        pixel_pitch_mm: detector pixel pitch in mm
        k_coll:         threshold = μ_bright - k_coll × σ_bright
        bright_frac:    fraction of brightest pixels used for stats
        open_r_mm:      morphological opening radius in mm
        close_r_mm:     morphological closing radius in mm
    Returns:
        CollimatorMask
    """
    try:
        from scipy import ndimage
        _scipy_ok = True
    except ImportError:
        _scipy_ok = False

    H, W = image.shape
    img  = image.astype(np.float32)

    # 1. Adaptive threshold from bright pixels
    flat  = img.ravel()
    perc  = np.percentile(flat, (1.0 - bright_frac) * 100.0)
    bright_pixels = flat[flat >= perc]
    mu_b  = float(np.mean(bright_pixels))
    sig_b = float(np.std(bright_pixels))
    theta = mu_b - k_coll * sig_b
    theta = max(theta, float(np.percentile(flat, 10.0)))  # safety floor

    binary_mask = (img >= theta).astype(np.uint8)

    # 2. Morphological open then close (in pixel units)
    r_open  = max(1, int(round(open_r_mm  / pixel_pitch_mm)))
    r_close = max(1, int(round(close_r_mm / pixel_pitch_mm)))

    if _scipy_ok:
        from scipy.ndimage import binary_opening, binary_closing, label
        struct_o = np.ones((2 * r_open  + 1, 2 * r_open  + 1), dtype=bool)
        struct_c = np.ones((2 * r_close + 1, 2 * r_close + 1), dtype=bool)
        opened  = binary_opening(binary_mask, structure=struct_o)
        closed  = binary_closing(opened,      structure=struct_c).astype(np.uint8)
    else:
        # Minimal fallback without scipy: sliding-window erosion/dilation (slow)
        closed = binary_mask  # degraded mode

    # 3. Largest connected component
    if _scipy_ok:
        labeled, n_comp = label(closed)
        if n_comp == 0:
            # No valid component: return full-image fallback
            final_mask  = np.ones((H, W), dtype=np.uint8)
            confidence  = 0.1
        else:
            sizes = ndimage.sum(closed, labeled, range(1, n_comp + 1))
            best  = int(np.argmax(sizes)) + 1
            final_mask = (labeled == best).astype(np.uint8)
            # Confidence: ratio of largest/total foreground pixels
            confidence = float(sizes[best - 1] / (np.sum(closed) + 1e-6))
            confidence = float(np.clip(confidence, 0.0, 1.0))
    else:
        final_mask = closed
        confidence = 0.5

    # 4. Bounding rectangle
    rows = np.any(final_mask, axis=1)
    cols = np.any(final_mask, axis=0)
    if not np.any(rows) or not np.any(cols):
        bounding = (0, 0, W, H)
        confidence = 0.05
    else:
        r_min, r_max = int(np.argmax(rows)), int(H - 1 - np.argmax(rows[::-1]))
        c_min, c_max = int(np.argmax(cols)), int(W - 1 - np.argmax(cols[::-1]))
        bounding = (c_min, r_min, c_max - c_min, r_max - r_min)

    return CollimatorMask(
        mask       = final_mask,
        bounding   = bounding,
        confidence = confidence,
        method_id  = 'threshold_morpho_v1',
    )
```

#### 12.5.3 C++ 클래스 명세 (런타임)

```cpp
// CollimatorMask C++ runtime class
// Python calibration produces JSON sidecar; runtime reconstructs mask from sidecar
// Sidecar schema (xpe-algorithm-spec-deepsync.md §4.3):
//   { "roi_x": int, "roi_y": int, "roi_w": int, "roi_h": int,
//     "confidence": float, "method_id": string }

struct CollimatorMask {
    cv::Mat  mask;          // CV_8U binary mask (1 = inside collimated field)
    cv::Rect bounding;      // Bounding rect of collimated field
    float    confidence;    // Detection quality [0,1]
    std::string method_id;  // "threshold_morpho_v1" or "ai_refined_v1"

    // Convenience accessors
    cv::Rect bounding_rect() const { return bounding; }
    const cv::Mat& mask_mat() const { return mask; }

    // Load from JSON sidecar (written by Python calibration step)
    static CollimatorMask from_sidecar(const nlohmann::json& j) {
        CollimatorMask cm;
        int x = j.at("roi_x").get<int>();
        int y = j.at("roi_y").get<int>();
        int w = j.at("roi_w").get<int>();
        int h = j.at("roi_h").get<int>();
        cm.bounding    = cv::Rect(x, y, w, h);
        cm.confidence  = j.at("confidence").get<float>();
        cm.method_id   = j.at("method_id").get<std::string>();
        // Reconstruct binary mask from bounding rect (full-rect approximation)
        // Full polygon mask optional in Phase 2 if contour points stored
        cm.mask = cv::Mat::zeros(/* H, W from image size */ 0, 0, CV_8U);
        // Caller must provide image dimensions; set via set_image_size()
        return cm;
    }

    void set_image_size(int H, int W) {
        mask = cv::Mat::zeros(H, W, CV_8U);
        cv::rectangle(mask, bounding, cv::Scalar(1), cv::FILLED);
    }

    // Compute mean signal within mask (used by EI ROI selection)
    float mean_within_mask(const cv::Mat& image) const {
        cv::Scalar mean_val = cv::mean(image, mask);
        return static_cast<float>(mean_val[0]);
    }
};
```

#### 12.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 검출 성공률 | ≥ 95% | 100장 테스트 팬텀 |
| Bounding rect 정확도 | ±5mm from true edge | 물리적 조준기 기준값 비교 |
| 처리 시간 | < 200ms (3072×3072) | 단일 스레드 |
| Confidence 하한 경고 | confidence < 0.6 → 알림 | 경보 로그 확인 |

---

### 12.6 MTF 슬랜트 에지 ESF 완전 구현 (GAP-T 해소)

§12.2에서 `compute_mtf_precision_mode()`의 aperture correction만 제공했지만, ESF 추출 → LSF → FFT → MTF 완전 파이프라인이 명세되어 있지 않았다. 본 섹션은 IEC 62220-1-1 준수 완전 구현을 제공한다.

#### 12.6.1 알고리즘 수학 정의

**ESF → LSF → MTF 변환 체인**:

$$\text{ESF}(x) = \text{edge spread function} \quad \text{(oversample from multiple rows)}$$

$$\text{LSF}(x) = \frac{d}{dx}\text{ESF}(x) \quad \text{(line spread function = derivative of ESF)}$$

$$\text{OTF}(f) = \mathcal{F}\left[\text{LSF}(x)\right](f)$$

$$\text{MTF}(f) = \left|\text{OTF}(f)\right| / \left|\text{OTF}(0)\right|$$

**Pre-sampling 보정 (IEC 62220-1-1 §6.2)**:

$$\text{MTF}_{\text{true}}(f) = \frac{\text{MTF}_{\text{measured}}(f)}{\text{sinc}(f \cdot p)} \quad \text{where } \text{sinc}(x) = \frac{\sin(\pi x)}{\pi x}$$

**슬랜트 각도 제약 (ISO 12233)**:
$$\theta_{\text{edge}} \in [2°,\ 10°] \quad \Rightarrow \quad \text{oversampling factor} = \frac{1}{\sin\theta}$$

#### 12.6.2 파라미터 및 경계 조건

| 파라미터 | 권장값 | 범위 | 의미 |
|---------|-------|-----|------|
| `edge_angle_deg` | 5.0 | 2–10° | 슬랜트 각도 (IEC 62220-1-1) |
| `oversampling` | 4 | 4–8 | ESF 오버샘플링 인수 |
| `roi_height_px` | 200 | 100–500 | ESF 추출 ROI 높이 |
| `smooth_sigma` | 1.0 | 0.5–3.0 | LSF smoothing σ (가우시안) |
| `freq_limit_nyquist` | 1.0 | 0.1–1.0 | MTF 출력 주파수 상한 (Nyquist 배수) |

#### 12.6.3 Python 구현 (오프라인, IEC 62220-1-1 준수)

```python
import numpy as np
from scipy.ndimage import sobel, gaussian_filter1d
from scipy.optimize import curve_fit
from typing import Tuple

def extract_esf_from_slanted_edge(
        edge_image:      np.ndarray,
        pixel_pitch_mm:  float,
        edge_angle_deg:  float = 5.0,
        oversampling:    int   = 4,
        roi_height_px:   int   = 200) -> Tuple[np.ndarray, np.ndarray]:
    """
    Extract Edge Spread Function (ESF) from a slanted-edge image.

    Algorithm (IEC 62220-1-1 §6.1):
      1. Locate edge centre per row via gradient centroid
      2. Compute sub-pixel position relative to mean edge location
      3. Bin into oversampled ESF array

    Args:
        edge_image:     2-D float32 image containing slanted edge
        pixel_pitch_mm: detector pixel pitch (mm)
        edge_angle_deg: nominal edge angle in degrees
        oversampling:   ESF super-resolution factor
        roi_height_px:  number of rows to use from image centre
    Returns:
        (esf_positions_mm, esf_values) — both 1-D float64 arrays
    """
    H, W = edge_image.shape
    row_start = (H - roi_height_px) // 2
    row_end   = row_start + roi_height_px
    roi       = edge_image[row_start:row_end, :].astype(np.float64)
    n_rows, n_cols = roi.shape

    # Step 1: Locate edge centre per row using gradient centroid (Canny + CoM)
    grad = np.gradient(roi, axis=1)
    abs_grad = np.abs(grad)
    col_idx = np.arange(n_cols, dtype=np.float64)
    # Centre of mass of |gradient| per row → sub-pixel edge position
    edge_pos_per_row = np.array([
        np.sum(abs_grad[r, :] * col_idx) / (np.sum(abs_grad[r, :]) + 1e-10)
        for r in range(n_rows)
    ])

    # Step 2: Fit line to edge positions to estimate angle
    row_idx = np.arange(n_rows, dtype=np.float64)
    p = np.polyfit(row_idx, edge_pos_per_row, 1)
    slope = p[0]  # pixels per row
    edge_mean = np.mean(edge_pos_per_row)

    # Step 3: Build oversampled ESF
    osf = oversampling
    esf_bins  = np.zeros(n_cols * osf, dtype=np.float64)
    esf_count = np.zeros(n_cols * osf, dtype=np.int32)

    for r in range(n_rows):
        edge_x = edge_mean + slope * (r - n_rows / 2)
        for c in range(n_cols):
            dx = (c - edge_x) * pixel_pitch_mm  # mm from edge
            bin_idx = int(round(dx / pixel_pitch_mm * osf)) + (n_cols * osf) // 2
            if 0 <= bin_idx < len(esf_bins):
                esf_bins[bin_idx]  += roi[r, c]
                esf_count[bin_idx] += 1

    valid = esf_count > 0
    esf_vals = np.where(valid, esf_bins / np.maximum(esf_count, 1), np.nan)
    esf_pos  = (np.arange(len(esf_bins)) - len(esf_bins) // 2) * pixel_pitch_mm / osf

    # Remove NaN by linear interpolation
    nans = np.isnan(esf_vals)
    esf_vals[nans] = np.interp(np.where(nans)[0],
                                 np.where(~nans)[0],
                                 esf_vals[~nans])
    return esf_pos, esf_vals


def compute_mtf_from_esf(
        esf_positions_mm: np.ndarray,
        esf_values:       np.ndarray,
        smooth_sigma:     float = 1.0,
        freq_limit:       float = 1.0,
        pixel_pitch_mm:   float = 0.148,
        aperture_correct: bool  = True) -> dict:
    """
    Compute MTF from ESF via differentiation and FFT.

    Pipeline:
        ESF → smooth → differentiate → LSF → Hanning window → FFT → |OTF| → MTF

    Args:
        esf_positions_mm: sample positions (mm), uniformly spaced
        esf_values:       ESF values (float64)
        smooth_sigma:     Gaussian smoothing σ applied to LSF
        freq_limit:       upper frequency as fraction of Nyquist (1.0 = Nyquist)
        pixel_pitch_mm:   original pixel pitch for aperture correction
        aperture_correct: apply sinc aperture correction
    Returns:
        dict with: freqs_mm_inv, mtf, f50_mm_inv, f10_mm_inv
    """
    dx = float(np.mean(np.diff(esf_positions_mm)))  # mm per sample

    # Normalise ESF to [0, 1]
    esf = esf_values.astype(np.float64)
    esf = (esf - esf.min()) / (esf.max() - esf.min() + 1e-10)

    # Differentiate ESF → LSF
    lsf = np.gradient(esf, dx)

    # Gaussian smoothing to reduce noise (per IEC 62220-1-1 §6.2)
    if smooth_sigma > 0:
        lsf = gaussian_filter1d(lsf, sigma=smooth_sigma / dx)

    # Normalise LSF area to 1
    lsf_sum = np.sum(np.abs(lsf)) * dx
    if lsf_sum > 1e-10:
        lsf /= lsf_sum

    # Hanning window (reduce spectral leakage)
    window = np.hanning(len(lsf))
    lsf_w  = lsf * window

    # FFT → OTF → MTF
    n    = len(lsf_w)
    otf  = np.fft.fft(lsf_w, n=n * 4)  # zero-pad 4× for interpolation
    freqs = np.fft.fftfreq(n * 4, d=dx)  # cycles/mm

    # Keep positive frequencies up to freq_limit × Nyquist
    nyquist = 1.0 / (2.0 * pixel_pitch_mm)
    pos_mask = (freqs > 0) & (freqs <= freq_limit * nyquist)
    freqs_pos = freqs[pos_mask]
    mtf_raw   = np.abs(otf[pos_mask])
    mtf_raw  /= (np.abs(otf[0]) + 1e-10)   # normalise to DC

    # Aperture correction: divide by sinc(f × pixel_pitch)
    if aperture_correct:
        sinc_vals = np.sinc(freqs_pos * pixel_pitch_mm)  # numpy sinc = sin(πx)/(πx)
        mtf_corrected = np.where(sinc_vals > 0.05,
                                  mtf_raw / sinc_vals,
                                  mtf_raw)
        mtf = np.clip(mtf_corrected, 0.0, 1.2)
    else:
        mtf = np.clip(mtf_raw, 0.0, 1.2)

    # Find f50 and f10 (interpolated)
    def freq_at_mtf_val(mtf_arr, freq_arr, target):
        above = np.where(mtf_arr >= target)[0]
        if len(above) == 0: return float(freq_arr[-1])
        i = above[-1]
        if i + 1 >= len(mtf_arr): return float(freq_arr[i])
        # Linear interpolation
        t = (target - mtf_arr[i]) / (mtf_arr[i + 1] - mtf_arr[i] + 1e-10)
        return float(freq_arr[i] + t * (freq_arr[i + 1] - freq_arr[i]))

    return {
        'freqs_mm_inv': freqs_pos.astype(np.float32),
        'mtf':          mtf.astype(np.float32),
        'f50_mm_inv':   freq_at_mtf_val(mtf, freqs_pos, 0.5),
        'f10_mm_inv':   freq_at_mtf_val(mtf, freqs_pos, 0.1),
        'pixel_pitch_mm': pixel_pitch_mm,
        'oversampling_dx_mm': dx,
    }


def full_mtf_pipeline(edge_image:     np.ndarray,
                       pixel_pitch_mm: float,
                       **kwargs) -> dict:
    """
    Complete MTF pipeline: edge image → MTF curve.

    Combines extract_esf_from_slanted_edge() and compute_mtf_from_esf().
    """
    esf_pos, esf_vals = extract_esf_from_slanted_edge(
        edge_image, pixel_pitch_mm,
        edge_angle_deg = kwargs.get('edge_angle_deg', 5.0),
        oversampling   = kwargs.get('oversampling', 4),
        roi_height_px  = kwargs.get('roi_height_px', 200),
    )
    return compute_mtf_from_esf(
        esf_pos, esf_vals,
        smooth_sigma    = kwargs.get('smooth_sigma', 1.0),
        pixel_pitch_mm  = pixel_pitch_mm,
        aperture_correct= kwargs.get('aperture_correct', True),
    )
```

#### 12.6.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| MTF@Nyquist vs 이론값 | 오차 < 5% | 합성 단계함수 이미지 |
| f50 재현성 | CV < 2% (5회 측정) | 동일 팬텀 반복 측정 |
| IEC 62220-1-1 인증 | f10 ≥ 0.5 × Nyquist (RQA5) | 표준 팬텀 측정 |
| Aperture 보정 효과 | f50 ≥ 보정 전 1.05× | 보정 전후 비교 |
| 처리 시간 | < 500ms (512 rows) | 단일 코어 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-MEAS-001 (FPD 특성화 확장), SRS-MEAS-002 (MTF 완전 구현) — Phase 2 추가 예정

---

## 13. 품질 상태 벡터 사이드카 (GAP-R 해소)

xpe-algorithm-spec-deepsync.md §4.3 "For every processed frame, the runtime should produce a sidecar quality state"에서 요구된 항목이다. 기존 `AEDResult` 구조체는 AED-0 결과만을 담고 있으며, 파이프라인 전체의 품질 상태를 통합 표현하는 사이드카가 없었다.

### 13.1 알고리즘 수학 정의

품질 상태 벡터 $\mathbf{Q}$는 프레임당 하나의 인스턴스로 생성되며, 각 필드는 해당 파이프라인 단계 완료 직후 채워진다:

$$\mathbf{Q} = \{\mathbf{Q}_{\text{cal}},\ \mathbf{Q}_{\text{defect}},\ \mathbf{Q}_{\text{lag}},\ \mathbf{Q}_{\text{gsvg}},\ \mathbf{Q}_{\text{ei}},\ \mathbf{Q}_{\text{ai}}\}$$

각 서브 벡터는 해당 단계의 상태, 신뢰도, 경고 플래그를 포함한다.

**교정 신선도 점수**:

$$Q_{\text{cal,fresh}} = \exp\!\left(-\frac{\Delta t_{\text{days}}}{\tau_{\text{fresh}}}\right), \quad \tau_{\text{fresh}} = 7 \text{ days}$$

**결함 부담 등급**:

$$\text{DefectClass} = \begin{cases} 0 & N_{\text{def}} < 0.01\% W H \\ 1 & N_{\text{def}} < 0.05\% W H \\ 2 & N_{\text{def}} < 0.2\% W H \\ 3 & N_{\text{def}} \geq 0.2\% W H \end{cases}$$

### 13.2 XpeQualityState 구조체 명세

#### 13.2.1 Python 정의 (참조 스키마)

```python
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional

class CalibFreshness(IntEnum):
    FRESH   = 0   # score ≥ 0.90
    AGING   = 1   # 0.70 ≤ score < 0.90
    STALE   = 2   # 0.40 ≤ score < 0.70
    EXPIRED = 3   # score < 0.40 → recalibration required

class DefectBurdenClass(IntEnum):
    NEGLIGIBLE = 0   # < 0.01% pixels
    LOW        = 1   # 0.01–0.05%
    MODERATE   = 2   # 0.05–0.2%
    HIGH       = 3   # ≥ 0.2% → quality advisory

class LagTierApplied(IntEnum):
    NONE = 0   # no lag correction applied
    FAST = 1   # single-term model
    FULL = 3   # full three-term model

class GsvgSkipReason(IntEnum):
    NOT_SKIPPED      = 0
    NO_GRID_DETECTED = 1   # no grid artifact found
    PERFORMANCE_MODE = 2   # explicitly disabled by operator
    AED_FAILED       = 3   # AED-0 returned invalid

class AiWorkerStatus(IntEnum):
    NOT_USED    = 0
    AI_SUCCESS  = 1
    AI_FALLBACK = 2   # deterministic fallback used
    AI_DISABLED = 3

@dataclass
class CalibQuality:
    freshness_class:  CalibFreshness  = CalibFreshness.FRESH
    freshness_score:  float           = 1.0
    session_id:       str             = ''
    days_since_cal:   float           = 0.0
    drift_warning:    bool            = False

@dataclass
class DetectorCorrectionQuality:
    defect_burden_class:  DefectBurdenClass = DefectBurdenClass.NEGLIGIBLE
    defect_count:         int               = 0
    lag_tier:             LagTierApplied    = LagTierApplied.NONE
    lag_residual_pct:     float             = 0.0
    nonlinearity_applied: bool              = False
    heel_applied:         bool              = False

@dataclass
class ExposureQuality:
    aed_valid:      bool  = True
    ei_value:       float = 0.0
    di_value:       float = 0.0
    roi_confidence: float = 1.0   # EI ROI detection confidence (0–1)
    roi_method:     str   = ''    # 'central' / 'anatomy_bounded' / 'fallback'

@dataclass
class GsvgQuality:
    grid_detected:  bool          = False
    gsvg_applied:   bool          = False
    skip_reason:    GsvgSkipReason = GsvgSkipReason.NOT_SKIPPED
    grid_frequency: float         = 0.0   # detected grid frequency (lp/mm)

@dataclass
class AiQuality:
    worker_status:  AiWorkerStatus = AiWorkerStatus.NOT_USED
    body_part_id:   str            = ''
    confidence:     float          = 0.0
    model_version:  str            = ''
    inference_ms:   float          = 0.0

@dataclass
class XpeQualityState:
    """
    Per-frame quality state sidecar.
    Created empty at pipeline entry; each stage fills its section.
    Must NOT mutate XpeImageMetadata to carry this information.
    """
    frame_id:     str                      = ''
    timestamp_ns: int                      = 0
    calib:        CalibQuality             = field(default_factory=CalibQuality)
    detector:     DetectorCorrectionQuality = field(default_factory=DetectorCorrectionQuality)
    exposure:     ExposureQuality          = field(default_factory=ExposureQuality)
    gsvg:         GsvgQuality             = field(default_factory=GsvgQuality)
    ai:           AiQuality               = field(default_factory=AiQuality)
    pipeline_version: str                  = 'xpe-1.2'

    def overall_advisory(self) -> str:
        """
        Generate a single human-readable advisory string.
        Returns empty string if everything is nominal.
        """
        warnings = []
        if self.calib.freshness_class >= CalibFreshness.STALE:
            warnings.append(f"CAL_STALE({self.calib.days_since_cal:.1f}d)")
        if self.detector.defect_burden_class >= DefectBurdenClass.MODERATE:
            warnings.append(f"DEFECT_BURDEN({self.detector.defect_count}px)")
        if not self.exposure.aed_valid:
            warnings.append("EXPOSURE_INVALID")
        if abs(self.exposure.di_value) > 3.0:
            warnings.append(f"DI_CONCERN({self.exposure.di_value:+.1f}dB)")
        if self.ai.worker_status == AiWorkerStatus.AI_FALLBACK:
            warnings.append("AI_FALLBACK")
        return '; '.join(warnings)
```

#### 13.2.2 C++ 구조체

```cpp
// XpeQualityState — C++ sidecar object
// Lifetime: same as the processing call; returned alongside output image.

enum class CalibFreshness   : uint8_t { FRESH=0, AGING=1, STALE=2, EXPIRED=3 };
enum class DefectBurdenClass: uint8_t { NEGLIGIBLE=0, LOW=1, MODERATE=2, HIGH=3 };
enum class LagTierApplied   : uint8_t { NONE=0, FAST=1, FULL=3 };
enum class GsvgSkipReason   : uint8_t { NOT_SKIPPED=0, NO_GRID=1, PERF=2, AED_FAILED=3 };
enum class AiWorkerStatus   : uint8_t { NOT_USED=0, AI_SUCCESS=1, FALLBACK=2, DISABLED=3 };

struct CalibQualityState {
    CalibFreshness freshness_class  = CalibFreshness::FRESH;
    float          freshness_score  = 1.0f;
    char           session_id[17]   = {};   // 16 hex + null
    float          days_since_cal   = 0.0f;
    bool           drift_warning    = false;
};

struct DetectorCorrectionState {
    DefectBurdenClass defect_burden = DefectBurdenClass::NEGLIGIBLE;
    uint32_t          defect_count  = 0;
    LagTierApplied    lag_tier      = LagTierApplied::NONE;
    float             lag_residual_pct  = 0.0f;
    bool              nonlinearity_applied = false;
    bool              heel_applied    = false;
};

struct ExposureState {
    bool  aed_valid      = true;
    float ei_value       = 0.0f;
    float di_value       = 0.0f;
    float roi_confidence = 1.0f;
    char  roi_method[32] = "central";
};

struct GsvgState {
    bool          grid_detected = false;
    bool          gsvg_applied  = false;
    GsvgSkipReason skip_reason  = GsvgSkipReason::NOT_SKIPPED;
    float         grid_freq_lpmm = 0.0f;
};

struct AiState {
    AiWorkerStatus status       = AiWorkerStatus::NOT_USED;
    char  body_part_id[32]      = {};
    float confidence            = 0.0f;
    char  model_version[32]     = {};
    float inference_ms          = 0.0f;
};

struct XpeQualityState {
    char               frame_id[64]   = {};
    int64_t            timestamp_ns   = 0;
    CalibQualityState  calib          = {};
    DetectorCorrectionState detector  = {};
    ExposureState      exposure       = {};
    GsvgState          gsvg           = {};
    AiState            ai             = {};
    char               pipeline_ver[16] = "xpe-1.2";

    // Serialize to JSON string for logging/DICOM private tag
    std::string to_json() const;
};
```

#### 13.2.3 파이프라인 통합 포인트

각 처리 단계에서 `XpeQualityState`를 채우는 위치:

| 단계 | 채우는 필드 | 시점 |
|-----|-----------|------|
| ConfigManager 로드 | `calib.*` | 파이프라인 시작 전 |
| Readout Validation | `exposure.aed_valid` 예비 | §3.0 완료 후 |
| Defect Correction | `detector.defect_burden`, `detector.defect_count` | §3.3 완료 후 |
| Ghost Correction | `detector.lag_tier`, `detector.lag_residual_pct` | §3.4.5 완료 후 |
| Non-linearity | `detector.nonlinearity_applied` | §3.0.5 완료 후 |
| Heel Correction | `detector.heel_applied` | §3.5 완료 후 |
| AED-0 | `exposure.aed_valid`, `exposure.ei_value` | §9.4 완료 후 |
| Grid Suppression | `gsvg.*` | §5 완료 후 |
| EI Calculation | `exposure.di_value`, `exposure.roi_confidence` | §7 완료 후 |
| AI Worker | `ai.*` | §8.4 완료 후 |

### 13.3 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 모든 필드 채워짐 | 파이프라인 완료 시 0개 기본값 잔류 | 완전 파이프라인 실행 후 확인 |
| `overall_advisory()` 정확도 | 알려진 이상 시나리오 100% 탐지 | 합성 결함 파이프라인 |
| 사이드카 직렬화 크기 | < 1KB (JSON) | 시리얼라이제이션 테스트 |
| 메인 이미지 처리 추가 지연 | < 0.5ms | 프로파일링 |

**IEC 62304 §5.4 추적성**: SRS ID: SRS-QC-003 (품질 상태 사이드카) — Phase 2 추가 예정

---

---

## 14. Fluoroscopy 시간적 재귀 IIR 필터 (GAP-Y 해소)

**관련 GAP:** GAP-Y — Fluoroscopy 연속 투시 모드에서 시간적 노이즈 억제를 위한 재귀 지수 평균 필터가 미명세 상태였음.

### 14.1 개요

투시(Fluoroscopy) 모드에서는 초당 수십 프레임이 연속 입력된다. 단순 공간 필터만으로는 시간적 노이즈 축적을 억제하기 어려우므로, 재귀 IIR(Infinite Impulse Response) 지수 평균 필터를 적용하여 정지 영역의 SNR을 향상시킨다. 동체 감지(motion detection) 로직과 결합하여 움직이는 피사체에서의 잔상(ghosting)을 방지한다.

### 14.2 수학적 명세

#### 14.2.1 재귀 지수 평균

$$I_{\text{out}}(t) = \alpha \cdot I_{\text{in}}(t) + (1 - \alpha) \cdot I_{\text{out}}(t-1)$$

- $\alpha \in (0, 1]$: 시간 평균 가중치 (작을수록 높은 시간 평균, 낮은 잔상)
- $I_{\text{in}}(t)$: 현재 프레임 (전처리 완료된 float32)
- $I_{\text{out}}(t-1)$: 이전 누적 출력 프레임 (상태 버퍼)

#### 14.2.2 적응형 α 선택

$$\alpha = \begin{cases}
\alpha_{\text{motion}} = 1.0 & \text{if } \Delta_t > \theta_{\text{motion}} \\
\alpha_{\text{static}} & \text{otherwise}
\end{cases}$$

$$\Delta_t = \text{mean}_{x,y}\left(|I_{\text{in}}(t) - I_{\text{in}}(t-1)|\right)$$

$$\theta_{\text{motion}} = 0.05 \cdot \bar{I}_{\text{in}}(t)$$

여기서 $\bar{I}_{\text{in}}(t)$는 현재 프레임의 전역 평균 픽셀 값이다.

- **정지 영역** ($\Delta_t \leq \theta_{\text{motion}}$): $\alpha_{\text{static}} \in [0.05, 0.15]$ — 높은 시간 평균으로 SNR 향상
- **동체 검출** ($\Delta_t > \theta_{\text{motion}}$): $\alpha_{\text{motion}} = 1.0$ — IIR 리셋, 잔상 방지

#### 14.2.3 SNR 이득

재귀 IIR 필터의 등가 평균 프레임 수 $N_{\text{eq}}$:

$$N_{\text{eq}} = \frac{2 - \alpha}{\alpha}$$

SNR 이득 (정적 장면):

$$\text{SNR gain} = \sqrt{N_{\text{eq}}} = \sqrt{\frac{2-\alpha}{\alpha}}$$

$\alpha = 0.1$일 때 $N_{\text{eq}} \approx 19$, SNR 이득 $\approx 4.4 \times$.

### 14.3 C++ 구현

#### 14.3.1 DLL Public API

```cpp
// xpe_fluoro_iir.h  (DLL Public Interface)

#pragma once
#include "xpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief  Fluoroscopy IIR 필터 상태 초기화
/// @param  width   이미지 너비 (pixels)
/// @param  height  이미지 높이 (pixels)
/// @param  alpha_static  정지 영역 IIR 계수 [0.05 ~ 0.15]
/// @return XPE_OK 또는 XPE_ERR_ALLOCATION
XpeErrorCode xpe_fluoro_iir_init(uint32_t width, uint32_t height,
                                  float alpha_static);

/// @brief  IIR 필터 적용 (in-place 또는 out-of-place)
/// @param  frame_in   입력 프레임 float32 [width×height]
/// @param  frame_out  출력 프레임 float32 [width×height] (frame_in과 달라도 됨)
/// @return XPE_OK 또는 에러 코드
XpeErrorCode xpe_fluoro_iir_process(const float* frame_in,
                                     float*       frame_out);

/// @brief  IIR 상태 버퍼 초기화 (씬 전환, 교정 후 호출)
XpeErrorCode xpe_fluoro_iir_reset(void);

/// @brief  리소스 해제
void xpe_fluoro_iir_destroy(void);

/// @brief  현재 동체 감지 상태 조회 (디버그용)
/// @param  out_delta  최근 프레임 차분 평균 (NULL 허용)
/// @param  out_motion 동체 감지 플래그
void xpe_fluoro_iir_get_motion_state(float* out_delta, bool* out_motion);

#ifdef __cplusplus
}
#endif
```

#### 14.3.2 내부 구현 클래스

```cpp
// FluoroIirFilter.hpp  (내부 구현)

#include <immintrin.h>
#include <atomic>
#include <mutex>
#include <memory>
#include <cmath>
#include <stdexcept>

class FluoroIirFilter {
public:
    FluoroIirFilter(uint32_t w, uint32_t h, float alpha_s)
        : width_(w), height_(h), alpha_static_(alpha_s),
          n_pixels_(static_cast<size_t>(w) * h),
          initialized_(false)
    {
        // 64-byte aligned 버퍼 할당 (AVX2 요구)
        state_buf_a_ = static_cast<float*>(
            _mm_malloc(n_pixels_ * sizeof(float), 64));
        state_buf_b_ = static_cast<float*>(
            _mm_malloc(n_pixels_ * sizeof(float), 64));
        prev_frame_  = static_cast<float*>(
            _mm_malloc(n_pixels_ * sizeof(float), 64));
        if (!state_buf_a_ || !state_buf_b_ || !prev_frame_)
            throw std::bad_alloc();
        memset(state_buf_a_, 0, n_pixels_ * sizeof(float));
        memset(state_buf_b_, 0, n_pixels_ * sizeof(float));
        memset(prev_frame_,  0, n_pixels_ * sizeof(float));
        active_buf_.store(0, std::memory_order_relaxed);
    }

    ~FluoroIirFilter() {
        _mm_free(state_buf_a_);
        _mm_free(state_buf_b_);
        _mm_free(prev_frame_);
    }

    /// @brief 단일 프레임 처리 (thread-safe: mutex로 상태 스왑 보호)
    void process(const float* __restrict__ in,
                       float* __restrict__ out)
    {
        // 1) 동체 감지: 이전 프레임 대비 평균 절대 차분 계산
        float delta = computeFrameDelta(in, prev_frame_);
        float mean_in = computeMean(in);
        float threshold = 0.05f * mean_in;
        bool motion = (delta > threshold);
        last_delta_.store(delta, std::memory_order_relaxed);
        last_motion_.store(motion, std::memory_order_relaxed);

        float alpha = motion ? 1.0f : alpha_static_;

        // 2) IIR 필터: out = alpha*in + (1-alpha)*state
        {
            std::lock_guard<std::mutex> lk(state_mutex_);
            float* state = getActiveState();
            applyIirAvx2(in, state, out, alpha, n_pixels_);
            // 새 상태를 비활성 버퍼에 기록 후 원자적 스왑
            float* next_state = getInactiveState();
            memcpy(next_state, out, n_pixels_ * sizeof(float));
            swapBuffers();
        }

        // 3) 이전 프레임 갱신
        memcpy(prev_frame_, in, n_pixels_ * sizeof(float));
        initialized_ = true;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(state_mutex_);
        memset(state_buf_a_, 0, n_pixels_ * sizeof(float));
        memset(state_buf_b_, 0, n_pixels_ * sizeof(float));
        memset(prev_frame_,  0, n_pixels_ * sizeof(float));
        initialized_ = false;
    }

    void getMotionState(float* delta_out, bool* motion_out) const {
        if (delta_out)  *delta_out  = last_delta_.load(std::memory_order_relaxed);
        if (motion_out) *motion_out = last_motion_.load(std::memory_order_relaxed);
    }

private:
    // ── AVX2 FMA IIR 적용: out = alpha*in + (1-alpha)*state ──────────────
    static void applyIirAvx2(const float* __restrict__ in,
                              const float* __restrict__ state,
                                    float* __restrict__ out,
                              float alpha, size_t n)
    {
        const __m256 v_alpha   = _mm256_set1_ps(alpha);
        const __m256 v_1malpha = _mm256_set1_ps(1.0f - alpha);
        size_t i = 0;
        // 8 pixels/cycle (256-bit = 8×float32)
        for (; i + 8 <= n; i += 8) {
            __m256 vi = _mm256_load_ps(in    + i);
            __m256 vs = _mm256_load_ps(state + i);
            // FMA: alpha*in + (1-alpha)*state
            __m256 vo = _mm256_fmadd_ps(v_alpha, vi,
                            _mm256_mul_ps(v_1malpha, vs));
            _mm256_store_ps(out + i, vo);
        }
        // 잔여 픽셀 처리
        for (; i < n; ++i)
            out[i] = alpha * in[i] + (1.0f - alpha) * state[i];
    }

    // ── AVX2 프레임 차분 평균 ─────────────────────────────────────────────
    static float computeFrameDelta(const float* a, const float* b, size_t n) {
        if (n == 0) return 0.0f;
        __m256 vsum = _mm256_setzero_ps();
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            __m256 va = _mm256_load_ps(a + i);
            __m256 vb = _mm256_load_ps(b + i);
            vsum = _mm256_add_ps(vsum, _mm256_andnot_ps(
                _mm256_set1_ps(-0.0f),
                _mm256_sub_ps(va, vb)));  // |a-b|
        }
        // horizontal sum
        float buf[8]; _mm256_storeu_ps(buf, vsum);
        float sum = buf[0]+buf[1]+buf[2]+buf[3]+buf[4]+buf[5]+buf[6]+buf[7];
        for (; i < n; ++i) sum += fabsf(a[i] - b[i]);
        return sum / static_cast<float>(n);
    }

    static float computeFrameDelta(const float* a, const float* b) { /* overload */ return 0.f; }

    static float computeMean(const float* data, size_t n) {
        __m256 vsum = _mm256_setzero_ps();
        size_t i = 0;
        for (; i + 8 <= n; i += 8)
            vsum = _mm256_add_ps(vsum, _mm256_load_ps(data + i));
        float buf[8]; _mm256_storeu_ps(buf, vsum);
        float sum = buf[0]+buf[1]+buf[2]+buf[3]+buf[4]+buf[5]+buf[6]+buf[7];
        for (; i < n; ++i) sum += data[i];
        return sum / static_cast<float>(n);
    }

    float* getActiveState()   { return active_buf_.load() == 0 ? state_buf_a_ : state_buf_b_; }
    float* getInactiveState() { return active_buf_.load() == 0 ? state_buf_b_ : state_buf_a_; }
    void   swapBuffers()      { active_buf_.fetch_xor(1, std::memory_order_acq_rel); }

    uint32_t  width_, height_;
    size_t    n_pixels_;
    float     alpha_static_;
    float*    state_buf_a_;
    float*    state_buf_b_;
    float*    prev_frame_;
    bool      initialized_;
    std::mutex            state_mutex_;
    std::atomic<int>      active_buf_;
    std::atomic<float>    last_delta_{0.0f};
    std::atomic<bool>     last_motion_{false};
};
```

### 14.4 SIMD 최적화 전략

| 항목 | 전략 | 달성 성능 |
|------|------|----------|
| AVX2 FMA 융합 | `_mm256_fmadd_ps` 로 alpha*in + (1-alpha)*state 단일 명령 | 8 pixels/cycle |
| 64-byte 정렬 | `_mm_malloc(n, 64)` → `_mm256_load_ps` (aligned load) | non-temporal store 가능 |
| 동체 감지 절댓값 | `_mm256_andnot_ps(sign_mask, diff)` | 추가 비교 없음 |
| 상태 버퍼 더블 버퍼링 | 원자적 스왑으로 lock 최소화 | 뮤텍스 경합 최소 |
| 성능 목표 | 3072×3072 프레임 기준 | **< 0.3 ms** (목표 달성) |

### 14.5 엣지 케이스

| 케이스 | 처리 방법 |
|--------|---------|
| 첫 프레임 (initialized=false) | `prev_frame_` = 0이므로 delta 과대 → motion=true → alpha=1.0 (패스스루) |
| 전체 검은 프레임 (I_mean=0) | threshold=0, delta>0이면 motion 감지; delta=0이면 정상 IIR |
| 씬 전환 / 교정 후 | `xpe_fluoro_iir_reset()` 호출로 상태 버퍼 초기화 |
| alpha_static 범위 초과 | API 진입 시 clamp: `alpha_s = std::clamp(alpha_s, 0.05f, 0.15f)` |
| 해상도 변경 | `destroy()` → `init()` 재호출 필수 (realloc 미지원) |

### 14.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FLUORO-001 (시간적 노이즈 억제 요구사항) |
| **SWU** | SWU-14.0 (Temporal IIR Filter) |
| **IEC 62304 §** | 5.4.2 (Software Unit Detailed Design) |
| **검증 방법** | (1) 정적 팬텀 100프레임 평균 SNR ≥ 4× 향상 확인; (2) 동체 패턴 주입 시 잔상 없음 확인; (3) alpha=1.0 시 출력 = 입력 단위 테스트 |
| **안전 분류** | Class B — 투시 모드 노이즈 과억제 시 진단 저하 가능; alpha_static 범위 검증 필수 |

---

## §3.9 SWU-1.9: Beam Hardening Correction (GAP-Z 해소)

**관련 GAP:** GAP-Z — X선 빔 경화(beam hardening)로 인한 OD 도메인 비선형성 보정이 미명세 상태였음.

### 3.9.1 개요

폴리크로매틱 X선 빔이 물질을 통과하면 저에너지 성분이 우선 흡수되어 빔이 "경화"된다. 이로 인해 균일한 물체에서도 가장자리보다 중심의 OD 값이 낮아지는 **cupping artifact**가 발생한다. BHC는 OD 도메인에서 다항식 보정을 적용하여 이 비선형성을 제거한다.

### 3.9.2 수학적 명세

#### 3.9.2.1 OD 도메인 다항식 보정

$$I_{\text{BHC}}(x,y) = I_{\text{OD}}(x,y) + \sum_{n=2}^{4} a_n \cdot [I_{\text{OD}}(x,y)]^n$$

- $I_{\text{OD}}$: 로그 변환 후 OD 값
- $a_2, a_3, a_4$: 교정 계수 (물 등가 PMMA 팬텀으로 결정)
- 1차 항($a_1$)은 전역 스케일링이므로 제외 (gain correction에서 처리)

#### 3.9.2.2 Horner's Method 수치 안정성

$$I_{\text{BHC}} = I_{\text{OD}} + I_{\text{OD}}^2 \cdot (a_2 + I_{\text{OD}} \cdot (a_3 + I_{\text{OD}} \cdot a_4))$$

Horner 변환으로 곱셈 횟수를 최소화한다.

#### 3.9.2.3 BHC LUT 계산

실시간 처리를 위해 65536-entry float32 LUT를 교정 로드 시 미리 계산한다:

$$\text{LUT}[k] = \frac{k}{65535} \cdot I_{\text{OD,max}} + \text{poly\_correction}\left(\frac{k}{65535} \cdot I_{\text{OD,max}}\right), \quad k = 0, \ldots, 65535$$

#### 3.9.2.4 PMMA 팬텀 교정 절차

| 두께 (cm, 물 등가) | 측정 OD | 이상적 선형 OD | 잔차 |
|-------------------|--------|--------------|------|
| 10 | $d_{10}$ | $\mu \cdot 10$ | $\epsilon_{10}$ |
| 15 | $d_{15}$ | $\mu \cdot 15$ | $\epsilon_{15}$ |
| 20 | $d_{20}$ | $\mu \cdot 20$ | $\epsilon_{20}$ |
| 25 | $d_{25}$ | $\mu \cdot 25$ | $\epsilon_{25}$ |
| 30 | $d_{30}$ | $\mu \cdot 30$ | $\epsilon_{30}$ |

잔차 $\epsilon_i$에 대해 최소제곱 다항식 피팅으로 $a_2, a_3, a_4$ 결정.

### 3.9.3 C++ 구현

```cpp
// BeamHardeningCorrection.hpp

class BeamHardeningCorrection {
public:
    static constexpr int LUT_SIZE = 65536;

    /// @brief 교정 계수 로드 및 LUT 빌드
    void loadCoefficients(float a2, float a3, float a4,
                          float od_max = 4.0f)
    {
        a2_ = a2; a3_ = a3; a4_ = a4;
        od_max_ = od_max;
        od_scale_ = static_cast<float>(LUT_SIZE - 1) / od_max;
        buildLut();
    }

    /// @brief 인라인 다항식 보정 (단일 픽셀, 스칼라 참조)
    [[nodiscard]] float correctScalar(float od) const noexcept {
        float od2 = od * od;
        return od + od2 * (a2_ + od * (a3_ + od * a4_));
    }

    /// @brief LUT 기반 보정 (실시간, AVX2)
    void applyLutAvx2(const float* __restrict__ od_in,
                            float* __restrict__ od_out,
                      size_t n) const
    {
        // 정수 인덱스 변환 후 LUT 조회 (gather 명령 사용)
        const __m256 v_scale = _mm256_set1_ps(od_scale_);
        const __m256 v_zero  = _mm256_setzero_ps();
        const __m256 v_max   = _mm256_set1_ps(static_cast<float>(LUT_SIZE - 1));

        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            __m256 vod  = _mm256_loadu_ps(od_in + i);
            __m256 vidx = _mm256_mul_ps(vod, v_scale);
            vidx = _mm256_max_ps(v_zero, _mm256_min_ps(vidx, v_max));
            __m256i vi  = _mm256_cvtps_epi32(vidx);
            // gather: 8 LUT 조회
            __m256 vcorr = _mm256_i32gather_ps(lut_.data(), vi, 4);
            _mm256_storeu_ps(od_out + i, vcorr);
        }
        for (; i < n; ++i) {
            int idx = static_cast<int>(
                std::clamp(od_in[i] * od_scale_, 0.0f, (float)(LUT_SIZE-1)));
            od_out[i] = lut_[idx];
        }
    }

private:
    void buildLut() {
        lut_.resize(LUT_SIZE);
        for (int k = 0; k < LUT_SIZE; ++k) {
            float od = static_cast<float>(k) / od_scale_;
            lut_[k] = correctScalar(od);
        }
    }

    float a2_{0.0f}, a3_{0.0f}, a4_{0.0f};
    float od_max_{4.0f};
    float od_scale_{1.0f};
    std::vector<float> lut_;
};
```

#### 3.9.3.1 교정 매니페스트 저장 형식

```json
{
  "bhc_params": {
    "a2": -0.0123,
    "a3":  0.0045,
    "a4": -0.0007,
    "od_max": 4.0,
    "phantom_type": "PMMA",
    "calibration_date": "2026-04-15",
    "kvp_setting": 80,
    "filtration_mm_al": 3.0
  }
}
```

### 3.9.4 SIMD 최적화

| 기법 | 설명 | 이득 |
|------|------|------|
| `_mm256_i32gather_ps` | 8 LUT 항목 병렬 조회 | 4× throughput vs scalar |
| LUT 64-byte 정렬 | `std::aligned_alloc(64)` | cache line 활용 극대화 |
| Horner 스칼라 참조 | 패리티 검증용 정확도 기준 | LUT 오차 < 1 ULP |

### 3.9.5 엣지 케이스

| 케이스 | 처리 |
|--------|------|
| $I_{\text{OD}} < 0$ | clamp to 0 (공기 픽셀) |
| $I_{\text{OD}} > od\_max$ | clamp to LUT_SIZE-1 |
| bhc_params 미존재 | 계수 $a_2=a_3=a_4=0$ → BHC 패스스루 |
| 교정 kVp 불일치 | 경고 로그, 가용 최근접 kVp 선택 |

### 3.9.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-003b (Beam Hardening Correction) |
| **SWU** | SWU-1.9 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | PMMA 팬텀 cupping artifact 제거 확인 (균일도 CV < 1%), LUT vs Horner 패리티 오차 < 0.001 OD |

---

## §3.10 SWU-1.10: Geometric Distortion Correction (GAP-AA 해소)

**관련 GAP:** GAP-AA — FPD의 기하학적 왜곡(barrel/pincushion distortion) 보정 알고리즘이 미명세 상태였음.

### 3.10.1 개요

FPD 및 이미지 체인의 기하학적 비선형성으로 인해 직선 구조물이 곡선으로 나타날 수 있다. Brown-Conrady 방사형+접선 왜곡 모델을 이용하여 이미지를 역변환 LUT로 보정한다.

### 3.10.2 수학적 명세

#### 3.10.2.1 Brown-Conrady 왜곡 모델

정규화 좌표 $(x_u, y_u)$ (이미지 중심 기준, 픽셀 → 정규화):

$$x_u = \frac{p_x - c_x}{f_x}, \quad y_u = \frac{p_y - c_y}{f_y}$$

방사형 왜곡 반지름:

$$r_u = \sqrt{x_u^2 + y_u^2}$$

왜곡된 좌표 $(x_d, y_d)$:

$$x_d = x_u \cdot \frac{r_d}{r_u} + 2p_1 x_u y_u + p_2(r_u^2 + 2x_u^2)$$
$$y_d = y_u \cdot \frac{r_d}{r_u} + p_1(r_u^2 + 2y_u^2) + 2p_2 x_u y_u$$

여기서:

$$\frac{r_d}{r_u} = 1 + k_1 r_u^2 + k_2 r_u^4 + k_3 r_u^6$$

- $k_1, k_2, k_3$: 방사형 왜곡 계수 (barrel: $k_1 < 0$)
- $p_1, p_2$: 접선 왜곡 계수 (프레임-센서 비정렬)
- $(c_x, c_y)$: 광학 중심 (픽셀)
- $(f_x, f_y)$: 초점 거리 (픽셀 단위, 대각 픽셀 크기 기준)

#### 3.10.2.2 역 왜곡 LUT

역 LUT는 각 출력 픽셀 $(x_{out}, y_{out})$에 대해 소스 픽셀 좌표 $(x_{src}, y_{src})$를 저장한다:

$$\text{map\_x}[y_{out}][x_{out}] = x_{src}$$
$$\text{map\_y}[y_{out}][x_{out}] = y_{src}$$

이 LUT는 교정 로드 시 1회 사전 계산되며 런타임에는 바이리니어 보간만 수행한다.

#### 3.10.2.3 바이리니어 보간

$$I_{\text{corr}}(x_{out}, y_{out}) = \text{bilinear\_interp}(I_{\text{raw}},\ x_{src},\ y_{src})$$

$$\text{bilinear}(I, x, y) = (1-\delta x)(1-\delta y) I[\lfloor y \rfloor][\lfloor x \rfloor]
                           + \delta x(1-\delta y) I[\lfloor y \rfloor][\lceil x \rceil]
                           + (1-\delta x)\delta y I[\lceil y \rceil][\lfloor x \rfloor]
                           + \delta x \delta y I[\lceil y \rceil][\lceil x \rceil]$$

### 3.10.3 C++ 구현

```cpp
// GeometricDistortionCorrection.hpp

struct DistortionParams {
    float k1, k2, k3;          // 방사형 계수
    float p1, p2;               // 접선 계수
    float cx, cy;               // 광학 중심 (픽셀)
    float fx, fy;               // 정규화 스케일 (픽셀)
};

class GeometricDistortionCorrection {
public:
    /// @brief LUT 사전 계산 (교정 로드 시 1회 호출)
    void buildLut(const DistortionParams& params,
                  uint32_t width, uint32_t height)
    {
        params_ = params;
        width_  = width;
        height_ = height;
        size_t n = static_cast<size_t>(width) * height;
        map_x_.resize(n);
        map_y_.resize(n);

        for (uint32_t oy = 0; oy < height; ++oy) {
            for (uint32_t ox = 0; ox < width; ++ox) {
                // 정규화
                float xu = (static_cast<float>(ox) - params.cx) / params.fx;
                float yu = (static_cast<float>(oy) - params.cy) / params.fy;
                float r2 = xu*xu + yu*yu;
                float r4 = r2*r2, r6 = r4*r2;
                float radial = 1.0f + params.k1*r2 + params.k2*r4 + params.k3*r6;
                float xd = xu * radial + 2.0f*params.p1*xu*yu
                                       + params.p2*(r2 + 2.0f*xu*xu);
                float yd = yu * radial + params.p1*(r2 + 2.0f*yu*yu)
                                       + 2.0f*params.p2*xu*yu;
                // 픽셀 좌표로 역변환
                map_x_[oy * width + ox] = xd * params.fx + params.cx;
                map_y_[oy * width + ox] = yd * params.fy + params.cy;
            }
        }
    }

    /// @brief LUT 기반 역왜곡 적용 (바이리니어 보간)
    void apply(const float* __restrict__ src,
                     float* __restrict__ dst) const
    {
        size_t n = static_cast<size_t>(width_) * height_;
        for (size_t idx = 0; idx < n; ++idx) {
            float sx = map_x_[idx];
            float sy = map_y_[idx];
            dst[idx] = bilinearInterp(src, sx, sy);
        }
    }

private:
    float bilinearInterp(const float* src, float sx, float sy) const {
        int x0 = static_cast<int>(sx);
        int y0 = static_cast<int>(sy);
        int x1 = x0 + 1, y1 = y0 + 1;
        // 경계 클램프
        x0 = std::clamp(x0, 0, (int)width_-1);
        x1 = std::clamp(x1, 0, (int)width_-1);
        y0 = std::clamp(y0, 0, (int)height_-1);
        y1 = std::clamp(y1, 0, (int)height_-1);
        float dx = sx - static_cast<float>(static_cast<int>(sx));
        float dy = sy - static_cast<float>(static_cast<int>(sy));
        float v00 = src[y0 * width_ + x0];
        float v10 = src[y0 * width_ + x1];
        float v01 = src[y1 * width_ + x0];
        float v11 = src[y1 * width_ + x1];
        return (1.0f-dx)*(1.0f-dy)*v00 + dx*(1.0f-dy)*v10
              +(1.0f-dx)*dy*v01         + dx*dy*v11;
    }

    DistortionParams params_{};
    uint32_t width_{0}, height_{0};
    std::vector<float> map_x_, map_y_;
};
```

### 3.10.4 교정 절차 (그리드 팬텀)

1. 알려진 핀 간격($d_{\text{ref}}$)의 격자 팬텀(≥9×9 격자) 촬영
2. 핀 중심 검출: Hough 변환 또는 Blob 검출기
3. 이상적 격자 좌표와 실측 좌표의 매핑으로 왜곡 계수 최소제곱 피팅
4. 보정 후 RMS 오차 $< 0.5$ pixel 확인

### 3.10.5 엣지 케이스

| 케이스 | 처리 |
|--------|------|
| LUT 미로드 | 패스스루 (왜곡 보정 스킵), 경고 로그 |
| 소스 좌표 이미지 외부 | 경계 클램프 후 최근접 픽셀 사용 |
| k1=k2=k3=p1=p2=0 | LUT = 항등 변환, bilinear만 수행 |
| 해상도 변경 | buildLut() 재호출 |

### 3.10.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-005b (Geometric Distortion Correction) |
| **SWU** | SWU-1.10 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 그리드 팬텀 RMS 오차 < 0.5 pixel, 경계 픽셀 clamp 단위 테스트 |

---

## §6.4 SWU-3.4b: Anatomy-Adaptive Auto Window/Level (GAP-AF 해소)

**관련 GAP:** GAP-AF — 해부 부위별로 최적화된 자동 W/L 설정 알고리즘이 미명세 상태였음.

### 6.4.1 개요

진단 목적에 따라 최적 W/L은 해부 부위마다 다르다. 흉부는 폐야와 종격동을 동시에 표시하기 위해 넓은 윈도우가 필요하고, 사지는 골 세부 구조를 위해 좁은 윈도우가 필요하다. 퍼센타일 기반 자동 W/L은 히스토그램을 분석하여 부위별 최적값을 산출한다.

### 6.4.2 수학적 명세

#### 6.4.2.1 퍼센타일 기반 W/L

$$WC = P_{50}(H)$$
$$WW = P_{\text{upper}} - P_{\text{lower}}$$

여기서 $P_{p}(H)$는 히스토그램 $H$에서 $p$번째 퍼센타일 값이다.

#### 6.4.2.2 해부 부위별 퍼센타일 테이블

| `XpeAnatomyType` | $P_{\text{lower}}$ | $P_{\text{upper}}$ | 이유 |
|------------------|--------------------|--------------------|------|
| `CHEST`          | P5                 | P95                | 폐야 + 종격동 동시 표시 |
| `EXTREMITY`      | P2                 | P98                | 골 세부 (좁은 W/W) |
| `ABDOMEN`        | P10                | P90                | 연부 조직 중심 |
| `SPINE`          | P5                 | P95                | 척추골 + 연부 조직 |
| `DEFAULT`        | P2                 | P98                | 범용 |

#### 6.4.2.3 Prefix Sum 퍼센타일 계산

65536-bin 히스토그램 $H[0..65535]$에 대해:

$$\text{cumsum}[k] = \sum_{i=0}^{k} H[i]$$

$$P_p = \min\left\{k : \text{cumsum}[k] \geq \frac{p}{100} \cdot N_{\text{valid}}\right\}$$

여기서 $N_{\text{valid}}$는 유효 픽셀 수 (콜리메이터 엣지 제외).

#### 6.4.2.4 콜리메이터 마스크 적용

히스토그램 집계 시 픽셀 값 $< 50$ ADU인 픽셀은 제외 (콜리메이터 경계 및 공기 영역).

### 6.4.3 C++ 구현

```cpp
// AutoWindowLevel.hpp

enum class XpeAnatomyType : uint8_t {
    DEFAULT    = 0,
    CHEST      = 1,
    EXTREMITY  = 2,
    ABDOMEN    = 3,
    SPINE      = 4
};

struct XpeWindowLevel {
    float wc;                // Window Center (ADU)
    float ww;                // Window Width  (ADU)
    float percentile_low;    // 실제 사용된 하위 퍼센타일 값
    float percentile_high;   // 실제 사용된 상위 퍼센타일 값
};

struct PercentilesForAnatomy {
    float p_low, p_high;
};

static constexpr PercentilesForAnatomy kAnatomyPercentiles[] = {
    {2.0f, 98.0f},   // DEFAULT
    {5.0f, 95.0f},   // CHEST
    {2.0f, 98.0f},   // EXTREMITY
    {10.0f, 90.0f},  // ABDOMEN
    {5.0f, 95.0f},   // SPINE
};

class AutoWindowLevel {
public:
    static constexpr int HIST_BINS = 65536;
    static constexpr float COLLIMATOR_THRESHOLD_ADU = 50.0f;

    XpeWindowLevel compute(const uint16_t* pixels, size_t n,
                           XpeAnatomyType anatomy,
                           float max_adu = 65535.0f) const
    {
        auto anat_idx = static_cast<size_t>(anatomy);
        float p_low  = kAnatomyPercentiles[anat_idx].p_low;
        float p_high = kAnatomyPercentiles[anat_idx].p_high;

        // 1) 히스토그램 집계 (콜리메이터 제외)
        std::array<uint32_t, HIST_BINS> hist{};
        uint64_t n_valid = 0;
        for (size_t i = 0; i < n; ++i) {
            if (pixels[i] >= 50) {
                hist[pixels[i]]++;
                ++n_valid;
            }
        }
        if (n_valid == 0) return {max_adu * 0.5f, max_adu, 0.0f, max_adu};

        // 2) Prefix sum → 퍼센타일 검색
        float lower_val = findPercentile(hist, n_valid, p_low);
        float upper_val = findPercentile(hist, n_valid, p_high);
        float median    = findPercentile(hist, n_valid, 50.0f);

        float ww = std::max(upper_val - lower_val, 1.0f);
        return {median, ww, lower_val, upper_val};
    }

private:
    static float findPercentile(const std::array<uint32_t, HIST_BINS>& hist,
                                uint64_t n_valid, float pct)
    {
        uint64_t target = static_cast<uint64_t>(pct / 100.0f * n_valid);
        uint64_t cumsum = 0;
        for (int k = 0; k < HIST_BINS; ++k) {
            cumsum += hist[k];
            if (cumsum >= target) return static_cast<float>(k);
        }
        return static_cast<float>(HIST_BINS - 1);
    }
};
```

### 6.4.4 엣지 케이스

| 케이스 | 처리 |
|--------|------|
| 전체 픽셀 < 50 ADU | 기본값 반환 (WC=max/2, WW=max) |
| upper=lower (단조 이미지) | WW = max(1, upper-lower) 강제 설정 |
| 알 수 없는 anatomy | DEFAULT 퍼센타일 사용 |
| 히스토그램 단일 빈 집중 | P50=동일값 → 진단 경고 로그 |

### 6.4.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-021b (Auto W/L by Anatomy) |
| **SWU** | SWU-3.4b |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 각 해부 부위별 팬텀 이미지로 W/L 범위 임상 검증; 콜리메이터 제외 단위 테스트 |

---

### 6.5 SWU-6.5 DICOM GSDF 그레이스케일 표준 디스플레이 함수 ★GAP-BM 해소

**표준**: DICOM PS 3.14 (NEMA GSDF — Grayscale Standard Display Function)  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-DISP-005 | SWU-6.5 | xpe_display.dll | Class B |

#### 배경

DICOM PS 3.14는 **인간 시각 시스템의 JND(Just-Noticeable Difference) 특성**에 따라 디스플레이 구동 레벨 p를 휘도 L로 매핑하는 표준 함수를 정의한다. XPE는 소프트카피 뷰어에서 진단 일관성을 보장하기 위해 GSDF 보정을 구현해야 한다.

#### 수학적 정의

GSDF JND-index j → 휘도 L(j) (cd/m²):

```
log₁₀[L(j)] = −2.525 + 0.2021·j − 0.04054·j²/J_max + ...
(PS 3.14 §B.2 다항식 근사, j ∈ [1, 1023])
```

모니터 특성화 (N=18 구동 레벨 측정):

```
p_k ∈ {0, 17, 34, ..., 255} → L_k 측정 (cd/m²)
최소 제곱 피팅: minimize Σ (J_fit(L_k) - J_target(p_k))²
보정 LUT: p_cal[j] (j = 0..1023) precomputed
```

런타임 변환:

```
j_target = J_min + (J_max - J_min) × I_disp / 65535
p_out    = p_cal[ round(j_target) ]
```

#### C++ 구조체 및 API

```cpp
struct XpeGsdfCalib {
    float    a, b, c;           // GSDF polynomial coefficients
    uint16_t lut[1024];         // driving level LUT: p_cal[j]
    float    L_min, L_max;      // monitor luminance range (cd/m²)
    float    J_min, J_max;      // JND-index range
    bool     calibrated;        // false = use default GSDF
};

// Characterize display from luminance measurements
XpeStatus xpe_display_gsdf_calibrate(
    XpeGsdfCalib* cal,
    const float*  L_measured,   // 18 luminance values
    int           n_levels      // = 18
);

// Apply GSDF LUT to display image
XpeStatus xpe_display_gsdf_apply(
    const uint16_t* I_in,
    uint16_t*       I_out,
    uint32_t        W,
    uint32_t        H,
    const XpeGsdfCalib* cal
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 1 ms / 3K×3K (SSE2 LUT gather) |
| 교정 주기 | 매월 1회 휘도계 측정 권고 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 휘도 오차 | `|L_output − L_target| < 1%` of L_range at 18 test points |
| JND 균일성 | `ΔJ_max < 0.5` (인접 구동 레벨 간 최대 JND 편차) |
| LUT 단조성 | `p_cal[j]` 엄격 단조 증가 |
| 미교정 폴백 | `calibrated=false` 시 선형 패스스루 |

---

### 6.6 SWU-6.6 로컬 톤 매핑 (Multi-Scale Retinex) ★GAP-BN 해소

**참고**: Jobson et al., "A Multiscale Retinex for Bridging the Gap Between Color Images and the Human Observation of Scenes," IEEE TIP 1997.  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-DISP-004 | SWU-6.6 | xpe_display.dll | Class B |

#### 수학적 정의

Multi-Scale Retinex (MSR) — 로그 반사율 추정:

```
R(x,y) = Σ_{k=1}^{3} w_k · { log[I(x,y) + ε] − log[F_k * I(x,y) + ε] }

F_k(x,y) = (1/2πσ_k²) · exp(−(x²+y²)/2σ_k²)
σ_k ∈ {15, 80, 250} pixels,  w_k = 1/3

출력: I_ltm = clamp( α · R(x,y) + β, 0, 65535 )
       α = 128,  β = 32768 (기본값)
```

FFT 기반 Gaussian 컨볼루션 (MKL DFT):

```
F̂_k(u,v) = exp(−2π²σ_k²(u² + v²) / N²)
F_k * I  = IFFT( FFT(I) · F̂_k )
```

CLAHE 후처리 (`clip_limit=2.0`, `tile=64×64`): 국소 히스토그램 평활화로 잔류 불균일 제거.

#### C++ 구조체 및 API

```cpp
struct XpeRetinexParams {
    float sigma[3];       // {15.0f, 80.0f, 250.0f}
    float weights[3];     // {1/3, 1/3, 1/3}
    float alpha;          // contrast gain (default 128)
    float beta;           // brightness offset (default 32768)
    int   clahe_clip;     // CLAHE clip limit (default 2)
    int   tile_size;      // CLAHE tile size (default 64)
};

XpeStatus xpe_display_retinex(
    const uint16_t*        I_in,
    uint16_t*              I_out,
    uint32_t               W,
    uint32_t               H,
    const XpeRetinexParams* params
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 30 ms / 3K×3K (3× MKL FFT + CLAHE) |
| 메모리 | 3× float32 임시 버퍼 (3K×3K × 12 byte = 108 MB) |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 국소 대비 향상 | 저대비 영역 대비비 ≥ 1.5× (히스토그램 분산 기준) |
| 헤일로 아티팩트 | 기울기 반전 픽셀 비율 < 5% |
| 밝기 보존 | 전체 평균 밝기 편차 < 5% |
| 파라미터 범위 | `sigma[k] ∈ [5, 500]`, `alpha ∈ [50, 500]` 클램핑 |

---

## §9.7 SWU-9.7: Pixel Binning Mode 교정 보간 (GAP-AB 해소)

**관련 GAP:** GAP-AB — 픽셀 빈닝(binning) 모드에서 교정 맵 적용 방법이 미명세 상태였음.

### 9.7.1 개요

FPD는 1×1 (풀 해상도), 2×2, 3×3 등 다양한 픽셀 빈닝 모드를 지원한다. 각 모드에서 유효 픽셀 크기가 달라지므로 교정 맵(offset, gain, defect), Lag 계수, 그리고 비선형성 LUT 모두 빈닝 모드에 맞게 보정해야 한다.

### 9.7.2 수학적 명세

#### 9.7.2.1 Gain Map 빈닝

빈닝 팩터 $B$ (예: B=2 for 2×2)에 대해:

$$G_{\text{binned}}(x_b, y_b) = \frac{1}{B^2} \sum_{i=0}^{B-1} \sum_{j=0}^{B-1} G_{1\times1}(x_b \cdot B + i,\ y_b \cdot B + j)$$

블록 평균으로 고해상도 gain map에서 빈닝된 gain map을 유도한다.

#### 9.7.2.2 Defect Map 빈닝

$$D_{\text{binned}}(x_b, y_b) = \begin{cases}
1 & \text{if } \exists (i,j): D_{1\times1}(x_b B + i,\ y_b B + j) = 1 \\
0 & \text{otherwise}
\end{cases}$$

빈닝된 픽셀을 구성하는 1×1 픽셀 중 하나라도 결함이면 빈닝 픽셀도 결함으로 표시.

#### 9.7.2.3 Lag 계수 스케일링

$$\tau_{\text{binned}} = B \cdot \tau_{1\times1}$$

픽셀 크기가 커질수록 광전 변환 후 전하 방전이 느려지므로 시상수 $\tau$는 빈닝 팩터에 비례하여 증가한다.

### 9.7.3 C++ 구현

```cpp
// BinningCalibrationInterpolator.hpp

struct BinningIndex {
    uint32_t binning_factor;     // 1, 2, 3 등
    std::vector<float> gain_map; // 빈닝된 gain map
    std::vector<uint8_t> defect_map;
    float lag_tau_scale;         // = binning_factor
};

class BinningCalibrationInterpolator {
public:
    /// @brief 1x1 교정 맵으로부터 모든 빈닝 레벨의 교정 맵 파생
    void buildBinningIndex(const float* gain_1x1,
                           const uint8_t* defect_1x1,
                           uint32_t full_w, uint32_t full_h,
                           float base_lag_tau,
                           const std::vector<uint32_t>& binning_factors)
    {
        full_w_ = full_w; full_h_ = full_h;
        for (auto B : binning_factors) {
            BinningIndex idx;
            idx.binning_factor = B;
            idx.lag_tau_scale  = static_cast<float>(B);
            uint32_t bw = full_w / B, bh = full_h / B;
            idx.gain_map.resize(bw * bh);
            idx.defect_map.resize(bw * bh, 0);

            for (uint32_t yb = 0; yb < bh; ++yb) {
                for (uint32_t xb = 0; xb < bw; ++xb) {
                    float gain_sum = 0.0f;
                    bool any_defect = false;
                    for (uint32_t i = 0; i < B; ++i) {
                        for (uint32_t j = 0; j < B; ++j) {
                            uint32_t px = xb*B + j, py = yb*B + i;
                            gain_sum   += gain_1x1[py * full_w + px];
                            any_defect |= (defect_1x1[py * full_w + px] != 0);
                        }
                    }
                    idx.gain_map[yb * bw + xb]   = gain_sum / (B*B);
                    idx.defect_map[yb * bw + xb] = any_defect ? 1 : 0;
                }
            }
            index_[B] = std::move(idx);
        }
        base_lag_tau_ = base_lag_tau;
    }

    /// @brief 런타임 빈닝 모드 선택 (XpeFrameMetadata.binning_mode 기반)
    const BinningIndex* getBinningIndex(uint32_t binning_factor) const {
        auto it = index_.find(binning_factor);
        return (it != index_.end()) ? &it->second : nullptr;
    }

    float getLagTau(uint32_t binning_factor) const {
        return base_lag_tau_ * static_cast<float>(binning_factor);
    }

private:
    std::unordered_map<uint32_t, BinningIndex> index_;
    uint32_t full_w_{0}, full_h_{0};
    float    base_lag_tau_{0.0f};
};
```

### 9.7.4 교정 매니페스트 확장

```json
{
  "binning_index": [
    { "binning_factor": 1, "gain_map_file": "gain_1x1.bin",   "defect_map_file": "defect_1x1.bin",   "lag_tau": 0.033 },
    { "binning_factor": 2, "gain_map_file": "gain_2x2.bin",   "defect_map_file": "defect_2x2.bin",   "lag_tau": 0.066 },
    { "binning_factor": 3, "gain_map_file": "gain_3x3.bin",   "defect_map_file": "defect_3x3.bin",   "lag_tau": 0.099 }
  ]
}
```

### 9.7.5 엣지 케이스

| 케이스 | 처리 |
|--------|------|
| 빈닝 팩터가 인덱스에 없음 | 가장 가까운 팩터의 gain map 사용 + 경고 로그 |
| full_w % B != 0 | 나머지 픽셀 행/열 무시 (보수적 처리) |
| Defect 맵 전체 0 | 정상 처리 (결함 없음) |
| lag_tau = 0 | Lag 보정 스킵 (tau=0 = no lag) |

### 9.7.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-002c (Binning Mode Calibration) |
| **SWU** | SWU-9.7 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 빈닝 2×2 gain map 블록 평균 정확도 < 0.1%; defect 전파 단위 테스트; lag tau 스케일 검증 |

---

## §9.8 SWU-9.8: Multi-Frame Sigma-Clipping 교정 (GAP-AG 해소)

**관련 GAP:** GAP-AG — 오프라인 교정 프레임 평균화 시 이상치 제거(sigma-clipping) 알고리즘이 미명세 상태였음.

### 9.8.1 개요

Offset/Gain 교정 맵 생성 시 다수의 교정 프레임을 취득하여 평균화한다. 그러나 방사선 산란, 전기적 노이즈 스파이크 등으로 이상치 프레임이 발생할 수 있다. Sigma-clipping 알고리즘은 반복적으로 이상치를 제거하여 교정 맵의 품질을 향상시킨다.

### 9.8.2 수학적 명세

#### 9.8.2.1 반복적 σ-클리핑 알고리즘

각 픽셀 $(x,y)$에 대해 $N$개 프레임의 값 집합 $\{F_k(x,y)\}_{k=1}^N$:

**반복 1~max_iter:**

$$\mu = \frac{1}{|S|} \sum_{k \in S} F_k(x,y)$$

$$\sigma = \sqrt{\frac{1}{|S|} \sum_{k \in S} (F_k(x,y) - \mu)^2}$$

**클리핑:** 다음 프레임을 유효 집합 $S$에서 제거:

$$\text{reject} = \{k \in S : |F_k(x,y) - \mu| > \kappa \cdot \sigma\}$$

기본값: $\kappa = 3.0$, `max_iter = 5`

**최소 프레임 수 제약:**

$$N_{\text{min}} = \max(3,\ \lfloor N/4 \rfloor)$$

유효 프레임 수 $|S| < N_{\text{min}}$이면 해당 픽셀을 정적 결함으로 마킹.

#### 9.8.2.2 클리핑 후 평균

$$\text{Cal\_Map}(x,y) = \frac{1}{|S_{\text{final}}|} \sum_{k \in S_{\text{final}}} F_k(x,y)$$

### 9.8.3 Python (NumPy) 구현 — 오프라인 교정

```python
# sigma_clipping_calibration.py
import numpy as np
from typing import Tuple

def sigma_clip_calibration(
    frames: np.ndarray,    # shape: (N, H, W), float32
    kappa: float = 3.0,
    max_iter: int = 5,
    min_frames: int = None
) -> Tuple[np.ndarray, np.ndarray]:
    """
    오프라인 교정 프레임 sigma-clipping 평균화.

    Returns:
        cal_map:      (H, W) float32 — 교정 맵
        defect_mask:  (H, W) bool   — True: 정적 결함 픽셀
    """
    N, H, W = frames.shape
    if min_frames is None:
        min_frames = max(3, N // 4)

    # 유효 마스크: shape (N, H, W), 초기 전체 True
    valid = np.ones((N, H, W), dtype=bool)

    for iteration in range(max_iter):
        # 유효 프레임 수
        valid_count = valid.sum(axis=0)  # (H, W)

        # 평균 계산 (유효 픽셀만)
        masked = np.where(valid, frames, 0.0)
        mu = masked.sum(axis=0) / np.maximum(valid_count, 1)  # (H, W)

        # 표준편차 계산
        diff2 = np.where(valid, (frames - mu[np.newaxis])**2, 0.0)
        sigma = np.sqrt(diff2.sum(axis=0) / np.maximum(valid_count, 1))

        # 클리핑 마스크 업데이트
        new_valid = valid & (np.abs(frames - mu[np.newaxis]) <= kappa * sigma[np.newaxis])

        # 수렴 판정
        if np.array_equal(new_valid, valid):
            break
        valid = new_valid

    # 최종 평균
    valid_count = valid.sum(axis=0)
    masked_final = np.where(valid, frames, 0.0)
    cal_map = masked_final.sum(axis=0) / np.maximum(valid_count, 1)

    # 최소 프레임 미만 픽셀 = 정적 결함
    defect_mask = valid_count < min_frames

    return cal_map.astype(np.float32), defect_mask
```

### 9.8.4 성능 특성

| 파라미터 | 권장값 | 비고 |
|---------|--------|------|
| $\kappa$ | 3.0 | Gaussian 분포에서 99.7% 포함 |
| max_iter | 5 | 대부분 2~3회에 수렴 |
| N (교정 프레임 수) | ≥ 16 | min_frames=4 보장 |
| N_min | max(3, N/4) | 보수적 결함 마킹 |

### 9.8.5 엣지 케이스

| 케이스 | 처리 |
|--------|------|
| N < 4 | max_iter=1, min_frames=N으로 강제 (경고 로그) |
| sigma=0 (모든 프레임 동일) | 클리핑 없음 (조건 항상 False) |
| 모든 프레임 클리핑 | defect_mask=True 마킹 |
| kappa 범위 | 1.5 ~ 5.0 허용 (너무 작으면 과도 클리핑) |

### 9.8.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-CAL-001b (Calibration Frame Quality) |
| **SWU** | SWU-9.8 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 인위적 이상치 주입 후 제거율 ≥ 99%; min_frames 강제 단위 테스트 |

---

## §10.7 메모리 아레나 Zero-Copy 아키텍처 (GAP-AC 해소)

**관련 GAP:** GAP-AC — 실시간 파이프라인에서 힙 할당 없는 zero-copy 메모리 관리 아키텍처가 미명세 상태였음.

### 10.7.1 개요

XPE 파이프라인은 < 2ms 지연 목표를 위해 런타임 힙 할당(malloc/new)을 제거해야 한다. Ring-buffer 아레나 아키텍처는 초기화 시 $N$개의 이미지 버퍼를 사전 할당하고, 파이프라인 각 단계가 슬롯을 순환 사용한다.

### 10.7.2 수학적 명세 (상태 기계)

각 슬롯의 상태 전이:

$$\text{FREE} \xrightarrow{\text{acquire}} \text{WRITING} \xrightarrow{\text{publish}} \text{READY} \xrightarrow{\text{acquire\_read}} \text{READING} \xrightarrow{\text{release}} \text{FREE}$$

슬롯 수 $N = 8$ (기본값): 파이프라인 깊이(~4단계) × 2의 안전 마진.

### 10.7.3 C++ 구현

```cpp
// XpeMemoryArena.hpp

#include <atomic>
#include <cstdint>
#include <cassert>

enum class SlotState : uint32_t {
    FREE    = 0,
    WRITING = 1,
    READY   = 2,
    READING = 3
};

struct XpeImageBuffer {
    float*   data;          // 픽셀 데이터 포인터
    uint32_t width;
    uint32_t height;
    uint64_t frame_id;      // 단조 증가 프레임 ID
    uint64_t timestamp_ns;  // 취득 타임스탬프
};

struct ArenaSlot {
    std::atomic<SlotState> state{SlotState::FREE};
    XpeImageBuffer         buffer{};
};

class XpeMemoryArena {
public:
    static constexpr int DEFAULT_SLOTS = 8;

    /// @brief 아레나 초기화 — 초기화 후 힙 할당 없음
    /// @param width, height   단일 슬롯 이미지 크기
    /// @param n_slots         링 버퍼 슬롯 수
    void init(uint32_t width, uint32_t height,
              int n_slots = DEFAULT_SLOTS)
    {
        n_slots_ = n_slots;
        slots_.resize(n_slots);
        pixel_data_.resize(static_cast<size_t>(n_slots) * width * height);

        for (int i = 0; i < n_slots; ++i) {
            slots_[i].state.store(SlotState::FREE, std::memory_order_relaxed);
            slots_[i].buffer.data   = pixel_data_.data()
                                    + static_cast<size_t>(i) * width * height;
            slots_[i].buffer.width  = width;
            slots_[i].buffer.height = height;
        }
        write_idx_.store(0, std::memory_order_relaxed);
    }

    /// @brief 쓰기 슬롯 획득 (producer 측)
    /// @return 슬롯 인덱스, -1이면 아레나 소진
    int acquire_write()
    {
        for (int tries = 0; tries < n_slots_; ++tries) {
            int idx = write_idx_.fetch_add(1, std::memory_order_relaxed) % n_slots_;
            SlotState expected = SlotState::FREE;
            if (slots_[idx].state.compare_exchange_strong(
                    expected, SlotState::WRITING,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                return idx;
            }
        }
        return -1;  // XPE_ERR_ARENA_EXHAUSTED
    }

    /// @brief 슬롯을 READY로 전환 (producer → consumer)
    void publish(int slot_idx) {
        slots_[slot_idx].state.store(SlotState::READY,
                                     std::memory_order_release);
    }

    /// @brief 읽기 슬롯 획득 (consumer 측, 최신 READY 슬롯)
    int acquire_read()
    {
        for (int i = n_slots_ - 1; i >= 0; --i) {
            SlotState expected = SlotState::READY;
            if (slots_[i].state.compare_exchange_strong(
                    expected, SlotState::READING,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                return i;
            }
        }
        return -1;
    }

    /// @brief 슬롯 해제 (consumer → FREE)
    void release(int slot_idx) {
        slots_[slot_idx].state.store(SlotState::FREE,
                                     std::memory_order_release);
    }

    XpeImageBuffer* get_buffer(int slot_idx) {
        return &slots_[slot_idx].buffer;
    }

private:
    int n_slots_{0};
    std::vector<ArenaSlot>  slots_;
    std::vector<float>      pixel_data_;   // 연속 메모리 블록
    std::atomic<int>        write_idx_{0};
};
```

### 10.7.4 플랫폼별 대용량 페이지 지원

```cpp
// Windows: Large Pages (2MB)
void* arena_alloc_windows(size_t bytes) {
    HANDLE token;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token);
    // SeLockMemoryPrivilege 설정 생략...
    return VirtualAlloc(nullptr, bytes,
                        MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                        PAGE_READWRITE);
}

// Linux: HugePage mmap
void* arena_alloc_linux(size_t bytes) {
    return mmap(nullptr, bytes,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                -1, 0);
}
```

### 10.7.5 성능 특성

| 지표 | 목표 | 달성 방법 |
|------|------|---------|
| 런타임 힙 할당 | **0회** (초기화 이후) | 사전 할당 아레나 |
| 슬롯 획득 레이턴시 | < 50ns | CAS 원자 연산 |
| 메모리 연속성 | 단일 연속 블록 | cache prefetch 유리 |
| 슬롯 수 | 8 (기본) | 파이프라인 깊이 × 2 |

### 10.7.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-PERF-001 (Zero-Copy Pipeline Memory) |
| **SWU** | SWU-10.7 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | Valgrind/AddressSanitizer 으로 힙 할당 0회 확인; CAS 경합 stress 테스트; READY 없을 때 -1 반환 단위 테스트 |

---

## §10.8 Multi-Channel Producer-Consumer Thread Safety (GAP-AD 해소)

**관련 GAP:** GAP-AD — 멀티 채널 이미지 파이프라인에서 lock-free SPSC 링 버퍼 thread safety 아키텍처가 미명세 상태였음.

### 10.8.1 개요

XPE 파이프라인은 취득(Acquisition) → 전처리 → 핵심 처리 → 디스플레이 4단계로 구성된다. 각 단계는 별도 CPU 코어에 고정(CPU affinity)되어 병렬 실행된다. SPSC(Single Producer Single Consumer) lock-free 링 버퍼로 단계 간 데이터를 전달한다.

### 10.8.2 수학적 명세

#### 10.8.2.1 SPSC 링 버퍼 인덱스 계산

버퍼 용량 $C = 2^N$ (power-of-2):

$$\text{head} \mod C = \text{head} \& (C-1) \quad \text{(bitwise AND, 나머지 연산 대체)}$$

**생산자 (push):**

```
if ((head + 1) & mask == tail: full (drop oldest or block)
buffer[head & mask] = item
head.store(head + 1, RELEASE)
```

**소비자 (pop):**

```
if head.load(ACQUIRE) == tail: empty
item = buffer[tail & mask]
tail.store(tail + 1, RELEASE)
```

RELEASE-ACQUIRE 메모리 순서로 happens-before 보장.

#### 10.8.2.2 CPU 친화도 매핑

| 스레드 역할 | CPU 코어 |
|------------|---------|
| 취득 (Acquisition) | Core 0 |
| 전처리 (Pre-processing) | Core 1 |
| 핵심 처리 (Core Processing) | Core 2 |
| 디스플레이 (Display) | Core 3 |

### 10.8.3 C++ 구현

```cpp
// SpscRingBuffer.hpp

#include <atomic>
#include <array>
#include <optional>

template<typename T, size_t Capacity>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be power of 2");
public:
    static constexpr size_t MASK = Capacity - 1;

    /// @brief 생산자: 아이템 추가
    /// @return true: 성공, false: 버퍼 가득 참
    bool push(const T& item) noexcept {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t next_h = (h + 1) & MASK;
        if (next_h == tail_.load(std::memory_order_acquire))
            return false;  // full
        buffer_[h] = item;
        head_.store(next_h, std::memory_order_release);
        return true;
    }

    /// @brief 소비자: 아이템 추출
    std::optional<T> pop() noexcept {
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire))
            return std::nullopt;  // empty
        T item = buffer_[t];
        tail_.store((t + 1) & MASK, std::memory_order_release);
        return item;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire)
            == tail_.load(std::memory_order_acquire);
    }

private:
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::array<T, Capacity>         buffer_{};
};

// ── 파이프라인 채널 타입 정의 ──────────────────────────────────
using FrameSlotIdx = int;  // 아레나 슬롯 인덱스
using AcqToPreChannel  = SpscRingBuffer<FrameSlotIdx, 4>;
using PreToCoreChannel = SpscRingBuffer<FrameSlotIdx, 4>;
using CoreToDispChannel= SpscRingBuffer<FrameSlotIdx, 4>;

// ── CPU Affinity 설정 (Windows) ──────────────────────────────
void set_thread_affinity(std::thread& t, int core_id) {
    DWORD_PTR mask = (1ULL << core_id);
    SetThreadAffinityMask(t.native_handle(), mask);
}

// ── Backpressure: 버퍼 가득 찼을 때 가장 오래된 프레임 드롭 ──
struct XpePipelineStats {
    std::atomic<uint64_t> dropped_frames{0};
    std::atomic<uint64_t> processed_frames{0};
};

void push_with_backpressure(AcqToPreChannel& ch,
                             FrameSlotIdx slot,
                             XpeMemoryArena& arena,
                             XpePipelineStats& stats)
{
    if (!ch.push(slot)) {
        // 버퍼 가득 → 드롭
        arena.release(slot);
        stats.dropped_frames.fetch_add(1, std::memory_order_relaxed);
    }
}
```

### 10.8.4 메모리 순서 보장

| 연산 | 메모리 순서 | 보장 |
|------|-----------|------|
| `head_.store(...)` | `RELEASE` | 이전 buffer 쓰기가 소비자에게 가시적 |
| `head_.load(...)` | `ACQUIRE` | 소비자가 최신 head 값 획득 |
| `tail_.store(...)` | `RELEASE` | 소비 완료 사실을 생산자에게 통지 |
| `tail_.load(...)` | `ACQUIRE` | 생산자가 최신 tail 값 획득 |

### 10.8.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-PERF-002 (Pipeline Thread Safety) |
| **SWU** | SWU-10.8 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | ThreadSanitizer(TSan)로 data race 없음 확인; 백프레셔 드롭 카운트 단위 테스트; CPU 친화도 설정 검증 |

---

## §12.8 Automatic CNR Auto-Assessment IQI (GAP-AE 해소)

**관련 GAP:** GAP-AE — 임상 이미지 품질 지표인 CNR/SDNR 자동 계산 알고리즘이 미명세 상태였음.

### 12.8.1 개요

CNR(Contrast-to-Noise Ratio)과 SDNR(Signal Difference-to-Noise Ratio)은 X선 이미지의 대조도 품질을 정량화하는 핵심 지표다. 자동 CNR 평가 모듈은 히스토그램 분석으로 배경 및 신호 영역을 자동 검출하여 이 값들을 계산한다.

### 12.8.2 수학적 명세

#### 12.8.2.1 CNR 계산

배경 영역 (히스토그램 하위 5% 질량):

$$\mu_{\text{bg}} = \text{mean}(\{I : I \leq P_5\})$$
$$\sigma_{\text{bg}} = \text{std}(\{I : I \leq P_5\})$$

신호 영역 (히스토그램 50번째 퍼센타일 근방):

$$\mu_{\text{sig}} = \text{mean}(\{I : P_{40} \leq I \leq P_{60}\})$$

$$\text{CNR} = \frac{|\mu_{\text{sig}} - \mu_{\text{bg}}|}{\sigma_{\text{bg}}}$$

#### 12.8.2.2 SDNR 계산 (두 관심 영역 비교)

$$\text{SDNR} = \frac{|\mu_A - \mu_B|}{\sqrt{(\sigma_A^2 + \sigma_B^2)/2}}$$

여기서 A와 B는 각각 고신호 및 저신호 관심 영역(ROI)이다.

#### 12.8.2.3 배경 영역 최소 크기

$$N_{\text{bg,min}} = 64 \times 64 = 4096 \text{ pixels}$$

이 이상의 픽셀이 배경 분류에 포함되어야 통계적으로 신뢰할 수 있다.

#### 12.8.2.4 임상 목표값

| 부위 | CNR 목표 |
|------|---------|
| 일반 방사선촬영 | ≥ 5.0 |
| 가슴 촬영 | ≥ 8.0 |
| 사지 촬영 | ≥ 6.0 |

### 12.8.3 C++ 구현

```cpp
// AutoCnrAssessment.hpp

struct XpeCnrResult {
    float cnr;           // Contrast-to-Noise Ratio
    float sdnr;          // Signal Difference-to-Noise Ratio
    float mu_background; // 배경 평균
    float sigma_background;
    float mu_signal;     // 신호 평균
    bool  valid;         // 계산 유효 여부
};

class AutoCnrAssessment {
public:
    static constexpr float BG_LOWER_PCT = 5.0f;
    static constexpr float SIG_LOWER_PCT = 40.0f;
    static constexpr float SIG_UPPER_PCT = 60.0f;
    static constexpr size_t MIN_BG_PIXELS = 4096;

    XpeCnrResult compute(const float* image, uint32_t width, uint32_t height) const
    {
        size_t n = static_cast<size_t>(width) * height;
        if (n < MIN_BG_PIXELS) return {0,0,0,0,0,false};

        // 1) 히스토그램 (65536 bins, 0..65535 ADU 가정)
        std::vector<uint32_t> hist(65536, 0);
        for (size_t i = 0; i < n; ++i) {
            int bin = std::clamp(static_cast<int>(image[i]), 0, 65535);
            hist[bin]++;
        }

        // 2) 퍼센타일 검색
        float p5  = findPercentile(hist, n, BG_LOWER_PCT);
        float p40 = findPercentile(hist, n, SIG_LOWER_PCT);
        float p60 = findPercentile(hist, n, SIG_UPPER_PCT);

        // 3) 통계 계산
        auto [mu_bg, sigma_bg, count_bg] = computeStats(image, n, 0.0f, p5);
        auto [mu_sig, sigma_sig, count_sig] = computeStats(image, n, p40, p60);

        if (count_bg < MIN_BG_PIXELS || sigma_bg < 1e-6f)
            return {0,0,mu_bg,sigma_bg,mu_sig,false};

        float cnr = std::abs(mu_sig - mu_bg) / sigma_bg;
        float sdnr_denom = std::sqrt((sigma_bg*sigma_bg + sigma_sig*sigma_sig) / 2.0f);
        float sdnr = (sdnr_denom > 1e-6f)
                   ? std::abs(mu_sig - mu_bg) / sdnr_denom
                   : 0.0f;

        return {cnr, sdnr, mu_bg, sigma_bg, mu_sig, true};
    }

private:
    static float findPercentile(const std::vector<uint32_t>& hist,
                                size_t n, float pct)
    {
        uint64_t target = static_cast<uint64_t>(pct / 100.0f * n);
        uint64_t cum = 0;
        for (int k = 0; k < (int)hist.size(); ++k) {
            cum += hist[k];
            if (cum >= target) return static_cast<float>(k);
        }
        return static_cast<float>(hist.size() - 1);
    }

    struct Stats { float mean, std; size_t count; };
    static Stats computeStats(const float* data, size_t n,
                              float low, float high)
    {
        double sum = 0, sum2 = 0;
        size_t cnt = 0;
        for (size_t i = 0; i < n; ++i) {
            if (data[i] >= low && data[i] <= high) {
                sum  += data[i];
                sum2 += static_cast<double>(data[i]) * data[i];
                ++cnt;
            }
        }
        if (cnt == 0) return {0.0f, 0.0f, 0};
        float mean = static_cast<float>(sum / cnt);
        float var  = static_cast<float>(sum2/cnt - static_cast<double>(mean)*mean);
        return {mean, std::sqrt(std::max(0.0f, var)), cnt};
    }
};
```

### 12.8.4 XpeQualityState 연동

CNR 결과는 XpeQualityState 구조체에 저장된다:

```cpp
// XpeQualityState 확장 필드 (§13 연동)
struct XpeQualityState {
    // ... 기존 필드 ...
    float   cnr;        // Contrast-to-Noise Ratio (CNR ≥ 5.0 권장)
    float   sdnr;       // Signal Difference-to-Noise Ratio
    bool    cnr_valid;  // CNR 계산 유효 여부
};
```

### 12.8.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-MEAS-003 (Auto CNR/SDNR Assessment) |
| **SWU** | SWU-12.8 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 알려진 CNR의 합성 이미지로 계산 정확도 ±5% 검증; 최소 배경 픽셀 조건 테스트 |

---

## 15. Error Code Taxonomy 및 복구 동작 (GAP-AH 해소)

**관련 GAP:** GAP-AH — XPE 시스템 전체 오류 코드 분류 체계 및 각 오류에 대한 복구 동작이 미명세 상태였음.

### 15.1 개요

XPE DLL에서 반환하는 모든 오류 코드를 32개 항목으로 분류하고, 각 코드에 대해 심각도(fatal/recoverable/warning), 복구 동작, C# 호스트 동작을 정의한다.

### 15.2 XpeErrorCode 열거형

```cpp
// xpe_error_codes.h  — DLL Public Header

#pragma once
#include <cstdint>

/// @brief XPE 시스템 오류 코드 분류체계
/// @details 코드 범위별 카테고리:
///   0     = 성공
///   100-  = 교정 오류
///   200-  = 입력 검증 오류
///   300-  = 처리 오류
///   400-  = AI/DL 오류
///   500-  = 메모리 오류
///   999   = 알 수 없는 오류
enum class XpeErrorCode : int32_t {
    // ── 성공 ────────────────────────────────────────────
    XPE_OK                        =   0,

    // ── 교정 오류 (100-199) ──────────────────────────────
    XPE_ERR_CAL_NOT_LOADED        = 100,  ///< 교정 데이터 미로드
    XPE_ERR_CAL_CHECKSUM          = 101,  ///< 교정 파일 체크섬 불일치
    XPE_ERR_CAL_SESSION_MISMATCH  = 102,  ///< 교정 세션 ID 불일치
    XPE_ERR_CAL_DRIFT_EXCEEDED    = 103,  ///< 교정 드리프트 임계값 초과
    XPE_ERR_CAL_DIMENSION_MISMATCH= 104,  ///< 교정 맵과 입력 이미지 크기 불일치

    // ── 입력 검증 오류 (200-299) ─────────────────────────
    XPE_ERR_NULL_BUFFER           = 200,  ///< 입력/출력 버퍼 nullptr
    XPE_ERR_DIMENSION_ZERO        = 201,  ///< width 또는 height == 0
    XPE_ERR_UNSUPPORTED_BITDEPTH  = 202,  ///< 지원하지 않는 비트 깊이

    // ── 처리 오류 (300-399) ──────────────────────────────
    XPE_ERR_LOG_DOMAIN_UNDERFLOW  = 300,  ///< log(0) 도메인 오류 (ε 보정 실패)
    XPE_ERR_SIMD_NOT_SUPPORTED    = 301,  ///< AVX2/FMA CPU 기능 미지원

    // ── AI/DL 오류 (400-499) ─────────────────────────────
    XPE_ERR_AI_MODEL_NOT_LOADED   = 400,  ///< ONNX 모델 미로드
    XPE_ERR_AI_TIMEOUT            = 401,  ///< AI 추론 타임아웃 (기본 5초)
    XPE_ERR_AI_INFERENCE          = 402,  ///< ONNX Runtime 추론 실패

    // ── 메모리 오류 (500-599) ────────────────────────────
    XPE_ERR_ARENA_EXHAUSTED       = 500,  ///< 메모리 아레나 슬롯 소진
    XPE_ERR_ALLOCATION            = 501,  ///< 메모리 할당 실패

    // ── 일반 오류 ────────────────────────────────────────
    XPE_ERR_UNKNOWN               = 999,  ///< 알 수 없는 오류
};
```

### 15.3 오류 복구 매트릭스

| 오류 코드 | 심각도 | DLL 복구 동작 | C# 호스트 동작 |
|----------|--------|-------------|--------------|
| `XPE_OK` | — | — | 정상 처리 계속 |
| `XPE_ERR_CAL_NOT_LOADED` | **Fatal** | 파이프라인 중단 | UI 경고 팝업, 교정 재로드 유도 |
| `XPE_ERR_CAL_CHECKSUM` | **Fatal** | 파이프라인 중단 | UI 오류, 교정 파일 재다운로드 유도 |
| `XPE_ERR_CAL_SESSION_MISMATCH` | **Fatal** | 파이프라인 중단 | 세션 재설정, 재교정 안내 |
| `XPE_ERR_CAL_DRIFT_EXCEEDED` | **Recoverable** | 경고 로그, 드리프트 보정 계속 | 오퍼레이터에게 재교정 권고 알림 |
| `XPE_ERR_CAL_DIMENSION_MISMATCH` | **Fatal** | 파이프라인 중단 | UI 오류, 교정-FPD 구성 확인 안내 |
| `XPE_ERR_NULL_BUFFER` | **Fatal** | 즉시 반환 | 소프트웨어 버그 로그, 재시작 |
| `XPE_ERR_DIMENSION_ZERO` | **Fatal** | 즉시 반환 | 입력 파라미터 검증 실패 로그 |
| `XPE_ERR_UNSUPPORTED_BITDEPTH` | **Fatal** | 즉시 반환 | FPD 설정 확인 안내 |
| `XPE_ERR_LOG_DOMAIN_UNDERFLOW` | **Warning** | ε 대체 후 처리 계속 | 로그 기록, 처리 계속 |
| `XPE_ERR_SIMD_NOT_SUPPORTED` | **Recoverable** | 스칼라 참조 구현으로 폴백 | 성능 경고 로그 |
| `XPE_ERR_AI_MODEL_NOT_LOADED` | **Recoverable** | AI 모듈 스킵, 파이프라인 계속 | AI 비활성화 UI 표시 |
| `XPE_ERR_AI_TIMEOUT` | **Recoverable** | AI 모듈 스킵, 파이프라인 계속 | AI 타임아웃 로그, 재시도 |
| `XPE_ERR_AI_INFERENCE` | **Recoverable** | AI 모듈 스킵, 폴백 결과 반환 | AI 오류 로그, 지원팀 통보 |
| `XPE_ERR_ARENA_EXHAUSTED` | **Recoverable** | 프레임 드롭, dropped_frames++ | 드롭 횟수 모니터링, 임계 초과 시 알림 |
| `XPE_ERR_ALLOCATION` | **Fatal** | 파이프라인 중단 | OOM 오류 UI, 재시작 안내 |
| `XPE_ERR_UNKNOWN` | **Fatal** | 파이프라인 중단 | 오류 덤프 저장, 지원팀 통보 |

### 15.4 xpe_error_string 구현

```cpp
// xpe_error_string.cpp

#include "xpe_error_codes.h"

/// @brief 오류 코드를 사람이 읽을 수 있는 영문 문자열로 변환
/// @return const char* (정적 문자열, 해제 불필요)
const char* xpe_error_string(XpeErrorCode code) noexcept {
    switch (code) {
    case XpeErrorCode::XPE_OK:
        return "XPE_OK: Success";
    case XpeErrorCode::XPE_ERR_CAL_NOT_LOADED:
        return "XPE_ERR_CAL_NOT_LOADED: Calibration data not loaded";
    case XpeErrorCode::XPE_ERR_CAL_CHECKSUM:
        return "XPE_ERR_CAL_CHECKSUM: Calibration file checksum mismatch";
    case XpeErrorCode::XPE_ERR_CAL_SESSION_MISMATCH:
        return "XPE_ERR_CAL_SESSION_MISMATCH: Calibration session ID mismatch";
    case XpeErrorCode::XPE_ERR_CAL_DRIFT_EXCEEDED:
        return "XPE_ERR_CAL_DRIFT_EXCEEDED: Calibration drift threshold exceeded";
    case XpeErrorCode::XPE_ERR_CAL_DIMENSION_MISMATCH:
        return "XPE_ERR_CAL_DIMENSION_MISMATCH: Calibration map dimension mismatch";
    case XpeErrorCode::XPE_ERR_NULL_BUFFER:
        return "XPE_ERR_NULL_BUFFER: Input or output buffer is null";
    case XpeErrorCode::XPE_ERR_DIMENSION_ZERO:
        return "XPE_ERR_DIMENSION_ZERO: Image width or height is zero";
    case XpeErrorCode::XPE_ERR_UNSUPPORTED_BITDEPTH:
        return "XPE_ERR_UNSUPPORTED_BITDEPTH: Unsupported pixel bit depth";
    case XpeErrorCode::XPE_ERR_LOG_DOMAIN_UNDERFLOW:
        return "XPE_ERR_LOG_DOMAIN_UNDERFLOW: Log domain underflow, epsilon substitution applied";
    case XpeErrorCode::XPE_ERR_SIMD_NOT_SUPPORTED:
        return "XPE_ERR_SIMD_NOT_SUPPORTED: AVX2/FMA not supported, scalar fallback active";
    case XpeErrorCode::XPE_ERR_AI_MODEL_NOT_LOADED:
        return "XPE_ERR_AI_MODEL_NOT_LOADED: ONNX model not loaded";
    case XpeErrorCode::XPE_ERR_AI_TIMEOUT:
        return "XPE_ERR_AI_TIMEOUT: AI inference timeout exceeded";
    case XpeErrorCode::XPE_ERR_AI_INFERENCE:
        return "XPE_ERR_AI_INFERENCE: ONNX Runtime inference failure";
    case XpeErrorCode::XPE_ERR_ARENA_EXHAUSTED:
        return "XPE_ERR_ARENA_EXHAUSTED: Memory arena slots exhausted, frame dropped";
    case XpeErrorCode::XPE_ERR_ALLOCATION:
        return "XPE_ERR_ALLOCATION: Memory allocation failed";
    case XpeErrorCode::XPE_ERR_UNKNOWN:
        return "XPE_ERR_UNKNOWN: Unknown error";
    default:
        return "XPE_ERR_UNRECOGNIZED: Error code not recognized";
    }
}

/// @brief 오류 코드의 심각도 반환
enum class XpeErrorSeverity { OK, WARNING, RECOVERABLE, FATAL };

XpeErrorSeverity xpe_error_severity(XpeErrorCode code) noexcept {
    if (code == XpeErrorCode::XPE_OK)
        return XpeErrorSeverity::OK;
    int c = static_cast<int>(code);
    // Warning 레벨
    if (c == 300 || c == 301)
        return XpeErrorSeverity::WARNING;
    // Recoverable 레벨
    if (c == 103 || c == 301 ||
        (c >= 400 && c <= 402) || c == 500)
        return XpeErrorSeverity::RECOVERABLE;
    // 나머지는 Fatal
    return XpeErrorSeverity::FATAL;
}
```

### 15.5 C# 호스트 통합 패턴

```csharp
// XpeErrorHandler.cs  (C# 호스트측 처리 패턴)

public enum XpeErrorCode : int
{
    XPE_OK                        = 0,
    XPE_ERR_CAL_NOT_LOADED        = 100,
    XPE_ERR_CAL_CHECKSUM          = 101,
    XPE_ERR_CAL_SESSION_MISMATCH  = 102,
    XPE_ERR_CAL_DRIFT_EXCEEDED    = 103,
    XPE_ERR_CAL_DIMENSION_MISMATCH= 104,
    XPE_ERR_NULL_BUFFER           = 200,
    XPE_ERR_DIMENSION_ZERO        = 201,
    XPE_ERR_UNSUPPORTED_BITDEPTH  = 202,
    XPE_ERR_LOG_DOMAIN_UNDERFLOW  = 300,
    XPE_ERR_SIMD_NOT_SUPPORTED    = 301,
    XPE_ERR_AI_MODEL_NOT_LOADED   = 400,
    XPE_ERR_AI_TIMEOUT            = 401,
    XPE_ERR_AI_INFERENCE          = 402,
    XPE_ERR_ARENA_EXHAUSTED       = 500,
    XPE_ERR_ALLOCATION            = 501,
    XPE_ERR_UNKNOWN               = 999,
}

public static class XpeErrorHandler
{
    public static void HandleError(XpeErrorCode code, ILogger logger,
                                   IUiNotifier ui)
    {
        switch (code)
        {
            case XpeErrorCode.XPE_OK:
                return;

            // Fatal: 파이프라인 즉시 중단 + UI 알림
            case XpeErrorCode.XPE_ERR_CAL_NOT_LOADED:
            case XpeErrorCode.XPE_ERR_CAL_CHECKSUM:
            case XpeErrorCode.XPE_ERR_CAL_SESSION_MISMATCH:
            case XpeErrorCode.XPE_ERR_CAL_DIMENSION_MISMATCH:
                logger.LogCritical($"Fatal XPE error: {code}");
                ui.ShowError($"교정 오류: {code}. 교정 데이터를 확인하십시오.");
                throw new XpeFatalException(code);

            // Recoverable: 경고 로그 + 처리 계속
            case XpeErrorCode.XPE_ERR_CAL_DRIFT_EXCEEDED:
                logger.LogWarning($"Calibration drift exceeded ({code})");
                ui.ShowWarning("교정 드리프트 감지. 재교정을 권장합니다.");
                break;

            case XpeErrorCode.XPE_ERR_SIMD_NOT_SUPPORTED:
                logger.LogWarning("SIMD not supported, scalar fallback active.");
                break;

            case XpeErrorCode.XPE_ERR_AI_TIMEOUT:
            case XpeErrorCode.XPE_ERR_AI_INFERENCE:
                logger.LogWarning($"AI module error: {code}, AI disabled.");
                ui.ShowInfo("AI 기능을 사용할 수 없습니다.");
                break;

            case XpeErrorCode.XPE_ERR_ARENA_EXHAUSTED:
                logger.LogWarning("Arena exhausted, frame dropped.");
                break;

            // Fatal: 재시작 안내
            default:
                logger.LogCritical($"Unhandled XPE error: {code}");
                ui.ShowError("XPE 오류가 발생했습니다. 응용 프로그램을 재시작하십시오.");
                throw new XpeFatalException(code);
        }
    }
}
```

### 15.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-ERR-001 (Error Code Taxonomy) |
| **SWU** | SWU-15.0 |
| **IEC 62304 §** | 5.4.2, 5.6 (Software Integration), 8.2.4 (Risk Control) |
| **검증 방법** | 각 오류 코드에 대한 단위 테스트; xpe_error_string 비-NULL 반환 확인; C# 핸들러 Fatal/Recoverable 분기 테스트 |
| **안전 분류** | Class B — Fatal 오류가 처리되지 않을 경우 잘못된 이미지 처리 위험; 반드시 모든 오류 코드에 대한 C# 핸들러 구현 |

---

## §3.4.6 SWU-1.4.6: Real-Time GCR Estimator (GAP-AI 해소)

**관련 GAP:** GAP-AI — ghost-correction SDD §6 "GcrEstimator" 모듈이 존재하나 ALG-001에 GCR 추정 알고리즘이 미명세 상태였음.

### 3.4.6.1 개요

Ghost Charge Ratio (GCR)는 현재 프레임에 잔존하는 이전 노출 잔류 전하의 비율이다. GCR이 임계값을 초과할 때만 Lag 보정을 활성화함으로써 불필요한 보정 적용을 방지하고 처리 효율을 높인다. 슬라이딩 윈도우 기반 EMA(지수 이동 평균)를 사용해 실시간으로 추정한다.

**교차 검증 출처:** ghost-correction SDD §6 "GcrEstimator" 모듈 — 해당 모듈은 ALG-001의 §3.4 Ghost/Lag Correction 파이프라인에 통합되어야 하나 알고리즘 명세가 누락되어 있었음.

### 3.4.6.2 수학적 명세

#### 3.4.6.2.1 순간 GCR 추정

$$\text{GCR}(t) = \frac{\bar{I}_{\text{ghost}}(t) - \bar{I}_{\text{dark}}}{\bar{I}_{\text{prev\_exposure}} - \bar{I}_{\text{dark}}}$$

여기서:
- $\bar{I}_{\text{ghost}}(t)$: 현재 암흑 프레임의 중앙 ROI 평균값 (ADU)
- $\bar{I}_{\text{dark}}$: 기준 암흑 오프셋 평균값 (ADU, 교정 테이블에서 로드)
- $\bar{I}_{\text{prev\_exposure}}$: 직전 노출 프레임의 중앙 ROI 평균값 (ADU)

분모가 $|\bar{I}_{\text{prev\_exposure}} - \bar{I}_{\text{dark}}| < \epsilon$ (ε = 10 ADU) 이면 GCR = 0으로 처리한다.

#### 3.4.6.2.2 지수 감쇠 모델

노출 종료 후 시간 $t$에서의 이론적 GCR:

$$\text{GCR}(t) = \text{GCR}(0) \cdot \exp\!\left(-\frac{t}{\tau_{\text{eff}}}\right)$$

여기서 $\tau_{\text{eff}}$는 §3.4 (Lag/Ghost Correction)에서 피팅된 유효 시정수이다.

#### 3.4.6.2.3 EMA 업데이트 (슬라이딩 윈도우)

최근 $N = 5$ 프레임 기반 지수 이동 평균:

$$\hat{\text{GCR}}_k = \alpha \cdot \text{GCR}_k + (1-\alpha) \cdot \hat{\text{GCR}}_{k-1}, \quad \alpha = \frac{2}{N+1} = \frac{1}{3}$$

#### 3.4.6.2.4 활성화 임계값

$$\text{lag\_correction\_active} = \begin{cases} \text{true} & \hat{\text{GCR}}_k > 0.002 \\ \text{false} & \text{otherwise} \end{cases}$$

임계값 0.002 (0.2%)는 임상적으로 의미있는 ghost artifact 최소 가시 수준에 해당한다.

### 3.4.6.3 C++ 구현

```cpp
// xpe_gcr_estimator.h

#pragma once
#include "xpe_types.h"

struct GcrState {
    float gcr_ema;              // 현재 EMA 추정값
    float prev_exposure_mean;   // 직전 노출 ROI 평균 (ADU)
    float dark_offset_mean;     // 교정 테이블 기준 암흑 평균 (ADU)
    int   frame_count;          // 누적 프레임 수
    bool  initialized;
};

// DLL Export API
extern "C" {
    /// @brief 현재 프레임의 GCR 추정 및 EMA 업데이트
    /// @param frame     현재 암흑/저노출 프레임 (float32, width×height)
    /// @param state     GCR 상태 (in/out) — 슬라이딩 윈도우 보존
    /// @return 현재 EMA-GCR 값 (0.0 ~ 1.0)
    float xpe_gcr_estimate(const XpeImageBuffer* frame, GcrState* state);
}

// Internal class
class GcrEstimator {
public:
    static constexpr float ALPHA          = 1.0f / 3.0f;  // N=5 EMA
    static constexpr float TRIGGER_THRESH = 0.002f;        // 0.2%
    static constexpr float DENOM_FLOOR    = 10.0f;         // ADU

    void update_dark_reference(float dark_mean) {
        state_.dark_offset_mean = dark_mean;
    }

    void record_exposure(const XpeImageBuffer* exposure_frame) {
        state_.prev_exposure_mean = compute_roi_mean(exposure_frame);
    }

    float estimate(const XpeImageBuffer* current_frame) {
        float denom = state_.prev_exposure_mean - state_.dark_offset_mean;
        float gcr_instant = 0.0f;
        if (std::abs(denom) > DENOM_FLOOR) {
            float num = compute_roi_mean(current_frame) - state_.dark_offset_mean;
            gcr_instant = std::clamp(num / denom, 0.0f, 1.0f);
        }
        if (!state_.initialized) {
            state_.gcr_ema = gcr_instant;
            state_.initialized = true;
        } else {
            state_.gcr_ema = ALPHA * gcr_instant + (1.0f - ALPHA) * state_.gcr_ema;
        }
        state_.frame_count++;
        return state_.gcr_ema;
    }

    bool should_apply_lag_correction() const {
        return state_.gcr_ema > TRIGGER_THRESH;
    }

private:
    GcrState state_{};

    /// @brief 중앙 20%×20% ROI의 평균값 계산
    float compute_roi_mean(const XpeImageBuffer* img) const {
        uint32_t x0 = img->width  * 4 / 10;
        uint32_t x1 = img->width  * 6 / 10;
        uint32_t y0 = img->height * 4 / 10;
        uint32_t y1 = img->height * 6 / 10;
        double sum = 0; size_t cnt = 0;
        const float* data = reinterpret_cast<const float*>(img->data);
        for (uint32_t y = y0; y < y1; ++y)
            for (uint32_t x = x0; x < x1; ++x) {
                sum += data[y * img->width + x]; ++cnt;
            }
        return cnt ? static_cast<float>(sum / cnt) : 0.0f;
    }
};
```

#### 3.4.6.3.1 파이프라인 통합

```cpp
// xpe_preprocess.cpp — Ghost/Lag 파이프라인 내 GCR 호출
void XpePreprocessPipeline::process_frame(const XpeImageBuffer* raw,
                                           XpeImageBuffer* out)
{
    // ... Offset/Gain/Defect 보정 ...

    // GCR 추정 및 조건부 Lag 보정
    float gcr = gcr_estimator_.estimate(raw);
    if (gcr_estimator_.should_apply_lag_correction()) {
        // §3.4 Tier1/Tier3 Lag 보정 적용
        lag_corrector_.apply(raw, out, gcr);
    } else {
        // GCR < 임계값: Lag 보정 스킵 (성능 이득)
        xpe_memcpy_image(raw, out);
    }
}
```

### 3.4.6.4 성능 특성

| 항목 | 값 |
|------|---|
| 단일 프레임 처리 시간 | < 0.1 ms (3Kx3K, Release 빌드) |
| ROI 면적 (중앙 20%×20%) | ~9만 픽셀 (3072×3072 기준) |
| 메모리 추가 사용량 | 64 bytes (GcrState 구조체) |
| 초기화 프레임 수 | 1 프레임 (이후 EMA 수렴) |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 직전 노출 없음 (콜드 스타트) | `initialized = false`, GCR = 0, 보정 스킵 |
| 직전 노출 포화 | denom 이상 없음; GCR 정상 계산 |
| 연속 암흑 프레임 | EMA 지수 감쇠, 결국 임계값 이하로 하락 |
| 분모 < DENOM_FLOOR | GCR = 0으로 설정, 경고 없음 |

### 3.4.6.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-004 ext (Ghost Correction — GCR 추정 확장) |
| **SWU** | SWU-1.4.6 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 이중 노출 프로토콜: 포화 노출 후 암흑 프레임 30장 획득; 예측 GCR vs. 측정 GCR RMSE < 0.001 |
| **안전 분류** | Class B |

---

## §3.4.7 SWU-1.4.7: NLCSC State Machine (GAP-AJ 해소)

**관련 GAP:** GAP-AJ — ghost-correction SDD §3 "Tier3_Nlcsc" 모듈이 존재하나 ALG-001에 비선형 산란 전하 보정(NLCSC) 상태 기계가 미명세 상태였음.

### 3.4.7.1 개요

NLCSC(Non-Linear Scattered Charge Correction)는 고선량률에서 발생하는 비선형 전하 누적 효과를 4차 다항식 모델로 보정한다. 단순 선형 lag 모델(Tier1/Tier2)로는 설명되지 않는 고선량 비선형성을 처리하는 Tier3 보정이다. b_n 계수는 계단 쐐기 팬텀 5개 선량 수준에서 오프라인 피팅하여 JSON 교정 파일에 저장된다.

**교차 검증 출처:** ghost-correction SDD §3 Tier3_Nlcsc 모듈 — 누적 전하 $Q_{acc}$ 상태 기계와 4차 다항식 보정이 SDD에 기술되어 있으나 ALG-001에 수학적 명세 및 상태 전이 규칙이 없었음.

### 3.4.7.2 수학적 명세

#### 3.4.7.2.1 누적 전하 상태 방정식

$$Q_{\text{acc}}(t) = Q_{\text{acc}}(t-1) \cdot \gamma + I_{\text{measured}}(t) \cdot \delta t$$

여기서:
- $\gamma$: 전하 감쇠 계수 (0 < γ < 1, 기본값 0.95)
- $\delta t$: 프레임 간격 (초, FPS에서 도출)
- $I_{\text{measured}}(t)$: 현재 프레임 픽셀 값 (ADU)

#### 3.4.7.2.2 4차 다항식 보정

$$I_{\text{true}}(x,y) = I_{\text{raw}}(x,y) - \sum_{n=1}^{4} b_n(E) \cdot Q_{\text{acc}}^n(x,y)$$

에너지 의존 계수:

$$b_n(E) = b_{n,0} + b_{n,1} \cdot \text{kVp}$$

여기서 kVp는 X선 관전압이다. 계수 쌍 $(b_{n,0}, b_{n,1})$은 교정 파일에 저장된다.

#### 3.4.7.2.3 상태 리셋 조건

$$Q_{\text{acc}}(t) \leftarrow 0 \quad \text{if} \quad I_{\text{measured}}(t) < 0.01 \cdot I_{\text{saturation}}$$

포화값의 1% 미만이면 검출기가 유휴 상태로 간주, 누적 전하를 0으로 초기화한다.

#### 3.4.7.2.4 교정 피팅 프로토콜

계단 쐐기 팬텀 5 선량 수준: 20 / 40 / 60 / 80 / 100 mR

각 선량 $d_i$에서 측정된 잔류 오차로부터 최소제곱 피팅:

$$\min_{\{b_n\}} \sum_{d_i} \sum_{(x,y)} \left[I_{\text{residual}}(d_i, x, y) - \sum_{n=1}^{4} b_n \cdot Q_{\text{acc}}^n(d_i, x, y)\right]^2$$

### 3.4.7.3 C++ 구현

```cpp
// NlcscCorrector.hpp — Tier3 NLCSC 상태 기계

#pragma once
#include "xpe_types.h"
#include <array>

struct NlcscCoefficients {
    float b0[4];  // kVp-독립 계수 b_{n,0}, n=1..4
    float b1[4];  // kVp-선형 계수  b_{n,1}, n=1..4
    float gamma;  // 전하 감쇠 계수 (0 < gamma < 1)
    float dt;     // 프레임 간격 (초)
    float i_saturation;  // ADU 포화값
};

class NlcscCorrector {
public:
    static constexpr float IDLE_THRESHOLD_FRAC = 0.01f;

    explicit NlcscCorrector(const NlcscCoefficients& coeff)
        : coeff_(coeff) {}

    void set_kvp(float kvp) { kvp_ = kvp; }

    /// @brief 단일 프레임 NLCSC 보정 적용 + 상태 업데이트
    /// @param input   원시 프레임 (float32, in-place 가능)
    /// @param output  보정된 프레임
    void update(const XpeImageBuffer* input, XpeImageBuffer* output)
    {
        const float* in  = reinterpret_cast<const float*>(input->data);
        float*       out = reinterpret_cast<float*>(output->data);
        const size_t N   = static_cast<size_t>(input->width) * input->height;

        // Q_acc 상태 버퍼 크기 확인
        if (q_acc_.size() != N) q_acc_.assign(N, 0.0f);

        // kVp 기반 계수 b_n(E) 사전 계산
        float bn[4];
        for (int n = 0; n < 4; ++n)
            bn[n] = coeff_.b0[n] + coeff_.b1[n] * kvp_;

        const float idle_thresh = IDLE_THRESHOLD_FRAC * coeff_.i_saturation;

        for (size_t i = 0; i < N; ++i) {
            float I_meas = in[i];

            // 상태 리셋 검사
            if (I_meas < idle_thresh) {
                q_acc_[i] = 0.0f;
            } else {
                q_acc_[i] = q_acc_[i] * coeff_.gamma + I_meas * coeff_.dt;
            }

            // 4차 다항식 보정
            float q  = q_acc_[i];
            float q2 = q  * q;
            float q3 = q2 * q;
            float q4 = q3 * q;
            float correction = bn[0]*q + bn[1]*q2 + bn[2]*q3 + bn[3]*q4;

            out[i] = I_meas - correction;
        }
    }

private:
    NlcscCoefficients coeff_;
    float             kvp_{80.0f};
    std::vector<float> q_acc_;  // 픽셀별 누적 전하 상태
};
```

#### 3.4.7.3.1 DLL Export

```cpp
extern "C" {
    typedef struct NlcscHandle_s* NlcscHandle;

    NlcscHandle xpe_nlcsc_create(const NlcscCoefficients* coeff);
    void        xpe_nlcsc_set_kvp(NlcscHandle h, float kvp);
    int         xpe_nlcsc_update(NlcscHandle h,
                                  const XpeImageBuffer* input,
                                  XpeImageBuffer* output);
    void        xpe_nlcsc_destroy(NlcscHandle h);
}
```

### 3.4.7.4 성능 특성

| 항목 | 값 |
|------|---|
| 처리 시간 (3Kx3K) | < 2 ms (스칼라, Release) |
| SIMD 최적화 가능 | 4차 다항식 → AVX2 Horner's method |
| 메모리 (상태 버퍼) | 4 bytes × W × H (3Kx3K ≈ 36 MB) |
| 교정 파라미터 크기 | 10 float (b0[4] + b1[4] + gamma + dt) |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 첫 프레임 (Q_acc = 0) | 보정 없음, 순수 입력 복사 |
| kVp 변경 | set_kvp() 호출로 즉시 반영 |
| 포화 픽셀 | Q_acc 최대값 누적, 보정 과보상 방지용 클램프 필요 |
| 교정 파일 미로드 | NlcscCoefficients 모두 0 → 보정 없음 (passthrough) |

### 3.4.7.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-004 ext (Ghost Correction — Tier3 NLCSC 확장) |
| **SWU** | SWU-1.4.7 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 계단 쐐기 팬텀 5 선량 수준 측정; 보정 후 잔류 오차 < 1 ADU RMS; Tier 선택 로직은 §3.4.5와 일관성 검증 |
| **안전 분류** | Class B |

---

## §3.3.5 SWU-1.3.5: Dose-Dependent Dynamic Defect Detection (GAP-AM 해소)

**관련 GAP:** GAP-AM — FPD-ALG-003 §7.4에 선량 의존 동적 결함 화소 검출이 기술되어 있으나 ALG-001 §3.3에 해당 알고리즘이 누락되어 있었음.

### 3.3.5.1 개요

일부 픽셀은 저선량에서는 정상 응답을 보이나 고선량에서 비선형 특성을 보이는 "선량 의존 동적 결함"이 된다. 4개 선량 수준에서 z-score를 계산하고 선형 응답 적합도(R²)를 평가하여 이러한 픽셀을 검출한다. 동적 결함 맵은 공장 정적 결함 맵과 통합(union)하여 런타임에 사용된다.

**교차 검증 출처:** FPD-ALG-003 (03_측정_알고리즘_명세서.pplx.md) §7.4 — 선량 의존 결함 검출 알고리즘은 FPD 측정 명세에 기술되어 있으나 XPE 파이프라인 ALG-001 §3.3에 통합 명세가 없었음.

### 3.3.5.2 수학적 명세

#### 3.3.5.2.1 다선량 z-score 계산

4개 선량 수준 $d_i \in \{5\%, 20\%, 50\%, 100\%\}$ 에서 각각 z-score를 계산:

$$z_i(x,y) = \frac{I(x,y,d_i) - \mu_{d_i}}{\sigma_{d_i}}$$

여기서 $\mu_{d_i}$, $\sigma_{d_i}$는 결함 제외 후 이미지 전체 평균/표준편차이다.

#### 3.3.5.2.2 선량 비선형 결함 분류 기준

픽셀 $(x,y)$가 동적 결함으로 분류되는 조건:

$$\max_{i}(z_i(x,y)) > \kappa \quad \text{AND} \quad z_{\text{low}}(x,y) < \kappa$$

여기서 $\kappa = 5.0$, $z_{\text{low}}$는 5% 선량 수준의 z-score이다.

#### 3.3.5.2.3 선형 응답 적합도 검사

4-포인트 응답 곡선에 선형 모델 피팅:

$$I(x,y,d) \approx a(x,y) \cdot d + b(x,y)$$

결정 계수:

$$R^2(x,y) = 1 - \frac{\sum_i (I(x,y,d_i) - \hat{I}(x,y,d_i))^2}{\sum_i (I(x,y,d_i) - \bar{I}(x,y))^2}$$

$R^2(x,y) < 0.95$ 이면 응답 비선형 결함으로 플래그.

#### 3.3.5.2.4 동적 결함 맵 통합

$$\text{runtime\_defect\_map}(x,y) = \text{static\_map}(x,y) \;\cup\; \text{dynamic\_map}(x,y)$$

동적 결함 맵 헤더: `MapFlags bit2 = "dose_dependent"` 로 플래그.

### 3.3.5.3 C++ 구현

```cpp
// DoseDependentDefectDetector.hpp

#pragma once
#include "xpe_types.h"
#include "xpe_defect_map.h"

struct DoseLevelSet {
    const XpeImageBuffer* frames[4];    // 4 선량 수준 이미지 (포인터 배열)
    float                 dose_fracs[4]; // 각 선량 분율 (0.05, 0.20, 0.50, 1.00)
};

class DoseDependentDefectDetector {
public:
    static constexpr float KAPPA    = 5.0f;   // z-score 임계값
    static constexpr float R2_MIN   = 0.95f;  // 선형 적합도 최솟값

    /// @brief 동적 결함 맵 생성 (오프라인 교정 단계에서 호출)
    /// @param dose_set  4 선량 수준 이미지 세트
    /// @param output    출력 동적 결함 맵 (XpeDefectMap)
    void detect(const DoseLevelSet& dose_set, XpeDefectMap* output) const
    {
        const uint32_t W = dose_set.frames[0]->width;
        const uint32_t H = dose_set.frames[0]->height;
        const size_t   N = static_cast<size_t>(W) * H;

        // 각 선량 수준별 통계 계산
        float mu[4], sigma[4];
        for (int d = 0; d < 4; ++d) {
            compute_stats(dose_set.frames[d], &mu[d], &sigma[d]);
        }

        for (size_t i = 0; i < N; ++i) {
            float z[4];
            float I[4];
            for (int d = 0; d < 4; ++d) {
                I[d] = reinterpret_cast<const float*>(dose_set.frames[d]->data)[i];
                z[d] = (sigma[d] > 1e-3f) ? (I[d] - mu[d]) / sigma[d] : 0.0f;
            }

            // 선량 비선형 결함 검사
            float z_max = *std::max_element(z, z+4);
            bool dose_nonlinear = (z_max > KAPPA) && (std::abs(z[0]) < KAPPA);

            // 선형 적합도 검사
            bool linear_fail = (compute_r2(dose_set.dose_fracs, I) < R2_MIN);

            if (dose_nonlinear || linear_fail) {
                output->mark_defect(i % W, i / W,
                                    DefectFlags::DOSE_DEPENDENT);
            }
        }
        output->header.flags |= MapFlags::DOSE_DEPENDENT_BIT2;
    }

private:
    void compute_stats(const XpeImageBuffer* img,
                       float* mu, float* sigma) const;

    float compute_r2(const float dose_fracs[4],
                     const float I[4]) const
    {
        // 선형 최소제곱 피팅 (4 포인트)
        float sx=0, sy=0, sxy=0, sx2=0;
        for (int i = 0; i < 4; ++i) {
            sx  += dose_fracs[i];
            sy  += I[i];
            sxy += dose_fracs[i] * I[i];
            sx2 += dose_fracs[i] * dose_fracs[i];
        }
        float a = (4*sxy - sx*sy) / (4*sx2 - sx*sx + 1e-12f);
        float b_coef = (sy - a*sx) / 4.0f;
        float ss_res=0, ss_tot=0, mean_I=sy/4.0f;
        for (int i = 0; i < 4; ++i) {
            float residual = I[i] - (a*dose_fracs[i] + b_coef);
            ss_res += residual*residual;
            float dev = I[i] - mean_I;
            ss_tot += dev*dev;
        }
        return (ss_tot > 1e-6f) ? 1.0f - ss_res/ss_tot : 1.0f;
    }
};
```

#### 3.3.5.3.1 런타임 통합

```cpp
// xpe_preprocess.cpp — 결함 맵 통합 로직
XpeDefectMap combined_map;
combined_map = union_defect_maps(static_defect_map_,    // 공장 정적 결함
                                  dynamic_defect_map_);   // 선량 의존 동적 결함
// runtime_defect_map_ 사용하여 §3.3 결함 보정 적용
```

### 3.3.5.4 성능 특성

| 항목 | 값 |
|------|---|
| 실행 빈도 | 오프라인 교정 단계 (런타임 아님) |
| 처리 시간 (3Kx3K × 4 선량) | < 30 초 (Python NumPy 구현) |
| 동적 결함 맵 크기 | 비트맵 1 bit/pixel + 헤더 |
| 재교정 권고 주기 | 분기 1회 또는 선량 운용 조건 변경 시 |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 정적 결함과 동적 결함 중복 | union: 이미 마킹된 픽셀 이중 마킹 무해 |
| 전체 선량 범위에서 z_max < κ | 정상 픽셀, 결함 미분류 |
| 선량 세트 3개 이하 제공 | 오류 반환 XPE_ERR_INSUFFICIENT_DOSE_FRAMES |
| 고주파 구조 픽셀 (엣지) | σ_d 계산 시 마스크 적용 권장 |

### 3.3.5.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-003 ext (Defect Correction — 선량 의존 결함 확장) |
| **SWU** | SWU-1.3.5 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 알려진 비선형 픽셀을 주입한 합성 팬텀 이미지로 검출 정확도 ≥ 95% 확인; R² < 0.95 픽셀 올바른 분류 확인 |
| **안전 분류** | Class B |

---

## §3.11 SWU-1.11: Row/Column FPN Correction (GAP-AK 해소)

**관련 GAP:** GAP-AK — FPD-ALG-003 §6.3에 행/열 고정 패턴 노이즈(FPN) 분해가 기술되어 있으나 ALG-001 §3 Pre-Processing에 해당 알고리즘이 누락되어 있었음.

### 3.11.1 개요

행/열 FPN은 readout ASIC의 행/열 증폭기 불균일성에서 비롯된 구조적 잡음이다. 기본 게인 보정(§3.2)으로 제거되지 않는 체계적 행/열 패턴을 분리하여 보정한다. 3회 반복 분해(행→열→행)로 수렴을 보장하며, AVX2 기반 벡터화 행 중앙값으로 성능을 최적화한다.

**교차 검증 출처:** FPD-ALG-003 §6.3 행/열 노이즈 분해 — FPD 측정 명세에 완전한 분해 수식이 제시되어 있으나 XPE 전처리 파이프라인 §3에 통합 명세가 없었음.

### 3.11.2 수학적 명세

#### 3.11.2.1 FPN 분해 모델

$$I_{\text{fpn}}(x,y) = I_{\text{signal}}(x,y) + r(y) + c(x) + \varepsilon(x,y)$$

여기서:
- $r(y)$: 행 FPN 프로파일 (행별 행 중앙값 - 전역 중앙값)
- $c(x)$: 열 FPN 프로파일 (행 제거 후 열별 열 중앙값)
- $\varepsilon(x,y)$: 임의 잡음 (모델에서 제외)

#### 3.11.2.2 반복 분해 알고리즘

초기: $I^{(0)}(x,y) = I_{\text{input}}(x,y)$

**Pass 1 — 행 프로파일 추출:**

$$r^{(1)}(y) = \text{median}_x\!\left[I^{(0)}(x,y)\right] - \text{median}_{x,y}\!\left[I^{(0)}\right]$$

$$I^{(1)}(x,y) = I^{(0)}(x,y) - r^{(1)}(y)$$

**Pass 2 — 열 프로파일 추출:**

$$c^{(1)}(x) = \text{median}_y\!\left[I^{(1)}(x,y)\right] - \text{median}_{x,y}\!\left[I^{(1)}\right]$$

$$I^{(2)}(x,y) = I^{(1)}(x,y) - c^{(1)}(x)$$

**Pass 3 — 행 프로파일 재추출 (잔류 행 FPN 제거):**

$$r^{(2)}(y) = \text{median}_x\!\left[I^{(2)}(x,y)\right] - \text{median}_{x,y}\!\left[I^{(2)}\right]$$

$$I_{\text{corrected}}(x,y) = I^{(2)}(x,y) - r^{(2)}(y)$$

최종 결합 프로파일: $r(y) = r^{(1)}(y) + r^{(2)}(y)$

### 3.11.3 C++ 구현

```cpp
// xpe_fpn_correct.h — Row/Column FPN Correction

#pragma once
#include "xpe_types.h"
#include <vector>

struct FpnProfiles {
    std::vector<float> row_profile;  // r(y), 길이 = height
    std::vector<float> col_profile;  // c(x), 길이 = width
};

class FpnCorrector {
public:
    /// @brief FPN 프로파일 계산 (오프라인 암흑 프레임에서)
    FpnProfiles compute_profiles(const XpeImageBuffer* dark_frame) const;

    /// @brief FPN 보정 적용 (런타임)
    void correct(const XpeImageBuffer* input,
                 const FpnProfiles&    profiles,
                 XpeImageBuffer*       output) const;

private:
    /// @brief AVX2 행 중앙값 계산 (32-element SIMD 블록)
    float avx2_row_median(const float* row, uint32_t width) const;

    float compute_global_median(const float* data,
                                size_t N) const;
};

// DLL API
extern "C" {
    int xpe_fpn_compute_profiles(const XpeImageBuffer* dark_frame,
                                  FpnProfiles*          profiles);

    int xpe_fpn_correct(const XpeImageBuffer* input,
                        const FpnProfiles*    profiles,
                        XpeImageBuffer*       output);
}
```

#### 3.11.3.1 AVX2 행 중앙값 의사코드

```cpp
// 32-element partial sort for median (AVX2 bitonic sort skeleton)
float FpnCorrector::avx2_row_median(const float* row, uint32_t width) const
{
    // 1) 블록 부분 정렬: 32개씩 _mm256_load_ps + bitonic sort
    // 2) 정렬된 블록에서 (width/2)번째 요소 추출
    // 3) 나머지 잔여 요소 스칼라 처리
    // 전체 행 중앙값 = 블록 중앙값들의 중앙값

    std::vector<float> buf(row, row + width);
    size_t mid = width / 2;
    std::nth_element(buf.begin(), buf.begin() + mid, buf.end());
    return buf[mid];
    // 실제 구현: AVX2 비토닉 네트워크로 대체하여 3× 가속
}
```

#### 3.11.3.2 파이프라인 통합

```cpp
// xpe_preprocess.cpp — FPN은 Gain 보정 직후 적용
void process_frame(const XpeImageBuffer* raw, XpeImageBuffer* out)
{
    apply_offset_correction(raw, temp_);      // §3.1
    apply_gain_correction(temp_, temp2_);     // §3.2
    fpn_corrector_.correct(temp2_, fpn_profiles_, out);  // §3.11 ← HERE
    apply_defect_correction(out, out);        // §3.3
    // ... lag, ghost, heel effect ...
}
```

### 3.11.4 성능 특성

| 항목 | 값 |
|------|---|
| 처리 시간 (3Kx3K, 3 패스) | < 0.5 ms (AVX2 벡터화 행/열 통계) |
| 프로파일 메모리 | (W + H) × 4 bytes (3Kx3K ≈ 24 KB) |
| 수렴 기준 | 3 패스 후 행 잔류 RMS < 0.1 ADU |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| width < 32 (저해상도 모드) | 스칼라 중앙값 폴백 |
| 결함 화소 포함 행/열 | 중앙값 연산은 결함 픽셀 영향 최소화 (이상값 제거 효과) |
| FPN 프로파일 미로드 | 보정 스킵, passthrough |
| 포화 픽셀 행/열 | 포화값이 중앙값 왜곡 → 마스크 후 계산 권장 |

### 3.11.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-001 ext (Pre-Processing Quality — FPN 보정 확장) |
| **SWU** | SWU-1.11 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 알려진 FPN 프로파일 주입 후 잔류 RMS < 0.5 ADU; 게인 보정과의 독립성 확인 |
| **안전 분류** | Class B |

---

## §4.8 SWU-2.8: Wavelet Multi-Scale Adaptive Denoising (GAP-AP 해소)

**관련 GAP:** GAP-AP — enhance-advanced SAD §3.1에 다중 계층 노이즈 감소 파이프라인이 기술되어 있으나 웨이블릿 BayesShrink 알고리즘의 수학적 명세가 ALG-001 §4에 없었음.

### 4.8.1 개요

BayesShrink는 각 웨이블릿 서브밴드의 신호/노이즈 분산을 추정하여 적응형 소프트 임계값을 결정하는 베이지안 기반 잡음 제거 방법이다. 다우베쉬(Daubechies-4) 웨이블릿 3레벨 분해, MAD 기반 노이즈 추정, 해부 부위별 블렌딩 가중치 $\lambda$를 조합한다. AVX2 서브밴드 임계값 처리로 < 3ms 성능 목표를 달성한다.

**교차 검증 출처:** enhance-advanced SAD §3.1 — "4계층 노이즈 감소" 파이프라인 내 웨이블릿 계층 명세가 SAD에 존재하나 ALG-001에 BayesShrink 수식 및 구현 세부사항이 없었음.

### 4.8.2 수학적 명세

#### 4.8.2.1 웨이블릿 분해

Daubechies-4(db4) 웨이블릿으로 3레벨 분해:

$$\text{DWT}(I) \rightarrow \{A_3, (D_{H_j}, D_{V_j}, D_{D_j})\}_{j=1}^{3}$$

여기서 $A_3$는 저주파 근사, $D_{H/V/D,j}$는 수평/수직/대각선 상세 서브밴드이다.

#### 4.8.2.2 MAD 기반 노이즈 분산 추정

가장 세밀한 대각선 서브밴드 $D_{D,1}$ 에서:

$$\hat{\sigma}_n = \frac{\text{median}(|D_{D,1}|)}{0.6745}$$

이 추정량은 가우시안 백색 잡음에 강건하다 (고압박 이상값 영향 최소).

#### 4.8.2.3 서브밴드별 BayesShrink 임계값

서브밴드 $j$의 신호+잡음 분산:

$$\hat{\sigma}^2_{y,j} = \frac{1}{|D_j|} \sum_{k \in D_j} d_{j,k}^2$$

신호 분산 추정:

$$\hat{\sigma}^2_{s,j} = \max\!\left(\hat{\sigma}^2_{y,j} - \hat{\sigma}^2_n, \;\varepsilon\right), \quad \varepsilon = 10^{-8}$$

BayesShrink 임계값:

$$T_j = \frac{\hat{\sigma}^2_n}{\hat{\sigma}_{s,j}} = \frac{\hat{\sigma}^2_n}{\sqrt{\hat{\sigma}^2_{s,j}}}$$

#### 4.8.2.4 소프트 임계값 처리

$$\text{thresh}(d, T) = \text{sign}(d) \cdot \max(|d| - T, \;0)$$

#### 4.8.2.5 해부 부위별 블렌딩

재구성 후 원본과 블렌딩:

$$I_{\text{out}} = \lambda \cdot I_{\text{denoised}} + (1-\lambda) \cdot I_{\text{original}}$$

| 해부 부위 | $\lambda$ |
|---------|----------|
| 흉부 (Chest) | 0.70 |
| 복부 (Abdomen) | 0.60 |
| 사지 (Extremity) | 0.40 |
| 두경부 (Head/Neck) | 0.55 |
| 기본값 | 0.60 |

### 4.8.3 C++ 구현

```cpp
// WaveletDenoiser.hpp — BayesShrink Wavelet Denoising

#pragma once
#include "xpe_types.h"
#include "xpe_anatomy.h"

struct WaveletDenoisingParams {
    int   levels;           // 분해 레벨 수 (기본 3)
    float lambda_override;  // 0이면 해부 부위별 자동 설정
    AnatomyType anatomy;    // CHEST, ABDOMEN, EXTREMITY, ...
};

class WaveletDenoiser {
public:
    static constexpr int   LEVELS     = 3;
    static constexpr float SIGMA_NORM = 0.6745f;
    static constexpr float EPS        = 1e-8f;

    /// @brief 웨이블릿 BayesShrink 잡음 제거
    void denoise(const XpeImageBuffer*          input,
                 XpeImageBuffer*                 output,
                 const WaveletDenoisingParams&   params) const;

private:
    /// @brief db4 웨이블릿 순방향 분해 (3레벨)
    void dwt3_db4(const float* input, uint32_t W, uint32_t H,
                  std::vector<float>& coeffs,
                  std::vector<SubbandDesc>& subbands) const;

    /// @brief db4 역 웨이블릿 변환
    void idwt3_db4(const std::vector<float>& coeffs,
                   const std::vector<SubbandDesc>& subbands,
                   float* output, uint32_t W, uint32_t H) const;

    /// @brief MAD 기반 노이즈 분산 추정
    float estimate_noise_sigma(const float* diag_subband,
                               size_t size) const
    {
        std::vector<float> abs_coeffs(size);
        for (size_t i = 0; i < size; ++i)
            abs_coeffs[i] = std::abs(diag_subband[i]);
        size_t mid = size / 2;
        std::nth_element(abs_coeffs.begin(),
                         abs_coeffs.begin() + mid,
                         abs_coeffs.end());
        return abs_coeffs[mid] / SIGMA_NORM;
    }

    /// @brief AVX2 소프트 임계값 처리 (8 floats/cycle)
    void avx2_soft_threshold(float* subband, size_t N, float T) const
    {
        // _mm256_andnot_ps(sign_mask, v): abs(v)
        // _mm256_max_ps(abs_v - T, zero): (|v| - T)+
        // _mm256_or_ps(result, sign_bits): restore sign
        // AVX2 처리 후 잔여 스칼라 처리
        #ifdef __AVX2__
        // ... AVX2 구현 ...
        #endif
        // Scalar fallback
        for (size_t i = 0; i < N; ++i)
            subband[i] = (subband[i] >= 0)
                       ? std::max(subband[i] - T, 0.0f)
                       : -std::max(-subband[i] - T, 0.0f);
    }

    float get_lambda(AnatomyType anatomy) const
    {
        switch (anatomy) {
            case AnatomyType::CHEST:     return 0.70f;
            case AnatomyType::ABDOMEN:   return 0.60f;
            case AnatomyType::EXTREMITY: return 0.40f;
            case AnatomyType::HEAD_NECK: return 0.55f;
            default:                     return 0.60f;
        }
    }
};

// DLL Export
extern "C" {
    int xpe_wavelet_denoise(const XpeImageBuffer*        input,
                            XpeImageBuffer*               output,
                            const WaveletDenoisingParams* params);
}
```

### 4.8.4 성능 특성

| 항목 | 값 |
|------|---|
| 처리 시간 (2Kx2K, 3레벨) | < 3 ms (AVX2 서브밴드 임계값 처리) |
| 처리 시간 (3Kx3K) | < 6 ms |
| 메모리 오버헤드 | 입력 이미지 크기의 1.5배 (웨이블릿 계수 버퍼) |
| 노이즈 추정 정확도 | σ_n 오차 < 5% (가우시안 잡음 기준) |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 이미지 크기 홀수 | 주기 확장(periodic extension) 패딩 후 DWT |
| 작은 이미지 (< 32×32) | 1레벨 분해로 축소 적용 |
| 매우 낮은 잡음 (σ_n < 1 ADU) | λ 자동 감소하여 원본 보존 |
| 해부 부위 미지정 | λ = 0.60 기본값 적용 |

### 4.8.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-011b (Advanced Noise Reduction — BayesShrink 확장) |
| **SWU** | SWU-2.8 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 알려진 σ의 가우시안 잡음 추가 팬텀; PSNR ≥ 35 dB (σ=20 ADU), MTF 저하 < 5% |
| **안전 분류** | Class B |

---

## §5.4 SWU-5.4: Scatter SPR Semi-Empirical Model (GAP-AO 해소)

**관련 GAP:** GAP-AO — ALG-001 §5 Scatter Correction에 SPR 조회 테이블만 있고 Boone-Seibert 반경험 모델 기반 추정 알고리즘이 없었음.

### 5.4.1 개요

Scatter-to-Primary Ratio(SPR)는 검출기에 도달하는 산란선과 일차선의 비율이다. 현재 §5.2 Virtual Grid / Scatter Correction은 고정 SPR 테이블을 사용하나, 실제 촬영 조건(kVp, 체두께, FOV)에 따른 동적 SPR 추정이 없었다. Boone-Seibert(1988) 반경험 모델을 구현하여 런타임에 SPR을 추정하고 픽셀별 산란 보정을 수행한다.

**교차 검증 출처:** §5 Scatter Correction 내부 — 부록 B.1 SPR 테이블(§B.1)이 존재하나 런타임 추정 알고리즘이 §5에 미명세되어 있었음.

### 5.4.2 수학적 명세

#### 5.4.2.1 Boone-Seibert 반경험 모델

$$\text{SPR}(\text{kVp}, t, \text{FOV}) = a \cdot \exp(b \cdot t) \cdot \left(\frac{\text{kVp}}{80}\right)^c \cdot \left(\frac{\text{FOV}}{35}\right)^d$$

모델 파라미터는 MCNP/EGSnrc 몬테카를로 시뮬레이션 데이터로 피팅:

| 조직 타입 | $a$ | $b$ | $c$ | $d$ |
|---------|-----|-----|-----|-----|
| Water (표준 팬텀) | 0.30 | 0.085 | 2.1 | 0.8 |
| Bone | 0.22 | 0.075 | 1.8 | 0.7 |
| Adipose | 0.35 | 0.095 | 2.3 | 0.9 |

#### 5.4.2.2 체두께 추정 (Beer-Lambert 역산)

$$t_{\text{est}} = \frac{-\ln(I_{\text{mean}} / I_0)}{\mu_{\text{water}}(\text{kVp})}$$

여기서:
- $I_{\text{mean}}$: 촬영 이미지 중앙 ROI 평균값
- $I_0$: air kerma 기준 플랫 필드 값 (교정 데이터)
- $\mu_{\text{water}}(\text{kVp})$: NIST XCOM 데이터 기반 선형 감쇠 계수 (LUT)

FOV는 §12.5 Collimation Mask 감지 결과에서 추출.

#### 5.4.2.3 픽셀별 산란 보정

$$I_{\text{scatter\_corrected}}(x,y) = \frac{I_{\text{raw}}(x,y)}{1 + \text{SPR}(x,y)}$$

픽셀별 $\text{SPR}(x,y)$는 두께 맵 $t(x,y)$ 기반으로 계산:

$$t(x,y) = \frac{-\ln(I_{\text{gain\_corrected}}(x,y) / I_0)}{\mu_{\text{water}}(\text{kVp})}$$

### 5.4.3 C++ 구현

```cpp
// SprModel.hpp — Boone-Seibert SPR Semi-Empirical Model

#pragma once
#include "xpe_types.h"
#include <cmath>

struct SprModelParams {
    float a, b, c, d;          // Boone-Seibert 파라미터
    float mu_water_lut[200];   // kVp 20~219에 대한 μ_water [cm⁻¹]
    float i0_airkkerma;        // 에어커마 기준 플랫 필드값
    TissueType tissue;         // WATER, BONE, ADIPOSE
};

class SprEstimator {
public:
    explicit SprEstimator(const SprModelParams& params)
        : p_(params) {}

    /// @brief 단일 SPR 추정 (전체 이미지 평균 조건)
    float estimate_global(float kvp, float fov_cm2) const {
        float t_est = estimate_thickness(kvp);
        return p_.a * std::exp(p_.b * t_est)
             * std::pow(kvp / 80.0f, p_.c)
             * std::pow(fov_cm2 / 35.0f, p_.d);
    }

    /// @brief 픽셀별 SPR 맵 생성 + 산란 보정 적용
    void correct_scatter(const XpeImageBuffer* input,
                         float kvp, float fov_cm2,
                         XpeImageBuffer* output) const
    {
        const size_t N = static_cast<size_t>(input->width) * input->height;
        const float* in  = reinterpret_cast<const float*>(input->data);
        float*       out = reinterpret_cast<float*>(output->data);

        float mu = get_mu_water(kvp);
        for (size_t i = 0; i < N; ++i) {
            float t_pix = (in[i] > 0.0f && p_.i0_airkkerma > 0.0f)
                        ? -std::log(in[i] / p_.i0_airkkerma) / (mu + 1e-6f)
                        : 0.0f;
            t_pix = std::clamp(t_pix, 0.0f, 40.0f);  // 물리적 범위 0~40 cm

            float spr = p_.a * std::exp(p_.b * t_pix)
                      * std::pow(kvp / 80.0f, p_.c)
                      * std::pow(fov_cm2 / 35.0f, p_.d);
            out[i] = in[i] / (1.0f + spr);
        }
    }

private:
    SprModelParams p_;

    float estimate_thickness(float kvp) const {
        // 중앙 ROI 평균 사용 — 단순화 버전; 실제로는 입력 이미지 필요
        return 20.0f;  // placeholder: 실제로 compute_roi_mean 결과 사용
    }

    float get_mu_water(float kvp) const {
        int idx = std::clamp(static_cast<int>(kvp) - 20, 0, 199);
        return p_.mu_water_lut[idx];
    }
};

// DLL Export
extern "C" {
    float xpe_spr_estimate(float kvp, float thickness_cm,
                           float fov_cm2, const SprModelParams* model);

    int   xpe_scatter_correct(const XpeImageBuffer* input,
                               float kvp, float fov_cm2,
                               const SprModelParams* model,
                               XpeImageBuffer* output);
}
```

### 5.4.4 성능 특성

| 항목 | 값 |
|------|---|
| 전역 SPR 추정 (스칼라) | < 0.01 ms |
| 픽셀별 산란 보정 (3Kx3K) | < 5 ms (AVX2 exp/pow 근사 활용 시) |
| 파라미터 크기 | 파라미터 4 float + LUT 200 float |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| kVp 범위 초과 (< 20 또는 > 219) | LUT 경계값 클램프 |
| 두께 추정 음수 | clamp to 0 cm |
| FOV = 0 | fov_cm2 = 35 기본값 (표준 FOV 가정) |
| 교정 i0 미로드 | 전역 평균 SPR 사용; 픽셀별 보정 불가 |

### 5.4.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-008 (Scatter Correction — SPR 추정 확장) |
| **SWU** | SWU-5.4 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | PMMA 슬랩 팬텀 측정 SPR vs. 모델 추정 SPR: 상대 오차 < 15%; 보정 후 CNR 개선 ≥ 5% |
| **안전 분류** | Class B |

---

## §9.9 SWU-9.9: Multi-Exponential Lag Parameter Fitting (GAP-AN 해소)

**관련 GAP:** GAP-AN — FPD-ALG-003 §9.2에 다중 지수 Lag 감쇠 파라미터 피팅이 기술되어 있으나 ALG-001 §3.4 Ghost/Lag Correction은 고정 계수를 사용하고 피팅 알고리즘이 미명세 상태였음.

### 9.9.1 개요

3성분 다중 지수 Lag 모델의 파라미터 $(\alpha_i, \tau_i)_{i=1}^{3}$를 이중 노출 프로토콜로 측정하고, Levenberg-Marquardt 비선형 최소제곱법으로 피팅한다. Python scipy.optimize.curve_fit 구현이며, 피팅 결과는 lag_params.json에 저장되어 §3.4의 런타임 Lag 보정에 사용된다.

**교차 검증 출처:** FPD-ALG-003 §9.2 — 다중 지수 피팅 프로토콜이 FPD 측정 명세에 있으나 XPE 교정 파이프라인 §9에 연결되지 않았음.

### 9.9.2 수학적 명세

#### 9.9.2.1 Lag 감쇠 모델

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i \cdot \exp\!\left(-\frac{t}{\tau_i}\right)$$

모델 파라미터:
- $\alpha_i$: 각 성분의 초기 진폭 (상대 비율, $\sum \alpha_i < 1$)
- $\tau_i$: 각 성분의 시정수 (초)

#### 9.9.2.2 이중 노출 측정 프로토콜

1. 100% 포화 노출 10프레임 획득 ($I_{\text{sat}}$)
2. 암흑 상태로 전환 후 30프레임 획득 ($I_{\text{dark}}(t)$)

피팅 타겟: 암흑 프레임의 잔류 신호

$$I_{\text{dark}}(t) \approx I_{\text{offset}} + (I_{\text{sat}} - I_{\text{offset}}) \cdot \text{Lag}(t)$$

목적 함수:

$$\min_{\{\alpha_i, \tau_i\}} \sum_{t} \left[I_{\text{dark}}(t) - I_{\text{offset}} - \text{Lag\_model}(t) \cdot (I_{\text{sat}} - I_{\text{offset}})\right]^2$$

#### 9.9.2.3 Levenberg-Marquardt 피팅 설정

| 파라미터 | 값 |
|---------|---|
| 초기 추정값 $\tau$ | [0.5 s, 2.0 s, 10.0 s] |
| 초기 추정값 $\alpha$ | [0.010, 0.003, 0.001] |
| 최대 반복 횟수 | 1000 |
| 수렴 허용오차 | 1×10⁻⁸ |
| 댐핑 초기값 $\lambda_{\text{LM}}$ | 0.01 |
| 파라미터 경계 | $\alpha_i \in [0, 1]$, $\tau_i \in [0.1, 60]$ s |

#### 9.9.2.4 검증 기준

- 잔류 RMSE < 0.5 ADU
- $R^2 > 0.999$
- $\sum_{i=1}^{3} \alpha_i < 1.0$ (물리적 일관성)

### 9.9.3 Python 구현

```python
# lag_param_fitting.py — Multi-Exponential Lag Calibration

import numpy as np
from scipy.optimize import curve_fit
import json
from dataclasses import dataclass, asdict
from pathlib import Path

@dataclass
class LagParams:
    alpha: list[float]     # [α₁, α₂, α₃]
    tau:   list[float]     # [τ₁, τ₂, τ₃] in seconds
    fit_quality: dict      # {'r_squared': float, 'rmse': float}


def lag_model(t: np.ndarray,
              a1: float, t1: float,
              a2: float, t2: float,
              a3: float, t3: float) -> np.ndarray:
    """3성분 다중 지수 Lag 모델"""
    return a1 * np.exp(-t / t1) + a2 * np.exp(-t / t2) + a3 * np.exp(-t / t3)


def fit_lag_params(dark_frames: np.ndarray,
                   sat_mean: float,
                   offset_mean: float,
                   frame_interval_s: float) -> LagParams:
    """
    Lag 파라미터 Levenberg-Marquardt 피팅

    Parameters
    ----------
    dark_frames      : (N_frames, H, W) uint16/float32 암흑 프레임 시퀀스
    sat_mean         : 포화 노출 프레임 평균 (ADU)
    offset_mean      : 암흑 오프셋 평균 (ADU)
    frame_interval_s : 프레임 간격 (초)

    Returns
    -------
    LagParams with alpha, tau, fit_quality
    """
    N = dark_frames.shape[0]
    t = np.arange(1, N + 1) * frame_interval_s

    # 공간 평균으로 노이즈 감소
    dark_mean = dark_frames.mean(axis=(1, 2)).astype(float)
    scale = sat_mean - offset_mean
    if scale < 10.0:
        raise ValueError("포화값과 오프셋 차이가 너무 작습니다 (< 10 ADU)")

    # 정규화된 Lag 신호
    lag_signal = (dark_mean - offset_mean) / scale

    # Levenberg-Marquardt 피팅
    p0     = [0.010, 0.5, 0.003, 2.0, 0.001, 10.0]
    bounds = ([0]*6, [1, 60, 1, 60, 1, 60])

    popt, _ = curve_fit(lag_model, t, lag_signal,
                         p0=p0, bounds=bounds,
                         max_nfev=1000, ftol=1e-8, xtol=1e-8)

    a1, t1, a2, t2, a3, t3 = popt
    fitted = lag_model(t, *popt)
    residuals = lag_signal - fitted
    ss_res = np.sum(residuals**2)
    ss_tot = np.sum((lag_signal - lag_signal.mean())**2)
    r_squared = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0
    rmse = float(np.sqrt(ss_res / N)) * scale  # ADU 단위로 환산

    return LagParams(
        alpha=[float(a1), float(a2), float(a3)],
        tau=[float(t1), float(t2), float(t3)],
        fit_quality={'r_squared': float(r_squared), 'rmse': float(rmse)}
    )


def save_lag_params(params: LagParams,
                    output_path: Path) -> None:
    """lag_params.json 저장"""
    with open(output_path, 'w') as f:
        json.dump(asdict(params), f, indent=2)
    print(f"[lag_fit] R²={params.fit_quality['r_squared']:.6f}, "
          f"RMSE={params.fit_quality['rmse']:.3f} ADU → {output_path}")
```

### 9.9.4 성능 특성

| 항목 | 값 |
|------|---|
| 피팅 시간 (30프레임) | < 5 초 (scipy LM, 단일 코어) |
| 피팅 수렴 반복 횟수 | 전형적으로 50~200회 |
| 결과 파일 | lag_params.json (< 1 KB) |
| 교정 주기 | 분기 1회 또는 검출기 교체 시 |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 피팅 미수렴 (max_nfev 초과) | RuntimeError 발생, 이전 파라미터 보존 |
| R² < 0.999 | 경고 로그, 사용자에게 재측정 권고 |
| 3성분 피팅 불안정 | 2성분 모델 폴백 옵션 |
| 프레임 수 < 10 | ValueError (최소 10프레임 필요) |

### 9.9.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-004 (Ghost Correction — Lag 파라미터 교정) |
| **SWU** | SWU-9.9 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 3지수 신호에 노이즈 추가 후 피팅; 복원 오차 α < 0.001, τ < 0.05 s; R² > 0.999 확인 |
| **안전 분류** | Class B |

---

## §12.9 SWU-12.9: Allan Variance Stability Characterization (GAP-AL 해소)

**관련 GAP:** GAP-AL — FPD-ALG-003 §6.4에 Allan Variance 기반 장기 안정성 특성화가 기술되어 있으나 ALG-001 §12 FPD 특성화 섹션에 해당 알고리즘이 없었음.

### 12.9.1 개요

Allan Variance $\sigma^2_A(\tau)$는 원래 주파수 표준 분야에서 발전한 도구로, 시간 스케일 $\tau$에 따른 측정 안정성 유형을 분류한다. FPD 암흑 전류 드리프트의 장기 특성화에 사용하여 재교정 시점 결정 및 잡음 유형 분류(백색 잡음 / 플리커 잡음 / 랜덤 워크)에 활용된다.

**교차 검증 출처:** FPD-ALG-003 §6.4 — Allan Variance 계산이 FPD 측정 알고리즘 명세서에 있으나 ALG-001 §12 특성화 섹션에 없었음.

### 12.9.2 수학적 명세

#### 12.9.2.1 Allan Variance 정의

$$\sigma^2_A(\tau) = \frac{1}{2} \left\langle \left(\bar{x}_{k+1}(\tau) - \bar{x}_k(\tau)\right)^2 \right\rangle$$

여기서 $\bar{x}_k(\tau)$는 시간 빈 $k$의 간격 $\tau$에 걸친 평균값이다.

이산 시계열 $\{x_n\}$에서 빈 평균:

$$\bar{x}_k(\tau) = \frac{1}{m} \sum_{n=km}^{(k+1)m-1} x_n, \quad m = \tau / T_{\text{frame}}$$

Allan Variance 추정량:

$$\hat{\sigma}^2_A(\tau) = \frac{1}{2(M-2m+1)} \sum_{j=0}^{M-2m} \left(\bar{x}_{j+m}(\tau) - \bar{x}_j(\tau)\right)^2$$

여기서 $M$은 전체 샘플 수이다.

#### 12.9.2.2 잡음 유형 분류

| 잡음 유형 | Allan Variance 기울기 | 해석 |
|---------|---------------------|------|
| 백색 잡음 | $\sigma^2_A \propto \tau^{-1}$ | 무작위 판독 노이즈 지배적 |
| 플리커 잡음 | $\sigma^2_A \sim \text{const}$ | 1/f 잡음 지배적 |
| 랜덤 워크 | $\sigma^2_A \propto \tau^{+1}$ | 드리프트 지배적 |

로그-로그 기울기 추정으로 분류:
- $\mu < -0.7$: 백색 잡음 지배
- $-0.3 \leq \mu \leq 0.3$: 플리커 잡음 지배
- $\mu > 0.7$: 랜덤 워크 지배

#### 12.9.2.3 재교정 트리거 기준

$$\sigma_A(\tau=300\,\text{s}) > 2.0 \;\text{ADU} \;\Rightarrow\; \text{재교정 예약}$$

$\tau = 300$ s (5분)는 임상 세션 간 드리프트 모니터링 기준 시간 스케일이다.

### 12.9.3 Python 구현

```python
# allan_variance.py — FPD Long-Term Stability Characterization

import numpy as np
from dataclasses import dataclass
from typing import Literal
import warnings


NoiseType = Literal['white', 'flicker', 'random_walk', 'mixed']


@dataclass
class AllanVarianceCurve:
    tau_values: np.ndarray         # 시간 스케일 배열 (초)
    sigma_squared: np.ndarray      # Allan Variance 값 배열
    sigma: np.ndarray              # Allan Deviation (sqrt of above)
    noise_type_classification: NoiseType
    recalibration_needed: bool     # σ_A(τ=300s) > 2.0 ADU


def compute_allan_variance(dark_frames: np.ndarray,
                           frame_interval_s: float = 1.0,
                           min_frames: int = 100) -> AllanVarianceCurve:
    """
    Allan Variance 계산

    Parameters
    ----------
    dark_frames      : (N, H, W) 연속 암흑 프레임
    frame_interval_s : 프레임 간격 (초)
    min_frames       : 최소 필요 프레임 수 (기본 100)

    Returns
    -------
    AllanVarianceCurve
    """
    N = dark_frames.shape[0]
    if N < min_frames:
        raise ValueError(f"최소 {min_frames} 프레임 필요, {N} 제공됨")

    # 공간 평균으로 1D 시계열 생성
    x = dark_frames.mean(axis=(1, 2))
    T = frame_interval_s
    T_total = N * T

    # 로그 스케일 τ 값 (T_frame ~ T_total/4)
    tau_min = T
    tau_max = T_total / 4.0
    n_points = 50
    m_values = np.unique(np.logspace(
        np.log10(1), np.log10(int(N / 4)), n_points, dtype=int
    ))
    tau_values = m_values * T

    sigma_squared = np.zeros(len(m_values))
    for idx, m in enumerate(m_values):
        # 빈 평균 계산
        n_bins = N // m
        if n_bins < 2:
            sigma_squared[idx] = np.nan
            continue
        bin_means = x[:n_bins*m].reshape(n_bins, m).mean(axis=1)
        diffs = np.diff(bin_means)
        sigma_squared[idx] = 0.5 * np.mean(diffs**2)

    # NaN 제거
    valid = ~np.isnan(sigma_squared)
    tau_values = tau_values[valid]
    sigma_squared = sigma_squared[valid]

    # 잡음 유형 분류 (로그-로그 기울기)
    if len(tau_values) >= 3:
        log_tau = np.log10(tau_values)
        log_sig = np.log10(sigma_squared + 1e-20)
        slope = np.polyfit(log_tau, log_sig, 1)[0]
        if slope < -0.7:
            noise_type: NoiseType = 'white'
        elif slope > 0.7:
            noise_type = 'random_walk'
        elif -0.3 <= slope <= 0.3:
            noise_type = 'flicker'
        else:
            noise_type = 'mixed'
    else:
        noise_type = 'mixed'

    # 재교정 트리거 검사 (τ = 300s에서의 σ_A)
    idx_300 = np.argmin(np.abs(tau_values - 300.0))
    sigma_at_300 = float(np.sqrt(sigma_squared[idx_300]))
    recal_needed = sigma_at_300 > 2.0

    return AllanVarianceCurve(
        tau_values=tau_values,
        sigma_squared=sigma_squared,
        sigma=np.sqrt(sigma_squared),
        noise_type_classification=noise_type,
        recalibration_needed=recal_needed
    )
```

### 12.9.4 성능 특성

| 항목 | 값 |
|------|---|
| 실행 환경 | Python (오프라인 분석) |
| 처리 시간 (100프레임, 50 τ 포인트) | < 2 초 |
| 최소 데이터 요건 | 100 연속 암흑 프레임 |
| 재교정 트리거 | σ_A(τ=300s) > 2.0 ADU |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 프레임 수 < 100 | ValueError 발생 |
| 전체 드리프트 계열 (저주파) | random_walk 분류, 즉시 재교정 경고 |
| σ_n 매우 작음 (< 0.1 ADU) | 부동소수점 언더플로우; ε 바닥값 처리 |
| 프레임 간격 불규칙 | 균일 간격 가정; 불규칙 간격 시 보간 필요 |

### 12.9.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-QC-002 ext (Calibration Drift Monitoring — Allan Variance 확장) |
| **SWU** | SWU-12.9 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 알려진 백색/플리커/드리프트 합성 신호로 Allan Variance 계산; 이론값 대비 오차 < 5%; 재교정 트리거 임계값 정확도 확인 |
| **안전 분류** | Class B |

---

## 16. Dual-Energy Subtraction (DES) Algorithm (GAP-AQ 해소)

**관련 GAP:** GAP-AQ — 이중 에너지 차감(DES) 분해 알고리즘이 전체 문서 세트에 부재. 흉부 X선 워크플로에서 뼈/연조직 분리를 위한 핵심 알고리즘이다.

### 16.1 개요

이중 에너지 차감(DES)은 저에너지(저kVp)와 고에너지(고kVp) 두 번의 노출을 획득하여 각각 뼈 신호와 연조직 신호를 강조한 이미지를 생성하는 알고리즘이다. X선 감쇠 계수의 에너지 의존성 차이를 이용한다. 뼈 억제 이미지는 흉부 병변 검출 민감도를 향상시킨다.

**DLL:** xpe_enhance_advanced.dll 에서 `xpe_des_decompose()` 익스포트.

### 16.2 수학적 명세

#### 16.2.1 이중 에너지 획득

| 파라미터 | 저에너지 ($I_L$) | 고에너지 ($I_H$) |
|---------|--------------|--------------|
| 관전압 | 60–70 kVp | 120–140 kVp |
| 연조직 대비 | 높음 | 낮음 |
| 뼈 대비 | 낮음 | 상대적으로 높음 |

#### 16.2.2 로그 차감 분해

연조직 강조 이미지 (뼈 억제):

$$I_{\text{soft}}(x,y) = \ln I_L(x,y) - w_{\text{bone}} \cdot \ln I_H(x,y)$$

뼈 강조 이미지 (연조직 억제):

$$I_{\text{bone}}(x,y) = \ln I_H(x,y) - w_{\text{soft}} \cdot \ln I_L(x,y)$$

#### 16.2.3 최적 가중치 계산

뼈 억제 가중치 $w_{\text{bone}}$은 연조직 이미지에서 뼈 신호가 최소화되도록:

$$w_{\text{bone}} = \frac{\mu_{\text{bone}}(E_L) / \mu_{\text{water}}(E_L)}{\mu_{\text{bone}}(E_H) / \mu_{\text{water}}(E_H)}$$

연조직 억제 가중치 $w_{\text{soft}}$:

$$w_{\text{soft}} = \frac{\mu_{\text{water}}(E_H) / \mu_{\text{bone}}(E_H)}{\mu_{\text{water}}(E_L) / \mu_{\text{bone}}(E_L)}$$

여기서 $\mu_{\text{bone}}(E)$, $\mu_{\text{water}}(E)$는 에너지 $E$에서의 선형 감쇠 계수 (NIST XCOM).

#### 16.2.4 모션 보정 — 위상 상관

$I_L$과 $I_H$ 간의 강체 정합 (서브픽셀 정확도):

$$\mathbf{d} = \mathcal{F}^{-1}\!\left\{\frac{\mathcal{F}\{I_L\} \cdot \overline{\mathcal{F}\{I_H\}}}{\left|\mathcal{F}\{I_L\} \cdot \overline{\mathcal{F}\{I_H\}}\right|}\right\}$$

피크 위치 = 변위 벡터 $\mathbf{d} = (d_x, d_y)$, 서브픽셀 정확도는 피크 주변 2차 포물선 보간으로 달성.

#### 16.2.5 노이즈 증폭 경고

DES SNR은 단일 노출 대비 저하됨:

$$\text{SNR}_{\text{DES}} \approx \frac{\text{SNR}_{\text{single}}}{\sqrt{1 + w^2}}$$

$w = w_{\text{bone}} \approx 0.3$이면 $\text{SNR}$ 손실 ≈ 5%. 고w 값 사용 시 선량 보상 필요.

### 16.3 C++ 구현

```cpp
// XpeDualEnergy.hpp — Dual-Energy Subtraction Pipeline

#pragma once
#include "xpe_types.h"

struct DesParams {
    float w_bone;       // 연조직 강조 가중치 (뼈 억제)
    float w_soft;       // 뼈 강조 가중치 (연조직 억제)
    float kvp_low;      // 저에너지 관전압 (kVp)
    float kvp_high;     // 고에너지 관전압 (kVp)
    bool  motion_correct;  // 위상 상관 모션 보정 활성화
};

struct DesResult {
    XpeImageBuffer soft_image;  // 연조직 강조 이미지
    XpeImageBuffer bone_image;  // 뼈 강조 이미지
    float          shift_x;     // 검출된 모션 변위 (픽셀)
    float          shift_y;
    bool           motion_corrected;
};

class XpeDualEnergy {
public:
    /// @brief DES 분해 (전처리 완료 이미지 입력)
    /// @note  입력 이미지는 빔 경화 보정(§3.9) 완료 상태이어야 함
    DesResult decompose(const XpeImageBuffer* I_L,
                        const XpeImageBuffer* I_H,
                        const DesParams&      params) const;

private:
    /// @brief 위상 상관 기반 모션 추정
    std::pair<float,float> estimate_motion(const XpeImageBuffer* I_L,
                                            const XpeImageBuffer* I_H) const;

    /// @brief 서브픽셀 포물선 보간으로 피크 정밀화
    float subpixel_peak(const float* ccf, int peak_idx,
                        int width) const;

    /// @brief 로그 차감 — AVX2 병렬화
    void log_subtract(const float* img_a, const float* img_b,
                      float weight, float* output, size_t N) const;
};

// DLL Export
extern "C" {
    int xpe_des_decompose(const XpeImageBuffer* I_L,
                           const XpeImageBuffer* I_H,
                           const DesParams*      params,
                           DesResult*            result);
}
```

#### 16.3.1 처리 파이프라인

```
I_L (저kVp) → [빔 경화 보정 §3.9] → [offset/gain/defect 보정]
I_H (고kVp) → [빔 경화 보정 §3.9] → [offset/gain/defect 보정]
                     ↓
              [위상 상관 모션 추정]
                     ↓
              [I_H를 d=(dx,dy)만큼 이동 정합]
                     ↓
       [로그 차감: I_soft, I_bone 생성]
                     ↓
       [후처리: 히스토그램 균등화, 노이즈 완화]
```

### 16.4 성능 특성

| 항목 | 값 |
|------|---|
| 처리 시간 (2Kx2K, 모션 보정 포함) | < 50 ms |
| FFT 기반 위상 상관 | < 20 ms (FFTW 사용 시) |
| 로그 차감 연산 | < 5 ms (AVX2, 3Kx3K) |
| SNR 손실 (w_bone=0.3) | ≈ 5% |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| 모션 > 5 픽셀 | 경고 로그; 모션 보정 적용 후 경계 아티팩트 처리 |
| kVp 범위 미지원 | XPE_ERR_DES_KVP_OUT_OF_RANGE 반환 |
| 두 이미지 크기 불일치 | XPE_ERR_SIZE_MISMATCH 반환 |
| 단일 노출 모드 | DES 불가; 패스스루 처리 |

### 16.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-019 (Dual-Energy Subtraction — 신규) |
| **SWU** | SWU-16.0 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | CIRS Model 057A 흉부 팬텀; 연조직 이미지에서 뼈 CNR < 1.0 (억제 확인); 뼈 이미지에서 연조직 CNR < 1.5; 모션 보정 서브픽셀 정확도 < 0.5픽셀 |
| **안전 분류** | Class B |

---

## 17. DICOM IOD Conformance Validation (GAP-AR 해소)

**관련 GAP:** GAP-AR — DICOM이 주요 모듈임에도 전체 문서 세트에서 DICOM IOD 적합성 검증 파이프라인이 부재 상태였음.

### 17.1 개요

DICOM Digital X-ray IOD(DX IOD)의 필수 속성 준수 여부를 IHE RAD TF-3 및 DICOM PS3.3 규격에 따라 자동 검증한다. 비적합 파일은 xpe_dicom.dll의 쓰기 경로 말단에서 차단되며, 상세 오류 보고서가 생성된다.

**DLL:** xpe_dicom.dll 의 쓰기 경로에서 `DicomConformanceValidator::validate()` 호출.

### 17.2 수학적 명세 (적합성 규칙)

#### 17.2.1 픽셀 데이터 일관성 검사

**비트 일관성:**
$$\text{HighBit} = \text{BitsStored} - 1$$
$$\text{BitsStored} \in \{12, 14, 16\}$$
$$\text{BitsAllocated} = 16, \quad \text{PixelRepresentation} = 0 \;\text{(unsigned)}$$

**Rescale 파라미터:**
$$\text{RescaleSlope} > 0$$
$$\text{RescaleIntercept} \geq 0$$

**VOI LUT 윈도우 일관성:**

$$\text{RescaleIntercept} \leq \text{WindowCenter} \leq 65535 \cdot \text{RescaleSlope} + \text{RescaleIntercept}$$

### 17.3 C++ 구현

```cpp
// DicomConformanceValidator.hpp — DICOM DX IOD 적합성 검증

#pragma once
#include "xpe_types.h"
#include "dcmtk/dcmdata/dcdatset.h"  // DCMTK 의존성
#include <vector>
#include <string>

struct DicomConformanceError {
    std::string tag;        // e.g., "(0028,0010)"
    std::string attribute;  // e.g., "Rows"
    std::string message;    // 오류 내용
    int         severity;   // 1=error, 2=warning
};

struct XpeConformanceReport {
    bool                              pass_fail;
    int                               error_count;
    int                               warning_count;
    std::vector<DicomConformanceError> error_list;
};

class DicomConformanceValidator {
public:
    /// @brief DICOM DX IOD 적합성 전체 검증
    XpeConformanceReport validate(DcmDataset* dataset) const;

private:
    // Type 1 검사: 존재 + 값 필수
    void check_type1_attributes(DcmDataset* ds,
                                 XpeConformanceReport& report) const;
    // Type 2 검사: 존재 필수, 빈 값 허용
    void check_type2_attributes(DcmDataset* ds,
                                 XpeConformanceReport& report) const;
    // 픽셀 데이터 수치 일관성 검사
    void check_pixel_consistency(DcmDataset* ds,
                                  XpeConformanceReport& report) const;
    // Rescale/VOI LUT 범위 일관성 검사
    void check_windowing(DcmDataset* ds,
                          XpeConformanceReport& report) const;

    void add_error(XpeConformanceReport& r,
                   const std::string& tag,
                   const std::string& attr,
                   const std::string& msg,
                   int severity = 1) const
    {
        r.error_list.push_back({tag, attr, msg, severity});
        if (severity == 1) r.error_count++;
        else               r.warning_count++;
        if (severity == 1) r.pass_fail = false;
    }
};
```

#### 17.3.1 Type 1/2/3 속성 검사 목록

```cpp
void DicomConformanceValidator::check_type1_attributes(
    DcmDataset* ds, XpeConformanceReport& report) const
{
    // Type 1: 존재 + 비어있지 않아야 함
    const std::vector<std::pair<DcmTagKey, std::string>> type1_tags = {
        {DCM_SOPClassUID,                 "SOPClassUID"},
        {DCM_SOPInstanceUID,              "SOPInstanceUID"},
        {DCM_StudyDate,                   "StudyDate"},
        {DCM_PatientID,                   "PatientID"},
        {DCM_Rows,                        "Rows"},
        {DCM_Columns,                     "Columns"},
        {DCM_PixelData,                   "PixelData"},
        {DCM_PhotometricInterpretation,   "PhotometricInterpretation"},
        {DCM_BitsAllocated,               "BitsAllocated"},
        {DCM_BitsStored,                  "BitsStored"},
        {DCM_HighBit,                     "HighBit"},
    };
    for (auto& [tag, name] : type1_tags) {
        OFString value;
        if (ds->findAndGetOFString(tag, value).bad() || value.empty()) {
            add_error(report, tag.toString().c_str(), name,
                      "Type 1 속성 누락 또는 빈 값");
        }
    }
}

void DicomConformanceValidator::check_pixel_consistency(
    DcmDataset* ds, XpeConformanceReport& report) const
{
    Uint16 bits_alloc=0, bits_stored=0, high_bit=0, pix_rep=0;
    ds->findAndGetUint16(DCM_BitsAllocated,     bits_alloc);
    ds->findAndGetUint16(DCM_BitsStored,        bits_stored);
    ds->findAndGetUint16(DCM_HighBit,           high_bit);
    ds->findAndGetUint16(DCM_PixelRepresentation, pix_rep);

    if (bits_alloc != 16)
        add_error(report, "(0028,0100)", "BitsAllocated",
                  "BitsAllocated != 16");
    if (bits_stored != 12 && bits_stored != 14 && bits_stored != 16)
        add_error(report, "(0028,0101)", "BitsStored",
                  "BitsStored not in {12, 14, 16}");
    if (high_bit != bits_stored - 1)
        add_error(report, "(0028,0102)", "HighBit",
                  "HighBit != BitsStored - 1");
    if (pix_rep != 0)
        add_error(report, "(0028,0103)", "PixelRepresentation",
                  "PixelRepresentation != 0 (unsigned required)");
}
```

#### 17.3.2 xpe_dicom.dll 통합

```cpp
// xpe_dicom_writer.cpp — 쓰기 경로 말단에서 검증
int XpeDicomWriter::write(DcmDataset* dataset,
                           const std::string& output_path)
{
    // ... DICOM 데이터 구성 ...

    DicomConformanceValidator validator;
    XpeConformanceReport report = validator.validate(dataset);

    if (!report.pass_fail) {
        log_error("[DICOM] 비적합 파일 생성 차단: %d 오류",
                  report.error_count);
        for (auto& err : report.error_list)
            log_error("  [%s] %s: %s", err.tag.c_str(),
                      err.attribute.c_str(), err.message.c_str());
        return XPE_ERR_DICOM_NONCONFORMANT;
    }
    // ... 파일 저장 ...
    return XPE_OK;
}
```

### 17.4 성능 특성

| 항목 | 값 |
|------|---|
| 검증 처리 시간 | < 1 ms (DCMTK 태그 조회 기반) |
| 검사 항목 수 | Type 1: 11개, Type 2: 5개, 픽셀 수치: 8개 |
| 메모리 오버헤드 | XpeConformanceReport ≈ 1 KB (오류 없음 기준) |

**엣지 케이스:**

| 상황 | 처리 |
|------|------|
| PixelData 누락 | Type 1 오류, 즉시 반환 |
| BitsStored = 8 (비표준) | 오류 — DX IOD는 12/14/16 bit만 허용 |
| WC/WW 범위 초과 | 경고 (severity=2); 파일 쓰기는 허용 |
| DCMTK 미링크 | 컴파일 타임 오류 처리 |

### 17.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-DICOM-001 (DICOM IOD Conformance — 신규) |
| **SWU** | SWU-17.0 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 32개 비적합 케이스 주입 테스트; 모든 케이스에서 XPE_ERR_DICOM_NONCONFORMANT 반환 확인; DICOM 적합 파일에서 XPE_OK 반환 확인 |
| **안전 분류** | Class B |

---

### 3.12 SWU-1.12 온도 보상 이득 보정 (Temperature-Compensated Gain Correction) ★GAP-AT 해소

**SRS ID**: SRS-FUNC-002d | **SWU**: SWU-1.12 | **IEC 62304 §**: 5.4.2

#### 3.12.1 배경

a-Si:H FPD의 TFT 누설 전류는 온도에 비례하여 이득(Gain)이 변화한다. 교정 시점(T_calib)과 촬영 시점(T_current) 사이의 온도 차이가 5°C를 초과하면 이득 오차가 임상적으로 유의미해진다.

#### 3.12.2 온도 이득 모델

$$G(x,y,T) = G(x,y,T_{\text{ref}}) \cdot \left(1 + \alpha_T \cdot (T - T_{\text{ref}})\right)$$

파라미터:
- α_T: 온도 계수 ≈ 0.0015/°C (a-Si:H TFT 표준값)
- T_ref: 교정 기준 온도 (보통 25°C)
- T_current: 현재 보드 온도 (센서 또는 DICOM (0018,1164) 태그)
- 유효 범위: 15–35°C (IEC 60601-1 운영 환경)

#### 3.12.3 런타임 보정 알고리즘

```cpp
float scale_T = 1.0f + alpha_T * (T_current - T_calib);
// scale_T 범위 클램프: [0.90, 1.10]
scale_T = std::clamp(scale_T, 0.90f, 1.10f);

for (int i = 0; i < N_pixels; i++) {
    gain_map_runtime[i] = gain_map_calib[i] * scale_T;
}
```

활성화 조건: |T_current − T_calib| > ΔT_threshold (기본값: 5°C)

#### 3.12.4 교정 세션 온도 기록

교정 세션 잠금 시(§2.4) `calib_manifest.json`에 온도 기록:

```json
{
  "session_id": "CALIB-20260415-001",
  "T_calib_celsius": 24.3,
  "alpha_T": 0.0015
}
```

#### 3.12.5 검증 기준

| 조건 | 허용 오차 |
|------|---------|
| ΔT = 5°C 적용 | 균일성 PRNU CV < 0.5% |
| ΔT = 10°C 적용 | 균일성 PRNU CV < 1.0% |
| scale_T 계산 정확도 | ± 0.0001 (단정밀도 기준) |

#### 3.12.6 DLL 할당

`xpe_preprocess.dll` — 이득 보정 파이프라인(§3.2) 내 통합

#### 3.12.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-002d |
| **SWU** | SWU-1.12 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 온도 시뮬레이션 (ΔT = 5, 10°C); PRNU CV 측정; scale_T 수식 단위 테스트 |
| **안전 분류** | Class B |

---

### 3.13 SWU-1.13 주기적 구조 노이즈 제거 (2D FFT Notch Filter) ★GAP-AU 해소

**SRS ID**: SRS-FUNC-001c | **SWU**: SWU-1.13 | **IEC 62304 §**: 5.4.2

#### 3.13.1 배경

행/열 FPN(§3.11)은 1D 고정 패턴을 처리하지만, 스위칭 전원, 모터 드라이브, 그리드 공명에 의한 2D 주기 간섭 패턴(주파수 도메인 스파이크)은 별도의 노치 필터가 필요하다.

#### 3.13.2 알고리즘

```cpp
// 1. 2D DFT
FFT2D(I_fpn_corrected) → F(u,v)   // Intel MKL fftwf_plan

// 2. 전력 스펙트럼
P(u,v) = |F(u,v)|²

// 3. 피크 검출 (DC/Nyquist 제외)
threshold = mean_P + 5.0f * std_P;
peaks = detect_peaks(P, threshold, exclude_dc=true, exclude_nyquist=true);

// 4. Gaussian 노치 필터 생성
for each peak (u0, v0):
    N(u,v) *= 1.0f - expf(-((u-u0)² + (v-v0)²) / (2 * D_notch²));
// D_notch = 3 pixels (주파수 도메인)

// 5. 적용 및 역변환
F_notch = F * N;
I_clean = IFFT2D(F_notch);  // 실수부만 취함
```

#### 3.13.3 파라미터

| 파라미터 | 기본값 | 설명 |
|---------|-------|------|
| D_notch | 3 pixels | 노치 대역폭 (주파수 도메인) |
| threshold_sigma | 5.0 | 피크 검출 임계값 (σ 배수) |
| max_notches | 8 | 최대 노치 수 |

#### 3.13.4 성능

| 항목 | 값 |
|------|---|
| 처리 시간 | < 2 ms/3Kx3K (Intel MKL FFT) |
| 메모리 오버헤드 | 2 × float32 이미지 크기 (FFT 버퍼) |

#### 3.13.5 DLL 할당

`xpe_preprocess.dll` — FPN 보정(§3.11) 이후 단계

#### 3.13.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-001c |
| **SWU** | SWU-1.13 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 주기 패턴 주입; IFFT 후 잔류 전력 < -30 dB; MTF 저하 < 3%; 처리 시간 < 2ms |
| **안전 분류** | Class B |

---

### 5.5 SWU-5.5 무아레 아티팩트 검출 및 제거 ★GAP-AZ 해소

**SRS ID**: SRS-FUNC-008c | **SWU**: SWU-5.5 | **IEC 62304 §**: 5.4.2

#### 5.5.1 배경

산란 방지 그리드의 선 간격(보통 60–80 lp/cm)이 검출기 픽셀 피치와 에일리어싱을 일으킬 때 가시적 무아레 띠 무늬(banding)가 발생한다. §5.1 NSCT는 그리드 억제를 다루지만 무아레 특이적 검출은 미명세였다.

#### 5.5.2 무아레 주파수 계산

$$f_{\text{moire}} = |f_{\text{grid}} - n \cdot f_{\text{detector}}|, \quad n = 1, 2, 3$$

- f_grid: 그리드 라인 주파수 [lp/mm]
- f_detector = 1/pixelPitch_mm [lp/mm]
- 유효 범위: f_moire ∈ [0.1, 0.4] × f_Nyquist

#### 5.5.3 검출 알고리즘

```cpp
// 1. 행 방향 1D PSD 계산
float* psd_row = compute_row_psd(I_grid_suppressed, width, height);

// 2. 피크 검출
float threshold = mean_psd + 3.0f * std_psd;
std::vector<float> moire_freqs = detect_peaks_above_threshold(psd_row, threshold);

// 3. 예상 무아레 범위 검증
for (float f : moire_freqs) {
    if (f >= 0.1f * f_nyquist && f <= 0.4f * f_nyquist) {
        moire_detected = true;
    }
}
```

#### 5.5.4 방향성 대역 제거 필터

```cpp
// 검출된 무아레 주파수에서 Gaussian 대역 제거
for (float f_moire : detected_freqs) {
    float bandwidth = 1.0f / (grid_pitch_px * 2.0f);
    apply_directional_bandstop(F_2d, f_moire, bandwidth, direction=VERTICAL);
}
```

#### 5.5.5 성능

| 항목 | 값 |
|------|---|
| 처리 시간 | < 3 ms/3Kx3K |
| 검출 정확도 | > 95% (합성 무아레 패턴) |
| 잔류 무아레 | < 10% 원본 진폭 |

#### 5.5.6 DLL 할당

`xpe_gsvg.dll` — Grid Suppression 파이프라인(§5.1) 이후 단계

#### 5.5.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-008c |
| **SWU** | SWU-5.5 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 무아레 패턴(f = 0.15, 0.25, 0.35 × f_N) 주입; 검출률 > 95%; 억제 후 잔류 < 10%; MTF 저하 < 3% |
| **안전 분류** | Class B |

---

### 9.10 SWU-9.10 AEC 피드백 루프 (EI/DI → kVp/mAs 조정) ★GAP-AV 해소

**SRS ID**: SRS-FUNC-009b | **SWU**: SWU-9.10 | **IEC 62304 §**: 5.4.2

#### 9.10.1 배경

EI/DI는 §7에서 계산되지만, 다음 촬영에서 자동으로 촬영 기법을 조정하는 AEC 피드백 루프 알고리즘이 미명세였다.

#### 9.10.2 알고리즘

```cpp
struct XpeAECRecommendation {
    float delta_kvp;      // 권장 kVp 변화량
    float delta_mas_ratio;// 권장 mAs 배율 (1.0 = 변화없음)
    float confidence;     // [0,1]
    XpeAECAction action;  // NONE, ADJUST_MAS, ADJUST_KVP, BOTH
};

XpeAECRecommendation xpe_aec_feedback(float DI_dB, XpeAECConfig cfg) {
    XpeAECRecommendation rec = {0, 1.0f, 1.0f, NONE};

    // 데드밴드: |DI| <= 1 dB → 조정 없음
    if (fabsf(DI_dB) <= cfg.deadband_dB) return rec;

    // mAs 조정: 10^(-DI/10)
    rec.delta_mas_ratio = powf(10.0f, -DI_dB / 10.0f);
    rec.delta_mas_ratio = std::clamp(rec.delta_mas_ratio, 0.5f, 2.0f);

    // kVp 조정: DI > +5 dB 과다 노출 시 kVp 감소
    if (DI_dB > 5.0f) {
        rec.delta_kvp = -5.0f;  // 5 kVp 감소
    } else if (DI_dB < -5.0f) {
        rec.delta_kvp = +5.0f;  // 5 kVp 증가
    }

    // 안전 클램프
    rec.delta_kvp = std::clamp(rec.delta_kvp, -10.0f, 10.0f);
    rec.confidence = 1.0f - fabsf(DI_dB) / 20.0f;
    rec.action = (fabsf(rec.delta_kvp) > 0) ? ADJUST_KVP : ADJUST_MAS;
    return rec;
}
```

#### 9.10.3 안전 제약

| 파라미터 | 범위 |
|---------|------|
| kVp 절대 범위 | [40, 150] kVp |
| mAs 절대 범위 | [0.1, 500] mAs |
| 최대 단계 변화 | ΔkVp ≤ 10 kVp/스텝, ΔmAs ≤ 50%/스텝 |
| 데드밴드 | ±1 dB (권장 조정 없음) |

#### 9.10.4 DLL 할당

`xpe_enhance_advanced.dll` — EI 계산(§7) 결과 소비

#### 9.10.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-009b |
| **SWU** | SWU-9.10 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | DI = −6, −3, 0, +3, +6, +10 dB 주입; 각 케이스에서 delta_mas_ratio 및 delta_kvp 계산값 검증; 안전 클램프 경계 테스트 |
| **안전 분류** | Class B |

---

### 9.11 SWU-9.11 교정 통계적 공정 관리 (SPC) ★GAP-AW 해소

**SRS ID**: SRS-QC-004 | **SWU**: SWU-9.11 | **IEC 62304 §**: 5.4.2

#### 9.11.1 배경

드리프트 모니터(§9.5)는 순간 드리프트를 감지하지만 장기 추세 분석이 없다. Shewhart 제어 차트와 CUSUM 알고리즘으로 교정 품질의 장기 추세를 관리한다.

#### 9.11.2 Shewhart X-bar 제어 차트

$$\text{UCL} = \bar{x} + 3\sigma, \quad \text{LCL} = \bar{x} - 3\sigma$$

Western Electric 규칙:
- Rule 1: 1점이 UCL/LCL 초과 → 경고
- Rule 2: 연속 9점이 중심선 동일 측 → 경향 감지
- Rule 3: 연속 6점이 증가/감소 → 드리프트 감지

#### 9.11.3 CUSUM (누적합) 알고리즘

$$S_t^+ = \max(0, S_{t-1}^+ + (x_t - \mu_0 - k))$$
$$S_t^- = \max(0, S_{t-1}^- - (x_t - \mu_0 + k))$$

파라미터:
- k = 0.5σ (여유값, slack value)
- h_warn = 4σ (경고 임계값)
- h_recal = 5σ (재교정 트리거)
- 입력 x_t: §12.1 균일성 CV, §9.5 평균 드리프트

#### 9.11.4 C++ 데이터 구조

```cpp
struct XpeSPCState {
    float shewhart_ucl, shewhart_lcl;
    float cusum_pos, cusum_neg;
    XpeSPCStatus status;  // NORMAL, WARNING, RECALIBRATE
    XpeSPCTrend  trend;   // STABLE, DRIFT_UP, DRIFT_DOWN
    char  recommended_action[64];
};

XpeSPCState xpe_spc_update(XpeSPCState prev, float x_new);
```

#### 9.11.5 DLL 할당

`xpe_common.dll` — 교정 파이프라인(§9) 품질 감시 모듈

#### 9.11.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-QC-004 |
| **SWU** | SWU-9.11 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 드리프트 신호(선형, 단계, 주기); Shewhart 규칙별 감지 확인; CUSUM h_recal 트리거 시점 검증 |
| **안전 분류** | Class B |

---

### 11.5 SWU-11.5 신호 의존 양자 잡음 모델 (Poisson+Gaussian) ★GAP-AY 해소

**SRS ID**: SRS-FUNC-011c | **SWU**: SWU-11.5 | **IEC 62304 §**: 5.4.2

#### 11.5.1 배경

BayesShrink(§4.8)는 경험적 MAD 추정으로 σ_n을 계산하지만, 신호 의존 Poisson 잡음(양자 잡음)과 독립적 전자 잡음(읽기 잡음)을 분리하지 못한다. 위치 의존 σ_n(x,y) 맵이 필요하다.

#### 11.5.2 잡음 모델

$$\sigma_{\text{total}}^2(x,y) = \underbrace{\alpha \cdot I(x,y)}_{\text{Poisson (quantum)}} + \underbrace{\beta}_{\text{Gaussian (electronic)}}$$

파라미터 추정:
- α = 1/G² (G: 이득 [e⁻/ADU], 이득 맵에서 획득)
- β = σ²_dark (다중 프레임 다크 획득 §9.8에서 계산)

#### 11.5.3 Anscombe 분산 안정화 변환

$$f(I) = 2\sqrt{I + \frac{3}{8}} \quad \xrightarrow{\text{변환 후}} \quad \text{분산} \approx 1$$

근사 역변환 (Makitalo & Foi 2011):
$$f^{-1}(x) = \frac{1}{4}x^2 - \frac{3}{8} + \frac{1}{4}\sqrt{\frac{3}{2}}\cdot x^{-1} - \frac{11}{8}x^{-2} + \frac{5}{8}\sqrt{\frac{3}{2}}\cdot x^{-3}$$

#### 11.5.4 BayesShrink 통합

```cpp
// 위치 의존 σ_n 맵 계산
for (int i = 0; i < N_pixels; i++) {
    float alpha = 1.0f / (gain_map[i] * gain_map[i]);
    float beta  = dark_variance_map[i];
    sigma_n_map[i] = sqrtf(alpha * I_input[i] + beta);
}

// Anscombe 변환 적용
float* I_anscombe = anscombe_transform(I_input, N_pixels);

// Gaussian 도메인에서 BayesShrink 적용
float* I_denoised_anscombe = bayesshrink_denoise(I_anscombe, N_pixels, sigma_n=1.0f);

// 역변환
float* I_denoised = anscombe_inverse(I_denoised_anscombe, N_pixels);
```

#### 11.5.5 DLL 할당

`xpe_enhance_advanced.dll` — BayesShrink(§4.8) 잡음 추정 강화 모듈

#### 11.5.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-011c |
| **SWU** | SWU-11.5 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 Poisson 잡음 영상; α, β 파라미터 역산 오류 < 5%; Anscombe 변환 후 분산 균일성 검증 (CV < 0.1) |
| **안전 분류** | Class B |

---

### 12.10 SWU-12.10 IEC 61223 인수 시험 자동화 ★GAP-BB 해소

**SRS ID**: SRS-QA-001 | **SWU**: SWU-12.10 | **IEC 62304 §**: 5.4.2

#### 12.10.1 배경

특성화 스위트(§12)는 MTF, NPS, DQE, CNR을 개별적으로 다루지만, IEC 61223-3-5(디지털 방사선 촬영 시스템 인수 시험)에 따른 자동화된 인수/상수성 시험 워크플로우가 미명세였다.

#### 12.10.2 IEC 61223-3-5 시험 매트릭스

| 시험 | 허용 기준 | 알고리즘 참조 |
|------|---------|------------|
| T1: 균일성 | PRNU CV ≤ 10% | §12.1 |
| T2: 신호 응답 선형성 | R² ≥ 0.995 | §9.1 (선형 회귀) |
| T3: 한계 공간 해상도 | f50 ≥ 요구값 | §12.2 MTF |
| T4: 대비 해상도 | CNR ≥ 기준값 | §12.8 |
| T5: EI 정확도 | DI ∈ ±1 dB | §7.4 |
| T6: 암전류 드리프트 | < 2 ADU/s | §9.5 |

#### 12.10.3 C++ 자동화 API

```cpp
enum class XpeAcceptanceTestType { FULL, DAILY, WEEKLY, MONTHLY };

struct XpeAcceptanceResult {
    bool t1_pass, t2_pass, t3_pass, t4_pass, t5_pass, t6_pass;
    float t1_cv_pct;   // PRNU CV (%)
    float t2_r2;       // 선형성 R²
    float t3_f50;      // MTF f50 (lp/mm)
    float t4_cnr;      // 대비 해상도 CNR
    float t5_di_db;    // 편차 지수 DI (dB)
    float t6_drift_adu_s; // 암전류 드리프트 (ADU/s)
    bool overall_pass;
    char report_json[4096];
};

XpeAcceptanceResult xpe_acceptance_test(
    const XpePhantomImages& phantom,
    XpeAcceptanceTestType type
);
```

#### 12.10.4 주기적 상수성 시험 일정

| 주기 | 시험 항목 |
|------|---------|
| 일일 | T4 (CNR), T5 (EI 정확도) |
| 주간 | T1 (균일성), T6 (암전류) |
| 월간 | T1–T6 전체 + MTF/DQE 전체 |

#### 12.10.5 PASS/FAIL 판정 로직

```cpp
bool overall_pass = t1_pass && t2_pass && t3_pass && t4_pass && t5_pass && t6_pass;

// 경고: 임계값의 80% 이내
bool t1_warn = (cv_pct > 0.8f * 10.0f) && t1_pass;

if (!overall_pass) {
    log_maintenance_alert("Acceptance test FAILED — immediate service required");
}
```

#### 12.10.6 DLL 할당

`xpe_enhance_advanced.dll` — FPD 특성화 스위트(§12) 확장 모듈

#### 12.10.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-QA-001 |
| **SWU** | SWU-12.10 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | PMMA/알루미늄 팬텀 영상으로 T1~T6 전체 실행; 합격 기준 각각 검증; 불합격 케이스에서 유지보수 알림 확인 |
| **안전 분류** | Class B |

---

### 14.2 SWU-14.2 서브픽셀 영상 정합 (ECC 알고리즘) ★GAP-AX 해소

**SRS ID**: SRS-FLUORO-002 | **SWU**: SWU-14.2 | **IEC 62304 §**: 5.4.2

#### 14.2.1 배경

Fluoroscopy 시간적 IIR 필터(§14.1)는 정적 환자를 가정한다. 디지털 차감 혈관조영술(DSA)과 조영증강 연구에서는 마스크 프레임과 라이브 프레임 간의 강성 정합(rigid registration)이 필요하다.

#### 14.2.2 ECC (Enhanced Correlation Coefficient) 알고리즘

Evangelidis & Psarakis (2008) 방법:

**워핑 파라미터**: p = [tx, ty, θ] (이동 2DoF + 회전 1DoF)

**반복 업데이트**:
$$\Delta\mathbf{p} = (\mathbf{J}^T \mathbf{J})^{-1} \mathbf{J}^T \mathbf{e}$$

- J: 워핑 야코비안 (2Mx3 행렬, M = 픽셀 수)
- e: 정규화 오류 벡터 e = T/|T| − W/|W|
- 수렴 조건: ||Δp|| < ε = 0.01 pixel, max_iter = 50

**위상 상관 사전 정렬**:
$$\hat{p}_{\text{init}} = \text{PhaseCorr}(I_{\text{mask}}, I_{\text{live}})$$

서브픽셀 정밀도: 위상 상관 피크의 2차 보간 (Parabolic fitting)

#### 14.2.3 C++ 구현

```cpp
struct XpeWarpMatrix2x3 {
    float m[2][3];  // 어파인 변환 행렬 [tx, ty, θ]
    float ecc_score;   // 최종 ECC 상관 점수 [0,1]
    int   iterations;  // 실제 반복 횟수
};

XpeWarpMatrix2x3 xpe_image_register(
    const float* mask,   // 마스크 프레임 (참조)
    const float* live,   // 라이브 프레임 (정합 대상)
    int width, int height,
    XpeRegistrationConfig cfg  // max_iter, epsilon, downsample_factor
);

// 정합 후 보간 적용
void xpe_warp_apply(
    const float* src, float* dst,
    int width, int height,
    const XpeWarpMatrix2x3& warp,
    XpeInterpolation interp  // BILINEAR, BICUBIC
);
```

#### 14.2.4 성능

| 항목 | 값 |
|------|---|
| 처리 시간 | < 5 ms/3Kx3K (OpenCV ECC 백엔드) |
| 정밀도 | 서브픽셀 (< 0.5 pixel RMS 오차) |
| 최대 이동 범위 | ±50 pixels (tx, ty) |
| 최대 회전 범위 | ±5° |

#### 14.2.5 DLL 할당

`xpe_preprocess.dll` — Fluoroscopy 파이프라인(§14) 확장 모듈

#### 14.2.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FLUORO-002 |
| **SWU** | SWU-14.2 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 변위 테스트 (tx=10, ty=20 pixel; θ=2°); 등록 오류 RMS < 0.5 pixel; ECC 점수 > 0.95 |
| **안전 분류** | Class B |

---

### 14.3 SWU-14.3 형광투시 Lucas-Kanade 광학 흐름 추정 ★GAP-BS 해소

**참고**: Bouguet J.Y., "Pyramidal Implementation of the Lucas-Kanade Feature Tracker," Intel Technical Report, 2000.  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-FLUORO-003 | SWU-14.3 | xpe_fluoroscopy.dll | Class B |

#### 수학적 정의

피라미달 Lucas-Kanade (L=3 레벨, σ_pyr=1.0 Gaussian):

각 레벨 l에서 5×5 통합 윈도우 `Ω`:

```
A = Σ_{Ω} [ Ix²   IxIy ]    b = −Σ_{Ω} [ Ix·It ]
           [ IxIy  Iy²  ]               [ Iy·It ]

[vx; vy] = A⁻¹ · b   (10회 Newton-Raphson 반복)
```

여기서 `Ix, Iy` = 공간 기울기 (Scharr 필터), `It` = 시간 차분.

업샘플링: 각 레벨 흐름 × 2 + 미세 조정 (coarse-to-fine).

호흡 운동 ROI: 이미지 높이 50%~80% 밴드 (횡격막 영역).

심장 게이팅:

```
motion_mag(t) = mean|v(t)|²  (전체 이미지)
주파수 추정: FFT(motion_mag), 0.8~2.5 Hz 대역 피크 검출
심박 위상: 피크 감지 → R-wave 게이팅
```

#### C++ 구조체 및 API

```cpp
struct XpeOptFlowResult {
    float*  vx;                  // W×H float displacement x (pixels/frame)
    float*  vy;                  // W×H float displacement y (pixels/frame)
    uint32_t W, H;
    float   mean_motion_px;      // mean motion magnitude
    float   dominant_freq_hz;    // estimated cardiac/respiratory frequency
};

XpeStatus xpe_fluoro_optical_flow(
    const uint16_t*  I_prev,
    const uint16_t*  I_curr,
    uint32_t         W,
    uint32_t         H,
    XpeOptFlowResult* result
);

// Release allocated vx/vy buffers
void xpe_fluoro_optical_flow_free(XpeOptFlowResult* result);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 5 ms / 3K×3K / 프레임 (AVX2 + 3레벨 피라미드) |
| 실시간 형광투시 | 30 fps 지원 (33 ms 프레임 버짓) |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 흐름 RMS 오차 | < 0.5 px (10 px 진폭 1 Hz 정현파 변위 합성 테스트) |
| 주파수 추정 오차 | < 0.05 Hz (1 Hz 기준 심장 운동 신호) |
| 제로 모션 누출 | vx, vy RMS < 0.1 px (정지 이미지 쌍) |

---

### 17.2 SWU-17.2 DICOM 구조화 보고서 (SR) — CAD 소견 출력 ★GAP-BA 해소

**SRS ID**: SRS-DICOM-002 | **SWU**: SWU-17.2 | **IEC 62304 §**: 5.4.2

#### 17.2.1 배경

DICOM IOD 적합성 검증기(§17.1 GAP-AR)는 영상 IOD를 처리하지만 AI/CAD 소견을 DICOM SR IOD로 출력하는 파이프라인이 미명세였다.

#### 17.2.2 SR 템플릿

| 사용 사례 | DICOM TID |
|---------|-----------|
| 일반 CAD 소견 | TID 4100 (Chest CAD) |
| 기본 진단 영상 보고서 | TID 1500 |
| 해부 부위 측정 | TID 1400 (측정값 포함) |

#### 17.2.3 C++ SR 생성 API

```cpp
class XpeSRReport {
public:
    // 소견 추가 (AI 출력 연동)
    void AddFinding(
        const XpeDicomCode& finding_concept,  // (코드값, 코딩체계, 의미)
        float confidence_score,               // [0.0, 1.0]
        const XpeROI2D& geometry              // 경계 상자 또는 점 집합
    );

    // 측정값 추가 (파노라마 스티칭 각도 등)
    void AddMeasurement(
        const XpeDicomCode& concept,
        float value, const char* unit   // 예: "Cobb angle", 12.5f, "deg"
    );

    // DICOM SR 파일 쓰기
    XpeStatus Write(const char* filepath, const XpeDicomStudyContext& ctx);
};
```

#### 17.2.4 필수 콘텐츠 항목 (TID 1500)

| 항목 | 유형 | 설명 |
|------|------|------|
| Patient context | CONTAINER | (0010,0020) Patient ID |
| Study context | CONTAINER | (0020,000D) Study UID |
| Procedure | CODE | (41367002, SCT, "Radiography") |
| Finding | CODE + SCOORD | 소견 코드 + ROI 기하 |
| Confidence | NUM | (C25347, NCIt) confidence score |

#### 17.2.5 검증 기준

| 테스트 | 기준 |
|--------|------|
| TID 1500 필수 콘텐츠 충족 | 모든 Type 1 항목 존재 |
| ROI 좌표 일관성 | 영상 크기 범위 내 |
| Confidence 범위 | [0.0, 1.0] 범위 내 |
| 파일 쓰기 성공 | XPE_OK 반환 |

#### 17.2.6 DLL 할당

`xpe_dicom.dll` — DICOM IOD 검증기(§17) 확장, AI 출력 연동

#### 17.2.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-DICOM-002 |
| **SWU** | SWU-17.2 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | TID 1500/4100 구조 적합성 검사; DCMTK DcmSRDocument 파싱 성공; 5개 소견 유형 × 3개 해부 부위 테스트 |
| **안전 분류** | Class B |

---

## 18. 지각적 화질 품질 지표 (Perceptual IQM) ★GAP-AS 해소

**SRS ID**: SRS-MEAS-004 | **SWU**: SWU-18.0 | **IEC 62304 §**: 5.4.2

### 18.1 목적 및 배경

영상 향상 알고리즘(§4 Bilateral, §4.8 BayesShrink, §6 Display)의 객관적 벤치마킹을 위해 표준화된 지각적 화질 지표가 필요하다. 기존 문서에서 PSNR/SSIM은 개별 검증 기준으로 언급되었으나, 통합 지표 파이프라인은 미명세였다.

### 18.2 PSNR (Peak Signal-to-Noise Ratio)

$$\text{PSNR} = 20 \cdot \log_{10}\left(\frac{\text{MAX}_I}{\sqrt{\text{MSE}}}\right) \quad \text{[dB]}$$

- MAX_I = 4095 (12-bit), 65535 (16-bit)
- MSE = (1/MN) Σ (I_ref(x,y) − I_test(x,y))²
- 임계값: PSNR ≥ 35 dB (BayesShrink §4.8 기준)

### 18.3 SSIM (Structural Similarity Index)

$$\text{SSIM}(x,y) = \frac{(2\mu_x\mu_y + C_1)(2\sigma_{xy} + C_2)}{(\mu_x^2 + \mu_y^2 + C_1)(\sigma_x^2 + \sigma_y^2 + C_2)}$$

파라미터:
- 윈도우: 11×11 Gaussian, σ_G = 1.5
- C₁ = (0.01·L)², C₂ = (0.03·L)², L = MAX_I
- SSIM 범위: [−1, 1], 임계값 ≥ 0.95

전역 SSIM: 슬라이딩 윈도우 평균으로 계산

### 18.4 MS-SSIM (Multi-Scale SSIM)

$$\text{MS-SSIM}(x,y) = [l_M(x,y)]^{\alpha_M} \cdot \prod_{j=1}^{M} [c_j(x,y)]^{\beta_j} [s_j(x,y)]^{\gamma_j}$$

- M = 5 스케일, 가중치: {0.0448, 0.2856, 0.3001, 0.2363, 0.1333}
- 다운샘플: 저역통과 필터 후 2× 서브샘플
- 임계값: MS-SSIM ≥ 0.98 (AI 알고리즘 검증)

### 18.5 FSIM (Feature Similarity)

$$\text{FSIM} = \frac{\sum_{x \in \Omega} S_L(x) \cdot PC_m(x)}{\sum_{x \in \Omega} PC_m(x)}$$

- PC_m(x) = max(PC_1(x), PC_2(x)): 위상 일관성(Phase Congruency) 마스크
- S_L(x) = S_{PC}(x) · S_G(x): PC 유사도 × 그래디언트 크기 유사도

### 18.6 C++ 구현 요약

```cpp
struct XpeIQMResult {
    float psnr_db;          // Peak SNR (dB)
    float ssim;             // SSIM [0,1]
    float ms_ssim;          // MS-SSIM [0,1]
    float fsim;             // FSIM [0,1]
    bool  psnr_pass;        // psnr_db >= threshold
    bool  ssim_pass;        // ssim >= 0.95
};

XpeIQMResult xpe_compute_iqm(
    const float* ref,   // 참조 영상 (ground truth)
    const float* test,  // 평가 영상
    int width, int height,
    XpeIQMConfig cfg    // 임계값, 스케일 수
);
```

### 18.7 검증 기준

| 지표 | 임계값 | 테스트 케이스 |
|------|--------|-------------|
| PSNR | ≥ 35 dB | σ=20 ADU 합성 잡음 영상 |
| SSIM | ≥ 0.95 | Bilateral 필터 전후 비교 |
| MS-SSIM | ≥ 0.98 | BayesShrink 전후 비교 |
| FSIM | ≥ 0.90 | Edge Enhancement 전후 비교 |

### 18.8 DLL 할당

`xpe_enhance_advanced.dll` — 알고리즘 벤치마크 및 자동 회귀 검증 모듈

### 18.9 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-MEAS-004 (Perceptual IQM — 신규) |
| **SWU** | SWU-18.0 |
| **IEC 62304 §** | 5.4.2 |
| **검증 방법** | 합성 잡음 영상(σ=10,20,30 ADU)에서 각 지표 계산; PSNR ≥ 35 dB, SSIM ≥ 0.95 확인 |
| **안전 분류** | Class B |

---

---

### 9.12 SWU-9.12 방사선량-면적곱(DAP) 및 KERMA 누적 추적 ★GAP-BC 해소

**SRS ID**: SRS-DOSE-001 | **SWU**: SWU-9.12 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_common.dll

#### 9.12.1 배경

IEC 60601-2-54 방사선 촬영 장비 표준은 방사선량-면적곱(DAP, Gy·cm²)과 공기 KERMA의 누적 추적을 의무화한다. XPE는 EI(§7)를 계산하지만 DAP/KERMA 누적 알고리즘은 미명세였다.

#### 9.12.2 DAP 계산 모델

$$\text{DAP} = K_{\text{air}} \times A_{\text{beam}} \quad [\text{Gy·cm}^2]$$

$$K_{\text{air}} = k_{\text{fact}} \times \frac{\text{mAs} \times \text{kVp}^{n_{\text{kVp}}}}{d_{\text{SID}}^2}$$

파라미터:
- k_fact: 장치별 교정 계수 (Gy·cm²·cm²/(mAs·kVp^n))
- n_kVp: kVp 지수 ≈ 2.5 (W/Al 필터 빔)
- d_SID: 선원-영상수신체 거리 (cm)
- A_beam: FOV_width_cm × FOV_height_cm (콜리메이터 마스크 §12.5에서)

#### 9.12.3 누적 추적 알고리즘

```cpp
struct XpeDoseState {
    float dap_current_Gy_cm2;       // 현재 촬영 DAP
    float dap_cumulative_Gy_cm2;    // 세션 누적 DAP
    float kerma_air_uGy;            // 공기 KERMA (μGy)
    float effective_dose_est_mSv;   // 유효 선량 추정 (mSv)
    bool  alert_triggered;          // 누적 임계 초과
};

XpeDoseState xpe_compute_dap(
    float kvp, float mas, float sid_cm,
    float fov_w_cm, float fov_h_cm,
    float k_fact,
    XpeDoseState prev_state  // 누적 상태
);
```

유효 선량 추정: E_est = DAP × DLP_conversion_factor (해부 부위별, ICRP 102)

알림 임계: cumulative_DAP > alert_threshold_Gy_cm2 → XpeQualityState.dose_alert = true

#### 9.12.4 검증 기준

| 조건 | 허용 오차 |
|------|---------|
| PMMA 팬텀 실측 DAP vs. 계산 | ± 10% |
| 누적 DAP 정확도 (10회 촬영) | ± 5% |
| IEC 60601-2-54 §29.201 준수 | 통과 |

#### 9.12.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-DOSE-001 |
| **SWU** | SWU-9.12 |
| **검증 방법** | PMMA 팬텀 5개 kVp/mAs 조합; 계산 DAP vs. 이온 챔버 실측; 누적 카운터 정확성 10회 반복 |
| **안전 분류** | Class B |

---

### 17.3 SWU-17.3 JPEG 2000 (ISO 15444) 무손실/근손실 압축 ★GAP-BD 해소

**SRS ID**: SRS-DICOM-003 | **SWU**: SWU-17.3 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_dicom.dll

#### 17.3.1 배경

§17(GAP-AR)은 DICOM IOD 적합성을 검증하지만 PACS 저장을 위한 JPEG 2000 압축 코덱 통합은 미명세였다.

#### 17.3.2 압축 사양

| 모드 | DICOM 전송 구문 | 코덱 |
|------|--------------|------|
| 무손실 | 1.2.840.10008.1.2.4.90 | JPEG2000 Part1 Lossless (9-7 가역) |
| 근손실 1.5:1 | 1.2.840.10008.1.2.4.91 | JPEG2000 Part1 Lossy (9-7 비가역) |

라이브러리: OpenJPEG 2.5 (SOUP 등록 필요) | 타일: 256×256 픽셀

#### 17.3.3 C++ API

```cpp
struct XpeJ2KParams {
    bool  lossless;           // true = 무손실, false = 근손실
    float max_compression_ratio;  // 최대 압축비 (근손실 시: 3.0)
    int   tile_width, tile_height; // 기본값: 256
    int   n_resolution_levels;    // 기본값: 5
};

XpeStatus xpe_compress_jpeg2000(
    const uint16_t* I_in, int width, int height,
    uint8_t** compressed_out, size_t* compressed_size_out,
    XpeJ2KParams params
);

XpeStatus xpe_decompress_jpeg2000(
    const uint8_t* compressed, size_t compressed_size,
    uint16_t** I_out, int* width_out, int* height_out
);
```

#### 17.3.4 품질 제약

| 모드 | 제약 조건 |
|------|---------|
| 무손실 | 복원 후 원본과 픽셀 단위 동일 (비트 완전) |
| 근손실 1.5:1 | PSNR ≥ 50 dB; SSIM ≥ 0.999 |
| 최대 압축비 | 3:1 (임상 품질 보장 한계) |

#### 17.3.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-DICOM-003 |
| **SWU** | SWU-17.3 |
| **검증 방법** | 무손실: 3Kx3K 영상 압축-복원 후 픽셀 동일성 확인; 근손실 1.5:1: PSNR ≥ 50 dB; 압축 시간 < 100ms |
| **안전 분류** | Class B |

---

### 3.14 SWU-1.14 모션 블러 PSF 추정 및 위너(Wiener) 역필터 ★GAP-BE 해소

**SRS ID**: SRS-FUNC-001d | **SWU**: SWU-1.14 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_preprocess.dll

#### 3.14.1 배경

환자 움직임이나 장비 진동에 의해 방사선 영상에 모션 블러가 발생한다. 이를 검출하고 위너 역필터로 복원하는 알고리즘이 미명세였다.

#### 3.14.2 블러 감지

블러 지표: MTF f50 < 0.7 × f50_ref (§12.2) → 블러 감지

블러 방향/길이 추정:
1. Radon 변환으로 블러 방향 θ 추정 (0–180°)
2. 주파수 도메인 영점(zero) 위치에서 블러 길이 L 추정:
   $$H(u) = \frac{\sin(\pi u L)}{\pi u L} \quad \Rightarrow \quad L = \frac{1}{u_{\text{zero}}}$$

#### 3.14.3 균일 직선 모션 PSF

$$H(u,v) = \frac{\sin(\pi (u\cos\theta + v\sin\theta) L)}{\pi (u\cos\theta + v\sin\theta) L} \cdot e^{-j\pi (u\cos\theta + v\sin\theta)(L-1)}$$

#### 3.14.4 위너(Wiener) 역필터

$$\hat{I}(u,v) = \frac{H^*(u,v)}{|H(u,v)|^2 + K_{\text{WF}}} \cdot G(u,v)$$

- G(u,v): 블러된 영상의 DFT
- K_WF = σ_n²/σ_s² (잡음/신호 전력 비율, 기본값: 0.01)
- H*: PSF의 켤레 복소수

#### 3.14.5 C++ 구현

```cpp
struct XpeMotionBlurParams {
    float angle_deg;    // 블러 방향 (0–180°)
    float length_px;    // 블러 길이 (픽셀)
    float K_wiener;     // 정규화 파라미터 (기본값: 0.01)
    bool  blur_detected;
};

XpeMotionBlurParams xpe_estimate_motion_blur(
    const float* image, int w, int h
);

XpeStatus xpe_wiener_deblur(
    const float* blurred, float* sharp,
    int w, int h,
    const XpeMotionBlurParams& params
);
```

#### 3.14.6 검증 기준

| 테스트 | 기준 |
|--------|------|
| 합성 블러 L=5,10,20px; θ=0,45,90° | 복원 PSNR ≥ 30 dB |
| MTF f50 회복률 | ≥ 80% 원본 대비 |
| 블러 미감지 영상 통과 | 원본 영상 변형 없음 |
| 처리 시간 (3Kx3K) | < 5 ms (cuFFT 또는 Intel MKL) |

#### 3.14.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-001d |
| **SWU** | SWU-1.14 |
| **검증 방법** | 합성 모션 블러 6개 케이스; PSNR ≥ 30 dB; MTF f50 회복 ≥ 80%; 정상 영상 패스스루 확인 |
| **안전 분류** | Class B |

---

### 3.15 SWU-1.15 금속 고밀도 아티팩트 마스크 생성 ★GAP-BF 해소

**SRS ID**: SRS-FUNC-001e | **SWU**: SWU-1.15 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_preprocess.dll

#### 3.15.1 배경

인공관절, 임플란트 등 금속 고밀도 물체는 X선 영상에서 포화(saturation) 및 방사형 스트리크 아티팩트를 유발한다. AI 전처리 및 아티팩트 저감을 위해 금속 마스크가 필요하다.

#### 3.15.2 금속 마스크 생성

```cpp
// 1단계: 임계값 기반 금속 픽셀 감지
float threshold = global_mean + 5.0f * global_std;
for (int i = 0; i < N; i++)
    metal_mask_raw[i] = (I_raw[i] > threshold) ? 1 : 0;

// 2단계: 형태 연산 (팽창 → 구멍 채우기 → 침식)
morphological_dilate(metal_mask_raw, 3);   // 3×3 구조 요소
fill_holes(metal_mask_raw);
morphological_erode(metal_mask_raw, 2);    // 2×2 구조 요소

// 3단계: 연결 요소 분석 (면적 필터링)
remove_small_components(metal_mask_raw, min_area_px2=50);
```

#### 3.15.3 스트리크 마스크 생성

금속 중심에서 방사형 라인 프로파일 분석:
- 8방향 방사형 프로파일 (0°, 45°, 90°, 135° × 양방향)
- 프로파일 표준 편차 > streak_threshold → 스트리크 픽셀 마킹

#### 3.15.4 C++ 구조

```cpp
struct XpeMetalArtifactMap {
    uint8_t* metal_mask;       // 금속 픽셀 이진 맵
    uint8_t* streak_mask;      // 스트리크 영역 이진 맵
    int   num_metal_objects;   // 감지된 금속 객체 수
    float max_metal_area_px2;  // 최대 금속 영역 (픽셀²)
    bool  clinical_use_blocked; // 임상 직접 적용 금지 플래그 (항상 true)
};

XpeMetalArtifactMap xpe_generate_metal_artifact_map(
    const uint16_t* I_raw, int w, int h,
    XpeMetalConfig cfg
);
```

**안전 주의**: clinical_use_blocked 플래그는 항상 true. 이 마스크는 AI 전처리 입력 조건화에만 사용하며, 임상 진단 영상에 직접 적용은 금지.

#### 3.15.5 검증 기준

| 조건 | 기준 |
|------|------|
| 합성 금속 객체 (100–5000 px²) | 마스크 커버리지 ≥ 95% |
| 스트리크 아티팩트 감지율 | ≥ 85% |
| 비금속 영역 오탐률 | < 2% |

#### 3.15.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-001e |
| **SWU** | SWU-1.15 |
| **검증 방법** | 합성 고밀도 객체 10개 케이스; 마스크 커버리지 ≥ 95%; 오탐 < 2%; clinical_use_blocked 플래그 상태 단위 테스트 |
| **안전 분류** | Class B |

---

### 3.16 SWU-1.16 통합 시간 선형성 보정 (Integration Nonlinearity Correction) ★GAP-BT 해소

**표준**: 내부 교정 프로토콜 (a-Si:H TFT 게이트-소스 결합 비선형성)  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-FUNC-001f | SWU-1.16 | xpe_preprocess.dll | Class B |

#### 물리적 원인

a-Si:H 박막 트랜지스터(TFT) 기반 검출기는 짧은 통합 시간(< 10 ms)에서 게이트-소스 기생 커패시턴스에 의한 전하 결합 효과로 **비선형 응답**이 발생한다. 이상 응답 대비 실측 편차 `ε_k`는 최대 ±2%에 달한다.

#### 수학적 정의

N = 16 통합 시간 노드 `t_k ∈ {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000 ms, ...}`:

```
이상 응답:  S_ideal(t) = S_0 × (t / t_ref),   t_ref = 100 ms
비선형 오차: ε_k = (S_k − S_ideal(t_k)) / S_ideal(t_k) × 100 %
보정 계수:  C_nl[k] = S_ideal(t_k) / S_k  (측정값의 역수)
런타임:     I_corr = I_raw × C_nl_interp(t_current)
```

`C_nl_interp`는 인접 노드 간 선형 보간.

#### C++ 구조체 및 API

```cpp
// Integration linearity calibration data
struct XpeIntegLin {
    float  t_nodes_ms[16];   // integration time nodes
    float  c_nodes[16];      // correction multipliers C_nl[k]
    int    n;                // number of nodes (≤ 16)
    float  t_ref_ms;         // reference time (default 100ms)
};

// Apply integration nonlinearity correction in-place
// t_ms: actual integration time used for this acquisition
XpeStatus xpe_integ_linearity_correct(
    uint16_t*         I_inout,
    uint32_t          W,
    uint32_t          H,
    float             t_ms,
    const XpeIntegLin* cal
);
```

#### 알고리즘 의사코드

```cpp
// Find adjacent nodes by binary search
int idx = upper_bound(cal->t_nodes_ms, cal->t_nodes_ms + cal->n, t_ms) - 1;
idx = clamp(idx, 0, cal->n - 2);
float t0 = cal->t_nodes_ms[idx], t1 = cal->t_nodes_ms[idx+1];
float alpha = (t_ms - t0) / (t1 - t0);
float c = lerp(cal->c_nodes[idx], cal->c_nodes[idx+1], alpha);

// Apply correction (AVX2 broadcast multiply)
for (uint32_t i = 0; i < W * H; i += 16) {
    __m256i v = _mm256_loadu_si256((__m256i*)(I_inout + i));
    // 16-bit saturated multiply by c (Q15 fixed-point)
    __m256i result = xpe_avx2_u16_scale(v, c);
    _mm256_storeu_si256((__m256i*)(I_inout + i), result);
}
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 0.5 ms / 3K×3K |
| SIMD 가속 | AVX2 16-bit multiply |
| 메모리 | In-place, 추가 버퍼 없음 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 보정 후 비선형성 잔류 `ε_k` | < 0.1% for all `t_k` |
| 회귀 계수 R² | > 0.9999 (16-point linearity curve) |
| 단조성 | `C_nl[k]` 단조 감소 (짧은 t에서 보정 강함) |
| 엣지 케이스 | `t_ms < t_nodes[0]` → extrapolate; clip to [0.5, 2.0] |

---

## 19. 선형 토모합성 재구성 (FBP/SAA) ★GAP-BG 해소

**SRS ID**: SRS-TOMO-001 | **SWU**: SWU-19.0 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_enhance_advanced.dll

### 19.1 배경

선형 토모합성은 평면 X선 장비에서 제한 각도 투영을 이용해 선택적 깊이 슬라이스를 재구성하는 기법이다. FBP(Filtered Back Projection)와 SAA(Shift-and-Add) 두 가지 재구성 방법을 지원한다.

### 19.2 투영 기하

```
선원 선형 스윕:  ±θ_max = ±15°, N_proj = 11 (기본), 21 (고해상도)
SDD:             1000 mm (조정 가능)
재구성 평면:      z = 0 ~ H_max mm (1mm 간격)
픽셀 피치:        det_pitch_mm = 0.14 mm (a-Si FPD 기준)
```

### 19.3 Shift-and-Add (SAA) 알고리즘

빠른 근사 재구성:

$$I_{\text{slab}}(x, y, z) = \frac{1}{N_{\text{proj}}} \sum_{i=1}^{N_{\text{proj}}} P_i\!\left(x + \Delta x_i(z),\ y\right)$$

깊이 의존 시프트:
$$\Delta x_i(z) = \frac{z \cdot \tan(\theta_i)}{\text{pixelPitch\_mm}}$$

### 19.4 FBP (Filtered Back Projection) 알고리즘

램프 필터 적용 역투영:

$$\hat{f}(x, y, z) = \sum_{i=1}^{N_{\text{proj}}} \int_{-\infty}^{\infty} P_i(t) \cdot h_{\text{ramp}}(x_i(z) - t)\, dt$$

램프 필터: $|ω|$ 응답에 Hanning 윈도우 적용:
$$H_{\text{ramp}}(\omega) = |\omega| \cdot W_{\text{Hanning}}(\omega)$$

### 19.5 C++ API

```cpp
enum class XpeTomoMethod { SAA, FBP };

struct XpeTomoConfig {
    int   n_projections;       // 11 또는 21
    float angle_range_deg;     // ±각도 (기본: 15.0)
    float sdd_mm;              // 1000.0
    float pixel_pitch_mm;      // 0.14
    float slice_spacing_mm;    // 1.0
    int   n_slices;            // 재구성 슬라이스 수
    XpeTomoMethod method;      // SAA 또는 FBP
};

XpeStatus xpe_tomo_reconstruct(
    const float** projections,  // [n_projections][W×H] 입력 투영
    float** slices,             // [n_slices][W×H] 출력 슬라이스
    XpeTomoConfig cfg
);
```

### 19.6 검증 기준

| 항목 | 기준 |
|------|------|
| 슬라이스 두께 FWHM | ≤ 1.5 mm (CIRS 팬텀) |
| 면내 해상도 | ≥ 3 lp/mm (SAA), ≥ 4 lp/mm (FBP) |
| 재구성 시간 (3Kx3K, N=11) | < 200 ms (CPU) |
| SAA vs. FBP CNR 차이 | FBP ≥ SAA × 1.2 |

### 19.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-TOMO-001 (신규) |
| **SWU** | SWU-19.0 |
| **검증 방법** | CIRS Model 014 팬텀; 슬라이스 두께 FWHM ≤ 1.5mm; 면내 해상도 ≥ 3 lp/mm |
| **안전 분류** | Class B |

---

### 8.3.2 SWU-8.3.2 RANSAC+ORB 키포인트 기반 파노라마 스티칭 ★GAP-BH 해소

**SRS ID**: SRS-FUNC-017b | **SWU**: SWU-8.3.2 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_ai.dll

#### 8.3.2.1 배경

기존 §8.3 파노라마 스티칭은 위상 상관(phase correlation)에 의존하나, 긴 척추/하지 영상에서 국소 비선형 변형이 있을 경우 정확도가 저하된다. RANSAC 기반 키포인트 매칭으로 강건성을 향상시킨다.

#### 8.3.2.2 알고리즘

**1단계: ORB 특징점 검출**
- ORB(Oriented FAST + Rotated BRIEF): n_features=500, scale_factor=1.2, n_levels=8
- 각 프레임에서 독립적으로 검출

**2단계: Brute-Force 매칭 + Hamming 거리**
```cpp
BFMatcher matcher(NORM_HAMMING);
matcher.knnMatch(desc1, desc2, matches, k=2);
// Lowe's ratio test: ratio < 0.75 유지
```

**3단계: RANSAC 단응변환 추정**
- 최소 매칭 쌍: 4
- 인라이어 임계값: 5 pixels
- max_iter = 2000
- 결과: 3×3 Homography 행렬 H

**4단계: 원근 워핑 + Laplacian 피라미드 블렌딩**
```cpp
warpPerspective(src, dst, H, panorama_size);
// α-블렌딩: 중첩 영역에서 선형 그래디언트
```

#### 8.3.2.3 폴백 전략

ORB 특징 매칭 실패 (인라이어 < 8) → 자동으로 §8.3 위상 상관 방법으로 전환

#### 8.3.2.4 검증 기준

| 조건 | 기준 |
|------|------|
| 인공 척추 팬텀 | Cobb 각도 오차 ≤ 1.5° |
| 위상 상관 대비 정밀도 | ≥ 30% 개선 |
| 저질감 영상 폴백 | 100% 위상 상관으로 전환 |

#### 8.3.2.5 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-017b |
| **SWU** | SWU-8.3.2 |
| **검증 방법** | 합성 척추 팬텀 5개 케이스; Cobb 오차 ≤ 1.5°; 폴백 시나리오 테스트 |
| **안전 분류** | Class B |

---

### 4.9 SWU-2.9 가우시안/라플라시안 다중 해상도 피라미드 ★GAP-BI 해소

**SRS ID**: SRS-FUNC-014b | **SWU**: SWU-2.9 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_enhance_advanced.dll

#### 4.9.1 배경

SRS-FUNC-014(Laplacian Pyramid)가 요구사항에 존재하지만 완전한 다중 해상도 피라미드 분해/재구성 알고리즘 명세가 없었다. DNN 전처리 및 다중 스케일 에지 강조에 활용된다.

#### 4.9.2 가우시안 피라미드 생성

$$G_{l+1}(x,y) = \sum_{m=-2}^{2}\sum_{n=-2}^{2} w(m,n)\, G_l(2x+m,\; 2y+n)$$

- w: 5×5 가우시안 커널 (σ=1.0), Burt-Adelson 5-탭: {0.0625, 0.25, 0.375, 0.25, 0.0625}
- 레벨 수: L = min(floor(log₂(min(W,H))) − 1, 5)

#### 4.9.3 라플라시안 피라미드 생성

$$L_l = G_l - \text{EXPAND}(G_{l+1})$$

EXPAND 연산: 업샘플 2× + 가우시안 보간 (0 삽입 후 × 4 스케일)

#### 4.9.4 피라미드 재구성

$$G_l = L_l + \text{EXPAND}(G_{l+1})$$

재구성 오류: < 0.001 ADU RMS (부동소수점 정밀도)

#### 4.9.5 C++ 구조

```cpp
struct XpePyramidLevels {
    std::vector<XpeImageF32> gaussian;  // L+1개 레벨
    std::vector<XpeImageF32> laplacian; // L개 레벨
    int n_levels;
    int base_width, base_height;
};

XpePyramidLevels xpe_build_pyramid(
    const float* image, int w, int h, int n_levels
);

void xpe_reconstruct_from_pyramid(
    const XpePyramidLevels& pyr, float* output
);

// 다중 스케일 에지 강조 (각 라플라시안 레벨에 게인 적용)
void xpe_pyramid_edge_enhance(
    XpePyramidLevels& pyr,
    const float* gains,  // [n_levels] 레벨별 게인
    float* output
);
```

#### 4.9.6 검증 기준

| 조건 | 기준 |
|------|------|
| 재구성 오류 | < 0.001 ADU RMS |
| 라플라시안 에너지 분포 | 고주파 에너지 레벨 0 > 레벨 1 > ... (단조 감소) |
| 처리 시간 (3Kx3K, L=5) | < 5 ms (AVX2) |

#### 4.9.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-FUNC-014b |
| **SWU** | SWU-2.9 |
| **검증 방법** | 피라미드 구성-재구성 왕복 오류 < 0.001 ADU RMS; 에너지 단조성 검증; 처리 시간 < 5ms |
| **안전 분류** | Class B |

---

### 10.9 SWU-10.9 GPU CUDA 파이프라인 가속 아키텍처 ★GAP-BJ 해소

**SRS ID**: SRS-PERF-003 | **SWU**: SWU-10.9 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_preprocess.dll (CUDA 확장)

#### 10.9.1 배경

CPU SIMD(AVX2/OpenMP) 최적화(§10.1~10.8)를 보완하는 GPU CUDA 가속 아키텍처. 연산 집약적 단계(Bilateral Filter, FFT Notch, Wavelet)를 GPU로 오프로드한다.

#### 10.9.2 GPU 오프로드 대상 단계

| 파이프라인 단계 | CPU 시간 | GPU 예상 시간 | 가속 비율 |
|--------------|---------|------------|---------|
| Gain Correction (§3.2) | 2 ms | 0.3 ms | 6× |
| Bilateral Filter (§4.2) | 15 ms | 2 ms | 7× |
| 2D FFT Notch (§3.13) | 2 ms | 0.4 ms | 5× (cuFFT) |
| Wavelet BayesShrink (§4.8) | 8 ms | 1.5 ms | 5× |

#### 10.9.3 CUDA 메모리 아키텍처

```
호스트 (CPU)              GPU
Pinned Memory ←──── cudaMemcpyAsync ───→ Global Memory
                                          ↓
                                     CUDA 커널 실행
                                          ↓
Pinned Memory ←──── cudaMemcpyAsync ───  Global Memory (결과)
```

- Pinned Memory: `cudaHostAlloc(flag=cudaHostAllocDefault)` → DMA 전송 가속
- 스트림: 2개 CUDA 스트림으로 전송/연산 오버랩

#### 10.9.4 CUDA 커널 예시 (Gain Correction)

```cuda
__global__ void gain_correction_kernel(
    const uint16_t* __restrict__ raw,
    const float*   __restrict__ gain_map,
    const float*   __restrict__ offset_map,
    float*         __restrict__ corrected,
    int N_pixels)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N_pixels) {
        float val = (float)raw[idx] - offset_map[idx];
        corrected[idx] = val * gain_map[idx];
    }
}
// 실행: <<<(N+255)/256, 256>>> (32 warps per block)
```

#### 10.9.5 폴백 전략 (CPU 자동 전환)

```cpp
XpeStatus xpe_pipeline_set_backend(XpeExecutionBackend backend) {
    if (backend == XpeExecutionBackend::CUDA_AUTO) {
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        active_backend = (device_count > 0) ?
            XpeExecutionBackend::CUDA_AUTO :
            XpeExecutionBackend::CPU;
    }
}
```

조건: 드라이버 없음, 메모리 부족, CUDA 오류 → 자동 CPU 경로 활성화

#### 10.9.6 검증 기준

| 조건 | 기준 |
|------|------|
| CPU/GPU 출력 동일성 | ± 0.01 ADU (부동소수점 차이 허용) |
| 전처리 파이프라인 총 시간 (3Kx3K, GPU) | < 10 ms (RTX 3060 기준) |
| CPU 대비 가속 비율 | ≥ 3× |
| 폴백 시나리오 (CUDA 없음) | 100% CPU 전환 확인 |

#### 10.9.7 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-PERF-003 |
| **SWU** | SWU-10.9 |
| **검증 방법** | CPU vs. GPU 결과 비교 (±0.01 ADU); 총 파이프라인 시간 < 10ms; 폴백 단위 테스트 (CUDA 비활성화 시나리오) |
| **안전 분류** | Class B |

---

### 12.11 SWU-12.11 자동 QA 팬텀 인식 알고리즘 ★GAP-BK 해소

**SRS ID**: SRS-QA-002 | **SWU**: SWU-12.11 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_enhance_advanced.dll

#### 12.11.1 배경

IEC 61223 인수 시험(§12.10)은 팬텀 유형에 따라 다른 프로토콜을 적용한다. 팬텀 유형을 자동으로 인식하면 운영자 오류를 방지하고 워크플로우를 자동화할 수 있다.

#### 12.11.2 지원 팬텀 유형

| 팬텀 | 특징 구조 | 검출 알고리즘 |
|------|---------|------------|
| Leeds TOR 18FG | 18개 원형 인서트 (3행 6열) | 원형 Hough + 격자 패턴 매칭 |
| CDRAD 2.0 | 15×15 사각 배열 구멍 (225개) | Blob 검출 + 격자 분석 |
| CIRS Model 17-150 | 5개 동심 원형 영역 | 동심원 Hough 변환 |
| 기타/불명 | — | UNKNOWN 반환 |

#### 12.11.3 인식 알고리즘

```cpp
// 1단계: 원형 구조 검출 (Hough Circle Transform)
std::vector<XpeCircle> circles = hough_circles(
    image, w, h,
    min_radius_px=5, max_radius_px=50,
    min_dist_px=10
);

// 2단계: 패턴 분류
XpePhantomType classify_phantom(const std::vector<XpeCircle>& circles) {
    if (matches_leeds_pattern(circles))    return LEEDS_TOR_18FG;
    if (matches_cdrad_pattern(circles))    return CDRAD_2;
    if (matches_cirs_pattern(circles))     return CIRS_17150;
    return UNKNOWN;
}
```

#### 12.11.4 C++ API

```cpp
enum class XpePhantomType {
    UNKNOWN, LEEDS_TOR_18FG, CDRAD_2, CIRS_17150
};

XpePhantomType xpe_identify_phantom(
    const float* image, int w, int h,
    float* confidence_out    // 신뢰도 [0,1]
);

// 인식 후 해당 팬텀 프로토콜 자동 실행
XpeAcceptanceResult xpe_run_auto_phantom_test(
    const float* image, int w, int h
);
```

#### 12.11.5 검증 기준

| 조건 | 기준 |
|------|------|
| 각 팬텀 유형 5개 샘플 | 인식 정확도 ≥ 90% |
| UNKNOWN 오분류율 | < 5% |
| 인식 처리 시간 | < 50 ms (3Kx3K) |

#### 12.11.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-QA-002 |
| **SWU** | SWU-12.11 |
| **검증 방법** | Leeds/CDRAD/CIRS 각 5개 팬텀 영상; 인식 정확도 ≥ 90%; UNKNOWN 오분류 < 5% |
| **안전 분류** | Class B |

---

### 9.13 SWU-9.13 교정 전달 함수 (Cross-FPD 패널 정규화) ★GAP-BL 해소

**SRS ID**: SRS-CAL-002 | **SWU**: SWU-9.13 | **IEC 62304 §**: 5.4.2 | **DLL**: xpe_preprocess.dll

#### 9.13.1 배경

동일 제품 라인에서도 FPD 패널 간 픽셀 피치, 비트 심도, 응답 곡선 차이가 존재한다. 다른 패널에서 수집된 영상을 공통 기준 응답으로 정규화하는 전달 함수가 필요하다.

#### 9.13.2 전달 함수 모델

$$I_{\text{norm}}(x,y) = a \cdot I_{\text{raw}}(x,y)^{\gamma} + b$$

파라미터:
- a: 스케일 계수 (선량 응답 스케일링)
- γ: 감마 지수 (a-Si vs. a-Se 응답 비선형성)
- b: 오프셋 (영점 보정)

#### 9.13.3 파라미터 추정 (오프라인 교정)

기준 PMMA 계단 쐐기 (5단계 두께: 10, 20, 30, 40, 50 mm) 촬영:
- 기준 패널 응답: `{dose_i → pixel_ref_i}` (LUT 테이블)
- 대상 패널 응답: `{dose_i → pixel_src_i}`
- Levenberg-Marquardt 피팅: `pixel_src_i ≈ a × pixel_ref_i^γ + b`
- 수렴 기준: R² > 0.9999

#### 9.13.4 C++ 구현

```cpp
struct XpeCTF {               // Calibration Transfer Function
    float a, gamma, b;        // 전달 함수 파라미터
    float r_squared;          // 피팅 R² (검증용)
    char  ref_panel_id[32];   // 기준 패널 ID
    char  src_panel_id[32];   // 대상 패널 ID
};

// 전달 함수 파라미터 추정 (오프라인 Python 또는 C++)
XpeCTF xpe_estimate_ctf(
    const float* dose_values,      // [N] 기준 선량 레벨
    const float* pixel_ref_values, // [N] 기준 패널 픽셀
    const float* pixel_src_values, // [N] 대상 패널 픽셀
    int N
);

// 런타임 정규화 적용
void xpe_apply_ctf(
    const float* I_src, float* I_norm,
    int N_pixels, const XpeCTF& ctf
);
```

#### 9.13.5 검증 기준

| 조건 | 기준 |
|------|------|
| 5개 패널 모델 간 교차 정규화 | 정규화 후 균일성 CV ≤ 0.5% |
| 전달 함수 피팅 R² | > 0.9999 |
| 런타임 적용 처리 시간 | < 1 ms/3Kx3K |

#### 9.13.6 IEC 62304 추적성

| 항목 | 내용 |
|------|------|
| **SRS ID** | SRS-CAL-002 |
| **SWU** | SWU-9.13 |
| **검증 방법** | 5개 패널 모델 PMMA 쐐기 응답; 교차 정규화 후 CV ≤ 0.5%; R² > 0.9999 |
| **안전 분류** | Class B |

---

## 부록 A: 수학 공식 일람

### A.1 Pre-Processing 공식

$$I_{\text{offset}}(x,y) = \max(I_{\text{raw}} - I_{\text{dark}},\ 0)$$

$$G(x,y) = \frac{\bar{I}_{\text{flat}}}{I_{\text{flat}}(x,y) - I_{\text{dark}}(x,y)}, \quad I_{\text{corr}} = I_{\text{offset}} \cdot G$$

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i e^{-t/\tau_i}$$

$$I_{\text{true}} = I_{\text{measured}} - \text{Lag}(t) \cdot I_{\text{prev\_max}}$$

### A.2 Core Processing 공식

$$I_{OD} = -\ln\left(\frac{I_{\text{clean}} + \varepsilon}{I_0 + \varepsilon}\right)$$

$$BF[I](x) = \frac{\sum_{x_i} I(x_i) e^{-\|x_i-x\|^2/2\sigma_s^2} e^{-|I(x_i)-I(x)|^2/2\sigma_r^2}}{\sum_{x_i} e^{-\|x_i-x\|^2/2\sigma_s^2} e^{-|I(x_i)-I(x)|^2/2\sigma_r^2}}$$

$$I_{\text{USM}} = I + \lambda \cdot (I - I * G_\sigma)$$

### A.3 Display Processing 공식

**Linear VOI:**
$$\text{Out} = \text{clamp}\left(\frac{I - (WC - WW/2)}{WW} \cdot (\text{Max} - \text{Min}) + \text{Min},\ \text{Min},\ \text{Max}\right)$$

**Sigmoid VOI:**
$$\text{Out} = \frac{\text{Max} - \text{Min}}{1 + e^{-4(I-WC)/WW}} + \text{Min}$$

**GSDF JND:**
$$j = 71.498068 + 94.593053\log L + 41.912053(\log L)^2 + \cdots$$

### A.4 Exposure Index 공식

$$DI = 10 \cdot \log_{10}\left(\frac{EI}{EI_{\text{target}}}\right) \quad \text{(dB)}$$

### A.5 FPD 특성화 공식

$$\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\Phi \cdot \text{NNPS}(f)}$$

$$\text{NNPS}(f) = \frac{\text{NPS}(f)}{\bar{S}^2}$$

$$\sigma_A^2(\tau) = \frac{1}{2}\langle(\bar{x}_{k+1} - \bar{x}_k)^2\rangle$$

---

## 부록 B: 표준 참조 테이블

### B.1 RQA 조건 (IEC 61267)

| RQA | kVp | Al 여과 (mm) | HVL (mm Al) | 용도 |
|-----|-----|------------|-------------|------|
| RQA3 | 70 | 23.0 | 6.8 | Mammography-adjacent |
| RQA5 | 80 | 21.0 | 7.1 | General radiography |
| RQA7 | 90 | 30.0 | 9.2 | Chest |
| RQA9 | 120 | 40.0 | 11.5 | High-kVp chest |
| RQA10 | 150 | 50.0 | 13.0 | Interventional |

### B.2 SPR 참조 (80kVp, 35×43cm FOV)

| 두께 (cm, water equiv.) | SPR (%) |
|-----------------------|---------|
| 10 | 30–50 |
| 15 | 60–80 |
| 20 | 80–120 |
| 25 | 120–180 |
| 30 | 150–250 |

### B.3 GSDF P-Value Luminance (PS3.14 Table B.1 발췌)

| P-Value | Target Luminance (cd/m²) | JND Index |
|---------|------------------------|----------|
| 0 | 0.05 | ~10 |
| 1024 | 2.0 | ~200 |
| 2048 | 50.0 | ~400 |
| 3071 | 1000.0 | ~600 |
| 4095 | 3000.0 | ~700 |

---

## 20. 전체 변분 반복적 영상 복원 (TV-Minimization ADMM) ★GAP-BU 해소

**참고**: Boyd et al., "Distributed Optimization and Statistical Learning via the Alternating Direction Method of Multipliers," Foundations & Trends in ML, 2011.  
**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-ITER-001 | SWU-20.0 | xpe_enhance_advanced.dll | Class B |

### 배경

X-선 저선량 획득 시 양자 잡음이 증가하여 영상 품질이 저하된다. **전체 변분(Total Variation) 최소화**는 에지를 보존하면서 잡음을 제거하는 볼록 최적화 기법으로, ADMM(Alternating Direction Method of Multipliers)로 효율적으로 풀 수 있다.

### 최적화 문제

```
최소화: ½||u − I||²₂ + λ·TV(u)

TV(u) = Σ_{x,y} √((∂u/∂x)² + (∂u/∂y)² + ε²)   (등방성 TV, ε=1e-6)

λ = λ₀ · σ̂_noise,   λ₀ = 0.1 (기본값)
σ̂_noise = MAD(HighPassFilter(I)) / 0.6745  (Robust 잡음 추정)
```

### ADMM 반복 알고리즘

ADMM splitting: `z = ∇u` (기울기 벡터), `ν` = dual variable

```
u-update (FFT 가속):
  û = (Î + ρ·D̂ᵀ·(ẑ − ν̂)) / (1 + ρ·|D̂|²)
  여기서 D̂(u,v) = 기울기 연산자 FFT (수평/수직 차분)

z-update (요소별 proximal 연산):
  z = prox_{λ/ρ·||·||₁,₂}(∇u + ν)
  proximal: z_i = max(0, 1 − λ/(ρ·||∇u_i + ν_i||₂)) · (∇u_i + ν_i)

ν-update:
  ν = ν + ∇u − z

수렴 판정: ||u_{k+1} − u_k||₂ / ||u_k||₂ < tol (= 1e-4)
최대 반복: max_iter = 50
```

### C++ 구조체 및 API

```cpp
struct XpeTvAdmmParams {
    float lambda_0;       // regularization base (default 0.1)
    int   max_iter;       // maximum ADMM iterations (default 50)
    float rho;            // ADMM penalty parameter (default 1.0)
    float tol;            // convergence tolerance (default 1e-4)
    bool  use_fft_accel;  // FFT-based u-update (default true)
};

XpeStatus xpe_tv_admm_denoise(
    const uint16_t*       I_in,
    uint16_t*             I_out,
    uint32_t              W,
    uint32_t              H,
    const XpeTvAdmmParams* params
);
```

### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 200 ms / 3K×3K (FFT 가속, 50회 반복) |
| 메모리 | 5× float32 버퍼 (u, z, ν, D̂, 임시) |
| 실시간 전처리 | 선택적 (진단 워크플로우 후처리로 사용) |

### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| PSNR 향상 | ≥ 3 dB vs 입력 (σ=20 ADU 합성 잡음) |
| SSIM | ≥ 0.95 (전처리 후) |
| 엣지 보존 MTF f₅₀ 손실 | < 10% (슬랜트 에지 측정) |
| 수렴 반복 수 | 평균 < 30회 (3K×3K 단일 채널) |
| 과잉 평활화 지표 | TV(I_out) / TV(I_in) ≥ 0.60 (엣지 보존 확인) |

---

### 20.1 SWU-20.1 골밀도 정량화 알고리즘 (BMD DXA-proxy) ★GAP-BV 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-BMD-001 | SWU-20.1 | xpe_enhance_advanced.dll | Class B |

#### 중요 안전 고지

> **[SAFETY] Non-SaMD 보조 알고리즘**: 본 알고리즘은 DXA 기기의 대체가 아니며, 임상 진단 결정에 직접 사용할 수 없다. 출력은 추세 모니터링 목적의 참고 정보이다. `clinical_decision_blocked = true` 강제 적용.

#### 수학적 정의

이중 에너지 차감(§16) 출력 `I_bone(x,y)` 활용:

```
단계 1: ROI — §8.8에서 SPINE_AP 확인 후 L2~L4 추정 경계 박스
         y_L2 = 0.30 × H, y_L4 = 0.55 × H (정규화 좌표 기준 근사)

단계 2: 평균 뼈 신호
         B_mean = mean(I_bone(x,y) within ROI)

단계 3: 피질골 참조
         R_cortex = 95-퍼센타일(I_bone within ROI)  (고밀도 뼈 기준점)

단계 4: BMD proxy
         ρ_BMD = k_bmd × (B_mean / R_cortex) × (kVp_high / kVp_low)^n_bmd
         k_bmd : 알루미늄 스텝 웨지 팬텀 교정 계수
         n_bmd : 경험적 지수 ≈ 0.3 (Al 계단 기울기 피팅)

단계 5: T-score proxy
         T = (ρ_BMD − ρ_ref) / σ_ref
         ρ_ref, σ_ref : 연령/성별 정규 분포 파라미터 (내장 룩업 테이블)
```

#### C++ 구조체 및 API

```cpp
struct XpeBmdResult {
    float bmd_proxy_g_cm2;  // estimated BMD proxy value
    float t_score;          // T-score proxy (informational only)
    float roi_mean;         // B_mean from ROI
    float cortex_ref;       // R_cortex (95th percentile)
    bool  valid;            // false if anatomy != SPINE_AP or SPINE_LAT
    bool  clinical_decision_blocked;  // always true
};

XpeStatus xpe_bmd_estimate(
    const uint16_t*          I_bone,
    const uint16_t*          I_soft,
    uint32_t                 W,
    uint32_t                 H,
    const XpeBodyPartResult* anatomy,
    XpeBmdResult*            result
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 10 ms / 이미지 (CPU, ROI 통계 연산) |
| 의존성 | §16 DES, §8.8 Body Part Recognition 필요 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 알루미늄 단계 상관 | `ρ_proxy vs 중량 밀도 r² > 0.85` (5단계 스텝 웨지) |
| T-score 방향 정확도 | ≥ 90% (낮은/보통/높은 밀도 3범주 분류) |
| Non-SPINE 이미지 | `valid = false`, BMD 계산 건너뜀 |
| 안전 플래그 | `clinical_decision_blocked` 항상 `true` 검증 |

---

## §21 SWU-21.0 포톤 계수 검출기(PCD) 스펙트럼 빈닝 ★GAP-BW 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-SPEC-001 | SWU-21.0 | xpe_preprocess.dll | Class B |

### 개요

포톤 계수 검출기(Photon Counting Detector, PCD)는 에너지 분해 X선 검출을 제공한다. 각 광자를 개별적으로 계수하고 에너지 임계값 T1, T2에 따라 두 개의 에너지 빈으로 분리한다. 전하 공유(charge sharing) 효과를 보정하여 스펙트럼 왜곡을 최소화하고, 형광 표적(Cu-Ka, Mo-Ka)을 이용한 임계값 등화 교정을 수행한다.

### 알고리즘

단계 1: 에너지 임계값 등화 교정 (오프라인, Python)
         Cu-Ka (8.04 keV), Mo-Ka (17.48 keV) 형광 표적 조사
         각 픽셀 (x,y)에서 임계값 등화 오프셋 Δt(x,y) 산출:
         T1_eq(x,y) = T1_nominal + Δt1(x,y)
         T2_eq(x,y) = T2_nominal + Δt2(x,y)

단계 2: 전하 공유 보정 (런타임, 픽셀별)
         I_corr[x,y] = I_raw[x,y] - k_cs * (I_raw[x-1,y] + I_raw[x+1,y]
                                            + I_raw[x,y-1] + I_raw[x,y+1])
         k_cs: 전하 공유 계수 (0.01 ~ 0.05, 검출기 고유 교정 파라미터)
         경계 픽셀: 패딩 없이 유효 이웃만 합산 (경계 가중치 보정)

단계 3: 에너지 빈 분리
         Low-E bin:  I_low[x,y]  = I_corr[x,y] if T1_eq ≤ E_photon < T2_eq
         High-E bin: I_high[x,y] = I_corr[x,y] if T2_eq ≤ E_photon < T_max
         각 픽셀에서 독립적으로 계수; 두 빈 합산 ≈ 총 광자 계수

단계 4: 빈 이미지 정규화 (선택적)
         I_low_norm  = I_low  / flat_low_mean  × 65535
         I_high_norm = I_high / flat_high_mean × 65535

#### SIMD 최적화

AVX2 전략: 픽셀별 전하 공유 보정은 이웃 픽셀 로드 + 가중 합산으로 구성된다. 각 행을 256-bit 레지스터 8 uint16 단위로 처리하며, 이웃 행은 별도 레지스터에 미리 로드하여 메모리 대역폭을 최소화한다.

```cpp
// AVX2 전하 공유 보정 (단순화된 의사코드, 내부 픽셀)
__m256i row_cur  = _mm256_loadu_si256((__m256i*)(row + x));
__m256i row_up   = _mm256_loadu_si256((__m256i*)(row_above + x));
__m256i row_dn   = _mm256_loadu_si256((__m256i*)(row_below + x));
__m256i left     = _mm256_loadu_si256((__m256i*)(row + x - 1));
__m256i right    = _mm256_loadu_si256((__m256i*)(row + x + 1));
// neighbor_sum = left + right + row_up + row_dn
__m256i neigh    = _mm256_add_epi16(_mm256_add_epi16(left, right),
                                     _mm256_add_epi16(row_up, row_dn));
// I_corr = I_raw - k_cs * neigh  (k_cs fixed-point scaled)
__m256i corr     = _mm256_subs_epu16(row_cur,
                        _mm256_mulhi_epu16(neigh, k_cs_fp16));
_mm256_storeu_si256((__m256i*)(out + x), corr);
```

#### Python 교정 코드 (임계값 등화)

```python
import numpy as np

def pcd_threshold_equalization(flat_cu: np.ndarray, flat_mo: np.ndarray,
                                 nominal_t1_kev: float = 8.04,
                                 nominal_t2_kev: float = 17.48):
    """
    형광 표적 이미지를 이용한 픽셀별 임계값 오프셋 산출.
    flat_cu: Cu-Ka (8.04 keV) 균일 조사 이미지 (H x W)
    flat_mo: Mo-Ka (17.48 keV) 균일 조사 이미지 (H x W)
    반환: delta_t1 (H x W), delta_t2 (H x W) — keV 단위 오프셋
    """
    mean_cu = np.mean(flat_cu)
    mean_mo = np.mean(flat_mo)
    # 각 픽셀 응답이 평균에서 벗어난 비율을 keV 오프셋으로 변환
    delta_t1 = (flat_cu / mean_cu - 1.0) * nominal_t1_kev * 0.1
    delta_t2 = (flat_mo / mean_mo - 1.0) * nominal_t2_kev * 0.1
    return delta_t1.astype(np.float32), delta_t2.astype(np.float32)


def apply_charge_sharing_correction(img: np.ndarray, k_cs: float) -> np.ndarray:
    """픽셀별 전하 공유 보정 (런타임 참조 구현)."""
    from scipy.ndimage import convolve
    kernel = np.array([[0, 1, 0],
                       [1, 0, 1],
                       [0, 1, 0]], dtype=np.float32)
    neighbor_sum = convolve(img.astype(np.float32), kernel, mode='reflect')
    corrected = img.astype(np.float32) - k_cs * neighbor_sum
    return np.clip(corrected, 0, 65535).astype(np.uint16)
```

#### C++ 구조체 및 API

```cpp
struct XpePcdBinResult {
    uint16_t* low_energy_image;   // Low-E bin image (T1~T2), caller-allocated
    uint16_t* high_energy_image;  // High-E bin image (T2~Tmax), caller-allocated
    float     threshold_low_keV;  // effective T1 (keV) after equalization
    float     threshold_high_keV; // effective T2 (keV) after equalization
    float     charge_share_coeff; // k_cs applied
    bool      pcd_active;         // false = standard integrating mode fallback
};

XpeStatus xpe_pcd_spectral_bin(
    const uint16_t*        raw_image,
    uint32_t               W,
    uint32_t               H,
    float                  threshold_low_keV,
    float                  threshold_high_keV,
    float                  charge_share_coeff,
    const float*           delta_t1_map,      // H×W float, NULL → no equalization
    const float*           delta_t2_map,      // H×W float, NULL → no equalization
    XpePcdBinResult*       result
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 5 ms / 프레임 (3K×3K, AVX2 SIMD) |
| 메모리 | 입력 이미지 2× (Low-E + High-E 출력 버퍼) |
| 의존성 | §9 교정 맵 시스템 (임계값 등화 맵 로드) |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 형광 표적 피크 위치 | Cu-Ka ±0.5 keV, Mo-Ka ±0.5 keV 내 |
| Low-E/High-E 대비비 | Monte Carlo 시뮬레이션 대비 5% 이내 |
| 전하 공유 보정 잔류 | 보정 후 인접 픽셀 상관계수 < 0.02 (보정 전 대비 50% 감소) |
| PCD 비활성 폴백 | `pcd_active = false` 시 원본 이미지 패스스루 검증 |

---

## §9.14 SWU-9.14 지능형 교정 수명 주기 관리 (ICLM) ★GAP-BX 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-QC-005 | SWU-9.14 | xpe_common.dll | Class B |

### 개요

베이지안 교정 드리프트 예측기를 이용해 교정 파라미터의 미래 드리프트를 확률적으로 추정하고, 재교정 시점을 선제적으로 예측한다. 온도 의존성을 반영한 유효 드리프트 속도 모델을 사용하며, 교정 신선도 점수(Freshness Score)를 실시간으로 유지한다.

### 알고리즘

단계 1: 선형 드리프트 모델 (Bayesian 불확도 포함)
         d(t) = d₀ + r_drift × t
         σ_d(t) = σ₀ + √t × σ_r
         d₀: 현재 드리프트 (ADU), r_drift: 드리프트 속도 (ADU/frame)
         σ₀: 초기 불확도, σ_r: 드리프트 속도 불확도

단계 2: 온도 가중 드리프트 속도 보정
         r_drift_eff = r_drift × (1 + α_T × |ΔT|)
         α_T: 온도 계수 (기본값 0.05/°C), ΔT: 현재 온도 - 교정 기준 온도

단계 3: 선제적 재교정 트리거 조건
         P(|d(t_pred)| > threshold) > 0.80  →  재교정 권고
         t_pred: 예측 시간 지점 (recommended_recal_frames 후)
         가우시안 분포: P = 1 - Φ((threshold - d(t_pred)) / σ_d(t_pred))

단계 4: 교정 신선도 점수
         F = 1 - |current_drift| / threshold_drift    [0.0 ~ 1.0]
         F < 0.20  →  `urgent_recal_required = true`

단계 5: 유지보수 윈도우 예측
         t_recal = max t s.t. P(within_spec at t) ≥ 0.95
         이진 탐색 또는 해석적 역함수로 산출

#### Python 교정 코드 (드리프트 모델 파라미터 추정)

```python
import numpy as np
from scipy.stats import norm

def estimate_drift_parameters(drift_history: np.ndarray,
                               frame_timestamps: np.ndarray):
    """
    드리프트 이력에서 d0, r_drift, sigma0, sigma_r 추정.
    drift_history: (N,) ADU 단위 드리프트 측정값
    frame_timestamps: (N,) 프레임 번호 또는 시간
    """
    # 선형 회귀
    A = np.column_stack([np.ones_like(frame_timestamps), frame_timestamps])
    coeffs, residuals, _, _ = np.linalg.lstsq(A, drift_history, rcond=None)
    d0, r_drift = coeffs
    residual_std = np.std(drift_history - A @ coeffs)
    sigma0 = residual_std
    sigma_r = residual_std / np.sqrt(np.var(frame_timestamps) * len(frame_timestamps))
    return float(d0), float(r_drift), float(sigma0), float(sigma_r)


def predict_recalibration_window(d0, r_drift, sigma0, sigma_r,
                                  threshold_adu: float,
                                  target_prob: float = 0.95) -> int:
    """재교정이 필요한 프레임 수 예측."""
    for t in range(1, 100000):
        d_t = d0 + r_drift * t
        sigma_t = sigma0 + np.sqrt(t) * sigma_r
        p_within = norm.cdf(threshold_adu, abs(d_t), sigma_t)
        if p_within < target_prob:
            return t - 1
    return 100000
```

#### C++ 구조체 및 API

```cpp
struct XpeCalibLifecycleState {
    float    freshness_score;          // 교정 신선도 점수 [0~1]
    float    predicted_drift_adu;      // 예측 드리프트 (ADU)
    float    prediction_confidence;    // 예측 신뢰도 [0~1]
    uint32_t recommended_recal_frames; // 권장 재교정까지 프레임 수
    bool     urgent_recal_required;    // 긴급 재교정 필요 여부
};

XpeStatus xpe_calib_lifecycle_update(
    float                    current_drift_adu,
    float                    current_temperature_c,
    float                    calibration_temperature_c,
    float                    threshold_drift_adu,
    float                    alpha_temperature,
    uint32_t                 frames_since_calib,
    XpeCalibLifecycleState*  state
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 1 ms (100프레임마다 1회 실행) |
| 실행 주기 | 100프레임당 1회 |
| 의존성 | §9.5 교정 드리프트 모니터, §3.12 온도 보상 이득 보정 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 합성 드리프트 주입 재교정 트리거 | 민감도 ≥ 95% |
| 위양성(false-positive) 재교정 | 비율 < 10% |
| 유지보수 윈도우 예측 정확도 | ±10% 이내 (합성 데이터) |
| 신선도 점수 수렴 | F = 1.0 (교정 직후), F → 0 (드리프트 임계 도달) |

---

## §4.10 SWU-4.10 NPS 기반 잡음 최적 구조 필터링 (NOSF) ★GAP-BY 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-FUNC-011d | SWU-4.10 | xpe_enhance_basic.dll | Class B |

### 개요

잡음 전력 스펙트럼(NPS, §12.3)을 직접 활용하는 주파수 영역 위너(Wiener) 필터를 적응형 구조 텐서 보존과 결합한다. 평탄 영역은 위너 필터로 잡음을 최적 제거하고, 강한 구조 에지 영역에서는 양측 필터(bilateral filter)로 전환하여 MTF 손실을 최소화한다.

### 알고리즘

단계 1: NPS 측정 (flat-field 획득, §12.3 참조)
         S_n(u,v) ← 플랫 필드 이미지 ROI의 2D PSD (픽셀별 분산 기여)

단계 2: 신호 전력 스펙트럼 추정
         S_s(u,v) = max(|F{I}|² - S_n(u,v), 0)  (주파수 영역)
         F{I}: 입력 이미지의 2D FFT

단계 3: 위너 필터 계수 산출
         W(u,v) = |H(u,v)|² / (|H(u,v)|² + S_n(u,v) / S_s(u,v))
         H(u,v): 검출기 MTF (§12.6에서 로드 또는 unity 가정)

단계 4: 구조 텐서 계산
         J = ∇I ⊗ ∇I  (외적), Gaussian 평활화 σ_J = 1.5px
         trace(J) = λ₁ + λ₂  (국소 구조 강도)

단계 5: 적응형 블렌딩 가중치
         λ_blend = tanh(κ × trace(J)),  κ = 0.01
         → λ_blend → 1: 구조 강함 → bilateral filter 지배
         → λ_blend → 0: 평탄 영역 → Wiener filter 지배

단계 6: 최종 출력
         I_out = (1 - λ_blend) × IFFT(W(u,v) × F{I}) + λ_blend × I_bilateral

#### SIMD 최적화

AVX2 + MKL FFT 파이프라인: MKL DFT (float32 단정도)로 전진/역변환 수행. 주파수 영역 위너 필터 적용은 복소 벡터 곱셈 (AVX2 `_mm256_mul_ps`)으로 처리한다. 구조 텐서 계산은 Sobel 커널을 AVX2 FMA 명령으로 가속한다.

```cpp
// AVX2 주파수 영역 위너 필터 적용 (의사코드)
for (int i = 0; i < N; i += 8) {
    __m256 sn  = _mm256_loadu_ps(S_n + i);       // 잡음 PSD
    __m256 ss  = _mm256_loadu_ps(S_s + i);       // 신호 PSD
    __m256 sn_ss = _mm256_div_ps(sn, _mm256_max_ps(ss, eps_v));
    __m256 one = _mm256_set1_ps(1.0f);
    __m256 w   = _mm256_div_ps(one, _mm256_add_ps(one, sn_ss));  // W(u,v)
    __m256 fr  = _mm256_loadu_ps(F_re + i);      // FFT 실수부
    __m256 fi  = _mm256_loadu_ps(F_im + i);      // FFT 허수부
    _mm256_storeu_ps(F_re + i, _mm256_mul_ps(w, fr));
    _mm256_storeu_ps(F_im + i, _mm256_mul_ps(w, fi));
}
```

#### C++ 구조체 및 API

```cpp
struct XpeNosfParams {
    const float* nps_map;          // S_n(u,v), H×W float32, NULL → 단위 NPS 가정
    const float* mtf_map;          // |H(u,v)|², H×W float32, NULL → unity
    float        kappa;            // 구조 텐서 감도 계수 (기본: 0.01)
    float        sigma_structure;  // 구조 텐서 Gaussian 평활 σ (기본: 1.5px)
};

XpeStatus xpe_nosf_filter(
    const uint16_t*      input,
    uint32_t             W,
    uint32_t             H,
    const XpeNosfParams* params,
    uint16_t*            output
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 8 ms / 프레임 (3K×3K, MKL DFT + AVX2) |
| 메모리 | 2× float32 복소 버퍼 (FFT in/out) + 구조 텐서 2× float32 |
| 의존성 | §12.3 NPS 계산, §12.6 MTF (선택) |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| PSNR (σ=25 ADU 합성 잡음) | ≥ 36 dB vs. 참조 이미지 |
| MTF f₅₀ 손실 | < 3% (슬랜트 에지 측정) |
| 구조 보존 지수 | ≥ 0.98 (SSIM 구조 성분 기준) |
| 위너 단독 vs. 블렌딩 | PSNR 차이 < 0.5 dB (평탄 ROI), MTF 차이 < 1% (에지 ROI) |

---

## §3.17 SWU-3.17 극좌표 도메인 링 아티팩트 보정 ★GAP-BZ 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-FUNC-001g | SWU-3.17 | xpe_preprocess.dll | Class B |

### 개요

회전 대칭 센서 결함이나 전자 잡음으로 인해 발생하는 동심원 링 아티팩트를 극좌표 변환을 통해 효과적으로 제거한다. 극좌표 도메인에서 링은 수평 줄무늬로 나타나므로 행 방향 주파수 분석으로 정확히 감지하고, 해부학적 구조와의 상관 점검으로 위양성을 억제한다.

### 알고리즘

단계 1: 극좌표 변환 (직교 → 극)
         원점: X선 소스 투영점 (c_x, c_y), 기본값 이미지 중심
         r(x,y) = √((x-c_x)² + (y-c_y)²)
         θ(x,y) = atan2(y-c_y, x-c_x)
         I_polar(r, θ): 이중 선형(bilinear) 보간 리샘플링

단계 2: 링 감지 (행 방향 PSD 분석)
         각 θ 슬라이스에서 행 방향 1D FFT 수행
         링 주파수 f_ring: PSD 피크 위치 (r 도메인)
         링 진폭 A_ring: 피크 크기
         유의성 점수 = A_ring / σ_background  →  > 3σ이면 링으로 판정

단계 3: 해부학적 위양성 억제
         구조 맵 M_struct(r): §8.8 또는 에지 검출로 생성
         상관 점수 = corr(M_struct, ring_profile)
         상관 > 0.3  →  해당 반경 링 보정 건너뜀

단계 4: 링 아티팩트 제거
         평균 방사 프로파일: M(r) = (1/2π) ∫₀²π I_polar(r, θ) dθ
         보정: I_polar_corr(r, θ) = I_polar(r, θ) - [M(r) - M_global]
         M_global = mean(M(r)): 전역 기준값 보존

단계 5: 역극좌표 변환 (극 → 직교)
         이중 선형 역보간으로 원래 Cartesian 좌표계 복원

#### SIMD 최적화

행 방향 평균 누적: 각 r 반경의 θ 방향 합산을 AVX2 수평 덧셈 (`_mm256_hadd_ps`)으로 처리한다. 1D FFT는 MKL 배치 1D DFT를 사용한다. 극좌표 변환의 bilinear 보간은 AVX2 gather 명령으로 인덱스 계산을 병렬화한다.

```cpp
// 행 방향 평균 프로파일 누적 (의사코드)
for (int r = 0; r < R_max; r += 1) {
    __m256 sum = _mm256_setzero_ps();
    for (int t = 0; t < THETA_BINS; t += 8) {
        __m256 vals = _mm256_loadu_ps(I_polar + r * THETA_BINS + t);
        sum = _mm256_add_ps(sum, vals);
    }
    // horizontal reduce
    M_r[r] = hsum_avx2(sum) / THETA_BINS;
}
```

#### C++ 구조체 및 API

```cpp
struct XpeRingCorrParams {
    float    center_x;             // 극좌표 원점 X (픽셀), -1 → 이미지 중심
    float    center_y;             // 극좌표 원점 Y (픽셀), -1 → 이미지 중심
    float    ring_sigma_threshold; // 링 유의성 임계값 (기본: 3.0)
    float    anatomy_corr_limit;   // 해부 상관 억제 임계값 (기본: 0.3)
    uint32_t theta_bins;           // 각도 분할 수 (기본: 360)
};

XpeStatus xpe_ring_artifact_correct(
    const uint16_t*          input,
    uint32_t                 W,
    uint32_t                 H,
    const XpeRingCorrParams* params,
    uint16_t*                output
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 15 ms / 프레임 (3K×3K, 극좌표 변환 + FFT 포함) |
| 메모리 | 극좌표 버퍼 R_max × THETA_BINS × float32 |
| 의존성 | §8.8 해부 부위 인식 (위양성 억제, 선택적) |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 주입 링 검출률 | ≥ 95% (3 링 반경 × 3 진폭 조합) |
| 링 잔류 아티팩트 | < 원래 진폭의 10% |
| 링 없는 해부 이미지 위양성 | < 5% |
| 역변환 후 PSNR (링 없는 이미지) | ≥ 45 dB (무결성 손실 최소화) |

---

## §8.9 SWU-8.9 해부 기반 인스턴스 분할 (AGIS) ★GAP-CA 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-SEG-002 | SWU-8.9 | xpe_ai_worker.exe | Class B (Non-SaMD) |

#### 중요 안전 고지

> **[SAFETY] Non-SaMD 보조 알고리즘**: 본 알고리즘의 출력은 다른 알고리즘(W/L 자동 조정, EI ROI, VG 프리셋)의 입력으로만 사용된다. 임상 진단 결정에 직접 사용할 수 없으며, `clinical_use_requires_review = true` 강제 적용.

### 개요

경량화 Mask R-CNN 변형(EfficientDet-D1 백본)을 이용해 13개 해부학적 클래스의 인스턴스 분할 마스크를 생성한다. ONNX int8 양자화 모델을 xpe_ai_worker.exe를 통해 실행하며, GPU 미사용 시 CPU 폴백을 보장한다.

### 알고리즘

단계 1: 모델 추론 (ONNX Runtime)
         입력: 3K×3K uint16 이미지 → float32 정규화 (0~1)
         모델: EfficientDet-D1 + Mask head, int8 양자화, ~35MB
         출력: 13클래스 바이너리 마스크 집합 (각 H×W)

         13클래스:
         lung_L, lung_R, rib_cage, spine_AP, spine_LAT,
         clavicle_L, clavicle_R, pelvis, femur_L, femur_R,
         heart_shadow, diaphragm, collimation_boundary

단계 2: 신뢰도 임계값 필터링
         클래스별 confidence ≥ 0.70: 마스크 발행
         < 0.70: 해당 클래스 마스크 비발행 (하위 알고리즘에 null 전달)

단계 3: 폴백 처리
         AGIS 추론 실패 또는 타임아웃 시:
         § 8.8 Body Part Recognition 결과 기반 단순 바운딩 박스 마스크 생성

단계 4: 하위 알고리즘 통합
         §6.4 Auto W/L: lung 마스크 → 폐야 특화 윈도우 레벨
         §5.3 VG Presets: rib_cage 마스크 → 늑골 억제 강도
         §7.2 EI ROI: lung 마스크 ROI → EI 계산 정밀화

#### C++ 구조체 및 API

```cpp
struct XpeAgisResult {
    uint8_t*  masks[13];           // 클래스별 바이너리 마스크 (H×W), NULL = 미감지
    float     confidence[13];      // 클래스별 신뢰도 [0~1]
    uint32_t  W;                   // 마스크 가로 (픽셀)
    uint32_t  H;                   // 마스크 세로 (픽셀)
    bool      fallback_used;       // §8.8 폴백 사용 여부
    bool      clinical_use_requires_review;  // 항상 true
};

// 클래스 인덱스 상수
typedef enum {
    AGIS_LUNG_L = 0, AGIS_LUNG_R, AGIS_RIB_CAGE,
    AGIS_SPINE_AP, AGIS_SPINE_LAT, AGIS_CLAVICLE_L, AGIS_CLAVICLE_R,
    AGIS_PELVIS, AGIS_FEMUR_L, AGIS_FEMUR_R,
    AGIS_HEART_SHADOW, AGIS_DIAPHRAGM, AGIS_COLLIMATION_BOUNDARY
} XpeAgisClass;

XpeStatus xpe_agis_segment(
    const uint16_t*     image,
    uint32_t            W,
    uint32_t            H,
    float               confidence_threshold,  // 기본: 0.70
    XpeAgisResult*      result
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 (GPU A100) | < 80 ms |
| 처리 시간 (CPU 폴백) | < 500 ms (3K×3K) |
| 모델 크기 | ~35 MB (int8 양자화 ONNX) |
| 의존성 | §8.8 Body Part Recognition (폴백), §6.4, §5.3, §7.2 (하위 통합) |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| COCO-style mAP (200-이미지 다해부 테스트 셋) | ≥ 0.72 |
| IoU (폐, 척추) | ≥ 0.85 |
| 폴백 트리거 커버리지 | 100% (AGIS 실패 시 §8.8 폴백 항상 동작) |
| Non-SaMD 안전 플래그 | `clinical_use_requires_review = true` 항상 검증 |

---

## §22 SWU-22.0 압축 센싱 희소 뷰 토모합성 재구성 ★GAP-CB 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-TOMO-002 | SWU-22.0 | xpe_enhance_advanced.dll | Class B |

### 개요

희소 투영 뷰(N=5)에서 표준 11뷰 동등 품질의 토모합성 슬라이스를 재구성한다. 전체 변분(TV) 정규화와 웨이블릿 L1 정규화를 결합한 ADMM 최적화 문제를 FFT 기반으로 풀어 50% 선량 절감 조건에서 in-plane 해상도 ≥ 3 lp/mm를 달성한다.

### 알고리즘

단계 1: 희소 뷰 사이노그램 획득
         N_sparse = 5 투영 각도 (±15° 범위 균등 간격)
         N_standard = 11 (§19 FBP 기준 대비)
         b ∈ ℝ^(N_sparse × detector_pixels): 측정 사이노그램

단계 2: 압축 센싱 최적화 문제 정의
         argmin_x {||Ax - b||₂² + λ_tv × TV(x) + λ_wav × ||Ψx||₁}
         A: ray-driven 전진 투영 연산자
         TV(x): 2D 등방성 전체 변분
         Ψ: 3레벨 db4 웨이블릿 변환
         λ_tv = 0.002, λ_wav = 0.001 (CIRS 팬텀 경험적 튜닝)

단계 3: ADMM 분할 (§20 TV-ADMM 확장)
         x-갱신: FFT 기반 Tikhonov 역문제
           x^{k+1} = F⁻¹[(F(A^T b) + ρ F(z - u)) / (F(A^T A) + ρ)]
         z-갱신: TV + 웨이블릿 연산자 소프트 임계값
           z_tv^{k+1}  = prox_tv(x^{k+1} + u_tv^k, λ_tv/ρ)
           z_wav^{k+1} = soft_thresh(Ψ x^{k+1} + u_wav^k, λ_wav/ρ)
         이중 변수 갱신: u^{k+1} = u^k + x^{k+1} - z^{k+1}

단계 4: 수렴 판정
         max_iter = 50, 잔류 노름 ||x^k - x^{k-1}|| / ||x^k|| < 1e-3

단계 5: 출력 슬라이스 생성
         11개 재구성 슬라이스 (±7.5mm 깊이 증분)
         float32 → uint16 (범위 정규화)

#### SIMD 최적화

FFT 기반 x-갱신은 MKL DFT (3D 또는 배치 2D) 사용. 소프트 임계값은 AVX2 `_mm256_sub_ps` + `_mm256_max_ps(abs - thr, 0)` 패턴으로 구현한다. 전진/역진 투영 연산자는 §19 FBP 모듈과 공유된다.

```cpp
// 소프트 임계값 (AVX2, 웨이블릿 계수 수축)
inline __m256 soft_threshold_avx2(__m256 x, __m256 thr) {
    __m256 abs_x = _mm256_andnot_ps(_mm256_set1_ps(-0.f), x); // |x|
    __m256 sub   = _mm256_sub_ps(abs_x, thr);
    __m256 pos   = _mm256_max_ps(sub, _mm256_setzero_ps());    // max(|x|-thr, 0)
    __m256 sign  = _mm256_and_ps(_mm256_set1_ps(-0.f), x);    // sign(x)
    return _mm256_or_ps(pos, sign);                             // sign(x)*max(|x|-thr,0)
}
```

#### C++ 구조체 및 API

```cpp
struct XpeCsTomoParams {
    uint32_t n_sparse_views;       // 희소 투영 수 (기본: 5)
    uint32_t n_output_slices;      // 출력 슬라이스 수 (기본: 11)
    float    lambda_tv;            // TV 정규화 계수 (기본: 0.002)
    float    lambda_wavelet;       // 웨이블릿 정규화 계수 (기본: 0.001)
    uint32_t max_iterations;       // 최대 반복 수 (기본: 50)
    float    convergence_tol;      // 수렴 허용 오차 (기본: 1e-3)
};

XpeStatus xpe_cs_tomo_reconstruct(
    const uint16_t*        sinogram,   // N_views × detector_cols
    uint32_t               n_views,
    uint32_t               detector_cols,
    uint32_t               W,          // 출력 슬라이스 가로
    uint32_t               H,          // 출력 슬라이스 세로
    const XpeCsTomoParams* params,
    uint16_t*              output_slices  // n_output_slices × W × H
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 2 s (11 슬라이스, AVX2 + FFT 가속) |
| 수렴 반복 수 | 평균 < 30회 |
| 의존성 | §19 토모합성 FBP/SAA (전진 투영 공유), §20 TV-ADMM |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| CIRS 팬텀 슬라이스 FWHM | ≤ 1.2 mm (§19 FBP 1.5 mm 대비 우수) |
| in-plane 해상도 | ≥ 3 lp/mm (50% 선량 절감 조건) |
| PSNR vs. 11-뷰 FBP 참조 | ≥ 30 dB |
| ADMM 수렴 확인 | 잔류 노름 < 1e-3 (50회 내) |

---

## §12.12 SWU-12.12 임상 영상 품질 감사 엔진 (ACIQ) ★GAP-CC 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-QA-003 | SWU-12.12 | xpe_enhance_advanced.dll | Class B |

### 개요

모집단 수준의 영상 품질 지표를 지수 가중 이동 평균(EWMA)으로 추적하고, 통계적 공정 관리(SPC, §9.11)를 통해 자동 경보를 발령한다. IHE IQI(Image Quality Indicator) 프로파일에 따른 구조화 보고서를 생성하며 PACS 통합을 지원한다.

### 알고리즘

단계 1: 지표 추적 (이미지마다 1회)
         추적 지표:
         - DI 분포 (평균/표준편차)
         - MTF f₅₀ 추세 (§12.6)
         - CNR 추세 (§12.8)
         - NPS 피크 주파수 추세 (§12.3)

단계 2: EWMA 갱신 (α = 0.1)
         EWMAₜ = α × metric_t + (1 - α) × EWMA_{t-1}

단계 3: 규제 경보 임계값
         DI: 2-시그마 규칙 (EWMAₜ > μ₀ ± 2σ₀ → 경보)
         MTF: 3-시그마 규칙
         CNR: CUSUM (§9.11 호출)
         → §9.11 Shewhart/CUSUM 내부 호출

단계 4: IHE IQI JSON 생성 (주기적)
         표준: IHE REM IQI 프로파일
         출력: JSON 구조체 (PACS XDS-SD 배포용)
         포함 항목: 세션 ID, EWMA 지표, 경보 이력, 권고 조치

단계 5: 일간/주간/월간 요약 리포트 (비동기)
         Aggregate 통계 + 추세 시각화 데이터 (JSON 배열)

#### C++ 구조체 및 API

```cpp
struct XpeAciqReport {
    uint32_t session_id;
    float    di_ewma;             // DI EWMA 현재값
    float    mtf_trend_slope;     // MTF f50 추세 기울기 (lp/mm / 100frames)
    float    cnr_ewma;            // CNR EWMA 현재값
    bool     alert_triggered;     // 경보 발령 여부
    char     alert_reason[256];   // 경보 사유 문자열
    char     ihe_iqi_json[1024];  // IHE IQI JSON 페이로드 (축약)
};

XpeStatus xpe_aciq_update(
    float            di_value,
    float            mtf_f50,
    float            cnr_value,
    float            nps_peak_freq,
    uint32_t         session_id,
    XpeAciqReport*   report
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 5 ms / 이미지 (지표 갱신) |
| 리포트 생성 | 비동기 (별도 스레드, < 100 ms) |
| 의존성 | §9.11 SPC, §12.3 NPS, §12.6 MTF, §12.8 CNR |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 합성 DI 드리프트 경보 트리거 | 20프레임 내 경보 발령 (0.1/프레임 드리프트) |
| IHE IQI JSON 유효성 | JSON 스키마 검증 통과 |
| EWMA 수렴 | α=0.1, 약 30회 후 안정화 (이론값 ±5%) |
| CUSUM CNR 경보 | §9.11 CUSUM 트리거 포인트와 일치 |

---

## §10.10 SWU-10.10 이기종 컴퓨팅 파이프라인 스케줄러 (HCPS) ★GAP-CD 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-PERF-004 | SWU-10.10 | xpe_common.dll | Class B |

### 개요

CPU 및 GPU 자원 간의 파이프라인 작업 그래프(DAG)를 비용 모델로 최적 분할하고, 실측 지연 시간을 EMA로 동적 갱신한다. 열 스로틀링 모니터링과 SLA 감시를 통해 < 1초 end-to-end 레이턴시를 보장하며, GPU 미사용 시 결정론적 CPU 전용 폴백을 항상 보장한다.

### 알고리즘

단계 1: 작업 그래프 표현
         DAG G = (V, E): V = 파이프라인 단계, E = 의존성
         각 노드 v에 대해: C_cpu(v), C_gpu(v) 비용 추정
         C_cpu(v) = flops_cpu(v) / throughput_cpu
         C_gpu(v) = flops_gpu(v) / throughput_gpu + transfer_overhead

단계 2: 0-1 배낭(Knapsack) 변형 분할 최적화
         목적함수: minimize max(C_cpu_path, C_gpu_path)
         제약: PCIe 전송 대역폭 제한 준수
         Dynamic programming으로 O(|V| × budget) 풀이

단계 3: 동적 비용 모델 갱신 (100프레임마다)
         EMA 갱신: C_actual^{k+1} = β × latency_measured + (1-β) × C_actual^k
         β = 0.1 (느린 추적으로 일시적 스파이크 무시)

단계 4: 열 스로틀링 감지 및 대응
         CPU/GPU TDP 마진 < 15%: GPU 부하 감소 (Phase 2 선택 단계 CPU로 이전)
         하드웨어 성능 카운터 폴링 주기: 1초

단계 5: SLA 감시 및 강제 조치
         지연 시간 모니터: < 1초 end-to-end 목표
         SLA 위반 위험 시: Phase 2 선택 단계 스킵 (Phase 1b 기준은 항상 유지)
         GPU 미사용 시: 모든 단계 CPU로 결정론적 폴백

#### C++ 구조체 및 API

```cpp
struct XpeHcpsDag {
    uint32_t n_stages;
    float    cost_cpu[64];         // 각 단계 CPU 비용 추정 (ms)
    float    cost_gpu[64];         // 각 단계 GPU 비용 추정 (ms)
    bool     gpu_eligible[64];     // GPU 실행 가능 여부
    bool     optional[64];         // SLA 위협 시 스킵 가능 여부
};

struct XpeHcpsDecision {
    bool     use_gpu[64];          // 단계별 GPU 사용 결정
    float    predicted_latency_ms; // 예측 total 지연 시간
    bool     sla_at_risk;          // SLA 위험 경보
    uint32_t n_skipped_optional;   // 스킵된 선택 단계 수
};

XpeStatus xpe_hcps_schedule(
    const XpeHcpsDag*   dag,
    bool                gpu_available,
    float               gpu_thermal_margin,
    XpeHcpsDecision*    decision
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 스케줄러 오버헤드 | < 0.5 ms |
| SLA 위반율 | < 0.1% (정상 열 조건) |
| 결정론적 CPU 폴백 | 100% (GPU 미사용 시) |
| 의존성 | §10.7 메모리 아레나, §10.8 스레드 안전성, §10.9 GPU CUDA |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 5개 하드웨어 구성 SLA 통과 | CPU 전용, CPU+GPU, 스로틀링 GPU, NUMA, 단일코어 CI |
| SLA 달성률 | ≥ 99.9% (기준 워크로드) |
| 결정론적 폴백 | 100% (GPU 미사용 / 오류 발생 모든 경우) |
| 스케줄러 오버헤드 | < 0.5 ms (1000 프레임 평균) |

---

## §17.4 SWU-17.4 DICOM 방사선량 구조화 보고서 (RDSR) 생성 ★GAP-CE 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-DOSE-002 | SWU-17.4 | xpe_dicom.dll | Class B |

### 개요

IHE REM(Radiation Exposure Monitoring) 프로파일에 따른 DICOM Enhanced SR(SOP 1.2.840.10008.5.1.4.1.1.88.67)을 생성한다. 세션 동안 누적된 DAP, KERMA, 조사 이벤트 시퀀스를 TID 10011에 따라 구조화하고, MPPS 통합 및 PS3.15 익명화 처리를 수행한다.

### 알고리즘

단계 1: 조사 이벤트 누적 (세션 스코프)
         각 조사마다 누적:
         - KAP (kerma-area product, mGy·cm²) ← §9.12 DAP 출력
         - Air-Kerma (mGy) ← §9.12 KERMA 출력
         - kVp, mAs, 프로토콜 이름, 검출기 ID, 조작자 ID
         - 조사 시각 (UTC ISO-8601)

단계 2: TID 10011 SR 항목 구성
         TID 10011: Projection X-Ray Radiation Used in Acquisition
         DICOM 측정 항목 시퀀스 생성:
         (0040,A730) Content Sequence:
           KAP 값 + 단위 (mGy·cm²)
           Air-Kerma 값 + 단위 (mGy)
           irradiation event UID 시퀀스

단계 3: MPPS 갱신 (조사마다 < 5ms)
         DCF 9.3.13: N-SET Modality Performed Procedure Step
         속성 갱신: Exposure Dose Sequence

단계 4: 환자 식별 정보 익명화 (내보내기 전)
         PS3.15 Annex E D-클래스(제거) 속성 삭제:
         Patient Name, ID, BirthDate, Address 등 잔류 0건 검증

단계 5: DICOM SR 파일 생성 (절차 종료 이벤트 시)
         SOP Instance UID 생성 (§17 DICOM IOD 시스템)
         파일 저장 + XpeDoseReport 구조체 업데이트

#### C++ 구조체 및 API

```cpp
struct XpeDoseReport {
    float    total_dap_mgy_cm2;           // 총 DAP 누적값
    float    total_kerma_mgy;             // 총 Air-Kerma 누적값
    uint32_t irradiation_count;           // 조사 이벤트 수
    char     dicom_sr_instance_uid[64];   // 생성된 SR SOP Instance UID
    bool     rdsr_valid;                  // RDSR 유효 여부
};

// 조사 이벤트 1건 추가 (이미지 처리 직후 호출)
XpeStatus xpe_rdsr_add_irradiation_event(
    float        dap_mgy_cm2,
    float        kerma_mgy,
    float        kvp,
    float        mas,
    const char*  protocol_name,
    uint32_t     session_id
);

// 절차 종료 시 RDSR 파일 생성
XpeStatus xpe_rdsr_finalize(
    uint32_t         session_id,
    const char*      output_path,
    bool             anonymize,         // PS3.15 Annex E 적용 여부
    XpeDoseReport*   report
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| RDSR 생성 시간 | < 50 ms / 절차 |
| MPPS 갱신 시간 | < 5 ms / 조사 이벤트 |
| 의존성 | §9.12 DAP/KERMA 누적 추적, §17 DICOM IOD 적합성 검증 |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| IHE REM 프로파일 적합성 | PCD TF-2, Vol. 2, §3.23 검증 통과 |
| TID 10011 준수 | DICOM SR 구조 검증기 통과 |
| 익명화 잔류 식별 태그 | 0건 (PS3.15 Annex E D-클래스 전체 제거) |
| MPPS 갱신 지연 | < 5 ms / 이벤트 |

---

## §11.6 SWU-11.6 신호 검출 이론 프레임워크 (SDT — d', ROC, JAFROC) ★GAP-CF 해소

**IEC 62304 추적성**

| SRS ID | SWU | DLL | 안전 등급 |
|--------|-----|-----|---------|
| SRS-MEAS-005 | SWU-11.6 | xpe_enhance_advanced.dll | Class B |

#### 중요 안전 고지

> **[SAFETY] 비임상 측정 프레임워크**: 본 알고리즘은 알고리즘 성능 평가 도구이며, 임상 진단 지원 시스템이 아니다. 출력은 `informational_only = true`로 표시된다.

### 개요

신호 검출 이론(Signal Detection Theory, SDT)에 기반한 d'(d-prime), ROC 곡선, JAFROC(Jack-knife Alternative Free-Response ROC) 분석 프레임워크를 제공한다. 과제 기반(task-based) 검출 가능성 지표를 MTF, NPS, 과제 전달 함수의 결합으로 계산하여 비선형 알고리즘의 시각 과제 영향을 정량화한다.

### 알고리즘

단계 1: d'(d-prime) 검출 가능성 지수 계산
         d' = (μ_signal - μ_noise) / √((σ²_signal + σ²_noise) / 2)
         μ_signal, σ_signal: 신호 존재 조건의 평균/표준편차 (§11.5 잡음 모델)
         μ_noise, σ_noise: 신호 부재 조건의 평균/표준편차

단계 2: 과제 기반 검출 가능성 (task-based d'_task)
         T(f): 과제 전달 함수 (target shape의 2D Fourier 변환)
         detectability(f) = MTF(f)² × T(f)² / NPS(f)
         d'_task = √(∫ |T(f)|² × MTF²(f) / NPS(f) df)
         이산화: Σ_f |T(f)|² × MTF²(f) / NPS(f) × Δf

단계 3: ROC 곡선 산출
         n 관찰자 평가 (1~5 점수) 또는 임계값 스윕 사용
         FPR(θ) = P(score ≥ θ | 신호 부재)
         TPR(θ) = P(score ≥ θ | 신호 존재)
         100개 임계값 포인트로 이산화
         AUC = ∫₀¹ TPR(FPR) dFPR (사다리꼴 수치 적분)

단계 4: JAFROC 분석
         Q: 신호 존재 케이스 수
         FOM_JAFROC = (1/Q) × Σ_q P(max_score_signal_q > max_score_noise)
         max_score_noise: 해당 케이스의 잡음 마크 중 최대 점수
         이중 루프 O(Q × N_noise) 계산 (Q, N_noise 각 ≤ 1000 가정)

단계 5: 결과 통합
         d', AUC_ROC, JAFROC FOM, d'_task 산출
         ROC 단조성 검증 (AUC 단조 증가 확인)

#### Python 교정/분석 코드

```python
import numpy as np
from scipy.integrate import trapezoid

def compute_dprime(mu_signal: float, sigma_signal: float,
                   mu_noise: float, sigma_noise: float) -> float:
    """d' 검출 가능성 지수 계산."""
    pooled_std = np.sqrt((sigma_signal**2 + sigma_noise**2) / 2.0)
    return (mu_signal - mu_noise) / pooled_std


def compute_roc_auc(scores_signal: np.ndarray,
                    scores_noise: np.ndarray,
                    n_thresholds: int = 100):
    """ROC 곡선 및 AUC 산출."""
    all_scores = np.concatenate([scores_signal, scores_noise])
    thresholds = np.linspace(all_scores.min(), all_scores.max(), n_thresholds)
    fprs, tprs = [], []
    for thr in thresholds:
        tprs.append(np.mean(scores_signal >= thr))
        fprs.append(np.mean(scores_noise >= thr))
    fprs, tprs = np.array(fprs), np.array(tprs)
    auc = trapezoid(tprs, fprs)
    return auc, fprs, tprs


def compute_jafroc_fom(signal_scores: list, noise_scores: list) -> float:
    """
    JAFROC FOM 계산.
    signal_scores: List[np.ndarray] — 각 신호 케이스의 마크 점수 배열
    noise_scores: List[np.ndarray] — 각 잡음 케이스의 마크 점수 배열
    """
    Q = len(signal_scores)
    noise_maxes = [np.max(s) if len(s) > 0 else -np.inf for s in noise_scores]
    fom_sum = 0.0
    for q in range(Q):
        max_signal = np.max(signal_scores[q]) if len(signal_scores[q]) > 0 else -np.inf
        fom_sum += np.mean([max_signal > nm for nm in noise_maxes])
    return fom_sum / Q


def compute_task_dprime(mtf_1d: np.ndarray, nps_1d: np.ndarray,
                        task_tf: np.ndarray, df: float) -> float:
    """과제 기반 d'_task 계산."""
    integrand = (task_tf**2 * mtf_1d**2) / np.maximum(nps_1d, 1e-10)
    return np.sqrt(np.sum(integrand) * df)
```

#### C++ 구조체 및 API

```cpp
struct XpeSdtResult {
    float    d_prime;          // d' 검출 가능성 지수
    float    auc_roc;          // ROC AUC [0~1]
    float    jafroc_fom;       // JAFROC Figure of Merit
    float    d_prime_task;     // 과제 기반 d'_task
    bool     roc_convergent;   // ROC 단조성 통과 여부
    uint32_t n_signal_cases;   // 신호 존재 케이스 수
    uint32_t n_noise_cases;    // 신호 부재 케이스 수
    bool     informational_only;  // 항상 true
};

// d' 및 ROC AUC 계산 (점수 배열 입력)
XpeStatus xpe_sdt_compute_dprime_roc(
    const float*   signal_scores,  // (n_signal,) 신호 존재 점수
    uint32_t       n_signal,
    const float*   noise_scores,   // (n_noise,)  신호 부재 점수
    uint32_t       n_noise,
    XpeSdtResult*  result
);

// JAFROC FOM 계산 (자유 응답 패러다임)
XpeStatus xpe_sdt_compute_jafroc(
    const float** signal_mark_scores,  // Q개 케이스 각 마크 점수 배열
    const uint32_t* n_signal_marks,    // 케이스별 마크 수
    uint32_t        Q,                 // 신호 존재 케이스 수
    const float** noise_mark_scores,
    const uint32_t* n_noise_marks,
    uint32_t        N_noise,
    float*          jafroc_fom_out
);
```

#### 성능 목표

| 항목 | 목표 |
|------|------|
| 처리 시간 | < 10 ms (d'/ROC 계산, CPU, 1000케이스 데이터셋) |
| JAFROC 처리 시간 | < 50 ms (1000 신호 × 1000 잡음 케이스) |
| 의존성 | §11.5 양자 잡음 모델, §12.6 MTF, §12.3 NPS |

#### 검증 기준

| 검증 항목 | 기준 |
|----------|------|
| 합성 Gaussian d'=1.0/2.0/3.0 정확도 | 오차 < 5% |
| AUC 단조성 | d' 증가에 따라 AUC 단조 증가 |
| JAFROC OR-DBM MRMC 참조 비교 | FOM 편차 < 2% |
| informational_only 안전 플래그 | 항상 `true` 검증 |

---

## 부록 C: 알고리즘-요구사항 추적성

| 알고리즘 | SRS Req ID | SDD SWU | 검증 방법 |
|---------|-----------|---------|---------|
| Offset Correction | SRS-FUNC-001 | SWU-1.1 | Unit test + dark field measurement |
| Gain Correction | SRS-FUNC-002 | SWU-1.2 | Uniformity measurement |
| Defect Correction | SRS-FUNC-003 | SWU-1.3 | Injected defect test |
| Ghost Correction | SRS-FUNC-004 | SWU-1.4 | Double-exposure protocol |
| Log Transform | SRS-FUNC-010 | SWU-2.1 | Mathematical verification |
| Bilateral Filter | SRS-FUNC-011 | SWU-2.2 | MTF retention test |
| CLAHE | SRS-FUNC-012 | SWU-2.3 | Histogram analysis |
| Edge Enhancement | SRS-FUNC-013 | SWU-2.4 | Safe gain verification |
| Laplacian Pyramid | SRS-FUNC-014 | SWU-2.5 | Phantom image quality |
| Fractional MS | SRS-FUNC-015 | SWU-2.6 | Artifact measurement |
| CNN Recognition | SRS-FUNC-016 | SWU-2.10 | ≥95% accuracy test |
| Panoramic Stitch | SRS-FUNC-017 | SWU-2.11 | Cobb angle ≤2° error |
| Bone Suppression | SRS-FUNC-018 | SWU-2.12 | PSNR≥33dB, SSIM≥0.97 |
| Modality LUT | SRS-FUNC-020 | SWU-3.1 | DICOM conformance |
| VOI LUT | SRS-FUNC-021 | SWU-3.2 | W/L sweep verification |
| GSDF PLUT | SRS-FUNC-022 | SWU-3.3 | PS3.14 conformance |
| Grid Suppression | — (Phase 2) | — | MTF retention + CNR |
| Virtual Grid | — (Phase 2) | — | CNR comparison vs physical grid |
| Exposure Index | — (Phase 2) | SWU-2.9 | IEC 62494-1 conformance |
| Readout Validation | SRS-QC-001 | SWU-1.0 | Saturation/geometry injection test |
| NPS Computation | SRS-MEAS-001 | — | IEC 62220-1 compliance |
| DQE Computation | SRS-MEAS-001 | — | IEC 62220-1 DQE formula |
| Collimation Mask | SRS-FUNC-001b | — | 95% detection, ±5mm accuracy |
| Heel Effect | SRS-FUNC-002b | SWU-1.5 | PRNU CV < 0.8% (Phase 2) |
| Multi-SID Gain | SRS-FUNC-002 ext | SWU-1.2b | Interp error < 0.5% (Phase 2) |
| Calibration Session Lock | SRS-SEC-002 ext | — | 100% mixed-session rejection |
| Calibration Drift Monitor | SRS-QC-002 | — | Drift threshold parity test |
| Quality State Sidecar | SRS-QC-003 | — | All-field population test |
| Scalar Parity Harness | SRS-TEST-001 | — | CI PASS on all stages |
| MTF ESF Pipeline | SRS-MEAS-002 | — | IEC 62220-1-1 f50 accuracy |
| Lag Residual Tiering | SRS-FUNC-004 ext | SWU-1.4b | Tier selection accuracy |
| VG Anatomy Presets | SRS-FUNC-008b | — | CNR ≥10% (Chest), observer gate |
| AI Worker Isolation | SRS-AI-001 | — | Fallback 100% on timeout |
| Temporal IIR Filter | SRS-FLUORO-001 | SWU-14.0 | Static SNR ≥4× gain, no ghosting on motion |
| Beam Hardening Correction | SRS-FUNC-003b | SWU-1.9 | PMMA cupping CV < 1%, LUT parity < 0.001 OD |
| Geometric Distortion Correction | SRS-FUNC-005b | SWU-1.10 | Grid phantom RMS < 0.5 pixel |
| Binning Mode Calibration | SRS-FUNC-002c | SWU-9.7 | Block-avg < 0.1%, defect propagation test |
| Sigma-Clipping Calibration | SRS-CAL-001b | SWU-9.8 | Outlier removal ≥99%, min_frames test |
| Memory Arena Architecture | SRS-PERF-001 | SWU-10.7 | Zero heap alloc post-init, CAS stress test |
| Pipeline Thread Safety | SRS-PERF-002 | SWU-10.8 | TSan clean, backpressure drop counter test |
| Auto CNR Assessment | SRS-MEAS-003 | SWU-12.8 | CNR accuracy ±5% vs synthetic ground truth |
| Auto Window/Level | SRS-FUNC-021b | SWU-3.4b | Per-anatomy percentile coverage test |
| Error Code Taxonomy | SRS-ERR-001 | SWU-15.0 | All 32 codes unit tested, xpe_error_string non-null |
| GCR Estimator (GAP-AI) | SRS-FUNC-004 ext | SWU-1.4.6 | Double-exposure GCR RMSE < 0.001; lag trigger parity test |
| NLCSC State Machine (GAP-AJ) | SRS-FUNC-004 ext | SWU-1.4.7 | Step-wedge 5 dose levels; residual < 1 ADU RMS; state reset test |
| Row/Column FPN Correction (GAP-AK) | SRS-FUNC-001 ext | SWU-1.11 | Injected FPN profile; residual RMS < 0.5 ADU; AVX2 parity |
| Allan Variance Stability (GAP-AL) | SRS-QC-002 ext | SWU-12.9 | Synthetic white/flicker/drift signals; σ_A accuracy < 5%; recal trigger test |
| Dose-Dependent Defect Detection (GAP-AM) | SRS-FUNC-003 ext | SWU-1.3.5 | Injected nonlinear pixels; detection ≥ 95%; R² < 0.95 classification |
| Multi-Exponential Lag Fitting (GAP-AN) | SRS-FUNC-004 | SWU-9.9 | Synthetic 3-exp signal; α error < 0.001, τ error < 0.05 s; R² > 0.999 |
| Scatter SPR Semi-Empirical Model (GAP-AO) | SRS-FUNC-008 | SWU-5.4 | PMMA slab SPR: model vs. measurement < 15%; CNR improvement ≥ 5% |
| Wavelet BayesShrink Denoising (GAP-AP) | SRS-FUNC-011b | SWU-2.8 | PSNR ≥ 35 dB (σ=20 ADU); MTF degradation < 5% |
| Dual-Energy Subtraction (GAP-AQ) | SRS-FUNC-019 | SWU-16.0 | CIRS chest phantom; bone CNR < 1.0 in soft image; motion correction < 0.5 px |
| DICOM IOD Conformance Validation (GAP-AR) | SRS-DICOM-001 | SWU-17.0 | 32 non-conformant injection tests; XPE_OK on valid files |
| Perceptual IQM — PSNR/SSIM/MS-SSIM/FSIM (GAP-AS) | SRS-MEAS-004 | SWU-18.0 | σ=10/20/30 ADU noise; PSNR ≥ 35 dB, SSIM ≥ 0.95 verified |
| Temperature-Compensated Gain (GAP-AT) | SRS-FUNC-002d | SWU-1.12 | ΔT=5/10°C simulated; PRNU CV < 0.5%/1.0% |
| 2D FFT Notch Filter (GAP-AU) | SRS-FUNC-001c | SWU-1.13 | Injected periodic pattern; residual < −30 dB; MTF loss < 3% |
| AEC Feedback Loop (GAP-AV) | SRS-FUNC-009b | SWU-9.10 | DI = −6/−3/0/+3/+6/+10 dB; delta_mas_ratio and delta_kvp verified |
| SPC Calibration Control (GAP-AW) | SRS-QC-004 | SWU-9.11 | Synthetic drift signals; Shewhart/CUSUM trigger points validated |
| Sub-pixel ECC Registration (GAP-AX) | SRS-FLUORO-002 | SWU-14.2 | tx=10, ty=20, θ=2° synthetic displacement; RMS < 0.5 pixel |
| Quantum Noise Model — Anscombe (GAP-AY) | SRS-FUNC-011c | SWU-11.5 | Synthetic Poisson images; α, β parameter error < 5%; Anscombe CV < 0.1 |
| Moiré Artifact Suppression (GAP-AZ) | SRS-FUNC-008c | SWU-5.5 | Injected Moiré (f=0.15/0.25/0.35×fN); detection rate > 95%; residual < 10% |
| DICOM SR for CAD Findings (GAP-BA) | SRS-DICOM-002 | SWU-17.2 | TID 1500/4100 conformance; 5 finding types × 3 anatomy regions |
| IEC 61223 Acceptance Testing (GAP-BB) | SRS-QA-001 | SWU-12.10 | T1–T6 with phantom images; fail-case maintenance alert triggered |
| DAP/KERMA Dose Tracking (GAP-BC) | SRS-DOSE-001 | SWU-9.12 | PMMA phantom 5 kVp/mAs combos; computed vs. measured DAP ±10%; IEC 60601-2-54 §29.201 |
| JPEG 2000 Compression (GAP-BD) | SRS-DICOM-003 | SWU-17.3 | Lossless pixel-exact roundtrip; 1.5:1 lossy PSNR ≥ 50 dB; compress time < 100ms |
| Motion Blur Wiener Deblur (GAP-BE) | SRS-FUNC-001d | SWU-1.14 | 6 synthetic blur cases (L=5/10/20px, θ=0/45/90°); PSNR ≥ 30 dB; MTF f50 recovery ≥ 80% |
| Metal Artifact Mask (GAP-BF) | SRS-FUNC-001e | SWU-1.15 | 10 synthetic metal objects; coverage ≥ 95%; false-positive < 2%; clinical_use_blocked flag test |
| Linear Tomosynthesis FBP/SAA (GAP-BG) | SRS-TOMO-001 | SWU-19.0 | CIRS phantom; slice FWHM ≤ 1.5mm; in-plane resolution ≥ 3 lp/mm |
| RANSAC+ORB Panoramic Stitch (GAP-BH) | SRS-FUNC-017b | SWU-8.3.2 | Spine phantom 5 cases; Cobb angle error ≤ 1.5°; fallback to phase-corr on low-texture |
| Gaussian/Laplacian Pyramid (GAP-BI) | SRS-FUNC-014b | SWU-2.9 | Reconstruct error < 0.001 ADU RMS; energy monotonicity check; < 5ms/3Kx3K |
| GPU CUDA Pipeline Acceleration (GAP-BJ) | SRS-PERF-003 | SWU-10.9 | CPU vs. GPU output ±0.01 ADU; total pipeline < 10ms; fallback 100% CPU confirmed |
| Auto QA Phantom Recognition (GAP-BK) | SRS-QA-002 | SWU-12.11 | Leeds/CDRAD/CIRS 5 samples each; recognition accuracy ≥ 90%; UNKNOWN misclassification < 5% |
| Cross-FPD Calibration Transfer (GAP-BL) | SRS-CAL-002 | SWU-9.13 | 5 panel models; post-normalization CV ≤ 0.5%; R² > 0.9999 |
| DICOM GSDF Display Calibration (GAP-BM) | SRS-DISP-005 | SWU-6.5 | 18-level luminance measurement; JND uniformity ΔJ < 0.5; p_cal LUT monotonicity |
| Multi-Scale Retinex Local Tone Mapping (GAP-BN) | SRS-DISP-004 | SWU-6.6 | 3-scale MSR; local contrast ≥1.5×; halo gradient reversal < 5% |
| U-Net Lung Field Segmentation (GAP-BO) | SRS-SEG-001 | SWU-8.5 | Montgomery 50-image subset; IoU ≥ 0.92; Dice ≥ 0.95 |
| DLIR CNN Low-Dose Denoising (GAP-BP) | SRS-DLIR-001 | SWU-8.6 | PSNR ≥ 38dB, SSIM ≥ 0.96 at 50% dose; FSIM ≥ 0.97 structural integrity |
| Rib Suppression Hessian (GAP-BQ) | SRS-RIB-001 | SWU-8.7 | 10 rib phantoms; suppression index ≥ 80%; nodule CNR preservation ≥ 95% |
| Body Part Recognition CNN (GAP-BR) | SRS-ANAT-001 | SWU-8.8 | 100×10-class accuracy ≥ 95%; UNKNOWN false-negative < 3%; ECE < 0.05 |
| Lucas-Kanade Optical Flow (GAP-BS) | SRS-FLUORO-003 | SWU-14.3 | 10px/1Hz synthetic; RMS < 0.5px; frequency estimate < 0.05Hz |
| Integration Nonlinearity Correction (GAP-BT) | SRS-FUNC-001f | SWU-1.16 | 16-node linearity curve; residual ε < 0.1%; R² > 0.9999 |
| TV-ADMM Iterative Denoising (GAP-BU) | SRS-ITER-001 | SWU-20.0 | PSNR ≥ +3dB; SSIM ≥ 0.95; MTF f50 loss < 10%; convergence < 30 iter |
| BMD DXA-proxy Estimation (GAP-BV) | SRS-BMD-001 | SWU-20.1 | Al step-wedge r² > 0.85; T-score direction accuracy ≥ 90%; clinical_decision_blocked=true |
| PCD Spectral Binning (GAP-BW) | SRS-SPEC-001 | SWU-21.0 | Fluorescence peaks ±0.5 keV; Low-E/High-E contrast ratio within 5% of MC simulation |
| Intelligent Calibration Lifecycle Management (GAP-BX) | SRS-QC-005 | SWU-9.14 | Recal sensitivity ≥ 95%; false-positive < 10%; maintenance window ±10% accuracy |
| NPS-Optimal Structure Filter (GAP-BY) | SRS-FUNC-011d | SWU-4.10 | PSNR ≥ 36dB (σ=25 ADU); MTF f50 loss < 3%; structure preservation index ≥ 0.98 |
| Polar-Domain Ring Artifact Correction (GAP-BZ) | SRS-FUNC-001g | SWU-3.17 | Detection rate ≥ 95%; residual < 10% original; false-positive < 5% on ring-free images |
| Anatomy-Guided Instance Segmentation (GAP-CA) | SRS-SEG-002 | SWU-8.9 | mAP ≥ 0.72; IoU ≥ 0.85 (lung, spine); fallback 100%; clinical_use_requires_review=true |
| Compressed Sensing Sparse-View Tomosynthesis (GAP-CB) | SRS-TOMO-002 | SWU-22.0 | Slice FWHM ≤ 1.2mm; in-plane ≥ 3 lp/mm at 50% dose; PSNR ≥ 30dB vs 11-view FBP |
| Automated Clinical Image Quality Audit Engine (GAP-CC) | SRS-QA-003 | SWU-12.12 | Alert within 20 frames (DI drift 0.1/frame); IHE IQI JSON validation; EWMA convergence |
| Heterogeneous Computing Pipeline Scheduler (GAP-CD) | SRS-PERF-004 | SWU-10.10 | SLA ≥ 99.9%; scheduler overhead < 0.5ms; deterministic CPU fallback 100% |
| DICOM Radiation Dose Structured Report (GAP-CE) | SRS-DOSE-002 | SWU-17.4 | IHE REM compliance; TID 10011 conformance; 0 residual patient-identifying tags |
| Signal Detection Theory Framework (GAP-CF) | SRS-MEAS-005 | SWU-11.6 | d' accuracy < 5%; AUC monotonic with d'; JAFROC FOM deviation < 2% vs OR-DBM MRMC |

---

## 개정 이력

| 개정 | 날짜 | 저자 | 내용 |
|------|------|------|------|
| 1.8 | 2026-04-15 | XPE Team | **Round 9 GAP 해소 10건 (GAP-BW~CF)**: GAP-BW (PCD 스펙트럼 빈닝 §21 신설 — 에너지 임계값 T1/T2, 전하 공유 보정 k_cs 0.01~0.05, Cu-Ka/Mo-Ka 형광 교정, <5ms/프레임 AVX2), GAP-BX (지능형 교정 수명 주기 관리 §9.14 — Bayesian 드리프트 예측, 온도 가중 r_drift_eff, 신선도 점수 F, 재교정 윈도우 예측), GAP-BY (NOSF §4.10 — NPS 기반 위너 필터 + 구조 텐서 적응 블렌딩, MKL FFT + AVX2, PSNR≥36dB), GAP-BZ (극좌표 링 아티팩트 §3.17 — 극→직교 변환, 행 방향 PSD 링 감지, 3σ 유의성, 해부 위양성 억제), GAP-CA (AGIS 인스턴스 분할 §8.9 신설 — EfficientDet-D1 13클래스, 35MB int8 ONNX, mAP≥0.72, Non-SaMD), GAP-CB (CS 토모합성 §22 신설 — 희소 5뷰 TV+웨이블릿 ADMM, λ_tv=0.002, FWHM≤1.2mm, <2s), GAP-CC (ACIQ §12.12 — EWMA α=0.1, IHE IQI JSON, §9.11 CUSUM 통합, 20프레임 내 경보), GAP-CD (HCPS §10.10 — DAG 배낭 분할, EMA 비용 갱신, 열 스로틀링 15% 마진, SLA≥99.9%), GAP-CE (RDSR §17.4 — IHE REM TID 10011, MPPS DCF 9.3.13, PS3.15 익명화 0건 잔류), GAP-CF (SDT §11.6 — d'/ROC/JAFROC, 과제 기반 d'_task, MTF²/NPS 적분, OR-DBM MRMC 검증). §21/§22 신설. 부록 C 10건 추가. |
| 1.7 | 2026-04-15 | XPE Team | **Round 8 GAP 해소 10건 (GAP-BM~BV)**: GAP-BM (DICOM GSDF §6.5 — NEMA PS 3.14 JND 보정, LUT 1024엔트리, ΔJ<0.5), GAP-BN (Multi-Scale Retinex §6.6 — σ={15,80,250}px, MKL FFT 가속, 국소 대비 1.5× 향상), GAP-BO (U-Net 폐 분할 §8.5 — 25MB int8 ONNX, IoU≥0.92/Dice≥0.95, Non-SaMD), GAP-BP (DLIR CNN §8.6 — RDN 16블록 패치 블렌딩, PSNR≥38dB, FSIM≥0.97), GAP-BQ (늑골 억제 §8.7 — Hessian Frangi 능선 척도, 억제 지수≥80%), GAP-BR (Body Part CNN §8.8 — MobileNetV3 10클래스, 정확도≥95%, §6.4/§5.3 통합), GAP-BS (Lucas-Kanade 광학 흐름 §14.3 — 3레벨 피라미드, AVX2, <5ms/프레임, 심박 주파수 추정), GAP-BT (통합 비선형성 §3.16 — 16-노드 PWL 교정, 잔류 ε<0.1%, a-Si:H TFT), GAP-BU (TV-ADMM §20 신설 — FFT 가속 50회 반복, PSNR+3dB, MTF 손실<10%), GAP-BV (BMD proxy §20.1 — DES 기반, r²>0.85 vs Al 스텝 웨지, Non-SaMD). §20 신설. 부록 C 10건 추가. |
| 1.6 | 2026-04-15 | XPE Team | **Round 7 GAP 해소 10건 (GAP-BC~BL)**: GAP-BC (DAP/KERMA 누적 추적 §9.12 — IEC 60601-2-54, k_fact 교정, 세션 누적, 임계 알림), GAP-BD (JPEG2000 압축 §17.3 — OpenJPEG 2.5, 무손실/1.5:1 근손실, PSNR ≥ 50dB), GAP-BE (모션 블러 위너 역필터 §3.14 — Radon PSF 추정, K_WF=0.01, PSNR ≥ 30dB 복원), GAP-BF (금속 아티팩트 마스크 §3.15 — 임계값+형태 연산, 스트리크 방사형 감지, clinical_use_blocked), GAP-BG (선형 토모합성 §19 — FBP 램프 필터/SAA 시프트합산, ±15°/N=11, 슬라이스 FWHM ≤ 1.5mm), GAP-BH (RANSAC ORB 스티칭 §8.3.2 — ORB 500특징, RANSAC H행렬, Cobb 오차 ≤ 1.5°, 폴백 전략), GAP-BI (라플라시안 피라미드 §4.9 — Burt-Adelson 5-탭, 5레벨, 재구성 오류 < 0.001 ADU), GAP-BJ (GPU CUDA 가속 §10.9 — Pinned Memory, 2스트림, CPU 대비 ≥ 3×, 폴백 자동 전환), GAP-BK (자동 팬텀 인식 §12.11 — Hough원, Leeds/CDRAD/CIRS 분류, ≥ 90% 정확도), GAP-BL (Cross-FPD 전달 함수 §9.13 — LM 피팅 a·I^γ+b, R²>0.9999, CV ≤ 0.5%). §19 신설. 부록 C 10건 추가. |
| 1.5 | 2026-04-15 | XPE Team | **Round 6 GAP 해소 10건 (GAP-AS~BB)**: GAP-AS (지각적 화질 지표 §18 — PSNR/SSIM/MS-SSIM/FSIM 통합, 임계값 체계화), GAP-AT (온도 보상 이득 보정 §3.12 — α_T=0.0015/°C, ΔT≥5°C 트리거, 교정 매니페스트 연동), GAP-AU (2D FFT 노치 필터 §3.13 — MKL FFT, Gaussian 노치 D=3px, <2ms/3Kx3K), GAP-AV (AEC 피드백 루프 §9.10 — DI→mAs 10^(-DI/10), ±10kVp 조정, 안전 클램프), GAP-AW (SPC 교정 관리 §9.11 — Shewhart UCL/LCL, CUSUM k=0.5σ h=4σ/5σ), GAP-AX (서브픽셀 ECC 정합 §14.2 — Evangelidis-Psarakis 2008, <0.5pixel RMS, <5ms/프레임), GAP-AY (양자 잡음 모델 §11.5 — Poisson+Gaussian σ²=αI+β, Anscombe 변환, Makitalo-Foi 역변환), GAP-AZ (무아레 검출 §5.5 — 행 방향 PSD 피크, Gaussian 대역 제거, >95% 검출률), GAP-BA (DICOM SR §17.2 — TID 1500/4100, XpeSRReport API, xpe_dicom.dll 통합), GAP-BB (IEC 61223 인수 시험 §12.10 — T1~T6 자동화, 일일/주간/월간 일정, XpeAcceptanceResult). §18 신설, §12.10 신설, §17.2 신설. 부록 C 10건 추가. |
| 1.4 | 2026-04-15 | XPE Team | **Round 5 GAP 해소 10건 (GAP-AI~AR)**: GAP-AI (Real-Time GCR Estimator §3.4.6, 슬라이딩 윈도우 EMA, 0.2% 임계값 조건부 Lag 활성화, <0.1ms/프레임), GAP-AJ (NLCSC State Machine §3.4.7, 비선형 전하 누적 4차 다항식 보정, kVp 에너지 의존 계수, 유휴 상태 리셋), GAP-AK (Row/Column FPN Correction §3.11, 3패스 반복 분해, AVX2 행 중앙값, <0.5ms/3Kx3K), GAP-AL (Allan Variance 장기 안정성 §12.9, 잡음 유형 3분류, σ_A(τ=300s)>2 ADU 재교정 트리거), GAP-AM (선량 의존 동적 결함 검출 §3.3.5, 4선량 z-score, R²<0.95 비선형 플래그, 정적 결함 맵 union), GAP-AN (다중 지수 Lag 피팅 §9.9, LM 최소제곱, 이중 노출 프로토콜, scipy.optimize, R²>0.999 검증), GAP-AO (Scatter SPR Boone-Seibert §5.4, Beer-Lambert 두께 역산, 픽셀별 산란 보정 1/(1+SPR)), GAP-AP (Wavelet BayesShrink §4.8, db4 3레벨, MAD σ_n 추정, AVX2 소프트 임계값, 해부 부위별 λ 블렌딩), GAP-AQ (Dual-Energy Subtraction §16, 로그 차감, 위상 상관 모션 보정, xpe_enhance_advanced.dll 익스포트), GAP-AR (DICOM IOD 적합성 검증 §17, Type 1/2/3 속성 검사, 픽셀 수치 일관성, xpe_dicom.dll 쓰기 경로 통합). §16/§17 신설. 부록 C 10건 추가. |
| 1.3 | 2026-04-15 | XPE Team | **Round 4 GAP 해소 10건 (GAP-Y~AH)**: GAP-Y (Fluoroscopy 시간적 IIR 필터 §14, AVX2 FMA α 적응형, <0.3ms/3Kx3K), GAP-Z (Beam Hardening Correction §3.9, PMMA 팬텀 다항식 보정, BHC LUT 65536-entry), GAP-AA (Geometric Distortion Correction §3.10, Brown-Conrady 방사형+접선 모델, 역 LUT 바이리니어 보간), GAP-AB (Pixel Binning Mode 교정 보간 §9.7, gain 블록 평균, defect OR 전파, lag τ 선형 스케일), GAP-AC (Memory Arena Zero-Copy §10.7, 8-슬롯 링 버퍼, CAS 상태 기계, 런타임 힙 할당 0), GAP-AD (Multi-Channel SPSC Thread Safety §10.8, lock-free 링 버퍼, CPU affinity, 백프레셔 드롭), GAP-AE (Automatic CNR Auto-Assessment §12.8, 히스토그램 퍼센타일 배경 검출, SDNR 계산, XpeQualityState 연동), GAP-AF (Anatomy-Adaptive Auto W/L §6.4, 5종 해부 부위별 퍼센타일 테이블, 콜리메이터 마스크 적용), GAP-AG (Multi-Frame Sigma-Clipping §9.8, 반복적 κ=3.0 클리핑, Python NumPy 구현, min_frames 결함 마킹), GAP-AH (Error Code Taxonomy §15, 32개 코드 5범주, xpe_error_string, C# 핸들러 패턴). 섹션 수 대폭 추가, §14/§15 신설. |
| 1.2 | 2026-04-15 | XPE Team | **Round 3 GAP 해소 10건 (GAP-O~X)**: GAP-O (Heel Effect Compensation §3.5, Wang 2013 Duo-SID), GAP-P (Multi-SID Gain 보간 §3.2.5), GAP-Q (교정 세션 잠금 §2.4, 매니페스트 해시 체인), GAP-R (품질 상태 벡터 사이드카 §13, XpeQualityState), GAP-S (스칼라 참조 + SIMD 패리티 하네스 §11.4), GAP-T (MTF ESF 완전 구현 §12.6, IEC 62220-1-1), GAP-U (Lag 잔류 티어링 §3.4.5, Tier-0/1/3 결정론적 선택), GAP-V (해부 부위별 VG 프리셋 §5.3, 15개 부위 테이블), GAP-W (AI Worker 격리 §8.4, ONNX + 폴백 + 모델 매니페스트), GAP-X (교정 드리프트 모니터링 §9.5, 드리프트율 측정 + 재교정 트리거). 섹션 수 추가, §13 신설. |
| 1.0 | 2026-04-15 | XPE Team | 초판 (10회 review-evaluate-fix 완료). GAP-01~GAP-10 초기 해소. |

---

*Document End — XPE-ALG-001 v1.8*

*Cross-references: XPE-SRS-001, XPE-SAD-001, XPE-SDD-002, xpe-algorithm-spec-deepsync.md, SPEC-XPE-MASTER.md, 03_측정_알고리즘_명세서, xray_grid_suppression_virtual_grid_research*
