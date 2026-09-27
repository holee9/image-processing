# QA-A-30 검증 보고서 — `xpe_error.h` 알림 API Doxygen 정정 (#133, 문서만)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-30 (`.moai/lanes/pre/inbox/QA-A-30.md`)
- 대상 파일: `modules/common/include/xpe/common/xpe_error.h` (카드 경로 정정 — §2)
- 코드 변경 없음. 주석만.

## 1. 주장 (Claim)

1. 카드가 지목한 **2건**(truncation, severity NULL)을 구현에 맞게 고쳤다.
2. 같은 블록을 구현과 대조해 **3건을 추가로** 찾아 함께 고쳤다 — 오류 문자열, `xpe_alert_push` 의 truncation·NULL 서술, 그리고 QA-A-28 이 바꾼 큐 동작이 문서에 반영돼 있지 않던 부분.
3. 문서화되지 않았던 반환 경로 **2건**(`msgLen == 0` → `INVALID_INPUT`, `BUFFER_TOO_SMALL` 자체)을 `@return` 에 추가했다.
4. ci-common **경고 0, 69/69 PASS**, ci-preprocess **554/554 PASS**. 코드를 건드리지 않았으므로 회귀 0이 예상이었고 실측이 일치한다.

## 2. 카드 경로 정정

카드가 적은 `modules/common/include/xpe/xpe_error.h` 는 **존재하지 않는다.** 실제 경로는 `modules/common/include/xpe/common/xpe_error.h` 다. 줄 번호(113/116)도 실제로는 114/117 이었다. 파일을 찾아 대조표를 실측 줄 번호로 다시 작성했다.

## 3. 대조표 — 주석 ↔ 구현

| # | 헤더 주석 (수정 전) | 구현 | 판정 |
|---|---|---|---|
| 1 | `:87` `xpe_error_string` — 미인식 코드에 `"Unknown error code"` 반환 | `xpe_common.cpp:345` `default: return "Unknown error";` | **불일치** — 문자열이 다르다. 정확히 비교하는 호출자는 실패한다 |
| 2 | `:114` 메시지가 `msgLen - 1` 보다 길면 **잘라서** 널 종료 | `xpe_common.cpp:300-301` `if (e.message.size() + 1 > msgLen) return XPE_ERR_BUFFER_TOO_SMALL;` | **불일치** — 자르지 않고 거부한다. 아무것도 쓰지 않는다 |
| 3 | `:117` `severity` 는 **NULL 이어도 된다** | `xpe_common.cpp:290` `if (!msg \|\| msgLen == 0 \|\| !severity \|\| index < 0) return XPE_ERR_INVALID_INPUT;` | **불일치** — 필수 out-parameter 다. NULL 이면 거부 |
| 4 | `:118-119` `@return` 에 `XPE_OK` 와 `INVALID_INPUT` 만 | 같은 줄 — `BUFFER_TOO_SMALL` 도 반환 | **누락** — api-spec §5.10 의 오류 코드 목록과도 어긋난다 |
| 5 | (없음) `msgLen == 0` 의 동작 | `:290` `INVALID_INPUT` | **누락** |
| 6 | `:145-146` `xpe_alert_push` — 내부 버퍼보다 길면 **잘림**, NULL **금지** | `xpe_common.cpp:96-98` `e.message = msg ? msg : "";` (`std::string`) | **불일치 2건** — 자르지 않고(가변 길이 문자열), NULL 은 빈 메시지로 허용된다 |
| 7 | `xpe_get_pending_alert_count` — 큐에 쌓인 알림 수 | QA-A-28 이후: 오버플로가 있었으면 합성 유실 알림 1건이 포함되고 슬롯 하나를 차지한다 | **오래됨** — 새 동작 미반영 |
| 8 | `xpe_clear_alerts` — 호출 후 count 0 | QA-A-28 이후: 누적 유실 카운터도 함께 0으로 되돌린다 | **오래됨** — 새 동작 미반영 |

수정 후 각 항목은 구현 줄을 근거로 서술하며, 5·8 은 `api-spec.md` §5.17 을 참조로 달았다.

**`severity` 항목의 성격**: 이것은 단순한 문서 오류가 아니다. "NULL 이어도 된다" 를 믿고 `severity` 를 넘기지 않는 호출자는 **모든 호출에서 `XPE_ERR_INVALID_INPUT` 을 받는다.** P/Invoke 로 이 헤더를 읽어 시그니처를 만드는 C# 쪽(SRS-ALERT-006)에 특히 위험한 서술이었다.

## 4. 증거 (Evidence)

### 변경 규모 (`a30-diffstat.txt`)

```
 modules/common/include/xpe/common/xpe_error.h | 43 +++++++++++++++++++--------
 1 file changed, 31 insertions(+), 12 deletions(-)
```

헤더 한 개, 주석만. 선언·시그니처·매크로는 건드리지 않았다.

### ci-common (`a30-common.log`, exit=0)

```
100% tests passed, 0 tests failed out of 69
```

`grep -c "warning C"` = **0** — 카드가 요구한 "경고 0" 을 실측했다.

### ci-preprocess (`a30-pre.log`, exit=0)

```
100% tests passed, 0 tests failed out of 554
```

## 5. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-common | 69/69 PASS (QA-A-34, `../QA-A-34/a34-common.log`) | **69/69 PASS** (`a30-common.log`) | 없음 |
| ci-preprocess | 554/554 PASS (QA-A-34, `../QA-A-34/a34-item4b.log`) | **554/554 PASS** (`a30-pre.log`) | 없음 |

카드는 "63/69 ctest 무회귀" 라고 적었으나 현재 이 프리셋의 케이스 수는 **69** 다(A-13·A-14 가 추가, A-24 가 제거한 결과). 무회귀라는 요구는 충족했고, 숫자는 실측값으로 적는다.

## 6. 미검증 (Gaps)

- **api-spec §5.9 / §5.11 / §5.16 과의 전면 대조는 하지 않았다.** §5.10(`xpe_get_pending_alert`)의 오류 코드 목록만 확인했다. 다른 절이 헤더와 어긋나는지는 모른다.
- **`xpe_error.h` 의 나머지 블록**(오류 코드 매크로 주석, `XpeAlertSeverity` enum 주석)은 이번 대조 범위 밖이다. 카드가 "같은 블록" 으로 한정했다.
- **C# 쪽 P/Invoke 선언이 잘못된 문서를 따라 작성돼 있는지 확인하지 않았다.** `severity` 를 NULL 로 넘기는 호출자가 실제로 있는지 grep 하지 않았다 — Lane C 소유다. 있다면 그 코드는 이미 깨져 있다.
- **문서 변경이므로 실행으로 검증할 대상이 없다.** 대조표의 "판정" 은 구현 소스를 읽어 내린 것이고, 각 불일치를 재현하는 테스트를 새로 쓰지는 않았다. 다만 3·4·5 는 QA-A-28 의 `test_alert_queue_overflow.cpp` 와 QA-A-14 의 프로브가 이미 실행으로 덮고 있다.
- **Doxygen 을 실제로 생성해 보지 않았다.** 주석 문법 오류가 있어도 컴파일은 통과한다.

## 7. 잔여 위험 (Residual risk)

- **이미 잘못된 문서를 믿고 작성된 코드가 있을 수 있다.** 특히 `severity` NULL 허용 서술은 모든 호출을 실패시키는 형태라, 그 서술을 따른 호출자는 이미 동작하지 않는다. 이 카드는 문서를 고쳤을 뿐 그런 호출자를 찾지 않았다.
- **`"Unknown error"` 문자열을 못 박은 문서가 이제 계약이 된다.** 구현이 문자열을 바꾸면 문서가 다시 어긋난다. 문자열을 정확히 비교하는 테스트는 QA-A-13 이 이식한 `ErrorStringForInvalidInputHasExactText` 하나뿐이고, 미인식 코드 쪽은 값을 못 박지 않는다.
- **QA-A-28 의 큐 동작 서술이 헤더와 api-spec 두 곳에 중복된다.** 한쪽만 갱신되면 다시 어긋난다. 헤더는 api-spec §5.17 을 참조로 달아 완화했지만 서술 자체는 양쪽에 있다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| truncation / severity NULL 정정 (카드 지목 2건) | QA-A-30 |
| 같은 블록 추가 불일치 3건 + 누락 2건 | QA-A-30 |
| ci-common 경고 0 + 무회귀, ci-preprocess 무회귀 | QA-A-30 |
| api-spec §5.9/§5.11/§5.16 전면 대조 | 신규 카드 필요 |
| `severity` NULL 을 넘기는 C# 호출자 존재 여부 | Lane C 확인 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a30-common.log` | ci-common 69/69 PASS, 경고 0 (exit=0) |
| `a30-pre.log` | ci-preprocess 554/554 PASS (exit=0) |
| `a30-diffstat.txt` | `git diff --stat` — 헤더 1개, +31/−12 |
