using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using ImageProcTest.PInvokeWrappers;

namespace ImageProcTest
{
    internal sealed record DicomExportValidationResult(
        string Status,
        bool Passed,
        string WriteErrorCode,
        string ValidateErrorCode,
        string ReportJson,
        string? DicomPath,
        string? DicomSha256,
        long DicomSizeBytes,
        string Details);

    internal sealed record NativePresentationExportResult(
        string DisplayDllPath,
        string DicomDllPath,
        string CommonDllPath,
        string InputSource,
        IReadOnlyList<NativePreviewStageResult> Stages,
        NativePreviewMetrics Metrics,
        IReadOnlyList<ushort> OutputPixels,
        double TotalLatencyMs,
        ushort OutputMin,
        ushort OutputMax,
        string ArtifactDirectory,
        DicomExportValidationResult? DicomValidation)
    {
        public bool DicomPassed => DicomValidation?.Passed == true;

        public string Summary
        {
            get
            {
                var displayStages = string.Join("->", Stages.Where(stage => stage.Stage != "dicom-write").Select(stage => stage.Stage));
                var dicom = DicomValidation is null
                    ? "dicom=not selected"
                    : $"dicom={DicomValidation.Status}; file={Path.GetFileName(DicomValidation.DicomPath ?? "none")}";
                return $"display={displayStages}; {dicom}; output={OutputMin}..{OutputMax}; changed={Metrics.ChangedPixels}/{Metrics.PixelCount}; nanInf={Metrics.NaNInfCount}";
            }
        }
    }

    internal static class NativePresentationExportService
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode AllocImageDelegate(
            uint width,
            uint height,
            XpeCommonApi.XpePixelFormat format,
            out XpeCommonApi.XpeImageBuffer buffer);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode FreeImageDelegate(ref XpeCommonApi.XpeImageBuffer buffer);

        public static NativePresentationExportResult Run(
            RawPreviewResult preview,
            IReadOnlyList<float>? inputPixels,
            string inputSource,
            IReadOnlyList<string>? displayStageOrder,
            IReadOnlyList<string>? dicomStageOrder,
            string? artifactDirectory = null)
        {
            var displayStages = NormalizeDisplayStageOrder(displayStageOrder, NeedsDicomWrite(dicomStageOrder));
            var dicomStages = NormalizeDicomStageOrder(dicomStageOrder);
            if (displayStages.Count == 0 && dicomStages.Count == 0)
            {
                throw new InvalidOperationException("Select at least one display or DICOM stage before running presentation/export.");
            }

            var pixels = inputPixels is { Count: > 0 }
                ? inputPixels.ToArray()
                : preview.SampledPixels.Select(value => (float)value).ToArray();
            if (pixels.Length != preview.SampledPixels.Length)
            {
                throw new InvalidOperationException("Presentation/export input buffer dimensions do not match the loaded raw preview.");
            }

            var displayDllPath = ResolveDllPath(XpeDisplayWrapper.DllName, "image-processing", "xpe-post");
            var dicomDllPath = dicomStages.Count > 0
                ? ResolveDllPath(XpeDicomWrapper.DllName, "dicom", "image-processing", "xpe-post")
                : "not-selected";
            var commonDllPath = ResolveCommonDllPath(displayDllPath);
            var outputDirectory = artifactDirectory ?? CreateArtifactDirectory("presentation-export");
            Directory.CreateDirectory(outputDirectory);

            NativeDependencyLoader.TryLoadFor(commonDllPath);
            NativeDependencyLoader.TryLoadFor(displayDllPath);
            if (!NativeLibrary.TryLoad(commonDllPath, out var commonHandle))
            {
                throw new InvalidOperationException($"Failed to load {commonDllPath}.");
            }

            if (!NativeLibrary.TryLoad(displayDllPath, out var displayHandle))
            {
                NativeLibrary.Free(commonHandle);
                throw new InvalidOperationException($"Failed to load {displayDllPath}.");
            }

            var buffer = default(XpeCommonApi.XpeImageBuffer);
            var stopwatch = Stopwatch.StartNew();
            try
            {
                var allocImage = GetRequiredDelegate<AllocImageDelegate>(commonHandle, "xpe_alloc_image", XpeCommonApi.ResolvedDllPath);
                var freeImage = GetRequiredDelegate<FreeImageDelegate>(commonHandle, "xpe_free_image", XpeCommonApi.ResolvedDllPath);
                var allocCode = allocImage(
                    checked((uint)preview.PreviewWidth),
                    checked((uint)preview.PreviewHeight),
                    XpeCommonApi.XpePixelFormat.Float32,
                    out buffer);
                if (allocCode != XpeCommonApi.XpeErrorCode.OK)
                {
                    throw new InvalidOperationException($"xpe_alloc_image(float32) returned {allocCode}.");
                }

                Marshal.Copy(pixels, 0, buffer.Data, pixels.Length);

                var stages = new List<NativePreviewStageResult>();
                var requestedDisplayStages = new HashSet<string>(
                    (displayStageOrder ?? []).Where(IsDisplayStage).Select(NormalizeStageKey),
                    StringComparer.OrdinalIgnoreCase);

                RunDisplayStages(
                    displayHandle,
                    displayStages,
                    requestedDisplayStages,
                    preview,
                    pixels,
                    ref buffer,
                    stages);

                DicomExportValidationResult? dicomValidation = null;
                if (dicomStages.Contains("dicom-write", StringComparer.OrdinalIgnoreCase))
                {
                    if (buffer.Format != XpeCommonApi.XpePixelFormat.UInt16)
                    {
                        throw new InvalidOperationException("DICOM write requires a uint16 presentation buffer.");
                    }

                    var dicom = RunDicomWriteAndValidate(
                        dicomDllPath,
                        outputDirectory,
                        ref buffer,
                        out var dicomStage);
                    dicomValidation = dicom;
                    stages.Add(dicomStage);
                    if (!dicom.Passed)
                    {
                        throw new InvalidOperationException($"xpe_dicom_write/validate failed: {dicom.Details}");
                    }
                }

                var outputPixels = buffer.Format == XpeCommonApi.XpePixelFormat.UInt16
                    ? CopyUInt16(buffer.Data, checked((int)(buffer.DataSize / 2)))
                    : ScaleFloatToUInt16(CopyFloat(buffer.Data, pixels.Length));
                var metrics = ComputeMetrics(pixels, outputPixels);
                var (outputMin, outputMax) = ComputeMinMax(outputPixels);
                stopwatch.Stop();

                return new NativePresentationExportResult(
                    displayDllPath,
                    dicomDllPath,
                    commonDllPath,
                    inputSource,
                    stages,
                    metrics,
                    outputPixels,
                    stopwatch.Elapsed.TotalMilliseconds,
                    outputMin,
                    outputMax,
                    outputDirectory,
                    dicomValidation);
            }
            finally
            {
                stopwatch.Stop();
                if (buffer.Data != IntPtr.Zero)
                {
                    try
                    {
                        var freeImage = GetRequiredDelegate<FreeImageDelegate>(commonHandle, "xpe_free_image", commonDllPath);
                        freeImage(ref buffer);
                    }
                    catch
                    {
                        // Cleanup is best-effort; the primary native error is reported by the caller.
                    }
                }

                NativeLibrary.Free(displayHandle);
                NativeLibrary.Free(commonHandle);
            }
        }

        private static void RunDisplayStages(
            IntPtr displayHandle,
            IReadOnlyList<string> displayStages,
            HashSet<string> requestedDisplayStages,
            RawPreviewResult preview,
            ReadOnlySpan<float> inputPixels,
            ref XpeCommonApi.XpeImageBuffer buffer,
            List<NativePreviewStageResult> stages)
        {
            var applyModality = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.ApplyModalityLutDelegate>(
                displayHandle,
                "xpe_apply_modality_lut");
            var applyVoi = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.ApplyVoiLutDelegate>(
                displayHandle,
                "xpe_apply_voi_lut");
            var applyPresentation = XpeDisplayWrapper.GetRequiredDelegate<XpeDisplayWrapper.ApplyPresentationLutDelegate>(
                displayHandle,
                "xpe_apply_presentation_lut");

            foreach (var stage in displayStages)
            {
                var autoDetails = requestedDisplayStages.Contains(stage) ? "" : " auto prerequisite for DICOM write.";
                switch (stage)
                {
                    case "modality-lut":
                    {
                        var parameters = XpeModalityLutParams.Identity;
                        var stageStopwatch = Stopwatch.StartNew();
                        var code = applyModality(ref buffer, ref parameters);
                        stageStopwatch.Stop();
                        var result = CreateStageResult(
                            "modality-lut",
                            code,
                            stageStopwatch.Elapsed.TotalMilliseconds,
                            $"mode={parameters.Mode}; slope={parameters.RescaleSlope:0.###}; intercept={parameters.RescaleIntercept:0.###}.{autoDetails}");
                        AddRequiredStage(stages, result);
                        break;
                    }

                    case "voi-lut":
                    {
                        var parameters = CreateAutoVoi(inputPixels);
                        var stageStopwatch = Stopwatch.StartNew();
                        var code = applyVoi(ref buffer, ref parameters);
                        stageStopwatch.Stop();
                        var result = CreateStageResult(
                            "voi-lut",
                            code,
                            stageStopwatch.Elapsed.TotalMilliseconds,
                            $"mode={parameters.Mode}; center={parameters.Center:0.###}; width={parameters.Width:0.###}; output={parameters.MinOut:0.###}..{parameters.MaxOut:0.###}.{autoDetails}");
                        AddRequiredStage(stages, result);
                        break;
                    }

                    case "presentation-lut":
                    {
                        var parameters = XpePresentationLutParams.LinearUInt16();
                        var stageStopwatch = Stopwatch.StartNew();
                        var code = applyPresentation(ref buffer, ref parameters);
                        stageStopwatch.Stop();
                        var result = CreateStageResult(
                            "presentation-lut",
                            code,
                            stageStopwatch.Elapsed.TotalMilliseconds,
                            $"linearUInt16=true; gsdf={parameters.GsdfEnabled}; preview={preview.PreviewWidth}x{preview.PreviewHeight}.{autoDetails}");
                        AddRequiredStage(stages, result);
                        break;
                    }

                    default:
                        throw new InvalidOperationException($"Native display preview does not support stage '{stage}'.");
                }
            }
        }

        private static DicomExportValidationResult RunDicomWriteAndValidate(
            string dicomDllPath,
            string artifactDirectory,
            ref XpeCommonApi.XpeImageBuffer buffer,
            out NativePreviewStageResult stage)
        {
            NativeDependencyLoader.TryLoadFor(dicomDllPath);
            if (!NativeLibrary.TryLoad(dicomDllPath, out var dicomHandle))
            {
                throw new InvalidOperationException($"Failed to load {dicomDllPath}.");
            }

            var stopwatch = Stopwatch.StartNew();
            try
            {
                var write = XpeDicomWrapper.GetRequiredDelegate<XpeDicomWrapper.WriteDelegate>(
                    dicomHandle,
                    "xpe_dicom_write");
                var validate = XpeDicomWrapper.GetRequiredDelegate<XpeDicomWrapper.ValidateDelegate>(
                    dicomHandle,
                    "xpe_dicom_validate");

                var dicomPath = Path.Combine(artifactDirectory, $"phase1b-{DateTimeOffset.UtcNow:yyyyMMdd-HHmmss-fff}.dcm");
                var metadata = CreateMetadata();
                var writeCode = write(dicomPath, ref buffer, ref metadata);
                var reportJson = "";
                var validateCode = XpeCommonApi.XpeErrorCode.PROCESSING_FAILED;
                var valid = false;

                if (writeCode == XpeCommonApi.XpeErrorCode.OK)
                {
                    var report = new StringBuilder(65536);
                    validateCode = validate(dicomPath, report, checked((uint)report.Capacity));
                    reportJson = report.ToString();
                    valid = validateCode == XpeCommonApi.XpeErrorCode.OK && IsValidDicomReport(reportJson);
                }

                stopwatch.Stop();
                var exists = File.Exists(dicomPath);
                var size = exists ? new FileInfo(dicomPath).Length : 0;
                var sha = exists ? ComputeFileSha256(dicomPath) : null;
                var passed = writeCode == XpeCommonApi.XpeErrorCode.OK &&
                    validateCode == XpeCommonApi.XpeErrorCode.OK &&
                    valid &&
                    size > 0;
                var details =
                    $"write={writeCode}; validate={validateCode}; valid={valid}; size={size}; sha256={sha ?? "none"}";

                var result = new DicomExportValidationResult(
                    passed ? "Pass" : "Fail",
                    passed,
                    writeCode.ToString(),
                    validateCode.ToString(),
                    reportJson,
                    exists ? dicomPath : null,
                    sha,
                    size,
                    details);
                stage = new NativePreviewStageResult(
                    "dicom-write",
                    passed ? "OK" : $"{writeCode}/{validateCode}",
                    stopwatch.Elapsed.TotalMilliseconds,
                    Executed: true,
                    details);
                return result;
            }
            finally
            {
                NativeLibrary.Free(dicomHandle);
            }
        }

        private static void AddRequiredStage(
            List<NativePreviewStageResult> stages,
            NativePreviewStageResult result)
        {
            stages.Add(result);
            if (!string.Equals(result.ErrorCode, XpeCommonApi.XpeErrorCode.OK.ToString(), StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException($"{result.Stage} returned {result.ErrorCode}: {result.Details}");
            }
        }

        private static NativePreviewStageResult CreateStageResult(
            string stage,
            XpeCommonApi.XpeErrorCode code,
            double latencyMs,
            string details)
        {
            return new NativePreviewStageResult(stage, code.ToString(), latencyMs, Executed: true, details);
        }

        private static XpeVoiLutParams CreateAutoVoi(ReadOnlySpan<float> inputPixels)
        {
            var min = float.PositiveInfinity;
            var max = float.NegativeInfinity;
            for (var index = 0; index < inputPixels.Length; index++)
            {
                var value = inputPixels[index];
                if (!float.IsFinite(value))
                {
                    continue;
                }

                min = Math.Min(min, value);
                max = Math.Max(max, value);
            }

            if (!float.IsFinite(min) || !float.IsFinite(max))
            {
                min = 0.0f;
                max = 1.0f;
            }

            var width = Math.Max(1.0f, max - min);
            return new XpeVoiLutParams
            {
                Mode = XpeVoiLutMode.Linear,
                Center = min + (width * 0.5f),
                Width = width,
                MinOut = 0.0f,
                MaxOut = 1.0f
            };
        }

        private static IReadOnlyList<string> NormalizeDisplayStageOrder(
            IReadOnlyList<string>? displayStageOrder,
            bool needsDicomWrite)
        {
            var result = (displayStageOrder ?? [])
                .Where(IsDisplayStage)
                .Select(NormalizeStageKey)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToList();

            if (needsDicomWrite && !result.Contains("presentation-lut", StringComparer.OrdinalIgnoreCase))
            {
                EnsureStage(result, "modality-lut");
                EnsureStage(result, "voi-lut");
                EnsureStage(result, "presentation-lut");
            }

            return result;
        }

        private static IReadOnlyList<string> NormalizeDicomStageOrder(IReadOnlyList<string>? dicomStageOrder)
        {
            return (dicomStageOrder ?? [])
                .Where(IsDicomStage)
                .Select(NormalizeStageKey)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToList();
        }

        private static void EnsureStage(List<string> stages, string stage)
        {
            if (!stages.Contains(stage, StringComparer.OrdinalIgnoreCase))
            {
                stages.Add(stage);
            }
        }

        private static bool NeedsDicomWrite(IReadOnlyList<string>? dicomStageOrder) =>
            (dicomStageOrder ?? []).Any(IsDicomStage);

        private static bool IsDisplayStage(string stage) =>
            string.Equals(stage, "modality-lut", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stage, "voi-lut", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stage, "presentation-lut", StringComparison.OrdinalIgnoreCase);

        private static bool IsDicomStage(string stage) =>
            string.Equals(stage, "dicom-write", StringComparison.OrdinalIgnoreCase);

        private static string NormalizeStageKey(string stage) => stage.Trim().ToLowerInvariant();

        private static string ResolveDllPath(string dllName, params string[] siblingRoots)
        {
            return NativeModuleLibraryLocator.TryFindDll(dllName, siblingRoots) ??
                throw new FileNotFoundException($"{dllName} was not found in known GUI/build output locations.");
        }

        private static string ResolveCommonDllPath(string displayDllPath)
        {
            var sibling = Path.Combine(Path.GetDirectoryName(displayDllPath) ?? AppContext.BaseDirectory, "xpe_common.dll");
            if (File.Exists(sibling))
            {
                return sibling;
            }

            return NativeModuleLibraryLocator.TryFindDll("xpe_common.dll", "image-processing", "xpe-pre") ??
                throw new FileNotFoundException("xpe_common.dll was not found next to xpe_display.dll or in known build outputs.");
        }

        private static TDelegate GetRequiredDelegate<TDelegate>(
            IntPtr handle,
            string exportName,
            string dllName)
            where TDelegate : Delegate
        {
            if (!NativeLibrary.TryGetExport(handle, exportName, out var symbol))
            {
                throw new EntryPointNotFoundException($"{exportName} was not found in {dllName}.");
            }

            return Marshal.GetDelegateForFunctionPointer<TDelegate>(symbol);
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
                AcquisitionTime = checked((ulong)DateTimeOffset.UtcNow.ToUnixTimeSeconds()),
                Flags = 0
            };
        }

        private static ushort[] CopyUInt16(IntPtr source, int count)
        {
            var bytes = new byte[checked(count * sizeof(ushort))];
            Marshal.Copy(source, bytes, 0, bytes.Length);
            var output = new ushort[count];
            Buffer.BlockCopy(bytes, 0, output, 0, bytes.Length);
            return output;
        }

        private static float[] CopyFloat(IntPtr source, int count)
        {
            var output = new float[count];
            Marshal.Copy(source, output, 0, count);
            return output;
        }

        private static ushort[] ScaleFloatToUInt16(ReadOnlySpan<float> values)
        {
            var min = float.PositiveInfinity;
            var max = float.NegativeInfinity;
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

            if (!float.IsFinite(min) || !float.IsFinite(max))
            {
                min = 0.0f;
                max = 1.0f;
            }

            var range = Math.Max(1.0f, max - min);
            var output = new ushort[values.Length];
            for (var i = 0; i < values.Length; i++)
            {
                var value = float.IsFinite(values[i]) ? values[i] : min;
                output[i] = (ushort)Math.Clamp((int)Math.Round(((value - min) / range) * ushort.MaxValue), 0, ushort.MaxValue);
            }

            return output;
        }

        private static NativePreviewMetrics ComputeMetrics(
            ReadOnlySpan<float> input,
            ReadOnlySpan<ushort> output)
        {
            var min = float.PositiveInfinity;
            var max = float.NegativeInfinity;
            for (var i = 0; i < input.Length; i++)
            {
                var value = input[i];
                if (!float.IsFinite(value))
                {
                    continue;
                }

                min = Math.Min(min, value);
                max = Math.Max(max, value);
            }

            if (!float.IsFinite(min) || !float.IsFinite(max))
            {
                min = 0.0f;
                max = 1.0f;
            }

            var range = Math.Max(1.0f, max - min);
            var changed = 0;
            var nanInf = 0;
            var absSum = 0.0;
            var sqSum = 0.0;
            var maxAbs = 0.0;
            for (var i = 0; i < output.Length; i++)
            {
                var inputValue = input[i];
                if (!float.IsFinite(inputValue))
                {
                    nanInf++;
                    inputValue = min;
                }

                var normalized = Math.Clamp((inputValue - min) / range, 0.0f, 1.0f);
                var expected = normalized * ushort.MaxValue;
                var delta = output[i] - expected;
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

        private static (ushort Min, ushort Max) ComputeMinMax(ReadOnlySpan<ushort> values)
        {
            var min = ushort.MaxValue;
            var max = ushort.MinValue;
            for (var i = 0; i < values.Length; i++)
            {
                if (values[i] < min)
                {
                    min = values[i];
                }

                if (values[i] > max)
                {
                    max = values[i];
                }
            }

            return values.Length == 0 ? ((ushort)0, (ushort)0) : (min, max);
        }

        private static bool IsValidDicomReport(string reportJson)
        {
            if (string.IsNullOrWhiteSpace(reportJson))
            {
                return false;
            }

            try
            {
                using var document = JsonDocument.Parse(reportJson);
                return document.RootElement.TryGetProperty("valid", out var valid) &&
                    valid.ValueKind == JsonValueKind.True;
            }
            catch (JsonException)
            {
                return false;
            }
        }

        private static string CreateArtifactDirectory(string prefix)
        {
            return Path.Combine(
                AppContext.BaseDirectory,
                "phase1b-artifacts",
                $"{prefix}-{DateTimeOffset.UtcNow:yyyyMMdd-HHmmss-fff}");
        }

        private static string ComputeFileSha256(string path)
        {
            using var stream = File.OpenRead(path);
            return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
        }
    }
}
