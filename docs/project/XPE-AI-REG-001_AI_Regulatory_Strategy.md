# AI Regulatory Strategy

**Document ID**: XPE-AI-REG-001  
**Version**: 1.1.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`  
**Parent**: `XPE-PRD-SYSTEM-001_System_Product_Requirements.md`, `XPE-MRD-001_Market_Requirements_Document.md`, `Regulatory-Feature-Boundary-Matrix.md`

---

## 1. Purpose

This document defines the regulatory and product-boundary strategy for Phase 3 AI features in XPE.

Its role is to keep AI development aligned with the current product claim boundary while still allowing controlled research and future promotion.

---

## 2. AI Scope in the Current Program

The current program allows only **assistive, non-blocking AI features**. These features may influence presentation or operator workflow, but they shall not:

- replace the deterministic baseline image,
- produce diagnosis or triage claims,
- produce patient-dose guidance claims,
- remove operator review or override capability.

### 2.1 In-scope assistive AI

| Feature | Classification | Allowed claim style |
|---|---|---|
| body-part recognition | assistive | automatic preset suggestion, operator-overridable |
| AI collimation refinement | assistive | suggested ROI or boundary, non-blocking |
| bone suppression | assistive | secondary visualization aid with original toggle |
| DL denoising | assistive | secondary noise-reduced output with labeling |

### 2.2 Out-of-scope or hold items

| Feature | Current status | Why it is held |
|---|---|---|
| pathology-aware enhancement | regulatory-hold | drifts toward lesion-performance claims |
| diagnosis or triage outputs | regulatory-hold | outside current claim boundary |
| ALARA advisor or dose recommendation | regulatory-hold | becomes dose-guidance logic |
| reject-workflow optimization claims | regulatory-hold | expands into workflow performance claims |

---

## 3. Intended Use Boundary

The AI features in XPE are intended only to assist image processing and visualization within an operator-supervised radiographic workflow.

They are not intended to:

- diagnose disease,
- prioritize cases,
- recommend treatment,
- replace physician interpretation,
- serve as the sole delivered output.

The deterministic baseline path remains the primary delivered image path at all times.

---

## 4. Mandatory Controls for Any AI Feature

Every AI feature in the current program shall satisfy all of the following:

1. worker isolation through `xpe_ai.dll` plus `xpe_ai_worker.exe`,
2. deterministic fallback to the non-AI baseline,
3. operator-visible labeling that the output is assistive,
4. model-version and confidence reporting,
5. documented out-of-distribution behavior,
6. documented training and evaluation provenance,
7. explicit promotion review before any claim expansion.

---

## 5. Transparency and Human-Factors Controls

### 5.1 Operator-visible information

For any assistive AI output, the host system shall be able to show:

- feature name,
- assistive status,
- model version,
- confidence or uncertainty indicator where relevant,
- baseline versus assistive output distinction,
- how to revert to the deterministic baseline.

### 5.2 Labeling rule

AI outputs shall be labelled in a way that does not imply autonomous diagnosis, autonomous prioritization, or dose guidance.

### 5.3 Human override rule

All AI-assisted decisions that influence processing presets or secondary outputs must remain operator-overridable or disableable.

---

## 6. Data and Model Governance

Each AI model release shall record:

- model identifier and version,
- training dataset manifest and hash,
- validation dataset manifest and hash,
- intended input population,
- known exclusions or out-of-distribution boundaries,
- evaluation report location,
- approval date and approver.

If a model is retrained, updated, or replaced, the resulting change shall be reviewed under the product change process before release.

---

## 7. Validation Expectations

### 7.1 Mandatory evidence

No AI feature may be treated as release-ready unless all of the following exist:

- model-performance evidence for the intended assistive task,
- degraded-mode and worker-resilience evidence,
- transparency and usability evidence,
- traceability to the relevant PRD and SVVP requirements.

### 7.2 Additional evidence for suppressive features

Bone suppression, DL denoising, and other nonlinear suppressive features require:

- task-based or observer-centered evidence,
- artifact review,
- parameter-sensitivity review,
- comparison against the deterministic baseline.

---

## 8. Regulatory Positioning

### 8.1 United States

The current strategy is to keep AI features inside an assistive image-processing boundary rather than a diagnosis-support claim boundary.

This requires:

- conservative claim language,
- explicit fallback,
- transparent operator-facing behavior,
- documented software and model maintenance.

### 8.2 Europe and other markets

Any market-specific classification that treats software image analysis more aggressively than the current U.S. interpretation shall be reviewed before shipment. The safest assumption is that AI-enabled medical-image functions may attract higher scrutiny than deterministic image processing.

### 8.3 Escalation trigger

Any of the following triggers a regulatory-boundary review:

- lesion-oriented or diagnosis-oriented claims,
- fully automatic processing claims without operator control,
- dose recommendation or exposure recommendation claims,
- materially changed model scope or intended use,
- new autonomous workflow actions.

---

## 9. Change Management and Post-Market Monitoring

Post-release AI monitoring shall review:

- feature usage rate,
- operator override rate,
- low-confidence or unknown-case frequency,
- worker crash or timeout rate,
- field complaints related to misleading or unsafe assistive output.

Any concerning drift or complaint pattern may trigger:

- model hold,
- rollback,
- documentation update,
- expanded evaluation before re-release.
