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

    public bool DisplayPanelVisible { get; set; }

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

    public bool MenuCommandReportCreated { get; set; }

    public string? Error { get; set; }
}
