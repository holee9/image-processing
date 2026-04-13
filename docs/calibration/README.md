# X-ray FPD Calibration Pre-Processing Module

**Module**: `xpe_preprocess.dll` (Layer 1, Phase 1a)  
**Owner DLL**: `xpe_preprocess.dll`  
**Dependency**: `xpe_common.dll` (Layer 0)  
**Safety Class**: IEC 62304 Class B  
**Document Version**: 1.0.0  
**Date**: 2026-04-14  
**Normative Spec**: [ALG-SPEC-001 v3.0.0-ds2](../../.moai/specs/xpe-algorithm-spec-deepsync.md)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Pipeline Stages](#3-pipeline-stages)
4. [Stage Dependency Graph](#4-stage-dependency-graph)
5. [Data Flow and Format Transitions](#5-data-flow-and-format-transitions)
6. [Stage Bypass (On/Off) Policy](#6-stage-bypass-onoff-policy)
7. [Bypass Configuration](#7-bypass-configuration)
8. [Calibration Data Management](#8-calibration-data-management)
9. [API Reference](#9-api-reference)
10. [Performance Budget](#10-performance-budget)
11. [Safety Constraints](#11-safety-constraints)
12. [References](#12-references)

---

## 1. Overview

`xpe_preprocess.dll` is the calibration pre-processing engine for X-ray Flat Panel Detector (FPD) image processing. It transforms raw ADC sensor output into a clean, calibrated `float32` image by correcting physical detector artifacts: dark current, pixel sensitivity non-uniformity, dead/hot pixels, charge-trapping lag, temperature drift, and response nonlinearity.

### Key Characteristics

- **9 processing stages** (0 through 4, including sub-stages)
- **18 exported C ABI functions** for correction, calibration data I/O, and ghost state management
- **3 mandatory stages** that cannot be bypassed under any circumstance
- **6 conditionally bypassable stages** with documented safety constraints
- **1 critical format boundary**: `uint16` to `float32` conversion at Gain Correction (stage 2)
- **3-tier ghost correction** with automatic escalation (LTI -> Exposure-Weighted -> NLCSC)

### Supported Detector Types

| Detector | Conversion | Representative | Calibration Notes |
|----------|-----------|---------------|-------------------|
| a-Si TFT FPD | Indirect (CsI:Tl, GOS) | Varex XRD 4343N | Lag correction essential, high temperature sensitivity |
| CMOS FPD | Indirect/Direct | Vieworks VIVIX-S | Low lag, high dynamic range |
| Perovskite FPD | Direct | Research-stage | Special nonlinearity correction |
| Se/CdTe Direct FPD | Direct | Siemens, Philips | Unique defect patterns |

---

## 2. Architecture

### Layer Position

```
Layer 2  ImageProcTest.exe (C# WPF)       Pipeline orchestrator
           |
           | P/Invoke (C ABI)
           v
Layer 1  xpe_preprocess.dll  <-- THIS MODULE
           |
           | Link dependency
           v
Layer 0  xpe_common.dll                    Types, Memory, Config, Error, Alert
```

### Anti-Spaghetti Rule

- `xpe_preprocess.dll` depends ONLY on `xpe_common.dll`
- No lateral dependencies to other Layer 1 DLLs (`xpe_enhance_basic`, `gsvg`, etc.)
- All shared types and utilities go through `xpe_common.dll`

### Software Units (SWU)

| SWU ID | Name | Stage | Description |
|--------|------|:-----:|-------------|
| SWU-1.1 | OffsetCorrection | (1) | Dark current subtraction with dynamic interpolation |
| SWU-1.2 | GainCorrection | (2) | Flat-field normalization + multi-gain polynomial |
| SWU-1.3 | DefectCorrection | (3) | Bad pixel detection and interpolation |
| SWU-1.4 | GhostCorrection | (4) | 3-tier lag/ghosting removal |
| SWU-1.5 | CalibDataManager | (0) | Calibration file I/O, expiry validation, versioning |
| SWU-1.6 | ReadoutValidator | (0.5) | Raw frame integrity validation |
| SWU-1.7 | TempCompensation | (0.7) | Temperature-dependent dark current compensation |
| SWU-1.8 | NonlinearityCorrection | (1.5) | Detector response linearization |
| SWU-1.9 | BinningCorrection | (2.5) | Binning mode compensation |

---

## 3. Pipeline Stages

### 3.1 Complete Pre-Processing Sequence

```
Raw Frame (uint16, 14/16-bit ADC)
  |
  v
+================================================================+
|  (0) CalibManager Load                          [STARTUP ONLY] |
|  Load offset map, gain map, BPM, NLCSC coefficients            |
|  Budget: 200 ms (one-time)                                     |
+================================================================+
  |
  v
+----------------------------------------------------------------+
|  (0.5) Readout Artifact Validation               [ADVISORY]    |
|  Function: xpe_validate_readout_artifact()                     |
|  Checks: stuck rows/cols, ADC saturation, dropped lines        |
|  Mutates image: NO (flag + alert only)                         |
|  Bypass: Always safe                                           |
|  Flag: XPE_FLAG_READOUT_VALIDATED                              |
+----------------------------------------------------------------+
  |
  v
+----------------------------------------------------------------+
|  (0.7) Temperature Compensation                 [CONDITIONAL]  |
|  Function: xpe_temp_compensate()                               |
|  Model: I_dark(T) = I0 * exp(-Eg / 2*kB*T)                    |
|  Input: detector temperature from NTC sensor                   |
|  Bypass: sensor unavailable OR within +/-2C of nominal         |
|  Flag: XPE_FLAG_TEMP_COMPENSATED                               |
+----------------------------------------------------------------+
  |
  v  uint16
+=================================================================+
|| (1) Offset Correction                          [MANDATORY]   ||
|| Function: xpe_offset_correct()                               ||
|| Formula: I_corr(x,y) = I_raw(x,y) - I_dark(x,y)            ||
|| Dynamic: bilinear interpolation by temperature + PREP time   ||
|| Bypass: NEVER (dark current corrupts all downstream)         ||
|| Hard fail: offsetMap absent -> XPE_ERR_NOT_INITIALIZED       ||
+=================================================================+
  |
  v  uint16
+----------------------------------------------------------------+
|  (1.5) Nonlinearity Correction                  [CONDITIONAL]  |
|  Function: xpe_nonlinearity_correct()                          |
|  Model: LUT or monotonic polynomial linearization              |
|  Bypass: panel.linear = true in detector profile               |
|  Rationale: MUST precede Gain (linearize before normalize)     |
|  Flag: XPE_FLAG_NONLINEARITY_CORRECTED                         |
+----------------------------------------------------------------+
  |
  v  uint16
+=================================================================+
|| (2) Gain Correction                            [MANDATORY]   ||
|| Function: xpe_gain_correct()                                 ||
|| Formula: I_corr(x,y) = I_off(x,y) / G(x,y)                 ||
|| Multi-gain: G(x,y,E) = sum(c_k * E^k), internal selection   ||
|| Heel effect: Duo-SID projection (Wang 2013)                  ||
|| Bypass: NEVER (format conversion + normalization)            ||
|| Hard fail: gainMap absent -> XPE_ERR_NOT_INITIALIZED         ||
||                                                              ||
|| >>> FORMAT BOUNDARY: uint16 -> float32 <<<                   ||
+=================================================================+
  |
  v  float32
+----------------------------------------------------------------+
|  (2.5) Binning Correction                       [CONDITIONAL]  |
|  Function: xpe_binning_correct()                               |
|  Trigger: binningMode != 1 (not native 1x1)                   |
|  Bypass: binning mode inactive                                 |
|  Flag: XPE_FLAG_BINNING_CORRECTED                              |
+----------------------------------------------------------------+
  |
  v  float32
+----------------------------------------------------------------+
|  (3) Defect Correction                          [CONDITIONAL]  |
|  Function: xpe_defect_correct()                                |
|  Detection: RMM (Robust Mask Maker), lambda=8.0               |
|  Baseline: edge-aware bilinear interpolation                   |
|  Advanced: FixPix MLP (1425 params, FPGA-friendly)             |
|  Optional: xpe_defect_detect_runtime() for transient defects   |
|  Bypass: BPM empty AND runtime detection disabled              |
|  Flag: XPE_FLAG_DEFECT_CORRECTED                               |
+----------------------------------------------------------------+
  |
  v  float32
+----------------------------------------------------------------+
|  (4) Ghost / Lag Correction                     [CONDITIONAL]  |
|  Functions: xpe_ghost_create/correct/reset/destroy()           |
|  Tier 1: LTI multi-exponential (N=4) deconvolution            |
|  Tier 2: Exposure-weighted LTI                                 |
|  Tier 3: NLCSC (signal-dependent coefficients)                 |
|  State: exposureHistory ring buffer (8 frames, ~150 MB)        |
|  Bypass: first frame, single-shot, no history                  |
|  Flag: XPE_FLAG_GHOST_CORRECTED                                |
+----------------------------------------------------------------+
  |
  v  float32 (calibrated)
  |
  [To Enhancement Domain: stage (5) Log Transform]
```

### 3.2 Stage Summary Table

| # | Stage | SWU | Type | Input | Output | Calib Data | Stateful |
|---|-------|-----|:----:|:-----:|:------:|:----------:|:--------:|
| 0 | CalibManager Load | 1.5 | Startup | Files | Maps | All | Yes |
| 0.5 | Readout Validation | 1.6 | Advisory | uint16 | uint16 | None | No |
| 0.7 | Temp Compensation | 1.7 | Conditional | uint16 | uint16 | LUT/poly | No |
| 1 | Offset Correction | 1.1 | **Mandatory** | uint16 | uint16 | offsetMap | Yes |
| 1.5 | Nonlinearity | 1.8 | Conditional | uint16 | uint16 | LUT/poly | No |
| 2 | Gain Correction | 1.2 | **Mandatory** | uint16 | **float32** | gainMap | Yes |
| 2.5 | Binning Correction | 1.9 | Conditional | float32 | float32 | Config | No |
| 3 | Defect Correction | 1.3 | Conditional | float32 | float32 | BPM | Yes |
| 4 | Ghost Correction | 1.4 | Conditional | float32 | float32 | History+Coeff | Yes |

---

## 4. Stage Dependency Graph

### 4.1 Visual Dependency Map

```
                    ┌──────────────────────────────────────────────────────────┐
                    │                  (0) CalibManager                        │
                    │         Load calibration data at startup                 │
                    └────┬──────────┬───────────┬──────────┬──────────────────┘
                    DATA |     DATA |      DATA |     DATA |
                         v          v           v          v
  ┌────────────────────────────────────────────────────────────────────────┐
  │                        CALIBRATION DATA POOL                          │
  │   offsetMap    gainMap    BPM    NLCSC coefficients    Temp LUT       │
  └────┬───────────┬─────────┬──────┬───────────────────┬────────────────┘
       |           |         |      |                   |
       |           |         |      |        ┌──────────┘
       |           |         |      |        |
  ┌────▼───┐       |         |      |   ┌────▼────┐
  │  (0.5) │       |         |      |   │  (0.7)  │
  │Readout │       |         |      |   │  Temp   │
  │  Valid  │       |         |      |   │ Comp    │
  │ [A]    │       |         |      |   │ [C]     │
  └────┬───┘       |         |      |   └────┬────┘
       │           |         |      |        │
       └─────┐     |         |      |   ┌────┘
             v     |         |      |   v
          ┌════▼═══╪═════════╪══════╪═══▼════════════════════════┐
          ║  (1) Offset Correction                    [MANDATORY] ║
          ║  I_corr = I_raw - I_dark                              ║
          ║  REQUIRES: offsetMap                                   ║
          ╚════════════╤══════╪══════╪════════════════════════════╝
                       │      |      |
                ORDER  v      |      |
          ┌────────────────┐  |      |
          │ (1.5) Nonlin   │  |      |
          │ Correction [C] │  |      |
          └───────┬────────┘  |      |
                  │           |      |
            ORDER v      DATA v      |
          ┌════════════════════════╗  |
          ║  (2) Gain Correction   ║  |
          ║  I_corr = I_off / G    ║  |
          ║  [MANDATORY]           ║  |
          ║                        ║  |
          ║  <<< uint16 -> float32 ║  |
          ║      FORMAT BOUNDARY>>>║  |
          ╚═══════════╤════════════╝  |
                      │               |
               FORMAT v               |
          ┌──────────────────┐        |
          │ (2.5) Binning [C]│        |
          └───────┬──────────┘        |
                  │                   |
           FORMAT v              DATA v
          ┌──────────────────────────────┐
          │ (3) Defect Correction    [C] │
          │ REQUIRES: BPM + float32 data │
          └───────┬──────────────────────┘
                  │
            ORDER v
          ┌──────────────────────────────┐
          │ (4) Ghost/Lag Correction [C] │
          │ REQUIRES: exposure history   │
          │ + fully corrected frame      │
          └───────┬──────────────────────┘
                  │
                  v
          [Calibrated float32 output]

Legend:
  ═══  MANDATORY stage (double border)
  ───  CONDITIONAL stage (single border)
  [A]  ADVISORY (non-mutating)
  [C]  CONDITIONAL (bypassable)
  DATA  Requires calibration data
  ORDER Execution order constraint (physical/mathematical)
  FORMAT Requires float32 format from stage (2)
```

### 4.2 Dependency Matrix

Each cell shows whether the **row** stage depends on the **column** stage, and the type of dependency.

|  | (0) Calib | (0.5) Read | (0.7) Temp | (1) Offset | (1.5) NL | (2) Gain | (2.5) Bin | (3) Defect | (4) Ghost |
|--|:---------:|:----------:|:----------:|:----------:|:--------:|:--------:|:---------:|:----------:|:---------:|
| **(0) CalibManager** | -- | | | | | | | | |
| **(0.5) Readout** | | -- | | | | | | | |
| **(0.7) Temp** | | | -- | | | | | | |
| **(1) Offset** | `DATA` | | | -- | | | | | |
| **(1.5) Nonlin** | `DATA` | | | `ORDER` | -- | | | | |
| **(2) Gain** | `DATA` | | | `ORDER` | `ORDER` | -- | | | |
| **(2.5) Binning** | | | | | | `FMT` | -- | | |
| **(3) Defect** | `DATA` | | | | | `FMT` | | -- | |
| **(4) Ghost** | `DATA` | | | | | `FMT` | | `ORDER` | -- |

| Key | Meaning | Violation Impact |
|-----|---------|-----------------|
| `DATA` | Requires calibration data loaded by CalibManager | **Hard fail**: `XPE_ERR_NOT_INITIALIZED` |
| `ORDER` | Must execute after predecessor (physical constraint) | **Silent corruption**: incorrect correction |
| `FMT` | Requires `float32` format produced by Gain stage | **Crash**: type mismatch or buffer corruption |

### 4.3 Critical Path Analysis

The **minimum mandatory path** through pre-processing:

```
(0) CalibManager -> (1) Offset -> (2) Gain -> [output]
```

This path MUST always execute. It produces a minimally-corrected `float32` image. All other stages are optional enhancements that improve image quality but are not strictly required for a valid output.

---

## 5. Data Flow and Format Transitions

### 5.1 Format Domains

```
 ┌──────────────────────────────────────────────────────────────────┐
 │                                                                  │
 │   uint16 DOMAIN                 float32 DOMAIN                   │
 │   (Raw / Integer)               (Normalized / Floating-Point)    │
 │                                                                  │
 │   (0.5) Readout      ║         (2.5) Binning                    │
 │   (0.7) Temperature  ║         (3)   Defect                     │
 │   (1)   Offset       ║         (4)   Ghost                      │
 │   (1.5) Nonlinearity ║                                          │
 │                      ║                                          │
 │              ┌═══════╩════════════┐                              │
 │              ║  (2) Gain Correct  ║                              │
 │              ║  FORMAT BOUNDARY   ║                              │
 │              ║  uint16 -> float32 ║                              │
 │              ╚════════════════════╝                              │
 │                                                                  │
 └──────────────────────────────────────────────────────────────────┘
```

### 5.2 Buffer Specifications

| Parameter | Value |
|-----------|-------|
| Max dimensions | 4096 x 4096 |
| Typical dimensions | 3072 x 3072 |
| uint16 buffer | ~18.9 MB (3072 x 3072 x 2 bytes) |
| float32 buffer | ~37.7 MB (3072 x 3072 x 4 bytes) |
| Calibration maps | ~60 MB (offset + gain + BPM) |
| Ghost history (8 frames) | ~150 MB |
| Peak memory (pre-processing) | ~190 MB |

### 5.3 Metadata Flag Lifecycle

Each stage sets its corresponding flag bit in `XpeImageMetadata.flags` upon successful execution. Bypassed stages leave their flag **unset**.

```
flags = 0x00000000  (raw frame)

After (0.5):  flags |= XPE_FLAG_READOUT_VALIDATED       0x0010
After (0.7):  flags |= XPE_FLAG_TEMP_COMPENSATED        0x0020
After (1):    [no dedicated flag - always executed]
After (1.5):  flags |= XPE_FLAG_NONLINEARITY_CORRECTED  0x0040
After (2):    flags |= XPE_FLAG_GAIN_CORRECTED           0x0008
After (2.5):  flags |= XPE_FLAG_BINNING_CORRECTED        0x0080
After (3):    flags |= XPE_FLAG_DEFECT_CORRECTED         0x0004
After (4):    flags |= XPE_FLAG_GHOST_CORRECTED          0x0001

Example: full pre-processing applied, no binning
  flags = 0x007D = READOUT | TEMP | NONLIN | GAIN | DEFECT | GHOST

Example: minimal (offset + gain only, raw export mode)
  flags = 0x0008 = GAIN
```

---

## 6. Stage Bypass (On/Off) Policy

### 6.1 Bypass Classification

| Symbol | Category | Description |
|:------:|----------|-------------|
| `M` | **MANDATORY** | Cannot bypass. Pipeline fails without it. |
| `C` | **CONDITIONAL** | Bypassable under documented conditions. |
| `A` | **ADVISORY** | Non-mutating. Always safe to skip. |

### 6.2 Bypass Decision Table

| Stage | Cat. | Can Off? | Bypass Condition | Safety Impact | Downstream Effect |
|-------|:----:|:--------:|------------------|:-------------:|-------------------|
| **(0) CalibManager** | `M` | NO | -- | FATAL | No calibration data |
| **(0.5) Readout** | `A` | YES | Config: `readout_validation.enabled = false` | NONE | No integrity check |
| **(0.7) Temp** | `C` | YES | No sensor OR temp within +/-2C of nominal | LOW | Minor dark drift |
| **(1) Offset** | `M` | NO | -- | CRITICAL | Dark bias in all pixels |
| **(1.5) Nonlinearity** | `C` | YES | `panel.linear = true` in detector profile | LOW | Minor response curve error |
| **(2) Gain** | `M` | NO | -- | CRITICAL | No normalization + no float32 conversion |
| **(2.5) Binning** | `C` | YES | `binningMode == 1` (native) | NONE | Not applicable |
| **(3) Defect** | `C` | YES | BPM empty OR diagnostic mode | MEDIUM | Defect artifacts visible |
| **(4) Ghost** | `C` | YES | Single-shot OR first frame OR no history | MEDIUM | Lag artifacts visible |

### 6.3 Bypass Decision Flowchart

```
                        START: New raw frame acquired
                                    |
                    +===============v================+
                    |   (0) CalibManager loaded?      |
                    +================================+
                        |                    |
                       YES                  NO
                        |                    |
                        v               ABORT PIPELINE
                +-------v--------+
                | (0.5) Readout  |
                | enabled?       |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: no mutation, safe]
                  |
                  v
            xpe_validate_readout_artifact()
                  |
            score > CRITICAL? --YES--> ABORT FRAME
                  |
                 NO
                  v
                +-------v--------+
                | (0.7) Temp     |
                | sensor avail?  |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: use nominal 25C + alert]
                  |
            |temp - 25C| > 2.0?
                  |           |
                 YES         NO -----> [SKIP: within tolerance]
                  |
                  v
            xpe_temp_compensate()
                  |
                  v
            +=========v==========+
            || (1) Offset       ||
            || ALWAYS EXECUTE   ||
            +====================+
            offsetMap loaded?
              |           |
             YES         NO -----> HARD FAIL: XPE_ERR_NOT_INITIALIZED
              |
              v
            xpe_offset_correct()
              |
              v
                +-------v--------+
                | (1.5) Nonlin   |
                | panel.linear?  |
                +----------------+
                  |           |
                 NO (apply)  YES (linear) -> [SKIP: profile says linear]
                  |
                  v
            xpe_nonlinearity_correct()
              |
              v
            +=========v==========+
            || (2) Gain         ||
            || ALWAYS EXECUTE   ||
            || uint16 -> float32||
            +====================+
            gainMap loaded?
              |           |
             YES         NO -----> HARD FAIL: XPE_ERR_NOT_INITIALIZED
              |
              v
            xpe_gain_correct()
              |
              v  [now float32]
                +-------v--------+
                | (2.5) Binning  |
                | mode != 1x1?  |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: native resolution]
                  |
                  v
            xpe_binning_correct()
              |
              v
                +-------v--------+
                | (3) Defect     |
                | BPM non-empty? |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: no defects]
                  |
            diagnostic mode?
                  |           |
                 NO          YES -----> [SKIP: raw export]
                  |
                  v
            xpe_defect_correct()
              |
              v
                +-------v--------+
                | (4) Ghost      |
                | history avail? |
                +----------------+
                  |           |
                 YES         NO -----> [SKIP: first frame / single-shot]
                  |
            single-shot mode?
                  |           |
                 NO          YES -----> [SKIP: no temporal correction]
                  |
                  v
            xpe_ghost_correct()
            [auto-escalate Tier 1->2->3]
              |
              v
            CALIBRATED float32 OUTPUT
```

### 6.4 Operating Modes

| Mode | Stages Executed | Use Case |
|------|:--------------:|----------|
| **Full Clinical** | All 9 stages | Normal clinical imaging |
| **Minimal Clinical** | (0), (1), (2), (3) | Fast acquisition, linear detector, no lag concern |
| **Diagnostic / Raw Export** | (0), (1), (2) only | External analysis tools, QA investigation |
| **Single-Shot** | All except (4) | First frame after power-on |
| **Fluoro / Continuous** | All 9 + Tier 2/3 ghost | Real-time fluoroscopy with lag correction |

---

## 7. Bypass Configuration

### 7.1 JSON Configuration Schema

Configure bypass behavior via `xpe_configure()`:

```json
{
  "preprocess": {
    "readout_validation": {
      "enabled": true,
      "critical_threshold": 500,
      "warn_threshold": 200
    },
    "temp_compensation": {
      "enabled": true,
      "auto_bypass_tolerance_c": 2.0,
      "nominal_temp_c": 25.0
    },
    "nonlinearity": {
      "enabled": true,
      "bypass_if_linear_profile": true
    },
    "binning": {
      "enabled": true
    },
    "defect_correction": {
      "enabled": true,
      "runtime_detection": false,
      "interpolation_mode": "bilinear",
      "advanced_mode": "none"
    },
    "ghost_correction": {
      "enabled": true,
      "max_tier": 3,
      "bypass_single_shot": true,
      "min_history_frames": 1
    },
    "mode": "clinical"
  }
}
```

### 7.2 Mode Presets

| Config Key | `"clinical"` | `"diagnostic"` | `"fluoro"` |
|------------|:------------:|:--------------:|:----------:|
| readout_validation | ON | OFF | ON |
| temp_compensation | ON (auto) | OFF | ON |
| offset_correction | **ON** | **ON** | **ON** |
| nonlinearity | ON (profile) | OFF | ON |
| gain_correction | **ON** | **ON** | **ON** |
| binning | ON (auto) | OFF | ON |
| defect_correction | ON | OFF | ON |
| ghost_correction | ON (Tier 1-3) | OFF | ON (Tier 2-3) |

---

## 8. Calibration Data Management

### 8.1 Required Calibration Files

| Data | File Format | Generated By | Load Function | Mandatory |
|------|------------|-------------|---------------|:---------:|
| Offset (Dark) Map | `.raw` / `.dcm` | `xpe_calib_generate_offset()` | `xpe_calib_load_offset()` | YES |
| Gain (Flat-field) Map | `.raw` / `.dcm` | External calibration tool | `xpe_calib_load_gain()` | YES |
| Bad Pixel Map (BPM) | `.raw` / `.dcm` | External detection tool | `xpe_calib_load_defect_map()` | YES |
| NLCSC Coefficients | JSON config | Calibration session | `xpe_ghost_create()` config | NO |
| Temperature LUT | JSON config | Factory calibration | `xpe_configure()` | NO |
| Nonlinearity Curves | JSON config | Factory calibration | `xpe_configure()` | NO |

### 8.2 Calibration Data Lifecycle

```
Factory Calibration ──> [offsetMap, gainMap, BPM, temp LUT, NL curves]
       |                               |
       |    Install at clinical site   |
       v                               v
Field Calibration ──> [field offset update, new BPM entries]
       |                               |
       |    Periodic QA / Drift        |
       v                               v
Runtime Monitoring ──> [drift detection, expiry check, recal alert]
       |                               |
       |    Annual / Emergency         |
       v                               v
Re-calibration ──> [full factory refresh OR field update]
```

### 8.3 Expiry and Drift Detection

| Trigger | Threshold | Action |
|---------|-----------|--------|
| Calibration file expired | `xpe_calib_check_expiry()` returns `XPE_ERR_CALIBRATION_EXPIRED` | Block pipeline startup |
| Temperature drift | `abs(current - reference) > 3.0 C` | Auto field dark update |
| Time elapsed | `> 30 minutes` since last calibration | Auto field dark update |
| Flat-field residual | `sigma/mean > 1.5%` | Emergency recalibration alert |
| SNR degradation | Outside 95% confidence interval | Recalibration recommended |

---

## 9. API Reference

### 9.1 Correction Functions

| Function | Stage | In-Place | Thread-Safe | Mandatory |
|----------|:-----:|:--------:|:-----------:|:---------:|
| `xpe_validate_readout_artifact()` | 0.5 | No (read-only) | Yes | No |
| `xpe_temp_compensate()` | 0.7 | Yes | Yes | No |
| `xpe_offset_correct()` | 1 | Yes | Yes | **Yes** |
| `xpe_nonlinearity_correct()` | 1.5 | Yes | Yes | No |
| `xpe_gain_correct()` | 2 | Yes | Yes | **Yes** |
| `xpe_binning_correct()` | 2.5 | Yes | Yes | No |
| `xpe_defect_correct()` | 3 | Yes | Yes | No |
| `xpe_defect_detect_runtime()` | 3 | No (output map) | Yes | No |
| `xpe_ghost_correct()` | 4 | Yes | Per-handle | No |

### 9.2 Ghost State Management

| Function | Purpose | Call Pattern |
|----------|---------|-------------|
| `xpe_ghost_create()` | Create corrector handle | Once at startup |
| `xpe_ghost_correct()` | Apply lag correction | Every frame |
| `xpe_ghost_reset()` | Clear exposure history | Between patients |
| `xpe_ghost_destroy()` | Free resources | At shutdown |

### 9.3 Calibration I/O

| Function | Purpose | Returns |
|----------|---------|---------|
| `xpe_calib_load_offset()` | Load dark map from file | `XPE_ERR_CALIBRATION_EXPIRED` if expired |
| `xpe_calib_load_gain()` | Load gain map from file | `XPE_ERR_CALIBRATION_EXPIRED` if expired |
| `xpe_calib_load_defect_map()` | Load BPM from file | `XPE_OK` |
| `xpe_calib_generate_offset()` | Generate offset from dark frames | `XPE_OK` |
| `xpe_calib_save()` | Save calibration with expiry | `XPE_OK` |
| `xpe_calib_check_expiry()` | Validate calibration freshness | `XPE_ERR_CALIBRATION_EXPIRED` |

---

## 10. Performance Budget

### 10.1 Per-Stage Time Allocation

| Stage | Budget (ms) | Estimated (ms) | Notes |
|-------|:-----------:|:--------------:|-------|
| (0) CalibManager | 200 | 200 | Startup only, excluded from per-frame |
| (0.5) Readout Validation | 15 | 10 | Read-only scan |
| (0.7) Temperature Comp | 10 | 5 | LUT lookup |
| (1) Offset Correction | 60 | 55 | Pixel-wise subtraction |
| (1.5) Nonlinearity | 25 | 20 | LUT/polynomial evaluation |
| (2) Gain Correction | 60 | 55 | Pixel-wise division + format conversion |
| (2.5) Binning Correction | 15 | 10 | Conditional multiplication |
| (3) Defect Correction | 110 | 95 | BPM scan + interpolation |
| (4) Ghost Tier 1 | 150 | 140 | Recursive deconvolution |
| (4) Ghost Tier 2 | +40 | +40 | Exposure-weighted selection |
| (4) Ghost Tier 3 | +90 | +90 | NLCSC full algorithm |
| **Pre-processing Total** | **500** | **~390 (T1)** | Hard ceiling: 500 ms/frame |

### 10.2 Memory Budget

| Component | Size | Notes |
|-----------|:----:|-------|
| Offset map (uint16) | 18.9 MB | 3072 x 3072 |
| Gain map (float32) | 37.7 MB | 3072 x 3072 |
| BPM (uint8) | 9.4 MB | 3072 x 3072 |
| Working buffer (float32) | 37.7 MB | Output frame |
| Ghost history (8 x float32) | 150 MB | Ring buffer |
| **Peak Total** | **~190 MB** | Phase 1 only |

---

## 11. Safety Constraints

### 11.1 Bypass Safety Rules (BYP-SAFE)

| ID | Rule | Rationale |
|----|------|-----------|
| BYP-SAFE-001 | Offset (1) SHALL NOT be bypassable via config | Dark bias always present |
| BYP-SAFE-002 | Gain (2) SHALL NOT be bypassable via config | Format conversion (uint16->float32) required by downstream |
| BYP-SAFE-003 | Bypassed stages SHALL NOT set their `XPE_FLAG_*` bit | Downstream and QA must know what was applied |
| BYP-SAFE-004 | Ghost bypass auto-triggers on first frame after reset | No history = garbage correction |
| BYP-SAFE-005 | Defect bypass with non-empty BPM SHALL emit warning alert | Skipping known defects is abnormal |
| BYP-SAFE-006 | Nonlinearity bypass requires explicit `panel.linear = true` | Silent skip risks undetected artifacts |
| BYP-SAFE-007 | All bypass decisions logged to diagnostic JSON | IEC 62304 traceability |
| BYP-SAFE-008 | Diagnostic mode: only (0), (1), (2) mandatory | Minimal correction for valid float32 output |

### 11.2 IEC 62304 Compliance Notes

- All calibration functions validate null pointers, buffer dimensions, and format before processing
- `XPE_ERR_CALIBRATION_EXPIRED` prevents use of stale calibration data
- Alert queue captures all anomalies without blocking image delivery
- Deterministic output: same binary + config + input = identical output hash
- No unbounded heap allocation in per-frame hot paths

---

## 12. References

### Standards

| Standard | Relevance |
|----------|-----------|
| IEC 62220-1-1:2015 | DQE measurement (validates offset/gain quality) |
| IEC 62494-1 | Exposure Index (requires detector-domain data) |
| IEC 62304:2006+A1:2015 | Software lifecycle (safety classification) |
| ISO 14971:2019 | Risk management |

### Research Papers

| Citation | Topic | Impact on This Module |
|----------|-------|----------------------|
| [Starman et al. 2012](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) | NLCSC lag correction | Tier 3 ghost algorithm |
| [Pang et al. 2006](https://pmc.ncbi.nlm.nih.gov/articles/PMC5722609/) | Lag vs ghosting model | Lag/ghost distinction |
| [Ranger et al. 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/) | Gain/offset SNR calibration | Drift detection thresholds |
| [Jeon et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC7930811/) | DL defect correction | Advanced defect repair |
| [FixPix 2023](https://arxiv.org/html/2310.11637v2) | MLP bad pixel correction | FixPix MLP architecture |
| [Wang 2013](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) | Duo-SID heel effect | Gain map heel compensation |
| [EP2148500A1](https://patents.google.com/patent/EP2148500A1/en) | Dynamic dark correction | Temperature/PREP time model |

### Project Documents

| Document | Path |
|----------|------|
| Algorithm Spec (normative) | `.moai/specs/xpe-algorithm-spec-deepsync.md` |
| Pipeline Spec | `.moai/project/pipeline-spec.md` |
| API Spec | `.moai/project/api-spec.md` |
| Calibration PRD | `docs/xray-fpd-research/xray-detector-calibration-prd.md` |
| Ghost Correction SRS | `docs/ghost-correction/srs_ghost_correction.md` |
| Cross-Verification Report | `.moai/specs/SPEC-XPE-MASTER/cross-verification-report.md` |

---

*End of Calibration Module README v1.0.0*
