using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// Adapter-chain smoke tests for the GUI-facing preprocess path, written against the
/// api-spec "Calibration state model" (#117): correction functions read their map from
/// the module-global calibration store, and return XPE_ERR_CALIB_NOT_LOADED (-16) when
/// the module is initialised but the map they need was never loaded.
///
/// These tests previously assumed an uncalibrated chain preserves its input. That
/// behaviour is not in the contract and the native side never promised it, so the tests
/// were rewritten rather than the implementation (#117 B).
///
/// Calibration is produced in-test from synthetic frames via
/// xpe_calib_generate_offset / xpe_calib_generate_gain into a temp directory. The repo's
/// tests/test_data/calibration_cases fixtures are deliberately NOT used: they are
/// git-ignored local media and are absent in CI, so a test depending on them would
/// silently skip there.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessCorrectionChainSmokeTests
{
    private const int Width = 16;
    private const int Height = 16;
    private const int PixelCount = Width * Height;

    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();
    private static readonly string SkipReason = DllPath is null
        ? "Skipped: xpe_preprocess.dll not staged"
        : string.Empty;

    /// <summary>
    /// #117: with the module initialised but no map loaded, every correction entry point
    /// returns CALIB_NOT_LOADED — not OK, and not NOT_INITIALIZED.
    /// </summary>
    [SkippableFact]
    public void CorrectionChain_WithoutCalibration_ReturnsCalibNotLoaded()
    {
        var handle = LoadDll();
        try
        {
            var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");

            // shutdown() first drops any calibration a previous test in this process loaded.
            shutdown();
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

            try
            {
                var raw = SyntheticUInt16();
                var u16Out = new ushort[PixelCount];
                var f32In = SyntheticFloat32();
                var f32Out = new float[PixelCount];
                var metadata = CreateMetadata();

                Assert.Equal(
                    XpeCommonNative.XpeErrorCode.CALIB_NOT_LOADED,
                    CallPinned(GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_offset_correct"),
                        raw, u16Out,
                        MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort)),
                        MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort)),
                        ref metadata));

                Assert.Equal(
                    XpeCommonNative.XpeErrorCode.CALIB_NOT_LOADED,
                    CallPinned(GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_gain_correct"),
                        raw, f32Out,
                        MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort)),
                        MakeBuffer(XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount * sizeof(float)),
                        ref metadata));

                Assert.Equal(
                    XpeCommonNative.XpeErrorCode.CALIB_NOT_LOADED,
                    CallPinned(GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_defect_correct"),
                        f32In, f32Out,
                        MakeBuffer(XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount * sizeof(float)),
                        MakeBuffer(XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount * sizeof(float)),
                        ref metadata));
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

    /// <summary>
    /// REQ-GUI-IT-061: with calibration loaded, running the offset→gain chain twice on
    /// identical input MUST produce bit-identical output — RMSE between runs == 0.
    /// Non-determinism would break reproducibility for regulated workflows.
    /// </summary>
    [SkippableFact]
    public void CorrectionChain_RunTwice_DeterministicRmseIsZero()
    {
        var handle = LoadDll();
        try
        {
            RunCalibratedChain(handle, out var first);
            RunCalibratedChain(handle, out var second);

            Assert.Equal(first.Length, second.Length);
            Assert.Equal(0.0, ComputeRmse(first, second));
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-061: output of the calibrated chain must contain no NaN or Infinity.
    /// Such sentinels would propagate through windowing and edge enhancement downstream.
    /// </summary>
    [SkippableFact]
    public void CorrectionChain_Output_HasNoNanOrInf()
    {
        var handle = LoadDll();
        try
        {
            RunCalibratedChain(handle, out var gain);
            for (var i = 0; i < gain.Length; i++)
            {
                Assert.False(float.IsNaN(gain[i]), $"gain[{i}] is NaN");
                Assert.False(float.IsInfinity(gain[i]), $"gain[{i}] is Infinity");
            }
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    // ---------- helpers ----------

    private static IntPtr LoadDll()
    {
        SkipHelper.SkipIf(DllPath is null, SkipReason);
        SkipHelper.SkipIf(
            !NativeLibrary.TryLoad(DllPath!, out var handle),
            $"Skipped: xpe_preprocess.dll load failed: {DllPath}");
        return handle;
    }

    /// <summary>
    /// init → generate+load offset and gain maps → offset_correct → gain_correct.
    /// Returns the FLOAT32 gain-stage output. Every native call's return code is asserted,
    /// so a failure names the step that broke rather than surfacing as odd pixels.
    /// </summary>
    private static void RunCalibratedChain(IntPtr handle, out float[] gainOutput)
    {
        var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
        var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
        var offsetCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_offset_correct");
        var gainCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_gain_correct");

        shutdown();
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

        var tempDir = Path.Combine(Path.GetTempPath(), $"xpe_calib_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        try
        {
            GenerateAndLoadCalibration(handle, tempDir);

            var raw = SyntheticUInt16();
            var offsetOut = new ushort[PixelCount];
            var gainOut = new float[PixelCount];
            var metadata = CreateMetadata();

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                CallPinned(offsetCorrect, raw, offsetOut,
                    MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort)),
                    MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort)),
                    ref metadata));

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                CallPinned(gainCorrect, offsetOut, gainOut,
                    MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort)),
                    MakeBuffer(XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount * sizeof(float)),
                    ref metadata));

            gainOutput = gainOut;
        }
        finally
        {
            shutdown();
            try { Directory.Delete(tempDir, recursive: true); } catch (IOException) { /* temp dir, best effort */ }
        }
    }

    /// <summary>Generates offset and gain maps from synthetic frames and loads them into the global store.</summary>
    private static void GenerateAndLoadCalibration(IntPtr handle, string tempDir)
    {
        var generateOffset = GetDelegate<XpePreprocessNative.CalibGenerateOffsetDelegate>(handle, "xpe_calib_generate_offset");
        var generateGain = GetDelegate<XpePreprocessNative.CalibGenerateGainDelegate>(handle, "xpe_calib_generate_gain");
        var loadOffset = GetDelegate<XpePreprocessNative.CalibLoadDelegate>(handle, "xpe_calib_load_offset");
        var loadGain = GetDelegate<XpePreprocessNative.CalibLoadDelegate>(handle, "xpe_calib_load_gain");

        var offsetPath = Path.Combine(tempDir, "offset.xcal");
        var gainPath = Path.Combine(tempDir, "gain.xcal");

        // Dark frames: a flat low-level pedestal. Flat frames: a brighter uniform field.
        var dark = new ushort[PixelCount];
        var flat = new ushort[PixelCount];
        for (var i = 0; i < PixelCount; i++)
        {
            dark[i] = 100;
            flat[i] = 2000;
        }

        var darkHandle = GCHandle.Alloc(dark, GCHandleType.Pinned);
        var flatHandle = GCHandle.Alloc(flat, GCHandleType.Pinned);
        try
        {
            var darkFrame = MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort));
            darkFrame.Data = darkHandle.AddrOfPinnedObject();
            var flatFrame = MakeBuffer(XpeCommonNative.XpePixelFormat.UInt16, 16, PixelCount * sizeof(ushort));
            flatFrame.Data = flatHandle.AddrOfPinnedObject();

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                generateOffset(new[] { darkFrame }, 1, 100.0f, 25.0f, offsetPath));
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, loadOffset(offsetPath));

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                generateGain(new[] { flatFrame }, 1, IntPtr.Zero, gainPath, null));
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, loadGain(gainPath));
        }
        finally
        {
            flatHandle.Free();
            darkHandle.Free();
        }
    }

    private static ushort[] SyntheticUInt16()
    {
        var raw = new ushort[PixelCount];
        for (var i = 0; i < PixelCount; i++)
            raw[i] = (ushort)(1000 + i);
        return raw;
    }

    private static float[] SyntheticFloat32()
    {
        var raw = new float[PixelCount];
        for (var i = 0; i < PixelCount; i++)
            raw[i] = 1000.0f + i;
        return raw;
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

    private static XpeCommonNative.XpeImageBuffer MakeBuffer(
        XpeCommonNative.XpePixelFormat format, uint bits, int dataSize) =>
        new()
        {
            Width = Width,
            Height = Height,
            BitsAllocated = bits,
            BitsStored = bits,
            Format = format,
            DataSize = (nuint)dataSize,
        };

    private static XpeCommonNative.XpeImageMetadata CreateMetadata() =>
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
}
