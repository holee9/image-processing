# GUI-C-27 — 자동화 인자 파싱 분리 + `--automation-backend` 값 검증 (#136 잔여)

- 카드: GUI-C-27 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `d76f066` (미푸시) · 선행 C-26(`36badfe`) 뒤, dev/gui 위에서 바로
- **결과: slnx 재빌드 경고 0 / 오류 0 · dotnet 실패 0 / 통과 161 / 건너뜀 1 / 전체 162** (153 → +9)

---

## 1. 왜 이 카드가 필요했는가

C-26 은 `--automation-backend` 를 **회귀 없이** 내보냈고, 그 사실을 그때 보고했다. 파싱이 `App.ParseAutomationArgs`(WPF `Application` 멤버) 안에 있어 테스트 어셈블리가 링크할 수 없었기 때문이다. 이 커밋이 그 구멍을 닫는다.

## 2. 구현

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/Services/AutomationArgs.cs` (신규) | WPF 비의존 정적 파서 `AutomationArgs.Parse(string[])` — C-24 의 `InvokeWithDrain` 과 같은 분리 |
| `gui/ImageProcTest/App.xaml.cs` | 파싱 로직 제거, **호출하고 적용만** 한다. 거부 시 리포트 기록 + 종료코드 2 |
| `clients/…IntegrationTests.csproj` | `AutomationArgs.cs` 링크 — 이제 CI 에서 돈다 |
| `Functional/AutomationArgsTests.cs` (신규) | 회귀 9건 |

### 검증 규칙

- `--automation-backend` 는 `Mock|Native`(대소문자 무시)만 받고 **표준 철자로 정규화**한다. 그 밖의 값은 거부.
- 값 없는 스위치(`--automation-backend` 가 마지막 인자), 정수 아닌 `width`/`height` 도 같은 이유로 거부.
- **거부는 리포트 경로만 남기고 나머지 인자를 버린다.** 반쯤 적용된 명령줄로 실행을 시작하는 편이 더 나쁘다 — 요청받은 실행처럼 보이기 때문이다.
- 거부가 `--automation-report` 보다 먼저 일어나면 쓸 곳이 없으므로 **종료코드만 신호**가 된다(§5).

거부가 필요한 이유는 관측된 형태다: 예전 동작에서 `Nativ` 는 그대로 설정에 흘러 `XpeBackendFactory` 가 조용히 Mock 으로 떨어뜨렸고, 리포트는 출처를 `arg` 라고 적었다. **잰 대상이 요청과 다른데 리포트만 맞아 보인다.**

## 3. 회귀 9건 (`step2-regression.log`)

```
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9
```

| 케이스 | 확인 |
|---|---|
| `BackendSwitch_AcceptsBothModes_AndNormalisesTheSpelling` (Theory 4건) | `Mock`/`Native`/`native`/`MOCK` → 표준 철자 |
| `BackendSwitch_RejectsAnUnknownValue_RatherThanFallingBackToMock` | `Nativ` 거부, `BackendMode` null, 사유에 값과 허용 목록 포함 |
| `NoBackendSwitch_LeavesTheDecisionToTheSettingsFile` | 인자 없음 → 오류 아님, 파일이 결정 |
| `SwitchWithoutItsValue_IsRefused` | 값 없는 스위치 거부 |
| `Rejection_KeepsTheReportPath_AndDropsTheRunParameters` | 거부 시 리포트 경로 보존, `RawPath` 버림, `IsAutomationMode=false` |
| `NonIntegerDimension_IsRefused` | `--automation-width 1o24` 거부 |

### 반증 (`step3-falsify.log`) — 값 검증 제거(옛 동작 복원)

```
실패!  - 실패: 2, 통과: 7, 전체: 9
BackendSwitch_RejectsAnUnknownValue_RatherThanFallingBackToMock [FAIL]
Rejection_KeepsTheReportPath_AndDropsTheRunParameters [FAIL]
```

카드는 "오타 케이스만 실패" 를 예상했고 **2건이 실패했다 — 둘 다 오타 케이스다**(두 번째도 `Nativ` 를 입력으로 쓴다). 검증이 없으면 두 단언 모두 성립하지 않으므로 예상과 어긋나지 않는다. 원복 후 재확인.

## 4. 실행 확인 (카드 4항)

| 실행 | 종료코드 | 리포트 |
|---|---|---|
| `--automation-backend Mock` (정상) | **0** | `Passed=true`, `BackendMode=Mock`(`arg`), `v0.0.0-mock` |
| `--automation-backend Nativ` (오타) | **2** | `Passed=false`, `Error=--automation-backend 'Nativ' is not one of Mock \| Native.` |

리포트: `ok-Mock.json`, `rejected-typo.json`. 정상 실행은 C-26 과 같은 결과 — 무회귀.

## 5. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   161, 건너뜀:     1, 전체:   162
```

Baseline 귀속: C-26 최종 `0/152/1/153`(`GUI-C-26/step3-final.log`) → 증가 9 = 신규 회귀 9건(Theory 4건이 개별 항목으로 집계).

## 6. 미검증 (Gaps)

- **`App.OnStartup` 의 적용부에는 회귀가 없다.** 테스트가 덮는 것은 `AutomationArgs.Parse` 까지다. 파서가 준 값을 App 이 정적 프로퍼티에 옮기는 부분과 거부 시 리포트 기록·`Environment.Exit(2)` 는 여전히 WPF 안이라, §4 의 실행 관측 2건이 유일한 증거다.
- **거부가 `--automation-report` 보다 먼저 일어나는 경우를 실행으로 보지 않았다.** 그때는 리포트가 없고 종료코드만 남는데(설계된 동작), 유닛에서 `ReportPath` 보존만 확인했다.
- **`Environment.Exit` 의 부작용을 확인하지 않았다.** WPF 시작 도중 프로세스를 즉시 끝내므로 정리 경로가 돌지 않는다 — 자동화 거부 상황에서는 의도한 바지만, 열린 핸들 등은 관측하지 않았다.
- 알 수 없는 `--automation-*` 스위치(예: `--automation-foo bar`)는 **조용히 무시**된다. 값 소비는 하지만 거부하지는 않는다.
- CI 실행 결과는 아직 없다. leader 가 갱신한 CI 잡(`--automation-backend Mock` + 출처 단언)과의 상호작용도 미확인.

## 7. 잔여 위험 (Residual risk)

- **거부 판단이 파서에 있고 실행 중단은 App 에 있다.** 둘의 연결(`if (!parsed.IsValid) … Exit`)이 끊겨도 회귀는 통과한다 — 파서 단언만 보기 때문이다. C-25 의 래퍼 누락 가드와 같은 종류의 사각이고, 같은 방식(소스 대조)으로 막을 수 있지만 이번엔 넣지 않았다.
- 허용 목록 `AcceptedBackendModes` 는 `XpeBackendFactory` 가 실제로 인식하는 문자열과 **별도로 관리된다.** 한쪽이 늘면 다른 쪽이 낡는다 — 지금은 둘 다 Mock/Native 2개뿐이다.
- 종료코드 2 는 이 앱의 관례일 뿐 CI 가 그 값을 구분해 쓰지는 않는다(0 이 아니면 실패).

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test … --filter "FullyQualifiedName~AutomationArgsTests"     # 회귀·반증
ImageProcTest.exe --automation-raw … --automation-report … --automation-backend {Mock|Nativ}
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
