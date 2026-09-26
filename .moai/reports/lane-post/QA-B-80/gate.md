# QA-B-80 (#162) 게이트 보고서 — 호출처 없는 두 파서 삭제

**카드**: QA-B-80 · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋**: `6221931` (`Refs #162`) — 4파일, 113줄 삭제 · push 없음
**BUILD_EXIT**: 세 프리셋 모두 `BUILD=0` / `EXIT=0`, ctest **533 / 224 / 192**, 실패 0 (`_verify.log`)

---

## 1. 주장

| # | 항목 | 결과 |
|---|---|---|
| 1 | `MfpConfig::fromJson` 삭제 (정의 + 선언) | `mfp_scalar.cpp` 정의 47줄, `mfp_scalar.h` 선언 |
| 2 | `FractionalConfig::fromJson` 삭제 (정의 + 선언 + 주석 블록) | `fractional_derivative.cpp` 정의 52줄, `detail/fractional_derivative.h` 선언 |
| 3 | 두 `.cpp` 에서 이 함수만 쓰던 `#include <nlohmann/json.hpp>` 제거 | 삭제 후 두 파일의 `nlohmann` 등장이 include 줄 1건뿐임을 grep 으로 확인하고 뺐다. 빌드 `BUILD=0` |
| 4 | 커밋 메시지에 근거 세 가지 | 적었다 (호출처 0 / 같은 스키마·다른 클램프 / git 보존) |
| 5 | 삭제 후 두 이름이 저장소에 0건 | **0건** — 같은 패턴이 삭제 전 HEAD 에서는 4건 (대조군) |
| 6 | 두 번째 구현이 생긴 이력 | §3 |

## 2. 반증과 증거의 구분

- 삭제 후 빌드와 전체 시험이 초록인 것은 **증거가 아니다.** 호출처가 없었으니 당연한 결과다.
- 호출처가 없다는 증거는 **QA-B-79 의 링커 확인**이다.
  - 대상: 정의를 빼도 `BUILD=0`
  - 대조군 `parse_mfp_config`: 같은 방법으로 빼면 `LNK2019`/`LNK1120` 로 `BUILD=1`

**이름 0건 확인** (`_grep_control.txt`, `_grep_after.txt`)

```
== HEAD (before)   git grep -E '(MfpConfig|FractionalConfig)::fromJson|fromJson\(' HEAD -- modules/enhance_advanced
  detail/fractional_derivative.h:52   static FractionalConfig fromJson(...)
  fractional_derivative.cpp:101       FractionalConfig FractionalConfig::fromJson(...)
  mfp_scalar.cpp:16                   MfpConfig MfpConfig::fromJson(...)
  mfp_scalar.h:24                     static MfpConfig fromJson(...)
== worktree (after)  (같은 패턴)       0건
```

**저장소 전체**(`build/`·`.git/`·`reports/` 제외, 모든 파일 형식):
- `(MfpConfig|FractionalConfig)::fromJson` 에 걸린 것은 `.moai/specs/SPEC-XPE-P2-ADV/_workspace/02_algorithm_enhance_advanced_notes.md:250` 1건뿐이다.
- 이것은 2026-04 설계 노트의 과거 서술이고 리더 소유 경로라 손대지 않았다.
- `modules/`·`tests/` 에 남은 `fromJson` 은 `preprocess/src/pipeline.cpp` 의 `PipelineConfig::fromJson` 뿐이다. 이것은 다른 클래스이고 실제로 호출된다(:329, :407, :452).

## 3. 이력 — 두 번째 구현이 왜 생겼나 (`git log -S`)

```
MfpConfig::fromJson         → 10b5551 (2026-04-18 스켈레톤), f057d9e (2026-04-20)
FractionalConfig::fromJson  → 10b5551, f057d9e
parse_mfp_config / parse_fractional_config → f057d9e
```

- **`10b5551`(스켈레톤)**: 두 `fromJson` 을 만들었고, 진입점이 이것을 **직접 불렀다**.
  - `+ auto config = xpe::enhance_advanced::MfpConfig::fromJson(configJsonOrNull);`
  - `+ auto config = xpe::enhance_advanced::FractionalConfig::fromJson(configJsonOrNull);`
  - 같은 커밋에 `CollimationConfig::fromJson` 도 있었다. 이것은 현재 `modules/` 에 없다.
- **`f057d9e`**: `helpers.cpp` 에 `parse_mfp_config`/`parse_fractional_config` 를 새로 만들고 두 호출을 그리로 바꿨다. 이 커밋 diff 에 위 두 줄의 `-` 가 있다. 이때 **정의와 선언만 남았다.**
  - 같은 커밋의 노트: "parse_mfp_config() did not support nested "mfp" JSON key; test config was silently ignored" — 이후 새 파서에 중첩 스키마 지원이 추가됐다.
  - 같은 커밋의 설계 노트: "MfpConfig::fromJson uses `noiseThreshold = 0.02f` … internal.h `5.0f` … inconsistency must be resolved"
- 정리: 두 번째 구현은 **교체 과정에서 옛 구현을 지우지 않아서** 남은 것이다(이력상 사실). 교체한 이유는 위 노트 문장이 전부이고, 더 찾아보지 않았다.

## 4. 삭제 범위의 정확성

- `MfpConfig` 구조체의 필드와 기본값은 그대로다. `multiscale_process.cpp:102` 가 기본 생성 후 필드를 채워 쓴다.
- `FractionalConfig::defaultConfig()` 는 남겼다. 헤더의 인라인 함수이고, 이번 삭제 대상이 아니다. 호출처가 있는지는 이번에 확인하지 않았다.
- SAF-100 금지 키 검사는 현행 `parse_fractional_config` 에 그대로 있다. 금지 키 5개이고, 삭제된 사본은 4개였다. `T305`·`DisableOvershootViaConfigRejected` 가 전체 시험에서 통과했다.

## 5. 미검증

- `FractionalConfig::defaultConfig()` 의 호출처는 확인하지 않았다.
- `build/asan-adv` 등 이번에 다시 빌드하지 않은 빌드 디렉터리에는 옛 오브젝트가 남아 있다(grep 에 걸림). 소스와는 무관하다.
- `_workspace/` 설계 노트의 과거 서술은 고치지 않았다(리더 소유).

## 6. 잔여 위험

- 없음에 가깝다. 호출처가 없다는 것은 링커로, 이름이 0건이라는 것은 대조군과 함께 grep 으로 확인했다. 저장소 밖 코드가 이 내부 헤더(`src/`)를 쓸 경로는 include 경로상 없다. 다만 이 점은 판독으로만 확인했다.

## 부록 — 증거

`_env.bat`, `_verify.bat`, `_verify.log`, `_grep_after.txt`, `_grep_control.txt`, QA-B-79 `_link_*.log`
