# QA-B-195b — 워커의 메모리 부족이 -2 응답 대신 워커를 죽인다 (Codex #88 높음 1건)

카드: QA-B-195b · 관련: #130 · 근거: Codex #88 · 195 병합 조건

## 주장 (Claim)

1. **결함을 재현했다**(수정 전의 동작을 만들어서). 모델을 읽는 버퍼의 할당이 실패하는 지점(`ReadFileBounded` 직전)에서 `bad_alloc` 이 나고 **두 방어를 모두 끄면**(`original_defect_out.txt`): 워커의 응답 코드는 `-2` 가 아니라 `-3`, 워커 PID 는 0(죽음), 같은 워커로의 다음 요청도 `-3`, 감독자의 시작 횟수는 2(재시작됨). 호스트는 -2 가 아니라 연결 실패를 받는다.
2. **두 층으로 막았고 각 층을 따로 시험한다**(Codex 가 요구한 대로):
   - **생성기 안**: `OnnxSession::Create` 가 `CreateUnguarded` 를 try 로 감싸 `bad_alloc` 을 `kOutOfMemory` 로 바꾼다. 검증할 파일 읽기(모델·사이드카·서명), `TrustedModelKeys()`, 사이드카 파싱, 세션 생성 전부가 범위 안이다. 예외는 `Create` 밖으로 나가지 않는다. 메시지는 작은 문자열 버퍼에 들어가는 `"out of memory"` 라서 설정할 때 할당하지 않는다.
   - **워커 요청 경계**(`WorkerServer::Run`): 메시지 하나를 처리하는 구간 전체(페이로드 할당 + `HandleMessage`)를 감쌌다. `bad_alloc` → `XPE_ERR_OUT_OF_MEMORY`(-2) ERROR 프레임, 그 밖의 모든 예외 → `XPE_ERR_PROCESSING_FAILED`(-3) ERROR 프레임, 그리고 루프는 계속된다. 답을 만들거나 보낼 수 없으면(`SendErrorSafe` 가 false) 루프를 끝내서 호스트가 예산 끝까지 기다리지 않고 바로 알게 한다. **페이로드 버퍼 할당 자체가 실패하면** 파이프에는 그 페이로드의 바이트가 남아 있으므로 읽어서 버린다(`DiscardPayload`, 파이프가 메시지 모드라 `ERROR_MORE_DATA` 를 이어서 읽음). 안 그러면 다음 프레임을 이 프레임의 중간부터 해석한다.
3. **프로세스 안 경로도 같은 지점에서 -2**: 뼈 억제·부위 인식 각각, 모델·사이드카·서명 세 번의 읽기 중 어느 것이 실패해도 `XPE_ERR_OUT_OF_MEMORY`, 알림 없음, 출력 미기록, 다음 호출 정상.
4. **실패 계수는 194b 표의 규칙 그대로**: 워커의 메모리 부족은 워커 경로의 실패로 **센다**(1회), 다음 성공이 0 으로 되돌린다(C ABI 시험으로 확인). 이번 변경은 그것이 이제 **응답**이어서 워커가 계속 서비스한다는 점만 바꾼다.
5. **다른 예외 종류가 워커 경계를 넘는 경로**(리더 질문 한 줄): 수정 전에는 핸들러 안에서 나는 모든 예외(`bad_alloc`, `std::length_error`, 표준 예외 일반)가 미처리로 워커를 죽였다. 수정 후에는 요청 처리 구간의 모든 C++ 예외가 ERROR 프레임이 된다. **남은 경로**: ① 구조적 예외(접근 위반 등 SEH)와 `std::terminate` 로 이어지는 `noexcept` 소멸자 — 부위 인식 워커 경로의 `labels` 읽기는 여전히 JSON 문서(DOM)이고, 비어 있지 않은 문서의 소멸자 할당 실패는 경계가 잡을 수 없다(`QA-B-194 M5` 에서 문서화한 한계, 사이드카 쪽은 `QA-B-197 M1` 에서 SAX 로 없앴다). ② 요청 루프 바깥(`main` 의 시작 단계)의 예외.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 504 tests.`(197 M1 이후 496 → +8: `ModelReadOom` 2, `WorkerBoundary` 6), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests`: `[  PASSED  ] 12 tests.`(10 → +2). 스텁: `xpe_ai_tests` 400 통과(스킵 109), `xpe_ai_oom_tests` 6 통과 + 건너뜀 6, 경고 0. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py` 0 findings.
- 시험 전용 훅(`XPE_AI_TEST_HOOKS` 빌드에만): `OnnxSession::TestSetBeforeFileReadHook`(파일 버퍼를 할당하기 직전에 부른다. `std::bad_alloc` 을 던지는 훅이 그 지점의 메모리 부족). 워커는 테스트 빌드에서만 환경변수를 읽는다: `XPE_AI_TEST_FAIL_MODEL_READ=<n>`(처음 n 번의 파일 읽기 실패), `XPE_AI_TEST_FAIL_WORKER_REQUEST=oom|std|payload`(요청 하나가 경계에서 실패: 핸들러 안의 `bad_alloc` / 다른 예외 / 페이로드 할당). 출하 빌드에는 없다(`#ifdef`, 이전 카드들과 같은 방식).
- 새 시험(`test_ai_worker_boundary.cpp` 8건, `test_ai_oom_injection.cpp` 2건):
  - `ModelReadOom.ABadAllocAtAnyOfTheThreeFileReadsIsOutOfMemoryAndNeverAnException`: 세 읽기를 차례로 실패시켜도 `Create` 는 예외 없이 `kOutOfMemory`, 이어서 정상(대조군 `ControlTheModelLoadsWhenNoReadFails`)
  - `WorkerBoundary.*`(진짜 워커, 감독자 수준): 모델 읽기 실패 / 핸들러 안 `bad_alloc` / 다른 예외 / 페이로드 할당 실패 각각에 대해 뼈 억제와 부위 인식 모두 — 응답 코드(-2, -2, -3, -2), 출력 미기록, 부위 인식에서 `LastModelUnavailable()` 거짓("모델 사용 불가"가 아님), **워커 PID 가 실패 전후 같고 시작 횟수가 1**, **같은 워커로의 다음 요청 성공**(뼈 억제 ×2, 부위 인식 `CHEST`); 대조군 `ControlWithNoFaultTheWorkerAnswersBothRequestTypes`; C ABI 로 실패 수 1 → 성공 뒤 0(`xpe_ai_worker_state`, 워커 `ACTIVE`)
  - `AiOom.AModelBufferThatCannotBeAllocatedIsOutOfMemoryOnTheInProcessBonePath` / `...BodyPartPath`: 세 읽기 각각에서 -2, 알림 0, 출력 미기록, 다음 호출 정상(뼈 억제는 픽셀별로 입력의 2배)
- 반증(`arms_out.txt`), 매번 빌드 성공, 두 소스 바이트 동일 복원, 대조군 8/8: **K1** `Create` 가 `bad_alloc` 을 매핑하지 않음 → `ModelReadOom` 시험만 빨강(워커 시험은 경계가 받아서 초록 — 각 층이 따로 시험됨) / **K2** 경계가 `bad_alloc` 을 그것으로 못 잡음 → 핸들러 안 `bad_alloc` 시험만 빨강 / **K3** 다른 예외용 `catch` 없음 → 다른 예외 시험만 빨강(워커가 죽음) / **K4** 남은 페이로드를 안 버림 → 페이로드 시험만 빨강(다음 프레임이 어긋남) / **K5** 페이로드 할당을 안 감쌈 → 페이로드 시험만 빨강. 그리고 K1+K2+K3 를 **동시에** 끄면 원래 결함이 그대로 재현된다(§주장 1, `original_defect_out.txt`).
- 중간에 만난 시험 쪽 오류 두 가지: 페이로드 실패 훅이 처음에는 모든 메시지를 대상으로 해서 **워커 시작 때의 하트비트에 소모**됐고(워커가 하트비트에 오류로 답해 감독자가 재시작) 요청 메시지 종류(뼈 억제·부위 인식)에만 걸리게 고쳤다. 프로세스 안 뼈 억제 시험은 기대값을 "모든 픽셀이 2"로 잘못 써서(입력은 `1.0 + i`) 입력의 2배로 고쳤다.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(QA-B-197 M1 `31357681` 위), `cmake --build --preset ci-ai`·`ci-post`, 이 실행의 출력.

## 미검증 (Gaps)

- 실제 메모리 부족(운영체제가 할당을 거부)은 일으키지 않았다. 훅이 `bad_alloc` 을 그 지점에서 던지는 방식이다. 큰 파일(수백 MiB)로 실제 할당 실패를 재현하지는 않았다.
- `std::terminate` 로 이어지는 경로(§주장 5 ①)와 SEH, 요청 루프 바깥의 예외는 시험하지 않았다.
- 경계의 `catch (...)` 는 -3 으로 답한다. 어떤 예외인지는 답에 싣지 않는다(문자열을 만들면 그것도 던질 수 있다). 원인은 워커 로그에도 남기지 않는다.
- 워커가 ERROR 프레임을 보낸 뒤 호스트가 그 워커를 계속 쓰는 것은 기존 규칙(ERROR 프레임은 워커를 유지)에 따른 것이고, 메모리 부족이 지속되는 상황에서 연속 3회 실패로 꺼지는 것도 기존 정책이다.
- `release` 프리셋 전체 빌드는 하지 않았다(훅과 환경변수 읽기가 `#ifdef XPE_AI_TEST_HOOKS` 안에 있다는 것은 코드로만 확인).

## 잔여 위험

- 이 층들은 C++ 예외만 잡는다. 메모리 압박에서 더 흔한 `std::terminate`(noexcept 안의 할당 실패)가 워커의 남은 DOM 경로(부위 인식 `labels`)에 있다.
- `Create` 의 래퍼는 `CreateUnguarded` 의 호출자 한 곳뿐이라는 가정에 기댄다(비공개 정적 멤버).
