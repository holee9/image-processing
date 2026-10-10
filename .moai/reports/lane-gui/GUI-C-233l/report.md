# GUI-C-233l 보고 — WPF E2E 의 옛 HUD 형식 단언, 소비자 전수, CI gui 잡 로컬 실행 (Refs #251)

dev/gui 에서 작업, 233k(`1090de16`) 위. 푸시 안 함. 고친 파일은 시험 소비자 한 곳(`gui/ImageProcTest.E2E/Program.cs`)이고 제품 코드는 바꾸지 않았다.

## 1. 단언 수정
- 원인: 233j·233k 가 HUD `Calib:` 줄의 의미를 "설정된 모드 수(N/7)"에서 "체인이 실제로 실행한 것"으로 바꿨는데, 같은 속성을 읽는 `ImageProcTest.E2E/Program.cs` 의 `EndsWith("/7 stages")` 가 그대로였다. 233j 때 저는 `SelfCheck`·UIA·통합 시험·`SettingsProcessingConnectionTests` 만 소비자로 보았고 **세 번째 실행 파일(WPF E2E 러너)을 찾지 못했다**. PR CI 의 `gui-shell-runners` 가 잡았다.
- 고침: 기대값을 형식이 아니라 **러너가 실제로 쥔 체인 상태**에서 규칙으로 도출한다(`ExpectedCalibLine`: 체인 없음 `—` / 전처리 실행 `3 stages (offset · gain · defect)` / 요청했으나 미적용 `failed` / 그 밖 `not run`). 두 지점에서 단언한다.
  1. 초기(프레임 없음): 기록이 없고 줄이 정확히 `—`.
  2. **끝(실제 렌더 뒤)**: 1024×1024 fixture 를 Mock 으로 불러 렌더가 커밋된 뒤(레인·시간까지) 체인의 preprocess 가 `NotRequested`, `PreprocessRan=false`, 줄이 `not run`. 이 지점이 없으면 초기 지점(`—`)은 HUD 가 무엇에서 파생되든 맞으므로 아무것도 검증하지 못한다 — 제 첫 반증이 바로 그것을 드러냈다(아래).
- 360행 주석("all 7 calibration stages")을 사실에 맞게: 분석 패널은 7개 **설정** 컨트롤을 제공하고, 전처리 호출이 실제로 실행하는 것은 offset·gain·defect 뿐이다.
- 반증: SelfCheck 단계를 임시 우회한 사본에서 HUD 가 "7/7 stages" 를 말하게 하면 E2E 가 `After a render whose preprocess stage is NotRequested the HUD line should be 'not run', got: 7/7 stages` 로 빨강 (`falsification_hud_reports_7of7.txt`). **첫 반증은 무효였다**: 프레임이 없는 초기 지점만 단언했을 때는 같은 변조에도 통과했다(그 지점에서는 "—" 가 맞으므로). 그래서 렌더 지점을 추가했다.
- 구현 중 알게 된 것: 이 러너에서 `async void` 인 `LoadImage` 는 디스패처 프레임 **밖**에서 시작하면 계속 실행이 풀 스레드로 가서 UI 컬렉션을 건드려 `NotSupportedException` 이 난다. 그래서 시작과 대기를 모두 `PumpUntil`(디스패처 프레임 안)로 했다.

## 2. 233g~k 가 바꾼 이름·형식의 소비자 전수 (`git grep`: gui/ clients/ tools/ docs/ README .github/)
검색 원자료: `consumer_grep_counts.txt` (이름별 파일별 건수).

| 바꾼 것 | 읽는 곳 | 기대 형식 | 고침 |
|---|---|---|---|
| `CalibStageCountDisplay` 형식 (233j/k) | `gui/ImageProcTest.E2E/Program.cs` | 규칙 도출값 | **이번에 고침** |
| 〃 | `gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs` (시나리오 13·14·15) | 규칙값 | 233k 에서 고침 |
| 〃 | `ViewportShell.xaml` 바인딩 | — | 233j |
| UIA 자동화 ID `HudCalibStages`, `LaneATag`, `LaneBTag`, `Lane*EmptyText`, `LaneBStaleText` (신규) | `clients/ImageProcTest.E2ETests/.../TwoLaneWorkbenchScenarios.cs` L07 | `Calib:  not run` / `—` | 233j/k |
| 레인 표지 `Rendered{A,B}Algorithm` (233j/k) | `SettingsProcessingConnectionTests` (바인딩 핀), SelfCheck 14, `ViewportShell.xaml` | 기록의 `Inputs`, 레인 영상 없으면 null | 233j/k |
| `LaneAImage`/`LaneBImage` 가 기록 파생 (233h) | `ViewportShell.xaml`, SelfCheck, E2ETests `TwoLane…` (L01·02·03·07) | 기록 | 233h/j — 이번 로컬 FlaUI 스위트에서 통과 |
| 〃 | `docs/design/README.md`, `docs/design/IMPLEMENTATION_GUIDE.md` | 계획 문서의 옛 서술 | 소비자가 아니라 과거 설계 문서. 안 고침 |
| 자체 점검 보고 키 `Requested*` (233f) | `MainWindow.xaml.cs`(생산·판정), `GuiAutomationReport.cs`, `README.md`, `tools/e2e/Invoke-ImageProcTestGuiRealE2E.ps1`, `BaselineReviewFixTests` 핀 | `Requested…` | 233f/h |
| 메뉴 보고서 `calibrationEvaluation.*` (summary·modes null 가능, `requested`, `executed`), `displayPipeline.*`(`renderedBy`, `version`, `applied`…), `processingChain.*` | `MainWindow.xaml.cs:664` (`"calibrationEvaluation"` 문자열 존재만), SelfCheck, `GsvgWristSliceScenarios`(Native 전용: `processingChain.stages/gsvgMode/status/pipelineTimings`) | 키 이름은 그대로 | 이름이 안 바뀌어 소비자 수정 불필요. `GsvgWristSliceScenarios` 는 Native 라 이번에 **못 돌렸다**. `tools/`·`docs/help/` 에는 소비자 0건 |
| 기록 파생 프로퍼티 `PreprocessRan`/`PreprocessStages`/`ChainStatus`/`PipelineTimings`/`HasPipelineDiagnostics`/`DisplayPipelineSummary` (233g: 이벤트마다 초기화됨, 시간은 레인 뒤에 추가) | `MainWindow.xaml.cs` 자체 점검(:316-326, :611) — Mock 실행 `Passed=True`(이번에 실행), Native 실행 `Passed=True`(233i); `ImageComparisonViewport.cs`(ChainStatus 텍스트); `QuickStartWalkthroughScenarios`(W 시리즈, 환경 변수 필요) ; `AutomationReportBackendTests` A20(Native) | 의미 변화(이벤트 뒤 초기화) | 자체 점검은 통과. W·A20 은 이번에 안 돌렸다(환경 변수/Native). `docs/project/XPE-GUI-DISP-INT-001…md` 는 `LoadedImageFrame.DisplayPipelineSummary` 를 서술하는 설계 문서라 무관 |
| `RenderedVoi*` / HUD 창 값 | `DisplaySettingsPanel.xaml`, `ViewportShell.xaml`, `WorkbenchObservation.HudCenter`(E2E V 시리즈) | 기록 | V 시리즈는 이번 Mock FlaUI 스위트에서 통과 |
| `PreviewStaleReason` 새 값 `StaleBackendReplaced` | `WorkbenchObservation.StaleIndicator`, L07 | 문자열에 "backend was replaced" | L07 통과 |
| fixture manifest 키 분리 (233i) | `GuiFixtureManifest.cs`, `ImageProcTest.SelfCheck/Program.cs`, `tools/e2e/Invoke-…RealE2E.ps1` | `…Mock`/`…Native` | 233i. 그 밖 읽는 곳 0건 |
| 전처리 결과 문구 (233k: `ExecutedStages` 로 조립) | 고정 핀 없음 | 글자 동일 | — |

못 본 것: 이름이 아니라 **동작**에 기대는 소비자(예: 어떤 시험이 "새 프레임 뒤에도 이전 체인 상태가 남아 있다"를 가정)는 grep 으로 찾을 수 없다. 이번 로컬 실행에서 드러난 것은 아래 표의 실패 0건이 전부다.

## 3. CI gui 잡을 로컬에서 같은 명령으로 (ci.yml: main 의 `3c41b3d5` 판 + 이 브랜치의 게이트 변경)
| 잡 / 단계 | 명령 (ci.yml 그대로) | 결과 | 증거 |
|---|---|---|---|
| gui-shell-runners / 빌드 | `dotnet build` 3개 프로젝트 `-c Debug` | 통과 | `job_gui_shell_runners_build.txt` |
| 〃 / SelfCheck | `dotnet run --project …SelfCheck -c Debug --no-build` | 통과 (`GUI-S0 self-check passed.`) | `job_gui_shell_runners_selfcheck.txt` |
| 〃 / WPF E2E | `dotnet run --project …E2E -c Debug --no-build` | **수정 전 CI 가 빨강이던 곳 → 통과** (`GUI-S0 E2E passed.`) | `job_gui_shell_runners_wpf_e2e.txt` |
| gui-shipped-build | `./tools/ci/Test-ShippedBuild.ps1` | 통과 (control ok / release ok / gate ok / refusal ok). `RUNNER_TEMP` 를 직접 설정 | `job_gui_shipped_build.txt` |
| gui-automation / 빌드 | `dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug` | 통과 | — |
| 〃 / 숨김 창 Mock 자동화 | step 의 PowerShell 본문 그대로 | `exit=0`, `Passed=True`, `BackendModeSource=arg` | `job_gui_automation_step1_hidden_run.txt` |
| 〃 / FlaUI Mock 스위트 | `dotnet test clients/ImageProcTest.E2ETests/… -c Debug --logger trx --results-directory build/e2e-test-results` | **실패 0**, 통과 224, 건너뜀 75 (17 m 59 s) | `job_gui_automation_step2_flaui_smoke_mock.txt` |
| 〃 / "입력 게이트 건너뜀 없음" 단언 | (실행 안 함) | 로컬에서는 **빨강일 수밖에 없다**: 건너뜀 75 중 입력 게이트가 15개. CI 는 `XPE_E2E_ALLOW_GLOBAL_INPUT=1` 로 실제 마우스·키보드 입력을 켜지만 이 PC 는 전경 입력 금지(그 입력은 리더 터미널로 가서 세션을 끊은 전례) | TRX 에서 센 값 |
| dotnet-tests / 네이티브 DLL 확인 | 단계 본문 그대로 | **로컬에서는 빨강**: `xpe_dicom.dll`, `dcmdata.dll`, `openjp2.dll` 없음(이 PC 에는 dicom 빌드 산출물이 없다) | `job_dotnet_tests_step_verify_native_binaries.txt` |
| 〃 / 통합 시험 | `dotnet test …IntegrationTests -c Debug --logger trx --results-directory build/dotnet-test-results` (`build/ci-common/bin` 에 DLL 7개 배치) | 통과 955, 실패 0, 건너뜀 1 (심볼릭 링크 생성이 이 PC 에서는 개발자 모드/관리자 권한 필요 — CI 러너는 높은 무결성) | `job_dotnet_tests_step_run_integration_tests.txt` |
| 〃 / AC-12 시간 게이트 (`AC12_ENFORCE=1`) | 단계 본문 그대로 | 전체 24.0 s(한도 180), Smoke 1.3 s, Safety 1.8 s 모두 한도 안. **빨강인 유일한 이유**: Safety 범주에 건너뜀 1(위의 심볼릭 링크) | `job_dotnet_tests_step_ac12.txt` |
| 〃 / 실모듈 시험 실행 단언 | 이 브랜치의 ci.yml 본문 그대로 | 통과: EnhanceBasic 4/4, BaselineDicom 4/4, BaselineDisplay 3/3, 실모듈 러너 시험 1건 `Passed` | `job_dotnet_tests_step_realmod.txt` |
| gui-e2e-native | — | **안 돌렸다**: 결합 CI 가 만든 DLL 한 벌과 전체 Native E2E 환경이 필요하고, 이 PC 의 DLL 은 출처 미증명 조합이라 CI 와 같은 입력이 아니다 | — |

로컬과 CI 의 차이(결과 해석에 영향): ① CI 는 `pwsh`(7), 여기는 Windows PowerShell 5.1 — 같은 `.ps1` 본문을 5.1 로 돌렸다. ② `RUNNER_TEMP` 등 러너 환경 변수는 직접 설정. ③ DLL 세트는 `c233_native`(CI 빌드 7개 조합 + post 의 `xpe_display`·`gsvg`; 출처 미증명). ④ 전경 입력 옵트인을 켜지 않았다.

## 4. WSL 가상 머신 (리더 요청)
`clients/`·`gui/`·`tools/` 의 `Process.Start`·`ProcessStartInfo` 를 전수 검색했다. bash·sh·wsl 을 부르는 C#·ps1 호출은 **0건**(띄우는 것은 `git`, `cmd.exe`, 앱 exe, 교정 생성기 exe). `.sh` 히트는 `tools/mcsim/*.sh` 안의 줄뿐이고 csproj/targets 에도 없다. 대신 `.claude/settings.json` 의 훅 명령 34개 중 33개가 맨 `bash` 로 시작하고, 이 PC 의 `where bash` 는 Git Bash → `System32\bash.exe`(WSL) → WindowsApps 순이다. 훅이 어느 bash 를 고르는지는 **관측하지 못했다**(추정). 설정은 리더·템플릿 소유라 건드리지 않았다.

## 통과·범위 요약
로컬에서 돌린 gui 잡 명령 중 **실패 0**; 로컬에서만 빨강인 것은 환경 차이 세 가지(전경 입력 옵트인, dicom DLL 부재, 심볼릭 링크 권한)와 `gui-e2e-native`(안 돌림). 이 환경 차이 때문에 이 로컬 결과는 "PR CI 가 초록일 것"의 보증이 아니다.
