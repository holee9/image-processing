// GUI-C-212c (#249, Codex #98): what the parent believes about the oracle's child, and what happens to the child when the parent lets go.
using System.Diagnostics;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// The parent used to return the last line that parsed as JSON whatever the exit code, and the readiness check read the single field <c>Passed</c>: a child that crashed after printing
/// <c>{"Passed":true}</c>, or any stray JSON line, was a pass. These tests hand the parent every shape of untrustworthy output and require a failure that names what was wrong, hold the
/// one shape that must pass (the control: a checker that rejects everything proves nothing), and show that a child in the kill-on-close job dies when the parent's handle closes.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessSyntheticOracleProtocolTests
{
    private static readonly string Hash = new('a', 64);

    /// <summary>A complete, internally consistent passing result: the one shape that must be accepted.</summary>
    private static PreprocessSyntheticOracleResult Good() => new(
        Status: "Synthetic oracle pass",
        Details: "ok",
        Executed: true,
        Passed: true,
        TotalLatencyMs: 1.5,
        InputPreserved: true,
        RawSha256Before: Hash,
        RawSha256After: Hash,
        OutputSha256: new string('b', 64),
        NaNInfCount: 0,
        DeterminismRmse: 0.0,
        OutputMin: 1.0,
        OutputMax: 50.0,
        Stages:
        [
            new PreprocessSyntheticStageResult("offset", "OK", 0.1, 3.0, true),
            new PreprocessSyntheticStageResult("gain", "OK", 0.1, 2.0, true),
            new PreprocessSyntheticStageResult("defect", "OK", 0.1, 1.0, true),
        ]);

    private static string Line(PreprocessSyntheticOracleResult r) => XpePreprocessOracleProcess.FormatResultLine(r) + "\n";

    // ---- the control and the shapes that must fail

    [Fact]
    public void Control_ACompleteConsistentResult_OnExitCodeZero_IsAccepted()
    {
        var parsed = XpePreprocessOracleProcess.ParseOutput("some log line\n" + Line(Good()) + "another log line\n", 0, string.Empty);

        Assert.True(parsed.Passed, $"{parsed.Status}: {parsed.Details}");
        Assert.Equal("Synthetic oracle pass", parsed.Status);
    }

    [Fact]
    public void ACrashAfterAGoodLine_IsAFailure_BecauseTheExitCodeIsNotZero()
    {
        var parsed = XpePreprocessOracleProcess.ParseOutput(Line(Good()), 1, "access violation");

        Assert.False(parsed.Passed);
        Assert.Equal("Oracle process failed", parsed.Status);
        Assert.Contains("Exit code 1", parsed.Details);
        Assert.Contains("access violation", parsed.Details);
    }

    [Fact]
    public void TwoResultLines_AreAmbiguous_AndFail()
    {
        var parsed = XpePreprocessOracleProcess.ParseOutput(Line(Good()) + Line(Good()), 0, string.Empty);

        Assert.False(parsed.Passed);
        Assert.Equal("Oracle process gave an ambiguous result", parsed.Status);
    }

    [Fact]
    public void AJsonLineWithoutTheProtocolPrefix_IsNotAResult()
    {
        var parsed = XpePreprocessOracleProcess.ParseOutput(XpePreprocessOracleProcess.Serialize(Good()) + "\n", 0, string.Empty);

        Assert.False(parsed.Passed);
        Assert.Equal("Oracle process gave no result", parsed.Status);
    }

    [Fact]
    public void AResultThatSaysOnlyPassedTrue_IsInvalid_BecauseRequiredFieldsAreMissing()
    {
        var parsed = XpePreprocessOracleProcess.ParseOutput(XpePreprocessOracleProcess.ResultPrefix + "{\"Passed\":true}\n", 0, string.Empty);

        Assert.False(parsed.Passed);
        Assert.Equal("Oracle process result invalid", parsed.Status);
        Assert.Contains("Required field(s) missing", parsed.Details);
        Assert.Contains("Status", parsed.Details);
        Assert.Contains("Stages", parsed.Details);
    }

    [Theory]
    [InlineData("[1, 2]")]
    [InlineData("\"passed\"")]
    [InlineData("{ not json")]
    [InlineData("null")]
    public void ALineThatIsNotAJsonObject_IsInvalid(string payload)
    {
        var parsed = XpePreprocessOracleProcess.ParseOutput(XpePreprocessOracleProcess.ResultPrefix + payload + "\n", 0, string.Empty);

        Assert.False(parsed.Passed);
        Assert.Equal("Oracle process result invalid", parsed.Status);
    }

    /// <summary>Every success invariant, broken one at a time on an otherwise good result that still says <c>Passed: true</c>.</summary>
    private static readonly (string What, PreprocessSyntheticOracleResult Broken, string Fragment)[] Cases = BuildCases();

    private static (string, PreprocessSyntheticOracleResult, string)[] BuildCases()
    {
        var good = Good();
        return
        [
            ("not executed", good with { Executed = false }, "not Executed"),
            ("another status", good with { Status = "Synthetic oracle fail" }, "Status is"),
            ("a stage that failed", good with { Stages = [good.Stages[0], good.Stages[1] with { Passed = false }, good.Stages[2]] }, "did not pass"),
            ("a stage with an error code", good with { Stages = [good.Stages[0], good.Stages[1] with { ErrorCode = "CALIB_NOT_LOADED" }, good.Stages[2]] }, "code CALIB_NOT_LOADED"),
            ("stages in another order", good with { Stages = [good.Stages[1], good.Stages[0], good.Stages[2]] }, "expected [offset, gain, defect]"),
            ("a missing stage", good with { Stages = [good.Stages[0], good.Stages[1]] }, "expected [offset, gain, defect]"),
            ("no stages", good with { Stages = [] }, "expected [offset, gain, defect]"),
            ("input not preserved", good with { InputPreserved = false }, "not preserved"),
            ("an input hash that changed", good with { RawSha256After = new string('c', 64) }, "hash changed"),
            ("an empty input hash", good with { RawSha256Before = string.Empty, RawSha256After = string.Empty }, "hash is missing"),
            ("an empty output hash", good with { OutputSha256 = string.Empty }, "output hash"),
            ("non-finite output values", good with { NaNInfCount = 3 }, "non-finite output"),
            ("runs that differ", good with { DeterminismRmse = 0.25 }, "two runs differ"),
            ("a NaN determination", good with { DeterminismRmse = double.NaN }, "two runs differ"),
            ("an output with no range", good with { OutputMin = 5.0, OutputMax = 5.0 }, "no finite range"),
            ("an infinite output maximum", good with { OutputMax = double.PositiveInfinity }, "no finite range"),
            ("a NaN stage measurement", good with { Stages = [good.Stages[0] with { MaxAbsError = double.NaN }, good.Stages[1], good.Stages[2]] }, "non-finite measurement"),
        ];
    }

    public static IEnumerable<object[]> BrokenInvariants() => Enumerable.Range(0, 17).Select(i => new object[] { i });

    [Fact]
    public void TheCaseTable_HasAsManyEntriesAsTheTheoryEnumerates()
    {
        Assert.Equal(17, Cases.Length);
    }

    [Theory]
    [MemberData(nameof(BrokenInvariants))]
    public void ASelfContradictoryPass_IsInvalid(int index)
    {
        var (what, broken, expectedFragment) = Cases[index];
        Assert.True(broken.Passed, $"{what}: the control itself must still claim a pass, or this tests nothing");

        var parsed = XpePreprocessOracleProcess.ParseOutput(Line(broken), 0, string.Empty);

        Assert.False(parsed.Passed, $"{what}: accepted");
        Assert.Equal("Oracle process result invalid", parsed.Status);
        Assert.Contains(expectedFragment, parsed.Details, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>An honest failure is not an invalid result: the child can say it did not pass, and the parent passes that on.</summary>
    [Fact]
    public void AnHonestFailureResult_IsPassedOn_AsAFailureWithItsOwnStatus()
    {
        var failed = PreprocessSyntheticOracleResult.Failed("Calibration setup failed", "xpe_calib_load_gain returned CALIB_NOT_LOADED.");

        var parsed = XpePreprocessOracleProcess.ParseOutput(Line(failed), 0, string.Empty);

        Assert.False(parsed.Passed);
        Assert.Equal("Calibration setup failed", parsed.Status);
    }

    // ---- with a real child process

    private static string FakeChild(string stdout, int exitCode)
    {
        var dir = Path.Combine(Path.GetTempPath(), $"xpe_fakechild_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "out.txt"), stdout);
        var cmd = Path.Combine(dir, "run.cmd");
        File.WriteAllText(cmd, $"@type \"%~dp0out.txt\"\r\n@exit /b {exitCode}\r\n");
        return cmd;
    }

    private static PreprocessSyntheticOracleResult RunFake(string stdout, int exitCode)
    {
        var cmd = FakeChild(stdout, exitCode);
        try
        {
            return XpePreprocessOracleProcess.Run(Path.Combine(Environment.SystemDirectory, "cmd.exe"), ["/c", cmd], TimeSpan.FromSeconds(30));
        }
        finally
        {
            try { Directory.Delete(Path.GetDirectoryName(cmd)!, recursive: true); } catch (IOException) { /* temp folder */ }
        }
    }

    [Fact]
    public void Control_ARealChildThatPrintsAGoodLine_AndExitsZero_IsAccepted()
    {
        var parsed = RunFake(Line(Good()), 0);

        Assert.True(parsed.Passed, $"{parsed.Status}: {parsed.Details}");
    }

    [Fact]
    public void ARealChildThatPrintsAGoodLine_ButExitsWithOne_IsAFailure()
    {
        var parsed = RunFake(Line(Good()), 1);

        Assert.False(parsed.Passed);
        Assert.Equal("Oracle process failed", parsed.Status);
        Assert.Contains("Exit code 1", parsed.Details);
    }

    // ---- the job object

    private static Process StartPing()
    {
        var info = new ProcessStartInfo(Path.Combine(Environment.SystemDirectory, "PING.EXE")) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true };
        info.ArgumentList.Add("-n");
        info.ArgumentList.Add("60");
        info.ArgumentList.Add("127.0.0.1");
        return Process.Start(info)!;
    }

    [Fact]
    public void AChildInTheJob_DiesWhenTheJobHandleCloses_AndOneNotInTheJobDoesNot()
    {
        using var inside = StartPing();
        using var outside = StartPing();
        try
        {
            var job = KillOnCloseJob.Create();
            job.Assign(inside);

            Assert.False(inside.HasExited);
            job.Dispose();   // what the system does when the parent process ends, in any way

            Assert.True(inside.WaitForExit(15000), "the child in the kill-on-close job outlived the closed handle");
            Assert.False(outside.HasExited, "control: a child that was never assigned to the job is not affected");
        }
        finally
        {
            if (!outside.HasExited) outside.Kill(entireProcessTree: true);
            if (!inside.HasExited) inside.Kill(entireProcessTree: true);
        }
    }

    /// <summary>The host's own wiring: the child it starts is already in a kill-on-close job when the test seam sees it, and closing that job ends it long before the 60 s timeout.</summary>
    [Fact]
    public void TheHostsChild_IsInAKillOnCloseJob_SoClosingItEndsTheChildAtOnce()
    {
        var ping = Path.Combine(Environment.SystemDirectory, "PING.EXE");
        var pid = 0;
        var watch = Stopwatch.StartNew();

        var result = XpePreprocessOracleProcess.Run(ping, ["-n", "60", "127.0.0.1"], TimeSpan.FromSeconds(60), (process, job) =>
        {
            pid = process.Id;
            job.Dispose();   // the parent letting go
        });

        watch.Stop();
        Assert.False(result.Passed);
        Assert.True(watch.Elapsed < TimeSpan.FromSeconds(30), $"the child was not ended by the job ({watch.Elapsed.TotalSeconds:0.#} s, timeout 60 s)");
        Assert.NotEqual(0, pid);
        Assert.Throws<ArgumentException>(() => Process.GetProcessById(pid));
    }

    [Fact]
    public void Control_WithoutClosingTheJob_TheSameChildRunsUntilItIsDoneOrKilled()
    {
        var result = XpePreprocessOracleProcess.Run(Path.Combine(Environment.SystemDirectory, "PING.EXE"), ["-n", "60", "127.0.0.1"], TimeSpan.FromSeconds(2));

        Assert.Equal("Oracle process timed out", result.Status);   // it was still running when the timeout came: the job closing is what ended it in the test above
    }

    [Fact]
    public void AHookThatThrows_StillEndsTheChild_AndSaysSo()
    {
        var pid = 0;
        var result = XpePreprocessOracleProcess.Run(Path.Combine(Environment.SystemDirectory, "PING.EXE"), ["-n", "60", "127.0.0.1"], TimeSpan.FromSeconds(60), (process, _) =>
        {
            pid = process.Id;
            throw new InvalidOperationException("boom");
        });

        Assert.False(result.Passed);
        Assert.Equal("Oracle process host failed", result.Status);
        Assert.Contains("was killed", result.Details);
        Assert.Throws<ArgumentException>(() => Process.GetProcessById(pid));
    }
}
