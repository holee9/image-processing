# QA-B-177c — spdlog 없는 빌드의 AI_LOG_TEXT 정의 누락

Refs #130 (Codex #26 발견 1, 낮음)

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `ai_log.h` 의 `#else` (spdlog 없음) 분기에 `AI_LOG_TEXT` 정의가 없었다. 지금은 인자를 평가하지 않는 `do {} while (0)` 로 있다. |
| C2 | 그 분기를 컴파일하는 시험이 있다(번역 단위가 그 분기를 직접 컴파일한다). |
| C3 | QA-B-177b 보고서 7절의 "비 spdlog 분기의 `AI_LOG_TEXT` 는 `((void)0)` 이다" 는 틀렸고 정정했다. |

## 2. 증거

### 2.1 결함 재현 (시험을 먼저 넣고 정의는 아직 없을 때)

`before_fix_build_error.txt` — 새 시험 번역 단위를 빌드하면 `error C3861: 'AI_LOG_TEXT': 식별자를 찾을 수 없습니다` (`BUILD=1`). 즉 `ai_log.h` 를 spdlog 없이 포함해 `AI_LOG_TEXT` 를 쓰는 코드는 컴파일되지 않았다. 지금까지 깨지지 않은 이유는 유일한 호출부 `ai_onnx_session.cpp` 가 그 분기에서 자체 no-op `LOG_*` 를 쓰기 때문이다.

### 2.2 수정과 시험

- `ai_log.h`: `#else` 분기에 `#define AI_LOG_TEXT(level, msg) do {} while (0)`. 인자를 평가하지 않으므로 호출 비용이 없고, `level` 토큰(예: `spdlog::level::err`)이 존재하지 않아도 된다.
- 시험 2개 (`test_ai_log_macros.cpp`, 이 파일은 `XPE_AI_USE_SPDLOG` 를 `#undef` 한 뒤 `ai_log.h` 를 포함하므로 **그 분기를 직접 컴파일한다**. 이것이 "spdlog 없이 컴파일을 보장하는 번역 단위"이다):
  - `LogTextCompilesWithoutSpdlogAndEvaluatesNothing`: 부작용이 있는 인자를 넘겨 평가 횟수 0, 출력 없음. spdlog 를 포함하지 않았는데도 `spdlog::level::err` 를 일부러 적어, 매크로가 그것을 버리고 컴파일하지 않음을 함께 확인.
  - `LogTextIsOneStatementAfterAnUnbracedIf`: 괄호 없는 `if/else` 아래에서 한 문장.

| 실행 | 결과 |
|------|------|
| `AiLogMacros*:AiLogFixture.*` (ci-post) | 24 passed |
| ci-post 전체 | `100% tests passed, 0 tests failed out of 1014` (`after_full_ci_post_ctest.txt`) |
| ci-ai 전체 | `100% tests passed, 0 tests failed out of 368` (`after_full_ci_ai_ctest.txt`) |
| 경고 | 두 빌드 로그 `warning C` 0건 |

반증은 2.1 이 그것이다: 정의가 없으면 이 시험 번역 단위가 컴파일되지 않는다(빌드 실패로 빨강).

## 3. 정정 (QA-B-177b 보고서 7절)

해당 줄을 "정정" 표시와 함께 고쳤다. 원인은 커밋 `bcbeb523` 을 만들 때 정의를 넣는 치환 스크립트의 대상 문자열에 따옴표 heredoc 의 이스케이프가 섞여 **맞지 않았고**, 그 치환에는 "정확히 한 번 일치" 단언이 없어 **아무 오류 없이 아무것도 바꾸지 못한** 것이다. 그 뒤 코드를 다시 읽지 않고 보고서에 "`((void)0)` 이다" 라고 적었다. 적기 전에 `grep AI_LOG_TEXT` 한 번이면 잡혔다.

이번 카드의 치환은 처음부터 단언이 있었고(그래서 같은 실수를 즉시 알아챔) 실제 수정은 편집 도구로 했다.

## 4. Gaps (미검증)

- 이 시험은 비 spdlog 분기를 **헤더 단위**로 컴파일한다. DLL 자체를 spdlog 없이 빌드한 구성은 만들지 않았다(모든 프리셋에 spdlog 가 있음).
- `AI_LOG_TEXT` 의 비 spdlog 분기 정의가 다른 `AI_LOG_*` 와 같은 형태(인자 무평가·한 문장)인지는 위 두 시험으로만 확인했다.

## 5. 잔여 위험

없음(낮음 등급). 호출부가 하나뿐이고 그 호출부는 이 정의를 쓰지 않는다.
