ACK #49 2026-10-01 19:07:13 UTC

# QA-A-209 독립 검토

범위: `D:/workspace-github/xpe-pre`의 `967fb146..90c1b6b1 -- modules`, QA-A-209 보고서와 근거, main의 `docs/project/api-spec.md` §6.19(2). 코드를 대조했으며 빌드와 시험을 새로 실행하지는 않았다.

## 발견 1 — 보통·차단: 비NULL 빈 설정 텍스트가 객체가 아니어도 수락됨

- **무엇:** `helpers.cpp:148-152`의 `xpe_json_top_level_scalar`는 길이 0 또는 공백뿐인 텍스트를 `Absent`로 반환한다. `xpe_config_get_string`·`_double`은 이를 `XPE_OK`로 바꾸므로 파이프라인·고스트·비선형성·오프셋 생성이 비NULL `""`와 공백만 있는 설정에 기본값으로 진행한다. 레인 계약은 **NULL만** 기본값이며, 텍스트가 하나의 JSON 객체가 아니면 `XPE_ERR_CONFIG_INVALID`라고 한다. 14행 표에는 이 경우가 없다.
- **재현:** 유효한 영상과 `configJsonOrNull=""` 또는 `"   "`으로 `xpe_ghost_create`를 호출한다. 키 조회는 전부 Absent가 되어 핸들을 만들고 `XPE_OK`를 반환한다. 계약상 두 입력 모두 거부되어야 한다.
- **고칠 방향:** 비NULL 호출자 설정은 키 조회 전 빈/공백 전용 텍스트를 거부한다. 파일 설정 블록의 *실제 부재*(길이 0)를 계속 허용해야 한다면 그 경로와 비NULL 호출자 텍스트의 규칙을 분리해 시험한다.

## 발견 2 — 보통·차단: 알 수 없는 최상위 키의 중복은 검출하지 못함

- **무엇:** `TopLevelProbe::key` (`helpers.cpp:102-107`)는 조회 대상 `wanted_`와 같은 키만 센다. 따라서 알려진 키의 중복은 거부하지만 `{"future":1,"future":2}` 같은 다른 최상위 중복은 모든 조회에서 `Absent`로 끝나며 허용된다. “같은 이름의 최상위 키가 두 번 나오면 거부”라는 레인·공개 계약보다 좁다.
- **재현:** 위 JSON을 `xpe_ghost_create`에 주면 일곱 키 조회가 모두 성공하고 기본값 핸들이 만들어진다. 최상위 중복 키를 전역으로 검사하면 거부되어야 한다.
- **고칠 방향:** 한 번의 파싱에서 최상위 키의 중복을 조회 대상과 무관하게 검출하거나, 계약을 “읽는 키가 중복될 때만 거부”로 명시해 시험·헤더를 일치시킨다.

## 발견 3 — 높음·차단: XCal 압축 메타데이터는 여전히 첫 출현을 읽음

- **무엇:** 호출자 전수에서 `modules/preprocess/src/xcal_reader.cpp:35-93`의 `parse_compression_meta`가 빠졌다. 이 함수는 XCal `config_json` 설정 블록에서 `cfg.find("\"xcal_compression\":")`와 `cfg.find("\"xcal_raw_payload_len\":")`로 첫 문자열을 찾아 압축 여부·원시 길이를 결정한다. 최상위 여부, 중복, JSON 객체 유효성을 검사하지 않는다. offset·gain·defect·LUT 적재가 공유하는 `read_xcal_file`에서 이 판정을 헤더 검증·해시·압축 해제 전에 사용한다.
- **재현:** 체크섬을 맞춘 비압축 XCal의 설정 블록에 `{"nested":{"xcal_compression":1,"xcal_raw_payload_len":N}}`을 넣는다. 계약상 압축 키는 없지만 reader는 중첩 값을 읽고 비압축 payload를 RLE로 해석하려 한다. 중첩 값을 앞에 두고 실제 최상위 압축 키를 뒤에 둔 블록도 첫 값을 잘못 택한다.
- **고칠 방향:** 길이 지정 JSON 블록을 압축 판정 전에 한 객체로 검증하고 두 압축 키를 최상위에서만, 중복 없이 읽는다. 비압축 파일의 중첩 가짜 키, 압축 파일의 중첩+최상위 키, 중복·깨진 JSON을 offset/defect 포함 적재 시험에 넣는다.

## 발견 4 — 높음·차단: 비선형성 단독 C ABI에 새 파싱 할당 예외가 노출됨

- **무엇:** `xpe_nonlinearity_correct` (`nonlinearity_correct.cpp:391-395`)는 `xpe_nonlinearity_apply`를 그대로 반환하며 예외 가드가 없다. 이번 변경의 `xpe_config_get_string` 3회와 다항식 경로의 `xpe_config_get_double` 6회는 nlohmann SAX·문자열 할당을 수행한다. 그 `std::bad_alloc`은 이 공개 함수에서 잡히지 않아 C ABI 경계를 넘을 수 있다. 레인도 단독 진입점 할당 실패 스윕 미실시를 적었다. 파이프라인 진입점의 예외 가드는 이 단독 호출을 보호하지 않는다.
- **재현:** 유효한 영상과 비NULL JSON으로 `xpe_nonlinearity_correct`를 호출하면서 SAX/문자열 할당 지점에 실패를 주입한다. `TopLevelProbe` 또는 SAX가 던진 `bad_alloc`을 잡는 코드 없이 공개 함수가 종료된다.
- **고칠 방향:** 공개 단독 진입점에 `bad_alloc→XPE_ERR_OUT_OF_MEMORY`, 기타 예외→처리 실패 가드를 두고, 실패 때 영상 불변을 확인하는 할당 실패 스윕을 실행한다.

## 요청 항목 확인

- **202b 숫자 규칙:** 두 표 시험은 원래 `"detectorTempC":" +25"`·`"alpha1":" +2"` 같은 **JSON 문자열 값**을 만들었다. 이번 `jsonStringBody()`는 원시 TAB 등을 JSON 이스케이프로 바꿀 뿐 파서가 돌려주는 문자열 내용과 수락 기대를 유지한다. 파이프라인의 `detectorTempC`·`binningMode`와 고스트의 숫자 키는 `xpe_config_get_string` 뒤 `xpe_strict::parse_float/int/double`을 쓰므로 202b의 앞 공백·단일 `+` 규칙이 공존한다. `xpe_config_get_double`을 쓰는 다항식 계수, 오프셋 생성, XCal 범위·LUT 키는 따옴표 없는 JSON 숫자만 숫자로 취급한다. 보고서의 “숫자는 따옴표 없는 JSON 숫자만”이라는 문장은 **모든 숫자 키**에 대한 설명으로 읽히면 틀리므로 경로별 계약을 분명히 해야 한다.
- **거부 전 상태:** 알려진 키의 중복·깨진 JSON은 파이프라인 세 진입점에서 교정 적재·첫 프레임 전에 검사한다. `_batch`는 동일 설정을 루프 전에 검사하므로 그 경우 여러 프레임과 메타는 불변이다. 고스트는 핸들을 밖에 돌려주기 전에 검사하고 LUT·다항식 게인은 저장소 커밋 전에 읽는다. 이는 발견 1~3의 미거부 입력에는 적용되지 않는다.
- **비용:** 보고된 +23.3 µs/호출은 일반 검출기 프레임의 수십 ms 처리에 비해 작다. 8×8 고빈도 호출에서는 의미가 있을 수 있고, 19회 재파싱과 할당 실패 지점 14→464 증가는 한 번 파싱하는 개선의 근거다. 이 비용만으로 병합을 막지는 않는다.

판정: 보류
