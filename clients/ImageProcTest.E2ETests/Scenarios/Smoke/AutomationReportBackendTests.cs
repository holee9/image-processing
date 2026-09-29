// #175 (GUI-C-83): the self-driving automation run must not pass on a backend it was not asked to use.
using System.Diagnostics;
using System.Text.Json;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// Launches the app the way the <c>gui-automation</c> CI job does (<c>--automation-report</c>, hidden)
/// and reads the report it writes.
///
/// <para>GUI-C-82 measured <c>Passed=True</c> with <c>BackendMode=Native ActualBackendMode=Mock</c>: the
/// CI gate reads <c>Passed</c>, so a Native automation run that fell back would have been green. A-01 is
/// that case; A-02 is the control — an intentional Mock run still passes, so a rule that fails every
/// Mock run cannot pass here.</para>
/// </summary>
[Collection(AutomationReportCollection.Name)]
public sealed class AutomationReportBackendTests(ITestOutputHelper output)
{
    private const string RawRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    [SkippableFact]
    public void A01_NativeRequested_FallsBackToMock_IsNotAPass()
    {
        var empty = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), $"xpe-e2e-empty-native-a01-{Environment.ProcessId}"));
        foreach (var file in empty.GetFiles()) file.Delete();

        var (report, exitCode) = Run("A01", "Native", empty.FullName);
        Assert.Equal("Native", report.GetProperty("BackendMode").GetString());
        Assert.Equal("Mock", report.GetProperty("ActualBackendMode").GetString());
        Assert.False(report.GetProperty("BackendMatchesRequest").GetBoolean(), "The report does not flag the substituted backend.");
        Assert.False(
            report.GetProperty("Passed").GetBoolean(),
            "Native was requested, Mock ran, and the automation report still says Passed=true (#175).");
        Assert.True(exitCode == 1, $"A failed automation run exited {exitCode}; a caller reading only the exit code would take it for a pass (GUI-C-84).");
    }

    [SkippableFact]
    public void A02_MockRequested_IsStillAPass()
    {
        var (report, exitCode) = Run("A02", "Mock", nativeDirectory: null);
        Assert.Equal("Mock", report.GetProperty("ActualBackendMode").GetString());
        Assert.True(report.GetProperty("BackendMatchesRequest").GetBoolean());
        Assert.True(report.GetProperty("Passed").GetBoolean(), $"An intentional Mock automation run failed: Error='{report.GetProperty("Error")}'.");
        Assert.True(exitCode == 0, $"A passing automation run exited {exitCode}.");
    }

    /// <summary>
    /// A-03 (#225, GUI-C-155): the three commands GUI-C-154 wired report what they did, and the smoke
    /// answers from the real libraries.
    ///
    /// <para><b>Why here rather than through the menus.</b> The stop case has to catch a render that is
    /// still running, and the render is tens of milliseconds — two attempts at a menu-driven version
    /// lost that race (GUI-C-155). The automation run drives the same commands in-process, where the
    /// cancellation is issued before the render's first await, so the observation is deterministic.</para>
    /// </summary>
    [SkippableFact]
    public void A03_TheWiredCommands_ReportWhatTheyDid()
    {
        var (report, _) = Run("A03", "Native", NativeDirectoryOrSkip());

        // Row 6: answered, and from the real library — the mock display reports v0.0.0-mock-display, so
        // asserting only "passed" would not separate a fallback from a native run.
        Assert.True(report.GetProperty("PInvokeSmokeTestPassed").GetBoolean(),
            $"The smoke failed on a run with the native DLLs staged: {report.GetProperty("PInvokeSmokeTestDetail")}");
        var detail = report.GetProperty("PInvokeSmokeTestDetail").GetString() ?? string.Empty;
        Assert.Contains("xpe_display_version", detail, StringComparison.Ordinal);
        Assert.DoesNotContain("mock-display", detail, StringComparison.OrdinalIgnoreCase);

        // Row 11, in flight: the render was discarded, and the app counted it.
        var stopped = report.GetProperty("StopInFlightStatus").GetString() ?? string.Empty;
        Assert.StartsWith("Render stopped", stopped, StringComparison.Ordinal);
        Assert.True(report.GetProperty("StoppedRenderCount").GetInt32() >= 1,
            $"Stop was pressed during a render and the count stayed at {report.GetProperty("StoppedRenderCount")}.");

        // Row 11, empty: the case GUI-C-154's first implementation got wrong — it answered "the render
        // in flight will be discarded" on a render that had finished. Without this line it comes back.
        Assert.Equal("Stop: no render is in flight.", report.GetProperty("StopWithNothingRunningStatus").GetString());

        // Row 12: what it shows is the render's own chain line, not a second measurement.
        var timing = report.GetProperty("StageTimingReport").GetString() ?? string.Empty;
        Assert.Contains("chain:", timing, StringComparison.Ordinal);
        Assert.DoesNotContain("no render has run yet", timing, StringComparison.Ordinal);
    }

    /// <summary>
    /// The staged native directory, or a skip. Row 6 asserts the libraries ANSWER, so a run without
    /// them cannot observe it — and passing it anyway would make "the DLLs are absent" and "the DLLs
    /// are broken" the same green (#214). ci.yml:454-455 says the Mock job has no native DLLs.
    /// </summary>
    private static string NativeDirectoryOrSkip()
    {
        var dir = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
        Skip.If(string.IsNullOrWhiteSpace(dir) || !Directory.Exists(dir),
            $"{ApplicationFixture.NativeDirVariable} is not set to an existing directory, so no native " +
            "libraries are staged for this run and the smoke cannot be observed answering.");
        return dir!;
    }

    private (JsonElement Report, int ExitCode) Run(string scenario, string backend, string? nativeDirectory)
    {
        var exe = ApplicationFixture.ResolveApplicationExecutable();
        Skip.If(exe is null, "ImageProcTest.exe was not built.");
        var workDir = Path.GetDirectoryName(exe)!;
        Skip.If(!File.Exists(Path.Combine(workDir, RawRelativePath)), "The fixture image is not staged next to the app.");

        var reportPath = Path.Combine(Path.GetTempPath(), $"xpe-automation-{scenario}-{Environment.ProcessId}.json");
        File.Delete(reportPath);

        var start = new ProcessStartInfo(exe!) { WorkingDirectory = workDir, UseShellExecute = false, WindowStyle = ProcessWindowStyle.Hidden };
        foreach (var arg in new[]
                 {
                     "--automation-raw", RawRelativePath, "--automation-report", reportPath,
                     "--automation-backend", backend, "--automation-width", "1024", "--automation-height", "1024",
                 })
        {
            start.ArgumentList.Add(arg);
        }

        if (nativeDirectory is not null)
        {
            start.Environment[ApplicationFixture.NativeDirVariable] = nativeDirectory;
            start.Environment["XPE_NATIVE_DIR_EXCLUSIVE"] = "1";
        }

        using var process = Process.Start(start)!;
        if (!process.WaitForExit(120_000))
        {
            process.Kill(entireProcessTree: true);
            Assert.Fail($"{scenario}: the automation run did not finish within 120 s.");
        }

        Assert.True(File.Exists(reportPath), $"{scenario}: no report was written (exit {process.ExitCode}).");
        using var doc = JsonDocument.Parse(File.ReadAllText(reportPath));
        var root = doc.RootElement.Clone();
        output.WriteLine($"{scenario} exit={process.ExitCode} BackendMode={root.GetProperty("BackendMode")} " +
                         $"ActualBackendMode={root.GetProperty("ActualBackendMode")} " +
                         $"BackendMatchesRequest={root.GetProperty("BackendMatchesRequest")} Passed={root.GetProperty("Passed")}");
        return (root, process.ExitCode);
    }
}

/// <summary>Its own collection: these tests launch their own app processes.</summary>
[CollectionDefinition(Name)]
public sealed class AutomationReportCollection
{
    public const string Name = "AutomationReport";
}
