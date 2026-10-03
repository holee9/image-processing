// GUI-C-212b (#249, Codex #96): the oracle's own process, its temporary folder, and what a hung or silent child means.
using System.Diagnostics;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// The in-process oracle calls xpe_preprocess_shutdown and loads synthetic maps into the module's process-global store; run inside the app it would erase the operator's loaded
/// calibration. The app therefore runs it in a child process (<c>XpePreprocessOracleProcess</c>). These tests hold the pieces that do not need the app executable: the temporary-folder
/// ownership, the child's timeout / kill / no-result handling, and the result's JSON round trip. That the operator's calibration survives an oracle run is the E2E scenario
/// (<c>LegacyPreviewChainScenarios</c>'s project), which launches the real executable.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessSyntheticOracleProcessTests
{
    private static readonly string? DllPath = XpePreprocessNative.TryFindDll();

    private static string NewRoot()
    {
        var root = Path.Combine(Path.GetTempPath(), $"xpe_oracle_root_{Guid.NewGuid():N}");
        Directory.CreateDirectory(root);
        return root;
    }

    private static string OwnName() => $"xpe_oracle_{Guid.NewGuid():N}";

    [SkippableFact]
    public void ADeletionFailure_IsReportedInTheResult_NotHidden()
    {
        Skip.If(DllPath is null, "Skipped: xpe_preprocess.dll not staged");
        var root = NewRoot();
        FileStream? held = null;
        try
        {
            var result = XpePreprocessSyntheticOracle.Run(DllPath!, new XpePreprocessSyntheticOracle.OracleOptions(
                TempRoot: root,
                OnTempDirReady: dir => held = new FileStream(Path.Combine(dir, "held.bin"), FileMode.Create, FileAccess.ReadWrite, FileShare.None)));

            Assert.True(result.Passed, result.Details);   // the chain itself is unaffected
            Assert.NotNull(result.TempCleanupWarning);
            Assert.Contains("was not deleted", result.TempCleanupWarning);
            Assert.Single(Directory.GetDirectories(root));   // and the folder really is still there: the warning is true
        }
        finally
        {
            held?.Dispose();
            Directory.Delete(root, recursive: true);
        }
    }

    /// <summary>The control for the test above: with nothing holding the folder, there is no warning (a warning that is always present says nothing).</summary>
    [SkippableFact]
    public void ACleanRun_HasNoCleanupWarning_AndLeavesNothing()
    {
        Skip.If(DllPath is null, "Skipped: xpe_preprocess.dll not staged");
        var root = NewRoot();
        try
        {
            var result = XpePreprocessSyntheticOracle.Run(DllPath!, new XpePreprocessSyntheticOracle.OracleOptions(TempRoot: root));
            Assert.True(result.Passed, result.Details);
            Assert.Null(result.TempCleanupWarning);
            Assert.Empty(Directory.GetDirectories(root));
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [SkippableFact]
    public void AStart_ReclaimsItsOwnOldFolders_AndOnlyThose()
    {
        Skip.If(DllPath is null, "Skipped: xpe_preprocess.dll not staged");
        var root = NewRoot();
        try
        {
            var old = Path.Combine(root, OwnName());
            var young = Path.Combine(root, OwnName());
            var foreignOld = Path.Combine(root, "someone_elses_old_folder");
            var lookalikeOld = Path.Combine(root, "xpe_oracle_not_a_guid");
            foreach (var d in new[] { old, young, foreignOld, lookalikeOld })
            {
                Directory.CreateDirectory(d);
                File.WriteAllText(Path.Combine(d, "f.txt"), "x");
            }

            foreach (var d in new[] { old, foreignOld, lookalikeOld })
            {
                Directory.SetLastWriteTimeUtc(d, DateTime.UtcNow.AddDays(-2));
            }

            var result = XpePreprocessSyntheticOracle.Run(DllPath!, new XpePreprocessSyntheticOracle.OracleOptions(TempRoot: root));

            Assert.True(result.Passed, result.Details);
            Assert.False(Directory.Exists(old), "an old folder of the oracle's own name shape must be reclaimed");
            Assert.True(Directory.Exists(young), "a young folder may belong to a run still going: never touched");
            Assert.True(Directory.Exists(foreignOld), "a folder that is not the oracle's is never touched, however old");
            Assert.True(Directory.Exists(lookalikeOld), "only the exact name shape (prefix + 32 hex) counts as the oracle's own");
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [SkippableFact]
    public void AChildThatNeverAnswers_IsKilled_AndReportedAsATimeout()
    {
        var ping = Path.Combine(Environment.SystemDirectory, "PING.EXE");
        Skip.If(!File.Exists(ping), "PING.EXE not available");
        var watch = Stopwatch.StartNew();

        var result = XpePreprocessOracleProcess.Run(ping, ["-n", "60", "127.0.0.1"], TimeSpan.FromSeconds(2));

        watch.Stop();
        Assert.Equal("Oracle process timed out", result.Status);
        Assert.False(result.Passed);
        Assert.Contains("was killed", result.Details);
        Assert.True(watch.Elapsed < TimeSpan.FromSeconds(30), $"the wait must be bounded by the timeout, not by the child ({watch.Elapsed.TotalSeconds:0.#} s)");
    }

    [Fact]
    public void AChildThatExitsWithoutAResultLine_IsAFailure_NeverAPass()
    {
        var cmd = Path.Combine(Environment.SystemDirectory, "cmd.exe");
        var result = XpePreprocessOracleProcess.Run(cmd, ["/c", "echo not json"], TimeSpan.FromSeconds(30));

        Assert.False(result.Passed);
        Assert.Equal("Oracle process gave no result", result.Status);
        Assert.Contains("no result line", result.Details);
    }

    [Fact]
    public void AMissingExecutable_IsNotRun()
    {
        var result = XpePreprocessOracleProcess.Run(Path.Combine(Path.GetTempPath(), $"no_such_{Guid.NewGuid():N}.exe"), [], TimeSpan.FromSeconds(5));

        Assert.False(result.Executed);
        Assert.False(result.Passed);
    }

    /// <summary>The worker's one line parses back into the same verdict (NaN included: the failed-result shape carries NaN fields).</summary>
    [Fact]
    public void TheResultLine_RoundTrips_IncludingNaNFields()
    {
        var failed = PreprocessSyntheticOracleResult.Failed("Calibration setup failed", "xpe_calib_load_gain returned CALIB_NOT_LOADED.") with { TempCleanupWarning = "w" };

        var parsed = XpePreprocessOracleProcess.TryParse(XpePreprocessOracleProcess.Serialize(failed));

        Assert.NotNull(parsed);
        Assert.Equal(failed.Status, parsed!.Status);
        Assert.False(parsed.Passed);
        Assert.True(double.IsNaN(parsed.DeterminismRmse));
        Assert.Equal("w", parsed.TempCleanupWarning);
        Assert.Null(XpePreprocessOracleProcess.TryParse("{ not json"));
    }

    /// <summary>
    /// A SOURCE-TEXT check, named as one: the behavioural guards above and the E2E isolation scenario hold the host, but nothing observable from outside says which entry the app's own
    /// readiness code calls, and a caller that goes back to the in-process oracle brings the wipe back. Only the host and the worker (this file's two linked classes) may call it.
    /// </summary>
    [Fact]
    public void NoAppCode_RunsTheOracleInTheAppsProcess_ExceptTheWorkerEntry()
    {
        var appDir = FindAppDir();
        var callers = Directory.EnumerateFiles(appDir, "*.cs", SearchOption.AllDirectories)
            .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") && !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .Where(f => File.ReadAllText(f).Contains("XpePreprocessSyntheticOracle.Run(", StringComparison.Ordinal))
            .Select(f => Path.GetFileName(f))
            .Order()
            .ToArray();

        // Control: the scan sees the call that must exist (the host's worker entry), so an empty result cannot be a scan that read nothing.
        Assert.Contains("XpePreprocessOracleProcess.cs", callers);
        Assert.Equal(["XpePreprocessOracleProcess.cs"], callers);
        // GUI-C-219: the readiness probe asks the shared verdict holder, and the holder's runner is the child-process host: the in-process oracle is reached by neither.
        Assert.Contains("PreprocessOracleVerdicts.", File.ReadAllText(Path.Combine(appDir, "Diagnostics", "XpePreprocessReadinessProbe.cs")), StringComparison.Ordinal);
        Assert.Contains("= XpePreprocessOracleProcess.Run;", File.ReadAllText(Path.Combine(appDir, "Diagnostics", "PreprocessOracleVerdicts.cs")), StringComparison.Ordinal);
    }

    private static string FindAppDir()
    {
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest");
            if (File.Exists(Path.Combine(candidate, "App.xaml.cs"))) return candidate;
        }

        throw new DirectoryNotFoundException("clients/ImageProcTest was not found above the test output.");
    }

    // GUI-C-225b: TheWorker_WritesExactlyOneParsableResultLine lived here and called RunWorker inside the test host. It moved to LegacyOracleConfinementScenarios, which runs the worker as the clean
    // separate process it is in the app (the worker's load is confined and audited, and a host that already holds modules of the same names would be reported by that audit).
}
