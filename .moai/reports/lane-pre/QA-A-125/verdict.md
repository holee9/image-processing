# QA-A-125 (#186) — 거짓이 된 서술 둘을 고치고, 우선순위를 적고, baseline 경로를 쟀습니다

Lane A (pre), `dev/preprocess`, 커밋 `54dde9b`. `origin/main def22f5` 병합(`2e48d31`).
`BUILD_EXIT=0`, `CTEST_EXIT=0` — **731건 전부 통과**.

## (A) 고친 두 문장

### 1. `xpe_preprocess_internal.h`

**전**: *"...the known modes, which apply no correction today (SRS-CALIB-FUNC-006 is not implemented)."*

**후** — 세 가지를 갈랐습니다:

- `*applied` 가 **참이 되는 경우**: LUT 가 적재돼 있을 때. EXT 6a 는 생성(`xpe_calib_generate_nonlin_lut()`)·적재(`xpe_calib_load_nonlin_lut()`)·적용·**파이프라인 배선(stage 3, 게인 앞)** 까지 끝까지 있습니다.
- **거짓이 되는 경우**: NULL 설정, `"mode"` 없는 설정, 그리고 **LUT 가 없을 때의** 알려진 모드 — 그 경로는 옛 REQ-P1A-012 baseline 이고 화소를 바꾸지 않습니다.
- **없는 것**: EXT 6b(전역 4차 다항식, embedded/FPGA). 생성·적재·적용도 XCal 타입도 없습니다.

그리고 한 줄 남겼습니다: *"FUNC-006 is not implemented" — 이 주석이 예전에 하던 말 — 과 "FUNC-006 is implemented" 는 **둘 다 거짓**이다. 6a 는 있고 6b 는 없다.*

### 2. `nonlinearity_correct.cpp` (baseline 경로 주석)

**전**: *"Real coefficients would be loaded from a per-detector calibration profile (SRS-CALIB-FUNC-006, not implemented)."*

**후**: 이 경로는 **LUT 가 없을 때만** 닿는다는 것, 바로 위 분기가 있을 때는 적용한다는 것, 그리고 *"그 주석이 말하던 검출기별 프로파일이 바로 위에서 적재하는 LUT 다"* 를 적었습니다. 없는 것은 EXT 6b 라고 명시했습니다.

**두 문장 다 QA-A-111 이 6a 를 넣기 전에는 참이었습니다.** 6a 가 들어오면서 거짓이 됐는데 **그 문장을 읽는 상황이 없어서** 조용했습니다.

## (B) 적은 문구 — LUT 우선순위

`nonlinearity_correct.cpp` 의 LUT 분기 앞에 넣었습니다. **동작은 한 줄도 바꾸지 않았습니다.**

> **A LOADED LUT TAKES PRECEDENCE OVER THE DETECTOR MODE.** 이 블록이 `"mode"` 파싱보다 앞에 있으므로, LUT 가 적재돼 있으면 `mode` 가 무엇이든 적용된다 — **아래 목록이 `XPE_ERR_CONFIG_INVALID` 로 거부했을 모드까지 포함해서.** 의도된 순서지만(실제 교정이 보정을 고르지 않는 모드 이름보다 우선한다) 어디에도 적혀 있지 않아 여기 적는다.
>
> 이는 `#187` 이 게인 모델에 대해 *"마지막에 적재한 것이 이긴다"* 로 답한 것과 **같은 질문**이다. 지금은 모델이 하나라 우선순위가 모호하지 않지만, EXT 6b 가 오면 둘이 되고 **LUT 대 다항식을 그때 정해야 한다** — 어느 분기가 먼저 오느냐로 유추할 일이 아니다.

## (C) `kKnownModes` 호출자 — **0건**

`{"standard", "high_gain", "low_dose"}` 세 이름을 넘기는 곳입니다.

| 범위 | 결과 |
|---|---|
| `modules/` | **0건** (정의 자체 `nonlinearity_correct.cpp:21` 제외) |
| `tests/` | **0건** |
| `clients/` · `gui/` (읽기만) | **0건** |

**대조군을 짝지었습니다** — 아무것도 안 읽혀서 0 이 된 것이 아닙니다:

- `kKnownModes` 자체는 2건으로 잡힙니다(`:21` 정의, `:88` 사용)
- `nonlinearity` 를 포함한 파일이 `modules/preprocess/tests/` 에 7개(`test_nonlin_lut_apply.cpp` 등), `clients/`·`gui/` 에 4개
- `"mode"` JSON 키는 저장소에서 10건 이상 잡힙니다

**걷어내지 않았습니다** — 카드가 "0건이면 걷어냅니다" 라고 했지만, 아래 관측 때문에 **결정을 리더에게 돌립니다.**

### 재면서 나온 것 — 이 경로는 죽은 코드가 아니라 **거부하는 관문**입니다

저장소가 실제로 쓰는 `"mode"` 값은 **`clinical` · `research` · `production` · `test`** 입니다(`preprocess_api.h:71` 예시, `test_xpe_preprocess_init.cpp:62`·`:88`, `test_xcal_compression.cpp:306`, `test_xcal_writer.cpp:182`). **`kKnownModes` 의 세 이름과 하나도 겹치지 않습니다.**

그리고 `nonlinearity_correct.cpp:91` 은 모르는 모드에 **`XPE_ERR_CONFIG_INVALID`** 를 냅니다(REQ-P1A-014).

즉 이 경로가 하는 일은 "아무것도 안 함" 이 아니라 **"세 이름만 통과시키고 나머지는 거부"** 입니다. `"mode":"clinical"` 을 담은 설정이 파이프라인 stage 3 에 닿으면 — LUT 가 없고 `panel.linear` 가 `"false"` 가 아닐 때 — **파이프라인 전체가 실패합니다.**

그래서 걷어내는 것은 **두 방향의 동작 변경**입니다:

| | 걷어내면 |
|---|---|
| 세 이름을 넘기던 호출자 | 성공 → (경로가 사라지니) 성공. 변화 없음 |
| **다른 이름을 넘기던 호출자** | **거부 → 성공.** 지금 막히던 것이 통과하게 됩니다 |

**두 번째가 이 저장소 밖에 있을 수 있습니다.** 0건은 "이 저장소가 안 쓴다" 이지 "아무도 안 쓴다" 가 아닙니다 — 그리고 `XPE_ERR_CONFIG_INVALID` 를 기대하는 호출자는 **이름을 넘기지 않음으로써** 그 동작에 기대고 있어 검색에 잡히지 않습니다.

**리더 판단이 필요합니다.** 카드의 "0건이면 걷어냅니다" 를 그대로 적용할지, 아니면 REQ-P1A-014(모르는 모드 거부)가 살아 있는 요구인지 먼저 볼지입니다.

## EXT 6b — 만들지 않았습니다

전역 4차 다항식(embedded/FPGA)은 **사용자 요청 목록에 올라갔고**, 그 전까지 만들지 않습니다. `#186` 코멘트에 적었습니다.

## baseline 귀속

- 빌드: `cmake --build build/ci-preprocess --config RelWithDebInfo` (타깃 미지정) → `BUILD_EXIT=0`
- 전체: `ctest --test-dir build/ci-preprocess -C RelWithDebInfo` → `100% tests passed, 0 tests failed out of 731`, `CTEST_EXIT=0`
- (C) 의 0건은 `grep -rn` 세 번 + 대조군 세 번을 같은 실행에서 돌린 결과입니다.

## 미검증

- **주석만 바꿨으므로 반증할 동작이 없습니다.** (A)(B) 는 시험으로 고정할 수 있는 주장이 아닙니다 — 그것이 주석의 한계이고, 그래서 이 형태의 결함이 조용히 남습니다.
- **`clients/`·`gui/` 는 읽기만 했습니다.** 세 이름이 문자열 상수가 아니라 조립되는 경우(예: `"low" + "_dose"`)는 잡지 못합니다.
- **저장소 밖 호출자는 볼 수 없습니다.** 위 (C) 의 두 번째 방향이 그 때문입니다.
- **REQ-P1A-014 가 지금도 유효한 요구인지** 확인하지 않았습니다(SPEC 본문 미확인).
- `kKnownModes` 세 이름의 **출처**(실제 검출기 모드인지)는 여전히 모릅니다.
- 파이프라인 시험이 **LUT 적용을 실제로 단언하는지** 이번에도 세지 않았습니다.

## 잔여 위험

- **주석은 다시 낡습니다.** 6b 가 들어오면 이 주석 세 곳이 또 거짓이 됩니다. 오늘 고친 이유와 같은 이유로, 그때도 같이 고쳐야 합니다.
- `"mode"` 어휘가 두 갈래(`clinical`/`research`/... 대 `standard`/`high_gain`/`low_dose`)인 것 자체가 **정리되지 않은 상태**입니다. (C) 의 결정이 이것을 건드립니다.
