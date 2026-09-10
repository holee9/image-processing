// #134 / #136: every native call in RealXpeBackend must go through the alert-draining wrapper.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Source-text guard over <c>RealXpeBackend</c>: a native call added outside
/// <c>InvokeNative</c> silently loses the alert drain, and nothing else would notice.
///
/// GUI-C-24 moved the drain from one call site to a common post-step, which removed the need to
/// wire each call site — but not the possibility of forgetting the wrapper on a NEW one. That gap
/// was reported at the time and this is the guard for it.
///
/// The check is textual, like ErrorCodeHeaderParityTests: the drain's presence is a property of the
/// source layout, and there is no runtime seam to observe it through (RealXpeBackend is WPF-bound
/// and cannot be loaded from this assembly).
/// </summary>
[Trait("Category", "Functional")]
public sealed class NativeCallWrapperGuardTests
{
    private const string BackendRelativePath = "gui/ImageProcTest/Services/RealXpeBackend.cs";

    /// <summary>
    /// Members allowed to call native without the wrapper, each with the reason it is exempt.
    /// Kept as data so an addition is a deliberate edit rather than a regex tweak.
    /// </summary>
    public static IEnumerable<object[]> ExemptMembers() =>
    [
        ["DrainNativeAlerts", "the drain itself — wrapping it would recurse"],
        ["ReadNativeAlert", "reads one queue entry on behalf of the drain"],
        ["HasExports", "static export probe used before an instance exists (CanUseNative)"],
    ];

    private static readonly string[] ExemptMemberNames =
        [.. ExemptMembers().Select(row => (string)row[0])];

    /// <summary>Native surfaces whose use must sit inside a wrapped member.</summary>
    private static readonly Regex NativeCall =
        new(@"\b(XpeCommonNative|XpeDisplayNative|NativeLibrary)\.", RegexOptions.Compiled);

    /// <summary>Start of a member body, used to attribute each call line to a member.</summary>
    private static readonly Regex MemberDeclaration =
        new(@"^\s{4}(?:\[[^\]]+\]\s*)?(?:public|private|internal|protected)\s[^;=]*?\b(?<name>\w+)\s*(?:<[^>]*>)?\s*\(",
            RegexOptions.Compiled);

    /// <summary>
    /// Every native call belongs to a member that is either routed through InvokeNative or listed
    /// as exempt. A new call site added without the wrapper fails here, naming the line.
    /// </summary>
    [Fact]
    public void EveryNativeCall_IsInsideAWrappedOrExemptMember()
    {
        var lines = File.ReadAllLines(ResolveRepositoryFile(BackendRelativePath));

        var wrappedMembers = WrappedMemberNames(lines);
        var offenders = new List<string>();
        var currentMember = "<file scope>";

        for (var i = 0; i < lines.Length; i++)
        {
            var declaration = MemberDeclaration.Match(lines[i]);
            if (declaration.Success)
            {
                currentMember = declaration.Groups["name"].Value;
            }

            if (!NativeCall.IsMatch(lines[i]))
            {
                continue;
            }

            if (wrappedMembers.Contains(currentMember) || ExemptMemberNames.Contains(currentMember))
            {
                continue;
            }

            offenders.Add($"{BackendRelativePath}:{i + 1} in '{currentMember}': {lines[i].Trim()}");
        }

        Assert.True(
            offenders.Count == 0,
            "Native calls outside InvokeNative and outside the exempt list — each loses the #110 alert " +
            "drain:" + Environment.NewLine + string.Join(Environment.NewLine, offenders));
    }

    /// <summary>
    /// The guard is only meaningful while the wrapper exists and at least one member uses it — a
    /// renamed or deleted InvokeNative would otherwise make every member "unwrapped" and, if the
    /// exempt list ever grew to cover everything, pass silently.
    /// </summary>
    [Fact]
    public void TheWrapperExists_AndAtLeastOneMemberRoutesThroughIt()
    {
        var lines = File.ReadAllLines(ResolveRepositoryFile(BackendRelativePath));

        Assert.Contains(lines, l => l.Contains("NativeAlertDrain.InvokeWithDrain", StringComparison.Ordinal));
        Assert.NotEmpty(WrappedMemberNames(lines));
    }

    /// <summary>Identifiers named on an InvokeNative line — the body the wrapper actually runs.</summary>
    private static readonly Regex Identifier = new(@"\b(?<name>\w+)\b", RegexOptions.Compiled);

    /// <summary>
    /// Members that run inside the wrapper: those whose body calls <c>InvokeNative</c>, plus the
    /// bodies the wrapper delegates to on that same line (<c>InvokeNative(() =&gt; XCore(...))</c>).
    /// Without the second half a split-out Core method reads as unwrapped while it is in fact only
    /// ever reached through the wrapper — measured on this file, which flagged four such lines.
    /// </summary>
    private static HashSet<string> WrappedMemberNames(string[] lines)
    {
        var wrapped = new HashSet<string>(StringComparer.Ordinal);
        var currentMember = "<file scope>";

        foreach (var line in lines)
        {
            var declaration = MemberDeclaration.Match(line);
            if (declaration.Success)
            {
                currentMember = declaration.Groups["name"].Value;
            }

            // The wrapper's own definition mentions InvokeNative on its declaration line; skip it.
            if (!line.Contains("InvokeNative(", StringComparison.Ordinal) ||
                line.Contains("private T InvokeNative", StringComparison.Ordinal))
            {
                continue;
            }

            wrapped.Add(currentMember);

            foreach (Match identifier in Identifier.Matches(line))
            {
                wrapped.Add(identifier.Groups["name"].Value);
            }
        }

        return wrapped;
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
