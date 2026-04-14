# Software Detailed Design

> **Document ID**: XPE-SDD-002 | **Version**: 1.0 | **Date**: 2026-04-14
>
> **IEC 62304 Clause**: 5.4 (Class B: voluntary detailed design)
>
> **Safety Classification**: Class B
>
> **Trace Source**: XPE-SDD-001, XPE-SAD-001, XPE-SRS-001, ALG-SPEC-001

---

## 1. Purpose & Scope

XPE-SDD-001에서 식별된 32개 Software Unit(SWU)에 대한 상세 설계를 정의한다.
각 unit에 대해 C++ 내부 설계, DLL export C ABI 시그니처, 알고리즘 의사코드, edge case를 명시한다.

> **Note**: IEC 62304 Class B는 unit-level detailed design을 필수로 요구하지 않으나,
> 구현 정확성 확보 및 검증 용이성을 위해 자발적으로 작성한다.

### 표기 규칙

- **DLL API**: `extern "C"` 함수 시그니처 (C# P/Invoke 호환)
- **Internal**: C++17 클래스/함수 (내부 구현)
- **Pseudocode**: 언어 비종속 알고리즘 기술
- **Edge Case**: 입력-동작-근거 테이블

---

## 2. SWI-1: Pre-Processing Module

### 2.1 SWU-1.1: OffsetCorrector

**설계 목적**: Raw frame에서 dark offset map을 pixel별 감산. Trace: SRS-FUNC-001

#### DLL API

```cpp
// C ABI (extern "C") — xpe_preprocess.dll export
XPE_API XpeErrorCode xpe_offset_correct(
    const XpeImageBuffer* raw,
    const XpeImageBuffer* offsetMap,
    XpeImageBuffer*       output);
```

#### Internal Class

```cpp
namespace xpe::preprocess {

class OffsetCorrector {
public:
    struct Stats {
        uint32_t underflowCount = 0;
        uint32_t overflowCount  = 0;
    };

    /// @brief  Offset 감산 수행
    /// @param  raw       X-ray raw frame (non-null, uint16)
    /// @param  offset    Dark offset map (non-null, uint16, same dimensions)
    /// @param  output    Result frame (non-null, may alias raw)
    /// @return XPE_OK on success
    /// @post   output[y][x] = clamp(raw[y][x] - offset[y][x], 0, 65535)
    XpeErrorCode correct(
        const XpeImageBuffer& raw,
        const XpeImageBuffer& offset,
        XpeImageBuffer&       output);

    Stats getStats() const noexcept;

private:
    Stats stats_{};
};

} // namespace xpe::preprocess
```

#### Pseudocode

```
INPUT:  raw[H][W], offset[H][W]   (uint16)
OUTPUT: out[H][W]                  (uint16)

underflow_count = 0
overflow_count  = 0

FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    diff = (int32_t)raw[y][x] - (int32_t)offset[y][x]
    IF diff < 0:
      out[y][x] = 0
      underflow_count++
    ELSE IF diff > 65535:
      out[y][x] = 65535
      overflow_count++
    ELSE:
      out[y][x] = (uint16_t)diff

SIMD Optimization (AVX2, 16 pixels/iteration):
  - _mm256_loadu_si256 for raw, offset (256-bit load)
  - _mm256_subs_epu16 for saturating subtraction
  - _mm256_storeu_si256 for result
  - Underflow detection via comparison mask
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Normal | raw=1000, offset=300 | 700 | SRS-FUNC-001 |
| Underflow | raw=100, offset=300 | 0 + count++ | SRS-FUNC-001 (clamp) |
| Max value | raw=65535, offset=0 | 65535 | Normal |
| Zero both | raw=0, offset=0 | 0 | Normal |
| NULL raw | raw=nullptr | XPE_ERR_INVALID_INPUT | Null check |
| NULL offset | offset=nullptr | XPE_ERR_INVALID_INPUT | Null check |
| Dimension mismatch | raw.width != offset.width | XPE_ERR_INVALID_INPUT | Precondition |

---

### 2.2 SWU-1.2: GainCorrector

**설계 목적**: Pixel 간 감도 차이 및 heel effect 보정. Trace: SRS-FUNC-002

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_gain_correct(
    const XpeImageBuffer* input,
    const XpeImageBuffer* gainMap,
    uint32_t              gainMean,
    XpeImageBuffer*       output);
```

#### Internal Class

```cpp
namespace xpe::preprocess {

class GainCorrector {
public:
    /// @brief  Flat-field gain 보정
    /// @param  gainMean  Gain map의 median 값 (정규화 기준)
    /// @post   output[y][x] = clamp(input[y][x] * gainMean / gainMap[y][x], 0, 65535)
    ///         gainMap[y][x] == 0 → dead pixel, input 값 그대로 통과
    XpeErrorCode correct(
        const XpeImageBuffer& input,
        const XpeImageBuffer& gainMap,
        uint32_t              gainMean,
        XpeImageBuffer&       output);
};

} // namespace xpe::preprocess
```

#### Pseudocode

```
INPUT:  input[H][W] (uint16), gainMap[H][W] (uint16), gainMean (uint32)
OUTPUT: out[H][W] (uint16)

FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    g = gainMap[y][x]

    IF g == 0:
      out[y][x] = input[y][x]   // dead pixel bypass
      CONTINUE

    num = (uint32_t)input[y][x] * (uint32_t)gainMean
    result = num / (uint32_t)g
    out[y][x] = MIN(result, 65535)

Gain Map Generation (factory calibration):
  flat_field[y][x] = uniform X-ray exposure pixel value
  gainMean = median(flat_field)
  gainMap[y][x] = flat_field[y][x]   // raw stored, normalized at runtime

SID-specific gain map support:
  gainMap = gainMapRegistry.get(currentSID)
  IF not found: gainMap = default gainMap + WARNING
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Unity gain | pixel=1000, gain=mean=32768 | 1000 | SRS-FUNC-002 |
| Low gain pixel | pixel=1000, gain=16384, mean=32768 | 2000 | Amplification |
| High gain pixel | pixel=1000, gain=65535, mean=32768 | 500 | Attenuation |
| Overflow clamp | pixel=60000, gain=16384, mean=32768 | 65535 | Saturated |
| Dead pixel (gain=0) | pixel=1000, gain=0 | 1000 (pass-through) | SRS-FUNC-002 |
| NULL input | nullptr | XPE_ERR_INVALID_INPUT | Null check |

---

### 2.3 SWU-1.3: DefectPixelCorrector

**설계 목적**: Bad pixel 검출 및 보간. Trace: SRS-FUNC-003

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_defect_correct(
    XpeImageBuffer*       image,        // in-place correction
    const uint8_t*        badPixelMap,   // 1=defect, 0=normal
    uint32_t              mapWidth,
    uint32_t              mapHeight,
    int32_t               method);       // 0=avg, 1=bilinear, 2=median
```

#### Internal Class

```cpp
namespace xpe::preprocess {

enum class InterpolationMethod { Average = 0, Bilinear = 1, Median = 2 };

class DefectPixelCorrector {
public:
    struct Result {
        uint32_t correctedCount   = 0;
        uint32_t uncorrectedCount = 0;  // no valid neighbors
    };

    XpeErrorCode correct(
        XpeImageBuffer&       image,
        std::span<const uint8_t> badPixelMap,
        InterpolationMethod   method = InterpolationMethod::Average);

    Result getResult() const noexcept;

private:
    Result result_{};

    uint16_t interpolateAverage(const XpeImageBuffer& img, uint32_t x, uint32_t y,
                                std::span<const uint8_t> bpm);
    uint16_t interpolateBilinear(const XpeImageBuffer& img, uint32_t x, uint32_t y,
                                 std::span<const uint8_t> bpm);
    uint16_t interpolateMedian(const XpeImageBuffer& img, uint32_t x, uint32_t y,
                               std::span<const uint8_t> bpm);
};

} // namespace xpe::preprocess
```

#### Pseudocode

```
FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    IF badPixelMap[y * W + x] == 0: CONTINUE   // normal pixel

    neighbors = collect_valid_8neighbors(x, y, badPixelMap)

    IF neighbors.count == 0:
      // Leave pixel unchanged, emit WARNING
      uncorrectedCount++
      LOG_WARNING("No valid neighbors for defect at ({}, {})", x, y)
      CONTINUE

    SWITCH method:
      Average:  image[y][x] = mean(neighbors.values)
      Bilinear: image[y][x] = distance_weighted_mean(4-directional nearest valid)
      Median:   image[y][x] = median(neighbors.values)

    correctedCount++

Line Defect Handling:
  Row defect (entire row): average of row above and row below
  Column defect: average of left column and right column
  Consecutive 2 rows: linear interpolation from nearest valid rows
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Single defect, 8 neighbors | center=0, neighbors=1000 | 1000 (avg) | SRS-FUNC-003 |
| Corner defect (0,0) | 3 valid neighbors | avg of 3 | Boundary |
| Cluster (2 adjacent) | 2 defect in 3x3 | avg of valid only | SRS-FUNC-003 |
| All neighbors defect | 0 valid | unchanged + WARNING | SRS-ALERT-001 |
| Row defect | entire row bad | avg(above, below) | Line defect |
| NULL badPixelMap | nullptr | XPE_ERR_INVALID_INPUT | Null check |

---

### 2.4 SWU-1.4: GhostCorrector

**설계 목적**: Multi-exponential lag correction. Trace: SRS-FUNC-004

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_ghost_correct(
    const XpeImageBuffer*  input,
    const XpeImageBuffer*  darkPost,       // nullable (Tier 1 only if null)
    const XpeImageBuffer*  darkPre,        // nullable
    const char*            irfParamsJson,   // IRF calibration parameters
    XpeImageBuffer*        output);
```

#### Internal Class

```cpp
namespace xpe::preprocess {

struct IrfParams {
    uint8_t  numExpTerms;       // 3 (standard) or 4 (NLCSC)
    float    alpha[4];          // per-term coefficients
    float    tau[4];            // time constants (seconds)
    float    tempCoeffBeta;     // temperature compensation
    float    refTemperature;    // 25.0 degC
};

class GhostCorrector {
public:
    enum class Tier { Simple = 1, AR1 = 2, NLCSC = 3 };

    XpeErrorCode correct(
        const XpeImageBuffer& input,
        const XpeImageBuffer* darkPost,   // nullptr → skip Tier 2
        const XpeImageBuffer* darkPre,
        const IrfParams&      irf,
        float                 temperature,
        XpeImageBuffer&       output);

    /// @brief  NLCSC state reset (long idle > 30min, config change)
    void resetState() noexcept;

    Tier getLastTierUsed() const noexcept;

private:
    Tier tier_ = Tier::Simple;

    // NLCSC persistent state: [N_TERMS][H][W]
    std::vector<std::vector<float>> nlcscState_;

    void applyTier2AR1(const XpeImageBuffer& input,
                       const XpeImageBuffer& darkPost,
                       const XpeImageBuffer& darkPre,
                       const IrfParams& irf, float temp,
                       XpeImageBuffer& output);

    void applyTier3NLCSC(const XpeImageBuffer& input,
                         const IrfParams& irf, float temp,
                         XpeImageBuffer& output);
};

} // namespace xpe::preprocess
```

#### Pseudocode (Tier 2: AR(1))

```
INPUT:  input[H][W], darkPost[H][W], darkPre[H][W], alpha, temperature
OUTPUT: out[H][W]

alpha_adj = alpha * (1 + beta * (temperature - T_ref))

FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    lag_raw = (int32_t)darkPost[y][x] - (int32_t)darkPre[y][x]
    IF lag_raw < 0: lag_raw = 0   // negative lag is noise

    lag_est = (int32_t)(alpha_adj * lag_raw)
    corrected = (int32_t)input[y][x] - lag_est
    out[y][x] = CLAMP(corrected, 0, 65535)
```

#### Pseudocode (Tier 3: NLCSC, N=4)

```
FOR each pixel (y, x):
  dt = current_time - last_frame_time

  FOR n = 0 TO 3:
    decay = exp(-dt / tau[n])
    state[n][y][x] = state[n][y][x] * decay + x_hat_prev[y][x] * b[n]

  lag_sum = state[0] + state[1] + state[2] + state[3]
  x_hat = (input[y][x] - lag_sum) / b0    // b0 = 1 - sum(b[n])
  out[y][x] = CLAMP(x_hat, 0, 65535)
  x_hat_prev[y][x] = x_hat
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| darkPost=nullptr | no post-dark | Skip Tier 2, use Tier 1 only | SRS-FUNC-004 |
| Negative lag | darkPost < darkPre | lag=0 (noise) | Ghost SDD FR-202 |
| Temperature 35C | T=35 | alpha increased ~20% | Temp compensation |
| Long idle (>30min) | dt > 1800s | Reset NLCSC state | State decay |
| Ghost >= 90% removal | standard case | Verify GCR <= 0.1% | SRS-FUNC-004 |
| NULL input | nullptr | XPE_ERR_INVALID_INPUT | Null check |

---

### 2.5 SWU-1.5: CalibrationManager

**설계 목적**: Calibration 데이터 로드/검증/만료 관리. Trace: SRS-FUNC-001..004, SRS-ALERT-005

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_load_calibration(
    const char*  calibDir,
    const char*  bodyPart);

XPE_API XpeErrorCode xpe_check_calibration_expiry(
    int32_t*  isExpired);
```

#### Internal Class

```cpp
namespace xpe::preprocess {

struct CalibrationData {
    XpeImageBuffer offsetMap;
    XpeImageBuffer gainMap;
    uint32_t       gainMean;
    std::vector<uint8_t> badPixelMap;
    IrfParams      irfParams;
    uint64_t       calibrationTimestamp;   // epoch ms
    uint32_t       crc32;
};

class CalibrationManager {
public:
    XpeErrorCode load(const std::filesystem::path& calibDir,
                      std::string_view bodyPart);

    bool isExpired(uint64_t maxAgeMs = 30ULL * 24 * 3600 * 1000) const;

    const CalibrationData& getData() const;

private:
    CalibrationData data_{};
    bool loaded_ = false;

    XpeErrorCode validateCrc(const std::filesystem::path& filepath,
                             uint32_t expectedCrc);
};

} // namespace xpe::preprocess
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| CRC mismatch | corrupted file | XPE_ERR_CONFIG_INVALID | Data integrity |
| Expired calibration | age > 30 days | XPE_ERR_CALIBRATION_EXPIRED + SRS-ALERT-005 | Safety |
| Missing file | file not found | XPE_ERR_IO_FAILED | Error handling |
| Dimension mismatch | offsetMap != image size | XPE_ERR_INVALID_INPUT | Precondition |

---

## 3. SWI-2: Core Processing Module

### 3.1 SWU-2.1: LogTransform

**설계 목적**: 선형 detector response를 logarithmic domain으로 변환. Trace: SRS-FUNC-010

#### DLL API

```cpp
// xpe_enhance_basic.dll export
XPE_API XpeErrorCode xpe_log_transform(
    const XpeImageBuffer* input,      // uint16
    float                 I0,          // reference intensity
    XpeImageBuffer*       output);     // float32
```

#### Pseudocode

```
INPUT:  input[H][W] (uint16), I0 (float, reference intensity)
OUTPUT: out[H][W] (float32)

EPSILON = 1e-6f

FOR y = 0 TO H-1:
  FOR x = 0 TO W-1:
    val = MAX((float)input[y][x], EPSILON)
    out[y][x] = -logf(val / I0)

Note: I0 is typically the mean of unattenuated region
      or derived from detector calibration data
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Zero pixel | input=0 | -ln(epsilon/I0) | SRS-FUNC-010 (epsilon clamp) |
| Negative (impossible) | input<0 | epsilon clamp | Safety |
| I0=0 | invalid ref | XPE_ERR_INVALID_INPUT | Division by zero |
| Max value | input=65535 | -ln(65535/I0) | Normal |

---

### 3.2 SWU-2.2: NoiseReducer

**설계 목적**: Edge-preserving noise reduction. Trace: SRS-FUNC-011

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_noise_reduce(
    const XpeImageBuffer* input,       // float32
    XpeImageBuffer*       output,      // float32
    int32_t               method,      // 0=bilateral, 1=NLM
    const char*           paramsJson); // optional overrides
```

#### Pseudocode

```
Method 0: Bilateral Filter (default)
  sigma_spatial = 2.0
  sigma_range   = 0.1
  kernel_size   = 2 * ceil(3 * sigma_spatial) + 1

  FOR each pixel (y, x):
    weighted_sum = 0
    weight_total = 0
    FOR each (dy, dx) in kernel:
      spatial_dist = sqrt(dy^2 + dx^2)
      spatial_w = exp(-spatial_dist^2 / (2 * sigma_spatial^2))
      range_dist = |input[y][x] - input[y+dy][x+dx]|
      range_w = exp(-range_dist^2 / (2 * sigma_range^2))
      w = spatial_w * range_w
      weighted_sum += w * input[y+dy][x+dx]
      weight_total += w
    output[y][x] = weighted_sum / weight_total

Method 1: Non-Local Means (high quality)
  Delegates to OpenCV cv::fastNlMeansDenoising()
  h = estimated_sigma * 0.8   // noise sigma via MAD estimator
  templateWindowSize = 7
  searchWindowSize = 21

Noise Sigma Estimation (MAD):
  sigma = 1.4826 * median(|HH_wavelet_coeffs|)
  // HH = diagonal detail of single-level Haar wavelet
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Uniform image | all same value | output = input (no change) | No noise |
| sigma_spatial=0 | invalid param | XPE_ERR_INVALID_INPUT | Division by zero |
| Very noisy | high sigma | Stronger denoising | Adaptive |
| Edge region | sharp gradient | Preserved by range weight | SRS-FUNC-011 |

---

### 3.3 SWU-2.3: ContrastEnhancer

**설계 목적**: CLAHE 기반 adaptive contrast enhancement. Trace: SRS-FUNC-012

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_contrast_enhance(
    const XpeImageBuffer* input,       // float32
    XpeImageBuffer*       output,      // float32
    int32_t               blockSize,   // default 8
    float                 clipLimit,   // default 2.0
    int32_t               numBins);    // default 256
```

#### Pseudocode

```
CLAHE Algorithm:
  1. Divide image into blockSize x blockSize tiles
  2. FOR each tile:
     a. Compute histogram (numBins bins)
     b. Clip histogram at clipLimit * (tilePixels / numBins)
     c. Redistribute clipped counts evenly across all bins
     d. Build CDF from clipped histogram
     e. Normalize CDF to [0, 1]
  3. FOR each pixel:
     a. Determine 4 surrounding tile centers
     b. Apply bilinear interpolation of 4 tile CDFs
     c. output = interpolated_cdf(input_value)

Implementation: Delegates to OpenCV cv::createCLAHE()
  with validated parameters from ParameterValidator
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| clipLimit=0 | no clipping | Standard HE (not CLAHE) | Edge case |
| blockSize=1 | single pixel tile | XPE_ERR_INVALID_INPUT | Meaningless |
| Uniform tile | all same value | Pass through (flat CDF) | Normal |
| clipLimit > 40 | excessive | Clamp to 40.0 via ParameterValidator | SRS-SAFE-002 |

---

### 3.4 SWU-2.4: EdgeEnhancer

**설계 목적**: Frequency-selective edge enhancement. Trace: SRS-FUNC-013

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_edge_enhance(
    const XpeImageBuffer* input,       // float32
    XpeImageBuffer*       output,      // float32
    float                 gain,        // enhancement strength
    float                 sigma,       // gaussian kernel sigma
    const char*           bodyPart);   // for safe-range lookup
```

#### Pseudocode

```
Unsharp Masking:
  blurred = GaussianBlur(input, sigma)
  highpass = input - blurred
  output = input + gain * highpass

  // Gain clamping (SRS-SAFE-005, HAZ-005)
  maxGain = ParameterValidator::getMaxGain(bodyPart)
  gain = MIN(gain, maxGain)

Multi-kernel variant (optional):
  FOR each sigma_i in [1.0, 2.0, 4.0]:
    blurred_i = GaussianBlur(input, sigma_i)
    hp_i = input - blurred_i
    output += gain_i * hp_i
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| gain > maxGain | excessive | Clamp to body-part max | SRS-SAFE-005 / HAZ-005 |
| gain = 0 | no enhancement | output = input | Pass-through |
| sigma = 0 | invalid | XPE_ERR_INVALID_INPUT | Division by zero |
| Unknown bodyPart | unrecognized | Use most conservative maxGain | Safety default |

---

### 3.5 SWU-2.5: MultiscaleProcessor

**설계 목적**: Laplacian pyramid multiscale frequency processing. Trace: SRS-FUNC-014 (Phase 2)

#### DLL API

```cpp
// xpe_enhance_advanced.dll export
XPE_API XpeErrorCode xpe_multiscale_process(
    const XpeImageBuffer* input,       // float32
    XpeImageBuffer*       output,      // float32
    int32_t               numLevels,   // >= 8
    const float*          gains,       // per-level gain array
    int32_t               gainsCount);
```

#### Pseudocode

```
Laplacian Pyramid Build:
  gaussian[0] = input
  FOR i = 1 TO numLevels:
    gaussian[i] = pyrDown(gaussian[i-1])   // 2x downscale + blur
  FOR i = 0 TO numLevels-1:
    laplacian[i] = gaussian[i] - pyrUp(gaussian[i+1])
  residual = gaussian[numLevels]

Non-linear Gain Application:
  FOR i = 0 TO numLevels-1:
    laplacian[i] = gains[i] * nonlinear_map(laplacian[i])
    // nonlinear_map: sigmoid-like, attenuates extreme values

Reconstruction:
  result = residual
  FOR i = numLevels-1 DOWNTO 0:
    result = pyrUp(result) + laplacian[i]
  output = result
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| numLevels < 1 | invalid | XPE_ERR_INVALID_INPUT | Minimum 1 level |
| Image too small for levels | 64x64 with 8 levels | Reduce levels to fit | Auto-adjust |
| All gains = 1.0 | identity | output = input | Pass-through |
| gainsCount != numLevels | mismatch | XPE_ERR_INVALID_INPUT | Precondition |

---

### 3.6 SWU-2.6: FractionalProcessor

**설계 목적**: Fractional multiscale processing for density transition artifact 제거. Trace: SRS-FUNC-015 (Phase 2)

#### Pseudocode

```
FMP extends MFP with fractional scale decomposition:
  1. Build Laplacian pyramid at fractional octave intervals (e.g., 1/3 octave)
  2. At each fractional level:
     a. Compute local density gradient
     b. Apply adaptive gain based on gradient magnitude
     c. Stronger suppression at density transition zones
  3. Reconstruct from fractional pyramid

Key difference from MFP:
  - Finer frequency resolution at transition boundaries
  - Reduces halo artifacts near bone/soft-tissue boundaries
```

---

### 3.7 SWU-2.7: BodyPartRecognizer

**설계 목적**: CNN 기반 body-part 분류. Trace: SRS-FUNC-016 (Phase 2)

#### DLL API

```cpp
// xpe_ai.dll export (proxy to xpe_ai_worker.exe)
XPE_API XpeErrorCode xpe_recognize_bodypart(
    const XpeImageBuffer* input,
    char*                 bodyPartOut,    // buffer >= 64 chars
    size_t                bodyPartLen,
    float*                confidence);
```

#### Pseudocode

```
1. Resize input to 224x224 (MobileNet-v3 input size)
2. Normalize to [0, 1] range
3. Run ONNX inference via xpe_ai_worker.exe (IPC)
4. Argmax over 15+ categories
5. IF confidence >= 0.95:
     bodyPartOut = predicted_category
   ELSE:
     bodyPartOut = DICOM tag (0018,0015) fallback

IPC Protocol:
  Main process → worker: shared memory image + config JSON
  Worker → main: result JSON { "bodyPart": "CHEST", "confidence": 0.97 }
  Timeout: 5s → fallback to DICOM tag
  Worker crash: restart + use DICOM tag
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Low confidence (<0.95) | ambiguous | Use DICOM tag fallback | SRS-FUNC-016 |
| Worker timeout | >5s | Use DICOM tag + WARNING | Degradation |
| Worker crash | process died | Restart worker + DICOM tag | SRS-SAFE-008 |
| No DICOM tag | missing | Use "UNKNOWN" + default preset | Safety |

---

### 3.8 SWU-2.8: CollimationDetector

**설계 목적**: Beam edge / ROI detection. Trace: SRS-FUNC-016 (Phase 2)

#### Pseudocode

```
1. Compute gradient magnitude: Sobel(input)
2. Threshold gradient at 3-sigma above mean
3. Apply Hough line transform on thresholded image
4. Detect 4 boundary lines (top, bottom, left, right)
5. Compute ROI rectangle from line intersections
6. Store ROI in orchestration sidecar (not XpeImageMetadata)

Validation:
  - ROI area must be >= 10% of total image area
  - ROI must be rectangular (angle tolerance < 5 degrees)
  - IF validation fails: ROI = full image + WARNING
```

---

### 3.9 SWU-2.9: ImageStitcher

**설계 목적**: Panoramic stitching. Trace: SRS-FUNC-017 (Phase 2)

#### Pseudocode

```
INPUT: images[2..4], overlap 10-30%

1. FOR each adjacent pair (i, i+1):
   a. Extract overlap region from both images
   b. Phase correlation for coarse alignment (FFT-based)
   c. Sub-pixel refinement (parabolic peak fitting)
   d. Compute translation vector (dx, dy)

2. Global alignment:
   a. Chain translations into global coordinate system
   b. Compute output canvas size

3. Blending:
   a. FOR each pixel in output canvas:
      - Determine contributing images
      - Apply distance-weighted blending in overlap zone
      - Linear ramp: weight = distance_from_edge / overlap_width

Validation:
  - Cobb angle measurement error <= 2 degrees
  - Stitching seam visibility: not perceptible at clinical viewing
```

---

### 3.10 SWU-2.10: ExposureIndexCalc

**설계 목적**: IEC 62494-1 EI/DI 계산. Trace: SRS-FUNC-016 (Phase 2)

#### Pseudocode

```
IEC 62494-1 Exposure Index Calculation:
  1. Determine VOI (Value of Interest) region:
     - Use CollimationDetector ROI if available
     - Exclude direct-exposure and collimated areas
  2. Compute median pixel value in VOI region
  3. EI = c * median_value   // c = manufacturer calibration constant
  4. DI = 10 * log10(EI / EI_target)   // Deviation Index

EI_target: body-part specific target exposure
  - Stored in xpe_config.json per body-part preset
```

---

### 3.11 SWU-2.11: BoneSuppressionEngine

**설계 목적**: DL 기반 virtual dual-energy subtraction. Trace: SRS-FUNC-018 (Phase 3)

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_bone_suppress(
    const XpeImageBuffer* input,       // float32
    XpeImageBuffer*       output,      // float32 (soft-tissue only)
    int32_t               enabled);    // 0=skip, 1=apply
```

#### Pseudocode

```
IF NOT enabled: output = input; RETURN

1. Preprocess: normalize input to [0, 1]
2. Tile input into 256x256 patches with 32-pixel overlap
3. FOR each patch:
   a. Send to xpe_ai_worker.exe via IPC
   b. Worker runs Residual U-Net ONNX model
   c. Receive bone component prediction
4. Reassemble patches with overlap blending
5. soft_tissue = input - bone_prediction
6. Validate: PSNR >= 33dB, SSIM >= 0.97 vs reference (if available)
7. Tag output as AI-processed (SRS-SAFE-008)

Fallback:
  AI worker failure → return input unchanged + SRS-ALERT-004
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| enabled=0 | disabled | Pass-through | Toggle control |
| Worker crash | AI failure | Return input + alert | SRS-SAFE-008 / HAZ-008 |
| Non-chest image | wrong body part | Skip + return input | Only applicable to PA/AP chest |
| Low confidence | uncertain result | Return input + WARNING | Safety |

---

### 3.12 SWU-2.12: DLDenoiser

**설계 목적**: DnCNN 기반 low-dose denoising. Trace: SRS-FUNC-018 (Phase 3)

#### Pseudocode

```
Similar to BoneSuppressionEngine architecture:
  1. Tile input into patches
  2. Run DnCNN model via xpe_ai_worker.exe
  3. Model predicts noise component
  4. output = input - predicted_noise
  5. Tag as AI-processed

Shares AI worker infrastructure with SWU-2.11
```

---

## 4. SWI-3: Display Processing Module

### 4.1 SWU-3.1: ModalityLUT

**설계 목적**: DICOM Modality LUT 적용. Trace: SRS-FUNC-020

#### DLL API

```cpp
// xpe_display.dll export
XPE_API XpeErrorCode xpe_apply_modality_lut(
    const XpeImageBuffer* input,       // uint16 stored pixel
    XpeImageBuffer*       output,      // float32
    float                 rescaleSlope,
    float                 rescaleIntercept);
```

#### Pseudocode

```
DICOM PS3.3 C.11.1.1.1:
  output[y][x] = rescaleSlope * (float)input[y][x] + rescaleIntercept

  rescaleSlope    = DICOM tag (0028,1053), default 1.0
  rescaleIntercept = DICOM tag (0028,1052), default 0.0
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Default (1.0, 0.0) | identity | output = input | Normal |
| Slope = 0 | invalid | XPE_ERR_INVALID_INPUT | Division by zero in reverse |
| Negative intercept | valid | output may be negative | DICOM allows |

---

### 4.2 SWU-3.2: VoiLUT

**설계 목적**: Window/Level 변환. Trace: SRS-FUNC-021

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_apply_voi_lut(
    const XpeImageBuffer* input,       // float32
    XpeImageBuffer*       output,      // uint8 or uint16
    float                 windowCenter,
    float                 windowWidth,
    int32_t               voiType);    // 0=LINEAR, 1=LINEAR_EXACT, 2=SIGMOID
```

#### Pseudocode

```
Type 0: LINEAR (DICOM default)
  IF input <= center - 0.5 - (width - 1) / 2:
    output = minOut
  ELSE IF input > center - 0.5 + (width - 1) / 2:
    output = maxOut
  ELSE:
    output = ((input - (center - 0.5)) / (width - 1) + 0.5) * (maxOut - minOut) + minOut

Type 1: LINEAR_EXACT
  IF input <= center - width / 2:
    output = minOut
  ELSE IF input >= center + width / 2:
    output = maxOut
  ELSE:
    output = ((input - center) / width + 0.5) * (maxOut - minOut) + minOut

Type 2: SIGMOID
  output = maxOut / (1 + exp(-4 * (input - center) / width))

W/L range check (SRS-SAFE-006, HAZ-006):
  IF windowWidth <= 0 OR windowWidth > maxAllowed:
    Emit SRS-ALERT-002 WARNING
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| windowWidth = 0 | invalid | XPE_ERR_INVALID_INPUT + WARNING | SRS-SAFE-006 |
| windowWidth > max | out of range | Clamp + SRS-ALERT-002 | HAZ-006 |
| windowCenter out of data range | extreme | All min or all max output | DICOM behavior |
| Interactive drag | 60fps | <= 16ms response | SRS-PERF-003 |

---

### 4.3 SWU-3.3: PresentationLUT

**설계 목적**: GSDF P-Value 변환. Trace: SRS-FUNC-022, SRS-FUNC-023

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_apply_presentation_lut(
    const XpeImageBuffer* input,
    XpeImageBuffer*       output,
    int32_t               photometricInterpretation,  // 0=MONO1, 1=MONO2
    int32_t               gsdfEnabled);
```

#### Pseudocode

```
DICOM PS3.14 GSDF:
  1. Convert input to Luminance (cd/m2) using display calibration
  2. Apply GSDF curve: P-Value = f(Luminance)
     - GSDF ensures perceptually linear JND spacing
  3. IF photometricInterpretation == MONOCHROME1:
       output = maxVal - pValue   // invert for white=low density

GSDF compliance check (SRS-SAFE-007, HAZ-007):
  IF NOT gsdfEnabled:
    Emit SRS-ALERT-003 WARNING ("Non-GSDF display detected")

Toggle support (SRS-SAFE-009, HAZ-009):
  Maintain both original and processed buffers
  Toggle latency: < 100ms
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Non-GSDF display | gsdfEnabled=0 | Linear LUT + WARNING | SRS-SAFE-007 |
| MONOCHROME1 | inversion needed | Invert output | SRS-FUNC-023 |
| Toggle request | switch view | < 100ms switch | SRS-SAFE-009 |

---

### 4.4 SWU-3.4: LUTManager

**설계 목적**: Body-part preset 관리. Trace: SRS-FUNC-021

#### Internal Class

```cpp
namespace xpe::display {

struct VoiPreset {
    std::string name;
    float windowCenter;
    float windowWidth;
    int32_t voiType;   // LINEAR, LINEAR_EXACT, SIGMOID
};

class LUTManager {
public:
    /// @brief  Load preset database from JSON config
    XpeErrorCode loadPresets(const std::filesystem::path& configPath);

    /// @brief  Get presets for body part (>= 20 presets total)
    std::span<const VoiPreset> getPresets(std::string_view bodyPart) const;

    /// @brief  Auto-select best preset for body part
    const VoiPreset& autoSelect(std::string_view bodyPart) const;

    /// @brief  CRUD for custom user presets
    XpeErrorCode addCustomPreset(std::string_view bodyPart, const VoiPreset& preset);
    XpeErrorCode removeCustomPreset(std::string_view bodyPart, std::string_view name);

private:
    std::unordered_map<std::string, std::vector<VoiPreset>> presets_;
};

} // namespace xpe::display
```

---

## 5. SWI-4: DICOM I/O Module

### 5.1 SWU-4.1: DicomReader

**설계 목적**: DICOM DX IOD 파싱. Trace: SRS-FUNC-030

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_dicom_read(
    const char*      filepath,
    XpeImageBuffer*  imageOut,
    char*            metadataJson,     // JSON string output
    size_t           metadataJsonLen);
```

#### Pseudocode

```
1. Open DICOM file via dcmtk DcmFileFormat::loadFile()
2. Validate SOP Class UID == DX IOD
3. Read pixel data:
   a. Get (7FE0,0010) Pixel Data
   b. Decode transfer syntax (Explicit VR LE, J2K Lossless)
   c. Populate XpeImageBuffer from pixel data
4. Read metadata tags:
   a. (0028,0010) Rows, (0028,0011) Columns
   b. (0028,0100) BitsAllocated, (0028,0101) BitsStored
   c. (0018,0015) BodyPartExamined
   d. (0028,0004) PhotometricInterpretation
   e. (0028,1053/1052) Rescale Slope/Intercept
5. Return as XpeImageBuffer + JSON metadata
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Non-DX SOP | wrong modality | XPE_ERR_UNSUPPORTED_FORMAT | SRS-FUNC-030 |
| Corrupt pixel data | truncated | XPE_ERR_IO_FAILED | Error handling |
| Missing Type 1 tag | mandatory tag absent | XPE_ERR_INVALID_INPUT | DICOM conformance |
| J2K compressed | transfer syntax | Auto-decompress | SRS-FUNC-032 |

---

### 5.2 SWU-4.2: DicomWriter

**설계 목적**: DICOM DX IOD 생성. Trace: SRS-FUNC-030, SRS-FUNC-032

#### DLL API

```cpp
XPE_API XpeErrorCode xpe_dicom_write(
    const XpeImageBuffer*  image,
    const char*            metadataJson,
    const char*            outputPath,
    int32_t                transferSyntax);  // 0=ExplicitVRLE, 1=J2KLossless
```

#### Pseudocode

```
1. Create DcmFileFormat via dcmtk
2. Populate all Type 1 tags (mandatory):
   a. Patient, Study, Series, Instance UIDs
   b. Image Pixel Module
   c. DX Series/Image modules
3. Populate Type 2 tags (required, may be empty)
4. Set ghost correction flag: (0028,0303) = "MODIFIED" if processed
5. Set AI-processed private tag if applicable (SRS-SAFE-008)
6. Encode pixel data in selected transfer syntax
7. Write file
8. Verify: re-read and validate tag completeness
```

---

### 5.3 SWU-4.3: PresentationStateIO

**설계 목적**: GSPS 생성/적용. Trace: SRS-FUNC-031

#### Pseudocode

```
Create GSPS:
  1. Create Grayscale Softcopy Presentation State SOP
  2. Store current W/L, LUT, annotations
  3. Reference source image SOP Instance UID
  4. Write as separate DICOM file

Apply GSPS:
  1. Read GSPS file
  2. Match referenced SOP Instance UID
  3. Apply stored VOI LUT parameters
  4. Apply stored annotations/overlays
```

---

### 5.4 SWU-4.4: DicomNetworkSCU

**설계 목적**: DICOM network operations. Trace: SRS-IF-002, SRS-IF-003, SRS-SEC-001

#### Pseudocode

```
C-STORE SCU:
  1. Establish association with PACS (TLS 1.2+ if configured)
  2. Negotiate presentation context (DX IOD + transfer syntax)
  3. Send C-STORE request with DICOM file
  4. IF association fail: queue + retry (3 attempts, exponential backoff)
  5. IF all retries fail: XPE_ERR_NETWORK_FAILED + SRS-ALERT-006

C-FIND SCU (MWL):
  1. Establish association with worklist server
  2. Send C-FIND request with query keys
  3. Receive matching worklist items
  4. Parse and return as structured data
```

---

## 6. SWI-5: Common Infrastructure

### 6.1 SWU-5.1: MemoryPool

**설계 목적**: Pre-allocated image buffer pool. Trace: SRS-SAFE-001, SRS-PERF-004

#### Internal Class

```cpp
namespace xpe::infra {

class MemoryPool {
public:
    /// @brief  Initialize pool with pre-allocated buffers
    /// @param  maxImages     Maximum concurrent images (default: 4)
    /// @param  maxPixels     Max pixels per image (default: 4096*4096)
    XpeErrorCode init(uint32_t maxImages = 4, uint32_t maxPixels = 4096 * 4096);

    /// @brief  Acquire buffer from pool (non-owning pointer)
    ///         Input buffers are marked read-only (SRS-SAFE-001)
    XpeImageBuffer* acquire(uint32_t width, uint32_t height, XpePixelFormat format);

    /// @brief  Release buffer back to pool
    void release(XpeImageBuffer* buffer);

    /// @brief  Peak memory usage in bytes
    size_t peakUsage() const noexcept;

    void shutdown();

private:
    struct PoolEntry {
        XpeImageBuffer buffer;
        bool inUse = false;
    };
    std::vector<PoolEntry> pool_;
    std::mutex mutex_;
};

} // namespace xpe::infra
```

#### Edge Case

| Case | Input | Action | Rationale |
|------|-------|--------|-----------|
| Pool exhausted | no free buffer | XPE_ERR_OUT_OF_MEMORY | SRS-PERF-004 |
| Peak > 2GB | memory limit | Reject + XPE_ERR_OUT_OF_MEMORY | SRS-PERF-004 |
| Double release | already free | Ignore (idempotent) | Safety |

---

### 6.2 SWU-5.2: ThreadPool

**설계 목적**: Task-based parallel execution. Trace: SRS-PERF-001..006

#### Internal Class

```cpp
namespace xpe::infra {

class ThreadPool {
public:
    /// @param  numThreads  0 = auto (hardware_concurrency - 1)
    explicit ThreadPool(uint32_t numThreads = 0);
    ~ThreadPool();

    /// @brief  Submit task and get future
    template<typename F>
    auto submit(F&& task) -> std::future<std::invoke_result_t<F>>;

    /// @brief  Wait for all pending tasks to complete
    void waitAll();

    uint32_t threadCount() const noexcept;

private:
    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;
};

} // namespace xpe::infra
```

---

### 6.3 SWU-5.3: ErrorHandler

**설계 목적**: Centralized error/alert management. Trace: SRS-SAFE-003, SRS-ALERT-*

#### Internal Class

```cpp
namespace xpe::infra {

class ErrorHandler {
public:
    /// @brief  Post alert (thread-safe)
    void postAlert(XpeAlertSeverity severity, std::string_view message);

    /// @brief  Get pending alerts (for C ABI polling via xpe_get_pending_alert)
    std::vector<std::pair<XpeAlertSeverity, std::string>> getPendingAlerts();

    /// @brief  Clear all pending alerts
    void clearAlerts();

    /// @brief  Module boundary exception handler
    ///         Catches C++ exceptions, converts to XpeErrorCode
    static XpeErrorCode catchAll(std::function<XpeErrorCode()> fn) noexcept;

private:
    std::vector<std::pair<XpeAlertSeverity, std::string>> alerts_;
    std::mutex mutex_;
};

} // namespace xpe::infra
```

---

### 6.4 SWU-5.4: Logger

**설계 목적**: Audit trail logging. Trace: SRS-SEC-003

#### Internal Class

```cpp
namespace xpe::infra {

class Logger {
public:
    static Logger& instance();

    void init(const std::filesystem::path& logDir, spdlog::level::level_enum level);

    void info(std::string_view msg);
    void warn(std::string_view msg);
    void error(std::string_view msg);

    /// @brief  Audit log for parameter changes (SRS-SEC-003)
    void audit(std::string_view paramName, std::string_view oldValue,
               std::string_view newValue, std::string_view user);

private:
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<spdlog::logger> auditLogger_;   // separate audit file
};

} // namespace xpe::infra
```

---

### 6.5 SWU-5.5: ParameterValidator

**설계 목적**: Body-part별 safe range 검증. Trace: SRS-SAFE-002, SRS-SAFE-005

#### Internal Class

```cpp
namespace xpe::infra {

struct ParamRange {
    float minVal;
    float maxVal;
    float defaultVal;
};

class ParameterValidator {
public:
    XpeErrorCode loadRanges(const std::filesystem::path& configPath);

    /// @brief  Validate and clamp parameter to safe range
    /// @return Clamped value (may differ from input)
    float validate(std::string_view bodyPart, std::string_view paramName,
                   float value);

    /// @brief  Get range for GUI slider binding (SRS-USE-001)
    ParamRange getRange(std::string_view bodyPart, std::string_view paramName) const;

private:
    // bodyPart -> paramName -> range
    std::unordered_map<std::string,
        std::unordered_map<std::string, ParamRange>> ranges_;
};

} // namespace xpe::infra
```

---

### 6.6 SWU-5.6: ConfigManager

**설계 목적**: System/user config persistence. Trace: SRS-SEC-002

#### Internal Class

```cpp
namespace xpe::infra {

class ConfigManager {
public:
    XpeErrorCode load(const std::filesystem::path& configPath);
    XpeErrorCode save();

    nlohmann::json get(std::string_view key) const;
    void set(std::string_view key, const nlohmann::json& value);

    /// @brief  Verify config file integrity (SHA-256)
    bool verifyChecksum() const;

private:
    nlohmann::json config_;
    std::filesystem::path path_;
    std::string expectedHash_;
};

} // namespace xpe::infra
```

---

### 6.7 SWU-5.7: PipelineOrchestrator

**설계 목적**: Processing stage sequencing. Trace: SRS-PERF-002

#### Internal Class

```cpp
namespace xpe::infra {

class PipelineOrchestrator {
public:
    /// @brief  Execute full pipeline
    /// @param  input   Raw detector image
    /// @param  config  Pipeline configuration (which stages enabled)
    /// @return Processed image + metadata
    XpeErrorCode execute(
        const XpeImageBuffer& input,
        const nlohmann::json& config,
        XpeImageBuffer&       output,
        XpeImageMetadata&     metadata);

    /// @brief  Get processing time breakdown (SRS-PERF-002)
    struct TimingInfo {
        float preprocessMs;
        float coreMs;
        float displayMs;
        float dicomMs;
        float totalMs;
    };
    TimingInfo getLastTiming() const noexcept;

private:
    // Pipeline order (ALG-SPEC-001 v3.0.0-ds2):
    // Pre: Offset -> Gain -> Defect -> Ghost
    // Core: Log -> Noise -> Contrast -> Edge -> (MFP -> FMP -> BodyPart -> Stitch -> Bone -> DL)
    // Display: ModalityLUT -> VoiLUT -> PresentationLUT
    // Output: DICOM write

    MemoryPool& memPool_;
    ThreadPool& threadPool_;
};

} // namespace xpe::infra
```

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| 1.0 | 2026-04-14 | XPE Team | Initial release — 32 SWU detailed design |

---

*Document End — XPE-SDD-002 v1.0*
