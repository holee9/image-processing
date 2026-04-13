# SDD: Software Detailed Design

> **Document ID**: SDD-GHOST-001 | **Version**: 1.0 | **Date**: 2026-04-02
>
> **Trace Source**: SAD-GHOST-001, SRS-GHOST-001

---

## 1. Module: Tier1_Offset

### 1.1 설계 목적

Raw frame에서 dark frame을 pixel별로 차감. Trace: FR-101~105

### 1.2 함수 상세

```c
/**
 * @brief  Offset correction via dark subtraction
 * @param  raw     X-ray raw frame (non-NULL)
 * @param  dark    Pre-exposure dark frame (non-NULL)
 * @param  output  Result frame (non-NULL, may alias raw)
 * @return CORR_OK on success
 * @note   output = clamp(raw - dark, 0, 65535)
 *         Underflow pixels → 0, overflow pixels → 65535
 *         Counts stored in module-internal counters, retrieved via tier1_get_stats()
 */
CorrectionError tier1_offset_correct(
    const Frame* raw, const Frame* dark, Frame* output);

/**
 * @brief  Get overflow/underflow statistics from last call
 */
void tier1_get_stats(uint32_t* overflow_count, uint32_t* underflow_count);
```

### 1.3 알고리즘 상세

```
INPUT:  raw[H][W], dark[H][W]  (uint16)
OUTPUT: out[H][W]              (uint16)

overflow_count = 0
underflow_count = 0

FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    diff = (int32_t)raw[y][x] - (int32_t)dark[y][x]
    IF diff < 0:
      out[y][x] = 0
      underflow_count++
    ELSE IF diff > 65535:
      out[y][x] = 65535
      overflow_count++
    ELSE:
      out[y][x] = (uint16_t)diff

SIMD 최적화 (ARM NEON):
  - 8 pixels × uint16 동시 로드 (vld1q_u16)
  - vsubl_u16 → int32×4 차분
  - vmax → 0 clamp
  - vmin → 65535 clamp
  - vst1q_u16 → 저장
  - 루프 당 8 pixels, ~1.2M iterations for 9.4M pixels
```

### 1.4 Edge Case

| 케이스 | 입력 | 동작 | 근거 |
|---|---|---|---|
| Normal | raw=1000, dark=300 | 700 | 정상 |
| Underflow | raw=100, dark=300 | 0 + count++ | FR-102 |
| Max value | raw=65535, dark=0 | 65535 | 정상 |
| Zero both | raw=0, dark=0 | 0 | 정상 |
| NULL raw | raw=NULL | CORR_ERR_NULL_PTR | FR-105 |

---

## 2. Module: Tier2_Lag

### 2.1 설계 목적

Post-exposure dark frame과 pre-exposure dark의 차분으로 lag를 추정하고 보정. Trace: FR-201~205

### 2.2 함수 상세

```c
/**
 * @brief  AR(1) based lag correction using post-exposure dark
 * @param  I_oc       Offset-corrected frame (Tier 1 output)
 * @param  dark_post  Post-exposure dark frame
 * @param  dark_pre   Pre-exposure dark frame
 * @param  irf        IRF parameters (contains α LUT)
 * @param  history    Exposure history array
 * @param  hist_count Number of valid history entries
 * @param  temperature Current panel temperature (°C)
 * @param  output     Lag-corrected frame
 * @return CORR_OK, CORR_ERR_NULL_PTR
 */
CorrectionError tier2_lag_correct(
    const Frame* I_oc,
    const Frame* dark_post,
    const Frame* dark_pre,
    const IrfParams* irf,
    const ExposureRecord* history,
    uint8_t hist_count,
    float temperature,
    Frame* output);
```

### 2.3 알고리즘 상세

```
INPUT:  I_oc[H][W], D_post[H][W], D_pre[H][W], α(E), temperature
OUTPUT: out[H][W]

// Step 1: α(E) 결정
E = history[last].exposure_level
alpha = lut_lookup_alpha(E, irf, temperature)
  // α는 Q16.16 fixed-point
  // 온도 보정: alpha *= temp_scale(temperature)
  // E 범위 외: clamp to nearest calibrated level

// Step 2: 잔류 lag 맵 계산 + 보정
FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    // Lag signal = D_post - D_pre
    lag_raw = (int32_t)D_post[y][x] - (int32_t)D_pre[y][x]

    // Negative lag clamp (noise)
    IF lag_raw < 0: lag_raw = 0

    // Lag estimate in X-ray frame
    lag_est = fixed_mul_q1616(alpha, lag_raw)
      // Q16.16 × int32 → int32 (>>16)

    // Subtract from offset-corrected image
    corrected = (int32_t)I_oc[y][x] - lag_est

    // Clamp
    out[y][x] = CLAMP(corrected, 0, 65535)
```

### 2.4 α(E) LUT 설계

```
LUT 구조:
  uint8_t num_entries;  // 9 (교정 exposure 수준 수)
  uint32_t E_levels[9]; // exposure levels (ascending)
  q1616_t  alpha[9];    // corresponding α values

조회:
  E가 E_levels[i]와 E_levels[i+1] 사이이면:
    α = linear_interp(alpha[i], alpha[i+1], E, E_levels[i], E_levels[i+1])
  E < E_levels[0]: α = alpha[0] (외삽 금지, 최소값 사용)
  E > E_levels[8]: α = alpha[8]

온도 보정:
  α_adjusted = α × (1 + β × (T - T_ref))
  β ≈ 0.02/°C, T_ref = 25°C
```

---

## 3. Module: Tier3_Nlcsc

### 3.1 설계 목적

Exposure-dependent NLCSC로 최고 정밀도 lag 보정. Trace: FR-301~305

### 3.2 State Management

```
Persistent state (frame 간 유지):
  pixel_wide_t state[N_EXP_TERMS][IMG_HEIGHT][IMG_WIDTH]
  // 4 × 3072 × 3072 × 4 bytes = 75.6 MB

State lifecycle:
  - correction_init() 시 0으로 초기화
  - 매 frame 처리 후 업데이트
  - 30분 이상 idle 시 0으로 reset (deep trap 감쇠 완료)
  - config 변경 시 0으로 reset
```

### 3.3 알고리즘 상세

```
FOR each pixel (y, x):

  // 1. Time-decay state update
  dt = current_timestamp - last_timestamp  // seconds
  FOR n = 0 TO 3:
    tau_n = get_tau(n, irf, temperature)
    decay = exp_lut(-dt / tau_n)        // LUT lookup

    E_prev = history[last].exposure_level
    bn_E = lookup_bn(n, E_prev, irf)   // polynomial eval: b = Qn(E)/E
    an_E = lookup_an(n, E_prev, irf)    // a1,n + a2,n(E)

    state[n][y][x] = fixed_mul(state[n][y][x], decay)
                   + fixed_mul(x_hat_prev[y][x], bn_E)

  // 2. Corrected signal
  lag_sum = state[0][y][x] + state[1][y][x] + state[2][y][x] + state[3][y][x]

  // b0 = 1 - Σbn (constraint)
  x_hat = fixed_div(y_k - lag_sum, irf.b0)

  output[y][x] = CLAMP(x_hat, 0, 65535)

  // 3. Store x_hat for next frame's state update
  x_hat_prev[y][x] = x_hat
```

### 3.4 Polynomial Evaluation (bₙ(E))

```
Qₙ(E) = c₀ + c₁E + c₂E² + c₃E³ + c₄E⁴  (4th-order polynomial)
bₙ(E) = Qₙ(E) / E

Fixed-point 구현:
  Horner's method: Q = c₄
  Q = Q×E + c₃
  Q = Q×E + c₂
  Q = Q×E + c₁
  Q = Q×E + c₀
  b = Q / E

  모든 연산 Q16.16, overflow 주의
  E=0일 때: b=0 (division by zero 방지)
```

---

## 4. Module: GainCorrection

### 4.1 알고리즘

```
FOR each pixel (y, x):
  g = gain_map[y][x]   // uint16, mean-normalized

  IF g == 0:
    output[y][x] = input[y][x]  // dead pixel
    CONTINUE

  // output = input × gain_mean / g
  // 32-bit intermediate: (uint32_t)input × (uint32_t)gain_mean
  num = (uint32_t)input[y][x] * (uint32_t)gain_mean
  result = num / (uint32_t)g

  output[y][x] = MIN(result, 65535)
```

### 4.2 Gain Map Normalization

```
Factory calibration에서:
  flat_field[y][x] = 균일 X-ray 조사 시 pixel 값
  gain_mean = median(flat_field)   // median이 outlier에 robust
  gain_map[y][x] = flat_field[y][x]  // 정규화하지 않고 원본 저장

보정 시:
  output = input × gain_mean / gain_map[y][x]
  → 감도가 높은 pixel(큰 gain_map 값)은 나눗셈으로 줄어듦
  → 감도가 낮은 pixel은 증폭됨
  → 결과: 균일한 응답
```

---

## 5. Module: DefectCorrection

### 5.1 보간 방법 3종

```
Method 0: Neighbor Average
  value = mean of valid 8-neighbors

Method 1: Bilinear Interpolation
  4 방향(상하좌우) 최근접 valid pixel 탐색
  거리 가중 평균

Method 2: Median
  value = median of valid 8-neighbors
  → Edge 보존에 유리

Cluster 처리:
  3×3 window 내 복수 defect → valid neighbor만 사용
  Valid neighbor 0개 → 원본 유지 + WARNING 로그
```

### 5.2 Line Defect 처리

```
Row defect (전체 row가 defect):
  위 row와 아래 row의 평균으로 대체
  연속 2 row defect: 양 끝 valid row에서 선형 보간

Column defect:
  동일 원리, 좌우 방향
```

---

## 6. Module: GcrEstimator

### 6.1 Real-time GCR 추정

```
Tier escalation 판단을 위한 빠른 GCR 추정.
전체 pixel이 아닌 서브샘플링으로 속도 확보.

Algorithm:
  1. Frame을 16×16 블록으로 분할 (192×192 블록)
  2. 각 블록의 mean 계산
  3. 블록 mean의 std 계산 → σ_block
  4. σ_block / mean(전체) → 근사 GCR

  연산량: 192×192 = 36,864 블록 × 256 pixels = ~9.4M ops
  → Tier 1과 비슷한 연산량이므로, 더 경량화 필요

최적화:
  1. 4×4 서브샘플링 (768×768 = 589,824 pixels만 사용)
  2. ROI 기반: 사전 정의된 2개 ROI (중앙, 가장자리)의 mean 차이
  → ~1.2M ops, < 5ms
```

---

## 7. Module: MathUtils

### 7.1 Fixed-Point 연산

```c
// Q16.16 곱셈 (overflow-safe)
static inline q1616_t fixed_mul(q1616_t a, q1616_t b) {
    int64_t tmp = (int64_t)a * (int64_t)b;
    return (q1616_t)((tmp + 0x8000) >> 16);  // Round half-up
}

// Q16.16 나눗셈 (division-by-zero safe)
static inline q1616_t fixed_div(q1616_t a, q1616_t b) {
    if (b == 0) return 0;  // Safe fallback
    int64_t tmp = ((int64_t)a << 16);
    return (q1616_t)(tmp / (int64_t)b);
}

// Pixel × Q16.16 → pixel (for lag subtraction)
static inline int32_t fixed_mul_pixel(q1616_t coeff, int32_t pixel) {
    int64_t tmp = (int64_t)coeff * (int64_t)pixel;
    return (int32_t)((tmp + 0x8000) >> 16);
}

// Clamp int32 to uint16
static inline pixel_t clamp_u16(int32_t val) {
    if (val < 0) return 0;
    if (val > 65535) return 65535;
    return (pixel_t)val;
}
```

### 7.2 Exp LUT

```c
// 256-entry exp(-a) LUT
// Index: a_index = (uint8_t)(a_q1616 >> 10)  → a 범위 0~16
// Value: Q0.16 (0~65535 representing 0.0~1.0)

static const uint16_t exp_lut[256] = {
    65535, 65282, 65030, ...  // exp(-0), exp(-0.0625), exp(-0.125), ...
    // exp(-16) ≈ 0 → last entries are 0
};

static inline q16_t exp_lut_lookup(q1616_t a) {
    if (a <= 0) return 0xFFFF;        // exp(0) = 1.0
    if (a >= (16 << 16)) return 0;    // exp(-16) ≈ 0

    uint32_t idx = (uint32_t)a >> 10;  // 0~1023 → 0~255 (upper 8 bits of fractional part)
    if (idx >= 255) return 0;

    // Linear interpolation
    uint16_t v0 = exp_lut[idx];
    uint16_t v1 = exp_lut[idx + 1];
    uint16_t frac = (a >> 2) & 0xFF;  // 8-bit fractional between entries

    return v0 + (((int32_t)(v1 - v0) * frac) >> 8);
}
```

---

## 8. Module: CalibFileIO

### 8.1 .gcal 파일 읽기

```
Function: calib_load(const char* filepath, CalibrationData* out)

1. Open file, read 64-byte header
2. Verify magic == 0x47434C42
3. Verify version compatibility
4. Read data section
5. Compute CRC32 of data section
6. Compare with header CRC
7. If mismatch → CORR_ERR_CALIB_CRC
8. Populate CalibrationData struct
9. Close file

CRC32 algorithm: ISO 3309 (same as zlib)
```

### 8.2 .gcal 파일 쓰기 (교정 도구용)

```
Function: calib_save(const char* filepath, const CalibrationData* data)

1. Compute CRC32 of data section
2. Populate header with magic, version, CRC
3. Write header (64 bytes)
4. Write data section
5. Flush and close

파일 무결성: write 후 재읽기하여 CRC 재검증
```
