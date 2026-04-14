# GSVG 영상 취득 프로토콜
## IAP-GSVG-001: Grid Suppression 및 Virtual Grid 캘리브레이션

**문서 ID**: IAP-GSVG-001  
**버전**: 1.0.0  
**일자**: 2026-04-14  
**목표 청중**: 캘리브레이션 엔지니어, 현장 서비스 기술자  
**IEC 62304 분류**: Class B (안전 관련)  
**규범 참조**: [GSVG-SRS-001](GSVG-SRS-001_Requirements.md), [GSVG-SAD-001](GSVG-SAD-001_Architecture.md), [GSVG-SDD-001](GSVG-SDD-001_Detailed_Design.md)

---

## 목차

1. [문서 정보 및 범위](#1-문서-정보-및-범위)
2. [목적 및 적용](#2-목적-및-적용)
3. [용어 정의](#3-용어-정의)
4. [장비 및 환경 요건](#4-장비-및-환경-요건)
5. [Grid Characterization 취득 프로토콜](#5-grid-characterization-취득-프로토콜)
6. [DWT/DCT 필터 파라미터 최적화](#6-dwt-dct-필터-파라미터-최적화)
7. [Virtual Grid 검증 취득](#7-virtual-grid-검증-취득)
8. [Aliasing Risk Assessment](#8-aliasing-risk-assessment)
9. [Grid Library 데이터베이스 구성](#9-grid-library-데이터베이스-구성)
10. [영상 품질 합격 기준](#10-영상-품질-합격-기준)
11. [재캘리브레이션 트리거](#11-재캘리브레이션-트리거)
12. [참고문헌](#12-참고문헌)

---

## 1. 문서 정보 및 범위

### 1.1 개요

이 문서는 X-ray Flat Panel Detector (FPD) 시스템에서 Grid Suppression (GS) 및 Virtual Grid (VG) 모듈(`gsvg.dll`)의 **캘리브레이션을 위한 영상 취득 프로토콜**입니다.

- **범위**: 제조 공장(Factory) 및 의료 현장(Field)에서 grid characterization, filter parameter optimization, 및 virtual grid validation 영상을 취득하는 절차
- **대상**: 캘리브레이션 엔지니어, 현장 서비스 기술자, 의료 물리학자
- **용도**: Grid frequency 파라미터 추출, DWT/DCT 필터 최적화, Virtual Grid CNR 검증, Aliasing risk 평가

### 1.2 지원 Grid 유형

| Grid 유형 | Grid Ratio | 초점 거리 | 전형 f_grid (lp/mm) | 적용 부위 |
|-----------|-----------|---------|-------------------|---------|
| Focused (8:1) | 8:1 | 40cm | 3.2 | 흉부, 골반 |
| Focused (10:1) | 10:1 | 40cm | 4.0 | 흉부, 척추 |
| Focused (12:1) | 12:1 | 40cm | 4.8 | 흉부, 두부 |
| Parallel (6:1) | 6:1 | ∞ | 2.4 | 유방, 인터벤션 |
| Crossed Grid (8:1) | 8:1×8:1 | 40cm | 3.2 (two directions) | 이동식, 특수 |

### 1.3 문서 버전 이력

| 버전 | 일자 | 변경 사항 |
|-----|------|---------|
| 1.0.0 | 2026-04-14 | 초판 발행: Grid characterization + VG validation protocol |

---

## 2. 목적 및 적용

### 2.1 목적

Grid suppression 및 virtual grid 기능의 캘리브레이션 영상 취득의 **일관성, 정확성, 추적성**을 보장합니다:

1. **Grid Parameter 정확성**: Physical grid의 frequency (f_grid), angle (θ_grid), amplitude (A_grid)를 정확히 측정
2. **필터 최적화**: DWT/DCT 필터 강도를 각 grid type에 맞게 최적화
3. **Virtual Grid 검증**: Virtual grid CNR이 physical grid 대비 90% 이상 달성 확인
4. **Aliasing Risk 식별**: 고주파 grid에 대한 suppression 불가능 조건 문서화
5. **재현성**: 공장 캘리브레이션과 현장 재검증 간 일관된 결과 보증

### 2.2 적용 범위

- **Factory Calibration**: 신규 X-ray system 설치 시 모든 grid type 캘리브레이션
- **Field Validation**: 의료 현장 설치 후 QA 및 연 1회 주기 검증
- **Emergency Recalibration**: Physical grid 교체, kVp 변경 후 재검증

---

## 3. 용어 정의

| 용어 | 정의 |
|------|------|
| **f_grid** | Grid line frequency (lp/mm, lines per mm) — DFT spectrum의 peak 주파수 |
| **θ_grid** | Grid orientation angle (degrees) — Hough transform으로 추출 |
| **A_grid** | Grid amplitude (%) — Peak-to-valley intensity ratio in line profile |
| **MSI** | Moiré Severity Index (0–1) — Grid artifact 가시성 정량화 (0: invisible, 1: severe) |
| **CNR** | Contrast-to-Noise Ratio — 병변 대 배경 신호 대 노이즈 비율 |
| **MTF** | Modulation Transfer Function — 공간 주파수별 신호 전달 함수 |
| **SPR** | Scatter-to-Primary Ratio — Scatter radiation 강도 vs primary beam |
| **SID** | Source-to-Image Distance (cm) — X-ray tube에서 detector까지 거리 |
| **f_Nyquist** | Nyquist frequency (lp/mm) — 0.5 × detector sampling frequency |
| **Aliasing** | f_grid > f_Nyquist일 때 frequency folding artifact 발생 |

---

## 4. 장비 및 환경 요건

### 4.1 X-ray System 안정화 요구

| 항목 | 요구사항 | 용도 |
|------|---------|------|
| **kVp 안정성** | ±0.5% (예: 70kVp ± 0.35 kV) | Grid frequency는 beam quality에 약하게 의존 |
| **mAs 안정성** | ±1% (tube current stability) | Pixel intensity uniformity 보증 |
| **Beam hardening** | RQA-5 (ISO spectrum) | Standardized 조사 조건 (재현성) |
| **Detector warm-up** | Minimum 30분 full power | Thermal drift 최소화 (CMOS/aSi 감도 변화) |

### 4.2 환경 조건

| 항목 | 범위 | 근거 |
|------|------|------|
| **실내 온도** | 20–25°C (±1°C) | Grid thermal expansion (α ≈ 10–20 ppm/K) |
| **상대 습도** | 45–55% | Grid alignment 안정성 |
| **Grid alignment** | Perpendicular to beam ±0.5° | Grid tilt는 f_grid detection 오류 유발 |
| **Detector calibration** | 최근 30일 이내 dark/gain maps | 검출기 응답 안정성 |

### 4.3 장비 체크리스트

- [ ] X-ray tube kVp/mA calibration (공인 기관 검증, 최근 12개월)
- [ ] Detector flat-field 최신화 (지난 2주 이내)
- [ ] Grid 장착 위치 확인 (alignment jig 사용)
- [ ] SID measurement 정확성 (±5mm, laser distance meter)

---

## 5. Grid Characterization 취득 프로토콜

### 5.1 목적

Physical anti-scatter grid의 **grid frequency (f_grid), angle (θ_grid), amplitude (A_grid)** 를 정확히 측정하여 DWT/DCT filter 파라미터 결정의 기초 데이터 생성.

### 5.2 기본 설정

**Standard Condition (Factory Baseline)**

| 항목 | 설정값 | 근거 |
|------|-------|------|
| **SID** | 100 cm (고정) | Factory baseline standard |
| **kVp** | 70 kVp | Chest fluoroscopy 표준 |
| **Exposure** | 40–60% saturate (16-bit) | Signal-to-noise 균형 |
| **Frame 수** | 100 (averaged) | 카메라 노이즈 suppression |
| **Spectrum** | RQA-5 (ISO 4037-1) | Standardized beam quality |

### 5.3 Grid Characterization 취득 절차

#### **Step A: Grid-Present Flat Field 촬영**

```
Acquisition Setup:
  1. X-ray tube: 70 kVp, mAs 설정 → 40–60% 16-bit saturation
  2. Grid mounting: Detector 바로 앞 표준 위치 (typical SID 100cm 기준)
  3. Collimation: Full FOV (3072×3072 또는 detector native size)
  4. Pulse mode: Single pulse per frame (모션 artifact 최소화)
  
Acquisition:
  - 100 frames 연속 촬영
  - Averaging: Pixel-wise mean (노이즈 감소)
  - Output: `grid_WITH_100frames_avg.raw` (16-bit, little-endian)
  
Acceptance:
  - Intensity mean: 40,000–50,000 (16-bit range 중 40–60%)
  - Intensity stdev: < 2,000 (균일성 ±4%)
```

#### **Step B: Grid-Absent Flat Field 촬영 (Reference)**

동일한 조사 조건에서 **grid를 제거하고 촬영**:

```
Acquisition:
  - Grid 제거
  - 모든 조사 조건 동일: kVp, mAs, geometry
  - 100 frames averaging
  - Output: `no_grid_100frames_avg.raw`
  
Purpose:
  - Grid presence 확인 (Grid-WITH - Grid-ABSENT = grid artifact)
  - Detector response baseline
```

#### **Step C: 2D FFT 분석 — Grid Parameter 추출**

```
Analysis Pipeline:
  1. Image preparation:
     - Convert to float64
     - Subtract DC offset (intensity mean)
     - Apply Hanning window (spectral leakage 억제)
  
  2. 2D FFT 계산:
     - numpy.fft.fft2() or FFTW
     - Magnitude spectrum |F(u,v)|
     - Log scale visualization
  
  3. Grid frequency 검출:
     Peak detection in frequency domain:
     - Find dominant peaks in magnitude spectrum
     - Exclude DC (0,0) and low-frequency components
     - Typically 2–4 peaks for oriented grid
     
     Expected pattern (8:1 grid at 70kVp):
       - Primary peak: (u, v) ≈ (240, 0) pixels in FFT
       - f_grid = |peak_freq| / image_size × pixel_frequency
       - pixel_frequency = 1 / detector_pixel_pitch (µm)
  
  4. Grid angle θ_grid:
     - Apply Hough transform on peak loci
     - Dominant angle = grid orientation
     - Expected: 0° or 90° for axis-aligned grids
     - Tilted: 3–15° for manually installed grids
  
  5. Grid amplitude A_grid:
     - Extract line profile along grid direction
     - Measure peak intensity - valley intensity
     - A_grid = (peak - valley) / mean
     - Expected: 3–8% for typical anti-scatter grids
```

**Expected Results (8:1 Grid at 70kVp, SID 100cm)**

| Parameter | Value | Tolerance |
|-----------|-------|-----------|
| f_grid | 4.0 lp/mm | ±0.2 lp/mm |
| θ_grid | 0° | ±0.5° |
| A_grid | 5.2% | ±1.5% |

### 5.4 Multi-SID Grid Characterization

**선택 사항**: Portable/mobile 시스템의 경우 여러 SID에서 grid frequency 측정:

| SID (cm) | Expected f_grid Change | Reason |
|----------|----------------------|--------|
| 100 | f_grid_baseline | Reference |
| 115 | +2.9% | Magnification 증가 |
| 130 | +5.8% | Magnification 증가 |
| 150 | +10.3% | Magnification 증가 |

수식: $f_{grid}(SID_2) = f_{grid}(SID_1) \times \frac{SID_2}{SID_1}$

**취득 절차**:
- 각 SID에서 위의 Step A–C 반복
- 결과 저장: `grid_characterization_SID{100,115,130,150}.json`

### 5.5 Multi-kVp Grid Characterization

**선택 사항**: Beam quality 변화에 따른 grid visibility 평가:

| kVp | Reason | Expected f_grid |
|-----|--------|------------------|
| 60 | Low-energy (soft tissue rich) | f_grid slightly lower due to beam hardening |
| 70 | Baseline (chest) | f_grid_baseline |
| 80 | High-energy (bone penetration) | f_grid slightly higher |
| 100 | High-energy (obese patients) | f_grid continues rising |
| 125 | Maximum clinical | f_grid plateau |

**취득**: 각 kVp에서 Step A–B–C 반복.

---

## 6. DWT/DCT 필터 파라미터 최적화

### 6.1 목적

Grid suppression 필터 강도를 최적화하여:
- **MSI < 0.1** (grid artifact invisible)
- **CNR 손실 < 5%** (진단 성능 보존)
- **MTF @ Nyquist > 95%** (공간 해상도 유지)

### 6.2 Phantom 설정

**CDRAD 2.0 Phantom** (Artinis Medical Systems)

| 항목 | 설명 |
|-----|------|
| **구성** | 100+ contrast-detail discs (varying diameter/contrast) |
| **FOV** | 200×200mm (detector에 맞춤) |
| **위치** | Detector 중앙, grid + phantom 조합 촬영 |

**Phantom 자세**: Parallel to detector (자체 calibration curve 확보 가능)

### 6.3 필터 강도 스윕 (Tier 1: DWT)

**DWT Bandstop Filter Parameter Sweep**

```
Test Matrix:
  Wavelet: Daubechies-4 (db4)
  Decomposition levels: 3, 4, 5
  Bandstop width: ±1, ±1.5, ±2 pixels (frequency domain)
  Gaussian σ: 1.0, 1.5, 2.0 pixels
  
Test Design (5×3 grid):
  - 15 configurations
  - Each config: Single CDRAD image processed
  - Per config output metrics:
    * MSI (Moiré Severity Index)
    * CNR_artifact-free / CNR_grid_present ratio
    * MTF preservation @ diagnostic freq (2–4 lp/mm)
```

### 6.4 필터 강도 스윕 (Tier 2: DCT)

**DCT Dynamic Segmentation Parameter Sweep**

```
Test Matrix:
  Block size: 32×32, 64×64, 128×128 pixels
  Harmonic suppression: f_grid, f_grid ± f_grid/2 (1.5× harmonic)
  Wiener filter SNR threshold: 2, 4, 8 dB
  Inter-block blending: none, linear taper (5 pixel fade)
  
Per block output:
  - DCT coefficient suppression energy
  - Blocking artifact at inter-block boundaries
```

### 6.5 합격 기준

**Acceptance Criteria**

| 지표 | 기준 | 측정 방법 |
|------|------|---------|
| **MSI** | < 0.1 | CDRAD image: grid visibility score (0–1) |
| **CNR 손실** | < 5% | CNR_processed / CNR_reference × 100% |
| **MTF @ 3 lp/mm** | > 95% | Wire phantom MTF curve |
| **Blocking Artifact** (DCT) | Invisible | Visual inspection + power spectrum |

**선택 기준**: MSI 달성 + MTF 손실 최소인 configuration 선택.

### 6.6 결과 저장 형식

**`grid_filter_optimization_{tier1_or_tier2}.json`**

```json
{
  "grid_type": "8:1 Focused",
  "grid_frequency_lp_mm": 4.0,
  "optimization_tier": "DWT",
  "configurations_tested": 15,
  "best_configuration": {
    "wavelet": "db4",
    "decomposition_levels": 4,
    "bandstop_width_px": 2,
    "gaussian_sigma_px": 1.5,
    "msi_achieved": 0.08,
    "cnr_preservation_percent": 97.2,
    "mtf_at_3lpmm_percent": 96.5
  },
  "date": "2026-04-14",
  "engineer": "John Doe"
}
```

---

## 7. Virtual Grid 검증 취득

### 7.1 목적

**Virtual Grid (VG)** 알고리즘이 physical grid 없이 동등한 CNR을 달성하는지 검증.

### 7.2 Reference Acquisition: Physical Grid 있음

| 항목 | 설정 |
|-----|------|
| **Phantom** | CDRAD 2.0 (동일) |
| **Grid** | 6:1 or 8:1 focused (표준) |
| **kVp/mAs** | Chest standard (70 kVp, 40–60% saturate) |
| **Frames** | 10 (averaging) |
| **Output** | `vg_reference_physical_grid.raw` |

### 7.3 Test Acquisition: Grid-less, Virtual Grid 적용

| 항목 | 설정 |
|-----|------|
| **Phantom** | 동일 CDRAD 2.0 |
| **Grid** | **제거** |
| **조사 조건** | 물리적 grid 영상과 동일 |
| **VG Algorithm** | gsvg_virtual_grid() 실행 |
| **VG Ratio** | 6:1 or 8:1 (reference와 일치) |
| **Output** | `vg_test_virtual_grid.raw` |

### 7.4 Scatter Fraction Measurement (Pb-blocker Method)

**Physical Grid 영상의 Scatter Fraction 정량화**

```
Setup:
  1. Lead blocker: 1mm Pb sheet, 선 형태 (detector 위 positioning)
  2. Acquisition:
     - Full field X-ray (CDRAD + grid)
     - Blocker 있음 → Primary + grid artifact
     - Blocker 제거 → Primary + scatter + grid artifact
  3. Scatter estimation:
     Scatter = (Full field) - (Behind blocker) - (Grid artifact)
     SPR = Scatter / Primary
```

### 7.5 CNR 비교 분석

**CDRAD Phantom CNR-D Curve**

```
Analysis:
  1. Extract contrast-detail disc detection threshold
     - Physical grid: CDT_physical (baseline)
     - Virtual grid: CDT_virtual
  2. Calculate CNR improvement:
     CNR_improvement = (CDT_physical - CDT_virtual) / CDT_physical × 100%
  3. Acceptance criterion:
     CNR_virtual >= 90% × CNR_physical
```

### 7.6 Leeds TOR CDR (Contrast Resolution)

**선택 사항**: Clinical phantom으로 추가 검증

| Phantom | Metric | Expected |
|---------|--------|----------|
| Leeds TOR CDR | Contrast resolution score | ≥ 90% of physical grid |
| ALVIM | Scatter control | < 0.3% SPR reduction |

---

## 8. Aliasing Risk Assessment

### 8.1 목적

$f_{grid} > 0.8 \times f_{Nyquist}$ 조건에서 **aliasing으로 인한 suppression 불가능 영역** 식별.

### 8.2 Aliasing 검출 절차

**Step 1: Nyquist Frequency 계산**

```
f_Nyquist = 1 / (2 × detector_pixel_pitch_mm)

Example (100µm pixel pitch):
  f_Nyquist = 1 / (2 × 0.1) = 5.0 lp/mm
```

**Step 2: Aliasing Risk Zone 확인**

```
IF f_grid > 0.8 × f_Nyquist:
  THEN Aliasing Risk Detected
  ELSE No aliasing risk
  
Example:
  f_grid = 4.0 lp/mm
  Threshold = 0.8 × 5.0 = 4.0 lp/mm
  → At boundary (marginal risk)
```

**Step 3: Aliasing Artifact 가시화**

```
Synthetic Grid Generation:
  1. Create pure sinusoidal grid image
     I(x,y) = offset + amplitude × sin(2π × f_grid × x)
  2. Apply detector sampling: pixel_pitch = 100µm
  3. Observe aliasing Moiré pattern at low SID or high f_grid
  4. Verify gsvg_detect_grid() issues warning
```

### 8.3 Aliasing이 있을 때의 Suppression 한계

| Scenario | Expected Behavior |
|----------|------------------|
| **Partial Aliasing** (0.7 < f_grid/f_Nyquist < 0.8) | Suppression 50–70% 가능, warning issued |
| **Full Aliasing** (f_grid/f_Nyquist > 0.8) | Suppression impossible (frequency folded), strong warning |
| **Action** | Original image passed through, log warning, recommend physical grid or lower kVp |

### 8.4 문서화

**`grid_aliasing_analysis_{grid_type}.json`**

```json
{
  "grid_type": "12:1 Focused",
  "detector_pixel_pitch_mm": 0.1,
  "f_nyquist_lp_mm": 5.0,
  "f_grid_measured_lp_mm": 4.8,
  "aliasing_ratio": 0.96,
  "aliasing_risk": "MARGINAL",
  "recommendation": "Monitor; suppress quality may degrade at high SID",
  "warning_threshold": true,
  "date": "2026-04-14"
}
```

---

## 9. Grid Library 데이터베이스 구성

### 9.1 Grid Library 목적

모든 grid characterization 결과를 **grid library database**로 저장하여 runtime에 자동으로 filter parameters 로드.

### 9.2 Grid Library 구조

**Directory Layout**

```
gsvg_grid_library/
├── README.md
├── grid_types.json              # Master registry
├── grids/
│   ├── grid_8_1_focused_40cm.json
│   ├── grid_10_1_focused_40cm.json
│   ├── grid_12_1_focused_40cm.json
│   ├── grid_6_1_parallel.json
│   └── grid_8_1_crossed.json
├── filter_parameters/
│   ├── dwt_tier1_8_1_focused.json
│   ├── dct_tier2_8_1_focused.json
│   └── ...
├── validation_data/
│   ├── cnr_reference_8_1_focused.json
│   └── mtf_baseline_8_1_focused.json
└── calibration_logs/
    ├── calibration_2026_04_14_factory.log
    └── calibration_2026_04_14_field_update.log
```

### 9.3 Grid Registry File Format

**`grid_types.json`**

```json
{
  "version": "1.0",
  "generation_date": "2026-04-14",
  "grids": [
    {
      "id": "grid_8_1_focused_40cm",
      "manufacturer": "Antares TRTL",
      "grid_ratio": "8:1",
      "focus_distance_cm": 40,
      "frequency_lp_mm": 4.0,
      "frequency_tolerance_lp_mm": 0.2,
      "angle_degrees": 0.0,
      "angle_tolerance_degrees": 0.5,
      "amplitude_percent": 5.2,
      "amplitude_tolerance_percent": 1.5,
      "f_nyquist_lp_mm": 5.0,
      "aliasing_ratio": 0.8,
      "aliasing_risk": "NO",
      "filter_tier1_config": "dwt_tier1_8_1_focused.json",
      "filter_tier2_config": "dct_tier2_8_1_focused.json",
      "cnr_reference_db": "cnr_reference_8_1_focused.json",
      "mtf_baseline_db": "mtf_baseline_8_1_focused.json",
      "calibration_date": "2026-04-14",
      "calibration_engineer": "John Doe",
      "expected_virtual_grid_cnr_percent": 92.0
    }
  ]
}
```

### 9.4 Filter Parameter File Format

**`dwt_tier1_8_1_focused.json`**

```json
{
  "algorithm": "DWT Bandstop",
  "grid_type": "8:1 Focused",
  "grid_frequency_lp_mm": 4.0,
  "wavelet": "Daubechies-4",
  "decomposition_levels": 4,
  "bandstop_width_frequency_domain_px": 2,
  "gaussian_sigma_px": 1.5,
  "performance_metrics": {
    "msi_achieved": 0.08,
    "cnr_preservation_percent": 97.2,
    "mtf_at_3lpmm_percent": 96.5,
    "processing_time_ms": 28
  },
  "validation_date": "2026-04-14"
}
```

---

## 10. 영상 품질 합격 기준

### 10.1 Grid Characterization Image Acceptance

| 기준 | 값 | 검증 방법 |
|------|------|---------|
| **Intensity Mean** | 40,000–50,000 (16-bit) | Histogram mean |
| **Intensity Stdev** | < 2,000 (±4%) | Histogram stdev |
| **Clipping** | < 0.1% (both ends) | Pixel value distribution |
| **Grid Visibility** | f_grid spectral peak SNR > 20dB | 2D FFT magnitude |

### 10.2 Filter Optimization Image Acceptance

| 기준 | 값 | 검증 방법 |
|------|------|---------|
| **MSI (Moiré Severity Index)** | < 0.1 | CDRAD phantom grid artifact assessment |
| **CNR Preservation** | > 95% | CDRAD contrast-detail curve comparison |
| **MTF @ 3 lp/mm** | > 95% | Wire phantom MTF measurement |
| **Blocking Artifacts** (DCT) | Invisible | Visual + power spectrum inspection |

### 10.3 Virtual Grid Validation Image Acceptance

| 기준 | 값 | 검증 방법 |
|------|------|---------|
| **CNR Equivalence** | ≥ 90% physical grid | CDRAD CDT comparison |
| **Overcorrection Artifact** | None visible | Radiologist review |
| **Intensity Range** | 0–65535 (no clipping) | Histogram validation |

---

## 11. 재캘리브레이션 트리거

### 11.1 필수 재캘리브레이션 조건

| 이벤트 | 액션 |
|-------|------|
| **Physical grid 교체** | 신규 grid에 대해 전체 characterization 반복 (Section 5) |
| **kVp 변경 > 20 kVp** | Multi-kVp characterization 재실행 (Section 5.5) |
| **SID 변경 > 15 cm** | Multi-SID characterization 재실행 (Section 5.4) |
| **연 1회 주기 검증** | Field validation: Step A–B–C 간단 버전 수행 |
| **온도 보상 재조정** | Filter parameters 검증 필요 |

### 11.2 재캘리브레이션 절차 (Simplified)

```
Trigger Event → Recalibration Initiated

IF grid replacement:
  THEN Execute full Section 5 (all steps A–C)
  AND update grid_types.json
  
ELSE IF kVp change or SID change:
  THEN Execute multi-kVp or multi-SID variant (5.4–5.5)
  AND update grid library
  
ELSE IF annual field validation:
  THEN Execute Step A–B–C with single SID (100cm) and kVp
  AND verify f_grid within tolerance
  AND verify filter performance (MSI, CNR, MTF)
  AND log to calibration_logs/

ENDIF
```

---

## 12. 참고문헌

### 표준 및 규제

- **IEC 62304:2015** — Medical device software lifecycle processes (Class B)
- **IEC 62220-1-1:2015** — Medical electrical equipment — Determination of the detective quantum efficiency
- **ISO 4037-1:2019** — Radiation protection — X and gamma reference radiation for calibration of dosimeters and dose rate meters

### 기술 논문

- **Tang et al. (2015)** — "Grid artifact removal using wavelet transform and Gaussian band-stop filter," Medical Physics, vol. 42, no. 9, pp. 5432–5441.
  - DWT decomposition strategy, band-stop filter parameter selection

- **Lin et al. (2006)** — "Comparison of grid suppression algorithms," J. Digital Imaging, vol. 19, no. 3, pp. 268–278.
  - Gaussian vs. notch filter comparison

- **Neitzel et al. (2006)** — "Virtual grid for scatter radiation correction without physical grid," Proc. SPIE, vol. 6142, pp. 614210.
  - Virtual grid CNR performance benchmarks

- **Lisson et al. (2020)** — "Clinical evaluation of virtual grid scatter correction in chest radiography," Radiology, vol. 297, no. 2, pp. 398–407.
  - Clinical CNR improvement data (90% equivalence criterion)

- **Lim et al. (2023)** — "Noise amplification in scatter subtraction: Wiener filter optimization," IEEE TMI, vol. 42, no. 8, pp. 2245–2256.
  - De-noising strategy after scatter subtraction

### 기타 참고 자료

- **GSVG-SRS-001** — Software Requirements Specification
- **GSVG-SAD-001** — Software Architecture Design
- **GSVG-SDD-001** — Software Detailed Design
- **GSVG-SVP-001** — Software Verification Plan

---

## 부록 A: Grid Frequency 계산 예시

**예**: 8:1 focused grid, SID 100cm

```
Grid line pitch (physical): 1 / 8 = 0.125 mm
Magnification at SID 100cm: Assume 1× (detector at image plane)
Grid frequency on detector: 1 / 0.125 = 8.0 lp/mm

(Note: Actual f_grid depends on detector position relative to focus)

Detector pixel pitch: 100 µm = 0.1 mm
Sampling frequency: 1 / 0.1 = 10 lp/mm
Nyquist frequency: 10 / 2 = 5 lp/mm

Aliasing risk: f_grid / f_Nyquist = 8.0 / 5.0 = 1.6 → ALIASING DETECTED
→ Use lower SID or accept suppression limitation
```

---

## 부록 B: MSI (Moiré Severity Index) 계산

```python
def calculate_msi(image_processed, image_reference):
    """
    MSI = Power of residual grid artifact / Maximum possible artifact power
    
    Args:
        image_processed: Grid suppressed image
        image_reference: Grid-free reference image
    
    Returns:
        msi: Moiré Severity Index (0–1, 0=invisible, 1=severe)
    """
    # 2D FFT
    F_proc = np.fft.fft2(image_processed.astype(np.float32))
    F_ref = np.fft.fft2(image_reference.astype(np.float32))
    
    # Extract frequency components around grid frequency (±0.5 lp/mm)
    # Assuming f_grid = 4.0 lp/mm → search region: 3.5–4.5 lp/mm
    grid_region = extract_frequency_band(np.abs(F_proc), f_grid, bandwidth=0.5)
    max_artifact = extract_frequency_band(np.abs(F_ref), f_grid, bandwidth=0.5)
    
    # MSI = residual / maximum
    msi = np.sum(grid_region) / (np.sum(max_artifact) + 1e-10)
    return np.clip(msi, 0.0, 1.0)
```

---

**문서 승인:**

| 역할 | 이름 | 날짜 | 서명 |
|------|------|------|------|
| 저자 | | | |
| 검토자 | | | |
| 승인자 | | | |

**Revision History**

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | — | Initial release |
