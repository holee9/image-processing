using ImageProcTest.Models;

namespace ImageProcTest.Services;

public sealed class MockXpeBackend : IXpeBackend
{
    private readonly RawImageLoader _rawImageLoader;
    private readonly string _commonDllPath;
    private readonly bool _commonDllDetected;
    private readonly string _displayDllPath;
    private readonly bool _displayDllDetected;
    private readonly List<AlertEntry> _alerts = new();
    private readonly List<string> _logs = new();
    private BackendRuntimeInfo _runtimeInfo = new();

    public MockXpeBackend(
        RawImageLoader rawImageLoader,
        string commonDllPath,
        bool commonDllDetected,
        string displayDllPath,
        bool displayDllDetected)
    {
        _rawImageLoader = rawImageLoader;
        _commonDllPath = commonDllPath;
        _commonDllDetected = commonDllDetected;
        _displayDllPath = displayDllPath;
        _displayDllDetected = displayDllDetected;
    }

    public BackendRuntimeInfo Initialize(AppSettings settings)
    {
        _alerts.Clear();
        _logs.Clear();

        _runtimeInfo = new BackendRuntimeInfo
        {
            BackendName = "MockXpeBackend",
            Version = "v0.0.0-mock",
            State = "Initialized",
            SupportsNativeRuntime = false,
            NativeDllDetected = _commonDllDetected,
            NativeDllPath = _commonDllPath,
            DisplayVersion = GetDisplayVersion(),
            DisplayDllDetected = _displayDllDetected,
            DisplayDllPath = _displayDllPath
        };

        AddLog("MockXpeBackend bootstrap started.");
        AddLog($"Requested backend mode = {settings.BackendMode}");
        AddLog($"Offset calib dir = {settings.OffsetCalibrationDirectory}");
        AddLog($"Gain calib dir = {settings.GainCalibrationDirectory}");
        AddLog($"Defect calib dir = {settings.DefectCalibrationDirectory}");

        _alerts.Add(new AlertEntry
        {
            Severity = "INFO",
            Code = "MOCK_BACKEND_ACTIVE",
            Message = "Mock backend is active. Native P/Invoke integration is deferred to SPRINT-P0-07.",
            Timestamp = DateTimeOffset.Now
        });

        _alerts.Add(new AlertEntry
        {
            Severity = "WARN",
            Code = _displayDllDetected ? "DISPLAY_DLL_DETECTED_BUT_UNUSED" : "DISPLAY_DLL_NOT_FOUND",
            Message = _displayDllDetected
                ? $"xpe_display.dll detected at '{_displayDllPath}', but backend mode is Mock."
                : $"xpe_display.dll not found at '{_displayDllPath}'. Mock display mode is expected.",
            Timestamp = DateTimeOffset.Now
        });

        _alerts.Add(new AlertEntry
        {
            Severity = "ERROR",
            Code = "NO_REAL_DICOM_IN_GUI_S0",
            Message = "Real DICOM loading is intentionally unavailable in GUI-S0 and remains owned by xpe_dicom.dll.",
            Timestamp = DateTimeOffset.Now
        });

        return _runtimeInfo;
    }

    public string GetVersion() => _runtimeInfo.Version;

    public string GetDisplayVersion() => "v0.0.0-mock-display";

    public LoadedImageFrame LoadRawImage(string path, AppSettings settings)
    {
        AddLog($"LoadRawImage('{path}') invoked.");
        return _rawImageLoader.Load(path, settings);
    }

    public LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, AppSettings settings)
    {
        var summary = $"MOCK Display: Modality({settings.ModalityRescaleSlope:0.###}/{settings.ModalityRescaleIntercept:0.###}) -> VOI({settings.VoiLutMode}, C={settings.VoiWindowCenter:0.###}, W={settings.VoiWindowWidth:0.###}) -> GSDF({(settings.GsdfEnabled ? "on" : "off")})";
        AddLog(summary);

        return new LoadedImageFrame
        {
            Preview = rawFrame.Preview,
            ProcessedPreview = rawFrame.ProcessedPreview ?? rawFrame.Preview,
            Summary = rawFrame.Summary,
            MetadataText = rawFrame.MetadataText + Environment.NewLine + summary,
            RawPixels = rawFrame.RawPixels,
            Width = rawFrame.Width,
            Height = rawFrame.Height,
            BitsStored = rawFrame.BitsStored,
            DisplayPipelineApplied = true,
            DisplayPipelineSummary = summary
        };
    }

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart) => bodyPart switch
    {
        XpeBodyPartEnum.Bone => new VoiPreset(500.0f, 2000.0f, "Linear"),
        XpeBodyPartEnum.Lung => new VoiPreset(-600.0f, 1600.0f, "Linear"),
        XpeBodyPartEnum.Abdomen => new VoiPreset(40.0f, 400.0f, "Linear"),
        XpeBodyPartEnum.Head => new VoiPreset(40.0f, 80.0f, "Linear"),
        _ => throw new ArgumentOutOfRangeException(nameof(bodyPart), bodyPart, "Unsupported body part preset.")
    };

    public int GetAlertCount() => _alerts.Count;

    public AlertEntry? GetAlert(int index) => index >= 0 && index < _alerts.Count ? _alerts[index] : null;

    public int GetLogCount() => _logs.Count;

    public string? GetLog(int index) => index >= 0 && index < _logs.Count ? _logs[index] : null;

    public BackendRuntimeInfo GetRuntimeInfo() => _runtimeInfo;

    public void Shutdown()
    {
        AddLog("MockXpeBackend shutdown requested.");
        _runtimeInfo = new BackendRuntimeInfo
        {
            BackendName = "MockXpeBackend",
            Version = "v0.0.0-mock",
            State = "Shutdown",
            SupportsNativeRuntime = false,
            NativeDllDetected = _commonDllDetected,
            NativeDllPath = _commonDllPath,
            DisplayVersion = GetDisplayVersion(),
            DisplayDllDetected = _displayDllDetected,
            DisplayDllPath = _displayDllPath
        };
    }

    private void AddLog(string message)
    {
        _logs.Add($"[{DateTimeOffset.Now:HH:mm:ss.fff}] {message}");
    }
}
