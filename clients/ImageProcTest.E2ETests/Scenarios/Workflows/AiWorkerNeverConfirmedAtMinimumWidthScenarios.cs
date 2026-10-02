// #225 (GUI-C-192d): the notice for an AI worker that never answers from the start, at the narrowest window, without a native module.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The app is launched with <c>--automation-fault ai-worker-silent:0</c>: the worker's status read never answers, not even once. No "Active"
/// is ever shown, so this is the case the 192c withdrawal cannot cover. For the first seconds there is no notice (a start-up with no answer yet
/// is normal); a whole freshness bound (15 s) after the read began the notice must be on screen, say that it has not been confirmed SINCE
/// START, and stay in the automation tree at the window's minimum width. Real time passes here; the state is injected.
/// </summary>
[Collection(AiWorkerMuteApplicationCollection.Name)]
public sealed class AiWorkerNeverConfirmedAtMinimumWidthScenarios(AiWorkerMuteApplicationFixture app, ITestOutputHelper output)
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
    public void M03_TheNeverConfirmedNotice_IsAbsentAtStart_ThenAppears_AndIsInTheTreeAtTheMinimumWidth()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        FlaUI.Core.AutomationElements.AutomationElement? Banner() => window.FindFirstDescendant(cf => cf.ByAutomationId("AiWorkerBanner"));
        FlaUI.Core.AutomationElements.AutomationElement? Restart() => window.FindFirstDescendant(cf => cf.ByAutomationId("AiRestartButton"));

        var started = DateTime.UtcNow;
        Assert.Null(Banner());
        Thread.Sleep(TimeSpan.FromSeconds(5));
        Assert.True(Banner() is null,
            $"A notice was on screen {(DateTime.UtcNow - started).TotalSeconds:F1} s after launch, before the freshness bound could have run out; fault '{WorkbenchObservation.FaultInjectionStatus(window)}'.");
        output.WriteLine($"M03 no notice {(DateTime.UtcNow - started).TotalSeconds:F1} s after launch");

        Assert.True(PollFor(() => Banner() is not null, TimeSpan.FromSeconds(40)),
            $"The worker never answered but no notice appeared; fault '{WorkbenchObservation.FaultInjectionStatus(window)}'.");
        output.WriteLine($"M03 notice after {(DateTime.UtcNow - started).TotalSeconds:F1} s: '{Banner()!.Name}'");
        Assert.Contains("no answer since the AI session started", Banner()!.Name, StringComparison.Ordinal);

        var minimum = WindowMinimumWidth.ResizeToMinimum(window);
        output.WriteLine($"M03 minimum width: {minimum.Describe()}");
        Assert.True(minimum.Problem is null, "The window is not at its minimum width, so this run says nothing about it: " + minimum.Describe());

        var bannerAtMinimum = PollFor(() => Banner() is not null, TimeSpan.FromSeconds(5));
        var restartAtMinimum = PollFor(() => Restart() is not null, TimeSpan.FromSeconds(5));
        Assert.True(bannerAtMinimum && restartAtMinimum,
            $"At the window's minimum width ({minimum.Final} px) the notice is not in the automation tree: banner {(bannerAtMinimum ? "found" : "NOT found")}, " +
            $"Restart AI {(restartAtMinimum ? "found" : "NOT found")}. {minimum.Describe()}");
        Assert.Contains("no answer since the AI session started", Banner()!.Name, StringComparison.Ordinal);
    }
}
