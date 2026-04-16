using ImageProcTest.Models;

namespace ImageProcTest.Services;

public sealed class MockXpeBackend : IXpeBackend
{
    private readonly RawImageLoader _rawImageLoader;
    private readonly string _nativeDllPath;
    private readonly bool _nativeDllDetected;
    private readonly List<AlertEntry> _alerts = new();
    private readonly List<string> _logs = new();
    private BackendRuntimeInfo _runtimeInfo = new();

    public MockXpeBackend(RawImageLoader rawImageLoader, string nativeDllPath, bool nativeDllDetected)
    {
        _rawImageLoader = rawImageLoader;
        _nativeDllPath = nativeDllPath;
        _nativeDllDetected = nativeDllDetected;
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
            NativeDllDetected = _nativeDllDetected,
            NativeDllPath = _nativeDllPath
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
            Code = _nativeDllDetected ? "NATIVE_DLL_DETECTED_BUT_UNUSED" : "NATIVE_DLL_NOT_FOUND",
            Message = _nativeDllDetected
                ? $"xpe_common.dll detected at '{_nativeDllPath}', but GUI-S0 remains mock-only."
                : $"xpe_common.dll not found at '{_nativeDllPath}'. Mock mode is expected.",
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

    public LoadedImageFrame LoadRawImage(string path, AppSettings settings)
    {
        AddLog($"LoadRawImage('{path}') invoked.");
        return _rawImageLoader.Load(path, settings);
    }

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
            NativeDllDetected = _nativeDllDetected,
            NativeDllPath = _nativeDllPath
        };
    }

    private void AddLog(string message)
    {
        _logs.Add($"[{DateTimeOffset.Now:HH:mm:ss.fff}] {message}");
    }
}
