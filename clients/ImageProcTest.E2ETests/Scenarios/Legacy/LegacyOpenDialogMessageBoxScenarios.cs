// GUI-C-246 (Codex #197 finding 5): the legacy "Load Raw..." dialog when its [Open] raises a message box of its own. 411c7c30 posts the [Open] click instead of sending it, because a sent BM_CLICK returns
// only when the button's handler has - and a handler that shows a modal message box would then make the test thread wait for a box that only that thread answers (the W9 self-deadlock, GUI-C-244).
// This is the input that actually raises such a box: a file name that does not exist.
using Xunit;
using Xunit.Abstractions;
using ImageProcTest.E2ETests.Scenarios.Operator;

namespace ImageProcTest.E2ETests.Scenarios.Legacy;

[Trait("Category", "E2E")]
public sealed class LegacyOpenDialogMessageBoxScenarios(ITestOutputHelper output)
{
    [SkippableFact]
    public void OpeningAFileThatDoesNotExist_RaisesTheDialogsMessageBox_AndEverythingCloses_WithoutADeadlock()
    {
        using var app = LegacyPreprocessReadinessScenarios.LegacyApp.LaunchOrSkip(breakTemp: false);
        // a test that may wait on a message box is bounded from outside: if a click were to block, the app is killed after 120 s and the call returns
        using var watchdog = new AppWatchdog(TimeSpan.FromSeconds(120), app.Kill, output.WriteLine);
        var missing = Path.Combine(Path.GetTempPath(), "xpe_c246_no_such_file_" + Guid.NewGuid().ToString("N") + ".raw");
        Assert.False(File.Exists(missing));
        var windowsBefore = app.TopLevelWindowCount();

        var outcome = app.TryOpenAMissingFile(missing, TimeSpan.FromSeconds(30));
        foreach (var line in outcome.Trace) output.WriteLine("   native " + line);
        output.WriteLine($"message box: {(outcome.BoxTitle is null ? "NONE" : $"'{outcome.BoxTitle}'")}, answered by {outcome.AnsweredBy}; box gone: {outcome.BoxGone}; file dialog stayed open after it: {outcome.DialogStayedOpen}; file dialog open at the end: {outcome.DialogStillOpenAtTheEnd}");

        Assert.False(watchdog.Fired, "the watchdog had to kill the app: a click blocked");
        Assert.True(outcome.BoxTitle is not null, "the dialog raised no message box for a file that does not exist: the input did not exercise the path this test is about");
        Assert.True(outcome.BoxGone, "the message box was not closed");
        Assert.False(outcome.DialogStillOpenAtTheEnd, "the file dialog was not closed (Cancel)");

        // the app is alive and answers UI Automation afterwards, with the windows it had before
        Assert.Equal(windowsBefore, app.TopLevelWindowCount());
    }
}
