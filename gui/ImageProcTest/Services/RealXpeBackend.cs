using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using ImageProcTest.Models;
using ImageProcTest.Services.Native;

namespace ImageProcTest.Services;

public sealed class RealXpeBackend : IXpeBackend
{
    private static readonly string[] RequiredCommonExports =
    {
        "xpe_alloc_image",
        "xpe_free_image"
    };

    private static readonly string[] RequiredDisplayExports =
    {
        "xpe_display_version",
        "xpe_apply_modality_lut",
        "xpe_apply_voi_lut",
        "xpe_voi_preset_create",
        "xpe_apply_presentation_lut",
        "xpe_gsdf_calibrate"
    };

    private readonly RawImageLoader _rawImageLoader;
    private readonly string _commonDllPath;
    private readonly string _displayDllPath;
    private readonly List<AlertEntry> _alerts = new();
    private readonly List<string> _logs = new();
    private BackendRuntimeInfo _runtimeInfo = new();

    public RealXpeBackend(RawImageLoader rawImageLoader, string commonDllPath, string displayDllPath)
    {
        _rawImageLoader = rawImageLoader;
        _commonDllPath = commonDllPath;
        _displayDllPath = displayDllPath;
    }

    // @MX:ANCHOR: [AUTO] Factory guard: verifies both DLLs have all required exports before routing to RealXpeBackend
    // @MX:REASON: Called by XpeBackendFactory.Create; must remain side-effect-free (loads DLL, checks exports, immediately frees); false → MockXpeBackend
    public static bool CanUseNative(string commonDllPath, string displayDllPath) =>
        HasExports(commonDllPath, RequiredCommonExports) &&
        HasExports(displayDllPath, RequiredDisplayExports);

    public BackendRuntimeInfo Initialize(AppSettings settings)
    {
        _alerts.Clear();
        _logs.Clear();

        var displayVersion = GetDisplayVersion();
        _runtimeInfo = new BackendRuntimeInfo
        {
            BackendName = "RealXpeBackend",
            Version = $"xpe_display {displayVersion}",
            State = "Initialized",
            SupportsNativeRuntime = true,
            NativeDllDetected = File.Exists(_commonDllPath),
            NativeDllPath = _commonDllPath,
            DisplayVersion = displayVersion,
            DisplayDllDetected = File.Exists(_displayDllPath),
            DisplayDllPath = _displayDllPath
        };

        AddLog("RealXpeBackend bootstrap started.");
        AddLog($"Requested backend mode = {settings.BackendMode}");
        AddLog($"xpe_common.dll = {_commonDllPath}");
        AddLog($"xpe_display.dll = {_displayDllPath}");
        AddLog($"Display version = {displayVersion}");

        _alerts.Add(new AlertEntry
        {
            Severity = "INFO",
            Code = "REAL_DISPLAY_BACKEND_ACTIVE",
            Message = "Real display backend is active. Modality, VOI, and Presentation LUT are called through P/Invoke.",
            Timestamp = DateTimeOffset.Now
        });

        return _runtimeInfo;
    }

    public string GetVersion() => _runtimeInfo.Version;

    public string GetDisplayVersion() => XpeDisplayNative.GetVersion();

    public LoadedImageFrame LoadRawImage(string path, AppSettings settings)
    {
        AddLog($"LoadRawImage('{path}') invoked.");
        return _rawImageLoader.Load(path, settings);
    }

    // @MX:WARN: [AUTO] Allocates native XpeImageBufferNative via xpe_alloc_image; try/finally ensures xpe_free_image on any exception path
    // @MX:REASON: Native memory is not GC-managed; omitting finally causes heap leak in xpe_common.dll; do not restructure without preserving the try/finally guard
    public LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, AppSettings settings)
    {
        if (rawFrame.RawPixels is null || rawFrame.Width <= 0 || rawFrame.Height <= 0)
        {
            throw new InvalidOperationException("Display pipeline requires a loaded UInt16 raw frame.");
        }

        var count = checked(rawFrame.Width * rawFrame.Height);
        if (rawFrame.RawPixels.Length < count)
        {
            throw new InvalidOperationException("Raw frame pixel array is smaller than width x height.");
        }

        var image = default(XpeImageBufferNative);
        var allocated = false;

        try
        {
            CheckNativeResult(
                XpeCommonNative.xpe_alloc_image(
                    (uint)rawFrame.Width,
                    (uint)rawFrame.Height,
                    XpePixelFormatNative.Float32,
                    out image),
                "xpe_alloc_image");
            allocated = true;

            var floatPixels = new float[count];
            for (var i = 0; i < count; i++)
            {
                floatPixels[i] = rawFrame.RawPixels[i];
            }

            Marshal.Copy(floatPixels, 0, image.Data, count);

            var modality = new XpeModalityLutParamsNative
            {
                Mode = 0,
                RescaleSlope = settings.ModalityRescaleSlope == 0.0f ? 1.0f : settings.ModalityRescaleSlope,
                RescaleIntercept = settings.ModalityRescaleIntercept,
                LutData = IntPtr.Zero,
                LutLength = 0,
                LutFirstMapped = 0,
                LutBitsStored = 16
            };
            CheckNativeResult(XpeDisplayNative.xpe_apply_modality_lut(ref image, ref modality), "xpe_apply_modality_lut");

            var voi = new XpeVoiLutParamsNative
            {
                Mode = ToNativeVoiMode(settings.VoiLutMode),
                Center = settings.VoiWindowCenter,
                Width = Math.Max(1.0f, settings.VoiWindowWidth),
                MinOut = 0.0f,
                MaxOut = 1.0f
            };
            CheckNativeResult(XpeDisplayNative.xpe_apply_voi_lut(ref image, ref voi), "xpe_apply_voi_lut");

            var presentation = XpePresentationLutParamsNative.CreateLinear(settings.GsdfEnabled);
            if (settings.GsdfEnabled)
            {
                var luminanceValues = new[] { 0.05f, 1.0f, 10.0f, 100.0f, 400.0f };
                CheckNativeResult(
                    XpeDisplayNative.xpe_gsdf_calibrate(luminanceValues, (uint)luminanceValues.Length, ref presentation),
                    "xpe_gsdf_calibrate");
            }

            CheckNativeResult(XpeDisplayNative.xpe_apply_presentation_lut(ref image, ref presentation), "xpe_apply_presentation_lut");

            var processedPixels = CopyNativeUInt16Pixels(image.Data, count);
            var processedPreview = CreatePreview(processedPixels, rawFrame.Width, rawFrame.Height);
            var summary = $"CalibrationEval({BuildCalibrationEvaluationSummary(settings)}; preprocess native bridge pending) -> Display: Modality({modality.RescaleSlope:0.###}/{modality.RescaleIntercept:0.###}) -> VOI({NormalizeVoiMode(settings.VoiLutMode)}, C={voi.Center:0.###}, W={voi.Width:0.###}) -> GSDF({(settings.GsdfEnabled ? "on" : "off")})";
            AddLog(summary);

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
        finally
        {
            if (allocated)
            {
                XpeCommonNative.xpe_free_image(ref image);
            }
        }
    }

    public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart)
    {
        var nativeParams = new XpeVoiLutParamsNative();
        CheckNativeResult(
            XpeDisplayNative.xpe_voi_preset_create(ref nativeParams, ToNativeBodyPart(bodyPart)),
            "xpe_voi_preset_create");

        return new VoiPreset(nativeParams.Center, nativeParams.Width, "Linear");
    }

    public int GetAlertCount() => _alerts.Count;

    public AlertEntry? GetAlert(int index) => index >= 0 && index < _alerts.Count ? _alerts[index] : null;

    public int GetLogCount() => _logs.Count;

    public string? GetLog(int index) => index >= 0 && index < _logs.Count ? _logs[index] : null;

    public BackendRuntimeInfo GetRuntimeInfo() => _runtimeInfo;

    public void Shutdown()
    {
        AddLog("RealXpeBackend shutdown requested.");
        _runtimeInfo = new BackendRuntimeInfo
        {
            BackendName = "RealXpeBackend",
            Version = _runtimeInfo.Version,
            State = "Shutdown",
            SupportsNativeRuntime = true,
            NativeDllDetected = File.Exists(_commonDllPath),
            NativeDllPath = _commonDllPath,
            DisplayVersion = _runtimeInfo.DisplayVersion,
            DisplayDllDetected = File.Exists(_displayDllPath),
            DisplayDllPath = _displayDllPath
        };
    }

    private static ushort[] CopyNativeUInt16Pixels(IntPtr source, int count)
    {
        var signedPixels = new short[count];
        Marshal.Copy(source, signedPixels, 0, count);

        var pixels = new ushort[count];
        Buffer.BlockCopy(signedPixels, 0, pixels, 0, count * sizeof(ushort));
        return pixels;
    }

    private static BitmapSource CreatePreview(IReadOnlyList<ushort> pixels, int width, int height)
    {
        ushort minValue = ushort.MaxValue;
        ushort maxValue = ushort.MinValue;
        for (var i = 0; i < pixels.Count; i++)
        {
            var sample = pixels[i];
            if (sample < minValue)
            {
                minValue = sample;
            }

            if (sample > maxValue)
            {
                maxValue = sample;
            }
        }

        var scale = Math.Max(1, maxValue - minValue);
        var grayscale = new byte[pixels.Count];
        for (var i = 0; i < pixels.Count; i++)
        {
            grayscale[i] = (byte)(((pixels[i] - minValue) * 255) / scale);
        }

        var preview = BitmapSource.Create(
            width,
            height,
            96,
            96,
            PixelFormats.Gray8,
            null,
            grayscale,
            width);
        preview.Freeze();
        return preview;
    }

    // @MX:NOTE: [AUTO] XPE error convention: 0 = Ok, negative = error code; positive values reserved for future warnings
    private static void CheckNativeResult(int code, string functionName)
    {
        if (code < (int)XpeErrorCodeNative.Ok)
        {
            throw new InvalidOperationException($"{functionName} failed with XPE error code {code}.");
        }
    }

    private static int ToNativeVoiMode(string mode) => NormalizeVoiMode(mode) switch
    {
        "LinearExact" => 1,
        "Sigmoid" => 2,
        _ => 0
    };

    private static string NormalizeVoiMode(string mode)
    {
        if (string.Equals(mode, "LinearExact", StringComparison.OrdinalIgnoreCase))
        {
            return "LinearExact";
        }

        if (string.Equals(mode, "Sigmoid", StringComparison.OrdinalIgnoreCase))
        {
            return "Sigmoid";
        }

        return "Linear";
    }

    private static string BuildCalibrationEvaluationSummary(AppSettings settings)
    {
        return $"Offset={settings.OffsetCorrectionMode}, Gain={settings.GainCorrectionMode}, Defect={settings.DefectCorrectionMode}, " +
               $"Ghost={settings.GhostCorrectionMode}, Temp={settings.TemperatureCompensationMode}, " +
               $"Nonlinearity={settings.NonlinearityCorrectionMode}, Binning={settings.BinningCorrectionMode}";
    }

    private static XpeBodyPartEnumNative ToNativeBodyPart(XpeBodyPartEnum bodyPart) => bodyPart switch
    {
        XpeBodyPartEnum.Bone => XpeBodyPartEnumNative.Bone,
        XpeBodyPartEnum.Lung => XpeBodyPartEnumNative.Lung,
        XpeBodyPartEnum.Abdomen => XpeBodyPartEnumNative.Abdomen,
        XpeBodyPartEnum.Head => XpeBodyPartEnumNative.Head,
        _ => throw new ArgumentOutOfRangeException(nameof(bodyPart), bodyPart, "Unsupported body part preset.")
    };

    private void AddLog(string message)
    {
        _logs.Add($"[{DateTimeOffset.Now:HH:mm:ss.fff}] {message}");
    }

    private static bool HasExports(string dllPath, IEnumerable<string> requiredExports)
    {
        if (!File.Exists(dllPath))
        {
            return false;
        }

        if (!NativeLibrary.TryLoad(dllPath, out var handle))
        {
            return false;
        }

        try
        {
            foreach (var exportName in requiredExports)
            {
                if (!NativeLibrary.TryGetExport(handle, exportName, out _))
                {
                    return false;
                }
            }

            return true;
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }
}
