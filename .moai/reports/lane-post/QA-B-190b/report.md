# QA-B-190b — #210 주석 정정 (modules/ai, 동작 불변)

카드: QA-B-190b · 관련: #210 · 근거: QA-B-190 보고서(`37a53a56`), 리더 승인.

## 1. 주장 (Claim)

`modules/ai` 의 주석 5곳을 현재 코드와 맞췄다. 비주석 줄은 한 줄도 바꾸지 않았다.

| 파일 | 고친 것 |
|---|---|
| `src/ai_ipc_bridge.cpp` 머리 주석 `REQ-AI-092` 블록 | "예산이 배포 빌드에서 안 돈다·bridge 호출 0곳·알림 없음" → 지금 동작: 예산은 `ai.cpp` → `WorkerSupervisor` → 이 브리지의 한 교환 예산이고, fallback(입력 반환)과 알림(Warning 1건)은 `ai.cpp` 의 워커 경로 몫이며, 이 파일에는 알림 호출이 없다. 범위: opt-in 워커 경로의 `xpe_bone_suppress` 만. 기본 경로와 스텁 세 진입점은 예산 없음. 009 의 출처도 "개명이 아니라 대량 커밋 `dd7c8e05` 에서 코드·RTM 에 함께 들어온 오번호"로 고침 |
| `src/ai_ipc_bridge.h` 머리 주석 | "Only part … implemented" 삭제, 위 블록을 가리키는 두 줄 |
| `include/xpe/ai/ai_worker_protocol.h` 머리 주석 | 같은 취지 |
| `src/ai.cpp` `xpe_dl_denoise` 주석 | `N2V` 가 든 전략 목록 → `REQ-AI-021` 의 목록(Noise2Noise, Noise2Self, Neighbor2Neighbor, Noise2Sim) |
| `tests/test_ai_fallback.cpp` 머리 주석 | `REQ-AI-012` 줄 아래에 "이 파일은 012 를 시험하지 않는다: `ConfidenceThreshold*` 는 헤더 상수만 보고 `confidence_threshold` 를 읽는 코드가 없다" |

## 2. 증거 (Evidence)

- **비주석 변경 0줄**: `git diff -U0` 에서 주석·빈 줄을 뺀 변경 줄이 0.
- **인용한 번호·함수·동작을 코드와 대조**(`claims_check.txt`, 항목마다 대조군 병기): 브리지의 `xpe_alert_push` 0건(대조군 `ai.cpp` 5건), 감독자가 `xpe_ai_ipc_bridge_create(pipe, config_.timeout_ms)` 를 호출(`ai_worker_supervisor.cpp`), `ai.cpp` 가 `timeout_ms` 를 파싱해 감독자 설정으로 넘김, `XPE_AI_DEFAULT_TIMEOUT_MS 5000`, 실패 알림 문구가 `REQ-AI-002, REQ-AI-092` 인용, `useWorker{false}`, ONNX 세션 파일의 timeout/budget/deadline 단어 0건(대조군 브리지 54), `Stub implementation` 주석 3곳(bodypart·stitch·denoise), `REQ-AI-021` 정의 줄의 전략 목록, 002·012·092 정의 줄 존재, `modules/ai` 의 009 리터럴 0 파일(대조군 092 는 10 파일), 009 의 첫 등장 `dd7c8e05`, `confidenceThreshold` 읽기 0곳, `ai.cpp` 에 `XPE_AI_USE_ONNXRUNTIME` 0건.
- 인용 검사: `python tools/docs/check_req_citations.py` → `no new orphans (1 known orphan ids, 6 citations)`.
- 빌드·시험: `build\g190b-run.bat` — ci-ai `===BUILD=0===`, 다시 컴파일된 17개 단위에서 경고 0줄(`build/g190b-build.txt`, 커밋 안 함), 같은 필터의 시험 `budget_tests_run.txt`: `[  PASSED  ] 107 tests.`(QA-B-190 과 같은 107, 스킵 1은 스텁 전용 시험).
- doxygen(헤더 주석을 고쳤으므로): `doxygen Doxyfile` → 종료 코드 0, 출력의 `warning` 0줄.
- 텍스트 린트 0 오류.

## 3. 기준 (Baseline)

같은 트리(main 병합 후 `dev/postprocess`), 같은 필터·같은 빌드 구성(ci-ai). 직전 기준은 QA-B-190 의 107개 통과.

## 4. 미검증 (Gaps)

- 107 은 QA-B-190 이 쓴 필터(`IpcDeadline.*`, `WorkerPathFixture.*`, `WorkerSupervisor.A*`, `AiFallbackTest.*`, `AiIpcBridgeTest.Receive*`)의 수다. `ci-ai` 전체 ctest 는 이번에 돌리지 않았다. 주석만 바뀌었으므로 동작 시험은 컴파일 결과가 같다는 것에 기댄다.
- "경고 0" 은 이번 빌드가 다시 컴파일한 17개 단위의 것이다. 전체 재빌드의 경고는 보지 않았다.
- 새 주석의 문장(예: 예산이 "한 교환" 단위)은 `ai_ipc_bridge.cpp` 의 기존 주석("ONE time budget … for the whole exchange")에서 옮겼다.

## 5. 잔여 위험 (Residual risk)

- 주석은 코드가 바뀌면 다시 낡는다. 이번 정정이 가리키는 두 줄(워커 경로만 예산이 있다, 알림은 `ai.cpp` 에 있다)은 #130 T-006 등이 진입점을 추가하면 바뀐다.
