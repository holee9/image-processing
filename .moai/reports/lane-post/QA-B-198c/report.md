# QA-B-198c — Codex #99 보류 3건 (거부 모델 재검증의 잠금, 워커 인자, 카드 조회 설명)

근거: Codex #99 (`.moai/state/codex-archive/99.md`), 카드 `QA-B-198c.md`. #95 의 세 항목은 닫힘으로 확인되었다. 추적: 항목 1·3은 #130, 항목 2는 #250.

## 0. 결과 요약

| 항목 | 고친 것 | 시험 | 반증 |
|---|---|---|---|
| 1 보통 | 뼈 억제·부위 인식의 모델 읽기·서명 검증을 모듈 잠금 밖으로. 잠금 아래에서는 경로 복사와 "적재 중" 표지만, 읽은 뒤 다시 잠가 경로·초기화 상태가 그대로일 때만 결과를 사용. 같은 역할의 적재가 진행 중이면 기다리지 않고 "사용 불가"로 즉시 답함 | `TrustRecheck` 3개 | 3팔 모두 터짐 |
| 2 낮음 | 워커 인자를 정확히 두 형식으로: 감독 `<pipe> <host-pid>`, 수동 진단 `--diagnostic [<pipe>]`. 그 밖은 모두 종료 코드 2 | `WorkerPipePeer` 2개 | 2팔 터짐 |
| 3 낮음 | `ai_api.h` 의 `xpe_ai_get_model_card` 스레드 설명을 사실대로 고침 | (문서) | — |

검증(`summary_*.txt`, `arms_out.txt`): `ci-ai` 전체 559개 중 통과 554, 실패 0, 건너뜀 5. OOM 시험 13개 통과. 스텁 빌드 437 통과, 스텁 OOM 6 통과. 새·바뀐 시험 21개(`WorkerPipePeer`+`TrustRecheck`)는 시험마다 별도 프로세스(ctest)로도 21/21 통과. doxygen 경고 0건, `check_header_docs.py` 20개 헤더 0건.

## 1. 거부된 큰 모델의 재검증이 추론을 직렬화한다

### 문제의 크기
198b 에서 거부 메모를 없앤 뒤, 거부된 모델은 호출마다 읽고 검증한다(256 MiB 에서 343.4 ms, `QA-B-198b/report.md`). 그 읽기가 `state->mtx` 를 쥔 채 일어나서, 그동안 다른 역할의 추론과 카드 조회도 잠금을 기다렸다. 운영 빌드는 신뢰 목록이 비어 있어(#243) 모든 모델이 거부 상태이므로 이 비용이 운영에서 매 호출 난다.

### 바뀐 것 (`ai.cpp`)
`xpe_bone_suppress` 와 `xpe_bodypart_recognize` 의 인-프로세스 경로. 잠금을 `lock_guard` 에서 `unique_lock` 으로 바꾸고, 적재가 필요한 곳에서만 다음 순서를 따른다.
1. 이 역할의 적재가 진행 중이면(`boneLoading`/`bodyPartLoading`) 기다리지 않고 "사용 불가"로 돌아간다(뼈: `XPE_ERR_CONFIG_INVALID`, 부위: `UNKNOWN`).
2. 아니면 모델 디렉터리를 복사하고 표지를 세운 뒤 잠금을 풀고 읽는다·검증한다(`OnnxSession::Create`, `LoadBodyPartModel`).
3. 다시 잠그고 표지를 내린다. 예외가 나도 잠근 뒤 표지를 내리고 다시 던진다.
4. 모듈이 그 사이 초기화 해제됐거나 다른 디렉터리를 가리키면 결과를 버리고 "사용 불가"로 돌아간다(알림도 상태도 만들지 않음).
5. 그렇지 않으면 이전과 같은 방식으로 결과를 처리한다(알림은 세션당 1회, 성공하면 세션 게시).

### 선택: 같은 역할의 둘째 호출은 기다리지 않고 "사용 불가"
카드가 두 길을 허용했다(기다림 / 즉시 사용 불가). 즉시 사용 불가를 택했다.
- **기다리면 문제가 그대로다.** 둘째 호출이 첫째의 읽기를 기다리면 거부된 큰 모델에서 호출들이 검증 시간만큼씩 줄을 선다. 잠금 대신 역할별 대기가 생길 뿐 직렬화는 사라지지 않는다.
- **같은 답이다.** 운영(전부 거부)에서는 어차피 모든 호출이 "사용 불가"로 끝난다. 즉시 돌아가도 답이 같고 시간만 줄어든다.
- **안전한 방향이다.** 사용 불가는 문서화된 대체(deterministic 조회, 호출자의 폴백)이다.
- **대가.** 좋은 모델에서도 두 스레드가 같은 역할의 첫 호출을 동시에 하면 둘째가 그 첫 적재 동안 "사용 불가"를 받는다(헤더 `MODEL LOADS DO NOT HOLD THE MODULE LOCK` 문단에 적음). 아무것도 기억하지 않으므로 다음 호출은 로드된 모델을 쓴다.

### 시험 (`test_ai_trust_recheck.cpp`)
읽기 훅을 한 스레드에서만 400 ms 로 느리게(스레드 지역 표지) 해 큰 모델을 흉내 낸다. 거부 검증 하나가 약 1.2 초(읽기 3번) 걸린다.
- AnotherThreadIsNotKeptWaitingWhileARefusedModelIsVerifiedAgain — 뼈 모델은 거부, 부위 모델은 정상. 한 스레드가 느리게 뼈 검증을 하는 동안 다른 스레드의 부위 추론과 카드 조회가 400 ms 미만. 대조군: 거부 호출이 ≥ 1000 ms, 거부 코드 `XPE_ERR_CONFIG_INVALID`, 라벨 `CHEST`.
- AnotherThreadIsNotKeptWaitingWhileARefusedBodyPartModelIsVerifiedAgain — 역할을 바꾼 같은 측정(부위 사이드카가 거부, 뼈는 정상: 뼈 추론과 카드 조회 400 ms 미만).
- ASecondCallForTheSameRoleDuringItsFirstLoadAnswersUnavailableAtOnceAndTheNextCallWorks — 좋은 뼈 모델의 느린 첫 적재 동안 둘째 호출이 400 ms 미만에 `XPE_ERR_CONFIG_INVALID` 를 받고 출력은 그대로, 첫 호출은 결국 `XPE_OK`(출력 ×2), 셋째 호출도 `XPE_OK`. 선택의 동작을 시험으로 못 박는다.

### 반증 (`arms_out.txt`)
| 끈 방어 | 빨개진 시험 | 결과 |
|---|---|---|
| 뼈 검증을 잠금 안으로 되돌림 | AnotherThreadIsNotKeptWaiting…ARefusedModel…, ASecondCallForTheSameRole… | 터짐(둘 다) |
| 부위 검증을 잠금 안으로 되돌림 | AnotherThreadIsNotKeptWaiting…ARefusedBodyPartModel… | 터짐 |
| 뼈의 "적재 중이면 즉시 사용 불가" 검사 삭제 | ASecondCallForTheSameRole… | 터짐 |

세 팔 모두 의도한 시험만 빨개졌고 다른 시험은 빨개지지 않았으며, 끝난 뒤 원본을 바이트 단위로 복원해 해시가 같음을 확인했다(`restored byte-identical: True`).

### 사용되는 비용
거부된 모델을 부르는 스레드 자신은 여전히 호출마다 검증 시간을 쓴다(256 MiB 약 0.34초). 달라진 것은 **다른 스레드를 막지 않는다**는 점이고, 같은 역할의 동시 호출은 검증하지 않고 즉시 사용 불가로 돌아간다.

## 2. 워커 인자

### 바뀐 것 (`ai_worker_main.cpp`)
- 감독 형식: 정확히 `xpe_ai_worker <pipe> <host-pid>`(인자 2개). 호스트 PID 가 양의 정수가 아니면 종료 코드 1(198b).
- 수동 진단 형식: `xpe_ai_worker --diagnostic [<pipe>]`. 호스트 확인 없음, 파이프를 안 주면 기본 파이프.
- 그 밖(추가 인자, 옛 `<pipe>` 단독, 인자 없음, 진단 형식의 추가 인자)은 사용법을 출력하고 종료 코드 2.

옛 `<pipe>` 단독 형식은 인자 개수의 우연으로 호스트 확인 없이 시작되었다. 이제 명시한 `--diagnostic` 만이 확인 없는 시작이다.

### 직접 띄우는 곳의 갱신
시험 6곳이 옛 형식으로 워커를 띄웠다. `--diagnostic <pipe>` 로 바꿨다: `test_bone_suppress_worker_path.cpp`, `test_ipc_deadline.cpp`, `test_worker_protocol_conformance.cpp`, `test_worker_supervisor.cpp`, `test_ai_worker_sandbox.cpp`, `test_ai_worker_pipe_peer.cpp`(진단 형식 시험). `docs/`·`tools/`·`.github/` 에는 옛 형식으로 워커를 띄우는 호출이 없었다(검색: `docs` 에서 호출 형태 0건, `tools` 에서 0건, `.github` 는 바이너리 이름만 나옴). 감독자(`WorkerSupervisor`)는 이미 감독 형식으로 띄운다.

### 시험 (`test_ai_worker_pipe_peer.cpp`)
- AnExtraArgumentEndsTheWorkerInBothForms — 감독 형식과 진단 형식 각각에 추가 인자를 주면 종료 코드 2.
- ThePipeOnlyFormAndNoArgumentAreRefused — 옛 `<pipe>` 단독과 인자 없음 모두 종료 코드 2.
- AWorkerStartedInTheDiagnosticFormDoesNotCheck — 진단 형식은 호스트 없이 하트비트에 답한다(기존 시험의 개명).

### 반증
감독 형식의 개수 검사를 느슨하게(`argc != 3` → `argc < 3`): AnExtraArgumentEndsTheWorkerInBothForms 빨강. 진단 형식의 상한을 느슨하게(`argc > 3` → `argc > 9`): 같은 시험 빨강. 둘 다 터졌고 다른 시험은 빨개지지 않았다.

## 3. 카드 조회 설명

`xpe_ai_get_model_card` 의 "Thread-safe (the module lock is held while the files are read)" 는 198b 이후 반대다. 다음으로 고쳤다: 다른 호출과는 스레드 안전, 파일은 모듈 잠금 **없이** 읽고 검증한다(잠금은 경로 복사와 읽은 뒤의 확인에만), `xpe_ai_init`/`xpe_ai_shutdown` 과의 동시 호출은 계약상 금지. 문서 변경만이므로 시험이 없다(헤더 점검 20개 0건, doxygen 경고 0건).

## 4. Gap / 잔여 위험

Gap(관측하지 않은 것)
- 적재 중 예외(메모리 부족 등)가 났을 때 표지가 풀리는지를 이번 시험에서 직접 시험하지 않았다. OOM 시험 13개(전체 빌드)가 통과했고 코드는 `catch (...)` 에서 잠근 뒤 표지를 내린다고 읽어 확인했다. 표지가 남는다면 이후 그 역할의 호출이 영구히 사용 불가가 되는데, 이를 재현하는 시험은 이 변경에 들어 있지 않다.
- "읽는 동안 모듈이 다시 초기화됨" 분기(4번)는 init 과 호출의 동시 실행을 요구하므로 모듈 계약상 만들 수 없어 시험하지 않았다.
- 시험은 400 ms 대 1000 ms 상한을 쓴다. 부하가 큰 기계에서는 거짓 빨강이 날 수 있다(방향은 안전). 이번 실행에서는 모두 통과했다.
- 워커 경로(`use_worker`)의 검증은 워커 프로세스에서 일어나므로 이 변경의 대상이 아니다.

잔여 위험
- 거부된 모델을 부르는 스레드 자신의 호출당 비용(256 MiB 약 0.34초)은 그대로다. 호출자가 거부된 모델을 연속으로 부르면 그만큼 걸린다.
- 같은 역할의 두 스레드가 첫 호출을 동시에 하면 좋은 모델에서도 둘째가 한 번 "사용 불가"를 받는다(§1 의 선택). 호출자의 폴백이 안전한 방향이다.
