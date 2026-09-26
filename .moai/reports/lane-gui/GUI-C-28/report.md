# GUI-C-28 — 알 수 없는 스위치 거부 + 파서 거부→중단 연결 가드 (#136 마감)

- 카드: GUI-C-28 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `de1e837` (미푸시) · 선행 C-27(`d76f066`) 뒤, dev/gui 위에서 바로
- **결과: slnx 재빌드 경고 0 / 오류 0 · dotnet 실패 0 / 통과 167 / 건너뜀 1 / 전체 168** (162 → +6)

---

## 1. 무엇을 닫았는가

C-27 이 **스스로 보고한 사각 두 개**를 닫는다.

| 사각 | 이 카드의 조치 |
|---|---|
| 알 수 없는 `--automation-*` 스위치가 조용히 무시됨 | 거부(`IsValid=false` + 사유) |
| 파서의 거부 판단과 App 의 중단이 갈라져, 연결이 끊겨도 회귀 9건이 전부 통과 | 소스 대조 가드 4건 |

## 2. 구현

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/Services/AutomationArgs.cs` | `--automation-*` 로 시작하는데 인식되지 않는 스위치는 거부. `--automation-` 밖의 인자는 그대로 무시(우리 것이 아니다) |
| `Functional/AutomationArgsTests.cs` | 회귀 2건 추가 |
| `Functional/AutomationRejectionWiringGuardTests.cs` (신규) | 연결 가드 4건 |

스위치 이름의 오타를 값의 오타와 같게 다룬 근거: 둘 다 **실행은 시작되고 리포트는 요청받은 것처럼 보이는데 그 옵션만 아무 일도 하지 않는다.** C-27 이 값 쪽에서 없앤 형태가 이름 쪽에 그대로 남아 있었다.

### 연결 가드가 보는 것

`App.xaml.cs` 에 (a) `AutomationArgs.Parse` 호출, (b) `!IsValid` 분기, (c) `Environment.Exit` 이 있어야 하고, **(c)가 (b)의 본문 안에** 있어야 한다. 세 줄을 따로 세기만 하면 무조건 종료하거나 엉뚱한 곳으로 옮겨진 `Exit` 을 통과시키므로, 분기 본문을 잡아 그 안을 확인한다.

## 3. 회귀 (`step2-regression.log`)

```
통과!  - 실패: 0, 통과: 15, 건너뜀: 0, 전체: 15
```

(`AutomationArgsTests` 11건 + `AutomationRejectionWiringGuardTests` 4건)

| 신규 케이스 | 확인 |
|---|---|
| `UnknownAutomationSwitch_IsRefused` | `--automation-backends Mock` → 거부, 사유에 스위치 이름 |
| `NonAutomationArguments_AreStillIgnored` | `--verbose` 같은 남의 인자는 무시하고 나머지는 정상 파싱 |
| `RefusalWiring_IsPresentInApp` (Theory 3행) | Parse 호출 / `!IsValid` 분기 / `Environment.Exit` |
| `TheStop_SitsInsideTheValidityBranch` | 중단이 분기 **안**에 있음 |

## 4. 반증 2회 — 둘 다 유효

**(A) 스위치 거부를 조용한 무시로 되돌림** (`step3-falsify-a.log`)

```
실패!  - 실패: 1, 통과: 14, 전체: 15
AutomationArgsTests.UnknownAutomationSwitch_IsRefused [FAIL]
```

**(B) `!IsValid` 분기를 무력화** (`step3-falsify-b.log`)

```
실패!  - 실패: 2, 통과: 13, 전체: 15
AutomationRejectionWiringGuardTests.RefusalWiring_IsPresentInApp(pattern: "if\s*\(\s*!\s*\w+\.IsValid\s*\)") [FAIL]
AutomationRejectionWiringGuardTests.TheStop_SitsInsideTheValidityBranch [FAIL]
```

**(B)가 이 카드의 요점이다** — 연결을 끊었는데 **파서 회귀 9건은 전부 통과했다.** C-27 이 예고한 사각이 실재함을 그대로 보여 주고, 가드만이 그것을 잡는다. 둘 다 원복 후 재확인.

## 5. 실행 확인 (카드 3항)

| 실행 | 종료코드 | 리포트 |
|---|---|---|
| `--automation-backend Mock` | **0** | `Passed=true` |
| `--automation-backend Nativ` | **2** | `Passed=false`, `--automation-backend 'Nativ' is not one of Mock \| Native.` |
| `--automation-backends Mock` (스위치 오타) | **2** | `Passed=false`, `--automation-backends is not a recognised automation switch.` |

리포트: `ok-Mock.json`, `rejected-typo.json`, `rejected-unknown-switch.json`. 오타 실행의 exit 2 는 C-27 과 동일 — 무회귀.

## 6. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   167, 건너뜀:     1, 전체:   168
```

Baseline 귀속: C-27 최종 `0/161/1/162`(`GUI-C-27/step5-final.log`) → 증가 6 = 회귀 2건 + 가드 4건(Theory 3행 개별 집계).

**커밋 메시지 정정 기록**: 처음 커밋(`b856785`)의 메시지에 실측 전 추정치 `0/164/1/165 (+3)` 을 적었다. 실제는 `0/167/1/168 (+6)` 이라 `--amend` 로 바로잡았다(`de1e837`). 추정치를 실측처럼 적은 것이 잘못이고, 미푸시 상태라 정정 비용은 없었다.

## 7. 미검증 (Gaps)

- **가드는 텍스트 대조다.** 정규식이 보는 것은 모양이지 동작이 아니다 — `Environment.Exit(0)` 으로 바꾸면 통과한다(종료코드 값은 보지 않는다). 실제 종료코드는 §5 의 실행 관측 2건이 유일한 증거다.
- **`App.OnStartup` 의 적용부에는 여전히 회귀가 없다.** 파서가 준 값을 정적 프로퍼티로 옮기는 부분은 가드도 회귀도 덮지 않는다.
- 거부가 `--automation-report` 보다 먼저 일어나는 경우(리포트 없이 종료코드만)는 이번에도 실행으로 보지 않았다.
- 알 수 없는 스위치가 **값을 소비한다**는 점은 그대로다(`--automation-foo bar` 에서 `bar` 를 값으로 먹고 거부). 거부하므로 결과는 같지만, 오류 메시지는 스위치 이름만 말한다.
- CI 실행 결과는 아직 없다.

## 8. 잔여 위험 (Residual risk)

- **가드의 정규식이 리팩터링에 약하다.** `if (!parsed.IsValid)` 를 `if (parsed.Error is not null)` 로 바꾸면 동작은 같은데 가드가 실패한다 — 거짓 경보다. 실패 메시지에 "왜 이 가드가 있는가" 를 적어 두어 다음 사람이 판단할 수 있게 했지만, 형태를 고정하는 비용은 남는다.
- 두 가드(C-25 래퍼, 이번 연결)가 같은 방식으로 늘고 있다. 소스 대조는 값싸지만 **동작을 재지 않는다** — 진짜 확인은 실행 관측이고, 그것은 여전히 사람이 자동화를 돌릴 때만 일어난다.
- `AcceptedBackendModes` 와 `XpeBackendFactory` 의 인식 문자열이 별도로 관리되는 문제(C-27 §7)는 이번에도 남아 있다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test … --filter "FullyQualifiedName~AutomationArgsTests|FullyQualifiedName~AutomationRejectionWiringGuardTests"
ImageProcTest.exe --automation-raw … --automation-report … --automation-backend {Mock|Nativ}
ImageProcTest.exe --automation-raw … --automation-report … --automation-backends Mock   # 스위치 오타
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
