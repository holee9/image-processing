# GUI-C-268 계측 관측과 원복
## 실행·결과
HEAD1071de9b + 승인된 두 파일 임시 계측, PRE d3216b2a 혼합 staging. 2026-11-17T03:12:42.4602025+09:00~2026-11-17T03:48:12.9014657+09:00, exit1.
전체369 = 343 PASS / 2 FAIL / 24 SKIP. D258 세 시험은 모두 PASS. 종료 appAfter 빈 배열.
단독 원본20회와 합치지 않는다. 두 실패는 아래 원문대로 내부 runner 복사본 신선도 검사에서 막혔으며, GUI만 빌드하고 두 runner를 빌드하지 않은 레인 준비 누락이다. 전체 통과·CI 성공으로 보고하지 않는다.
명령: `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=e2e.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-268\instrumented-full"`
## D258 실제 읽기·commit 순서
hostPID=19304, guiPID=40576, 두 프로세스 QPC frequency=10000000. D258_APP의 reflectionError=none.
아래 상대시각의0은 첫 Find(ChainStatusText).Name 호출 직전 QPC다. 호출 구간 안의 정확한 Name 값 평가 순간은 더 세분화하지 않았다.
| 사건 | UTC | QPC | 상대 ms |
|---|---|---:|---:|
| ChainStatusText 읽기 시작 | 2026-11-16T18:26:36.2705766Z | 13588922562629 | 0.000000 |
| 최초 non-null render 대입(null→1) | 2026-11-16T18:26:36.2887924Z | 13588922744763 | 18.213400 |
| 해당 파생 속성 알림 완료 | 2026-11-16T18:26:36.2904477Z | 13588922761312 | 19.868300 |
| ChainStatusText 읽기 종료 | 2026-11-16T18:26:36.3636495Z | 13588923493328 | 93.069900 |
| FrameTimingText 읽기 시작 | 2026-11-16T18:26:36.3638079Z | 13588923494920 | 93.229100 |
| FrameTimingText 읽기 종료 | 2026-11-16T18:26:36.3868288Z | 13588923725122 | 116.249300 |
| StatusBarText 읽기 시작 | 2026-11-16T18:26:36.3868304Z | 13588923725144 | 116.251500 |
| StatusBarText 읽기 종료 | 2026-11-16T18:26:36.4239516Z | 13588924096348 | 153.371900 |
반환값:
- ChainStatusText: preprocess skipped · enhance_basic applied · gsvg off · ai_bone_suppress off · auto C 4021.869 / W 1395.963
- FrameTimingText: preprocess 2.547 ms · enhance_basic 36.523 ms · gsvg 0 ms · ai_bone_suppress 0 ms
- StatusBarText: WARN: Preprocessing is on but was skipped: no selected calibration: all stages Off or Auto without maps; executed=[]; input kept The image is shown WITHOUT offset/gain/defect correction.
관측: 최초 footer 읽기 시작18.2134ms 뒤 최초 non-null render 대입이 일어났고, 읽기는 완료 문구를 반환해 PASS했다. 읽기 호출과 startup commit의 시간 겹침은 관측했으나 실패 회차의 직접 계측은 없다. CI의 혼합값 실패를 이 기록만으로 원인 확정하지 않는다. VM 상태/바인딩/UIA 표현 경계의 실제 실패 시점을 아직 구분하지 못했다.
원본 코드 연결: StateDisplayScenarios.cs:52의 세 UIA 순차 읽기, MainWindowViewModel.cs:1165~1172의 null footer 문구, :1211~1219의 SetRender, :3036의 ReportChain→:3063의 SetRender. 임시 계측 코드 자체는 commit하지 않으며 patch는 로컬에만 보존한다.
## 실패·SKIP 원문
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.DetachedViewerSyncScenarios.W16_ChangingTheModeInTheMainWindow_ReachesTheDetachedWindow
```text
W16 (F5/F8 pressed in the main window) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.QuickStartWalkthroughScenarios.W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings
```text
XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository. Last run by hand on 2026-10-15 against calib-cyan-v3 (GUI-C-247 report). XPE-SKIP-ALLOWED:272
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V07_APan_PutsTheImageWhereItWasAsked
```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.RealInputScenarios.R02_RealKeystrokes_EditTheVoiBox_AndARealTabMovesOn
```text
R02 (real key presses into a text box, then a real Tab) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.DetachedViewerSyncScenarios.W15_ScrollingTheDetachedViewer_MovesTheMainWindowsZoom
```text
W15 (the mouse wheel over the detached viewer) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D4_Control_TheSameLineRecordInsideTheFrame_IsAccepted
```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.SettingsWarningRecoveryScenarios.CopyWithNothingSelected_CannotLookLikeItWorked
```text
CopyWithNothingSelected (a mouse press on a disabled button) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D3_ALineRecordOutsideTheFrame_IsRefusedWithItsReason
```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F8", mode: "DifferenceHeatmap")
```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Legacy.LegacyStartupResponsivenessProbe.MeasureTheLegacyAppsStartUp_MessagePumpAndUiAutomation
```text
XPE_C235_RUNS is not set (this is a measurement, not a regression test).
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V05_SwipePosition_ResetRedrawsTheDividerAtTheMiddle
```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Legacy.LegacyStartupResponsivenessProbe.MeasureTheFlaUiWindowCall_WithASlowStartUpProbe
```text
XPE_C235_DELAYS is not set (this is a measurement, not a regression test).
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V02_ZoomScale_ResetDrawsFit
```text
Scrolling the viewport with the mouse wheel sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D2_AnUnsupportedVersionNumber_IsRefusedWithItsReason
```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```
### Failed — ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported
```text
The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: The copy of the gui app that ImageProcTest.E2E runs against is older than its sources, so this run would test the PREVIOUS build. 'D:\workspace-github\xpe-gui\gui\ImageProcTest.E2E\bin\Debug\net8.0-windows\ImageProcTest.dll' written 2026-11-16T13:34:58.4800031Z; newest source 'D:\workspace-github\xpe-gui\gui\ImageProcTest\ViewModels\MainWindowViewModel.cs' written 2026-11-16T18:09:06.3551965Z. Build the RUNNER, not the app: dotnet build gui/ImageProcTest.E2E/ImageProcTest.E2E.csproj -c Debug. Building gui/ImageProcTest alone does not refresh this copy.'.
```
### Failed — ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A04_SelfCheck_ActuallyRuns_AndThePassIsReported
```text
The self-check runner RAN and FAILED when launched from the app: 'Self-check FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: The copy of the gui app that ImageProcTest.SelfCheck runs against is older than its sources, so this run would test the PREVIOUS build. 'D:\workspace-github\xpe-gui\gui\ImageProcTest.SelfCheck\bin\Debug\net8.0-windows\ImageProcTest.dll' written 2026-11-16T13:34:58.4800031Z; newest source 'D:\workspace-github\xpe-gui\gui\ImageProcTest\ViewModels\MainWindowViewModel.cs' written 2026-11-16T18:09:06.3551965Z. Build the RUNNER, not the app: dotnet build gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj -c Debug. Building gui/ImageProcTest alone does not refresh this copy.'.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V04_PanY_ResetRecentres
```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V06_AnExplicitZoom_IsTheScaleThatWasDrawn
```text
Scrolling the viewport with the mouse wheel sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V03_PanX_ResetRecentres
```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D1_ATruncatedDefectFile_IsRefusedWithItsReason
```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F6", mode: "SplitLocked")
```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F7", mode: "OverlayOpacity")
```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F5", mode: "SwipeVertical")
```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.RealInputScenarios.R01_TheViewMenu_OpensWithARealMouseClick
```text
R01 (a real mouse click on the View menu) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.RealTestScenarios.R1_OneFrame_CorrectedWithASet_PicturesAndDataSaved
```text
XPE_C231_NATIVE_DIR/XPE_C250_CALIB/XPE_C247_RAW/XPE_C250_OUT/XPE_C250_TAG not set - the real-frame set is not in the repository. XPE-SKIP-ALLOWED:273
```
### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.QuickStartWalkthroughScenarios.W10_AnOldDllWithoutPipelineOut_FailsWithAnUpdateInstruction_AndProcessesNothing
```text
NOT RUN: W10 needs a native DLL folder that predates xpe_preprocess_pipeline_out (XPE_C232B_OLD_NATIVE_DIR); no such old build is in the repository or in the CI artifacts. Last run by hand on 2026-10-13 (GUI-C-232b report). XPE-SKIP-ALLOWED:265
```
## 원복 증거
원복 시각: 2026-11-16T18:50:02.571Z. git diff bytes=0, lines=0; clients/gui 전체 소스 diff 출력 없음.
- gui/ImageProcTest/ViewModels/MainWindowViewModel.cs: SHA256 03aeb70dd655cd97a4dd5bcfdc93003c45c810d6748cc88005836859c1b5f46a; 원본과 바이트 동일=True
- clients/ImageProcTest.E2ETests/Scenarios/Workflows/StateDisplayScenarios.cs: SHA256 b5ed5721384e9bf080cee4366706ad873a3f2f320a7b33ec26492fbd149f4454; 원본과 바이트 동일=True
원문 SHA256:
- e2e.stdout.txt: d55d7f2852206ff897c3b1235561c0c643e491ad0ead782b8eb7765fdd78f0f5
- e2e.stderr.txt: ccfbd96297d24d1384df0716ab0f41338e60e059c03503f3a8e5984d9b68cfbf
- e2e.trx: ce7dd8c3d80a81c66fbb6d8274e21e1773723c56ace86268acdc1e817964eb85
- trace/render-40576.tsv: 2fe60c6946d20315e22e4d243cae1d0096b8b065909f9f410f8dcd3e5f15cc9b
