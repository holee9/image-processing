# QA-B-195 M3 — 모델 적재에 서명 검증을 연결 (REQ-AI-007 / REQ-AI-091)

카드: QA-B-195 M3 · 관련: #130 · 설계 승인: `design.md` D1~D4·D6~D8, 사용자 결정 D5(운영 키는 실모델 입고 때, #243)
**이 마일스톤부터 모델 적재의 동작이 바뀐다**: 서명이 맞지 않는 모델은 어떤 경우에도 적재되지 않는다. 운영 신뢰 목록은 비어 있고, 따라서 **운영 빌드는 모든 모델을 거부한다**(의도된 상태, 헤더에 명시).

## 주장 (Claim)

1. **모델을 여는 곳은 `OnnxSession::Create` 하나**이고(뼈 억제 프로세스 안·부위 인식·워커 셋 모두), 검증을 거기에 두었다. `Create` 는 모델·사이드카(`<이름>.json`)·서명(`<이름>.sig`)을 **한 번씩만 읽어** 함께 검증하고(M1 검증기, 역할 포함), **그 바이트로** 세션을 만든다(`Ort::Session(env, 버퍼, 크기, 옵션)`; 경로로 다시 열지 않는다). 검증을 통과하지 못하면 `OnnxErrorCode::kModelNotTrusted`(새 값 7), 아무것도 적재하지 않는다. **스텁 빌드에서도** 같은 검증을 한다(스텁은 적재하지 않지만 거부가 빌드에 따라 달라지면 안 된다).
2. **역할**: `OnnxSessionConfig::role`(기본 `bone_suppress`)이 서명 메시지에 들어간다. 뼈 억제 적재·워커는 `bone_suppress`, 부위 인식 로더는 `bodypart` 를 명시한다. 역할 없는 시험용 모델(`min_scale*`, `not_a_model`)은 기본값으로 서명돼 있다.
3. **사이드카는 검증된 텍스트를 쓴다**: `OnnxSession::VerifiedSidecar()` 가 검증한 사이드카를 돌려주고, 메타데이터와 부위 인식 라벨(`LoadBodyPartLabels(const std::string*, …)`)은 그것에서 읽는다. 파일을 두 번째로 열지 않는다(이전의 `ifstream(경로)` 제거).
4. **신뢰 목록**(`TrustedModelKeys()`): **운영 목록은 비어 있다.** `XPE_AI_TEST_HOOKS` 로 컴파일한 빌드에서만 환경변수 `XPE_AI_TEST_TRUSTED_KEYS`(128자 hex 항목을 쉼표로 구분)의 키를 더한다. 읽을 수 없는 항목(127·129자, hex 아님, 빈 항목)은 **무시**한다 — 키가 더 적어질 뿐 더 많아지지 않는다. 워커도 같은 코드이고 같은 환경변수를 부모에게서 상속한다(워커 타깃에도 같은 `XPE_AI_TEST_HOOKS` 정의).
5. **거부의 겉모습**(M4 가 이유를 보이게 한다): 뼈 억제는 `XPE_ERR_CONFIG_INVALID`(지금의 "읽을 수 없는 모델"과 같은 코드), 부위 인식은 `UNKNOWN` + 세션당 한 번의 "unavailable" Warning + `XPE_ERR_PROCESSING_FAILED`, 워커는 부위 인식에서 실패로 세지 않는다(`model_unavailable`). 이유(어느 부류의 실패인가)는 지금은 로그와 `OnnxResult::message` 에만 있다.
6. 한계와 D5 상태를 헤더(`ai_api.h` "MODEL SIGNING")에 적었다: 신뢰 목록이 비어 있으면 모든 모델을 거부 — 운영 키는 #243 / 키가 DLL·워커 안이라 실행 파일이 바뀌면 무력 / 옛 모델 허용(롤백 방어 없음).

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 464 tests.`(M2 449 → +15), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests` 8건 통과(소스를 같은 정의로 컴파일하는 그 실행 파일도 신뢰 설정을 받는다). 스텁: `xpe_ai_tests` `[  PASSED  ] 380 tests.`(371 → +9, 스킵 83 → 89: 새 시험 중 6건은 모델을 실제로 올려야 해서 스텁에서 건너뜀), `xpe_ai_oom_tests` 6건 + 건너뜀 2. 경고 0. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py` 0 findings, 린트 0.
- 새 시험 15건(`test_ai_model_loading_trust.cpp`):
  - 대조군: 시험용 서명 도우미(아래)가 쓴 것을 검증기가 받아들이고 바이트 하나를 바꾸면 거부 / 서명된 모델은 **자기 역할로만** 적재(부위 인식 모델을 뼈 역할로, 뼈 모델을 부위 역할로 요청하면 거부)
  - **한 바이트 변조 전수**: 뼈 모델 122바이트 전부 + 반으로 잘림·64바이트 덧붙임·길이 0·다른 유효 모델(x3) — 각각 `kModelNotTrusted` 이고 이유는 "서명 불일치". **이전의 프로브(`design.md` §2.2)에서 조용히 통과하던 변조가 전부 거부된다.** 잘림·덧붙임·빈 파일은 이전에는 파서가 `-4` 로 거부하던 것이고 이제는 파서에 가기 전에 서명이 먼저 거부한다(이유가 정확해진다)
  - 서명 파일: 없음 → "no signature file" / **길이 0 의 존재하는 파일 → "malformed"(없음이 아님)** / 한 바이트 짧음 → malformed / 키 id 변조 → "not trusted" / r 변조 → "does not match"
  - 사이드카: 서명 없이 추가됨(사이드카 없이 서명된 뼈 모델) / 라벨 교체(`HAND`) / **같은 텍스트의 CRLF 줄바꿈** / 제거 — 모두 서명 불일치
  - 신뢰 목록: 빈 목록(환경변수 없음)은 서명이 맞아도 모든 모델 거부(이유 "not trusted") / 다른 키만 신뢰하면 거부 / 두 키를 쉼표로 이으면 적재 / 읽을 수 없는 항목 6종은 무시되고 좋은 항목 하나는 살아남음
  - **검증한 바이트를 그대로 쓴다**(검증과 적재 사이에 파일을 바꾸는 훅, `XPE_AI_TEST_HOOKS`): 세션 층에서 x2 모델 파일을 검증 직후 x3 로 바꾸어도 세션은 x2 대로 입력을 2배한다 / 바꾼 파일이 쓰레기여도 적재가 실패하지 않는다 / `VerifiedSidecar()` 는 바뀐 파일이 아니라 검증된 텍스트다. **C ABI 끝에서 끝까지**(DLL 의 훅 전달 함수 `xpe_ai_test_set_after_verify_hook`): `xpe_bone_suppress` 가 실제로 2배한 결과를 돌려주고, `xpe_bodypart_recognize` 가 바꿔치기된 사이드카의 `HAND` 가 아니라 검증된 `CHEST` 를 돌려준다
  - C ABI: 변조된 뼈 모델 → `XPE_ERR_CONFIG_INVALID` 이고 출력 버퍼는 -777 그대로 / 변조된 부위 인식 사이드카 → 프로세스 안·워커 두 경로 모두 `UNKNOWN`(바뀐 라벨이 아님), 4번 불러도(상한 3 넘어) 워커 상태 ACTIVE·실패 0, 서명을 갱신하면 같은 디렉터리가 사이드카의 라벨로 답한다 / 있지만 읽을 수 없는 서명·사이드카(자리에 디렉터리) → "could not run", 건너뛰지 않음
- 반증 12개(`m3_arms_out.txt`), 매번 빌드 성공, 세 소스 바이트 동일 복원, 대조군 15/15: C1 검증 결과 무시 → 10건 빨강 / C2 역할 미반영 / C3 사이드카 미반영 → 각각 7건 빨강(사이드카·역할 시험과 부위 인식 경로) / C4 서명 없음 허용 → 서명 파일 시험만 / **C5 세션을 경로로 만듦(옛 방식) → 세 TOCTOU 시험 빨강** / **C6 사이드카를 검증 뒤에 파일에서 다시 읽음 → 사이드카 TOCTOU 둘 빨강** / C7 읽을 수 없는 서명·사이드카 건너뜀 → 해당 시험만 / C8 빈 서명 파일을 없음으로 → 서명 파일 시험만 / C9 128자 초과 항목 허용, E1 비hex 문자를 0 으로 → 신뢰 항목 시험만 / **E2 부위 인식 라벨을 파일에서 다시 읽음 → 끝에서 끝까지 라벨 TOCTOU 시험만** / E3 부위 인식 로더가 역할을 안 정함 → 부위 인식 경로 시험 둘.
- **배달 빌드 확인**(`delivery_probe_*`): `ai_model_signer.cpp` 를 `XPE_AI_TEST_HOOKS` 없이 컴파일하면(`cl /W4`, 경고 0) 유효한 128자 hex 항목이 환경변수에 있어도 `TrustedModelKeys().size() == 0`, 훅과 함께 컴파일하면 1. 즉 배달 빌드는 환경변수로 키를 들일 수 없다(D4).

### 기존 시험의 변경 (5건, 모두 같은 이유)

임시 디렉터리에 모델을 복사해 다른 사이드카를 쓰거나 모델만 복사하던 시험이 서명 갱신 없이 쓰여, 새 규칙에서 "서명 불일치"로 거부됐다(첫 전체 실행에서 5건 빨강). **우회로를 만들지 않고** 시험 전용 서명 도우미(`test_signing_helper.h`, 시험 키로 CNG 서명)를 써서 서명을 갱신하게 했다: `BodyPartWorkerPath.ARefusedLabel…`·`WorkerBodyPartAgreement.ALabelOutside…`(사이드카를 쓸 때마다 서명 갱신), `BoneSuppressNonFinite.AValidNonFinite…`·`WorkerPathFixture.ASuccessResets…`·`WorkerPathFixture.IntermittentFailures…`(모델과 함께 `.sig` 복사). 그 밖의 시험은 한 건도 바뀌지 않았고 모두 통과한다. 이 도우미의 개인 스칼라는 시험 키의 것(저장소에 공개)이고 `signer_vectors.inc` 에 생성돼 들어간다.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(9e650d6a 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 바뀌는 반환 코드와 호출자

- **서명이 없거나 맞지 않는 모델**: 뼈 억제 `XPE_ERR_CONFIG_INVALID`(이전과 같은 코드, 이전에는 적재 성공), 부위 인식 `UNKNOWN`+`XPE_ERR_PROCESSING_FAILED`. 모든 운영 빌드는 신뢰 목록이 비어 있어 **모든** 모델에 대해 이렇게 답한다 — 지금 실모델이 없으므로 의도된 상태이고 실모델 입고 때(#243) 운영 키를 신뢰 목록에 넣어야 한다.
- 잘린 모델 등 이전에 파서가 `kModelLoadFailed`(→ `-4`)로 거부하던 것은 이제 `kModelNotTrusted`(→ `-4`). C ABI 코드는 같다.
- `OnnxErrorCode::kModelNotTrusted = 7` 추가(`ai_onnx_session.h`), `OnnxSessionConfig::role` 추가, `OnnxSession::VerifiedSidecar()` 추가. 워커의 뼈 억제 가지는 `kModelNotTrusted` 를 `XPE_ERR_CONFIG_INVALID`("model not trusted")로 보낸다 — **워커 뼈 억제 경로에서는 이것이 지금 실패로 세는 것으로 읽힌다**(`ParseWorkerErrorFrame` 가 뼈 억제에서는 `model_unavailable` 을 받지 않는다는 코드 읽기의 결론이고 시험하지 않았다). 설계 D6 은 세지 않는다고 했으므로 M4 에서 바로잡는다.
- GUI 는 이 모듈의 새 코드에 영향받는 호출을 하지 않는다(`xpe_ai_init`/`xpe_bone_suppress` 의 반환 코드는 같다). `clients/` 시험 확인은 M4 에서 같은 검색으로 한다.

## 미검증 (Gaps)

- **배달 빌드 전체는 구성해 보지 않았다**: `release` 프리셋(`XPE_AI_TEST_HOOKS=OFF`)으로 DLL·워커 전체를 빌드·실행하지 않았다. 검증기 한 파일을 훅 없이 컴파일해 목록이 비는 것만 확인했다. 훅 없는 빌드에서 `ai_onnx_session.cpp`·`ai.cpp` 의 훅 가지가 모두 빠져 컴파일되는지는 이 실행에서 확인하지 못했다(`#ifdef XPE_AI_TEST_HOOKS` 로 감싼 곳이 이 파일들뿐이라는 것은 코드로 확인).
- **메모리 최대치**: 이제 모델 바이트가 메모리에 한 번 올라온 뒤 ONNX Runtime 이 파싱한다. 장난감 모델(수십~수백 바이트)로만 돌렸고, 큰 모델에서 정점이 얼마나 오르는지는 재지 않았다. 세션이 만들어지면 버퍼를 비운다.
- **상한 1 GiB**(D8)의 읽기 쪽(`kTooLarge`)은 시험하지 않았다(거대한 파일을 만들어야 한다). 검증기 쪽 상한은 M1 에서 시험했다.
- **실패를 세는 방식**(워커 뼈 억제 경로의 계수), 이유가 운영자에게 보이는지, 실패 기억(같은 거부를 매 호출 다시 해시하지 않기)은 M4 의 일이다. 지금은 부위 인식이 매 호출 다시 적재를 시도하고 매번 해시한다(작은 모델에서는 눈에 안 띈다).
- **ONNX Runtime 이 메모리 버퍼를 세션 수명 동안 참조하는지**는 문서로 확인하지 않았다. 시험으로는 버퍼를 비운 뒤에도 세션이 올바른 결과를 낸다(TOCTOU 시험이 `Run` 의 결과를 확인한다).
- 모델 한 개의 `.sig` 와 `.json` 이 같은 디렉터리의 다른 모델(`bodypart`·`bone_suppress` 둘 다 있는 디렉터리)과 섞이는 경우는 이름(`<이름>.sig`)이 달라 섞이지 않는다고 설계했으나 한 디렉터리에 두 모델을 둔 시험은 만들지 않았다.
- 시험 키 개인 스칼라가 `signer_vectors.inc` 에 들어갔다(저장소에 이미 PEM 이 있으므로 새로 노출되는 것은 없다).

## 잔여 위험

- 운영 신뢰 목록이 비어 있는 동안 운영 빌드는 모든 모델을 거부한다. 실모델 입고(#243) 전에 운영 키가 정해지고 목록에 들어가야 한다. 이 상태를 잊고 실모델을 넣으면 AI 가 조용히(알림 없이, M4 전에는) 꺼진다.
- 키가 DLL·워커 안에 있다는 한계(헤더 참조).
- 알림 문구가 아직 없다: M4 가 문구와 그 계약을 정한다.
