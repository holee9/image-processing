# XPE Algorithm Specification (DeepSync Upgrade v2)

**Document ID**: ALG-SPEC-001  
**Version**: 3.0.0-ds2  
**Date**: 2026-04-14  
**Status**: Working Draft  
**Project**: ImageProcTest - X-Ray Image Processing Engine (XPE)  
**Upgrade**: Deep Research + Cross-Verification + DeepSync Integration

---

## Changelog (v2.0.0-ds1 -> v3.0.0-ds2)

| Change | Detail |
|--------|--------|
| Calibration deep research | Integrated 15+ peer-reviewed papers, IEC standards, and patent references for calibration algorithm validation |
| Cross-verification v2.0 | Addressed all 20 issues from 3-round cross-verification report |
| Section 5 rewritten | Detector-domain corrections upgraded with research-validated mathematical models, performance targets, and quality gates |
| Section 5.3 NEW | Calibration data management lifecycle specification added |
| Section 5.4 NEW | Calibration drift detection and recalibration strategy |
| Section 5.5 NEW | Multi-gain and nonlinearity correction integration model |
| Section 9 NEW | Research-backed improvement roadmap with gap analysis |
| Section 6A NEW | Pre-processing stage dependency matrix and bypass (on/off) contract with 8 safety constraints |
| EI-0 resolution | Adopted Resolution B: SWU-2.0 EI_Baseline added to Phase 1b (xpe_enhance_basic.dll) |
| Pipeline order validated | Research-validated optimal ordering confirmed: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost |

---

## 1. Purpose

This specification is the normative algorithm contract for the XPE pre-processing calibration pipeline. It integrates:

- Local design documents (PRD-FPD-CAL-001, Ghost PRD v2, Panel Defect Plan)
- Cross-verification findings (XPE-XVER-001 v1.0.0, SPEC-XPE-MASTER cross-verification v2.0.0)
- Deep research against peer-reviewed literature and IEC/AAPM standards
- DeepSync conflict resolution decisions

This document supersedes ALG-SPEC-001 v2.0.0-ds1 for all algorithm behavior, quality gates, and phase ownership.

---

## 2. DeepSync Decisions (Carried Forward + New)

### 2.1 Carried Forward (from v2.0.0-ds1)

| Topic | Decision |
|-------|----------|
| Phase ownership | Phase 2 limited to deterministic classical algorithms. Phase 3 owns xpe_ai.dll and xpe_ai_worker.exe. |
| Collimation output | ROI stored in orchestration sidecar, not in XpeImageMetadata. |
| Metadata flags | XPE_FLAG_* is state-only. Failure details go through alert queue or diagnostic JSON. |
| Exposure Index | EI/DI computed from detector-domain, pre-presentation data only. |
| EI applicability | IEC 62494-1 applies to single irradiation event images only. |
| Detector QC metrics | MTF, NPS, DQE measured on detector-domain images only. |
| AI safety | AI modules assistive and degradable. Pipeline never blocks on AI worker. |

### 2.2 New Decisions (v3.0.0-ds2)

| Topic | Decision | Research Basis |
|-------|----------|---------------|
| Pipeline order | Nonlinearity correction BEFORE gain (stage 1.5), not after. Linearize pixel response before flat-field normalization. | Physical correctness: gain normalization assumes linear detector response. PRD Section 4.1 order (after gain) is superseded by pipeline-spec Section 1.2. |
| Multi-gain model | Multi-gain polynomial correction integrated INTO gain correction stage (2), not as separate stage. Gain map selection by exposure level is internal to xpe_gain_correct(). | Varex multi-gain calibration (6-10 signal levels), PRD-FPD-CAL-001 Section 5.2.4. |
| Defect correction strategy | Baseline: edge-aware bilinear interpolation. Advanced: MLP (FixPix architecture, 1425 parameters, FPGA-friendly). Research path only: CNN/ViT for cluster defects. | FixPix (2023): 14.2x NMSE improvement over linear interpolation. Concatenated CNN (PMC7930811): MSE 91.80 vs traditional TMC 243.6. Simple ANN achieves near-optimal with 18x fewer parameters. |
| Lag model selection | NLCSC with N=4 multi-exponential IRF as advanced tier. LTI deconvolution as baseline. Only 2 longest time constants treated as exposure-dependent for computational efficiency. | Starman et al. 2012 (PMC3465354): NLCSC achieves <0.29% first-frame, <0.0052% 50th-frame lag. 88% reduction over uncorrected. |
| Lag vs ghosting distinction | Lag = residual signal in subsequent frames (charge trap release). Ghosting = detector sensitivity change from prior exposures. Both corrected in stage (4) but via different mechanisms. | PMC5722609: For indirect-conversion FPD, lag (~1-4%) dominates over ghosting (~0.1%) at clinical doses. |
| Heel effect compensation | Duo-SID projection method (Wang 2013) adopted for arbitrary-SID gain map reconstruction from 2 reference calibrations. | Wang 2013: ~80% RMSE reduction vs single-SID, ~70% vs interpolation. |
| Temperature compensation model | Exponential dark current model: I_dark(T) = I_0 * exp(-E_g / 2*k_B*T). Dynamic dark map interpolation with PREP time and temperature. | EP2148500A1 patent. 23-month stability study showing 0.5% (1 SD) with dynamic correction. |
| Calibration drift strategy | Drift-aware recalibration with automatic scheduling based on temperature delta, elapsed time, and flat-field residual monitoring. | Kwan et al. 2006, Wenz et al. 2023: One-time factory constants insufficient for long-term clinical use. |
| EI-0 Phase assignment | NEW SWU-2.0 EI_Baseline in Phase 1b (xpe_enhance_basic.dll). Phase 2 adds ROI-aware refinement via SWU-2.10. | Resolves CRITICAL issue N11 from cross-verification v2.0. |

---

## 3. Source Base

### 3.1 Local Sources

- `.moai/plans/memoized-conjuring-aurora.md`
- `.moai/project/pipeline-spec.md` (v1.1.0, normative for pipeline order)
- `.moai/project/api-spec.md` (v1.1.0, target v1.2.0)
- `docs/xray_fpd_tech_classification_final.md`
- `docs/ghost-correction/srs_ghost_correction.md`
- `docs/ghost-correction/sad_ghost_correction.md`
- `docs/ghost-correction/sw_lag_correction_prd_v2.md`
- `docs/post-processing/xpe/XPE-SAD-001_Software_Architecture_Document.md`
- `docs/xray-fpd-research/xray-detector-calibration-prd.md` (PRD-FPD-CAL-001 v1.0.0)
- `docs/panel-defect-algorithm/plan.md`
- `.moai/specs/SPEC-XPE-MASTER/cross-verification-report.md` (v2.0.0)
- `docs/cross-verification-report-2026-04-13.md` (XPE-XVER-001 v1.0.0)

### 3.2 Public Technical Sources (Research-Validated)

| Domain | Source | Use in this spec | Validation Status |
|--------|--------|-----------------|-------------------|
| Lag correction (NLCSC) | Starman et al., Med Phys 2012, [PMC3465354](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) | Tiered lag design: LTI baseline + NLCSC advanced. N=4 multi-exponential IRF with signal-dependent coefficients. | **VERIFIED** - Full algorithm extracted, performance metrics confirmed |
| Lag vs ghosting model | Pang et al., Med Phys 2006, [PMC5722609](https://pmc.ncbi.nlm.nih.gov/articles/PMC5722609/) | Dual-exponential lag model. Lag (1-4%) dominates over ghosting (0.1%) for indirect-conversion FPD. | **VERIFIED** - Parameters and clinical significance confirmed |
| Gain/offset calibration SNR | Ranger et al., J Digit Imaging 2014, [PMC3965338](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/) | Gain/offset calibration reduces SNR variation. Recalibration indicated when SNR falls outside 95% CI. | **VERIFIED** - Quantitative SNR improvement data confirmed |
| Deep learning defect correction | Jeon et al., Phys Med 2021, [PMC7930811](https://pmc.ncbi.nlm.nih.gov/articles/PMC7930811/) | Concatenated CNN best MSE (91.80 for 5x5 defect vs TMC 243.6). Simple ANN nearly as good (94.67) with 18x fewer parameters (1425 vs 26891). | **VERIFIED** - Architecture and performance metrics confirmed |
| FixPix bad pixel correction | Schirrmacher et al., 2023, [arXiv:2310.11637](https://arxiv.org/html/2310.11637v2) | MLP-based correction with 14.2x NMSE improvement. Confidence-calibrated segmentation for detection. ViT auto-encoder for high corruption rates. | **VERIFIED** - Network architecture and FPGA feasibility confirmed |
| Unrolled dual-domain correction | 2026, [arXiv:2601.20995](https://arxiv.org/html/2601.20995) | Synthetic data training for low-performing pixel correction. Outperforms state-of-art for 1-2% detector defects. | **NEW** - Latest research, evaluation pending |
| Flat-field / recalibration drift | Kwan et al., 2006, [PubMed 16532945](https://pubmed.ncbi.nlm.nih.gov/16532945/) | Improved flat-field correction for multi-point calibration. | **VERIFIED** |
| Calibration drift management | Wenz et al., 2023, [PubMed 36897395](https://pubmed.ncbi.nlm.nih.gov/36897395/) | Drift handling and recalibration scheduling for clinical use. | **VERIFIED** |
| Heel effect (Duo-SID) | Wang, Med Phys 2013, [PDF](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) | Duo-SID projection for arbitrary-SID gain map from 2 references. 80% RMSE reduction. | **VERIFIED** |
| Dynamic dark correction | EP2148500A1, [Patent](https://patents.google.com/patent/EP2148500A1/en) | Offset adjustment maps for PREP time and temperature compensation. Portable detector power-mode handling. | **VERIFIED** |
| NUC algorithm | Multiple sources via [Science.gov](https://www.science.gov/topicpages/n/non-uniformity+correction+algorithm) | Two-point calibration (TPC) as baseline. Piecewise linear or polynomial for nonlinearity. | **VERIFIED** |
| Display calibration | DICOM PS3.14 GSDF | Presentation path preserves GSDF-consistent grayscale. Separate from detector-domain processing. | **VERIFIED** |
| Exposure Index | IEC 62494-1, AAPM TG-232 | EI/DI tied to detector-domain data. DI = 10 * log10(EI / EIT). | **VERIFIED** |
| DQE standard | IEC 62220-1-1:2015, [UMich PDF](https://websites.umich.edu/~ners580/ners-bioe_481/lectures/pdfs/2003-10-IEC_62220-DQE.pdf) | DQE(f) = MTF^2(f) / (NPS(f) * q). Calibration quality directly affects DQE measurement accuracy. | **VERIFIED** |
| Scatter / virtual grid | Lisson et al., 2020, Virtual Grid study 2022 | Phase-gated premium features requiring clinical image-quality validation. | **VERIFIED** |

---

## 4. Canonical Pipeline Contract

### 4.1 Pre-Processing Stages (Detector Domain)

Research-validated ordering. Each stage validated against physical correctness and published literature.

| Stage | Function | Data Domain | Owner | Phase | Fallback | Research Validation |
|-------|----------|------------|-------|-------|----------|-------------------|
| (0) | Calibration load | N/A | xpe_preprocess.dll | Startup | Fail startup if mandatory maps missing | Standard practice |
| (0.5) | Readout validation | Raw uint16 | xpe_preprocess.dll | Phase 1 | Flag + alert only, do not mutate | PRE-01: Pattern validation |
| (0.7) | Temperature compensation | Raw uint16 | xpe_preprocess.dll | Phase 1 | Use nominal 25C if sensor missing | EP2148500A1: Exponential dark current model |
| (1) | Offset correction | Raw uint16 | xpe_preprocess.dll | Phase 1 | Hard fail if dark map absent | IEC 62220-1-1: Dark field subtraction required |
| (1.5) | Nonlinearity correction | Raw uint16 | xpe_preprocess.dll | Phase 1 | Bypass only if panel profile says linear | NUC literature: Linearize BEFORE gain normalization |
| (2) | Gain correction | float32 | xpe_preprocess.dll | Phase 1 | Hard fail if gain map absent | IEC 62220-1-1: Flat-field normalization required |
| (2.5) | Binning correction | float32 | xpe_preprocess.dll | Phase 1 cond. | No-op if binning inactive | Mode-specific compensation |
| (3) | Defect correction | float32 | xpe_preprocess.dll | Phase 1 | Preserve original + alert if neighborhood unusable | PMC7930811: Edge-aware interpolation baseline |
| (4) | Lag / ghost correction | float32 | xpe_preprocess.dll | Phase 1 | Tier downgrade if history insufficient | PMC3465354: NLCSC tiered design |

### 4.2 Pipeline Order Validation (Research Cross-Check)

The pipeline order was validated against physical principles and published literature:

**Why Nonlinearity (1.5) comes BEFORE Gain (2):**
- Gain correction (flat-field normalization) assumes linear detector response
- If pixel response is nonlinear, gain normalization introduces systematic errors
- Two-point calibration (TPC) literature assumes linearized input for gain map application
- PRD-FPD-CAL-001 Section 4.1 shows nonlinearity after gain (superseded by this decision)

**Why Defect (3) comes AFTER Gain (2):**
- Gain-corrected image provides uniform background for defect detection
- Defect interpolation quality improves on normalized data
- BPM detection algorithms (robust statistics) work best on gain-corrected images

**Why Ghost/Lag (4) is LAST in pre-processing:**
- Lag correction requires clean, fully-corrected current frame as input
- NLCSC algorithm compares current frame against exposure history
- All systematic corrections must be applied before temporal artifact removal

### 4.3 Data-Domain Rule (Unchanged)

Three data domains shall not be mixed:
1. Detector domain: raw or detector-corrected frames (offset, gain, defect, lag, EI, QC)
2. Enhancement domain: log/contrast/edge/scatter processed frames
3. Presentation domain: GSDF/LUT-applied frames

EI/DI, lag residual, flat-field residual, MTF/NPS/DQE belong to detector domain only.

---

## 5. Detector-Domain Algorithm Profiles (Upgraded)

### 5.1 PRE-01: Readout Artifact Validation

| Aspect | Specification |
|--------|--------------|
| **Baseline** | Pattern validation: saturation, stuck rows/cols, clipped dynamic range, impossible readout geometry |
| **Advanced** | None |
| **Inputs** | Raw frame + acquisition metadata |
| **Release Gate** | False negatives on injected synthetic faults shall be zero in regression set |
| **Mutability** | Read-only stage. Shall NOT modify pixel data. Flag + alert only. |

### 5.2 PRE-02: Offset (Dark) Correction

| Aspect | Specification |
|--------|--------------|
| **Mathematical Model** | I_corrected(x,y) = I_raw(x,y) - I_dark(x,y) |
| **Dynamic Model** | I_dark_adjusted = (1-alpha) * I_dark_k + alpha * I_dark_{k+1}, where alpha interpolates by temperature and PREP time |
| **Dark Map Generation** | Factory: N >= 100 frames averaged, IQR outlier filtering, frequency decomposition (LF: median 11x11 + HF: frame averaging). Field: N >= 16 frames. |
| **Temperature Model** | I_dark(T) = I_0 * exp(-E_g / 2*k_B*T). Temperature interpolation within +/-2.5C of reference maps. |
| **Portable Detector** | Multi-capture mode (2 post-dark after exposure). Power-mode-specific offset adjustment maps. Min 1.5s PREP time enforcement. |
| **Drift Strategy** | Auto field update: elapsed > 30min OR temp delta > 3C. Factory recalibration: initial + annual. Emergency: QA failure or shock detection. |
| **Advanced Tier** | Drift-aware refresh scheduling with exponential PREP-time model: m(t) = x1 * exp(x2 * t + x3) |
| **Inputs** | Raw frame + dark map + temperature + PREP time metadata |
| **Release Gate** | Residual dark bias stable across temperature sweep (15-40C range). Post-correction mean dark level < 5 ADU. |
| **Performance** | < 1ms/frame (FPGA), < 55ms/frame (Host PC, 3072x3072) |
| **Research Basis** | EP2148500A1 (dynamic offset), PMC3965338 (calibration SNR optimization), 23-month stability study |

### 5.3 PRE-07: Temperature Compensation

| Aspect | Specification |
|--------|--------------|
| **Baseline** | Temperature LUT or low-order polynomial compensation |
| **Advanced** | Adaptive interpolation by detector profile |
| **Model** | Dark current exponential temperature dependency + LUT-based or polynomial correction per pixel region |
| **Inputs** | Raw frame + panel temperature (from NTC sensor) + calibration coefficients |
| **Release Gate** | Compensation must not destabilize flat-field residual across operating range (15-40C) |
| **Sensor Fallback** | If NTC sensor absent or failed: use nominal 25C compensation, flag XPE_FLAG_TEMP_COMPENSATED with degraded-mode alert |

### 5.4 PRE-08: Nonlinearity Correction

| Aspect | Specification |
|--------|--------------|
| **Baseline** | Inverse response LUT or monotonic polynomial linearization |
| **Multi-Gain Integration** | For detectors with multiple gain modes (e.g., Varex 6-mode): mode-specific LUT families. G(x,y,E) = sum(c_k(x,y) * E^k), K=1-3. |
| **NUC Model** | Two-point calibration (TPC) as minimum. Multi-point correction (5+ exposure levels) for high-accuracy applications. |
| **Piecewise Linear** | For simple detectors: piecewise linear correction with breakpoints at calibration knots |
| **Advanced** | Second-order polynomial NUC per pixel for improved accuracy with minimal stored coefficients |
| **Inputs** | Raw frame + response curve coefficients |
| **Release Gate** | Monotonic output. No introduced banding. Unit-tested at calibration knots. |
| **Ordering Rationale** | MUST precede gain correction (stage 1.5 before stage 2). Gain normalization assumes linear detector response. |
| **Research Basis** | NUC literature (Science.gov survey), Varex multi-gain calibration, PRD-FPD-CAL-001 Section 5.9 |

### 5.5 PRE-03: Gain (Flat-Field) Correction

| Aspect | Specification |
|--------|--------------|
| **Mathematical Model** | I_corrected(x,y) = (I_raw(x,y) - I_dark(x,y)) / G(x,y) |
| **Gain Map Formula** | G(x,y) = (F_avg(x,y) - D_avg(x,y)) / spatial_mean(F_avg - D_avg) over valid pixel region |
| **Multi-Gain Model** | G(x,y,E) = sum(c_k(x,y) * E^k), K=1-3. Polynomial fit from 5+ exposure levels (20-80% saturation range). |
| **Gain Map Generation** | P >= 16 flat-field frames averaged. Frequency decomposition: G_LF = Gaussian_filter(G, sigma=2-10). High-frequency noise removed. Outlier replacement at +/-3 sigma. |
| **Heel Effect Compensation** | Duo-SID projection (Wang 2013): G(x,y;d) = g0(x,y) * g_tilde(x,y;d). Iterative separation with convergence criterion epsilon=1.5, max 10 iterations. Arbitrary-SID reconstruction via ray-tracing projection. |
| **Drift Handling** | Periodic field gain update supported. Drift detection via flat-field residual monitoring (sigma/mean threshold). |
| **Advanced Tier** | Drift-triggered recalibration scheduling |
| **Inputs** | Offset-corrected, linearized frame + gain map (exposure-level matched) |
| **Release Gate** | Uniform phantom residual sigma/mean < 1% (80% FOV). DQE degradation < 5% per IEC 62220-1-1. |
| **Performance** | < 5ms/frame (Host PC), < 55ms allocated in pipeline |
| **Grid Handling** | If anti-scatter grid installed: grid removed during calibration OR grid pattern compensated in flat-field |
| **Research Basis** | IEC 62220-1-1:2015 (DQE), PMC3965338 (SNR optimization), Wang 2013 (Duo-SID), Kwan 2006 (multi-point correction) |

### 5.6 PRE-09: Binning Correction

| Aspect | Specification |
|--------|--------------|
| **Baseline** | Mode-specific binning compensation factors |
| **Advanced** | Fluoro/CBCT tuned kernels |
| **Inputs** | Binning mode + corrected frame |
| **Release Gate** | No-op outside binning mode. No geometry change introduced. |
| **Conditional** | Only active when binning mode is detected in acquisition metadata |

### 5.7 PRE-06: Defect Pixel Correction

| Aspect | Specification |
|--------|--------------|
| **Defect Types** | Dead pixel (cold), Hot pixel (excess dark current), Flickering/Unstable, Stuck, Row/Column defect, Cluster defect (2x2+), Partial line defect |
| **BPM Generation** | Union of: HotPixelMask (dark frame, lambda=8.0 robust statistics) + ColdPixelMask (flat-field, lambda=8.0) + FlickeringPixelMask (temporal CV analysis) + LineDefectMask + ClusterMask |
| **Detection Algorithm** | Robust Mask Maker (RMM) with FLkOS optimization: SNR(i) = abs(x(i) - mu_hat) / sigma_hat > lambda (lambda=8.0) |
| **Baseline Correction** | Edge-aware bilinear interpolation for isolated defects. Cluster-safe median fallback for grouped defects. Line interpolation for row/column defects. |
| **Advanced Correction** | MLP repair (FixPix architecture): 2-layer MLP, 5x5 patch (24 neighbor pixels), 1425 parameters. 14.2x NMSE improvement. FPGA-implementable without external compute. |
| **Research Path** | Concatenated CNN (MSE 91.80 for 5x5 defect, PMC7930811). ViT auto-encoder for high corruption rates (>5% defect density). Unrolled dual-domain method (arXiv:2601.20995) for CT applications. |
| **ML Constraint** | ML repair allowed only as secondary path. Baseline interpolation MUST be available as fallback. No artificial edge creation at defect sites. Cluster fallback prefers preservation over hallucination. |
| **Runtime Detection** | xpe_defect_detect_runtime() for transient defects at acquisition time. Does not replace static BPM but supplements it. |
| **Inputs** | BPM + gain-corrected float32 frame |
| **Release Gate** | No artificial edge creation at defect sites. Cluster fallback preserves original pixel when neighborhood is unusable. |
| **Performance** | < 95ms/frame (Host PC, 3072x3072) |
| **Research Basis** | PMC7930811 (CNN comparison), arXiv:2310.11637 (FixPix), PMC9721322 (RMM), arXiv:2601.20995 (dual-domain 2026) |

### 5.8 PRE-04/05: Lag / Ghost Correction

| Aspect | Specification |
|--------|--------------|
| **Lag Definition** | Residual signal present in frames subsequent to the frame in which it was generated. Caused by charge trapping in a-Si defect states. |
| **Ghosting Definition** | Change of detector pixel sensitivity due to previous exposures. Distinct from lag but corrected in same stage. |
| **Relative Magnitude** | For indirect-conversion FPD (CsI:Tl): Lag ~1-4% first frame, Ghosting ~0.1% at clinical doses. Lag dominates. |

#### Tier 1: Linear Time-Invariant (LTI) Baseline

| Parameter | Value |
|-----------|-------|
| Model | Multi-exponential IRF: h(k) = b0*delta(k) + sum(bn * exp(-an*k)), N=4 |
| Correction | Recursive deconvolution: x_k = y(k) - sum(bn * Sn_k * exp(-an)) |
| Calibration | Single falling step-response at mid-range exposure |
| Trigger | Default path. Residual artifact < threshold_1. |
| Performance | First-frame lag < 0.5%, 50th-frame lag < 0.01% |

#### Tier 2: Exposure-Weighted LTI

| Parameter | Value |
|-----------|-------|
| Model | LTI with intensity-weighting: coefficients selected from nearest calibrated exposure level |
| Escalation | Tier 1 insufficient: artifact >= threshold_1 |
| Performance | First-frame lag < 0.35% |

#### Tier 3: NLCSC (Non-Linear Correction with Signal-dependent Coefficients)

| Parameter | Value |
|-----------|-------|
| **IRF Model** | h(k, x_k) = b0(x_k)*delta(k) + sum(bn(x_k) * exp(-an(x_k)*k)), N=4 |
| **Exposure-Dependent Rates** | an(x) = a1_n + a2_n(x), where a2_n(x) = c1*(1 - exp(-c2*x)) |
| **Stored Charge** | qn_k = Sn_k * bn(x_k) * exp(-an(x_k)) / (1 - exp(-an(x_k))) |
| **Correction Algorithm** | NLCSC 3-step recursive per frame: (1) State variable update for consistency, (2) Signal correction with deconvolution, (3) State variable propagation |
| **Computational Simplification** | Only 2 longest time constants (n=1,2) treated as exposure-dependent. Shorter constants use fixed coefficients. |
| **Calibration** | 3-step: (1) Base lag rates from mid-range step-response, (2) Stored charge function from 9 exposures (2-92% saturation), (3) Exposure-dependent rates from global optimization on rising step-response |
| **Escalation** | Tier 2 insufficient: artifact >= threshold_2. Requires valid exposure history and NLCSC coefficients. |
| **Performance Target** | First-frame lag <= 0.29%, 50th-frame lag <= 0.0052% (per Starman et al. 2012) |
| **CBCT Performance** | Pelvic phantom: 11 HU avg / 19 HU max error. Head phantom: 3 HU avg / 5 HU max error. |

#### Ghost Correction Integration

| Parameter | Value |
|-----------|-------|
| Model | Dual-exponential: Ln = C0 + C1*exp(-n*tau*P1) + C2*exp(-n*tau*P2) |
| Correction | I_corrected = I_n - sum(Lm * I_{n-m}), limited to 10 prior frames |
| Typical Values | C0: 0.023-0.024, C1: 2.9-3.8, C2: 0.14-0.32, P1: 0.35-0.43, P2: 1.38-2.68 |
| Performance | >80% lag artifact correction in projection images |

#### Quality Targets

- Phase 1 clinical baseline: Tier 1/2 path meets Ghost SRS latency budgets. No visible ring/shading artifact in baseline phantom review set.
- Phase 2/3 advanced target: NLCSC with valid coefficients targets <= 0.3% first-frame, <= 0.01% 50th-frame on calibrated exposure set.
- Tier downgrade must be explicit in diagnostics when exposure history or coefficients are insufficient.

| Metric | Uncorrected | LTI (Tier 1/2) | NLCSC (Tier 3) | Target |
|--------|------------|-----------------|----------------|--------|
| 1st frame lag | 3.7% | 0.25% | <0.29% | <= 0.3% |
| 50th frame lag | 0.96% | 0.0038% | <0.0052% | <= 0.01% |
| CBCT pelvic (avg HU) | 35 | 14 | 11 | < 15 |
| CBCT head (avg HU) | 16 | 2 | 3 | < 5 |

#### State Requirements

- `exposureHistory`: Ring buffer of prior frames (8 frames, ~150 MB at 3072x3072 float32)
- NLCSC coefficients: Calibration-generated, detector-specific
- Time deltas between frames for frame-rate-dependent correction

| Research Source | Key Finding | Impact |
|----------------|-------------|--------|
| Starman et al. 2012 | NLCSC eliminates blurred ring artifact visible in LTI corrections | Validates Tier 3 necessity for CBCT |
| Pang et al. 2006 | Lag dominates ghosting by 10-40x for indirect-conversion FPD | Validates lag-first correction priority |
| Starman et al. 2012 | Forward bias hardware method: 88%/70% lag reduction (Tier 1/50th frame) | Hardware alternative for Tier 3 equivalent |

### 5.9 Calibration Data Management

| Aspect | Specification |
|--------|--------------|
| **File Format** | Calibration maps stored with metadata: version, creation timestamp, expiry timestamp, acquisition conditions (kVp, mAs, SID, temperature), detector serial number |
| **Expiry Validation** | xpe_calib_check_expiry() prevents use of expired calibration. Error code: XPE_ERR_CALIBRATION_EXPIRED. |
| **Versioning** | Each calibration file versioned with monotonic counter. Backup of previous version maintained. |
| **Storage** | Factory maps: read-only partition. Field updates: writable partition with backup. |
| **Startup Load** | CalibManager loads all required maps at startup. Budget: 200ms. Fail startup if mandatory maps (offset, gain, BPM) are missing. |

### 5.10 Calibration Drift Detection and Recalibration

| Trigger | Condition | Action |
|---------|-----------|--------|
| Temperature drift | Panel temperature delta > 3C from reference | Auto field dark update |
| Time-based | Elapsed time > 30 min since last calibration | Auto field dark update |
| QA failure | Flat-field residual sigma/mean > 1.5% | Emergency recalibration alert |
| Performance degradation | SNR outside 95% confidence interval | Recalibration recommended (per PMC3965338) |
| Shock/vibration | Accelerometer event detected | Emergency recalibration alert |
| Scheduled | Annual factory recalibration | Service procedure |

---

## 6. Enhancement and Post-Processing (Unchanged from v2.0.0-ds1)

### 6.1 Baseline Enhancement

| Module | Baseline Rule | Guardrail |
|--------|---------------|-----------|
| Log transform | Epsilon floor before log; never take log of zero/negative | Preserve monotonic intensity ordering |
| Noise reduction | Edge-preserving bilateral/NLM or wavelet-style | MTF loss at task-relevant frequencies within configured tolerance |
| Contrast enhancement | CLAHE or exam-profile curve with bounded clip limit | No local over-amplification in low-dose backgrounds |
| Edge enhancement | Unsharp or multiband sharpening with overshoot limiter | No clinically misleading halo/ringing artifacts |

### 6.2 Clinical Advanced Enhancement

| Module | Required Behavior | Release Gate |
|--------|-------------------|--------------|
| Baseline collimation | Gradient/Hough deterministic boundary detection on log-domain | ROI confidence required; low confidence reverts to whole-image |
| ROI-aware EI refinement | Collimation ROI on detector-domain image, not enhanced output | Reuse whole-image EI if ROI absent or low-confidence |
| GSVG / scatter correction | Separate grid suppression from gridless virtual-grid mode | Failure preserves original buffer + alert |
| Multiscale / fractional | Low-contrast anatomy enhancement without destabilizing noise | Auto-disable when artifact monitors exceed thresholds |

### 6.3 Exposure Index and Deviation Index

- EI derived from relevant image region in detector-domain data
- DI = 10 * log10(EI / EIT)
- EIT selected by exam/view database first, bodyPart classification may refine
- Preferred band: -1 <= DI <= +1; Acceptable: -3 <= DI <= +3; Outside +/-3: QC alert
- EI/DI suppressed for: stitched images, multi-irradiation images, invalid ROI selection
- **Phase 1b**: SWU-2.0 EI_Baseline in xpe_enhance_basic.dll (whole-image EI)
- **Phase 2**: SWU-2.10 ROI-aware EI refinement in xpe_enhance_advanced.dll

---

## 6A. Pre-Processing Stage Dependency and Bypass Contract

### 6A.1 Mandatory vs Conditional Stages

| Stage | Bypass Category | Bypass Condition | Downstream Impact if Bypassed |
|-------|:--------------:|------------------|-------------------------------|
| (0) CalibManager | **MANDATORY** | N/A | Fatal: no calibration data for any stage |
| (0.5) Readout Validation | ADVISORY | Always safe (non-mutating) | None: advisory flag only |
| (0.7) Temp Compensation | CONDITIONAL | Sensor unavailable OR detector within +/-2C of nominal | Low: minor dark drift at nominal temperature |
| (1) Offset Correction | **MANDATORY** | N/A | Critical: dark current bias propagates to all downstream |
| (1.5) Nonlinearity | CONDITIONAL | Panel profile declares linear response | Low-Medium: minor response curve error for near-linear detectors |
| (2) Gain Correction | **MANDATORY** | N/A | Critical: format conversion (uint16->float32) AND pixel normalization |
| (2.5) Binning | CONDITIONAL | Binning mode inactive (1x1 native) | None: no correction needed |
| (3) Defect Correction | CONDITIONAL | BPM empty (zero defect pixels) OR diagnostic mode | Medium: known defect pixels uncorrected |
| (4) Ghost Correction | CONDITIONAL | Single-shot, first frame, or no exposure history | Medium: lag artifacts present in subsequent frames |

### 6A.2 Safety Constraints (BYP-SAFE)

- **BYP-SAFE-001**: Offset (1) and Gain (2) are NEVER bypassable via configuration
- **BYP-SAFE-002**: Stage (2) is the sole uint16->float32 format boundary; bypass would crash downstream
- **BYP-SAFE-003**: Bypassed stages SHALL NOT set their corresponding `XPE_FLAG_*` bit
- **BYP-SAFE-004**: Ghost bypass auto-triggers on first frame after reset or power-on
- **BYP-SAFE-005**: Defect bypass with non-empty BPM SHALL emit warning alert
- **BYP-SAFE-006**: Nonlinearity bypass requires explicit `panel.linear = true` in detector profile
- **BYP-SAFE-007**: All bypass decisions logged to diagnostic JSON with stage name, reason, frame ID
- **BYP-SAFE-008**: Diagnostic/raw-export mode: only (0), (1), (2) mandatory; all others skip

For the complete dependency graph, bypass flowchart, and configuration interface, see `pipeline-spec.md` Sections 1A and 1B.

---

## 7. AI Modules (Unchanged from v2.0.0-ds1)

AI remains optional and assistive. Worker process never on critical path.

| Module | Requirement | Release Gate |
|--------|-------------|--------------|
| Body-part recognition | Sidecar classification only; no pixel mutation | Top-1 >= 95% on locked validation set |
| AI collimation refinement | Refines baseline ROI only | Must improve edge localization without false crop risk |
| Bone suppression | Derived image only, never overwrite primary | Reader-performance gain demonstrated |
| DL denoiser | Research/premium path only | Preserve anatomy, fail closed to classical denoiser |
| DL defect repair | Research path for cluster defects (>2x2) | FixPix MLP baseline must remain available |

---

## 8. Code-Quality and Verification Gates

### 8.1 General Engineering Gates (Unchanged)

| Gate | Requirement |
|------|-------------|
| ABI stability | Blittable structs, packing-checked across C/C# |
| Dependency hygiene | No lateral DLL dependency; all sharing via xpe_common.dll |
| Error reporting | Flags = state only; details via alert queue/diagnostic JSON |
| Determinism | Same binary + config + input = identical output hash |
| Hot-path allocation | No unbounded heap allocation in per-frame loops |
| Null/bounds safety | Validate pointers, format, dimensions, buffer sizes |
| Regression assets | Golden phantom and clinical smoke sets hash-locked |

### 8.2 Test Depth

| Area | Minimum Gate |
|------|--------------|
| Core preprocess (SWU-1.1 to SWU-1.9) | Statement >= 90%, Branch >= 80% |
| Phase 1b enhancement and display | Statement >= 85%, Branch >= 75% |
| GSVG and Phase 2 advanced modules | Golden-data regression + negative-path + clinical review set |
| AI worker | Contract tests, timeout tests, crash recovery, deterministic snapshot. MISRA N/A (ONNX Runtime dependency), substitute: clang-tidy + ONNX contract tests. Branch coverage >= 60% (justified: inference wrapper code only). |

### 8.3 Performance Gates

| Scope | Hard Limit | Notes |
|-------|-----------|-------|
| Detector-domain subset (0.5-4) | <= 500 ms/frame | Existing pipeline budget, research-validated |
| Phase 1 total | <= 3000 ms/frame | Includes display and DICOM path |
| Ghost Tier 1 | <= 150 ms | Within pre-processing 500ms budget |
| Ghost Tier 2 | <= 190 ms | Pipeline stage (4) budget: 150ms + 40ms escalation |
| Ghost Tier 3 | <= 240 ms | Pipeline stage (4) budget: 150ms + 90ms escalation |
| Alert handling | Non-blocking | Alerts may not stall image delivery |
| Worker timeout | Configured fail-closed | AI timeout skips AI result, not freeze pipeline |

### 8.4 Image-Quality Verification Pack

The locked verification pack shall include:

| Dataset | Purpose | Calibration Algorithms Tested |
|---------|---------|------------------------------|
| Flat-field and dark-field datasets | Offset/gain drift checks | PRE-02, PRE-03, PRE-07 |
| Step-wedge phantom | Lag/ghost residual, contrast stability, nonlinearity | PRE-04/05, PRE-08 |
| Defect-mask synthetic set | Isolated and clustered bad pixels | PRE-06 |
| Temperature sweep set (15-40C) | Compensation validation | PRE-02, PRE-07 |
| Multi-exposure linearity set | Nonlinearity and multi-gain response | PRE-08, PRE-03 |
| Heel effect SID variation set | Duo-SID gain map reconstruction | PRE-03 (Heel) |
| Body-region scatter/grid set | GSVG and virtual-grid evaluation | POST (GSVG) |
| Chest radiograph set | Collimation, EI/DI, body-part, bone suppression | POST, AI |
| Long-length overlap set | Stitching | POST (AI) |

---

## 9. Research-Backed Improvement Roadmap

### 9.1 Gap Analysis: Current Spec vs Research State-of-Art

| Area | Current Spec Level | Research State-of-Art | Gap | Priority |
|------|-------------------|----------------------|-----|----------|
| Offset correction | Dynamic dark with temperature interpolation | Frequency decomposition (LF/HF separation), PREP-time exponential model | **Partially covered** in PRD, needs formal integration into pipeline spec | High |
| Gain correction | Single-point flat-field normalization | Multi-gain polynomial (5-10 levels), Duo-SID heel effect, frequency decomposition noise reduction | **Major gap**: Multi-gain not in pipeline spec, Duo-SID described in PRD only | Critical |
| Nonlinearity correction | LUT or polynomial linearization | Piecewise linear with NUC, second-order polynomial per pixel | **Adequate**: Current spec sufficient for Phase 1 | Medium |
| Defect correction | Bilinear/median interpolation | FixPix MLP (14.2x NMSE), Concatenated CNN, ViT auto-encoder, dual-domain unrolled | **Significant gap**: ML approaches documented in PRD but no formal integration path | High |
| Lag correction | 3-tier LTI/NLCSC | NLCSC fully characterized (Starman 2012), forward bias hardware alternative | **Well covered**: Tier 3 NLCSC targets match published results | Low |
| Ghost correction | Exposure-history-based | Dual-exponential model (Pang 2006), lag dominates ghosting distinction | **Adequate**: Model matches literature | Low |
| Temperature compensation | LUT/polynomial + exponential model | Adaptive interpolation, power-mode-specific maps for portable | **Partially covered**: Portable detector handling in PRD, needs pipeline integration | Medium |
| Calibration data management | Load/save/expiry check | Drift-aware scheduling, SNR-based recalibration triggers, version management | **Gap**: Drift detection strategy documented here but not yet in API/pipeline | High |
| Calibration QC metrics | DQE/MTF/NPS per IEC 62220 | Real-time flat-field residual monitoring, SNR confidence intervals | **Gap**: No runtime QC metric computation specified | Medium |

### 9.2 Recommended Implementation Phases

#### Phase 1a: Foundation Calibration (Priority: Critical)

- PRE-02 Offset correction with static dark map
- PRE-03 Gain correction with single-point flat-field
- PRE-06 Defect correction with bilinear interpolation baseline
- PRE-04 Lag correction Tier 1 (LTI)
- Calibration data load/save/expiry

#### Phase 1a+: Enhanced Calibration (Priority: High)

- PRE-07 Temperature compensation (LUT/polynomial)
- PRE-08 Nonlinearity correction (LUT)
- PRE-01 Readout validation
- PRE-09 Binning correction
- PRE-04 Lag correction Tier 2 (exposure-weighted)
- Dynamic dark map interpolation (temperature + PREP time)

#### Phase 1b+: Advanced Calibration (Priority: High)

- Multi-gain polynomial correction (5+ exposure levels)
- Duo-SID heel effect compensation
- Frequency decomposition for dark and gain maps
- Calibration drift detection and recalibration scheduling
- EI-0 baseline computation

#### Phase 2: Premium Calibration (Priority: Medium)

- PRE-04/05 Lag correction Tier 3 (NLCSC)
- FixPix MLP defect correction (advanced tier)
- Real-time calibration QC metric computation
- SNR-based recalibration trigger
- Portable detector power-mode compensation

#### Phase 3+: Research Calibration (Priority: Low)

- CNN/ViT-based cluster defect repair
- Scene-based NUC (no-reference recalibration)
- Dual-domain unrolled defect correction (2026 method)
- Adaptive gain map aging compensation

---

## 10. Sync Actions Required Outside This Document

1. **pipeline-spec.md v1.2.0**: Add multi-gain model as internal to stage (2). Clarify Tier 2/3 ghost time allocation within 500ms pre-processing budget. Add EI-0 stage assignment.
