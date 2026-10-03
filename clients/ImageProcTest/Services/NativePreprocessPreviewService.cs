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
        // GUI-C-212: this was named InputPreserved, which the evaluation protocol defines as sha256(raw_before) == sha256(raw_after). It never measured that: it is
        // "no pixel moved by more than 0.5 and nothing is non-finite", i.e. the OUTPUT equals the input. A working correction (offset subtracts the dark level) is
        // supposed to move pixels, so the old name displayed "Input preserved: False" for a correct run.
        bool OutputIdenticalToInput,
        int NaNInfCount);

    internal sealed record NativePreprocessPreviewResult(
        string DllPath,
        string ArtifactDirectory,
        IReadOnlyList<NativePreviewCalibrationResult> CalibrationLoads,
        IReadOnlyList<NativePreviewStageResult> Stages,
        NativePreviewMetrics Metrics,
        DetectorDomainMetrics DetectorMetrics,
        IReadOnlyList<float> OutputPixels,
        double TotalLatencyMs,
        double OutputMin,
        double OutputMax,
        WriteableBitmap Bitmap);

    internal static class NativePreprocessPreviewService
    {
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
            ref XpeCommonApi.XpeImageBuffer input,
            ref XpeCommonApi.XpeImageBuffer output,
            ref XpeCommonApi.XpeImageMetadata metadata);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode GainCorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer input,
            ref XpeCommonApi.XpeImageBuffer output,
            ref XpeCommonApi.XpeImageMetadata metadata);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        // GUI-C-213 (#249): these three were declared as (image, map[, config]) -- the shape the functions had before calibration moved into the module's store (#117). The header's
        // shape is (input, output, metadata) and the maps are loaded by LoadCalibrationFiles. Called the old way, xpe_offset_correct returned OK, left the image untouched and wrote
        // the corrected image into the 'map' array (measured against the current DLL, see the GUI-C-213 report).
        private delegate XpeCommonApi.XpeErrorCode DefectCorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer input,
            ref XpeCommonApi.XpeImageBuffer output,
            ref XpeCommonApi.XpeImageMetadata metadata);

        // GUI-C-214 (#249): the module generates the offset and gain files itself (xpe_calib_generate_*) and loads all three (xpe_calib_load_*) into its calibration store, which is where the
        // corrections read their maps (#117). This service used to write files in a format of its own ("XPEC", CRC-32) that the module cannot read, and never loaded anything: every
        // correction answered CALIB_NOT_LOADED. A request therefore carries the PIXELS the module generates from; the defect map is the one file written here (the module has no generator for it).
        private sealed record CalibrationRequest(
            string Stage,
            CalibrationRole Role,
            CalibrationFileDescriptor Source,
            string XCalPath,
            string Details,
            ushort[]? DarkPixels,
            ushort[]? FlatPixels,
            ushort[]? FlatDarkReference,
            byte[]? DefectMap,
            int Width,
            int Height);

        private sealed record GeneratedCalibration(
            string XCalPath,
            string Details,
            ushort[]? DarkPixels = null,
            ushort[]? FlatPixels = null,
            ushort[]? FlatDarkReference = null,
            byte[]? DefectMap = null);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate XpeCommonApi.XpeErrorCode GenerateOffsetDelegate(
            [In] XpeCommonApi.XpeImageBuffer[] darkFrames,
            int numFrames,
            float integrationTimeMs,
            float temperatureC,
            [MarshalAs(UnmanagedType.LPStr)] string outputPath,
            [MarshalAs(UnmanagedType.LPStr)] string? configJsonOrNull);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate XpeCommonApi.XpeErrorCode GenerateGainDelegate(
            [In] XpeCommonApi.XpeImageBuffer[] flatFrames,
            int numFrames,
            IntPtr darkReferenceOrNull,
            [MarshalAs(UnmanagedType.LPStr)] string outputPath,
            [MarshalAs(UnmanagedType.LPStr)] string? metadataJsonOrNull);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        private delegate XpeCommonApi.XpeErrorCode LoadCalibrationDelegate([MarshalAs(UnmanagedType.LPStr)] string path);

        private sealed record ModuleCalibration(
            GenerateOffsetDelegate GenerateOffset,
            GenerateGainDelegate GenerateGain,
            LoadCalibrationDelegate LoadOffset,
            LoadCalibrationDelegate LoadGain,
            LoadCalibrationDelegate LoadDefect);

        private sealed record PreparedCalibration(
            IReadOnlyList<CalibrationRequest> Requests,
            IReadOnlyList<NativePreviewCalibrationResult> MissingLoads,
            string ArtifactDirectory);

        public static NativePreprocessPreviewResult Run(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
            FixtureCaseInfo? fixtureCase,
            string? preferredDllPath,
            IReadOnlyList<string>? stageOrder = null)
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
                var module = new ModuleCalibration(
                    GetRequiredDelegate<GenerateOffsetDelegate>(handle, "xpe_calib_generate_offset"),
                    GetRequiredDelegate<GenerateGainDelegate>(handle, "xpe_calib_generate_gain"),
                    GetRequiredDelegate<LoadCalibrationDelegate>(handle, "xpe_calib_load_offset"),
                    GetRequiredDelegate<LoadCalibrationDelegate>(handle, "xpe_calib_load_gain"),
                    GetRequiredDelegate<LoadCalibrationDelegate>(handle, "xpe_calib_load_defect_map"));

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
                        checkExpiry,
                        module);

                    return RunChain(
                        preview,
                        selection,
                        fixtureCase,
                        dllPath,
                        preparedCalibration.ArtifactDirectory,
                        loadResults,
                        preparedCalibration.Requests,
                        stageOrder,
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

        public static NativePreprocessPreviewResult CreateBypass(
            RawPreviewResult preview,
            string reason)
        {
            var output = preview.SampledPixels.Select(value => (float)value).ToArray();
            var stages = new[]
            {
                new NativePreviewStageResult("offset", "Bypassed", 0, Executed: false, reason),
                new NativePreviewStageResult("gain", "Bypassed", 0, Executed: false, reason),
                new NativePreviewStageResult("defect", "Bypassed", 0, Executed: false, reason)
            };
            var metrics = ComputeMetrics(preview.SampledPixels, output);
            var (outputMin, outputMax) = ComputeMinMax(output);
            var bitmap = RawPreviewService.CreateGray8Bitmap(
                output,
                preview.PreviewWidth,
                preview.PreviewHeight,
                preview.MinValue,
                preview.MaxValue);

            return new NativePreprocessPreviewResult(
                "bypass",
                "none",
                [],
                stages,
                metrics,
                MetricsComputationService.Empty("all correction stages bypassed"),
                output,
                0,
                outputMin,
                outputMax,
                bitmap);
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
                if (mode == PreprocessStageMode.On)
                {
                    throw new InvalidOperationException(
                        $"{stage} calibration is required because the stage is On, but no {role} calibration file exists in the selected calibration folder.");
                }

                missingLoads.Add(new NativePreviewCalibrationResult(
                    stage,
                    "Missing",
                    Loaded: false,
                    SourceRawPath: null,
                    XCalPath: null,
                    LatencyMs: 0,
                    Details: $"Skipped: no {role} calibration file exists in the selected calibration folder.",
                    Expiry: null));
                return;
            }

            var generated = generate(source);
            if (generated.DefectMap is not null && !File.Exists(generated.XCalPath))
            {
                throw new FileNotFoundException($"Generated {stage} XCal file was not found.", generated.XCalPath);
            }

            requests.Add(new CalibrationRequest(
                stage,
                role,
                source,
                generated.XCalPath,
                generated.Details,
                generated.DarkPixels,
                generated.FlatPixels,
                generated.FlatDarkReference,
                generated.DefectMap,
                preview.PreviewWidth,
                preview.PreviewHeight));
        }

        private static IReadOnlyList<NativePreviewCalibrationResult> LoadCalibrationFiles(
            PreparedCalibration prepared,
            CalibrationExpiryDelegate checkExpiry,
            ModuleCalibration module)
        {
            var results = new List<NativePreviewCalibrationResult>(prepared.MissingLoads);

            foreach (var request in prepared.Requests)
            {
                var stopwatch = Stopwatch.StartNew();
                var result = GenerateAndLoad(request, module);
                stopwatch.Stop();

                var expiry = File.Exists(request.XCalPath)
                    ? CheckCalibrationExpiry(checkExpiry, request.XCalPath)
                    : null;
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

        /// <summary>Generates the offset/gain file with the module (the defect file is already written) and loads it into the module's calibration store.</summary>
        private static XpeCommonApi.XpeErrorCode GenerateAndLoad(CalibrationRequest request, ModuleCalibration module)
        {
            const float IntegrationTimeMs = 100.0f;   // inside the header's 1..10000 ms; the synthetic and fixture frames carry no exposure record
            const float TemperatureC = 25.0f;         // inside the header's -20..60 C

            switch (request.Stage)
            {
                case "offset":
                {
                    var code = WithFrame(request.DarkPixels!, request.Width, request.Height, frame =>
                        module.GenerateOffset([frame], 1, IntegrationTimeMs, TemperatureC, request.XCalPath, null));
                    return code == XpeCommonApi.XpeErrorCode.OK ? module.LoadOffset(request.XCalPath) : code;
                }

                case "gain":
                {
                    var code = WithFrame(request.FlatPixels!, request.Width, request.Height, flat =>
                    {
                        if (request.FlatDarkReference is null)
                        {
                            return module.GenerateGain([flat], 1, IntPtr.Zero, request.XCalPath, null);
                        }

                        return WithFrame(request.FlatDarkReference, request.Width, request.Height, dark =>
                        {
                            var darkPointer = Marshal.AllocHGlobal(Marshal.SizeOf<XpeCommonApi.XpeImageBuffer>());
                            try
                            {
                                Marshal.StructureToPtr(dark, darkPointer, fDeleteOld: false);
                                return module.GenerateGain([flat], 1, darkPointer, request.XCalPath, null);
                            }
                            finally
                            {
                                Marshal.FreeHGlobal(darkPointer);
                            }
                        });
                    });
                    return code == XpeCommonApi.XpeErrorCode.OK ? module.LoadGain(request.XCalPath) : code;
                }

                case "defect":
                    return module.LoadDefect(request.XCalPath);

                default:
                    throw new InvalidOperationException($"Unknown calibration stage '{request.Stage}'.");
            }
        }

        private static XpeCommonApi.XpeErrorCode WithFrame(
            ushort[] pixels,
            int width,
            int height,
            Func<XpeCommonApi.XpeImageBuffer, XpeCommonApi.XpeErrorCode> use)
        {
            var handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
            try
            {
                var frame = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, pixels.Length * sizeof(ushort));
                frame.Data = handle.AddrOfPinnedObject();
                return use(frame);
            }
            finally
            {
                handle.Free();
            }
        }

        private static NativePreprocessPreviewResult RunChain(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
            FixtureCaseInfo? fixtureCase,
            string dllPath,
            string artifactDirectory,
            IReadOnlyList<NativePreviewCalibrationResult> calibrationLoads,
            IReadOnlyList<CalibrationRequest> calibrationRequests,
            IReadOnlyList<string>? stageOrder,
            OffsetCorrectionDelegate offsetCorrect,
            GainCorrectionDelegate gainCorrect,
            DefectCorrectionDelegate defectCorrect)
        {
            var stages = new List<NativePreviewStageResult>();
            var stopwatch = Stopwatch.StartNew();
            var metadata = CreateMetadata();
            var currentUInt16 = preview.SampledPixels.ToArray();
            float[]? currentFloat = null;
            float[]? defectBefore = null;
            float[]? defectAfter = null;

            foreach (var stageKey in NormalizeStageOrder(stageOrder))
            {
                switch (stageKey)
                {
                    case "offset":
                        if (currentFloat is not null)
                        {
                            throw new InvalidOperationException("Offset correction cannot run after a float32 stage in the current native adapter.");
                        }

                        if (selection.Offset != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "offset"))
                        {
                            _ = GetRequiredCalibration(calibrationRequests, "offset").DarkPixels ??
                                throw new InvalidOperationException("Offset calibration map was not prepared.");
                            stages.Add(CallStage(
                                "offset",
                                () => CallOffset(offsetCorrect, currentUInt16, preview.PreviewWidth, preview.PreviewHeight, out currentUInt16),
                                GetLoadDetails(calibrationLoads, "offset")));
                        }
                        else
                        {
                            stages.Add(CreateSkippedStage("offset", selection.Offset, calibrationLoads));
                        }
                        break;

                    case "gain":
                        if (selection.Gain != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "gain"))
                        {
                            _ = GetRequiredCalibration(calibrationRequests, "gain").FlatPixels ??
                                throw new InvalidOperationException("Gain calibration map was not prepared.");
                            stages.Add(CallStage(
                                "gain",
                                () => CallGain(gainCorrect, currentUInt16, preview.PreviewWidth, preview.PreviewHeight, out currentFloat),
                                GetLoadDetails(calibrationLoads, "gain")));
                        }
                        else
                        {
                            stages.Add(CreateSkippedStage("gain", selection.Gain, calibrationLoads));
                        }
                        break;

                    case "defect":
                        if (selection.Defect != PreprocessStageMode.Off && IsLoaded(calibrationLoads, "defect"))
                        {
                            currentFloat ??= currentUInt16.Select(value => (float)value).ToArray();
                            // GUI-C-182: the defect stage corrects in place, so the image it was GIVEN has to be copied
                            // before the call. The protocol's GoodPixelDeltaP99 is measured against it (Y_no_defect_stage).
                            defectBefore = (float[])currentFloat.Clone();
                            _ = GetRequiredCalibration(calibrationRequests, "defect").DefectMap ??
                                throw new InvalidOperationException("Defect calibration map was not prepared.");
                            stages.Add(CallStage(
                                "defect",
                                () => CallDefect(defectCorrect, currentFloat!, preview.PreviewWidth, preview.PreviewHeight, out currentFloat),
                                GetLoadDetails(calibrationLoads, "defect")));
                            defectAfter = (float[])currentFloat.Clone();
                        }
                        else
                        {
                            stages.Add(CreateSkippedStage("defect", selection.Defect, calibrationLoads));
                        }
                        break;

                    default:
                        throw new InvalidOperationException($"Native preprocess preview does not support stage '{stageKey}'.");
                }
            }

            stopwatch.Stop();

            if (!stages.Any(stage => stage.Executed))
            {
                throw new InvalidOperationException("No native preprocessing stage executed. Select a fixture case that contains matching calibration files.");
            }

            var finalValues = currentFloat ?? currentUInt16.Select(value => (float)value).ToArray();
            var metrics = ComputeMetrics(preview.SampledPixels, finalValues);
            var detectorMetrics = MetricsComputationService.Compute(
                preview,
                finalValues,
                fixtureCase,
                stages,
                defectBefore is null || defectAfter is null ? null : new DefectStageImages(defectBefore, defectAfter));
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
                detectorMetrics,
                finalValues.ToArray(),
                stopwatch.Elapsed.TotalMilliseconds,
                outputMin,
                outputMax,
                bitmap);
        }

        private static IReadOnlyList<string> NormalizeStageOrder(IReadOnlyList<string>? stageOrder)
        {
            var requested = stageOrder is null || stageOrder.Count == 0
                ? ["offset", "gain", "defect"]
                : stageOrder;
            return requested
                .Where(stage => string.Equals(stage, "offset", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(stage, "gain", StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(stage, "defect", StringComparison.OrdinalIgnoreCase))
                .Select(stage => stage.ToLowerInvariant())
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToArray();
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
                return new NativePreviewStageResult(stage, "Skipped", 0, Executed: false, "Stage switch is Off.");
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

            return new GeneratedCalibration(
                xcalPath,
                $"Offset calibration generated by the module from {source.Name}; raw range={offsetPreview.MinValue}..{offsetPreview.MaxValue}.",
                DarkPixels: values);
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
                offsetPixels = LoadMatchingCalibrationPreview(offsetSource, targetPreview).SampledPixels.ToArray();
            }

            var offsetNote = offsetSource is null ? "without dark subtraction" : $"dark-subtracted with {offsetSource.Name}";
            return new GeneratedCalibration(
                xcalPath,
                $"Gain calibration generated by the module from {source.Name}; {offsetNote}.",
                FlatPixels: gainPreview.SampledPixels.ToArray(),
                FlatDarkReference: offsetPixels);
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

            WriteDefectXCal(xcalPath, targetPreview.PreviewWidth, targetPreview.PreviewHeight, mask);
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

        /// <summary>
        /// A DEFECT XCal v1 file (modules/preprocess/include/xpe/preprocess/xcal_format.h): the 152-byte header (magic "XCAL", version 1, type DEFECT, UINT8_MASK, size, created, expiry, an EMPTY
        /// session id, no config, payload length, SHA-256 of config||payload) followed by the mask. The module has no generator for defect maps.
        /// </summary>
        private static void WriteDefectXCal(string path, int width, int height, byte[] mask)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path) ?? AppContext.BaseDirectory);
            using var stream = File.Create(path);
            using var writer = new BinaryWriter(stream);
            writer.Write(Encoding.ASCII.GetBytes("XCAL"));
            writer.Write(1u);                                  // version
            writer.Write(2u);                                  // type: DEFECT
            writer.Write(2u);                                  // pixel format: UINT8_MASK
            writer.Write(checked((uint)width));
            writer.Write(checked((uint)height));
            writer.Write(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
            writer.Write(DateTimeOffset.UtcNow.AddDays(365).ToUnixTimeMilliseconds());
            writer.Write(new byte[64]);                        // session id (empty)
            writer.Write(0UL);                                 // config length
            writer.Write((ulong)mask.Length);                  // payload length
            writer.Write(SHA256.HashData(mask));               // SHA-256 of (config || payload)
            writer.Write(mask);
        }

        private static CalibrationRequest GetRequiredCalibration(
            IReadOnlyList<CalibrationRequest> requests,
            string stage)
        {
            return requests.FirstOrDefault(request => request.Stage == stage) ??
                throw new InvalidOperationException($"{stage} calibration map was not prepared.");
        }

        // GUI-C-213 (#249): each correction takes (input, output, metadata) and writes its OWN output buffer; the calibration maps are not parameters, they are in the module's store
        // (LoadCalibrationFiles). A stage that fails leaves the image it was given untouched (the out parameter keeps the input).

        private static XpeCommonApi.XpeErrorCode CallOffset(
            OffsetCorrectionDelegate correction,
            ushort[] image,
            int width,
            int height,
            out ushort[] output)
        {
            output = image;
            var result = new ushort[image.Length];
            var imageHandle = GCHandle.Alloc(image, GCHandleType.Pinned);
            var resultHandle = GCHandle.Alloc(result, GCHandleType.Pinned);
            try
            {
                var inputBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, image.Length * sizeof(ushort));
                var outputBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, result.Length * sizeof(ushort));
                inputBuffer.Data = imageHandle.AddrOfPinnedObject();
                outputBuffer.Data = resultHandle.AddrOfPinnedObject();
                var metadata = CreateMetadata();
                var code = correction(ref inputBuffer, ref outputBuffer, ref metadata);
                if (code == XpeCommonApi.XpeErrorCode.OK)
                {
                    output = result;
                }

                return code;
            }
            finally
            {
                resultHandle.Free();
                imageHandle.Free();
            }
        }

        private static XpeCommonApi.XpeErrorCode CallGain(
            GainCorrectionDelegate correction,
            ushort[] image,
            int width,
            int height,
            out float[] output)
        {
            output = new float[image.Length];
            var imageHandle = GCHandle.Alloc(image, GCHandleType.Pinned);
            var outputHandle = GCHandle.Alloc(output, GCHandleType.Pinned);
            try
            {
                var inputBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, image.Length * sizeof(ushort));
                var outputBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, output.Length * sizeof(float));
                inputBuffer.Data = imageHandle.AddrOfPinnedObject();
                outputBuffer.Data = outputHandle.AddrOfPinnedObject();
                var metadata = CreateMetadata();
                return correction(ref inputBuffer, ref outputBuffer, ref metadata);
            }
            finally
            {
                outputHandle.Free();
                imageHandle.Free();
            }
        }

        private static XpeCommonApi.XpeErrorCode CallDefect(
            DefectCorrectionDelegate correction,
            float[] image,
            int width,
            int height,
            out float[] output)
        {
            output = image;
            var result = new float[image.Length];
            var imageHandle = GCHandle.Alloc(image, GCHandleType.Pinned);
            var resultHandle = GCHandle.Alloc(result, GCHandleType.Pinned);
            try
            {
                var inputBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, image.Length * sizeof(float));
                var outputBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, result.Length * sizeof(float));
                inputBuffer.Data = imageHandle.AddrOfPinnedObject();
                outputBuffer.Data = resultHandle.AddrOfPinnedObject();
                var metadata = CreateMetadata();
                var code = correction(ref inputBuffer, ref outputBuffer, ref metadata);
                if (code == XpeCommonApi.XpeErrorCode.OK)
                {
                    output = result;
                }

                return code;
            }
            finally
            {
                resultHandle.Free();
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
                PixelPitch_mm = DetectorDefaults.PixelPitchMm,   // GUI-C-100: 140 µm, one source
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
                OutputIdenticalToInput: changed == 0 && nanInf == 0,
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
