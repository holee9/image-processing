# QA-A-140 (#196) — 이유가 **있었고, 두 겹으로 무효**입니다. 걷어냈고 빈도는 수로 정했습니다

Lane A (pre), `dev/preprocess`, 커밋 `58edac7`. `origin/main 22851e1` 병합(`0eb604b`).

## 주장

| # | 주장 | 판정 |
|---|---|---|
| 1 | 조기 반환에 **이유가 있었다** | **참** — 주석과 요구 번호까지 |
| 2 | 그 이유가 **아직 유효하다** | **거짓** — 두 겹으로 무효 |
| 3 | `config==null` + LUT 없음 → 알림이 뜬다 | **참** |
| 4 | LUT 있음 → 안 뜨고 **프레임이 실제로 바뀐다** | **참** |
| 5 | 프레임마다 뜨면 다른 알림이 밀려난다 | **참** — 실측 63/64 |
| 6 | 위생 가드가 새 누출을 잡는다 | **참** — 3건 |

## (a) blame — 이유는 **있었습니다**

`git log -L 94,94:modules/preprocess/src/nonlinearity_correct.cpp` 는 **커밋 하나**만 냅니다:

```
ee2c607 feat(preprocess): SPEC-XPE-P1A 전체 구현 완료
+    if (!configJsonOrNull) return XPE_OK;
```

**최초 구현과 함께 들어왔고 그 뒤 한 번도 수정되지 않았습니다.** 그리고 당시 그 줄에는 주석이 붙어 있었습니다(`git show ee2c607:...`):

```cpp
// REQ-P1A-013: no-op when no config supplied
if (!configJsonOrNull) return XPE_OK;

// Parse detector mode from JSON
const std::string mode = xpe_json_get_string(configJsonOrNull, "mode");
```

**그때는 옳았습니다.** 바로 다음 문장이 config 에서 `"mode"` 를 파싱했으니, config 가 없으면 파싱할 것이 없었습니다. **가드는 자기 뒤에 있던 것에 대해 건전했습니다.**

**QA-A-127 이 그 파싱을 무동작 보고로 바꾸면서 가드만 남았습니다.** 그 주석도 지금은 없습니다 — 현재 그 줄에는 아무 설명이 없었습니다.

### 그 이유는 두 겹으로 무효입니다

**첫째, 요구가 사라졌습니다.** 옛 `REQ-P1A-013`(`ee2c607` `spec.md:88`):

> *"WHERE the detector panel profile indicates linear response (no nonlinearity coefficients loaded), the system SHALL bypass the correction and return `XPE_OK` without modifying the image."*

**현재 집합의 `REQ-P1A-013` 은 결함 보정입니다** — Hampel recipe, BP-04, TPR ≥ 99.9% / FPR < 0.001% (`.moai/specs/SPEC-XPE-P1A/acceptance.md:273, 615-616`). `bc22093` 재번호가 그 번호를 다른 요구에 줬습니다. **같은 파일 위쪽 주석이 014·015 에 대해 이미 적어 둔 것과 같은 형태** — 재번호 하나가 낳은 고아 인용입니다.

**대조군**: 같은 검색으로 `REQ-P1A-010`·`-014`·`-016`·`-020` 이 현재 `spec.md` 에서 잡힙니다. 검색이 헛돈 것이 아닙니다.

**둘째, 옛 요구조차 침묵을 요구하지 않았습니다.** 그것이 지시한 것은 **"보정을 건너뛰고, 영상을 바꾸지 않고, `XPE_OK` 를 반환"** 이고 **셋 다 지금도 성립합니다** — 알림은 화소를 바꾸지 않고 반환값도 `XPE_OK` 입니다. 그리고 그 요구의 조건은 **패널 프로파일**이지 *"config 문자열을 넘겼는가"* 가 아니었습니다. 패널 프로파일은 이미 위쪽 `panel_linear` 가 다룹니다.

**그래서 (b) 로 갔습니다.**

> 카드가 짚은 대로 `#162`(post, `multiscale_process` 가 복구 계약을 `if (levels <= 3)` 로 감쌈)와 같은 형태입니다. 다만 한 가지가 다릅니다 — **여기서는 감싼 조건이 처음부터 잘못된 것이 아니라, 감싸인 내용이 나중에 바뀌었습니다.** 그 변경을 한 것이 QA-A-127, 즉 **제가 한 카드**입니다.

## (b) 변경

### 조기 반환 제거

그 자리에 위 판단을 주석으로 남겼습니다. 앞선 두 반환(`panel_linear=="true"`, LUT 적용)과 `panel.linear=="false"` 오류 경로는 그대로이므로 **기존 동작은 바뀌지 않습니다.**

### 빈도 — **조건당 한 번**, 근거는 수

알림 메시지는 **프레임별 정보를 싣지 않습니다** — 매 호출 바이트 단위로 같습니다. 그리고 알림 큐는 **64건 상한에 FIFO 축출**입니다(`xpe_common.cpp:59`).

**실측**: 억제를 끄고 200프레임을 돌리면 **63건**이 큐에 남습니다 — 상한에 눌린 수이고, 그 사이 **다른 알림은 전부 밀려났습니다.** 그중에는 gui 가 `#198` 에서 기다리는 `#194` 클램프 알림도 들어갑니다.

`nonlin_noop_reported` 래치를 `CalibrationData` 에 두고, **LUT 가 적재·해제될 때 재무장**합니다(`xpe_preprocess_shutdown()` 이 구조체를 통째로 초기화하므로 그 경로도 덮입니다). 상황이 바뀐 뒤의 무동작은 **새 사실**이니까요.

**`gain_correct.cpp:374` 가 프레임당인 것은 그 메시지가 그 프레임의 클램프 건수를 싣기 때문입니다.** 빈도는 메시지가 무엇을 말하느냐를 따르는 것이지 집안 양식이 아닙니다 — 그래서 참고는 했지만 그대로 따르지 않았습니다.

## §6 양방향 단언 — `test_nonlin_noop_report.cpp` 6건

| 시험 | 단언 |
|---|---|
| `NullConfigStillReports` | **gui 의 실제 호출 형태** — config `nullptr`, LUT 없음 → 알림 1건 |
| `ConfigPresentStillReports` | config 있음 → 여전히 1건 (기존 동작 유지) |
| `LutLoadedIsSilentAndActuallyChangesTheFrame` | **대조군, 양쪽 다** — 알림 0건 **그리고** 화소 1000 → 500 |
| `NonLinearPanelWithoutLutIsStillAnError` | `XPE_ERR_CALIB_NOT_LOADED` + ERROR 알림, 무동작 알림은 0건 |
| `ManyFramesRaiseOneAlert` | **200프레임 → 알림 1건**, 큐 총량 ≤ 2 |
| `LoadingThenUnloadingALutReArmsTheReport` | LUT 왔다 가면 다음 무동작은 **다시** 보고 |

**대조군의 둘째 반쪽이 핵심입니다.** 알림 없음만 단언하면 *"무엇에나 알림 안 주는 코드"* 와 구별되지 않습니다. 화소가 **실제로 반으로 줄었다**는 것이 적용됐다는 증거입니다.

## 반증 — 둘, 각각 `BUILD_EXIT=0` 확인 후

### ① 조기 반환 복원 (런타임 거짓 조건)

```
[  FAILED  ] NullConfigStillReports
[       OK ] ConfigPresentStillReports
[       OK ] LutLoadedIsSilentAndActuallyChangesTheFrame
[       OK ] NonLinearPanelWithoutLutIsStillAnError
[  FAILED  ] ManyFramesRaiseOneAlert
[  FAILED  ] LoadingThenUnloadingALutReArmsTheReport
```
**null-config 경로 3건만 빨강, 대조군 3건은 초록.** 단언이 이 변경에 반응한다는 뜻입니다.

### ② 억제 래치 무력화

```
[  FAILED  ] ManyFramesRaiseOneAlert
      Which is: 63
```
**나머지 5건은 초록.** 63 은 큐 상한 64 에 눌린 수입니다 — 홍수와 축출이 실측됐습니다.

## 위생 가드가 제 일을 했습니다

이 변경이 알림을 새 경로에서 밀자 **QA-A-138 의 넷째 축이 시험 3건을 잡았습니다**:

```
NonlinearityCorrect.NullConfigIsNoOp -- pending alerts 0 -> 1
Integration.FullPipelineSmallImage -- pending alerts 0 -> 1
PreprocessDegraded.BP05_NonlinearityNullConfigIsIdentity -- pending alerts 0 -> 1
```

**세 시험 자체는 통과했고**(무동작·반환값 단언은 그대로 성립), 가드만 빨개졌습니다. 전부 **제품 경로 `xpe_clear_alerts()`** 로 비웠습니다. 셋 다 픽스처가 없어 본문 끝에서 비웁니다.

> 카드가 *"잡히면 그게 정상"* 이라고 적었고, 그대로였습니다. 이 셋은 **ctest 에서는 영원히 초록**이었을 것입니다.

## 증거

```
BUILD_EXIT=0
100% tests passed, 0 tests failed out of 749
CTEST_EXIT=0
```

한 프로세스, 네 순서:

```
[default] ran=680  PASSED 672  위반 0
[seed=1]  ran=680  PASSED 672  위반 0
[seed=2]  ran=680  PASSED 672  위반 0
[seed=9]  ran=680  PASSED 672  위반 0
```

`ran=` 네 실행 동일. 674 → 680 은 **새 시험 6건**입니다. 672 와의 차이 8건은 기존 Skipped 로 동일합니다.

## baseline 귀속

- blame, `ee2c607` 원문, 현재 `REQ-P1A-013` 의 정체, 반증 ①②, 위생 가드 3건, 전체 ctest, 네 순서: 전부 이 트리에서 직접 실행·열람했습니다.
- 큐 상한 64 와 FIFO 축출: `xpe_common.cpp:59` 를 읽었고, 200프레임 63건으로 **실측 확인**했습니다.
- gui 가 config 를 `nullptr` 로 넘긴다는 것: **리더 실측이며 재현하지 않았습니다**(`clients/`·`gui/` 는 Lane C 소유). 제 시험은 `xpe_nonlinearity_correct(&b, nullptr)` 라는 **호출 형태**를 고정할 뿐, gui 가 실제로 그 형태로 부르는지는 확인하지 않았습니다.

## 미검증

- **gui 화면까지 닿는지** 보지 않았습니다 — 큐에 들어가는 것까지입니다.
- **옛 `REQ-P1A-013` 을 인용한 다른 자리**를 고치지 않았습니다. `test_temp_nonlinearity_binning.cpp:96` 과 `test_preprocess_degraded.cpp:330` 이 여전히 그 번호를 인용합니다 — `#197`/QA-A-128 에서 리더가 *"인용을 고치지 마십시오"* 라고 판정했으므로 **보고만 합니다.**
- **래치가 여러 스레드에서 동작하는지** 시험하지 않았습니다. `g_calib_mutex` 아래에서 읽고 쓰지만, 동시 호출에서 정확히 1건인지는 재지 않았습니다.
- **`xpe_preprocess_shutdown()` 이 래치를 재무장하는 경로**를 시험으로 고정하지 않았습니다. `g_calib = CalibrationData{}` 라는 코드 수준 사실만 있습니다.
- 시드 1·2·9 외의 순서는 돌리지 않았습니다.

## 잔여 위험

- **래치는 프로세스 전역입니다.** 한 호스트가 여러 검출기를 번갈아 처리하면서 LUT 를 적재하지 않으면, 첫 보고 이후 조용합니다. LUT 적재·해제가 재무장 지점이므로 **프로파일을 바꿀 때 `xpe_calib_unload_nonlin_lut()` 를 부르는 호스트**는 다시 보고받습니다 — 부르지 않는 호스트는 아닙니다.
- **`REQ-P1A-013` 인용 2건이 남아 있습니다.** 다음 사람이 그 번호를 현재 SPEC 에서 찾으면 결함 보정 요구를 만납니다 — 오늘 반복해서 본 형태입니다.
