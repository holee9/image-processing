# Software Requirements Specification - XPE Preprocessing Calibration Module

**Document ID:** SRS-CALIB-001 v1.2  
**IEC 62304 Clause:** 5.2 (Software Requirements Specification)  
**Safety Classification:** Class B  
**Date:** 2026-04-24  
**Author:** XPE Calibration Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

### 1.1 Purpose

This Software Requirements Specification (SRS) defines all functional, safety, performance, and interface requirements for the XPE Preprocessing Calibration Module (`xpe_preprocess.dll`, Layer 1, Phase 1a). The module transforms raw detector data (uint16, 14-16 bit ADC output) into calibrated, corrected images (float32) by applying systematic corrections for physical detector artifacts including dark current offset, pixel gain variation, defective pixels, lag/ghosting, temperature drift, nonlinearity, and binning effects.

### 1.2 Scope

This document specifies requirements for calibration data management, correction algorithms, and quality assurance mechanisms required to meet IEC 62304 Class B medical device safety standards. The module is required for all clinical imaging workflows and mandatory for any system using the xpe_preprocess.dll interface.

**Out of scope:** Image enhancement processing (CLAHE, log transform, edge enhancement) is handled downstream in `xpe_enhance_basic.dll`. Grid suppression and virtual grid correction are handled in `gsvg.dll`.

---

## 2. Functional Requirements

### 2.1 Calibration Data Load Management (SRS-CALIB-FUNC-001 through SRS-CALIB-FUNC-003)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-001** | System shall load offset calibration file (.xpe_calib format) containing dark current map (uint16). File format: 34-byte header (magic=0x585045, version, timestamp, expiryEpochMs, reserved) + CRC-32 (4 bytes) + pixel data (3072×3072×2 bytes). CRC-32 validation via polynomial 0x04C11DB7 shall reject corrupted files with error `XPE_ERR_IO_FAILED`. | Offset map is essential baseline for all downstream corrections; CRC-32 protects against silent data corruption. Factory-calibrated maps have expiry epochs for compliance tracking. | Test: CRC validation, corruption detection |
| **SRS-CALIB-FUNC-002** | System shall load gain calibration file (.xpe_calib format) containing normalization factors (float32). File format: 34-byte header + CRC-32 + gain coefficients (3072×3072×4 bytes). Values shall be in range [0.1, 10.0]; out-of-range values shall trigger `XPE_ERR_INVALID_CALIB_DATA` error. | Gain map normalizes pixel-to-pixel sensitivity variation (FPN). Float32 enables multi-gain polynomial support. Range limits prevent over/under-correction artifacts. | Test: Range validation, file parsing |
| **SRS-CALIB-FUNC-003** | System shall load bad pixel map (BPM) from .xpe_calib file (uint8, 1 byte per pixel). BPM format: pixel value 0=good, 1-255=defect type (1=dead, 2=hot, 3=stuck, 4=noisy). Sparse map optimization supported via run-length encoding (RLE). Maximum 5% defect density tolerance. | BPM enables targeted defect correction without full-image filtering. RLE compression reduces memory footprint (typical 9.4MB to <500KB). Defect type field supports algorithmic selection. | Test: BPM parsing, RLE decompression |

### 2.2 Pixel-Level Corrections (SRS-CALIB-FUNC-004 through SRS-CALIB-FUNC-009)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-004** | System shall apply offset (dark current) correction: `I_corr(x,y) = I_raw(x,y) - I_dark(x,y)`. Negative results after offset subtraction shall be clamped to 0 (uint16 domain). **Temperature compensation shall be active when NTC sensor data is available**: when loaded offset map has associated temperature metadata (T_ref), system shall apply exponential model `I_dark(T) = I_dark(T_ref) × exp(-(E_g/2kB) × (1/T - 1/T_ref))` where E_g=1.12eV (Si), kB=Boltzmann constant. Compensation shall be bypassed only when sensor is unavailable or |T - T_ref| ≤ 2°C. PREP-time exponential decay model `offset_adj = offset × exp(-t_prep / τ)` shall be applied when `t_prep` metadata is provided (τ=0.1s default). | Dark current is primary readout artifact; doubles every 6-8°C in a-Si FPD. Without active temperature compensation, clinical images acquired at detector temperatures different from calibration temperature contain systematic bias. PREP-time decay models dark current accumulation after detector reset. Both corrections are essential for real-world clinical environments where detector temperature varies continuously. | Test: Offset arithmetic, clamp verification, temperature compensation accuracy, PREP-time decay |
| **SRS-CALIB-FUNC-005** | System shall apply gain (flat-field) correction: `I_norm(x,y) = I_corr(x,y) / G(x,y)`. Output shall be converted to float32 format. Gain values shall be validated to be non-zero; division by zero shall return `XPE_ERR_INVALID_CALIB_DATA`. Multi-gain mode with energy-dependent polynomial `G(x,y,E) = Σ(c_k × E^k)` shall be supported for SID-specific gain maps. Heel effect correction (Wang 2013 projection model) shall be supported. | Gain correction normalizes across detector's FPN and SID-dependent intensity falloff. Float32 conversion at this stage is mandatory (format boundary). Multi-gain supports clinical applications using multiple source positions. | Test: Gain arithmetic, format conversion |
| **SRS-CALIB-FUNC-006** | System shall apply nonlinearity correction using lookup table (LUT) or monotonic polynomial fitting before gain correction. Correction model: `I_lin(x,y) = f_nonlin(I_raw(x,y))` where `f_nonlin` is detector-specific and stored in calibration profile. LUT shall have minimum 256 entries; polynomial degree ≤ 5. Detailed algorithm specification in SRS-CALIB-FUNC-006-EXT below. | Detector response is non-linear due to charge trapping and fill factor effects. Correction must precede gain normalization (linearize before normalize). Detector profile governs enable/disable via field `panel.linear = true/false`. | Test: LUT lookup, polynomial evaluation, max residual ≤ 0.3% ADU |

**SRS-CALIB-FUNC-006-EXT: Nonlinearity Correction Algorithm Extension**

**6a. LUT Method (preferred for production)**

The nonlinearity LUT maps raw ADU values to linearized ADU values:

- LUT size: 4096 entries (covers 12-bit ADC range) or 65536 entries (16-bit full range)
- LUT data type: uint16 (output values in ADU)
- Lookup: `I_lin = LUT[I_raw]` (direct index, O(1))
- LUT generation (factory calibration procedure):
  1. Acquire flat-field images at N ≥ 10 dose levels spanning 5% to 95% ADC full scale
  2. For each dose level, record mean signal `S_meas` and reference dose `D_ref`
  3. Fit ideal linear response: `S_ideal(D) = G_nominal × D` where `G_nominal` is mean gain
  4. Compute correction: `LUT[S_meas] = S_ideal`
  5. Interpolate LUT entries between measured points using monotone cubic spline (Fritsch-Carlson 1980)
  6. Boundary conditions: `LUT[0] = 0`, `LUT[ADC_max] = ADC_max` (identity at extremes)
- Maximum interpolation error requirement: ≤ 0.3% of ADC full scale at any input value
- Monotonicity check: `LUT[i] ≤ LUT[i+1]` for all i (enforced; non-monotone LUT = `XPE_ERR_INVALID_CALIB_DATA`)

**6b. Polynomial Method (for embedded/FPGA use)**

The polynomial model uses a 4th-degree global polynomial fit:

```
I_lin(x,y) = c₀ + c₁ × I_raw + c₂ × I_raw² + c₃ × I_raw³ + c₄ × I_raw⁴
```

Evaluated using Horner's method to minimize arithmetic operations:

```
I_lin = c₀ + I_raw × (c₁ + I_raw × (c₂ + I_raw × (c₃ + I_raw × c₄)))
```

Polynomial fitting procedure:
1. Use the same N ≥ 10 dose-level flat-field dataset as LUT method
2. Fit via least-squares regression (numpy.polyfit or scipy.optimize.curve_fit)
3. Validate: maximum residual ≤ 0.5% ADC full scale across all measurement points
4. Enforce monotonicity in [0, ADC_max] by checking derivative root locations
5. If polynomial is non-monotone in operational range, reject and fallback to LUT method

**6c. Precision Comparison (LUT vs Polynomial)**

| Method | Interpolation error | Memory | Execution time | Platform |
|--------|-------------------|--------|----------------|----------|
| LUT (4096) | ≤ 0.30% | 8 KB | ~5 ns (cache hit) | CPU/FPGA |
| LUT (65536) | ≤ 0.01% | 128 KB | ~5 ns (L1 miss risk) | CPU only |
| Polynomial (deg 4) | ≤ 0.50% | <100 bytes | ~30 ns (5 MACs) | CPU/MCU/FPGA |
| Polynomial (deg 2) | ≤ 1.00% | <50 bytes | ~15 ns | MCU/FPGA |

Selection logic: If `panel.nonlinearity_mode == "LUT"` use 6a; if `"POLY"` use 6b; if `"AUTO"` select LUT for CPU targets, polynomial for MCU/FPGA targets (detected via `panel.target_platform` field in calibration profile).
| **SRS-CALIB-FUNC-007** | System shall apply defect pixel correction in three modes selectable via configuration: (a) neighbor averaging (4-neighbor or 8-neighbor), (b) bilinear interpolation from surrounding pixels, (c) median filter from neighborhood. Output shall replace defective pixels with interpolated values. Defect detection shall use Robust Mask Maker (RMM) with lambda=8.0 for runtime detection. | Defects manifest as dead pixels (no signal), hot pixels (excessive signal), and noisy pixels. Three interpolation modes provide trade-offs between speed and quality. RMM is robust to non-Gaussian noise. | Test: Interpolation correctness |
| **SRS-CALIB-FUNC-008** | System shall apply temperature compensation to dark current using exponential model: `I_dark_compensated(T) = I_dark(T_ref) × exp(-(E_g/2kB) × (1/T - 1/T_ref))`. Temperature input shall be from NTC thermistor sensor (0-50°C range). Compensation shall be bypassed if sensor unavailable or if |T - T_ref| ≤ 2°C (within tolerance). Reference temperature T_ref = 25°C (nominal). | Dark current doubles approximately every 6-8°C (intrinsic semiconductor physics). Temperature compensation prevents image drift between dark and clinical exposures. 2°C tolerance prevents false triggers. | Test: Exponential model accuracy, sensor integration |
| **SRS-CALIB-FUNC-009** | System shall check calibration expiry by comparing file timestamp (expiryEpochMs from header) against current time. If `current_time_ms > expiryEpochMs`, function shall return error `XPE_ERR_CALIBRATION_EXPIRED` and halt pipeline. Expiry date field shall be loaded from calibration file header (offset 8-11, uint32, milliseconds since 2000-01-01). **Default expiry period**: Factory calibration files shall set `expiryEpochMs = created_epoch_ms + 7776000000` (90 days). Zero-value expiry (`expiryEpochMs = 0`) shall be interpreted as "no expiry" and shall log a warning `XPE_WARN_NO_EXPIRY` indicating potential staleness. Configuration override via `calibration.expiry_days` in JSON config shall be supported (minimum: 7 days, maximum: 365 days). | Expired calibration data introduces systematic bias in corrected images. Hard block prevents clinical use of stale calibration. Expiry mechanism enables IEC 62304 traceability and regulatory compliance (21 CFR Part 11). Default 90-day expiry balances detector drift risk against operational burden. Never-expire flag (0) must trigger warning to prevent silent staleness accumulation. | Test: Date comparison logic, default expiry assignment, warning on zero expiry |
| **SRS-CALIB-FUNC-010** | System shall support runtime defect detection via `xpe_defect_detect_runtime()` function to identify new defects not present in static BPM. Detection algorithm: calculate pixel SNR over N=10 consecutive frames; flag pixels with SNR < 5 dB as defects. Runtime map shall be merged with static BPM and logged for QA review. | Clinical systems develop new defects over time (cosmic rays, electrostatic discharge). Runtime detection prevents image artifacts from escaping undetected. | Test: SNR calculation, defect logging |

### 2.3 Session and Binning Management (SRS-CALIB-FUNC-011 through SRS-CALIB-FUNC-012)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-011** | System shall support calibration session management via `xpe_calib_session_create()` function. Session shall be identified by unique session_id (UUID v4). Each session shall include timestamp, detector temperature, source kVp/mAs, binning mode, and associated calibration file versions. Session state shall persist across frame processing until explicit reset via `xpe_ghost_reset()`. | Sessions enable tracking of calibration state across multi-frame acquisitions. Session metadata enables post-processing traceability (PACS integration). Ghost correction requires frame history (see SRS-CALIB-FUNC-012). | Test: Session ID generation, state persistence |
| **SRS-CALIB-FUNC-012** | System shall support pixel binning mode compensation via `xpe_binning_correct()`. Binning modes shall include 1×1 (native), 2×2, 4×4. Gain correction factor shall be applied as `G_binned(x,y) = G_native(x,y) / (binning_factor^2)` to maintain normalized intensity. Binning correction shall only execute if binningMode ≠ 1 (native resolution). Output bit-depth shall remain float32. | Binning (on-detector pixel summing) reduces noise but requires gain re-normalization. Squared factor accounts for charge summation. Conditional execution prevents unnecessary computation. | Test: Binning factor validation |

### 2.4 Ghost/Lag Correction and Frame History (SRS-CALIB-FUNC-013 through SRS-CALIB-FUNC-014)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-013** | System shall support three-tier ghost/lag correction with auto-escalation: (1) Tier 1: Multi-exponential LTI deconvolution (N=4 exponentials, model: `Lag(t) = Σ(α_i × exp(-t/τ_i))`); (2) Tier 2: Exposure-weighted LTI (accounts for variable exposure); (3) Tier 3: NLCSC (Non-Linear Correlation Signal Correction, Starman 2012, accounts for signal-dependent lag). Escalation shall occur when residual lag after Tier 1 exceeds threshold (10% of signal). Minimum 90% ghost removal shall be achieved. | Lag/ghosting is temporal artifact from charge carrier trapping. Three tiers provide accuracy vs. performance trade-off. Auto-escalation ensures best achievable image quality. NLCSC handles complex detector physics. | Test: Lag residual measurement |
| **SRS-CALIB-FUNC-014** | System shall maintain exposure history ring buffer with minimum 8 frames (configurable up to 16). Frame buffer storage: 8 frames × 3072×3072×4 bytes = ~150 MB. History shall be reset via `xpe_ghost_reset()` after patient/study change or power-on. First frame after reset shall skip ghost correction (no history). Single-shot mode shall also skip ghost correction. | Frame history enables temporal filtering for lag artifact removal. 8-frame buffer is standard for commercial FPD systems (Varex, Vieworks). Ring buffer prevents memory bloat. Reset prevents cross-contamination between acquisitions. | Test: Buffer management, reset logic |

---

### 2.5 Best-in-Class Preprocessing Reinforcement and E2E Verifiability Addendum (2026-04-16)

The following requirements bind preprocessing algorithm quality to automated evidence defined in `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-015** | System shall emit a preprocessing E2E metric report for every automated raw/calibration validation run using schema `xpe-pre-e2e-report-v1`. The report shall include raw SHA-256 before/after, calibration file SHA-256, inferred dimensions, applied stages, stage timings, detector-domain metrics, and pass/fail gates. | Preprocessing quality must be reproducible and reviewable without relying on screenshots or subjective visual inspection. | Test: PRE-E2E-0, report schema validation |
| **SRS-CALIB-FUNC-016** | System shall compute detector-domain dark/offset metrics: `DarkBias`, `DSNU_ADU`, `DarkReduction_dB`, and `ClampRate`. Phase 1 acceptance shall require `abs(DarkBias) <= 5 ADU` or `DarkReduction_dB >= 10 dB` on applicable fixtures. | Offset correction is the first safety-critical calibration stage; dark residuals are objective indicators of drift or calibration mismatch. | Test: PRE-E2E-1 synthetic oracle, PRE-E2E-2 real fixture |
| **SRS-CALIB-FUNC-017** | System shall compute gain/flat-field metrics: `PRNU_CV`, `FlatResidualPct`, `FPN_Reduction_dB`, and `LineArtifactScore`. Phase 1 acceptance shall require `FlatResidualPct <= 1.0%` and target `<= 0.5%` for release-hardening fixtures where gain semantics are known. | Flat-field correction must reduce fixed-pattern noise while avoiding row/column artifact amplification. | Test: PRE-E2E-1 synthetic oracle, PRE-E2E-2 real fixture |
| **SRS-CALIB-FUNC-018** | System shall record calibration gain semantics as `normalized_gain`, `reciprocal_gain`, or `unknown`. Unknown semantics may run exploratory validation but shall not pass release gates. | Real legacy calibration files often encode gain differently; silent interpretation errors can create severe over/under-correction. | Test: calibration manifest parser, mismatch negative tests |
| **SRS-CALIB-FUNC-019** | System shall compute defect-correction metrics: `DefectRecall`, `DefectFPR`, `DefectResidualADU`, and `GoodPixelDeltaP99`. Synthetic BPM oracle cases shall require 100% defect recall and false-positive rate below 0.001%. | Defect correction must repair known bad pixels without altering good pixels or suppressing clinically relevant structures. | Test: PRE-E2E-1 defect synthetic oracle |
| **SRS-CALIB-FUNC-020** | System shall compute lag/ghost metrics: `LagResidualPct`, `GhostRemovalPct`, history length, tier selection, and bypass reason. Benchmark sequences with measurable lag shall require at least 90% ghost removal. | Lag and ghost correction is stateful; tier and bypass traceability is required to prevent hidden temporal contamination. | Test: PRE-E2E-1 synthetic lag, PRE-E2E-2 real lag fixtures when available |
| **SRS-CALIB-FUNC-021** | System shall compute a Calibration Effect Score (CES) from dark, flat, defect, nonlinearity, lag/ghost, reference, preservation, and performance subscores. Phase 1 implementation completeness shall require `CES >= 85`; release-hardening target shall be `CES >= 92`. Any raw preservation failure, NaN/Inf output, or unknown gain semantics shall cap the score as defined by XPE-PRE-E2E-001. | A single dashboard score helps sprint tracking, but blocking safety gates prevent high aggregate scores from hiding critical defects. | Test: PRE-E2E report score calculation |

---

### 2.6 BPM 생성 알고리즘 상세 (2026-04-19 종래기술 교차검증 결과)

다음 요구사항은 `docs/calibration/PRIOR-ART-BPM-ALGORITHM.md`의 MC/Blue 알고리즘 비교 분석 및 `tests/test_data/Grid_abnormal/` 데이터셋의 실제 검증 결과를 기반으로 추가되었습니다. 이는 기존 SRS-CALIB-FUNC-007 (BPM)의 구현 상세를 명시화합니다.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-022** | BPM 다크(어두운 영상) 불량 픽셀 검출 시, 마스크 윈도우 크기는 최소 32×32 이상이어야 합니다. RMM(Robust Mask Maker, lambda=8.0)을 사용하는 경우, 기반이 되는 국소 마스크는 32×32 픽셀 이상의 영역을 포함해야 합니다. 종래 MC 알고리즘(256×7 + 1×45 비대칭 마스크)은 적응형 마스크(32×32 이상)로 대체되어야 합니다. | 고정된 비대칭 마스크(256×7)는 이상 환경(그리드 아티팩트, 국소 아티팩트)에 취약합니다. 적응형 방식(32×32)은 지역 통계에 기반하여 더 견고한 불량 픽셀 검출을 제공합니다. 종래기술 분석(PPT 및 Grid_abnormal 데이터셋)에서 Blue 알고리즘(32×32 마스크)이 MC 대비 이상 환경에서 우수함을 입증했습니다. | Test: Grid_abnormal 데이터셋, MC vs Blue 비교 검증 |
| **SRS-CALIB-FUNC-023** | BPM 밝은(평탄화) 영상 불량 픽셀 검출 시, 마스크 윈도우 크기는 128×128 이상이어야 하며, 허용도(tolerance)는 마스크 평균의 5~9%로 설정해야 합니다. 종래 MC 알고리즘(60×60 마스크, 15% 허용도)은 보다 보수적인 Blue 알고리즘(128×128 마스크, 5~9% 허용도)으로 대체되어야 합니다. 선택 이유: 더 큰 마스크는 통계적 안정성을 향상시키며, 낮은 허용도는 과도한 오경보(False Positive)를 감소시킵니다. | MC 알고리즘의 15% 허용도는 과도한 불량 픽셀 탐지를 야기합니다(실험: MC 499개, Blue 798개 불량픽셀 탐지). 5~9% 허용도는 실제 결함과 정상 변동을 더 정확히 구분합니다. 종래기술 분석에서 Blue의 보수적 임계값이 이상 환경에서도 시각적 품질 향상을 입증했습니다. | Test: CalData_6 데이터셋, BPM 생성 및 Recall/FPR 검증 |
| **SRS-CALIB-FUNC-024** | 다중 단계 게인 보정(Multi-step Gain Calibration)에서, 단일 선량 조건에서 수집하는 평탄화 프레임의 최소 개수를 명시합니다. **수용 기준**(Frame-count Tier): (a) **최소**: 1 프레임 (노이즈 모델링 필수 — Poisson+readout noise 분산을 메타데이터에 명시하고, 다선량 보간 시 가중치로 반영); (b) **표준**: 5~10 프레임 (신호 안정화, 평균화로 DSNU 감소); (c) **권고**: 15~20 프레임 (최상의 FPN 제거); (d) **우수 품질**: 25~30 프레임 (최상의 비선형성 보정 LUT). 단일 프레임 모드에서는 `XPE_FLAG_SINGLE_FRAME_GAIN` 메타데이터 플래그를 설정하고, 생성된 gain 맵의 불확실성(uncertainty)을 `XpeCalibMetadata.gain_uncertainty` 필드에 기록해야 합니다. | 실사용환경에서는 시간 제약으로 인해 다중 프레임 취득이 불가능한 경우가 빈번합니다 (Emergency 재캘리브레이션, Field 환경). 단일 프레임이라도 오프셋 보정 후 gain 정보가 없는 것보다 낫습니다. 다만, 노이즈 불확실성을 downstream에 전달하여 임상 판단에 활용할 수 있도록 해야 합니다. CalData_6 데이터셋(6개 선량 레벨, 각 1개 프레임)은 단일 프레임 모드의 검증 데이터로 활용합니다. | Test: CalData_6 단일 프레임 모드 검증, uncertainty 필드 확인, 다중 프레임 대비 FPN 차이 < 2% 검증 |
| **SRS-CALIB-FUNC-025** | BPM 생성 알고리즘은 그리드 아티팩트(Anti-Scatter Grid Artifact) 환경에서도 견고해야 합니다. 특히 전처리(BPM 보정 적용) 후 생성된 이미지의 라인 아티팩트 점수(LineArtifactScore, 중주파 에너지 비율)가 10% 미만이어야 합니다. Blue 알고리즘을 적용할 경우, 그리드 아티팩트 환경에서도 LineArtifactScore < 5%의 우수 품질을 목표로 합니다. 측정 방법: 2D FFT 후 중주파 대역(0.05~0.3 cycles/pixel) 에너지 비율 계산. | 그리드 아티팩트는 의료 X-ray 촬영 환경에서 표준이며, BPM 알고리즘이 이상적 환경뿐만 아니라 실제 그리드 환경에서도 아티팩트를 효과적으로 제거해야 합니다. 종래기술 분석(PRIOR-ART-BPM-ALGORITHM.md) 및 Grid_abnormal 데이터셋에서 Blue 알고리즘의 그리드 견고성을 입증했습니다: Blue_NonPre (15% 라인 스코어) → Blue_Pre (5% 이후 전처리, 66% 감소). | Test: Grid_abnormal 데이터셋, Blue_Pre vs Blue_NonPre FFT 분석, LineArtifactScore < 5% 검증 |

**추가 설명:**

- **SRS-CALIB-FUNC-022 vs FUNC-007의 관계**: FUNC-007은 "BPM 적용 및 보간"을 정의하며, FUNC-022~023은 BPM **생성**의 알고리즘 상세를 명시합니다.
- **Gap 식별**: 기존 FUNC-007에서는 "RMM with lambda=8.0"만 언급되었으나, 실제 마스크 크기, 허용도 범위, 그리드 견고성이 명시되지 않았습니다. 새로운 FUNC-022~025가 이를 보완합니다.
- **Reference 문서**: `docs/calibration/PRIOR-ART-BPM-ALGORITHM.md`에서 MC/Blue 알고리즘 상세 비교 및 XPE RMM 개선 권고사항을 참고하세요.

---

### 2.7 Calibration Map Generation and Real-World Environment Adaptation (2026-04-24 v1.1)

다음 요구사항은 실사용환경(Factory 및 Field)에서 캘리브레이션 맵 생성 및 적응을 위한 기능을 정의합니다. 기존 FUNC-001~003이 맵 **로딩**을 다룬다면, 이 섹션은 맵 **생성**과 **실환경 적응**을 다룹니다.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-026** | System shall provide `xpe_calib_generate_gain()` function that creates gain map from flat-field frame stack. Input: array of `XpeImageBuffer` (flat-field frames, uint16), `XpeImageBuffer` (dark reference), detector metadata (kVp, mAs, SID, temperature). Algorithm: (1) Subtract dark reference from each flat-field frame; (2) Compute pixel-wise mean of dark-subtracted frames → raw gain map; (3) Normalize by dividing by global mean → flat-field gain map G(x,y); (4) Store with associated metadata (kVp, SID, T_ref). Output: `.xpe_calib` gain file with `XCAL_TYPE_GAIN`. Single-frame mode (FUNC-024 Tier a) shall compute gain from one dark-subtracted frame with uncertainty estimation. | Gain map generation is essential for both factory and field calibration. Current system only provides `xpe_calib_load_gain` but no generation function, forcing reliance on external tools. Field recalibration and emergency calibration require in-system gain map creation. | Test: Synthetic flat-field → gain map, CalData_6 multi-dose generation, single-frame uncertainty |
| **SRS-CALIB-FUNC-027** | System shall support multi-point gain polynomial fitting via `xpe_calib_generate_gain_polynomial()`. Input: array of gain maps at N ≥ 3 dose levels (each from FUNC-026), dose levels array. Algorithm: For each pixel (x,y), fit polynomial `G(x,y,E) = Σ(c_k × E^k)` where k=0..degree-1 (degree ≤ 4). Use least-squares regression. Output: polynomial coefficient array stored in `.xpe_calib` with `XCAL_TYPE_GAIN_POLY`. Coefficient validation: all coefficients must be finite; polynomial must be monotone in [E_min, E_max]. If non-monotone, reduce degree until monotonicity achieved (minimum degree 1 = linear). | Clinical X-ray uses multiple kVp values (50-120 kVp). Single gain map at 70 kVp RQA-5 introduces errors at other clinical energies. Multi-point polynomial captures energy-dependent sensitivity variation. CalData_6 (6 dose levels) and cyan_test (5 CalSet levels) provide real calibration data for validation. | Test: CalData_6 polynomial fit, cyan_test CalSet verification, monotonicity check, degree fallback |
| **SRS-CALIB-FUNC-028** | System shall support field calibration workflow via `xpe_calib_field_generate()`. This function combines offset and gain generation for field use with simplified acquisition: (1) Acquire N_dark ≥ 5 dark frames (single temperature, current PREP time); (2) Acquire N_flat ≥ 1 flat-field frames at clinical kVp (not restricted to RQA-5); (3) Generate offset map using mean averaging (same algorithm as `xpe_calib_generate_offset`); (4) Generate gain map using FUNC-026 algorithm; (5) Tag both files with `source: "field"`, `detector_temp: <current>`, `acquisition_duration: <measured>`; (6) Set expiry to configurable field period (default 30 days, shorter than factory 90 days). Field calibration shall log comparison metrics against previously loaded calibration: DarkBias delta, PRNU delta, defect count delta. | Field calibration occurs in clinical environments where full factory protocol is impractical. Simplified protocol (5+ dark, 1+ flat) enables periodic recalibration during maintenance windows. Shorter expiry (30 days) reflects lower statistical quality of field-acquired maps. Comparison logging enables drift detection over time. | Test: Field simulation with CalData_6 subset, comparison metrics output, expiry assignment |
| **SRS-CALIB-FUNC-029** | System shall support calibration drift detection via `xpe_calib_check_drift()`. When invoked with a new set of dark/flat frames (minimum 5 dark, 1 flat), system shall: (1) Compute quick offset statistics (mean, σ); (2) Compare against loaded offset map statistics; (3) Compute quick gain statistics (PRNU); (4) Compare against loaded gain map statistics; (5) Return drift report with metrics: DarkBiasDriftPct, PRNUChangePct, DefectCountDelta. If `DarkBiasDriftPct > 10%` or `PRNUChangePct > 5%` or `DefectCountDelta > 50`, system shall recommend recalibration via `XPE_WARN_RECALIBRATION_RECOMMENDED`. | FPD characteristics drift over time (dark current aging, new defect formation). Without drift detection, stale calibration accumulates systematic errors. Quick-check protocol (5 dark + 1 flat, <30 seconds acquisition) enables routine QA without full recalibration. Threshold values based on IEC 62220-1-1 DQE degradation sensitivity. | Test: Simulated drift injection, threshold trigger verification, false-positive rate |
| **SRS-CALIB-FUNC-030** | System shall support real-time offset adaptation for temperature changes during clinical acquisition. When `calibration.realtime_offset_adapt` is enabled in config and NTC sensor data is available in frame metadata: (1) On each frame, read current temperature T_current from metadata; (2) If |T_current - T_ref| > 2°C and T_current is within [T_min, T_max] range of loaded offset map: compute adjusted offset using exponential model (FUNC-004); (3) If T_current is outside loaded range: clamp to nearest boundary and log warning `XPE_WARN_OFFSET_TEMP_OOR`; (4) Track temperature history (rolling 100-frame window) and log temperature statistics per session. This adaptation shall NOT modify the stored offset map — only the in-memory working copy. | Clinical environments experience detector temperature drift during continuous acquisition (e.g., fluoroscopy). Static offset maps calibrated at 25°C introduce bias at 35°C. Real-time adaptation using the same exponential model (FUNC-004) but applied per-frame ensures consistent image quality without requiring recalibration. In-memory-only modification preserves original calibration for audit. | Test: Temperature ramp simulation, offset adaptation accuracy, boundary handling, memory isolation |

### 2.8 Calibration Mode Selection and Optimization (2026-04-24 v1.2)

The following requirements define explicit calibration mode selection, multi-point performance optimization, and quality metadata recording for gain calibration workflows. These extend FUNC-024 (frame count tiers) and FUNC-027 (multi-point polynomial fitting) with a unified mode selection mechanism.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-031** | **Calibration Mode Selection API.** System shall support explicit calibration mode selection via `XpeCalibrationMode` enum: `XPE_CALIB_MODE_SINGLE_POINT` (0, 1 dose level, 1 frame per level), `XPE_CALIB_MODE_DUAL_POINT` (1, 2 dose levels, linear interpolation), `XPE_CALIB_MODE_MULTI_POINT_5` (2, 5 dose levels, polynomial degree ≤ 2), `XPE_CALIB_MODE_MULTI_POINT_8` (3, 8 dose levels, polynomial degree ≤ 3), `XPE_CALIB_MODE_MULTI_POINT_10` (4, 10 dose levels, polynomial degree ≤ 4, maximum allowed), `XPE_CALIB_MODE_AUTO` (5, auto-select based on input data). (1) A new API function `xpe_calib_set_mode(XpeCalibrationMode mode)` shall set the active calibration mode. (2) A new API function `xpe_calib_get_mode()` shall return the current mode. (3) When mode is set to a specific value (not AUTO), the system shall enforce the maximum dose level count: SINGLE_POINT max 1 level, DUAL_POINT max 2 levels, MULTI_POINT_5 max 5 levels, MULTI_POINT_8 max 8 levels, MULTI_POINT_10 max 10 levels (hard cap). (4) Exceeding the mode's max levels shall return `XPE_ERR_INVALID_INPUT` with a descriptive message. (5) AUTO mode logic: based on input dose_levels count, auto-select the smallest fitting mode. (6) Mode selection shall be persisted in the generated XCal file metadata. (7) Default mode shall be `XPE_CALIB_MODE_MULTI_POINT_8` (industry standard per Rayence, Schmidgunst 2007). (8) Mode shall apply to both `xpe_calib_generate_gain()` (FUNC-026) and `xpe_calib_generate_gain_polynomial()` (FUNC-027). | Medical X-ray FPD calibration requires flexibility between quick single-point (emergency/field) and thorough multi-point (factory). Industry standards (Schmidgunst 2007, Rayence DR panels) recommend 8 calibration points as the optimal balance between accuracy and acquisition time. Explicit mode selection prevents under/over-calibration. | Test: Unit test for each mode, boundary test for max levels, AUTO mode selection test |
| **SRS-CALIB-FUNC-032** | **Multi-Point Calibration Performance Optimization.** System shall optimize multi-point calibration for both memory and computation. (1) **Online Accumulative Fitting**: Instead of loading all N gain maps into memory simultaneously, system shall process dose levels one at a time using online least-squares accumulation. Memory usage shall be O(W×H×degree) independent of N (dose level count), NOT O(N×W×H). (2) **SIMD-Parallel Polynomial Fitting**: Least-squares fitting per pixel shall be vectorized using AVX2/SSE4.2 intrinsics. Target: ≥ 4× speedup over scalar implementation for 3072×3072 images. (3) **Automatic Degree Reduction**: After fitting at max degree, system shall check R² improvement from degree-1. If improvement < 0.001 (negligible), reduce degree by 1 and re-fit. Continue until improvement is significant or degree reaches 1. (4) **Hard Cap at 10 Points**: Maximum calibration points shall be 10 regardless of user input. If user provides more than 10 dose levels, system shall use the 10 most uniformly spaced levels and log warning `XPE_WARN_CALIB_POINTS_CAPPED`. (5) **Pre-computed LUT**: After polynomial fitting, system shall pre-compute a LUT mapping raw ADU to corrected values. LUT size: 4096 entries (12-bit) or 65536 entries (16-bit). Runtime correction becomes O(1) per pixel regardless of polynomial degree. (6) **Batch Processing**: Multiple frames at the same dose level shall be processed using SIMD batch averaging before accumulation. Performance targets: 5-point fitting ≤ 2s, 8-point fitting ≤ 4s, 10-point fitting ≤ 6s for 3072×3072 image; LUT generation ≤ 500ms after fitting. | Multi-point calibration with N>5 without optimization causes: (a) memory blowup (N × 37.7 MB per gain map), (b) excessive fitting time, (c) overfitting risk. Online accumulation reduces memory from O(N×W×H) to O(W×H×degree). SIMD parallelization is critical for 9.4M pixel detectors. Degree auto-reduction prevents overfitting on well-behaved detectors. | Test: Performance benchmark (5/8/10 points), memory profiling, R² improvement validation, degree reduction test |
| **SRS-CALIB-FUNC-033** | **Calibration Quality Metadata Recording.** System shall record comprehensive quality metadata for all calibration modes. (1) **Mandatory Metadata Fields**: Every generated XCal gain file shall include: `calibration_mode` (XpeCalibrationMode enum value as string), `actual_dose_levels` (number of dose levels used), `polynomial_degree` (fitted polynomial degree; 0 for single-point), `fit_r_squared` (coefficient of determination; 1.0 for single-point by definition), `max_residual_pct` (maximum fitting residual as percentage of full scale), `mean_residual_pct` (mean fitting residual as percentage), `acquisition_duration_s` (total acquisition time in seconds), `detector_temperature_c` (detector temperature during calibration). (2) **Quality Gate**: If `fit_r_squared < 0.999` after fitting, system shall log `XPE_WARN_CALIB_POOR_FIT` and include recommendation to increase dose levels or check detector stability. (3) **Comparison with Previous**: When overwriting an existing calibration file, system shall compute and log: `dark_bias_delta` (change in dark bias vs previous), `prnu_delta_pct` (change in PRNU vs previous), `defect_count_delta` (change in defect count vs previous). (4) **Mode-Specific Metadata**: SINGLE_POINT shall additionally record `gain_uncertainty` (estimated from single-frame noise model); MULTI_POINT shall additionally record `per_point_r_squared[]` array. (5) All metadata shall be stored in XCal file header section (JSON-encoded in reserved header bytes). | Quality metadata enables: (a) automated QA pass/fail decisions, (b) drift tracking across calibration sessions, (c) regulatory traceability (IEC 62304), (d) mode comparison studies. R² < 0.999 threshold aligns with REQ-NLN-005 in PRD. | Test: Metadata presence validation, R² gate test, comparison metrics test, XCal header parsing test |

**Mode-to-Parameter Mapping:**

| Mode | Max Dose Levels | Max Poly Degree | Typical Use Case | Acquisition Time (3072×3072) |
|------|:--------------:|:--------------:|------------------|:--------------------------:|
| SINGLE_POINT | 1 | 0 (constant) | Emergency / field recalibration | ~2s |
| DUAL_POINT | 2 | 1 (linear) | Quick factory verification | ~5s |
| MULTI_POINT_5 | 5 | 2 (quadratic) | Standard clinical calibration | ~15s |
| MULTI_POINT_8 | 3 (cubic) | 8 | Full factory calibration (recommended) | ~30s |
| MULTI_POINT_10 | 4 (quartic) | 10 | Maximum precision / research | ~45s |
| AUTO | N/A (auto) | Determined by input count | Hands-off operation | Varies |

**Relationship to Existing Requirements:**

- **FUNC-031 vs FUNC-024**: FUNC-024 defines frame count tiers per dose level; FUNC-031 defines how many dose levels to use and the polynomial degree ceiling. Together they determine total frames: mode_count × frames_per_level.
- **FUNC-031 vs FUNC-027**: FUNC-027 defines the polynomial fitting algorithm; FUNC-031 adds mode enforcement (max levels, max degree) before fitting begins. FUNC-032 optimizes the fitting process itself.
- **FUNC-033 vs FUNC-015**: FUNC-015 defines E2E metric reporting; FUNC-033 defines calibration-specific quality metadata in XCal files. FUNC-033 metadata is consumed by FUNC-015 reporting during validation runs.

---

## 3. Safety Requirements

### 3.1 Mandatory Correction Policy (SRS-CALIB-SAFE-001 through SRS-CALIB-SAFE-003)

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SRS-CALIB-SAFE-001** | Offset correction (SRS-CALIB-FUNC-004) and gain correction (SRS-CALIB-FUNC-005) shall be mandatory and non-bypassable. System shall return hard error `XPE_ERR_NOT_INITIALIZED` if either offsetMap or gainMap is absent at pipeline start. Defect, ghost, nonlinearity, binning, and temperature corrections are optional and conditional. | Dark current bias and pixel gain variation are inherent detector artifacts present in all frames. Uncorrected images contain systematic errors that compromise diagnostic accuracy. Mandatory flag prevents accidental misconfiguration. | Test: Bypass prevention, error codes |
| **SRS-CALIB-SAFE-002** | System shall enforce calibration expiry checking (SRS-CALIB-FUNC-009). Pipeline shall abort image acquisition if any loaded calibration file has expired (current_time_ms > expiryEpochMs). Error code `XPE_ERR_CALIBRATION_EXPIRED` shall be returned and propagated to user interface. | Expired calibration introduces known systematic bias into diagnostic images. Hard enforcement prevents clinical use of stale data and ensures regulatory compliance (21 CFR Part 11, IEC 62304). | Test: Expiry validation |
| **SRS-CALIB-SAFE-003** | All calibration file loads (offset, gain, BPM) shall validate file integrity via CRC-32 checksum. Corrupted files shall be rejected with error `XPE_ERR_IO_FAILED`. No partial corrections shall be applied. Pipeline shall fail atomically: either all calibration files load successfully, or none are loaded. | CRC-32 prevents silent data corruption that could lead to systematic image bias. Atomic behavior ensures consistency: either fully calibrated or raw pass-through. | Test: Corruption detection |

**Test GUI evaluation exception:** `ImageProcTest.exe` may expose `Off`, `On`, and `Auto` controls for each calibration/preprocessing stage to support algorithm effect and performance evaluation. This exception is limited to QA/Test GUI workflows, shall be labelled evaluation-only, shall not relax the product-mode mandatory offset/gain policy, and shall record every bypass or forced-stage decision in the automation/evidence report.

### 3.2 Data Integrity and Buffer Protection (SRS-CALIB-SAFE-004 through SRS-CALIB-SAFE-005)

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SRS-CALIB-SAFE-004** | Calibration correction shall never modify the input image buffer. All corrections shall operate in-place on working buffers or produce separate output buffers. Original uint16 input data shall remain accessible for diagnostic/audit purposes. | In-place modification risks data loss and prevents audit trail reconstruction. Preservation of original enables forensic analysis and QA verification. | Test: Input buffer preservation |
| **SRS-CALIB-SAFE-005** | Gain correction float32 output shall be bounds-checked to prevent overflow/underflow. Maximum pixel value after gain correction shall be capped at 3.4e38 (float32 max exponent). Pixels exceeding this shall be clamped to 3.4e38 with warning log. Minimum value shall be 0.0 (no negative corrected values). | Float32 overflow produces infinity/NaN which corrupt downstream processing. Clipping is preferable to NaN. Bounds checking prevents silent data corruption. | Test: Overflow protection |

---

## 4. Performance Requirements

### 4.1 Processing Speed (SRS-CALIB-PERF-001)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-001** | Total preprocessing pipeline execution time shall not exceed 500 ms per 3072×3072 float32 frame on Intel Core i7 or equivalent processor. Breakdown: (0) CalibManager load ≤200 ms (one-time startup), (1) Offset ≤55 ms, (1.5) Nonlinearity ≤20 ms, (2) Gain ≤55 ms, (2.5) Binning ≤10 ms, (3) Defect ≤95 ms, (4) Ghost Tier 1 ≤140 ms, (4) Ghost Tiers 2-3 ≤+130 ms. | Clinical workflow requires <1 second per frame (including enhancement). Detailed budgets prevent bottleneck phases from exceeding hard limit. Timer instrumentation shall log per-phase duration. | Test: Profiling on reference hardware |

### 4.2 Memory Requirements (SRS-CALIB-PERF-002)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-002** | Peak memory allocation shall not exceed 200 MB per frame processing pipeline. Breakdown: offset map (18.9 MB) + gain map (37.7 MB) + BPM (9.4 MB) + working buffer (37.7 MB) + ghost history (150 MB, optional) = max 190 MB. System shall free allocated memory after frame processing completes. No memory leaks during 100-frame batch processing. | Desktop console memory constraints (typical 4-8 GB, shared with UI). 200 MB limit ensures multi-frame processing without swapping. Batch processing validation ensures long-running acquisitions remain stable. | Test: Memory profiling, leak detection |

### 4.3 File I/O Performance (SRS-CALIB-PERF-003)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-003** | Calibration file load time (CalibManager initialization) shall not exceed 200 ms for all three files (offset, gain, BPM) on SSD. File seek time from disk to memory shall be optimized via sequential read (not random access). CRC-32 validation shall be incremental (calculated during read, not post-hoc). | Clinical workflows load calibration once at startup, not per-frame. 200 ms overhead is negligible in context of 500 ms per-frame budget. Sequential read + streaming CRC minimize I/O latency. | Test: Timed file loads |

---

## 5. Interface Requirements

### 5.1 Input Interfaces (SRS-CALIB-IF-001)

| Req ID | Interface | Input Type | Data Format | Constraints |
|--------|-----------|-----------|-------------|-------------|
| **SRS-CALIB-IF-001** | Raw image data | `XpeImageBuffer` struct (uint16) | 14-16 bit unsigned integer per pixel | 3072×3072 or 4096×4096 maximum |
| **SRS-CALIB-IF-002** | Calibration file paths | String paths (C ABI: `const char*`) | UTF-8 encoded file paths | Paths provided by C# orchestrator (ImageProcTest.exe) |
| **SRS-CALIB-IF-003** | Detector metadata | `XpeDetectorProfile` struct | JSON or binary struct (temperature, kVp, SID, binning mode) | Required for temperature compensation and gain selection |
| **SRS-CALIB-IF-004** | Configuration | JSON config file (xpe_preprocess_config.json) | Key-value pairs for bypass flags, algorithm parameters | Loaded at startup via `xpe_configure()` |

### 5.2 Output Interfaces (SRS-CALIB-IF-002)

| Req ID | Interface | Output Type | Data Format | Constraints |
|--------|-----------|-----------|-------------|-------------|
| **SRS-CALIB-IF-005** | Corrected image | `XpeImageBuffer` struct (float32) | 32-bit floating-point per pixel | Output after Gain correction (format boundary) |
| **SRS-CALIB-IF-006** | Status flags | `uint32_t flags` field (XpeImageMetadata) | Bitmask: `XPE_FLAG_READOUT_VALIDATED`, `XPE_FLAG_TEMP_COMPENSATED`, `XPE_FLAG_GAIN_CORRECTED`, `XPE_FLAG_DEFECT_CORRECTED`, `XPE_FLAG_GHOST_CORRECTED`, etc. | Flags indicate which corrections were applied |
| **SRS-CALIB-IF-007** | Error codes | `XpeErrorCode` enum (int32_t) | `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_CALIBRATION_EXPIRED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_CALIB_DATA` | Return value from correction functions |
| **SRS-CALIB-IF-008** | Diagnostic log | JSON log object (optional) | Key-value pairs: phase durations, bypass decisions, warnings, defect counts | Attached to `XpeImageMetadata.diagnosticLog` |

### 5.3 C ABI Function Signatures (SRS-CALIB-IF-003)

Core calibration functions shall be exported from `xpe_preprocess.dll` with C ABI (no name mangling):

```c
// Calibration Data Load
XpeErrorCode xpe_calib_load_offset(const char* filepath);
XpeErrorCode xpe_calib_load_gain(const char* filepath);
XpeErrorCode xpe_calib_load_defect_map(const char* filepath);

// Correction Functions
XpeErrorCode xpe_offset_correct(XpeImageBuffer* image);
XpeErrorCode xpe_gain_correct(XpeImageBuffer* image);  // Output: float32
XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* image);
XpeErrorCode xpe_defect_correct(XpeImageBuffer* image);
XpeErrorCode xpe_temp_compensate(XpeImageBuffer* image, float temp_celsius);
XpeErrorCode xpe_binning_correct(XpeImageBuffer* image, int binning_mode);

// Ghost/Lag Correction (Stateful)
XpeGhostHandle xpe_ghost_create(XpeGhostConfig* config);
XpeErrorCode xpe_ghost_correct(XpeGhostHandle handle, XpeImageBuffer* image);
XpeErrorCode xpe_ghost_reset(XpeGhostHandle handle);
void xpe_ghost_destroy(XpeGhostHandle handle);

// Configuration & Expiry
XpeErrorCode xpe_configure(const char* json_config);
XpeErrorCode xpe_calib_check_expiry(void);
```

### 5.4 Error Code Contract (SRS-CALIB-IF-004)

All functions return `XpeErrorCode` enum with following semantics:

| Error Code | Meaning | Recovery |
|-----------|---------|----------|
| `XPE_OK` (0) | Operation succeeded | Proceed to next stage |
| `XPE_ERR_NOT_INITIALIZED` (-1) | Calibration data not loaded | Load calibration and retry |
| `XPE_ERR_CALIBRATION_EXPIRED` (-2) | Calibration file timestamp > expiry | Update calibration file, abort acquisition |
| `XPE_ERR_IO_FAILED` (-3) | File CRC failed or read error | Check file integrity, reload |
| `XPE_ERR_INVALID_CALIB_DATA` (-4) | Calibration file format or value error | Regenerate calibration |
| `XPE_ERR_INVALID_PARAM` (-5) | Invalid function parameter (null pointer, wrong size) | Validate inputs before calling |

---

## 6. Non-Functional Requirements

### 6.1 Reliability and Robustness (SRS-CALIB-NFR-001 through SRS-CALIB-NFR-003)

| Req ID | Requirement | Verification |
|--------|------------|--------------|
| **SRS-CALIB-NFR-001** | All dynamic memory allocations shall check for null pointer returns. Allocation failures shall return `XPE_ERR_NOT_INITIALIZED` rather than crashing. No recursive allocations in hot path (per-frame corrections). | Test: Malloc interception, OOM simulation |
| **SRS-CALIB-NFR-002** | All buffer array accesses shall be bounds-checked. Array index out-of-bounds shall be caught and return `XPE_ERR_INVALID_PARAM` rather than causing buffer overflow. | Test: Fuzz testing with invalid indices |
| **SRS-CALIB-NFR-003** | Thread safety: All correction functions shall be reentrant and thread-safe when called with non-overlapping image buffers. Ghost correction functions (stateful) shall be protected by mutex per handle. | Test: Concurrent frame processing |

### 6.2 Determinism and Reproducibility (SRS-CALIB-NFR-004)

| Req ID | Requirement | Verification |
|--------|------------|--------------|
| **SRS-CALIB-NFR-004** | Identical input image + calibration files + configuration + detector metadata shall produce byte-identical output (deterministic). No floating-point rounding differences across runs. No random number generators in deterministic code paths. | Test: Multiple runs with same inputs, output hash comparison |

### 6.3 Regulatory Compliance (SRS-CALIB-NFR-005 through SRS-CALIB-NFR-006)

| Req ID | Requirement | Verification |
|--------|------------|--------------|
| **SRS-CALIB-NFR-005** | All calibration operations shall be audit-loggable. Decisions to bypass corrections, detected defects, expiry checks shall be logged to diagnostic JSON with timestamps. | Test: Log completeness audit |
| **SRS-CALIB-NFR-006** | System shall support IEC 62304 traceability: each frame shall carry metadata (frame_id, calibration_version, session_id, timestamp, applied_corrections, warnings) for post-acquisition review. | Test: Metadata tagging |

---

## 7. Requirement Verification Method

### 7.1 Verification Methods by Requirement Type

| Category | Verification Method | Examples |
|----------|-------------------|----------|
| **Functional Correctness** | Unit test + reference image comparison + automated detector-domain E2E metrics | SRS-CALIB-FUNC-001 to SRS-CALIB-FUNC-021: compare corrected output to synthetic/golden reference where available and compute PRE-E2E dark, flat, defect, lag, preservation, and performance gates |
| **Safety** | Code review + threat modeling | SRS-CALIB-SAFE-001 to SRS-CALIB-SAFE-005: Verify abort conditions, buffer overflow prevention, expiry enforcement |
| **Performance** | Profiling + benchmark suite | SRS-CALIB-PERF-001 to SRS-CALIB-PERF-003: Measure wall-clock time and memory usage on reference hardware |
| **Interface** | Integration test + API contract verification | SRS-CALIB-IF-001 to SRS-CALIB-IF-008: Test C ABI function signatures and return codes |
| **Robustness** | Fuzz testing + stress testing | SRS-CALIB-NFR-001 to SRS-CALIB-NFR-006: Test with malformed files, concurrent access, OOM conditions |

### 7.2 Test Specification References

All requirements shall have traceability to corresponding test cases in:
- `tests/calib/test_offset_correction.cpp`
- `tests/calib/test_gain_correction.cpp`
- `tests/calib/test_defect_correction.cpp`
- `tests/calib/test_ghost_correction.cpp`
- `tests/calib/test_calib_file_io.cpp`
- `tests/calib/test_safety_expiry.cpp`
- `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`
- `tests/test_data/calibration_cases/README.md`
- `tests/calib/perf_benchmark.cpp`

---

## References

### Standards and Regulations

| Reference | Relevance |
|-----------|-----------|
| IEC 62304:2006 + A1:2015 | Medical device software life cycle processes; Class B safety classification |
| IEC 62220-1-1:2015 | Measurement of the Detective Quantum Efficiency (DQE) of digital X-ray imaging detectors |
| ISO 14971:2019 | Medical devices — Application of risk management to the manufacture of medical devices |
| 21 CFR Part 11 | Electronic Records; Electronic Signatures (FDA) — calibration date/expiry tracking |

### Research Papers

| Citation | Topic | Application |
|----------|-------|-------------|
| Starman et al. (2012) | Signal-dependent lag correction in flat-panel detectors | SRS-CALIB-FUNC-013: Tier 3 NLCSC algorithm |
| Wang et al. (2013) | Heel effect correction in dual-SID imaging | SRS-CALIB-FUNC-005: Multi-gain correction |
| Ranger et al. (2014) | Characterization of lag in fluoroscopic FPD | SRS-CALIB-FUNC-008: Exponential dark current model |
| Pang et al. (2006) | Multi-exponential lag model | SRS-CALIB-FUNC-013: Tier 1 LTI deconvolution |
| FixPix (2023) | Deep learning defect correction (MLP) | SRS-CALIB-FUNC-007: Advanced defect mode |
| EP2148500A1 | Dynamic dark current compensation | SRS-CALIB-FUNC-004: Temperature/time interpolation |

### Project Documentation

| Document | Location |
|----------|----------|
| Calibration Module README | `docs/calibration/README.md` |
| X-ray FPD Algorithm Specification | `.moai/specs/xpe-algorithm-spec-deepsync.md` |
| Pipeline Architecture | `.moai/project/pipeline-spec.md` |
| Detector Profiles | `.moai/project/detector-profiles.json` |
| Calibration Data Format | `docs/calibration/FORMAT_SPEC.md` |

---

*SRS-CALIB-001 v1.2 — End of Document*
