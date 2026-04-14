# Image Acquisition Protocol - Panel Defect Calibration

**Document ID:** IAP-DEFECT-001 v1.0  
**Module:** `xpe_preprocess.dll`, Stage 3, Layer 1  
**Process:** Static Bad Pixel Map (BPM) Generation  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Calibration Engineering Team  
**Language:** Korean (user-facing), English (specifications)  

---

## 목차

1. [개요](#개요)
2. [어두운 영상 취득 (Dark Frame Acquisition)](#어두운-영상-취득-dark-frame-acquisition)
3. [균일 조사 영상 취득 (Flat-Field Acquisition)](#균일-조사-영상-취득-flat-field-acquisition)
4. [깜빡이는 픽셀 분석 (Flickering Detection Acquisition)](#깜빡이는-픽셀-분석-flickering-detection-acquisition)
5. [그리드 캘리브레이션 (Grid Calibration Acquisition)](#그리드-캘리브레이션-grid-calibration-acquisition)
6. [수용 기준](#수용-기준)
7. [공장 vs 현장 비교](#공장-vs-현장-비교)

---

## 개요

### 목적

Static Bad Pixel Map (BPM) 생성을 위한 표준화된 영상 취득 절차를 정의합니다. BPM은 calibration-time에 한 번 생성되며, 모든 임상 영상 처리 시 사용됩니다.

### 범위

- AUO R1717 FPD (3072×3072 pixels)
- Hot Pixel, Cold Pixel, Flickering Pixel, Line Defect 검출
- 온도, 습도 통제 환경 (20°C~40°C)

---

## 어두운 영상 취득 (Dark Frame Acquisition)

### 2.1 목적

Dark current offset 측정 및 Hot Pixel 검출

### 2.2 조건

| 파라미터 | 설정값 |
|---------|--------|
| **X-ray Source** | OFF (차단) |
| **Detector Prep Time** | 30 seconds (standard PREP) |
| **Detector Cooler** | ON (active thermal management) |
| **Temperature Level 1** | 20°C ± 1°C |
| **Temperature Level 2** | 30°C ± 1°C |
| **Temperature Level 3** | 40°C ± 1°C |
| **Frames per Level** | 200 frames |
| **Frame Rate** | 1-2 fps (allow thermal stabilization) |
| **Acquisition Time per Level** | ~100-200 seconds |
| **Total Dark Frames** | 600 frames (3 levels × 200 frames) |
| **Total Acquisition Time** | ~10-15 minutes |

### 2.3 절차

1. **온도 안정화**: 설정된 온도(Level 1: 20°C)에서 detector 안정화 (≥ 30분)
2. **영상 취득**: 어두운 환경에서 200 프레임 연속 획득
   - Frame size: 3072 × 3072 pixels
   - Format: uint16 (14-bit depth)
   - No binning (native 1×1 resolution)
3. **온도 변경**: Level 2 (30°C)로 변경, 안정화, 200 프레임 취득
4. **반복**: Level 3 (40°C)에서 동일 절차
5. **데이터 저장**: 모든 600 프레임을 raw 형식으로 저장

### 2.4 Hot Pixel 검출 원칙

다중 온도 어두운 영상으로부터:

$$\mu_{\text{dark}}(i,j) = \frac{1}{600} \sum_{k=1}^{600} I_{\text{dark}, k}(i,j)$$

$$\sigma_{\text{dark}}(i,j) = \sqrt{\frac{1}{600} \sum_{k=1}^{600} (I_{\text{dark}, k}(i,j) - \mu_{\text{dark}}(i,j))^2}$$

Hot pixel:

$$\text{HotPixel}(i,j) = 1 \iff \frac{|I_{\text{dark}}(i,j) - \mu_{\text{dark}}|}{\sigma_{\text{dark}}} > 8.0$$

---

## 균일 조사 영상 취득 (Flat-Field Acquisition)

### 3.1 목적

Pixel-to-pixel gain variation 측정 및 Cold Pixel 검출

### 3.2 조건

| 파라미터 | 설정값 |
|---------|--------|
| **X-ray Source** | ON (RQA-5 spectrum) |
| **kVp** | 70 kVp |
| **Filtration** | 21 mm Aluminum (RQA-5 standard) |
| **Exposure** | 40-60% of saturation (optimal SNR) |
| **SID** | 1000 mm (standard clinic distance) |
| **Field** | Uniform (no collimation, full detector) |
| **Detector Temperature Level 1** | 20°C ± 1°C |
| **Detector Temperature Level 2** | 30°C ± 1°C |
| **Detector Temperature Level 3** | 40°C ± 1°C |
| **Frames per Level** | 200 frames |
| **Frame Rate** | 1-2 fps |
| **No Binning** | 1×1 native resolution |
| **Total Flat-Field Frames** | 600 frames |

### 3.3 절차

1. **온도 안정화**: 20°C로 설정
2. **X-ray 설정**: RQA-5 조건 (70 kVp, 21 mm Al) 준비, 출력 안정화 (≥ 30초)
3. **노출 조정**: 전체 영상이 40-60% 포화 도달하도록 mAs 조정
   - 모니터 영상 확인: 전체 detector 영역이 균등한 회색
   - Vignetting 없음 (Heel effect 최소화)
4. **영상 취득**: 200 프레임 연속 획득 (노출 조건 고정)
5. **온도 변경**: Level 2, Level 3에서 반복

### 3.4 Cold Pixel 검출 원칙

합산된 균일 조사 영상:

$$I_{\text{ff}, \text{corrected}}(i,j) = I_{\text{ff}}(i,j) - \mu_{\text{dark}}(i,j)$$

Gain map:

$$G(i,j) = \frac{I_{\text{ff}, \text{corrected}}(i,j)}{\overline{I}_{\text{corrected}}} \quad \text{(normalized to mean)}$$

Cold pixel:

$$\text{ColdPixel}(i,j) = 1 \iff G(i,j) < G_{\text{mean}} - 4\sigma_G$$

---

## 깜빡이는 픽셀 분석 (Flickering Detection Acquisition)

### 4.1 목적

Temporal 변동이 큰 불안정 픽셀 검출 (TFT 접촉 불량, 누설 전류 변동 등)

### 4.2 조건

| 파라미터 | 설정값 |
|---------|--------|
| **X-ray Source** | ON (normal clinical condition) |
| **kVp** | 60-70 kVp |
| **Exposure** | 50% saturation (constant) |
| **Detector Temperature** | Room temperature (~25°C) |
| **Acquisition Duration** | 200 frames continuous |
| **Frame Rate** | 1 fps |
| **Total Acquisition Time** | ~200 seconds |

### 4.3 절차

1. **안정화**: Detector와 X-ray 모두 안정 상태에서 30초 대기
2. **연속 취득**: 200 프레임을 1 fps로 연속 획득
   - 동일 노출 조건 유지
   - No acquisition pauses (연속성 중요)
3. **데이터 저장**: 200 프레임 시계열 저장

### 4.4 Flickering Pixel 검출 원칙

각 픽셀의 시간적 변동:

$$\text{CV}(i,j) = \frac{\sigma_{\text{temporal}}(i,j)}{\mu_{\text{temporal}}(i,j)} \times 100\%$$

Flickering pixel:

$$\text{FlickeringPixel}(i,j) = 1 \iff \text{CV}(i,j) > 5\%$$

---

## 그리드 캘리브레이션 (Grid Calibration Acquisition)

### 5.1 목적

DWT/DCT 필터 파라미터 및 그리드 주파수 추정

### 5.2 조건

| 파라미터 | 설정값 |
|---------|--------|
| **Anti-Scatter Grid** | Standard clinical grid (line pair/cm TBD) |
| **Grid 포함 영상** | Uniform exposure WITH grid |
| **Grid 제외 영상** | Uniform exposure WITHOUT grid |
| **Frames per Set** | 100 frames each |
| **Temperature** | Room temperature (~25°C) |
| **Total Frames** | 200 frames (100 with grid + 100 without) |

### 5.3 절차

1. **Grid 있음**: Anti-scatter grid를 detector 앞에 배치, 균일 조사, 100 프레임 취득
2. **Grid 제거**: Grid를 제거, 동일 조사 조건, 100 프레임 취득
3. **DWT 분석**: 두 세트의 차이에서 그리드 주파수 특성 분석

### 5.4 필터 파라미터 결정

DWT 분해 에너지 비교:

$$\text{Grid-related energy} = E_{\text{with grid}} - E_{\text{without grid}}$$

Grid 주파수: 부분대역(subband)에서 peak 에너지 위치로 추정

필터 중심 $(u_0, v_0)$ 및 bandwidth: 그리드 주파수로부터 설정

---

## 수용 기준

### 6.1 Hot Pixel Rate

$$\text{Hot pixel rate} = \frac{\text{Count of pixels with SNR} > 8.0}{\text{Total pixels (3072×3072)}} \times 100\%$$

**기준**: < 0.1% (약 94 픽셀 이하)

**부합 확인**: 통계 분석 및 공간 지도 검토

### 6.2 Cold Pixel Rate

$$\text{Cold pixel rate} = \frac{\text{Count of pixels with } G < G_{\text{mean}} - 4\sigma_G}{\text{Total pixels}} \times 100\%$$

**기준**: < 0.1% (약 94 픽셀 이하)

**부합 확인**: Gain map 히스토그램 검토

### 6.3 Flickering Pixel Rate

$$\text{Flickering rate} = \frac{\text{Count of pixels with CV} > 5\%}{\text{Total pixels}} \times 100\%$$

**기준**: < 0.01% (약 9 픽셀 이하)

**부합 확인**: Temporal CV 맵 검토

### 6.4 Line Defect

**기준**: < 5 개의 라인 결함/영상 (전체 600 dark frame 분석에서)

**정의**: 5 픽셀 이상 연속 결함 픽셀 또는 폭 1-3 픽셀, 길이 > 10 픽셀

**부합 확인**: Morphology labeling 및 시각적 검증

### 6.5 Grid Frequency Estimation Error

**기준**: DWT/DCT 계산된 그리드 주파수 vs. 특성(specification) 주파수 < 5% 편차

**부합 확인**: Frequency domain 분석 및 필터 설계 검증

---

## 공장 vs 현장 비교

### 7.1 공장 캘리브레이션 (Factory Calibration)

| 항목 | 내용 |
|------|------|
| **시기** | 제조 직후 및 품질 검사 시 |
| **환경** | 통제된 실험실 (온도 ± 1°C, 습도 40-60%) |
| **X-ray Source** | 고정식 관(tube) 또는 튜브 안정화 장비 |
| **SID** | 1000 mm (표준 거리) |
| **온도 범위** | 20°C, 30°C, 40°C (3단계) |
| **취득 기간** | ~15-20분 |
| **BPM 유효 기간** | 1년 또는 제조사 정책 |
| **목적** | 초기 hot/cold/flickering pixel 매핑 |

### 7.2 현장 캘리브레이션 (Field Calibration)

| 항목 | 내용 |
|------|------|
| **시기** | 설치 시 및 정기 유지보수 (연 1-2회) |
| **환경** | 임상 설치 위치 (온도 변동 가능) |
| **X-ray Source** | 임상 사용 관 (spectral 변동 가능) |
| **SID** | 현장 설치 거리 (일반적으로 1000 mm) |
| **온도 범위** | 실제 운영 온도 범위 (예: 18-32°C) |
| **취득 기간** | ~15-20분 |
| **BPM 유효 기간** | 설치 후 1년, 이후 정기 검사 마다 갱신 |
| **목적** | 현장 조건 반영 및 drift 추적 |

### 7.3 비교 요약

| 항목 | 공장 | 현장 |
|------|------|------|
| **정확도** | High (통제 환경) | Good (실제 조건) |
| **유효성** | 초기 고가치 | 주기적 갱신 필요 |
| **비용** | 높음 | 중간 |
| **변동 추적** | 기준선 역할 | Trending analysis |
| **임상 사용** | 모든 시스템 | 현장별 맞춤 |

---

**Document Version**: 1.0  
**Total Acquisition Frames**: 800 (dark) + 600 (flat-field) + 200 (flickering) + 200 (grid) = 1,800 frames  
**Total Acquisition Time**: ~45-60 minutes  
**Last Updated**: 2026-04-14  
**Next**: TDS-DEFECT-001 (Test Dataset Specification)
