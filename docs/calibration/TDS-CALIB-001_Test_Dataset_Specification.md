# Test Dataset Specification - XPE Calibration Preprocessing Module

**문서 ID**: TDS-CALIB-001 v1.0  
**IEC 62304 절차**: 5.6.3 (테스트 설계 및 실행)  
**안전 분류**: Class B  
**작성일**: 2026-04-14  
**담당 부서**: XPE 캘리브레이션 개발팀  
**승인**: __________________ 날짜: __________  

---

## 목차

1. [문서 정보 및 개요](#1-문서-정보-및-개요)
2. [목적 및 범위](#2-목적-및-범위)
3. [테스트 데이터 분류 체계](#3-테스트-데이터-분류-체계)
4. [알고리즘별 테스트 데이터 명세](#4-알고리즘별-테스트-데이터-명세)
5. [합성 데이터 생성 방법론](#5-합성-데이터-생성-방법론)
6. [실제 영상 데이터 요건](#6-실제-영상-데이터-요건)
7. [Golden Reference 데이터 관리](#7-golden-reference-데이터-관리)
8. [데이터셋 디렉토리 구조](#8-데이터셋-디렉토리-구조)
9. [IEC 62304 추적성](#9-iec-62304-추적성)
10. [참고문헌](#10-참고문헌)

---

## 1. 문서 정보 및 개요

### 1.1 개요

이 문서는 XPE 전처리 캘리브레이션 모듈(`xpe_preprocess.dll`)의 9개 보정 알고리즘을 검증하기 위해 개발 팀이 사용할 테스트 데이터셋의 명세입니다. 개발자는 이 명세에 따라 합성 데이터를 생성하고, 실제 검출기 영상을 수집하고, 알고리즘의 정확성과 견고성을 검증합니다.

### 1.2 대상 독자

- 알고리즘 개발 엔지니어 (캘리브레이션, 보정 처리)
- 소프트웨어 품질 보증 담당자 (테스트 계획 수립)
- 검증/검증(V&V) 담당자 (IEC 62304 준수)
- 영상 처리 엔지니어 (알고리즘 성능 평가)

### 1.3 문서 버전 이력

| 버전 | 날짜 | 변경 사항 | 담당자 |
|------|------|---------|--------|
| 1.0 | 2026-04-14 | 초기 버전: 9개 알고리즘 전체 명세 | XPE Calib Team |

---

## 2. 목적 및 범위

### 2.1 목적

이 Test Dataset Specification(TDS)은 다음을 달성합니다:

1. **알고리즘 검증**: 각 보정 알고리즘의 정확성, 견고성, 경계 조건을 체계적으로 검증
2. **재현성**: 합성 데이터를 통해 100% 결정론적 테스트 케이스 제공
3. **추적성**: 각 테스트 데이터셋이 어떤 요구사항(SRS-CALIB-FUNC-xxx)을 검증하는지 명확한 매핑
4. **IEC 62304 준수**: 테스트 데이터와 결과가 의료기기 안전 표준과 연결되도록 보장

### 2.2 범위

**포함 사항:**
- Offset (Dark) Correction (Stage 1)
- Gain (Flat-field) Correction (Stage 2)
- Defect Pixel Correction (Stage 3)
- Lag/Ghost Correction (Stage 4)
- Temperature Compensation (Stage 0.7)
- Nonlinearity Correction (Stage 1.5)
- Binning Correction (Stage 2.5)
- Readout Artifact Validation (Stage 0.5)
- Calibration Data I/O & Expiry Management (Stage 0)

**제외 사항:**
- Enhancement processing (xpe_enhance_basic.dll) - 후속 모듈
- Grid suppression (gsvg.dll) - 후속 모듈
- Machine learning defect detection (FixPix MLP) - 별도 모듈

---

## 3. 테스트 데이터 분류 체계

### 3.1 데이터 유형별 분류

| 유형 | 생성 방식 | 재현성 | 크기 | 용도 |
|------|---------|--------|------|------|
| **합성 (Synthetic)** | 프로그래밍 방식으로 생성 | 100% 결정론적 | 100-500 MB | 단위 테스트, 회귀 테스트 |
| **실제 (Real)** | 실제 검출기에서 취득 | 비재현적 | 1-10 GB | 통합 테스트, 성능 검증 |
| **Golden Reference** | 승인된 알고리즘 출력 | 영구 고정 | 100-500 MB | 회귀 테스트, 버전 관리 |

### 3.2 데이터 생성 계층

```
┌─────────────────────────────────────────────────┐
│  Golden Reference (알고리즘 v1.0 기준 성능)     │
│  저장: golden_reference/*.dcm, *.raw            │
│  SSIM > 0.999 기준, 버전 고정                   │
└─────────────────────────────────────────────────┘
                    ▲
                    │ 비교
                    │
┌─────────────────────────────────────────────────┐
│  Real Data (실제 검출기 영상)                   │
│  저장: real/dark/, real/flatfield/, etc.       │
│  Source: IAP-CALIB-001 참조                     │
│  최소 수량: 알고리즘당 30프레임                  │
└─────────────────────────────────────────────────┘
                    ▲
                    │ 검증
                    │
┌─────────────────────────────────────────────────┐
│  Synthetic Data (프로그래밍 생성)               │
│  저장: synthetic/dark/, synthetic/flatfield/   │
│  Ground Truth 포함: _true.raw, _degraded.raw  │
│  빠른 개발 순환, 단위 테스트용                  │
└─────────────────────────────────────────────────┘
```

---

## 4. 알고리즘별 테스트 데이터 명세

### 4.1 Offset (Dark) Correction 테스트 데이터

#### 4.1.1 테스트 목적

- 어두운 전류 차감 산술 연산 검증
- 음수 값 클램핑(uint16 도메인) 동작 확인
- 온도/PREP 시간 동적 보간 정확성 검증
- 시계열 온도 변화에 따른 어두운 드리프트 추적

#### 4.1.2 합성 데이터 명세

**Dataset**: `synthetic/dark_frames/`

| 테스트 케이스 | 설명 | 입력 매개변수 | Ground Truth |
|-------------|------|-------------|-------------|
| **offset_uniform** | 균일한 어두운 레벨 (5, 10, 50, 100, 500 ADU) | 어두운 레벨 D, 깨끗한 신호 I | I_corrected = max(0, I - D) |
| **offset_nonuniform_spatial** | 공간 변화 어두운 (저주파 gradient) | D(x,y) = 50 + 0.01×x, 신호 I(x,y)=1000 | Pixel-wise 차감 후 클램핑 |
| **offset_clamping_edge** | 경계값 테스트: I_raw=10, D=20 | D=20, I=10 | I_corrected=0 (클램핑 확인) |
| **offset_temp_interp_2pt** | 온도 2점 보간 | T1=20°C (D1=100), T2=30°C (D2=125), interpolate at 25°C | D(25°C) = (100+125)/2 = 112.5 |
| **offset_temp_interp_4pt** | 온도 4점 보간 (이중선형) | T={20,25,30,35}°C, PREP={10,20}ms | Bilinear interpolation 검증 |
| **offset_time_series** | 시간 경과에 따른 드리프트 추적 | 초기 D=100, 온도 변화로 인한 지수 모델 | 드리프트 곡선이 exp(-E_g/2k_B×T) 추종 |

**파일 형식**:
- `offset_uniform_5ADU_true.raw` (uint16, 3072×3072, 예상되는 "깨끗한" 입력)
- `offset_uniform_5ADU_degraded.raw` (uint16, 3072×3072, 5 ADU 어두운 레벨 추가)
- `offset_uniform_5ADU_darkmap.raw` (uint16, 3072×3072, 어두운 맵 계수)

**합격 기준**:
- 평균 오차 < 1 ADU (클램핑 적용 후)
- 클램핑된 픽셀 비율 < 0.1%
- 온도 보간 RMSE < 2 ADU

#### 4.1.3 실제 영상 데이터 명세

**출처**: IAP-CALIB-001 **섹션 6.1** (Dark Frame 취득)

> **[교차검증 수정]** 섹션 번호 정정: PRD PREP time 단위는 초(s) — 1, 2, 5, 10, 15, 20, 30초 (REQ-OFF-004)

| 조건 | 최소 수량 | 취득 조건 |
|------|--------|---------|
| 온도 기준 (25°C) | 10프레임 | PREP=10s, 검출기 안정화 30분 후 |
| 온도 20°C | 10프레임 | 실험실 냉방, 온도 센서 확인 |
| 온도 30°C | 10프레임 | 열판 또는 환경실, ±0.5°C 안정성 |
| 온도 35°C | 10프레임 | 극한 조건 테스트 |
| PREP 시간 변수 | 20프레임 | PREP=1s, 5s, 10s, 30s at 25°C (7개 레벨 중 4개 대표값) |
| 시간 드리프트 시퀀스 | 50프레임 | 4시간 연속 획득, 1시간당 1프레임 |

**파일 명명 규칙**:
- `real/dark_frames/DARK_T025_PREP10s_F001.raw` (온도 25°C, PREP 10초, 프레임 1)
- `real/dark_frames/metadata_T025_PREP10s.json` (메타데이터: 타임스탬프, 온도 센서값, PREP time)

**익명화 요건**: 없음 (환자 데이터 아님, 순수 어두운 프레임)

#### 4.1.4 Ground Truth 형식

**합성 데이터**:
```json
{
  "test_case": "offset_uniform_5ADU",
  "input_parameters": {
    "dark_level_adu": 5,
    "image_dimensions": [3072, 3072],
    "bit_depth": 16,
    "format": "uint16"
  },
  "expected_output": {
    "mean_dark_residual_adu": 0.0,
    "std_dark_residual_adu": 0.0,
    "clamped_pixels_percent": 0.0,
    "output_format": "uint16",
    "dimensions": [3072, 3072]
  },
  "algorithm_parameters": {
    "interpolation_method": "bilinear_temp_prep",
    "clamp_negative_to_zero": true
  }
}
```

**실제 데이터**:
```json
{
  "file": "DARK_T025_PREP100_F001.raw",
  "acquisition_timestamp": "2026-04-14T14:30:00Z",
  "detector_temperature_celsius": 25.0,
  "prep_time_s": 10,
  "frame_number": 1,
  "metadata": {
    "sensor_reading": "25.1°C",
    "humidity": "45%",
    "source_status": "off"
  }
}
```

#### 4.1.5 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-004 | 오프셋 차감 공식 및 클램핑 |
| SRS-CALIB-FUNC-008 | 온도 보상 모델 (보간 일부) |
| SRS-CALIB-SAFE-001 | 오프셋 필수 실행 정책 |

---

### 4.2 Gain (Flat-field) Correction 테스트 데이터

#### 4.2.1 테스트 목적

- 게인(정규화) 연산 검증
- uint16 → float32 형식 변환 정확성
- 공간 비균일성(FPN) 보정 효과 검증
- 다중 게인 다항식 (다중 에너지 모드) 지원 확인
- Heel effect 보상 정확성

#### 4.2.2 합성 데이터 명세

**Dataset**: `synthetic/flatfield_frames/`

| 테스트 케이스 | 설명 | 게인 특성 | Ground Truth |
|-------------|------|---------|-------------|
| **gain_uniform** | 균일 게인 (G=1.0 everywhere) | 공간 균일 | I_norm = I / 1.0 → float32 변환만 |
| **gain_nonuniform_10pct** | 10% FPN (Pixel Pitch 기반) | G(x,y) = 1.0 + 0.05×sin(2π×x/P) | σ/μ < 0.5% after correction |
| **gain_nonuniform_15pct** | 15% FPN (현실적 a-Si) | G(x,y) = random(0.85, 1.15) | σ/μ < 1.0% after correction |
| **gain_heel_effect** | Heel effect pattern | G(x,y) = 1.0 - 0.1×(x²+y²)/max² | Duo-SID 모델 검증 |
| **gain_multi_gain_2energy** | 다중 게인 다항식 (2개 에너지) | G(E) = c0 + c1×E | E-dependent correction selection |
| **gain_division_by_zero_edge** | 게인=0인 픽셀 처리 | G(x,y) = 0 (corrupt pixel) | Error code XPE_ERR_INVALID_CALIB_DATA |
| **gain_range_validation** | 게인 범위 체크 | G_min=0.1, G_max=10.0 | Out-of-range 거부 |
| **gain_format_conversion** | uint16 → float32 정확성 | I_uint16 in [0, 16383] | Bit-exact float32 변환 검증 |

**파일 형식**:
- `gain_uniform_true.raw` (float32, 3072×3072)
- `gain_uniform_uint16.raw` (uint16, 3072×3072, offset-corrected)
- `gain_uniform_gainmap.raw` (float32, 3072×3072, 게인 맵)
- `gain_uniform_expected.raw` (float32, 3072×3072, 예상 출력)

**합격 기준**:
- σ/μ < 0.5% (합성 10% FPN)
- σ/μ < 1.0% (합성 15% FPN)
- 형식 변환 오차 < 1e-6 (상대)
- 범위 위반 픽셀 0개

#### 4.2.3 실제 영상 데이터 명세

**출처**: IAP-CALIB-001 **섹션 6.2** (Flat-field Frame 취득)

| 조건 | 최소 수량 | 취득 조건 |
|------|--------|---------|
| 40% 포화도, SID 1000mm | 10프레임 | 표준 영상 조건 |
| 60% 포화도, SID 1000mm | 10프레임 | 높은 신호 품질 |
| 40% 포화도, SID 1500mm | 10프레임 | Heel effect 검증용 |
| 60% 포화도, SID 1500mm | 10프레임 | 다중 SID 캘리브레이션 |
| Multi-gain 다항식 (2 에너지) | 20프레임 | 에너지별 10프레임씩 |

**파일 명명**:
- `real/flatfield_frames/FF_SAT40_SID1000_F001.raw`
- `real/flatfield_frames/metadata_SAT40_SID1000.json`

#### 4.2.4 Ground Truth 형식

```json
{
  "test_case": "gain_nonuniform_15pct",
  "input_parameters": {
    "fpn_percent": 15,
    "flatfield_saturation_percent": 60,
    "image_dimensions": [3072, 3072],
    "input_format": "uint16",
    "output_format": "float32"
  },
  "expected_output": {
    "mean_normalized_intensity": 1.0,
    "std_normalized_intensity": 0.01,
    "coefficient_of_variation_percent": 1.0,
    "format": "float32"
  }
}
```

#### 4.2.5 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-005 | 게인 보정 공식 및 float32 변환 |
| SRS-CALIB-FUNC-002 | 게인 맵 로드 및 범위 검증 |
| SRS-CALIB-SAFE-001 | 게인 필수 실행 정책 |
| SRS-CALIB-SAFE-005 | 오버플로우 보호 |

---

### 4.3 Defect Pixel Correction 테스트 데이터

#### 4.3.1 테스트 목적

- 결함 픽셀 검출 정확성 (감지율 > 99.6%)
- 거짓 양성 제어 (FPR < 0.6%)
- 다양한 결함 유형 처리 (dead, hot, stuck, noisy)
- RMM(Robust Mask Maker) 알고리즘 검증 (lambda=8.0)
- 보간 방법의 정확성 (이웃 평균, 쌍선형, 중앙값)

#### 4.3.2 합성 데이터 명세

**Dataset**: `synthetic/defects/`

| 테스트 케이스 | 결함 유형 | 주입 비율 | Ground Truth |
|-------------|---------|---------|-------------|
| **defect_dead_0.01pct** | Dead (value=0) | 0.01% | 모든 dead 픽셀 정위치 검출 |
| **defect_hot_0.01pct** | Hot (value=max) | 0.01% | 모든 hot 픽셀 정위치 검출 |
| **defect_stuck_0.01pct** | Stuck row/col | 0.01% (5 rows) | Row/col 결함 라인 검출 |
| **defect_noisy_0.1pct** | Noisy (CV > 20%) | 0.1% | SNR < 5dB 픽셀 검출 |
| **defect_cluster_2x2** | 2×2 cluster | 0.1% | Cluster 전체 보간 |
| **defect_cluster_3x3** | 3×3 cluster | 0.1% | Cluster 전체 보간 |
| **defect_cluster_5x5** | 5×5 cluster | 0.05% | Large cluster 보간 |
| **defect_mixed_0.5pct** | Mixed types | 0.5% | 모든 결함 유형 혼합 |
| **defect_injection_rates** | 0.01%-1.0% range | 여러 비율 | 주입 비율별 검출 곡선 |

**파일 형식**:
- `defect_dead_0.01pct_clean.raw` (float32, 3072×3072, ground truth "clean")
- `defect_dead_0.01pct_degraded.raw` (float32, 3072×3072, dead 픽셀 주입)
- `defect_dead_0.01pct_defect_map.raw` (uint8, 3072×3072, 결함 위치 & 유형)
- `defect_dead_0.01pct_bpm.raw` (uint8, 3072×3072, Bad Pixel Map)

**합격 기준**:
- 감지율 > 99.6% (모든 주입된 결함)
- 거짓 양성율 < 0.6% (깨끗한 영역에서)
- 보간 오차 < 2% (원래 신호 대비)

#### 4.3.3 실제 영상 데이터 명세

**출처**: IAP-CALIB-001 **섹션 6.3** (BPM 생성용 영상 취득)

| 조건 | 최소 수량 | 취득 조건 |
|------|--------|---------|
| Dark frames (BPM 생성용) | 30프레임 | PREP=20s (Hot pixel 검출 시 긴 PREP 권장), 25°C |
| Flatfield frames (BPM 검증용) | 30프레임 | 40% saturation |
| High-contrast edge (보간 검증용) | 10프레임 | 알려진 결함 픽셀 근처 |
| Known defect regions | 10프레임 | 공장 캘리브레이션에서 식별된 결함 |

**파일 명명**:
- `real/defects/BPM_REF.raw` (uint8, 3072×3072, 기준 BPM)
- `real/defects/DARK_FOR_BPM_001.raw` (uint16, 3072×3072)
- `real/defects/FF_FOR_BPM_001.raw` (float32, 3072×3072)

#### 4.3.4 Ground Truth 형식

```json
{
  "test_case": "defect_mixed_0.5pct",
  "input_parameters": {
    "total_defect_count": 4718,
    "defect_distribution": {
      "dead": "30%",
      "hot": "35%",
      "stuck_row_col": "15%",
      "noisy": "20%"
    },
    "image_dimensions": [3072, 3072]
  },
  "expected_output": {
    "detection_rate_percent": 99.6,
    "false_positive_rate_percent": 0.6,
    "interpolation_rmse": 0.02,
    "bpm_format": "uint8_0_to_255"
  }
}
```

#### 4.3.5 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-007 | 결함 픽셀 보정 (보간 방법) |
| SRS-CALIB-FUNC-010 | 런타임 결함 검출 (SNR 기반) |
| SRS-CALIB-FUNC-003 | BPM 로드 및 파싱 |

---

### 4.4 Lag/Ghost Correction 테스트 데이터

#### 4.4.1 테스트 목적

- LTI 다중 지수 역컨볼루션 (N=4) 검증
- Exposure-weighted LTI (Tier 2) 정확성
- NLCSC (Non-Linear Correlation Signal Correction, Tier 3) 검증
- 자동 계층화 메커니즘 (Tier 1→2→3)
- 노출 이력 링 버퍼 관리 (8-16 프레임)

#### 4.4.2 합성 데이터 명세

**Dataset**: `synthetic/lag/`

**FSRF (Forward Step Response Function)**: 노출 프레임 시퀀스 → 어두운 프레임 시퀀스

**RSRF (Reverse Step Response Function)**: 어두운 프레임 시퀀스 → 노출 프레임 시퀀스

| 테스트 케이스 | 노출 수준 | 프레임 수 | LTI 모델 | Ground Truth |
|-------------|---------|---------|---------|-------------|
| **lag_tier1_lti_fsrf** | 27% saturation | 200 | Varex 4030CB 기준 파라미터 | 1st frame lag < 0.3% |
| **lag_tier1_lti_rsrf** | 27% saturation | 200 | 역 함수 (RSRF) | 50th frame lag < 0.01% |
| **lag_tier2_exposure_weighted** | 27%, 50%, 75% mixed | 100 frames | E-weighted coefficients | Residual lag < 10% after Tier 1 |
| **lag_tier3_nlcsc_9levels** | 2%, 5%, 10%, 20%, 30%, 50%, 70%, 80%, 92% | 각 40 frames | Signal-dependent coefficients | NLCSC 90% 제거율 달성 |
| **lag_auto_escalation** | 시작 Tier 1 | 100 frames | Auto-detect residual | Tier escalation 조건 검증 |
| **lag_history_ring_buffer** | 8-frame 윈도우 | 200 frames | FIFO ring buffer | 메모리 누수 없음, 정확한 상태 |

**파일 형식** (FSRF 예시):

```
synthetic/lag/FSRF_SAT27_001.raw        (uint16, 3072×3072, exposure frame)
synthetic/lag/FSRF_SAT27_002.raw        (uint16, 3072×3072, exposure frame)
...
synthetic/lag/FSRF_SAT27_dark_001.raw   (uint16, 3072×3072, dark follow-up)
synthetic/lag/FSRF_SAT27_dark_002.raw   (uint16, 3072×3072, dark follow-up)
...
synthetic/lag/metadata_FSRF_SAT27.json
  {
    "sequence_type": "FSRF",
    "exposure_frames": 200,
    "dark_frames": 200,
    "saturation_percent": 27,
    "lti_parameters": {
      "_note": "a_n은 감쇠 속도 [프레임^-1], b_n은 래그 계수 (무단위). PRD REQ-LAG-001 기준.",
      "a_n_per_frame": [2.5e-3, 2.1e-2, 1.6e-1, 7.6e-1],
      "b_n_fraction":  [7.1e-6, 1.1e-4, 1.7e-3, 1.8e-2],
      "_example_tau1_frames": "1/a_1 = 400 프레임 (15fps → 26.7초)",
      "detector_model": "Varex 4030CB, 15fps, 27% saturation"
    }
  }
```

**합격 기준**:
- 1st frame lag < 0.3% (Tier 1)
- 50th frame lag < 0.01% (Tier 1+)
- NLCSC 90% 제거율 (Tier 3)
- 메모리 누수 0 바이트 (200프레임 배치)

#### 4.4.3 실제 영상 데이터 명세

**출처**: IAP-CALIB-001 **섹션 6.5** (Lag/Ghost 측정용 영상)

> **[교차검증 수정 1]** 포화도 수준: PRD 기준 9개 수준은 2%, 5%, 10%, 20%, 30%, 50%, 70%, 80%, 92% (REQ-LAG-005)  
> **[교차검증 수정 2]** RSRF 프레임: N_pre=50 dark + N_post=200 = 250 프레임 (200+200 아님)

| 조건 | FSRF (pre+post) | RSRF (pre+post) | 취득 조건 |
|------|------|------|---------|
| 포화도 2% | 200+200=400 | 50+200=250 | 최저 신호 |
| 포화도 5% | 200+200=400 | 50+200=250 | 저신호 하한 |
| 포화도 10% | 200+200=400 | 50+200=250 | 저신호 |
| 포화도 20% | 200+200=400 | 50+200=250 | - |
| 포화도 30% | 200+200=400 | 50+200=250 | 중간 신호 |
| 포화도 50% | 200+200=400 | 50+200=250 | 임상 기본값 |
| 포화도 70% | 200+200=400 | 50+200=250 | 고신호 |
| 포화도 80% | 200+200=400 | 50+200=250 | - |
| 포화도 92% | 200+200=400 | 50+200=250 | 최고 신호 |

**파일 명명**:
- `real/lag/FSRF_SAT27_EXP_001.raw` ~ `.raw.200` (노출 프레임)
- `real/lag/FSRF_SAT27_DARK_001.raw` ~ `.raw.200` (어두운 추적)
- `real/lag/metadata_FSRF_SAT27.json`

#### 4.4.4 Ground Truth 형식

```json
{
  "test_case": "lag_tier3_nlcsc_9levels",
  "lti_parameters": {
    "n_exponentials": 4,
    "_note": "a_n = 감쇠 속도 [프레임^-1], b_n = 래그 계수 (무단위). tau_ms 아님!",
    "a_n_per_frame": [2.5e-3, 2.1e-2, 1.6e-1, 7.6e-1],
    "b_n_fraction": [7.1e-6, 1.1e-4, 1.7e-3, 1.8e-2],
    "detector_model": "Varex 4030CB"
  },
  "nlcsc_parameters": {
    "saturation_levels_percent": [2, 10, 20, 30, 40, 50, 75, 90, 92],
    "signal_dependent": true
  },
  "expected_output": {
    "lag_removal_rate_tier1_percent": 70,
    "lag_removal_rate_tier3_percent": 90,
    "1st_frame_lag_percent": 0.3,
    "50th_frame_lag_percent": 0.01
  }
}
```

#### 4.4.5 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-013 | 3단계 고스트 보정 및 자동 계층화 |
| SRS-CALIB-FUNC-014 | 노출 이력 링 버퍼 관리 |

---

### 4.5 Temperature Compensation 테스트 데이터

#### 4.5.1 테스트 목적

- 지수 온도 모델 (`I_dark(T) = I0 × exp(-Eg/2kB × T)`) 정확성
- 온도 센서 통합 (NTC 측정값)
- 이중선형 보간 (온도 × PREP 시간)
- Bypass 조건 (±2°C 공차, 센서 불가용)

#### 4.5.2 합성 데이터 명세

**Dataset**: `synthetic/temperature/`

| 테스트 케이스 | 온도 범위 | PREP 시간 | Ground Truth |
|-------------|---------|------|-------------|
| **temp_exponential_model** | 20, 25, 30, 35°C | 10s | 지수 곡선 추종 (E_g=1.12 eV) |
| **temp_interp_2pt** | 20°C와 30°C 사이 | 10s | 25°C에서 쌍선형 보간 |
| **temp_interp_4pt** | 20, 25, 30, 35°C | 1s, 5s, 10s, 30s | 이중선형 (온도 × PREP 초 단위) |
| **temp_bypass_within_tolerance** | nominal=25°C, actual=26°C | - | Bypass (±2°C 허용) |
| **temp_bypass_sensor_unavail** | - | - | Bypass, nominal 25°C 사용 |
| **temp_drift_long_term** | 25°C + 0.1°C/hour drift | 4시간 | 드리프트 추적 정확성 |

**파일 형식**:
```
synthetic/temperature/TEMP_EXP_20C_darkframe.raw    (uint16, ground truth)
synthetic/temperature/TEMP_EXP_25C_darkframe.raw    (uint16)
synthetic/temperature/TEMP_EXP_30C_darkframe.raw    (uint16)
synthetic/temperature/TEMP_EXP_35C_darkframe.raw    (uint16)
synthetic/temperature/metadata_TEMP_EXP.json
  {
    "test_case": "temp_exponential_model",
    "bandgap_energy_ev": 1.12,
    "boltzmann_constant_ev_k": 8.617e-5,
    "temperatures_celsius": [20, 25, 30, 35],
    "expected_dark_levels": [100, 125, 156, 195]
  }
```

**합격 기준**:
- 온도 보간 RMSE < 2 ADU
- 지수 모델 R² > 0.995
- Bypass 조건 정확한 적용 (±2°C)

#### 4.5.3 실제 영상 데이터 명세

**출처**: IAP-CALIB-001 **섹션 6.1** (Dark Frame 취득 — 온도 가변 조건)

| 조건 | 최소 수량 | 취득 조건 |
|------|--------|---------|
| 온도 스캔 (20-35°C) | 각 온도 10프레임 | ±0.1°C 센서 정확도 |
| 장기 드리프트 (4시간) | 50프레임 | 1시간당 1프레임 샘플링 |
| 센서 보정 (NTC vs RTD) | 20프레임 | 기준 온도계 병렬 측정 |

#### 4.5.4 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-008 | 온도 보상 지수 모델 |

---

### 4.6 Nonlinearity Correction 테스트 데이터

#### 4.6.1 테스트 목적

- 검출기 응답 곡선 선형화 (비선형성 LUT/다항식)
- 다항식 차수별 정확성 (degree 2-5)
- Multi-gain 비선형성 (에너지 의존성)
- Bypass 조건 (detector.linear = true)

#### 4.6.2 합성 데이터 명세

**Dataset**: `synthetic/nonlinearity/`

| 테스트 케이스 | 비선형성 모델 | LUT 크기 | Ground Truth |
|-------------|------------|---------|-------------|
| **nonlin_quadratic** | I_nonlin = 1.2×I - 0.001×I² | 256 entries | R² > 0.999 after correction |
| **nonlin_cubic** | I_nonlin = 0.9×I + 0.0005×I³ | 256 entries | - |
| **nonlin_poly_degree3** | 3차 다항식 fitting | LUT 또는 poly | - |
| **nonlin_poly_degree5** | 5차 다항식 fitting | LUT 또는 poly | - |
| **nonlin_multi_gain_2energy** | E1, E2에서 각각 다른 곡선 | 각 256 entries | 에너지별 선택 검증 |
| **nonlin_step_wedge** | 7-10 강도 수준 wedge | 실제 wedge 스캔 | 선형성 검증 |

**파일 형식**:
```
synthetic/nonlinearity/NONLIN_QUAD_clean.raw       (float32, 3072×3072)
synthetic/nonlinearity/NONLIN_QUAD_degraded.raw    (float32, 비선형성 추가)
synthetic/nonlinearity/NONLIN_QUAD_lut.raw         (uint16, 256 entries)
synthetic/nonlinearity/metadata_NONLIN_QUAD.json
```

**합격 기준**:
- 보정 후 R² > 0.999
- 보정 후 잔차 RMSE < 1% 신호

#### 4.6.3 실제 영상 데이터 명세

**출처**: IAP-CALIB-001 **섹션 6.4** (비선형성 측정용 영상)

| 조건 | 최소 수량 | 취득 조건 |
|------|--------|---------|
| 단색 X-ray flat-field (7-10 노출 수준) | 70프레임 | 5%-90% saturation |
| Multi-energy flat-field (2 에너지) | 140프레임 | 에너지당 70프레임 |

#### 4.6.4 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-006 | 비선형성 보정 (LUT/다항식) |

---

### 4.7 Binning Correction 테스트 데이터

#### 4.7.1 테스트 목적

- Binning 모드 인식 (1×1, 2×2, 4×4)
- 게인 재정규화 (binning_factor²로 나눔)
- No-op 검증 (1×1 = bypass)

#### 4.7.2 합성 데이터 명세

**Dataset**: `synthetic/binning/`

| 테스트 케이스 | Binning 모드 | Ground Truth |
|-------------|---------|-------------|
| **binning_1x1_native** | 1×1 (원본) | Bypass (no correction) |
| **binning_2x2_gain_factor** | 2×2 | G_binned = G_native / 4 |
| **binning_4x4_gain_factor** | 4×4 | G_binned = G_native / 16 |
| **binning_2x2_summing_accuracy** | 2×2 | 4-pixel summing 정확성 |

**파일 형식**:
```
synthetic/binning/NATIVE_1x1_3072x3072.raw  (float32)
synthetic/binning/BINNED_2x2_1536x1536.raw  (float32)
synthetic/binning/BINNED_4x4_768x768.raw    (float32)
synthetic/binning/metadata_BINNING.json
```

**합격 기준**:
- 2×2 binning: σ 감소 비율 ≈ 2 (binning_factor)
- 4×4 binning: σ 감소 비율 ≈ 4

#### 4.7.3 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-012 | Binning 모드 보정 |

---

### 4.8 Readout Artifact Validation 테스트 데이터

#### 4.8.1 테스트 목적

- Stuck rows/columns 검출
- ADC 포화 감지
- Dropped lines 감지
- 비변경 플래그 설정 검증

#### 4.8.2 합성 데이터 명세

**Dataset**: `synthetic/readout/`

| 테스트 케이스 | 아티팩트 유형 | 심각도 | Ground Truth |
|-------------|------------|-------|-------------|
| **readout_stuck_row** | Stuck row (constant value) | 1-5 rows | Detection + alert |
| **readout_stuck_column** | Stuck column | 1-5 columns | Detection + alert |
| **readout_adc_saturation** | ADC clipping | 5-10% pixels | Detection + warning |
| **readout_dropped_line** | Missing scanline | 1-3 lines | Detection + flag |

**합격 기준**:
- 모든 artefact 감지율 > 99%
- 거짓 양성 < 1%

#### 4.8.3 관련 요구사항

| 요구사항 ID | 검증 항목 |
|-----------|---------|
| SRS-CALIB-FUNC-001 (Stage 0.5) | 리드아웃 아티팩트 검증 |

---

## 5. 합성 데이터 생성 방법론

### 5.1 노이즈 모델

#### 5.1.1 어두운 프레임 노이즈

```
I_dark_simulated(x,y) = I_dark_dc(x,y) + N_gaussian(σ_dark)

여기서:
- I_dark_dc(x,y): 공간 변화 DC 오프셋 (저주파 그래디언트)
- N_gaussian(σ_dark): Gaussian 노이즈, σ_dark = 3-5 ADU
- 공간 변화: 3-5 ADU/프레임 (1시간당 온도 드리프트 시뮬레이션)
```

**Python 생성 코드 스니펫**:
```python
import numpy as np

def generate_dark_frame(height=3072, width=3072, dc_level=100, sigma_dark=4.0, 
                        spatial_gradient=0.01):
    # DC offset with spatial variation
    xx, yy = np.meshgrid(np.arange(width), np.arange(height))
    dc_spatial = dc_level + spatial_gradient * xx
    
    # Gaussian noise
    noise = np.random.normal(0, sigma_dark, (height, width))
    
    # Combine
    dark = dc_spatial + noise
    dark = np.clip(dark, 0, 65535).astype(np.uint16)
    return dark
```

#### 5.1.2 Flatfield 노이즈

```
I_flatfield = I_true × G(x,y) + N(σ)

여기서:
- I_true: Ground truth uniform intensity
- G(x,y): Pixel gain (FPN), 정규분포 N(1.0, σ_fpn)
- N(σ): Shot noise, σ = sqrt(I_true) [Poisson]
```

### 5.2 결함 주입 방법

#### 5.2.1 Dead Pixel 주입

```
1. Random 위치 선택 (원하는 비율에 따라)
2. 선택된 픽셀 = 0 (모든 intensity level에서)
3. 위치 기록 (BPM 생성)
```

#### 5.2.2 Hot Pixel 주입

```
1. Random 위치 선택
2. 선택된 픽셀 = 2×intensity (또는 포화도 clipped)
3. 위치 기록
```

#### 5.2.3 Stuck Row/Column 주입

```
1. Row/column index 선택 (예: row 1500)
2. 선택된 row 전체 = constant value (예: 32000)
3. 위치 기록
```

#### 5.2.4 Noisy Pixel 주입 (SNR-based)

```
1. Random 위치 선택 (0.1-1.0% 비율)
2. 해당 픽셀 coefficient of variation (CV) 증가:
   σ_pixel = mean_pixel × (1 + noise_factor)
3. 10 프레임 반복 측정에서 SNR < 5 dB 검증
4. 위치 및 noisy 플래그 기록
```

### 5.3 Ground Truth 생성 및 보관

#### 5.3.1 Ground Truth 저장 전략

```
synthetic/[algorithm]/
├── [testcase]_true.raw         # "Clean" signal (reference)
├── [testcase]_degraded.raw     # Signal with artifacts
├── [testcase]_parameters.json  # 생성 파라미터
└── [testcase]_expected_output.json  # 예상 보정 결과
```

#### 5.3.2 Ground Truth JSON 스키마

```json
{
  "test_case_id": "offset_uniform_5ADU",
  "algorithm": "offset_correction",
  "generated_timestamp": "2026-04-14T10:00:00Z",
  "image_parameters": {
    "height": 3072,
    "width": 3072,
    "bit_depth": 16,
    "format": "uint16"
  },
  "artifact_parameters": {
    "dark_level_adu": 5,
    "spatial_variation": false,
    "temperature_dependent": false
  },
  "expected_output": {
    "mean_residual_adu": 0.0,
    "std_residual_adu": 0.1,
    "clamped_pixels_percent": 0.0
  },
  "verification_method": "pixel_wise_comparison",
  "acceptance_tolerance": {
    "mean_error_adu": 1.0,
    "std_error_adu": 0.5,
    "clamped_pixel_count": 0
  }
}
```

#### 5.3.3 Ground Truth 검증

모든 합성 데이터 생성 후:

1. **데이터 무결성**: CRC-32 체크섬 계산 및 저장
2. **범위 검증**: min/max/mean/std 통계 기록
3. **시각적 검증**: Histogram 검토 (자동)
4. **문서화**: JSON 메타데이터 생성

---

## 6. 실제 영상 데이터 요건

### 6.1 최소 수량 요구사항

| 알고리즘 | 어두운 프레임 | Flatfield 프레임 | 특수 시퀀스 | 총 프레임 수 |
|---------|-------------|-----------------|-----------|-----------|
| Offset | 40 (4 temp × 10) | - | - | 40 |
| Gain | - | 40 (4 sat × 10) | - | 40 |
| Defect | 30 (BPM 생성용) | 30 (검증용) | - | 60 |
| Lag | - | - | FSRF 400 + RSRF 250 | 650 |
| Temperature | 40 (temp sweep) | - | 50 (시간 시리즈) | 90 |
| Nonlinearity | - | 70 (7 level × 10) | - | 70 |
| Binning | - | 40 (1×1, 2×2, 4×4) | - | 40 |
| Readout | 20 (known artefact) | 20 | - | 40 |
| **합계** | | | | **1,030** |

### 6.2 취득 조건 참조

모든 실제 데이터는 **IAP-CALIB-001** (Image Acquisition Protocol - Calibration) 다음 섹션을 따릅니다:

| IAP 섹션 | 제목 | 적용 대상 |
|------|------|---------|
| **5** | 일반 취득 전 조건 (General Prerequisites) | 모든 데이터 |
| **6.1** | Dark Frame 취득 | Offset correction, Temperature |
| **6.2** | Flat-field Frame 취득 | Gain correction, Nonlinearity, Binning |
| **6.3** | BPM 생성용 영상 취득 | Defect pixel correction |
| **6.4** | 비선형성 측정용 영상 | Nonlinearity correction |
| **6.5** | Lag/Ghost 측정용 영상 | Lag/Ghost correction |
| *(별도 취득 없음)* | Readout Artifact — 실시간 전자회로 보정 | Dark/Flat-field 재사용 (Section 6.1/6.2) |

> **[교차검증 수정]** TDS 초안의 "섹션 3.x" 번호는 IAP 문서 구조 (섹션 6.x)와 불일치했음. 전면 수정 완료. Readout Artifact는 IAP 별도 섹션 없음 — 앰프 오프셋/게인이 실시간 전자회로 보정이므로 특수 취득 영상 불필요 (기존 Dark/Flat-field 재사용).

### 6.3 데이터 익명화 요건

| 데이터 유형 | 익명화 필요 | 사유 |
|-----------|----------|------|
| Dark frames | 불필요 | 환자 정보 없음 |
| Flatfield frames | 불필요 | 환자 정보 없음 |
| Ghost/Lag sequences | 불필요 | 환자 정보 없음 |
| Clinical images (if any) | **필수** | 환자 개인정보 보호 |

**익명화 방법** (필요한 경우):
- Burned-in metadata 제거 (OCR 확인)
- DICOM tags 제거 또는 masking
- Histogram stretching (환자 식별 불가하도록)

### 6.4 파일 명명 규칙 및 메타데이터

**파일 명명**:
```
real/[domain]/[DOMAIN]_[CONDITION]_[FRAMENUM].raw

예:
real/dark_frames/DARK_T025_PREP10s_F001.raw
real/flatfield_frames/FF_SAT60_SID1000_F010.raw
real/lag/FSRF_SAT27_EXP_001.raw
```

**메타데이터 JSON**:
```json
{
  "file": "DARK_T025_PREP100_F001.raw",
  "acquisition_date": "2026-04-14",
  "detector_model": "Varex XRD 4343N",
  "detector_temperature_celsius": 25.0,
  "prep_time_s": 10,
  "exposure_time_ms": 0,
  "source_kvp": "off",
  "source_mas": "off",
  "image_dimensions": [3072, 3072],
  "bit_depth": 16,
  "format": "raw_binary_little_endian",
  "checksum_crc32": "0x12345678",
  "notes": "Standard calibration dark frame at nominal 25C"
}
```

---

## 7. Golden Reference 데이터 관리

### 7.1 Golden Reference 생성 절차

1. **알고리즘 v1.0 최종화**: 모든 9개 알고리즘 구현 완료 및 코드 리뷰 통과

2. **기준 영상 처리**: 합성 + 실제 모든 테스트 데이터셋에 대해 알고리즘 실행

3. **출력 검증**:
   - 수치 범위 확인 (underflow/overflow 없음)
   - NaN/Inf 체크
   - Histogram 통계 기록

4. **승인 프로세스**:
   - QA 엔지니어가 출력 시각적 검사
   - 팀 회의에서 기술적 타당성 검증
   - 서명 및 버전 기록

5. **저장소에 Commit**:
   ```bash
   git add docs/calibration/golden_reference/
   git commit -m "Golden reference baseline v1.0 (algorithm v1.0.0)"
   git tag v1.0.0-golden
   ```

### 7.2 Golden Reference 버전 관리

```
golden_reference/
├── v1.0.0_algorithm/
│   ├── offset_uniform_5ADU_output.raw
│   ├── offset_uniform_5ADU_output.json
│   ├── gain_nonuniform_15pct_output.raw
│   ├── defect_mixed_0.5pct_output.raw
│   └── ...
├── v1.0.0_manifest.json
└── v1.1.0_algorithm/ (향후 버전)
    └── ...
```

**Manifest 스키마**:
```json
{
  "version": "1.0.0",
  "algorithm_commit": "abc123def456...",
  "generation_date": "2026-04-14",
  "generated_by": "XPE Calib Team",
  "approval_sign_off": "Senior QA Engineer",
  "approval_date": "2026-04-14",
  "test_cases": {
    "offset_uniform_5ADU": {
      "output_file": "offset_uniform_5ADU_output.raw",
      "checksum_crc32": "0x12345678",
      "mean_value": 1000.5,
      "std_value": 10.2
    },
    ...
  }
}
```

### 7.3 회귀 테스트 절차

새로운 알고리즘 버전이 생성될 때마다:

1. **이전 Golden Reference와 비교**:
   ```python
   output_new = run_algorithm(test_data)
   output_ref = load_golden_reference(test_case)
   
   ssim_score = structural_similarity(output_new, output_ref)
   if ssim_score > 0.999:
       print("PASS: Regression test OK")
   else:
       print("FAIL: Output diverged from reference")
   ```

2. **SSIM 임계값**:
   - SSIM > 0.999: Pass (정상 수렴)
   - 0.995 < SSIM ≤ 0.999: Manual review required
   - SSIM ≤ 0.995: Fail (알고리즘 재검토 필요)

3. **영향 분석**:
   - 어느 알고리즘이 변경되었는지 식별
   - 의도적 개선인지 버그인지 판단
   - 필요시 새로운 Golden Reference 생성

---

## 8. 데이터셋 디렉토리 구조

```
test_data/
├── calibration/
│   ├── README.md (dataset 개요)
│   ├── synthetic/
│   │   ├── dark_frames/
│   │   │   ├── offset_uniform_5ADU_true.raw
│   │   │   ├── offset_uniform_5ADU_degraded.raw
│   │   │   ├── offset_uniform_5ADU_darkmap.raw
│   │   │   ├── offset_nonuniform_spatial_*.raw
│   │   │   ├── offset_temp_interp_2pt_*.raw
│   │   │   └── metadata_OFFSET.json
│   │   │
│   │   ├── flatfield_frames/
│   │   │   ├── gain_uniform_true.raw
│   │   │   ├── gain_uniform_uint16.raw
│   │   │   ├── gain_uniform_gainmap.raw
│   │   │   ├── gain_nonuniform_10pct_*.raw
│   │   │   ├── gain_heel_effect_*.raw
│   │   │   └── metadata_GAIN.json
│   │   │
│   │   ├── defects/
│   │   │   ├── defect_dead_0.01pct_clean.raw
│   │   │   ├── defect_dead_0.01pct_degraded.raw
│   │   │   ├── defect_dead_0.01pct_defect_map.raw
│   │   │   ├── defect_mixed_0.5pct_*.raw
│   │   │   └── metadata_DEFECTS.json
│   │   │
│   │   ├── lag/
│   │   │   ├── FSRF_SAT27_EXP_001.raw ~ .raw.200
│   │   │   ├── FSRF_SAT27_DARK_001.raw ~ .raw.200
│   │   │   ├── RSRF_SAT27_DARK_001.raw ~ .raw.200
│   │   │   ├── RSRF_SAT27_EXP_001.raw ~ .raw.200
│   │   │   ├── lag_tier2_exposure_weighted_*.raw
│   │   │   ├── lag_tier3_nlcsc_*.raw
│   │   │   └── metadata_LAG.json
│   │   │
│   │   ├── temperature/
│   │   │   ├── TEMP_EXP_20C_darkframe.raw
│   │   │   ├── TEMP_EXP_25C_darkframe.raw
│   │   │   ├── TEMP_EXP_30C_darkframe.raw
│   │   │   ├── TEMP_EXP_35C_darkframe.raw
│   │   │   ├── temp_interp_4pt_*.raw
│   │   │   └── metadata_TEMPERATURE.json
│   │   │
│   │   ├── nonlinearity/
│   │   │   ├── NONLIN_QUAD_clean.raw
│   │   │   ├── NONLIN_QUAD_degraded.raw
│   │   │   ├── NONLIN_QUAD_lut.raw
│   │   │   ├── NONLIN_POLY_degree5_*.raw
│   │   │   └── metadata_NONLINEARITY.json
│   │   │
│   │   ├── binning/
│   │   │   ├── NATIVE_1x1_3072x3072.raw
│   │   │   ├── BINNED_2x2_1536x1536.raw
│   │   │   ├── BINNED_4x4_768x768.raw
│   │   │   └── metadata_BINNING.json
│   │   │
│   │   └── readout/
│   │       ├── readout_stuck_row_degraded.raw
│   │       ├── readout_adc_saturation_degraded.raw
│   │       └── metadata_READOUT.json
│   │
│   ├── real/
│   │   ├── dark_frames/
│   │   │   ├── DARK_T020_PREP100_F001.raw ~ F010.raw
│   │   │   ├── DARK_T025_PREP100_F001.raw ~ F010.raw
│   │   │   ├── DARK_T030_PREP100_F001.raw ~ F010.raw
│   │   │   ├── DARK_T035_PREP100_F001.raw ~ F010.raw
│   │   │   ├── metadata_T020_PREP100.json
│   │   │   ├── metadata_T025_PREP10s.json
│   │   │   ├── metadata_T030_PREP100.json
│   │   │   └── metadata_T035_PREP100.json
│   │   │
│   │   ├── flatfield_frames/
│   │   │   ├── FF_SAT40_SID1000_F001.raw ~ F010.raw
│   │   │   ├── FF_SAT60_SID1000_F001.raw ~ F010.raw
│   │   │   ├── FF_SAT40_SID1500_F001.raw ~ F010.raw
│   │   │   ├── FF_SAT60_SID1500_F001.raw ~ F010.raw
│   │   │   └── metadata_SAT*.json
│   │   │
│   │   ├── lag/
│   │   │   ├── FSRF_SAT02_EXP_001.raw ~ EXP_200.raw
│   │   │   ├── FSRF_SAT02_DARK_001.raw ~ DARK_200.raw
│   │   │   ├── FSRF_SAT10_EXP_001.raw ~ .raw.200
│   │   │   ├── ... (9개 포화도 수준 × 2 시퀀스)
│   │   │   └── metadata_FSRF_*.json (RSRF 포함)
│   │   │
│   │   ├── defects/
│   │   │   ├── BPM_REF.raw
│   │   │   ├── DARK_FOR_BPM_001.raw ~ F030.raw
│   │   │   ├── FF_FOR_BPM_001.raw ~ F030.raw
│   │   │   └── metadata_DEFECTS.json
│   │   │
│   │   └── (다른 알고리즘용 디렉토리)
│   │
│   ├── golden_reference/
│   │   ├── v1.0.0_algorithm/
│   │   │   ├── offset_uniform_5ADU_output.raw
│   │   │   ├── offset_uniform_5ADU_output.json
│   │   │   ├── gain_nonuniform_15pct_output.raw
│   │   │   ├── gain_nonuniform_15pct_output.json
│   │   │   ├── defect_mixed_0.5pct_output.raw
│   │   │   ├── lag_tier1_lti_fsrf_output.raw
│   │   │   └── (모든 테스트 케이스 출력)
│   │   │
│   │   └── v1.0.0_manifest.json
│   │       ├── version, approval, checksums
│   │       └── per-test-case metadata
│   │
│   └── DATASET_INDEX.md (이 디렉토리의 전체 카탈로그)
```

---

## 9. IEC 62304 추적성

### 9.1 요구사항 → 테스트 데이터 매핑

| SRS Requirement ID | 테스트 케이스 | 데이터 유형 | 파일 경로 |
|------------------|------------|---------|---------|
| SRS-CALIB-FUNC-001 | offset_uniform | synthetic | synthetic/dark_frames/ |
| SRS-CALIB-FUNC-002 | gain_range_validation | synthetic | synthetic/flatfield_frames/ |
| SRS-CALIB-FUNC-003 | defect_bpm_parsing | real | real/defects/ |
| SRS-CALIB-FUNC-004 | offset_temp_interp | synthetic + real | synthetic/dark_frames/, real/dark_frames/ |
| SRS-CALIB-FUNC-005 | gain_format_conversion | synthetic | synthetic/flatfield_frames/ |
| SRS-CALIB-FUNC-006 | nonlin_poly_degree5 | synthetic + real | synthetic/nonlinearity/, real/ |
| SRS-CALIB-FUNC-007 | defect_mixed_0.5pct | synthetic | synthetic/defects/ |
| SRS-CALIB-FUNC-008 | temp_exponential_model | synthetic | synthetic/temperature/ |
| SRS-CALIB-FUNC-009 | (expire check - code test) | N/A | N/A |
| SRS-CALIB-FUNC-010 | (runtime detection - code test) | N/A | N/A |
| SRS-CALIB-SAFE-001 | offset_mandatory + gain_mandatory | integration test | synthetic + real |
| SRS-CALIB-SAFE-002 | (expire enforce - code test) | N/A | N/A |
| SRS-CALIB-SAFE-005 | gain_overflow_protection | synthetic | synthetic/flatfield_frames/ |

### 9.2 테스트 결과 문서화

**테스트 실행 후**, 결과를 다음과 같이 기록:

```
test_results/
├── 2026-04-14_ALGORITHM_v1.0.0_TEST_RESULTS.md
│   ├── Test Execution Summary
│   ├── Per-Algorithm Results
│   │   ├── Offset Correction: PASS (10/10 test cases)
│   │   ├── Gain Correction: PASS (8/8 test cases)
│   │   ├── ...
│   └── Traceability Matrix (SRS → Results)
```

**예시 결과 문서**:
```markdown
# Test Execution Report - XPE Calibration v1.0.0
**Date**: 2026-04-14
**Tested By**: QA Engineer, XPE Team
**Result**: PASS (135/135 test cases passed)

## Offset Correction (SRS-CALIB-FUNC-004)
- offset_uniform_5ADU: **PASS** (RMSE 0.8 ADU < 1.0 tolerance)
- offset_temp_interp_2pt: **PASS** (Interp error 1.5 ADU < 2.0)
- offset_clamping_edge: **PASS** (0 clamped pixels verified)

## Gain Correction (SRS-CALIB-FUNC-005)
- gain_nonuniform_15pct: **PASS** (σ/μ = 0.98% < 1.0%)
- gain_format_conversion: **PASS** (float32 conversion exact)

...

## IEC 62304 Traceability
All test cases mapped to SRS requirements:
✓ SRS-CALIB-FUNC-001 through SRS-CALIB-FUNC-010: Verified
✓ SRS-CALIB-SAFE-001 through SRS-CALIB-SAFE-005: Verified
✓ SRS-CALIB-PERF-001 through SRS-CALIB-PERF-003: Verified

## Approval
QA Sign-Off: __________________ Date: __________
```

---

## 9.3 Local Raw/Calibration E2E Fixture Addendum (2026-04-16)

This addendum links the local fixture staging area to the automated preprocessing proof protocol.

Normative protocol:

- `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`

Local fixture root:

- `tests/test_data/calibration_cases`

Current local cases:

| Case ID | Purpose | Required automated checks |
|---|---|---|
| `aed_shock_had1717mc` | AED/shock and dark-calibration workflow checks | fixture scan, calibration pairing, raw preservation, exploratory dark/gain effect |
| `auradr_release_line_trg` | line-trigger calibration and image pairing | fixture scan, calibration pairing, gain semantics classification, mismatch negative test |
| `corner_blemish_17a06b1` | defect/blemish comparison | fixture scan, BPM/defect metric extraction, before/after ROI comparison |

Required E2E modes:

| Mode | Dataset source | Expected evidence |
|---|---|---|
| `PRE-E2E-0` | all local cases | file size, SHA-256, inferred dimensions, Git ignore proof |
| `PRE-E2E-1` | generated synthetic cases | exact oracle output for offset/gain/nonlinearity/defect/lag |
| `PRE-E2E-2` | local real cases | detector-domain calibration-effect metrics |
| `PRE-E2E-3` | cases with known outputs such as `*_oc.raw` | RMSE/PSNR reference comparison after semantics confirmation |
| `PRE-E2E-4` | Test GUI or backend driver | same report schema as native E2E run |
| `PRE-E2E-5` | intentionally mismatched image/calibration cases | hard failure or visible warning; no silent correction |

Minimum report metrics:

- `DarkBias`, `DSNU_ADU`, `DarkReduction_dB`, `ClampRate`
- `PRNU_CV`, `FlatResidualPct`, `FPN_Reduction_dB`, `LineArtifactScore`
- `DefectRecall`, `DefectFPR`, `DefectResidualADU`, `GoodPixelDeltaP99`
- `LagResidualPct`, `GhostRemovalPct`
- `InputPreserved`, `NaNInfCount`, `PipelineTimeMs`, `PeakMemoryMB`
- `Calibration Effect Score`

Raw payload policy:

- `.raw` and `.dcm` payloads under `tests/test_data` remain local-only.
- Git may track fixture README, `.gitignore`, manifests, report schemas, and expected-output hashes.
- CI artifacts may include JSON/Markdown metric summaries, but must not upload protected patient or raw detector payloads unless explicitly anonymized and approved.

---

## 10. 참고문헌

### 표준 및 규제

| 참조 | 제목 | 관련 섹션 |
|------|------|---------|
| IEC 62304:2006+A1:2015 | Medical Device Software Lifecycle | 5.6.3 (Test Design) |
| IEC 62220-1-1:2015 | Measurement of digital radiography systems - Part 1-1: Determination of the detective quantum efficiency | Gain/Offset quality |
| ISO/IEC/IEEE 29119:2013 | Software and systems engineering - Test processes | Test case design |
| 21 CFR Part 11 | Electronic Records; Electronic Signatures | Data integrity, calibration |

### 문서

| 문서 | 경로 | 용도 |
|------|------|------|
| XPE 캘리브레이션 SRS | docs/calibration/SRS-CALIB-001 | 요구사항 참조 |
| 이미지 취득 프로토콜 | docs/calibration/IAP-CALIB-001 | 실제 데이터 취득 조건 |
| 알고리즘 사양 (규범) | .moai/specs/xpe-algorithm-spec-deepsync.md | 알고리즘 상세 정의 |
| 고스트 보정 SRS | docs/ghost-correction/srs_ghost_correction.md | Lag 보정 상세사항 |

### 참고 논문

| 인용 | 저자 | 연도 | 관련 주제 |
|------|------|------|---------|
| Starman et al. | NLCSC Lag Correction | 2012 | Tier 3 고스트 모델 |
| Pang et al. | Lag vs. Ghosting | 2006 | Lag 물리 모델 |
| Ranger et al. | SNR Calibration | 2014 | 드리프트 검출 임계값 |
| Wang | Heel Effect Correction | 2013 | Duo-SID 게인 보상 |
| Jeon et al. | Deep Learning Defect | 2021 | FixPix MLP 기반 |

---

**문서 끝**

*버전 1.0.0 | 최종 수정: 2026-04-14 | 승인 대기*
