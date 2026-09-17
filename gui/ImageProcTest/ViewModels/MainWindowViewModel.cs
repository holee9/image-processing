using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Text.Json;
using System.Windows.Controls;
using System.Windows.Data;
using ImageProcTest.Controls;
using ImageProcTest.Models;
using ImageProcTest.Services;
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
    private int _drainedBackendAlertCount;
    private bool _showRuntimePanel = true;
    private bool _showRawSettingsPanel = true;
    private bool _showCalibrationPanel = true;
    private bool _showImageSummaryPanel = true;
    private bool _showMetadataPanel = true;
    // OFF at start, per MENU-001 §9.2 (#165). The other panel flags keep their old value because
    // nothing reads them: only this one is wired to a region.
    private bool _showLogsPanel = false;
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
    private readonly object _telemetryLock = new();

    /// <summary>
    /// Hard-coded algorithm names compiled into this build.
    /// When a new algorithm is added to the project, add its name here and rebuild.
    /// There is no runtime discovery.
    /// </summary>
    public static readonly string[] AlgorithmOptions =
        ["Baseline v1.0", "Production v1.2", "Candidate v1.4", "Candidate v1.5-rc"];

    public MainWindowViewModel(
        AppSettings settings,
        AppSettingsService settingsService,
        Func<AppSettings, IXpeBackend> backendFactory)
    {
        Settings = settings;
        _settingsService = settingsService;
        _backendFactory = backendFactory;
        _backend = _backendFactory(settings);

        Logs = new ObservableCollection<string>();
        Alerts = new ObservableCollection<AlertEntry>();
        BackendModeOptions = new[] { "Mock", "Native" };
        CalibrationStageModeOptions = CalibrationStageMode.Options;
        VoiLutModeOptions = new[] { "Linear", "LinearExact", "Sigmoid" };
        BodyPartOptions = Enum.GetNames<XpeBodyPartEnum>();
        // #161: one list, in Models. This array used to be a third copy of the mode vocabulary.
        CompareModeOptions = ComparisonModes.All;
        Settings.PropertyChanged += OnSettingsPropertyChanged;

        InitializeBackendCommand = new RelayCommand(InitializeBackend);
        ShutdownBackendCommand = new RelayCommand(ShutdownBackend);
        LoadImageCommand = new RelayCommand(LoadImage);
        ApplyDisplayPipelineCommand = new RelayCommand(() => _ = ApplyDisplayPipelineAsync());
        ApplyBodyPartPresetCommand = new RelayCommand(ApplyBodyPartPreset);
        RunPreprocessingCommand = new RelayCommand(RunPreprocessing);
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
        ClearLogsCommand = new RelayCommand(() => Logs.Clear());
        ClearAlertsCommand = new RelayCommand(() => Alerts.Clear());
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

        // AFTER the backend, not before (#161, GUI-C-63). InitializeBackend clears Logs and Alerts,
        // so anything said before it is written and erased within the same constructor — measured in
        // GUI-C-62, where a run with a rejected mode showed six log lines, none of them the rejection
        // and none of them the "GUI-S0 initialized." line written immediately before it. The whole of
        // that moment was gone, not just one line.
        ReportRejectedComparisonMode();
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

    public ObservableCollection<AlertEntry> Alerts { get; }

    public RelayCommand InitializeBackendCommand { get; }

    public RelayCommand ShutdownBackendCommand { get; }

    public RelayCommand LoadImageCommand { get; }

    public RelayCommand ApplyDisplayPipelineCommand { get; }

    public RelayCommand ApplyBodyPartPresetCommand { get; }

    /// <summary>#141: true once a preprocess run completed every stage.</summary>
    public bool PreprocessRan { get; private set; }

    /// <summary>#141: the summary line of the last preprocess attempt (success or refusal).</summary>
    public string PreprocessStages { get; private set; } = string.Empty;

    /// <summary>#141: runs the Phase-1a preprocess stages on the loaded frame.</summary>
    public RelayCommand RunPreprocessingCommand { get; }

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
        a.VoiWindowCenter != b.VoiWindowCenter
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
    // removed (C-65) and the automation report stopped emitting them (C-68). Removal is a separate
    // card; ShowCalibrationPanel and ShowLogsPanel below are still read and stay.
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

    public bool ShowCalibrationPanel
    {
        get => _showCalibrationPanel;
        set => SetProperty(ref _showCalibrationPanel, value);
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
    public string LaneAAlgorithm
    {
        get => Settings.LaneAAlgorithm;
        set { if (Settings.LaneAAlgorithm != value) { Settings.LaneAAlgorithm = value; OnPropertyChanged(); } }
    }

    public string LaneBAlgorithm
    {
        get => Settings.LaneBAlgorithm;
        set { if (Settings.LaneBAlgorithm != value) { Settings.LaneBAlgorithm = value; OnPropertyChanged(); } }
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

    public double LaneBSharpeningSigma
    {
        get => Settings.LaneBSharpeningSigma;
        set { if (Math.Abs(Settings.LaneBSharpeningSigma - value) > 0.0001) { Settings.LaneBSharpeningSigma = value; OnPropertyChanged(); } }
    }

    public double LaneBDenoiseStrength
    {
        get => Settings.LaneBDenoiseStrength;
        set { if (Math.Abs(Settings.LaneBDenoiseStrength - value) > 0.0001) { Settings.LaneBDenoiseStrength = value; OnPropertyChanged(); } }
    }

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

    public void ShutdownBackend()
    {
        _backend.Shutdown();
        RuntimeInfo = _backend.GetRuntimeInfo();
        DrainBackendTelemetry();
        StatusText = "Backend shutdown.";
        Log("Backend shutdown requested.");
    }

    // @MX:NOTE: [AUTO] Replaces current backend via factory; disposes old backend if IDisposable; called from constructor and InitializeBackendCommand
    private void InitializeBackend()
    {
        try
        {
            Alerts.Clear();
            Logs.Clear();
            _drainedBackendLogCount = 0;
            _drainedBackendAlertCount = 0;

            lock (_telemetryLock)
            {
                (_backend as IDisposable)?.Dispose();
                _backend = _backendFactory(Settings);
            }
            RuntimeInfo = _backend.Initialize(Settings);
            DrainBackendTelemetry();

            StatusText = $"Backend initialized: {_backend.GetVersion()}";
            Log($"Initialized backend '{RuntimeInfo.BackendName}' ({_backend.GetVersion()}).");
        }
        catch (Exception ex)
        {
            StatusText = $"Backend initialization failed: {ex.Message}";
            Log(StatusText);
        }
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
    /// two scheduled flags drive the checkmark on their (disabled) menu items, and the comparison
    /// view really is restored. <b>If one of the removed panels is ever built, its flag belongs back
    /// in this list</b> — the reason it left was the missing panel, not the flag.</para>
    /// </summary>
    private void ResetLayout()
    {
        ShowCalibrationPanel = true;
        ShowLogsPanel = true;
        Settings.ShowDisplayPanel = true;
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
    /// visibility binding reads <see cref="ShowCalibrationPanel"/>, and the calibration directory
    /// settings have no markup at all). The command said "panel visible" anyway, so a reader of the
    /// log was told something that had not happened — worse than silence, because it is believed.</para>
    ///
    /// <para>The command stays: <c>MENU-001</c> §4 lists it and §9.1 marks it S0 · Always. What
    /// changes is only what it claims. The wording follows this repository's own precedent for a
    /// command whose implementation has not arrived — <c>"RunOnAllQueuedCommand: not implemented
    /// (Slice 7)."</c> — rather than inventing a screen, which GUI-C-64 stopped for the reason that
    /// a layout invented to satisfy a message would then be the design.</para>
    /// </summary>
    private void ShowCalibrationSettings()
    {
        ShowCalibrationPanel = true;
        StatusText = "Calibration settings: no panel implemented (#165).";
        Log("Menu command: calibration settings — not implemented; no panel is shown (#165).");
    }

    private void ShowFixtureManager()
    {
        var fixtureRoot = Path.Combine(AppContext.BaseDirectory, "fixtures", "gui-s0");
        StatusText = Directory.Exists(fixtureRoot)
            ? $"Fixture pack available: {fixtureRoot}"
            : $"Fixture pack missing: {fixtureRoot}";
        Log(StatusText);
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
            // the other two drive the checkmark on their (disabled) menu items. Removing was cheap
            // precisely because nothing consumes these fields yet; it gets expensive once something
            // does.
            visiblePanels = new
            {
                calibration = ShowCalibrationPanel,
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
            Alerts.Insert(0, new AlertEntry
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
        await ApplyDisplayPipelineAsync();
    }

    // @MX:NOTE: [AUTO] Display pipeline runs on Task.Run (thread pool); await resumes on dispatcher thread, so ObservableCollection writes are safe
    private async Task ApplyDisplayPipelineAsync()
    {
        if (ActiveImageFrame is null)
        {
            StatusText = "Display pipeline requires a loaded raw image.";
            Log(StatusText);
            Alerts.Insert(0, new AlertEntry
            {
                Severity = "WARN",
                Code = "DISPLAY_NO_IMAGE",
                Message = StatusText,
                Timestamp = DateTimeOffset.Now
            });
            return;
        }

        StatusText = "Applying display pipeline...";
        Log($"Display pipeline requested: mode={Settings.VoiLutMode}, bodyPart={Settings.SelectedBodyPart}, center={Settings.VoiWindowCenter}, width={Settings.VoiWindowWidth}, GSDF={Settings.GsdfEnabled}, calibrationEval=[{CalibrationEvaluationSummary}].");

        try
        {
            var sourceFrame = ActiveImageFrame;
            var inputs = Settings.Snapshot();
            var processedFrame = await Task.Run(() => _backend.ApplyDisplayPipeline(sourceFrame, inputs));
            DrainBackendTelemetry();

            ActiveImageFrame = processedFrame;
            ProcessedImage = processedFrame.ProcessedPreview ?? processedFrame.Preview;
            PreviewStaleReason = null;   // #171 ③: this render is current; ① is re-evaluated just below
            SetRenderedVoi(inputs);
            MetadataText = processedFrame.MetadataText;
            DisplayPipelineSummary = processedFrame.DisplayPipelineSummary;
            ActiveImageSummary = processedFrame.DisplayPipelineApplied
                ? $"{processedFrame.Summary} | {processedFrame.DisplayPipelineSummary}"
                : processedFrame.Summary;
            StatusText = processedFrame.DisplayPipelineSummary;
            OnPropertyChanged(nameof(FaultInjectionStatus));
        }
        catch (Exception ex)
        {
            OnPropertyChanged(nameof(FaultInjectionStatus));
            PreviewStaleReason = StalePipelineFailed;   // #171 ③
            StatusText = $"Display pipeline failed: {ex.Message}";
            Log(StatusText);
            Alerts.Insert(0, new AlertEntry
            {
                Severity = "ERROR",
                Code = "DISPLAY_PIPELINE_FAILED",
                Message = ex.Message,
                Timestamp = DateTimeOffset.Now
            });
        }
    }

    /// <summary>
    /// The preset the active backend last produced, kept so a caller can check that it was applied
    /// without knowing which backend produced it. #135: the automation harness used to compare the
    /// settings against MockXpeBackend's literal values, which made the check fail under the real
    /// DLL — whose abdomen window (C=40/W=400, HU) is the clinically validated one.
    /// </summary>
    public VoiPreset? LastAppliedVoiPreset { get; private set; }

    /// <summary>
    /// #141: Phase-1a preprocessing. A refusal (no calibration, Mock backend) is surfaced as an
    /// alert and a log line — not an exception — because both are expected states.
    /// </summary>
    private void RunPreprocessing()
    {
        if (ActiveImageFrame is null)
        {
            StatusText = "Load a raw image before running preprocessing.";
            Log(StatusText);
            return;
        }

        var result = _backend.RunPreprocessing(ActiveImageFrame, Settings);
        StatusText = result.Summary;
        Log(result.Summary);
        DrainBackendTelemetry();

        PreprocessRan = result.Ran;
        PreprocessStages = result.Summary;

        if (result.Ran && result.ProcessedPreview is not null)
        {
            // #141: the corrected frame reaches the processed viewport. Without this the run is
            // observable only in the log, and "it ran" could not be told from "it ran and produced
            // something the operator can see".
            ProcessedImage = result.ProcessedPreview ?? ProcessedImage;
            SetRenderedVoi(null);   // RealXpeBackend.CreatePreview stretches min..max; no VOI was applied
        }

        if (!result.Ran)
        {
            Alerts.Insert(0, new AlertEntry
            {
                Severity = "WARN",
                Code = "PREPROCESS_NOT_RUN",
                Message = result.Summary,
                Timestamp = DateTimeOffset.Now,
            });
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
            Alerts.Insert(0, new AlertEntry
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
            Alerts.Insert(0, alert);
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
            Alerts.Insert(0, new AlertEntry
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
        LaneBSharpeningSigma = 0.85;
        LaneBDenoiseStrength = 0.42;
        StatusText = "Lane B overrides reset to defaults.";
        Log(StatusText);
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

    private void Log(string message)
    {
        Logs.Insert(0, $"[{DateTimeOffset.Now:HH:mm:ss.fff}] {message}");
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
