# QA-B-198b — Codex #95 보류 3건 (파이프 상대 인증, 거부 메모, 카드 조회 잠금)

근거: Codex #95 (`.moai/state/codex-archive/95.md`), 카드 `QA-B-198b.md`. 대상 커밋은 198 M2(`e737a9ad`)와 195d(`0765c8b8`) 위에 쌓인다. 추적: 항목 1은 #250, 항목 2·3은 #130.

## 0. 결과 요약

| 항목 | 고친 것 | 시험 | 반증 |
|---|---|---|---|
| 1 높음 | 호스트는 연결 직후 서버 PID 가 자기가 띄운 자식인지 확인, 워커는 클라이언트 PID 가 호스트 PID(argv[2])인지 확인, 파이프 이름에 128비트 난수, `FILE_FLAG_FIRST_PIPE_INSTANCE`, 호스트는 익명 가장 수준으로 연결 | `WorkerPipePeer` 11개 | 7개 방어 모두 반증이 터짐 |
| 2 보통 | `bodyPartTrustFailStamp`·`boneTrustFailStamp`와 `modelFilesStamp` 삭제. 거부된 역할은 매 호출 다시 검증, 알림만 세션당 1회 | `TrustRecheck` 3개 | 뼈·부위 두 역할 모두 터짐 |
| 3 보통 | 카드 조회가 잠금 아래에서는 모델 경로만 복사하고 읽기·검증은 잠금 밖, 끝나면 다시 잠가 경로가 그대로인지 확인(바뀌면 3회까지 재시도) | `TrustRecheck` 2개 | 터짐 |

검증(관측 출력은 이 폴더의 `summary_*.txt`, `arms_out.txt`): `ci-ai` 전체 554개 중 통과 549, 실패 0, 건너뜀 5(이전 533 통과 + 새 16). OOM 시험 13개 통과. 스텁 빌드 435 통과(119 건너뜀), 스텁 OOM 6 통과. 새 시험 16개는 시험마다 별도 프로세스(ctest)로도 16/16 통과. doxygen 경고 0건, `check_header_docs.py` 20개 헤더 0건.

## 1. 파이프 상대 인증 (#250)

### 바뀐 것
- `ai_worker_supervisor.cpp`: `NewWorkerPipeName()` — `xpe_ai_worker_<호스트 PID>_<일련번호>_<난수 32 hex>`. 난수는 `BCryptGenRandom`(시스템 RNG) 128비트이고 실패하면 빈 이름을 돌려주어 워커를 시작하지 않는다(닫는 쪽 실패). 시작 인자는 `"<exe>" <파이프> <호스트 PID>`. 연결 직후 `GetNamedPipeServerProcessId` 가 자식 PID(`pid_`)와 다르면 자식을 종료하고 `XPE_ERR_PROCESSING_FAILED`, INIT 메시지는 보내지 않는다.
- `ai_worker_main.cpp`: 파이프를 `FILE_FLAG_FIRST_PIPE_INSTANCE` 로 만든다. `ConnectNamedPipe` 뒤(이미 연결된 경우 포함) `GetNamedPipeClientProcessId` 가 호스트 PID 와 같지 않으면 `DisconnectNamedPipe` 로 끊고 다음 클라이언트를 기다린다. 남의 클라이언트를 16번 거절하면 종료(코드 1)한다. argv[2] 가 없으면 검사하지 않는다(손으로 띄운 진단·프로토콜 시험용; 호스트는 항상 넘긴다). 있는데 양의 정수가 아니면 검사를 끄지 않고 종료한다.
- `ai_ipc_bridge.cpp`: 호스트 연결에 `SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS` — 파이프를 대신 서비스하는 프로세스가 호스트의 신원으로 행동할 수 없다.
- `tests/fake_worker_main.cpp`: 모드 `spoof_server` — 자기는 파이프를 만들지 않고, 자기 자신(모드 ok)을 둘째 프로세스로 띄워 그쪽이 파이프를 서비스하게 한다.

### 선택: 선점 공격이 끝나는 모양
같은 사용자 프로세스가 워커의 파이프에 호스트보다 먼저 붙으면 워커는 그것을 끊고 다음을 기다린다. 거절이 파이프를 비우므로 한 번의 선점이 파이프를 붙잡고 있지는 못한다. 16번을 넘기면 워커가 종료하므로 호스트의 시작이 실패한다 — 이 공격은 **서비스 거부로 끝나고 잘못된 상대와의 대화로 끝나지 않는다**(카드가 정한 방향). 이름이 예측 불가능하므로 공격자가 이름을 알아내려면 이미 호스트나 워커를 읽을 수 있어야 한다.

### 시험 (`modules/ai/tests/test_ai_worker_pipe_peer.cpp`, 11개)
| 시험 | 단언 |
|---|---|
| ControlTheWorkerAnswersTheHostItWasToldTo | 대조군: 올바른 호스트 PID 면 하트비트에 답함 |
| TheWorkerCutsOffAClientThatIsNotTheHostAndKeepsWaiting | 호스트가 아닌 클라이언트는 응답을 못 받고, 워커는 살아서 기다림 |
| AfterTooManyForeignClientsTheWorkerExitsInsteadOfServingThem | 17번 접속 뒤 워커가 종료 코드 1 |
| AWorkerThatIsToldNoHostDoesNotCheck | argv[2] 가 없으면 검사 없음(경계의 명시) |
| ABadHostIdEndsTheWorkerInsteadOfSwitchingTheCheckOff | `0`, `abc`, `12x`, `-5` 모두 종료 코드 1 |
| AWorkerCannotTakeANameThatIsAlreadyServed | 이름이 이미 서비스 중이면 종료 코드 1(선점자 최대 인스턴스 1과 255) |
| ControlTheSupervisorStartsAWorkerWhoseProcessServesThePipe | 대조군: 정상 모조 워커는 시작됨 |
| TheSupervisorRefusesAPipeServedByAProcessItDidNotStart | 파이프를 다른 프로세스가 서비스하면 `XPE_ERR_PROCESSING_FAILED`, 자식은 종료됨 |
| EveryPipeNameCarries128FreshRandomBits | 300개 이름의 난수 부분 32 hex 가 서로 모두 다름 |
| ControlADefaultClientCanBeImpersonatedByTheServerItConnectsTo | 대조군: 기본 수준에서는 서버가 클라이언트 신원을 얻음 |
| TheHostsBridgeConnectsAtTheAnonymousLevelSoAServerCannotActAsTheHost | 브리지로 연결하면 서버의 `OpenThreadToken` 이 `ERROR_CANT_OPEN_ANONYMOUS` |

### 시험이 찾아낸 결함 하나
처음에는 argv[2] 를 `strtoul` 로만 읽었다. `-5` 는 부호가 감겨 큰 양수가 되어 받아들여졌고, 워커는 검사를 끄는 대신 엉뚱한 PID 를 기다리는 상태로 남았다(`ABadHostId…` 가 10초를 기다린 뒤 빨강, 종료 코드 -1). 첫 글자가 숫자인지와 범위를 검사하도록 고쳤다.

### 반증 (`arms_out.txt`, 구동기 `arms_driver.py.txt`)
방어 하나만 끄고 재빌드해 해당 시험이 빨개지는지 보았고, 끝나면 원본을 바이트 단위로 복원해 해시가 같음을 확인했다(모든 팔 `restored byte-identical: True`).

| 끈 방어 | 빨개진 시험 | 결과 |
|---|---|---|
| 호스트의 서버 PID 확인 | TheSupervisorRefusesAPipeServed… | 터짐 |
| 워커의 클라이언트 PID 확인 | TheWorkerCutsOff…, AfterTooManyForeignClients… | 터짐 |
| `FILE_FLAG_FIRST_PIPE_INSTANCE` | AWorkerCannotTakeANameThatIsAlreadyServed | **처음엔 안 터졌고, 시험을 고친 뒤 터짐(아래)** |
| 난수 부분(0으로 채움) | EveryPipeNameCarries128FreshRandomBits | 터짐 |
| 익명 가장 수준(기본으로) | TheHostsBridgeConnectsAtTheAnonymousLevel… | 터짐 |
| 호스트 ID 의 `strtoul` 부호 검사 | ABadHostId… | 터짐 |
| 16회 상한(사실상 무제한) | AfterTooManyForeignClients… | 터짐 |

**안 터진 반증 하나와 그 원인.** `FILE_FLAG_FIRST_PIPE_INSTANCE` 를 끄고도 시험이 초록이었다. 이 방어를 중복이라며 지우지 않고 오류 코드를 실측했다(`first_instance_probe.cpp.txt`, `first_instance_out.txt`): 선점자가 최대 인스턴스 1로 만든 경우에는 플래그 없이도 워커의 생성이 오류 231로 실패하지만, **선점자가 최대 인스턴스 2 또는 255로 만들면 플래그 없는 워커가 같은 이름에 두 번째 인스턴스를 오류 없이 만든다**(플래그가 있으면 오류 5). 방어는 중복이 아니었고, 내 시험이 최대 인스턴스 1 선점자만 써서 그 경우를 못 잡고 있었다. 시험을 선점자 1과 255 둘 다로 고친 뒤 반증이 터졌다.

### 잔여 위험 (헤더 `WORKER PRIVILEGES` 문단에도 적음)
- 같은 사용자의 프로세스는 호스트나 워커를 디버그할 수 있다. 이 변경은 그것을 막지 않는다.
- 같은 사용자가 워커의 파이프에 호스트보다 먼저 17번 접속하면 시작이 실패한다(서비스 거부). 탈취는 아니다.
- 호스트 PID 가 재사용되는 경우: 워커는 호스트와 같은 job 에서 죽는다(`KILL_ON_JOB_CLOSE`)고 가정했다. 이 보고서에서 그 가정을 다시 시험하지 않았다.
- argv[2] 없이 손으로 띄운 워커는 상대를 확인하지 않는다(진단용). 공급되는 제품 경로는 항상 argv[2] 를 넘긴다.
- 네트워크 제한 없음(198 M2 와 같음, #250 의 AppContainer).

## 2. 거부 메모 삭제 (#130)

### 바뀐 것
`ai.cpp`: `boneTrustFailStamp`, `bodyPartTrustFailStamp`, 두 곳의 조기 반환, `modelFilesStamp` 함수를 삭제했다. 거부된 로드는 매 호출 `ReadVerifiedModelFiles` 로 다시 읽고 검증한다. 세션당 1회 알림(`boneTrustAlerted`/`bodyPartTrustAlerted`)은 그대로다. `ai_api.h` 의 "파일이 바뀔 때까지 기억됨" 문구를 "매 호출 다시 검증" 으로 고쳤다.

### 시험 (`test_ai_trust_recheck.cpp`)
- ARefusedBoneModelIsReadAndVerifiedAgainOnEveryCall — 읽기 훅으로 거부된 호출마다 읽기 수를 센다: 첫 호출 N>0(대조군), 둘째 호출 후 2N, 셋째 후 3N.
- ARepairedBoneModelWithTheSizeAndWriteTimeOfTheRefusedOneIsAcceptedAtOnce — 모델 한 바이트를 바꾸고(크기 동일) 쓰기 시각을 원래대로 맞춰 거부시킨 뒤, 원래 바이트와 같은 시각으로 되돌리면 다음 호출이 `XPE_OK`(출력 ×2).
- ARepairedBodyPartSidecarWithTheSizeAndWriteTimeOfTheRefusedOneIsAcceptedAtOnce — 부위 모델 사이드카에서 같은 일(`CHEST`→`CHESS`, 같은 크기, 같은 시각 → 복구 후 `CHEST`).

### 반증
뼈 역할 메모를 되살린 팔: 위 뼈 시험 둘이 빨강. 부위 역할 메모를 되살린 팔: 부위 시험이 빨강. 둘 다 터졌고 다른 시험은 빨개지지 않았다.

### 거부 상태에서 매 호출 재검증 비용 (`cost_run.txt`, 측정 시험 `DISABLED_MeasureTheCostOfARefusedCall`)
같은 빌드·같은 기계에서 5회 평균, 한 바이트를 바꾼 패딩 모델:

| 모델 크기 | 거부된 호출 1회 |
|---|---|
| 1 MiB | 6.2 ms |
| 64 MiB | 85.6 ms |
| 256 MiB | 343.4 ms |

이 측정은 호출자가 거부된 모델을 계속 호출하는 경우의 비용이며, **이 시간 동안 인-프로세스 경로는 모듈 잠금을 쥐고 있다**(`xpe_bone_suppress`·`xpe_bodypart_recognize` 는 로드를 잠금 아래에서 한다). 이 카드는 그 부분을 바꾸지 않았다. 거부된 모델을 계속 호출하는 구성이면 호출마다 256 MiB 기준 약 0.34초가 든다는 것이 이번에 새로 생긴 비용이다.

## 3. 카드 조회 잠금 (#130)

### 바뀐 것
`xpe_ai_get_model_card_impl`: 잠금 아래에서 `modelDirPath` 복사만 하고(예외 시험 훅 `xpe_ai_test_set_mutex_held_hook` 은 이 잠금 영역에 그대로), 읽기·서명 검증(`cardOfRoleIfItIs`, 이제 상태가 아닌 경로 문자열을 받음)은 잠금 밖에서, 끝난 뒤 다시 잠가 `initialized` 와 경로가 그대로인지 확인한다. 바뀌었으면 현재 경로로 다시 읽고(최대 3회), 그래도 안 맞으면 카드 없음. `xpe_ai_shutdown`/`xpe_ai_init` 과 호출이 겹치지 않는 것은 모듈의 기존 계약이며 이번에 바꾸지 않았다.

### 시험
- ASlowCardLookupDoesNotKeepAnotherCardLookupWaiting — 읽기 훅을 한 스레드에서만 400 ms 느리게(스레드 지역 표지) 하여 큰 모델을 흉내. 느린 조회는 ≥ 1000 ms(대조군), 250 ms 뒤 시작한 둘째 조회는 400 ms 미만.
- AnInferenceIsNotKeptOutByASlowCardLookup — 같은 느린 조회 동안 데워 둔 세션의 `xpe_bone_suppress` 가 400 ms 미만(스텁 빌드는 잠금을 잡는 추론이 없어 건너뜀).

### 반증
조회 전체를 잠금 아래로 되돌린 팔(잠금 공유): 두 시험 모두 빨강, 다른 시험은 빨개지지 않음.

### 시험하지 않은 것
읽는 동안 모듈이 다른 디렉터리로 다시 초기화되는 분기. 그것을 만들려면 다른 스레드가 호출 안에 있는 동안 init 을 해야 하는데 모듈 계약이 금지한다. 코드는 읽어서만 확인했다.

## 4. Gap / 잔여 위험

Gap(관측하지 않은 것)
- 호스트 PID 재사용 시 워커가 호스트와 함께 죽는다는 가정은 이번에 다시 시험하지 않았다.
- 카드 조회의 "경로가 바뀜" 재시도 분기는 실행하지 않았다.
- 호스트 쪽 서버 PID 확인은 모조 워커(`spoof_server`)로 시험했다. 실제 같은 사용자 공격자가 이름을 알아내는 경우는 이름이 난수이므로 구성하지 못했다.
- 이름 난수의 품질은 시스템 RNG 에 맡겼고 통계적으로 따로 시험하지 않았다(300개 중복 없음만 관측).
- GUI 의 낮은 무결성 워커 C08~C10 실행은 이 카드의 범위 밖이다(리더가 병합 뒤 지시).

잔여 위험
- 위 §1 의 같은 사용자 위협 모델(디버그, 17회 선점으로 인한 시작 거부).
- §2 의 거부 상태 호출 비용(256 MiB 0.34초, 잠금 아래).
- 새 시험 일부는 시간 상한(400 ms 대 1000 ms)을 쓴다. 부하가 큰 기계에서 둘째 조회가 400 ms 를 넘으면 위음성이 아니라 거짓 빨강이 난다(방향은 안전하지만 불안정할 수 있음). 이번 실행의 관측에서는 모두 통과.
