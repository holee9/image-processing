// #225 (GUI-C-206): the About dialog's build identity, and the one place the comparison zoom ceiling is written.
using System.Text.RegularExpressions;
using ImageProcTest.Models;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// <para><b>Build identity.</b> The csproj stamps the commit into the informational version as "1.0.0+&lt;sha&gt;" and the configuration as assembly metadata; <see cref="BuildIdentity"/> reads them back.
/// What it must do with a build that has no revision (no git) is say "unknown" — not an empty string, not a value that looks like a commit.</para>
///
/// <para><b>One ceiling.</b> GUI-C-204 changed the Zoom In command's 16.0 and Z01 stayed green, because the settings property held its own copy. The four copies (Zoom In, the settings setter, the
/// viewport's setter and its mouse wheel) now read <see cref="ComparisonZoomLimits.Max"/>; these tests hold that: no 16.0 literal near a zoom in those files, and every site names the constant.</para>
/// </summary>
public sealed class BuildIdentityAndZoomLimitTests
{
    [Theory]
    [InlineData("1.0.0+d700e74be0", "Debug", "d700e74be0", "Debug")]
    [InlineData("1.0.0+abc", "Release", "abc", "Release")]
    [InlineData("1.0.0+unknown", "Debug", "unknown", "Debug")]
    [InlineData("1.0.0", "Debug", "unknown", "Debug")]
    [InlineData("1.0.0+", "Debug", "unknown", "Debug")]
    [InlineData(null, null, "unknown", "unknown")]
    [InlineData("1.0.0+abc", "  ", "abc", "unknown")]
    public void TheIdentity_IsReadFromTheStamp_AndSaysUnknownWhereThereIsNone(string? informational, string? configuration, string revision, string expectedConfiguration)
    {
        var identity = BuildIdentity.From(informational, configuration);
        Assert.Equal(revision, identity.Revision);
        Assert.Equal(expectedConfiguration, identity.Configuration);
        Assert.Equal($"Build: {revision} ({expectedConfiguration})", identity.Describe());
    }

    [Fact]
    public void TheRunningAssembly_HasAStampedIdentity_OrSaysUnknown()
    {
        // The test assembly is not stamped by the app's csproj, so this reads a REAL assembly through the same code path and checks the result is well-formed, never empty.
        var identity = BuildIdentity.FromAssembly(typeof(BuildIdentity).Assembly);
        Assert.False(string.IsNullOrWhiteSpace(identity.Revision));
        Assert.False(string.IsNullOrWhiteSpace(identity.Configuration));
    }

    [Fact]
    public void TheCsproj_StampsTheRevisionAndTheConfiguration_AndAnAbsentGitDoesNotFailTheBuild()
    {
        var csproj = Source("gui/ImageProcTest/ImageProcTest.csproj");
        Assert.Contains("git rev-parse --short=10 HEAD", csproj, StringComparison.Ordinal);
        Assert.Contains("IgnoreExitCode=\"true\"", csproj, StringComparison.Ordinal);
        Assert.Contains("<SourceRevisionId Condition=\"'$(SourceRevisionId)' == ''\">unknown</SourceRevisionId>", csproj, StringComparison.Ordinal);
        Assert.Contains("BuildConfiguration", csproj, StringComparison.Ordinal);
        Assert.Contains("BuildIdentity.Current.Describe()", Source("gui/ImageProcTest/MainWindow.xaml.cs"), StringComparison.Ordinal);
    }

    [Fact]
    public void TheZoomCeiling_Is16_AndIsWrittenOnce()
    {
        Assert.Equal(16.0, ComparisonZoomLimits.Max);

        var sites = new[]
        {
            ("gui/ImageProcTest/ViewModels/MainWindowViewModel.cs", "Math.Min(ComparisonZoomLimits.Max, current * 1.25)"),
            ("gui/ImageProcTest/Models/AppSettings.cs", "Math.Clamp(value, 0.0, ComparisonZoomLimits.Max)"),
            ("gui/ImageProcTest/Controls/ImageComparisonViewport.cs", "Math.Clamp(value, 0.0, ComparisonZoomLimits.Max)"),
            ("gui/ImageProcTest/Controls/ImageComparisonViewport.cs", "Math.Clamp(currentScale * factor, 0.01, ComparisonZoomLimits.Max)"),
        };
        foreach (var (file, expected) in sites)
        {
            var text = Source(file);
            Assert.True(text.Contains(expected, StringComparison.Ordinal), $"{file} no longer reads ComparisonZoomLimits.Max: expected '{expected}'.");
            // No second copy of the number on a zoom line.
            var copies = text.Split('\n').Where(l => l.Contains("Zoom", StringComparison.OrdinalIgnoreCase) && Regex.IsMatch(l, @"\b16(\.0)?\b") && !l.TrimStart().StartsWith("//") && !l.TrimStart().StartsWith("///")).ToArray();
            Assert.True(copies.Length == 0, $"{file} writes the zoom ceiling as a literal again: {string.Join(" | ", copies.Select(c => c.Trim()))}");
        }
    }

    private static string Source(string relative) =>
        File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(relative));
}
