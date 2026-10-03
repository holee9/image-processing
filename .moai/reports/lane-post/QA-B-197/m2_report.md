# QA-B-197 M2 — 모델 카드는 검증을 통과한 사이드카에서만 만든다 (T-005b)

카드: QA-B-197 M2 · 관련: #130 · 선행: M1 `31357681`

## 주장 (Claim)

1. **카드는 검증된 모델의 사이드카에서만 나온다.** `ai.cpp` 의 고정 모델 id 4개 표(`loadedModels`)와 상수 카드(`buildStubModelCard`)를 지웠다. `xpe_ai_get_model_card` 는 모델 디렉터리의 두 역할(`bone_suppress`, `bodypart`)을 로딩과 같은 함수(`ReadVerifiedModelFiles`)로 읽고 서명을 확인한 뒤, 같은 규칙(`ParseModelSidecar`)으로 사이드카를 판정하고, `sidecar.model_id == modelId` 인 쪽의 카드를 낸다.
2. **모델이 없거나 검증에 실패하면 카드도 "사용 불가"**: `{"model_id":…,"error":"model_unavailable","reason":…}` 와 `XPE_ERR_CONFIG_INVALID`(-4). 이전에는 `XPE_ERR_IO_FAILED` 와 `model_not_loaded` 였다. 옛 id 4개(`bodypart_cnn_v1` 등)는 이제 모델이 없으니 -4 다.
3. **사이드카에 없는 선택 필드는 JSON `null`** 이다(`intended_use`, `training_data_summary`, `demographic_performance`, `limitations`, `published_date`). 기본값이나 추측으로 채우지 않는다. `pccp_status` 는 항상 `not_evaluated` 다: 비교할 승인된 PCCP 가 모듈에 없다(T-011).
4. **카드가 스키마에 맞는지 시험한다.** `tests/data/schemas/model-card.schema.json`(카드 두 갈래 oneOf)과 `model-sidecar.schema.json` 을 두고, 작은 검사기(`json_schema_mini.h`)로 모든 카드·답과 모든 픽스처 사이드카를 검사한다. 검사기는 모르는 키워드가 있는 스키마를 거부한다(조용히 무시하면 스키마가 실제보다 엄격해 보인다).
5. **실패한 호출은 모듈을 바꾸지 않는다.** 두 역할의 항목을 먼저 모두 준비하고 마지막에 문자열 이동으로 한꺼번에 반영한다. "없음" 답은 준비 전에 만든다. 처음 만든 방식(역할마다 바로 반영)은 OOM 스윕이 잡았다(K=76~85 에서 2건 남음).
6. 카드는 파일의 크기·쓰기 시각이 바뀌면 다시 만든다(같은 세션에서 모델이 교체되면 다음 호출이 본다). 안 바뀌면 파일을 읽지 않는다(실제 모델은 수백 MiB 일 수 있다).

## 시험이 바뀐 이유

`TC-MODELCARD` 계열(`test_ai_model_card.cpp`, `test_ai_model_versioning.cpp`)은 상수가 있다는 것을 단언했다. 실제 모델 없이도 통과했고, 실제 모델은 "0.1.0-stub" 으로 설명됐을 것이다. 또 요구에 없는 것(해시의 `sha256:` 접두, 지표 이름 4개 중 하나)을 상수가 우연히 가졌다는 이유로 요구했고, `ModelVersionsCanBeCompared` 는 아무것도 단언하지 않았다. 새 시험은 카드를 **같은 파일을 다른 파서(nlohmann DOM)로 읽은 값**과 비교하고, 버전은 semver 문법으로 검사한다. 이유는 두 파일 머리 주석에 적었다. 다른 파일의 카드 단언(`test_ai_fallback`, `test_ai_exception_guard`, `test_ai_oom_injection`, `test_ai_input_validation`)도 새 계약(-4, `model_unavailable`, 픽스처의 실제 id)에 맞췄다.

## 증거 (Evidence)

- ci-ai(`build\g191-ai.bat *`): `[  PASSED  ] 506 tests.`, 스킵 5, 실패 0, 컴파일 경고 0.
- `xpe_ai_oom_tests`: `[  PASSED  ] 12 tests.` 카드 스윕 둘: 로드된 모델 85개 할당 지점(rc -2 ×85, rc 0 ×1), 없는 모델 87개 지점(rc -4 ×1, rc -2 ×87), 실패 호출에 남은 할당 0.
- 스텁: `xpe_ai_tests` 402 통과(스킵 109), `xpe_ai_oom_tests` 6 통과 + 건너뜀 6, 경고 0.
- 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄. `check_header_docs.py`: `20 headers, 0 declarations skipped as unparseable, 0 findings`.
- 반증(`m2_arms_out.txt`), 매번 빌드 성공, 소스 바이트 동일 복원, 대조군 둘(변경 없음 → 43 통과 / OOM 12 통과) 초록:
  - **H1** 카드를 옛 상수로 되돌림 → 13건 빨강(버전·null·"stub" 부재·스키마·pccp 등)
  - **H2** 모델 없음이 OK → 9건 빨강
  - **H3** 없는 필드를 "N/A" 로 채움 → 3건 빨강
  - **H4** pccp_status 를 within_boundary → 2건 빨강
  - **H5** 파일이 바뀌어도 카드를 다시 안 만듦 → 2건 빨강
  - **H6** 첫 역할을 바로 반영 → OOM 스윕 2건 빨강
- 스키마 초안(`model-card.schema.json.txt`, `model-sidecar.schema.json.txt`)은 `tests/data/schemas/` 의 파일과 내용이 같다. 리더 몫인 `schemas/` 배치용 `.txt` 초안이다.

## 기준 (Baseline)

`dev/postprocess`, M1 `31357681` 위 195b `0b3373a6` 다음 트리. 위 명령들의 이번 실행 출력.

## 미검증 (Gaps)

- "서명이 안 맞는 모델은 카드가 없다"는 시험(`AModelThatDoesNotVerifyHasNoCard`)이 있고 통제(변조 전엔 카드 있음)를 짝지었지만, 서명 검사를 따로 끈 반증은 없다. 검사는 `ReadVerifiedModelFiles` 안에 있고 실패 시 사이드카 본문을 돌려주지 않으므로 카드 쪽 코드만 바꿔서는 끌 수 없다. 그 함수의 반증은 QA-B-195 M3 의 몫이다.
- 카드 상수가 남아 있지 않다는 확인은 시험 문자열("stub", "2026-04-22", "N/A", "ONNX Runtime")이다. 소스 전체의 같은 문자열 탐색은 하지 않았다.
- 모델이 3개 이상(또는 두 역할이 같은 model_id)인 디렉터리는 다루지 않는다. 같은 id 면 bone 이 먼저 이긴다.
- 문서(`api-spec.md`, `srs_ai.md`, `sdd_ai.md`, `vvp_ai.md` 의 카드 서술, `vvp_ai.md` 의 `ai.cpp:348-357` 앵커)는 리더 소유라 건드리지 않았다. 갱신 필요 여부는 확인하지 않았다.
- `release` 프리셋 전체 빌드는 하지 않았다.

## 잔여 위험

- 스키마가 `tests/data/schemas/` 와 (리더가 놓을) `schemas/` 두 곳에 있으면 갈라질 수 있다. `schemas/` 배치 뒤에는 시험의 경로를 그쪽으로 옮기고 사본을 지우는 편이 안전하다.
- `WorkerSupervisor.AStalledWorkerFailsThatCall…`(5초 정지 시험)가 중간 전체 실행 한 번에 `Ping()` -3 으로 실패했고, 단독 3회와 최종 전체 실행에서는 통과했다. 이번 변경이 건드린 경로가 아니라 부하로 보이지만 원인은 조사하지 않았다.
- 카드 호출은 모듈 잠금을 쥔 채 첫 호출에서 파일을 읽고 서명을 검증한다(큰 모델이면 첫 호출이 느리다). 이후 호출은 읽지 않는다.
