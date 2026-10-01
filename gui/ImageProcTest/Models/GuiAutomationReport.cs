namespace ImageProcTest.Models;

public sealed class GuiAutomationReport
{
    public bool Passed { get; set; }

    public string BackendVersion { get; set; } = string.Empty;

    /// <summary>#136: the backend mode this run used.</summary>
    public string BackendMode { get; set; } = string.Empty;

    /// <summary>#136: where that mode came from — "arg" (--automation-backend) or "file" (appsettings.json).</summary>
    public string BackendModeSource { get; set; } = string.Empty;

    /// <summary>#175 HAZ-GUI-005 (3): the backend that actually ran, which can differ from <see cref="BackendMode"/>.</summary>
    public string ActualBackendMode { get; set; } = string.Empty;

    public bool MockBackend { get; set; }

    /// <summary>
    /// #175 (GUI-C-83): the requested and the actual backend agree. <see cref="Passed"/> requires it, so a
    /// Native run that silently fell back to Mock is a failure rather than a green Mock run.
    /// </summary>
    public bool BackendMatchesRequest { get; set; }

    /// <summary>#129: directory the native library resolved from, or "loader"/empty. See RuntimeInfo.NativeSource.</summary>
    public string NativeSource { get; set; } = string.Empty;

    /// <summary>#141: true when a preprocess run completed every stage during this automation run.</summary>
    public bool PreprocessRan { get; set; }

    /// <summary>#141: the summary line of that attempt — the success stages, or the refusal reason.</summary>
    public string PreprocessStages { get; set; } = string.Empty;

    /// <summary>#180 (GUI-C-99): the pixel chain of the image on screen — each stage and its status.</summary>
    public string ChainStatus { get; set; } = string.Empty;

    /// <summary>#180 (GUI-C-99): per stage, <c>id=Status</c>, in chain order.</summary>
    public List<string> ChainStages { get; set; } = new();

    public int InitialLogCount { get; set; }

    public int InitialAlertCount { get; set; }

    public int LogCountAfterLoad { get; set; }

    public int AlertCountAfterLoad { get; set; }

    /// <summary>
    /// After <c>Clear Alerts</c> ALONE (#201 (a), GUI-C-136). The pair below is after Clear Logs as
    /// well; recording only that pair could not say which button changed what, which is how "Clear
    /// Alerts does nothing observable" stayed invisible in this report while both counts read 0.
    /// </summary>
    public int LogCountAfterClearAlerts { get; set; }

    /// <inheritdoc cref="LogCountAfterClearAlerts"/>
    public int AlertCountAfterClearAlerts { get; set; }

    public int LogCountAfterClear { get; set; }

    public int AlertCountAfterClear { get; set; }

    public string ActiveImageSummary { get; set; } = string.Empty;

    public string StatusAfterLoad { get; set; } = string.Empty;

    public string RuntimeStateAfterShutdown { get; set; } = string.Empty;

    public string LastRawDirectory { get; set; } = string.Empty;

    public bool LastRawDirPersisted { get; set; }

    public bool HelpWindowOpened { get; set; }

    public bool HelpDocumentLoaded { get; set; }

    public string HelpWindowTitle { get; set; } = string.Empty;

    public string HelpDocumentPath { get; set; } = string.Empty;

    public int TopLevelMenuCount { get; set; }

    public bool CanonicalMenuGroupsDetected { get; set; }

    public bool PlannedMenuPlaceholdersDetected { get; set; }

    public bool ToolbarMenuCommandParity { get; set; }

    // ResizableDiagnosticsLayoutDetected was removed here (#165, GUI-C-77). It was assigned the
    // constant true and never measured anything; measured for real it reads false, because the two
    // GridSplitters it described (see docs/design/reference/MainWindow.xaml) left with the Evaluation
    // Workbench layout. No requirement names that layout, so there is nothing left to measure.

    public bool DisplayPipelineApplied { get; set; }

    public string DisplayPipelineSummary { get; set; } = string.Empty;

    public string CalibrationEvaluationSummary { get; set; } = string.Empty;

    public string OffsetCorrectionMode { get; set; } = string.Empty;

    public string DefectCorrectionMode { get; set; } = string.Empty;

    public bool CalibrationEvaluationEvidenceExported { get; set; }

    /// <summary>The persisted Settings.ShowDisplayPanel as the run started. Reported, not part of the verdict.</summary>
    public bool DisplayPanelVisible { get; set; }

    /// <summary>#225 row 7 (GUI-C-170): the persisted Settings.ShowCalibrationPanel as the run started.</summary>
    public bool CalibrationPanelVisible { get; set; }

    /// <summary>#225 rows 7/8: the panel was found in the visual tree, visible and laid out, after its toggle was switched on.</summary>
    public bool CalibrationPanelRendered { get; set; }

    public bool DisplayPanelRendered { get; set; }

    /// <summary>The panels were on screen when the run STARTED (before the run switched them on) — i.e. because the persisted flag said so.</summary>
    public bool CalibrationPanelRenderedAtStart { get; set; }

    public bool DisplayPanelRenderedAtStart { get; set; }

    /// <summary>The text blocks of the rendered panel, in tree order — what the operator would read.</summary>
    public string[]? CalibrationPanelTexts { get; set; }

    public string[]? DisplayPanelTexts { get; set; }

    public string DisplayVersion { get; set; } = string.Empty;

    public bool VoiPresetApplied { get; set; }

    /// <summary>#135: the center the active backend's preset actually produced (Mock 32768 / native 40).</summary>
    public float VoiPresetCenter { get; set; }

    /// <summary>#135: the width the active backend's preset actually produced (Mock 65535 / native 400).</summary>
    public float VoiPresetWidth { get; set; }

    public bool ComparisonViewportDetected { get; set; }

    public string ComparisonMode { get; set; } = string.Empty;

    public double ComparisonZoomScale { get; set; }

    public double ComparisonSwipePosition { get; set; }

    public bool ComparisonSourcePreserved { get; set; }

    public bool ComparisonEvidenceExported { get; set; }

    public int DisabledFutureCommandCount { get; set; }

    /// <summary>
    /// #225 (GUI-C-169): the same quantity as <see cref="DisabledFutureCommandCount"/>, derived by a second,
    /// independent route — walking the live menu tree for greyed LEAF items that carry no command. The
    /// first route is a hand-written list of names; this one is structural. They can only agree if the
    /// list neither misses a placeholder nor lists something that is not one.
    /// </summary>
    public int UnimplementedMenuLeafCount { get; set; }

    /// <summary>
    /// Outcome of the P/Invoke smoke command (#225, GUI-C-154 row 6); null when it was not run.
    ///
    /// <para>Recorded rather than derived: the run that proves the smoke can FAIL — the app launched
    /// with an empty <c>XPE_NATIVE_DIR</c> and <c>XPE_NATIVE_DIR_EXCLUSIVE=1</c> — needs a machine
    /// readable answer, and re-deriving it from the log text would test the log format instead.</para>
    /// </summary>
    public bool? PInvokeSmokeTestPassed { get; set; }

    /// <summary>Per-probe outcomes of that run, so a failure says which probe failed and why.</summary>
    public string? PInvokeSmokeTestDetail { get; set; }

    /// <summary>What Stage Timing reported (#225, GUI-C-154 row 12); null when it was not run.</summary>
    public string? StageTimingReport { get; set; }

    /// <summary>Status line after pressing Stop with nothing running (#225 row 11) — the defined no-op.</summary>
    public string? StopWithNothingRunningStatus { get; set; }

    /// <summary>Status line after stopping a render that WAS in flight (#225 row 11).</summary>
    public string? StopInFlightStatus { get; set; }

    /// <summary>How many renders the stop command discarded during this run (#225 row 11).</summary>
    public int StoppedRenderCount { get; set; }

    /// <summary>Verdict of the Run Self-Check command (#225 row 15); null when it was not run.</summary>
    public bool? SelfCheckPassed { get; set; }

    /// <summary>The status line it produced, so a failure says why rather than only that.</summary>
    public string? SelfCheckStatus { get; set; }

    /// <summary>#225 row 16 (GUI-C-159): verdict of the GUI E2E runner the Tools menu launched.</summary>
    public bool? GuiE2EPassed { get; set; }

    /// <summary>#225 row 16: the status line the app showed after that run.</summary>
    public string? GuiE2EStatus { get; set; }

    /// <summary>#225 row 5 (GUI-C-160): the status line after the runtime-log export.</summary>
    public string? RuntimeLogExportStatus { get; set; }

    /// <summary>#225 row 5: the file the app says it wrote, so a test can open it.</summary>
    public string? RuntimeLogExportPath { get; set; }

    /// <summary>#225 row 5: lines actually present in that file, read back by the app after writing.</summary>
    public int RuntimeLogExportLineCount { get; set; }

    /// <summary>
    /// #225 row 1 (GUI-C-160): entries the recent-file history already held when this process started,
    /// BEFORE this run loaded anything. Non-zero only when a previous run wrote to the same settings
    /// file — which is the persistence the row was missing.
    /// </summary>
    public int RecentRawFileCountAtStartup { get; set; }

    /// <summary>#225 row 1: entries after this run's load.</summary>
    public int RecentRawFileCount { get; set; }

    /// <summary>#225 row 1: the newest entry after this run's load.</summary>
    public string? MostRecentRawFile { get; set; }

    /// <summary>
    /// #225 row 1 (GUI-C-160): whether the settings file on disk already carried the newest history
    /// entry BEFORE the scenario pressed Save. This is what attributes the persistence to the app
    /// rather than to the operator's save.
    /// </summary>
    public bool RecentHistoryPersistedBeforeSave { get; set; }

    /// <summary>#225 row 14 (GUI-C-163): the status line after Open Evidence Folder.</summary>
    public string? EvidenceFolderStatus { get; set; }

    /// <summary>#225 row 14: the directory the command resolved, or null when it declined.</summary>
    public string? EvidenceFolderPath { get; set; }

    /// <summary>
    /// #225 row 14: true when the file-browser launch was skipped because this is an automation run.
    /// Recorded in the DATA rather than only in prose, so a reader of the report can see that the
    /// launch itself is the one step no scenario observes.
    /// </summary>
    public bool EvidenceFolderLaunchSuppressed { get; set; }

    /// <summary>#225 row 3 (GUI-C-163): the status line after Export Evidence Bundle.</summary>
    public string? EvidenceBundleStatus { get; set; }

    /// <summary>#225 row 3: whether the menu item shares the command object with the workbench button.</summary>
    public bool EvidenceBundleMenuSharesButtonCommand { get; set; }

    /// <summary>#225 row 13 (GUI-C-168): whether the diagnostics panel was showing after the menu command.</summary>
    public bool PipelineDiagnosticsVisible { get; set; }

    /// <summary>#225 row 20 (GUI-C-169): the status line after Tools -> API Reference.</summary>
    public string? ApiReferenceStatus { get; set; }

    /// <summary>#225 row 20: the page the command resolved, or null when it had nothing to open.</summary>
    public string? ApiReferencePath { get; set; }

    /// <summary>#225 row 20: true when the browser launch was skipped because this is an automation run.</summary>
    public bool ApiReferenceLaunchSuppressed { get; set; }

    /// <summary>
    /// #225 row 13: what the panel showed for each stage, as the panel renders it — the status and the
    /// TIME AS DISPLAYED. A stage that was switched off must appear here with no number, which is the
    /// one thing this row can get wrong (a 0 read as "instantaneous").
    /// </summary>
    public string[]? PipelineDiagnosticsStageLines { get; set; }

    /// <summary>#225 row 13: false before any chain has run — the "no measurement" state.</summary>
    public bool PipelineDiagnosticsHasMeasurement { get; set; }

    /// <summary>#225 row 13: the app's own staleness reason while the panel was open, or null.</summary>
    public string? PipelineDiagnosticsStaleReason { get; set; }

    /// <summary>
    /// #225 row 13: whether a measurement existed at STARTUP, before this run rendered anything. False is
    /// the "never rendered" state the panel must not print as 0 ms.
    /// </summary>
    public bool PipelineDiagnosticsHadMeasurementAtStartup { get; set; }

    /// <summary>
    /// #225 row 13 (GUI-C-168): the staleness reason immediately after the first successful render —
    /// expected null. Recorded so the pair below is attributable: without a "before", a non-null "after"
    /// could be left over from something else entirely, which is what a first attempt measured.
    /// </summary>
    public string? StaleReasonBeforeParameterEdit { get; set; }

    /// <summary>#225 row 13: after changing a display parameter with NO re-render. This is the line that
    /// tells the operator the panel's numbers predate their edit.</summary>
    public string? StaleReasonAfterParameterEdit { get; set; }

    /// <summary>#225 row 13: after putting the parameter back — the reason must clear, or "stale" would
    /// just be a label that never comes off.</summary>
    public string? StaleReasonAfterParameterRestored { get; set; }

    public bool MenuCommandReportCreated { get; set; }

    public string? Error { get; set; }
}
