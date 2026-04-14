# Software Architecture Document - XPE Preprocessing Calibration Module

**Document ID:** SAD-CALIB-001 v1.0  
**IEC 62304 Clause:** 5.3 (Software Architectural Design)  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Architecture Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

### 1.1 Purpose

This Software Architecture Document defines the structural design of the XPE Preprocessing Calibration Module (`xpe_preprocess.dll`). It specifies the decomposition of software requirements (from SRS-CALIB-001) into manageable software units (SWU), their responsibilities, interfaces, data flow, and inter-dependencies. The document establishes the foundation for implementation, integration testing, and change management.

### 1.2 Scope

This architecture applies to calibration-related software units only:
- CalibrationManager (central orchestration)
- Five mandatory correction units (Offset, Gain, Nonlinearity, Defect, Ghost)
- Supporting units (TempCompensator, BinningCorrector, RuntimeDefectDetector, SessionManager)

Enhancement processing (log transform, CLAHE, edge enhancement) is out of scope and handled by downstream modules. Grid processing is handled by `gsvg.dll`.

---

## 2. System Context

### 2.1 Layer Architecture

```
Layer 2  ImageProcTest.exe (C# WPF GUI)
         ↓ P/Invoke (C ABI)
Layer 1  xpe_preprocess.dll ← [THIS MODULE - Calibration subsystem]
         ↓ Link dependency
Layer 0  xpe_common.dll (types, memory, error codes, alerts)
         ↓ Link dependency
         Win32 / OS APIs
```

### 2.2 Data Flow Context

```
Raw uint16 Frame (3072×3072)
    ↓ [Detector metadata: temperature, kVp, SID, binning_mode]
    ↓
[CalibrationManager: Load offset/gain/BPM files]
    ↓
[Temperature Compensation] (optional, conditional)
    ↓
[Offset Correction] (mandatory)
    ↓
[Nonlinearity Correction] (optional, conditional)
    ↓
[Gain Correction] (mandatory) ← Format boundary: uint16 → float32
    ↓
[Binning Correction] (optional, conditional)
    ↓
[Defect Correction] (optional, conditional)
    ↓
[Ghost/Lag Correction] (optional, conditional) ← Uses frame history
    ↓
Calibrated float32 Frame (3072×3072)
    ↓ [Status flags: which corrections applied]
    ↓ [Diagnostic log: timing, bypass decisions]
    ↓
[xpe_enhance_basic.dll] (downstream: log transform, CLAHE, etc.)
```

### 2.3 External Interface Dependencies

| System | Protocol | Direction | Purpose |
|--------|----------|-----------|---------|
| **xpe_common.dll** | C ABI (link-time) | Import | Types (`XpeImageBuffer`, `XpeImageMetadata`, `XpeErrorCode`), memory utils, alert queue |
| **Disk I/O** | Win32 File API | Input | Load calibration files (.xpe_calib) |
| **Detector Driver** | Detector SDK (passed via metadata) | Input | Temperature sensor, kVp, SID, binning mode |
| **C# GUI (ImageProcTest)** | P/Invoke / C ABI | Bidirectional | Configure(), process frames, retrieve status |

---

## 3. Software Items and Units

### 3.1 Software Item 1: CalibrationManager (SWI-CALIB-1)

Central orchestrator for calibration data loading, validation, and lifecycle management.

#### 3.1.1 Software Units (SWU) Decomposition

| SWU ID | Name | Responsibility | Dependencies | Threads |
|--------|------|-----------------|--------------|---------|
| **SWU-1.1** | OffsetCorrector | Apply offset (dark current) correction: `I_corr = I_raw - I_dark` | CalibFileIO (offsetMap), TempCompensator | Main thread |
| **SWU-1.2** | GainCorrector | Apply gain normalization + uint16→float32 conversion: `I_norm = I_corr / G` | CalibFileIO (gainMap) | Main thread |
| **SWU-1.3** | DefectCorrector | Detect and interpolate bad pixels using BPM + RMM algorithm | CalibFileIO (BPM), RuntimeDefectDetector | Main thread |
| **SWU-1.4** | GhostCorrector | Multi-tier lag/ghosting removal (Tier 1 LTI → Tier 2 exposure-weighted → Tier 3 NLCSC) | SessionManager (frame history), CalibFileIO (NLCSC coefficients) | Per-handle mutex |
| **SWU-1.5** | CalibFileIO | Load/parse/validate calibration files (offset, gain, BPM, NLCSC). CRC-32 validation, expiry checking. | xpe_common (error codes, alerts) | Main thread |

#### 3.1.2 Responsibilities and Behavior

**SWU-1.1 OffsetCorrector:**
- Input: uint16 raw frame + offsetMap (uint16)
- Operation: Pixel-wise subtraction with clamp-to-zero
- Output: uint16 offset-corrected frame
- Error handling: Null pointer check on offsetMap → `XPE_ERR_NOT_INITIALIZED`
- State: Stateless (no frame history needed)

**SWU-1.2 GainCorrector:**
- Input: uint16 offset-corrected frame + gainMap (float32)
- Operation: Pixel-wise division + conversion to float32
- Output: float32 gain-normalized frame ← **FORMAT BOUNDARY**
- Error handling: Zero/invalid gain values → `XPE_ERR_INVALID_CALIB_DATA`
- Multi-gain support: `G(x,y,E) = Σ(c_k × E^k)` for energy-dependent correction
- State: Stateless

**SWU-1.3 DefectCorrector:**
- Input: float32 frame + BPM (uint8) + defect configuration
- Operation: Identify defective pixels, apply interpolation (neighbor avg / bilinear / median)
- Output: float32 frame with defects corrected
- Runtime detection: Optional SNR-based defect flagging (log to runtime map)
- Error handling: Invalid interpolation mode → `XPE_ERR_INVALID_PARAM`
- State: Stateless

**SWU-1.4 GhostCorrector:**
- Input: float32 frame + frame history (ring buffer, 8 frames)
- Operation: Multi-exponential deconvolution (Tier 1) → exposure-weighted escalation (Tier 2) → NLCSC (Tier 3)
- Output: float32 frame with lag corrected
- State: Stateful (owns exposure history buffer, session_id)
- Lifecycle: Create handle at startup → correct() per frame → reset() on patient change → destroy() at shutdown
- Error handling: Empty history (first frame or after reset) → skip correction, log bypass

**SWU-1.5 CalibFileIO:**
- Input: File paths (const char*)
- Operation: Read binary .xpe_calib files, parse headers, validate CRC-32, check expiry
- Output: Loaded maps (offsetMap, gainMap, BPM)
- Error codes: `XPE_ERR_IO_FAILED` (CRC), `XPE_ERR_CALIBRATION_EXPIRED` (timestamp), `XPE_ERR_INVALID_CALIB_DATA` (format)
- State: Stateless per call (but LoadedCalibration global cache)

### 3.2 Software Item 2: Supporting Units (SWI-CALIB-2)

#### 3.2.1 TempCompensator (SWU-1.6)

**Responsibility:** Temperature-dependent dark current compensation.

- Input: uint16 frame + current temperature (°C) + temperature LUT/polynomial
- Operation: Calculate `I_dark(T) = I_dark(T_ref) × exp(-(E_g/2kB) × (1/T - 1/T_ref))`
- Output: Temperature-adjusted dark current map
- Bypass condition: `|T - T_ref| ≤ 2°C` → skip, use nominal
- State: Stateless (LUT loaded from config at startup)

#### 3.2.2 NonlinearityCorrector (SWU-1.7)

**Responsibility:** Detector response linearization.

- Input: uint16 frame + nonlinearity LUT/polynomial (from detector profile)
- Operation: LUT lookup or polynomial evaluation to linearize response
- Output: uint16 linearized frame
- Bypass condition: Detector profile `panel.linear = true` → skip
- State: Stateless (LUT loaded from detector profile)

#### 3.2.3 BinningCorrector (SWU-1.8)

**Responsibility:** Pixel binning mode gain adjustment.

- Input: float32 frame + binning mode (1, 2, 4)
- Operation: Divide gain by `binning_factor^2` (charge accumulation model)
- Output: float32 binning-corrected frame
- Bypass condition: `binning_mode == 1` (native resolution) → skip
- State: Stateless

#### 3.2.4 RuntimeDefectDetector (SWU-1.9)

**Responsibility:** Identify new defects not in static BPM.

- Input: 10 consecutive float32 frames
- Operation: Calculate per-pixel SNR, flag SNR < 5 dB as defects
- Output: Runtime defect map (uint8, same layout as BPM)
- State: Stateful (accumulates 10-frame buffer internally)
- Integration: Merge runtime map with static BPM before DefectCorrector execution

#### 3.2.5 SessionManager (SWU-1.10)

**Responsibility:** Track calibration session state (frame history, metadata).

- Input: Session configuration (detector temp, kVp, SID, binning, session_id)
- Operation: Create session, maintain frame history ring buffer, reset history
- Output: Session handle for passing to GhostCorrector
- State: Stateful per handle (one handle per concurrent acquisition)
- API:
  - `CreateSession()` → handle
  - `AddFrame(handle, frame)` → add to history buffer
  - `ResetSession(handle)` → clear history (patient change)
  - `DestroySession(handle)` → free resources

---

## 4. External Interfaces

### 4.1 Input Interfaces

#### IF-EXT-CALIB-1: Raw Detector Data

```c
typedef struct {
    uint16_t* pixel_data;        // Array of 3072×3072 pixels
    uint32_t width;              // 3072
    uint32_t height;             // 3072
    uint32_t timestamp_ms;       // Frame timestamp
    float temp_celsius;          // Detector temperature (from NTC sensor)
    uint16_t kVp;                // X-ray tube voltage (kV)
    uint16_t mAs;                // Exposure (mAs)
    uint32_t SID;                // Source-to-detector distance (mm)
    uint8_t binning_mode;        // 1 (native), 2, or 4
    uint32_t sequence_number;    // Frame number in sequence
} XpeRawFrame;
```

**Protocol:** C struct passed by reference to correction functions  
**Data encoding:** uint16 big-endian (network order) or little-endian (configurable)

#### IF-EXT-CALIB-2: Calibration File Paths

```c
typedef struct {
    const char* offset_file_path;   // "D:\Calibration\detector_offset.xpe_calib"
    const char* gain_file_path;     // "D:\Calibration\detector_gain.xpe_calib"
    const char* bpm_file_path;      // "D:\Calibration\detector_bpm.xpe_calib"
    const char* config_file_path;   // "D:\Config\xpe_preprocess_config.json"
} XpeCalibrationPaths;
```

**Protocol:** File paths passed at initialization via `xpe_configure()`  
**Format:** UTF-8 encoded C strings

#### IF-EXT-CALIB-3: Detector Profile Metadata

```c
typedef struct {
    const char* detector_model;      // "Varex XRD4343N"
    float nominal_temp_celsius;      // 25.0
    bool panel_is_linear;            // false (requires nonlinearity correction)
    uint16_t max_gray_value;         // 65535 (16-bit)
    float pixel_pitch_microns;       // 143.5
    uint16_t max_binning_mode;       // 4 (supports 1×1, 2×2, 4×4)
} XpeDetectorProfile;
```

**Protocol:** Loaded from detector configuration JSON at startup

### 4.2 Output Interfaces

#### IF-EXT-CALIB-3: Processed Image

```c
typedef struct {
    float* pixel_data;               // Array of 3072×3072 float32 pixels
    uint32_t width;                  // 3072
    uint32_t height;                 // 3072
    uint32_t flags;                  // XPE_FLAG_GAIN_CORRECTED | XPE_FLAG_DEFECT_CORRECTED | ...
    float min_pixel;                 // Minimum pixel value in frame
    float max_pixel;                 // Maximum pixel value in frame
    const char* diagnostic_log_json; // JSON string: {"offset_ms": 55, "gain_ms": 55, ...}
} XpeProcessedFrame;
```

**Protocol:** C struct returned from correction functions  
**Data encoding:** float32 IEEE 754 (little-endian)  
**Ownership:** Allocated by correction function, freed by caller via `xpe_free_image_buffer()`

#### IF-EXT-CALIB-4: Error Codes and Diagnostics

```c
typedef enum {
    XPE_OK = 0,
    XPE_ERR_NOT_INITIALIZED = -1,
    XPE_ERR_CALIBRATION_EXPIRED = -2,
    XPE_ERR_IO_FAILED = -3,
    XPE_ERR_INVALID_CALIB_DATA = -4,
    XPE_ERR_INVALID_PARAM = -5,
} XpeErrorCode;

typedef struct {
    XpeErrorCode error_code;
    const char* error_message;       // Human-readable error description
    const char* remediation_hint;    // "Load calibration file and retry"
    uint64_t timestamp_ns;           // When error occurred
} XpeErrorInfo;
```

**Protocol:** Return value from all correction functions  
**Logging:** Error details pushed to alert queue (xpe_common module)

---

## 5. Internal Data Flow and Processing Pipeline

### 5.1 Simplified Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│ INPUT: uint16 Raw Frame (3072×3072)                            │
│ METADATA: temperature, kVp, SID, binning_mode, session_id      │
└────────────────────────┬────────────────────────────────────────┘
                         │
         ┌───────────────┴────────────────┐
         │                                │
         v                                v
    [CalibFileIO]                    [SessionManager]
    Load offset map                  Add frame to history
    Load gain map                    (8-frame ring buffer)
    Load BPM
         │
         └────────┬──────────────────┘
                  │
                  v
    ┌─────────────────────────────────────────┐
    │  TempCompensator (if sensor & |ΔT|>2°C) │
    │  I_dark(T) = I_dark(T_ref) × exp(...) │
    └─────────────┬───────────────────────────┘
                  │ uint16
                  v
    ┌─────────────────────────────────────────┐
    │  OffsetCorrector                        │
    │  I_corr = I_raw - I_dark (MANDATORY)   │
    └─────────────┬───────────────────────────┘
                  │ uint16
                  v
    ┌─────────────────────────────────────────┐
    │  NonlinearityCorrector (if needed)      │
    │  I_lin = f_nonlinearity(I_corr)        │
    └─────────────┬───────────────────────────┘
                  │ uint16
                  v
    ┌─────────────────────────────────────────┐
    │  GainCorrector (MANDATORY)              │
    │  I_norm = I_corr / G(x,y)              │
    │  uint16 → float32 FORMAT BOUNDARY      │
    └─────────────┬───────────────────────────┘
                  │ float32
                  v
    ┌─────────────────────────────────────────┐
    │  BinningCorrector (if binning_mode≠1)  │
    │  G_binned = G_native / (factor^2)      │
    └─────────────┬───────────────────────────┘
                  │ float32
                  v
    ┌─────────────────────────────────────────┐
    │  DefectCorrector (if BPM non-empty)    │
    │  Interpolate bad pixels                │
    └─────────────┬───────────────────────────┘
                  │ float32
                  v
    ┌─────────────────────────────────────────┐
    │  GhostCorrector (if history available) │
    │  Tier 1: LTI deconvolution             │
    │  Tier 2: Exposure-weighted             │
    │  Tier 3: NLCSC (auto-escalate)        │
    └─────────────┬───────────────────────────┘
                  │ float32
                  v
    ┌─────────────────────────────────────────┐
    │ OUTPUT: float32 Calibrated Frame       │
    │ + FLAGS: which corrections applied     │
    │ + DIAGNOSTICS: timing, bypass decisions│
    └─────────────────────────────────────────┘
```

### 5.2 Control Flow: Which SWU Execute?

```
START: New raw frame
       │
       ├─→ CalibFileIO: Check if offset/gain/BPM loaded?
       │   NO  → Raise XPE_ERR_NOT_INITIALIZED, ABORT
       │   YES → Continue
       │
       ├─→ CalibFileIO: Check calibration expiry?
       │   EXPIRED → Raise XPE_ERR_CALIBRATION_EXPIRED, ABORT
       │   OK → Continue
       │
       ├─→ TempCompensator: Is sensor available AND |T - T_ref| > 2°C?
       │   YES → Apply temperature correction
       │   NO  → Skip, log bypass
       │
       ├─→ OffsetCorrector: ALWAYS EXECUTE
       │   I_corr = I_raw - I_dark
       │
       ├─→ NonlinearityCorrector: Is panel.linear = false?
       │   YES → Apply nonlinearity correction
       │   NO  → Skip, log bypass (detector already linear)
       │
       ├─→ GainCorrector: ALWAYS EXECUTE ← FORMAT BOUNDARY
       │   I_norm = I_corr / G(x,y) → float32
       │
       ├─→ BinningCorrector: Is binning_mode ≠ 1?
       │   YES → Apply binning gain adjustment
       │   NO  → Skip, native resolution already handled
       │
       ├─→ DefectCorrector: Is BPM non-empty?
       │   YES → Detect + interpolate defects
       │   NO  → Skip, no defects to correct
       │
       ├─→ SessionManager: Add corrected frame to history
       │   Store in ring buffer (max 8 frames)
       │
       ├─→ GhostCorrector: Is frame history available?
       │   YES → Apply ghost correction (Tier 1 → escalate if needed)
       │   NO  → Skip (first frame or after reset)
       │
       └─→ Return output + flags + diagnostic log
```

---

## 6. Architecture Constraints and Design Rules

### 6.1 Layering Rules

**Anti-Spaghetti Constraint:**
- `xpe_preprocess.dll` may only depend on `xpe_common.dll`
- No lateral dependencies on other Layer 1 DLLs (`xpe_enhance_basic`, `gsvg`, etc.)
- No direct OS API calls except through `xpe_common` wrapper functions

**Rationale:** Enables independent testing, reusability, and maintenance. Prevents hidden circular dependencies.

### 6.2 Data Ownership and Lifetime

| Resource | Owner | Lifetime |
|----------|-------|----------|
| offsetMap (uint16 array) | CalibFileIO | Loaded at startup, freed at shutdown |
| gainMap (float32 array) | CalibFileIO | Loaded at startup, freed at shutdown |
| BPM (uint8 array) | CalibFileIO | Loaded at startup, freed at shutdown |
| Frame history buffer | SessionManager | Per-session, freed on session destroy |
| Output frame buffer | Correction function | Allocated and returned; caller responsible for freeing via `xpe_free_image_buffer()` |

### 6.3 Thread Safety

| Component | Thread Safety | Notes |
|-----------|---------------|-------|
| CalibFileIO | Thread-safe (read-only after load) | Calibration files loaded once at startup; multiple threads can read |
| Correction functions (SWU-1.1, 1.2, 1.3, 1.7, 1.8) | Stateless, reentrant | No shared state; can process multiple frames concurrently with non-overlapping buffers |
| GhostCorrector (SWU-1.4) | Stateful, per-handle mutex | Frame history is per-handle; protect with mutex when multiple threads access same handle |
| SessionManager (SWU-1.10) | Per-handle locks | Each session handle has its own lock; creating multiple handles enables parallel sessions |

### 6.4 Error Handling Strategy

**Fail-Fast Principle:**
- Errors detected early in pipeline (CalibFileIO, OffsetCorrector) abort immediately
- Return detailed error codes and remediation hints
- Never silently skip mandatory corrections
- Conditional (optional) corrections log bypass decisions for audit

**No Cascading Failures:**
- If offset correction fails, abort pipeline before attempting downstream corrections
- If expiry check fails, prevent all processing (don't apply expired calibration)

### 6.5 Memory Allocation Strategy

**Bounded Allocation:**
- Maximum per-frame allocation: 200 MB (see SRS-CALIB-PERF-002)
- Pre-allocate calibration maps once at startup (offset: 18.9 MB, gain: 37.7 MB, BPM: 9.4 MB)
- Frame buffers allocated per-correction, deallocated after processing
- No recursive allocations in hot path

**Prevention of Memory Leaks:**
- Use RAII pattern where applicable; pair malloc/free
- Explicit free() calls logged in diagnostic output
- Memory profiling in test suite detects leaks

---

## 7. SOUP (Software Of Uncertain Pedigree) and External Components

### 7.1 Algorithms from Literature

| Algorithm | Source | Application | Integration Method |
|-----------|--------|-------------|-------------------|
| **Multi-exponential lag deconvolution** | Pang et al. (2006); Ranger et al. (2014) | SWU-1.4 Tier 1 ghost correction | Implemented in C with published coefficients |
| **NLCSC (Signal-Dependent Lag Correction)** | Starman et al. (2012) | SWU-1.4 Tier 3 ghost correction | Proprietary coefficients loaded from calibration file |
| **RMM (Robust Mask Maker)** | Published algorithm, lambda=8.0 | SWU-1.9 runtime defect detection | Published source; safety-critical, unit-tested |
| **Heel Effect Correction** | Wang et al. (2013) | SWU-1.2 multi-gain correction | Projection geometry model; validated against reference data |

### 7.2 Standard Libraries

| Library | Purpose | Risk Level |
|---------|---------|-----------|
| **Win32 FileAPI** | File I/O for calibration loading | Low (OS standard) |
| **C Standard Library** | Math (sin, cos, exp), memory (malloc/free) | Low (widely used) |
| **CRC-32 (Cygwin implementation)** | Checksum validation | Low (standard algorithm) |

### 7.3 SOUP Risk Management

All external algorithms shall be:
1. **Documented** with source papers cited in this architecture document
2. **Validated** against reference implementations or published test data
3. **Tested** with unit tests covering nominal and edge cases
4. **Monitored** for floating-point differences between implementations

---

## 8. Architecture Verification and Traceability

### 8.1 Requirements-to-Architecture Mapping

Each SRS requirement shall be traced to one or more SWUs:

| SRS Requirement | SWU | Verification Method |
|--------|-----|-------------------|
| SRS-CALIB-FUNC-001 (offset file load) | SWU-1.5 (CalibFileIO) | Unit test: parse header, validate CRC |
| SRS-CALIB-FUNC-002 (gain file load) | SWU-1.5 (CalibFileIO) | Unit test: parse header, range check |
| SRS-CALIB-FUNC-003 (BPM file load) | SWU-1.5 (CalibFileIO) | Unit test: RLE decompression |
| SRS-CALIB-FUNC-004 (offset correction) | SWU-1.1 (OffsetCorrector) | Unit test: pixel arithmetic, clamp |
| SRS-CALIB-FUNC-005 (gain correction) | SWU-1.2 (GainCorrector) | Unit test: division, float32 conversion |
| SRS-CALIB-FUNC-006 (nonlinearity) | SWU-1.7 (NonlinearityCorrector) | Unit test: LUT lookup accuracy |
| SRS-CALIB-FUNC-007 (defect correction) | SWU-1.3 (DefectCorrector) | Unit test: interpolation methods |
| SRS-CALIB-FUNC-008 (temp compensation) | SWU-1.6 (TempCompensator) | Unit test: exponential model |
| SRS-CALIB-FUNC-009 (expiry check) | SWU-1.5 (CalibFileIO) | Unit test: timestamp comparison |
| SRS-CALIB-FUNC-010 (runtime defects) | SWU-1.9 (RuntimeDefectDetector) | Integration test: SNR calculation |
| SRS-CALIB-FUNC-011 (session management) | SWU-1.10 (SessionManager) | Unit test: handle creation/reset |
| SRS-CALIB-FUNC-012 (binning correction) | SWU-1.8 (BinningCorrector) | Unit test: gain scaling formula |
| SRS-CALIB-FUNC-013 (ghost Tiers 1-3) | SWU-1.4 (GhostCorrector) | Integration test: lag residual measurement |
| SRS-CALIB-FUNC-014 (frame history) | SWU-1.10 (SessionManager) | Unit test: ring buffer management |

### 8.2 Architecture-to-Code Mapping

Folder structure and file naming shall follow this mapping:

```
xpe_preprocess.dll/
├── src/
│   ├── calib_offset.c          → SWU-1.1 OffsetCorrector
│   ├── calib_gain.c            → SWU-1.2 GainCorrector
│   ├── calib_defect.c          → SWU-1.3 DefectCorrector
│   ├── calib_ghost.c           → SWU-1.4 GhostCorrector
│   ├── calib_file_io.c         → SWU-1.5 CalibFileIO
│   ├── calib_temp.c            → SWU-1.6 TempCompensator
│   ├── calib_nonlinearity.c    → SWU-1.7 NonlinearityCorrector
│   ├── calib_binning.c         → SWU-1.8 BinningCorrector
│   ├── calib_runtime_defects.c → SWU-1.9 RuntimeDefectDetector
│   ├── calib_session.c         → SWU-1.10 SessionManager
│   └── calib_manager.c         → Central orchestration (CalibrationManager)
├── inc/
│   ├── calib_*.h               → Internal headers (one per SWU)
│   └── xpe_preprocess.h        → Public API (C ABI exports)
├── tests/
│   ├── test_calib_offset.cpp
│   ├── test_calib_gain.cpp
│   ├── test_calib_ghost.cpp
│   └── ... (one per SWU)
└── README.md
```

---

## 9. Architecture Constraints and Known Limitations

### 9.1 Technical Constraints

| Constraint | Rationale | Mitigation |
|-----------|-----------|-----------|
| Offset/gain maps fixed at 3072×3072 | Factory-calibrated maps designed for this detector | Document max detector size; reject larger frames |
| Float32 precision loss in gain stage | IEEE 754 single-precision accumulates rounding error | Clamp output to valid medical range [0, 3.4e38] |
| Ghost correction requires 8-frame history | Lag decay requires temporal context | Document that first frame after reset skips ghost |
| CRC-32 collision risk | ~1 in 4 billion for random data | Acceptable for medical device (single file, not cryptographic) |
| No GPU acceleration | Performance target met on CPU (500 ms/frame) | Document that GPU acceleration future work |

### 9.2 Known Limitations

1. **Nonlinearity Correction:** Assumes detector-specific polynomial coefficients available in detector profile. If not provided, stage is skipped (but logged). Detector calibrator must pre-compute coefficients.

2. **Temperature Compensation:** Relies on accurate NTC thermistor reading. If sensor drifts or fails, system uses nominal 25°C (logged as bypass).

3. **Binning Correction:** Assumes square binning (2×2, 4×4). Non-square binning (e.g., 2×4) not supported.

4. **Ghost Correction Tier 3 (NLCSC):** Requires pre-calculated signal-dependent coefficients (proprietary). Without coefficients, falls back to Tier 1 or Tier 2.

---

## 10. Future Extensibility Points

### 10.1 Planned Extensions

| Feature | Timeline | Impact |
|---------|----------|--------|
| GPU-accelerated gain correction (CUDA) | Phase 2 | Can reduce gain stage from 55 ms to 15 ms |
| Advanced defect correction (MLP neural network, FixPix) | Phase 2 | Replace RMM + bilinear with learned model |
| Scatter correction (pre-calibration step) | Phase 3 | New SWU for scatter removal before offset |
| Automatic calibration file discovery | Phase 2 | SWU-1.5 enhancement: search standard paths |

### 10.2 Interface Stability

- **C ABI public API** (xpe_preprocess.h exports): Stable contract; breaking changes require major version bump
- **Internal SWU interfaces**: Subject to change during implementation; documented in design reviews
- **Calibration file format** (.xpe_calib): Version field (header offset 4) allows format evolution

---

## 11. Design Decisions and Rationale

### Decision: Why Separate OffsetCorrector and GainCorrector?

**Rationale:** Offset and gain are mathematically independent operations on different domains (additive vs. multiplicative). Separating them enables:
1. Independent validation and testing
2. Easier debugging (localize errors to one stage)
3. Future GPU acceleration of individual stages
4. Conditional execution (future: skip offset if factory-corrected detectors)

### Decision: Why uint16→float32 at Gain Stage?

**Rationale:** Offset and nonlinearity corrections can remain in uint16 (integer math is faster). Gain correction requires division, which naturally produces floating-point results. By deferring format conversion to this stage, we minimize floating-point arithmetic in the hot path while ensuring downstream stages (defect, ghost) work with stable float32 format.

### Decision: Why 8-Frame History Buffer for Ghost Correction?

**Rationale:** Typical lag decay time constants (τ_i) range from 10 ms to 500 ms. At 30 fps frame rate, 8 frames = 267 ms ≈ 0.5 seconds, capturing 2-3 decay time constants. This balance provides:
- Sufficient temporal data for accurate deconvolution (Tier 1-2)
- Manageable memory footprint (~150 MB)
- Practical buffer management (ring buffer, no reallocation)

### Decision: Why Separate Session Management from Ghost Correction?

**Rationale:** SessionManager provides generic frame history tracking, potentially useful for other temporal operations (e.g., motion detection). GhostCorrector focuses specifically on lag correction algorithm. Separation enables:
1. Reuse of SessionManager for future temporal algorithms
2. Independent testing of history management
3. Flexibility in multi-session concurrent processing

---

## References

### Architecture Standards

| Reference | Application |
|-----------|-------------|
| IEC 62304:2006 §5.3 | Software architectural design requirements |
| ISO/IEC/IEEE 42010:2011 | Systems and software engineering — Architecture description |

### Research Papers (Referenced in Design)

| Citation | Topic |
|----------|-------|
| Starman et al. (2012) | NLCSC lag correction model |
| Wang et al. (2013) | Heel effect multi-gain correction |
| Pang et al. (2006) | Multi-exponential lag model |
| Ranger et al. (2014) | Lag time constant characterization |

### Project Documentation

| Document | Location |
|----------|----------|
| Software Requirements Specification | `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md` |
| Calibration Module README | `docs/calibration/README.md` |
| Algorithm Specification | `.moai/specs/xpe-algorithm-spec-deepsync.md` |

---

*SAD-CALIB-001 v1.0 — End of Document*
