// #198 / #194 (GUI-C-129): the clamp alert the gain stage pushes, measured on the path the gui uses.
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.P1AReady;

/// <summary>
/// <c>gain_correct.cpp</c> clamps pixels that fall outside the gain polynomial's fitted dose range and
/// pushes ONE alert per frame carrying the count (#194). This file measures that the alert is really
/// produced on the path the gui drives — load a gain calibration, run <c>xpe_gain_correct</c> — and
/// that a scalar gain file produces nothing of the kind.
///
/// <para><b>Why the alert needs arranging.</b> It fires only when a pixel sits outside the fitted
/// range, so a frame inside the range is silent. That silence is a correct outcome, not a failure, and
/// telling the two apart is what the control case below is for.</para>
///
/// <para><b>The fixture is generated, never committed.</b> <c>xpe_calib_fixture_gen --gain-poly</c>
/// (QA-A-136) writes <c>gain_poly.xcal</c>; this lane does not build native binaries (#98), so the
/// generator is used only when a CI-staged copy is present and the case SKIPS with a reason when it is
/// not — a missing tool is "not measured", never "measured and fine".</para>
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class GainPolyClampAlertTests(Xunit.Abstractions.ITestOutputHelper output)
{
    private const int Width = 64;
    private const int Height = 64;
    private const int PixelCount = Width * Height;

    /// <summary>The marker of the alert under test (gain_correct.cpp).</summary>
    private const string ClampMarker = "fell outside the gain polynomial";

    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();

    [SkippableFact]
    public void PixelsOutsideTheFittedRange_RaiseOneAlertCarryingTheCount()
    {
        Skip.If(DllPath is null, "xpe_preprocess.dll is not staged.");
        var fixture = GeneratePolyFixture(out var skipReason);
        Skip.If(fixture is null, skipReason);

        try
        {
            var alerts = RunGainCorrect(Path.Combine(fixture!, "gain_poly.xcal"), OutOfRangeFrame());
            foreach (var a in alerts) output.WriteLine($"alert: {a}");

            var clamp = alerts.FirstOrDefault(a => a.Contains(ClampMarker, StringComparison.Ordinal));
            Assert.True(clamp is not null,
                $"No clamp alert was pushed. Alerts seen: {alerts.Count}. " +
                "Either no pixel fell outside the fitted range, or the alert is not reaching the queue (#194).");

            // The count is the point of the alert: one line per frame, not per pixel.
            Assert.Matches(@"^\d+ pixel\(s\) fell outside", clamp!);
            Assert.Single(alerts.Where(a => a.Contains(ClampMarker, StringComparison.Ordinal)));
        }
        finally
        {
            TryDelete(fixture);
        }
    }

    /// <summary>
    /// The control: the same frame through a SCALAR gain file raises no clamp alert. Without this,
    /// "the alert appeared" could be an alert this module pushes for anything.
    /// </summary>
    [SkippableFact]
    public void AScalarGainFile_RaisesNoClampAlert()
    {
        Skip.If(DllPath is null, "xpe_preprocess.dll is not staged.");
        var fixture = GeneratePolyFixture(out var skipReason);
        Skip.If(fixture is null, skipReason);

        try
        {
            var alerts = RunGainCorrect(Path.Combine(fixture!, "gain.xcal"), OutOfRangeFrame());
            foreach (var a in alerts) output.WriteLine($"alert: {a}");

            Assert.DoesNotContain(alerts, a => a.Contains(ClampMarker, StringComparison.Ordinal));
        }
        finally
        {
            TryDelete(fixture);
        }
    }

    /// <summary>
    /// A frame whose values run far past any dose range a generated polynomial can cover, so the clamp
    /// branch is reached. Half the frame sits at the top of the 16-bit range — saturated pixels and
    /// direct-exposure areas are exactly what #194 is about.
    /// </summary>
    private static ushort[] OutOfRangeFrame()
    {
        var pixels = new ushort[PixelCount];
        for (var i = 0; i < PixelCount; i++)
        {
            pixels[i] = i % 2 == 0 ? (ushort)65535 : (ushort)(1000 + (i % 500));
        }

        return pixels;
    }

    /// <summary>Loads a gain calibration, runs the gain stage once, and returns what the queue holds.</summary>
    private List<string> RunGainCorrect(string gainPath, ushort[] pixels)
    {
        Assert.True(File.Exists(gainPath), $"{gainPath} was not produced by the generator.");

        var handle = NativeLibrary.Load(DllPath!);
        try
        {
            var init = GetDelegate<XpePreprocessNative.InitDelegate>(handle, "xpe_preprocess_init");
            var shutdown = GetDelegate<XpePreprocessNative.ShutdownDelegate>(handle, "xpe_preprocess_shutdown");
            var loadGain = GetDelegate<XpePreprocessNative.CalibLoadDelegate>(handle, "xpe_calib_load_gain");
            var gainCorrect = GetDelegate<XpePreprocessNative.CorrectionDelegate>(handle, "xpe_gain_correct");

            shutdown();
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, init(IntPtr.Zero));

            try
            {
                ClearAlerts(handle);
                Assert.Equal(XpeCommonNative.XpeErrorCode.OK, loadGain(gainPath));

                // The load itself may say something; the queue is cleared again so what is measured
                // below belongs to the gain call alone.
                ClearAlerts(handle);

                var output32 = new float[PixelCount];
                var inputPin = GCHandle.Alloc(pixels, GCHandleType.Pinned);
                var outputPin = GCHandle.Alloc(output32, GCHandleType.Pinned);
                try
                {
                    var input = Buffer16(inputPin.AddrOfPinnedObject());
                    var result = Buffer32(outputPin.AddrOfPinnedObject());
                    var metadata = Metadata();

                    var code = gainCorrect(ref input, ref result, ref metadata);
                    output.WriteLine($"xpe_gain_correct -> {code}");
                }
                finally
                {
                    inputPin.Free();
                    outputPin.Free();
                }

                return DrainAlerts(handle);
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

    private static XpeCommonNative.XpeImageBuffer Buffer16(IntPtr data) => new()
    {
        Width = Width,
        Height = Height,
        BitsAllocated = 16,
        BitsStored = 16,
        Format = XpeCommonNative.XpePixelFormat.UInt16,
        Data = data,
        DataSize = (nuint)(PixelCount * sizeof(ushort)),
    };

    private static XpeCommonNative.XpeImageBuffer Buffer32(IntPtr data) => new()
    {
        Width = Width,
        Height = Height,
        BitsAllocated = 32,
        BitsStored = 32,
        Format = XpeCommonNative.XpePixelFormat.Float32,
        Data = data,
        DataSize = (nuint)(PixelCount * sizeof(float)),
    };

    private static XpeCommonNative.XpeImageMetadata Metadata() => new()
    {
        BodyPart = "Abdomen",
        KVp = 70.0f,
        MAs = 2.0f,
        SID_mm = 1000.0f,
        PixelPitch_mm = 0.14f,
        AcquisitionTime = 0,
        Flags = 0,
    };

    /// <summary>
    /// Produces offset/gain/defect plus <c>gain_poly.xcal</c>, or null with a reason.
    ///
    /// <para>A small frame on purpose: GUI-C-48 measured the generator at 53 s for 1024×1024, and
    /// nothing here needs a large one.</para>
    /// </summary>
    private string? GeneratePolyFixture(out string reason)
    {
        var generator = ResolveGenerator(out var searched);
        if (generator is null)
        {
            reason = $"xpe_calib_fixture_gen.exe was not found. Looked in: {string.Join(" ; ", searched)}.";
            return null;
        }

        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c129-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);

        var startInfo = new ProcessStartInfo(generator)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        foreach (var argument in new[]
                 {
                     "--out", directory, "--width", Width.ToString(), "--height", Height.ToString(),
                     "--seed", "0", "--gain-poly",
                 })
        {
            startInfo.ArgumentList.Add(argument);
        }

        using var process = Process.Start(startInfo);
        if (process is null)
        {
            reason = $"Could not start {generator}.";
            return null;
        }

        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        process.WaitForExit(120_000);
        output.WriteLine($"generator exit {process.ExitCode}: {stdout.Trim()} {stderr.Trim()}");

        var produced = Directory.GetFiles(directory, "*.xcal").Select(Path.GetFileName).Order().ToArray();
        output.WriteLine($"produced: {string.Join(", ", produced)}");

        if (process.ExitCode != 0 || !produced.Contains("gain_poly.xcal"))
        {
            reason = $"The generator did not write gain_poly.xcal (exit {process.ExitCode}). " +
                     "A staged copy predating QA-A-136 does not accept --gain-poly.";
            TryDelete(directory);
            return null;
        }

        reason = string.Empty;
        return directory;
    }

    private static string? ResolveGenerator(out string[] searched)
    {
        var candidates = new List<string>();
        var fromEnvironment = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrWhiteSpace(fromEnvironment))
        {
            candidates.Add(Path.Combine(fromEnvironment, "xpe_calib_fixture_gen.exe"));
        }

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !Directory.Exists(Path.Combine(dir.FullName, "gui")))
        {
            dir = dir.Parent;
        }

        if (dir is not null)
        {
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-common", "bin", "xpe_calib_fixture_gen.exe"));
            candidates.Add(Path.Combine(dir.FullName, "build", "e2e-native-dlls", "xpe_calib_fixture_gen.exe"));
        }

        searched = candidates.ToArray();
        return candidates.FirstOrDefault(File.Exists);
    }

    private static void TryDelete(string? directory)
    {
        if (directory is null) return;
        try
        {
            Directory.Delete(directory, recursive: true);
        }
        catch
        {
            // A leftover temp directory is not worth failing a measurement over.
        }
    }

    // The alert queue lives in xpe_common.dll, which xpe_preprocess.dll already loads — the same
    // handle resolves these exports (they are re-exported through the loaded module set).
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int PendingCountDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private delegate int GetAlertDelegate(int index, StringBuilder message, nuint messageLength, out int severity);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void ClearAlertsDelegate();

    private static void ClearAlerts(IntPtr handle) =>
        GetCommonDelegate<ClearAlertsDelegate>("xpe_clear_alerts")();

    /// <summary>
    /// Reads the queue and empties it. The read itself is non-destructive
    /// (<c>xpe_error.h</c>: "The alert queue is not modified"); the clear is explicit, so nothing here
    /// consumes what a later assertion was going to look at.
    /// </summary>
    private static List<string> DrainAlerts(IntPtr handle)
    {
        var count = GetCommonDelegate<PendingCountDelegate>("xpe_get_pending_alert_count")();
        var read = GetCommonDelegate<GetAlertDelegate>("xpe_get_pending_alert");
        var alerts = new List<string>();
        for (var i = 0; i < count; i++)
        {
            var buffer = new StringBuilder(512);
            if (read(i, buffer, (nuint)buffer.Capacity, out _) == 0)
            {
                alerts.Add(buffer.ToString());
            }
        }

        GetCommonDelegate<ClearAlertsDelegate>("xpe_clear_alerts")();
        return alerts;
    }

    /// <summary>
    /// The alert queue lives in xpe_common.dll, and this resolves it BY NAME rather than by path.
    ///
    /// <para><b>Measured, not styled.</b> Loading it by full path passed when this file ran alone and
    /// failed inside the whole suite with "Alerts seen: 0": the gain stage pushes into whichever
    /// xpe_common the already-loaded xpe_preprocess is bound to, and an earlier test in the same
    /// process can have bound that to a copy at a different path. A second path-load then reads a
    /// different queue — one process, two queues, and the alert lands in the other one. A name-based
    /// load returns the module that is already in the process, which is the one that was written to.</para>
    /// </summary>
    private static T GetCommonDelegate<T>(string name) where T : Delegate
    {
        var handle = NativeLibrary.Load("xpe_common.dll", typeof(GainPolyClampAlertTests).Assembly, null);
        Assert.True(NativeLibrary.TryGetExport(handle, name, out var address), $"{name} is not exported.");
        return Marshal.GetDelegateForFunctionPointer<T>(address);
    }

    private static T GetDelegate<T>(IntPtr handle, string name) where T : Delegate
    {
        Assert.True(NativeLibrary.TryGetExport(handle, name, out var address), $"{name} is not exported.");
        return Marshal.GetDelegateForFunctionPointer<T>(address);
    }
}
