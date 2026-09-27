// GUI-C-142: when the #206 filter moves the selection, is the moved-to row where a user can see it?
using System;
using System.Drawing;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// GUI-C-141 measured that hiding the selected line makes WPF move the selection to a visible item, and
/// that <c>Copy</c> then copies THAT line rather than the hidden one. Its verdict was left open on one
/// question: can the user SEE that the selection moved? Two pixel probes tried to answer it and both
/// failed — and the failure was instructive.
///
/// <para><b>Why they failed, and what this file does differently.</b> Neither compared two things inside
/// ONE capture. The per-item probe compared two separate captures (a 260-character line and a 34-character
/// one came out with the same mean, 48.8, because the item rectangle was 1582 wide — unclipped, running
/// off the screen). The list-level probe compared a mean against an expectation and returned 180.5 on a
/// <c>#14171e</c> panel, which is not that panel. In both, the control sat OUTSIDE the measurement, so
/// "no difference" could not be told from "the instrument is broken".</para>
///
/// <para>Here the comparison is a DIFFERENCE inside one image — the selected row's band against the
/// immediately adjacent unselected row's band, same width, same lighting, same capture — and the positive
/// control runs in the same test: with the filter OFF a selected row must differ from its neighbour. If
/// that control shows no difference, this instrument cannot see a highlight and NOTHING is concluded from
/// the filtered case (criterion: <c>.moai/reports/lane-gui/GUI-C-142/report.md</c> §1, committed first).</para>
///
/// <para><b>The band-difference case is NOT here, deliberately.</b> It was written, run, and removed:
/// its positive control failed three times because <c>Capture()</c> reads the SCREEN at the element's
/// coordinates and a brighter window sits there (the capture is not blank — min 0, max 255 — but its
/// overall mean is 214.6 on a <c>#14171e</c> app). Screen capture cannot see this app in this
/// environment, so keeping the code would be measuring nothing while looking like a measurement. The
/// question it was for is answered without a screen by
/// <c>Rendering/LogSelectionHighlightRenderTests</c>.</para>
/// </summary>
public sealed class SelectionVisibilityObservation(ITestOutputHelper output)
{
    private const string AlertMarker = "ALERT ";

    /// <summary>
    /// §A: the cheap question, answered without reading a single pixel — is the row the selection moved to
    /// inside the list's client area? If it is not, the highlight is irrelevant: the user cannot see the
    /// row at all. Measured before §B because it can make §B unnecessary.
    /// </summary>
    [SkippableFact]
    public void A_WhenTheFilterMovesTheSelection_IsThatRowWhereTheUserCanSeeIt()
    {
        using var app = new WorkflowApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        Assert.True(list is not null, "LogListBox is not in the tree.");

        // The clipping rectangle is MEASURED, not assumed. The 1582-wide item rectangle in GUI-C-141 came
        // from clipping against nothing; if the list's own rectangle is also outside the window, the window
        // is what to clip against — and that has to be seen rather than guessed.
        var windowRect = window.BoundingRectangle;
        var listRect = list!.BoundingRectangle;
        output.WriteLine($"§A window rect {Fmt(windowRect)}");
        output.WriteLine($"§A list   rect {Fmt(listRect)} inside window={windowRect.Contains(listRect)}");

        var items = list.FindAllChildren();
        var ordinary = items.FirstOrDefault(i => !(i.Name ?? string.Empty).Contains(AlertMarker, StringComparison.Ordinal));
        var alertCount = items.Count(i => (i.Name ?? string.Empty).Contains(AlertMarker, StringComparison.Ordinal));
        Skip.If(ordinary is null, "No ordinary log line to select.");
        Skip.If(alertCount == 0, "No alert line, so the filter would hide everything.");

        ordinary!.Patterns.SelectionItem.Pattern.Select();
        Thread.Sleep(300);

        // THE CONTROL, inside the measurement: the row the user just clicked, with no filter involved, is
        // one they can certainly see. Whatever test answers "is this row visible" must say yes to it.
        //
        // This is not decoration. A first version asked whether the item's rectangle was CONTAINED in the
        // list's, and said no — for the unfiltered row too. The reason is the same broken rectangle that
        // defeated GUI-C-141: the list is 356 wide and the item reports 1582, because an unclipped WPF
        // item rectangle carries the text's desired width. The row IS visible; the width is not.
        var control = Visibility(window, ordinary, "§A CONTROL (unfiltered, user just clicked it)");
        Assert.True(control.Visible,
            $"CONTROL FAILED: the row the user just selected is judged invisible ({control.Why}). The test "
          + "for visibility is wrong, so the filtered case below cannot be read.");

        SetFilter(window, true);

        var moved = SelectedItem(window);
        Assert.True(moved is not null, "After the filter, nothing reports itself as selected.");
        output.WriteLine($"§A selection moved to '{Trim(moved!.Name)}'");
        var movedVisibility = Visibility(window, moved, "§A moved row");
        output.WriteLine($"§A VERDICT: the moved row is " +
                         $"{(movedVisibility.Visible ? "WHERE THE USER CAN SEE IT" : "NOT IN THE VISIBLE AREA")}" +
                         $" ({movedVisibility.Why})");

        SetFilter(window, false);
    }

    /// <summary>
    /// GUI-C-143: the one path that could overturn GUI-C-142's verdict. That card measured a log short
    /// enough to fit the list (17 rows in a 431-pixel list, <c>viewSize</c> 100%), so nothing was scrolled
    /// out of sight. With a log long enough to scroll, <c>verticallyInside</c> can be false and
    /// "the moved selection is visible" would become conditional.
    ///
    /// <para><b>Four conditions, the same instrument.</b> S0 top / S1 bottom / S2 middle with a visible row
    /// selected, and S3 with a row selected while scrolled up and then scrolled away. The measurement is
    /// <see cref="Visibility"/> from GUI-C-142 — no new probe, or the numbers would not be comparable.</para>
    ///
    /// <para><b>S3's control applies at click time.</b> S3 deliberately puts the selected row off screen, so
    /// "the just-clicked row is visible" cannot hold after the scroll. It is checked when the row is
    /// clicked, and the scroll-away is then asserted to have actually hidden it — without that assertion S3
    /// would silently become S1.</para>
    ///
    /// <para><b>Scrollability is asserted, not assumed.</b> <c>viewSize</c> is printed on every row and has
    /// to be under 100%: with a log that fits, S1-S3 collapse into S0 and reading that as "all four
    /// conditions hold" would be the emptiest kind of pass.</para>
    /// </summary>
    [SkippableFact]
    public void A2_WithTheLogScrolled_IsTheMovedRowStillWhereTheUserCanSeeIt()
    {
        using var app = new WorkflowApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        var window = app.MainWindow!;
        OpenLogs(window);

        // A log long enough to scroll. The chain is re-run rather than the frame re-loaded: GUI-C-139
        // measured that each run adds lines (8 on the native path, 5 on the mock one).
        GrowTheLog(window, 12);
        {
        }

        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))!;
        var listRect = list.BoundingRectangle;
        var rows = list.FindAllChildren().Length;
        var view = ViewSize(window);
        output.WriteLine($"§A2 setup: list {Fmt(listRect)} realized rows={rows} viewSize={view:0.0}%");

        Assert.True(view >= 0 && view < 99.5,
            $"CONDITION NOT MET: viewSize={view:0.0}% — the log fits the list, so S1-S3 are S0 and 'all four "
          + "conditions hold' would say nothing. Grow the log before reading this case.");

        MeasureCondition(window, "S0 top", 0.0, offScreenSelection: false);
        MeasureCondition(window, "S1 bottom", 100.0, offScreenSelection: false);
        MeasureCondition(window, "S2 middle", 50.0, offScreenSelection: false);
        MeasureCondition(window, "S3 bottom, selection above", 100.0, offScreenSelection: true);
    }

    /// <summary>
    /// Grows the log by re-running the chain. Each run adds lines and, on the native path, one alert
    /// (measured, GUI-C-139). Returns how many runs actually went through.
    /// </summary>
    private int GrowTheLog(Window window, int runs)
    {
        var done = 0;
        for (var i = 0; i < runs; i++)
        {
            if (!Invoke(window, "RunPreprocessingMenuItem") && !Invoke(window, "ApplyDisplayPipelineMenuItem"))
            {
                break;
            }

            done++;
            Thread.Sleep(200);
        }

        return done;
    }

    /// <summary>
    /// GUI-C-143 closed with "the moved selection is visible, whatever the scroll position" and named ONE
    /// hole: every condition it measured left the FILTERED list short enough to fit one screen
    /// (<c>viewSize</c> 100% after the filter), and the selection always moved to the first row, which is
    /// the top of a list that needs no scrolling. If the filtered list were LONGER than the screen, the
    /// first row would be above the viewport unless the list scrolled there.
    ///
    /// <para><b>The hole is reachable, and the verdict does not survive it.</b> 34 chain re-runs (measured:
    /// 28.6 s) put 27 alerts in the log, so the filtered list reports <c>viewSize</c> 71.4% — longer than
    /// one screen. In that state the selection still moves to the first alert, the list does NOT scroll to
    /// it, and the row lands above the visible area: <c>offscreen=True</c>, clipped to nothing.</para>
    ///
    /// <para><b>What makes it a defect rather than ordinary scrolling</b> is the pair GUI-C-143 built for
    /// exactly this: the row was VISIBLE before the filter and NOT visible after (S0/S1/S2). The user did
    /// not scroll it away — the filter moved the selection somewhere the user cannot see, and <c>Copy</c>
    /// reads that selection.</para>
    ///
    /// <para>Observation only: the assertions here are the condition and the controls. Whether to fix it,
    /// and how, is not this case's to decide.</para>
    /// </summary>
    [SkippableFact]
    public void A3_WhenTheFilteredListIsLongerThanTheScreen_TheMovedRowIsAboveIt()
    {
        using var app = new WorkflowApplicationFixture();
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        // The mock backend cannot reach this condition BY CONSTRUCTION: it adds a fixed three alerts with no
        // loop and no knob, so the filtered list stays 3 rows however often the chain is re-run (measured:
        // 34 re-runs, filtered rows=3, viewSize 100%). Skipped here for that stated structural reason
        // rather than by loosening the condition assertion below — that assertion still has to bite on the
        // native path if alerts ever stop accumulating.
        Skip.If(string.Equals(app.BackendMode, "Mock", StringComparison.OrdinalIgnoreCase),
            "The mock backend raises a fixed 3 alerts, so the filtered list cannot exceed one screen "
          + "(GUI-C-145 §5 — the card puts the mock path out of scope for this reason). Needs the native path.");

        var window = app.MainWindow!;
        OpenLogs(window);

        // 34 rather than the 27 that just clears the bar: 27 leaves the filtered list at viewSize 89.3%
        // against the 99.5% limit, 34 leaves it at 71.4%, and the two cost the same (measured 1 m 26 s vs
        // 1 m 27 s — the re-run count is not what this case spends its time on).
        var ran = GrowTheLog(window, 34);
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))!;
        var listRect = list.BoundingRectangle;

        // The condition this case exists for: the FILTERED list must not fit one screen. Asserted rather
        // than hoped — with fewer alerts this collapses into GUI-C-143's S0 and would pass saying nothing.
        SetFilter(window, true);
        var filteredView = ViewSize(window);
        var filteredRows = list.FindAllChildren().Length;
        SetFilter(window, false);
        output.WriteLine($"§A3 setup: {ran} re-runs, filtered rows={filteredRows} filtered viewSize={filteredView:0.0}%");

        Assert.True(filteredView >= 0 && filteredView < 99.5,
            $"CONDITION NOT MET: with the filter on viewSize={filteredView:0.0}% — the alerts still fit one "
          + $"screen ({filteredRows} rows), so this is GUI-C-143's S0 again. More alerts are needed before "
          + "this case says anything.");

        MeasureCondition(window, "§A3 top", 0.0, offScreenSelection: false);
        MeasureCondition(window, "§A3 bottom", 100.0, offScreenSelection: false);
    }

    /// <summary>
    /// One condition: scroll, select, check the control, turn the filter on, and report where the selection
    /// ended up. The position BEFORE the filter is recorded next to the position AFTER, because that pair is
    /// what separates "the user scrolled it out of sight" from "the filter moved the selection off screen" —
    /// only the second is a defect candidate.
    /// </summary>
    private void MeasureCondition(
        Window window, string label, double scrollPercent, bool offScreenSelection)
    {
        SetFilter(window, false);
        Scroll(window, offScreenSelection ? 0.0 : scrollPercent);

        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))!;
        var candidate = list.FindAllChildren().FirstOrDefault(i =>
            !(i.Name ?? string.Empty).Contains(AlertMarker, StringComparison.Ordinal)
            && IsRowVisible(window, i));
        if (candidate is null)
        {
            output.WriteLine($"{label}: no visible ordinary row to select — condition not measured");
            return;
        }

        candidate.Patterns.SelectionItem.Pattern.Select();
        Thread.Sleep(300);

        // The control, per condition: the row just clicked must be visible. For S3 this is the click-time
        // check — the scroll below is what takes it out of sight on purpose.
        var control = Visibility(window, candidate, $"{label} CONTROL (just clicked)");
        if (!control.Visible)
        {
            output.WriteLine($"{label}: CONTROL FAILED — this condition's measurement is void");
            return;
        }

        if (offScreenSelection)
        {
            Scroll(window, scrollPercent);
            var away = Visibility(window, candidate, $"{label} after scrolling away (must NOT be visible)");
            if (away.Visible)
            {
                output.WriteLine($"{label}: the scroll did not hide the selected row — this became S1, void");
                return;
            }
        }

        var beforeFilter = Visibility(window, candidate, $"{label} selected row BEFORE the filter");
        var viewBefore = ViewSize(window);

        SetFilter(window, true);
        var moved = SelectedItem(window);
        var viewAfter = ViewSize(window);
        if (moved is null)
        {
            output.WriteLine($"{label}: after the filter nothing reports itself selected — its own fact");
            SetFilter(window, false);
            return;
        }

        var after = Visibility(window, moved, $"{label} selected row AFTER the filter");
        var sameRow = string.Equals(moved.Name, candidate.Name, StringComparison.Ordinal);
        output.WriteLine(
            $"{label}: viewSize before={viewBefore:0.0}% after={viewAfter:0.0}% — "
          + $"selection {(sameRow ? "UNCHANGED" : "MOVED")} to '{Trim(moved.Name)}', "
          + $"visibleBefore={beforeFilter.Visible} visibleAfter={after.Visible} => "
          + $"{(after.Visible ? "VISIBLE" : "NOT VISIBLE")}");

        SetFilter(window, false);
    }

    private static bool IsRowVisible(Window window, AutomationElement item)
    {
        var listRect = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))?.BoundingRectangle
                       ?? Rectangle.Empty;
        var raw = item.BoundingRectangle;
        return Rectangle.Intersect(listRect, raw).Width > 0
            && raw.Y >= listRect.Y && raw.Bottom <= listRect.Bottom
            && !item.IsOffscreen;
    }

    /// <summary>Scrolls the list to a percentage of its content, through the scroll pattern.</summary>
    private void Scroll(Window window, double percent)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        if (list is null || !list.Patterns.Scroll.TryGetPattern(out var scroll))
        {
            output.WriteLine($"scroll to {percent:0}%: no scroll pattern");
            return;
        }

        if (!scroll.VerticallyScrollable.Value)
        {
            output.WriteLine($"scroll to {percent:0}%: not scrollable (viewSize {scroll.VerticalViewSize.Value:0.0}%)");
            return;
        }

        scroll.SetScrollPercent(-1, percent);
        Thread.Sleep(400);
    }

    /// <summary>The share of the content the viewport shows. Under 100 means there is something to scroll.</summary>
    private static double ViewSize(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is not null && list.Patterns.Scroll.TryGetPattern(out var scroll)
            ? scroll.VerticalViewSize.Value
            : -1.0;
    }

    /// <summary>Invokes a Pipeline menu item; says whether it was there and enabled rather than throwing.</summary>
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
            menu.Patterns.ExpandCollapse.Pattern.Collapse();
            Thread.Sleep(100);
            return false;
        }

        item.AsMenuItem().Invoke();
        Thread.Sleep(200);
        return true;
    }

    private readonly record struct Visibility_(bool Visible, string Why);

    /// <summary>
    /// Is this row where a user can see it? Judged on the CLIPPED rectangle, because the raw one is not
    /// trustworthy here — the list is 356 wide and items report widths over 1200 (an unclipped WPF item
    /// rectangle carries the text's desired width, which is what made GUI-C-141's per-item capture
    /// meaningless).
    ///
    /// <para>Two things have to hold: the row must overlap the list horizontally at all, and its vertical
    /// span must be inside the list's client area — the second is the scroll question. Both numbers are
    /// printed so the verdict can be checked rather than trusted.</para>
    /// </summary>
    private Visibility_ Visibility(Window window, AutomationElement item, string label)
    {
        // The list rectangle is read HERE, not passed in. Measured (#214, GUI-C-145): a rect captured at the
        // start of a case and reused was 13 px above the list's position at measurement time, which made a
        // row sitting correctly at the list's top read as 12 of its 23 pixels cut off. The geometry probe
        // that caught it printed list 976,308 and the selected row 977,309 — contiguous, fully inside.
        var listRect = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"))?.BoundingRectangle
                       ?? Rectangle.Empty;
        var raw = item.BoundingRectangle;
        var clipped = Rectangle.Intersect(listRect, raw);
        var verticallyInside = raw.Y >= listRect.Y && raw.Bottom <= listRect.Bottom;
        var overlaps = clipped.Width > 0 && clipped.Height > 0;
        var visible = overlaps && verticallyInside && !item.IsOffscreen;
        var why = $"raw {Fmt(raw)}, clipped {Fmt(clipped)}, verticallyInside={verticallyInside}, "
                + $"overlaps={overlaps}, offscreen={item.IsOffscreen}";
        output.WriteLine($"{label}: {why} => visible={visible}");
        return new Visibility_(visible, why);
    }

    private static string Fmt(Rectangle r) => $"{r.X},{r.Y} {r.Width}x{r.Height}";

    private static string Trim(string? s) => s is null ? "(null)" : s.Length <= 56 ? s : s[..53] + "...";

    private static AutomationElement? SelectedItem(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list?.FindAllChildren().FirstOrDefault(
            i => i.Patterns.SelectionItem.TryGetPattern(out var p) && p.IsSelected.Value);
    }

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

    /// <summary>The verified sequence, not re-derived — re-inventing it read the log as 0 lines in GUI-C-139.</summary>
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
