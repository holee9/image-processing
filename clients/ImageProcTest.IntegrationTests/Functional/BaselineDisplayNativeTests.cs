// #225 row 9 (GUI-C-196 M7): the baseline's display step against the REAL xpe_display.dll through the gui's own binding.
using System.Runtime.InteropServices;
using ImageProcTest.Services;
using ImageProcTest.Services.Native;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Skipped when xpe_common.dll / xpe_display.dll are not staged together. Like the other native tests of this assembly it loads the DLLs by full path and installs no
/// DllImport resolver (the linked interop compiles into this assembly, and a resolver here would replace the default probing every other native test relies on).
/// </summary>
[Trait("Category", "Functional")]
public sealed class BaselineDisplayNativeTests(ITestOutputHelper output)
{
    private static readonly string? NativeDirectory = FindDirectory();
    private static bool _loaded;
    private static string? _loadProblem;

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
            dir = dir.Parent;
        }

        return candidates.FirstOrDefault(c => File.Exists(Path.Combine(c, "xpe_display.dll")) && File.Exists(Path.Combine(c, "xpe_common.dll")));
    }

    private static bool EnsureLoaded()
    {
        if (_loaded)
        {
            return true;
        }

        try
        {
            ImageProcTest.IntegrationTests.Fixtures.SharedCommonModule.Load(NativeDirectory!);   // GUI-C-199: never a second copy of xpe_common.dll
            NativeLibrary.Load(Path.Combine(NativeDirectory!, "xpe_display.dll"));
            _loaded = true;
        }
        catch (Exception ex) when (ex is DllNotFoundException or BadImageFormatException)
        {
            _loadProblem = "the staged DLLs could not be loaded: " + ex.Message;
        }

        return _loaded;
    }

    private static ushort[] Image(int width, int height)
    {
        var random = new Random(4242);
        var pixels = new ushort[width * height];
        for (var i = 0; i < pixels.Length; i++)
        {
            pixels[i] = (ushort)(1000 + (i * 7 % 50000) + random.Next(0, 3000));
        }

        return pixels;
    }

    /// <summary>
    /// The step moved out of RealXpeBackend in M7. This calls the module's three functions DIRECTLY, in the order the previous implementation did, with the same fixed
    /// values, and requires the stage to return exactly those pixels: the move changed no output.
    /// </summary>
    [SkippableFact]
    public void TheDisplayStep_ReturnsExactlyWhatTheDirectNativeCallSequenceReturns()
    {
        Skip.If(NativeDirectory is null, "xpe_common.dll and xpe_display.dll are not staged together (XPE_NATIVE_DIR, build/ci-common/bin).");
        Skip.If(!EnsureLoaded(), _loadProblem ?? "the DLLs could not be loaded");
        const int w = 128, h = 96;
        var input = Image(w, h);

        var viaStage = BaselineDisplayStage.Run(input, w, h, new NativeBaselineDisplayBackend()).Pixels;

        XpeImageBufferNative image = default;
        Assert.True(XpeCommonNative.xpe_alloc_image(w, h, XpePixelFormatNative.Float32, out image) >= 0);
        try
        {
            var floats = input.Select(v => (float)v).ToArray();
            Marshal.Copy(floats, 0, image.Data, floats.Length);

            var modality = new XpeModalityLutParamsNative { Mode = 0, RescaleSlope = BaselineParameters.ModalityRescaleSlope, RescaleIntercept = BaselineParameters.ModalityRescaleIntercept, LutData = IntPtr.Zero, LutLength = 0, LutFirstMapped = 0, LutBitsStored = 16 };
            Assert.True(XpeDisplayNative.xpe_apply_modality_lut(ref image, ref modality) >= 0);
            var voi = new XpeVoiLutParamsNative { Mode = 0, Center = BaselineParameters.VoiWindowCenter, Width = BaselineParameters.VoiWindowWidth, MinOut = 0.0f, MaxOut = 1.0f };
            Assert.True(XpeDisplayNative.xpe_apply_voi_lut(ref image, ref voi) >= 0);
            var presentation = XpePresentationLutParamsNative.CreateLinear(BaselineParameters.GsdfEnabled);
            Assert.True(XpeDisplayNative.xpe_apply_presentation_lut(ref image, ref presentation) >= 0);

            var signed = new short[w * h];
            Marshal.Copy(image.Data, signed, 0, signed.Length);
            var direct = new ushort[signed.Length];
            Buffer.BlockCopy(signed, 0, direct, 0, signed.Length * sizeof(ushort));

            var diff = BaselineDeterminism.Compare(direct, viaStage);
            Assert.True(diff.Identical, $"the stage differs from the direct call sequence at {diff.FirstIndex} ({diff.DifferentCount} pixels)");
        }
        finally
        {
            XpeCommonNative.xpe_free_image(ref image);
        }
    }

    /// <summary>
    /// GUI-C-233 (Codex #167 finding 2): the polarity regression the test above cannot catch (it compares the stage with a direct call to the SAME function, so both invert or neither does).
    /// The expectation here is derived apart from the stage: the module's AS_IS presentation (the older ascending mapping, asked for by name) on the same input, and the shipped display must be its
    /// complement, 65535 - value (within 1 count for the table rounding). A stage that asked for AS_IS, or a module whose default flipped back, makes the sum 65535 nowhere and fails.
    /// </summary>
    [SkippableFact]
    public void TheShippedDisplay_IsTheComplementOfTheAscendingMapping_BoneBrightAirDark()
    {
        Skip.If(NativeDirectory is null, "xpe_common.dll and xpe_display.dll are not staged together (XPE_NATIVE_DIR, build/ci-common/bin).");
        Skip.If(!EnsureLoaded(), _loadProblem ?? "the DLLs could not be loaded");
        const int w = 128, h = 96;
        var input = Image(w, h);

        var shipped = BaselineDisplayStage.Run(input, w, h, new NativeBaselineDisplayBackend()).Pixels;

        XpeImageBufferNative image = default;
        Assert.True(XpeCommonNative.xpe_alloc_image(w, h, XpePixelFormatNative.Float32, out image) >= 0);
        try
        {
            var floats = input.Select(v => (float)v).ToArray();
            Marshal.Copy(floats, 0, image.Data, floats.Length);

            var modality = new XpeModalityLutParamsNative { Mode = 0, RescaleSlope = BaselineParameters.ModalityRescaleSlope, RescaleIntercept = BaselineParameters.ModalityRescaleIntercept, LutData = IntPtr.Zero, LutLength = 0, LutFirstMapped = 0, LutBitsStored = 16 };
            Assert.True(XpeDisplayNative.xpe_apply_modality_lut(ref image, ref modality) >= 0);
            var voi = new XpeVoiLutParamsNative { Mode = 0, Center = BaselineParameters.VoiWindowCenter, Width = BaselineParameters.VoiWindowWidth, MinOut = 0.0f, MaxOut = 1.0f };
            Assert.True(XpeDisplayNative.xpe_apply_voi_lut(ref image, ref voi) >= 0);
            var presentation = XpePresentationLutParamsNative.CreateLinear(BaselineParameters.GsdfEnabled);
            Assert.True(XpeDisplayNative.xpe_apply_presentation_lut_ex(ref image, ref presentation, XpeDisplayNative.PresentationAsIs) >= 0);

            var signed = new short[w * h];
            Marshal.Copy(image.Data, signed, 0, signed.Length);
            var ascending = new ushort[signed.Length];
            Buffer.BlockCopy(signed, 0, ascending, 0, signed.Length * sizeof(ushort));

            var worst = 0;
            var anyDifferent = false;
            for (var i = 0; i < ascending.Length; i++)
            {
                worst = Math.Max(worst, Math.Abs(shipped[i] + ascending[i] - 65535));
                anyDifferent |= shipped[i] != ascending[i];
            }

            output.WriteLine($"worst |shipped + ascending - 65535| = {worst}; any pixel different from the ascending mapping: {anyDifferent}");
            Assert.True(anyDifferent, "the shipped display equals the older ascending mapping everywhere: the polarity was not inverted");
            Assert.True(worst <= 1, $"the shipped display is not the complement of the ascending mapping (worst deviation {worst})");
        }
        finally
        {
            XpeCommonNative.xpe_free_image(ref image);
        }
    }

    [SkippableFact]
    public void TheDisplayStep_OnTheRealModule_IsFinite_Deterministic_AndChangesTheImage()
    {
        Skip.If(NativeDirectory is null, "xpe_common.dll and xpe_display.dll are not staged together (XPE_NATIVE_DIR, build/ci-common/bin).");
        Skip.If(!EnsureLoaded(), _loadProblem ?? "the DLLs could not be loaded");
        var input = Image(256, 256);

        var first = BaselineDisplayStage.Run(input, 256, 256, new NativeBaselineDisplayBackend());
        var second = BaselineDisplayStage.Run(input, 256, 256, new NativeBaselineDisplayBackend());
        output.WriteLine($"native directory: {NativeDirectory}; non-finite {first.NonFiniteCount}/{second.NonFiniteCount}; first output pixel {first.Pixels[0]} for input {input[0]}");

        Assert.Equal(0, first.NonFiniteCount);
        Assert.Equal(input.Length, first.Pixels.Length);
        Assert.True(BaselineDeterminism.Compare(first.Pixels, second.Pixels).Identical, "two runs of the display step on the real module differ");
        Assert.False(BaselineDeterminism.Compare(input, first.Pixels).Identical, "the display step returned its input unchanged");
    }
}
