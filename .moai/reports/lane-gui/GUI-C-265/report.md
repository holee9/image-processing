[2026-11-13 19:06 KST] 리더 수용 — 카드4(b)는 A06 두 회차 공유 불변으로 충족; 전체 설정 mtime 구간의 시험·저장 경로를 읽기 확인

## 19:05 리더 판정·설정 저장 경로 후속

리더는 4(b)가 내부 runner의 공유 설정 보존 기준이며 A06 두 회차 해시·mtime 동일로 충족됐다고 판정했다. 전체 suite 변화는 다른 시험이 지정 데이터 루트에 정상 저장한 것으로 수용했다. 원래 Native 설정이 쌓인 경로라는 설명은 리더의 가능성 판단이며, 과거 설정 생성 경위를 이번에 소급 증명한 것은 아니다. 기존 구현 커밋은 `3f3274cca1dbdcb9431b282013f2e067a3014ecb`이며 이번 추가 작업은 문서뿐이다.

읽기 결과, 최종 공유 파일 mtime `2026-11-13T18:59:14.7146693+09:00`을 포함하는 full.trx 시험은 **`ImageProcTest.E2ETests.Scenarios.Workflows.HiddenSelectionCopyObservation.PlainCopyWorks_ThenWhatHappensWhenTheSelectedLineIsHidden`**이다. 시작 `18:59:13.981045+09:00`, 종료 `18:59:32.503442+09:00`, PASS. 바로 앞 `ClearAlerts_PressedWhileTheFilterIsOn_LeavesThisBehind`는 `18:59:13.980803`에 종료해 해당 mtime 직전이다.

| 연결 | 실제 코드·읽기 근거 |
|---|---|
| 해당 시험이 새 앱 생성 | `clients/ImageProcTest.E2ETests/Scenarios/Workflows/HiddenSelectionCopyObservation.cs:40–46`: `using var app = new WorkflowApplicationFixture()` 후 로그 창을 연다. |
| fixture RAW 지정 | `clients/ImageProcTest.E2ETests/Fixtures/WorkflowApplicationFixture.cs:16–19`: `fixtures\\gui-s0\\raw\\synthetic_1024x1024.raw`를 base로 전달. :9–12 및 ApplicationFixture.cs:21–23은 report 인수 없는 interactive 실행임을 설명한다. |
| 앱 인수 전달 | `ApplicationFixture.cs:151–152,154–169`: backend와 RAW/1024×1024 인수 전달. :196–200 Native 경로 전달. 이 시험의 실행 환경은 full.result.json의 Native/공유 데이터 루트다. |
| RAW 자동 열기 | `gui/ImageProcTest/MainWindow.xaml.cs:237–250`: IsAutomationMode=false인 상태에서 AutomationRawPath가 있으면 LoadRawImageButton 클릭. `MainWindowViewModel.cs:2851–2853`: 해당 RAW를 LoadImageFromPathAsync로 연다. |
| 최근 파일 이력 저장 | `MainWindowViewModel.cs:2943–2946`: RAW 로드 뒤 RememberRecentRawFile(path). `:2484–2502`: 이력을 갱신하고 `_settingsService.Save(Settings)` 실행. 주석 :2480–2482는 Save Settings 메뉴를 기다리지 않고 이력을 지속화한다고 설명한다. |
| 저장 대상 | 기본 MainWindow는 CreateSettings의 비automation 기본 서비스, AppSettingsService.cs:45 / GuiDataPaths.cs:12–15에 따라 지정 데이터 루트 appsettings를 사용한다. 이번 새 runner의 전용 설정 주입 경로와 다르다. |
| 현재 파일의 보조 대조 | 공유 appsettings의 `recentRawFiles[0]`가 `D:\\workspace-github\\xpe-gui\\gui\\ImageProcTest\\bin\\Debug\\net8.0-windows\\fixtures\\gui-s0\\raw\\synthetic_1024x1024.raw`로 fixture RAW와 일치한다. 파일은 읽기만 했다. |

**판단:** 해당 mtime을 포함하는 시험명과 그 fixture에서 appsettings를 저장하는 경로는 위 코드로 확인했다. 파일 mtime·TRX 구간·이력 첫 항목이 이 경로와 일치한다. **정확한 write syscall/PID를 계측한 것은 아니므로 OS 수준의 실제 파일 작성자 귀속은 미확인**이다. 시험이 Save Settings 버튼을 직접 누른다고 주장하지 않는다. 이 시험은 RAW 로드 시 최근 파일 이력 저장 경로를 이용한다. 추가 실행·설정 수정·복원·다른 시험 변경 없음.

커밋 후 SHA 안내 기록이다. dev/gui에 승인된 소스3개·카드 문서2개만 커밋했으며 Refs #282 footer와 staged whitespace 검사 exit0를 확인했다. 훅 우회·amend·push 없음. 이 안내 줄은 커밋 후 로컬 기록으로 남긴다. 도우미·원문/TRX/JSON/ZIP은 카드 폴더에 로컬 보존했다.

## 최종 Claim

runner 소유 Mock 설정 주입과 내부 관측 줄 전달을 구현했다. A06 변수 있음/없음 각1회는 모두 PASS이며, 두 회차에서 공유 설정의 해시·mtime이 동일했다. 전체 E2E도 실패0으로 종료했다. 전체 suite 전후의 공유 설정은 달라졌으나, 리더의 19:05 판정에 따라 카드4(b)는 내부 runner의 A06 두 회차를 대상으로 하며 그 기준은 충족됐다. 전체 설정 변화의 관측값은 삭제하거나 복원하지 않고 보존한다.

## Diff와 근거

- `gui/ImageProcTest.E2E/Program.cs`: 기존 Temp에 고유 `xpe-gui-e2e-settings-<guid>.json` 하나를 Mock으로 저장하고 AppSettingsService를 MainWindow에 주입한다. finally에서 본 파일/.previous를 삭제한다. 설정 서비스의 기존 atomic Save가 .writing 임시 파일을 정리한다. 새 임시 폴더 없음. 실제 내부 ViewModel의 ActualBackendMode·RuntimeInfo.BackendName·주입한 FilePath를 시작 stdout 한 줄과 마지막 성공 줄에 기록한다.
- `gui/ImageProcTest/MainWindow.xaml.cs`: 기본 parameterless 생성자는 기존 동작을 유지하고, runner용 AppSettingsService override가 있으면 그 서비스의 Load를 먼저 사용한다. 이 분기는 기본 공유 설정 서비스를 만들거나 Load하지 않고 반환한다. 기존 사용자/automation 선택 경로는 override가 없는 경우 그대로다.
- `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs`: GUI E2E만 성공 stdout 요약을 선택해 GuiE2EStatus에 포함한다. 다른 runner의 요약 선택은 기존 null 기본값이고, 내부 실패 시 성공 줄이 없으면 기존 stderr fallback을 유지한다.
- 단언/대기 보존: HEAD의 Program과 현재 파일에서 Assert / WaitForExit / thread.Join / Thread.Sleep / Task.Delay 관련 행을 추출해 비교한 결과 baseline104행 / current104행 / 동일=true. 기대값·타이밍을 변경하지 않았다. `git diff --check` exit0.

## Evidence — 빌드·지정 진단·전체 1회

| 실행 | 시작→종료(KST) | 결과 | 공유 설정 전후 |
|---|---|---|---|
| GUI/SelfCheck/내부E2E/clients E2E 빌드4개 | 각 build-*.result.json | 모두 exit0 | 각 true |
| A06 / 변수 있음 | 18:04:36→18:05:10 | 1P/0F/0SKIP, exit0 | 해시·mtime 동일 |
| A06 / 변수 없음 | with-root 다음→18:05:43 | 1P/0F/0SKIP, exit0 | 해시·mtime 동일 |
| 전체 / 변수 있음 | 18:05:43.0415193→18:59:32.7586550 | 346P/0F/25SKIP, total371, exit0 | 해시·mtime 다름 |

빌드 명령은 `dotnet build <project> -c Debug --no-restore --nologo`, project는 GUI / SelfCheck / 내부 E2E / clients E2ETests 순서다. 두 지정 실행은 A06 FullyQualifiedName 필터1개만 사용한다. 전체는 `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=full.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-265"`, 필터·StopOnFail 없음. 전체 stdout 원문: `Test Run Successful. / Total tests: 371 / Passed: 346 / Skipped: 25 / Total time: 53.8118 Minutes`. TRX total371/executed346/passed346/failed0/aborted0/timeout0, NotExecuted25개. A06·EB02·IB01·IB02·D258 MetricsFooter·W5 PASS를 각 결과에서 확인했다.

각 build/with-root/without-root/full의 stdout.txt·stderr.txt·trx·result.json을 카드 폴더에 보존한다. `results.md`에 두 A06의 내부 관측 줄과 SKIP25개 정확한 이름·사유를 TRX에서 발췌했다. 모든 원문 명령·시각·자식 Native 환경·공유 파일 전후 값은 result.json, 소스해시·부모환경·정책은 preflight.json에 있다. 서버 재시작 뒤에도 독립 PID39540 및 기존 자식이 유지된 것을 확인했으며 재실행하지 않았다.

### 내부 선택·설정 경로 직접 관측

두 A06의 외부 GuiE2EStatus 모두 `ActualBackendMode=Mock; BackendName=MockXpeBackend`를 포함한다. 이는 외부 report.ActualBackendMode를 내부 값으로 오해한 것이 아니라 **내부 runner가 실제 ViewModel에서 얻어 출력한 한 줄**이다. with-root 전용 설정은 `.../xpe-gui-e2e-settings-0fad4025a0454e62a5ad41b66851b156.json`, without-root는 `.../xpe-gui-e2e-settings-026c8e22be4a4d25b9f7371791a2bff5.json`. 전체 실행의 A06도 `GUI E2E startup: ActualBackendMode=Mock; BackendName=MockXpeBackend; SettingsFile=C:\Users\drake\AppData\Local\Temp\xpe-gui-e2e-settings-6bb6d233e6d14c03a37a193afa62c234.json`을 외부 JSON에서 확인했다. 세 본 파일과 .previous 모두 종료 후 없음 확인.

시각에 맞는 Temp 자동화 JSON을 각1개 찾아 with-root-automation.json / without-root-automation.json / full-automation.json으로 복사하고 각 원본과 SHA 일치를 확인했다. full-automation SHA256 `AE213ABB177BA75ADDFFD4849419F3544BC703069139EB0DD21C83C5ECBBDA55`. 원본 자동화 JSON은 유지한다.

### 공유 설정 보존 범위·전체 구간 예외

공유 경로: `D:/workspace-github/xpe-gui/clients/ImageProcTest.E2ETests/bin/Debug/net8.0-windows/gui-c-259-check/appsettings.json`.

- 두 A06 회차 전후 모두 SHA256 `2D6B643387ED10D9F60D700661CA3FA27CE4F424DDA22F03F3A9E4A7A46A7280`, mtimeUTC `2026-11-13T08:00:59.1291864Z` 동일. 전용 runner 동작이 공유 설정을 바꾸지 않았다는 해당 구간 증거다. OS 파일 read 추적을 실행한 것은 아니며 읽기 배제는 위 override Load 분기의 코드로 확인했다.
- 전체 suite 전 동일 값 → 후 SHA256 `2E41758204F3A2586F43D4D6F06912DB2DC17335B7BA86ACB9E09B1BF9CB80D9`, mtimeUTC `2026-11-13T09:59:14.7146693Z`로 달라졌다. `full.result.json` sharedSettingsUnchanged=false를 그대로 보존한다.
- full stdout:2265에는 기존 Save Settings 동작 관측이 있고, :3493–3499에는 18:59:14 시각의 HiddenSelectionCopyObservation 로그가 있다. 기존 앱에는 최근 파일 이력 저장(`MainWindowViewModel.cs:2497–2502`)과 SaveSettings(:2550–2554)이 있다. **최종 파일을 어느 호출이 썼는지 OS 추적/계측으로 귀속하지 않았으므로 해당 작성자를 확정하지 않는다.** A06 단독 보존 결과를 전체 suite 불변으로 확대하지 않는다. 설정을 수동 수정하거나 되돌리지 않았다.

## Baseline-attribution / 정리

트리 `D:/workspace-github/xpe-gui`, 시작 HEAD `ba0bd1b48cfe3432836ae7042c6ec6c2c4ce0819` + 위3개 파일 diff를 빌드·측정했다. 외부 Native3·보정/raw/baseline·전역입력 금지0은 기존 조건이고 변수 없음 A06에서만 XPE_GUI_DATA_DIR를 제거했다. CI 수치는 이번 baseline에 섞지 않았다.

full appAfter=[]. full testhost37124의 A05 임시 사본은 실행 구간 생성·일반파일17개·하위 폴더/링크 없음 확인 후 ZIP17항목별 SHA 일치 검증→파일별 삭제→빈 폴더 삭제했다. 증거 `temporary-selfcheck-A05-37124.zip`, SHA256 `3FC7B9EC1B017764E81AB2BDD7CB637445E21AD97F858EDD1E3D87EEFCE852D3`. runner 전용 설정3개·.previous는 이미 finally 정리됨을 확인했다. 다른 Temp 전수 삭제·공유 evidence 수정은 하지 않았다.

## Gaps / Residual-risk / 다음 행동

전체 suite는 실패0이지만 SKIP25개는 미검증이고, 전체 구간 공유 설정은 변했다. 이는 리더가 수용한 별도 시험의 저장 경로이며 4(b)는 A06 구간 보존으로 충족됐다는 판정을 따른다. 기본 앱 생성 경로는 코드상 유지했고 전체 E2E를 측정했으나 별도 Integration 전체는 실행하지 않았다. 공유 파일 OS read 추적·쓰기 작성자 직접 귀속·강제 실패 시 임시 설정 정리의 별도 재현은 수행하지 않았다. 추가 시험·다른 시험 수정·설정 복원 없이 후속 문서를 dev/gui에 커밋하고 push는 하지 않는다. 병합은 리더가 한다.

리더가 GUI-C-264 제안3·4를 채택했다. 구현 담당은 runner와 꼭 필요한 주입/전달 경로만 수정하고, 루트는 빌드·A06 변수 있음/없음 각1회·전체E2E1회 및 공유 설정 전후 확인·보고·커밋을 맡는다. 기존 A06 RED(264 단일 실행 0P/1F, Event1026 Program.cs:219)를 근거로 tdd 진행한다. 단언·대기·다른 시험은 수정하지 않는다. 공유/사용자 설정 수동 변경, push, 훅 우회 없음.

## 현재 실측

- GUI / SelfCheck / 내부 E2E / clients E2E 시험 빌드4개 모두 exit0. 각 result.json의 공유 설정 불변 값 true.
- 변수 있음 A06 18:04:36→18:05:10 KST, PASS1/FAIL0. 변수 없음 A06도 PASS1/FAIL0. with-root/without-root TRX·stdout/stderr·result.json을 보존한다.
- 외부 JSON의 GuiE2EStatus에 내부 시작 기록이 그대로 전달됐다. 두 회차 모두 `ActualBackendMode=Mock; BackendName=MockXpeBackend`. 단순 외부 ActualBackendMode가 아니라 내부 runner가 실제 viewModel에서 얻어 stdout으로 보낸 줄이다.
- 읽은 설정 경로는 각각 `C:/Users/drake/AppData/Local/Temp/xpe-gui-e2e-settings-0fad4025a0454e62a5ad41b66851b156.json`, `.../xpe-gui-e2e-settings-026c8e22be4a4d25b9f7371791a2bff5.json`. 실행 후 두 본 파일·.previous 없음 확인. 새 임시 폴더는 만들지 않았다.
- 공유 appsettings의 두 A06 회차 전후 SHA256 `2D6B643387ED10D9F60D700661CA3FA27CE4F424DDA22F03F3A9E4A7A46A7280`, mtimeUTC `2026-11-13T08:00:59.1291864Z` 모두 동일. result.json에 파일 경로와 각 전후값을 보존했다.
- `with-root-automation.json`, `without-root-automation.json`은 실행 시각과 맞는 Temp A06 JSON을 각1개 찾아 복사하고 원본과 SHA 일치를 확인했다. 원본은 그대로 유지한다.
- full은 18:05:43.0415193 KST에 XPE_GUI_DATA_DIR 설정 상태로 시작했다. 필터·StopOnFail 없이1회이며 아직 전체 분모는 미확인이다.
