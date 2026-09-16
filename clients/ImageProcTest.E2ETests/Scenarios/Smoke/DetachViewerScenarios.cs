// #166 (GUI-C-71): the detached comparison viewer, after the binding modes were corrected.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// S11: pressing <c>Detach Comparison Viewer</c> opens a window.
///
/// <para>GUI-C-70 measured why it did not: <c>BindDetachedViewport</c> gave all eight bindings one
/// <c>TwoWay</c> mode, and the first two targets — <c>SourceImage</c> and <c>ProcessedImage</c>,
/// both with private setters — made <c>SetBinding</c> throw before the window was constructed. The
/// assertion here is <b>the window is there</b>, not "no exception was raised": the old build
/// raised one and reported nothing, so an exception-shaped assertion would have measured the wrong
/// thing.</para>
///
/// <para><b>The window is found under the main window, not on the desktop.</b> It is opened with
/// <c>Owner</c> set, and UIA nests an owned window beneath its owner — measured in GUI-C-71:
/// <c>desktopChild=False underMain=True</c> on every poll for ten seconds. GUI-C-46 looked for it
/// among the desktop's children and found "the taskbar, the main window and Program Manager"; that
/// search could not have found this window even on a build where it opens.</para>
///
/// <para>No <c>Category</c> trait: GUI-C-69 measured that the trait does not gate anything
/// (no tool reads it, and <c>Category="Workflow"</c> does not exist at all), and the leader's
/// judgement on that card is that the folder is the record. Attaching a trait here would write down
/// a classification the repository does not actually use.</para>
/// </summary>
[Collection(ApplicationCollection.Name)]
public sealed class DetachViewerScenarios(ApplicationFixture app, ITestOutputHelper output)
{
    private const string DetachedWindowTitle = "ImageProcTest Comparison Viewer";

    [SkippableFact]
    public void S11_DetachComparisonViewer_OpensTheWindow()
    {
        Measure("S11", window =>
        {
            // Establish the start state rather than assume it: these cases share one app, and a
            // previous run of this scenario could have left the window up (GUI-C-67 lost a card to
            // exactly that assumption in S08).
            CloseDetachedWindow(window);
            Assert.True(
                Find(window) is null,
                "A detached viewer was already open, so this run cannot say the command opened one.");

            OpenViewMenu(window);
            var item = window.FindFirstDescendant(cf => cf.ByAutomationId("DetachComparisonViewerMenuItem"));
            Assert.True(item is not null, "DetachComparisonViewerMenuItem is not in the View menu (MENU-001 §4.3).");
            item!.AsMenuItem().Invoke();

            var detached = WaitFor(window);
            output.WriteLine($"S11 detached={(detached is not null)}");

            Assert.True(
                detached is not null,
                $"No window titled '{DetachedWindowTitle}' appeared under the main window within " +
                "5 s. The command reports success on a path it never reaches when a binding throws " +
                "(#166).");

            CloseDetachedWindow(window);
        });
    }

    private static AutomationElement? Find(Window window) =>
        window.FindFirstDescendant(cf => cf.ByName(DetachedWindowTitle));

    private static AutomationElement? WaitFor(Window window)
    {
        // 250 ms steps rather than 500: the window was found on the first poll every time it was
        // measured, so the step size is the scenario's cost, not its patience. The ceiling stays 5 s.
        for (var i = 0; i < 20; i++)
        {
            Thread.Sleep(250);
            if (Find(window) is { } found) return found;
        }

        return null;
    }

    /// <summary>Closes the detached window if one is up, so the shared app goes back as it was.</summary>
    private static void CloseDetachedWindow(Window window)
    {
        if (Find(window) is not { } detached) return;

        try { detached.AsWindow().Close(); }
        catch (Exception) { /* a window that cannot be closed is reported by the next assertion */ }

        Thread.Sleep(400);
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
