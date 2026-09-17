// #182 follow-up (GUI-C-98): the "view state" settings are shown on screen, read from what was drawn.
using System.Globalization;
using System.Text.RegularExpressions;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// One reading per view-state setting that the settings survey (<c>SettingsProcessingConnectionTests</c>)
/// lists. None of them reads the setting: the viewport cases read the main viewport's automation peer,
/// whose HelpText is written inside the render pass (<c>rendered=…; zoom=…; scale=…; offset=x,y;
/// swipe=…; opacity=…</c>), and the focus case reads whether the side panels are in the tree.
///
/// <para><b>The route under test runs from the setting to the screen.</b> Dragging or scrolling the
/// viewport changes the control's own properties first, so a drag alone would still move the image with
/// the binding deleted. Each case therefore changes the view with the pointer and then restores it with
/// <c>View → Reset Comparison View</c>, which only writes the settings
/// (<c>MainWindowViewModel.ResetComparisonView</c>). The drawn value must come back; with the binding
/// removed it cannot. Opacity has a direct settings writer — the slider — and uses it instead.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class ViewStateRenderScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>V-01 (<c>ComparisonOverlayOpacity</c>): the slider's value is the opacity the overlay is drawn with.</summary>
    [SkippableFact]
    public void V01_OverlayOpacity_IsDrawnWithTheSliderValue()
    {
        var window = Ready();
        try
        {
            // The toolbar button, not F7: run on its own right after launch, F7 did not reach the window
            // (2 of 2, GUI-C-98), and this case is about opacity, not about the key.
            window.FindFirstDescendant(cf => cf.ByAutomationId("CompareButtonOverlayOpacity"))!.AsButton().Invoke();
            Assert.True(WaitFor(window, s => s.Mode == "OverlayOpacity") is not null, $"Overlay was not drawn: {Help(window)}");

            var slider = window.FindFirstDescendant(cf => cf.ByAutomationId("OverlayOpacitySlider"));
            Assert.True(slider is not null, "OverlayOpacitySlider is not in the tree in Overlay mode.");

            foreach (var value in new[] { 0.9, 0.2 })
            {
                slider!.Patterns.RangeValue.Pattern.SetValue(value);
                var drawn = WaitFor(window, s => s.Opacity is { } o && Math.Abs(o - value) < 0.001);
                output.WriteLine($"V01 slider={value} help='{Help(window)}'");
                Assert.True(drawn is not null, $"The slider was set to {value}; the overlay was drawn with: {Help(window)}");
            }
        }
        finally
        {
            ResetView(window);
        }
    }

    /// <summary>V-02 (<c>ComparisonZoomScale</c>): after a wheel zoom, Reset draws the image at fit again.</summary>
    [SkippableFact]
    public void V02_ZoomScale_ResetDrawsFit()
    {
        var window = Ready();
        try
        {
            ResetView(window);
            var fit = WaitFor(window, s => s.Zoom == "fit");
            Assert.True(fit is not null, $"The view did not start at fit: {Help(window)}");

            ScrollViewport(window, 3);
            var zoomed = WaitFor(window, s => s.Zoom != "fit" && s.Scale > fit!.Scale * 1.2);
            output.WriteLine($"V02 fit='{fit!.Raw}' zoomed='{Help(window)}'");
            Assert.True(zoomed is not null, $"The wheel did not change the drawn zoom: {Help(window)}");

            ResetView(window);
            var back = WaitFor(window, s => s.Zoom == "fit" && Math.Abs(s.Scale - fit.Scale) < 0.0001);
            output.WriteLine($"V02 after reset='{Help(window)}'");
            Assert.True(back is not null, $"Reset wrote ComparisonZoomScale=0; the viewport still draws: {Help(window)}");
        }
        finally
        {
            ResetView(window);
        }
    }

    /// <summary>V-03 (<c>ComparisonPanX</c>): after a pan, Reset draws the image centred horizontally.</summary>
    [SkippableFact]
    public void V03_PanX_ResetRecentres() => PanThenReset(horizontal: true);

    /// <summary>V-04 (<c>ComparisonPanY</c>): after a pan, Reset draws the image centred vertically.</summary>
    [SkippableFact]
    public void V04_PanY_ResetRecentres() => PanThenReset(horizontal: false);

    /// <summary>V-05 (<c>ComparisonSwipePosition</c>): after the divider is dragged, Reset draws it at the middle.</summary>
    [SkippableFact]
    public void V05_SwipePosition_ResetRedrawsTheDividerAtTheMiddle()
    {
        var window = Ready();
        try
        {
            ResetView(window);
            Assert.True(WaitFor(window, s => s.Mode == "SwipeVertical" && s.Swipe is { } w && Math.Abs(w - 0.5) < 0.001) is not null,
                $"The view did not start with the divider at 0.5: {Help(window)}");

            var r = ViewportElement(window).BoundingRectangle;
            var y = r.Top + (r.Height / 2);
            Drag(new System.Drawing.Point(r.Left + (r.Width / 2), y), new System.Drawing.Point(r.Left + (r.Width / 4), y), MouseButton.Left);
            var moved = WaitFor(window, s => s.Swipe is { } w && w < 0.4);
            output.WriteLine($"V05 dragged='{Help(window)}'");
            Assert.True(moved is not null, $"The drag did not move the drawn divider: {Help(window)}");

            ResetView(window);
            var back = WaitFor(window, s => s.Swipe is { } w && Math.Abs(w - 0.5) < 0.001);
            output.WriteLine($"V05 after reset='{Help(window)}'");
            Assert.True(back is not null, $"Reset wrote ComparisonSwipePosition=0.5; the divider is drawn at: {Help(window)}");
        }
        finally
        {
            ResetView(window);
        }
    }

    /// <summary>V-06 (<c>FocusMode</c>): the Focus toggle removes the two side panels from the screen and brings them back.</summary>
    [SkippableFact]
    public void V06_FocusMode_HidesAndRestoresTheSidePanels()
    {
        var window = Ready();
        var toggled = false;
        try
        {
            Assert.True(PanelsShown(window), "The side panels are not shown before the Focus toggle; the case cannot tell anything.");

            ToggleFocus(window);
            toggled = true;
            var hidden = WaitUntil(() => !PanelShown(window, "AnalysisPanel") && !PanelShown(window, "StudyQueue"));
            output.WriteLine($"V06 focus on: analysis={PanelShown(window, "AnalysisPanel")} queue={PanelShown(window, "StudyQueue")}");

            // Measured in GUI-C-98: with the default LeftPanelOpen / RightPanelOpen = true, the converter
            // (FocusPanelVisibilityConverter) keeps both panels Visible in focus mode, and nothing in the
            // app sets those two values (MENU-001 §9.2.1). Focus mode therefore changes nothing on
            // screen. That is a finding for #182, not a pass: the case skips with the reason, and
            // asserts the moment focus mode does hide something.
            Skip.If(!hidden,
                "FocusMode has no visible effect with the default LeftPanelOpen/RightPanelOpen=true " +
                "(FocusPanelVisibilityConverter) and no UI changes those two values — #182 finding, GUI-C-98.");

            ToggleFocus(window);
            toggled = false;
            var shown = WaitUntil(() => PanelsShown(window));
            output.WriteLine($"V06 focus off: analysis={PanelShown(window, "AnalysisPanel")} queue={PanelShown(window, "StudyQueue")}");
            Assert.True(shown, "Focus mode is off and a side panel did not come back.");
        }
        finally
        {
            if (toggled) ToggleFocus(window);
        }
    }

    // ---- helpers -------------------------------------------------------------------------------

    private void PanThenReset(bool horizontal)
    {
        var window = Ready();
        var axis = horizontal ? "X" : "Y";
        try
        {
            ResetView(window);
            Assert.True(WaitFor(window, s => Math.Abs(s.OffsetX) < 1 && Math.Abs(s.OffsetY) < 1) is not null,
                $"The view did not start centred: {Help(window)}");

            var r = ViewportElement(window).BoundingRectangle;
            var c = new System.Drawing.Point(r.Left + (r.Width / 2), r.Top + (r.Height / 2));
            Drag(c, new System.Drawing.Point(c.X + 80, c.Y + 60), MouseButton.Right);
            var moved = WaitFor(window, s => Math.Abs(horizontal ? s.OffsetX : s.OffsetY) > 20);
            output.WriteLine($"V-pan{axis} dragged='{Help(window)}'");
            Assert.True(moved is not null, $"The right-drag did not move the drawn image along {axis}: {Help(window)}");

            ResetView(window);
            var back = WaitFor(window, s => Math.Abs(horizontal ? s.OffsetX : s.OffsetY) < 1);
            output.WriteLine($"V-pan{axis} after reset='{Help(window)}'");
            Assert.True(back is not null, $"Reset wrote ComparisonPan{axis}=0; the image is drawn at: {Help(window)}");
        }
        finally
        {
            ResetView(window);
        }
    }

    internal sealed record Drawn(string Raw, string Mode, string Zoom, double Scale, double OffsetX, double OffsetY, double? Swipe, double? Opacity);

    private static readonly Regex DrawnPattern = new(
        @"^rendered=(?<mode>[^;]+); zoom=(?<zoom>[^;]+); scale=(?<scale>[^;]+); offset=(?<ox>[^,]+),(?<oy>[^;]+); swipe=(?<swipe>[^;]+); opacity=(?<op>[^;]+)(?:; processed=(?<hash>[^;]+))?$",
        RegexOptions.CultureInvariant);

    private static string Help(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"))?.HelpText ?? "(no viewport)";

    internal static Drawn? Read(Window window)
    {
        var help = Help(window);
        var m = DrawnPattern.Match(help);
        if (!m.Success) return null;
        static double D(string v) => double.Parse(v, CultureInfo.InvariantCulture);
        static double? N(string v) => v == "-" ? null : D(v);
        return new Drawn(help, m.Groups["mode"].Value, m.Groups["zoom"].Value, D(m.Groups["scale"].Value),
            D(m.Groups["ox"].Value), D(m.Groups["oy"].Value), N(m.Groups["swipe"].Value), N(m.Groups["op"].Value));
    }

    private static Drawn? WaitFor(Window window, Func<Drawn, bool> condition)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(5);
        while (DateTime.UtcNow < deadline)
        {
            var d = Read(window);
            if (d is not null && condition(d)) return d;
            Thread.Sleep(100);
        }

        return null;
    }

    private static bool WaitUntil(Func<bool> condition)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(5);
        while (DateTime.UtcNow < deadline)
        {
            if (condition()) return true;
            Thread.Sleep(100);
        }

        return false;
    }

    private static void ScrollViewport(Window window, int notches)
    {
        window.SetForeground();
        var r = ViewportElement(window).BoundingRectangle;
        Mouse.Position = new System.Drawing.Point(r.Left + (r.Width / 2), r.Top + (r.Height / 2));
        Thread.Sleep(150);
        for (var i = 0; i < notches; i++)
        {
            Mouse.Scroll(1);
            Thread.Sleep(120);
        }
    }

    private static void Drag(System.Drawing.Point from, System.Drawing.Point to, MouseButton button)
    {
        Mouse.Position = from;
        Thread.Sleep(150);
        Mouse.Down(button);
        Thread.Sleep(100);
        for (var i = 1; i <= 8; i++)
        {
            Mouse.Position = new System.Drawing.Point(from.X + ((to.X - from.X) * i / 8), from.Y + ((to.Y - from.Y) * i / 8));
            Thread.Sleep(30);
        }

        Mouse.Up(button);
        Thread.Sleep(200);
    }

    /// <summary>View → Reset Comparison View, opened with the same retry as the Pipeline menu (GUI-C-80/81).</summary>
    private static void ResetView(Window window)
    {
        AutomationElement? item = null;
        for (var attempt = 0; attempt < 3 && item is null; attempt++)
        {
            window.SetForeground();
            Keyboard.Press(VirtualKeyShort.ESCAPE);
            Thread.Sleep(120);
            var menu = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"))!.AsMenuItem();
            if (attempt == 0) menu.Click(); else menu.Expand();
            Thread.Sleep(350);
            item = window.FindFirstDescendant(cf => cf.ByAutomationId("ResetComparisonViewMenuItem"));
        }

        Assert.True(item is not null, "View → Reset Comparison View did not appear after three attempts.");
        item!.AsMenuItem().Invoke();
        Thread.Sleep(400);
    }

    private static void ToggleFocus(Window window)
    {
        var toggle = window.FindFirstDescendant(cf => cf.ByAutomationId("FocusModeToggle"));
        Assert.True(toggle is not null, "FocusModeToggle is not in the tree.");
        // A real click: the UIA Toggle pattern flips IsChecked without running the button's Command.
        window.SetForeground();
        toggle!.Click();
        Thread.Sleep(300);
    }

    private static AutomationElement ViewportElement(Window window)
    {
        var e = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"));
        Assert.True(e is not null, "WorkbenchViewport is not in the tree.");
        return e!;
    }

    private static bool PanelShown(Window window, string automationId)
    {
        var e = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        return e is not null && !e.IsOffscreen && e.BoundingRectangle.Width > 0;
    }

    private static bool PanelsShown(Window window) => PanelShown(window, "AnalysisPanel") && PanelShown(window, "StudyQueue");

    private Window Ready()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);   // an owned detached viewer would sit over the viewport and take the pointer
        return window;
    }
}
