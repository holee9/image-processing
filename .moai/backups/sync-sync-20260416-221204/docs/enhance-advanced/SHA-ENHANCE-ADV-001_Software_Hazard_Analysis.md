# Software Hazard Analysis - XPE Advanced Enhancement Module

**Document ID:** SHA-ENHANCE-ADV-001 v1.0  
**IEC 62304 Clause:** 5.1.9 (Software Hazard Analysis)  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Safety & Risk Management Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

This Software Hazard Analysis (SHA) identifies potential hazards arising from the XPE Advanced Enhancement Module and specifies risk mitigation controls per ISO 14971:2019. Analysis covers Stage 9-11 processing and SWU-2.10 EI correction.

---

## 2. Hazard Analysis

### HAZ-ADV-001: Edge Enhancement Overshoot → Halo Artifacts → False Clinical Finding

#### 2.1 Hazard Description

Uncontrolled edge enhancement amplification (unsharp masking without overshoot limiting) creates bright halo rings around edges (e.g., cardiac borders). Operator may misinterpret halo as pathological feature (e.g., cardiac dilation, pleural effusion border).

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Major | False clinical finding could impact diagnosis or treatment |
| **Probability (Pre-mitigation)** | Medium | Edge enhancement is standard; overshoot occurs if limiter fails |
| **Risk Score** | High | Major × Medium |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-001.1** | Overshoot limiting (±3σ_local clipping) is MANDATORY, non-configurable | Design | Code review, no configuration switch |
| **CTL-ADV-001.2** | Disable attempt → error `XPE_ERR_SAFETY_VIOLATION` | Implementation | Configuration validation test |
| **CTL-ADV-001.3** | Alert if >1% pixels clipped (indication of over-aggressive α) | Implementation | Clipping detection & logging test |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Very Low | Mandatory control prevents disable; clipping alerts user |
| **Risk Score** | Very Low | Major × Very Low |
| **Status** | Mitigated | Risk acceptable |

---

### HAZ-ADV-002: Noise Reduction Over-Smoothing → Lesion Obscuration → Missed Diagnosis

#### 2.1 Hazard Description

Excessive smoothing (high σ or strong filter) reduces spatial resolution. Fine pathological details (microcracks, small nodules) become invisible. Radiologist misses diagnosis.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Critical | Missed diagnosis could harm patient |
| **Probability (Pre-mitigation)** | Medium | Over-smoothing possible if poor parameter choice |
| **Risk Score** | Critical | Critical × Medium |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-002.1** | MTF preservation constraint: noise reduction shall not degrade MTF by > 5% at Nyquist (validated via wire phantom). Tier 4 parameters auto-tuned in calibration to meet this constraint. | Design | Wire phantom acquisition, LSF-to-MTF analysis |
| **CTL-ADV-002.2** | Tier selection logic: mode="Standard" (default) uses Tier 2 (bilateral, MTF loss ~3-5%). Ultra modes (Tier 3-4) documented as "research" with explicit user selection required. | Implementation | Default mode validation, documentation |
| **CTL-ADV-002.3** | Alert if calculated MTF loss > 5%: "Warning: Noise reduction MTF loss exceeds 5%. Consider using lower tier." | Implementation | MTF monitoring during processing |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Low | MTF constraint bounds smoothing; defaults are conservative |
| **Risk Score** | Medium | Critical × Low |
| **Status** | Mitigated | Risk acceptable with constraint verification |

---

### HAZ-ADV-003: Collimation ROI False Detection → Wrong ROI → EI Error → Missed Dose Alert

#### 2.1 Hazard Description

Hough ROI detection fails or returns wrong boundary (too large, too small, or shifted). Resulting EI calculation is inaccurate. If |DI| error > 3, QC alert misses over/under-exposure. Patient receives sub-optimal or excessive dose.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Major | Dose error affects image quality and patient safety |
| **Probability (Pre-mitigation)** | Medium | Hough detection fails ~20% in low-contrast images |
| **Risk Score** | High | Major × Medium |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-003.1** | Confidence threshold: if confidence ≤ 0.7, automatically fallback to full-image EI (SWU-2.0). No user override possible. | Design | Automatic fallback logic, no config switch |
| **CTL-ADV-003.2** | Fallback is logged: "ROI confidence (%.2f) below 0.7. Using full-image EI." Operator sees reason for fallback. | Implementation | Logging validation test |
| **CTL-ADV-003.3** | Full-image EI is less accurate than ROI-masked (includes shielded area) but valid and safe. EI error from full-image ≤ 10% in typical collimation. | Analysis | Phantom validation: compare ROI vs full-image EI across scenarios |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Very Low | Automatic fallback to valid method (full-image EI) |
| **Risk Score** | Very Low | Major × Very Low |
| **Status** | Mitigated | Risk acceptable; fallback ensures valid measurement |

---

### HAZ-ADV-004: EI ROI Correction Using Log-Domain Data → Inaccurate DI → Dose Miscalculation

#### 2.1 Hazard Description

SWU-2.10 requires detector-domain data (uint16, calibrated, pre-log) for EI calculation. If developer mistakenly uses enhancement-domain (log-transformed) data, EI is non-linear and DI calculation is invalid. Dose assessment fails.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Major | Invalid DI compromises dose tracking |
| **Probability (Pre-mitigation)** | Low | Developer error; clear API documentation mitigates |
| **Risk Score** | Medium | Major × Low |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-004.1** | API constraint: EI_ROI_Refiner input is explicitly detector_domain_data (uint16), NOT image (float32). Function signature enforces type. Documentation and comments state: "IMPORTANT: Use calibrated detector-domain data, NOT log-transformed enhancement-domain image." | Design | API review, code comments |
| **CTL-ADV-004.2** | Unit test: compare EI computed from detector-domain vs incorrectly-computed from log-domain. Verify DI error > 50% if log-domain used (detectable). | Testing | Unit test with synthetic data |
| **CTL-ADV-004.3** | Integration test: real phantom with known exposure. Verify EI_roi within ±5% of expected value. If EI_roi error > 10%, alert user and recommend recalibration. | Testing | Phantom acquisition, gold-standard dosimeter |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Very Low | Strong API design + documentation prevents misuse |
| **Risk Score** | Very Low | Major × Very Low |
| **Status** | Mitigated | Type-safe design prevents common error |

---

### HAZ-ADV-005: Noise Reduction MTF Loss Exceeds Specification → Spatial Resolution Below Diagnostic Standard

#### 2.1 Hazard Description

During clinical use, MTF loss from noise reduction exceeds pre-defined 5% limit (e.g., due to incorrect tier selection or parameter drift). Spatial resolution falls below diagnostic standard. Small lesions become imperceptible.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Major | Reduced resolution compromises diagnostic accuracy |
| **Probability (Pre-mitigation)** | Medium | Manual parameter selection could be wrong |
| **Risk Score** | High | Major × Medium |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-005.1** | Default mode="Standard" (Tier 2, bilateral) has MTF loss ~3-5%, known to be safe. Ultra modes require explicit user selection and come with disclaimer: "Ultra modes for research only." | Implementation | Default mode documentation |
| **CTL-ADV-005.2** | Factory calibration: Tier parameters are tuned and locked to ensure MTF constraint < 5%. Clinical sites cannot modify tier parameters (read-only config). | Design | Configuration read-only enforcement |
| **CTL-ADV-005.3** | QC protocol: wire phantom MTF measured monthly. If measured MTF loss > 5%, alert: "MTF degradation detected. Check detector optics and recalibrate." | Operational | Monthly QA protocol, maintenance procedure |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Low | Conservative defaults + locked parameters prevent drift |
| **Risk Score** | Medium | Major × Low |
| **Status** | Mitigated | Regular QA validates MTF performance |

---

### HAZ-ADV-006: Noise Reduction Hallucination (NLM) → Artificial Structure → False Diagnosis

#### 2.1 Hazard Description

Non-Local Means (NLM, Tier 3) denoising can "reconstruct" missing details via patch similarity matching, creating artificial structures (hallucinations) not present in original image. Radiologist misinterprets hallucination as pathology.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Major | False findings can impact diagnosis |
| **Probability (Pre-mitigation)** | Low | NLM is conservative algorithm; hallucinations rare but possible in very low-dose images |
| **Risk Score** | Medium | Major × Low |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-006.1** | NLM is Tier 3 (Premium mode), not default. Ultra mode (Tier 4, Wavelet) is safer for low-dose imaging. Documentation recommends Tier 4 for low-dose (<1 mGy) imaging. | Design | Mode documentation, guidelines |
| **CTL-ADV-006.2** | Tier 3 (NLM) h parameter is auto-computed: h = κ × σ_noise. In low-SNR areas, κ auto-decreases to reduce filtering. This conservative approach limits hallucination risk. | Implementation | h parameter tuning test with synthetic noise |
| **CTL-ADV-006.3** | Visual inspection: radiologist training includes "NLM may produce subtle artifacts in very low-dose images; recommend Standard (Tier 2) for clinical confidence." | Operational | Training materials, clinical guidelines |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Very Low | NLM not default; conservative h tuning; operator awareness |
| **Risk Score** | Very Low | Major × Very Low |
| **Status** | Mitigated | Risk acceptable with mode selection and training |

---

### HAZ-ADV-007: Sidecar JSON I/O Failure → ROI Not Found → EI Fallback Not Triggered → Missing Dose Alert

#### 2.1 Hazard Description

Sidecar JSON file generation fails (disk full, permissions error) or corruption occurs during write. ROI sidecar is never created. EI_ROI_Refiner cannot read sidecar; confidence is treated as 0.0 (not available). Fallback to full-image EI occurs, but without explicit knowledge of ROI detection result, QA may not investigate further.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Minor | Fallback to full-image EI is safe; dose alert may just be less accurate |
| **Probability (Pre-mitigation)** | Low | Disk errors are rare in clinical systems |
| **Risk Score** | Low | Minor × Low |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-007.1** | Sidecar write error is logged: "Warning: Failed to write ROI sidecar (reason: disk full). Proceeding with full-image EI." User is notified of fallback. | Implementation | Error logging and propagation test |
| **CTL-ADV-007.2** | Sidecar read error (file missing or corrupted) is handled gracefully: confidence = 0.0, automatic fallback to full-image EI. | Implementation | File I/O error handling test |
| **CTL-ADV-007.3** | Sidecar write is retried once with different path if primary fails (e.g., fallback directory). | Implementation | Retry logic test |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Very Low | Robust error handling + fallback + retry |
| **Risk Score** | Very Low | Minor × Very Low |
| **Status** | Mitigated | Risk acceptable; graceful fallback ensures EI |

---

### HAZ-ADV-008: High-Noise Image - Bilateral Filter Divergence → Pixel Runaway

#### 2.1 Hazard Description

In images with extreme noise (e.g., misaligned detector, severely saturated), bilateral filter's adaptive range weighting σ_r can diverge, causing pixel values to spike to float32 limits. Downstream processing (display, storage) may overflow or produce invalid images.

#### 2.2 Risk Assessment

| Factor | Value | Justification |
|--------|-------|---------------|
| **Severity** | Major | Invalid image values corrupt diagnostic output |
| **Probability (Pre-mitigation)** | Very Low | Detector should reject severely noisy frames; but possible during detector malfunction |
| **Risk Score** | Low | Major × Very Low |

#### 2.3 Mitigation Controls

| ID | Control | Type | Verification |
|----|---------|------|--------------|
| **CTL-ADV-008.1** | Bilateral filter includes bounds-checking: pixel output capped to [0, 3.4e38] (float32 max). No infinity or NaN produced. | Implementation | Numerical bounds test with extreme noise |
| **CTL-ADV-008.2** | Input validation: if image contains NaN or infinity, bilateral filter is skipped with alert: "Invalid input (NaN/Inf detected). Skipping noise reduction." | Implementation | Invalid input handling test |
| **CTL-ADV-008.3** | Upstream validation in xpe_enhance_basic should reject detector images with extreme noise. Advanced enhancement should never receive pathological input under normal conditions. | Design | Upstream validation in preprocessing |

#### 2.4 Post-Mitigation Risk

| Factor | Value | Justification |
|--------|-------|---------------|
| **Probability (Post-mitigation)** | Very Low | Bounds-checking + input validation + upstream detection |
| **Risk Score** | Very Low | Major × Very Low |
| **Status** | Mitigated | Numerical robustness ensures safe output |

---

## 3. Risk Summary Table

| Hazard ID | Hazard | Pre-Risk | Post-Risk | Status | Notes |
|-----------|--------|----------|-----------|--------|-------|
| HAZ-ADV-001 | Overshoot → halo → false finding | High | Very Low | Mitigated | Mandatory limiting |
| HAZ-ADV-002 | Over-smoothing → obscured lesion | Critical | Medium | Mitigated | MTF constraint |
| HAZ-ADV-003 | ROI false detection → missed alert | High | Very Low | Mitigated | Auto fallback |
| HAZ-ADV-004 | Wrong data domain → invalid DI | Medium | Very Low | Mitigated | Type-safe API |
| HAZ-ADV-005 | MTF loss drift → resolution below spec | High | Medium | Mitigated | QA protocol |
| HAZ-ADV-006 | NLM hallucination → false finding | Medium | Very Low | Mitigated | Mode selection |
| HAZ-ADV-007 | Sidecar I/O failure → missing alert | Low | Very Low | Mitigated | Graceful fallback |
| HAZ-ADV-008 | Bilateral divergence → pixel runaway | Low | Very Low | Mitigated | Bounds-checking |

---

## 4. Design and Configuration Risk Controls

### 4.1 Non-Configurable Safety Settings

The following are FROZEN at design-time and cannot be modified:

| Setting | Value | Rationale |
|---------|-------|-----------|
| **Overshoot limit enabled** | TRUE | Prevents halos (HAZ-001) |
| **ROI fallback on low confidence** | TRUE | Prevents false EI (HAZ-003) |
| **MTF constraint (noise reduction)** | < 5% | Prevents lesion obscuration (HAZ-002) |
| **Tier parameter read-only** | TRUE | Factory-tuned, prevent drift (HAZ-005) |
| **Default mode** | "Standard" (Tier 2) | Conservative, balances quality (HAZ-002, HAZ-006) |

### 4.2 Operational Risk Controls

| Control | Implementation | Frequency |
|---------|----------------|-----------|
| Wire phantom MTF measurement | QC protocol | Monthly |
| Detector image validation | Preprocessing | Per-acquisition |
| Sidecar existence check | QA procedure | Per-session |
| DI accuracy verification | Phantom dosimetry | Quarterly |

---

## 5. Traceability

- **SRS-ENHANCE-ADV-001**: Safety requirements (SAF-100, SAF-101, SAF-102) map to this SHA
- **RTM-ENHANCE-ADV-001**: Requirements-to-hazard mapping
- **TDS-ENHANCE-ADV-001**: Test cases validate hazard mitigation

---

*SHA-ENHANCE-ADV-001 v1.0 끝*
