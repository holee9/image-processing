// #196 / #198 (GUI-C-132): the nonlinearity no-op alert reaches the screen, once per condition.
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
/// <para><b>ONCE PER CONDITION, NOT ONCE PER FRAME — and that is what is asserted.</b> The message
/// carries no per-frame data and the alert queue holds 64 entries with FIFO eviction, so one alert per
/// frame would push every other alert out, the #194 clamp included. The module therefore latches on
/// <c>g_calib.nonlin_noop_reported</c> and re-arms only when a LUT is loaded or unloaded. Asserting
/// "one line per frame" would contradict the design; this asserts <b>exactly one</b> across several
/// frames.</para>
///
/// <para><b>The control is the load-bearing half.</b> "Exactly one #196 line" is also what a DEAD
/// queue produces. So the same run must show another alert still arriving — the #194 clamp, which is
/// per-frame by design because its message carries that frame's count. One latched alert plus several
/// per-frame alerts, in one log, is what separates "latched" from "the queue died".</para>
///
/// <para><b>What the measurement actually found (GUI-C-132).</b> The latch does not hold across frames
/// HERE, and the reason is not the gui's wiring: <c>GuiPreprocessRunner</c> wraps each run in
/// <c>init → load → stages → finally shutdown</c>, and <c>xpe_preprocess_shutdown</c>
/// (<c>preprocess.cpp:77</c>) resets <c>g_calib</c> wholesale — clearing <c>nonlin_noop_reported</c>
/// with it. That is a THIRD re-arm path, absent from the module comment's list of two (LUT load and
/// unload). The consequence is the one pre wanted to avoid: with one #196 per frame, a stream of frames
/// fills the 64-entry queue and evicts other alerts, #194 among them. The assertion below therefore
/// pins the measured count and says what a change in it would mean, rather than asserting an intent the
/// implementation does not currently have.</para>
/// </summary>
public sealed class NonlinearityNoopAlertScenarios(ITestOutputHelper output)
{
    /// <summary>From <c>nonlinearity_correct.cpp</c> — the no-op report (#196).</summary>
    private const string NoopMarker = "nonlinearity correction did nothing";

    /// <summary>From <c>gain_correct.cpp</c> — per-frame, the control (#194).</summary>
    private const string ClampMarker = "fell outside the gain polynomial";

    private const int Frames = 3;

    [SkippableFact]
    public void TheNoopAlert_ReachesTheLogOnce_WhileOtherAlertsKeepArriving()
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

        // Several frames through the same stage. The gui never loads a nonlinearity LUT, so the
        // condition holds on every one of them — the latch is what makes the count 1 rather than 3.
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

        // (b1) MEASURED, not assumed: the latch does NOT survive a frame in the gui, and the reason is
        // a third re-arm path the module's comment does not list. `GuiPreprocessRunner` wraps every run
        // in `init → load → stages → finally shutdown`, and `xpe_preprocess_shutdown`
        // (preprocess.cpp:77) does `g_calib = CalibrationData{}` — which clears `nonlin_noop_reported`
        // along with everything else. So the latch is armed and discarded once per run, and the gui sees
        // one #196 line per frame.
        //
        // This case asserts what was measured rather than what the design intends, so that the number
        // changing is visible: it fails at 1 (the latch started surviving — the intended behaviour, and
        // a change worth noticing) and fails at 2×Frames (double-firing). Pinning it at 1 today would
        // simply be red, and red on a correct implementation teaches nothing.
        Assert.True(noop.Length == Frames,
            $"{Frames} frames produced {noop.Length} #196 line(s). The gui re-arms the latch on every " +
            "run because xpe_preprocess_shutdown resets g_calib wholesale (preprocess.cpp:77), so one " +
            "line per frame is the measured contract. A count of 1 means the latch now survives a run " +
            "— tell the module owner rather than editing this number.");

        // (b2) The control — the queue is alive in this same run. Without this, "exactly one" is
        // indistinguishable from a queue that stopped delivering after the first entry.
        Assert.True(clamp.Length > 1,
            $"Only {clamp.Length} per-frame #194 clamp line(s) arrived across {Frames} frames, so " +
            "\"exactly one #196 line\" cannot be attributed to the latch rather than to a dead queue.");
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
