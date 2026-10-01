# QA-A-24 검증 보고서 — 네이티브 저하 모드 자리표시자 삭제 (#128, #56)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-24 (`.moai/lanes/pre/inbox/QA-A-24.md`)

## 1. 주장 (Claim)

1. **선행 조건을 먼저 실측했다.** `.github/workflows/benchmark-regression.yml` 에 `bp10-degraded-mode` 잡이 없다(`grep -c` = 0, 종료코드 1). 파일 4행에 이동 사실이 주석으로 남아 있다. 삭제해도 `passedCount` 가 0이 되는 잡이 없다.
2. `modules/common/tests/test_post_degraded_mode.cpp` (88줄, `DegradedMode` BP06~BP10 5케이스)를 삭제하고 `modules/common/CMakeLists.txt` 등록을 제거했다.
3. `TEST(DegradedMode,` 와 `test_post_degraded_mode` 참조가 소스 트리에서 **0건**임을 실측했다.
4. 재실측 무회귀. ci-common 74 → **69/69 PASS**, ci-preprocess 494/494 PASS. 두 로그 모두 `DegradedMode` 문자열 **0회** — skip 5건이 사라졌다.

## 2. 증거 (Evidence)

### 선행 조건 (삭제 전 실측)

```
$ grep -c "bp10-degraded-mode" .github/workflows/benchmark-regression.yml
0
count_exit=1
```

같은 파일 4행:

```
# BP-10 (degraded-mode cross-lane) moved to ci.yml dotnet-tests: DegradedModeReadinessTests /
```

### 삭제 전 상태 (`a24-before-skips.txt`, QA-A-14 실행 로그에서 발췌)

```
61/74 Test #61: DegradedMode.BP06_GsvgMissingReportsR0 .....................................***Skipped   0.01 sec
62/74 Test #62: DegradedMode.BP07_CollimationMissingEnhanceAdvancedReportsR0 ...............***Skipped   0.01 sec
63/74 Test #63: DegradedMode.BP08_EiMissingEnhanceBasicReportsR0 ...........................***Skipped   0.01 sec
64/74 Test #64: DegradedMode.BP09_DicomMissingReportsR0 ....................................***Skipped   0.01 sec
65/74 Test #65: DegradedMode.BP10_DisplayMissingReportsR0 ..................................***Skipped   0.01 sec
```

다섯 케이스는 `XPE_DEGRADED_ABSENT_DLL` 이 설정되지 않아 항상 `GTEST_SKIP()` 으로 끝났다(`test_post_degraded_mode.cpp:46`). 즉 일상 실행에서 단언을 하나도 수행하지 않았다.

### 삭제 후 재실측

```
100% tests passed, 0 tests failed out of 69     (a24-common.log, exit=0)
100% tests passed, 0 tests failed out of 494    (a24-pre.log,    exit=0)
```

두 로그에서 `DegradedMode` 문자열 검색 결과 각각 **0건**. ci-common 로그에는 `Skipped` 행 자체가 남아 있지 않다 — 이 프리셋의 유일한 skip 이 이 다섯이었다.

### grep (`a24-grep.txt`)

```
# exact suite name TEST(DegradedMode, ...) — the deleted placeholder
exit=1                                  ← 일치 0

# test_post_degraded_mode references anywhere
exit=1                                  ← 일치 0

# XPE_DEGRADED_ABSENT_DLL (the placeholder's gate variable)
./build/ci-common/Testing/Temporary/LastTest.log:1671: ...   ← 빌드 산출물(과거 실행 로그)뿐
```

`XPE_DEGRADED_ABSENT_DLL` 이 남은 곳은 **삭제 전 실행이 남긴 `build/` 아래 ctest 로그**뿐이고 소스에는 없다. 이 로그들은 다음 실행에서 덮어써진다.

### 삭제 규모 (`a24-diffstat.txt`)

```
 modules/common/tests/test_post_degraded_mode.cpp | 88 ------------------------
 1 file changed, 88 deletions(-)
 modules/common/CMakeLists.txt | 1 -
 1 file changed, 1 deletion(-)
```

## 3. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-common | 74/74 PASS, DegradedMode 5건 skip (QA-A-14, `../QA-A-14/a14-common.log`) | **69/69 PASS, skip 0** (`a24-common.log`) | −5 = 삭제한 케이스 수와 정확히 일치 |
| ci-preprocess | 499/499 PASS (QA-A-14, `../QA-A-14/a14-pre.log`) | **494/494 PASS** (`a24-pre.log`) | −5 = 동일(이 프리셋이 common 스위트를 함께 돈다) |

**카드의 예상치(63 → 58)와는 다르다.** 카드는 2026-09-10 발행 시점의 수를 적었고, 그 뒤 QA-A-13(+4)·QA-A-14(+1·+2)가 케이스를 추가했다. 감소분 −5 는 카드가 예측한 값과 같으므로 예상과 어긋난 것은 절대 수치뿐이다.

## 4. 미검증 (Gaps)

- **Lane C 의 xUnit 대체 검증을 실행하지 않았다.** 카드가 근거로 든 `DegradedModeReadinessTests` / `ModuleReadinessReportingTests` 가 실제로 저하 준비도를 검증하는지는 **카드와 C-14/C-18 판독을 인용한 것**이고, 이번 턴에 `dotnet test` 를 돌려 확인하지 않았다. Lane C 소유라 실행하지 않았다.
- **`benchmark-regression.yml` 을 읽었을 뿐 실행하지 않았다.** 잡이 없다는 것은 파일 검사로 확인했고, 워크플로가 실제로 통과하는지는 CI 가 판정한다.
- **다른 `*Degraded*` 스위트는 손대지 않았다.** `modules/gsvg/tests/test_gsvg_degraded.cpp`(`GsvgDegradedMode`, Lane B)와 `modules/preprocess/tests/test_preprocess_degraded.cpp`(BP-01~05, Lane A 소유)는 이름만 비슷할 뿐 다른 스위트이고 실제 단언을 수행한다. 카드가 "grep 0 이어야 함" 이라 했지만 `modules/**` 에 `DegradedMode` 문자열은 19곳 남아 있다 — 전부 이 둘과 그 주석이다. 정확한 대상(`TEST(DegradedMode,`)으로 좁히면 0이다.
- **삭제된 5케이스가 `XPE_DEGRADED_ABSENT_DLL` 을 설정한 환경에서 무엇을 검증했는지는 관측하지 않았다.** 그 환경을 만들어 돌려 본 적이 없으므로, "검증 가치가 없다" 는 판단은 소스 판독(단언이 `EXPECT_FALSE(exists(dll))` 하나뿐)에 근거한다.

## 5. 잔여 위험 (Residual risk)

- **네이티브 쪽 저하 모드 커버리지가 0이 된다.** 이제 이 영역을 지키는 것은 Lane C 의 xUnit 뿐이다. 그쪽이 비활성화되거나 조건부로 skip 되면 아무도 보지 않는 상태가 되고, 이번 카드는 그 조건을 확인하지 않았다.
- **`bp10-degraded-mode` 잡이 되살아나면 깨진다.** 워크플로가 이 파일을 다시 요구하도록 되돌려지면 `passedCount` 0 실패가 재현된다. 삭제는 워크플로 변경에 의존하는 순서 있는 작업이었고, 그 의존은 코드에 남아 있지 않다(카드와 이 보고서에만 있다).
- **항상 skip 되는 테스트는 삭제 전까지 "통과"로 집계됐다.** 같은 형태가 다른 스위트에도 있는지는 확인하지 않았다 — 환경변수로 게이트된 테스트는 존재만으로 커버리지가 있다는 인상을 준다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| 선행 조건(워크플로 잡 부재) 실측 | QA-A-24 |
| 자리표시자 삭제 + CMake 등록 제거 | QA-A-24 |
| `DegradedMode` 참조 grep | QA-A-24 |
| ci-common / ci-preprocess 재실측 (전·후 카운트) | QA-A-24 |
| 환경변수 게이트 테스트 전수 조사 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a24-before-skips.txt` | 삭제 전 5건 skip 행 (QA-A-14 실행 로그 발췌) |
| `a24-common.log` | 삭제 후 ci-common 69/69 PASS, skip 0 (exit=0) |
| `a24-pre.log` | 삭제 후 ci-preprocess 494/494 PASS (exit=0) |
| `a24-grep.txt` | 정확한 대상 grep 3종 결과 |
| `a24-grep-modules.txt` | `modules/**` 의 `DegradedMode` 문자열 19곳 (전부 다른 스위트) |
| `a24-diffstat.txt` | 삭제 `git diff --stat` |
