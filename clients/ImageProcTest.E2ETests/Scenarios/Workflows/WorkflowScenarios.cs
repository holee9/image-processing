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

            var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
            Assert.True(status is not null, "StatusBarText was not found.");
            Assert.False(string.IsNullOrWhiteSpace(status!.Name), "StatusBarText is empty after the load.");

            // Re-run the pipeline rather than reading whatever the status bar happens to hold.
            // Measured: with W-02 wired (GUI-C-36) it runs first under xUnit's ordering and leaves a
            // preprocess line there, which made this scenario fail on residue — the order dependency
            // flagged as a risk in GUI-C-34/35 actually biting. A scenario must establish the state
            // it asserts.
            InvokePipelineItem(window, "ApplyDisplayPipelineMenuItem");
            Thread.Sleep(1500);

            // "CalibrationEval(...)" is written only after a frame has been loaded AND run through
            // the pipeline, so its presence is evidence the raw image arrived and is usable. The log list would say
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
                    pipelineMenu.FindFirstDescendant(cf => cf.ByAutomationId("ApplyDisplayPipelineMenuItem"))
                    ?? window.FindFirstDescendant(cf => cf.ByAutomationId("ApplyDisplayPipelineMenuItem")));
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
    /// W-02: Run Preprocessing, per backend.
    ///
    /// Mock has no preprocess module, so the entry stays disabled and the tooltip says why — the
    /// contract GUI-C-34 pinned, kept because it is still the truth for that backend.
    ///
    /// Native: the entry is now wired (GUI-C-36) and invoking it must produce a preprocess line.
    /// WHICH line depends on whether a calibration set exists: the fixtures are generated at run
    /// time by xpe_calib_fixture_gen and none is staged here, so today the observable outcome is the
    /// refusal ("calibration file(s) not found"). Both outcomes are asserted through the same
    /// substring, and the report names the success path as unverified rather than pretending.
    /// </summary>
    [SkippableFact]
    public void W02_RunPreprocessing_MatchesTheBackendItRunsOn()
    {
        Measure("W-02", window =>
        {
            var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
            Assert.True(status is not null, "StatusBarText was not found.");

            var pipelineMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
            Assert.True(pipelineMenu is not null, "PipelineMenu was not found.");

            pipelineMenu!.AsMenuItem().Expand();
            try
            {
                var run = WaitFor(() =>
                    pipelineMenu.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem"))
                    ?? window.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem")));
                Assert.True(run is not null, "RunPreprocessingMenuItem was not found.");

                if (_app.BackendMode != "Native")
                {
                    Assert.False(
                        run!.IsEnabled,
                        "Run Preprocessing is enabled on the Mock backend, which has no preprocess module.");
                    return;
                }

                Assert.True(run!.IsEnabled, "Run Preprocessing is disabled on the native backend.");

                // #141: without a calibration set the only observable outcome is the refusal, and a
                // scenario that accepts it would report "measured and fine" for something it never
                // measured. Not measured is a SKIP.
                Skip.If(_app.CalibrationDirectory is null, _app.CalibrationNote);

                run.AsMenuItem().Invoke();
            }
            finally
            {
                try { pipelineMenu.AsMenuItem().Collapse(); } catch (Exception) { /* popup already closed by Invoke */ }
            }

            Thread.Sleep(3000);

            // Success-only. GUI-C-36 asserted a substring both the success and the refusal line
            // share, which its own report flagged as a risk: the scenario would keep passing once
            // calibration existed without ever proving the stages ran.
            var text = status!.Name;
            Assert.Contains("offset -> gain -> defect", text, StringComparison.Ordinal);
            Assert.DoesNotContain("skipped", text, StringComparison.OrdinalIgnoreCase);

            // The corrected frame must reach the viewport, not just the log.
            var viewport = WaitFor(() => window.FindFirstDescendant(cf => cf.ByAutomationId("ViewportShell")));
            Assert.True(viewport is not null, "ViewportShell was not found after preprocessing.");
            Assert.True(
                viewport!.BoundingRectangle.Width > 0,
                "ViewportShell is empty after preprocessing.");
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

            // The expected numbers ARE backend-specific, and the first version of this scenario
            // asserted the mock ones unconditionally — which passed locally (Mock) and failed in the
            // CI Native job. Reading them from the active backend is what the comment always
            // claimed and the code did not do.
            var (center, width) = _app.BackendMode == "Native"
                ? ("C=-600", "W=1600")      // modules/display/src/voi_lut.cpp XPE_BODY_LUNG (HU)
                : ("C=25000", "W=50000");   // MockXpeBackend.cs XpeBodyPartEnum.Lung

            var text = status!.Name;
            Assert.Contains(center, text, StringComparison.Ordinal);
            Assert.Contains(width, text, StringComparison.Ordinal);
        });
    }

    /// <summary>Expands the Pipeline menu, invokes one of its items, and closes the menu again.</summary>
    private static void InvokePipelineItem(Window window, string automationId)
    {
        var pipelineMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
        Assert.True(pipelineMenu is not null, "PipelineMenu was not found.");

        pipelineMenu!.AsMenuItem().Expand();
        try
        {
            var item = WaitFor(() =>
                pipelineMenu.FindFirstDescendant(cf => cf.ByAutomationId(automationId))
                ?? window.FindFirstDescendant(cf => cf.ByAutomationId(automationId)));

            Assert.True(item is not null, $"{automationId} was not found.");
            Assert.True(item!.IsEnabled, $"{automationId} is disabled.");
            item.AsMenuItem().Invoke();
        }
        finally
        {
            try { pipelineMenu.AsMenuItem().Collapse(); } catch (Exception) { /* closed by Invoke */ }
        }
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

                // GUI-C-51: a re-acquired window is reported, not silently swallowed. The fixture replaces
        // an element that cannot answer WPF properties (GUI-C-50); before this the replacement left
        // no trace in the run's record, so a run that hit the defect looked exactly like one that
        // did not. Written before the body so it survives a scenario that throws.
        if (!string.IsNullOrEmpty(_app.ReacquiredNote))
        {
            _output.WriteLine($"{scenario} fixture: {_app.ReacquiredNote}");
        }

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
