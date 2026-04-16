# 고급 이미지 개선 모듈 - 제품 요구사항 정의서 (PRD)

**문서 ID**: xpe-enhance-advanced-prd.md  
**IEC 62304 역할**: 규범 알고리즘 요구사항 원본  
**안전 분류**: Class B  
**날짜**: 2026-04-14  
**버전**: 1.0.0  

---

## 1. 범위

이 문서는 `xpe_enhance_advanced.dll` (Layer 1, Phase 2) 모듈의 4가지 핵심 처리 단계에 대한 알고리즘 요구사항을 정의합니다:

1. **Stage 9**: 노이즈 감소 (4 계층)
2. **Stage 10**: 엣지 강조 (언샤프 마스킹)
3. **Stage 11**: 조명 ROI 검출 (Hough 변환)
4. **SWU-2.10**: EI ROI 기반 선량 보정

모든 처리는 log 변환된 enhancement 도메인에서 이루어집니다 (float32).

---

## 2. Stage 9: 노이즈 감소 (4 계층)

### 2.1 노이즈 모델

X-ray FPD 이미지의 노이즈는 두 가지 독립적인 성분으로 구성됩니다:

#### 2.1.1 양자 노이즈 (Quantum Noise)

포아송 통계에 따르는 X선 양자 통계적 변동:

```
σ²_quantum(x,y) = I(x,y)  [포아송 분포, 평균 = 분산]
```

- **특성**: 신호에 비례 (강도가 높을수록 절대 노이즈 증가)
- **공식**: SNR_quantum = √I

#### 2.1.2 전자 노이즈 (Electronic Noise)

검출기 판독 회로의 열적 및 1/f 노이즈:

```
σ²_electronic = constant (신호 무관)
```

- **특성**: 신호 무관 (배경에서 지배적)
- **전형값**: 20-50 DN (14-bit ADC에서)

#### 2.1.3 총 노이즈

```
σ²_total(x,y) = σ²_quantum(I(x,y)) + σ²_electronic
              = I(x,y) + σ²_e
```

이 모델은 모든 4개 노이즈 감소 계층의 매개변수 선택에 영향을 미칩니다.

### 2.2 Tier 1: Gaussian 블러 (빠른 경로)

#### 공식

```
I_denoise(x,y) = ∫∫ G(u,v;σ) × I(x-u,y-v) du dv
```

여기서 `G(u,v;σ) = (1/2πσ²) × exp(-(u²+v²)/2σ²)`

#### 매개변수

- **σ (공간 표준편차)**: 0.5~3.0 픽셀
  - σ=0.5: 가벼운 노이즈 감소 (거의 원본 유지)
  - σ=1.5: 표준 (노이즈-해상도 균형)
  - σ=3.0: 강한 노이즈 감소 (세부 손실)

#### 특성

| 특성 | 값 |
|------|-----|
| 계산 시간 | ~10-15ms (3072×3072) |
| MTF 손실 | ~8-15% (σ=1.5일 때) |
| 노이즈 감소 | ~20-30% |
| 엣지 보존 | 약함 (blur 때문) |
| 사용 사례 | 실시간 형광투시 (Fluoro) |

#### 구현

```c
// Separable Gaussian (성능 최적)
// 1D horizontal pass + 1D vertical pass
// 예상 시간: 2×(width×height) operations
```

### 2.3 Tier 2: Bilateral Filter (엣지 보존)

#### 공식

```
I_bf(x,y) = Σ_{u,v∈N(x,y)} w(x,y,u,v) × I(u,v)
            / Σ_{u,v∈N(x,y)} w(x,y,u,v)

w(x,y,u,v) = exp(-(d_spatial)²/2σ_s²) × exp(-(d_intensity)²/2σ_r²)

d_spatial = √((x-u)² + (y-v)²)
d_intensity = |I(x,y) - I(u,v)|
```

#### 매개변수

| 매개변수 | 범위 | 설명 |
|---------|------|------|
| **σ_s (공간)** | 1.0~5.0 픽셀 | 공간 가중치 표준편차 |
| **σ_r (범위)** | 신호 의존 | 강도 차이 민감성 (자동 계산) |
| **윈도우 크기** | 5×5 ~ 11×11 | 계산 영역 |

#### σ_r 자동 선택 (신호 의존)

```
σ_r(x,y) = α × σ_local(x,y)

α = 0.5~1.5 (구성)
σ_local = 지역 (예: 7×7) 표준편차
```

**이유**: 고-SNR 영역(밝은 부위)에서는 작은 σ_r (노이즈 억제), 저-SNR 영역(어두운 부위)에서는 큰 σ_r (구조 보존)

#### 특성

| 특성 | 값 |
|------|-----|
| 계산 시간 | ~40-60ms (3072×3072, 윈도우 7×7) |
| MTF 손실 | ~3-5% |
| 노이즈 감소 | ~40-50% |
| 엣지 보존 | 매우 우수 |
| 사용 사례 | 표준 임상 |

### 2.4 Tier 3: Non-Local Means (NLM, 계산 집약적)

#### 공식

```
I_nlm(x,y) = Σ_j w_j(x,y) × I_j(x,y) / Σ_j w_j(x,y)

w_j(x,y) = exp(-‖P_i(x,y) - P_j(x,y)‖²_2 / h²)

P_i(x,y) = 위치 (x,y)를 중심으로 하는 7×7 패치
```

#### 매개변수

| 매개변수 | 값 |
|---------|-----|
| 패치 크기 | 7×7 픽셀 |
| 탐색 윈도우 | 21×21 픽셀 |
| h (필터 강도) | κ × σ (자동 선택) |
| κ | 0.8~1.5 (구성) |

#### h 자동 선택

```
h = κ × σ_noise

σ_noise는 전체 이미지 또는 지역 노이즈 추정 (Laplacian 기반)
```

#### 특성

| 특성 | 값 |
|------|-----|
| 계산 시간 | ~100-150ms (3072×3072) |
| MTF 손실 | ~2-4% |
| 노이즈 감소 | ~60-70% |
| 엣지 보존 | 우수 |
| 사용 사례 | 고품질 분석 |
| 계산 효율성 | 병렬화 가능 (패치별 독립) |

#### 구현 최적화

- **공간 탐색 제약**: 패치 유사성 임계값으로 탐색 범위 축소
- **병렬화**: OpenMP / SIMD로 패치 계산 병렬화
- **부분 처리**: 큰 이미지는 타일로 분할하여 캐시 효율성 향상

### 2.5 Tier 4: Wavelet Shrinkage (BayesShrink)

#### 공식

**1단계: 웨이블릿 분해**

```
{cA, cD_h, cD_v, cD_d} = DWT(I, wavelet)

cA: 근사 계수
cD_h, cD_v, cD_d: 상세 계수 (수평, 수직, 대각)
```

**2단계: Subband별 임계값 계산**

```
for each subband k in {cD_h, cD_v, cD_d}:
    σ_n(k) = 노이즈 표준편차 (Laplacian 기반)
    σ_s(k) = 신호 표준편차 = √(max(0, σ_y(k)² - σ_n(k)²))
    threshold_k = σ_n(k)² / σ_s(k)
```

**3단계: 하드 임계값 처리**

```
c_shrunk = {
    0,        if |c| ≤ threshold
    c,        if |c| > threshold
}
```

**4단계: 역 웨이블릿 변환**

```
I_denoised = IDWT({cA, c_shrunk_D_h, c_shrunk_D_v, c_shrunk_D_d})
```

#### 매개변수

| 매개변수 | 값 | 설명 |
|---------|-----|------|
| 웨이블릿 | Daubechies (db4) | 생의학 이미징에 적합 |
| 분해 수준 | 3~4 | 다중 스케일 처리 |
| 노이즈 추정 | Laplacian 기반 | 로버스트 추정 |

#### 특성

| 특성 | 값 |
|------|-----|
| 계산 시간 | ~200-300ms (3072×3072, 4 분해) |
| MTF 손실 | ~1-3% |
| 노이즈 감소 | ~75-85% |
| 엣지 보존 | 우수 |
| 사용 사례 | 학술 연구, 상세 분석 |
| 신호 충실도 | 최고 (신호+노이즈 모델) |

### 2.6 노이즈 감소 계층 선택 로직

```
IF 모드 == "Fast" (형광투시):
    USE Tier 1 (Gaussian, σ=1.0)
ELSE IF 모드 == "Standard" (임상):
    USE Tier 2 (Bilateral, σ_s=2.5)
ELSE IF 모드 == "Premium":
    USE Tier 3 (NLM, h=κ×σ_noise)
ELSE IF 모드 == "Ultra":
    USE Tier 4 (Wavelet, db4, 레벨=4)
ELSE IF 구성된 계층:
    USE 구성된 계층 (매개변수 적용)
```

### 2.7 MTF 제약 (필수)

**요구사항**: 노이즈 감소로 인한 MTF 손실 < 5%

**검증 방법**:
- Wire phantom 취득 (1mm W wire)
- 1D LSF (Line Spread Function) 측정
- MTF = FFT(LSF)
- Nyquist 주파수에서 < 5% 손실 확인

**실패 시 조치**:
- σ 또는 필터 강도 감소
- 더 약한 계층 선택

---

## 3. Stage 10: 엣지 강조 (언샤프 마스킹)

### 3.1 기본 공식

```
I_sharp(x,y) = I(x,y) + α × (I(x,y) - I_blur(x,y))
              = (1+α) × I(x,y) - α × I_blur(x,y)

α ∈ [0.1, 2.0]: 강화 계수
I_blur: Gaussian 블러, σ_blur ∈ [1.0, 3.0]
```

### 3.2 고주파 추출 원리

```
High-Freq = I - I_blur
            └─> 가우시안으로 제거된 고주파만 남음
            └─> 시각적 대비 증강에 기여
```

### 3.3 Overshoot 제한 (임상 안전, 의무)

**문제**: 과도한 강화 → halo 아티팩트, 의사 가장자리

**해결책**: 강화 효과를 로컬 신호 변동성에 제한

#### 알고리즘

```
// Step 1: 엣지 강조 값 계산
edge_boost(x,y) = α × (I(x,y) - I_blur(x,y))

// Step 2: 로컬 표준편차 계산 (예: 3×3 윈도우)
σ_local = std(I[x-1:x+2, y-1:y+2])

// Step 3: 클립 (±3σ_local)
clipped_boost(x,y) = clamp(edge_boost(x,y), 
                            -3.0 × σ_local, 
                            +3.0 × σ_local)

// Step 4: 최종 이미지
I_final(x,y) = I(x,y) + clipped_boost(x,y)
```

**이유**: 3σ는 95% 신뢰도 구간, 비정상 강화 방지

### 3.4 다중 대역 옵션 (고급)

고급 모드에서는 주파수 대역별로 다른 α 적용:

#### 3단계 분해 (FFT 또는 Laplacian 피라미드)

```
I = I_low + I_mid + I_high

I_sharp = I_low + (1+α_low)×I_mid + (1+α_mid)×I_mid + (1+α_high)×I_high
```

#### 권장 값

| 대역 | α 범위 | 용도 |
|------|--------|------|
| **Low** (해부학) | 0.1~0.3 | 전반적 구조 강조 억제 |
| **Mid** (세부) | 0.5~1.0 | 임상 해석 대상 강조 |
| **High** (미세) | 0.3~0.8 | 노이즈 증폭 방지 |

### 3.5 저용량 배경 처리

저용량 이미징에서 배경 노이즈 증폭 방지:

```
IF I(x,y) < 백분위수_5(이미지):
    // 저신호 영역
    α_effective = α × (I(x,y) / 백분위수_25)  // 감소
ELSE:
    α_effective = α  // 정상
```

### 3.6 매개변수 가이드

| α | 결과 | 용도 |
|---|------|------|
| 0.1~0.3 | 미묘한 강화 | 정상 해상도 이미지 |
| 0.5~0.8 | 중간 강화 | 저용량 이미징 |
| 1.0~1.5 | 강한 강화 | 매우 저용량 또는 학술 강조 |
| > 1.5 | 과도한 강화 (권장 안 함) | halo 위험 |

---

## 4. Stage 11: 조명 ROI 검출

### 4.1 목표

X-ray 조명 영역 (collimated region)의 자동 감지 및 경계 (collimation border) 추출

**응용**: 
- 노출 지수 (EI) 계산 개선
- 자동 크롭
- 환자 안전 (비조명 영역 검증)

### 4.2 알고리즘 단계

#### Step 1: 그래디언트 계산 (Canny 또는 Sobel)

```
G_x = ∂I/∂x (Sobel X 커널)
G_y = ∂I/∂y (Sobel Y 커널)

|G| = √(G_x² + G_y²)
θ = atan2(G_y, G_x)
```

**이유**: 조명 경계는 높은 그래디언트를 가짐

#### Step 2: Hough 직선 변환

```
For each edge pixel (x, y) with |G| > threshold:
    For θ ∈ [0°, 180°):
        ρ = x·cos(θ) + y·sin(θ)
        Accumulator[θ, ρ] += 1

Find peaks in Accumulator
```

**파라미터**:
- θ 해상도: 1° (또는 0.5° 고정밀)
- ρ 해상도: 1 픽셀
- 누적기 임계값: 최대값의 70%

#### Step 3: 축 정렬 제약 (직사각형)

```
// 직사각형 조명은 수평 + 수직 직선만
Filtered_lines = {
    (θ, ρ) where θ ≈ 0° OR θ ≈ 90° (허용오차: ±5°)
}
```

**이유**: 직사각형 조명기는 축 정렬됨

#### Step 4: 교점 찾기

```
// 4개 직선의 교점에서 직사각형의 4 코너 추출
(x1, y1) = line_0_intersection_line_2  // 위쪽-왼쪽
(x2, y2) = line_0_intersection_line_3  // 위쪽-오른쪽
(x3, y3) = line_1_intersection_line_3  // 아래쪽-오른쪽
(x4, y4) = line_1_intersection_line_2  // 아래쪽-왼쪽

ROI = AABB(x1, y1, x3, y3)
```

#### Step 5: 신뢰도 점수 계산

```
confidence = (직선 누적기 피크의 합) / (최대 가능 값)
           ∈ [0.0, 1.0]

신뢰도 임계값:
- > 0.7: 높음 (ROI 사용)
- 0.5-0.7: 중간 (재확인 권장)
- < 0.5: 낮음 (폴백)
```

### 4.3 Hough 매개변수 조정

| 매개변수 | 민감도 | 효과 |
|---------|--------|------|
| 누적기 임계값 높이기 | 감소 | 잘못된 직선 감소, 약한 경계 놓침 |
| 그래디언트 임계값 높이기 | 감소 | 노이즈 감소, 약한 경계 놓침 |
| θ 해상도 증가 (세밀) | 증가 | 정확성 증대, 계산 증가 |

### 4.4 실패 케이스

| 케이스 | 원인 | 폴백 |
|--------|------|------|
| 조명이 약함 | 저용량 이미징 | 전체 이미지 사용 |
| 비정사각형 조명 | 요각 조명기 | 경계 탐지 불가 → confidence < 0.7 |
| 그물 조명기 | 복잡한 패턴 | 다중 직선 충돌 → confidence < 0.7 |

---

## 5. SWU-2.10: EI ROI 기반 선량 보정

### 5.1 배경: EI와 DI

**EI (노출 지수)**: 검출기에 도달한 X선 양의 정규화된 척도

```
EI = mean(detector_signal) / detector_sensitivity
```

**DI (용량 지수)**: EI의 로그 스케일 (IEC 62494-1)

```
DI = 10 × log10(EI / EI_ref)

DI = 0: 목표 노출과 일치
DI > 0: 과노출 (밝음)
DI < 0: 저노출 (어두움)
```

### 5.2 문제: 전체 이미지 EI의 부정확성

```
Raw X-ray Beam
    |
    | (직접 조사)
    v
┌─────────────────────────┐
│  조명 영역              │  ← 진단 정보
│  (collimated)           │
├─────────────────────────┤
│  차폐 영역              │  ← 제로 신호
│  (collimated away)      │  ← EI 계산에 포함될 수 없음
└─────────────────────────┘

전체 EI = (조명 픽셀 합) / (전체 픽셀 수) 
        = 낮게 측정됨 (차폐 영역 때문)
        = DI 오류 발생
```

### 5.3 해결책: ROI 마스킹된 EI

```
EI_roi = mean(detector_signal[ROI만])
       = 정확한 측정 (차폐 영역 제외)

DI_roi = 10 × log10(EI_roi / EI_ref)
```

### 5.4 구현 알고리즘

```
INPUT:
  - detector_domain_data (uint16 또는 float32, calibrated)
  - roi_sidecar.json { x, y, width, height, confidence }
  - EI_ref (목표 노출 지수)

OUTPUT:
  - DI_roi (보정된 용량 지수)
  - qc_alert (|DI_roi| > 3 여부)

PROCESS:
  1. ROI confidence > 0.7 확인
     IF false:
       사용 SWU-2.0 EI (전체 이미지)
       로그: "Fallback to full-image EI"
       RETURN
  
  2. ROI 경계 추출
     x_min, y_min = roi.x, roi.y
     x_max, y_max = roi.x + roi.width, roi.y + roi.height
  
  3. ROI 마스크 적용
     roi_pixels = detector_domain_data[y_min:y_max, x_min:x_max]
  
  4. EI 계산 (ROI)
     EI_roi = mean(roi_pixels)
  
  5. DI 계산
     DI_roi = 10 × log10(EI_roi / EI_ref)
  
  6. QC 검사
     IF |DI_roi| > 3:
       경고: "Exposure out of range (|DI| > 3)"
       qc_alert = TRUE
     ELSE:
       qc_alert = FALSE
  
  7. 로깅
     출력: DI_roi, confidence, roi_coords
```

### 5.5 임계값 해석

| DI_roi 범위 | 해석 | 조치 |
|------------|------|------|
| -1 ~ +1 | 정상 | 계속 진행 |
| +1 ~ +3 | 약간 과노출 | 경고 (권장사항) |
| -1 ~ -3 | 약간 저노출 | 경고 (권장사항) |
| > +3 | 심각한 과노출 | 경고 + 재획득 권장 |
| < -3 | 심각한 저노출 | 경고 + 재획득 권장 |

---

## 6. 통합 처리 흐름

```
┌──────────────────────────────────────────────────────────────┐
│ 입력: Log 변환된 float32 이미지 (enhancement domain)         │
│     + 검출기 도메인 원본 데이터 (uint16, 캘리브레이션됨)    │
└──────────────────────────────────────────────────────────────┘
                           │
                           v
                   ┌─────────────────┐
                   │ Stage 9:        │
                   │ 노이즈 감소     │
                   │ (Tier 선택)     │
                   └────────┬────────┘
                            │
                            v
                   ┌─────────────────┐
                   │ Stage 10:       │
                   │ 엣지 강조       │
                   │ + Overshoot     │
                   │   제한          │
                   └────────┬────────┘
                            │
                            v
                   ┌─────────────────┐
                   │ Stage 11:       │
                   │ ROI 검출        │
                   │ (Hough)         │
                   └────────┬────────┘
                            │
        ┌───────────────────┴───────────────────┐
        │ JSON sidecar 생성                     │
        │ { roi, confidence }                   │
        └───────────────────┬───────────────────┘
                            │
                            v
                   ┌─────────────────┐
                   │ SWU-2.10:       │
                   │ EI ROI 보정     │
                   │ (DI 재계산)     │
                   └────────┬────────┘
                            │
                            v
        ┌───────────────────────────────────────┐
        │ 출력:                                 │
        │ - 고급 개선된 float32 이미지          │
        │ - ROI sidecar (JSON)                  │
        │ - DI 값 + QC 알림                     │
        │ - 상태 플래그 + 진단 로그             │
        └───────────────────────────────────────┘
```

---

## 7. 성능 및 품질 목표

### 7.1 처리 시간 (3072×3072 프레임)

| 모드 | 예상 시간 | 예산 |
|------|:---------:|:----:|
| Fast (Tier 1) | ~45ms | <100ms |
| Standard (Tier 2) | ~110ms | <200ms |
| Premium (Tier 3) | ~190ms | <300ms |
| Ultra (Tier 4) | ~270ms | <400ms |

### 7.2 메모리 (최고)

| 항목 | 크기 |
|------|:----:|
| 작업 버퍼 | 37.7 MB |
| NLM 패치 (Tier 3) | 10-15 MB |
| 웨이블릿 계수 (Tier 4) | 5-10 MB |
| **최고 총합** | **~60 MB** |

### 7.3 이미지 품질 목표

| 메트릭 | 목표 | 검증 |
|--------|------|------|
| MTF 손실 | < 5% | Wire phantom |
| 노이즈 감소 | > 50% | CNRD phantom |
| Overshoot 아티팩트 | 없음 | 시각 검사 |
| ROI 검출 정확도 | ± 5 픽셀 | 수동 측정 |
| EI 정확도 | ± 5% | 선량계 검증 |

---

## 8. 참고문헌

### 학술 논문

| 저자 | 연도 | 주제 | DOI / 링크 |
|------|------|------|-----------|
| Tomasi & Manduchi | 1998 | Bilateral Filtering | [ICCV](https://arxiv.org/pdf/1302.3755) |
| Buades, Coll, Morel | 2005 | Non-Local Image Denoising | [SIAM SIAM](https://epubs.siam.org/doi/abs/10.1137/040616024) |
| Donoho & Johnstone | 1994 | WaveShrink | [JSTOR](https://www.jstor.org/stable/2290318) |
| Duda & Hart | 1972 | The Hough Transform | [ACM](https://dl.acm.org/doi/10.1145/361237.361242) |
| Starman et al. | 2012 | NLCSC Lag Correction | [PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) |
| Wang | 2013 | Heel Effect Modeling | [Union](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) |

### 표준

| 표준 | 내용 |
|------|------|
| IEC 62494-1 | 노출 지수 정의 및 측정 |
| IEC 62220-1-1:2015 | DQE 측정 (MTF 검증) |
| IEC 62304:2006+A1:2015 | 의료기기 소프트웨어 생명주기 |
| ISO 14971:2019 | 위험 관리 |
| AAPM TG-233 | 저선량 이미징 특성화 |

---

*고급 개선 PRD v1.0.0 끝*
