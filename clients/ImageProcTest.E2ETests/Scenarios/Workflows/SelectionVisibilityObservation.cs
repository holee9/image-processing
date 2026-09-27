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
        var control = Visibility(listRect, ordinary, "§A CONTROL (unfiltered, user just clicked it)");
        Assert.True(control.Visible,
            $"CONTROL FAILED: the row the user just selected is judged invisible ({control.Why}). The test "
          + "for visibility is wrong, so the filtered case below cannot be read.");

        SetFilter(window, true);

        var moved = SelectedItem(window);
        Assert.True(moved is not null, "After the filter, nothing reports itself as selected.");
        output.WriteLine($"§A selection moved to '{Trim(moved!.Name)}'");
        var movedVisibility = Visibility(listRect, moved, "§A moved row");
        output.WriteLine($"§A VERDICT: the moved row is " +
                         $"{(movedVisibility.Visible ? "WHERE THE USER CAN SEE IT" : "NOT IN THE VISIBLE AREA")}" +
                         $" ({movedVisibility.Why})");

        SetFilter(window, false);
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
    private Visibility_ Visibility(Rectangle listRect, AutomationElement item, string label)
    {
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
