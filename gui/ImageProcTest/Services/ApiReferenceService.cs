using System;
using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// What Tools -> API Reference can honestly say right now (#225 row 20, GUI-C-169).
/// </summary>
/// <param name="IndexPath">The page to open, or null when there is nothing to open.</param>
/// <param name="GeneratedUtc">When that page was generated; null with <paramref name="IndexPath"/>.</param>
/// <param name="Message">One status-bar line that is true in every state, including "nothing to open".</param>
public sealed record ApiReferenceStatus(string? IndexPath, DateTime? GeneratedUtc, string Message)
{
    public bool IsAvailable => IndexPath is not null;
}

/// <summary>
/// Locates the generated native API reference and says what to do when it is not there.
///
/// <para><b>Pure and dependency-free on purpose.</b> It takes the repository root instead of finding it, so
/// the three states below can be tested against a temporary directory without touching the checkout, and
/// so a test project can link this one file (the repository's pattern for the WPF app).</para>
///
/// <para><b>Why only Doxygen.</b> The menu used to promise "(DocFX/Doxygen)". Measured on the artifacts of
/// the last green docs-generate run: the DocFX output holds 5 conceptual pages and NO API pages
/// (<c>xrefmap.yml</c> is 52 bytes and names no <c>ImageProcTest.</c> type), while the Doxygen output is a
/// real 265-file reference with <c>index.html</c> at its root. Opening the DocFX pages under the name "API
/// reference" would show something present-but-empty as if it were the thing — the same shape as printing
/// a switched-off stage as "0 ms" (GUI-C-168). It is left out and the tooltip says so.</para>
/// </summary>
public static class ApiReferenceService
{
    /// <summary>Where the Doxygen configuration writes its entry page, relative to the repository root.</summary>
    public const string DoxygenIndexRelativePath = "docs/help/generated/doxygen/html/index.html";

    private const string DoxygenOutputRelativePath = "docs/help/generated/doxygen";

    /// <summary>
    /// The generation instruction shown when nothing has been generated. Copied from
    /// <c>docs/help/doxygen/README.md</c> and the <c>doxygen-headers</c> CI job, not paraphrased: the
    /// clone is a PREREQUISITE (<c>Doxyfile</c> reads its stylesheet and scripts from
    /// <c>doxygen-awesome/</c>), so "run doxygen" alone would be an instruction that does not work.
    /// </summary>
    public const string GenerationHint =
        "Generate it: in docs/help/doxygen run 'git clone https://github.com/jothepro/doxygen-awesome-css " +
        "doxygen-awesome' and then 'doxygen Doxyfile' (Doxygen 1.12+; see docs/help/doxygen/README.md).";

    public static ApiReferenceStatus Resolve(string repositoryRoot)
    {
        var index = Path.Combine(repositoryRoot, DoxygenIndexRelativePath.Replace('/', Path.DirectorySeparatorChar));
        if (File.Exists(index))
        {
            var generatedUtc = File.GetLastWriteTimeUtc(index);
            return new ApiReferenceStatus(
                index,
                generatedUtc,
                // The time sits beside the claim: someone who regenerated nothing since the headers changed
                // should not read this page as current just because it opened.
                $"API reference (Doxygen) opened — generated {generatedUtc.ToLocalTime():yyyy-MM-dd HH:mm}: {index}");
        }

        var output = Path.Combine(repositoryRoot, DoxygenOutputRelativePath.Replace('/', Path.DirectorySeparatorChar));
        return Directory.Exists(output)
            // A directory with no entry page is a generation that stopped part-way, which is neither
            // "never generated" nor "available", and telling someone to generate what they think exists
            // would send them looking for the wrong problem.
            ? new ApiReferenceStatus(null, null,
                $"API reference is incomplete: {DoxygenOutputRelativePath} exists but has no html/index.html. {GenerationHint}")
            : new ApiReferenceStatus(null, null, $"API reference has not been generated. {GenerationHint}");
    }
}
