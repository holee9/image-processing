using System;
using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// What Help -> Troubleshooting can honestly say right now (#225 row 21, GUI-C-181).
/// </summary>
/// <param name="PagePath">The page to open, or null when there is nothing to open.</param>
/// <param name="GeneratedUtc">When that page was generated; null with <paramref name="PagePath"/>.</param>
/// <param name="Message">One status-bar line that is true in every state, including "not generated".</param>
public sealed record TroubleshootingPageStatus(string? PagePath, DateTime? GeneratedUtc, string Message)
{
    public bool IsAvailable => PagePath is not null;
}

/// <summary>
/// Locates the generated Troubleshooting page and says what to do when it is not there. The same shape as
/// <see cref="ApiReferenceService"/> (row 20), and pure for the same reason: it takes the repository root, so its
/// three states are tested against a temporary directory.
///
/// <para><b>Why a generated page and not a copy inside the app.</b> The page's single source is
/// <c>docs/help/content/troubleshooting.md</c>, and <c>TroubleshootingDocQuoteTests</c> guards every quote on it
/// against the source code. A second, hand-written HTML copy under the app's own <c>help/</c> folder would be a
/// table nothing checks. The product has no installed-app artifact yet, so the commands of this family (rows
/// 14-17 and 20) already assume a checkout; installing the page with the app is a separate decision.</para>
///
/// <para>The path is DocFX's: <c>docs/help/docfx/docfx.json</c> renders <c>docs/help/content/*.md</c> into
/// <c>content/</c> under its output <c>../generated/docfx</c>. <c>TroubleshootingPageServiceTests</c> reads that
/// file, so a change of either side fails a test instead of leaving a menu that opens nothing.</para>
/// </summary>
public static class TroubleshootingPageService
{
    /// <summary>Where DocFX writes the page, relative to the repository root.</summary>
    public const string PageRelativePath = "docs/help/generated/docfx/content/troubleshooting.html";

    private const string DocfxOutputRelativePath = "docs/help/generated/docfx";

    /// <summary>
    /// The generation instruction shown when nothing has been generated: the two commands of the
    /// <c>docfx-managed</c> job of <c>.github/workflows/docs-generate.yml</c>, not paraphrased.
    /// </summary>
    public const string GenerationHint =
        "Generate it: install DocFX with 'dotnet tool install -g docfx' and then run 'docfx docs/help/docfx/docfx.json' " +
        "(see .github/workflows/docs-generate.yml).";

    public static TroubleshootingPageStatus Resolve(string repositoryRoot)
    {
        var page = Path.Combine(repositoryRoot, PageRelativePath.Replace('/', Path.DirectorySeparatorChar));
        if (File.Exists(page))
        {
            var generatedUtc = File.GetLastWriteTimeUtc(page);
            return new TroubleshootingPageStatus(
                page,
                generatedUtc,
                // The time sits beside the claim: a page generated before the source was edited should not read as
                // current just because it opened.
                $"Troubleshooting page opened — generated {generatedUtc.ToLocalTime():yyyy-MM-dd HH:mm}: {page}");
        }

        var output = Path.Combine(repositoryRoot, DocfxOutputRelativePath.Replace('/', Path.DirectorySeparatorChar));
        return Directory.Exists(output)
            // Output without the page is a generation that stopped part-way (or ran before the page existed), which is
            // neither "never generated" nor "available".
            ? new TroubleshootingPageStatus(null, null,
                $"Troubleshooting page is incomplete: {DocfxOutputRelativePath} exists but has no content/troubleshooting.html. {GenerationHint}")
            : new TroubleshootingPageStatus(null, null, $"Troubleshooting page has not been generated. {GenerationHint}");
    }
}
