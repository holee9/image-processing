using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Media.Imaging;
using ImageProcTest.PInvokeWrappers;

namespace ImageProcTest
{
    internal sealed record EnhanceBasicStageSelection(
        bool ExposureIndex,
        bool Log,
        bool Noise,
        bool Contrast,
        bool Edge)
    {
        public bool HasAnyStage => ExposureIndex || Log || Noise || Contrast || Edge;
    }

    internal sealed record EnhanceBasicStageParameters(
        float LogNormFactor,
        XpeNoiseReduceParams Noise,
        XpeClaheParams Contrast,
        XpeUsmParams Edge)
    {
        public static EnhanceBasicStageParameters Default => new(
            1000.0f,
            XpeNoiseReduceParams.DefaultBilateral,
            XpeClaheParams.Default,
            XpeUsmParams.Default);
    }

    internal sealed record NativeEnhanceBasicPreviewResult(
        string DllPath,
        string InputSource,
        IReadOnlyList<NativePreviewStageResult> Stages,
        NativePreviewMetrics Metrics,
        IReadOnlyList<float> OutputPixels,
        double TotalLatencyMs,
        double OutputMin,
        double OutputMax,
        float? ExposureIndex,
        float? DeviationIndex,
        float? SigmaBefore,
        float? SigmaAfter,
        WriteableBitmap Bitmap);

    internal static class NativeEnhanceBasicPreviewService
    {
        public static NativeEnhanceBasicPreviewResult Run(
            RawPreviewResult preview,
            IReadOnlyList<float>? inputPixels,
            string inputSource,
            EnhanceBasicStageSelection selection,
            EnhanceBasicStageParameters parameters,
            string? preferredDllPath,
            IReadOnlyList<string>? stageOrder = null)
        {
            if (!selection.HasAnyStage)
            {
                throw new InvalidOperationException("Select at least one enhance_basic stage before running post-processing preview.");
            }

            var dllPath = ResolveDllPath(preferredDllPath);
            if (dllPath is null)
            {
                throw new FileNotFoundException("xpe_enhance_basic.dll was not found in known locations.");
            }

            var output = inputPixels is { Count: > 0 }
                ? inputPixels.ToArray()
                : preview.SampledPixels.Select(value => (float)value).ToArray();
            if (output.Length != preview.SampledPixels.Length)
            {
                throw new InvalidOperationException("Post-processing input buffer dimensions do not match the loaded raw preview.");
            }

            NativeDependencyLoader.TryLoadFor(dllPath);
            if (!NativeLibrary.TryLoad(dllPath, out var handle))
            {
                throw new InvalidOperationException($"Failed to load {dllPath}.");
            }

            try
            {
                var log = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.LogTransformDelegate>(
                    handle,
                    "xpe_log_transform");
                var noise = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.NoiseReduceDelegate>(
                    handle,
                    "xpe_noise_reduce");
                var sigma = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.NoiseEstimateSigmaDelegate>(
                    handle,
                    "xpe_noise_estimate_sigma");
                var contrast = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.ContrastEnhanceDelegate>(
                    handle,
                    "xpe_contrast_enhance");
                var edge = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.EdgeEnhanceDelegate>(
                    handle,
                    "xpe_edge_enhance");
                var ei = XpeEnhanceBasicWrapper.GetRequiredDelegate<XpeEnhanceBasicWrapper.CalcExposureIndexDelegate>(
                    handle,
                    "xpe_calc_exposure_index");

                return RunChain(
                    preview,
                    output,
                    inputSource,
                    selection,
                    parameters,
                    dllPath,
                    stageOrder,
                    log,
                    noise,
                    sigma,
                    contrast,
                    edge,
                    ei);
            }
            finally
            {
                NativeLibrary.Free(handle);
            }
        }

        private static NativeEnhanceBasicPreviewResult RunChain(
            RawPreviewResult preview,
            float[] output,
            string inputSource,
            EnhanceBasicStageSelection selection,
            EnhanceBasicStageParameters parameters,
            string dllPath,
            IReadOnlyList<string>? stageOrder,
            XpeEnhanceBasicWrapper.LogTransformDelegate log,
            XpeEnhanceBasicWrapper.NoiseReduceDelegate noise,
            XpeEnhanceBasicWrapper.NoiseEstimateSigmaDelegate sigma,
            XpeEnhanceBasicWrapper.ContrastEnhanceDelegate contrast,
            XpeEnhanceBasicWrapper.EdgeEnhanceDelegate edge,
            XpeEnhanceBasicWrapper.CalcExposureIndexDelegate ei)
        {
            var stages = new List<NativePreviewStageResult>();
            var stopwatch = Stopwatch.StartNew();
            float? exposureIndex = null;
            float? deviationIndex = null;
            float? sigmaBefore = null;
            float? sigmaAfter = null;

            var imageHandle = GCHandle.Alloc(output, GCHandleType.Pinned);
            try
            {
                var buffer = CreateFloatBuffer(preview.PreviewWidth, preview.PreviewHeight, output.Length);
                buffer.Data = imageHandle.AddrOfPinnedObject();

                foreach (var stageKey in NormalizeStageOrder(stageOrder))
                {
                    switch (stageKey)
                    {
                        case "ei-whole":
                            if (selection.ExposureIndex)
                            {
                                var metadata = CreateMetadata();
                                stages.Add(CallStage(
                                    "ei-whole",
                                    () =>
                                    {
                                        var code = ei(ref buffer, ref metadata, out var outEi, out var outDi);
                                        exposureIndex = outEi;
                                        deviationIndex = outDi;
                                        return code;
                                    },
                                    () => $"EI={FormatNullable(exposureIndex)}, DI={FormatNullable(deviationIndex)}; input={inputSource}"));
                            }
                            else
                            {
                                stages.Add(CreateSkippedStage("ei-whole"));
                            }
                            break;

                        case "log":
                            if (selection.Log)
                            {
                                stages.Add(CallStage(
                                    "log",
                                    () => log(ref buffer, parameters.LogNormFactor),
                                    () => $"normFactor={parameters.LogNormFactor:0.###}; input={inputSource}"));
                            }
                            else
                            {
                                stages.Add(CreateSkippedStage("log"));
                            }
                            break;

                        case "basic-noise":
                            if (selection.Noise)
                            {
                                var beforeCode = sigma(ref buffer, out var before);
                                if (beforeCode == XpeCommonApi.XpeErrorCode.OK)
                                {
                                    sigmaBefore = before;
                                }

                                var noiseParams = parameters.Noise;
                                stages.Add(CallStage(
                                    "basic-noise",
                                    () => noise(ref buffer, ref noiseParams),
                                    () =>
                                    {
                                        var afterCode = sigma(ref buffer, out var after);
                                        if (afterCode == XpeCommonApi.XpeErrorCode.OK)
                                        {
                                            sigmaAfter = after;
                                        }

                                        return $"mode={noiseParams.Mode}; sigmaSpace={noiseParams.SigmaSpace:0.###}; sigmaRange={noiseParams.SigmaRange:0.###}; " +
                                            $"sigmaBefore={FormatNullable(sigmaBefore)}, sigmaAfter={FormatNullable(sigmaAfter)}";
                                    }));
                            }
                            else
                            {
                                stages.Add(CreateSkippedStage("basic-noise"));
                            }
                            break;

                        case "contrast":
                            if (selection.Contrast)
                            {
                                var contrastParams = parameters.Contrast;
                                stages.Add(CallStage(
                                    "contrast",
                                    () => contrast(ref buffer, ref contrastParams),
                                    () => $"clip={contrastParams.ClipLimit:0.###}; tiles={contrastParams.TileWidth}x{contrastParams.TileHeight}"));
                            }
                            else
                            {
                                stages.Add(CreateSkippedStage("contrast"));
                            }
                            break;

                        case "edge":
                            if (selection.Edge)
                            {
                                var edgeParams = parameters.Edge;
                                stages.Add(CallStage(
                                    "edge",
                                    () => edge(ref buffer, ref edgeParams),
                                    () => $"amount={edgeParams.Amount:0.###}; radius={edgeParams.Radius:0.###}; threshold={edgeParams.Threshold:0.###}"));
                            }
                            else
                            {
                                stages.Add(CreateSkippedStage("edge"));
                            }
                            break;

                        default:
                            throw new InvalidOperationException($"Native enhance_basic preview does not support stage '{stageKey}'.");
                    }
                }
            }
            finally
            {
                imageHandle.Free();
            }

            stopwatch.Stop();

            if (!stages.Any(stage => stage.Executed))
            {
                throw new InvalidOperationException("No native enhance_basic stage executed.");
            }

            var metrics = ComputeMetrics(preview.SampledPixels, output);
            var (outputMin, outputMax) = ComputeMinMax(output);
            var bitmap = RawPreviewService.CreateGray8Bitmap(
                output,
                preview.PreviewWidth,
                preview.PreviewHeight,
                (float)outputMin,
                (float)outputMax);

            return new NativeEnhanceBasicPreviewResult(
                dllPath,
                inputSource,
                stages,
                metrics,
                output,
                stopwatch.Elapsed.TotalMilliseconds,
                outputMin,
                outputMax,
                exposureIndex,
                deviationIndex,
                sigmaBefore,
                sigmaAfter,
                bitmap);
        }

        private static NativePreviewStageResult CallStage(
            string stage,
            Func<XpeCommonApi.XpeErrorCode> call,
            Func<string> details)
        {
            var stopwatch = Stopwatch.StartNew();
            XpeCommonApi.XpeErrorCode code;
            try
            {
                code = call();
            }
            catch (Exception ex)
            {
                stopwatch.Stop();
                return new NativePreviewStageResult(stage, "Exception", stopwatch.Elapsed.TotalMilliseconds, Executed: true, ex.Message);
            }

            stopwatch.Stop();
            return new NativePreviewStageResult(
                stage,
                code.ToString(),
                stopwatch.Elapsed.TotalMilliseconds,
                Executed: true,
                details());
        }

        private static NativePreviewStageResult CreateSkippedStage(string stage) =>
            new(stage, "Skipped", 0, Executed: false, "Stage switch is Off.");

        private static IReadOnlyList<string> NormalizeStageOrder(IReadOnlyList<string>? stageOrder)
        {
            var order = stageOrder is { Count: > 0 }
                ? stageOrder
                : ["ei-whole", "log", "basic-noise", "contrast", "edge"];

            return order
                .Where(IsEnhanceBasicStage)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .ToArray();
        }

        private static bool IsEnhanceBasicStage(string stageKey) =>
            string.Equals(stageKey, "ei-whole", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "log", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "basic-noise", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "contrast", StringComparison.OrdinalIgnoreCase) ||
            string.Equals(stageKey, "edge", StringComparison.OrdinalIgnoreCase);

        private static XpeCommonApi.XpeImageBuffer CreateFloatBuffer(int width, int height, int pixelCount)
        {
            return new XpeCommonApi.XpeImageBuffer
            {
                Width = checked((uint)width),
                Height = checked((uint)height),
                BitsAllocated = 32,
                BitsStored = 32,
                Format = XpeCommonApi.XpePixelFormat.Float32,
                DataSize = (nuint)checked(pixelCount * sizeof(float))
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

        private static string? ResolveDllPath(string? preferredDllPath)
        {
            if (!string.IsNullOrWhiteSpace(preferredDllPath) &&
                !string.Equals(preferredDllPath, XpeEnhanceBasicLibraryLocator.DllName, StringComparison.OrdinalIgnoreCase) &&
                File.Exists(preferredDllPath))
            {
                return preferredDllPath;
            }

            return XpeEnhanceBasicLibraryLocator.TryFindDll();
        }

        private static string FormatNullable(float? value) =>
            value.HasValue ? value.Value.ToString("0.###") : "n/a";
    }
}
