# QA-B-191 M4c — `use_worker` 분기, 연속 실패 정책 S+, 알림 문구, 헤더 계약

카드: QA-B-191 M4c · 관련: #130, REQ-AI-003, REQ-AI-092, REQ-AI-012 · 설계: `m4_design.md`(리더 승인 D7~D11)

**시험에 쓴 모델은 장난감 모델이다(분류기가 아니다). 이 보고서의 어떤 수치도 실제 모델의 성능이나 적재 시간을 말하지 않는다.**

## 주장 (Claim)

1. `"use_worker": true` 이면 `xpe_bodypart_recognize` 가 모델을 워커 프로세스에서 돌린다. 문턱·`fallback_mode`·저신뢰 이벤트는 이쪽 프로세스가 결정한다(프로세스 안 경로와 같은 함수 `decideBodyPart`).
2. 연속 실패 카운트는 뼈 억제와 **하나를 공유**한다(D7 S+). 예외 하나: "부위 모델을 쓸 수 없음" 응답은 실패로 세지 않고 카운트를 0 으로 되돌린다. 모델은 있는데 실행이 실패하면 센다. 뼈 억제 쪽 규칙은 바꾸지 않았다.
3. 시간 예산(092): 응답이 없는 워커는 예산에서 포기하고 `UNKNOWN`, 워커를 끝내고 다음 호출은 새 워커로 회복한다.
4. 알림 문구(D10)는 승인된 §4.4 문구 그대로다.
5. 공개 헤더 `ai_api.h` 의 계약 문구를 갱신했다.

## 증거 (Evidence)

- ci-ai(풀): `[  PASSED  ] 392 tests.` 스킵 5, 실패 0, 컴파일 경고 0 (M4b 시점 378 통과: 새 시험 +14)
- 스텁: `[  PASSED  ] 316 tests.` 스킵 81, 실패 0, 경고 0 (M4b 시점 316 통과·스킵 67: 새 시험 14건은 풀 전용이라 스킵)
- 텍스트 린트 0 error. `tools/docs/check_header_docs.py` "20 headers, 0 findings", `check_spec_test_refs.py` "no stale citations", `check_req_citations.py` "no new orphans"
- 반증 7개(`m4c_arms_out.txt`), 매번 빌드 성공(build_ok=True) 뒤 시험이 빨개졌고 `ai.cpp` 는 바이트 동일하게 복원, 마지막 대조군 14/14 통과:

| 반증 | 빨강이 된 시험 |
|---|---|
| X1 "사용 불가" 면제 제거(전부 센다) | 3개: 사용 불가는 한 번의 경고, 뼈 억제가 건너뛰어지지 않음, 사용 불가 응답이 실패 연속을 끊음 |
| X2 사용 불가 응답이 카운트를 0 으로 되돌리지 않음 | `AnUnavailableAnswerEndsARunOfBoneSuppressionFailures` |
| X3 비유한 출력을 실패로 셈 | `TheModelsRefusedOutputIsAnAlertAndNeverAWorkerFailure` |
| X4 워커 경로가 문턱 결정을 건너뜀 | 2개(저신뢰 결정, fallback_mode 끔·런타임 전환) |
| X5 중단 알림 문구 한 단어 변경 | `AModelThatExistsAndFailsToRunIsCounted...` |
| X6 형식 검사를 워커 호출 뒤로 | `ANonFloatImageIsRefusedBeforeTheWorkerIsAsked...` |
| X7 중단된 워커를 다시 시도 | 2개(실패 3회 뒤 중단, 뼈 억제 실패가 부위 인식도 중단) |

- 설계 §4.2 시나리오 표와의 대응(시험으로 옮겼다): ① 워커 정지 → 예산 포기·새 워커 회복(`ASilentWorker...`), ②·③ 부위 모델 없음이 뼈 억제를 끌어내리지 않음(`WithNoBodyPartModel...`), ④ 두 함수가 한 상태를 본다(`BoneSuppressionFailuresSwitchOff...`, `AModelThatExists...`), ⑤ 복구 `shutdown`→`init`(`ShutdownThenInitRecovers...`).
- "모델은 있는데 실행이 실패" 만들기: `models_bodypart_runfail` — 같은 장난감 그래프 끝에 `Gather(A, IDX=[5])`(길이 1 축에 인덱스 5)를 붙였다. 로드·입력 모양·라벨 수 검사는 통과하고 ONNX Runtime 이 실행에서 거부한다(통제 시험이 프로세스 안 경로에서 "사용 불가" 경고 없이 UNKNOWN 인 것으로 확인). 기존 모델 파일은 생성 스크립트를 다시 돌려도 바이트 동일(새 디렉터리 하나만 추가됨).
- 두 경로 일치 매트릭스(M4b 시험)에 이 모델을 넣었다: 두 경로가 "error" 로 일치하고, 매트릭스가 오류 종류까지 덮는다는 통제(`error > 0`)를 추가했다.

## 기준 (Baseline)

같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. M4b 시점 수치는 `m4b_report.md`.

## 변경 요약

- `ai.cpp`: `bodyPartViaWorker`(새), `decideBodyPart`(프로세스 안 경로의 문턱 꼬리를 함수로 추출, 두 경로가 공유), `pushBodyPartNotProbabilityAlert`(범위 밖 알림 한 곳). 프로세스 안 경로의 동작은 바뀌지 않았다(기존 시험 전부 통과).
- `ai_api.h`: `use_worker` 가 두 함수를 덮는다는 것, `xpe_bodypart_recognize` 의 "WORKER PATH" 절(예산·카운트·면제·알림 문구·첫 호출 시간), 라벨 문자 제한, `xpe_ai_worker_state` 가 공유 상태라는 것.
- `make_bodypart_models.py`, `models_bodypart_runfail/`: 위 모델.

## 미검증 (Gaps)

- **doxygen 은 이 PC 에 없어서 돌리지 못했다**(CI 의 `doxygen-headers` 잡이 헤더 변경을 판정한다). python 점검 3개만 통과했다. 헤더 주석에 새로 쓴 `{n}`·`{k}`·`{reason}` 자리표시자는 기존 `{c}`·`{t}` 와 같은 모양이다.
- **중단 알림 하나가 두 함수를 다 말하지 못한다.** 승인된 D10 문구는 "body-part recognition returns UNKNOWN" 만 말한다. 공유 카운트라 부위 인식 실패로 중단되면 뼈 억제도 꺼지는데(시험이 확인) 그 알림에는 뼈 억제가 나오지 않고, 뼈 억제 쪽 알림은 "input images are returned unchanged" 만 말한다. 문구를 승인된 그대로 두었으니 고칠지는 리더의 결정이다.
- 실제 모델의 적재·추론 시간, 5 s 예산 안에 드는지는 모른다(저장소에 모델 없음).
- 워커 첫 호출의 느린 꼬리(약 650 ms)의 원인은 시작·연결 단계라는 것까지만 분리했다(`m4b_report.md`).
- 동시 호출(스레드 둘이 부위 인식과 뼈 억제를 동시에)은 새로 시험하지 않았다. 둘 다 같은 모듈 잠금 안에서 워커를 쓰므로 직렬화된다는 것은 코드 읽기이고 이 마일스톤의 시험이 아니다.
- 프로세스 안 경로와 워커 경로의 알림 순서·개수는 일치 매트릭스와 이 파일의 시험이 각각 덮지만, 한 영상에 대해 두 경로를 한 시험에서 나란히 비교하는 알림 비교는 하지 않았다.

## 잔여 위험

- 첫 호출이 느린 꼬리를 만나면(약 10회 중 1회) 5 s 예산 안이지만 PRD 의 300 ms 와는 별개로 사용자가 체감한다. 워커를 `xpe_ai_init` 에서 미리 띄우는 것은 이 카드의 범위가 아니다.
- 라벨 문자 제한(따옴표·역슬래시·제어 문자)은 M2 에서 허용되던 입력을 사용 불가로 만든다. 헤더에 적었다.
