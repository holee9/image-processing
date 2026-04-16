# XPE Algorithm Specification (DeepSync)

**Document ID**: ALG-SPEC-001  
**Version**: 3.3.0-ds5  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Purpose

This document is the normative algorithm contract for XPE. It converts research findings into release-safe rules, performance targets, and fallback behavior that the codebase can implement and validate.

This revision (ds5) adds preprocessing E2E proof contracts, detector-domain automatic evaluation formulas, local raw/calibration fixture gates, and Calibration Effect Score promotion rules. Previous revision (ds4) added implementation-feasibility rules, sidecar contract guidance, and benchmark schema gate. Previous revision (ds3) resolved DeepSync inconsistencies by:

- removing the duplicate `SWU-2.0` EI identifier,
- aligning all normative references to `docs/project/`,
- dividing advanced features into release-safe, research-gated, and regulatory-hold tiers,
- binding every premium claim to a benchmark or evidence requirement.

---

## 2. Normative Source Base

### 2.1 Internal canonical sources

- `product.md`
- `structure.md`
- `pipeline-spec.md`
- `api-spec.md`
- `SPEC-XPE-MASTER.md`
- `XPE-Module-Reinforcement-Plan.md`
- `XPE-Brainstorming-DeepSync-Execution.md`
- `Algorithm-Benchmark-Pack-Spec.md`
- `Algorithm-Evaluation-Protocol.md`
- `Preprocessing-E2E-Automated-Evaluation-Protocol.md`
- `Regulatory-Feature-Boundary-Matrix.md`

`.moai/project/` and `.moai/specs/` are not normative for this document.

### 2.3 Implementation Detail Reference

This document establishes the **algorithm contract** (what each algorithm must achieve, boundary conditions, and quality gates). For the corresponding **implementation detail** (mathematics, C++ pseudocode, SIMD strategy, and calibration code), refer to:

> **XPE-ALG-001** Unified Algorithm Development Specification  
> Path: `docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md`  
> Version: v1.1 (2026-04-15)  
> IEC 62304 Clause: 5.4 (Software Detailed Design)  

Relationship between this document and XPE-ALG-001:

| Concern | This document (ALG-SPEC-001) | XPE-ALG-001 |
|---------|------------------------------|-------------|
| Algorithm selection rationale | Normative | Informational reference |
| Release-safe / research-gated boundary | Normative | Follows this document |
| Mathematical derivation | Summary level | Full derivation with LaTeX |
| C++ implementation detail | Not provided | Full AVX2/SIMD code |
| Python calibration code | Not provided | Complete implementations |
| Verification criteria (unit test) | Quality gate targets | Test tables per algorithm |
| Phase 2 algorithm detail (GSVG, NSCT, collimation) | Contract level | GAP-D/N detailed in XPE-ALG-001 |

### 2.2 External technical references

- DICOM PS3.14 GSDF: https://dicom.nema.org/medical/dicom/current/output/chtml/part01/sect_6.14.html
- EMVA 1288 sensor characterization: https://www.emva.org/standards-technology/emva-1288/
- IEC 62494-1 EI scope: https://webstore.iec.ch/en/publication/7107
- IEC 62220-1-1 DQE determination: https://webstore.iec.ch/en/publication/21937
- AAPM TG-116 exposure indicator summary: https://pmc.ncbi.nlm.nih.gov/articles/PMC3908678/
- AAPM TG-151 ongoing DR quality control: https://pubmed.ncbi.nlm.nih.gov/26520756/
- Starman et al. NLCSC lag correction: https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/
- Pang et al. lag vs ghosting: https://pmc.ncbi.nlm.nih.gov/articles/PMC5722609/
- Jeon et al. defect correction CNN study: https://pmc.ncbi.nlm.nih.gov/articles/PMC7930811/
- FixPix bad-pixel correction: https://arxiv.org/html/2310.11637v2
- Wang 2013 heel-effect Duo-SID: https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf
- Kwan 2006 variable flat-field correction: https://pubmed.ncbi.nlm.nih.gov/16532945/
- Dual-gain calibration model: https://pubmed.ncbi.nlm.nih.gov/17926969/
- Virtual-grid observer study: https://pubmed.ncbi.nlm.nih.gov/29713231/
- Bone suppression feasibility study: https://pubmed.ncbi.nlm.nih.gov/34888191/
- FDA GMLP principles: https://www.fda.gov/medical-devices/software-medical-device-samd/good-machine-learning-practice-medical-device-development-guiding-principles
- FDA MLMD transparency principles: https://www.fda.gov/medical-devices/software-medical-device-samd/transparency-machine-learning-enabled-medical-devices-guiding-principles

---

## 3. DeepSync Invariants

| Topic | Canonical decision |
|---|---|
| EI unit ID | `SWU-2.10` only |
| EI baseline | Phase 1b, whole-image, detector-domain |
| EI refinement | Phase 2, same API re-invoked on ROI-cropped detector-domain image |
| Collimation ROI storage | orchestration sidecar, not `XpeImageMetadata` |
| Flags | state-only bitfield |
| GSVG failures | alert or diagnostic JSON, never encoded inside flags |
| AI | assistive, degradable, worker-isolated |
| Body part recognition | Phase 3 only |
| Image stitching | Phase 3 only |
| Detector metrics | computed only on detector-domain images |

---

## 4. Release-Safe Algorithm Stack

### 4.1 Detector correction

| Area | Release-safe baseline | Advanced but still release-safe |
|---|---|---|
| Readout validation | saturation, clipped DR, impossible geometry, row/column faults | periodic noise and EMI heuristics if false-positive rate is bounded |
| Offset / dark | dynamic dark interpolation with temperature and PREP time | drift-aware refresh scheduling |
| Nonlinearity | monotonic LUT or low-order polynomial | mode-specific family selection |
| Gain / flat-field | two-point or multi-point normalization | multi-gain polynomial fit, Duo-SID heel-effect projection |
| Defect correction | BPM + edge-aware interpolation | FixPix-lite MLP only if deterministic, bounded, and benchmarked |
| Lag / ghost | tiered LTI -> weighted -> NLCSC | exposure-dependent coefficient selection with deterministic caps |

### 4.2 Enhancement baseline

| Area | Release-safe baseline |
|---|---|
| Log transform | deterministic, invertible where needed |
| Noise reduction | deterministic bilateral / NLM family with bounded latency |
| Contrast | bounded CLAHE or equivalent local contrast enhancement |
| Edge | bounded unsharp masking or equivalent |
| Display | modality, VOI, presentation LUT with GSDF-aligned presentation |
| DICOM | deterministic DX export and network SCU |

### 4.3 Deterministic premium layer

These features may ship in Phase 2 if they stay deterministic and benchmarked:

- baseline collimation detection,
- ROI-aware EI refinement,
- multiscale processing,
- fractional processing,
- GSVG / virtual grid.

---

## 5. Research-Gated and Regulatory-Hold Stack

### 5.1 Research-gated

These features may be developed and benchmarked, but cannot graduate to release claims without explicit boundary review:

- AI collimation refinement
- body-part recognition-driven auto-parameter proposals
- DL denoising
- bone suppression
- advanced cluster-defect correction beyond FixPix-lite
- learned scatter estimation

### 5.2 Regulatory-hold

These remain outside the current product claim boundary:

- pathology-preserving or pathology-aware enhancement claims
- dose recommendation or ALARA advisor
- repeat / reject workflow optimization claims
- one-click fully automatic clinical optimization claims
- diagnosis-oriented confidence or triage claims

---

## 6. Algorithm Contracts by Workstream

### 6.1 Temperature, offset, and dark current

- Dynamic dark correction shall use temperature and PREP-time aware interpolation where metadata exists.
- Sensor failure shall trigger degraded compensation and an alert, not a silent skip.
- Release gate: post-correction mean dark bias < 5 ADU and bounded residual drift across the temperature sweep benchmark pack.

### 6.2 Gain, nonlinearity, and heel effect

- Nonlinearity correction must precede gain correction.
- Gain correction is the canonical `uint16 -> float32` boundary for downstream detector processing.
- Multi-gain calibration may be internal to gain correction and must not create a separate pipeline stage.
- Variable flat-field or exposure-dependent correction is preferred over a single fixed flat field when detector response is measurably nonlinear across the operating range.
- Heel-effect adaptation must be benchmarked across acquisition geometry and SID, not only on a single calibration distance.
- Release gate: flat-field residual and linearity error must satisfy the benchmark pack thresholds.

### 6.3 Defect correction

- Baseline release path is BPM plus deterministic interpolation.
- FixPix-lite or other small models are allowed only if they remain bounded, calibratable, and explainable as interpolation aids rather than diagnosis aids.
- Cluster-defect cases must be benchmarked separately from sparse isolated bad-pixel cases.

### 6.4 Lag and ghost correction

- Lag and ghosting remain one stage with separate internal mechanisms.
- Tier escalation must be deterministic and time-budgeted.
- First-frame or empty-history operation must degrade cleanly to bypass or a lower tier.

### 6.4.1 Preprocessing E2E proof contract

Every preprocessing algorithm change must pass the automated proof bundle in `Preprocessing-E2E-Automated-Evaluation-Protocol.md` before it can be promoted beyond implementation-in-progress.

The required proof bundle is:

- `PRE-E2E-0` fixture scan for file size, SHA-256, raw/calibration pairing, and Git ignore enforcement.
- `PRE-E2E-1` synthetic oracle for exact offset, gain, nonlinearity, defect, binning, and lag micro-cases.
- `PRE-E2E-2` real fixture calibration-effect evaluation using local cases under `tests/test_data/calibration_cases`.
- `PRE-E2E-3` reference-output comparison when a known output such as `*_oc.raw` is semantically confirmed.
- `PRE-E2E-4` GUI/native E2E execution for user-facing validation.
- `PRE-E2E-5` mismatch negative test to prove wrong calibration contexts are detected.

Release-safe preprocessing gates are:

- raw input SHA-256 must be preserved before and after processing;
- no NaN or Inf output is permitted;
- gain semantics must be known before release promotion;
- dark correction must satisfy `abs(DarkBias) <= 5 ADU` or `DarkReduction_dB >= 10 dB`;
- flat-field correction must satisfy `FlatResidualPct <= 1.0%` for Phase 1 and target `<= 0.5%` for release hardening;
- synthetic BPM tests must reach 100% defect recall with false-positive rate below 0.001%;
- lag benchmark sequences with measurable lag must reach at least 90% ghost removal;
- 3072 x 3072 preprocessing must remain within the 500 ms timing budget;
- Calibration Effect Score must be at least 85 for Phase 1 completeness and target at least 92 for release hardening.

The CES is an engineering readiness score only. It must not be used as a clinical diagnostic-performance claim.

### 6.5 Exposure Index

- `xpe_calc_exposure_index` is the only canonical EI API.
- Whole-image EI baseline is computed after detector correction and before presentation processing.
- Phase 2 refinement is a second invocation on an ROI crop or mask derived from collimation.
- Stitched or multi-irradiation images are non-normative EI inputs and must be rejected or explicitly flagged.
- EI and DI are operational detector-exposure signals and shall not be documented or marketed as direct patient-dose estimates.
- DI action bands used in QA shall be site-configurable and derived from local practice, not hard-coded as universal acceptance thresholds.

### 6.6 GSVG and scatter handling

- GSVG is optional and independent.
- When grid suppression is unavailable or inappropriate, the system must emit a state flag and external diagnostic reason.
- The deterministic image path must remain usable without GSVG.
- Virtual-grid defaults shall be anatomy-aware and observer-reviewed. A visually stronger grid effect is not automatically a safer default.

### 6.7 AI worker governance

- `xpe_ai.dll` is a proxy, not the inference engine itself.
- `xpe_ai_worker.exe` must be sandboxed and restartable.
- Worker crash, timeout, or model load failure must not interrupt deterministic Phase 1 and Phase 2 image delivery.
- AI outputs must include confidence, version, and failure semantics.
- Bone suppression and denoising remain research-gated until task-based validation demonstrates benefit without unacceptable suppression of clinically relevant findings.

---

## 7. Benchmark Binding

No premium algorithm is considered complete until it is tied to:

1. a dataset family in `Algorithm-Benchmark-Pack-Spec.md`,
2. a metric and acceptance rule in `Algorithm-Evaluation-Protocol.md`,
3. a deployment boundary in `Regulatory-Feature-Boundary-Matrix.md`.

Minimum benchmark families that must exist before algorithm freeze:

- temperature sweep
- multi-gain linearity
- heel-effect SID variation
- sparse and clustered defect maps
- lag history sequences
- grid and no-grid acquisitions
- stitched and multi-irradiation exclusion cases
- worker-failure and degraded-mode scenarios

### 7.1 Physical-metric bundle

Where applicable, premium detector and display claims shall be tied to a physical-metric bundle that includes some subset of:

- MTF
- NPS
- DQE
- EI / DI behavior
- artifact review
- observer review

---

## 8. Development Priority for Best-in-Class Quality

1. Lock the benchmark pack and manifest hashes.
2. Finish deterministic Phase 1 quality to a reproducible, benchmarked baseline.
3. Add deterministic Phase 2 premium features only after detector-domain metrics stay stable.
4. Add AI features behind explicit assistive boundaries and worker isolation.
5. Promote research features only after evidence is strong enough to cross the regulatory boundary review.

### 8.1 Implementation-feasibility rules

The following rules are normative for implementation planning:

- every major detector-domain stage shall have a scalar reference implementation before SIMD or multi-thread optimization,
- optimized paths shall be checked against parity harnesses and benchmark packs, not only spot images,
- ROI, diagnostics, confidence, and GSVG reason codes shall travel through sidecar structures or structured logs rather than generic image metadata mutation,
- the deterministic release path shall not depend on GPU availability, cloud inference, or optional AI components,
- learned features shall prefer bounded small-model designs with explicit disable and fallback behavior,
- benchmark manifest schema (`data/benchmark/schema/`) shall be defined and version-locked before Phase 1a feature work begins; no algorithm stage is considered production-ready without a corresponding benchmark family entry.

### 8.2 Sidecar contract guidance

Sidecar objects carry outputs that must not mutate `XpeImageMetadata`:

| Sidecar | Minimum fields | Created by | Consumed by |
|---|---|---|---|
| Collimation ROI sidecar | `roi_x`, `roi_y`, `roi_w`, `roi_h`, `confidence`, `method_id` | Phase 2 collimation | Phase 2 EI refinement, GSVG |
| Quality state vector | `calib_freshness`, `defect_burden_class`, `lag_tier`, `gsvg_state`, `ei_refinement_used`, `ai_worker_status` | Phase 1b/2 pipeline | diagnostic log, IPC to C# |
| AI confidence sidecar | `model_id`, `model_version`, `confidence`, `failure_code`, `fallback_used` | Phase 3 AI worker | orchestrator, DICOM tag insertion |

These fields are informational only in this specification. Concrete C struct definitions shall be in `xpe-implementation-reference.md`.
