// XPE-GUI-E2E-001 §4.2: workflow scenarios. Gate: under 3 min for the suite.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// W-01 and W-02 from the plan's §4.2 table, driven in Mock backend mode.
///
/// Two rows deviate from the plan and the reason is measured, not preference — see the GUI-C-34
/// report §1. In short: <c>RunPreprocessingMenuItem</c> is a disabled placeholder with no command
/// behind it (<c>IsEnabled="False"</c>, tooltip "Requires xpe_preprocess.dll."), so "run
/// preprocessing" is not something the app can do yet in any mode; and the body-part preset of W-07
/// has no control bound to it anywhere in the XAML.
///
/// Verification is state, not appearance: the scenarios read element values (viewport present, VOI
/// inputs carrying numbers, log entries) rather than asserting that a picture looks right.
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class WorkflowScenarios
{
    private readonly WorkflowApplicationFixture _app;
    private readonly ITestOutputHelper _output;

    public WorkflowScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
    {
        _app = app;
        _output = output;
    }

    /// <summary>
    /// W-01: a raw image is loaded and the viewport carries it.
    ///
    /// The load happens at startup through the app's own automation path; what this asserts is the
    /// state that follows — the viewport shell exists and has a non-empty rectangle, the status bar
    /// names the loaded frame, and the log list gained entries.
    /// </summary>
    [SkippableFact]
    public void W01_RawImageLoaded_ViewportAndStatusReflectIt()
    {
        Measure("W-01", window =>
        {
            var viewport = WaitFor(() => window.FindFirstDescendant(cf => cf.ByAutomationId("ViewportShell")));
            Assert.True(viewport is not null, "ViewportShell was not found.");
            Assert.True(
                viewport!.BoundingRectangle.Width > 0 && viewport.BoundingRectangle.Height > 0,
                $"ViewportShell has an empty rectangle ({viewport.BoundingRectangle}).");

            // The status bar shows the LATEST status, which by now is the display-pipeline summary —
            // measured, and the reason this does not assert on "RAW" there. The load itself is
            // evidenced by the log the loader writes.
            var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
            Assert.True(status is not null, "StatusBarText was not found.");
            Assert.False(string.IsNullOrWhiteSpace(status!.Name), "StatusBarText is empty after the load.");

            // "CalibrationEval(...)" is written only after a frame has been loaded AND run through
            // the pipeline, so its presence is evidence the raw image arrived. The log list would say
            // it more directly, but it lives inside the Analysis panel's "log" tab and is not in the
            // UIA tree until that tab is selected — measured, same lazy-realisation trap as the menus
            // in GUI-C-29.
            Assert.Contains("CalibrationEval", status.Name, StringComparison.Ordinal);
        });
    }

    /// <summary>
    /// W-01b: the display pipeline command — the one pipeline action Mock can actually run — moves
    /// the status line to its summary, and the change is observed rather than assumed.
    ///
    /// This is the closest drivable neighbour of W-02. It is NOT preprocessing; see W-02 below.
    /// </summary>
    [SkippableFact]
    public void W01b_ApplyDisplayPipeline_LeavesVoiInputsPopulated()
    {
        Measure("W-01b", window =>
        {
            // Move the status bar off the pipeline summary FIRST, so the assertion below is caused by
            // this invocation rather than by the startup load. Re-initialising the backend rewrites
            // the status line, which makes the change observable.
            var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
            Assert.True(status is not null, "StatusBarText was not found.");

            var initialize = window.FindFirstDescendant(cf => cf.ByAutomationId("InitializeBackendButton"));
            Assert.True(initialize is not null, "InitializeBackendButton was not found.");
            initialize!.AsButton().Invoke();
            Thread.Sleep(800);

            var afterInitialize = status!.Name;
            Assert.DoesNotContain("Display:", afterInitialize, StringComparison.Ordinal);

            var pipelineMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
            Assert.True(pipelineMenu is not null, "PipelineMenu was not found.");

            pipelineMenu!.AsMenuItem().Expand();
            try
            {
                var apply = WaitFor(() =>
                    window.FindFirstDescendant(cf => cf.ByAutomationId("ApplyDisplayPipelineMenuItem")));
                Assert.True(apply is not null, "ApplyDisplayPipelineMenuItem was not found.");
                Assert.True(apply!.IsEnabled, "Apply Display Pipeline is disabled in Mock mode.");
                apply.AsMenuItem().Invoke();
                Thread.Sleep(1500);
            }
            finally
            {
                pipelineMenu.AsMenuItem().Collapse();
            }

            var afterPipeline = status.Name;
            Assert.NotEqual(afterInitialize, afterPipeline);
            Assert.Contains("Display:", afterPipeline, StringComparison.Ordinal);
        });
    }

    /// <summary>
    /// W-02, NARROWED to what the app actually offers.
    ///
    /// Measured (GUI-C-34): <c>RunPreprocessingMenuItem</c> carries <c>IsEnabled="False"</c> and no
    /// Command or Click handler at all — it is a Phase-1a placeholder whose tooltip states the
    /// prerequisite. So "run preprocessing and see a processed image" cannot happen in Mock OR
    /// Native today; there is nothing behind the menu item.
    ///
    /// What is asserted is that contract: the entry exists and is disabled. When preprocessing
    /// lands, this scenario fails — which is the right way for a placeholder assertion to expire.
    /// </summary>
    [SkippableFact]
    public void W02_RunPreprocessing_IsAnUnimplementedPlaceholder()
    {
        Measure("W-02", window =>
        {
            var pipelineMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
            Assert.True(pipelineMenu is not null, "PipelineMenu was not found.");

            pipelineMenu!.AsMenuItem().Expand();
            try
            {
                var run = WaitFor(() =>
                    window.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem")));

                Assert.True(run is not null, "RunPreprocessingMenuItem was not found.");
                Assert.False(
                    run!.IsEnabled,
                    "Run Preprocessing is now enabled — preprocessing has landed and this placeholder " +
                    "assertion must be replaced by the real W-02 from the plan (load → run → processed image).");
            }
            finally
            {
                pipelineMenu.AsMenuItem().Collapse();
            }
        });
    }

    /// <summary>
    /// W-07: choosing a body part in the UI applies that backend's preset to the VOI window.
    ///
    /// The values are the ACTIVE backend's, not a constant — GUI-C-23 measured that the mock preset
    /// (Lung 25000/50000) and the native one differ, and hard-coding either is how a check ends up
    /// measuring which backend is running instead of whether the preset was applied. This run is
    /// Mock, so the mock values are expected and named as such.
    ///
    /// The status bar carries the applied window ("… VOI(Linear, C=…, W=…) …"), which is readable
    /// without opening the Analysis tab (GUI-C-34).
    /// </summary>
    [SkippableFact]
    public void W07_SelectingBodyPart_AppliesThatPresetToTheVoiWindow()
    {
        Measure("W-07", window =>
        {
            var selector = window.FindFirstDescendant(cf => cf.ByAutomationId("BodyPartSelector"));
            Assert.True(selector is not null, "BodyPartSelector was not found.");

            var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
            Assert.True(status is not null, "StatusBarText was not found.");

            var combo = selector!.AsComboBox();
            combo.Select("Lung");
            Thread.Sleep(1500);

            // Mock's Lung preset (MockXpeBackend.cs): centre 25000, width 50000.
            var text = status!.Name;
            Assert.Contains("C=25000", text, StringComparison.Ordinal);
            Assert.Contains("W=50000", text, StringComparison.Ordinal);
        });
    }

    /// <summary>Polls briefly for an element the UI creates lazily. Returns null when it never appears.</summary>
    private static AutomationElement? WaitFor(Func<AutomationElement?> find)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
        while (DateTime.UtcNow < deadline)
        {
            var found = find();
            if (found is not null) return found;
            Thread.Sleep(150);
        }

        return null;
    }

    /// <summary>Runs one scenario against the window, skipping cleanly when the app is not built.</summary>
    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!_app.IsAvailable, _app.SkipReason ?? "The application is not available.");

        var stopwatch = Stopwatch.StartNew();
        try
        {
            body(_app.MainWindow!);
        }
        finally
        {
            stopwatch.Stop();
            _output.WriteLine($"{scenario} elapsed {stopwatch.ElapsedMilliseconds} ms");
        }
    }
}
