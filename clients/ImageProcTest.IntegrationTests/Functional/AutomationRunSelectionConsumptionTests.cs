// #136: a run-selection switch that nothing consumes is worse than a missing switch.
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Every run-selection field of <see cref="AutomationArgs"/> is actually consumed by
/// <c>MainWindow.ApplyRunSelection</c>.
///
/// This exists because the same defect shipped twice. <c>--automation-backend</c> (GUI-C-31) and
/// <c>--automation-calib</c> (GUI-C-37) were each parsed, validated, stored on <c>App</c> — and then
/// applied only inside the isolation branch of <c>CreateSettings</c>, which an E2E launch never
/// enters. Both ran green: the switch was accepted, the app started, the report looked like the
/// requested one, and the option did nothing. GUI-C-38 unified the two branches so one
/// <c>ApplyRunSelection</c> serves every launch mode; this guard is what keeps the NEXT switch from
/// re-opening the same hole.
///
/// It is a source-text guard because the failure is an absence: no runtime observation distinguishes
/// "applied and made no difference" from "never applied".
/// </summary>
[Trait("Category", "Functional")]
public sealed class AutomationRunSelectionConsumptionTests
{
    private const string MainWindowRelativePath = "gui/ImageProcTest/MainWindow.xaml.cs";

    /// <summary>
    /// Properties that are NOT run selection, with the reason each is excluded.
    ///
    /// Named rather than inferred: an inferred rule would silently absorb a new property, which is
    /// the exact failure this class exists to catch.
    /// </summary>
    private static readonly Dictionary<string, string> NotRunSelection = new(StringComparer.Ordinal)
    {
        ["RawPath"] = "names the frame to load; consumed by the automation scenario, not by settings",
        ["ReportPath"] = "names where the report is written; consumed by App, not by settings",
        ["Error"] = "the rejection reason; a run carrying one never starts",
    };

    /// <summary>
    /// Each run-selection property has a matching <c>App.Automation&lt;Name&gt;</c> read inside
    /// <c>ApplyRunSelection</c>.
    ///
    /// Failing here means the switch parses and is stored but changes nothing about the run.
    /// </summary>
    [Fact]
    public void EveryRunSelectionField_IsAppliedByApplyRunSelection()
    {
        var body = ReadApplyRunSelectionBody();

        var unconsumed = RunSelectionProperties()
            .Where(name => !body.Contains($"App.Automation{name}", StringComparison.Ordinal))
            .Order()
            .ToArray();

        Assert.True(
            unconsumed.Length == 0,
            $"These AutomationArgs fields are parsed but never applied in {MainWindowRelativePath}: " +
            $"{string.Join(", ", unconsumed)}. Expected a read of App.Automation<Name> inside " +
            "ApplyRunSelection — a switch nothing consumes runs green while doing nothing.");
    }

    /// <summary>
    /// The exclusion list names only properties that still exist.
    ///
    /// Without this, renaming a property would leave a stale exclusion behind and the renamed
    /// property would be silently treated as run selection — or, worse, a genuine run-selection field
    /// could be parked in the list under a name nobody checks.
    /// </summary>
    [Fact]
    public void ExclusionList_NamesOnlyPropertiesThatExist()
    {
        var actual = AllProperties().ToHashSet(StringComparer.Ordinal);
        var stale = NotRunSelection.Keys.Where(name => !actual.Contains(name)).Order().ToArray();

        Assert.True(
            stale.Length == 0,
            $"These names are excluded from the run-selection check but are not AutomationArgs " +
            $"properties: {string.Join(", ", stale)}. Rename or remove the stale entries.");
    }

    /// <summary>
    /// The check covers at least the two switches whose absence was the original defect. A guard
    /// that quietly reduced to zero properties would still pass every other assertion here.
    /// </summary>
    [Fact]
    public void RunSelection_CoversTheTwoSwitchesThatRegressed()
    {
        var selection = RunSelectionProperties().ToHashSet(StringComparer.Ordinal);

        Assert.Contains("BackendMode", selection);
        Assert.Contains("CalibrationDirectory", selection);
    }

    private static IEnumerable<string> AllProperties() =>
        typeof(AutomationArgs)
            .GetProperties(BindingFlags.Public | BindingFlags.Instance)
            .Select(p => p.Name);

    /// <summary>Every property except the named non-selection ones.</summary>
    private static IEnumerable<string> RunSelectionProperties() =>
        AllProperties()
            .Where(name => !NotRunSelection.ContainsKey(name))
            // Computed conveniences over the other properties, not values of their own.
            .Where(name => name is not ("IsAutomationMode" or "IsValid"));

    /// <summary>Returns the body of ApplyRunSelection, brace-balanced.</summary>
    private static string ReadApplyRunSelectionBody()
    {
        var source = File.ReadAllText(ResolveRepositoryFile(MainWindowRelativePath));
        var signature = Regex.Match(source, @"void\s+ApplyRunSelection\s*\([^)]*\)\s*\{");

        Assert.True(
            signature.Success,
            $"ApplyRunSelection was not found in {MainWindowRelativePath}. If it was renamed, this " +
            "guard must be pointed at whatever now applies the run selection.");

        var depth = 0;
        var body = new StringBuilder();
        for (var i = signature.Index + signature.Length - 1; i < source.Length; i++)
        {
            var c = source[i];
            if (c == '{')
            {
                depth++;
                if (depth == 1) continue;
            }
            else if (c == '}')
            {
                depth--;
                if (depth == 0) return body.ToString();
            }

            body.Append(c);
        }

        Assert.Fail("ApplyRunSelection has unbalanced braces.");
        return string.Empty; // unreachable
    }

    /// <summary>
    /// Walks up from the test output directory to find a repository file. A missing file FAILS
    /// rather than skips — a skip would restore the blind spot this class removes.
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
