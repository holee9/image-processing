# QA-A-10 — xpe_configure JSON 실제 파싱 (#115, Class B)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-09   **Refs**: #115 (#98 후속)
**baseline**: `git merge main` → HEAD `ae62ff6` (ff, `origin/main...HEAD` = `0 0`)

## 1. 주장 (Claim)

`xpe_configure` 가 선행 공백을 건너뛴 뒤 첫 글자가 `'{'` 인지만 보고 나머지를 그대로
`g_configJson` 에 저장했다. `nlohmann::json::accept()` 기반 문법 검증으로 교체해
`{not json` 과 64 KB 오류 페이로드를 거부하게 했다. 기존 테스트 무회귀.

## 2. 증거 (Evidence)

### 2.1 RED — 수정 전 (`red-run.log`)

```
[  FAILED  ] XpeCommonTest.ConfigureWithMalformedJsonBodyReturnsInvalid
  xpe_configure("{not json")
    Which is: 0        <- XPE_OK. 기대: -4 (XPE_ERR_CONFIG_INVALID)
[  FAILED  ] XpeCommonTest.ConfigureWithVeryLongMalformedJsonReturnsNegative
  Expected: (static_cast<int>(result)) < (0), actual: 0 vs 0
[  PASSED  ] 6 tests.
[  FAILED  ] 2 tests
```

### 2.2 GREEN — 수정 후 (`green-run.log`)

```
[==========] 8 tests from 1 test suite ran.
[  PASSED  ] 8 tests.
```

### 2.3 무회귀 — ci-preprocess 전체 (`ctest.log`, 이번 실행 실측)

```
100% tests passed, 0 tests failed out of 347
Total Test time (real) =  23.66 sec
```

미실행 14건은 전부 사전 존재하던 Skipped/Disabled (DegradedMode BP06-10,
PipelinePerformance3072x3072, Calib/Preprocess 캘리브레이션 8건) — 이번 변경과 무관.

### 2.4 변경 diff

| 파일 | 내용 |
|---|---|
| `modules/common/src/xpe_common.cpp` | `#include <nlohmann/json.hpp>` + `accept()` 검증 (+14/-1) |
| `modules/common/tests/test_xpe_common.cpp` | RED 케이스 2개 + `<string>` (+23) |

`nlohmann_json::nlohmann_json` 은 `modules/common/CMakeLists.txt:63` 에서 이미
`xpe_common` 에 PUBLIC 링크돼 있어 빌드 설정 변경 없음.

### 2.5 C# 기대치와의 대조 (수정 금지 — 읽기만)

| C# 테스트 | 입력 | 기대 | 이번 gtest 대응 |
|---|---|---|---|
| `MetadataMarshallingTests` REQ-GUI-IT-022 | `TestDataLoader.MalformedConfigJson` = `"{not json"` | `CONFIG_INVALID` | `ConfigureWithMalformedJsonBodyReturnsInvalid` |
| `NativeErrorTranslationTests` REQ-GUI-IT-006 | `"{" + 64 KB 'x'` | 음수 코드, 예외 없음 | `ConfigureWithVeryLongMalformedJsonReturnsNegative` |

## 3. 설계 판단 — `'{'` 검사를 남긴 이유

카드는 "`accept()` 기반 검증으로 교체"라 했으나 `'{'` 검사를 **삭제하지 않고 유지**했다.
`accept("[1,2]")` 와 `accept("42")` 는 모두 `true` 다. `'{'` 검사를 지우면 배열·스칼라가
설정으로 통과하게 되어 **기존 계약이 넓어진다**. 카드가 요구한 것은 좁히는 것이므로
두 검사를 함께 두었다 — 결과는 이전보다 엄격하고, 넓어진 방향은 없다.

`json::parse` 가 아니라 `accept` 를 쓴 이유: DOM 을 만들지 않고 예외도 던지지 않아
64 KB 오류 페이로드가 C ABI 를 넘어 스택을 되감지 않는다.

## 4. Gaps (미검증)

- **CI dotnet-tests 2건은 이 레인에서 검증 못 했다.** C# 테스트를 실행하지 않았다
  (Lane C 소유, 수정·실행 범위 밖). leader 가 main 병합·push 후 CI 로 최종 판정한다.
- `ci-preprocess` **단일 구성**에서만 검증. `ci-common` / `ci-fullstack` 미실행.
- 스키마 검증(키 존재 여부)은 카드 지시대로 손대지 않았다 — 별개 요구.

## 5. 잔여 위험

- `accept()` 는 UTF-8 유효성도 함께 본다. 기존에 통과하던 **비UTF-8 바이트가 섞인 설정**이
  있었다면 이제 거부된다. 저장소 내 그런 호출자는 찾지 못했으나 전수 확인은 아니다.
- `std::string(jsonConfig)` 로 1회 복사한다. 64 KB 페이로드에서 측정 가능한 비용은
  아니지만, 훨씬 큰 설정을 넣는 호출자가 생기면 재검토 대상.
