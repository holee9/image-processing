using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Media.Imaging;

namespace ImageProcTest
{
    internal sealed record PreprocessStageSelection(bool Offset, bool Gain, bool Defect)
    {
        public bool HasAnyStage => Offset || Gain || Defect;
    }

    internal sealed record NativePreviewStageResult(
        string Stage,
        string ErrorCode,
        double LatencyMs,
        bool Executed);

    internal sealed record NativePreprocessPreviewResult(
        string DllPath,
        IReadOnlyList<NativePreviewStageResult> Stages,
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
        private delegate XpeCommonApi.XpeErrorCode CorrectionDelegate(
            ref XpeCommonApi.XpeImageBuffer input,
            ref XpeCommonApi.XpeImageBuffer output,
            ref XpeCommonApi.XpeImageMetadata metadata);

        public static NativePreprocessPreviewResult Run(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
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

            if (!NativeLibrary.TryLoad(dllPath, out var handle))
            {
                throw new InvalidOperationException($"Failed to load {dllPath}.");
            }

            try
            {
                var init = GetRequiredDelegate<InitDelegate>(handle, "xpe_preprocess_init");
                var shutdown = GetRequiredDelegate<ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
                var offsetCorrect = selection.Offset
                    ? GetRequiredDelegate<CorrectionDelegate>(handle, "xpe_offset_correct")
                    : null;
                var gainCorrect = selection.Gain
                    ? GetRequiredDelegate<CorrectionDelegate>(handle, "xpe_gain_correct")
                    : null;
                var defectCorrect = selection.Defect
                    ? GetRequiredDelegate<CorrectionDelegate>(handle, "xpe_defect_correct")
                    : null;

                shutdown();
                var initResult = init(IntPtr.Zero);
                if (initResult != XpeCommonApi.XpeErrorCode.OK)
                {
                    throw new InvalidOperationException($"xpe_preprocess_init(NULL) returned {initResult}.");
                }

                try
                {
                    return RunChain(preview, selection, dllPath, offsetCorrect, gainCorrect, defectCorrect);
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

        private static NativePreprocessPreviewResult RunChain(
            RawPreviewResult preview,
            PreprocessStageSelection selection,
            string dllPath,
            CorrectionDelegate? offsetCorrect,
            CorrectionDelegate? gainCorrect,
            CorrectionDelegate? defectCorrect)
        {
            var stages = new List<NativePreviewStageResult>();
            var stopwatch = Stopwatch.StartNew();
            var metadata = CreateMetadata();
            var currentUInt16 = preview.SampledPixels.ToArray();
            float[]? currentFloat = null;

            if (selection.Offset)
            {
                var output = new ushort[currentUInt16.Length];
                stages.Add(CallStage(
                    "offset",
                    () => CallUInt16ToUInt16(offsetCorrect!, currentUInt16, output, preview.PreviewWidth, preview.PreviewHeight, ref metadata)));
                currentUInt16 = output;
            }
            else
            {
                stages.Add(new NativePreviewStageResult("offset", "Skipped", 0, Executed: false));
            }

            if (selection.Gain)
            {
                var output = new float[currentUInt16.Length];
                stages.Add(CallStage(
                    "gain",
                    () => CallUInt16ToFloat(gainCorrect!, currentUInt16, output, preview.PreviewWidth, preview.PreviewHeight, ref metadata)));
                currentFloat = output;
            }
            else
            {
                stages.Add(new NativePreviewStageResult("gain", "Skipped", 0, Executed: false));
            }

            if (selection.Defect)
            {
                currentFloat ??= currentUInt16.Select(value => (float)value).ToArray();
                var output = new float[currentFloat.Length];
                stages.Add(CallStage(
                    "defect",
                    () => CallFloatToFloat(defectCorrect!, currentFloat, output, preview.PreviewWidth, preview.PreviewHeight, ref metadata)));
                currentFloat = output;
            }
            else
            {
                stages.Add(new NativePreviewStageResult("defect", "Skipped", 0, Executed: false));
            }

            stopwatch.Stop();

            if (currentFloat is not null)
            {
                var bitmap = RawPreviewService.CreateGray8Bitmap(
                    currentFloat,
                    preview.PreviewWidth,
                    preview.PreviewHeight,
                    out var min,
                    out var max);
                return new NativePreprocessPreviewResult(dllPath, stages, stopwatch.Elapsed.TotalMilliseconds, min, max, bitmap);
            }

            var outputMin = currentUInt16.Min();
            var outputMax = currentUInt16.Max();
            var ushortBitmap = RawPreviewService.CreateGray8Bitmap(
                currentUInt16,
                preview.PreviewWidth,
                preview.PreviewHeight,
                outputMin,
                outputMax);
            return new NativePreprocessPreviewResult(dllPath, stages, stopwatch.Elapsed.TotalMilliseconds, outputMin, outputMax, ushortBitmap);
        }

        private static NativePreviewStageResult CallStage(string stage, Func<XpeCommonApi.XpeErrorCode> call)
        {
            var stopwatch = Stopwatch.StartNew();
            var result = call();
            stopwatch.Stop();

            if (result != XpeCommonApi.XpeErrorCode.OK)
            {
                throw new InvalidOperationException($"{stage} returned {result}.");
            }

            return new NativePreviewStageResult(stage, result.ToString(), stopwatch.Elapsed.TotalMilliseconds, Executed: true);
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
