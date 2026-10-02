// #225 row 10 (GUI-C-191): where the "AI worker switched off" mark sits in the window.
using System.Xml.Linq;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// The mark (banner + Restart AI) was the last item of the toolbar. A WPF ToolBar sends the items that do not fit its width to an
/// overflow popup, where they are neither on screen nor in the UI Automation tree; at the app's own minimum width (1280) the mark
/// was there in the XAML and nowhere on screen (measured: banner and Restart AI NOT FOUND at 1280, found at 1400 and above). These
/// read MainWindow.xaml, so they see the markup's structure, not a rendered window; the rendering was observed once with the real
/// app (report GUI-C-191) and is exercised on every native run by C-09, which runs at the minimum width.
/// </summary>
[Trait("Category", "Functional")]
public sealed class AiWorkerPanelLayoutTests
{
    private static readonly XNamespace Presentation = "http://schemas.microsoft.com/winfx/2006/xaml/presentation";

    private static XDocument LoadWindow() =>
        XDocument.Load(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/MainWindow.xaml"));

    private static XElement ByAutomationId(XDocument document, string id) =>
        document.Descendants().Single(element =>
            (string?)element.Attributes().FirstOrDefault(a => a.Name.LocalName == "AutomationProperties.AutomationId")?.Value == id);

    [Theory]
    [InlineData("AiWorkerPanel")]
    [InlineData("AiWorkerBanner")]
    [InlineData("AiRestartButton")]
    public void TheMarkAndItsButton_AreNotInsideAToolBar_WhereTheOverflowCanTakeThemOffTheScreen(string id)
    {
        var element = ByAutomationId(LoadWindow(), id);

        Assert.DoesNotContain(element.Ancestors(), ancestor => ancestor.Name.LocalName == "ToolBar");
        Assert.DoesNotContain(element.Ancestors(), ancestor => ancestor.Name.LocalName == "ToolBarTray");
    }

    [Fact]
    public void TheBannerAndTheButton_AreInsideThePanel_AndThePanelStartsCollapsedUntilTheMarkIsVisible()
    {
        var document = LoadWindow();
        var panel = ByAutomationId(document, "AiWorkerPanel");

        Assert.Contains(ByAutomationId(document, "AiWorkerBanner").Ancestors(), ancestor => ancestor == panel);
        Assert.Contains(ByAutomationId(document, "AiRestartButton").Ancestors(), ancestor => ancestor == panel);

        // Collapsed by default, Visible only on the module's "off"/"could not start" (the binding is the view model's mark flag).
        var style = panel.Descendants().First(e => e.Name.LocalName == "Style");
        var setter = style.Elements(Presentation + "Setter").Single(e => (string?)e.Attribute("Property") == "Visibility");
        Assert.Equal("Collapsed", (string?)setter.Attribute("Value"));
        var trigger = style.Descendants(Presentation + "DataTrigger").Single();
        Assert.Equal("{Binding AiWorkerMarkVisible}", (string?)trigger.Attribute("Binding"));
        Assert.Equal("Visible", (string?)trigger.Elements(Presentation + "Setter").Single().Attribute("Value"));
    }

    /// <summary>
    /// C-09 is the only place the mark is rendered with a real module, so it runs at the narrowest width, waits for the banner as an event
    /// (no fixed 2.5 s sleep), and says what it READ when it fails (a source reading of the scenario).
    /// </summary>
    [Fact]
    public void C09_RunsAtTheMinimumWidth_WaitsForTheBannerAsAnEvent_AndItsFailureSaysWhatWasRead()
    {
        var source = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.E2ETests/Scenarios/Workflows/ProcessingChainScenarios.cs"));
        var start = source.IndexOf("public void C09_AWorkerSwitchedOffByRepeatedFailures_ShowsAMark_ThatRestartRemoves()", StringComparison.Ordinal);
        var end = source.IndexOf("private static bool PollFor(", start, StringComparison.Ordinal);
        Assert.True(start >= 0 && end > start, "C-09 was not found.");
        var c09 = source[start..end];

        Assert.Contains("var minimum = WindowMinimumWidth.ResizeToMinimum(window);", c09, StringComparison.Ordinal);
        Assert.Contains("Assert.True(minimum.Problem is null,", c09, StringComparison.Ordinal);       // a window that is not at its minimum fails the run
        Assert.Contains("ResizeTo(window, originalWidth)", c09, StringComparison.Ordinal);            // and puts the shared window back
        Assert.Contains("(after the chain text: {bannerAfterChain})", c09, StringComparison.Ordinal);  // the time to the banner, every attempt
        Assert.Contains("shown = PollFor(() => AiBanner(window) is not null, TimeSpan.FromMilliseconds(2500));", c09, StringComparison.Ordinal);
        Assert.DoesNotContain("Thread.Sleep(2500)", c09, StringComparison.Ordinal);
        Assert.DoesNotContain("the module never reported the worker switched off", c09, StringComparison.Ordinal);
        Assert.Contains("The app's own status says the worker is switched off", c09, StringComparison.Ordinal);
        Assert.Contains("the app never read the worker as switched off", c09, StringComparison.Ordinal);

        // The assertions the scenario exists for are still there (nothing was loosened to make it green).
        Assert.Contains("Assert.True(banner is not null,", c09, StringComparison.Ordinal);
        Assert.Contains("Assert.Equal(numbers.Groups[2].Value, numbers.Groups[1].Value);", c09, StringComparison.Ordinal);
        Assert.Contains("Assert.Matches(@\"^worker=Active; failures=0; ceiling=\\d+$\", after);", c09, StringComparison.Ordinal);
    }

    [Fact]
    public void ThePanel_SitsInTheWindowsOwnDock_AsARowOfItsOwn()
    {
        var panel = ByAutomationId(LoadWindow(), "AiWorkerPanel");

        // A direct child of the window's DockPanel, docked to the top: its own row under the toolbar, as wide as the window.
        Assert.Equal("DockPanel", panel.Parent!.Name.LocalName);
        Assert.Equal("Top", (string?)panel.Attribute("DockPanel.Dock"));
    }
}
