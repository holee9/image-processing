# QA-B-121 게이트 — #162 config 3건 · REQ-DISP-009 허용오차

커밋: `aae8d61` (`modules/display/tests/test_voi_lut.cpp` 1파일)

## 1. 주장 (Claim)

1. **#162 (1) `levels` 덮어쓰기 — 전제가 낡았습니다.** 이미 QA-B-79 에서 고쳐졌고, 시험으로도 박혀 있습니다. 새로 고칠 것도, 새로 만들 시험도 없습니다.
2. **#162 (2) 레벨별 게인 무효화 — 절반은 설계, 절반은 분기 순서 사고입니다.** `num_levels=3` 은 설계, `num_levels=2` 는 결함입니다.
3. **#162 (3) `step_size` — 요구 문서에 근거가 없습니다.** 설계 문서에만 있고 REQ-ADV-011 본문에는 나오지 않습니다. 선택지만 적고 구현하지 않았습니다.
4. **공통 처방** — 기존 `warn_unconsumed_keys_once` 와 같은 자리에 "이름은 아는데 효과 없는 키" 경고를 붙이는 것이 셋 모두의 근본 처방입니다.
5. **REQ-DISP-009 허용오차 0.05 로 조였고, 그것이 실제로 식을 잡는 것을 반증으로 확인했습니다.** 세 프리셋 + e2e 전부 초록입니다.

## 2. 증거 (Evidence)

### 2-1. #162 (1) — 이미 고쳐져 있음

`modules/enhance_advanced/src/enhance_advanced_helpers.cpp:171-181`

```cpp
// Backward compat: also accept "levels" (flat schema legacy key).
// Read FIRST so that "num_levels", when also present, overwrites it:
// the current name wins over the legacy one (#162, QA-B-79).
if (src.contains("levels") ...) outLevels = std::clamp(...);
if (src.contains("num_levels") ...) outLevels = std::clamp(...);
```

읽는 순서가 이미 뒤집혀 있어 **`num_levels` 가 이깁니다**. 시험도 있습니다 —
`modules/enhance_advanced/tests/test_config_value_dependency.cpp` 의
`NumLevelsWinsOverLegacyLevels` 는 둘 다 준 경우 `num_levels` 가 이기는 것과
**키 순서가 결과를 바꾸지 않는 것**까지 단언합니다.

이슈 본문은 2026-09-17 시점 기준이고, 그 사이에 QA-B-79 가 지나갔습니다.
고른 쪽은 "뒤엣것(현재 이름) 우선", 이유는 legacy 키가 현재 키를 덮으면
설정 파일을 최신 스키마로 옮길 방법이 없어지기 때문입니다.

### 2-2. #162 (2) — 설계/결함 판정

`modules/enhance_advanced/src/mfp_scalar.cpp:130-138`

```cpp
float gain = 1.0f;
if (level == numLevels_ - 2) gain = config.flatGain;   // Coarsest details
else if (level == 0)         gain = config.edgeGain;   // Finest details (edges)
else                         gain = config.textureGain;
```

`LaplacianPyramid::reconstruct` 의 루프는 `level = numLevels_-2 .. 0` 이므로
디테일 대역 수는 `numLevels - 1` 개입니다.

| num_levels | 디테일 대역 | 판정 |
|---|---|---|
| 4 (기본) | level 2,1,0 → flat/texture/edge 전부 도달 | 정상 |
| 3 | level 1,0 → `level==1` 은 첫 분기(1==3-2)에 잡혀 flat, level 0 은 edge. **중간 대역 자체가 없음** | **설계** — `texture_gain` 이 곱해질 대상이 없습니다 |
| 2 | 디테일 대역이 level 0 하나. 그런데 `0 == 2-2` 라 **첫 분기가 먼저 잡아 flat_gain 이 가져가고 `edge_gain` 이 가려짐** | **결함(분기 순서 사고)** — 대상은 있는데 엉뚱한 게인이 적용됩니다 |

- `num_levels=3` 은 문서화 + 경고 쪽입니다(대상이 없는 것이 자연스러움).
- `num_levels=2` 는 선택지입니다. (a) 분기 순서를 뒤집어 `level==0` 을 먼저 검사 → 단일 대역에 `edge_gain` 적용. **출력이 바뀝니다.** (b) `num_levels=2` 를 거절한다. (c) 그대로 두고 "2단에서는 flat_gain 만 유효" 를 문서화 + 경고.
  **결정 대기 — 구현하지 않았습니다.**

이 발산은 이미 `KnownDivergence_LowLevelCountSilencesGains` 로 박혀 있습니다(4단 대조군 포함).

### 2-3. #162 (3) — `step_size` 의 출처

- 파싱·클램프: `enhance_advanced_helpers.cpp:270-273` (`outStepSize`)
- 소비처: `fractional_process.cpp:132` 의 디버그 로그 문자열 **한 곳뿐**
- `FractionalConfig` (`detail/fractional_derivative.h:34`) 에는 `float order;` 만 있고 step 멤버가 없음
- 설계 문서: `01_architect_enhance_advanced_design.md:182`, `docs/project/sdd_adv.md:218-227` — 범위 0.01–1.0, 기본 0.25 (`XPE_FRAC_DEFAULT_STEP`)
- **요구 문서에는 없습니다**: `SPEC-XPE-P2-ADV/spec.md` 에서 `step_size` 검색 0건. 관련 요구 REQ-ADV-011 본문도 이 값을 언급하지 않습니다
- sdd_adv.md:227 에 QA-B-78(2026-09-17)의 실측 기록이 이미 있습니다: 0.01 과 1.0 의 출력 차이 **0.000000000**, 대조군 `iterations` 1 대 3 은 494.17

선택지와 영향:

| 안 | 내용 | 영향 |
|---|---|---|
| (a) | 필드를 만들어 실제로 쓴다 | **출력이 바뀝니다.** 어떤 식으로 쓸지의 근거가 요구에 없어, 정의부터 새로 써야 합니다 |
| (b) | 키를 거절한다 | 호출자가 신호를 받습니다. 기존 설정 파일이 거절당할 수 있습니다 |
| (c) | 문서에서 뺀다 | 출력 불변, 호출자 신호 없음. 이미 파싱은 남습니다 |

**요구 근거가 없다는 점이 (a) 를 약하게 만듭니다.** 결정 대기.

### 2-4. 공통 — "이름은 아는데 효과 없는 키"

현재 `warn_unconsumed_keys_once(cfg, kKnown, n, nested, fn, s_lastWarned)` 는
**kKnown 목록에 없는 키**만 잡습니다. 위 셋은 전부 kKnown 에 있어 통과합니다.

제안: 같은 함수 옆에 **무효 키(inert key)** 경고를 같은 틀로 추가합니다 — 스레드별 중복 억제도, 경보 통로(`xpe_alert_push`)도 그대로 씁니다.

- `levels` 와 `num_levels` 를 둘 다 준 경우 → "`levels` 는 무시됨(`num_levels` 우선)"
- `num_levels<=3` 인데 `texture_gain` 을 준 경우 → "이 단수에서는 적용 대상 없음"
- `step_size` 를 준 경우 → "파싱되지만 출력에 닿지 않음"

세 건의 성격이 달라도 **호출자가 받는 신호는 하나**로 통일됩니다. 이것이 근본 처방입니다.

### 2-5. REQ-DISP-009 허용오차

변경: `modules/display/tests/test_voi_lut.cpp` `VoiLut.Linear_CenterWindow`
`EXPECT_NEAR(pixels(img)[0], 127.5f, 0.5f)` → `0.05f`

- 현재 식(`voi_lut.cpp:36-42`)의 값 127.5, DICOM 표준 LINEAR 식의 값 127.628, 간격 0.128 → 0.5 는 둘 다 통과시킵니다
- **반증**: `voi_lut.cpp` 의 LINEAR 을 표준식(C.11.2.1.2.1)으로 임시 교체 → `VoiLut.Linear_CenterWindow` **만** 빨강. 되돌린 뒤 20건 전부 초록. 임시 패치 흔적(`TEMP-B121`) 이 남아 있지 않은 것도 확인했습니다
- 어느 식이 맞는지는 판정하지 않았습니다

### 2-6. 전체 검증 (타깃 없는 빌드)

`_verify.bat` — 세 프리셋 모두 `cmake --build build\ci-post` 등 **타깃 지정 없음**:

```
===CI_POST===   ===POST_BUILD=0===   100% tests passed, 0 tests failed out of 653   ===POST_EXIT=0===
===CI_AI===     ===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===CI_DICOM===  ===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
===E2E===       100% tests passed, 0 tests failed out of 28   ===E2E_EXIT=0===
```

`BUILD_EXIT` 전부 0 — 낡은 바이너리가 아닙니다. 로그: `.moai/reports/lane-post/QA-B-121/_verify1.log`

## 3. 기준 귀속 (Baseline-attribution)

- 비교 기준은 **이 실행, 이 트리** 입니다. 허용오차 조이기 전/후 모두 같은 타깃 없는 빌드로 돌렸습니다
- #162 전제 판정은 기억이 아니라 현재 소스 줄(위 인용)과 현재 시험 파일에서 읽었습니다
- `step_size` 의 "요구에 없음" 은 `SPEC-XPE-P2-ADV/spec.md` 검색 0건에 근거합니다 — 검색 범위를 이 한 파일로 한정했음을 밝힙니다

## 4. 미검증 (Gaps)

- `num_levels=2` 에서 분기 순서를 뒤집었을 때 출력이 **얼마나** 바뀌는지는 재지 않았습니다(결정 전이라 구현하지 않음)
- `step_size` 를 실제로 쓰는 (a) 안의 수치 영향도 재지 않았습니다
- 무효 키 경고는 **제안만** 했고 구현하지 않았습니다
- `LinearExact_CenterValue` 의 허용오차 1.0 은 그대로 둡니다 — 같은 성질의 느슨함일 수 있으나 이번 카드 범위 밖입니다
- 요구 문서 검색은 `SPEC-XPE-P2-ADV/spec.md` 한 파일 범위입니다. 다른 SPEC 에 `step_size` 근거가 있을 가능성은 배제하지 못했습니다

## 5. 잔여 위험 (Residual-risk)

- 0.05 는 이 플랫폼·이 컴파일러의 부동소수 결과에 대해 여유가 0.128-0.05 만큼 남아 있습니다. 다른 부동소수 설정에서 경계에 닿을 가능성은 남습니다(현재 20건 초록)
- `#156` 에서 LINEAR 식을 고치기로 결정되면 **이 시험이 즉시 빨강이 됩니다.** 그것이 이 조이기의 의도입니다 — 식 변경이 조용히 지나가지 않게 하는 것
- QA-B-79 이후 `levels` 경로가 다시 바뀌면 `NumLevelsWinsOverLegacyLevels` 가 잡습니다
