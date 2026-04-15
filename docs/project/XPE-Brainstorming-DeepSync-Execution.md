# XPE Brainstorming DeepSync Execution Plan

**Document ID**: XPE-BRAINSTORM-001  
**Version**: 1.1.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Purpose

This document converts the accumulated research set into a practical execution filter.

The goal is not to collect more ideas. The goal is to decide:

1. what actually improves image quality,
2. what remains implementable in this repository,
3. what can be validated with benchmark evidence,
4. what must stay outside the current release claim boundary.

---

## 2. Inputs Used for Brainstorming

The synthesis below was derived from:

- `xpe-algorithm-spec-deepsync.md`
- `Algorithm-Benchmark-Pack-Spec.md`
- `Algorithm-Evaluation-Protocol.md`
- `Regulatory-Feature-Boundary-Matrix.md`
- `XPE-Module-Reinforcement-Plan.md`
- `XPE-Implementation-Analysis-Report.md`
- `pipeline-spec.md`
- raw research captures in `XPE-PreProcess-DeepResearch.json` and `XPE-PostProcess-DeepResearch.json`

External anchors remain the same as the canonical set:

- DICOM GSDF
- IEC 62494-1
- IEC 62220-1-1
- AAPM TG-116 / TG-151 / TG-232
- published detector lag, defect correction, virtual-grid, and bone-suppression studies
- FDA GMLP and MLMD transparency guidance

---

## 3. Brainstorm Decision Matrix

| Idea | Quality upside | Implementation feasibility | Evidence strength | Boundary risk | Decision |
|---|---|---|---|---|---|
| calibration manifest chain + session lock | high | high | high | low | adopt now |
| exposure-stratified nonlinearity and gain models | high | medium | high | low | adopt now |
| geometry-aware heel compensation | medium-high | medium | medium-high | low | adopt now |
| class-aware defect routing + FixPix-lite for clusters | high | medium | medium-high | low-medium | adopt now with benchmark gate |
| lag residual-driven deterministic tiering | high | medium | high | low | adopt now |
| anatomy-bounded virtual-grid presets | medium-high | medium | medium-high | medium | adopt now with observer gate |
| ROI-aware EI re-invocation via sidecar ROI | medium | high | high | low | adopt now |
| stage-wise quality state vector | medium-high | high | medium | low | adopt now |
| scalar-reference plus SIMD parity harness | high | high | high | low | adopt now |
| worker-isolated assistive AI | medium-high | medium | high | medium | adopt now as gated architecture |
| full pathology-aware enhancement | uncertain | low | low-medium | high | do not promote |
| ALARA adviser / dose recommendation | uncertain | low | medium | very high | hold |
| fully automatic one-click optimization | medium | medium | low | high | hold |
| large-model end-to-end AI replacement of deterministic path | uncertain | low | low | very high | reject for current program |

---

## 4. Score-Lift Brainstorming: 66 to 85

The current planning assumption is:

- **66 / 100** for first implementation complete at `Phase 1b deterministic baseline`,
- **85 / 100** as the next meaningful target where the product becomes both strong and defensible.

The relevant question is therefore not ?œwhat sounds most advanced???but ?œwhat raises score, quality, and release credibility together???
### 4.1 Candidate uplift moves

| Move | Expected uplift | Why it is efficient | Decision |
|---|---:|---|---|
| scalar-reference plus SIMD parity for preprocess | +5 | improves correctness and keeps optimization safe | adopt now |
| benchmark manifest freeze plus automated result replay | +3 | turns quality into reproducible evidence | adopt now |
| complete deterministic Phase 1b delivery path | +4 | unlocks actual baseline product value | adopt now |
| baseline collimation plus ROI-aware EI refinement | +3 | adds premium value without AI boundary expansion | adopt now |
| regulated IEC package sync for baseline scope | +3 | closes major release-readiness debt | adopt now |
| reject-analysis and DI drift telemetry end-to-end | +2 | improves field readiness and operational score | adopt now |
| GSVG full inclusion before baseline is stable | +1 at best | upside exists, but timing risk is high too early | delay until Phase 1 stable |
| assistive AI before benchmark freeze | 0 or negative | increases novelty, but not reliable completion score | do not do first |
| pathology-aware enhancement early | negative | boundary risk too high | reject for current target |

### 4.2 Fastest credible 85-point bundle

The most credible bundle to move from **66** to **85** is:

1. finish `xpe_preprocess` with reference kernels and parity harness,
2. finish `xpe_enhance_basic`, `xpe_display`, and `xpe_dicom`,
3. freeze `BP-01` through `BP-10` manifests and automate replay,
4. synchronize baseline IEC package documents,
5. add baseline collimation and ROI-aware EI refinement as the first premium increment,
6. wire reject-analysis and DI drift telemetry through the host and evidence bundle.

This bundle is preferred over early AI because it raises:

- correctness,
- measured performance,
- release readiness,
- auditability,
- customer-visible premium value.

---

## 5. Highest-Value No-Regret Moves

These moves improve both quality and implementation feasibility.

### 5.1 Calibration integrity as first-class infrastructure

- Every offset, gain, BPM, nonlinearity, and lag coefficient pack shall carry a session identity and hash chain.
- Runtime shall reject mixed-session packs unless explicitly overridden for service diagnostics.
- Drift monitoring shall feed recalibration decisions rather than silently allowing quality erosion.

### 5.2 Detector-domain correctness before visual enhancement

- EI, DI, linearity, residual nonuniformity, lag residual, and defect burden are detector-domain quantities.
- Presentation tuning must never be allowed to hide detector-domain regression.
- Phase promotion must require detector-domain stability before display-side improvement claims.

### 5.3 Quality state vector instead of silent heuristics

For every processed frame, the runtime should produce a sidecar quality state with at least:

- calibration freshness state,
- detector correction residual state,
- defect burden class,
- lag tier used,
- GSVG used / skipped state,
- ROI confidence when EI refinement is invoked,
- AI worker status when applicable.

This should be a sidecar object or structured diagnostic log, not a set of overloaded image flags.

### 5.4 Deterministic router before premium logic

The strongest practical architecture is:

1. deterministic baseline always available,
2. deterministic premium stages opt-in only when prerequisites are satisfied,
3. assistive AI isolated and degradable,
4. explicit fallbacks with evidence.

This architecture is stronger than chasing the strongest-looking output on a single sample set.

---

## 6. Implementability-Maximizing Architecture Rules

### 6.1 Reference-first implementation

Every major stage shall have:

- one scalar reference implementation,
- one optimized implementation,
- one parity test harness,
- one benchmark family binding.

No AVX2, multithreaded, or AI path may become the only implementation.

### 6.2 Sidecar contracts over metadata mutation

The following outputs should travel as sidecars, not through `XpeImageMetadata` mutation:

- collimation ROI,
- EI refinement ROI,
- quality state vector,
- AI confidence and model identity,
- GSVG diagnostic reason.

This keeps ABI stable and reduces cross-module ambiguity.

### 6.3 Small-model policy for premium assistive logic

Where learned logic is allowed:

- prefer bounded small models,
- prefer ONNX CPU execution with quantized inference,
- require deterministic fallback,
- require versioned model manifests,
- require per-task disable controls.

### 6.4 Benchmark-first promotion

A feature is not treated as implemented for planning purposes until:

1. the code path exists,
2. the benchmark pack exists,
3. the evaluation rule exists,
4. the degraded mode exists,
5. the release boundary is explicitly assigned.

### 6.5 Memory and compute discipline

- Keep detector-domain stages tile-friendly and cache-aware.
- Reuse buffers across adjacent deterministic stages.
- Keep float32 as the canonical downstream detector-domain representation after gain correction.
- Avoid introducing large intermediate tensors into deterministic stages when a bounded filter or small MLP is sufficient.

---

## 7. Brainstormed Upgrades Worth Synchronizing Immediately

| Area | Immediate deep-sync action |
|---|---|
| algorithm spec | add implementation-feasibility rules and quality-state sidecar rules |
| reinforcement plan | promote no-regret priorities and explicit anti-patterns |
| implementation analysis | identify scaffolding that unlocks the most downstream work |
| sprint plan | insert non-negotiable scaffolding before Phase 1 feature expansion |
| master spec | reference this synthesis as planning input |

---

## 8. Anti-Patterns to Avoid

- Do not let display-side improvement hide detector-side regression.
- Do not introduce premium AI before scalar-reference and SIMD parity harnesses exist.
- Do not tune virtual-grid strength globally without anatomy-specific review.
- Do not treat EI or DI as dose surrogates in product language.
- Do not let raw research captures become de facto normative sources.

---

## 9. Definition of Earth-Class for This Program

For this repository, earth-class does not mean the most exotic algorithm list.

It means the product can prove:

1. detector-domain stability,
2. strong deterministic baseline quality,
3. premium deterministic features that remain bounded,
4. assistive AI that never breaks delivery,
5. reproducible benchmark evidence,
6. a release path that is still implementable by the current codebase.

That is the strongest credible target for this program.
