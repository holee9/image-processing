# QA-B-193b — ERROR 프레임의 binary 플래그를 장애로

카드: QA-B-193b · 관련: #130 · 근거: Codex #79 보류 1건(보통)

## 주장 (Claim)

`XPE_AI_MSG_ERROR` 는 JSON 만 있는 프레임이다. 두 요청(부위 인식·뼈 억제)이 같이 거치는 ERROR 해석 입구 `ParseWorkerErrorFrame` 이 이제 헤더를 받아 `XPE_AI_FLAG_HAS_BINARY_PAYLOAD`(0x1) 가 붙은 ERROR 를 JSON 을 보기도 전에 거절한다. 거절하면 연결을 끊고 실패로 센다. 예약 비트 0x2·0x4·0x8 은 거절하지 않는다(192 결정).

## 증거 (Evidence)

- 입구가 하나다: 검사는 `ParseWorkerErrorFrame` 안에 있고 두 ERROR 분기가 같은 함수를 `rh`(헤더)와 함께 부른다. 그래서 한 요청이 검사를 건너뛰는 일이 구조상 없다(반증 H3 이 그 전제를 시험한다).
- 시험 `WorkerErrorFrameFlags.*` 3건(가짜 워커 `bodypart_error_raw`·`bone_error_raw` 가 `XPE_FAKE_WORKER_FLAGS` 로 헤더 플래그를 보낸다):
  - `AnErrorFrameIsJsonOnlySoTheBinaryBitIsAFaultForBothRequestTypes`: 대조군 — 부위 인식의 `{"error_code":-9,"model_unavailable":true,...}` 는 `flags=0` 이면 인정되고(IO_FAILED, `unavailable`, 워커 유지), **같은 JSON 에 0x1 만 붙이면** 장애(IO_FAILED, `unavailable` 아님, 워커 폐기). 뼈 억제도 `flags=0` 이면 워커의 코드(-9) 그대로·워커 유지, 0x1 이면 `PROCESSING_FAILED`·워커 폐기
  - `TheBinaryBitCombinedWithReservedBitsIsStillAFault`: 0x1|0x2, 0x1|0x4, 0x1|0x8, 0x1|0x2|0x4|0x8 → 모두 장애(두 요청)
  - `TheReservedBitsAreNotRefusedOnAnErrorFrame`: 0x2, 0x4, 0x8, 세 비트 모두 → 두 요청 모두 평소처럼 해석, 워커 유지
- 실패 카운트에 드는 것: 부위 인식에서 장애는 `rc != OK` 이고 `LastModelUnavailable()==false` 라 `bodyPartViaWorker` 가 실패로 센다(M4f 의 같은 근거). Codex 의 시나리오(0x1 이 붙은 정상 JSON `model_unavailable:true` 가 카운트를 0 으로)는 위 첫 시험의 두 번째 호출이 그대로 재현한다: 이제 `unavailable` 이 거짓이다.
- ci-ai: `[  PASSED  ] 403 tests.` 스킵 5, 실패 0, 경고 0 (193 시점 400: +3). 스텁: `[  PASSED  ] 325 tests.` 스킵 83, 실패 0, 경고 0 (193 시점 322·스킵 83: 가짜 워커 시험 +3). 린트 0 error, `check_header_docs.py` 0 findings.
- 반증 3개(`arms_out.txt`), 매번 빌드 성공, 파일 복원 바이트 동일, 마지막 대조군 44/44:
  - H1 검사를 뺌 → 0x1 시험 2개만 빨강(카드가 예측한 대로 — 다른 어느 시험도 빨개지지 않음)
  - H2 예약 비트까지 거절 → 예약 비트 시험만 빨강(192 호환 결정이 고정됨)
  - H3 뼈 억제에서만 검사 → 0x1 시험 2개 빨강(부위 인식 쪽 누락이 잡힘)

## 기준 (Baseline)

같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. 이전 수치는 `QA-B-193/report.md`.

## 변경 요약

`ai_ipc_bridge.cpp`(`ParseWorkerErrorFrame` 이 헤더 인자를 받아 0x1 검사), `ai_worker_protocol.h`(ERROR 는 JSON 만이고 0x1 은 장애, 예약 비트는 거절하지 않는다는 문장), 가짜 워커(`XPE_FAKE_WORKER_FLAGS`), `test_worker_bodypart.cpp`(시험 3건).

## 미검증 (Gaps)

- 성공 응답 쪽의 binary 비트: 뼈 억제 응답은 0x1 이 없으면 장애(`ai_ipc_bridge.cpp` 의 기존 검사, 기존 시험 `test_ipc_deadline.cpp` 가 다룬다), 부위 인식 응답은 0x1 이 있으면 장애인 검사가 코드에 있다. **부위 인식 성공 응답에 0x1 을 붙인 전용 시험은 찾지 못했다**(가짜 워커의 `bodypart_raw` 는 flags 0 만 보낸다) — 이 카드의 범위(ERROR 프레임)가 아니라 하지 않았고, 필요하면 카드로 낼 수 있다.
- 실제 워커가 ERROR 프레임에 0x1 을 붙이는 일이 없음은 193 의 호환 시험(`WorkerBoneErrorFrame.TheRealWorkersOwnErrorFramesAreAcceptedAndKeepTheWorker`, 통과)과 `SendFrame`(`flags=0`)이 보여 준다 — 새 시험을 더하지 않았다.
- doxygen 은 이 PC 에 없다(헤더 주석을 바꿨다).

## 잔여 위험

- 다른 요청 종류의 클라이언트 함수가 생기면 같은 입구(`ParseWorkerErrorFrame`)를 부르는 것이 기본이다. 부르지 않으면 이 검사가 빠진다 — 입구가 하나라는 것은 구조상 보장이 아니라 현재 두 호출자가 그렇게 한다는 사실이다.
