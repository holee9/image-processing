# GUI-C-263 시험 결과

## 지정 6개

| 환경 | 시험 | 결과 |
|---|---|---|
| with-root | ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | Passed |
| with-root | ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.IB01_InitializeBackend_StartsANewRunSet | Passed |
| with-root | ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.EB02_TwoRunSets_KeepSeparateEvidence | Passed |
| without-root | ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.IB01_InitializeBackend_StartsANewRunSet | Passed |
| without-root | ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative | Passed |
| without-root | ImageProcTest.E2ETests.Scenarios.Workflows.EvidenceBundleScenarios.EB02_TwoRunSets_KeepSeparateEvidence | Passed |

## 전체 실행 실패·SKIP 원문

전체371 = 345P/1F/25SKIP. NotExecuted 25개는 SKIP 사유와 함께 아래 기록한다.

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.RealTestScenarios.R1_OneFrame_CorrectedWithASet_PicturesAndDataSaved

```text
XPE_C231_NATIVE_DIR/XPE_C250_CALIB/XPE_C247_RAW/XPE_C250_OUT/XPE_C250_TAG not set - the real-frame set is not in the repository. XPE-SKIP-ALLOWED:273
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.QuickStartWalkthroughScenarios.W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings

```text
XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository. Last run by hand on 2026-10-15 against calib-cyan-v3 (GUI-C-247 report). XPE-SKIP-ALLOWED:272
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D3_ALineRecordOutsideTheFrame_IsRefusedWithItsReason

```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V06_AnExplicitZoom_IsTheScaleThatWasDrawn

```text
Scrolling the viewport with the mouse wheel sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Legacy.LegacyStartupResponsivenessProbe.MeasureTheLegacyAppsStartUp_MessagePumpAndUiAutomation

```text
XPE_C235_RUNS is not set (this is a measurement, not a regression test).
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V05_SwipePosition_ResetRedrawsTheDividerAtTheMiddle

```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D2_AnUnsupportedVersionNumber_IsRefusedWithItsReason

```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.RealInputScenarios.R02_RealKeystrokes_EditTheVoiBox_AndARealTabMovesOn

```text
R02 (real key presses into a text box, then a real Tab) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V04_PanY_ResetRecentres

```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D4_Control_TheSameLineRecordInsideTheFrame_IsAccepted

```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V03_PanX_ResetRecentres

```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Legacy.LegacyStartupResponsivenessProbe.MeasureTheFlaUiWindowCall_WithASlowStartUpProbe

```text
XPE_C235_DELAYS is not set (this is a measurement, not a regression test).
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V07_APan_PutsTheImageWhereItWasAsked

```text
Moving the mouse sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.RealInputScenarios.R01_TheViewMenu_OpensWithARealMouseClick

```text
R01 (a real mouse click on the View menu) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ViewStateRenderScenarios.V02_ZoomScale_ResetDrawsFit

```text
Scrolling the viewport with the mouse wheel sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.DetachedViewerSyncScenarios.W15_ScrollingTheDetachedViewer_MovesTheMainWindowsZoom

```text
W15 (the mouse wheel over the detached viewer) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F6", mode: "SplitLocked")

```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.SettingsWarningRecoveryScenarios.CopyWithNothingSelected_CannotLookLikeItWorked

```text
CopyWithNothingSelected (a mouse press on a disabled button) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.DetachedViewerSyncScenarios.W16_ChangingTheModeInTheMainWindow_ReachesTheDetachedWindow

```text
W16 (F5/F8 pressed in the main window) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.QuickStartWalkthroughScenarios.W10_AnOldDllWithoutPipelineOut_FailsWithAnUpdateInstruction_AndProcessesNothing

```text
NOT RUN: W10 needs a native DLL folder that predates xpe_preprocess_pipeline_out (XPE_C232B_OLD_NATIVE_DIR); no such old build is in the repository or in the CI artifacts. Last run by hand on 2026-10-13 (GUI-C-232b report). XPE-SKIP-ALLOWED:265
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F7", mode: "OverlayOpacity")

```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F8", mode: "DifferenceHeatmap")

```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### Failed — ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported

```text
The GUI E2E runner RAN and FAILED when launched from the app: 'GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.'.
   at ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported() in D:\workspace-github\xpe-gui\clients\ImageProcTest.E2ETests\Scenarios\Smoke\AutomationReportBackendTests.cs:line 186
   at System.RuntimeMethodHandle.InvokeMethod(Object target, Void** arguments, Signature sig, Boolean isConstructor)
   at System.Reflection.MethodBaseInvoker.InvokeWithNoArgs(Object obj, BindingFlags invokeAttr)
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Legacy.LegacyInputKindScenarios.K1_TheInputKindChoice_ReachesThePreview_AndChoosingAnotherKind_InvalidatesOnlyTheMeasuredRows

```text
this xpe_preprocess.dll does not export xpe_verify_offset_signed_file (older than dev/preprocess 026ea384). XPE-SKIP-ALLOWED:273
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Workflows.ComparisonEntryPointScenarios.W14_BoundKeyGesture_SelectsThatMode(gesture: "F5", mode: "SwipeVertical")

```text
W-14 (a bound key gesture) sends real mouse/keyboard input to whichever window is in front, which on a shared desktop is not the application under test (GUI-C-171). Skipped; set XPE_E2E_ALLOW_GLOBAL_INPUT=1 on a desktop nothing else is using to run it.
```

### NotExecuted — ImageProcTest.E2ETests.Scenarios.Operator.DefectMapV2Scenarios.D1_ATruncatedDefectFile_IsRefusedWithItsReason

```text
XPE_C231_NATIVE_DIR/XPE_C249_SET/XPE_C249_WORK/XPE_C247_RAW not set - the version-2 set is not in the repository. XPE-SKIP-ALLOWED:273
```
