# SPEC-XPE-GSVG: Grid Suppression & Virtual Grid Module

**Document ID**: SPEC-XPE-GSVG
**Version**: 1.2.0
**Date**: 2026-04-22
**Status**: Active
**Owner Lane**: Post-B (`dev/postprocess`)
**Parent SPEC**: SPEC-XPE-MASTER v3.0.0 (Sprint S2-B)
**Companion SRS**: `docs/post-processing/gsvg/GSVG-SRS-001_Requirements.md` v1.0
**IEC 62304 Class**: B
**Module**: gsvg.dll
**Test Coverage**: gsvg ctest 110+ (억제·가상 그리드·마스크·상한·내구 포함). 요구별 증거는 각 Status 줄
**API Functions**: 8 exported (see api-spec.md)

---

## HISTORY

| Version | Date       | Author       | Changes |
|---------|------------|--------------|---------|
| 1.0.0   | 2026-04-22 | manager-spec | 초기 작성 — GSVG v0.2.0 구현 기반 SPEC 정의 |
| 1.2.0   | 2026-09-18 | xpe-leader | **Status 재정정**: 구현이 들어온 뒤의 실제 상태로 갱신(QA-B-106 의 요구별 증거표). 구현 13 / 부분 7 / 미구현 3. 019 는 실측 493 ms, 019b(가상 그리드 1.0 s) 신설, 020 은 241 MB 로 충족. 부분 4건(005·006·008·018)은 **합격 기준이 없어서** 부분이며 기준 결정 대기 |
| 1.1.0   | 2026-09-17 | xpe-leader | **Status 정정**: 21개 요구의 Implemented/Measured 표시가 코드와 맞지 않음(QA-B-87: 문언대로 구현 0, 다른 방식 2, 없음 19). 요구는 유지하고 구현한다(사용자 결정). 출처 정정·미확인 표시, FFTW3 제거 — 근거는 #180 의 문헌 조사 2회 |

---

## 1. Purpose and Scope

This SPEC defines the requirements, architecture, and acceptance criteria for the GSVG module
(Grid Suppression + Virtual Grid), which provides two independent image processing functions
for X-ray flat panel detector images:

1. **Grid Suppression (GS)**: Removal of anti-scatter grid line artifacts from images acquired with physical grids
2. **Virtual Grid (VG)**: Software-based scatter correction for images acquired without physical grids

### Module Independence

Per `.claude/rules/moai/development/xpe-module-principles.md`:
- gsvg.dll links only to xpe_common.dll and permissively licensed 3rd-party libs (FFTW3 removed — see §7 Dependencies)
- No lateral dependency on other XPE modules
- Readiness Level: R2 (ABI smoke test passing, DegradedMode verified)

---

## 2. EARS Requirements — Grid Suppression

### REQ-GSVG-001: Grid Line Frequency Auto-Detection

**When** an image with a physical anti-scatter grid is processed,
**the system shall** automatically determine the grid line frequency.

> **요구 개정 (2026-09-19, #180 — QA-B-126 뒤 리더 결정).** 원문은 "**DICOM 헤더**에서 산출" 이었습니다.
> 두 가지가 그 문구를 버리게 합니다.
>
> 1. **DICOM 은 범위 밖입니다**(사용자 결정 2026-09-18). 처리 체인의 입력은 검출기 raw 프레임이고,
>    GUI 파일 열기 필터가 `.raw` 뿐입니다(`clients/ImageProcTest/MainWindow.xaml.cs:214`).
>    **입력에 DICOM 이 없으면 헤더에서 읽을 것도 없습니다.**
> 2. **자기모순이었습니다.** Status 가 "미구현(DICOM 없음)" 인데 같은 줄에 "구현 1이 스펙트럼
>    봉우리로 찾는다(`grid_dwt.cpp:232`)" 가 있었습니다. Status 만 고치면 요구 본문이 여전히
>    DICOM 헤더를 요구해 다음 감사에 같은 지적이 다시 납니다 — 그래서 본문을 고칩니다.
>
> **출처를 헤더로 못 박지 않습니다.** 지금 구현(스펙트럼 봉우리 검출)이 요구를 만족하며,
> 태그가 들어오는 경로가 생기면 **교차 확인용**으로 더합니다(아래 담당 제안 참조).

- **Rationale**: Grid frequency is determined by detector pixel pitch and grid line density aliasing (Lin et al. 2006, *J Digit Imaging* 19(4):351-361 — CR, not flat-panel DR; the exact aliasing formula (Eq. 3-4) is not yet transcribed here)
- **Note (#180)**: the DICOM module reads no grid tags today; implementation 1 (QA-B-90) detects the grid frequency from the spectral peak instead
- **담당 제안 (2026-09-18, #180 — QA-B-115, 리더 승인)**: 읽기는 `modules/dicom` 이 맡고, gsvg 로는 새 인자 대신 기존 설정 키 `vg_grid_frequency_per_cm` 로 넘깁니다(그 키에 이미 검증이 붙어 있습니다). 스펙트럼 봉우리 검출을 **대체하지 않고 교차 확인**으로 쓰며, 불일치하면 검출값을 따릅니다. **표준 확인 (2026-09-18, QA-B-116)**: 선밀도를 담는 필드가 **있습니다** — `Grid Pitch` (0018,7044), DS, Type 3, "The pitch in mm of the X-Ray absorbing material used in the grid." 선밀도 = 10 / pitch[mm] (40 lp/cm ↔ 0.25 mm). `Grid Period`(0018,7048)는 왕복 주기(mSec)이므로 선밀도가 아닙니다. `Grid` 가 `NONE` 이면 격자 없음으로 읽어 적용 여부 판단에 쓸 수 있습니다. 이 확인은 B-115 의 "필드 없음(기억)" 보고를 **정정**한 것입니다. **원문 확인 (2026-09-18, QA-B-117)**: PS3.3 2026c **C.8.7.11 X-Ray Grid Module** — Table C.8-36 이 `Grid`(0018,1166)만 갖고 **Table C.8-36b "X-Ray Grid Description Macro"** 를 include 하며 `Grid Pitch`(0018,7044) Type 3 이 거기 있습니다. 2차 자료 확인이 원문과 일치했습니다. 매크로에 있으므로 같은 매크로를 쓰는 다른 IOD 에서도 기대할 수 있습니다. **혼동 주의 태그 셋**: `Grid Pitch`(mm, 이것이 맞음) / `Grid Period`(0018,7048, 왕복 주기 mSec) / `Grid Thickness`(흡수체 두께, 역시 mm).
- **이중 선택성**: X-Ray Grid 모듈이 User Optional 이고 그 안에서 Grid Pitch 가 Type 3 이라, 장비가 비워도 규격 위반이 아닙니다. **따라서 스펙트럼 봉우리 검출 경로는 반드시 남습니다** — 태그는 교차 확인용이며, 불일치 시 검출값을 따릅니다.
- **미확인**: 실제 장비가 이 태그를 채우는지는 모릅니다(Type 3 이라 비워도 규격 위반이 아닙니다) — 장비 영상(#151)이 오면 가장 먼저 확인할 항목입니다.
- **Verification**: Test
- **Status**: **Implemented (tested)** — 영상 스펙트럼의 봉우리에서 주파수를 찾습니다(`src/grid_dwt.cpp:232`, QA-B-90). 2026-09-19 요구 개정으로 출처가 헤더에 묶이지 않으므로 구현이 요구를 만족합니다

### REQ-GSVG-002: DWT Multi-Scale Decomposition

**When** grid suppression processes an input image,
**the system shall** decompose the image into multi-scale sub-bands using 2D Discrete Wavelet Transform.

- **Rationale**: DWT enables simultaneous spatial-frequency analysis for grid signal and anatomy separation (Tang et al. 2015, *Med Phys* 42(4):1721-1729, doi:10.1118/1.4914861 — verified)
- **Verification**: Test
- **Status**: **Implemented (tested)** — `src/grid_dwt.cpp:183` `Dwt2` / `:211` `Idwt2`(db4 8탭), `GsvgGridSuppression.Db4DwtReconstructsPerfectly`

### REQ-GSVG-003: Automatic Gridline Detection per Sub-Band

**When** DWT decomposition produces sub-bands,
**the system shall** automatically detect whether gridline signal energy exceeds threshold in each sub-band.

- **Rationale**: Auto-stop condition prevents over-decomposition (Tang 2015)
- **Verification**: Test
- **Status**: **Implemented (tested)** — `src/grid_dwt.cpp:272` `CheckSubband`(평균 + 3σ), `…SubbandPlacementMatchesHandValues`, `…WithoutInputGateGridFreeImagesChange`

### REQ-GSVG-004: Gaussian Band-Stop Filtering

**When** gridline signal is detected in a sub-band,
**the system shall** apply a Gaussian band-stop filter to remove the gridline signal.

- **Rationale**: Gaussian band-stop applied to the detected DWT sub-bands (Tang et al. 2015, verified). Lin et al. 2006 argues a Gaussian filter produces no ripple while notch filters ring — an argument from the Gaussian's Fourier transform, not a measured comparison. Yu & Wang 2021 (*Med Phys* 48(7)) report that spectral band-stop filtering can blur and ring; see the GRD option in §7
- **Verification**: Test
- **Status**: **Implemented (tested)** — `BandStop`(σ = 1.5 빈), `…GridIsSuppressedByAtLeast40dB`, 반증 `…WithoutBandStopOnlySuppressionFails`

### REQ-GSVG-005: Visual Artifact Removal

**When** grid suppression completes,
**the system shall** produce output where gridline artifacts are visually imperceptible.

- **Rationale**: Residual artifacts interfere with diagnosis (HAZ-005)
- **Verification**: Test + Review
- **Status**: **Partial** — 억제는 동작하지만 잔여 격자 에너지가 격자 없는 기준선의 **21–381배**입니다(`…KnownDivergence_ResidualStaysAboveTheGridFreeBaseline`). **빠진 것: "보이지 않는다" 의 합격 기준이 없습니다** — 기준 결정 대기 **잠정 기준 (사용자 결정 2026-09-18)**: 실질 방지선은 **억제 전/후 비율(after/before)** 에 겁니다 — 격자 없는 기준선 대비(after/baseline)는 분모가 장면마다 달라 같은 조건에서 187–547 로 3배 흔들려 회귀 방지선으로 부적합합니다(QA-B-109 측정). after/baseline 에는 느슨한 상한만 둡니다. #151 에서 기준을 다시 정할 때 **분모 자체를 먼저** 보십시오. 임상 합격선이 아니라 회귀 방지선입니다. 실제 장비 영상 확보 시 재설정(#151).

### REQ-GSVG-006: MTF Preservation

**When** grid suppression processes an image,
**the system shall** limit MTF degradation to < 5% compared to the original.

- **Rationale**: Excessive filtering degrades diagnostic resolution
- **Verification**: Test
- **Status**: **Partial** — 경계 법선 방향 선은 5% 미만(`…MtfLossStaysUnderFivePercentForLinesAlongTheEdgeNormal`). **경계를 가로지르는 선은 11% 손실로 요구 초과**(`…KnownDivergence_LinesAcrossAnEdge`) **잠정 기준 (사용자 결정 2026-09-18)**: 경계 법선 방향 5% 미만은 유지, 경계를 가로지르는 선은 **현재 11% 보다 나빠지면 실패**. 회귀 방지선이며 임상 합격선이 아닙니다. 실제 장비 영상 확보 시 재설정(#151).

### REQ-GSVG-007: Grid Frequency Range

**When** a grid with 60~200 lines/inch is used,
**the system shall** correctly process the image.

- **Rationale**: Market-available grid range coverage
- **Verification**: Test
- **Status**: **Implemented (tested)** — 60 / 103 / 200 lpi 모두 검출·필터(`test_grid_suppression.cpp:32`, `…GridIsSuppressedByAtLeast40dB`)

### REQ-GSVG-008: Moire Pattern Removal

**When** detector-grid frequency aliasing produces Moire patterns,
**the system shall** remove the aliasing artifacts.

- **Rationale**: Common artifact type from detector-grid frequency aliasing
- **Verification**: Test
- **Status**: **Partial** — 200 lpi 는 에일리어싱된 주파수에서 제거됩니다. **170–186 lpi 구간은 기록만 하고 단언이 없습니다**(`…ReportSevereAliasing`) — 합격 기준 결정 대기 **잠정 기준 (사용자 결정 2026-09-18)**: 170–186 lpi 구간의 현재 측정값을 기록하고, **그보다 나빠지면 실패**. 회귀 방지선입니다.

---

## 3. EARS Requirements — Virtual Grid

### REQ-GSVG-009: Body Thickness Estimation

**When** a non-grid image is processed,
**the system shall** estimate the water-equivalent thickness **per pixel, from the image itself** (`L = -ln(P/I0) = mu(t)·t`).

> **요구 개정 (2026-09-18, #180 — QA-B-114 조사 뒤 리더 결정)**
>
> 원문은 "kVp·mAs·SID·조사야 크기로 두께를 추정" 이었습니다. 두 가지가 그 방향을 버립니다.
>
> 1. **문헌은 반대 방향만 지지합니다.** 확인한 자료는 모두 "두께 → 노출 조건" 입니다 — Ching 2014 의 체계적 문헌고찰(PMC4175846)에 실린 체계는 두께를 캘리퍼로 재서 **입력**받고, 최근 연구도 적외선 센서로 실측합니다. 노출 조건에서 두께를 되돌리는 방법을 직접 지지하는 출처는 찾지 못했습니다. 뒤집어 쓰려면 장비·부위별 기준 노출 표, kVp·격자·SID 고정, AEC 목표 선량 일정이라는 세 조건이 필요하고, 근거로 쓰이는 25% 규칙조차 HVL 값이 문헌마다 3 / 3.3~3.8 / 4 cm 로 엇갈립니다.
> 2. **결과가 스칼라 하나입니다.** 산란 커널은 화소별 두께 지도로 인덱싱됩니다. 전역 두께 하나로 바꾸면 계단 경계 오차 중앙값이 0.01544 → 0.23991 로 **15.5배** 나빠집니다(`GsvgVirtualGridFalsify.GlobalThicknessIsWorseAtTheStep`).
>
> 노출 조건(kVp 등)은 **커널 표를 고르는 입력**으로 계속 쓰입니다(REQ-GSVG-010·011). 두께 추정의 입력이 아닙니다.
>
> 실제 장비 영상(#151)이 들어오면 이 추정의 정확도를 검증합니다.

- **Rationale**: Thickness is a primary determinant of SPR (Kyriakou & Kalender 2007, *Phys Med* 23(1):3-15 — flat-detector CT, thickness is a simulation input)
- **Verification**: Test
- **Status**: **Implemented (tested)** — 영상 기반 역산 `L = -ln(P/I0) = mu(t)·t`(`ThicknessFromLogAtten`, `GsvgVirtualGridKernel.ThicknessInversionRoundTrips`). 2026-09-18 개정으로 요구 자체가 화소별 영상 기반 추정이 되었으므로 구현과 요구가 일치합니다(위 개정 블록 참조)

### REQ-GSVG-010: SPR Calculation

**When** body thickness and exposure parameters are available,
**the system shall** calculate the Scatter-to-Primary Ratio (SPR).

- **Rationale**: SPR determines scatter correction strength
- **Verification**: Test
- **Status**: **Implemented (tested)** — 커널 합(무한 조사야 SPR)과 반복 갱신, `GsvgVirtualGridCap.KernelSumIsTheDefaultCap`

> **MC 진짜 산란과의 직접 비교 (2026-09-19, QA-B-125)**: 추정/진짜 비 중앙값 **1.008**
> (p05 0.964, p95 1.046). 합성 자료로는 불가능한 비교입니다.
>
> **[한계] 완전한 독립 검증이 아닙니다.** 이 커널 표는 **같은 MC 코드로 적합**된 것입니다
> (`fit_kernels.py`). 팬텀 장면(계단·쐐기)이 적합 자료(균일 슬래브)와 다르다는 점에서만
> 부분적으로 독립입니다.

### REQ-GSVG-011: Scatter Distribution Estimation

**When** SPR is calculated,
**the system shall** estimate scatter distribution using pre-computed scatter kernel LUT.

- **Rationale**: MC-based LUT enables real-time processing with physical accuracy. Established basis: thickness-adaptive kernel superposition (Sun & Star-Lack 2010, *Phys Med Biol* 55(22):6695-6720); a four-Gaussian kernel outperforms two-Gaussian (Bhatia et al. 2017, *J X-Ray Sci Technol* 25(4):613-628)
- **Verification**: Test
- **Status**: **Implemented (tested)** — `ScatterEstimate`(표의 gauss4 커널 중첩, 축소 격자), `…ConvolutionKeepsTheTableNormalisation`, `GsvgVirtualGridTable.Gauss4RowsWinOverGauss2`


> **MC 진짜 산란과의 직접 비교 (2026-09-19, QA-B-125)**: 추정/진짜 비 중앙값 **1.008**
> (p05 0.964, p95 1.046). 합성 자료로는 불가능한 비교입니다.
>
> **[한계] 완전한 독립 검증이 아닙니다.** 이 커널 표는 **같은 MC 코드로 적합**된 것입니다
> (`tools/mcsim/fit_kernels.py`). 팬텀 장면(계단·쐐기)이 적합 자료(균일 슬래브)와 다르다는
> 점에서만 부분적으로 독립입니다. **"MC 로 검증됨" 으로 읽지 마십시오.**

### REQ-GSVG-012: Scatter Subtraction

**When** scatter distribution is estimated,
**the system shall** subtract scatter from the original to produce a primary-only image.

- **Formula**: I_primary = I_total - I_scatter
- **Verification**: Test
- **Status**: **Implemented (tested)** — `P = I / (1 + SPR)`, `out = P + (Ts/Tp)·S`, MC 팬텀 대비 `GsvgVirtualGridMc.CompareToPrimary`

### REQ-GSVG-013: Multi-Scale Contrast Enhancement

**When** scatter subtraction completes,
**the system shall** apply Laplacian Pyramid decomposition for multi-scale contrast enhancement.

- **Rationale**: US8064676B2 discloses a 4-8 level Laplacian pyramid (verified). The patent's scatter model is empirical low-band attenuation; it does **not** support REQ-GSVG-009/010/011/025
- **Verification**: Test
- **Status**: **Implemented (tested)** — `virtual_grid.cpp:583` `PyramidContrast`(4–8단), `GsvgVirtualGridPyramid.UnitGainIsIdentityAndGainRaisesDetail`

### REQ-GSVG-014: De-Noising

**When** high-frequency bands contain amplified noise from scatter subtraction,
**the system shall** apply de-noising.

- **Rationale**: Scatter subtraction amplifies noise (Lim et al. 2023, *J Imaging* 9(12):272 — breast X-ray, GAN de-noising; the method is not transferable as-is)
- **Verification**: Test
- **Status**: **Implemented (tested)** — 최상위 대역 소프트 문턱, `GsvgVirtualGridPyramid.DenoiseLowersFlatRegionNoise`

### REQ-GSVG-015: CNR Preservation

**When** virtual grid processing completes,
**the system shall** achieve CNR >= 90% of a 6:1 physical grid reference image under identical conditions.

- **Rationale**: Minimum clinically meaningful performance threshold
- **Open (#180)**: an independent 2026 phantom study (Radiography 32(3):103354) found software CNR falls with thickness and physical grids remain superior at 33 cm. A single threshold across 10-30 cm is at risk; acceptance should be set per thickness
- **Verification**: Test
- **Status**: **Not implemented** — 물리 격자 기준 영상도 CNR 측정 코드도 없습니다. 실제 장비 영상 대기(#151)

### REQ-GSVG-016: Virtual Grid Ratio Selection

**When** a user selects a virtual grid ratio,
**the system shall** support 6:1, 8:1, 10:1, and 12:1 options.

- **Rationale**: Exam body part and patient size flexibility
- **Verification**: Test
- **Status**: **Implemented (tested)** — 표에서 (격자비, 선밀도) 설계 선택, 제품 표에 네 비율 모두 존재. `GsvgVgProductTable.LoadsWithEveryRatioOfReqGsvg016`, `GsvgVirtualGridRatio.ResidualSprFallsWithRatio`

### REQ-GSVG-017: Thickness Range

**When** acrylic thickness ranges from 10cm to 30cm,
**the system shall** produce valid virtual grid output.

- **Rationale**: Pediatric to obese patient range coverage
- **Verification**: Test
- **Status**: **Implemented (tested)** — 표 범위를 넘으면 표 최대로 제한하고 비율을 보고, 범위 아래는 0 으로 페이드(`GsvgVirtualGridRange.*`). 두께별 유효성은 아래 측정으로 확인했습니다.

> **두께별 측정 (2026-09-18, #180 — QA-B-115)**
>
> 정답은 1차(primary)가 아니라 `P + (Ts/Tp)(t)·S_true` 입니다 — 제품 표의 격자(비 6–12, Ts/Tp≈0.10)는 잔여 산란을 **설계상 통과시키므로**, 1차와 직접 비교하면 그 잔여분이 통째로 오차로 잡힙니다(1차 기준 오차 10 cm 14.8% → 30 cm 101.8%). MC 시험이 1차와 직접 비교할 수 있는 것은 그쪽이 이상 격자 행(tp 1, ts 0)을 쓰기 때문입니다.
>
> | 공칭 두께 | 중앙값 \|r−1\| | p95 \|r−1\| | 표 초과 화소 |
> |---|---|---|---|
> | 10 cm | 0.0018 | 0.0056 | 0% |
> | 15 cm | 0.0035 | — | 0% |
> | 20 cm | 0.0056 | — | 0% |
> | 25 cm | 0.0074 | — | 0% |
> | 30 cm | 0.0091 | 0.0450 | 38.06% |
>
> 관측 셋: (1) 두께가 커질수록 단조 증가(중앙값 5배, p95 8배)하며 **튀는 구간 없음**. (2) **노드와 보간 구간의 차이가 없습니다** — 13·17.5·22.5·27.5 cm 가 이웃 노드 사이에 매끄럽게 들어가므로 커널 보간이 별도 오차를 더하지 않습니다. (3) 두꺼울수록 일부 화소를 과다 차감하는 쪽으로 퍼집니다(비율 최소 0.998 → 0.947).
>
> **상단 경계 (리더 결정)**: 요구 범위 상단(30 cm)이 제품 표의 최상단 노드와 같아, 공칭 30 cm 장면에서는 화소별 두께 분포 때문에 38.06% 가 표를 넘어 제한 경로로 갑니다(29 cm 이하는 0%). 이는 커널 표의 구조적 제약이고 코드 결함이 아닙니다. 커널 데이터를 새로 만들지 않는 한 넓힐 수 없으므로, 본 요구의 "유효" 는 **공칭 두께 ≤ 30 cm** 로 읽습니다. 화소별 초과분은 제한 + 보고 경로(REQ-GSVG-024 계열)가 처리합니다.
>
> **합격 기준 (리더 결정, 시험 반영 QA-B-116)**: 위 값은 **회귀 바닥**으로만 씁니다 — `test_thickness_range.cpp`, 중앙값 문턱 0.0032 / 0.0057 / 0.0089 / 0.0114 / 0.0137, p95 상한 0.0702. **문턱은 시험이 실제로 도는 설정(512 px / 0.8 mm)에서 다시 잰 값**이며 위 표(1024 px / 0.4 mm)와는 잰 설정이 다릅니다(같은 41 cm 시야, 값은 만분의 3 안에서 일치). 반증 확인: 반복을 5→2 로 줄이면 다섯 두께 전부 빨강. **정확도 주장이 아닙니다** — 이 장면은 합성이고 산란 모델이 구현과 같은 커널을 쓰므로 자기 순환입니다. 정확도 판정은 512² MC 팬텀(QA-A-114)으로 다시 합니다.
>
> **MC 기준 정확도 (2026-09-19, QA-B-123·B-124)** — 512² 팬텀, 화소 간격 0.140 mm,
> **이상 격자(tp 1, ts 0)** 기준. 이상 격자를 쓰는 이유는 제품 격자로 보면 정답에 우리 표의
> `Ts/Tp` 가 들어가 자기 순환이 되기 때문입니다.
>
> **[중요] 정확도는 장면에 크게 의존합니다 — 표를 읽을 때 장면을 함께 보십시오.**
>
> | 공칭 두께 | 쐐기(매끄러움) | 계단(불연속) |
> |---|---|---|
> | 10.5 cm | 0.0124 | 0.0367 |
> | 24.5 cm | 0.0297 | 0.0362 |
> | 10 / 15 / 20 / 25 cm (쐐기) | 0.0115 / 0.0138 / 0.0184 / 0.0315 | — |
>
> **계단이 쐐기보다 1.2–3.0배 나쁩니다.** 계단 값은 두께에 거의 무관하고(0.0327–0.0367)
> 쐐기만 두께와 함께 커집니다 — 계단은 0.5 cm 마다 두께가 끊겨 모든 화소가 경계 근처이므로,
> 경계 근처의 오차가 중앙값을 지배합니다(QA-B-125 에서 확인).
>
> **진짜 이름은 "장면 의존" 이 아니라 "경계 근처 정확도" 입니다.** 오차는 경계에서 멀어질수록
> 줄어듭니다 — 0.28–0.56 mm 에서 0.0554(최대), 2.24–4.48 mm 에서 0.0227. ±N 제외도
> N=1.4 mm 까지 단조 감소(0.0344 → 0.0233)입니다.
>
> **[한계] 측정된 거리 범위는 경계로부터 4.5 mm 까지입니다.** 계단이 metric 영역에 18개,
> 간격 2.3 mm 라 **경계에서 4.5 mm 이상 떨어진 화소가 0개**이고, 가장 먼 구간도 쐐기(0.0156)의
> **1.45배**입니다. 즉 **"경계만 피하면 쐐기 수준" 이라고 말할 수 없습니다** — 그 거리에서
> 수렴하는지는 이 팬텀으로 확인되지 않습니다. 완결하려면 계단 간격 2 cm 이상인 팬텀이
> 필요합니다(지금은 만들지 않습니다). (N=20 에서 값이 도로 오르는 것은 화소가 8%만 남은
> 표본 편향입니다.)
>
> **봉우리가 경계 바로 위가 아닙니다** — 0.28–1.12 mm 에 있고 경계 화소 자체는 0.0311 로
> 오히려 낮습니다. 가장 좁은 커널 항 σ₁ = 0.92–1.21 mm 와 자릿수가 같아, 오차가 "경계 화소" 가
> 아니라 **가장 좁은 산란 항이 경계를 넘어 번지는 범위**에 붙은 것으로 읽힙니다(커널 항을
> 하나씩 꺼서 확인하지는 않았습니다).
>
> **임상적 함의**: 해부학적 경계는 어디에나 있으므로, 이 오차는 특수한 장면이 아니라
> **구조가 있는 모든 영상에서 경계 주변 수 mm 대역**에 나타납니다.
>
> **MC 오차가 합성 기준의 1.3–17배입니다.** 합성이 낙관적이었던 이유는 자기 순환(우리 커널로
> 만든 산란을 우리 커널로 뺌)과, MC 의 통계 잡음·실제 각분포입니다. **합성 바닥은 회귀용으로
> 그대로 두고, 정확도는 이 표로 봅니다** — 둘은 다른 일을 합니다.
>
> **합격선은 아직 정하지 않습니다.** MC 에도 통계 잡음과 각분포 가정이 있고, 실장비 영상이
> 오면 다시 봅니다(#151).
>
> **상단 30 cm 는 MC 근거가 없습니다 (리더 결정 2026-09-19).** 512² 팬텀의 조사야 안 최대
> 두께가 계단 27.0 cm · 쐐기 26.0 cm 이고, 80² 도 같은 한계였습니다. 팬텀을 다시 도는(약 2.5시간)
> 대신 구멍으로 기록합니다 — 추세가 단조·연속이고 외삽이 3 cm 뿐이며 산란분율에 문턱이 없습니다.
> **무엇보다 외삽 불확실성(0.03→0.04)보다 위의 장면 의존성(1.2–3.0배)이 훨씬 큽니다.**

### REQ-GSVG-018: No Artifacts from Overcorrection

**When** virtual grid processing completes,
**the system shall** not introduce artificial artifacts in anatomical structures.

- **Rationale**: Overcorrection artifacts can cause misdiagnosis (HAZ-003)
- **Verification**: Test + Review
- **Status**: **Partial** — 상한(CapMode GlobalSum)으로 과보정을 막습니다(`GsvgVirtualGridFalsify.SprCapPreventsOvercorrection`). **계단 경계에서 덜 뺍니다** — 512² 팬텀 재측정(QA-B-123)으로 봉우리 **1.0504**(약 5%), 최악 화소 1.1928, 중앙값 0.0342. **옛 80×80 값 17–40%(QA-B-95)를 대체합니다.** **좋아진 것으로 읽지 마십시오** — 팬텀이 다르고, 화소 크기 가설은 QA-B-124 에서 기각돼 어느 팬텀 성질 때문인지 모릅니다 — "인공물 없음" 의 합격 기준 결정 대기 **잠정 기준 (사용자 결정 2026-09-18)**: 계단 경계 두꺼운 쪽의 덜 뺌 비율이 **현재(17–40%)보다 커지면 실패**. 회귀 방지선이며 임상 합격선이 아닙니다.

> **문턱 여유 측정 (2026-09-18, #180 — QA-B-117)**: 봉우리 1.4521 에 문턱 1.465, 여유 0.013 이 무엇에서 나왔는지 세 축으로 쟀습니다.
> - **스레드 1/자동/8**: 1.452071 로 소수점 여섯 자리까지 동일, 퍼짐 0.
> - **입력 잡음**: 상대 잡음 0.1% 만으로 여유의 58–83% 를 먹고, 0.3% 면 1.4794–1.4817 로 문턱을 넘습니다(봉우리가 최댓값 통계라 잡음이 한쪽으로만 밉니다). 팬텀에 신뢰할 광자 수가 없어(`dn_scale` 은 플루언스→DN 변환이지 DN 당 광자 수가 아닙니다) 절대 잡음 수준은 **지어내지 않았습니다**.
> - **측정 영역 ±4 화소**: 1.4271–1.4572, 폭 0.030. 영역은 상수라 실제로 흔들리지는 않으나 지표가 가파르다는 증거.
>
> **512² 팬텀으로 재설정 (2026-09-19, QA-B-123 — 리더 결정)**: 옛 문턱 1.465 는 옛 팬텀
> 전용이었습니다. 새 팬텀에서 세 지표 모두 좋아졌고(1.0504 / 1.1928 / 0.0342), 문턱은
> **측정값 +5% 인 1.103 / 1.253 / 0.036** 입니다. **넓히지 않았습니다** — 0.3% 상대 잡음이
> 봉우리를 0.0001(여유의 0.2%)만 움직입니다. 옛 팬텀의 58–83% 가 이어지지 않는 이유는
> 봉우리가 여기서는 294행 열평균이고 거기서는 64행이었기 때문입니다. 반증: 되풀이를 1회로
> 깎으면 1.6603 으로 빨강.
>
> **좋아진 이유는 "팬텀 성질" 이지만 어느 성질인지는 모릅니다** (QA-B-124). 화소 크기 가설은
> 기각됐습니다 — 같은 팬텀을 2/4/8 로 묶어 화소를 키우니 봉우리가 오히려 **내려갑니다**
> (1.0504 → 1.0392 → 1.0337 → 0.9765). 옛 팬텀(4 mm)의 1.4521 과 방향이 반대입니다.
> (주의: f=8 은 봉우리가 1 아래이고 중앙값 |r−1| 0.5012 라 전체가 과도하게 빠진 상태이므로,
> 이 표를 "화소를 키우면 좋아진다" 로 읽으면 안 됩니다.) 조사야·계단 폭·대비·산란분율·히스토리
> 수를 분리해 재지 않았으므로 **다음 팬텀에서 또 달라질 수 있고, 교체 체크리스트는 계속
> 유효합니다.**
>
> 아래는 옛 팬텀 기준의 기록입니다.
>
> **결정 (리더, 2026-09-18)**: **문턱은 1.465 그대로 둡니다.** 이 시험이 도는 조건에서는 값이 전혀 움직이지 않습니다(잡음 없는 고정 자료 한 벌, 결정적 체인, 스레드 불변) — 값이 불변인 입력에서 문턱을 넓히면 얻는 것 없이 감지력만 잃습니다. 0.013 은 툴체인 차이를 흡수하는 몫입니다.
>
> **다만 이 문턱은 이 팬텀에 붙어 있습니다.** 512² MC 팬텀(QA-A-114)은 별도의 잡음 실현을 가지므로 교체하면 회귀가 아닌 이유로 빨강이 될 수 있습니다. **팬텀 교체 작업에 이 문턱 재설정을 반드시 포함합니다.**

---

## 4. Performance Requirements

### REQ-GSVG-019: Processing Time

**When** a 3072x3072 16-bit image is processed,
**the system shall** complete processing within 1.0 seconds (Tier 1 DWT, Intel i7 or equivalent).

- **Rationale**: Clinical workflow delay minimization (HAZ-006)
- **Verification**: Test
- **Status**: Measured (2026-09-18, QA-B-102/B-103, 개발 PC i7-12700, 제품 기본 스레드 설정) — **713–757 ms**, 요구 충족. CI 는 836 ms(추세 관찰용). 조건의 Tier 1 DWT 는 구현돼 있습니다(#180).

> **조사야 마스크 밖 처리 (#189) — 결정과 그 함정 (2026-09-19, QA-B-110·B-123)**
>
> 마스크 밖의 0 이 라플라시안 피라미드를 거쳐 안쪽 결과에 섞이는 문제입니다.
> **가장자리 복제(Replicate)를 씁니다.** 512² MC 팬텀에서도 방향이 유지됩니다 —
> Zero 가 두 단수 모두 최악이고, Replicate 가 Keep 보다 낫습니다.
>
> **[함정] 지표 하나로 결정하면 뒤집힙니다.** `RectOnly` 는 **경계 단차만 보면 더 낮은데
> (6단 0.5035 대 Replicate 0.5449) 정확도는 가장 나쁩니다**(중앙값 |r−1| 0.1111 대 0.0986).
> 두 지표가 반대를 가리킵니다. 정확도를 기준으로 Replicate 를 유지합니다 — 경계 단차는
> 이차적인 인공물 지표입니다. **단차만 보고 RectOnly 로 바꾸지 마십시오.**
>
> 측정 시 주의: 단차는 **primary 대비 비로 정규화**해야 합니다. 원시 평균으로 재면 계단
> 팬텀의 두께 기울기가 신호를 6배 흔들어 **팬텀을 재게 됩니다**(첫 측정에서 단차가 5–7 로
> 나와 발견).

### REQ-GSVG-019b: Virtual Grid Processing Time (추가, 2026-09-18, #179/#180)

**When** a 3072x3072 16-bit image is processed with the virtual grid (Tier 2),
**the system shall** complete processing within 1.0 seconds (기준 기계: 개발 PC i7-12700, 제품 기본 스레드 설정).

- **Rationale**: REQ-GSVG-019 의 조건은 Tier 1 DWT 억제이고, 가상 그리드는 그 조건에 없었습니다. 두 경로는 배타적이라 한 호출에서 합산되지 않으므로 시간 요구를 따로 둡니다.
- **Verification**: Test
- **Status**: Measured (QA-B-104) — 제품용 표로 **640 ms**, 요구 충족. 합성 표 597–613 ms 대비 약 5% 느립니다.

### REQ-GSVG-020: Peak Memory

**When** processing any single frame,
**the system shall** use no more than 512 MB peak memory.

- **Rationale**: Console PC memory constraint
- **Verification**: Test
- **Status**: **Met (measured, 2026-09-18)** — 3072² 커밋 차지: 억제 384 MB, 가상 그리드 241 MB(QA-B-107 에서 626 MB 에서 줄임), 마스크 경로 377 MB. 한계 512 MB

### REQ-GSVG-021: Memory Leak Prevention

**When** 100 consecutive frames are processed in batch mode,
**the system shall** exhibit zero memory leaks.

- **Rationale**: Long-term operational stability
- **Verification**: Test
- **Status**: **Met (measured)** — 1000 주기 CRT 힙 워크 + 가짜 누수 대조 2건(`GsvgEndurance.*`), #181

---

## 5. Safety Requirements

> **기록 누락 정정 (2026-09-19, QA-B-126)**: 022·024·025·026 은 **Status 줄이 비어 있었습니다.**
> 넷 다 HAZ 연결 요구인데, **넷 다 실제로는 시험이 있습니다** — 비워 둔 것이지 못 채운 것이
> 아닙니다. 빈 줄을 미구현으로 읽으면 **위험 연결 요구 4건이 미구현으로 집계됩니다.**
> post 레인이 요구별로 시험을 찾아 확인했고, 아래에 채웠습니다.

### REQ-GSVG-022: Original Image Protection

**When** any algorithm failure occurs,
**the system shall** not corrupt the original input image.

- **Hazard**: HAZ-001
- **Verification**: Test

- **Status**: **Implemented (tested)** — 실패 시 원본을 그대로 둡니다. `GsvgVirtualGridApi.ProcessesAndKeepsTheOriginalOnFailure`(`test_virtual_grid.cpp:765`, `dst==src` 확인) 와 `…SourceIntact`. 판정 2026-09-19 (QA-B-126)

### REQ-GSVG-023: DICOM Processing Mark

**When** an image is processed,
**the system shall** record a "Processed" marking in DICOM tags.

- **Hazard**: HAZ-002
- **Verification**: Test
- **Status**: **Not implemented (gsvg 범위 밖)** — gsvg 는 DICOM 을 쓰지 않습니다. 판정 2026-09-18 (QA-B-106)
- **담당 제안 (2026-09-18, #180 — QA-B-115, 리더 승인)**: 핵심은 `modules/dicom` 의 `DicomWriter.cpp:143` 에서 ImageType 이 `ORIGINAL\PRIMARY\` 로 고정되어 있는 것이며, 처리된 영상에 `DERIVED` 를 쓰는 신호 하나가 최소 변경입니다. 다만 **GUI 에 DICOM 쓰기 경로가 없어 지금은 소비자가 없으므로**, 내보내기 경로가 생길 때 함께 처리합니다.

### REQ-GSVG-024: Fail-Safe Pass-Through

**When** processing fails,
**the system shall** return the original image unmodified with an error code.

- **Hazard**: HAZ-001
- **Verification**: Test

- **Status**: **Implemented (tested)** — 잘못된 설정·널 설정에서 통과 모드로 떨어집니다. `GsvgEdgeCases.MalformedConfigFallsBackToPassThrough`, `GsvgDegradedMode.InitWithNullConfig_DefaultsToPassThrough`, `GsvgAbiSmoke.Lifecycle3072_PassThroughIsByteEqual`(바이트 동일). 판정 2026-09-19 (QA-B-126)

### REQ-GSVG-025: SPR Clamping

> **MC 에서의 상한 동작 (2026-09-19, QA-B-125)**: 상한이 실제 산란에서 **6.25% 화소에
> 걸립니다** — 합성에서만 걸리는 장식이 아닙니다. 비용은 거의 0(중앙값 1.0075 → 1.0079).
>
> **[한계] 이 팬텀은 상한이 막아 주는 상황을 만들지 못합니다** — 음수 1차가 세 설정 모두
> 0건입니다. **안전 기능 자체의 검증은 합성 시험(QA-B-112·113) 몫으로 남습니다.**


**When** scatter correction strength exceeds physical maximum,
**the system shall** clamp to the physical limit.

- **Hazard**: HAZ-003
- **Verification**: Test

- **Status**: **Implemented (tested)** — `GsvgVirtualGridFalsify.SprCapPreventsOvercorrection`, `GsvgVirtualGridCap.KernelSumIsTheDefaultCap`. MC 에서 상한이 실제 산란의 6.25% 화소에 걸립니다(QA-B-125). 판정 2026-09-19 (QA-B-126)

### REQ-GSVG-026: Output Value Range

**When** output pixel values are computed,
**the system shall** ensure all values are within valid DICOM range (0~65535).

- **Hazard**: HAZ-004
- **Verification**: Test

- **Status**: **Implemented (tested)** — `GsvgAbiSmoke` 의 65535 포화 0건 단언 외 2건. 판정 2026-09-19 (QA-B-126)

---

## 6. Acceptance Criteria

| Category | Criterion | Status |
|----------|-----------|--------|
| Grid Suppression | GS-FR-001~008 all PASS | ❌ Not met — implementation in progress (QA-B-90, #180) |
| Virtual Grid | VG-FR-001~010 all PASS | ❌ Not met — not implemented; thickness method open (#180) |
| Performance | PERF-001~004 all PASS | ❌ Not measured (#180, #179) |
| Safety | SAFE-001~005 all PASS | ⚠️ Partial — 022/026 implemented, 024 different (no pass-through), 023/025 not implemented (QA-B-87) |
| Benchmark | BP-06 GSVG Version Probe < 5000 us | ✅ PASS (2026-04-22) |
| DegradedMode | Graceful degradation without crash | ✅ PASS |
| API | 8 exported functions in gsvg.dll | ✅ Per api-spec.md |
| IEC 62304 | Class B documentation package | ✅ GSVG-SRS/SDD/VVP |

---

## 7. Architecture

```
gsvg.dll
├── Grid Suppression Pipeline
│   ├── Grid Frequency Detection (DICOM metadata)
│   ├── 2D DWT Decomposition (3-tier: DWT, DCT, GRD)
│   ├── Sub-band Gridline Detection
│   ├── Gaussian Band-Stop Filter
│   └── Reconstruction
├── Virtual Grid Pipeline
│   ├── Body Thickness Estimation
│   ├── SPR Calculation (Monte Carlo LUT)
│   ├── Scatter Subtraction
│   ├── Laplacian Pyramid Contrast Enhancement
│   └── De-Noising
└── Common
    ├── JSON Config Interface
    ├── Error Handling (fail-safe pass-through)
    └── Memory Management (xpe_common)
```

### Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| xpe_common.dll | XPE module | Shared runtime, types, memory |
| ~~FFTW3~~ | ~~3rd-party~~ | **Removed (#180)**: FFTW3 is GPL v2+ (or commercial) and is not linked by the code. If an FFT is needed, use a BSD-licensed library (PocketFFT or KissFFT) — decision pending |
| spdlog | 3rd-party | Logging |

---

## 8. Traceability

| This SPEC | SRS | SVVP | IEC 62304 |
|-----------|-----|------|-----------|
| REQ-GSVG-001~026 | GSVG-SRS-001 | Section 5.1 (BP-06) | Class B §5.2, §5.5, §5.6 |

---

## 9. References

- SRS: `docs/post-processing/gsvg/GSVG-SRS-001_Requirements.md` v1.0
- IEC 62304 Package: `docs/post-processing/gsvg/GSVG_IEC62304_ClassB_Document_Package.md`
- SOUP Analysis: `docs/post-processing/gsvg/GSVG-SOUP-001_SOUP_Analysis.md`
- Benchmark: `benchmark/BP-06-09-post-benchmark-baseline.md`
- API: `docs/project/api-spec.md` v1.3.0 (gsvg.dll: 8 functions)
- Lin 2006 (Grid line suppression)
- Tang 2015 (DWT multi-scale decomposition)
- Kyriakou 2007 (SPR estimation)
- Lim 2023 (Post-scatter noise)
- US8064676B2 (Laplacian Pyramid VG)

---

*Document End — SPEC-XPE-GSVG v1.0.0*
