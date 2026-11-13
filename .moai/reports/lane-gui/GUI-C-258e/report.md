[2026-11-04 16:38:40 KST] GUI-C-258e · 전체 Native 재검증 종료 342 PASS / 4 FAIL / 25 SKIP (371건, exit 1) · 허용 외 A06 내부 WPF E2E failed · 15:22 완료 기준 미충족, 지침대로 추가 수정·빌드·시험 중단 · 미푸시

## 수신·착수

- 재검증 중 허용된 evidence 3건 외에 `AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported`가 실패했다. 원문은 `GUI E2E FAILED (exit -532462766): ... WPF E2E failed.`이며, 이번에는 freshness 거절이 아니다. `AutomationReportBackendTests.cs:186`에서 앱을 통한 내부 runner 실패를 단언했다. 최신 지침에 따라 추가 source 수정·재빌드·새 시험을 중단했다. 이미 실행 중이던 전체 명령도 종료했고 원문을 보존했다. 세부 단언은 기존 로그만 읽어 확인했다.
- 허용 외 실패 이름·원인 한 줄: **A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported — 내부 runner의 WPF E2E failed(exit -532462766); 근접 시각 동일 runner 기존 이벤트는 Program.cs:219의 `Preprocessing menu must be disabled until Phase 1a.` 단언 위반(Assert :650)과 정합하나 특정 test PID 일대일 귀속은 미입증.**
- 기존 runtime-log는 stderr 첫 줄 요약만 남겨 inner exception을 알 수 없었다. 새 실행 없이 Windows Application의 기존 .NET Runtime Event ID 1026을 **16:22:20~16:24:10 KST / ImageProcTest.E2E 문자열**로만 좁혀 읽었다. [a06-existing-event.txt](a06-existing-event.txt)의 기존 이벤트 16:22:53.784와 16:23:27.776 둘 모두 위 inner exception/Program.cs:219 스택을 담고 있다. 접근 우회·새 제품 실행·시험·source 변경은 하지 않았다. 단언이 위반된 이유를 새로운 제품 결함이나 특정 정책의 잘못으로 더 추정하지 않는다.
- Reviewer도 최종 TRX 342/4/25와 371개 시험·25개 SKIP 일치를 직접 대조하여 15:22 완료 기준 미충족·중단 판정을 확인했다. 기존 이벤트 스택과 외부 A06 요약의 정합성은 확인하되, 해당 이벤트를 특정 test PID에 일대일 귀속하는 새 증명까지 한 것은 아니라는 한계를 유지한다. 독립 재실행은 하지 않았다.
- 최신 판정: `20261104-1522-gui.md`에서 EvidenceRoot helper 추가는 **불승인·범위 밖**으로 확정했다. 해당 제품·시험은 수정하지 않는다. 관리형 준비 복원 후 전체 재실행에서 실패가 IB01/IB02/EB02 세 건뿐이면 완료로 판정하고, 다른 실패가 남으면 이름·원인을 적고 추가 수정 없이 멈춘다.
- 위 지침에 따라 필터 없는 전체 Native를 재실행했다. 명령은 `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-restore --logger trx;LogFileName=full-native-rerun.trx --logger console;verbosity=detailed --results-directory .moai/reports/lane-gui/GUI-C-258e/full-rerun-results --blame-hang-timeout 10m --blame-hang-dump-type none`이며 필터는 없다. 환경은 full-command.txt와 같고 자체 data root만 `gui-c-258e-full-rerun`으로 분리했다. 실행 전 앱 프로세스 0개·새 data root 없음, 종료 후에도 앱 프로세스 0개를 확인했다.
- 재검증 TRX의 실제 시각은 **2026-11-04 15:38:30.4756869~16:26:49.6236975 KST**다. 최종 **342 PASS / 4 FAIL / 25 SKIP, 총 371건, exit 1**, 원문 elapsed 48.3190 Minutes. 첫 실행과 시험 이름 집합 371개가 같고, 카드 9건은 모두 다시 PASS였다. [rerun-failures.md](rerun-failures.md)에 실패 4건의 이름·원문, [rerun-skips.md](rerun-skips.md)에 SKIP 25건의 이름·사유를 보존했다. SKIP의 이름·사유도 첫 전체 실행과 모두 일치한다.
- 지정 [green-full.txt](green-full.txt)를 최종 `full-native-rerun.txt`의 동일 원문으로 갱신했다. 두 로컬 파일 SHA256은 `28E4C474D5F8CEE8AC24AF666B0C461260E33884F29829B0CE8EBAE51452FFE6`, 426517 bytes로 일치함을 확인했다. **실패 실행(exit 1)**을 담고 있으며 파일명으로 GREEN을 주장하지 않는다. [BLOCKED.md](BLOCKED.md)에 허용 외 이름·원인 한 줄을 남겼다.
- 15:14 상태 회신 당시 첫 전체 실행의 시각은 **14:29:31.3492688~15:09:46.2720551 KST**였다. 당시 green-full.txt에 복사했던 최초 원문은 `full-native.txt`와 커밋 `f915d770`에 그대로 보존되어 있다(로컬 최초 SHA256 `FD1FA73EF9B6D83122CA6C728566263501146151365E8954CF6E3823728A4829`, 388109 bytes). 두 실행을 덮어쓰거나 합산하지 않는다.
- 카드: `D:/workspace-github/image-processing/.moai/state/codex-msgs/20261104-1413-gui.md` (리더 표기 14:12 KST).
- 측정 트리: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `9374bf8ad85c0a4c58972c775463cc8cf68e7c1b`에 이번 시험 파일 3개의 working-tree 수정. 대상 파일에는 착수 전 diff가 없었다.
- 먼저 소스만 읽고 A 선택기 연결, 각 시험의 관찰 대상, W8 reader 경로를 부모에게 보고했다. 부모가 세 파일의 최소 수정을 승인했으며, 제품 코드는 변경하지 않았다. 다른 세션의 기존 변경을 보존했다.
- 구현 담당자는 변경 전 대상 RED → 원형 A 선택기 probe → 원형 W8 RED → 최소 수정 → 대상 GREEN만 담당했다. 필터 없는 전체 E2E 프로젝트 실행은 부모 레인 담당이며, 현재 보고서의 10개 통과를 전체 프로젝트 통과로 확대하지 않는다.

## Claim

Parameters의 Grid correction 라디오는 승인된 UI 계약대로 B를 제어한다. 메인 화소·시간·report를 검사하는 시험은 A 선택기로 메인 grid mode를 바꾸도록 손잡이만 정정했다. W8은 기존 거부 문자열을 그대로 둔 채 이미 제공되는 `StatusBarText.HelpText` reader를 사용하도록 정정했다. 카드의 실패 대상 9개와 C03 GridSuppression 대조군 1개, 총 10개가 같은 대상 실행에서 PASS, FAIL 0, SKIP 0으로 종료했다. 원승인 3시험 수정과 근거는 `fb7ce81292aed170a59a13ce53f452fee39d7ab8`로 커밋했으며 푸시는 하지 않았다. 추가 helper는 변경하지 않았고 카드 전체 완료는 아니다.

## Evidence

실행 환경과 정확한 필터는 [target-command.txt](target-command.txt)에 있다. 공통 조건은 `XPE_E2E_BACKEND=Native`, 기존 외부 `D:/workspace-github/image-processing/build/e2e-native-dlls`, `XPE_NATIVE_DIR_EXCLUSIVE=1`, **`XPE_E2E_ALLOW_GLOBAL_INPUT=0`**, 전용 `XPE_GUI_DATA_DIR=.../gui-c-258e-check`다. W8 발견을 위해 기존 자료를 가리키는 C231 Native/Calib/Raw/RawSize/Baseline 환경도 프로세스 안에서만 설정했다. Win32 파일 대화상자에 넘기는 경로는 최종 실행에서 `[IO.Path]::GetFullPath`로 Windows 절대경로로 정규화했다.

```text
dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter <target-command.txt의 정확한 필터>
```

| 실행·baseline | 실제 결과 | 종료 코드 | 원문 |
|---|---|---|---|
| 수정 전 grid 대상+C03 대조군 | FAIL 8 / PASS 1 / SKIP 0 / 총 9. W8은 C231 환경 부재로 **미발견** | 1 | [red-targets.txt](red-targets.txt) |
| 수정 전 원형 A 선택기 `TwoLaneWorkbenchScenarios.L06_AReferenceAlgorithm_MovesOnlyTheReference` | PASS 1 / 총 1 | 0 | [a-selector-probe.txt](a-selector-probe.txt) |
| 수정 전 W8, 최초 경로 전달 | FAIL 1 / 총 1. reader에 도달하기 전 파일 대화상자 확인 실패 | 1 | [red-w8.txt](red-w8.txt) |
| 수정 전 W8, 경로 정규화 후 | FAIL 1 / 총 1. 카드와 같은 square-file 거부 reader timeout 재현 | 1 | [red-w8-canonical.txt](red-w8-canonical.txt) |
| 수정 후 카드 9개+C03 대조군 | `Passed!  - Failed:     0, Passed:    10, Skipped:     0, Total:    10` | 0 | [green-targets.txt](green-targets.txt) |

첫 grid RED에서 P02는 `stage ms: 0, 0, 0`, P05는 `gsvgMode=None, status=NotRequested, elapsedMs=0`이었다. P10/P11의 원문 chain은 main `gsvg=NotRequested`와 B `gsvg=Applied`를 함께 보여 줬다. 이는 B 라디오를 바꾸면서 main 화소를 검사했던 시험 입력 불일치를 보여 준다. 수치 기준을 낮추거나 golden을 다시 기록하지 않았다.

### A 선택기의 실제 요청·화소 경로

- `gui/ImageProcTest/Views/AlgorithmBar.xaml:22-24`의 `LaneAAlgorithmPicker`는 VM `LaneAAlgorithm`에 TwoWay로 연결된다.
- `MainWindowViewModel.cs:1675-1683` setter → `AlgorithmPreset.For(value).ApplyTo(Settings)` → `Models/AlgorithmPreset.cs:89-96`의 `Settings.GsvgMode` → `Services/ProcessingChainPlan.cs:24`의 GSVG 요청 → `RealXpeBackend.cs:371`, `:419-422`의 `GuiGsvgRunner.Run` → `GuiGsvgRunner.cs:82`, `:106-124`, `:130/139`의 mode/config/native 호출로 이어진다. 이는 읽기 확인이며 경로 이름만으로 실제 요청을 추정한 것이 아니다.
- 제품 코드를 바꾸기 전에 기존 L06을 원형 그대로 실행했다. 실제 A hash는 `fac4bbd1e7eae4c5 → 6967623d96d2debf`(mean `57.952 → 166.427`), B hash는 `fac4bbd1e7eae4c5 → fac4bbd1e7eae4c5`(mean 고정)였다. A 선택기가 A/main 화소에 닿지 않는다는 중단 조건은 이 측정에서 발생하지 않았다.
- 변경 후 P05는 기존의 exported main `gsvgMode=VirtualGrid`, `status=Applied`, 양의 elapsed time 단언을 유지하고 통과했다. C03/C06의 main hash 비교와 P10/P11의 기존 hash/tile 단언도 그대로 통과했다. 표준 logger는 개별 PASS의 측정값을 출력하지 않으므로 새로운 hash 숫자를 임의로 덧붙이지 않는다.

### 바뀐 시험별 손잡이와 보존한 목적

아래의 main mode 선택은 모두 기존 `TwoLaneWorkbenchScenarios.SelectAlgorithm(window, "A", preset)` UIA 선택 경로를 재사용한다. `GsvgModeNone/GridSuppression/VirtualGrid` 입력을 각각 `No grid correction/Grid suppression/Virtual grid` preset으로 대응시켰다. ratio·주파수·air·pyramid·denoise·VOI 입력 및 복구 값은 그대로다.

| 시험 | 바꾼 손잡이와 유지한 oracle |
|---|---|
| C03 `GsvgStage_ReachesTheDrawnPixelsOrSaysWhyNot` (두 theory) | off/on 라디오 → A preset; 실제 status에 따른 main hash 변경/동일 조건을 유지. |
| C04 `MissingVirtualGridTable_IsRefusedAndTheImageIsUnchanged` | off/Virtual/복구 라디오 → A preset; missing-table 거부와 main hash 동일 조건 유지. |
| C06 `ChangingTheGridRatio_ChangesTheDrawnPixels` | Virtual/복구 라디오 → A preset; Parameters의 ratio 10/6 입력, Applied 및 hash 차이 단언 유지. |
| C07 `AGridRatioOutsideTheTable_IsRefused` | off/Virtual/복구 라디오 → A preset; Parameters의 ratio 7 거부와 hash 동일 조건 유지. |
| P01 `TheStageFitsInsideTheRender_OnTheWristSlice` | 공통 SetMode를 A 선택기로 변경; None/Suppress/Virtual의 stage 시간 ≤ 전체 render 시간 조건 유지. |
| P02 `RepeatedRender_RunsTheStageAgain` | Virtual/복구를 A로 선택; 두 번째·세 번째 stage 시간 > 1 ms 조건 유지. |
| P03 `AssumedDefaults_ReachTheDrawnPixels` | Virtual/복구를 A로 선택; 주파수 60/40 및 air 60000/45000의 실제 main hash 영향 조건 유지. |
| P04 `TheDrawnHud_NamesTheChain` | Virtual/복구를 A로 선택; 실제 drawn HUD의 chain/timing 표기 검사 유지. |
| P05 `TheExportedReport_CarriesTheChainAndTheVignetteFlag` | Virtual/복구를 A로 선택; main report mode/Applied/vignette=0/양의 시간 검사 유지. |
| P06 `WhatTheModuleDid_PerMode` | 각 mode/복구를 A로 선택; export된 module reason이 비어 있지 않아야 한다는 조건 유지. |
| P07 `TheTimeToADrawnFrame_SplitsIntoPhases` | None을 A로 선택; render 단계별 시간과 UIA polling 비용 관찰·기존 조건 유지. |
| P08 `TheAppsOwnStageTime_StaysWithinItsMeasuredSpread` | Virtual/복구를 A로 선택; 기존 반복 수·machine profile·median gate 유지. |
| P09 `PyramidGain_ChangesTheDrawnPixels_LevelsAloneDoNot` | Virtual/복구를 A로 선택; levels/gain 입력과 unity 동일·gain 적용 후 hash 차이 조건 유지. |
| P10 `ThePreviewChange_KeptTheDrawnPixels` | Virtual/복구를 A로 선택; 기존 수동 VOI/parameter 설정 및 bit-exact recorded hash 유지. |
| P11 `ThePreviewRender_StillCarriesTheRecordedTileSignature` | Virtual/복구를 A로 선택; 기존 tile signature·mean/max 허용치·viewport mean 대조 유지. |
| W8 `SwitchEnablesRunPreprocessing_AndAWrongRawSizeIsRefused` | square/odd-size 두 wait의 Text.Name 전체 검색 → 기존 `Read("StatusBarText")` HelpText reader; prefix·byte 수·설정 치수·negative RAW size 추정 금지 단언을 유지. |

C05 `GridCorrectionChoices_AreExclusive`는 B 라디오 자체의 배타성 시험이므로 기존 `SetGsvgMode`와 라디오 단언을 그대로 두었다. ProcessingChain의 main용 helper만 새로 분리했다. Wrist의 parameter `SetNumber :1081-1083`와 수동 window 설정은 자체적으로 Parameters를 열므로 mode helper가 A 선택만 수행해도 입력 경로를 유지한다. 변경 범위는 세 시험 파일 33 insertions / 18 deletions이며 제품 코드 변경은 없다.

### W8 상태 메시지 위치와 실제 reader 근거

- VM `MainWindowViewModel.cs:2876-2883`은 여전히 `StatusText = "Load failed: ..."`와 `LOAD_FAILED` alert를 기록한다. `MainWindow.xaml:526-530`은 `StatusBarText.HelpText=StatusText`, visible content는 `FooterAlert`로 제공한다.
- 기존 QuickStart `Read :1711-1718`은 StatusBarText를 HelpText로 읽는다. 이전 `AnyTextContains :1811-1812`는 Text.Name만 찾아 전체 prefix를 놓쳤다.
- 경로 정규화 RED에서는 대화상자가 닫힌 뒤 화면 Name이 `ERROR: Raw file has 18874368 bytes, more than the 2097152 bytes of 1024x1024 ...`인 것을 실제 timeout dump에서 읽었다. 메시지가 사라진 제품 결함으로 판단하지 않았다. 변경 후 같은 expected prefix/숫자를 HelpText에서 찾는 W8이 대상 10개 중 하나로 통과했다.
- 최초 W8의 dialog 확인 실패는 reader 실패와 분리했다. 경로 정규화 후에는 대화상자가 닫혀 원래 reader 실패까지 도달했다. 정규화 외의 모든 실행 조건을 독립 통제해 최초 실패의 유일한 원인을 확정한 시험은 아니므로, 이를 제품 수정 근거로 쓰지 않는다.

### 필터 없는 전체 Native 실행(부모 레인)

- 명령·환경: [full-command.txt](full-command.txt). 필터 없이 `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-restore`를 실행했고 상세 console/TRX logger 및 hang collector를 함께 사용했다. 원문은 [full-native.txt](full-native.txt), TRX는 `full-results/full-native.trx`다.
- 실제 종료 코드 **1**. 원문: `Total tests: 371`, `Passed: 327`, `Failed: 19`, `Skipped: 25`, `Total time: 40.2486 Minutes`. 전체 통과로 주장하지 않는다. 카드 9건은 이 전체 실행에서도 각각 `Passed`로 확인했다.
- 19건의 전체 이름·오류는 [full-failures.md](full-failures.md)에 있다. 원문 대조 결과 Legacy 관리형 DLL freshness 선행 거절 **15건**, 앱이 호출한 console E2E runner의 오래된 GUI 복사본 freshness 거절 **1건**, 전용 userdata와 시험의 고정 evidence 기대 경로 불일치 **3건**이며 나머지 유형은 없었다.
- CI `.github/workflows/ci.yml:1128-1137,1154-1158`에 있는 관리형 빌드 준비를 로컬 첫 실행 전에 충분히 하지 못했다. Legacy DLL은 10월 17일, 관련 source는 10월 24일 시각이었고 runner의 GUI 복사본은 10월 14일, 최신 GUI source는 11월 4일이었다. freshness 검사를 끄거나 파일 시각을 바꾸지 않았다.
- 첫 전체 실행 종료와 `ImageProcTest process count=0`을 확인한 뒤 기존 GUI·Legacy·SelfCheck·E2E runner의 **관리형 프로젝트 4개를 정상 빌드**, 각각 exit 0이었다. [managed-ci-build.txt](managed-ci-build.txt)에 명령과 전체 출력이 있다. 새 Native DLL 빌드·설치·제품 source 수정은 없었다.
- 준비 복원 뒤 같은 Native/C231/global0 조건에서 `--filter 'FullyQualifiedName~ImageProcTest.E2ETests.Scenarios.Legacy|FullyQualifiedName=ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported'`로 Legacy 그룹과 A06을 실행했다. 전용 data root는 `gui-c-258e-prepare`, logger/collector는 최초 전체 실행과 같았다. [prepare-native.txt](prepare-native.txt)/`prepare-results/prepare-native.trx`: **exit 0, 23 PASS / 0 FAIL / 3 SKIP, 총 26건**. 원문과 3 SKIP의 이름·사유는 [prepare-skips.md](prepare-skips.md)에 보존했다. 최초 전체 결과를 덮어쓰지 않았다.
- 두 TRX의 시험 이름을 교차 대조하여 최초 freshness 실패 **16건 모두** 후속 `Passed`와 일치함을 확인했다. EvidenceBundle의 IB01/IB02/EB02 3건은 이 보완 실행에 포함하지 않았고 여전히 미해소다. 서로 다른 실행·빌드 준비 상태의 숫자를 합쳐 단일 전체 GREEN으로 만들지 않는다.
- evidence 세 건은 `EvidenceBundleScenarios.cs:209-210`이 기존 source-bin/evidence를 고정해 읽는 반면, 제품은 `GuiDataPaths.Root/evidence`에 쓰는 차이였다. 첫 실행의 해당 6개 run 모두 전용 `gui-c-258e-full/evidence/<runId>/backend.json`에는 존재했고 옛 기대 위치에는 없었다. 실제 backend 값은 IB02 Mock→Native, EB02 Native→Mock, IB01 Native→Native로 기대와 일치했다. 이는 새 제품 쓰기 결함으로 주장하지 않는다.
- 기존 사용자 bin 설정을 건드리지 않기 위해 전용 `XPE_GUI_DATA_DIR`를 유지했다. evidence helper 추가안을 요청했던 이력은 있으나, 최신 15:22 지침에서 **불승인·범위 밖**으로 결정했다. 해당 helper는 원형을 유지하며 현재 승인 대기 사안으로 취급하지 않는다.

### 전체 SKIP과 CI 조건 차이

- **25건 모두 이름과 TRX 사유 원문을 [full-skips.md](full-skips.md)에 기록**했다. 15건은 전역 입력 opt-in 금지, 6건은 별도 실데이터 환경 미설정, 2건은 추가 Measurement opt-in 미설정, 1건은 기존 preprocess DLL의 `xpe_verify_offset_signed_file` export 부재, 1건은 역사적 구버전 DLL 부재다. PASS 수에 포함하지 않는다.
- CI는 `XPE_E2E_ALLOW_GLOBAL_INPUT=1` 및 `Category!=Measurement` filter를 쓰지만, 본 실행은 기존 전역 입력 금지에 따라 opt-in 0이고 카드대로 **필터 없음**이다. CI와 입력·Measurement 조건까지 같다고 주장하지 않는다. CI에는 `XPE_GUI_DATA_DIR` 설정이 없으며 위 evidence 세 건과 연결된다.
- Native 폴더는 기존 `D:/workspace-github/image-processing/build/e2e-native-dlls`이고 exclusive=1이다. 현재 CI artifact와의 바이너리 버전 동등성은 확인하지 않았다. 위 export 부재도 그대로 기록했으며, 최신 DLL을 사용한 임상 화질이나 제품 배포 검증으로 확대하지 않는다.

### Reviewer의 최종 범위 판정

Reviewer가 두 TRX를 직접 파싱하여 371건의 327/19/25, 보완 26건의 23/0/3 및 이전 freshness 실패 16건의 후속 PASS 이름 일치를 독립 대조했다. 원승인 시험 3파일의 기술 수용은 유지하며 그 범위 별도 커밋의 기술 차단은 없다고 회신했다. 다만 **전체 단일 GREEN·카드 전체 완료·CI 동등성 주장은 불가**하고 추가 EvidenceRoot helper는 새 승인 전 수정하지 않는다는 판정이다. 독립 재실행은 하지 않았다.

## Baseline-attribution

- grid RED와 A probe/W8 RED는 위 HEAD의 소스 미수정 상태였다. GREEN은 같은 HEAD에 승인된 세 시험 파일만 수정했다. 각 시험 프로세스의 종료를 확인한 뒤 다음 시험을 순차 실행했다.
- 첫 grid RED의 요청 필터에는 W8도 있었지만 `EnvGatedFactDiscoverer :25-34`가 C231 Native 환경 부재 시 빈 목록을 반환했다. 따라서 총 9였고 W8을 SKIP이나 PASS로 세지 않았다. 별도 원형 W8 RED 후 최종 GREEN은 C231 환경이 있어 10개를 모두 실행했다.
- `git diff --check -- <세 대상 시험 파일>`: 출력 없음, exit `0`. 로그는 전체 도구 반환 출력에서 줄 끝 공백과 EOF 빈 줄만 제거하고 마지막 개행 하나를 유지했다. 기존 CS1717·xUnit2013/2031 경고를 끄거나 수정하지 않았다.
- 대상 GREEN 종료 후 `Get-Process -Name ImageProcTest,testhost`에는 결과가 없었다(명령 exit 1은 대상 프로세스 없음). 부모에게 시험 독점 해제를 통보했다. 이후 추가 대상 시험을 실행하지 않았다.
- 커밋 전 `git diff --cached --check`: 원승인 시험 3파일 및 이 카드의 md/txt 증거 14파일을 명시적으로 stage한 뒤 실행, **exit 0 / 출력 없음**. 외부 설정·문서 변경과 추가 EvidenceRoot helper는 stage하지 않았다.
- 최종 중단 결과 문서 커밋 전 `git diff --cached --check`: report/BLOCKED/green-full 및 재검증 원문·실패/SKIP·기존 이벤트 7개 md/txt만 stage한 상태에서 **exit 0 / 출력 없음**. 새 제품·시험 source는 변경·stage하지 않았다. 최초·최종 TRX는 로컬 원본으로 보존한다.
- 15:14 상태 회신 문서 커밋 전 `git diff --cached --check`: `report.md`, `BLOCKED.md`, `green-full.txt` 3파일만 stage한 상태에서 **exit 0 / 출력 없음**. fetch로 원격 main의 `7faa61f2`(원승인 `fb7ce812` 병합)를 확인했고 파일 내용 차이 없는 fast-forward를 반영했다. 위 실행을 새 main에서 재실행했다고 주장하지 않는다.
- TRX 원본 두 개는 현재 저장소의 `.moai/reports/**` ignore 및 md/txt 예외 규칙에 따라 로컬에 보존하며 커밋에는 포함하지 않는다. 같은 실행의 전체 console 원문·모든 실패/skip 이름과 사유 표는 커밋 대상이다. 작업용 전용 userdata/evidence도 미해소 경로 대조 근거로 보존 중이다.

## Gaps

- 첫 전체 실행은 327/19/25, 관리형 준비 복원 후 선택 실행은 23/0/3, 최종 전체 재검증은 **342/4/25**로 각각 구분한다. 최종에는 허용 evidence 3건 외 A06이 남았으므로 15:22 완료 기준을 충족하지 못했다. A06은 과거 freshness 거절을 통과한 뒤 다른 내부 WPF 단언에서 실패한 것이며 선택 실행 PASS와 혼합하여 해결로 판정하지 않는다. 추가 수정·빌드·시험은 중단했다.
- CI는 global input opt-in을 사용하는 반면 로컬은 사용자 금지를 유지한 `0`이다. 대상 10개는 SKIP 없이 통과했지만 전체 프로젝트의 CI gesture 조건 동등성은 미확인이다.
- 사용한 외부 DLL은 기존 `e2e-native-dlls`다. 최신 CI artifact와의 버전·provenance 동등성은 확인하지 않았으며, 새 Native 빌드·설치를 하지 않았다. 테스트 fixture의 기존 calibration 생성과 C231 외부 calibration 원본 읽기를 구분한다. 외부 원본 calibration/raw/baseline을 변경하지 않았다.
- C04/C07 및 Wrist의 공통 helper sibling은 첫 전체 실행에서 통과했지만, 전체의 다른 실패를 숨기거나 대상 통과를 전체 통과로 확대하지 않는다.
- 임상 화질·배포 EXE·Tuner 연결은 검증 범위 밖이며 Tuner 보류를 유지했다.

## Residual-risk

이번 수정은 기존 시험의 손잡이를 현재 승인 UI 계약에 맞춘 것이다. hash·tile threshold·성능 gate·상태 내용은 유지했으며, 실패를 통과로 바꾸려고 기준을 새로 기록하지 않았다. 전체 프로젝트에는 외부 자료와 입력 방식에 따른 조건부 시험이 있으므로 부모는 미발견·SKIP·PASS를 구분해 최종 분모를 보고해야 한다.
