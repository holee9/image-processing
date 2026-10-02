# QA-B-181i — Codex #63 보류: 모델 결과의 비유한 값을 워커 고장으로 세지 않는다 (#233)

## 요약

| 항목 | 결과 |
|------|------|
| 극단 영상 3장이면 정상 워커가 세션 내내 꺼짐 | **실행으로 재현했다**: 수정 전 3번째 극단 영상에서 `xpe_ai_worker_state` 가 `DISABLED(2)`, 실패 횟수가 1·2·3 으로 증가 (`red_before_fix.txt`). 구분해서 고쳤다: 결과가 비유한이면 출력 거부 + 알림, 실패 횟수·워커 상태는 움직이지 않는다. |
| 반환 코드 | 두 경로 모두 `XPE_ERR_PROCESSING_FAILED` (181h 의 `INVALID_INPUT` 에서 변경) |
| 시험의 허점 (카드 2) | 맞았다. 이번 시험은 **케이스마다** 알림 1건·워커 장애 알림 0건·상태 `ACTIVE`·실패 횟수 0 을 단언하고, 극단 영상 6장을 연속으로 넣은 뒤 같은 세션에서 정상 영상이 처리됨을 확인한다. |

## 정정 (QA-B-181h 보고서)

181h 보고서 §1.3~1.4 의 "worker 경로 5건 거부" 는 **수정 전 코드에 대해서는 사실**이지만(실패 횟수 상한이 없던 성공 경로라 5건 모두 브리지를 거쳤다), **수정 후 시험이 5건 모두 브리지의 검사를 거쳤다는 증거는 아니었다**. 같은 세션에서 3번째 거부로 워커가 꺼지고 4·5번째는 즉시 반환 분기를 탔는데, "비OK + 출력=입력" 단언은 그 분기도 통과했다. 181h 의 해당 표현은 이 보고서로 대체한다. 코드의 검사 자체(복사 전 비트 검사)는 181h 와 같다.

## 1. 변경

| 위치 | 변경 |
|------|------|
| `ai_ipc_bridge.cpp` / `.h` | 응답이 정상 형식인데 화소가 비유한이면 `XPE_ERR_PROCESSING_FAILED` 를 반환하고 브리지의 `last_result_nonfinite` 를 세운다 (호출마다 먼저 지움). 복사 전 검사는 181h 그대로, 연결은 유지 |
| `ai_worker_supervisor.cpp` / `.h` | `LastResultWasNonFinite()` — 직전 호출이 이 사유로 거부됐는지. 호출 시그니처는 그대로 (시험 파일 3개가 같은 시그니처를 직접 선언하거나 호출한다) |
| `ai.cpp` 워커 경로 | 거부 플래그가 서 있으면: 출력 = 입력(이 경로의 문서화된 실패 규약), 실패 횟수 **0 으로** (아래 판단 참고), 알림 1건, `PROCESSING_FAILED` 반환. "AI worker failed" 알림은 나가지 않는다 |
| `ai.cpp` 프로세스 안 | 복사 전 거부 + 같은 알림, `PROCESSING_FAILED` (181h 는 알림이 없고 `INVALID_INPUT` 이었다) |
| `ai_api.h` | `xpe_bone_suppress` 반환 문서 갱신 |

**실패 횟수를 "올리지 않는다"가 아니라 0 으로 되돌리는 이유**: 워커는 정상 응답을 했다. "연속 실패" 는 그 건강한 교환으로 끊긴다고 읽었다 (기존 `ASuccessResetsTheConsecutiveFailureCount` 와 같은 논리). 실패 2회 뒤에 극단 영상 1장이 오면 횟수가 0 으로 돌아간다는 뜻이고, 이 부분은 **별도 시험으로 묶지 않았다** (시험은 "올라가지 않음"과 "워커가 켜져 있음"만 본다). 리더가 "올리지도 지우지도 않음"을 원하면 그 한 줄만 바꾸면 된다.

**입력 크기로 사전 거부는 넣지 않았다** (카드 지시: 모델별 허용 범위를 모른다). 검사는 모델 출력만 본다.

## 2. 알림 문구 — 레인 간 계약

`AI model output was non-finite (inf/NaN) and was rejected: the image's value range may exceed what the model accepts; the AI worker itself is healthy` (Warning). 원인(모델 출력이 유한 범위를 벗어남)과 짐작되는 해결(입력 값 범위)을 말하고, 워커가 정상임을 밝힌다. `clients/` 가 문구를 정규식으로 매칭한다면 알려야 할 문자열이다. 이 카드에서 `clients/` 의 매칭 여부는 확인하지 않았다.

## 3. 시험 (`test_bone_suppress_nonfinite.cpp`, ONNX 빌드)

| 시험 | 내용 |
|------|------|
| `InProcess…` | 극단 입력 6장 연속: 매번 `PROCESSING_FAILED`, 출력 센티널 불변, 입력 불변, 알림 정확히 1건, 이어서 정상 영상 `OK` = `2X` 비트 동일 |
| `WorkerPath…` | 같은 6장 연속: 매번 `PROCESSING_FAILED`, 출력 = 입력, 알림 1건(원인) + "AI worker failed" 0건, `xpe_ai_worker_state` = `ACTIVE`·실패 0 (케이스마다), 이어서 같은 세션에서 정상 영상 `OK` = `2X` |
| `WorkerFaultsStillSwitchTheWorkerOffAtTheCeiling` | 대조: 모델이 없는 디렉터리의 실제 워커 장애는 여전히 1·2·3 으로 세고 3번째에 `DISABLED` |
| 유지 | `OrdinaryPixels…`, `ALargestFiniteResultIsNotRefused`, `AiStubProducers…` |

빨강 → 초록: 수정 전 시험 2개 빨강(워커 시험은 round 1 부터 "counted toward the ceiling", round 3 "switched off"), 수정 후 5개 모두 초록.

## 4. 반증 (`arms.txt`)

| 약화 | 빨강이 된 시험 |
|------|----------------|
| Q1 구분 제거 (분기가 선택되지 않게) | 워커 시험만 |
| Q2 브리지가 플래그를 세우지 않음 | 워커 시험만 |
| Q3 프로세스 안 알림 제거 | 프로세스 안 시험만 |
| Q4 워커 경로 알림 제거 | 워커 시험만 |
| Q5 워커 경로가 실패 횟수를 올림 | 워커 시험만 |
| Q6 프로세스 안이 `INVALID_INPUT` 으로 되돌아감 | 프로세스 안 시험만 |
| 복원 후 | 소스 바이트 동일, 대조군 5개 초록 |

## 5. 회귀

`ci-ai` ctest (`XPE_AI_EXPECT_ONNX=1`, `ai-onnx` 잡과 같은 명령): 394개 중 실패 0. `ci-post`: 1091개 중 실패 0 (`ctest_summaries.txt`).

## Gaps (미검증)

- CI 러너 실행은 푸시 뒤에야 본다.
- 실패 2회 뒤 극단 영상 1장이 횟수를 0 으로 되돌리는 동작은 시험으로 묶지 않았다 (§1).
- 실제 모델(U-Net)로는 실행하지 않았다. `Y=2X` 장난감 모델만이다.
- `clients/`·`gui/` 에서 `AI worker failed|non-finite|INVALID_INPUT` 문구를 grep 했을 때 알림 문구 매칭은 없었다. 다만 `gui/ImageProcTest/Services/AiBoneSuppressionStage.cs:422` 는 `InvalidInput` 을 "시도하지 않음(NotAttempted)" 으로 분류한다. 모델이 실제로 돌았다가 결과를 낸 거부는 이제 `PROCESSING_FAILED`(시도함)로 분류되므로 이 변경이 GUI 분류와 맞는다 — 코드를 읽어 안 것이고 GUI 에서 실행해 보지는 않았다. 181h 커밋은 푸시 전이라 외부에 나간 적은 없다.

## Residual-risk (잔여 위험)

- 워커 경로의 거부 결과는 "출력 = 입력" 이라 호출자가 반환 코드를 보지 않으면 처리 안 된 영상을 처리된 것으로 쓸 수 있다. 이 경로의 기존 규약이며 알림이 이를 알린다.
