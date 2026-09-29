# QA-A-141 단계 B — 남은 것 처리

Lane A (pre), `dev/preprocess`, 커밋 `60e7db9`. 단계 A 는 `stage-a-survey.md`.

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 751
CTEST_EXIT=0
DLL build/ci-preprocess/bin/xpe_preprocess.dll  17:51:42

한 프로세스 네 순서:  ran=682 / 682 / 682 / 682,  실패 0, 위생 위반 0
```

`ran=` 680 → 682 는 **새 시험 2건**입니다(3번째 단언은 기존 게이트 시험 안에 들어갔습니다).

## 처리 결과

| 항목 | 결과 |
|---|---|
| `#194` ② 선량 단위 | **완료** — ADU 로 못 박고 한계·출구 조건 기록 |
| `#144` 게이트 유예 | **완료** — 절대 예산으로 재개 |
| `#144` 마지막 1.1배 | **닫지 못했습니다** — 시도했고 악화해 되돌렸습니다 |
| `#140` (1) 두 필드 · (3) 비교 3종 | **이 자리에서는 불가** — 검증했습니다 |
| `#140` (4) | 짓지 않음 (QA-A-135 판정 유지) |

---

## #194 ② — ADU 로 못 박음

### 무엇이 틀려 있었나

`preprocess_api.h:402` 가 `dose_levels` 를 *"mGy or relative units"* 라고 적었습니다. **구현은 mGy 를 받아들일 수 없습니다** — 적용기가 **화소값 그 자체로** 곡선을 색인하고(`gain_correct.cpp`), 기록된 `[dose_min, dose_max]` 클램프도 화소값과 비교합니다. 가로축은 **구성상 ADU** 입니다.

문서가 코드와 어긋난 채로 두면 그 문장을 믿고 mGy 로 적합한 파일을 넣는 사람이 생깁니다 — **조용히 틀리는 쪽**입니다.

### 무엇을 했나

`xpe_calib_generate_gain_polynomial` 의 문서에 UNITS 절을 넣고, **이것이 사지 못하는 것**을 같은 자리에 적었습니다:

> *"Writing "ADU" here does not make a wrong unit detectable: the XCal file carries no dose-unit field, so a file fitted in mGy loads, applies, and produces a WRONG IMAGE WITHOUT ANY ERROR."*

그리고 출구 조건을 붙였습니다:

```
@MX:DEBT: dose unit is documented, not enforced
@MX:CEILING: no dose-unit field exists in the XCal header; every file is
             assumed ADU and a mis-united file cannot be distinguished
@MX:UPGRADE: when #151 supplies real-detector calibration files, check what
             unit they actually carry; if any is not ADU, add the header
             dose-unit field (#194 option 1) and decide how pre-field files
             are read
```

### 범위 타당성 — 새 검사를 만들지 않았습니다

리더가 선택 사항으로 준 것입니다. **별도 스캔 대신 이미 있는 클램프 알림을 갈랐습니다.**

근거: 범위가 화소값 영역과 전혀 안 겹치면 **전 화소가 클램프**됩니다. 그 사실은 이미 건수로 세어지고 있었고, 빠져 있던 것은 *"포화 화소 몇 개"* 와 *"아무것도 범위 안에 안 들어왔다"* 를 읽는 사람이 구별할 수 없다는 점뿐이었습니다.

```
clamped_count == n  →  "ALL %zu pixel(s) fell outside ... the whole frame was
                        evaluated at one range edge, so the gain applied is
                        effectively constant. Check that the calibration's
                        dose levels are in pixel values (ADU): a file fitted
                        in other units loads without error and produces this"
그 외              →  기존 건수 문구 그대로
```

**비용은 프레임당 비교 1회**입니다 — 건수가 이미 있으므로.

주석에 한계를 못박았습니다: *"THIS IS A COARSE MIS-LOAD CHECK, NOT A UNIT CHECK."* mGy 를 ADU 와 구별하지 못하고, 정당하게 좁은 ADU 사다리도 같은 신호를 내며, 우연히 화소 범위와 겹치는 틀린 단위는 아무 신호도 안 냅니다. *"nothing landed in range"* 를 말할 뿐입니다.

### 단언과 반증

| 시험 | 단언 |
|---|---|
| `AFrameEntirelyOutOfRangeSaysSo` | D_min=14037 인 사다리에 균일 5 프레임 → `"ALL "` 과 `"pixel values (ADU)"` 가 알림에 있다 |
| `APartialClampKeepsThePlainWording` | **대조군** — 한 화소만 범위 밖이면 평문 유지, `"ALL "` 없음 |

```
[  PASSED  ] 14 tests.   (GainPolyDoseRangeTest)
```

**반증** — 새 문구를 무조건 찍게 하면(`clamped_count == n || <런타임 참>`):

```
BUILD_EXIT=0
DLL 17:49:42

[  FAILED  ] GainPolyDoseRangeTest.APartialClampKeepsThePlainWording
[  PASSED  ] 13 tests.
```

**대조군 하나만 빨강입니다.** 대조군이 없었으면 문구를 무조건 찍어도 통과했을 것입니다.

---

## #144 — 게이트 재개 (절대 예산)

### 왜 비율이 아니라 절대인가

유예 사유(QA-A-119)는 *"한 기계에서 비율 2.06 > 잡아야 할 회귀 1.496"* 이었고, 그것은 **비율의 문제**이지 이 경로를 막지 말자는 것이 아니었습니다. 절대 상한에는 분모가 없어 그 고장이 없습니다.

### 400 ms 의 출처 — **기록된 CI 실측**

| 값 | 출처 |
|---|---|
| 검출기 **93.3 ms** · **119.6 ms** (Xeon 6973P-C, CI) | **QA-A-119 기록 — 제가 재현하지 않았습니다** |
| 검출기 65.6 ms (i7-12700, 로컬) | QA-A-141 실측 |
| 잡아야 할 회귀: 옛 스칼라 경로 **1340.9 ms** (CI) | QA-A-59 기록 |

400 ms 는 **기록된 CI 최악값의 3.3배**이고 **잡아야 할 회귀의 3.4배 아래**입니다. 리더 규칙(*"CI 중앙값 ×4, 단 400 이하"*)에서 ×4 는 426 이 되어 **400 상한이 먼저 걸립니다.**

**비율은 계속 측정·출력합니다** — `#179` 의 복구 조건이 요구하는 증거가 매 실행 쌓입니다.

### 유예 기록은 지우지 않았습니다

원문(*"유예된 동안 실제 저하가 저항 없이 main 에 들어간다"*)을 그대로 두고 아래에 해제 사유를 덧붙였습니다. **비율 단언은 여전히 유예**이고, 바뀐 것은 그 대가를 더 치르지 않아도 된다는 것뿐입니다.

### 실행과 반증

```
[perf-gate-ratio] 3072 ratio=1.308 limit=1.450  (진단, 유예 유지)
[perf-gate-abs]   3072 best=66.2 ms budget=400.0 ms
[       OK ] RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio
```

**반증** — 예산을 40 ms 로 내리면:

```
BUILD_EXIT=0
[perf-gate-abs] 3072 best=69.2 ms budget=40.0 ms
3072x3072 runtime detection took 69.1686 ms, over the 40 ms ceiling.
[  FAILED  ] RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio
```

---

## #144 마지막 1.1배 — **닫지 못했습니다.** 시도와 실패를 적습니다

### 어디에 시간이 있는지 먼저 쟀습니다

임시 프로브(측정 후 제거)로 게이트와 **같은 프레임**(`mt19937(20260912)`, `N(3000, 10)`)을 써서:

```
[a141-probe] whole=70.5 ms  ComputeGlobalSigma=53.8 ms  (76.4% of whole)
```

**남은 격차(65.6 → 60 ms, −5.6 ms)가 통째로 이 함수 안에 있습니다.** 이 함수의 10% 만 줄여도 목표를 넘습니다.

> 첫 측정은 제 합성 자료(`100 + i%97`)로 해서 whole=130.2 ms 가 나왔고 게이트의 65.6 과 어긋났습니다. **자료가 다르면 비교가 안 된다**는 것을 보고 게이트의 프레임 생성을 그대로 복제해 다시 쟀습니다.

### 가설과 그 결과

`SelectKthSmallest` 는 65536칸 히스토그램을 씁니다 — **256 KB, 이 기계 L1 은 48 KB.** 9.4M 산포 증가가 전부 L1 을 놓칩니다. 11/11/10 비트 3단으로 쪼개면 표가 8 KB 가 되어 L1 에 들어갑니다.

정확성은 보존됩니다 — 같은 2단계 계수 선택이고 자릿수 분해만 바꾼 것이며, `-0.0`/`+0.0` 순서는 `FloatSortKey` 에서 오지 라딕스에서 오지 않습니다. 패리티 시험 **59건 전부 통과**했습니다.

**그런데 느려졌습니다:**

| | ComputeGlobalSigma | whole |
|---|---|---|
| 기존 (16/16, 2패스) | **53.8 ms** | 70.5 ms |
| 시도 (11/11/10, 3패스) | **82.7 ms** | 94.5 ms |
| 되돌린 뒤 | 51.2 ms | 67.5 ms |

**1.54배 악화.** 입력을 세 번 흘리는 비용이 L1 이득보다 컸습니다 — 이 함수는 산포 병목이 아니라 **스트리밍 병목**입니다.

**되돌렸습니다.** 가설이 틀렸다는 것이 이 시도의 산출물이고, 다음 사람이 같은 것을 다시 해 보지 않도록 여기 적습니다.

### 남은 방향 (착수하지 않음)

`ComputeGlobalSigma` 는 수평·수직 차분 배열 **둘**을 만들고 각각에 선택 **2회**씩, 합 **4회 선택 × 2패스 = 8회 스트리밍**입니다. 패스 수를 줄이는 쪽(둘 중 하나만 재고 조건부로 나머지를 재는 등)이 남은 방향인데, **결과가 바뀔 수 있어** 카드 범위에서 결정할 일이 아닙니다.

---

## #140 (1)(3) — 이 자리에서는 불가. **물려받지 않고 확인했습니다**

`SRS-CALIB-001:278-292` 에 이미 판정이 적혀 있습니다. **인용만 하지 않고 직접 확인했습니다:**

### (1) `acquisition_duration_s` · `detector_temperature_c`

두 진입점 시그니처를 읽었습니다:

```
:318  xpe_calib_generate_gain(flat_frames, num_frames, dark_reference,
                              output_path, metadata_json)
:414  xpe_calib_generate_gain_polynomial(gain_file_paths, dose_levels,
                                         num_levels, max_degree, output_path)
```

**취득 시간도 검출기 온도도 어느 쪽에도 없습니다.** 다항 경로에는 `metadata_json` 조차 없습니다. SRS 의 판정 — *"생성 함수가 자기 실행 시간을 `acquisition_duration_s` 에 넣는 것은 이름과 다른 값을 그 자리에 넣는 일"* — 이 맞습니다.

### (3) `dark_bias_delta` · `prnu_delta_pct` · `defect_count_delta`

요구는 *"When overwriting an existing calibration file"* 의 **이전 교정과의** 비교입니다.

| 필요한 것 | 있나 |
|---|---|
| 이전 교정 **파일**을 읽는 기구 | **없습니다** |
| 결함 맵 입력 | **없습니다** — 어느 시그니처에도 |
| 이전 오프셋 교정 | **없습니다** — `dark_reference` 는 *이번* 것입니다 |

**`previous_r_squared` 가 있으니 기구가 있는 것 아니냐**는 것이 가장 그럴듯한 반론이라 그것부터 봤습니다. `xpe_calib_mode.cpp:254` 를 읽으면:

```cpp
const double previous = g_quality_meta.r_squared;   // 프로세스 메모리
```

**이전 *파일*이 아니라 이 프로세스의 직전 생성 호출값**입니다. 파일을 읽는 기구는 없습니다. 그리고 SRS 가 짚듯 `previous_r_squared` 는 **요구가 나열한 세 필드 어디에도 없습니다.**

### 판정

셋 다 *"구현하지 않았다"* 가 아니라 **"이 자리에서는 할 수 없다"** 입니다. 여기서 구현하면 API 서명 변경(공개 API)이거나, 게인 생성 함수가 오프셋·결함 산출물까지 읽는 층 위반입니다.

**SRS 의 권고 — 요구를 교정 전체를 조율하는 층으로 재배치 — 에 동의하며, `docs/` 는 리더 소유라 보고만 합니다.**

---

## 미검증

- **CI 에서 아무것도 돌리지 않았습니다.** 400 ms 의 근거인 Xeon 수치 둘은 **QA-A-119 기록을 읽은 것**이지 제가 잰 것이 아닙니다. 새 절대 단언이 CI 에서 실제로 통과하는지 확인되지 않았습니다 — 여유가 3.3배라 낮은 위험으로 봅니다.
- **`#179` 의 유예 조건이 지금도 성립하는지** 확인하지 않았습니다. 비율 단언은 그대로 유예로 뒀습니다.
- **전 화소 클램프 문구가 gui 화면에 어떻게 보이는지** 확인하지 않았습니다 — `gui/` 소유.
- **`ComputeGlobalSigma` 를 프로파일러로 쪼개지 않았습니다.** 함수 단위 타이밍만 있고, 8회 스트리밍 중 어디가 비싼지는 재지 않았습니다.
- 3단 라딕스가 **다른 기계에서도** 느린지 확인하지 않았습니다 — 이 기계(i7-12700) 한 대입니다.
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **400 ms 가 CI 에서 처음 걸리는 값이면 main 이 빨개집니다.** 기록된 최악값의 3.3배라 낮게 보지만, 기록에 없는 더 느린 러너가 붙으면 그렇습니다. 그때 **상수를 올리기 전에 절대값을 읽으라**고 실패 메시지에 적어 뒀습니다.
- **`@MX:DEBT` 의 출구 조건이 `#151` 에 매여 있습니다.** `#151` 이 보류 상태로 오래 가면 DEBT 도 그만큼 남습니다 — 출구 조건이 있다는 것이 썩지 않는다는 뜻은 아닙니다.
- **1.1배는 그대로입니다.** 절대 게이트는 회귀를 막을 뿐 목표를 당기지 않습니다.
