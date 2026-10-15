# QA-B-194 M4 — 초기화와 설정 강화 (REQ-AI-090, D1·D7·D8)

카드: QA-B-194 M4 · 관련: #130 · 설계 승인: `design.md` D1·D7·D8
장난감 모델·스텁 기준이다. 할당 실패 때의 롤백은 **이 마일스톤에서 증명하지 않았다**: 밖에서 일으킬 수 없는 경로라 M5 의 할당 실패 스윕(`xpe_ai_oom_tests`)이 증거다(아래 미검증).

## 주장 (Claim)

1. **D1** `xpe_ai_init("")` 는 `XPE_ERR_INVALID_INPUT`. 이전에는 받아들여져 모델이 작업 디렉터리 기준으로 찾아졌다(워커의 "DLL 옆에서만" 원칙과 반대). 판정은 "이미 초기화됨 → 무시" 분기보다 **앞**이다: 인자가 틀린 것은 모듈 상태와 무관하다. 거부된 호출은 아무것도 초기화하지 않고 살아 있는 세션도 건드리지 않으며 알림도 없다(NULL 디렉터리와 같은 조용한 INVALID_INPUT).
2. **D7** 쓸 수 없는 설정은 **반환 코드를 OK 로 둔 채 Warning 알림**을 낸다(#145 이 정한 선).
   - 깨진 JSON(`{bad`, 빈 문자열): "ai config is not valid JSON and was ignored: every setting uses its default"
   - 객체가 아닌 유효 JSON(`[]`, `5`, `"x"`, `null`, `true`): "ai config is valid JSON but not an object and was ignored: …"
   - `timeout_ms` 정수가 [0, 2147483647] 밖: "ai config key 'timeout_ms' is out of range (0 to 2147483647 ms) and was ignored: the default is used (value: <값>)". 이전에는 `get<int>()` 후 `uint32_t` 변환이라 **-1 이 4294967295 ms(약 49일) 마감**이 되고 int 범위를 넘는 값은 조용히 잘렸다. 0 은 "기본값" 이므로 받아들인다(`timeoutMs != 0 ? … : DEFAULT` 가 쓰는 곳 모두에 있다).
   - 정수가 아닌 값(`1.5`)은 이전의 #145 "unexpected type" 알림 그대로이고 **그것 하나뿐**이다(범위 알림이 겹치지 않는다).
   - `confidence_threshold` 는 범위를 두지 않는다: `ai_api.h` 가 "range check 없음" 으로 문서화한 의도다(설계 §6).
3. **D8** 이미 초기화된 뒤의 둘째 `xpe_ai_init` 은 **OK·무시 그대로**(GUI 시험이 "무시"를 계약으로 가짐), 다만 모델 디렉터리나 설정 **텍스트가 첫 호출과 바이트 단위로 다르면 Warning** 한 건: "…called again with a different model directory or config while the module is already initialised: the call was ignored and the first settings stay in effect (call xpe_ai_shutdown first to change them)". 같은 호출을 되풀이하면 조용하다. NULL 설정과 빈 설정 텍스트는 같은 요청(둘 다 "기본값")으로 본다 — 의미가 같은 JSON 이라도 공백이 다르면 "다름" 으로 경고한다(바이트 비교, 일부러 단순하게).
4. **예외·롤백** `xpe_ai_init` 의 본문을 `extern "C++"` 도우미(`xpe_ai_init_impl`)로 옮기고 내보낸 함수는 try/catch 만 둔다(`bad_alloc` → `XPE_ERR_OUT_OF_MEMORY`, 그 밖 → `XPE_ERR_PROCESSING_FAILED`). 상태는 **마지막 한 걸음 전까지 `unique_ptr`** 이 갖고, `g_aiState` 는 `release()` 로 마지막에 한 번 쓴다. 이전에는 `parseConfig`·문자열 복사에서 예외가 나면 상태가 새고 예외가 C ABI 를 넘었다.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 431 tests.` 스킵 5, 실패 0, 컴파일 경고 0 (M3 425 → +6). 스텁: `[  PASSED  ] 353 tests.` 스킵 83, 실패 0, 경고 0 (M3 347 → +6). 기존 시험은 한 건도 고치지 않았다(config 경고 시험 포함).
- 로컬 doxygen 1.12.0(CI 순서, `WARN_AS_ERROR`): `exit=0`, `: (warning|error)` 줄 0. `check_header_docs.py` 0 findings. 린트 0 error.
- 새 시험 6건(`test_ai_input_validation.cpp`; 모듈 생명주기를 직접 다루므로 `AiInitHardening` 은 픽스처 없이 매번 shutdown 으로 시작하고 끝난다):
  - `AnEmptyModelDirectoryIsAMissingArgumentAndLeavesTheModuleUntouched`: `""` → `INVALID_INPUT`, 초기화되지 않음, 알림 0, NULL 대조군 그대로, 실제 디렉터리는 초기화(대조군)
  - `AnEmptyModelDirectoryIsRefusedWhetherOrNotTheModuleIsAlreadyInitialised`: 살아 있는 모듈에 `""` → `INVALID_INPUT`, 세션 그대로
  - `AConfigThatCannotBeUsedSaysSoAndTheCallStillSucceeds`: 8개 설정 × (OK, 초기화됨, 알림 정확히 1건, Warning, 문구) — 깨짐·빈 문자열·`[]`·`[1,2]`·`5`·`"x"`·`null`·`true`
  - `ATimeoutOutsideZeroToTwoToTheThirtyOneIsIgnoredWithAnAlert`: -1, -2147483648, 2147483648, 10000000000, 18446744073709551615(int64 위, unsigned 분기) 각각 OK + 알림 1건 + 값이 문구에 찍힘
  - `TheTimeoutBoundsThemselvesAreAcceptedSilently`: 0, 1, 5000, 2147483647 은 조용, `1.5` 는 옛 "unexpected type" 알림 하나뿐(대조군)
  - `ASecondInitIsIgnoredAndSaysSoOnlyWhenItAskedForSomethingDifferent`: 동일 → 조용·OK, 설정 다름 → OK + 경고 1건 + **첫 설정이 유지됨**(`use_worker` 가 켜진 채: 워커 상태 ACTIVE), 디렉터리 다름 → 경고, NULL 로 시작해 `""` 로 되풀이 → 조용
- 반증 12개(`m4_arms_out.txt`), 매번 빌드 성공, `ai.cpp` 바이트 동일 복원, 대조군 112/112:
  - P1 빈 디렉터리 허용 → 빈 디렉터리 시험 둘 빨강. P2 빈 디렉터리를 새 init 에서만 판정 → "이미 초기화" 시험만 빨강
  - P3 깨진 JSON 알림 제거 / P4 객체 아님 알림 제거 → 설정 시험만 빨강
  - P5 음수 timeout 범위 검사 제거 / P6 unsigned timeout 범위 검사 제거 → 범위 시험만 빨강
  - P7 상한을 하나 줄임(2147483647 거부) / P8 0 거부 → 경계 시험만 빨강
  - P9 둘째 init 이 항상 침묵 / Q1 항상 경고 / Q2 디렉터리만 비교 / Q3 설정만 비교 → 둘째 init 시험만 빨강
- **반증 P7·P8 은 처음에 동치 변이였다**: nlohmann 은 음수가 아닌 정수 리터럴을 전부 *unsigned* 로 파싱하므로 부호 있는 분기에는 음수만 들어온다. 그곳의 경계(`<= kMax`, `>= 0`)를 바꾼 반증은 시험이 아무것도 못 잡았다(첫 실행 `m4_arms_out_first_run.txt`: red 0). 시험의 빈틈이 아니라 반증 설계의 실수였고, 실제 경계가 있는 unsigned 분기(`u <= kMax`)로 다시 짜서 경계 시험만 빨강이 되는 것을 확인했다. 부호 있는 분기에서 `value <= kMaxTimeoutMs` 쪽 절반은 도달하지 않는 방어이고, 범위 검사를 한 줄에 두는 편이 읽기 쉬워 지우지 않았다.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(M3 b009db58 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 바뀌는 반환 코드와 호출자

- `xpe_ai_init`: `""` 가 `XPE_OK` → `XPE_ERR_INVALID_INPUT`. GUI 는 항상 절대 경로를 넘기고(`NormalizeDirectory` 가 공백을 기본 디렉터리로 바꾼다) 빈 문자열을 넘기는 호출은 찾지 못했다(`modules`·`clients`·`gui` 에서 리터럴 `xpe_ai_init("")` 를 검색한 결과 0건 — 변수로 넘기는 호출까지는 확인하지 못했고, GUI 쪽은 `NormalizeDirectory` 를 읽어서 본 것이다).
- 경고 알림이 늘었다: 깨진/객체 아닌 설정, 범위 밖 `timeout_ms`, 설정이 다른 둘째 init. 반환 코드는 그대로다. 알림 개수를 세는 클라이언트 시험이 있는지는 `clients/` 에서 확인하지 않았다(설계 §7 의 미검증 항목).
- `timeout_ms` 가 -1 이거나 2^31 이상이면 이전에는 그 값(의 잘린 값)이 마감이 되었고, 이제는 기본값이 쓰인다.

## 미검증 (Gaps)

- **롤백 증명 없음**: `xpe_ai_init` 의 try/catch·롤백은 코드로 있고 읽어서 확인했지만 실행으로는 증명하지 못했다. 할당 실패를 일으키는 방법은 M5 의 `operator new` 교체 스윕이다. 그때까지 "할당 실패에서 상태가 새지 않고 `g_aiState` 가 변하지 않는다" 는 문장은 주장이 아니라 설계 의도다.
- `timeout_ms` 의 **효과**(실제 마감 값)는 관찰하지 않았다. 시험이 보는 것은 알림뿐이다(마감 값을 내보내는 API 가 없다). 알림은 분기에 묶여 있어 분기를 되돌리면 시험이 빨개지는 것은 확인했다(P5·P6).
- 반환 코드를 바꾸지 않았으므로 "깨진 설정은 오류여야 한다" 는 논의는 #145 의 결정을 따른 것이다.
- nlohmann 이 연결되지 않은 빌드의 최소 파서는 바꾸지 않았다(알림 없음 그대로). 현재 두 CI 구성 모두 nlohmann 을 쓴다.
- 둘째 init 의 "다름" 은 바이트 비교다. 의미가 같은 JSON(공백만 다름)도 경고한다.
- 둘째 init 의 비교는 `modelDirPath`·`configJson` 을 잠금 없이 읽는다(`xpe_ai_shutdown` 과의 동시 호출은 문서화된 LIFECYCLE CONTRACT 가 이미 범위 밖으로 둔 것이고, 잠금을 잡으면 무응답 워커가 잠금을 쥔 시간만큼 init 이 멈춘다).

## 잔여 위험

- 새 경고가 클라이언트의 알림 표시 줄에 더 나올 수 있다(둘째 init 은 GUI 의 `NeedsNewSession → Shutdown` 경로를 거치므로 정상 흐름에서는 나오지 않는다고 읽었으나 실행해 보지는 않았다).
- `""` 를 넘기던 외부 호출자가 있다면 이제 `INVALID_INPUT` 을 받는다.
