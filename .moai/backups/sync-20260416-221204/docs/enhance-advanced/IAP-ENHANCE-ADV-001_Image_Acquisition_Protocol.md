# Image Acquisition Protocol - XPE Advanced Enhancement Module

**Document ID:** IAP-ENHANCE-ADV-001 v1.0  
**Purpose:** Operational protocol for acquiring test images for validation and calibration  
**Target Audience:** Calibration Engineers, QA Technicians  
**Date:** 2026-04-14  

---

## 1. Overview

This protocol specifies image acquisition procedures for validating and calibrating the four processing stages of xpe_enhance_advanced.dll:
- **Stage 9**: Noise reduction (4 tiers) parameter optimization
- **Stage 10**: Edge enhancement calibration
- **Stage 11**: Collimation ROI detection validation
- **SWU-2.10**: EI ROI correction accuracy validation

---

## 2. Noise Reduction Parameter Optimization

### 2.1 CDRAD 2.0 Phantom Acquisition (Contrast-Detail Evaluation)

#### Purpose
Optimize noise reduction tier and parameters while maintaining spatial resolution (MTF < 5% loss).

#### Equipment
- **Phantom**: CDRAD 2.0 (Leeds Test Objects, UK)
- **X-ray source**: Standard clinical tube (70 kVp recommended)
- **Detector**: Target detector (e.g., Varex XRD 4343N)
- **Dosimeter**: Calibrated ionization chamber (NIST-traceable)

#### Acquisition Protocol

| Dose Level | mGy | Frame Count | Purpose |
|-----------|-----|-------------|---------|
| Very Low | 0.1 | 20 repeated | Extreme noise, signal-dependent |
| Low | 0.25 | 20 repeated | Typical fluoroscopy |
| Standard | 0.5 | 20 repeated | Normal imaging |
| High | 1.0 | 20 repeated | High SNR baseline |
| Very High | 2.0 | 20 repeated | Reference (minimal noise) |

**Total frames**: 100 per acquisition session

#### Acquisition Procedure

1. **Setup**: Mount CDRAD 2.0 at 1.0 m SID (source-to-image distance)
2. **Exposure**: Activate X-ray beam for each dose level
   - Measure dose with ionization chamber at phantom center
   - Adjust mA·s to reach target dose ±10%
3. **Acquisition**: Capture 20 repeated frames at each dose
   - Stabilize tube output (warm-up 30 minutes prior)
   - Measure temperature and record
4. **Output**: Save frames as uint16 RAW + metadata (JSON)

#### Analysis Metrics

**CNRD (Contrast-to-Noise Ratio Diameter)**

```
CNRD(dose) = N_detail(dose) / N_noise

where:
  N_detail = number of visible detail discs in phantom
  N_noise = standard deviation of uniform background region
```

**Target**: After noise reduction, CNRD(0.1 mGy) should increase by 40-60% vs. unfiltered, while MTF loss < 5%.

**Acceptance Criteria**:
- CNRD improvement: ≥ 50%
- MTF loss: ≤ 5% at Nyquist
- No halo artifacts visible around detail discs

---

### 2.2 Wire Phantom Acquisition (MTF Measurement)

#### Purpose
Validate MTF preservation at each noise tier.

#### Equipment
- **Phantom**: 1mm tungsten wire (perpendicular to pixel rows/columns)
- **SID**: 1.0 m
- **Dose**: 1.0 mGy (high SNR to isolate MTF)

#### Procedure

1. **Alignment**: Position wire vertically (φ = 0°) and horizontally (φ = 90°)
2. **Exposure**: Single acquisition per orientation
3. **Analysis**:
   - Extract 1D line profile perpendicular to wire
   - Compute Line Spread Function (LSF) via edge detection
   - FFT(LSF) = MTF
   - Measure MTF at Nyquist frequency (f_Nyquist = 0.5 / pixel_pitch)

#### Acceptance Criteria

| Tier | MTF @ Nyquist | Requirement |
|------|:--------------:|-----------|
| Tier 1 (Gaussian) | ≥ 0.92 | ≤ 8% loss |
| Tier 2 (Bilateral) | ≥ 0.95 | ≤ 5% loss |
| Tier 3 (NLM) | ≥ 0.96 | ≤ 4% loss |
| Tier 4 (Wavelet) | ≥ 0.97 | ≤ 3% loss |

---

## 3. Edge Enhancement Calibration

### 3.1 Leeds TOR CDR Phantom Acquisition

#### Purpose
Validate edge enhancement strength (α) and overshoot limiting.

#### Equipment
- **Phantom**: Leeds TOR CDR (high-contrast targets)
- **Targets**: Circular discs 1-5 mm diameter, 50-80% contrast
- **SID**: 1.0 m
- **Dose**: 1.0 mGy (standard)

#### Acquisition Protocol

| Parameter | Value | Count |
|-----------|-------|-------|
| α (enhancement strength) | 0.1, 0.3, 0.5, 0.8, 1.0, 1.5, 2.0 | 7 levels |
| Repeated frames per α | 3 | 21 total |

#### Procedure

1. **Setup**: Position Leeds TOR at detector center
2. **Exposure**: Single X-ray exposure
3. **Processing**: Apply edge enhancement with each α value
4. **Inspection**: Visual assessment of halo artifacts around disc edges

#### Analysis Metrics

**Halo Width & Amplitude**

```
At disc edge, measure:
  Halo_width = distance (pixels) from disc boundary to halo peak
  Halo_amplitude = intensity differential above local background
```

**Target**: Halo amplitude ≤ 3σ_local (overshoot limiter constraint)

**Acceptance Criteria**:
- No visible halo beyond ±10 pixels at disc edge
- Clipping count ≤ 1% of image pixels (indicating over-aggressive α)
- Subjective assessment: "Enhancement visible but halo not clinically objectionable"

---

## 4. Collimation ROI Detection Validation

### 4.1 Manual Collimation Test Set

#### Purpose
Validate ROI detection accuracy against manually-measured collimation boundaries.

#### Equipment
- **Test Set**: 10 different patients, 5 imaging projections per patient (50 images total)
- **Manual Measurement**: 2 experienced technicians independently measure collimation borders using Hough visualization overlay
- **Gold Standard**: Average of 2 independent measurements

#### Procedure

1. **Acquisition**: Acquire clinical images with various collimation geometries
   - Include normal rectangular collimation
   - Include partial collimation (one edge not visible)
   - Include oblique collimation (±10°)
2. **Ground Truth**: Manually measure 4 corners of collimation rectangle using image viewer
   - Record pixel coordinates (x1,y1), (x2,y2), etc.
   - Precision: ±2 pixels
3. **Automated Detection**: Run xpe_detect_roi() on same images
4. **Comparison**: Compute detection error
   ```
   ROI_error = mean(|detected_corner - manual_corner|)
   ```

#### Acceptance Criteria

| Scenario | Detection Accuracy | Confidence Min |
|----------|:-----------------:|:-------------:|
| Normal rectangular collimation | ± 5 pixels | > 0.75 |
| Partial collimation (1 edge missing) | ± 10 pixels | > 0.6 |
| Oblique edges (±10°) | ± 8 pixels | > 0.65 |
| Low-contrast collimation | ± 15 pixels or fallback | > 0.5 |

---

## 5. EI ROI Correction Accuracy Validation

### 5.1 Phantom Exposure Levels with Collimation

#### Purpose
Validate that ROI-masked EI matches true exposure within ±5%.

#### Equipment
- **Phantom**: Flat-field phantom (uniform absorption) with physical collimation border
- **Gold Standard**: Calibrated dosimeter (ionization chamber, ±2% accuracy)
- **Detector**: Clinical detector

#### Acquisition Protocol

| Dose | Collimation Size | Repeated Frames |
|------|:----------------:|:---------------:|
| 0.25 mGy | Full field (no collimation) | 3 |
| 0.25 mGy | 75% collimation (typical) | 3 |
| 0.25 mGy | 50% collimation (tight) | 3 |
| 0.5 mGy | 75% collimation | 3 |
| 1.0 mGy | 75% collimation | 3 |

**Total**: 15 images per acquisition session

#### Procedure

1. **Setup**: Position flat-field phantom with known collimation pattern
2. **Dosimetry**: Place ionization chamber at phantom surface (unshielded area)
3. **Exposure**: Measure reference dose with dosimeter
4. **Acquisition**: Capture image and sidecar ROI
5. **Analysis**:
   ```
   EI_true = calibrated_dose_uncorrected / detector_sensitivity
   EI_roi_computed = mean(detector_domain_pixels[ROI])
   EI_error_pct = 100 × |EI_roi_computed - EI_true| / EI_true
   ```

#### Acceptance Criteria

```
EI_error ≤ 5% for all dose levels and collimation sizes
```

---

## 6. Monthly QA Protocol

### 6.1 Wire Phantom MTF Measurement (Monthly)

**Frequency**: 1st Friday of each month  
**Duration**: ~30 minutes

**Procedure**:
1. Acquire wire phantom images (Tier 1, 2, 3, 4)
2. Measure MTF @ Nyquist for each tier
3. Compare to baseline (from calibration phase)
   ```
   MTF_change = (MTF_current - MTF_baseline) / MTF_baseline
   ```
4. **Alert if** |MTF_change| > 5%
   - Indicates detector optics degradation or software drift
   - Trigger detector maintenance or software recalibration

### 6.2 Leeds TOR Visual Inspection (Monthly)

**Frequency**: 1st Friday of each month  
**Duration**: ~20 minutes

**Procedure**:
1. Acquire Leeds TOR with default parameters (α=0.8, Tier 2)
2. Visual inspection: "Are halos around discs visible?"
   - YES → over-aggressive parameters, adjust
   - NO → parameters acceptable
3. Log observation

### 6.3 Flat-Field EI Accuracy Check (Quarterly)

**Frequency**: Every 3 months  
**Duration**: ~1 hour

**Procedure**:
1. Acquire flat-field phantom at 3 dose levels (0.25, 0.5, 1.0 mGy) with typical collimation
2. Measure reference dose with dosimeter
3. Compute EI_roi and compare to EI_true
4. **Alert if** error > 5%

---

## 7. Parameter Tuning Matrix

### 7.1 Noise Tier Recommendations

| Clinical Scenario | Recommended Tier | Reasoning |
|----------|:----------:|-----------|
| Real-time fluoroscopy | Tier 1 (Fast) | Speed critical, noise acceptable |
| Standard radiography | Tier 2 (Standard) | Balance of speed & quality |
| Low-dose imaging (< 1 mGy) | Tier 4 (Wavelet) | Max noise reduction, MTF protection |
| Digital tomosynthesis | Tier 3 (NLM) | Quality within time budget |
| Research / archival | Tier 4 (Ultra) | No time constraint, best quality |

### 7.2 σ Parameter Tuning Guide

| Noise Tier | Parameter | Low Dose (0.1 mGy) | Standard (0.5 mGy) | High Dose (2.0 mGy) |
|-----------|:---------:|:--:|:--:|:--:|
| Tier 1 | σ (Gaussian) | 2.0 | 1.5 | 1.0 |
| Tier 2 | σ_s (Bilateral) | 3.0 | 2.5 | 2.0 |
| Tier 2 | σ_r_mult (Bilateral) | 1.5 | 1.0 | 0.8 |
| Tier 3 | h_strength (NLM) | 1.5 | 1.0 | 0.8 |
| Tier 4 | decomp_level (Wavelet) | 4 | 4 | 3 |

---

## 8. Documentation & Record-Keeping

### 8.1 Acquisition Log Format

```json
{
  "date": "2026-05-01T10:30:00Z",
  "protocol": "CDRAD 2.0 CNRD evaluation",
  "operator": "Tech_Name",
  "detector": "XRD-4343N",
  "tube_temp_c": 35.2,
  "images": [
    {
      "dose_mgy": 0.1,
      "frame_count": 20,
      "output_path": "/data/cdrad_01_dose01_*.raw"
    }
  ],
  "notes": "Standard acquisition, no issues",
  "approval": "Authorized by: _______"
}
```

### 8.2 Analysis Results Report

All results recorded in spreadsheet with fields:
- Date, Protocol, Operator
- Phantom ID, Dose Level
- Measured Metrics (CNRD, MTF, ROI error, EI error)
- Pass/Fail Status
- Corrective Action (if needed)

---

## 9. Safety Considerations

- **Radiation Safety**: Follow institutional ALARA (As Low As Reasonably Achievable) principle
- **Dosimetry**: Use calibrated dosimeter, traceable to NIST
- **Personnel**: Only qualified technicians allowed; annual radiation safety training required
- **Equipment**: Inspect X-ray tube and detector before each session for visible damage

---

*IAP-ENHANCE-ADV-001 v1.0 끝*
