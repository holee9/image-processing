// #196 / #198 (GUI-C-132, comments refreshed in GUI-C-136): the no-op alert reaches the screen, per frame.
using System;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// What the shipped preprocess path does about the nonlinearity stage, and the per-frame alert delivery this scenario used to measure with it.
///
/// <para><b>What this scenario used to claim (#196 / #198, GUI-C-132).</b> The app called <c>xpe_nonlinearity_correct(image, null)</c> on its own, with no LUT loaded, and the module answered with a
/// no-op alert (<c>nonlinearity correction did nothing</c>), one per frame; the scenario asserted that line reached the log three times, next to the gain-clamp alert (#194) as the control that the
/// queue was alive. The requirement behind the module's behaviour is REQ-P1A-107 (SPEC-XPE-P1A, PRE-08): <i>when the stage runs with no LUT and <c>panel.linear</c> is not "false", it reports that it did
/// nothing</i>. That is a MODULE contract, verified by <c>test_nonlin_noop_report.cpp</c>; no GUI-level requirement, hazard or RTM row cites this scenario or says the application must invoke the stage
/// (searched: SPEC-XPE-P1A, docs/project, docs/design, the gui README).</para>
///
/// <para><b>What changed (GUI-C-232, leader decision SRS-CALIB-SAFE-004).</b> The application's one preprocess path is now <c>xpe_preprocess_pipeline_out</c> with <c>bypassNonlinearity:true</c>
/// (<c>GuiPreprocessRunner.PipelineConfigJson</c>): the app has no linearity table to give the stage, so it does not run it. A stage that does not run reports nothing, so the no-op alert cannot
/// be raised from the application any more, and should not be: the alert exists to tell "ran and did nothing" from "ran and corrected", and "did not run" is now said by the application itself -
/// the HUD's <c>Calib:</c> line and the run's result text name the stages that ran (GUI-C-233k, <c>GuiPreprocessRunner.ExecutedStages</c>).</para>
///
/// <para><b>What is asserted now.</b> (1) Preprocessing ran and the HUD names exactly offset, gain and defect; (2) no #196 line appears, which is read only because (3) the control is alive in
/// the same run: the per-frame gain-clamp alert (#194, its message carries that frame's count) arrived once per frame. Without (3) an empty #196 count is also what a dead queue shows. The #198 (2)
/// delivery claim (a native alert reaches the screen) is also covered by <c>NativeAlertScenarios</c>; the per-frame part is kept here through the control.</para>
///
/// <para><b>Not re-measured:</b> the module-side no-op alert itself (it is reachable only by a caller that runs the stage without <c>bypassNonlinearity</c>; the module's own test covers it).</para>
/// </summary>
public sealed class NonlinearityBypassedScenarios(ITestOutputHelper output)
{
    /// <summary>From <c>nonlinearity_correct.cpp</c> - the no-op report (#196). Raised only when the stage runs, which the shipped path does not do.</summary>
    private const string NoopMarker = "nonlinearity correction did nothing";

    /// <summary>From <c>gain_correct.cpp</c> - per-frame, the control (#194).</summary>
    private const string ClampMarker = "fell outside the gain polynomial";

    /// <summary>The HUD line after a render whose preprocess stage ran (<c>MainWindowViewModel.CalibStageCountDisplay</c>).</summary>
    private const string ExecutedStagesHud = "3 stages (offset · gain · defect)";

    private const int Frames = 3;

    /// <summary>GUI-C-234b (U3): preprocessing is on by default, so the render that opens the frame preprocesses it too: one more preprocess run, one more per-frame alert, before the first Run Preprocessing.</summary>
    private const int RenderThatOpenedTheFrame = 1;

    [SkippableFact]
    public void TheShippedPath_RunsNoNonlinearityStage_SoNoNoopAlertIsRaised_WhileThePerFrameAlertKeepsArriving()
    {
        using var app = new PolynomialCalibrationApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        // The control alert comes from a native stage; the Mock backend has none, so under Mock this
        // measures nothing. Skipping says "not measured" rather than passing on a path that is absent.
        Skip.If(app.BackendMode != "Native", "The control alert is pushed by a native preprocess stage.");
        Skip.If(app.PolynomialCalibrationDirectory is null, app.PolynomialNote);

        var window = app.MainWindow!;
        output.WriteLine($"calibration: {app.PolynomialNote}");

        OpenLogs(window);

        // The log list is virtualised: UI Automation sees the lines that are on screen, and since GUI-C-234 every render also logs a long (wrapping) enhance line, so one read at the end sees only the newest
        // few. The newest line is first, so each frame's lines are collected right after the frame and the scenario counts over the union.
        var seen = new List<string>();
        for (var i = 1; i <= Frames; i++)
        {
            RunPreprocessing(window);
            output.WriteLine($"frame {i}: preprocessing invoked");
            foreach (var line in LogLines(window))
            {
                if (!seen.Contains(line)) seen.Add(line);
            }
        }

        var lines = seen.ToArray();
        var noop = lines.Where(t => t.Contains(NoopMarker, StringComparison.Ordinal)).ToArray();
        var clamp = lines.Where(t => t.Contains(ClampMarker, StringComparison.Ordinal)).ToArray();
        var preprocess = lines.Where(t => t.Contains("Preprocess", StringComparison.Ordinal)).ToArray();
        var hud = window.FindFirstDescendant(cf => cf.ByAutomationId("HudCalibStages"))?.Name ?? string.Empty;

        // Observation before assertions: a red run must say what was seen, not only that it failed.
        output.WriteLine($"log lines: {lines.Length}");
        output.WriteLine($"preprocess lines: {preprocess.Length}");
        output.WriteLine($"HUD calibration line: '{hud}'");
        output.WriteLine($"#196 no-op lines: {noop.Length}");
        output.WriteLine($"#194 clamp lines: {clamp.Length}");
        foreach (var l in clamp.Take(4)) output.WriteLine($"  [194] {l}");

        Skip.If(preprocess.Length == 0, "Preprocessing did not run, so no stage could have raised an alert.");

        // (1) The application says which stages ran, and the nonlinearity stage is not among them.
        Assert.Contains(ExecutedStagesHud, hud, StringComparison.Ordinal);

        // (3) The control, first: the queue is alive in this same run and delivers per frame. Without it the absence in (2) would not mean anything.
        Assert.True(clamp.Length == Frames + RenderThatOpenedTheFrame,
            $"{Frames} Run Preprocessing frames (plus the render that opened the frame) produced {clamp.Length} per-frame #194 clamp line(s), expected {Frames + RenderThatOpenedTheFrame}: the alert queue is not shown to deliver once per frame and the #196 count below cannot be read.");

        // (2) The stage did not run, so it did not report.
        Assert.True(noop.Length == 0,
            $"{noop.Length} #196 no-op line(s) reached the log although the shipped path bypasses the nonlinearity stage (bypassNonlinearity). Either the stage runs again - then the HUD and the " +
            "result text must say so - or the line comes from somewhere else.");
    }

    private static void RunPreprocessing(Window window)
    {
        window.SetForeground();
        Thread.Sleep(200);
        var item = window.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem"));
        if (item is null)
        {
            var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
            Assert.True(menu is not null, "PipelineMenu is not in the tree.");
            menu!.Patterns.ExpandCollapse.Pattern.Expand();
            Thread.Sleep(350);
            item = window.FindFirstDescendant(cf => cf.ByAutomationId("RunPreprocessingMenuItem"));
        }

        Assert.True(item is not null, "RunPreprocessingMenuItem is not in the tree.");
        item!.AsMenuItem().Invoke();
        Thread.Sleep(3000);
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is null ? [] : list.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
    }

    private static void OpenLogs(Window window)
    {
        var view = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
        Assert.True(view is not null, "ViewMenu is not in the tree.");
        view!.Patterns.ExpandCollapse.Pattern.Expand();
        Thread.Sleep(300);

        var toggle = window.FindFirstDescendant(cf => cf.ByAutomationId("ShowLogsPanelMenuItem"));
        Assert.True(toggle is not null, "ShowLogsPanelMenuItem is not in the tree.");
        if (toggle!.Patterns.Toggle.Pattern.ToggleState != FlaUI.Core.Definitions.ToggleState.On)
        {
            toggle.Patterns.Toggle.Pattern.Toggle();
            Thread.Sleep(200);
        }

        view.Patterns.ExpandCollapse.Pattern.Collapse();
        Thread.Sleep(200);
        window.FindFirstDescendant(cf => cf.ByName("Log"))?.AsButton().Invoke();
        Thread.Sleep(600);
    }
}
