// #198 (GUI-C-128): the gui runs the module's Stage 3, and adding it changes no pixels without a LUT.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// The gui's preprocess path used to run offset → gain → defect and skip the module's Stage 3
/// (nonlinearity) entirely, so what the gui drew was not what the module's pipeline produces. Read from
/// the module rather than taken on report: <c>pipeline.cpp</c> runs Stage 2 offset (:142), Stage 3
/// nonlinearity (:158) and Stage 4 gain (:184) in that order.
///
/// <para><b>What is asserted here is that the stage RUNS, not that an alert appears.</b> The alert
/// <c>#196</c> pushes is a side effect of the stage finally being called; asserting on it would measure
/// the side effect and let the wiring be removed as long as something logged.</para>
///
/// <para><b>The load-bearing case is the byte-identity one.</b> With no LUT loaded the stage returns
/// <c>XPE_OK</c> and leaves the frame untouched (<c>nonlinearity_correct.cpp:95</c>), so inserting it
/// into the chain must change nothing. If the bytes move, the stage is doing something other than
/// passing through — that is a finding, not a pass.</para>
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class NonlinearityStageWiringTests
{
    private const int Width = 16;
    private const int Height = 16;
    private const int PixelCount = Width * Height;

    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();
    private static readonly string SkipReason = DllPath is null
        ? "Skipped: xpe_preprocess.dll not staged"
        : string.Empty;

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private delegate XpeCommonNative.XpeErrorCode NonlinearityDelegate(
        ref XpeCommonNative.XpeImageBuffer image,
        string? configJson);

    /// <summary>
    /// The exported entry point exists and, with no LUT and no detector profile, passes the frame
    /// through byte for byte.
    /// </summary>
    [SkippableFact]
    public void WithoutALut_TheStageRunsAndLeavesTheFrameByteIdentical()
    {
        Skip.If(DllPath is null, SkipReason);

        var handle = NativeLibrary.Load(DllPath!);
        try
        {
            var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");

            // shutdown() first: another test in this process may have loaded a calibration.
            shutdown();
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

            try
            {
                var correct = GetDelegate<NonlinearityDelegate>(handle, "xpe_nonlinearity_correct");

                var pixels = Synthetic();
                var before = (ushort[])pixels.Clone();

                var pinned = GCHandle.Alloc(pixels, GCHandleType.Pinned);
                try
                {
                    var image = new XpeCommonNative.XpeImageBuffer
                    {
                        Width = Width,
                        Height = Height,
                        BitsAllocated = 16,
                        BitsStored = 16,
                        Format = XpeCommonNative.XpePixelFormat.UInt16,
                        Data = pinned.AddrOfPinnedObject(),
                        DataSize = (nuint)(PixelCount * sizeof(ushort)),
                    };

                    // The gui passes null for the config: it has no detector profile, and the module
                    // reads that as "panel.linear not declared".
                    var code = correct(ref image, null);
                    Assert.Equal(XpeCommonNative.XpeErrorCode.OK, code);
                }
                finally
                {
                    pinned.Free();
                }

                Assert.Equal(before, pixels);
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
    /// The control for the case above: a panel declared non-linear with no LUT is REFUSED, so
    /// "XPE_OK and unchanged" is a real answer about this input rather than a function that returns OK
    /// for everything.
    /// </summary>
    [SkippableFact]
    public void ANonLinearPanelWithoutALut_IsRefused()
    {
        Skip.If(DllPath is null, SkipReason);

        var handle = NativeLibrary.Load(DllPath!);
        try
        {
            var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");

            shutdown();
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

            try
            {
                var correct = GetDelegate<NonlinearityDelegate>(handle, "xpe_nonlinearity_correct");
                var pixels = Synthetic();
                var pinned = GCHandle.Alloc(pixels, GCHandleType.Pinned);
                try
                {
                    var image = new XpeCommonNative.XpeImageBuffer
                    {
                        Width = Width,
                        Height = Height,
                        BitsAllocated = 16,
                        BitsStored = 16,
                        Format = XpeCommonNative.XpePixelFormat.UInt16,
                        Data = pinned.AddrOfPinnedObject(),
                        DataSize = (nuint)(PixelCount * sizeof(ushort)),
                    };

                    var code = correct(ref image, "{\"panel.linear\":\"false\"}");
                    Assert.NotEqual(XpeCommonNative.XpeErrorCode.OK, code);
                }
                finally
                {
                    pinned.Free();
                }
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
    /// The gui calls the stage, between offset and gain, in the module's order.
    ///
    /// <para>A source guard rather than a scenario, for the reason <c>StartupRejectionSurvivesTests</c>
    /// gives: the outcome (identical pixels) cannot tell "the stage ran and passed through" from "the
    /// stage was never called". Both produce the same frame today, which is exactly what the byte
    /// identity case above measures — so the ordering has to be read from the source.</para>
    /// </summary>
    [Fact]
    public void TheGuiRunnerCallsTheStage_BetweenOffsetAndGain()
    {
        var source = File.ReadAllText(RunnerSource());

        var offset = source.IndexOf("xpe_offset_correct(ref input", StringComparison.Ordinal);
        var nonlinearity = source.IndexOf("xpe_nonlinearity_correct(ref offsetOut", StringComparison.Ordinal);
        var gain = source.IndexOf("xpe_gain_correct(ref offsetOut", StringComparison.Ordinal);

        Assert.True(offset >= 0, "GuiPreprocessRunner no longer calls xpe_offset_correct.");
        Assert.True(nonlinearity >= 0,
            "GuiPreprocessRunner does not call xpe_nonlinearity_correct, so the gui skips the module's " +
            "Stage 3 and what it draws is not what the pipeline produces (#198).");
        Assert.True(gain >= 0, "GuiPreprocessRunner no longer calls xpe_gain_correct.");

        Assert.True(offset < nonlinearity && nonlinearity < gain,
            "The nonlinearity stage is not between offset and gain. The module's pipeline runs Stage 2 " +
            "offset, Stage 3 nonlinearity, Stage 4 gain in that order (pipeline.cpp:142/158/184).");
    }

    private static string RunnerSource()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "gui")))
        {
            dir = dir.Parent;
        }

        Assert.True(dir is not null, "The repository root was not found from the test output directory.");
        var path = Path.Combine(dir!.FullName, "gui", "ImageProcTest", "Services", "Native", "GuiPreprocessRunner.cs");
        Assert.True(File.Exists(path), $"{path} does not exist.");
        return path;
    }

    private static ushort[] Synthetic()
    {
        var pixels = new ushort[PixelCount];
        for (var i = 0; i < PixelCount; i++)
        {
            pixels[i] = (ushort)(1000 + (i * 37 % 5000));
        }

        return pixels;
    }

    private static T GetDelegate<T>(IntPtr handle, string name) where T : Delegate
    {
        Assert.True(NativeLibrary.TryGetExport(handle, name, out var address), $"{name} is not exported.");
        return Marshal.GetDelegateForFunctionPointer<T>(address);
    }
}
