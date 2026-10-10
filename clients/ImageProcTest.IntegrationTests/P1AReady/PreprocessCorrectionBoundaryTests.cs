// GUI-C-214 (#249): the corrections write exactly the output buffer they are given, and read their input without changing it.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;
using static ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// GUI-C-213 found a caller that copied 1024 bytes out of a 512-byte buffer because it read the wrong argument as the result. A buffer boundary is only observed by looking past it: each
/// output here lives in the MIDDLE of a larger pinned array whose margins hold a sentinel pattern, so a write one element before or after the buffer changes a margin and fails the test.
/// The input array is copied before the call and compared after. The calls use the header's shape (input, output, metadata) with the calibration loaded as the chain tests load it.
/// </summary>
[Trait("Category", "P1AReady")]
[Collection(ImageProcTest.IntegrationTests.Fixtures.PreprocessModuleCollection.Name)]
public sealed class PreprocessCorrectionBoundaryTests
{
    private const int Guard = 64;   // elements of margin on each side of the output

    /// <summary>The sentinel: a bit pattern no correction writes (not a plausible pixel, not zero, and a quiet NaN for floats).</summary>
    private const uint Pattern = 0x7FC0A5A5;

    private static ushort[] GuardedU16() => Enumerable.Repeat((ushort)(Pattern & 0xFFFF), PixelCount + 2 * Guard).ToArray();

    private static float[] GuardedF32() => Enumerable.Repeat(BitConverter.UInt32BitsToSingle(Pattern), PixelCount + 2 * Guard).ToArray();

    private static bool MarginsIntact(ushort[] backing) =>
        backing.Take(Guard).Concat(backing.Skip(Guard + PixelCount)).All(v => v == (ushort)(Pattern & 0xFFFF));

    private static bool MarginsIntact(float[] backing) =>
        backing.Take(Guard).Concat(backing.Skip(Guard + PixelCount)).All(v => BitConverter.SingleToUInt32Bits(v) == Pattern);

    /// <summary>The checker itself: a write into a margin, one element before or one after, is seen (a check that cannot fail proves nothing).</summary>
    [Fact]
    public void Control_AWriteIntoEitherMargin_IsDetected()
    {
        var u16 = GuardedU16();
        Assert.True(MarginsIntact(u16));
        u16[Guard - 1] = 0; Assert.False(MarginsIntact(u16));
        u16[Guard - 1] = (ushort)(Pattern & 0xFFFF); Assert.True(MarginsIntact(u16));
        u16[Guard + PixelCount] = 0; Assert.False(MarginsIntact(u16));

        var f32 = GuardedF32();
        Assert.True(MarginsIntact(f32));
        f32[Guard + PixelCount] = 1.0f; Assert.False(MarginsIntact(f32));
        f32[Guard + PixelCount] = BitConverter.UInt32BitsToSingle(Pattern); f32[Guard - 1] = 1.0f; Assert.False(MarginsIntact(f32));
    }

    [SkippableFact]
    public void Offset_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone()
    {
        var handle = LoadCalibrated();
        try
        {
            var input = ChainInput();
            var snapshot = (ushort[])input.Clone();
            var backing = GuardedU16();
            var code = Call(handle, "xpe_offset_correct", input, XpeCommonNative.XpePixelFormat.UInt16, 16, backing, XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount);
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, code);
            Assert.True(MarginsIntact(backing), "xpe_offset_correct wrote outside the output buffer");
            Assert.Equal(snapshot, input);
            Assert.Contains(backing.Skip(Guard).Take(PixelCount), v => v != (ushort)(Pattern & 0xFFFF));   // and it DID write inside (the margins are not intact merely because nothing ran)
        }
        finally { Release(handle); }
    }

    [SkippableFact]
    public void Gain_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone()
    {
        var handle = LoadCalibrated();
        try
        {
            var input = ChainInput();
            var snapshot = (ushort[])input.Clone();
            var backing = GuardedF32();
            var code = Call(handle, "xpe_gain_correct", input, XpeCommonNative.XpePixelFormat.UInt16, 16, backing, XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount);
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, code);
            Assert.True(MarginsIntact(backing), "xpe_gain_correct wrote outside the output buffer");
            Assert.Equal(snapshot, input);
            Assert.Contains(backing.Skip(Guard).Take(PixelCount), v => BitConverter.SingleToUInt32Bits(v) != Pattern);
        }
        finally { Release(handle); }
    }

    [SkippableFact]
    public void Defect_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone()
    {
        var handle = LoadCalibrated();
        try
        {
            var input = Enumerable.Range(0, PixelCount).Select(i => 1000f + i).ToArray();
            input[DefectPixels[0]] = 60000f;
            var snapshot = (float[])input.Clone();
            var backing = GuardedF32();
            var code = Call(handle, "xpe_defect_correct", input, XpeCommonNative.XpePixelFormat.Float32, 32, backing, XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount);
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, code);
            Assert.True(MarginsIntact(backing), "xpe_defect_correct wrote outside the output buffer");
            Assert.Equal(snapshot, input);
            Assert.NotEqual(60000f, backing[Guard + DefectPixels[0]]);   // and it corrected the marked pixel inside
        }
        finally { Release(handle); }
    }

    /// <summary>An output buffer that says it is one pixel too small is refused, and nothing is written past what it declared.</summary>
    [SkippableFact]
    public void AnOutputBufferDeclaredTooSmall_IsRefused_WithoutWritingBeyondIt()
    {
        var handle = LoadCalibrated();
        try
        {
            var input = ChainInput();
            var backing = GuardedU16();
            var code = Call(handle, "xpe_offset_correct", input, XpeCommonNative.XpePixelFormat.UInt16, 16, backing, XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount - 1);
            Assert.NotEqual(XpeCommonNative.XpeErrorCode.OK, code);
            Assert.True(MarginsIntact(backing), "a refused call still wrote outside the declared output buffer");
            Assert.All(backing.Skip(Guard).Take(PixelCount), v => Assert.Equal((ushort)(Pattern & 0xFFFF), v));   // refused means untouched, not half done
        }
        finally { Release(handle); }
    }

    // ---- plumbing

    /// <summary>
    /// Loads the DLL, initialises the module and loads a calibration. GUI-C-233j: when anything after the successful <c>init</c> throws, the module is shut down again before the exception leaves
    /// (it used to free only the library handle and leave the module initialised for the next test); <paramref name="generate"/> lets a test inject that exception.
    /// </summary>
    internal static IntPtr LoadCalibrated(Action<IntPtr, string>? generate = null)
    {
        var handle = LoadDll();
        XpePreprocessNative.ShutdownDelegate? undoInit = null;
        try
        {
            var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
            shutdown();
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));
            undoInit = shutdown;
            var dir = Path.Combine(Path.GetTempPath(), $"xpe_bound_{Guid.NewGuid():N}");
            Directory.CreateDirectory(dir);
            try { (generate ?? GenerateAndLoadCalibration)(handle, dir); }
            finally { try { Directory.Delete(dir, recursive: true); } catch (IOException) { /* temp folder */ } }
        }
        catch
        {
            undoInit?.Invoke();
            NativeLibrary.Free(handle);
            throw;
        }

        return handle;
    }

    /// <summary>GUI-C-233j: the exception path of <see cref="LoadCalibrated"/> ends with the module UNINITIALISED (an injected failure after the init, then a first init must be accepted).</summary>
    [SkippableFact]
    public void LoadCalibrated_WhenTheCalibrationStepThrows_LeavesTheModuleUninitialised()
    {
        // The library is held loaded for the whole test: freeing the last handle unloads the DLL and with it the module's state, which would hide a leak (LoadDll also skips here when it is not staged).
        var keepLoaded = LoadDll();
        try
        {
            Assert.Throws<InvalidOperationException>(() => LoadCalibrated((_, _) => throw new InvalidOperationException("injected after init")));
        }
        catch
        {
            NativeLibrary.Free(keepLoaded);
            throw;
        }

        var handle = keepLoaded;
        try
        {
            var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
            var code = init(IntPtr.Zero);
            shutdown();
            Assert.True(code == XpeCommonNative.XpeErrorCode.OK, $"the module was left initialised by LoadCalibrated's exception path (init answered {code})");
        }
        finally { NativeLibrary.Free(handle); }
    }

    private static void Release(IntPtr handle)
    {
        GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown")();
        NativeLibrary.Free(handle);
    }

    /// <summary>Calls a correction with the output buffer pointing at the middle of <paramref name="backing"/> and declaring <paramref name="declaredPixels"/> pixels.</summary>
    private static XpeCommonNative.XpeErrorCode Call<TIn, TOut>(
        IntPtr handle, string export,
        TIn[] input, XpeCommonNative.XpePixelFormat inFormat, uint inBits,
        TOut[] backing, XpeCommonNative.XpePixelFormat outFormat, uint outBits, int declaredPixels)
        where TIn : struct where TOut : struct
    {
        var correction = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, export);
        var inHandle = GCHandle.Alloc(input, GCHandleType.Pinned);
        var outHandle = GCHandle.Alloc(backing, GCHandleType.Pinned);
        try
        {
            var inBuffer = MakeBuffer(inFormat, inBits, PixelCount * Marshal.SizeOf<TIn>());
            inBuffer.Data = inHandle.AddrOfPinnedObject();
            var outBuffer = MakeBuffer(outFormat, outBits, declaredPixels * Marshal.SizeOf<TOut>());
            outBuffer.Data = outHandle.AddrOfPinnedObject() + Guard * Marshal.SizeOf<TOut>();
            var metadata = CreateMetadata();
            return correction(ref inBuffer, ref outBuffer, ref metadata);
        }
        finally
        {
            outHandle.Free();
            inHandle.Free();
        }
    }
}
