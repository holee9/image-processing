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

**When** a DICOM image with a physical anti-scatter grid is processed,
**the system shall** automatically calculate the grid line frequency from DICOM header metadata and grid specification.

- **Rationale**: Grid frequency is determined by detector pixel pitch and grid line density aliasing (Lin et al. 2006, *J Digit Imaging* 19(4):351-361 — CR, not flat-panel DR; the exact aliasing formula (Eq. 3-4) is not yet transcribed here)
- **Note (#180)**: the DICOM module reads no grid tags today; implementation 1 (QA-B-90) detects the grid frequency from the spectral peak instead
- **Verification**: Test
- **Status**: **Not implemented** — 입력에 DICOM 이 없어 헤더에서 산출하지 않습니다. 구현 1(QA-B-90)은 영상 스펙트럼의 봉우리에서 주파수를 찾습니다(`src/grid_dwt.cpp:232`). 판정 2026-09-18 (QA-B-106)

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
- **Status**: **Partial** — 억제는 동작하지만 잔여 격자 에너지가 격자 없는 기준선의 **21–381배**입니다(`…KnownDivergence_ResidualStaysAboveTheGridFreeBaseline`). **빠진 것: "보이지 않는다" 의 합격 기준이 없습니다** — 기준 결정 대기

### REQ-GSVG-006: MTF Preservation

**When** grid suppression processes an image,
**the system shall** limit MTF degradation to < 5% compared to the original.

- **Rationale**: Excessive filtering degrades diagnostic resolution
- **Verification**: Test
- **Status**: **Partial** — 경계 법선 방향 선은 5% 미만(`…MtfLossStaysUnderFivePercentForLinesAlongTheEdgeNormal`). **경계를 가로지르는 선은 11% 손실로 요구 초과**(`…KnownDivergence_LinesAcrossAnEdge`)

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
- **Status**: **Partial** — 200 lpi 는 에일리어싱된 주파수에서 제거됩니다. **170–186 lpi 구간은 기록만 하고 단언이 없습니다**(`…ReportSevereAliasing`) — 합격 기준 결정 대기

---

## 3. EARS Requirements — Virtual Grid

### REQ-GSVG-009: Body Thickness Estimation

**When** a non-grid image with exposure parameters (kVp, mAs, SID, field size) is processed,
**the system shall** estimate the body equivalent thickness.

- **Rationale**: Thickness is a primary determinant of SPR (Kyriakou & Kalender 2007, *Phys Med* 23(1):3-15 — flat-detector CT, thickness is a simulation input)
- **Open (#180)**: no verified source supports estimating thickness **from kVp, mAs, SID and field size**. The estimation method must be chosen from image-based approaches before implementation; this requirement's method is not settled
- **Verification**: Test
- **Status**: **Partial** — 영상 기반 역산 `L = -ln(P/I0) = mu(t)·t`(`ThicknessFromLogAtten`, `GsvgVirtualGridKernel.ThicknessInversionRoundTrips`). **요구 원문의 방법(kVp·mAs·SID·조사야 크기)과 다릅니다** — 방법 미확정(Open)

### REQ-GSVG-010: SPR Calculation

**When** body thickness and exposure parameters are available,
**the system shall** calculate the Scatter-to-Primary Ratio (SPR).

- **Rationale**: SPR determines scatter correction strength
- **Verification**: Test
- **Status**: **Implemented (tested)** — 커널 합(무한 조사야 SPR)과 반복 갱신, `GsvgVirtualGridCap.KernelSumIsTheDefaultCap`

### REQ-GSVG-011: Scatter Distribution Estimation

**When** SPR is calculated,
**the system shall** estimate scatter distribution using pre-computed scatter kernel LUT.

- **Rationale**: MC-based LUT enables real-time processing with physical accuracy. Established basis: thickness-adaptive kernel superposition (Sun & Star-Lack 2010, *Phys Med Biol* 55(22):6695-6720); a four-Gaussian kernel outperforms two-Gaussian (Bhatia et al. 2017, *J X-Ray Sci Technol* 25(4):613-628)
- **Verification**: Test
- **Status**: **Implemented (tested)** — `ScatterEstimate`(표의 gauss4 커널 중첩, 축소 격자), `…ConvolutionKeepsTheTableNormalisation`, `GsvgVirtualGridTable.Gauss4RowsWinOverGauss2`

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
- **Status**: **Partial** — 표 범위를 넘으면 표 최대로 제한하고 비율을 보고, 범위 아래는 0 으로 페이드(`GsvgVirtualGridRange.*`). **빠진 것: 10–30 cm 두께별로 출력이 유효한지 확인하는 시험**

### REQ-GSVG-018: No Artifacts from Overcorrection

**When** virtual grid processing completes,
**the system shall** not introduce artificial artifacts in anatomical structures.

- **Rationale**: Overcorrection artifacts can cause misdiagnosis (HAZ-003)
- **Verification**: Test + Review
- **Status**: **Partial** — 상한(CapMode GlobalSum)으로 과보정을 막습니다(`GsvgVirtualGridFalsify.SprCapPreventsOvercorrection`). **계단 경계에서 17–40% 덜 뺍니다**(QA-B-95) — "인공물 없음" 의 합격 기준 결정 대기

---

## 4. Performance Requirements

### REQ-GSVG-019: Processing Time

**When** a 3072x3072 16-bit image is processed,
**the system shall** complete processing within 1.0 seconds (Tier 1 DWT, Intel i7 or equivalent).

- **Rationale**: Clinical workflow delay minimization (HAZ-006)
- **Verification**: Test
- **Status**: Measured (2026-09-18, QA-B-102/B-103, 개발 PC i7-12700, 제품 기본 스레드 설정) — **713–757 ms**, 요구 충족. CI 는 836 ms(추세 관찰용). 조건의 Tier 1 DWT 는 구현돼 있습니다(#180).

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

### REQ-GSVG-022: Original Image Protection

**When** any algorithm failure occurs,
**the system shall** not corrupt the original input image.

- **Hazard**: HAZ-001
- **Verification**: Test

### REQ-GSVG-023: DICOM Processing Mark

**When** an image is processed,
**the system shall** record a "Processed" marking in DICOM tags.

- **Hazard**: HAZ-002
- **Verification**: Test

### REQ-GSVG-024: Fail-Safe Pass-Through

**When** processing fails,
**the system shall** return the original image unmodified with an error code.

- **Hazard**: HAZ-001
- **Verification**: Test

### REQ-GSVG-025: SPR Clamping

**When** scatter correction strength exceeds physical maximum,
**the system shall** clamp to the physical limit.

- **Hazard**: HAZ-003
- **Verification**: Test

### REQ-GSVG-026: Output Value Range

**When** output pixel values are computed,
**the system shall** ensure all values are within valid DICOM range (0~65535).

- **Hazard**: HAZ-004
- **Verification**: Test

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
