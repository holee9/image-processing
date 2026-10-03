# QA-A-232 보고서 — 로그 함수 3곳 재현·수정 (M1) / preprocess_api.h metadata 문장 (M2)

카드: QA-A-232 · 추적: #253 (M2 는 #245) · 브랜치 dev/preprocess

## 0. 정정 (QA-A-231 보고서의 틀린 문장)

231 보고서는 "xpe_common 은 스스로 로그를 쓰지 않는다"고 적었다. 틀렸다.
`xpe_common.cpp` 의 `internal_log` 가 `xpe_init` 에서 `[XPE][INFO ] xpe_init: library initialised` 를
stderr(또는 자기 파일)에 쓴다. 내가 센 것은 `spdlog::info/...` 호출뿐이었고, 모듈에는 spdlog 를 거치지 않는
옛 경로 `internal_log` 가 따로 있다. 이 경로는 `xpe_log_set_level` / `xpe_log_set_file` 의 지배를 받지 않는다
(자체 `g_logLevel`, 자체 `g_logFile`). 이번 카드에서 이 두 경로를 갈라서 다뤘다. 아래 M1 의 "범위 밖"에 이 경로의 남은 문제를 적었다.

## M1. 로그 함수 3곳 — 실행으로 재현 → 헤더·요구 쪽으로 수정

### 1. 요구 문구 (인용)

| 출처 | 문구 |
|---|---|
| SRS-FUNC-040 (XPE-SRS-001) | "6단계 logging subsystem … TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, OFF=5. xpe_log_set_level 함수로 레벨 조정" |
| SRS-FUNC-041 | "로그 출력은 stderr (기본) 또는 file path로 redirect 가능 … NULL 입력 시 stderr로 복귀" |
| 헤더 xpe_log_set_level | "0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF. Messages below this threshold are silently discarded." |
| 헤더 xpe_log_set_file | "Pass NULL to revert to stderr." |
| REQ-P0-011 (SPEC-XPE-P0) | "set default logging to stderr at INFO level" |
| api-spec.md SRS-LOG-001, sprint-plan.md | 5=OFF |

레벨 5 의 이름은 한 곳만 어긋난다. SPEC-XPE-P0 REQ-P0-023 만 "CRITICAL=5" 이고,
SRS-FUNC-040·헤더·api-spec·sprint-plan·기존 시험 주석(`// OFF`)은 모두 OFF 다. 다수결이 아니라
공개 계약(헤더)과 상위 문서(SRS)가 OFF 라서 OFF 를 목표로 삼았다. REQ-P0-023 의 문구 정정은 리더 몫이다(아래 §4).

### 2. 재현 (고치기 전, 실패하는 시험 먼저)

새 시험 `modules/common/tests/test_common_logging_contract.cpp` (실행 파일 `xpe_common_logging_tests`).
fd 2 를 임시 파일로 돌려 stderr 에 실제로 닿은 것을 읽는다. 대조군 `CaptureSeesALineWrittenToStderr`
(stderr 에 쓴 줄을 캡처가 읽는다)가 통과한 상태에서 판정한다.
증거: `evidence/10_red_logging_contract.txt` — 10개 중 8개 빨강, 2개 통과(대조군, 그리고 아래 주의).

| 후보 | 관측 (고치기 전) | 확정 |
|---|---|---|
| (a) `xpe_log_set_file(NULL)` | 파일에서 NULL 로 돌아간 뒤 `spdlog::info("MARK_AFTER_NULL")` 가 stderr 에 없음 (null sink) | 재현됨 |
| (b) `xpe_log_set_level(5)` | 5 로 설정하고 파일을 열면 파일에 `[critical] MARK_CRITICAL` 이 기록됨 | 재현됨 |
| (c) 기본 수준 | 이름 붙인 파일에 수준 선택 없이 쓰면 `[trace] MARK_TRACE`, `[debug] MARK_DEBUG` 가 기록됨 | 재현됨 |
| (c') xpe_init 직후 | `spdlog::info/warn/error/critical` 이 stderr 에 하나도 안 닿음 (spdlog 기본 로거는 stdout, 모듈은 stderr 로거를 설치하지 않음) | 재현됨 |

주의: `LevelFiveIsOffOnStderr` 는 고치기 전에도 통과했다. xpe_init 직후엔 어떤 줄도 stderr 에 안 닿아서
"없음" 단언이 비어서 통과한 것이다. 이 시험은 (c') 가 고쳐진 뒤에 의미를 얻고, 짝인 `LevelFourStillRecordsErrorAndCritical`
(레벨 4 에서 critical 이 보임)이 그 의미를 보증한다. 시험 이름만으로 "5 가 OFF 를 막는다"고 읽지 않도록 적어 둔다.

### 3. 수정

`modules/common/src/xpe_logging.cpp`, `xpe_common.cpp`, 헤더 `xpe_common_api.h` (set_level 문단).

- (a) NULL → 널 싱크 대신 stderr 로거(`xpe_stderr`)를 설치한다. 수준은 호출자가 고른 값을 이어받는다.
- (b) 레벨 5 → `spdlog::level::off`.
- (c) `g_currentLevel` 초기값 0 → 2. 새 내부 함수 `xpe_log_internal_init()` 을 `xpe_init` 이 부른다: 파일이 이미 쓰이고 있지 않으면
  수준을 INFO 로 되돌리고 stderr 로거를 기본으로 설치한다. `xpe_log_internal_reset()`(shutdown)도 수준을 INFO 로 되돌린다.
- stderr 싱크는 spdlog 의 `stderr_sink_mt` 가 아니라 C 스트림 `stderr` 를 쓸 때마다 찾아 쓰는 작은 싱크(`StderrStreamSink`)다.
  이유(실측): spdlog 의 Windows stderr 싱크는 만들 때 잡은 OS 핸들을 들고 있어서, 그 뒤에 호스트가 fd 2 를 돌리면
  `WriteFile() failed. GetLastError(): 6` 만 남기고 줄이 사라진다 (첫 시험 실행에서 관측했다. 그 출력은 다음 실행이 덮어써서 증거 파일에는 없다. 호스트가 fd 2 를 돌리는 일은
  이 시험 틀만이 아니라 테스트 하니스·서비스 래퍼도 한다). C 스트림을 거치면 리디렉션을 따라간다.
- 헤더: "5 discards every message", "INFO until a caller sets another", xpe_init 이 stderr 를 가리킨다는 문장을 더했다.

### 4. 기본 수준을 INFO 로 바꾼 교차 영향 (grep 결과)

카드가 "고치기 전에 grep, 있으면 보고"라고 했다. 찾은 것:

- 기본 수준이 TRACE 이길 바라는 시험·문서: 없음. `[trace]` / `[debug]` 문자열을 기대하는 코드는 modules·clients 어디에도 없다.
- `xpe_log_set_level` 호출처(modules 밖): C# `PInvokeWrapper.cs`, `LoggingHandlerTests.cs`, `XpeCommonNative.cs`. 전부 반환 코드만 단언한다
  (유효 0..5 → OK, -1·6 → INVALID_INPUT). 출력 내용·수준에 의존하지 않는다.
- `xpe_log_set_file` 호출처(modules 밖): 같은 C# 시험 둘. 반환 코드만 본다.
- spdlog 소비자: modules/dicom (`spdlog::debug/error`), modules/ai (`ai_log.h`). 기본 수준이 INFO 가 되면 debug 줄이 기록되지 않는다
  (dicom 의 debug 줄을 보는 시험은 grep 으로 찾지 못했다). error 줄은 이제 stderr 로 간다(전에는 spdlog 기본인 stdout).
- 한계: 이 grep 은 이름·문자열로 센 것이다. dicom·ai·gsvg·enhance 시험 실행 파일은 이 작업 트리의 ci-common/ci-preprocess 빌드에 없어서
  돌리지 못했다 — 아래 §6 의 미검증.

문서 정정 제안(리더 소유): SPEC-XPE-P0 REQ-P0-023 "CRITICAL=5" → "OFF=5"; REQ-P0-011 은 이제 구현과 일치한다
("stderr, INFO"); 231 의 `spec_xpe_p0_fix_draft.txt` §2 의 REQ-P0-011·023 줄은 이 보고서로 대체된다(코드를 요구에 맞췄으므로 문서는 안 바꿔도 된다).

### 5. 검증 (이번 트리, 이번 실행의 출력)

| 항목 | 명령 | 관측 |
|---|---|---|
| 새 시험 | `xpe_common_logging_tests` (`evidence/20_green_xpe_common_logging_tests.txt`) | PASSED 11 tests, exit 0 |
| 기존 공용 시험 | `test_xpe_common` | PASSED 69 tests, exit 0 |
| 할당 실패 훑기 | `xpe_common_oom_tests` | PASSED 12 tests, exit 0 |
| preprocess 전체 | `xpe_preprocess_tests` (`evidence/40_preprocess_tests_after.txt`, ci-preprocess 에서 xpe_common 재빌드 후) | PASSED 1002 tests, exit 0 |
| 헤더 문서 | `python tools/docs/check_header_docs.py` | `20 headers, 0 declarations skipped as unparseable, 0 findings`, exit 0 |
| doxygen 1.12.0 (WARN_AS_ERROR) | `doxygen Doxyfile` | exit 0 (`evidence/51_doxygen.txt`) |

### 6. 반증 (한 번에 하나씩 되돌리고 전체 빌드, 복원 뒤 재확인) — `evidence/30_falsification_arms.txt`

| 팔 | 되돌린 것 | 빨개진 시험 |
|---|---|---|
| A1 | NULL → 널 싱크 | NullRevertsFromAFileToStderr, NullKeepsTheChosenLevel |
| A2 | 레벨 5 → critical | LevelFiveIsOffInAFile, LevelFiveIsOffOnStderr |
| A3 | 초기 수준 TRACE | (일반 실행에선 안 터짐 — xpe_init 이 2 로 맞춘다) → `LoggingContractFresh.*` 를 단독 프로세스로 돌리면 빨강 (`evidence/31_arm_A3_fresh_process.txt`) |
| A4 | xpe_init 이 stderr 기본을 안 설치 | 4개 (AfterInit…, ASecondInit…, LevelFour…, ALevelChosen…) |
| A5 | init 이 수준을 INFO 로 안 되돌림 | ASecondInitRestoresTheStderrDefault |
| 복원 | — | 없음 (exit 0) |

A3 은 처음 계획에서 안 터졌다. 정적 초기값은 xpe_init 이후엔 덮어써져서 관측되지 않기 때문이다. 이를 중복으로 지우지 않고,
init 이전 호출을 보는 시험(`LoggingContractFresh`, 픽스처 없음)을 더했고 그 팔이 터지는 것을 단독 프로세스로 확인했다.
`gtest_discover_tests` 는 시험마다 별도 프로세스로 돌리므로 ctest 에선 항상 첫 로깅 호출이다.

### 7. Gaps (미검증)

- dicom / ai / gsvg / enhance 시험은 이 트리에서 돌리지 못했다. 기본 수준 변경이 그쪽 출력(debug 줄 소멸, error 줄이 stdout → stderr)에
  의존하는 시험이 있는지는 문자열 grep 으로만 확인했다.
- 리눅스·macOS 빌드는 보지 않았다. `StderrStreamSink` 는 표준 C 만 쓰고 시험 틀은 `unistd.h` 분기를 갖지만 컴파일해 보지 않았다.
- 레거시 `internal_log` 경로는 건드리지 않았다(아래).

### 8. Residual-risk (잔여 위험)

- `internal_log` 는 아직 spdlog 경로와 따로 논다: `xpe_log_set_level(5)` 로 OFF 를 골라도 `xpe_init` 의 `[XPE][INFO ] library initialised`
  한 줄과 경보 진단 줄이 stderr 에 나온다(자체 `g_logLevel` 이 INFO). 이번에 고친 세 후보가 아니고, 한 경로로 합치는 것은 구조 변경이라 범위 밖으로 남겼다.
  SRS-FUNC-040 의 "xpe_log_set_level 로 레벨 조정"을 엄격히 읽으면 이 줄도 대상이다. 리더 판단 필요.
- xpe_init 이 INFO 로 올리는 것은 "파일이 이미 쓰이고 있지 않을 때" 만이다. init 전에 파일을 고르고 수준을 5 로 정한 호스트의 선택은 그대로 둔다(의도).
- `xpe_log_set_level` 은 파일 로거가 없을 때 `spdlog::set_level`(레지스트리 전체)을 부른다 — 다른 모듈이 만든 로거의 수준도 같이 움직인다. 기존 동작이고 이번 카드는 바꾸지 않았다.

## M2

(M2 커밋에서 이어서 적는다.)
