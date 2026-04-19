# cyan_test: Cyan 검출기 생산 캘리브레이션 + 임상 검증 데이터셋

**데이터셋 ID**: `cyan_prod_calib_clinical`  
**수집 기간**: 2026-04-02  
**검출기 타입**: Cyan 변형 X-ray FPD (3072×3072)  
**용도**: 생산 환경 캘리브레이션 워크플로우 및 E2E 임상 검증

---

## 목차

1. [개요](#1-개요)
2. [파일 목록 및 구성](#2-파일-목록-및-구성)
3. [파일 형식 명세](#3-파일-형식-명세)
4. [다중 게인 CalSet 다항식 피팅](#4-다중-게인-calset-다항식-피팅)
5. [임상 이미지 E2E 검증](#5-임상-이미지-e2e-검증)
6. [DICOM 출력 검증](#6-dicom-출력-검증)
7. [검증 체크리스트](#7-검증-체크리스트)

---

## 1. 개요

cyan_test 데이터셋은 **생산 환경에서의 완전한 캘리브레이션 워크플로우**와 **임상 이미지 전처리 검증**을 대표합니다.

### 주요 특징

- **완전한 다크 프레임 시리즈**: 시간 드리프트 분석
- **다중 게인 CalSet 세트**: 5개 선량 레벨의 게인 다항식 피팅
- **실제 임상 영상**: 흉부, 발 팬텀 이미지
- **DICOM 출력**: 의료기기 표준 준수
- **신호 천천히 변하는 드리프트 시뮬레이션**: 시스템 안정성 평가

### 사용 사례

| 사용 사례 | 설명 |
|--------|------|
| **생산 캘리브레이션 파이프라인** | Dark 수집 → Bright 수집 → CalSet 다항식 피팅 |
| **다중 게인 보정** | 5개 선량 조건에서의 에너지 의존 게인 모델 |
| **임상 이미지 E2E** | 실제 방사선 촬영 이미지 전처리 및 출력 검증 |
| **DICOM 호환성** | 임상 PACS 시스템 통합 테스트 |
| **시간 드리프트 보정** | 온도 변화에 따른 다크 신호 드리프트 검증 |

---

## 2. 파일 목록 및 구성

### 2.1 다크 프레임 시리즈 (시간 드리프트 분석)

| 파일명 | 크기 | 수집시간 간격 | 설명 |
|--------|------|-------------|------|
| `Dark_07.raw` | 18.9MB | T₀ | 기준 다크 프레임 (기본 선량) |
| `Dark_08.raw` | 18.9MB | T₀ + 5min | 2번째 다크 프레임 |
| `Dark_09.raw` | 18.9MB | T₀ + 10min | 3번째 다크 프레임 |
| `Dark_10.raw` | 18.9MB | T₀ + 15min | 4번째 다크 프레임 |
| `Dark_11.raw` | 18.9MB | T₀ + 20min | 5번째 다크 프레임 |
| `Dark_12.raw` | 18.9MB | T₀ + 25min | 6번째 다크 프레임 |
| `Dark_13.raw` | 18.9MB | T₀ + 30min | 7번째 다크 프레임 |
| `Dark_14.raw` | 18.9MB | T₀ + 35min | 8번째 다크 프레임 |
| `Dark_15.raw` | 18.9MB | T₀ + 40min | 9번째 다크 프레임 |
| `Dark_16.raw` | 18.9MB | T₀ + 45min | 10번째 다크 프레임 |

**용도:**
- 어두운 신호 시간 드리프트 분석
- 온도 보정 모델 검증 (SRS-CALIB-FUNC-008)
- 다크 신호의 장기 안정성 평가

### 2.2 다크 차분 프레임 (미분 분석)

| 파일명 | 포함된 데이터 | 설명 |
|--------|------------|------|
| `Dark_14-Dark_13.raw` | Δ(T₀ + 35min - T₀ + 30min) | 시간 차분 영상 (5분 드리프트) |
| `Dark_15-Dark_14.raw` | Δ(T₀ + 40min - T₀ + 35min) | 시간 차분 영상 (5분 드리프트) |
| `Dark_16-Dark_15.raw` | Δ(T₀ + 45min - T₀ + 40min) | 시간 차분 영상 (5분 드리프트) |

**용도:**
- 프레임 간 변화량 추적
- 드리프트 속도 계산
- 온도 보정 지수함수 모델의 tau 값 추정

### 2.3 마스터 캘리브레이션 기준값

| 파일명 | 크기 | 설명 |
|--------|------|------|
| `MasterDark.raw` | 18.9MB | 10개 다크 프레임의 평균 (통계적 기준) |
| `MasterBright.raw` | 18.9MB | 20개 밝음 프레임의 평균 (기본 평탄도) |

**계산:**
```
MasterDark = mean(Dark_07 ~ Dark_16)
MasterBright = mean(Bright_17 ~ Bright_36)
```

### 2.4 밝은(평탄화) 프레임 시리즈

| 파일명 | 크기 | 용도 |
|--------|------|------|
| `Bright_17.raw` ~ `Bright_36.raw` | 18.9MB × 20 | 기본 게인 맵 계산 (일정 선량, 단일 게인) |

**특징:**
- 단일 선량 레벨에서 20개 프레임 수집
- 통계적 안정화 (DSNU 감소)
- 통상적인 생산 절차

### 2.5 다중 게인 CalSet 시리즈 (에너지/선량 의존)

| 파일명 | ADU 레벨 (선량) | 설명 |
|--------|----------------|------|
| `CalSet_14037.raw` | 14,037 | 선량 레벨 1 (약 20% 선량) |
| `CalSet_17285.raw` | 17,285 | 선량 레벨 2 (약 35% 선량) |
| `CalSet_20985.raw` | 20,985 | 선량 레벨 3 (약 50% 선량) |
| `CalSet_30868.raw` | 30,868 | 선량 레벨 4 (약 75% 선량) |
| `CalSet_42677.raw` | 42,677 | 선량 레벨 5 (약 100% 선량) |

**파일명 규칙:**
- `CalSet_{ADU_value}.raw`
- 파일명의 숫자 = 해당 프레임의 평균 신호 (ADU)
- 서로 다른 선량 조건에서 수집

**용도:**
- 다중 게인 다항식 피팅
- 에너지 의존 게인 모델 구축
- 비선형 응답 특성화

---

## 3. 파일 형식 명세

### 3.1 Raw 파일 형식

**크기:** 3072 × 3072 × 2바이트 (uint16) = **18,874,368 바이트** ≈ 18.9MB

**데이터 구조:**
```
Offset  | Content                    | Size (bytes)
0       | Row 0, Col 0-3071 (uint16) | 6,144
6,144   | Row 1, Col 0-3071 (uint16) | 6,144
...
18,868,224 | Row 3071, Col 0-3071 (uint16) | 6,144
```

**바이트 순서:** Little-endian

**값 범위:** 0 ~ 65,535 ADU

### 3.2 DICOM 출력 파일 형식

DICOM 파일은 전처리 후 생성되며, 다음 예제 파일들이 포함되어 있습니다:

| 파일명 | 설명 |
|--------|------|
| `Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00_result_raw.dcm` | 흉부 팬텀 원본 DICOM |
| `Bright_Foot phantom_50kv_200ma_5mas(SID_110)_04_result_raw.dcm` | 발 팬텀 DICOM |

**DICOM 태그:**
- **Patient ID**: "CYAN-TEST-PHANTOM"
- **Modality**: "DX" (Digital Radiography)
- **Bits Allocated**: 16
- **Bits Stored**: 16
- **High Bit**: 15
- **Pixel Representation**: 0 (Unsigned)

---

## 4. 다중 게인 CalSet 다항식 피팅

### 4.1 개념

Cyan 검출기는 **에너지 의존 게인** 특성을 가지고 있습니다. 다양한 선량 조건에서 수집된 CalSet 파일을 이용해 다항식으로 모델링합니다.

### 4.2 피팅 절차

```
Step 1: 모든 CalSet 파일 로드
  ├─ CalSet_14037.raw → Dark 보정 → mean_14037
  ├─ CalSet_17285.raw → Dark 보정 → mean_17285
  ├─ CalSet_20985.raw → Dark 보정 → mean_20985
  ├─ CalSet_30868.raw → Dark 보정 → mean_30868
  └─ CalSet_42677.raw → Dark 보정 → mean_42677

Step 2: 암전류 보정
  └─ I_corrected = CalSet_{i} - MasterDark.raw
  └─ mean_signal[i] = mean(I_corrected)

Step 3: (선량 vs 신호) 선택
  └─ X = [14037, 17285, 20985, 30868, 42677]  (선량, ADU)
  └─ Y = [mean_14037, mean_17285, ..., mean_42677]  (신호)

Step 4: 다항식 피팅 (Least Squares)
  └─ 2차 다항식: Y = c₀ + c₁×X + c₂×X²
  └─ 또는 3차: Y = c₀ + c₁×X + c₂×X² + c₃×X³
  
  Python:
    coeffs = np.polyfit(X, Y, deg=2)
    
  C++:
    // Eigen, GSL, Boost.Math 사용

Step 5: 피팅 품질 평가
  └─ R² (결정계수) > 0.99 (필수)
  └─ RMSE (평균제곱오차) < 2% × max(Y) (권장)
  └─ 잔차 플롯: 체계적 패턴 없어야 함

Step 6: 픽셀별 게인 맵 생성 (선택사항)
  └─ G(x,y,E) = poly(dark_corrected_signal[x,y], coeffs)
  └─ E = 에너지 또는 선량 (다항식 입력)
```

### 4.3 다항식 모델

```
선형 모델 (1차):
  Signal = c₀ + c₁ × Dose

이차 모델 (2차, 권장):
  Signal = c₀ + c₁ × Dose + c₂ × Dose²
  
  예시 계수:
    c₀ = 50 (기저 신호, offset)
    c₁ = 0.85 (선형 게인)
    c₂ = 0.001 (비선형 항)

삼차 모델 (3차, 고급):
  Signal = c₀ + c₁ × Dose + c₂ × Dose² + c₃ × Dose³
  
  주의: 과적합 위험 (5개 데이터점, 4개 계수)
        검증 셋으로 검증 필수
```

### 4.4 검증

| 조건 | 값 | 상태 |
|------|-----|------|
| **R²** | > 0.99 | ✓ 합격 |
| **RMSE** | < 2% | ✓ 합격 |
| **Max Residual** | < 3% | ✓ 합격 |
| **단조성** | 증가 함수 | ✓ 필수 |

---

## 5. 임상 이미지 E2E 검증

### 5.1 임상 데이터셋 구성

| 그룹 | 파일명 | 크기 | 촬영 조건 | 용도 |
|-----|---------|------|---------|------|
| **흉부 팬텀** | `Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00.raw` | 18.9MB | 75kV, 320mA, 25.6ms, SID=110cm | 폐 신호 |
| | `..._01.raw` | 18.9MB | 동일 조건 반복 | 재현성 |
| | `..._02.raw` | 18.9MB | 동일 조건 반복 | 재현성 |
| **발 팬텀** | `Bright_Foot phantom_50kv_200ma_5mas(SID_110)_03.raw` | 18.9MB | 50kV, 200mA, 5ms, SID=110cm | 뼈 신호 |
| | `..._55kv_200ma_8mas(SID_110)_03.raw` | 18.9MB | 55kV, 200mA, 8ms, SID=110cm | 다른 에너지 |

### 5.2 전처리 파이프라인

```
입력: Bright_Chest_phantom_75kv_320ma_25.6(SID_110)_00.raw (uint16)
↓
Step 1: 암전류 보정
  └─ I_dark_corr = raw_image - MasterDark.raw
  └─ 결과: uint16 (음수 클램핑)
↓
Step 2: 다중 게인 보정 (CalSet 다항식)
  └─ I_gain_corr = I_dark_corr × poly_gain(dose_level)
  └─ 결과: float32
↓
Step 3: 비선형성 보정 (LUT)
  └─ I_linear = LUT[I_gain_corr]
  └─ 결과: float32
↓
Step 4: 불량 픽셀 보정 (BPM)
  └─ I_bpm_corr = interpolate_bad_pixels(I_linear, BPM)
  └─ 결과: float32
↓
Step 5: 온도 보정 (선택사항)
  └─ I_temp_corr = I_bpm_corr × temp_compensation_factor
  └─ 결과: float32
↓
Step 6: 고스트/Lag 보정 (선택사항)
  └─ I_ghost_corr = deconvolve_lag(I_temp_corr, history_buffer)
  └─ 결과: float32
↓
출력: Bright_Chest_phantom_..._result.raw (float32, 32-bit)
또는: Bright_Chest_phantom_..._result_raw.dcm (DICOM)
```

### 5.3 품질 검증 메트릭

| 메트릭 | 권장값 | 임상 기준 |
|--------|-------|---------|
| **PSNR** (Signal-to-Noise Ratio, dB) | > 40 dB | ≥ 35 dB |
| **SSIM** (Structural Similarity) | > 0.95 | ≥ 0.90 |
| **RMS Error** (vs. 황금 기준) | < 1% | < 2% |
| **Clipping Rate** (포화율) | < 0.1% | < 0.5% |
| **Artifact Score** (그리드/줄무늬) | < 5% | < 10% |

---

## 6. DICOM 출력 검증

### 6.1 DICOM 형식 요구사항

```
DICOM 파일명: {OriginalImageName}_result_raw.dcm

DICOM 헤더 필드:
  (0008,0012) InstanceCreationDate   = 2026-04-02
  (0008,0013) InstanceCreationTime   = HHMMSS
  (0008,0016) SOPClassUID            = "1.2.840.10008.5.1.4.1.1.1" (CR)
  (0008,0018) SOPInstanceUID         = {자동 생성}
  (0010,0010) PatientName            = "CYAN-TEST"
  (0010,0020) PatientID              = "CYAN-PHANTOM-001"
  (0012,0062) PatientIdentityRemoved = "YES"
  (0018,1030) ProtocolName           = "Chest/Foot Phantom"
  (0020,000D) StudyInstanceUID       = {자동 생성}
  (0020,000E) SeriesInstanceUID      = {자동 생성}
  (0028,0010) Rows                   = 3072
  (0028,0011) Columns                = 3072
  (0028,0100) BitsAllocated          = 16 (또는 32)
  (0028,0101) BitsStored             = 16 (또는 32)
  (0028,0102) HighBit                = 15 (또는 31)
  (0028,0103) PixelRepresentation    = 0 (Unsigned)
  (7FE0,0010) PixelData              = 전처리된 영상 데이터
```

### 6.2 DICOM 유효성 검증

```bash
# dcmvalidate 또는 pydicom 사용

# Python 예제
import pydicom

dcm = pydicom.dcmread("Bright_Chest_phantom_..._result_raw.dcm")

# 필수 필드 확인
assert dcm.Rows == 3072
assert dcm.Columns == 3072
assert dcm.BitsAllocated == 16 or dcm.BitsAllocated == 32
assert len(dcm.pixel_array) == 3072 * 3072

# 픽셀값 범위 확인 (float32 후)
assert dcm.pixel_array.min() >= 0.0
assert dcm.pixel_array.max() <= 1e8

print("✓ DICOM 유효성 검증 완료")
```

---

## 7. 검증 체크리스트

### 7.1 데이터 무결성

- [ ] **파일 크기 확인**
  - Dark_*.raw: 18,874,368 바이트 (10개)
  - Bright_*.raw: 18,874,368 바이트 (20개)
  - CalSet_*.raw: 18,874,368 바이트 (5개)
  - MasterDark.raw, MasterBright.raw: 18,874,368 바이트 각각
  - BPM.raw: 18,874,368 바이트

- [ ] **파일 체크섬** (SHA-256)
  ```bash
  sha256sum *.raw | tee checksums.txt
  ```

- [ ] **데이터 통계**
  - Dark 평균: 100~150 ADU (안정적)
  - Bright 평균: 500~5000 ADU (선량 의존)
  - CalSet 평균: 14k~43k ADU (파일명과 일치)

### 7.2 다크 시간 드리프트 분석

- [ ] **평균값 추이**
  ```
  Dark_07: 110 ADU
  Dark_08: 112 ADU (+1.8%)
  ...
  Dark_16: 125 ADU (+13.6%)
  ```
  - 기울기 계산: (Dark_16 - Dark_07) / 10 frames ≈ 1.5 ADU/frame
  - 지수 모델 적합: tau ≈ 10~20 min (온도 상수)

- [ ] **차분 분석**
  - Dark_14 - Dark_13: ~1.5 ADU (일관성)
  - Dark_15 - Dark_14: ~1.5 ADU (일관성)
  - Dark_16 - Dark_15: ~1.5 ADU (일관성)

### 7.3 다중 게인 CalSet 피팅

- [ ] **데이터 포인트**
  - 5개 CalSet 평균값 계산
  ```
  X (ADU):     [14037, 17285, 20985, 30868, 42677]
  Y (Signal):  [mean_14037, ..., mean_42677]
  ```

- [ ] **2차 다항식 피팅**
  ```
  Signal = c₀ + c₁ × ADU + c₂ × ADU²
  
  예상 결과:
    R² > 0.99 ✓
    RMSE < 2% ✓
    계수 c₂ > 0 (비선형 증가) ✓
  ```

- [ ] **외삽값 검증**
  - 새로운 선량 레벨(예: 25000 ADU)에서 게인 예측
  - 재현성 확인: ± 1% 이내

### 7.4 임상 이미지 E2E

- [ ] **전처리 실행**
  ```
  xpe-preprocess-cli \
    --dark MasterDark.raw \
    --bright MasterBright.raw \
    --calset CalSet_14037.raw CalSet_17285.raw ... \
    --bpm BPM.raw \
    --input Bright_Chest_phantom_75kv_320ma_25.6\(SID_110\)_00.raw \
    --output Bright_Chest_phantom_75kv_320ma_25.6\(SID_110\)_00_result.raw \
    --report chest_phantom_report.json
  ```

- [ ] **출력 파일 검증**
  - float32 형식 확인
  - 크기: 3072 × 3072 × 4 bytes = 37.7MB
  - 범위: 0.0 ~ 100000.0 (overflow 없음)

- [ ] **품질 메트릭**
  - PSNR > 40 dB ✓
  - SSIM > 0.95 ✓
  - LineArtifactScore < 5% ✓
  - Clipping < 0.1% ✓

- [ ] **DICOM 출력 (선택사항)**
  ```
  xpe-preprocess-cli \
    ... --output-dicom Bright_Chest_phantom_75kv_..._result_raw.dcm
  ```
  - DICOM 유효성 검증 실행
  - PACS 호환성 테스트

### 7.5 합격/불합격 기준

**합격 조건 (ALL 만족):**
- 모든 파일 크기 일치 ✓
- 다크 드리프트 < 15% ✓
- CalSet 다항식 R² > 0.99 ✓
- 임상 이미지 PSNR > 40 dB ✓
- CES 점수 ≥ 85 ✓

**불합격 조건 (ANY 만족):**
- 파일 손상 (크기 불일치)
- 다크 신호 비정상 (< 80 또는 > 200 ADU 평균)
- CalSet 피팅 오류 > 3%
- 임상 이미지 NaN/Inf 발생
- DICOM 헤더 누락

---

## 사용 예제

### Python 코드 (CalSet 다항식 피팅)

```python
import numpy as np
from scipy.interpolate import interp1d

# 1. CalSet 파일 로드 및 다크 보정
dose_levels = [14037, 17285, 20985, 30868, 42677]
calset_signals = []

with open('MasterDark.raw', 'rb') as f:
    dark_data = np.frombuffer(f.read(), dtype=np.uint16).reshape(3072, 3072)

for dose in dose_levels:
    filename = f'CalSet_{dose}.raw'
    with open(filename, 'rb') as f:
        calset_raw = np.frombuffer(f.read(), dtype=np.uint16).reshape(3072, 3072)
    
    # 다크 보정
    calset_dark_corr = np.maximum(0, calset_raw.astype(float) - dark_data.astype(float))
    signal = np.mean(calset_dark_corr)
    calset_signals.append(signal)

# 2. 다항식 피팅
X = np.array(dose_levels, dtype=float)
Y = np.array(calset_signals, dtype=float)

# 2차 다항식
coeffs = np.polyfit(X, Y, deg=2)
print(f"다항식 계수: {coeffs}")

# 3. 품질 평가
fitted_y = np.polyval(coeffs, X)
residuals = Y - fitted_y
rmse = np.sqrt(np.mean(residuals**2))
r_squared = 1 - np.sum(residuals**2) / np.sum((Y - np.mean(Y))**2)

print(f"R²: {r_squared:.4f}")
print(f"RMSE: {rmse:.2f} ({100*rmse/np.mean(Y):.2f}%)")

# 4. 검증용 외삽
dose_test = 25000
signal_pred = np.polyval(coeffs, dose_test)
print(f"Dose={dose_test} 예측값: {signal_pred:.1f}")
```

### C++ 코드 (E2E 파이프라인)

```cpp
#include <xpe_preprocess.h>
#include <xpe_dicom.h>
#include <cstdio>

int main() {
    // 1. 세션 생성
    xpe_session_t session;
    xpe_session_create(&session, "CYAN-TEST");
    
    // 2. 다크 파일 로드
    xpe_image_t dark;
    xpe_image_load(&dark, "MasterDark.raw");
    xpe_session_set_offset_frame(&session, dark);
    
    // 3. CalSet 다항식 피팅
    float dose_levels[] = {14037, 17285, 20985, 30868, 42677};
    xpe_image_t calsets[5];
    
    for (int i = 0; i < 5; i++) {
        char filename[256];
        sprintf(filename, "CalSet_%.0f.raw", dose_levels[i]);
        xpe_image_load(&calsets[i], filename);
    }
    
    // 다항식 계산 (외부 라이브러리: GSL, Eigen 등)
    // ... polynomial fitting code ...
    
    // 4. 임상 이미지 전처리
    xpe_image_t clinical_raw;
    xpe_image_load(&clinical_raw, "Bright_Chest_phantom_75kv_320ma_25.6(SID_110)_00.raw");
    
    xpe_image_t result;
    xpe_session_process(&session, clinical_raw, &result);
    
    // 5. DICOM 출력
    xpe_dicom_write(&result, "Bright_Chest_phantom_75kv_320ma_25.6(SID_110)_00_result_raw.dcm");
    
    // 6. E2E 보고서 생성
    xpe_report_t report;
    xpe_session_get_report(&session, &report);
    xpe_report_print(&report);
    
    // 정리
    xpe_image_destroy(&result);
    xpe_image_destroy(&clinical_raw);
    xpe_session_destroy(&session);
    
    return 0;
}
```

---

## 관련 문서

| 문서 | 링크 | 목적 |
|------|------|------|
| SRS 캘리브레이션 | `docs/calibration/SRS-CALIB-001.md` | 다중 게인, 비선형성 요구사항 |
| 종래기술 분석 | `docs/calibration/PRIOR-ART-BPM-ALGORITHM.md` | 알고리즘 비교 |
| TDS 전체 | `docs/calibration/TDS-CALIB-001.md` | 테스트 데이터셋 전체 명세 |
| E2E 프로토콜 | `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md` | 자동화 검증 메트릭 |

---

**데이터셋 끝**

*작성: 2026-04-19 | 버전: 1.0.0 | 데이터 수집: 2026-04-02*
