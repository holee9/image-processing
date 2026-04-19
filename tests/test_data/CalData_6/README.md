# CalData_6: 다중 단계 평탄화 캘리브레이션 데이터셋

**데이터셋 ID**: `caldata_6_mc_multistep`  
**수집일**: 2025-02-24  
**검출기 타입**: 표준 X-ray FPD (3072×3072)  
**용도**: 다중 선량 레벨 게인 맵 생성 및 비선형성 LUT 검증

---

## 목차

1. [개요](#1-개요)
2. [파일 목록](#2-파일-목록)
3. [파일 형식 명세](#3-파일-형식-명세)
4. [캘리브레이션 워크플로우](#4-캘리브레이션-워크플로우)
5. [검증 체크리스트](#5-검증-체크리스트)

---

## 1. 개요

이 데이터셋은 **다중 선량 레벨에서의 평탄화(Flat-field) 캘리브레이션**을 수행하기 위해 설계되었습니다.

### 주요 특징

- **6단계 선량 레벨**: 다양한 선량 조건에서의 게인 변화 측정
- **암전류 기준**: 어두운 영상 기준값 포함
- **불량 픽셀 맵**: 사전 생성된 BPM으로 검증 기준 제공
- **위치 정렬**: 픽셀 정렬 및 결함 맵 포함

### 사용 사례

| 사용 사례 | 설명 |
|--------|------|
| **다중 게인 보정** | 다양한 선량 조건에서 게인 계수 다항식 피팅 |
| **비선형성 보정 LUT 생성** | 6개 평탄도 영상에서 비선형성 특성 추출 |
| **BPM 검증** | 생성된 불량 픽셀 맵의 정확도 평가 |
| **알고리즘 벤치마킹** | MC vs Blue 알고리즘 비교 검증 |

---

## 2. 파일 목록

### 2.1 핵심 파일

| 파일명 | 크기 | 형식 | 설명 |
|--------|------|------|------|
| `dark.raw` | 18.9MB | uint16 | 암전류(Dark Current) 기준 프레임. 빛이 없는 상태에서 검출기의 고유 신호. 모든 보정의 기준점. |
| `bright01.raw` | 18.9MB | uint16 | 평탄화 프레임 1 (선량 레벨 1) |
| `bright02.raw` | 18.9MB | uint16 | 평탄화 프레임 2 (선량 레벨 2) |
| `bright03.raw` | 18.9MB | uint16 | 평탄화 프레임 3 (선량 레벨 3) |
| `bright04.raw` | 18.9MB | uint16 | 평탄화 프레임 4 (선량 레벨 4) |
| `bright05.raw` | 18.9MB | uint16 | 평탄화 프레임 5 (선량 레벨 5) |
| `bright06.raw` | 18.9MB | uint16 | 평탄화 프레임 6 (선량 레벨 6) |

### 2.2 메타데이터 및 참조 파일

| 파일명 | 크기 | 형식 | 설명 |
|--------|------|------|------|
| `BPMap.map` | 9.4MB | uint8 | 불량 픽셀 맵 (절반 크기 압축). 값: 0=정상, 1~255=결함 타입 (1=dead, 2=hot, 3=stuck, 4=noisy). RLE 압축 가능. |
| `PositionMap.raw` | 18.9MB | uint16 | 픽셀 위치 정렬 맵. 카메라 렌즈 비뭉침/왜곡 보정용. |
| `Total_Defect_Map.raw` | 18.9MB | uint16 | 통합 결함 맵. 모든 결함 타입을 포함하는 종합 맵. |
| `Cluster_position.txt` | (empty) | TXT | 클러스터 결함 위치 데이터 (선택적, 현재 비어있음) |

---

## 3. 파일 형식 명세

### 3.1 .raw 파일 형식

**크기:** 3072 (폭) × 3072 (높이) × 2바이트 (uint16) = **18,874,368 바이트** ≈ 18.9MB

**데이터 배열:**
```
Offset (bytes) | Content                    | Size
0              | Row 0, Col 0-3071         | 6144 (uint16 × 3072)
6144           | Row 1, Col 0-3071         | 6144
...
18,868,224     | Row 3071, Col 0-3071      | 6144
```

**바이트 순서:** Little-endian (Intel 표준)

**값 범위:** 0 ~ 65535 (ADU, Analog-to-Digital Unit)

### 3.2 .map 파일 형식 (BPMap.map)

**크기:** 3072 (폭) × 3072 (높이) × 1바이트 (uint8) = **9,437,184 바이트** ≈ 9.4MB

**데이터 배열:**
```
Offset (bytes) | Content                    | Size
0              | Row 0, Col 0-3071         | 3072 (uint8 × 3072)
3072           | Row 1, Col 0-3071         | 3072
...
9,434,112      | Row 3071, Col 0-3071      | 3072
```

**픽셀 값 의미:**

| 값 | 의미 | 보정 방법 |
|----|------|---------|
| 0 | 정상 픽셀 | 보정 필요 없음 |
| 1 | Dead Pixel (신호 없음) | 이웃 픽셀 평균값으로 보간 |
| 2 | Hot Pixel (과도한 신호) | 이웃 픽셀 평균값으로 보간 |
| 3 | Stuck Pixel (고정값) | 이웃 픽셀 중앙값으로 보간 |
| 4 | Noisy Pixel (높은 잡음) | 중앙값 필터링 (3×3) |

**압축 옵션:** RLE (Run-Length Encoding) 가능
```
예: 254개 연속된 0 → (count=254, value=0)
→ 크기 감소: 9.4MB → ~500KB (대부분 정상 픽셀)
```

### 3.3 Position Map 형식

**크기:** 3072 × 3072 × uint16 = 18.9MB (dark.raw와 동일)

**의미:**
- 각 픽셀의 물리적 위치 좌표 (서브픽셀 정확도)
- 렌즈 기하 왜곡 보정용
- 값: 픽셀 위치 (미세 이동값 인코딩)

---

## 4. 캘리브레이션 워크플로우

### 4.1 게인 맵 생성 (Multi-level Gain Calibration)

```
Step 1: 암전류 보정
  └─ I_dark_corrected = bright_frame - dark.raw
     (모든 bright01~06에 대해 반복)

Step 2: 정규화 (Normalization)
  └─ 각 선량 레벨별 평균 신호 계산
     Mean_1 = average(bright01_dark_corrected)
     Mean_2 = average(bright02_dark_corrected)
     ...
     Mean_6 = average(bright06_dark_corrected)

Step 3: 선형 회귀 (Linear Regression)
  └─ 선량 레벨 vs 평균 신호 피팅
     Mean_signal = gain × dose_level
     
     X = [1, 2, 3, 4, 5, 6]  (선량 레벨)
     Y = [Mean_1, Mean_2, ..., Mean_6]  (신호)
     
     게인 = slope of (Y vs X)

Step 4: 픽셀별 게인 맵 계산
  └─ G(x,y) = I_corrected(x,y) / mean_signal
     (각 픽셀의 정규화 인수)

Step 5: 유효성 검증
  └─ FPN (Fixed Pattern Noise) 계산
     FPN_CV (Coefficient of Variation) ≤ 5%
     → 게인 맵 승인 또는 재수집
```

### 4.2 비선형성 보정 LUT 생성

```
Step 1: 다양한 선량 평면에서 신호 수집
  └─ Dose = {20%, 40%, 60%, 80%, 100%} × ADC_max
  └─ 각 dose에 대해 flat-field 평균 신호: S_meas[i]

Step 2: 이상적 선형 응답 계산
  └─ S_ideal[i] = gain × dose[i]
     (게인은 Step 4.1에서 구한 전체 평균 게인)

Step 3: 비선형성 LUT 구성
  └─ LUT[S_meas[i]] = S_ideal[i]
  └─ 중간 값들은 단조 증가 스플라인으로 보간
     (Fritsch-Carlson 방식)

Step 4: LUT 검증
  └─ 모든 입력값에서 보간 오차 < 0.3% ADU
  └─ 단조성 확인: LUT[i] ≤ LUT[i+1]

Step 5: 최종 LUT 저장
  └─ 4096 또는 65536 엔트리 (ADC 범위에 따라)
```

### 4.3 불량 픽셀 맵(BPM) 검증

```
Step 1: 기준 BPM 로드
  └─ BPMap.map 로드 (9.4MB)

Step 2: MC/Blue 알고리즘 실행
  └─ 밝음 프레임에서 자동으로 BPM 검출
     (MC: 256×7 + 1×45 윈도우)
     (Blue: 32×32 + 128×128 윈도우, sigma-based)

Step 3: 생성된 맵 vs 기준 맵 비교
  └─ TP (True Positive): 올바르게 검출된 불량픽셀
  └─ FP (False Positive): 오류로 표시된 정상픽셀
  └─ FN (False Negative): 놓친 불량픽셀
  
     Recall = TP / (TP + FN)  → ≥ 95% 목표
     FPR = FP / (FP + TN)     → ≤ 1% 목표

Step 4: 결과 보고
  └─ 불량 픽셀 밀도: 4~5% (정상 범위)
  └─ 클러스터링 패턴: 프로세스 결함 분석
```

---

## 5. 검증 체크리스트

### 5.1 데이터 무결성 검사

- [ ] **파일 크기 확인**
  - dark.raw: 18,874,368 바이트 (± 0)
  - bright01~06.raw: 각각 18,874,368 바이트
  - BPMap.map: 9,437,184 바이트
  - PositionMap.raw: 18,874,368 바이트
  - Total_Defect_Map.raw: 18,874,368 바이트

- [ ] **파일 유효성** (바이트 단위 검증)
  ```bash
  # Linux/macOS
  md5sum dark.raw bright*.raw *.map *.raw
  
  # Windows PowerShell
  Get-FileHash dark.raw
  ```

- [ ] **이미지 통계**
  - dark.raw 평균값: 100~150 ADU (검출기 특성에 따라)
  - bright01~06 평균값: 400~4000 ADU (선량 레벨별 증가)
  - BPMap 불량 픽셀 비율: 3~5% (정상)

### 5.2 게인 맵 생성 검증

- [ ] **암전류 보정**
  - I_corrected = bright_frame - dark.raw 계산
  - 음수값 클램핑: max(0, I_corrected)

- [ ] **게인 맵 통계**
  - 전체 게인 평균: 1.0 ± 0.1
  - 게인 표준편차 (FPN): ≤ 5%
  - 게인값 범위: [0.8, 1.2] (정상 범위)

- [ ] **비선형성 검증**
  - 6개 선량 레벨 신호의 R² > 0.99 (직선성)
  - LUT 생성 후 residual: < 0.3% ADU

- [ ] **픽셀별 검증** (샘플링)
  ```
  corner (0,0):       G_normal ± 5%
  center (1536,1536): G_normal (기준)
  corner (3071,3071): G_normal ± 5%
  ```

### 5.3 BPM 검증

- [ ] **BPM 로드 및 통계**
  - BPMap.map 로드: 9.4MB (압축 전)
  - 불량 픽셀 비율: 4.5 ± 0.5%
  - 결함 타입 분포:
    - Dead (1): ~40%
    - Hot (2): ~30%
    - Stuck (3): ~20%
    - Noisy (4): ~10%

- [ ] **알고리즘 비교**
  - MC 알고리즘 결과: < 500개 BPM (과도한 탐지)
  - Blue 알고리즘 결과: > 100,000개 BPM (더 보수적)
  - 기준 맵과의 Recall: ≥ 95%

- [ ] **시각적 검증**
  - BPM 위치 클러스터링 분석
  - 제조 결함 패턴 확인
  - Edge defect 비율: < 10%

### 5.4 E2E 파이프라인 검증

- [ ] **PRE-E2E-2 (실제 픽스처 모드) 실행**
  ```
  xpe-preprocess-cli \
    --dark dark.raw \
    --bright bright01.raw bright02.raw ... bright06.raw \
    --bpm BPMap.map \
    --output-gain gain_map.raw \
    --output-lut nonlinearity_lut.txt \
    --report caldata_6_report.json
  ```

- [ ] **리포트 검증** (JSON)
  ```json
  {
    "DarkBias": ≤ 5 ADU,
    "FlatResidualPct": ≤ 1.0%,
    "DefectRecall": ≥ 95%,
    "Calibration_Effect_Score": ≥ 85
  }
  ```

### 5.5 합격/불합격 기준

**합격 조건 (ALL 만족):**
- 게인 맵 FPN < 5%
- 비선형성 LUT residual < 0.3% ADU
- BPM Recall ≥ 95%
- CES 점수 ≥ 85

**불합격 조건 (ANY 만족):**
- 파일 손상 (크기 불일치)
- dark 신호 비정상 (< 50 또는 > 300 ADU 평균)
- 게인 표준편차 > 10%
- 비선형성 오류 > 0.5%

---

## 사용 예제

### Python 코드

```python
import numpy as np
import struct

# 1. dark.raw 로드
with open('dark.raw', 'rb') as f:
    dark_data = np.frombuffer(f.read(), dtype=np.uint16).reshape(3072, 3072)

# 2. bright01~06.raw 로드 및 게인 계산
bright_frames = []
for i in range(1, 7):
    with open(f'bright{i:02d}.raw', 'rb') as f:
        bright_data = np.frombuffer(f.read(), dtype=np.uint16).reshape(3072, 3072)
        bright_frames.append(bright_data)

# 3. 암전류 보정
dark_corrected = [np.maximum(0, b - dark_data) for b in bright_frames]

# 4. 게인 맵 계산
means = np.array([np.mean(dc) for dc in dark_corrected])
gain_map = np.stack(dark_corrected) / means.reshape(6, 1, 1)

# 5. BPMap 로드 (검증용)
with open('BPMap.map', 'rb') as f:
    bpm = np.frombuffer(f.read(), dtype=np.uint8).reshape(3072, 3072)
    bad_pixel_count = np.sum(bpm > 0)
    bad_pixel_ratio = bad_pixel_count / (3072 * 3072)
    print(f"불량 픽셀 비율: {bad_pixel_ratio:.1%}")
```

### C++ 코드 (XPE 통합)

```cpp
#include <xpe_preprocess.h>
#include <cstdint>
#include <cstring>

int main() {
    // 1. 캘리브레이션 세션 생성
    xpe_calib_session_t session;
    xpe_calib_session_create(&session);
    
    // 2. dark.raw 로드
    uint16_t* dark_data = new uint16_t[3072 * 3072];
    FILE* f = fopen("dark.raw", "rb");
    fread(dark_data, 2, 3072 * 3072, f);
    fclose(f);
    
    xpe_calib_session_set_offset(&session, dark_data);
    
    // 3. bright frames 처리 및 게인 맵 생성
    for (int i = 1; i <= 6; i++) {
        char filename[64];
        sprintf(filename, "bright%02d.raw", i);
        
        uint16_t* bright_data = new uint16_t[3072 * 3072];
        f = fopen(filename, "rb");
        fread(bright_data, 2, 3072 * 3072, f);
        fclose(f);
        
        xpe_calib_session_add_bright(&session, bright_data);
        delete[] bright_data;
    }
    
    // 4. 게인 맵 계산
    float* gain_map = new float[3072 * 3072];
    xpe_calib_compute_gain(&session, gain_map);
    
    // 5. BPM 검증
    uint8_t* bpm = new uint8_t[3072 * 3072];
    FILE* bpm_file = fopen("BPMap.map", "rb");
    fread(bpm, 1, 3072 * 3072, bpm_file);
    fclose(bpm_file);
    
    int bad_count = 0;
    for (int i = 0; i < 3072 * 3072; i++) {
        if (bpm[i] > 0) bad_count++;
    }
    printf("Bad pixel ratio: %.2f%%\n", 100.0 * bad_count / (3072 * 3072));
    
    // 정리
    delete[] dark_data;
    delete[] gain_map;
    delete[] bpm;
    xpe_calib_session_destroy(&session);
    
    return 0;
}
```

---

## 관련 문서

| 문서 | 링크 | 목적 |
|------|------|------|
| SRS 캘리브레이션 | `docs/calibration/SRS-CALIB-001.md` | 요구사항 명세 |
| 종래기술 분석 | `docs/calibration/PRIOR-ART-BPM-ALGORITHM.md` | MC/Blue 알고리즘 비교 |
| TDS 전체 | `docs/calibration/TDS-CALIB-001.md` | 테스트 데이터셋 명세 |

---

**데이터셋 끝**

*작성: 2026-04-19 | 버전: 1.0.0 | 데이터 수집: 2025-02-24*
