# QA-B-191 M4b — 워커·브리지·감독자의 부위 인식 경로

카드: QA-B-191 M4b · 관련: #130, REQ-AI-003, REQ-AI-092 · 설계: `m4_design.md`(리더 승인 D7~D11)

**시험에 쓴 모델은 장난감 모델이다(분류기가 아니다). 이 보고서의 어떤 수치도 실제 모델의 성능이나 적재 시간을 말하지 않는다.**

## 주장 (Claim)

1. 워커가 `BODYPART_RECOGNIZE`(10)에 답한다. 이진 요청(뼈 억제와 같은 길이 접두 JSON + float32 화소), JSON 응답(`outcome` ok / non_finite / out_of_range). 원본을 받아 워커가 모델 입력 크기로 줄인다(D8).
2. 브리지가 그 응답을 엄격히 읽는다. 금지된 응답 28가지는 모두 프로토콜 장애(`IO_FAILED`, 연결 끊김, 워커 폐기)다.
3. "모델을 쓸 수 없음"은 `"model_unavailable":true` 가 붙은 오류 프레임이고, 감독자의 `LastModelUnavailable()` 로 구분된다(D7 의 전제). 워커는 유지된다.
4. 프로세스 안 경로와 워커가 같은 모델 적재(`ai_bodypart_model.h`)와 같은 판정(`ai_bodypart_decision.h`)을 쓰며, 같은 모델·영상에서 같은 말을 한다(13개 모델 디렉터리 × 6개 영상).
5. 워커 첫 호출의 느린 꼬리는 프로세스 시작·연결 단계에서 나며 모델 적재가 아니다.

## 증거 (Evidence)

- ci-ai(풀): `[  PASSED  ] 378 tests.` 스킵 5, 실패 0, 컴파일 경고 0 (M4a 시점 364 통과·스킵 5: 새 시험 +14)
- 스텁: `[  PASSED  ] 316 tests.` 스킵 67, 실패 0, 경고 0 (M4a 시점 306 통과·스킵 63: 가짜 워커 시험 +10 통과, 풀 전용 4건 스킵)
- 텍스트 린트 `python build/lint-tracked.py` 0 error
- 반증 7개(`m4b_arms_run1_out.txt`, `m4b_arms_run2_out.txt`), 매번 빌드 성공(build_ok=True) 뒤 시험이 빨개졌고 파일은 바이트 동일하게 복원, 마지막 대조군 14/14 통과:

| 반증 | 빨강이 된 시험 |
|---|---|
| W1 워커만 동점 규칙을 마지막 클래스 우선으로 포크 | `WorkerBodyPartAgreement.TheRealWorkerAndTheInProcessPath...` |
| W2 브리지가 64바이트 라벨 허용 | `WorkerBodyPartReply.EveryReplyTheProtocolForbids...` |
| W3 unavailable 플래그를 프레임 어디서든 읽음 | `WorkerBodyPartReply.TheFlagCountsOnlyBeforeTheFreeTextMessage` |
| W4 적재기가 따옴표 든 라벨 허용 | `WorkerBodyPartAgreement.ALabelWithAQuoteIsUnavailableInBothPaths` |
| W5 브리지가 1 초과 confidence 허용 | `WorkerBodyPartReply.EveryReplyTheProtocolForbids...` |
| W6 워커가 사용 불가 모델에 플래그를 안 붙임 | 시험 3개(일치 매트릭스, 따옴표 라벨, 워커 유지) |
| W7 브리지가 라벨 든 거절 응답 허용 | `WorkerBodyPartReply.EveryReplyTheProtocolForbids...` |

  첫 실행에서 W3·W7 은 제 반증 코드가 경고 오류(미사용 변수, 도달 불가 코드)로 빌드 자체가 실패해 무효였고(`m4b_arms_run1_out.txt` 의 `build_ok=False`), 런타임에서만 거짓인 조건으로 바꿔 다시 돌렸다(`m4b_arms_run2_out.txt`).
- 워커 첫 호출 분포(새 프로세스, 장난감 모델): 30회×2 실행에서 중앙값 77~80 ms, 최소 67 ms, 60회 중 5회가 약 670~740 ms(`m4b_cold_start_out.txt`). 시작+연결과 모델 적재+추론을 나눠 잰 40회(`m4b_cold_start_split_out.txt`): 느린 5회(626~707 ms)는 **전부 시작+연결** 쪽이고 모델 적재+추론은 매번 34~56 ms.

## 기준 (Baseline)

같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. M4a 시점 수치는 `m4a_report.md`.

## 변경 요약

- `ai_bodypart_model.h`(새): `BodyPartModel`·라벨 적재·모델 적재를 `ai.cpp` 에서 옮겨 양쪽이 같은 정의를 쓴다. 라벨에 따옴표·역슬래시·제어 문자가 있으면 **양쪽 모두** 사용 불가로 본다(워커 응답에 이스케이프가 없기 때문).
- 워커: `HandleBodyPart`, `ParseImageRequest`(뼈 억제와 공유 — 기존 뼈 억제 시험 전부 통과), 오류 프레임의 `model_unavailable` 필드, 워커 타깃에 nlohmann(D9; 정의는 `XPE_AI_BODYPART_NLOHMANN`).
- 브리지: `xpe_ai_ipc_bridge_bodypart`, 엄격 파서 `ParseFlatObject`(뼈 억제 파서에서 추출, 실수 허용은 옵션). 감독자: `BodyPartRecognize`, `LastModelUnavailable`.
- 프로토콜 헤더 주석을 새 형식으로 교체(낡은 응답 예시 삭제).
- 기존 시험 한 건 수정: `AMessageTheWorkerDoesNotHandle...` 가 "처리되지 않는 정의된 메시지"의 예로 10번을 썼다. 10번을 이제 처리하므로 같은 의도로 12번(STITCH_IMAGES)으로 바꿨다.

## 미검증 (Gaps)

- **`ai.cpp` 의 `use_worker` 분기는 아직 부위 인식을 워커로 보내지 않는다**(M4c). 이 마일스톤은 워커·브리지·감독자까지이며, 사용자가 `xpe_bodypart_recognize` 를 불러서 워커로 가는 경로는 없다.
- 연속 실패 카운트(D7)·알림 문구(D10)·092 예산 시험은 M4c.
- 느린 시작(약 650 ms)의 **원인**은 모른다. 시작+연결 단계라는 것까지만 분리했다(프로세스 생성, 보안 소프트웨어의 실행 파일 검사, 파이프 연결 재시도 간격 중 어느 것인지는 측정하지 않았다). 5 s 예산(092) 안이다. 실제 모델의 적재 시간은 이 측정과 무관하다.
- 응답 크기 한계(1024바이트 버퍼)는 63바이트 라벨로 시험했고 더 큰 응답은 시험하지 않았다. 프로토콜상 더 클 수 없다.
- 일치 매트릭스는 장난감 모델 13종 × 영상 6종이다. 실제 모델의 출력 분포는 보지 않았다.
- "두 경로가 같은 함수를 부른다"는 함수 동일성이 아니라 결과 일치(포크 반증 W1 이 잡음)로 단언했다.

## 잔여 위험

- 라벨 문자 제한(따옴표·역슬래시·제어 문자)은 M2 에서 허용되던 입력을 이제 사용 불가로 만든다. 현재 사이드카(장난감)에는 해당 문자가 없고 실제 라벨 형식은 정해지지 않았다. `ai_api.h` 의 라벨 계약 문구에 반영해야 하며 M4c 에서 함께 적는다.
- 워커 타깃이 nlohmann 을 링크한다(헤더 전용 라이브러리이며 DLL 과 같은 버전). 워커 실행 파일 크기와 /WX 영향은 크기를 재지 않았다.
