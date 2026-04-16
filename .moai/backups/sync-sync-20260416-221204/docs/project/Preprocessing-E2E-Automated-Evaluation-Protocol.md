# Preprocessing E2E Automated Evaluation Protocol

**Document ID**: XPE-PRE-E2E-001  
**Version**: 1.0.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Scope**: `xpe_preprocess.dll`, Test GUI E2E, local raw/calibration fixtures  
**Linked Issue**: GitHub #12

---

## 1. Purpose

This protocol defines the automated evidence required to prove that preprocessing algorithms are not only callable, but measurably correct, stable, and safe on synthetic oracle data and local raw/calibration fixture data.

The goal is to make preprocessing development quality measurable from the first implementation sprint:

- every correction must expose a numeric before/after effect;
- every calibration effect must be traceable to the matching calibration context;
- every run must preserve the original raw input bytes;
- every result must be reproducible from a machine-readable report;
- every promotion claim must pass objective gates before GUI or visual review is accepted.

---

## 2. Evidence Basis

The evaluation bundle is grounded in these external references:

- EMVA 1288 sensor characterization terminology, especially DSNU and PRNU style uniformity measures: https://www.emva.org/standards-technology/emva-1288/
- IEC 62220-1-1 detector image-quality measurements and DQE context: https://webstore.iec.ch/en/publication/21937
- AAPM TG-151 ongoing digital radiography quality-control logic: https://pubmed.ncbi.nlm.nih.gov/26520756/
- AAPM TG-116 exposure indicator and detector-domain QA context: https://pmc.ncbi.nlm.nih.gov/articles/PMC3908678/
- Starman et al. lag correction and NLCSC basis: https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/
- Pang et al. lag/ghosting physics and distinction: https://pmc.ncbi.nlm.nih.gov/articles/PMC5722609/
- Jeon et al. defect correction with neural-network-assisted reconstruction: https://pmc.ncbi.nlm.nih.gov/articles/PMC7930811/
- FixPix bad-pixel correction as a deterministic/ML-assisted research reference: https://arxiv.org/html/2310.11637v2
- Wang heel-effect projection model for multi-SID gain behavior: https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf
- Kwan variable flat-field correction for exposure-dependent detector response: https://pubmed.ncbi.nlm.nih.gov/16532945/

These references are used as engineering evidence, not as a clinical-performance claim.

---

## 3. Input and Fixture Contract

### 3.1 Canonical input symbols

| Symbol | Meaning | Domain |
|---|---|---|
| `R(x,y)` | Raw detector frame | uint16 ADU |
| `D(x,y,T,tprep)` | Dark/offset map, optionally temperature and PREP-time interpolated | uint16 or float32 ADU |
| `F(x,y,E,SID)` | Flat-field frame or derived gain context | uint16 or float32 ADU |
| `G(x,y,E,SID)` | Normalized gain map | float32 |
| `B(x,y)` | Bad-pixel map, 0=good, nonzero=defect class | uint8 |
| `N(I)` | Nonlinearity LUT or polynomial | uint16/float32 ADU |
| `H_k(x,y)` | Previous-frame history for lag/ghost correction | float32 |
| `Y(x,y)` | Final preprocessing output | float32 |

### 3.2 Local fixture root

Local raw/calibration fixture payloads shall use:

```text
tests/test_data/calibration_cases/
```

Large raw payload files remain local-only and are intentionally ignored by Git. The repository tracks only fixture documentation, manifests, and future expected-output hashes.

### 3.3 Fixture directory contract

Each real case shall use:

```text
<case-id>/
  calibration/
    *.raw
  images/
    *.raw
```

The `calibration/` directory is the matching calibration context for every image under the same case. A test may intentionally mix cases only when validating mismatch detection.

---

## 4. Best-in-Class Preprocessing Algorithm Contract

### 4.1 Pipeline order

The release-safe preprocessing order shall be:

1. fixture and calibration manifest validation;
2. raw input immutability hash capture;
3. readout validation;
4. nonlinearity correction;
5. dynamic dark/offset correction;
6. gain/flat-field correction;
7. temperature and PREP-time residual compensation when metadata exists;
8. defect correction;
9. binning compensation when applicable;
10. lag/ghost correction with deterministic tier escalation;
11. metric extraction and report serialization.

Nonlinearity must precede gain correction. Gain correction remains the canonical `uint16 -> float32` boundary.

### 4.2 Calibration manifest validation

Every automated E2E run shall record:

- absolute fixture case ID;
- SHA-256 of every raw input and calibration file;
- inferred width, height, bytes per pixel, and endianness;
- calibration family assignment: dark, gain/flat, BPM, reference output, unknown;
- detector metadata used for temperature, exposure, SID, binning, and PREP-time logic;
- calibration mismatch policy result.

Missing metadata is allowed for research fixtures, but the report must mark the affected gates as `degraded_evidence=true`.

### 4.3 Dynamic dark and temperature model

Offset correction shall use the strongest available model:

```text
D_interp(x,y) =
  bilinear_or_linear_interpolate(D_i(x,y), T, tprep)
```

When only one dark map exists:

```text
I_dark(x,y) = max(R(x,y) - D_ref(x,y), 0)
```

When temperature metadata exists:

```text
D_T(x,y) = D_ref(x,y) * exp(-(Eg / (2*kB)) * (1/T - 1/T_ref))
```

The E2E report shall include dark-bias and DSNU-style residual metrics even when the temperature branch is bypassed.

### 4.4 Gain and flat-field model

The normalized gain map shall be derived or interpreted explicitly:

```text
F_dark(x,y) = max(F(x,y) - D_flat(x,y), epsilon)
G(x,y) = F_dark(x,y) / mean_roi(F_dark)
I_gain(x,y) = I_dark(x,y) / max(G(x,y), epsilon)
```

If the native calibration file stores reciprocal gain, the implementation shall record:

```text
I_gain(x,y) = I_dark(x,y) * G_recip(x,y)
```

The report must state `gain_semantics = normalized_gain | reciprocal_gain | unknown`. Unknown semantics may run exploratory tests but cannot pass release gates.

### 4.5 Defect correction model

The deterministic release path shall support:

- isolated defect: edge-aware 4/8-neighbor interpolation;
- line defect: directional interpolation orthogonal to the defect line;
- cluster defect: bounded median or plane-fit interpolation with max cluster size cap;
- research-gated path: FixPix-lite or CNN-assisted correction only when deterministic fallback and benchmark evidence exist.

The defect correction gate is based on residual error at defect locations and false modification of known-good pixels, not on visual review alone.

### 4.6 Lag and ghost correction model

Lag and ghost correction shall remain one preprocessing stage with internal tier escalation:

```text
Tier 1: multi-exponential LTI correction
Tier 2: exposure-weighted LTI correction
Tier 3: NLCSC for signal-dependent residuals
```

The stage shall emit tier, residual percentage, history length, and bypass reason. Empty history, first-frame operation, or single-shot mode shall be deterministic bypasses.

---

## 5. Automatic Evaluation Formula Pack

All metrics shall be computed on detector-domain images before presentation LUT, enhancement, or AI processing.

### 5.1 Pixel error metrics

```text
MAE(A,B)  = mean(abs(A - B))
RMSE(A,B) = sqrt(mean((A - B)^2))
PSNR(A,B) = 20 * log10(65535 / max(RMSE(A,B), epsilon))
MaxAbs(A,B) = max(abs(A - B))
```

Use `A=Y` and `B=Y_ref` when a golden output exists. Use synthetic oracle references for bit-exact unit/integration gates.

### 5.2 Dark and offset metrics

```text
DarkBias = mean(Y_dark_roi)
DSNU_ADU = std(Y_dark_roi)
DarkReduction_dB = 20 * log10(std(R_dark_roi) / max(std(Y_dark_roi), epsilon))
ClampRate = count(R - D < 0) / pixel_count
```

Acceptance defaults:

- `abs(DarkBias) <= 5 ADU` or `DarkReduction_dB >= 10 dB`;
- no underflow wraparound;
- `ClampRate` is reported and reviewed when it exceeds 5% outside synthetic clamp tests.

### 5.3 Gain and flat-field metrics

```text
PRNU_CV = std(Y_flat_roi) / max(mean(Y_flat_roi), epsilon)
FlatResidualPct = 100 * PRNU_CV
FPN_Reduction_dB = 20 * log10(std(R_flat_roi) / max(std(Y_flat_roi), epsilon))
LineArtifactScore = max(std(row_mean(Y)), std(col_mean(Y))) / max(std(tile_mean(Y)), epsilon)
```

Acceptance defaults:

- Phase 1 target: `FlatResidualPct <= 1.0%`;
- release-hardening target: `FlatResidualPct <= 0.5%`;
- `FPN_Reduction_dB >= 10 dB` for real fixture evidence when raw non-uniformity exists;
- line artifact score shall not increase by more than 10% from the pre-gain corrected image.

### 5.4 Nonlinearity metrics

```text
LinearityResidualPct = 100 * max(abs(S_corrected - S_ideal)) / ADC_full_scale
R2 = 1 - sum((S_corrected - S_ideal)^2) / sum((S_ideal - mean(S_ideal))^2)
MonotonicViolations = count(N(i+1) < N(i))
```

Acceptance defaults:

- LUT path: `LinearityResidualPct <= 0.3%`;
- polynomial path: `LinearityResidualPct <= 0.5%`;
- `MonotonicViolations = 0`.

### 5.5 Defect metrics

```text
DefectRecall = TP / max(TP + FN, 1)
DefectFPR = FP / max(FP + TN, 1)
DefectResidualADU = mean(abs(Y(defect_pixels) - neighbor_model(defect_pixels)))
GoodPixelDeltaP99 = percentile99(abs(Y(good_pixels) - Y_no_defect_stage(good_pixels)))
```

Acceptance defaults:

- synthetic BPM oracle: `DefectRecall = 100%`;
- synthetic false-positive rate: `DefectFPR < 0.001%`;
- good-pixel alteration is bounded by `GoodPixelDeltaP99 <= 1 ADU` unless a later stage intentionally changes all pixels.

### 5.6 Lag and ghost metrics

```text
LagResidualPct(k) = 100 * abs(mean(Y_blank_k) - mean(DarkRef)) / max(mean(ExposureSignal), epsilon)
GhostRemovalPct = 100 * (LagRawPct - LagResidualPct) / max(LagRawPct, epsilon)
HistoryContamination = similarity(previous_patient_history, current_first_frame)
```

Acceptance defaults:

- `GhostRemovalPct >= 90%` for benchmark sequences where lag is present;
- first frame after reset reports `ghost_bypassed=true`;
- no cross-patient history reuse is allowed.

### 5.7 Input preservation and determinism metrics

```text
InputPreserved = sha256(raw_before) == sha256(raw_after)
DeterminismRMSE = RMSE(Y_run_1, Y_run_2)
NaNInfCount = count(isnan(Y) or isinf(Y))
```

Acceptance defaults:

- `InputPreserved = true`;
- `DeterminismRMSE = 0` for CPU deterministic path or explicitly bounded if SIMD rounding differs;
- `NaNInfCount = 0`.

### 5.8 Performance metrics

```text
PipelineTimeMs = t_end - t_start
StageTimePct(stage) = stage_ms / max(PipelineTimeMs, epsilon)
PeakMemoryMB = max(process_working_set_delta)
ThroughputMPixPerSec = width * height / 1,000,000 / (PipelineTimeMs / 1000)
```

Acceptance defaults:

- 3072 x 3072 preprocessing: `PipelineTimeMs <= 500 ms`;
- stage timing is logged for every run;
- memory follows SRS-CALIB-PERF-002 unless the test explicitly enables a larger research-only history buffer.

---

## 6. Calibration Effect Score

The E2E evaluator shall compute a single summary score for dashboards, while still preserving all raw metrics.

```text
CES = 100 * (
  0.18 * DarkScore +
  0.18 * FlatScore +
  0.14 * DefectScore +
  0.12 * NonlinearityScore +
  0.12 * LagGhostScore +
  0.10 * ReferenceScore +
  0.08 * PreservationScore +
  0.08 * PerformanceScore
)
```

Each component is normalized to `[0,1]` by clamping measured performance against the corresponding acceptance target.

Promotion gates:

- `CES >= 85`: Phase 1 implementation completeness gate;
- `CES >= 92`: release-hardening target;
- any blocking safety failure sets `CES_CAP = 60` even if numeric image-quality scores are high;
- unknown gain semantics, missing raw preservation proof, or NaN/Inf output sets `CES_CAP = 70`.

The CES is not a clinical claim. It is an engineering readiness score.

---

## 7. E2E Test Modes

| Mode | Name | Purpose | Blocking |
|---|---|---|:---:|
| `PRE-E2E-0` | Fixture scan | Verify file presence, size, SHA-256, case/calibration pairing, and `.raw` Git ignore status | Yes |
| `PRE-E2E-1` | Synthetic oracle | Prove exact arithmetic for offset, gain, defect, nonlinearity, binning, and lag micro-cases | Yes |
| `PRE-E2E-2` | Real fixture calibration effect | Run local real raw/calibration cases and evaluate before/after detector-domain metrics | Yes |
| `PRE-E2E-3` | Reference-output comparison | Compare outputs against known reference frames such as `*_oc.raw` when semantics are confirmed | Conditional |
| `PRE-E2E-4` | GUI/native E2E | Drive the Test GUI or backend with real fixture paths and export the same metric report | Yes for GUI release |
| `PRE-E2E-5` | Mismatch negative test | Intentionally pair image and calibration from different cases and require a warning or hard failure | Yes |

---

## 8. Report Schema

Each run shall emit JSON and Markdown summaries.

```json
{
  "schema": "xpe-pre-e2e-report-v1",
  "git_sha": "<commit>",
  "case_id": "aed_shock_had1717mc",
  "image_path": "tests/test_data/calibration_cases/<case>/images/<file>.raw",
  "calibration_paths": ["..."],
  "raw_sha256_before": "...",
  "raw_sha256_after": "...",
  "input_preserved": true,
  "width": 3072,
  "height": 3072,
  "pixel_format": "uint16-le",
  "gain_semantics": "normalized_gain",
  "stages": [
    {"name": "offset", "enabled": "auto", "applied": true, "time_ms": 0.0},
    {"name": "gain", "enabled": "auto", "applied": true, "time_ms": 0.0}
  ],
  "metrics": {
    "dark_bias_adu": 0.0,
    "dsnu_adu": 0.0,
    "flat_residual_pct": 0.0,
    "fpn_reduction_db": 0.0,
    "defect_recall": 1.0,
    "rmse_reference": null,
    "psnr_reference_db": null,
    "pipeline_time_ms": 0.0,
    "peak_memory_mb": 0.0,
    "calibration_effect_score": 0.0
  },
  "gates": [
    {"id": "PRE-E2E-RAW-PRESERVE", "result": "pass"},
    {"id": "PRE-E2E-NO-NAN-INF", "result": "pass"}
  ],
  "degraded_evidence": false
}
```

---

## 9. Implementation Backlog

| Backlog ID | Work item | Output |
|---|---|---|
| `PRE-E2E-BL-001` | Add fixture scanner for `tests/test_data/calibration_cases` | JSON fixture manifest |
| `PRE-E2E-BL-002` | Add synthetic oracle generator for offset/gain/nonlinearity/defect/lag | deterministic raw + expected output |
| `PRE-E2E-BL-003` | Add preprocessing metric calculator | JSON + Markdown reports |
| `PRE-E2E-BL-004` | Add GUI/backend E2E driver hook | same report schema from app execution |
| `PRE-E2E-BL-005` | Add CI artifact policy for reports without raw payload upload | CI-safe evidence archive |
| `PRE-E2E-BL-006` | Add issue-comment summary formatter with `codex:` prefix | GitHub issue traceability |

---

## 10. Traceability

| Artifact | Linkage |
|---|---|
| SRS-CALIB-001 | Adds requirements SRS-CALIB-FUNC-015 through SRS-CALIB-FUNC-021 |
| TDS-CALIB-001 | Adds local fixture and E2E dataset contract |
| XPE-EVAL-001 | Imports formula pack and CES summary score |
| XPE-VVP-001 | Adds integration/system verification IDs for preprocessing E2E |
| `tests/test_data/calibration_cases/README.md` | Defines local fixture inventory and Git policy |

---

*Document End - XPE-PRE-E2E-001 v1.0.0*
