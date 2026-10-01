// #225 rows 15, 16, 17 (GUI-C-177): "could not be started" and "ran and failed" are different things, and the one
// runner process executor says so in different words for all three menu commands.
using System.Text.RegularExpressions;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Real processes, not stubs: a path that is not there, a file that is not an executable, a program that exits 3
/// and one that exits 0 — each observed through <see cref="RunnerProcess.Run"/> — then the wording for each of the
/// three commands' labels, then the coupling that makes the wording the commands' own (they reach one describer, and
/// no other place in the view model words a runner outcome), with the survey itself tested on mutated copies.
/// </summary>
[Trait("Category", "Functional")]
public sealed class RunnerProcessTests : IDisposable
{
    private const string ViewModelPath = "gui/ImageProcTest/ViewModels/MainWindowViewModel.cs";
    private const string RunnerProcessPath = "gui/ImageProcTest/Services/RunnerProcess.cs";

    /// <summary>The labels the three commands pass: row 15, row 16 and row 17.</summary>
    private static readonly string[] CommandLabels = { "Self-check", "GUI E2E", "Benchmark runner" };

    private readonly string _root = Path.Combine(Path.GetTempPath(), $"xpe-runner-{Guid.NewGuid():N}");

    public RunnerProcessTests() => Directory.CreateDirectory(_root);

    public void Dispose()
    {
        if (Directory.Exists(_root)) Directory.Delete(_root, recursive: true);
    }

    // -- the three outcomes, from real processes --------------------------------------------------------

    [Fact]
    public void AMissingExecutable_DidNotStart_AndHasNoVerdict()
    {
        var outcome = RunnerProcess.Run(Path.Combine(_root, "not-there.exe"));

        Assert.False(outcome.Started);
        Assert.Null(outcome.Verdict);
        Assert.False(string.IsNullOrWhiteSpace(outcome.StartError));
    }

    [Fact]
    public void ABareNameThatIsNotOnPath_DidNotStart_AndTheReasonSaysItHasToBeOnPath()
    {
        // The row 17 case: "ctest" is looked up on PATH, and the operator who launched the app outside a developer
        // prompt has to be told that, not handed a raw OS message.
        var outcome = RunnerProcess.Run("xpe-no-such-program-gui-c-177");

        Assert.False(outcome.Started);
        Assert.Null(outcome.Verdict);
        Assert.Contains("has to be on PATH", outcome.StartError, StringComparison.Ordinal);
    }

    [SkippableFact]
    public void AFileThatIsNotAnExecutable_DidNotStart_AndHasNoVerdict()
    {
        Skip.IfNot(OperatingSystem.IsWindows(), "Starting a text file as a program fails this way on Windows.");
        var notAProgram = Path.Combine(_root, "text.exe");
        File.WriteAllText(notAProgram, "this is text, not an executable");

        var outcome = RunnerProcess.Run(notAProgram);

        Assert.False(outcome.Started);
        Assert.Null(outcome.Verdict);
    }

    [SkippableFact]
    public void AProgramThatExitsNonZero_RanAndFailed_WithItsExitCode()
    {
        Skip.IfNot(OperatingSystem.IsWindows(), "cmd.exe is the program used to produce an exit code.");

        var outcome = RunnerProcess.Run(Cmd(), new[] { "/c", "exit", "3" });

        Assert.True(outcome.Started);
        Assert.Equal(3, outcome.ExitCode);
        Assert.False(outcome.Verdict);
        Assert.Null(outcome.StartError);
    }

    [SkippableFact]
    public void AProgramThatExitsZero_RanAndPassed()
    {
        Skip.IfNot(OperatingSystem.IsWindows(), "cmd.exe is the program used to produce an exit code.");

        var outcome = RunnerProcess.Run(Cmd(), new[] { "/c", "exit", "0" });

        Assert.True(outcome.Started);
        Assert.True(outcome.Verdict);
    }

    // -- the wording, for each command ------------------------------------------------------------------

    [Theory]
    [InlineData("Self-check", false)]
    [InlineData("GUI E2E", false)]
    [InlineData("Benchmark runner", true)]
    public void ForEachCommand_ACouldNotStartIsWordedDifferentlyFromARunThatFailed(string label, bool quotesSummary)
    {
        var notStarted = RunnerProcess.Describe(label, "ctest", RunnerOutcome.DidNotStart("the file was not found"), quotesSummary);
        var failed = RunnerProcess.Describe(label, "ctest", new RunnerOutcome(true, 3, "1 tests failed out of 2", 5, null), quotesSummary);

        Assert.NotEqual(notStarted, failed);
        Assert.Contains(label, notStarted, StringComparison.Ordinal);
        Assert.Contains(label, failed, StringComparison.Ordinal);

        Assert.Contains("did not run", notStarted, StringComparison.Ordinal);
        Assert.Contains("the file was not found", notStarted, StringComparison.Ordinal);
        Assert.DoesNotContain("FAILED", notStarted, StringComparison.Ordinal);

        Assert.Contains("FAILED (exit 3): 1 tests failed out of 2", failed, StringComparison.Ordinal);
        Assert.DoesNotContain("did not run", failed, StringComparison.Ordinal);
    }

    [Fact]
    public void AllThreeCommandLabels_AreCovered_ByTheTheoryAbove()
    {
        // The three labels the commands really pass, read from the view model, are the three the theory runs with.
        var vm = File.ReadAllText(ResolveRepositoryFile(ViewModelPath));

        foreach (var label in CommandLabels)
        {
            Assert.Contains($"\"{label}\"", vm, StringComparison.Ordinal);
        }

        Assert.Equal(CommandLabels.Length, typeof(RunnerProcessTests)
            .GetMethod(nameof(ForEachCommand_ACouldNotStartIsWordedDifferentlyFromARunThatFailed))!
            .GetCustomAttributes(typeof(InlineDataAttribute), false).Length);
    }

    [Fact]
    public void ThePassingLine_KeepsItsShortFormForTheRunnersAndQuotesTheSummaryForTheBenchmark()
    {
        var passed = new RunnerOutcome(true, 0, "100% tests passed, 0 tests failed out of 2", 12, null);

        Assert.Equal("Self-check passed in 12 ms.", RunnerProcess.Describe("Self-check", "x", passed, quotesSummary: false));
        Assert.Equal("Benchmark runner passed in 12 ms. Exit 0. 100% tests passed, 0 tests failed out of 2",
            RunnerProcess.Describe("Benchmark runner", "x", passed, quotesSummary: true));
    }

    // -- the coupling: one describer, no second wording -------------------------------------------------

    [Fact]
    public void TheViewModel_WordsARunnerOutcomeInOnePlaceOnly()
    {
        var problems = DescriberProblems(
            File.ReadAllText(ResolveRepositoryFile(ViewModelPath)),
            File.ReadAllText(ResolveRepositoryFile(RunnerProcessPath)));

        Assert.True(problems.Count == 0, "Runner wording is not in one place: " + string.Join("; ", problems));
    }

    [Fact]
    public void TheDescriberSurvey_SeesASecondWording_AndAnExecutorThatStopsUsingTheDescriber()
    {
        var vm = File.ReadAllText(ResolveRepositoryFile(ViewModelPath));
        var runner = File.ReadAllText(ResolveRepositoryFile(RunnerProcessPath));

        // A command that words its own failure.
        var ownWording = vm + "\n    private void Mine(int code) { StatusText = $\"Benchmark runner FAILED (exit {code})\"; }\n";
        Assert.NotEmpty(DescriberProblems(ownWording, runner));

        // An executor that no longer asks the describer.
        var noDescriber = vm.Replace("RunnerProcess.Describe(", "string.Concat(", StringComparison.Ordinal);
        Assert.NotEqual(vm, noDescriber);
        Assert.NotEmpty(DescriberProblems(noDescriber, runner));

        // A describer that has lost one of its two wordings.
        var collapsed = runner.Replace("did not run:", "FAILED (exit 0):", StringComparison.Ordinal);
        Assert.NotEqual(runner, collapsed);
        Assert.NotEmpty(DescriberProblems(vm, collapsed));
    }

    private static List<string> DescriberProblems(string viewModel, string runnerProcess)
    {
        var problems = new List<string>();

        var executor = BenchmarkRunnerServiceTests.MethodBody(viewModel, "ExecuteRunnerAsync");
        if (executor is null)
        {
            problems.Add("ExecuteRunnerAsync was not found");
        }
        else
        {
            if (!executor.Contains("RunnerProcess.Run(", StringComparison.Ordinal)) problems.Add("the executor does not call RunnerProcess.Run(");
            if (!executor.Contains("RunnerProcess.Describe(", StringComparison.Ordinal)) problems.Add("the executor does not call RunnerProcess.Describe(");
        }

        // No status assignment in the view model carries either wording itself: it all comes from the describer.
        foreach (Match m in Regex.Matches(viewModel, @"StatusText\s*=\s*\$?""[^""]*(FAILED \(exit|did not run:|could not be started)"))
        {
            problems.Add($"a status line in the view model words an outcome itself: {m.Value.Trim()}");
        }

        foreach (var wording in new[] { "FAILED (exit", "did not run:" })
        {
            var n = Regex.Matches(runnerProcess, Regex.Escape(wording)).Count;
            if (n != 1) problems.Add($"'{wording}' appears {n} times in RunnerProcess (expected exactly 1)");
        }

        return problems;
    }

    private static string Cmd() => Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe";

    private static string ResolveRepositoryFile(string relativePath) => BenchmarkRunnerServiceTests.ResolveRepositoryFile(relativePath);
}
