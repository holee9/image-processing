// #136: the parser's refusal must still stop the app. The two halves live in different files.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Source-text guard over <c>App.OnStartup</c>: the refusal <see cref="ImageProcTest.Services.AutomationArgs"/>
/// produces has to be acted on.
///
/// GUI-C-27 put the decision in the parser and the stop in the WPF <c>Application</c>, and reported
/// the gap that follows: delete the line that connects them and all nine parser tests still pass,
/// because they only ever look at the parser. This is the guard for that seam — the same shape as
/// the wrapper guard from GUI-C-25, for the same reason.
///
/// Textual, like <c>NativeCallWrapperGuardTests</c>: <c>App</c> derives from
/// <c>System.Windows.Application</c> and cannot be loaded from this assembly at all.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AutomationRejectionWiringGuardTests
{
    private const string AppRelativePath = "gui/ImageProcTest/App.xaml.cs";

    /// <summary>The parse call, the validity branch, and the stop — all three must be present.</summary>
    public static IEnumerable<object[]> RequiredWiring() =>
    [
        [@"AutomationArgs\.Parse\(", "App must parse through AutomationArgs rather than inline"],
        [@"if\s*\(\s*!\s*\w+\.IsValid\s*\)", "App must branch on the parser's verdict"],
        [@"Environment\.Exit\(", "App must stop the process when the command line was refused"],
    ];

    /// <summary>
    /// Each required piece of the refusal wiring appears in App.xaml.cs. Deleting any one of them —
    /// most easily the branch — makes a rejected command line start a normal run.
    /// </summary>
    [Theory]
    [MemberData(nameof(RequiredWiring))]
    public void RefusalWiring_IsPresentInApp(string pattern, string why)
    {
        var source = File.ReadAllText(ResolveRepositoryFile(AppRelativePath));

        Assert.True(
            Regex.IsMatch(source, pattern),
            $"{AppRelativePath} no longer matches /{pattern}/ — {why}. " +
            "The parser's rejection tests keep passing when this breaks, which is why this guard exists.");
    }

    /// <summary>
    /// The stop must sit INSIDE the validity branch. A guard that only counted the two lines
    /// separately would pass while an unconditional exit — or an exit moved elsewhere — silently
    /// changed what the app does with a valid command line.
    /// </summary>
    [Fact]
    public void TheStop_SitsInsideTheValidityBranch()
    {
        var source = File.ReadAllText(ResolveRepositoryFile(AppRelativePath));

        var branch = Regex.Match(source, @"if\s*\(\s*!\s*\w+\.IsValid\s*\)\s*\{(?<body>[^}]*)\}");

        Assert.True(branch.Success, $"{AppRelativePath}: could not find the '!IsValid' branch body.");
        Assert.Contains("Environment.Exit(", branch.Groups["body"].Value, StringComparison.Ordinal);
    }

    /// <summary>
    /// Walks up from the test output directory to find a repository file. A missing file FAILS
    /// rather than skips — silently skipping would restore the blind spot this class removes.
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

        Assert.Fail(
            $"Could not locate {relativePath} by walking up from {AppContext.BaseDirectory}. " +
            "This test reads repository sources directly and must not be run outside the repository tree.");
        return string.Empty; // unreachable
    }
}
