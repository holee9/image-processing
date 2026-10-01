# QA-A-182 (`#230`) — 소스 파일 자신의 옛 REQ 인용 정정: `readout_validate`·`temp_compensate`·`pipeline`

주석만 바꿨습니다 (세 소스 파일 15곳). `QA-A-181` 과 같은 방식으로 요구 본문과 구현을 읽고 대조했고, 맞는 번호가 없는 곳은 지어내지 않고 "요구가 말하지 않음" 으로 적었습니다. 마지막에 `#230` 의 번호 정정을 닫기 위한 **잔여 점검**(§4)을 붙였습니다.

**조건**: 시작 전 로컬 `main`(`11324d02`, 리더가 `QA-A-181` 을 병합한 것)을 `dev/preprocess` 에 fast-forward 병합(이쪽 0 커밋 앞섬·1 커밋 뒤처짐). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 빌드 `build/ci-preprocess` RelWithDebInfo. 인용한 이슈 `#232` 는 `gh issue view 232` 로 확인했습니다(`OPEN`, 제목 "preprocess 요구 본문과 구현 불일치 2건 — REQ-P1A-041 선 잡음 미구현, REQ-P1A-021 오류 코드").

---

## 1. 바꾼 곳

### `readout_validate.cpp` — `REQ-P1A-041` (일부만 구현, `#232`)

| 줄 | 이전 | 이후 |
|---|---|---|
| `:4` | `REQ-P1A-001 to REQ-P1A-004` | `REQ-P1A-041 (partly implemented: no line-noise check, see #232)` |
| `:16` `@MX:SPEC` | `REQ-P1A-001` | `REQ-P1A-041` |
| `:32` | `// REQ-P1A-002: detect dropped columns (…)` | `// REQ-P1A-041: detect dropped columns (…)` |
| `:42` | `// REQ-P1A-003: detect gain nonuniformity (rows where mean > 0.9 * UINT16_MAX)` | 네 줄: *"REQ-P1A-041 names line noise, dropped columns and ADC saturation patterns. This check flags rows whose mean exceeds 0.9 * UINT16_MAX and reports it as has_nonuniform_gain. It is not a line-noise check (kLineNoiseFrac is a misnomer), and no line-noise check exists in this function (#232)."* |
| `:56` | `// REQ-P1A-004: always XPE_OK` | 두 줄: *"Always XPE_OK once the arguments are valid. No current requirement states this; the READOUT_VALIDATED flag (REQ-P1A-096) is set by the pipeline."* |

**근거**

- `REQ-P1A-041` 본문(`spec.md:858`): *"validate it for **line noise, dropped columns, and ADC saturation patterns** via `xpe_validate_readout_artifact()`"*. 구현은 (1) 모든 화소가 0 인 열, (2) 행 평균 `> 0.9 × 65535` 인 행만 검사합니다. 선 잡음 검출은 없습니다(`QA-A-181` §4 a, `#232`).
- **`:42` 를 "ADC 포화 패턴 검사" 라고 단정하지 않았습니다.** 밝은 행 검사가 본문의 셋 중 무엇에 해당하는지는 본문이 말하지 않고, 구현은 이를 `has_nonuniform_gain` 으로 보고합니다. 그래서 *"본문이 셋을 든다 / 이 검사는 이것을 한다 / 선 잡음은 아니다"* 만 적었습니다.
- **`:56` 은 옛 번호의 의미를 확인하고 썼습니다.** `git show ee2c607:.moai/specs/SPEC-XPE-P1A/spec.md` 의 옛 `001`~`004` 는 `001` 읽기 검증, `002` **아티팩트 점수와 메시지 출력**, `003` **점수 > 80 이면 경고 알림**, `004` **`XPE_FLAG_READOUT_VALIDATED` 설정**입니다(`:58-64`). 즉 옛 주석의 "`002` = 빠진 열, `003` = 이득 비균일, `004` = 항상 `XPE_OK`" 는 **옛 SPEC 본문과도 맞지 않았습니다.** 점수·메시지·알림은 이 함수에 없고, 플래그는 `pipeline.cpp:122`(`meta->flags |= XPE_FLAG_READOUT_VALIDATED`)가 설정합니다(`REQ-P1A-096`). 그래서 "항상 `XPE_OK`" 를 근거로 삼을 요구가 현재 없다고 적었습니다.

### `temp_compensate.cpp` — `REQ-P1A-080`·`081`

| 줄 | 이전 | 이후 |
|---|---|---|
| `:5` | `REQ-P1A-080 to REQ-P1A-082` | `REQ-P1A-080, REQ-P1A-081 (the flag, REQ-P1A-082, is set by the pipeline)` |
| `:23` `@MX:SPEC` | `REQ-P1A-005` | `REQ-P1A-080, REQ-P1A-081` |
| `:51` | `// REQ-P1A-005: apply correction — …` | `// REQ-P1A-080: apply correction — …` |

근거는 `QA-A-181` §2 와 같습니다: `080` 본문(UINT16·제자리·`exp` 비·`65535` 클램프)과 `081` 본문(NaN → 25 °C, `[-20, +60]` 가드)이 구현과 일치하고, `082` 의 플래그는 메타데이터를 받지 않는 이 함수가 아니라 파이프라인이 겁니다. 옛 `005` 는 현재 *Input Validation*(`spec.md:129`)입니다. `:17`·`:32`·`:35` 의 `REQ-P1A-081` 인용은 이미 맞아 그대로 둡니다.

### `pipeline.cpp` — `REQ-P1A-095` ~ `101`

| 줄 | 이전 | 이후 |
|---|---|---|
| `:4` | `REQ-P1A-095 to REQ-P1A-099` | `REQ-P1A-095 to REQ-P1A-101` |
| `:24` | `REQ-P1A-095 to REQ-P1A-099` | `REQ-P1A-095 to REQ-P1A-101` |
| `:325` `@MX:SPEC` | `REQ-P1A-095 to REQ-P1A-099` | `REQ-P1A-095 to REQ-P1A-101` |

근거: `100`(데이터 영역 전이)·`101`(결함 단계 가용성)을 `QA-A-181` §2 에서 본문과 `pipeline.cpp:199`, `:248-253` 으로 대조했습니다.

---

## 2. 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | `HEAD` 와 작업본을 **주석 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_comment_only_check.py`) | 세 파일 모두 **IDENTICAL** |
| 대조군 | 같은 도구에 코드 토큰을 바꾼 사본 (`has_nonuniform_gain`, `kTempFallback`, `xpe_preprocess_pipeline` — 각각 해당 파일의 **코드**에 나오는 이름) | 세 파일 모두 **바뀜을 감지함** |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개**, 서수·이름 동일, RVA·`@ILT` 열까지 **줄 전체 동일** (`DIFF_EXIT=0`) |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | `BUILD_EXIT=0`, `error C`·`warning C` 0건, **세 파일이 실제로 재컴파일됨**(`Building CXX` 3건) |
| 시험 (전체) | `xpe_preprocess_tests.exe` 필터 없이 (`evidence/06`) | `721 tests from 94 test suites ran`, **`713 PASSED`**, 8 `SKIPPED`, **실패 0**, `TESTS_EXIT=0` — `QA-A-181` 의 실행과 같은 건수 |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

### 비교 스크립트를 한 번 고쳤습니다 — 기록

첫 시도에서 `readout_validate.cpp` 가 **`DIFFERS`** 로 나왔고 대조군은 **감지하지 못했습니다**(`False`). 원인은 둘이었고 둘 다 **코드 변경이 아니라 비교 방식의 문제**였습니다:

1. 한 줄 주석을 네 줄로 늘리면 주석 제거 뒤 남는 **빈 줄 수**가 달라져 비교가 어긋납니다(이전 카드들은 줄 수가 같게 바꿔서 드러나지 않았음). 스크립트가 빈 줄과 줄 끝 공백을 정규화하도록 고쳤습니다.
2. 대조군이 바꾼 식별자(`xpe_validate_readout_artifact`)가 그 파일에서 **처음 나오는 곳이 주석**(`@MX:ANCHOR` 줄)이라, 바뀐 것이 주석 안이어서 감지할 수 없었습니다 — **판별력이 없는 대조군**이었습니다. 이제 스크립트는 대조 토큰이 **코드에 실제로 나오는지**를 먼저 단언하고(`control token occurs in code: True`), 파일마다 코드 토큰을 따로 골라 세 파일 모두에서 감지를 보였습니다.

정규화가 느슨해진 만큼(빈 줄·공백 무시) 의미 없는 서식 변경은 통과시킵니다. 코드 의미를 바꾸는 변경은 세 대조군이 잡았고 내보내기·시험·빌드가 따로 확인합니다.

---

## 3. 이 카드에서 건드리지 않은 것 — 목록만

- `pipeline.cpp:3`, `:23`: `(stages 0.5-4)` — 단계는 `0.5`~`7`(`:114`~`:276`)이라 **낡은 서술**이지만 요구 번호가 아니어서 그대로 둡니다.
- `readout_validate.cpp` 의 상수 이름 `kLineNoiseFrac`·플래그 이름 `has_nonuniform_gain` 은 코드라 바꾸지 않았고, 주석이 어긋남을 사실대로 적었습니다(`#232`).

---

## 4. `#230` 번호 정정을 닫기 전 — 잔여 점검 (목록만, 미수정)

`modules/preprocess` 전체(`src`·`include`·`tests`·`tools`·`CMakeLists.txt`·`.md`)에서 인용된 `REQ-P1A-NNN` 53개 중 **현재 `spec.md` 에 `#### REQ-P1A-…` 제목이 없는 번호**를 뽑았습니다(`evidence/10_closing_sweep_undefined_req_numbers.txt`, 스크립트 `10b_req_orphan_sweep.py`, 범위 표기 `..` 은 풀어서 셈). 정의된 제목은 43개입니다.

| 정의 없는 번호 | 인용 위치 | 비고 |
|---|---|---|
| `006` `007` `008` `023` | `tests/test_golden_reference.cpp:12` (`REQ-P1A-005..008, REQ-P1A-020..023, REQ-P1A-001..004`; 앞줄 `:11` 에도 `016..019` 등 옛 목록) | 옛 번호 목록. `005`·`001`~`004`·`020`~`022` 는 정의돼 있으나 **의미가 다른 번호**라 이 도구로는 잡히지 않음 |
| `043`~`047` | `include/xpe/preprocess_api.h:1067`, `src/xpe_verify_metrics.cpp:685` (`SRS-CALIB-FUNC-015 / -021 / REQ-P1A-041..047: Pipeline verification`) | `041`·`042` 는 정의돼 있지만 읽기 검증·파라미터 질의 — "pipeline verification" 과 의미가 다름 |
| `066` | `CMakeLists.txt:433`, `tests/test_req_p1a_066.cpp` (`:3`, `:35`, `:68`, `:95`, `:137`) | **`spec.md` 에 `REQ-P1A-066` 이 한 번도 나오지 않음**(`grep -c` 0). 시험 이름과 VVP(`REQ-P1A-066 | test_req_p1a_066.cpp`)가 인용하는 번호가 SPEC 에 없음 |

**한계**: 이 점검은 **정의 없는 번호**만 잡습니다. 정의는 있지만 의미가 어긋난 인용(예: `test_golden_reference.cpp` 의 `001..004`, 이번 카드가 고친 종류)은 제목 목록만으로는 못 잡고, 본문 대조가 필요합니다. 역사 서술("old REQ-P1A-013 …")은 줄 단위로 걸러 보려 했으나(`history narration`) 위 표의 줄은 모두 서술이 아닌 일반 인용으로 분류됐습니다.

---

## 5. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, push 없음, 새 이슈 없음
- §3·§4 의 목록, `docs/`·SPEC·VVP·생성된 Doxygen 수정 없음

## 6. 미검증

- `readout_validate.cpp:42` 의 밝은 행 검사가 `REQ-P1A-041` 본문의 어느 항목(예: ADC 포화 패턴)에 해당하는지는 판단하지 않았습니다(§1).
- `SKIPPED` 8건을 변경 전 바이너리의 건수와 직접 비교하지는 않았습니다. `QA-A-181` 실행과 같은 건수이고 내보내기가 줄 전체 동일하다는 것이 간접 근거입니다.
- 잔여 점검(§4)은 `modules/preprocess` 안만 봤습니다. `clients/`·`docs/`·`.moai/specs/` 의 `REQ-P1A-…` 인용은 보지 않았습니다.

🗿 MoAI
