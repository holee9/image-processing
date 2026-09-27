# QA-A-138 (#198) — 누락이었습니다. 넓혔고, **어제의 결함에 대고 돌려 빨개지는 것**을 확인했습니다

Lane A (pre), `dev/preprocess`, 커밋 `cc26a66`. `origin/main a18ab2e` 병합.

## 주장

| # | 주장 | 판정 |
|---|---|---|
| 1 | 가드는 **열거식**이고 세 축만 본다 | **참** |
| 2 | 알림 큐 누락이 **의도**다 | **거짓** — 이유가 어디에도 없습니다 |
| 3 | 넓힌 가드가 **QA-A-137 이전 상태에서 빨개진다** | **참** — §4 |
| 4 | 대조군: 지금 상태에서는 초록 | **참** — 위반 0건 |
| 5 | 기존 시험을 **제품 경로로만** 고쳤다 | **참** — 전용 훅 없음 |

## (a) 지금 무엇을 보는가 — **열거식 3축**

`test_global_state_hygiene.cpp` 는 패턴으로 훑지 않습니다. 머리말이 *"WHAT IS CHECKED, AND WHY EXACTLY THESE THREE"* 로 **세 축을 이름으로 셉니다**:

1. 모듈 초기화 (`xpe_preprocess_is_initialized()`)
2. 교정 모드 (`xpe_calib_get_mode()`)
3. 품질 메타 (`xpe_calib_get_quality_meta()`)

검사는 `HygieneListener::OnTestStart/OnTestEnd` 에서 **시험마다** 기준을 잡고 비교합니다.

### 누락입니까, 의도입니까 — **누락입니다**

**이 파일은 제외한 것에 이유를 적습니다.** 교정 맵에 대해:

> *"The calibration MAPS are not checked here: there is no read-only query for them, and the only way to clear them is shutdown, which is not this guard's to call — a suite that loads maps once in `SetUpTestSuite` (`test_xpe_calib_endurance.cpp`) legitimately holds them across its own tests."*

**알림 큐는 세 축 목록에도, 이 제외 목록에도 없습니다.** 파일 전체(176행)에서 `alert` 검색 0건 — `#include "xpe/common/xpe_error.h"` 가 있는데도 알림 API 를 한 번도 부르지 않습니다.

**대조군**: 같은 파일이 제외 사유를 **실제로 적는다**는 것이 위 인용입니다. 즉 "사유가 안 보이니 누락" 이 아니라, **사유를 적는 파일에서 이 축만 사유 없이 빠져 있습니다.**

카드가 경계하라고 한 gui `#161` 형태(*"근거를 읽었더니 근거가 아니었다"*)의 반대입니다 — 여기서는 **읽을 근거 자체가 없습니다.**

그리고 축 3(품질 메타)의 이력이 이 축의 선례입니다: 공개 복구 경로가 없어 **보고만** 하다가, `xpe_preprocess_shutdown()` 이 지우게 된 뒤 실패 축이 됐습니다. **알림 큐는 처음부터 공개 복구 경로(`xpe_clear_alerts()`)가 있으므로 보고만 할 이유가 없습니다.**

## (b) 넣은 것

`Baseline` 에 `int32_t alerts` 를 더하고, `OnTestStart` 에서 `xpe_get_pending_alert_count()` 를 기록해 `OnTestEnd` 에서 비교합니다. 다르면 **양쪽 수**를 적습니다:

```
FixtureGenGainPolyTest.PolyAlertIsPushed -- pending alerts 1 -> 2 (drain with xpe_clear_alerts());
```

**개수로 봅니다** — 민 만큼 비운 시험은 큐를 찾은 그대로 둔 것이고 그것이 계약입니다. 두 수를 다 적는 이유는 `0 -> 3`(남기고 감)과 `3 -> 0`(남의 것을 비움)이 **읽는 사람을 다른 줄로 보내기** 때문입니다.

가드 자신의 대조 시험도 다른 두 축과 같은 형태로 더했습니다 — `GlobalStateHygieneTest.TheProbeDetectsAPendingAlert`(밀고, 보이는지 확인하고, 제품 경로로 되돌림).

### 기존 시험 33건 — 전부 제품 경로로

가드를 켜자 **33건**이 걸렸습니다(다른 세 축은 0건). 전부 `modules/preprocess/tests/` 이며 제 소유입니다.

| 고친 자리 | 덮은 건수 |
|---|---|
| `XpePreprocessStateFixture::TearDown` — 진입 시 개수를 기록해 **바뀐 경우에만** 드레인 | 15 |
| 각 스위트 `TearDown` 5곳 (`test_pipeline_stages` · `test_calib_mode_enforcement` · `test_pipeline_stage_values` · `test_gain_poly_not_applied` · `test_calib_gain_poly_load`) | 17 |
| 픽스처 없는 시험 본문 1곳 (`NonlinearityCorrect.UnknownModeIsNoLongerAnError`) | 1 |

**33 → 0.** 전용 훅은 만들지 않았고 전부 `xpe_clear_alerts()` 입니다 — QA-A-137 의 원칙 그대로.

공용 픽스처가 **조건부**로 드레인하는 이유: `xpe_clear_alerts()` 는 큐 전체를 비우므로, 이 시험이 건드리지 않았는데 부르면 남의 알림을 지웁니다. 진입 시 개수와 다를 때만 부릅니다.

## §4 — 양방향 확인 (**이 카드의 핵심**)

### 빨강: QA-A-137 이전 상태

공용 픽스처 드레인과 `FixtureGenGainPolyTest` 의 지역 초기화 **둘 다** 런타임 거짓 조건으로 껐습니다(QA-A-138 이전엔 둘 다 없었습니다). **`BUILD_EXIT=0` 확인 후**:

```
Tests left global module state changed. Each of these has to undo
  FixtureGenGainPolyTest.PolyAlertIsPushed -- pending alerts 1 -> 2 (drain with xpe_clear_alerts());
[  PASSED  ] 665 tests.
[  FAILED  ] 1 test
```
pending-alert 위반 총 **17건**.

**가드가 빨개지고, QA-A-137 이 수동으로 찾았던 바로 그 시험을 이름으로 지목합니다.**

**그리고 그때 `ScalarGainRaisesNoPolyAlert` 자체는 통과했습니다.** 순서상 사이에 낀 다른 시험이 큐를 비웠기 때문입니다. **즉 가드는 하류 단언이 우연히 통과해도 누출을 잡습니다** — 수동 실행보다 강합니다. 이것이 "넓힌 척" 이 아니라는 증거입니다.

### 초록: 지금 상태

pending-alert 위반 **0건**, 다른 세 축도 0건.

## 증거

### 전체 ctest

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 743
CTEST_EXIT=0
```

### 한 프로세스, 네 순서

```
[default] ran=674  PASSED 666   위반 0
[seed=1]  ran=674  PASSED 666   위반 0
[seed=2]  ran=674  PASSED 666   위반 0
[seed=9]  ran=674  PASSED 666   위반 0
```

`ran=` 네 실행 동일. QA-A-137 의 673 에서 **674 로 하나 늘어난 것은 새 프로브 시험** `TheProbeDetectsAPendingAlert` 입니다. 666 과의 차이 8건은 **기존 Skipped** 로 QA-A-137 과 동일합니다.

## §5 — `/WX` 문단 위치

`test_global_state_hygiene.cpp` 머리말, *"WHY THIS EXISTS"* 바로 다음, 축 목록 앞:

> **FALSIFYING A GUARD IN THIS REPOSITORY — READ BEFORE WRITING ONE.** … `if (false && ...)`, `if (kSomeConstexpr == 0)` 류는 상수 조건 경고 → `/WX` → `BUILD_EXIT=1` 이 되고, **낡은 바이너리가 돌아 통과를 찍습니다** — 반증이 "가드가 불필요했다" 로 읽힙니다. QA-A-136·137 이 연속으로 밟았습니다. 런타임 거짓 조건(`::testing::UnitTest::GetInstance() == nullptr`)을 쓰고, **어떤 반증 결과든 믿기 전에 `BUILD_EXIT` 를 읽으십시오.**

가드를 쓰려는 사람이 반드시 읽는 자리입니다. 이번 카드의 반증도 그 조건으로 걸었고, 그래서 첫 시도에 성립했습니다.

## baseline 귀속

- 33 → 0 의 각 단계, §4 양방향, 전체 ctest, 네 순서 실행: 전부 이 트리에서 직접 실행한 출력입니다.
- 가드의 현재 범위·제외 사유: `test_global_state_hygiene.cpp` 를 176행 전부 읽었습니다.
- `#176` CI 가 시드 1·2·9 로 돈다는 것: **리더 실측이며 재현하지 않았습니다.**

## 미검증

- **알림 큐의 *내용*은 안 봅니다** — 개수만입니다. 한 알림을 밀고 다른 하나를 비운 시험은 수가 같아 통과합니다. 실제로 그런 시험이 있는지 찾지 않았습니다.
- **다른 모듈 바이너리**(`xpe_common_tests` 등)에는 이 가드가 없습니다. 같은 누출이 거기서도 가능한지 보지 않았습니다 — 다른 레인 소유.
- **공용 픽스처의 조건부 드레인이 "남의 알림 보존" 을 실제로 달성하는지** 시험으로 고정하지 않았습니다. 진입 개수와 같으면 안 부른다는 코드 수준 사실만 있습니다.
- `test_xpe_calib_endurance.cpp` 처럼 **스위트 단위로 알림을 의도적으로 들고 가는 곳**이 있는지 확인하지 않았습니다. 이번 33건 중에는 없었습니다.
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **가드가 개수만 보므로, 같은 수의 다른 알림으로 바꿔치는 누출은 통과합니다.** 오늘 범위에서는 그런 사례가 없었지만 구조적 공백입니다.
- **33건을 고치면서 시험 8개 파일을 건드렸습니다.** 각 드레인은 `TearDown` 첫 줄이라 단언 이후에 돌지만, 어떤 시험이 `TearDown` 에서 알림을 읽기를 기대했다면 그 기대가 깨집니다 — 전체 ctest 와 네 순서가 초록이므로 그런 시험은 없습니다.
