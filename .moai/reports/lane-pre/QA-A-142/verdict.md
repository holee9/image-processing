# QA-A-142 (#199) — 판정 **(B) 래치를 없앤다**. 억제율이 실제 호스트에서 0% 였습니다

Lane A (pre), `dev/preprocess`, 커밋 `26b7363`. `origin/main` 병합 완료.

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 752
CTEST_EXIT=0

한 프로세스 네 순서:  ran=683 / 683 / 683 / 683,  실패 0, 위생 위반 0
```

## §1 전제 확인 — 넷 다 참

| # | 전제 | 확인 | 결과 |
|---|---|---|---|
| 1 | `nonlin_noop_reported` 가 `g_calib` 멤버 | 소스 | **참** |
| 2 | 종료 함수가 `g_calib = CalibrationData{}` | `preprocess.cpp:77` | **참** |
| 3 | 그래서 래치가 함께 `false` 로 | **실행** | **참** |
| 4 | 재무장 경로는 그 외 적재·해제 둘 | 소스 | **참** |

**3번은 읽지 않고 쟀습니다** (카드 지시):

```
[a142] premise 3: first=1  second-in-session=1  after-shutdown=1
```

첫 호출 1건 → 같은 세션 재호출에도 **여전히 1건**(래치가 잡음) → 종료 후 다시 호출하니 **또 1건**(재무장됨). 구조체 대입이 멤버를 덮는다는 것이 실제로 참입니다.

## §2 판정 — **(B)**. 근거는 측정입니다

### 먼저, 리더가 물은 것 — 제가 래치를 넣은 근거

`nonlinearity_correct.cpp` 에 제가 적어 둔 것이 그대로 남아 있었고, **근거는 둘**이었습니다:

1. *"the alert queue holds 64 entries with FIFO eviction … so a stream of frames would push every other alert out, the #194 clamp count included"*
2. *"The message carries no per-frame data — it is byte-identical on every call"*

**1번이 리더가 철회한 바로 그 전제입니다.** 드레인이 매 호출 `finally` 에서 도니 축출은 일어나지 않습니다. 저는 **생산만 세고 소비를 안 봤습니다** — 리더와 같은 자리에서 같은 실수를 했고, 제 쪽은 코드에 그 문장으로 남아 있었습니다.

2번은 아직 참입니다. 그래서 그것만으로 래치가 정당한지를 **소음 크기로** 따져야 했습니다.

### 래치가 실제로 억제한 양 (20회 실행)

| 호스트 모양 | 호출 | 알림 |
|---|---|---|
| 모듈을 띄워 둠 | 20 | **1건** — 래치 동작 |
| 매 실행 종료 함수 호출 | 20 | **20건** — 래치 무동작 |

**둘째가 유일하게 존재하는 호스트입니다.** `gui/ImageProcTest/Services/Native/GuiPreprocessRunner.cs:61-88` 이 매 실행을

```
xpe_preprocess_init(null)
  → offset/gain/defect 적재 → RunStages(...)
finally { xpe_preprocess_shutdown(); }
```

로 감쌉니다. 종료 함수가 `g_calib` 을 통째로 치우니 **래치가 매 실행 리셋**됩니다.

> **즉 `#199` 를 보고한 호스트에서 억제율은 0% 였습니다.** 래치는 있으나 아무것도 억제하지 않았고, gui 가 본 "프레임마다 한 줄"이 그 결과입니다.

### (A) 도 재 봤습니다 — **소음을 하나도 줄이지 못합니다**

카드는 (A) 를 고르더라도 종료 함수가 래치를 치우게 두라고 했습니다(`#176` 을 되돌리지 말 것). **그 제약 아래에서는 측정값이 바뀌지 않습니다:**

| | GUI 모양 | 모듈 상주 |
|---|---|---|
| 현재 (래치) | 20/20 | 1/20 |
| **(A)** `g_calib` 밖 + 종료 함수가 치움 | **20/20 — 변화 없음** | 1/20 |
| **(B)** 제거 | **20/20 — 변화 없음** | 20/20 |

(A) 는 성질 다른 상태를 `g_calib` 에서 빼는 **정리**이지 `#199` 의 수정이 아닙니다.

**그리고 이 모듈 안에서는 그 호스트를 위한 억제가 애초에 불가능합니다.** 종료 함수를 넘어 사는 억제는 곧 *"종료 함수보다 오래 사는 전역 상태"* 이고, 그것은 `#176` 이 금지하고 `test_global_state_hygiene.cpp` 가 실패로 잡는 것입니다. **같은 줄을 합치는 일은 로그를 그리는 쪽 몫**입니다(`#201` 이 이미 gui 건).

### 그래서 (B)

| 남길 때 | 없앨 때 |
|---|---|
| 실제 호스트 억제 **0%** | 실제 호스트 동작 **동일**(20/20) |
| `g_calib` 에 교정 데이터가 아닌 상태 1개 | 그 상태가 사라짐 |
| 어느 호스트도 안 쓰는 재무장 규약(LUT 적재·해제) 유지 | 규약 소멸 |
| 동작이 호스트 의존 — gui 가 결함으로 신고한 이유 | 동작이 균일: 호출당 1건 |

**잃는 것도 적습니다:** 모듈을 띄워 두는 호스트는 세션당 1건 대신 프레임당 1건을 받습니다. **그런 호스트는 지금 없습니다.** 생기면 그 수치로 다시 엽니다 — 코드 주석이 그 기록입니다.

> (C) 는 별도 선택지가 아니었습니다. *"설정이 바뀔 때마다"* 가 이미 현재 동작(적재·해제 시 재무장)이고, 그것이 실제 호스트에서 한 번도 안 걸립니다 — gui 는 비선형 LUT 를 적재하지도 해제하지도 않습니다(`NonlinearityLatchRearmTests.cs`, gui 측정).

## §3 `#176` 과 부딪히지 않습니다

래치를 **없앴으므로** 종료 함수가 치울 것이 하나 줄었을 뿐, `preprocess.cpp:77` 의 *"이름대로 전부 치운다"* 는 그대로입니다. 되돌린 것이 없습니다.

## §4 반증 — (B) 형태

카드가 지정한 대로, (B) 의 반증은 *"억제를 없앴더니 다른 것까지 조용해지는가"* 입니다. **과잉 제거를 실제로 만들어** 봤습니다 — `panel.linear=="false"` 의 ERROR 알림까지 런타임 거짓 조건으로 막았습니다.

```
BUILD_EXIT=0      DLL 21:31:55 (갱신 확인)

[  FAILED  ] NonlinNoopReportTest.NonLinearPanelWithoutLutIsStillAnError
[  FAILED  ] NonlinNoopReportTest.RemovingSuppressionDidNotSilenceTheOtherPaths
[  PASSED  ] 5 tests.
```

**대조군 2건이 잡습니다.** 무동작 보고 쪽 시험 5건은 초록이라, 이 반증이 "무동작 보고가 사라졌다"가 아니라 **"다른 경로가 조용해졌다"** 를 정확히 겨눕니다.

## 시험 (7건, 전부 통과)

| 시험 | 단언 |
|---|---|
| `NullConfigStillReports` | gui 호출 형태에서 보고된다 |
| `ConfigPresentStillReports` | 기존 경로 유지 |
| `LutLoadedIsSilentAndActuallyChangesTheFrame` | 대조군 — 조용하고 **화소가 실제로 바뀐다** |
| `NonLinearPanelWithoutLutIsStillAnError` | ERROR 와 반환값 유지 |
| **`EveryNoopFrameReportsOnce`** | **40프레임 → 40건** (이전 수가 1이었으므로 수를 계속 단언) |
| **`WithAHostThatDrainsEachCallTheQueueNeverGrows`** | 드레인하는 호스트 모형 — 매 프레임 1건, 큐 잔량 0 |
| **`RemovingSuppressionDidNotSilenceTheOtherPaths`** | **대조군** — LUT 경로 침묵 유지, 비선형 ERROR 유지 |

40프레임을 쓴 이유: 큐 상한이 64 라 그 위로 돌리면 **생산자가 아니라 상한을 재게** 됩니다.

## 소유 밖 인용 확인 (규약 `79d37f3`)

제가 지난번에 약속한 절차를 밟았습니다. `clients/` **세 파일**이 래치를 이름으로 서술합니다:

| 파일 | 성격 | 깨지는가 |
|---|---|---|
| `E2ETests/.../NonlinearityNoopAlertScenarios.cs:25,37,105` | 주석 서술 + `:113` 이 `noop.Length == Frames` 단언 | **아니오** |
| `IntegrationTests/Functional/NonlinearityLatchRearmTests.cs:7` | 주석 서술. 단언은 **gui 호출부 grep** | **아니오** |

**E2E `:113` 이 프레임당 1건을 단언하고 있어 이 변경과 정확히 일치합니다.** 오히려 전에는 종료 함수가 래치를 리셋해 **우연히** 통과하던 것이, 이제 설계로 보장됩니다.

**세 파일의 서술이 낡은 것은 사실이고, gui 소유라 보고만 합니다.**

## 미검증

- **GUI 를 실제로 띄워 재지 않았습니다.** `GuiPreprocessRunner.cs` 의 호출 형태를 읽고 그 모양을 시험으로 흉내 냈을 뿐입니다 — gui 실행 측정은 gui 소유.
- **소음 비율(44줄 중 6줄)을 재현하지 않았습니다.** `GUI-C-132` 의 수치이고 제가 재지 않았습니다. 제 판정은 그 비율이 아니라 **억제율 0%** 에 근거합니다.
- **드레인이 매 호출 돈다는 것**(`NativeAlertDrain.cs:109`)은 **리더·gui 측정**이며 제가 재현하지 않았습니다. 다만 제 판정은 이것이 틀려도 바뀌지 않습니다 — 억제율 0% 는 독립적으로 쟀습니다.
- **모듈을 띄워 두는 호스트가 정말 없는지** 전수로 확인하지 않았습니다. `clients/`·`gui/` 에서 본 두 경로가 모두 `finally` 종료인 것만 확인했습니다.
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **상주형 호스트가 생기면 프레임당 1건이 됩니다.** 그때는 소비자 쪽 합치기가 먼저이고, 그래도 부족하면 그 호스트의 수치로 다시 엽니다. 주석에 그 조건을 적어 뒀습니다.
- **`clients/` 세 파일의 서술이 사라진 래치를 설명합니다.** 다음 사람이 그 주석을 읽고 없는 것을 찾을 수 있습니다 — gui 에 전달했습니다.
