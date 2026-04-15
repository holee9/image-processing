# X-ray FPD 고급 이미지 개선 모듈

**모듈**: `xpe_enhance_advanced.dll` (Layer 1, Phase 2)  
**소유자 DLL**: `xpe_enhance_advanced.dll`  
**의존성**: `xpe_enhance_basic.dll` (선행 처리), `xpe_common.dll` (Layer 0)  
**안전 등급**: IEC 62304 Class B  
**문서 버전**: 2.0  
**날짜**: 2026-04-15  
**최종 갱신**: 2026-04-15 (5차 교차검증, Wiener 필터/PSNR·SSIM/OpenMP 요구사항 추가)
**규범 사양**: [ALG-SPEC-001 v3.0.0-ds2](../../.moai/specs/xpe-algorithm-spec-deepsync.md)

---

## 고급 개선 문서 패키지 빠른 참조

이 README는 8개의 상호 연관된 고급 개선 문서 중 하나입니다. 역할에 따라 바로 이동하세요:

| 역할 | 읽어야 할 문서 | 목적 |
|------|--------------|------|
| **소프트웨어 개발자** | 이 README → SRS → SAD | 4단계 파이프라인 구조, API, 알고리즘 이해 |
| **영상 처리 엔지니어** | xpe-enhance-advanced-prd.md | 노이즈 감소, 엣지 강조, 조명 ROI, EI 보정 알고리즘 |
| **QA / 테스트 엔지니어** | TDS-ENHANCE-ADV-001 → RTM | 테스트 데이터 구성, 합격 기준, 알고리즘별 검증 |
| **안전/위험 담당자** | SHA-ENHANCE-ADV-001 → RTM | 위험 식별 (overshoot, 노이즈 손실, ROI 오류), ISO 14971 |
| **의료기기 규제 담당자** | SRS → RTM → SHA → SAD | IEC 62304 추적성 패키지 |
| **캘리브레이션 엔지니어** | IAP-ENHANCE-ADV-001 | 영상 취득 절차 (CDRAD, Leeds TOR, 매개변수 최적화) |

### 문서 생태계 구조

```
┌────────────────────────────────────────────────────────────────────┐
│          고급 개선 모듈 문서 패키지 (v1.0)                        │
│                                                                    │
│  ┌───────────────────────────────────────────────────────────┐   │
│  │  xpe-enhance-advanced-prd.md  (PRD)                       │   │
│  │  4단계 알고리즘 요구사항 원본                             │   │
│  │  · 노이즈 감소 (4 tier) · 엣지 강조 · 조명 ROI           │   │
│  │  · EI ROI 보정 · MTF/NPS 평가 기준                        │   │
│  └─────────────┬──────────────────────────────────────────┘   │
│                │ 파생                                            │
│        ┌───────┼──────────────────────┐                        │
│        │       │                      │                        │
│        v       v                      v                        │
│ ┌────────┐ ┌────────┐ ┌──────────────────────────────┐         │
│ │SRS-    │ │SAD-    │ │SHA-ENHANCE-ADV-001          │         │
│ │ENHANCE-│ │ENHANCE-│ │소프트웨어 위험 분석          │         │
│ │ADV-001 │ │ADV-001 │ │(8개 위험, ISO 14971)         │         │
│ │소프트웨│ │소프트웨│ │                              │         │
│ │어 요건 │ │어 아키│ │                              │         │
│ │명세서  │ │텍처   │ │                              │         │
│ └────┬──┘ └────┬──┘ └──────────┬───────────────────┘         │
│      │         │               │                              │
│      └─────────┼───────────────┘                              │
│                │ 추적                                         │
│                v                                             │
│       ┌──────────────────────┐                               │
│       │ RTM-ENHANCE-ADV-001  │                               │
│       │ 요구사항 추적        │                               │
│       │ 행렬 (SRS↔Test)      │                               │
│       └────────┬─────────────┘                               │
│                │ 테스트 입력                                 │
│    ┌───────────┼────────────────────┐                        │
│    v           v                    v                        │
│ ┌─────┐   ┌──────────┐              │                        │
│ │IAP- │   │TDS-      │              │                        │
│ │ENH- │   │ENHANCE-  │              │                        │
│ │ADV- │   │ADV-001   │──────────────┘                        │
│ │001  │   │테스트    │                                       │
│ │영상 │   │데이터    │                                       │
│ │취득 │   │셋 명세서 │                                       │
│ │프로 │   │(개발자/) │                                       │
│ │토콜 │   │QA용)     │                                       │
│ └─────┘   └──────────┘                                       │
│                                                               │
│ ▶ 이 파일 (README.md) = 고급 개선 파이프라인 기술 개요        │
└────────────────────────────────────────────────────────────────────┘
```

---

## 목차

1. [개요](#1-개요)
2. [아키텍처](#2-아키텍처)
3. [파이프라인 단계](#3-파이프라인-단계)
4. [노이즈 감소 계층](#4-노이즈-감소-4-계층)
5. [엣지 강조 (언샤프 마스킹)](#5-엣지-강조-언샤프-마스킹)
6. [조명 ROI 검출](#6-조명-roi-검출)
7. [EI ROI 보정 (SWU-2.10)](#7-ei-roi-보정-swu-210)
8. [성능 예산](#8-성능-예산)
9. [안전 제약 조건](#9-안전-제약-조건)
10. [API 레퍼런스](#10-api-레퍼런스)
11. [MTF-NPS 트레이드오프](#11-mtf-nps-트레이드오프)
12. [참고문헌](#12-참고문헌)

---

## 1. 개요

`xpe_enhance_advanced.dll`은 로그 변환된 X-ray FPD 이미지에 대한 고급 처리 엔진입니다. 기본 개선 (`xpe_enhance_basic.dll`에서 CLAHE 및 log 변환 이후)에서 4단계로 진행됩니다:

- **Stage 9**: 노이즈 감소 (4 계층: Gaussian, Bilateral, NLM, Wavelet)
- **Stage 10**: 엣지 강조 (언샤프 마스킹 + overshoot 제한)
- **Stage 11**: 조명 ROI 검출 (Hough 변환)
- **SWU-2.10**: EI ROI 보정 (검출기 도메인 데이터 활용)

### 주요 특성

- **노이즈-해상도 트레이드오프**: MTF 손실 < 5% 유지하면서 노이즈 감소 최대화
- **4계층 노이즈 감소**: 속도 vs 품질의 자동 선택 또는 구성 기반 선택
- **신호 의존 노이즈 모델**: σ²_total(x,y) = σ²_q(I) + σ²_e (양자 + 전자 노이즈)
- **조명 ROI 자동 검출**: Hough 변환 + 신뢰도 점수 (0.0-1.0)
- **EI ROI 보정**: 검출기 도메인 데이터 참조로 측정 선량 정확성 향상
- **overshoot 제한**: 임상적 안전을 위해 ±3σ로 강조 효과 제한

### 지원되는 처리 모드

| 모드 | 사용 경우 | 노이즈 계층 | ROI 기능 | 성능 |
|------|---------|-----------|---------|------|
| 고속 (Fast) | 실시간 형광투시 | Tier 1 (Gaussian) | 검출만 | ~50ms |
| 표준 (Standard) | 일반 임상 | Tier 2 (Bilateral) | 검출 + 보정 | ~120ms |
| 고품질 (Premium) | 상세 분석 | Tier 3 (NLM) | 검출 + 보정 | ~200ms |
| 초고품질 (Ultra) | 학술 연구 | Tier 4 (Wavelet) | 검출 + 보정 | ~300ms |

---

## 2. 아키텍처

### 레이어 위치

```
Layer 2  ImageProcTest.exe (C# WPF)           파이프라인 오케스트레이터
           |
           | P/Invoke (C ABI)
           v
Layer 1  xpe_enhance_advanced.dll  <-- 이 모듈
           |
           | 링크 의존성
           v
Layer 0  xpe_common.dll                        타입, 메모리, 구성, 에러, 알림
```

### 소프트웨어 단위 (SWU)

| SWU ID | 이름 | 단계 | 설명 |
|--------|------|:-----:|-------------|
| SWU-2.5 | GaussianDenoiser | 9 | 2D Gaussian 블러 (빠른 경로) |
| SWU-2.6 | BilateralFilter | 9 | 엣지 보존 양측 필터 (Tier 2) |
| SWU-2.7 | NLMDenoiser | 9 | Non-Local Means (Tier 3, 계산 집약적) |
| SWU-2.8 | WaveletShrinker | 9 | BayesShrink 웨이블릿 (Tier 4) |
| SWU-2.9 | EdgeEnhancer | 10 | 언샤프 마스킹 + overshoot 제한 |
| SWU-2.10 | EI_ROI_Refiner | SWU | ROI 기반 선량 지수 보정 |
| SWU-2.11 | CollimationDetector | 11 | Hough 변환 ROI 검출 |
| SWU-2.12 | SidecarManager | -- | JSON sidecar I/O |

---

## 3. 파이프라인 단계

### 3.1 전체 처리 시퀀스

```
로그 변환 이미지 (float32, enhancement domain)
  |
  v
+================================================================+
|  (9) 노이즈 감소                                   [조건부]   |
|  Tier 1: Gaussian σ=0.5-3.0                                  |
|  Tier 2: Bilateral f(Δx,ΔI) exponential                      |
|  Tier 3: NLM w(i,j) = exp(-‖P_i-P_j‖²/h²)                   |
|  Tier 4: Wavelet BayesShrink (subband 임계값)                |
|  MTF 제약: 손실 < 5%                                         |
|  신호 의존 모델: σ²(x,y) = σ²_q(I) + σ²_e                   |
+================================================================+
  |
  v
+================================================================+
|  (10) 엣지 강조                                    [조건부]   |
|  언샤프: I_enh = I + α × (I - I_blur)                        |
|  α: 0.1-2.0 (구성)                                          |
|  I_blur: Gaussian σ=1.0-3.0                                  |
|  Overshoot 제한: ±3σ 클립 (임상 안전)                       |
|  고주파 부스트: 저용량 배경에서 제한                         |
+================================================================+
  |
  v
+================================================================+
|  (11) 조명 ROI 검출                              [조건부]   |
|  Step 1: Sobel/Canny 그래디언트                              |
|  Step 2: Hough 직선 검출 (축 근처 ±5°)                     |
|  Step 3: 4-코너 직사각형 피팅                                |
|  Step 4: 신뢰도 점수 (0.0-1.0)                              |
|  출력: JSON sidecar { "roi": {...}, "confidence": 0.7-1.0 }  |
+================================================================+
  |
  v
+================================================================+
|  (SWU-2.10) EI ROI 보정 (선택사항)                [조건부]   |
|  조건: ROI confidence > 0.7                                   |
|  Step 1: 검출기 도메인 데이터 마스킹 (sidecar 참조)          |
|  Step 2: ROI 픽셀에서만 EI 계산                             |
|  Step 3: DI = 10 × log10(EI_roi / EI_T) 재계산             |
|  Step 4: |DI| > 3 시 QC 알림                                |
|  폴백: ROI 부재/낮은 신뢰도 → 전체 이미지 EI 사용 (SWU-2.0)  |
+================================================================+
  |
  v
고급 개선된 float32 이미지 + ROI sidecar
  |
  | [상태 플래그: 적용된 항목]
  | [진단 로그: 시간, ROI 신뢰도, EI 선택]
  |
  v
[xpe_display.dll] (downstream: 디스플레이 파이프라인)
```

### 3.2 단계 요약 테이블

| # | 단계 | SWU | 유형 | 입력 | 출력 | 시간 (ms) |
|---|-------|-----|:----:|:-----:|:------:|:--------:|
| 9 | 노이즈 감소 | 2.5-2.8 | 조건부 | float32 | float32 | 50-200 |
| 10 | 엣지 강조 | 2.9 | 조건부 | float32 | float32 | 30-50 |
| 11 | ROI 검출 | 2.11 | 조건부 | float32 | sidecar | 80-150 |
| SWU | EI 보정 | 2.10 | 조건부 | sidecar | DI 업데이트 | <5 |

---

## 4. 노이즈 감소 (4 계층)

### 4.1 Tier 1: Gaussian (빠른 경로)

**공식**: `I_denoise(x,y) = G(σ) * I(x,y)`

- σ: 0.5-3.0 픽셀 (구성)
- 계산: 빠름 (~10-15ms)
- 품질: 기본
- 사용 사례: 실시간 형광투시

### 4.2 Tier 2: Bilateral Filter (엣지 보존)

**공식**: 
```
I_bf(x,y) = Σ(w(x,y,u,v) × I(u,v)) / Σ(w(x,y,u,v))
w(x,y,u,v) = exp(-(x-u)²/2σs² - |I(x,y)-I(u,v)|²/2σr²)
```

- σs (공간): 1-5 픽셀
- σr (범위, 강도): 적응형 (SNR 기반)
- 계산: 중간 (~40-60ms)
- 품질: 우수 (엣지 보존)
- 사용 사례: 표준 임상

### 4.3 Tier 3: Non-Local Means (NLM, 계산 집약적)

**공식**:
```
I_nlm(x,y) = Σ(w(i,j) × I_j(x,y)) / Σ(w(i,j))
w(i,j) = exp(-‖P_i - P_j‖²_2 / h²)
```

- 패치 크기: 7×7
- 탐색 윈도우: 21×21
- h (필터 강도): 문서화된 자동 선택
- 계산: 느림 (~100-150ms)
- 품질: 매우 우수
- 사용 사례: 고품질 분석

### 4.4 Tier 4: Wavelet Shrinkage (BayesShrink)

**공식**:
```
threshold(subband_k) = σ²_n(k) / σ_signal(k)
coeff_shrunk = hard_threshold(coeff, threshold)
```

- 웨이블릿: Daubechies (db4)
- 분해 수준: 3-4
- 계산: 매우 느림 (~200-300ms)
- 품질: 최우수 (신호 및 노이즈 모델링)
- 사용 사례: 학술 연구, 상세 분석

### 4.5 신호 의존 노이즈 모델

**양자 노이즈**: σ²_q(I) ∝ I (포아송 통계)  
**전자 노이즈**: σ²_e (상수, 판독 연자)

**총 노이즈**: σ²_total(x,y) = σ²_q(I(x,y)) + σ²_e

각 계층은 이 모델을 고려하여 비선형적으로 강도 조정합니다.

---

## 5. 엣지 강조 (언샤프 마스킹)

### 5.1 기본 공식

```
I_enh(x,y) = I(x,y) + α × (I(x,y) - I_blur(x,y))
```

- I: 원본 이미지
- I_blur: Gaussian blur σ=1.0-3.0
- α: 강화 계수 0.1-2.0 (구성)
- 차이: 고주파 성분 추출

### 5.2 Overshoot 제한 (임상 안전)

**규칙**: 강화 효과를 로컬 신호의 ±3σ 내로 클립

```
enhancement(x,y) = α × (I(x,y) - I_blur(x,y))
limited(x,y) = clamp(enhancement(x,y), -3σ_local, +3σ_local)
I_final(x,y) = I(x,y) + limited(x,y)
```

- σ_local: 로컬 영역 (예: 3×3)의 표준편차
- 목적: halo 아티팩트 및 의사 가장자리 방지

### 5.3 다중 대역 옵션 (고급)

저/중/고 주파수 대역마다 다른 α:

- α_low: 해부학적 구조 (0.1-0.3)
- α_mid: 세부사항 (0.5-1.0)
- α_high: 미세 구조 (0.3-0.8)

---

## 6. 조명 ROI 검출

### 6.1 알고리즘

1. **그래디언트 계산**: Sobel 또는 Canny
2. **Hough 변환**: 직선 검출 (θ, ρ)
3. **축 정렬 제약**: θ ≈ 0° 또는 90° (±5° 허용)
4. **4-코너 교점**: 4개 직선 교점 찾기
5. **신뢰도 점수**: 0.0-1.0 (검출 강도 기반)

### 6.2 신뢰도 임계값

```
if confidence > 0.7:
    ROI 사용 (EI 보정 활성화)
else:
    ROI 폴백 (전체 이미지 사용)
```

### 6.3 Sidecar 형식

**파일**: `{image_path}.roi.json`

```json
{
  "roi": {
    "x": 100,
    "y": 150,
    "width": 2500,
    "height": 2700
  },
  "confidence": 0.85,
  "timestamp_ms": 1713052800000,
  "detector_id": "XRD-4343N",
  "collimation_type": "rectangular"
}
```

---

## 7. EI ROI 보정 (SWU-2.10)

### 7.1 알고리즘

**EI (노출 지수)**: 검출기에 도달한 X선 양의 정규화된 척도

**문제**: 전체 이미지 EI는 직접 빔 영역 (차폐)을 포함하여 정확하지 않음

**해결책**: ROI 마스크 적용 후 ROI 픽셀에서만 EI 계산

### 7.2 구현 단계

1. **검출기 도메인 데이터 참조**: sidecar의 ROI 좌표 읽기
2. **마스킹**: 검출기 도메인 (캘리브레이션 후, 로그 변환 전) 데이터에 ROI 마스크 적용
3. **EI 계산**: ROI 픽셀 평균 계산
   ```
   EI_roi = mean(detector_domain_pixels[ROI])
   ```
4. **DI (용량 지수) 재계산**:
   ```
   DI_roi = 10 × log10(EI_roi / EI_T)
   ```
   여기서 EI_T는 목표 노출 지수

### 7.3 QC 알림

```
if |DI_roi| > 3:
    경고: "Exposure out of range (|DI| > 3)"
    권장 조치: 획득 재실행
```

### 7.4 폴백 로직

```
if ROI absent or confidence <= 0.7:
    사용: SWU-2.0 (전체 이미지 EI)
    로그: "Fallback to full-image EI due to low confidence ROI"
```

---

## 8. 성능 예산

### 8.1 단계별 시간 할당

| 단계 | 예산 (ms) | Tier 1 | Tier 2 | Tier 3 | Tier 4 |
|------|:-----------:|:------:|:------:|:------:|:------:|
| (9) 노이즈 감소 | 200 | 15 | 50 | 120 | 200 |
| (10) 엣지 강조 | 50 | 30 | 30 | 35 | 40 |
| (11) ROI 검출 | 150 | -- | -- | -- | -- |
| (SWU) EI 보정 | 5 | -- | -- | -- | -- |
| **총합** | **300** | **~45** | **~110** | **~190** | **~270** |

### 8.2 메모리 예산

| 구성요소 | 크기 | 설명 |
|-----------|:----:|-------|
| 작업 버퍼 (float32) | 37.7 MB | 출력 이미지 |
| NLM 패치 버퍼 (Tier 3) | 10-15 MB | 7×7 패치 탐색 |
| 웨이블릿 계수 (Tier 4) | 5-10 MB | 3-4 분해 수준 |
| **최고 총합** | **~60 MB** | NLM 또는 Wavelet 포함 |

---

## 9. 안전 제약 조건

### 9.1 Overshoot 제한 의무

| ID | 규칙 | 근거 |
|----|------|-----------|
| **SAFE-ADV-001** | Overshoot 제한은 구성으로 비활성화 불가 | 의사 가장자리 (halo) 아티팩트 방지 |
| **SAFE-ADV-002** | ROI 폴백 로직은 의무 | ROI 신뢰도 낮음 → 전체 이미지로 자동 전환 |
| **SAFE-ADV-003** | MTF 손실 < 5% 검증 필수 | 해상도 보존 확인 |
| **SAFE-ADV-004** | EI ROI 보정은 검출기 도메인 데이터 사용 | 로그 변환 이후 데이터로는 정확한 선량 계산 불가 |
| **SAFE-ADV-005** | 모든 노이즈 계층 선택은 로깅 | IEC 62304 추적성 |

---

## 10. API 레퍼런스

### 10.1 노이즈 감소

```c
// Tier 1: Gaussian
int xpe_denoise_gaussian(
    const float* input,
    float* output,
    int width, int height,
    float sigma  // 0.5-3.0
);

// Tier 2: Bilateral
int xpe_denoise_bilateral(
    const float* input,
    float* output,
    int width, int height,
    float sigma_spatial,  // 1-5 pixels
    float sigma_range     // intensity-adaptive
);

// Tier 3: NLM
int xpe_denoise_nlm(
    const float* input,
    float* output,
    int width, int height,
    float h  // filter strength
);

// Tier 4: Wavelet
int xpe_denoise_wavelet(
    const float* input,
    float* output,
    int width, int height,
    int wavelet_level  // 3-4
);
```

### 10.2 엣지 강조

```c
int xpe_enhance_edges(
    const float* input,
    float* output,
    int width, int height,
    float alpha,        // 0.1-2.0
    float gaussian_sigma,  // 1.0-3.0
    bool apply_overshoot_limit  // always true
);
```

### 10.3 ROI 검출 및 EI 보정

```c
// ROI 검출
typedef struct {
    int x, y, width, height;
    float confidence;
} XpeROI;

int xpe_detect_roi(
    const float* image,
    int width, int height,
    XpeROI* roi_out
);

// EI ROI 보정
int xpe_refine_ei_by_roi(
    const uint16_t* detector_domain_data,  // 검출기 도메인
    const float* sidecar_roi,  // JSON sidecar
    float* di_out  // DI 값
);

// Sidecar 관리
int xpe_sidecar_write(
    const char* image_path,
    const XpeROI* roi
);

int xpe_sidecar_read(
    const char* image_path,
    XpeROI* roi_out
);
```

---

## 11. MTF-NPS 트레이드오프

### 그래프

```
   노이즈 감소 강도 (%)
   ↑
   |     Gaussian        Bilateral    NLM        Wavelet
  100 │                    ●                       ★
      │                                          /
      │        ●                              /
   50 │         \                          /
      │          \                      /
      │           \                  /
    0 │____________\________________/________________________→ MTF 손실 (%)
      0              2              5              10
      
   성능 선택 지표:
   ─────────────────────────────────────────────────────────
   ★ = 최적점 (MTF 손실 < 5%, 노이즈 감소 > 60%)
   ● = 현실적 선택 (속도 vs 품질)
```

---

## 12. 참고문헌

### 표준

| 표준 | 관련성 |
|----------|-----------|
| IEC 62220-1-1:2015 | DQE 측정 (MTF 평가) |
| IEC 62494-1 | 노출 지수 (EI 정의) |
| IEC 62304:2006+A1:2015 | 소프트웨어 생명주기 |
| ISO 14971:2019 | 위험 관리 |
| AAPM TG-233 | 저용량 이미징 (노이즈 특성화) |

### 연구 논문

| 인용 | 주제 |
|----------|-------|
| [Tomasi & Manduchi 1998](https://arxiv.org/pdf/1302.3755) | Bilateral Filter (엣지 보존 노이즈 감소) |
| [Buades et al. 2005](https://epubs.siam.org/doi/abs/10.1137/040616024) | Non-Local Means (NLM) 알고리즘 |
| [Donoho & Johnstone 1994](https://www.jstor.org/stable/2290318) | WaveShrink & BayesShrink |
| [Starman et al. 2012](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) | Lag correction (NLCSC) |
| [Duda & Hart 1972](https://dl.acm.org/doi/10.1145/361237.361242) | Hough 변환 |
| [Wang 2013](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) | Heel 효과 모델링 |

### 프로젝트 문서

#### IEC 62304 규제 문서 패키지

| 문서 ID | 제목 | 경로 | 대상 |
|--------|------|------|------|
| **PRD** | 고급 개선 PRD | `docs/enhance-advanced/xpe-enhance-advanced-prd.md` | 개발자 |
| **SRS-ENHANCE-ADV-001** | 소프트웨어 요건 명세서 | `docs/enhance-advanced/SRS-ENHANCE-ADV-001_Software_Requirements_Specification.md` | 개발자 |
| **SAD-ENHANCE-ADV-001** | 소프트웨어 아키텍처 문서 | `docs/enhance-advanced/SAD-ENHANCE-ADV-001_Software_Architecture_Document.md` | 개발자 |
| **SHA-ENHANCE-ADV-001** | 소프트웨어 위험 분석 | `docs/enhance-advanced/SHA-ENHANCE-ADV-001_Software_Hazard_Analysis.md` | 안전 담당자 |
| **RTM-ENHANCE-ADV-001** | 요건 추적 행렬 | `docs/enhance-advanced/RTM-ENHANCE-ADV-001_Requirements_Traceability_Matrix.md` | QA / 개발자 |
| **IAP-ENHANCE-ADV-001** | 영상 취득 프로토콜 | `docs/enhance-advanced/IAP-ENHANCE-ADV-001_Image_Acquisition_Protocol.md` | 캘리브레이션 엔지니어 |
| **TDS-ENHANCE-ADV-001** | 테스트 데이터셋 명세서 | `docs/enhance-advanced/TDS-ENHANCE-ADV-001_Test_Dataset_Specification.md` | QA / 개발자 |

---

*고급 개선 모듈 README v2.0 끝*

---

### v2.0 변경 내역 (2026-04-15)

**SRS-ENHANCE-ADV-001 주요 개선 사항** (5차 교차검증 결과):

- **Wiener 필터 추가** (FR-050): 주파수 도메인 최적 선형 필터, Research 모드 전용, NSR 자동 계산
- **PSNR/SSIM 품질 게이트** (FR-1900): 3 dB ≤ ΔPSNR ≤ 15 dB, SSIM ≥ 0.95 기준, Fast 모드 제외
- **OpenMP 병렬화 요구사항** (FR-2000): NLM (행 레벨), Wavelet (서브밴드), Bilateral (블록 레벨), 최대 8 스레드
- **컴파일 요구사항**: OpenMP 필수, 미사용 시 단일 스레드 폴백 모드 컴파일 경고
