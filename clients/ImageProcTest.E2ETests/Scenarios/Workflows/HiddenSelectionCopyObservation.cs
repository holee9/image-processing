// GUI-C-141: with the #206 filter hiding lines, what does Copy put on the clipboard? Observation only.
using System;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// GUI-C-140 gave the log an "Alerts only" filter, which made some lines INVISIBLE for the first time.
/// <c>Copy</c> (GUI-C-123) copies whatever is selected. If a selection survives on a line the filter has
/// hidden, the clipboard receives something that is not on screen — the paste succeeds, and nobody checks
/// whether it was the wanted line. It is the trap GUI-C-123 named, "do not turn a silent no-op into a
/// silent wrong answer", and this time this lane built it.
///
/// <para><b>The criterion was written before these numbers</b>
/// (<c>.moai/reports/lane-gui/GUI-C-141/report.md</c> §1, in its own earlier commit). What decides is
/// question 2 — what lands on the clipboard — NOT whether a selection survives: a surviving selection is
/// harmless if <c>Copy</c> reads only visible items.</para>
///
/// <para><b>Three controls, because the instrument has been wrong twice.</b> GUI-C-139 read the log as 0
/// lines (a re-invented panel-opening sequence), and GUI-C-140 read 81 lines as 27 (virtualization after a
/// view refresh). Both looked like findings. Here: C-1 proves the clipboard can be read at all, C-2 plants
/// a unique string first so "unchanged" can be told from "unreadable", and C-3 proves the list really was
/// filtered — without it there is no hidden line and the question does not exist.</para>
/// </summary>
public sealed class HiddenSelectionCopyObservation(ITestOutputHelper output)
{
    private const string AlertMarker = "ALERT ";

    /// <summary>
    /// C-1 (the control) and questions 1-3 in one app: a plain copy must work before any negative result
    /// here means anything, and the hidden-selection case has to be measured in the same run so the two
    /// share one clipboard and one list.
    /// </summary>
    [SkippableFact]
    public void PlainCopyWorks_ThenWhatHappensWhenTheSelectedLineIsHidden()
    {
        using var app = new WorkflowApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        var lines = LogLines(window);
        var ordinary = lines.FirstOrDefault(l => !l.Contains(AlertMarker, StringComparison.Ordinal));
        var alerts = lines.Count(l => l.Contains(AlertMarker, StringComparison.Ordinal));
        output.WriteLine($"log lines={lines.Length} alerts={alerts} ordinary sample='{Trim(ordinary)}'");
        Skip.If(ordinary is null, "No ordinary log line to select.");
        Skip.If(alerts == 0, "No alert line, so the filter would empty the list and hide nothing selectable.");

        // Question 3, measured rather than read off the XAML.
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))!;
        var multiple = list.Patterns.Selection.TryGetPattern(out var selection)
            ? selection.CanSelectMultiple.Value.ToString()
            : "no selection pattern";
        output.WriteLine($"Q3 CanSelectMultiple={multiple}");

        // ---- C-1: the control. A plain copy, no filter involved. -------------------------------
        var planted = $"XPE-C141-PLANT-{Guid.NewGuid():N}";
        SetClipboard(planted);                                  // C-2: so "unchanged" is distinguishable
        Select(window, ordinary!);
        var copyEnabledPlain = IsEnabled(window, "CopyLogLineButton");
        Press(window, "CopyLogLineButton");
        var afterPlain = GetClipboard();
        output.WriteLine($"C-1 plain copy: button enabled={copyEnabledPlain} clipboard='{Trim(afterPlain)}' status='{Status(window)}'");

        Assert.True(string.Equals(afterPlain, ordinary, StringComparison.Ordinal),
            $"CONTROL C-1 FAILED: a plain copy of the selected line put '{Trim(afterPlain)}' on the "
          + $"clipboard, expected '{Trim(ordinary)}'. Every negative result below would be a broken "
          + "clipboard read rather than an observation about the filter.");

        // ---- Question 1 + 2: hide the selected line, then press Copy. --------------------------
        var planted2 = $"XPE-C141-PLANT-{Guid.NewGuid():N}";
        SetClipboard(planted2);
        SetFilter(window, true);

        var visible = LogLines(window);
        output.WriteLine($"C-3 after filter on: visible={visible.Length} " +
                         $"all alerts={visible.All(l => l.Contains(AlertMarker, StringComparison.Ordinal))}");
        Assert.True(visible.Length > 0 && visible.All(l => l.Contains(AlertMarker, StringComparison.Ordinal)),
            $"CONTROL C-3 FAILED: the filtered list shows {visible.Length} lines and not all are alerts, "
          + "so the selected ordinary line may not be hidden at all.");
        Assert.DoesNotContain(ordinary!, visible);

        var selectedAfterFilter = SelectedLine(window);
        var copyEnabledHidden = IsEnabled(window, "CopyLogLineButton");
        output.WriteLine($"Q1 selection after filter on: uia selected='{Trim(selectedAfterFilter)}' " +
                         $"copy button enabled={copyEnabledHidden}");

        string? afterHidden = null;
        var statusHidden = "(not pressed)";
        if (copyEnabledHidden)
        {
            Press(window, "CopyLogLineButton");
            afterHidden = GetClipboard();
            statusHidden = Status(window);
        }

        var verdict = afterHidden is null ? "button disabled — nothing to press"
            : string.Equals(afterHidden, ordinary, StringComparison.Ordinal) ? "HIDDEN LINE copied"
            : string.Equals(afterHidden, planted2, StringComparison.Ordinal) ? "clipboard unchanged"
            : visible.Contains(afterHidden, StringComparer.Ordinal) ? "a VISIBLE line copied"
            : "something else";
        output.WriteLine($"Q2 clipboard after Copy with a hidden selection: '{Trim(afterHidden)}' " +
                         $"=> {verdict}; status='{statusHidden}'");

        // ---- Question 4: the other direction. --------------------------------------------------
        var alertLine = visible[0];
        Select(window, alertLine);
        SetFilter(window, false);
        var selectedAfterOff = SelectedLine(window);
        output.WriteLine($"Q4 alert selected, filter off: uia selected='{Trim(selectedAfterOff)}' " +
                         $"kept={string.Equals(selectedAfterOff, alertLine, StringComparison.Ordinal)} " +
                         $"copy enabled={IsEnabled(window, "CopyLogLineButton")}");

        // Nothing about what the app SHOULD do is asserted beyond the controls: the report judges, using
        // the rule fixed before the measurement.
    }

    /// <summary>
    /// §3: the order GUI-C-140's condition F did not cover — press <c>Clear Alerts</c> while the filter is
    /// ON. The filter's predicate is membership of the set that command empties, so the list can look as
    /// though everything disappeared. Observation; nothing is claimed to be wrong.
    /// </summary>
    [SkippableFact]
    public void ClearAlerts_PressedWhileTheFilterIsOn_LeavesThisBehind()
    {
        using var app = new WorkflowApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        var before = LogLines(window);
        var alertsBefore = before.Count(l => l.Contains(AlertMarker, StringComparison.Ordinal));
        Skip.If(alertsBefore == 0, $"No alert line in the log ({before.Length} lines).");

        SetFilter(window, true);
        var filtered = LogLines(window);
        Press(window, "ClearAlertsButton");
        var afterClearFiltered = LogLines(window);
        SetFilter(window, false);
        var afterClearUnfiltered = LogLines(window);

        output.WriteLine($"F' unfiltered before={before.Length} (alerts {alertsBefore})");
        output.WriteLine($"F' filtered before Clear Alerts={filtered.Length}");
        output.WriteLine($"F' filtered after  Clear Alerts={afterClearFiltered.Length}");
        output.WriteLine($"F' unfiltered after Clear Alerts={afterClearUnfiltered.Length} " +
                         $"(alerts {afterClearUnfiltered.Count(l => l.Contains(AlertMarker, StringComparison.Ordinal))})");

        // The control: the ordinary lines are still in the log once the filter is off. Without it, "the
        // filtered list emptied" could mean the log itself was wiped.
        Assert.True(afterClearUnfiltered.Length > 0,
            $"CONTROL FAILED: with the filter off the log has {afterClearUnfiltered.Length} lines, so an "
          + "empty filtered list says nothing about the filter.");
    }

    // ---- helpers -------------------------------------------------------------------------------

    private static string Trim(string? s) =>
        s is null ? "(null)" : s.Length <= 60 ? s : s[..57] + "...";

    /// <summary>
    /// Reads the clipboard on a dedicated STA thread. The test runner is MTA and
    /// <c>System.Windows.Clipboard</c> throws there; returning null rather than throwing keeps a clipboard
    /// held by another process from reading as a finding. Control C-1 is what proves this path works.
    /// </summary>
    private static string? GetClipboard() => OnStaThread(() =>
        System.Windows.Clipboard.ContainsText() ? System.Windows.Clipboard.GetText() : null);

    private static void SetClipboard(string text) => OnStaThread(() =>
    {
        System.Windows.Clipboard.SetText(text);
        return text;
    });

    private static string? OnStaThread(Func<string?> action)
    {
        string? result = null;
        Exception? failure = null;
        var thread = new Thread(() =>
        {
            // The clipboard is shared with every process on the machine and can be locked briefly;
            // a few retries beat reporting a lock as an observation.
            for (var attempt = 0; attempt < 5; attempt++)
            {
                try
                {
                    result = action();
                    return;
                }
                catch (Exception ex)
                {
                    failure = ex;
                    Thread.Sleep(120);
                }
            }
        });
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join(TimeSpan.FromSeconds(10));
        return failure is not null && result is null ? null : result;
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is null ? [] : list.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
    }

    /// <summary>The line UIA reports as selected, or null. Read from the item's SelectionItem pattern.</summary>
    private static string? SelectedLine(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        if (list is null)
        {
            return null;
        }

        foreach (var item in list.FindAllChildren())
        {
            if (item.Patterns.SelectionItem.TryGetPattern(out var pattern) && pattern.IsSelected.Value)
            {
                return item.Name;
            }
        }

        return null;
    }

    private static void Select(Window window, string line)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))!;
        var item = list.FindAllChildren().FirstOrDefault(i => string.Equals(i.Name, line, StringComparison.Ordinal));
        Assert.True(item is not null, $"The line to select is not in the list: {Trim(line)}");
        item!.Patterns.SelectionItem.Pattern.Select();
        Thread.Sleep(300);
    }

    private static bool IsEnabled(Window window, string automationId)
    {
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        return element is not null && element.IsEnabled;
    }

    private static string Status(Window window)
    {
        var element = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
        return element?.Name ?? "(no StatusBarText element)";
    }

    private static void Press(Window window, string automationId)
    {
        var button = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(button is not null, $"{automationId} is not in the tree.");
        button!.AsButton().Invoke();
        Thread.Sleep(400);
    }

    /// <summary>Same as GUI-C-140's: the toggle is driven through its pattern and the state is asserted.</summary>
    private static void SetFilter(Window window, bool on)
    {
        var toggle = window.FindFirstDescendant(cf => cf.ByAutomationId("AlertsOnlyFilterToggle"));
        Assert.True(toggle is not null, "AlertsOnlyFilterToggle is not in the tree.");

        var want = on ? FlaUI.Core.Definitions.ToggleState.On : FlaUI.Core.Definitions.ToggleState.Off;
        if (toggle!.Patterns.Toggle.Pattern.ToggleState != want)
        {
            toggle.Patterns.Toggle.Pattern.Toggle();
            Thread.Sleep(400);
        }

        Assert.Equal(want, toggle.Patterns.Toggle.Pattern.ToggleState.Value);
    }

    /// <summary>
    /// The verified sequence from <c>ClearAlertsObservationScenarios</c>, deliberately not re-derived: a
    /// re-invented version read the log as 0 lines in GUI-C-139.
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
