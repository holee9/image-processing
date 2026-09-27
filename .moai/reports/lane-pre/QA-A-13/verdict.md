# QA-A-13 검증 보고서 — `tests/common*` 3개 트리 처분 (#109 #113 2단계)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-13 (`.moai/lanes/pre/inbox/QA-A-13.md`)
- 선행 병합: `git merge origin/main` → `b994eee`

## 1. 주장 (Claim)

1. `tests/common_smoke/test_common_smoke.cpp` (104줄, 12 케이스)를 `modules/common/tests/` 와 전수 대조했다. **고유 단언 2건**을 이식하고 나머지 10건은 이미 덮여 있음을 확인했다.
2. `tests/common`, `tests/common_unit`, `tests/common_smoke` 세 트리를 삭제했다 (`git rm -r`, 1,211줄).
3. A-28 발견 항목 처리: `modules/common/tests/test_xpe_error_safety_violation.cpp` 는 **중복이 아니라 고유**였다. 등록했다.
4. 재실측 무회귀. ci-common 69 → **73/73 PASS**, ci-preprocess 493 → **497/497 PASS**.

증가분 +4 는 전부 이번 카드의 것이다(이식 2 + 안전위반 2). ci-preprocess 프리셋이 common 스위트까지 함께 돌린다는 점을 실측으로 확인했다 — `a13-pre.log:159` 에 `XpeErrorSafetyViolationTest` 가 72번으로 실행된다.

## 2. 대조표 — `tests/common_smoke` 12 케이스

| smoke 케이스 | 판정 | 근거 |
|---|---|---|
| `InitAcceptsNullConfig` | 덮임 | `XpeCommonTest.InitReturnsOk` — `xpe_init(nullptr)` 로 동일 검사 |
| `VersionIsNonNull` | 덮임 | `VersionReturnsValidString` — `ASSERT_NE(ver, nullptr)` |
| `VersionIsNonEmpty` | 덮임 | 같은 케이스 — `EXPECT_STRNE(ver, "")` + 길이 ≥ 3 (더 강함) |
| `ErrorStringMapsKnownCode` | **이식** | 정확한 문자열 `"Invalid input parameter"` 를 못 박는 단언이 모듈 쪽에 없었다. `ErrorStringReturnsNonNullForAllCodes` 는 "각 코드가 자기 문자열을 가진다" 까지만 본다 |
| `GetParamRangeReturnsOk` | 덮임 | `GetParamRangeReturnsValidValues` |
| `GetParamRangeIsOrdered` | 덮임 | 같은 케이스가 `min <= def <= max` 를 이미 검사 |
| `AllocSourceHasBackingBuffer` | 덮임 | `AllocImageSucceedsForValidInput` — `EXPECT_NE(buf.data, nullptr)` |
| `AllocSourceHasCorrectDataSize` | 덮임 | `AllocImageUint16Format` — `dataSize == 100*100*2` (같은 규칙, 더 큰 크기) |
| `CopyImageReturnsOk` | 덮임 | `CopyImageSucceeds` |
| `CopyImagePreservesContents` | **이식** | smoke 는 버퍼 전체를 `memcmp` 하고, 모듈 쪽 `CopyImageSucceeds` 는 원소 [0] 하나만 본다. 전체 비교가 더 강하다 |
| `CopyImagePreservesDimensions` | 덮임 | `CopyImageSucceeds` — width/height 검사 |
| `FreshRuntimeHasNoAlerts` | 덮임 | `GetPendingAlertCountInitiallyZero` |

이식 결과는 `modules/common/tests/test_xpe_common.cpp` 에 새 케이스 2개로 추가했다 (`ErrorStringForInvalidInputHasExactText`, `CopyImageReproducesEveryByte`). 기존 케이스의 단언은 하나도 건드리지 않았다 — 카드의 "모듈 테스트 단언 변경 금지" 준수.

`tests/common`(596줄)·`tests/common_unit`(491줄)의 고유 커버리지 0 은 QA-A-04/05 에서 이미 확정된 값이라 이번에 다시 대조하지 않았다(§4 참조).

## 3. `test_xpe_error_safety_violation.cpp` 판정 — 등록

| 케이스 | 중복 여부 | 근거 |
|---|---|---|
| `SafetyViolationErrorCodeExists` | **고유** | `XPE_ERR_SAFETY_VIOLATION == -11` 이라는 **값**을 못 박는 단언이 다른 어디에도 없다 |
| `SafetyViolationErrorStringMapped` | **고유** | 정확한 문자열 `"Safety violation"` 을 못 박는다. `ErrorStringReturnsNonNullForAllCodes` 는 -11 이 자기 문자열을 가진다는 것까지만 본다 |

둘 다 AC-LC-005 / REQ-ADV-051(SAF-100)에 직접 대응하는 안전 관련 단언이다. 중복이 아니므로 처분이 아니라 **등록**이 옳다. `modules/common/CMakeLists.txt` 에 등록했고 두 케이스 모두 통과한다.

## 4. 증거 (Evidence)

### ci-common 재실측 (`a13-common.log`, exit=0)

```
100% tests passed, 0 tests failed out of 73
```

### ci-preprocess 재실측 (`a13-pre.log`, exit=0)

```
100% tests passed, 0 tests failed out of 497
```

같은 로그 159행 — 등록된 안전위반 스위트가 실제로 실행된다:

```
 72/498 Test  #72: XpeErrorSafetyViolationTest.SafetyViolationErrorCodeExists .............   Passed    0.01 sec
```

### 삭제 + 추가 규모 (`a13-delete-diffstat.txt`)

```
 tests/common/test_xpe_common.cpp         | 596 -------------------------------
 tests/common_smoke/CMakeLists.txt        |  10 -
 tests/common_smoke/test_common_smoke.cpp | 104 ------
 tests/common_unit/CMakeLists.txt         |  10 -
 tests/common_unit/test_xpe_common.cpp    | 491 -------------------------
 5 files changed, 1211 deletions(-)
 modules/common/CMakeLists.txt            |  4 ++++
 modules/common/tests/test_xpe_common.cpp | 35 ++++++++++++++++++++++++++++++++
 2 files changed, 39 insertions(+)
```

## 5. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-common | 69/69 PASS (QA-A-29, `../QA-A-29/a29-common.log`) | **73/73 PASS** (`a13-common.log`) | +4 = 이식 2 + 안전위반 2 |
| ci-preprocess | 493/493 PASS (QA-A-22, `../QA-A-22/a22-green2.log`) | **497/497 PASS** (`a13-pre.log`) | +4 = 위와 동일(같은 common 스위트를 함께 돈다) |

두 프리셋 모두 이 워크트리에서 이번 턴에 측정했다. 삭제된 1,211줄은 어떤 빌드에도 들어간 적이 없으므로 테스트 수에 변화를 주지 않는다 — 실측이 그 예상과 일치한다.

## 6. 미검증 (Gaps)

- **카드의 전제 하나가 이미 어긋나 있었다.** 카드는 세 트리가 "루트가 읽지 않는 `tests/CMakeLists.txt` 아래" 있다고 적었지만, **이 트리에 `tests/CMakeLists.txt` 는 존재하지 않는다**(`grep` 종료코드 2 = 파일 없음). #113 3단계가 이미 처리했거나 다른 경로로 사라진 것으로 보인다. 결과적으로 삭제 후 매달린 참조가 없다는 점은 같지만, 그 이유는 카드가 적은 것과 다르다. 어떤 커밋이 언제 지웠는지는 추적하지 않았다.
- **`tests/common`·`tests/common_unit` 은 이번에 재대조하지 않았다.** 고유 커버리지 0 은 QA-A-04/05 의 판정을 그대로 받았고, 이번 턴에 1,087줄을 다시 읽어 확인하지는 않았다. 카드가 그 두 트리를 "확정" 으로 기술했기 때문이지만, **이번 보고서 기준으로는 인용이지 실측이 아니다.**
- **삭제한 파일은 되살릴 수 있으나 확인하지 않았다.** `git rm` 이므로 이력에는 남지만, 실제로 복구해 보지는 않았다.
- **다른 미등록 트리는 조사하지 않았다.** `tests/` 아래에는 아직 `ai_tests`, `e2e_post_pipeline`, `preprocess`, `preprocess_smoke`, `test_data` 가 남아 있다. 이번 카드 범위가 `common*` 셋이라 나머지는 열어보지 않았다.

## 7. 잔여 위험 (Residual risk)

- **이식한 두 단언은 문자열 상수에 의존한다.** `"Invalid input parameter"` 와 `"Safety violation"` 은 사용자에게 보이는 문구라 언젠가 바뀔 수 있고, 그때 이 테스트가 먼저 깨진다. 그것이 이 단언의 목적(문구가 계약이라는 표시)이지만, 문구 변경을 계획하는 쪽에서는 실패 이유를 오해할 수 있다.
- **`XPE_ERR_SAFETY_VIOLATION == -11` 을 못 박은 결과**, 오류 코드 목록 중간에 새 코드를 끼워 넣는 변경은 이 테스트가 막는다. C ABI 상수라 의도된 보호지만, 값 재배치가 필요해지면 이 파일이 관문이 된다.
- **`preprocess_smoke` 등 남은 트리도 같은 상태일 수 있다.** 이번에 확인하지 않았으므로, 빌드에 들어가지 않는 죽은 테스트가 더 있는지는 알 수 없다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| `common_smoke` 12케이스 대조 + 고유 2건 이식 | QA-A-13 |
| 세 트리 삭제 (1,211줄) | QA-A-13 |
| `test_xpe_error_safety_violation.cpp` 판정·등록 | QA-A-13 |
| ci-common / ci-preprocess 재실측 | QA-A-13 |
| `tests/` 남은 트리(ai_tests·preprocess·preprocess_smoke·e2e_post_pipeline) 조사 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a13-common.log` | ci-common 73/73 PASS (exit=0) |
| `a13-pre.log` | ci-preprocess 497/497 PASS (exit=0), 159행에 안전위반 스위트 실행 |
| `a13-delete-diffstat.txt` | 삭제 + 추가 `git diff --stat` |
