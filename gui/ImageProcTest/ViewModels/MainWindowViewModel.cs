using System.Collections.ObjectModel;
using System.IO;
using System.Text.Json;
using ImageProcTest.Models;
using ImageProcTest.Services;
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
    private System.Windows.Media.ImageSource? _sourceImage;
    private System.Windows.Media.ImageSource? _processedImage;
    private BackendRuntimeInfo _runtimeInfo = new();
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

        InitializeBackendCommand = new RelayCommand(InitializeBackend);
        ShutdownBackendCommand = new RelayCommand(ShutdownBackend);
        LoadImageCommand = new RelayCommand(LoadImage);
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

    public ObservableCollection<string> Logs { get; }

    public ObservableCollection<AlertEntry> Alerts { get; }

    public RelayCommand InitializeBackendCommand { get; }

    public RelayCommand ShutdownBackendCommand { get; }

    public RelayCommand LoadImageCommand { get; }

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
        StatusText = "Layout reset.";
        Log("Menu command: layout reset.");
    }

    private void ShowNativeDiagnostics()
    {
        StatusText = RuntimeInfo.NativeDllDetected
            ? $"Native DLL detected at '{RuntimeInfo.NativeDllPath}'."
            : "Native DLL not detected; mock backend remains active.";

        Log($"Native diagnostics: backend={RuntimeInfo.BackendName}, version={RuntimeInfo.Version}, state={RuntimeInfo.State}, nativeDetected={RuntimeInfo.NativeDllDetected}, nativePath='{RuntimeInfo.NativeDllPath}'.");
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
                imageSummary = ShowImageSummaryPanel,
                metadata = ShowMetadataPanel,
                logs = ShowLogsPanel,
                alerts = ShowAlertsPanel
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

    private void LoadImage()
    {
        if (!string.IsNullOrWhiteSpace(App.AutomationRawPath) && File.Exists(App.AutomationRawPath))
        {
            LoadImageFromPath(App.AutomationRawPath, "automation raw image");
            return;
        }

        var automationPath = Environment.GetEnvironmentVariable("XPE_GUI_AUTOMATION_RAW_PATH");
        if (!string.IsNullOrWhiteSpace(automationPath) && File.Exists(automationPath))
        {
            LoadImageFromPath(automationPath, "automation raw image");
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

        try
        {
            LoadImageFromPath(dialog.FileName, "raw image");
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

    private void LoadImageFromPath(string path, string sourceLabel)
    {
        Settings.LastRawDirectory = Path.GetDirectoryName(path) ?? string.Empty;
        var loadedFrame = _backend.LoadRawImage(path, Settings);
        DrainBackendTelemetry();

        SourceImage = loadedFrame.Preview;
        ProcessedImage = loadedFrame.Preview;
        ActiveImageSummary = loadedFrame.Summary;
        MetadataText = loadedFrame.MetadataText;
        StatusText = $"Loaded {sourceLabel} '{path}'.";
        Log($"Loaded {sourceLabel} '{path}'.");
    }

    private void DrainBackendTelemetry()
    {
        for (var i = 0; i < _backend.GetLogCount(); i++)
        {
            var log = _backend.GetLog(i);
            if (!string.IsNullOrWhiteSpace(log))
            {
                Logs.Insert(0, log);
            }
        }

        for (var i = 0; i < _backend.GetAlertCount(); i++)
        {
            var alert = _backend.GetAlert(i);
            if (alert is not null)
            {
                Alerts.Insert(0, alert);
            }
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
