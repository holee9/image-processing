# Test Dataset Specification - XPE Advanced Enhancement Module

**Document ID:** TDS-ENHANCE-ADV-001 v1.0  
**IEC 62304 Clause:** 5.1.8 (Test Cases and Test Data)  
**Date:** 2026-04-14  

---

## 1. Purpose and Scope

This Test Dataset Specification defines test inputs, golden references, and acceptance criteria for validating xpe_enhance_advanced.dll. Test data covers four processing stages: noise reduction (4 tiers), edge enhancement, ROI detection, and EI correction.

---

## 2. Test Data Organization

### 2.1 Directory Structure

```
docs/enhance-advanced/test-data/
├── stage-9-noise-reduction/
│   ├── tier-1-gaussian/
│   │   ├── input_synthetic_white_noise.float32
│   │   ├── golden_output_gaussian_sigma1.5.float32
│   │   ├── reference_metrics.json
│   ├── tier-2-bilateral/
│   │   └── ...
│   ├── tier-3-nlm/
│   │   └── ...
│   └── tier-4-wavelet/
│       └── ...
├── stage-10-edge-enhancement/
│   ├── unsharp_alpha_0.8.float32
│   ├── overshoot_limit_test.float32
│   └── ...
├── stage-11-roi-detection/
│   ├── rect_roi_vertical.float32
│   ├── rect_roi_45deg_oblique.float32
│   └── ...
├── svu-2-10-ei-roi-correction/
│   ├── detector_domain_uniform.uint16
│   ├── sidecar_roi_high_confidence.json
│   └── ...
└── golden-references/
    └── sha256_hashes.txt
```

---

## 3. Stage 9: Noise Reduction Test Data

### 3.1 Synthetic Test Data

#### TC-Syn-001: White Gaussian Noise

**Input**: 
- Flat field (uniform pixel value 1000) + white Gaussian noise (σ=50)
- 512×512 float32
- SNR = 1000/50 = 20 dB

**Expected Output** (Tier 2, σ_s=2.5):
- Visual: noise visibly reduced, flat field appears smooth
- Metric: SNR improvement ≥ 40% (SNR_out ≥ 28 dB)

**Golden Reference**: 
- File: `noise_white_snr20db_tier2_golden.float32` (SHA-256: hash_xxx)
- Acceptance: output matches golden within 1% RMS error

---

#### TC-Syn-002: Signal-Dependent Poisson Noise

**Input**:
- Synthetic X-ray image (varying intensity 0-4096, 512×512)
- Poisson noise model: σ_pix = √I (quantum noise) + 20 (electronic noise)
- Multiple dose levels: 1×, 2×, 5× reference dose

**Expected Output** (Tier 2):
- Visual: edges preserved, noise suppressed in background
- Metric: CNRD improvement ≥ 50% at 1× dose

**Golden Reference**: 
- File: `signal_dependent_poisson_tier2_golden.float32`

---

#### TC-Syn-003: Edge-Preserving Denoising Validation

**Input**:
- Synthetic edge: step function I(x) = 1000 for x < 256, 500 for x ≥ 256
- Add Gaussian noise (σ=100) to entire 512×512 image
- Edge should be preserved, interior smoothed

**Expected Output** (Tier 2, Bilateral):
- Edge: LSF width < 5 pixels (preserved)
- Interior: noise reduction ≥ 40%

**Acceptance**: 
- Edge sharpness (LSF) vs. unfiltered: < 5% loss
- Noise reduction: ≥ 40%

---

### 3.2 Real Clinical Image Data

#### TC-Real-001: CDRAD 2.0 Phantom (Multiple Dose Levels)

**Source**: IAP acquisition protocol (§2.1)  
**Dose Levels**: 0.1, 0.25, 0.5, 1.0, 2.0 mGy (20 repeated frames each)

**Test Cases**:
- **TC-Real-001a** (Tier 1): Verify CNRD improvement ≥ 20% at 0.1 mGy
- **TC-Real-001b** (Tier 2): Verify CNRD improvement ≥ 50% at 0.1 mGy, MTF loss ≤ 5%
- **TC-Real-001c** (Tier 3): Verify CNRD improvement ≥ 60% at 0.1 mGy, MTF loss ≤ 4%
- **TC-Real-001d** (Tier 4): Verify CNRD improvement ≥ 70% at 0.1 mGy, MTF loss ≤ 3%

**Metrics**:
```
CNRD = N_visible_details / σ_background
MTF_loss = (MTF_unfiltered - MTF_denoised) / MTF_unfiltered
```

**Acceptance Criteria**:
- CNRD improvement meets target
- MTF loss ≤ specification per tier
- No halo artifacts visible

---

#### TC-Real-002: Wire Phantom (1mm W wire, 1.0 mGy)

**Source**: IAP acquisition protocol (§2.2)  
**Orientations**: Vertical (φ=0°) and Horizontal (φ=90°)

**Test Cases**:
- **TC-Real-002-T1** (Tier 1): MTF @ Nyquist ≥ 0.92
- **TC-Real-002-T2** (Tier 2): MTF @ Nyquist ≥ 0.95
- **TC-Real-002-T3** (Tier 3): MTF @ Nyquist ≥ 0.96
- **TC-Real-002-T4** (Tier 4): MTF @ Nyquist ≥ 0.97

**Golden Reference**: 
- Pre-calibrated LSF and MTF curves for reference (unfiltered)
- Tolerance: ±5% difference from baseline

---

### 3.3 Edge Case Test Data

#### TC-Edge-001: Low-Dose Extreme Noise (0.01 mGy)

**Input**: Very high noise (σ_noise ≈ I), challenging for denoising  
**Test**: Tier 4 (Wavelet) should handle without hallucination  
**Visual Inspection**: No artificial structures; content-preserving

---

#### TC-Edge-002: High-Dose Low-Noise (10 mGy)

**Input**: Minimal noise, high SNR  
**Test**: Denoising should be minimal (near-bypass), execution fast  
**Acceptance**: Processing time < 5ms (skip condition triggered)

---

## 4. Stage 10: Edge Enhancement Test Data

### 4.1 Synthetic Edge Tests

#### TC-Edge-001: Step Edge (Unsharp Masking)

**Input**:
- 512×512 image, sharp step at x=256 (I_left=1500, I_right=1000)
- Clean (no noise initially)

**Test Cases**:
- **TC-Edge-001a** (α=0.8): Halo width < 10 pixels, amplitude ≤ 3σ_local
- **TC-Edge-001b** (α=1.5): Halo amplitude approaches ±3σ_local limit
- **TC-Edge-001c** (α=2.0): Clipping detected, alert issued

**Golden Reference**:
- Enhanced image with α=0.8 stored
- Comparison: output matches golden within 1% RMS

---

#### TC-Edge-002: Overshoot Limiting Validation

**Input**:
- Synthetic step edge + Gaussian noise (σ=50)
- Apply edge enhancement with α=2.0 (potentially excessive)

**Expected Behavior**:
- Overshoot limiting prevents halo amplification
- Clipping count logged
- Output < 1% visual difference with α=1.0

**Acceptance**: No clipping if |boost| ≤ 3σ_local throughout image

---

### 4.2 Real Clinical Tests

#### TC-Real-003: Leeds TOR CDR (High-Contrast Phantom)

**Source**: IAP acquisition protocol (§3.1)  
**Targets**: Circular discs 1-5 mm, 50-80% contrast

**Test Cases** (7 α values: 0.1, 0.3, 0.5, 0.8, 1.0, 1.5, 2.0):
- Measure halo width and amplitude at disc edges
- Visual inspection: "Halo clinically acceptable?"
- Subjective: operator rating (1=no halo, 5=severe halo)

**Acceptance Criteria**:
- α=0.8: halo amplitude ≤ 3σ_local, subjective rating ≤ 2
- α ≥ 1.5: clipping alert issued

**Golden Reference**:
- Operator consensus image with α=0.8
- Visual comparison against new processing

---

## 5. Stage 11: ROI Detection Test Data

### 5.1 Synthetic ROI Tests

#### TC-ROI-001: Perfect Rectangle

**Input**: 
- Black background (I=100)
- White rectangle (I=4000) at (x=100, y=80, w=300, h=350)
- Sharp boundary (no anti-aliasing)

**Expected Output**:
- Detected ROI: (100, 80, 300, 350) ± 2 pixels
- Confidence > 0.95

**Acceptance**: Detection error ≤ 2 pixels

---

#### TC-ROI-002: Blurred/Soft Collimation Border

**Input**: 
- Gaussian transition zone (5-pixel blur) at collimation edge
- Confidence should be lower than perfect edge

**Expected Output**:
- Detected ROI: within ± 5 pixels of true edge
- Confidence 0.70-0.85

**Acceptance**: Detection error ≤ 5 pixels, confidence > 0.7

---

#### TC-ROI-003: Oblique Collimation (±10°)

**Input**: Rectangle rotated by 10° from axis-aligned

**Expected Output**:
- Axis-aligned filter rejects non-rectangular geometry
- Confidence < 0.7 → fallback to full-image

**Acceptance**: Fallback triggered appropriately

---

### 5.2 Real Clinical ROI Tests

#### TC-Real-004: Patient X-ray Images with Collimation

**Source**: IAP acquisition protocol (§4.1)  
**Dataset**: 50 clinical images, 5 projections × 10 patients  
**Ground Truth**: Manually measured corners (2 independent technicians)

**Test Cases**:
- **TC-Real-004a** (Normal collimation): ±5 pixel error, confidence > 0.75
- **TC-Real-004b** (Partial collimation): ±10 pixel error, confidence > 0.6
- **TC-Real-004c** (Oblique ±5°): ±8 pixel error, confidence > 0.65
- **TC-Real-004d** (Low-contrast collimation): ±15 pixel error or fallback, confidence > 0.5

**Golden Reference**: CSV file with manual measurements and expected confidence ranges

---

## 6. SWU-2.10: EI ROI Correction Test Data

### 6.1 Synthetic EI Tests

#### TC-EI-001: Uniform Phantom with ROI

**Input**:
- Detector-domain uniform image (value=1000, 512×512)
- ROI sidecar: (100, 80, 300, 350), confidence=0.95

**Expected Output**:
- EI_roi = 1000 (exact match)
- DI = 0 (if EI_ref=1000)

**Acceptance**: |DI| < 0.01

---

#### TC-EI-002: Dose Accuracy ±10%

**Input**:
- Detector-domain with 10% lower intensity (900 vs. 1000)
- ROI masked

**Expected Output**:
- DI = 10 × log10(0.9) ≈ -0.46

**Acceptance**: |DI_computed - DI_expected| < 0.05

---

### 6.2 Real Phantom EI Tests

#### TC-Real-005: Flat-Field Phantom (Dose Accuracy)

**Source**: IAP acquisition protocol (§5.1)  
**Doses**: 0.25, 0.5, 1.0 mGy with typical collimation  
**Gold Standard**: Ionization chamber measurement ±2% accuracy

**Test Cases**:
- **TC-Real-005a** (0.25 mGy, 75% collimation): EI error ≤ 5%
- **TC-Real-005b** (0.5 mGy, 75% collimation): EI error ≤ 5%
- **TC-Real-005c** (1.0 mGy, 75% collimation): EI error ≤ 5%

**Acceptance**: |EI_roi - EI_true| / EI_true ≤ 0.05

---

#### TC-Real-006: EI Fallback (Confidence ≤ 0.7)

**Input**: Same images as TC-Real-005, but simulate ROI detection failure (confidence=0.5)

**Expected Behavior**:
- Fallback to full-image EI (SWU-2.0)
- Log message: "Using full-image EI (fallback reason: confidence)"

**Acceptance**: Full-image EI computed, no error

---

## 7. Golden References & Hash Verification

### 7.1 Golden Reference Management

All golden reference outputs are stored with SHA-256 hashes:

```
golden-references/
├── noise_white_snr20db_tier2_golden.float32
│   Hash: a1b2c3d4e5f6g7h8...
├── signal_dependent_poisson_tier2_golden.float32
│   Hash: b2c3d4e5f6g7h8i9...
└── sha256_hashes.txt
```

### 7.2 Hash Verification Procedure

Before each test run:
```bash
sha256sum -c golden-references/sha256_hashes.txt
```

If any hash mismatch: **FAIL** (golden reference corrupted)

---

## 8. Test Execution & Results

### 8.1 Test Suite Organization

| Suite | Test IDs | Stage | Execution Time |
|-------|----------|-------|---|
| Unit Tests | TC-Syn-001 to 003 | 9 | ~5 min |
| MTF Validation | TC-Real-002 | 9 | ~2 min |
| CNRD Validation | TC-Real-001 | 9 | ~3 min |
| Edge Tests | TC-Edge-001 to 002 | 10 | ~2 min |
| ROI Tests | TC-ROI-001 to 003, TC-Real-004 | 11 | ~5 min |
| EI Tests | TC-EI-001 to 002, TC-Real-005 to 006 | SWU-2.10 | ~3 min |
| **Total** | | | ~20 min |

### 8.2 Acceptance Criteria Summary

| Test Category | Pass Criteria |
|--------|---|
| Gaussian (Tier 1) | SNR improvement ≥ 20%, MTF loss ≤ 8% |
| Bilateral (Tier 2) | CNRD improvement ≥ 50%, MTF loss ≤ 5% |
| NLM (Tier 3) | CNRD improvement ≥ 60%, MTF loss ≤ 4% |
| Wavelet (Tier 4) | CNRD improvement ≥ 70%, MTF loss ≤ 3% |
| Edge Enhancement | Halo amplitude ≤ 3σ_local, no artifacts |
| ROI Detection | Error ≤ ±5 pixels, confidence > 0.7 |
| EI ROI Correction | Error ≤ ±5%, DI accuracy ±0.05 |

---

## 9. Continuous Integration & Regression Testing

### 9.1 CI/CD Integration

Test suite executed automatically on:
- Pull request (before merge)
- Release build
- Monthly regression validation

### 9.2 Regression Detection

If any test fails:
1. Investigate root cause (code change, data corruption, etc.)
2. Document in ticket
3. Do NOT merge PR until regression resolved

---

*TDS-ENHANCE-ADV-001 v1.0 끝*
