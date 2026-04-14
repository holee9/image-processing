# Algorithm Benchmark Pack Specification

**Document ID**: XPE-BENCHMARK-PACK-001  
**Version**: 1.1.0  
**Date**: 2026-04-14  
**Status**: Controlled Draft

---

## 1. Purpose

This document defines the dataset families that must exist before XPE algorithm quality claims can be considered repeatable, promotable, or releasable.

---

## 2. Source Basis

This benchmark structure is aligned to:

- IEC 62494-1 for detector-domain EI/DI usage
- DICOM PS3.14 GSDF for presentation-path verification
- AAPM TG-151 for ongoing artifact and QC workflows
- AAPM TG-232 for site-specific EI/DI operational review
- published virtual-grid and detector-correction studies for anatomy-aware evaluation

---

## 3. Benchmark Families

| Pack ID | Purpose | Primary algorithms |
|---|---|---|
| `BP-01` | temperature sweep | offset, temperature compensation, drift logic |
| `BP-02` | multi-gain linearity | nonlinearity, gain correction |
| `BP-03` | heel-effect SID variation | gain / heel compensation |
| `BP-04` | sparse defect and cluster defect | defect correction |
| `BP-05` | lag history sequence | lag / ghost correction |
| `BP-06` | grid and no-grid | GSVG, contrast stability |
| `BP-07` | collimation ROI | collimation, ROI-aware EI refinement |
| `BP-08` | single-irradiation EI reference | EI / DI validation |
| `BP-09` | stitched and multi-irradiation exclusion | EI rejection logic |
| `BP-10` | degraded-mode stress | missing optional binaries, worker failure, timeout recovery |

---

## 4. Required Manifest Fields

Each benchmark item shall include:

- dataset ID
- detector model and serial
- acquisition protocol
- irradiation count
- image dimensions and pixel format
- temperature and timing metadata when relevant
- calibration session identifier
- SHA-256 content hash
- allowed use classification
- `domain_class`
- `release_relevance`
- `body_part`
- `grid_state`
- `benchmark_owner`
- `reference_measurement_available`

---

## 5. Pack Design Rules

### 5.1 Detector-domain packs

Detector-domain packs shall preserve enough raw metadata to validate:

- EI / DI,
- residual nonuniformity,
- lag history,
- temperature behavior,
- calibration session continuity.

### 5.2 Enhancement and presentation packs

These packs shall include observer-reviewable outputs and at least one objective artifact or contrast metric, but must not replace detector-domain packs.

### 5.3 Assistive AI packs

AI packs shall include:

- intended input population,
- out-of-distribution notes,
- confidence outputs,
- degraded-mode expected behavior,
- worker crash or timeout cases when relevant.

---

## 6. Minimal Stratification

| Pack ID | Required strata |
|---|---|
| `BP-01` | low, nominal, high temperature; varying PREP times |
| `BP-02` | low, medium, high exposure; multiple detector modes |
| `BP-03` | at least two calibration SID anchors plus extrapolated clinical SID |
| `BP-04` | isolated bad pixels, short lines, clustered patches |
| `BP-05` | first frame, short history, long history, reset boundary |
| `BP-06` | chest plus at least one denser anatomy family; multiple virtual-grid settings |
| `BP-07` | easy, moderate, and difficult collimation boundaries |
| `BP-08` | single-irradiation reference cases with approved EI target values |
| `BP-09` | stitched and multi-irradiation cases that must be rejected or flagged |
| `BP-10` | missing DLL, missing model, worker crash, timeout, corrupted config |

---

## 7. Freeze and Promotion Rule

Once a pack is used for a release gate:

1. manifest schema version is frozen,
2. content hashes are frozen,
3. any replacement requires a version bump and rationale,
4. historical results must remain reproducible against the old pack version.
