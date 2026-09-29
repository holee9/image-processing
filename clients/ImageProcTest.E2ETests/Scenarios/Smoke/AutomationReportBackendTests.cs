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
    /// A-04 (#225, GUI-C-159): row 15 — the self-check actually RUNS and the app reports the verdict.
    ///
    /// <para>GUI-C-158 wired the command and put <c>SelfCheckPassed</c> in the report, but nothing
    /// asserted it — the shape GUI-C-155 was corrected for. Asserted on both backends: the runner drives
    /// the mock display on purpose (its own assertion is <c>v0.0.0-mock-display</c>), so its verdict does
    /// not depend on whether native libraries are staged, and there is no precondition to skip on.</para>
    /// </summary>
    [SkippableFact]
    public void A04_SelfCheck_ActuallyRuns_AndThePassIsReported()
    {
        var (report, _) = Run("A04", "Mock", nativeDirectory: null);

        var status = report.GetProperty("SelfCheckStatus").GetString() ?? string.Empty;
        var verdict = VerdictOrSkip(report, "SelfCheckPassed", "self-check", "ImageProcTest.SelfCheck", status);
        Assert.True(verdict,
            $"The self-check did not pass: '{status}'.");
        Assert.Contains("passed", status, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// A-05 (#225, GUI-C-159): row 15 — and a FAILING self-check is reported as a failure, WITH the
    /// reason.
    ///
    /// <para>A-04 alone cannot separate "reports the verdict" from "reports everything as a pass", which
    /// is the shape #205, #207 and #212 each turned out to be. The failure is produced structurally
    /// rather than by breaking the runner's source: a copy of the runner staged outside the repository
    /// cannot locate the repository root, which GUI-C-157 measured it dying on. The app is pointed at
    /// that copy through <c>--automation-selfcheck-exe</c>.</para>
    /// </summary>
    [SkippableFact]
    public void A05_AFailingSelfCheck_IsReportedAsAFailure_WithTheReason()
    {
        var staged = StageSelfCheckRunnerOutsideTheRepository("A05");

        var (report, _) = Run("A05", "Mock", nativeDirectory: null,
            extraArgs: ["--automation-selfcheck-exe", staged]);

        var status = report.GetProperty("SelfCheckStatus").GetString() ?? string.Empty;
        Assert.False(report.GetProperty("SelfCheckPassed").GetBoolean(),
            $"The self-check was staged where it cannot run, and the app still reported a pass: '{status}'.");
        // The reason, not just the verdict: GUI-C-158 first reported the LAST stderr line ("at
        // Program...line 62"), which is true and tells the operator nothing.
        Assert.Contains("Repository root", status, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// A-06 (#225, GUI-C-159): row 16 — the GUI E2E runner actually runs from the Tools menu and the
    /// app reports its verdict.
    ///
    /// <para>This is also the concurrency observation the card asked for, standing rather than one-off:
    /// the runner is launched by an app that is already up, so a regression where two windows interfere
    /// shows up here as a failed or never-finishing run rather than only in a report someone read once.
    /// Backend-independent for the same reason A-04 is — the runner drives the mock display.</para>
    /// </summary>
    [SkippableFact]
    public void A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported()
    {
        var (report, _) = Run("A06", "Mock", nativeDirectory: null);

        var status = report.GetProperty("GuiE2EStatus").GetString() ?? string.Empty;
        var verdict = VerdictOrSkip(report, "GuiE2EPassed", "GUI E2E", "ImageProcTest.E2E", status);
        Assert.True(verdict,
            $"The GUI E2E runner did not pass when launched from the app: '{status}'.");
        Assert.Contains("passed", status, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Copies the built self-check runner into a temporary directory OUTSIDE the checkout and returns
    /// the copied executable. Nothing in the repository is modified.
    /// </summary>
    private static string StageSelfCheckRunnerOutsideTheRepository(string scenario)
    {
        var relative = Path.Combine(
            "gui", "ImageProcTest.SelfCheck", "bin", "Debug", "net8.0-windows");

        string? source = null;
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, relative);
            if (File.Exists(Path.Combine(candidate, "ImageProcTest.SelfCheck.exe")))
            {
                source = candidate;
                break;
            }
        }

        Skip.If(source is null, "ImageProcTest.SelfCheck.exe was not built, so its failure cannot be observed.");

        var target = Path.Combine(Path.GetTempPath(), $"xpe-selfcheck-{scenario}-{Environment.ProcessId}");
        if (Directory.Exists(target)) Directory.Delete(target, recursive: true);
        Directory.CreateDirectory(target);
        foreach (var file in Directory.GetFiles(source!))
        {
            File.Copy(file, Path.Combine(target, Path.GetFileName(file)), overwrite: true);
        }

        return Path.Combine(target, "ImageProcTest.SelfCheck.exe");
    }

    /// <summary>
    /// Reads a runner verdict that the app reports as THREE states, and keeps them three (#225,
    /// GUI-C-161).
    ///
    /// <para><c>null</c> is "the command did not run", not "it failed": the app leaves the field null
    /// when the runner executable is absent, exactly as <c>RunConsoleRunnerAsync</c> returns null rather
    /// than false for a runner it could not reach. Reading it with <c>GetBoolean()</c> collapsed that
    /// third state into an exception and turned main red — the test had erased a distinction the product
    /// code makes. The same defect class as #219 and #221.</para>
    ///
    /// <para>The absent-runner configuration is NORMAL, not broken: the <c>gui-automation</c> CI job
    /// builds <c>gui/ImageProcTest</c> only, so no runner exe exists beside the app there. That
    /// configuration skips with the reason stated; a configuration that HAS the runner asserts. Passing
    /// it loosely instead would make "the runner is absent" and "the runner failed" the same green
    /// (#214, GUI-C-155).</para>
    /// </summary>
    private static bool VerdictOrSkip(
        JsonElement report, string property, string label, string runnerProject, string status)
    {
        var present = report.TryGetProperty(property, out var value) &&
                      value.ValueKind is JsonValueKind.True or JsonValueKind.False;

        Skip.IfNot(present,
            $"The app reported no {label} verdict ({property} is null), which means the command did not " +
            $"run: this configuration has no {runnerProject} executable beside the app — the " +
            "gui-automation CI job builds gui/ImageProcTest only, which is normal. The app's status " +
            $"line said: '{status}'.");

        return value.GetBoolean();
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

    private (JsonElement Report, int ExitCode) Run(
        string scenario, string backend, string? nativeDirectory, string[]? extraArgs = null)
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

        foreach (var arg in extraArgs ?? [])
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
