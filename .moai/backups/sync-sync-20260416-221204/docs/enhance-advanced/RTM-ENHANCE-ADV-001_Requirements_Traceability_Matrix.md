# Requirements Traceability Matrix - XPE Advanced Enhancement Module

**Document ID:** RTM-ENHANCE-ADV-001 v1.0  
**IEC 62304 Clause:** 5.1.1(c) (Bidirectional Traceability)  
**Date:** 2026-04-14  

---

## 1. Overview

This RTM provides bidirectional mapping between:
- **SRS** (Software Requirements Specification)
- **SAD** (Software Architecture Document) / SWU decomposition
- **SHA** (Software Hazard Analysis) / Hazard mitigation
- **Test cases** (from TDS-ENHANCE-ADV-001)

---

## 2. SRS ↔ SAD Traceability

### Noise Reduction (Stage 9)

| SRS Req ID | Description | SAD SWU | Implementation | Verification |
|-----------|-------------|---------|----------------|--------------|
| FR-100 | Gaussian blur (Tier 1) | SWU-2.5 | xpe_enhance_denoise_gaussian() | test_gaussian_denoise |
| FR-200 | Bilateral filter (Tier 2) | SWU-2.6 | xpe_enhance_denoise_bilateral() | test_bilateral_filter |
| FR-300 | NLM (Tier 3) | SWU-2.7 | xpe_enhance_denoise_nlm() | test_nlm_denoise |
| FR-400 | Wavelet shrinkage (Tier 4) | SWU-2.8 | xpe_enhance_denoise_wavelet() | test_wavelet_shrink |
| FR-500 | Tier selection logic | SWU orchestration | xpe_enhance_denoise(mode) | test_tier_selection |
| FR-600 | MTF constraint < 5% | SWU design | Algorithm tuning | test_wire_phantom_mtf |
| FR-700 | Signal-dependent noise model | All SWU | σ_noise computation | test_noise_characterization |

### Edge Enhancement (Stage 10)

| SRS Req ID | Description | SAD SWU | Implementation | Verification |
|-----------|-------------|---------|----------------|--------------|
| FR-800 | Unsharp masking | SWU-2.9 | xpe_enhance_edges() | test_unsharp_masking |
| FR-900 | Overshoot limiting (mandatory) | SWU-2.9 | ±3σ clipping logic | test_overshoot_limiting |
| FR-1000 | Multi-band (advanced) | SWU-2.9 optional | Frequency decomposition | test_multiband_enhancement |

### ROI Detection (Stage 11)

| SRS Req ID | Description | SAD SWU | Implementation | Verification |
|-----------|-------------|---------|----------------|--------------|
| FR-1100 | Hough transform ROI | SWU-2.11 | xpe_detect_roi() | test_hough_transform |
| FR-1200 | Confidence scoring | SWU-2.11 | confidence computation | test_confidence_score |
| FR-1300 | JSON sidecar output | SWU-2.12 | xpe_sidecar_write() | test_sidecar_io |

### EI ROI Correction (SWU-2.10)

| SRS Req ID | Description | SAD SWU | Implementation | Verification |
|-----------|-------------|---------|----------------|--------------|
| FR-1400 | ROI-masked EI | SWU-2.10 | xpe_refine_ei_by_roi() | test_ei_roi_computation |
| FR-1500 | DI + QC alert | SWU-2.10 | DI formula, alert logic | test_di_qc_alert |
| FR-1600 | Fallback logic | SWU-2.10 | Auto fallback to full-image | test_fallback_logic |

### Integration & Metadata

| SRS Req ID | Description | SAD SWU | Implementation | Verification |
|-----------|-------------|---------|----------------|--------------|
| FR-1700 | Status flags | All SWU | Flag setting per stage | test_flag_setting |
| FR-1800 | Diagnostic logging | All SWU | JSON log per-stage | test_logging |

---

## 3. SRS ↔ SHA Traceability

### Safety Requirement Coverage

| SRS Req ID | SHA Control | Hazard Mitigated | Status |
|-----------|-------------|------------------|--------|
| SAF-100 | CTL-ADV-001 | HAZ-ADV-001 (overshoot) | ✓ Mitigated |
| SAF-101 | CTL-ADV-003 | HAZ-ADV-003 (ROI false) | ✓ Mitigated |
| SAF-102 | CTL-ADV-002 | HAZ-ADV-002 (over-smooth) | ✓ Mitigated |
| FR-600 (MTF constraint) | CTL-ADV-005 | HAZ-ADV-005 (MTF drift) | ✓ Mitigated |
| FR-1400 (data domain check) | CTL-ADV-004 | HAZ-ADV-004 (wrong EI) | ✓ Mitigated |
| FR-200.4 (edge detection check) | CTL-ADV-008 | HAZ-ADV-008 (divergence) | ✓ Mitigated |

---

## 4. Test Case ↔ SRS Traceability

### Test Coverage Matrix

| Test Case ID | SRS Req | Stage | Purpose | Acceptance Criteria |
|-----------|---------|-------|---------|-------------------|
| TC-001 | FR-100.1 | 9 | Gaussian sigma validation | σ ∈ [0.5, 3.0], time ≤ 15ms |
| TC-002 | FR-100.4 | 9 | MTF loss Tier 1 | MTF loss ≤ 8% |
| TC-003 | FR-200.1 | 9 | Bilateral σ_s, σ_r | Parameters accepted, output valid |
| TC-004 | FR-200.4 | 9 | Noise detection | Skip bilateral if variance < threshold |
| TC-005 | FR-300.1 | 9 | NLM patch/window | Patch 7×7, window 21×21 |
| TC-006 | FR-300.3 | 9 | NLM similarity truncation | Computation 40-60% reduction, quality preserved |
| TC-007 | FR-300.4 | 9 | NLM timing | ≤ 150ms for 3072×3072 |
| TC-008 | FR-400.1 | 9 | Wavelet decomposition | db4, 3-4 levels |
| TC-009 | FR-400.2 | 9 | BayesShrink threshold | Computed per subband correctly |
| TC-010 | FR-500.1 | 9 | Tier selection by mode | Fast→T1, Standard→T2, Premium→T3, Ultra→T4 |
| TC-011 | FR-600.1 | 9 | MTF validation wire phantom | MTF loss < 5% at Nyquist |
| TC-012 | FR-700.1 | 9 | Signal-dependent noise | σ²_total ∝ I + constant |
| TC-013 | FR-800.1 | 10 | Unsharp masking | I_enh = I + α(I-I_blur), α ∈ [0.1, 2.0] |
| TC-014 | FR-900.1 | 10 | Overshoot limiting | ±3σ_local clipping enforced |
| TC-015 | FR-900.2 | 10 | Overshoot disable block | Config with limit_disabled → error |
| TC-016 | FR-900.3 | 10 | Clipping alert | Alert if > 1% pixels clipped |
| TC-017 | FR-1100.1 | 11 | Hough ROI detection | Corners detected ±5 pixels vs. manual |
| TC-018 | FR-1100.4 | 11 | Axis alignment filter | Lines with θ ≈ 0° or 90° (±5°) selected |
| TC-019 | FR-1200.1 | 11 | Confidence scoring | confidence = sum_peaks / (4×max), ∈ [0, 1] |
| TC-020 | FR-1300.1 | 11 | ROI JSON sidecar | File at {image}.roi.json, format valid |
| TC-021 | FR-1300.2 | 11 | ROI boundary clipping | x,y,w,h within image bounds |
| TC-022 | FR-1400.1 | SWU-2.10 | ROI-masked EI | EI_roi = mean(detector[ROI]) |
| TC-023 | FR-1500.1 | SWU-2.10 | DI computation | DI = 10×log10(EI/EI_ref) per IEC 62494-1 |
| TC-024 | FR-1500.2 | SWU-2.10 | QC alert |

 \|DI\| > 3 → alert issued |
| TC-025 | FR-1600.1 | SWU-2.10 | Fallback logic | confidence ≤ 0.7 → full-image EI |
| TC-026 | FR-1700.1 | All | Status flags | Flags set only if executed |
| TC-027 | FR-1800.1 | All | Diagnostic logging | JSON log per-stage with timing |
| TC-028 | PERF-100 | All | Timing budget | T1≤15ms, T2≤60ms, T3≤150ms, T4≤300ms |
| TC-029 | PERF-102 | All | Memory budget | Peak ≤ 60MB, no leaks in 100-frame batch |

---

## 5. Hazard ↔ Test Case Traceability

### Hazard Mitigation Validation

| Hazard ID | Hazard | Mitigating SRS Req | Mitigating Test Case | Verification Method |
|-----------|--------|-------------------|----------------------|-------------------|
| HAZ-ADV-001 | Overshoot → halo | SAF-100 (FR-900.1-3) | TC-015, TC-016 | Config validation, clipping detection |
| HAZ-ADV-002 | Over-smooth → lesion | SAF-102 (FR-600.1) | TC-011 | Wire phantom MTF |
| HAZ-ADV-003 | ROI false → missed alert | SAF-101 (FR-1600.1) | TC-025 | Fallback triggering, logging |
| HAZ-ADV-004 | Wrong data domain → invalid DI | FR-1400.1 | Unit test (not in SRS-centric) | Type checking, API review |
| HAZ-ADV-005 | MTF drift → resolution | CTL-ADV-005 | TC-011 (monthly QA) | Scheduled wire phantom test |
| HAZ-ADV-006 | NLM hallucination | FR-300 design | TC-006 | Visual inspection guidelines |
| HAZ-ADV-007 | Sidecar I/O failure | FR-1300.1 design | Edge case testing | Error injection test |
| HAZ-ADV-008 | Bilateral divergence | FR-200.1 bounds-checking | Numerical stress test | Extreme noise input |

---

## 6. Configuration Consistency

### Frozen Settings (Non-Configurable)

| Setting | SRS Ref | SAD Design | SHA Control | Rationale |
|---------|---------|-----------|-------------|-----------|
| Overshoot limit = ON | SAF-100 | SWU-2.9 logic | CTL-001 | HAZ-001 mitigation |
| Tier parameters (read-only) | FR-600 | Algorithm tuning | CTL-005 | HAZ-005 mitigation |
| Default mode = Standard | FR-500 | Orchestration | Design default | HAZ-002, HAZ-006 mitigation |
| ROI fallback = AUTO | SAF-101 | SWU-2.10 logic | CTL-003 | HAZ-003 mitigation |

---

## 7. Completeness Checklist

- [x] All SRS functional requirements traced to SAD SWU
- [x] All SRS safety requirements traced to SHA hazard mitigations
- [x] All hazards traced to test cases
- [x] All test cases linked to SRS requirements
- [x] Frozen settings documented and locked
- [x] No orphaned requirements or test cases
- [x] No circular dependencies or unresolved traces

---

*RTM-ENHANCE-ADV-001 v1.0 끝*
