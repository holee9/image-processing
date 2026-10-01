// #225 row 17 (GUI-C-176): the Benchmark Runner menu runs CI's benchmark tests through the SAME runner the
// self-check and GUI E2E menus use, says "not built" instead of running nothing, and judges nothing itself.
using System.Text.RegularExpressions;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Three kinds of claim, each against something other than itself:
/// <list type="bullet">
/// <item>the answers (<c>Resolve</c>, <c>Summarize</c>) against a TEMPORARY directory and fixed ctest output;</item>
/// <item>the pattern against the workflow step it was copied from, read from the checkout;</item>
/// <item>the coupling — one process executor, reached by all three runner commands — against the view model's
/// source, with the check itself tested on mutated copies so it cannot pass by being blind.</item>
/// </list>
/// </summary>
[Trait("Category", "Functional")]
public sealed class BenchmarkRunnerServiceTests : IDisposable
{
    private const string ViewModelPath = "gui/ImageProcTest/ViewModels/MainWindowViewModel.cs";
    private const string WorkflowPath = ".github/workflows/benchmark-regression.yml";

    private readonly string _root = Path.Combine(Path.GetTempPath(), $"xpe-bench-{Guid.NewGuid():N}");

    public BenchmarkRunnerServiceTests() => Directory.CreateDirectory(_root);

    public void Dispose()
    {
        if (Directory.Exists(_root)) Directory.Delete(_root, recursive: true);
    }

    // -- the answers -------------------------------------------------------------------------------------

    [Fact]
    public void NoBuildTree_SaysNotBuiltAndHow_AndStartsNothing()
    {
        var plan = BenchmarkRunnerService.Resolve(_root);

        Assert.False(plan.IsReady);
        Assert.Null(plan.TestDirectory);
        Assert.Empty(plan.Arguments);
        Assert.Contains("not built", plan.Message, StringComparison.Ordinal);
        Assert.Contains(BenchmarkRunnerService.BuildHint, plan.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ADirectoryWithoutCTestFiles_IsNotABuildTree()
    {
        // build/ci-post can exist from a configure that failed, or from an unrelated tool: the marker is
        // the file ctest itself needs, not the directory.
        Directory.CreateDirectory(Path.Combine(_root, "build", "ci-post"));

        var plan = BenchmarkRunnerService.Resolve(_root);

        Assert.False(plan.IsReady);
        Assert.Contains("not built", plan.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ABuildTree_GetsCiArguments_AsAListWithThePatternAsOneElement()
    {
        var tree = Path.Combine(_root, "build", "ci-post");
        Directory.CreateDirectory(tree);
        File.WriteAllText(Path.Combine(tree, "CTestTestfile.cmake"), "# tests");

        var plan = BenchmarkRunnerService.Resolve(_root);

        Assert.True(plan.IsReady);
        Assert.Equal(tree, plan.TestDirectory);
        var args = plan.Arguments.ToList();
        Assert.Equal(tree, args[args.IndexOf("--test-dir") + 1]);
        Assert.Equal("RelWithDebInfo", args[args.IndexOf("--build-config") + 1]);
        // One element, with its '|' and '\.' intact: no shell is involved, so nothing may split or unescape it.
        Assert.Equal(BenchmarkRunnerService.TestPattern, args[args.IndexOf("-R") + 1]);
        Assert.Contains("--no-tests=error", args);
    }

    // The three outputs below were CAPTURED from ctest 3.31.6 on Windows, run with exactly the arguments
    // Resolve builds, against a throwaway CMake project with tests named like the benchmark ones (only the
    // build directory in the first two lines is shortened). Written by hand they would only prove that
    // Summarize reads what its author expected ctest to print. Two facts from the same captures the app
    // relies on: ctest exits 0 when the pattern selects nothing unless --no-tests=error is given (then 8, with
    // "No tests were found!!!" on STDERR), and "Errors while running CTest" is on stderr as well.
    private const string CapturedPass =
        "Internal ctest changing into directory: C:/x/build\n" +
        "Test project C:/x/build\n" +
        "    Start 1: BenchmarkFreeze.PassA\n" +
        "1/2 Test #1: BenchmarkFreeze.PassA ............   Passed    0.03 sec\n" +
        "    Start 2: BenchmarkFreeze.PassB\n" +
        "2/2 Test #2: BenchmarkFreeze.PassB ............   Passed    0.03 sec\n" +
        "\n" +
        "100% tests passed, 0 tests failed out of 2\n" +
        "\n" +
        "Total Test time (real) =   0.07 sec\n";

    private const string CapturedFail =
        "Internal ctest changing into directory: C:/x/build\n" +
        "Test project C:/x/build\n" +
        "    Start 1: BenchmarkFreeze.PassA\n" +
        "1/3 Test #1: BenchmarkFreeze.PassA ................................   Passed    0.03 sec\n" +
        "    Start 2: BenchmarkFreeze.PassB\n" +
        "2/3 Test #2: BenchmarkFreeze.PassB ................................   Passed    0.03 sec\n" +
        "    Start 3: FullPipelineE2E.PostProcess_3072x3072_Within3000ms\n" +
        "3/3 Test #3: FullPipelineE2E.PostProcess_3072x3072_Within3000ms ...***Failed    0.03 sec\n" +
        "\n" +
        "\n" +
        "67% tests passed, 1 tests failed out of 3\n" +
        "\n" +
        "Total Test time (real) =   0.10 sec\n" +
        "\n" +
        "The following tests FAILED:\n" +
        "\t  3 - FullPipelineE2E.PostProcess_3072x3072_Within3000ms (Failed)\n";

    private const string CapturedNoTestsError =
        "Internal ctest changing into directory: C:/x/build\n" +
        "Test project C:/x/build\n";

    [Fact]
    public void ThePassingSummary_IsCTestsOwnTotals_NotAJudgement()
    {
        Assert.Equal("100% tests passed, 0 tests failed out of 2 | Total Test time (real) =   0.07 sec",
            BenchmarkRunnerService.Summarize(CapturedPass));
    }

    [Fact]
    public void TheFailingSummary_NamesTheTestsThatFailed()
    {
        Assert.Equal(
            "67% tests passed, 1 tests failed out of 3 | Total Test time (real) =   0.10 sec | " +
            "The following tests FAILED: | 3 - FullPipelineE2E.PostProcess_3072x3072_Within3000ms (Failed)",
            BenchmarkRunnerService.Summarize(CapturedFail));
    }

    [Fact]
    public void ANoTestsError_HasNoSummaryOnStdout_SoTheSharedRunnerQuotesStderrsReason()
    {
        // With --no-tests=error ctest prints no totals at all: the reason ("No tests were found!!!") is on
        // stderr, which is where the shared runner's own line choice looks when there is no summary.
        Assert.Null(BenchmarkRunnerService.Summarize(CapturedNoTestsError));
        Assert.Null(BenchmarkRunnerService.Summarize(string.Empty));
    }

    // -- the pattern is CI's -----------------------------------------------------------------------------

    [Fact]
    public void ThePattern_IsTheOneTheBenchmarkWorkflowStepUses()
    {
        var workflow = File.ReadAllText(ResolveRepositoryFile(WorkflowPath));
        var step = workflow.IndexOf("Run BP-06~09 benchmark freeze GTests", StringComparison.Ordinal);
        Assert.True(step >= 0, "The workflow no longer has the step this runner copies its pattern from.");

        var match = Regex.Match(workflow[step..], @"\$pattern\s*=\s*'([^']*)'");
        Assert.True(match.Success, "The step has no single-quoted $pattern line to compare against.");

        Assert.Equal(match.Groups[1].Value, BenchmarkRunnerService.TestPattern);
    }

    [Fact]
    public void TheBuildHint_NamesThePresetTheWorkflowConfiguresAndBuilds()
    {
        var workflow = File.ReadAllText(ResolveRepositoryFile(WorkflowPath));

        Assert.Contains($"cmake --preset {BenchmarkRunnerService.BuildPreset}", workflow, StringComparison.Ordinal);
        Assert.Contains($"cmake --build --preset {BenchmarkRunnerService.BuildPreset} --parallel", workflow, StringComparison.Ordinal);
        Assert.Contains($"cmake --preset {BenchmarkRunnerService.BuildPreset}", BenchmarkRunnerService.BuildHint, StringComparison.Ordinal);
        Assert.Contains($"cmake --build --preset {BenchmarkRunnerService.BuildPreset} --parallel", BenchmarkRunnerService.BuildHint, StringComparison.Ordinal);

        // build/ci-post is the preset's binaryDir ("build/${presetName}"), not a second constant that can drift.
        var presets = File.ReadAllText(ResolveRepositoryFile("CMakePresets.json"));
        Assert.Matches(@"""name"":\s*""ci-post""[^}]*?""binaryDir"":\s*""\$\{sourceDir\}/build/\$\{presetName\}""", presets);
    }

    // -- the coupling: one executor ----------------------------------------------------------------------

    [Fact]
    public void AllThreeRunnerCommands_ReachTheSameProcessExecutor()
    {
        var problems = CouplingProblems(File.ReadAllText(ResolveRepositoryFile(ViewModelPath)));

        Assert.True(problems.Count == 0, "Runner coupling is broken: " + string.Join("; ", problems));
    }

    [Fact]
    public void TheCouplingCheck_SeesABenchmarkThatStartsItsOwnProcess()
    {
        // The check has to be able to fail. Two ways to reimplement, each applied to a copy of the real source.
        var source = File.ReadAllText(ResolveRepositoryFile(ViewModelPath));

        var ownProcess = source.Replace(
            "ExecuteRunnerAsync(\"Benchmark runner\"",
            "System.Diagnostics.Process.Start(\"Benchmark runner\"", StringComparison.Ordinal);
        Assert.NotEqual(source, ownProcess);
        Assert.NotEmpty(CouplingProblems(ownProcess));

        var secondExecutor = source + "\n    private static void Other() { var s = new System.Diagnostics.ProcessStartInfo(\"x\") { RedirectStandardOutput = true }; }\n";
        Assert.NotEmpty(CouplingProblems(secondExecutor));
    }

    private static List<string> CouplingProblems(string source)
    {
        var problems = new List<string>();

        // The three commands, and the method each must go through.
        foreach (var (command, callee) in new[]
                 {
                     ("RunSelfCheckAsync", "RunConsoleRunnerAsync("),
                     ("RunGuiE2EAsync", "RunConsoleRunnerAsync("),
                     ("RunBenchmarkAsync", "ExecuteRunnerAsync("),
                     ("RunConsoleRunnerAsync", "ExecuteRunnerAsync("),
                 })
        {
            var body = MethodBody(source, command);
            if (body is null) problems.Add($"{command} was not found");
            else if (!body.Contains(callee, StringComparison.Ordinal)) problems.Add($"{command} does not call {callee}");
        }

        // And nothing else in the file starts a redirected child process: exactly one place does.
        var starts = Regex.Matches(source, @"RedirectStandardOutput\s*=\s*true").Count;
        if (starts != 1) problems.Add($"{starts} places start a redirected process (expected exactly 1: RunProcess)");

        var runProcess = MethodBody(source, "RunProcess");
        if (runProcess is null || !runProcess.Contains("RedirectStandardOutput", StringComparison.Ordinal))
            problems.Add("RunProcess is not the one that starts the process");

        return problems;
    }

    /// <summary>The text between the braces of the first method with this name, or null. Counts braces; the bodies it is used on contain no braces in strings.</summary>
    private static string? MethodBody(string source, string name)
    {
        var declaration = Regex.Match(source, $@"\b{name}\s*\([^)]*\)\s*(\r?\n\s*)?\{{");
        if (!declaration.Success) return null;

        var open = declaration.Index + declaration.Length - 1;
        var depth = 0;
        for (var i = open; i < source.Length; i++)
        {
            if (source[i] == '{') depth++;
            else if (source[i] == '}' && --depth == 0) return source[open..(i + 1)];
        }

        return null;
    }

    private static string ResolveRepositoryFile(string relativePath)
    {
        var native = relativePath.Replace('/', Path.DirectorySeparatorChar);
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, native);
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        Assert.Fail(
            $"Could not locate {relativePath} by walking up from {AppContext.BaseDirectory}. " +
            "This test reads repository sources directly and must not be run outside the repository tree.");
        return string.Empty; // unreachable
    }
}
