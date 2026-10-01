// #225 (GUI-C-191b): the "AI worker switched off" mark at the narrowest window, in the UI, without a native module.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The app is launched with <c>--automation-fault ai-worker-disabled</c>: the AI worker status read answers "switched off, 3 of 3", so
/// the mark that C-09 can only look at on a native run is on screen on any run. The window is then put at the smallest width it accepts
/// (checked, not assumed: <see cref="WindowMinimumWidth"/>) and the banner and the Restart AI button must be in the automation tree.
/// They were in a ToolBar, whose overflow popup takes them out of the tree below about 1400 px (GUI-C-191); restoring that layout turns
/// this red (GUI-C-191b falsification). The state is injected, so what is observed here is the mark's PLACEMENT, not the module.
/// </summary>
[Collection(AiWorkerDisabledApplicationCollection.Name)]
public sealed class AiWorkerMarkAtMinimumWidthScenarios(AiWorkerDisabledApplicationFixture app, ITestOutputHelper output)
{
    private static bool PollFor(Func<bool> condition, TimeSpan limit)
    {
        var deadline = DateTime.UtcNow + limit;
        while (!condition())
        {
            if (DateTime.UtcNow >= deadline)
            {
                return false;
            }

            Thread.Sleep(100);
        }

        return true;
    }

    [SkippableFact]
    public void M01_TheMark_IsInTheAutomationTree_AtTheLaunchWidth_AndAtTheMinimumWidth()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        FlaUI.Core.AutomationElements.AutomationElement? Banner() => window.FindFirstDescendant(cf => cf.ByAutomationId("AiWorkerBanner"));
        FlaUI.Core.AutomationElements.AutomationElement? Restart() => window.FindFirstDescendant(cf => cf.ByAutomationId("AiRestartButton"));

        // The injected status reaches the screen after the image the launch loads (the read follows the first chain).
        var launchWidth = (int)window.BoundingRectangle.Width;
        Assert.True(PollFor(() => Banner() is not null, TimeSpan.FromSeconds(20)),
            $"The injected 'worker switched off' state never put the mark on screen at the launch width ({launchWidth} px); " +
            $"fault status: '{WorkbenchObservation.FaultInjectionStatus(window)}'.");
        output.WriteLine($"M01 launch width {launchWidth}: banner '{Banner()!.Name}'");

        var minimum = WindowMinimumWidth.ResizeToMinimum(window);
        output.WriteLine($"M01 minimum width: {minimum.Describe()}");
        Assert.True(minimum.Problem is null, "The window is not at its minimum width, so this run says nothing about it: " + minimum.Describe());

        var bannerAtMinimum = PollFor(() => Banner() is not null, TimeSpan.FromSeconds(5));
        var restartAtMinimum = PollFor(() => Restart() is not null, TimeSpan.FromSeconds(5));
        Assert.True(bannerAtMinimum && restartAtMinimum,
            $"At the window's minimum width ({minimum.Final} px) the mark is not in the automation tree: banner {(bannerAtMinimum ? "found" : "NOT found")}, " +
            $"Restart AI {(restartAtMinimum ? "found" : "NOT found")}. {minimum.Describe()}");
        Assert.Contains("switched off", Banner()!.Name, StringComparison.Ordinal);
    }
}
