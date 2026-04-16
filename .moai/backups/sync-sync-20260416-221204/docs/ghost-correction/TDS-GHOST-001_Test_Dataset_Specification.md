# 테스트 데이터셋 명세서
## TDS-GHOST-001: 래그/고스팅 보정 알고리즘 검증용

**문서 ID**: TDS-GHOST-001  
**버전**: 1.0.0  
**일자**: 2026-04-14  
**목표 청중**: QA 엔지니어, 테스트 엔지니어, 개발자  
**IEC 62304 분류**: Class B (검증 데이터)  
**규범 참조**: [SRS-GHOST-001](srs_ghost_correction.md), [STP-STC](stp_stc_ghost_correction.md), [RTM](rtm_ghost_correction.md)

---

## 목차

1. [문서 정보 및 범위](#1-문서-정보-및-범위)
2. [테스트 데이터 분류](#2-테스트-데이터-분류)
3. [합성 테스트 데이터셋](#3-합성-테스트-데이터셋)
4. [반합성 테스트 데이터셋](#4-반합성-테스트-데이터셋)
5. [실제 취득 데이터셋](#5-실제-취득-데이터셋)
6. [골든 기준 값](#6-골든-기준-값)
7. [엣지 케이스 데이터셋](#7-엣지-케이스-데이터셋)
8. [성능 테스트 데이터셋](#8-성능-테스트-데이터셋)
9. [데이터셋 저장 및 추적](#9-데이터셋-저장-및-추적)
10. [테스트 실행 체크리스트](#10-테스트-실행-체크리스트)
11. [참고문헌](#11-참고문헌)

---

## 1. 문서 정보 및 범위

### 1.1 개요

이 문서는 **래그/고스팅 소프트웨어 보정 알고리즘** (Tier 1, 2, 3)의 검증 및 검증용 테스트 데이터셋 명세서입니다.

- **범위**: 합성, 반합성, 실제 취득 데이터셋 구성 및 기준 값
- **대상**: QA 엔지니어, 테스트 엔지니어, 소프트웨어 개발자
- **용도**: 단위 테스트, 통합 테스트, 회귀 테스트, 성능 검증

### 1.2 데이터셋 구성

```
테스트 데이터셋 = 합성 + 반합성 + 실제 취득
                (알려진 계수)  (주입된 오류)  (자연 래그)
```

---

## 2. 테스트 데이터 분류

### 2.1 세 가지 데이터 범주

| 범주 | 정의 | 신뢰도 | 사용 시기 |
|-----|------|--------|---------|
| **합성 (Synthetic)** | 알려진 수학 모델로 생성 | 높음 (기준값 확정) | 단위 테스트, 알고리즘 검증 |
| **반합성 (Semi-synthetic)** | 실제 데이터 + 알려진 주입 오류 | 중간 (실제 + 통제) | 통합 테스트, 경계 케이스 |
| **실제 취득 (Real)** | IAP-GHOST-001 프로토콜로 취득 | 낮음 (변수 많음) | 회귀 테스트, 현장 검증 |

---

## 3. 합성 테스트 데이터셋

### 3.1 목적

알려진 모수를 가진 합성 이미지로 알고리즘의 **정확도** 검증:
- 지수 감쇠 곡선 복원 능력
- 다중 지수 항 분해 정확도
- 비선형성 모델링 정확도

### 3.2 합성 데이터 생성 모델

#### 3.2.1 다중 지수 래그 모델

```
I_measured[n] = I_true[n] + Σ( b_k × (1 - a_k)^n )

여기서:
  - I_true[n] = 실제 신호 (우리가 복원하려는 값)
  - b_k = 래그 계수 (초기 갇힌 전하 비율)
  - a_k = 감쇠 상수 (frame^-1)
  - n = 프레임 인덱스 (0부터 시작)
```

#### 3.2.2 테스트 케이스: Tier 2 검증용

**테스트 케이스 3.2.1: 알려진 N=4 지수 항**

```
모수 (고정):
  I_true = 2000 ADU (상수 신호)
  
  래그 항 (문헌값):
  b_1 = 50 ADU    (20mm 이상의 주 래그)
  a_1 = 0.2      (τ_1 = 1/(ln(1/0.8)) = 3.1 frame ≈ 104 ms @ 30 fps)
  
  b_2 = 15 ADU    (중간 래그)
  a_2 = 0.05     (τ_2 = 20 frame ≈ 667 ms)
  
  b_3 = 5 ADU     (장시간 래그)
  a_3 = 0.01     (τ_3 = 100 frame ≈ 3.3 s)
  
  b_4 = 1 ADU     (극장시간 래그)
  a_4 = 0.002    (τ_4 = 500 frame ≈ 16.7 s)

생성:
  FOR n = 0 TO 300:
      I_measured[n] = 2000 + 50*(1-0.2)^n + 15*(1-0.05)^n 
                            + 5*(1-0.01)^n + 1*(1-0.002)^n
      (노이즈 추가: Gaussian σ = 10 ADU @ SNR = 20)

예상 출력 (보정 후):
  I_corrected[n] ≈ 2000 ± 20 ADU (RMSE < 1%)
  
기준값 (Starman et al. 2012 참조):
  1st frame lag = (50+15+5+1) / 2000 = 3.55% → 보정 후 < 0.3%
  50th frame lag = 50*0.8^50 + 15*0.95^50 + ...
                ≈ 0.04% → 보정 후 < 0.01%
```

**검증 메트릭**:

| 메트릭 | 임계값 | 판정 |
|------|--------|------|
| **RMSE (I_corrected vs I_true)** | < 1% | PASS |
| **1st frame residual** | < 0.3% | PASS |
| **50th frame residual** | < 0.01% | PASS |
| **지수 피팅 R²** | > 0.99 | PASS |

---

#### 3.2.3 테스트 케이스: Tier 3 (NLCSC) 검증용

**테스트 케이스 3.2.2: 신호 의존 비선형성**

```
모수 (신호 수준에 따라 변함):

E = 30% 포화 (신호 = 1000 ADU):
  Tier 2 (LTI) 후 잔여 래그 = 0.8%

E = 50% 포화 (신호 = 1700 ADU):
  Tier 2 (LTI) 후 잔여 래그 = 0.5% (신호 증가 → 래그 비율 감소)

E = 80% 포화 (신호 = 2700 ADU):
  Tier 2 (LTI) 후 잔여 래그 = 0.3% (더 감소)

NLCSC 모델 (노출 의존 a2_n):
  a2_n(E) = c1_n × (1 - exp(-c2_n × E))
  
  예시:
  a2_1(E) = 0.8 × (1 - exp(-0.001 × E))
  
  @ E = 1000: a2_1 = 0.8 × 0.632 = 0.506
  @ E = 2000: a2_1 = 0.8 × 0.865 = 0.692
  (신호 증가 → 감쇠 상수 증가 → 래그 빨리 사라짐)

생성:
  FOR E in [1000, 1700, 2700]:
      I_measured[n] = I_true[n] + residual_lag[n](E)
      
예상 출력 (Tier 3 후):
  모든 E에서 최종 래그 < 0.005% (NLCSC 보정 전: E-dependent)
```

**검증 메트릭**:

| 메트릭 | 임계값 | 판정 |
|------|--------|------|
| **NLCSC 후 최대 래그 (모든 E)** | < 0.005% | PASS |
| **보정 후 E-의존성 제거** | σ(lag across E) < 0.001% | PASS |
| **비선형 모델 R²** | > 0.98 | PASS |

---

### 3.3 합성 데이터셋 파일 목록

| 파일명 | 크기 | 프레임 | 설명 |
|------|------|--------|------|
| `synthetic_tier2_simple_n4.raw` | 2.4 MB | 300 | Tier 2 단순 N=4 검증 |
| `synthetic_tier3_nlcsc_3levels.raw` | 7.2 MB | 300×3 | Tier 3 신호 의존 검증 |
| `synthetic_first_frame_lag.raw` | 0.8 MB | 100 | 1st frame 구체 테스트 |
| `synthetic_50th_frame_lag.raw` | 0.8 MB | 100 | 50th frame 정밀도 테스트 |
| `synthetic_cold_detector.raw` | 1.2 MB | 150 | 냉 검출기 (no history) |

**생성 스크립트**: `test/synthetic_dataset_generator.py`

---

## 4. 반합성 테스트 데이터셋

### 4.1 목적

실제 검출기 데이터에 **알려진 교정 오류**를 주입하여:
- 경계 조건 처리 검증
- 오류 복구 로직 검증
- Tier 자동 에스컬레이션 트리거 검증

### 4.2 기본 데이터 소스

**실제 취득 Dark + Flat-field 프레임** (IAP-CALIB-001)을 기반으로 합성 래그 주입

### 4.3 반합성 케이스

#### 4.3.1 NLCSC 계수 주입 테스트

```
테스트 케이스 4.3.1: 이미지에 알려진 NLCSC 계수 주입

방법:
1. 실제 flat-field 이미지 I_raw 로드
2. 알려진 NLCSC 계수 세트 주입:
   
   FOR n = 0 TO 100:  # 100 프레임 시뮬레이션
       synthetic_lag[n] = polynomial_function(coeff_set, n)
       I_corrupted[n] = I_raw + synthetic_lag[n]
       
3. 보정 알고리즘 실행
4. 복원된 I_corrected와 원본 I_raw 비교

검증:
  RMSE(I_corrected, I_raw) < 1% → PASS
```

**테스트 파일**:

| 파일명 | 주입 계수 | 기대 결과 |
|------|--------|---------|
| `semi_synthetic_nlcsc_inject_weak.raw` | 약한 NLCSC | RMSE < 1% |
| `semi_synthetic_nlcsc_inject_strong.raw` | 강한 NLCSC | RMSE < 2% |
| `semi_synthetic_nlcsc_inject_extreme.raw` | 극강 NLCSC | Tier 3 사용, RMSE < 5% |

---

#### 4.3.2 Boundary Condition 테스트

```
테스트 케이스 4.3.2: Power-on 후 첫 촬영 (no history)

상황:
  - 검출기 방금 ON
  - ExposureHistory: 비어있음 (또는 NULL)
  - 이전 노출 없음

시나리오:
  1. 첫 프레임 = 높은 신호 (3000 ADU)
  2. 래그 신호: 없음 (처음이므로)
  3. 2번째 프레임 = 저신호 (500 ADU) + 1프레임 래그

기대 동작:
  ├─ 1st frame: bypass lag correction (history empty) → 원본 통과
  ├─ 2nd frame: 1프레임 래그만 고려 (이전 프레임 정보 사용)
  └─ 3rd frame 이상: 정상 Tier 2/3

검증:
  1st frame RMSE < 1% (보정 없음, 원본 통과)
  2nd frame RMSE < 2% (불완전 history)
  3rd frame RMSE < 1% (정상 보정)
```

**테스트 파일**: `semi_synthetic_power_on_first_frame.raw`

---

#### 4.3.3 Patient Change 경계 조건

```
테스트 케이스 4.3.3: 환자 변경 (노출 간 긴 대기)

상황:
  - 취득 1: 환자 A, 신호 2000 ADU
  - 대기: 10분 (모든 래그 방전)
  - 취득 2: 환자 B, 신호 1500 ADU

기대 동작:
  ├─ 취득 2: ExposureHistory는 존재하지만 시간 오래됨
  ├─ 자동 결정: τ 기반 decay → 무시할 수 있는 수준
  └─ 결과: bypass lag correction (또는 minimal correction)

검증:
  RMSE < 0.5% (래그 보정 불필요)
  Tier escalation: 1로 유지 (Tier 2 필요 없음)
```

**테스트 파일**: `semi_synthetic_patient_change_long_wait.raw`

---

### 4.4 반합성 데이터셋 파일 목록

| 파일명 | 크기 | 주입 오류 | 기대 RMSE |
|------|------|---------|----------|
| `semi_synthetic_nlcsc_weak.raw` | 1.8 MB | 약한 NLCSC | < 1% |
| `semi_synthetic_nlcsc_strong.raw` | 1.8 MB | 강한 NLCSC | < 2% |
| `semi_synthetic_nlcsc_extreme.raw` | 1.8 MB | 극강 NLCSC | < 5% |
| `semi_synthetic_power_on.raw` | 0.9 MB | 시작 조건 | < 1% (1f), < 2% (2f) |
| `semi_synthetic_patient_change.raw` | 0.9 MB | 긴 대기 | < 0.5% |
| `semi_synthetic_overflow_underflow.raw` | 0.9 MB | 언더/오버플로우 | 경계 처리 PASS |

---

## 5. 실제 취득 데이터셋

### 5.1 출처

IAP-GHOST-001 프로토콜에 따라 취득한 실제 FSRF/RSRF 영상

### 5.2 구성

#### 5.2.1 FSRF (Falling Step Response) 데이터셋

```
Directory: test/data/real_fsrf/

구조:
  └─ real_fsrf/
      ├─ signal_02pct/   (포화 2%)
      │   ├─ dark.raw
      │   ├─ pre_exposure.raw  (200 프레임)
      │   └─ post_exposure.raw (200 프레임)
      ├─ signal_05pct/
      ├─ signal_10pct/
      ├─ signal_30pct/
      ├─ signal_50pct/   ← 표준
      ├─ signal_70pct/
      ├─ signal_80pct/
      └─ signal_92pct/
      
총 파일 수: 9 × 3 = 27 개
총 프레임: 9 × (200 + 200 + 20) = 3,780 프레임
총 크기: ~71 MB

메타데이터:
  └─ manifest.json
      ├─ 취득 날짜/시간
      ├─ 검출기 모델
      ├─ 검출기 온도
      ├─ mAs, kVp 설정
      ├─ 신호 수준 (ADU)
      └─ SHA-256 해시 (무결성)
```

#### 5.2.2 RSRF (Rising Step Response) 데이터셋

```
Directory: test/data/real_rsrf/

구조: FSRF와 동일
  └─ real_rsrf/
      ├─ signal_02pct/
      │   ├─ dark.raw
      │   ├─ pre_exposure.raw  (50 프레임)
      │   └─ post_exposure.raw (200 프레임)
      ├─ ... (8 더)
      
총 프레임: 9 × (50 + 200 + 20) = 2,430 프레임
총 크기: ~46 MB
```

#### 5.2.3 실제 CBCT 시퀀스 데이터셋

```
Directory: test/data/real_cbct/

임상 이미지 시뮬레이션:
  ├─ pelvis_phantom/
  │   ├─ uncorrected_stack_100frames.raw  (교정 전)
  │   ├─ tier1_corrected.raw              (Tier 1만)
  │   ├─ tier2_corrected.raw              (Tier 2)
  │   └─ tier3_corrected.raw              (Tier 3)
  │
  └─ head_phantom/
      ├─ uncorrected_stack_100frames.raw
      ├─ tier1_corrected.raw
      ├─ tier2_corrected.raw
      └─ tier3_corrected.raw

메타데이터:
  └─ cbct_metadata.json
      ├─ 촬영 각도 범위 (0~360도)
      ├─ 각 프레임 신호 수준
      ├─ 팬텀 재료 (골밀도)
      └─ 기대 HU 값 범위
```

**기대 결과**:

| 팬텀 | 미보정 평균 HU | 목표 HU | Tier | 기대 결과 |
|-----|--------|---------|------|---------|
| **골반** | 35 HU | < 15 HU | Tier 2 | PASS |
| **머리** | 16 HU | < 5 HU | Tier 3 | PASS |

---

### 5.3 실제 데이터 저장 방식

**압축**: 저장소 용량 절감을 위해 선택적 ZIP 압축
```
real_fsrf/
├─ signal_50pct/
│   ├─ dark.raw.zip (SHA-256 hash: abc123...)
│   ├─ pre_exposure.raw.zip
│   └─ post_exposure.raw.zip
```

**무결성 검증**:
```
File: manifest_hashes.txt
─────────────────────────
dark_signal_50pct.raw.zip
  SHA-256: abc123def456...
  
pre_exposure_signal_50pct.raw.zip
  SHA-256: ghi789jkl012...
```

---

## 6. 골든 기준 값

### 6.1 Starman et al. 2012 기준값

이 문서의 모든 검증은 다음 기준문헌의 골든 기준값을 따릅니다:

**논문**: Starman et al., "MedPhys" 2012  
**제목**: "Nonlinear Charge-dependent Signal Correction Scheme for Flat-panel Detectors"

### 6.2 표준 FPD (a-Si TFT) 기준값

#### 6.2.1 Varex XRD 4343N (3072×3072)

**Tier 1 (Offset Correction)**:
```
입력 (FB 미적용, 높은 신호):
  평균 GCR (Ghost Coefficient Ratio) = 0.33% (미보정)
  
출력 (Tier 1 후):
  GCR = 0.33% (Tier 1은 offset만, lag 보정 아님)
  
기대: 이 값 그대로 (Tier 2로 진행)
```

**Tier 2 (LTI + Exposure-Weighted)**:
```
입력 (Tier 1 + Dark 정정):
  D_post1 잔여 래그 신호 = 0.015% 검출기 신호
  
출력 (Tier 2 후):
  GCR = 0.25% (기준문헌 Starman et al.)
  
기대: GCR < 0.25% (검증 성공)
```

**Tier 3 (NLCSC)**:
```
입력 (Tier 2 + 신호 의존 비선형성):
  노출 의존 a2_n 계수 활성화
  
출력 (Tier 3 후):
  1st frame GCR = < 0.29% (Starman et al. Table 1)
  50th frame GCR = < 0.0052% (극도로 작음)
  
기대: 
  1st frame: < 0.29%
  50th frame: < 0.0052%
```

#### 6.2.2 AUO R1717 (3072×3072)

**기대값** (동일 모듈 아키텍처):
```
Tier 2: GCR < 0.25%
Tier 3: 1st frame < 0.29%, 50th frame < 0.0052%
```

---

### 6.3 골든 기준 해시 (데이터 무결성)

모든 기준값 이미지는 SHA-256 해시로 고정:

```
File: golden_reference_hashes.txt
──────────────────────────────────────
synthetic_tier2_simple_n4.raw
  SHA-256: 1a2b3c4d5e6f7g8h9i0j1k2l3m4n5o6p

synthetic_tier3_nlcsc_3levels.raw
  SHA-256: qr9st8uv7wx6yz5ab4cd3ef2gh1ij0kl

real_fsrf/dark_signal_50pct.raw
  SHA-256: 2m3n4o5p6q7r8s9t0u1v2w3x4y5z6ab

... (모든 파일)
```

이 해시는 **회귀 테스트 중 변경되면 안 됨**:
- 해시 불일치 = 데이터 손상 또는 버전 업데이트 필요

---

## 7. 엣지 케이스 데이터셋

### 7.1 Overflow/Underflow 경계

#### 7.1.1 극단적 포화 (Overflow)

```
테스트 케이스 7.1.1: ADC 포화

이미지: 신호 수준 > 99% 포화
  I_raw[y][x] = 65535 (14-bit max)
  
예상 동작:
  ├─ Offset 보정: clamped to 65535 (언더플로우 없음)
  ├─ Gain 보정: 포화 픽셀 처리 (ID 유지 또는 flag)
  └─ Lag 보정: 포화는 lag 계산에 제외

기대 결과:
  포화 픽셀 map: identified, masked in output
  PASS: 포화 픽셀 < 1%
```

**테스트 파일**: `edge_case_extreme_saturation.raw`

---

#### 7.1.2 극단적 검은색 (Underflow)

```
테스트 케이스 7.1.2: ADC 최저값

이미지: 모든 픽셀 = 0
  I_raw[y][x] = 0
  
예상 동작:
  ├─ Offset 보정: I_raw - I_dark = 0 - 100 = -100 → clamp to 0
  ├─ Gain 보정: 0/G = 0 (유지)
  └─ Lag 보정: 신호 0 → lag 0

기대 결과:
  출력: 모두 0 (또는 noise floor)
  PASS: underflow flag set correctly
```

**테스트 파일**: `edge_case_all_black.raw`

---

### 7.2 NaN/INF 처리

#### 7.2.1 NaN 입력 (부동소수점 오류)

```
테스트 케이스 7.2.1: NaN 픽셀

상황: 산술 오류로 일부 픽셀이 NaN
  I_raw[100][100] = NaN

예상 동작:
  ├─ 감지: NaN check during offset correction
  ├─ 처리: Replace with median of neighbors OR 0
  └─ 로깅: NaN pixel count reported

기대 결과:
  NaN pixels handled gracefully
  Output: valid image (no NaN propagation)
  PASS: NaN count < 10 pixels/frame
```

**테스트 파일**: `edge_case_nan_pixels.raw`

---

### 7.3 NULL 포인터 입력 (FR-204)

```
테스트 케이스 7.3.1: NULL 포인터

입력:
  └─ pFrame = NULL
  
예상 동작:
  xpe_ghost_correct(NULL, ...) → XPE_ERR_NULL_PTR
  
기대 결과:
  ├─ 에러 코드 반환 (abort, corruption 없음)
  ├─ 로깅: "Invalid input pointer"
  └─ 이전 상태 유지 (side effect 없음)

PASS: 정확한 에러 코드 반환
```

---

### 7.4 History Buffer Overflow

```
테스트 케이스 7.4.1: ExposureHistory 초과

상황: 16 frame 초과 노출 기록
  history_count = 17 (MAX = 16)
  
예상 동작:
  ├─ 감지: count check
  ├─ 처리: 오래된 것부터 제거 (ring buffer)
  └─ 사용: 최신 16개만 유효

기대 결과:
  PASS: 가장 오래된 프레임 자동 제거
  사용 가능한 history = 최신 16개
```

---

## 8. 성능 테스트 데이터셋

### 8.1 성능 예산

```
처리 시간 목표:
  Tier 1 + 2: < 70ms (3072×3072 float32 프레임)
  Tier 3: < 200ms (전체, 상태 관리 포함)
  
메모리:
  ExposureHistory: ~150 MB (16 프레임 × 2.8 MB/frame)
  LUT: ~5 MB
  임시 버퍼: ~30 MB
  ───────────
  합계: ~185 MB (허용 범위: 200 MB)
```

### 8.2 성능 테스트 데이터셋

```
File: test/data/performance/

구성:
  ├─ perf_3072x3072_float32_batch10.raw
  │   (3072×3072 × 10 프레임, float32)
  │   크기: 360 MB
  │
  ├─ perf_3072x3072_uint16_batch10.raw
  │   (3072×3072 × 10 프레임, uint16)
  │   크기: 180 MB
  │
  └─ perf_metadata.json
      └─ 기대 처리 시간: 70ms (Tier 1+2), 200ms (Tier 3)

테스트 절차:
  1. 프레임 로드
  2. 10회 반복 처리 (warmup + measurement)
  3. 평균 시간 계산
  4. 기대값과 비교
  
PASS 기준:
  평균 시간 <= 예산
  편차 (std) < 20%
```

---

## 9. 데이터셋 저장 및 추적

### 9.1 디렉토리 구조

```
docs/ghost-correction/test_data/
├─ README.md                  (이 파일)
├─ manifest.yaml             (모든 DS의 메타데이터)
│
├─ synthetic/
│   ├─ tier2_simple_n4.raw
│   ├─ tier3_nlcsc_3levels.raw
│   ├─ first_frame_lag.raw
│   ├─ 50th_frame_lag.raw
│   └─ generator.py          (생성 스크립트)
│
├─ semi_synthetic/
│   ├─ nlcsc_weak.raw
│   ├─ nlcsc_strong.raw
│   ├─ nlcsc_extreme.raw
│   ├─ power_on.raw
│   ├─ patient_change.raw
│   └─ generator.py
│
├─ real/
│   ├─ fsrf/
│   │   ├─ signal_02pct/
│   │   ├─ ... (9 수준)
│   │   └─ signal_92pct/
│   │
│   ├─ rsrf/
│   │   ├─ signal_02pct/
│   │   ├─ ... (9 수준)
│   │   └─ signal_92pct/
│   │
│   ├─ cbct_pelvis/
│   │   ├─ uncorrected.raw
│   │   ├─ tier1.raw
│   │   ├─ tier2.raw
│   │   └─ tier3.raw
│   │
│   └─ cbct_head/
│       ├─ uncorrected.raw
│       ├─ tier1.raw
│       ├─ tier2.raw
│       └─ tier3.raw
│
├─ edge_cases/
│   ├─ extreme_saturation.raw
│   ├─ all_black.raw
│   ├─ nan_pixels.raw
│   └─ null_pointer_test.cpp
│
├─ performance/
│   ├─ batch10_3072x3072_float32.raw
│   ├─ batch10_3072x3072_uint16.raw
│   └─ performance_results.json
│
└─ golden_reference/
    ├─ hashes.txt            (SHA-256 모든 파일)
    ├─ starman_et_al_2012.pdf
    └─ criteria.yaml         (기대값 정의)
```

### 9.2 메타데이터 파일 (manifest.yaml)

```yaml
version: 1.0.0
last_updated: 2026-04-14
total_datasets: 27

datasets:
  synthetic_tier2_simple_n4:
    path: synthetic/tier2_simple_n4.raw
    size_mb: 2.4
    frames: 300
    source: generated
    model: N=4 exponential
    parameters:
      b: [50, 15, 5, 1]
      a: [0.2, 0.05, 0.01, 0.002]
      noise_sigma_adu: 10
    expected_output:
      rmse_percent: < 1
      first_frame_lag: < 0.3%
      fiftieth_frame_lag: < 0.01%
    sha256: 1a2b3c4d5e6f...
    
  real_fsrf_signal_50pct:
    path: real/fsrf/signal_50pct/
    size_mb: 8.5
    frames: 420
    source: iap_ghost_001_protocol
    acquisition_date: 2026-03-15
    detector: AUO_R1717
    detector_temp_c: 25.0
    signal_level_adu: 3276
    signal_saturation_percent: 50
    expected_output:
      first_frame_lag_percent: < 0.25
      exponential_fit_r2: > 0.98
    sha256: [dark], [pre], [post] hashes...
    
  ... (모든 데이터셋)
```

---

## 10. 테스트 실행 체크리스트

### 10.1 회귀 테스트 체크리스트

```
회귀 테스트 (모든 PR/커밋 전)
═════════════════════════════

합성 데이터 테스트
[ ] synthetic_tier2_simple_n4
    └─ RMSE < 1%, R² > 0.99
[ ] synthetic_tier3_nlcsc_3levels
    └─ RMSE < 2%, E-의존성 제거 확인
[ ] synthetic_first_frame_lag
    └─ 1st frame lag < 0.3%

반합성 데이터 테스트
[ ] semi_synthetic_nlcsc_weak
    └─ RMSE < 1%
[ ] semi_synthetic_nlcsc_strong
    └─ RMSE < 2%
[ ] semi_synthetic_power_on
    └─ 1st frame bypass 확인, 2nd frame < 2%

실제 데이터 테스트
[ ] real_fsrf_signal_50pct
    └─ RMSE < 0.25%, 골든 기준값 비교
[ ] real_rbct_pelvis
    └─ HU bias < 15 (Tier 2)
[ ] real_cbct_head
    └─ HU bias < 5 (Tier 3)

엣지 케이스
[ ] edge_case_extreme_saturation
    └─ 포화 < 1%, no corruption
[ ] edge_case_all_black
    └─ underflow 처리 OK
[ ] edge_case_nan_pixels
    └─ NaN propagation 없음

성능 테스트
[ ] perf_batch10_float32
    └─ Tier 1+2: < 70ms, Tier 3: < 200ms
[ ] perf_memory
    └─ < 200 MB
```

---

## 11. 참고문헌

### 11.1 주요 문헌

| 논문 | 주제 | 기대값 출처 |
|-----|------|----------|
| **Starman et al. 2012** | NLCSC 래그 보정 | 1st frame < 0.29%, 50th < 0.0052% |
| **Pang 2006** | a-Si 검출기 특성 | 지수 시간상수 모델 |
| **Siewerdsen & Jaffray 1999** | FPD 래그 물리학 | 온도/신호 의존성 |
| **Zhao 2004** | 온도 보상 | 온도 계수 |

### 11.2 관련 문서

| 문서 | 역할 |
|-----|------|
| **SRS-GHOST-001** | 소프트웨어 요구 정의 |
| **STP-STC** | 테스트 계획 및 경우 |
| **RTM** | 요구 → 테스트 추적성 |
| **IAP-GHOST-001** | 데이터 취득 방법론 |

---

**문서 끝**

**승인**: [성명]  
**날짜**: 2026-04-14  
**버전**: 1.0.0 (최종)

---

*본 문서는 IEC 62304 Class B 검증 데이터 명세 표준을 따릅니다.*

*마지막 갱신: 2026-04-14*
