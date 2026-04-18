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
                var gain = new float[raw.Length];
                var defect = new float[raw.Length];
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
                    CallPinned(gainCorrect, offset, gain,
                        CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.UInt16, offset.Length * sizeof(ushort)),
                        CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.Float32, gain.Length * sizeof(float)),
                        ref metadata));

                Assert.All(Enumerable.Range(0, gain.Length), index => Assert.Equal(offset[index], gain[index]));

                Assert.Equal(
                    XpeCommonNative.XpeErrorCode.OK,
                    CallPinned(defectCorrect, gain, defect,
                        CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.Float32, gain.Length * sizeof(float)),
                        CreateBuffer(width, height, XpeCommonNative.XpePixelFormat.Float32, defect.Length * sizeof(float)),
                        ref metadata));

                Assert.All(Enumerable.Range(0, defect.Length), index => Assert.Equal(gain[index], defect[index]));
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
