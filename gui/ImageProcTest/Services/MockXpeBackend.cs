using ImageProcTest.Models;
using System.Windows.Media;
using System.Windows.Media.Imaging;

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
        var calibrationSummary = BuildCalibrationEvaluationSummary(settings);
        var summary = $"MOCK CalibrationEval({calibrationSummary}) -> Display: Modality({settings.ModalityRescaleSlope:0.###}/{settings.ModalityRescaleIntercept:0.###}) -> VOI({settings.VoiLutMode}, C={settings.VoiWindowCenter:0.###}, W={settings.VoiWindowWidth:0.###}) -> GSDF({(settings.GsdfEnabled ? "on" : "off")})";
        AddLog(summary);
        var processedPreview = CreateDisplayPreview(rawFrame, settings);

        return new LoadedImageFrame
        {
            Preview = rawFrame.Preview,
            ProcessedPreview = processedPreview,
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

    private static BitmapSource CreateDisplayPreview(LoadedImageFrame rawFrame, AppSettings settings)
    {
        if (rawFrame.RawPixels is null || rawFrame.Width <= 0 || rawFrame.Height <= 0)
        {
            return rawFrame.ProcessedPreview ?? rawFrame.Preview;
        }

        var count = checked(rawFrame.Width * rawFrame.Height);
        var calibratedPixels = CreateMockCalibrationPixels(rawFrame, settings);
        var output = new byte[count];
        var width = Math.Max(1.0, settings.VoiWindowWidth);
        var center = settings.VoiWindowCenter;
        var lower = center - (width / 2.0);
        var upper = center + (width / 2.0);
        var range = Math.Max(1.0, upper - lower);

        for (var i = 0; i < count; i++)
        {
            var modality = (calibratedPixels[i] * settings.ModalityRescaleSlope) + settings.ModalityRescaleIntercept;
            var normalized = NormalizeVoi(modality, lower, range, settings.VoiLutMode);
            output[i] = (byte)Math.Clamp((int)Math.Round(normalized * 255.0), 0, 255);
        }

        var preview = BitmapSource.Create(
            rawFrame.Width,
            rawFrame.Height,
            96,
            96,
            PixelFormats.Gray8,
            null,
            output,
            rawFrame.Width);
        preview.Freeze();
        return preview;
    }

    // @MX:NOTE: [AUTO] 6-stage mock calibration applied in fixed order: Offset → Temperature → Nonlinearity → Gain → Binning → Defect → Ghost; stage order encodes physical dependencies
    private static double[] CreateMockCalibrationPixels(LoadedImageFrame rawFrame, AppSettings settings)
    {
        var count = checked(rawFrame.Width * rawFrame.Height);
        var pixels = new double[count];
        for (var i = 0; i < count; i++)
        {
            pixels[i] = rawFrame.RawPixels![i];
        }

        if (ShouldApplyStage(settings.OffsetCorrectionMode, autoApplies: true))
        {
            for (var i = 0; i < count; i++)
            {
                pixels[i] = Math.Max(0.0, pixels[i] - 128.0);
            }
        }

        if (ShouldApplyStage(settings.TemperatureCompensationMode, autoApplies: false))
        {
            for (var y = 0; y < rawFrame.Height; y++)
            {
                var drift = 64.0 * y / Math.Max(1, rawFrame.Height - 1);
                var rowStart = y * rawFrame.Width;
                for (var x = 0; x < rawFrame.Width; x++)
                {
                    pixels[rowStart + x] = Math.Max(0.0, pixels[rowStart + x] - drift);
                }
            }
        }

        if (ShouldApplyStage(settings.NonlinearityCorrectionMode, autoApplies: false))
        {
            for (var i = 0; i < count; i++)
            {
                var normalized = Math.Clamp(pixels[i] / ushort.MaxValue, 0.0, 1.0);
                pixels[i] = Math.Pow(normalized, 1.015) * ushort.MaxValue;
            }
        }

        if (ShouldApplyStage(settings.GainCorrectionMode, autoApplies: true))
        {
            for (var y = 0; y < rawFrame.Height; y++)
            {
                var rowStart = y * rawFrame.Width;
                var yTerm = rawFrame.Height <= 1 ? 0.0 : y / (double)(rawFrame.Height - 1);
                for (var x = 0; x < rawFrame.Width; x++)
                {
                    var xTerm = rawFrame.Width <= 1 ? 0.0 : x / (double)(rawFrame.Width - 1);
                    var gain = 0.985 + (0.03 * xTerm) + (0.01 * yTerm);
                    pixels[rowStart + x] = pixels[rowStart + x] / Math.Max(0.1, gain);
                }
            }
        }

        if (ShouldApplyStage(settings.BinningCorrectionMode, autoApplies: false))
        {
            var smoothed = (double[])pixels.Clone();
            for (var y = 0; y < rawFrame.Height - 1; y += 2)
            {
                var rowStart = y * rawFrame.Width;
                var nextRowStart = rowStart + rawFrame.Width;
                for (var x = 0; x < rawFrame.Width - 1; x += 2)
                {
                    var average = (pixels[rowStart + x] + pixels[rowStart + x + 1] + pixels[nextRowStart + x] + pixels[nextRowStart + x + 1]) / 4.0;
                    smoothed[rowStart + x] = average;
                    smoothed[rowStart + x + 1] = average;
                    smoothed[nextRowStart + x] = average;
                    smoothed[nextRowStart + x + 1] = average;
                }
            }

            pixels = smoothed;
        }

        if (ShouldApplyStage(settings.DefectCorrectionMode, autoApplies: !string.IsNullOrWhiteSpace(settings.DefectCalibrationDirectory)))
        {
            var stride = Math.Max(257, rawFrame.Width / 3);
            for (var i = stride; i < count - stride; i += stride)
            {
                pixels[i] = (pixels[i - 1] + pixels[i + 1] + pixels[Math.Max(0, i - rawFrame.Width)] + pixels[Math.Min(count - 1, i + rawFrame.Width)]) / 4.0;
            }
        }

        if (ShouldApplyStage(settings.GhostCorrectionMode, autoApplies: false))
        {
            for (var y = 0; y < rawFrame.Height; y++)
            {
                var rowStart = y * rawFrame.Width;
                for (var x = 1; x < rawFrame.Width; x++)
                {
                    var index = rowStart + x;
                    pixels[index] = Math.Max(0.0, pixels[index] - (pixels[index - 1] * 0.01));
                }
            }
        }

        return pixels;
    }

    private static string BuildCalibrationEvaluationSummary(AppSettings settings)
    {
        return string.Join(", ", new[]
        {
            ResolveStage("Offset", settings.OffsetCorrectionMode, true),
            ResolveStage("Gain", settings.GainCorrectionMode, true),
            ResolveStage("Defect", settings.DefectCorrectionMode, !string.IsNullOrWhiteSpace(settings.DefectCalibrationDirectory)),
            ResolveStage("Ghost", settings.GhostCorrectionMode, false),
            ResolveStage("Temp", settings.TemperatureCompensationMode, false),
            ResolveStage("Nonlinearity", settings.NonlinearityCorrectionMode, false),
            ResolveStage("Binning", settings.BinningCorrectionMode, false)
        });
    }

    private static string ResolveStage(string label, string mode, bool autoApplies)
    {
        var normalized = CalibrationStageMode.Normalize(mode);
        var applied = ShouldApplyStage(normalized, autoApplies) ? "applied" : "bypassed";
        return $"{label}={normalized}/{applied}";
    }

    private static bool ShouldApplyStage(string mode, bool autoApplies)
    {
        return CalibrationStageMode.Normalize(mode) switch
        {
            CalibrationStageMode.On => true,
            CalibrationStageMode.Off => false,
            _ => autoApplies
        };
    }

    // @MX:NOTE: [AUTO] Sigmoid steepness = 4 / range matches clinical VOI display behavior; Linear branch clamps to [0, 1]
    private static double NormalizeVoi(double value, double lower, double range, string mode)
    {
        if (string.Equals(mode, "Sigmoid", StringComparison.OrdinalIgnoreCase))
        {
            var center = lower + (range / 2.0);
            var steepness = 4.0 / Math.Max(1.0, range);
            return 1.0 / (1.0 + Math.Exp(-steepness * (value - center)));
        }

        return Math.Clamp((value - lower) / range, 0.0, 1.0);
    }

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart) => bodyPart switch
    {
        XpeBodyPartEnum.Bone => new VoiPreset(40000.0f, 30000.0f, "Linear"),
        XpeBodyPartEnum.Lung => new VoiPreset(25000.0f, 50000.0f, "Linear"),
        XpeBodyPartEnum.Abdomen => new VoiPreset(32768.0f, 65535.0f, "Linear"),
        XpeBodyPartEnum.Head => new VoiPreset(35000.0f, 40000.0f, "Linear"),
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
