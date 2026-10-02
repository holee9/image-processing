# QA-B-181j — Codex #65 보류: 쓰레기 응답이 예외를 타고 상한을 피함 + 정책 확정 2건 (#233)

## 요약

| 항목 | 결과 |
|------|------|
| 쓰레기 성공 응답이 "모델 비유한" 예외를 탐 (카드 1) | **재현했다**: 수정 전 `reply_json` 이 빈 JSON·쓰레기·잘못된 폭/높이/형식 등 13가지 봉투 모두 NaN 화소와 함께(그리고 유한 화소와도) 성공 경로로 들어갔다 (`red_before_fix.txt`). 브리지가 JSON 을 엄격하게 파싱해 `success:true`·요청과 같은 폭·높이·`float32` 를 검증한다. 어긋나면 **프로토콜 장애**: 연결을 끊고 `XPE_ERR_IO_FAILED`, 상한에 센다. |
| 정책 확정: 유효한 응답은 장애 연속성을 끊는다 (카드 2) | 혼합 순서 `장애·장애·유효한 비유한·장애·장애` 의 상태·알림 수를 시험으로 고정했다. `ai.cpp` 의 옛 주석을 새 정책으로 고쳐 썼다. 문서 위치는 §3. |
| 공통 알림이 워커 건강을 단정 (카드 3) | 알림을 관측 사실만으로 줄였다. 두 경로의 코드·알림 계약은 유지, 시험이 문구 전체를 고정한다. |

## 정정

QA-B-181i 보고서가 "6장"이라 적은 곳은 **5가지 × 2회 = 10장** 이다. 시험 코드의 반복 구조를 잘못 센 것이다. 181i 보고서 본문을 고치고 머리말에 정정을 달았다.

## 1. 봉투 검증 (카드 1)

### 1.1 변경

`ai_ipc_bridge.cpp` 의 `ValidSuccessEnvelope`: 워커의 응답 JSON 은 평평한 객체 하나(`{"success":true,"width":W,"height":H,"format":"float32"}`)이므로 그 모양만 받는 엄격한 파서다.

- 문법: 문자열 키, 값은 문자열·음이 아닌 정수·`true`·`false`·`null`. 중첩 없음, 키 중복 없음, 닫는 괄호 뒤에 아무것도 없음, 문자열 안 이스케이프 없음.
- 의미: `success` 는 불리언 `true`, `width`·`height` 는 요청과 같음, `format` 은 `"float32"`. 네 키는 필수, 그 밖의 키는 문법만 맞으면 허용.
- 위치: 총 길이 확인 뒤, 화소를 보기 **전**. 실패하면 `DropConnection` + `XPE_ERR_IO_FAILED`. 이 호출은 `last_result_nonfinite` 를 세우지 않으므로 `ai.cpp` 가 일반 장애로 세어 상한에 반영한다.
- 비유한 예외는 이 검증을 통과한 **유효한 성공 응답**에서만 일어난다.

이 검사는 181i 이전부터 있던 공백도 함께 막는다: 쓰레기 봉투 + **유한** 화소가 성공으로 받아들여지던 것. 시험이 그 경우도 본다 (`+ finite pixels`).

### 1.2 시험

| 시험 | 내용 |
|------|------|
| `IpcEnvelope.ControlAValidEnvelope…NonFinite…` | 대조: 유효한 봉투 + NaN → `PROCESSING_FAILED`, 연결 유지, 출력 불변 |
| `IpcEnvelope.ControlAValidEnvelopeWithFinitePixelsIsAccepted` | 대조: 유효한 봉투 + 유한 화소 → `OK` |
| `IpcEnvelope.ABadEnvelopeIsAProtocolFault…` | 13가지 나쁜 봉투 × {NaN 화소, 유한 화소} = 26건: 빈 JSON, 쓰레기, 닫히지 않음, `success:false`·누락·문자열 `"true"`, 폭 틀림·누락, 높이 틀림, 형식 틀림·누락, 키 중복, 뒤에 붙은 쓰레기. 모두 `IO_FAILED`, 연결 끊김, 출력 불변 |
| `WorkerSupervisor.AValidEnvelopeWithNaNPixels…` | 가짜 워커 프로세스: 유효한 봉투 + NaN → `PROCESSING_FAILED`, `LastResultWasNonFinite()` 참, 워커 유지, 두 번째 호출도 같은 워커가 응답 |
| `WorkerSupervisor.AGarbageEnvelopeWithNaNPixels…` | 가짜 워커: 빈 봉투 + NaN → `IO_FAILED`, 플래그 **거짓**, 워커 폐기 (이것이 "상한에 센다"의 근거: `ai.cpp` 는 플래그가 선 경우에만 예외로 본다) |

### 1.3 반증 (`arms.txt`)

| 약화 | 결과 |
|------|------|
| R1 봉투 검사 제거 | 13가지 나쁜 봉투 모두 통과 → 봉투 시험·가짜 워커 쓰레기 시험 빨강 |
| R2 폭 비교 제거 / R3 높이 비교 제거 | `wrong_width` / `wrong_height` 만 통과 → 봉투 시험 빨강 |
| R4 형식 비교 제거 | `wrong_format` 만 통과 → 빨강 |
| R5 `success` 비교 제거 | `success_false`, `success_not_a_bool` 통과 → 빨강 |
| R6 중복 키 허용 | `duplicate_key` 만 통과 → 빨강 |
| R7 뒤따르는 쓰레기 허용 | `trailing_junk` 만 통과 → 빨강 |
| 복원 후 | 소스 바이트 동일, 대조군 11개 초록 |

R2·R3 는 처음 빌드가 실패했다(`width` 를 쓰지 않아 미사용 매개변수 경고가 `/WX` 로 오류가 됨). 비교를 남겨 둔 채 거짓이 되도록 약화시켜 다시 돌렸고(`arms.txt` 아래쪽), 위 결과는 그 재실행이다. 처음 실패한 빌드의 오래된 실행 파일을 읽지 않도록 `build_ok` 를 먼저 확인했다.

### 1.4 한계

**유효한 봉투가 워커의 건강을 증명하지는 않는다.** 봉투를 위조·유지하면서 화소만 쓰레기로 보내는 워커는 이 검사를 통과한다. 이 검사가 하는 일은 봉투가 **깨진** 응답이 "워커가 정상 응답했다"는 분류를 빌리지 못하게 하는 것뿐이다. 화소가 유한한 쓰레기는 여전히 성공으로 받는다 (그것은 모델의 정확성 문제이고 이 계층에서 판별할 수 없다).

문법 검사의 범위: 이 프로토콜의 평평한 객체만 받는다. 이스케이프(`\`)가 든 문자열은 거부한다 — 워커가 만드는 JSON 에는 없다. 워커가 나중에 키를 더하면 문법만 맞는 한 허용된다. 중첩 객체를 더하면 거부되므로 그때는 이 파서도 고쳐야 한다.

## 2. 정책: 유효한 응답은 장애 연속성을 끊는다 (카드 2)

- 시험 `AValidNonFiniteResponseEndsARunOfWorkerFaultsAndNeverSwitchesTheWorkerOff`: 실제 워커로 `장애(모델 없음)·장애 → 유효한 비유한 응답 → 장애(멈춘 워커, 죽임)·장애(새 워커, 모델 없음)`. 확인: 비유한 응답 직후 실패 횟수 0·상태 `ACTIVE`, 마지막에 실패 횟수 2·상태 `ACTIVE`(총 장애 4회지만 연속 3회는 없다), "AI worker failed" 알림 4건·비유한 알림 1건·"disabled" 0건. 유효한 응답이 연속성을 끊지 않는다면 네 번째 이벤트가 세 번째 연속이 되어 꺼진다.
- 이 시험은 181i 의 동작을 고정하는 것이라 코드 변경 전에도 초록이었다 (빨강→초록이 아니라 고정이다). 반증 R8(되돌림 제거) → 이 시험만 빨강.
- `ai.cpp` 의 옛 주석("EVERY non-OK result … counts … even a healthy worker's model refusal")을 새 정책으로 고쳐 썼다: 연속 워커·전송 장애를 센다, 모델이 에러 프레임으로 거부하는 것은 여전히 센다, 유효한 봉투의 비유한 응답은 장애가 아니고 연속성을 끊는다, 세션 총량 제한 없음, 봉투가 깨지면 프로토콜 장애로 센다.
- `ai_api.h` 의 `xpe_ai_init` 문서의 같은 문단과 `xpe_bone_suppress` 문서를 같은 내용으로 고쳤다.

### 상한 문장이 있는 문서 위치 (수정은 리더)

`rg` 로 `docs/` 와 공개 헤더에서 "연속/consecutive/switched off" 를 찾았다. 이 목록은 그 검색에 걸린 것이고 다른 표현으로 쓴 문장은 못 잡았을 수 있다.

| 위치 | 문장 |
|------|------|
| `docs/ai-module/SRS-AI-001_Software_Requirements_Specification.md` 43, 70, 442–443행 | "3회 연속 미신호 → 워커 사망 선언", "타임아웃 3회 연속 → AI 기능 전체 비활성화", "IPC 타임아웃 3회 연속" |
| `docs/ai-module/RTM-AI-001_Requirements_Traceability_Matrix.md` 83행 | "5000ms 타임아웃, 3회 연속 시 AI 비활성화" |
| `docs/ai-module/SHA-AI-001_Software_Hazard_Analysis.md` 176행 | "IPC 타임아웃이 3회 연속 발동 → 파이프라인 정지" |
| `docs/ai-module/xpe-ai-prd.md` 214, 236, 357–358행, `README.md` 184행, `SAD-AI-001…` 382행 | 같은 계열 (미신호·타임아웃 3회 연속) |
| `docs/post-processing/xpe/XPE-SDD-002_Software_Detailed_Design.md` 916행 | "알림 정책(실패마다 Warning, 연속 3회에 세션 동안 워커 중단)" |

이 문서들은 "타임아웃·미신호"에 한정해 쓰여 있어 "유효한 응답은 연속성을 끊는다"와 충돌하는 곳은 없어 보이지만, 전부 읽고 판정하지는 않았다.

## 3. 알림 문구 — 레인 간 계약 변경 (카드 3)

| | 문구 |
|---|------|
| 181i | `AI model output was non-finite (inf/NaN) and was rejected: the image's value range may exceed what the model accepts; the AI worker itself is healthy` |
| **181j (현재)** | `AI model output was non-finite (inf/NaN); this image was not AI-processed` |

관측한 사실만: 모델 출력이 비유한이었고 이 영상은 AI 처리되지 않았다. 워커·원인 추정은 뺐다. 워커 상태는 `xpe_ai_worker_state()` 로 본다. 심각도는 Warning 그대로. 두 경로의 반환 코드(`PROCESSING_FAILED`)와 알림 1건 계약은 유지.

시험은 문구 **전체**를 고정한다(`OnlyTheContractAlert`): 알림 중 정확히 이 문구가 1건, "healthy" 를 담은 알림 0건. 반증 R9(문구에 "the AI worker itself is healthy" 를 다시 붙임) → 프로세스 안·워커 시험 둘 다 빨강.

`clients/`·`gui/` 가 이 문구를 매칭하는지는 181i 에서 grep 으로 확인했고(매칭 없음), 이번에 바뀐 문구에 대해서는 다시 확인하지 않았다.

## 4. 회귀

`ci-ai` ctest (`XPE_AI_EXPECT_ONNX=1`): 400개 중 실패 0. `ci-post`: 1097개 중 실패 0 (`ctest_summaries.txt`).

## Gaps (미검증)

- CI 러너 실행은 푸시 뒤에야 본다.
- 가짜 워커로 `ai.cpp` 전체를 거치는 시험은 없다. `ai.cpp` 가 쓰는 운영 워커 경로는 `xpe_ai_worker.exe` 고정이라 응답을 바꿀 수 없어서, "상한에 센다"는 브리지(IO_FAILED)와 감독자(플래그 거짓)까지 실행으로, `ai.cpp` 의 "플래그가 설 때만 예외" 분기는 181i 의 반증 Q1·Q2 로 묶었다.
- 실제 모델로는 실행하지 않았다 (`Y=2X` 장난감 모델).
- 문서 목록의 문장이 새 정책과 충돌하는지는 판정하지 않았다 (§2).
- 파서는 이 프로토콜의 평평한 JSON 만 다룬다 (§1.4).

## Residual-risk (잔여 위험)

- 유효한 봉투를 위조하는 비정상 워커는 이 검사를 통과한다 (§1.4).
- 비유한 응답이 상한의 연속성을 끊으므로, 장애와 비유한 거부가 번갈아 오는 워커(장애 2 → 거부 1 → 장애 2 …)는 영원히 꺼지지 않는다. 리더가 확정한 정책이고 시험으로 고정했다. 알림은 장애마다 나가므로 사용자에게는 반복적으로 보인다.
