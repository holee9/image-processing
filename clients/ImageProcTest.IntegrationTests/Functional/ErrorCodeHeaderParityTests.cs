// GUI-C-12: keep XpeCommonNative.XpeErrorCode in step with modules/common/include/xpe/common/xpe_error.h.
using System.Text.RegularExpressions;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Detects drift between the native error-code header and the C# enum that mirrors it.
///
/// The traversal test in <see cref="ErrorCodeMappingTests"/> asks "does every code the enum
/// declares have a message?". That is only as complete as the enum, so it cannot notice a code
/// the header gained and the enum never did — which is exactly how -11..-15 stayed missing while
/// a test called ForAllDefinedCodes kept passing (#117, GUI-C-11). This class closes that gap by
/// reading the header itself.
///
/// No native DLL is involved: the header is a source file in the repository, present in CI, so
/// these cases run everywhere and never skip.
/// </summary>
[Trait("Category", "Functional")]
public sealed class ErrorCodeHeaderParityTests
{
    private const string HeaderRelativePath = "modules/common/include/xpe/common/xpe_error.h";

    /// <summary>Matches `#define XPE_OK 0` and `#define XPE_ERR_NAME -12`, ignoring trailing comments.</summary>
    private static readonly Regex DefinePattern = new(
        @"^\s*#define\s+(XPE_(?:OK|ERR_[A-Z0-9_]+))\s+(-?\d+)",
        RegexOptions.Multiline | RegexOptions.Compiled);

    /// <summary>Every code name → value declared by the header, keyed by the C# member name.</summary>
    private static IReadOnlyDictionary<string, int> ReadHeaderCodes()
    {
        var path = ResolveHeaderPath();
        var text = File.ReadAllText(path);
        var codes = new Dictionary<string, int>(StringComparer.Ordinal);

        foreach (Match m in DefinePattern.Matches(text))
        {
            // XPE_ERR_INVALID_INPUT -> INVALID_INPUT ; XPE_OK -> OK
            var native = m.Groups[1].Value;
            var managed = native.StartsWith("XPE_ERR_", StringComparison.Ordinal)
                ? native["XPE_ERR_".Length..]
                : native["XPE_".Length..];
            codes[managed] = int.Parse(m.Groups[2].Value);
        }

        Assert.True(codes.Count > 0, $"No XPE_OK / XPE_ERR_* defines parsed from {path} — the header format changed.");
        return codes;
    }

    private static IReadOnlyDictionary<string, int> ReadEnumCodes() =>
        Enum.GetValues<XpeCommonNative.XpeErrorCode>()
            .ToDictionary(c => c.ToString(), c => (int)c, StringComparer.Ordinal);

    /// <summary>#117: every code the header declares exists in the C# enum with the same value.</summary>
    [Fact]
    public void Enum_CoversEveryHeaderCode_WithMatchingValue()
    {
        var header = ReadHeaderCodes();
        var managed = ReadEnumCodes();

        var missing = header.Keys.Where(k => !managed.ContainsKey(k)).OrderBy(k => k).ToArray();
        Assert.True(
            missing.Length == 0,
            $"XpeErrorCode is missing header code(s): {string.Join(", ", missing)}. " +
            $"Add them to PInvoke/XpeCommonNative.cs with the value from {HeaderRelativePath}.");

        var mismatched = header
            .Where(kv => managed[kv.Key] != kv.Value)
            .Select(kv => $"{kv.Key}: header={kv.Value} enum={managed[kv.Key]}")
            .OrderBy(s => s)
            .ToArray();
        Assert.True(
            mismatched.Length == 0,
            $"XpeErrorCode value(s) disagree with the header: {string.Join("; ", mismatched)}");
    }

    /// <summary>
    /// #117: the enum declares nothing the header does not. An invented member would make the
    /// traversal test assert a message for a code the native side never returns.
    /// </summary>
    [Fact]
    public void Enum_DeclaresNoCodeAbsentFromHeader()
    {
        var header = ReadHeaderCodes();
        var managed = ReadEnumCodes();

        var extra = managed.Keys.Where(k => !header.ContainsKey(k)).OrderBy(k => k).ToArray();
        Assert.True(
            extra.Length == 0,
            $"XpeErrorCode declares code(s) absent from {HeaderRelativePath}: {string.Join(", ", extra)}");
    }

    /// <summary>
    /// Resolves the header through the repository root. A missing header FAILS rather than skips:
    /// silently skipping would restore the blind spot this class exists to remove.
    /// </summary>
    private static string ResolveHeaderPath()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, HeaderRelativePath.Replace('/', Path.DirectorySeparatorChar));
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        Assert.Fail(
            $"Could not locate {HeaderRelativePath} by walking up from {AppContext.BaseDirectory}. " +
            "This test reads the native header directly and must not be run outside the repository tree.");
        return string.Empty; // unreachable
    }
}
