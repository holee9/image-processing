// REQ-GUI-IT-007 (GUI-C-228b, c, d, e): "Mock fallback is a test failure", asserted from the test assembly's own metadata and from a conservative source scan, so the verdict does not depend on which test ran first.
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
/// in general, and chasing the shapes one at a time (Codex #126, #127, #128) leaves the next shape open, so no rule here guesses at C# syntax. The dynamic-load and reflection-creation APIs
/// are forbidden outright in the sources compiled into this assembly, whatever their arguments look like. The pattern table, the allow-list and the controls live in
/// <c>Resources/forbidden-reflection-apis.json</c>, which is not compiled and so cannot hide code; this file names none of those APIs. The files scanned are not found by walking a folder: they are
/// the Document table of the assembly's own PDB, so the sources linked in from outside the project folder and the generated sources are in it, this file included. EVERY line of every one of them is
/// scanned, comments and strings too; nothing is classified or skipped. A line that names an API in prose is a hit like any other and is allow-listed as the whole line, in the named file, with its
/// reason; an allow-list entry whose line changed or vanished is red. What the scan does NOT see: an API missing from the table, a call that goes through another assembly, code generated while the
/// tests run, and a native library loaded by path (a native module is not a managed type of the apps).</para>
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

    /// <summary>A source file the compiler read, as the PDB names it, and where it is in this checkout.</summary>
    internal sealed record CompiledSource(string PdbPath, string Key, string FullPath);

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
        Assert.True(allowed.Select(a => (a.File, a.Line)).Distinct().Count() == allowed.Count, "The allow-list holds the same (file, line) twice.");
        return new ScanData(patterns, allowed, controls, cases);
    }

    private static string RepoRoot => Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("CMakePresets.json"))!;

    private static string ProjectDir => Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;

    /// <summary>
    /// The source files the compiler actually read for the test assembly, from the Document table of its portable PDB (a separate file next to the assembly, or embedded in it). That list holds
    /// the project's own files, the files linked in from outside the project folder, and generated files. Nothing is excluded here. A document that cannot be found in this checkout is an
    /// error, not a skip: the path in the PDB is where the build machine had the file, so when it is not there, the same path is looked for under the repository root by its longest existing tail.
    /// </summary>
    internal static List<CompiledSource> CompiledSources(string? assemblyPath = null, string? repoRoot = null)
    {
        assemblyPath ??= TestAssemblyPath;
        repoRoot ??= RepoRoot;
        using var assemblyStream = File.OpenRead(assemblyPath);
        using var pe = new PEReader(assemblyStream);
        var found = pe.TryOpenAssociatedPortablePdb(assemblyPath, p => File.Exists(p) ? File.OpenRead(p) : null, out var provider, out var pdbPath);
        Assert.True(found && provider is not null, "No portable PDB found for " + assemblyPath + " (neither embedded nor next to it): the list of compiled sources cannot be read.");
        using (provider)
        {
            var md = provider!.GetMetadataReader();
            var list = new List<CompiledSource>();
            var unresolved = new List<string>();
            foreach (var h in md.Documents)
            {
                var name = md.GetString(md.GetDocument(h).Name);
                if (!name.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)) continue;
                var full = ResolveDocument(name, repoRoot);
                if (full is null) { unresolved.Add(name); continue; }
                list.Add(new CompiledSource(name, KeyOf(full, repoRoot), full));
            }

            Assert.True(md.Documents.Count > 0, "The PDB " + pdbPath + " lists no documents.");
            Assert.True(unresolved.Count == 0, "These compiled sources named by the PDB cannot be found in this checkout:\n" + string.Join("\n", unresolved));
            Assert.True(list.Count > 100, $"Only {list.Count} compiled sources found in the PDB; the project has far more.");
            return list;
        }
    }

    private static string? ResolveDocument(string name, string repoRoot)
    {
        if (File.Exists(name)) return Path.GetFullPath(name);
        var parts = name.Replace('\\', '/').Split('/', StringSplitOptions.RemoveEmptyEntries);
        for (var k = 1; k < parts.Length; k++)
        {
            var candidate = Path.Combine(new[] { repoRoot }.Concat(parts[k..]).ToArray());
            if (File.Exists(candidate)) return candidate;
        }

        return null;
    }

    /// <summary>The repository-relative path with forward slashes, or the full path for a file outside the repository.</summary>
    internal static string KeyOf(string full, string repoRoot)
    {
        var root = Path.GetFullPath(repoRoot).TrimEnd('\\', '/') + Path.DirectorySeparatorChar;
        var f = Path.GetFullPath(full);
        return (f.StartsWith(root, StringComparison.OrdinalIgnoreCase) ? f[root.Length..] : f).Replace('\\', '/');
    }

    /// <summary>
    /// Every line is scanned: comments, strings and code alike. No line is classified, so there is no syntax rule to get wrong. A line that names an API in prose is a hit like any other and is
    /// listed in the allow-list with its reason.
    /// </summary>
    private static IEnumerable<(string Id, int No, string Line)> HitsIn(string text, IEnumerable<Pattern> patterns)
    {
        var all = patterns.ToList();
        var lines = text.Replace("\r\n", "\n").Split('\n');
        for (var i = 0; i < lines.Length; i++)
        {
            foreach (var p in all)
            {
                if (p.Regex.IsMatch(lines[i])) yield return (p.Id, i + 1, lines[i].Trim());
            }
        }
    }

    /// <summary>Every hit in every compiled source, as (repository-relative path, trimmed line, pattern id, line number).</summary>
    private static List<(string File, string Line, string Id, int No)> ForbiddenHits(ScanData data, IEnumerable<CompiledSource> sources)
    {
        var hits = new List<(string, string, string, int)>();
        foreach (var s in sources)
        {
            hits.AddRange(HitsIn(File.ReadAllText(s.FullPath), data.Patterns).Select(h => (s.Key, h.Line, h.Id, h.No)));
        }

        return hits;
    }

    /// <summary>
    /// The scan. No dynamic-load or reflection-creation API appears in any source compiled into the test assembly except the exact allow-listed lines (the whole trimmed line, in the named
    /// file), and every allow-listed line is still there and still needed.
    /// </summary>
    [Fact]
    public void EverySourceCompiledIntoTheTestAssembly_UsesNoDynamicLoadOrReflectionCreationApi_ExceptTheExactAllowList()
    {
        var data = LoadScanData();
        var hits = ForbiddenHits(data, CompiledSources());

        var unallowed = hits.Where(h => !data.Allowed.Any(a => a.File == h.File && a.Line == h.Line)).Select(h => $"{h.File}:{h.No} [{h.Id}] {h.Line}").Distinct().ToList();
        Assert.True(unallowed.Count == 0,
            "A dynamic-load or reflection-creation API appears in a source compiled into the test assembly. Remove it, or add the file and the whole line to the allow-list in Resources/forbidden-reflection-apis.json with the reason it loads no assembly and creates no app type:\n" + string.Join("\n", unallowed));

        var stale = data.Allowed.Where(a => !hits.Any(h => h.File == a.File && h.Line == a.Line)).Select(a => $"{a.File}: {a.Line}").ToList();
        Assert.True(stale.Count == 0, "These allow-list entries no longer match a line (the line changed or the use is gone); update or remove them:\n" + string.Join("\n", stale));
    }

    /// <summary>
    /// Control: the PDB list is the real compile list. It holds this file, a source linked in from outside the project folder, and a generated source; and every explicit compile item of the
    /// project file is in it, so nothing the project says it compiles is missing from what is scanned.
    /// </summary>
    [Fact]
    public void TheCompiledSourceList_HoldsThisFile_ALinkedAppSource_AGeneratedSource_AndEveryCompileItem()
    {
        var sources = CompiledSources();
        var keys = sources.Select(s => s.Key).ToHashSet(StringComparer.OrdinalIgnoreCase);

        Assert.Contains("clients/ImageProcTest.IntegrationTests/Safety/MockBlockingStaticTests.cs", keys);
        Assert.Contains("clients/ImageProcTest/Diagnostics/XpePreprocessOracleProcess.cs", keys);
        Assert.Contains(keys, k => k.StartsWith("clients/ImageProcTest.IntegrationTests/obj/", StringComparison.OrdinalIgnoreCase));

        var csproj = File.ReadAllText(Path.Combine(ProjectDir, "ImageProcTest.IntegrationTests.csproj"));
        var items = Regex.Matches(csproj, @"<Compile\s+Include=""([^""]+)""").Select(m => m.Groups[1].Value).ToList();
        Assert.NotEmpty(items);
        var missing = items.Where(i => !keys.Contains(KeyOf(Path.GetFullPath(Path.Combine(ProjectDir, i)), RepoRoot))).ToList();
        Assert.True(missing.Count == 0, "These explicit compile items are not in the compiled-source list read from the PDB:\n" + string.Join("\n", missing));
    }

    /// <summary>Control: the PDB reader is strict. A copy of the assembly with no PDB next to it is an error, not an empty list.</summary>
    [Fact]
    public void ThePdbReader_FailsWhenThereIsNoPdb()
    {
        var dir = Path.Combine(Path.GetTempPath(), "mbs_nopdb_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        try
        {
            var copy = Path.Combine(dir, Path.GetFileName(TestAssemblyPath));
            File.Copy(TestAssemblyPath, copy);
            Assert.ThrowsAny<Exception>(() => CompiledSources(copy));
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    /// <summary>Control: this file is scanned like every other, in full, and holds none of the patterns itself.</summary>
    [Fact]
    public void ThisFile_IsScannedInFull_AndHoldsNoPattern()
    {
        var data = LoadScanData();
        var self = File.ReadAllText(Path.Combine(ProjectDir, "Safety", "MockBlockingStaticTests.cs"));

        Assert.Empty(HitsIn(self, data.Patterns));
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

    /// <summary>Control: no line is excluded. Each synthetic source from the data file is scanned whole; comment-looking lines, a line inside an interpolated raw string and code after a comment all report.</summary>
    [Fact]
    public void NoLineIsExcluded_ByCommentOrByAnythingElse()
    {
        var data = LoadScanData();
        foreach (var c in data.Cases)
        {
            var hit = HitsIn(string.Join("\n", c.Lines), data.Patterns).Any();
            Assert.True(hit == c.ExpectHit, $"case '{c.Name}': expected hit={c.ExpectHit}, got {hit}");
        }
    }
}
