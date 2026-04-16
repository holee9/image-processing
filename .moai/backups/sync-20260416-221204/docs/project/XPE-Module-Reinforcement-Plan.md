# XPE Module Reinforcement Plan

**Document ID**: XPE-REINFORCE-001  
**Version**: 1.3.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Objective

This document converts best-in-class image quality into a constrained reinforcement roadmap that still respects implementation reality and the medical-device claim boundary.

The roadmap is divided into:

- **Release-safe**: high-value, deterministic, benchmarkable improvements
- **Research-gated**: promising, but not yet release claims
- **Regulatory-hold**: do not fold into the product claim boundary in the current program

This revision also absorbs the score-lift path recorded in `XPE-Brainstorming-DeepSync-Execution.md`.

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

- Virtual-grid settings must be anatomy-aware and bounded.
- EI and DI should be used as detector-exposure management signals, not as patient-dose proxies.
- Ongoing QC is not a one-time acceptance event; artifact, reject, and exposure monitoring must remain in the operating loop.

---

## 3. Earth-Class Differentiators Worth Building

The strongest defensible differentiators in the current program are:

1. detector-domain stability across temperature, gain mode, and lag history,
2. deterministic premium enhancement that degrades safely when unavailable,
3. assistive AI that is transparent, restartable, and never the only path to a delivered image.

These are harder to market than speculative AI claims, but they create a stronger regulated product.

### 3.1 No-regret implementation accelerators

These items increase both image-quality ceiling and implementation feasibility:

- calibration manifest chain with session identity and integrity lock,
- scalar-reference plus SIMD parity harness for every major detector stage,
- sidecar quality-state vector instead of metadata overloading,
- class-aware defect router with small-model repair only for bounded cluster cases,
- deterministic premium router that can disable advanced stages cleanly,
- benchmark-first promotion rules before any premium stage is called complete.

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

### 6.1 Fast-track execution spine

The fastest route to a strong implementation is:

1. calibration manifest + detector-domain benchmark harness,
2. scalar reference kernels for preprocess stages,
3. SIMD parity and timing harness,
4. quality-state sidecar contract,
5. deterministic premium stages,
6. assistive AI worker architecture.

This order is intentionally different from feature-excitement-first planning. It is chosen to maximize proof, reuse, and downstream velocity.

### 6.2 Score-lift roadmap to 85 / 100

Assuming the project sits at **66 / 100** when `Phase 1b deterministic baseline` is first implemented, the preferred route to **85 / 100** is:

| Order | Reinforcement move | Expected uplift | Why it matters |
|---:|---|---:|---|
| 1 | preprocess reference kernels plus SIMD parity | +5 | closes the highest detector-quality risk safely |
| 2 | benchmark freeze and automated replay | +3 | turns quality wins into repeatable evidence |
| 3 | deterministic Phase 1b completion with measured budgets | +4 | converts architecture into a usable product |
| 4 | IEC package sync for baseline scope | +3 | removes the largest release-readiness debt |
| 5 | baseline collimation plus ROI-aware EI refinement | +3 | adds premium value without AI boundary expansion |
| 6 | reject-analysis and DI drift telemetry | +2 | improves operational maturity and field-quality readiness |
| 7 | selected deterministic premium refinement | +2 | raises image-quality ceiling without destabilizing the baseline |
| **Total** |  | **+22** | **66 -> 88 possible** |

The minimum practical subset to reach **85** is the first six items.

### 6.3 What not to substitute for the 85-point path

The following are not efficient substitutes for reaching 85:

- early assistive AI implementation without benchmark freeze,
- pathology-aware or dose-guidance features,
- visually aggressive virtual-grid defaults without observer review,
- optimized kernels without reference parity.

These can increase apparent sophistication while lowering actual delivery confidence.

---

## 7. Things Not To Do

- Do not tune virtual-grid defaults to the visually strongest setting without anatomy-specific review.
- Do not describe EI or DI as patient-dose estimators.
- Do not allow assistive AI output to overwrite the deterministic baseline silently.
- Do not promote pathology-aware or dose-guidance concepts through the same release path as classical image processing.
- Do not implement an optimized path before a scalar reference exists.
- Do not store ROI, AI confidence, or GSVG diagnostics by mutating generic image metadata fields.
