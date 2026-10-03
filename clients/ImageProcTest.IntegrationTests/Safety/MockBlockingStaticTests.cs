// REQ-GUI-IT-007 (GUI-C-228b): "Mock fallback is a test failure", asserted from the test assembly's own metadata so the verdict does not depend on which test ran first.
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text.RegularExpressions;
using ImageProcTest.IntegrationTests.Functional;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// <see cref="MockBlockingTests"/> looks at the types loaded WHEN IT RUNS. A functional test that runs later and touches a Mock backend is invisible to a scan that already finished
/// (Codex #125). This reads the compiled test assembly instead: every type it defines or references, and every assembly it references, are rows in its metadata whatever the run order.
///
/// <para><b>What counts as a Mock backend.</b> Not a name list kept here: the set is read from the apps' own source, as every class under <c>gui/ImageProcTest</c> or
/// <c>clients/ImageProcTest</c> that implements <c>IXpeBackend</c>, except the <c>Real*</c> ones. Today that is MockXpeBackend (both apps), CompositeXpeBackend,
/// FaultInjectingBackend and two private CompositeDisposableBackend classes. A backend added tomorrow joins the set without an edit here. The test assembly does not reference the app
/// assemblies; it compiles a few app source files in. So a Mock reaches it either as a TypeDef (its source linked in) or as a TypeRef/AssemblyRef (an assembly reference added); both are read.
/// A generic argument is a type use like any other and carries its own TypeRef/TypeDef row, which the control below shows.</para>
///
/// <para><b>Limit.</b> A type built from a string at run time (<c>Type.GetType("…")</c>, <c>Activator.CreateInstance</c>, <c>Assembly.Load</c>) has no metadata row. The test project has no such
/// path today; <see cref="TheTestProject_HasNoRuntimeTypeFromStringPath"/> holds that, so adding one is a red test that asks for review, not a silent gap.</para>
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
    /// The assertion. The test assembly defines or references no type from the apps' non-real backend set and references no app assembly.
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

    private static readonly Regex TypeFromString = new(
        @"\bType\.GetType\s*\(|\bActivator\.CreateInstance\s*\(|\bAssembly\.(Load|LoadFrom|LoadFile)\s*\(|\.GetType\s*\(\s*""|\bAppDomain\.[A-Za-z.]*CreateInstance",
        RegexOptions.Compiled);

    /// <summary>The limit made visible: no source file of the test project builds a type from a string. This file is left out because it names the patterns.</summary>
    [Fact]
    public void TheTestProject_HasNoRuntimeTypeFromStringPath()
    {
        var projectDir = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;
        var hits = new List<string>();
        var scanned = 0;
        foreach (var file in Directory.EnumerateFiles(projectDir, "*.cs", SearchOption.AllDirectories))
        {
            var rel = file[projectDir.Length..].Replace('\\', '/');
            if (rel.StartsWith("/obj/") || rel.StartsWith("/bin/") || rel.EndsWith("/MockBlockingStaticTests.cs")) continue;
            scanned++;
            if (TypeFromString.IsMatch(File.ReadAllText(file))) hits.Add(rel);
        }

        Assert.True(scanned > 50, $"The scan read only {scanned} files; it is not looking at the project.");
        Assert.True(hits.Count == 0, "A type is built from a string in: " + string.Join(", ", hits) + ". A static metadata check cannot see it; review the new path.");
    }

    /// <summary>Control for the scan above: the patterns match the calls they name.</summary>
    [Theory]
    [InlineData("var t = Type.GetType(\"A.B\");")]
    [InlineData("var o = Activator.CreateInstance(t);")]
    [InlineData("var a = Assembly.LoadFrom(path);")]
    [InlineData("var t = asm.GetType(\"A.B\");")]
    public void TheFromStringPattern_MatchesTheCallsItNames(string line) => Assert.Matches(TypeFromString, line);
}
