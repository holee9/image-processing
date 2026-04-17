using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace ImageProcTest
{
    internal static class XpePreprocessSyntheticOracle
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode InitDelegate(IntPtr config);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void ShutdownDelegate();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate XpeCommonApi.XpeErrorCode CorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer input,
            ref XpeCommonApi.XpeImageBuffer output,
            ref XpeCommonApi.XpeImageMetadata metadata);

        public static PreprocessSyntheticOracleResult Run(string dllPath)
        {
            if (!File.Exists(dllPath))
            {
                return PreprocessSyntheticOracleResult.NotRun($"DLL not found: {dllPath}");
            }

            if (!NativeLibrary.TryLoad(dllPath, out var handle))
            {
                return PreprocessSyntheticOracleResult.NotRun($"DLL load failed: {dllPath}");
            }

            try
            {
                if (!TryGetDelegate(handle, "xpe_preprocess_init", out InitDelegate? init) ||
                    !TryGetDelegate(handle, "xpe_preprocess_shutdown", out ShutdownDelegate? shutdown) ||
                    !TryGetDelegate(handle, "xpe_offset_correct", out CorrectionDelegate? offsetCorrect) ||
                    !TryGetDelegate(handle, "xpe_gain_correct", out CorrectionDelegate? gainCorrect) ||
                    !TryGetDelegate(handle, "xpe_defect_correct", out CorrectionDelegate? defectCorrect))
                {
                    return PreprocessSyntheticOracleResult.NotRun("Mandatory correction exports are not available.");
                }

                var initFn = init ?? throw new InvalidOperationException("xpe_preprocess_init delegate is null.");
                var shutdownFn = shutdown ?? throw new InvalidOperationException("xpe_preprocess_shutdown delegate is null.");
                var offsetFn = offsetCorrect ?? throw new InvalidOperationException("xpe_offset_correct delegate is null.");
                var gainFn = gainCorrect ?? throw new InvalidOperationException("xpe_gain_correct delegate is null.");
                var defectFn = defectCorrect ?? throw new InvalidOperationException("xpe_defect_correct delegate is null.");

                shutdownFn();
                var initResult = initFn(IntPtr.Zero);
                if (initResult != XpeCommonApi.XpeErrorCode.OK)
                {
                    return new PreprocessSyntheticOracleResult(
                        Status: "Init failed",
                        Details: $"xpe_preprocess_init(NULL) returned {initResult}.",
                        Executed: true,
                        Passed: false,
                        TotalLatencyMs: 0,
                        InputPreserved: false,
                        RawSha256Before: "",
                        RawSha256After: "",
                        OutputSha256: "",
                        NaNInfCount: 0,
                        DeterminismRmse: double.NaN,
                        OutputMin: double.NaN,
                        OutputMax: double.NaN,
                        Stages: []);
                }

                try
                {
                    var first = RunChain(offsetFn, gainFn, defectFn);
                    var second = RunChain(offsetFn, gainFn, defectFn);
                    var determinismRmse = CalculateRmse(first.Output, second.Output);
                    var outputSha = ComputeSha256(first.Output);
                    var passed = first.Passed && second.Passed && determinismRmse == 0;

                    return new PreprocessSyntheticOracleResult(
                        Status: passed ? "Synthetic oracle pass" : "Synthetic oracle fail",
                        Details: passed
                            ? "16x16 synthetic offset->gain->defect adapter chain passed with identity/no-calibration semantics."
                            : "Synthetic adapter chain executed but one or more gates failed.",
                        Executed: true,
                        Passed: passed,
                        TotalLatencyMs: first.TotalLatencyMs + second.TotalLatencyMs,
                        InputPreserved: first.InputPreserved && second.InputPreserved,
                        RawSha256Before: first.RawSha256Before,
                        RawSha256After: first.RawSha256After,
                        OutputSha256: outputSha,
                        NaNInfCount: first.NaNInfCount + second.NaNInfCount,
                        DeterminismRmse: determinismRmse,
                        OutputMin: first.OutputMin,
                        OutputMax: first.OutputMax,
                        Stages: first.Stages);
                }
                finally
                {
                    shutdownFn();
                }
            }
            catch (Exception ex)
            {
                return new PreprocessSyntheticOracleResult(
                    Status: "Synthetic oracle exception",
                    Details: ex.Message,
                    Executed: true,
                    Passed: false,
                    TotalLatencyMs: 0,
                    InputPreserved: false,
                    RawSha256Before: "",
                    RawSha256After: "",
                    OutputSha256: "",
                    NaNInfCount: 0,
                    DeterminismRmse: double.NaN,
                    OutputMin: double.NaN,
                    OutputMax: double.NaN,
                    Stages: []);
            }
            finally
            {
                NativeLibrary.Free(handle);
            }
        }

        private static bool TryGetDelegate<TDelegate>(IntPtr handle, string exportName, out TDelegate? value)
            where TDelegate : Delegate
        {
            if (!NativeLibrary.TryGetExport(handle, exportName, out var symbol))
            {
                value = null;
                return false;
            }

            value = Marshal.GetDelegateForFunctionPointer<TDelegate>(symbol);
            return true;
        }

        private static ChainRunResult RunChain(
            CorrectionDelegate offsetCorrect,
            CorrectionDelegate gainCorrect,
            CorrectionDelegate defectCorrect)
        {
            const uint width = 16;
            const uint height = 16;
            const int pixelCount = (int)(width * height);

            var raw = Enumerable.Range(0, pixelCount)
                .Select(index => (ushort)(1000 + (index % 97)))
                .ToArray();
            var offset = new ushort[pixelCount];
            var gain = new float[pixelCount];
            var defect = new float[pixelCount];

            var rawShaBefore = ComputeSha256(raw);
            var stages = new List<PreprocessSyntheticStageResult>();
            var total = Stopwatch.StartNew();

            var rawHandle = GCHandle.Alloc(raw, GCHandleType.Pinned);
            var offsetHandle = GCHandle.Alloc(offset, GCHandleType.Pinned);
            var gainHandle = GCHandle.Alloc(gain, GCHandleType.Pinned);
            var defectHandle = GCHandle.Alloc(defect, GCHandleType.Pinned);

            try
            {
                var metadata = CreateMetadata();
                var rawBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, rawHandle.AddrOfPinnedObject(), raw.Length * sizeof(ushort));
                var offsetBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.UInt16, offsetHandle.AddrOfPinnedObject(), offset.Length * sizeof(ushort));
                var gainBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, gainHandle.AddrOfPinnedObject(), gain.Length * sizeof(float));
                var defectBuffer = CreateBuffer(width, height, XpeCommonApi.XpePixelFormat.Float32, defectHandle.AddrOfPinnedObject(), defect.Length * sizeof(float));

                var offsetResult = TimedCall("offset", () => offsetCorrect(ref rawBuffer, ref offsetBuffer, ref metadata));
                stages.Add(offsetResult.ToStageResult(MaxAbsError(offset, raw), offsetResult.ErrorCode == XpeCommonApi.XpeErrorCode.OK && MaxAbsError(offset, raw) == 0));

                var gainResult = TimedCall("gain", () => gainCorrect(ref offsetBuffer, ref gainBuffer, ref metadata));
                stages.Add(gainResult.ToStageResult(MaxAbsError(gain, offset), gainResult.ErrorCode == XpeCommonApi.XpeErrorCode.OK && MaxAbsError(gain, offset) == 0));

                var defectResult = TimedCall("defect", () => defectCorrect(ref gainBuffer, ref defectBuffer, ref metadata));
                stages.Add(defectResult.ToStageResult(MaxAbsError(defect, gain), defectResult.ErrorCode == XpeCommonApi.XpeErrorCode.OK && MaxAbsError(defect, gain) == 0));
            }
            finally
            {
                rawHandle.Free();
                offsetHandle.Free();
                gainHandle.Free();
                defectHandle.Free();
                total.Stop();
            }

            var rawShaAfter = ComputeSha256(raw);
            var nanInfCount = defect.Count(value => float.IsNaN(value) || float.IsInfinity(value));
            var inputPreserved = string.Equals(rawShaBefore, rawShaAfter, StringComparison.OrdinalIgnoreCase);
            var passed = inputPreserved && nanInfCount == 0 && stages.All(stage => stage.Passed);

            return new ChainRunResult(
                Passed: passed,
                TotalLatencyMs: total.Elapsed.TotalMilliseconds,
                InputPreserved: inputPreserved,
                RawSha256Before: rawShaBefore,
                RawSha256After: rawShaAfter,
                NaNInfCount: nanInfCount,
                OutputMin: defect.Min(),
                OutputMax: defect.Max(),
                Output: defect,
                Stages: stages);
        }

        private static XpeCommonApi.XpeImageBuffer CreateBuffer(
            uint width,
            uint height,
            XpeCommonApi.XpePixelFormat format,
            IntPtr data,
            int dataSize)
        {
            return new XpeCommonApi.XpeImageBuffer
            {
                Width = width,
                Height = height,
                BitsAllocated = format == XpeCommonApi.XpePixelFormat.UInt16 ? 16u : 32u,
                BitsStored = format == XpeCommonApi.XpePixelFormat.UInt16 ? 16u : 32u,
                Format = format,
                Data = data,
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

        private static TimedCallResult TimedCall(string stage, Func<XpeCommonApi.XpeErrorCode> call)
        {
            var stopwatch = Stopwatch.StartNew();
            var errorCode = call();
            stopwatch.Stop();
            return new TimedCallResult(stage, errorCode, stopwatch.Elapsed.TotalMilliseconds);
        }

        private static double MaxAbsError(ushort[] actual, ushort[] expected)
        {
            var max = 0.0;
            for (var i = 0; i < actual.Length; i++)
            {
                max = Math.Max(max, Math.Abs(actual[i] - expected[i]));
            }

            return max;
        }

        private static double MaxAbsError(float[] actual, ushort[] expected)
        {
            var max = 0.0;
            for (var i = 0; i < actual.Length; i++)
            {
                max = Math.Max(max, Math.Abs(actual[i] - expected[i]));
            }

            return max;
        }

        private static double MaxAbsError(float[] actual, float[] expected)
        {
            var max = 0.0;
            for (var i = 0; i < actual.Length; i++)
            {
                max = Math.Max(max, Math.Abs(actual[i] - expected[i]));
            }

            return max;
        }

        private static double CalculateRmse(float[] first, float[] second)
        {
            var sum = 0.0;
            for (var i = 0; i < first.Length; i++)
            {
                var diff = first[i] - second[i];
                sum += diff * diff;
            }

            return Math.Sqrt(sum / first.Length);
        }

        private static string ComputeSha256<T>(T[] values)
            where T : struct
        {
            var bytes = MemoryMarshal.AsBytes(values.AsSpan());
            return Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
        }

        private sealed record TimedCallResult(
            string Stage,
            XpeCommonApi.XpeErrorCode ErrorCode,
            double LatencyMs)
        {
            public PreprocessSyntheticStageResult ToStageResult(double maxAbsError, bool passed)
            {
                return new PreprocessSyntheticStageResult(Stage, ErrorCode.ToString(), LatencyMs, maxAbsError, passed);
            }
        }

        private sealed record ChainRunResult(
            bool Passed,
            double TotalLatencyMs,
            bool InputPreserved,
            string RawSha256Before,
            string RawSha256After,
            int NaNInfCount,
            double OutputMin,
            double OutputMax,
            float[] Output,
            IReadOnlyList<PreprocessSyntheticStageResult> Stages);
    }
}
