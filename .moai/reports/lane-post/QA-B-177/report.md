# QA-B-177 — AI_LOG_* printf 서식이 spdlog 경로에서 풀리지 않던 진단 결함

Refs #130

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `ai.cpp` 의 `AI_LOG_*` 호출 12곳 중 9곳이 printf 서식이었고, spdlog 경로에서 `%s/%d/%u/%zu` 가 그대로 로그에 찍혔다. 워커 실패 코드와 연속 실패 횟수도 로그에 남지 않았다. |
| C2 | 매크로를 printf-먼저 방식으로 바꾸자 호출 문자열은 한 글자도 바꾸지 않고 값이 찍힌다. |
| C3 | 로그 싱크로 잡은 실제 텍스트에서 `%` 변환 지정자가 사라졌고, 매크로를 되돌리면 시험이 빨개진다. |

## 2. 증거

### 2.1 결함 (수정 전, ci-post)

`before_fix_ci_post_run.txt` — 12개 실패, 2개 통과. DLL 이 실제로 낸 줄:

```
got: xpe_ai initialized: model_dir=%s, ep=%d, timeout=%u ms
got: xpe_ai shutdown: worker_pid=%u
lines: { "bone_suppress: worker path failed (%d), input returned unchanged (%u of %u consecutive failures)" }
```

### 2.2 수정

- `modules/ai/src/ai_log.h` (신규, 매크로를 ai.cpp 에서 옮김): spdlog 분기는 `vsnprintf` 로 필요한 크기만큼 먼저 서식을 풀고 결과를 `spdlog::log(level, "{}", text)` 의 **인자**로 넘긴다. 고정 버퍼 없음, 비활성 레벨은 서식 비용 없음.
- 비 spdlog 분기: 두 문장짜리 매크로를 `do { … } while (0)` 한 문장으로.
- 호출부 12곳은 손대지 않았다.
- 시험 전용 `xpe_ai_test_set_log_capture` (DLL 의 기본 로거에 callback sink 연결): `XPE_AI_TEST_HOOKS` 와 spdlog 가 있을 때만 컴파일되고 내보내진다. DLL 이 자기 spdlog 사본을 갖기 때문에 시험 프로세스에서는 DLL 의 로거에 닿을 수 없어 필요했다.

### 2.3 수정 후

| 구성 | 명령 | 결과 |
|------|------|------|
| ci-post | `xpe_ai_tests --gtest_filter=AiLogFixture.*:AiLogMacros*.*` | 16 passed (`after_fix_ci_post_run.txt`) |
| ci-ai | 같은 필터 | 16 passed (`after_fix_ci_ai_run.txt`) |
| ci-post 전체 ctest | | 100% passed, 0 failed / 1004 (최초 커밋 시점 1003 + 아래 7절의 시험 1개) (`after_full_ci_post_ctest.txt`) |
| ci-ai 전체 ctest | | 100% passed, 0 failed / 358 (`after_full_ci_ai_ctest.txt`) |
| 경고 | 두 구성의 빌드 로그 `warning C` | 0건 |

모든 실행에서 `BUILD=0` 을 확인했다 (낡은 바이너리 아님).

### 2.4 반증

| 팔 | 조작 | 결과 | 파일 |
|----|------|------|------|
| 1 | 옛 매크로 복원, 한-문장 시험 유지 | 빌드 실패 `C2181` (if 없는 else) | `arm_old_macros_one_statement_build_error.txt` |
| 2 | 옛 매크로 복원, 그 시험 하나만 `#if 0` | 12개 실패 / 3개 통과 (DLL 시험 6개 전부, spdlog 매크로 시험 6개 전부 빨강) | `arm_old_macros_behaviour_red_run.txt` |
| 3 | 호출부에 `%u` → `%s` 불일치 주입 | 빌드 `BUILD=0`, 경고 없음 | (증거 파일 지움, 아래 3.2) |
| 4 | `"{}"` 래퍼 제거 | 13개 모두 통과 (반증 실패) | (지움, 아래 3.3) |
| 4b | 서식 결과를 `fmt::runtime(text)` 로 | 2개 실패 (`BracesInTheDataAreNotAFormat`, `ALiteralWithBraces…`) | `arm_text_read_as_runtime_format_run.txt` |

## 3. 중간에 틀렸던 것 (정정)

1. **시험 자체의 오류**: 첫 green 실행에서 spdlog 매크로 시험 7개가 실패했다. 원인은 매크로가 아니라 spdlog 가 Windows 에서 줄 끝을 CR LF 로 쓰는 것을 시험이 몰랐던 것. 실제 출력(`Which is: "info|a=5 …\r\n"`)을 읽고 시험의 줄 끝 정규화로 고쳤다. 그 뒤 팔 2 를 다시 돌려 **올바른 이유로** 빨간지 확인했다.
2. **내가 쓴 주석이 거짓이었다**: `ai_log.h` 에 "`_Printf_format_string_` 이 있으니 서식 불일치는 C4477 컴파일 경고, /WX 에서 오류" 라고 적었다. 팔 3 이 이를 반박했다 — 일반 컴파일은 검사하지 않고 `/analyze` 에서만 동작한다. 주석을 측정 결과로 고쳤다.
3. **"예전엔 중괄호 리터럴이 예외를 던졌다"는 주장도 거짓**이었다. 팔 4 가 그대로 통과해 드러났다 (spdlog 는 인자 하나짜리 호출을 그대로 기록한다). 해당 시험은 과거 결함이 아니라 **회귀 방지**라고 주석을 바꿨다. 이 시험과 `"{}"` 래퍼가 막는 것은 4b 의 변형(런타임 서식으로 읽기)뿐이다.

## 4. 호출 전수와 다른 모듈

- `modules/ai/src/ai.cpp`: `AI_LOG_*` 12곳. printf 서식 9곳(init, shutdown 의 worker_pid, 워커 실패 코드·횟수, 경로 `%s` 4곳, `%zu` 2곳, Fallback mode).
- 같은 매크로를 쓰는 다른 모듈: grep(`#define … spdlog::` / 서식 지정자를 넘기는 `spdlog::*(`) 으로 찾았을 때 없음. 목록으로 낼 것 없음.
- `ai_onnx_session.cpp` 는 미리 만든 `std::string` 을 넘기는 자체 `LOG_*` 매크로라 영향 없음. `ai_worker_main.cpp` 는 `std::cerr/cout` 직접 사용.

## 5. Gaps (미검증)

- 비 spdlog 분기는 DLL 로는 빌드되지 않는다 (프리셋 전부 spdlog 있음). 그 분기는 헤더 단위 시험(`AiLogMacrosPrintf.*`)으로만 확인했다.
- `/analyze` (Code Analysis) 는 돌리지 않았다. 서식 불일치는 지금 시험이 읽는 줄에 한해서만 잡힌다.
- 다른 모듈에 서식 불일치가 없다는 것은 grep 범위 안의 결론이다 (매크로를 거치지 않는 간접 호출은 못 본다).
- ONNX 실경로 로그(`ai_onnx_session.cpp`)는 이번 카드 범위 밖이고 stub 구성에서는 실행되지 않는다.
- 공개 헤더는 바꾸지 않아 Doxygen 은 돌리지 않았다.

## 6. 잔여 위험

- 새 호출부가 서식 지정자와 인자 개수를 틀리게 쓰면 컴파일러는 못 잡는다. `NoPrintfPlaceholderSurvivesAnyLoggedPath` 가 그 경로를 지나갈 때만 잡는다.
- 로그 캡처 훅은 `XPE_AI_TEST_HOOKS` ON 일 때만 내보내진다. 납품 구성(OFF)에는 없음을 이번 카드에서 `dumpbin` 으로 다시 확인하지는 않았다 (QA-B-173 에서 확인한 메커니즘과 같은 옵션을 재사용).

## 7. 추가 (리더 요청): 로그는 절대 던지지 않는다

`AI_LOG_*` 는 DLL 의 `extern "C"` 수출 함수 안에서 불린다. 서식 문자열과 spdlog 는 할당하므로 메모리 부족 시 `std::bad_alloc` 이 C ABI 밖으로 나가고, `/EHsc` 에서는 수출 함수 지역(lock_guard 등)이 풀리지 않는다(#233, pre 레인 실측).

- `LogPrintf` 를 `noexcept` 로 하고 본문 전체를 `try { … } catch (...) {}` 로 감쌌다. noexcept 는 catch 가 있어야만 안전하다(던지면 terminate). 핸들러는 비어 있어 스스로 할당하지 않는다. 이유는 `ai_log.h` 주석에 있다.
- 시험 `NoAllocationFailureEscapesALogCall`: 전역 `operator new` 를 시험 exe 안에서 교체해(평소엔 malloc 그대로, 시험이 호출 하나를 감쌀 때만 무장) **로그 호출 안의 1, 2, 3… 번째 할당을 하나씩 실패**시키며, 호출이 할당을 덜 할 때까지 훑는다. 모든 할당 지점이 대상이다. 대조: 실제로 주입이 일어났는지, 훑기가 끝까지 도달했는지, 실패 없는 호출이 뒤에 정상 기록되는지.
- 반증(`arm_log_without_swallow_run.txt`, BUILD=0): noexcept 와 catch 를 제거하면 `an allocation failure at allocation #1 escaped the log call` 로 빨강.
- 시험 설계 중 틀린 것: 같은 캡처로 "실패 후 정상 기록"을 확인하려다 실패했다. 주입한 실패가 시험용 ostream 싱크를 망가뜨린 것이었고(제품이 아님), 새 캡처로 확인하도록 고쳤다.

Gaps: 할당 실패만 주입했다(다른 예외 원천은 vsnprintf 가 던지지 않으므로 해당 없음으로 판단하나 측정은 안 함). 전역 operator new 교체가 같은 exe 의 다른 시험에 영향을 줄 수 있어 무장 시에만 실패하도록 했고, 두 구성 전체 통과로 확인.
