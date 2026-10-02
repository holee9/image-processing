# QA-B-193 — 뼈 억제 ERROR 응답도 같은 엄격 파서로

카드: QA-B-193 · 관련: #130 · 출처: QA-B-191 M4f 보고서 한계 ①, 191 의 원칙("결과가 아니라 결합")

## 주장 (Claim)

브리지의 ERROR 응답 해석이 **함수 하나**(`ParseWorkerErrorFrame`, `ai_ipc_bridge.cpp`)로 통일됐다. 부분 문자열 검색인 `ParseErrorCode` 는 삭제했다. 뼈 억제는 이 함수를 `model_unavailable` 불허로, 부위 인식은 허용으로 부른다. 뼈 억제의 실패 카운트·중단 규칙은 건드리지 않았다.

## 판단: 뼈 억제 ERROR 에 `model_unavailable` 이 오면

**프로토콜 장애**(연결을 끊고 실패로 센다)로 정했다. `true` 만이 아니라 `false`·문자열 등 어떤 형태든 마찬가지다.
- 근거: 이 필드는 "부위 모델을 쓸 수 없음" 이라는 부위 인식 요청만의 개념이다. 뼈 억제 요청의 답에 이 필드가 오면 워커가 이 요청에 없는 말을 하는 것이고, 그 프레임의 다른 필드도 믿을 수 없다. 이렇게 두면 나중에 뼈 억제에도 비슷한 필드를 만들 때 "어느 요청에서 무엇을 믿는가" 를 파서의 한 인자(`allow_model_unavailable`)에서 정한다.
- 알 수 없는 다른 키는 두 요청 모두에서 무시한다(성공 응답과 같은 규칙, M4f 와 동일).

## 증거 (Evidence)

- 돌려주는 코드는 그대로다(호환). 뼈 억제는 이전과 같이 해석 불가 프레임에 `XPE_ERR_PROCESSING_FAILED` 와 연결 끊김, 부위 인식은 M4f 와 같이 `XPE_ERR_IO_FAILED` 와 연결 끊김이다(기존 Codex #11 시험이 뼈 억제의 코드를 단언하므로 바꾸지 않았다). 파서만 공유하고 "장애일 때 무슨 코드를 돌려주나" 는 호출자의 몫이다.
- ci-ai: `[  PASSED  ] 400 tests.` 스킵 5, 실패 0, 경고 0 (M4f 시점 396: +4). 스텁: `[  PASSED  ] 322 tests.` 스킵 83, 실패 0, 경고 0 (M4f 시점 319·스킵 82: 가짜 워커 시험 +3, 풀 전용 +1 스킵). 기존 `IpcDeadline.*` ERROR 프레임 시험(코드 없음·문자열 코드·범위 밖·정수 아님)은 변경 없이 그대로 통과.
- 시험(`test_worker_bodypart.cpp` 의 `WorkerBoneErrorFrame.*`, 목록은 새 헤더 `tests/error_frame_cases.h` 로 옮겨 두 요청이 같은 목록을 쓴다):
  - `EveryFrameTheProtocolForbidsEverywhereIsAFaultForBoneSuppressionToo`: 부위 인식 금지 형태 34종을 뼈 억제에 그대로 → 모두 `PROCESSING_FAILED`·워커 폐기
  - `TheBodyPartFieldIsAFaultInAnyFormBecauseBoneSuppressionHasNoSuchNotion`: `true`+IO_FAILED, `true`+CONFIG_INVALID, `false`, 문자열 → 뼈 억제에서는 모두 장애(부위 인식에서는 앞의 둘이 허용되는 형태)
  - `EveryFrameTheProtocolAllowsIsPassedThroughAndKeepsTheWorker`: 최소형, 메시지, 이스케이프된 따옴표·역슬래시, 모르는 키, -99, 토큰 사이 공백 → 워커의 코드 그대로, 워커 유지
  - `TheRealWorkersOwnErrorFramesAreAcceptedAndKeepTheWorker`: **실제 `xpe_ai_worker.exe`** 에 뼈 억제를 요청 — 뼈 모델이 없는 디렉터리(`models_missing`)는 IO_FAILED, 모델 파일이 모델이 아닌 디렉터리(`models_broken`)는 CONFIG_INVALID. 각 두 번 호출해 같은 워커(시작 1회)가 둘 다 답함. (이 시험의 경로는 슬래시라 역슬래시 이스케이프가 든 실제 워커 메시지는 시험하지 않았다 — 그 형태는 가짜 워커의 허용 목록 시험이 덮는다)
  - 별개의 실제 워커 호환 증거: M4c 의 `BodyPartWorkerPath.BoneSuppressionFailuresSwitchOffBodyPartRecognitionToo` 가 중단 알림의 코드 `-9` 를 단언하는데 이것은 실제 워커의 뼈 억제 ERROR 프레임을 새 파서가 읽어야 통과한다(통과함)
- 반증 3개(`arms_out.txt`), 매번 빌드 성공, 파일 복원 바이트 동일, 마지막 대조군 41/41:
  - G1 뼈 억제가 `model_unavailable` 을 허용 → `TheBodyPartFieldIsAFault…` 빨강
  - G2 뼈 억제가 해석 불가 ERROR 프레임을 답으로 취급(코드 0 으로 흘러감) → 새 금지 형태 시험 + 기존 `IpcDeadline.AnErrorFrameWith…` 3개 + `ControlAWorkersOwnErrorFrameKeepsTheConnection` 등 9개 빨강. 카드가 말한 "부분 문자열 검색으로 되돌리면" 의 문자 그대로의 복귀(옛 `ParseErrorCode` 복원)는 하지 않았다 — 옛 함수를 지웠기 때문에, 같은 성질(해석이 안 되는 프레임을 답으로 받는다)을 만든 G2 로 갈음했다
  - G3 공유 파서가 모순된 플래그를 믿음 → 부위 인식 시험 2개 빨강. **뼈 억제의 금지 형태 시험은 G3 에서 빨개지지 않는다**: 뼈 억제는 플래그가 어떤 형태로든 오면 장애로 보므로 모순 여부를 보기 전에 이미 막힌다(이 자체가 G1 의 시험이 덮는 성질).

## 기준 (Baseline)

같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. 이전 수치는 `QA-B-191/m4f_report.md`.

## 변경 요약

- `ai_ipc_bridge.cpp`: `ParseWorkerErrorFrame` 신설(M4f 의 인라인 규칙을 함수로), `ParseErrorCode` 삭제, 뼈 억제·부위 인식 ERROR 가 같은 함수를 호출.
- 시험: 공유 목록 헤더 `error_frame_cases.h`, 가짜 워커 모드 `bone_error_raw`, `WorkerBoneErrorFrame.*` 4건. 부위 인식 시험은 같은 목록을 헤더에서 가져오도록 바꿨다(목록 내용은 M4f 와 같음).

## 미검증 (Gaps)

- 뼈 억제 외의 요청 종류(스티치·노이즈 제거·모델 카드)에는 브리지의 클라이언트 함수가 없어서 해석할 ERROR 가 없다. 그런 함수가 생기면 `ParseWorkerErrorFrame` 을 `allow_model_unavailable=false` 로 부르는 것이 기본이며, 이 보고서는 그것을 시험하지 않았다.
- `ExpectErrorFrameDrops` 계열의 기존 시험(`test_ipc_deadline.cpp`)은 목록 4종이라 새 34종 목록과 별개로 남겼다. 합치지 않았다.
- doxygen 은 이 PC 에 없다(이 변경은 `.cpp` 와 시험이라 헤더 주석은 바뀌지 않았다).

## 잔여 위험

- 파서를 공유하므로 이후 이 파서의 규칙을 바꾸면 두 요청이 동시에 바뀐다. 그것이 목적이지만, 한쪽만 느슨하게 해야 할 때는 `allow_model_unavailable` 처럼 인자로 구분해야 한다.
