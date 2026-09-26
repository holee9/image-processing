# QA-B-122 게이트 — #162 결정 셋 실행

커밋: `06dcc88` (8파일: enhance_advanced 헤더 1 + 소스 3 + 시험 2, display 시험 1... 실제 8파일은 아래 목록)

변경 파일:
`internal.h`, `enhance_advanced_helpers.cpp`, `fractional_process.cpp`, `mfp_scalar.cpp`,
`multiscale_process.cpp`, `test_config_value_dependency.cpp`, `test_config_warning_once.cpp`,
`test_voi_lut.cpp`

## 1. 주장 (Claim)

1. **`num_levels=2` 사용처는 시험 한 곳뿐입니다** — 착수 전에 열거했고, 설정 파일·클라이언트 호출자는 0건입니다. 그래서 멈추지 않고 고쳤습니다.
2. **분기 순서를 고쳤고, `edge_gain` 이 실제로 걸리는 것을 수치로 확인했습니다** — maxdiff 0 → 150.909546.
3. **`num_levels=3` 의 `texture_gain` 은 건드리지 않았습니다.** 설계이므로 경고 대상으로만 돌렸습니다.
4. **무효 키 경고를 붙였고, 반증을 양쪽으로 했습니다** — 무효 키면 뜨고, 유효 키만이면 안 뜹니다.
5. **`step_size` 는 구현하지 않았습니다.** 경고 대상으로만 두었습니다.
6. **`LinearExact_CenterValue` 허용오차를 0.05 로 조였고 반증했습니다.**
7. 세 프리셋 + e2e 전부 초록, `BUILD_EXIT` 전부 0.

## 2. 증거 (Evidence)

### 2-1. `num_levels=2` 사용처 열거 (착수 전)

검색 범위: **저장소 전체** (`.git`, `build` 제외), 패턴 `"(num_)?levels"\s*:\s*2`, `num_levels\s*=\s*2`, `numLevels\s*=\s*2`.
추가로 `*.json / *.ini / *.yaml / *.yml / *.cs / *.xaml` 전수 검색 — **0건**.

| 위치 | 성격 | 고친 뒤 영향 |
|---|---|---|
| `test_config_value_dependency.cpp:282-287` (`KnownDivergence_LowLevelCountSilencesGains`) | 시험 — 2단에서 `edge_gain` 이 죽는다는 기대 | **기대를 뒤집었습니다**(아래) |
| `test_config_value_dependency.cpp:231-233` (`NumLevelsWinsOverLegacyLevels`) | 시험 — `levels:2` 를 "5와 구별되는 값" 으로만 씀 | 출력 수치는 바뀌되 5단과 여전히 구별되어 초록 유지 (실행으로 확인) |
| 설정 파일 | — | **없음** |
| `clients/`·GUI 호출자 | — | **없음** |
| `modules/preprocess` 의 `num_levels` | **다른 API**(gain 생성). MFP 와 무관 | 없음 |

**멈출 사유가 없어 진행했습니다.**

### 2-2. 분기 순서 정정

`modules/enhance_advanced/src/mfp_scalar.cpp`

```cpp
// Level 0 is tested FIRST: at numLevels_ == 2 the single detail band is
// level 0 and also satisfies `level == numLevels_ - 2`, ...
float gain = 1.0f;
if (level == 0) {
    gain = config.edgeGain;      // Finest details (edges)
} else if (level == numLevels_ - 2) {
    gain = config.flatGain;      // Coarsest details
} else {
    gain = config.textureGain;   // Mid-level details (texture)
}
```

바뀌는 것은 `numLevels == 2` 뿐입니다. 3단은 level 1(=3-2)이 flat, level 0 이 edge 로 이전과 같고, 4단 이상은 level 0 이 `numLevels-2` 와 겹치지 않습니다.

**실측** (`ConfigValueDependency` `-V` 출력, 이 실행):

```
[  INFO ] gain reach by level count: tex@3=0.000000 edge@2=150.909546 tex@2=0.000000 (threshold 0.080585)
```

- `edge@2` 가 **0 → 150.909546**. 고침 전에는 문턱 아래였고 지금은 넘습니다 — `edge_gain` 이 실제로 걸립니다.
- `tex@2`, `tex@3` 은 0 그대로 — 설계인 쪽은 안 건드렸습니다.

시험도 같은 방향으로 뒤집었습니다(`test_config_value_dependency.cpp`): 기존 `EXPECT_LT(edge2, ...)` → `EXPECT_GT(edge2, ...)`, 실패 문구는 "분기 순서가 되돌아갔다" 로. **값을 바꾸면 출력이 바뀌어야 한다**는 카드 요구를 이 단언이 박습니다.

### 2-3. 무효 키 경고

새 API (`internal.h`):

```cpp
struct InertKey { const char* key; const char* reason; };

void warn_inert_keys_once(const char* json, const InertKey* inertKeys, size_t inertCount,
                          const char* nestedObject, const char* fnLabel, std::string& lastWarned);
```

기존 `warn_unconsumed_keys_once` 와 **같은 틀**입니다 — 스레드별 `lastWarned` 로 중복 억제, 키 이름 집합을 서명으로 비교, `xpe_alert_push(..., XPE_ALERT_WARNING)` 로 같은 통로. 목록이 비면 기억을 지우는 것(같은 실수를 다시 하면 다시 들리게)도 같습니다.

**설계 판단 하나**: 목록에 있는 키 전부가 아니라 **호출자가 실제로 쓴 키만** 경고합니다. 안 쓴 키까지 경고하면 모든 설정에서 떠서 아무것도 구분하지 못합니다. 중첩 `mfp` 객체도 함께 봅니다 — 파서가 두 스키마를 다 받으므로, 한쪽만 보면 호출자 절반에게 침묵합니다.

붙인 곳:

| 진입점 | 키 | 무효인 이유 | 조건 |
|---|---|---|---|
| `xpe_fractional_process` | `step_size` | `FractionalConfig` 에 담길 필드가 없음 | 항상 |
| `xpe_multiscale_process` | `texture_gain` | `num_levels <= 3` 이면 중간 대역이 없음 | 값 의존 |

**반증 — 양쪽 다** (`test_config_warning_once.cpp`, 새 시험 6건, 전부 초록):

| 시험 | 설정 | 기대 | 결과 |
|---|---|---|---|
| `InertKey_TextureGainAtThreeLevelsWarns` | `num_levels:3, texture_gain` × 100프레임 | 경고 1건, "no effect" 포함 | 통과 |
| `InertKey_NestedTextureGainWarns` | `mfp:{num_levels:2, texture_gain}` | 경고 1건 | 통과 |
| `InertKey_TextureGainAtFourLevelsIsSilent` | `num_levels:4, texture_gain` × 100 | **0건** | 통과 |
| `InertKey_ThreeLevelsWithoutTextureGainIsSilent` | `num_levels:3, edge_gain` × 100 | **0건** | 통과 |
| `InertKey_FractionalStepSizeWarns` | `step_size` × 100 | 경고 1건 | 통과 |
| `InertKey_FractionalIterationsIsSilent` | `iterations` × 100 | **0건** | 통과 |

침묵 쪽 3건이 이 경고가 무언가를 실제로 구분한다는 증거입니다. 100프레임에 1건인 것은 기존 경고와 같은 억제가 걸린다는 뜻입니다.

### 2-4. `step_size` — 구현하지 않음

요구에 근거가 없으므로 동작을 발명하지 않았습니다. 경고 문구에 그 사실을 적었습니다:
`"the fractional derivative uses a fixed step; no requirement defines this value's effect"`.
`KnownDivergence_FractionalStepSizeIsReadAndDiscarded` 는 그대로 남아 출력 불변(비트 동일)을 계속 단언합니다.

### 2-5. `LinearExact_CenterValue` 허용오차

`1.0f` → `0.05f`. 중심값은 정확히 127.5 입니다.

**반증**: `voi_lut.cpp` 의 `XPE_VOI_LINEAR_EXACT` 를 C.11.2.1.2.1 LINEAR 형태로 임시 교체(`TEMP-B122`) → 재빌드(`BUILD=0`) → `VoiLut` 23건 중 **`LinearExact_CenterValue` 하나만 빨강**. 되돌린 뒤 `TEMP-B122` 잔존 0건, `git diff` 0줄.
로그: `_falsify_linearexact.log`

부수로 확인한 것 — 이 교체의 값은 129.114 로 127.5 에서 **1.614** 떨어져 있어, 옛 허용오차 1.0 도 이 특정 교체는 잡았습니다. 다만 여유가 0.6 밖에 없었고, 0.05 에는 그런 여유가 없습니다. **"범위 밖" 이라던 제 B-121 보고가 틀렸고, 같은 파일·같은 성질이라는 지적이 맞습니다.**

### 2-6. 전체 검증 (타깃 없는 빌드)

```
===CI_POST===   ===POST_BUILD=0===   100% tests passed, 0 tests failed out of 659   ===POST_EXIT=0===
===CI_AI===     ===AI_BUILD=0===     100% tests passed, 0 tests failed out of 225   ===AI_EXIT=0===
===CI_DICOM===  ===DICOM_BUILD=0===  100% tests passed, 0 tests failed out of 194   ===DICOM_EXIT=0===
===E2E===       100% tests passed, 0 tests failed out of 28   ===E2E_EXIT=0===
```

ci-post 653 → **659** (새 시험 6건). 로그: `QA-B-122_verify.log`

## 3. 기준 귀속 (Baseline-attribution)

- `edge@2 = 150.909546` 은 **이 트리, 이 실행**의 `ConfigValueDependency -V` 출력입니다. 고침 전 값 0 은 B-121 게이트에 기록된 같은 시험의 단언(`EXPECT_LT(edge2, 0.080585)`)이 초록이었다는 사실에서 옵니다 — 같은 팬텀, 같은 문턱
- 반증은 패치 → **재빌드 `BUILD=0` 확인** → 실행 → 되돌림의 순서로 했습니다. 낡은 바이너리가 아닙니다
- `num_levels=2` 부재 주장의 검색 범위는 2-1 표 위에 적었습니다

## 4. 미검증 (Gaps)

- 무효 키 목록은 **지금 알려진 둘**뿐입니다. 다른 진입점(`collimation`)에 같은 성질의 키가 있는지는 조사하지 않았습니다
- `num_levels=2` 를 쓰는 **외부** 소비자(이 저장소 밖 설정)는 확인할 수 없습니다
- 경고 문구의 문자열 길이 상한(256바이트)에 걸리는 경우는 만들어 보지 않았습니다 — 현재 두 문구는 여유가 있습니다
- `step_size` 를 요구로 쓸지 말지는 결정되지 않았습니다(리더가 사용자께 올리기로 함)
- `voi_lut.cpp` 의 **LINEAR** 식이 맞는지는 이번에도 판정하지 않았습니다(#156)

## 5. 잔여 위험 (Residual-risk)

- `num_levels=2` 로 돌던 호출자가 저장소 밖에 있다면 **출력이 바뀝니다.** 바뀌는 방향은 "명시한 `edge_gain` 이 이제 적용된다" 이지만, 그 설정을 기준으로 영상을 맞춰 둔 곳이 있으면 다르게 보일 수 있습니다
- 무효 키 경고는 **값 의존**입니다 — `num_levels` 를 프레임마다 바꾸는 호출자는 3단↔4단을 오갈 때마다 경고 억제가 풀려 여러 번 들을 수 있습니다. 기존 미지 키 경고와 같은 성질입니다
- 새 시험 6건은 알림 큐를 공유하므로, 다른 시험이 알림을 남기면 간섭할 수 있습니다. 각 시험이 `xpe_clear_alerts()` 로 시작·종료하는 기존 픽스처를 그대로 씁니다
