# QA-A-208b — Codex #40 보류: 손으로 쓴 부분 JSON 파서를 검증된 파서(nlohmann-json)로 교체

기준 커밋 `5d003c4f`(QA-A-202e) 위. 감사 원문은 `evidence/00_codex40_audit_original.md`. 증거는 `evidence/` (번호 순). 이슈 #233, #234.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | 품질 필드의 조회 `xpe_json_top_level_scalar` 는 이제 설정 텍스트를 **nlohmann-json 의 SAX 파서**(`strict`, 주석 없음)로 읽는다. 텍스트가 유효한 JSON 객체 하나여야 하고, 키는 **이스케이프를 해석한 뒤** 비교되며(`"fit_r_squared"` = `fit_r_squared`), 깊이 1 의 같은 키가 두 번이면 `Duplicate`. 반환 의미(없음/스칼라/비스칼라/중복/깨짐)와 호출부는 그대로다. | 빨강 `02_red.txt`, 초록 `04_dll_full.txt`, 반증 `arm_*` |
| 2 | Codex 의 재현 전부가 `XPE_ERR_CONFIG_INVALID` 이고 게인 맵·품질 기록이 호출 전 그대로다: 이스케이프 키(불량 값 / 중복 3종), 깨진 중첩 `{bad}`, 괄호 종류 불일치 `{]`, 뒤 텍스트 `… garbage`, `{} garbage`, 금지된 escape `\q`, 문자열 속 제어 문자, 끝 쉼표, 작은따옴표, bare `+7`, 콜론 누락, 닫히지 않은 객체, 최상위가 배열/문자열. | 같음 |
| 3 | 파서 호출은 예외 없는 경로(SAX, 오류는 콜백이 `false` 를 돌려 알린다)이고, 할당 실패(`std::bad_alloc`)는 기존 가드가 받는다: 할당 실패 스윕이 품질 설정이 있는 게인 적재에서 **12 → 91 개의 할당 지점**을 전부 통과했다(예외가 밖으로 나간 경우 없음, 상태 불변). | `06_oom_sweep_before_after.txt` |
| 4 | 새 의존성이 아니다: 같은 라이브러리·같은 버전을 이미 `common`(소스 1개), `ai`(2), `dicom`(4), `enhance_advanced`(3) 가 소스에서 쓴다(`grep` 으로 센 파일 수; 같은 검색에서 `enhance_basic`·`display`·`gsvg` 는 0). | 6절 |

## 2. 무엇을 바꿨나

- **`helpers.cpp`**: 손으로 쓴 `json_skip_*` 와 멤버 단위 걷기를 지우고 `TopLevelProbe`(`nlohmann::json::json_sax_t` 구현, 익명 네임스페이스)로 바꿨다. 깊이를 세어 깊이 1 의 키만 비교하고, 첫 일치 멤버의 값 종류(스칼라 텍스트 / 중첩)를 기록한다. 최상위가 객체가 아니면(배열·스칼라) 콜백이 `false` 로 멈춘다. 빈 텍스트·공백뿐인 텍스트는 파싱하지 않고 "없음"(키 없는 설정)으로 둔다 — 전과 같다. 숫자는 float 이면 SAX 가 주는 **원문 토큰**, 정수는 십진 문자열로 되돌려 엄격 변환기에 넘긴다.
- **`CMakeLists.txt`**: `xpe_preprocess`·`xpe_preprocess_oom_tests` 에 `nlohmann_json::nlohmann_json` 를 PRIVATE 로 **명시**했다. 이미 `xpe_common` 의 PUBLIC 링크로 닿아 있었으므로(헤더 전용, 바이너리 영향 없음) 사고가 아니라 선언된 의존성이 되게 한 것이다.
- **내부 헤더**: `xpe_json_top_level_scalar` 의 문서를 새 구현에 맞췄다. `xpe_json_find_scalar` / `xpe_json_get_string`(파이프라인 설정 쪽)은 **바꾸지 않았다** — QA-A-209 의 몫이다.
- **`extern "C"`**: `helpers.cpp` 에는 `extern "C"` 가 하나도 없고 새 도우미는 익명 네임스페이스이며 선언은 내부 헤더의 C 구간 밖이다(C++ 연결).
- **`preprocess_api.h`**: 방어 검사 실패의 계약을 좁혀 적었다(7절).

## 3. 동작 변화 (호환성 메모)

설정은 이제 **유효한 JSON** 이어야 한다. 손 파서가 받아들이던 것 중 거부되는 것: bare `+7`·`1.` 같은 JSON 이 아닌 숫자 표기, 끝 쉼표, 작은따옴표, 금지된 escape, 문자열 속 제어 문자, 잘못된 UTF-8, 객체 뒤의 텍스트, 괄호 짝이 맞지 않는 중첩. 따옴표 안의 값(`" +0.95"`, `" 3"` 등)은 문자열이므로 전과 같이 엄격 변환기가 판정한다. 제품 생성기는 `%d`/`%.9f` 로 유효한 JSON 객체를 쓰고(202e 에서 확인), 이번 전체 시험(생성기 산출물 포함)에서 새로 거부된 파일은 없다. 현장 구형 XCal 은 저장소 밖이라 점검하지 못했다.

## 4. 시험

`AQualityConfigMustBeOneValidJsonObjectAndItsKeysAreComparedAfterTheEscapesAreRead` — 품질 네 필드 각각 × 22 행(`@` 는 필드명, `#` 는 첫 밑줄을 `_` 로 쓴 같은 이름):

- 이스케이프 키: 불량 값 → 거부 / 유효 값 → 읽힘 / `@`+`#`, `#`+`@`, `#`+`#` 중복 → 거부 (Codex 발견 1)
- 깨진 JSON: 깨진 중첩 후 유효 키, 괄호 종류 불일치, `… garbage`, `{} garbage`, `\q`, 문자열 속 개행, 끝 쉼표, 작은따옴표, bare `+7`, 콜론 누락, 닫히지 않음, 최상위 배열, 최상위 문자열 → 거부 (발견 2)
- 계속 통과해야 하는 유효 입력: `{}`(없음), 공백·서로게이트 쌍·`\" \\ \/ \b\f\n\r\t` 가 섞인 정상 객체, 문자열 끝의 `\"키`, 중첩된 같은 이름의 키
- 거부 행은 모두 `XPE_ERR_CONFIG_INVALID` + 게인 맵·메타데이터 불변, 읽힘 행은 `valid=1` 과 값 7.

기존 `AQualityFieldIsTakenFromTheTopLevelOfTheConfigOnly`(11 행 × 4 필드)는 **고치지 않고** 그대로 통과한다.

## 5. 수정 전 / 후, 반증

수정 전: 새 시험이 11 종류의 행 × 4 필드에서 272 개의 단언 실패(`02_red.txt`). 반증(한 번에 하나, 전체 빌드 후 `ConfigStrictParse.*` 실행, 복원 뒤 `cmp` 동일):

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| hand_parser_back | `helpers.cpp` 를 HEAD 의 손 파서 본으로 되돌림 | 새 시험 |
| dup_not_checked | 최상위 중복 검사를 뺌 | 새 시험, `AQualityFieldIsTakenFromTheTopLevel…` |
| trailing_garbage_ok | `strict=false` (뒤 텍스트 허용) | 새 시험 |
| nested_keys_counted | 깊이 1 이 아닌 키도 셈 | 새 시험, `AQualityFieldIsTakenFromTheTopLevel…` |

## 6. 새 의존성이 아니라는 근거

- `third_party/common/vcpkg.json`: `{ "name": "nlohmann-json", "version>=": "3.11.3" }`
- FetchContent 대비 경로: `modules/common/CMakeLists.txt` 의 `GIT_TAG v3.11.3`, 루트 `CMakeLists.txt` 도 같은 라이브러리
- 이 빌드의 `_deps/nlohmann_json-src` 는 3.11.3 이다: `json.hpp` 의 버전 가드가 `!= 3 || != 11 || != 3` 이면 `#error` 이고 이 빌드가 통과한다(`abi_macros.hpp` 가 `NLOHMANN_JSON_VERSION_MAJOR 3` 을 정의)
- `xpe_common` 이 `nlohmann_json::nlohmann_json` 를 **PUBLIC** 으로 링크하고(`modules/common/CMakeLists.txt` 의 `target_link_libraries(xpe_common PUBLIC … nlohmann_json::nlohmann_json …)`), `ai` 모듈은 PRIVATE 로 쓴다. 이제 `xpe_preprocess` 도 PRIVATE 로 명시한다.

## 7. 208 보고서 정정 (Codex #40 이 확인 사항으로 적은 것)

1. **방어 검사 실패의 계약.** 208 의 판정서는 방어 검사가 막으면 "읽기 초과가 아니라 오류로 끝난다"고만 적었다. 정확한 계약은 이렇다: 단계가 이미 몇 개 돌았다면 그 **단계 완료 플래그는 메타데이터에 이미 기록되어 있을 수 있다**(OOM 외의 모든 오류가 전부터 그랬다). 영상은 건드려지지 않는다(결과는 마지막 단계 뒤에만 되써진다). 헤더(`preprocess_api.h`)에 이를 적었다: "…leaves the frame … as the caller gave it … does not undo the stage-completion flags … (A refusal before any stage runs sets none.)" "메타데이터 전체 불변"이 아니다.
2. **96 구성의 범위.** 96 은 **선택한 축의 전수**이다: 온도·오프셋·게인·결함·고스트(핸들 있음)의 이진 축 5개와 비닝 3상태. 전체 설정 공간이 아니다. 들어 있지 않은 것: 비선형성 표(`bypassNonlinearity` 는 켠 구성이 없다 — 시험은 항상 bypass), NLCSC/읽기 모드, 핸들 없는 고스트(예측식에서 float 을 요구하지 않는다), 배치 진입점, 할당 실패 스윕, float32 입력.
3. **감시 영역의 한계.** "초과 읽기 0" 은 16바이트 경계에 맞춰 둔 블록에서 이번 크기(32·64바이트)에 대한 결과이고, 모든 길이·플랫폼으로 일반화할 수 없다(208 의 Gaps 에 이미 적었다).

## 8. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 778 실행 / 770 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 39 통과 (`26_pre_oom.txt`; 직전과 같음, 스윕 지점 수는 3번 주장) |
| common OOM exe / common 기존 | 12 / 69 통과 |
| `ctest -N` | 938 (직전 937, 새 시험 1건) |
| 수출 이름 | preprocess 48, common 16 불변 (`29_exports_pre_diff.txt`) |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12 |

## 9. 미검증 (Gaps)

- **UTF-8 BOM·잘못된 UTF-8 로 시작하는 설정**은 시험하지 않았다. nlohmann 이 잘못된 UTF-8 문자열을 거부하는 것은 라이브러리 동작에 기댄 것이다.
- **아주 깊거나 큰 설정**(수 MB, 깊이 수만)은 시험하지 않았다. 게인 파일의 설정 블록은 작고 생성기는 얕은 객체를 쓴다.
- **성능은 재지 않았다.** 게인 적재 한 번당 할당 지점이 12 → 91 로 늘었다(작은 문자열들): 3072² 파일 읽기(수십 MB)에 비하면 작지만 숫자는 없다.
- **파이프라인 설정 JSON**(`xpe_json_get_string`)은 그대로이며 첫 출현 한계가 있다 — 209 의 몫.
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만 돌렸다.

## 10. 잔여 위험

- 현장 구형 XCal 이 유효한 JSON 이 아닌 설정을 갖고 있다면(예: 끝 쉼표) 이제 품질 필드 조회에서 `CONFIG_INVALID` 로 거부된다. 제품 생성기 산출물은 영향이 없다.
- 라이브러리 버전이 바뀌면(`>= 3.11.3` 의 상한이 없다) SAX 인터페이스의 서명이 달라질 수 있다 — 컴파일 오류로 드러난다.
