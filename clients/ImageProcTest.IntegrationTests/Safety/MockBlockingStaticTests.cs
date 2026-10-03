// REQ-GUI-IT-007 (GUI-C-228b, 228c): "Mock fallback is a test failure", asserted from the test assembly's own metadata and from a conservative source scan, so the verdict does not depend on which test ran first.
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
/// in general, and chasing the shapes one at a time (Codex #126) leaves the next shape open. So the dynamic-load and reflection-creation APIs are forbidden outright in the test project's
/// sources, whatever their arguments look like (<see cref="ForbiddenApis"/>), and the few legitimate uses are an exact allow-list (<see cref="Allowed"/>): file, the exact line, and why it does not
/// load an assembly or create a type of the apps. A forbidden API is red until it is either removed or added to the list with that reason, so the review happens at the moment the API appears.
/// What the scan does NOT see: a call that goes through another assembly (a helper library that loads by itself), generated code, an API missing from the table, and a native library loaded
/// by path (a native module is not a managed type of the apps). None of those is claimed.</para>
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
    // The pattern table, the allow-list and the controls name the forbidden APIs on purpose. They sit between the two marker lines below, and ONLY that region of THIS file is left out of
    // the scan (each marker must occur exactly once); every other line of this file is scanned like any other file.

    // SCAN-EXEMPT-BEGIN
    /// <summary>The dynamic-load and reflection-creation APIs, by id. Forbidden in the test project's sources whatever the arguments are.</summary>
    internal static readonly (string Id, Regex Pattern)[] ForbiddenApis =
    [
        ("AssemblyLoadContext", new(@"\bAssemblyLoadContext\b", RegexOptions.Compiled)),
        ("AssemblyLoad", new(@"\bAssembly\s*\.\s*(Load|LoadFrom|LoadFile|UnsafeLoadFrom|LoadWithPartialName|ReflectionOnlyLoad\w*)\b", RegexOptions.Compiled)),
        ("TypeGetType", new(@"\bType\s*\.\s*GetType\b", RegexOptions.Compiled)),
        ("GetTypeWithArgument", new(@"\.GetType\s*\(\s*[^)\s]", RegexOptions.Compiled)),
        ("Activator", new(@"\bActivator\b", RegexOptions.Compiled)),
        ("CreateInstance", new(@"\bCreateInstance\w*\b", RegexOptions.Compiled)),
        ("ReflectionMember", new(@"\b(ConstructorInfo|MethodInfo|MethodBase)\b|\.(GetConstructors?|GetMethods?|GetMembers?|InvokeMember|DynamicInvoke)\s*\(", RegexOptions.Compiled)),
        ("DynamicCode", new(@"\b(CreateDelegate|DynamicMethod|ILGenerator|GetUninitializedObject|MakeGenericType|MakeGenericMethod)\b|\bExpression\s*\.\s*(Lambda|New|Call|Invoke)\b", RegexOptions.Compiled)),
    ];

    /// <summary>
    /// The exact uses that stay. A line is allowed only as a whole (trimmed text equal), in the named file. None of them loads an assembly, and none creates a type of the apps: each acts on a
    /// type that is already compiled into this assembly.
    /// </summary>
    internal static readonly (string File, string Line, string Why)[] Allowed =
    [
        ("Functional/NativeCommonSingleInstanceTests.cs",
            "var method = type.GetMethod(loader, BindingFlags.NonPublic | BindingFlags.Static);",
            "type comes from NativeTestClasses, a fixed array of three test classes of this assembly; the method is the private loader of a native module. No assembly is loaded and no app type is created."),
        ("Functional/RunnerProcessTests.cs",
            ".GetMethod(nameof(ForEachCommand_ACouldNotStartIsWordedDifferentlyFromARunThatFailed))!",
            "reads this test class's own method to count its InlineData rows; the method is never invoked."),
        ("Functional/AlertDisplayFormatterTests.cs",
            ".GetMembers(System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)",
            "lists the public static member names of AlertDisplayFormatter (a source file linked into this assembly) to assert none is a severity override; names only, nothing is invoked."),
    ];

    /// <summary>One line of control input per pattern id, as it would appear in a test that tries to build a Mock without a metadata row (the shapes of Codex #125 and #126 among them).</summary>
    internal static readonly (string Id, string Line)[] ControlLines =
    [
        ("AssemblyLoadContext", "var asm = AssemblyLoadContext.Default.LoadFromAssemblyPath(path);"),
        ("AssemblyLoad", "var a = Assembly.LoadFrom(path);"),
        ("TypeGetType", "var t = Type.GetType(name);"),
        ("GetTypeWithArgument", "var t = asm.GetType(typeName);"),
        ("GetTypeWithArgument", "var t = asm.GetType(\"A.B\");"),
        ("Activator", "var o = Activator.CreateInstance(t);"),
        ("CreateInstance", "var o = AppDomain.CurrentDomain.CreateInstanceAndUnwrap(a, n);"),
        ("ReflectionMember", "var ctor = t.GetConstructor(Type.EmptyTypes);"),
        ("ReflectionMember", "ConstructorInfo c = null;"),
        ("ReflectionMember", "var m = t.GetMethod(\"Make\");"),
        ("DynamicCode", "var d = Delegate.CreateDelegate(typeof(Func<object>), m);"),
        ("DynamicCode", "var f = Expression.Lambda<Func<object>>(Expression.New(t)).Compile();"),
    ];
    // SCAN-EXEMPT-END

    private const string ExemptBegin = "// SCAN-EXEMPT-" + "BEGIN";
    private const string ExemptEnd = "// SCAN-EXEMPT-" + "END";

    private static string ProjectDir => Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;

    /// <summary>This file without the marked region; the markers must be there exactly once each, in order.</summary>
    internal static string WithoutExemptRegion(string text)
    {
        var begin = text.IndexOf(ExemptBegin, StringComparison.Ordinal);
        var end = text.IndexOf(ExemptEnd, StringComparison.Ordinal);
        Assert.True(begin >= 0 && end > begin, "the exempt markers are missing or out of order");
        Assert.Equal(begin, text.LastIndexOf(ExemptBegin, StringComparison.Ordinal));
        Assert.Equal(end, text.LastIndexOf(ExemptEnd, StringComparison.Ordinal));
        return text[..begin] + text[(end + ExemptEnd.Length)..];
    }

    /// <summary>Every line of every source of the project that matches a forbidden pattern, as (relative path, trimmed line, pattern id, line number).</summary>
    internal static List<(string File, string Line, string Id, int No)> ForbiddenHits(out int filesScanned)
    {
        var hits = new List<(string, string, string, int)>();
        filesScanned = 0;
        foreach (var file in Directory.EnumerateFiles(ProjectDir, "*.cs", SearchOption.AllDirectories))
        {
            var rel = file[ProjectDir.Length..].Replace('\\', '/').TrimStart('/');
            if (rel.StartsWith("obj/") || rel.StartsWith("bin/")) continue;
            filesScanned++;
            var text = File.ReadAllText(file);
            if (rel == "Safety/MockBlockingStaticTests.cs") text = WithoutExemptRegion(text);
            hits.AddRange(HitsIn(text, rel));
        }

        return hits;
    }

    internal static IEnumerable<(string File, string Line, string Id, int No)> HitsIn(string text, string rel)
    {
        var lines = text.Replace("\r\n", "\n").Split('\n');
        for (var i = 0; i < lines.Length; i++)
        {
            foreach (var (id, pattern) in ForbiddenApis)
            {
                if (pattern.IsMatch(lines[i])) yield return (rel, lines[i].Trim(), id, i + 1);
            }
        }
    }

    /// <summary>
    /// The scan. No dynamic-load or reflection-creation API appears in the project's sources except the exact allow-listed lines, and every allow-listed line is still there and still needed.
    /// </summary>
    [Fact]
    public void TheTestProject_UsesNoDynamicLoadOrReflectionCreationApi_ExceptTheExactAllowList()
    {
        var hits = ForbiddenHits(out var scanned);
        Assert.True(scanned > 50, $"The scan read only {scanned} files; it is not looking at the project.");

        var unallowed = hits.Where(h => !Allowed.Any(a => a.File == h.File && a.Line == h.Line)).Select(h => $"{h.File}:{h.No} [{h.Id}] {h.Line}").Distinct().ToList();
        Assert.True(unallowed.Count == 0,
            "A dynamic-load or reflection-creation API appears in the test project. Remove it, or add the exact line to Allowed with the reason it loads no assembly and creates no app type:\n" + string.Join("\n", unallowed));

        var stale = Allowed.Where(a => !hits.Any(h => h.File == a.File && h.Line == a.Line)).Select(a => $"{a.File}: {a.Line}").ToList();
        Assert.True(stale.Count == 0, "These allow-list entries no longer match a line (the line changed or the use is gone); update or remove them:\n" + string.Join("\n", stale));
    }

    /// <summary>Control: each pattern id matches its own control line, so a pattern that was edited into matching nothing is red here.</summary>
    [Fact]
    public void EveryPattern_MatchesItsControlLines_AndEveryControlLineIsMatchedByItsPattern()
    {
        foreach (var (id, line) in ControlLines)
        {
            var pattern = ForbiddenApis.Single(p => p.Id == id).Pattern;
            Assert.True(pattern.IsMatch(line), $"pattern {id} does not match its control line: {line}");
        }

        foreach (var (id, _) in ForbiddenApis)
        {
            Assert.True(ControlLines.Any(c => c.Id == id), $"pattern {id} has no control line");
        }
    }

    /// <summary>Control: ordinary code that must NOT be flagged is not (a zero-argument <c>GetType()</c>, the assembly of a type, a delegate invoke).</summary>
    [Theory]
    [InlineData("var t = value.GetType();")]
    [InlineData("var path = typeof(Foo).Assembly.Location;")]
    [InlineData("callback?.Invoke(path);")]
    [InlineData("Assert.Equal(typeof(Foo), x.GetType());")]
    public void TheScan_DoesNotFlagOrdinaryCode(string line) => Assert.Empty(HitsIn(line, "x.cs"));

    /// <summary>
    /// Control for the exemption itself: this file with its marked region left IN would be flagged (the table names the APIs), and with the region taken out it is clean. So the exemption covers
    /// exactly the marked lines and nothing else of this file.
    /// </summary>
    [Fact]
    public void ThisFile_IsFlaggedWithItsExemptRegion_AndCleanWithoutIt()
    {
        var text = File.ReadAllText(Path.Combine(ProjectDir, "Safety", "MockBlockingStaticTests.cs"));

        Assert.NotEmpty(HitsIn(text, "Safety/MockBlockingStaticTests.cs"));
        Assert.Empty(HitsIn(WithoutExemptRegion(text), "Safety/MockBlockingStaticTests.cs"));
    }
}
