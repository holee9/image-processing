# QA-B-191 M4e — 중단 알림이 공유 효과를 말하게, 라벨 허용 문자

카드: QA-B-191 M4e · 관련: #130, REQ-AI-002, REQ-AI-092 · 결정: 리더 "M4c 수신" 절(D10 대체)

## 주장 (Claim)

1. 워커가 중단되는 알림(3회째 실패)이 두 기능의 효과를 모두 말하고 어느 기능의 실패가 원인인지 적는다. 부위 인식과 뼈 억제 어느 쪽이 3회째여도 같은 문구 틀을 쓴다(함수 `pushWorkerDisabledAlert` 하나).
2. 라벨 허용 문자는 인쇄 가능한 ASCII(0x20~0x7E) 중 큰따옴표·역슬래시 제외다. 어긋나는 사이드카는 두 경로 모두 "모델을 쓸 수 없음"이다. 헤더에 한 줄로 적었다.

## 전체 문구 (레인 간 계약)

중단(3회째 실패), `{cause}` 는 `body-part recognition` 또는 `bone suppression`:

```
AI worker failed (code {n}, failure 3 of 3) during {cause} and is disabled for this session: body-part recognition returns UNKNOWN, bone suppression returns the input image unchanged (REQ-AI-002, REQ-AI-092)
```

중단 전 실패 알림은 바꾸지 않았다(기능별): 부위 인식 `AI worker failed (code {n}, failure {k} of 3): body-part recognition returns UNKNOWN; use the deterministic body-part lookup (REQ-AI-002, REQ-AI-092)`, 뼈 억제 `AI worker failed (code {n}, failure {k} of 3): the input image is returned unchanged (REQ-AI-002, REQ-AI-092)`. 이전 중단 문구 두 가지(D10 부위 인식판, 뼈 억제의 "input images are returned unchanged")는 이 문구로 대체됐다.

clients/·gui/ 매칭 여부: `clients/ImageProcTest`, `gui/ImageProcTest` 를 `disabled for this session|AI worker failed|returned unchanged` 로 검색했다. 알림 문구와 매칭하는 코드는 없다. GUI 는 `xpe_ai_worker_state` 의 상태값으로 자기 문구를 만든다(`AiBoneSuppressionStage.cs:755`), 클라이언트의 `AlertDisplayFormatter.cs` 는 손실 알림만 다룬다. 메시지 길이는 약 215자로 기존 저신뢰 문구(약 190자)와 같은 규모다.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 392 tests.` 스킵 5, 실패 0, 경고 0. 스텁: 316 통과·스킵 81·실패 0, 경고 0. (시험 수 변동 없음: 기존 시험을 갱신했다)
- 린트 0 error, `check_header_docs.py` 0 findings, `check_spec_test_refs.py` stale 없음, `check_req_citations.py` 새 고아 없음
- 반증 3개(`m4e_arms_out.txt`), 매번 build_ok=True, 파일 복원 바이트 동일, 마지막 대조군 18/18:
  - E1 옛 문구(부위 인식만 말함)로 되돌림 → 문구 시험 2개만 빨강(부위 인식 원인 중단, 뼈 억제 원인 중단)
  - E2 적재기가 비 ASCII 라벨 허용 → `ALabelOutsidePrintableAsciiOr...` 빨강
  - E3 뼈 억제가 원인 이름을 틀리게 적음 → 뼈 억제 원인 시험 빨강
- 시험: 부위 인식 원인 중단은 코드 -3, 뼈 억제 원인 중단은 코드 -9(뼈 모델 없음)로 문구 전체를 일치 단언. 라벨은 큰따옴표·역슬래시·제어 문자(BEL)·비 ASCII 두 가지·DEL 이 두 경로에서 모두 "unavailable", 경계(공백, 물결표)가 있는 라벨 `UPPER ARM~` 은 두 경로 모두 정상.

## 기준 (Baseline)

같은 트리 `dev/postprocess`, `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. M4c 시점 수치는 `m4c_report.md`.

## 미검증 (Gaps)

- doxygen 은 이 PC 에 없어서 CI 의 `doxygen-headers` 잡이 판정한다(python 점검 3종은 통과).
- 뼈 억제 기존 시험 중 중단 알림의 옛 문구를 단언하는 것은 없었다(검색: `disabled for this session|input images are returned` 는 시험에서 부위 인식 시험뿐). 문구가 다른 레인의 시험에 인용됐는지는 이 저장소 전체를 검색하지 않았다(`clients/`, `gui/` 만).
- 실제 라벨 형식은 정해지지 않았다(리더: 정해지면 다시 본다). 현재 규칙은 장난감 사이드카 기준이다.

## 잔여 위험

- 비 ASCII 라벨(예: 한글 부위명)이 필요해지면 워커 응답 형식(이스케이프·인코딩)을 먼저 정해야 하고, 그때까지 두 경로 모두 사이드카를 거절한다. 거절은 "unavailable" Warning 으로 보이며 이유 문구에 라벨 문자 문제가 들어간다.
