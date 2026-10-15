// #225 row 9 (GUI-C-196 M2): the enhance_basic stage against the REAL xpe_enhance_basic.dll, through the gui's own P/Invoke binding.
using System.Runtime.InteropServices;
using ImageProcTest.Services;
using ImageProcTest.Services.Native;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// What the fake cannot show: that the declarations (struct layouts, calling convention, in-place float32 buffer) match the module, and that two runs on the
/// same input give bit-identical output. Skipped when the DLLs are not staged (the C# integration job stages them; so does XPE_NATIVE_DIR on a developer
/// machine). The DLL directory needs xpe_common.dll and xpe_enhance_basic.dll.
///
/// <para>This test project is a separate assembly from the app, so the app's own resolver (GuiNativeLibraryResolver) is not what loads the DLLs here: they are
/// preloaded by full path from the staged directory. That the app's resolver maps xpe_enhance_basic.dll is a source-reading check and the Native E2E.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class EnhanceBasicNativeTests(ITestOutputHelper output)
{
    private static readonly string? NativeDirectory = FindDirectory();
    private static bool _resolverInstalled;
    private static string? _resolverProblem;

    private static string? FindDirectory()
    {
        var candidates = new List<string>();
        var injected = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrWhiteSpace(injected))
        {
            candidates.Add(injected);
        }

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-common", "bin"));
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-post", "bin"));
            dir = dir.Parent;
        }

        return candidates.FirstOrDefault(c => File.Exists(Path.Combine(c, "xpe_enhance_basic.dll")) && File.Exists(Path.Combine(c, "xpe_common.dll")));
    }

    private static bool EnsureResolver()
    {
        if (_resolverInstalled)
        {
            return true;
        }

        try
        {
            // No DllImport resolver is installed here: the linked interop sources compile into THIS assembly, and a resolver on it would replace the default
            // probing every other native test in the assembly relies on (the first version did, and 34 of them went red). Instead both DLLs are loaded by full
            // path, so a later DllImport of the same file name is answered by the already-loaded module.
            ImageProcTest.IntegrationTests.Fixtures.SharedCommonModule.Load(NativeDirectory!);   // GUI-C-199: never a second copy of xpe_common.dll
            NativeLibrary.Load(Path.Combine(NativeDirectory!, "xpe_enhance_basic.dll"));
            _resolverInstalled = true;
        }
        catch (Exception ex) when (ex is DllNotFoundException or BadImageFormatException)
        {
            _resolverProblem = "the staged DLLs could not be loaded: " + ex.Message;
        }

        return _resolverInstalled;
    }

    /// <summary>A reproducible test image with structure (a ramp, a bright disc, seeded noise) so every step has something to do.</summary>
    private static ushort[] StructuredImage(int width, int height)
    {
        var random = new Random(12345);
        var pixels = new ushort[width * height];
        for (var y = 0; y < height; y++)
        {
            for (var x = 0; x < width; x++)
            {
                var ramp = 2000 + (x * 30000.0 / width);
                var dx = x - (width * 0.6);
                var dy = y - (height * 0.4);
                var disc = (dx * dx) + (dy * dy) < (width * width / 25.0) ? 12000 : 0;
                var noise = random.NextDouble() * 600;
                pixels[(y * width) + x] = (ushort)Math.Clamp(ramp + disc + noise, 0, 65535);
            }
        }

        return pixels;
    }

    private void RequireNative()
    {
        Skip.If(NativeDirectory is null, "xpe_enhance_basic.dll and xpe_common.dll are not staged (XPE_NATIVE_DIR, build/ci-common/bin).");
        Skip.If(!EnsureResolver(), _resolverProblem ?? "the native resolver could not be installed");
        output.WriteLine($"native directory: {NativeDirectory}");
        output.WriteLine($"xpe_enhance_basic version: {XpeEnhanceBasicNative.GetVersion()}");
    }

    [SkippableFact]
    public void TheStage_RunsOnTheRealModule_AndTwoRunsAreBitIdentical()
    {
        RequireNative();
        var input = StructuredImage(256, 256);

        var first = EnhanceBasicStage.Run(input, 256, 256, new NativeEnhanceBasicBackend());
        var second = EnhanceBasicStage.Run(input, 256, 256, new NativeEnhanceBasicBackend());
        output.WriteLine(first.Summary);

        Assert.True(first.Ran, first.Summary);
        Assert.True(second.Ran, second.Summary);
        Assert.Equal(0, first.NaNInfCount);
        Assert.Equal(input.Length, first.Pixels!.Length);
        Assert.False(BaselineDeterminism.Compare(input, first.Pixels).Identical, "the stage did not change the image at all");
        var diff = BaselineDeterminism.Compare(first.Pixels, second.Pixels!);
        Assert.True(diff.Identical, $"two runs differ: first at {diff.FirstIndex}, {diff.DifferentCount} pixels, max {diff.MaxAbsDifference}");
    }

    [SkippableFact]
    public void AnImageSmallerThanTwiceTheTileGrid_IsRefusedByTheModule_AndTheStageSaysWhichStep()
    {
        RequireNative();
        var input = StructuredImage(10, 10);   // CLAHE needs width and height >= 2 x tile (16); log and noise accept it

        var result = EnhanceBasicStage.Run(input, 10, 10, new NativeEnhanceBasicBackend());
        output.WriteLine(result.Summary);

        Assert.False(result.Ran);
        Assert.Null(result.Pixels);
        Assert.Contains("contrast", result.Summary, StringComparison.Ordinal);
        Assert.Contains("refused by the module", result.Summary, StringComparison.Ordinal);
    }

    /// <summary>
    /// Measurement, not a gate (the 3000 ms budget of product.md covers the whole baseline and has not been calibrated on a CI runner): the stage on a 3072 x 3072
    /// frame, once, with each step's time in the log line. Asserts only that it ran and that two runs agree.
    /// </summary>
    [SkippableFact]
    public void TheStage_AtFullSize_IsMeasured_AndStillDeterministic()
    {
        RequireNative();
        var input = StructuredImage(3072, 3072);

        var watch = System.Diagnostics.Stopwatch.StartNew();
        var first = EnhanceBasicStage.Run(input, 3072, 3072, new NativeEnhanceBasicBackend());
        var firstMs = watch.Elapsed.TotalMilliseconds;
        watch.Restart();
        var second = EnhanceBasicStage.Run(input, 3072, 3072, new NativeEnhanceBasicBackend());
        var secondMs = watch.Elapsed.TotalMilliseconds;
        output.WriteLine($"MEASURED 3072x3072 enhance_basic: run 1 {firstMs:0} ms, run 2 {secondMs:0} ms");
        output.WriteLine($"MEASURED run 1 steps: {first.Summary}");

        Assert.True(first.Ran, first.Summary);
        Assert.True(second.Ran, second.Summary);
        Assert.True(BaselineDeterminism.Compare(first.Pixels!, second.Pixels!).Identical);
    }

    [SkippableFact]
    public void AFlatImage_IsAccepted_NotRefused()
    {
        RequireNative();
        var input = Enumerable.Repeat((ushort)30000, 64 * 64).ToArray();

        var result = EnhanceBasicStage.Run(input, 64, 64, new NativeEnhanceBasicBackend());
        output.WriteLine(result.Summary);

        Assert.True(result.Ran, result.Summary);
        Assert.Equal(0, result.NaNInfCount);
    }
}
