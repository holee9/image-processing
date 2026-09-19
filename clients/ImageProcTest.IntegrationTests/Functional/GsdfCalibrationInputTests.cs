// #155 (GUI-C-131): what the gui hands xpe_gsdf_calibrate, and whether the curve reaches the LUT.
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// <c>REQ-DISP-029</c> gave the luminance array a meaning it did not have: element <c>i</c> is the
/// luminance measured at driving level <c>DDL_i = i/(count-1) × 65535</c>, so the SPACING is the
/// display's characteristic curve. The gui was passing a log grid (0.05, 1, 10, 100, 400) — correct
/// while the header said only the minimum and maximum were read, and a fabricated curve under the new
/// contract.
///
/// <para><b>Two endpoints is the honest input.</b> No panel has been measured here, so an interior
/// value would be invented; <c>count == 2</c> states the assumption the old call already made — the
/// luminance at driving level 0 and at full scale, linear between them.</para>
///
/// <para><b>The third case is the load-bearing one.</b> It asks whether the values reach the LUT at
/// all: change the maximum and the LUT must change. If it does not, the curve is still not connected
/// and the fix #155 made has not arrived on the gui's path — which is a finding, not a pass.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class GsdfCalibrationInputTests(Xunit.Abstractions.ITestOutputHelper output)
{
    private static readonly string? DllPath = FindDisplayDll();

    [StructLayout(LayoutKind.Sequential)]
    private struct PresentationLutParams
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1024)]
        public ushort[] LutData;

        public int GsdfEnabled;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int GsdfCalibrateDelegate(
        [In] float[] luminanceValues,
        uint count,
        ref PresentationLutParams outParams);

    /// <summary>The call the gui makes, with the values the gui passes.</summary>
    [SkippableFact]
    public void TheGuiCall_PassesTwoEndpoints_AndSucceeds()
    {
        Skip.If(DllPath is null, "xpe_display.dll is not staged.");

        var values = GuiLuminanceValues();
        output.WriteLine($"gui passes [{string.Join(", ", values)}] (count {values.Length})");

        Assert.Equal(2, values.Length);
        Assert.Equal(0.05f, values[0]);
        Assert.Equal(400.0f, values[1]);

        var (code, parameters) = Calibrate(values);
        output.WriteLine($"xpe_gsdf_calibrate -> {code}, gsdfEnabled={parameters.GsdfEnabled}");

        Assert.Equal(0, code);
        Assert.Equal(1, parameters.GsdfEnabled);
        Assert.Contains(parameters.LutData, v => v != 0);
    }

    /// <summary>
    /// The control: a different maximum produces a different LUT. This is what says the array reaches
    /// the curve rather than being read for its presence alone.
    /// </summary>
    [SkippableFact]
    public void ChangingTheMaximum_ChangesTheLut()
    {
        Skip.If(DllPath is null, "xpe_display.dll is not staged.");

        var (code400, at400) = Calibrate([0.05f, 400.0f]);
        var (code200, at200) = Calibrate([0.05f, 200.0f]);

        Assert.Equal(0, code400);
        Assert.Equal(0, code200);

        var differences = at400.LutData.Zip(at200.LutData).Count(pair => pair.First != pair.Second);
        output.WriteLine($"LUT entries differing between max=400 and max=200: {differences} of {at400.LutData.Length}");
        output.WriteLine($"  at index 512: {at400.LutData[512]} vs {at200.LutData[512]}");

        Assert.True(differences > 0,
            "Changing the maximum luminance left the LUT byte-identical, so the values do not reach " +
            "the curve on this path (#155).");
    }

    /// <summary>
    /// The source guard: the gui passes exactly these two values and no invented interior ones.
    ///
    /// <para>Read from the source because the runtime cases above would pass just as well on a call
    /// that passed five values — <c>count</c> is the thing under test, and only the call site says
    /// what it is.</para>
    /// </summary>
    [Fact]
    public void TheGuiCallSite_PassesNoInventedInteriorValues()
    {
        var source = File.ReadAllText(BackendSource());
        var match = Regex.Match(source, @"var luminanceValues = new\[\] \{([^}]*)\}");

        Assert.True(match.Success, "RealXpeBackend no longer builds a luminanceValues array.");
        var values = match.Groups[1].Value
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .ToArray();

        Assert.Equal(2, values.Length);
        Assert.Equal("0.05f", values[0]);
        Assert.Equal("400.0f", values[1]);
    }

    private static float[] GuiLuminanceValues() => [0.05f, 400.0f];

    private static (int Code, PresentationLutParams Parameters) Calibrate(float[] values)
    {
        var handle = NativeLibrary.Load(DllPath!);
        try
        {
            Assert.True(NativeLibrary.TryGetExport(handle, "xpe_gsdf_calibrate", out var address),
                "xpe_gsdf_calibrate is not exported.");
            var calibrate = Marshal.GetDelegateForFunctionPointer<GsdfCalibrateDelegate>(address);

            var parameters = new PresentationLutParams { LutData = new ushort[1024], GsdfEnabled = 1 };
            for (var i = 0; i < 1024; i++)
            {
                parameters.LutData[i] = (ushort)Math.Clamp((int)MathF.Round(i / 1023.0f * ushort.MaxValue), 0, ushort.MaxValue);
            }

            var code = calibrate(values, (uint)values.Length, ref parameters);
            return (code, parameters);
        }
        finally
        {
            NativeLibrary.Free(handle);
        }
    }

    private static string BackendSource()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "gui")))
        {
            dir = dir.Parent;
        }

        Assert.True(dir is not null, "The repository root was not found.");
        var path = Path.Combine(dir!.FullName, "gui", "ImageProcTest", "Services", "RealXpeBackend.cs");
        Assert.True(File.Exists(path), $"{path} does not exist.");
        return path;
    }

    private static string? FindDisplayDll()
    {
        var candidates = new List<string>();
        var nativeDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrWhiteSpace(nativeDir))
        {
            candidates.Add(Path.Combine(nativeDir, "xpe_display.dll"));
        }

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-common", "bin", "xpe_display.dll"));
            candidates.Add(Path.Combine(dir.FullName, "build", "e2e-native-dlls", "xpe_display.dll"));
            dir = dir.Parent;
        }

        return candidates.Distinct().FirstOrDefault(File.Exists);
    }
}
