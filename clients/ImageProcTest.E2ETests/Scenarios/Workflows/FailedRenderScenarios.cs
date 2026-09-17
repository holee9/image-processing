// #171 ③ (GUI-C-79): a display pipeline that fails must not leave the old image looking current.
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// W-23: the failure path, driven through the command-line fault seam.
///
/// <para>Neither backend can be made to fail its display pipeline from the UI (GUI-C-78), so this app is
/// started with <c>--automation-fault display-pipeline-after:2</c>: the load (call 1) and one preset change
/// (call 2) render, and every later call throws. The preset change is the control — it must replace the
/// image and leave no indicator — so a version that never moves, or an indicator that is always up,
/// fails here rather than passing.</para>
/// </summary>
[Collection(FaultInjectedApplicationCollection.Name)]
public sealed class FailedRenderScenarios(FaultInjectedApplicationFixture app, ITestOutputHelper output)
{
    [SkippableFact]
    public void W23_FailedRender_MarksTheImageStale()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;

        // The seam is armed, and loudly: the title and the automation status both say so.
        var armed = WaitForFaultCalls(window, 1);
        output.WriteLine($"W23 start: title='{window.Title}' status='{armed}'");
        Assert.Contains("FAULT INJECTION ARMED", window.Title, StringComparison.Ordinal);
        Assert.True(
            armed == "faultInjection=display-pipeline-after:2 calls=1",
            $"Expected the load to be display pipeline call 1 of an armed seam, but the window reports '{armed}'.");

        OpenParameters(window);

        // Control: call 2 still renders, so the image is replaced and nothing is stale.
        var beforePreset = Viewport(window).ProcessedVersion;
        SelectBodyPart(window, BodyPart(window) == "Bone" ? "Lung" : "Bone");
        var afterPreset = WaitForProcessedVersionAbove(window, beforePreset);
        var indicatorAfterPreset = StaleIndicator(window);
        output.WriteLine($"W23 preset: v{beforePreset} -> v{afterPreset} indicator='{indicatorAfterPreset}' status='{FaultInjectionStatus(window)}'");
        Assert.True(afterPreset > beforePreset, "The preset change (call 2) did not replace the processed image; the version cannot be trusted.");
        Assert.True(indicatorAfterPreset is null, $"After a successful render the stale indicator reads '{indicatorAfterPreset}'.");

        // Call 3 fails: the old image stays up, and it must be marked as such.
        ApplyDisplayPipeline(window);
        var status = WaitForFaultCalls(window, 3);
        var afterFailure = Viewport(window).ProcessedVersion;
        var indicator = StaleIndicator(window);
        output.WriteLine($"W23 failed apply: v{afterFailure} indicator='{indicator}' status='{status}'");
        Assert.True(status.EndsWith("calls=3", StringComparison.Ordinal), $"The Apply did not reach the display pipeline (status '{status}').");
        Assert.True(afterFailure == afterPreset, $"A failed render replaced the processed image (v{afterPreset} -> v{afterFailure}).");
        Assert.True(
            indicator is not null && indicator.Contains("pipeline failed", StringComparison.Ordinal),
            $"The display pipeline failed and the image on screen is from before it, but the stale indicator reads " +
            $"'{indicator ?? "(absent)"}'. An out-of-date image with no sign of it is HAZ-GUI-004 (#171 ③).");
    }

    private static string WaitForFaultCalls(Window window, int calls)
    {
        var suffix = $"calls={calls}";
        var last = FaultInjectionStatus(window);
        for (var i = 0; i < 20 && !last.EndsWith(suffix, StringComparison.Ordinal); i++)
        {
            Thread.Sleep(250);
            last = FaultInjectionStatus(window);
        }

        return last;
    }
}

/// <summary>The workflow image, launched with the display pipeline fault armed after two calls.</summary>
public sealed class FaultInjectedApplicationFixture : ApplicationFixture
{
    public FaultInjectedApplicationFixture()
        : base(@"fixtures\gui-s0\raw\synthetic_1024x1024.raw", ["--automation-fault", "display-pipeline-after:2"])
    {
    }
}

/// <summary>Its own collection: a fault-armed app must never be shared with an ordinary scenario.</summary>
[CollectionDefinition(Name)]
public sealed class FaultInjectedApplicationCollection : ICollectionFixture<FaultInjectedApplicationFixture>
{
    public const string Name = "gui-fault-injected-application";
}
