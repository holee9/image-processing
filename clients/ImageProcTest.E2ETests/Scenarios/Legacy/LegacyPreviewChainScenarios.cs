// GUI-C-214 (#249): the legacy diagnostic app's native preprocess preview, run end to end against the real xpe_preprocess.dll.
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// GUI-C-213 fixed the call shape of the legacy preview's three correction delegates and could not run the chain (no fixtures here). Running it, this card found what the shape was hiding: the
/// service wrote its calibration files in a format of its own ("XPEC", CRC-32) that the module cannot read and never loaded anything into the module, so every correction answered
/// CALIB_NOT_LOADED — the chain had been dead since the module's calibration state model (#117). It now has the module generate the offset and gain files and loads all three.
///
/// <para>These scenarios build a small synthetic case on disk (target frame, dark, flat, defect map as RAW files — what the app's fixture folders hold), run
/// <c>NativePreprocessPreviewService.Run</c> with each stage combination, and compare every pixel with values computed HERE from the inputs by plain arithmetic (not by the module):
/// offset = raw − dark, gain = (raw − dark) ÷ ((flat − dark) ÷ mean(flat − dark)), defect = the mean of the unmarked 4-neighbours at the marked pixel and the previous stage's value everywhere else.
/// Passing each stage's output to the next is what the three-stage expectation checks: a value that did not travel would not match.</para>
/// </summary>
public sealed class LegacyPreviewChainScenarios(ITestOutputHelper output)
{
    private const int N = 32;
    private static readonly (int X, int Y) DefectPixel = (5, 5);
    private static readonly int DefectIndex = DefectPixel.Y * N + DefectPixel.X;

    private sealed record Case(ushort[] Raw, ushort[] Dark, ushort[] Flat, string Dir, string DllPath);

    private static Case Build()
    {
        var dll = Path.Combine(Environment.GetEnvironmentVariable("XPE_NATIVE_DIR") ?? string.Empty, "xpe_preprocess.dll");
        Skip.If(!File.Exists(dll), "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");

        var dir = Path.Combine(Path.GetTempPath(), $"xpe_preview_{Guid.NewGuid():N}");
        var cal = Path.Combine(dir, "calibration");
        Directory.CreateDirectory(cal);
        var raw = new ushort[N * N];
        var dark = new ushort[N * N];
        var flat = new ushort[N * N];
        var bpm = new ushort[N * N];
        for (var i = 0; i < raw.Length; i++)
        {
            raw[i] = (ushort)(1000 + i % 97);
            dark[i] = (ushort)(100 + i % 7);
            flat[i] = (ushort)(2000 + i % 13 * 10);
        }

        raw[DefectIndex] = 60000;   // hot: a correction that did nothing leaves it 59,000 away from its neighbours
        bpm[DefectIndex] = 1;
        Write(Path.Combine(dir, "target.raw"), raw);
        Write(Path.Combine(cal, "cdark.raw"), dark);
        Write(Path.Combine(cal, "cbr_flat.raw"), flat);
        Write(Path.Combine(cal, "bpm.raw"), bpm);
        return new Case(raw, dark, flat, dir, dll);
    }

    private static void Write(string path, ushort[] values)
    {
        var bytes = new byte[values.Length * sizeof(ushort)];
        Buffer.BlockCopy(values, 0, bytes, 0, bytes.Length);
        File.WriteAllBytes(path, bytes);
    }

    private static NativePreprocessPreviewResult Run(Case c, PreprocessStageMode offset, PreprocessStageMode gain, PreprocessStageMode defect, out RawPreviewResult preview)
    {
        preview = RawPreviewService.LoadUInt16Preview(Path.Combine(c.Dir, "target.raw"), N);
        var cal = Path.Combine(c.Dir, "calibration");
        var files = new List<CalibrationFileDescriptor>
        {
            new(new RawFileDescriptor(Path.Combine(cal, "cdark.raw")), CalibrationRole.Offset),
            new(new RawFileDescriptor(Path.Combine(cal, "cbr_flat.raw")), CalibrationRole.Gain),
            new(new RawFileDescriptor(Path.Combine(cal, "bpm.raw")), CalibrationRole.Defect),
        };
        var fixture = new FixtureCaseInfo("synthetic", c.Dir, [new RawFileDescriptor(Path.Combine(c.Dir, "target.raw"))], files, cal);
        return NativePreprocessPreviewService.Run(preview, new PreprocessStageSelection(offset, gain, defect), fixture, c.DllPath);
    }

    // ---- expected values: plain arithmetic on the inputs, no module call

    private static float[] ExpectedOffset(Case c) => Enumerable.Range(0, N * N).Select(i => (float)Math.Max(c.Raw[i] - c.Dark[i], 0)).ToArray();

    private static float[] ExpectedGain(Case c)
    {
        var offset = ExpectedOffset(c);
        var flatMinusDark = Enumerable.Range(0, N * N).Select(i => (double)(c.Flat[i] - c.Dark[i])).ToArray();
        var mean = flatMinusDark.Average();
        return Enumerable.Range(0, N * N).Select(i => (float)(offset[i] / (flatMinusDark[i] / mean))).ToArray();
    }

    private static float[] ExpectedDefect(Case c)
    {
        var gain = ExpectedGain(c);
        var result = (float[])gain.Clone();
        var neighbours = new[] { DefectIndex - 1, DefectIndex + 1, DefectIndex - N, DefectIndex + N };   // 4-connected, none of them marked
        result[DefectIndex] = neighbours.Select(i => gain[i]).Average();
        return result;
    }

    private static void AssertClose(IReadOnlyList<float> actual, float[] expected, string what)
    {
        Assert.Equal(expected.Length, actual.Count);
        var worst = 0.0;
        var worstIndex = -1;
        for (var i = 0; i < expected.Length; i++)
        {
            var error = Math.Abs(actual[i] - expected[i]) / Math.Max(1.0, Math.Abs(expected[i]));
            if (error > worst) { worst = error; worstIndex = i; }
        }

        Assert.True(worst <= 1e-3, $"{what}: pixel {worstIndex} is {(worstIndex >= 0 ? actual[worstIndex] : 0)}, expected {(worstIndex >= 0 ? expected[worstIndex] : 0)} (relative error {worst:0.####})");
    }

    // ---- the scenarios

    [SkippableFact]
    public void P01_OffsetOnly_SubtractsTheDarkFrame_PixelByPixel()
    {
        var c = Build();
        try
        {
            var result = Run(c, PreprocessStageMode.On, PreprocessStageMode.Off, PreprocessStageMode.Off, out _);
            Assert.Equal(["OK"], result.Stages.Where(s => s.Executed).Select(s => s.ErrorCode));
            AssertClose(result.OutputPixels, ExpectedOffset(c), "offset");
        }
        finally { Cleanup(c); }
    }

    [SkippableFact]
    public void P02_OffsetThenGain_PassesTheOffsetOutputToTheGainStage()
    {
        var c = Build();
        try
        {
            var result = Run(c, PreprocessStageMode.On, PreprocessStageMode.On, PreprocessStageMode.Off, out _);
            Assert.Equal(["offset", "gain"], result.Stages.Where(s => s.Executed).Select(s => s.Stage));
            AssertClose(result.OutputPixels, ExpectedGain(c), "offset→gain");
            Assert.True(result.OutputPixels[DefectIndex] > 50000f, "the hot pixel must still be hot before the defect stage (proves the defect stage is not what changed it)");
        }
        finally { Cleanup(c); }
    }

    [SkippableFact]
    public void P03_AllThreeStages_ChainToTheExpectedImage_AndOnlyTheMarkedPixelDiffersFromTheGainOutput()
    {
        var c = Build();
        try
        {
            var result = Run(c, PreprocessStageMode.On, PreprocessStageMode.On, PreprocessStageMode.On, out _);
            Assert.Equal(["offset", "gain", "defect"], result.Stages.Where(s => s.Executed).Select(s => s.Stage));
            Assert.All(result.Stages, s => Assert.Equal("OK", s.ErrorCode));
            AssertClose(result.OutputPixels, ExpectedDefect(c), "offset→gain→defect");

            var gain = ExpectedGain(c);
            var changed = Enumerable.Range(0, N * N).Where(i => Math.Abs(result.OutputPixels[i] - gain[i]) > 1e-3 * Math.Max(1.0, Math.Abs(gain[i]))).ToArray();
            Assert.Equal([DefectIndex], changed);
        }
        finally { Cleanup(c); }
    }

    [SkippableFact]
    public void P04_TheInputIsNotModified_NeitherTheFileNorThePreviewPixels()
    {
        var c = Build();
        try
        {
            var path = Path.Combine(c.Dir, "target.raw");
            var before = RawPreviewService.ComputeFileSha256(path);
            var result = Run(c, PreprocessStageMode.On, PreprocessStageMode.On, PreprocessStageMode.On, out var preview);
            Assert.Equal(before, RawPreviewService.ComputeFileSha256(path));
            Assert.Equal(c.Raw, preview.SampledPixels);   // the pixels the service was handed are still the file's pixels
            Assert.NotEmpty(result.OutputPixels);
            output.WriteLine($"input sha256 {before}");
        }
        finally { Cleanup(c); }
    }

    [SkippableFact]
    public void P05_TheCalibrationIsLoadedIntoTheModule_NotJustWrittenToDisk()
    {
        var c = Build();
        try
        {
            var result = Run(c, PreprocessStageMode.On, PreprocessStageMode.On, PreprocessStageMode.On, out _);
            Assert.Equal(["offset", "gain", "defect"], result.CalibrationLoads.Select(l => l.Stage).OrderBy(s => s == "offset" ? 0 : s == "gain" ? 1 : 2));
            Assert.All(result.CalibrationLoads, l => { Assert.True(l.Loaded, $"{l.Stage}: {l.Status}"); Assert.Equal("OK", l.Status); });
            foreach (var l in result.CalibrationLoads)
            {
                var header = File.ReadAllBytes(l.XCalPath!).AsSpan(0, 4).ToArray();
                Assert.Equal("XCAL"u8.ToArray(), header);   // the module's file format, not the old "XPEC"
            }
        }
        finally { Cleanup(c); }
    }

    private static void Cleanup(Case c)
    {
        try { Directory.Delete(c.Dir, recursive: true); } catch (IOException) { /* temp folder */ }
        try
        {
            var artifacts = Path.Combine(AppContext.BaseDirectory, "fixture-preview-artifacts");
            foreach (var d in Directory.GetDirectories(artifacts, "synthetic-*")) Directory.Delete(d, recursive: true);
        }
        catch (DirectoryNotFoundException) { }
        catch (IOException) { /* artifacts folder under the test output */ }
    }
}
