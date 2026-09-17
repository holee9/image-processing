// #182 / GUI-C-95: every setting a panel lets the user change either reaches processing, or says it does not.
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml.Linq;
using ImageProcTest.Models;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Connection survey over the GUI sources: each <see cref="AppSettings"/> property a XAML binding can
/// change must be read on the Real backend's processing path, or be declared unconnected with every
/// control that changes it disabled.
///
/// <para><b>How bound properties are collected.</b> Every <c>*.xaml</c> under <c>gui/ImageProcTest</c> is
/// parsed as XML and every attribute value of the form <c>{Binding [Path=]X…}</c> is read.
/// <c>Settings.P</c> maps to <c>P</c>; a bare <c>X</c> maps to <c>P</c> when the view model declares
/// <c>X</c> with a getter <c>=&gt; Settings.P</c> (the Lane B wrapper shape). Names are then checked
/// against <see cref="AppSettings"/> by reflection, so a parse that produced garbage fails.</para>
///
/// <para><b>What the collection misses:</b> bindings created in code-behind; <c>Binding</c> element
/// syntax (<c>&lt;Binding Path="…"/&gt;</c>); multi-bindings; wrappers that reach <c>Settings</c> through
/// any other shape than a <c>=&gt; Settings.P</c> getter; and settings changed by commands rather than
/// bindings (the calibration directory browse commands, which no XAML currently references).</para>
///
/// <para><b>Which bindings must be disabled.</b> Only bindings that can write the setting (see
/// <c>IsWritable</c>). A label that shows an unconnected value (<c>ViewportShell.xaml</c> shows the Lane A/B
/// algorithm names) is collected but needs no disabled control. "Disabled" means a literal
/// <c>IsEnabled="False"</c> on the element or an ancestor; a disabled state produced by a binding, style or
/// code is not seen, and the E2E scenarios (<c>UnappliedSettingsScenarios</c>) read the running app for that.</para>
///
/// <para><b>How "read on the processing path" is decided.</b> Entry points are the
/// <see cref="IXpeBackendProcessingEntries"/> of <c>RealXpeBackend</c>. A method body is taken by brace
/// matching, interpolated and plain string literals are removed, and what remains is searched for
/// <c>settings.P</c>. When the whole <c>settings</c> object is passed to another method, that method is
/// followed if it is found in <see cref="ProcessingSources"/>; if it is a declared summary/log helper it is
/// not followed; anything else fails the survey, because the analysis cannot see what it reads.</para>
///
/// <para><b>Summary and log text is not processing.</b> <c>ApplyDisplayPipelineCore</c> builds a summary
/// string that calls <c>BuildCalibrationEvaluationSummary(settings)</c>, which reads all seven calibration
/// modes. Removing string literals drops that call, and the helper is also on the not-followed list.
/// <see cref="Control_NaiveSearch_CountsTheSummaryAsProcessing"/> shows that a plain text search would
/// have counted the seven modes as connected. A read that feeds only a log call written without an
/// interpolated string (e.g. <c>AddLog(settings.P.ToString())</c>) is NOT distinguished — it would count
/// as processing. No such read exists in the entry points today.</para>
///
/// <para><b>Mock is not a processing path here.</b> <c>MockXpeBackend</c> simulates the seven calibration
/// modes on its synthetic pixels (<c>CreateMockCalibrationPixels</c>); the Real backend has no
/// counterpart, which is the finding #182 records. Counting the simulator would mark the modes connected
/// on the backend that has no such processing.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class SettingsProcessingConnectionTests
{
    private const string GuiRoot = "gui/ImageProcTest";
    private const string ViewModelPath = "gui/ImageProcTest/ViewModels/MainWindowViewModel.cs";

    /// <summary>Real backend methods that are processing. Everything they read is "connected".</summary>
    internal static readonly string[] IXpeBackendProcessingEntries = ["LoadRawImage", "ApplyDisplayPipeline", "RunPreprocessing"];

    /// <summary>Files searched when a processing method passes the whole settings object on.</summary>
    internal static readonly string[] ProcessingSources =
    [
        "gui/ImageProcTest/Services/RealXpeBackend.cs",
        "gui/ImageProcTest/Services/RawImageLoader.cs",
    ];

    /// <summary>Helpers that take the whole settings object only to describe it. Not followed.</summary>
    internal static readonly string[] SummaryHelpers = ["BuildCalibrationEvaluationSummary"];

    /// <summary>
    /// Declared unconnected (#182). Each must be bound, must not be read by processing, and every
    /// control bound to it must be disabled.
    /// </summary>
    internal static readonly string[] Unconnected =
    [
        nameof(AppSettings.OffsetCorrectionMode),
        nameof(AppSettings.GainCorrectionMode),
        nameof(AppSettings.DefectCorrectionMode),
        nameof(AppSettings.GhostCorrectionMode),
        nameof(AppSettings.TemperatureCompensationMode),
        nameof(AppSettings.NonlinearityCorrectionMode),
        nameof(AppSettings.BinningCorrectionMode),
        nameof(AppSettings.LaneBSharpeningSigma),
        nameof(AppSettings.LaneBDenoiseStrength),
        nameof(AppSettings.LaneAAlgorithm),
        nameof(AppSettings.LaneBAlgorithm),
    ];

    /// <summary>
    /// View state: where and how the image is shown, consumed by the viewport and panels, never meant
    /// for a backend. Listed explicitly so a new binding cannot land here by default.
    /// </summary>
    internal static readonly string[] ViewState =
    [
        nameof(AppSettings.ComparisonMode),
        nameof(AppSettings.ComparisonOverlayOpacity),
        nameof(AppSettings.ComparisonPanX),
        nameof(AppSettings.ComparisonPanY),
        nameof(AppSettings.ComparisonSwipePosition),
        nameof(AppSettings.ComparisonZoomScale),
        nameof(AppSettings.ShowDisplayPanel),
        nameof(AppSettings.AnalysisTab),
        nameof(AppSettings.FocusMode),
    ];

    [Fact]
    public void EveryBoundSetting_IsConnectedOrDeclaredAndDisabled()
    {
        var survey = Survey.Run(Unconnected, ViewState);
        Assert.True(survey.Violations.Count == 0, string.Join(Environment.NewLine, survey.Violations));
    }

    /// <summary>The collection itself: the counts measured in GUI-C-95, so a parser that silently finds
    /// nothing cannot pass the survey above by having nothing to judge.</summary>
    [Fact]
    public void Collection_FindsTheKnownBindings()
    {
        var survey = Survey.Run(Unconnected, ViewState);

        Assert.Equal(24, survey.Bindings.Select(b => b.Property).Distinct().Count());
        Assert.Equal(21, survey.Bindings.Count(b => Unconnected.Take(7).Contains(b.Property)));
        Assert.Contains(survey.Bindings, b => b.Property == nameof(AppSettings.LaneBSharpeningSigma) && b.Via == "LaneBSharpeningSigma" && b.Writable);
        Assert.Contains(survey.Bindings, b => b.Property == nameof(AppSettings.LaneAAlgorithm) && b.Writable);
        Assert.Contains(survey.Bindings, b => b.Property == nameof(AppSettings.LaneAAlgorithm) && !b.Writable);
        Assert.All(survey.Bindings.Where(b => b.Property == nameof(AppSettings.GhostCorrectionMode)), b => Assert.True(b.Writable && b.Disabled));
    }

    /// <summary>Presence control for the processing search: values the Real backend does hand to native
    /// code are found, including one reached only by following the whole-object pass into the loader.</summary>
    [Fact]
    public void ProcessingSearch_FindsKnownReads()
    {
        var reads = ProcessingAnalysis.Run(followSummaries: false, stripStrings: true).Reads;

        foreach (var p in new[]
                 {
                     nameof(AppSettings.VoiLutMode), nameof(AppSettings.VoiWindowCenter), nameof(AppSettings.VoiWindowWidth),
                     nameof(AppSettings.GsdfEnabled), nameof(AppSettings.ModalityRescaleSlope),
                     nameof(AppSettings.OffsetCalibrationDirectory), nameof(AppSettings.GainCalibrationDirectory),
                     nameof(AppSettings.DefectCalibrationDirectory), nameof(AppSettings.SelectedBodyPart),
                     nameof(AppSettings.RawWidth),
                 })
        {
            Assert.Contains(p, reads);
        }
    }

    /// <summary>
    /// Control: a text search that follows the summary helper and keeps string literals counts all seven
    /// calibration modes as processed — the reading #182 rejects. The real analysis must not.
    /// </summary>
    [Fact]
    public void Control_NaiveSearch_CountsTheSummaryAsProcessing()
    {
        var naive = ProcessingAnalysis.Run(followSummaries: true, stripStrings: false).Reads;
        var real = ProcessingAnalysis.Run(followSummaries: false, stripStrings: true).Reads;

        foreach (var mode in Unconnected.Take(7))
        {
            Assert.Contains(mode, naive);
            Assert.DoesNotContain(mode, real);
        }
    }

    /// <summary>Control: dropping any entry from the unconnected list turns the survey red for that entry.</summary>
    [Fact]
    public void Control_RemovingAnUnconnectedEntry_IsRed()
    {
        foreach (var removed in Unconnected)
        {
            var survey = Survey.Run(Unconnected.Where(p => p != removed).ToArray(), ViewState);
            Assert.Contains(survey.Violations, v => v.Contains(removed, StringComparison.Ordinal));
        }
    }

    /// <summary>Control: a newly bound setting that no processing reads is red until it is declared.</summary>
    [Fact]
    public void Control_BindingANewUnreadSetting_IsRed()
    {
        var extra = new Binding(nameof(AppSettings.LastRunSetId), "Settings.LastRunSetId", "synthetic.xaml", Disabled: false, Writable: true);
        var survey = Survey.Run(Unconnected, ViewState, extra);

        Assert.Contains(survey.Violations, v => v.Contains(nameof(AppSettings.LastRunSetId), StringComparison.Ordinal));
    }

    /// <summary>Control: an unconnected setting whose control is enabled is red.</summary>
    [Fact]
    public void Control_EnabledControlForUnconnectedSetting_IsRed()
    {
        var extra = new Binding(nameof(AppSettings.GhostCorrectionMode), "Settings.GhostCorrectionMode", "synthetic.xaml", Disabled: false, Writable: true);
        var survey = Survey.Run(Unconnected, ViewState, extra);

        Assert.Contains(survey.Violations, v => v.Contains(nameof(AppSettings.GhostCorrectionMode), StringComparison.Ordinal) && v.Contains("enabled", StringComparison.Ordinal));
    }

    /// <param name="Writable">The binding can change the setting. Display bindings (a label showing the
    /// value) are collected but do not need a disabled control.</param>
    internal sealed record Binding(string Property, string Via, string File, bool Disabled, bool Writable);

    internal sealed record Survey(IReadOnlyList<Binding> Bindings, IReadOnlyList<string> Violations)
    {
        public static Survey Run(string[] unconnected, string[] viewState, params Binding[] extra)
        {
            var bindings = CollectBindings().Concat(extra).ToList();
            var reads = ProcessingAnalysis.Run(followSummaries: false, stripStrings: true);
            var violations = new List<string>(reads.Blind);
            var known = typeof(AppSettings).GetProperties(BindingFlags.Public | BindingFlags.Instance)
                .Select(p => p.Name).ToHashSet(StringComparer.Ordinal);

            foreach (var group in bindings.GroupBy(b => b.Property))
            {
                var p = group.Key;
                if (!known.Contains(p))
                {
                    violations.Add($"{p}: bound in {group.First().File} but is not an AppSettings property");
                    continue;
                }

                if (reads.Reads.Contains(p)) continue;

                if (unconnected.Contains(p))
                {
                    foreach (var b in group.Where(b => b.Writable && !b.Disabled))
                        violations.Add($"{p}: declared unconnected but the control bound via '{b.Via}' in {b.File} is enabled");
                    continue;
                }

                if (viewState.Contains(p)) continue;

                violations.Add($"{p}: bound in {group.First().File} via '{group.First().Via}', not read by Real processing, and not declared unconnected");
            }

            foreach (var p in unconnected)
            {
                if (reads.Reads.Contains(p)) violations.Add($"{p}: declared unconnected but Real processing reads it — remove it from the list");
                if (!bindings.Any(b => b.Property == p)) violations.Add($"{p}: declared unconnected but nothing binds it — stale entry");
            }

            foreach (var p in viewState.Where(reads.Reads.Contains))
                violations.Add($"{p}: declared view state but Real processing reads it");

            return new Survey(bindings, violations);
        }
    }

    internal sealed record ProcessingAnalysis(HashSet<string> Reads, List<string> Blind)
    {
        public static ProcessingAnalysis Run(bool followSummaries, bool stripStrings)
        {
            var sources = ProcessingSources.Select(f => File.ReadAllText(ResolveRepositoryFile(f))).ToArray();
            var reads = new HashSet<string>(StringComparer.Ordinal);
            var blind = new List<string>();
            var visited = new HashSet<string>(StringComparer.Ordinal);
            var queue = new Queue<string>(IXpeBackendProcessingEntries);

            while (queue.Count > 0)
            {
                var method = queue.Dequeue();
                if (!visited.Add(method)) continue;

                var bodies = sources.Select(s => MethodBody(s, method)).Where(b => b is not null).ToList();
                if (bodies.Count == 0)
                {
                    blind.Add($"processing method '{method}' was not found in {string.Join(", ", ProcessingSources)}");
                    continue;
                }

                foreach (var raw in bodies)
                {
                    var body = stripStrings ? StripStrings(raw!) : raw!;
                    foreach (Match m in Regex.Matches(body, @"\bsettings\.(\w+)"))
                        reads.Add(m.Groups[1].Value);

                    foreach (Match m in Regex.Matches(body, @"\bsettings\b(?!\s*\.)"))
                    {
                        if (IsParameterDeclaration(body, m.Index)) continue;
                        var callee = EnclosingCallee(body, m.Index);
                        if (callee is null)
                            blind.Add($"'{method}' uses the settings object outside a call; the survey cannot follow it");
                        else if (SummaryHelpers.Contains(callee) && !followSummaries)
                            continue;
                        else if (sources.Any(s => MethodBody(s, callee) is not null))
                            queue.Enqueue(callee);
                        else
                            blind.Add($"'{method}' passes the settings object to '{callee}', which the survey cannot read");
                    }
                }
            }

            return new ProcessingAnalysis(reads, blind);
        }

        private static bool IsParameterDeclaration(string body, int index) =>
            Regex.IsMatch(body[..index], @"AppSettings\s*$");

        /// <summary>Identifier before the innermost unmatched '(' that encloses <paramref name="index"/>.</summary>
        private static string? EnclosingCallee(string body, int index)
        {
            var depth = 0;
            for (var i = index - 1; i >= 0; i--)
            {
                if (body[i] == ')') depth++;
                else if (body[i] == '(')
                {
                    if (depth == 0)
                    {
                        var m = Regex.Match(body[..i], @"(\w+)\s*$");
                        return m.Success ? m.Groups[1].Value : null;
                    }
                    depth--;
                }
                else if (body[i] is ';' or '{' && depth == 0) return null;
            }
            return null;
        }
    }

    /// <summary>Body of the first method declaration named <paramref name="name"/>: a braced block or an
    /// expression body up to its terminating ';'. Null when not declared in <paramref name="source"/>.</summary>
    internal static string? MethodBody(string source, string name)
    {
        var decl = Regex.Match(source, $@"(?m)^\s*(?:public|private|internal|protected)[^;=(]*\b{Regex.Escape(name)}\s*\([^)]*\)\s*(=>|\{{)");
        if (!decl.Success) return null;

        var start = decl.Index + decl.Length;
        var clean = StripStrings(source);
        if (decl.Groups[1].Value == "=>")
        {
            var depth = 0;
            for (var i = start; i < clean.Length; i++)
            {
                if (clean[i] is '(' or '{') depth++;
                else if (clean[i] is ')' or '}') depth--;
                else if (clean[i] == ';' && depth == 0) return source[start..i];
            }
            return null;
        }

        var braces = 1;
        for (var i = start; i < clean.Length; i++)
        {
            if (clean[i] == '{') braces++;
            else if (clean[i] == '}' && --braces == 0) return source[start..i];
        }
        return null;
    }

    /// <summary>
    /// Replaces every string and char literal — plain, verbatim and interpolated, including the
    /// expressions inside interpolation holes — with spaces of the same length, and blanks // comments.
    /// Length is preserved so indexes into the result are indexes into the source.
    /// </summary>
    internal static string StripStrings(string s)
    {
        var sb = new StringBuilder(s);
        var i = 0;
        while (i < s.Length)
        {
            if (s[i] == '/' && i + 1 < s.Length && s[i + 1] == '/')
            {
                var end = s.IndexOf('\n', i);
                if (end < 0) end = s.Length;
                Blank(sb, i, end);
                i = end;
            }
            else if (s[i] == '\'' )
            {
                var end = i + 1;
                while (end < s.Length && s[end] != '\'') end += s[end] == '\\' ? 2 : 1;
                Blank(sb, i, Math.Min(end + 1, s.Length));
                i = end + 1;
            }
            else if (s[i] is '"' or '$' or '@' && StringStart(s, i) is { } len)
            {
                var end = SkipString(s, i, len);
                Blank(sb, i, end);
                i = end;
            }
            else i++;
        }
        return sb.ToString();
    }

    private static int? StringStart(string s, int i)
    {
        var j = i;
        while (j < s.Length && s[j] is '$' or '@') j++;
        return j < s.Length && s[j] == '"' && j - i <= 2 ? j - i : null;
    }

    private static int SkipString(string s, int i, int prefixLength)
    {
        var prefix = s.Substring(i, prefixLength);
        var interpolated = prefix.Contains('$');
        var verbatim = prefix.Contains('@');
        var k = i + prefixLength + 1;
        while (k < s.Length)
        {
            var c = s[k];
            if (!verbatim && c == '\\') { k += 2; continue; }
            if (c == '"')
            {
                if (verbatim && k + 1 < s.Length && s[k + 1] == '"') { k += 2; continue; }
                return k + 1;
            }
            if (interpolated && c == '{')
            {
                if (k + 1 < s.Length && s[k + 1] == '{') { k += 2; continue; }
                k = SkipHole(s, k + 1);
                continue;
            }
            k++;
        }
        return s.Length;
    }

    private static int SkipHole(string s, int k)
    {
        var depth = 0;
        while (k < s.Length)
        {
            if (s[k] is '"' or '$' or '@' && StringStart(s, k) is { } len) { k = SkipString(s, k, len); continue; }
            if (s[k] == '{') depth++;
            else if (s[k] == '}')
            {
                if (depth == 0) return k + 1;
                depth--;
            }
            k++;
        }
        return s.Length;
    }

    private static void Blank(StringBuilder sb, int from, int to)
    {
        for (var k = from; k < to && k < sb.Length; k++)
            if (sb[k] != '\n' && sb[k] != '\r') sb[k] = ' ';
    }

    internal static IEnumerable<Binding> CollectBindings()
    {
        var wrappers = Regex.Matches(File.ReadAllText(ResolveRepositoryFile(ViewModelPath)),
                @"public\s+[\w<>?]+\s+(\w+)\s*\{\s*get\s*=>\s*Settings\.(\w+)\s*;")
            .ToDictionary(m => m.Groups[1].Value, m => m.Groups[2].Value, StringComparer.Ordinal);

        var root = Path.GetDirectoryName(ResolveRepositoryFile(ViewModelPath))!;
        root = Path.GetDirectoryName(root)!;
        var files = Directory.EnumerateFiles(root, "*.xaml", SearchOption.AllDirectories)
            .Where(f => !f.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}") &&
                        !f.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}"))
            .ToList();

        Assert.NotEmpty(files);

        foreach (var file in files)
        {
            var doc = XDocument.Load(file);
            var rel = Path.GetRelativePath(root, file).Replace('\\', '/');
            foreach (var el in doc.Descendants())
            {
                foreach (var attr in el.Attributes())
                {
                    var m = Regex.Match(attr.Value, @"^\{Binding\s+(?:Path=)?([\w.]+)");
                    if (!m.Success) continue;

                    var path = m.Groups[1].Value;
                    string? property = path.StartsWith("Settings.", StringComparison.Ordinal)
                        ? path["Settings.".Length..]
                        : wrappers.GetValueOrDefault(path);
                    if (property is null || property.Contains('.')) continue;

                    yield return new Binding(property, path, $"{GuiRoot}/{rel}", IsDisabled(el), IsWritable(el, attr.Name.LocalName, attr.Value));
                }
            }
        }
    }

    private static readonly Dictionary<string, string[]> DefaultTwoWay = new(StringComparer.Ordinal)
    {
        ["TextBox"] = ["Text"],
        ["ComboBox"] = ["SelectedItem", "SelectedValue", "Text"],
        ["ListBox"] = ["SelectedItem", "SelectedValue"],
        ["RadioButton"] = ["IsChecked"],
        ["CheckBox"] = ["IsChecked"],
        ["ToggleButton"] = ["IsChecked"],
        ["MenuItem"] = ["IsChecked"],
        ["Slider"] = ["Value"],
    };

    /// <summary>
    /// Whether a binding can write the setting: an explicit TwoWay/OneWayToSource mode, or a WPF
    /// property that binds two-way by default. An explicit OneWay/OneTime is read-only. Elements from
    /// other namespaces (custom controls) are treated as writable — their defaults are not known here.
    /// </summary>
    private static bool IsWritable(XElement el, string attribute, string value)
    {
        if (Regex.IsMatch(value, @"Mode=(TwoWay|OneWayToSource)")) return true;
        if (Regex.IsMatch(value, @"Mode=(OneWay|OneTime)")) return false;
        if (el.Name.NamespaceName != "http://schemas.microsoft.com/winfx/2006/xaml/presentation") return true;
        return DefaultTwoWay.TryGetValue(el.Name.LocalName, out var props) && props.Contains(attribute);
    }

    /// <summary>True when the element or an ancestor carries a literal IsEnabled="False".</summary>
    private static bool IsDisabled(XElement el) =>
        el.AncestorsAndSelf().Any(a => string.Equals((string?)a.Attribute("IsEnabled"), "False", StringComparison.Ordinal));

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

        Assert.Fail($"Could not locate {relativePath} by walking up from {AppContext.BaseDirectory}.");
        return string.Empty;
    }
}
