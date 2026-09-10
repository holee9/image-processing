// #138: a delegate whose arity drifts from the header is a silent ABI mismatch, not a build error.
using System.Text;
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Compares each preprocess delegate's parameter count against the native header's.
///
/// The hazard is specific: a <c>[UnmanagedFunctionPointer]</c> delegate is bound by NAME through
/// <c>GetProcAddress</c>, so an argument added on the native side still compiles and links — the
/// callee reads whatever occupies the slot the caller never wrote. What happens next is not
/// diagnosable: the GUI-C-39 falsification, run with the DLL staged, aborted the whole test host
/// mid-run with no failing assertion; without the DLL the same drift is simply invisible.
/// GUI-C-39 was opened for exactly that on <c>xpe_calib_generate_offset</c>, and QA-A-29 had already
/// done the same to <c>xpe_calib_save</c>.
///
/// This is a source-text guard for the same reason as the other guards in this suite: the mismatch
/// lives in the relationship between two files, and no runtime observation reveals it.
/// </summary>
[Trait("Category", "Functional")]
public sealed class PreprocessDelegateArityGuardTests
{
    private const string HeaderRelativePath = "modules/preprocess/include/xpe/preprocess_api.h";
    private const string DelegatesRelativePath =
        "clients/ImageProcTest.IntegrationTests/PInvoke/XpePreprocessNative.cs";

    /// <summary>
    /// Delegate to native-function pairs. One row per delegate that takes arguments.
    ///
    /// A delegate reused across several exports is paired with one representative: the three
    /// <c>xpe_calib_load_*</c> entry points share <c>CalibLoadDelegate</c>, and the three correction
    /// stages share <c>CorrectionDelegate</c>, because each group declares an identical signature.
    /// </summary>
    public static IEnumerable<object[]> DelegateArityPairs() =>
    [
        ["CalibGenerateOffsetDelegate", "xpe_calib_generate_offset"],
        ["CalibGenerateGainDelegate", "xpe_calib_generate_gain"],
        ["CorrectionDelegate", "xpe_offset_correct"],
        ["CalibLoadDelegate", "xpe_calib_load_offset"],
        ["InitDelegate", "xpe_preprocess_init"],
    ];

    /// <summary>
    /// The delegate declares exactly as many parameters as the header's function takes.
    ///
    /// When Lane A adds an argument, this fails on the next run and names both counts — instead of
    /// the call silently reading a stale stack slot.
    /// </summary>
    [Theory]
    [MemberData(nameof(DelegateArityPairs))]
    public void DelegateArity_MatchesTheHeader(string delegateName, string nativeFunction)
    {
        var headerArity = CountParameters(ExtractHeaderArguments(nativeFunction));
        var delegateArity = CountParameters(ExtractDelegateArguments(delegateName));

        Assert.True(
            headerArity == delegateArity,
            $"{delegateName} declares {delegateArity} parameter(s) but {nativeFunction} takes " +
            $"{headerArity} in {HeaderRelativePath}. A delegate is bound by name, so this mismatch " +
            "raises no compile or link error — the callee reads a slot the caller never wrote.");
    }

    /// <summary>
    /// The pair table covers every delegate that takes arguments. A new delegate added without a row
    /// would otherwise be unguarded — the gap this whole class exists to close.
    /// </summary>
    [Fact]
    public void EveryArgumentTakingDelegate_HasARow()
    {
        var source = File.ReadAllText(ResolveRepositoryFile(DelegatesRelativePath));

        var declared = Regex.Matches(source, @"public delegate [^;]*?(?<name>\w+Delegate)\s*\(")
            .Select(m => m.Groups["name"].Value)
            .Where(name => CountParameters(ExtractDelegateArguments(name)) > 0)
            .ToHashSet(StringComparer.Ordinal);

        var covered = DelegateArityPairs().Select(row => (string)row[0]).ToHashSet(StringComparer.Ordinal);
        var missing = declared.Except(covered).Order().ToArray();

        Assert.True(
            missing.Length == 0,
            $"These delegates take arguments but have no arity row: {string.Join(", ", missing)}. " +
            "Add them to DelegateArityPairs so a header change cannot pass unnoticed.");
    }

    /// <summary>Reads the header's argument list for one native function.</summary>
    private static string ExtractHeaderArguments(string nativeFunction)
    {
        var header = File.ReadAllText(ResolveRepositoryFile(HeaderRelativePath));
        var declaration = Regex.Match(header, $@"XPE_API\s+\w+\s+{Regex.Escape(nativeFunction)}\s*\(");

        Assert.True(declaration.Success, $"{nativeFunction} was not found in {HeaderRelativePath}.");
        return ReadBalancedArguments(header, declaration.Index + declaration.Length - 1);
    }

    /// <summary>Reads the delegate's argument list.</summary>
    private static string ExtractDelegateArguments(string delegateName)
    {
        var source = File.ReadAllText(ResolveRepositoryFile(DelegatesRelativePath));
        var declaration = Regex.Match(
            source, $@"public delegate [^;]*?{Regex.Escape(delegateName)}\s*\(");

        Assert.True(declaration.Success, $"{delegateName} was not found in {DelegatesRelativePath}.");
        return ReadBalancedArguments(source, declaration.Index + declaration.Length - 1);
    }

    /// <summary>
    /// Returns the text between the opening parenthesis at <paramref name="openIndex"/> and its
    /// matching close.
    ///
    /// Balanced rather than "up to the next close paren": a parameter can carry an attribute that
    /// contains parentheses of its own (<c>[MarshalAs(UnmanagedType.LPStr)]</c>), and stopping at the
    /// first close truncates the list. The first version of this guard did exactly that and reported
    /// CalibGenerateGainDelegate as taking 4 parameters when it declares 5.
    /// </summary>
    private static string ReadBalancedArguments(string text, int openIndex)
    {
        var depth = 0;
        var body = new StringBuilder();
        for (var i = openIndex; i < text.Length; i++)
        {
            var c = text[i];
            if (c == '(')
            {
                depth++;
                if (depth == 1) continue;
            }
            else if (c == ')')
            {
                depth--;
                if (depth == 0) return body.ToString();
            }

            body.Append(c);
        }

        Assert.Fail($"Unbalanced parentheses starting at offset {openIndex}.");
        return string.Empty; // unreachable
    }

    /// <summary>
    /// Counts parameters by splitting on commas that sit outside any nested parentheses or brackets,
    /// so an attribute or an array rank inside a parameter does not add to the count.
    /// </summary>
    private static int CountParameters(string arguments)
    {
        var trimmed = arguments.Trim();
        if (trimmed.Length == 0 || string.Equals(trimmed, "void", StringComparison.Ordinal)) return 0;

        var depth = 0;
        var count = 1;
        foreach (var c in trimmed)
        {
            if (c is '(' or '[') depth++;
            else if (c is ')' or ']') depth--;
            else if (c == ',' && depth == 0) count++;
        }

        return count;
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
