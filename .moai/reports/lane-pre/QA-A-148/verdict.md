# QA-A-148 (#212) — 재봤고, 네 자리 모두 **좁혔습니다**

카드 §1 표의 **첫 행**("네 순서 모두 같은 값 → 그 값 하나로 좁히고 주석 문장 삭제")입니다.
`memleak` 은 좁히긴 했으나 **좁힌 값이 제가 예상한 것과 다릅니다** — 그것도 측정이 알려줬습니다.

---

## 측정 — 네 자리 × 네 순서

`A-147` §1 과 같은 프로브(`std::printf` 로 rc 를 찍고 기본 순서 + shuffle seed 1/2/9).

| 자리 | 기본 | seed 1 | seed 2 | seed 9 | 값 |
|---|---|---|---|---|---|
| `test_preprocess_degraded.cpp:127` (offset) | `-6` | `-6` | `-6` | `-6` | `XPE_ERR_NOT_INITIALIZED` |
| `test_preprocess_degraded.cpp:176` (gain) | `-6` | `-6` | `-6` | `-6` | `XPE_ERR_NOT_INITIALIZED` |
| `test_preprocess_degraded.cpp:223` (defect) | `-6` | `-6` | `-6` | `-6` | `XPE_ERR_NOT_INITIALIZED` |
| `test_xpe_preprocess_memleak.cpp:85` (offset) | `-16` | `-16` | `-16` | `-16` | `XPE_ERR_CALIB_NOT_LOADED` |

`memleak` 자리는 한 실행에서 **2200번** 호출되고(1000 프레임 × 2 시험 + 워밍업),
**2200번 전부 같은 값**이었습니다. 네 순서에서 모두 그렇습니다 — 표본이 작아서 안 갈린 것이 아닙니다.

**갈리지 않았습니다.** 카드 §1 표의 둘째 행(별도 이슈로)은 해당 없습니다.

---

## 1. `test_preprocess_degraded` 세 자리 — 좁힘 + 주석 삭제

`EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, rc)` 로 바꾸고, 문제의 문장을 지웠습니다:

> ~~*"Accept NOT_INITIALIZED (no calibration loaded) or OK (if a prior test populated g_calib).
> Both paths are graceful degradation."*~~

리더 판단에 동의합니다 — **결정의 기록이 아니라 관측의 기록**이었습니다. 지운 자리에 왜 지웠는지를
적었습니다: 이 단언이 `XPE_OK` 를 받는 동안에는 **전역 상태 누수가 일어나도 이 시험이 초록**이고,
그것이 단일 프로세스·셔플 하네스(`#162`·`#176`)가 잡으려는 바로 그 결함입니다. 단언이 먼저 삼켰습니다.

## 2. `test_xpe_preprocess_memleak:85` — 좁혔고, **주석이 코드 이름부터 틀렸습니다**

옛 주석: *"Accepts NOT_INITIALIZED when calibration not loaded"*.

**이 파일은 바로 위(`:75`)에서 `xpe_preprocess_init()` 을 부릅니다.** 그래서 실제로 나는 코드는
`NOT_INITIALIZED`(-6)가 아니라 **`CALIB_NOT_LOADED`(-16)** 입니다 — `#117` 결정 B 가 둘을 구분한
이유가 정확히 이것입니다. 주석은 실행되지 않는 상태를 설명하고 있었습니다.

`EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, rc)` 로 좁혔습니다.

`A-147` 에서 제가 "이 파일은 초기화를 부르니 `XPE_OK` 가 정당할 수 있다" 고 적은 것은 **절반만
맞았습니다** — 초기화는 부르지만 캘리브레이션은 싣지 않으므로 `XPE_OK` 는 나지 않습니다.
재보지 않고 적은 추정이었고, 재보니 달랐습니다. 리더 지적("그것도 재야 압니다")이 맞습니다.

목적이 누수 검출이라 느슨해도 된다는 논리는 쓰지 않았습니다 — 비용은 측정 한 번이었습니다.

---

## 3. 반증 — 좁힌 단언이 무는가

`xpe_offset_correct` 가 런타임-거짓 가드 뒤에서 그냥 `XPE_OK` 를 반환하도록(= **아무 일도 안 하고
성공을 보고**) 만들었습니다. `A-147` §4 와 같은 주입입니다.

```
test_preprocess_degraded.cpp(131): error: Expected equality of these values:
    Which is: 0
[  FAILED  ] PreprocessDegraded.BP01_OffsetNullCalibrationReturnsNotInitialized
```

```
test_xpe_preprocess_memleak.cpp(91): error: Expected equality of these values:
    Which is: 0
```

**둘 다 빨강.** 옛 단언은 `XPE_OK` 를 허용 집합에 갖고 있었으므로 이 주입에 **초록**이었습니다.

`BUILD_EXIT=0`, DLL 타임스탬프 갱신 확인(21:50:29), 확인 후 제거(`grep -c` 0건).

> 두 번 돌렸습니다. 첫 실행에서 `head` 에 memleak 출력만 잡혀 `BP01` 의 빨강을 **보지 못했고**,
> 보지 않은 것을 적지 않으려고 `BP01` 만 다시 돌려 관측했습니다.

---

## 4. `#212` 에 남길 사실 (리더 요청)

`DISABLED_PipelinePerformance3072x3072` 관련 실제 상태를 `#212` 코멘트에 남겼습니다:

- `UncalibratedPipelineComposes`(옛 `FullPipelineSmallImage`)에는 `QA-A-147` 전까지
  **정량 단언이 0건**이었습니다
- `500 ms` 인수 기준은 **`DISABLED_` 시험 안에만** 있고, 기본 실행에서 돌지 않습니다
- `DISABLED_` 는 `modules/preprocess/tests/` 전체에서 **이 하나뿐**입니다

성능 기준이 살아 있다고 믿는 다음 사람을 위한 기록이며, 조치는 범위 밖으로 두었습니다.

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **757 / 757** (`CTEST_EXIT=0`) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=688`, `PASSED 680`, 실패 0 |

`EXPECT_TRUE(rc == … || rc == …)` 잔여 현황 (`A-147` 전수 14곳 기준):

| 분류 | 건수 | 상태 |
|---|---|---|
| `XPE_OK` + 무동작 코드 (같은 결함) | 11 | **전부 좁힘** (A-147 7건 + A-148 4건) |
| 두 오류 코드 택일 (`IO_FAILED \|\| CONFIG_INVALID`) | 3 | 해당 없음 — 무동작을 성공으로 읽지 않음 |

## 미검증 / 잔여 위험

- **좁힌 단언은 전역 위생이 계속 지켜진다는 전제입니다.** 앞선 시험이 초기화나 캘리브레이션을
  흘리면 이제 여기서 빨개집니다. 그것이 목적이지만, **실패가 흘린 쪽이 아니라 여기에 나타난다**는
  점은 기록해 둡니다 — 진단할 때 이 시험이 원인처럼 읽힐 수 있습니다.
- 네 순서만 쟀습니다. 다른 seed 에서 갈릴 가능성을 0 으로 만든 것은 아닙니다. 다만 `memleak`
  자리는 한 실행에 2200회 호출되므로 그 자리에 한해 표본은 충분합니다.
- 나머지 3건(`IO_FAILED || CONFIG_INVALID`)은 재지 않았습니다. 두 오류 중 하나를 받는 형태라
  무동작을 성공으로 읽는 문제가 없다고 판단했고, 그 판단은 코드를 읽어서 한 것이지 측정한 것이
  아닙니다.
- `DISABLED_PipelinePerformance3072x3072` 는 손대지 않았습니다 — 범위 밖.

🗿 MoAI
