// #225 (GUI-C-192c): the "AI worker status check delayed" notice at the narrowest window, in the UI, without a native module.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The app is launched with <c>--automation-fault ai-worker-silent</c>: the worker's status read answers "active" once and never again. The
/// refresher asks again after a third of its freshness bound, the read never returns, and at the bound the "Active" is withdrawn to a notice.
/// Before that the notice must be ABSENT (an Active worker and an idle app show nothing), then it must appear, and it must still be in the
/// automation tree at the window's minimum width, as the switched-off mark is (M01). Real time passes here (the bound is 15 s); the state is
/// injected, so what is observed is the notice's appearance and placement, not the module.
/// </summary>
[Collection(AiWorkerSilentApplicationCollection.Name)]
public sealed class AiWorkerUnconfirmedAtMinimumWidthScenarios(AiWorkerSilentApplicationFixture app, ITestOutputHelper output)
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
    public void M02_TheUnconfirmedNotice_IsAbsentWhileTheWorkerIsActive_ThenAppears_AndIsInTheTreeAtTheMinimumWidth()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        FlaUI.Core.AutomationElements.AutomationElement? Banner() => window.FindFirstDescendant(cf => cf.ByAutomationId("AiWorkerBanner"));
        FlaUI.Core.AutomationElements.AutomationElement? Restart() => window.FindFirstDescendant(cf => cf.ByAutomationId("AiRestartButton"));

        // The first read ("active") follows the first chain, and the notice comes a whole freshness bound (15 s) after it. So for the first
        // seconds after the window exists there is no notice: an Active worker, and an app that has only just started, show nothing.
        var started = DateTime.UtcNow;
        Assert.Null(Banner());
        Thread.Sleep(TimeSpan.FromSeconds(5));
        Assert.True(Banner() is null,
            $"A notice was on screen {(DateTime.UtcNow - started).TotalSeconds:F1} s after launch, before the freshness bound could have run out; fault '{WorkbenchObservation.FaultInjectionStatus(window)}'.");
        output.WriteLine($"M02 no notice {(DateTime.UtcNow - started).TotalSeconds:F1} s after launch");

        Assert.True(PollFor(() => Banner() is not null, TimeSpan.FromSeconds(40)),
            $"The worker went silent but no notice appeared; fault '{WorkbenchObservation.FaultInjectionStatus(window)}'.");
        output.WriteLine($"M02 notice after {(DateTime.UtcNow - started).TotalSeconds:F1} s: '{Banner()!.Name}'");
        Assert.Contains("status check delayed", Banner()!.Name, StringComparison.Ordinal);

        var minimum = WindowMinimumWidth.ResizeToMinimum(window);
        output.WriteLine($"M02 minimum width: {minimum.Describe()}");
        Assert.True(minimum.Problem is null, "The window is not at its minimum width, so this run says nothing about it: " + minimum.Describe());

        var bannerAtMinimum = PollFor(() => Banner() is not null, TimeSpan.FromSeconds(5));
        var restartAtMinimum = PollFor(() => Restart() is not null, TimeSpan.FromSeconds(5));
        Assert.True(bannerAtMinimum && restartAtMinimum,
            $"At the window's minimum width ({minimum.Final} px) the notice is not in the automation tree: banner {(bannerAtMinimum ? "found" : "NOT found")}, " +
            $"Restart AI {(restartAtMinimum ? "found" : "NOT found")}. {minimum.Describe()}");
        Assert.Contains("status check delayed", Banner()!.Name, StringComparison.Ordinal);

        // GUI-C-192d: the measurements the native CI log reads travel in the AI checkbox's item status (the diagnostics line). Read here on the
        // fault backend so the wiring is observed on every run, not only on the native one: the notice just raised is counted in it.
        WorkbenchObservation.OpenParameters(window);
        var diagnostics = window.FindFirstDescendant(cf => cf.ByAutomationId("AiBoneSuppressionInChainCheckBox"))?.Properties.ItemStatus.ValueOrDefault ?? string.Empty;
        output.WriteLine($"M02 diagnostics line: '{diagnostics}'");
        Assert.Contains("refresher: reads=", diagnostics, StringComparison.Ordinal);
        Assert.Contains("noticesWithdrawn=1", diagnostics, StringComparison.Ordinal);
    }
}
