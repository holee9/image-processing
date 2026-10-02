using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.ViewModels;

namespace ImageProcTest;

public partial class MainWindow : System.Windows.Window
{
    private readonly HelpBundleService _helpBundleService = new();

    /// <summary>Where this run's settings are read from and written to. #136: not the shipped file under automation.</summary>
    private readonly string _settingsFilePath;

    public MainWindow()
    {
        InitializeComponent();

        var (settings, settingsService, preservedSettingsPath, preservedIsFromEarlier) = CreateSettings();
        _settingsFilePath = settingsService.FilePath;
#if XPE_TEST_FAULTS
        // #171 (GUI-C-79): Wrap returns the real backend untouched unless --automation-fault was given. Test builds only (GUI-C-193).
        var failAfter = App.AutomationDisplayPipelineFailAfter;
        var aiWorkerDisabled = App.AutomationAiWorkerDisabled;
        var aiWorkerSilentAfter = App.AutomationAiWorkerSilentAfter;
        var viewModel = new MainWindowViewModel(
            settings,
            settingsService,
            s => FaultInjectingBackend.Wrap(XpeBackendFactory.Create(s), failAfter, aiWorkerDisabled, aiWorkerSilentAfter),
            preservedSettingsPath,
            preservedIsFromEarlier);
#else
        var viewModel = new MainWindowViewModel(
            settings,
            settingsService,
            XpeBackendFactory.Create,
            preservedSettingsPath,
            preservedIsFromEarlier);
#endif
        DataContext = viewModel;

#if XPE_TEST_FAULTS
        if (FaultInjectingBackend.Armed is not null)
        {
            // Loud on purpose: a window carrying an injected fault must not look like a normal one.
            // The title is bound to WindowTitle, which adds the marker once this is announced.
            viewModel.AnnounceFaultInjection();
        }
#endif

        Loaded += OnLoaded;

        // A Windows session end must not be held up by a cancelled close (GUI-C-186e).
        if (System.Windows.Application.Current is { } application)
        {
            application.SessionEnding += (_, _) => _forceClose = true;
        }
    }

    /// <summary>
    /// #136: an automation run starts from defaults and writes to a throwaway file.
    ///
    /// Settings are persisted, so without this a check could be decided by whatever a PREVIOUS run
    /// (or a human sitting at the app) happened to leave behind: measured — with the identical
    /// binary and scenario, a leftover selectedBodyPart of "Lung" fails the run and "Abdomen"
    /// passes it, while the scenario never sets that value at all.
    ///
    /// BackendMode is the one value carried over: it is the operator's input selecting WHICH backend
    /// this run exercises, not state the run produced.
    /// </summary>
    /// <summary>
    /// Applies every run-selection switch to a settings object — one place, every launch mode.
    ///
    /// GUI-C-38. The two that came before it were the same defect twice: <c>--automation-backend</c>
    /// (GUI-C-31) and <c>--automation-calib</c> (GUI-C-37) were each applied only inside the
    /// isolation branch, which runs when <c>--automation-report</c> is ALSO given. The E2E fixture
    /// deliberately omits the report, so both switches were passed and silently ignored there —
    /// once producing "mode=Mock" on a Native run, once "calibration file(s) not found" while the
    /// files existed.
    ///
    /// The split that caused it is now gone: WHAT the run selects is applied here unconditionally,
    /// and only WHERE settings are stored (the isolated file, and starting from defaults) stays
    /// conditional on automation mode. A launch selection is not a stored value.
    ///
    /// Every field of <see cref="AutomationArgs"/> that selects run behaviour must be consumed by
    /// this method; <c>AutomationRunSelectionConsumptionTests</c> fails when one is not.
    /// </summary>
    private static void ApplyRunSelection(AppSettings settings)
    {
        if (!string.IsNullOrWhiteSpace(App.AutomationBackendMode))
        {
            settings.BackendMode = App.AutomationBackendMode;
        }

        if (!string.IsNullOrWhiteSpace(App.AutomationCalibrationDirectory))
        {
            // One generated set feeds all three stages (#141).
            settings.OffsetCalibrationDirectory = App.AutomationCalibrationDirectory;
            settings.GainCalibrationDirectory = App.AutomationCalibrationDirectory;
            settings.DefectCalibrationDirectory = App.AutomationCalibrationDirectory;
        }

        if (App.AutomationRawWidth is > 0)
        {
            settings.RawWidth = App.AutomationRawWidth.Value;
        }

        if (App.AutomationRawHeight is > 0)
        {
            settings.RawHeight = App.AutomationRawHeight.Value;
        }
    }

    /// <summary>
    /// Builds this run's settings and the service that persists them.
    ///
    /// Two decisions, deliberately separate:
    ///  - WHAT the run selects → <see cref="ApplyRunSelection"/>, applied in every launch mode.
    ///  - WHERE settings live → isolated to a throwaway file, and started from defaults, ONLY under
    ///    automation (#136): settings are persisted, so a check could otherwise be decided by
    ///    whatever a previous run left behind.
    /// </summary>
    private static (AppSettings Settings, AppSettingsService Service, string? PreservedOriginalPath, bool PreservedIsFromAnEarlierFailure) CreateSettings()
    {
        // #173 (GUI-C-119): --automation-settings names the file this run reads and writes, so an E2E
        // case can put a deliberately corrupt file in front of a real launch. Without it the automation
        // branch below starts from defaults in a throwaway directory and the unreadable-file path could
        // only be claimed from reading the code — which is the kind of claim this issue keeps removing.
        var settingsPath = App.AutomationSettingsPath;
        if (!string.IsNullOrWhiteSpace(settingsPath))
        {
            var named = new AppSettingsService(settingsPath);
            var namedResult = named.Load();
            ApplyRunSelection(namedResult.Settings);
            return (namedResult.Settings, named, namedResult.PreservedOriginalPath, namedResult.PreservedIsFromAnEarlierFailure);
        }

        var shipped = new AppSettingsService();

        if (!App.IsAutomationMode)
        {
            var persisted = shipped.Load();
            ApplyRunSelection(persisted.Settings);
            return (persisted.Settings, shipped, persisted.PreservedOriginalPath, persisted.PreservedIsFromAnEarlierFailure);
        }

        var isolated = new AppSettings();
        ApplyRunSelection(isolated);

        // BackendMode carries over from the shipped file when no switch named one — it is the
        // operator's input selecting WHICH backend this run exercises, not state the run produced.
        if (string.IsNullOrWhiteSpace(App.AutomationBackendMode))
        {
            isolated.BackendMode = shipped.Load().Settings.BackendMode;
        }

        var isolatedDirectory = Path.Combine(
            Path.GetTempPath(),
            $"xpe_gui_automation_{Guid.NewGuid():N}");

        return (isolated, new AppSettingsService(Path.Combine(isolatedDirectory, "appsettings.json")), null, false);
    }

    // GUI-C-186e (Codex #33): closing the window waits for the AI session gate (a frame that waits on a silent worker holds it for
    // up to the module's time budget), so the first close request only STARTS the shutdown in the background and keeps the window
    // open, responsive, saying "Backend shutting down..."; the window closes itself when the shutdown is done. There is no upper
    // limit and no "close anyway": a native call cannot be interrupted, the wait ends inside the module's own time budget, and
    // whether the worker process would be left behind by a forced exit could not be checked.
    private bool _closeAfterShutdown;
    private bool _closeScheduled;
    private bool _forceClose;

    protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
    {
        if (!_closeAfterShutdown && !_forceClose && DataContext is MainWindowViewModel viewModel)
        {
            e.Cancel = true;
            viewModel.StopAiStatusUpdates(); // a status read started or running now must not touch the closing screen (GUI-C-186d)
            if (!_closeScheduled)
            {
                _closeScheduled = true;
                viewModel.BeginShutdown(() =>
                {
                    _closeAfterShutdown = true;
                    Close();
                });
            }
        }

        base.OnClosing(e);
    }

    protected override void OnClosed(System.EventArgs e)
    {
        if (DataContext is MainWindowViewModel viewModel)
        {
            viewModel.StopAiStatusUpdates();
            if (!_closeAfterShutdown)
            {
                // Only reached when the close could not wait: the automation self-run ends with Application.Shutdown(code), and a
                // Windows session end must not be held up by a cancelled close. Nothing is left to keep responsive.
                viewModel.ShutdownBackendBlocking();
            }
        }

        base.OnClosed(e);
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        if (App.IsAutomationMode)
        {
            await RunAutomationScenarioAsync();
            return;
        }

        if (!string.IsNullOrWhiteSpace(App.AutomationRawPath))
        {
            // Dimensions are applied by ApplyRunSelection at construction (GUI-C-38); re-applying
            // them here duplicated the rule in a second place, which is how the two earlier
            // switch-wiring defects survived.
            ClickButton(LoadRawImageButton);
        }
    }

    private async Task RunAutomationScenarioAsync()
    {
        var report = new GuiAutomationReport();

        try
        {
            if (DataContext is not MainWindowViewModel viewModel)
            {
                throw new InvalidOperationException("DataContext is not MainWindowViewModel.");
            }

            // #225 (GUI-C-168) row 13: read before anything renders. This is the "never rendered"
            // state, and it can only be observed here — one line later the scenario has a chain.
            report.PipelineDiagnosticsHadMeasurementAtStartup = viewModel.HasPipelineDiagnostics;

            // #225 (GUI-C-160) row 1: read FIRST, before this run loads anything — what is here now came
            // from a previous process through the settings file, and that is the feature.
            report.RecentRawFileCountAtStartup = viewModel.RecentRawFiles.Count;

            report.BackendVersion = viewModel.RuntimeInfo.Version;
            report.BackendMode = viewModel.Settings.BackendMode;
            report.BackendModeSource = string.IsNullOrWhiteSpace(App.AutomationBackendMode) ? "file" : "arg";
            report.NativeSource = viewModel.RuntimeInfo.NativeSource;
            report.ActualBackendMode = viewModel.ActualBackendMode;
            report.MockBackend = viewModel.IsMockBackend;
            // #175 (GUI-C-83): a run that asked for one backend and exercised another has not verified
            // what it was asked to verify, however well everything else went.
            report.BackendMatchesRequest = string.Equals(
                report.BackendMode, report.ActualBackendMode, StringComparison.OrdinalIgnoreCase);
            report.InitialLogCount = viewModel.Logs.Count;
            report.InitialAlertCount = viewModel.Alerts.Count;

            // Dimensions come from ApplyRunSelection (GUI-C-38) — one rule, one place.
            ClickButton(LoadRawImageButton);
            await Task.Delay(1500);

            viewModel.Settings.OffsetCorrectionMode = CalibrationStageMode.Off;
            viewModel.Settings.DefectCorrectionMode = CalibrationStageMode.On;
            await Task.Delay(100);

            // #135: park the window at a sentinel no preset produces, so "applied" below means the
            // command actually wrote the backend's values HERE — settings are persisted between
            // runs, so a previous run's window would otherwise satisfy the check on its own.
            viewModel.Settings.VoiWindowCenter = -1.0f;
            viewModel.Settings.VoiWindowWidth = 1.0f;

            viewModel.ApplyBodyPartPresetCommand.Execute(null);
            await Task.Delay(250);
            viewModel.ApplyDisplayPipelineCommand.Execute(null);
            await Task.Delay(1500);

            report.LogCountAfterLoad = viewModel.Logs.Count;
            report.AlertCountAfterLoad = viewModel.Alerts.Count;
            report.ActiveImageSummary = viewModel.ActiveImageSummary;
            report.StatusAfterLoad = viewModel.StatusText;
            report.LastRawDirectory = viewModel.Settings.LastRawDirectory;
            report.DisplayPipelineApplied = viewModel.ActiveImageFrame?.DisplayPipelineApplied ?? false;

            // #225 (GUI-C-168) row 13, third distinction — measured HERE, right after the first
            // successful render, and not later beside the panel. Measured why: by the time the panel is
            // opened the row-11 stop test has already left PreviewStaleReason = StalePipelineFailed, and
            // that reason is documented as NOT replaced by a later parameter edit. Reading it there
            // showed a stale reason that had nothing to do with the edit — true, and not the thing being
            // checked. Isolating it is the only way the parameters-changed path is actually observed.
            report.StaleReasonBeforeParameterEdit = viewModel.PreviewStaleReason;
            viewModel.Settings.VoiWindowCenter += 100.0f;
            await Task.Delay(150);
            report.StaleReasonAfterParameterEdit = viewModel.PreviewStaleReason;
            viewModel.Settings.VoiWindowCenter -= 100.0f;
            await Task.Delay(150);
            report.StaleReasonAfterParameterRestored = viewModel.PreviewStaleReason;

            // #141: preprocessing needs a loaded frame, so it runs HERE — measured: placed earlier
            // it reported "menu enabled=True, frame loaded=False" and never attempted.
            if (RunPreprocessingMenuItem.IsEnabled)
            {
                ClickMenuItem(RunPreprocessingMenuItem);
                await Task.Delay(3000);
            }

            report.PreprocessRan = viewModel.PreprocessRan;

            // Record WHY when nothing ran: "entry disabled" (backend has no preprocessing) is a
            // different fact from "it ran and refused". Reporting them alike hides one behind the other.
            report.PreprocessStages = string.IsNullOrEmpty(viewModel.PreprocessStages)
                ? $"not attempted (menu enabled={RunPreprocessingMenuItem.IsEnabled}, " +
                  $"frame loaded={viewModel.ActiveImageFrame is not null})"
                : viewModel.PreprocessStages;
            report.ChainStatus = viewModel.ChainStatus;
            report.ChainStages = viewModel.LastChain?.Stages.Select(s => $"{s.StageId}={s.Status} {s.ElapsedMs:0}ms").ToList() ?? new();
            report.DisplayPipelineSummary = viewModel.DisplayPipelineSummary;
            report.CalibrationEvaluationSummary = viewModel.CalibrationEvaluationSummary;
            report.OffsetCorrectionMode = viewModel.Settings.OffsetCorrectionMode;
            report.DefectCorrectionMode = viewModel.Settings.DefectCorrectionMode;
            // #225 rows 7 and 8 (GUI-C-170): the persisted flags, as they stood when the run started.
            // Reported, never part of the verdict below.
            report.DisplayPanelVisible = viewModel.Settings.ShowDisplayPanel;
            report.CalibrationPanelVisible = viewModel.Settings.ShowCalibrationPanel;
            // ...and whether the panels were actually ON SCREEN at that moment. A persisted flag that is true
            // while nothing shows is exactly the state these flags were in before GUI-C-170.
            report.CalibrationPanelRenderedAtStart =
                FindDescendant<Views.CalibrationPathsPanel>(this) is { IsVisible: true, ActualWidth: > 0 };
            report.DisplayPanelRenderedAtStart =
                FindDescendant<Views.DisplaySettingsPanel>(this) is { IsVisible: true, ActualWidth: > 0 };
            report.DisplayVersion = viewModel.RuntimeInfo.DisplayVersion;
            // #172 (GUI-C-79): measured. This was the constant true, and it said "detected" for the
            // whole time the main viewport received no images at all (GUI-C-78). It now requires the
            // workbench viewport to exist AND to have been handed both images.
            report.ComparisonViewportDetected = FindDescendant<ImageProcTest.Controls.ImageComparisonViewport>(this)
                is { SourceImage: not null, ProcessedImage: not null };
            report.ComparisonMode = viewModel.Settings.ComparisonMode;
            report.ComparisonZoomScale = viewModel.Settings.ComparisonZoomScale;
            report.ComparisonSwipePosition = viewModel.Settings.ComparisonSwipePosition;
            report.ComparisonSourcePreserved =
                viewModel.ActiveImageFrame?.Preview is not null &&
                ReferenceEquals(viewModel.SourceImage, viewModel.ActiveImageFrame.Preview);
            // #135: compare against what the ACTIVE backend produced, not against MockXpeBackend's
            // literals. The native abdomen preset is C=40/W=400 (HU, "clinically validated" per
            // display_api.h), so hard-coded mock values made this false for every native run while
            // the preset had in fact been applied correctly.
            var appliedPreset = viewModel.LastAppliedVoiPreset;
            report.VoiPresetCenter = appliedPreset?.Center ?? 0.0f;
            report.VoiPresetWidth = appliedPreset?.Width ?? 0.0f;
            report.VoiPresetApplied =
                string.Equals(viewModel.Settings.SelectedBodyPart, "Abdomen", StringComparison.OrdinalIgnoreCase) &&
                appliedPreset is not null &&
                appliedPreset.Width > 0.0f &&
                Math.Abs(viewModel.Settings.VoiWindowCenter - appliedPreset.Center) < 0.001f &&
                Math.Abs(viewModel.Settings.VoiWindowWidth - appliedPreset.Width) < 0.001f;

            ClickMenuItem(ZoomActualMenuItem);
            await Task.Delay(100);
            report.ComparisonZoomScale = viewModel.Settings.ComparisonZoomScale;
            ClickMenuItem(ZoomFitMenuItem);
            await Task.Delay(100);

            // #225 (GUI-C-160) row 1: read the settings file on disk BEFORE the Save click below.
            // The script presses Save, so a check made after it cannot separate "the app persisted the
            // history itself" from "the operator happened to save" — measured: removing the app's own
            // save left the A-08 scenario green. Attribution needs the reading to sit between the two
            // actions (#201).
            report.RecentHistoryPersistedBeforeSave =
                viewModel.RecentRawFiles.FirstOrDefault() is { } newest &&
                File.Exists(_settingsFilePath) &&
                File.ReadAllText(_settingsFilePath).Contains(
                    newest.Path.Replace("\\", "\\\\"), StringComparison.OrdinalIgnoreCase);

            ClickButton(SaveSettingsButton);
            await Task.Delay(250);

            report.TopLevelMenuCount = MainMenu.Items.Count;
            report.CanonicalMenuGroupsDetected =
                FileMenu is not null &&
                BackendMenu is not null &&
                ViewMenu is not null &&
                PipelineMenu is not null &&
                ToolsMenu is not null &&
                HelpMenu is not null;
            // GUI-C-169: NAMED "placeholders detected" but no longer tests any placeholder. Cards 160, 163 and
            // 168 each removed the item they implemented, correctly, and the last removal left only the
            // Help term below — so today this is "the Help menu item is enabled". Kept (that check is
            // real), not renamed (a report field others may read), and the placeholder question is now
            // answered by DisabledFutureCommandCount == UnimplementedMenuLeafCount in the verdict.
            report.PlannedMenuPlaceholdersDetected =
                // OpenRecentMenuItem left this list in GUI-C-160: #225 row 1 is implemented. It is
                // enabled whenever the history is non-empty, so requiring it to be disabled would make
                // the flag depend on whether a file had been opened yet rather than on what is built.
                // ExportEvidenceBundleMenuItem and OpenEvidenceFolderMenuItem left this list in
                // GUI-C-163: #225 rows 3 and 14 are implemented.
                // OpenRuntimeLogsMenuItem left this list in GUI-C-160: #225 row 5 is implemented, so
                // requiring it to be disabled would make this flag claim the opposite of what the app
                // does — the same correction RunPreprocessingMenuItem got in GUI-C-36.
                // OpenPipelineDiagnosticsMenuItem left this list in GUI-C-168: #225 row 13 is implemented.
                OpenCurrentWorkflowHelpMenuItem.IsEnabled;
            report.ToolbarMenuCommandParity =
                ReferenceEquals(InitializeBackendButton.Command, InitializeBackendMenuItem.Command) &&
                ReferenceEquals(ShutdownBackendButton.Command, ShutdownBackendMenuItem.Command) &&
                ReferenceEquals(LoadRawImageButton.Command, OpenRawMenuItem.Command) &&
                ReferenceEquals(SaveSettingsButton.Command, SaveSettingsMenuItem.Command) &&
                ReferenceEquals(ClearLogsButton.Command, ClearLogsMenuItem.Command) &&
                ReferenceEquals(ClearAlertsButton.Command, ClearAlertsMenuItem.Command);
            report.DisabledFutureCommandCount = new[]
                {
                    OpenDicomMenuItem,
                    NativeBackendModeMenuItem,
                    PInvokeSmokeTestMenuItem,
                    ZoomFitMenuItem,
                    ZoomActualMenuItem,
                    // RunPreprocessingMenuItem is no longer a placeholder (#141, GUI-C-36): it is
                    // enabled on the native backend, so counting it as a disabled future command
                    // would make this report claim the opposite of what the app now does.
                    // RunDeterministicBaselineMenuItem is a command now (#225 row 9, GUI-C-196 M4): enabled on the native backend, disabled on Mock with a command
                    // still bound, so it left this list exactly as RunPreprocessingMenuItem did.
                    StopProcessingMenuItem,
                    StageTimingMenuItem,
                    RunSelfCheckMenuItem,
                    RunGuiE2EMenuItem,
                    // #225 row 17 (GUI-C-176): the Benchmark Runner is real now and enabled, so it left the list.
                    QaConstancyMenuItem,
                    GsdfCalibrateMenuItem,
                    // #225 rows 7 and 8 (GUI-C-170): the two panel toggles that were counted here
                    // (#165, GUI-C-65) are real now and enabled, so they left the list.
                    // #225 row 21 (GUI-C-181): Troubleshooting opens its generated page now, so it left the list too.
                }
                .Count(item => !item.IsEnabled);

            // #225 (GUI-C-169): the independent second derivation of the same number. See the verdict below
            // for why this replaced a floor.
            report.UnimplementedMenuLeafCount = CountUnimplementedMenuLeaves(MainMenu.Items);

            // #225 (GUI-C-154, row 6): run the smoke through the MENU, so the automation run exercises
            // the same command a user has, and record its verdict. Invoked before the report is exported
            // so the value is in the file.
            ClickMenuItem(PInvokeSmokeTestMenuItem);
            await Task.Delay(200);
            report.PInvokeSmokeTestPassed = viewModel.PInvokeSmokeTestPassed;
            report.PInvokeSmokeTestDetail = viewModel.PInvokeSmokeTestDetail;

            // #225 (GUI-C-154) rows 11 and 12, through the menu items a user has.
            ClickMenuItem(StageTimingMenuItem);
            await Task.Delay(150);
            report.StageTimingReport = viewModel.LastStageTimingReport;

            // Row 11, the case that matters: stop a render that IS in flight.
            //
            // Driven through the commands rather than the menu, and the reason is not convenience: the
            // render is 2.3-2.5 s on the wrist slice but ~16 ms on this synthetic frame, and opening a
            // menu takes longer than that. A menu-driven version would be a race the run sometimes lost,
            // which is a flaky test rather than an observation. ApplyDisplayPipelineAsync creates its
            // cancellation source synchronously, before its first await, so a stop issued on the next
            // line always lands while the render is in flight.
            viewModel.ApplyDisplayPipelineCommand.Execute(null);
            viewModel.StopProcessingCommand.Execute(null);
            await Task.Delay(1500);
            report.StopInFlightStatus = viewModel.StatusText;
            report.StoppedRenderCount = viewModel.StoppedRenderCount;

            // Pressed with no render in flight: the defined no-op, recorded so "it does nothing" is
            // distinguishable from "it did nothing and said nothing". This runs AFTER the render above
            // has finished, which is the state a user is in most of the time.
            ClickMenuItem(StopProcessingMenuItem);
            await Task.Delay(150);
            report.StopWithNothingRunningStatus = viewModel.StatusText;

            // #225 (GUI-C-158) row 15. Driven through the menu, then awaited: the runner takes about a
            // second (GUI-C-157) and the command is deliberately asynchronous, so reading the verdict
            // immediately would record the state before it finished.
            ClickMenuItem(RunSelfCheckMenuItem);
            for (var waited = 0; waited < 60 && viewModel.SelfCheckRunning; waited++)
            {
                await Task.Delay(250);
            }

            report.SelfCheckPassed = viewModel.SelfCheckPassed;
            report.SelfCheckStatus = viewModel.StatusText;

            // #225 (GUI-C-159) row 16. Same shape as row 15 and awaited the same way; the runner takes
            // about four seconds (GUI-C-159 §2, measured with this app already up).
            ClickMenuItem(RunGuiE2EMenuItem);
            for (var waited = 0; waited < 120 && viewModel.GuiE2ERunning; waited++)
            {
                await Task.Delay(250);
            }

            report.GuiE2EPassed = viewModel.GuiE2EPassed;
            report.GuiE2EStatus = viewModel.StatusText;

            // #225 (GUI-C-176) row 17. Clicked through the menu like rows 15 and 16, but the launch itself is
            // suppressed under automation (see RunBenchmarkAsync), so this returns at once: with no build tree
            // it records the "not built" answer, with one it records that the launch was suppressed.
            ClickMenuItem(BenchmarkRunnerMenuItem);
            await Task.Delay(200);
            report.BenchmarkPassed = viewModel.BenchmarkPassed;
            report.BenchmarkStatus = viewModel.StatusText;
            report.BenchmarkLaunchSuppressed = viewModel.BenchmarkLaunchSuppressed;

            // #225 (GUI-C-160) row 5. The path and the line count are read back FROM DISK rather than
            // from what the command said it did: "wrote 42 lines" and "a file with 42 lines exists" are
            // different claims, and only the second one is the feature.
            ClickMenuItem(OpenRuntimeLogsMenuItem);
            await Task.Delay(200);
            report.RuntimeLogExportStatus = viewModel.StatusText;
            report.RuntimeLogExportPath = viewModel.LastRuntimeLogExportPath;
            report.RecentRawFileCount = viewModel.RecentRawFiles.Count;
            report.MostRecentRawFile = viewModel.RecentRawFiles.FirstOrDefault()?.Path;

            report.RuntimeLogExportLineCount =
                viewModel.LastRuntimeLogExportPath is { } logPath && File.Exists(logPath)
                    ? File.ReadAllLines(logPath).Length
                    : 0;

            // #225 (GUI-C-163) rows 3 and 14. AFTER the row-5 export on purpose: that is what creates
            // evidence/<RunId> in a run that recorded no verdict, so the bundle has something to zip
            // and the folder exists to resolve. Ordered the other way round, both commands would only
            // ever be observed answering "there is nothing yet".
            //
            // Row 3 was a placeholder the capability outran: the same command object the workbench
            // buttons bind is now on the menu item, and that identity is what is recorded — a copy of
            // the logic behind the menu could drift from the buttons without any test noticing.
            report.EvidenceBundleMenuSharesButtonCommand =
                ReferenceEquals(ExportEvidenceBundleMenuItem.Command, viewModel.ExportEvidenceBundleCommand);
            ClickMenuItem(ExportEvidenceBundleMenuItem);
            await Task.Delay(300);
            report.EvidenceBundleStatus = viewModel.StatusText;

            // Row 14: everything except the launch runs here — see OpenEvidenceFolder's remarks and
            // EvidenceFolderLaunchSuppressed.
            // #225 (GUI-C-169) row 20. Clicked through the menu, then read from the app's own state. The
            // scenario compares what the app CLAIMS against the disk independently (see A-11), so the
            // claim is checked rather than merely recorded.
            ClickMenuItem(OpenApiReferenceMenuItem);
            await Task.Delay(200);
            report.ApiReferenceStatus = viewModel.StatusText;
            report.ApiReferencePath = viewModel.LastApiReferencePath;
            report.ApiReferenceLaunchSuppressed = viewModel.ApiReferenceLaunchSuppressed;

            // #225 (GUI-C-181) row 21. Same shape as row 20: clicked through the menu, read from the app's own state,
            // and compared against the disk independently by the scenario (A-16 and A-17).
            ClickMenuItem(OpenTroubleshootingMenuItem);
            await Task.Delay(200);
            report.TroubleshootingStatus = viewModel.StatusText;
            report.TroubleshootingPagePath = viewModel.LastTroubleshootingPagePath;
            report.TroubleshootingLaunchSuppressed = viewModel.TroubleshootingLaunchSuppressed;

            // #225 rows 7 and 8 (GUI-C-170). Both panels are switched ON through their View toggles, then
            // read from what is RENDERED (the panel's own visibility and its text blocks), not from the
            // settings: the settings flag was already reported for months with no panel behind it, and
            // reading it again would assert nothing new. The Browse buttons are NOT pressed — they open a
            // modal folder dialog, which would hang an unattended run (same reason row 14 suppresses its
            // launch); their command binding is asserted by the runner instead.
            ToggleOn(ShowCalibrationPanelMenuItem);
            ToggleOn(ShowDisplaySettingsPanelMenuItem);
            await Task.Delay(250);
            var calibrationPanel = FindDescendant<Views.CalibrationPathsPanel>(this);
            var displayPanel = FindDescendant<Views.DisplaySettingsPanel>(this);
            report.CalibrationPanelRendered = calibrationPanel is { IsVisible: true, ActualWidth: > 0 };
            report.DisplayPanelRendered = displayPanel is { IsVisible: true, ActualWidth: > 0 };
            report.CalibrationPanelTexts = ReadPanelTexts(calibrationPanel);
            report.DisplayPanelTexts = ReadPanelTexts(displayPanel);

            // Persist WHILE they are on, so a following launch on the same settings file starts with them
            // on (A-13), then switch them off again before the verdict is computed. The second half is the
            // point of GUI-C-170's third falsification: the run must reach Passed=True with both panels
            // closed, which shows the verdict does not depend on the panels' state.
            ClickButton(SaveSettingsButton);
            await Task.Delay(200);
            ShowCalibrationPanelMenuItem.IsChecked = false;
            ShowDisplaySettingsPanelMenuItem.IsChecked = false;
            await Task.Delay(100);

            // #225 (GUI-C-168) row 13. The stage lines are read from the PANEL, through the same
            // converter the operator sees — not rebuilt from LastChain here. Reading the model would
            // assert that the data is right while leaving the one thing this row can get wrong (a
            // switched-off stage rendered as "0 ms") entirely unobserved.
            ClickMenuItem(OpenPipelineDiagnosticsMenuItem);
            await Task.Delay(200);
            report.PipelineDiagnosticsVisible = viewModel.ShowPipelineDiagnostics;
            report.PipelineDiagnosticsHasMeasurement = viewModel.HasPipelineDiagnostics;
            report.PipelineDiagnosticsStaleReason = viewModel.PreviewStaleReason;
            report.PipelineDiagnosticsStageLines = ReadDiagnosticsStageLines();


            ClickMenuItem(OpenEvidenceFolderMenuItem);
            await Task.Delay(200);
            report.EvidenceFolderStatus = viewModel.StatusText;
            report.EvidenceFolderPath = viewModel.LastEvidenceFolderPath;
            report.EvidenceFolderLaunchSuppressed = viewModel.EvidenceFolderLaunchSuppressed;


            // #225 row 9 (GUI-C-196 M4): the Deterministic Baseline, through the MENU a user has, LAST among the measurements: it takes seconds on the native
            // backend and sets the status line, the alerts and the log, none of which the steps above may see changed. NotRun (menu disabled, as on Mock) is
            // recorded as NotRun, never as a pass or a fail.
            report.BaselineMenuEnabled = RunDeterministicBaselineMenuItem.IsEnabled;
            var baselineNotAttempted = string.Empty;
            if (report.BaselineMenuEnabled && !report.PreprocessRan)
            {
                // Without a calibration set the preprocess stage cannot run, and the baseline would FAIL for that reason in every automation run that has none.
                // A failure that means "no calibration was given" is not the baseline's verdict: it is recorded as not attempted, with why.
                baselineNotAttempted = " (not attempted in this automation run: preprocessing did not run, so no calibration set was given)";
            }
            else if (report.BaselineMenuEnabled)
            {
                ClickMenuItem(RunDeterministicBaselineMenuItem);
                for (var waited = 0; waited < 600 && viewModel.LastBaselineResult is null; waited++)
                {
                    await Task.Delay(100);
                }
            }

            var baseline = viewModel.LastBaselineResult;
            report.BaselineStatusText = viewModel.BaselineStatusText + baselineNotAttempted;
            if (baseline is not null)
            {
                report.BaselineRan = true;
                report.BaselineStatus = baseline.Passed ? "Pass" : "Fail";
                report.BaselineBitIdentical = baseline.Verdict.BitIdentical;
                report.BaselineFirstDifference = baseline.Verdict.Difference is { Identical: false } d
                    ? $"pixel {d.FirstIndex}, {d.DifferentCount} differ, max {d.MaxAbsDifference}"
                    : string.Empty;
                report.BaselineOutputSha256 = baseline.Verdict.OutputSha256;
                report.BaselineStageTimes = baseline.StageTimes;
                report.BaselineTotalMs = Math.Round(baseline.TotalMs, 1);
                report.BaselineDicomValid = baseline.DicomValid;
                report.BaselineDicomRoundTripIdentical = baseline.DicomRoundTripIdentical;
                report.BaselineExposureIndex = baseline.ExposureIndex;
                report.BaselineEvidenceFolder = baseline.EvidenceFolder;
            }

            ClickMenuItem(ExportAutomationReportMenuItem);
            await Task.Delay(200);
            var menuCommandReportPath = Path.Combine(AppContext.BaseDirectory, "menu-command-report.json");
            report.MenuCommandReportCreated = File.Exists(menuCommandReportPath);
            report.ComparisonEvidenceExported =
                report.MenuCommandReportCreated &&
                File.ReadAllText(menuCommandReportPath).Contains("\"comparison\"", StringComparison.OrdinalIgnoreCase);
            report.CalibrationEvaluationEvidenceExported =
                report.MenuCommandReportCreated &&
                File.ReadAllText(menuCommandReportPath).Contains("\"calibrationEvaluation\"", StringComparison.OrdinalIgnoreCase);

            // #136: read the file THIS run wrote, which under automation is the isolated one.
            var settingsFile = _settingsFilePath;
            if (File.Exists(settingsFile))
            {
                using var document = JsonDocument.Parse(File.ReadAllText(settingsFile));
                if (document.RootElement.TryGetProperty("lastRawDir", out var lastRawDirElement))
                {
                    report.LastRawDirPersisted = string.Equals(
                        lastRawDirElement.GetString(),
                        report.LastRawDirectory,
                        StringComparison.OrdinalIgnoreCase);
                }
            }

            OpenHelpPage(HelpPageKind.QuickStart);
            await Task.Delay(350);

            if (OwnedWindows.OfType<HelpWindow>().FirstOrDefault() is { } helpWindow)
            {
                report.HelpWindowOpened = true;
                report.HelpWindowTitle = helpWindow.Title;
                report.HelpDocumentPath = helpWindow.CurrentDocumentPath;
                report.HelpDocumentLoaded = helpWindow.DocumentLoaded;
                helpWindow.Close();
            }

            // #201 (a) / GUI-C-136: press them ONE AT A TIME and record between. Pressing both and
            // recording once could not say which button did what — and that is exactly how the "Clear
            // Alerts does nothing observable" defect stayed invisible in this report while both counts
            // read 0. Clear Alerts goes first because it is the narrower of the two.
            ClickButton(ClearAlertsButton);
            await Task.Delay(200);

            report.LogCountAfterClearAlerts = viewModel.Logs.Count;
            report.AlertCountAfterClearAlerts = viewModel.Alerts.Count;

            ClickButton(ClearLogsButton);
            await Task.Delay(200);

            report.LogCountAfterClear = viewModel.Logs.Count;
            report.AlertCountAfterClear = viewModel.Alerts.Count;

            ClickButton(ShutdownBackendButton);
            // GUI-C-186e: the shutdown runs in the background now; wait for it to finish (bounded) before reading the state it leaves.
            for (var waited = 0; viewModel.IsBackendTransitioning && waited < 200; waited++)
            {
                await Task.Delay(50);
            }

            await Task.Delay(200);

            report.RuntimeStateAfterShutdown = viewModel.RuntimeInfo.State;
            report.Passed =
                report.BackendMatchesRequest &&
                !string.IsNullOrWhiteSpace(report.BackendVersion) &&
                report.InitialLogCount >= 5 &&
                report.InitialAlertCount >= 1 &&
                report.LogCountAfterLoad > report.InitialLogCount &&
                report.ActiveImageSummary.StartsWith("RAW ", StringComparison.Ordinal) &&
                report.LastRawDirPersisted &&
                report.DisplayPipelineApplied &&
                report.CalibrationEvaluationSummary.Contains("Offset=Off", StringComparison.Ordinal) &&
                report.CalibrationEvaluationSummary.Contains("Defect=On", StringComparison.Ordinal) &&
                report.CalibrationEvaluationEvidenceExported &&
                // GUI-C-170: "report.DisplayPanelVisible &&" stood here (since the panel flag was constantly
                // true). It is not a verdict term. Whether a panel is showing is the operator's layout
                // choice, and with a panel behind the flag it now varies run to run: keeping it would make
                // the app fail itself whenever the panel is closed — the same trap as the ">= 10" floor
                // GUI-C-169 replaced. The value is still reported (DisplayPanelVisible,
                // CalibrationPanelVisible, *PanelRendered), so the evidence is not lost, only the coupling.
                !string.IsNullOrWhiteSpace(report.DisplayVersion) &&
                report.ComparisonViewportDetected &&
                report.ComparisonSourcePreserved &&
                report.ComparisonEvidenceExported &&
                report.ComparisonZoomScale > 0.0 &&
                string.Equals(report.ComparisonMode, "SwipeVertical", StringComparison.Ordinal) &&
                Math.Abs(report.ComparisonSwipePosition - 0.5) < 0.001 &&
                report.VoiPresetApplied &&
                report.HelpWindowOpened &&
                report.HelpDocumentLoaded &&
                !string.IsNullOrWhiteSpace(report.HelpDocumentPath) &&
                report.TopLevelMenuCount >= 6 &&
                report.CanonicalMenuGroupsDetected &&
                report.PlannedMenuPlaceholdersDetected &&
                report.ToolbarMenuCommandParity &&
                // GUI-C-169: was "DisabledFutureCommandCount >= 10", a snapshot from the very first menu
                // commit (d5432d2, 2026-04-16) with no stated reason. A floor on a set that is MEANT to
                // shrink fails at the point the work lands: #225 row 13 stopped at exactly 10 and row 20 took
                // the count to 9, which made the app's own verdict fail. Replaced, not deleted: the two
                // counts are derived independently (a list of names vs a walk of the menu tree), so this
                // still catches a placeholder that is not counted or a counted item that is not a
                // placeholder — and it stays true at 0, when every row is built.
                report.DisabledFutureCommandCount == report.UnimplementedMenuLeafCount &&
                report.MenuCommandReportCreated &&
                report.LogCountAfterClear == 0 &&
                report.AlertCountAfterClear == 0 &&
                report.RuntimeStateAfterShutdown == "Shutdown";
        }
        catch (Exception ex)
        {
            report.Passed = false;
            report.Error = ex.ToString();
        }
        finally
        {
            if (!string.IsNullOrWhiteSpace(App.AutomationReportPath))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(App.AutomationReportPath)!);
                File.WriteAllText(
                    App.AutomationReportPath,
                    JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
            }

            // GUI-C-84: Shutdown(code) closes the windows itself. Closing the main window first had already
            // shut the app down with 0 (main-window-close shutdown), so the code passed afterwards was ignored.
            _forceClose = true; // GUI-C-186e: the self-run ends the process; a window close that waits in the background must not hold it up
            System.Windows.Application.Current.Shutdown(report.Passed ? 0 : App.AutomationFailedExitCode);
        }
    }

    private static void ClickButton(System.Windows.Controls.Primitives.ButtonBase button)
    {
        if (button.Command is not null && button.Command.CanExecute(button.CommandParameter))
        {
            button.Command.Execute(button.CommandParameter);
            return;
        }

        button.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent, button));
    }

    /// <summary>
    /// A checkable toggle has no command, and a raised Click does not flip IsChecked (only the menu's
    /// own input path does), so this sets the state a click would and lets the two-way binding write it
    /// through to Settings.
    /// </summary>
    private static void ToggleOn(System.Windows.Controls.MenuItem menuItem)
    {
        menuItem.IsChecked = true;
    }

    /// <summary>The text a panel actually renders, as the operator would read it.</summary>
    private static string[]? ReadPanelTexts(DependencyObject? panel)
    {
        if (panel is null) return null;
        var texts = new List<string>();
        CollectVisibleText(panel, texts);
        return texts.ToArray();
    }

    /// <summary>Only what is on screen: a collapsed stand-in ("(not set)" while a path is set) is not read.</summary>
    private static void CollectVisibleText(DependencyObject root, List<string> into)
    {
        if (root is UIElement { IsVisible: false }) return;
        if (root is TextBlock text && !string.IsNullOrWhiteSpace(text.Text)) into.Add(text.Text.Trim());

        for (var i = 0; i < System.Windows.Media.VisualTreeHelper.GetChildrenCount(root); i++)
        {
            CollectVisibleText(System.Windows.Media.VisualTreeHelper.GetChild(root, i), into);
        }
    }

    private static void ClickMenuItem(System.Windows.Controls.MenuItem menuItem)
    {
        if (menuItem.Command is not null && menuItem.Command.CanExecute(menuItem.CommandParameter))
        {
            menuItem.Command.Execute(menuItem.CommandParameter);
            return;
        }

        menuItem.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.MenuItem.ClickEvent, menuItem));
    }

    private void ExitMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private void OpenHelpIndexMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.Index);
    }

    private void OpenQuickStartHelpMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.QuickStart);
    }

    private void OpenScopeHelpMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.Scope);
    }

    private void OpenCurrentWorkflowHelpMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        OpenHelpPage(HelpPageKind.QuickStart);
    }

    private void AboutBuildInfoMenuItem_OnClick(object sender, RoutedEventArgs e)
    {
        System.Windows.MessageBox.Show(
            $"ImageProcTest GUI-S0{Environment.NewLine}Backend: {(DataContext as MainWindowViewModel)?.RuntimeInfo.Version ?? "unknown"}{Environment.NewLine}Help bundle: packaged offline HTML",
            "About ImageProcTest",
            System.Windows.MessageBoxButton.OK,
            System.Windows.MessageBoxImage.Information);
    }

    private void OpenHelpPage(HelpPageKind pageKind)
    {
        var pagePath = _helpBundleService.GetHelpPagePath(pageKind);
        var helpTitle = pageKind switch
        {
            HelpPageKind.Index => "ImageProcTest Help",
            HelpPageKind.QuickStart => "ImageProcTest Help - Quick Start",
            HelpPageKind.Scope => "ImageProcTest Help - Scope and Limitations",
            _ => "ImageProcTest Help"
        };

        if (!_helpBundleService.HelpPageExists(pageKind))
        {
            System.Windows.MessageBox.Show(
                $"Help page not found: {pagePath}",
                "ImageProcTest Help",
                System.Windows.MessageBoxButton.OK,
                System.Windows.MessageBoxImage.Warning);
            return;
        }

        var helpWindow = new HelpWindow(helpTitle, pagePath)
        {
            Owner = this
        };
        helpWindow.Show();
        helpWindow.Activate();
    }

    /// <summary>
    /// #225 row 13 (GUI-C-168): each stage row of the diagnostics panel, as the panel renders it.
    ///
    /// <para>Walks the rendered <c>ItemsControl</c> rather than the model so the CONVERTER is in the
    /// measurement. The defect this row risks lives in the rendering step: <c>ElapsedMs</c> is 0 for a
    /// stage that was switched off, and a panel that prints that number says "instantaneous". Reading
    /// the model would confirm the data and miss exactly that.</para>
    /// </summary>
    private string[]? ReadDiagnosticsStageLines()
    {
        // Find the PANEL first. FindDescendant<ItemsControl>(this) would return the window's first
        // ItemsControl, which is a menu — it would have read the wrong control and reported its text as
        // the diagnostics rows, silently.
        var panel = FindDescendant<Views.PipelineDiagnosticsPanel>(this);
        if (panel is null) return null;

        var items = FindDescendant<ItemsControl>(panel);
        if (items?.Items is null) return null;

        var lines = new List<string>();
        for (var i = 0; i < items.Items.Count; i++)
        {
            if (items.ItemContainerGenerator.ContainerFromIndex(i) is not DependencyObject container) continue;
            var texts = new List<string>();
            CollectTextBlocks(container, texts);
            if (texts.Count > 0) lines.Add(string.Join(" ", texts).Trim());
        }

        return lines.ToArray();
    }

    private static void CollectTextBlocks(DependencyObject root, List<string> into)
    {
        if (root is TextBlock text && !string.IsNullOrWhiteSpace(text.Text)) into.Add(text.Text.Trim());

        for (var i = 0; i < System.Windows.Media.VisualTreeHelper.GetChildrenCount(root); i++)
        {
            CollectTextBlocks(System.Windows.Media.VisualTreeHelper.GetChild(root, i), into);
        }
    }

    /// <summary>
    /// #225 (GUI-C-169): counts greyed leaf menu items that have no command — the structural definition of
    /// "not implemented yet".
    ///
    /// <para><b>Items with an <c>ItemsSource</c> are skipped on purpose.</b> "Open Recent" is a data-driven
    /// submenu: with an empty history it is greyed and has no children, which is exactly the shape of a
    /// placeholder while meaning "nothing to list yet". Counting it would make this number depend on
    /// whether a file had been opened — the same trap GUI-C-165 found in the runner's Open Recent line,
    /// which measured "the list is empty" instead of "the feature is missing".</para>
    ///
    /// <para>A greyed item WITH a command is not counted: that is a state-disabled command, not an
    /// unbuilt one.</para>
    /// </summary>
    private static int CountUnimplementedMenuLeaves(System.Windows.Controls.ItemCollection items)
    {
        var count = 0;
        foreach (var item in items)
        {
            if (item is not System.Windows.Controls.MenuItem menuItem) continue;

            if (menuItem.Items.Count > 0)
            {
                count += CountUnimplementedMenuLeaves(menuItem.Items);
            }
            else if (menuItem.ItemsSource is null && !menuItem.IsEnabled && menuItem.Command is null)
            {
                count++;
            }
        }

        return count;
    }

    private static T? FindDescendant<T>(System.Windows.DependencyObject root) where T : System.Windows.DependencyObject
    {
        if (root is T match)
        {
            return match;
        }

        for (var i = 0; i < System.Windows.Media.VisualTreeHelper.GetChildrenCount(root); i++)
        {
            if (FindDescendant<T>(System.Windows.Media.VisualTreeHelper.GetChild(root, i)) is { } found)
            {
                return found;
            }
        }

        return null;
    }
}
