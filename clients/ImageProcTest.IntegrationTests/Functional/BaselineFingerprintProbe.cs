// GUI-C-234: a MEASUREMENT with one fixed expectation (GUI-C-234c). It runs the Deterministic Baseline's chain (preprocess -> enhance_basic -> the fixed display step) on real data with the real modules and prints the SHA-256 of
// every stage's output, so a change to a Baseline parameter (the log normFactor, GUI-C-234) can be recorded as before/after. It asserts that the chain ran and, for the recorded input, that every stage's hash is the one recorded for the CI DLL set of main 386182e1 (a change of any hash is a change of the Baseline's fingerprint and has to be
// recorded on purpose, not found by accident). Needs XPE_NATIVE_DIR (staged modules incl. xpe_display
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
    // GUI-C-234c (Codex #181): the fingerprint of the Baseline chain with normFactor 1000, measured with ONE DLL set (the push CI of main 386182e1, run 38216547608) on the recorded input; evidence:
    // .moai/reports/lane-gui/GUI-C-234b/baseline_fingerprint_now_onedll_386182e1.txt. Different modules (another compiler, another build) can give different hashes: the expectation is that of this set, and it is only
    // asserted when the input is the recorded one. The values are checked into this test on purpose - the independent record of what the fingerprint is, so a regression cannot make itself the new expectation.
    private const string RecordedInputSha = "c823233f2196a217f4512bfec47e62a17f003cc894ef7947e1f40dd0b59a3d70";
    private const string RecordedPreprocessSha = "2ab3c65cea9c731ad97af3dd478c30564a3fe338ccfebc2d36df2fa2b3312c75";
    private const string RecordedEnhanceSha = "c5b673dad95fde857e06fb72a498336ecb97c730f136f56ea48a24d3085c215c";
    private const string RecordedDisplaySha = "f360938d8cc5509cac09655f147f9447217b9a0f910ad3c80affe194a886e616";

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

        if (Sha(raw) == RecordedInputSha)
        {
            Assert.Equal(RecordedPreprocessSha, Sha(pre.Pixels!));
            Assert.Equal(RecordedEnhanceSha, Sha(enhanced.Pixels!));
            Assert.Equal(RecordedDisplaySha, Sha(shown.Pixels));
        }
    }
}
