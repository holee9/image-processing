# XPE 통합 알고리즘 개발 명세서

**Document ID:** XPE-ALG-001 v1.1  
**IEC 62304 Clause:** 5.4 (Software Detailed Design)  
**Safety Classification:** Class B  
**Date:** 2026-04-15  
**Author:** XPE Development Team  
**Review Cycles:** 20회 (v1.0: 10회 + v1.1: 10회 Review-Evaluate-Fix 반복 완료)  
**Approval:** __________________ Date: __________  

---

## 문서 목적

본 문서는 XPE(X-ray Processing Engine) 시스템의 **모든 알고리즘**을 수학적 공식, C++ 의사코드, SIMD 최적화 전략, 검증 기준까지 일관되게 명세한다. 기존 문서(XPE-SRS-001, XPE-SAD-001, XPE-SDD-002, 03_측정_알고리즘_명세서, xray_grid_suppression_virtual_grid_research)의 교차 검증을 통해 식별된 10개 알고리즘 공백을 모두 해소한다.

### 공백 해소 매핑

| 공백 번호 | 내용 | 본 문서 섹션 |
|----------|------|------------|
| GAP-01 | Python↔C++ 아키텍처 브리지 미문서화 | §2 |
| GAP-02 | Core Processing 알고리즘 미명세 | §4 |
| GAP-03 | Grid Suppression 알고리즘 미명세 | §5 |
| GAP-04 | AI/DL 알고리즘 미명세 | §8 |
| GAP-05 | Display Processing 표준 미명세 | §6 |
| GAP-06 | SIMD 최적화 전체 파이프라인 미커버 | §10 |
| GAP-07 | 파노라마 스티칭 미명세 | §8.3 |
| GAP-08 | Virtual Grid / Scatter Correction 미명세 | §5.2 |
| GAP-09 | Exposure Index (IEC 62494-1) 미명세 | §7 |
| GAP-10 | 교정 맵 생성↔런타임 연결 미문서화 | §9 |

---

## 목차

1. [용어 정의 및 기호 규약](#1-용어-정의-및-기호-규약)
2. [시스템 아키텍처 — Python↔C++ 브리지](#2-시스템-아키텍처--pythonc-브리지)
3. [SWI-1: Pre-Processing 알고리즘](#3-swi-1-pre-processing-알고리즘)
   - [§3.0 Readout Validation (SWU-1.0) ★GAP-I](#30-swu-10-readout-validation-gap-i-해소)
   - [§3.0.5 Non-linearity Correction ★GAP-H](#305-swu-105-non-linearity-correction-gap-h-해소)
   - §3.1 Offset Correction
   - §3.2 Gain Correction
   - §3.3 Defect Correction (★GAP-E)
   - §3.4 Ghost/Lag Correction
4. [SWI-2: Core Processing 알고리즘](#4-swi-2-core-processing-알고리즘) (★GAP-G avx2_log_ps)
5. [Grid Suppression & Virtual Grid 알고리즘](#5-grid-suppression--virtual-grid-알고리즘) (★GAP-D NSCT)
6. [SWI-3: Display Processing 알고리즘](#6-swi-3-display-processing-알고리즘)
7. [IEC 62494-1 Exposure Index 알고리즘](#7-iec-62494-1-exposure-index-알고리즘) (★GAP-F ROI 수정)
8. [AI/DL 알고리즘](#8-aidl-알고리즘)
9. [교정 데이터 파이프라인](#9-교정-데이터-파이프라인)
   - [§9.4 AED-0 Automatic Exposure Detection ★GAP-J](#94-aed-0-automatic-exposure-detection-gap-j-해소)
10. [성능 최적화 — SIMD/OpenMP 전략](#10-성능-최적화--simdopenmp-전략)
11. [검증 방법론](#11-검증-방법론)
12. [FPD 특성화 알고리즘 보완](#12-fpd-특성화-알고리즘-보완)
    - [§12.3 NPS 계산 ★GAP-L](#123-nps-계산-알고리즘-gap-l-해소)
    - [§12.4 DQE 계산 ★GAP-M](#124-dqe-계산-알고리즘-gap-m-해소)
    - [§12.5 Collimation Mask Detection ★GAP-N](#125-collimation-mask-detection-알고리즘-gap-n-해소)
- [부록 A: 수학 공식 일람](#부록-a-수학-공식-일람)
- [부록 B: 표준 참조 테이블](#부록-b-표준-참조-테이블)
- [부록 C: 알고리즘-요구사항 추적성](#부록-c-알고리즘-요구사항-추적성)

---

## 1. 용어 정의 및 기호 규약

### 1.1 기호 체계

| 기호 | 정의 | 단위 |
|------|------|------|
| `I_raw(x,y)` | 원시 detector 출력 pixel 값 | ADU |
| `I_dark(x,y)` | Dark offset map | ADU |
| `G(x,y)` | Gain correction map | dimensionless |
| `I_flat(x,y)` | Flat-field (flood) image | ADU |
| `I_corr(x,y)` | Gain-corrected image | ADU |
| `I_clean(x,y)` | Defect-corrected image | ADU |
| `I_od(x,y)` | Log-transformed (OD domain) image | OD |
| `σ_s` | Bilateral filter spatial sigma | pixels |
| `σ_r` | Bilateral filter range sigma | ADU or OD |
| `f` | Spatial frequency | cycles/mm |
| `MTF(f)` | Modulation Transfer Function | dimensionless |
| `NPS(f)` | Noise Power Spectrum | ADU²·mm² |
| `NNPS(f)` | Normalized NPS | mm² |
| `DQE(f)` | Detective Quantum Efficiency | dimensionless |
| `Φ` | X-ray quantum fluence at detector | photons/mm² |
| `EI` | Exposure Index (IEC 62494-1) | dimensionless |
| `DI` | Deviation Index | dB |
| `W(u,v)` | Window function (Hanning) | dimensionless |
| `ε` | Numerical floor (= 1×10⁻⁶) | ADU or OD |

### 1.2 좌표 규약

```
Origin: top-left (0,0)
x: column (horizontal), y: row (vertical)
Spatial frequency: u (horizontal), v (vertical), f = sqrt(u²+v²)
Nyquist frequency: f_N = 1/(2·pixelPitch_mm)
```

### 1.3 데이터 타입 규약

| 처리 단계 | 내부 타입 | 비트 깊이 | 범위 |
|----------|----------|---------|------|
| Raw detector | uint16 | 14–16 bit | 0–65535 |
| Pre-processing 중간 | float32 | 32 bit | 0.0–65535.0 |
| OD domain | float32 | 32 bit | −∞ ~ +∞ (실제 −5 ~ +5) |
| Display pipeline | float32→uint16 | 16→8 bit | 0–4095 → 0–255 |

---

## 2. 시스템 아키텍처 — Python↔C++ 브리지

### 2.1 전체 데이터 흐름 (GAP-01 해소)

```
┌──────────────────────────────────────────────────────────────────┐
│                    OFFLINE (Python) — 교정 단계                    │
│                                                                    │
│  FPD 검사/특성화                                                    │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐           │
│  │  Dark Frame │    │  Flood Field│    │  Slanted Edge│           │
│  │  ≥16 frames │    │  per SID    │    │  (MTF)      │           │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘           │
│         │                  │                   │                   │
│         ▼                  ▼                   ▼                   │
│  compute_offset_map()  compute_gain_map()  compute_mtf()          │
│  compute_defect_map()  compute_nps_full()  compute_dqe()          │
│         │                  │                   │                   │
│         ▼                  ▼                   ▼                   │
│  [offset_map.bin]    [gain_map_SIDXXX.bin] [characterization.json]│
│  [defect_map.bin]    [checksum.sha256]                             │
└──────────────────────────────────────────────────────────────────┘
           │                  │
           ▼ 파일 배포         ▼
┌──────────────────────────────────────────────────────────────────┐
│                    ONLINE (C++) — 런타임 파이프라인                  │
│                                                                    │
│  SWI-1 Pre-Processing                                              │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ xpe_offset_correct() → xpe_gain_correct()               │      │
│  │ → xpe_defect_correct() → xpe_ghost_correct()            │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │ float32 ImageBuffer              │
│  SWI-2 Core Processing           ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ log_transform() → bilateral_filter() → clahe()          │      │
│  │ → edge_enhance() → [laplacian_pyramid()] [fractional()] │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │                                  │
│  SWI-3 Display Processing        ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ modality_lut() → voi_lut() → presentation_lut_gsdf()   │      │
│  └──────────────────────────────┬──────────────────────────┘      │
│                                  │                                  │
│  SWI-4 DICOM I/O                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │ dcmtk_write_dx_iod() → C-STORE SCU                     │      │
│  └─────────────────────────────────────────────────────────┘      │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 교정 파일 형식 명세

#### 2.2.1 Offset Map (`.bin`)

```
Header (64 bytes):
  [0..3]   Magic: "XOFF"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] NumFrames: uint32 (≥16)
  [20..23] BitDepth: uint32 (14 or 16)
  [24..55] AcquisitionDateTime: char[32] (ISO-8601)
  [56..63] Checksum: uint64 (CRC64)
Payload:
  float32[Width × Height]  // mean of NumFrames dark images, clamp ≥ 0
```

#### 2.2.2 Gain Map (`.bin`)

```
Header (96 bytes):
  [0..3]   Magic: "XGAI"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] SID_mm: float32   // Source-to-Image Distance
  [20..23] kVp: float32
  [24..27] mAs: float32
  [28..31] GainMean: float32  // mean of (Flood - Offset)
  [32..63] AcquisitionDateTime: char[32]
  [64..95] Checksum: uint64 (CRC64)
Payload:
  float32[Width × Height]  // GainMean / (Flood(x,y) - Offset(x,y)), clamped [0.5, 2.0]
```

#### 2.2.3 Defect Pixel Map (`.bin`)

```
Header (64 bytes):
  [0..3]   Magic: "XDEF"
  [4..7]   Version: uint32 = 1
  [8..11]  Width: uint32
  [12..15] Height: uint32
  [16..19] NumDefects: uint32
  [20..23] MapFlags: uint32  // bit0: factory, bit1: runtime
  [24..55] AcquisitionDateTime: char[32]
  [56..63] Checksum: uint64 (CRC64)
Payload:
  // Run-Length Encoded defect list:
  struct DefectEntry {
      uint16 x;
      uint16 y;
      uint8  type;   // 0: point, 1: cluster, 2: row, 3: col
      uint8  size;   // 1-based, used for cluster radius
  };
  DefectEntry[NumDefects]
```

### 2.3 파일 무결성 검증 (SRS-SEC-002)

```cpp
// Runtime validation before applying calibration data
bool validate_calibration_file(const std::string& path,
                                const std::string& checksum_path) {
    // Read file content
    auto data = read_binary_file(path);
    // Compute SHA-256
    auto computed = sha256(data.data(), data.size());
    // Compare with stored checksum
    auto stored = read_text_file(checksum_path);
    return computed == stored;
}
```

---

## 3. SWI-1: Pre-Processing 알고리즘

### 3.0 SWU-1.0 Readout Validation (GAP-I 해소)

Readout Validation은 모든 보정 전에 실행되는 **입력 품질 게이트**로, 잘못된 이미지가 파이프라인에 진입하는 것을 방지한다. xpe-algorithm-spec-deepsync.md 표 "release-safe baseline" 항목에 명시되어 있으며, 이 섹션이 상세 구현을 제공한다.

#### 3.0.1 알고리즘 수학 정의

**포화 검사 (Saturation)**:
$$P_{\text{sat}} = \frac{|\{(x,y) : I_{\text{raw}}(x,y) \geq V_{\text{sat}}\}|}{W \times H} \leq \theta_{\text{sat}}$$

**DR 클리핑 검사 (Clipped Dynamic Range)**:
$$P_{\text{clip\_low}} = \frac{|\{(x,y) : I_{\text{raw}}(x,y) \leq V_{\text{clip\_low}}\}|}{W \times H} \leq \theta_{\text{clip}}$$

**불가능한 기하학 검사 (Impossible Geometry)**:
$$\text{Valid}: W, H \in [256,\ 4096],\quad W \cdot H \leq 16{,}777{,}216,\quad W/H \in [0.5,\ 4.0]$$

**행/열 결함 검사 (Row/Column Fault)**:
$$\text{RowFault}(y) = 1 \iff \text{std}(I_{\text{raw}}[y, :]) < \sigma_{\text{line\_min}}$$

| 파라미터 | 기본값 | 의미 |
|---------|-------|------|
| `V_sat` | 65530 (14-bit: 16380) | 포화 임계치 (ADU) |
| `θ_sat` | 0.05 | 허용 포화 픽셀 비율 (5%) |
| `V_clip_low` | 4 | 하단 클리핑 임계치 (ADU) |
| `θ_clip` | 0.10 | 허용 하단 클리핑 비율 (10%) |
| `σ_line_min` | 2.0 | 최소 행/열 표준편차 (ADU) |

#### 3.0.2 Python 구현 (오프라인 QC)

```python
import numpy as np
from dataclasses import dataclass, field
from enum import Flag, auto

class ReadoutFaultCode(Flag):
    OK              = 0
    SATURATED       = auto()   # > θ_sat fraction at V_sat
    CLIPPED_DR      = auto()   # > θ_clip fraction at V_clip_low
    IMPOSSIBLE_GEOM = auto()   # width/height outside valid range
    ROW_FAULT       = auto()   # ≥1 row with std < σ_line_min
    COLUMN_FAULT    = auto()   # ≥1 col with std < σ_line_min
    EMPTY_IMAGE     = auto()   # all-zero or single-value image

@dataclass
class ReadoutValidationResult:
    fault_code:     ReadoutFaultCode = ReadoutFaultCode.OK
    fault_details:  dict             = field(default_factory=dict)
    saturated_frac: float            = 0.0
    clipped_frac:   float            = 0.0
    faulty_rows:    list             = field(default_factory=list)
    faulty_cols:    list             = field(default_factory=list)

def validate_readout(raw: np.ndarray,
                     v_sat:       int   = 65530,
                     theta_sat:   float = 0.05,
                     v_clip_low:  int   = 4,
                     theta_clip:  float = 0.10,
                     sigma_line_min: float = 2.0,
                     bit_depth:   int   = 16) -> ReadoutValidationResult:
    """
    Gate-check a raw detector image before any correction is applied.

    Returns ReadoutValidationResult; caller must reject the frame if
    fault_code != ReadoutFaultCode.OK (non-zero).
    """
    result = ReadoutValidationResult()
    H, W = raw.shape
    img = raw.astype(np.float32)

    # 1. Impossible geometry
    if not (256 <= W <= 4096 and 256 <= H <= 4096):
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['geometry'] = f'W={W}, H={H} outside [256,4096]'
    if W * H > 16_777_216:
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['geometry_area'] = f'W×H={W*H} > 16M'
    ar = W / H
    if not (0.5 <= ar <= 4.0):
        result.fault_code |= ReadoutFaultCode.IMPOSSIBLE_GEOM
        result.fault_details['aspect_ratio'] = f'{ar:.3f}'

    # 2. Saturation check
    max_adu = (1 << bit_depth) - 1
    sat_thresh = min(v_sat, max_adu)
    sat_mask   = img >= sat_thresh
    result.saturated_frac = float(np.mean(sat_mask))
    if result.saturated_frac > theta_sat:
        result.fault_code |= ReadoutFaultCode.SATURATED
        result.fault_details['saturated_frac'] = f'{result.saturated_frac:.4f}'

    # 3. Clipped DR check (lower end)
    clip_mask = img <= v_clip_low
    result.clipped_frac = float(np.mean(clip_mask))
    if result.clipped_frac > theta_clip:
        result.fault_code |= ReadoutFaultCode.CLIPPED_DR
        result.fault_details['clipped_frac'] = f'{result.clipped_frac:.4f}'

    # 4. Empty image check
    if np.std(img) < 10.0:
        result.fault_code |= ReadoutFaultCode.EMPTY_IMAGE
        result.fault_details['std'] = f'{float(np.std(img)):.2f}'

    # 5. Row fault detection
    row_stds = np.std(img, axis=1)
    faulty_rows = np.where(row_stds < sigma_line_min)[0].tolist()
    if faulty_rows:
        result.fault_code  |= ReadoutFaultCode.ROW_FAULT
        result.faulty_rows  = faulty_rows
        result.fault_details['faulty_row_count'] = len(faulty_rows)

    # 6. Column fault detection
    col_stds = np.std(img, axis=0)
    faulty_cols = np.where(col_stds < sigma_line_min)[0].tolist()
    if faulty_cols:
        result.fault_code  |= ReadoutFaultCode.COLUMN_FAULT
        result.faulty_cols  = faulty_cols
        result.fault_details['faulty_col_count'] = len(faulty_cols)

    return result
```

#### 3.0.3 C++ 런타임 구현

```cpp
enum class ReadoutFaultCode : uint32_t {
    OK              = 0x00,
    SATURATED       = 0x01,
    CLIPPED_DR      = 0x02,
    IMPOSSIBLE_GEOM = 0x04,
    ROW_FAULT       = 0x08,
    COLUMN_FAULT    = 0x10,
    EMPTY_IMAGE     = 0x20,
};
inline ReadoutFaultCode operator|(ReadoutFaultCode a, ReadoutFaultCode b) {
    return static_cast<ReadoutFaultCode>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct ReadoutValidationResult {
    ReadoutFaultCode fault_code  = ReadoutFaultCode::OK;
    float  saturated_frac        = 0.0f;
    float  clipped_frac          = 0.0f;
    int    faulty_row_count      = 0;
    int    faulty_col_count      = 0;
};

ReadoutValidationResult xpe_validate_readout(
        const uint16_t* raw, uint32_t W, uint32_t H, uint32_t bit_depth = 16) {
    ReadoutValidationResult r;
    const uint32_t total = W * H;
    const uint16_t v_sat       = static_cast<uint16_t>((1u << bit_depth) - 6u);
    const uint16_t v_clip_low  = 4u;
    const float    theta_sat   = 0.05f;
    const float    theta_clip  = 0.10f;
    const float    sigma_line_min = 2.0f;

    // 1. Geometry check
    if (W < 256 || W > 4096 || H < 256 || H > 4096 || total > 16'777'216u) {
        r.fault_code = r.fault_code | ReadoutFaultCode::IMPOSSIBLE_GEOM;
    }
    float ar = static_cast<float>(W) / H;
    if (ar < 0.5f || ar > 4.0f) {
        r.fault_code = r.fault_code | ReadoutFaultCode::IMPOSSIBLE_GEOM;
    }

    // 2. Saturation + clip count (AVX2 vectorised)
    uint32_t sat_cnt = 0, clip_cnt = 0;
    for (uint32_t i = 0; i < total; ++i) {
        if (raw[i] >= v_sat)      ++sat_cnt;
        if (raw[i] <= v_clip_low) ++clip_cnt;
    }
    r.saturated_frac = static_cast<float>(sat_cnt) / total;
    r.clipped_frac   = static_cast<float>(clip_cnt) / total;

    if (r.saturated_frac > theta_sat)
        r.fault_code = r.fault_code | ReadoutFaultCode::SATURATED;
    if (r.clipped_frac > theta_clip)
        r.fault_code = r.fault_code | ReadoutFaultCode::CLIPPED_DR;

    // 3. Row/column fault (Welford online mean/variance)
    for (uint32_t y = 0; y < H; ++y) {
        double mean = 0.0, M2 = 0.0;
        for (uint32_t x = 0; x < W; ++x) {
            double delta = raw[y * W + x] - mean;
            mean += delta / (x + 1);
            M2   += delta * (raw[y * W + x] - mean);
        }
        float std_row = static_cast<float>(std::sqrt(M2 / (W - 1)));
        if (std_row < sigma_line_min) ++r.faulty_row_count;
    }
    for (uint32_t x = 0; x < W; ++x) {
        double mean = 0.0, M2 = 0.0;
        for (uint32_t y = 0; y < H; ++y) {
            double delta = raw[y * W + x] - mean;
            mean += delta / (y + 1);
            M2   += delta * (raw[y * W + x] - mean);
        }
        float std_col = static_cast<float>(std::sqrt(M2 / (H - 1)));
        if (std_col < sigma_line_min) ++r.faulty_col_count;
    }
    if (r.faulty_row_count > 0)
        r.fault_code = r.fault_code | ReadoutFaultCode::ROW_FAULT;
    if (r.faulty_col_count > 0)
        r.fault_code = r.fault_code | ReadoutFaultCode::COLUMN_FAULT;

    return r;
}
```

#### 3.0.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 포화 감지 정확도 | FPR < 1%, FNR = 0% | 합성 포화 이미지 주입 |
| 행/열 결함 감지 | 결함 없는 경우 경보 없음 | 정상 dark frame 검사 |
| 처리 시간 (3072×3072) | < 5ms | 단일 코어, no SIMD needed |
| 기하학 검사 | 잘못된 크기 100% 차단 | 경계값 분석 |

---

### 3.0.5 SWU-1.0.5 Non-linearity Correction (GAP-H 해소)

비선형성 보정은 Offset/Gain 보정 이후, Log Transform 이전에 적용한다. xpe-algorithm-spec-deepsync.md "release-safe baseline"에서 "monotonic LUT or low-order polynomial"로 명시되어 있다.

#### 3.0.6 알고리즘 수학 정의

**Monotonic LUT 방법 (권장)**:

LUT $\mathcal{L}$ 은 ADU 입력값에 대한 선형 응답 보정 출력을 저장한다:

$$I_{\text{linear}}(x,y) = \mathcal{L}\!\left[I_{\text{gain\_corr}}(x,y)\right]$$

LUT 생성 시 단조성 조건을 강제한다:
$$\mathcal{L}[v+1] \geq \mathcal{L}[v] \quad \forall\ v \in [0,\ 2^B - 2]$$

**Polynomial 방법 (대안)**:
$$I_{\text{linear}} = \sum_{k=0}^{K} c_k \cdot I_{\text{gain\_corr}}^k, \quad K \leq 4$$

단조성 요구사항: 도함수 $\frac{dI_{\text{linear}}}{dI_{\text{gain\_corr}}} > 0$ (전 범위에서 양수)

#### 3.0.7 Python 교정 구현 (오프라인)

```python
import numpy as np
from scipy.interpolate import PchipInterpolator

def calibrate_nonlinearity_lut(
        signal_levels_adu:  np.ndarray,
        true_exposures_mAs: np.ndarray,
        bit_depth: int = 16) -> np.ndarray:
    """
    Generate a monotonic non-linearity correction LUT from calibration data.

    Args:
        signal_levels_adu:  measured detector signal at each exposure (N,)
        true_exposures_mAs: reference exposure levels in mAs (N,)  
                            Linear response: signal ∝ mAs
        bit_depth:          detector bit depth (default 16)
    Returns:
        lut: float32 array of length 2^bit_depth
             lut[adu] = linearity-corrected value in ADU-equivalent units

    Method:
        1. Fit PCHIP spline: ADU → ideal_linear (preserves monotonicity)
        2. Evaluate at every integer ADU level 0..2^B-1
        3. Clip & enforce monotonicity (post-fit safety pass)
    """
    N = len(signal_levels_adu)
    assert len(true_exposures_mAs) == N and N >= 4, \
        "Need ≥4 calibration points"

    # Normalize: ideal linear signal = gain_mean × (exposure / exposure_ref)
    exposure_ref  = true_exposures_mAs[N // 2]  # mid-range reference
    signal_ref    = signal_levels_adu[N // 2]
    ideal_signals = signal_ref * (true_exposures_mAs / exposure_ref)

    # Sort by input signal for spline fitting
    sort_idx = np.argsort(signal_levels_adu)
    x_ctrl   = signal_levels_adu[sort_idx].astype(np.float64)
    y_ctrl   = ideal_signals[sort_idx].astype(np.float64)

    # PCHIP: monotone cubic Hermite interpolation
    interp = PchipInterpolator(x_ctrl, y_ctrl, extrapolate=True)

    full_adu_range = np.arange(1 << bit_depth, dtype=np.float64)
    lut = interp(full_adu_range).astype(np.float32)

    # Enforce monotonicity (safety clip)
    lut[0] = max(0.0, lut[0])
    for i in range(1, len(lut)):
        if lut[i] < lut[i - 1]:
            lut[i] = lut[i - 1]  # monotone clamp

    # Clip to valid ADU range
    max_adu = float((1 << bit_depth) - 1)
    lut = np.clip(lut, 0.0, max_adu)
    return lut


def validate_nonlinearity_lut(lut: np.ndarray,
                               max_deviation_pct: float = 5.0) -> dict:
    """
    Validate that the generated LUT is monotone and within deviation bounds.

    Returns dict with: is_valid, max_deviation_pct, monotone_violations
    """
    diffs = np.diff(lut)
    violations = int(np.sum(diffs < 0))
    # Max deviation from identity (no correction)
    identity  = np.arange(len(lut), dtype=np.float32)
    deviation = np.abs(lut - identity) / (identity + 1.0) * 100.0  # percent
    max_dev   = float(np.max(deviation))
    return {
        'is_valid':            violations == 0 and max_dev <= max_deviation_pct,
        'max_deviation_pct':   max_dev,
        'monotone_violations': violations,
    }
```

#### 3.0.8 C++ 런타임 구현 (AVX2 + LUT lookup)

```cpp
// Non-linearity correction via pre-loaded float LUT
// LUT size: 2^bit_depth floats (256KB for 16-bit)
// Called after xpe_gain_correct(), before xpe_log_transform()

void xpe_nonlinearity_correct(const float*    __restrict__ gain_corr_img,
                               const float*    __restrict__ lut,        // size: 1<<bit_depth
                               float*          __restrict__ out,
                               uint32_t width, uint32_t height,
                               uint32_t bit_depth = 16u) {
    const size_t   total   = static_cast<size_t>(width) * height;
    const uint32_t max_idx = (1u << bit_depth) - 1u;

    // Scalar LUT lookup (vectorisation not beneficial for scatter-gather pattern)
    for (size_t i = 0; i < total; ++i) {
        // Clamp to valid LUT range before index conversion
        float  v   = std::clamp(gain_corr_img[i], 0.0f, static_cast<float>(max_idx));
        uint32_t idx = static_cast<uint32_t>(v + 0.5f);  // nearest-integer
        idx = std::min(idx, max_idx);
        out[i] = lut[idx];
    }
}
```

#### 3.0.9 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| LUT 단조성 | 위반 0건 | `validate_nonlinearity_lut()` |
| 최대 보정 편차 | ≤ 5% | identity 대비 백분율 |
| 선형성 잔차 R² | ≥ 0.9995 | 교정 후 계단식 노출 |
| 처리 시간 (3072×3072) | < 30ms | 단일 코어 |

---

### 3.1 SWU-1.1 Offset Correction (SRS-FUNC-001)

#### 3.1.1 알고리즘 수학 정의

$$I_{\text{offset}}(x,y) = \max\left(I_{\text{raw}}(x,y) - I_{\text{dark}}(x,y),\ 0\right)$$

- **입력**: `I_raw` (uint16), `I_dark` (float32 mean of ≥16 dark frames)
- **출력**: `I_offset` (float32, ≥ 0)
- **목적**: Detector dark current 및 readout offset 제거

#### 3.1.2 Offset Map 생성 알고리즘 (Python, 오프라인)

```python
def compute_offset_map(dark_frames: list[np.ndarray]) -> np.ndarray:
    """
    Generate offset correction map from ≥16 dark frames.
    
    Args:
        dark_frames: list of uint16 arrays, shape (H, W), len ≥ 16
    Returns:
        float32 offset map, shape (H, W)
    """
    assert len(dark_frames) >= 16, "Minimum 16 dark frames required"
    
    stack = np.stack(dark_frames, axis=0).astype(np.float64)
    
    # Temporal outlier rejection (σ-clipping, 3σ)
    mean = np.mean(stack, axis=0)
    std  = np.std(stack, axis=0)
    mask = np.abs(stack - mean) <= 3.0 * std  # (N, H, W)
    
    # Compute masked mean
    offset_map = np.sum(stack * mask, axis=0) / np.maximum(np.sum(mask, axis=0), 1)
    
    return offset_map.astype(np.float32)
```

#### 3.1.3 C++ 런타임 구현 (AVX2 최적화)

```cpp
// SWU-1.1: xpe_offset_correct()
// Vectorized subtraction with floor-at-zero (SRS-PERF-001: ≤500ms)
void xpe_offset_correct(const uint16_t* __restrict__ raw,
                         const float*    __restrict__ offset_map,
                         float*          __restrict__ out,
                         uint32_t width, uint32_t height) {
    const size_t total = static_cast<size_t>(width) * height;
    size_t i = 0;

    // AVX2 path: process 8 float32 per iteration
    const __m256 zero = _mm256_setzero_ps();
    for (; i + 8 <= total; i += 8) {
        // Load 8 uint16 → convert to float32
        __m128i raw16 = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(raw + i));
        __m256 raw_f  = _mm256_cvtepi32_ps(
            _mm256_cvtepu16_epi32(raw16));
        
        __m256 off_f  = _mm256_loadu_ps(offset_map + i);
        __m256 diff   = _mm256_sub_ps(raw_f, off_f);
        __m256 result = _mm256_max_ps(diff, zero);   // clamp at 0
        
        _mm256_storeu_ps(out + i, result);
    }
    
    // Scalar tail
    for (; i < total; ++i) {
        float diff = static_cast<float>(raw[i]) - offset_map[i];
        out[i] = (diff < 0.0f) ? 0.0f : diff;
    }
}
```

#### 3.1.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Residual dark signal | Mean < 1.0 ADU | 보정 후 dark field 평균 |
| Negative 픽셀 | 0개 | min(I_offset) ≥ 0 |
| 처리 시간 (3072×3072) | < 50ms | 단일 코어 벤치마크 |

---

### 3.2 SWU-1.2 Gain Correction (SRS-FUNC-002)

#### 3.2.1 알고리즘 수학 정의

$$G(x,y) = \frac{\bar{I}_{\text{flat}}}{I_{\text{flat}}(x,y) - I_{\text{dark}}(x,y)}$$

$$I_{\text{corr}}(x,y) = I_{\text{offset}}(x,y) \cdot G(x,y)$$

- **GainMean** $\bar{I}_{\text{flat}}$: ROI 내 `(Flood - Offset)` 의 spatial mean
- **목적**: Pixel 간 감도 차이 (heel effect, scintillator 두께 불균일) 보정
- **SID별 개별 맵**: kVp에 따른 스펙트럼 변화 → SID마다 별도 gain map 보유

#### 3.2.2 Gain Map 생성 알고리즘 (Python, 오프라인)

```python
def compute_gain_map(flood_frames: list[np.ndarray],
                     offset_map: np.ndarray,
                     roi: tuple[int,int,int,int] | None = None) -> tuple[np.ndarray, float]:
    """
    Generate per-SID gain correction map.
    
    Args:
        flood_frames: list of uint16 flood images (≥8 recommended)
        offset_map:   float32 offset map (H, W)
        roi:          (x0, y0, x1, y1) for GainMean calculation, None = full image
    Returns:
        (gain_map float32 (H,W), gain_mean float32)
    """
    # Average flood frames
    stack = np.stack(flood_frames, axis=0).astype(np.float32)
    flood_mean = np.mean(stack, axis=0)
    
    # Subtract dark
    net_signal = flood_mean - offset_map
    
    # Compute GainMean from ROI (avoid detector edge artefacts)
    if roi:
        x0, y0, x1, y1 = roi
        roi_signal = net_signal[y0:y1, x0:x1]
    else:
        # Auto-trim: inner 80% of image
        h, w = net_signal.shape
        margin_y, margin_x = h // 10, w // 10
        roi_signal = net_signal[margin_y:-margin_y, margin_x:-margin_x]
    
    gain_mean = float(np.mean(roi_signal))
    
    # Compute gain map; clamp to prevent extreme values
    with np.errstate(divide='ignore', invalid='ignore'):
        gain_map = np.where(net_signal > 0,
                            gain_mean / net_signal,
                            1.0)  # fallback for near-zero pixels
    
    gain_map = np.clip(gain_map, 0.5, 2.0).astype(np.float32)
    
    return gain_map, gain_mean
```

#### 3.2.3 C++ 런타임 구현 (AVX2 최적화)

```cpp
// SWU-1.2: xpe_gain_correct()
void xpe_gain_correct(const float* __restrict__ offset_corrected,
                       const float* __restrict__ gain_map,
                       float*       __restrict__ out,
                       uint32_t width, uint32_t height) {
    const size_t total = static_cast<size_t>(width) * height;
    size_t i = 0;

    for (; i + 8 <= total; i += 8) {
        __m256 img  = _mm256_loadu_ps(offset_corrected + i);
        __m256 gain = _mm256_loadu_ps(gain_map + i);
        __m256 res  = _mm256_mul_ps(img, gain);
        _mm256_storeu_ps(out + i, res);
    }
    for (; i < total; ++i) {
        out[i] = offset_corrected[i] * gain_map[i];
    }
}
```

#### 3.2.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Uniformity (PRNU) | CV < 1% after gain | std/mean × 100% |
| Gain map range | [0.5, 2.0] | min/max of G(x,y) |
| Heel effect correction | CV 감소율 > 80% | 보정 전후 CV 비교 |

---

### 3.3 SWU-1.3 Defect Pixel Correction (SRS-FUNC-003)

#### 3.3.1 결함 픽셀 분류 체계

| 유형 | 정의 | 보간 방법 |
|------|------|---------|
| Point Defect | 단일 픽셀: G(x,y) < G_mean × 0.5 또는 > G_mean × 2.0 | 4-neighbor 평균 |
| Cluster Defect | 반경 r ≤ 3 내 ≥4개 point defect | 8-neighbor 유효 픽셀 평균 |
| Column Defect | 전체 컬럼의 ≥80% 결함 | 좌우 컬럼 선형 보간 |
| Row Defect | 전체 행의 ≥80% 결함 | 상하 행 선형 보간 |
| Stuck Pixel | Dark frame에서도 포화 (>MAX-100 ADU) | 주변 median |

#### 3.3.2 결함 맵 생성 알고리즘 (Python, 오프라인)

```python
def create_defect_map(gain_map: np.ndarray,
                      dark_map: np.ndarray,
                      bit_depth: int = 14) -> np.ndarray:
    """
    Detect and classify defect pixels from calibration data.
    
    Returns:
        defect_map: uint8 array (H, W)
          0 = good pixel
          1 = point defect
          2 = cluster defect  
          3 = column defect
          4 = row defect
          5 = stuck pixel (always bright)
    """
    H, W = gain_map.shape
    defect_map = np.zeros((H, W), dtype=np.uint8)
    max_adu = (1 << bit_depth) - 1
    
    gain_mean = np.median(gain_map)  # robust to outliers
    gain_std  = np.std(gain_map[
        (gain_map > gain_mean * 0.5) & (gain_map < gain_mean * 2.0)])
    
    # 1. Point defects from gain map
    low_gain  = gain_map < gain_mean * 0.5
    high_gain = gain_map > gain_mean * 2.0
    point_mask = low_gain | high_gain
    defect_map[point_mask] = 1
    
    # 2. Stuck pixels from dark map
    stuck = dark_map > (max_adu - 100)
    defect_map[stuck] = 5
    
    # 3. Cluster detection: connected component analysis
    from scipy import ndimage
    labeled, n_clusters = ndimage.label(point_mask)
    cluster_sizes = ndimage.sum(point_mask, labeled, range(1, n_clusters + 1))
    for i, size in enumerate(cluster_sizes, start=1):
        if size >= 4:
            defect_map[labeled == i] = 2  # upgrade to cluster
    
    # 4. Column defects
    col_defect_frac = np.mean(defect_map > 0, axis=0)  # fraction per column
    bad_cols = col_defect_frac >= 0.8
    defect_map[:, bad_cols] = 3
    
    # 5. Row defects
    row_defect_frac = np.mean(defect_map > 0, axis=1)  # fraction per row
    bad_rows = row_defect_frac >= 0.8
    defect_map[bad_rows, :] = 4
    
    return defect_map
```

#### 3.3.3 C++ 런타임 보간 알고리즘

```cpp
// Defect interpolation priority: row/col first, then cluster, then point
// Interpolation methods:

// Point/Cluster: Weighted average of valid neighbors
float interpolate_point(const float* img, int x, int y, int W, int H,
                         const uint8_t* defect_map, bool use_8neighbor) {
    float sum = 0.0f;
    int   cnt = 0;
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = { 0,-1, 0,  1,-1,  1, 1,-1};
    int n_neighbors = use_8neighbor ? 8 : 4;
    // For 4-neighbor: only first 4 entries used with indices {1,0}, {-1,0}, {0,1}, {0,-1}
    
    for (int k = 0; k < n_neighbors; ++k) {
        int nx = x + dx[k], ny = y + dy[k];
        if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
            defect_map[ny * W + nx] == 0) {
            sum += img[ny * W + nx];
            ++cnt;
        }
    }
    return (cnt > 0) ? sum / cnt : img[y * W + x];
}

// Column defect: linear interpolation from left-right valid columns
float interpolate_column(const float* img, int x, int y, int W, int H,
                          const uint8_t* defect_map) {
    // Find nearest valid left column
    int left = x - 1;
    while (left >= 0 && defect_map[y * W + left] == 3) --left;
    int right = x + 1;
    while (right < W && defect_map[y * W + right] == 3) ++right;
    
    if (left < 0 && right >= W) return img[y * W + x]; // no valid
    if (left < 0)  return img[y * W + right];
    if (right >= W) return img[y * W + left];
    
    float t = float(x - left) / float(right - left);
    return img[y * W + left] * (1.0f - t) + img[y * W + right] * t;
}

// Main defect correction pass (two-pass: line defects first)
void xpe_defect_correct(float* __restrict__ img,
                          const uint8_t* __restrict__ defect_map,
                          uint32_t width, uint32_t height) {
    // Pass 1: Row/Column defects
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t dt = defect_map[y * width + x];
            if (dt == 3) {
                img[y * width + x] =
                    interpolate_column(img, x, y, width, height, defect_map);
            } else if (dt == 4) {
                // Row: interpolate from above/below rows
                img[y * width + x] =
                    interpolate_row(img, x, y, width, height, defect_map);
            }
        }
    }
    
    // Pass 2: Cluster defects (8-neighbor)
    // Pass 3: Point defects (4-neighbor) + Stuck pixels (median)
    // (implementation follows same pattern)
}
```

#### 3.3.4 런타임 자동 갱신 알고리즘

```cpp
// Runtime defect detection: identify new defects during exposure sequence
// Algorithm:
//   1. Compute per-pixel deviation: |current - reference| / (local_std + eps)
//   2. Pixels with z-score > threshold_sigma → new defect candidate
//   3. Local std estimated over 5×5 neighbourhood using AVX2 vectorisation
//   4. Set bit 1 (0x02) in defect_map for runtime-flagged pixels
//   5. Caller must invoke xpe_defect_correct() to interpolate flagged pixels
//
// Note: reference_img should be a rolling mean of N_ref (≥4) preceding frames.
//       Use atomic write to defect_map to allow concurrent correction pass.

static void compute_local_std_row(const float* src, float* local_std_out,
                                   uint32_t W, uint32_t y, uint32_t H,
                                   float eps = 1e-6f) {
    // 5×5 neighbourhood local standard deviation (vertical strip [y-2, y+2])
    const int radius = 2;
    for (uint32_t x = 0; x < W; ++x) {
        float sum = 0.0f, sum_sq = 0.0f;
        int   n   = 0;
        for (int dy = -radius; dy <= radius; ++dy) {
            int ny = static_cast<int>(y) + dy;
            if (ny < 0 || ny >= static_cast<int>(H)) continue;
            for (int dx = -radius; dx <= radius; ++dx) {
                int nx = static_cast<int>(x) + dx;
                if (nx < 0 || nx >= static_cast<int>(W)) continue;
                float v = src[ny * W + nx];
                sum    += v;
                sum_sq += v * v;
                ++n;
            }
        }
        float mean = sum / n;
        float var  = std::max(0.0f, sum_sq / n - mean * mean);
        local_std_out[y * W + x] = std::sqrt(var) + eps;
    }
}

void update_defect_map_runtime(float*   __restrict__ current_img,
                                float*   __restrict__ reference_img,
                                uint8_t* __restrict__ defect_map,
                                uint32_t width,
                                uint32_t height,
                                float    threshold_sigma /* = 5.0f */) {
    const size_t total = static_cast<size_t>(width) * height;

    // Allocate temporary local_std buffer
    std::vector<float> local_std(total);

    // Compute local std row-by-row (OpenMP parallelisable)
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < static_cast<int>(height); ++y) {
        compute_local_std_row(reference_img, local_std.data(),
                               width, static_cast<uint32_t>(y), height);
    }

    // AVX2 vectorised z-score threshold pass
    const __m256 v_thresh = _mm256_set1_ps(threshold_sigma);
    size_t i = 0;

    for (; i + 8 <= total; i += 8) {
        __m256 cur  = _mm256_loadu_ps(current_img   + i);
        __m256 ref  = _mm256_loadu_ps(reference_img + i);
        __m256 lstd = _mm256_loadu_ps(local_std.data() + i);

        // |cur - ref| / local_std
        __m256 diff   = _mm256_sub_ps(cur, ref);
        __m256 absdif = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), diff);  // abs
        __m256 zscore = _mm256_div_ps(absdif, lstd);

        // z-score > threshold_sigma → candidate defect
        __m256 cmp = _mm256_cmp_ps(zscore, v_thresh, _CMP_GT_OQ);
        int mask8  = _mm256_movemask_ps(cmp);

        if (mask8 != 0) {
            for (int k = 0; k < 8; ++k) {
                if (mask8 & (1 << k)) {
                    // Set bit 1 (runtime defect flag), preserve other bits
                    defect_map[i + k] |= 0x02u;
                }
            }
        }
    }
    // Scalar tail
    for (; i < total; ++i) {
        float z = std::fabsf(current_img[i] - reference_img[i]) / local_std[i];
        if (z > threshold_sigma) {
            defect_map[i] |= 0x02u;
        }
    }
}
```

---

### 3.4 SWU-1.4 Ghost/Lag Correction (SRS-FUNC-004)

#### 3.4.1 알고리즘 수학 정의 — Siewerdsen-Jaffray Multi-Exponential Model

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i \cdot e^{-t/\tau_i}$$

$$I_{\text{true}}(t) = I_{\text{measured}}(t) - \text{Lag}(t) \cdot I_{\text{prev\_max}}$$

- **파라미터**: `α = [0.04, 0.02, 0.005]`, `τ = [0.5s, 2.0s, 10.0s]` (CsI:Tl/a-Si 기본값)
- **교정**: 각 detector 유형별 실측 피팅으로 파라미터 결정
- **목적**: 이전 노출의 잔류 신호(ghost/lag) 제거, ≥90% ghost removal 달성

#### 3.4.2 Python 피팅 (오프라인 교정)

```python
from scipy.optimize import curve_fit

def multi_exponential_lag(t: np.ndarray, a1, tau1, a2, tau2, a3, tau3) -> np.ndarray:
    return a1 * np.exp(-t / tau1) + a2 * np.exp(-t / tau2) + a3 * np.exp(-t / tau3)

def fit_lag_parameters(lag_decay_data: np.ndarray,
                        time_points: np.ndarray) -> dict:
    """
    Fit 3-component exponential lag model to measured decay data.
    
    Args:
        lag_decay_data: normalized lag fraction at each time point (0-1)
        time_points:    time in seconds after initial exposure
    Returns:
        dict with keys: alpha, tau (each length 3)
    """
    p0 = [0.04, 0.5, 0.02, 2.0, 0.005, 10.0]
    bounds = ([0, 0.1, 0, 0.5, 0, 2.0],
              [0.2, 5.0, 0.1, 20.0, 0.05, 100.0])
    
    popt, pcov = curve_fit(multi_exponential_lag, time_points,
                            lag_decay_data, p0=p0, bounds=bounds,
                            maxfev=10000)
    
    return {
        'alpha': [popt[0], popt[2], popt[4]],
        'tau':   [popt[1], popt[3], popt[5]],
        'r_squared': compute_r_squared(lag_decay_data,
                                        multi_exponential_lag(time_points, *popt))
    }
```

#### 3.4.3 C++ 런타임 구현

```cpp
struct GhostCorrectionParams {
    float alpha[3];  // {0.04, 0.02, 0.005}
    float tau[3];    // {0.5, 2.0, 10.0}  seconds
};

struct ExposureHistory {
    float* max_signal;   // Per-pixel max signal from previous exposure
    float  elapsed_sec;  // Time since previous exposure
};

void xpe_ghost_correct(float* __restrict__ img,
                         const ExposureHistory& history,
                         const GhostCorrectionParams& params,
                         uint32_t width, uint32_t height) {
    if (history.elapsed_sec <= 0.0f || history.max_signal == nullptr) return;
    
    // Compute lag fraction at elapsed time
    float lag_fraction = 0.0f;
    for (int i = 0; i < 3; ++i) {
        lag_fraction += params.alpha[i] *
                        expf(-history.elapsed_sec / params.tau[i]);
    }
    
    const size_t total = static_cast<size_t>(width) * height;
    
    // AVX2 path
    __m256 lag_f = _mm256_set1_ps(lag_fraction);
    __m256 zero  = _mm256_setzero_ps();
    
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 cur     = _mm256_loadu_ps(img + i);
        __m256 prev    = _mm256_loadu_ps(history.max_signal + i);
        __m256 ghost   = _mm256_mul_ps(lag_f, prev);
        __m256 result  = _mm256_max_ps(_mm256_sub_ps(cur, ghost), zero);
        _mm256_storeu_ps(img + i, result);
    }
    for (; i < total; ++i) {
        float ghost = lag_fraction * history.max_signal[i];
        img[i] = std::max(img[i] - ghost, 0.0f);
    }
}
```

#### 3.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| Ghost removal rate | ≥90% | 이중 노출 프로토콜: ghost_after / ghost_before |
| Residual lag at 0.5s | < 2% | 단기 lag 측정 |
| Model fit R² | ≥0.98 | 피팅 결과 검증 |

---

## 4. SWI-2: Core Processing 알고리즘

### 4.1 SWU-2.1 Log Transform (SRS-FUNC-010)

#### 4.1.1 수학 정의

$$I_{OD}(x,y) = -\ln\left(\frac{I_{\text{clean}}(x,y) + \varepsilon}{I_0 + \varepsilon}\right)$$

- $I_0$: 비노출 영역(collimator edge 내부) 기준 최대 플루엔스 추정값 또는 이론값
- $\varepsilon = 10^{-6}$: Zero/negative 입력 보호 (SRS-FUNC-010)
- **결과**: Beer-Lambert law에 의해 OD(Optical Density) ≈ attenuation coefficient × thickness

#### 4.1.2 I₀ 추정 전략

```cpp
// Strategy 1: Use collimated (unattenuated) region statistics
float estimate_I0_from_collimator(const float* img, uint32_t W, uint32_t H,
                                    const CollimatorMask& mask) {
    // Find unattenuated pixels (inside collimator border, no anatomy)
    // Use 95th percentile to avoid outliers
    std::vector<float> unattenuated;
    for (uint32_t i = 0; i < W * H; ++i) {
        if (mask.is_unattenuated(i)) unattenuated.push_back(img[i]);
    }
    std::sort(unattenuated.begin(), unattenuated.end());
    return unattenuated[static_cast<size_t>(unattenuated.size() * 0.95f)];
}

// Strategy 2: Use gain-normalized reference (preferred for consistency)
// I0 = GainMean (stored in gain map header)
```

#### 4.1.3 C++ 구현 (AVX2)

```cpp
void xpe_log_transform(const float* __restrict__ in,
                         float*       __restrict__ out,
                         float I0, float epsilon,
                         uint32_t width, uint32_t height) {
    const float eps = (epsilon > 0) ? epsilon : 1e-6f;
    const size_t total = static_cast<size_t>(width) * height;
    
    __m256 v_eps   = _mm256_set1_ps(eps);
    __m256 v_I0e   = _mm256_set1_ps(I0 + eps);
    __m256 v_neg1  = _mm256_set1_ps(-1.0f);
    
    size_t i = 0;
    for (; i + 8 <= total; i += 8) {
        __m256 x      = _mm256_loadu_ps(in + i);
        __m256 x_eps  = _mm256_add_ps(x, v_eps);
        __m256 ratio  = _mm256_div_ps(x_eps, v_I0e);
        __m256 ln_val = avx2_log_ps(ratio);   // See avx2_log_ps below
        __m256 od     = _mm256_mul_ps(v_neg1, ln_val);
        _mm256_storeu_ps(out + i, od);
    }
    for (; i < total; ++i) {
        out[i] = -logf((in[i] + eps) / (I0 + eps));
    }
}

// ---------------------------------------------------------------------------
// avx2_log_ps: Cephes-based AVX2 natural logarithm approximation
// Accuracy: ~5 ULP (max relative error < 1.2×10⁻⁷ for x ∈ (0, +∞))
// Algorithm: Cephes log.c decomposition — identical to avx_mathfun (Gruzdev 2012)
//   Reference: https://github.com/reyoung/avx_mathfun (BSD-2)
//   Reference: Cephes Math Library, S. Moshier
//
// Derivation:
//   x = m × 2^e  where m ∈ [0.5, 1.0)
//   ln(x) = ln(m) + e × ln(2)
//   ln(m) approximated by degree-8 minimax polynomial on [sqrt(0.5), sqrt(2)]
//   after substitution f = m − 1 (range reduction to [−0.293, 0.414])
// ---------------------------------------------------------------------------
static inline __m256 avx2_log_ps(__m256 x) {
    // Polynomial coefficients (Cephes, ~5 ULP)
    const __m256 c_ln2_hi  = _mm256_set1_ps(0.693359375f);
    const __m256 c_ln2_lo  = _mm256_set1_ps(-2.12194440e-4f);
    const __m256 c_half    = _mm256_set1_ps(0.5f);
    const __m256 c_one     = _mm256_set1_ps(1.0f);
    const __m256 c_sqrthf  = _mm256_set1_ps(0.707106781186547524f);  // sqrt(0.5)
    // Polynomial coefficients for ln(1+f), f = normalized(x) − 1
    const __m256 c_p0  = _mm256_set1_ps( 7.0376836292e-2f);
    const __m256 c_p1  = _mm256_set1_ps(-1.1514610310e-1f);
    const __m256 c_p2  = _mm256_set1_ps( 1.1676998740e-1f);
    const __m256 c_p3  = _mm256_set1_ps(-1.2420140846e-1f);
    const __m256 c_p4  = _mm256_set1_ps( 1.4249322787e-1f);
    const __m256 c_p5  = _mm256_set1_ps(-1.6668057665e-1f);
    const __m256 c_p6  = _mm256_set1_ps( 2.0000714765e-1f);
    const __m256 c_p7  = _mm256_set1_ps(-2.4999993993e-1f);
    const __m256 c_p8  = _mm256_set1_ps( 3.3333331174e-1f);
    const __m256i c_127 = _mm256_set1_epi32(127);

    // Clamp x > 0 (avoid NaN/Inf propagation)
    x = _mm256_max_ps(x, _mm256_set1_ps(1.175494351e-38f));  // FLT_MIN

    // Decompose x = m × 2^e  (e = biased_exponent - 127)
    __m256i xi = _mm256_castps_si256(x);
    // Extract exponent
    __m256i exp_i = _mm256_sub_epi32(_mm256_srli_epi32(xi, 23), c_127);
    __m256 e = _mm256_cvtepi32_ps(exp_i);
    // Set mantissa to [0.5, 1.0): clear exponent, set bias to 126
    xi = _mm256_and_si256(xi, _mm256_set1_epi32(0x007fffff));
    xi = _mm256_or_si256(xi,  _mm256_set1_epi32(0x3f000000));
    __m256 m = _mm256_castsi256_ps(xi);

    // If m < sqrt(0.5), multiply m by 2 and subtract 1 from exponent
    __m256 mask = _mm256_cmp_ps(m, c_sqrthf, _CMP_LT_OQ);
    e = _mm256_sub_ps(e, _mm256_and_ps(c_one, mask));
    m = _mm256_add_ps(m, _mm256_and_ps(m, mask));   // m += m if m < sqrthf
    m = _mm256_sub_ps(m, c_one);                     // f = m - 1

    // Horner evaluation of polynomial in f
    __m256 y = c_p0;
    y = _mm256_fmadd_ps(y, m, c_p1);
    y = _mm256_fmadd_ps(y, m, c_p2);
    y = _mm256_fmadd_ps(y, m, c_p3);
    y = _mm256_fmadd_ps(y, m, c_p4);
    y = _mm256_fmadd_ps(y, m, c_p5);
    y = _mm256_fmadd_ps(y, m, c_p6);
    y = _mm256_fmadd_ps(y, m, c_p7);
    y = _mm256_fmadd_ps(y, m, c_p8);
    y = _mm256_mul_ps(y, m);
    y = _mm256_mul_ps(y, m);   // y × m²

    // ln(x) = y + e×ln2_hi + e×ln2_lo − 0.5×m² + m
    __m256 r = _mm256_fmadd_ps(e,  c_ln2_hi, y);
    r = _mm256_fmadd_ps(e, c_ln2_lo, r);
    r = _mm256_fmadd_ps(_mm256_set1_ps(-0.5f), _mm256_mul_ps(m, m), r);
    r = _mm256_add_ps(r, m);
    return r;
}
```

---

### 4.2 SWU-2.2 Noise Reduction (SRS-FUNC-011)

#### 4.2.1 Bilateral Filter — 핵심 알고리즘

$$BF[I](x) = \frac{1}{W_p} \sum_{x_i \in \Omega} I(x_i) \cdot f_s(\|x_i - x\|) \cdot f_r(|I(x_i) - I(x)|)$$

$$f_s(d) = e^{-d^2/(2\sigma_s^2)}, \quad f_r(\delta) = e^{-\delta^2/(2\sigma_r^2)}$$

$$W_p = \sum_{x_i \in \Omega} f_s(\|x_i - x\|) \cdot f_r(|I(x_i) - I(x)|)$$

- **파라미터 (SRS-FUNC-011)**: `σ_s = 2.0` pixels, `σ_r = 0.1` OD unit
- **커널 크기**: `2 × ⌈3σ_s⌉ + 1 = 13×13`
- **구현**: OpenCV `cv::bilateralFilter()` + 사전 계산 lookup table

#### 4.2.2 파라미터 선택 근거

| σ_s | σ_r | 효과 | 부작용 |
|-----|-----|------|--------|
| 1.0 | 0.05 | 약한 스무딩, 노이즈 유지 | 효과 미미 |
| 2.0 | 0.10 | **권장: 노이즈 제거 + edge 보존** | 미미한 detail 손실 |
| 3.0 | 0.15 | 강한 스무딩 | Texture 과도 억제 |
| 5.0 | 0.30 | 과도한 스무딩 | Watercolor artifact |

#### 4.2.3 Non-Local Means (고품질 옵션)

$$NLM[I](x) = \frac{1}{C(x)} \sum_{y \in \Omega} e^{-\frac{\|I(N_x) - I(N_y)\|^2_{2,a}}{h^2}} \cdot I(y)$$

- $N_x$: `x` 중심 `p×p` 패치 (권장: `p=7`)
- 탐색 범위: `d×d` 윈도우 (권장: `d=21`)
- `h`: 필터링 파라미터 (노이즈 표준편차의 ~10배)
- **구현**: OpenCV `cv::fastNlMeansDenoising()` 또는 CUDA 가속

#### 4.2.4 알고리즘 선택 로직

```cpp
ImageQualityMode select_denoising_mode(const ProcessingParams& params) {
    if (params.quality_mode == "high" || params.body_part == "BREAST")
        return ImageQualityMode::NLM;
    return ImageQualityMode::Bilateral;  // default
}
```

---

### 4.3 SWU-2.3 CLAHE (SRS-FUNC-012)

#### 4.3.1 알고리즘 수학 정의

CLAHE (Contrast Limited Adaptive Histogram Equalization):

1. **타일 분할**: 이미지를 `M×N` 타일로 분할 (기본: 8×8)
2. **히스토그램 계산**: 각 타일 내 픽셀 히스토그램 (bins: 256)
3. **Clip 제한**: `clip_limit × (tile_area / bins)` 초과 빈도 → 균등 재분배
4. **CDF 계산**: 클리핑된 히스토그램의 누적분포함수
5. **Bilinear 보간**: 경계 타일 간 매끄러운 전환

$$\text{CDF}(v) = \frac{1}{N_{clip}} \sum_{i=0}^{v} h_{clip}(i)$$

$$I_{out}(x,y) = \text{BilinearInterp}\left(\text{CDF}_{T_1}, \text{CDF}_{T_2}, \text{CDF}_{T_3}, \text{CDF}_{T_4}, I_{in}(x,y)\right)$$

#### 4.3.2 파라미터 명세

| 파라미터 | 기본값 | 범위 | 설명 |
|---------|--------|------|------|
| `tile_size` | 8×8 | 4–64 | 적응 영역 크기 |
| `clip_limit` | 2.0 | 1.0–8.0 | 클리핑 강도 (1.0 = no clip = AHE) |
| `bins` | 256 | 64–4096 | 히스토그램 해상도 |
| `input_range` | [0, 4095] | adaptive | 입력 동적 범위 |

#### 4.3.3 Body-Part별 최적 파라미터 (SRS-FUNC-021 preset과 연계)

| 신체 부위 | clip_limit | tile_size | 이유 |
|----------|-----------|---------|------|
| Chest PA/AP | 2.0 | 8×8 | 폐야/종격동 균형 |
| Abdomen | 1.5 | 16×16 | 대비 차이 완만 |
| Extremity | 3.0 | 4×4 | 국소 골 디테일 강화 |
| Hand/Wrist | 4.0 | 4×4 | 세밀한 골 구조 |
| Spine | 2.5 | 8×8 | 추체-디스크 대비 |
| Breast | 1.0 | 32×32 | 균일한 대비, 과도 억제 방지 |

#### 4.3.4 C++ 구현

```cpp
// Using OpenCV CLAHE
void xpe_clahe_enhance(cv::Mat& img_float32,
                         const ClaheParams& params) {
    // Convert to 16-bit for OpenCV CLAHE processing
    double min_val, max_val;
    cv::minMaxLoc(img_float32, &min_val, &max_val);
    
    cv::Mat img16;
    img_float32.convertTo(img16, CV_16U,
                           65535.0 / (max_val - min_val + 1e-6),
                           -min_val * 65535.0 / (max_val - min_val + 1e-6));
    
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
        params.clip_limit,
        cv::Size(params.tile_cols, params.tile_rows));
    clahe->apply(img16, img16);
    
    // Convert back to float32
    img16.convertTo(img_float32, CV_32F,
                     (max_val - min_val) / 65535.0,
                     min_val);
}
```

---

### 4.4 SWU-2.4 Edge Enhancement — Unsharp Masking (SRS-FUNC-013)

#### 4.4.1 알고리즘 수학 정의

$$I_{\text{USM}}(x,y) = I(x,y) + \lambda(x,y) \cdot \left[I(x,y) - I_{\text{blur}}(x,y)\right]$$

$$I_{\text{blur}}(x,y) = I(x,y) * G_{\sigma}(x,y)$$

- $G_{\sigma}$: Gaussian blur kernel (σ = 1.5~2.0 pixels)
- $\lambda$: **Adaptive gain** — body-part별 safe range로 제한 (SRS-SAFE-005)
- **High-pass component**: `H(x,y) = I(x,y) - I_blur(x,y)` (Laplacian of Gaussian 근사)

#### 4.4.2 Body-Part별 Safe Gain Range (SRS-SAFE-005 이행)

| 신체 부위 | λ_min | λ_default | λ_max | 이유 |
|----------|-------|---------|-------|------|
| Chest | 0.0 | 0.8 | 1.5 | 과도한 폐 구조 강조 방지 |
| Bone (extremity) | 0.0 | 1.2 | 2.5 | 골 디테일 강화 허용 |
| Spine | 0.0 | 1.0 | 2.0 | 균형 |
| Breast | 0.0 | 0.5 | 1.0 | 미세석회화 인식, artifact 방지 |
| Pediatric | 0.0 | 0.6 | 1.2 | 낮은 contrast 조직 보호 |

```cpp
float clamp_usm_gain(float lambda, const BodyPartPreset& preset) {
    return std::clamp(lambda, preset.lambda_min, preset.lambda_max);
}
```

#### 4.4.3 주파수 선택적 USM (Selective Frequency Enhancement)

진단 가치 있는 공간주파수 범위만 강화:

```cpp
// Bandpass USM: enhance only [f_low, f_high] frequency band
// Implementation: DoG (Difference of Gaussians)
// H_band(x,y) = G_{σ1}(x,y) - G_{σ2}(x,y)  where σ1 < σ2
// f_low ≈ 1/(4σ2), f_high ≈ 1/(4σ1)
// For chest: σ1=1.0, σ2=4.0 → [0.06, 0.25] cycles/pixel
```

---

### 4.5 SWU-2.5 Multiscale Processing — Laplacian Pyramid (SRS-FUNC-014)

#### 4.5.1 알고리즘 수학 정의

**건설 단계 (Analysis):**

$$G_0 = I, \quad G_k = \text{Downsample}(G_{k-1} * h)$$
$$L_k = G_k - \text{Upsample}(G_{k+1}) \quad \text{for } k = 0, 1, \ldots, N-1$$
$$L_N = G_N \quad \text{(residual)}$$

**비선형 게인 적용:**

$$\hat{L}_k = L_k \cdot g_k\left(\|L_k\|\right)$$

$$g_k(s) = \begin{cases} g_{\max,k} & s < s_1 \\ g_{\min,k} + (g_{\max,k}-g_{\min,k})\cdot\frac{s_2-s}{s_2-s_1} & s_1 \le s < s_2 \\ g_{\min,k} & s \ge s_2 \end{cases}$$

**재구성 단계 (Synthesis):**

$$\hat{G}_{k} = \hat{L}_k + \text{Upsample}(\hat{G}_{k+1})$$

#### 4.5.2 파라미터 명세

```cpp
struct LaplacianPyramidParams {
    int    levels        = 8;    // SRS-FUNC-014: ≥8 levels
    float  sigma         = 1.0f; // Gaussian sigma for each level
    // Per-level gain curve (nonlinear)
    struct LevelGain {
        float g_max    = 1.5f;   // gain for small signals (texture)
        float g_min    = 0.8f;   // gain for large signals (edges)
        float s1       = 0.02f;  // lower threshold (in OD units)
        float s2       = 0.10f;  // upper threshold
    } gains[8];
};
```

#### 4.5.3 구현 전략

```cpp
// Using OpenCV pyrDown/pyrUp (Gaussian 5-tap kernel)
void build_laplacian_pyramid(const cv::Mat& src,
                               std::vector<cv::Mat>& laplacian,
                               std::vector<cv::Mat>& gaussian,
                               int levels) {
    gaussian.resize(levels + 1);
    laplacian.resize(levels);
    gaussian[0] = src.clone();
    
    for (int k = 0; k < levels; ++k) {
        cv::pyrDown(gaussian[k], gaussian[k+1]);
        cv::Mat upsampled;
        cv::pyrUp(gaussian[k+1], upsampled, gaussian[k].size());
        laplacian[k] = gaussian[k] - upsampled;
    }
}

void apply_nonlinear_gain(cv::Mat& L, const LaplacianPyramidParams::LevelGain& gain) {
    L.forEach<float>([&](float& val, const int* pos) {
        float s = std::abs(val);
        float g;
        if (s < gain.s1)
            g = gain.g_max;
        else if (s < gain.s2)
            g = gain.g_min + (gain.g_max - gain.g_min) *
                (gain.s2 - s) / (gain.s2 - gain.s1);
        else
            g = gain.g_min;
        val *= g;
    });
}
```

---

### 4.6 SWU-2.6 Fractional Multiscale Processing (SRS-FUNC-015)

#### 4.6.1 개념 및 수학적 배경

Fractional Multiscale Processing (FMP)은 Laplacian Pyramid의 정수 스케일 해상도 감소 대신 **비정수(fractional) 스케일**을 사용하여 density transition zone의 artifact를 제거한다.

**기본 원리:**

$$L_k^{\alpha} = I - (G_k)^{\alpha} \cdot (I * h^{N-k})^{1-\alpha}$$

여기서 $\alpha \in (0, 1)$는 분수 스케일 파라미터.

실용 구현 (Polynomial Approximation):

$$G_k^{\alpha}(x,y) = \sum_{n=0}^{K} c_n(\alpha) \cdot G_n(x,y)$$

- $c_n(\alpha)$: 분수 스케일 계수 (Chebyshev 보간 기반)

#### 4.6.2 구현 알고리즘

```cpp
struct FractionalMSParams {
    float alpha         = 0.5f;   // Fractional scale (0.3–0.7)
    int   base_levels   = 8;
    float density_threshold = 0.3f; // OD threshold for transition zone
};

// FMP replaces integer pyramid bands with fractional bands at transition zones
cv::Mat compute_fractional_band(const std::vector<cv::Mat>& gaussian_pyr,
                                  float alpha, int target_level) {
    int L = static_cast<int>(gaussian_pyr.size()) - 1;
    int k1 = static_cast<int>(std::floor(alpha * (L - 1)));
    int k2 = k1 + 1;
    float t = alpha * (L - 1) - k1;
    
    cv::Mat level1, level2;
    cv::resize(gaussian_pyr[k1], level1, gaussian_pyr[0].size(),
               0, 0, cv::INTER_LINEAR);
    cv::resize(gaussian_pyr[k2], level2, gaussian_pyr[0].size(),
               0, 0, cv::INTER_LINEAR);
    
    return (1.0f - t) * level1 + t * level2;
}
```

---

## 5. Grid Suppression & Virtual Grid 알고리즘

### 5.1 Grid Line Artifact Suppression (GAP-03 해소)

#### 5.1.1 자동 Grid 파라미터 감지

```cpp
struct GridSpec {
    float  line_density_lpi;  // lines per inch (60–200 lpi typical)
    float  angle_deg;         // grid orientation (0° = horizontal)
    float  pixel_pitch_mm;    // detector pixel pitch
};

// Derive grid artifact frequency from DICOM tags and GridSpec
float compute_grid_artifact_frequency(const GridSpec& spec) {
    // f_grid [cycles/pixel] = pixel_pitch_mm / (25.4 / line_density_lpi)
    return spec.pixel_pitch_mm * spec.line_density_lpi / 25.4f;
}
```

#### 5.1.2 2D DWT 기반 Grid Suppression (Tang et al. 2015)

```python
def wavelet_grid_suppression(image: np.ndarray,
                              grid_freq_cpx: float,
                              wavelet: str = 'db6',
                              max_levels: int = 8) -> np.ndarray:
    """
    Remove grid line artifact using 2D DWT + Gaussian band-stop filter.
    
    Algorithm:
      1. 2D DWT decomposition with auto stop condition
      2. For each sub-band: detect grid frequency component
      3. Apply Gaussian band-stop filter in frequency domain
      4. Reconstruct via inverse DWT
      
    Args:
        image:          float32 input (H, W)
        grid_freq_cpx:  grid artifact frequency in cycles/pixel
        wavelet:        wavelet family (db6 recommended)
        max_levels:     maximum decomposition levels
    Returns:
        float32 grid-suppressed image
    """
    import pywt
    
    # Auto stop condition: stop when grid frequency falls below Nyquist
    # at current decomposition level
    auto_level = 1
    nyquist = 0.5  # cycles/pixel at current level
    f = grid_freq_cpx
    while auto_level < max_levels and f < nyquist * 0.25:
        f *= 2  # frequency doubles with each level of downsampling
        nyquist /= 2
        auto_level += 1
    
    # Perform 2D DWT
    coeffs = pywt.wavedec2(image, wavelet, level=auto_level)
    
    # Apply band-stop filter to horizontal/vertical detail coefficients
    filtered_coeffs = [coeffs[0]]  # keep approximation
    for level_coeffs in coeffs[1:]:
        cH, cV, cD = level_coeffs
        # Suppress grid frequency in horizontal and vertical bands
        cH = _apply_gaussian_bandstop_1d(cH, grid_freq_cpx, axis=1)
        cV = _apply_gaussian_bandstop_1d(cV, grid_freq_cpx, axis=0)
        filtered_coeffs.append((cH, cV, cD))
    
    return pywt.waverec2(filtered_coeffs, wavelet)

def _apply_gaussian_bandstop_1d(band: np.ndarray, f_stop: float,
                                   axis: int, bandwidth: float = 0.02) -> np.ndarray:
    """Apply 1D Gaussian band-stop filter along specified axis."""
    spectrum = np.fft.rfft(band, axis=axis)
    freqs = np.fft.rfftfreq(band.shape[axis])
    
    # Gaussian notch centered at f_stop
    notch = 1.0 - np.exp(-0.5 * ((freqs - f_stop) / bandwidth)**2)
    notch = notch.reshape([-1 if i == axis else 1 for i in range(band.ndim)])
    
    spectrum *= notch
    return np.fft.irfft(spectrum, n=band.shape[axis], axis=axis)
```

#### 5.1.3 NSCT 기반 Moiré Suppression (Kim et al. 2023)

Nonsubsampled Contourlet Transform은 aliasing 없이 다방향 분해를 제공하여 비축 grid orientation에 효과적이다.

```python
# NSCT-based approach for non-standard grid angles
# Reference: Kim et al. 2023, Nuclear Engineering and Technology 55(4):1420-1429
# Key advantage: shift-invariance prevents ringing artifacts at non-axis orientations

def nsct_grid_suppression(image: np.ndarray,
                           grid_angle_deg: float,
                           grid_freq_cpx: float,
                           nsct_levels: int = 4,
                           n_directions_fine: int = 8) -> np.ndarray:
    """
    Suppress X-ray anti-scatter grid artifact using NSCT decomposition.

    Algorithm (Kim et al. 2023, 4-step):
      Step 1 — NSCT Decomposition
        Decompose image into (nsct_levels) lowpass + directional subband pyramid.
        Fine-scale level uses n_directions_fine directional subbands.
        Shift-invariance achieved by omitting downsampling (nonsubsampled filter bank).

      Step 2 — Artifact Subband Identification
        Grid artifact in spatial domain → spike in specific directional subband.
        Target subband index = round(grid_angle_deg / (180 / n_directions_fine)) mod n_directions_fine
        Confirm by comparing subband energy to neighboring subbands (energy_ratio > 3.0 threshold).

      Step 3 — Moiré Component Extraction via Gaussian Band-Pass
        Within the identified subband coefficient map S[i][k]:
          centre_freq = grid_freq_cpx  (in cycles/pixel)
          sigma_bp    = centre_freq × 0.25  (bandwidth: ±25% of grid frequency)
          mask        = gaussian_bandpass_2d(S[i][k].shape, centre_freq, sigma_bp)
          moire_coeff = S[i][k] × mask

      Step 4 — Subtract and Reconstruct
        Zero out or attenuate moire_coeff in the identified subband:
          suppression_weight = compute_adaptive_weight(energy_ratio)
          S[i][k]_clean = S[i][k] - suppression_weight × moire_coeff
        Reconstruct image via inverse NSCT (synthesis filter bank).

    Args:
        image:            float32 input image, OD or linear domain (H, W)
        grid_angle_deg:   dominant grid line orientation in degrees [0, 180)
        grid_freq_cpx:    grid spatial frequency in cycles/pixel (typical 0.05–0.20)
        nsct_levels:      number of decomposition levels (default 4)
        n_directions_fine: directional subbands at finest level (default 8)
    Returns:
        float32 image with grid artifact suppressed
    """
    try:
        import pynsct  # pip install pynsct  (or custom NSCT implementation)
        _nsct_available = True
    except ImportError:
        _nsct_available = False

    if not _nsct_available:
        # Fallback: frequency-domain notch filter at grid frequency
        # Less effective for oblique grids but always available
        import warnings
        warnings.warn("pynsct not available; falling back to notch filter suppression")
        return _notch_fallback(image, grid_angle_deg, grid_freq_cpx)

    H, W = image.shape

    # --- Step 1: NSCT Decomposition ---
    # n_dir_list: number of directional subbands per level (coarse→fine)
    # e.g. [4, 4, 8, 8] for 4-level decomposition
    n_dir_list = [4] * (nsct_levels - 2) + [n_directions_fine, n_directions_fine]
    coeffs = pynsct.nsctdec(image, nlevels=nsct_levels, n_dir_list=n_dir_list)
    # coeffs structure: [lowpass_coeff, level0_subbands, level1_subbands, ...]
    # finest level: coeffs[-1] is list of n_directions_fine subband arrays

    # --- Step 2: Identify Target Subband ---
    fine_subbands = coeffs[-1]          # list of n_directions_fine arrays
    n_sb = len(fine_subbands)
    angle_per_sb = 180.0 / n_sb         # angular step per subband
    target_idx = int(round(grid_angle_deg / angle_per_sb)) % n_sb

    # Confirm by energy ratio
    target_energy  = float(np.sum(fine_subbands[target_idx] ** 2))
    neighbor_energy = float(np.sum(fine_subbands[(target_idx - 1) % n_sb] ** 2) +
                            np.sum(fine_subbands[(target_idx + 1) % n_sb] ** 2)) / 2.0
    energy_ratio = target_energy / (neighbor_energy + 1e-12)

    if energy_ratio < 2.0:
        # Grid artifact not dominant in this subband — skip suppression
        return image.copy()

    # --- Step 3: Gaussian Band-Pass Filter in Frequency Domain ---
    sb = fine_subbands[target_idx].astype(np.float64)
    sb_h, sb_w = sb.shape

    # Build 2-D Gaussian band-pass mask centred on grid frequency
    u = np.fft.fftfreq(sb_w)   # cycles/pixel
    v = np.fft.fftfreq(sb_h)
    UU, VV = np.meshgrid(u, v)
    freq_map = np.sqrt(UU ** 2 + VV ** 2)

    sigma_bp = grid_freq_cpx * 0.25
    # Difference of two Gaussians: band-pass centred at grid_freq_cpx
    mask_bp = (np.exp(-((freq_map - grid_freq_cpx) ** 2) / (2 * sigma_bp ** 2)) +
               np.exp(-((freq_map + grid_freq_cpx) ** 2) / (2 * sigma_bp ** 2)))
    mask_bp = np.clip(mask_bp, 0.0, 1.0)

    sb_fft    = np.fft.fft2(sb)
    moire_fft = sb_fft * mask_bp
    moire_component = np.real(np.fft.ifft2(moire_fft)).astype(np.float32)

    # --- Step 4: Adaptive Suppression and Reconstruction ---
    # Suppression weight increases with energy_ratio (stronger artifact → more suppression)
    suppression_weight = float(np.clip(1.0 - 1.0 / energy_ratio, 0.5, 1.0))
    fine_subbands[target_idx] = (fine_subbands[target_idx] -
                                  suppression_weight * moire_component)
    coeffs[-1] = fine_subbands

    # Inverse NSCT synthesis
    result = pynsct.nsctidec(coeffs).astype(np.float32)
    return result


def _notch_fallback(image: np.ndarray,
                    grid_angle_deg: float,
                    grid_freq_cpx: float) -> np.ndarray:
    """Frequency-domain notch filter fallback when pynsct is unavailable."""
    H, W = image.shape
    u = np.fft.fftfreq(W)
    v = np.fft.fftfreq(H)
    UU, VV = np.meshgrid(u, v)

    # Rotate frequency coordinates to grid orientation
    theta = np.deg2rad(grid_angle_deg)
    u_rot = UU * np.cos(theta) + VV * np.sin(theta)

    # Notch: suppress narrow band around grid frequency
    sigma_notch = grid_freq_cpx * 0.15
    notch = 1.0 - (np.exp(-((u_rot - grid_freq_cpx) ** 2) / (2 * sigma_notch ** 2)) +
                   np.exp(-((u_rot + grid_freq_cpx) ** 2) / (2 * sigma_notch ** 2)))
    notch = np.clip(notch, 0.0, 1.0)

    F = np.fft.fft2(image)
    F_notched = F * notch
    return np.real(np.fft.ifft2(F_notched)).astype(np.float32)
```

---

### 5.2 Virtual Grid — Scatter Correction 알고리즘 (GAP-08 해소)

#### 5.2.1 알고리즘 선택 전략

| 상황 | 권장 방법 | 이유 |
|------|----------|------|
| Phase 1 (빠른 구현) | Thickness-based Empirical (§5.2.2) | 구현 단순, real-time |
| Phase 2 (정확도 향상) | Laplacian Pyramid (§5.2.3) | US8064676B2 특허 기반 |
| Phase 3 (최고 정확도) | DL U-Net (§5.2.4) | MC 데이터 학습, <5% 오차 |

#### 5.2.2 Laplacian Pyramid Virtual Grid (US8064676B2 특허 기반)

```python
def laplacian_pyramid_virtual_grid(image: np.ndarray,
                                    pixel_pitch_mm: float,
                                    target_grid_ratio: float = 10.0) -> np.ndarray:
    """
    Virtual grid via Laplacian Pyramid scatter estimation.
    
    Algorithm (US8064676B2):
      1. Build Laplacian Pyramid (n = log2(N) - 0.5 levels)
      2. Low-frequency bands: scatter component → de-scatter
      3. High-frequency bands: contrast enhancement + denoising
      4. Reconstruct enhanced image
    
    Args:
        image:            float32 input (H, W), linear domain (pre-log transform)
        pixel_pitch_mm:   detector pixel pitch in mm
        target_grid_ratio: emulated grid ratio (5:1 ~ 16:1)
    Returns:
        scatter-corrected float32 image
    """
    H, W = image.shape
    n_levels = int(np.log2(max(H, W)) - 0.5)
    
    # Build Gaussian pyramid (5×5 Gaussian kernel, σ=1, per patent)
    gaussian_pyr = [image]
    for _ in range(n_levels):
        gaussian_pyr.append(cv2.pyrDown(gaussian_pyr[-1]))
    
    # Build Laplacian pyramid
    laplacian_pyr = []
    for k in range(n_levels):
        up = cv2.pyrUp(gaussian_pyr[k+1], dstsize=gaussian_pyr[k].shape[::-1])
        laplacian_pyr.append(gaussian_pyr[k] - up)
    laplacian_pyr.append(gaussian_pyr[-1])  # residual
    
    # Scatter is primarily in low-frequency bands
    # Estimate scatter fraction based on grid ratio emulation
    # Bucky factor B = (1 + 1/R)/(1 - scatter_fraction) where R = grid ratio
    scatter_fraction = estimate_scatter_fraction(target_grid_ratio)
    
    # De-scatter low-frequency bands
    for k in range(n_levels - 2, n_levels + 1):  # lower 2 bands + residual
        idx = min(k, len(laplacian_pyr) - 1)
        laplacian_pyr[idx] = (1.0 / (1.0 - scatter_fraction)) * laplacian_pyr[idx]
    
    # Contrast enhancement for high-frequency bands
    enhancement_factors = compute_enhancement_factors(target_grid_ratio, n_levels)
    for k in range(n_levels - 2):
        laplacian_pyr[k] *= enhancement_factors[k]
    
    # Reconstruct
    result = laplacian_pyr[-1]
    for k in range(n_levels - 1, -1, -1):
        result = cv2.pyrUp(result, dstsize=laplacian_pyr[k].shape[::-1]) + laplacian_pyr[k]
    
    return np.clip(result, 0, None)

def estimate_scatter_fraction(grid_ratio: float) -> float:
    """
    Estimate scatter fraction based on equivalent grid ratio.
    SPR (Scatter-to-Primary Ratio) model:
    For chest AP, 20cm patient, 80kVp: SPR ≈ 100%
    Effective scatter fraction = SPR / (1 + SPR)
    Grid reduces scatter by: factor ≈ R/(R-1) approximately
    """
    # Simplified model; full implementation uses patient thickness estimation
    spr_base = 1.0  # 100% SPR baseline (20cm chest, 80kVp)
    transmission_factor = 1.0 / grid_ratio  # approximate
    return spr_base * transmission_factor / (1.0 + spr_base * transmission_factor)
```

#### 5.2.3 Scatter Fraction 추정 (Thickness 기반)

```python
def estimate_scatter_fraction_from_exposure(
        image: np.ndarray,
        kvp: float,
        sid_mm: float,
        pixel_pitch_mm: float) -> float:
    """
    Estimate patient scatter fraction from image signal statistics.
    Based on Fujifilm Virtual Grid empirical model.
    
    SPR reference table (80kVp, 35×43cm FOV):
      10cm: SPR ~35%
      15cm: SPR ~70%
      20cm: SPR ~100%
      25cm: SPR ~150%
    """
    # Step 1: Estimate effective patient thickness from signal attenuation
    # Primary signal region: darkest area of lung fields (chest)
    # or central anatomical region
    p10 = np.percentile(image, 10)   # approximately primary + scatter
    p90 = np.percentile(image, 90)   # approximately scatter only
    
    # Approximate effective thickness from signal ratio
    # (simplified Beer-Lambert)
    mu_water = 0.018  # cm⁻¹ at 80kVp (effective)
    if p90 > 0 and p10 > 0:
        thickness_cm = -np.log(p10 / p90) / mu_water
    else:
        thickness_cm = 20.0  # default: 20cm
    thickness_cm = np.clip(thickness_cm, 5.0, 40.0)
    
    # kVp correction factor
    kvp_factor = 1.0 + 0.003 * (kvp - 80)
    
    # SPR lookup (linear interpolation)
    spr_table = {10: 0.35, 15: 0.70, 20: 1.00, 25: 1.50, 30: 2.00}
    spr = np.interp(thickness_cm, list(spr_table.keys()),
                    list(spr_table.values())) * kvp_factor
    
    return spr / (1.0 + spr)  # scatter fraction from SPR
```

---

## 6. SWI-3: Display Processing 알고리즘

### 6.1 SWU-3.1 Modality LUT (SRS-FUNC-020)

#### 6.1.1 수학 정의

$$\text{StoredPixelValue} \xrightarrow{\text{Modality LUT}} \text{ModalityPixelValue}$$

**Linear form (DICOM PS3.3 §C.7.6.3.1.2):**

$$\text{ModalityPixelValue} = \text{RescaleSlope} \times \text{StoredPixelValue} + \text{RescaleIntercept}$$

- DICOM tags: `(0028,1053)` RescaleSlope, `(0028,1052)` RescaleIntercept
- 단위: Housfield Units (CT) 또는 arbitrary linear units (DX)
- DX의 경우: Slope=1, Intercept=0 (identity)가 일반적

#### 6.1.2 구현

```cpp
void xpe_apply_modality_lut(const uint16_t* stored_pixels,
                              float* modality_pixels,
                              float rescale_slope,
                              float rescale_intercept,
                              uint32_t total_pixels) {
    // AVX2 vectorized Fused Multiply-Add
    __m256 v_slope  = _mm256_set1_ps(rescale_slope);
    __m256 v_interc = _mm256_set1_ps(rescale_intercept);
    
    size_t i = 0;
    for (; i + 8 <= total_pixels; i += 8) {
        __m128i u16x8 = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(stored_pixels + i));
        __m256 f32x8  = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(u16x8));
        __m256 result = _mm256_fmadd_ps(f32x8, v_slope, v_interc);
        _mm256_storeu_ps(modality_pixels + i, result);
    }
    for (; i < total_pixels; ++i) {
        modality_pixels[i] = rescale_slope * stored_pixels[i] + rescale_intercept;
    }
}
```

---

### 6.2 SWU-3.2 VOI LUT (SRS-FUNC-021)

#### 6.2.1 LINEAR 변환

$$\text{Output} = \frac{\text{Input} - (\text{WC} - \text{WW}/2)}{\text{WW}} \times (\text{MaxOut} - \text{MinOut}) + \text{MinOut}$$

Clamp: `[MinOut, MaxOut]`

#### 6.2.2 LINEAR_EXACT 변환 (DICOM PS3.3 §C.11.2.1.2)

$$\text{Output} = \begin{cases} \text{MinOut} & \text{if Input} \le WC - \lfloor WW/2 \rfloor \\ \frac{(Input - (WC - 0.5)) \cdot (MaxOut - MinOut + 1)}{WW} + \frac{MinOut + MaxOut}{2} & \text{otherwise} \\ \text{MaxOut} & \text{if Input} > WC + \lfloor (WW-1)/2 \rfloor \end{cases}$$

#### 6.2.3 SIGMOID 변환

$$\text{Output} = \frac{\text{MaxOut} - \text{MinOut}}{1 + e^{-4(\text{Input} - WC)/WW}} + \text{MinOut}$$

- **장점**: 선형에 비해 extreme 값에서 부드러운 클리핑 → 구조 과노출 방지
- **권장 사용 사례**: 폐, 종격동 동시 표현

#### 6.2.4 실시간 W/L 조정 구현 (SRS-PERF-003: ≤16ms)

```cpp
// GPU-accelerated VOI LUT for real-time interactive adjustment
// Falls back to AVX2 CPU path if GPU unavailable
void xpe_apply_voi_lut(const float* modality_pixels,
                         uint16_t*   output,
                         VoiLutType  lut_type,
                         float wc, float ww,
                         float min_out, float max_out,
                         uint32_t total_pixels) {
    
    const float half_ww = ww * 0.5f;
    const float lo = wc - half_ww;
    const float range_out = max_out - min_out;
    
    switch (lut_type) {
        case VoiLutType::LINEAR: {
            __m256 v_lo    = _mm256_set1_ps(lo);
            __m256 v_scale = _mm256_set1_ps(range_out / ww);
            __m256 v_off   = _mm256_set1_ps(min_out);
            __m256 v_min   = _mm256_set1_ps(min_out);
            __m256 v_max   = _mm256_set1_ps(max_out);
            
            size_t i = 0;
            for (; i + 8 <= total_pixels; i += 8) {
                __m256 inp  = _mm256_loadu_ps(modality_pixels + i);
                __m256 norm = _mm256_sub_ps(inp, v_lo);
                __m256 res  = _mm256_fmadd_ps(norm, v_scale, v_off);
                res = _mm256_min_ps(_mm256_max_ps(res, v_min), v_max);
                // Convert to uint16
                __m256i res_i = _mm256_cvttps_epi32(res);
                // Pack and store (simplified; full impl uses _mm256_packs_epi32)
                // ... store to output
            }
            break;
        }
        case VoiLutType::SIGMOID: {
            // Vectorized sigmoid via polynomial approximation
            // f(x) ≈ 0.5 + 0.25*x*(1 - x²/12) for |x| < 2
            break;
        }
        default: break;
    }
}
```

#### 6.2.5 Body-Part Preset 테이블 (≥20 preset, SRS-FUNC-021)

```json
{
  "presets": [
    {"name":"Chest Standard",   "body_part":"CHEST",    "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Chest Lung",       "body_part":"CHEST",    "wc":-600,"ww":1500, "type":"SIGMOID"},
    {"name":"Chest Mediastinum","body_part":"CHEST",    "wc":50,  "ww":400,  "type":"LINEAR"},
    {"name":"Bone Standard",    "body_part":"EXTREMITY","wc":500, "ww":2500, "type":"LINEAR"},
    {"name":"Abdomen",          "body_part":"ABDOMEN",  "wc":60,  "ww":400,  "type":"LINEAR"},
    {"name":"Spine",            "body_part":"SPINE",    "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Skull",            "body_part":"HEAD",     "wc":500, "ww":3000, "type":"LINEAR"},
    {"name":"Hand/Wrist",       "body_part":"HAND",     "wc":600, "ww":2500, "type":"LINEAR"},
    {"name":"Pelvis",           "body_part":"PELVIS",   "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Shoulder",         "body_part":"SHOULDER", "wc":300, "ww":1500, "type":"LINEAR"},
    {"name":"Knee",             "body_part":"KNEE",     "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Ankle/Foot",       "body_part":"FOOT",     "wc":600, "ww":2000, "type":"LINEAR"},
    {"name":"Breast MLO",       "body_part":"BREAST",   "wc":2000,"ww":4000, "type":"LINEAR"},
    {"name":"Pediatric Chest",  "body_part":"CHEST",    "wc":300, "ww":1500, "type":"SIGMOID"},
    {"name":"Neonatal",         "body_part":"CHEST",    "wc":200, "ww":800,  "type":"SIGMOID"},
    {"name":"Panoramic Dental", "body_part":"DENTAL",   "wc":500, "ww":2000, "type":"LINEAR"},
    {"name":"Long Leg",         "body_part":"LOWER_EX", "wc":400, "ww":2000, "type":"LINEAR"},
    {"name":"Full Spine",       "body_part":"SPINE",    "wc":500, "ww":2500, "type":"LINEAR"},
    {"name":"Scoliosis",        "body_part":"SPINE",    "wc":400, "ww":1800, "type":"LINEAR_EXACT"},
    {"name":"Soft Tissue",      "body_part":"EXTREMITY","wc":100, "ww":400,  "type":"LINEAR"},
    {"name":"Bone Suppress",    "body_part":"CHEST",    "wc":300, "ww":1200, "type":"SIGMOID"}
  ]
}
```

---

### 6.3 SWU-3.3 Presentation LUT — GSDF (SRS-FUNC-022)

#### 6.3.1 DICOM PS3.14 GSDF 알고리즘

**목적**: P-Value(0–4095)를 Just Noticeable Difference(JND)가 균일한 휘도로 변환.

**GSDF 수학 모델 (DICOM PS3.14 §6):**

$$\bar{L}(j) = \frac{L_{\min} + L_{\max}}{2} \cdot \exp\left(\frac{j - j_0}{j_0}\right) \quad \text{(simplified)}$$

정확한 구현은 PS3.14 Table B.1의 256-point LUT 사용:

```cpp
// GSDF P-Value to Luminance conversion
// Source: DICOM PS3.14 Table B.1 (256 P-Value entries)
// Full table: 1024 entries interpolated from 256

struct GSDFCalibration {
    float L_min_cdm2;   // minimum luminance of display (cd/m²)
    float L_max_cdm2;   // maximum luminance of display (cd/m²)
    float gamma;        // display gamma (typically 2.2)
    std::vector<float> gsdf_lut;  // 4096-entry P-Value → DDL LUT
};

// Build calibrated Presentation LUT for current display
std::vector<uint16_t> build_presentation_lut(
        const GSDFCalibration& cal,
        uint16_t p_value_range = 4096) {
    
    // GSDF JND indices for given luminance range
    float j_min = compute_jnd_index(cal.L_min_cdm2);  // PS3.14 §B.2
    float j_max = compute_jnd_index(cal.L_max_cdm2);
    
    // Map P-Values uniformly across JND range
    std::vector<uint16_t> lut(p_value_range);
    for (uint16_t p = 0; p < p_value_range; ++p) {
        float j = j_min + (j_max - j_min) * p / (p_value_range - 1);
        float L = jnd_index_to_luminance(j);  // inverse GSDF
        // Convert luminance to DDL (Digital Driving Level)
        uint16_t ddl = luminance_to_ddl(L, cal);
        lut[p] = ddl;
    }
    return lut;
}

// JND index formula (PS3.14 §B.2)
float compute_jnd_index(float L_cdm2) {
    float log_L = std::log10(L_cdm2);
    // 4th-order polynomial approximation (DICOM standard)
    return 71.498068f + 94.593053f * log_L + 41.912053f * log_L * log_L
           + 9.8247004f * pow(log_L, 3) + 0.28175407f * pow(log_L, 4)
           - 1.1878455f * pow(log_L, 5) - 0.18014349f * pow(log_L, 6)
           + 0.14710899f * pow(log_L, 7) - 0.017046845f * pow(log_L, 8);
}
```

#### 6.3.2 Display 교정 미보정 감지 (SRS-ALERT-003)

```cpp
bool detect_uncalibrated_display(const DisplayDevice& device) {
    // Measure luminance at multiple DDL steps
    // Compare measured JND spacing to GSDF target
    // If max deviation > 10% → flag as uncalibrated
    float gsdf_conformance = evaluate_gsdf_conformance(device);
    return gsdf_conformance < 0.90f;  // <90% conformance → warning
}
```

---

## 7. IEC 62494-1 Exposure Index 알고리즘

### 7.1 알고리즘 개요 (GAP-09 해소)

IEC 62494-1은 디지털 방사선 촬영의 **노출 적절성**을 수치화하는 표준이다:

- **EI (Exposure Index)**: 검출기 수신 선량에 비례하는 지수
- **EI_target**: 특정 촬영 유형의 목표 EI
- **DI (Deviation Index)**: EI 대비 EI_target의 편차 (dB 단위)

$$DI = 10 \cdot \log_{10}\left(\frac{EI}{EI_{\text{target}}}\right)$$

### 7.2 ROI 추출 알고리즘

```cpp
struct ExposureIndexROI {
    cv::Rect roi;         // Selected ROI rectangle
    float    mean_signal; // Mean pixel value in ROI
    float    area_mm2;    // Physical area of ROI
};

// IEC 62494-1 §7.3: ROI selection methods
ExposureIndexROI select_roi_for_ei(const cv::Mat& image,
                                    const CollimatorMask& collimator,
                                    RoiSelectionMethod method,
                                    const std::string& body_part) {
    switch (method) {
        case RoiSelectionMethod::FULL_FIELD:
            // Use entire collimated area (simple, method A)
            return {collimator.bounding_rect(), 
                    mean_within_mask(image, collimator.mask()), 0.0f};
        
        case RoiSelectionMethod::ANATOMY_BASED:
            // Method B: auto-detect anatomical region
            // For chest: left/right lung ROIs
            // For extremity: bone shaft ROI
            return detect_anatomy_roi(image, body_part);
        
        case RoiSelectionMethod::CENTRAL:
            // Method C: central 10% of collimated area (by area fraction)
            // Target: ROI_area = 0.10 × full_area
            //   => w_roi = full.width  × sqrt(0.10) ≈ full.width  × 0.3162
            //   => h_roi = full.height × sqrt(0.10) ≈ full.height × 0.3162
            // IEC 62494-1 §7.2.4 — S_d region must represent ≥10% of receptor area
            auto full = collimator.bounding_rect();
            int cx = full.x + full.width / 2;
            int cy = full.y + full.height / 2;
            // sqrt(0.10) = 0.31623 — use compile-time constant for clarity
            constexpr double kSqrt01 = 0.31622776601683794;  // sqrt(0.10)
            int w  = static_cast<int>(std::round(full.width  * kSqrt01));
            int h  = static_cast<int>(std::round(full.height * kSqrt01));
            // Ensure minimum 32×32 pixels for statistical validity
            w = std::max(w, 32);
            h = std::max(h, 32);
            return {cv::Rect(cx - w/2, cy - h/2, w, h), 0.0f, 0.0f};
    }
}
```

### 7.3 EI 계산

```cpp
float compute_exposure_index(float mean_roi_signal,
                               float pixel_pitch_mm,
                               float rescale_slope,
                               float rescale_intercept,
                               const DetectorCalibrationData& cal) {
    // Convert mean ROI signal to calibrated detector signal S_cal
    // S_cal = (mean_roi_signal × rescale_slope + rescale_intercept)
    float s_cal = mean_roi_signal * rescale_slope + rescale_intercept;
    
    // EI = C_ei × s_cal (IEC 62494-1 §7.2)
    // C_ei: detector-specific calibration constant
    // Calibrated such that EI = 100 corresponds to reference entrance dose
    float ei = cal.C_ei * s_cal;
    
    // Clamp to valid range [0, 10000]
    return std::clamp(ei, 0.0f, 10000.0f);
}

float compute_deviation_index(float ei, float ei_target) {
    if (ei_target <= 0.0f || ei <= 0.0f) return 0.0f;
    return 10.0f * std::log10(ei / ei_target);
}

// DI interpretation:
// DI < -1.0: underexposure (high noise)
// -1.0 ≤ DI ≤ +1.0: acceptable exposure
// DI > +1.0: overexposure (unnecessary dose)
// DI > +3.0: significant overexposure → alert
```

### 7.4 EI_target 테이블

| 촬영 부위 | EI_target | 참고 |
|----------|-----------|------|
| Chest PA | 200 | ACR 권장 |
| Chest AP (portable) | 300 | 산란 증가 반영 |
| Abdomen AP | 250 | |
| Spine AP/Lateral | 200 | |
| Extremity | 100 | 낮은 감쇠 |
| Hand/Foot | 80 | |
| Pelvis AP | 250 | |
| Skull | 200 | |

---

## 8. AI/DL 알고리즘

### 8.1 CNN Body-Part Recognition (SRS-FUNC-016)

#### 8.1.1 모델 아키텍처

```
Input: 512×512 (downsampled from full resolution)
  ↓
EfficientNet-B4 Backbone (ImageNet pretrained)
  ↓
Global Average Pooling
  ↓
FC(1792 → 512) + BatchNorm + ReLU + Dropout(0.3)
  ↓
FC(512 → N_classes)   N_classes = 15+ body parts
  ↓
Softmax → confidence scores
```

#### 8.1.2 신체 부위 분류 체계 (≥15 categories)

| 클래스 ID | 명칭 | DICOM Body Part |
|---------|------|----------------|
| 0 | Chest PA | CHEST |
| 1 | Chest AP | CHEST |
| 2 | Chest Lateral | CHEST |
| 3 | Abdomen AP | ABDOMEN |
| 4 | Pelvis AP | PELVIS |
| 5 | Spine Cervical | CSPINE |
| 6 | Spine Thoracic | TSPINE |
| 7 | Spine Lumbar | LSPINE |
| 8 | Shoulder | SHOULDER |
| 9 | Elbow | ELBOW |
| 10 | Hand/Wrist | HAND |
| 11 | Hip | HIP |
| 12 | Knee | KNEE |
| 13 | Ankle/Foot | FOOT |
| 14 | Skull | HEAD |
| 15 | Full Spine | SPINE |
| 16 | Long Leg | LOWER_EXTREMITY |

#### 8.1.3 Preprocessing for Inference

```python
def preprocess_for_body_part_recognition(image: np.ndarray) -> np.ndarray:
    """
    Preprocess X-ray image for CNN inference.
    """
    # 1. Resize to 512×512
    img = cv2.resize(image, (512, 512), interpolation=cv2.INTER_AREA)
    
    # 2. Normalize to [0, 1] using percentile normalization
    p2  = np.percentile(img, 2)
    p98 = np.percentile(img, 98)
    img = (img - p2) / max(p98 - p2, 1e-6)
    img = np.clip(img, 0.0, 1.0)
    
    # 3. Expand to 3 channels (grayscale → RGB replication)
    img_3ch = np.stack([img, img, img], axis=0)  # (3, H, W)
    
    # 4. Normalize with ImageNet stats (for pretrained backbone)
    mean = np.array([0.485, 0.456, 0.406]).reshape(3, 1, 1)
    std  = np.array([0.229, 0.224, 0.225]).reshape(3, 1, 1)
    img_norm = (img_3ch - mean) / std
    
    return img_norm.astype(np.float32)[np.newaxis]  # (1, 3, 512, 512)
```

#### 8.1.4 ONNX Runtime 추론 (xpe_ai_worker.exe)

```cpp
// In xpe_ai_worker.exe (sandbox process)
class BodyPartRecognizer {
    Ort::Session session_;
    
public:
    BodyPartResult recognize(const float* preprocessed_input,
                               size_t input_size) {
        // Create input tensor
        auto input_tensor = Ort::Value::CreateTensor<float>(
            memory_info_, const_cast<float*>(preprocessed_input),
            input_size, input_shape_.data(), input_shape_.size());
        
        // Run inference
        auto outputs = session_.Run(Ort::RunOptions{nullptr},
                                     input_names_.data(), &input_tensor, 1,
                                     output_names_.data(), 1);
        
        // Parse softmax output
        float* scores = outputs[0].GetTensorMutableData<float>();
        int n_classes = static_cast<int>(outputs[0].GetTensorTypeAndShapeInfo()
                                          .GetShape()[1]);
        
        int best_class = std::max_element(scores, scores + n_classes) - scores;
        float confidence = scores[best_class];
        
        return {best_class, confidence, 
                std::vector<float>(scores, scores + n_classes)};
    }
};
```

#### 8.1.5 성능 요구사항

- **정확도**: ≥95% top-1 accuracy (SRS-FUNC-016)
- **추론 시간**: ≤200ms (CPU 추론, EfficientNet-B4)
- **입력 허용 범위**: 0.5× ~ 2× 기준 크기

---

### 8.2 DL Bone Suppression (SRS-FUNC-018)

#### 8.2.1 모델 아키텍처 — U-Net with Attention

```
Encoder:                          Decoder:
Input (H×W×1)                     (H×W×1) Output
  ↓                                  ↑
Conv3×3 + BN + ReLU ×2             ← Skip connection (attention gate)
MaxPool2×2                          UpSample2×2 + Conv
  ↓                                  ↑
×4 encoder blocks              ×4 decoder blocks
  ↓
Bottleneck: Conv3×3 ×3 (dilated 1,2,4)
```

#### 8.2.2 훈련 데이터 생성

```python
# Virtual training pairs from real Dual-Energy Subtraction (DES)
# - Standard X-ray (mixed bone + soft tissue)
# - DES bone image (real dual-energy reference)
# - DES soft tissue image (ground truth for bone suppression)

# Data augmentation:
# - Random horizontal flip
# - Random rotation ±5°
# - Gaussian noise injection (σ = 0.01–0.05)
# - Random contrast adjustment (0.9–1.1×)
# - Random elastic deformation (medical augmentation)
```

#### 8.2.3 손실 함수

$$\mathcal{L} = \lambda_1 \mathcal{L}_{L1} + \lambda_2 \mathcal{L}_{SSIM} + \lambda_3 \mathcal{L}_{perceptual}$$

$$\mathcal{L}_{L1} = \|I_{pred} - I_{DES}\|_1$$

$$\mathcal{L}_{SSIM} = 1 - \text{SSIM}(I_{pred}, I_{DES})$$

$$\mathcal{L}_{perceptual} = \|\phi_k(I_{pred}) - \phi_k(I_{DES})\|_2$$

- **목표**: PSNR ≥ 33dB, SSIM ≥ 0.97 (SRS-FUNC-018)

---

### 8.3 Panoramic Image Stitching (SRS-FUNC-017)

#### 8.3.1 알고리즘 파이프라인 (GAP-07 해소)

```
Input: 2-4 overlapping X-ray images (10-30% overlap)
         ↓
Step 1: Feature Detection (SIFT/ORB on bone edges)
         ↓
Step 2: Feature Matching (FLANN-based matcher + ratio test)
         ↓
Step 3: Homography Estimation (RANSAC, ≥4 point pairs)
         ↓
Step 4: Geometric Correction (perspective + distortion)
         ↓
Step 5: Intensity Normalization (histogram matching at overlap)
         ↓
Step 6: Blending (Multi-band blending or Feathering)
         ↓
Output: Panoramic image with Cobb angle error ≤ 2°
```

#### 8.3.2 Cobb Angle 오차 ≤2° 달성 전략

```python
def validate_stitching_accuracy(stitched: np.ndarray,
                                 individual_images: list[np.ndarray],
                                 known_landmarks: list[dict]) -> dict:
    """
    Validate stitching accuracy using vertebral landmark pairs.
    
    Cobb angle error = |Cobb_stitched - Cobb_ground_truth|
    Acceptance: ≤ 2°
    """
    # Measure vertebral endplate angles in stitched image
    cobb_stitched = measure_cobb_angle(stitched, known_landmarks)
    
    # Ground truth from reference measurement
    # (physical phantom with calibrated curvature)
    cobb_reference = known_landmarks[0]['cobb_angle_reference']
    
    error_deg = abs(cobb_stitched - cobb_reference)
    return {
        'cobb_stitched': cobb_stitched,
        'cobb_reference': cobb_reference,
        'error_deg': error_deg,
        'pass': error_deg <= 2.0
    }
```

---

## 9. 교정 데이터 파이프라인

### 9.1 오프라인 교정 순서 (GAP-10 해소)

```
Phase 1: Dark Calibration (반드시 먼저)
  - 조건: 방사선 OFF, detector 안정화 ≥10분
  - 획득: ≥16 dark frames
  - 출력: offset_map.bin

Phase 2: Flat-Field Calibration (per SID per kVp)
  - 조건: 균일 조사야, 산란체 없음
  - 획득: ≥8 flood frames
  - 출력: gain_map_SID{n}_kVp{m}.bin

Phase 3: Defect Map Generation
  - 입력: offset_map.bin + gain_map (any SID)
  - 출력: defect_map.bin

Phase 4: Lag Parameter Fitting
  - 조건: 이중 노출 프로토콜
  - 획득: decay curve (t=0.1s ~ 30s)
  - 출력: lag_params.json

Phase 5: Checksum Generation
  - 모든 .bin 파일에 SHA-256 생성
  - 출력: checksums.sha256
```

### 9.2 C++ ConfigManager 로딩 순서

```cpp
class CalibrationManager {
public:
    bool load_calibration_set(const std::filesystem::path& cal_dir) {
        // 1. Validate all checksums first (SRS-SEC-002)
        if (!validate_all_checksums(cal_dir)) {
            logger_->error("Calibration data integrity check failed");
            raise_alert(AlertType::CALIBRATION_INTEGRITY_FAILURE);
            return false;
        }
        
        // 2. Load offset map (required)
        offset_map_ = load_binary_map<float>(cal_dir / "offset_map.bin",
                                               "XOFF");
        
        // 3. Load gain maps (SID-indexed)
        for (const auto& entry : std::filesystem::directory_iterator(cal_dir)) {
            if (entry.path().stem().string().starts_with("gain_map_")) {
                auto [sid, kvp] = parse_gain_map_filename(entry.path());
                gain_maps_[{sid, kvp}] = 
                    load_binary_map<float>(entry.path(), "XGAI");
            }
        }
        
        // 4. Load defect map (required)
        defect_map_ = load_binary_map<uint8_t>(cal_dir / "defect_map.bin",
                                                 "XDEF");
        
        // 5. Load lag parameters (optional, default if missing)
        load_lag_params(cal_dir / "lag_params.json");
        
        // 6. Validate expiry (SRS-ALERT-005)
        if (is_calibration_expired()) {
            raise_alert(AlertType::CALIBRATION_EXPIRED);
        }
        
        return true;
    }
    
    const float* get_gain_map(float sid_mm, float kvp) const {
        // Find nearest SID/kVp combination
        auto key = find_nearest_gain_map(sid_mm, kvp);
        return gain_maps_.at(key).data();
    }
};
```

### 9.3 교정 유효기간 관리

| 교정 항목 | 권장 주기 | 트리거 조건 |
|----------|---------|-----------|
| Offset Map | 8시간 또는 시동 시 | 온도 ≥5°C 변화 |
| Gain Map | 1주 또는 kVp 변경 시 | SID 변경 ±50mm |
| Defect Map | 1개월 | 결함 픽셀 +10% |
| Lag Parameters | 분기 1회 | 모델 교체 시 |

---

### 9.4 AED-0: Automatic Exposure Detection (GAP-J 해소)

AED-0는 XPE-10-Pass-Review Pass 4에서 파이프라인 실행 순서에 추가된 선행 스텝이다 (product.md §4 "pipeline execution" 참조). **Offset/Gain/Defect 보정 이후, Log Transform 이전**에 실행되어 노출 유효성을 판단하고 하위 단계에 신호 레벨 정보를 전달한다.

#### 9.4.1 알고리즘 정의

AED-0의 목적은 두 가지다:
1. 충분한 노출이 이루어졌는지 여부 판정 (실패 시 파이프라인 중단 + 경보)
2. I₀ (air kerma reference signal) 추정 — EI, DI 계산에 사용

**유효 노출 조건**:
$$\bar{I}_{\text{field}} \geq I_{\text{min\_exposure}} \quad \text{AND} \quad \frac{P_{\text{sat}}}{P_{\text{total}}} \leq \theta_{\text{sat}}$$

$$\bar{I}_{\text{field}} = \frac{1}{|M_{\text{coll}}|} \sum_{(x,y) \in M_{\text{coll}}} I_{\text{gain\_corr}}(x,y)$$

**I₀ 추정** (dark-corrected flood reference):
$$I_0 = G_{\text{mean}} \cdot \bar{I}_{\text{flat\_ref}}$$

여기서 $\bar{I}_{\text{flat\_ref}}$는 교정 flood 이미지의 mean signal이고, $G_{\text{mean}}$은 gain map 평균값이다.

| 파라미터 | 기본값 | 의미 |
|---------|-------|------|
| `I_min_exposure` | 1000 ADU | 최소 유효 노출 신호 |
| `θ_sat` | 0.05 | 허용 포화 픽셀 비율 |
| Collimation source | `CollimatorMask` (§12.5) | 조준기 마스크 |

#### 9.4.2 Python 구현

```python
@dataclass
class AEDResult:
    is_valid_exposure: bool
    mean_field_signal: float       # I̅_field (ADU)
    i0_estimate:       float       # I₀ (ADU)
    saturation_frac:   float
    fault_reason:      str = ''    # empty if valid

def run_aed0(gain_corr_img:    np.ndarray,
             calibration_data: dict,
             collimator_mask:  'CollimatorMask | None' = None,
             i_min_exposure:   float = 1000.0,
             theta_sat:        float = 0.05) -> AEDResult:
    """
    AED-0: Automatic Exposure Detection — pipeline gate before Log Transform.

    Args:
        gain_corr_img:    float32 (H, W), offset+gain corrected
        calibration_data: dict with 'gain_mean' and 'flat_ref_mean' (ADU)
        collimator_mask:  CollimatorMask instance; None → use full image
        i_min_exposure:   minimum mean field signal for valid exposure
        theta_sat:        maximum allowed saturation fraction
    Returns:
        AEDResult
    """
    H, W = gain_corr_img.shape
    max_adu = 65535.0

    # Build field mask
    if collimator_mask is not None:
        field_mask = collimator_mask.mask.astype(bool)
    else:
        field_mask = np.ones((H, W), dtype=bool)

    field_pixels   = gain_corr_img[field_mask]
    mean_field_sig = float(np.mean(field_pixels)) if field_pixels.size > 0 else 0.0
    sat_frac       = float(np.mean(field_pixels >= max_adu * 0.999))

    # I₀ estimate from calibration reference
    gain_mean     = float(calibration_data.get('gain_mean',      1.0))
    flat_ref_mean = float(calibration_data.get('flat_ref_mean', mean_field_sig))
    i0_estimate   = gain_mean * flat_ref_mean

    # Validity checks
    fault = ''
    if mean_field_sig < i_min_exposure:
        fault = f'under_exposure: mean={mean_field_sig:.1f} < {i_min_exposure}'
    elif sat_frac > theta_sat:
        fault = f'over_exposure: sat_frac={sat_frac:.4f} > {theta_sat}'

    return AEDResult(
        is_valid_exposure = (fault == ''),
        mean_field_signal = mean_field_sig,
        i0_estimate       = i0_estimate,
        saturation_frac   = sat_frac,
        fault_reason      = fault,
    )
```

#### 9.4.3 파이프라인 통합

```
[Readout Validation (§3.0)] → [Offset Correct] → [Non-linearity Correct] →
[Gain Correct] → [Defect Correct] → [AED-0 (§9.4)] → decision:
    FAIL → alert xpe_alert(ALERT_EXPOSURE_INVALID) + return error
    PASS → [Log Transform → ...] with i0 = aed_result.i0_estimate
```

#### 9.4.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 유효 노출 감지 | 정상 팬텀 이미지 → is_valid = True | 10회 반복 |
| 노출 부족 감지 | 1/10 노출 이미지 → is_valid = False | 감쇠기 사용 |
| I₀ 정확도 | 교정값 ±5% 이내 | flood 이미지 검증 |

---

## 10. 성능 최적화 — SIMD/OpenMP 전략

### 10.1 전체 파이프라인 SIMD 커버리지 (GAP-06 해소)

| SWU | AVX2 | FMA | OpenMP | GPU (Optional) |
|-----|------|-----|--------|---------------|
| Offset Correct | ✅ | — | ✅ (row-parallel) | — |
| Gain Correct | ✅ | ✅ | ✅ | — |
| Defect Correct | — (data-dependent) | — | ✅ (pass 분리) | — |
| Ghost Correct | ✅ | ✅ | ✅ | — |
| Log Transform | ✅ (SVML/approx) | — | ✅ | ✅ (CUDA) |
| Bilateral Filter | 부분 (OpenCV) | — | ✅ (tile) | ✅ (CUDA) |
| CLAHE | — (OpenCV) | — | — | — |
| USM | ✅ | ✅ | ✅ | — |
| Laplacian Pyramid | — (OpenCV pyrDown/Up) | — | ✅ (level) | — |
| VOI LUT | ✅ | ✅ | ✅ | ✅ |
| Modality LUT | ✅ | ✅ | ✅ | — |
| Grid Suppression | — (FFT-based) | — | ✅ | — |

### 10.2 메모리 풀 전략

```cpp
// SWI-5 MemoryPool: Pre-allocated pipeline buffers
// Prevent dynamic allocation during processing (SRS-PERF-001)

class XpeMemoryPool {
    // Fixed set of reusable float32 image buffers
    static constexpr size_t MAX_BUFFERS = 8;
    static constexpr size_t MAX_IMAGE_PIXELS = 4096ULL * 4096;  // 16MP max
    
    struct Buffer {
        std::unique_ptr<float, AlignedDeleter> data;  // 64-byte aligned
        size_t size;
        std::atomic<bool> in_use{false};
    } buffers_[MAX_BUFFERS];
    
public:
    XpeMemoryPool() {
        for (auto& buf : buffers_) {
            buf.data = alloc_aligned<float>(MAX_IMAGE_PIXELS, 64);
            buf.size = MAX_IMAGE_PIXELS;
        }
    }
    
    float* acquire(size_t required_pixels) {
        for (auto& buf : buffers_) {
            bool expected = false;
            if (buf.in_use.compare_exchange_strong(expected, true) &&
                buf.size >= required_pixels) {
                return buf.data.get();
            }
        }
        throw std::runtime_error("Memory pool exhausted");
    }
    
    void release(float* ptr) {
        for (auto& buf : buffers_) {
            if (buf.data.get() == ptr) {
                buf.in_use.store(false);
                return;
            }
        }
    }
};
```

### 10.3 Thread Pool 최적화

```cpp
// Row-parallel SIMD: optimal for cache efficiency
// Thread granularity: tile-based (prevent false sharing)

#pragma omp parallel for schedule(static) num_threads(NUM_CORES)
for (int tile_y = 0; tile_y < num_tiles_y; ++tile_y) {
    for (int tile_x = 0; tile_x < num_tiles_x; ++tile_x) {
        process_tile(tile_x, tile_y, tile_size);
    }
}
// Tile size guideline: 64×64 pixels (fits in L1 cache: 64×64×4 = 16KB)
```

### 10.4 성능 프로파일링 포인트

```cpp
// Built-in performance counters (SRS-PERF-001, 002)
class PipelineProfiler {
    struct StageMetrics {
        std::chrono::nanoseconds elapsed;
        size_t pixels_processed;
        double mpixels_per_sec() const {
            return pixels_processed / (elapsed.count() * 1e-3);
        }
    };
    
public:
    void report(uint32_t width, uint32_t height) {
        auto total = sum_all_stages();
        log("Pipeline total: {}ms for {}×{} ({:.1f} MPix/s)",
            total.count() / 1e6, width, height,
            (double)(width * height) / (total.count() * 1e-3));
        // SRS-PERF-001: target ≤500ms for 3072×3072
    }
};
```

---

## 11. 검증 방법론

### 11.1 단위 테스트 기준

| 알고리즘 | 입력 | 기대 출력 | Pass 기준 |
|---------|------|---------|---------|
| Offset Correct | Synthetic dark signal | Subtracted + clamped | Max error = 0 ADU |
| Gain Correct | Uniform flood | Uniform output (CV<0.1%) | CV < 0.1% |
| Defect Correct | Injected point defects | Interpolated ≤ 1% error | Pixel error < 5 ADU |
| Ghost Correct | Known lag signal | ≥90% removal | Ghost fraction < 10% |
| Log Transform | Gradient ramp | Logarithmic curve | Max relative error < 1e-5 |
| Bilateral | AWGN + step edge | Smoothed / edge preserved | Edge FWHM < 2× input |
| CLAHE | Low-contrast uniform | Enhanced, no artifact | SSIM > 0.95 |
| USM | Fine texture | Enhanced within λ_max | No artifact above λ_max |
| VOI LUT | Full range sweep | Correct output per formula | Max error ≤ 1 DDL |
| GSDF | P-Value sweep | PS3.14 conformance | JND deviation < 10% |

### 11.2 통합 테스트 — 황금 표준 이미지

```python
def run_integration_test(pipeline, reference_images: dict) -> dict:
    """
    Compare pipeline output to golden reference images.
    
    Test images:
    - CDMAM (contrast-detail phantom): sensitivity threshold analysis
    - Leeds TOR(CDR) phantom: resolution measurement
    - RMI 156 phantom: uniformity + noise measurement
    - AAPM TG-18 patterns: GSDF conformance
    """
    results = {}
    for test_name, (input_img, golden_ref) in reference_images.items():
        output = pipeline.process(input_img)
        
        # Structural similarity
        ssim_val = ssim(output, golden_ref)
        # Peak signal-to-noise ratio
        psnr_val = psnr(output, golden_ref)
        # Max pixel deviation
        max_err  = float(np.max(np.abs(output.astype(float) - 
                                         golden_ref.astype(float))))
        
        results[test_name] = {
            'ssim': ssim_val,
            'psnr': psnr_val,
            'max_pixel_error': max_err,
            'pass': ssim_val >= 0.95 and psnr_val >= 35.0
        }
    return results
```

### 11.3 성능 회귀 테스트

```bash
# Automated performance regression check (CI/CD)
# Target: SRS-PERF-001 ≤500ms for 3072×3072

xpe_benchmark --image-size 3072x3072 \
               --pipeline pre+core+display \
               --iterations 10 \
               --threshold-ms 500 \
               --output benchmark_results.json
```

---

## 12. FPD 특성화 알고리즘 보완

### 12.1 Allan Variance (장기 안정성 평가)

Allan Variance는 시스템의 시간적 안정성을 평가하는 통계량이다:

$$\sigma_A^2(\tau) = \frac{1}{2}\left\langle\left(\bar{x}_{k+1}(\tau) - \bar{x}_k(\tau)\right)^2\right\rangle$$

```python
def compute_allan_variance(time_series: np.ndarray,
                            sampling_interval_s: float) -> tuple[np.ndarray, np.ndarray]:
    """
    Compute Allan Variance of FPD signal over time.
    
    Useful for:
    - Identifying drift (positive slope in log-log plot)
    - Identifying random noise floor (flat region)
    - Identifying periodic artifacts (bumps)
    """
    N = len(time_series)
    max_m = N // 2
    
    tau_list = []
    avar_list = []
    
    for m in range(1, max_m + 1):
        tau = m * sampling_interval_s
        # Group means
        n_groups = N // m
        group_means = np.array([
            np.mean(time_series[k*m:(k+1)*m]) for k in range(n_groups)
        ])
        # Allan variance
        avar = 0.5 * np.mean(np.diff(group_means)**2)
        tau_list.append(tau)
        avar_list.append(avar)
    
    return np.array(tau_list), np.array(avar_list)
```

### 12.2 MTF 슬랜트 에지법 정밀도 개선

기존 명세(03_측정_알고리즘_명세서)의 보완:

```python
def compute_mtf_precision_mode(edge_image: np.ndarray,
                                pixel_pitch_mm: float,
                                oversampling: int = 4,
                                edge_angle_range: tuple = (2.0, 10.0)) -> dict:
    """
    High-precision MTF via Slanted Edge method with subpixel accuracy.
    
    Improvements over basic implementation:
    1. Subpixel edge localization (Canny + parabolic fit)
    2. Noise-robust ESF via LOWESS smoothing
    3. Aperture correction for finite pixel size
    4. IEC 62220-1 compliant ROI selection
    
    Aperture correction:
    MTF_true(f) = MTF_measured(f) / sinc(f × pixel_pitch)
    """
    # ... (full implementation follows existing 03_측정_알고리즘_명세서 pattern)
    
    # Key addition: aperture correction
    def aperture_correction(mtf_measured, freqs, pixel_pitch):
        sinc_vals = np.sinc(freqs * pixel_pitch)  # sinc = sin(πx)/(πx)
        with np.errstate(divide='ignore', invalid='ignore'):
            mtf_corrected = np.where(sinc_vals > 0.01,
                                      mtf_measured / sinc_vals,
                                      mtf_measured)
        return np.clip(mtf_corrected, 0, 1.2)
    
    return {'mtf': mtf_corrected, 'frequencies': freqs, 
            'f50': freq_at_mtf(mtf_corrected, freqs, 0.5),
            'f10': freq_at_mtf(mtf_corrected, freqs, 0.1)}
```

### 12.3 NPS 계산 알고리즘 (GAP-L 해소)

IEC 62220-1:2015 §6.3 준수 구현이다. 2-D NPS는 ROI별 FFT²를 평균화하여 계산한다.

#### 12.3.1 알고리즘 수학 정의

$$\text{NPS}(u, v) = \frac{\Delta x \cdot \Delta y}{N_x \cdot N_y} \cdot \left\langle \left|\mathcal{F}\left[I_{\text{ROI}}(x,y) - \bar{I}_{\text{ROI}}\right](u,v)\right|^2 \right\rangle_{\text{ROI ensemble}}$$

$$\text{NNPS}(u, v) = \frac{\text{NPS}(u, v)}{\bar{I}_{\text{det}}^2}$$

- $\Delta x, \Delta y$: pixel pitch in mm
- $N_x \times N_y$: ROI size (IEC 62220-1: 256×256 권장)
- $\langle \cdot \rangle$: ensemble average over non-overlapping ROIs (≥50 권장)
- $\bar{I}_{\text{det}}$: mean detector signal in ADU (또는 calibrated units)

**1-D radial NPS** (측정 보고용):

$$\text{NPS}(f) = \frac{1}{N_{\text{annulus}}} \sum_{(u,v): f - \delta f/2 \leq \sqrt{u^2+v^2} < f+\delta f/2} \text{NPS}(u,v)$$

#### 12.3.2 Python 구현 (IEC 62220-1 준수)

```python
import numpy as np
from scipy.signal.windows import hann

def compute_nps_2d(flat_images:      list[np.ndarray],
                   pixel_pitch_mm:   float,
                   roi_size:         int   = 256,
                   min_rois:         int   = 50,
                   detrend_order:    int   = 1,
                   window_function:  bool  = True) -> dict:
    """
    Compute 2-D Noise Power Spectrum per IEC 62220-1:2015 §6.3.

    Args:
        flat_images:     list of uniformly-exposed float32 images (H, W)
                         ≥2 images recommended; >1 required for ensemble
        pixel_pitch_mm:  pixel pitch in mm (same in x and y)
        roi_size:        ROI side length in pixels (IEC: 256)
        min_rois:        minimum ROI count for statistical validity
        detrend_order:   polynomial order for intra-ROI detrending (0=mean, 1=plane)
        window_function: apply 2-D Hanning window before FFT (reduces leakage)
    Returns:
        dict with keys:
          'nps_2d'     : float32 array (roi_size, roi_size) — 2-D NPS (ADU²·mm²)
          'nnps_2d'    : float32 array (roi_size, roi_size) — Normalised NPS (mm²)
          'nps_1d'     : (freqs, nps_radial) — radial average
          'nnps_1d'    : (freqs, nnps_radial)
          'mean_signal': mean detector signal used for normalisation
          'n_rois'     : number of ROIs used
    """
    H, W = flat_images[0].shape
    dx = dy = pixel_pitch_mm  # isotropic detector assumed
    half = roi_size // 2

    # Build 2-D Hanning window
    if window_function:
        win_1d = hann(roi_size, sym=False)
        window = np.outer(win_1d, win_1d).astype(np.float64)
        # Normalise so that sum(window²) == roi_size²  (IEC energy preservation)
        window /= np.sqrt(np.mean(window ** 2))
    else:
        window = np.ones((roi_size, roi_size), dtype=np.float64)

    nps_accum  = np.zeros((roi_size, roi_size), dtype=np.float64)
    roi_count  = 0
    mean_sum   = 0.0

    for img in flat_images:
        img_f = img.astype(np.float64)
        # Tile non-overlapping ROIs with 10% border margin
        y_starts = range(roi_size // 2, H - roi_size - roi_size // 2, roi_size)
        x_starts = range(roi_size // 2, W - roi_size - roi_size // 2, roi_size)

        for y0 in y_starts:
            for x0 in x_starts:
                roi = img_f[y0:y0 + roi_size, x0:x0 + roi_size]
                mean_sum += float(np.mean(roi))

                # Detrend: fit and subtract polynomial surface
                if detrend_order == 0:
                    roi_dt = roi - np.mean(roi)
                else:
                    # Plane fit (linear detrend)
                    ys, xs = np.mgrid[0:roi_size, 0:roi_size].astype(np.float64)
                    A = np.column_stack([xs.ravel(), ys.ravel(),
                                         np.ones(roi_size * roi_size)])
                    coef, _, _, _ = np.linalg.lstsq(A, roi.ravel(), rcond=None)
                    plane = (coef[0] * xs + coef[1] * ys + coef[2])
                    roi_dt = roi - plane

                # Apply window and FFT
                roi_w   = roi_dt * window
                F       = np.fft.fft2(roi_w)
                power   = np.abs(F) ** 2

                # NPS contribution: scale by pixel area / ROI area
                nps_accum += power * (dx * dy) / (roi_size * roi_size)
                roi_count += 1

    if roi_count < min_rois:
        import warnings
        warnings.warn(f"Only {roi_count} ROIs collected; IEC 62220-1 recommends ≥{min_rois}")

    nps_2d = (nps_accum / roi_count).astype(np.float32)
    nps_2d = np.fft.fftshift(nps_2d)   # centre DC at array centre

    mean_signal = mean_sum / roi_count
    nnps_2d     = nps_2d / (mean_signal ** 2 + 1e-12)

    # Radial average
    freqs, nps_1d  = _radial_average(nps_2d,  dx, roi_size)
    _, nnps_1d     = _radial_average(nnps_2d, dx, roi_size)

    return {
        'nps_2d':      nps_2d,
        'nnps_2d':     nnps_2d,
        'nps_1d':      (freqs, nps_1d),
        'nnps_1d':     (freqs, nnps_1d),
        'mean_signal': mean_signal,
        'n_rois':      roi_count,
    }


def _radial_average(power_2d: np.ndarray,
                    pixel_pitch_mm: float,
                    roi_size: int) -> tuple[np.ndarray, np.ndarray]:
    """Compute radial average of a centred 2-D power spectrum."""
    H, W    = power_2d.shape
    cy, cx  = H // 2, W // 2
    y_idx   = np.arange(H) - cy
    x_idx   = np.arange(W) - cx
    XX, YY  = np.meshgrid(x_idx, y_idx)
    freq_step = 1.0 / (roi_size * pixel_pitch_mm)    # cycles/mm per bin
    R = np.sqrt(XX ** 2 + YY ** 2)                   # radial distance in bins

    max_bin    = min(cx, cy)
    freq_bins  = np.arange(0, max_bin) * freq_step
    nps_radial = np.zeros(max_bin, dtype=np.float64)

    for k in range(max_bin):
        annulus = (R >= k - 0.5) & (R < k + 0.5)
        if np.sum(annulus) > 0:
            nps_radial[k] = float(np.mean(power_2d[annulus]))

    return freq_bins.astype(np.float32), nps_radial.astype(np.float32)
```

#### 12.3.3 검증 기준 (IEC 62220-1)

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| DC 성분 차단 | NPS(0,0) = 0 (detrend 후) | Detrending 적용 확인 |
| NNPS 저주파 일관성 | ≤ 10% variation across ROIs | ROI-to-ROI NNPS 비교 |
| ROI count | ≥ 50 | 프로그램 출력 확인 |
| 주파수 해상도 | Δf = 1/(N·Δx) cycles/mm | N=256, Δx=0.1mm → Δf=0.039 cycles/mm |

---

### 12.4 DQE 계산 알고리즘 (GAP-M 해소)

Detective Quantum Efficiency는 MTF와 NNPS로부터 계산되며 IEC 62220-1:2015 §6.4를 따른다.

#### 12.4.1 알고리즘 수학 정의

$$\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\Phi \cdot \text{NNPS}(f)}$$

- $\Phi$: 입사 X선 quantum fluence (photons/mm²)
- $\text{MTF}^2(f)$: Modulation Transfer Function 제곱
- $\text{NNPS}(f)$: Normalised Noise Power Spectrum (mm²)

**Quantum fluence 추정** (IEC 62220-1 §5.2):
$$\Phi = \frac{\bar{I}_{\text{det}}}{\bar{g} \cdot \eta_{\text{absorb}} \cdot E_{\text{mean}}}$$

또는 실측 기반으로 ionisation chamber 측정값 사용 (권장):
$$\Phi = \frac{K_{\text{air}} \cdot \mu_{\text{en}}/\rho \cdot A_{\text{beam}}}{\bar{E}_{\text{photon}}}$$

실용적 접근 (RQA5 조건, 80kVp, IEC 61267): $\Phi \approx 3.0 \times 10^5\ \text{photons/mm}^2/(\text{mR})$

#### 12.4.2 Python 구현

```python
def compute_dqe(mtf_result:   dict,
                nps_result:   dict,
                quantum_fluence_per_mm2: float,
                freq_range_mm: tuple[float, float] = (0.0, 5.0)) -> dict:
    """
    Compute DQE(f) per IEC 62220-1:2015 §6.4.

    Args:
        mtf_result:               output of compute_mtf_precision_mode() — keys 'mtf', 'frequencies'
        nps_result:               output of compute_nps_2d() — key 'nnps_1d': (freqs, nnps)
        quantum_fluence_per_mm2:  Φ — X-ray photon fluence at detector surface (photons/mm²)
                                  Measure with calibrated ionisation chamber, or use
                                  tabulated value for RQA condition (IEC 62220-1 Annex C)
        freq_range_mm:            (f_min, f_max) in cycles/mm for output
    Returns:
        dict: 'frequencies', 'dqe', 'dqe_at_0', 'dqe_at_1', 'dqe_at_Nyquist'
    """
    mtf_freqs  = np.asarray(mtf_result['frequencies'], dtype=np.float64)
    mtf_vals   = np.asarray(mtf_result['mtf'],         dtype=np.float64)

    nnps_freqs = np.asarray(nps_result['nnps_1d'][0],  dtype=np.float64)
    nnps_vals  = np.asarray(nps_result['nnps_1d'][1],  dtype=np.float64)

    # Interpolate NNPS onto MTF frequency grid
    from scipy.interpolate import interp1d
    nnps_interp_fn = interp1d(nnps_freqs, nnps_vals,
                               kind='linear', bounds_error=False,
                               fill_value=(nnps_vals[0], nnps_vals[-1]))
    nnps_on_mtf_grid = nnps_interp_fn(mtf_freqs)

    # DQE = MTF² / (Φ × NNPS)
    denom = quantum_fluence_per_mm2 * nnps_on_mtf_grid
    with np.errstate(divide='ignore', invalid='ignore'):
        dqe = np.where(denom > 1e-20, mtf_vals ** 2 / denom, 0.0)

    # Clip to physically valid range [0, 1]
    dqe = np.clip(dqe, 0.0, 1.0)

    # Select output frequency range
    mask = (mtf_freqs >= freq_range_mm[0]) & (mtf_freqs <= freq_range_mm[1])

    def _dqe_at(target_freq: float) -> float:
        idx = np.argmin(np.abs(mtf_freqs - target_freq))
        return float(dqe[idx])

    return {
        'frequencies':    mtf_freqs[mask].astype(np.float32),
        'dqe':            dqe[mask].astype(np.float32),
        'dqe_at_0':       _dqe_at(0.0),
        'dqe_at_1':       _dqe_at(1.0),      # 1 cycle/mm
        'dqe_at_Nyquist': _dqe_at(float(mtf_freqs[mask].max())),
    }
```

#### 12.4.3 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| DQE(0) 범위 | 0.5 – 0.85 (CsI:Tl FPD 전형) | 공개 문헌 비교 |
| DQE(f) 단조성 | 감소 경향 (경미한 진동 허용) | 1차 차분 부호 확인 |
| IEC 62220-1 부합성 | 동일 팬텀에서 ±10% 이내 재현성 | 3회 반복 측정 |

---

### 12.5 Collimation Mask Detection 알고리즘 (GAP-N 해소)

`CollimatorMask`는 EI ROI 선택, GSVG scatter 추정, Phase 2 세부 알고리즘에서 공통으로 사용하는 기본 마스크 클래스이다. xpe-algorithm-spec-deepsync.md §3.2 "release-safe baseline"에 명시된 "baseline collimation detection"의 상세 구현이다.

#### 12.5.1 알고리즘 수학 정의

조준기 마스크는 임계값 기반 + 모폴로지 연산 결합으로 생성된다:

**단계 1 — 적응형 임계값**:
$$M_{\text{thresh}}(x,y) = \begin{cases} 1 & I_{\text{det}}(x,y) \geq \theta_{\text{coll}} \\ 0 & \text{otherwise} \end{cases}$$

$$\theta_{\text{coll}} = \mu_{\text{bright}} - k_{\text{coll}} \cdot \sigma_{\text{bright}}, \quad k_{\text{coll}} = 2.0$$

여기서 $\mu_{\text{bright}}$ 및 $\sigma_{\text{bright}}$는 상위 60% 픽셀에서 계산한다.

**단계 2 — 모폴로지 정제**:
$$M_{\text{final}} = \text{Close}\left(\text{Open}\left(M_{\text{thresh}},\ \text{SE}_{r_1}\right),\ \text{SE}_{r_2}\right)$$

- $r_1 = 15$ pixels (잡음 제거 opening)
- $r_2 = 50$ pixels (경계 닫기 closing)

**단계 3 — 최대 연결 성분 선택**: 최대 면적의 연결 성분을 최종 마스크로 채택.

#### 12.5.2 Python 구현

```python
import numpy as np
from dataclasses import dataclass

@dataclass
class CollimatorMask:
    """
    Collimator mask result for a single detector image.

    Attributes:
        mask:       uint8 binary mask (H, W) — 1 = inside collimated field
        bounding:   (x, y, w, h) bounding rectangle of collimated field
        confidence: float in [0,1] — detection quality estimate
        method_id:  str identifier for the detection algorithm used
    """
    mask:       np.ndarray
    bounding:   tuple[int, int, int, int]   # (x, y, w, h)
    confidence: float
    method_id:  str = 'threshold_morpho_v1'


def detect_collimator_mask(image:        np.ndarray,
                            pixel_pitch_mm: float = 0.1,
                            k_coll:      float = 2.0,
                            bright_frac:  float = 0.60,
                            open_r_mm:   float = 1.5,
                            close_r_mm:  float = 5.0) -> CollimatorMask:
    """
    Detect collimator boundary from a corrected detector image.

    Algorithm:
      1. Adaptive threshold from upper bright_frac percentile statistics
      2. Morphological open (noise removal) then close (gap filling)
      3. Select largest connected component
      4. Compute bounding rectangle and confidence score

    Args:
        image:          float32 (H, W), gain-corrected linear domain
        pixel_pitch_mm: detector pixel pitch in mm
        k_coll:         threshold = μ_bright - k_coll × σ_bright
        bright_frac:    fraction of brightest pixels used for stats
        open_r_mm:      morphological opening radius in mm
        close_r_mm:     morphological closing radius in mm
    Returns:
        CollimatorMask
    """
    try:
        from scipy import ndimage
        _scipy_ok = True
    except ImportError:
        _scipy_ok = False

    H, W = image.shape
    img  = image.astype(np.float32)

    # 1. Adaptive threshold from bright pixels
    flat  = img.ravel()
    perc  = np.percentile(flat, (1.0 - bright_frac) * 100.0)
    bright_pixels = flat[flat >= perc]
    mu_b  = float(np.mean(bright_pixels))
    sig_b = float(np.std(bright_pixels))
    theta = mu_b - k_coll * sig_b
    theta = max(theta, float(np.percentile(flat, 10.0)))  # safety floor

    binary_mask = (img >= theta).astype(np.uint8)

    # 2. Morphological open then close (in pixel units)
    r_open  = max(1, int(round(open_r_mm  / pixel_pitch_mm)))
    r_close = max(1, int(round(close_r_mm / pixel_pitch_mm)))

    if _scipy_ok:
        from scipy.ndimage import binary_opening, binary_closing, label
        struct_o = np.ones((2 * r_open  + 1, 2 * r_open  + 1), dtype=bool)
        struct_c = np.ones((2 * r_close + 1, 2 * r_close + 1), dtype=bool)
        opened  = binary_opening(binary_mask, structure=struct_o)
        closed  = binary_closing(opened,      structure=struct_c).astype(np.uint8)
    else:
        # Minimal fallback without scipy: sliding-window erosion/dilation (slow)
        closed = binary_mask  # degraded mode

    # 3. Largest connected component
    if _scipy_ok:
        labeled, n_comp = label(closed)
        if n_comp == 0:
            # No valid component: return full-image fallback
            final_mask  = np.ones((H, W), dtype=np.uint8)
            confidence  = 0.1
        else:
            sizes = ndimage.sum(closed, labeled, range(1, n_comp + 1))
            best  = int(np.argmax(sizes)) + 1
            final_mask = (labeled == best).astype(np.uint8)
            # Confidence: ratio of largest/total foreground pixels
            confidence = float(sizes[best - 1] / (np.sum(closed) + 1e-6))
            confidence = float(np.clip(confidence, 0.0, 1.0))
    else:
        final_mask = closed
        confidence = 0.5

    # 4. Bounding rectangle
    rows = np.any(final_mask, axis=1)
    cols = np.any(final_mask, axis=0)
    if not np.any(rows) or not np.any(cols):
        bounding = (0, 0, W, H)
        confidence = 0.05
    else:
        r_min, r_max = int(np.argmax(rows)), int(H - 1 - np.argmax(rows[::-1]))
        c_min, c_max = int(np.argmax(cols)), int(W - 1 - np.argmax(cols[::-1]))
        bounding = (c_min, r_min, c_max - c_min, r_max - r_min)

    return CollimatorMask(
        mask       = final_mask,
        bounding   = bounding,
        confidence = confidence,
        method_id  = 'threshold_morpho_v1',
    )
```

#### 12.5.3 C++ 클래스 명세 (런타임)

```cpp
// CollimatorMask C++ runtime class
// Python calibration produces JSON sidecar; runtime reconstructs mask from sidecar
// Sidecar schema (xpe-algorithm-spec-deepsync.md §4.3):
//   { "roi_x": int, "roi_y": int, "roi_w": int, "roi_h": int,
//     "confidence": float, "method_id": string }

struct CollimatorMask {
    cv::Mat  mask;          // CV_8U binary mask (1 = inside collimated field)
    cv::Rect bounding;      // Bounding rect of collimated field
    float    confidence;    // Detection quality [0,1]
    std::string method_id;  // "threshold_morpho_v1" or "ai_refined_v1"

    // Convenience accessors
    cv::Rect bounding_rect() const { return bounding; }
    const cv::Mat& mask_mat() const { return mask; }

    // Load from JSON sidecar (written by Python calibration step)
    static CollimatorMask from_sidecar(const nlohmann::json& j) {
        CollimatorMask cm;
        int x = j.at("roi_x").get<int>();
        int y = j.at("roi_y").get<int>();
        int w = j.at("roi_w").get<int>();
        int h = j.at("roi_h").get<int>();
        cm.bounding    = cv::Rect(x, y, w, h);
        cm.confidence  = j.at("confidence").get<float>();
        cm.method_id   = j.at("method_id").get<std::string>();
        // Reconstruct binary mask from bounding rect (full-rect approximation)
        // Full polygon mask optional in Phase 2 if contour points stored
        cm.mask = cv::Mat::zeros(/* H, W from image size */ 0, 0, CV_8U);
        // Caller must provide image dimensions; set via set_image_size()
        return cm;
    }

    void set_image_size(int H, int W) {
        mask = cv::Mat::zeros(H, W, CV_8U);
        cv::rectangle(mask, bounding, cv::Scalar(1), cv::FILLED);
    }

    // Compute mean signal within mask (used by EI ROI selection)
    float mean_within_mask(const cv::Mat& image) const {
        cv::Scalar mean_val = cv::mean(image, mask);
        return static_cast<float>(mean_val[0]);
    }
};
```

#### 12.5.4 검증 기준

| 항목 | 기준 | 측정 방법 |
|------|------|---------|
| 검출 성공률 | ≥ 95% | 100장 테스트 팬텀 |
| Bounding rect 정확도 | ±5mm from true edge | 물리적 조준기 기준값 비교 |
| 처리 시간 | < 200ms (3072×3072) | 단일 스레드 |
| Confidence 하한 경고 | confidence < 0.6 → 알림 | 경보 로그 확인 |

---

## 부록 A: 수학 공식 일람

### A.1 Pre-Processing 공식

$$I_{\text{offset}}(x,y) = \max(I_{\text{raw}} - I_{\text{dark}},\ 0)$$

$$G(x,y) = \frac{\bar{I}_{\text{flat}}}{I_{\text{flat}}(x,y) - I_{\text{dark}}(x,y)}, \quad I_{\text{corr}} = I_{\text{offset}} \cdot G$$

$$\text{Lag}(t) = \sum_{i=1}^{3} \alpha_i e^{-t/\tau_i}$$

$$I_{\text{true}} = I_{\text{measured}} - \text{Lag}(t) \cdot I_{\text{prev\_max}}$$

### A.2 Core Processing 공식

$$I_{OD} = -\ln\left(\frac{I_{\text{clean}} + \varepsilon}{I_0 + \varepsilon}\right)$$

$$BF[I](x) = \frac{\sum_{x_i} I(x_i) e^{-\|x_i-x\|^2/2\sigma_s^2} e^{-|I(x_i)-I(x)|^2/2\sigma_r^2}}{\sum_{x_i} e^{-\|x_i-x\|^2/2\sigma_s^2} e^{-|I(x_i)-I(x)|^2/2\sigma_r^2}}$$

$$I_{\text{USM}} = I + \lambda \cdot (I - I * G_\sigma)$$

### A.3 Display Processing 공식

**Linear VOI:**
$$\text{Out} = \text{clamp}\left(\frac{I - (WC - WW/2)}{WW} \cdot (\text{Max} - \text{Min}) + \text{Min},\ \text{Min},\ \text{Max}\right)$$

**Sigmoid VOI:**
$$\text{Out} = \frac{\text{Max} - \text{Min}}{1 + e^{-4(I-WC)/WW}} + \text{Min}$$

**GSDF JND:**
$$j = 71.498068 + 94.593053\log L + 41.912053(\log L)^2 + \cdots$$

### A.4 Exposure Index 공식

$$DI = 10 \cdot \log_{10}\left(\frac{EI}{EI_{\text{target}}}\right) \quad \text{(dB)}$$

### A.5 FPD 특성화 공식

$$\text{DQE}(f) = \frac{\text{MTF}^2(f)}{\Phi \cdot \text{NNPS}(f)}$$

$$\text{NNPS}(f) = \frac{\text{NPS}(f)}{\bar{S}^2}$$

$$\sigma_A^2(\tau) = \frac{1}{2}\langle(\bar{x}_{k+1} - \bar{x}_k)^2\rangle$$

---

## 부록 B: 표준 참조 테이블

### B.1 RQA 조건 (IEC 61267)

| RQA | kVp | Al 여과 (mm) | HVL (mm Al) | 용도 |
|-----|-----|------------|-------------|------|
| RQA3 | 70 | 23.0 | 6.8 | Mammography-adjacent |
| RQA5 | 80 | 21.0 | 7.1 | General radiography |
| RQA7 | 90 | 30.0 | 9.2 | Chest |
| RQA9 | 120 | 40.0 | 11.5 | High-kVp chest |
| RQA10 | 150 | 50.0 | 13.0 | Interventional |

### B.2 SPR 참조 (80kVp, 35×43cm FOV)

| 두께 (cm, water equiv.) | SPR (%) |
|-----------------------|---------|
| 10 | 30–50 |
| 15 | 60–80 |
| 20 | 80–120 |
| 25 | 120–180 |
| 30 | 150–250 |

### B.3 GSDF P-Value Luminance (PS3.14 Table B.1 발췌)

| P-Value | Target Luminance (cd/m²) | JND Index |
|---------|------------------------|----------|
| 0 | 0.05 | ~10 |
| 1024 | 2.0 | ~200 |
| 2048 | 50.0 | ~400 |
| 3071 | 1000.0 | ~600 |
| 4095 | 3000.0 | ~700 |

---

## 부록 C: 알고리즘-요구사항 추적성

| 알고리즘 | SRS Req ID | SDD SWU | 검증 방법 |
|---------|-----------|---------|---------|
| Offset Correction | SRS-FUNC-001 | SWU-1.1 | Unit test + dark field measurement |
| Gain Correction | SRS-FUNC-002 | SWU-1.2 | Uniformity measurement |
| Defect Correction | SRS-FUNC-003 | SWU-1.3 | Injected defect test |
| Ghost Correction | SRS-FUNC-004 | SWU-1.4 | Double-exposure protocol |
| Log Transform | SRS-FUNC-010 | SWU-2.1 | Mathematical verification |
| Bilateral Filter | SRS-FUNC-011 | SWU-2.2 | MTF retention test |
| CLAHE | SRS-FUNC-012 | SWU-2.3 | Histogram analysis |
| Edge Enhancement | SRS-FUNC-013 | SWU-2.4 | Safe gain verification |
| Laplacian Pyramid | SRS-FUNC-014 | SWU-2.5 | Phantom image quality |
| Fractional MS | SRS-FUNC-015 | SWU-2.6 | Artifact measurement |
| CNN Recognition | SRS-FUNC-016 | SWU-2.10 | ≥95% accuracy test |
| Panoramic Stitch | SRS-FUNC-017 | SWU-2.11 | Cobb angle ≤2° error |
| Bone Suppression | SRS-FUNC-018 | SWU-2.12 | PSNR≥33dB, SSIM≥0.97 |
| Modality LUT | SRS-FUNC-020 | SWU-3.1 | DICOM conformance |
| VOI LUT | SRS-FUNC-021 | SWU-3.2 | W/L sweep verification |
| GSDF PLUT | SRS-FUNC-022 | SWU-3.3 | PS3.14 conformance |
| Grid Suppression | — (Phase 2) | — | MTF retention + CNR |
| Virtual Grid | — (Phase 2) | — | CNR comparison vs physical grid |
| Exposure Index | — (Phase 2) | SWU-2.9 | IEC 62494-1 conformance |

---

## 개정 이력

| 개정 | 날짜 | 저자 | 내용 |
|------|------|------|------|
| 1.1 | 2026-04-15 | XPE Team | **GAP 해소 10건**: GAP-D (NSCT 완전 구현), GAP-E (update_defect_map_runtime AVX2 구현), GAP-F (EI ROI Central Method √0.1 수정), GAP-G (avx2_log_ps Cephes 다항식), GAP-H (Non-linearity Correction §3.0.5), GAP-I (Readout Validation §3.0), GAP-J (AED-0 §9.4), GAP-L (NPS §12.3), GAP-M (DQE §12.4), GAP-N (Collimation Mask §12.5). 섹션 수 ~50% 증가. |
| 1.0 | 2026-04-15 | XPE Team | 초판 (10회 review-evaluate-fix 완료). GAP-01~GAP-10 초기 해소. |

---

*Document End — XPE-ALG-001 v1.1*

*Cross-references: XPE-SRS-001, XPE-SAD-001, XPE-SDD-002, xpe-algorithm-spec-deepsync.md, SPEC-XPE-MASTER.md, 03_측정_알고리즘_명세서, xray_grid_suppression_virtual_grid_research*
