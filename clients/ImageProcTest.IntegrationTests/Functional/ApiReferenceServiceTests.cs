// #225 row 20 (GUI-C-169): the API Reference menu tells the truth in every state, and its notion of where
// the generated pages live is the generator's, not a copy that can drift.
using System.Text.RegularExpressions;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The menu's three answers, each tested against a TEMPORARY directory so the checkout is never touched,
/// plus the two couplings that make the answers mean something: the path is what <c>Doxyfile</c> writes,
/// and the how-to-generate line is what the README and CI actually do.
/// </summary>
[Trait("Category", "Functional")]
public sealed class ApiReferenceServiceTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), $"xpe-apiref-{Guid.NewGuid():N}");

    public ApiReferenceServiceTests() => Directory.CreateDirectory(_root);

    public void Dispose()
    {
        if (Directory.Exists(_root)) Directory.Delete(_root, recursive: true);
    }

    [Fact]
    public void NothingGenerated_SaysSoAndSaysHow_NotAnEmptyResult()
    {
        var status = ApiReferenceService.Resolve(_root);

        Assert.False(status.IsAvailable);
        Assert.Null(status.IndexPath);
        Assert.Contains("has not been generated", status.Message, StringComparison.Ordinal);
        Assert.Contains(ApiReferenceService.GenerationHint, status.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void AGenerationThatStoppedPartWay_IsNeitherAvailableNorNeverGenerated()
    {
        // The directory exists, the entry page does not: telling someone to generate what they think
        // exists sends them looking for the wrong problem, and calling it available opens nothing.
        Directory.CreateDirectory(Path.Combine(_root, "docs", "help", "generated", "doxygen"));

        var status = ApiReferenceService.Resolve(_root);

        Assert.False(status.IsAvailable);
        Assert.Contains("incomplete", status.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("has not been generated", status.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Generated_ReturnsTheEntryPage_AndWhenItWasGenerated()
    {
        var index = Path.Combine(_root, "docs", "help", "generated", "doxygen", "html", "index.html");
        Directory.CreateDirectory(Path.GetDirectoryName(index)!);
        File.WriteAllText(index, "<html/>");
        var stamp = new DateTime(2026, 1, 2, 3, 4, 0, DateTimeKind.Utc);
        File.SetLastWriteTimeUtc(index, stamp);

        var status = ApiReferenceService.Resolve(_root);

        Assert.True(status.IsAvailable);
        Assert.Equal(index, status.IndexPath);
        Assert.Equal(stamp, status.GeneratedUtc);
        // The time is printed beside the claim, in local time, so a page that predates the current headers
        // is not read as current just because it opened.
        Assert.Contains($"{stamp.ToLocalTime():yyyy-MM-dd HH:mm}", status.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void TheGenerationHint_CarriesThePrerequisite_NotJustTheCommand()
    {
        // Doxyfile reads its stylesheet and scripts from doxygen-awesome/. "doxygen Doxyfile" alone is an
        // instruction that does not work on a clean checkout — the failure this test exists to keep out.
        Assert.Contains("doxygen-awesome", ApiReferenceService.GenerationHint, StringComparison.Ordinal);
        Assert.Contains("doxygen Doxyfile", ApiReferenceService.GenerationHint, StringComparison.Ordinal);
    }

    [Fact]
    public void TheEntryPagePath_IsWhatTheDoxyfileActuallyWrites()
    {
        // Two facts from the generator's own configuration, combined the way Doxygen does: the output
        // directory (relative to the Doxyfile) plus the html subdirectory. If someone moves either, the
        // menu would keep answering "has not been generated" about a tree that exists under a new name.
        var doxyfile = ResolveRepositoryFile("docs/help/doxygen/Doxyfile");
        var text = File.ReadAllText(doxyfile);
        var output = Value(text, "OUTPUT_DIRECTORY");
        var html = Value(text, "HTML_OUTPUT");

        var doxyDir = Path.GetDirectoryName(doxyfile)!;
        var repoRoot = Path.GetFullPath(Path.Combine(doxyDir, "..", "..", ".."));
        var expected = Path.GetFullPath(Path.Combine(doxyDir, output, html, "index.html"));
        var actual = Path.GetFullPath(Path.Combine(
            repoRoot, ApiReferenceService.DoxygenIndexRelativePath.Replace('/', Path.DirectorySeparatorChar)));

        Assert.Equal(expected, actual);
    }

    [Fact]
    public void TheHintsCloneCommand_IsTheOneTheReadmeGives()
    {
        var readme = File.ReadAllText(ResolveRepositoryFile("docs/help/doxygen/README.md"));
        const string clone = "git clone https://github.com/jothepro/doxygen-awesome-css doxygen-awesome";

        Assert.Contains(clone, readme, StringComparison.Ordinal);
        Assert.Contains(clone, ApiReferenceService.GenerationHint, StringComparison.Ordinal);
    }

    private static string Value(string doxyfile, string key)
    {
        var match = Regex.Match(doxyfile, $@"^{key}\s*=\s*(\S+)\s*$", RegexOptions.Multiline);
        Assert.True(match.Success, $"{key} not found in the Doxyfile.");
        return match.Groups[1].Value;
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
