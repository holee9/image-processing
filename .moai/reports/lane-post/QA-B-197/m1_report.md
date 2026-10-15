# QA-B-197 M1 — T-005a: 모델 사이드카가 REQ-AI-008 이 요구하는 것을 말하지 않으면 모델을 거부한다

카드: QA-B-197 M1 · 관련: #130 · 리더 결정(2026-10-03): D3 필수 필드 누락은 로딩 시 거부(시험 자산 재서명 허용, 시험 키), 운영 키는 #243

## 주장 (Claim)

1. **필수 필드는 REQ-AI-008 원문에서 정했다.** 원문(`spec.md`): "Model versioning shall follow semver; model metadata shall include: model_id, version, pccp_scope, training_data_hash, validation_metrics." → 필수 다섯 필드가 정확히 그것이고 `version` 은 semver 다.

   | 필드 | 규칙 |
   |---|---|
   | `model_id` | 문자열, 영문·숫자·`.`·`_`·`-` 로 1~64자 (`xpe_ai_get_model_card` 의 식별자 규칙과 같음) |
   | `version` | 문자열, Semantic Versioning 2.0.0 (`MAJOR.MINOR.PATCH[-pre][+build]`, 앞자리 0 금지, 빈 식별자 금지) |
   | `pccp_scope` | 비어 있지 않은 문자열 |
   | `training_data_hash` | 비어 있지 않은 문자열 |
   | `validation_metrics` | 비어 있지 않은 JSON 객체 |

   선택(REQ-AI-010 의 입력, M2 의 카드가 그대로 쓴다): `intended_use`·`training_data_summary`·`limitations`(문자열), `demographic_performance`(객체), `published_date`(달력 날짜 `YYYY-MM-DD`). 이름이 있는 키의 형식이 틀리면 거부. 이름이 없는 키(`labels`, `note`)는 이 검사가 판단하지 않는다. **최상위 키가 두 번 나오면 거부**한다(서명된 텍스트가 두 가지를 말하게 된다). 사이드카는 1 MiB 이하(구현 안전 상한, 요구 아님).
2. **검증은 서명 검증 뒤, 검증된 바이트에서만 한다.** `OnnxSession::Create` 가 `ReadVerifiedModelFiles`(읽기·서명 검증을 한 곳으로 뺀 공유 함수, M2 의 카드도 이것을 쓴다) 다음에 `ParseModelSidecar` 를 부른다. 서명이 안 맞는 모델의 사이드카는 파서가 보지 않는다(시험으로 확인: 사이드카가 쓰레기이고 서명도 낡았을 때 거부 이유는 서명이다). 모든 모델 적재 경로(프로세스 안 뼈 억제·부위 인식·워커)가 `Create` 를 지나므로 같은 규칙이 적용된다. 사이드카가 **없는** 모델도 거부한다.
3. **거부의 겉모습은 서명 거부와 같다**: 뼈 억제 -4(`XPE_ERR_CONFIG_INVALID`, 출력 미기록), 부위 인식 `UNKNOWN` + `XPE_ERR_PROCESSING_FAILED`, 워커에서는 `-4` + `model_unavailable`(실패로 세지 않음, 워커는 `ACTIVE` 유지), 파일이 안 바뀌면 다시 읽지 않고(M4 의 도장) 바뀌면 같은 세션에서 바로 다시 검사. 역할마다 세션당 한 번 `XPE_ALERT_ERROR`(서명 거부와 같은 한 번 플래그를 공유):
   - 프로세스 안: `AI [bone suppression|body-part recognition] is unavailable: its model sidecar failed the metadata check ([reason]) and nothing was loaded (REQ-AI-008)` — `[reason]` 은 필드 이름이 든 문장(예: `the required field pccp_scope is missing`).
   - 워커가 거부했을 때(어느 검사인지 프레임에서 알 수 없음): `AI bone suppression is unavailable: the AI worker refused its model (the signature check or the sidecar check failed, see the worker log) and nothing was loaded (REQ-AI-007, REQ-AI-008, REQ-AI-091)`. M4 때의 문구("failed signature verification (refused by the AI worker…)")는 사이드카 거부에는 틀린 말이라 이 문구로 바꿨다.
   두 문구는 `ai_api.h` 에 레인 간 계약으로 적었다.
4. **사이드카는 JSON 문서(DOM)를 만들지 않고 읽는다(nlohmann SAX).** 처음에는 DOM 으로 파싱했고, 그러자 `xpe_ai_oom_tests` 가 콜드 뼈 억제 스윕 중 비정상 종료(종료 코드 3)했다. 뼈 억제 픽스처에 사이드카가 생기자 스윕이 비어 있지 않은 DOM 의 소멸자(noexcept 안의 할당, QA-B-194 M5 에서 문서화한 한계)에 닿은 것으로 읽는다(스택을 확인하지는 않았다). SAX 핸들러로 바꾸자 같은 스윕이 통과했다(`[  PASSED  ] 10 tests.`). 중첩 값(`validation_metrics`, `demographic_performance`)은 파서 이벤트로 다시 만든 압축 텍스트이고 숫자는 원문 리터럴 그대로다. 부수 효과: 중복 키 검출이 가능해졌다. **부위 인식의 `labels` 읽기(`LoadBodyPartLabels`)는 여전히 DOM** 이고 그 콜드 경로는 스윕으로 증명되지 않는다(헤더의 (1), 변화 없음).
5. **시험 자산**: 이 모듈의 시험 모델 20개(19 + 새 `models_card_full`)에 REQ-AI-008 다섯 필드가 있는 사이드카를 갖추고 시험 키로 재서명했다. `tests/data/model_sidecar_fields.py` 가 값을 만들고 두 생성 스크립트(`make_min_models.py`, `make_bodypart_models.py`)가 쓴다. **`.onnx` 파일은 하나도 바뀌지 않았다**(`git diff -- '*.onnx'` 빈 출력). 값은 장난감 모델의 것임을 말한다(`pccp_scope: "none: toy model for wiring tests"` 등) — 실제 모델에 대한 주장이 아니다.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 496 tests.`(M4 이후 474 → +22: `ModelSidecarParser` 11, `ModelSidecarLoading` 6, `ModelSidecarRefusal` 5, 기존 474건은 한 건도 사라지지 않음), 스킵 5, 실패 0, 컴파일 경고 0. `xpe_ai_oom_tests` 10건 통과. 스텁: `xpe_ai_tests` 398 통과(스킵 103), `xpe_ai_oom_tests` 6 통과 + 건너뜀 4, 경고 0. 로컬 doxygen 1.12.0 `exit=0`, 경고 0줄, `check_header_docs.py` 0 findings. (doxygen 은 처음에 `ReadVerifiedModelFiles` 의 인자·반환값 문서 누락 3건을 냈고 고쳤다.)
- 새 시험(`test_ai_model_sidecar.cpp`): 파서 — 필수 다섯 필드 각각 부재/형식/빈 값 → 필드 이름이 든 이유, semver 표(통과 11, 거부 25), 달력 날짜 표, JSON 아님·배열·스칼라·너무 큼·없음, 중첩 값 보존(쉼표·이스케이프·숫자 리터럴 `2.50`/`1e3`·빈 객체/배열·비ASCII), 중복 키(최상위만, 중첩된 같은 이름은 허용), 잘리거나 깨진 텍스트 12종과 20만 단계 중첩에서 예외 없이 거부, 이름 없는 키 무시. 적재 — 대조군(복사본이 로드되고 `GetModelMetadata` 가 사이드카 값을 돌려줌), 필수 필드 하나씩 빼고 **현재 서명으로 재서명**한 모델이 `kSidecarInvalid` + 필드 이름, 사이드카 없는 모델, 나쁜 semver·깨진 JSON·배열·잘못된 형식·나쁜 날짜, 서명이 낡은 상태의 쓰레기 사이드카는 `kModelNotTrusted`(파싱 전 거부), `tests/data` 의 모든 `.json` 이 파서를 통과(20개 이상). C ABI — 뼈 억제 -4·출력 -777 그대로·세션당 알림 1건(Error, 역할, `sidecar failed the metadata check`, 필드 이름, `REQ-AI-008`), 사이드카를 고치면 같은 세션에서 바로 성공(×2), 워커 5번 호출에도 `ACTIVE`·실패 0·알림 1건, 부위 인식 프로세스 안 `UNKNOWN` + 알림 1건, 다른 이유로 다시 거부되어도(서명 → 사이드카 → 다른 필드) 역할당 알림은 1건.
- 반증 13개(`m1_arms_out.txt`), 매번 빌드 성공, 다섯 소스 바이트 동일 복원, 대조군 22/22: H1 `Create` 가 사이드카를 안 봄(9건 빨강) / H2 `pccp_scope` 부재 허용 / H3 semver 검사 없음 / H4 앞자리 0 허용 / H5 달력 검사 없음 / H6 빈 `validation_metrics` 허용 / H7 선택 필드 형식 오류 허용 / H8 프로세스 안 뼈 억제가 거부를 못 알아봄(-3) / H9 워커가 `model_unavailable` 을 안 보냄 / H10 부위 인식 로더가 거부를 "읽을 수 없는 모델"로 보고 / H11 알림이 매 호출 / H12 중복 키 허용 / H13 중첩 값의 쉼표 누락 — 각각 해당 시험만 빨강. H7 의 첫 변이는 컴파일되지 않아(미사용 인자) 그 결과가 앞 arm 의 것으로 읽혔기에 변이를 고쳐 다시 돌렸다. H11 은 처음에는 아무 시험도 빨갛게 만들지 못했다(재검사 기억이 같은 거부의 중복 호출을 가리기 때문): 그래서 "파일이 바뀌어 다른 이유로 다시 거부될 때 알림이 한 번뿐"을 단언하는 시험을 더했고 그 뒤 빨강이 됐다.

### 기존 시험의 변경 (11건 실패 → 수정, 이유는 모두 같은 규칙)

사이드카가 필수가 되자 첫 전체 실행에서 11건이 빨갛게 떴다. 원인은 세 가지다. ① 모델과 서명만 복사하던 도우미(`CopyModelWithSignature`/`RemoveModelDir`)가 사이드카를 안 옮겼다 → 사이드카도 옮기도록 고쳤다(`BoneSuppressNonFinite`, `WorkerPathFixture` 2건). ② 시험이 `{"labels": [...]}` 만 쓴 사이드카를 만들어 서명하던 곳 → 새 도우미 `xpe_test::WithMetadata` 가 다섯 필드를 앞에 붙인다(`test_worker_bodypart`, `test_bodypart_worker_path`, `ModelLoadingTrust` 의 서명 갱신 3곳). ③ 사이드카가 없던 `models_bodypart_no_labels` 픽스처 → 이제 사이드카가 있고 `labels` 키만 없다(없는 사이드카는 사이드카 검사가 먼저 거부하므로 라벨 실패를 고정하는 유일한 방법). 그 시험의 기대 이유도 `not found` → `no non-empty labels array` 로 바꿨고 이유를 주석에 적었다. 그 밖에 서명 도우미 시험은 사이드카 한 바이트를 뒤집던 것을 `0.0.1` → `0.0.2` 로 바꿨고(뒤집힌 바이트는 JSON 을 깨서 재서명 후에도 거부된다), 읽을 수 없는 사이드카 시험은 복사된 사이드카를 먼저 지운 뒤 디렉터리를 만든다. 서명 갱신 없이 사이드카만 바꾸는 시험들(서명이 낡은 상태를 단언)은 그대로이고 모두 통과한다.

## 기준 (Baseline)

같은 트리 `dev/postprocess`(7304a6f0 위, 커밋 `be746353` 포함), `cmake --build --preset ci-ai`·`ci-post`, 이 실행의 출력.

## 미검증 (Gaps)

- **중복 키는 최상위만** 검출한다. `validation_metrics` 안의 같은 이름은 허용하며 그 안의 중복은 보지 않는다.
- **`LoadBodyPartLabels` 는 DOM** 이다: 콜드 부위 인식 경로를 할당 단위 스윕으로 증명하지 못한다는 기존 한계(헤더 (1))는 그대로다. `ReadVerifiedModelFiles` 와 사이드카 파서는 스윕이 닿는 경로에 있으나, 부위 인식 콜드 경로는 라벨 읽기에서 닿지 못한다.
- `pccp_scope`·`training_data_hash` 는 "비어 있지 않은 문자열"만 본다. 값의 어휘(허용 범위 이름, 해시 형식)는 요구에 없어 정하지 않았다.
- **실제 모델의 사이드카는 없다.** 시험 자산의 값은 장난감 모델의 것이다. 실모델 입고(#243) 때 실제 값이 이 규칙을 통과하는지는 그때 확인해야 한다.
- 스키마 파일(`model-sidecar.schema.json`)은 아직 만들지 않았다. M2 에서 카드 스키마와 함께 `.txt` 초안으로 낸다.
- 서명 파일·사이드카 한 바이트 변조 전수 같은 M3 형식의 전수 시험을 사이드카 쪽에는 하지 않았다(서명이 사이드카 바이트를 덮는 것은 M1~M3 에서 시험됨).
- `release` 프리셋 전체 빌드는 하지 않았다.

## 잔여 위험

- 사이드카 값을 바꾸면 그 모델의 `.sig` 를 다시 만들어야 한다(카드의 `published_date` 정정도 재서명). 의도된 성질이다.
- 필수 필드 강제로 사이드카가 없던 모든 모델은 거부된다. 운영 신뢰 목록이 비어 있어 어차피 모든 모델이 거부되는 현재 상태에서는 보이지 않고, 운영 키가 들어올 때 사이드카도 함께 있어야 한다(#243 에 같이 적을 일).
