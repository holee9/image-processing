# QA-A-29 검증 보고서 — `xpe_calib_save` 만료 인자 추가 (#132, SRS-ALERT-005 복원)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-29 (`.moai/lanes/pre/inbox/QA-A-29.md`)
- 정본: `docs/project/api-spec.md` §6.14 (normative), REQ-P1A-019
- 선행 병합: `git merge origin/main` → `45fbe2c`, §6.14 3인자 문서가 트리에 존재함을 확인
- 커밋 2건: `0836c31` (ABI), `45702fe` (테스트)

## 1. 주장 (Claim)

1. `xpe_calib_save` 를 `(filepath, calib_type, uint64_t expiry_epoch_ms)` 3인자로 바꾸고, offset/gain/defect 세 분기 모두가 그 값을 XCal 헤더에 기록한다. `0` 은 만료 없음.
2. 만료 정책 값(30일 등)은 내장하지 않았다. 네이티브는 호출자가 준 값을 그대로 기록만 한다 — 카드의 "하지 않을 것" 준수.
3. `xpe_calib_generate_*` 시그니처는 건드리지 않았다.
4. **SRS-ALERT-005 경로가 API 로 도달 가능해졌다.** 과거 만료값으로 저장한 파일을 `xpe_calib_load_offset` 이 `XPE_ERR_CALIBRATION_EXPIRED` 로 거부하는 것을 실측했다 — #132 이전에는 공개 API 로 이 상태를 만들 수 없었다.
5. 기존 호출자 24곳을 전부 갱신했다(모두 `modules/preprocess/tests`). C# 두 곳은 export 이름 문자열 목록이라 시그니처 영향이 없다.
6. RED(컴파일 실패) → GREEN 실측. ci-preprocess 487/487, ci-common 69/69 PASS. `xpe_preprocess.dll` export 목록 **완전 동일**(50개).

부수 수정 1건 — `modules/preprocess/tests/fixtures/make_xcal.hpp` 가 `std::ofstream` 을 쓰면서 `<fstream>` 을 포함하지 않아, 포함 순서에 따라 C2079 로 깨졌다(RED 로그 15행). 헤더가 직접 포함하도록 고쳤다. 카드 범위 밖이지만 이번 변경이 드러낸 결함이고 한 줄이라 함께 처리했다.

## 2. 증거 (Evidence)

### RED — ABI 변경 전 (`a29-red.log`, exit=91)

새 테스트가 3인자로 호출하자 컴파일이 실패한다. 기능이 없었다는 직접 증거다.

```
test_calib_save_expiry.cpp(77): error C2660: 'xpe_calib_save': 함수가 3개의 인수를 사용하지 않습니다.
test_calib_save_expiry.cpp(94): error C2660: 'xpe_calib_save': 함수가 3개의 인수를 사용하지 않습니다.
test_calib_save_expiry.cpp(108): error C2660: 'xpe_calib_save': 함수가 3개의 인수를 사용하지 않습니다.
```

같은 로그 15행에 `make_xcal.hpp(128): error C2079` — 위에 적은 부수 결함.

### GREEN — ci-preprocess (`a29-green.log`, exit=0)

```
100% tests passed, 0 tests failed out of 487
```

### ci-common 재실측 (`a29-common.log`, exit=0)

```
100% tests passed, 0 tests failed out of 69
```

### export 목록 diff (`a29-dumpbin.log`, `a29-export-diff.txt`)

```
          50 number of functions
          50 number of names
```

`a29-old-names.txt`(QA-A-01 `abi.log` 의 `xpe_preprocess.dll` 표) 와 `a29-new-names.txt`(이번 실측) 의 `diff` 는 **빈 출력, exit 0** — 이름 하나도 바뀌지 않았다. `extern "C"` 라 인자 개수가 데코레이션에 반영되지 않으므로 예상된 결과이며, 그 예상을 실제 diff 로 확인했다.

### 변경 규모 (`a29-diffstat.txt`)

```
 11 files changed, 185 insertions(+), 32 deletions(-)
```

## 3. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 482/482 PASS (QA-A-28, `../QA-A-28/a28-pre.log`) | **487/487 PASS** (`a29-green.log`) | +5 = 신규 케이스, 회귀 0 |
| ci-common | 69/69 PASS (QA-A-28, `../QA-A-28/a28-green.log`) | **69/69 PASS** (`a29-common.log`) | 변화 없음 |
| `xpe_preprocess.dll` export | 50 names (QA-A-01 `abi.log:132`) | **50 names, 목록 동일** (`a29-export-diff.txt` 빈 출력) | 변화 없음 |

세 수치 모두 이 워크트리에서 이번 턴에 측정했다.

## 4. 호출자 전수 (`grep -rn xpe_calib_save modules/ clients/ gui/`)

### 갱신한 호출 (24곳, 전부 Lane A 소유)

| 파일 | 호출 수 |
|---|---|
| `modules/preprocess/tests/test_xpe_calib_save.cpp` | 8 |
| `modules/preprocess/tests/test_xpe_preprocess_calibration.cpp` | 5 |
| `modules/preprocess/tests/test_calibration_manager.cpp` | 4 |
| `modules/preprocess/tests/test_calibration_roundtrip.cpp` | 3 (호출 2 + 헤더 주석 1) |
| `modules/preprocess/tests/test_boundary.cpp` | 2 |
| `modules/preprocess/tests/test_xpe_preprocess.cpp` | 2 |

모두 `, 0` 을 붙여 종전 동작(만료 없음)을 유지했다. 프로덕션 코드에는 호출자가 없다 — `xpe_calib_save` 는 아직 앱에서 호출되지 않는다.

### Lane C 소유 — 목록만 보고, 수정하지 않음

| 파일 | 내용 |
|---|---|
| `clients/ImageProcTest/Diagnostics/XpePreprocessReadinessProbe.cs:24` | `"xpe_calib_save"` — export 존재 확인용 이름 문자열 |
| `clients/ImageProcTest.IntegrationTests/PInvoke/XpePreprocessNative.cs:74` | 같음 |

둘 다 P/Invoke 시그니처 선언이 아니라 이름 목록이므로 이번 ABI 변경으로 **깨지지 않는다**. 다만 C# 이 실제로 이 함수를 호출하게 될 때는 3인자 선언이 필요하다.

### 주석에만 등장(변경 불필요)

`modules/preprocess/tests/test_calibration_cache.cpp:38`, `test_pipeline_ex.cpp:84`, `modules/preprocess/src/xcal_writer.hpp:8`, `modules/preprocess/src/xpe_calibration.cpp:8`.

## 5. 미검증 (Gaps)

- **카드 항목 (d) 는 대체하지 않고 사유를 남긴다.** `test_calibration_roundtrip.cpp` 의 만료 케이스는 여전히 XCal 파일을 직접 기록한다. API 경로로 바꾸면 리더의 만료 강제를 라이터의 정확성에 의존해 검증하게 되어, 한쪽이 깨졌을 때 두 케이스가 함께 침묵한다. API 경로는 새 스위트가 별도로 덮는다. 파일 헤더 주석에 같은 내용을 적었다.
- **C# 실행 미확인.** 위 두 `.cs` 파일이 이름 목록이라는 판단은 해당 줄을 읽어서 내린 것이고, C# 테스트를 돌려 확인하지는 않았다. Lane C 소유라 실행하지 않았다.
- **ASan 재측정 없음.** 이번 변경은 헤더 필드 기록 한 줄이라 메모리 동작이 바뀌지 않지만, 측정하지 않은 것은 측정하지 않은 것이다.
- **`uint64_t` → `int64_t` 변환 극단값 미검증.** 헤더 필드가 `int64_t` 라 `static_cast` 를 쓴다. `expiry_epoch_ms > INT64_MAX` 를 넘기면 구현정의 동작으로 음수가 되지만, 그 값을 실제로 넣어 보지 않았다. 실용 범위(현재 시각 ± 수백 년)에서는 도달할 수 없다.
- **defect / gain 분기의 만료 기록은 코드로만 확인**했다. 테스트는 offset 경로만 실행한다.

## 6. 잔여 위험 (Residual risk)

- **깨는 변경이다.** 헤더 시그니처가 바뀌었으므로 이 DLL 헤더로 빌드하던 외부 소비자는 재컴파일이 필요하다. export 이름은 그대로라 **런타임 로드는 성공하고 인자만 어긋난다** — C ABI 상 가장 조용한 실패 형태다. C# 이 2인자로 선언한 채 호출하면 스택에 쓰레기 만료값이 들어간다. 현재 C# 은 이름만 참조하므로 지금은 무해하지만, GUI 가 이 함수를 쓰기 시작할 때 반드시 3인자로 선언해야 한다.
- **만료 정책이 어디에도 없다.** 네이티브는 값을 기록만 하므로, 앱이 만료를 계산해 넘기지 않으면 실무에서는 여전히 만료 없는 파일만 생긴다. #132 는 도달 가능성을 복원했을 뿐 정책을 만들지는 않았다.
- **시계 의존.** 만료 판정은 `system_clock` 이다. 장비 시계가 어긋나면 유효한 캘리브레이션이 만료로 판정되어 촬영이 차단될 수 있다(SRS-ALERT-005 의 의도된 동작이지만, 오작동 경로이기도 하다).

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| ABI 3인자 변경 (커밋 1) | QA-A-29 |
| RED/GREEN 테스트 5케이스 (커밋 2) | QA-A-29 |
| 호출자 전수 갱신 + Lane C 목록 보고 | QA-A-29 |
| ci-preprocess / ci-common 재실측 + export diff | QA-A-29 |
| C# 3인자 선언 (실제 호출 시작 시) | Lane C 신규 카드 필요 |
| defect / gain 분기 만료 기록 테스트 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a29-red.log` | ABI 변경 전 빌드 실패 — C2660 3인자 미지원 (exit=91) |
| `a29-green.log` | ci-preprocess 487/487 PASS (exit=0) |
| `a29-common.log` | ci-common 69/69 PASS (exit=0) |
| `a29-dumpbin.log` | `dumpbin /exports xpe_preprocess.dll` — 50 functions / 50 names |
| `a29-old-names.txt`, `a29-new-names.txt` | export 이름 목록 (baseline / 이번 실측) |
| `a29-export-diff.txt` | 위 둘의 diff — 빈 출력 |
| `a29-diffstat.txt` | 커밋 2건의 `git diff --stat` |
