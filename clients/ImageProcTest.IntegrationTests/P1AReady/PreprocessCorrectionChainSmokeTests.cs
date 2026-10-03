using System.Runtime.InteropServices;
using System.Security.Cryptography;
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
    internal const int Width = 16;
    internal const int Height = 16;
    internal const int PixelCount = Width * Height;

    /// <summary>
    /// Pixels the chain's defect map marks (GUI-C-210, REQ-GUI-IT-061). Two isolated pixels well apart: each has eight unmarked neighbours, so the correction's
    /// fill is a plain neighbour statistic. The synthetic input carries a hot value there (<see cref="HotValue"/>), so a defect stage that does nothing leaves a value
    /// 59,000 ADU away from its neighbours — a correction cannot go unnoticed.
    /// </summary>
    internal static readonly int[] DefectPixels = [5 * Width + 5, 12 * Width + 9];

    private const ushort HotValue = 60000;

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
    /// REQ-GUI-IT-061: with calibration loaded, running the offset→gain→defect chain twice on
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
    /// REQ-GUI-IT-061: output of the three-stage (offset→gain→defect) calibrated chain must contain no NaN or Infinity.
    /// Such sentinels would propagate through windowing and edge enhancement downstream.
    /// </summary>
    [SkippableFact]
    public void CorrectionChain_Output_HasNoNanOrInf()
    {
        var handle = LoadDll();
        try
        {
            RunCalibratedChain(handle, out var output);
            for (var i = 0; i < output.Length; i++)
            {
                Assert.False(float.IsNaN(output[i]), $"output[{i}] is NaN");
                Assert.False(float.IsInfinity(output[i]), $"output[{i}] is Infinity");
            }
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-061 (GUI-C-210), the defect stage. The map is really loaded (<c>xpe_calib_load_defect_map</c> must return OK; without it the stage answers CALIB_NOT_LOADED, see the
    /// first test) and really used: the pixels it marks hold a hot value (60,000) in the input, and after the stage each of them lies within the range of its eight neighbours
    /// (the correction fills a defective pixel from its neighbours), far from the hot value, while EVERY OTHER pixel is bit-identical to what the gain stage produced.
    /// </summary>
    [SkippableFact]
    public void CorrectionChain_DefectStage_CorrectsExactlyTheMarkedPixels_AndLeavesTheRestBitIdentical()
    {
        var handle = LoadDll();
        try
        {
            var run = RunChain(handle);

            foreach (var i in DefectPixels)
            {
                Assert.True(run.GainOutput[i] > 50000f, $"setup: the hot input must still be hot after the gain stage (pixel {i}: {run.GainOutput[i]})");
                var neighbours = Neighbours(i).Select(n => run.GainOutput[n]).ToArray();
                Assert.InRange(run.DefectOutput[i], neighbours.Min(), neighbours.Max());
                Assert.True(Math.Abs(run.DefectOutput[i] - run.GainOutput[i]) > 50000f, $"pixel {i} was not corrected: {run.GainOutput[i]} -> {run.DefectOutput[i]}");
            }

            var changed = Enumerable.Range(0, PixelCount)
                .Where(i => BitConverter.SingleToInt32Bits(run.DefectOutput[i]) != BitConverter.SingleToInt32Bits(run.GainOutput[i]))
                .ToArray();
            Assert.Equal(DefectPixels.OrderBy(i => i), changed);
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-061 (GUI-C-210): the input buffer's SHA-256 is the same after the chain as before — the stages read their input and write their own output. The instrument is held by two
    /// checks: the "before" hash equals the hash of an independently regenerated copy of the input (it is the hash of the right thing), and
    /// <see cref="Control_Sha256Hex_SeesAOneByteChange"/> shows the hash sees a one-byte change (a hash that cannot change would pass any regression).
    /// </summary>
    [SkippableFact]
    public void CorrectionChain_InputBuffer_Sha256IsPreserved()
    {
        var handle = LoadDll();
        try
        {
            var run = RunChain(handle);
            Assert.Equal(Sha256Hex(ChainInput()), run.InputSha256Before);
            Assert.Equal(run.InputSha256Before, run.InputSha256After);
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    /// <summary>The hash the preservation test relies on changes when one byte of the input changes (it would otherwise pass an in-place write).</summary>
    [Fact]
    public void Control_Sha256Hex_SeesAOneByteChange()
    {
        var input = ChainInput();
        var before = Sha256Hex(input);
        input[DefectPixels[0] + 1] ^= 1;
        Assert.NotEqual(before, Sha256Hex(input));
    }

    private static IEnumerable<int> Neighbours(int index)
    {
        var (row, col) = (index / Width, index % Width);
        for (var dr = -1; dr <= 1; dr++)
        {
            for (var dc = -1; dc <= 1; dc++)
            {
                if ((dr, dc) == (0, 0)) continue;
                var (r, c) = (row + dr, col + dc);
                if (r >= 0 && r < Height && c >= 0 && c < Width) yield return r * Width + c;
            }
        }
    }

    // ---------- helpers ----------

    internal static IntPtr LoadDll()
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
    private static void RunCalibratedChain(IntPtr handle, out float[] output)
    {
        output = RunChain(handle).DefectOutput;
    }

    /// <summary>One run of the whole chain: what went in, what each stage produced, and the input's hash before and after.</summary>
    private sealed record ChainRun(ushort[] Input, string InputSha256Before, string InputSha256After, float[] GainOutput, float[] DefectOutput);

    /// <summary>The input of the chain: a ramp, with a hot value at every pixel the defect map marks.</summary>
    internal static ushort[] ChainInput()
    {
        var raw = SyntheticUInt16();
        foreach (var i in DefectPixels) raw[i] = HotValue;
        return raw;
    }

    private static string Sha256Hex<T>(T[] values) where T : struct =>
        Convert.ToHexString(SHA256.HashData(MemoryMarshal.AsBytes(values.AsSpan())));

    /// <summary>
    /// A DEFECT XCal file for <see cref="DefectPixels"/>: the 152-byte header of xcal_format.h (magic, version 1, type DEFECT, UINT8_MASK, 16x16, no expiry, empty session,
    /// no config, payload 256 bytes, SHA-256 of config||payload) and the mask. Written here because the module has no generator for defect maps.
    /// </summary>
    private static void WriteDefectMapFile(string path)
    {
        var mask = new byte[PixelCount];
        foreach (var i in DefectPixels) mask[i] = 1;

        using var stream = File.Create(path);
        using var w = new BinaryWriter(stream);
        w.Write("XCAL"u8);
        w.Write(1u);                      // version
        w.Write(2u);                      // type: DEFECT
        w.Write(2u);                      // pixel format: UINT8_MASK
        w.Write((uint)Width);
        w.Write((uint)Height);
        w.Write(DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
        w.Write(0L);                      // expiry: never
        w.Write(new byte[64]);            // session id
        w.Write(0UL);                     // config length
        w.Write((ulong)mask.Length);      // payload length
        w.Write(SHA256.HashData(mask));   // SHA-256 of (config || payload)
        w.Write(mask);
    }

    private static ChainRun RunChain(IntPtr handle)
    {
        var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
        var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
        var offsetCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_offset_correct");
        var gainCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_gain_correct");
        var defectCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_defect_correct");

        shutdown();
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

        var tempDir = Path.Combine(Path.GetTempPath(), $"xpe_calib_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        try
        {
            GenerateAndLoadCalibration(handle, tempDir);

            var raw = ChainInput();
            var shaBefore = Sha256Hex(raw);
            var offsetOut = new ushort[PixelCount];
            var gainOut = new float[PixelCount];
            var defectOut = new float[PixelCount];
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

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                CallPinned(defectCorrect, gainOut, defectOut,
                    MakeBuffer(XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount * sizeof(float)),
                    MakeBuffer(XpeCommonNative.XpePixelFormat.Float32, 32, PixelCount * sizeof(float)),
                    ref metadata));

            return new ChainRun(raw, shaBefore, Sha256Hex(raw), gainOut, defectOut);
        }
        finally
        {
            shutdown();
            try { Directory.Delete(tempDir, recursive: true); } catch (IOException) { /* temp dir, best effort */ }
        }
    }

    /// <summary>Generates offset and gain maps from synthetic frames and loads them into the global store.</summary>
    internal static void GenerateAndLoadCalibration(IntPtr handle, string tempDir)
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
                // null config (#138): the defaults, which the header defines as the pre-parameter
                // behaviour — this smoke test measures the chain, not the generation method.
                generateOffset(new[] { darkFrame }, 1, 100.0f, 25.0f, offsetPath, null));
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, loadOffset(offsetPath));

            Assert.Equal(
                XpeCommonNative.XpeErrorCode.OK,
                generateGain(new[] { flatFrame }, 1, IntPtr.Zero, gainPath, null));
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, loadGain(gainPath));

            // The defect map is loaded for real, from an XCal file the test writes (there is no generator for it in the module's API).
            var defectPath = Path.Combine(tempDir, "defect.xcal");
            WriteDefectMapFile(defectPath);
            var loadDefect = GetDelegate<XpePreprocessNative.CalibLoadDelegate>(handle, "xpe_calib_load_defect_map");
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, loadDefect(defectPath));
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

    internal static XpeCommonNative.XpeImageBuffer MakeBuffer(
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

    internal static XpeCommonNative.XpeImageMetadata CreateMetadata() =>
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

    internal static TDelegate GetDelegate<TDelegate>(IntPtr handle, string exportName)
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
