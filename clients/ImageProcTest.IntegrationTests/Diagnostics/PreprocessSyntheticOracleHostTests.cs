// GUI-C-212d (#249, Codex #100): the child starts suspended and inside its job; its output is read under fixed limits; the protocol is UTF-8.
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// 212c put the child in a kill-on-close job after <c>Process.Start</c>, which leaves an instant in which the child runs outside it; and read the child's output with
/// <c>ReadToEndAsync</c>, which holds whatever the child prints. Here the child is observed SUSPENDED before it is in the job, a close of the job before the resume means the child never ran,
/// and a child that prints without bound is stopped at fixed limits while the parent's memory stays small.
/// </summary>
[Trait("Category", "P1AReady")]
public sealed class PreprocessSyntheticOracleHostTests
{
    private static readonly string Cmd = Path.Combine(Environment.SystemDirectory, "cmd.exe");
    private static readonly string Ping = Path.Combine(Environment.SystemDirectory, "PING.EXE");

    private static string TempDir()
    {
        var dir = Path.Combine(Path.GetTempPath(), $"xpe_host_{Guid.NewGuid():N}");
        Directory.CreateDirectory(dir);
        return dir;
    }

    private static void Cleanup(string dir)
    {
        try { Directory.Delete(dir, recursive: true); } catch (IOException) { /* temp folder */ }
    }

    /// <summary>A child that writes <paramref name="bytes"/> to standard output (or standard error) exactly as they are, then exits 0.</summary>
    private static PreprocessSyntheticOracleResult RunBytes(byte[] bytes, bool toStderr = false, OutputLimits? limits = null)
    {
        var dir = TempDir();
        try
        {
            File.WriteAllBytes(Path.Combine(dir, "out.bin"), bytes);
            File.WriteAllText(Path.Combine(dir, "run.cmd"), $"@type \"%~dp0out.bin\"{(toStderr ? " 1>&2" : string.Empty)}\r\n@exit /b 0\r\n");
            return XpePreprocessOracleProcess.Run(Cmd, ["/c", Path.Combine(dir, "run.cmd")], TimeSpan.FromSeconds(60), limits: limits);
        }
        finally
        {
            Cleanup(dir);
        }
    }

    private static byte[] Repeat(string prefix, int count, byte fill = (byte)'x') =>
        Encoding.ASCII.GetBytes(prefix).Concat(Enumerable.Repeat(fill, count)).ToArray();

    private static PreprocessSyntheticOracleResult Good() => new(
        Status: "Synthetic oracle pass", Details: "ok", Executed: true, Passed: true, TotalLatencyMs: 1.5, InputPreserved: true,
        RawSha256Before: new string('a', 64), RawSha256After: new string('a', 64), OutputSha256: new string('b', 64), NaNInfCount: 0, DeterminismRmse: 0.0,
        OutputMin: 1.0, OutputMax: 50.0,
        Stages:
        [
            new PreprocessSyntheticStageResult("offset", "OK", 0.1, 3.0, true),
            new PreprocessSyntheticStageResult("gain", "OK", 0.1, 2.0, true),
            new PreprocessSyntheticStageResult("defect", "OK", 0.1, 1.0, true),
        ]);

    // ---- 2: suspended start, then the job, then the resume

    private static bool IsSuspended(int pid)
    {
        using var process = Process.GetProcessById(pid);
        process.Refresh();
        return process.Threads.Cast<ProcessThread>().All(t => t.ThreadState == System.Diagnostics.ThreadState.Wait && t.WaitReason == ThreadWaitReason.Suspended);
    }

    [Fact]
    public void TheChild_IsSuspendedWhenItExists_AndRunningOnlyAfterTheResume()
    {
        bool? suspendedAtCreate = null;
        bool? suspendedAfterAssign = null;
        bool? suspendedAfterResume = null;

        var result = XpePreprocessOracleProcess.Run(Ping, ["-n", "60", "127.0.0.1"], TimeSpan.FromSeconds(3), new HostSeams(
            AfterCreate: pid => suspendedAtCreate = IsSuspended(pid),
            AfterAssign: (pid, _) => suspendedAfterAssign = IsSuspended(pid),
            AfterResume: (pid, _) => { Thread.Sleep(300); suspendedAfterResume = IsSuspended(pid); }));

        Assert.Equal("Oracle process timed out", result.Status);   // it ran until the timeout: it was resumed
        Assert.True(suspendedAtCreate, "the child was not suspended when it was created");
        Assert.True(suspendedAfterAssign, "the child was not still suspended once it was in the job");
        Assert.False(suspendedAfterResume, "control: after the resume the same observation says running, so 'suspended' above was a measurement");
    }

    /// <summary>
    /// The instant that mattered: the parent ending between the start and the assignment. Here the job closes right after the assignment and before the resume (the closest a test can
    /// get): the child is killed suspended, and a marker the child would write on its first instruction is never written.
    /// </summary>
    [Fact]
    public void AJobClosedBeforeTheResume_MeansTheChildNeverRan()
    {
        var dir = TempDir();
        try
        {
            var marker = Path.Combine(dir, "ran.txt");
            File.WriteAllText(Path.Combine(dir, "run.cmd"), "@echo x> \"%~dp0ran.txt\"\r\n@exit /b 0\r\n");

            var closed = XpePreprocessOracleProcess.Run(Cmd, ["/c", Path.Combine(dir, "run.cmd")], TimeSpan.FromSeconds(30), new HostSeams(AfterAssign: (_, job) => job.Dispose()));
            Assert.False(closed.Passed);
            Assert.False(File.Exists(marker), "the child ran before it was in the job");

            // control: the same child with nothing closed does run and write its marker
            var open = XpePreprocessOracleProcess.Run(Cmd, ["/c", Path.Combine(dir, "run.cmd")], TimeSpan.FromSeconds(30));
            Assert.Equal("Oracle process gave no result", open.Status);
            Assert.True(File.Exists(marker), "control: the marker is what a running child writes");
        }
        finally
        {
            Cleanup(dir);
        }
    }

    /// <summary>Quoting is what makes a path with spaces or quotes reach the child as one argument; the system's own parser (CommandLineToArgvW) reads it back.</summary>
    [Theory]
    [InlineData("plain")]
    [InlineData("with space")]
    [InlineData("a\"quote")]
    [InlineData("trailing backslash\\")]
    [InlineData("two trailing\\\\")]
    [InlineData("C:\\a b\\c d\\x.cmd")]
    [InlineData("")]
    [InlineData("한글 경로")]
    public void Quote_IsReadBackExactly_ByTheSystemsOwnParser(string argument)
    {
        var line = "prog.exe " + SuspendedChild.Quote(argument) + " tail";

        var parsed = CommandLineToArgv(line);

        Assert.Equal(["prog.exe", argument, "tail"], parsed);
    }

    private static string[] CommandLineToArgv(string line)
    {
        var argv = CommandLineToArgvW(line, out var count);
        try
        {
            return Enumerable.Range(0, count).Select(i => Marshal.PtrToStringUni(Marshal.ReadIntPtr(argv, i * IntPtr.Size))!).ToArray();
        }
        finally
        {
            LocalFree(argv);
        }
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr CommandLineToArgvW(string commandLine, out int count);

    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr memory);

    // ---- 3: bounded output

    [Fact]
    public void Control_AnOrdinaryChild_WithinTheLimits_IsStillRead()
    {
        var line = Encoding.UTF8.GetBytes("a log line\n" + XpePreprocessOracleProcess.FormatResultLine(Good()) + "\n");

        var result = RunBytes(line);

        Assert.True(result.Passed, $"{result.Status}: {result.Details}");
    }

    [Fact]
    public void ALargeOutputWithoutNewlines_IsStoppedAtTheStdoutLimit_AndReportedAsTooLarge()
    {
        var result = RunBytes(Repeat("noise ", 5 * 1024 * 1024));

        Assert.False(result.Passed);
        Assert.Equal("Oracle process output too large", result.Status);
        Assert.Contains("standard output exceeded", result.Details);
    }

    [Fact]
    public void AResultLineThatNeverEnds_IsStoppedAtTheLineLimit_AndTheParentsMemoryStaysSmall()
    {
        var bytes = Repeat(XpePreprocessOracleProcess.ResultPrefix, 32 * 1024 * 1024);   // 32 MiB on ONE protocol line: the 64 KiB line limit must stop it long before the 1 MiB total
        var before = GC.GetTotalAllocatedBytes(precise: false);

        var result = RunBytes(bytes);

        var allocated = GC.GetTotalAllocatedBytes(precise: false) - before;
        Assert.False(result.Passed);
        Assert.Equal("Oracle process output too large", result.Status);
        Assert.Contains("a result line exceeded", result.Details);
        Assert.True(allocated < 24L * 1024 * 1024, $"the parent allocated {allocated / 1024.0 / 1024.0:F1} MiB while a child printed a 32 MiB line: the output was buffered, not streamed");
    }

    [Fact]
    public void ALargeStandardError_IsStoppedAtTheStderrLimit()
    {
        var result = RunBytes(Repeat("e", 600 * 1024), toStderr: true);

        Assert.False(result.Passed);
        Assert.Equal("Oracle process output too large", result.Status);
        Assert.Contains("standard error exceeded", result.Details);
    }

    [Fact]
    public void ALimitWithinWhichTheOutputFits_IsNotAFailure_AndTheSameOutputOverAnotherLimitIs()
    {
        var output = Encoding.UTF8.GetBytes(new string('n', 3000) + "\n" + XpePreprocessOracleProcess.FormatResultLine(Good()) + "\n");

        var fits = RunBytes(output, limits: new OutputLimits(MaxResultLineBytes: 16 * 1024, MaxStdoutBytes: 16 * 1024, MaxStderrBytes: 1024));
        var tooSmall = RunBytes(output, limits: new OutputLimits(MaxResultLineBytes: 16 * 1024, MaxStdoutBytes: 2048, MaxStderrBytes: 1024));
        var lineTooSmall = RunBytes(output, limits: new OutputLimits(MaxResultLineBytes: 100, MaxStdoutBytes: 16 * 1024, MaxStderrBytes: 1024));

        Assert.True(fits.Passed, $"{fits.Status}: {fits.Details}");
        Assert.Equal("Oracle process output too large", tooSmall.Status);
        Assert.Equal("Oracle process output too large", lineTooSmall.Status);
        Assert.Contains("a result line exceeded 100", lineTooSmall.Details);
    }

    // ---- the protocol is UTF-8

    [Fact]
    public void AResultWithNonAsciiText_IsReadAsUtf8_WhateverTheConsoleCodePageIs()
    {
        var withKorean = Good() with { Details = "한글 상세 — ok" };
        var raw = JsonSerializer.Serialize(withKorean, new JsonSerializerOptions { NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals, Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping });
        Assert.Contains("한글", raw);   // control: the line really carries raw non-ASCII characters, not \u escapes

        var result = RunBytes(new UTF8Encoding(false).GetBytes(XpePreprocessOracleProcess.ResultPrefix + raw + "\n"));

        Assert.True(result.Passed, $"{result.Status}: {result.Details}");
        Assert.Equal("한글 상세 — ok", result.Details);
    }

    [Fact]
    public void AResultLineThatIsNotValidUtf8_IsAFailure_NotAGuess()
    {
        var bytes = Encoding.ASCII.GetBytes(XpePreprocessOracleProcess.ResultPrefix + "{\"Status\":\"").Concat(new byte[] { 0xFF, 0xFE }).Concat(Encoding.ASCII.GetBytes("\"}\n")).ToArray();

        var result = RunBytes(bytes);

        Assert.False(result.Passed);
        Assert.Equal("Oracle process result invalid", result.Status);   // the reader stops and kills the child on undecodable protocol bytes
        Assert.Contains("not valid UTF-8", result.Details);
    }
}
