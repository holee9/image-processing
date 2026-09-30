using System;
using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// #225 (GUI-C-166): refuses to let a program run against a build older than the sources it was
/// compiled from.
///
/// <para><b>Why this is a shared class and not a rule to remember.</b> The rule — "build the project you
/// are about to run, not the one you just edited" — is correct and was already known when GUI-C-165
/// broke it: the app project was rebuilt, the E2E runner's own copy of <c>ImageProcTest.dll</c> stayed
/// two minutes old, and two falsification arms both exited 0. The cost is not a wrong green, which the
/// next run contradicts anyway. It is a wrong FACT: that run was about to be written up as "WPF coerces
/// IsEnabled from CanExecute, so a local False does nothing", and nothing would ever have contradicted
/// that sentence once it was in a report.</para>
///
/// <para><b>What lives here and what does not.</b> The load-bearing part is which files count as compile
/// inputs, and it is subtle enough that a second copy would drift: <c>bin</c>/<c>obj</c> are excluded
/// (<c>obj</c> is written DURING the build, so including it compares a build against itself), and only
/// <c>.cs</c>/<c>.xaml</c>/<c>.csproj</c>/<c>.resx</c> count. The reason for the extension filter is
/// measured, not stylistic: the app writes <c>automation-report.json</c> into its own project directory
/// after the build, and treating that as a source turned the original guard into a false red on every
/// gui-automation run (main, run 35399591335 — 97 of 127 cases failed in 13 s on that exception).
/// Content files are wrong here for a second reason too: editing <c>appsettings.json</c> and rebuilding
/// copies the file without relinking, so the artifact keeps its timestamp and the guard would fire on a
/// build that IS current. The question this answers is "was this artifact compiled from these sources",
/// and only compile inputs can answer it.</para>
///
/// <para><b>Where the sources are stays with the caller</b>, because it genuinely differs: the app's exe
/// sits three levels under its own project, while a runner's copy of the app dll sits under a DIFFERENT
/// project and must be compared against the app's sources. Sharing that guess would be sharing a bug.</para>
/// </summary>
public static class BuildFreshnessGuard
{
    /// <summary>
    /// Throws when any compile input under <paramref name="sourceRoot"/> is newer than
    /// <paramref name="artifactPath"/>.
    ///
    /// <para>Throws rather than warning or skipping, matching the guard this generalises: a skip would
    /// put "did not run" silently next to "passed", which is the distinction this repository keeps
    /// having to restore.</para>
    ///
    /// <para>Stays quiet when the artifact or the source root is missing — it cannot tell a stale build
    /// from an unusual layout, and inventing a failure there would make the guard the thing that breaks
    /// unfamiliar setups.</para>
    /// </summary>
    /// <param name="artifactPath">The built file whose age is in question (an exe or a dll).</param>
    /// <param name="sourceRoot">Directory holding the sources it was compiled from.</param>
    /// <param name="artifactLabel">How to name the artifact in the message, e.g. "gui app executable".</param>
    /// <param name="remedy">The exact command that fixes it — see the message note below.</param>
    public static void EnsureArtifactIsNotOlderThanSources(
        string artifactPath, string sourceRoot, string artifactLabel, string remedy)
    {
        if (!File.Exists(artifactPath) || !Directory.Exists(sourceRoot)) return;

        var artifactWrittenUtc = File.GetLastWriteTimeUtc(artifactPath);
        var newest = NewestCompileInput(sourceRoot, artifactWrittenUtc);
        if (newest is null) return;

        // Both paths and both times, and the command to run. "It is stale" alone does not say WHICH
        // project to build, and in the GUI-C-165 case the answer was the counter-intuitive one: the
        // runner project, not the app project that had just been edited.
        throw new InvalidOperationException(
            $"The {artifactLabel} is older than its sources, so this run would test the PREVIOUS build. " +
            $"'{artifactPath}' written {artifactWrittenUtc:O}; newest source '{newest.FullName}' written " +
            $"{newest.LastWriteTimeUtc:O}. {remedy}");
    }

    /// <summary>
    /// The newest compile input under <paramref name="sourceRoot"/>, or null when none is newer than
    /// <paramref name="thresholdUtc"/>.
    /// </summary>
    private static FileInfo? NewestCompileInput(string sourceRoot, DateTime thresholdUtc)
    {
        FileInfo? newest = null;

        foreach (var file in Directory.EnumerateFiles(sourceRoot, "*", SearchOption.AllDirectories))
        {
            if (file.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase) ||
                file.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            if (Path.GetExtension(file) is not (".cs" or ".xaml" or ".csproj" or ".resx")) continue;

            var info = new FileInfo(file);
            if (newest is null || info.LastWriteTimeUtc > newest.LastWriteTimeUtc) newest = info;
        }

        return newest is not null && newest.LastWriteTimeUtc > thresholdUtc ? newest : null;
    }
}
