// #225 (GUI-C-204): File > Exit, asserted as what it does — the process ends, with exit code 0.
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// What the handler does (<c>MainWindow.xaml.cs</c> <c>ExitMenuItem_OnClick</c> = <c>Close()</c>, then <c>OnClosing</c>): the FIRST close request does not close. It cancels the close, starts the
/// backend shutdown in the background and keeps the window open and responsive ("Backend shutting down..."); the window closes itself when the shutdown is done. Nothing asks about unsaved
/// settings — there is no prompt and no save on exit (the settings file is written only by Save Settings). So the observable result is: the process ends on its own, and the exit code is 0.
///
/// <para>This scenario makes a change that is NOT saved before it exits (Zoom In), so "exit with an unsaved change" is the case measured: the exit must neither stop at a prompt nor fail.</para>
///
/// <para>UIA only: the item is invoked through its automation peer. No key, no mouse — those go to whatever window is in front (the leader's terminal).</para>
/// </summary>
[Collection(ExitApplicationCollection.Name)]
public sealed class MenuExitScenarios(ExitApplicationFixture app, ITestOutputHelper output)
{
    [SkippableFact]
    public void X01_FileExit_EndsTheProcessWithExitCodeZero_EvenWithAnUnsavedChange()
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
        var window = app.MainWindow!;
        output.WriteLine($"X-01 backend={app.BackendMode}");

        app.WatchForExit();

        // An unsaved change: Zoom In moves the comparison zoom from Fit to 125% and nothing saves it.
        Invoke(window, "ViewMenu", "ZoomInMenuItem");

        Invoke(window, "FileMenu", "ExitMenuItem");

        var exitCode = app.WaitForExitCode(TimeSpan.FromSeconds(60));
        output.WriteLine($"X-01 exit code: {(exitCode is null ? "still running after 60 s" : exitCode.ToString())}");
        Assert.True(exitCode is not null,
            "The app was still running 60 s after File > Exit: the close did not complete (a prompt, or a shutdown that never finished).");
        Assert.Equal(0, exitCode);
    }

    private static void Invoke(Window window, string menuId, string itemId)
    {
        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId(menuId));
        Assert.True(menu is not null, $"{menuId} was not found.");
        menu!.AsMenuItem().Expand();

        AutomationElement? item = null;
        for (var attempt = 0; attempt < 20 && item is null; attempt++)
        {
            item = window.FindFirstDescendant(cf => cf.ByAutomationId(itemId));
            if (item is null) Thread.Sleep(100);
        }

        Assert.True(item is not null, $"{itemId} did not appear under {menuId}.");
        item!.AsMenuItem().Invoke();
        try { menu.AsMenuItem().Collapse(); } catch (Exception) { /* the window may already be closing */ }
        Thread.Sleep(300);
    }
}
