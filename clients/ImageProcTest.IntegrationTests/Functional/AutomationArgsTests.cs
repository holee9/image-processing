// #136: the automation switches must be parsed and validated where a test can see it.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Regression for <see cref="AutomationArgs"/>.
///
/// GUI-C-26 added --automation-backend with no test at all, because the parsing sat inside the WPF
/// Application class. That was reported at the time; this is the guard. The switch decides WHICH
/// backend a run measures, so a silently-ignored value makes every report from that run misleading.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AutomationArgsTests
{
    /// <summary>Both accepted names parse, case-insensitively, and come back in canonical spelling.</summary>
    [Theory]
    [InlineData("Mock", "Mock")]
    [InlineData("Native", "Native")]
    [InlineData("native", "Native")]
    [InlineData("MOCK", "Mock")]
    public void BackendSwitch_AcceptsBothModes_AndNormalisesTheSpelling(string given, string expected)
    {
        var parsed = AutomationArgs.Parse(["--automation-backend", given]);

        Assert.True(parsed.IsValid, parsed.Error);
        Assert.Equal(expected, parsed.BackendMode);
    }

    /// <summary>
    /// A misspelling is refused, not skipped. Before this, 'Nativ' flowed into settings and
    /// XpeBackendFactory quietly fell back to Mock while the report still said the source was "arg".
    /// </summary>
    [Fact]
    public void BackendSwitch_RejectsAnUnknownValue_RatherThanFallingBackToMock()
    {
        var parsed = AutomationArgs.Parse(["--automation-backend", "Nativ"]);

        Assert.False(parsed.IsValid);
        Assert.Null(parsed.BackendMode);
        Assert.Contains("Nativ", parsed.Error);
        Assert.Contains("Mock | Native", parsed.Error);
    }

    /// <summary>No backend switch is not an error — the settings file decides, as before.</summary>
    [Fact]
    public void NoBackendSwitch_LeavesTheDecisionToTheSettingsFile()
    {
        var parsed = AutomationArgs.Parse(["--automation-raw", "frame.raw", "--automation-report", "r.json"]);

        Assert.True(parsed.IsValid, parsed.Error);
        Assert.Null(parsed.BackendMode);
        Assert.True(parsed.IsAutomationMode);
    }

    /// <summary>A switch given without its value is refused; it used to be skipped silently.</summary>
    [Fact]
    public void SwitchWithoutItsValue_IsRefused()
    {
        var parsed = AutomationArgs.Parse(["--automation-raw", "frame.raw", "--automation-backend"]);

        Assert.False(parsed.IsValid);
        Assert.Contains("--automation-backend", parsed.Error);
        Assert.Null(parsed.BackendMode);
    }

    /// <summary>
    /// A rejection keeps the report destination and drops everything else: the refusal has to be
    /// writable somewhere the caller named, and a half-applied command line must not start a run.
    /// </summary>
    [Fact]
    public void Rejection_KeepsTheReportPath_AndDropsTheRunParameters()
    {
        var parsed = AutomationArgs.Parse(
            ["--automation-report", "r.json", "--automation-raw", "frame.raw", "--automation-backend", "Nativ"]);

        Assert.False(parsed.IsValid);
        Assert.NotNull(parsed.ReportPath);
        Assert.Null(parsed.RawPath);
        Assert.False(parsed.IsAutomationMode);
    }

    /// <summary>
    /// #136: an --automation-* switch nobody recognises is refused. A typo in the SWITCH name is the
    /// same hazard as one in its value — the run starts and the option silently did nothing.
    /// </summary>
    [Fact]
    public void UnknownAutomationSwitch_IsRefused()
    {
        var parsed = AutomationArgs.Parse(["--automation-backends", "Mock"]);

        Assert.False(parsed.IsValid);
        Assert.Contains("--automation-backends", parsed.Error);
    }

    /// <summary>
    /// #141: --automation-calib names the directory holding the generated XCal set. It must be a
    /// RECOGNISED switch — GUI-C-28 made every unknown --automation-* an error, so adding a switch
    /// to the app without adding it here would make the app refuse its own command line.
    /// </summary>
    [Fact]
    public void CalibrationSwitch_IsRecognisedAndResolvedToAFullPath()
    {
        var parsed = AutomationArgs.Parse(["--automation-calib", "calib-set"]);

        Assert.True(parsed.IsValid, parsed.Error);
        Assert.NotNull(parsed.CalibrationDirectory);
        Assert.True(Path.IsPathRooted(parsed.CalibrationDirectory), parsed.CalibrationDirectory);
    }

    /// <summary>Arguments outside the --automation- namespace stay ignored — they are not ours.</summary>
    [Fact]
    public void NonAutomationArguments_AreStillIgnored()
    {
        var parsed = AutomationArgs.Parse(["--verbose", "somefile.raw", "--automation-backend", "Mock"]);

        Assert.True(parsed.IsValid, parsed.Error);
        Assert.Equal("Mock", parsed.BackendMode);
    }

    /// <summary>Non-integer dimensions are refused too — the same silent-skip shape.</summary>
    [Fact]
    public void NonIntegerDimension_IsRefused()
    {
        var parsed = AutomationArgs.Parse(["--automation-width", "1o24"]);

        Assert.False(parsed.IsValid);
        Assert.Contains("--automation-width", parsed.Error);
    }
}
