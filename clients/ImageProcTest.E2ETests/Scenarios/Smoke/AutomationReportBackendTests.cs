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
