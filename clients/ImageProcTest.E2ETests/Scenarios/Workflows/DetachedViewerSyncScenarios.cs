// #166 (GUI-C-72): the detached viewer's bindings, measured as values moving rather than as
// bindings being accepted.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Two directions across the detached comparison viewer, asserted separately.
///
/// <para>GUI-C-71 corrected the binding modes and measured that <c>SetBinding</c> no longer throws.
/// That is a weaker claim than the one <c>MENU-001</c> §4.3 makes — "must not create unsynchronised
/// source/processed image windows" is about values moving, and a binding being accepted is not a
/// value moving. These two cases close that gap from both ends.</para>
///
/// <para><b>Why the Workflow suite.</b> By character, not by headroom (GUI-C-69 judgement): both
/// cases need the loaded-image fixture, a second window, a hit-test probe and a wheel gesture.
/// §4.1's cases press one thing and read one result; these drive a sequence across two windows.</para>
///
/// <para><b>What is NOT asserted here.</b> Whether the detached <i>viewport</i> redraws — the image
/// path, which is §4.3's own subject. When these cases were written the control had no automation peer
/// (the detached window exposed seven UIA descendants, six of them title-bar chrome and one the status
/// line), and pixel capture is blind here — GUI-C-72 measured the MAIN window capturing blank white too.
/// Since #172 (GUI-C-79) the control does have a peer that reports which images it received
/// (<c>ViewportTruthScenarios</c>); that says what the viewport was handed, not that it redrew.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class DetachedViewerSyncScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private const string Title = DetachedTitle;

    /// <summary>
    /// W-15: a wheel gesture inside the detached viewer moves the MAIN window's zoom readout.
    ///
    /// <para>This is what the six <c>TwoWay</c> bindings are for.</para>
    ///
    /// <para><b>How the gesture is attributed, and why the first version was wrong.</b> The wheel
    /// goes to whichever window owns the pixel under the cursor, so the question "did the detached
    /// viewer receive it?" is a question about that pixel. The first version answered it with
    /// geometry — it moved the detached window beside the main one and required the point to be
    /// outside the main window's rectangle. That premise was the test's own invention: the command
    /// sets no position at all (no <c>WindowStartupLocation</c>, no <c>Left</c>/<c>Top</c> anywhere
    /// in the app), so the window lands wherever the OS puts it. On CI it opened at <c>{0,0}</c>
    /// overlapping the main window, the move off-screen did not take, and the case refused to
    /// attribute — correctly, but for a premise that was never going to hold (#166, GUI-C-73).</para>
    ///
    /// <para>It now asks the tree instead: <c>FromPoint</c> reports which top-level window owns that
    /// pixel, which is the same hit-test the wheel follows. The control is that the SAME point
    /// answers differently once the detached window is closed — a probe that always answered
    /// "detached" would attribute anything.</para>
    /// </summary>
    [SkippableFact]
    public void W15_ScrollingTheDetachedViewer_MovesTheMainWindowsZoom()
    {
        Measure("W15", window =>
        {
            ResetTheView(window);
            CloseDetached(window);

            var detached = OpenDetached(window);
            var r = detached.BoundingRectangle;
            var point = new System.Drawing.Point(r.Left + (r.Width / 2), r.Top + (r.Height / 2));

            output.WriteLine($"W15 detachedRect={r} point={point} mainRect={window.BoundingRectangle}");
            Assert.True(
                OwnsPixel(point, detached),
                $"The pixel at {point} is not owned by the detached viewer (FromPoint answered " +
                $"'{NameAt(point)}'), so a wheel gesture there cannot be attributed to it.");

            // The control: a pixel the detached viewer does NOT own must answer no. Without it the
            // probe could be answering yes to everything.
            var mainRect = window.BoundingRectangle;
            var elsewhere = APixelOutside(
                new System.Drawing.Rectangle(r.Left, r.Top, r.Width, r.Height),
                new System.Drawing.Rectangle(mainRect.Left, mainRect.Top, mainRect.Width, mainRect.Height));
            var elsewhereName = NameAt(elsewhere);
            output.WriteLine($"W15 control point={elsewhere} name='{elsewhereName}'");
            Assert.True(
                elsewhereName != "<none>",
                $"Nothing at all answers for {elsewhere}, so the control is vacuous — a probe that " +
                "says no to an empty pixel has not been shown to discriminate.");
            Assert.True(
                !OwnsPixel(elsewhere, detached),
                $"The probe says the detached viewer owns {elsewhere}, which is outside its " +
                $"rectangle {r}; it cannot tell the two windows apart.");

            detached.AsWindow().SetForeground();
            Thread.Sleep(300);
            var before = Zoom(window);
            Scroll(point);
            var after = Zoom(window);
            output.WriteLine($"W15 zoom {before} -> {after}");

            CloseDetached(window);
            ResetTheView(window);

            Assert.True(
                after != before,
                $"The main window's zoom stayed at '{after}' after the detached viewer was scrolled. " +
                "The viewport's write-back never reached the settings, so the two windows are not " +
                "one synchronised state (MENU-001 §4.3, COMPARE-001 GUI-CMP-FR-004, #166).");
        });
    }

    /// <summary>
    /// W-16: a mode change in the main window reaches the detached window.
    ///
    /// <para>Read from the detached window's own status line, which is the only element it exposes.
    /// That line is bound to the view model, so this asserts the detached window's content follows
    /// the shared state — not that the viewport's CompareMode property does; see the class note.</para>
    /// </summary>
    [SkippableFact]
    public void W16_ChangingTheModeInTheMainWindow_ReachesTheDetachedWindow()
    {
        Measure("W16", window =>
        {
            CloseDetached(window);
            var detached = OpenDetached(window);

            PressInMainWindow(window, VirtualKeyShort.F8);
            var difference = StatusLine(detached);

            PressInMainWindow(window, VirtualKeyShort.F5);
            var swipe = StatusLine(detached);

            output.WriteLine($"W16 afterF8='{difference}' afterF5='{swipe}'");
            CloseDetached(window);

            Assert.Contains("DifferenceHeatmap", difference);
            Assert.Contains("SwipeVertical", swipe);
            Assert.True(
                difference != swipe,
                "The detached window's status line read the same in both modes, so it is not " +
                "following the main window (#166).");
        });
    }

    /// <summary>
    /// Whether the detached window owns the pixel at <paramref name="point"/> — the same hit-test
    /// the wheel follows. The element at a point can be the window itself or something inside it,
    /// so the answer is "this element, or one of its ancestors, is that window".
    /// </summary>
    private bool OwnsPixel(System.Drawing.Point point, AutomationElement detached)
    {
        var target = detached.Properties.NativeWindowHandle.ValueOrDefault;
        var walker = app.Automation.TreeWalkerFactory.GetControlViewWalker();
        var current = app.Automation.FromPoint(point);

        for (var i = 0; i < 12 && current is not null; i++)
        {
            if (current.Properties.NativeWindowHandle.ValueOrDefault == target) return true;
            current = walker.GetParent(current);
        }

        return false;
    }

    private string NameAt(System.Drawing.Point point)
    {
        var e = app.Automation.FromPoint(point);
        return e is null ? "<none>" : SafeName(e);
    }

    /// <summary>
    /// A pixel the detached window does not cover, preferring one the MAIN window does — a pixel
    /// the two windows could plausibly both be claimed to own makes the sharpest control. Falls
    /// back to just outside the detached window's edge when the two rectangles leave no such pixel.
    /// </summary>
    private static System.Drawing.Point APixelOutside(
        System.Drawing.Rectangle detachedRect, System.Drawing.Rectangle mainRect)
    {
        System.Drawing.Point[] candidates =
        [
            new(mainRect.Right - 40, mainRect.Top + 40),
            new(mainRect.Left + 40, mainRect.Bottom - 40),
            new(mainRect.Right - 40, mainRect.Bottom - 40),
            new(mainRect.Left + 40, mainRect.Top + 40),
        ];

        foreach (var candidate in candidates)
        {
            if (candidate.X >= 0 && candidate.Y >= 0 && !detachedRect.Contains(candidate))
            {
                return candidate;
            }
        }

        return detachedRect.Left - 10 >= 0
            ? new System.Drawing.Point(detachedRect.Left - 10, detachedRect.Top + 40)
            : new System.Drawing.Point(detachedRect.Right + 10, detachedRect.Top + 40);
    }

    private static string SafeName(AutomationElement e)
    {
        try { return e.Name; } catch (Exception ex) { return $"<{ex.GetType().Name}>"; }
    }

    private static string StatusLine(AutomationElement detached)
    {
        var texts = detached.FindAllDescendants(cf => cf.ByControlType(FlaUI.Core.Definitions.ControlType.Text));
        return string.Join(" | ", texts.Select(t => { try { return t.Name; } catch { return "<unreadable>"; } }));
    }

    private static string Zoom(Window window)
    {
        var e = window.FindFirstDescendant(cf => cf.ByAutomationId("ZoomPercentText"));
        Assert.True(e is not null, "ZoomPercentText is not reachable, so nothing about the zoom can be said.");
        return e!.Name;
    }

    private static void Scroll(System.Drawing.Point point)
    {
        Mouse.MovePixelsPerMillisecond = 100;
        Mouse.Position = point;
        Thread.Sleep(250);
        Mouse.Scroll(3);
        Thread.Sleep(700);
    }

    private static void PressInMainWindow(Window window, VirtualKeyShort key)
    {
        window.SetForeground();
        Thread.Sleep(200);
        Keyboard.Press(key);
        Thread.Sleep(700);
    }

    /// <summary>Puts zoom, pan and mode back, so the next case starts where this one did.</summary>
    private static void ResetTheView(Window window)
    {
        OpenViewMenu(window);
        window.FindFirstDescendant(cf => cf.ByAutomationId("ResetLayoutMenuItem"))!.AsMenuItem().Invoke();
        Thread.Sleep(500);
    }

    private static void OpenViewMenu(Window window)
    {
        window.SetForeground();
        Keyboard.Press(VirtualKeyShort.ESCAPE);
        Thread.Sleep(120);
        window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"))!.AsMenuItem().Click();
        Thread.Sleep(300);
    }

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        if (!string.IsNullOrEmpty(app.ReacquiredNote))
        {
            output.WriteLine($"{scenario} fixture: {app.ReacquiredNote}");
        }

        var stopwatch = Stopwatch.StartNew();
        try
        {
            body(app.MainWindow!);
        }
        finally
        {
            output.WriteLine($"{scenario}: {stopwatch.ElapsedMilliseconds} ms");
        }
    }
}
