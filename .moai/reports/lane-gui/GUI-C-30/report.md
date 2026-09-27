# GUI-C-30 — Runtime 버전 라벨 추가 + S-05 원문 복원 (XPE-GUI-E2E-001 §4.1)

- 카드: GUI-C-30 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `fb2f991` (미푸시) · 선행 `git merge origin/main` → `a959497`(C-29 병합 + ci.yml E2E 잡 포함)
- **결과: 스모크 5/5 (합 686 ms, 게이트 30 s) · 자동화 Mock 무회귀 · slnx 0/0 · 통합 0/167/1/168**

---

## 1. 배경 — C-29 가 남긴 구멍

C-29 실측: **버전을 렌더하는 요소가 어느 XAML 에도 없었다.** `RuntimeInfo` 는 뷰에 바인딩되지 않고 버전 문자열은 로그에만 들어간다. 결과로 두 가지가 막혀 있었다.

- 운영자가 **화면에서** 어느 백엔드·어느 네이티브 버전으로 도는지 확인할 수 없다.
- 계획서 S-05 가 단언할 대상이 없어 좁혀서 구현할 수밖에 없었다.

leader 가 이 카드에 한해 UI 추가를 허용했다.

## 2. 구현

| 파일 | 변경 |
|---|---|
| `ViewModels/MainWindowViewModel.cs` | `RuntimeVersionSummary` — `mode` + `common` + `display` 를 한 줄로. `RuntimeInfo` 세터가 이 프로퍼티도 함께 통지 |
| `MainWindow.xaml` | 상태바에 `StatusBarItem` 1개 (`AutomationId="RuntimeCommonVersionText"`) + 구분선 |
| `Scenarios/Smoke/SmokeScenarios.cs` | S-05 를 계획서 원문으로 복원 |

설계에서 지킨 것:

- **백엔드가 실제로 보고한 것만 넣는다.** `Version`/`DisplayVersion` 이 비어 있으면 그 항목을 아예 빼고, Mock 이면 자기 표식(`v0.0.0-mock`)을 그대로 보인다 — **없는 네이티브 버전을 지어내지 않는다.** 이 레인이 C-19 에서 "건수를 지어내지 않는다" 로 정한 것과 같은 원칙이다.
- **`RuntimeInfo` 세터가 요약도 통지한다.** 통지가 없으면 백엔드 초기화 뒤에도 라벨이 낡은 값을 들고 있게 된다 — 화면이 조용히 틀리는 형태다.
- **AutomationId 는 앱 규칙**(요소명)을 따랐다 — C-29 leader 결정(계획서 §4.1 정정, main `2e93965`).
- 기존 `StatusBarText` 는 건드리지 않았다.

표시 예(Mock 실행): `mode=Mock  |  common=v0.0.0-mock`

## 3. S-05 복원

```csharp
Assert.Contains("mode=", text);
Assert.True(Regex.IsMatch(text, @"\d+\.\d+\.\d+") || text.Contains("mock", OrdinalIgnoreCase));
```

계획서 원문("non-empty semver")을 그대로 두면 Mock 실행에서 실패한다. Mock 은 **정의상 semver 를 갖지 않으므로**, 카드가 명시한 대로 mock 표식을 동등하게 인정한다. 두 경우 모두 "백엔드가 보고한 버전이 화면에 있다" 는 같은 사실을 확인한다.

## 4. 반증 (`step3-falsify.log`)

라벨 바인딩을 `Content=""` 로 끊었다:

```
실패!  - 실패: 1, 통과: 4, 전체: 5
S05_RuntimePanel_ShowsBackendVersion: The runtime version label is empty.
```

S-05 만 실패하고 나머지 4건은 통과한다. 요소는 여전히 존재하므로 "not found" 가 아니라 "empty" 로 실패하는 것이 맞다 — 단언이 존재가 아니라 **내용**을 본다는 증거다. 원복 후 재확인.

## 5. 실측 (verbatim)

### 스모크 — 시나리오별 시간

| 시나리오 | 시간 |
|---|---|
| S-01 | 3 ms |
| S-02 | 120 ms |
| S-03 | 157 ms |
| S-04 | 382 ms |
| S-05 | 24 ms |
| **합** | **686 ms** (게이트 30 s 대비 2.3 %) |

```
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug
통과!  - 실패:     0, 통과:     5, 건너뜀:     0, 전체:     5

ImageProcTest.exe --automation-backend Mock …
자동화 Mock exit=0 · Passed=True · BackendMode=Mock(arg) · v0.0.0-mock

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   167, 건너뜀:     1, 전체:   168
```

Baseline 귀속: 통합 스위트는 C-29 최종 `0/167/1/168` 과 동일(UI·E2E 변경이라 통합 항목을 바꾸지 않는다 — 예상값이자 실측값). 스모크는 C-29 의 584 ms → 686 ms 로, 같은 5건이고 실행 편차 범위다(S-04 는 382 ms 로 거의 동일).

## 6. 미검증 (Gaps)

- **Native 모드에서 라벨이 무엇을 보이는지 확인하지 않았다.** 이 카드의 스모크·자동화는 Mock 뿐이다. `common=` 에 실제 semver 가 찍히는지는 관측하지 않았고, 정규식은 그 경우도 통과하도록 썼을 뿐이다.
- **`display=` 항목을 실제로 본 적이 없다.** Mock 의 `DisplayVersion` 이 비어 있으면 항목 자체가 빠지는데, 이번 실행에서 어느 쪽이었는지 라벨 원문을 따로 기록하지 않았다.
- **라벨이 갱신되는 순간을 보지 않았다.** 통지 배선은 코드로 넣었지만, 백엔드를 초기화해 값이 바뀌는 장면은 E2E 로 확인하지 않았다(S-05 는 시작 시점의 값만 본다).
- 상태바가 좁을 때 문자열이 잘리는지 등 레이아웃 영향은 보지 않았다.
- CI 실행 결과는 아직 없다(leader 가 `ci.yml` 에 넣은 gui-e2e-smoke 잡의 첫 결과 미확인).

## 7. 잔여 위험 (Residual risk)

- **S-05 는 이제 UI 문자열 형식에 묶인다.** `mode=` 접두나 구분자를 바꾸면 시나리오가 깨진다 — 표시 문구가 계약이 된 셈이다. 대신 그 계약이 얇아서(접두 1개 + 숫자/mock) 문구 다듬기까지 막지는 않는다.
- **Mock 표식을 semver 와 동등하게 인정하므로**, 네이티브인데 버전을 못 읽어 "mock" 이 찍히는 상황은 S-05 가 잡지 못한다. Native 스모크가 생기면 그쪽에서 semver 를 강제해야 한다.
- 라벨은 상태바에 있어 창이 좁으면 잘릴 수 있다. UIA 는 잘려도 `Name` 을 온전히 주므로 테스트는 통과하지만 **사람은 못 볼 수 있다** — 테스트 통과가 가시성의 증거는 아니다.

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → a959497
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --logger "console;verbosity=detailed"
ImageProcTest.exe --automation-raw … --automation-report … --automation-backend Mock
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
