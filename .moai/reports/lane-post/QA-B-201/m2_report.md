# QA-B-201 M2 — 모델카드·사이드카 시험이 루트 `schemas/` 를 읽는다 (#130)

카드: `.moai/lanes/post/inbox/QA-B-201.md` M2. 리더가 main 루트에 넣은 `schemas/model-card.schema.json`·`schemas/model-sidecar.schema.json`(`f557390c`)을 시험이 읽게 하고, `modules/ai/tests/data/schemas/` 의 사본은 지웠다.

## 0. 결과

| 항목 | 내용 |
|---|---|
| 바뀐 것 | `modules/ai/CMakeLists.txt`: `XPE_REPO_SCHEMAS_DIR`(`${CMAKE_CURRENT_SOURCE_DIR}/../../schemas`)를 `xpe_ai_tests` 에 컴파일 정의로 추가(기존 `XPE_AI_TEST_DATA_DIR` 와 같은 방식: 컴파일 시점에 박아 cwd 에 의존하지 않음). `test_ai_model_card.cpp`: 스키마 세 곳이 `kSchemas`(루트)를 읽는다. 사본 2개 삭제(`git rm`). |
| 같이 고친 것 | (1) 사이드카 시험의 `parent == "schemas"` 건너뛰기를 제거 — 그 디렉터리가 사라져 아무것도 막지 않는 가드가 됐다. (2) 스키마를 읽지 못하면 "not found under `<경로>`" 를 보이도록 단언 메시지를 세 곳 모두에 달았다(두 번째 카드 시험에는 단언 자체가 없었다). (3) 사본 경로를 가리키던 주석 3곳(CMake, `json_schema_mini.h`, 시험 머리말)을 루트로. |
| 시험 | 스키마 시험 23개 통과(`AiModelCardTest.*`, `ModelCardSchema.*`, `ModelSidecarSchema.*`). ci-post ctest 전체 1360개 통과·0 실패(루트 `schemas/` 가 작업 트리에 있을 때). |
| 반증 | 4팔 모두 의도한 시험이 빨개졌고 루트 파일은 바이트 단위로 복원(`m2_arms_out.txt`). |

## 1. 사본과 루트가 같았는가
main 의 `schemas/*.json`(`git show FETCH_HEAD:…`)과 지운 사본을 바이트 비교했다: 두 파일 모두 동일(`cmp` 일치, 경로를 바꾸는 것 말고 스키마 내용은 바뀌지 않았다). 시험이 루트를 읽게 된 뒤 같은 시험이 통과한다.

## 2. 지운 경로를 인용하는 문서 (`git grep "tests/data/schemas"`)
- 소스·시험의 인용 3곳(CMake 주석, `json_schema_mini.h`, `test_ai_model_card.cpp` 머리말)은 이 커밋에서 루트로 고쳤다.
- 남은 인용은 **내 과거 보고서** `.moai/reports/lane-post/QA-B-197/m2_report.md` 3곳뿐이다(10·31·47행). 그것은 QA-B-197 시점의 기록이라 고치지 않았다 — 47행은 바로 이 상황("두 곳에 있으면 갈라질 수 있다, `schemas/` 배치 뒤에는 시험이 루트를 읽게")을 예고한 문장이다.
- `docs/`, `.moai/specs/`, `modules/ai/include` 의 `schemas/model-card.schema.json` 인용(SRS `REQ-AI-011`, `srs_ai.md`, `ai_api.h:667`)은 **루트 경로**를 가리키고 있었으므로 이제 사실과 맞는다.

## 3. 반증 (`m2_arms_driver.py.txt`, `m2_arms_out.txt`)
시험 소스는 건드리지 않고 **루트 파일**을 바꾼다. 경로는 컴파일 때 박히고 내용은 실행 때 읽으므로 재빌드 없이 팔마다 시험 실행만 한다.

| 팔 | 빨개진 시험 | 메시지 |
|---|---|---|
| 대조군: 루트 파일 그대로 | 없음 | |
| 루트 `model-card.schema.json` 삭제 | `EveryCardAndEveryUnavailableAnswerConformsToTheSchema`, `ControlTheSchemaRefusesWhatItShould` | `schemas/model-card.schema.json not found under D:/workspace-github/xpe-post/schemas` |
| 루트 `model-sidecar.schema.json` 삭제 | `EveryShippedFixtureSidecarConformsAndTheSchemaRefuses…` | `schemas/model-sidecar.schema.json not found under …/schemas` |
| 루트 카드 스키마를 "아무 객체나 허용"으로 교체 | `ControlTheSchemaRefusesWhatItShould` | |
| 루트 사이드카 스키마를 "아무 객체나 허용"으로 교체 | `EveryShippedFixtureSidecarConformsAnd…` | |

사본을 지웠으므로 "루트가 아니라 남은 사본을 읽고 있다"는 경우는 구조적으로 없고, 위 4팔은 시험이 실제로 루트 파일의 내용에 반응한다는 것을 보인다.

## 4. 이 커밋에 필요한 전제 — 리더 확인 요청
**내 브랜치(`dev/postprocess`)에는 아직 루트 `schemas/` 가 없다**(main 에만 있다, `f557390c`). 따라서 이 커밋만으로는 내 브랜치에서 위 시험 3개가 "not found" 로 빨개진다. main 병합(리더 결정) 뒤에 해소된다. 검증 때는 main 의 두 파일을 작업 트리에 **추적하지 않는 상태로 임시로** 놓고 돌렸고, 커밋에는 넣지 않았다(리더 소유 경로). 병합 충돌을 피하려고 커밋 뒤 지운다.

## 5. Gap / 잔여 위험
Gap
- ci-post 이외의 구성에서 `xpe_ai_tests` 가 빌드되는지(예: 모듈 단독 빌드, `CMAKE_SOURCE_DIR` 이 모듈 폴더인 경우)는 돌리지 않았다. 경로는 `${CMAKE_CURRENT_SOURCE_DIR}/../../schemas` 라 저장소 체크아웃 안에서는 같은 위치를 가리킨다.
- 루트 스키마의 `$id`(`model-card.schema.json`)는 그대로이고 어떤 참조 해석에도 쓰이지 않는다(`json_schema_mini.h` 는 `$ref` 를 지원하지 않음) — 확인은 코드 읽기.

잔여 위험
- 루트 `schemas/` 가 없는 체크아웃(내 브랜치 단독)에서는 시험이 빨개진다 — 의도한 실패이고 메시지가 원인을 말한다.
