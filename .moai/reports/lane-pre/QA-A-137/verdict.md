# QA-A-137 (#198) — 잔류는 **알림 큐**입니다. 제품 결함이 아니고, 시험이 제품의 리셋 경로를 안 불렀습니다

Lane A (pre), `dev/preprocess`, 커밋 `2a734dd`. `origin/main 2a8ab20` 병합(`acb2387`).

## 주장

| # | 주장 | 판정 |
|---|---|---|
| 1 | 한 프로세스 실행에서 `ScalarGainRaisesNoPolyAlert` 가 실패한다 | **참** — 재현 |
| 2 | 새는 것은 **적재된 교정 데이터**다 | **거짓** |
| 3 | 새는 것은 **알림 큐**다 | **참** |
| 4 | 실제 앱에서도 새는 제품 결함이다 | **거짓** — 근거 아래 |
| 5 | 고친 뒤 네 순서 전부 초록, `ran=` 동일 | **참** — 673 / 673 / 673 / 673 |

## (a) 잔류의 정체 — 세 실행으로 갈랐습니다

동일 바이너리, `--gtest_filter` 만 바꿨습니다.

| 실행 | 결과 |
|---|---|
| **A** `PolyAlertIsPushed` → `Scalar` | `Scalar` **FAILED** (`Actual: true`) |
| **B** `LoadAloneIsSilent` → `Scalar` | 둘 다 **OK** |
| **C** `Scalar` 단독 | **OK** |

**B 가 결정적입니다.** `LoadAloneIsSilent` 도 다항식 파일을 `xpe_calib_load_gain()` 으로 적재합니다. 그 뒤 `Scalar` 가 통과한다는 것은 **적재된 교정 데이터가 다음 시험으로 새지 않는다**는 뜻입니다. A 와 B 의 차이는 **알림을 밀었는가 하나뿐**입니다.

**→ 남는 것은 알림 큐 하나입니다. 교정 데이터도, 둘 다도 아닙니다.**

**어느 시험 뒤에 실패하는가**: `PolyAlertIsPushed` 뒤입니다(A). 기본 순서에서 바로 앞 시험입니다.

## (a) 셋째 질문 — **시험 사이에만 샙니까, 앱에서도 샙니까**

**시험 사이에만입니다.** 제품 결함이 아닙니다. 근거 셋:

1. **알림 큐는 `common` 모듈 전역이고, `xpe_preprocess_shutdown()` 이 의도적으로 소유하지 않습니다.** 그 함수가 지우는 것은 `g_calib` 과 `xpe_calib_mode_reset_globals()` 입니다(`preprocess.cpp:71-83`). 한 모듈이 내려갈 때 큐를 비우면 **다른 모듈의 미전달 알림이 사라집니다** — 큐를 비우는 것이 preprocess 의 일이 아닌 이유입니다.
2. **비우는 공개 경로가 있습니다** — `xpe_clear_alerts()` (`xpe_error.h:149`, 구현 `xpe_common.cpp:381`). 소비자가 `xpe_get_pending_alert*` 로 읽고 비웁니다.
3. **큐에 상한이 있습니다.** 축출은 Info → Warning → Error 순, 클래스 안에서는 FIFO 이고 **모든 축출이 보고됩니다**(`xpe_common.cpp:99-105`, `g_alertsDropped`). 장시간 앱에서 무한히 자라지 않습니다.

**즉 "소비자가 드레인한다" 가 설계이고, 제 시험이 그 한 단계를 안 밟았습니다.**

카드가 경계하라고 한 post 사례(제품이 복구 계약을 `if` 로 감싸 실행 불가)와는 다릅니다 — **저쪽은 리셋 경로가 실행되지 않았고, 이쪽은 리셋 경로가 호출되지 않았습니다.** 경로 자체는 살아 있고 이 저장소 다른 시험들이 이미 부르고 있습니다.

## (b) 방향과 이유 — **시험이 제품의 리셋 경로로 초기화한다**

시험 전용 훅을 만들지 않았습니다. `SetUp` / `TearDown` 에서 **공개 API `xpe_clear_alerts()`** 를 부릅니다.

**이유**: 이 저장소에 **이미 확립된 관행**이고, 관행이 곧 증거입니다 —

| 기존 시험 | 자리 |
|---|---|
| `test_gain_poly_dose_range.cpp:250, 265, 283, 301, 440` | 알림 단언 직전마다 |
| `test_alert_queue_overflow.cpp:87-88` | `SetUp` / `TearDown` 양쪽 |
| `modules/ai/tests/test_config_warning.cpp:40, 45` | 동일 |

**전용 훅을 만들었으면 "제품에 그 경로가 있다" 는 사실이 가려졌을 것**입니다 — 카드가 짚은 그대로입니다.

## 증거

### 고친 뒤 — 네 순서, 한 프로세스

```
[default]      673 tests from 91 test suites ran.  PASSED 665
[seed=1]       673 tests from 91 test suites ran.  PASSED 665
[seed=2]       673 tests from 91 test suites ran.  PASSED 665
[seed=9]       673 tests from 91 test suites ran.  PASSED 665
```

**`ran=` 네 실행 모두 673 으로 동일. 실패 0.** 665 와의 차이 8건은 **기존 Skipped** 입니다(`CalibSaveTest.SaveOffset_WhenNotLoaded_ReturnsInvalidInput`, `PreprocessCalibrationTest.*` 7건) — ctest 가 Skipped 로 보고하던 바로 그 8건이며, 이번 변경과 무관합니다. `Integration.DISABLED_PipelinePerformance3072x3072` 는 애초에 673 에 안 들어갑니다.

### 전체 ctest

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 742
CTEST_EXIT=0
```

### 반증

두 초기화(`SetUp`·`TearDown`)를 런타임 거짓 조건으로 껐습니다. **`BUILD_EXIT=0` 확인 후**:

```
[       OK ] TypeByteIsGainPoly
[       OK ] DefaultRunStaysScalarAndWritesNoPolyFile
[       OK ] GeneratedPolyFileLoads
[       OK ] LoadAloneIsSilent
[       OK ] PolyAlertIsPushed
  Actual: true
[  FAILED  ] ScalarGainRaisesNoPolyAlert
```

**`TearDown` 만 남겨서는 빨강이 안 납니다** — 매 시험 뒤에 비우므로 다음 시험이 깨끗한 큐로 시작합니다. 그래서 둘 다 꺼야 반증이 성립합니다. 이것을 먼저 확인하지 않았으면 "반증이 안 터진다" 로 읽힐 뻔했습니다.

**첫 두 반증 시도는 무효였습니다** — `if (false && ...)` 와 `if (kW == 0)` 둘 다 `/WX` 의 상수 조건 경고에 걸려 `BUILD_EXIT=1` 이 났고, 낡은 바이너리가 전부 초록을 찍었습니다. QA-A-136 에 이어 **같은 함정에 두 번째**입니다. 컴파일러가 접지 못하는 런타임 조건(`::testing::UnitTest::GetInstance() != nullptr`)으로 바꿔 다시 쟀습니다.

## baseline 귀속

- 세 갈래 실행(A/B/C), 네 순서 실행, 전체 ctest, 반증: 전부 이 트리·이 커밋에서 직접 실행한 출력입니다.
- `preprocess.cpp:71-83`, `xpe_common.cpp:381`, `xpe_error.h:149`, 기존 시험 호출부 3건: 파일에서 직접 읽었습니다.
- `#176` 의 CI 가 시드 1·2·9 로 돈다는 것: **리더 실측이며 재현하지 않았습니다.** 카드가 준 네 명령을 그대로 돌렸을 뿐입니다.

## 미검증

- **`xpe_preprocess_shutdown()` 이 알림 큐를 지우지 *않아야* 하는지**를 제품 요구로 확인하지 않았습니다. 위 1번은 설계 근거(다른 모듈 알림 손실)이지 인용한 요구가 아닙니다.
- **다른 모듈 시험 바이너리**(`xpe_common_tests` 등)를 네 순서로 돌리지 않았습니다 — 카드 범위가 `xpe_preprocess_tests` 입니다.
- **앱(GUI)이 실제로 `xpe_clear_alerts()` 를 부르는지** 확인하지 않았습니다. `clients/`·`gui/` 는 Lane C 소유입니다. 앱이 드레인하지 않으면 큐는 축출로 상한을 지킬 뿐 오래된 알림이 남습니다 — **보고만 합니다.**
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **알림 큐를 단언하는 새 시험은 앞으로도 같은 함정에 빠집니다.** 관행은 있지만 강제하는 것이 없습니다 — `test_global_state_hygiene.cpp` 가 교정 상태는 보는데 알림 큐를 보는지는 확인하지 않았습니다. **가드를 넓힐지는 리더 판단입니다.**
- **`ran=673` 은 이 바이너리·이 기계 기준**입니다. CI 가 다른 수를 세면 시험 구성이 다른 것이므로 그 차이부터 봐야 합니다.
