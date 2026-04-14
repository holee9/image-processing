# XPE 모듈 보강 계획: 세계 수준의 엔진 설계

**문서 ID**: XPE-REINFORCE-001
**버전**: 1.0.0
**날짜**: 2026-04-14
**상태**: 초안 -- 검토 대기 중
**작성자**: MoAI (DeepResearch + DeepThink + 브레인스토밍)
**분류**: 계획 문서 (규범적 아님)
**범위**: 전처리 (9 SWU) + 후처리 (24 SWU) + 혁신 로드맵
**참고**: ALG-SPEC-001 v3.0.0-ds2, PIPE-SPEC-001 v1.3.0, SPEC-XPE-MASTER v2.0.0

---

## 실행 요약

3개 병렬 분석(전처리 심층 연구, 후처리 심층 연구, 혁신 브레인스토밍)의 결과를 통합한 극강 엔진 설계 보강 계획. 총 **23개 전처리 개선**, **35개 후처리 개선**, **78개 혁신 아이디어**에서 도출된 핵심 보강 항목.

### 주요 발견 사항

| 영역 | 현재 수준 | 목표 수준 | 핵심 갭 |
|------|----------|----------|---------|
| 전처리 (Pre-Processing) | 양호 (NLCSC, FixPix 업계 선도) | 극강 (SIMD 2배 성능, 자동 캘리브레이션) | 성능 최적화, 캘리브레이션 자동화 |
| 후처리 (Post-Processing) | 기본 설계 완료 | Agfa MUSICA Xpert급 | 통합 멀티스케일, AI 적응형 처리 |
| AI 모듈 (AI Module) | 아키텍처 정의됨 | 실시간 해부학 인식 처리 | 모델 학습 데이터, INT8 양자화 |
| 격자 억제 (GSVG) | DWT 기반 설계 | 물리 그리드 대체 수준 | DL 산란 추정, CNR 향상 |
| 표시/DICOM (Display/DICOM) | 표준 준수 설계 | HDR + 초해상도 + 클라우드 통합 | GSDF 자동화, DICOMweb |

---

## Part I: 전처리 모듈 보강 (xpe_preprocess.dll)

### 1. 보정 관리자 (CalibManager) (SWU-1.5) -- Stage 0

#### 현재 스펙 vs 보강 목표

| 항목 | 현재 | 보강 목표 |
|------|------|----------|
| 로딩 시간 | 전체 200ms | < 80ms (필수 맵) + < 150ms (비동기 지연 로딩) |
| 무결성 검증 | CRC-32 | SHA-256 해시 체인 (offset+gain+BPM 세션 그룹화) |
| 캐싱 전략 | 정의되지 않음 | 3-계층: DRAM L1 -> NVMe L2 -> 네트워크 L3 |
| 세션 검증 | 정의되지 않음 | 동일 캘리브레이션 세션 검증 (session_id 매칭) |
| 드리프트 감지 | 시간+온도 임계값 | 복합 점수 (age*w1 + temp_delta*w2 + usage_frames*w3) |

#### 세분화된 서브 모듈

```
calib_registry_init        -> 파일 레지스트리 초기화
calib_map_locate           -> 유형+시리얼 기반 파일 위치 검색
calib_integrity_verify     -> SHA-256 + CRC 검증 (병렬 처리 가능)
calib_header_parse         -> 메타데이터 헤더 파싱
calib_session_validate     -> offset/gain/BPM 세션 일치 확인
calib_expiry_check         -> 만료 + 드리프트 점수 + 사용량 확인
calib_map_deserialize      -> 바이너리 역직렬화 (메모리 매핑 I/O)
calib_hot_cache_insert     -> DRAM L1 캐시 삽입 (LRU)
calib_deferred_loader      -> NLCSC, 비선형 LUT 비동기 로딩
```

#### 캘리브레이션 파일 포맷 설계

```
[Header: 256 bytes]
  magic:            uint32  "XCal"
  version:          uint16  major.minor
  type:             uint8   (0=offset, 1=gain, 2=BPM, 3=nonlinearity, 4=NLCSC, 5=binning)
  detector_serial:  char[32]
  session_id:       uint64  (offset+gain+BPM 연결)
  created_at:       int64   (Unix epoch seconds)
  expires_at:       int64   (Unix epoch seconds, 0=never)
  kVp:              float32
  mAs:              float32
  SID_mm:           float32
  temperature_C:    float32
  frame_count:      uint32  (평균화된 프레임 수)
  width:            uint32
  height:           uint32
  pixel_format:     uint8   (0=uint16, 1=float32, 2=float64)
  compression:      uint8   (0=none, 1=LZ4, 2=ZSTD)
  payload_size:     uint64
  sha256_payload:   uint8[32]
  sha256_header:    uint8[32]
  reserved:         pad to 256

[Payload: variable]
  calibration map data
  optional: per-pixel confidence scores
```

#### 자가 치유 캘리브레이션 전략

1. **런타임 드리프트 감지**: 매 프레임의 collimation 외부 영역 통계로 offset 드리프트 추정
2. **Kalman filter 온라인 보정**: 지수 이동 평균(EMA) 기반 dark map 연속 갱신
3. **예측적 재캘리브레이션**: ML 기반 캘리브레이션 열화 예측 (온도 이력, 노출 횟수, 결함 픽셀 추이)

---

### 2. Readout Artifact Validation (SWU-1.9) -- Stage 0.5

#### 보강 목표

| 항목 | 현재 | 보강 |
|------|------|------|
| 감지 카테고리 | ~4종 (포화, 고착 행/열, DR, 기하) | 8+종 (+ 주기적 노이즈, 히스토그램 이상, AED 불일치, EMI) |
| 위양성률 | 미정의 | < 0.1% |
| 처리 시간 | < 15ms | < 10ms (AVX2 가속) |

#### 추가 검사 항목

| 서브 함수 | 목적 | SIMD 가능 |
|-----------|------|:---------:|
| `readout_check_periodic_noise` | FFT 기반 주기적 패턴 감지 (ADC 클록 블리드) | FFTW |
| `readout_check_histogram` | 히스토그램 형상 분석 (첨도, 왜도, 다봉성) | AVX2 |
| `readout_check_aed_consistency` | AED 트리거 상태 vs 프레임 내용 교차 검증 | 스칼라 |
| `readout_aggregate_score` | 복합 아티팩트 점수 산출 | 스칼라 |

---

### 3. Temperature Compensation (SWU-1.6) -- Stage 0.7

#### 보강 목표

| 항목 | 현재 | 보강 |
|------|------|------|
| 온도 범위 | 15-40C | 10-45C (휴대용 야외 사용 확장) |
| 공간 모델 | 단일 센서 균일 가정 | 다중 센서 융합 -> 2D 온도 필드 |
| 보정 잔차 | 미정량화 | < 2 ADU RMS |
| 예측 추적 | 미정의 | 열 시상수 기반 다음 프레임 예측 |

#### 세분화 서브 모듈

```
temp_sensor_fuse           -> 4-8개 NTC 센서 -> 2D 온도 필드 (bilinear 보간)
temp_model_exponential     -> 아레니우스 암전류 모델 (픽셀 영역별)
temp_drift_predict         -> 온도 궤적 기반 다음 프레임 예측
temp_power_mode_adjust     -> sleep/standby/active 전환 모델
temp_correction_map_gen    -> 0.5C 간격 사전계산 맵 보간
```

#### 성능 최적화

- **사전계산 보정 맵**: 0.5C 간격으로 작동 범위 내 보정 맵 사전 생성. 런타임에는 인접 2개 맵 보간만 수행
- **맵 재사용**: 이전 프레임 대비 온도 변화 < 0.1C이면 이전 보정 맵 그대로 사용
- **AVX2 지수 근사**: Remez 다항식으로 `exp()` 4x 가속

---

### 4. Offset Correction (SWU-1.1) -- Stage 1

#### 보강 목표

| 항목 | 현재 | 보강 |
|------|------|------|
| 잔여 dark bias | < 5 ADU mean | < 2 ADU mean, < 3 ADU sigma |
| 처리 시간 | < 55ms | < 25ms (AVX2) |
| 온도 안정성 | +/-2.5C | +/-5C (다점 보간) |
| 갱신 전략 | 30분 임계값 | 연속 EMA + 예측적 스케줄링 |

#### 핵심 보강

1. **이중 시상수 PREP-time 모델**: `m(t) = x1*exp(x2*t+x3) + x4*exp(x5*t)` (장시간 PREP 대응)
2. **재귀적 dark 평균화 (EMA)**: `D_new = alpha*D_current + (1-alpha)*D_measured` (비차단 연속 갱신)
3. **주파수 분해 강화**: 적응형 커널 크기 기반 LF/HF 분리
4. **AVX2 포화 감산**: `_mm256_subs_epu16`로 하드웨어 레벨 floor-at-zero

---

### 5. Nonlinearity Correction (SWU-1.7) -- Stage 1.5

#### 보강 목표

| 항목 | 현재 | 보강 |
|------|------|------|
| 잔여 비선형성 | 미정량화 | < 0.5% (10-90% 범위) |
| 캘리브레이션 점 | 2점 (TPC 최소) | 5+점 (MPC, 고정밀 검출기) |
| 응답 곡선 저장 | 미정의 | 16x16 블록별 3계수 다항식 |
| kVp 의존성 | 미정의 | kVp별 LUT 패밀리 (NEW) |
| 처리 시간 | < 25ms | < 10ms (LUT) / < 15ms (다항식) |

#### 핵심 보강: kVp-의존 비선형성 보정

일부 a-Si 검출기는 60kVp vs 120kVp에서 2-3% 다른 비선형성을 보임. kVp별 LUT 패밀리로 보정.

```
nonlin_gain_mode_select    -> 현재 gain 모드에 따른 LUT/계수 선택
nonlin_kvp_family_select   -> kVp에 따른 LUT 패밀리 선택 (NEW)
nonlin_poly_apply          -> Horner 방법: c0 + x*(c1 + x*c2), AVX2 FMA 2회
nonlin_monotonicity_enforce -> 보정 후 단조성 보장 (등장 회귀)
```

---

### 6. Gain Correction (SWU-1.2) -- Stage 2

#### 보강 목표 (FORMAT BOUNDARY: uint16->float32)

| 항목 | 현재 | 보강 |
|------|------|------|
| Flat-field 잔차 | sigma/mean < 1% (80% FOV) | < 0.5% (90% FOV) |
| DQE 열화 | < 5% | < 3% |
| 처리 시간 | < 55ms | < 20ms (역수 곱셈 + AVX2 + 다중 스레드) |
| Heel effect | 80% RMSE 감소 | 90% (반복 개선) |
| 다중 게인 레벨 | 5+ | 8-10 (Varex급) |

#### 핵심 보강: Cross-Talk 보정 (NEW)

현재 스펙에 없음. Agfa, Siemens 시스템은 포함.

```
gain_crosstalk_deconv      -> 3x3 디컨볼루션 커널로 인접 픽셀 크로스톡 제거
gain_reciprocal_precompute -> 1.0f / G(x,y) 사전 계산 (나눗셈 -> 곱셈 3-5x 가속)
gain_validate_output       -> NaN/Inf/음수 검증 (AVX2 비교)
```

#### 융합 타입 변환 + 정규화

```c
// 단일 패스에서 타입 변환 + gain 정규화 수행
// memory traffic 50% 감소
for (tile) {
    __m256i u16 = _mm256_loadu_si256(raw_ptr);
    __m256 f32 = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16));
    __m256 recip_gain = _mm256_load_ps(reciprocal_gain_ptr);
    __m256 result = _mm256_mul_ps(f32, recip_gain);
    _mm256_store_ps(output_ptr, result);
}
```

---

### 7. Binning Correction (SWU-1.8) -- Stage 2.5

#### 보강: 서브픽셀 보정

- 2x2 binning 시 실효 픽셀 응답 함수(aperture function) 변화를 디컨볼루션 커널로 보상
- 분수 binning(1.5x) 지원 (보간 기반)
- 다운스트림 파라미터 자동 조정 (결함 맵 좌표 리매핑, 노이즈 감소 강도)

---

### 8. Defect Correction (SWU-1.3) -- Stage 3

#### 보강 목표

| 항목 | 현재 | 보강 |
|------|------|------|
| 격리 픽셀 NMSE | 미정량화 | FixPix 수준 14.2x 개선 |
| 클러스터 MSE | 미정량화 | < 100 MSE (5x5 클러스터) |
| 처리 시간 | < 95ms | < 60ms (baseline) / < 80ms (MLP) |
| 검출 lambda | 8.0 고정 | 적응형 lambda (지역 통계 기반) |
| 위양성률 | 미정의 | < 0.001% |

#### 핵심 보강: 적응형 결함 분류 + 진화 추적

```
defect_classify            -> 결함 유형별 분류: 격리/라인/클러스터/핫/콜드/깜빡임
defect_interpolate_isolated -> 가장자리 인지 쌍선형 (AVX2 이웃 수집 + 가중 평균)
defect_interpolate_line    -> 방향성 보간 (행/열 결함용)
defect_interpolate_cluster -> 중앙값 폴백 (2x2+ 그룹)
defect_mlp_repair          -> FixPix MLP (5x5 패치, 1425 파라미터, AVX2 밀집 행렬곱)
defect_evolution_track     -> 시간에 따른 결함 모집단 변화 추적 (예측적 유지보수)
defect_auto_promote        -> N회 연속 감지된 일시적 결함을 정적 BPM으로 자동 승격 (NEW)
```

#### 희소 처리 최적화

일반적 0.1% 결함률에서 3072x3072 중 ~9,000 픽셀만 처리. 깨끗한 픽셀은 완전 스킵.

---

### 9. Ghost/Lag Correction (SWU-1.4) -- Stage 4

#### 현재 상태: **업계 선도** (3-tier NLCSC)

현재 스펙은 이미 최신 연구 수준. 보강은 점진적 개선에 집중.

#### 보강 목표

| 항목 | 현재 | 보강 |
|------|------|------|
| 1차 프레임 lag (Tier 3) | <= 0.29% | <= 0.25% (적응형 계수) |
| 50차 프레임 lag | <= 0.0052% | <= 0.004% |
| 히스토리 버퍼 | 8 프레임 고정 | 4-16 프레임 설정 가능 |
| 티어 선택 | 고정 임계값 | 베이지안 결정 프레임워크 (NEW) |
| 영역별 티어 | 미정의 | 영역별 다른 티어 할당 (NEW) |

#### 연구 추적 항목

- **Lag-Net (2025)**: CNN 기반 lag 보정. 학습 데이터 획득에 하드웨어 수정 필요. 연구 경로로 추적.
- **자가 캘리브레이션**: 잔여 lag가 예상 초과 시 자동 재캘리브레이션 트리거

---

### Pre-Processing 성능 예산 요약 (보강 목표)

| Stage | 현재 할당 (ms) | 현재 추정 (ms) | 보강 목표 (ms) | SIMD 속도향상 |
|:-----:|:--------------:|:--------------:|:-------------:|:------------:|
| (0) CalibManager | 200 (시작) | 200 | 80 (필수) | 1.5x (병렬 I/O) |
| (0.5) Readout | 15 | 10 | 8 | 2x (AVX2 리덕션) |
| (0.7) Temp Comp | 10 | 5 | 3 | 3x (맵 재사용 + AVX2) |
| (1) Offset | 60 | 55 | 20 | 4x (AVX2 subs_epu16) |
| (1.5) Nonlinearity | 25 | 20 | 8 | 3x (LUT gather/FMA) |
| (2) Gain | 60 | 55 | 18 | 4x (역수 곱셈) |
| (2.5) Binning | 15 | 10 | 3 | 3x (단순 곱셈) |
| (3) Defect | 110 | 95 | 50 | 2x (희소 + MLP 벡터화) |
| (4) Ghost Tier 1 | 150 | 140 | 90 | 2x (FMA 체인) |
| **합계 Tier 1** | **500** | **~390** | **~200** | **~2x 전체** |

---

## Part II: 후처리 모듈 보강

### 10. 향상 파이프라인 혁신: 통합 멀티해상도 프레임워크

#### 핵심 개념: 단일 Laplacian 피라미드 공유

현재: 4개 스테이지가 순차적으로 독립 실행 (Log -> Noise -> Contrast -> Edge)
보강: **단일 Laplacian 피라미드 분해**를 4개 처리에서 공유

```
Input float32 image
    |
    v
[Laplacian Pyramid Decomposition] (8 levels)
    |
    +-> Level 0-1 (최고해상도): Edge enhancement + HF 노이즈 제거
    +-> Level 2-4 (중간): Contrast enhancement (CLAHE 등가)
    +-> Level 5-7 (저해상도): Anatomy / 다중스케일 처리
    +-> Residual (DC): 전체 밝기 정규화
    |
[Per-Level Processing] -- body part + 선량 기반 파라미터
    |
[Reconstruction]
    |
    v
Output enhanced image
```

**장점**:
- 1회 분해로 4개 처리 수행 (3배 계산 절감)
- 스테이지 간 아티팩트 누적 제거
- 자연스러운 주파수 대역별 노이즈 게이팅
- 통합 파라미터 모델: 레벨당 이득(gain) 벡터 하나로 모든 처리 제어

**성능 추정**: 통합 멀티해상도 < 250ms (현재 순차 체인: 170+120+80 = 370ms)

#### 구현 로드맵

| Phase | 접근 방법 |
|:-----:|----------|
| 1b | 공간 도메인 처리 (bilateral, CLAHE, USM) -- 단순, 신뢰, 검증됨 |
| 2 | 통합 Laplacian 피라미드로 전환 (multiscale + fractional과 통합) |
| 3+ | 전체 주파수 도메인 파이프라인 (공유 분해) |

---

### 11. 3-Tier 적응형 파라미터 선택 시스템 (NEW)

#### 아키텍처

```
Tier 1: Exam Profile (정적)
  입력: DICOM bodyPart + examView
  출력: 기본 파라미터 세트 (JSON 프로파일 DB)
  
Tier 2: Image Statistics (동적)
  입력: 영상 통계 (mean, sigma, SNR, histogram entropy)
  출력: 파라미터 조정 계수
  
Tier 3: AI Refinement (Phase 3)
  입력: Body Part 분류 + confidence
  출력: 해부학 영역별 미세 조정 파라미터
```

#### 파라미터 흐름 테이블

| 파라미터 | Tier 1 | Tier 2 조정 | Tier 3 AI |
|---------|--------|------------|----------|
| `noise_strength` | 부위 프로파일 | x (sigma/sigma_ref) | 영역별 가중치 |
| `clahe_clip_limit` | 검사 유형 | 히스토그램 첨도 보정 | 해부학 인지 clip map |
| `edge_amount` | 부위 | 추정 MTF 스케일링 | 뼈 vs 연조직 영역 |
| `multiscale_gains[0..7]` | 검사/작업 프로파일 | 레벨별 노이즈 게이팅 | AI 작업 프리셋 |
| `log_base` | 고정 (10.0) | 동적 범위 적응 | -- |

#### 자동 튜닝 결정 테이블

| 조건 | 동작 |
|------|------|
| SNR < 10 | NLM 전환, edge 50% 감소, HF 다중스케일 대역 억제 |
| SNR 10-30 | 표준 bilateral, 정상 edge, 표준 다중스케일 |
| SNR > 30 | 최소 노이즈 감소, 공격적 edge, 전체 다중스케일 |
| DI < -3 (저선량) | 알림 + 공격적 대비 부스트 + 노이즈 감소 우선 |
| 엔트로피 < 4.0 | CLAHE clip 2x 증가, 중주파 다중스케일 게인 부스트 |

---

### 12. Noise Reduction (SWU-2.2) 보강

#### 보강 목표

| 기능 | 현재 | 보강 |
|------|------|------|
| 방법 | Bilateral/NLM/Wavelet | + BM3D + Guided Filter + 주파수 선택적 |
| 선량 적응 | 미정의 | EI 기반 자동 강도 조절 (Fujifilm FNC 동등) |
| 주파수 선택 | 미정의 | Wavelet 도메인: HF 공격적, LF 보존 |
| MTF 보존 | 설정 가능 허용치 | >= 98% at Nyquist/2 |

#### 핵심 연구 결과

[벤치마킹 연구 (Eulig et al. 2024)](https://aapm.onlinelibrary.wiley.com/doi/10.1002/mp.17379): RED-CNN(초기 DL 방법 중 하나)이 표준화된 메트릭에서 최신 아키텍처와 동등. DnCNN/RED-CNN 선택의 실용적 타당성 검증.

---

### 13. Contrast Enhancement (SWU-2.3) 보강

#### 보강: Multiscale CLAHE

- Laplacian 피라미드 레벨별 CLAHE (레벨 특정 clip limit)
- Body-part 적응형 clip limit
- 히스토그램 명세화: "이상적" 검사 기준 히스토그램 형상으로 매칭
- 안티-아티팩트 가드: CLAHE 출력의 overshoot 감지 + 자동 clip 감소

---

### 14. Edge Enhancement (SWU-2.4) 보강

#### 보강: Phase-Preserving Sharpening

- **다중 대역 USM**: Laplacian 피라미드 레벨별 별도 sharpening
- **위상 보존 sharpening**: Monogenic 신호 프레임워크 (링잉 아티팩트 제거)
- **방향 선택적 sharpening**: Steerable 필터로 골소주 세부 강조
- **백분위 기반 overshoot 클램핑**: 구배 크기의 P1/P99 기반 (현재 limiter 개선)

---

### 15. GSVG (SI-001~004) Deep Enhancement

#### 보강 로드맵

| Phase | 접근 | 성능 목표 |
|:-----:|------|----------|
| 2 (현재) | DWT 노치 필터 + 적응형 대역폭 | CNR >= 0.9x (6:1 물리 그리드) |
| 2+ | + 희소 사전 방법 (프리미엄 옵션) | CNR >= 1.0x |
| 3 | + CNN 기반 억제 (AI worker) | CNR >= 1.2x (6:1~12:1 사이) |
| 3+ | + DL 산란 추정 (DSE U-Net) | 물리 12:1 그리드 대체 |

#### Virtual Grid 품질 비교 목표

| 메트릭 | 물리 6:1 Grid | 물리 12:1 Grid | 현재 Virtual Grid | 보강 Virtual Grid |
|--------|:-------------:|:--------------:|:-----------------:|:----------------:|
| CNR | 1.0x (기준) | ~1.5x | >= 0.9x | **>= 1.2x** |
| MTF 손실 | -5~-10% | -8~-15% | <= -5% | **<= -3%** |
| 산란 감소 | 60-80% | 80-95% | 50-70% | **70-85%** |
| 선량 영향 | +2-3x | +3-5x | **0x** | **0x** |

**핵심 차별화**: Virtual Grid로 물리 그리드 선량 패널티 제거 + Phase 3에서 DL 산란 추정으로 12:1 그리드급 품질 접근.

---

### 16. AI Module Architecture 보강

#### 모델 선택 및 최적화

| AI 작업 | 선택 모델 | ONNX INT8 크기 | 추론 시간 | 대체 모델 |
|---------|----------|:--------------:|:---------:|----------|
| Body Part | MobileNet-v3-Small | ~2 MB | < 10ms | EfficientNet-B0 |
| Bone Suppression | Residual U-Net | ~50 MB | < 300ms | xU-NetFullSharp |
| DL Denoiser | DnCNN (17-layer) | ~5 MB | < 150ms | RED-CNN |
| Scatter Estimation | DSE U-Net | ~30 MB | < 200ms | LUT 폴백 |
| Collimation (AI) | U-Net (256x256) | ~15 MB | < 50ms | Hough 폴백 |
| Defect (ML) | FixPix MLP | < 0.1 MB | < 5ms | 쌍선형 폴백 |

#### ONNX Runtime 최적화 전략

1. **INT8 양자화**: 모델 크기 -75%, 추론 2-4x 가속 (캘리브레이션 데이터셋 100+ 필요)
2. **FP16 혼합 정밀도**: GPU에서 2x 가속, < 1% 정확도 손실
3. **연산자 융합**: 10-30% 지연 감소 (자동)
4. **공유 메모리 IPC**: mmap으로 프록시-워커 간 직렬화 제거

---

### 17. Display Processing (SWU-3.1~3.4) 보강

#### HDR 톤 매핑 for X-ray (NEW)

| 기법 | 설명 | 용도 |
|------|------|------|
| 전역 톤 매핑 | Reinhard 스타일 X-ray 적응 | 전체 동적 범위 보존 |
| 지역 톤 매핑 | Durand bilateral 분해 | 넓은 범위에서 최대 세부 가시성 |
| 이중 창 표시 | 두 개의 VOI 창 동시 표시 (예: 뼈+연조직) | 흉부 판독 워크플로우 |
| 다중 노출 융합 | Mertens 노출 융합 | 전체 범위에서 최대 세부 |

#### GSDF 자동 준수 검증 강화

| 검사 항목 | 통과 기준 |
|----------|----------|
| JND 선형성 | 18개 GSDF 기준 레벨에서 모두 +/-10% 이내 |
| Lmin/Lmax | 진단: Lmin <= 1.0, Lmax >= 350 cd/m2 |
| 주변광 비율 | Lmin의 10% 미만 |
| 균일성 | >= 70% (중앙 vs 최악 모서리) |

---

### 18. DICOM Integration (SWU-4.1~4.4) 보강

#### DICOM Structured Reporting 통합

| SR 유형 | 설명 | 우선순위 |
|---------|------|:--------:|
| 처리 로그 SR | 적용 스테이지, 파라미터, bypass 결정 | 중 |
| 선량 SR (RDSR) | IHE REM 프로파일, DAP/EI/DI | **높음** |
| QC SR | AAPM TG-151 검정 결과 | 중 |
| AI SR | AI 결과, 신뢰도, 뼈 억제 품질 | 낮음 |

#### DICOMweb 로드맵

| Phase | 기능 |
|:-----:|------|
| 1b | DICOM 전통 (C-STORE, C-FIND) -- API 구현 |
| 2 | DICOMweb STOW-RS (대체 저장 경로) |
| 3+ | 전체 DICOMweb (WADO-RS, QIDO-RS) + FHIR 통합 |

---

## Part III: 혁신 로드맵 -- 킬러 기능

### 19. 7대 킬러 기능

| # | 기능명 | 핵심 가치 | Phase | 영향도 |
|---|--------|----------|:-----:|:------:|
| K1 | **OneClick** (원클릭) | 촬영 버튼 하나로 최적 영상 완성 | 3 | 5/5 |
| K2 | **InstaQC** (즉시 QC) | 촬영 직후 1초 이내 진단 적합성 자동 판정 | 2 | 5/5 |
| K3 | **NanoGrid** (나노그리드) | 소프트웨어 가상 격자로 물리 격자 대체 (선량 30% 절감) | 2 | 5/5 |
| K4 | **UniversalLook** (범용 화질) | 검출기 무관 일관된 진단 화질 보장 | 2 | 5/5 |
| K5 | **DoseGuard** (선량 보호) | 선량 최적화 자동 파일럿 (ALARA) | 3 | 5/5 |
| K6 | **DetectorIQ** (검출기 지능형) | 검출기 지능형 자가진단 + 예측적 유지보수 | 3 | 4/5 |
| K7 | **Audit Trail** (감사 기록) | 완전한 처리 파이프라인 추적성 (IEC 62304) | 1 | 5/5 |

---

### 20. Phase별 구현 로드맵

#### Phase 1 (즉시 구현 가능)

| 아이디어 | 영역 | 핵심 가치 |
|---------|------|----------|
| Zero-Allocation Memory Pool | Performance | 파이프라인 내 malloc/free 제거 |
| Tile-Based Processing | Performance | L2 캐시 최적화로 병렬성 극대화 |
| SIMD Instruction Cascade | Architecture | SSE4.2->AVX2->AVX-512 런타임 자동 선택 |
| Pipeline Audit Trail | QA | DICOM Private Tag으로 전 스테이지 추적 |
| IEC 62494-1 Auto Compliance | QA | EI/DI 자동 검증 및 보고 |
| DICOM SR Auto Generation | Clinical | 처리 메타데이터 구조화 보고서 |
| Async I/O Pipeline | Performance | DICOM Write 비동기화로 I/O 병목 제거 |

#### Phase 2 (핵심 차별화)

| 아이디어 | 영역 | 핵심 가치 |
|---------|------|----------|
| Dose-Aware Adaptive Processing | AI/Clinical | EI 기반 파라미터 자동 조절 |
| Self-Calibrating Pipeline | Innovation | 매 프레임 캘리브레이션 드리프트 자동 보정 |
| No-Reference Real-Time IQA | Innovation | 실시간 영상 품질 점수 (InstaQC 기반) |
| NanoGrid (GSVG 강화) | Killer | DL 산란 추정으로 물리 그리드 대체 |
| Multi-Detector Normalization | Clinical | 검출기 간 일관된 화질 (UniversalLook) |
| Plugin Architecture | Architecture | 서드파티 알고리즘 런타임 삽입 |
| Automated Constancy Test | QA | AAPM TG-151 자동 QC |
| Unified Multi-Resolution Pipeline | Processing | 단일 피라미드 공유로 30% 성능 향상 |

#### Phase 3 (AI 혁신)

| 아이디어 | 영역 | 핵심 가치 |
|---------|------|----------|
| Anatomy-Aware Region Processing | AI | 해부학 영역별 개별 파라미터 |
| Pathology-Preserving Enhancement | AI | 병변 영역 보존적 처리 |
| OneClick (완전 자동) | Killer | 파라미터 조절 제로 |
| ALARA Dose Advisor | Clinical | 최적 선량 추천 |
| Repeat/Reject Analysis | Clinical | 재촬영 원인 자동 분류 |

#### Phase 4+ (미래 연구)

| 아이디어 | 영역 | 핵심 가치 |
|---------|------|----------|
| Computational Flat-Field | Innovation | HDR 스태킹으로 저선량 합성 |
| Federated Learning | AI | 설치 기지 전체 모델 지속 향상 |
| TimeWarp (시간 변화 감지) | Killer | 과거-현재 영상 자동 정합 + 변화 강조 |
| Spectral Unmixing | Innovation | 가상 이중 에너지 분리 |
| Neural Radiance Field | Innovation | 제한 뷰에서 3D 재구성 |

---

## Part IV: 경쟁 포지셔닝

### 21. 상용 시스템 대비 포지셔닝

```
                     처리 품질
                       ^
                       |
    Agfa MUSICA Xpert* |  * XPE Phase 3 (목표)
                       |
      Fujifilm FNC *   |  * XPE Phase 2
                       |
  Carestream Eclipse * |
                       |  * XPE Phase 1b
                       |
                       +-------------------------> 기능 범위
```

### 22. XPE 고유 차별화 요소

| # | 차별화 요소 | 설명 | 경쟁사 대비 |
|---|-----------|------|-----------|
| 1 | **3-Tier NLCSC Lag Correction** | 대부분 상용 시스템은 단일 tier LTI. NLCSC Tier 3로 < 0.29% 1차 프레임 lag | 업계 선도 |
| 2 | **FixPix MLP Defect Correction** | 1425 파라미터로 14.2x NMSE 개선. FPGA 이식 가능 | 업계 유일 |
| 3 | **연구 검증 파이프라인 순서** | 모든 스테이지 순서가 논문 + 물리 원리로 검증 | 문서화 수준 업계 선도 |
| 4 | **Duo-SID Heel Effect** | 임의 SID에서 80% RMSE 감소 | 희소 |
| 5 | **8-Safety Bypass Contract** | BYP-SAFE-001~008 형식적 bypass 분류 | 업계 유일 (문서화 수준) |

### 23. 제안 신규 차별화 (상용 시스템에 없는 기능)

| # | 기능 | 설명 |
|---|------|------|
| 1 | 예측적 캘리브레이션 스케줄링 | ML 기반 최적 재캘리브레이션 시점 예측 |
| 2 | 자가진단 파이프라인 | 스테이지별 품질 점수로 부적절 보정 사전 감지 |
| 3 | 캘리브레이션 디지털 트윈 | 검출기 모델 시뮬레이션으로 예측적 유지보수 |
| 4 | 적응형 결함 맵 진화 | 일시적 결함의 정적 BPM 자동 승격 |
| 5 | 픽셀별 보정 신뢰도 맵 | 각 전처리 스테이지가 픽셀별 신뢰도 점수 출력 |

---

## Part V: 규제 리스크 매핑

| 리스크 수준 | 해당 기능 | 규제 고려사항 |
|:----------:|---------|-------------|
| **낮음** (Class B 유지) | Memory Pool, Tile Processing, SIMD, Audit Trail, Async I/O, DICOM SR, Multi-Detector Norm | 기존 IEC 62304 Class B 범위 내 |
| **중간** (추가 검증) | Self-Calibrating, No-Ref IQA, NanoGrid, Automated QC, Anatomy-Aware | 알고리즘 검증 강화 필요, 510(k) predicate 존재 |
| **높음** (규제 전략 필요) | Pathology-Preserving, OneClick (Phase 3), ALARA Advisor, Repeat/Reject | CADx/CADe 경계, FDA Class II 가능성, 별도 전략 필요 |

---

## Part VI: 비전

> **"방사선 기사가 촬영 버튼 하나만 누르면, 어떤 검출기에서든 동일한 진단 최적 영상이 1초 안에 완성되고, 품질 인증서가 자동으로 발급되는 세계."**

이 비전을 실현하기 위한 5대 설계 철학:

1. **Zero Configuration** (제로 설정): 설치 후 설정 없이 최적 화질. Body Part Recognition + Dose-Aware + Detector Normalization이 결합되어 "즉작 동작"
2. **Invisible Complexity** (숨겨진 복잡성): 17-스테이지 파이프라인의 복잡성은 완전히 숨기고, 사용자에게는 "촬영 -> 최적 영상" 단일 인터페이스만 노출
3. **Continuous Improvement** (지속적 개선): 매 프레임에서 학습하는 자동 캘리브레이션 + 예측적 유지보수
4. **Transparency** (투명성): 처리 감사 기록으로 모든 처리 과정이 완전 추적 가능
5. **Safety First** (안전 최우선): 결정론적 폴백이 항상 사용 가능. AI는 보조적이며 안전하게 기능 저하 가능

---

## References

### Pre-Processing Sources
- Starman et al. 2012 (PMC3465354) -- NLCSC lag correction
- Pang et al. 2006 (PMC5722609) -- Lag vs ghosting model
- Ranger et al. 2014 (PMC3965338) -- Gain/offset calibration SNR
- Jeon et al. 2021 (PMC7930811) -- DL defect correction
- Schirrmacher et al. 2023/2024 (arXiv:2310.11637v2) -- FixPix
- Wang 2013 -- Duo-SID heel effect
- EP2148500A1 -- Dynamic dark correction patent
- Lag-Net 2025 (ScienceDirect) -- CNN lag correction
- ACPSEM 2024 (PMC11408574) -- Digital X-ray QA

### Post-Processing Sources
- Eulig et al. 2024 (AAPM) -- DL denoising benchmark
- arXiv:2411.01373 -- Multiscale CLAHE
- PMC5352826 -- 2D DWT grid suppression
- ScienceDirect 2024 -- xU-NetFullSharp bone suppression
- PMC9793471 -- DES-free bone suppression training
- arXiv:2409.05681 -- SX-Stitch image stitching
- Agfa MUSICA Xpert (RSNA 2025)
- Fujifilm FNC (fujifilm.com)
- Carestream ImageView 2024

### Innovation Sources
- PMC8724686 -- X-ray imaging technology review
- PMC11941271 -- AI-driven low-dose imaging
- arXiv:2510.24579 -- Physics-inspired scatter correction
- PMC12027808 -- Medical image quality assessment review
- Simd Library (GitHub) -- SIMD optimized processing

---

*End of Module Reinforcement Plan v1.0.0*
