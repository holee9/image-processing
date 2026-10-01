// #225 row 21 (GUI-C-181): Help -> Troubleshooting opens the page DocFX generates, or says it is not there and how to make it.
using System.Text.Json;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The three answers against a TEMPORARY directory so the checkout is never touched, and the two couplings that make them
/// mean something: the page path is what <c>docfx.json</c> produces for <c>troubleshooting.md</c>, and the how-to-generate
/// line is what the <c>docs-generate</c> workflow runs.
/// </summary>
[Trait("Category", "Functional")]
public sealed class TroubleshootingPageServiceTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), $"xpe-trouble-{Guid.NewGuid():N}");

    public TroubleshootingPageServiceTests() => Directory.CreateDirectory(_root);

    public void Dispose()
    {
        if (Directory.Exists(_root)) Directory.Delete(_root, recursive: true);
    }

    [Fact]
    public void NothingGenerated_SaysSoAndSaysHow_NotAnEmptyResult()
    {
        var status = TroubleshootingPageService.Resolve(_root);

        Assert.False(status.IsAvailable);
        Assert.Null(status.PagePath);
        Assert.Null(status.GeneratedUtc);
        Assert.Contains("has not been generated", status.Message, StringComparison.Ordinal);
        Assert.Contains(TroubleshootingPageService.GenerationHint, status.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void OutputWithoutThePage_IsIncomplete_NotNeverGenerated()
    {
        Directory.CreateDirectory(Path.Combine(_root, "docs", "help", "generated", "docfx"));

        var status = TroubleshootingPageService.Resolve(_root);

        Assert.False(status.IsAvailable);
        Assert.Contains("is incomplete", status.Message, StringComparison.Ordinal);
        Assert.DoesNotContain("has not been generated", status.Message, StringComparison.Ordinal);
        Assert.Contains(TroubleshootingPageService.GenerationHint, status.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void Generated_ReturnsThePage_AndWhenItWasGenerated()
    {
        var page = Path.Combine(_root, "docs", "help", "generated", "docfx", "content", "troubleshooting.html");
        Directory.CreateDirectory(Path.GetDirectoryName(page)!);
        File.WriteAllText(page, "<html/>");
        var stamp = new DateTime(2026, 1, 2, 3, 4, 0, DateTimeKind.Utc);
        File.SetLastWriteTimeUtc(page, stamp);

        var status = TroubleshootingPageService.Resolve(_root);

        Assert.True(status.IsAvailable);
        Assert.Equal(page, status.PagePath);
        Assert.Equal(stamp, status.GeneratedUtc);
        Assert.Contains($"{stamp.ToLocalTime():yyyy-MM-dd HH:mm}", status.Message, StringComparison.Ordinal);
    }

    [Fact]
    public void ThePagePath_IsWhereDocfxWritesTheSourceMarkdown()
    {
        var docfxJson = ResolveRepositoryFile("docs/help/docfx/docfx.json");
        using var json = JsonDocument.Parse(File.ReadAllText(docfxJson), new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
        var build = json.RootElement.GetProperty("build");

        var output = build.GetProperty("output").GetString()!;
        var contentEntry = build.GetProperty("content").EnumerateArray()
            .First(e => e.TryGetProperty("src", out var src) && src.GetString() == "../content");
        var dest = contentEntry.GetProperty("dest").GetString()!;
        var source = ResolveRepositoryFile("docs/help/content/troubleshooting.md");

        // docs/help/docfx + ../generated/docfx + content + <markdown name>.html, as a path from the repository root.
        var expected = Path.GetRelativePath(
            Path.GetFullPath(Path.Combine(Path.GetDirectoryName(docfxJson)!, "..", "..", "..")),
            Path.GetFullPath(Path.Combine(Path.GetDirectoryName(docfxJson)!, output, dest, Path.GetFileNameWithoutExtension(source) + ".html")))
            .Replace('\\', '/');

        Assert.Equal(expected, TroubleshootingPageService.PageRelativePath);
    }

    [Fact]
    public void TheGenerationHint_IsTheTwoCommandsTheDocsWorkflowRuns()
    {
        var workflow = File.ReadAllText(ResolveRepositoryFile(".github/workflows/docs-generate.yml"));

        Assert.Contains("dotnet tool install -g docfx", workflow, StringComparison.Ordinal);
        Assert.Contains("docfx docs/help/docfx/docfx.json", workflow, StringComparison.Ordinal);
        Assert.Contains("'dotnet tool install -g docfx'", TroubleshootingPageService.GenerationHint, StringComparison.Ordinal);
        Assert.Contains("'docfx docs/help/docfx/docfx.json'", TroubleshootingPageService.GenerationHint, StringComparison.Ordinal);
    }

    private static string ResolveRepositoryFile(string relativePath) => BenchmarkRunnerServiceTests.ResolveRepositoryFile(relativePath);
}
