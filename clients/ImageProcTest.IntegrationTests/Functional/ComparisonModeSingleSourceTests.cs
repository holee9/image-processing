// #161 (GUI-C-60): the three places a user can read the comparison mode must say the same thing.
using System.Text.Json;
using System.Text.RegularExpressions;
using ImageProcTest.Models;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Holds the three readouts of the comparison mode to one answer.
///
/// <para>GUI-C-59 measured them disagreeing. With <c>"NotAMode"</c> in <c>appsettings.json</c> the
/// viewport's HUD said <c>SwipeVertical</c>, the automation <c>HelpText</c> said <c>NotAMode</c>, and
/// every menu check was off — three readings of one setting, because each resolved the string
/// separately. <b>The invalid value exposed the structure; it was not the structure.</b></para>
///
/// <para><b>What each readout actually reads</b>, measured by following the bindings rather than
/// assumed:</para>
///
/// <list type="bullet">
/// <item><b>HUD</b> — <c>ImageComparisonViewport.DrawHud</c> is given
/// <c>NormalizeMode(CompareMode)</c>, and <c>CompareMode</c> is bound two-way to
/// <c>Settings.ComparisonMode</c> (<c>ViewportShell.xaml:18</c>). So: the setting, resolved.</item>
/// <item><b>HelpText</b> — <c>AutomationProperties.HelpText="{Binding Settings.ComparisonMode}"</c>
/// (<c>MainWindow.xaml</c>). So: the setting, raw.</item>
/// <item><b>Menu check</b> — <c>IsChecked</c> compares <c>Settings.ComparisonMode</c> against a
/// literal in each item's <c>ConverterParameter</c>. So: the setting, raw, against a fixed list.</item>
/// </list>
///
/// <para>They read the SAME property. The divergence was that one of the three resolved it and two
/// did not, so the three agreed only while the stored value happened to be resolvable to itself. The
/// fix is therefore not "make the readouts agree" but <b>make the property incapable of holding a
/// value they could disagree about</b> — <see cref="ComparisonModes"/> is that gate.</para>
///
/// <para><b>Both directions are asserted.</b> The invalid case alone would pass if every readout
/// returned one constant, so the supported modes are checked too.</para>
/// </summary>
[Trait("Category", "Functional")]
public sealed class ComparisonModeSingleSourceTests
{
    public static IEnumerable<object[]> SupportedModes() =>
        ComparisonModes.All.Select(mode => new object[] { mode });

    /// <summary>
    /// For every supported mode, all three readouts resolve to that mode.
    ///
    /// The menu is the one readout that can be absent rather than wrong: it carries an item per mode
    /// it lists, and <c>SwipeHorizontal</c> has none (#149 — no entry point was ever specified for
    /// it). That case is reported by <see cref="EveryMenuParameter_IsASupportedMode"/> rather than
    /// hidden here, because "no item is checked" is a different statement from "the wrong item is
    /// checked".
    /// </summary>
    [Theory]
    [MemberData(nameof(SupportedModes))]
    public void EveryReadout_ResolvesToTheSameMode(string mode)
    {
        var settings = new AppSettings { ComparisonMode = mode };

        // HelpText reads the property raw.
        Assert.Equal(mode, settings.ComparisonMode);

        // The HUD reads it through the renderer's resolution.
        Assert.Equal(mode, ComparisonModes.Normalize(settings.ComparisonMode));

        // Nothing was turned away, so nothing is reported.
        Assert.Null(settings.RejectedComparisonMode);
    }

    /// <summary>
    /// An unsupported value is not stored, and it is not swallowed either.
    ///
    /// <para>Kept after the fix on purpose: this input is what made the divergence visible, and a
    /// test that disappears once it passes stops guarding the thing it was written for.</para>
    /// </summary>
    [Theory]
    [InlineData("NotAMode")]
    [InlineData("sourceonly")]          // right mode, wrong case — still not a supported value
    [InlineData("")]
    [InlineData("   ")]
    public void UnsupportedValue_FallsBackAndIsNamed(string stored)
    {
        var settings = new AppSettings { ComparisonMode = stored };

        Assert.Equal(ComparisonModes.Default, settings.ComparisonMode);
        Assert.Equal(ComparisonModes.Default, ComparisonModes.Normalize(settings.ComparisonMode));

        Assert.Equal(stored, settings.RejectedComparisonMode);
    }

    /// <summary>
    /// The gate is on the property, so it also catches what comes out of the settings FILE.
    ///
    /// GUI-C-59 measured the file path end to end by launching the app with a doctored
    /// <c>appsettings.json</c>; this is the part of that measurement that can live in CI, since a
    /// committed test must not rewrite the shipped settings file (the hazard GUI-C-46 named).
    /// </summary>
    [Fact]
    public void AValueFromTheSettingsFile_GoesThroughTheSameGate()
    {
        var settings = JsonSerializer.Deserialize<AppSettings>("""{"comparisonMode": "NotAMode"}""");

        Assert.NotNull(settings);
        Assert.Equal(ComparisonModes.Default, settings!.ComparisonMode);
        Assert.Equal("NotAMode", settings.RejectedComparisonMode);
    }

    /// <summary>
    /// Every mode the View → Compare Mode menu names is a supported mode, and the modes it does NOT
    /// name are reported rather than assumed.
    ///
    /// <para>This is the third readout's half of the claim: the menu compares against literals in
    /// XAML, which no compiler checks. A typo there would silently give a mode that can never be
    /// checked — the same shape as the defect this card closes, one layer out.</para>
    /// </summary>
    [Fact]
    public void EveryMenuParameter_IsASupportedMode()
    {
        var xaml = File.ReadAllText(MainWindowXamlPath());

        var parameters = Regex
            .Matches(xaml, @"SetComparisonModeCommand\}""[\s\S]*?CommandParameter=""([^""]+)""")
            .Select(match => match.Groups[1].Value)
            .ToArray();

        Assert.NotEmpty(parameters);

        var unsupported = parameters.Where(p => !ComparisonModes.IsKnown(p)).ToArray();
        Assert.True(
            unsupported.Length == 0,
            $"View → Compare Mode names {string.Join(", ", unsupported)}, which is not a supported " +
            "comparison mode. A menu item with a name nothing answers to can never show as checked.");

        var withoutAnItem = ComparisonModes.All.Except(parameters, StringComparer.Ordinal).ToArray();
        Assert.Equal(new[] { ComparisonModes.SwipeHorizontal }, withoutAnItem);
    }

    /// <summary>
    /// Locates <c>MainWindow.xaml</c> from the test binary, walking up to the repository root.
    ///
    /// The file is read rather than linked: it is XAML, and the claim is about what the shipped
    /// markup says.
    /// </summary>
    private static string MainWindowXamlPath()
    {
        var directory = new DirectoryInfo(AppContext.BaseDirectory);
        while (directory is not null)
        {
            var candidate = Path.Combine(directory.FullName, "gui", "ImageProcTest", "MainWindow.xaml");
            if (File.Exists(candidate)) return candidate;
            directory = directory.Parent;
        }

        throw new FileNotFoundException(
            "gui/ImageProcTest/MainWindow.xaml was not found above " + AppContext.BaseDirectory);
    }
}
