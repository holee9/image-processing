# QA-B-177b — Codex #24 (A) 보류 2건: 다른 spdlog 진입점, null 기본 로거

Refs #130 (위험 배경 #233)

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `ai_onnx_session.cpp` 의 `LOG_*` 는 이제 예외를 삼키고 null 로거를 견디는 `LogText` 진입점을 거친다. 완성된 문자열을 printf 서식으로 다시 해석하지 않는다. |
| C2 | `ai_log.h` 의 두 진입점(`LogPrintf`, `LogText`)은 기본 로거 포인터를 한 번 읽고 null 이면 반환한다. |
| C3 | modules/ai 의 spdlog 직접 호출은 C ABI 경로에 남아 있지 않다. |

## 2. 변경

- `ai_log.h`: `LogText(level, const std::string&) noexcept` 추가, 두 진입점 모두 `logger == nullptr` 검사 후 `logger->log(level, "{}", text)` (포인터를 한 번만 읽음). `AI_LOG_TEXT(level, msg)` 매크로는 **인자 표현식(`std::string("…") + e.what()` 같은 연결)까지** try/catch 로 감싼 한 문장. 호출 계약(기본 로거 교체와 로그 호출의 동시 실행은 spdlog 가 금지, 여기서 고치지 않음)을 주석에 기록.
- `ai_onnx_session.cpp`: `LOG_INFO/WARN/ERROR` → `AI_LOG_TEXT`. 호출부 10곳 무변경.
- `ai.cpp`: 시험 훅의 `spdlog::default_logger()` null 방어 1줄(시험 전용 코드).
- 시험 전용 이음새 `XPE_AI_LOG_TEST_SCOPE` (CMake 가 `xpe_ai_tests` exe 에만 정의, DLL 에는 없음): 로그 호출이 진행 중일 때만 할당 실패를 주입할 수 있게 한다. exe 가 `ai_onnx_session.cpp` 를 직접 컴파일하므로 `XPE_AI_USE_SPDLOG` 도 exe 에 정의해 그 로그가 살아 있게 했다.

## 3. 전수 (두 축)

이름 축: `LOG_(INFO|WARN|ERROR|DEBUG|TRACE)` 를 포함하는 modules/ai 의 파일.
호출 축: `spdlog::` 를 포함하는 modules/ai 비-시험 소스의 비주석 줄.

| 파일 | 이름 축 | 호출 축 | 분류 |
|------|---------|---------|------|
| `src/ai.cpp` | 있음 (`AI_LOG_*` 12곳) | `spdlog::default_logger`, `callback_sink_mt` (4줄, 시험 훅) | 로그는 `AI_LOG_*` 경유. spdlog 직접 사용은 시험 훅의 싱크 관리이며 로그 호출이 아님 |
| `src/ai_log.h` | 있음 | 진입점 구현 | 래퍼 자체 |
| `src/ai_onnx_session.cpp` | 있음 (`LOG_*` 10곳) | 변경 전 직접 3줄 → 변경 후 0줄 | 이번에 래퍼로 이동 |
| `src/ai_worker_main.cpp` | 없음 | 없음 | `std::cerr/cout` 스트림 (별도 프로세스, C ABI 아님) |
| `src/*` 그 외 | 없음 | 없음 | — |

두 축이 같은 집합이다: 변경 후 `spdlog::` 를 로그 호출로 쓰는 소스는 `ai_log.h` 뿐이고 이름 축의 소스는 그 매크로만 쓴다. 한계: 이는 grep 범위(modules/ai 소스·헤더) 결론이며, 매크로를 거치지 않는 간접 호출은 못 본다.

## 4. 시험과 반증

신규 시험 5개 (`test_ai_log_macros_spdlog.cpp`), 전체 필터 27개 통과.

| 시험 | 무엇을 잡나 |
|------|-------------|
| `AllEntryPointsTolerateANullDefaultLogger` | `set_default_logger(nullptr)` 후 `AI_LOG_TRACE…ERROR`, `AI_LOG_TEXT`, `LogText` 가 아무 일 없이 돌아오고, 로거를 되돌리면 정상 기록 (대조: 로거가 실제로 null 임을 단언) |
| `LogTextKeepsPercentAndBracesAsData` | `%s`, `%d`, `{x}`, `{}` 가 든 완성 문자열이 그대로 기록됨 |
| `NoAllocationFailureEscapesALogTextCall` | 인자 연결 포함 로그 호출 안의 할당을 1, 2, 3… 번째까지 하나씩 실패시키는 훑기, 예외 탈출 0 (대조 2개) |
| `ModelNotFoundReportSurvivesAnyAllocationFailureInItsLogCall` | 실제 `OnnxSession::Create`(없는 모델) 의 로그 호출 안 할당 실패를 훑기: 탈출 0, 반환 코드 `kInvalidModelPath`, 원래 메시지 동일, 이후 호출 정상 (대조 2개) |
| `ModelNotFoundWithNoDefaultLoggerStillReturnsTheOriginalError` | 기본 로거가 null 인 채로 실제 `Create` 가 원래 오류를 반환 |

| 반증 | 조작 | 결과 | 파일 |
|------|------|------|------|
| A | `ai_onnx_session.cpp` 의 `LOG_*` 를 직접 `spdlog::*` 로 복원 (이음새는 유지) | null 로거 시험 빨강 | `arm_A_run.txt` |
| C | `AI_LOG_TEXT` 의 try/catch 제거 | `NoAllocationFailureEscapesALogTextCall` 빨강 (`threw`) | `arm_C_run.txt` |
| N | 두 진입점의 null 검사 제거 | `AllEntryPointsTolerateANullDefaultLogger` 에서 SEH `0xc0000005` | `arm_N_run.txt` |

N 은 "죽는 시험" 이다: gtest 가 Windows 에서 SEH 를 시험 본문 실패로 잡으므로 별도 프로세스 없이 빨강으로 관측된다.

## 5. 정정과 발견 (측정이 말한 것)

1. **A1 의 "로그 예외가 C ABI 밖으로 나간다" 는 이 경로에서 할당 실패만으로는 재현되지 않았다.** 반증 A 를 처음 `ModelNotFound…SweepSurvives` 시험에 돌렸더니 **통과**했다 (`BUILD=0`, `RUN=0`). spdlog 의 `logger::log` 가 자체 try/catch 로 내부 실패를 삼키고, `LOG_ERROR(result.message)` 의 인자는 할당하지 않기 때문이다. 이 훑기는 래퍼 이전 코드와 구별하지 못한다. 그래서 (a) 인자 연결을 포함하는 매크로 훑기(반증 C 가 잡음)와 (b) null 로거 시험(반증 A 가 잡음)을 추가했다. 실제로 막은 위험은 **catch 본문 안의 인자 연결(`std::string(...) + e.what()`)이 던지는 경우**와 **null 로거**이다.
2. 시험 설계 오류: 첫 빌드가 `OnnxErrorCode::kSuccess` (없는 이름) 로 실패했다. 열거형 이름은 `kOk`. 그 실행 결과(22개 통과)는 낡은 바이너리였으므로 버리고 `BUILD=0` 인 실행만 읽었다.
3. 카드 요구 "`use_worker=false` + 없는 모델 경로로 `xpe_bone_suppress` 를 부르며 훑기" 는 **DLL 단에서는 하지 않았다.** DLL 은 자체 할당기를 가져 exe 의 `operator new` 교체로 주입할 수 없다. 대신 같은 소스(`ai_onnx_session.cpp`)를 exe 가 직접 컴파일한 것으로 `OnnxSession::Create` 를 훑었다. `xpe_bone_suppress` → `Create` 의 호출 자체와 반환 코드(-9)는 QA-B-177 의 `InProcessErrorLogNamesThePath` 가 DLL 단에서 확인한다.
4. 앞 보고서의 "모든 할당 지점" 표현을 정정했다(QA-B-177 report.md 7절, 시험 주석): 범위는 "이 시험 exe 의 `operator new` 를 거쳐 동기 싱크에서 관측된 할당 지점".

## 6. 전체 결과

| 구성 | 결과 |
|------|------|
| ci-post | `100% tests passed, 0 tests failed out of 1009` (이전 1004 + 신규 5) |
| ci-ai | `100% tests passed, 0 tests failed out of 363` (이전 358 + 신규 5) |
| 경고 | 두 빌드 로그 `warning C` 0건 |

## 7. Gaps

- 시험 전용 이음새와 `XPE_AI_USE_SPDLOG` 가 exe 에만 있으므로, DLL 안의 로그 호출 OOM 은 주입하지 못했다 (DLL 의 `AI_LOG_*` 는 같은 `ai_log.h` 코드이지만 이 코드 경로를 DLL 에서 직접 실행해 보지는 않았다).
- 파일 싱크 등 실제 운영 싱크의 할당과 `err_handler_` 경로는 시험하지 않았다.
- 기본 로거 교체와 로그 호출의 동시 실행은 시험하지 않았다 (spdlog 가 금지하는 사용이며 호출 계약으로 주석에 기록).
- **정정 (QA-B-177c, Codex #26)**: 이 항목은 틀렸다. 비 spdlog 분기에는 `AI_LOG_TEXT` 정의가 **없었다**(커밋 `bcbeb523` 의 `ai_log.h`). 호출부 `ai_onnx_session.cpp` 가 그 분기에서 자체 no-op `LOG_*` 를 써서 컴파일이 깨지지 않았을 뿐이다. 원인: 정의를 넣으려던 치환 스크립트가 따옴표 heredoc 안의 `\n` 때문에 문자열이 맞지 않아 **조용히 아무것도 바꾸지 못했고**, 그 뒤 코드를 다시 읽지 않고 보고서에 "`((void)0)` 이다" 라고 적었다. QA-B-177c 에서 `#else` 분기에 인자를 평가하지 않는 `do {} while (0)` 를 추가하고, 그 분기를 컴파일하는 시험을 뒀다. (적기 전에 grep 으로 확인했으면 잡혔다.)

## 8. 참고 (Codex 지적, 수정 대상 아님)

- MSVC `_Printf_format_string_` 은 일반 컴파일에서 불일치를 잡지 않는다 (QA-B-177 에서 `%u→%s` 변형이 경고 없이 통과함을 측정, 헤더 주석에 기록).
- OOM 시험은 `operator new` 만 교체한다 (범위 표현을 4절과 정정 4번대로 적음).
