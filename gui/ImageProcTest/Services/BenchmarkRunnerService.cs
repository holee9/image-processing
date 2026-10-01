using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace ImageProcTest.Services;

/// <summary>
/// What Tools -> Benchmark Runner can do right now (#225 row 17, GUI-C-176).
/// </summary>
/// <param name="TestDirectory">The CMake build tree to run <c>ctest</c> in, or null when there is none.</param>
/// <param name="Arguments">The <c>ctest</c> argument list, empty when <paramref name="TestDirectory"/> is null.</param>
/// <param name="Message">One status-bar line that is true in every state, including "not built".</param>
public sealed record BenchmarkRunPlan(string? TestDirectory, IReadOnlyList<string> Arguments, string Message)
{
    public bool IsReady => TestDirectory is not null;
}

/// <summary>
/// Decides whether the benchmark freeze tests can be run from here, and with what arguments.
///
/// <para><b>What is run is CI's, not a second opinion.</b> The pattern and the build tree are the ones the
/// <c>BP-06~09 benchmark freeze GTests</c> step of <c>.github/workflows/benchmark-regression.yml</c> uses;
/// <c>BenchmarkRunnerServiceTests</c> reads that step and fails when <see cref="TestPattern"/> stops matching
/// it. The 3000 ms budget and every other number are asserted by the tests themselves, so this class — and
/// the menu — judge nothing: the verdict is <c>ctest</c>'s exit code and its summary, shown as they came.</para>
///
/// <para><b>Who owns the arguments.</b> Everything below is a constant of this class or the repository root
/// the app derived; no operator-supplied string reaches the process, and the arguments go to the process as a
/// list, never through a shell.</para>
/// </summary>
public static class BenchmarkRunnerService
{
    /// <summary>The CMake preset that configures and builds the tree CI runs the benchmarks in.</summary>
    public const string BuildPreset = "ci-post";

    /// <summary>The preset's <c>binaryDir</c> (<c>build/${presetName}</c> in <c>CMakePresets.json</c>).</summary>
    public const string TestDirectoryRelativePath = "build/ci-post";

    /// <summary>The preset's <c>CMAKE_BUILD_TYPE</c>, passed as CI passes it.</summary>
    public const string BuildConfig = "RelWithDebInfo";

    /// <summary>
    /// Copied from the <c>$pattern = '...'</c> line of the step "Run BP-06~09 benchmark freeze GTests" in
    /// <c>.github/workflows/benchmark-regression.yml</c>. A copy, because the workflow is a PowerShell script
    /// and not a data file; <c>BenchmarkRunnerServiceTests</c> is what keeps the two from drifting.
    /// </summary>
    public const string TestPattern =
        @"FullPipelineE2E\.PostProcess_3072x3072_Within3000ms|BenchmarkFreeze|CollimationDetectTest\.BenchmarkFreeze_BP07_CollimationDetectionBaseline|ExposureIndex\.BenchmarkFreeze_BP0[89]_(EI|DI)CalcTimeBaseline";

    /// <summary>The two commands the same workflow step sequence runs before <c>ctest</c>, not paraphrased.</summary>
    public const string BuildHint =
        "Build it first: 'cmake --preset ci-post' and then 'cmake --build --preset ci-post --parallel' " +
        "(see .github/workflows/benchmark-regression.yml).";

    private const string TreeMarkerFile = "CTestTestfile.cmake";

    public static BenchmarkRunPlan Resolve(string repositoryRoot)
    {
        var testDirectory = Path.Combine(repositoryRoot, "build", BuildPreset);
        if (!File.Exists(Path.Combine(testDirectory, TreeMarkerFile)))
        {
            // Nothing is started: a ctest run in a directory with no tests would say "No tests were found"
            // and read as a result, when the truth is that there is nothing built to measure.
            return new BenchmarkRunPlan(null, Array.Empty<string>(),
                $"Benchmark runner is not built: {TestDirectoryRelativePath} has no {TreeMarkerFile}. {BuildHint}");
        }

        return new BenchmarkRunPlan(
            testDirectory,
            new[]
            {
                "--test-dir", testDirectory,
                "--build-config", BuildConfig,
                "--output-on-failure",
                // ctest treats a pattern that selects nothing as success. A benchmark run that ran no
                // benchmark must not be able to pass, so it is made an error by ctest itself rather than
                // by a judgement in the GUI.
                "--no-tests=error",
                "-R", TestPattern,
            },
            $"Benchmark runner ready: ctest in {TestDirectoryRelativePath}.");
    }

    /// <summary>
    /// The part of ctest's output that is its verdict: from the "N% tests passed" line to the end, which is
    /// the totals, and on failure the list of failed tests. Null when ctest printed no such line, so the shared
    /// runner falls back to its own choice of line rather than this method inventing one.
    /// </summary>
    public static string? Summarize(string stdout)
    {
        var lines = stdout.Split('\n').Select(line => line.Trim()).ToList();
        var start = lines.FindLastIndex(line => Regex.IsMatch(line, @"^\d+% tests passed"));
        if (start < 0)
        {
            return null;
        }

        return string.Join(" | ", lines.Skip(start).Where(line => line.Length > 0));
    }
}
