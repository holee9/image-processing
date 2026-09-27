# QA-A-126 (#186) — 거짓 거부는 **실재합니다**. 그리고 REQ 인용이 전부 다른 요구를 가리킵니다

Lane A (pre), `dev/preprocess`. **코드 변경 없음 — 측정과 조사만.**
`BUILD_EXIT=0`, `CTEST_EXIT=0`.

## (a) 도달합니다 — 실측

`xpe_preprocess_pipeline()` 에 설정만 바꿔 넣고 실제로 태웠습니다. 8×8, 값 1000, LUT 미적재, `calibPath=nullptr`.

```
[a126] CONTROL no mode key           ...bypass 7개...                          -> rc=0
[a126] detector mode standard        ...bypass 7개..., "mode":"standard"       -> rc=0
[a126] OPERATING mode clinical       ...bypass 7개..., "mode":"clinical"       -> rc=-4
[a126] OPERATING mode research       ...bypass 7개..., "mode":"research"       -> rc=-4
[a126] clinical + panel.linear true  ...,"mode":"clinical","panel.linear":"true" -> rc=0
[a126] clinical + nonlin bypassed    ...,"bypassNonlinearity":true,"mode":"clinical" -> rc=0
[a126] XPE_ERR_CONFIG_INVALID = -4
```

**`-4` 는 `XPE_ERR_CONFIG_INVALID` 입니다. 파이프라인 전체가 실패합니다.**

**대조군이 셋입니다** — 이게 없으면 "전부 실패하는 프로브" 와 구별이 안 됩니다:

1. `"mode"` 키 없음 → `rc=0`. **경로가 살아 있고 통과합니다.**
2. `"mode":"standard"`(검출기 모드) → `rc=0`. **목록에 있는 이름은 통과합니다.**
3. `panel.linear":"true"` 또는 `bypassNonlinearity` → `rc=0`. **stage 3 를 건너뛰면 사라집니다.** 거부가 이 단계에서 난다는 확인입니다.

### 첫 측정은 눈이 멀어 있었습니다

처음에 `{"bypassGain":true}` 만 넣고 쟀더니 **여섯 줄이 전부 `rc=-16`** 이었습니다. `XPE_ERR_CALIB_NOT_LOADED` 로, **오프셋 보정이 stage 3 전에 막고 있었습니다.** 대조군("mode 키 없음")까지 같은 값이라 알아챘습니다 — 기준선이 없었으면 *"모드와 무관하게 실패한다"* 로 잘못 읽었을 것입니다.

앞 단계를 전부 우회한 뒤에야 차이가 드러났습니다.

## (b) 세 이름의 출처 — **아무 데도 없습니다**

`{"standard", "high_gain", "low_dose"}` 를 `docs/` 와 `.moai/specs/` 전체에서 찾았습니다.

| 이름 | 결과 |
|---|---|
| `standard` | 일반 영어 단어로만 잡힘(`"IEC standards"`, `"test coverage standard"`, `"standard image display"`). **검출기 모드로 쓰인 곳 0건** |
| `high_gain` | `XPE-ALG-001:1481` 한 곳 — **다른 뜻**입니다. `high_gain = gain_map > gain_mean * 2.0` 로, 결함 검출의 **화소 마스크 변수**이지 모드 이름이 아닙니다 |
| `low_dose` | `docs/display/` 의 `pediatric_low_dose` — **디스플레이 LUT 이름**이고 전처리 모드가 아닙니다 |

**세 이름을 검출기 모드로 정의한 문서가 없습니다.** 요구에 없는 세 이름이 파이프라인을 거부하고 있습니다.

## (c) REQ-P1A-014 — **"모르는 모드 거부" 가 아닙니다.** 그리고 더 있습니다

SPEC 원문(`.moai/specs/SPEC-XPE-P1A/spec.md`)을 읽었습니다. **주석이 인용한 네 개가 전부 다른 요구입니다.**

| 주석의 인용 | SPEC 의 실제 정의 |
|---|---|
| `:98` "REQ-P1A-014: unknown mode -> XPE_ERR_CONFIG_INVALID" | **`:335` REQ-P1A-014: Calibration File Loading (Offset)** — `xpe_calib_load_offset(filepath)` |
| `:91` "REQ-P1A-013: no-op when no config supplied" | **`:184` REQ-P1A-013: Runtime Defect Detection** — `xpe_defect_detect_runtime()` |
| `:105` "REQ-P1A-012/015: apply identity polynomial" | **`:167` REQ-P1A-012: Defect Correction Execution** / **`:342` REQ-P1A-015: Calibration File Loading (Gain)** |
| `:18` `@MX:SPEC: REQ-P1A-012` | 같음 — 결함 보정 |

**그리고 비선형 보정은 이 SPEC 의 범위가 아닙니다.** 같은 파일이 세 곳에서 말합니다:

- `:49` — "본 SPEC 범위 14개 함수 (**Ghost/Temp/Nonlinearity/Binning 제외**)"
- `:55` — "**PRE-08**: Nonlinearity Correction — **별도 SPEC**"
- `:560` — `xpe_nonlinearity_correct()` | Separate SWU | **SPEC-XPE-P1D**

**그 `SPEC-XPE-P1D` 는 `.moai/specs/` 에 없습니다**(디렉터리 20개를 확인했고 `P1A`·`P1B-DICOM`·`P1B-DISP`·`P1B-ENH`·`P2-ADV`·`P3-AI` 는 있으나 `P1D` 없음).

### 그래서 (c) 의 답

**"모르는 모드는 거부" 는 이 저장소 안 어느 요구에도 없습니다.** 인용된 번호는 결함 보정과 교정 적재를 가리키고, 비선형 보정을 다룰 SPEC 은 이름만 있고 존재하지 않습니다.

거부 자체가 **근거 없이** 서 있습니다.

## 세 답을 합치면

카드가 제시한 세 갈래 중 **첫 번째입니다**:

> 거짓 거부가 **실재하면** → 목록을 고치거나 두 "mode" 를 분리하는 것이 먼저이고, 걷는 것은 그 다음

- **도달 불가가 아닙니다** — `rc=-4` 를 봤습니다
- **REQ-P1A-014 가 살아 있는 요구가 아닙니다** — 다른 것을 가리킵니다
- **세 이름은 출처가 없습니다**

즉 지금 상태는 **"출처 없는 세 이름이, 근거 없는 거부로, 실제 설정을 막고 있다"** 입니다.

**결정은 하지 않았습니다** — 카드가 (a)(b)(c) 뒤라고 했고, 선택지가 여럿입니다(키 분리 / 목록 제거 / 거부 제거 / 경로 전체 제거). 어느 것이든 **동작 변경**이라 리더 몫입니다.

## 6b 가 들어오면 거짓이 될 주석 세 곳

`#186` 에 적어 둡니다.

| 파일 | 줄 | 지금 무엇을 말하나 |
|---|---|---|
| `modules/preprocess/include/xpe/preprocess/xpe_preprocess_internal.h` | 110-126 | "EXT 6b … is NOT implemented; there is no generate, load, or apply for it and no XCal type" |
| `modules/preprocess/src/nonlinearity_correct.cpp` | 48-59 | "when EXT 6b (the global polynomial) arrives there will be two, and LUT-vs-polynomial has to be decided then" |
| `modules/preprocess/src/nonlinearity_correct.cpp` | 105-117 | "What is missing is EXT 6b … no generate, load, or apply, and no XCal type for it" |

## baseline 귀속

- (a) 는 임시 프로브 `test_a126_probe.cpp` 의 stdout 입니다. **커밋 전에 삭제**했고 `CMakeLists.txt` 등록도 되돌렸습니다.
- 빌드: `cmake --build build/ci-preprocess --config RelWithDebInfo` (타깃 미지정) → `BUILD_EXIT=0`
- 전체: `ctest --test-dir build/ci-preprocess -C RelWithDebInfo` → `CTEST_EXIT=0`
- (b)(c) 는 `docs/`·`.moai/specs/` 를 `grep -rn` 한 결과이고, 인용한 줄 번호는 모두 직접 읽었습니다.

## 미검증

- **`panel.linear` 의 실제 기본값**을 확인하지 않았습니다. 프로브는 키를 안 넣었고, 그 경우 `xpe_json_get_string` 이 빈 문자열을 주어 `"true"` 가 아니므로 통과합니다 — **설정 파일이 기본으로 무엇을 넣는지**는 다른 문제입니다.
- **실제 `clients`·`gui` 가 `"mode"` 를 담은 설정을 파이프라인에 넘기는지** 확인하지 않았습니다(다른 레인 소유, 읽기만 가능). 거짓 거부가 **실사용에서 나고 있는지**는 그것에 달렸습니다 — 이 측정은 "날 수 있다" 까지입니다.
- `SPEC-XPE-P1D` 가 **다른 이름으로** 존재하는지(예: `SPEC-XPE-P1B-ENH` 안에) 내용까지 뒤지지 않았습니다. 디렉터리 이름만 확인했습니다.
- `REQ-P1A-012~015` 번호가 **과거 판본에서 달랐을 가능성**은 확인했지만(변경 이력 `:27-30`) 옛 판본 원문을 읽지는 못했습니다. 지금 판본 기준의 불일치입니다.
- 나머지 stage 들이 `"mode"` 를 읽는지 보지 않았습니다.

## 잔여 위험

- **이 거부가 언제부터 있었는지 모릅니다.** `"mode"` 를 담은 설정을 쓰는 쪽이 있었다면 이미 실패하고 있었을 것이고, 없었다면 앞으로 누가 넣는 순간 실패합니다.
- **주석의 REQ 번호가 전부 틀린 것**은 이 파일만의 문제가 아닐 수 있습니다. 다른 파일의 `REQ-P1A-*` 인용을 대조하지 않았습니다.
