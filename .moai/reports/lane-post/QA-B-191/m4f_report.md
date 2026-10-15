# QA-B-191 M4f — ERROR 응답을 엄격 파서로, 라벨 범위 확인, TIMEOUT 플래그 정리

카드: QA-B-191 M4f · 관련: #130 · 출처: Codex #77 (높음 1건, 보통 1건), 리더 "Codex #77 보류 + 192 수신" 절

## 주장 (Claim)

1. **(높음) 모순된 ERROR 응답이 실패 카운트를 0 으로 만들던 우회를 막았다.** 브리지의 부위 인식 ERROR 처리가 부분 문자열 검색이 아니라 성공 응답과 같은 엄격 파서를 쓴다. `model_unavailable:true` 는 모델 적재·구성 오류 코드(`XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`)와 같이 올 때만 믿고, 모순·malformed 는 프로토콜 장애(연결 폐기, `XPE_ERR_IO_FAILED`, 실패로 셈)다.
2. **(보통) 라벨 허용 범위 0x20–0x7E**: M4e 가 사이드카 적재기에는 이미 넣었고 시험도 있었다. 다만 아래 두 곳이 덮이지 않았다 — 그래서 같은 커밋에서 보강했다: (a) **정상 성공 응답의 라벨**에는 범위 검사가 없었다(DEL·0x80 이상 바이트가 통과), (b) "세션당 첫 Warning 1건·워커 실패로 안 셈"을 C ABI 로 두 경로에서 단언하는 시험이 없었다.
3. `XPE_AI_FLAG_TIMEOUT`(0x2)을 192 와 같은 방식으로 정리했다(쓰는 곳 0, 0x2 예약 주석).

## 증거 (Evidence)

- ci-ai(풀): `[  PASSED  ] 396 tests.` 스킵 5, 실패 0, 경고 0 (M4e 시점 392: 새 시험 +4). 스텁: `[  PASSED  ] 319 tests.` 스킵 82, 실패 0, 경고 0 (M4e 시점 316·스킵 81: 가짜 워커 시험 +3, 풀 전용 +1 스킵). 린트 0 error, `check_header_docs.py` 0 findings.
- **ERROR 금지 형태 목록 시험**(`WorkerBodyPartReply.EveryErrorFrameTheProtocolForbidsIsAProtocolFaultAndTheWorkerIsDiscarded`): 34가지 — 모순된 플래그(PROCESSING_FAILED·INVALID_INPUT·BUFFER_TOO_SMALL·알 수 없는 코드와 true), 플래그 값(문자열·숫자·null·중복), 코드(없음·중복·0·-0·양수 1·2자리·99·-100·선행 0·소수·지수·문자열·null·`-` 홀로), 메시지(숫자·null·알 수 없는 이스케이프·`\u` 이스케이프·원시 제어 문자·닫히지 않음), 객체(꼬리 문자·중첩·배열·빈 객체·빈 본문·JSON 아님). 모두 `XPE_ERR_IO_FAILED`, 워커 폐기, `model_unavailable` 미인정.
- **허용되는 ERROR 형태**(`AnErrorFrameTheProtocolAllowsIsAcceptedAndKeepsTheWorker`): 최소형, 메시지, 이스케이프된 따옴표·역슬래시, 플래그 false, IO_FAILED·CONFIG_INVALID 와 true, 모르는 키, -99, 토큰 사이 공백 — 워커 유지.
- **플래그는 파싱한 값이다**(`TheFlagIsParsedNotSearchedSoTextThatLooksLikeItIsOnlyText`): 메시지 안의 `\"model_unavailable\":true` 텍스트는 플래그가 아니고, 실제 플래그는 키 순서와 무관하게 믿는다.
- **Codex 의 우회 시나리오**(`ThreeContradictoryFramesInARowAreThreeCountedFailuresNeverAnUnavailableAnswer`): `{"error_code":-3,"model_unavailable":true,...}` 를 보내는 워커에 3번 → 매번 비정상 코드, `LastModelUnavailable()==false`, 워커 폐기. `ai.cpp` 는 "비정상 코드이면서 unavailable" 일 때만 면제하므로(`bodyPartViaWorker`) 이 세 호출은 모두 실패로 센다.
- 성공 라벨 범위: `EveryReplyTheProtocolForbids…` 에 DEL, 비 ASCII 바이트(0xC3 0xA9), 제어 문자, 이스케이프된 따옴표·역슬래시 추가.
- 세션 단위 라벨 시험(`BodyPartWorkerPath.ARefusedLabelIsOneWarningPerSessionOnBothPathsAndNeverAWorkerFailure`): 따옴표·역슬래시·DEL·비 ASCII 사이드카 × (프로세스 안, 워커) 에서 4번 호출 → 각 경로에서 "unavailable" Warning 정확히 1건·다른 알림 없음, 워커 경로는 상태 ACTIVE·실패 0. 같은 모델의 평범한 라벨은 두 경로 모두 응답(대조군).
- 반증 6개(`m4f_arms_out.txt`), 매번 빌드 성공, 파일 복원 바이트 동일, 마지막 대조군 32/32:
  - F1 모순된 플래그를 믿음 → 목록 시험과 3회 시험 빨강
  - F2 양수 코드 허용 → **처음엔 빨강이 안 났다**: 한 자리 양수는 길이 검사가, 부호 검사만 막는 입력(두 자리 양수 13·99)이 목록에 없었다. 케이스 세 개를 더하고 다시 돌려 빨강을 확인
  - F3 ERROR 를 이스케이프 없이 파싱 → 허용 목록 시험과 플래그-텍스트 시험 빨강
  - F4 비불리언 플래그 허용 → 목록 시험 빨강
  - F5 성공 라벨 범위 검사 제거 → 성공 목록 시험 빨강
  - F6 적재기가 DEL 허용 → 세션 단위 라벨 시험과 일치 시험 빨강
- TIMEOUT 플래그: `XPE_AI_FLAG_TIMEOUT` 를 `modules`, `clients`, `gui`, `tools`, `docs`, `.moai/specs`, `.moai/project` 에서 검색했다. 정의 한 곳 외에 읽고 쓰는 곳 0건. `docs/help/generated/doxygen/html/` 의 몇 건은 gitignore 된 생성물이다(`git check-ignore` 로 확인). 192 의 대조군(`HAS_BINARY_PAYLOAD` 16건 사용)이 같은 검색식이 쓰이는 플래그를 찾는다는 것을 이미 보였다.

## 기준 (Baseline)

같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. 이전 수치는 `m4e_report.md`.

## 변경 요약

- `ai_ipc_bridge.cpp`: `ParseFlatObject` 에 `allow_escapes`(ERROR 의 자유 텍스트가 가지는 `\"`·`\` 두 이스케이프만), 부위 인식 ERROR 처리를 엄격 파서로 교체, 성공 라벨 범위 검사.
- `ai_worker_protocol.h`: 프로토콜 문서(ERROR 규칙과 라벨 범위), 0x2 예약.
- 시험: `test_worker_bodypart.cpp`(옛 "플래그는 메시지 앞에서만" 시험은 "플래그는 파싱한 값" 시험으로 교체: 그 규칙은 부분 문자열 검색의 보정이었고 이제 필요 없다), `test_bodypart_worker_path.cpp`.

## 미검증 (Gaps)

- **뼈 억제의 ERROR 처리(`xpe_ai_ipc_bridge_bone_suppress`)는 여전히 `ParseErrorCode` 부분 문자열 검색을 쓴다.** 그쪽에는 `model_unavailable` 같은 의미를 바꾸는 필드가 없어 같은 우회는 없다(코드 숫자만 읽는다). 같은 엄격 파서로 통일할지는 이 카드의 범위 밖이라 하지 않았다 — 기존 시험이 코드 파싱 동작에 기대고 있다(Codex #11 시험).
- 모순 프레임 3회가 `ai.cpp` 에서 **워커 중단까지** 가는 것을 C ABI 로 직접 구동하지는 못했다: 워커 실행 파일이 `xpe_ai.dll` 옆의 `xpe_ai_worker.exe` 로 고정이라 가짜 워커를 끼울 수 없다. 대신 감독자 수준에서 "세 호출 모두 `LastModelUnavailable()==false` 인 실패"를 보였고, `ai.cpp` 의 계수 규칙은 M4c 의 `BodyPartWorkerPath.AModelThatExistsAndFailsToRunIsCounted…` 가 실제 워커로 단언한다. 두 조각이 이어지는 부분은 코드 읽기다.
- doxygen 은 이 PC 에 없어 CI 가 판정한다(헤더 주석 변경).

## 잔여 위험

- ERROR 프레임의 알 수 없는 키는 무시한다(성공 응답과 같은 규칙). 키를 더 엄격히(허용 목록만) 하려면 프로토콜 확장 시 같이 정해야 한다.
