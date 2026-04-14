# X-Ray Image Processing Pipeline Specification

**Document ID**: PIPE-SPEC-001  
**Version**: 1.3.0  
**Date**: 2026-04-14  
**Project**: ImageProcTest — X-Ray Image Processing Engine (Modular DLL Architecture)  
**Changelog**: v1.1.0 -> v1.2.0: Deep research cross-verification. Enhanced pre-processing order research-validated. Multi-gain model integrated into stage (2). Tier 2/3 ghost escalation time budget clarified. Calibration drift detection stage notes added. EI-0 phase assignment resolved. v1.2.0 -> v1.3.0: Added Section 1A (Dependency Graph with data flow, dependency matrix, dependency types) and Section 1B (Bypass Policy with classification, decision flowchart, configuration interface, safety constraints, format boundary analysis, diagnostic mode).

---

## 1. Pipeline Overview

The C# GUI application `ImageProcTest` orchestrates a 17-stage image processing pipeline. Stages are implemented as modular DLLs loaded at runtime. Execution is divided into three phases gated by DLL availability.

Normative algorithm ownership, quality gates, and DeepSync conflict resolution are defined in `.moai/specs/xpe-algorithm-spec-deepsync.md`.

### 1.1 Standard Pipeline Sequence

```
Raw Frame
  -> (1)  Offset Correction
  -> (2)  Gain Correction
  -> (3)  Defect Correction
  -> (4)  Ghost Artifact Removal
  -> (5)  Log Transform
  -> (5a) Body Part Recognition      [Phase 3]
  -> (5b) Collimation Detection      [Phase 2 baseline, Phase 3 AI refinement]
  -> (6)  Noise Reduction
  -> (7)  Contrast Enhancement
  -> (8)  Edge Enhancement
  -> (9)  GSVG (Grid Suppression / Virtual Grid)  [Phase 2]
  -> (10) Multiscale Processing      [Phase 2]
  -> (11) Fractional Processing      [Phase 2]
  -> (12) Image Stitching            [Phase 3, conditional]
  -> (13) Bone Suppression           [Phase 3]
  -> (14) Modality LUT
  -> (15) VOI LUT
  -> (16) Presentation LUT
  -> (17) DICOM Write
```

### 1.2 Enhanced Pre-Processing Order

Research-validated ordering for pre-processing stages. Each stage validated against physical principles and peer-reviewed literature (see ALG-SPEC-001 v3.0.0-ds2 Section 4.2).

```
(0)   CalibManager Load              [startup-only, 200ms budget]
(0.5) Readout Artifact Validation    [non-mutating, flag + alert only]
(0.7) Temperature Compensation       [exponential dark current model, EP2148500A1]
(1)   Offset Correction              [I_corr = I_raw - I_dark, dynamic interpolation]
(1.5) Nonlinearity Correction        [BEFORE gain: linearize response for valid normalization]
(2)   Gain Correction                [flat-field normalization + multi-gain polynomial internal]
(2.5) Binning Correction             [conditional — only if binning mode active]
(3)   Defect Correction              [edge-aware interpolation, BPM-based + runtime detection]
(4)   Ghost Artifact Removal         [3-tier: LTI -> Exposure-Weighted -> NLCSC]
```

**Ordering rationale** (research-validated):
- Nonlinearity (1.5) BEFORE Gain (2): Gain normalization assumes linear detector response. Linearization must precede flat-field correction (NUC two-point calibration principle).
- Defect (3) AFTER Gain (2): Gain-corrected uniform background improves defect detection and interpolation quality.
- Ghost/Lag (4) LAST: NLCSC requires fully-corrected current frame for comparison against exposure history.

**Multi-gain model**: Multi-gain polynomial correction G(x,y,E) = sum(c_k * E^k) is INTERNAL to stage (2). Gain map selection by exposure level is handled within xpe_gain_correct(). Not a separate pipeline stage.

**Calibration drift monitoring**: Runtime calibration drift assessment (temperature delta, elapsed time, flat-field residual) executes asynchronously at stage (0) startup and periodically during idle. Does not add to per-frame latency.

---

## 1A. Pre-Processing Dependency Graph

### 1A.1 Data Flow and Dependency Chain

```
                          ┌──────────────────┐
                          │  (0) CalibManager │
                          │  Load (startup)   │
                          └──────┬───────────┘
                     ┌───────────┼───────────────────────┐
                     │           │                       │
              ┌──────▼──┐  ┌────▼────┐  ┌──────────────▼───────┐
              │offsetMap │  │ gainMap │  │ BPM (defectMap)      │
              └──────┬──┘  └────┬────┘  │ + NLCSC coefficients │
                     │          │       └──────────┬───────────┘
  ┌──────────────────┼──────────┼──────────────────┤
  │                  │          │                   │
  ▼                  │          │                   │
┌──────────────┐     │          │                   │
│ (0.5) Readout│     │          │                   │
│  Validation  │  [non-mutating, advisory]          │
│  uint16 → uint16   │          │                   │
└──────┬───────┘     │          │                   │
       │             │          │                   │
       ▼             │          │                   │
┌──────────────┐     │          │                   │
│ (0.7) Temp   │     │          │                   │
│ Compensation │  [temperature metadata required]   │
│  uint16 → uint16   │          │                   │
└──────┬───────┘     │          │                   │
       │             │          │                   │
       ▼             ▼          │                   │
┌──────────────────────┐        │                   │
│ (1) Offset Correct   │◄───offsetMap               │
│ I_corr = I_raw-I_dark│        │                   │
│  uint16 → uint16     │  [MANDATORY]               │
└──────┬───────────────┘        │                   │
       │                        │                   │
       ▼                        │                   │
┌──────────────────────┐        │                   │
│(1.5) Nonlinearity    │        │                   │
│ LUT/polynomial       │        │                   │
│  uint16 → uint16     │ [CONDITIONAL bypass]       │
└──────┬───────────────┘        │                   │
       │                        │                   │
       ▼                        ▼                   │
┌──────────────────────────────────┐                │
│ (2) Gain Correction              │◄───gainMap     │
│ I_corr = I_off / G(x,y)         │                │
│ uint16 → float32 FORMAT BOUNDARY │                │
│         [MANDATORY]              │                │
└──────┬───────────────────────────┘                │
       │                                            │
       ▼                                            │
┌──────────────────────┐                            │
│(2.5) Binning Correct │                            │
│  float32 → float32   │ [CONDITIONAL: binning only]│
└──────┬───────────────┘                            │
       │                                            │
       ▼                                            ▼
┌──────────────────────────────────┐
│ (3) Defect Correction            │◄───BPM
│ Interpolate bad pixels           │
│  float32 → float32              │  [CONDITIONAL bypass]
└──────┬───────────────────────────┘
       │
       ▼
┌──────────────────────────────────┐
│ (4) Ghost / Lag Correction       │◄───exposureHistory + NLCSC coefficients
│ 3-tier: LTI → Weighted → NLCSC  │
│  float32 → float32              │  [CONDITIONAL bypass]
└──────┬───────────────────────────┘
       │
       ▼
  [To Enhancement Domain: stage (5) Log Transform]
```

### 1A.2 Dependency Matrix

Each cell indicates whether the ROW stage depends on the COLUMN stage.

| Stage ↓ depends on → | (0) Calib | (0.5) Readout | (0.7) Temp | (1) Offset | (1.5) Nonlin | (2) Gain | (2.5) Bin | (3) Defect | (4) Ghost |
|----------------------|:---------:|:-------------:|:----------:|:----------:|:------------:|:--------:|:---------:|:----------:|:---------:|
| **(0) CalibManager** | — | | | | | | | | |
| **(0.5) Readout**    | | — | | | | | | | |
| **(0.7) Temp**       | | | — | | | | | | |
| **(1) Offset**       | DATA | | | — | | | | | |
| **(1.5) Nonlin**     | DATA | | | ORDER | — | | | | |
| **(2) Gain**         | DATA | | | ORDER | ORDER | — | | | |
| **(2.5) Binning**    | | | | | | FORMAT | — | | |
| **(3) Defect**       | DATA | | | | | FORMAT | | — | |
| **(4) Ghost**        | DATA | | | | | FORMAT | | ORDER | — |

Legend:
- **DATA**: Requires calibration data loaded by the column stage
- **ORDER**: Must execute after the column stage for correctness (physical constraint)
- **FORMAT**: Depends on the format conversion (uint16 -> float32) performed by stage (2)
- Empty: No dependency

### 1A.3 Dependency Types

| Type | Description | Violation Consequence |
|------|-------------|----------------------|
| **DATA** | Calibration data loaded at startup is required | Hard fail: `XPE_ERR_NOT_INITIALIZED` or `XPE_ERR_CALIBRATION_EXPIRED` |
| **ORDER** | Physical/mathematical constraint on execution sequence | Silent quality degradation: incorrect correction results |
| **FORMAT** | Data type boundary (uint16 -> float32) created by stage (2) | Hard fail: type mismatch crash or buffer corruption |
| **STATE** | Runtime-accumulated state (exposure history) | Graceful degradation: tier downgrade or bypass |

---

## 1B. Pre-Processing Stage Bypass (On/Off) Policy

### 1B.1 Bypass Classification

Each pre-processing stage is classified into one of three bypass categories:

| Category | Symbol | Description |
|----------|:------:|-------------|
| **MANDATORY** | `M` | Cannot be bypassed. Pipeline hard-fails without it. |
| **CONDITIONAL** | `C` | Can be bypassed under specific, documented conditions. |
| **ADVISORY** | `A` | Non-mutating stage. Bypass has no impact on image data. |

### 1B.2 Stage Bypass Table

| Stage | Category | Can Bypass? | Bypass Condition | Safety Impact | Flag on Bypass |
|-------|:--------:|:-----------:|------------------|:-------------:|---------------|
| **(0) CalibManager** | `M` | NO | N/A — pipeline cannot start | FATAL | N/A |
| **(0.5) Readout Validation** | `A` | YES | Always safe to skip | NONE | No flag set (validation skipped) |
| **(0.7) Temp Compensation** | `C` | YES | Temperature sensor unavailable OR detector at nominal temp (25C +/-2C) | LOW | `XPE_FLAG_TEMP_COMPENSATED` not set |
| **(1) Offset Correction** | `M` | NO | N/A — dark current bias corrupts all downstream | CRITICAL | Hard fail if offsetMap absent |
| **(1.5) Nonlinearity** | `C` | YES | Detector profile declares linear response (`panel.linear = true` in config) | LOW | `XPE_FLAG_NONLINEARITY_CORRECTED` not set |
| **(2) Gain Correction** | `M` | NO | N/A — provides uint16->float32 format conversion + pixel normalization | CRITICAL | Hard fail if gainMap absent |
| **(2.5) Binning** | `C` | YES | Binning mode inactive (`binningMode == 1` i.e. 1x1 native) | NONE | `XPE_FLAG_BINNING_CORRECTED` not set |
| **(3) Defect Correction** | `C` | YES | BPM is empty (zero defect pixels detected) OR diagnostic/raw-export mode | MEDIUM | `XPE_FLAG_DEFECT_CORRECTED` not set |
| **(4) Ghost Correction** | `C` | YES | Single-shot mode (no prior exposure) OR first frame after detector power-on OR exposure history empty | MEDIUM | `XPE_FLAG_GHOST_CORRECTED` not set |

### 1B.3 Bypass Decision Flowchart

```
For each pre-processing frame:

  (0) CalibManager loaded?
       NO  → ABORT pipeline startup
       YES ↓

  (0.5) Readout Validation enabled in config?
       NO  → SKIP (advisory, no image mutation)
       YES → Execute xpe_validate_readout_artifact()
              If artifactScore > CRITICAL_THRESHOLD → ABORT frame
              If artifactScore > WARN_THRESHOLD → Flag + continue
              Else → Continue ↓

  (0.7) Temperature sensor available?
       NO  → SKIP (use nominal 25C, emit alert)
       YES → Is abs(detectorTemp - nominalTemp) > 2.0C?
              NO  → SKIP (within tolerance, no correction needed)
              YES → Execute xpe_temp_compensate() ↓

  (1) Offset correction → ALWAYS EXECUTE
       offsetMap loaded? NO → HARD FAIL
       Execute xpe_offset_correct() ↓

  (1.5) Detector profile linear?
       YES → SKIP (panel.linear = true in config)
       NO  → Execute xpe_nonlinearity_correct() ↓

  (2) Gain correction → ALWAYS EXECUTE
       gainMap loaded? NO → HARD FAIL
       Execute xpe_gain_correct()
       [uint16 → float32 conversion happens here] ↓

  (2.5) binningMode == 1 (native)?
       YES → SKIP (no binning active)
       NO  → Execute xpe_binning_correct(binningMode) ↓

  (3) BPM has zero entries AND runtime detection disabled?
       YES → SKIP (no defects to correct)
       NO  → Execute xpe_defect_correct()
              If runtime detection enabled:
                xpe_defect_detect_runtime() → merge with static BPM ↓

  (4) Exposure history empty OR single-shot mode?
       YES → SKIP (no lag to correct, first frame)
       NO  → Execute xpe_ghost_correct()
              Auto-escalate Tier 1 → 2 → 3 as needed ↓

  → Continue to stage (5) Log Transform
```

### 1B.4 Bypass Configuration Interface

Bypass control is exposed through `xpe_configure()` JSON and per-frame metadata:

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
      "interpolation_mode": "bilinear"
    },
    "ghost_correction": {
      "enabled": true,
      "max_tier": 3,
      "bypass_single_shot": true,
      "min_history_frames": 1
    }
  }
}
```

### 1B.5 Bypass Safety Constraints

| Constraint ID | Rule | Rationale |
|--------------|------|-----------|
| **BYP-SAFE-001** | Offset correction (stage 1) SHALL NOT be bypassable via configuration. | Dark current bias is always present and corrupts all downstream processing. |
| **BYP-SAFE-002** | Gain correction (stage 2) SHALL NOT be bypassable via configuration. | Gain stage performs the critical uint16 -> float32 format conversion required by all downstream stages. |
| **BYP-SAFE-003** | When any CONDITIONAL stage is bypassed, the corresponding `XPE_FLAG_*` bit SHALL NOT be set in metadata. | Downstream stages and QA systems must be able to detect which corrections were applied. |
| **BYP-SAFE-004** | Ghost correction bypass SHALL automatically trigger on first frame after `xpe_ghost_reset()` or detector power-on. | No exposure history exists; attempting correction would produce garbage. |
| **BYP-SAFE-005** | Defect correction bypass SHALL emit a warning alert if BPM contains > 0 entries and bypass was user-requested. | Intentionally skipping known defect correction is abnormal and should be logged. |
| **BYP-SAFE-006** | Nonlinearity bypass SHALL only be allowed when the detector profile explicitly declares `panel.linear = true`. | Silent bypass without profile validation risks undetected nonlinearity artifacts. |
| **BYP-SAFE-007** | All bypass decisions SHALL be logged to the diagnostic JSON with stage name, reason, and frame ID. | Traceability for IEC 62304 compliance and post-hoc QA analysis. |
| **BYP-SAFE-008** | In diagnostic/raw-export mode, ALL stages except (0), (1), and (2) MAY be bypassed. Offset and gain remain mandatory. | Raw export still needs format conversion and baseline correction for valid output. |

### 1B.6 Format Boundary Impact

Stage (2) Gain Correction is the **sole format boundary** in the pre-processing pipeline:

```
  Stages (0.5) → (1.5):  uint16 domain
                          ───────────────
  Stage (2):              uint16 → float32 conversion  [FORMAT BOUNDARY]
                          ───────────────
  Stages (2.5) → (4):    float32 domain
```

This has critical implications for bypass:

- Stages (0.5), (0.7), (1), (1.5) operate on `uint16` data. Their bypass does not affect data format.
- Stage (2) CANNOT be bypassed because stages (2.5)-(4) require `float32` input. Even if gain normalization is not needed (hypothetical), the format conversion must occur.
- Stages (2.5), (3), (4) operate on `float32` data. Their bypass is format-safe.

### 1B.7 Diagnostic / Raw Export Mode

A special `raw_export` mode allows maximum bypass for debugging and QA:

| Stage | Normal Mode | Raw Export Mode |
|-------|:-----------:|:---------------:|
| (0) CalibManager | Mandatory | Mandatory |
| (0.5) Readout Validation | Configurable | SKIP |
| (0.7) Temperature Compensation | Configurable | SKIP |
| (1) Offset Correction | Mandatory | **Mandatory** (dark bias removal) |
| (1.5) Nonlinearity | Configurable | SKIP |
| (2) Gain Correction | Mandatory | **Mandatory** (format conversion) |
| (2.5) Binning | Conditional | SKIP |
| (3) Defect Correction | Configurable | SKIP |
| (4) Ghost Correction | Configurable | SKIP |

In raw export mode, only the two mandatory corrections (offset + gain) are applied, producing a minimally-corrected float32 frame suitable for external analysis tools.

---

## 2. ImageBuffer State Transitions

### 2.1 Format Transitions

| Stage Range | Buffer Format | Notes |
|-------------|--------------|-------|
| Raw Frame input | `uint16` | Sensor ADC output |
| After stage (2) Gain Correction | `float32` | Converted for floating-point arithmetic |
| After stage (16) Presentation LUT | `uint16` | Converted back for output/display |

### 2.2 Buffer Size

| Parameter | Value |
|-----------|-------|
| Maximum dimensions | 4096 x 4096 pixels |
| Typical dimensions | 3072 x 3072 pixels |
| float32 buffer size (3072x3072) | ~37.7 MB |
| uint16 buffer size (3072x3072) | ~18.9 MB |

### 2.3 Metadata Flags

The pipeline uses the stable `XPE_FLAG_*` bitfield defined in `xpe_types.h` / `api-spec.md`. Not every stage requires a persistent metadata bit; stage ordering in the orchestrator still prevents invalid re-entry for offset/log transforms.

| Flag | Set by Stage | Purpose |
|------|-------------|---------|
| `XPE_FLAG_READOUT_VALIDATED` | (0.5) Readout Validation | Raw frame integrity checked before correction |
| `XPE_FLAG_TEMP_COMPENSATED` | (0.7) Temperature Compensation | Temperature LUT/polynomial compensation applied |
| `XPE_FLAG_GAIN_CORRECTED` | (2) Gain | Float32 conversion and gain normalization complete |
| `XPE_FLAG_BINNING_CORRECTED` | (2.5) Binning | Binning-mode compensation applied |
| `XPE_FLAG_DEFECT_CORRECTED` | (3) Defect | BPM correction complete |
| `XPE_FLAG_GHOST_CORRECTED` | (4) Ghost | Ghost tier applied and recorded |
| `XPE_FLAG_COLLIMATION_DETECTED` | (5b) Collimation | ROI coordinates stored in metadata sidecar |
| `XPE_FLAG_STITCHED` | (12) Stitching | Multi-exposure merge complete |
| `XPE_FLAG_BONE_SUPPRESSED` | (13) Bone Suppression | Bone suppression result generated |
| `XPE_FLAG_GSVG_SKIPPED` | (9) GSVG fallback | SAFE-003 path taken, original buffer preserved, failure reason emitted via alert queue |

---

## 3. Branching Points

Six conditional branching points determine processing paths at runtime.

### BP-1: Body Part Recognition (Stage 5a)

- **Trigger**: Executes after Log Transform only when Phase 3 AI components are loaded.
- **Input**: Log-transformed float32 buffer.
- **Output**: Body part label (e.g., `CHEST`, `HAND`, `SPINE`) + confidence score.
- **Downstream effect**: Sets algorithm parameters for stages (6)–(11): noise reduction strength, contrast enhancement curve, edge sharpening coefficients.
- **Fallback**: If Phase 3 is unavailable or recognition fails, default parameter set is used.

### BP-2: Collimation Detection (Stage 5b)

- **Trigger**: Executes after Log Transform when Phase 2 DLLs are loaded. Phase 3 may refine the baseline ROI, but Phase 2 remains the required deterministic path.
- **Input**: Log-transformed float32 buffer.
- **Output**: ROI bounding box stored in an orchestration sidecar / result object, not in `XpeImageMetadata`.
- **Downstream effect**:
  - Exposure Index (EI) calculation restricted to ROI.
  - Display windowing limited to ROI area.
- **Fallback**: Full image used for EI and display if detection fails.

### BP-3: GSVG Grid Detection (Stage 9)

- **Trigger**: Executes within GSVG stage when Phase 2 DLLs are loaded.
- **Decision**:
  - Grid detected → `GridSuppression` path (removes anti-scatter grid artifact).
  - Grid not detected → `VirtualGrid` path (adds synthetic grid texture for display preference).
- **Input format**: Accepts `float32` or `uint16`; GSVG performs internal conversion as needed.
- **Error behavior**: On any GSVG processing error, the original unmodified buffer is returned. See GSVG SAFE-003.

### BP-4: Ghost Tier Escalation (Stage 4)

Three-tier ghost artifact removal with automatic escalation. Research-validated per Starman et al. 2012 (PMC3465354) and Pang et al. 2006 (PMC5722609).

| Tier | Algorithm | Trigger Condition | Performance Target | Time Budget |
|------|-----------|-------------------|-------------------|-------------|
| Tier 1 | LTI multi-exponential (N=4) deconvolution | Default path. Residual < threshold_1 | 1st frame lag < 0.5% | 150 ms |
| Tier 2 | Exposure-weighted LTI with intensity-matched coefficients | Tier 1 insufficient: artifact >= threshold_1 | 1st frame lag < 0.35% | 190 ms |
| Tier 3 | NLCSC with signal-dependent coefficients | Tier 2 insufficient: artifact >= threshold_2 | 1st frame lag <= 0.29% | 240 ms |

**Lag vs Ghosting**: Lag (signal persistence from charge trapping, 1-4% magnitude) and ghosting (sensitivity change from prior exposures, ~0.1% magnitude) are distinct phenomena corrected in the same stage. Lag dominates for indirect-conversion FPD at clinical doses.

- **State required**: `exposureHistory` (ring buffer of 8 prior frames, ~150 MB) + NLCSC correction coefficients.
- **Fallback**: Tier 1 always available; Tier 2/3 require sufficient exposure history entries.
- **Tier downgrade**: Must be explicit in diagnostics when exposure history or coefficients are insufficient.

### BP-5: Image Stitching (Stage 12)

- **Trigger**: Activated only when multi-exposure acquisition was performed and Phase 3 AI components are loaded.
- **Input**: Two or more float32 frame buffers from separate exposures.
- **Output**: Single merged float32 buffer with extended field of view.
- **Skip condition**: Single-exposure acquisition — stage is a no-op, buffer passes through unchanged.

### BP-6: DL Processing Toggle (Stage 13)

- **Reference**: SRS-SAFE-009.
- **Trigger**: User-controlled toggle in `ImageProcTest` GUI.
- **States**:
  - `DL_ENABLED`: Bone suppression inference executes using ONNX model.
  - `DL_DISABLED`: Stage 13 is skipped entirely; buffer passes through unchanged.
- **DLL requirement**: Phase 3 `xpe_ai.dll` must be loaded for `DL_ENABLED` to be operational.

---

## 4. Stateful vs. Stateless Stage Classification

### Stateful Stages

Stages that maintain internal state between calls or depend on external calibration data.

| Stage | State Dependency | State Type |
|-------|-----------------|------------|
| (1) Offset Correction | `calibMap` (offset calibration map) | Calibration file, loaded at startup |
| (2) Gain Correction | `calibMap` (gain calibration map) | Calibration file, loaded at startup |
| (3) Defect Correction | BPM (Bad Pixel Map) | Calibration file, loaded at startup |
| (4) Ghost Removal | `exposureHistory` ring buffer + NLCSC coefficients | Runtime accumulation |
| (5a) Body Part Recognition | ONNX model weights | Model file, loaded with Phase 2 |
| (9) GSVG | `scatterLUT` (scatter lookup table) | Calibration file, loaded with Phase 2 |
| (13) Bone Suppression | ONNX model weights | Model file, loaded with Phase 3 |
| (16) / Display | LUT presets (user-selectable) | Configuration, user-settable |

### Stateless Stages

Stages with no persistent state; output is a deterministic function of input only.

| Stage | Notes |
|-------|-------|
| (0.5) Readout Artifact Validation | Validates pixel patterns, no state |
| (0.7) Temperature Compensation | Uses sensor temperature reading from current frame metadata |
| (1.5) Nonlinearity Correction | Uses fixed polynomial coefficients from calibration load |
| (2.5) Binning Correction | Conditional; fixed correction factors |
| (5) Log Transform | Mathematical transform, no state |
| (5b) Collimation Detection | Stateless per-frame detection |
| (6) Noise Reduction | Filter-based, no state |
| (7) Contrast Enhancement | Curve-based, no state |
| (8) Edge Enhancement | Kernel-based, no state |
| (10) Multiscale Processing | Decomposition/reconstruction, no state |
| (11) Fractional Processing | Mathematical, no state |
| (12) Stitching | Combines frames, no persistent state |
| (14) Modality LUT | LUT application, no state |
| (15) VOI LUT | LUT application, no state |
| (17) DICOM Write | File I/O, no processing state |

---

## 5. Performance Budget

**Reference**: SRS-PERF-001, SRS-PERF-002

### 5.1 Phase Time Budgets

| Scope | Time Budget | Status |
|-------|------------|--------|
| Startup calibration load | 200 ms one-time | `CalibManager Load` is startup-only and excluded from per-frame latency budgets |
| Pre-processing subset (0.5–4) | 500 ms / frame | Estimated ~390 ms (Tier 1) / ~430–480 ms (Tier 2/3) |
| Phase 1 per-frame total | 3000 ms / frame | Estimated ~1005 ms (margin: ~1995 ms) |
| Phase 2 additions | Phase 1 + optional modules | Estimated ~2205–2505 ms total |
| Phase 3 additions | Phase 2 + AI inference | Estimated ~2655–2955 ms total |

### 5.2 Per-Stage Time Allocation

| Stage | Allocated (ms) | Phase 1 Estimate (ms) | Phase 2 Estimate (ms) |
|-------|---------------|----------------------|----------------------|
| (0) CalibManager Load | 200 | 200 (startup-only) | — |
| (0.5) Readout Artifact Validation | 15 | 10 | — |
| (0.7) Temperature Compensation | 10 | 5 | — |
| (1) Offset Correction | 60 | 55 | — |
| (1.5) Nonlinearity Correction | 25 | 20 | — |
| (2) Gain Correction | 60 | 55 | — |
| (2.5) Binning Correction | 15 | 10 | — |
| (3) Defect Correction | 110 | 95 | — |
| (4) Ghost Removal (Tier 1: LTI) | 150 | 140 | — |
| (4) Ghost Escalation Tier 2 (Exposure-Weighted) | +40 | +40 | — |
| (4) Ghost Escalation Tier 3 (NLCSC) | +90 | +90 | — |
| (5) Log Transform | 40 | 35 | — |
| (5a) Body Part Recognition | 300 | — | 280 |
| (5b) Collimation Detection | 140 | — | 130 |
| (6) Noise Reduction | 180 | 170 | — |
| (7) Contrast Enhancement | 130 | 120 | — |
| (8) Edge Enhancement | 90 | 80 | — |
| (9) GSVG | 400 | — | 380 |
| (10) Multiscale Processing | 250 | — | 230 |
| (11) Fractional Processing | 200 | — | 180 |
| (12) Stitching (conditional) | 300 | — | 0–300 |
| (13) Bone Suppression | 500 | — | 450 |
| (14) Modality LUT | 25 | 20 | — |
| (15) VOI LUT | 25 | 20 | — |
| (16) Presentation LUT | 25 | 20 | — |
| (17) DICOM Write | 150 | 150 | — |
| **Pre-processing subtotal (0.5–4)** | **500** | **~390** | — |
| **Phase 1 per-frame total** | **3000** | **~1005** | — |
| **Phase 2 total** | **Phase 1 + optional** | — | **~2205–2505** |
| **Phase 3 total** | **Phase 2 + AI** | — | **~2655–2955** |

---

## 6. GSVG Integration Point

### 6.1 Position in Pipeline

GSVG executes at stage (9), after basic image enhancement stages (6)–(8) and before advanced processing stages (10)–(11).

```
... -> (8) Edge Enhancement -> (9) GSVG -> (10) Multiscale -> ...
```

### 6.2 Interface

| Property | Value |
|----------|-------|
| Input formats | `float32` or `uint16` |
| Format handling | Internal conversion applied automatically |
| Output format | Matches input format |
| Grid detection | Automatic (determines GridSuppression vs VirtualGrid path) |
| Error behavior | Returns original unmodified buffer (GSVG SAFE-003) |

### 6.3 GSVG SAFE-003 Contract

On any internal error during GSVG processing:
1. Log the error with full context (frame ID, error type, timestamp).
2. Return the original `ImageBuffer` unmodified to the pipeline.
3. Set metadata flag `XPE_FLAG_GSVG_SKIPPED` with error code.
4. Pipeline continues normally — GSVG failure is non-fatal.

---

## 7. Phase-Gated Stage Loading

### 7.1 DLL Phase Assignment

| Phase | DLLs | Availability |
|-------|------|-------------|
| Phase 1 | `xpe_common.dll`, `xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` | REQUIRED — pipeline cannot start without these |
| Phase 2 | `gsvg.dll`, `xpe_enhance_advanced.dll` | OPTIONAL — graceful degradation if absent |
| Phase 3 | `xpe_ai.dll`, `xpe_ai_worker.exe` | OPTIONAL — graceful degradation if absent |

### 7.2 DLL Loading Strategy

```csharp
// Phase 1 — required, throw on failure
var common = NativeLibrary.Load("xpe_common.dll");
var preprocess = NativeLibrary.Load("xpe_preprocess.dll");
var enhanceBasic = NativeLibrary.Load("xpe_enhance_basic.dll");
var display = NativeLibrary.Load("xpe_display.dll");
var dicom = NativeLibrary.Load("xpe_dicom.dll");

// Phase 2 — optional, log and degrade
bool phase2Available = TryLoad("gsvg.dll", out var gsvg)
                    && TryLoad("xpe_enhance_advanced.dll", out var enhanceAdvanced);

// Phase 3 — optional, log and degrade
bool phase3Available = TryLoad("xpe_ai.dll", out var ai);
// When enabled, xpe_ai_init() launches the sandboxed companion worker xpe_ai_worker.exe.
```

### 7.3 Stage Execution per Phase Availability

| Stage | Phase 1 Only | Phase 1+2 | Phase 1+2+3 |
|-------|:-----------:|:---------:|:-----------:|
| (0)–(4) Pre-processing | Yes | Yes | Yes |
| (5) Log Transform | Yes | Yes | Yes |
| (5a) Body Part Recognition | Skip | Skip | Yes |
| (5b) Collimation Detection | Skip | Yes (baseline) | Yes (baseline + optional AI refinement) |
| (6)–(8) Basic Enhancement | Yes | Yes | Yes |
| (9) GSVG | Skip | Yes | Yes |
| (10)–(11) Advanced Enhancement | Skip | Yes | Yes |
| (12) Stitching | Skip | Skip | Yes (conditional) |
| (13) Bone Suppression | Skip | Skip | Yes (if DL_ENABLED) |
| (14)–(17) LUT + DICOM | Yes | Yes | Yes |

---

## 8. Appendices

### Appendix A: Interface Summary

| Interface ID | Name | Caller -> Callee | Key Parameters |
|-------------|------|-----------------|---------------|
| IF-INT-001 | PreprocessInterface | GUI -> xpe_preprocess.dll | `ImageBuffer*`, `CalibData*`, `PreprocessFlags` |
| IF-INT-002 | EnhanceBasicInterface | GUI -> xpe_enhance_basic.dll | `ImageBuffer*`, `EnhanceParams`, `BodyPartLabel` |
| IF-INT-003 | EnhanceAdvancedInterface | GUI -> xpe_enhance_advanced.dll | `ImageBuffer*`, `AdvancedParams`, `MultiscaleConfig` |
| IF-INT-004 | DisplayInterface | GUI -> xpe_display.dll | `ImageBuffer*`, `LUTPreset`, `WindowLevel` |
| IF-GSVG-001 | GSVGInterface | GUI -> gsvg.dll | `ImageBuffer*`, `ScatterLUT*`, `GSVGMode` |
| IF-AI-001 | AIInterface | GUI -> xpe_ai.dll -> xpe_ai_worker.exe | `ImageBuffer*`, `ONNXModelHandle`, `InferenceConfig` |

### Appendix B: Checksum Verification Points

Quality checksum verification is performed at five pipeline checkpoints to detect data corruption or processing errors.

| Checkpoint | Location | Verification Target |
|-----------|----------|-------------------|
| CK-1 | After raw frame acquisition | Raw pixel data integrity (CRC-32) |
| CK-2 | After Gain Correction (stage 2) | float32 buffer range check (no NaN/Inf values) |
| CK-3 | After Defect Correction (stage 3) | Bad pixel count within expected bounds |
| CK-4 | After Log Transform (stage 5) | Histogram distribution within expected range |
| CK-5 | After Presentation LUT (stage 16) | uint16 output range [0, 65535] fully populated |

### Appendix C: Memory Budget

Peak memory usage estimates across pipeline phases.

| Phase Configuration | Peak Memory Usage |
|--------------------|------------------|
| Phase 1 only | ~190 MB |
| Phase 1 + Phase 2 | ~440 MB |
| Phase 1 + Phase 2 + Phase 3 | ~740 MB |

**Memory breakdown factors:**
- Raw uint16 frame buffer: ~18.9 MB (3072x3072)
- Working float32 buffer: ~37.7 MB (3072x3072)
- Calibration maps (offset + gain + BPM): ~60 MB
- GSVG scatter LUT: ~50 MB
- Ghost exposure history (ring buffer, 8 frames): ~150 MB
- Body Part ONNX model: ~80 MB
- Bone Suppression ONNX model: ~200 MB
- DICOM output buffer + metadata: ~25 MB

---

*End of Pipeline Specification*
