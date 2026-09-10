// GUI-C-12/13: keep every C# copy of the error-code enum in step with
// modules/common/include/xpe/common/xpe_error.h.
using System.Text.RegularExpressions;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Detects drift between the native error-code header and the C# enums that mirror it.
///
/// The traversal test in <see cref="ErrorCodeMappingTests"/> asks "does every code the enum
/// declares have a message?". That is only as complete as the enum, so it cannot notice a code
/// the header gained and the enum never did — which is how -11..-15 stayed missing while a test
/// called ForAllDefinedCodes kept passing (#117, GUI-C-11/12).
///
/// The repository holds more than one such enum, and a "kept in sync intentionally" comment is
/// not a mechanism. Each mirroring copy is therefore listed as DATA in <see cref="MirroringCopies"/>
/// and read from SOURCE as text: no project reference, no visibility requirement, and a new copy
/// costs one line here. A copy that is deliberately NOT a mirror is listed in
/// <see cref="NonMirroringCopies"/> with its reason, so it reads as excluded rather than forgotten.
///
/// No native DLL is involved — headers and sources are files in the repository, present in CI,
/// so these cases run everywhere and never skip.
/// </summary>
[Trait("Category", "Functional")]
public sealed class ErrorCodeHeaderParityTests
{
    private const string HeaderRelativePath = "modules/common/include/xpe/common/xpe_error.h";

    /// <summary>A C# enum that claims to mirror the native error codes 1:1.</summary>
    /// <param name="RelativePath">Repository-relative path of the file declaring it.</param>
    /// <param name="EnumName">Enum type name as written in that file.</param>
    public sealed record MirroringCopy(string RelativePath, string EnumName)
    {
        public override string ToString() => $"{RelativePath}::{EnumName}";
    }

    /// <summary>
    /// Every copy that must match the header. Add a line when a new mirror appears.
    /// </summary>
    public static readonly MirroringCopy[] AllMirroringCopies =
    {
        new("clients/ImageProcTest.IntegrationTests/PInvoke/XpeCommonNative.cs", "XpeErrorCode"),
        new("clients/ImageProcTest/PInvokeWrapper.cs", "XpeErrorCode"),
    };

    /// <summary>
    /// Enums that name error codes but are NOT header mirrors, with the reason they are excluded.
    /// Listed so a reader sees a decision rather than an oversight.
    /// </summary>
    public static readonly (string Path, string EnumName, string Why)[] NonMirroringCopies =
    {
        ("gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs", "XpeErrorCodeNative",
         "Declares only Ok = 0 and is used as a sign threshold (code < Ok means failure), " +
         "not as a name table. Forcing 1:1 here would add 16 members nothing reads."),
    };

    /// <summary>Path and enum name as plain strings — xUnit serialises those, so each copy
    /// shows up as its own named test case.</summary>
    public static IEnumerable<object[]> MirroringCopies() =>
        AllMirroringCopies.Select(c => new object[] { c.RelativePath, c.EnumName });

    /// <summary>Matches `#define XPE_OK 0` and `#define XPE_ERR_NAME -12`, ignoring trailing comments.</summary>
    private static readonly Regex DefinePattern = new(
        @"^\s*#define\s+(XPE_(?:OK|ERR_[A-Z0-9_]+))\s+(-?\d+)",
        RegexOptions.Multiline | RegexOptions.Compiled);

    /// <summary>#117: every code the header declares exists in the copy with the same value.</summary>
    [Theory]
    [MemberData(nameof(MirroringCopies))]
    public void Copy_CoversEveryHeaderCode_WithMatchingValue(string relativePath, string enumName)
    {
        var copy = new MirroringCopy(relativePath, enumName);
        var header = ReadHeaderCodes();
        var managed = ReadEnumFromSource(copy);

        var missing = header.Keys.Where(k => !managed.ContainsKey(k)).OrderBy(k => k).ToArray();
        Assert.True(
            missing.Length == 0,
            $"{copy} is missing header code(s): {string.Join(", ", missing)}. " +
            $"Add them with the value from {HeaderRelativePath}.");

        var mismatched = header
            .Where(kv => managed[kv.Key] != kv.Value)
            .Select(kv => $"{kv.Key}: header={kv.Value} copy={managed[kv.Key]}")
            .OrderBy(s => s)
            .ToArray();
        Assert.True(
            mismatched.Length == 0,
            $"{copy} value(s) disagree with the header: {string.Join("; ", mismatched)}");
    }

    /// <summary>
    /// #117: the copy declares nothing the header does not. An invented member would make the
    /// traversal test assert a message for a code the native side never returns.
    /// </summary>
    [Theory]
    [MemberData(nameof(MirroringCopies))]
    public void Copy_DeclaresNoCodeAbsentFromHeader(string relativePath, string enumName)
    {
        var copy = new MirroringCopy(relativePath, enumName);
        var header = ReadHeaderCodes();
        var managed = ReadEnumFromSource(copy);

        var extra = managed.Keys.Where(k => !header.ContainsKey(k)).OrderBy(k => k).ToArray();
        Assert.True(
            extra.Length == 0,
            $"{copy} declares code(s) absent from {HeaderRelativePath}: {string.Join(", ", extra)}");
    }

    /// <summary>
    /// Guards the parser itself: for the copy this assembly actually compiles, the members read
    /// from source must equal what the compiler produced. Without this, a parser bug could make
    /// every copy look compliant.
    /// </summary>
    [Fact]
    public void SourceParser_AgreesWithCompiledEnum_ForThisAssemblysCopy()
    {
        var parsed = ReadEnumFromSource(AllMirroringCopies[0]);
        var compiled = Enum.GetValues<XpeCommonNative.XpeErrorCode>()
            .ToDictionary(c => c.ToString(), c => (int)c, StringComparer.Ordinal);

        Assert.Equal(compiled.OrderBy(kv => kv.Key), parsed.OrderBy(kv => kv.Key));
    }

    // ---------- readers ----------

    /// <summary>Header code name → value, keyed by the C# member name (XPE_ERR_X → X, XPE_OK → OK).</summary>
    private static IReadOnlyDictionary<string, int> ReadHeaderCodes()
    {
        var path = ResolveRepositoryFile(HeaderRelativePath);
        var codes = new Dictionary<string, int>(StringComparer.Ordinal);

        foreach (Match m in DefinePattern.Matches(File.ReadAllText(path)))
        {
            var native = m.Groups[1].Value;
            var managed = native.StartsWith("XPE_ERR_", StringComparison.Ordinal)
                ? native["XPE_ERR_".Length..]
                : native["XPE_".Length..];
            codes[managed] = int.Parse(m.Groups[2].Value);
        }

        Assert.True(codes.Count > 0, $"No XPE_OK / XPE_ERR_* defines parsed from {path} — the header format changed.");
        return codes;
    }

    /// <summary>Member name → value for one enum declaration, read from its .cs source.</summary>
    private static IReadOnlyDictionary<string, int> ReadEnumFromSource(MirroringCopy copy)
    {
        var path = ResolveRepositoryFile(copy.RelativePath);
        var text = File.ReadAllText(path);

        var declaration = new Regex(
            $@"enum\s+{Regex.Escape(copy.EnumName)}\s*(?::\s*\w+\s*)?\{{(?<body>[^}}]*)\}}",
            RegexOptions.Singleline);

        var match = declaration.Match(text);
        Assert.True(match.Success, $"Could not find `enum {copy.EnumName}` in {path}.");

        var members = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (Match m in Regex.Matches(match.Groups["body"].Value, @"^\s*(\w+)\s*=\s*(-?\d+)\s*,?", RegexOptions.Multiline))
            members[m.Groups[1].Value] = int.Parse(m.Groups[2].Value);

        Assert.True(members.Count > 0, $"`enum {copy.EnumName}` in {path} parsed to zero members — the declaration format changed.");
        return members;
    }

    /// <summary>
    /// Resolves a repository-relative file by walking up from the test output directory.
    /// A missing file FAILS rather than skips: silently skipping would restore the blind spot
    /// this class exists to remove.
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
