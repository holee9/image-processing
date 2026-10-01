ACK #43 2026-10-01 17:43:46 UTC

# QA-A-208b 독립 검토

인박스 SHA-256 `da881ad477ee1a020dcfdd33a9e30f49adcb41ec0a7c899d0a7e420ed8ad07ce` 확인. `D:/workspace-github/xpe-pre`의 `5d003c4f..2cb3a943 -- modules`, 레인 보고서와 #40 원문, 이 빌드의 nlohmann-json 3.11.3 파서 소스를 검토했다. 새 빌드·실행 시험은 하지 않았다.

## 발견

### 1. 보통 · 차단 — 설정 블록 중간의 NUL 뒤 바이트가 JSON 검증을 우회함

- **무엇:** XCal 설정은 `config_json_len` 길이로 읽어 `std::vector<uint8_t>`에 보관한다 (`xcal_reader.cpp:131-141`). 게인 적재는 이를 길이를 보존한 `std::string`으로 복사하지만 (`xpe_calib_load_gain.cpp:101`), `config_copy.c_str()`만 품질 파서에 전달한다 (`:108`). 새 `xpe_json_top_level_scalar`는 `std::strlen(json)`으로 SAX 입력 끝을 정한다 (`helpers.cpp:204`). 따라서 XCal 블록에 원시 NUL이 있으면 뒤의 바이트는 `strict=true`라도 검사하지 않는다. ‘설정 텍스트 전체가 유효한 JSON 객체 하나’라는 이번 계약과 #40의 깨진 설정 거부 항목이 여전히 성립하지 않는다. 파일의 SHA-256은 전체 블록에 대해 계산되므로 정상 해시만으로 이 문제는 걸러지지 않는다.
- **재현:** 기존 시험의 `writeGainWithConfig`처럼 `write_xcal_file`에 설정 데이터와 `json.size()`를 전달한다. 설정을 `std::string("{\"fit_r_squared\":0.9}\0garbage", 29)` 같은 **길이 지정 바이트열**로 만들어 적재한다(실제 길이는 구성한 문자열의 `size()`를 사용). JSON 전체는 원시 NUL 때문에 잘못됐지만 파서는 NUL 앞의 객체만 읽어 `xpe_calib_load_gain`이 성공한다. `"{}\0{\"fit_r_squared\":\"bad\"}"` 형태는 NUL 뒤의 품질 키까지 숨긴다. 현재 22행 회귀 시험은 중간 NUL을 포함하지 않는다.
- **고칠 방향:** XCal 설정의 실제 길이를 파서에 전달하고 `sax_parse(begin, end, ..., strict=true)`가 전체 바이트를 읽게 한다. 또는 파싱 전 `config_json`/`config_copy`에 원시 NUL이 있으면 `XPE_ERR_CONFIG_INVALID`로 거부한다. 두 재현에 오류 코드와 맵·품질 메타데이터 불변 단언을 추가한다. C 문자열 API를 유지한다면 길이 지정 XCal 경계에서의 NUL 검사가 필요하다.

## 요청 사항 확인

- **#40 발견 1:** `TopLevelProbe::key`는 nlohmann이 이스케이프를 해석한 키를 깊이 1에서 비교한다 (`helpers.cpp:150-155`). 동일 키의 원문/`\u005f` 표기와 두 이스케이프 표기 중복은 `found_ > 1`로 거부된다. 중첩 객체·배열의 키는 깊이 1이 아니라서 세지 않는다. 값이 객체/배열이면 `nested()`가 `NotScalar`로 기록하고, 스칼라면 해당 콜백이 `Scalar`와 값을 기록한다. 첫 항목 뒤에도 끝까지 파싱하므로 나중의 중복·문법 오류를 놓치지 않는다. 이 발견은 해소됐다.
- **#40 발견 2:** nlohmann 3.11.3의 `sax_parse`는 내부 `std::vector<bool> states`와 반복 루프로 객체·배열의 짝을 검사한다. `strict=true`는 첫 값 뒤 EOF를 요구하고, `parse_error` 콜백은 false를 반환한다. 따라서 일반 잘못된 JSON, 금지 escape, 중첩 괄호 불일치, 후행 텍스트는 `Malformed`가 된다. 단, 위 NUL 경계 때문에 XCal **전체 블록**에 대한 보장은 아직 없다.
- **예외/OOM:** 문법 오류는 SAX 콜백의 false로 반환된다. 콜백의 문자열 복사·`std::to_string`, nlohmann 내부 벡터/문자열 등은 `std::bad_alloc`을 던질 수 있지만, `xpe_calib_stage_gain`의 `catch (std::bad_alloc)` (`xpe_calib_load_gain.cpp:147-150`)이 `XPE_ERR_OUT_OF_MEMORY`로 바꾸고 확정 전 상태를 지킨다. 레인 증거의 해당 적재 스윕 12→91 지점 통과는 그 경로를 뒷받침한다. 91은 이 시험 입력에서 계측한 할당 지점 수이지 모든 가능한 JSON의 상한이 아니다.
- **반환 5종:** 호출부는 `Absent`만 기본값 유지, `Scalar`만 엄격 숫자 변환, `NotScalar`·`Duplicate`·`Malformed`는 `XPE_ERR_CONFIG_INVALID`로 처리한다 (`xpe_calib_mode.cpp:288-337`). 깊이·키·숫자 처리와 일치한다. 부분 품질 필드의 이력 문제는 #41/QA-A-202f 별도 범위다.
- **UTF-8/BOM:** 이 버전 lexer는 시작의 완전한 UTF-8 BOM `EF BB BF`를 건너뛰어 유효한 객체를 받아들이고, 불완전·잘못된 BOM은 parse error로 거부한다 (`lexer.hpp:1487-1516`). 문자열의 잘못된 UTF-8과 잘못된 서로게이트 escape도 거부한다. 이는 ‘유효한 객체’ 계약과 맞지만 레인 실행 시험에는 BOM·잘못된 UTF-8 행이 없다. 중간 NUL은 그 검증 이전에 입력 길이가 잘려 생기는 별도 문제다.
- **아주 깊은 입력:** nlohmann 3.11.3 `parser.hpp:180-188`은 재귀 호출 대신 상태 벡터와 루프를 사용하므로 파서 호출 스택이 중첩 깊이에 비례해 자라지 않는다. XCal 설정 길이 상한은 1 MiB (`XCAL_MAX_CONFIG_JSON_LEN`)라 `TopLevelProbe::depth_`의 `int` 오버플로에는 닿지 않는다. 힙 할당 실패는 위 가드가 받지만 1 MiB 깊이 스트레스는 실행 검증되지 않았다.

판정: 보류
