// #225 (GUI-C-190b): the wording of a runner scenario without a verdict is pinned per cause, and tied to the app's own status lines.
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// No app is launched here: the classification reads the status line the app leaves, so the samples are those lines. A drift between a
/// sample and the app's real text is caught by <see cref="TheSamplesAreTheAppsOwnStatusLines"/>, which looks for each phrase in the
/// app's source (a source reading: it sees this tree's text, not a running app).
/// </summary>
public sealed class RunnerVerdictWordingTests
{
    [Theory]
    [InlineData("Self-check runner is not built.")]
    [InlineData("Self-check needs the repository; this build is not running from a checkout.")]
    public void NoExecutable_IsSaidAsNoExecutable_AndNeverAsATimeout(string status)
    {
        Assert.Equal(RunnerVerdictGap.NoExecutable, RunnerVerdictWording.Classify(status));
        var text = RunnerVerdictWording.NoVerdict("self-check", "ImageProcTest.SelfCheck", status);
        Assert.StartsWith("NO EXECUTABLE:", text, StringComparison.Ordinal);
        Assert.DoesNotContain("DID NOT FINISH", text, StringComparison.Ordinal);
        Assert.Contains(status, text, StringComparison.Ordinal);
    }

    [Fact]
    public void ARunnerStillRunning_IsSaidAsNotAMissingExecutable_WhichIsTheCaseThatWasMisread()
    {
        const string status = "Running Self-check…";   // the status line of the 17 s runner the app stopped waiting for at 15 s
        Assert.Equal(RunnerVerdictGap.StillRunning, RunnerVerdictWording.Classify(status));
        var text = RunnerVerdictWording.NoVerdict("self-check", "ImageProcTest.SelfCheck", status);
        Assert.StartsWith("NOT A MISSING EXECUTABLE - DID NOT FINISH IN TIME:", text, StringComparison.Ordinal);
        Assert.DoesNotContain("there is no ImageProcTest.SelfCheck executable", text, StringComparison.Ordinal);
        Assert.Contains(status, text, StringComparison.Ordinal);
    }

    [Fact]
    public void AnExecutableThatCouldNotBeStarted_IsSaidAsSuch()
    {
        const string status = "Self-check did not run: 'C:\\x\\ImageProcTest.SelfCheck.exe' could not be started (the file was not found). Nothing was executed.";
        Assert.Equal(RunnerVerdictGap.DidNotStart, RunnerVerdictWording.Classify(status));
        Assert.StartsWith("COULD NOT START:", RunnerVerdictWording.NoVerdict("self-check", "ImageProcTest.SelfCheck", status), StringComparison.Ordinal);
    }

    [Fact]
    public void AStatusNoOneRecognises_IsSaidAsUnknown_NotAsAnyOfTheOthers()
    {
        const string status = "Self-check ended abnormally: something else.";
        Assert.Equal(RunnerVerdictGap.Unexplained, RunnerVerdictWording.Classify(status));
        Assert.StartsWith("NO VERDICT, CAUSE UNKNOWN:", RunnerVerdictWording.NoVerdict("self-check", "ImageProcTest.SelfCheck", status), StringComparison.Ordinal);
    }

    [Fact]
    public void ARunnerThatRanAndFailed_IsSaidAsFailed_AndCarriesTheStatus()
    {
        const string status = "Self-check FAILED (exit 1): Assertion failed.";
        var text = RunnerVerdictWording.Failed("self-check", status);
        Assert.Contains("RAN and FAILED", text, StringComparison.Ordinal);
        Assert.Contains(status, text, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData(RunnerVerdictGap.NoExecutable, true)]
    [InlineData(RunnerVerdictGap.StillRunning, false)]
    [InlineData(RunnerVerdictGap.DidNotStart, false)]
    [InlineData(RunnerVerdictGap.Unexplained, false)]
    public void OnlyAMissingExecutableSkips_EveryOtherGapFails(RunnerVerdictGap gap, bool skips)
    {
        Assert.Equal(skips, RunnerVerdictWording.SkipsInsteadOfFailing(gap));
    }

    /// <summary>The scenarios' own gate (VerdictOrSkip) applies that rule, and throws with the quoted status line (a source reading).</summary>
    [Fact]
    public void VerdictOrSkip_SkipsOnlyThroughTheRule_AndOtherwiseThrowsTheReason()
    {
        string? source = null;
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "clients", "ImageProcTest.E2ETests", "Scenarios", "Smoke", "AutomationReportBackendTests.cs");
            if (File.Exists(candidate))
            {
                source = File.ReadAllText(candidate);
                break;
            }
        }

        Assert.True(source is not null, "AutomationReportBackendTests.cs was not found above the test output directory.");
        var start = source!.IndexOf("private static bool VerdictOrSkip(", StringComparison.Ordinal);
        var end = source.IndexOf("The staged native directory, or a skip.", start, StringComparison.Ordinal);
        Assert.True(start >= 0 && end > start, "VerdictOrSkip was not found.");
        var body = source[start..end];

        Assert.Contains("Skip.If(RunnerVerdictWording.SkipsInsteadOfFailing(gap), reason);", body, StringComparison.Ordinal);
        Assert.Contains("throw new Xunit.Sdk.XunitException(reason);", body, StringComparison.Ordinal);
        Assert.DoesNotContain("Skip.IfNot(present", body, StringComparison.Ordinal);
    }

    [Fact]
    public void TheFourCauses_AreFourDifferentTexts()
    {
        var texts = new[]
        {
            RunnerVerdictWording.NoVerdict("self-check", "p", "Self-check runner is not built."),
            RunnerVerdictWording.NoVerdict("self-check", "p", "Running Self-check…"),
            RunnerVerdictWording.NoVerdict("self-check", "p", "Self-check did not run: x"),
            RunnerVerdictWording.NoVerdict("self-check", "p", "other"),
        };

        Assert.Equal(4, texts.Select(text => text[..text.IndexOf(':')]).Distinct().Count());
    }

    /// <summary>The phrases the classification keys on are the phrases the app writes (a source reading).</summary>
    [Fact]
    public void TheSamplesAreTheAppsOwnStatusLines()
    {
        string? source = null;
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "gui", "ImageProcTest", "ViewModels", "MainWindowViewModel.cs");
            if (File.Exists(candidate))
            {
                source = File.ReadAllText(candidate);
                break;
            }
        }

        Assert.True(source is not null, "MainWindowViewModel.cs was not found above the test output directory.");
        Assert.Contains("runner is not built.", source, StringComparison.Ordinal);
        Assert.Contains("needs the repository; this build is not running from a checkout.", source, StringComparison.Ordinal);
        Assert.Contains("StatusText = $\"Running {label}…\";", source, StringComparison.Ordinal);

        string? describe = null;
        for (var dir = new DirectoryInfo(AppContext.BaseDirectory); dir is not null; dir = dir.Parent)
        {
            var candidate = Path.Combine(dir.FullName, "gui", "ImageProcTest", "Services", "RunnerProcess.cs");
            if (File.Exists(candidate))
            {
                describe = File.ReadAllText(candidate);
                break;
            }
        }

        Assert.True(describe is not null, "RunnerProcess.cs was not found above the test output directory.");
        Assert.Contains("{label} did not run:", describe, StringComparison.Ordinal);
    }
}
