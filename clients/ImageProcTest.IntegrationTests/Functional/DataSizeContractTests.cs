// REQ #123: client-side regression for the XpeImageBuffer.dataSize input contract.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Observes the three clauses of the api-spec "XpeImageBuffer.dataSize on input"
/// contract (docs/project/api-spec.md, #123) from the client side, across the two
/// native modules this client actually calls:
///
///   exact dataSize            -> accepted
///   0 &lt; dataSize &lt; required  -> XPE_ERR_INVALID_INPUT
///   dataSize == 0             -> accepted (unspecified, legacy behaviour)
///
/// The backing allocation is always FULL; only the declared dataSize is short, so a
/// call that slips past the guard reads inside its own allocation and fails on the
/// return code rather than by corrupting memory.
///
/// Only return codes are asserted. Pixel-value behaviour of these entry points is
/// covered elsewhere and is deliberately not re-checked here.
/// </summary>
[Trait("Category", "Functional")]
public sealed class DataSizeContractTests
{
    private const uint Width = 16;
    private const uint Height = 16;
    private const int PixelCount = (int)(Width * Height);

    // ---------- axis 1: xpe_preprocess.dll / xpe_offset_correct (UINT16) ----------

    private static readonly string? PreprocessDllPath = XpePreprocessNative.TryFindDll();

    private static readonly string PreprocessSkipReason = PreprocessDllPath is null
        ? "Skipped: xpe_preprocess.dll not staged — build P1A first or set XPE_NATIVE_DIR"
        : string.Empty;

    /// <summary>
    /// #123: exact input dataSize passes the size gate in xpe_offset_correct.
    /// Without calibration loaded the call then stops at the NEXT gate and returns
    /// NOT_INITIALIZED — that is still an observation that the size gate did not
    /// reject, because the size check precedes the calibration check in the native
    /// implementation. Either outcome proves the same thing; INVALID_INPUT would not.
    /// </summary>
    [SkippableFact]
    public void OffsetCorrect_ExactDataSize_PassesSizeGate()
    {
        var code = RunOffsetCorrect(inputDataSize: (nuint)(PixelCount * sizeof(ushort)));
        AssertPassedSizeGate(code);
    }

    /// <summary>#123: a non-zero input dataSize below width*height*2 is refused.</summary>
    [SkippableFact]
    public void OffsetCorrect_ShortDataSize_ReturnsInvalidInput()
    {
        var code = RunOffsetCorrect(inputDataSize: (nuint)(PixelCount * sizeof(ushort) / 2));
        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, code);
    }

    /// <summary>#123: dataSize 0 means unspecified and stays accepted by the size gate.</summary>
    [SkippableFact]
    public void OffsetCorrect_ZeroDataSize_PassesSizeGate()
    {
        var code = RunOffsetCorrect(inputDataSize: 0);
        AssertPassedSizeGate(code);
    }

    /// <summary>
    /// The size gate rejects with INVALID_INPUT and nothing else, so any other code
    /// means the call got past it. Naming the two codes we actually expect keeps this
    /// from degenerating into "not INVALID_INPUT", which would pass on any failure.
    /// </summary>
    private static void AssertPassedSizeGate(XpeCommonNative.XpeErrorCode code) =>
        Assert.True(
            code is XpeCommonNative.XpeErrorCode.OK or XpeCommonNative.XpeErrorCode.NOT_INITIALIZED,
            $"Expected the dataSize gate to accept (OK, or NOT_INITIALIZED at the next gate), got {code}");

    // ---------- axis 2: xpe_enhance_basic.dll / xpe_log_transform (FLOAT32) ----------

    private static readonly string? EnhanceBasicDllPath = XpeEnhanceBasicNative.TryFindDll();

    private static readonly string EnhanceBasicSkipReason = EnhanceBasicDllPath is null
        ? "Skipped: xpe_enhance_basic.dll not staged — set XPE_NATIVE_DIR or stage the ci-post artifact"
        : string.Empty;

    /// <summary>#123: exact dataSize is accepted by xpe_log_transform.</summary>
    [SkippableFact]
    public void LogTransform_ExactDataSize_IsAccepted()
    {
        var code = RunLogTransform(dataSize: (nuint)(PixelCount * sizeof(float)));
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, code);
    }

    /// <summary>#123: a non-zero dataSize below width*height*4 is refused.</summary>
    [SkippableFact]
    public void LogTransform_ShortDataSize_ReturnsInvalidInput()
    {
        var code = RunLogTransform(dataSize: (nuint)(PixelCount * sizeof(float) / 2));
        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, code);
    }

    /// <summary>#123: dataSize 0 means unspecified and stays accepted.</summary>
    [SkippableFact]
    public void LogTransform_ZeroDataSize_IsAccepted()
    {
        var code = RunLogTransform(dataSize: 0);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, code);
    }

    // ---------- helpers ----------

    /// <summary>
    /// Calls xpe_offset_correct with a fully allocated UINT16 input whose DECLARED
    /// input dataSize is <paramref name="inputDataSize"/>. The output buffer always
    /// declares its exact size — a short output is BUFFER_TOO_SMALL, a different clause.
    /// </summary>
    private static XpeCommonNative.XpeErrorCode RunOffsetCorrect(nuint inputDataSize)
    {
        SkipHelper.SkipIf(PreprocessDllPath is null, PreprocessSkipReason);
        SkipHelper.SkipIf(
            !NativeLibrary.TryLoad(PreprocessDllPath!, out var handle),
            $"Skipped: xpe_preprocess.dll load failed: {PreprocessDllPath}");

        var inputPixels = new ushort[PixelCount];
        var outputPixels = new ushort[PixelCount];
        for (var i = 0; i < PixelCount; i++)
            inputPixels[i] = (ushort)(1000 + i);

        var inputHandle = GCHandle.Alloc(inputPixels, GCHandleType.Pinned);
        var outputHandle = GCHandle.Alloc(outputPixels, GCHandleType.Pinned);
        try
        {
            var init = GetExport<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetExport<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
            var offsetCorrect = GetExport<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_offset_correct");

            shutdown();
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

            var input = MakeBuffer(
                XpeCommonNative.XpePixelFormat.UInt16, 16,
                inputHandle.AddrOfPinnedObject(), inputDataSize);
            var output = MakeBuffer(
                XpeCommonNative.XpePixelFormat.UInt16, 16,
                outputHandle.AddrOfPinnedObject(), (nuint)(PixelCount * sizeof(ushort)));
            var metadata = MakeMetadata();

            return offsetCorrect(ref input, ref output, ref metadata);
        }
        finally
        {
            outputHandle.Free();
            inputHandle.Free();
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// Calls xpe_log_transform in place on a fully allocated FLOAT32 image whose
    /// DECLARED dataSize is <paramref name="dataSize"/>.
    /// </summary>
    private static XpeCommonNative.XpeErrorCode RunLogTransform(nuint dataSize)
    {
        SkipHelper.SkipIf(EnhanceBasicDllPath is null, EnhanceBasicSkipReason);
        SkipHelper.SkipIf(
            !NativeLibrary.TryLoad(EnhanceBasicDllPath!, out var handle),
            $"Skipped: xpe_enhance_basic.dll load failed: {EnhanceBasicDllPath}");

        var pixels = new float[PixelCount];
        for (var i = 0; i < PixelCount; i++)
            pixels[i] = 0.5f;

        var pinned = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        try
        {
            var logTransform = GetExport<XpeEnhanceBasicNative.LogTransformDelegate>(handle, "xpe_log_transform");

            var image = MakeBuffer(
                XpeCommonNative.XpePixelFormat.Float32, 32,
                pinned.AddrOfPinnedObject(), dataSize);

            return logTransform(ref image, 1.0f);
        }
        finally
        {
            pinned.Free();
            NativeLibrary.Free(handle);
        }
    }

    private static XpeCommonNative.XpeImageBuffer MakeBuffer(
        XpeCommonNative.XpePixelFormat format, uint bits, IntPtr data, nuint dataSize) =>
        new()
        {
            Width = Width,
            Height = Height,
            BitsAllocated = bits,
            BitsStored = bits,
            Format = format,
            Data = data,
            DataSize = dataSize,
        };

    private static XpeCommonNative.XpeImageMetadata MakeMetadata() =>
        new()
        {
            BodyPart = "CHEST",
            KVp = 70.0f,
            MAs = 2.0f,
            SID_mm = 1000.0f,
            PixelPitch_mm = 0.14f,
            AcquisitionTime = 0,
            Flags = 0,
        };

    private static T GetExport<T>(IntPtr handle, string name) where T : Delegate
    {
        Assert.True(NativeLibrary.TryGetExport(handle, name, out var sym), $"Export '{name}' not found");
        return Marshal.GetDelegateForFunctionPointer<T>(sym);
    }
}
