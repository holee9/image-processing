// #201 (a) / GUI-C-136 단계 A: press Clear Alerts and record what the user sees. Observation only.
using System;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// <c>#201</c>(a) says pressing <c>Clear Alerts</c> changes nothing the user can see: the command runs
/// <c>Alerts.Clear()</c> on a collection no element is bound to, while the <c>ALERT …</c> lines
/// GUI-C-125 put in the Application Log stay. The issue recorded that as READ FROM CODE, not observed —
/// so this case observes it before anything is fixed.
///
/// <para><b>The existing automation evidence cannot answer this.</b> <c>MainWindow.xaml.cs:378-384</c>
/// presses <c>ClearLogsButton</c> AND <c>ClearAlertsButton</c> and then records both counts at zero, so
/// the report's <c>AlertCountAfterClear == 0</c> says nothing about which button did it. This case
/// presses <b>only</b> Clear Alerts.</para>
///
/// <para><b>Why the control is a POSITIVE one.</b> The card asked for a zero-alert arm as the control,
/// but "no visible change" there is indistinguishable from "the harness never pressed anything" — the
/// same reading both arms produce. So the control here presses <c>Clear Logs</c> in the same run and
/// requires the log to actually shrink: that proves the harness can press a button and that the
/// observation is live. Without it, this file would pass on a broken driver.</para>
///
/// <para><b>Observation only.</b> Nothing is asserted about what SHOULD happen — the numbers are printed
/// and the claim is the narrow one the issue makes. Direction (a dedicated Alerts surface, or unifying
/// on the log) is not this card's to choose.</para>
///
/// <para><b>Its own app instance, not the shared collection fixture.</b> The positive control EMPTIES
/// the log, and every other case in the shared collection reads that log — GUI-C-126 already had a
/// "before == 0" assertion break because a fixture was shared, and a teardown that wipes state is worse
/// than an assertion that reads it. This launches one app for this case and disposes it.</para>
/// </summary>
public sealed class ClearAlertsObservationScenarios(ITestOutputHelper output)
{
    /// <summary>The marker RaiseAlert stamps on every alert it writes to the log (GUI-C-125).</summary>
    private const string AlertMarker = "ALERT ";

    [SkippableFact]
    public void PressingClearAlerts_ChangesNothingTheUserSees_WhileClearLogsDoes()
    {
        using var app = new ApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        var before = LogLines(window);
        var beforeAlerts = before.Count(l => l.Contains(AlertMarker, StringComparison.Ordinal));
        output.WriteLine($"BEFORE  log lines: {before.Length}, alert lines: {beforeAlerts}");

        // Without an alert on screen there is nothing for the button to fail to remove.
        Skip.If(beforeAlerts == 0,
            $"No ALERT line is in the log ({before.Length} lines), so this run cannot observe (a).");

        Press(window, "ClearAlertsButton");

        var afterAlerts = LogLines(window);
        var afterAlertCount = afterAlerts.Count(l => l.Contains(AlertMarker, StringComparison.Ordinal));
        output.WriteLine($"AFTER Clear Alerts  log lines: {afterAlerts.Length}, alert lines: {afterAlertCount}");

        // The positive control, in the same run: a button press that DOES change what is on screen.
        Press(window, "ClearLogsButton");
        var afterLogs = LogLines(window);
        output.WriteLine($"AFTER Clear Logs    log lines: {afterLogs.Length}, " +
                         $"alert lines: {afterLogs.Count(l => l.Contains(AlertMarker, StringComparison.Ordinal))}");

        Assert.True(afterLogs.Length < afterAlerts.Length,
            $"CONTROL FAILED: Clear Logs left the log at {afterLogs.Length} lines (was {afterAlerts.Length}), " +
            "so this run cannot tell 'Clear Alerts did nothing' from 'no button was pressed at all'.");

        // The observation #201(a) predicted.
        Assert.Equal(before.Length, afterAlerts.Length);
        Assert.Equal(beforeAlerts, afterAlertCount);
    }

    /// <summary>
    /// The guard #201 asked for BEFORE (a) is fixed: <c>Clear Alerts</c> must not take the ORDINARY log
    /// lines with it. Written first on purpose — a guard authored after the fix is written to match the
    /// implementation, and this one has to survive the fix rather than describe it.
    ///
    /// <para>Today it passes trivially, because the button changes nothing at all (the case above). Its
    /// job starts the moment the command begins removing lines: if someone implements "clear alerts" as
    /// "clear the log", this is what says no.</para>
    /// </summary>
    [SkippableFact]
    public void PressingClearAlerts_KeepsTheOrdinaryLogLines()
    {
        using var app = new ApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        var before = LogLines(window);
        var ordinaryBefore = before.Where(l => !l.Contains(AlertMarker, StringComparison.Ordinal)).ToArray();
        output.WriteLine($"BEFORE  log lines: {before.Length}, ordinary: {ordinaryBefore.Length}");
        foreach (var l in ordinaryBefore.Take(6)) output.WriteLine($"  ordinary: {l}");

        // Without ordinary lines there is nothing for the button to wrongly remove.
        Skip.If(ordinaryBefore.Length == 0, "The log carries no ordinary line, so this guard measures nothing.");

        Press(window, "ClearAlertsButton");

        var after = LogLines(window);
        var ordinaryAfter = after.Where(l => !l.Contains(AlertMarker, StringComparison.Ordinal)).ToArray();
        output.WriteLine($"AFTER   log lines: {after.Length}, ordinary: {ordinaryAfter.Length}");

        // Every ordinary line that was there must still be there — by content, not by count, so a
        // replacement that keeps the count but swaps the lines cannot pass.
        var missing = ordinaryBefore.Except(ordinaryAfter, StringComparer.Ordinal).ToArray();
        foreach (var l in missing) output.WriteLine($"  MISSING: {l}");

        Assert.True(missing.Length == 0,
            $"Clear Alerts removed {missing.Length} ordinary log line(s). It must only remove the lines " +
            "RaiseAlert wrote (#201 (a)); the Application Log is not its to empty — Clear Logs is a " +
            "different button.");
    }

    private static void Press(Window window, string automationId)
    {
        window.SetForeground();
        Thread.Sleep(150);
        var button = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(button is not null, $"{automationId} is not in the automation tree.");
        Assert.True(button!.IsEnabled, $"{automationId} is disabled, so the press would measure nothing.");
        button.AsButton().Invoke();
        Thread.Sleep(400);
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
