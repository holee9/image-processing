# GUI-C-203 — every menu item, one row (dev/gui = main at 52a2b2a5)

Columns: item · where · enabled rule (XAML) · UIA enabled on Mock / Native with xpe_ai.dll / Native without it · what the click does · tests or automation-run steps that observe the BEHAVIOUR (not only the name: where a row says NONE, no test references the id, the command or the handler).

| item (AutomationId) | header | enabled rule | UIA Mock / Native+ai / Native-ai | what the click does | observed by |
|---|---|---|---|---|---|
| `OpenRawMenuItem` | Open Raw... | always enabled | on/on/on | file dialog, loads a raw frame (LoadImageCommand) | shell E2E Program; LifetimeScenarios; the automation run loads a frame through the same command |
| `OpenRecentMenuItem` | Open Recent | IsEnabled = HasRecentRawFiles | on/on/on | submenu of recent raw files (RecentRawFileMenuItem entries), each reloads that file | A08_RecentFileHistory_SurvivesTheProcess; shell E2E Program |
| `OpenDicomMenuItem` | Open DICOM... (Phase 1b) | always disabled (IsEnabled="False") | off/off/off | none (no command bound) | shell E2E Program asserts it is disabled |
| `SaveSettingsMenuItem` | Save Settings | always enabled | on/on/on | writes the settings file | shell E2E Program; the automation run reads the file before and after the click |
| `ExportAutomationReportMenuItem` | Export Automation Report | always enabled | on/on/on | writes the automation report | A03 and every automation run; GsvgWristSliceScenarios P05/P06 read the exported report |
| `ExportEvidenceBundleMenuItem` | Export Evidence Bundle | always enabled | on/on/on | zips the evidence bundle | A09_EvidenceBundleAndFolder_Answer; shell E2E Program |
| `ExitMenuItem` | Exit | always enabled | on/on/on | closes the window (Click handler ExitMenuItem_OnClick) | NONE (no test names it, closing the app is the test host's own act) |
| `InitializeBackendMenuItem` | Initialize Backend | always enabled | on/on/on | re-creates the backend from the settings and initializes it | BackendLifecycleTests, LifetimeScenarios, AiMenuAvailabilityScenarios.E02, shell E2E Program |
| `ShutdownBackendMenuItem` | Shutdown Backend | always enabled | on/on/on | shuts the backend down (background, ticketed) | BackendLifecycleTests, AiMenuAvailabilityScenarios.E02, LifetimeScenarios |
| `MockBackendModeMenuItem` | Mock | always enabled | on/on/on | sets BackendMode=Mock (checkable, follows the setting) | WorkflowMenuScenarios.W03/W03b; ComparisonModeSingleSourceTests-style source checks |
| `NativeBackendModeMenuItem` | Native | always enabled | on/on/on | sets BackendMode=Native | WorkflowMenuScenarios.W03/W03b; shell E2E Program |
| `NativeDllDiagnosticsMenuItem` | Native DLL Diagnostics | always enabled | on/on/on | writes a status line and a log line about the display DLL (ShowNativeDiagnostics) | NONE (command and handler are referenced by no test) |
| `OpenRuntimeLogsMenuItem` | Export Runtime Logs | always enabled | on/on/on | exports the runtime logs to a file (header says Export Runtime Logs) | A07_RuntimeLogs_AreWrittenToAFile |
| `PInvokeSmokeTestMenuItem` | Run P/Invoke Smoke Test | always enabled | on/on/on | runs the P/Invoke smoke test and records the verdict | MenuCommandScenarios.R06_PInvokeSmoke_AnswersFromTheNativeLibraries |
| `ShowCalibrationPanelMenuItem` | Calibration Paths Panel | always enabled | on/on/on | checkable: shows the calibration paths panel (persisted flag) | PanelToggleScenarios S07/S09, A12/A13 |
| `ShowDisplaySettingsPanelMenuItem` | Display Settings Panel | always enabled | on/on/on | checkable: shows the display settings panel (persisted flag) | PanelToggleScenarios, A12/A13 |
| `ShowLogsPanelMenuItem` | Logs Panel | always enabled | on/on/on | checkable: shows the log area | PanelToggleScenarios S06/S07 (many) |
| `ClearLogsMenuItem` | Clear Logs | always enabled | on/on/on | clears the log list | the automation run asserts LogCountAfterClear == 0 (behavior); shell E2E asserts the binding |
| `ClearAlertsMenuItem` | Clear Alerts | always enabled | on/on/on | clears the alert list | the automation run asserts AlertCountAfterClear == 0 (behavior); shell E2E asserts the binding |
| `ResetLayoutMenuItem` | Reset Layout | always enabled | on/on/on | restores panels/tab to defaults | PanelToggleScenarios.S10_ResetLayout_RestoresTheState |
| `ZoomFitMenuItem` | Zoom Fit | always enabled | on/on/on | sets the comparison zoom to fit | binding presence only (shell E2E asserts Command is not null); the automation run clicks it and asserts no result |
| `ZoomActualMenuItem` | Zoom 100% | always enabled | on/on/on | sets the comparison zoom to 100% | the automation run clicks it and records ComparisonZoomScale |
| `ZoomInMenuItem` | Zoom In | always enabled | on/on/on | raises the comparison zoom | NONE (ZoomInCommand is referenced by no test) |
| `ZoomOutMenuItem` | Zoom Out | always enabled | on/on/on | lowers the comparison zoom | NONE (ZoomOutCommand is referenced by no test) |
| `CompareSwipeMenuItem` | Swipe | always enabled | on/on/on | sets the comparison mode (checkable, F5) | ComparisonModeSingleSourceTests, ComparisonEntryPointScenarios W27 |
| `CompareSplitMenuItem` | Split | always enabled | on/on/on | sets the comparison mode (F6) | ComparisonModeSingleSourceTests, ComparisonEntryPointScenarios |
| `CompareOverlayMenuItem` | Overlay | always enabled | on/on/on | sets the comparison mode (F7) | ComparisonModeSingleSourceTests, ComparisonEntryPointScenarios |
| `CompareDifferenceMenuItem` | Difference | always enabled | on/on/on | sets the comparison mode (F8) | ComparisonModeSingleSourceTests, ComparisonEntryPointScenarios W27 |
| `CompareSourceOnlyMenuItem` | Source Only | always enabled | on/on/on | sets the comparison mode (no shortcut) | ComparisonModeSingleSourceTests, ComparisonEntryPointScenarios |
| `CompareProcessedOnlyMenuItem` | Processed Only | always enabled | on/on/on | sets the comparison mode (no shortcut) | ComparisonModeSingleSourceTests, ComparisonEntryPointScenarios |
| `ResetComparisonViewMenuItem` | Reset Comparison View | always enabled | on/on/on | resets pan/zoom of the comparison view | ViewStateRenderScenarios (ResetView helper) |
| `DetachComparisonViewerMenuItem` | Detach Comparison Viewer | always enabled | on/on/on | opens the detached viewer window | DetachViewerScenarios.S11_DetachComparisonViewer_OpensTheWindow |
| `ApplyDisplayPipelineMenuItem` | Apply Display Pipeline | always enabled | on/on/on | renders modality/VOI/presentation LUTs on the loaded frame; with no frame it says so on the status line | MenuCommandScenarios.R11b, many E2E scenarios, the automation run |
| `RunPreprocessingMenuItem` | Run Preprocessing (Phase 1a) | IsEnabled = CanRunPreprocessing | off/on/on | runs the preprocess stage and re-renders | ClampAlertOnScreenScenarios, ProcessingChainScenarios C01..C07, NonlinearityNoopAlertScenarios |
| `RunDeterministicBaselineMenuItem` | Run Deterministic Baseline (Phase 1b) | IsEnabled = CanRunDeterministicBaseline | off/on/on | runs the fixed chain twice, bit comparison, DICOM write (GUI-C-196) | DeterministicBaselineScenarios B01/B02, AutomationReportBackendTests A18..A21, BaselineExecutionTests |
| `RunFullPipelineMenuItem` | Run AI Bone Suppression | IsEnabled = AiBoneSuppressionAvailability.CanRun, Mode=OneWay | off/on/off | runs AI bone suppression on the loaded frame (header 'Run AI Bone Suppression') | ProcessingChainScenarios C08/C09/C09b/C10, AiMenuAvailabilityScenarios E01/E02, AiMenuLateBindingScenarios L01/L02 |
| `StopProcessingMenuItem` | Stop Processing | always enabled | on/on/on | discards the render in flight; with nothing running says so | MenuCommandScenarios.R11b; the automation run drives the command |
| `StageTimingMenuItem` | Stage Timing | always enabled | on/on/on | shows the stage times the last render reported | MenuCommandScenarios.R12_StageTiming_ShowsTheValuesTheRenderAlreadyReported |
| `OpenPipelineDiagnosticsMenuItem` | Open Pipeline Diagnostics | always enabled | on/on/on | opens the last chain/diagnostics report | A10_PipelineDiagnostics_KeepsNoMeasurementApartFromZero; shell E2E Program |
| `CalibrationSettingsMenuItem` | Calibration Settings | always enabled | on/on/on | opens the calibration settings panel | PanelToggleScenarios.S09_CalibrationSettings_OpensThePanelItAnnounces |
| `FixtureManagerMenuItem` | Fixture Manager | always enabled | on/on/on | reports whether the fixture pack is present | WorkflowMenuScenarios.W10_FixtureManager_ReportsWhetherTheFixturePackIsPresent |
| `ExportEvidenceSnapshotMenuItem` | Export Evidence Snapshot | always enabled | on/on/on | the SAME command as Export Automation Report (ExportAutomationReportCommand) | NONE by its own id (the shared command is tested through the File item) |
| `OpenEvidenceFolderMenuItem` | Open Evidence Folder | always enabled | on/on/on | opens the evidence folder | A09_EvidenceBundleAndFolder_Answer; shell E2E Program |
| `RunSelfCheckMenuItem` | Run Self-Check | always enabled | on/on/on | runs gui/ImageProcTest.SelfCheck as a child process and reports its verdict | A04/A05/A15 (the automation run clicks it) |
| `RunGuiE2EMenuItem` | Run GUI E2E | always enabled | on/on/on | runs gui/ImageProcTest.E2E as a child process | A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported |
| `BenchmarkRunnerMenuItem` | Benchmark Runner (Phase 1a+) | always enabled | on/on/on | describes the build tree / runs the benchmark ctest when a tree exists | A14_BenchmarkRunner_DescribesTheBuildTree_AndReportsNoVerdictItDidNotGet, BenchmarkRunnerServiceTests |
| `QaConstancyMenuItem` | QA Constancy (Phase 1b+) | always disabled (IsEnabled="False") | off/off/off | none (no command bound) | none needed: disabled (counted by the three-way census) |
| `GsdfCalibrateMenuItem` | GSDF Calibrate... (Phase 1b+) | always disabled (IsEnabled="False") | off/off/off | none (no command bound) | none needed: disabled (counted by the three-way census) |
| `OpenHelpIndexMenuItem` | Help Home | always enabled | on/on/on | opens the offline help home in the in-app help window | SmokeScenarios.S04_HelpMenu_OffersOfflineHelpHome; shell E2E Program |
| `OpenQuickStartHelpMenuItem` | Quick Start | always enabled | on/on/on | opens the quick start page | NO behavior test: shell E2E only fetches the control (a name census); nothing opens the page |
| `OpenScopeHelpMenuItem` | Scope and Limitations | always enabled | on/on/on | opens the scope page | NO behavior test: shell E2E only fetches the control; nothing opens the page |
| `OpenCurrentWorkflowHelpMenuItem` | Current Workflow Help | always enabled | on/on/on | opens the workflow page | NO behavior test: the automation run reads only IsEnabled (PlannedMenuPlaceholdersDetected); nothing opens the page |
| `OpenApiReferenceMenuItem` | API Reference (Doxygen) | always enabled | on/on/on | opens the generated Doxygen index, or says how to generate it | A11_ApiReference_ClaimMatchesTheDisk_AndTheCountsAgree |
| `OpenTroubleshootingMenuItem` | Troubleshooting | always enabled | on/on/on | opens the generated troubleshooting page, or says how to generate it | A16/A17 (the automation run clicks it) |
| `AboutBuildInfoMenuItem` | About / Build Info | always enabled | on/on/on | shows the build information (Click handler) | NONE |

leaf items: 55; containers (menus and submenus): 8; plus one dynamic item (`RecentRawFileMenuItem`) under Open Recent.
items without a hand annotation (must be none): none
