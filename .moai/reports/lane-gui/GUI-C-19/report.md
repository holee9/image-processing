# GUI-C-19 — 알림 큐 유실 알림 표시 + 회귀 (#110, SRS-ALERT-007)

- 카드: GUI-C-19 · Refs #110 · 커밋 `a24f69b` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `4b723cd` (main 병합, `.claude` 변경 0건)
- **결과: 앱 재빌드 경고 0 / 오류 0 · dotnet 실패 0 / 통과 135 / 건너뜀 1 / 전체 136**

---

## 1. 소비 경로 실측 (카드 1항) — **네이티브 알림 "메시지" 를 읽는 앱 경로는 없다**

| 후보 | 무엇을 하는가 | 판정 |
|---|---|---|
| `clients/ImageProcTest/Backends/RealXpeCommonBackend.cs:30` | `xpe_get_pending_alert_count()` 를 읽어 `$"Alerts: pending={n}"` 진단 문자열에 넣는다 | **개수만** — 메시지 미조회 |
| `gui/ImageProcTest/Services/RealXpeBackend.cs:73` | `_alerts` 는 이 클래스의 `List<AlertEntry>`(`:31`)이고 앱 자체 이벤트로 채운다 | **네이티브 큐 무관** |
| `gui/…/MainWindowViewModel.cs:898-905` `DrainBackendTelemetry` | `_backend.GetAlert(i)` 를 UI 컬렉션에 넣는다 — 그 backend 가 위의 것 | 〃 |
| `clients/ImageProcTest.IntegrationTests/**` | `xpe_get_pending_alert` 를 실제로 호출하는 유일한 곳 | **테스트 전용** |

즉 `xpe_get_pending_alert` 로 **메시지 본문을 읽어 사용자에게 보여주는 코드는 앱에 없다.**
카드 지시대로 **표시 경로는 신설하지 않고** 2·3 항만 수행했다.

### 부수 실측 — 한국어 리소스가 없다

카드는 "한국어 리소스 있으면 거기에" 라는 조건을 달았다. 실측:

- `.resx` 파일 **0건** (`clients/**`, `gui/**`)
- 한국어 문자열을 포함한 소스/XAML **0건**
- `MainWindow.xaml` 의 사용자 문구는 전부 영어 (`Initialize Backend`, `Clear Logs`, `_File` …)

그래서 문구를 **기존 등록(영어)에 맞췄다.** 전부 영어인 UI 에 이 문장만 한국어로 넣으면
일관성이 깨진다 — 한국어화는 리소스 도입을 포함하는 별개 결정이다(§6).

## 2. 구현 (카드 2항)

`clients/ImageProcTest/Services/AlertDisplayFormatter.cs` — 순수 문자열 입출력.

| 입력 | 출력 |
|---|---|
| `alert queue overflow: 7 alert(s) dropped` | `7 alerts were dropped because the alert queue overflowed.` |
| `alert queue overflow: 1 alert(s) dropped` | `1 alert was dropped …` (단수형) |
| `alert queue overflow:` (건수 없음) | **원문 그대로** |
| 무관한 메시지 | **원문 그대로** |
| `null` | `""` |

설계에서 지킨 것:

- **심각도를 바꾸는 통로를 두지 않았다.** §5.17 이 "다른 Error 와 같이 렌더" 를 요구하므로,
  낮출 수단이 존재하는 것 자체가 계약 위반의 여지다. 테스트가 `Severity` 이름을 가진 공개
  멤버가 없음을 단언한다 — 나중에 누가 추가하면 **테스트가 깨진다**
- **건수를 지어내지 않는다.** 접두는 있는데 숫자가 없으면 원문을 보여준다. 숫자를 만들거나
  알림을 버리면 **"유실이 있었다" 는 사실 자체가 사라진다** — 그쪽이 더 나쁜 실패다
- **접두 판정은 ordinal·대소문자 구분.** `Alert queue overflow:` 나 선행 공백은 계약의 접두가
  아니므로 통과시키지 않는다(테스트로 고정)
- `IsOverflowAlert` 는 배지 용도로만 제공하고, 주석에 "숨기거나 낮추라는 뜻이 아니다" 를 명시

## 3. 회귀 10건 (카드 3항)

| 묶음 | 건수 |
|---|---|
| 건수 파싱 정상 (7 / 1 / 128, 단수형 포함) | 3 |
| 접두만 있고 건수 없음 → 원문 | 2 |
| 무관 메시지 → 원문 (대소문자 변형·선행 공백 포함) | 3 |
| 심각도 통로 부재 + 식별 헬퍼 동작 | 1 |
| null → 빈 문자열 | 1 |

네이티브 없이 문자열 입력만으로 돈다 — QA-A-28 의 진행 여부와 무관하다.

## 4. 반증 — 2회, 첫 번째는 **반증에 실패했다**

정직하게 둘 다 적는다.

**1차 (무효).** `if (!match.Success || !int.TryParse(...))` 의 앞 조건을 `false` 로 바꿨다.
테스트가 **그대로 통과**했다. 변형이 무력했기 때문이다 — `match.Success` 가 false 면
`match.Value` 는 `""` 이고 `int.TryParse("")` 가 실패해 뒤 조건이 여전히 폴백을 탄다.
**"반증이 통과했다" 를 "가드가 튼튼하다" 로 읽으면 안 된다.** 그것은 변형이 아무것도 바꾸지
못했다는 뜻이고, 이 레인이 반복해 잡아온 "관측처럼 보이는 비관측" 이다. 채택하지 않았다.

**2차 (유효).** 폴백 분기를 지워 `dropped = 0` 으로 **건수를 지어내게** 했다:

```
실패 2 / 통과 8 / 전체 10

OverflowAlert_WithoutCount_FallsBackToRawText(nativeMessage: "alert queue overflow: alert(s) dropped")
  Expected: "alert queue overflow: alert(s) dropped"
  Actual:   "0 alerts were dropped because the alert q"…
```

정확히 폴백 케이스 **2건만** 실패한다. 로그: `step3-falsify.log`. 원복 후 재확인
(`step4-final.log`).

### 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   135, 건너뜀:     1, 전체:   136
```

136 = C-17 의 126 + 이번 10. 건너뜀 1은 C-12 부터의 `-15` 문자열 건.

## 5. 미검증 (Gaps)

- **네이티브가 정확히 그 메시지를 내는지 확인하지 않았다.** 이 회귀는 §5.17 이 고정한
  문자열을 직접 입력한다. 계약과 구현이 어긋나면 이 테스트는 통과한 채로 앱이 틀린다 —
  그 접합부는 QA-A-28 소유이고, 양쪽이 붙은 뒤 **네이티브가 실제로 만든 메시지**로 한 번
  확인하는 케이스가 별도로 필요하다(§6 후보)
- **포맷터를 부르는 곳이 아직 없다.** 표시 경로 신설이 카드 범위 밖이라, 이 유닛은 현재
  테스트에서만 호출된다. 소비 경로가 생기기 전까지 **사용자는 이 문구를 볼 수 없다**
- 단수/복수 외의 언어 규칙(0건, 매우 큰 N 의 자릿수 구분)은 다루지 않았다
- WPF 앱을 실행해 알림 목록을 보지 않았다 — 빌드만 확인
- CI 실행 결과는 아직 없다

## 6. 잔여 위험 / 후속 후보

- **포맷터의 배치.** `clients/ImageProcTest`(네이티브 알림 API 를 만지는 앱)에 뒀다.
  실제 알림 UI 는 `gui/ImageProcTest` 에 있는데 그쪽 `AlertEntry` 는 앱 자체 알림이라
  네이티브 큐와 연결돼 있지 않다. **소비 경로를 만들 때 이 배치가 맞는지 다시 볼 필요가 있다**
  — 두 앱 중 어디가 네이티브 알림을 표시할지가 아직 정해지지 않았다
- **한국어 문구는 리소스 도입과 함께 결정할 사안이다.** 지금 앱은 전부 영어이고 `.resx` 가
  없다. 카드의 "한국어 리소스 있으면" 조건이 성립하지 않았음을 보고한다
- 후속 후보 2건: (a) 소비 경로 신설 + 포맷터 배선(어느 앱인지 판정 필요),
  (b) QA-A-28 완료 후 네이티브가 만든 실제 메시지로 계약 접합 확인 1건

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → 4b723cd
grep -rn "xpe_get_pending_alert\|GetAlert" --include=*.cs clients gui
find clients gui -name "*.resx"                          # → 0건
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --filter "FullyQualifiedName~AlertDisplayFormatterTests"   # 반증
```
