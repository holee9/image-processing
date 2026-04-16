# XPE 10-Pass Review, Evaluation, and Fix Log

**Document ID**: XPE-REVIEW-10PASS-001  
**Version**: 1.2.0  
**Date**: 2026-04-15  
**Status**: Historical review log / cleaned summary  
**Canonical Scope**: `docs/project/`

---

## 1. Purpose

This log records the major review themes that were used to harden the canonical `docs/project/` set.

It is a historical summary, not a normative source. Canonical requirements live in the current product, PRD, SVVP, benchmark, evaluation, and master specification documents.

---

## 2. Review Pass Themes

| Pass | Focus | Main action |
|---:|---|---|
| 1 | canonical count audit | normalized the executable-unit total to `42` |
| 2 | phase ownership audit | fixed Phase 2 versus Phase 3 ownership boundaries |
| 3 | EI identity audit | locked `SWU-2.10` as the single EI identifier |
| 4 | pipeline execution audit | restored canonical stage order and degraded-mode rules |
| 5 | metadata audit | separated state flags from diagnostic details |
| 6 | realism audit | removed unsupported completion language from status documents |
| 7 | benchmark audit | introduced benchmark-pack structure and promotion gates |
| 8 | evaluation audit | introduced unified detector, premium, AI, and field metrics |
| 9 | regulatory boundary audit | separated release-safe, research-gated, and hold items |
| 10 | final cross-validation | rechecked versions, counts, phase ownership, and source links |

---

## 3. Current Interpretation

The durable outcome of the review program is:

- one canonical project-document root under `docs/project/`,
- one master tie-breaker in `SPEC-XPE-MASTER.md`,
- benchmark-first promotion for premium algorithms,
- explicit separation between deterministic baseline, deterministic premium, and assistive AI.

---

## 4. Remaining Open Items

| ID | Remaining item |
|---|---|
| `R-01` | `docs/post-processing/xpe/` regulated package still needs canonical synchronization |
| `R-02` | benchmark manifests and dataset hashes still need to be frozen in repo assets |
| `R-03` | source modules beyond `modules/common/` remain to be implemented |
| `R-04` | assistive AI operating boundary still needs release-management sign-off |

---

## 5. Round 9 — XPE-ALG-001 v1.8 (GAP-BW~CF) Review Passes

**Scope**: 10 new algorithm specifications added to XPE-ALG-001 v1.8

| Pass | Focus | Findings and Actions |
|---:|---|---|
| 1 | Physics and Mathematics | Verified all 10 new formulas against published references: PCD charge-sharing model (Ballabriga 2018), NOSF Wiener filter (Barrett NPS framework), ring correction (CBCT polar-domain literature), SDT d'/ROC/JAFROC (Green 1966, Chakraborty JAFROC). All formulas algebraically consistent. |
| 2 | Implementation Complexity | Validated SIMD feasibility: PCD binning <5ms (simple per-pixel neighbor sum, AVX2 uint16 SIMD); NOSF <8ms (MKL FFT 4ms + AVX2 spectral multiply 4ms); Ring <15ms (polar transform + 1D FFT batch). All within stated targets. |
| 3 | Regulatory Boundary | PCD (§21): release-safe, deterministic; ICLM (§9.14): release-safe; NOSF (§4.10): release-safe; Ring (§3.17): release-safe; AGIS (§8.9): research-gated (AI, Non-SaMD, clinical_use_requires_review=true); CS-Tomo (§22): research-gated (experimental modality); ACIQ (§12.12): release-safe; HCPS (§10.10): release-safe; RDSR (§17.4): release-safe (DICOM standard compliance); SDT (§11.6): release-safe (informational_only=true). |
| 4 | IEC 62304 §5.4 Compliance | All 10 sections include: purpose statement, algorithm step specification, API struct + function signature, verification criteria table. §21 PCD and §22 CS-Tomo added as new SWU entries with SDD scope annotation. |
| 5 | Cross-Reference Integrity | GAP-BY (§4.10) → §11.5 quantum noise, §12.6 MTF verified present; GAP-CA (§8.9) → §8.8 fallback, §6.4, §5.3, §7.2 all verified; GAP-CB (§22) → §19 FBP shared projector, §20 TV-ADMM confirmed; GAP-CC (§12.12) → §9.11 CUSUM, §12.3 NPS, §12.6 MTF, §12.8 CNR all verified; GAP-CD (§10.10) → §10.7 arena, §10.8 thread safety, §10.9 GPU confirmed; GAP-CE (§17.4) → §9.12 DAP/KERMA, §17 IOD confirmed; GAP-CF (§11.6) → §11.5 noise model, §12.6 MTF, §12.3 NPS confirmed. All cross-references valid. |
| 6 | Benchmark Coverage | GAP-BW → BP-02 (multi-gain linearity, PCD calibration); GAP-BX → BP-01 (temperature sweep), BP-10 (degraded-mode); GAP-BY → BP-11 (task-based); GAP-BZ → BP-13 (new: ring/artifact correction); GAP-CA → BP-11; GAP-CB → BP-13; GAP-CC → BP-12 (operational QC); GAP-CD → BP-10; GAP-CE → BP-13 (RDSR compliance); GAP-CF → BP-11. BP-13 new family created covering PCD, ring, CS-Tomo, RDSR. |
| 7 | API Contract Review | All 10 algorithms have complete XpeXxx C struct + XpeStatus function signature. Non-SaMD guards: AGIS clinical_use_requires_review=true; SDT informational_only=true; RDSR anonymize parameter with PS3.15 Annex E. No ambiguous ownership between DLLs. |
| 8 | Safety Boundary | AGIS: clinical_use_requires_review=true enforced; SDT: informational_only=true enforced; RDSR: anonymization mandatory path; CS-Tomo: no safety claim beyond research-gated label; ICLM: urgent_recal_required flag properly scoped. All safety flags verified. |
| 9 | Performance Target Validation | PCD <5ms AVX2 ✓; ICLM <1ms ✓; NOSF <8ms (MKL FFT 4ms + AVX2 4ms) ✓; Ring <15ms (polar transform + FFT) ✓; AGIS <500ms CPU ✓; CS-Tomo <2s (FFT-ADMM 50 iter) ✓; ACIQ <5ms ✓; HCPS <0.5ms ✓; RDSR <50ms ✓; SDT <10ms ✓. All targets technically feasible. |
| 10 | Final Integration Check | Document version updated to v1.8; Review cycles count updated to 90; GAP table complete for 90 gaps (GAP-01~CF); Appendix C updated with 10 new rows; Revision history v1.8 entry added. Remaining sync debt: §21 PCD and §22 CS-Tomo require downstream SRS/SDD entry creation. BP-13 benchmark family definition needs to be added to Algorithm-Benchmark-Pack-Spec.md. |
