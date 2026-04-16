# Software Requirements Specification - XPE Preprocessing Calibration Module

**Document ID:** SRS-CALIB-001 v1.0  
**IEC 62304 Clause:** 5.2 (Software Requirements Specification)  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Calibration Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

### 1.1 Purpose

This Software Requirements Specification (SRS) defines all functional, safety, performance, and interface requirements for the XPE Preprocessing Calibration Module (`xpe_preprocess.dll`, Layer 1, Phase 1a). The module transforms raw detector data (uint16, 14-16 bit ADC output) into calibrated, corrected images (float32) by applying systematic corrections for physical detector artifacts including dark current offset, pixel gain variation, defective pixels, lag/ghosting, temperature drift, nonlinearity, and binning effects.

### 1.2 Scope

This document specifies requirements for calibration data management, correction algorithms, and quality assurance mechanisms required to meet IEC 62304 Class B medical device safety standards. The module is required for all clinical imaging workflows and mandatory for any system using the xpe_preprocess.dll interface.

**Out of scope:** Image enhancement processing (CLAHE, log transform, edge enhancement) is handled downstream in `xpe_enhance_basic.dll`. Grid suppression and virtual grid correction are handled in `gsvg.dll`.

---

## 2. Functional Requirements

### 2.1 Calibration Data Load Management (SRS-CALIB-FUNC-001 through SRS-CALIB-FUNC-003)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-001** | System shall load offset calibration file (.xpe_calib format) containing dark current map (uint16). File format: 34-byte header (magic=0x585045, version, timestamp, expiryEpochMs, reserved) + CRC-32 (4 bytes) + pixel data (3072×3072×2 bytes). CRC-32 validation via polynomial 0x04C11DB7 shall reject corrupted files with error `XPE_ERR_IO_FAILED`. | Offset map is essential baseline for all downstream corrections; CRC-32 protects against silent data corruption. Factory-calibrated maps have expiry epochs for compliance tracking. | Test: CRC validation, corruption detection |
| **SRS-CALIB-FUNC-002** | System shall load gain calibration file (.xpe_calib format) containing normalization factors (float32). File format: 34-byte header + CRC-32 + gain coefficients (3072×3072×4 bytes). Values shall be in range [0.1, 10.0]; out-of-range values shall trigger `XPE_ERR_INVALID_CALIB_DATA` error. | Gain map normalizes pixel-to-pixel sensitivity variation (FPN). Float32 enables multi-gain polynomial support. Range limits prevent over/under-correction artifacts. | Test: Range validation, file parsing |
| **SRS-CALIB-FUNC-003** | System shall load bad pixel map (BPM) from .xpe_calib file (uint8, 1 byte per pixel). BPM format: pixel value 0=good, 1-255=defect type (1=dead, 2=hot, 3=stuck, 4=noisy). Sparse map optimization supported via run-length encoding (RLE). Maximum 5% defect density tolerance. | BPM enables targeted defect correction without full-image filtering. RLE compression reduces memory footprint (typical 9.4MB to <500KB). Defect type field supports algorithmic selection. | Test: BPM parsing, RLE decompression |

### 2.2 Pixel-Level Corrections (SRS-CALIB-FUNC-004 through SRS-CALIB-FUNC-009)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-004** | System shall apply offset (dark current) correction: `I_corr(x,y) = I_raw(x,y) - I_dark(x,y)`. Negative results after offset subtraction shall be clamped to 0 (uint16 domain). Dynamic offset interpolation via bilinear temperature/PREP-time-dependent lookup shall be supported. | Dark current is primary readout artifact; offset correction must precede all other corrections. Clamping prevents wraparound. Dynamic compensation models time/temperature drift (exponential model: `I_dark(T) = I0 * exp(-Eg/2kB*T)`). | Test: Offset arithmetic, clamp verification |
| **SRS-CALIB-FUNC-005** | System shall apply gain (flat-field) correction: `I_norm(x,y) = I_corr(x,y) / G(x,y)`. Output shall be converted to float32 format. Gain values shall be validated to be non-zero; division by zero shall return `XPE_ERR_INVALID_CALIB_DATA`. Multi-gain mode with energy-dependent polynomial `G(x,y,E) = Σ(c_k × E^k)` shall be supported for SID-specific gain maps. Heel effect correction (Wang 2013 projection model) shall be supported. | Gain correction normalizes across detector's FPN and SID-dependent intensity falloff. Float32 conversion at this stage is mandatory (format boundary). Multi-gain supports clinical applications using multiple source positions. | Test: Gain arithmetic, format conversion |
| **SRS-CALIB-FUNC-006** | System shall apply nonlinearity correction using lookup table (LUT) or monotonic polynomial fitting before gain correction. Correction model: `I_lin(x,y) = f_nonlin(I_raw(x,y))` where `f_nonlin` is detector-specific and stored in calibration profile. LUT shall have minimum 256 entries; polynomial degree ≤ 5. Detailed algorithm specification in SRS-CALIB-FUNC-006-EXT below. | Detector response is non-linear due to charge trapping and fill factor effects. Correction must precede gain normalization (linearize before normalize). Detector profile governs enable/disable via field `panel.linear = true/false`. | Test: LUT lookup, polynomial evaluation, max residual ≤ 0.3% ADU |

**SRS-CALIB-FUNC-006-EXT: Nonlinearity Correction Algorithm Extension**

**6a. LUT Method (preferred for production)**

The nonlinearity LUT maps raw ADU values to linearized ADU values:

- LUT size: 4096 entries (covers 12-bit ADC range) or 65536 entries (16-bit full range)
- LUT data type: uint16 (output values in ADU)
- Lookup: `I_lin = LUT[I_raw]` (direct index, O(1))
- LUT generation (factory calibration procedure):
  1. Acquire flat-field images at N ≥ 10 dose levels spanning 5% to 95% ADC full scale
  2. For each dose level, record mean signal `S_meas` and reference dose `D_ref`
  3. Fit ideal linear response: `S_ideal(D) = G_nominal × D` where `G_nominal` is mean gain
  4. Compute correction: `LUT[S_meas] = S_ideal`
  5. Interpolate LUT entries between measured points using monotone cubic spline (Fritsch-Carlson 1980)
  6. Boundary conditions: `LUT[0] = 0`, `LUT[ADC_max] = ADC_max` (identity at extremes)
- Maximum interpolation error requirement: ≤ 0.3% of ADC full scale at any input value
- Monotonicity check: `LUT[i] ≤ LUT[i+1]` for all i (enforced; non-monotone LUT = `XPE_ERR_INVALID_CALIB_DATA`)

**6b. Polynomial Method (for embedded/FPGA use)**

The polynomial model uses a 4th-degree global polynomial fit:

```
I_lin(x,y) = c₀ + c₁ × I_raw + c₂ × I_raw² + c₃ × I_raw³ + c₄ × I_raw⁴
```

Evaluated using Horner's method to minimize arithmetic operations:

```
I_lin = c₀ + I_raw × (c₁ + I_raw × (c₂ + I_raw × (c₃ + I_raw × c₄)))
```

Polynomial fitting procedure:
1. Use the same N ≥ 10 dose-level flat-field dataset as LUT method
2. Fit via least-squares regression (numpy.polyfit or scipy.optimize.curve_fit)
3. Validate: maximum residual ≤ 0.5% ADC full scale across all measurement points
4. Enforce monotonicity in [0, ADC_max] by checking derivative root locations
5. If polynomial is non-monotone in operational range, reject and fallback to LUT method

**6c. Precision Comparison (LUT vs Polynomial)**

| Method | Interpolation error | Memory | Execution time | Platform |
|--------|-------------------|--------|----------------|----------|
| LUT (4096) | ≤ 0.30% | 8 KB | ~5 ns (cache hit) | CPU/FPGA |
| LUT (65536) | ≤ 0.01% | 128 KB | ~5 ns (L1 miss risk) | CPU only |
| Polynomial (deg 4) | ≤ 0.50% | <100 bytes | ~30 ns (5 MACs) | CPU/MCU/FPGA |
| Polynomial (deg 2) | ≤ 1.00% | <50 bytes | ~15 ns | MCU/FPGA |

Selection logic: If `panel.nonlinearity_mode == "LUT"` use 6a; if `"POLY"` use 6b; if `"AUTO"` select LUT for CPU targets, polynomial for MCU/FPGA targets (detected via `panel.target_platform` field in calibration profile).
| **SRS-CALIB-FUNC-007** | System shall apply defect pixel correction in three modes selectable via configuration: (a) neighbor averaging (4-neighbor or 8-neighbor), (b) bilinear interpolation from surrounding pixels, (c) median filter from neighborhood. Output shall replace defective pixels with interpolated values. Defect detection shall use Robust Mask Maker (RMM) with lambda=8.0 for runtime detection. | Defects manifest as dead pixels (no signal), hot pixels (excessive signal), and noisy pixels. Three interpolation modes provide trade-offs between speed and quality. RMM is robust to non-Gaussian noise. | Test: Interpolation correctness |
| **SRS-CALIB-FUNC-008** | System shall apply temperature compensation to dark current using exponential model: `I_dark_compensated(T) = I_dark(T_ref) × exp(-(E_g/2kB) × (1/T - 1/T_ref))`. Temperature input shall be from NTC thermistor sensor (0-50°C range). Compensation shall be bypassed if sensor unavailable or if |T - T_ref| ≤ 2°C (within tolerance). Reference temperature T_ref = 25°C (nominal). | Dark current doubles approximately every 6-8°C (intrinsic semiconductor physics). Temperature compensation prevents image drift between dark and clinical exposures. 2°C tolerance prevents false triggers. | Test: Exponential model accuracy, sensor integration |
| **SRS-CALIB-FUNC-009** | System shall check calibration expiry by comparing file timestamp (expiryEpochMs from header) against current time. If `current_time_ms > expiryEpochMs`, function shall return error `XPE_ERR_CALIBRATION_EXPIRED` and halt pipeline. Expiry date field shall be loaded from calibration file header (offset 8-11, uint32, milliseconds since 2000-01-01). | Expired calibration data introduces systematic bias in corrected images. Hard block prevents clinical use of stale calibration. Expiry mechanism enables IEC 62304 traceability and regulatory compliance (21 CFR Part 11). | Test: Date comparison logic |
| **SRS-CALIB-FUNC-010** | System shall support runtime defect detection via `xpe_defect_detect_runtime()` function to identify new defects not present in static BPM. Detection algorithm: calculate pixel SNR over N=10 consecutive frames; flag pixels with SNR < 5 dB as defects. Runtime map shall be merged with static BPM and logged for QA review. | Clinical systems develop new defects over time (cosmic rays, electrostatic discharge). Runtime detection prevents image artifacts from escaping undetected. | Test: SNR calculation, defect logging |

### 2.3 Session and Binning Management (SRS-CALIB-FUNC-011 through SRS-CALIB-FUNC-012)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-011** | System shall support calibration session management via `xpe_calib_session_create()` function. Session shall be identified by unique session_id (UUID v4). Each session shall include timestamp, detector temperature, source kVp/mAs, binning mode, and associated calibration file versions. Session state shall persist across frame processing until explicit reset via `xpe_ghost_reset()`. | Sessions enable tracking of calibration state across multi-frame acquisitions. Session metadata enables post-processing traceability (PACS integration). Ghost correction requires frame history (see SRS-CALIB-FUNC-012). | Test: Session ID generation, state persistence |
| **SRS-CALIB-FUNC-012** | System shall support pixel binning mode compensation via `xpe_binning_correct()`. Binning modes shall include 1×1 (native), 2×2, 4×4. Gain correction factor shall be applied as `G_binned(x,y) = G_native(x,y) / (binning_factor^2)` to maintain normalized intensity. Binning correction shall only execute if binningMode ≠ 1 (native resolution). Output bit-depth shall remain float32. | Binning (on-detector pixel summing) reduces noise but requires gain re-normalization. Squared factor accounts for charge summation. Conditional execution prevents unnecessary computation. | Test: Binning factor validation |

### 2.4 Ghost/Lag Correction and Frame History (SRS-CALIB-FUNC-013 through SRS-CALIB-FUNC-014)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-013** | System shall support three-tier ghost/lag correction with auto-escalation: (1) Tier 1: Multi-exponential LTI deconvolution (N=4 exponentials, model: `Lag(t) = Σ(α_i × exp(-t/τ_i))`); (2) Tier 2: Exposure-weighted LTI (accounts for variable exposure); (3) Tier 3: NLCSC (Non-Linear Correlation Signal Correction, Starman 2012, accounts for signal-dependent lag). Escalation shall occur when residual lag after Tier 1 exceeds threshold (10% of signal). Minimum 90% ghost removal shall be achieved. | Lag/ghosting is temporal artifact from charge carrier trapping. Three tiers provide accuracy vs. performance trade-off. Auto-escalation ensures best achievable image quality. NLCSC handles complex detector physics. | Test: Lag residual measurement |
| **SRS-CALIB-FUNC-014** | System shall maintain exposure history ring buffer with minimum 8 frames (configurable up to 16). Frame buffer storage: 8 frames × 3072×3072×4 bytes = ~150 MB. History shall be reset via `xpe_ghost_reset()` after patient/study change or power-on. First frame after reset shall skip ghost correction (no history). Single-shot mode shall also skip ghost correction. | Frame history enables temporal filtering for lag artifact removal. 8-frame buffer is standard for commercial FPD systems (Varex, Vieworks). Ring buffer prevents memory bloat. Reset prevents cross-contamination between acquisitions. | Test: Buffer management, reset logic |

---

## 3. Safety Requirements

### 3.1 Mandatory Correction Policy (SRS-CALIB-SAFE-001 through SRS-CALIB-SAFE-003)

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SRS-CALIB-SAFE-001** | Offset correction (SRS-CALIB-FUNC-004) and gain correction (SRS-CALIB-FUNC-005) shall be mandatory and non-bypassable. System shall return hard error `XPE_ERR_NOT_INITIALIZED` if either offsetMap or gainMap is absent at pipeline start. Defect, ghost, nonlinearity, binning, and temperature corrections are optional and conditional. | Dark current bias and pixel gain variation are inherent detector artifacts present in all frames. Uncorrected images contain systematic errors that compromise diagnostic accuracy. Mandatory flag prevents accidental misconfiguration. | Test: Bypass prevention, error codes |
| **SRS-CALIB-SAFE-002** | System shall enforce calibration expiry checking (SRS-CALIB-FUNC-009). Pipeline shall abort image acquisition if any loaded calibration file has expired (current_time_ms > expiryEpochMs). Error code `XPE_ERR_CALIBRATION_EXPIRED` shall be returned and propagated to user interface. | Expired calibration introduces known systematic bias into diagnostic images. Hard enforcement prevents clinical use of stale data and ensures regulatory compliance (21 CFR Part 11, IEC 62304). | Test: Expiry validation |
| **SRS-CALIB-SAFE-003** | All calibration file loads (offset, gain, BPM) shall validate file integrity via CRC-32 checksum. Corrupted files shall be rejected with error `XPE_ERR_IO_FAILED`. No partial corrections shall be applied. Pipeline shall fail atomically: either all calibration files load successfully, or none are loaded. | CRC-32 prevents silent data corruption that could lead to systematic image bias. Atomic behavior ensures consistency: either fully calibrated or raw pass-through. | Test: Corruption detection |

**Test GUI evaluation exception:** `ImageProcTest.exe` may expose `Off`, `On`, and `Auto` controls for each calibration/preprocessing stage to support algorithm effect and performance evaluation. This exception is limited to QA/Test GUI workflows, shall be labelled evaluation-only, shall not relax the product-mode mandatory offset/gain policy, and shall record every bypass or forced-stage decision in the automation/evidence report.

### 3.2 Data Integrity and Buffer Protection (SRS-CALIB-SAFE-004 through SRS-CALIB-SAFE-005)

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SRS-CALIB-SAFE-004** | Calibration correction shall never modify the input image buffer. All corrections shall operate in-place on working buffers or produce separate output buffers. Original uint16 input data shall remain accessible for diagnostic/audit purposes. | In-place modification risks data loss and prevents audit trail reconstruction. Preservation of original enables forensic analysis and QA verification. | Test: Input buffer preservation |
| **SRS-CALIB-SAFE-005** | Gain correction float32 output shall be bounds-checked to prevent overflow/underflow. Maximum pixel value after gain correction shall be capped at 3.4e38 (float32 max exponent). Pixels exceeding this shall be clamped to 3.4e38 with warning log. Minimum value shall be 0.0 (no negative corrected values). | Float32 overflow produces infinity/NaN which corrupt downstream processing. Clipping is preferable to NaN. Bounds checking prevents silent data corruption. | Test: Overflow protection |

---

## 4. Performance Requirements

### 4.1 Processing Speed (SRS-CALIB-PERF-001)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-001** | Total preprocessing pipeline execution time shall not exceed 500 ms per 3072×3072 float32 frame on Intel Core i7 or equivalent processor. Breakdown: (0) CalibManager load ≤200 ms (one-time startup), (1) Offset ≤55 ms, (1.5) Nonlinearity ≤20 ms, (2) Gain ≤55 ms, (2.5) Binning ≤10 ms, (3) Defect ≤95 ms, (4) Ghost Tier 1 ≤140 ms, (4) Ghost Tiers 2-3 ≤+130 ms. | Clinical workflow requires <1 second per frame (including enhancement). Detailed budgets prevent bottleneck phases from exceeding hard limit. Timer instrumentation shall log per-phase duration. | Test: Profiling on reference hardware |

### 4.2 Memory Requirements (SRS-CALIB-PERF-002)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-002** | Peak memory allocation shall not exceed 200 MB per frame processing pipeline. Breakdown: offset map (18.9 MB) + gain map (37.7 MB) + BPM (9.4 MB) + working buffer (37.7 MB) + ghost history (150 MB, optional) = max 190 MB. System shall free allocated memory after frame processing completes. No memory leaks during 100-frame batch processing. | Desktop console memory constraints (typical 4-8 GB, shared with UI). 200 MB limit ensures multi-frame processing without swapping. Batch processing validation ensures long-running acquisitions remain stable. | Test: Memory profiling, leak detection |

### 4.3 File I/O Performance (SRS-CALIB-PERF-003)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-003** | Calibration file load time (CalibManager initialization) shall not exceed 200 ms for all three files (offset, gain, BPM) on SSD. File seek time from disk to memory shall be optimized via sequential read (not random access). CRC-32 validation shall be incremental (calculated during read, not post-hoc). | Clinical workflows load calibration once at startup, not per-frame. 200 ms overhead is negligible in context of 500 ms per-frame budget. Sequential read + streaming CRC minimize I/O latency. | Test: Timed file loads |

---

## 5. Interface Requirements

### 5.1 Input Interfaces (SRS-CALIB-IF-001)

| Req ID | Interface | Input Type | Data Format | Constraints |
|--------|-----------|-----------|-------------|-------------|
| **SRS-CALIB-IF-001** | Raw image data | `XpeImageBuffer` struct (uint16) | 14-16 bit unsigned integer per pixel | 3072×3072 or 4096×4096 maximum |
| **SRS-CALIB-IF-002** | Calibration file paths | String paths (C ABI: `const char*`) | UTF-8 encoded file paths | Paths provided by C# orchestrator (ImageProcTest.exe) |
| **SRS-CALIB-IF-003** | Detector metadata | `XpeDetectorProfile` struct | JSON or binary struct (temperature, kVp, SID, binning mode) | Required for temperature compensation and gain selection |
| **SRS-CALIB-IF-004** | Configuration | JSON config file (xpe_preprocess_config.json) | Key-value pairs for bypass flags, algorithm parameters | Loaded at startup via `xpe_configure()` |

### 5.2 Output Interfaces (SRS-CALIB-IF-002)

| Req ID | Interface | Output Type | Data Format | Constraints |
|--------|-----------|-----------|-------------|-------------|
| **SRS-CALIB-IF-005** | Corrected image | `XpeImageBuffer` struct (float32) | 32-bit floating-point per pixel | Output after Gain correction (format boundary) |
| **SRS-CALIB-IF-006** | Status flags | `uint32_t flags` field (XpeImageMetadata) | Bitmask: `XPE_FLAG_READOUT_VALIDATED`, `XPE_FLAG_TEMP_COMPENSATED`, `XPE_FLAG_GAIN_CORRECTED`, `XPE_FLAG_DEFECT_CORRECTED`, `XPE_FLAG_GHOST_CORRECTED`, etc. | Flags indicate which corrections were applied |
| **SRS-CALIB-IF-007** | Error codes | `XpeErrorCode` enum (int32_t) | `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_CALIBRATION_EXPIRED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_CALIB_DATA` | Return value from correction functions |
| **SRS-CALIB-IF-008** | Diagnostic log | JSON log object (optional) | Key-value pairs: phase durations, bypass decisions, warnings, defect counts | Attached to `XpeImageMetadata.diagnosticLog` |

### 5.3 C ABI Function Signatures (SRS-CALIB-IF-003)

Core calibration functions shall be exported from `xpe_preprocess.dll` with C ABI (no name mangling):

```c
// Calibration Data Load
XpeErrorCode xpe_calib_load_offset(const char* filepath);
XpeErrorCode xpe_calib_load_gain(const char* filepath);
XpeErrorCode xpe_calib_load_defect_map(const char* filepath);

// Correction Functions
XpeErrorCode xpe_offset_correct(XpeImageBuffer* image);
XpeErrorCode xpe_gain_correct(XpeImageBuffer* image);  // Output: float32
XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* image);
XpeErrorCode xpe_defect_correct(XpeImageBuffer* image);
XpeErrorCode xpe_temp_compensate(XpeImageBuffer* image, float temp_celsius);
XpeErrorCode xpe_binning_correct(XpeImageBuffer* image, int binning_mode);

// Ghost/Lag Correction (Stateful)
XpeGhostHandle xpe_ghost_create(XpeGhostConfig* config);
XpeErrorCode xpe_ghost_correct(XpeGhostHandle handle, XpeImageBuffer* image);
XpeErrorCode xpe_ghost_reset(XpeGhostHandle handle);
void xpe_ghost_destroy(XpeGhostHandle handle);

// Configuration & Expiry
XpeErrorCode xpe_configure(const char* json_config);
XpeErrorCode xpe_calib_check_expiry(void);
```

### 5.4 Error Code Contract (SRS-CALIB-IF-004)

All functions return `XpeErrorCode` enum with following semantics:

| Error Code | Meaning | Recovery |
|-----------|---------|----------|
| `XPE_OK` (0) | Operation succeeded | Proceed to next stage |
| `XPE_ERR_NOT_INITIALIZED` (-1) | Calibration data not loaded | Load calibration and retry |
| `XPE_ERR_CALIBRATION_EXPIRED` (-2) | Calibration file timestamp > expiry | Update calibration file, abort acquisition |
| `XPE_ERR_IO_FAILED` (-3) | File CRC failed or read error | Check file integrity, reload |
| `XPE_ERR_INVALID_CALIB_DATA` (-4) | Calibration file format or value error | Regenerate calibration |
| `XPE_ERR_INVALID_PARAM` (-5) | Invalid function parameter (null pointer, wrong size) | Validate inputs before calling |

---

## 6. Non-Functional Requirements

### 6.1 Reliability and Robustness (SRS-CALIB-NFR-001 through SRS-CALIB-NFR-003)

| Req ID | Requirement | Verification |
|--------|------------|--------------|
| **SRS-CALIB-NFR-001** | All dynamic memory allocations shall check for null pointer returns. Allocation failures shall return `XPE_ERR_NOT_INITIALIZED` rather than crashing. No recursive allocations in hot path (per-frame corrections). | Test: Malloc interception, OOM simulation |
| **SRS-CALIB-NFR-002** | All buffer array accesses shall be bounds-checked. Array index out-of-bounds shall be caught and return `XPE_ERR_INVALID_PARAM` rather than causing buffer overflow. | Test: Fuzz testing with invalid indices |
| **SRS-CALIB-NFR-003** | Thread safety: All correction functions shall be reentrant and thread-safe when called with non-overlapping image buffers. Ghost correction functions (stateful) shall be protected by mutex per handle. | Test: Concurrent frame processing |

### 6.2 Determinism and Reproducibility (SRS-CALIB-NFR-004)

| Req ID | Requirement | Verification |
|--------|------------|--------------|
| **SRS-CALIB-NFR-004** | Identical input image + calibration files + configuration + detector metadata shall produce byte-identical output (deterministic). No floating-point rounding differences across runs. No random number generators in deterministic code paths. | Test: Multiple runs with same inputs, output hash comparison |

### 6.3 Regulatory Compliance (SRS-CALIB-NFR-005 through SRS-CALIB-NFR-006)

| Req ID | Requirement | Verification |
|--------|------------|--------------|
| **SRS-CALIB-NFR-005** | All calibration operations shall be audit-loggable. Decisions to bypass corrections, detected defects, expiry checks shall be logged to diagnostic JSON with timestamps. | Test: Log completeness audit |
| **SRS-CALIB-NFR-006** | System shall support IEC 62304 traceability: each frame shall carry metadata (frame_id, calibration_version, session_id, timestamp, applied_corrections, warnings) for post-acquisition review. | Test: Metadata tagging |

---

## 7. Requirement Verification Method

### 7.1 Verification Methods by Requirement Type

| Category | Verification Method | Examples |
|----------|-------------------|----------|
| **Functional Correctness** | Unit test + reference image comparison | SRS-CALIB-FUNC-001 to SRS-CALIB-FUNC-014: Compare corrected output to known-good reference using PSNR ≥ 40 dB |
| **Safety** | Code review + threat modeling | SRS-CALIB-SAFE-001 to SRS-CALIB-SAFE-005: Verify abort conditions, buffer overflow prevention, expiry enforcement |
| **Performance** | Profiling + benchmark suite | SRS-CALIB-PERF-001 to SRS-CALIB-PERF-003: Measure wall-clock time and memory usage on reference hardware |
| **Interface** | Integration test + API contract verification | SRS-CALIB-IF-001 to SRS-CALIB-IF-008: Test C ABI function signatures and return codes |
| **Robustness** | Fuzz testing + stress testing | SRS-CALIB-NFR-001 to SRS-CALIB-NFR-006: Test with malformed files, concurrent access, OOM conditions |

### 7.2 Test Specification References

All requirements shall have traceability to corresponding test cases in:
- `tests/calib/test_offset_correction.cpp`
- `tests/calib/test_gain_correction.cpp`
- `tests/calib/test_defect_correction.cpp`
- `tests/calib/test_ghost_correction.cpp`
- `tests/calib/test_calib_file_io.cpp`
- `tests/calib/test_safety_expiry.cpp`
- `tests/calib/perf_benchmark.cpp`

---

## References

### Standards and Regulations

| Reference | Relevance |
|-----------|-----------|
| IEC 62304:2006 + A1:2015 | Medical device software life cycle processes; Class B safety classification |
| IEC 62220-1-1:2015 | Measurement of the Detective Quantum Efficiency (DQE) of digital X-ray imaging detectors |
| ISO 14971:2019 | Medical devices — Application of risk management to the manufacture of medical devices |
| 21 CFR Part 11 | Electronic Records; Electronic Signatures (FDA) — calibration date/expiry tracking |

### Research Papers

| Citation | Topic | Application |
|----------|-------|-------------|
| Starman et al. (2012) | Signal-dependent lag correction in flat-panel detectors | SRS-CALIB-FUNC-013: Tier 3 NLCSC algorithm |
| Wang et al. (2013) | Heel effect correction in dual-SID imaging | SRS-CALIB-FUNC-005: Multi-gain correction |
| Ranger et al. (2014) | Characterization of lag in fluoroscopic FPD | SRS-CALIB-FUNC-008: Exponential dark current model |
| Pang et al. (2006) | Multi-exponential lag model | SRS-CALIB-FUNC-013: Tier 1 LTI deconvolution |
| FixPix (2023) | Deep learning defect correction (MLP) | SRS-CALIB-FUNC-007: Advanced defect mode |
| EP2148500A1 | Dynamic dark current compensation | SRS-CALIB-FUNC-004: Temperature/time interpolation |

### Project Documentation

| Document | Location |
|----------|----------|
| Calibration Module README | `docs/calibration/README.md` |
| X-ray FPD Algorithm Specification | `.moai/specs/xpe-algorithm-spec-deepsync.md` |
| Pipeline Architecture | `.moai/project/pipeline-spec.md` |
| Detector Profiles | `.moai/project/detector-profiles.json` |
| Calibration Data Format | `docs/calibration/FORMAT_SPEC.md` |

---

*SRS-CALIB-001 v1.0 — End of Document*
