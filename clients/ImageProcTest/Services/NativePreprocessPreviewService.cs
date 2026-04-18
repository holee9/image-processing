using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Media.Imaging;

namespace ImageProcTest
{
    internal enum PreprocessStageMode
    {
        Off,
        On,
        Auto
    }

    internal sealed record PreprocessStageSelection(
        PreprocessStageMode Offset,
        PreprocessStageMode Gain,
        PreprocessStageMode Defect)
    {
        public bool HasAnyStage => Offset != PreprocessStageMode.Off ||
            Gain != PreprocessStageMode.Off ||
            Defect != PreprocessStageMode.Off;
    }

    internal sealed record NativePreviewStageResult(
        string Stage,
        string ErrorCode,
        double LatencyMs,
        bool Executed,
        string Details = "");

    internal sealed record NativePreviewCalibrationResult(
        string Stage,
        string Status,
        bool Loaded,
        string? SourceRawPath,
        string? XCalPath,
        double LatencyMs,
        string Details,
        NativePreviewCalibrationExpiryResult? Expiry);

    internal sealed record NativePreviewCalibrationExpiryResult(
        string Status,
        bool Checked,
        bool Expired,
        ulong ExpiryEpochMs,
        string? ExpiryUtc,
        double? RemainingDays,
        double LatencyMs,
        string Details);

    internal sealed record NativePreviewMetrics(
        double MeanAbsoluteDelta,
        double Rmse,
        double MaxAbsoluteDelta,
        int ChangedPixels,
        int PixelCount,
        double ChangedPixelRatio,
        bool InputPreserved,
        int NaNInfCount);

    internal sealed record NativePreprocessPreviewResult(
        string DllPath,
        string ArtifactDirectory,
        IReadOnlyList<NativePreviewCalibrationResult> CalibrationLoads,
        IReadOnlyList<NativePreviewStageResult> Stages,
        NativePreviewMetrics Metrics,
        double TotalLatencyMs,
        double OutputMin,
        double OutputMax,
        WriteableBitmap Bitmap);

    internal static class NativePreprocessPreviewService
    {
        private const uint XCalVersion = 1;
        private const uint XCalTypeOffset = 0;
        private const uint XCalTypeGain = 1;
        private const uint XCalTypeDefect = 2;
        private const uint XCalFormatFloat32 = 1;
        private const uint XCalFormatUInt8Mask = 2;

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode InitDelegate(IntPtr config);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void ShutdownDelegate();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode CalibrationLoadDelegate(IntPtr path);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode CalibrationExpiryDelegate(
            IntPtr path,
            out ulong expiryEpochMs);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode CorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer input,
            ref XpeCommonApi.XpeImageBuffer output,
            ref XpeCommonApi.XpeImageMetadata metadata);

        private sealed record CalibrationRequest(
            string Stage,
            CalibrationRole Role,
            CalibrationFileDescriptor Source,
            string XCalPath,
            string Details);

        private sealed record PreparedCalibration(
            IReadOnlyList<CalibrationRequest> Requests,
            IReadOnlyList<NativePreviewCalibrationResult> MissingLoads,
            string ArtifactDirectory);

        public static NativePreprocessPreviewResult Run(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
            FixtureCaseInfo? fixtureCase,
            string? preferredDllPath)
        {
            if (!selection.HasAnyStage)
            {
                throw new InvalidOperationException("Select at least one preprocess stage before running native preview.");
            }

            var dllPath = ResolveDllPath(preferredDllPath);
            if (dllPath is null)
            {
                throw new FileNotFoundException("xpe_preprocess.dll was not found in known locations.");
            }

            var preparedCalibration = PrepareCalibrationFiles(preview, selection, fixtureCase);

            if (!NativeLibrary.TryLoad(dllPath, out var handle))
            {
                throw new InvalidOperationException($"Failed to load {dllPath}.");
            }

            try
            {
                var init = GetRequiredDelegate<InitDelegate>(handle, "xpe_preprocess_init");
                var shutdown = GetRequiredDelegate<ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
                var loadOffset = GetRequiredDelegate<CalibrationLoadDelegate>(handle, "xpe_calib_load_offset");
                var loadGain = GetRequiredDelegate<CalibrationLoadDelegate>(handle, "xpe_calib_load_gain");
                var loadDefect = GetRequiredDelegate<CalibrationLoadDelegate>(handle, "xpe_calib_load_defect_map");
                var checkExpiry = GetRequiredDelegate<CalibrationExpiryDelegate>(handle, "xpe_calib_check_expiry");
                var offsetCorrect = GetRequiredDelegate<CorrectionDelegate>(handle, "xpe_offset_correct");
                var gainCorrect = GetRequiredDelegate<CorrectionDelegate>(handle, "xpe_gain_correct");
                var defectCorrect = GetRequiredDelegate<CorrectionDelegate>(handle, "xpe_defect_correct");

                shutdown();
                var initResult = init(IntPtr.Zero);
                if (initResult != XpeCommonApi.XpeErrorCode.OK)
                {
                    throw new InvalidOperationException($"xpe_preprocess_init(NULL) returned {initResult}.");
                }

                try
                {
                    var loadResults = LoadCalibrationFiles(
                        preparedCalibration,
                        loadOffset,
                        loadGain,
                        loadDefect,
                        checkExpiry);

                    return RunChain(
                        preview,
                        selection,
                        dllPath,
                        preparedCalibration.ArtifactDirectory,
                        loadResults,
                        offsetCorrect,
                        gainCorrect,
                        defectCorrect);
                }
                finally
                {
                    shutdown();
                }
            }
            finally
            {
                NativeLibrary.Free(handle);
            }
        }

        private static PreparedCalibration PrepareCalibrationFiles(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
            FixtureCaseInfo? fixtureCase)
        {
            var timestamp = DateTimeOffset.UtcNow.ToString("yyyyMMdd-HHmmss-fff");
            var safeCaseName = fixtureCase?.Name ?? "manual";
            var artifactDirectory = Path.Combine(
                AppContext.BaseDirectory,
                "fixture-preview-artifacts",
                $"{safeCaseName}-{timestamp}");
            Directory.CreateDirectory(artifactDirectory);

            var requests = new List<CalibrationRequest>();
            var missingLoads = new List<NativePreviewCalibrationResult>();
            var offsetSource = FindCalibration(fixtureCase, CalibrationRole.Offset);

            AddCalibrationRequest(
                preview,
                selection.Offset,
                fixtureCase,
                CalibrationRole.Offset,
                "offset",
                artifactDirectory,
                requests,
                missingLoads,
                source => GenerateOffsetXCal(preview, source, Path.Combine(artifactDirectory, "offset.xcal")));

            AddCalibrationRequest(
                preview,
                selection.Gain,
                fixtureCase,
                CalibrationRole.Gain,
                "gain",
                artifactDirectory,
                requests,
                missingLoads,
                source => GenerateGainXCal(preview, source, offsetSource, Path.Combine(artifactDirectory, "gain.xcal")));

            AddCalibrationRequest(
                preview,
                selection.Defect,
                fixtureCase,
                CalibrationRole.Defect,
                "defect",
                artifactDirectory,
                requests,
                missingLoads,
                source => GenerateDefectXCal(preview, source, Path.Combine(artifactDirectory, "defect.xcal")));

            return new PreparedCalibration(requests, missingLoads, artifactDirectory);
        }

        private static void AddCalibrationRequest(
            RawPreviewResult preview,
            PreprocessStageMode mode,
            FixtureCaseInfo? fixtureCase,
            CalibrationRole role,
            string stage,
            string artifactDirectory,
            List<CalibrationRequest> requests,
            List<NativePreviewCalibrationResult> missingLoads,
            Func<CalibrationFileDescriptor, (string XCalPath, string Details)> generate)
        {
            if (mode == PreprocessStageMode.Off)
            {
                return;
            }

            var source = FindCalibration(fixtureCase, role);
            if (source is null)
            {
                var modeText = mode == PreprocessStageMode.Auto ? "Auto skipped" : "On requested but blocked";
                missingLoads.Add(new NativePreviewCalibrationResult(
                    stage,
                    "Missing",
                    Loaded: false,
                    SourceRawPath: null,
                    XCalPath: null,
                    LatencyMs: 0,
                    Details: $"{modeText}: no {role} calibration file exists in the selected fixture case.",
                    Expiry: null));
                return;
            }

            var (xcalPath, details) = generate(source);
            if (!File.Exists(xcalPath))
            {
                throw new FileNotFoundException($"Generated {stage} XCal file was not found.", xcalPath);
            }

            requests.Add(new CalibrationRequest(stage, role, source, xcalPath, details));
        }

        private static IReadOnlyList<NativePreviewCalibrationResult> LoadCalibrationFiles(
            PreparedCalibration prepared,
            CalibrationLoadDelegate loadOffset,
            CalibrationLoadDelegate loadGain,
            CalibrationLoadDelegate loadDefect,
            CalibrationExpiryDelegate checkExpiry)
        {
            var results = new List<NativePreviewCalibrationResult>(prepared.MissingLoads);

            foreach (var request in prepared.Requests)
            {
                var load = request.Role switch
                {
                    CalibrationRole.Offset => loadOffset,
                    CalibrationRole.Gain => loadGain,
                    CalibrationRole.Defect => loadDefect,
                    _ => throw new InvalidOperationException($"Unsupported calibration role {request.Role}.")
                };

                var expiry = CheckCalibrationExpiry(checkExpiry, request.XCalPath);
                var stopwatch = Stopwatch.StartNew();
                var result = CallCalibrationLoad(load, request.XCalPath);
                stopwatch.Stop();

                var loadResult = new NativePreviewCalibrationResult(
                    request.Stage,
                    result.ToString(),
                    Loaded: result == XpeCommonApi.XpeErrorCode.OK,
                    request.Source.Path,
                    request.XCalPath,
                    stopwatch.Elapsed.TotalMilliseconds,
                    request.Details,
                    expiry);
                results.Add(loadResult);

                if (result != XpeCommonApi.XpeErrorCode.OK)
                {
                    throw new InvalidOperationException(
                        $"{request.Stage} calibration load failed with {result}: {request.Source.Path}");
                }
            }

            return results;
        }

        private static NativePreprocessPreviewResult RunChain(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
            string dllPath,
            string artifactDirectory,
            IReadOnlyList<NativePreviewCalibrationResult> calibrationLoads,
            CorrectionDelegate offsetCorrect,
            CorrectionDelegate gainCorrect,
            CorrectionDelegate defectCorrect)
        {
            var stages = new List<NativePreviewStageResult>();
            var stopwatch = Stopwatch.StartNew();
            var metadata = CreateMetadata();
            var currentUInt16 = preview.SampledPixels.ToArray();
            float[]? currentFloat = null;

            if (selection.Offset != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "offset"))
            {
                var output = new ushort[currentUInt16.Length];
                stages.Add(CallStage(
                    "offset",
                    () => CallUInt16ToUInt16(offsetCorrect, currentUInt16, output, preview.PreviewWidth, preview.PreviewHeight, ref metadata),
                    GetLoadDetails(calibrationLoads, "offset")));
                currentUInt16 = output;
            }
            else
            {
                stages.Add(CreateSkippedStage("offset", selection.Offset, calibrationLoads));
            }

            if (selection.Gain != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "gain"))
            {
                var output = new float[currentUInt16.Length];
                stages.Add(CallStage(
                    "gain",
                    () => CallUInt16ToFloat(gainCorrect, currentUInt16, output, preview.PreviewWidth, preview.PreviewHeight, ref metadata),
                    GetLoadDetails(calibrationLoads, "gain")));
                currentFloat = output;
            }
            else
            {
                stages.Add(CreateSkippedStage("gain", selection.Gain, calibrationLoads));
            }

            if (selection.Defect != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "defect"))
            {
                currentFloat ??= currentUInt16.Select(value => (float)value).ToArray();
                var output = new float[currentFloat.Length];
                stages.Add(CallStage(
                    "defect",
                    () => CallFloatToFloat(defectCorrect, currentFloat, output, preview.PreviewWidth, preview.PreviewHeight, ref metadata),
                    GetLoadDetails(calibrationLoads, "defect")));
                currentFloat = output;
            }
            else
            {
                stages.Add(CreateSkippedStage("defect", selection.Defect, calibrationLoads));
            }

            stopwatch.Stop();

            if (!stages.Any(stage => stage.Executed))
            {
                throw new InvalidOperationException("No native preprocessing stage executed. Select a fixture case that contains matching calibration files.");
            }

            var finalValues = currentFloat ?? currentUInt16.Select(value => (float)value).ToArray();
            var metrics = ComputeMetrics(preview.SampledPixels, finalValues);
            var (outputMin, outputMax) = ComputeMinMax(finalValues);
            var bitmap = RawPreviewService.CreateGray8Bitmap(
                finalValues,
                preview.PreviewWidth,
                preview.PreviewHeight,
                preview.MinValue,
                preview.MaxValue);

            return new NativePreprocessPreviewResult(
                dllPath,
                artifactDirectory,
                calibrationLoads,
                stages,
                metrics,
                stopwatch.Elapsed.TotalMilliseconds,
                outputMin,
                outputMax,
                bitmap);
        }

        private static NativePreviewStageResult CallStage(
            string stage,
            Func<XpeCommonApi.XpeErrorCode> call,
            string details)
        {
            var stopwatch = Stopwatch.StartNew();
            var result = call();
            stopwatch.Stop();

            if (result != XpeCommonApi.XpeErrorCode.OK)
            {
                throw new InvalidOperationException($"{stage} returned {result}.");
            }

            return new NativePreviewStageResult(
                stage,
                result.ToString(),
                stopwatch.Elapsed.TotalMilliseconds,
                Executed: true,
                details);
        }

        private static NativePreviewStageResult CreateSkippedStage(
            string stage,
            PreprocessStageMode mode,
            IReadOnlyList<NativePreviewCalibrationResult> calibrationLoads)
        {
            if (mode == PreprocessStageMode.Off)
            {
                return new NativePreviewStageResult(stage, "Skipped", 0, Executed: false, "Stage mode is Off.");
            }

            var load = calibrationLoads.FirstOrDefault(item => item.Stage == stage);
            return new NativePreviewStageResult(
                stage,
                load?.Status ?? "Missing",
                0,
                Executed: false,
                load?.Details ?? "No matching calibration was loaded.");
        }

        private static bool IsLoaded(IReadOnlyList<NativePreviewCalibrationResult> calibrationLoads, string stage)
        {
            return calibrationLoads.Any(load => load.Stage == stage && load.Loaded);
        }

        private static string GetLoadDetails(IReadOnlyList<NativePreviewCalibrationResult> calibrationLoads, string stage)
        {
            return calibrationLoads.FirstOrDefault(load => load.Stage == stage && load.Loaded)?.Details ?? "";
        }

        private static (string XCalPath, string Details) GenerateOffsetXCal(
            RawPreviewResult targetPreview,
            CalibrationFileDescriptor source,
            string xcalPath)
        {
            var offsetPreview = LoadMatchingCalibrationPreview(source, targetPreview);
            var values = new float[offsetPreview.SampledPixels.Length];
            for (var i = 0; i < values.Length; i++)
            {
                values[i] = offsetPreview.SampledPixels[i];
            }

            WriteXCalFloat32(xcalPath, XCalTypeOffset, targetPreview.PreviewWidth, targetPreview.PreviewHeight, values);
            return (xcalPath, $"Offset calibration generated from {source.Name}; raw range={offsetPreview.MinValue}..{offsetPreview.MaxValue}.");
        }

        private static (string XCalPath, string Details) GenerateGainXCal(
            RawPreviewResult targetPreview,
            CalibrationFileDescriptor source,
            CalibrationFileDescriptor? offsetSource,
            string xcalPath)
        {
            var gainPreview = LoadMatchingCalibrationPreview(source, targetPreview);
            ushort[]? offsetPixels = null;
            if (offsetSource is not null)
            {
                offsetPixels = LoadMatchingCalibrationPreview(offsetSource, targetPreview).SampledPixels;
            }

            var flat = new float[gainPreview.SampledPixels.Length];
            var sum = 0.0;
            for (var i = 0; i < flat.Length; i++)
            {
                var dark = offsetPixels is null ? 0.0f : offsetPixels[i];
                var value = Math.Max(1.0f, gainPreview.SampledPixels[i] - dark);
                flat[i] = value;
                sum += value;
            }

            var mean = Math.Max(1.0, sum / Math.Max(1, flat.Length));
            var gainMap = new float[flat.Length];
            for (var i = 0; i < gainMap.Length; i++)
            {
                gainMap[i] = Math.Clamp((float)(flat[i] / mean), 0.001f, 1000.0f);
            }

            WriteXCalFloat32(xcalPath, XCalTypeGain, targetPreview.PreviewWidth, targetPreview.PreviewHeight, gainMap);
            var offsetNote = offsetSource is null ? "without dark subtraction" : $"dark-subtracted with {offsetSource.Name}";
            return (xcalPath, $"Gain calibration generated from {source.Name}; {offsetNote}; normalized mean={mean:0.###}.");
        }

        private static (string XCalPath, string Details) GenerateDefectXCal(
            RawPreviewResult targetPreview,
            CalibrationFileDescriptor source,
            string xcalPath)
        {
            var defectPreview = LoadMatchingCalibrationPreview(source, targetPreview);
            var mask = new byte[defectPreview.SampledPixels.Length];
            var nonZeroCount = defectPreview.SampledPixels.Count(value => value != 0);
            var nonZeroRatio = nonZeroCount / (double)Math.Max(1, defectPreview.SampledPixels.Length);
            var invertMask = nonZeroRatio > 0.5;
            var defectCount = 0;

            for (var i = 0; i < mask.Length; i++)
            {
                var defective = invertMask
                    ? defectPreview.SampledPixels[i] == 0
                    : defectPreview.SampledPixels[i] != 0;
                mask[i] = defective ? (byte)1 : (byte)0;
                if (defective)
                {
                    defectCount++;
                }
            }

            WriteXCalMask(xcalPath, targetPreview.PreviewWidth, targetPreview.PreviewHeight, mask);
            return (
                xcalPath,
                $"Defect calibration generated from {source.Name}; defect pixels={defectCount}/{mask.Length}; nonZeroRatio={nonZeroRatio:0.###}; inverted={invertMask}.");
        }

        private static RawPreviewResult LoadMatchingCalibrationPreview(
            CalibrationFileDescriptor source,
            RawPreviewResult targetPreview)
        {
            var maxSide = Math.Max(targetPreview.PreviewWidth, targetPreview.PreviewHeight);
            var calibrationPreview = RawPreviewService.LoadUInt16Preview(source.Path, maxSide);
            if (calibrationPreview.PreviewWidth != targetPreview.PreviewWidth ||
                calibrationPreview.PreviewHeight != targetPreview.PreviewHeight)
            {
                throw new InvalidDataException(
                    $"{source.Role} calibration dimensions do not match the selected image preview. " +
                    $"Calibration={calibrationPreview.PreviewWidth}x{calibrationPreview.PreviewHeight}, " +
                    $"image={targetPreview.PreviewWidth}x{targetPreview.PreviewHeight}.");
            }

            return calibrationPreview;
        }

        private static CalibrationFileDescriptor? FindCalibration(FixtureCaseInfo? fixtureCase, CalibrationRole role)
        {
            return fixtureCase?.CalibrationFiles
                .Where(file => file.Role == role)
                .OrderBy(file => GetCalibrationPriority(file, role))
                .ThenBy(file => file.Name, StringComparer.OrdinalIgnoreCase)
                .FirstOrDefault();
        }

        private static int GetCalibrationPriority(CalibrationFileDescriptor file, CalibrationRole role)
        {
            var name = Path.GetFileNameWithoutExtension(file.Name).ToLowerInvariant();
            return role switch
            {
                CalibrationRole.Offset when name == "cdark" => 0,
                CalibrationRole.Offset when name.Contains("dark", StringComparison.Ordinal) => 1,
                CalibrationRole.Gain when name.StartsWith("cbr", StringComparison.Ordinal) => 0,
                CalibrationRole.Gain when name.StartsWith("calset", StringComparison.Ordinal) => 1,
                CalibrationRole.Defect when name == "bpm" => 0,
                CalibrationRole.Defect when name.EndsWith("_bpm", StringComparison.Ordinal) => 1,
                CalibrationRole.Defect when name.EndsWith("_bpmall", StringComparison.Ordinal) => 2,
                _ => 10
            };
        }

        private static XpeCommonApi.XpeErrorCode CallCalibrationLoad(
            CalibrationLoadDelegate load,
            string path)
        {
            var pathPointer = Marshal.StringToHGlobalAnsi(path);
            try
            {
                return load(pathPointer);
            }
            finally
            {
                Marshal.FreeHGlobal(pathPointer);
            }
        }

        private static NativePreviewCalibrationExpiryResult CheckCalibrationExpiry(
            CalibrationExpiryDelegate checkExpiry,
            string path)
        {
            var stopwatch = Stopwatch.StartNew();
            try
            {
                var result = CallCalibrationExpiry(checkExpiry, path, out var expiryEpochMs);
                stopwatch.Stop();
                var expiry = ConvertExpiryEpoch(expiryEpochMs);
                var checkedResult = result == XpeCommonApi.XpeErrorCode.OK ||
                    result == XpeCommonApi.XpeErrorCode.CALIBRATION_EXPIRED;
                var expired = result == XpeCommonApi.XpeErrorCode.CALIBRATION_EXPIRED ||
                    (expiry.ExpiresAtUtc is not null && expiry.ExpiresAtUtc.Value <= DateTimeOffset.UtcNow);
                var details = expiry.ExpiresAtUtc is null
                    ? "Calibration expiry epoch could not be converted."
                    : $"Calibration expires at {expiry.ExpiresAtUtc.Value:O}.";

                return new NativePreviewCalibrationExpiryResult(
                    result.ToString(),
                    checkedResult,
                    expired,
                    expiryEpochMs,
                    expiry.ExpiresAtUtc?.ToString("O"),
                    expiry.RemainingDays,
                    stopwatch.Elapsed.TotalMilliseconds,
                    details);
            }
            catch (Exception ex)
            {
                stopwatch.Stop();
                return new NativePreviewCalibrationExpiryResult(
                    "Exception",
                    Checked: false,
                    Expired: false,
                    ExpiryEpochMs: 0,
                    ExpiryUtc: null,
                    RemainingDays: null,
                    LatencyMs: stopwatch.Elapsed.TotalMilliseconds,
                    Details: $"xpe_calib_check_expiry failed: {ex.Message}");
            }
        }

        private static XpeCommonApi.XpeErrorCode CallCalibrationExpiry(
            CalibrationExpiryDelegate checkExpiry,
            string path,
            out ulong expiryEpochMs)
        {
            var pathPointer = Marshal.StringToHGlobalAnsi(path);
            try
            {
                return checkExpiry(pathPointer, out expiryEpochMs);
            }
            finally
            {
                Marshal.FreeHGlobal(pathPointer);
            }
        }

        private static (DateTimeOffset? ExpiresAtUtc, double? RemainingDays) ConvertExpiryEpoch(
            ulong expiryEpochMs)
        {
            if (expiryEpochMs > long.MaxValue)
            {
                return (null, null);
            }

            try
            {
                var expiresAtUtc = DateTimeOffset.FromUnixTimeMilliseconds((long)expiryEpochMs);
                return (expiresAtUtc, (expiresAtUtc - DateTimeOffset.UtcNow).TotalDays);
            }
            catch (ArgumentOutOfRangeException)
            {
                return (null, null);
            }
        }

        private static void WriteXCalFloat32(
            string path,
            uint type,
            int width,
            int height,
            ReadOnlySpan<float> values)
        {
            var payload = new byte[checked(values.Length * sizeof(float))];
            MemoryMarshal.AsBytes(values).CopyTo(payload);
            WriteXCal(path, type, XCalFormatFloat32, width, height, payload);
        }

        private static void WriteXCalMask(
            string path,
            int width,
            int height,
            byte[] mask)
        {
            WriteXCal(path, XCalTypeDefect, XCalFormatUInt8Mask, width, height, mask);
        }

        private static void WriteXCal(
            string path,
            uint type,
            uint pixelFormat,
            int width,
            int height,
            byte[] payload)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? AppContext.BaseDirectory);
            var sha256 = SHA256.HashData(payload);
            var nowMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
            var sessionId = new byte[64];
            var sessionBytes = Encoding.ASCII.GetBytes("gui-preview");
            Array.Copy(sessionBytes, sessionId, sessionBytes.Length);

            using var stream = File.Create(path);
            using var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: false);
            writer.Write(Encoding.ASCII.GetBytes("XCAL"));
            writer.Write(XCalVersion);
            writer.Write(type);
            writer.Write(pixelFormat);
            writer.Write(checked((uint)width));
            writer.Write(checked((uint)height));
            writer.Write(nowMs);
            writer.Write(0L);
            writer.Write(sessionId);
            writer.Write(0UL);
            writer.Write(checked((ulong)payload.Length));
            writer.Write(sha256);
            writer.Write(payload);
        }

        private static XpeCommonApi.XpeErrorCode CallUInt16ToUInt16(
            CorrectionDelegate correction,
            ushort[] input,
            ushort[] output,
            int width,
            int height,
            ref XpeCommonApi.XpeImageMetadata metadata)
        {
            return CallPinned(
                correction,
                input,
                output,
                CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, input.Length * sizeof(ushort)),
                CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, output.Length * sizeof(ushort)),
                ref metadata);
        }

        private static XpeCommonApi.XpeErrorCode CallUInt16ToFloat(
            CorrectionDelegate correction,
            ushort[] input,
            float[] output,
            int width,
            int height,
            ref XpeCommonApi.XpeImageMetadata metadata)
        {
            return CallPinned(
                correction,
                input,
                output,
                CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, input.Length * sizeof(ushort)),
                CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, output.Length * sizeof(float)),
                ref metadata);
        }

        private static XpeCommonApi.XpeErrorCode CallFloatToFloat(
            CorrectionDelegate correction,
            float[] input,
            float[] output,
            int width,
            int height,
            ref XpeCommonApi.XpeImageMetadata metadata)
        {
            return CallPinned(
                correction,
                input,
                output,
                CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, input.Length * sizeof(float)),
                CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, output.Length * sizeof(float)),
                ref metadata);
        }

        private static XpeCommonApi.XpeErrorCode CallPinned<TInput, TOutput>(
            CorrectionDelegate correction,
            TInput[] input,
            TOutput[] output,
            XpeCommonApi.XpeImageBuffer inputBuffer,
            XpeCommonApi.XpeImageBuffer outputBuffer,
            ref XpeCommonApi.XpeImageMetadata metadata)
            where TInput : struct
            where TOutput : struct
        {
            var inputHandle = GCHandle.Alloc(input, GCHandleType.Pinned);
            var outputHandle = GCHandle.Alloc(output, GCHandleType.Pinned);

            try
            {
                inputBuffer.Data = inputHandle.AddrOfPinnedObject();
                outputBuffer.Data = outputHandle.AddrOfPinnedObject();
                return correction(ref inputBuffer, ref outputBuffer, ref metadata);
            }
            finally
            {
                inputHandle.Free();
                outputHandle.Free();
            }
        }

        private static XpeCommonApi.XpeImageBuffer CreateBuffer(
            int width,
            int height,
            XpeCommonApi.XpePixelFormat format,
            int dataSize)
        {
            return new XpeCommonApi.XpeImageBuffer
            {
                Width = checked((uint)width),
                Height = checked((uint)height),
                BitsAllocated = format == XpeCommonApi.XpePixelFormat.UInt16 ? 16u : 32u,
                BitsStored = format == XpeCommonApi.XpePixelFormat.UInt16 ? 16u : 32u,
                Format = format,
                DataSize = (nuint)dataSize
            };
        }

        private static XpeCommonApi.XpeImageMetadata CreateMetadata()
        {
            return new XpeCommonApi.XpeImageMetadata
            {
                BodyPart = "CHEST",
                KVp = 120.0f,
                MAs = 10.0f,
                SID_mm = 1200.0f,
                PixelPitch_mm = 0.143f,
                AcquisitionTime = 0,
                Flags = 0
            };
        }

        private static NativePreviewMetrics ComputeMetrics(
            ReadOnlySpan<ushort> original,
            ReadOnlySpan<float> output)
        {
            if (original.Length != output.Length)
            {
                throw new ArgumentException("Original and output buffers must have the same length.");
            }

            var changed = 0;
            var nanInf = 0;
            var absSum = 0.0;
            var sqSum = 0.0;
            var maxAbs = 0.0;

            for (var i = 0; i < output.Length; i++)
            {
                var value = output[i];
                if (!float.IsFinite(value))
                {
                    nanInf++;
                    value = 0;
                }

                var delta = value - original[i];
                var abs = Math.Abs(delta);
                absSum += abs;
                sqSum += delta * delta;
                maxAbs = Math.Max(maxAbs, abs);
                if (abs > 0.5)
                {
                    changed++;
                }
            }

            var count = Math.Max(1, output.Length);
            return new NativePreviewMetrics(
                absSum / count,
                Math.Sqrt(sqSum / count),
                maxAbs,
                changed,
                output.Length,
                changed / (double)count,
                InputPreserved: changed == 0 && nanInf == 0,
                nanInf);
        }

        private static (double Min, double Max) ComputeMinMax(ReadOnlySpan<float> values)
        {
            var min = double.PositiveInfinity;
            var max = double.NegativeInfinity;
            for (var i = 0; i < values.Length; i++)
            {
                var value = values[i];
                if (!float.IsFinite(value))
                {
                    continue;
                }

                min = Math.Min(min, value);
                max = Math.Max(max, value);
            }

            if (!double.IsFinite(min) || !double.IsFinite(max))
            {
                return (0, 1);
            }

            return (min, max);
        }

        private static TDelegate GetRequiredDelegate<TDelegate>(IntPtr handle, string exportName)
            where TDelegate : Delegate
        {
            if (!NativeLibrary.TryGetExport(handle, exportName, out var symbol))
            {
                throw new EntryPointNotFoundException($"{exportName} was not found in xpe_preprocess.dll.");
            }

            return Marshal.GetDelegateForFunctionPointer<TDelegate>(symbol);
        }

        private static string? ResolveDllPath(string? preferredDllPath)
        {
            if (!string.IsNullOrWhiteSpace(preferredDllPath) &&
                !string.Equals(preferredDllPath, XpePreprocessLibraryLocator.DllName, StringComparison.OrdinalIgnoreCase) &&
                File.Exists(preferredDllPath))
            {
                return preferredDllPath;
            }

            return XpePreprocessLibraryLocator.TryFindDll();
        }
    }
}
