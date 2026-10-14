// GUI-C-237 (milestone 3, U9): the quick-start pages (English and Korean) quote the app. Every string a page quotes must be a string the app has, so a page and the app cannot drift apart unnoticed.
using System.Net;
using System.Text.RegularExpressions;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// The pages are <c>gui/ImageProcTest/help/quick-start.html</c> and <c>quick-start.ko.html</c>; the app is the source of <c>gui/ImageProcTest</c> (C# and XAML). The test needs no data and no running app.
/// What it takes from a page: the <c>&lt;em&gt;</c> and <c>&lt;code&gt;</c> spans (the page's way of quoting the screen) and the bold menu paths. What it does with them:
/// <list type="bullet">
/// <item>a menu path: every name in it is a <c>MenuItem Header</c> of the app's XAML (accelerator underscore removed); the running-app check of the English page is <c>W0</c> in the E2E project;</item>
/// <item>a UI name (a short <c>&lt;em&gt;</c>: <i>Calibration paths</i>, <i>Load Raw Image</i>, <i>Swipe</i>): appears in the app source as a whole quoted word or attribute value;</item>
/// <item>a message (a span with words): each stretch of it that is not a placeholder (<c>…</c>, <c>&lt;n&gt;</c>, a number) and is at least 10 characters long appears in the app source, with C# interpolation holes removed.</item>
/// </list>
/// What is NOT checked is listed by category in the test output, with the reason: launch commands, file and flag names (not app strings), messages that come from the native DLLs (XPE_WARN_*, XPE_ERR_*,
/// whose text lives in the modules), and emphasis that quotes nothing.
/// </summary>
public sealed class QuickStartCitationTests(ITestOutputHelper output)
{
    private static readonly string[] Pages = ["quick-start.html", "quick-start.ko.html"];

    /// <summary>Spans that are emphasis or prose in a page, not quotes of the screen. Every entry says why; a new one needs a reason.</summary>
    private static readonly Dictionary<string, string> NotQuotes = new(StringComparer.Ordinal)
    {
        ["directly contains"] = "emphasis in the prose of 'What You Need'",
        ["바로 들어 있는"] = "emphasis in the prose of '준비물'",
        ["바로 아래"] = "emphasis in the prose",
        ["Starting on Mock and switching:"] = "a lead-in label of the prose",
        ["Mock으로 시작해서 바꾸려면:"] = "a lead-in label of the prose",
        ["This guide's own check judged the saved float file against the first-stage reference image by SHA-256 (step 5), not the picture on screen; look at the picture yourself."] = "the page's own disclaimer",
        ["이 안내의 점검은 화면의 그림이 아니라 5번에서 저장한 float 파일을 1단계 기준 영상과 SHA-256 으로 비교했습니다. 그림은 직접 보고 판단하십시오."] = "the page's own disclaimer",
        ["When Something Is Wrong"] = "a heading of the page itself",
        ["문제가 생겼을 때"] = "a heading of the page itself",
    };

    private static string RepositoryRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !File.Exists(Path.Combine(dir.FullName, "gui", "ImageProcTest", "help", "quick-start.html")))
        {
            dir = dir.Parent;
        }

        Assert.True(dir is not null, "gui/ImageProcTest/help/quick-start.html was not found above " + AppContext.BaseDirectory);
        return dir!.FullName;
    }

    private static string Page(string name) => File.ReadAllText(Path.Combine(RepositoryRoot(), "gui", "ImageProcTest", "help", name));

    /// <summary>The C# and XAML files of the app (not obj, bin or the help pages), one text per file, with C# escapes undone.</summary>
    private static List<string> AppFiles()
    {
        var app = Path.Combine(RepositoryRoot(), "gui", "ImageProcTest");
        var sep = Path.DirectorySeparatorChar;
        return Directory.EnumerateFiles(app, "*.*", SearchOption.AllDirectories)
            .Where(f => (f.EndsWith(".cs", StringComparison.Ordinal) || f.EndsWith(".xaml", StringComparison.Ordinal))
                        && !f.Contains($"{sep}obj{sep}", StringComparison.Ordinal) && !f.Contains($"{sep}bin{sep}", StringComparison.Ordinal) && !f.Contains($"{sep}help{sep}", StringComparison.Ordinal))
            .Select(f => File.ReadAllText(f).Replace("\\\"", "\"").Replace("\\u2014", "—").Replace("\\\\", "\\"))
            .ToList();
    }

    /// <summary>All the files joined, with interpolation holes (<c>{x}</c>) removed, so a quoted stretch of a message can be looked up in it.</summary>
    private static string AppSource(List<string> files) => Regex.Replace(string.Join('\n', files), @"\{[^{}\r\n]*\}", string.Empty);

    private static IEnumerable<string> Spans(string page, string tag) =>
        Regex.Matches(page, $"<{tag}>(.*?)</{tag}>", RegexOptions.Singleline).Select(m => WebUtility.HtmlDecode(Regex.Replace(m.Groups[1].Value, "<[^>]+>", string.Empty)).Trim());

    /// <summary>Stretches of a quoted message that are real text: split at placeholders (<c>…</c>, <c>&lt;n&gt;</c>, <c>&lt;DLL folder name&gt;</c>) and at number runs, keep those of at least 10 characters.</summary>
    internal static IEnumerable<string> Fragments(string span) =>
        Regex.Split(span, @"…|<[^>]*>|\d{2,}|\.\.\.|^(?:Load failed|Save failed|preprocess): ").Select(f => f.Trim()).Where(f => f.Length >= 10);

    /// <summary>The prefixes the app puts in front of a message it composes (<c>$"Load failed: {ex.Message}"</c>); the page quotes the composed text, so the prefix is looked up on its own.</summary>
    private static readonly string[] ComposedPrefixes = ["Load failed: ", "Save failed: ", "preprocess: "];

    private static string? NotAnAppString(string span)
    {
        if (span.Length == 0) return "empty";
        if (span.StartsWith("--", StringComparison.Ordinal) || span.StartsWith("-Raw", StringComparison.Ordinal) || span.StartsWith(".\\", StringComparison.Ordinal)
            || span.StartsWith("$env", StringComparison.Ordinal) || span.StartsWith("XPE_NATIVE_DIR", StringComparison.Ordinal) || span.StartsWith("tools\\", StringComparison.Ordinal)
            || span.StartsWith("Get-FileHash", StringComparison.Ordinal))
        {
            return "launch command or flag (not an app string)";
        }

        if (Regex.IsMatch(span, @"^[\w.\\/-]+\.(dll|xcal|raw|png|ps1|h)$") || Regex.IsMatch(span, @"^(_corrected_\w+\.\w+|data/calibration/\w+|xpe_\w+|-?\d+|src|main|bytes|times)$"))
        {
            return "file, flag, library or module name (not an app string)";
        }

        if (Regex.IsMatch(span, @"^XPE_(WARN|ERR)_\w+")) return "message of the native DLLs (its text lives in the modules)";
        if (span.StartsWith("ALERT WARN", StringComparison.Ordinal)) return "log line prefix written by the alert drain";
        if (span.Contains("The process cannot access the file", StringComparison.Ordinal)) return "message of the operating system (an IOException text the app passes on)";
        return NotQuotes.TryGetValue(span, out var why) ? why : null;
    }

    [Fact]
    public void EveryMenuPathTheQuickStartPrints_NamesMenuItemsOfTheApp()
    {
        var xaml = File.ReadAllText(Path.Combine(RepositoryRoot(), "gui", "ImageProcTest", "MainWindow.xaml"));
        var headers = Regex.Matches(xaml, @"<MenuItem\b[^>]*?\bHeader=""([^""]+)""").Select(m => WebUtility.HtmlDecode(m.Groups[1].Value).Replace("_", string.Empty)).ToHashSet(StringComparer.Ordinal);
        Assert.True(headers.Count > 20, $"read only {headers.Count} menu headers from MainWindow.xaml: the scan does not see the menu.");
        Assert.Contains("Open Raw...", headers);   // control: the scan finds a name the page certainly quotes

        var missing = new List<string>();
        var total = 0;
        foreach (var name in Pages)
        {
            var paths = Regex.Matches(Page(name), @"<strong>([^<]*?&gt;[^<]*?)</strong>")
                .Select(m => WebUtility.HtmlDecode(m.Groups[1].Value).Split('>').Select(x => x.Trim()).ToArray())
                .Where(p => p.Length >= 2 && p[0] is "File" or "Backend" or "View" or "Pipeline" or "Tools" or "Help")
                .Distinct(new PathComparer())
                .ToList();
            Assert.True(paths.Count >= 8, $"{name}: expected at least 8 menu paths, found {paths.Count}");
            foreach (var path in paths)
            {
                total++;
                var absent = path.Where(part => !headers.Contains(part)).ToArray();
                output.WriteLine($"{(absent.Length == 0 ? "ok     " : "MISSING")} {name}: {string.Join(" > ", path)}");
                if (absent.Length > 0) missing.Add($"{name}: {string.Join(" > ", path)} (no menu item named {string.Join(", ", absent)})");
            }
        }

        output.WriteLine($"{total} menu paths checked in {Pages.Length} pages");
        Assert.True(missing.Count == 0, "menu paths a quick-start page prints that the app does not have: " + string.Join("; ", missing));
    }

    [Fact]
    public void EveryStringTheQuickStartQuotes_IsAStringOfTheApp()
    {
        var files = AppFiles();
        var source = AppSource(files);
        Assert.Contains("Raw file is too small", source, StringComparison.Ordinal);   // control: the scan reads the app's messages

        var failures = new List<string>();
        var checkedCount = 0;
        var skipped = new SortedDictionary<string, int>(StringComparer.Ordinal);
        var tiers = new SortedDictionary<string, int>(StringComparer.Ordinal);
        foreach (var name in Pages)
        {
            var page = Page(name);
            foreach (var (kind, span) in Spans(page, "em").Select(s => ("em", s)).Concat(Spans(page, "code").Select(s => ("code", s))).Distinct())
            {
                var why = NotAnAppString(span);
                if (why is not null)
                {
                    skipped[why] = skipped.GetValueOrDefault(why) + 1;
                    continue;
                }

                checkedCount++;
                var fragments = Fragments(span).ToList();
                string tier;
                if (fragments.Count == 0 && span.Length <= 12)
                {
                    // a short quote: a UI name or a status word; it must stand as a whole quoted word or attribute value of the app
                    tier = Regex.IsMatch(source, $@"[""'>_]{Regex.Escape(span)}[""'<]") || ((span.Length >= 8 || span.EndsWith(':')) && source.Contains(span, StringComparison.Ordinal)) ? "exact" : "none";
                }
                else if (fragments.Count > 0 && fragments.All(f => source.Contains(f, StringComparison.Ordinal))
                         && ComposedPrefixes.Where(prefix => span.StartsWith(prefix, StringComparison.Ordinal)).All(prefix => source.Contains(prefix, StringComparison.Ordinal)))
                {
                    tier = "exact";
                }
                else
                {
                    // The app builds many of these lines at run time from parts (an interpolated value, an enum name), so the quoted text is not one literal of the source. Then the quote is cut at its
                    // separators (; , and the dash) and the words of five letters or more of each piece must occur TOGETHER in one source file: a renamed word (small -> tiny, Applied -> Done) fails, because
                    // no file holds the piece any more; a reordering or another value does not. Reported as "words" so the weaker check is visible.
                    var rest = ComposedPrefixes.Aggregate(span, (s, prefix) => s.StartsWith(prefix, StringComparison.Ordinal) ? s[prefix.Length..] : s);
                    var pieces = Regex.Split(rest, @"[;,—]").Select(piece => Regex.Matches(piece, @"[A-Za-z_]{5,}").Select(m => m.Value).Distinct().ToList()).Where(words => words.Count > 0).ToList();
                    tier = pieces.Count > 0 && pieces.All(words => files.Any(file => words.All(w => file.Contains(w, StringComparison.Ordinal)))) ? "words" : "none";
                }

                var found = tier != "none";
                tiers[tier] = tiers.GetValueOrDefault(tier) + 1;

                output.WriteLine($"{(found ? "ok " + tier.PadRight(5) : "MISSING")} {name} <{kind}> {span}");
                if (!found) failures.Add($"{name} <{kind}> {span}");
            }
        }

        output.WriteLine($"{checkedCount} quoted strings checked in {Pages.Length} pages ({string.Join(", ", tiers.Select(kv => $"{kv.Key}: {kv.Value}"))}); not app strings (by category): " + string.Join("; ", skipped.Select(kv => $"{kv.Key} x{kv.Value}")));
        Assert.True(checkedCount >= 40, $"only {checkedCount} quoted strings were found: the page parse is broken.");
        Assert.True(failures.Count == 0, "strings a quick-start page quotes that the app source does not have:\n" + string.Join("\n", failures));
    }

    private sealed class PathComparer : IEqualityComparer<string[]>
    {
        public bool Equals(string[]? x, string[]? y) => x is not null && y is not null && x.SequenceEqual(y);

        public int GetHashCode(string[] obj) => string.Join(">", obj).GetHashCode();
    }
}
