// #206 (GUI-C-139): how much of the log must be READ to find the alerts in it. Observation only.
using System;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// <c>#206</c> says the severity of a log line is carried by TEXT alone — no colour, no icon, no filter —
/// so an alert cannot be told from an ordinary line at a glance. That is a claim about a reader's cost,
/// and it had exactly ONE sample behind it: GUI-C-132 observed 6 alert lines among 44.
///
/// <para><b>What this file measures, and the criterion, are fixed before the numbers</b>
/// (<c>.moai/reports/lane-gui/GUI-C-139/report.md</c> §2, committed in a separate earlier commit so the
/// order is checkable). The share of alert lines is NOT the criterion: it throws away position, and a
/// statistic that throws away position cannot see layout — 6 alerts bunched at the top of 44 lines and 6
/// spread through them are the same percentage and a completely different reading task.</para>
///
/// <para>The four numbers, per condition:</para>
/// <list type="bullet">
/// <item><b>C1 page</b> — how many lines the list shows without scrolling, MEASURED from the rendered
/// geometry rather than derived from <c>MaxHeight</c>. This is the threshold, and it comes from the UI
/// rather than from a number this lane picked.</item>
/// <item><b>C2 newest</b> — lines from the top down to the first <c>ALERT</c> line: the cost of "is
/// there an alert?".</item>
/// <item><b>C3 all</b> — lines down to the OLDEST alert: the cost of "have I seen them all?". This is
/// the number the criterion judges.</item>
/// <item><b>C4 gap</b> — the longest run of non-alert lines between alerts: what the eye has to cross.</item>
/// </list>
///
/// <para><b>Judgment rule, from the report:</b> bounded (C3 ≤ C1 in every condition, and C3 staying
/// inside C1 as the run lengthens) reads as a design whose cost does not grow with use; C3 exceeding C1
/// in any realistic condition, or growing with run length, reads as a defect. No absolute line count is
/// used as the bar — GUI-C-138 measured what happens when this lane invents a threshold: the one derived
/// from a model went red on a legitimate correction.</para>
///
/// <para><b>Nothing here asserts what the app SHOULD do.</b> The assertions are instrument checks: that
/// the log was actually read, and that the conditions differ from each other. Direction and cost are the
/// report's, and implementation is a later card.</para>
/// </summary>
public sealed class LogAlertDiscriminabilityObservation(ITestOutputHelper output)
{
    /// <summary>The marker <c>RaiseAlert</c> stamps on every alert line it writes (GUI-C-125).</summary>
    private const string AlertMarker = "ALERT ";

    /// <summary>How many extra chain runs the "frequent" condition asks for.</summary>
    private const int ExtraRuns = 8;

    /// <summary>
    /// Condition A — the control: an app with no frame loaded. Its job is to prove the reading path is
    /// live before any conclusion is drawn from the other conditions: a measurement that returns zero
    /// lines produces "no alerts found" just as convincingly as a log with none in it.
    /// </summary>
    [SkippableFact]
    public void A_AnAppWithNoFrame_ShowsWhatTheReadingPathReturns()
    {
        using var app = new ApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);
        var measured = Measure(window, "A no frame");

        // The instrument check: lines came back at all. Without this the other conditions could report
        // "C3 = 0, comfortably inside the page" from a driver that read nothing.
        Assert.True(measured.Lines > 0,
            $"CONTROL FAILED: the log read back {measured.Lines} lines, so no condition in this file can "
          + "tell 'few alerts' from 'the list was never read'.");
    }

    /// <summary>
    /// Conditions B and C in one app, because C must be the SAME run grown longer — that is what the
    /// criterion asks (does the cost stay inside the page as the log lengthens?). Two separate apps would
    /// compare two histories instead of one history at two lengths.
    /// </summary>
    [SkippableFact]
    public void BandC_OneRunMeasuredShortThenLong_ShowsWhetherTheCostGrows()
    {
        using var app = new WorkflowApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        var rare = Measure(window, "B after load");

        // Re-run the chain. Which command is available depends on the backend: preprocessing needs the
        // native one plus a calibration set (CanRunPreprocessing), the display pipeline does not. Both
        // are "the user asked for the work again", which is the path #199 says produces an alert every
        // time on this host — the suppression never engages because each run calls the shutdown function.
        // Sampled after EVERY run, not only at the end: the judgment turns on whether the cost GROWS,
        // and two endpoints cannot tell a step from a slope.
        var ran = 0;
        for (var i = 0; i < ExtraRuns; i++)
        {
            if (Invoke(window, "RunPreprocessingMenuItem") || Invoke(window, "ApplyDisplayPipelineMenuItem"))
            {
                ran++;
                Thread.Sleep(250);
                Measure(window, $"C run {ran}");
            }
        }

        output.WriteLine($"extra chain runs invoked: {ran} of {ExtraRuns}");
        var frequent = Measure(window, "C after re-runs");

        // Instrument check: the log actually grew, so "C3 did not move" would mean the cost is bounded
        // rather than that nothing happened.
        Assert.True(frequent.Lines > rare.Lines,
            $"CONTROL FAILED: the log stayed at {frequent.Lines} lines after {ran} re-runs (was "
          + $"{rare.Lines}), so this run cannot tell a bounded cost from an idle harness.");
    }

    // ---- measurement ---------------------------------------------------------------------------

    private sealed record Observation(int Page, int Lines, int Alerts, int Newest, int All, int Gap);

    private Observation Measure(Window window, string label)
    {
        var page = PageSize(window);
        var lines = LogLines(window);
        var alertAt = Enumerable.Range(0, lines.Length)
            .Where(i => lines[i].Contains(AlertMarker, StringComparison.Ordinal))
            .ToArray();

        // Logs.Insert(0, …) puts the newest line first, so index 0 is the top of the list and a depth is
        // simply "index + 1" — the number of lines a reader passes to reach it.
        var newest = alertAt.Length is 0 ? -1 : alertAt[0] + 1;
        var all = alertAt.Length is 0 ? -1 : alertAt[^1] + 1;
        var gap = 0;
        var previous = -1;
        foreach (var at in alertAt)
        {
            gap = Math.Max(gap, at - previous - 1);
            previous = at;
        }

        var observation = new Observation(page, lines.Length, alertAt.Length, newest, all, gap);
        output.WriteLine(
            $"{label,-18} C1 page={observation.Page,3}  lines={observation.Lines,3}  alerts={observation.Alerts,3}  "
          + $"C2 newest={observation.Newest,3}  C3 all={observation.All,3}  C4 gap={observation.Gap,3}  "
          + $"share={(observation.Lines is 0 ? 0 : 100.0 * observation.Alerts / observation.Lines):0.0}%  "
          + $"C3<=C1? {(observation.All < 0 ? "n/a" : observation.All <= observation.Page ? "yes" : "NO")}");
        return observation;
    }

    /// <summary>
    /// How many lines the list shows without scrolling, from the rendered geometry: the list's own height
    /// divided by one item's height. Measured rather than computed from <c>MaxHeight="600"</c> and
    /// <c>FontSize="11"</c> — those would give a number this lane derived, and the whole point of using
    /// the page as the threshold is that the UI supplies it.
    ///
    /// <para>Returns -1 when the geometry cannot be read (no items yet, or a zero-height container), so a
    /// missing measurement is visible in the table instead of silently becoming a large or small page.</para>
    /// </summary>
    private static int PageSize(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        var items = list?.FindAllChildren();
        if (list is null || items is null || items.Length is 0)
        {
            return -1;
        }

        var listHeight = list.BoundingRectangle.Height;
        var itemHeight = items[0].BoundingRectangle.Height;
        return itemHeight <= 0 || listHeight <= 0 ? -1 : listHeight / itemHeight;
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is null ? [] : list.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
    }

    /// <summary>Invokes a menu item and says whether it was there and enabled, rather than throwing.</summary>
    private static bool Invoke(Window window, string automationId)
    {
        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("PipelineMenu"));
        if (menu is null)
        {
            return false;
        }

        menu.Patterns.ExpandCollapse.Pattern.Expand();
        Thread.Sleep(200);

        var item = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        if (item is null || !item.IsEnabled)
        {
            // Leave the menu closed so the next probe starts from the same state as this one.
            menu.Patterns.ExpandCollapse.Pattern.Collapse();
            Thread.Sleep(100);
            return false;
        }

        item.AsMenuItem().Invoke();
        Thread.Sleep(200);
        return true;
    }

    /// <summary>
    /// The same sequence <c>ClearAlertsObservationScenarios</c> uses, deliberately not re-derived: a
    /// first attempt here clicked the menu elements instead of driving their patterns and skipped the
    /// <c>Log</c> tab, and the log read back ZERO lines. The control in case A is what caught that —
    /// without it this file would have reported "0 alerts, comfortably inside the page".
    ///
    /// <para>The tab click at the end is load-bearing: the log panel's visibility is a MultiBinding over
    /// <c>AnalysisTab</c> AND <c>ShowLogsPanel</c> (<c>AnalysisPanel.xaml:472-478</c>), so toggling the
    /// panel on while another tab is selected shows nothing.</para>
    /// </summary>
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
