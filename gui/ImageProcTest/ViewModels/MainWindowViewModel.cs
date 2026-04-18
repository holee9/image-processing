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

    public void ShutdownBackend()
    {
        _backend.Shutdown();
        RuntimeInfo = _backend.GetRuntimeInfo();
        DrainBackendTelemetry();
        StatusText = "Backend shutdown.";
        Log("Backend shutdown requested.");
    }

    private void InitializeBackend()
    {
        try
        {
            Alerts.Clear();
            Logs.Clear();
            _drainedBackendLogCount = 0;
            _drainedBackendAlertCount = 0;

            _backend = _backendFactory(Settings);
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

    private void ApplyBodyPartPreset()
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
            _ = ApplyDisplayPipelineAsync();
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

    private void DrainBackendTelemetry()
    {
        var logCount = _backend.GetLogCount();
        for (var i = _drainedBackendLogCount; i < logCount; i++)
        {
            var log = _backend.GetLog(i);
            if (!string.IsNullOrWhiteSpace(log))
            {
                Logs.Insert(0, log);
            }
        }
        _drainedBackendLogCount = logCount;

        var alertCount = _backend.GetAlertCount();
        for (var i = _drainedBackendAlertCount; i < alertCount; i++)
        {
            var alert = _backend.GetAlert(i);
            if (alert is not null)
            {
                Alerts.Insert(0, alert);
            }
        }
        _drainedBackendAlertCount = alertCount;
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
