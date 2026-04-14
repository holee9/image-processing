# XPE Implementation Reference

**Document ID**: XPE-IMPL-REF-001
**Version**: 1.1.0
**Date**: 2026-04-14
**Purpose**: Sprint 개발자가 질문 없이 구현할 수 있도록 api-spec, sprint-plan, pipeline-spec에서 누락된 상세 명세를 제공
**Source Documents**: api-spec.md v1.3.0, sprint-plan.md v1.2.0, pipeline-spec.md v1.4.0, SPEC-XPE-MASTER v2.0.0, xpe-algorithm-spec-deepsync.md v3.0.0-ds2
**Changelog**: v1.0.0→v1.1.0: (1) 추가 §9 Logging Output Format — plain-text 줄 형식 + Alert JSON 스키마. (2) 추가 §10 LUT 파일 형식 — DICOM 기반 바이너리 레이아웃. (3) 추가 §11 GSDF 컴플라이언스 검증 공식. (4) 추가 §12 AI Worker IPC 프로토콜 — Named Pipe 메시지 형식. (5) 추가 §13 DL 모델 양자화 명세. (6) 추가 §14 Calibration session_id 형식.

---

## 1. Calibration File Binary Format (GAP G1)

### 1.1 File Extension

| Extension | Format |
|-----------|--------|
| `.xpe_calib` | Native XPE calibration binary |
| `.dcm` | DICOM calibration image (load via DCMTK) |

### 1.2 Binary Layout (`.xpe_calib`)

All multi-byte fields are **little-endian**. Struct packing: `#pragma pack(push, 1)`.

```
Offset  Size  Field             Type         Description
------  ----  ----------------  -----------  ----------------------------------------
0       4     magic             char[4]      "XPEC" (0x58 0x50 0x45 0x43)
4       2     version           uint16_t     Format version. Current = 1.
6       4     width             uint32_t     Image width in pixels.
10      4     height            uint32_t     Image height in pixels.
14      2     pixelFormat       uint16_t     0 = uint16, 1 = float32.
16      1     calibType         uint8_t      0 = offset (dark), 1 = gain (flat), 2 = defect map.
17      1     reserved          uint8_t      Zero. Reserved for future use.
18      8     expiryEpochMs     uint64_t     Expiry timestamp (UNIX epoch ms). 0 = never expires.
26      4     crc32             uint32_t     CRC-32 of data[] only (header excluded).
30      4     dataLength        uint32_t     Byte length of data[].
34      var   data              uint8_t[]    Pixel data. Row-major, no padding between rows.

Total header size: 34 bytes.
```

### 1.3 CRC-32 Computation

- Algorithm: CRC-32 (ISO 3309 / ITU-T V.42, polynomial 0xEDB88320 reflected)
- Scope: `data[]` bytes only. Header is NOT included in CRC.
- Verification: On load, compute CRC of read data and compare with stored `crc32`. Mismatch → `XPE_ERR_IO_FAILED`.

### 1.4 Data Layout by calibType

| calibType | pixelFormat | data content |
|:---------:|:-----------:|-------------|
| 0 (offset) | 0 (uint16) | Per-pixel dark offset values. Same dimensions as detector. |
| 1 (gain) | 1 (float32) | Per-pixel gain normalization factors. Unity = 1.0f. |
| 2 (defect) | 0 (uint16) | Bad pixel map. Non-zero = defect pixel. Values encode defect type: 1=dead, 2=hot, 3=cluster. |

### 1.5 Expiry Handling

- `expiryEpochMs = 0`: Calibration never expires.
- `expiryEpochMs < current_time_ms`: Load returns `XPE_ERR_CALIBRATION_EXPIRED`.
- `expiryEpochMs - current_time_ms < 86400000` (within 24 hours): Load succeeds but alert is emitted (calibration expiring soon).

### 1.6 File Size Validation

On load, verify: `dataLength == width * height * bytesPerPixel(pixelFormat)`.
- uint16: `bytesPerPixel = 2`
- float32: `bytesPerPixel = 4`

If mismatch → `XPE_ERR_IO_FAILED`.

---

## 2. JSON Configuration Schemas (GAP G2)

All JSON fields are optional unless noted. Unknown keys are silently ignored (forward-compatible).

### 2.1 xpe_init Config

```json
{
  "logLevel": 2,
  "logFile": "xpe_runtime.log",
  "threadPoolSize": 0,
  "maxImageDimension": 4096,
  "preprocess": {
    "readout_validation": { "enabled": true },
    "temp_compensation": { "enabled": true },
    "nonlinearity": { "enabled": true },
    "binning": { "enabled": true },
    "defect_correction": { "enabled": true, "runtime_detection": false },
    "ghost_correction": { "enabled": true, "max_tier": 3 }
  }
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| logLevel | int | 2 (INFO) | 0-5 | 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF |
| logFile | string | null (stderr) | valid file path | UTF-8 path, append mode |
| threadPoolSize | int | 0 (auto) | 0-32 | 0 = hardware_concurrency - 1 |
| maxImageDimension | int | 4096 | 256-8192 | Maximum width or height |

### 2.2 xpe_aed_configure Config

```json
{
  "trigger_threshold_adu": 500,
  "settle_time_ms": 100,
  "min_exposure_ms": 5,
  "max_exposure_ms": 5000,
  "signal_hysteresis_pct": 10
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| trigger_threshold_adu | int | 500 | 100-65535 | ADU level to detect exposure start |
| settle_time_ms | int | 100 | 10-1000 | Consecutive signal above threshold required |
| min_exposure_ms | int | 5 | 1-100 | Minimum exposure duration to register event |
| max_exposure_ms | int | 5000 | 100-60000 | Timeout for exposure end detection |
| signal_hysteresis_pct | int | 10 | 0-50 | Percentage drop from peak to detect exposure end |

### 2.3 xpe_defect_correct Config

```json
{
  "interpolation_mode": "bilinear",
  "cluster_max_size": 5,
  "runtime_detection": {
    "enabled": false,
    "hot_pixel_threshold_sigma": 5.0,
    "dead_pixel_threshold_adu": 10
  }
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| interpolation_mode | string | "bilinear" | "nearest", "bilinear", "median" | Algorithm for pixel replacement |
| cluster_max_size | int | 5 | 1-10 | Maximum cluster dimension. Larger clusters use surrounding border. |
| runtime_detection.enabled | bool | false | — | Enable transient defect detection |
| runtime_detection.hot_pixel_threshold_sigma | float | 5.0 | 3.0-10.0 | Standard deviations above local mean |
| runtime_detection.dead_pixel_threshold_adu | int | 10 | 0-100 | ADU threshold below which pixel is dead |

### 2.4 xpe_ghost_create Config

```json
{
  "max_tier": 3,
  "history_depth": 8,
  "irf": {
    "n_exponentials": 4,
    "amplitudes": [0.015, 0.008, 0.003, 0.001],
    "time_constants_ms": [50, 200, 1000, 5000]
  },
  "tier_thresholds": {
    "tier1_residual": 0.005,
    "tier2_residual": 0.0035,
    "tier3_residual": 0.0029
  },
  "bypass_single_shot": true,
  "min_history_frames": 1
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| max_tier | int | 3 | 1-3 | Maximum correction tier to attempt |
| history_depth | int | 8 | 2-16 | Ring buffer depth for exposure history |
| irf.n_exponentials | int | 4 | 2-8 | Number of exponential terms in IRF |
| irf.amplitudes | float[] | [0.015, 0.008, 0.003, 0.001] | 0.0-0.1 | Relative amplitude of each exponential |
| irf.time_constants_ms | float[] | [50, 200, 1000, 5000] | 10-100000 | Decay time constant per component |
| tier_thresholds.tier1_residual | float | 0.005 | 0.001-0.05 | Residual above this triggers Tier 2 |
| tier_thresholds.tier2_residual | float | 0.0035 | 0.001-0.05 | Residual above this triggers Tier 3 |
| tier_thresholds.tier3_residual | float | 0.0029 | 0.001-0.05 | Target residual for Tier 3 |
| bypass_single_shot | bool | true | — | Auto-bypass on first frame after reset |
| min_history_frames | int | 1 | 0-8 | Minimum history entries before correction |

### 2.5 xpe_noise_reduce Config

```json
{
  "method": "bilateral",
  "strength": 0.5,
  "spatial_sigma": 3.0,
  "range_sigma": 0.1,
  "nlm_patch_size": 7,
  "nlm_search_window": 21
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| method | string | "bilateral" | "bilateral", "nlm" | Noise reduction algorithm |
| strength | float | 0.5 | 0.0-1.0 | Overall strength multiplier |
| spatial_sigma | float | 3.0 | 0.5-10.0 | Bilateral spatial kernel sigma (pixels) |
| range_sigma | float | 0.1 | 0.01-1.0 | Bilateral range kernel sigma (normalized) |
| nlm_patch_size | int | 7 | 3-15 (odd) | NLM patch size (NLM mode only) |
| nlm_search_window | int | 21 | 5-35 (odd) | NLM search window size (NLM mode only) |

### 2.6 xpe_contrast_enhance Config

```json
{
  "method": "clahe",
  "clip_limit": 2.0,
  "tile_grid_size": 8
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| method | string | "clahe" | "clahe" | Contrast enhancement algorithm |
| clip_limit | float | 2.0 | 0.5-10.0 | CLAHE clip limit (higher = more contrast) |
| tile_grid_size | int | 8 | 2-32 | Grid size for contextual regions |

### 2.7 xpe_edge_enhance Config

```json
{
  "method": "usm",
  "strength": 0.5,
  "radius": 1.5,
  "threshold": 0
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| method | string | "usm" | "usm" | Unsharp masking |
| strength | float | 0.5 | 0.0-2.0 | Sharpening strength (0 = no sharpening) |
| radius | float | 1.5 | 0.5-5.0 | Gaussian blur radius for mask |
| threshold | int | 0 | 0-255 | Minimum brightness change to sharpen |

### 2.8 xpe_log_transform Config

```json
{
  "base": 10.0,
  "offset": 1.0
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| base | float | 10.0 | 2.0-100.0 | Logarithm base |
| offset | float | 1.0 | 0.0-1000.0 | Added before log to avoid log(0) |

Formula: `output = log_base(input + offset) / log_base(maxValue + offset)`

### 2.9 xpe_temp_compensate Config

```json
{
  "model": "exponential",
  "nominal_temp_c": 25.0,
  "tolerance_c": 2.0,
  "coefficients": {
    "I0": 100.0,
    "Eg_eV": 1.12
  }
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| model | string | "exponential" | "exponential", "lut" | Temperature compensation model |
| nominal_temp_c | float | 25.0 | 15.0-40.0 | Reference temperature for calibration |
| tolerance_c | float | 2.0 | 0.5-5.0 | Bypass if abs(T - nominal) <= tolerance |
| coefficients.I0 | float | 100.0 | >0 | Dark current prefactor |
| coefficients.Eg_eV | float | 1.12 | 1.0-1.5 | Semiconductor band gap energy |

### 2.10 xpe_nonlinearity_correct Config

```json
{
  "model": "polynomial",
  "order": 3,
  "coefficients": [0.0, 1.0, -1e-7, 5e-14],
  "bypass_if_linear_profile": true
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| model | string | "polynomial" | "polynomial", "lut" | Nonlinearity correction model |
| order | int | 3 | 1-5 | Polynomial order |
| coefficients | float[] | [0, 1, 0, 0] | any | Polynomial coefficients c0 + c1*x + c2*x^2 + ... |
| bypass_if_linear_profile | bool | true | — | Auto-bypass when panel config declares linear response |

### 2.11 xpe_binning_correct Config

```json
{
  "mode": 2,
  "normalization": "area"
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| mode | int | 1 | 1-4 | Binning mode: 1=1x1, 2=2x2, 3=3x3, 4=4x4 |
| normalization | string | "area" | "area", "average" | area = divide by mode^2, average = no division |

### 2.12 xpe_multiscale_process Config

```json
{
  "method": "laplacian_pyramid",
  "levels": 4,
  "band_coefficients": [1.0, 1.2, 1.5, 1.0, 0.8]
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| method | string | "laplacian_pyramid" | "laplacian_pyramid" | Decomposition method |
| levels | int | 4 | 2-6 | Number of decomposition levels |
| band_coefficients | float[] | all 1.0 | 0.0-3.0 | Per-band enhancement. Length = levels + 1. 1.0 = identity. |

### 2.13 xpe_detect_collimation Config

```json
{
  "method": "gradient_hough",
  "downscale_factor": 2,
  "edge_threshold": 50,
  "min_line_length_ratio": 0.3,
  "iou_fallback_threshold": 0.5
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| method | string | "gradient_hough" | "gradient_hough" | Detection algorithm |
| downscale_factor | int | 2 | 1-4 | Downscale before detection (performance) |
| edge_threshold | int | 50 | 10-200 | Minimum gradient magnitude for edge |
| min_line_length_ratio | float | 0.3 | 0.1-0.8 | Minimum line length as ratio of image dimension |
| iou_fallback_threshold | float | 0.5 | 0.1-1.0 | Below this IoU, fall back to full image bounds |

### 2.14 xpe_dl_denoise Config

```json
{
  "model_variant": "auto",
  "strength": 1.0,
  "tile_size": 512,
  "tile_overlap": 32
}
```

| Field | Type | Default | Range | Notes |
|-------|------|---------|-------|-------|
| model_variant | string | "auto" | "auto", "chest_low", "chest_high", "extremity" | Model selection. "auto" uses bodyPart + mAs. |
| strength | float | 1.0 | 0.0-2.0 | Output blending with input (0 = no denoising) |
| tile_size | int | 512 | 256-1024 | Tile size for large image processing |
| tile_overlap | int | 32 | 16-64 | Overlap between tiles to avoid seam artifacts |

---

## 3. Body Part to EIT Mapping (GAP G3)

Exposure Index Target (EIT) values for IEC 62494-1 DI calculation. DI = 10 * log10(EI / EIT).

Source: AAPM TG-232 (Exposure Indicator Standardization), IEC 62494-1.

| bodyPart String | EIT (μGy) | Exam Type | Notes |
|----------------|:---------:|-----------|-------|
| `CHEST` | 2.5 | Chest PA/AP | Default adult chest |
| `CHEST_LAT` | 5.0 | Chest Lateral | Higher exposure for lateral |
| `ABDOMEN` | 3.0 | Abdomen AP | — |
| `PELVIS` | 3.0 | Pelvis AP | — |
| `SPINE` | 3.0 | Spine AP/PA | Thoracic/Lumbar |
| `SPINE_LAT` | 6.0 | Spine Lateral | Higher exposure for lateral |
| `NECK` | 2.0 | Cervical spine AP | — |
| `SKULL` | 2.0 | Skull PA/AP | — |
| `SHOULDER` | 2.0 | Shoulder AP | — |
| `CLAVICLE` | 1.5 | Clavicle AP | — |
| `HAND` | 1.0 | Hand PA | Extremity |
| `WRIST` | 1.0 | Wrist PA | Extremity |
| `FOOT` | 1.0 | Foot AP/Oblique | Extremity |
| `KNEE` | 1.5 | Knee AP/PA | — |
| `ELBOW` | 1.0 | Elbow AP/Lat | — |
| `ANKLE` | 1.0 | Ankle AP/Lat | — |
| `DENTAL` | 1.0 | Dental intraoral | Periapical/bitewing |
| `MAMMO` | 1.5 | Mammography CC/MLO | — |
| `PEDIATRIC` | 0.5 | Pediatric (any) | Default reduced exposure |
| `GENERAL` | 2.0 | Unspecified | Fallback for unknown body parts |

**Unknown body part handling**: If `bodyPart` is empty or not in the table, use `GENERAL` EIT = 2.0 μGy.

**Case sensitivity**: Body part strings are compared case-insensitively ("Chest" == "CHEST" == "chest").

---

## 4. Ghost IRF Default Parameters (GAP G4)

### 4.1 N=4 Exponential Impulse Response Function

Model: `h(t) = Σ(n=1..4) aₙ * exp(-t / τₙ)`

Default values for **indirect-conversion a-Si flat panel detector** at clinical dose:

| Component (n) | Amplitude (aₙ) | Time Constant (τₙ) | Fraction (%) | Physical Origin |
|:---:|:---:|:---:|:---:|---|
| 1 | 0.015 | 50 ms | 50.0 | Shallow trap release (prompt) |
| 2 | 0.008 | 200 ms | 26.7 | Medium-depth trap release |
| 3 | 0.003 | 1000 ms | 10.0 | Deep trap release |
| 4 | 0.001 | 5000 ms | 3.3 | Very deep trap release |

**Total first-frame lag (uncorrected)**: ~3.0% of original signal

### 4.2 Tier Performance Targets

| Tier | Algorithm | Target Residual | Time Budget |
|:---:|-----------|:---:|:---:|
| Tier 1 | LTI deconvolution (N=4, fixed coefficients) | < 0.5% | 150 ms |
| Tier 2 | Exposure-weighted LTI (intensity-matched coefficients) | < 0.35% | +40 ms (190 total) |
| Tier 3 | NLCSC (signal-dependent coefficients) | <= 0.29% | +90 ms (240 total) |

### 4.3 Escalation Logic

```python
# Pseudocode for auto-escalation
residual = compute_residual(corrected_image, reference)

if tier == 1 and residual > tier1_threshold:  # default: 0.005
    escalate to tier 2
elif tier == 2 and residual > tier2_threshold:  # default: 0.0035
    escalate to tier 3
elif tier == 3 and residual > tier3_target:  # default: 0.0029
    log_warning("NLCSC target not met. Best effort applied.")
    # Do NOT escalate further. Emit alert with actual residual.
```

### 4.4 Memory Budget

Exposure history ring buffer:
- Per frame: 3072 × 3072 × 4 bytes (float32) = 37.7 MB
- Default depth: 8 frames → 301.6 MB
- Configurable: `history_depth` in ghost config (2-16 range)

### 4.5 Calibration Requirement

The default parameters above are starting estimates. Production calibration procedure:
1. Acquire 100 dark frames at clinical kVp
2. Acquire 100 flat-field frames
3. Measure lag by analyzing sequential frame differences
4. Fit N=4 exponential model to measured lag curve
5. Store fitted parameters in ghost config JSON

Source: Starman et al. 2012, Med Phys (PMC3465354)

---

## 5. DICOM Tag to Metadata Mapping (GAP G5)

### 5.1 XpeImageMetadata Field Mapping

| XpeImageMetadata Field | DICOM Tag | VR | Source | Default if Absent |
|---|---|---|---|---|
| `bodyPart[64]` | (0018,0015) Body Part Examined | CS | Module: General | `""` (empty string) |
| `kVp` | (0018,0060) KVP | DS | Module: X-Ray | `0.0f` |
| `mAs` | Computed: (0018,1152) Exposure / (0018,1150) Exposure Time * 1000 | IS | Module: X-Ray | `0.0f` |
| `SID_mm` | (0018,1110) Distance Source to Detector | DS | Module: X-Ray | `1000.0f` |
| `pixelPitch_mm` | (0018,1164) Imager Pixel Spacing[0] | DS | Module: Detector | `0.139f` (139 μm) |
| `acquisitionTime` | (0008,002A) Acquisition DateTime → parsed to UNIX ms | DT | Module: General | `0` (epoch) |
| `flags` | Computed from pipeline state | — | — | `0x00000000` |

### 5.2 XpeImageBuffer Field Mapping

| XpeImageBuffer Field | DICOM Tag | VR | Notes |
|---|---|---|---|
| `width` | (0028,0011) Columns | US | — |
| `height` | (0028,0010) Rows | US | — |
| `bitsAllocated` | (0028,0100) Bits Allocated | US | Typically 16 |
| `bitsStored` | (0028,0101) Bits Stored | US | Typically 14 |
| `format` | Derived from bitsAllocated | — | 16 → XPE_PIXEL_UINT16 |
| `dataSize` | Computed: width * height * (bitsAllocated/8) | — | — |

### 5.3 Tags Read for Pipeline Decisions

| DICOM Tag | Purpose | Used By |
|---|---|---|
| (0018,1164) Imager Pixel Spacing | Binning detection (if spacing != expected) | xpe_binning_correct |
| (0018,0060) KVP | Exposure parameter routing | xpe_gain_correct (multi-gain) |
| (0018,1152) Exposure | mAs derivation for AI model selection | xpe_dl_denoise |
| (0018,0015) Body Part Examined | EI EIT selection, LUT auto-select | xpe_calc_exposure_index, xpe_lut_auto_select |
| (0028,1050) Window Center | Default W/L initialization | Orchestrator (C#) |
| (0028,1051) Window Width | Default W/L initialization | Orchestrator (C#) |
| (0028,1052) Rescale Intercept | Modality LUT parameter | xpe_modality_lut_apply |
| (0028,1053) Rescale Slope | Modality LUT parameter | xpe_modality_lut_apply |

### 5.4 Tags Written by xpe_dicom_write

When writing DICOM output, the following tags are populated:

| DICOM Tag | Source | VR |
|---|---|---|
| (0008,0016) SOP Class UID | "1.2.840.10008.5.1.4.1.1.1.1" (DX Image Storage) | UI |
| (0008,0018) SOP Instance UID | Generated (UUID-based) | UI |
| (0008,0020) Study Date | From acquisitionTime | DA |
| (0008,0030) Study Time | From acquisitionTime | TM |
| (0018,0015) Body Part Examined | From bodyPart | CS |
| (0018,0060) KVP | From kVp | DS |
| (0018,1110) Distance Source to Detector | From SID_mm | DS |
| (0018,1164) Imager Pixel Spacing | From pixelPitch_mm | DS |
| (0028,0010) Rows | From height | US |
| (0028,0011) Columns | From width | US |
| (0028,0100) Bits Allocated | From bitsAllocated | US |
| (0028,0101) Bits Stored | From bitsStored | US |
| (0028,0102) High Bit | bitsStored - 1 | US |
| (0028,0103) Pixel Representation | 0 (unsigned) | US |

---

## 6. Performance Measurement Methodology (GAP G6)

### 6.1 Standard Benchmark Protocol

| Parameter | Value |
|---|---|
| Test image size | 3072 × 3072 (default) |
| Warm-up iterations | 10 (excluded from measurement) |
| Measurement iterations | 100 |
| Reported statistics | median, p95, min, max |
| Build configuration | Release (/O2, no sanitizers, no coverage) |
| SIMD | AVX2 enabled (if available). Report SIMD vs scalar delta. |
| Thread model | Single-threaded unless function explicitly uses ThreadPool |
| Memory | Pre-allocated buffers. Exclude allocation time. |
| Platform | Windows 11 x64 |

### 6.2 Benchmark Test Template

```cpp
// Standard benchmark pattern for all XPE functions
void benchmark_xpe_function(const char* name, XpeImageBuffer& img) {
    // Warm-up
    for (int i = 0; i < 10; i++) {
        call_function_under_test(img);
    }

    // Measurement
    std::vector<double> times;
    times.reserve(100);
    for (int i = 0; i < 100; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        call_function_under_test(img);
        auto end = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    std::sort(times.begin(), times.end());
    double median = times[50];
    double p95 = times[95];
    double min_val = times[0];
    double max_val = times[99];

    printf("[PERF] %s: median=%.2fms, p95=%.2fms, min=%.2fms, max=%.2fms\n",
           name, median, p95, min_val, max_val);
}
```

### 6.3 Performance Gate Thresholds

| Metric | Gate | Action |
|---|---|---|
| median > budget | FAIL | Sprint cannot complete. Optimize. |
| p95 > 2x budget | WARNING | Investigate outliers. |
| max > 3x budget | FAIL | Unacceptable tail latency. |

### 6.4 Phase-Level Integration Benchmark

End-of-phase integration tests use the following protocol:

| Phase | Image | Iterations | Metric |
|---|---|---|---|
| Phase 1a | 3072x3072 synthetic raw | 10 | Full pre-processing pipeline < 500ms (median) |
| Phase 1b | DICOM → full pipeline → DICOM | 10 | End-to-end < 3000ms (median) |
| Phase 2 | Phase 1 + advanced + GSVG | 10 | Total < 2500ms (median) |
| Phase 3 | Phase 1+2 + AI inference | 10 | Total < 3000ms (median) |

### 6.5 Memory Leak Test Protocol

```
Process 1000 frames through full pipeline.
Record peak memory after frame 1 and frame 1000.
Growth = peak_1000 - peak_1.
Threshold: Growth < 1 MB (tolerance for OS jitter).
```

---

## 7. AED State Machine (GAP G7)

### 7.1 State Diagram

```
                    xpe_init() or
                    xpe_aed_configure(NULL)
                          |
                          v
                    +---------+
            +------>|   IDLE  |
            |       +---------+
            |            |
            |            | xpe_aed_configure(valid_config)
            |            v
            |       +---------+
            |       |  ARMED  |<-----------------------+
            |       +---------+                        |
            |            |                             |
            |            | Signal > threshold          |
            |            | for > settle_time_ms         |
            |            v                             |
            |       +-----------+                     |
            |       | TRIGGERED |----- Signal drops ---+
            |       +-----------+     below threshold
            |            |           for > settle_time_ms
            |            |             (exposure_end event)
            |            |
            |            | xpe_aed_reset() or
            |            | exposure history cleared
            |            v
            |       +---------+
            +-------|  ARMED  |  (re-arm)
                    +---------+

State transitions to IDLE:
  - xpe_shutdown()
  - xpe_aed_configure(NULL)
```

### 7.2 State Definitions

| State | ID | Description | Events Generated |
|---|:---:|---|---|
| IDLE | 0 | AED not configured or shut down | None |
| ARMED | 1 | Configured and monitoring for exposure | None |
| TRIGGERED | 2 | Exposure detected, signal above threshold | exposure_start (on entry), exposure_end (on exit) |

### 7.3 Event Types

| Event Type | ID | Trigger | Data |
|---|:---:|---|---|
| exposure_start | 0 | Entering TRIGGERED state | timestamp, initial signal level |
| exposure_end | 1 | Leaving TRIGGERED state (signal drops) | timestamp, peak signal level |
| exposure_trigger | 2 | Peak signal detected during TRIGGERED | timestamp, peak signal level |

### 7.4 Thread Safety

- `xpe_aed_configure`: Not thread-safe. Call during initialization.
- `xpe_aed_poll_event`: Thread-safe. Uses internal mutex.
- `xpe_aed_get_status`: Thread-safe. Atomic read.

---

## 8. Pipeline Error Recovery Policy (GAP G8)

### 8.1 Error Classification

| Category | Symbol | Description | Example |
|---|:---:|---|---|
| FATAL | `F` | Pipeline cannot continue. Abort frame. | Mandatory stage missing calibration data |
| RECOVERABLE | `R` | Skip stage, set alert, continue. | Optional stage processing failure |
| DEGRADABLE | `D` | Fall back to simpler algorithm, continue. | AI model unavailable → classical fallback |
| INFORMATIONAL | `I` | Log and continue. No quality impact. | Advisory stage bypass |

### 8.2 Per-Stage Error Policy

| Stage | Category | Error Condition | Recovery Action | Alert? |
|---|:---:|---|---|---|
| (0) CalibManager | F | Mandatory map missing | ABORT startup, return error | No (hard fail) |
| (0.5) Readout | I | Validation fails | Flag + alert only. Continue pipeline. | Yes |
| (0.5) Readout | F | artifactScore > CRITICAL_THRESHOLD | ABORT frame. Possible detector failure. | Yes (critical) |
| (0.7) Temp | R | Sensor unavailable | Use nominal 25C, emit degradation alert | Yes |
| (1) Offset | F | offsetMap NULL | ABORT frame. Mandatory correction. | No (hard fail) |
| (1.5) Nonlinearity | R | Config bypass enabled | Skip stage. XPE_FLAG not set. | No |
| (2) Gain | F | gainMap NULL | ABORT frame. Format boundary required. | No (hard fail) |
| (2.5) Binning | R | Mode=1 (native) | No-op. XPE_FLAG not set. | No |
| (3) Defect | R | BPM empty | Skip correction. XPE_FLAG not set. | No |
| (3) Defect | I | Runtime detection disabled | Static BPM only | No |
| (4) Ghost | R | First frame / no history | Skip correction. XPE_FLAG not set. | No |
| (4) Ghost | D | Tier 3 target not met | Best-effort result + warning alert | Yes |
| (5) Log Transform | F | All-zero image | ABORT frame. log(0) undefined. | No |
| (6) Noise Reduce | D | NLM timeout | Fall back to bilateral | Yes |
| (7) Contrast | D | CLAHE internal error | Skip enhancement | Yes |
| (8) Edge Enhance | R | USM kernel failure | Skip sharpening | Yes |
| (9) GSVG | R | Any GSVG error | SAFE-003: return original buffer | Yes (XPE_FLAG_GSVG_SKIPPED) |
| (10-11) Advanced | R | Processing error | Skip stage | Yes |
| (12) Stitching | R | No overlap detected | Return first image only | Yes |
| (13) Bone Suppression | D | AI model unavailable | Skip entirely (DL_DISABLED) | No |
| (14-16) Display LUT | F | Invalid LUT parameters | ABORT frame. | No |
| (17) DICOM Write | F | I/O error | ABORT frame. Return XPE_ERR_IO_FAILED | No |

### 8.3 Orchestrator Error Handling Flow

```csharp
// Pseudocode for PipelineOrchestrator error handling
XpeErrorCode RunPipeline(XpeImageBuffer img, XpeImageMetadata meta) {
    // Stage execution with error policy
    var result = RunStage(Stage.Offset, img, offsetMap);
    if (result.IsFatal) return result.Code;     // ABORT
    if (result.IsRecoverable) { /* skip, alert */ }

    result = RunStage(Stage.Gain, img, gainMap);
    if (result.IsFatal) return result.Code;     // ABORT

    // ... continue for each stage ...

    result = RunStage(Stage.GSVG, img, gsvgConfig);
    if (result.IsRecoverable) {
        // SAFE-003: original buffer already preserved by GSVG
        meta.flags |= XPE_FLAG_GSVG_SKIPPED;
        EmitAlert($"GSVG skipped: {result.Message}");
    }

    // ... AI stages are degradable ...
    result = RunStage(Stage.BoneSuppression, img, aiConfig);
    if (result.IsDegradable) {
        // Fallback: skip bone suppression
        EmitAlert($"Bone suppression skipped: {result.Message}");
    }

    return XPE_OK;  // Pipeline completed (possibly with degraded stages)
}
```

### 8.4 Alert Severity Levels

| Severity | ID | Meaning | Example |
|---|:---:|---|---|
| INFO | 0 | Informational. No action needed. | Calibration expires in 7 days. |
| WARN | 1 | Quality may be reduced. Review recommended. | Temperature compensation using nominal value. |
| ERROR | 2 | Stage skipped or degraded. Investigate. | GSVG processing failed. |
| CRITICAL | 3 | Possible detector or system failure. Immediate attention. | Readout artifact score exceeds critical threshold. |

---

## 6. GUI 경로 영속성 설계 (ImageProcTest Path Persistence)

### 6.1 appsettings.json 스키마

ImageProcTest GUI는 다음 경로들을 `appsettings.json`에 저장하여 사용자의 마지막 선택을 기억합니다:

```json
{
  "paths": {
    "lastImageDir": "C:\\clinical\\data",
    "lastDicomDir": "C:\\clinical\\dicom",
    "calibrationDirs": {
      "offsetDir": "C:\\calib\\offset",
      "gainDir": "C:\\calib\\gain",
      "defectDir": "C:\\calib\\defect"
    },
    "gsvgLutDir": "C:\\calib\\gsvg"
  },
  "recentFiles": [
    "C:\\clinical\\data\\patient_001.dcm",
    "C:\\clinical\\data\\patient_002.dcm"
  ]
}
```

### 6.2 구현 패턴 (C#)

```csharp
using System.Text.Json;

public class GuiPathSettings
{
    private string _settingsPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "ImageProcTest",
        "appsettings.json"
    );
    
    public class PathConfig
    {
        public string LastImageDir { get; set; } = "C:\\";
        public string LastDicomDir { get; set; } = "C:\\";
        public CalibDirs CalibrationDirs { get; set; } = new();
        public List<string> RecentFiles { get; set; } = new();
    }
    
    public class CalibDirs
    {
        public string OffsetDir { get; set; } = "C:\\";
        public string GainDir { get; set; } = "C:\\";
        public string DefectDir { get; set; } = "C:\\";
    }
    
    private PathConfig _config;
    
    public GuiPathSettings()
    {
        Load();
    }
    
    public void Load()
    {
        try
        {
            if (File.Exists(_settingsPath))
            {
                var json = File.ReadAllText(_settingsPath);
                var root = JsonDocument.Parse(json);
                var pathsElement = root.RootElement.GetProperty("paths");
                
                _config = new PathConfig
                {
                    LastImageDir = pathsElement.GetProperty("lastImageDir").GetString() ?? "C:\\",
                    LastDicomDir = pathsElement.GetProperty("lastDicomDir").GetString() ?? "C:\\",
                };
            }
            else
            {
                _config = new PathConfig();
            }
        }
        catch
        {
            _config = new PathConfig();
        }
    }
    
    public void Save()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath));
        
        var config = new
        {
            paths = new
            {
                lastImageDir = _config.LastImageDir,
                lastDicomDir = _config.LastDicomDir,
                calibrationDirs = new
                {
                    offsetDir = _config.CalibrationDirs.OffsetDir,
                    gainDir = _config.CalibrationDirs.GainDir,
                    defectDir = _config.CalibrationDirs.DefectDir
                }
            },
            recentFiles = _config.RecentFiles
        };
        
        var json = JsonSerializer.Serialize(config, new JsonSerializerOptions 
        { 
            WriteIndented = true 
        });
        
        File.WriteAllText(_settingsPath, json);
    }
    
    // 파일 다이얼로그에 사용
    public void UpdateLastImageDir(string path)
    {
        _config.LastImageDir = Path.GetDirectoryName(path) ?? "C:\\";
        Save();
    }
    
    public void AddRecentFile(string path)
    {
        _config.RecentFiles.Remove(path);  // 중복 제거
        _config.RecentFiles.Insert(0, path);  // 맨 앞에 추가
        if (_config.RecentFiles.Count > 10)  // 최대 10개 유지
            _config.RecentFiles.RemoveAt(_config.RecentFiles.Count - 1);
        Save();
    }
}
```

### 6.3 WPF UI 통합

```csharp
public partial class MainWindow : Window
{
    private GuiPathSettings _pathSettings;
    
    public MainWindow()
    {
        InitializeComponent();
        _pathSettings = new GuiPathSettings();
    }
    
    private void LoadImageButton_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Load Raw X-ray Image",
            Filter = "Raw Image (*.raw)|*.raw|DICOM (*.dcm)|*.dcm|All Files (*.*)|*.*",
            InitialDirectory = _pathSettings.LastImageDir  // ← 마지막 경로 기억
        };
        
        if (dialog.ShowDialog() == true)
        {
            _pathSettings.UpdateLastImageDir(dialog.FileName);  // ← 새 경로 저장
            ProcessImage(dialog.FileName);
        }
    }
    
    private void CalibrationSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new FolderBrowserDialog
        {
            Description = "Select Calibration Data Directory",
            SelectedPath = _pathSettings._config.CalibrationDirs.OffsetDir
        };
        
        if (dialog.ShowDialog() == DialogResult.OK)
        {
            _pathSettings._config.CalibrationDirs.OffsetDir = dialog.SelectedPath;
            _pathSettings.Save();
            StatusLabel.Text = $"Calibration path updated: {dialog.SelectedPath}";
        }
    }
}
```

---

## Appendix A: Parameter Range Quick Reference

Body-part-specific parameter ranges returned by `xpe_get_param_range()`:

| Parameter | Body Part | Min | Max | Default |
|---|---|---|---|---|
| noiseReductionStrength | CHEST | 0.0 | 1.0 | 0.5 |
| noiseReductionStrength | HAND | 0.0 | 0.5 | 0.2 |
| noiseReductionStrength | ABDOMEN | 0.0 | 1.0 | 0.4 |
| contrastStrength | CHEST | 0.5 | 3.0 | 2.0 |
| contrastStrength | HAND | 0.5 | 2.0 | 1.0 |
| edgeEnhanceStrength | CHEST | 0.0 | 1.5 | 0.5 |
| edgeEnhanceStrength | BONE | 0.0 | 2.0 | 1.0 |
| ghostCorrectionTier | * | 1 | 3 | 3 |
| defectInterpolationMode | * | 0 | 2 | 1 (bilinear) |

Note: `*` = same for all body parts. This table provides baseline defaults; implementation should allow JSON overrides.

---

---

## 9. Logging Output Format (GAP-C4)

### 9.1 Log Line Format (Plain Text)

모든 `xpe_log_*` 함수는 spdlog를 통해 다음 포맷으로 출력한다.

```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [TID] message
```

| 필드 | 형식 | 예 |
|---|---|---|
| timestamp | ISO 8601 로컬 타임, 밀리초 포함 | `2026-04-14 09:23:45.123` |
| LEVEL | 5글자 고정폭 대문자 | `TRACE`, `DEBUG`, ` INFO`, ` WARN`, `ERROR` |
| TID | OS 스레드 ID, 5자리 0-패딩 | `00042` |
| message | 자유 형식 문자열, 개행 없음 | `xpe_init: config loaded OK` |

예:
```
[2026-04-14 09:23:45.123] [ INFO] [00042] xpe_init: config loaded OK
[2026-04-14 09:23:45.456] [ WARN] [00042] CalibManager: expiry in 3600000 ms
[2026-04-14 09:23:46.789] [ERROR] [00099] GSVG: processing failed code=-5
```

### 9.2 Log Level 값

| 정수 | 심벌 | 설명 |
|:---:|---|---|
| 0 | TRACE | 저수준 진단 (디버그 빌드에만 권장) |
| 1 | DEBUG | 내부 알고리즘 상태 |
| 2 | INFO | 정상 동작 이정표 |
| 3 | WARN | 품질 저하 가능 (Alert 큐에도 발행) |
| 4 | ERROR | 스테이지 스킵/실패 (Alert 큐에도 발행) |
| 5 | OFF | 로그 비활성화 |

### 9.3 Alert JSON 스키마 (GAP-D5)

`xpe_get_pending_alert()`가 반환하는 `msg` 버퍼에 기록되는 JSON:

```json
{
  "severity": 2,
  "code": "GSVG_PROCESSING_FAILED",
  "message": "GSVG internal error: scatter LUT not loaded",
  "timestamp_ms": 1744588800000,
  "stage_id": 9,
  "stage_name": "GSVG",
  "frame_index": 42,
  "details": {}
}
```

| 필드 | 타입 | 필수 | 설명 |
|---|---|:---:|---|
| `severity` | int | ✓ | 0=INFO, 1=WARN, 2=ERROR, 3=CRITICAL |
| `code` | string | ✓ | SCREAMING_SNAKE_CASE 고유 코드 |
| `message` | string | ✓ | 사람이 읽을 수 있는 설명 |
| `timestamp_ms` | uint64 | ✓ | UNIX epoch 밀리초 |
| `stage_id` | int | ✓ | 파이프라인 스테이지 번호 (-1 = 파이프라인 외부) |
| `stage_name` | string | ✓ | 스테이지 이름 |
| `frame_index` | uint32 | ✓ | 현재 프레임 카운터 (0-based) |
| `details` | object | — | 스테이지별 추가 진단 정보 |

**정의된 Alert 코드**:

| code | severity | 발생 조건 |
|---|:---:|---|
| `CALIB_EXPIRING_SOON` | WARN | 만료 24시간 이내 |
| `CALIB_EXPIRED` | ERROR | 만료됨 |
| `GHOST_TIER_DOWNGRADED` | WARN | Tier 3 목표 미달, 최선 결과 사용 |
| `GSVG_PROCESSING_FAILED` | ERROR | GSVG SAFE-003 경로 진입 |
| `TEMP_SENSOR_UNAVAILABLE` | WARN | 온도 보상에 공칭값 25°C 사용 |
| `READOUT_ARTIFACT_CRITICAL` | CRITICAL | artifactScore > CRITICAL_THRESHOLD |
| `AI_MODEL_UNAVAILABLE` | WARN | DL 모델 미로드, 스테이지 스킵 |
| `PIPELINE_STAGE_SKIPPED` | INFO | 선택적 스테이지 바이패스 |
| `PARAM_RANGE_CLAMPED` | INFO | 요청값이 허용 범위를 초과하여 클램핑 |

### 9.4 JSON 로그 출력 (옵션)

`xpe_init` config에서 `"logFormat": "json"` 설정 시 각 줄이 다음 JSON으로 출력됨:

```json
{"ts":"2026-04-14T09:23:45.123Z","lvl":"INFO","tid":42,"msg":"xpe_init: config loaded OK"}
```

---

## 10. LUT 파일 형식 (.lut) (GAP-D6)

디스플레이 파이프라인(스테이지 14-16)에서 사용되는 LUT 파일 형식.

### 10.1 파일 확장자

| 확장자 | 용도 |
|---|---|
| `.lut` | XPE 네이티브 LUT (이 섹션에서 정의) |
| `.dcm` | DICOM 내장 LUT (0028,3010 VOI LUT Sequence) |

### 10.2 바이너리 레이아웃 (.lut)

엔디언: **little-endian**. 패킹: `#pragma pack(push, 1)`.

```
Offset  Size  Field           Type        설명
------  ----  --------------  ----------  ----------------------------------------
0       4     magic           char[4]     "XLUT" (0x58 0x4C 0x55 0x54)
4       2     version         uint16_t    포맷 버전. 현재 = 1.
6       2     lutType         uint16_t    0=Modality, 1=VOI, 2=Presentation
8       4     numEntries      uint32_t    LUT 엔트리 수 (256, 1024, 4096, 65536)
12      2     inputBits       uint16_t    입력 비트 깊이 (e.g., 12, 14, 16)
14      2     outputBits      uint16_t    출력 비트 깊이 (e.g., 8, 10, 12)
16      4     firstStored     int32_t     첫 저장 픽셀 값 (DICOM (0028,3002)[1])
20      4     crc32           uint32_t    data[] 의 CRC-32
24      var   data            uint16_t[]  LUT 값 배열. 크기 = numEntries * 2 bytes.

총 헤더 크기: 24 bytes.
```

### 10.3 LUT 타입별 사용 스테이지

| lutType | 값 | 적용 스테이지 | DICOM 태그 |
|---|:---:|---|---|
| Modality | 0 | (14) `xpe_modality_lut_apply` | (0028,3000) |
| VOI | 1 | (15) `xpe_voi_lut_apply` | (0028,3010) |
| Presentation | 2 | (16) `xpe_presentation_lut_apply` | (2050,0010) |

### 10.4 검증 규칙

- `numEntries` ∈ {256, 1024, 4096, 65536}; 기타 값 → `XPE_ERR_CONFIG_INVALID`
- `data[i]` 값은 단조 비감소(monotonically non-decreasing)이어야 함
- CRC-32 불일치 → `XPE_ERR_IO_FAILED`

---

## 11. GSDF 컴플라이언스 검증 공식 (GAP-D10)

DICOM Part 14 GSDF(Grayscale Standard Display Function) 준수 검증 절차.

### 11.1 GSDF JND 인덱스 참조값 (18 레벨)

| 레벨 | JND Index | 최소 휘도 (cd/m²) | 최대 휘도 (cd/m²) |
|:---:|:---:|:---:|:---:|
| L0 | 1.0 | 0.05 | 0.08 |
| L1 | 10.0 | 0.45 | 0.60 |
| L2 | 50.0 | 2.60 | 3.20 |
| L3 | 100.0 | 7.00 | 8.50 |
| L4 | 200.0 | 19.0 | 23.0 |
| L5 | 300.0 | 38.0 | 46.0 |
| L6 | 400.0 | 65.0 | 79.0 |
| L7 | 500.0 | 101 | 123 |
| L8 | 600.0 | 148 | 180 |
| L9 | 700.0 | 207 | 252 |
| L10 | 800.0 | 281 | 342 |
| L11 | 900.0 | 371 | 451 |
| L12 | 1000.0 | 479 | 583 |
| L13 | 1500.0 | 950 | 1050 |
| L14 | 2000.0 | 1250 | 1400 |
| L15 | 2500.0 | 1550 | 1700 |
| L16 | 3000.0 | 1750 | 1950 |
| L17 | 4000.0 | 2100 | 2400 |

### 11.2 합격 기준

합격: **모든 18개 레벨**에서 측정 휘도가 ±10% 이내:

```
|L_measured(i) - L_gsdf(i)| / L_gsdf(i) <= 0.10
```

한 레벨이라도 초과 시 불합격 → 디스플레이 캘리브레이션 필요.

### 11.3 검증 절차 (QA Constancy Test)

1. `xpe_display_calibrate_gsdf()` 호출하여 측광계 연결
2. 각 JND 레벨(L0..L17)에 해당하는 픽셀값 출력
3. 측광계로 실제 휘도(cd/m²) 측정
4. 합격 기준 적용 → `XPE_OK` 또는 `XPE_ERR_DISPLAY_CALIBRATION_REQUIRED`
5. 결과를 `QaConstancyTestResult` JSON으로 저장

---

## 12. AI Worker IPC 프로토콜 (GAP-D8)

Phase 3 `xpe_ai.dll`이 별도 프로세스(`xpe_ai_worker.exe`)와 통신하는 방법.

### 12.1 채널

| 항목 | 값 |
|---|---|
| 메커니즘 | Windows Named Pipe (`\\.\pipe\xpe_ai_worker_{PID}`) |
| 버퍼 크기 | 64 KB (제어 메시지) |
| 이미지 데이터 | Named Shared Memory (`Local\xpe_ai_shm_{PID}_{FrameID}`) |
| 최대 대기 | 50 ms per 메시지 왕복 |

### 12.2 메시지 프레임 형식

```
[LEN: 4 bytes little-endian uint32][TYPE: 4 bytes ASCII][BODY: LEN bytes UTF-8 JSON]
```

### 12.3 메시지 타입

| TYPE (4 bytes) | 방향 | 설명 |
|---|---|---|
| `INFT` | DLL → Worker | 추론 요청 |
| `RSPN` | Worker → DLL | 추론 결과 |
| `PING` | DLL → Worker | 생존 확인 |
| `PONG` | Worker → DLL | 생존 확인 응답 |
| `KILL` | DLL → Worker | 정상 종료 요청 |
| `ERRO` | Worker → DLL | 오류 보고 |

### 12.4 추론 요청 (INFT) JSON

```json
{
  "frame_id": 42,
  "task": "bone_suppression",
  "shm_name": "Local\\xpe_ai_shm_1234_42",
  "width": 3072,
  "height": 3072,
  "format": "float32",
  "body_part": "CHEST",
  "config": {}
}
```

### 12.5 추론 결과 (RSPN) JSON

```json
{
  "frame_id": 42,
  "status": "ok",
  "shm_name": "Local\\xpe_ai_shm_1234_42_out",
  "latency_ms": 35,
  "model_version": "bone_sup_v1.2.0"
}
```

### 12.6 크래시 복구 정책

1. DLL은 Worker 프로세스를 `CreateProcess()`로 생성
2. PING/PONG으로 50 ms 간격 생존 확인
3. PONG 없음 3회 연속 → Worker 크래시로 판단
4. `TerminateProcess()` 후 새 Worker 프로세스 생성 (최대 3회 재시도)
5. 재시도 실패 시 `xpe_ai.dll` 비활성화 + `AI_MODEL_UNAVAILABLE` 알림

---

## 13. DL 모델 양자화 명세 (GAP-D7)

Phase 3 ONNX 모델의 INT8 양자화 상세 명세.

### 13.1 대상 모델

| 모델 | 구조 | 용도 | 파라미터 |
|---|---|---|---|
| BoneSuppression | U-Net (EfficientNet-B2 인코더) | 뼈 조직 억제 | ~28M FP32 / ~7M INT8 |
| DLDenoiser | DnCNN (17-layer) | 딥러닝 노이즈 제거 | ~556K FP32 / ~139K INT8 |
| BodyPartRecognizer | EfficientNet-B0 | 신체 부위 분류 | ~5.3M FP32 / ~1.3M INT8 |

### 13.2 양자화 기법

| 항목 | 값 |
|---|---|
| 방법 | Post-Training Quantization (PTQ) — Static |
| 정밀도 | INT8 (Weight + Activation) |
| 캘리브레이션 데이터셋 | 신체 부위별 200장 임상 영상 (15 카테고리 × 200 = 3,000장) |
| 캘리브레이션 알고리즘 | MinMax + Entropy (Histogram) |
| 도구 | ONNX Runtime Quantization Tool (`onnxruntime.quantization`) |

### 13.3 합격 기준

| 메트릭 | 목표 |
|---|---|
| PSNR 손실 (FP32 vs INT8) | < 2 dB |
| 분류 정확도 손실 | < 1% |
| 추론 속도 향상 | ≥ 2x (FP32 대비) |
| 모델 크기 감소 | ≥ 70% |

### 13.4 모델 파일 배포

```
data/models/
  bone_suppression_int8.onnx      (≤ 8 MB)
  dl_denoiser_int8.onnx           (≤ 1 MB)
  body_part_recognizer_int8.onnx  (≤ 2 MB)
```

---

## 14. Calibration session_id 형식 (GAP-C10)

캘리브레이션 데이터 세션 식별자 및 검증 로직.

### 14.1 session_id 형식

```
{detector_serial}_{YYYYMMDD}_{calibType}_{sha256_prefix8}
```

| 파트 | 형식 | 예 |
|---|---|---|
| `detector_serial` | 영문/숫자 최대 16자 | `AU1234567` |
| `YYYYMMDD` | 캘리브레이션 수행 날짜 | `20260414` |
| `calibType` | `offset` \| `gain` \| `defect` | `offset` |
| `sha256_prefix8` | 데이터 SHA-256의 앞 8 hex 문자 | `a3f2b1c9` |

전체 예: `AU1234567_20260414_offset_a3f2b1c9`

### 14.2 생성 알고리즘

```c
// session_id 생성 (CalibManager 내부)
void generate_session_id(
    const char* detector_serial,
    const uint8_t* data, size_t data_len,
    uint8_t calib_type,
    char* out_session_id, size_t buf_size)
{
    uint8_t sha256[32];
    SHA256(data, data_len, sha256);

    char date_str[9];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(date_str, sizeof(date_str), "%Y%m%d", t);

    const char* type_names[] = {"offset", "gain", "defect"};
    snprintf(out_session_id, buf_size,
        "%.16s_%s_%s_%02x%02x%02x%02x",
        detector_serial, date_str, type_names[calib_type],
        sha256[0], sha256[1], sha256[2], sha256[3]);
}
```

### 14.3 검증 규칙

1. 정규식: `^[A-Za-z0-9]{1,16}_\d{8}_(offset|gain|defect)_[0-9a-f]{8}$`
2. 날짜 유효성: YYYYMMDD가 유효한 날짜인지 확인
3. SHA-256 일치: 파일 로드 시 데이터를 재계산하여 접두사 8자 일치 확인
4. 만료: `expiryEpochMs` 필드와 병행 사용 (session_id는 식별 전용)

### 14.4 다중 캘리브레이션 세트 관리

동일 detector에 대해 여러 날짜의 세트를 보관할 때:

```json
{
  "active_sessions": {
    "offset": "AU1234567_20260414_offset_a3f2b1c9",
    "gain":   "AU1234567_20260414_gain_f1e2d3c4",
    "defect": "AU1234567_20260401_defect_b5a69871"
  }
}
```

- 각 calibType별 독립적으로 최신 세션 관리
- `defect` 세션 날짜가 `offset`/`gain`보다 오래될 수 있음 (BPM은 덜 자주 갱신)

---

*Document End -- XPE-IMPL-REF-001 v1.1.0*
