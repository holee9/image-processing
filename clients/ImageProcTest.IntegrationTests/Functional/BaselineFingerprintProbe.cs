// GUI-C-234: a MEASUREMENT, not a regression test. It runs the Deterministic Baseline's chain (preprocess -> enhance_basic -> the fixed display step) on real data with the real modules and prints the SHA-256 of
// every stage's output, so a change to a Baseline parameter (the log normFactor, GUI-C-234) can be recorded as before/after. It asserts only that the chain ran. Needs XPE_NATIVE_DIR (staged modules incl. xpe_display
// and xpe_enhance_basic), XPE_C231_CALIB_DIR (a real offset/gain/defect folder) and XPE_C231_RAW (+ XPE_C231_RAW_SIZE, default 3072x3072); skipped without them.
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using ImageProcTest.Models;
using ImageProcTest.Services;
using ImageProcTest.Services.Native;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

// GUI-C-234b: it initialises and runs the preprocess module (through GuiPreprocessRunner.Run), so it belongs to the module's collection and to the leak sweep like every other file that does.
[Collection(ImageProcTest.IntegrationTests.Fixtures.PreprocessModuleCollection.Name)]
public sealed class BaselineFingerprintProbe(ITestOutputHelper output)
{
    private static string Sha(ushort[] pixels) => Convert.ToHexString(SHA256.HashData(MemoryMarshal.AsBytes(pixels.AsSpan()))).ToLowerInvariant();

    [SkippableFact]
    public void PrintTheStageHashesOfTheBaselineChainOnRealData()
    {
        var native = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        var calib = Environment.GetEnvironmentVariable("XPE_C231_CALIB_DIR");
        var rawPath = Environment.GetEnvironmentVariable("XPE_C231_RAW");
        Skip.If(string.IsNullOrWhiteSpace(native) || string.IsNullOrWhiteSpace(calib) || string.IsNullOrWhiteSpace(rawPath) || !File.Exists(rawPath), "XPE_NATIVE_DIR, XPE_C231_CALIB_DIR and XPE_C231_RAW are needed.");
        foreach (var dll in new[] { "xpe_preprocess.dll", "xpe_enhance_basic.dll", "xpe_display.dll" })
        {
            Skip.IfNot(File.Exists(Path.Combine(native!, dll)), dll + " is not in XPE_NATIVE_DIR");
        }

        ImageProcTest.IntegrationTests.Fixtures.SharedCommonModule.Load(native!);
        foreach (var dll in new[] { "xpe_preprocess.dll", "xpe_enhance_basic.dll", "xpe_display.dll" })
        {
            NativeLibrary.Load(Path.Combine(native!, dll));
        }

        var size = (Environment.GetEnvironmentVariable("XPE_C231_RAW_SIZE") ?? "3072x3072").Split('x');
        int width = int.Parse(size[0]), height = int.Parse(size[1]);
        var bytes = File.ReadAllBytes(rawPath!);
        Assert.Equal(width * height * 2, bytes.Length);
        var raw = new ushort[width * height];
        Buffer.BlockCopy(bytes, 0, raw, 0, bytes.Length);

        var defaults = new AppSettings();
        output.WriteLine($"LogNormFactor = {BaselineParameters.LogNormFactor:R}");
        output.WriteLine($"input sha256 = {Sha(raw)}");
        var pre = GuiPreprocessRunner.Run(raw, width, height, calib!, calib!, calib!, defaults.SelectedBodyPart, defaults.ExposureKvp, defaults.PixelPitchMm, measureExposureIndex: true);
        Assert.True(pre.Ran, pre.Summary);
        output.WriteLine($"preprocess sha256 = {Sha(pre.Pixels!)}");
        output.WriteLine("preprocess summary = " + pre.Summary);

        var enhanced = EnhanceBasicStage.Run(pre.Pixels!, width, height, new NativeEnhanceBasicBackend());
        Assert.True(enhanced.Ran, enhanced.Summary);
        output.WriteLine($"enhance sha256 = {Sha(enhanced.Pixels!)}");
        output.WriteLine("enhance summary = " + enhanced.Summary);

        var shown = BaselineDisplayStage.Run(enhanced.Pixels!, width, height, new NativeBaselineDisplayBackend());
        output.WriteLine($"display sha256 = {Sha(shown.Pixels)} (nonfinite {shown.NonFiniteCount})");
        output.WriteLine($"display min/max = {shown.Pixels.Min()}/{shown.Pixels.Max()}");
    }
}
