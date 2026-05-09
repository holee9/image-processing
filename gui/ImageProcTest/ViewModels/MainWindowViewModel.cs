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
    private BackendRuntimeInfo _runtimeInfo = new();
    private LoadedImageFrame? _activeImageFrame;
    private int _drainedBackendLogCount;
    private int _drainedBackendAlertCount;
    private bool _showRuntimePanel = true;
    private bool _showRawSettingsPanel = true;
    private bool _showCalibrationPanel = true;
    private bool _showImageSummaryPanel = true;
    private bool _showMetadataPanel = true;
    private bool _showLogsPanel = true;
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
        CompareModeOptions = new[]
        {
            "SwipeVertical",
            "SwipeHorizontal",
            "SplitLocked",
            "OverlayOpacity",
            "DifferenceHeatmap",
            "SourceOnly",
            "ProcessedOnly"
        };
        Settings.PropertyChanged += OnSettingsPropertyChanged;

        InitializeBackendCommand = new RelayCommand(InitializeBackend);
        ShutdownBackendCommand = new RelayCommand(ShutdownBackend);
        LoadImageCommand = new RelayCommand(LoadImage);
        ApplyDisplayPipelineCommand = new RelayCommand(() => _ = ApplyDisplayPipelineAsync());
        ApplyBodyPartPresetCommand = new RelayCommand(ApplyBodyPartPreset);
        ZoomFitCommand = new RelayCommand(ZoomFit);
        ZoomActualCommand = new RelayCommand(ZoomActual);
        ZoomInCommand = new RelayCommand(ZoomIn);
        ZoomOutCommand = new RelayCommand(ZoomOut);
        ResetComparisonViewCommand = new RelayCommand(ResetComparisonView);
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
    }

    public AppSettings Settings { get; }

    public string[] BackendModeOptions { get; }

    public string[] CalibrationStageModeOptions { get; }

    public string[] VoiLutModeOptions { get; }

    public string[] BodyPartOptions { get; }

    public string[] CompareModeOptions { get; }

    public ObservableCollection<string> Logs { get; }

    public ObservableCollection<AlertEntry> Alerts { get; }

    public RelayCommand InitializeBackendCommand { get; }

    public RelayCommand ShutdownBackendCommand { get; }

    public RelayCommand LoadImageCommand { get; }

    public RelayCommand ApplyDisplayPipelineCommand { get; }

    public RelayCommand ApplyBodyPartPresetCommand { get; }

    public RelayCommand ZoomFitCommand { get; }

    public RelayCommand ZoomActualCommand { get; }

    public RelayCommand ZoomInCommand { get; }

    public RelayCommand ZoomOutCommand { get; }

    public RelayCommand ResetComparisonViewCommand { get; }

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

    public BackendRuntimeInfo RuntimeInfo
    {
        get => _runtimeInfo;
        private set => SetProperty(ref _runtimeInfo, value);
    }

    public LoadedImageFrame? ActiveImageFrame
    {
        get => _activeImageFrame;
        private set => SetProperty(ref _activeImageFrame, value);
    }

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

    private void ResetLayout()
    {
        ShowRuntimePanel = true;
        ShowRawSettingsPanel = true;
        ShowCalibrationPanel = true;
        ShowImageSummaryPanel = true;
        ShowMetadataPanel = true;
        ShowLogsPanel = true;
        ShowAlertsPanel = true;
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

    private void ShowCalibrationSettings()
    {
        ShowCalibrationPanel = true;
        StatusText = "Calibration settings panel visible.";
        Log("Menu command: calibration settings panel shown.");
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
            activeImageSummary = ActiveImageSummary,
            status = StatusText,
            settings = Settings,
            visiblePanels = new
            {
                runtime = ShowRuntimePanel,
                rawSettings = ShowRawSettingsPanel,
                calibration = ShowCalibrationPanel,
                display = Settings.ShowDisplayPanel,
                imageSummary = ShowImageSummaryPanel,
                metadata = ShowMetadataPanel,
                logs = ShowLogsPanel,
                alerts = ShowAlertsPanel
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
            var processedFrame = await Task.Run(() => _backend.ApplyDisplayPipeline(sourceFrame, Settings));
            DrainBackendTelemetry();

            ActiveImageFrame = processedFrame;
            ProcessedImage = processedFrame.ProcessedPreview ?? processedFrame.Preview;
            MetadataText = processedFrame.MetadataText;
            DisplayPipelineSummary = processedFrame.DisplayPipelineSummary;
            ActiveImageSummary = processedFrame.DisplayPipelineApplied
                ? $"{processedFrame.Summary} | {processedFrame.DisplayPipelineSummary}"
                : processedFrame.Summary;
            StatusText = processedFrame.DisplayPipelineSummary;
        }
        catch (Exception ex)
        {
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

    private void ResetComparisonView()
    {
        Settings.ComparisonMode = "SwipeVertical";
        Settings.ComparisonZoomScale = 0.0;
        Settings.ComparisonPanX = 0.0;
        Settings.ComparisonPanY = 0.0;
        Settings.ComparisonSwipePosition = 0.5;
        Settings.ComparisonOverlayOpacity = 0.5;
        OnPropertyChanged(nameof(ComparisonStatus));
    }

    private void OpenDetachedComparisonViewer()
    {
        var viewport = new ImageComparisonViewport
        {
            Margin = new System.Windows.Thickness(12),
            MinWidth = 640,
            MinHeight = 480
        };
        BindDetachedViewport(viewport, ImageComparisonViewport.SourceImageProperty, nameof(SourceImage));
        BindDetachedViewport(viewport, ImageComparisonViewport.ProcessedImageProperty, nameof(ProcessedImage));
        BindDetachedViewport(viewport, ImageComparisonViewport.CompareModeProperty, "Settings.ComparisonMode");
        BindDetachedViewport(viewport, ImageComparisonViewport.ZoomScaleProperty, "Settings.ComparisonZoomScale");
        BindDetachedViewport(viewport, ImageComparisonViewport.PanXProperty, "Settings.ComparisonPanX");
        BindDetachedViewport(viewport, ImageComparisonViewport.PanYProperty, "Settings.ComparisonPanY");
        BindDetachedViewport(viewport, ImageComparisonViewport.SwipePositionProperty, "Settings.ComparisonSwipePosition");
        BindDetachedViewport(viewport, ImageComparisonViewport.OverlayOpacityProperty, "Settings.ComparisonOverlayOpacity");

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
        Grid.SetRow(status, 1);
        grid.Children.Add(status);

        var window = new System.Windows.Window
        {
            Title = "ImageProcTest Comparison Viewer",
            Width = 1280,
            Height = 820,
            MinWidth = 900,
            MinHeight = 620,
            Content = grid
        };

        var owner = System.Windows.Application.Current.Windows.OfType<System.Windows.Window>().FirstOrDefault(w => w.IsActive);
        if (owner is not null && !ReferenceEquals(owner, window))
        {
            window.Owner = owner;
        }

        window.Show();
        RefreshComparisonStatus("Detached comparison viewer opened.");
    }

    private void BindDetachedViewport(System.Windows.DependencyObject target, System.Windows.DependencyProperty property, string path)
    {
        BindingOperations.SetBinding(target, property, new DataBinding(path)
        {
            Source = this,
            Mode = BindingMode.TwoWay,
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
}
