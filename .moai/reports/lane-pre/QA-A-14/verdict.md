# QA-A-14 검증 보고서 — common·preprocess 오류코드 우선순위 프로브 (#119)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-14 (`.moai/lanes/pre/inbox/QA-A-14.md`, 2026-09-10 leader 정정 포함)
- 정본: `docs/project/api-spec.md:1485` "Error code precedence (normative, 2026-09-09; narrowed same day per #119)"

## 1. 주장 (Claim)

1. **계약 위반 0건.** common 10행 + preprocess 28행, 합계 **38행 전부** 미초기화·초기화 두 상태에서 `XPE_ERR_INVALID_INPUT`(-1) 을 반환했다. 고칠 것이 없다.
2. 프로브를 `CommonErrorPrecedenceTest` / `PreprocessErrorPrecedenceTest` 두 케이스로 남겨 회귀를 막는다. 각 케이스는 표를 stdout 에 찍으므로, 나중에 값이 바뀌면 실패와 함께 어느 행인지 드러난다.
3. 재실측 무회귀. ci-preprocess 497 → **499/499 PASS**, ci-common 73 → **74/74 PASS** (+2 = 프로브 2케이스).

### 카드 대비 좁힌 범위 — 이것이 이 카드의 핵심 판단

카드 "계약" 절은 `INVALID_INPUT → NOT_INITIALIZED → 내용 검증 → 처리 오류` 라는 **4단 순서**를 적었다. 그러나 정본(api-spec:1487-1491)은 같은 날 #119 로 **좁혀졌고**, 현재 문구는 이렇다:

> 2. **After the null checks the order is implementation-defined** between `XPE_ERR_NOT_INITIALIZED` and content validation ... Reference implementations differ here (`preprocess` validates format and dimensions before the initialization check; `common` and `enhance_advanced` check initialization first) **and both are conforming.**

즉 **강제되는 것은 규칙 1(널 required 포인터 우선)뿐**이고, `NOT_INITIALIZED` 와 내용 검증의 선후는 계약이 아니다. 그래서 이 프로브는 규칙 1만 단언한다. 두 상태에서 각각 호출하는 것이 바로 규칙 1의 검사다 — "답이 초기화 상태에 의존하지 않아야 한다".

`NOT_INITIALIZED` 가 내용 검증보다 먼저 나오는지를 단언했다면 **정본에 없는 계약을 발명하는 것**이고, api-spec 이 `preprocess` 를 "반대 순서지만 적합" 의 예시로 명시했으므로 그 단언은 틀린다. 카드 문구가 아니라 정본을 따랐다.

## 2. 반환값 표 (실측, `a14-tables.log`)

`XPE_ERR_INVALID_INPUT = -1`, `XPE_ERR_NOT_INITIALIZED = -6`.

### common — 10행

| entry point | NULL 인자 | 미초기화 | 초기화 |
|---|---|---|---|
| `xpe_configure` | jsonConfig | -1 | -1 |
| `xpe_get_param_range` | bodyPart | -1 | -1 |
| `xpe_get_param_range` | paramName | -1 | -1 |
| `xpe_get_param_range` | minValue | -1 | -1 |
| `xpe_get_pending_alert` | message | -1 | -1 |
| `xpe_get_pending_alert` | severity | -1 | -1 |
| `xpe_alloc_image` | out | -1 | -1 |
| `xpe_free_image` | buffer | -1 | -1 |
| `xpe_copy_image` | source | -1 | -1 |
| `xpe_copy_image` | target | -1 | -1 |

### preprocess — 28행

| entry point | NULL 인자 | 미초기화 | 초기화 |
|---|---|---|---|
| `xpe_calib_load_offset` | filepath | -1 | -1 |
| `xpe_calib_load_gain` | filepath | -1 | -1 |
| `xpe_calib_load_defect_map` | filepath | -1 | -1 |
| `xpe_offset_correct` | input | -1 | -1 |
| `xpe_offset_correct` | output | -1 | -1 |
| `xpe_gain_correct` | input | -1 | -1 |
| `xpe_gain_correct` | output | -1 | -1 |
| `xpe_defect_correct` | input | -1 | -1 |
| `xpe_defect_correct` | output | -1 | -1 |
| `xpe_calib_generate_offset` | dark_frames | -1 | -1 |
| `xpe_calib_generate_offset` | output_path | -1 | -1 |
| `xpe_calib_check_expiry` | filepath | -1 | -1 |
| `xpe_calib_check_expiry` | is_expired | -1 | -1 |
| `xpe_calib_check_expiry` | remaining_days | -1 | -1 |
| `xpe_calib_save` | filepath | -1 | -1 |
| `xpe_calib_save` | calib_type | -1 | -1 |
| `xpe_validate_readout_artifact` | image | -1 | -1 |
| `xpe_validate_readout_artifact` | has_dropped_columns | -1 | -1 |
| `xpe_preprocess_pipeline` | img | -1 | -1 |
| `xpe_preprocess_pipeline` | meta | -1 | -1 |
| `xpe_calib_load_offset_cached` | filePath | -1 | -1 |
| `xpe_calib_load_offset_cached` | offsetMapOut | -1 | -1 |
| `xpe_calib_load_gain_cached` | gainMapOut | -1 | -1 |
| `xpe_calib_load_defect_cached` | defectMapOut | -1 | -1 |
| `xpe_preprocess_get_param_range` | param_name | -1 | -1 |
| `xpe_preprocess_get_param_range` | min_value | -1 | -1 |
| `xpe_ghost_correct` | handle | -1 | -1 |
| `xpe_ghost_reset` | handle | -1 | -1 |

`xpe_ghost_correct` / `xpe_ghost_reset` 의 널 핸들이 -1 인 것은 api-spec:1493 의 핸들 규칙("NULL 핸들은 널 required 포인터")과 일치한다.

### 프로브에서 제외한 export — 사유

| export | 제외 사유 |
|---|---|
| `xpe_init(configJsonOrNull)`, `xpe_preprocess_init(config)`, `xpe_log_set_file(filePath)` | NULL 이 "기본값 사용" / "stderr 로 복귀" 라는 **정의된 값**이다. 정본 규칙 1이 optional 포인터를 명시적으로 제외한다 (api-spec:1489, `xpe_logging.cpp:74`) |
| `xpe_version`, `xpe_error_string` | `XpeErrorCode` 를 반환하지 않아 관측할 값이 없다 |
| `xpe_alert_push` | void 반환 |
| `xpe_shutdown`, `xpe_clear_alerts`, `xpe_log_flush`, `xpe_get_pending_alert_count`, `xpe_log_set_level` | 포인터 인자가 없다 |

common 16개 export 중 위 제외분을 빼면 프로브 대상은 6개 함수이고, 인자별로 나누어 10행이 된다.

## 3. 증거 (Evidence)

### 프로브 실행 (`a14-tables.log`, exit=0)

```
[ RUN      ] CommonErrorPrecedenceTest.NullRequiredPointerWinsOverInitializationState
...
(XPE_ERR_INVALID_INPUT = -1, XPE_ERR_NOT_INITIALIZED = -6)
[       OK ] CommonErrorPrecedenceTest.NullRequiredPointerWinsOverInitializationState (0 ms)
...
[ RUN      ] PreprocessErrorPrecedenceTest.NullRequiredPointerWinsOverInitializationState
...
[       OK ] PreprocessErrorPrecedenceTest.NullRequiredPointerWinsOverInitializationState (0 ms)
```

표 전문은 같은 로그에 있다. 두 케이스 모두 `EXPECT_EQ` 이므로 위반이 있었다면 모든 행을 측정한 뒤 실패했을 것이다 — 첫 위반에서 멈추지 않는다.

### 재실측

```
100% tests passed, 0 tests failed out of 499     (a14-pre.log,    exit=0)
100% tests passed, 0 tests failed out of 74      (a14-common.log, exit=0)
```

## 4. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 497/497 PASS (QA-A-13, `../QA-A-13/a13-pre.log`) | **499/499 PASS** (`a14-pre.log`) | +2 = 프로브 2케이스 |
| ci-common | 73/73 PASS (QA-A-13, `../QA-A-13/a13-common.log`) | **74/74 PASS** (`a14-common.log`) | +1 = common 프로브 (preprocess 프로브는 이 프리셋에 없음) |

두 수치 모두 `git merge origin/main`(dd9cc90 반영) 이후 이 워크트리에서 측정했다.

## 5. 미검증 (Gaps)

- **규칙 2·3 은 검증하지 않았다.** 정본이 구현정의라고 명시했으므로 검증 대상이 아니지만, 그 결과 "`NOT_INITIALIZED` 가 언제 나오는가" 는 이 카드가 아무 말도 하지 않는다. 카드 배경이 인용한 `xpe_common.cpp:174→180, 230→235` 의 순서도 확인하지 않았다 — 확인해도 계약이 아니라 관측일 뿐이다.
- **널이 아닌 인자의 범위 검사는 다루지 않았다.** 규칙 1은 널 포인터만 다루고, 0 크기·범위 밖 스칼라는 규칙 2(구현정의)에 속한다.
- **preprocess 진입점 전수는 아니다.** 45개 export 중 널 required 포인터를 가진 주요 진입점 28행을 골랐다. `xpe_verify_*`, `xpe_bpm_generate`, `xpe_calib_generate_gain*`, `xpe_temp_compensate`, `xpe_nonlinearity_correct`, `xpe_binning_correct`, `xpe_calib_state_load`, `xpe_preprocess_pipeline_ex/_batch`, `xpe_defect_detect_runtime`, `xpe_calib_get_*` 는 프로브하지 않았다. 표본이지 전수가 아니다.
- **카드 배경의 `xpe_preprocess.cpp:181→185`** 는 leader 정정대로 죽은 파일이라 손대지 않았다. 다만 이 프로브는 공개 export 를 호출하므로 어떤 TU 가 진입점을 정의하는지와 무관하게 살아 있는 구현을 측정한다 — 줄 번호를 다시 잡을 필요 자체가 없었다.
- **동시 호출은 다루지 않았다.** 프로브는 단일 스레드에서 `shutdown → 호출 → init → 호출` 순으로 돈다.

## 6. 잔여 위험 (Residual risk)

- **프로브가 모듈 전역 상태를 흔든다.** 각 행마다 `shutdown` / `init` 을 반복하므로, 같은 실행 안의 다른 스위트와 순서 의존이 생길 수 있다. 이번 실행에서는 관측되지 않았고 두 프로브 모두 `TearDown` 에서 shutdown 하지만, gtest 의 실행 순서가 바뀌면 드러날 수 있는 종류의 결합이다.
- **표를 stdout 에 찍는 방식은 ctest 기본 출력에서 보이지 않는다.** `--output-on-failure` 는 실패 시에만 보여주므로, 값이 바뀌지 않는 한 표는 로그에 남지 않는다. 표가 필요하면 이번처럼 바이너리를 직접 돌려야 한다(`a14-tables.bat`).
- **전수가 아니라는 점이 회귀 방지의 한계다.** 프로브하지 않은 17개 export 중 하나가 규칙 1을 어기게 되어도 이 케이스는 침묵한다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| common 프로브 10행 + 반환값 표 | QA-A-14 |
| preprocess 프로브 28행 + 반환값 표 | QA-A-14 |
| 위반 0 → `ErrorPrecedenceTest` 회귀 케이스로 유지 | QA-A-14 |
| ci-preprocess / ci-common 재실측 | QA-A-14 |
| 프로브 미포함 17개 export 전수화 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a14-tables.log` | 두 프로브 직접 실행 — 38행 반환값 표 전문 (exit=0) |
| `a14-pre.log` | ci-preprocess 499/499 PASS (exit=0) |
| `a14-common.log` | ci-common 74/74 PASS (exit=0) |
| `a14-diffstat.txt` | `git diff --stat` (등록 2줄; 신규 파일 2개는 untracked 상태로 집계되지 않음) |
