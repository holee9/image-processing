# Regulatory Feature Boundary Matrix

**Document ID**: XPE-REG-BOUNDARY-001  
**Version**: 1.1.0  
**Date**: 2026-04-14  
**Status**: Controlled Draft

---

## 1. Purpose

This matrix separates features that may ship inside the current product claim boundary from features that remain research or hold items.

---

## 2. Boundary Matrix

| Feature group | Classification | Claim style | Deployment rule |
|---|---|---|---|
| detector correction, EI baseline, display LUT, DICOM export | release-safe | deterministic image processing | part of deterministic baseline |
| baseline collimation, ROI EI refinement, GSVG, multiscale, fractional | release-safe if benchmarked | deterministic premium processing | optional deterministic premium |
| body-part recognition, AI collimation refinement, bone suppression, DL denoise | research-gated assistive | assistive / advisory only | optional, clearly labelled, degradable |
| pathology-aware enhancement, ALARA advisor, repeat/reject analytics, one-click automatic optimization, diagnosis-oriented outputs | regulatory-hold | diagnostic, dose-guidance, or workflow-optimization claim | do not claim or ship in current release boundary |

---

## 3. Promotion Criteria

| Classification | Minimum criteria before promotion |
|---|---|
| release-safe | deterministic behavior, benchmark evidence, documented fallback, operator-facing description |
| research-gated assistive | worker isolation, confidence reporting, transparency notes, degraded-mode proof |
| regulatory-hold | separate claim strategy and separate approval path required |

---

## 4. Mandatory Controls for Research-Gated AI

- worker isolation
- confidence reporting
- degraded-mode fallback
- operator-visible labeling
- documented training and evaluation scope

---

## 5. Transparency and Human-Factors Rule

Any assistive AI feature must provide clear information about:

- intended use,
- when it may fail,
- what fallback is used,
- which output is baseline versus assistive.

This follows the direction of FDA GMLP and transparency guidance for ML-enabled medical devices.

---

## 6. Escalation Rule

Any feature moving from research-gated to release-safe requires:

1. benchmark evidence,
2. evaluation report,
3. documentation update across `product`, `pipeline`, `api`, and UAT plans,
4. regulatory sign-off.
