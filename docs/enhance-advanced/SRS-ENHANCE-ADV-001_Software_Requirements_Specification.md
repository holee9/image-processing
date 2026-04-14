# Software Requirements Specification - XPE Advanced Enhancement Module

**Document ID:** SRS-ENHANCE-ADV-001 v1.0  
**IEC 62304 Clause:** 5.2 (Software Requirements Specification)  
**Safety Classification:** Class B  
**Date:** 2026-04-14  
**Author:** XPE Advanced Enhancement Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

### 1.1 Purpose

This Software Requirements Specification (SRS) defines all functional, safety, performance, and interface requirements for the XPE Advanced Enhancement Module (`xpe_enhance_advanced.dll`, Layer 1, Phase 2). The module implements four processing stages for enhancement-domain (log-transformed) float32 images: noise reduction (4 tiers), edge enhancement with overshoot limiting, collimation ROI detection, and EI ROI-based dose correction.

### 1.2 Scope

This document specifies requirements for all four processing stages and their integration. Enhancement is performed in the log-transformed enhancement domain (post-basic enhancement, pre-display). The module is optional for clinical workflows but when enabled, all safety and quality requirements must be met.

**Out of scope:** Basic enhancement (log transform, CLAHE) is handled by `xpe_enhance_basic.dll`. Display pipeline is handled by `xpe_display.dll`.

---

## 2. Functional Requirements

### 2.1 Stage 9: Noise Reduction (4 Tiers)

#### FR-100: Tier 1 - Gaussian Blur

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-100.1** | System shall implement Gaussian blur noise reduction with configurable σ (sigma) parameter. Range: σ ∈ [0.5, 3.0] pixels. Execution time shall not exceed 15ms on reference hardware (Intel Core i7). | Gaussian blur is fast approximation to optimal Wiener filter for white noise. Configurable σ allows speed-quality tradeoff for real-time fluoroscopy. | Test: Parameter validation, timing profiling |
| **FR-100.2** | System shall use separable 1D Gaussian convolution (horizontal pass + vertical pass) for computational efficiency. Output shall be identical to 2D convolution within float32 precision (error < 1e-6). | Separable convolution reduces complexity from O(w²) to O(w), enabling real-time processing. | Test: Numerical accuracy vs 2D reference |
| **FR-100.3** | System shall handle boundary pixels using mirror padding (Neumann boundary conditions). Padding shall extend beyond the image border by σ pixels to reduce edge artifacts. | Mirror padding prevents artificial edge darkening from Gaussian kernel. Essential for seamless processing. | Test: Edge pixel comparison with interior |
| **FR-100.4** | Tier 1 shall be selectable for Fast mode (fluoroscopy). When Tier 1 is selected, noise reduction target is 20-30% with MTF loss ≤ 8%. | Fluoroscopy requires <50ms per frame. Tier 1 meets this requirement while maintaining acceptable image quality. | Test: Visual quality assessment |

#### FR-200: Tier 2 - Bilateral Filter

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-200.1** | System shall implement bilateral filter with spatial (σ_s) and range (σ_r) weighting. Spatial range: σ_s ∈ [1.0, 5.0] pixels. Range weighting shall be adaptive: σ_r(x,y) = α × σ_local(x,y), where α ∈ [0.5, 1.5] (configurable). | Bilateral filter preserves edges (unlike Gaussian) by weighting with both spatial distance and intensity difference. Adaptive σ_r optimizes for local SNR. | Test: Parameter ranges, edge preservation |
| **FR-200.2** | System shall compute local σ_local using 7×7 window. σ_local shall be computed per-pixel or in 8×8 blocks for efficiency trade-off (configurable). | Local σ enables signal-dependent noise model (quantum + electronic noise). Blocking reduces computation by 64x with minimal quality loss. | Test: Local statistics accuracy |
| **FR-200.3** | Execution time shall not exceed 60ms for 3072×3072 image with window size 7×7. | Standard clinical imaging requires <200ms per frame. Bilateral represents ~30% budget. | Test: Timing profiling on reference hardware |
| **FR-200.4** | System shall detect and skip bilateral filter if input appears to contain no noise (local variance everywhere > 0.1 × global mean). Alert user to "Unnecessary bilateral filtering configured". | Bilateral filtering adds computation cost. Detecting noise-free images allows bypass. | Test: Variance analysis and alert logging |

#### FR-300: Tier 3 - Non-Local Means (NLM)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-300.1** | System shall implement Non-Local Means (NLM) denoising with patch size 7×7 and search window 21×21. Patch similarity weighting: w(i,j) = exp(-‖P_i - P_j‖²_2 / h²), where h is filter strength (configurable). | NLM exploits image redundancy (self-similarity) for robust denoising. 7×7 patch and 21×21 window are standard for balance. | Test: Patch similarity computation, weight distribution |
| **FR-300.2** | System shall auto-compute filter strength h = κ × σ_noise, where κ ∈ [0.8, 1.5] (configurable) and σ_noise is estimated from image Laplacian. Equation: σ_noise = median(|∇²I|) / 0.6745 (robust estimator). | Auto h selection enables parameter-free operation. Laplacian-based estimation is robust to image statistics. | Test: Noise estimation accuracy vs ground truth |
| **FR-300.3** | System shall support patch similarity optimization: truncate search window to only patches with distance < 2×h to reduce computation by 40-60% without quality loss. | Full search is O(n⁴), prohibitively slow. Truncation maintains quality with practical performance. | Test: Similarity threshold impact on output |
| **FR-300.4** | Execution time shall not exceed 150ms for 3072×3072 image. Parallelization (OpenMP) is recommended to achieve target on multi-core processors. | Premium quality mode targets ~200ms total. NLM is 50-75% of budget and parallelizable. | Test: Single-thread and multi-thread timing |
| **FR-300.5** | System shall generate alert if execution exceeds 150ms, recommending Tier 2 or Fast mode. | User should be aware of performance implications of Tier 3 in time-critical workflows. | Test: Timing alert triggering |

#### FR-400: Tier 4 - Wavelet Shrinkage (BayesShrink)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-400.1** | System shall implement wavelet decomposition using Daubechies db4 wavelet with 3-4 decomposition levels. Decomposition: {cA, cD_h, cD_v, cD_d} per level. | db4 is orthogonal and symmetric, suitable for biomedical images. 3-4 levels cover frequency range relevant to X-ray imaging. | Test: Wavelet coefficient computation |
| **FR-400.2** | System shall compute threshold per subband using BayesShrink formula: threshold_k = σ²_n(k) / max(σ_s(k), σ_min), where σ_n(k) = Laplacian-based noise estimate, σ_s(k) = √(max(0, σ_y(k)² - σ_n(k)²)), σ_min = 1e-6 (regularization). | BayesShrink balances noise suppression (small threshold) with signal preservation (large σ_s). Regularization prevents division by zero. | Test: Subband threshold computation |
| **FR-400.3** | System shall apply hard thresholding: coeff_shrunk = coeff if |coeff| > threshold, else 0. Soft thresholding is NOT supported (hard is standard for BayesShrink). | Hard thresholding preserves significant signal components. Soft thresholding biases coefficients and introduces artifacts in medical images. | Test: Thresholding operation correctness |
| **FR-400.4** | System shall perform inverse wavelet transform (IDWT) to reconstruct denoised image. Output shall be float32, identical to original data type. | Reconstruction must be numerically stable and invertible. | Test: Forward-inverse DWT cycle precision |
| **FR-400.5** | Execution time shall not exceed 300ms for 3072×3072 image with 4 decomposition levels. | Ultra-quality mode allows slower processing. 300ms is acceptable for research/offline workflows. | Test: Timing profiling for all decomposition levels |

#### FR-500: Tier Selection Logic

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-500.1** | System shall select noise reduction tier based on mode configuration: (1) if mode=="Fast" → Tier 1; (2) if mode=="Standard" → Tier 2; (3) if mode=="Premium" → Tier 3; (4) if mode=="Ultra" → Tier 4. If no mode specified, default to Standard (Tier 2). | Explicit mode mapping prevents ambiguity. Standard is safe default for clinical use. | Test: Mode selection logic coverage |
| **FR-500.2** | System shall log the selected tier and execution time in diagnostic output. Tier information shall be included in frame metadata (flags field). | Traceability: users need to know which algorithm processed their images. | Test: Logging completeness, metadata population |
| **FR-500.3** | System shall validate noise tier parameter against range [1, 4]. Values outside this range shall return error `XPE_ERR_INVALID_PARAM`. | Range validation prevents silent misconfiguration. | Test: Out-of-range parameter handling |

#### FR-600: MTF Constraint (Mandatory)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-600.1** | Noise reduction shall not degrade MTF (Modulation Transfer Function) by more than 5% at Nyquist frequency. MTF shall be measured from wire phantom (1mm tungsten wire) using 1D LSF method. Target: MTF(f_Nyquist) ≥ 0.95 × MTF(f_Nyquist, original). | MTF loss → reduced spatial resolution → missed fine details. 5% loss is imperceptible; beyond 5% indicates over-smoothing. | Test: Wire phantom acquisition, LSF-to-MTF conversion, frequency response analysis |
| **FR-600.2** | Tier 4 (Wavelet) parameters shall be adjusted in calibration phase to meet MTF constraint. If target MTF cannot be achieved, system shall alert user and recommend weaker tier. | Wavelet parameters (decomposition level, threshold multiplier) directly affect MTF. Calibration ensures clinical images meet quality standard. | Test: Iterative parameter tuning in lab |

#### FR-700: Signal-Dependent Noise Model

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-700.1** | All noise reduction tiers shall account for signal-dependent (quantum) noise where σ²_quantum(I) ∝ I, plus constant (electronic) noise σ²_e. Total noise: σ²_total(x,y) = I(x,y) + σ²_e. | Accurate noise model enables algorithm parameters (h, threshold) to adapt to local SNR. Naive algorithms treat all noise equally, wasting computation in low-noise areas. | Test: Noise characterization on flat-field phantom at multiple exposure levels |
| **FR-700.2** | At low exposure levels (I < 10% max), electronic noise dominates (σ²_e > σ²_q). Denoising shall reduce strength in these areas to prevent over-smoothing and artifact creation. Implementation: κ parameter in h = κ × σ_noise shall auto-decrease to 0.6 in low-SNR areas. | Low-dose imaging has high noise. Over-aggressive denoising creates hallucinations and false structures. Signal-dependent adaptation prevents this. | Test: Noise characterization in low-dose images |

---

### 2.2 Stage 10: Edge Enhancement

#### FR-800: Unsharp Masking

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-800.1** | System shall implement unsharp masking: I_enhanced(x,y) = I(x,y) + α × (I(x,y) - I_blur(x,y)), where α ∈ [0.1, 2.0] (configurable), I_blur is Gaussian with σ_blur ∈ [1.0, 3.0] (configurable). | Unsharp masking amplifies high-frequency components (edges, details). Clinical standard for enhancing fine structures. | Test: Parameter validation, visual assessment |
| **FR-800.2** | System shall compute I_blur using Gaussian blur with configurable σ_blur. The difference (I - I_blur) isolates frequencies in band [1/(2πσ_blur), ∞). Different σ_blur values target different detail scales. | Frequency band selection allows anatomy-aware enhancement: low-frequency for structures, high-frequency for fine detail. | Test: Frequency response analysis |
| **FR-800.3** | Default parameters: α=0.8, σ_blur=2.0 (suitable for standard clinical imaging). Users may override via configuration. | Defaults balance enhancement visibility with artifact prevention. | Test: Default behavior validation |

#### FR-900: Overshoot Limiting (Mandatory Safety)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-900.1** | Overshoot limiting is MANDATORY and non-configurable. Enhancement effect shall be clipped to ±3σ_local, where σ_local is the standard deviation of a local 3×3 window around pixel (x,y). Formula: limited_boost(x,y) = clamp(α × (I - I_blur), -3σ_local, +3σ_local). | Overshoot → halo artifacts and pseudo-edges (false clinical findings). 3σ corresponds to 95% confidence interval; preventing 5% outlier amplification. Mandatory flag ensures this protection is never disabled. | Test: Artifact inspection on phantoms and clinical images |
| **FR-900.2** | System shall reject configuration with overshoot_limit_enabled=false. Return error `XPE_ERR_SAFETY_VIOLATION` and log: "Overshoot limiting is mandatory and cannot be disabled". | Configuration safety-check prevents accidental clinical use with overshoot enabled. | Test: Configuration validation |
| **FR-900.3** | If computed boost exceeds ±3σ_local, system shall alert user: "Edge enhancement clipped at (count) pixels. Consider reducing α parameter." This count and ratio shall be logged. | User awareness of clipping enables parameter tuning. High clipping ratio indicates over-aggressive configuration. | Test: Clipping detection and logging |

#### FR-1000: Multi-Band Edge Enhancement (Advanced)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1000.1** | Advanced mode shall support frequency-band-dependent α: α_low, α_mid, α_high for low/mid/high frequency bands respectively. Decomposition via Laplacian pyramid or FFT (configurable). | Anatomy-specific enhancement: subtle structures (low-freq) require less boost than diagnostic details (mid-freq), which require less than noise (high-freq). | Test: Multi-band decomposition, frequency response verification |
| **FR-1000.2** | Recommended defaults: α_low=0.3, α_mid=1.0, α_high=0.5. | Balance of anatomy and detail enhancement without noise amplification. | Test: Default multi-band behavior |
| **FR-1000.3** | Multi-band mode is optional; single-band (FR-800) is standard. Documentation shall recommend single-band for clinical use, multi-band for research. | Complexity vs benefit. Clinical workflows prefer predictability; research can afford tuning. | Test: Mode selection and behavior |

---

### 2.3 Stage 11: Collimation ROI Detection

#### FR-1100: ROI Detection via Hough Transform

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1100.1** | System shall detect collimation borders using Hough line transform: (1) compute edge map via Canny or Sobel gradient; (2) apply Hough transform to accumulate votes for (θ, ρ) pairs; (3) extract peaks (threshold: 70% of max accumulator value); (4) filter for axis-aligned lines (θ ≈ 0° or 90°, tolerance ±5°); (5) compute 4 corner intersections → rectangular ROI. | Hough transform is robust to missing/noisy edges. Axis alignment constraint exploits rectangular collimators. | Test: Hough accumulator computation, line extraction, ROI fitting |
| **FR-1100.2** | Edge detection shall use Canny operator with threshold range [50, 150] (configurable). Canny output is binary edge map. Alternative: Sobel magnitude with threshold (configurable). | Canny is more robust to noise than simple gradient. Threshold range allows sensitivity adjustment. | Test: Edge map quality, threshold impact |
| **FR-1100.3** | Hough accumulator resolution: θ step = 1° (or 0.5° for high-precision mode), ρ step = 1 pixel. Accumulator peak detection: local maximum with minimum count = max_count × 0.7. | 1° resolution sufficient for rectangular collimators. 0.5° enables sub-degree accuracy for oblique edges (rare). | Test: Resolution impact on ROI accuracy |
| **FR-1100.4** | Axis alignment filter: select only lines with θ ∈ [-5°, +5°] ∪ [85°, 95°]. Lines outside this range are discarded. Purpose: reject oblique or non-rectangular collimation. | Rectangular collimators have horizontal and vertical edges only. Oblique edges indicate unusual geometry or detection error. | Test: Filter correctness, handling of near-axis edges |

#### FR-1200: Confidence Scoring

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1200.1** | System shall compute confidence score: confidence = (sum of 4 accumulator peak values) / (4 × max_accumulator_value). Range: [0.0, 1.0]. Interpretation: (1) confidence > 0.7 → high (ROI used); (2) 0.5-0.7 → medium (re-check recommended); (3) < 0.5 → low (fallback to full image). | Confidence quantifies detection robustness. High-confidence detections are used for EI correction; low-confidence fall back to full-image EI. | Test: Confidence computation, fallback logic |
| **FR-1200.2** | Confidence score shall be reported in ROI sidecar JSON. If confidence < 0.7, system shall automatically use full-image EI (SWU-2.0 fallback) and log: "ROI confidence (%.2f) below threshold 0.7. Using full-image EI." | Fallback logic ensures safe degradation: lower quality is better than false ROI. Logging enables diagnostic traceability. | Test: Fallback triggering, logging |

#### FR-1300: ROI Output Format

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1300.1** | ROI shall be output to JSON sidecar file at path: `{image_base_path}.roi.json`. Format: `{ "roi": {x, y, width, height}, "confidence": 0.0-1.0, "timestamp_ms": epoch_ms, "detector_id": string, "collimation_type": string }`. | JSON is human-readable and language-agnostic. Sidecar keeps ROI data near image file. | Test: JSON parsing, file location, field completeness |
| **FR-1300.2** | ROI coordinates (x, y, width, height) shall be integer pixel indices. Boundaries shall be clipped to image dimensions: 0 ≤ x < width_image, 0 ≤ y < height_image. | Integer coordinates prevent aliasing. Clipping prevents out-of-bounds access in downstream code. | Test: Boundary clipping logic |
| **FR-1300.3** | If ROI detection fails (confidence < 0.5 AND cannot fit rectangle), system shall generate sidecar with null/empty ROI and confidence=0.0. No error shall be raised; fallback to full image is automatic. | Graceful degradation: detection failure does not crash pipeline. Fallback ensures images are still processable. | Test: Failure handling, null ROI generation |

---

### 2.4 SWU-2.10: EI ROI-Based Dose Correction

#### FR-1400: EI Calculation with ROI Masking

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1400.1** | System shall compute exposure index (EI) using detector-domain data (uint16 or calibrated float32, pre-log-transform). If ROI confidence > 0.7: EI_roi = mean(detector_domain_pixels[ROI]). If confidence ≤ 0.7: EI_full = mean(detector_domain_pixels[all]), use SWU-2.0 EI. | EI definition requires physical detector signal, not log-transformed image. ROI masking excludes shielded (direct beam) areas, improving measurement accuracy. | Test: Detector-domain data sourcing, ROI masking, mean computation |
| **FR-1400.2** | System shall verify that detector-domain data is available and uncorrupted. If detector-domain data is unavailable, system shall log warning "Detector-domain data unavailable. Cannot compute ROI-based EI." and use SWU-2.0 EI fallback. | Detector-domain data may be released or unavailable in some workflows. Graceful fallback maintains EI computation without errors. | Test: Data availability check, fallback triggering |
| **FR-1400.3** | ROI reference shall be from sidecar file generated in Stage 11. Sidecar read shall handle missing/corrupted files: if sidecar file missing or unreadable, treat as confidence=0.0 (fallback). | Sidecar may be accidentally deleted or corrupted. Robust fallback prevents EI calculation failure. | Test: Sidecar I/O error handling |

#### FR-1500: DI Computation and QC Alerting

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1500.1** | System shall compute Dose Index (DI) per IEC 62494-1: DI = 10 × log10(EI / EI_ref), where EI_ref is the target EI (user-configurable, typical range 100-500 depending on detector and application). Output DI shall be float32. | DI is standard metric for exposure assessment. Log scale allows intuitive interpretation: DI=0 → target, |DI|=1 → ±26% exposure error. | Test: DI computation accuracy, IEC 62494-1 conformance |
| **FR-1500.2** | System shall issue QC alert if |DI| > 3: "ALERT: Exposure out of range (|DI| = %.1f > 3). Recommend image re-acquisition." Alert severity: WARNING (does not block image, but flags for review). | |DI| > 3 means >200% exposure error (100× dose). Beyond acceptable range for dose compliance. Alert prompts operator decision. | Test: Alert triggering threshold, severity level |
| **FR-1500.3** | System shall log DI value, confidence, and alert status in diagnostic output and frame metadata. Log entry: "DI_roi=%.2f, confidence=%.2f, roi_box=[x,y,w,h]". | Traceability for dose audits and QA. Frame metadata enables post-acquisition analysis. | Test: Logging completeness, metadata population |

#### FR-1600: Fallback Logic

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1600.1** | If ROI confidence ≤ 0.7 OR detector-domain data unavailable OR sidecar unreadable, system shall automatically use full-image EI from SWU-2.0 (XPE preprocessing module) without error. Fallback shall be logged: "Using full-image EI (fallback reason: ROI confidence too low)". | Graceful degradation: always compute EI, even if ROI fails. Full-image EI is less accurate but valid. | Test: All fallback paths, logging, error-free execution |
| **FR-1600.2** | System shall never mix EI methods (ROI vs full-image) in same image. Either use ROI (confidence > 0.7) or use full-image, never both. | Mixing methods is ill-defined and creates inconsistency. | Test: EI method consistency check |

---

### 2.5 Integration and Metadata

#### FR-1700: Status Flags

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1700.1** | System shall set metadata flags for all applied processing: (1) XPE_FLAG_NOISE_DENOISED (tier used); (2) XPE_FLAG_EDGE_ENHANCED; (3) XPE_FLAG_ROI_DETECTED; (4) XPE_FLAG_EI_ROI_CORRECTED. Flags shall be set only if processing actually executed, not if bypassed. | Flags enable downstream modules and QA to determine which algorithms were applied. Missing flags indicate missing processing. | Test: Flag setting logic for each stage |
| **FR-1700.2** | If a stage is skipped (e.g., ROI detection confidence < 0.5), corresponding flag shall NOT be set. Absence of flag indicates stage was skipped or failed. | Clear distinction between success (flag set) and skip/failure (flag absent). | Test: Skip detection and flag omission |

#### FR-1800: Diagnostic Logging

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **FR-1800.1** | System shall log per-stage execution time, parameter values, and decision points. Log format (JSON): `{stage: "9", tier: 2, time_ms: 45, confidence: 0.85, alerts: []}`. Logs shall be included in frame metadata or written to diagnostic file. | Detailed logs enable performance tuning and issue diagnosis. JSON format enables automated parsing. | Test: Log generation, format validation |
| **FR-1800.2** | System shall log all alerts (clipping, low confidence, out-of-range DI, etc.) with timestamp and description. Alerts shall be propagated to user interface. | Operators need visibility into quality issues. Alerts drive decision-making (re-acquire, adjust parameters, etc.). | Test: Alert logging and UI integration |

---

## 3. Safety Requirements

### 3.1 Mandatory Safeguards

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SAF-100** | Overshoot limiting (FR-900.1) is MANDATORY. Disabling is prohibited via configuration and code review. | HAZ-ADV-001 (halo artifacts → false edge misinterpretation) | Overshoot creates pseudo-edges that can be misdiagnosed as pathology. Clinical safety risk. | Test: Overshoot disable attempt → error, code review for hardcoded override |
| **SAF-101** | ROI fallback logic (FR-1600.1) is MANDATORY. If ROI confidence ≤ 0.7, system shall automatically use full-image EI without user override. | HAZ-ADV-003 (ROI false detection → wrong EI → missed dose alert) | Wrong ROI can cause EI error. Automatic fallback ensures correct EI calculation even if ROI fails. | Test: Fallback automatic triggering, no manual override |
| **SAF-102** | MTF loss validation (FR-600.1) is required during calibration. Actual MTF loss ≥ 5% shall trigger alert and recommendation to weaken denoising. | HAZ-ADV-002 (over-smoothing → obscured lesion) | Excessive smoothing reduces spatial resolution and can hide fine pathological findings. Validation prevents this. | Test: Wire phantom MTF measurement, alert triggering |

---

## 4. Performance Requirements

### 4.1 Processing Speed

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **PERF-100** | Tier 1 (Gaussian) ≤ 15ms, Tier 2 (Bilateral) ≤ 60ms, Tier 3 (NLM) ≤ 150ms, Tier 4 (Wavelet) ≤ 300ms for 3072×3072 float32 frame on Intel Core i7 2.6GHz. | Clinical workflow: capture + preprocess (500ms) + basic enhance (100ms) + advanced enhance (this module, <300ms) + display (<100ms) = target <1 second. Advanced enhance must fit within 300ms budget. | Test: Profiling on reference hardware with timer instrumentation |
| **PERF-101** | Total pipeline time (Stage 9 + 10 + 11 + SWU-2.10) shall not exceed 300ms for Standard mode (Tier 2). | Standard mode is default for clinical use. 300ms budget ensures <1 second total processing per frame. | Test: End-to-end timing validation |

### 4.2 Memory Requirements

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **PERF-102** | Peak memory allocation ≤ 60MB for Tier 3 (NLM) or Tier 4 (Wavelet). Breakdown: work buffer (37.7MB) + patch buffers (10-15MB) + coefficient buffers (5-10MB). | Desktop console has ~4-8GB shared with UI. 60MB is <2% of system RAM, safe for long-running acquisitions. | Test: Memory profiling (valgrind, Windows Task Manager) over 100-frame batch |
| **PERF-103** | System shall free all temporary allocations after frame processing. No memory leaks detected in 100-frame batch test. | Long-running fluoroscopy can acquire 1000+ frames. Memory leaks cause OOM and crash. | Test: Memory leak detection (valgrind --leak-check=full) |

---

## 5. Interface Requirements

### 5.1 API Function Signatures (C ABI)

#### Noise Reduction

```c
// Tier 1: Gaussian
int xpe_enhance_denoise_gaussian(
    const float *input_image,
    float *output_image,
    int width, int height,
    float sigma
);

// Tier 2: Bilateral
int xpe_enhance_denoise_bilateral(
    const float *input_image,
    float *output_image,
    int width, int height,
    float sigma_spatial,
    float sigma_range_multiplier
);

// Tier 3: NLM
int xpe_enhance_denoise_nlm(
    const float *input_image,
    float *output_image,
    int width, int height,
    float h_strength
);

// Tier 4: Wavelet
int xpe_enhance_denoise_wavelet(
    const float *input_image,
    float *output_image,
    int width, int height,
    int decomposition_level
);

// Auto tier selection
int xpe_enhance_denoise(
    const float *input_image,
    float *output_image,
    int width, int height,
    const char *mode  // "Fast", "Standard", "Premium", "Ultra"
);
```

#### Edge Enhancement

```c
int xpe_enhance_edges(
    const float *input_image,
    float *output_image,
    int width, int height,
    float alpha,
    float gaussian_sigma,
    bool apply_overshoot_limit  // always true
);
```

#### ROI Detection and EI Correction

```c
typedef struct {
    int x, y, width, height;
    float confidence;
} XpeROI;

int xpe_detect_roi(
    const float *image,
    int width, int height,
    XpeROI *roi_out
);

int xpe_refine_ei_by_roi(
    const uint16_t *detector_domain_data,
    int width, int height,
    const XpeROI *roi,
    float ei_ref,
    float *di_out,
    bool *qc_alert
);

int xpe_sidecar_write(const char *image_path, const XpeROI *roi);
int xpe_sidecar_read(const char *image_path, XpeROI *roi_out);
```

---

## 6. Traceability

All requirements in this SRS trace to:
- **SAD-ENHANCE-ADV-001**: Software Architecture Document (SWU decomposition)
- **SHA-ENHANCE-ADV-001**: Software Hazard Analysis (hazard mitigation)
- **TDS-ENHANCE-ADV-001**: Test Dataset Specification (acceptance criteria)
- **RTM-ENHANCE-ADV-001**: Requirements Traceability Matrix (bidirectional mapping)

---

*SRS-ENHANCE-ADV-001 v1.0 끝*
