// #225 row 21 (GUI-C-180): the troubleshooting page quotes messages the app and its modules write. A quote that no longer
// exists in the source is a page that silently went stale, so every quote is looked up.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The rule: on <c>docs/help/content/troubleshooting.md</c>, every text in backticks is a verbatim quote from the source, and
/// <c>&lt;placeholder&gt;</c> inside it is a wildcard for text that varies (a C# interpolation hole, a printf conversion, an
/// error message). The quote has to be found in one source file under <c>gui/</c>, <c>clients/ImageProcTest/</c> or
/// <c>modules/</c> (tests, build output and third-party code are not searched, so a test that quotes the page cannot satisfy it; comments are stripped, so a message that survives only in a comment does not count).
///
/// <para><b>A placeholder page fails; it is not skipped.</b> A page with no quotes verifies nothing, and a skipped test
/// next to a passing one is the "did not run" that reads as "passed" (#214). Until the page is written this test is red on
/// purpose.</para>
///
/// <para>Backticks are for quotes only: a span shorter than six characters, or made of placeholders alone, is not a
/// verifiable quote and fails too, so identifiers and values are written without backticks.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class TroubleshootingDocQuoteTests
{
    private const string DocPath = "docs/help/content/troubleshooting.md";
    private const int MinimumQuoteLength = 6;

    [Fact]
    public void EveryQuoteOnTheTroubleshootingPage_ExistsInTheSource()
    {
        var docFile = BenchmarkRunnerServiceTests.ResolveRepositoryFile(DocPath);
        var quotes = QuotesIn(File.ReadAllText(docFile));

        if (quotes.Count == 0)
        {
            Assert.Fail(
                $"{DocPath} quotes no message, so nothing on it is verified — it is still the placeholder. " +
                "Quote each message in backticks (a <placeholder> is a wildcard); this test fails, rather than skips, until it does.");
        }

        var unverifiable = quotes.Where(q => !IsVerifiable(q)).ToList();
        Assert.True(unverifiable.Count == 0,
            "These backtick spans cannot be verified as quotes (shorter than " + MinimumQuoteLength + " characters, or only placeholders). " +
            "Write identifiers and values without backticks:" + Environment.NewLine + string.Join(Environment.NewLine, unverifiable.Select(q => "  `" + q + "`")));

        var root = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(docFile)!, "..", "..", ".."));
        var sources = LoadSources(root);
        Assert.True(sources.Count > 50, $"Only {sources.Count} source files were found under {root}; the search would verify nothing.");

        var missing = quotes.Where(q => !sources.Any(s => Matches(q, s))).Distinct().ToList();
        Assert.True(missing.Count == 0,
            $"{missing.Count} quoted message(s) on {DocPath} are in no source file under gui/, clients/ImageProcTest/ or modules/ " +
            "(the text changed, or the page was edited): " + Environment.NewLine + string.Join(Environment.NewLine, missing.Select(q => "  `" + q + "`")));
    }

    // -- the matcher can say no, and can say yes the way the real sources are written -----------------------------

    [Fact]
    public void AQuote_IsFoundWhenItsTextIsInTheSource_AndNotWhenAWordChanges()
    {
        var source = Normalize("var m = \"Benchmark runner is not built: \" + dir;");

        Assert.True(Matches("Benchmark runner is not built:", source));
        Assert.False(Matches("Benchmark runner is not built!", source));
        Assert.False(Matches("Benchmark runner was not built:", source));
    }

    [Fact]
    public void APlaceholder_StandsForAnInterpolationHoleOrAPrintfConversion_ButNotForAnotherLine()
    {
        Assert.True(Matches("<label> did not run: '<exe>' could not be started",
            Normalize("$\"{label} did not run: '{exePath}' could not be started ({reason}).\"")));
        Assert.True(Matches("<N> pixel(s) fell outside", Normalize("\"%zu pixel(s) fell outside the gain\"")));

        // The literal text around a placeholder still has to be there, and a placeholder never reaches into the next line.
        Assert.False(Matches("<label> did not run:", Normalize("$\"{label} could not run:\"")));
        Assert.False(Matches("did not run: <a> second", Normalize("var a = \"did not run: first\";\nvar b = \"unrelated second\";")));
    }

    [Fact]
    public void StringLiteralsWrittenAsSeveralPieces_AreJoinedBeforeTheLookup()
    {
        // C++ adjacent literals and C# concatenation, both as the sources really write long messages.
        var cpp = Normalize("snprintf(msg, n,\n    \"gain polynomial carries an inverted dose range \"\n    \"[%.1f, %.1f]: the clamp is not applied\",\n    lo, hi);");
        var cs = Normalize("var m = \"Your saved settings could not be read, \" +\n        $\"The original file was kept at '{path}'.\";");

        Assert.True(Matches("carries an inverted dose range [<a>, <b>]: the clamp is not applied", cpp));
        Assert.True(Matches("could not be read, The original file was kept at '<path>'.", cs));
    }

    [Fact]
    public void AnEscapedQuoteInTheSource_IsCompared_AsTheQuoteTheOperatorSees()
    {
        var source = Normalize("\"panel.linear is not \\\"false\\\", so the frame passed through\"");

        Assert.True(Matches("panel.linear is not \"false\", so the frame passed through", source));
    }

    [Fact]
    public void AMessageThatSurvivesOnlyInAComment_IsNotFound_ButTheSameTextInCodeIs()
    {
        const string message = "Stop: no render is in flight.";

        var comment = StripComments(Normalize("// StatusText = \"" + message + "\";\n/* \"" + message + "\" */\nvar x = 1;"));
        var code = StripComments(Normalize("// an old comment\nStatusText = \"" + message + "\";"));

        Assert.False(Matches(message, comment));
        Assert.True(Matches(message, code));
    }

    [Fact]
    public void ABacktickSpanThatIsTooShortOrOnlyPlaceholders_IsNotAVerifiableQuote()
    {
        Assert.False(IsVerifiable("-1"));
        Assert.False(IsVerifiable("<message>"));
        Assert.True(IsVerifiable("Export failed: <message>"));
    }

    // -- implementation ---------------------------------------------------------------------------------------

    internal static List<string> QuotesIn(string markdown) =>
        Regex.Matches(markdown, @"`([^`\r\n]+)`").Select(m => m.Groups[1].Value).ToList();

    internal static bool IsVerifiable(string quote) =>
        quote.Length >= MinimumQuoteLength && Regex.Split(quote, "<[^<>]*>").Any(part => part.Length > 0);

    /// <summary>Joins adjacent string literals (C++ <c>"a" "b"</c>, C# <c>"a" + "b"</c>, interpolated or not) and unescapes quotes.</summary>
    internal static string Normalize(string text)
    {
        text = text.Replace("\r\n", "\n");
        text = Regex.Replace(text, "\"[ \\t]*(?:\\+[ \\t]*)?\\n[ \\t]*\\$?\"", string.Empty);
        return text.Replace("\\\"", "\"").Replace("\\'", "'");
    }

    /// <summary>
    /// Removes block comments and whole-line <c>//</c> comments, so a message that survives only in a comment — code that no
    /// longer says it — is not found. A trailing <c>//</c> comment after code is left alone (stripping it would need a real
    /// parser, because <c>//</c> also occurs inside strings such as URLs).
    /// </summary>
    internal static string StripComments(string text)
    {
        text = Regex.Replace(text, @"/\*.*?\*/", string.Empty, RegexOptions.Singleline);
        return string.Join("\n", text.Split('\n').Where(line => !Regex.IsMatch(line, @"^\s*//")));
    }

    internal static bool Matches(string quote, string normalizedSource)
    {
        var parts = Regex.Split(quote, "<[^<>]*>");
        var pattern = string.Join("[^\\n]{0,200}?", parts.Select(Regex.Escape));
        return Regex.IsMatch(normalizedSource, pattern, RegexOptions.CultureInvariant);
    }

    private static List<string> LoadSources(string root)
    {
        var excluded = new[] { "/obj/", "/bin/", "/tests/", "/test/", "/build/", "/third_party/" };
        var wanted = new (string Dir, string[] Patterns)[]
        {
            ("gui", new[] { "*.cs" }),
            (Path.Combine("clients", "ImageProcTest"), new[] { "*.cs" }),
            ("modules", new[] { "*.cpp", "*.c", "*.h" }),
        };

        var sources = new List<string>();
        foreach (var (dir, patterns) in wanted)
        {
            var full = Path.Combine(root, dir);
            if (!Directory.Exists(full)) continue;
            foreach (var pattern in patterns)
            {
                foreach (var file in Directory.EnumerateFiles(full, pattern, SearchOption.AllDirectories))
                {
                    var relativePath = "/" + Path.GetRelativePath(root, file).Replace('\\', '/');
                    if (excluded.Any(relativePath.Contains)) continue;

                    // Not searched: nothing references this file (GUI-C-152 §3), so a message that exists only here is a
                    // message the app never shows — and it repeats text the live code also has ("Display pipeline failed: …"),
                    // which would let the page keep passing after the live message changed. Remove this line if it ever goes live.
                    if (Path.GetFileName(file) == "PipelineOrchestrator.cs") continue;
                    sources.Add(StripComments(Normalize(File.ReadAllText(file))));
                }
            }
        }

        return sources;
    }
}
