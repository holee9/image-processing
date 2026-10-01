using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Text.Json;
using System.Windows.Controls;
using System.Windows.Data;
using ImageProcTest.Controls;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.Services.Native;
using DataBinding = System.Windows.Data.Binding;
using Win32OpenFileDialog = Microsoft.Win32.OpenFileDialog;
using FormsDialogResult = System.Windows.Forms.DialogResult;
using FormsFolderBrowserDialog = System.Windows.Forms.FolderBrowserDialog;

namespace ImageProcTest.ViewModels;

public sealed class MainWindowViewModel : ObservableObject
{
    private readonly AppSettingsService _settingsService;
    private readonly Func<AppSettings, IXpeBackend> _backendFactory;
    private IXpeBackend _backend;
    private string _statusText = "Ready";
    private string _activeImageSummary = "No raw image loaded.";
    private string _metadataText = "GUI-S0 accepts raw binary frames only. Real DICOM remains owned by xpe_dicom.dll in Phase 1b.";
    private string _displayPipelineSummary = "Display pipeline has not run.";
    private System.Windows.Media.ImageSource? _sourceImage;
    private System.Windows.Media.ImageSource? _processedImage;
    private float? _renderedVoiCenter;
    private float? _renderedVoiWidth;
    private string? _renderedVoiMode;
    private AppSettings? _renderedInputs;
    private string? _previewStaleReason;
    private BackendRuntimeInfo _runtimeInfo = new();
    private LoadedImageFrame? _activeImageFrame;
    private int _drainedBackendLogCount;

    /// <summary>
    /// The exact log lines <see cref="RaiseAlert"/> wrote, so <c>Clear Alerts</c> can remove those and
    /// nothing else (#201 (a), GUI-C-136). A set of the inserted strings rather than a text pattern: the
    /// timestamp makes each line unique in practice, and an ordinary message that happens to contain
    /// "ALERT " is not caught by accident.
    /// </summary>
    private readonly HashSet<string> _alertLogLines = new(StringComparer.Ordinal);
    private int _drainedBackendAlertCount;
    /// <summary>#206 (GUI-C-140): off by default — the filter is a reader's tool, not a new default view.</summary>
    private bool _showAlertsOnly;
    private bool _showRuntimePanel = true;
    private bool _showRawSettingsPanel = true;
    private bool _showImageSummaryPanel = true;
    private bool _showMetadataPanel = true;
    // OFF at start, per MENU-001 §9.2 (#165). The other panel flags keep their old value because
    // nothing reads them: only this one is wired to a region.
    private bool _showLogsPanel = false;
    private string? _selectedLog;
    private bool _showAlertsPanel = true;

    // Slice 2 — workbench VM-only backing fields
    private System.Windows.Media.ImageSource? _laneAImage;
    private System.Windows.Media.ImageSource? _laneBImage;
    private string _activeStudyId = string.Empty;
    private RunSetState _runSet = new();
    private Verdict? _activeVerdict;
    private string _verdictNotes = string.Empty;
    private bool _roiActive;
    private bool _histogramActive;
    private bool? _pInvokeSmokeTestPassed;
    private string? _pInvokeSmokeTestDetail;
    private CancellationTokenSource? _renderCancellation;
    private int _stoppedRenderCount;
    private string? _lastStageTimingReport;
    private bool _selfCheckRunning;
    private bool? _selfCheckPassed;
    private bool _guiE2ERunning;
    private bool? _guiE2EPassed;
    private string? _lastRuntimeLogExportPath;
    private string? _lastEvidenceFolderPath;
    private bool _evidenceFolderLaunchSuppressed;
    private bool _showPipelineDiagnostics;
    private string? _lastApiReferencePath;
    private bool _apiReferenceLaunchSuppressed;
    private string? _lastTroubleshootingPagePath;
    private bool _troubleshootingLaunchSuppressed;
    private bool _benchmarkRunning;
    private bool? _benchmarkPassed;
    private bool _benchmarkLaunchSuppressed;
    private readonly object _telemetryLock = new();

    /// <summary>
    /// Hard-coded algorithm names compiled into this build.
    /// When a new algorithm is added to the project, add its name here and rebuild.
    /// There is no runtime discovery.
    /// </summary>
    /// <summary>
    /// The picker's options, taken from the presets themselves (#173, GUI-C-114) — a list written here
    /// as well would be a second place for an option to exist, and the one the picker shows would not
    /// have to be one the chain knows how to run.
    /// </summary>
    public static readonly string[] AlgorithmOptions =
        AlgorithmPreset.All.Select(p => p.Name).ToArray();

    /// <param name="preservedSettingsPath">
    /// Where the unreadable settings file was moved, when the stored settings could not be read
    /// (#173, GUI-C-119). Null on an ordinary start. The three things the message has to carry are
    /// measured, not chosen: GUI-C-118 found that a corrupt file loses EVERY stored setting silently,
    /// so saying only "could not read" leaves the user unaware their settings are gone, and saying it
    /// without the path makes moving the original aside pointless.
    /// </param>
    public MainWindowViewModel(
        AppSettings settings,
        AppSettingsService settingsService,
        Func<AppSettings, IXpeBackend> backendFactory,
        string? preservedSettingsPath = null,
        bool preservedSettingsIsFromAnEarlierFailure = false)
    {
        Settings = settings;
        _settingsService = settingsService;
        _backendFactory = backendFactory;
        _backend = _backendFactory(settings);

        Logs = new ObservableCollection<string>();
        // #206 (GUI-C-140): the view the list binds to. Built here rather than lazily so the filter
        // predicate exists before the first line is written — a log line inserted before the view was
        // created would be in Logs but not in the view until a Refresh nobody calls.
        LogsView = new CollectionViewSource { Source = Logs }.View;
        LogsView.Filter = item => !ShowAlertsOnly || (item is string line && _alertLogLines.Contains(line));
        Alerts = new ObservableCollection<AlertEntry>();
        BackendModeOptions = new[] { "Mock", "Native" };
        CalibrationStageModeOptions = CalibrationStageMode.Options;
        VoiLutModeOptions = new[] { "Linear", "LinearExact", "Sigmoid" };
        BodyPartOptions = Enum.GetNames<XpeBodyPartEnum>();
        // #161: one list, in Models. This array used to be a third copy of the mode vocabulary.
        CompareModeOptions = ComparisonModes.All;
        Settings.PropertyChanged += OnSettingsPropertyChanged;

        InitializeBackendCommand = new RelayCommand(InitializeBackend);
        SetBackendModeCommand = new RelayCommand<string>(SetBackendMode);
        RunPInvokeSmokeTestCommand = new RelayCommand(RunPInvokeSmokeTest);
        StopProcessingCommand = new RelayCommand(StopProcessing);
        ShowStageTimingCommand = new RelayCommand(ShowStageTiming);
        RunSelfCheckCommand = new RelayCommand(() => _ = RunSelfCheckAsync());
        RunGuiE2ECommand = new RelayCommand(() => _ = RunGuiE2EAsync());
        RunBenchmarkCommand = new RelayCommand(() => _ = RunBenchmarkAsync());
        LoadRecentRawFileCommand = new RelayCommand<string>(path => _ = LoadRecentRawFileAsync(path));
        // Seeded from the persisted settings: the history exists before this process does (#225 row 1).
        RefreshRecentRawFiles();
        ExportRuntimeLogsCommand = new RelayCommand(ExportRuntimeLogs);
        OpenEvidenceFolderCommand = new RelayCommand(OpenEvidenceFolder);
        OpenApiReferenceCommand = new RelayCommand(OpenApiReference);
        OpenTroubleshootingCommand = new RelayCommand(OpenTroubleshooting);
        OpenPipelineDiagnosticsCommand = new RelayCommand(() => ShowPipelineDiagnostics = true);
        ClosePipelineDiagnosticsCommand = new RelayCommand(() => ShowPipelineDiagnostics = false);
        ShutdownBackendCommand = new RelayCommand(() => BeginShutdown());
        LoadImageCommand = new RelayCommand(LoadImage);
        ApplyDisplayPipelineCommand = new RelayCommand(() => _ = ApplyDisplayPipelineAsync());
        ApplyBodyPartPresetCommand = new RelayCommand(ApplyBodyPartPreset);
        RunPreprocessingCommand = new RelayCommand(RunPreprocessing);
        RunAiBoneSuppressionCommand = new RelayCommand(RunAiBoneSuppression);
        RestartAiSessionCommand = new RelayCommand(RestartAiSession);
        ZoomFitCommand = new RelayCommand(ZoomFit);
        ZoomActualCommand = new RelayCommand(ZoomActual);
        ZoomInCommand = new RelayCommand(ZoomIn);
        ZoomOutCommand = new RelayCommand(ZoomOut);
        ResetComparisonViewCommand = new RelayCommand(ResetComparisonView);
        SetComparisonModeCommand = new RelayCommand<string>(SetComparisonMode);
        DetachComparisonViewerCommand = new RelayCommand(OpenDetachedComparisonViewer);
        SaveSettingsCommand = new RelayCommand(SaveSettings);
        BrowseOffsetCalibrationDirectoryCommand = new RelayCommand(() => BrowseCalibrationDirectory(CalibrationPathKind.Offset));
        BrowseGainCalibrationDirectoryCommand = new RelayCommand(() => BrowseCalibrationDirectory(CalibrationPathKind.Gain));
        BrowseDefectCalibrationDirectoryCommand = new RelayCommand(() => BrowseCalibrationDirectory(CalibrationPathKind.Defect));
        ClearLogsCommand = new RelayCommand(ClearLogs);
        // Disabled while nothing is selected (#173, GUI-C-123) — see the command's own note.
        CopySelectedLogCommand = new RelayCommand(CopySelectedLog, () => !string.IsNullOrEmpty(SelectedLog));
        ClearAlertsCommand = new RelayCommand(ClearAlerts);
        ResetLayoutCommand = new RelayCommand(ResetLayout);
        ShowNativeDiagnosticsCommand = new RelayCommand(ShowNativeDiagnostics);
        ShowCalibrationSettingsCommand = new RelayCommand(ShowCalibrationSettings);
        ShowFixtureManagerCommand = new RelayCommand(ShowFixtureManager);
        ExportAutomationReportCommand = new RelayCommand(ExportAutomationReport);

        // Slice 2 — workbench commands
        Studies = new ObservableCollection<StudyEntry>();
        RunOnAllQueuedCommand = new RelayCommand(() => Log("RunOnAllQueuedCommand: not implemented (Slice 7)."));
        RecordVerdictCommand = new RelayCommand<Verdict>(RecordVerdict);
        SaveAndNextCommand = new RelayCommand(SaveAndNext);
        ToggleFocusModeCommand = new RelayCommand(() => FocusMode = !FocusMode);
        ToggleRoiCommand = new RelayCommand(() => RoiActive = !RoiActive);
        ExportEvidenceBundleCommand = new RelayCommand(ExportEvidenceBundle);
        SwitchAnalysisTabCommand = new RelayCommand<string>(tab => { if (!string.IsNullOrWhiteSpace(tab)) AnalysisTab = tab; });
        ResetLaneBOverridesCommand = new RelayCommand(ResetLaneBOverrides);

        Log("GUI-S0 initialized.");
        InitializeBackend();

        // AFTER the backend, not before (#161, GUI-C-63). InitializeBackend used to clear Logs and
        // Alerts, so anything said before it was written and erased within the same constructor —
        // measured in GUI-C-62, where a run with a rejected mode showed six log lines, none of them the
        // rejection and none of them the "GUI-S0 initialized." line written immediately before it.
        // GUI-C-126 replaced that clear with a separator, so the erasure is gone; the order stays
        // because it is also the order these things happen in.
        ReportRejectedComparisonMode();

        // Same ordering reason as the line above (and it no longer depends on the clear, which
        // GUI-C-126 replaced with a separator).
        ReportUnreadableSettings(preservedSettingsPath, preservedSettingsIsFromAnEarlierFailure);
    }

    /// <summary>
    /// Says, once, that the stored settings could not be read (#173, GUI-C-119).
    ///
    /// <para>Three things, all load-bearing: the file could not be read, <b>the app started from
    /// defaults</b>, and <b>where the original is</b>. GUI-C-118 measured that a corrupt file loses
    /// every stored setting with nothing on screen — so a message missing the second part leaves the
    /// user thinking nothing was lost, and one missing the third makes preserving the file pointless.</para>
    /// </summary>
    private void ReportUnreadableSettings(string? preservedPath, bool fromAnEarlierFailure)
    {
        if (string.IsNullOrWhiteSpace(preservedPath)) return;

        // The second sentence differs because the fact differs: on a repeat failure nothing was moved
        // now, and the path names what an EARLIER failure rescued (#173, GUI-C-120). Saying "the
        // original was kept" there would be false — this run kept nothing — and the user would be
        // looking for a file holding what they had, which this one does not.
        var message = fromAnEarlierFailure
            ? "Your saved settings could not be read, so this session started from defaults. " +
              $"An earlier failure already rescued your original settings, kept at '{preservedPath}'."
            : "Your saved settings could not be read, so this session started from defaults. " +
              $"The original file was kept at '{preservedPath}'.";

        RaiseAlert(new AlertEntry
        {
            Severity = "WARN",
            Code = "SETTINGS_UNREADABLE",
            Message = message,
            Timestamp = DateTimeOffset.Now
        });

        // RaiseAlert already wrote the line; saying it twice would put the same sentence in the log
        // twice, once with the ALERT marker and once without (#198 ①, GUI-C-125).
        StatusText = message;
    }

    public AppSettings Settings { get; }

    public string[] BackendModeOptions { get; }

    public string[] CalibrationStageModeOptions { get; }

    public string[] VoiLutModeOptions { get; }

    /// <summary>
    /// The body part shown in the toolbar selector. Setting it applies the backend's preset —
    /// GUI-C-34 measured that no control invoked ApplyBodyPartPresetCommand at all, so the preset
    /// was reachable only from the automation harness.
    /// </summary>
    public string SelectedBodyPart
    {
        get => Settings.SelectedBodyPart;
        set
        {
            if (string.Equals(Settings.SelectedBodyPart, value, StringComparison.Ordinal))
            {
                return;
            }

            Settings.SelectedBodyPart = value;
            OnPropertyChanged();
            ApplyBodyPartPresetCommand.Execute(null);
        }
    }

    public string[] BodyPartOptions { get; }

    public string[] CompareModeOptions { get; }

    public ObservableCollection<string> Logs { get; }

    /// <summary>
    /// What the log list actually shows (#206, GUI-C-140). A view over <see cref="Logs"/> rather than a
    /// second collection: a copy would have to be kept in step with every insert and removal, and
    /// <c>Clear Alerts</c> already removes lines (#201 (a), GUI-C-136) — two collections drifting apart
    /// is the failure that would produce.
    ///
    /// <para>The filter tests membership of <see cref="_alertLogLines"/> — the exact lines
    /// <see cref="RaiseAlert"/> wrote — NOT a text match on "ALERT ". GUI-C-136 chose that set for the
    /// same reason: an ordinary message containing the word would be caught by a text rule, and a
    /// filter that hides ordinary lines while claiming to show alerts is worse than no filter.</para>
    /// </summary>
    public ICollectionView LogsView { get; }

    /// <summary>
    /// Show only the alert lines (#206, GUI-C-140). GUI-C-139 measured why this exists: the depth a
    /// reader must cover to be sure they have seen every alert grows by 8 lines per chain run on the
    /// native path and leaves the visible page after 3-4 runs, while the alert SHARE moves in the
    /// opposite direction and so cannot report the cost.
    /// </summary>
    public bool ShowAlertsOnly
    {
        get => _showAlertsOnly;
        set
        {
            if (!SetProperty(ref _showAlertsOnly, value)) return;
            LogsView.Refresh();
        }
    }

    public ObservableCollection<AlertEntry> Alerts { get; }

    /// <summary>The log line the user picked, for <see cref="CopySelectedLogCommand"/>.</summary>
    public string? SelectedLog
    {
        get => _selectedLog;
        set
        {
            if (!SetProperty(ref _selectedLog, value)) return;
            CopySelectedLogCommand.RaiseCanExecuteChanged();
        }
    }

    /// <summary>
    /// Puts the selected log line on the clipboard (#173, GUI-C-122).
    ///
    /// <para>The startup warning about unreadable settings names a file path the user has to go and
    /// find. It is said once — the status bar carrying it is overwritten by the next action, measured
    /// in GUI-C-122 — and what survives is the log list, whose items support no text pattern. So the
    /// path was on screen and could only be copied by reading it off and typing it again.</para>
    ///
    /// <para>The item template is deliberately unchanged: every other log line renders exactly as it
    /// did, and this adds a way to take one rather than a new way to show them.</para>
    ///
    /// <para><b>Disabled while nothing is selected (#173, GUI-C-123).</b> It used to be pressable with
    /// no selection and then do nothing at all — no copy, no message — so the clipboard still held
    /// whatever was there before and pasting produced something else entirely, with the app having said
    /// nothing. Of the ways to fix that, a disabled button is the only one that cannot mislead: a
    /// message after the press still leaves the user having pressed it, and copying "the last line"
    /// instead would be the app choosing on their behalf — a different silent wrong answer.</para>
    /// </summary>
    public RelayCommand CopySelectedLogCommand { get; }

    private void CopySelectedLog()
    {
        if (string.IsNullOrEmpty(SelectedLog)) return;

        try
        {
            System.Windows.Clipboard.SetText(SelectedLog);
            StatusText = "Log line copied.";
        }
        catch (Exception ex)
        {
            // The clipboard can be held by another process. Saying so beats a button that looks like
            // it worked.
            StatusText = $"Could not copy the log line: {ex.Message}";
        }
    }

    public RelayCommand InitializeBackendCommand { get; }

    /// <summary>
    /// Switches the requested backend and re-initialises, so the Backend &gt; Mode menu is a command
    /// rather than a label (#225, GUI-C-153, table row 4).
    ///
    /// <para><b>What was missing.</b> The routing already existed — <c>XpeBackendFactory.Create</c> reads
    /// <see cref="AppSettings.BackendMode"/>, and CI's gui-e2e-native drives that path through the
    /// command line. What did not exist was any way to change it from inside the app: the <c>_Mock</c>
    /// item was checkable but carried no command, and <c>_Native</c> was disabled behind a tooltip
    /// saying the P/Invoke integration had not happened yet. It had.</para>
    ///
    /// <para><b>Why it re-initialises rather than only setting the property.</b> The backend object is
    /// built once by the factory; setting the property alone changes what is REQUESTED and leaves the
    /// running backend as it was — <see cref="RequestedBackendMode"/> would then disagree with
    /// <see cref="ActualBackendMode"/> with nothing having happened. Re-initialising is what makes the
    /// menu mean what it says.</para>
    ///
    /// <para><b>The fall-back stays visible.</b> The factory silently returns the mock backend when the
    /// native DLLs cannot load, so asking for Native is not the same as getting it. That gap is already
    /// surfaced by <see cref="ActualBackendMode"/> / <see cref="RequestedBackendMode"/>, and the menu
    /// check marks follow the ACTUAL mode for that reason.</para>
    /// </summary>
    public RelayCommand<string> SetBackendModeCommand { get; }

    /// <summary>
    /// Calls three native entry points directly and reports whether they answered (#225, GUI-C-153/154,
    /// table row 6).
    ///
    /// <para><b>It goes around the backend deliberately.</b> Every other native path in this app runs
    /// through <see cref="IXpeBackend"/>, which falls back to the mock when the DLLs will not load — a
    /// smoke test built on that abstraction would report success while nothing native ran, which is the
    /// failure this command exists to make impossible. These probes are <c>DllImport</c> calls into
    /// <c>xpe_common.dll</c> and <c>xpe_display.dll</c>; when the libraries are absent the runtime throws
    /// and the command says FAILED.</para>
    ///
    /// <para><b>It can be red</b>, and that was checked rather than assumed: GUI-C-154 ran it with
    /// <c>XPE_NATIVE_DIR</c> pointed at an empty directory and <c>XPE_NATIVE_DIR_EXCLUSIVE=1</c> — the
    /// pin the shared search policy honours (#129, and GUI-C-31 measured what happens without it) — and
    /// the report carried the failure. A smoke test that cannot go red is not a smoke test.</para>
    /// </summary>
    public RelayCommand RunPInvokeSmokeTestCommand { get; }

    /// <summary>
    /// Stops the render in flight (#225, GUI-C-154, table row 11).
    ///
    /// <para><b>What it actually does, stated rather than implied.</b> The work it stops is the
    /// background task in <see cref="ApplyDisplayPipelineAsync"/> — the chain plus the display pipeline,
    /// 2.3-2.5 s on the wrist slice (GUI-C-153 measured it). The native calls inside that task are
    /// synchronous and are NOT interrupted: what cancelling does is discard their result instead of
    /// applying it, so the viewport and the status keep describing the frame that is actually shown.
    /// Claiming it aborts native work would be the kind of promise #208 spent two cards removing.</para>
    ///
    /// <para><b>Pressed with nothing running it says so.</b> A command that silently does nothing is the
    /// #165 shape — enabled, and no way to tell whether it worked. The no-op path writes a status line.</para>
    /// </summary>
    public RelayCommand StopProcessingCommand { get; }

    /// <summary>
    /// Shows the timings the last render already reported (#225, GUI-C-154, table row 12).
    ///
    /// <para><b>It re-measures nothing.</b> The numbers come from <see cref="PipelineTimings"/> and
    /// <see cref="ChainStatus"/>, which the render itself wrote. Timing the work again from this command
    /// would produce a second set of numbers that disagrees with the status bar for reasons no reader
    /// could resolve — #201 named that shape: an instrument that fires its own action cannot attribute.</para>
    ///
    /// <para><b>Before any render it says so</b> rather than showing an empty line, because "no timings
    /// yet" and "the render reported nothing" are different states.</para>
    /// </summary>
    public RelayCommand ShowStageTimingCommand { get; }

    /// <summary>
    /// Runs the GUI-S0 self-check as a child process and reports its verdict (#225, GUI-C-158, row 15).
    ///
    /// <para><b>Measured before it was wired</b> (GUI-C-157): the runner takes about 1 s (1.05 s alone,
    /// 0.78 s with this app already up) and exits 0 on success. That is short enough that a progress
    /// line is enough and no cancellation is needed — but long enough that running it on the UI thread
    /// would look like a hang, so it is awaited off-thread.</para>
    ///
    /// <para><b>The verdict is <c>ExitCode == 0</c>, and only that.</b> A failing run exits
    /// <c>-532462766</c> (0xE0434352, the .NET unhandled-exception code), NOT 1 — the self-check
    /// reports failure by throwing. GUI-C-157 nearly recorded "127" for this, which is what bash's
    /// <c>$?</c> reports; the real process code came from reading it directly. Anything that tests for
    /// a specific failure number here will be wrong.</para>
    ///
    /// <para><b>Development-machine only, and it says so.</b> The runner locates its fixtures by walking
    /// up to the repository root, so outside a checkout it dies in ~150 ms with "Repository root could
    /// not be located" (GUI-C-157 §4). This command therefore looks for the runner under the repository
    /// and, when there is none, writes a status line rather than being disabled: a disabled item would
    /// change <c>DisabledFutureCommandCount</c> depending on where the app was launched from, so the
    /// same build would report different numbers (GUI-C-156 §3.2).</para>
    /// </summary>
    public RelayCommand RunSelfCheckCommand { get; }

    /// <summary>#225 row 16: runs the GUI E2E runner and reports its verdict.</summary>
    public RelayCommand RunGuiE2ECommand { get; }

    /// <summary>#225 row 17: runs CI's benchmark freeze tests through ctest and shows ctest's own verdict.</summary>
    public RelayCommand RunBenchmarkCommand { get; }

    /// <summary>#225 row 5: writes the in-memory runtime log to a file under this run set's evidence.</summary>
    public RelayCommand ExportRuntimeLogsCommand { get; }

    /// <summary>#225 row 14: opens this run set's evidence directory in the OS file browser.</summary>
    public RelayCommand OpenEvidenceFolderCommand { get; }

    /// <summary>#225 row 13: shows the pipeline diagnostics panel.</summary>
    public RelayCommand OpenPipelineDiagnosticsCommand { get; }

    /// <summary>#225 row 13: hides it again.</summary>
    public RelayCommand ClosePipelineDiagnosticsCommand { get; }

    /// <summary>
    /// #225 row 13 (GUI-C-168): whether the pipeline diagnostics panel is showing.
    ///
    /// <para><b>Session state, deliberately not persisted.</b> Three reasons, and the third is the one
    /// that decided it. (1) It is a view preference, not a run selection — #136 separated those because
    /// a persisted value let whatever a PREVIOUS run left behind decide what a check sees. (2) The
    /// automation report already emits the chain itself (<c>stages</c>, <c>displayInput</c>), so a
    /// scenario checks the DATA without needing this panel open; a report field for its visibility would
    /// buy nothing. (3) The flag needs a READER, and here the panel is it — which is exactly what the two
    /// panel flags lacked at the time (persisted, reported, no panel behind them).</para>
    ///
    /// <para><b>Why rows 7 and 8 went the other way (GUI-C-170).</b> This is a command
    /// (<c>Open Pipeline Diagnostics</c>, no <c>IsCheckable</c>), so there is no toggle state for a menu
    /// checkmark to follow; rows 7 and 8 ARE checkable toggles whose state belongs on the menu across runs,
    /// and their flag already had the stronger readers (report, integration tests). Same criterion — "does
    /// the flag have a reader" — applied to a different shape.</para>
    /// </summary>
    public bool ShowPipelineDiagnostics
    {
        get => _showPipelineDiagnostics;
        private set => SetProperty(ref _showPipelineDiagnostics, value);
    }

    /// <summary>
    /// #225 row 13: whether there is a measurement to show at all — <see cref="LastChain"/> is null until
    /// a chain has run.
    ///
    /// <para>The panel needs this as its own line because an EMPTY LIST reads as "nothing happened"
    /// rather than "nothing was measured". Derived, not stored: nothing here can go stale.</para>
    /// </summary>
    public bool HasPipelineDiagnostics => LastChain is not null;

    public RelayCommand ShutdownBackendCommand { get; }

    public RelayCommand LoadImageCommand { get; }

    public RelayCommand ApplyDisplayPipelineCommand { get; }

    public RelayCommand ApplyBodyPartPresetCommand { get; }

    /// <summary>#141: true when the last chain run applied the preprocess stage (Applied or AppliedNoChange).</summary>
    public bool PreprocessRan { get; private set; }

    /// <summary>#141: the preprocess stage's message from the last chain run that requested it.</summary>
    public string PreprocessStages { get; private set; } = string.Empty;

    /// <summary>
    /// #141 / #180 (GUI-C-99): switches the preprocess stage on in the pixel chain and renders again.
    /// The corrected pixels now feed the display pipeline instead of replacing the preview directly.
    /// </summary>
    public RelayCommand RunPreprocessingCommand { get; }

    /// <summary>
    /// #225 row 10 (GUI-C-184): switches AI bone suppression on and renders again, as Run Preprocessing does for its
    /// stage. The result is the chain's: Applied only when the module returned 0.
    /// </summary>
    public RelayCommand RunAiBoneSuppressionCommand { get; }

    /// <summary>
    /// "AI-processed: bone suppression" when the last chain's AI stage was Applied (the module returned 0 AND the
    /// pixels changed); empty otherwise. Never set for a stage that was refused or failed, nor for one that returned 0
    /// with an unchanged image.
    /// </summary>
    public string AiProcessedLabel { get; private set; } = string.Empty;

    private AiWorkerStatus _aiWorkerStatus = AiWorkerStatus.Unknown;

    /// <summary>
    /// GUI-C-185: true while the module reports the AI worker switched off for this session. The source is
    /// <c>xpe_ai_worker_state</c>, not an alert: alerts are drained by their reader and can overflow, the state stays.
    /// </summary>
    public bool AiWorkerDisabled => _aiWorkerStatus.State == AiWorkerState.Disabled;

    /// <summary>
    /// True when the persistent mark and the Restart AI button show: the worker is switched off, OR the AI session could not be
    /// started (a restart that failed, or a start that failed after the directory changed). The retry button must not vanish
    /// when recovery has just failed (Codex #24 B1).
    /// </summary>
    public bool AiWorkerMarkVisible => AiBoneSuppressionStage.ShowsMark(_aiWorkerStatus);

    /// <summary>The persistent text: the module's failure count and ceiling for a switched-off worker, the reason for a failed start; empty otherwise.</summary>
    public string AiWorkerBannerText => AiBoneSuppressionStage.BannerFor(_aiWorkerStatus);

    /// <summary>The state as one line for automation (read from the AI checkbox's help text), e.g. <c>worker=Active; failures=0; ceiling=3</c>.</summary>
    public string AiWorkerStatusSummary => AiBoneSuppressionStage.DescribeStatus(_aiWorkerStatus);

    /// <summary>GUI-C-185: shutdown then init under the one lock; the mark goes when the module reports a new session.</summary>
    public RelayCommand RestartAiSessionCommand { get; }

    /// <summary>
    /// Asks for the worker state to be read again and shown when it changed. Returns at once: the read waits for the session gate
    /// in the background (a frame waiting on a silent worker holds it) and the result is applied on the UI thread when it is
    /// still current (GUI-C-186d). A backend without an AI session answers Unknown, which shows nothing.
    /// </summary>
    private void RefreshAiWorkerStatus() => AiStatus.Request();

    private AiStatusRefresher? _aiStatusRefresher;

    private readonly System.Windows.Threading.Dispatcher _uiDispatcher = System.Windows.Threading.Dispatcher.CurrentDispatcher;

    private AiStatusRefresher AiStatus => _aiStatusRefresher ??=
        new AiStatusRefresher(() => _backend, ReadAiWorkerStatus, ApplyAiWorkerStatus, work => Task.Run(work), PostToUi);

    /// <summary>Runs on a background thread, for the backend that was current when the read was requested.</summary>
    private static AiWorkerStatus? ReadAiWorkerStatus(object? backend) =>
        backend is IAiSessionBackend session ? session.GetAiWorkerStatus() : AiWorkerStatus.Unknown;

    /// <summary>Hands <paramref name="action"/> to the UI thread; dropped when the dispatcher is already shutting down (a closed screen is not updated).</summary>
    private void PostToUi(Action action)
    {
        if (_uiDispatcher.HasShutdownStarted)
        {
            return;
        }

        _uiDispatcher.BeginInvoke(System.Windows.Threading.DispatcherPriority.Normal, action);
    }

    /// <summary>The application is closing: no status read, started or running, may update the screen after this.</summary>
    public void StopAiStatusUpdates() => AiStatus.Stop();

    private void ApplyAiWorkerStatus(AiWorkerStatus status)
    {
        if (status == _aiWorkerStatus)
        {
            return;
        }

        _aiWorkerStatus = status;
        OnPropertyChanged(nameof(AiWorkerDisabled));
        OnPropertyChanged(nameof(AiWorkerMarkVisible));
        OnPropertyChanged(nameof(AiWorkerBannerText));
        OnPropertyChanged(nameof(AiWorkerStatusSummary));
    }

    private async void RestartAiSession()
    {
        if (RefusedWhileTransitioning("Restart AI"))
        {
            return;
        }

        if (_backend is not IAiSessionBackend session)
        {
            StatusText = "AI session restart needs the native backend.";
            Log(StatusText);
            RefreshAiWorkerStatus(); // a backend with no AI session reports Unknown, which shows nothing
            return;
        }

        try
        {
            var directory = Settings.AiModelDirectory;
            var result = await Task.Run(() => session.RestartAiSession(directory));
            DrainBackendTelemetry();
            StatusText = result.Message;
            Log(result.Message);
        }
        catch (Exception ex)
        {
            StatusText = $"AI session could not be restarted: {ex.Message}";
            Log(StatusText);
        }

        RefreshAiWorkerStatus();
    }

    private ChainResult? _lastChain;
    private float _renderedLaneBWidth;
    private string _renderedLaneBAlgorithm = string.Empty;
    private double _renderedLaneBDenoiseK = 2.0;
    private string _pipelineTimings = string.Empty;
    private string _chainStatus = "chain: not run";

    /// <summary>The pixel chain of the processed image on screen (#180, GUI-C-99), or null before the first render.</summary>
    public ChainResult? LastChain
    {
        get => _lastChain;
        private set
        {
            if (!SetProperty(ref _lastChain, value)) return;
            // #225 row 13 (GUI-C-168): the panel's "no measurement yet" line is derived from this, so
            // without this raise the first chain leaves the panel still saying nothing has run.
            OnPropertyChanged(nameof(HasPipelineDiagnostics));
        }
    }

    /// <summary>
    /// Status bar summary of <see cref="LastChain"/>: each stage and its status, and whether the display started
    /// from the raw frame. A requested stage that did not apply says so here (HAZ-GUI-005).
    /// </summary>
    public string ChainStatus
    {
        get => _chainStatus;
        private set => SetProperty(ref _chainStatus, value);
    }

    /// <summary>
    /// Where the time of the last apply went (#180, GUI-C-103): the background work, this view model's own
    /// share, and the display pipeline's four phases. It does NOT include the render — nothing on this side
    /// can observe when the frame reached the screen, so that share is measured from outside.
    /// </summary>
    public string PipelineTimings
    {
        get => _pipelineTimings;
        private set => SetProperty(ref _pipelineTimings, value);
    }

    /// <summary>
    /// #141: whether the menu entry is usable. False on Mock, which has no preprocess module —
    /// the entry stays visible with a tooltip rather than disappearing, so the reason is on screen.
    /// </summary>
    public bool CanRunPreprocessing => _backend.SupportsPreprocessing;

    public RelayCommand ZoomFitCommand { get; }

    public RelayCommand ZoomActualCommand { get; }

    public RelayCommand ZoomInCommand { get; }

    public RelayCommand ZoomOutCommand { get; }

    public RelayCommand ResetComparisonViewCommand { get; }

    /// <summary>
    /// Selects a comparison mode by name (GUI-C-58, #149 G-1/G-2/G-3).
    ///
    /// <para>The renderer has supported seven modes since GUI-C-46, and four of them could be reached
    /// by the segmented buttons in <c>ViewportShell</c>; the rest had no caller at all. The buttons
    /// write <c>Settings.ComparisonMode</c> from their own code-behind, so a menu item or a key
    /// gesture had nothing to bind to — this command is that missing seam, and every entry point now
    /// goes through it.</para>
    ///
    /// <para>An unknown or empty name is ignored rather than written through: the parameter comes
    /// from XAML, where a typo is not a compile error, and a bad write would leave the viewport
    /// falling back to its default mode with nothing saying why.</para>
    /// </summary>
    public RelayCommand<string> SetComparisonModeCommand { get; }

    public RelayCommand DetachComparisonViewerCommand { get; }

    public RelayCommand SaveSettingsCommand { get; }

    public RelayCommand BrowseOffsetCalibrationDirectoryCommand { get; }

    public RelayCommand BrowseGainCalibrationDirectoryCommand { get; }

    public RelayCommand BrowseDefectCalibrationDirectoryCommand { get; }

    public RelayCommand ClearLogsCommand { get; }

    public RelayCommand ClearAlertsCommand { get; }

    public RelayCommand ResetLayoutCommand { get; }

    public RelayCommand ShowNativeDiagnosticsCommand { get; }

    public RelayCommand ShowCalibrationSettingsCommand { get; }

    public RelayCommand ShowFixtureManagerCommand { get; }

    public RelayCommand ExportAutomationReportCommand { get; }

    // Slice 2 — workbench commands
    public RelayCommand RunOnAllQueuedCommand { get; }
    public RelayCommand<Verdict> RecordVerdictCommand { get; }
    public RelayCommand SaveAndNextCommand { get; }
    public RelayCommand ToggleFocusModeCommand { get; }
    public RelayCommand ToggleRoiCommand { get; }
    public RelayCommand ExportEvidenceBundleCommand { get; }
    public RelayCommand<string> SwitchAnalysisTabCommand { get; }
    public RelayCommand ResetLaneBOverridesCommand { get; }

    public string StatusText
    {
        get => _statusText;
        private set => SetProperty(ref _statusText, value);
    }

    public string ActiveImageSummary
    {
        get => _activeImageSummary;
        private set => SetProperty(ref _activeImageSummary, value);
    }

    public string MetadataText
    {
        get => _metadataText;
        private set => SetProperty(ref _metadataText, value);
    }

    public string DisplayPipelineSummary
    {
        get => _displayPipelineSummary;
        private set => SetProperty(ref _displayPipelineSummary, value);
    }

    public string CalibrationEvaluationSummary =>
        $"Offset={Settings.OffsetCorrectionMode}, Gain={Settings.GainCorrectionMode}, Defect={Settings.DefectCorrectionMode}, " +
        $"Ghost={Settings.GhostCorrectionMode}, Temp={Settings.TemperatureCompensationMode}, " +
        $"Nonlinearity={Settings.NonlinearityCorrectionMode}, Binning={Settings.BinningCorrectionMode}";

    public string CalibStageCountDisplay
    {
        get
        {
            string[] modes =
            [
                Settings.OffsetCorrectionMode, Settings.GainCorrectionMode,
                Settings.DefectCorrectionMode, Settings.GhostCorrectionMode,
                Settings.TemperatureCompensationMode, Settings.NonlinearityCorrectionMode,
                Settings.BinningCorrectionMode
            ];
            var enabled = modes.Count(m => !string.Equals(m, "Off", StringComparison.OrdinalIgnoreCase));
            return $"{enabled}/7 stages";
        }
    }

    public string ComparisonStatus =>
        $"Mode={Settings.ComparisonMode}, Zoom={(Settings.ComparisonZoomScale <= 0.0 ? "Fit" : $"{Settings.ComparisonZoomScale * 100.0:0}%")}, " +
        $"Pan=({Settings.ComparisonPanX:0},{Settings.ComparisonPanY:0}), Swipe={Settings.ComparisonSwipePosition:P0}, Overlay={Settings.ComparisonOverlayOpacity:P0}";

    public System.Windows.Media.ImageSource? SourceImage
    {
        get => _sourceImage;
        private set => SetProperty(ref _sourceImage, value);
    }

    public System.Windows.Media.ImageSource? ProcessedImage
    {
        get => _processedImage;
        private set => SetProperty(ref _processedImage, value);
    }

    // #171 ② (GUI-C-79): the VOI values that produced the image in the processed viewport — NOT the
    // current settings. The HUD next to the image used to read Settings, so after a VOI edit it showed
    // a window the image was never rendered with (GUI-C-77). Null means the processed image was not
    // produced by the display pipeline (a fresh load, or a preprocessing preview).
    public float? RenderedVoiCenter
    {
        get => _renderedVoiCenter;
        private set => SetProperty(ref _renderedVoiCenter, value);
    }

    public float? RenderedVoiWidth
    {
        get => _renderedVoiWidth;
        private set => SetProperty(ref _renderedVoiWidth, value);
    }

    public string? RenderedVoiMode
    {
        get => _renderedVoiMode;
        private set => SetProperty(ref _renderedVoiMode, value);
    }

    private void SetRenderedVoi(AppSettings? inputs)
    {
        _renderedInputs = inputs;
        RenderedVoiCenter = inputs?.VoiWindowCenter;
        RenderedVoiWidth = inputs?.VoiWindowWidth;
        RenderedVoiMode = inputs?.VoiLutMode;
        RefreshParametersStale();
    }

    // #171 ① / ③ (GUI-C-79): why the processed image no longer matches what the operator asked for, or
    // null when it does. ① is "apply + mark stale" rather than an immediate re-render: the pipeline has no
    // cancellation or ordering, so re-rendering on every keystroke could let an older value's result
    // arrive last and be shown as current — a new stale path created by the control itself.
    public const string StaleParametersChanged =
        "STALE — display parameters changed since this image was rendered. Apply the display pipeline to update it.";

    /// <summary>
    /// ③ — a display pipeline call threw, so the processed image on screen is the one from before the
    /// attempt. It stays up (nothing better exists to show) and is marked until a render succeeds or a new
    /// image is loaded; a later parameter edit does not replace this reason.
    /// </summary>
    public const string StalePipelineFailed =
        "STALE — the display pipeline failed; the image shown is from before the failed attempt.";

    public string? PreviewStaleReason
    {
        get => _previewStaleReason;
        private set
        {
            if (SetProperty(ref _previewStaleReason, value))
            {
                OnPropertyChanged(nameof(IsPreviewStale));
            }
        }
    }

    public bool IsPreviewStale => PreviewStaleReason is not null;

    /// <summary>
    /// #171 (GUI-C-79): <c>faultInjection=off</c> unless the app was started with
    /// <c>--automation-fault</c>. Exposed as the main window's automation status so the E2E suite can
    /// check that an ordinary launch carries no fault, not just assume it.
    /// </summary>
    public string FaultInjectionStatus => FaultInjectingBackend.Describe();

    /// <summary>Called once at start-up when a fault was armed — the log says so first.</summary>
    public void AnnounceFaultInjection()
    {
        Log($"FAULT INJECTION ARMED: {FaultInjectionStatus}. Display pipeline calls past the limit throw on purpose.");
        _faultInjectionAnnounced = true;
        OnPropertyChanged(nameof(FaultInjectionStatus));
        OnPropertyChanged(nameof(WindowTitle));
    }

    /// <summary>
    /// ① — the image is stale when a setting the display pipeline reads differs from the snapshot that
    /// rendered it. Only meaningful while the processed image IS a display-pipeline render.
    /// </summary>
    private void RefreshParametersStale()
    {
        var differs = _renderedInputs is not null && DisplayInputsDiffer(_renderedInputs, Settings);

        if (differs && PreviewStaleReason is null)
        {
            PreviewStaleReason = StaleParametersChanged;
        }
        else if (!differs && PreviewStaleReason == StaleParametersChanged)
        {
            PreviewStaleReason = null;
        }
    }

    /// <summary>The settings either backend's ApplyDisplayPipeline reads (Mock and Real, GUI-C-79).</summary>
    private static bool DisplayInputsDiffer(AppSettings a, AppSettings b) =>
        ChainInputsDiffer(a, b)
        || a.VoiWindowCenter != b.VoiWindowCenter
        || a.VoiWindowWidth != b.VoiWindowWidth
        || !string.Equals(a.VoiLutMode, b.VoiLutMode, StringComparison.Ordinal)
        || a.ModalityRescaleSlope != b.ModalityRescaleSlope
        || a.ModalityRescaleIntercept != b.ModalityRescaleIntercept
        || a.GsdfEnabled != b.GsdfEnabled
        || !string.Equals(a.OffsetCorrectionMode, b.OffsetCorrectionMode, StringComparison.Ordinal)
        || !string.Equals(a.GainCorrectionMode, b.GainCorrectionMode, StringComparison.Ordinal)
        || !string.Equals(a.DefectCorrectionMode, b.DefectCorrectionMode, StringComparison.Ordinal)
        || !string.Equals(a.GhostCorrectionMode, b.GhostCorrectionMode, StringComparison.Ordinal)
        || !string.Equals(a.TemperatureCompensationMode, b.TemperatureCompensationMode, StringComparison.Ordinal)
        || !string.Equals(a.NonlinearityCorrectionMode, b.NonlinearityCorrectionMode, StringComparison.Ordinal)
        || !string.Equals(a.BinningCorrectionMode, b.BinningCorrectionMode, StringComparison.Ordinal);

    /// <summary>
    /// The settings the pixel chain reads (#180, GUI-C-99): the stage switch, the one exposure kVp, and the
    /// preprocess inputs (calibration directories, body part). A change to any of them makes the image stale.
    /// </summary>
    private static bool ChainInputsDiffer(AppSettings a, AppSettings b) =>
        a.PreprocessInChain != b.PreprocessInChain
        || a.AiBoneSuppressionInChain != b.AiBoneSuppressionInChain
        || !string.Equals(a.AiModelDirectory, b.AiModelDirectory, StringComparison.Ordinal)
        || !string.Equals(a.GsvgMode, b.GsvgMode, StringComparison.Ordinal)
        || !string.Equals(a.GsvgTablePath, b.GsvgTablePath, StringComparison.Ordinal)
        || a.GsvgGridRatio != b.GsvgGridRatio
        || a.GsvgGridFrequencyPerCm != b.GsvgGridFrequencyPerCm
        || a.GsvgAirSignal != b.GsvgAirSignal
        || a.GsvgIterations != b.GsvgIterations
        || a.GsvgPyramidLevels != b.GsvgPyramidLevels
        || Math.Abs(a.GsvgPyramidGain - b.GsvgPyramidGain) > 0.0001
        || Math.Abs(a.GsvgDenoiseK - b.GsvgDenoiseK) > 0.0001
        || a.PixelPitchMm != b.PixelPitchMm
        || a.ExposureKvp != b.ExposureKvp
        || !string.Equals(a.OffsetCalibrationDirectory, b.OffsetCalibrationDirectory, StringComparison.Ordinal)
        || !string.Equals(a.GainCalibrationDirectory, b.GainCalibrationDirectory, StringComparison.Ordinal)
        || !string.Equals(a.DefectCalibrationDirectory, b.DefectCalibrationDirectory, StringComparison.Ordinal)
        || !string.Equals(a.SelectedBodyPart, b.SelectedBodyPart, StringComparison.Ordinal);

    public BackendRuntimeInfo RuntimeInfo
    {
        get => _runtimeInfo;
        private set
        {
            if (SetProperty(ref _runtimeInfo, value))
            {
                OnPropertyChanged(nameof(RuntimeVersionSummary));
                RaiseBackendIdentityChanged();
            }
        }
    }

    // #175 HAZ-GUI-005 (GUI-C-82): which backend is ACTUALLY producing the images. The factory falls back
    // to Mock silently when Native cannot load (XpeBackendFactory.Create), and Settings.BackendMode only
    // says what was requested. RuntimeInfo is reported by the backend itself - through any wrapper - so it
    // is the one source for the status bar, the banner, the title and the reports. Anything that is not
    // positively the native backend counts as Mock: an unknown backend is warned about, not trusted.
    public const string BaseWindowTitle = "ImageProcTest GUI-S0";

    private bool _faultInjectionAnnounced;

    public bool IsMockBackend => !string.Equals(RuntimeInfo.BackendName, "RealXpeBackend", StringComparison.Ordinal);

    public string ActualBackendMode => IsMockBackend ? "Mock" : "Native";

    /// <summary>The requested mode, when it differs from the actual one; otherwise null.</summary>
    public string? RequestedBackendMismatch =>
        string.Equals(Settings.BackendMode, ActualBackendMode, StringComparison.OrdinalIgnoreCase) ? null : Settings.BackendMode;

    /// <summary>HAZ-GUI-005 (1): the text of the banner that cannot be dismissed while Mock is active.</summary>
    public string MockBackendWarning =>
        RequestedBackendMismatch is { } requested
            ? $"MOCK BACKEND — {requested} was requested but could not be used. Images and results are synthetic; not for clinical judgement."
            : "MOCK BACKEND — images and results are synthetic; not for clinical judgement.";

    /// <summary>HAZ-GUI-005 (2): <c>[MOCK]</c> in the title while Mock is active.</summary>
    public string WindowTitle =>
        (IsMockBackend ? "[MOCK] " : string.Empty) + BaseWindowTitle
        + (_faultInjectionAnnounced ? " — FAULT INJECTION ARMED" : string.Empty);

    private void RaiseBackendIdentityChanged()
    {
        OnPropertyChanged(nameof(IsMockBackend));
        OnPropertyChanged(nameof(ActualBackendMode));
        OnPropertyChanged(nameof(RequestedBackendMismatch));
        OnPropertyChanged(nameof(MockBackendWarning));
        OnPropertyChanged(nameof(WindowTitle));
    }

    /// <summary>
    /// One line naming the backend mode and the versions behind it, for the status bar.
    ///
    /// #136 / XPE-GUI-E2E-001 S-05: until GUI-C-30 no element rendered a version at all — the
    /// operator could not tell from the screen which native build was running, and the E2E scenario
    /// had nothing to assert. Only versions the backend actually reported are listed; a Mock run
    /// shows its own marker rather than pretending a native version exists.
    /// </summary>
    public string RuntimeVersionSummary
    {
        get
        {
            // #175: the ACTUAL backend, with the request beside it when the two differ.
            var parts = new List<string>
            {
                RequestedBackendMismatch is { } requested
                    ? $"mode={ActualBackendMode} (requested {requested})"
                    : $"mode={ActualBackendMode}"
            };

            if (!string.IsNullOrWhiteSpace(RuntimeInfo.Version))
            {
                parts.Add($"common={RuntimeInfo.Version}");
            }

            if (!string.IsNullOrWhiteSpace(RuntimeInfo.DisplayVersion))
            {
                parts.Add($"display={RuntimeInfo.DisplayVersion}");
            }

            // #129 (GUI-C-33): where the library actually came from. "loader" means the DllImport
            // resolver did not supply it — the observation C-32 could not make. Absent for Mock.
            if (!string.IsNullOrWhiteSpace(RuntimeInfo.NativeSource))
            {
                parts.Add($"src={RuntimeInfo.NativeSource}");
            }

            return string.Join("  |  ", parts);
        }
    }

    public LoadedImageFrame? ActiveImageFrame
    {
        get => _activeImageFrame;
        private set => SetProperty(ref _activeImageFrame, value);
    }

    // No readers since GUI-C-68: ShowRuntimePanel, ShowRawSettingsPanel, ShowImageSummaryPanel,
    // ShowMetadataPanel and ShowAlertsPanel name panels that do not exist — their menu items were
    // removed (C-65) and the automation report stopped emitting them (C-68). Removal is a separate card.
    //
    // #225 (GUI-C-168) CORRECTION to the line that used to end this comment, updated by GUI-C-170. It said
    // "ShowCalibrationPanel and ShowLogsPanel below are still read and stay". GUI-C-168 measured that only
    // half was true: ShowLogsPanel is read by the log region, while ShowCalibrationPanel's ONLY reader was its
    // own menu item's IsChecked binding — and the neighbouring persisted Settings.ShowDisplayPanel also had
    // no panel (persistence is not a reader).
    //
    // GUI-C-170 (rows 7 and 8) gave both a reader and settled where they live: BOTH are persisted
    // Settings.ShowCalibrationPanel / Settings.ShowDisplayPanel, read by Views/CalibrationPathsPanel and
    // Views/DisplaySettingsPanel, and both reach the automation report (never the Passed verdict — see the
    // comment at the verdict). ShowCalibrationPanel MOVED here-to-Settings; it was not deleted, and its old
    // readers (menu IsChecked, the diagnostic dump, Reset Layout, Tools > Calibration Settings) were
    // re-pointed rather than dropped.
    public bool ShowRuntimePanel
    {
        get => _showRuntimePanel;
        set => SetProperty(ref _showRuntimePanel, value);
    }

    public bool ShowRawSettingsPanel
    {
        get => _showRawSettingsPanel;
        set => SetProperty(ref _showRawSettingsPanel, value);
    }

    public bool ShowImageSummaryPanel
    {
        get => _showImageSummaryPanel;
        set => SetProperty(ref _showImageSummaryPanel, value);
    }

    public bool ShowMetadataPanel
    {
        get => _showMetadataPanel;
        set => SetProperty(ref _showMetadataPanel, value);
    }

    public bool ShowLogsPanel
    {
        get => _showLogsPanel;
        set => SetProperty(ref _showLogsPanel, value);
    }

    public bool ShowAlertsPanel
    {
        get => _showAlertsPanel;
        set => SetProperty(ref _showAlertsPanel, value);
    }

    // Slice 2 — settings-backed pass-through properties
    /// <summary>
    /// The Reference lane's algorithm. Choosing it writes that preset into the MAIN chain settings,
    /// because the Reference lane IS the main render (GUI-C-113) — if the two could differ, the lane
    /// would be showing something the rest of the window is not.
    /// </summary>
    public string LaneAAlgorithm
    {
        get => Settings.LaneAAlgorithm;
        set
        {
            if (Settings.LaneAAlgorithm == value) return;
            Settings.LaneAAlgorithm = value;
            AlgorithmPreset.For(value).ApplyTo(Settings);
            OnPropertyChanged();
        }
    }

    /// <summary>The Candidate lane's algorithm. Read only by the Candidate's own render.</summary>
    public string LaneBAlgorithm
    {
        get => Settings.LaneBAlgorithm;
        set
        {
            if (Settings.LaneBAlgorithm == value) return;
            Settings.LaneBAlgorithm = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(LaneBIsStale));
            OnPropertyChanged(nameof(LaneBDenoiseKApplies));
            OnPropertyChanged(nameof(LaneBDenoiseKUnapplied));
        }
    }

    public bool FocusMode
    {
        get => Settings.FocusMode;
        set { if (Settings.FocusMode != value) { Settings.FocusMode = value; OnPropertyChanged(); } }
    }

    public bool LeftPanelOpen
    {
        get => Settings.LeftPanelOpen;
        set { if (Settings.LeftPanelOpen != value) { Settings.LeftPanelOpen = value; OnPropertyChanged(); } }
    }

    public bool RightPanelOpen
    {
        get => Settings.RightPanelOpen;
        set { if (Settings.RightPanelOpen != value) { Settings.RightPanelOpen = value; OnPropertyChanged(); } }
    }

    public string AnalysisTab
    {
        get => Settings.AnalysisTab;
        set { if (Settings.AnalysisTab != value) { Settings.AnalysisTab = value; OnPropertyChanged(); } }
    }

    /// <summary>The Candidate lane's own virtual-grid de-noise k. Read only by the Candidate's render.</summary>
    public double LaneBGsvgDenoiseK
    {
        get => Settings.LaneBGsvgDenoiseK;
        set
        {
            if (Math.Abs(Settings.LaneBGsvgDenoiseK - value) <= 0.0001) return;
            Settings.LaneBGsvgDenoiseK = value;
            OnPropertyChanged();
            OnPropertyChanged(nameof(LaneBIsStale));
        }
    }

    /// <summary>
    /// Whether the Candidate's de-noise k can reach any pixel right now (#173, GUI-C-117).
    ///
    /// <para>GuiGsvgRunner sends <c>vg_denoise_k</c> only while the lane runs the virtual grid, and
    /// sends <c>0.0</c> when the pyramid is switched off. Outside those the value is accepted, stored,
    /// and silently ignored — which is the shape of every defect this issue has turned up. The input is
    /// disabled and marked instead.</para>
    /// </summary>
    public bool LaneBDenoiseKApplies =>
        string.Equals(AlgorithmPreset.For(Settings.LaneBAlgorithm).GsvgMode, GsvgModes.VirtualGrid, StringComparison.Ordinal)
        && Settings.GsvgPyramidLevels > 0;

    /// <summary>The negation, for the "미적용" mark the screen shows while the value reaches nothing.</summary>
    public bool LaneBDenoiseKUnapplied => !LaneBDenoiseKApplies;

    // Slice 2 — VM-only properties
    public System.Windows.Media.ImageSource? LaneAImage
    {
        get => _laneAImage;
        private set => SetProperty(ref _laneAImage, value);
    }

    public System.Windows.Media.ImageSource? LaneBImage
    {
        get => _laneBImage;
        private set => SetProperty(ref _laneBImage, value);
    }

    public string ActiveStudyId
    {
        get => _activeStudyId;
        set => SetProperty(ref _activeStudyId, value);
    }

    public ObservableCollection<StudyEntry> Studies { get; }

    public RunSetState RunSet
    {
        get => _runSet;
        set => SetProperty(ref _runSet, value);
    }

    public Verdict? ActiveVerdict
    {
        get => _activeVerdict;
        set => SetProperty(ref _activeVerdict, value);
    }

    public string VerdictNotes
    {
        get => _verdictNotes;
        set => SetProperty(ref _verdictNotes, value);
    }

    public bool RoiActive
    {
        get => _roiActive;
        set => SetProperty(ref _roiActive, value);
    }

    public bool HistogramActive
    {
        get => _histogramActive;
        set => SetProperty(ref _histogramActive, value);
    }

    private BackendLifecycle? _backendLifecycle;

    private BackendLifecycle Lifecycle => _backendLifecycle ??= new BackendLifecycle(work => Task.Run(work), PostToUi);

    /// <summary>True while the backend is being shut down in the background; processing requests are refused meanwhile (GUI-C-186e).</summary>
    public bool IsBackendTransitioning => Lifecycle.IsTransitioning;

    /// <summary>
    /// GUI-C-186e (Codex #33): ends the backend WITHOUT making the UI thread wait. The shutdown waits for the AI session gate
    /// (a frame that waits on a silent worker holds it for up to the module's time budget), so it runs in the background; this
    /// method only starts it and shows "shutting down". The screen is brought up to date when it is done, and
    /// <paramref name="whenDone"/> (closing the window) runs then. A second call while one is running starts nothing.
    /// </summary>
    public void BeginShutdown(Action? whenDone = null)
    {
        var backend = _backend;
        var started = Lifecycle.Begin(() => backend.Shutdown(), error => FinishShutdown(backend, error));
        if (started)
        {
            AiStatus.Reset(); // the status shown belongs to a session that is ending; the read after the shutdown says what is left
            StatusText = "Backend shutting down...";
            Log("Backend shutdown requested.");
            OnPropertyChanged(nameof(IsBackendTransitioning));
        }
        else
        {
            Log("Backend shutdown is already in progress.");
        }

        if (whenDone is not null)
        {
            Lifecycle.WhenIdle(whenDone);
        }
    }

    private void FinishShutdown(IXpeBackend backend, Exception? error)
    {
        OnPropertyChanged(nameof(IsBackendTransitioning));
        if (ReferenceEquals(backend, _backend))
        {
            RuntimeInfo = _backend.GetRuntimeInfo();
            DrainBackendTelemetry();
            RefreshAiWorkerStatus();
        }

        StatusText = error is null ? "Backend shutdown." : $"Backend shutdown failed: {error.Message}";
        Log(error is null ? "Backend shutdown completed." : StatusText);
    }

    /// <summary>
    /// The application is ending without a window close that could wait (Application.Shutdown from the automation self-run,
    /// or a session end): the one place the shutdown still runs on the calling thread, because nothing is left to keep responsive.
    /// </summary>
    public void ShutdownBackendBlocking()
    {
        if (Lifecycle.IsTransitioning)
        {
            return; // the background shutdown is already doing it
        }

        _backend.Shutdown();
        RuntimeInfo = _backend.GetRuntimeInfo();
        DrainBackendTelemetry();
    }

    /// <summary>A processing request while the backend is shutting down is refused with a line, not queued behind it.</summary>
    private bool RefusedWhileTransitioning(string what)
    {
        if (Lifecycle.TryAdmit(out var refusal))
        {
            return false;
        }

        StatusText = refusal!;
        Log($"{what}: {refusal}");
        return true;
    }

    /// <summary>
    /// Requests <paramref name="mode"/> and re-initialises. A request for the mode already running is
    /// still honoured — re-initialising is what the Backend menu's Initialize item does too — so the
    /// command never silently does nothing.
    /// </summary>
    private void SetBackendMode(string mode)
    {
        if (string.IsNullOrWhiteSpace(mode))
        {
            return;
        }

        var requested = BackendModeOptions.FirstOrDefault(
            option => string.Equals(option, mode, StringComparison.OrdinalIgnoreCase));
        if (requested is null)
        {
            Log($"Backend mode '{mode}' is not one of {string.Join(", ", BackendModeOptions)}; ignored.");
            return;
        }

        Log($"Backend mode requested: {requested}.");
        Settings.BackendMode = requested;
        InitializeBackend();
    }

    /// <summary>
    /// Runs the row-6 probes. Each is caught on its own so one missing library does not hide the state
    /// of the others, and the outcome of every probe is written to the log rather than summarised away.
    /// </summary>
    private void RunPInvokeSmokeTest()
    {
        var outcomes = new List<string>();
        var failed = 0;

        // Probe 1: allocate and release a buffer through xpe_common. Chosen because it both crosses the
        // ABI and returns something checkable — a code AND a non-null pointer.
        try
        {
            var code = XpeCommonNative.xpe_alloc_image(16, 16, XpePixelFormatNative.UInt16, out var buffer);
            if (code != 0)
            {
                failed++;
                outcomes.Add($"xpe_alloc_image -> code {code} (expected 0)");
            }
            else if (buffer.Data == IntPtr.Zero)
            {
                failed++;
                outcomes.Add("xpe_alloc_image -> code 0 but a null data pointer");
                XpeCommonNative.xpe_free_image(ref buffer);
            }
            else
            {
                var freeCode = XpeCommonNative.xpe_free_image(ref buffer);
                if (freeCode != 0)
                {
                    failed++;
                    outcomes.Add($"xpe_free_image -> code {freeCode} (expected 0)");
                }
                else
                {
                    outcomes.Add("xpe_alloc_image/xpe_free_image -> ok");
                }
            }
        }
        catch (Exception ex)
        {
            failed++;
            outcomes.Add($"xpe_alloc_image threw {ex.GetType().Name}: {ex.Message}");
        }

        // Probe 2: the display library answers with a version string, and it is not the mock's.
        try
        {
            var version = XpeDisplayNative.GetVersion();
            if (string.IsNullOrWhiteSpace(version) || version == "unknown")
            {
                failed++;
                outcomes.Add($"xpe_display_version -> '{version}'");
            }
            else
            {
                outcomes.Add($"xpe_display_version -> '{version}'");
            }
        }
        catch (Exception ex)
        {
            failed++;
            outcomes.Add($"xpe_display_version threw {ex.GetType().Name}: {ex.Message}");
        }

        // Probe 3: the alert queue answers. A negative count would mean the call crossed but the
        // contract did not hold, which is a different failure from the library being absent.
        try
        {
            var count = XpeCommonNative.xpe_get_pending_alert_count();
            if (count < 0)
            {
                failed++;
                outcomes.Add($"xpe_get_pending_alert_count -> {count} (expected >= 0)");
            }
            else
            {
                outcomes.Add($"xpe_get_pending_alert_count -> {count}");
            }
        }
        catch (Exception ex)
        {
            failed++;
            outcomes.Add($"xpe_get_pending_alert_count threw {ex.GetType().Name}: {ex.Message}");
        }

        var total = outcomes.Count;
        PInvokeSmokeTestPassed = failed == 0;
        PInvokeSmokeTestDetail = string.Join("; ", outcomes);
        StatusText = failed == 0
            ? $"P/Invoke smoke: {total} of {total} probes answered."
            : $"P/Invoke smoke FAILED: {failed} of {total} probes did not answer.";

        Log($"P/Invoke smoke ({(failed == 0 ? "PASS" : "FAIL")}): {string.Join("; ", outcomes)}");
    }

    /// <summary>
    /// Whether the last <see cref="RunPInvokeSmokeTestCommand"/> run passed; null before it has run.
    /// Exposed so the automation report and the E2E scenarios read the SAME value the user sees rather
    /// than re-deriving it.
    /// </summary>
    public bool? PInvokeSmokeTestPassed
    {
        get => _pInvokeSmokeTestPassed;
        private set => SetProperty(ref _pInvokeSmokeTestPassed, value);
    }

    /// <summary>
    /// Per-probe outcomes of the last smoke run, in the order they ran; null before it has run.
    ///
    /// <para>Carried alongside the boolean because a bare <c>false</c> makes the next reader re-run the
    /// thing to learn which probe failed — and the run that produced the false may not be reproducible
    /// (it was launched with a pinned, empty native directory).</para>
    /// </summary>
    public string? PInvokeSmokeTestDetail
    {
        get => _pInvokeSmokeTestDetail;
        private set => SetProperty(ref _pInvokeSmokeTestDetail, value);
    }

    /// <summary>
    /// Cancels the render in flight, or reports that there is none.
    /// </summary>
    private void StopProcessing()
    {
        var cancellation = _renderCancellation;
        if (cancellation is null || cancellation.IsCancellationRequested)
        {
            StatusText = "Stop: no render is in flight.";
            Log("Stop processing: nothing was running.");
            return;
        }

        cancellation.Cancel();
        StatusText = "Stop requested; the render in flight will be discarded.";
        Log("Stop processing: cancellation requested for the render in flight.");
    }

    /// <summary>
    /// How many renders have been discarded by <see cref="StopProcessingCommand"/> in this session.
    /// Read by the automation report so "it stopped" is a number rather than a claim.
    /// </summary>
    public int StoppedRenderCount
    {
        get => _stoppedRenderCount;
        private set => SetProperty(ref _stoppedRenderCount, value);
    }

    /// <summary>
    /// Surfaces the last render's own timing strings.
    /// </summary>
    private void ShowStageTiming()
    {
        var haveStage = !string.IsNullOrWhiteSpace(ChainStatus);
        var haveTotals = !string.IsNullOrWhiteSpace(PipelineTimings);

        if (!haveStage && !haveTotals)
        {
            StatusText = "Stage timing: no render has run yet.";
            Log("Stage timing: nothing to show — no render has run in this session.");
            return;
        }

        var parts = new List<string>();
        if (haveTotals)
        {
            parts.Add(PipelineTimings);
        }

        if (haveStage)
        {
            parts.Add(ChainStatus);
        }

        LastStageTimingReport = string.Join(" || ", parts);
        StatusText = $"Stage timing: {LastStageTimingReport}";
        Log($"Stage timing (from the last render, not re-measured): {LastStageTimingReport}");
    }

    /// <summary>
    /// What <see cref="ShowStageTimingCommand"/> last reported; null before it has run. Recorded so a
    /// test can compare it against the status bar's own values rather than re-deriving them.
    /// </summary>
    public string? LastStageTimingReport
    {
        get => _lastStageTimingReport;
        private set => SetProperty(ref _lastStageTimingReport, value);
    }

    /// <summary>Runs the self-check runner off the UI thread and reports what it said.</summary>
    private async Task RunSelfCheckAsync()
    {
        if (SelfCheckRunning)
        {
            StatusText = "Self-check is already running.";
            return;
        }

        SelfCheckRunning = true;
        SelfCheckPassed = null;
        try
        {
            var verdict = await RunConsoleRunnerAsync(
                "Self-check", "ImageProcTest.SelfCheck", "ImageProcTest.SelfCheck.exe",
                App.AutomationSelfCheckExePath);
            if (verdict.HasValue)
            {
                SelfCheckPassed = verdict;
            }
        }
        finally
        {
            SelfCheckRunning = false;
        }
    }

    /// <summary>
    /// #225 row 16 (GUI-C-159): runs the GUI E2E runner off the UI thread and reports what it said.
    ///
    /// <para>Measured before it was wired (GUI-C-159 §2), because GUI-C-157 could not: with the operator
    /// app already up the runner still finished (4.03 s / 3.04 s over two samples, against 4.07 s / 4.08 s
    /// with nothing else running) and exited 0, and the operator's window survived. It cannot attach to
    /// the wrong window: it builds its own <c>MainWindow</c> on its own STA thread inside its own process
    /// rather than searching the desktop for one — the failure shape #228 was is structurally absent
    /// here.</para>
    /// </summary>
    private async Task RunGuiE2EAsync()
    {
        if (GuiE2ERunning)
        {
            StatusText = "GUI E2E is already running.";
            return;
        }

        GuiE2ERunning = true;
        GuiE2EPassed = null;
        try
        {
            var verdict = await RunConsoleRunnerAsync(
                "GUI E2E", "ImageProcTest.E2E", "ImageProcTest.E2E.exe", overridePath: null);
            if (verdict.HasValue)
            {
                GuiE2EPassed = verdict;
            }
        }
        finally
        {
            GuiE2ERunning = false;
        }
    }

    /// <summary>
    /// #225 row 17 (GUI-C-176): runs the benchmark freeze tests the way CI does, or says why it cannot.
    ///
    /// <para><b>Same executor as rows 15 and 16.</b> The process is started by ExecuteRunnerAsync, the method
    /// <c>RunConsoleRunnerAsync</c> hands over to, so the status line, the log, the exit-code rule and the
    /// failure shape are theirs and not a copy (<c>BenchmarkRunnerServiceTests</c> asserts the coupling). That
    /// executor has no cancellation, so neither does this command; Stop Processing stops a render, not this run.</para>
    ///
    /// <para><b>What the verdict is.</b> ctest's exit code and its summary, as printed. The 3000 ms budget and
    /// every other number are asserted by the tests themselves; a second copy of them here would give the GUI
    /// a pass criterion that can disagree with CI's. The tests and the pattern are CI's
    /// (<see cref="BenchmarkRunnerService.TestPattern"/>).</para>
    ///
    /// <para><b>No build tree is a state, not a failure.</b> It answers "not built" with how to build, starts
    /// nothing, and leaves <see cref="BenchmarkPassed"/> null, which is "did not run" (as for the runners).
    /// Under automation everything except the launch runs: a CI machine must not start a multi-minute native
    /// benchmark because a check clicked a menu, and the suppression is recorded (as for rows 14 and 20).</para>
    /// </summary>
    private async Task RunBenchmarkAsync()
    {
        if (BenchmarkRunning)
        {
            StatusText = "Benchmark runner is already running.";
            return;
        }

        BenchmarkRunning = true;
        BenchmarkPassed = null;
        try
        {
            string repositoryRoot;
            try
            {
                repositoryRoot = GuiFixtureManifestService.FindRepositoryRoot(AppContext.BaseDirectory);
            }
            catch (InvalidOperationException ex)
            {
                StatusText = "Benchmark runner needs the repository; this build is not running from a checkout.";
                Log($"Benchmark runner not run: {ex.Message}");
                return;
            }

            var plan = BenchmarkRunnerService.Resolve(repositoryRoot);
            if (!plan.IsReady)
            {
                StatusText = plan.Message;
                Log(plan.Message);
                return;
            }

            if (App.IsAutomationMode)
            {
                BenchmarkLaunchSuppressed = true;
                StatusText = $"{plan.Message} (launch suppressed under automation)";
                Log(StatusText);
                return;
            }

            var verdict = await ExecuteRunnerAsync("Benchmark runner", "ctest", plan.Arguments,
                repositoryRoot, BenchmarkRunnerService.Summarize);
            if (verdict.HasValue)
            {
                BenchmarkPassed = verdict;
            }
        }
        finally
        {
            BenchmarkRunning = false;
        }
    }

    /// <summary>
    /// Resolves a console runner, runs it off the UI thread, and writes the verdict to the status bar.
    /// Returns the verdict, or null when the runner could not be reached at all (no checkout, not built)
    /// — that is "did not run", which is not the same as a failure and must not be recorded as one.
    /// </summary>
    private async Task<bool?> RunConsoleRunnerAsync(
        string label, string projectDirectory, string executableName, string? overridePath)
    {
        string exePath;
        if (!string.IsNullOrWhiteSpace(overridePath))
        {
            // Command-line override (#225, GUI-C-159). It exists so the FAILING path can be observed:
            // a scenario stages a copy of the runner outside the repository, where it genuinely dies,
            // and points this at it. Without it a test could only ever watch the self-check succeed,
            // which cannot separate "reports success correctly" from "reports everything as success".
            exePath = overridePath!;
        }
        else
        {
            try
            {
                var repositoryRoot = GuiFixtureManifestService.FindRepositoryRoot(AppContext.BaseDirectory);
                exePath = Path.Combine(
                    repositoryRoot, "gui", projectDirectory, "bin", "Debug", "net8.0-windows",
                    executableName);
            }
            catch (InvalidOperationException ex)
            {
                StatusText = $"{label} needs the repository; this build is not running from a checkout.";
                Log($"{label} not run: {ex.Message}");
                return null;
            }
        }

        if (!File.Exists(exePath))
        {
            StatusText = $"{label} runner is not built.";
            Log($"{label} not run: '{exePath}' does not exist. Build gui/{projectDirectory} first.");
            return null;
        }

        return await ExecuteRunnerAsync(label, exePath, arguments: null, workingDirectory: null, summarize: null);
    }

    /// <summary>
    /// The one place a runner process is started and its verdict written (#225 rows 15, 16 and 17). Rows 15 and
    /// 16 reach it through <c>RunConsoleRunnerAsync</c>, which finds their executable; row 17 reaches it directly
    /// because its executable is ctest, found on PATH, and what must exist is a build tree.
    /// <paramref name="summarize"/> picks the line the verdict quotes from stdout; null keeps the choice the
    /// self-check and E2E runners were measured with (GUI-C-158).
    ///
    /// <para>Returns null when nothing could be started (the executable is missing, ctest is not on PATH, the file
    /// is not a program), false when something ran and exited non-zero, true when it exited zero. The status line
    /// says "did not run" for the first and "FAILED (exit N)" for the second (GUI-C-177).</para>
    /// </summary>
    private async Task<bool?> ExecuteRunnerAsync(
        string label, string exePath, IReadOnlyList<string>? arguments, string? workingDirectory,
        Func<string, string?>? summarize)
    {
        StatusText = $"Running {label}…";
        Log($"{label} started: '{exePath}'.");

        try
        {
            var outcome = await Task.Run(() => RunnerProcess.Run(exePath, arguments, workingDirectory, summarize));

            // The wording, and with it the difference between "could not be started" and "ran and failed", is
            // RunnerProcess.Describe's: nothing here words an outcome. A start that failed has a null verdict,
            // "did not run", which is not a failure and is not recorded as one (GUI-C-177).
            StatusText = RunnerProcess.Describe(label, exePath, outcome, quotesSummary: summarize is not null);
            Log(outcome.Started
                ? $"{label} finished: exit={outcome.ExitCode}, {outcome.ElapsedMs:0} ms, reported line: {outcome.LastLine}"
                : $"{label} did not run: {outcome.StartError}");
            return outcome.Verdict;
        }
        catch (Exception ex)
        {
            // Reachable only AFTER the process started (RunnerProcess.Run returns a failed start as an outcome,
            // not as an exception): something ran and its output could not be read. That is not "did not run".
            StatusText = $"{label} ended abnormally: {ex.Message}";
            Log($"{label} ended abnormally: {ex.GetType().Name}: {ex.Message}");
            return false;
        }
    }

    /// <summary>True while the self-check child process is running.</summary>
    public bool SelfCheckRunning
    {
        get => _selfCheckRunning;
        private set => SetProperty(ref _selfCheckRunning, value);
    }

    /// <summary>Verdict of the last self-check run; null before one has finished.</summary>
    public bool? SelfCheckPassed
    {
        get => _selfCheckPassed;
        private set => SetProperty(ref _selfCheckPassed, value);
    }

    /// <summary>True while the benchmark ctest process is running (#225 row 17).</summary>
    public bool BenchmarkRunning
    {
        get => _benchmarkRunning;
        private set => SetProperty(ref _benchmarkRunning, value);
    }

    /// <summary>ctest's verdict of the last benchmark run; null before one has finished, and when nothing ran (#225 row 17).</summary>
    public bool? BenchmarkPassed
    {
        get => _benchmarkPassed;
        private set => SetProperty(ref _benchmarkPassed, value);
    }

    /// <summary>True when a run skipped the ctest launch because it is an automation run (#225 row 17).</summary>
    public bool BenchmarkLaunchSuppressed
    {
        get => _benchmarkLaunchSuppressed;
        private set => SetProperty(ref _benchmarkLaunchSuppressed, value);
    }

    /// <summary>True while the GUI E2E child process is running (#225 row 16).</summary>
    public bool GuiE2ERunning
    {
        get => _guiE2ERunning;
        private set => SetProperty(ref _guiE2ERunning, value);
    }

    /// <summary>Verdict of the last GUI E2E run; null before one has finished (#225 row 16).</summary>
    public bool? GuiE2EPassed
    {
        get => _guiE2EPassed;
        private set => SetProperty(ref _guiE2EPassed, value);
    }

    // @MX:NOTE: [AUTO] Replaces current backend via factory; disposes old backend if IDisposable; called from constructor and InitializeBackendCommand
    private void InitializeBackend()
    {
        // GUI-C-186e: replacing a backend while its shutdown is still waiting in the background would leave two owners of one
        // session. The replacement itself takes no AI gate (the backends are not IDisposable and Initialize does not touch
        // the session), so it stays synchronous; it just waits its turn.
        if (RefusedWhileTransitioning("Initialize backend"))
        {
            return;
        }

        // GUI-C-186d: the status on screen belongs to the backend being replaced. Raise the generation and show Unknown NOW, so a
        // read still running for the old backend cannot be applied; the read for the new one is requested after the
        // initialisation has finished, whether it worked or not (below).
        AiStatus.Reset();
        try
        {
            // #198 (GUI-C-126, lead decision): the record the user has seen is kept and a boundary is
            // written instead of clearing it.
            //
            // Why not clear: nothing says why the clear was there. It arrived in d5432d2 ("GUI 메뉴와
            // 오프라인 도움말 추가") with no stated reason, and #161 did not decide it — that card
            // worked AROUND it by moving what it said to after this call. One reason it might have had
            // is real: alerts from the previous backend could be read as describing the new one. A
            // separator satisfies that reading as well as the opposite one, so neither has to be proven.
            //
            // The drain cursors below are a different thing and are still reset: they are backend
            // state — the new backend's queue starts at 0 and must be read from 0.
            if (Logs.Count > 0 || Alerts.Count > 0)
            {
                Log("--- backend re-initialised ---");
            }

            _drainedBackendLogCount = 0;
            _drainedBackendAlertCount = 0;

            lock (_telemetryLock)
            {
                (_backend as IDisposable)?.Dispose();
                _backend = _backendFactory(Settings);
            }
            RuntimeInfo = _backend.Initialize(Settings);
            DrainBackendTelemetry();

            // #178 (GUI-C-89): every successful initialisation starts a new run set. The actual backend can
            // differ from the previous initialisation with the same request (the DLLs may have appeared or
            // gone), and one run id for both would let the later backend.json overwrite the earlier record.
            // A failed initialisation throws before this line and keeps the current run set.
            RunSet = new RunSetState();

            StatusText = $"Backend initialized: {_backend.GetVersion()}";
            Log($"Initialized backend '{RuntimeInfo.BackendName}' ({_backend.GetVersion()}).");
        }
        catch (Exception ex)
        {
            StatusText = $"Backend initialization failed: {ex.Message}";
            Log(StatusText);
        }

        RefreshAiWorkerStatus();
    }

    /// <summary>
    /// #225 row 1 (GUI-C-160): records a successfully loaded raw file at the head of the history and
    /// persists it immediately.
    ///
    /// <para>Written only on a load that SUCCEEDED — this sits after the frame is in hand, so a path
    /// that failed to load never enters the list. A history of files that cannot be opened is worse
    /// than none: every entry is an invitation to the same failure.</para>
    ///
    /// <para>Saved here rather than waiting for File -> Save Settings, because "persisted" is the
    /// feature; a list that only survives when the operator happens to save is not a history.</para>
    /// </summary>
    private void RememberRecentRawFile(string path)
    {
        const int capacity = 8;

        var history = Settings.RecentRawFiles
            .Where(entry => !string.Equals(entry, path, StringComparison.OrdinalIgnoreCase))
            .ToList();
        history.Insert(0, path);
        if (history.Count > capacity)
        {
            history.RemoveRange(capacity, history.Count - capacity);
        }

        Settings.RecentRawFiles = history;
        RefreshRecentRawFiles();

        try
        {
            _settingsService.Save(Settings);
        }
        catch (Exception ex)
        {
            // The image is loaded either way; losing the history entry must not look like a load failure.
            Log($"Recent-file history could not be persisted: {ex.GetType().Name}: {ex.Message}");
        }
    }

    private void RefreshRecentRawFiles()
    {
        RecentRawFiles.Clear();
        foreach (var path in Settings.RecentRawFiles)
        {
            RecentRawFiles.Add(new RecentRawFile(path, LoadRecentRawFileCommand));
        }

        OnPropertyChanged(nameof(HasRecentRawFiles));
    }

    /// <summary>The Open Recent submenu's entries, newest first (#225 row 1).</summary>
    public ObservableCollection<RecentRawFile> RecentRawFiles { get; } = new();

    /// <summary>Whether Open Recent has anything to offer; the menu item is disabled when it does not.</summary>
    public bool HasRecentRawFiles => RecentRawFiles.Count > 0;

    /// <summary>Loads one entry of the history (#225 row 1).</summary>
    public RelayCommand<string> LoadRecentRawFileCommand { get; }

    private async Task LoadRecentRawFileAsync(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        if (!File.Exists(path))
        {
            // Kept in the list rather than dropped: a file on a disconnected share comes back, and
            // silently removing the entry would make the operator think they never opened it.
            StatusText = $"Recent file is not available: {path}";
            Log(StatusText);
            return;
        }

        await LoadImageFromPathAsync(path, "recent raw");
    }

    private void SaveSettings()
    {
        _settingsService.Save(Settings);
        StatusText = "Settings saved.";
        Log($"Settings saved to '{_settingsService.FilePath}'.");
    }

    /// <summary>
    /// View → Reset Layout.
    ///
    /// <para><b>Five writes were removed here</b> (#165, GUI-C-67). This method used to set eight
    /// panel flags; measured, seven of them changed nothing a user could see, and five of those seven
    /// have no writer left at all now that their menu items are gone — they are permanently
    /// <c>true</c>, so assigning <c>true</c> to them was a no-op with a reassuring name. Removing
    /// them changes no observable behaviour, which is why it is safe; keeping them would keep the
    /// message "Layout reset." half false.</para>
    ///
    /// <para>What remains all does something: the Logs toggle drives the log region (GUI-C-65), the
    /// two panel flags drive the panels they are named for (GUI-C-170), and the comparison
    /// view really is restored. <b>If one of the removed panels is ever built, its flag belongs back
    /// in this list</b> — the reason it left was the missing panel, not the flag.</para>
    ///
    /// <para>GUI-C-170: the two panel flags are set to their DEFAULT (off), not to true. While no panel
    /// existed, true was harmless and "Layout reset." was half false; with a panel behind each flag, true
    /// would open two regions the shipped layout does not show.</para>
    /// </summary>
    private void ResetLayout()
    {
        Settings.ShowCalibrationPanel = false;
        ShowLogsPanel = true;
        Settings.ShowDisplayPanel = false;
        ResetComparisonView();
        StatusText = "Layout reset.";
        Log("Menu command: layout reset.");
    }

    private void ShowNativeDiagnostics()
    {
        StatusText = RuntimeInfo.DisplayDllDetected
            ? $"Display DLL detected at '{RuntimeInfo.DisplayDllPath}' ({RuntimeInfo.DisplayVersion})."
            : "Display DLL not detected; mock display pipeline remains active.";

        Log($"Native diagnostics: backend={RuntimeInfo.BackendName}, version={RuntimeInfo.Version}, state={RuntimeInfo.State}, commonDetected={RuntimeInfo.NativeDllDetected}, commonPath='{RuntimeInfo.NativeDllPath}', displayDetected={RuntimeInfo.DisplayDllDetected}, displayPath='{RuntimeInfo.DisplayDllPath}', displayVersion='{RuntimeInfo.DisplayVersion}'.");
    }

    /// <summary>
    /// Tools → Calibration Settings.
    ///
    /// <para><b>The panel this used to announce does not exist</b> (#165, measured in GUI-C-65: no
    /// visibility binding reads <see cref="AppSettings.ShowCalibrationPanel"/>, and the calibration directory
    /// settings have no markup at all). The command said "panel visible" anyway, so a reader of the
    /// log was told something that had not happened — worse than silence, because it is believed.</para>
    ///
    /// <para>The command stays: <c>MENU-001</c> §4 lists it and §9.1 marks it S0 · Always. What
    /// changes is only what it claims. The wording follows this repository's own precedent for a
    /// command whose implementation has not arrived — <c>"RunOnAllQueuedCommand: not implemented
    /// (Slice 7)."</c> — rather than inventing a screen, which GUI-C-64 stopped for the reason that
    /// a layout invented to satisfy a message would then be the design.</para>
    ///
    /// <para><b>#225 row 7 (GUI-C-170): the panel exists now</b>, so the command opens it — it sets the same
    /// persisted flag the View-menu toggle drives, and the message says only what happened.</para>
    /// </summary>
    private void ShowCalibrationSettings()
    {
        Settings.ShowCalibrationPanel = true;
        StatusText = "Calibration paths panel opened.";
        Log("Menu command: calibration settings — calibration paths panel opened.");
    }

    private void ShowFixtureManager()
    {
        var fixtureRoot = Path.Combine(AppContext.BaseDirectory, "fixtures", "gui-s0");
        StatusText = Directory.Exists(fixtureRoot)
            ? $"Fixture pack available: {fixtureRoot}"
            : $"Fixture pack missing: {fixtureRoot}";
        Log(StatusText);
    }

    /// <summary>
    /// #225 row 5 (GUI-C-160): writes the runtime log the app is already keeping in memory to a file
    /// under this run set's evidence directory, and reports the path.
    ///
    /// <para>Row 5's missing piece was named precisely: the list on screen was live, the way OUT of the
    /// process was not. This writes; it deliberately does not launch anything to view the file — the
    /// menu used to say "Open", and starting an external viewer from a medical-device GUI is a decision
    /// nobody has made. The header now says Export, which is what it does.</para>
    ///
    /// <para>The directory is the one the app already owns — <c>evidence/&lt;RunId&gt;</c>, created by
    /// <see cref="RecordVerdict"/> and zipped by the bundle export — so the log lands inside whatever a
    /// later bundle packages rather than in a second place nobody collects.</para>
    /// </summary>
    private void ExportRuntimeLogs()
    {
        // Same guard as RecordVerdict and the bundle export (#178): without a run id the path collapses
        // to evidence/ itself.
        if (string.IsNullOrWhiteSpace(RunSet.RunId))
        {
            StatusText = "Runtime logs not exported: no run set has started.";
            Log(StatusText);
            return;
        }

        if (Logs.Count == 0)
        {
            StatusText = "Runtime logs not exported: the log is empty.";
            return;
        }

        try
        {
            var directory = Path.Combine(AppContext.BaseDirectory, "evidence", RunSet.RunId);
            Directory.CreateDirectory(directory);
            var path = Path.Combine(directory, "runtime-log.txt");

            // Snapshot first: Logs is an ObservableCollection the UI thread mutates, and enumerating it
            // while a backend drain appends throws.
            var lines = Logs.ToArray();
            File.WriteAllLines(path, lines);

            StatusText = $"Runtime logs exported ({lines.Length} lines): {path}";
            LastRuntimeLogExportPath = path;
            Log($"Runtime logs written to '{path}'.");
        }
        catch (Exception ex)
        {
            StatusText = $"Runtime logs could not be exported: {ex.Message}";
            Log($"Runtime log export failed: {ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>
    /// Where <see cref="ExportRuntimeLogsCommand"/> last wrote; null before it has succeeded. Recorded
    /// so a test can open the file the app claims it wrote rather than trusting the status line.
    /// </summary>
    public string? LastRuntimeLogExportPath
    {
        get => _lastRuntimeLogExportPath;
        private set => SetProperty(ref _lastRuntimeLogExportPath, value);
    }

    private void ExportAutomationReport()
    {
        var reportPath = Path.Combine(AppContext.BaseDirectory, "menu-command-report.json");
        var report = new
        {
            generatedAt = DateTimeOffset.Now,
            backend = RuntimeInfo,
            // #175 HAZ-GUI-005 (3): what actually produced the results, not what was asked for.
            actualBackendMode = ActualBackendMode,
            mockBackend = IsMockBackend,
            requestedBackendMode = Settings.BackendMode,
            activeImageSummary = ActiveImageSummary,
            status = StatusText,
            settings = Settings,
            // #165 (GUI-C-68): five fields removed — runtime, rawSettings, imageSummary, metadata,
            // alerts. Their panels were replaced by the Evaluation Workbench and their menu items are
            // gone, so each was permanently true: the report said five panels were visible that do
            // not exist. The three that remain describe real state — logs drives the log region, and
            // the other two drive the panels they are named for (GUI-C-170; before that, the checkmark on
            // their disabled menu items). Removing was cheap
            // precisely because nothing consumes these fields yet; it gets expensive once something
            // does.
            visiblePanels = new
            {
                calibration = Settings.ShowCalibrationPanel,
                display = Settings.ShowDisplayPanel,
                logs = ShowLogsPanel
            },
            displayPipeline = new
            {
                applied = ActiveImageFrame?.DisplayPipelineApplied ?? false,
                summary = DisplayPipelineSummary,
                version = RuntimeInfo.DisplayVersion,
                mode = Settings.VoiLutMode,
                center = Settings.VoiWindowCenter,
                width = Settings.VoiWindowWidth,
                bodyPart = Settings.SelectedBodyPart,
                gsdf = Settings.GsdfEnabled
            },
            calibrationEvaluation = new
            {
                summary = CalibrationEvaluationSummary,
                offset = Settings.OffsetCorrectionMode,
                gain = Settings.GainCorrectionMode,
                defect = Settings.DefectCorrectionMode,
                ghost = Settings.GhostCorrectionMode,
                temperature = Settings.TemperatureCompensationMode,
                nonlinearity = Settings.NonlinearityCorrectionMode,
                binning = Settings.BinningCorrectionMode
            },
            processingChain = DescribeChain(),
            comparison = new
            {
                mode = Settings.ComparisonMode,
                zoomScale = Settings.ComparisonZoomScale,
                panX = Settings.ComparisonPanX,
                panY = Settings.ComparisonPanY,
                swipePosition = Settings.ComparisonSwipePosition,
                overlayOpacity = Settings.ComparisonOverlayOpacity,
                sourceLayerPresent = SourceImage is not null,
                processedLayerPresent = ProcessedImage is not null,
                sourcePreserved = ActiveImageFrame?.Preview is not null && ReferenceEquals(SourceImage, ActiveImageFrame.Preview)
            },
            logCount = Logs.Count,
            alertCount = Alerts.Count
        };

        File.WriteAllText(reportPath, JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
        StatusText = $"Automation report exported: {reportPath}";
        Log(StatusText);
    }

    private void BrowseCalibrationDirectory(CalibrationPathKind kind)
    {
        using var dialog = new FormsFolderBrowserDialog
        {
            Description = "Select calibration directory",
            UseDescriptionForTitle = true,
            InitialDirectory = kind switch
            {
                CalibrationPathKind.Offset => Settings.OffsetCalibrationDirectory,
                CalibrationPathKind.Gain => Settings.GainCalibrationDirectory,
                CalibrationPathKind.Defect => Settings.DefectCalibrationDirectory,
                _ => string.Empty
            }
        };

        if (dialog.ShowDialog() != FormsDialogResult.OK)
        {
            return;
        }

        switch (kind)
        {
            case CalibrationPathKind.Offset:
                Settings.OffsetCalibrationDirectory = dialog.SelectedPath;
                break;
            case CalibrationPathKind.Gain:
                Settings.GainCalibrationDirectory = dialog.SelectedPath;
                break;
            case CalibrationPathKind.Defect:
                Settings.DefectCalibrationDirectory = dialog.SelectedPath;
                break;
        }

        Log($"{kind} calibration directory set to '{dialog.SelectedPath}'.");
    }

    // @MX:WARN: [AUTO] async void; unhandled exceptions escape the WPF dispatcher and crash the application
    // @MX:REASON: Bound to RelayCommand which cannot propagate async Task; inner try/catch is the only exception boundary
    private async void LoadImage()
    {
        try
        {
            if (!string.IsNullOrWhiteSpace(App.AutomationRawPath) && File.Exists(App.AutomationRawPath))
            {
                await LoadImageFromPathAsync(App.AutomationRawPath, "automation raw image");
                return;
            }

            var automationPath = Environment.GetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH");
            if (!string.IsNullOrWhiteSpace(automationPath) && File.Exists(automationPath))
            {
                await LoadImageFromPathAsync(automationPath, "automation raw image");
                return;
            }

            var dialog = new Win32OpenFileDialog
            {
                Title = "Load Raw Image",
                Filter = "Raw Files (*.raw)|*.raw|All Files (*.*)|*.*",
                CheckFileExists = true
            };

            if (dialog.ShowDialog() != true)
            {
                return;
            }

            await LoadImageFromPathAsync(dialog.FileName, "raw image");
        }
        catch (Exception ex)
        {
            StatusText = $"Load failed: {ex.Message}";
            Log(StatusText);
            RaiseAlert(new AlertEntry
            {
                Severity = "ERROR",
                Code = "LOAD_FAILED",
                Message = ex.Message,
                Timestamp = DateTimeOffset.Now
            });
        }
    }

    private async Task LoadImageFromPathAsync(string path, string sourceLabel)
    {
        Settings.LastRawDirectory = Path.GetDirectoryName(path) ?? string.Empty;
        var loadedFrame = _backend.LoadRawImage(path, Settings);
        DrainBackendTelemetry();

        SourceImage = loadedFrame.Preview;
        ProcessedImage = loadedFrame.ProcessedPreview ?? loadedFrame.Preview;
        PreviewStaleReason = null;   // a new image replaces whatever was stale
        SetRenderedVoi(null);        // not a display-pipeline render yet
        ActiveImageFrame = loadedFrame;
        ResetComparisonView();
        ActiveImageSummary = loadedFrame.Summary;
        MetadataText = loadedFrame.MetadataText;
        StatusText = $"Loaded {sourceLabel} '{path}'.";
        Log($"Loaded {sourceLabel} '{path}'.");
        RememberRecentRawFile(path);
        await ApplyDisplayPipelineAsync();
    }

    // @MX:NOTE: [AUTO] Display pipeline runs on Task.Run (thread pool); await resumes on dispatcher thread, so ObservableCollection writes are safe
    private async Task ApplyDisplayPipelineAsync()
    {
        if (RefusedWhileTransitioning("Display pipeline"))
        {
            return;
        }

        if (ActiveImageFrame is null)
        {
            StatusText = "Display pipeline requires a loaded raw image.";
            Log(StatusText);
            RaiseAlert(new AlertEntry
            {
                Severity = "WARN",
                Code = "DISPLAY_NO_IMAGE",
                Message = StatusText,
                Timestamp = DateTimeOffset.Now
            });
            return;
        }

        CancellationTokenSource? cancellationMarker = null;
        StatusText = "Applying display pipeline...";
        Log($"Display pipeline requested: mode={Settings.VoiLutMode}, bodyPart={Settings.SelectedBodyPart}, center={Settings.VoiWindowCenter}, width={Settings.VoiWindowWidth}, GSDF={Settings.GsdfEnabled}, calibrationEval=[{CalibrationEvaluationSummary}].");

        try
        {
            var sourceFrame = ActiveImageFrame;
            var inputs = Settings.Snapshot();
            // #180 (GUI-C-103): where the time to a drawn frame goes. `work` covers the background
            // task (chain + display); `total` covers this method, so the difference is the UI-thread
            // share. Neither covers the render itself — that is measured from outside, by the E2E.
            var total = System.Diagnostics.Stopwatch.StartNew();
            var work = System.Diagnostics.Stopwatch.StartNew();
            // #225 (GUI-C-154, row 11): one cancellation source per render, replaced rather than
            // reused so a stop can only ever affect the render it was pressed during.
            var cancellation = new CancellationTokenSource();
            _renderCancellation?.Dispose();
            _renderCancellation = cancellation;
            cancellationMarker = cancellation;
            // Measured (GUI-C-154): without clearing this when the render ENDS, Stop answered
            // "the render in flight will be discarded" on a render that had finished seconds earlier —
            // the source stayed non-null, so the no-op path never ran. The marker is cleared in the
            // finally below, and only when it is still this render's own source.

            // #180 (GUI-C-99): the chain runs first, on the same snapshot as the display (#171 ②), and the
            // display starts from the chain's last result — the raw frame only when no stage produced pixels.
            var (chain, processedFrame) = await Task.Run(() =>
            {
                var chainResult = _backend.RunChain(sourceFrame, ProcessingChainPlan.BuildStages(inputs), inputs);
                return (chainResult, _backend.ApplyDisplayPipeline(sourceFrame, chainResult.DisplayInput, inputs));
            });
            var workMs = work.Elapsed.TotalMilliseconds;

            // The native calls above are synchronous and ran to completion; what cancellation decides is
            // whether their result is APPLIED. Discarding it here keeps the viewport and the status
            // describing the frame that is actually on screen.
            if (cancellation.IsCancellationRequested)
            {
                StoppedRenderCount++;
                PreviewStaleReason = StalePipelineFailed;
                StatusText = $"Render stopped; the result was discarded after {workMs:0} ms of work.";
                Log($"Display pipeline stopped by the user after {workMs:0} ms; result discarded.");
                DrainBackendTelemetry();
                return;
            }

            DrainBackendTelemetry();
            ReportChain(chain);

            ActiveImageFrame = processedFrame;
            ProcessedImage = processedFrame.ProcessedPreview ?? processedFrame.Preview;
            PreviewStaleReason = null;   // #171 ③: this render is current; ① is re-evaluated just below
            SetRenderedVoi(inputs);
            MetadataText = processedFrame.MetadataText;
            DisplayPipelineSummary = processedFrame.DisplayPipelineSummary;
            ActiveImageSummary = processedFrame.DisplayPipelineApplied
                ? $"{processedFrame.Summary} | {processedFrame.DisplayPipelineSummary}"
                : processedFrame.Summary;
            StatusText = $"{chain.Summary} | {processedFrame.DisplayPipelineSummary}";
            await RenderLanesAsync(sourceFrame, inputs, ProcessedImage);
            PipelineTimings = string.Join("; ", new[]
            {
                $"work={workMs:0} ms",
                $"vm={total.Elapsed.TotalMilliseconds - workMs:0} ms",
                processedFrame.DisplayTimings,
            }.Where(part => !string.IsNullOrWhiteSpace(part)));
            OnPropertyChanged(nameof(FaultInjectionStatus));
        }
        catch (Exception ex)
        {
            OnPropertyChanged(nameof(FaultInjectionStatus));
            PreviewStaleReason = StalePipelineFailed;   // #171 ③
            StatusText = $"Display pipeline failed: {ex.Message}";
            Log(StatusText);
            RaiseAlert(new AlertEntry
            {
                Severity = "ERROR",
                Code = "DISPLAY_PIPELINE_FAILED",
                Message = ex.Message,
                Timestamp = DateTimeOffset.Now
            });
        }
        finally
        {
            if (ReferenceEquals(_renderCancellation, cancellationMarker))
            {
                _renderCancellation = null;
            }
        }
    }

    /// <summary>
    /// The preset the active backend last produced, kept so a caller can check that it was applied
    /// without knowing which backend produced it. #135: the automation harness used to compare the
    /// settings against MockXpeBackend's literal values, which made the check fail under the real
    /// DLL — whose abdomen window (C=40/W=400, HU) is the clinically validated one.
    /// </summary>
    public VoiPreset? LastAppliedVoiPreset
    {
        get => _lastAppliedVoiPreset;
        private set
        {
            if (SetProperty(ref _lastAppliedVoiPreset, value))
            {
                OnPropertyChanged(nameof(LastAppliedVoiPresetSummary));
            }
        }
    }

    private VoiPreset? _lastAppliedVoiPreset;

    /// <summary>
    /// #225 row 8 (GUI-C-170): the preset line of the Display Settings panel. Derived from
    /// <see cref="LastAppliedVoiPreset"/> — nothing stored — and it says "none" rather than printing a zero
    /// window for a preset that was never applied. The property gained change notification for this: it was
    /// an auto-property, so a panel bound to it would have shown whatever it held when the panel was built.
    /// </summary>
    public string LastAppliedVoiPresetSummary => LastAppliedVoiPreset is { } p
        ? $"{p.Mode} · C={p.Center:0.###} · W={p.Width:0.###}"
        : "none applied yet";

    /// <summary>
    /// #141 / #180 (GUI-C-99): Phase-1a preprocessing is a stage of the pixel chain. The menu entry switches
    /// the stage on and renders again; the corrected pixels feed the display pipeline.
    /// </summary>
    private async void RunPreprocessing()
    {
        if (ActiveImageFrame is null)
        {
            StatusText = "Load a raw image before running preprocessing.";
            Log(StatusText);
            return;
        }

        try
        {
            Settings.PreprocessInChain = true;
            await ApplyDisplayPipelineAsync();
        }
        catch (Exception ex)
        {
            StatusText = $"Preprocessing failed: {ex.Message}";
            Log(StatusText);
        }
    }

    /// <summary>#225 row 10 (GUI-C-184): AI bone suppression is a stage of the pixel chain; the menu entry switches it on and renders again.</summary>
    private async void RunAiBoneSuppression()
    {
        if (ActiveImageFrame is null)
        {
            StatusText = "Load a raw image before running AI bone suppression.";
            Log(StatusText);
            return;
        }

        try
        {
            Settings.AiBoneSuppressionInChain = true;
            await ApplyDisplayPipelineAsync();
        }
        catch (Exception ex)
        {
            StatusText = $"AI bone suppression failed: {ex.Message}";
            Log(StatusText);
        }
    }

    /// <summary>
    /// Publishes a chain result (#180, GUI-C-99): status bar summary, the preprocess fields the automation
    /// report reads, a log line per stage, and an alert for every requested stage that did not apply. A
    /// refusal is an expected state (no calibration, Mock backend), so it is a WARN, not an exception.
    /// </summary>
    private void ReportChain(ChainResult chain)
    {
        LastChain = chain;
        // The reason of a stage that did not apply belongs on screen: "RequestedNotApplied" alone sends
        // the operator to the log to find out why (#180, GUI-C-101).
        var refused = chain.Stages
            .Where(st => st.Status == StageStatus.RequestedNotApplied)
            .Select(st => $"{st.StageId}: {st.Reason}")
            .ToArray();
        // #225 row 10 (GUI-C-184): the label is derived from the stage's STATUS, which the chain derives from the module's
        // return code and a pixel comparison. It is not read from an alert or from a message, and a refused, failed or
        // unchanged AI stage never gets it.
        AiProcessedLabel = AiBoneSuppressionStage.LabelFor(chain);
        OnPropertyChanged(nameof(AiProcessedLabel));
        RefreshAiWorkerStatus();
        ChainStatus = $"{chain.Summary}; {chain.Timings}; display input={(chain.DisplaysRaw ? "raw" : "chain")}"
            + (AiProcessedLabel.Length == 0 ? string.Empty : $" — {AiProcessedLabel}")
            + (refused.Length == 0 ? string.Empty : " — " + string.Join(" | ", refused));

        foreach (var stage in chain.Stages)
        {
            Log($"Chain {stage.StageId}: {stage.Status} — {stage.Reason}");

            if (stage.StageId == StageIds.Preprocess && stage.Status != StageStatus.NotRequested)
            {
                PreprocessRan = stage.Status is StageStatus.Applied or StageStatus.AppliedNoChange;
                PreprocessStages = stage.Reason;
            }

            if (stage.Status == StageStatus.RequestedNotApplied)
            {
                RaiseAlert(new AlertEntry
                {
                    Severity = "WARN",
                    Code = stage.StageId == StageIds.Preprocess ? "PREPROCESS_NOT_RUN" : "CHAIN_STAGE_NOT_APPLIED",
                    Message = $"{stage.StageId} was requested and not applied; the display used its input. {stage.Reason}",
                    Timestamp = DateTimeOffset.Now,
                });
            }
        }
    }

    // @MX:WARN: [AUTO] async void; same crash risk as LoadImage; inner try/catch is the only safety net
    // @MX:REASON: Bound to RelayCommand; must remain async void for command infrastructure compatibility
    private async void ApplyBodyPartPreset()
    {
        if (!Enum.TryParse<XpeBodyPartEnum>(Settings.SelectedBodyPart, ignoreCase: true, out var bodyPart))
        {
            bodyPart = XpeBodyPartEnum.Abdomen;
            Settings.SelectedBodyPart = nameof(XpeBodyPartEnum.Abdomen);
        }

        try
        {
            var preset = _backend.CreateVoiPreset(bodyPart);
            LastAppliedVoiPreset = preset;
            Settings.VoiWindowCenter = preset.Center;
            Settings.VoiWindowWidth = preset.Width;
            Settings.VoiLutMode = preset.Mode;
            DrainBackendTelemetry();

            StatusText = $"Applied {bodyPart} VOI preset: C={preset.Center:0.###}, W={preset.Width:0.###}.";
            Log(StatusText);
            await ApplyDisplayPipelineAsync();
        }
        catch (Exception ex)
        {
            StatusText = $"VOI preset failed: {ex.Message}";
            Log(StatusText);
            RaiseAlert(new AlertEntry
            {
                Severity = "ERROR",
                Code = "VOI_PRESET_FAILED",
                Message = ex.Message,
                Timestamp = DateTimeOffset.Now
            });
        }
    }

    private void ZoomFit()
    {
        Settings.ComparisonZoomScale = 0.0;
        Settings.ComparisonPanX = 0.0;
        Settings.ComparisonPanY = 0.0;
        RefreshComparisonStatus("Comparison viewport reset to fit.");
    }

    private void ZoomActual()
    {
        Settings.ComparisonZoomScale = 1.0;
        Settings.ComparisonPanX = 0.0;
        Settings.ComparisonPanY = 0.0;
        RefreshComparisonStatus("Comparison viewport set to 100%.");
    }

    private void ZoomIn()
    {
        var current = Settings.ComparisonZoomScale <= 0.0 ? 1.0 : Settings.ComparisonZoomScale;
        Settings.ComparisonZoomScale = Math.Min(16.0, current * 1.25);
        RefreshComparisonStatus("Comparison viewport zoomed in.");
    }

    private void ZoomOut()
    {
        var current = Settings.ComparisonZoomScale <= 0.0 ? 1.0 : Settings.ComparisonZoomScale;
        Settings.ComparisonZoomScale = Math.Max(0.05, current / 1.25);
        RefreshComparisonStatus("Comparison viewport zoomed out.");
    }

    /// <summary>
    /// Says out loud that a stored comparison mode was not one we support (#161, GUI-C-60).
    ///
    /// <para>The alternative was to replace it silently, and this repository has twice decided
    /// against that shape: #150 refuses a value rather than truncating it, and QA-B-60 names the key
    /// it could not read. A user whose settings file says <c>"NotAMode"</c> would otherwise see the
    /// default mode, assume the file was ignored, and write it again.</para>
    ///
    /// <para><b>The file is not rewritten.</b> Fixing someone's settings file at start-up, without
    /// being asked, is a larger act than reporting it — so the message repeats on every launch until
    /// the file is corrected or the user saves settings. That repetition is the cost, and it is the
    /// honest one: the file IS still wrong.</para>
    /// </summary>
    private void ReportRejectedComparisonMode()
    {
        var rejected = Settings.RejectedComparisonMode;
        if (rejected is null) return;

        Log(
            $"appsettings.json: comparisonMode '{rejected}' is not a supported comparison mode; " +
            $"using '{Settings.ComparisonMode}'. Supported: {string.Join(", ", ComparisonModes.All)}.");
    }

    private void SetComparisonMode(string? mode)
    {
        if (!ComparisonModes.IsKnown(mode)) return;

        Settings.ComparisonMode = mode;
    }

    private void ResetComparisonView()
    {
        Settings.ComparisonMode = ComparisonModes.Default;
        Settings.ComparisonZoomScale = 0.0;
        Settings.ComparisonPanX = 0.0;
        Settings.ComparisonPanY = 0.0;
        Settings.ComparisonSwipePosition = 0.5;
        Settings.ComparisonOverlayOpacity = 0.5;
        OnPropertyChanged(nameof(ComparisonStatus));
    }

    /// <summary>
    /// Opens the detached comparison viewer (#166, MENU-001 §4.3 "Detach Viewer").
    ///
    /// <para>Every binding carries its mode explicitly. Eight of them used to share one
    /// <c>TwoWay</c>, and the first target — <see cref="SourceImage"/>, whose setter is private —
    /// made <c>SetBinding</c> throw, so the window was never constructed (GUI-C-70 measured it).
    /// The images travel one way by design: §4.3 asks the viewer to stay in step with the source,
    /// not to be able to replace it.</para>
    ///
    /// <para>The failure path reports. Before #166 an exception here left the status bar on its
    /// previous value with nothing in the log, so pressing the command was indistinguishable from
    /// not pressing it.</para>
    /// </summary>
    private void OpenDetachedComparisonViewer()
    {
        try
        {
            OpenDetachedComparisonViewerCore();
        }
        catch (Exception ex)
        {
            // Say it on both surfaces the user actually reads. "Nothing happened" is the one
            // outcome this command must never produce again (#166).
            StatusText = $"Detached comparison viewer failed to open: {ex.GetType().Name}.";
            Log($"DetachComparisonViewerCommand failed: {ex.GetType().Name}: {ex.Message}");
        }
    }

    private void OpenDetachedComparisonViewerCore()
    {
        var viewport = new ImageComparisonViewport
        {
            Margin = new System.Windows.Thickness(12),
            MinWidth = 640,
            MinHeight = 480
        };
        BindDetachedViewport(viewport, ImageComparisonViewport.SourceImageProperty, nameof(SourceImage), BindingMode.OneWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.ProcessedImageProperty, nameof(ProcessedImage), BindingMode.OneWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.CompareModeProperty, "Settings.ComparisonMode", BindingMode.TwoWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.ZoomScaleProperty, "Settings.ComparisonZoomScale", BindingMode.TwoWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.PanXProperty, "Settings.ComparisonPanX", BindingMode.TwoWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.PanYProperty, "Settings.ComparisonPanY", BindingMode.TwoWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.SwipePositionProperty, "Settings.ComparisonSwipePosition", BindingMode.TwoWay);
        BindDetachedViewport(viewport, ImageComparisonViewport.OverlayOpacityProperty, "Settings.ComparisonOverlayOpacity", BindingMode.TwoWay);

        var status = new TextBlock
        {
            Margin = new System.Windows.Thickness(12, 0, 12, 12),
            Foreground = System.Windows.Media.Brushes.White
        };
        status.SetBinding(TextBlock.TextProperty, new DataBinding(nameof(ComparisonStatus)) { Source = this });

        var grid = new Grid
        {
            Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(15, 23, 42))
        };
        grid.RowDefinitions.Add(new RowDefinition { Height = new System.Windows.GridLength(1, System.Windows.GridUnitType.Star) });
        grid.RowDefinitions.Add(new RowDefinition { Height = System.Windows.GridLength.Auto });
        grid.Children.Add(viewport);

        // #171 (GUI-C-80): the detached viewer shows the same image, so it carries the same two truths as
        // the main shell — the window that rendered it (②) and why it is stale (①③). Both bind to the
        // view-model state the main shell reads; nothing is recomputed per window, so the two cannot drift.
        var hud = new TextBlock
        {
            Margin = new System.Windows.Thickness(24, 20, 0, 0),
            HorizontalAlignment = System.Windows.HorizontalAlignment.Left,
            VerticalAlignment = System.Windows.VerticalAlignment.Top,
            FontSize = 11,
            FontFamily = new System.Windows.Media.FontFamily("Consolas"),
            Foreground = System.Windows.Media.Brushes.White
        };
        System.Windows.Automation.AutomationProperties.SetAutomationId(hud, "DetachedHudVoiWindow");
        hud.Inlines.Add(new System.Windows.Documents.Run("C "));
        hud.Inlines.Add(BoundRun(nameof(RenderedVoiCenter), "{0:0}", "—"));
        hud.Inlines.Add(new System.Windows.Documents.Run(" · W "));
        hud.Inlines.Add(BoundRun(nameof(RenderedVoiWidth), "{0:0}", "—"));
        hud.Inlines.Add(new System.Windows.Documents.Run("  VOI: "));
        hud.Inlines.Add(BoundRun(nameof(RenderedVoiMode), null, "not applied"));
        grid.Children.Add(hud);

        var staleText = new TextBlock
        {
            Foreground = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(0xFF, 0xFB, 0xEB)),
            TextWrapping = System.Windows.TextWrapping.Wrap
        };
        System.Windows.Automation.AutomationProperties.SetAutomationId(staleText, "DetachedPreviewStaleIndicator");
        staleText.SetBinding(TextBlock.TextProperty, new DataBinding(nameof(PreviewStaleReason)) { Source = this, Mode = BindingMode.OneWay });
        var staleBanner = new Border
        {
            HorizontalAlignment = System.Windows.HorizontalAlignment.Center,
            VerticalAlignment = System.Windows.VerticalAlignment.Top,
            Margin = new System.Windows.Thickness(0, 20, 0, 0),
            Padding = new System.Windows.Thickness(10, 6, 10, 6),
            CornerRadius = new System.Windows.CornerRadius(6),
            Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromArgb(0xE6, 0xB4, 0x53, 0x09)),
            BorderBrush = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(0xF5, 0x9E, 0x0B)),
            BorderThickness = new System.Windows.Thickness(1),
            Child = staleText
        };
        staleBanner.SetBinding(System.Windows.UIElement.VisibilityProperty, new DataBinding(nameof(IsPreviewStale))
        {
            Source = this,
            Mode = BindingMode.OneWay,
            Converter = new System.Windows.Controls.BooleanToVisibilityConverter()
        });
        grid.Children.Add(staleBanner);

        // #175 HAZ-GUI-005 (1): the same Mock warning, bound to the same view-model state.
        var mockText = new TextBlock
        {
            Foreground = System.Windows.Media.Brushes.White,
            FontWeight = System.Windows.FontWeights.Bold,
            TextWrapping = System.Windows.TextWrapping.Wrap
        };
        System.Windows.Automation.AutomationProperties.SetAutomationId(mockText, "DetachedMockBackendBannerText");
        mockText.SetBinding(TextBlock.TextProperty, new DataBinding(nameof(MockBackendWarning)) { Source = this, Mode = BindingMode.OneWay });
        var mockBanner = new Border
        {
            VerticalAlignment = System.Windows.VerticalAlignment.Bottom,
            Margin = new System.Windows.Thickness(12, 0, 12, 12),
            Padding = new System.Windows.Thickness(10, 6, 10, 6),
            Background = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Color.FromRgb(0xB9, 0x1C, 0x1C)),
            Child = mockText
        };
        mockBanner.SetBinding(System.Windows.UIElement.VisibilityProperty, new DataBinding(nameof(IsMockBackend))
        {
            Source = this,
            Mode = BindingMode.OneWay,
            Converter = new System.Windows.Controls.BooleanToVisibilityConverter()
        });
        grid.Children.Add(mockBanner);

        Grid.SetRow(status, 1);
        grid.Children.Add(status);

        var window = new System.Windows.Window
        {
            Width = 1280,
            Height = 820,
            MinWidth = 900,
            MinHeight = 620,
            Content = grid
        };

        // #175 HAZ-GUI-005 (2): "[MOCK] " in front while Mock is active.
        window.SetBinding(System.Windows.Window.TitleProperty, new DataBinding(nameof(IsMockBackend))
        {
            Source = this,
            Mode = BindingMode.OneWay,
            Converter = new MockTitleConverter("ImageProcTest Comparison Viewer")
        });

        var owner = System.Windows.Application.Current.Windows.OfType<System.Windows.Window>().FirstOrDefault(w => w.IsActive);
        if (owner is not null && !ReferenceEquals(owner, window))
        {
            window.Owner = owner;
        }

        window.Show();
        RefreshComparisonStatus("Detached comparison viewer opened.");
    }

    private System.Windows.Documents.Run BoundRun(string path, string? format, string nullText)
    {
        var run = new System.Windows.Documents.Run();
        run.SetBinding(System.Windows.Documents.Run.TextProperty, new DataBinding(path)
        {
            Source = this,
            Mode = BindingMode.OneWay,
            StringFormat = format,
            TargetNullValue = nullText
        });
        return run;
    }

    private void BindDetachedViewport(
        System.Windows.DependencyObject target,
        System.Windows.DependencyProperty property,
        string path,
        BindingMode mode)
    {
        BindingOperations.SetBinding(target, property, new DataBinding(path)
        {
            Source = this,
            Mode = mode,
            UpdateSourceTrigger = UpdateSourceTrigger.PropertyChanged
        });
    }

    private void RefreshComparisonStatus(string message)
    {
        OnPropertyChanged(nameof(ComparisonStatus));
        StatusText = message;
        Log($"{message} {ComparisonStatus}");
    }

    private void OnSettingsPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        RefreshParametersStale();   // #171 ①

        // #173 (GUI-C-113): only the Candidate can go stale — the Reference never reads this value,
        // so an edit to it cannot make the Reference wrong.
        if (e.PropertyName is nameof(AppSettings.LaneBVoiWindowWidth)
            or nameof(AppSettings.LaneBAlgorithm)
            or nameof(AppSettings.LaneBGsvgDenoiseK))
        {
            OnPropertyChanged(nameof(LaneBIsStale));
        }

        // Whether the Candidate's de-noise k reaches anything depends on the Candidate's algorithm and
        // on the pyramid being on, so both have to re-raise it.
        if (e.PropertyName is nameof(AppSettings.LaneBAlgorithm) or nameof(AppSettings.GsvgPyramidLevels))
        {
            OnPropertyChanged(nameof(LaneBDenoiseKApplies));
            OnPropertyChanged(nameof(LaneBDenoiseKUnapplied));
        }

        if (e.PropertyName is nameof(AppSettings.BackendMode))
        {
            OnPropertyChanged(nameof(RuntimeVersionSummary));   // #175: the "requested" half can change alone
            RaiseBackendIdentityChanged();
        }

        if (e.PropertyName is nameof(AppSettings.ComparisonMode)
            or nameof(AppSettings.ComparisonZoomScale)
            or nameof(AppSettings.ComparisonPanX)
            or nameof(AppSettings.ComparisonPanY)
            or nameof(AppSettings.ComparisonSwipePosition)
            or nameof(AppSettings.ComparisonOverlayOpacity))
        {
            OnPropertyChanged(nameof(ComparisonStatus));
        }

        if (e.PropertyName is nameof(AppSettings.OffsetCorrectionMode)
            or nameof(AppSettings.GainCorrectionMode)
            or nameof(AppSettings.DefectCorrectionMode)
            or nameof(AppSettings.GhostCorrectionMode)
            or nameof(AppSettings.TemperatureCompensationMode)
            or nameof(AppSettings.NonlinearityCorrectionMode)
            or nameof(AppSettings.BinningCorrectionMode))
        {
            OnPropertyChanged(nameof(CalibrationEvaluationSummary));
        }
    }

    // @MX:ANCHOR: [AUTO] Drains backend log and alert queues into ObservableCollections; lock guards against concurrent backend calls
    // @MX:REASON: Called after every _backend operation; skipping leaves telemetry invisible in UI; fan_in >= 5 call sites
    private void DrainBackendTelemetry()
    {
        List<string> pendingLogs;
        List<AlertEntry> pendingAlerts;

        lock (_telemetryLock)
        {
            pendingLogs = new List<string>();
            var logCount = _backend.GetLogCount();
            for (var i = _drainedBackendLogCount; i < logCount; i++)
            {
                var log = _backend.GetLog(i);
                if (!string.IsNullOrWhiteSpace(log))
                    pendingLogs.Add(log);
            }
            _drainedBackendLogCount = logCount;

            pendingAlerts = new List<AlertEntry>();
            var alertCount = _backend.GetAlertCount();
            for (var i = _drainedBackendAlertCount; i < alertCount; i++)
            {
                var alert = _backend.GetAlert(i);
                if (alert is not null)
                    pendingAlerts.Add(alert);
            }
            _drainedBackendAlertCount = alertCount;
        }

        foreach (var log in pendingLogs)
            Logs.Insert(0, log);
        foreach (var alert in pendingAlerts)
            RaiseAlert(alert);
    }

    private void RecordVerdict(Verdict verdict)
    {
        ActiveVerdict = verdict;

        // #178: without a run id the path collapses to evidence/ itself; write nothing rather than that.
        if (string.IsNullOrWhiteSpace(RunSet.RunId))
        {
            Log("Verdict not written to evidence: no run set has started.");
            return;
        }

        var dir = Path.Combine(AppContext.BaseDirectory, "evidence", RunSet.RunId);
        Directory.CreateDirectory(dir);
        var path = Path.Combine(dir, "verdicts.json");

        var verdicts = new Dictionary<string, string>();
        if (File.Exists(path))
        {
            var existing = File.ReadAllText(path);
            var deserialized = JsonSerializer.Deserialize<Dictionary<string, string>>(existing);
            if (deserialized is not null)
                verdicts = deserialized;
        }

        if (!string.IsNullOrEmpty(ActiveStudyId))
            verdicts[ActiveStudyId] = $"{verdict}|{_verdictNotes}|{DateTimeOffset.Now:O}";

        // #175 HAZ-GUI-005 (3): the evidence bundle is this directory zipped, so the backend that produced
        // the judged images travels with the verdicts.
        try
        {
            File.WriteAllText(Path.Combine(dir, "backend.json"), JsonSerializer.Serialize(new
            {
                actualBackendMode = ActualBackendMode,
                mockBackend = IsMockBackend,
                requestedBackendMode = Settings.BackendMode,
                backendName = RuntimeInfo.BackendName,
                nativeSource = RuntimeInfo.NativeSource,
                processingChain = DescribeChain(),
                writtenAt = DateTimeOffset.Now,
            }, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception ex)
        {
            Log($"Failed to write backend.json: {ex.Message}");
        }

        try
        {
            File.WriteAllText(path, JsonSerializer.Serialize(verdicts, new JsonSerializerOptions { WriteIndented = true }));
            Log($"Verdict recorded: {ActiveStudyId} = {verdict}.");
        }
        catch (Exception ex)
        {
            Log($"Failed to write verdict file: {ex.Message}");
            RaiseAlert(new AlertEntry
            {
                Severity = "ERROR",
                Code = "VERDICT_WRITE_FAILED",
                Message = ex.Message,
                Timestamp = DateTimeOffset.Now
            });
        }
    }

    private void SaveAndNext()
    {
        var current = Studies.FirstOrDefault(s => s.Id == ActiveStudyId);
        if (current is not null && ActiveVerdict.HasValue)
        {
            current.Status = ActiveVerdict.Value switch
            {
                Verdict.Pass => StudyStatus.Pass,
                Verdict.Defer => StudyStatus.Defer,
                Verdict.Fail => StudyStatus.Fail,
                _ => StudyStatus.Queued
            };
        }

        var next = Studies.FirstOrDefault(s => s.Status == StudyStatus.Queued);
        ActiveStudyId = next?.Id ?? string.Empty;
        ActiveVerdict = null;
        VerdictNotes = string.Empty;

        RunSet.Passed = Studies.Count(s => s.Status == StudyStatus.Pass);
        RunSet.Failed = Studies.Count(s => s.Status == StudyStatus.Fail);
        RunSet.Deferred = Studies.Count(s => s.Status == StudyStatus.Defer);
        RunSet.Total = Studies.Count;

        StatusText = next is not null ? $"Advanced to study '{next.Id}'." : "No more queued studies.";
        Log(StatusText);
    }

    private void ResetLaneBOverrides()
    {
        LaneBGsvgDenoiseK = 2.0;
        StatusText = "Lane B overrides reset to defaults.";
        Log(StatusText);
    }

    /// <summary>
    /// #225 row 20 (GUI-C-169): opens the generated native (Doxygen) API reference, or says why it cannot.
    ///
    /// <para><b>Same boundary as row 14:</b> the path is computed from the repository root by
    /// <see cref="ApiReferenceService"/>, never taken from operator input, and existence is checked before
    /// anything is launched.</para>
    ///
    /// <para>The three answers are all real and all different: not generated (with how to generate it),
    /// generated part-way, opened (with when it was generated). None of them opens an empty window or does
    /// nothing, which are the two ways a documentation menu quietly lies.</para>
    /// </summary>
    private void OpenApiReference()
    {
        string repositoryRoot;
        try
        {
            repositoryRoot = GuiFixtureManifestService.FindRepositoryRoot(AppContext.BaseDirectory);
        }
        catch (InvalidOperationException ex)
        {
            // Same answer rows 15 and 16 give: this menu needs a checkout, and a build running from
            // somewhere else says so rather than being greyed out (which would make the enabled count
            // depend on where the app happens to run).
            StatusText = "API reference needs the repository; this build is not running from a checkout.";
            Log($"API reference not opened: {ex.Message}");
            return;
        }

        var status = ApiReferenceService.Resolve(repositoryRoot);
        LastApiReferencePath = status.IndexPath;
        if (!status.IsAvailable)
        {
            StatusText = status.Message;
            Log(status.Message);
            return;
        }

        // Automation is headless and unattended; a browser window left on a CI machine is not what a check
        // should do. Resolution and the existence check above DO run, so a scenario observes everything but
        // the launch, and the suppression is recorded in the report data (as for row 14).
        if (App.IsAutomationMode)
        {
            ApiReferenceLaunchSuppressed = true;
            StatusText = $"{status.Message} (launch suppressed under automation)";
            Log(StatusText);
            return;
        }

        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = status.IndexPath!,
                UseShellExecute = true
            });
            StatusText = status.Message;
            Log(status.Message);
        }
        catch (Exception ex)
        {
            StatusText = $"API reference could not be opened: {ex.Message}";
            Log($"API reference launch failed: {ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>The page <see cref="OpenApiReferenceCommand"/> resolved last; null when it had nothing to open.</summary>
    public string? LastApiReferencePath
    {
        get => _lastApiReferencePath;
        private set => SetProperty(ref _lastApiReferencePath, value);
    }

    /// <summary>True when a run suppressed the browser launch because it is an automation run.</summary>
    public bool ApiReferenceLaunchSuppressed
    {
        get => _apiReferenceLaunchSuppressed;
        private set => SetProperty(ref _apiReferenceLaunchSuppressed, value);
    }

    /// <summary>#225 row 20: opens the generated Doxygen API reference, or says how to generate it.</summary>
    public RelayCommand OpenApiReferenceCommand { get; }

    /// <summary>
    /// #225 row 21 (GUI-C-181): opens the generated Troubleshooting page, or says why it cannot.
    ///
    /// <para>Row 20's boundary and row 20's three answers: the path is computed from the repository root by
    /// <see cref="TroubleshootingPageService"/>, never taken from operator input, and existence is checked before
    /// anything is launched. Not generated (with how to generate it), generated part-way, opened (with when it was
    /// generated) — none of them opens an empty window or does nothing.</para>
    /// </summary>
    private void OpenTroubleshooting()
    {
        string repositoryRoot;
        try
        {
            repositoryRoot = GuiFixtureManifestService.FindRepositoryRoot(AppContext.BaseDirectory);
        }
        catch (InvalidOperationException ex)
        {
            // The answer rows 15, 16, 17 and 20 give: this menu needs a checkout, and says so rather than being greyed out.
            StatusText = "Troubleshooting page needs the repository; this build is not running from a checkout.";
            Log($"Troubleshooting page not opened: {ex.Message}");
            return;
        }

        var status = TroubleshootingPageService.Resolve(repositoryRoot);
        LastTroubleshootingPagePath = status.PagePath;
        if (!status.IsAvailable)
        {
            StatusText = status.Message;
            Log(status.Message);
            return;
        }

        // Automation is headless and unattended: resolution and the existence check above run, the browser launch
        // does not, and the suppression is recorded (as for rows 14 and 20).
        if (App.IsAutomationMode)
        {
            TroubleshootingLaunchSuppressed = true;
            StatusText = $"{status.Message} (launch suppressed under automation)";
            Log(StatusText);
            return;
        }

        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = status.PagePath!,
                UseShellExecute = true
            });
            StatusText = status.Message;
            Log(status.Message);
        }
        catch (Exception ex)
        {
            StatusText = $"Troubleshooting page could not be opened: {ex.Message}";
            Log($"Troubleshooting page launch failed: {ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>The page <see cref="OpenTroubleshootingCommand"/> resolved last; null when it had nothing to open.</summary>
    public string? LastTroubleshootingPagePath
    {
        get => _lastTroubleshootingPagePath;
        private set => SetProperty(ref _lastTroubleshootingPagePath, value);
    }

    /// <summary>True when a run suppressed the browser launch because it is an automation run.</summary>
    public bool TroubleshootingLaunchSuppressed
    {
        get => _troubleshootingLaunchSuppressed;
        private set => SetProperty(ref _troubleshootingLaunchSuppressed, value);
    }

    /// <summary>#225 row 21: opens the generated Troubleshooting page, or says how to generate it.</summary>
    public RelayCommand OpenTroubleshootingCommand { get; }

    /// <summary>
    /// #225 row 14 (GUI-C-163): opens this run set's evidence directory in the OS file browser.
    ///
    /// <para><b>The boundary the lead set is not "shell or not" but "who owns the path".</b> The path
    /// here is computed by the app (<c>evidence/&lt;RunId&gt;</c>, the same directory
    /// <see cref="RecordVerdict"/> writes and <see cref="ExportEvidenceBundle"/> zips); no user string
    /// reaches the shell. Opening an arbitrary path the operator typed is a DIFFERENT decision and is
    /// deliberately not implemented here.</para>
    ///
    /// <para>Existence is checked before launching, and a missing directory answers on the status line
    /// rather than creating an empty one — an empty folder would imply evidence exists. The live
    /// precedent in <c>clients/ImageProcTest</c> creates instead of checking; that difference is
    /// deliberate and recorded in the GUI-C-163 report.</para>
    ///
    /// <para><c>Process.Start</c> throws when no program is associated, so the failure is caught and
    /// reported rather than swallowed: a menu item that silently does nothing is the defect this row
    /// is supposed to remove.</para>
    /// </summary>
    private void OpenEvidenceFolder()
    {
        if (string.IsNullOrWhiteSpace(RunSet.RunId))
        {
            StatusText = "No evidence folder to open: no run set has started.";
            Log(StatusText);
            return;
        }

        var directory = Path.Combine(AppContext.BaseDirectory, "evidence", RunSet.RunId);
        if (!Directory.Exists(directory))
        {
            StatusText = $"No evidence folder yet for this run set: {directory}";
            Log(StatusText);
            return;
        }

        LastEvidenceFolderPath = directory;

        // An automation run is headless and unattended; leaving file-browser windows behind on a CI
        // machine is not what a check should do. The resolution and the existence check above DO run,
        // so a scenario observes everything except the launch itself — stated as a gap in the report
        // and recorded in the report data as EvidenceFolderLaunchSuppressed.
        if (App.IsAutomationMode)
        {
            EvidenceFolderLaunchSuppressed = true;
            StatusText = $"Evidence folder resolved (launch suppressed under automation): {directory}";
            Log(StatusText);
            return;
        }

        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = directory,
                UseShellExecute = true
            });
            StatusText = $"Evidence folder opened: {directory}";
            Log(StatusText);
        }
        catch (Exception ex)
        {
            StatusText = $"Evidence folder could not be opened: {ex.Message}";
            Log($"Evidence folder launch failed: {ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>The evidence directory <see cref="OpenEvidenceFolderCommand"/> last resolved; null before it succeeded.</summary>
    public string? LastEvidenceFolderPath
    {
        get => _lastEvidenceFolderPath;
        private set => SetProperty(ref _lastEvidenceFolderPath, value);
    }

    /// <summary>True when a run suppressed the file-browser launch because it is an automation run.</summary>
    public bool EvidenceFolderLaunchSuppressed
    {
        get => _evidenceFolderLaunchSuppressed;
        private set => SetProperty(ref _evidenceFolderLaunchSuppressed, value);
    }

    private void ExportEvidenceBundle()
    {
        try
        {
            if (string.IsNullOrWhiteSpace(RunSet.RunId))
            {
                // #178: an empty id would zip evidence/ into a file inside it.
                StatusText = "Export failed: no run set has started.";
                Log(StatusText);
                return;
            }

            var evidenceDir = Path.Combine(AppContext.BaseDirectory, "evidence", RunSet.RunId);
            if (!Directory.Exists(evidenceDir))
            {
                StatusText = "No evidence directory found for current run-set.";
                Log(StatusText);
                return;
            }

            var bundleDir = Path.Combine(AppContext.BaseDirectory, "evidence", "bundles");
            Directory.CreateDirectory(bundleDir);

            var zipPath = Path.Combine(bundleDir, $"{RunSet.RunId}.zip");
            if (File.Exists(zipPath))
                File.Delete(zipPath);

            System.IO.Compression.ZipFile.CreateFromDirectory(evidenceDir, zipPath);
            StatusText = $"Evidence bundle exported: {zipPath}";
            Log(StatusText);
        }
        catch (Exception ex)
        {
            StatusText = $"Export failed: {ex.Message}";
            Log(StatusText);
        }
    }

    /// <summary>
    /// Draws both workbench lanes from ONE original (#173, GUI-C-113).
    ///
    /// <para>The workbench compares settings, not images, so the frame handed to each lane is the same
    /// object and only the settings differ: Lane A (Reference) runs the snapshot as it stands, Lane B
    /// (Candidate) runs a COPY of it with the override applied. The copy matters — mutating the shared
    /// snapshot would make the lanes one pipeline wearing two labels, which is the failure L-02 exists
    /// to catch.</para>
    ///
    /// <para>An override of 0 means "follow the Reference", and then both lanes run identical settings
    /// and must draw identical pixels. That is the control case: if they differ there, the lanes are
    /// not drawing the same original.</para>
    /// </summary>
    private async Task RenderLanesAsync(LoadedImageFrame sourceFrame, AppSettings inputs, System.Windows.Media.ImageSource? reference)
    {
        // GUI-C-186e: the Candidate's chain (which can include the AI stage, and so the AI session gate) runs in the background like the
        // main render's does. It used to run here, on the UI thread, after the await above: measured 3028 ms of UI-dispatcher
        // latency behind a gate held for 3000 ms. Only the result comes back; it is dropped when the backend was replaced or shut
        // down meanwhile.
        var backend = _backend;
        try
        {
            // The Reference IS what the main viewport just drew — same original, same settings. Running
            // the pipeline again for it would be a second computation of a result already in hand, and
            // it would not be the same claim either: two runs of one setting can only agree.
            LaneAImage = reference;

            // With no override the Candidate is the Reference, so nothing is run for it either. That
            // keeps an ordinary apply at exactly one pipeline call — measured: adding a second and third
            // call broke W-23 and W-26, which count the calls an Apply makes through the fault-injection
            // seam. The cost is paid only when the user actually asks the two lanes to differ.
            var differs = inputs.LaneBVoiWindowWidth > 0.0f
                || !string.Equals(inputs.LaneBAlgorithm, inputs.LaneAAlgorithm, StringComparison.Ordinal)
                || Math.Abs(inputs.LaneBGsvgDenoiseK - inputs.GsvgDenoiseK) > 0.0001;

            if (differs)
            {
                var candidate = inputs.Snapshot();
                AlgorithmPreset.For(inputs.LaneBAlgorithm).ApplyTo(candidate);
                if (inputs.LaneBVoiWindowWidth > 0.0f)
                {
                    candidate.VoiWindowWidth = inputs.LaneBVoiWindowWidth;
                }

                // The Candidate's own vg_denoise_k. Copied like the width above, so GuiGsvgRunner reads
                // it from the lane's settings without knowing a lane exists.
                candidate.GsvgDenoiseK = inputs.LaneBGsvgDenoiseK;

                var candidateImage = await Task.Run(() => RenderLane(backend, sourceFrame, candidate));
                if (!ReferenceEquals(backend, _backend) || Lifecycle.IsTransitioning)
                {
                    Log("Lane B result dropped: the backend was replaced or is shutting down.");
                    return;
                }

                LaneBImage = candidateImage;
            }
            else
            {
                LaneBImage = reference;
            }

            _renderedLaneBWidth = inputs.LaneBVoiWindowWidth;
            _renderedLaneBAlgorithm = inputs.LaneBAlgorithm;
            _renderedLaneBDenoiseK = inputs.LaneBGsvgDenoiseK;
            OnPropertyChanged(nameof(LaneBIsStale));
        }
        catch (Exception ex)
        {
            Log($"Lane rendering failed: {ex.Message}");
            LaneAImage = null;
            LaneBImage = null;
        }
    }

    private static System.Windows.Media.ImageSource? RenderLane(IXpeBackend backend, LoadedImageFrame sourceFrame, AppSettings settings)
    {
        var chain = backend.RunChain(sourceFrame, ProcessingChainPlan.BuildStages(settings), settings);
        var frame = backend.ApplyDisplayPipeline(sourceFrame, chain.DisplayInput, settings);
        return frame.ProcessedPreview ?? frame.Preview;
    }

    /// <summary>
    /// Whether the Candidate lane on screen was drawn with the override currently in the settings
    /// (#173). Only the Candidate can be stale: the Reference does not read the override at all, so an
    /// edit to it cannot make the Reference wrong.
    /// </summary>
    public bool LaneBIsStale =>
        Math.Abs(Settings.LaneBVoiWindowWidth - _renderedLaneBWidth) > 0.0001f
        || !string.Equals(Settings.LaneBAlgorithm, _renderedLaneBAlgorithm, StringComparison.Ordinal)
        || Math.Abs(Settings.LaneBGsvgDenoiseK - _renderedLaneBDenoiseK) > 0.0001;

    /// <summary>The chain of the image on screen, for the reports (#180, GUI-C-99).</summary>
    public object DescribeChain() => new
    {
        status = ChainStatus,
        pipelineTimings = PipelineTimings,
        exposureKvp = _renderedInputs?.ExposureKvp,
        pixelPitchMm = _renderedInputs?.PixelPitchMm,
        gsvgMode = _renderedInputs?.GsvgMode,
        gsvgGridRatio = _renderedInputs?.GsvgGridRatio,
        gsvgGridFrequencyPerCm = _renderedInputs?.GsvgGridFrequencyPerCm,
        preprocessRequested = _renderedInputs?.PreprocessInChain,
        displayInput = LastChain is null ? "not run" : LastChain.DisplaysRaw ? "raw" : "chain",
        stages = LastChain?.Stages.Select(s => new { id = s.StageId, status = s.Status.ToString(), reason = s.Reason, elapsedMs = s.ElapsedMs }).ToArray()
            ?? Array.Empty<object>(),
    };

    /// <summary>
    /// Records an alert — into <see cref="Alerts"/>, and as a line in the log the user can actually
    /// see (#198 ①, GUI-C-125).
    ///
    /// <para><b>Why the log rather than a panel of its own.</b> Nothing on screen displayed
    /// <see cref="Alerts"/> at all: measured in GUI-C-122, the automation tree carried
    /// <c>ClearAlertsButton</c> and no list to clear. The log already survives (the status bar is
    /// overwritten by the next action), scrolls, and can be copied (GUI-C-122, GUI-C-123) — three
    /// properties a second panel would have to earn again, while splitting the user's attention
    /// across two places. A message in a place nobody looks is the silent failure this issue is about.</para>
    ///
    /// <para><b>Why the prefix, and why text.</b> Merged without a marker an alert would be buried
    /// rather than shown. The marker is the word ALERT plus the severity and code, in the line text
    /// itself — a colour or an icon would need the item template, which GUI-C-122 deliberately left
    /// alone, and neither is readable by automation or carried along when the line is copied. The text
    /// travels with the clipboard, which is the point of copying it.</para>
    ///
    /// <para><see cref="Alerts"/> itself is unchanged: it is where the native drain will land (#198 ②)
    /// and what Clear Alerts clears.</para>
    /// </summary>
    private void RaiseAlert(AlertEntry alert)
    {
        Alerts.Insert(0, alert);
        // #201 (a), GUI-C-136: remember the exact line so Clear Alerts can take it back out. Matching
        // the text later would be a heuristic — an ordinary message containing "ALERT " would be caught
        // with it. The line that was inserted is the only thing that identifies it without guessing.
        _alertLogLines.Add(Log($"ALERT {alert.Severity} {alert.Code}: {alert.Message}"));
    }

    /// <summary>Writes one line and returns it, so a caller can record which line it wrote.</summary>
    private string Log(string message)
    {
        var line = $"[{DateTimeOffset.Now:HH:mm:ss.fff}] {message}";
        Logs.Insert(0, line);
        return line;
    }

    /// <summary>
    /// #201 (a) (GUI-C-136): clearing alerts takes the alert LINES off the screen too.
    ///
    /// <para>Before this, the command ran <c>Alerts.Clear()</c> on a collection no element is bound to,
    /// so pressing it changed nothing a user could see — measured: log 11 lines / 3 alert lines before
    /// and after, while Clear Logs took the same log to 0. "The button does nothing observable" was the
    /// defect, not a missing panel.</para>
    ///
    /// <para><b>Only the lines RaiseAlert wrote.</b> The Application Log is not this button's to empty —
    /// Clear Logs is a different button, and <c>PressingClearAlerts_KeepsTheOrdinaryLogLines</c> (written
    /// BEFORE this change) is what says so.</para>
    /// </summary>
    private void ClearAlerts()
    {
        Alerts.Clear();

        foreach (var line in _alertLogLines)
        {
            Logs.Remove(line);
        }

        _alertLogLines.Clear();
    }

    /// <summary>
    /// Clear Logs empties the log, so the alert-line bookkeeping goes with it — otherwise a later Clear
    /// Alerts would try to remove lines that are gone, and a NEW line that happened to match an old
    /// string could be removed instead.
    /// </summary>
    private void ClearLogs()
    {
        Logs.Clear();
        _alertLogLines.Clear();
    }

    private enum CalibrationPathKind
    {
        Offset,
        Gain,
        Defect
    }

    private sealed class MockTitleConverter(string title) : System.Windows.Data.IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture) =>
            value is true ? "[MOCK] " + title : title;

        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture) =>
            throw new NotSupportedException();
    }
}
