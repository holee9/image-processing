// #129: the same DLL name is now spelled in two places per module; keep the pairs equal.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Guards the DLL-name literals that GUI-C-16/17 duplicated on purpose.
///
/// Splitting each candidate list away from its P/Invoke wrapper meant the locator could no longer
/// reference the wrapper's <c>DllName</c> const — doing so drags <c>XpeCommonApi</c> back in and
/// this assembly cannot hold two DllImport resolvers (GUI-C-13). The cost is the same string in two
/// files. A comment saying "keep these in sync" is not a mechanism — that lesson is GUI-C-13's, and
/// this is the mechanism.
///
/// Pairs are DATA: a new split costs one row. Both sides are read from SOURCE, so no project
/// reference or visibility is required, and the cases never skip.
/// </summary>
[Trait("Category", "Functional")]
public sealed class DllNameParityTests
{
    /// <summary>Each row: two files that must spell the same DLL name in their <c>DllName</c> const.</summary>
    public static IEnumerable<object[]> DuplicatedNames() =>
    [
        [
            "clients/ImageProcTest/Diagnostics/XpeCommonLibraryLocator.cs",
            "clients/ImageProcTest/PInvokeWrapper.cs",
            "xpe_common.dll",
        ],
        [
            "clients/ImageProcTest/Diagnostics/XpeEnhanceBasicLibraryLocator.cs",
            "clients/ImageProcTest/PInvokeWrappers/XpeEnhanceBasicWrapper.cs",
            "xpe_enhance_basic.dll",
        ],
    ];

    /// <summary>#129: both files name the same DLL, and it is the one the pair is declared for.</summary>
    [Theory]
    [MemberData(nameof(DuplicatedNames))]
    public void DuplicatedDllName_AgreesAcrossBothFiles(string locatorPath, string wrapperPath, string expected)
    {
        var fromLocator = ReadDllNameLiteral(locatorPath);
        var fromWrapper = ReadDllNameLiteral(wrapperPath);

        Assert.Equal(expected, fromLocator);
        Assert.Equal(fromLocator, fromWrapper);
    }

    /// <summary>Reads the single `const string DllName = "...";` a file declares.</summary>
    private static string ReadDllNameLiteral(string relativePath)
    {
        var path = ResolveRepositoryFile(relativePath);
        var matches = Regex.Matches(
            File.ReadAllText(path),
            @"const\s+string\s+DllName\s*=\s*""(?<name>[^""]+)""\s*;");

        Assert.True(
            matches.Count == 1,
            $"Expected exactly one `const string DllName = \"...\";` in {relativePath}, found {matches.Count}. " +
            "If the declaration moved or multiplied, this guard needs updating rather than deleting.");

        return matches[0].Groups["name"].Value;
    }

    /// <summary>
    /// Resolves a repository-relative file by walking up from the test output directory. A missing
    /// file FAILS rather than skips — a silent skip would leave the duplication unguarded, which is
    /// the state this class exists to end.
    /// </summary>
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
        return string.Empty; // unreachable
    }
}
