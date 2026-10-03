// REQ-GUI-IT-007 (GUI-C-228b, 228c, 228d): "Mock fallback is a test failure", asserted from the test assembly's own metadata and from a conservative source scan, so the verdict does not depend on which test ran first.
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text.RegularExpressions;
using ImageProcTest.IntegrationTests.Functional;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// <see cref="MockBlockingTests"/> looks at the types loaded WHEN IT RUNS. A functional test that runs later and touches a Mock backend is invisible to a scan that already finished
/// (Codex #125). Two checks close that, and each claims only what it can see:
///
/// <para><b>1. Metadata (what is proven).</b> The compiled test assembly's own metadata lists every type it defines, references or forwards, and every assembly it references, whatever the run
/// order. The test asserts that none of them is a non-real backend of the apps and that no app assembly is referenced. "Non-real backend" is not a name list kept here: it is read from the
/// apps' source, as every class under <c>gui/ImageProcTest</c> or <c>clients/ImageProcTest</c> that implements <c>IXpeBackend</c>, except the <c>Real*</c> ones (today MockXpeBackend in both
/// apps, CompositeXpeBackend, FaultInjectingBackend and two private CompositeDisposableBackend classes). The test assembly does not reference the app assemblies; it compiles a few app source
/// files in, so a Mock reaches it as a TypeDef (source linked in) or as a TypeRef/AssemblyRef (a reference added). A generic argument is a type use like any other and carries its own row.</para>
///
/// <para><b>2. Source scan (what is forbidden, not proven absent).</b> A type built at run time from a path or a name has no metadata row. Reflection escapes cannot be excluded by a static check
/// in general, and chasing the shapes one at a time (Codex #126, #127) leaves the next shape open. So the dynamic-load and reflection-creation APIs are forbidden outright in the test project's
/// sources, whatever their arguments look like. The pattern table, the allow-list of the few legitimate uses (file, the exact line, why it loads no assembly and creates no app type) and the
/// controls live in <c>Resources/forbidden-reflection-apis.json</c>, which is not compiled and so cannot hide code; this file names none of those APIs. EVERY .cs file is scanned, this one
/// included, with no exempt region. Only a line that is wholly a comment is left out (<see cref="CodeLines"/>); strings and end-of-line comments are scanned, so a false alarm is possible and goes
/// to the allow-list, while a hidden call is not. What the scan does NOT see: a call that goes through another assembly, generated code, an API missing from the table, a native library loaded
/// by path, and a line of a multi-line string that looks like a comment and holds no quote.</para>
/// </summary>
[Trait("Category", "Safety")]
public sealed class MockBlockingStaticTests
{
    private static readonly string[] AppSourceFolders = ["gui/ImageProcTest", "clients/ImageProcTest"];
    private static readonly Regex BackendClass = new(@"\bclass\s+(\w+)\b[^{;=]*?\bIXpeBackend\b", RegexOptions.Compiled);

    /// <summary>The names the apps give to their non-real backends, from their source.</summary>
    internal static HashSet<string> MockBackendTypeNames()
    {
        var names = new HashSet<string>(StringComparer.Ordinal);
        foreach (var folder in AppSourceFolders)
        {
            var dir = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile(folder + "/App.xaml.cs"))!;
            foreach (var file in Directory.EnumerateFiles(dir, "*.cs", SearchOption.AllDirectories))
            {
                var rel = file[dir.Length..];
                if (rel.Contains("\\obj\\") || rel.Contains("/obj/") || rel.Contains("\\bin\\") || rel.Contains("/bin/")) continue;
                foreach (Match m in BackendClass.Matches(File.ReadAllText(file)))
                {
                    if (!m.Groups[1].Value.StartsWith("Real", StringComparison.Ordinal)) names.Add(m.Groups[1].Value);
                }
            }
        }

        return names;
    }

    /// <summary>Simple names of every type the assembly defines, references or forwards, and the names of the assemblies it references.</summary>
    internal static (HashSet<string> Types, HashSet<string> Assemblies) ReadMetadataNames(string assemblyPath)
    {
        using var stream = File.OpenRead(assemblyPath);
        using var pe = new PEReader(stream);
        var md = pe.GetMetadataReader();

        var types = new HashSet<string>(StringComparer.Ordinal);
        foreach (var h in md.TypeDefinitions) types.Add(md.GetString(md.GetTypeDefinition(h).Name));
        foreach (var h in md.TypeReferences) types.Add(md.GetString(md.GetTypeReference(h).Name));
        foreach (var h in md.ExportedTypes) types.Add(md.GetString(md.GetExportedType(h).Name));

        var assemblies = new HashSet<string>(StringComparer.Ordinal);
        foreach (var h in md.AssemblyReferences) assemblies.Add(md.GetString(md.GetAssemblyReference(h).Name));
        return (types, assemblies);
    }

    private static string TestAssemblyPath => typeof(MockBlockingStaticTests).Assembly.Location;

    /// <summary>
    /// The metadata assertion. The test assembly defines or references no type from the apps' non-real backend set and references no app assembly.
    /// </summary>
    [Fact]
    public void TheTestAssemblyMetadata_NamesNoMockBackendType_AndReferencesNoAppAssembly()
    {
        var mocks = MockBackendTypeNames();
        var (types, assemblies) = ReadMetadataNames(TestAssemblyPath);

        var hits = types.Intersect(mocks).OrderBy(n => n, StringComparer.Ordinal).ToList();
        Assert.True(hits.Count == 0, "The test assembly defines or references a non-real backend type: " + string.Join(", ", hits));

        var appAssemblies = assemblies.Where(a => a == "ImageProcTest" || a.StartsWith("ImageProcTest.", StringComparison.Ordinal) && a != "ImageProcTest.IntegrationTests").ToList();
        Assert.True(appAssemblies.Count == 0, "The test assembly references an app assembly: " + string.Join(", ", appAssemblies));
    }

    /// <summary>Control 1: the set is read from the source and contains the backends it should, so an empty set cannot make the assertion pass.</summary>
    [Fact]
    public void TheDerivedBackendSet_ContainsTheAppsMockBackends_AndNoRealOne()
    {
        var mocks = MockBackendTypeNames();

        Assert.Contains("MockXpeBackend", mocks);
        Assert.Contains("CompositeXpeBackend", mocks);
        Assert.Contains("FaultInjectingBackend", mocks);
        Assert.DoesNotContain("RealXpeBackend", mocks);
        Assert.DoesNotContain("RealXpeCommonBackend", mocks);
    }

    /// <summary>
    /// Control 2: the reader finds a type the assembly defines (a TypeDef), a type from another assembly (a TypeRef), and a type that appears ONLY as a generic argument
    /// (<see cref="GenericArgumentOnly"/>, the one use of <c>SocketFlags</c> in this project). An empty read would pass the assertion above; this shows it is not empty.
    /// </summary>
    [Fact]
    public void TheMetadataReader_FindsDefinedReferencedAndGenericArgumentTypes()
    {
        var (types, assemblies) = ReadMetadataNames(TestAssemblyPath);

        Assert.Contains(nameof(MockBlockingStaticTests), types);
        Assert.Contains("Assert", types);
        Assert.Contains("SocketFlags", types);
        Assert.Contains("xunit.core", assemblies);
        Assert.NotNull(GenericArgumentOnly());
    }

    private static object GenericArgumentOnly() => new List<System.Net.Sockets.SocketFlags>();

    /// <summary>Control 3: the comparison itself flags a hit. The same read, compared with a set that holds a type the assembly really references, is non-empty.</summary>
    [Fact]
    public void TheComparison_FlagsAnAssemblyThatReferencesAMemberOfTheSet()
    {
        var (types, _) = ReadMetadataNames(TestAssemblyPath);
        var pretendMockSet = new HashSet<string> { "Assert", "FactAttribute" };

        Assert.Equal(["Assert", "FactAttribute"], types.Intersect(pretendMockSet).OrderBy(n => n, StringComparer.Ordinal).ToList());
    }

    // ---- the source scan -------------------------------------------------------------------------------------------------------------------------------------------------------

    private sealed record Pattern(string Id, Regex Regex);

    private sealed record AllowedLine(string File, string Line, string Why);

    private sealed record ScanCase(string Name, string[] Lines, bool ExpectHit);

    private sealed record ControlLine(string Id, string Line);

    private sealed record ScanData(IReadOnlyList<Pattern> Patterns, IReadOnlyList<AllowedLine> Allowed, IReadOnlyList<ControlLine> Controls, IReadOnlyList<ScanCase> Cases);

    /// <summary>The ids the data file must define. The ids are labels, not API names.</summary>
    private static readonly string[] RequiredPatternIds =
    [
        "load-by-context", "load-assembly", "type-by-name-static", "type-by-name-instance",
        "activator-class", "create-instance-methods", "reflection-members", "dynamic-code",
    ];

    private static ScanData LoadScanData()
    {
        var path = Path.Combine(AppContext.BaseDirectory, "Resources", "forbidden-reflection-apis.json");
        Assert.True(File.Exists(path), "The pattern file is missing from the output folder: " + path);

        using var doc = System.Text.Json.JsonDocument.Parse(File.ReadAllText(path));
        var root = doc.RootElement;
        var patterns = root.GetProperty("patterns").EnumerateArray().Select(e => new Pattern(e.GetProperty("id").GetString()!, new Regex(e.GetProperty("regex").GetString()!, RegexOptions.Compiled))).ToList();
        var allowed = root.GetProperty("allowed").EnumerateArray().Select(e => new AllowedLine(e.GetProperty("file").GetString()!, e.GetProperty("line").GetString()!, e.GetProperty("why").GetString()!)).ToList();
        var controls = root.GetProperty("controls").EnumerateArray().Select(e => new ControlLine(e.GetProperty("id").GetString()!, e.GetProperty("line").GetString()!)).ToList();
        var cases = root.GetProperty("cases").EnumerateArray()
            .Select(e => new ScanCase(e.GetProperty("name").GetString()!, e.GetProperty("lines").EnumerateArray().Select(l => l.GetString()!).ToArray(), e.GetProperty("expectHit").GetBoolean())).ToList();

        // An empty or thinned table must not let the scan pass in silence.
        foreach (var id in RequiredPatternIds)
        {
            Assert.True(patterns.Any(p => p.Id == id && p.Regex.ToString().Length > 3), $"The pattern file defines no usable pattern '{id}'.");
        }

        Assert.Equal(RequiredPatternIds.Length, patterns.Count);
        Assert.NotEmpty(cases);
        Assert.NotEmpty(controls);
        Assert.True(allowed.All(a => a.Why.Length > 20), "Every allow-list entry needs its reason.");
        return new ScanData(patterns, allowed, controls, cases);
    }

    private static string ProjectDir => Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;

    /// <summary>
    /// The lines to scan, with their numbers. A line is left out only when it is wholly a comment: its trimmed text starts with two slashes, or it lies inside a block comment from its first
    /// character to its last. A line that holds a quote is always scanned, and so is a line where code follows the end of a block comment. No attempt is made to understand strings or to
    /// find a comment that starts after code, which is where a lexer mistake would hide a call.
    /// </summary>
    internal static IEnumerable<(int No, string Text)> CodeLines(string text)
    {
        var lines = text.Replace("\r\n", "\n").Split('\n');
        var inBlock = false;
        for (var i = 0; i < lines.Length; i++)
        {
            var raw = lines[i];
            var t = raw.Trim();
            var hasQuote = t.Contains('"');
            if (inBlock)
            {
                var close = t.IndexOf("*/", StringComparison.Ordinal);
                if (close < 0)
                {
                    if (hasQuote) yield return (i + 1, raw);
                    continue;
                }

                inBlock = false;
                if (t[(close + 2)..].Trim().Length == 0 && !hasQuote) continue;
                yield return (i + 1, raw);
                continue;
            }

            if (t.StartsWith("//", StringComparison.Ordinal) && !hasQuote) continue;

            if (t.StartsWith("/*", StringComparison.Ordinal))
            {
                var close = t.IndexOf("*/", 2, StringComparison.Ordinal);
                if (close < 0)
                {
                    inBlock = true;
                    if (hasQuote) yield return (i + 1, raw);
                    continue;
                }

                if (t[(close + 2)..].Trim().Length == 0 && !hasQuote) continue;
            }

            yield return (i + 1, raw);
        }
    }

    private static IEnumerable<(string Id, int No, string Line)> HitsIn(string text, IEnumerable<Pattern> patterns)
    {
        var all = patterns.ToList();
        foreach (var (no, raw) in CodeLines(text))
        {
            foreach (var p in all)
            {
                if (p.Regex.IsMatch(raw)) yield return (p.Id, no, raw.Trim());
            }
        }
    }

    /// <summary>Every scanned hit in every .cs file of the project, with no file and no region left out.</summary>
    private static List<(string File, string Line, string Id, int No)> ForbiddenHits(ScanData data, out int filesScanned)
    {
        var hits = new List<(string, string, string, int)>();
        filesScanned = 0;
        foreach (var file in Directory.EnumerateFiles(ProjectDir, "*.cs", SearchOption.AllDirectories))
        {
            var rel = file[ProjectDir.Length..].Replace('\\', '/').TrimStart('/');
            if (rel.StartsWith("obj/") || rel.StartsWith("bin/")) continue;
            filesScanned++;
            hits.AddRange(HitsIn(File.ReadAllText(file), data.Patterns).Select(h => (rel, h.Line, h.Id, h.No)));
        }

        return hits;
    }

    /// <summary>
    /// The scan. No dynamic-load or reflection-creation API appears in any .cs of the project except the exact allow-listed lines, and every allow-listed line is still there and still needed.
    /// </summary>
    [Fact]
    public void EveryCsFileOfTheTestProject_UsesNoDynamicLoadOrReflectionCreationApi_ExceptTheExactAllowList()
    {
        var data = LoadScanData();
        var hits = ForbiddenHits(data, out var scanned);
        Assert.True(scanned > 50, $"The scan read only {scanned} files; it is not looking at the project.");

        var unallowed = hits.Where(h => !data.Allowed.Any(a => a.File == h.File && a.Line == h.Line)).Select(h => $"{h.File}:{h.No} [{h.Id}] {h.Line}").Distinct().ToList();
        Assert.True(unallowed.Count == 0,
            "A dynamic-load or reflection-creation API appears in the test project. Remove it, or add the exact line to the allow-list in Resources/forbidden-reflection-apis.json with the reason it loads no assembly and creates no app type:\n" + string.Join("\n", unallowed));

        var stale = data.Allowed.Where(a => !hits.Any(h => h.File == a.File && h.Line == a.Line)).Select(a => $"{a.File}: {a.Line}").ToList();
        Assert.True(stale.Count == 0, "These allow-list entries no longer match a line (the line changed or the use is gone); update or remove them:\n" + string.Join("\n", stale));
    }

    /// <summary>Control: this file is among the scanned files and reaches the scan almost whole, so there is no region of it to put code in.</summary>
    [Fact]
    public void ThisFile_IsScanned_LikeEveryOtherCsFile()
    {
        var data = LoadScanData();
        var self = File.ReadAllText(Path.Combine(ProjectDir, "Safety", "MockBlockingStaticTests.cs"));

        Assert.Empty(HitsIn(self, data.Patterns));
        Assert.True(CodeLines(self).Count() > 100, "most of this file must reach the scan");
        Assert.NotEmpty(HitsIn(self + "\n[Fact] public void Hidden() { var o = " + "Acti" + "vator.Create" + "Instance(t); }\n", data.Patterns));
    }

    /// <summary>Control: each pattern matches its own control lines, so a pattern edited into matching nothing is red here.</summary>
    [Fact]
    public void EveryPattern_MatchesItsControlLines()
    {
        var data = LoadScanData();
        foreach (var c in data.Controls)
        {
            var pattern = data.Patterns.Single(p => p.Id == c.Id);
            Assert.True(pattern.Regex.IsMatch(c.Line), $"pattern {c.Id} does not match its control line: {c.Line}");
        }

        foreach (var p in data.Patterns)
        {
            Assert.True(data.Controls.Any(c => c.Id == p.Id), $"pattern {p.Id} has no control line");
        }
    }

    /// <summary>Control: the comment rule. Each synthetic source from the data file is scanned; a whole-line comment is skipped, anything with code on the line is not.</summary>
    [Fact]
    public void TheCommentRule_SkipsOnlyWholeLineComments_AndNeverHidesACallOnTheSameLine()
    {
        var data = LoadScanData();
        foreach (var c in data.Cases)
        {
            var hit = HitsIn(string.Join("\n", c.Lines), data.Patterns).Any();
            Assert.True(hit == c.ExpectHit, $"case '{c.Name}': expected hit={c.ExpectHit}, got {hit}");
        }
    }
}
