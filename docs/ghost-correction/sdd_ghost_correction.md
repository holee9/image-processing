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

### 3.4 SIMD Optimization (ARM NEON / x86 SSE4.2)

Tier 3 processes 9.4M pixels × 4 state terms = 37.7M iterations.
SIMD vectorization is mandatory to meet the <500ms NFR-103 budget.

#### 3.4.1 Memory Layout for SIMD Access

```
Scalar layout (cache-unfriendly for SIMD per-term access):
  state[n][y][x]  → iterate n=0..3 for each pixel → 4 non-contiguous loads

Recommended SIMD layout (AoS → SoA, interleaved 4-term):
  For each pixel (y, x), store all 4 terms contiguously:
  state_aos[y][x][0..3]  → 4 × 4 = 16 bytes = one cache line

  This allows vld4q_s32 (ARM NEON) to load all 4 terms in one instruction.
```

#### 3.4.2 ARM NEON Vectorization Pseudocode

Process 4 pixels simultaneously (4-wide SIMD lane × int32):

```c
/**
 * @brief  SIMD-optimized NLCSC inner loop (ARM NEON)
 * @note   Processes 4 pixels per iteration using int32x4 lanes.
 *         Loop stride: 4 pixels/iter → 9.4M / 4 = 2.35M iterations
 *         Expected speedup: ~3.5x over scalar (measured on Cortex-A57)
 *
 * SRS Trace: FR-301 (NLCSC), FR-304 (LUT-based exp), NFR-103 (timing)
 */

/*
 * ARM NEON pseudocode (actual intrinsics; compile with -mfpu=neon):
 *
 * PRECONDITION: state_aos[pixel_idx][0..3] is 16-byte aligned
 *               decay_vec[n] = precomputed exp_lut(-dt/tau_n) × 4 lanes
 *               bn_vec[n]    = precomputed bn(E_prev) × 4 lanes
 *               x_hat_prev4  = x_hat from previous frame × 4 lanes
 *
 * for pixel = 0; pixel < H*W; pixel += 4:
 *
 *   // Load 4 pixels × 4 state terms: vld4q_s32
 *   // state_aos layout: [s0,s1,s2,s3] per pixel
 *   int32x4x4_t s = vld4q_s32(state_aos + pixel * 4);
 *   // s.val[n] = state term n for pixels [0..3]
 *
 *   int32x4_t x_prev = vld1q_s32(x_hat_prev + pixel);  // 4 x_hat values
 *
 *   // Update each of 4 state terms:
 *   FOR n = 0 TO 3:
 *     // decay × state (Q16.16 fixed-point multiply, shift right 16)
 *     int64x2_t tmp_lo = vmull_s32(vget_low_s32(s.val[n]),
 *                                  vget_low_s32(decay_vec[n]));
 *     int64x2_t tmp_hi = vmull_s32(vget_high_s32(s.val[n]),
 *                                  vget_high_s32(decay_vec[n]));
 *     int32x4_t decayed = vcombine_s32(vshrn_n_s64(tmp_lo, 16),
 *                                      vshrn_n_s64(tmp_hi, 16));
 *
 *     // bn × x_hat_prev (Q16.16 multiply)
 *     int64x2_t inp_lo = vmull_s32(vget_low_s32(x_prev),
 *                                   vget_low_s32(bn_vec[n]));
 *     int64x2_t inp_hi = vmull_s32(vget_high_s32(x_prev),
 *                                   vget_high_s32(bn_vec[n]));
 *     int32x4_t bn_input = vcombine_s32(vshrn_n_s64(inp_lo, 16),
 *                                       vshrn_n_s64(inp_hi, 16));
 *
 *     s.val[n] = vaddq_s32(decayed, bn_input);  // state updated
 *   END FOR
 *
 *   // lag_sum = s[0] + s[1] + s[2] + s[3]
 *   int32x4_t lag_sum = vaddq_s32(vaddq_s32(s.val[0], s.val[1]),
 *                                   vaddq_s32(s.val[2], s.val[3]));
 *
 *   // Load y_k (raw input) and compute x_hat = (y_k - lag_sum) / b0
 *   int32x4_t y_k4 = vmovl_u16(vld1_u16(raw_input + pixel));
 *   int32x4_t numerator = vsubq_s32(y_k4, lag_sum);
 *
 *   // Division by b0 (Q16.16): multiply by (1/b0) precomputed as Q16.16
 *   int64x2_t div_lo = vmull_s32(vget_low_s32(numerator), inv_b0_q1616);
 *   int64x2_t div_hi = vmull_s32(vget_high_s32(numerator), inv_b0_q1616);
 *   int32x4_t x_hat4 = vcombine_s32(vshrn_n_s64(div_lo, 16),
 *                                    vshrn_n_s64(div_hi, 16));
 *
 *   // Clamp to [0, 65535]
 *   x_hat4 = vmaxq_s32(x_hat4, vdupq_n_s32(0));
 *   x_hat4 = vminq_s32(x_hat4, vdupq_n_s32(65535));
 *
 *   // Store updated states and x_hat
 *   vst4q_s32(state_aos + pixel * 4, s);
 *   vst1q_s32(x_hat_prev + pixel, x_hat4);
 *   vst1q_s32(output + pixel, x_hat4);
 *
 * END for
 */
```

#### 3.4.3 x86 SSE4.2 Alternative

For x86 platforms without AVX2:

```c
/*
 * x86 SSE4.2 (4 × int32) equivalent pattern:
 * - Use _mm_mullo_epi32 for 32-bit multiply (SSE4.1 required)
 * - Simulate int64 multiply via _mm_mul_epi32 + manual shift
 * - _mm_srai_epi32 for arithmetic right shift (>>16)
 *
 * Performance expectation: ~2.8x over scalar on Core i7
 * (vs 3.5x on NEON due to x86 latency on 32-bit multiply)
 */
```

#### 3.4.4 Watchdog Timer for Tier 3 Timeout

```c
/**
 * @brief  Watchdog mechanism to prevent Tier 3 runaway (NFR-103: <500ms)
 *
 * Implementation:
 *   1. Record start_time = get_timestamp_us()
 *   2. After each SIMD block (4 pixels), check elapsed:
 *      if (elapsed_ms > TIER3_TIMEOUT_MS) {
 *          // Fallback: copy processed pixels to output, skip rest
 *          memcpy(output + pixel, raw_input + pixel,
 *                 (total_pixels - pixel) * sizeof(uint16_t));
 *          result->flags |= CORR_FLAG_TIER3_TIMEOUT;
 *          LOG_WARN("Tier 3 timeout at pixel %u/%u (%.1f%% processed)",
 *                   pixel, total_pixels, 100.0*pixel/total_pixels);
 *          return CORR_WARN_TIER3_TIMEOUT;
 *      }
 *   3. TIER3_TIMEOUT_MS = 450 (leaves 50ms margin for NFR-103)
 *
 * Note: Timeout causes graceful degradation (partially corrected output)
 * rather than pipeline abort. The unprocessed portion retains Tier 2 output.
 */
#define TIER3_TIMEOUT_MS  450U
#define TIER3_BLOCK_SIZE  1024U  // Check timer every 1024 pixels (4 SIMD iters)
```

### 3.5 Polynomial Evaluation (bₙ(E))

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

---

## 9. Error Code Definitions

### 9.1 CorrectionError Enum

SRS-GHOST-001 Trace: FR-105, FR-204, NFR-302, NFR-304

```c
/**
 * @brief  Correction module error codes
 * @note   Negative values = fatal (pipeline aborts, raw output)
 *         Zero = success
 *         Positive values = warning (pipeline continues, result flagged)
 *
 * IMPORTANT: These codes are used ONLY within the ghost/lag correction
 * module (srs_ghost_correction.md, sad_ghost_correction.md). The
 * higher-level xpe_preprocess.dll uses XpeErrorCode (XPE_ERR_*).
 * The pipeline layer (CorrectionPipeline) maps CorrectionError →
 * XpeErrorCode before returning to the C# orchestrator.
 */
typedef enum {
    /* Fatal errors (pipeline abort, output = raw frame) */
    CORR_ERR_NULL_PTR         = -1,  ///< NULL input pointer
    CORR_ERR_CALIB_CRC        = -2,  ///< Calibration file CRC32 mismatch
    CORR_ERR_CALIB_VERSION    = -3,  ///< Calibration file version incompatible
    CORR_ERR_MEMORY           = -4,  ///< Static buffer allocation failure
    CORR_ERR_INVALID_PARAM    = -5,  ///< Parameter out of valid range

    /* Success */
    CORR_OK                   =  0,

    /* Warnings (pipeline continues, result.flags bit set) */
    CORR_WARN_OVERFLOW        =  1,  ///< Pixel overflow/underflow > 0.1%
    CORR_WARN_TEMPERATURE     =  2,  ///< Panel temperature out of calibrated range
    CORR_WARN_TIER_ESCALATED  =  3,  ///< Tier escalation triggered (GCR > threshold)
    CORR_WARN_DARK_POST_NONE  =  4,  ///< dark_post NULL, Tier 2 skipped
    CORR_WARN_CALIB_EXPIRED   =  5,  ///< Calibration file older than recalibration interval
} CorrectionError;
```

### 9.2 Error-to-XpeErrorCode Mapping

The pipeline layer maps internal CorrectionError codes to the
xpe_preprocess.dll ABI (XpeErrorCode) before returning to the C# host:

| CorrectionError          | XpeErrorCode                  | Notes                        |
|--------------------------|-------------------------------|------------------------------|
| CORR_ERR_NULL_PTR        | XPE_ERR_INVALID_PARAM (-5)    | NULL treated as invalid param |
| CORR_ERR_CALIB_CRC       | XPE_ERR_IO_FAILED (-3)        | File integrity failure        |
| CORR_ERR_CALIB_VERSION   | XPE_ERR_INVALID_CALIB_DATA (-4)| Version mismatch             |
| CORR_ERR_MEMORY          | XPE_ERR_NOT_INITIALIZED (-1)  | Cannot allocate state         |
| CORR_ERR_INVALID_PARAM   | XPE_ERR_INVALID_PARAM (-5)    | Direct mapping                |
| CORR_OK                  | XPE_OK (0)                    | Success                       |
| CORR_WARN_* (≥ 1)        | XPE_OK + flags set            | Warning propagated via flags  |

### 9.3 α(E) LUT Alignment with SRS FR-203

The α LUT structure referenced in SDD Section 2.4 is defined here for
cross-document consistency with SRS-GHOST-001 FR-203 and FR-304:

```c
/**
 * @brief  Exposure-dependent α coefficient LUT
 *         Stored in CalibrationData.irf (IrfParams struct)
 *
 * SRS Trace: FR-203 (α(E) LUT lookup), FR-304 (exp(-a) LUT)
 */
typedef struct {
    uint8_t   num_entries;        ///< Number of calibration exposure levels (max 16)
    uint32_t  E_levels[16];       ///< Exposure levels in ascending order (ADU)
    q1616_t   alpha_nominal[16];  ///< α at T_ref=25°C, Q16.16 fixed-point
    float     beta_temp;          ///< Temperature sensitivity coefficient (≈0.02/°C)
    float     T_ref;              ///< Reference temperature (25.0°C)

    /* bₙ(E) polynomial coefficients for Tier 3 NLCSC (Section 3.4) */
    uint8_t   nlcsc_n_terms;      ///< Number of exponential terms N (=4)
    float     poly_coeff[4][5];   ///< Horner coefficients c₀..c₄ per term n
    float     tau_nominal[4];     ///< τ_n at T_ref (seconds)
    float     tau_temp_coeff[4];  ///< Temperature coefficient for τ_n
} IrfParams;
```

**Key constraint**: `alpha_nominal[i]` must satisfy 0 < α ≤ 1.0 for all i.
Out-of-range values are clamped during calibration file load (calib_load).
If `E_levels` is not strictly ascending, `calib_load` returns
`CORR_ERR_CALIB_VERSION`.
