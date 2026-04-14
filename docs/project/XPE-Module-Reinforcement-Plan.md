# XPE Module Reinforcement Plan

**Document ID**: XPE-REINFORCE-001  
**Version**: 1.2.0  
**Date**: 2026-04-14  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Objective

This document converts best-in-class image quality into a constrained reinforcement roadmap that still respects implementation reality and the medical-device claim boundary.

The roadmap is divided into:

- **Release-safe**: high-value, deterministic, benchmarkable improvements
- **Research-gated**: promising, but not yet release claims
- **Regulatory-hold**: do not fold into the product claim boundary in the current program

---

## 2. Release-Safe Reinforcement Priorities

| Priority | Area | Reinforcement | Why it matters |
|---:|---|---|---|
| 1 | Calibration | dynamic dark + drift-aware refresh + stronger manifest integrity | prevents baseline instability |
| 2 | Gain / detector physics | multi-gain fit + heel-effect compensation + explicit linearity checks | improves detector-domain correctness |
| 3 | Defect handling | BPM quality rules + cluster-defect benchmark + FixPix-lite evaluation | closes visible artifact gaps safely |
| 4 | Lag / ghost | deterministic tier policy with bounded escalation | preserves throughput while improving temporal artifacts |
| 5 | Runtime quality | SIMD dispatch, tile processing, buffer reuse, failure telemetry | improves quality and reproducibility under time budgets |
| 6 | Presentation discipline | GSDF verification + DICOM-linked diagnostics | keeps output interpretable and auditable |

### 2.1 Research-backed design notes

- Virtual-grid settings must be anatomy-aware and bounded. Published chest and pelvic studies show that overly aggressive software grid ratios can damage useful anatomy even when average image-quality scores improve.
- EI and DI should be used as detector-exposure management signals, not as patient-dose proxies.
- Ongoing QC is not a one-time acceptance event; artifact, reject, and exposure monitoring must remain in the operating loop.

---

## 3. Earth-Class Differentiators Worth Building

The strongest defensible differentiators in the current program are:

1. detector-domain stability across temperature, gain mode, and lag history,
2. deterministic premium enhancement that degrades safely when unavailable,
3. assistive AI that is transparent, restartable, and never the only path to a delivered image.

These are harder to market than speculative AI claims, but they create a stronger regulated product.

---

## 4. Research-Gated Reinforcement

| Topic | Why it is attractive | Why it stays gated |
|---|---|---|
| body-part driven auto-parameter tuning | can improve default presets | can drift into opaque automation |
| AI collimation refinement | can improve difficult borders | requires confidence handling and failure transparency |
| bone suppression | strong clinical value in thorax workflows | needs dedicated evidence and claim review |
| DL denoising | dose-reduction value | can reshape subtle findings if poorly validated |
| learned scatter estimation | potential image quality gain | changes physical-model boundary and evidence burden |

---

## 5. Regulatory-Hold Topics

| Topic | Reason for hold |
|---|---|
| pathology-preserving enhancement claims | implies lesion-aware performance claims |
| ALARA recommendation logic | becomes dose guidance, not just image processing |
| repeat / reject workflow optimization | expands into workflow and quality-management claims |
| one-click fully automatic optimization | high automation burden and explainability risk |
| diagnosis-oriented confidence or triage claims | outside the current assistive imaging boundary |

---

## 6. Implementation Order

1. Finish deterministic detector-domain excellence.
2. Freeze benchmark pack manifests and thresholds.
3. Promote deterministic premium features.
4. Add assistive AI behind worker isolation.
5. Revisit research-gated features only after evidence is strong enough for boundary review.

---

## 7. Things Not To Do

- Do not tune virtual-grid defaults to the visually strongest setting without anatomy-specific review.
- Do not describe EI or DI as patient-dose estimators.
- Do not allow assistive AI output to overwrite the deterministic baseline silently.
- Do not promote pathology-aware or dose-guidance concepts through the same release path as classical image processing.
