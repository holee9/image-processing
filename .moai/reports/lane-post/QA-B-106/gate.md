# QA-B-106 게이트 — GSVG 요구별 상태, 메모리, B-105 미검증 3건 (#180)

커밋 `f433afa`(시험 2건 추가), 미푸시. SPEC 은 고치지 않았다(리더 소유).

## 1. 주장 — 요구별 현재 상태

판정 규칙: **시험이 없는 항목은 "구현됨" 으로 적지 않았다.** 근거가 코드 읽기뿐이면 그렇게 적었다.
경로는 이 워크트리 기준이다(`modules/gsvg/…`).

### 그리드 억제 (001–008)

| 요구 | 상태 | 근거(파일:줄) | 확인하는 시험 |
|---|---|---|---|
| 001 격자 주파수를 **DICOM 헤더에서** 자동 산출 | **미구현** | 입력은 화소와 JSON 설정뿐이다(`include/xpe/gsvg/gsvg_api.h:186` 인자 목록). 주파수는 영상 스펙트럼에서 찾는다(`src/grid_dwt.cpp:232` `DetectInputGrid`) | 없음(DICOM 경로 자체가 없다) |
| 002 2D DWT 다중 스케일 분해 | **구현됨** | `src/grid_dwt.cpp:183` `Dwt2`, `:211` `Idwt2`(db4 8탭) | `GsvgGridSuppression.Db4DwtReconstructsPerfectly` |
| 003 서브밴드별 격자 에너지 검출 | **구현됨** | `src/grid_dwt.cpp:272` `CheckSubband`(평균 + 3σ), 예측 빈 게이트 | `GsvgGridSuppression.SubbandPlacementMatchesHandValues`, `…WithoutInputGateGridFreeImagesChange` |
| 004 가우시안 대역 차단 | **구현됨** | `src/grid_dwt.cpp` `BandStop`(σ = 1.5 빈) | `GsvgGridSuppression.GridIsSuppressedByAtLeast40dB`, 반증 `…WithoutBandStopOnlySuppressionFails` |
| 005 격자 잔상이 보이지 않을 것 | **부분** | 억제는 동작한다(위). 다만 잔여 격자 에너지가 격자 없는 기준선의 **21–381배**로 남는다 | `GsvgGridSuppression.KnownDivergence_ResidualStaysAboveTheGridFreeBaseline` |
| | | **빠진 것**: "보이지 않는다" 의 판정 기준이 없다. 현재 시험은 40 dB 감쇠와 잔여 비율을 재는 것뿐이고, 사람이 보는 판정(Review)은 하지 않았다 | |
| 006 MTF 저하 < 5% | **부분** | 경계의 법선 방향으로 놓인 선은 5% 미만 | `GsvgGridSuppression.MtfLossStaysUnderFivePercentForLinesAlongTheEdgeNormal` |
| | | **빠진 것**: 경계를 가로지르는 선은 **11% 손실**로 요구를 넘는다 | `GsvgGridSuppression.KnownDivergence_LinesAcrossAnEdge` |
| 007 60–200 lines/inch 처리 | **구현됨** | 세 주파수 모두 검출·필터된다 | `GsvgGridSuppression.GridIsSuppressedByAtLeast40dB`(`kLpis = {60, 103, 200}`, `test_grid_suppression.cpp:32`) |
| 008 Moire(에일리어싱) 제거 | **부분** | 200 lpi 는 에일리어싱된 주파수에서 검출·제거된다(007 과 같은 시험). 170–186 lpi 구간은 **기록만** 한다 | `GsvgGridSuppression.ReportSevereAliasing`(단언 없음) |
| | | **빠진 것**: 심한 에일리어싱 구간의 합격 기준이 없다 | |

### 가상 그리드 (009–018)

| 요구 | 상태 | 근거 | 시험 |
|---|---|---|---|
| 009 체두께 추정 | **부분** | 영상 기반으로 추정한다: `L = -ln(P/I0) = mu(t)·t` 역산(`src/virtual_grid.cpp` `ThicknessFromLogAtten`) | `GsvgVirtualGridKernel.ThicknessInversionRoundTrips` |
| | | **빠진 것**: 요구 원문의 조건(kVp·mAs·SID·조사야 크기로 추정)과 방법이 다르다. SPEC 자체가 "방법 미확정(Open)" 으로 적고 있다 | |
| 010 SPR 계산 | **구현됨** | 커널 합(무한 조사야 SPR)과 반복 갱신(`virtual_grid.cpp` `SprCapAt`, `spr[i]`) | `GsvgVirtualGridCap.KernelSumIsTheDefaultCap` |
| 011 커널 LUT 로 산란 분포 추정 | **구현됨** | `virtual_grid.cpp` `ScatterEstimate`(표의 gauss4 커널 중첩, 축소 격자) | `GsvgVirtualGridKernel.ConvolutionKeepsTheTableNormalisation`, `…NodesBlendsAndBounds`, `GsvgVirtualGridTable.Gauss4RowsWinOverGauss2` |
| 012 산란 차감 | **구현됨** | `P = I / (1 + SPR)`, 최종 `out = P + (Ts/Tp)·S` | `GsvgVirtualGridMc.CompareToPrimary`(MC 팬텀 대비) |
| 013 라플라시안 피라미드 대비 | **구현됨** | `virtual_grid.cpp:583` `PyramidContrast`(4–8단) | `GsvgVirtualGridPyramid.UnitGainIsIdentityAndGainRaisesDetail` |
| 014 잡음 제거 | **구현됨** | 최상위 대역 소프트 문턱(`denoiseK`) | `GsvgVirtualGridPyramid.DenoiseLowersFlatRegionNoise` |
| 015 CNR ≥ 물리 격자 6:1 의 90% | **미구현** | 물리 격자 기준 영상도, CNR 측정 코드도 없다 | 없음 |
| 016 격자비 6/8/10/12 지원 | **구현됨** | 표에서 (격자비, 선밀도) 설계를 고른다(`virtual_grid.cpp` `SelectGrid`), 제품 표에 네 비율 모두 있다 | `GsvgVgProductTable.LoadsWithEveryRatioOfReqGsvg016`, `GsvgVirtualGridRatio.ResidualSprFallsWithRatio` |
| 017 두께 10–30 cm 에서 유효한 출력 | **부분** | 표 범위를 넘으면 표 최대로 제한하고 비율을 보고한다. 범위 아래는 0 으로 페이드 | `GsvgVirtualGridRange.ThickRegionIsLimitedAndReported`, `…ThinRegionIsCountedNotRefused` |
| | | **빠진 것**: "10–30 cm 에서 출력이 유효하다" 를 두께별로 확인한 시험이 없다. 제품 표의 커널 두께 노드는 5–30 cm 다 | |
| 018 과보정 인공물 없음 | **부분** | 상한(CapMode GlobalSum)으로 과보정을 막는다 | `GsvgVirtualGridFalsify.SprCapPreventsOvercorrection` |
| | | **빠진 것**: 계단 경계에서 실제보다 **17–40% 덜 빼는** 편차가 남아 있고(QA-B-95 기록), "인공물 없음" 의 판정 기준이 없다 | `GsvgVirtualGridFalsify.GlobalThicknessIsWorseAtTheStep` |

### 성능·자원 (019–021)

| 요구 | 상태 | 근거 | 시험 |
|---|---|---|---|
| 019 억제 1.0 s | **충족(측정)** | 로컬 493 ms(자동), CI 539 ms | `GsvgGridSuppression.BenchmarkFreeze_Performance_REQ_GSVG_019_GridDwt3072` |
| 019b 가상 그리드 1.0 s | **충족(측정)** | 로컬 488 ms(자동), CI 492 ms | `GsvgVirtualGridBench.BenchmarkFreeze_Performance_REQ_GSVG_019_VirtualGrid3072` |
| 020 최대 메모리 ≤ 512 MB | **억제 충족 / 가상 그리드 초과** | §2 | 없음(이번에 임시로 쟀다) |
| 021 메모리 누수 0 | **구현됨(측정)** | 1000 주기 CRT 힙 워크, 가짜 누수 대조 2건 | `GsvgEndurance.ThousandCycles_CrtHeapDoesNotGrow`, `…ControlLeak_LargeBlockIsCaught`, `…ControlLeak_SmallBlockPerCycleIsCaught` |

### 안전 (022–026)

| 요구 | 상태 | 근거 | 시험 |
|---|---|---|---|
| 022 원본 영상 보호 | **구현됨** | 실패 시 원본 복원(`src/gsvg.cpp` `original` 복사 → `memcpy`), 입력 버퍼 불변 | `GsvgAbiSmoke.Lifecycle3072_VignetteAndGrid_OutputClampedAndSourceIntact`, `GsvgVirtualGridApi.ProcessesAndKeepsTheOriginalOnFailure` |
| 023 DICOM "Processed" 표시 | **미구현** | gsvg 는 DICOM 태그를 읽지도 쓰지도 않는다(공개 API 인자에 DICOM 이 없다) | 없음. 이 요구는 dicom 모듈 또는 호출자의 책임으로 보인다 — 판단은 리더 |
| 024 실패 시 원본 + 오류 코드 | **구현됨** | 가상 그리드 거부 시 `XPE_ERR_CONFIG_INVALID` + 원본, 사유는 alert 큐(`gsvg.cpp`) | `GsvgVirtualGridApi.ProcessesAndKeepsTheOriginalOnFailure`, `GsvgVirtualGridRatio.UnknownRatioIsRefusedAndImageKept`, `GsvgResult.VirtualGridAppliedAndRefused` |
| 025 SPR 상한 | **구현됨** | CapMode 5종, 기본 GlobalSum(`virtual_grid.h` `CapMode`) | `GsvgVirtualGridCap.*`, `GsvgVirtualGridCapChoice.CompareCandidates`, 반증 `GsvgVirtualGridFalsify.SprCapPreventsOvercorrection` |
| 026 출력 0–65535 | **구현됨** | 비네팅·가상 그리드 모두 clamp(`gsvg.cpp`), 초과 화소 수를 보고 | `GsvgCoverage.VignetteGainRoundsAndClampsToUint16Range` |

**요약**: 구현됨 13, 부분 7(005·006·008·009·017·018 + 020 의 억제 쪽), 미구현 3(001·015·023). 019·019b·021 은 측정으로 충족.

## 2. 메모리 — REQ-GSVG-020

**요구 원문**(`.moai/specs/SPEC-XPE-GSVG/spec.md:237-240`)

```
### REQ-GSVG-020: Peak Memory
When processing any single frame,
the system shall use no more than 512 MB peak memory.
```

**재는 방법**
- `GetProcessMemoryInfo` 의 `PROCESS_MEMORY_COUNTERS_EX.PrivateUsage`(프로세스 **커밋 차지**)를 본다. OS 에 뒤를 받아 달라고 요청한 주소 공간이다.
- **작업 집합(working set)은 쓰지 않았다.** 상주 여부는 시스템 압력에 따라 움직여서 호출이 무엇을 필요로 하는지의 근거가 되지 않는다(#181 교훈).
- 호출당 몫을 가르려고, 준비 호출 1회 뒤의 커밋을 기준선으로 잡고 **호출 중에 0.5 ms 간격으로 표본**을 뜨는 스레드를 띄워 최댓값을 잡았다. 보고값 = 최댓값 − 기준선.

**측정 (3072², 제품용 표)**

| 경로 | 스레드 1 | 자동(4) | 판정 |
|---|---|---|---|
| 그리드 억제 | 385.0 MB | 384.2 MB | 512 MB 안 |
| 가상 그리드 | 626.4 MB | 626.6 MB | **초과 (+114 MB)** |

- **병렬화로 늘어난 몫은 없다.** 스레드 1 과 자동의 차이가 억제 0.8 MB, 가상 그리드 0.2 MB 로 표본 잡음 수준이다. 스레드마다 잡는 버퍼는 작다(억제의 밴드 분할은 공용 버퍼를 나눠 쓰고, 가상 그리드도 마찬가지).
- 초과는 **병렬화 이전부터 있던 것**이다.
- 어디서 나오는지(코드 읽기): 3072² double 버퍼 하나가 75.5 MB 다. 가상 그리드는 `img`(입력 복사), 산란 `Sf`, 잔여 비율 `Rf`, `out`, 피라미드 0단 평면들(`g[0]`, `lap[0]`, `up`)이 모두 전 해상도 double 이다. 여기에 원본 uint16 복사(18.9 MB)와 축소 격자 버퍼가 더해진다. **정확한 내역은 측정하지 않았다**(§4).

## 3. B-105 미검증 3건

1. **마스크 경로** — `GsvgThreads.MaskedPathsAreBitIdenticalForEveryThreadCount` 추가. `xpe_gsvg_process_masked` 와 `_ex` 를 스레드 1·2·3·4·8·자동에서 비교해 바이트 동일. 마스크 밖 화소가 입력 그대로인 것과 `_ex` 의 결과 구조체(적용됨/사유 0)도 함께 단언한다.
2. **제품 표가 아닌 설정** — `GsvgThreads.SyntheticTableAndOtherRatioAreAlsoIdentical` 추가. 합성 표 + 격자비 6 + 화소 간격 1.0 + 반복 5 에서도 스레드 2·4·자동이 1 과 같다.
3. **CI 값** — 아래.

| 항목 | 병렬화 전(3a39206) | **1efa2d0 포함 실행(e10e0bd, run 35282869001)** | 로컬(자동) |
|---|---|---|---|
| 억제 | 1507 ms | **539 ms** | 493 ms |
| 가상 그리드 | 598 ms | **492 ms** | 488 ms |

- CI 에서도 두 경로 모두 요구(1.0 s) 안이다.

## 4. 미검증

- 메모리 내역(어느 버퍼가 몇 MB인지)은 측정하지 않았다. 코드 읽기 기반 추정만 적었다.
- 메모리는 3072² 한 크기, 제품 표 한 설정에서만 쟀다. 피라미드를 끄면 줄어들 것으로 보이나 재지 않았다.
- 측정은 커밋 차지다. 실제 물리 메모리 사용(상주)이나 단편화는 보지 않았다.
- 표의 "구현됨" 은 해당 시험이 그 동작을 실제로 확인한다는 뜻이다. 요구 문장 전체(임상적 판정, 사람 검토)를 덮지는 않는다.
- 005·008·018 의 "보이지 않을 것 / 인공물 없음" 은 판정 기준이 없어 시험으로 고정할 수 없었다.
- 023 의 소유 모듈(dicom 인지 호출자인지)은 판단하지 않았다.
- CI 값은 1회 실행이다.

## 5. 잔여 위험

- **REQ-GSVG-020 을 가상 그리드가 넘는다(626 MB > 512 MB).** 콘솔 PC 메모리 제약이 근거인 요구이므로, 줄이거나 요구를 고쳐야 한다. 줄이는 방향(판독만): 전 해상도 double 버퍼를 float 로 내리거나(출력이 바뀐다), 피라미드 입력을 제자리로 쓰거나, 산란·잔여 비율을 축소 격자에서 합쳐 한 버퍼로 만든다.
- 부분 7건 중 005·006·008·018 은 모두 "기준이 없어서 부분" 이다. 합격 기준을 정하지 않으면 영원히 부분으로 남는다.
- 001·023 은 gsvg 밖(DICOM 경로)의 결정이 필요하다.

## Card Cross-Check

| 카드 항목 | 결과 |
|---|---|
| 요구 001–025 상태·근거·시험 | §1 (026 도 함께 적었다) |
| 부분이면 무엇이 빠졌는지 | §1 각 항목의 "빠진 것" 줄 |
| KnownDivergence 3건을 해당 요구에 | 005(21–381배), 006(11%), 018(17–40%) |
| 메모리 요구 인용·측정·방법 | §2 |
| 병렬화로 늘어난 몫 | §2, 없음 |
| 마스크 경로 시험 | §3 (1), `f433afa` |
| 제품 표가 아닌 설정 | §3 (2) |
| CI 값 | §3 (3) |
| BUILD_EXIT | BUILD=1 1건(heredoc `\n` 파손) 폐기 후 재측정, 나머지 0 |
