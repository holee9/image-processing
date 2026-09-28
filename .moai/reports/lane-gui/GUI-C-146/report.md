# GUI-C-146 (#197) — `AC-10` 은 `027`·`028` 로 **완전히 덮인다**. 그리고 번호는 존재한 적 없는 것이 아니었다

상태: **판정 완료.** 세울 요구는 **없다.** 매트릭스에서 세 줄을 삭제했고, 그 과정에서 **`ac` 열의 어긋남 6건**을 찾아 3건을 고쳤다(§4).

---

## 1. 판정 — `AC-10` 은 `027`·`028` 로 이미 덮인다

SPEC 이 직접 답한다. `.moai/specs/SPEC-XPE-GUI-IT/spec.md:517`:

```
### AC-10: Alert Queue Never Crashes on Empty (REQ-GUI-IT-027, 028)

`AlertTests`:
- empty queue에서 count == 0, fetch(0)는 `INVALID_INPUT`
- clear_alerts 3회 연속 호출 무예외
```

**`AC-10` 의 정의가 곧 `027`·`028` 이다.** 본문 두 줄이 그 두 요구이고, 그 밖에 덮이지 않은 항목은 없다.
따라서 카드 §1 표의 첫째 갈래 — **세 줄 삭제**이고, **세울 요구는 없다.**

`032`·`033`·`034` 는 되살리지 않았다. 다음 빈 번호로도 세우지 않았다 — 세울 내용이 없기 때문이다.

## 2. 시험 2건이 실제로 무엇을 단언하는가 — `AC-10` 본문과 1:1

`Lifecycle/AlertCallbackTests.cs`:

| 시험 | 실제 단언 | 대응 |
|---|---|---|
| `AlertQueue_Empty_CountIsZeroAndFetchReturnsInvalidInput` | `clear_alerts()` 뒤 `get_pending_alert_count() == 0`, `get_pending_alert(0, …) == INVALID_INPUT` | `AC-10` 첫째 줄 = **`027`** |
| `ClearAlerts_CalledRepeatedly_DoesNotThrow` | `clear_alerts()` 3회 반복에 예외 없음(`Record.Exception` 이 null), 그 뒤 count 여전히 0 | `AC-10` 둘째 줄 = **`028`** |

**하나가 둘을 겸하지도, 하나가 빠지지도 않았다.** 요구 둘 · 시험 둘 · `AC-10` 본문 두 줄이 정확히 맞물린다.
클래스 doc-comment 도 이미 `Covers REQ-GUI-IT-027, REQ-GUI-IT-028, AC-11` 로 **요구 둘만** 적고 있다
(단, 거기 적힌 `AC-11` 은 SPEC 기준 `AC-10` 이다 — §4 와 같은 어긋남. 코드 주석이므로 함께 고쳤다).

**계측 주의**: 이 파일에 `[Fact]`·`[Theory]` 는 **0건**이다. 두 시험은 `[SkippableFact]` 다.
`[Fact]` 로 세면 "0건" 이 나오고 "시험이 없다" 로 오독된다 — 세는 패턴에 `SkippableFact` 를 포함해야 2건이 나온다.

## 3. 번호의 출처 — 존재한 적 없는 것이 아니라 **다른 문서에 다른 주제로** 있다

카드는 "존재한 적 없는 번호" 로 기술했다. **그 부분을 정정한다.**

`docs/post-processing/xpe/RTM-GUI-001_Requirements_Traceability_Matrix.md:75-77`:

| ID | RTM 의 내용 |
|---|---|
| `REQ-GUI-IT-032` | Detector-firmware callback — Configure Default *(out of XPE scope; ABI smoke only)* |
| `REQ-GUI-IT-033` | Detector-firmware callback — Configure Invalid JSON *(out of XPE scope)* |
| `REQ-GUI-IT-034` | Detector-firmware callback — Poll Empty Queue *(out of XPE scope)* |

즉 매트릭스는 번호를 지어낸 것이 아니라, **범위 밖(out of XPE scope)으로 표시된 RTM 번호를 가져와 알림 큐 시험에 붙였다.**
주제도 다르다 — 검출기 펌웨어 콜백이지 알림 큐가 아니다. 그래서 "인용 수정" 이 아니라 **삭제**가 맞다.

두 번째 씨앗: `spec.md:483` 의 `### AC-4: All 18 PInvoke Symbols Have a Functional Test (REQ-GUI-IT-020~034)` —
SPEC 안에서 `032`~`034` 가 나타나는 **유일한 자리**이고, `####` 정의는 없는 범위다. 범위 표기가 정의를 만들어 주지 않는다.
**`AC-4` 의 범위를 `020~031` 로 좁힐지는 SPEC 소유자 판단**이다(제안만).

## 4. 전수 — 정의 없는 ID 는 이 셋뿐, 그러나 `ac` 열이 6건 어긋났다

### 4.1 정의 없는 ID (카드 §3)

| 항목 | 실측 |
|---|---|
| 매트릭스 행 수(고치기 전) | **36** |
| SPEC `####` 정의 수 | **36** |
| 정의 없는 ID | **`032`·`033`·`034` — 이 셋뿐** |
| **대조군**: 같은 검사가 살아 있는 ID 를 통과시키는가 | `001`~`005` 전부 정의 있음으로 판정 — 검사가 눈먼 것이 아니다 |
| **반대 방향** | **`063`·`064`·`065` 는 SPEC 에 정의가 있으나 매트릭스에 없다** — 카드 범위 밖, 보고만 |

### 4.2 `ac` 열 대조 — 덤으로 나온 것

행마다 SPEC 의 `### AC-n:` 머리말이 그 요구를 열거하는지 대조했다. **27건 일치 / 6건 불일치**
(일치 27건이 대조군 역할을 한다 — 파서가 양쪽을 읽고 있다는 관측).

| 요구 | 매트릭스 | SPEC 머리말 | 조치 |
|---|---|---|---|
| `027`·`028` | `AC-11` | `AC-10` (알림 큐) | **고쳤다 → `AC-10`** |
| `029`·`030`·`031` | `AC-12` | `AC-11` (로그 하위계통) | **고쳤다 → `AC-11`** |
| `060`·`061`·`062` | `AC-14` | `AC-13` (P1A 선택 시험) | **고쳤다 → `AC-13`** |
| `005`·`010`·`043` | `AC-4`·`AC-7`·`AC-2` | **어떤 AC 머리말도 열거하지 않음** | **안 고쳤다** — 어느 AC 에 속하는지는 SPEC 소유자 판단. 보고만 |

세 줄만 지우면 **`AC-10` 을 가리키는 행이 0이 되고** `027`·`028` 은 로그 AC 를 가리킨 채 남는다.
그래서 삭제와 `ac` 열 정정을 같은 커밋에 넣었다. **시험은 더하지도 지우지도 않았다**(카드 §4).

## 5. 도구 — `PYTHONIOENCODING` 없이 돌지만, **이 결함에는 구조적으로 눈멀어 있다**

| 확인 | 실측 |
|---|---|
| `PYTHONIOENCODING` 현재 값 | `unset` (이 콘솔) |
| 내 HEAD 의 `check_req_citations.py` | `unset` 상태로 **정상 종료**(exit 0), `UnicodeEncodeError` 없음. 출력 `no new orphans (20 known orphan ids, 73 citations)` |
| `311718f` 판(리더가 고친 것) | `unset` 상태로 **정상 동작**. 인코딩 오류 없음 |
| `311718f` 이 내 HEAD 에 있는가 | **없다** (`git merge-base --is-ancestor` → false). 그래서 내 기본 실행은 옛 판이고 수치도 20/73 으로 리더의 14/58 과 다르다 |

**눈먼 이유**: `tools/docs/req_citation_baseline.json:7-11` 의 `"GUI-IT": ["032","033","034"]` — 이 셋이
**baseline 허용 목록에 등재되어 있다.** 그래서 도구는 "no new orphans" 로 통과시킨다. 도구가 못 본 것이 아니라 **보고도 넘기게 되어 있었다.**

**양성 대조군** (도구가 `clients/` 를 실제로 읽는지): 없는 번호 `REQ-GUI-IT-099` 를
`clients/…/AlertCallbackTests.cs` 에 한 줄 넣고 돌렸더니 **잡았다** —
`REQ-GUI-IT-099 is cited at clients/…/AlertCallbackTests.cs:67 but no SPEC … defines it`. 즉시 원복했고 `git diff` 로 확인했다.
파일 확장자 필터는 없고 `git ls-files` 전체를 보므로 `requirement-matrix.json` 도 검색 범위 안이다.

**제안**: 이 세 줄을 지웠으므로 baseline 의 `"GUI-IT"` 항목은 **빈 배열로 내릴 수 있다**(도구 docstring 이 "내리는 것은 항상 허용" 이라 적음).
`tools/` 는 리더 소유이므로 제가 고치지 않았다.

**범위 밖 관측 1건**: `311718f` 판이 새 orphan 을 하나 냈다 —
`REQ-P1A-066 is cited at modules/preprocess/CMakeLists.txt:424 but no SPEC … defines it`.
`modules/` 는 Lane A·B 소유이므로 손대지 않았다. **리더에게 전달한다.**

## 6. 바꾼 것

| 파일 | 변경 |
|---|---|
| `clients/ImageProcTest.IntegrationTests/Resources/requirement-matrix.json` | `032`·`033`·`034` 행 **삭제**(36 → 33), `027`·`028` → `AC-10`, `029`~`031` → `AC-11`, `060`~`062` → `AC-13`. JSON 파싱으로 유효성 확인 |
| `clients/ImageProcTest.IntegrationTests/Lifecycle/AlertCallbackTests.cs` | doc-comment 의 `AC-11` → `AC-10`, 머리 주석 `// AC-11:` → `// AC-10:` (SPEC 기준 정정). **시험 코드 무변경** |

SPEC · 시험 추가/삭제 · 번호 `032`~`034` 재사용 — **모두 하지 않았다**(카드 §4).

## 7. 미검증 / 잔여 위험

- **`dotnet test` 를 돌리지 않았다.** 다만 이 매트릭스를 읽는 코드는 없다 —
  `clients/`·`gui/` 의 `*.cs`·`*.csproj` 전체에서 `requirement-matrix` 를 검색한 결과 **일치 1건**이고,
  그것은 `ImageProcTest.IntegrationTests.csproj:36` 의 `<Content Include=… CopyToOutputDirectory>` 뿐이다(복사만 한다).
  단정하는 시험이 없으므로 행 수 36 → 33 이 시험을 깨뜨릴 경로가 보이지 않는다 —
  **다만 이것은 검색 결과이고 실행 관측은 아니다**
- `005`·`010`·`043` 이 어느 AC 에 속해야 하는지 — SPEC 소유 판단, 미해결
- `AC-4` 범위 `020~034` 를 좁힐지 — SPEC 소유 판단, 제안만
- RTM 의 `032`~`034`(범위 밖 표시) 를 어떻게 처리할지 — `docs/` 는 리더 소유
- baseline 의 `"GUI-IT"` 항목 비우기 — `tools/` 는 리더 소유
- `REQ-P1A-066` orphan — `modules/` 소유 밖
- `311718f` 이 내 워크트리에 없어 리더의 14/58 수치를 **내 HEAD 에서 재현하지 못했다**

---

Refs #197
