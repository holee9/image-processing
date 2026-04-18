using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// Optional adapter-chain smoke test for the GUI-facing preprocess path.
/// Validates the public correction exports used by the WPF preview adapter.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessCorrectionChainSmokeTests
{
    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();

    [Fact]
    public void CorrectionChain_WithoutCalibration_PreservesSyntheticInput()
    {
        if (DllPath is null) return;
        if (!NativeLibrary.TryLoad(DllPath, out var handle)) return;

        try
        {
            RunSingleCorrectionPass(handle, out _, out _);
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-061 (Optional): running the synthetic correction chain twice with
    /// identical input MUST produce bit-identical output — RMSE between runs == 0.
    /// Any non-determinism would break reproducibility guarantees for regulated workflows.
    /// </summary>
    [Fact]
    public void CorrectionChain_RunTwice_DeterministicRmseIsZero()
    {
        if (DllPath is null) return;
        if (!NativeLibrary.TryLoad(DllPath, out var handle)) return;

        try
        {
            RunSingleCorrectionPass(handle, out var firstGain, out var firstDefect);
            RunSingleCorrectionPass(handle, out var secondGain, out var secondDefect);

            Assert.Equal(firstGain.Length, secondGain.Length);
            Assert.Equal(firstDefect.Length, secondDefect.Length);

            Assert.Equal(0.0, ComputeRmse(firstGain, secondGain));
            Assert.Equal(0.0, ComputeRmse(firstDefect, secondDefect));
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-061 (Optional): output of the correction chain must contain no NaN
    /// or Infinity values. Such sentinels would propagate through downstream algorithms
    /// (windowing, edge enhancement) and corrupt diagnostic output.
    /// </summary>
    [Fact]
    public void CorrectionChain_Output_HasNoNanOrInf()
    {
        if (DllPath is null) return;
        if (!NativeLibrary.TryLoad(DllPath, out var handle)) return;

        try
        {
            RunSingleCorrectionPass(handle, out var gain, out var defect);
            AssertAllFinite(gain, nameof(gain));
            AssertAllFinite(defect, nameof(defect));
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    private static void AssertAllFinite(float[] data, string label)
    {
        for (var i = 0; i < data.Length; i++)
        {
            var v = data[i];
            Assert.False(float.IsNaN(v), $"{label}[{i}] is NaN");
            Assert.False(float.IsInfinity(v), $"{label}[{i}] is Infinity");
        }
    }

    private static double ComputeRmse(float[] a, float[] b)
    {
        double sumSq = 0.0;
        for (var i = 0; i < a.Length; i++)
        {
            var d = (double)a[i] - b[i];
            sumSq += d * d;
        }
        return Math.Sqrt(sumSq / a.Length);
    }

    private static void RunSingleCorrectionPass(IntPtr handle, out float[] gain, out float[] defect)
    {
        var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
        var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
        var offsetCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_offset_correct");
        var gainCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_gain_correct");
        var defectCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_defect_correct");

        shutdown();
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

        try
        {
            const int width = 16;
            const int height = 16;
            var raw = Enumerable.Range(0, width * height).Select(index => (ushort)(1000 + index)).ToArray();
            var offset = new ushort[raw.Length];
            var gainBuf = new float[raw.Length];
            var defectBuf = new float[raw.Length];
            var metadata = CreateMetadata();

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                CallPinned(offsetCorrect, raw, offset,
                    CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.UInt16, raw.Length * sizeof(ushort)),
                    CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.UInt16, offset.Length * sizeof(ushort)),
                    ref metadata));

            Assert.Equal(raw, offset);

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                CallPinned(gainCorrect, offset, gainBuf,
                    CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.UInt16, offset.Length * sizeof(ushort)),
                    CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.Float32, gainBuf.Length * sizeof(float)),
                    ref metadata));

            Assert.All(Enumerable.Range(0, gainBuf.Length), index => Assert.Equal(offset[index], gainBuf[index]));

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                CallPinned(defectCorrect, gainBuf, defectBuf,
                    CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.Float32, gainBuf.Length * sizeof(float)),
                    CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.Float32, defectBuf.Length * sizeof(float)),
                    ref metadata));

            Assert.All(Enumerable.Range(0, defectBuf.Length), index => Assert.Equal(gainBuf[index], defectBuf[index]));

            gain = gainBuf;
            defect = defectBuf;
        }
        finally
        {
            shutdown();
        }
    }

    private static TDelegate GetDelegate<TDelegate>(IntPtr handle, string exportName)
        where TDelegate : Delegate
    {
        Assert.True(NativeLibrary.TryGetExport(handle, exportName, out var symbol), $"Export '{exportName}' not found.");
        return Marshal.GetDelegateForFunctionPointer<TDelegate>(symbol);
    }

    private static XpeCommonNative.XpeErrorCode CallPinned<TInput, TOutput>(
        XpePreprocessNative.CorrectionDelegate correction,
        TInput[] input,
        TOutput[] output,
        XpeCommonNative.XpeImageBuffer inputBuffer,
        XpeCommonNative.XpeImageBuffer outputBuffer,
        ref XpeCommonNative.XpeImageMetadata metadata)
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

    private static XpeCommonNative.XpeImageBuffer CreateBuffer(
        int width,
        int height,
        XpeCommonNative.XpePixelFormat format,
        int dataSize)
    {
        return new XpeCommonNative.XpeImageBuffer
        {
            Width = (uint)width,
            Height = (uint)height,
            BitsAllocated = format == XpeCommonNative.XpePixelFormat.UInt16 ? 16u : 32u,
            BitsStored = format == XpeCommonNative.XpePixelFormat.UInt16 ? 16u : 32u,
            Format = format,
            DataSize = (nuint)dataSize
        };
    }

    private static XpeCommonNative.XpeImageMetadata CreateMetadata()
    {
        return new XpeCommonNative.XpeImageMetadata
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
}
