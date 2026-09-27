# QA-A-141 단계 A — pre 소유 미차단 5건 전제 훑기

Lane A (pre), `dev/preprocess`, `origin/main` 병합(`15e8552`). **이 단계에서 제품 코드는 바꾸지 않았습니다** — 대조군 패치는 전부 측정 후 원복했고, 아래 기준선이 그 증거입니다.

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 749
CTEST_EXIT=0
DLL build/ci-preprocess/bin/xpe_preprocess.dll  17:38:26  (원복 후 재빌드됨)
```

## 판정표

| # | 이슈가 주장하는 결함 | 아직 재현되는가 | 판정 |
|---|---|---|---|
| **187** | 다항식 적재하면 게인 보정이 `CALIB_NOT_LOADED` | **아니오** | **(A) 이미 해결** |
| **194** | ① 적합 범위 밖 무제한 외삽 ② 선량 단위 미강제 | ① 아니오 ② **예** | **(B) 절반** |
| **196** | 비선형 단계가 저장소 자신의 설정을 거부 | **아니오** | **(A) 이미 해결** |
| **140** | R² 게이트가 API 만 있고 생성 경로에 미배선 | **아니오** | **(B) 배선은 됨, FUNC-033 잔여 3항목** |
| **144** | 스칼라 1024² 1.3 s / 3072² 12 s, AVX2 배선 미확인 | **아니오** | **(B) 수치는 죽었고 1.1배와 유예된 단언이 남음** |

**(C) 그대로 인 건은 하나도 없습니다.** 다섯 건 모두 그 사이에 손이 갔습니다.

---

## #187 — (A) 이미 해결

### 현재

```
$ xpe_preprocess_tests --gtest_filter='GainPolyNotAppliedTest.*'
[  PASSED  ] 10 tests.
```
그중 직접 해당하는 것: `PolynomialGainLoadsAndIsAppliedWithoutAlerts`, `PolynomialLoadReplacesAWorkingScalarMapAndAppliesInstead`, `PolynomialGainIsAppliedPerPixelUsingThePixelsOwnValue`, `TheModelLoadedLastIsTheOneApplied`.

### 대조군 — 고침을 되돌리면 **이슈의 증상 그대로** 재현됩니다

`gain_correct.cpp:306` 의 다항식 분기 조건에 컴파일러가 접을 수 없는 거짓항을 붙여, 다항식이 적재돼도 `else if (!g_calib.gain_map) return XPE_ERR_CALIB_NOT_LOADED;` 로 떨어지게 했습니다 — **QA-A-121 이전 상태의 충실한 복원**입니다.

```
BUILD_EXIT=0
DLL 17:36:14   (재빌드 확인)

    Which is: -16
[  FAILED  ] GainPolyNotAppliedTest.PolynomialGainLoadsAndIsAppliedWithoutAlerts
    Which is: -16
[  FAILED  ] GainPolyNotAppliedTest.PolynomialLoadReplacesAWorkingScalarMapAndAppliesInstead
    Which is: -16
[  FAILED  ] GainPolyNotAppliedTest.PolynomialGainIsAppliedPerPixelUsingThePixelsOwnValue
    Which is: -16
[  FAILED  ] GainPolyNotAppliedTest.TheModelLoadedLastIsTheOneApplied
[  PASSED  ] 6 tests.
```

**`-16` 은 `XPE_ERR_CALIB_NOT_LOADED`**(`xpe_error.h:63`) — **이슈 제목이 적은 바로 그 코드**입니다. 나머지 6건(스칼라 경로)은 초록이므로 되돌림이 다항식 경로만 건드렸습니다.

> **첫 대조군은 무효였습니다.** `if (!poly.empty())` 쪽을 껐더니 `gainmap` 이 크기를 못 받아 접근 위반(`0xc0000005`)으로 죽었습니다 — 시험은 빨갛지만 **이슈의 증상이 아닙니다.** "되돌리면 빨강" 만 봤으면 통과로 적었을 자리입니다. 충실한 복원으로 다시 쟀습니다.

**닫아도 됩니다.**

---

## #194 — (B) 절반

### ① 적합 범위 밖 외삽 — 해결

```
$ xpe_preprocess_tests --gtest_filter='GainPolyDoseRangeTest.*'
[  PASSED  ] 12 tests.
```

**대조군** — `gain_correct.cpp:354` 의 클램프를 거짓 조건으로 끄면:

```
BUILD_EXIT=0
DLL 17:34:34

[  FAILED  ] GainPolyDoseRangeTest.ASaturatedPixelReceivesTheDMaxGainInsteadOfTheExtrapolatedOne
[  FAILED  ] GainPolyDoseRangeTest.ASaturatedPixelIsNotDarkerThanADMaxPixel
[  FAILED  ] GainPolyDoseRangeTest.ClampingRaisesExactlyOneAlertCarryingTheCount
[  PASSED  ] 9 tests.

같은 실행에서 GainPolyNotAppliedTest: [  PASSED  ] 10 tests.   ← 간섭 없음
```

두 번째 시험 이름이 이슈가 말하는 결과입니다 — **포화 화소가 D_max 화소보다 어두워지는 단조성 역전**이 클램프를 끄면 되살아납니다.

### ② 선량 단위 미강제 — **그대로입니다**

| 확인 | 결과 |
|---|---|
| `dose_unit`·`dose_units`·`kDoseUnit`·`unit.*mGy` 검색 (`modules/` 전체 `.h`·`.cpp`) | **0건** |
| **존재 대조군** — 같은 검색 방식의 `dose_min` | **23건** |
| `preprocess_api.h:402` | `@param dose_levels Array of N dose levels (mGy or relative units)` — **여전히 둘 다 허용한다고 적습니다** |

**손대지 않았습니다.** 이슈 본문 자체가 *"`#151` 뒤에 결정하는 것이 맞"* 다고 적고, 선택지 셋(헤더 필드 추가 / 옛 파일 처리 / ADU 로 못 박고 문서 정정) 중 무엇을 고를지는 **리더 결정**입니다. 제 메모리에도 `#151` 은 보류로 기록돼 있습니다.

**닫을 수 없습니다 — ① 만 해결입니다.**

---

## #196 — (A) 이미 해결

### 현재

```
$ xpe_preprocess_tests --gtest_filter='NonlinModeTest.*:NonlinNoopReportTest.*'
[  PASSED  ] 11 tests.
```

### 대조군 — 거부를 복원하면 **이슈의 측정표가 행 단위로** 재현됩니다

`nonlinearity_correct.cpp` 에 `kKnownModes` 거부를 거짓 조건 없이 되살렸습니다.

```
BUILD_EXIT=0
DLL 17:33:38

[       OK ] NonlinModeTest.ControlAPipelineWithNoModeKeySucceeds        ← 이슈 표의 "mode 키 없음 → rc=0"
    Which is: -4
[  FAILED  ] NonlinModeTest.AnOperatingModeNoLongerFailsThePipeline      ← 이슈 표의 "clinical → rc=-4"
    Which is: -4
[  FAILED  ] NonlinModeTest.AnArbitraryModeNameIsAlsoAccepted
    Which is: -4
[  FAILED  ] NonlinModeTest.DoingNothingIsReported
[       OK ] NonlinModeTest.DoingNothingIsReportedWithNoModeKeyEither
```

**`-4` = `XPE_ERR_CONFIG_INVALID`** — 이슈가 측정한 그 값이고, **대조 행(mode 키 없음)은 초록**입니다. 이슈의 표 자체를 재현한 셈입니다.

그리고 QA-A-140 이 `config == null` 경로까지 열었으므로, GUI 가 쓰는 호출 형태에서도 무동작이 보고됩니다.

**닫아도 됩니다.** (화면까지 닿는지는 gui 소유 — GUI-C-132.)

---

## #140 — (B) 배선은 됨, FUNC-033 잔여 3항목

### 현재

```
$ xpe_preprocess_tests --gtest_filter='CalibQualityMetaTest.*'
[  PASSED  ] 15 tests.
```

### 대조군 — 메타 기록을 막으면 절반이 무너집니다

`xpe_calib_generate_gain.cpp` 의 `xpe_calib_record_quality_meta(quality)` 호출 **두 곳**(단일점 `:316`, 다항식 `:616`)을 거짓 조건으로 단락시켰습니다.

```
BUILD_EXIT=0
DLL 17:33:38

[  FAILED  ] CalibQualityMetaTest.R2QualityGate_Pass_WhenAboveThreshold
[  FAILED  ] CalibQualityMetaTest.R2FallsAsTheSeriesDepartsFromTheFit
[  FAILED  ] CalibQualityMetaTest.GoodFitRaisesNoWarning
[  FAILED  ] CalibQualityMetaTest.PreviousRSquared_Regression
[  FAILED  ] CalibQualityMetaTest.PreviousRSquared_Stable
[  FAILED  ] CalibQualityMetaTest.GeneratedPolyFileKeepsTheFittedDegree_WhenTheFitDropsADegree
[  FAILED  ] CalibQualityMetaTest.GeneratedPolyFileKeepsTheFittedDegree_WhenNoDegreeIsDropped
[  PASSED  ] 8 tests.
```

**15건 중 7건이 그 호출에 매달려 있습니다.** 배선이 장식이 아니라는 뜻입니다.

### 남은 것 — 이슈 본문의 개정판이 이미 적은 셋

QA-A-134·135 가 확정한 것이고 제가 다시 쓰지 않습니다:

| 항목 | 성격 |
|---|---|
| (1) `acquisition_duration_s` · `detector_temperature_c` | **API 서명 변경** — 두 값이 함수까지 오지 않음 |
| (3) `dark_bias_delta` · `prnu_delta_pct` · `defect_count_delta` | 이전 교정과의 비교 계산 |
| (4) `gain_uncertainty` · `per_point_r_squared[]` | **정의 없음** — QA-A-135 가 짓지 않고 멈춘 자리 |

**(4) 는 단계 B 에서도 손대지 않습니다** — 정의가 오기 전에 채우면 제가 고른 해석이 요구가 됩니다.

---

## #144 — (B) 이슈의 수치는 죽었고, 1.1배와 유예된 단언이 남습니다

### 측정 (이 기계, 이 실행)

```
[perf-gate-machine] cpu="12th Gen Intel(R) Core(TM) i7-12700" logical=20 avx2=1 bandwidth=23.9 GB/s
[perf-gate-machine] numerator=xpe_preprocess(/arch:AVX2) denominator=in-test kernel(default arch)
[perf-gate] 3072x3072 single thread, warm, min of 5: 65.6 ms
[perf-gate] 1024x1024 single thread, warm, min of 5:  6.8 ms
[perf-gate-ratio] 3072 ratio=1.300 limit=1.450
[perf-gate] SPEC improvement target 60 ms (AVX2, single thread) -- currently 1.1x the target
[perf-gate-OK] 3072 ratio=1.300 within limit=1.450 (assertion suspended, #179)
```

| 이슈의 주장 | 실측 | 배율 |
|---|---|---|
| 1024² ≈ **1.3 s** | **6.8 ms** | **191배 빨라짐** |
| 3072² ≈ **12 s** | **65.6 ms** | **183배 빨라짐** |
| SPEC `< 35 ms (scalar)` 대비 **340배** | 목표 60 ms(AVX2) 대비 **1.1배** | — |
| *"AVX2 가 이 공개 진입점에 연결돼 있는지 미확인"* | **연결됨** — `avx2=1`, 분자가 `/arch:AVX2` 로 빌드됨 | — |

**이슈 제목의 "목표까지 1.1배" 가 지금 상태를 정확히 말합니다** — 제목만 아직 맞습니다.

### 남은 둘

1. **마지막 1.1배** — 65.6 ms → 60 ms. 최적화 작업입니다.
2. **게이트 단언이 유예 상태** — `test_runtime_detection_performance_gate.cpp:512`, **QA-A-119(#179) 리더 결정**. Xeon 6973P-C 한 대에서 코드 변경 0줄로 비율이 2.06까지 올랐고, 그 기계를 통과시키려면 한계를 **잡아야 할 회귀(1.496)보다 높게** 잡아야 해서 껐습니다. 그 파일이 대가도 적어 뒀습니다 — *"이 단언이 유예된 동안 런타임 검출기의 실제 저하는 아무 저항 없이 main 에 들어간다."*

**단언 재개는 리더 결정이라 제가 되돌리지 않습니다.** 카드도 게이트 수치를 로컬 측정만으로 확정하지 말라고 적었고, 여기서는 수치를 새로 잡는 것이 아니라 **이미 내려진 유예를 되돌리는** 문제입니다.

---

## 단계 B 로 넘어가는 것

**(C) 는 없고, 실제로 남은 것은 셋입니다:**

| 남은 일 | 왜 |
|---|---|
| `#140` (1) 두 필드 · (3) 비교 3종 | 구현 가능. (1) 은 API 서명 변경이라 크기가 다름 |
| `#144` 마지막 1.1배 | 최적화. 카드 지시대로 **마지막** |
| `#194` ② 단위 | **리더 결정 대기** — 손대지 않음 |
| `#140` (4) | **정의 없음** — 짓지 않음 |

## 미검증 / 범위 밖

- **gui 화면까지** 닿는지는 어느 건도 확인하지 않았습니다 — `gui/` 소유.
- **CI 에서의 재현**은 하지 않았습니다. 위 수치는 전부 이 기계(i7-12700)입니다.
- `#179` 의 유예 조건이 지금도 성립하는지(그 Xeon 러너가 여전히 그렇게 도는지) 확인하지 않았습니다.
- `#143` `#148` `#186` 은 차단 상태라 손대지 않았습니다.
- 대조군은 모두 **런타임 거짓 조건**으로 걸었습니다. 처음 시도한 `std::getenv` 는 MSVC `/WX` 의 C4996 에 걸려 `BUILD_EXIT=1` 이었고, **DLL 타임스탬프가 안 바뀐 것**(17:43:31 유지)이 그 눈멂을 드러냈습니다 — 카드 §4 가 경고한 자리입니다. `std::string("off").size() == 99` 로 바꿔 다시 쟀습니다.
