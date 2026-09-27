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
/// The second alert #198 names. <c>nonlinearity_correct.cpp</c> reports that the stage did nothing —
/// no LUT is loaded and <c>panel.linear</c> is not "false", so the frame leaves the stage
/// byte-identical and silence could not be told apart from a correction that ran.
///
/// <para><b>Why this could not be measured before.</b> An early return
/// (<c>if (!configJsonOrNull) return XPE_OK;</c>) sat in front of the alert, and the gui passes
/// <c>config</c> as null, so the alert was unreachable from here — module-side, not a wiring fault.
/// QA-A-140 removed that guard; this case is the last leg, from the native queue to the screen.</para>
///
/// <para><b>ONE PER FRAME, and that is now the design.</b> The module used to latch the report on
/// <c>g_calib.nonlin_noop_reported</c>; <c>QA-A-142</c> (<c>f772b31</c>) REMOVED that latch, and the
/// stated reason is the pair of facts this lane measured: the host drains the queue in a <c>finally</c>
/// after every native call (<c>NativeAlertDrain.cs:109</c>), so nothing is ever evicted and the feared
/// 64-entry overflow does not happen; and <c>GuiPreprocessRunner</c> wraps each run in
/// <c>init → … → finally shutdown</c>, where <c>xpe_preprocess_shutdown</c> clears every module global
/// (#176) and put the latch back anyway. Measured there: 20 calls → 20 alerts, a suppression rate of
/// zero. De-duplicating an identical line was handed to whoever renders the log.</para>
///
/// <para><b>The control is still the load-bearing half.</b> A count that matches the frames is also
/// what a queue delivering nothing new would produce if the lines came from somewhere else. So the same
/// run must show another alert still arriving — the #194 clamp, which is per-frame because its message
/// carries that frame's count. Two independent alerts arriving per frame, in one log, is what separates
/// "the stage reports every frame" from "the queue died and these lines are stale".</para>
///
/// <para><b>What GUI-C-132 measured, and what became of it.</b> This case found the latch not holding
/// across frames and traced it to a third re-arm path the module's comment did not list —
/// <c>xpe_preprocess_shutdown</c> (<c>preprocess.cpp:77</c>) resetting <c>g_calib</c> wholesale. That
/// measurement is what <c>QA-A-142</c> acted on: the latch was removed rather than made to hold, because
/// holding it across this host's pattern would need state that outlives <c>shutdown</c>, which #176
/// forbids. So the count below is unchanged, and its reason is now simpler — the module reports every
/// frame by design.</para>
///
/// <para><b>Not re-measured against a post-removal binary.</b> <c>f772b31</c> is not in the CI artifacts
/// this lane has staged (the run carrying it had not finished), so the numbers here come from the
/// latch-era build. They survive the change because both designs produce one line per frame in THIS
/// host — the latch's suppression rate here was zero (measured 20/20 by QA-A-142). Worth re-running once
/// a build with <c>f772b31</c> is staged.</para>
/// </summary>
public sealed class NonlinearityNoopAlertScenarios(ITestOutputHelper output)
{
    /// <summary>From <c>nonlinearity_correct.cpp</c> — the no-op report (#196).</summary>
    private const string NoopMarker = "nonlinearity correction did nothing";

    /// <summary>From <c>gain_correct.cpp</c> — per-frame, the control (#194).</summary>
    private const string ClampMarker = "fell outside the gain polynomial";

    private const int Frames = 3;

    [SkippableFact]
    public void TheNoopAlert_ReachesTheLogEveryFrame_WhileOtherAlertsKeepArriving()
    {
        using var app = new PolynomialCalibrationApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        // Both alerts come from native stages; the Mock backend has neither, so under Mock this
        // measures nothing. Skipping says "not measured" rather than passing on a path that is absent.
        Skip.If(app.BackendMode != "Native", "Both alerts are pushed by native preprocess stages.");
        Skip.If(app.PolynomialCalibrationDirectory is null, app.PolynomialNote);

        var window = app.MainWindow!;
        output.WriteLine($"calibration: {app.PolynomialNote}");

        OpenLogs(window);

        // Several frames through the same stage. The gui never loads a nonlinearity LUT, so the no-op
        // condition holds on every one of them and the stage reports it every time (QA-A-142 removed the
        // suppression latch; before that, this host re-armed it on every run anyway).
        for (var i = 1; i <= Frames; i++)
        {
            RunPreprocessing(window);
            output.WriteLine($"frame {i}: preprocessing invoked");
        }

        var lines = LogLines(window);
        var noop = lines.Where(t => t.Contains(NoopMarker, StringComparison.Ordinal)).ToArray();
        var clamp = lines.Where(t => t.Contains(ClampMarker, StringComparison.Ordinal)).ToArray();
        var preprocess = lines.Where(t => t.Contains("Preprocess", StringComparison.Ordinal)).ToArray();

        // Observation before assertions: a red run must say what was seen, not only that it failed.
        output.WriteLine($"log lines: {lines.Length}");
        output.WriteLine($"preprocess lines: {preprocess.Length}");
        output.WriteLine($"#196 no-op lines: {noop.Length}");
        foreach (var l in noop) output.WriteLine($"  [196] {l}");
        output.WriteLine($"#194 clamp lines: {clamp.Length}");
        foreach (var l in clamp.Take(4)) output.WriteLine($"  [194] {l}");

        Skip.If(preprocess.Length == 0, "Preprocessing did not run, so neither alert could be raised.");

        // (a) It reaches the screen at all.
        Assert.True(noop.Length > 0,
            $"The nonlinearity stage ran {Frames} time(s) with no LUT loaded and no #196 line reached " +
            $"the log. Lines carrying ALERT: {lines.Count(t => t.Contains("ALERT", StringComparison.Ordinal))}.");

        // It arrives as an alert (GUI-C-125) from the native queue (GUI-C-126).
        Assert.Contains("ALERT", noop[0], StringComparison.Ordinal);
        Assert.Contains("NATIVE_ALERT", noop[0], StringComparison.Ordinal);

        // (b1) One line per frame. GUI-C-132 measured this while the module still carried a
        // suppression latch — and found it did not survive a frame here, because
        // `xpe_preprocess_shutdown` (preprocess.cpp:77) assigns `g_calib = CalibrationData{}` and put
        // the latch back on every run. QA-A-142 then removed the latch, citing that measurement: holding
        // it across this host's pattern would need state outliving shutdown, which #176 forbids.
        //
        // So the number is unchanged and its reason is now simply the design. It still fails at 1 (a
        // suppression that DOES survive a run — worth noticing, and a module-side change) and at
        // 2×Frames (double-firing).
        Assert.True(noop.Length == Frames,
            $"{Frames} frames produced {noop.Length} #196 line(s). The stage reports the no-op on every " +
            "frame (QA-A-142 removed the suppression latch), so one line per frame is the contract. A " +
            "count of 1 means suppression came back — tell the module owner rather than editing this " +
            "number.");

        // (b2) The control — the queue is alive in this same run. Without it, a #196 count that matches
        // the frames could equally come from a queue that stopped delivering while these lines sat
        // there, so the count would not be attributable to the stage reporting at all.
        Assert.True(clamp.Length > 1,
            $"Only {clamp.Length} per-frame #194 clamp line(s) arrived across {Frames} frames, so " +
            "the #196 count cannot be attributed to the stage reporting rather than to a dead queue.");
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
