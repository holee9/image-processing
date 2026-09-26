# QA-B-79 (#162) 게이트 보고서 — 새 이름이 이기게, 그리고 호출처 없는 두 번째 파서

**카드**: QA-B-79 · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋**: `1b752e3` (`Refs #162`) — `enhance_advanced_helpers.cpp`, `test_config_value_dependency.cpp` · push 없음
**BUILD_EXIT**: 세 프리셋 모두 `BUILD=0` / `EXIT=0`, ctest **533 / 224 / 192**, 실패 0 (`_verify.log`)

---

## 1. 주장

| # | 항목 | 결과 |
|---|---|---|
| 1 | `num_levels` 가 이기도록 읽는 순서 변경 | 완료 (`helpers.cpp:171-181`, `levels` 먼저 읽고 `num_levels` 가 덮어씀) |
| 2 | 어느 값이 적용됐는지 출력으로 단언 | `NumLevelsWinsOverLegacyLevels` — num_levels 단독과 **0.000000000**, levels 단독과 **54.681946** |
| 3 | 옛 시험을 이름과 함께 교체 | `KnownDivergence_LevelsSilentlyOverridesNumLevels` 삭제. 옛 이름의 grep 결과(`modules/`·`docs/`·`.moai/specs`) 0건 |
| 4 | 반증 — 순서를 되돌리면 새 시험만 빨강 | **확인** — 8건 중 1건만 FAILED |
| 5 | `MfpConfig::fromJson` 호출처 | **없음** — 괄호 포함 grep 0건, 링커 확인 `BUILD=0`. 대조군은 `LNK2019` 로 `BUILD=1` |
| 부수 | `FractionalConfig::fromJson` 호출처 | **없음** — 같은 방법으로 `BUILD=0` |
| 부수 | 중첩 `"mfp"` 스키마 | **고아가 아니다** — 실제로 쓰이는 파서 `parse_mfp_config` 가 `helpers.cpp:163-168` 에서 이 스키마를 읽는다 |

## 2. 순서 변경

```cpp
// Backward compat: also accept "levels" (flat schema legacy key).
// Read FIRST so that "num_levels", when also present, overwrites it:
// the current name wins over the legacy one (#162, QA-B-79).
if (src.contains("levels") ...)     outLevels = clamp(...);
if (src.contains("num_levels") ...) outLevels = clamp(...);
```

단일 키만 줄 때의 동작은 바뀌지 않는다. 클램프 범위도 그대로다.

## 3. 시험 — 어느 값이 적용됐는가 (`_green.log`)

```
num_levels=5 + levels=2: maxdiff to num_levels-only=0.000000000, to levels-only=54.681946,
                         key-order-swapped to num_levels-only=0.000000000 (2-vs-5 control 54.681946)
[OK] NumLevelsWinsOverLegacyLevels      8/8 PASSED, BUILD=0
```

- **대조군(ASSERT)**: `levels=2` 단독과 `num_levels=5` 단독의 maxdiff 가 문턱(0.080585)보다 커야 한다. 두 값이 같은 영상을 내면 이 시험은 둘을 구별하지 못하기 때문이다.
- `EXPECT_EQ(0, to5)`: 두 키를 함께 준 출력이 num_levels 단독 출력과 **비트 동일**해야 한다.
- `EXPECT_GT(to2, 문턱)`: 두 키를 함께 준 출력이 levels 단독 출력과 달라야 한다.
- JSON 에서 키를 적는 순서를 바꾼 경우도 비트 동일(`swapTo5 == 0`)을 단언했다. 이 부분은 nlohmann 이 키를 정렬해 보관하므로 원래도 기대되는 결과이고, 단언으로 고정해 둔 것이다.

## 4. 반증 (`_falsify.log`)

파서 파일만 `git show HEAD:` 로 되돌렸다. `BUILD=0`, `helpers.cpp` 가 다시 컴파일된 것을 로그에서 확인했다.

```
num_levels=5 + levels=2: maxdiff to num_levels-only=54.681945801, to levels-only=0.000000, key-order-swapped ...=54.681945801
the applied value is not num_levels=5
[  FAILED  ] ConfigValueDependency.NumLevelsWinsOverLegacyLevels
[  PASSED  ] 7 tests.      ===EXIT=1===
```

원복 후 `git diff --stat` 에서 helpers 12줄 변경을 확인했고, 이어 전체 검증이 초록이었다.

## 5. `MfpConfig::fromJson` — 호출처 확인

### 5.1 grep (괄호 포함, 정의 줄 제외)

`MfpConfig|fromJson\s*\(` 를 `*.cpp *.h *.hpp *.c *.cc *.cmake *.txt` 에서 찾았다(`build/` 제외).
- `MfpConfig::fromJson`: 정의(`mfp_scalar.cpp:16`)와 선언(`mfp_scalar.h:24`)만 있고 **호출은 0건**이다.
- `MfpConfig` 라는 이름은 `multiscale_process.cpp:102` 에서도 쓰이지만, 기본 생성 후 필드를 직접 채울 뿐 `fromJson` 은 부르지 않는다.

### 5.2 링커 (`_build.bat` = `cmake --build build\ci-post`)

| 실험 | 방법 | 결과 |
|---|---|---|
| **대상** `MfpConfig::fromJson` | 정의 이름을 자유 함수 이름으로 바꿈(선언은 그대로) | `[1/4] mfp_scalar.cpp.obj` 재컴파일, `[2/4] Linking xpe_enhance_advanced.dll` → **`BUILD=0`** (`_link_mfp.log`) |
| **대조군** `config::parse_mfp_config` | 같은 방법 | `multiscale_process.cpp.obj : error LNK2019 … parse_mfp_config …`, `LNK1120` → **`BUILD=1`** (`_link_control.log`) |
| **부수** `FractionalConfig::fromJson` | 이름 변경은 **컴파일 오류(C3861 `defaultConfig`)** 가 나서 판정 불가 → 정의 전체를 `#if 0` 으로 감쌈 | 재컴파일, DLL 링크 → **`BUILD=0`** (`_link_frac.log`) |

세 실험 모두 `git checkout --` 로 원복했다. 원복 후 빌드 `BUILD=0` 을 확인했다(`_restored_build.log`).

**결론(측정)**: ci-post 가 빌드하는 모든 타깃에서 두 `fromJson` 모두 참조가 없다. 두 함수 모두 지우지 않았다.

### 5.3 B-61 의 중첩 스키마와의 관계

- 중첩 `"mfp"` 스키마는 **이 파서만을 위한 것이 아니다.** 실제로 쓰이는 파서 `parse_mfp_config` 가 `helpers.cpp:163-168` 에서 `cfg["mfp"]` 가 객체이면 그 안에서 키를 읽는다.
- 미지 키 경고도 `nestedObject="mfp"` 로 호출된다(`multiscale_process.cpp:89-91`).
- 따라서 "파서는 없는데 스키마만 남은" 상태는 **아니다.**
- 남아 있는 것은 **같은 스키마를 다른 클램프(`[1,6]` 대 `[2,8]`)로 읽는, 쓰이지 않는 두 번째 구현**이다.

## 6. 미검증

- 링커 확인은 **ci-post 빌드 타깃만** 대상으로 했다. `mfp_scalar.h` 는 `src/` 안의 내부 헤더라 다른 모듈의 include 경로에 없으므로 ai·dicom 프리셋은 따로 돌리지 않았다. 다만 전체 검증 단계에서는 세 프리셋 모두 원본 소스로 빌드됐다.
- 두 번째 파서가 **왜 생겼는지**(이력)는 보지 않았다.
- SDD 에 `num_levels` 를 추가하는 일은 리더 몫이라 하지 않았다.

## 7. 잔여 위험

- 기존 호출자가 두 키를 함께 보내면서 `levels` 값이 적용되기를 기대했다면 동작이 바뀐다. 저장소 안에서 두 키를 **한 config 에 함께** 쓰는 곳은 이 시험뿐이다. 확인 방법: `"levels"` 와 `num_levels` 를 둘 다 포함하는 `*.cpp *.h *.json *.py *.cs *.md` 파일을 찾았다(`build/` 제외). 나온 것은 파서 2개, 이 시험, `test_coverage_ext.cpp`(주석뿐), 보고서 2개다. 저장소 밖 호출자는 알 수 없다.
- 호출처 없는 두 `fromJson` 은 그대로 남아 있다. 나중에 누가 이것을 부르면 다른 클램프 범위가 적용된다.

## 부록 — 증거

`_env.bat`, `_build.bat`, `_b79.bat`, `_verify.bat`, `_link_mfp.log`, `_link_control.log`, `_link_frac.log`, `_restored_build.log`, `_green.log`, `_falsify.log`, `_verify.log`
