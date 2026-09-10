// #98 (GUI-C-41): a Native pass must name the binaries behind it.
using System.Diagnostics;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// Refuses a Native run whose staged binaries have no stated origin, and prints the origin when
/// they do.
///
/// GUI-C-40 produced "Native 9/9" against an <c>xpe_preprocess.dll</c> built before the signature
/// that run was meant to exercise. Nothing failed, because nothing was checking: the suite knew the
/// DIRECTORY it pinned and never asked what was in it. This test is the missing question.
///
/// It deliberately does not launch the app. The subject is the staged directory, not the GUI, and
/// keeping it out of <c>ApplicationFixture</c> means the provenance verdict is readable on its own
/// line rather than buried in a fixture failure that takes nine scenarios down with it.
/// </summary>
[Trait("Category", "Smoke")]
public sealed class NativeProvenanceTests(ITestOutputHelper output)
{
    /// <summary>
    /// A Native run states which CI run and commit produced its binaries, and their md5s.
    ///
    /// Missing record → FAIL: an unattributed Native pass reads as "the current code works
    /// natively", which is the claim GUI-C-40 could not actually support.
    /// Different commit → WARNING and pass: under #98 the newest artifact is routinely older than
    /// the working tree, and failing there would only teach the lane to delete the file.
    /// </summary>
    [SkippableFact]
    public void NativeRun_NamesTheBinariesItExercises()
    {
        var backend = Environment.GetEnvironmentVariable(ApplicationFixture.BackendVariable);
        Skip.If(
            !string.Equals(backend, "Native", StringComparison.OrdinalIgnoreCase),
            $"{ApplicationFixture.BackendVariable} is not 'Native' — this run stages no native binaries.");

        var nativeDirectory = Environment.GetEnvironmentVariable(ApplicationFixture.NativeDirVariable);
        Assert.False(
            string.IsNullOrWhiteSpace(nativeDirectory),
            $"A Native run must pin {ApplicationFixture.NativeDirVariable}; otherwise the loader " +
            "decides which binaries are used and no record can describe them (#129).");

        var provenance = NativeProvenance.Read(nativeDirectory!, out var reason);
        Assert.True(provenance is not null, reason);

        var description = provenance!.Describe(nativeDirectory!, TryCurrentHeadSha());
        output.WriteLine(description);

        // The record must describe files that are actually there — a record listing nothing would
        // satisfy every assertion above while naming no binary at all.
        Assert.True(
            provenance.Files is { Count: > 0 },
            $"{NativeProvenance.FileName} lists no files, so it attributes nothing.");
    }

    /// <summary>
    /// The commit under test, or null when git cannot answer.
    ///
    /// Null suppresses the drift warning rather than inventing one: an unknown current commit is not
    /// evidence that the artifacts match, and it is not evidence that they differ either.
    /// </summary>
    private static string? TryCurrentHeadSha()
    {
        try
        {
            using var process = Process.Start(new ProcessStartInfo("git", "rev-parse HEAD")
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                WorkingDirectory = AppContext.BaseDirectory,
            });

            if (process is null) return null;

            var sha = process.StandardOutput.ReadToEnd().Trim();
            process.WaitForExit(5000);
            return process.ExitCode == 0 && sha.Length > 0 ? sha : null;
        }
        catch (Exception)
        {
            // git absent or unusable — reported as "unknown" by returning null.
            return null;
        }
    }
}
