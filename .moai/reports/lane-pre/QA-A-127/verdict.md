# QA-A-127 (#196) — 거부를 없애고 무동작 + 알림으로. 그리고 REQ 인용은 이 파일만의 문제가 아닙니다

Lane A (pre), `dev/preprocess`. `BUILD_EXIT=0`.

## (a) 목록을 제거하고 알림으로 바꿨습니다

`kKnownModes = {"standard", "high_gain", "low_dose"}` 정의와 그것을 쓰던 거부 분기를 **둘 다 제거**했습니다. 이제 이 단계는 `"mode"` 에서 **아무것도 결정하지 않습니다.**

빠진 자리에 알림 1건이 들어갔습니다:

> `nonlinearity correction did nothing: no LUT is loaded and panel.linear is not "false", so the frame passed through unchanged; load a LUT with xpe_calib_load_nonlin_lut() if this panel needs correcting (issue #196)` — `XPE_ALERT_WARNING`

**조용히 통과시키지 않은 이유를 주석에 적었습니다**: 프레임이 어느 쪽이든 바이트가 같고 `XPE_FLAG_NONLINEARITY_CORRECTED` 도 양쪽 다 없습니다. 즉 **"보정됐다" 와 "아무 일도 없었다" 를 밖에서 구별할 수단이 없습니다.** 오류는 최소한 말은 하고 있었고, 그 성질을 지켰습니다.

LUT 적재 시 동작은 그대로입니다. `panel.linear == "false"` + LUT 없음의 `XPE_ERR_CALIB_NOT_LOADED` 도 그대로입니다.

### 함께 고친 것

- 파일 헤더의 `REQ-P1A-012 to REQ-P1A-015` 인용 → SRS-CALIB-FUNC-006 / -EXT 6a. **네 요구 전부 다른 것을 가리켰습니다**(QA-A-126).
- `@MX:SPEC: REQ-P1A-012`(결함 보정) → `SRS-CALIB-FUNC-006-EXT 6a`
- QA-A-125 가 적은 우선순위 문구에서 *"the list below would reject"* 를 목록이 사라진 현재에 맞춰 고쳤습니다. **우선순위 note 자체는 남겼습니다** — 6b 가 오면 질문이 되살아납니다.

## 시험 5건과 (b) 반증

```
[       OK ] NonlinModeTest.ControlAPipelineWithNoModeKeySucceeds (0 ms)
[       OK ] NonlinModeTest.AnOperatingModeNoLongerFailsThePipeline (0 ms)
[       OK ] NonlinModeTest.AnArbitraryModeNameIsAlsoAccepted (0 ms)
[       OK ] NonlinModeTest.DoingNothingIsReported (0 ms)
[       OK ] NonlinModeTest.DoingNothingIsReportedWithNoModeKeyEither (0 ms)
[  PASSED  ] 5 tests.
```

대조군(`ControlAPipelineWithNoModeKeySucceeds`)이 맨 앞에 있습니다. **첫 QA-A-126 프로브가 눈이 멀었던 이유를 픽스처 주석에 적어 뒀습니다** — 다른 스테이지를 우회하지 않으면 오프셋 보정이 stage 3 전에 막습니다.

### 반증 — 알림만 빼면

`xpe_alert_push` 를 런타임에 닿지 않게 약화(`if (img == nullptr)`)했습니다.

- **`BUILD_EXIT=0`** — 빌드는 통과했습니다
- **2건 빨강**: `DoingNothingIsReported`, `DoingNothingIsReportedWithNoModeKeyEither`
- **3건은 초록**: `Control...`, `AnOperatingMode...`, `AnArbitraryModeName...`

**카드가 경고한 그대로입니다** — `rc` 만 단언하는 시험 셋은 **알림이 없어도 통과합니다.** 알림을 단언하는 둘만 빨강입니다. 되돌리면 5/5 초록.

### 전체 ctest 가 낡은 기대 1건을 잡았습니다

첫 전체 실행에서 `CTEST_EXIT=8`, **1건 빨강**이었습니다:

```
135 - NonlinearityCorrect.UnknownModeReturnsConfigError (Failed)
  test_temp_nonlinearity_binning.cpp(109): Expected equality of these values:
    -4
    xpe_nonlinearity_correct(&buf, R"({"mode":"unknown_xyz"})")
      Which is: 0
```

**지금 지운 바로 그 동작을 고정하던 시험**이고, **쓰였을 당시에는 옳았습니다.** `UnknownModeIsNoLongerAnError` 로 다시 썼습니다 — 오류가 아님과 프레임 불변을 단언하고, 왜 기대가 뒤집혔는지(측정 셋)를 주석에 적었습니다. 알림 단언은 `NonlinModeTest` 쪽에 있으므로 여기서 중복하지 않았습니다.

**이 시험이 없었으면 동작 변경이 조용히 지나갔을 것입니다.** 그것이 주석과 다른 점입니다 — 코드는 틀리면 무언가 깨집니다.

## (c) REQ 인용 대조 — **이 파일만의 문제가 아닙니다**

`modules/` 의 `.cpp`/`.h`/`.hpp` 전체입니다.

| 항목 | 수 |
|---|---|
| `REQ-P1A-*` 인용 총 건수 | **350** |
| 인용된 서로 다른 ID | **42** |
| 인용하는 파일 | **72** |
| `SPEC-XPE-P1A/` **어디에도 없는** ID | **16** |
| 그 16개 ID 의 인용 건수 | **63** |
| 그 16개를 인용하는 파일 | **16** |

### 기계적으로 확정되는 것 — 없는 요구 63건

| 인용 ID | 건수 |
|---|---|
| `REQ-P1A-006` | 1 |
| `REQ-P1A-007` | 4 |
| `REQ-P1A-008` | 7 |
| `REQ-P1A-009` | 6 |
| `REQ-P1A-023` | 6 |
| `REQ-P1A-024` | 3 |
| `REQ-P1A-025` | 1 |
| `REQ-P1A-026` | 1 |
| `REQ-P1A-027` | 2 |
| `REQ-P1A-028` | 1 |
| `REQ-P1A-029` | 10 |
| `REQ-P1A-034` | 9 |
| `REQ-P1A-035` | 5 |
| `REQ-P1A-038` | 1 |
| `REQ-P1A-039` | 1 |
| `REQ-P1A-047` | 5 |

**대조군**: 같은 검색으로 `REQ-P1A-014` 는 SPEC 쪽에서 잡힙니다. SPEC 디렉터리 전체(`spec.md`·`plan.md`·`acceptance.md`·`progress.md`)에 나타나는 ID 는 28개이고, 위 16개는 그 어디에도 없습니다.

해당 16개 파일:
`include/xpe/preprocess/xpe_preprocess_internal.h`, `include/xpe/preprocess_api.h`, `src/binning_correct.cpp`, `src/calibration_cache.cpp`, `src/calibration_manager.cpp`, `src/ghost_correct.cpp`, `src/offset_correct.cpp`, `src/pipeline.cpp`, `src/temp_compensate.cpp`, `src/xpe_calib_generate_gain.cpp`, `tests/test_calibration_roundtrip.cpp`, `tests/test_defect_correct.cpp`, `tests/test_ghost_correct.cpp`, `tests/test_golden_reference.cpp`, `tests/test_offset_correct.cpp`, `tests/test_temp_nonlinearity_binning.cpp`

### 판단이 필요한 것 — 손으로 대조한 표본

**222개 (파일, ID) 쌍 전부를 판정하지 않았습니다.** 확인한 것만 적습니다.

| 인용 위치 | 인용 ID | SPEC 의 실제 제목 | 파일이 말하는 것 | 맞나 |
|---|---|---|---|---|
| `nonlinearity_correct.cpp:98`(수정 전) | 014 | `:335` Calibration File Loading (Offset) | unknown mode → CONFIG_INVALID | **아니오** |
| `nonlinearity_correct.cpp:91`(수정 전) | 013 | `:184` Runtime Defect Detection | no-op when no config | **아니오** |
| `nonlinearity_correct.cpp:105`(수정 전) | 012/015 | `:167` Defect Correction / `:342` Gain Loading | identity polynomial | **아니오** |
| `binning_correct.cpp:24` | 020 | `:386` Not-Initialized Guard | no-op for binningMode == 1 | **아니오** |
| `binning_correct.cpp:27` | 021 | `:401` Dimension Mismatch Guard | unknown binning mode | **아니오** |
| `binning_correct.cpp:28` | 022 | `:408` Format Mismatch Guard | float32 format (post-gain stage) | **아니오** |
| `ghost_correct.cpp:30` | 030 | `:417` No Exceptions Across C ABI | configJsonOrNull for IRF override | **아니오** |
| `ghost_correct.cpp:31` | 031 | `:424` No Memory Leak | OUT_OF_MEMORY on alloc failure | **아니오** |
| `ghost_correct.cpp:213` | 033 | `:439` No NaN/Inf in Output | time delta in frames | **아니오** |
| `temp_compensate.cpp:23` | 005 | `:129` Input Validation | (파일 전반) | 판단 보류 — 너무 일반적 |

**표본 9건 중 9건이 다른 요구를 가리킵니다.**

> ### 정정 (QA-A-128·129, 2026-09-19)
>
> **위 표의 "맞나 = 아니오" 는 관측으로는 맞지만, 그 위에 얹은 해석이 틀렸습니다.**
>
> 이 보고서는 이것을 **"인용이 엉뚱한 요구를 가리킨다"** 로 읽었습니다. 실제는 **"인용이 가리키던 요구가 재번호로 다른 요구에 덮였다"** 입니다. 옛 판본(`ee2c607`)에서는 이 인용들이 **정확했습니다** — 옛 `REQ-P1A-012`~`015` 가 전부 비선형 보정이고, 특히 옛 **`REQ-P1A-015`** 가 *"IF `configJsonOrNull` specifies an unknown detector mode, THEN the system SHALL return `XPE_ERR_CONFIG_INVALID`"* 입니다. `nonlinearity_correct.cpp` 헤더가 선언하던 범위 `REQ-P1A-012 to REQ-P1A-015` 는 옛 체계에서 맞는 범위였습니다.
>
> **따라서 QA-A-127 이 제거한 거부는 "근거 없는 것" 이 아니라 요구 하나를 정확히 구현하고 있었습니다.** 대응표 전체는 QA-A-128 (`.moai/reports/lane-pre/QA-A-128/verdict.md`) 에 있습니다 — 재사용 25 / 이동 13 / 삭제 18 / 확정 못 함 27.
>
> **동작 판단은 유지됩니다.** 거짓 거부는 실측됐고(QA-A-126), 그 요구는 현재 SPEC 집합에 집이 없으므로(`SPEC-XPE-P1D` 부재) 되돌리지 않습니다. 진짜 결함은 가드가 아니라 **`"mode"` 키 충돌**이었고, 옳은 수정은 검출기 모드에 자기 키를 주는 것이었습니다 — `nonlinearity_correct.cpp` 주석에 그렇게 적었습니다(QA-A-129).
>
> **다만 세 이름의 출처는 여전히 없습니다.** 옛 `015` 도 *"unknown detector mode"* 라고만 하고 이름을 열거하지 않습니다. 요구는 있었지만 `{standard, high_gain, low_dose}` 라는 목록은 그 요구에서 따라 나오지 않습니다.

### 뿌리로 보이는 것

각 파일이 헤더에 **자기 범위**를 선언하고(`binning_correct.cpp:5` "REQ-P1A-020 to REQ-P1A-023", `temp_compensate.cpp:5` "REQ-P1A-005 to REQ-P1A-008", `ghost_correct.cpp:8` "REQ-P1A-029 to REQ-P1A-034"), 그 안에서 **자기 요구를 1번부터 세고 있습니다.** 즉 `REQ-P1A-0NN` 이 **전역 ID 가 아니라 파일 지역 번호**로 쓰이고 있습니다.

이것이 사실이라면 없는 ID 63건과 어긋난 인용은 **같은 원인의 두 증상**입니다 — 파일마다 자기 번호대를 잡았고, SPEC 의 실제 번호대와는 무관합니다.

> ### 정정 (QA-A-128, 2026-09-19)
>
> **"파일 지역 번호" 가설은 틀렸습니다.** 파일들이 자기 번호를 지어낸 것이 아니라 **옛 SPEC 의 전역 번호를 그대로 쓰고 있었습니다.** 옛 판본은 `001`~`071` 연속이고 절 구조(2.1 판독검증 … 2.15 경계조건)가 주제별로 끊깁니다 — `binning_correct.cpp:5` 의 `"REQ-P1A-020 to REQ-P1A-023"` 은 옛 **§2.6 Binning Correction** 의 실제 범위이고, `ghost_correct.cpp:8` 의 `"029 to 034"` 는 옛 **§2.8 Ghost/Lag** 입니다.
>
> 즉 "파일이 자기 범위를 선언한다" 는 관찰 자체는 맞았지만 **그 범위가 옛 SPEC 것**이었습니다. 두 증상의 원인은 **`bc22093` 의 재번호 한 번**입니다.

**`#195`(RTM ↔ SRS 번호 체계 불일치)와 같은 성질입니다.**

**고치지 않았습니다** — 카드가 "몇 건인지가 전부" 라고 했습니다.

## baseline 귀속

- 빌드: `cmake --build build/ci-preprocess --config RelWithDebInfo` (타깃 미지정) → `BUILD_EXIT=0`
- 시험: `xpe_preprocess_tests.exe --gtest_filter='NonlinModeTest.*'` → 5/5
- 반증: 같은 명령, 알림 약화 후 → `BUILD_EXIT=0` + 2건 빨강
- (c) 의 수는 `grep -rho` / `comm` 결과이고, 손으로 대조한 9건은 양쪽 줄을 직접 읽었습니다

## 미검증

- **222개 (파일, ID) 쌍을 전수 판정하지 않았습니다.** 표본 9건입니다. "9/9 가 틀렸다" 는 표본의 결과이고 전체 비율이 아닙니다.
- **파일 지역 번호 가설을 검증하지 않았습니다.** 세 파일의 헤더 선언에서 읽은 것이고, 72개 파일 전부를 보지 않았습니다.
- **SPEC 의 과거 판본**에서 번호가 달랐을 가능성 — 변경 이력(`spec.md:27-30`)에 재기술 기록이 있으나 옛 원문을 읽지 못했습니다. **파일들이 옛 번호를 따르고 있을 수 있습니다.**
- `REQ-P1A-066`(5건)은 SPEC 디렉터리 어딘가에 있어 "없는 ID" 목록에서 빠졌지만, **어느 문서에 어떤 뜻으로 있는지 확인하지 않았습니다.**
- **`clients`·`gui` 가 `"mode"` 를 넘기는지는 여전히 모릅니다**(리더가 가져감). 이번 변경은 그것과 무관하게 거부를 없앱니다.

## 잔여 위험

- **이 알림도 프레임마다 뜹니다.** LUT 없이 도는 파이프라인에서는 매 프레임 1건입니다 — `#194` 의 클램프 알림 누적과 같은 문제이고, 억제 정책이 없으면 둘 다 읽히지 않게 됩니다.
- 거부를 없앴으므로 **오타 난 `"mode"` 도 조용히 통과합니다.** 알림이 그 자리를 메우지만, 알림은 오류처럼 흐름을 멈추지 않습니다.
