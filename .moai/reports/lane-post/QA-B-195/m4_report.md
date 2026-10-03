# QA-B-195 M4 — 거부의 동작: 알림·계수·재검사·헤더 계약 (REQ-AI-007 / REQ-AI-091)

카드: QA-B-195 M4 · 관련: #130 · 설계 D4·D6 · 리더 결정(2026-10-03): 뼈 억제 워커 경로의 D6 는 (a) — QA-B-193 규칙 변경

## 주장 (Claude)

1. **알림**: 서명 거부는 역할마다 세션당 **한 번**의 `XPE_ALERT_ERROR` 를 올린다. 문구는 `AI [bone suppression|body-part recognition] is unavailable: its model failed signature verification ([reason]) and nothing was loaded (REQ-AI-007, REQ-AI-091)`. `[reason]` 은 서명 검증기의 이유 문구(`no signature file` / `the signature file is malformed` / `the signing key is not trusted` / `the signature does not match the model files` …) 그대로다. 문구는 `ai_api.h` "MODEL SIGNING" 에 레인 간 계약으로 적었다. Warning 이 아니라 Error 인 이유: 서명이 안 맞는 모델은 설치가 빠진 것이 아니라 변조의 가능성이다.
2. **반환 코드**: 뼈 억제 `XPE_ERR_CONFIG_INVALID`(출력 미기록), 부위 인식 `UNKNOWN` + `XPE_ERR_PROCESSING_FAILED`. M3 와 같다.
3. **S+ 계수에 안 센다(D6)**: 부위 인식 워커 경로는 M3 부터 안 셌다. **뼈 억제 워커 경로를 이번에 바꿨다**: 워커가 서명 거부를 `{"error_code":-4,"model_unavailable":true}` 로 보내고(`ai_worker_main.cpp`), 브리지가 뼈 억제 ERROR 프레임에서도 `model_unavailable` 을 인정하며(`ai_ipc_bridge.cpp`, 조건은 부위 인식과 같음: `true` 는 -4/-9 와 함께일 때만), 감독자가 플래그를 전달하고(`ai_worker_supervisor.cpp`), 호스트가 `-4 + 플래그` 이면 입력을 그대로 돌려주고 연속 실패 수를 0 으로 되돌리고 알림을 한 번 올린다(`ai.cpp`). 서명 거부가 워커를 끄는 일은 없다.
4. **QA-B-193 규칙 변경(리더 승인)**: 옛 시험 `TheBodyPartFieldIsAFaultInAnyFormBecauseBoneSuppressionHasNoSuchNotion` 은 "뼈 억제 ERROR 프레임의 `model_unavailable` 은 어떤 형태든 결함"을 지켰다. 그 근거("뼈 억제엔 모델 사용 불가라는 개념이 없다")가 서명 검증으로 바뀌었다. 새 시험 `TheUnavailableFlagIsBelievedForBoneSuppressUnderTheBodyPartRuleAndNothingElseChanged` 가 새 규칙을 지키고, **옛 시험이 지키던 것(모순·형식 오류 프레임은 믿지 않는다)은 새 규칙 아래에서도 지켜진다는 대조**: ① 코드와 모순되는 플래그(-3/-1/-8/알 수 없는 코드 + `true`), 불리언이 아닌 플래그(문자열·숫자·null), 중복 플래그는 `error_frame_cases::ForbiddenEverywhere()` 가 뼈 억제에도 돌고(`EveryFrameTheProtocolForbidsEverywhereIsAFaultForBoneSuppressionToo`, 변경 없이 통과) ② 문자열 플래그는 새 시험의 마지막 단언이 결함으로 확인한다. 파서(`ParseWorkerErrorFrame`)는 한 줄도 바뀌지 않았고 호출부의 `allow_model_unavailable` 만 `false`→`true` 다.
5. **재검사(실패 기억)**: 같은 모델이 연속으로 거부되면 매 호출 다시 읽고 해시하지 않는다(256 MiB 에서 125 ms, design.md). 거부 때 세 파일(`.onnx`/`.json`/`.sig`)의 크기와 쓰기 시각(Win32 `GetFileAttributesEx`)을 기억하고, 같은 값이면 기억으로 답한다. **세 파일 중 하나라도 바뀌면 같은 세션 안에서 바로 다시 검사한다**(서명 갱신 후 재시작이 필요 없다). 뼈 억제·부위 인식 각각.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 470 tests.`(M3 464 → +6: `ModelRefusalBehavior` 새 시험 6건; 규칙이 바뀐 시험은 옛 1건을 새 1건으로 바꾼 것이라 수가 변하지 않는다), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests` 8건 통과. 스텁: `xpe_ai_tests` 380건 통과(스킵 89 → 95: 새 시험 6건은 모델을 실제로 올려야 해서 스텁에서 건너뜀), `xpe_ai_oom_tests` 6건 + 건너뜀 2. 경고 0. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py` 0 findings, 린트 0.
- 새 시험(`ModelRefusalBehavior.*` 6건 + 규칙 변경 시험 1건):
  - 뼈 억제 거부 4번 호출 × 두 세션: 알림은 세션마다 정확히 1건, Error, 문구에 `bone suppression` 과 `the signature does not match the model files` 와 `REQ-AI-007`, 출력은 -777 그대로
  - 이유 3종(서명 파일 없음 / 한 바이트 짧음 / 키 id 변조): 알림이 각각 `no signature file` / `malformed` / `not trusted` 를 말한다
  - 부위 인식 거부 4번 호출 × 프로세스 안·워커: 알림 1건. 프로세스 안은 Error + 역할 + 이유, 워커는 Warning `unavailable`(아래 한계)
  - 재검사 둘(뼈 억제·부위 인식): 거부, 한 번 더 거부(기억), 서명 파일을 갱신하고 쓰기 시각을 한 시간 앞으로 옮기면 **같은 세션에서** 통과(뼈: 출력이 더 이상 -4 아님, 부위: 라벨 `HAND`)
  - 워커 뼈 억제 5번 호출(상한 3 넘어): 매번 `XPE_ERR_CONFIG_INVALID`, 출력은 입력 그대로(1.0), `xpe_ai_worker_state` 가 5번 모두 `ACTIVE`·실패 0, 알림은 전체 1건(Error, `bone suppression`), "AI worker failed" Warning 없음
- 반증 12개(`m4_arms_out.txt`), 매번 빌드 성공, 세 소스(`ai.cpp`·`ai_ipc_bridge.cpp`·`ai_worker_main.cpp`) 바이트 동일 복원, 대조군 28/28: F1 알림을 호출마다 → 워커 시험 빨강(**프로세스 안 시험은 안 빨갛다: 재검사 기억이 같은 거부를 두 번째부터 막아 알림 중복을 가리기 때문이고, 알림 1회 보장은 그 기억과 겹쳐 있다**) / F2 알림 없음 / F3 Warning / F4·F12 기억이 파일 변경을 못 봄 / F5·F6·F9 워커 계수 / F7 브리지가 플래그를 안 믿음 / F8 이유가 항상 unknown / F10 부위 인식이 일반 Warning 경로 / F11 도장이 쓰기 시각을 무시.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(15a47238 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. 처음 전체 실행에서 17건이 빨갛게 떴다: 15건은 내가 "파일 없음(-9)"도 안 세게 넓힌 것이 기존 정책(2026-10-01 사용자 승인, REQ-CHANGE-LOG-P3-AI.md 3행: 모델이 세 번 거부하면 AI 는 쓸 수 없다)과 충돌한 것이라 **서명 거부(-4+플래그)로만 좁혔고**(D6 원문과 같다), 1건은 규칙 변경 시험, 1건은 도장 계산이 읽을 수 없는 경로에서 예외를 던져 -3 이 된 것(Win32 호출로 교체). 그 뒤 oom 시험 2건이 "남은 할당"으로 빨갛게 떴고 도장 계산을 `std::filesystem` 에서 Win32 `GetFileAttributesEx` 로 바꾸자 통과했다(원인이 `std::filesystem` 안의 누수라고 확정하지는 못했다: 바꿨더니 사라졌다는 관측뿐).

## 알림 문구와 호출자

- 새 알림은 클라이언트가 모르는 새 문구다. `clients/` 와 `gui/` 에서 `unavailable` 이나 `AI worker failed` 같은 이 모듈의 알림 문구를 맞추는 코드는 검색에서 찾지 못했다(`grep` 으로 `.cs` 만 확인, 정규식 앵커 시험은 확인하지 않았다).
- GUI 는 `-4` 를 따로 해석하지 않는다(`AiBoneSuppressionStage`/`GuiAiRunner` 에서 `CONFIG_INVALID`·`-4` 검색 결과 없음). 알림은 알림 큐로 가고 호출 결과는 일반 실패 메시지가 된다.
- **리더 요청(서명과 무관한 워커 실패 수단)에 대한 사실**: 서명 거부는 안 세므로 gui 의 C09 E2E 와 `AiBoneSuppressionStageTests` 가 쓰는 "서명 없는 가짜 모델 파일"은 더 이상 워커 반복 실패를 만들지 못한다. 세는 실패를 만드는 길: ① **서명된 비모델 파일** — `modules/ai/tests/data/models_broken/`(`bone_suppress.onnx` + `.sig`, 시험 키로 서명)를 모델 디렉터리로 쓰면 워커는 `kModelLoadFailed` → `-4` **플래그 없음**으로 답하고 호스트는 센다. 조건은 두 가지: 시험 대상 DLL·워커가 `XPE_AI_TEST_HOOKS` 로 빌드되어야 하고(출하 빌드에는 시험 키를 들일 코드가 없다), 프로세스 환경에 `XPE_AI_TEST_TRUSTED_KEYS=78299ea060af78eed712ccaec277f19a5020445d764729ebf889f60565869d662357adba54ea164bc1afb6980a8f6d85a7e588d257b670a45aaa28ab9a55d175`(시험 키 1의 공개 점, 128자 hex)가 있어야 한다. 새 코드가 없고, 출하 빌드에는 존재하지 않는 수단이다. 이 길을 gui 의 C09 에서 실제로 돌려 보지는 않았다. ② 모델 파일이 아예 없으면(-9) 워커는 센다. 다만 gui 는 호출 전에 파일 유무를 먼저 검사하므로(GUI-C-185) 그 경로가 막혀 있다고 읽었다(코드 읽기, 미실행).

## 미검증 (Gaps)

- **부위 인식 워커 경로의 알림은 Error 가 아니라 기존 Warning 한 건**이다. 워커의 부위 인식 ERROR 프레임에는 코드(-4)와 `model_unavailable` 플래그뿐이어서 서명 거부·읽을 수 없는 모델·라벨 문제가 선 위에서 구분되지 않는다. 이유를 싣는 방법은 프로토콜 필드를 더하는 것인데 이 마일스톤에서 하지 않았다(리더 결정 필요: 필드 추가 vs 지금대로). 뼈 억제 워커 경로의 Error 알림은 이유 자리에 "refused by the AI worker, see its log" 를 쓴다.
- 파일 변경 감지는 크기와 쓰기 시각이다. 같은 크기에 같은 쓰기 시각으로 내용만 바뀌는 파일(쓰기 시각을 되돌리는 도구)은 거부 기억을 못 푼다. 서명 검증 자체를 우회하는 것이 아니라(기억은 거부만 유지한다) 고친 파일이 늦게 인식되는 방향의 한계다.
- 기억이 실제로 해시를 건너뛰는지(성능)를 시간으로 재지는 않았다. 시험은 동작(재검사·같은 코드)만 본다. 큰 모델로 거부 반복 비용을 재지 않았다.
- `[reason]` 중 `the signature format is not supported`, `a model file is too large to verify`, `the verifier could not run` 이 알림에 실리는 것은 시험하지 않았다(검증기 단계에서는 M1 이 각각 시험).
- 서명 검증기 실패 때 알림이 한 번만 나오는 것은 시험했으나, **세션 도중 다른 이유로 다시 거부**되어도 알림은 다시 나오지 않는다(역할당 세션당 한 번이 요구이고 그렇게 구현).
- `release` 프리셋 전체 빌드(`XPE_AI_TEST_HOOKS=OFF`)는 이번에도 실행하지 않았다(M3 와 같은 한계).
- GUI 쪽 실제 표시(알림 목록에서 새 문구가 어떻게 보이는지)는 보지 않았다.

## 잔여 위험

- 운영 신뢰 목록이 비어 있어 운영 빌드는 모든 모델을 거부하고, 이 상태가 이제 Error 알림으로 눈에 띈다. 실모델 입고(#243) 전에 운영 키가 정해져야 한다.
- 새 알림 문구를 레인 간 계약으로 적었으므로 이후 문구를 바꾸면 통보 대상이다.
- 뼈 억제 ERROR 프레임 규칙 변경은 이 모듈의 워커와 브리지가 같이 배포된다는 전제다(둘은 한 설치물). 옛 워커와 새 호스트(또는 반대)가 섞이면 플래그가 결함으로 읽히거나 안 읽힌다 — 프로토콜 버전은 올리지 않았다.
