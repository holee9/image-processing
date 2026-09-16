// #166 (GUI-C-72): the detached viewer's bindings, measured as values moving rather than as
// bindings being accepted.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

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
/// cases need the loaded-image fixture, a second window, a window move and a wheel gesture. §4.1's
/// cases press one thing and read one result; these drive a sequence across two windows.</para>
///
/// <para><b>What is NOT asserted here.</b> Whether the detached <i>viewport</i> redraws — the image
/// path, which is §4.3's own subject. It has no observation point in this harness: the control is a
/// bare <c>FrameworkElement</c> with no automation peer (the detached window exposes seven UIA
/// descendants, six of them title-bar chrome and one the status line), and pixel capture is blind
/// here — GUI-C-72 measured the MAIN window capturing blank white too, so a blank detached capture
/// says nothing about the detached window.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class DetachedViewerSyncScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private const string Title = "ImageProcTest Comparison Viewer";

    /// <summary>
    /// W-15: a wheel gesture inside the detached viewer moves the MAIN window's zoom readout.
    ///
    /// <para>This is what the six <c>TwoWay</c> bindings are for. Before it, the control: the same
    /// screen point is scrolled with no detached window open and the readout must NOT move.
    /// Without that control the case measures nothing — GUI-C-72 first ran it over the main
    /// window's own viewport, where the identical gesture zooms for the ordinary reason, and the
    /// result could not say which window had received the wheel.</para>
    /// </summary>
    [SkippableFact]
    public void W15_ScrollingTheDetachedViewer_MovesTheMainWindowsZoom()
    {
        Measure("W15", window =>
        {
            ResetTheView(window);
            CloseDetached(window);

            var main = window.BoundingRectangle;
            var point = new System.Drawing.Point(main.Right + 200, main.Top + 400);

            // Control: nothing of ours is under that point yet.
            var before = Zoom(window);
            Scroll(point);
            var afterControl = Zoom(window);
            output.WriteLine($"W15 control {before} -> {afterControl} at {point}");
            Assert.True(
                afterControl == before,
                $"The zoom readout moved from '{before}' to '{afterControl}' with no detached viewer " +
                "open, so a later move cannot be attributed to one.");

            var detached = OpenDetached(window);

            // Move it clear of the main window: inside the overlap the gesture is ambiguous.
            detached.AsWindow().Move(main.Right + 20, main.Top);
            Thread.Sleep(500);
            var r = detached.BoundingRectangle;
            Assert.True(
                !main.Contains(point.X, point.Y) && r.Contains(point.X, point.Y),
                $"The gesture point {point} is not inside the detached window {r} and outside the " +
                $"main window {main}; this run cannot attribute a change to the detached viewer.");

            detached.AsWindow().SetForeground();
            Thread.Sleep(300);
            var beforeScroll = Zoom(window);
            Scroll(point);
            var afterScroll = Zoom(window);
            output.WriteLine($"W15 detached {beforeScroll} -> {afterScroll}");

            CloseDetached(window);
            ResetTheView(window);

            Assert.True(
                afterScroll != beforeScroll,
                $"The main window's zoom stayed at '{afterScroll}' after the detached viewer was " +
                "scrolled. The viewport's write-back never reached the settings, so the two windows " +
                "are not one synchronised state (MENU-001 §4.3, #166).");
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

    private static AutomationElement OpenDetached(Window window)
    {
        OpenViewMenu(window);
        window.FindFirstDescendant(cf => cf.ByAutomationId("DetachComparisonViewerMenuItem"))!.AsMenuItem().Invoke();

        for (var i = 0; i < 20; i++)
        {
            Thread.Sleep(250);
            if (window.FindFirstDescendant(cf => cf.ByName(Title)) is { } found) return found;
        }

        throw new InvalidOperationException($"No '{Title}' window appeared within 5 s (#166).");
    }

    private static void CloseDetached(Window window)
    {
        if (window.FindFirstDescendant(cf => cf.ByName(Title)) is not { } detached) return;

        try { detached.AsWindow().Close(); }
        catch (Exception) { /* the next assertion reports a window that will not close */ }

        Thread.Sleep(400);
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
