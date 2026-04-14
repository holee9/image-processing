# XPE Algorithm Specification (DeepSync Upgrade v2) (XPE 알고리즘 사양 (DeepSync 업그레이드 v2))

**Document ID**: ALG-SPEC-001  
**Version**: 3.0.0-ds2  
**Date**: 2026-04-14  
**Status**: Working Draft  
**Project**: ImageProcTest - X-Ray Image Processing Engine (XPE)  
**Upgrade**: 심층 연구 + 교차검증 + DeepSync 통합

---

## Changelog (v2.0.0-ds1 -> v3.0.0-ds2) (변경 로그)

| Change | Detail (상세) |
|--------|--------|
| Calibration deep research | Calibration 알고리즘 검증을 위해 15개 이상의 동료 검증 논문, IEC 표준, 특허 참고 통합 |
| Cross-verification v2.0 | 3회차 교차검증 보고서의 20개 이슈 모두 해결 |
| Section 5 rewritten | Detector 도메인 보정이 연구 검증된 수학적 모델, 성능 목표, 품질 게이트로 업그레이드됨 |
| Section 5.3 NEW | Calibration 데이터 관리 lifecycle 사양 추가 |
| Section 5.4 NEW | Calibration drift 감지 및 recalibration 전략 |
| Section 5.5 NEW | Multi-gain 및 nonlinearity 보정 통합 모델 |
| Section 9 NEW | Gap 분석을 포함한 연구 기반 개선 로드맵 |
| Section 6A NEW | 8개 안전 제약을 포함한 전처리 단계 종속성 매트릭스 및 우회(on/off) 계약 |
| EI-0 resolution | Resolution B 채택: SWU-2.0 EI_Baseline을 Phase 1b (xpe_enhance_basic.dll)에 추가 |
| Pipeline order validated | 연구 검증된 최적 순서 확인: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost |

---

## 1. Purpose (목적)

이 사양은 XPE 전처리 calibration 파이프라인의 규범적 알고리즘 계약입니다. 다음을 통합합니다:

- Local 설계 문서 (PRD-FPD-CAL-001, Ghost PRD v2, Panel Defect Plan)
- 교차검증 결과 (XPE-XVER-001 v1.0.0, SPEC-XPE-MASTER 교차검증 v2.0.0)
- 동료 검증 문헌 및 IEC/AAPM 표준에 대한 심층 연구
- DeepSync 충돌 해결 결정

이 문서는 모든 알고리즘 행동, 품질 게이트, 단계 소유권에 대해 ALG-SPEC-001 v2.0.0-ds1을 대체합니다.

---

## 2. DeepSync Decisions (Carried Forward + New) (DeepSync 결정 (인계 + 신규))

### 2.1 Carried Forward (from v2.0.0-ds1) (인계 (v2.0.0-ds1에서))

| Topic | Decision (결정) |
|-------|----------|
| Phase ownership | Phase 2는 결정론적 classical 알고리즘으로 제한됨. Phase 3은 xpe_ai.dll 및 xpe_ai_worker.exe를 소유합니다. |
| Collimation output | ROI은 orchestration sidecar에 저장됨, XpeImageMetadata에는 아님. |
| Metadata flags | XPE_FLAG_*는 state 전용입니다. 실패 상세는 alert queue 또는 진단 JSON을 통해 전달됩니다. |
| Exposure Index | EI/DI는 detector 도메인, presentation 전 데이터에서만 계산됨. |
| EI applicability | IEC 62494-1은 단일 irradiation 이벤트 이미지에만 적용됨. |
| Detector QC metrics | MTF, NPS, DQE는 detector 도메인 이미지에서만 측정됨. |
| AI safety | AI 모듈은 보조적이고 성능 저하 가능합니다. 파이프라인은 AI worker에서 절대 차단되지 않습니다. |

### 2.2 New Decisions (v3.0.0-ds2) (신규 결정)

| Topic | Decision (결정) | Research Basis (연구 기반) |
|-------|----------|---------------|
| Pipeline order | Nonlinearity 보정이 gain (stage 1.5) 전, 후가 아님. Flat-field normalization 전 픽셀 응답을 선형화합니다. | Physical correctness: gain normalization은 선형 검출기 응답을 가정합니다. PRD Section 4.1 순서(gain 후)는 pipeline-spec Section 1.2로 대체됩니다. |
| Multi-gain model | Multi-gain 다항식 보정이 gain 보정 stage (2)에 통합됨, 별도 단계가 아님. 노출 수준별 이득 맵 선택은 xpe_gain_correct() 내부입니다. | Varex multi-gain calibration (6-10 신호 수준), PRD-FPD-CAL-001 Section 5.2.4. |
| Defect correction strategy | Baseline: edge-aware bilinear 보간. Advanced: MLP (FixPix 아키텍처, 1425 파라미터, FPGA 친화적). 연구 경로만: 클러스터 결함용 CNN/ViT. | FixPix (2023): 선형 보간 대비 14.2배 NMSE 개선. Concatenated CNN (PMC7930811): traditional TMC 243.6 대비 MSE 91.80. Simple ANN은 18배 적은 파라미터(1425 vs 26891)로 거의 최적 달성. |
| Lag model selection | N=4 multi-exponential IRF를 갖춘 NLCSC를 advanced 계층으로. LTI deconvolution을 baseline으로. 계산 효율을 위해 2개 가장 긴 시간 상수만 노출 의존적으로 처리. | Starman et al. 2012 (PMC3465354): NLCSC는 <0.29% first-frame, <0.0052% 50th-frame lag 달성. 미보정 대비 88% 감소. |
| Lag vs ghosting distinction | Lag = 후속 프레임의 residual 신호 (charge trap 방출). Ghosting = 이전 노출로 인한 검출기 민감도 변화. 둘 다 stage (4)에서 보정되지만 다른 메커니즘을 통해. | PMC5722609: Indirect-conversion FPD의 경우, 임상 용량에서 lag (~1-4%)이 ghosting (~0.1%)을 지배합니다. |
| Heel effect compensation | 2개 reference calibration에서 arbitrary-SID gain map 재구성을 위해 Duo-SID projection 방법(Wang 2013) 채택. | Wang 2013: single-SID 대비 ~80% RMSE 감소, 보간 대비 ~70%. |
| Temperature compensation model | Exponential dark current 모델: I_dark(T) = I_0 * exp(-E_g / 2*k_B*T). PREP time 및 온도를 이용한 dynamic dark map 보간. | EP2148500A1 특허. 23개월 안정성 연구로 dynamic 보정을 이용한 0.5% (1 SD) 표시. |
| Calibration drift strategy | Temperature delta, 경과 시간, flat-field residual 모니터링에 기반한 자동 스케줄링을 갖춘 drift-aware recalibration. | Kwan et al. 2006, Wenz et al. 2023: 일회성 factory 상수는 장기 임상 사용에 불충분. |
| EI-0 Phase assignment | NEW SWU-2.0 EI_Baseline in Phase 1b (xpe_enhance_basic.dll). Phase 2는 SWU-2.10을 통해 ROI-aware refinement 추가. | Cross-verification v2.0에서 CRITICAL 이슈 N11 해결. |

---

## 3. Source Base (출처 기반)

### 3.1 Local Sources (Local 출처)

- `.moai/plans/memoized-conjuring-aurora.md`
- `.moai/project/pipeline-spec.md` (v1.1.0, 파이프라인 순서의 규범)
- `.moai/project/api-spec.md` (v1.1.0, 목표 v1.2.0)
- `docs/xray_fpd_tech_classification_final.md`
- `docs/ghost-correction/srs_ghost_correction.md`
- `docs/ghost-correction/sad_ghost_correction.md`
- `docs/ghost-correction/sw_lag_correction_prd_v2.md`
- `docs/post-processing/xpe/XPE-SAD-001_Software_Architecture_Document.md`
- `docs/xray-fpd-research/xray-detector-calibration-prd.md` (PRD-FPD-CAL-001 v1.0.0)
- `docs/panel-defect-algorithm/plan.md`
- `.moai/specs/SPEC-XPE-MASTER/cross-verification-report.md` (v2.0.0)
- `docs/cross-verification-report-2026-04-13.md` (XPE-XVER-001 v1.0.0)

### 3.2 Public Technical Sources (Research-Validated) (공개 기술 출처 (연구 검증))

| Domain | Source | Use in this spec (이 사양에서의 사용) | Validation Status (검증 상태) |
|--------|--------|-----------------|-------------------|
| Lag correction (NLCSC) | Starman et al., Med Phys 2012, [PMC3465354](https://pmc.ncbi.nlm.nih.gov/articles/PMC3465354/) | Tiered lag 설계: LTI baseline + NLCSC advanced. N=4 multi-exponential IRF with signal-dependent 계수. | **VERIFIED** - 전체 알고리즘 추출, 성능 지표 확인 |
| Lag vs ghosting model | Pang et al., Med Phys 2006, [PMC5722609](https://pmc.ncbi.nlm.nih.gov/articles/PMC5722609/) | Dual-exponential lag 모델. Lag (1-4%)이 indirect-conversion FPD의 ghosting (0.1%)을 지배합니다. | **VERIFIED** - 파라미터 및 임상 의의 확인 |
| Gain/offset calibration SNR | Ranger et al., J Digit Imaging 2014, [PMC3965338](https://pmc.ncbi.nlm.nih.gov/articles/PMC3965338/) | Gain/offset calibration은 SNR 변동을 감소시킵니다. SNR이 95% CI 외에 떨어질 때 recalibration 지시됨. | **VERIFIED** - 정량적 SNR 개선 데이터 확인 |
| Deep learning defect correction | Jeon et al., Phys Med 2021, [PMC7930811](https://pmc.ncbi.nlm.nih.gov/articles/PMC7930811/) | Concatenated CNN 최고 MSE (5x5 결함의 경우 traditional TMC 243.6 대비 91.80). Simple ANN이 거의 동일(94.67) while 18배 적은 파라미터(1425 vs 26891). | **VERIFIED** - 아키텍처 및 성능 지표 확인 |
| FixPix bad pixel correction | Schirrmacher et al., 2023, [arXiv:2310.11637](https://arxiv.org/html/2310.11637v2) | MLP 기반 보정 14.2배 NMSE 개선. 감지를 위한 confidence-calibrated 분할. 높은 손상률의 경우 ViT auto-encoder. | **VERIFIED** - 네트워크 아키텍처 및 FPGA 실현성 확인 |
| Unrolled dual-domain correction | 2026, [arXiv:2601.20995](https://arxiv.org/html/2601.20995) | 낮은 성능 픽셀 보정을 위한 합성 데이터 학습. 1-2% 검출기 결함의 경우 state-of-art 우월. | **NEW** - 최신 연구, 평가 대기 중 |
| Flat-field / recalibration drift | Kwan et al., 2006, [PubMed 16532945](https://pubmed.ncbi.nlm.nih.gov/16532945/) | 다중 포인트 calibration의 개선된 flat-field 보정. | **VERIFIED** |
| Calibration drift management | Wenz et al., 2023, [PubMed 36897395](https://pubmed.ncbi.nlm.nih.gov/36897395/) | 임상 사용을 위한 drift 처리 및 recalibration 스케줄링. | **VERIFIED** |
| Heel effect (Duo-SID) | Wang, Med Phys 2013, [PDF](https://www.math.union.edu/~wangj/papers/Wang13.Heel%20Effect%20%5BMed%20Phys%5D.pdf) | 2개 참고에서 arbitrary-SID gain map의 Duo-SID projection. 80% RMSE 감소. | **VERIFIED** |
| Dynamic dark correction | EP2148500A1, [Patent](https://patents.google.com/patent/EP2148500A1/en) | PREP time 및 온도 보정을 위한 offset 조정 맵. Portable 검출기 power-mode 처리. | **VERIFIED** |
| NUC algorithm | Multiple 출처 via [Science.gov](https://www.science.gov/topicpages/n/non-uniformity+correction+algorithm) | Two-point calibration (TPC)을 baseline으로. Nonlinearity를 위한 piecewise linear 또는 다항식. | **VERIFIED** |
| Display calibration | DICOM PS3.14 GSDF | Presentation 경로는 GSDF-consistent 그레이스케일을 보존합니다. Detector 도메인 처리와 분리. | **VERIFIED** |
| Exposure Index | IEC 62494-1, AAPM TG-232 | EI/DI는 detector 도메인 데이터와 연결됩니다. DI = 10 * log10(EI / EIT). | **VERIFIED** |
| DQE standard | IEC 62220-1-1:2015, [UMich PDF](https://websites.umich.edu/~ners580/ners-bioe_481/lectures/pdfs/2003-10-IEC_62220-DQE.pdf) | DQE(f) = MTF^2(f) / (NPS(f) * q). Calibration 품질은 DQE 측정 정확도에 직접 영향을 줍니다. | **VERIFIED** |
| Scatter / virtual grid | Lisson et al., 2020, Virtual Grid 연구 2022 | 임상 이미지 품질 검증이 필요한 phase-gated premium 기능. | **VERIFIED** |

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
2. **api-spec.md v1.2.0**: Add AED functions (5.16-5.18). Update function counts (82 total). Document multi-gain calibration API parameters.
3. **XPE-SDD-001 v1.1**: Add SWU-1.6 to SWU-1.9, SWU-5.8, SWU-6.1. Add SWU-2.0 (EI_Baseline).
4. **XPE-SRS-001 v1.1**: Add SRS-AED-* requirements. Add calibration drift detection requirements.
5. **XPE-RTM-001 v1.1**: Track all new SWUs and requirements.
6. **PRD-FPD-CAL-001**: Add cross-reference note: "For normative algorithm behavior and pipeline ordering, refer to ALG-SPEC-001 v3.0.0-ds2."
7. **product.md v1.1**: Update SWU count from 38 to 43 (or current correct total).

---

## 11. Acceptance Summary

The upgraded algorithm spec is accepted only when ALL of the following are true:

1. Detector-domain algorithms satisfy performance and regression gates without relying on AI.
2. Pipeline order follows research-validated sequence: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost.
3. EI/DI computed in detector domain, suppressed for unsupported image classes.
4. Phase 2 and Phase 3 ownership is unambiguous in pipeline, API, and plan documents.
5. Failure details use alerts/diagnostics, not overloaded metadata flags.
6. AI features remain optional, provenance-aware, and safely degradable.
7. Calibration drift detection strategy is documented and testable.
8. All 20 cross-verification issues (v2.0) are resolved or tracked with action plans.
9. Multi-gain and nonlinearity correction ordering is validated against physical principles.
10. Lag/ghost correction targets match peer-reviewed performance benchmarks.

---

*End of Algorithm Specification v3.0.0-ds2*
