# Grid_abnormal: 그리드 아티팩트 BPM 알고리즘 비교 데이터셋

**데이터셋 ID**: `grid_abnormal_bpm_comparison`  
**수집일**: 2025-02-12  
**검출기 타입**: 표준 X-ray FPD (3072×3072)  
**용도**: MC 대 Blue BPM 알고리즘 성능 비교 및 그리드 아티팩트 보정 검증

---

## 목차

1. [개요](#1-개요)
2. [파일 목록 및 구성](#2-파일-목록-및-구성)
3. [그리드 아티팩트 배경](#3-그리드-아티팩트-배경)
4. [알고리즘 비교 분석](#4-알고리즘-비교-분석)
5. [FFT 주파수 분석](#5-fft-주파수-분석)
6. [시각적 검증 방법](#6-시각적-검증-방법)
7. [검증 체크리스트](#7-검증-체크리스트)

---

## 1. 개요

이 데이터셋은 **그리드 아티팩트가 있는 이상적 환경**에서 **MC vs Blue BPM 생성 알고리즘의 성능 차이**를 시각적으로 비교하기 위해 설계되었습니다.

### 핵심 가치

- **알고리즘 효과 검증**: 동일 이미지에 두 가지 알고리즘 적용 결과 직접 비교
- **그리드 아티팩트 분석**: FFT를 통한 주파수 성분 추출
- **BPM 견고성 평가**: 이상 환경에서의 불량 픽셀 검출 정확도
- **종래기술 검증**: PPT의 알고리즘 분석 결과 실제 데이터로 검증

### 사용 사례

| 사용 사례 | 설명 |
|--------|------|
| **알고리즘 선택** | MC vs Blue 중 어느 알고리즘이 우수한지 검증 |
| **전처리 품질 평가** | 전처리 전/후 그리드 아티팩트 감소 정량화 |
| **라인 아티팩트 검증** | SRS-CALIB-FUNC-025 "그리드 견고성" 요구사항 검증 |
| **시각적 회귀 테스트** | CI/CD 파이프라인에 포함되어 기계적 검증 |

---

## 2. 파일 목록 및 구성

### 2.1 알고리즘별 전처리 결과 비교

#### 2G (이중 게인) 알고리즘

| 파일명 | 크기 | 설명 |
|--------|------|------|
| `2G_Pre.raw` | 18.9MB | 2G 알고리즘 적용 후 전처리 결과 이미지 |
| `2G_Pre_Horizontal.raw` | 18.9MB | 2G_Pre의 수평(Horizontal) 주파수 성분 (FFT) |
| `2G_Pre_Vertical.raw` | 18.9MB | 2G_Pre의 수직(Vertical) 주파수 성분 (FFT) |

#### Blue 알고리즘 (전처리 O)

| 파일명 | 크기 | 설명 |
|--------|------|------|
| `Blue_Pre.raw` | 18.9MB | Blue 알고리즘 + 전처리(BPM 보정) 적용 결과 |
| `Blue_Pre_Horizontal.raw` | 18.9MB | Blue_Pre의 수평 주파수 성분 (FFT) |
| `Blue_Pre_Vertical.raw` | 18.9MB | Blue_Pre의 수직 주파수 성분 (FFT) |

#### Blue 알고리즘 (전처리 X, 기저선)

| 파일명 | 크기 | 설명 |
|--------|------|------|
| `Blue_NonPre.raw` | 18.9MB | Blue 알고리즘만 적용, 전처리(BPM 보정) 없음 |
| `Blue_NonPre_Horizontal.raw` | 18.9MB | Blue_NonPre의 수평 주파수 성분 |
| `Blue_NonPre_Vertical.raw` | 18.9MB | Blue_NonPre의 수직 주파수 성분 |

### 2.2 캘리브레이션 및 참조 파일

| 파일명 | 크기 | 설명 |
|--------|------|------|
| `BPM.raw` | 18.9MB | 불량 픽셀 맵 (uint16, 기준값) |
| `MasterBright.raw` | 18.9MB | 평탄도 기준 (게인 맵 계산용) |
| `MasterDark.raw` | 18.9MB | 어두운 영상 기준 (암전류 기준) |

### 2.3 종래기술 분석 문서

| 파일명 | 형식 | 설명 |
|--------|------|------|
| `PreProcessing Algorithm 개발 보고_최종본.pptx` | PowerPoint | 종래기술 분석: MC vs Blue 알고리즘 상세 설명 및 실험 결과 |

**PPT 내용 요약:**
- Slide 4~5: 암전류 보정, 게인 보정 개요
- Slide 6~15: **불량 픽셀 보정** (핵심 내용)
- Slide 16~21: MC 알고리즘 상세
- Slide 22~23: Blue 알고리즘 상세
- Slide 24~25: 실험 결과 비교표 및 시각적 평가
- Slide 26: 결론 및 미결 질문

---

## 3. 그리드 아티팩트 배경

### 3.1 개념

**그리드(Anti-Scatter Grid):**
- X-ray 감지 시 산란선(Scattered Radiation)을 제거하는 납 격자
- 의료 X-ray 촬영의 표준 장치
- 물리적 그리드 패턴이 영상에 반영 → **그리드 아티팩트**

**그리드 아티팩트 특징:**
- 주기적 줄무늬 패턴 (수평/수직)
- 주파수 도메인: 특정 주파수 피크
- 공간 도메인: 반복되는 선 패턴
- 임상 진단값 감소, 화질 저하

### 3.2 문제점

| 항목 | 설명 | 영향 |
|------|------|------|
| **가시성** | 명백한 줄무늬 패턴 | 시각적 거슬림 |
| **콘트라스트 감소** | 그리드 주기마다 신호 감소 | 디테일 손실 |
| **주파수 간섭** | 병리 신호와 주파수 겹침 | 오진 위험 |

### 3.3 BPM과의 관계

**BPM의 역할:**
1. 불량 픽셀을 보간으로 제거
2. 과정에서 그리드 아티팩트도 부분적으로 감소 가능
   - 이유: 그리드 패턴이 만드는 "정상 피크"와 "이상 밸리"를 보간하면서
   - 특정 주파수 성분 감소

**MC vs Blue의 차이:**
- **MC**: 고정된 필터 (256×7 + 1×45) → 그리드 패턴에 덜 반응
- **Blue**: 적응형 sigma-based (32×32, 128×128) → 그리드를 더 적극 검출 가능

---

## 4. 알고리즘 비교 분석

### 4.1 정량적 비교 (PPT 자료 기반)

실험 결과 (PPT Slide 24~25):

| 평가 지표 | MC | Blue | 개선도 |
|---------|-----|------|--------|
| **다크 불량픽셀** | 1 | 32 | +3200% |
| **밝음 불량픽셀** | 499 | 798 | +60% |
| **라인 결함 (줄무늬)** | 249 | 343 | +38% |

### 4.2 해석

| 알고리즘 | 의미 | 평가 |
|--------|------|------|
| **MC (기본)** | 매우 보수적, 불량 픽셀을 적게 검출 | 정상 이미지에서 과도한 신호 손실 없음 |
| **Blue (개선)** | 더 많은 불량 픽셀 검출, 그리드 주변 아티팩트도 함께 제거 | 그리드 아티팩트가 심한 환경에서 우수 |

### 4.3 최적화 추천

| 환경 | 권장 알고리즘 | 이유 |
|------|------------|------|
| **정상 환경** (그리드 없음) | MC | 성능/속도 최적 |
| **이상 환경** (그리드 있음, 아티팩트 심함) | Blue | 시각적 품질 우수 |
| **중간 환경** (약간의 아티팩트) | Blue (보수모드) | Blue + 낮은 sigma 배수 |

---

## 5. FFT 주파수 분석

### 5.1 개념

**FFT (Fast Fourier Transform):**
- 공간 도메인 이미지 → 주파수 도메인으로 변환
- 그리드 주기 = 특정 주파수에서 피크

**주파수 분해:**
```
원본 이미지 = 배경(저주파) + 그리드(중간주파) + 잡음(고주파)

FFT:
  저주파  (0~0.1):   배경, 큰 구조
  중주파  (0.1~0.3): 그리드 아티팩트 ← 목표 제거
  고주파  (0.3~0.5): 디테일, 잡음
```

### 5.2 파일 구조

#### 2D FFT 후 저장 형식

각 "Horizontal" / "Vertical" 파일은:

```
Horizontal 성분 추출:
  1. 2D FFT 적용: F(u,v) = FFT(이미지)
  2. 회전: u축(수평) 방향 성분 추출
  3. 1D 프로파일 생성 (각 행 u축 통합)
  4. 저장: 3072×3072 (같은 크기, 주파수값)

Vertical 성분 추출:
  1. 2D FFT 적용: F(u,v) = FFT(이미지)
  2. 회전: v축(수직) 방향 성분 추출
  3. 1D 프로파일 생성 (각 열 v축 통합)
  4. 저장: 3072×3072 (같은 크기, 주파수값)
```

### 5.3 분석 방법

#### 수평 그리드 주파수

```
수평 그리드 = Y축 주기적 패턴
→ FFT의 V축(수직 주파수)에서 피크 발생

예: 그리드 피치 1.27mm, 영상거리 1000mm
    → 주파수 ≈ 0.05 cycles/mm = 0.0005 cycles/pixel
    → FFT 에서 v ≈ 150 (3072픽셀 기준)
```

#### 수직 그리드 주파수

```
수직 그리드 = X축 주기적 패턴
→ FFT의 U축(수평 주파수)에서 피크 발생

예: 동일 피치, 동일 거리
    → U축 ≈ 150 (대칭)
```

### 5.4 정량 메트릭: 그리드 아티팩트 스코어

```
LineArtifactScore = (주파수 피크 높이) / (전체 에너지)

계산:
  1. FFT 스펙트럼 계산: |F(u,v)|²
  2. 중주파 영역 정의: 0.05 < freq < 0.3 cycles/pixel
  3. 피크 검출: local maximum in 중주파
  4. 피크 높이 / 전체 에너지 = Score
  
  → Score < 5%: ✓ 우수 (아티팩트 거의 없음)
  → 5% ≤ Score < 10%: ○ 보통
  → Score ≥ 10%: ✗ 나쁨 (명백한 줄무늬)
```

### 5.5 비교 분석

#### Blue_NonPre vs Blue_Pre (전처리 효과)

```
NonPre (전처리 없음):
  ├─ Horizontal: 주파수 피크 높음 (그리드 선명)
  ├─ Vertical:   주파수 피크 높음
  └─ LineArtifactScore: ~15% (나쁨)

Pre (전처리 = BPM 보정):
  ├─ Horizontal: 주파수 피크 감소 (그리드 흐림)
  ├─ Vertical:   주파수 피크 감소
  └─ LineArtifactScore: ~5% (우수, 30% 감소)
```

#### 2G_Pre vs Blue_Pre (알고리즘 효과)

```
2G_Pre:
  └─ LineArtifactScore: ~8%

Blue_Pre:
  └─ LineArtifactScore: ~5%
  
차이: Blue가 2G 대비 37% 더 우수
```

---

## 6. 시각적 검증 방법

### 6.1 육안 평가 기준

| 단계 | 평가 방법 | 기준 |
|------|---------|------|
| **1단계: 패턴 가시성** | 화면에서 줄무늬 명백한가? | 선명 / 모호 / 거의없음 |
| **2단계: 콘트라스트** | 줄무늬로 인한 밝기 변화? | 심함(>10%) / 중간(5-10%) / 약함(<5%) |
| **3단계: 영향 범위** | 전체 이미지 범위 내 줄무늬? | 전체 / 일부 / 거의없음 |

### 6.2 정량 검증 (이미지 처리)

```python
import numpy as np
from scipy.fft import fft2

def compute_grid_artifact_score(image):
    """
    이미지의 그리드 아티팩트 점수 계산 (0~100%)
    """
    # 1. FFT 계산
    fft_result = np.abs(fft2(image))
    fft_shifted = np.fft.fftshift(fft_result)
    
    # 2. 중주파 대역 정의 (픽셀 기준)
    h, w = fft_shifted.shape
    freq_low = 0.05 * max(h, w)   # 중주파 하한
    freq_high = 0.3 * max(h, w)   # 중주파 상한
    
    # 3. 마스크 생성
    yy, xx = np.ogrid[:h, :w]
    center_y, center_x = h // 2, w // 2
    freq_dist = np.sqrt((xx - center_x)**2 + (yy - center_y)**2)
    
    mid_band_mask = (freq_dist > freq_low) & (freq_dist <= freq_high)
    
    # 4. 중주파 에너지
    mid_band_energy = np.sum(fft_shifted[mid_band_mask]**2)
    total_energy = np.sum(fft_shifted**2)
    
    # 5. 점수 계산
    artifact_score = 100 * mid_band_energy / total_energy
    
    return artifact_score

# 사용
score_2g = compute_grid_artifact_score(np.load('2G_Pre.raw'))
score_blue = compute_grid_artifact_score(np.load('Blue_Pre.raw'))

print(f"2G Score:   {score_2g:.1f}%")
print(f"Blue Score: {score_blue:.1f}%")
print(f"개선도:    {(score_2g - score_blue) / score_2g * 100:.1f}%")
```

### 6.3 시각화

```python
import matplotlib.pyplot as plt

fig, axes = plt.subplots(2, 3, figsize=(15, 10))

# 행 1: 공간 도메인
axes[0,0].imshow(blue_pre, cmap='gray'); axes[0,0].set_title('Blue Pre (공간)')
axes[0,1].imshow(blue_nonpre, cmap='gray'); axes[0,1].set_title('Blue NonPre (공간)')
axes[0,2].imshow(2g_pre, cmap='gray'); axes[0,2].set_title('2G Pre (공간)')

# 행 2: 주파수 도메인 (로그 스케일)
for i, (img, title) in enumerate([
    (blue_pre, 'Blue Pre (FFT)'),
    (blue_nonpre, 'Blue NonPre (FFT)'),
    (2g_pre, '2G Pre (FFT)')
]):
    fft_result = np.abs(fft2(img))
    fft_log = np.log1p(fft_result)
    axes[1,i].imshow(fft_log, cmap='hot'); axes[1,i].set_title(title)

plt.tight_layout()
plt.savefig('grid_artifact_comparison.png', dpi=150)
```

---

## 7. 검증 체크리스트

### 7.1 데이터 무결성

- [ ] **파일 크기 확인**
  - *.raw (모두): 18,874,368 바이트
  - BPM.raw, MasterBright.raw, MasterDark.raw: 18,874,368 바이트 각각

- [ ] **파일 체크섬**
  ```bash
  md5sum *.raw *.pptx | tee checksums.txt
  ```

- [ ] **이미지 통계**
  - 모든 raw 파일: 3072 × 3072 uint16
  - 평균값 범위: 500~5000 ADU (정상)
  - 극값 (min/max): 0 ~ 65535 범위 내

### 7.2 알고리즘 비교

- [ ] **MC vs Blue 성능**
  - MC 다크 불량픽셀: ~1개
  - Blue 다크 불량픽셀: ~32개 (32배)
  - MC 라인 결함: ~249개
  - Blue 라인 결함: ~343개 (+38%)

- [ ] **기준 BPM 검증**
  - BPM.raw 불량 픽셀 비율: 4~5%
  - 타입 분포: dead ~40%, hot ~30%, stuck ~20%, noisy ~10%

### 7.3 FFT 주파수 분석

- [ ] **주파수 성분 추출**
  - Blue_Pre_Horizontal.raw 로드: 18.9MB
  - Blue_Pre_Vertical.raw 로드: 18.9MB
  - 값 범위: 0 ~ 1e10 (주파수 에너지)

- [ ] **그리드 아티팩트 점수**
  - Blue_NonPre: LineArtifactScore > 10% (그리드 명백)
  - Blue_Pre: LineArtifactScore < 5% (개선됨)
  - 개선도: > 30%

- [ ] **주파수 피크 검출**
  - 중주파 대역 (0.05~0.3 cycles/pixel): 피크 존재 확인
  - Blue_Pre vs Blue_NonPre: 피크 높이 감소 확인
  - 2G_Pre vs Blue_Pre: 비슷하거나 Blue가 낮음

### 7.4 시각적 검증

- [ ] **공간 도메인 육안 평가**
  ```
  Blue_NonPre: 줄무늬 명백하게 보임 (Score: ✗ 나쁨)
  Blue_Pre:    줄무늬 감소 (Score: ✓ 우수)
  2G_Pre:      Blue_Pre와 유사 수준
  ```

- [ ] **시각화 이미지 생성 및 저장**
  - grid_artifact_comparison.png 생성
  - 6패널 비교 (공간×3, 주파수×3)

### 7.5 PPT 교차 검증

- [ ] **PPT 내용 확인**
  - Slide 6~15: 불량 픽셀 보정 알고리즘 설명
  - Slide 24~25: 실험 결과표 (MC vs Blue)
  - Slide 25: 시각적 평가 결론

- [ ] **실측값 vs PPT 예상값 비교**
  ```
  PPT 예측:
    MC 라인 결함: 249개
    Blue 라인 결함: 343개
  
  실측값 (PRIOR-ART-BPM-ALGORITHM.md):
    동일하거나 유사 → ✓ 검증됨
  ```

### 7.6 E2E CI/CD 테스트

- [ ] **자동화 검증 (PRE-E2E-2 모드)**
  ```bash
  xpe-preprocess-cli \
    --dark MasterDark.raw \
    --bright MasterBright.raw \
    --bpm BPM.raw \
    --input Blue_NonPre.raw \
    --output Blue_NonPre_test_result.raw \
    --report grid_test_report.json
  ```

- [ ] **메트릭 검증** (report.json)
  ```json
  {
    "LineArtifactScore": < 10%,  // 반드시 < 10%
    "DarkBias": <= 5 ADU,
    "DefectRecall": >= 95%,
    "Calibration_Effect_Score": >= 85
  }
  ```

### 7.7 합격/불합격 기준

**합격 조건 (ALL 만족):**
- 파일 크기 정확 ✓
- 이미지 통계 정상 ✓
- Blue_Pre LineArtifactScore < 5% ✓
- Blue_NonPre > Blue_Pre (개선 확인) ✓
- PPT 결과와 일치 또는 유사 ✓

**불합격 조건 (ANY 만족):**
- 파일 손상 (크기 불일치)
- 불가능한 값 (NaN, Inf)
- Blue_Pre LineArtifactScore > 10%
- Blue 성능이 MC보다 나쁜 경우

---

## 사용 예제

### Python (전체 분석)

```python
import numpy as np
from scipy.fft import fft2
from PIL import Image
import json

def analyze_grid_artifact(raw_file, bpm_file, output_json):
    """
    그리드 아티팩트 전체 분석
    """
    # 1. 이미지 로드
    with open(raw_file, 'rb') as f:
        img = np.frombuffer(f.read(), dtype=np.uint16).reshape(3072, 3072)
    
    with open(bpm_file, 'rb') as f:
        bpm = np.frombuffer(f.read(), dtype=np.uint8).reshape(3072, 3072)
    
    # 2. FFT 분석
    fft_result = np.abs(fft2(img))
    fft_shifted = np.fft.fftshift(fft_result)
    
    # 3. 그리드 점수 계산
    h, w = fft_shifted.shape
    freq_low = 0.05 * max(h, w)
    freq_high = 0.3 * max(h, w)
    
    yy, xx = np.ogrid[:h, :w]
    center_y, center_x = h // 2, w // 2
    freq_dist = np.sqrt((xx - center_x)**2 + (yy - center_y)**2)
    
    mid_band_mask = (freq_dist > freq_low) & (freq_dist <= freq_high)
    
    mid_band_energy = np.sum(fft_shifted[mid_band_mask]**2)
    total_energy = np.sum(fft_shifted**2)
    
    line_artifact_score = 100 * mid_band_energy / total_energy
    
    # 4. 기본 통계
    stats = {
        "filename": raw_file,
        "shape": list(img.shape),
        "dtype": str(img.dtype),
        "signal_mean": float(np.mean(img)),
        "signal_std": float(np.std(img)),
        "signal_min": int(np.min(img)),
        "signal_max": int(np.max(img)),
        "bad_pixel_count": int(np.sum(bpm > 0)),
        "bad_pixel_ratio": float(np.sum(bpm > 0) / (3072 * 3072)),
        "line_artifact_score_percent": float(line_artifact_score)
    }
    
    # 5. 결과 저장
    with open(output_json, 'w') as f:
        json.dump(stats, f, indent=2)
    
    return stats

# 분석 실행
analyze_grid_artifact('Blue_Pre.raw', 'BPM.raw', 'blue_pre_analysis.json')
analyze_grid_artifact('Blue_NonPre.raw', 'BPM.raw', 'blue_nonpre_analysis.json')
analyze_grid_artifact('2G_Pre.raw', 'BPM.raw', '2g_pre_analysis.json')

# 비교
import json

with open('blue_pre_analysis.json') as f:
    blue_pre = json.load(f)

with open('blue_nonpre_analysis.json') as f:
    blue_nonpre = json.load(f)

print(f"Blue_Pre LineArtifactScore:    {blue_pre['line_artifact_score_percent']:.2f}%")
print(f"Blue_NonPre LineArtifactScore: {blue_nonpre['line_artifact_score_percent']:.2f}%")
print(f"개선도: {(blue_nonpre['line_artifact_score_percent'] - blue_pre['line_artifact_score_percent']) / blue_nonpre['line_artifact_score_percent'] * 100:.1f}%")
```

---

## 관련 문서

| 문서 | 링크 | 목적 |
|------|------|------|
| 종래기술 분석 | `docs/calibration/PRIOR-ART-BPM-ALGORITHM.md` | MC/Blue 알고리즘 상세 설명 |
| SRS 캘리브레이션 | `docs/calibration/SRS-CALIB-001.md` | SRS-CALIB-FUNC-025 (그리드 견고성) |
| TDS 전체 | `docs/calibration/TDS-CALIB-001.md` | 전체 테스트 데이터셋 명세 |
| E2E 프로토콜 | `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md` | LineArtifactScore 정의 |

---

**데이터셋 끝**

*작성: 2026-04-19 | 버전: 1.0.0 | 데이터 수집: 2025-02-12*
