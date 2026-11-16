# Software Requirements Specification - XPE Preprocessing Calibration Module

**Document ID:** SRS-CALIB-001 v1.6  
**IEC 62304 Clause:** 5.2 (Software Requirements Specification)  
**Safety Classification:** Class B  
**Date:** 2026-10-16 (v1.3: 2026-10-03, v1.2: 2026-04-24)  
**Revision v1.3 (2026-10-03, #245 사용자 결정 / `QA-A-233` 결정 11·12·14):** 구현이 없는 FUNC-011·014·015·021·028·029·030 의 검증 칸에 "미구현(요구 유지) — 사용자 결정 2026-10-03, #245" 표시. 요구 문장은 바꾸지 않았다. 변경 기록 `docs/project/REQ-CHANGE-LOG-2026-10-03-WORDING.md`.  
**Revision v1.4 (2026-10-16, 사용자 지시 "결함 화소 맵은 최신 기술 리서치로 개선"):** FUNC-022·023 개정(특성별 합집합 검출, 데이터로 정한 임계값, 라인·덩어리 결함), FUNC-025 의 Blue 문구 삭제, FUNC-039 신설(라인 기록·방향 보간). 근거와 원문 대조: `.moai/state/research/bpm-advanced-research.md`, `verify-bpm-claims.md`.  
**Revision v1.6 (2026-11-17, 리더 결정):** FUNC-030 을 삭제 (사용자 결정 2026-10-15 단일 오프셋 맵, 2026-11-16 온도 보정 제거). FUNC-005 의 에너지 다항식 다중 게인 조항을 삭제 (사용자 결정 2026-11-16 다항 게인 제거; 다항 gain.xcal 형식 3 은 엔진이 거부, xpe-pre d3216b2a). FUNC-005 의 단일 게인 보정·힐 효과 조항은 유지.  
**Revision v1.5 (2026-11-17, 사용자 결정 2026-11-16):** 온도 보상(FUNC-008, FUNC-004 의 온도 조항), 비선형 보정(FUNC-006, FUNC-006-EXT), 다선량 다항식 게인(FUNC-027)과 그 최적화(FUNC-032)를 삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 a72d59c9·2d9576de·d3216b2a). 요구 ID 는 추적을 위해 남기고 번호는 바꾸지 않았다. FUNC-031 은 단일점(SINGLE_POINT) 하나로 축소했다.  
**Author:** XPE Calibration Development Team  
**Approval:** __________________ Date: __________  

---

## 1. Purpose and Scope

### 1.1 Purpose

This Software Requirements Specification (SRS) defines all functional, safety, performance, and interface requirements for the XPE Preprocessing Calibration Module (`xpe_preprocess.dll`, Layer 1, Phase 1a). The module transforms raw detector data (uint16, 14-16 bit ADC output) into calibrated, corrected images (float32) by applying systematic corrections for physical detector artifacts including dark current offset, pixel gain variation, defective pixels, lag/ghosting, and binning effects. (Temperature-drift and nonlinearity corrections were removed — user decision 2026-11-16, FUNC-006·008 삭제.)

### 1.2 Scope

This document specifies requirements for calibration data management, correction algorithms, and quality assurance mechanisms required to meet IEC 62304 Class B medical device safety standards. The module is required for all clinical imaging workflows and mandatory for any system using the xpe_preprocess.dll interface.

**Out of scope:** Image enhancement processing (CLAHE, log transform, edge enhancement) is handled downstream in `xpe_enhance_basic.dll`. Grid suppression and virtual grid correction are handled in `gsvg.dll`.

---

## 2. Functional Requirements

### 2.1 Calibration Data Load Management (SRS-CALIB-FUNC-001 through SRS-CALIB-FUNC-003)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-001** | System shall load offset calibration file (.xpe_calib format) containing dark current map (uint16). File format: 34-byte header (magic=0x585045, version, timestamp, expiryEpochMs, reserved) + CRC-32 (4 bytes) + pixel data (3072×3072×2 bytes). CRC-32 validation via polynomial 0x04C11DB7 shall reject corrupted files with error `XPE_ERR_IO_FAILED`. | Offset map is essential baseline for all downstream corrections; CRC-32 protects against silent data corruption. Factory-calibrated maps have expiry epochs for compliance tracking. | Test: CRC validation, corruption detection |
| **SRS-CALIB-FUNC-002** | System shall load gain calibration file (.xpe_calib format) containing normalization factors (float32). File format: 34-byte header + CRC-32 + gain coefficients (3072×3072×4 bytes). Values are expected in range [0.1, 10.0]; each out-of-range pixel shall be classified as a defect pixel (corrected by SRS-CALIB-FUNC-007) and reported with a warning, not rejected as a whole map. If the out-of-range fraction exceeds an upper bound, the map shall be rejected with `XPE_ERR_INVALID_CALIB_DATA` (provisional bound 5%, reused from the BPM density limit of SRS-CALIB-FUNC-003 — not yet derived from measured detector data: the only real map measured has 0.445%; to be confirmed when more detector samples exist, QA-A-241 / Codex #157). [Changed 2026-10-05, user decision: a real detector gain map built by the product generator carries 42,026 out-of-range pixels (min 0.0, max 15.41), so whole-map rejection made the real map unusable — QA-A-240.] | Gain map normalizes pixel-to-pixel sensitivity variation (FPN). Float32 enables multi-gain polynomial support. Range limits prevent over/under-correction artifacts. | Test: Range validation, file parsing |
| **SRS-CALIB-FUNC-003** | System shall load bad pixel map (BPM) from .xpe_calib file (uint8, 1 byte per pixel). BPM format: pixel value 0=good, 1-255=defect type (1=dead, 2=hot, 3=stuck, 4=noisy). Sparse map optimization supported via run-length encoding (RLE). Maximum 5% defect density tolerance: a map above 5% (exactly 5% is within tolerance) is loaded and the load raises warning `XPE_WARN_DEFECT_MAP_OVER_LIMIT`, consistent with the combined-mask 5% warning. [Over-limit behaviour defined 2026-10-05, user decision "warn and load" — the earlier text named the limit but no behaviour, QA-A-241c / Codex #159.] | BPM enables targeted defect correction without full-image filtering. RLE compression reduces memory footprint (typical 9.4MB to <500KB). Defect type field supports algorithmic selection. | Test: BPM parsing, RLE decompression |

> **파일 형식 정정 (2026-09-18, #188 — QA-A-106 에서 실제 파일과 코드로 확인)**
>
> 위 FUNC-001·FUNC-002 와 아래 SAFE-003·PERF-003 이 적은 파일 형식은 실제 구현과 다릅니다. 실제(`.xcal`)는 다음과 같습니다.
>
> | 항목 | 문서(옛 서술) | 실제 |
> |---|---|---|
> | 헤더 크기 | 34바이트 | **152바이트** |
> | 무결성 값 | CRC-32 4바이트(다항식 0x04C11DB7) | **SHA-256 32바이트**, 헤더 오프셋 120 |
> | 검증 방식 | post-hoc CRC | 읽는 중 스트리밍 SHA-256(QA-A-105), Windows 는 CNG(QA-A-106) |
> | 게인 값 범위 검사 [0.1, 10.0] → `XPE_ERR_INVALID_CALIB_DATA` | 요구에 있음 | **구현 없음** (별도 이슈로 다룸) |
>
> 무결성은 CRC-32 로 약화하지 않습니다. 교정 파일이 조용히 손상되면 모든 영상에 계통 오차가 생기기 때문입니다. PERF-003 의 200 ms 는 CNG 전환 뒤 3파일 합계 130–137 ms 로 충족합니다(#179).

### 2.2 Pixel-Level Corrections (SRS-CALIB-FUNC-004 through SRS-CALIB-FUNC-009)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-004** | System shall apply offset (dark current) correction: `I_corr(x,y) = I_raw(x,y) - I_dark(x,y)`. Negative results after offset subtraction shall be clamped to 0 (uint16 domain). ~~Temperature compensation (exponential dark-current model with NTC sensor data)~~ — 온도 보상 조항은 삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 a72d59c9). PREP-time exponential decay model `offset_adj = offset × exp(-t_prep / τ)` shall be applied when `t_prep` metadata is provided (τ=0.1s default). | Dark current is primary readout artifact. PREP-time decay models dark current accumulation after detector reset. | Test: Offset arithmetic, clamp verification, PREP-time decay |
| **SRS-CALIB-FUNC-005** | System shall apply gain (flat-field) correction: `I_norm(x,y) = I_corr(x,y) / G(x,y)`. Output shall be converted to float32 format. Gain values shall be validated to be non-zero; division by zero shall return `XPE_ERR_INVALID_CALIB_DATA`. ~~Multi-gain mode with energy-dependent polynomial `G(x,y,E) = Σ(c_k × E^k)` for SID-specific gain maps~~ — 다중 게인 다항식 조항은 삭제 (사용자 결정 2026-11-16 다항 게인 제거; 다항 gain.xcal 형식 3 은 엔진이 거부, xpe-pre d3216b2a). Heel effect correction (Wang 2013 projection model) shall be supported. | Gain correction normalizes across detector's FPN and SID-dependent intensity falloff. Float32 conversion at this stage is mandatory (format boundary). | Test: Gain arithmetic, format conversion |
| **SRS-CALIB-FUNC-006** | ~~Nonlinearity correction (LUT / polynomial, `panel.linear`, `panel.nonlinearity_mode`)~~ — **삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a)**. 요구 문장은 효력이 없다. 단일 선량 게인이 다른 선량을 시간 잡음 수준까지 평탄화해 필요가 측정되지 않았다(패널 5종). | — | — |

**SRS-CALIB-FUNC-006-EXT (비선형 보정 알고리즘 확장: LUT·다항식 방법, 정밀도 비교, 선택 논리)** — **삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a)**. 6a·6b·6c 와 2026-09-18 모순 정정 기록은 이력(git)에만 남긴다.

| **SRS-CALIB-FUNC-007** | System shall apply defect pixel correction in three modes selectable via configuration: (a) neighbor averaging (4-neighbor or 8-neighbor), (b) bilinear interpolation from surrounding pixels, (c) median filter from neighborhood. Output shall replace defective pixels with interpolated values. Defect detection shall use Robust Mask Maker (RMM) with lambda=8.0 for runtime detection. | Defects manifest as dead pixels (no signal), hot pixels (excessive signal), and noisy pixels. Three interpolation modes provide trade-offs between speed and quality. RMM is robust to non-Gaussian noise. | Test: Interpolation correctness |
| **SRS-CALIB-FUNC-008** | ~~Temperature compensation of dark current (exponential model, NTC sensor, `xpe_temp_compensate`, config `detectorTempC`)~~ — **삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 a72d59c9)**. 요구 문장은 효력이 없다. | — | — |
| **SRS-CALIB-FUNC-009** | System shall check calibration expiry by comparing file timestamp (expiryEpochMs from header) against current time. If `current_time_ms > expiryEpochMs`, function shall return error `XPE_ERR_CALIBRATION_EXPIRED` and halt pipeline. Expiry date field shall be loaded from calibration file header (offset 8-11, uint32, milliseconds since 2000-01-01). **Default expiry period**: Factory calibration files shall set `expiryEpochMs = created_epoch_ms + 7776000000` (90 days). Zero-value expiry (`expiryEpochMs = 0`) shall be interpreted as "no expiry" and shall log a warning `XPE_WARN_NO_EXPIRY` indicating potential staleness. Configuration override via `calibration.expiry_days` in JSON config shall be supported (minimum: 7 days, maximum: 365 days). | Expired calibration data introduces systematic bias in corrected images. Hard block prevents clinical use of stale calibration. Expiry mechanism enables IEC 62304 traceability and regulatory compliance (21 CFR Part 11). Default 90-day expiry balances detector drift risk against operational burden. Never-expire flag (0) must trigger warning to prevent silent staleness accumulation. | Test: Date comparison logic, default expiry assignment, warning on zero expiry |
| **SRS-CALIB-FUNC-010** | System shall support runtime defect detection via `xpe_defect_detect_runtime()` function to identify new defects not present in static BPM. Detection algorithm: calculate pixel SNR over N=10 consecutive frames; flag pixels with SNR < 5 dB as defects. Runtime map shall be merged with static BPM and logged for QA review. | Clinical systems develop new defects over time (cosmic rays, electrostatic discharge). Runtime detection prevents image artifacts from escaping undetected. | Test: SNR calculation, defect logging |

### 2.3 Session and Binning Management (SRS-CALIB-FUNC-011 through SRS-CALIB-FUNC-012)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-011** | System shall support calibration session management via `xpe_calib_session_create()` function. Session shall be identified by unique session_id (UUID v4). Each session shall include timestamp, detector temperature, source kVp/mAs, binning mode, and associated calibration file versions. Session state shall persist across frame processing until explicit reset via `xpe_ghost_reset()`. | Sessions enable tracking of calibration state across multi-frame acquisitions. Session metadata enables post-processing traceability (PACS integration). Ghost correction requires frame history (see SRS-CALIB-FUNC-012). | Test: Session ID generation, state persistence<br>**상태: 미구현(요구 유지) — 사용자 결정 2026-10-03, #245** (`QA-A-233` 결정 12). `xpe_calib_session_create()` 는 없다. 구현된 것은 세 교정 파일의 `session_id` 일치 검사와 "세션 미지정" 경고(`QA-A-229` M4)이며, UUID 생성·세션 상태 유지는 아니다. |
| **SRS-CALIB-FUNC-012** | System shall support pixel binning mode compensation via `xpe_binning_correct()`. Binning modes shall include 1×1 (native), 2×2, 4×4. Gain correction factor shall be applied as `G_binned(x,y) = G_native(x,y) / (binning_factor^2)` to maintain normalized intensity. Binning correction shall only execute if binningMode ≠ 1 (native resolution). Output bit-depth shall remain float32. | Binning (on-detector pixel summing) reduces noise but requires gain re-normalization. Squared factor accounts for charge summation. Conditional execution prevents unnecessary computation. | Test: Binning factor validation |

### 2.4 Ghost/Lag Correction and Frame History (SRS-CALIB-FUNC-013 through SRS-CALIB-FUNC-014)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-013** | System shall support three-tier ghost/lag correction with auto-escalation: (1) Tier 1: Multi-exponential LTI deconvolution (N=4 exponentials, model: `Lag(t) = Σ(α_i × exp(-t/τ_i))`); (2) Tier 2: Exposure-weighted LTI (accounts for variable exposure); (3) Tier 3: NLCSC (Non-Linear Correlation Signal Correction, Starman 2012, accounts for signal-dependent lag). Escalation shall occur when residual lag after Tier 1 exceeds threshold (10% of signal). Minimum 90% ghost removal shall be achieved. | Lag/ghosting is temporal artifact from charge carrier trapping. Three tiers provide accuracy vs. performance trade-off. Auto-escalation ensures best achievable image quality. NLCSC handles complex detector physics. | Test: Lag residual measurement |
| **SRS-CALIB-FUNC-014** | System shall maintain exposure history ring buffer with minimum 8 frames (configurable up to 16). Frame buffer storage: 8 frames × 3072×3072×4 bytes = ~150 MB. History shall be reset via `xpe_ghost_reset()` after patient/study change or power-on. First frame after reset shall skip ghost correction (no history). Single-shot mode shall also skip ghost correction. | Frame history enables temporal filtering for lag artifact removal. 8-frame buffer is standard for commercial FPD systems (Varex, Vieworks). Ring buffer prevents memory bloat. Reset prevents cross-contamination between acquisitions. | Test: Buffer management, reset logic<br>**상태: 미구현(요구 유지) — 사용자 결정 2026-10-03, #245** (`QA-A-233` 결정 14). 고스트 보정은 화소별 누산기 둘로 동작하며 8~16 프레임 링 버퍼는 없다. |

---

### 2.5 Best-in-Class Preprocessing Reinforcement and E2E Verifiability Addendum (2026-04-16)

The following requirements bind preprocessing algorithm quality to automated evidence defined in `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md`.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-015** | System shall emit a preprocessing E2E metric report for every automated raw/calibration validation run using schema `xpe-pre-e2e-report-v1`. The report shall include raw SHA-256 before/after, calibration file SHA-256, inferred dimensions, applied stages, stage timings, detector-domain metrics, and pass/fail gates. | Preprocessing quality must be reproducible and reviewable without relying on screenshots or subjective visual inspection. | Test: PRE-E2E-0, report schema validation<br>**상태: 미구현(요구 유지) — 사용자 결정 2026-10-03, #245** (`QA-A-233` 결정 14). 스키마 `xpe-pre-e2e-report-v1` 를 내는 코드가 없다. |
| **SRS-CALIB-FUNC-016** | System shall compute detector-domain dark/offset metrics: `DarkBias`, `DSNU_ADU`, `DarkReduction_dB`, and `ClampRate`. Phase 1 acceptance shall require `abs(DarkBias) <= 5 ADU` or `DarkReduction_dB >= 10 dB` on applicable fixtures. | Offset correction is the first safety-critical calibration stage; dark residuals are objective indicators of drift or calibration mismatch. | Test: PRE-E2E-1 synthetic oracle, PRE-E2E-2 real fixture |
| **SRS-CALIB-FUNC-017** | System shall compute gain/flat-field metrics: `PRNU_CV`, `FlatResidualPct`, `FPN_Reduction_dB`, and `LineArtifactScore`. Phase 1 acceptance shall require `FlatResidualPct <= 1.0%` and target `<= 0.5%` for release-hardening fixtures where gain semantics are known. | Flat-field correction must reduce fixed-pattern noise while avoiding row/column artifact amplification. | Test: PRE-E2E-1 synthetic oracle, PRE-E2E-2 real fixture |
| **SRS-CALIB-FUNC-018** | System shall record calibration gain semantics as `normalized_gain`, `reciprocal_gain`, or `unknown`. Unknown semantics may run exploratory validation but shall not pass release gates. | Real legacy calibration files often encode gain differently; silent interpretation errors can create severe over/under-correction. | Test: calibration manifest parser, mismatch negative tests |
| **SRS-CALIB-FUNC-019** | System shall compute defect-correction metrics: `DefectRecall`, `DefectFPR`, `DefectResidualADU`, and `GoodPixelDeltaP99`. Synthetic BPM oracle cases shall require 100% defect recall and false-positive rate below 0.001%. | Defect correction must repair known bad pixels without altering good pixels or suppressing clinically relevant structures. | Test: PRE-E2E-1 defect synthetic oracle |
| **SRS-CALIB-FUNC-020** | System shall compute lag/ghost metrics: `LagResidualPct`, `GhostRemovalPct`, history length, tier selection, and bypass reason. Benchmark sequences with measurable lag shall require at least 90% ghost removal. | Lag and ghost correction is stateful; tier and bypass traceability is required to prevent hidden temporal contamination. | Test: PRE-E2E-1 synthetic lag, PRE-E2E-2 real lag fixtures when available |
| **SRS-CALIB-FUNC-021** | System shall compute a Calibration Effect Score (CES) from dark, flat, defect, ~~nonlinearity~~ (FUNC-006 삭제, 사용자 결정 2026-11-16), lag/ghost, reference, preservation, and performance subscores. Phase 1 implementation completeness shall require `CES >= 85`; release-hardening target shall be `CES >= 92`. Any raw preservation failure, NaN/Inf output, or unknown gain semantics shall cap the score as defined by XPE-PRE-E2E-001. | A single dashboard score helps sprint tracking, but blocking safety gates prevent high aggregate scores from hiding critical defects. | Test: PRE-E2E report score calculation<br>**상태: 미구현(요구 유지) — 사용자 결정 2026-10-03, #245** (`QA-A-233` 결정 14). CES 계산 코드가 없다. |

---

### 2.6 BPM 생성 알고리즘 상세 (2026-04-19 종래기술 교차검증 결과, 2026-10-16 결함 화소 맵 2판으로 개정)

다음 요구사항은 `docs/calibration/PRIOR-ART-BPM-ALGORITHM.md`의 MC/Blue 알고리즘 비교 분석 및 `tests/test_data/Grid_abnormal/` 데이터셋의 실제 검증 결과를 기반으로 추가되었습니다. 이는 기존 SRS-CALIB-FUNC-007 (BPM)의 구현 상세를 명시화합니다.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-022** | **[개정 2026-10-16, 결함 화소 맵 2판]** 결함 화소 맵 생성은 단일 고정 임계값이 아니라 화소별 특성 여러 개를 각각 검사하고, 하나라도 기준을 넘으면 결함으로 표시하는 합집합 방식이어야 합니다. 특성은 최소한 (1) 다크 평균의 국소 대비 편차, (2) 다크 시간 잡음(화소별 표준편차, 프레임별 전체·행 성분을 뺀 뒤), (3) 선량별 응답의 기울기와 직선성 잔차, (4) 평탄 영상의 국소 대비 편차를 포함합니다. 편차는 강건 통계(중앙값·MAD)로 계산합니다. 임계값은 고정 상수가 아니라 목표 오경보 수(화소 수 대비)에서 정하고, 그 값과 근거를 manifest 에 기록합니다. 다크는 안정화 전 첫 프레임을 쓰지 않습니다. **[2026-10-16 사용자 결정, 잠정]** 응답(감도) 결함: 오프셋만 보정한 밝은 영상에서 화소 응답이 국소 중앙값의 45 % 미만 또는 150 % 초과이면 결함으로 표시합니다. 그 안쪽은 결함 맵에 넣지 않고 gain 보정에 맡깁니다(45~70 % 는 "약한 응답"으로 진단 기록만). **[2026-10-16 리더 결정, 공동 조사 P2·P3 — 위 사용자 결정의 범위는 응답 비율 45 %·150 % 뿐]** 잡음 비용 결함은 응답 비율로 추정하지 않고, gain 보정 후 같은 선량 프레임에서 실측한 화소 시간 분산이 국소 기대의 2배(3.01 dB) 이상일 때로 합니다. 결함 맵에는 같은 위치에서 프레임마다 지속되는 이상만 넣습니다(움직이는 줄 잡음은 대상 아님). | 표준은 결함의 공통 정의를 두지 않습니다(EMVA 1288 R4.0 §4.4; AAPM TG-150 §4.3.8: 제조사와 무관한 신뢰할 만한 검출법 없음). 특성별 합집합 판정은 PMC9721322(RMM)의 방식입니다. 고정 λ=8 은 화소 9.4M 에서 기대 오경보 1개에 해당하는 λ≈5.3 보다 높아 약한 결함을 놓칩니다(계산). 이전 판(32×32 MAD λ=8 + 128×128 평균 ±7 %, 사내 보고서의 Blue 알고리즘)은 비교 기준으로만 남깁니다. 45 %·150 % 는 공개된 유일한 정량 기준인 PerkinElmer XRD 1620/1621 매뉴얼 §5.2.4.1 에서 온 잠정값이며, 추가 촬영 데이터로 패널 적합성을 확인합니다(공동 조사 종합 `.moai/project/bpm-v2-collab-synthesis.md`). | Test: 결함 주입 평가(위치를 아는 결함을 실제 영상에 심어 검출률·오경보 측정), 프레임 절반 분할 일치도. 임계 목표값과 합격 수치는 주입 실험으로 확정(잠정) |
| **SRS-CALIB-FUNC-023** | **[개정 2026-10-16, 결함 화소 맵 2판]** 결함 화소 맵 생성은 가로(행)·세로(열) 라인 결함을 별도 범주로 검출해야 합니다. 범위: 라인 전체, 라인 일부 구간, 인접한 두 줄, 교차하는 행·열. 판정은 offset·gain 보정을 적용한 뒤에도 남는 행·열 성분으로 하며, 생성에 쓰지 않은 프레임에서 확인합니다. offset·gain 보정으로 사라지는 정상 띠 무늬는 결함으로 표시하지 않습니다. 결함 덩어리(인접 결함 화소 묶음)는 크기와 함께 구분합니다. 라인 결함이 3줄 이상 연속이면 보정 대상이 아니라 패널 불합격 조건으로 보고합니다. | 행·열 프로파일과 이웃 차이 D_n 을 상관 없는 잡음 예측치와 비교하는 방법은 AAPM TG-150 §4.3.9 에 있습니다(임상 허용 한계는 없음). EMVA 1288 §4.3 은 공간 분산을 행·열·화소 성분으로 나눕니다. 3줄 이상 연속 라인은 보간이 어렵다는 기술이 Hamamatsu EP2315434 에, 두 줄 연속 처리는 Philips US6747697 에 있습니다. | Test: 라인 결함 주입(전체·일부·두 줄·교차 + 결함이 아닌 띠 무늬 대조군), 대조군 오검출 0 |
| **SRS-CALIB-FUNC-024** | 다중 단계 게인 보정(Multi-step Gain Calibration)에서, 단일 선량 조건에서 수집하는 평탄화 프레임의 최소 개수를 명시합니다. **수용 기준**(Frame-count Tier): (a) **최소**: 1 프레임 (노이즈 모델링 필수 — Poisson+readout noise 분산을 메타데이터에 명시); (b) **표준**: 5~10 프레임 (신호 안정화, 평균화로 DSNU 감소); (c) **권고**: 15~20 프레임 (최상의 FPN 제거); (d) **우수 품질**: 25~30 프레임. 단일 프레임 모드에서는 `XPE_FLAG_SINGLE_FRAME_GAIN` 메타데이터 플래그를 설정하고, 생성된 gain 맵의 불확실성(uncertainty)을 `XpeCalibMetadata.gain_uncertainty` 필드에 기록해야 합니다. | 실사용환경에서는 시간 제약으로 인해 다중 프레임 취득이 불가능한 경우가 빈번합니다 (Emergency 재캘리브레이션, Field 환경). 단일 프레임이라도 오프셋 보정 후 gain 정보가 없는 것보다 낫습니다. 다만, 노이즈 불확실성을 downstream에 전달하여 임상 판단에 활용할 수 있도록 해야 합니다. CalData_6 데이터셋(6개 선량 레벨, 각 1개 프레임)은 단일 프레임 모드의 검증 데이터로 활용합니다. | Test: CalData_6 단일 프레임 모드 검증, uncertainty 필드 확인, 다중 프레임 대비 FPN 차이 < 2% 검증 |
| **SRS-CALIB-FUNC-025** | BPM 생성 알고리즘은 그리드 아티팩트(Anti-Scatter Grid Artifact) 환경에서도 견고해야 합니다. 전처리(BPM 보정 적용) 후 이미지의 라인 아티팩트 점수(LineArtifactScore, 중주파 에너지 비율)가 10% 미만이어야 합니다. 측정 방법: 2D FFT 후 중주파 대역(0.05~0.3 cycles/pixel) 에너지 비율 계산. **[개정 2026-10-16]** 이전 판의 "Blue 알고리즘 적용 시 5 % 목표" 문구는 결함 화소 맵 2판(FUNC-022·023) 도입으로 삭제했습니다. | 그리드 아티팩트는 의료 X-ray 촬영 환경에서 표준이며, BPM 알고리즘이 실제 그리드 환경에서도 아티팩트를 남기지 않아야 합니다. | Test: Grid_abnormal 데이터셋 FFT 분석, LineArtifactScore < 10 % |
| **SRS-CALIB-FUNC-039** | **[신설 2026-10-16]** 결함 화소 맵은 화소별 결함 코드(FUNC-003 과 호환) 외에 라인 결함 기록(방향, 행·열 번호, 시작·끝, 종류, 보정 방법)과 결함 덩어리 표시를 담아야 합니다. 결함 보정은 라인 결함을 라인에 수직인 방향의 이웃으로 보간하고, 두 줄 연속 결함은 양쪽의 정상 라인으로 보간하며, 쓸 수 있는 이웃이 없는 덩어리는 "신뢰 낮음"으로 표시합니다. | 화소 단위 표시만으로는 라인 결함의 방향을 보정 단계가 알 수 없습니다. 방향 적응 보간은 Agfa US10726527, GE US7068854 에, 보정 화소의 잡음 질감이 눈에 띌 수 있다는 기술은 GE US6498831 에 있습니다. | Test: 라인 주입 영상의 보정 오차(국소 잡음 대비), 보정 후 D_n, 앱 적재 호환 |

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
| **SRS-CALIB-FUNC-027** | ~~Multi-point (multi-dose) gain polynomial fitting via `xpe_calib_generate_gain_polynomial()`, `XCAL_TYPE_GAIN_POLY`~~ — **삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a)**. 요구 문장은 효력이 없다. 다항식 게인 파일(.xcal 형식 3)은 적재 시 명시 오류(`XPE_ERR_CONFIG_INVALID`)로 거부된다. 2026-09-18 변수 정정 기록은 이력(git)에만 남긴다. | — | — |
| **SRS-CALIB-FUNC-028** | System shall support field calibration workflow via `xpe_calib_field_generate()`. This function combines offset and gain generation for field use with simplified acquisition: (1) Acquire N_dark ≥ 5 dark frames (single temperature, current PREP time); (2) Acquire N_flat ≥ 1 flat-field frames at clinical kVp (not restricted to RQA-5); (3) Generate offset map using mean averaging (same algorithm as `xpe_calib_generate_offset`); (4) Generate gain map using FUNC-026 algorithm; (5) Tag both files with `source: "field"`, `detector_temp: <current>`, `acquisition_duration: <measured>`; (6) Set expiry to configurable field period (default 30 days, shorter than factory 90 days). Field calibration shall log comparison metrics against previously loaded calibration: DarkBias delta, PRNU delta, defect count delta. | Field calibration occurs in clinical environments where full factory protocol is impractical. Simplified protocol (5+ dark, 1+ flat) enables periodic recalibration during maintenance windows. Shorter expiry (30 days) reflects lower statistical quality of field-acquired maps. Comparison logging enables drift detection over time. | Test: Field simulation with CalData_6 subset, comparison metrics output, expiry assignment<br>**상태: 미구현(요구 유지) — 사용자 결정 2026-10-03, #245** (`QA-A-233` 결정 14). `xpe_calib_field_generate()` 가 없다. |
| **SRS-CALIB-FUNC-029** | System shall support calibration drift detection via `xpe_calib_check_drift()`. When invoked with a new set of dark/flat frames (minimum 5 dark, 1 flat), system shall: (1) Compute quick offset statistics (mean, σ); (2) Compare against loaded offset map statistics; (3) Compute quick gain statistics (PRNU); (4) Compare against loaded gain map statistics; (5) Return drift report with metrics: DarkBiasDriftPct, PRNUChangePct, DefectCountDelta. If `DarkBiasDriftPct > 10%` or `PRNUChangePct > 5%` or `DefectCountDelta > 50`, system shall recommend recalibration via `XPE_WARN_RECALIBRATION_RECOMMENDED`. | FPD characteristics drift over time (dark current aging, new defect formation). Without drift detection, stale calibration accumulates systematic errors. Quick-check protocol (5 dark + 1 flat, <30 seconds acquisition) enables routine QA without full recalibration. Threshold values based on IEC 62220-1-1 DQE degradation sensitivity. | Test: Simulated drift injection, threshold trigger verification, false-positive rate<br>**상태: 미구현(요구 유지) — 사용자 결정 2026-10-03, #245** (`QA-A-233` 결정 14). `xpe_calib_check_drift()` 가 없다. |
| **SRS-CALIB-FUNC-030** | ~~Real-time offset adaptation for temperature changes (`calibration.realtime_offset_adapt`, NTC 온도 메타데이터, 지수 모델, 온도 범위 밖 경고)~~ — **삭제 (사용자 결정 2026-10-15 단일 오프셋 맵, 2026-11-16 온도 보정 제거)**. 요구 문장은 효력이 없다. 미구현 상태였고(#245) 구현하지 않는다. | — | — |

### 2.8 Calibration Mode Selection and Optimization (2026-04-24 v1.2)

The following requirements define calibration mode selection and quality metadata recording for gain calibration. 2026-11-16 사용자 결정으로 다선량 다항식 게인(FUNC-027)이 삭제되어, 모드는 단일점(SINGLE_POINT) 하나만 남는다.

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-FUNC-031** | **Calibration Mode Selection API (단일점 전용, 2026-11-16 축소).** `XpeCalibrationMode` 열거형의 값은 `XPE_CALIB_SINGLE_POINT` (0, 1 dose level, 상수 게인) 하나다. (1) `xpe_calib_set_mode(mode)` shall accept `XPE_CALIB_SINGLE_POINT` and shall return `XPE_ERR_INVALID_INPUT` for any other value, leaving the mode unchanged. (2) `xpe_calib_get_mode()` shall return `XPE_CALIB_SINGLE_POINT`. (3) The gain generator (`xpe_calib_generate_gain()`, FUNC-026) shall record `calibration_mode` 0 in the generated XCal file. (4) The gain loader shall accept recorded `calibration_mode` values 0..4 in single-map gain files (older files; the value is a record only). (5) A polynomial gain file (.xcal type 3) shall be refused with an explicit error. | 다선량 다항식 게인과 그 모드(DUAL_POINT, MULTI_POINT_5/8/10, AUTO)는 사용자 결정 2026-11-16 으로 삭제됐다(근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a). 그 번호 1~5 는 재사용하지 않는다. `xpe_calib_get_max_points` · `xpe_calib_get_poly_degree` 도 삭제됐다. | Test: set_mode 단일점 수락·그 밖의 값 거부, 생성 파일 `calibration_mode` 0, 옛 기록값 0..4 적재, 형식 3 파일 거부 |

> **2026-09-16 열거형 표기 정정(QA-A-77/78)과 AUTO 미구현(#169) 기록은 삭제된 다점 모드에 관한 것이라 이력(git)에만 남긴다.**
| **SRS-CALIB-FUNC-032** | ~~Multi-Point Calibration Performance Optimization (online accumulative fitting, SIMD polynomial fitting, degree reduction, 10-point cap, pre-computed LUT)~~ — **삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a)** (FUNC-027 삭제에 따라 대상이 없어짐). 요구 문장은 효력이 없다. | — | — |
| **SRS-CALIB-FUNC-033** | **Calibration Quality Metadata Recording.** System shall record comprehensive quality metadata for all calibration modes. (1) **Mandatory Metadata Fields**: Every generated XCal gain file shall include: `calibration_mode` (XpeCalibrationMode enum value as string), `actual_dose_levels` (number of dose levels used), `polynomial_degree` (fitted polynomial degree; 0 for single-point), `fit_r_squared` (coefficient of determination; 1.0 for single-point by definition), `max_residual_pct` (maximum fitting residual as percentage of full scale), `mean_residual_pct` (mean fitting residual as percentage), `acquisition_duration_s` (total acquisition time in seconds), `detector_temperature_c` (detector temperature during calibration). (2) **Quality Gate**: If `fit_r_squared < 0.999` after fitting, system shall log `XPE_WARN_CALIB_POOR_FIT` and include recommendation to increase dose levels or check detector stability. *(Implementation note, 2026-09-10 / QA-A-35: no `XPE_WARN_*` code exists in `xpe_error.h`; the warning is emitted through the alert queue (`xpe_alert_push`, severity WARNING) with the identifier `XPE_WARN_CALIB_POOR_FIT` as message prefix, plus the residual figure. Gate constant `XPE_CALIB_R_SQUARED_GATE = 0.999`.)* (3) **Comparison with Previous**: When overwriting an existing calibration file, system shall compute and log: `dark_bias_delta` (change in dark bias vs previous), `prnu_delta_pct` (change in PRNU vs previous), `defect_count_delta` (change in defect count vs previous). (4) **Mode-Specific Metadata**: SINGLE_POINT shall additionally record `gain_uncertainty` (estimated from single-frame noise model); ~~MULTI_POINT shall additionally record `per_point_r_squared[]` array~~ (다점 모드 삭제, 사용자 결정 2026-11-16). (5) All metadata shall be stored in XCal file header section (JSON-encoded in reserved header bytes). *(Layout note, 2026-09-17 / QA-A-86: `XCalFileHeader` (`xcal_format.h:100-112`) has **no reserved bytes**. The metadata is written as a JSON block **immediately after** the fixed header, whose length is carried in `config_json_len`, and it is covered by the header's `sha256`. The requirement's intent — metadata travels inside the XCal file, integrity-protected — is met; the phrase "reserved header bytes" does not describe the shipped layout. Also: on the polynomial path the file's `polynomial_degree` records the requested cap `max_degree` while the in-memory quality struct records the actually fitted `highest_degree` — the SRS says "fitted", so the file value is the one that disagrees; tracked in #140.)* *(Degree note superseded, 2026-09-19 / QA-A-134: the mismatch above is resolved. Both the file (`:683`) and the in-memory struct now record `highest_degree`, and the requested cap has its own key `max_polynomial_degree` (`:684`). The 2026-09-17 sentence is kept above rather than deleted so the record of what was wrong survives — but it no longer describes the shipped format. What remains open on #140 is (1) `acquisition_duration_s` / `detector_temperature_c` (neither value reaches the generator through its current signature), (3) the previous-calibration deltas, and (4) `gain_uncertainty` / `per_point_r_squared[]`. Items (1)-(5) of this requirement are otherwise wired and tested — `test_calib_quality_meta.cpp`, 15 cases.)* *(Disposition, 2026-09-26 / QA-A-141, `#140` 종결: 남아 있던 셋의 처분을 확정한다. **(1) `acquisition_duration_s` · `detector_temperature_c` — 호출자 몫.** 두 진입점 시그니처 어디에도 없고 다항 경로에는 `metadata_json` 인자조차 없다. 생성 함수가 자기 실행 시간을 `acquisition_duration_s` 에 넣으면 **이름과 다른 값을 그 자리에 넣는 일**이므로 하지 않는다. 시그니처 확장은 그 값을 실제로 쓰는 소비자가 생길 때. **(3) 이전 교정 비교 셋 — 층이 다르므로 이 요구에서 분리한다.** `dark_bias_delta` 는 오프셋 파일, `defect_count_delta` 는 결함 맵의 양인데 게인 생성 경로에 그 입력이 없고, **이전 교정 파일을 읽는 기구 자체가 저장소에 없다.** 가장 그럴듯한 반론인 `previous_r_squared` 를 확인했는데 `xpe_calib_mode.cpp:254` 의 **프로세스 메모리**(직전 생성 호출값)이지 파일이 아니다. 따라서 (3)은 게인 생성 함수의 미충족 항목이 아니라 **세 산출물을 모두 가진 교정 조율 층의 요구**다. 그 층은 아직 없으므로, 층이 생길 때 그쪽 SRS 로 옮긴다 — 여기 남겨 두면 본문만 읽는 사람이 계속 미구현으로 읽는다. **(4) `gain_uncertainty` · `per_point_r_squared[]` — 정의 부재로 미정의 유지.** 특히 후자는 한 도스 점에 표본이 하나라 R² 가 수학적으로 정의되지 않는다. 정의는 편집 정정이 아니라 제품 판단이므로 여기서 만들지 않는다. **(2) R² 게이트는 배선돼 있다** — 메타 기록을 막으면 15건 중 7건이 빨개지는 것으로 확인했다(장식이 아님). 셋 다 "구현하지 않았다"가 아니라 **"이 자리에서는 할 수 없다"**이고, 그 구분이 다음 사람이 같은 조사를 반복하지 않게 한다.)* | Quality metadata enables: (a) automated QA pass/fail decisions, (b) drift tracking across calibration sessions, (c) regulatory traceability (IEC 62304), (d) mode comparison studies. R² < 0.999 threshold aligns with REQ-NLN-005 in PRD. | Test: Metadata presence validation, R² gate test, comparison metrics test, XCal header parsing test |

**Mode-to-Parameter Mapping (2026-11-16 이후):**

| Mode | Max Dose Levels | Max Poly Degree | Typical Use Case |
|------|:--------------:|:--------------:|------------------|
| SINGLE_POINT (0) | 1 | 0 (constant) | 공장·현장 게인 교정 |

DUAL_POINT · MULTI_POINT_5 · MULTI_POINT_8 · MULTI_POINT_10 · AUTO 행은 삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a). 2026-09-16 열 뒤바뀜 정정 기록은 이력(git)에만 남긴다.

**Relationship to Existing Requirements:**

- **FUNC-031 vs FUNC-024**: FUNC-024 defines frame count tiers for the single dose level; FUNC-031 fixes the dose level count at 1.
- **FUNC-033 vs FUNC-015**: FUNC-015 defines E2E metric reporting; FUNC-033 defines calibration-specific quality metadata in XCal files. FUNC-033 metadata is consumed by FUNC-015 reporting during validation runs.

### FUNC-033 구현 현황 — 무엇이 되고 무엇이 왜 안 되는지 (2026-09-16, leader / QA-A-70)

> **2026-11-16 이후**: 다항식 경로는 삭제 (사용자 결정 2026-11-16, 근거: `.moai/reports/lead/nonlin-need-20261116/report.md`, 엔진 커밋 2d9576de·d3216b2a). 아래 기록의 "다항 경로" 언급과 `per_point_r_squared[]`(다점 모드 전용)는 더 이상 해당하지 않습니다. 생성 파일은 `calibration_mode` 0, `polynomial_degree` 0 을 기록합니다.

**요구가 구현도 검증도 없다고 적혔던 상태는 해소돼 있었습니다.** QA-A-35 가 두 생성 경로 모두에 배선했고(`xpe_calib_generate_gain.cpp:301, :563`), 파일에서 다시 읽으며(`xpe_calib_load_gain.cpp:108`), R² 는 상수가 아니라 잔차에서 계산됩니다. 검증도 있습니다.

**(1) 필수 필드 8개 중 6개 기록, 2개 미기록.**

| 필드 | 상태 |
|---|---|
| `calibration_mode` · `actual_dose_levels` · `polynomial_degree` · `fit_r_squared` · `max_residual_pct` · `mean_residual_pct` | 기록됨 |
| **`acquisition_duration_s`** · **`detector_temperature_c`** | **미기록** |

**미기록 둘은 구현 누락이 아니라 이 함수가 알 수 없는 값입니다.** 취득은 이 모듈 밖에서 일어나고 온도는 하드웨어에서 옵니다 — 두 진입점 시그니처에 도달하지 않으며, 다항 경로에는 `metadata_json` 인자조차 없습니다.

> **판정: 이 둘은 호출자가 채우는 필드입니다.** 생성 함수가 자기 실행 시간을 `acquisition_duration_s` 에 넣는 것은 **이름과 다른 값을 그 자리에 넣는 일**이고, 그러면 그 필드를 읽는 다음 사람이 취득 시간으로 읽습니다. QA-A-70 이 그렇게 하지 않고 올린 것이 옳습니다. 시그니처를 넓히는 것은 공개 API 변경이므로 **그 값을 실제로 쓰는 소비자가 생길 때** 합니다(QA-A-61 의 ABI 판정과 같은 기준).

**(3) 비교 지표 셋 — 전부 없고, 이 경로에 있을 수 없습니다.**

`dark_bias_delta` 는 오프셋 파일의 양이고 `defect_count_delta` 는 결함 맵의 양이라 **게인 생성 경로에 그 입력이 없습니다.** `prnu_delta_pct` 는 게인 맵에서 계산 가능하지만 **다항 경로의 출력은 게인 맵이 아니라 계수 배열**입니다.

> **판정: 이 셋은 세 산출물을 모두 가진 층에서 계산해야 합니다.** 게인 생성 함수가 아니라 교정 전체를 조율하는 쪽입니다. 그 층이 아직 없으므로 **지금은 미구현으로 기록하고, 요구를 그 층에 재배치하는 것이 맞습니다.** 구조체의 `previous_r_squared` 는 **위 셋 어디에도 없는 필드**이고 요구 목록 밖입니다.

**(4) 모드별 둘 — 정본에 정의가 없습니다.**

`gain_uncertainty` 는 **어떤 잡음 모델로 어떤 단위**인지 이 문서에 없습니다. `per_point_r_squared[]` 는 더 근본적입니다 — **한 도스 점에 표본이 하나라 R² 가 수학적으로 정의되지 않습니다.** 화소 방향으로 정의하면 수는 나오지만 **이 문서가 그렇게 말하지 않습니다.**

> **판정: 정의를 여기서 만들지 않습니다.** 임상 품질 지표의 정의는 편집 정정이 아니라 제품 판단이고, 근거 없이 채우면 **요구가 구현을 따라가는** 형태가 됩니다. **미정의로 기록하고, 정의가 오면 구현합니다.** QA-A-70 이 해석하지 않고 올린 것이 옳습니다.

**요약: (1)의 둘은 호출자 몫, (3)은 층이 다름, (4)는 정의 부재.** 셋 다 "구현하지 않았다" 가 아니라 **"이 자리에서는 할 수 없다"** 이고, 그 구분이 다음 사람이 같은 조사를 반복하지 않게 합니다.

---

## 3. Safety Requirements

### 3.1 Mandatory Correction Policy (SRS-CALIB-SAFE-001 through SRS-CALIB-SAFE-003)

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SRS-CALIB-SAFE-001** | Offset correction (SRS-CALIB-FUNC-004) and gain correction (SRS-CALIB-FUNC-005) shall be mandatory by default. System shall return hard error `XPE_ERR_NOT_INITIALIZED` if either offsetMap or gainMap is absent at pipeline start. An explicit bypass (`bypassOffset` / `bypassGain`) is permitted for research and diagnostics only; when used, the system shall set a warning flag and log the bypass so the output is identifiable as uncorrected. [Changed 2026-10-05, user decision "allow with warning and record" — previously "non-bypassable"; the bypass existed and was used silently, QA-A-240.] Defect, ghost, and binning corrections are optional and conditional (nonlinearity and temperature corrections 삭제, 사용자 결정 2026-11-16). | Dark current bias and pixel gain variation are inherent detector artifacts present in all frames. Uncorrected images contain systematic errors that compromise diagnostic accuracy. Mandatory flag prevents accidental misconfiguration. | Test: Bypass prevention, error codes |
| **SRS-CALIB-SAFE-002** | System shall enforce calibration expiry checking (SRS-CALIB-FUNC-009). Pipeline shall abort image acquisition if any loaded calibration file has expired (current_time_ms > expiryEpochMs). Error code `XPE_ERR_CALIBRATION_EXPIRED` shall be returned and propagated to user interface. | Expired calibration introduces known systematic bias into diagnostic images. Hard enforcement prevents clinical use of stale data and ensures regulatory compliance (21 CFR Part 11, IEC 62304). | Test: Expiry validation |
| **SRS-CALIB-SAFE-003** | All calibration file loads (offset, gain, BPM) shall validate file integrity via CRC-32 checksum. Corrupted files shall be rejected with error `XPE_ERR_IO_FAILED`. No partial corrections shall be applied. Pipeline shall fail atomically: either all calibration files load successfully, or none are loaded. | CRC-32 prevents silent data corruption that could lead to systematic image bias. Atomic behavior ensures consistency: either fully calibrated or raw pass-through. | Test: Corruption detection |

**Test GUI evaluation exception:** `ImageProcTest.exe` may expose `Off`, `On`, and `Auto` controls for each calibration/preprocessing stage to support algorithm effect and performance evaluation. This exception is limited to QA/Test GUI workflows, shall be labelled evaluation-only, shall not relax the product-mode mandatory offset/gain policy, and shall record every bypass or forced-stage decision in the automation/evidence report.

### 3.2 Data Integrity and Buffer Protection (SRS-CALIB-SAFE-004 through SRS-CALIB-SAFE-005)

| Req ID | Requirement | Hazard Ref | Rationale | Verification |
|--------|------------|-----------|-----------|--------------|
| **SRS-CALIB-SAFE-004** | Calibration correction on the shipping and diagnostic path shall never modify the input image buffer: the operator application and every shipping caller shall use the input-preserving entry point (`xpe_preprocess_pipeline_out`, separate output buffer; overlapping input/output ranges are rejected). The legacy in-place entry points (`xpe_preprocess_pipeline`, `_ex`, `_batch`) are permitted only where the caller explicitly chooses in-place processing (tools, tests, benchmarks) and shall not be used on the shipping path. Original uint16 input data shall remain accessible for diagnostic/audit purposes. [Changed 2026-10-05, user decision — previously an unconditional "never"; the legacy entry points write into the caller buffer by public contract, QA-A-241 / Codex #157.] | In-place modification risks data loss and prevents audit trail reconstruction. Preservation of original enables forensic analysis and QA verification. | Test: Input buffer preservation |
| **SRS-CALIB-SAFE-005** | Gain correction float32 output shall be bounds-checked to prevent overflow/underflow. Maximum pixel value after gain correction shall be capped at 3.4e38 (float32 max exponent). Pixels exceeding this shall be clamped to 3.4e38 with warning log. Minimum value shall be 0.0 (no negative corrected values). | Float32 overflow produces infinity/NaN which corrupt downstream processing. Clipping is preferable to NaN. Bounds checking prevents silent data corruption. | Test: Overflow protection |

---

## 4. Performance Requirements

### 4.1 Processing Speed (SRS-CALIB-PERF-001)

| Req ID | Requirement | Rationale | Verification |
|--------|------------|-----------|--------------|
| **SRS-CALIB-PERF-001** | Total preprocessing pipeline execution time shall not exceed 500 ms per 3072×3072 float32 frame on Intel Core i7 or equivalent processor. Breakdown: (0) CalibManager load ≤200 ms (one-time startup), (1) Offset ≤55 ms, ~~(1.5) Nonlinearity ≤20 ms~~ (FUNC-006 삭제, 사용자 결정 2026-11-16), (2) Gain ≤55 ms, (2.5) Binning ≤10 ms, (3) Defect ≤95 ms, (4) Ghost Tier 1 ≤140 ms, (4) Ghost Tiers 2-3 ≤+130 ms. | Clinical workflow requires <1 second per frame (including enhancement). Detailed budgets prevent bottleneck phases from exceeding hard limit. Timer instrumentation shall log per-phase duration. | Test: Profiling on reference hardware |

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
| **SRS-CALIB-IF-003** | Detector metadata | `XpeDetectorProfile` struct | JSON or binary struct (temperature, kVp, SID, binning mode) | Required for gain selection (온도 보상은 FUNC-008 삭제로 해당 없음, 사용자 결정 2026-11-16) |
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
// (삭제 2026-11-16: xpe_nonlinearity_correct, xpe_temp_compensate — 사용자 결정, 엔진 커밋 a72d59c9·2d9576de)
XpeErrorCode xpe_defect_correct(XpeImageBuffer* image);
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

## Codex Requirements Addendum - 2026-04-28

Scope: `feat/preprocessing`, Issues #68, #69, #70.

| Req ID | Requirement | Rationale | Verification |
|--------|-------------|-----------|--------------|
| **SRS-CALIB-FUNC-034** | Offset calibration generation shall support deterministic `mean`, `median`, `sigma_clip`, and `winsor` methods from a stack of UINT16 dark frames. The default method shall remain `mean` for the stable C file-writing API. Generated XCal metadata shall record `method`, `integration_time_ms`, and `temperature_c` when available. Invalid frame counts, mismatched dimensions, unsupported formats, or too-small output buffers shall return an error instead of producing partial output. | Field and factory calibration workflows require robust dark-map generation under outliers while preserving backward compatibility with the existing mean-based API. | Unit: `test_calib_generate_offset_multi.cpp`; Integration: preprocessing ctest 341/341 passed on 2026-04-28. |
| **SRS-CALIB-FUNC-035** | **Dark-ROI selection shall be explicit and its failure shall be visible.** `Y_dark_roi` (`FUNC-016`) shall be selected by a stated, reproducible rule, and the rule shall be recorded alongside every `DarkBias` / `DSNU_ADU` value it produced. When the selection yields fewer than `dark_roi_min_pixels` candidates, the system shall NOT silently substitute a different population: it shall report the metric as unmeasurable per `FUNC-036`. Phase 1 rule: the darkest decile of the raw frame (`raw < percentile10(raw)`), `dark_roi_min_pixels = 1% of pixel_count`. | A dark residual measured over the whole frame is a different quantity from one measured over dark pixels, and the gate in `FUNC-016` is written for the latter. A relative decile on a bright frame selects bright pixels, so the rule must be stated rather than inferred from a variable name. Observed 2026-09-28 (`#221`): the empty-candidate path substituted the full frame with no signal, and `DarkBias` was reported as 1000 and 1599.6 ADU. | Test: unit — frame with zero dark candidates reports unmeasurable, not a full-frame mean; control — frame with dark candidates reports the decile mean |
| **SRS-CALIB-FUNC-036** | **Verification metrics shall distinguish "failed" from "could not be measured."** Every `xpe_verify_*` function shall report, separately from its pass/fail result, whether each metric was actually measured. A metric that could not be measured shall never be reported as a measured value, and shall not on its own produce a pass. Until a dedicated status field exists, an unmeasurable metric shall set `overall_pass = false` on every verification path. | A single boolean cannot carry three states (passed / failed / unmeasurable), and collapsing the third into "passed" lets an unverified frame through. Observed 2026-09-28 (`#219`): `xpe_verify_offset` reported `overall_pass = true` for a uniform frame while `xpe_verify_gain` reported `false` for the same situation, and a test named for correction quality never exercised it. | Test: unit — each verification path on an unmeasurable input yields `overall_pass = false`; control — the same path on a measurable input yields a measured value |
| **SRS-CALIB-FUNC-037** | **The verification metrics structure shall carry every value its gates decide on, under the canonical name, plus a per-metric measured/unmeasured status.** `XpeCalibrationMetrics` shall be extended once, not three times, to close the three gaps that share it: (i) `DarkReduction_dB` (`FUNC-016`) shall be reported, not computed and discarded; (ii) the canonical `DSNU_ADU = std(Y_dark_roi)` (ADU) shall be reachable under a name that says so — the existing `dsnu` field holds a coefficient of variation in percent, and the canonical quantity is what `residual_noise` already holds; (iii) the per-metric status required by `FUNC-036` shall be a field, replacing that requirement's interim `overall_pass = false` contract. Existing field names and offsets shall be preserved and new fields appended; a field whose name no longer describes its content shall be documented in place rather than repurposed. | Three separate ABI changes to one structure means three rounds of re-verification against every caller, and each round is where a silently-repurposed field is introduced. `#222` measured the cost of the naming gap directly: `dsnu` is a coefficient of variation, which diverges as offset correction improves, so gating it rejected a frame that met the requirement with tenfold margin. A name that promises the canonical quantity while holding another is what invited that gate. | Test: ABI — every pre-existing field keeps its offset (static assertion on struct layout); unit — `DarkReduction_dB` reaches the caller with the value the gate used; unit — an unmeasurable metric is reported as unmeasured rather than as a value |
| **SRS-CALIB-FUNC-038** | **Management-surface APIs shall carry a lifetime and error contract, traced as one requirement rather than one each.** The exported functions that administer state rather than deliver a calibration capability — cache control (`xpe_calib_cache_clear`, `xpe_calib_cache_set_max_size`), configuration getters (`xpe_calib_get_quality_meta`; `xpe_calib_get_max_points` · `xpe_calib_get_poly_degree` 는 2026-11-16 삭제), lifetime (`xpe_calib_state_release`), initialization query (`xpe_preprocess_is_initialized`), and batch dispatch (`xpe_preprocess_pipeline_batch`) — shall each state, in the header at the declaration: who owns any returned memory, whether the call is valid before `xpe_preprocess_init`, and which `XpeErrorCode` values it can return. A getter shall not report a value it did not read. | Writing one requirement per management function makes the requirement set a copy of the API listing, which adds traceability rows without adding a testable claim. What these functions actually share is a contract about state and ownership, and that contract is what a caller gets wrong. Recorded 2026-09-29 (`#216`): eight exported functions were named by no requirement, and all eight are of this kind. | Test: header review — each named function declares ownership, pre-init validity, and error codes; unit — calling each before `xpe_preprocess_init` returns the declared code rather than undefined behaviour |
| **SRS-CALIB-NFR-003-CACHE** | Calibration LRU cache implementations shall guard all cache list and index mutations with a mutex and shall not expose unsynchronized shared state across concurrent load/read paths. | Calibration state is shared process infrastructure in an IEC 62304 Class B preprocessing component; data races can corrupt calibration selection and break determinism. | Code review: `CalibrationLRUCache`; Regression: preprocessing ctest 341/341 passed on 2026-04-28. |
| **SRS-CALIB-NFR-004-SIMD** | SIMD correction paths shall remain numerically equivalent to scalar correction paths within the documented tolerance for offset, gain, defect, and runtime-detection functions. Runtime dispatch shall validate CPU and OS SIMD state before selecting AVX2. | Medical image preprocessing must not trade deterministic clinical output for speed, and runtime dispatch must be safe on systems where CPU feature bits and OS register support differ. | Unit: Offset/Gain/Defect/Runtime AVX2 parity tests; Regression: preprocessing ctest 341/341 passed on 2026-04-28. |

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
| Wang et al. (2013) | Heel effect correction in dual-SID imaging | SRS-CALIB-FUNC-005: Heel effect correction (다중 게인 다항식은 삭제, 사용자 결정 2026-11-16) |
| Ranger et al. (2014) | Characterization of lag in fluoroscopic FPD | ~~SRS-CALIB-FUNC-008: Exponential dark current model~~ (FUNC-008 삭제, 사용자 결정 2026-11-16) |
| Pang et al. (2006) | Multi-exponential lag model | SRS-CALIB-FUNC-013: Tier 1 LTI deconvolution |
| FixPix (2023) | Deep learning defect correction (MLP) | SRS-CALIB-FUNC-007: Advanced defect mode |
| EP2148500A1 | Dynamic dark current compensation | SRS-CALIB-FUNC-004: PREP-time interpolation (온도 조항은 삭제, 사용자 결정 2026-11-16) |

### Project Documentation

| Document | Location |
|----------|----------|
| Calibration Module README | `docs/calibration/README.md` |
| X-ray FPD Algorithm Specification | `.moai/specs/xpe-algorithm-spec-deepsync.md` |
| Pipeline Architecture | `.moai/project/pipeline-spec.md` |
| Detector Profiles | `.moai/project/detector-profiles.json` |
| Calibration Data Format | `docs/calibration/FORMAT_SPEC.md` |

---

*SRS-CALIB-001 v1.6 — End of Document*
