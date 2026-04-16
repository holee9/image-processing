# Algorithm Evaluation Protocol

**Document ID**: XPE-EVAL-001  
**Version**: 1.4.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft

---

## 1. Purpose

This document defines how XPE algorithm revisions are compared, promoted, or held.

---

## 2. Source Basis

This protocol is aligned to:

- EMVA 1288 DSNU/PRNU terminology for detector uniformity measurement
- IEC 62220-1-1 detector image-quality and DQE context
- IEC 62494-1 and supporting EI literature for detector-domain exposure metrics
- DICOM PS3.14 GSDF for presentation-path conformance
- AAPM TG-151 for ongoing QC logic and artifact tracking
- AAPM TG-232 for site-derived DI action levels
- AAPM TG-305 for reject-analysis data structure and field review
- published virtual-grid observer and image-quality studies
- task-based image-quality literature for nonlinear algorithms
- FDA GMLP and transparency guidance for assistive AI reporting

---

## 3. Detector-Domain Metrics

| Metric | Use |
|---|---|
| residual dark bias | offset and temperature compensation |
| flat-field residual | gain and nonlinearity correction |
| linearity error | multi-gain and response correction |
| lag residual | temporal artifact removal |
| ghost residual | sensitivity-memory artifact removal |
| bad-pixel residual | defect correction |
| EI / DI error | exposure index validation |

### 3.1 Detector-domain rule

Detector-domain metrics shall be computed before presentation LUT application and before any assistive AI transformation.

### 3.2 EI / DI operational rule

- EI shall not be used as a patient-dose surrogate.
- DI should target a site mean near `0.0`.
- Operational action bands should be derived from site data, with tighter review around `±1` and escalation around `±2` standard deviations of local DI behavior.

### 3.3 Preprocessing E2E automatic evaluation formula pack

The normative preprocessing formula pack is `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`.

Minimum detector-domain preprocessing metrics:

| Metric | Formula or source | Use |
|---|---|---|
| `MAE` | `mean(abs(Y - Ref))` | reference-output comparison |
| `RMSE` | `sqrt(mean((Y - Ref)^2))` | reference-output comparison |
| `PSNR` | `20 * log10(65535 / max(RMSE, epsilon))` | synthetic/golden output gate |
| `DarkBias` | `mean(Y_dark_roi)` | offset residual |
| `DSNU_ADU` | `std(Y_dark_roi)` | dark spatial non-uniformity |
| `PRNU_CV` | `std(Y_flat_roi) / mean(Y_flat_roi)` | flat-field residual |
| `FPN_Reduction_dB` | `20 * log10(std(R_flat_roi) / std(Y_flat_roi))` | before/after fixed-pattern improvement |
| `DefectRecall` | `TP / (TP + FN)` | BPM oracle detection/correction |
| `DefectFPR` | `FP / (FP + TN)` | good-pixel preservation |
| `LagResidualPct` | `100 * abs(mean(Y_blank_k) - DarkRef) / ExposureSignal` | lag/ghost residual |
| `InputPreserved` | `sha256(raw_before) == sha256(raw_after)` | safety and audit preservation |
| `Calibration Effect Score` | weighted dark/flat/defect/nonlinearity/lag/reference/preservation/performance score | sprint dashboard |

Promotion gates:

- `PRE-E2E-0` fixture scan, `PRE-E2E-1` synthetic oracle, and `PRE-E2E-2` real fixture calibration-effect checks are mandatory for Phase 1 preprocessing.
- `PRE-E2E-3` reference-output comparison is mandatory when a semantically confirmed reference output exists.
- `PRE-E2E-4` GUI/native execution is mandatory before a user-facing GUI validation claim.
- `PRE-E2E-5` mismatch negative testing is mandatory before calibration auto-selection is considered safe.
- `CES >= 85` is the Phase 1 completeness target; `CES >= 92` is the release-hardening target.
- Any raw mutation, NaN/Inf output, or unknown gain semantics is a blocking safety issue regardless of aggregate score.

---

## 4. Enhancement and Presentation Metrics

| Metric | Use |
|---|---|
| CNR | enhancement quality |
| edge overshoot / ringing | sharpening safety |
| artifact score | grid, defect, and denoise side effects |
| GSDF conformance | presentation path |
| interactive latency | VOI and display usability |

### 4.1 Virtual-grid evaluation rule

Virtual-grid quality shall not be judged by one scalar metric alone. The minimum evaluation bundle is:

- observer or expert review,
- an objective image-quality score,
- anatomy-specific artifact review,
- parameter-sensitivity sweep.

High virtual-grid ratios or aggressive settings that visibly degrade anatomy are not promotable to release defaults.

---

## 5. Task-Based and Observer-Centered Metrics

The following evidence class is mandatory for nonlinear or suppressive algorithms when anatomical visibility or lesion visibility may change materially:

| Metric or evidence | Use |
|---|---|
| task-based detectability figure of merit | structured comparison of algorithm impact on a defined imaging task |
| observer or expert review | anatomy-preservation and artifact review |
| sensitivity to parameter sweep | prevents promotion of brittle defaults |
| baseline-versus-assisted comparison | ensures gains are not created by hiding baseline information |

### 5.1 Mandatory scope

This evidence class applies at minimum to:

- virtual-grid default promotion,
- bone suppression,
- DL denoising with clinical-use intent,
- any future learned scatter or anatomy-aware enhancement.

---

## 6. AI Metrics

| Metric | Use |
|---|---|
| classification accuracy and calibration | body-part recognition |
| IoU / boundary error | collimation refinement |
| PSNR / SSIM plus artifact review | bone suppression and denoise |
| crash recovery rate | worker resilience |

### 5.1 AI reporting rule

AI evaluation reports shall always include:

- model version,
- dataset scope,
- confidence behavior,
- fallback success rate,
- transparency notes visible to the operator or reviewer.

### 5.2 AI promotion rule

No AI metric may be used as a release promotion argument unless the corresponding degraded-mode tests also pass.

---

## 7. Operational and Field Metrics

| Metric | Use |
|---|---|
| DI distribution drift | exposure-management review |
| reject rate and repeat rate | ongoing field QC |
| reject-reason taxonomy completeness | telemetry usefulness and comparability |
| AI override rate | trust and operational-fit monitoring |
| GSVG skip frequency | premium-stage robustness |

### 7.1 Operational rule

Operational metrics are not substitutes for lab benchmarks, but they are mandatory for post-release evidence that the system remains stable in practice.

---

## 8. Decision Logic

| Decision class | Minimum evidence |
|---|---|
| release-safe promotion | benchmark pack + metrics + regression stability + operator review |
| research-gated continuation | benchmark pack + exploratory metrics + explicit non-release label |
| regulatory-hold | concept review only; no release thresholding |

---

## 9. Signal Detection Theory (SDT) Evaluation Framework

Signal detection theory provides the gold-standard evaluation framework for visual task performance in X-ray imaging.

### 9.1 Mandatory SDT metrics

| Metric | Definition | Use |
|---|---|---|
| d' (d-prime) | Separation of signal and noise distributions in standard deviation units | Overall detectability index |
| AUC (area under ROC curve) | Probability that a random signal case scores higher than a random noise case | Classification performance |
| JAFROC FOM | Fraction of signal-present cases correctly ranked above all noise marks | Free-response (lesion-level) performance |
| d'_task | MTF²(f) × T(f)² / NPS(f) integrated over frequency | Physics-based task detectability |

### 9.2 Application scope

SDT d'_task computation is **mandatory** for:

- all algorithms with confirmed visual impact on lesion or structure visibility,
- nonlinear or suppressive processing when anatomical visibility may change materially,
- comparative evaluation between algorithm revisions where a visual quality claim is made.

JAFROC analysis is **mandatory** for:

- any algorithm where a lesion-detection clinical claim is intended (even informational).

### 9.3 SDT evaluation rule

SDT metrics are computed using §11.6 (SDT Framework, GAP-CF). Results are logged with `informational_only = true` and are not used as standalone release arguments. SDT results supplement but do not replace BP-11 task-based and observer-centered assessment packs.

### 9.4 Reference implementation

The reference Python implementation for d', ROC AUC, and JAFROC FOM is defined in §11.6 of XPE-ALG-001 v1.8. C++ runtime API: `xpe_sdt_compute_dprime_roc()` and `xpe_sdt_compute_jafroc()` in `xpe_enhance_advanced.dll`.
