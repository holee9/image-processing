// GUI-C-212b (#249, Codex #96): running the readiness oracle must not change the calibration the operator has loaded.
using System.Diagnostics;
using System.Runtime.InteropServices;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

/// <summary>
/// The synthetic oracle shuts the module down, loads synthetic maps into its process-global calibration store and shuts down again. In the app's process that erased the operator's
/// loaded calibration on every readiness refresh. The app now runs the oracle in a child process (its own executable, <c>--run-preprocess-oracle</c>); this scenario holds a calibration in
/// THIS process (the "operator"), runs the oracle through the real executable while a second thread keeps calling the module, and compares correction output byte for byte before, during
/// and after. The same scenario with the oracle run in this process is the falsification: it must fail, which is what tells the passing run something.
/// </summary>
public sealed class LegacyOracleIsolationScenarios(ITestOutputHelper output)
{
    private const int N = 16;
    private const ushort OperatorDark = 300;   // distinct from the oracle's own synthetic dark (100), so a synthetic map in the module shows in the output

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate XpeCommonApi.XpeErrorCode InitDelegate(IntPtr config);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void ShutdownDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate XpeCommonApi.XpeErrorCode CorrectionDelegate(ref XpeCommonApi.XpeImageBuffer input, ref XpeCommonApi.XpeImageBuffer output, ref XpeCommonApi.XpeImageMetadata metadata);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private delegate XpeCommonApi.XpeErrorCode GenerateOffsetDelegate(
        [In] XpeCommonApi.XpeImageBuffer[] darkFrames, int numFrames, float integrationTimeMs, float temperatureC,
        [MarshalAs(UnmanagedType.LPStr)] string outputPath, [MarshalAs(UnmanagedType.LPStr)] string? configJsonOrNull);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private delegate XpeCommonApi.XpeErrorCode LoadDelegate([MarshalAs(UnmanagedType.LPStr)] string path);

    private sealed record Operator(IntPtr Handle, CorrectionDelegate Offset, ushort[] Raw, ushort[] Expected, string Dir);

    private static string? Dll() => Environment.GetEnvironmentVariable("XPE_NATIVE_DIR") is { Length: > 0 } d && File.Exists(Path.Combine(d, "xpe_preprocess.dll")) ? Path.Combine(d, "xpe_preprocess.dll") : null;

    private static T Bind<T>(IntPtr handle, string name) where T : Delegate => Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(handle, name));

    /// <summary>This process becomes the operator: DLL loaded, module initialised, an offset calibration (a uniform dark of 300) generated and loaded.</summary>
    private static Operator BecomeTheOperator(string dll)
    {
        NativeDependencyLoader.TryLoadFor(dll);
        var handle = NativeLibrary.Load(dll);
        Bind<ShutdownDelegate>(handle, "xpe_preprocess_shutdown")();
        Assert.Equal(XpeCommonApi.XpeErrorCode.OK, Bind<InitDelegate>(handle, "xpe_preprocess_init")(IntPtr.Zero));

        var dir = Path.Combine(Path.GetTempPath(), $"xpe_operator_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        var dark = Enumerable.Repeat(OperatorDark, N * N).ToArray();
        var pin = GCHandle.Alloc(dark, GCHandleType.Pinned);
        try
        {
            var frame = Buffer(XpeCommonApi.XpePixelFormat.UInt16, pin.AddrOfPinnedObject());
            var path = Path.Combine(dir, "offset.xcal");
            Assert.Equal(XpeCommonApi.XpeErrorCode.OK, Bind<GenerateOffsetDelegate>(handle, "xpe_calib_generate_offset")([frame], 1, 100f, 25f, path, null));
            Assert.Equal(XpeCommonApi.XpeErrorCode.OK, Bind<LoadDelegate>(handle, "xpe_calib_load_offset")(path));
        }
        finally { pin.Free(); }

        var raw = Enumerable.Range(0, N * N).Select(i => (ushort)(1000 + i)).ToArray();
        var expected = raw.Select(v => (ushort)(v - OperatorDark)).ToArray();   // plain arithmetic, not the module's answer
        return new Operator(handle, Bind<CorrectionDelegate>(handle, "xpe_offset_correct"), raw, expected, dir);
    }

    private static XpeCommonApi.XpeImageBuffer Buffer(XpeCommonApi.XpePixelFormat format, IntPtr data) => new()
    {
        Width = N, Height = N, BitsAllocated = 16, BitsStored = 16, Format = format, Data = data, DataSize = (nuint)(N * N * sizeof(ushort)),
    };

    /// <summary>One offset correction of the operator's raw frame: the return code and the output.</summary>
    private static (XpeCommonApi.XpeErrorCode Code, ushort[] Output) Correct(Operator op)
    {
        var output = new ushort[N * N];
        var raw = (ushort[])op.Raw.Clone();
        var inPin = GCHandle.Alloc(raw, GCHandleType.Pinned);
        var outPin = GCHandle.Alloc(output, GCHandleType.Pinned);
        try
        {
            var input = Buffer(XpeCommonApi.XpePixelFormat.UInt16, inPin.AddrOfPinnedObject());
            var result = Buffer(XpeCommonApi.XpePixelFormat.UInt16, outPin.AddrOfPinnedObject());
            var metadata = new XpeCommonApi.XpeImageMetadata { BodyPart = "CHEST", KVp = 70f, MAs = 2f, SID_mm = 1000f, PixelPitch_mm = 0.14f };
            var code = op.Offset(ref input, ref result, ref metadata);
            return (code, output);
        }
        finally { outPin.Free(); inPin.Free(); }
    }

    private static void Retire(Operator op)
    {
        Bind<ShutdownDelegate>(op.Handle, "xpe_preprocess_shutdown")();
        NativeLibrary.Free(op.Handle);
        try { Directory.Delete(op.Dir, recursive: true); } catch (IOException) { /* temp folder */ }
    }

    private static string? FindExecutable()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "ImageProcTest.exe");
            if (File.Exists(candidate)) return candidate;
        }

        return null;
    }

    /// <summary>A stale binary reports a green for code that never ran: the legacy ImageProcTest.dll must be newer than every source it was built from.</summary>
    private static void AssertFresh(string exe)
    {
        var projectDir = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(exe)!, "..", "..", ".."));
        var built = File.GetLastWriteTimeUtc(Path.ChangeExtension(exe, ".dll"));
        var newer = Directory.EnumerateFiles(projectDir, "*.cs", SearchOption.AllDirectories)
            .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .FirstOrDefault(f => File.GetLastWriteTimeUtc(f) > built);
        Assert.True(newer is null, $"clients/ImageProcTest's ImageProcTest.dll is older than {newer}: rebuild it (dotnet build clients/ImageProcTest/ImageProcTest.csproj -c Debug) before this scenario.");
    }

    /// <summary>Runs <paramref name="runOracle"/> while another thread keeps correcting; returns how many of those corrections differed from the operator's expected output.</summary>
    private static (int Calls, int Deviations, string First) CorrectWhile(Operator op, Action runOracle)
    {
        var stop = false;
        var calls = 0;
        var deviations = 0;
        var first = "";
        var worker = new Thread(() =>
        {
            while (!Volatile.Read(ref stop))
            {
                var (code, outPixels) = Correct(op);
                calls++;
                if (code != XpeCommonApi.XpeErrorCode.OK || !outPixels.SequenceEqual(op.Expected))
                {
                    deviations++;
                    if (first.Length == 0) first = code != XpeCommonApi.XpeErrorCode.OK ? $"code {code}" : $"output[0]={outPixels[0]} (expected {op.Expected[0]})";
                }
            }
        });
        worker.Start();
        try { runOracle(); }
        finally { Volatile.Write(ref stop, true); worker.Join(); }
        return (calls, deviations, first);
    }

    [SkippableFact]
    public void TheOracleInItsOwnProcess_LeavesTheOperatorsCalibration_ByteForByte()
    {
        var dll = Dll();
        Skip.If(dll is null, "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");
        var exe = FindExecutable();
        Skip.If(exe is null, "clients/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe was not found: build clients/ImageProcTest first.");
        AssertFresh(exe!);

        var op = BecomeTheOperator(dll!);
        try
        {
            var (code, before) = Correct(op);
            Assert.Equal(XpeCommonApi.XpeErrorCode.OK, code);
            Assert.Equal(op.Expected, before);   // the operator's calibration is the one loaded, and the arithmetic agrees

            PreprocessSyntheticOracleResult? verdict = null;
            var (calls, deviations, first) = CorrectWhile(op, () => verdict = XpePreprocessOracleProcess.Run(exe, [XpePreprocessOracleProcess.ModeArgument, dll!], TimeSpan.FromSeconds(90)));

            Assert.NotNull(verdict);
            Assert.True(verdict!.Passed, $"{verdict.Status}: {verdict.Details}");
            Assert.True(calls > 0, "the concurrent corrections never ran: the 'during' half of this scenario observed nothing");
            Assert.True(deviations == 0, $"{deviations} of {calls} corrections made while the oracle ran differed from the operator's output (first: {first})");

            var (afterCode, after) = Correct(op);
            Assert.Equal(XpeCommonApi.XpeErrorCode.OK, afterCode);
            Assert.Equal(before, after);
            output.WriteLine($"oracle verdict {verdict.Status}; {calls} concurrent corrections, {deviations} deviations; operator output unchanged");
        }
        finally { Retire(op); }
    }

    /// <summary>The falsification, kept as a test: the oracle run IN this process destroys the operator's calibration, and the same observation sees it. A scenario that could not fail proves nothing.</summary>
    [SkippableFact]
    public void Control_TheOracleInTheSameProcess_ErasesTheOperatorsCalibration_AndTheScenarioSeesIt()
    {
        var dll = Dll();
        Skip.If(dll is null, "XPE_NATIVE_DIR does not name a folder containing xpe_preprocess.dll.");

        var op = BecomeTheOperator(dll!);
        try
        {
            var (_, deviations, first) = CorrectWhile(op, () => XpePreprocessSyntheticOracle.Run(dll!));
            var (afterCode, after) = Correct(op);
            var changed = deviations > 0 || afterCode != XpeCommonApi.XpeErrorCode.OK || !after.SequenceEqual(op.Expected);

            Assert.True(changed, "an in-process oracle left the operator's calibration alone: the isolation scenario cannot tell the two apart");
            output.WriteLine($"in-process oracle: {deviations} deviations (first: {first}); after: code {afterCode}");
        }
        finally { Retire(op); }
    }
}
