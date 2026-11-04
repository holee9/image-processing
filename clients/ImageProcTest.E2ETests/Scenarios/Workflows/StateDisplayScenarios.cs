using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

public sealed class StateDisplayApplicationFixture : ApplicationFixture, IDisposable
{
    // The explicit second path deliberately withholds maps to exercise Native + Skipped. It creates nothing.
    public StateDisplayApplicationFixture() : base(@"fixtures\gui-s0\raw\synthetic_1024x1024.raw",
        ["--automation-calib", Path.Combine(AppContext.BaseDirectory, "gui-c-258-maps-not-present"), "--automation-settings", SettingsPath()]) { }

    private static string OwnedDirectory => Path.Combine(AppContext.BaseDirectory, "gui-c-258-settings");
    private static string SettingsPath()
    {
        Directory.CreateDirectory(OwnedDirectory);
        return Path.Combine(OwnedDirectory, "appsettings.json");
    }

    void IDisposable.Dispose()
    {
        base.Dispose();
        var full = Path.GetFullPath(OwnedDirectory);
        var allowed = Path.GetFullPath(AppContext.BaseDirectory).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        if (!full.StartsWith(allowed, StringComparison.OrdinalIgnoreCase)) throw new IOException("Test settings cleanup escaped the runner output directory.");
        if (Directory.Exists(full)) Directory.Delete(full, recursive: true);
        if (Directory.Exists(full)) throw new IOException("Test settings directory remains after cleanup.");
    }
}

public sealed class StateDisplayScenarios(StateDisplayApplicationFixture app, ITestOutputHelper output)
    : IClassFixture<StateDisplayApplicationFixture>
{
    [Fact]
    public void D258_ExistingCalibrationMissingMap_IsAnExplicitFailure_WithoutGeneration()
    {
        var missing = Path.Combine(AppContext.BaseDirectory, "gui-c-258-maps-not-present");
        Assert.False(Directory.Exists(missing));
        var error = Assert.Throws<FileNotFoundException>(() => ApplicationFixture.ValidateExistingCalibrationDirectory(missing));
        Assert.Contains("generation is disabled", error.Message);
        Assert.False(Directory.Exists(missing));
    }

    [SkippableFact]
    public void D258_MetricsFooterAndPipelineOrder_AreVisibleAndDistinct()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "Application unavailable.");
        var window = app.MainWindow!;
        AutomationElement Find(string id) => window.FindFirstDescendant(cf => cf.ByAutomationId(id))
            ?? throw new InvalidOperationException($"Missing {id}.");
        var footer = new[] { Find("ChainStatusText").Name, Find("FrameTimingText").Name, Find("StatusBarText").Name };
        output.WriteLine("D258 visible footer: " + string.Join(" | ", footer));
        Assert.All(footer, text => Assert.False(string.IsNullOrWhiteSpace(text)));
        Assert.Equal(3, footer.Distinct().Count());
        Assert.Contains("C ", footer[0]);
        Assert.Contains("ms", footer[1]);
        Assert.Contains("WARN", footer[2]);
        Assert.StartsWith("render #", Find("ChainStatusText").HelpText);
        Assert.Contains("preprocess=", WorkbenchObservation.FullChainText(window));
        var metrics = Find("MetricsRows");
        var texts = metrics.FindAllDescendants().Select(e => e.Name ?? string.Empty).ToArray();
        output.WriteLine("D258 actual Metrics: " + string.Join(" | ", texts));
        Assert.Contains("preprocess", texts);
        Assert.Contains("VOI center", texts);
        Assert.DoesNotContain(texts, t => t.Contains("Metrics populated after pipeline run"));
        var scroll = Find("AnalysisScroll").Patterns.Scroll.Pattern;
        if (scroll.VerticallyScrollable.Value) scroll.SetScrollPercent(-1, 100);
        Thread.Sleep(150);
        var lowerTexts = metrics.FindAllDescendants().Select(e => e.Name ?? string.Empty).ToArray();
        Assert.Contains(lowerTexts, t => t.Contains("EI/DI"));
        foreach (var prefix in new[] { "MetricLabel:", "MetricA:", "MetricB:" })
        {
            var cell = Find(prefix + "Calibration stages");
            output.WriteLine($"D258 actual calibration TextBlock: id={cell.AutomationId}; text={cell.Name}; rect={cell.BoundingRectangle}; offscreen={cell.IsOffscreen}");
            Assert.False(cell.IsOffscreen);
            Assert.True(cell.BoundingRectangle.Width > 0 && cell.BoundingRectangle.Height > 0);
            Assert.Equal(prefix == "MetricLabel:" ? "Calibration stages" : "not run", cell.Name);
        }
        output.WriteLine("D258 lower Metrics after scrolling: " + string.Join(" | ", lowerTexts));
        UiaMenu.Open(window, "PipelineMenu");
        try
        {
            var ids = Find("PipelineMenu").FindAllDescendants().Select(e => e.AutomationId).ToList();
            var expected = new[] { "RunPreprocessingMenuItem", "RunDeterministicBaselineMenuItem", "RunFullPipelineMenuItem", "ApplyDisplayPipelineMenuItem", "UseAutomaticWindowMenuItem", "StopProcessingMenuItem", "StageTimingMenuItem", "OpenPipelineDiagnosticsMenuItem" };
            var positions = expected.Select(id => ids.IndexOf(id)).ToArray();
            output.WriteLine("D258 Pipeline: " + string.Join(", ", ids));
            Assert.All(positions, p => Assert.True(p >= 0));
            Assert.True(positions.SequenceEqual(positions.Order()));
        }
        finally { UiaMenu.CollapseAll(window); }
    }

    [SkippableFact]
    public void D258_NativeSkippedBanner_IsFullyInsideViewport_AndClearOfHudAndToolbar()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "Application unavailable.");
        Skip.If(app.BackendMode != "Native", "This geometry case requires Native + Skipped.");
        var window = app.MainWindow!;
        WorkbenchObservation.AssertWrappedBackendMatches(window, "Native", output.WriteLine, "D258");
        AutomationElement Find(string id) => window.FindFirstDescendant(cf => cf.ByAutomationId(id))
            ?? throw new InvalidOperationException($"Missing {id}.");
        var until = DateTime.UtcNow.AddSeconds(20);
        while (window.FindFirstDescendant(cf => cf.ByAutomationId("PreprocessSkippedIndicator")) is null && DateTime.UtcNow < until)
            Thread.Sleep(100);

        void Measure(string size)
        {
            var text = Find("PreprocessSkippedIndicator");
            Assert.Contains("NOT CORRECTED", text.Name);
            Assert.False(text.IsOffscreen);
            var viewport = Find("WorkbenchViewport").BoundingRectangle;
            var banner = Find("PreprocessSkippedBannerBorder").BoundingRectangle;
            var hud = Find("StudyHud").BoundingRectangle;
            var toolbar = Find("ViewportToolbar").BoundingRectangle;
            var content = text.BoundingRectangle;
            output.WriteLine($"D258 {size}: viewport={viewport}; banner={banner}; text={content}; study={hud}; toolbar={toolbar}; fullText={text.Name}");
            Assert.True(banner.Width > 0 && banner.Height > 0 && content.Width > 0 && content.Height > 0);
            Assert.True(viewport.Contains(banner), "Banner must be inside the viewport.");
            Assert.True(banner.Contains(content), "Full wrapped text must fit the banner.");
            Assert.False(banner.IntersectsWith(hud), "Banner overlaps the STUDY HUD.");
            Assert.False(banner.IntersectsWith(toolbar), "Banner overlaps the viewport toolbar.");
        }
        Measure("launch");
        var minimum = WindowMinimumWidth.ResizeToMinimum(window);
        output.WriteLine("D258 minimum: " + minimum.Describe());
        Assert.Null(minimum.Problem);
        Measure("minimum");
    }
}
