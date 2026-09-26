# GUI-C-26 — `--automation-backend` 인자 + gui 프로젝트 slnx 편입 (#136 잔여)

- 카드: GUI-C-26 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `f0ce14c`(자동화 백엔드 인자), `36badfe`(slnx 편입) — 미푸시
- 선행: C-25(`a288fbd`) 뒤, dev/gui 위에서 바로 (leader 지시대로 origin/main 병합 없음)
- **결과: slnx 재빌드 경고 0 / 오류 0 (3개 프로젝트) · dotnet 실패 0 / 통과 152 / 건너뜀 1 / 전체 153**

---

## 1. 커밋 1 — `--automation-backend {Mock|Native}` (`f0ce14c`)

C-25 격리가 남긴 유일한 구멍이 `BackendMode` 였다. 그것만은 배포 `appsettings.json` 에서 이어받았는데, 다른 선택 수단이 없었기 때문이다. 이 인자가 그것을 닫는다.

| 파일 | 변경 |
|---|---|
| `App.xaml.cs` | `--automation-backend` 파싱 → `AutomationBackendMode` (인자 없으면 `null`) |
| `MainWindow.CreateSettings()` | 인자가 있으면 파일보다 **우선**, 없으면 기존 동작 유지 |
| `Models/GuiAutomationReport.cs` | `BackendMode` + `BackendModeSource`(`arg`/`file`) |

리포트에 출처를 남기는 이유: **리포트만 보고 "무엇을 쟀는지" 를 알 수 있어야 한다.** C-22~C-25 내내 어느 백엔드로 돈 실행인지는 `BackendVersion` 문자열로 역추론해야 했다.

### 관측 (배포 파일 `backendMode=Native` 로 고정한 채)

| 실행 | `BackendMode` | 출처 | `BackendVersion` | `Passed` |
|---|---|---|---|---|
| `--automation-backend Mock` | `Mock` | `arg` | `v0.0.0-mock` | true |
| `--automation-backend Native` | `Native` | `arg` | `xpe_display 1.0.0` | true |
| 인자 없음 | `Native` | `file` | `xpe_display 1.0.0` | true |

리포트: `arg-Mock.json`, `arg-Native.json`, `noarg-file.json`.

**첫 줄이 핵심 증거다** — 파일은 `Native` 라고 적혀 있는데 인자로 `Mock` 을 주니 실제로 Mock 백엔드가 떴다(`v0.0.0-mock`). 인자가 파일을 이긴다는 것을 값으로 확인했다. 세 번째 줄은 인자 없는 기존 동작이 그대로임을 보인다.

## 2. 커밋 2 — slnx 편입 (`36badfe`)

C-22 에서 발견한 사실: gui 가 솔루션 밖이라 "앱 재빌드 0/0" 이 clients 만 가리켰고, gui 는 매번 따로 빌드해야 했다.

### 평면 편입은 거부된다 (실측)

```
dotnet build clients/ImageProcTest.slnx
error MSB4025: Project name 'ImageProcTest' already exists in the 'Root' solution folder.
```

두 프로젝트 이름이 똑같기 때문이다(`clients/ImageProcTest`, `gui/ImageProcTest`). 솔루션 폴더 `/gui/` 아래에 두어 해결했다 — 프로젝트 파일명을 바꾸는 것보다 영향이 작다.

### 빌드 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개
경과 시간: 00:00:02.14
```

산출물 3개 전부 확인:

```
ImageProcTest.IntegrationTests -> clients\ImageProcTest.IntegrationTests\bin\Debug\net8.0\ImageProcTest.IntegrationTests.dll
ImageProcTest                 -> clients\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll
ImageProcTest                 -> gui\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.dll
```

### CI 영향 범위 (카드 요구: 한 줄)

**영향 없음** — `ci.yml` 은 slnx 를 쓰지 않고 통합 테스트 csproj 를 직접 호출한다(`grep -n "slnx" .github/workflows/*.yml` → 0건). 빌드 시간·WPF 타깃 변화는 로컬 `slnx` 빌드에만 나타나고, 그 값이 위의 2.14초다.

## 3. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   152, 건너뜀:     1, 전체:   153
```

Baseline 귀속: C-25 최종 `0/152/1/153`(`GUI-C-25/step5-final.log`) → **변동 없음**. 이번 변경은 자동화 인자와 솔루션 구성이라 테스트 항목을 더하지 않는다 — 증감 0이 예상값이고 실측도 그렇다.

## 4. 미검증 (Gaps)

- **인자에 잘못된 값을 준 경우를 확인하지 않았다.** `--automation-backend Nativ` 같은 오타는 `AppSettings.BackendMode` 에 그대로 들어가고 `XpeBackendFactory` 가 Mock 으로 떨어뜨린다 — 조용한 오폭이다. 검증·거부는 넣지 않았다(카드 범위 밖).
- **인자에 회귀 테스트가 없다.** 이 동작은 자동화 리포트 3건으로만 확인했다. `App.ParseAutomationArgs` 는 WPF `Application` 파생 클래스 안이라 테스트 프로젝트가 링크할 수 없다.
- **slnx 를 쓰는 다른 소비자를 조사하지 않았다.** `.github/workflows` 만 확인했고, 로컬 스크립트나 IDE 설정은 보지 않았다.
- `dotnet test clients/ImageProcTest.slnx` 는 돌려 보지 않았다 — 편입으로 WPF 앱이 테스트 대상에 끼는지는 미확인이다(CI 는 csproj 직접 호출이라 현재 영향 없음).
- CI 실행 결과는 아직 없다.

## 5. 잔여 위험 (Residual risk)

- **`--automation-backend` 는 값을 검증하지 않는다**(§4). 오타가 Mock 실행을 낳고 리포트에는 `BackendModeSource=arg` 로 찍혀, "인자로 Native 를 줬다" 는 기억과 리포트가 어긋날 수 있다. 다만 `BackendMode`·`BackendVersion` 두 값이 함께 남으므로 리포트를 읽으면 드러난다.
- slnx 에 gui 가 들어오면서 `dotnet build <slnx>` 가 WPF 타깃을 함께 빌드한다. 로컬 2.14초로 부담은 없지만, CI 가 나중에 slnx 로 바꾸면 windows 러너에서 WPF 워크로드가 필요해진다.
- 솔루션 폴더 이름 `/gui/` 는 디렉터리 구조와 우연히 같을 뿐 연동되지 않는다. 프로젝트가 옮겨지면 폴더 이름이 낡는다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
ImageProcTest.exe --automation-raw … --automation-report … --automation-backend {Mock|Native}
grep -n "slnx" .github/workflows/*.yml            # → 0건
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
