// #225 row 9 (GUI-C-196 M5): the leader's ruling on the automation report — a failed baseline fails the report, a baseline that was not run leaves it alone.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class BaselineAutomationRuleTests
{
    private static BaselineExecutionResult Result(bool passed)
    {
        var verdict = new BaselineVerdict(passed ? BaselineStatus.Pass : BaselineStatus.Fail, passed ? string.Empty : "x", 2, true, passed, null, 0, "a", "a", "o", [], []);
        return new BaselineExecutionResult(passed, passed ? "PASS" : "FAIL", verdict, "f", "f/baseline.json", string.Empty, string.Empty, 1, null, passed, passed, string.Empty);
    }

    [Fact]
    public void AResultThatPassed_IsPass_AndAResultThatFailed_IsFail()
    {
        Assert.Equal("Pass", BaselineAutomationRule.StatusOf(attempted: true, Result(true)));
        Assert.Equal("Fail", BaselineAutomationRule.StatusOf(attempted: true, Result(false)));
    }

    [Fact]
    public void NotAttempted_IsNotRun_NeverAPassOrAFail()
    {
        Assert.Equal("NotRun", BaselineAutomationRule.StatusOf(attempted: false, null));
    }

    [Fact]
    public void TriedWithNoResult_IsFail_NotNotRun()
    {
        // it threw, its result was dropped, or it did not finish in time: "nothing happened" would hide a hang or a crash
        Assert.Equal("Fail", BaselineAutomationRule.StatusOf(attempted: true, null));
    }

    [Theory]
    [InlineData("Pass", true)]
    [InlineData("NotRun", true)]
    [InlineData("Fail", false)]
    [InlineData("pass", false)]      // case matters: the report writes exactly these three words
    [InlineData("", false)]
    [InlineData("Unknown", false)]
    [InlineData(null, false)]
    public void OnlyPassAndNotRunLetTheReportPass(string? status, bool allowed)
    {
        Assert.Equal(allowed, BaselineAutomationRule.AllowsAutomationPass(status));
    }

    [Fact]
    public void TheReportVerdict_UsesTheRule_AndTheStatusComesFromIt()
    {
        var code = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/MainWindow.xaml.cs")).Replace("\r\n", "\n");

        var passedAt = code.IndexOf("report.Passed =\n", StringComparison.Ordinal);
        Assert.True(passedAt >= 0);
        var verdict = code[passedAt..code.IndexOf(";\n", passedAt, StringComparison.Ordinal)];
        Assert.Contains("BaselineAutomationRule.AllowsAutomationPass(report.BaselineStatus)", verdict, StringComparison.Ordinal);

        Assert.Contains("report.BaselineStatus = BaselineAutomationRule.StatusOf(baselineAttempted, baseline);", code, StringComparison.Ordinal);
        // the baseline step sits BEFORE the verdict is computed, so the verdict sees it
        Assert.True(code.IndexOf("BaselineAutomationRule.StatusOf(", StringComparison.Ordinal) < passedAt, "the baseline step must run before report.Passed is computed");
        // and the wait ends on the command's own failure line, so a command that threw is not waited on for a minute
        Assert.Contains("viewModel.LastBaselineResult is null && !viewModel.BaselineStatusText.StartsWith(\"Deterministic Baseline FAIL\"", code, StringComparison.Ordinal);
    }
}
