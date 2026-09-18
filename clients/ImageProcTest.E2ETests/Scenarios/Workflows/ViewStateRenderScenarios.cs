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
/// swipe=…; opacity=…</c>), and focus mode is not here — its toggle is disabled (#182, UnappliedSettingsScenarios U-04).
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

    /// <summary>
    /// V-06 (#173, GUI-C-112): the image is drawn at the size that was ASKED for.
    ///
    /// <para>Every other case here compares a key with ITSELF — V-02 checks that zooming raises the
    /// scale and that Reset brings it back to whatever it was, V-03/V-04 that the offset returns to
    /// where it started. A renderer that computed the scale wrongly but consistently passes all of
    /// them, and nothing on this suite can see the difference: there is no pixel capture (GUI-C-72,
    /// GUI-C-78), so "the image is drawn too small" has no observer.</para>
    ///
    /// <para>This one compares two keys that are produced by different code. <c>zoom</c> is the value
    /// that was requested (the dependency property). <c>scale</c> is measured from the rectangle the
    /// frame actually drew into, divided by the source width. The control's own rule is that an
    /// explicit zoom IS the effective scale (GetEffectiveScale returns ZoomScale unchanged when it is
    /// above zero), so the two must agree — and they disagree exactly when the frame drew at a size
    /// other than the one asked for.</para>
    ///
    /// <para>What this does NOT cover: an error inside GetEffectiveScale itself. Change that method and
    /// the requested value and the drawn rectangle move together, so this case stays green. The two keys
    /// are independent because of how the code is arranged today — the request is a dependency property
    /// and the measurement comes off the drawn rectangle — not because anything enforces it.</para>
    ///
    /// <para>Only the explicit-zoom case is checked. At fit, the expected scale depends on the
    /// control's layout size in device-independent units, which this harness cannot read: comparing
    /// against the element's screen rectangle would make the case a DPI measurement rather than a
    /// rendering one.</para>
    /// </summary>
    [SkippableFact]
    public void V06_AnExplicitZoom_IsTheScaleThatWasDrawn()
    {
        var window = Ready();
        try
        {
            ResetView(window);
            Assert.True(WaitFor(window, s => s.Zoom == "fit") is not null, $"The view did not start at fit: {Help(window)}");

            ScrollViewport(window, 3);
            var zoomed = WaitFor(window, s => s.Zoom != "fit");
            Assert.True(zoomed is not null, $"The wheel did not move the view off fit: {Help(window)}");

            var requested = double.Parse(zoomed!.Zoom, CultureInfo.InvariantCulture);
            output.WriteLine($"V06 requested zoom={requested:0.####}; drawn scale={zoomed.Scale:0.####}; raw='{zoomed.Raw}'");

            Assert.True(
                Math.Abs(zoomed.Scale - requested) < 0.001,
                $"The view was asked to draw at {requested:0.####} and the frame drew at {zoomed.Scale:0.####} " +
                $"— the image on screen is {(zoomed.Scale < requested ? "smaller" : "larger")} than requested " +
                $"by a factor of {zoomed.Scale / requested:0.###}. {Help(window)}");
        }
        finally
        {
            ResetView(window);
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

    /// <summary>
    /// The fields these cases read, in the order the peer writes them.
    ///
    /// <para>The tail is deliberately open (<c>(?:;.*)?$</c>) rather than an exhaustive list of the
    /// remaining fields. An exhaustive tail makes every case here fail the moment the peer gains a
    /// field it does not read: GUI-C-102 appended <c>processedMean</c> and <c>hud</c>, GUI-C-103 added
    /// <c>renderMs</c>, and this pattern stopped matching — <c>Read</c> returned null, so all five
    /// cases timed out with messages quoting a HelpText whose values were in fact correct.</para>
    /// </summary>
    private static readonly Regex DrawnPattern = new(
        @"^rendered=(?<mode>[^;]+); zoom=(?<zoom>[^;]+); scale=(?<scale>[^;]+); offset=(?<ox>[^,]+),(?<oy>[^;]+); swipe=(?<swipe>[^;]+); opacity=(?<op>[^;]+)(?:;.*)?$",
        RegexOptions.CultureInvariant | RegexOptions.Singleline);

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

    private static AutomationElement ViewportElement(Window window)
    {
        var e = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"));
        Assert.True(e is not null, "WorkbenchViewport is not in the tree.");
        return e!;
    }


    private Window Ready()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        CloseDetached(window);   // an owned detached viewer would sit over the viewport and take the pointer
        return window;
    }
}
