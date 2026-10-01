# QA-A-125 정찰 (#186) — FUNC-006 본체와 EXT 6a

Lane A (pre), `dev/preprocess`, `origin/main def22f5` 병합(`2e48d31`). **코드 변경 없음 — 확인만.**

## 결론 먼저

**리더 관찰이 맞습니다.** "미구현" 이 아니라 **EXT 6a 는 있고, EXT 6b 가 없고, 옛 baseline 경로가 남아 있습니다.**
그리고 `xpe_preprocess_internal.h` 의 *"SRS-CALIB-FUNC-006 is not implemented"* 는 **지금 거짓입니다.**

그래서 이 이슈는 **"구현" 과 "서술 정정" 이 섞여 있습니다** — 어느 한쪽이 아닙니다.

## ① FUNC-006 본체와 EXT 6a 는 무엇이 다른가

원문(`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:55-57`, `:100`)을 읽으면 **다른 요구가 아니라 요구와 그 구현 방법입니다.**

**FUNC-006(본체, `:55`)** 은 *무엇을* 할지만 정합니다 — `I_lin(x,y) = f_nonlin(I_raw(x,y))`, 게인 보정 **앞에**, `panel.linear` 로 켜고 끄며, 잔차 ≤ 0.3% ADU. 그리고 **`f_nonlin` 을 얻는 방법으로 두 가지를 허용합니다**: "lookup table (LUT) **or** monotonic polynomial fitting". 본체 문장 끝이 "Detailed algorithm specification in SRS-CALIB-FUNC-006-EXT below" 라고 EXT 로 넘깁니다.

**EXT(`:57` 이하)** 가 그 두 방법의 절차입니다:

| | 6a — LUT | 6b — 다항식 |
|---|---|---|
| 용도 | "preferred for production" | "for embedded/FPGA use" |
| 모델 | `I_lin = LUT[I_raw]`, 4096 또는 65536 항목, uint16 | 전역 **4차** 다항식, Horner |
| 생성 | N ≥ 10 선량, 통과원점 선형 적합, Fritsch–Carlson 단조 스플라인 | 같은 자료로 최소제곱 |
| 판정 | 보간 오차 ≤ 0.3% (**측정 구간 안에서만** — 2026-09-18 정정) | 잔차 ≤ 0.5% |
| 단조성 | `LUT[i] ≤ LUT[i+1]` 강제, 위반 시 `XPE_ERR_INVALID_CALIB_DATA` | `[0, ADC_max]` 에서 도함수 근으로 확인, 위반 시 **6a 로 폴백** |

**즉 6a 와 6b 는 같은 `f_nonlin` 을 얻는 두 경로이고, 6b 가 실패하면 6a 로 돌아가게 돼 있습니다.** 본체가 없이 EXT 만 있는 관계가 아니라, **본체가 EXT 를 통해서만 실현됩니다.**

**FUNC-027(화소별 게인 다항식, `#187`)과의 구별**은 SRS 가 직접 적고 있습니다(`:201`):

> **FUNC-006 과의 관계는 중복이 아니라 계층입니다.** FUNC-006 은 전역 LUT(모든 화소 공통 응답), 이 요구는 화소별 다항식 — 전역으로 못 잡는 화소별 편차를 잡습니다.

## ② 지금 있는 것 — 적용이 배선돼 있는가

**배선돼 있습니다.** `#187` 의 "적재만 되고 적용이 없던" 함정은 여기서는 나지 않았습니다.

| 단계 | 근거 |
|---|---|
| 생성 | `xpe_calib_generate_nonlin_lut.cpp:319` 이 `XCAL_TYPE_NONLIN_LUT` 로 씀. API 는 `preprocess_api.h:325` |
| 적재 | `xpe_calib_load_nonlin_lut.cpp:41` 이 `expected_type=XCAL_TYPE_NONLIN_LUT` 로 읽음. API 는 `:364` |
| 적용 | `nonlinearity_correct.cpp:49-66` — `g_calib.nonlin_lut` 가 있으면 화소마다 `lut[raw]`, `changed = true` |
| **파이프라인 배선** | **`pipeline.cpp:177`** — `xpe_nonlinearity_apply(&stage3, cfg.rawJson, &applied)`, Stage 3 |
| 순서 | Stage 3(비선형) → **Stage 4 게인**(`pipeline.cpp:184`). FUNC-006 의 "before gain correction" 과 일치 |
| 플래그 | `:181` — 화소가 실제로 바뀌었을 때만 `XPE_FLAG_NONLINEARITY_CORRECTED` |
| 형식 검증 | `xcal_validator.cpp:68-95` — 타입 상한이 `NONLIN_LUT`, 그 타입은 `UINT16` 강제 |

`panel.linear == "true"` 면 스테이지를 건너뛰고(`:45`), LUT 없이 `"false"` 면 `XPE_ERR_CALIB_NOT_LOADED` + ERROR 알림(`:73`)입니다.

**한 가지 읽어 둘 것**: LUT 분기가 **모드 검사보다 앞**에 있습니다(`:48` vs `:85`). LUT 가 적재돼 있으면 `mode` 가 무엇이든 적용됩니다. 의도적으로 보이지만 어디에도 그렇게 적혀 있지 않습니다.

## ③ 없는 것

### (1) EXT 6b — 전역 비선형 **다항식** 경로가 통째로 없습니다

검색 범위: `preprocess_api.h` 전체, `modules/preprocess/src/*.cpp`. 비선형 다항식의 **생성·적재·적용 어느 것도 없습니다.** `XCAL_TYPE_*` 에 해당 타입도 없습니다(상한이 `NONLIN_LUT`).

6b 가 "embedded/FPGA use" 용이고 6a 가 "preferred for production" 이므로, **이 저장소가 production 경로만 구현한 상태**로 읽는 것이 자연스럽습니다. 다만 6b 의 **폴백 규칙**(다항식이 비단조면 LUT 로) 은 6b 가 있어야 의미가 있으니, 6b 부재는 그 규칙도 함께 부재라는 뜻입니다.

### (2) 옛 baseline 경로가 살아 있고, 아무 일도 하지 않습니다

`nonlinearity_correct.cpp:20` 의 `kKnownModes = {"standard", "high_gain", "low_dose"}` 와 `:93` 의 "identity polynomial for now" 입니다. LUT 가 없고 `panel.linear` 가 `"false"` 가 아닐 때 여기로 옵니다. **모드 이름만 확인하고 화소를 건드리지 않고 `XPE_OK` 를 냅니다.**

이것이 REQ-P1A-012/014/015 시절의 경로이고, FUNC-006 과는 다른 계보입니다. **지금은 "알려진 모드면 통과시킨다" 외에 하는 일이 없습니다.**

### (3) 서술이 거짓입니다 — 두 군데

| 위치 | 문장 | 왜 거짓인가 |
|---|---|---|
| `xpe_preprocess_internal.h:110` | "(SRS-CALIB-FUNC-006 is not implemented)" | EXT 6a 가 생성·적재·적용·배선까지 있습니다. **범위 한정 없이 전체를 부정합니다** |
| `nonlinearity_correct.cpp:95` | "Real coefficients would be loaded from a per-detector calibration profile (SRS-CALIB-FUNC-006, not implemented)" | 같은 파일 `:49` 가 바로 그 프로파일(LUT)을 적재해 적용합니다 |

두 문장 다 **QA-A-111(#186) 이 6a 를 넣기 전에 쓰인 것**이고, 그때는 참이었습니다. 6a 가 들어오면서 거짓이 됐는데 **그 문장을 읽는 상황이 없어서** 조용했습니다 — 어제 알림이 거짓을 말하던 것과 같은 형태입니다.

정확히 쓰면 이렇습니다: *"EXT 6a(LUT)는 구현돼 있다. 이 baseline 경로는 LUT 가 없을 때의 옛 REQ-P1A-012 경로이고 화소를 바꾸지 않는다. EXT 6b(전역 다항식)는 미구현."*

## 그래서 이 이슈는 무엇인가 — 판단은 리더 몫입니다

셋이 성격이 다릅니다.

| 항목 | 성격 | 크기 |
|---|---|---|
| 서술 정정 2건 | **문서/주석** | 작음. 읽는 사람이 "미구현" 으로 읽고 6a 를 다시 만들 위험을 막습니다 |
| EXT 6b 전역 다항식 | **구현** | 생성·적재·적용 + 새 XCAL 타입. 6a 와 같은 크기의 작업 |
| baseline 경로 처리 | **결정** | 남길지(그 역할을 적고) 걷을지. 지금은 이름만 확인하는 관문입니다 |

**6b 가 실제로 필요한지는 이 저장소 안에서 판단할 근거가 없습니다** — "embedded/FPGA use" 라고만 돼 있고, 이 제품이 그 경로를 쓰는지는 SRS 밖의 사실입니다.

## 미검증

- **SRS 외의 문서는 보지 않았습니다.** SAD·RTM·TDS 가 FUNC-006 을 어떻게 추적하는지(`RTM:45` 은 FUNC-006 을 "Validate offset map dimensions" 로 적고 있어 **본문과 다릅니다** — 별개 문제일 수 있습니다) 확인하지 않았습니다.
- **`clients`·`gui` 가 `xpe_nonlinearity_*` 를 어떻게 쓰는지** 보지 않았습니다(다른 레인 소유).
- **시험이 무엇을 덮는지** 세지 않았습니다. 배선은 코드로 확인했지만 "파이프라인 통과 시험이 LUT 적용을 실제로 단언하는가" 는 안 봤습니다.
- `kKnownModes` 세 이름이 어디서 왔는지(실제 검출기 모드인지) 확인하지 않았습니다.
- **빌드·시험을 돌리지 않았습니다** — 코드 변경이 없어 필요하지 않았습니다.

## 잔여 위험

- LUT 분기가 모드 검사보다 앞이라는 점이 문서화돼 있지 않습니다. 6b 를 넣을 때 우선순위를 다시 정해야 합니다(LUT 우선? 모드 우선? 둘 다 있으면?) — `#187` 의 "마지막에 적재한 것이 이긴다" 와 같은 질문입니다.
- `RTM:45` 의 FUNC-006 설명이 SRS 본문과 다릅니다. 추적표가 다른 요구를 가리키고 있다면 추적이 끊긴 것입니다.
