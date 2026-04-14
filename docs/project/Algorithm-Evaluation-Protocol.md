# Algorithm Evaluation Protocol

**Document ID**: XPE-EVAL-001  
**Version**: 1.1.0  
**Date**: 2026-04-14  
**Status**: Controlled Draft

---

## 1. Purpose

This document defines how XPE algorithm revisions are compared, promoted, or held.

---

## 2. Source Basis

This protocol is aligned to:

- IEC 62494-1 and supporting EI literature for detector-domain exposure metrics
- DICOM PS3.14 GSDF for presentation-path conformance
- AAPM TG-151 for ongoing QC logic and artifact tracking
- AAPM TG-232 for site-derived DI action levels
- published virtual-grid observer and image-quality studies
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

## 5. AI Metrics

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

## 6. Decision Logic

| Decision class | Minimum evidence |
|---|---|
| release-safe promotion | benchmark pack + metrics + regression stability + operator review |
| research-gated continuation | benchmark pack + exploratory metrics + explicit non-release label |
| regulatory-hold | concept review only; no release thresholding |
