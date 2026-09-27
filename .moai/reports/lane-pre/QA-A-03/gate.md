# QA-A-03 — xpe_test_inject_alert 공개 API 승격

- **브랜치**: `dev/preprocess` · **워크트리**: `D:/workspace-github/xpe-pre`
- **지시**: lead 판정 [B] (a) 공개 API 승격. 개명은 별도 카드. Lane B 코드 변경 없음.
- **결과**: `xpe_common.dll` export 16 = 공개 헤더 선언 16, **심볼명 전건 일치**

## 변경

### 1. `modules/common/include/xpe/common/xpe_error.h` (+21줄)

`xpe_clear_alerts()` 뒤, `extern "C"` 블록 안에 선언 추가. alert 폴링 API 3종이 이미
이 헤더에 모여 있어 생산자 함수의 자리로 적절하다.

```c
XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity);
```

Doxygen 주석에 `test_` 접두가 역사적 잔재이며 개명은 호출 모듈 동시 수정이 필요해
별도로 추적된다는 점을 명시했다(lead 지시 반영).

### 2. `modules/common/src/xpe_common.cpp` (정의부 주석 정정)

선언이 공개된 이상 아래 두 표기가 사실과 어긋나므로 함께 고쳤다.

```diff
-/* Internal test-support helpers (white-box linkage for unit tests). */
-/** @cond INTERNAL */
+/* Alert producer (declared in xpe_error.h). */
```
`@cond INTERNAL` / `@endcond` 제거. 남겨 두면 공개 헤더가 선언한 함수를 Doxygen이
문서에서 배제해 헤더와 문서가 갈린다.

### 3. `modules/common/include/xpe/common/xpe_common_api.h` (카운트 정정)

`4 alert functions` → `5`, `Error/Alert (4)` → `(5)` 및 목록에 심볼 추가.

## REQ-P0-008 불일치 — lead 판정 필요 (조치 보류)

같은 헤더에 요구사항이 박혀 있다:

```
REQ-P0-008: xpe_common.dll SHALL export exactly 15 functions with C linkage.
```

**이 요구사항은 본 카드 이전부터 이미 깨져 있었다.** QA-A-01 실측에서 DLL은 16개를
export했다(REQ 15). 게이트 5 "심볼 수 문서 일치" 불일치의 정체가 바로 이것이다.
본 카드는 헤더를 바이너리에 맞췄을 뿐, 바이너리를 바꾸지 않았다 — export 수는
16으로 이전과 동일하다.

요구사항 번호는 SRS·RTM 추적성 사안이라 임의로 고치지 않았다. 대신 헤더에 사실을
드러내는 NOTE를 남겼다(REQ 텍스트 자체는 원문 유지).

추가 드리프트 2건도 함께 발견했다. 어느 쪽 숫자와도 맞지 않는다:

```
modules/common/include/xpe/common/xpe_common_api.h:157   "counted toward the 18-function total"
modules/common/include/xpe/common/xpe_memory.h:6         "18-function export contract"
```

정리하면 한 모듈 안에 **15(REQ) / 16(실측·헤더) / 18(주석 2곳)** 세 숫자가 공존한다.
SRS 갱신 방향은 lead 판정 사항.

## 증거

```
cmake --preset ci-preprocess && cmake --build build/ci-preprocess
CONFIGURE_EXIT=0  BUILD_EXIT=0   warning C####: 0건

ctest --output-on-failure
100% tests passed, 0 tests failed out of 345   CTEST_EXIT=0

dumpbin /exports xpe_common.dll      → 16
공개 헤더 XPE_API 선언 (xpe/common/*.h) → 16
심볼명 집합 비교 diff                 → 차이 없음
  (dumpbin 출력의 "characteristics"/"time"은 export 가 아니라 헤더 필드 잡음)

dumpbin /exports xpe_preprocess.dll  → 45 (변동 없음, 헤더 45와 계속 일치)
```

원문: `build.log`, `tests.log`, `abi.log`

## 미검증 (Gaps)

- **Lane B 재빌드 미수행.** 본 카드는 헤더에 선언을 추가했을 뿐 `enhance_basic`의
  자체 extern 선언과 시그니처가 동일하므로 충돌하지 않을 것으로 보이나, Lane B 모듈은
  이 빌드 구성(`BUILD_ENHANCE_BASIC=OFF`)에 포함되지 않아 **실측하지 않았다**.
  Lane B 측 확인이 필요하다.
- `enhance_basic_internal.h:27-30`의 자체 extern 선언은 이제 불필요하지만 Lane B 소유라
  건드리지 않았다. 중복 선언은 시그니처가 같으면 무해하다.
- `modules/common/tests/test_xpe_common.cpp:20` 등 테스트의 자체 extern 선언도 이제
  헤더 include로 대체 가능하나, lead 지시가 "선언 추가만"이라 범위 밖으로 두었다.
