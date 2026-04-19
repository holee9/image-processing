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

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode InitDelegate(IntPtr config);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void ShutdownDelegate();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode CalibrationExpiryDelegate(
            IntPtr path,
            IntPtr firstOut,
            IntPtr secondOut);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode OffsetCorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeCommonApi.XpeImageBuffer offsetMap);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode GainCorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeCommonApi.XpeImageBuffer gainMap);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode DefectCorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer image,
            ref XpeCommonApi.XpeImageBuffer defectMap,
            IntPtr config);

        private sealed record CalibrationRequest(
            string Stage,
            CalibrationRole Role,
            CalibrationFileDescriptor Source,
            string XCalPath,
            string Details,
            ushort[]? OffsetMap,
            float[]? GainMap,
            byte[]? DefectMap);

        private sealed record GeneratedCalibration(
            string XCalPath,
            string Details,
            ushort[]? OffsetMap = null,
            float[]? GainMap = null,
            byte[]? DefectMap = null);

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

            NativeDependencyLoader.TryLoadFor(dllPath);
            if (!NativeLibrary.TryLoad(dllPath, out var handle))
            {
                throw new InvalidOperationException($"Failed to load {dllPath}.");
            }

            try
            {
                var init = GetRequiredDelegate<InitDelegate>(handle, "xpe_preprocess_init");
                var shutdown = GetRequiredDelegate<ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
                var checkExpiry = GetRequiredDelegate<CalibrationExpiryDelegate>(handle, "xpe_calib_check_expiry");
                var offsetCorrect = GetRequiredDelegate<OffsetCorrectionDelegate>(handle, "xpe_offset_correct");
                var gainCorrect = GetRequiredDelegate<GainCorrectionDelegate>(handle, "xpe_gain_correct");
                var defectCorrect = GetRequiredDelegate<DefectCorrectionDelegate>(handle, "xpe_defect_correct");

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
                        checkExpiry);

                    return RunChain(
                        preview,
                        selection,
                        dllPath,
                        preparedCalibration.ArtifactDirectory,
                        loadResults,
                        preparedCalibration.Requests,
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
            Func<CalibrationFileDescriptor, GeneratedCalibration> generate)
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

            var generated = generate(source);
            if (!File.Exists(generated.XCalPath))
            {
                throw new FileNotFoundException($"Generated {stage} XCal file was not found.", generated.XCalPath);
            }

            requests.Add(new CalibrationRequest(
                stage,
                role,
                source,
                generated.XCalPath,
                generated.Details,
                generated.OffsetMap,
                generated.GainMap,
                generated.DefectMap));
        }

        private static IReadOnlyList<NativePreviewCalibrationResult> LoadCalibrationFiles(
            PreparedCalibration prepared,
            CalibrationExpiryDelegate checkExpiry)
        {
            var results = new List<NativePreviewCalibrationResult>(prepared.MissingLoads);

            foreach (var request in prepared.Requests)
            {
                var expiry = CheckCalibrationExpiry(checkExpiry, request.XCalPath);
                var stopwatch = Stopwatch.StartNew();
                var result = File.Exists(request.XCalPath)
                    ? XpeCommonApi.XpeErrorCode.OK
                    : XpeCommonApi.XpeErrorCode.IO_FAILED;
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
                        $"{request.Stage} calibration preparation failed with {result}: {request.Source.Path}");
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
            IReadOnlyList<CalibrationRequest> calibrationRequests,
            OffsetCorrectionDelegate offsetCorrect,
            GainCorrectionDelegate gainCorrect,
            DefectCorrectionDelegate defectCorrect)
        {
            var stages = new List<NativePreviewStageResult>();
            var stopwatch = Stopwatch.StartNew();
            var metadata = CreateMetadata();
            var currentUInt16 = preview.SampledPixels.ToArray();
            float[]? currentFloat = null;

            if (selection.Offset != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "offset"))
            {
                var offsetMap = GetRequiredCalibration(calibrationRequests, "offset").OffsetMap ??
                    throw new InvalidOperationException("Offset calibration map was not prepared.");
                stages.Add(CallStage(
                    "offset",
                    () => CallOffset(offsetCorrect, currentUInt16, offsetMap, preview.PreviewWidth, preview.PreviewHeight),
                    GetLoadDetails(calibrationLoads, "offset")));
            }
            else
            {
                stages.Add(CreateSkippedStage("offset", selection.Offset, calibrationLoads));
            }

            if (selection.Gain != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "gain"))
            {
                var gainMap = GetRequiredCalibration(calibrationRequests, "gain").GainMap ??
                    throw new InvalidOperationException("Gain calibration map was not prepared.");
                stages.Add(CallStage(
                    "gain",
                    () => CallGain(gainCorrect, currentUInt16, gainMap, preview.PreviewWidth, preview.PreviewHeight, out currentFloat),
                    GetLoadDetails(calibrationLoads, "gain")));
            }
            else
            {
                stages.Add(CreateSkippedStage("gain", selection.Gain, calibrationLoads));
            }

            if (selection.Defect != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "defect"))
            {
                currentFloat ??= currentUInt16.Select(value => (float)value).ToArray();
                var defectMap = GetRequiredCalibration(calibrationRequests, "defect").DefectMap ??
                    throw new InvalidOperationException("Defect calibration map was not prepared.");
                stages.Add(CallStage(
                    "defect",
                    () => CallDefect(defectCorrect, currentFloat, defectMap, preview.PreviewWidth, preview.PreviewHeight),
                    GetLoadDetails(calibrationLoads, "defect")));
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

        private static GeneratedCalibration GenerateOffsetXCal(
            RawPreviewResult targetPreview,
            CalibrationFileDescriptor source,
            string xcalPath)
        {
            var offsetPreview = LoadMatchingCalibrationPreview(source, targetPreview);
            var values = offsetPreview.SampledPixels.ToArray();

            WriteXCalUInt16(xcalPath, targetPreview.PreviewWidth, targetPreview.PreviewHeight, values);
            return new GeneratedCalibration(
                xcalPath,
                $"Offset calibration generated from {source.Name}; raw range={offsetPreview.MinValue}..{offsetPreview.MaxValue}.",
                OffsetMap: values);
        }

        private static GeneratedCalibration GenerateGainXCal(
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

            WriteXCalFloat32(xcalPath, targetPreview.PreviewWidth, targetPreview.PreviewHeight, gainMap);
            var offsetNote = offsetSource is null ? "without dark subtraction" : $"dark-subtracted with {offsetSource.Name}";
            return new GeneratedCalibration(
                xcalPath,
                $"Gain calibration generated from {source.Name}; {offsetNote}; normalized mean={mean:0.###}.",
                GainMap: gainMap);
        }

        private static GeneratedCalibration GenerateDefectXCal(
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
            return new GeneratedCalibration(
                xcalPath,
                $"Defect calibration generated from {source.Name}; defect pixels={defectCount}/{mask.Length}; nonZeroRatio={nonZeroRatio:0.###}; inverted={invertMask}.",
                DefectMap: mask);
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

        private static NativePreviewCalibrationExpiryResult CheckCalibrationExpiry(
            CalibrationExpiryDelegate checkExpiry,
            string path)
        {
            var stopwatch = Stopwatch.StartNew();
            try
            {
                var result = CallCalibrationExpiry(
                    checkExpiry,
                    path,
                    out var expiryEpochMs,
                    out var boolRemainingDaysAbi,
                    out var isExpired,
                    out var remainingDays);
                stopwatch.Stop();

                if (boolRemainingDaysAbi)
                {
                    var remaining = remainingDays == int.MaxValue
                        ? (double?)null
                        : remainingDays;
                    var boolAbiChecked = result == XpeCommonApi.XpeErrorCode.OK ||
                        result == XpeCommonApi.XpeErrorCode.CALIBRATION_EXPIRED;
                    var boolAbiExpired = isExpired || result == XpeCommonApi.XpeErrorCode.CALIBRATION_EXPIRED;
                    var boolAbiDetails = remainingDays == int.MaxValue
                        ? "Calibration expiry checked through bool/int32 ABI; calibration does not expire."
                        : $"Calibration expiry checked through bool/int32 ABI; remainingDays={remainingDays}.";

                    return new NativePreviewCalibrationExpiryResult(
                        result.ToString(),
                        boolAbiChecked,
                        boolAbiExpired,
                        ExpiryEpochMs: 0,
                        ExpiryUtc: null,
                        remaining,
                        stopwatch.Elapsed.TotalMilliseconds,
                        boolAbiDetails);
                }

                var expiry = ConvertExpiryEpoch(expiryEpochMs);
                var checkedResult = result == XpeCommonApi.XpeErrorCode.OK ||
                    result == XpeCommonApi.XpeErrorCode.CALIBRATION_EXPIRED;
                var expired = result == XpeCommonApi.XpeErrorCode.CALIBRATION_EXPIRED ||
                    (expiry.ExpiresAtUtc is not null && expiry.ExpiresAtUtc.Value <= DateTimeOffset.UtcNow);
                var details = expiry.ExpiresAtUtc is null
                    ? "Calibration expiry checked through uint64 epoch ABI, but epoch could not be converted."
                    : $"Calibration expiry checked through uint64 epoch ABI; expires at {expiry.ExpiresAtUtc.Value:O}.";

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
            out ulong expiryEpochMs,
            out bool boolRemainingDaysAbi,
            out bool isExpired,
            out int remainingDays)
        {
            var pathPointer = Marshal.StringToHGlobalAnsi(path);
            var firstOut = Marshal.AllocHGlobal(sizeof(long));
            var secondOut = Marshal.AllocHGlobal(sizeof(int));
            try
            {
                Marshal.WriteInt64(firstOut, 0);
                Marshal.WriteInt32(secondOut, int.MinValue);

                var result = checkExpiry(pathPointer, firstOut, secondOut);
                remainingDays = Marshal.ReadInt32(secondOut);
                boolRemainingDaysAbi = remainingDays != int.MinValue;
                isExpired = Marshal.ReadByte(firstOut) != 0;
                expiryEpochMs = boolRemainingDaysAbi ? 0 : unchecked((ulong)Marshal.ReadInt64(firstOut));
                return result;
            }
            finally
            {
                Marshal.FreeHGlobal(secondOut);
                Marshal.FreeHGlobal(firstOut);
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

        private static void WriteXCalUInt16(
            string path,
            int width,
            int height,
            ReadOnlySpan<ushort> values)
        {
            var payload = new byte[checked(values.Length * sizeof(ushort))];
            MemoryMarshal.AsBytes(values).CopyTo(payload);
            WriteXCal(path, XpeCommonApi.XpePixelFormat.UInt16, width, height, payload);
        }

        private static void WriteXCalFloat32(
            string path,
            int width,
            int height,
            ReadOnlySpan<float> values)
        {
            var payload = new byte[checked(values.Length * sizeof(float))];
            MemoryMarshal.AsBytes(values).CopyTo(payload);
            WriteXCal(path, XpeCommonApi.XpePixelFormat.Float32, width, height, payload);
        }

        private static void WriteXCalMask(
            string path,
            int width,
            int height,
            byte[] mask)
        {
            WriteXCal(path, XpeCommonApi.XpePixelFormat.UInt8, width, height, mask);
        }

        private static void WriteXCal(
            string path,
            XpeCommonApi.XpePixelFormat pixelFormat,
            int width,
            int height,
            byte[] payload)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? AppContext.BaseDirectory);
            var expiryMs = DateTimeOffset.UtcNow.AddDays(365).ToUnixTimeMilliseconds();
            var crc32 = ComputeCrc32(payload);

            using var stream = File.Create(path);
            using var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: false);
            writer.Write(Encoding.ASCII.GetBytes("XPEC"));
            writer.Write(XCalVersion);
            writer.Write(checked((uint)width));
            writer.Write(checked((uint)height));
            writer.Write((uint)pixelFormat);
            writer.Write((ulong)expiryMs);
            writer.Write(crc32);
            for (var i = 0; i < 7; i++)
            {
                writer.Write(0u);
            }
            writer.Write(payload);
        }

        private static uint ComputeCrc32(byte[] payload)
        {
            uint crc = 0xFFFFFFFFu;
            foreach (var value in payload)
            {
                crc ^= value;
                for (var bit = 0; bit < 8; bit++)
                {
                    crc = (crc & 1u) != 0
                        ? 0xEDB88320u ^ (crc >> 1)
                        : crc >> 1;
                }
            }

            return crc ^ 0xFFFFFFFFu;
        }

        private static CalibrationRequest GetRequiredCalibration(
            IReadOnlyList<CalibrationRequest> requests,
            string stage)
        {
            return requests.FirstOrDefault(request => request.Stage == stage) ??
                throw new InvalidOperationException($"{stage} calibration map was not prepared.");
        }

        private static XpeCommonApi.XpeErrorCode CallOffset(
            OffsetCorrectionDelegate correction,
            ushort[] image,
            ushort[] offsetMap,
            int width,
            int height)
        {
            var imageHandle = GCHandle.Alloc(image, GCHandleType.Pinned);
            var mapHandle = GCHandle.Alloc(offsetMap, GCHandleType.Pinned);
            try
            {
                var imageBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, image.Length * sizeof(ushort));
                var mapBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, offsetMap.Length * sizeof(ushort));
                imageBuffer.Data = imageHandle.AddrOfPinnedObject();
                mapBuffer.Data = mapHandle.AddrOfPinnedObject();
                return correction(ref imageBuffer, ref mapBuffer);
            }
            finally
            {
                mapHandle.Free();
                imageHandle.Free();
            }
        }

        private static XpeCommonApi.XpeErrorCode CallGain(
            GainCorrectionDelegate correction,
            ushort[] image,
            float[] gainMap,
            int width,
            int height,
            out float[] output)
        {
            output = new float[image.Length];
            var imageHandle = GCHandle.Alloc(image, GCHandleType.Pinned);
            var mapHandle = GCHandle.Alloc(gainMap, GCHandleType.Pinned);
            try
            {
                var imageBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, image.Length * sizeof(ushort));
                var mapBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, gainMap.Length * sizeof(float));
                imageBuffer.Data = imageHandle.AddrOfPinnedObject();
                mapBuffer.Data = mapHandle.AddrOfPinnedObject();

                var originalData = imageBuffer.Data;
                var result = correction(ref imageBuffer, ref mapBuffer);
                if (result == XpeCommonApi.XpeErrorCode.OK)
                {
                    Marshal.Copy(imageBuffer.Data, output, 0, output.Length);
                    if (imageBuffer.Data != originalData)
                    {
                        XpeCommonApi.xpe_free_image(ref imageBuffer);
                    }
                }

                return result;
            }
            finally
            {
                mapHandle.Free();
                imageHandle.Free();
            }
        }

        private static XpeCommonApi.XpeErrorCode CallDefect(
            DefectCorrectionDelegate correction,
            float[] image,
            byte[] defectMap,
            int width,
            int height)
        {
            var imageHandle = GCHandle.Alloc(image, GCHandleType.Pinned);
            var mapHandle = GCHandle.Alloc(defectMap, GCHandleType.Pinned);
            try
            {
                var imageBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, image.Length * sizeof(float));
                var mapBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt8, defectMap.Length);
                imageBuffer.Data = imageHandle.AddrOfPinnedObject();
                mapBuffer.Data = mapHandle.AddrOfPinnedObject();
                return correction(ref imageBuffer, ref mapBuffer, IntPtr.Zero);
            }
            finally
            {
                mapHandle.Free();
                imageHandle.Free();
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
                BitsAllocated = format switch
                {
                    XpeCommonApi.XpePixelFormat.UInt8 => 8u,
                    XpeCommonApi.XpePixelFormat.UInt16 => 16u,
                    _ => 32u,
                },
                BitsStored = format switch
                {
                    XpeCommonApi.XpePixelFormat.UInt8 => 8u,
                    XpeCommonApi.XpePixelFormat.UInt16 => 16u,
                    _ => 32u,
                },
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
