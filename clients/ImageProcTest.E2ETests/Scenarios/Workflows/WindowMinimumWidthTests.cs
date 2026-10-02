// #225 (GUI-C-191b): the minimum-width check itself, without an app. Pure arithmetic and the verdict rule.
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

public sealed class WindowMinimumWidthTests
{
    /// <summary>The app's 1280 DIP in the pixels UIA reports: 96 dpi = 100 %, 120 = 125 %, 144 = 150 %, 192 = 200 %.</summary>
    [Theory]
    [InlineData(96u, 1280)]
    [InlineData(120u, 1600)]
    [InlineData(144u, 1920)]
    [InlineData(192u, 2560)]
    public void TheAppsMinWidth_IsConvertedWithTheWindowsDpi(uint dpi, int expectedPixels)
    {
        Assert.Equal(expectedPixels, WindowMinimumWidth.ExpectedPixels(1280, dpi));
    }

    [Fact]
    public void AWindowThatStopsWhereTheMinWidthSays_AndStaysThere_HasNoProblem()
    {
        Assert.Null(WindowMinimumWidth.ProblemWith(effective: 1280, expected: 1280, final: 1280));
        Assert.Null(WindowMinimumWidth.ProblemWith(effective: 1281, expected: 1280, final: 1279));   // within the 2 px a frame can add
    }

    [Fact]
    public void AWindowThatStopsSomewhereElse_IsAProblem_WhichIsWhatAWideRunOrAWrongDpiLooksLike()
    {
        // The window refused to shrink (or was resized to something else): it is NOT at the app's minimum, so a pass proves nothing.
        var wide = WindowMinimumWidth.ProblemWith(effective: 1560, expected: 1280, final: 1560);
        Assert.NotNull(wide);
        Assert.Contains("1560", wide, StringComparison.Ordinal);
        Assert.Contains("1280", wide, StringComparison.Ordinal);

        // High DPI, but the expectation was made at 96: the same mismatch.
        Assert.NotNull(WindowMinimumWidth.ProblemWith(effective: 1600, expected: 1280, final: 1600));
    }

    [Fact]
    public void AWindowThatDriftsAwayFromItsMinimum_IsAProblem()
    {
        var drift = WindowMinimumWidth.ProblemWith(effective: 1280, expected: 1280, final: 1400);
        Assert.NotNull(drift);
        Assert.Contains("did not stay", drift, StringComparison.Ordinal);
    }

    [Fact]
    public void TheAppsMinWidth_IsReadFromMainWindowXaml_NotCopiedHere()
    {
        Assert.Equal(1280.0, WindowMinimumWidth.ReadMinWidthDip());
    }
}
