# QA-B-195 M2 — 서명 도구, 시험 자산 19개 서명, 낡은 서명 시험 (REQ-AI-007 / REQ-AI-091)

카드: QA-B-195 M2 · 관련: #130 · 설계 승인: `design.md` §6 (리더 2026-10-03)
이 마일스톤도 **로딩 코드에 연결하지 않는다**(M3). 제품 동작은 바뀌지 않았다. 운영 키는 리더가 사용자에게 받은 결정대로 실모델 입고 때(#243) 정해지고, 이 마일스톤의 서명은 모두 **시험 키**(`tests/data/signing/test_key_1.pem`)로 만든다.

## 주장 (Claim)

1. **서명 도구**: `tools/ai/xpe_model_signing.py` 에 명령줄을 더했다.
   - `sign --key K.pem --role R --model M.onnx [--sidecar S.json] --out M.sig` — 한 모델 서명(범용; 어떤 키로든)
   - `sign-test-assets` — `tests/data` 의 모든 `.onnx` 를 시험 키로 서명
   - `check-test-assets` — 누락·낡음·고아 서명을 찾아 종료 코드 1
   서명은 결정적(RFC 6979)이라 다시 돌려도 같은 바이트다(두 번 돌려 `.sig` 19개 전체의 해시 `f7c053be…` 동일).
2. **시험 자산 19개를 모두 서명했다**: 뼈 억제 3(`models_x2`·`models_x3`·`models_broken`), 부위 인식 13 디렉터리, 루트 3(`min_scale2`·`min_scale3`·`not_a_model`). 규칙(도구와 C++ 시험이 같은 규칙을 일부러 두 번 적는다): 역할 = 파일 이름이 `bone_suppress`/`bodypart` 이면 그것, 아니면(역할이 없는 시험용 모델) `bone_suppress`; 사이드카 = 모델 옆의 `<이름>.json`(없다는 사실도 서명에 들어간다); 서명 = `<이름>.sig`.
   - **일부러 깨진 모델도 서명했다**(`models_broken`, `models_bodypart_broken`, `not_a_model`): 서명하지 않으면 M3 이후 지금의 "읽을 수 없음"을 보는 시험이 "서명 없음"을 보게 되어 시험의 의도가 바뀐다. 사이드카가 없는 `models_bodypart_no_labels` 는 사이드카 없음으로 서명했다.
3. **생성 스크립트가 서명을 갱신한다**: `make_min_models.py`·`make_bodypart_models.py` 가 끝에서 `sign_test_assets()` 를 부른다(모델을 다시 만들면 서명도 새로 나온다). 두 스크립트를 다시 돌려 모델·사이드카 바이트가 변하지 않음(`git status` 에 모델 변경 없음)과 서명이 새로 쓰임을 확인했다.
4. **낡은 서명 시험(C++)** `test_ai_model_assets_signed.cpp`(Python 에 기대지 않는다): `tests/data` 를 훑어 모든 `.onnx` 가 `.sig` 를 갖고 시험 키로 검증되는지, 모델 없이 남은 `.sig` 가 없는지 본다. 대조군 세 가지를 같이 둔다: 훑기가 자산을 19개 이상 찾았는가 / 서명 파일이 19개 이상인가 / **검사가 실패할 수 있는가**(모델 바이트 변경·역할 변경·사이드카 제거·다른 모델의 서명이 각각 `kBadSignature`).
5. **줄바꿈 위험을 막았다**(발견): 서명은 사이드카의 **정확한 바이트**를 덮는데 `.gitattributes` 에는 `*.csv` 규칙뿐이었다. 체크아웃이 사이드카의 줄바꿈을 CRLF 로 바꾸면 모든 서명이 낡게 된다. 이 경로의 `*.json` 은 `text eol=lf`(csv 선례와 같은 방식), `*.onnx`·`*.sig` 는 `binary` 로 못박았다. `git check-attr` 로 적용을 확인했다. 현재 사이드카 12개에 CR 이 없음을 바이트로 확인했다.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 449 tests.`(M1 446 → +3), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests` 8건 통과. 스텁: `xpe_ai_tests` `[  PASSED  ] 371 tests.`(368 → +3, 스킵 83), `xpe_ai_oom_tests` 6건 + 건너뜀 2. 경고 0.
- 로컬 `check-test-assets`: `19 test models checked, 0 problem(s)`. 훼손 확인(손으로): 모델 한 바이트 변경 → `PROBLEM: stale or invalid signature: models_x2\bone_suppress.sig`; 고아 서명 → `PROBLEM: orphan signature (no model beside it)`; 복원 후 0건. (종료 코드 1 은 이 실행에서 직접 읽지 않았다: 명령 끝의 `tail` 이 코드를 가렸다. 문구와 개수로만 확인.)
- 반증 10개(`m2_arms_out.txt`; 이번에는 소스가 아니라 **데이터 자산을 훼손**하고 재빌드 없이 시험을 돌렸다. 자산은 매번 복원, 전체 자산의 해시가 전후로 같음 `3f662e36…`, 대조군 3/3):
  - D1 모델 한 바이트 → 서명 시험 빨강 / D2 `.sig` 삭제 → 서명 시험 + 고아 시험(`.sig` 수가 19 미만) 빨강 / D3 사이드카 한 바이트 → 서명 시험 + "검사가 실패할 수 있는가" 빨강(그 시험의 대조군이 같은 자산을 쓴다) / D4 두 모델의 서명 교환 → 같음 / D5 모델 없는 서명(`ghost.sig`) → 고아 시험만 빨강 / **D6 사이드카를 CRLF 로**(체크아웃 변환) → 서명 시험 빨강 / D7 신뢰하지 않는 키(두 번째 시험 키)로 서명 → 서명 시험 빨강 / D8 사이드카 삭제 → 서명 시험 빨강 / D9 서명 파일 한 바이트 짧음 → 서명 시험 빨강 / D10 다른 역할로 서명 → 서명 시험 빨강.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(f426a0c8 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 미검증 (Gaps)

- **CI 러너에서의 줄바꿈**: 로컬은 `core.autocrlf` 가 설정돼 있지 않고 사이드카가 LF 다. `.gitattributes` 의 `eol=lf` 가 러너에서도 LF 를 보장하는지는 CI 에서 `ModelAssets.*` 가 도는 것을 보기 전에는 모른다(이 변경은 푸시 전이다).
- **서명 도구의 `sign` 명령(범용)은 시험하지 않았다**: `sign-test-assets` 가 쓰는 함수 `sign()` 은 벡터·자산 시험이 검증하지만 `sign` 하위 명령의 인자 처리(키 경로·출력 경로)는 실행해 보지 않았다. 운영 서명 절차는 D5(#243)가 정해질 때 만든다.
- 시험 키 개인키가 저장소에 있다. 그래서 운영 신뢰 목록에 절대 넣지 않는다는 규칙은 아직 코드가 아니라 설계(D4: 시험 키는 `XPE_AI_TEST_HOOKS` 아래 주입 함수로만)에 있다 — M3 에서 구현·시험한다.
- `check-test-assets`(Python)와 `ModelAssets.*`(C++)는 같은 규칙을 두 번 구현했다. 어느 한쪽만 규칙을 바꾸면 갈라진다(예: 역할 기본값). 갈라짐을 잡는 시험은 없다.
- 이번 마일스톤은 **제품 코드를 바꾸지 않았다**. 모델 로딩은 여전히 서명을 보지 않고, 서명된 자산이 실제로 로딩 경로를 통과하는지는 M3 의 일이다.

## 잔여 위험

- 시험 자산을 바꾸는 사람이 서명을 갱신하지 않으면 `ModelAssets.*` 가 빨개진다(의도). 갱신 명령은 시험 실패 문구에 적혀 있다.
- 모델 파일은 바이너리로 커밋되므로 서명이 낡았는지는 diff 로 보이지 않고 시험으로만 보인다.
