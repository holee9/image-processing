// GUI-C-234e (Codex #183 finding 2): the "preprocessing is off because of an earlier version's setting" notice, through the real window and UI Automation patterns only.
using System;
using System.IO;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// SelfCheck 19 drives the view model's command directly, so it never sees the XAML: the banner's visibility binding, its text and the OK button's command binding. This case does: a settings file
/// written by an earlier build (no schema version, <c>preprocessInChain=false</c>) is put in front of a real launch, and the notice has to be on screen, survive Clear Alerts, go away when OK is
/// invoked (and be stored as acknowledged), and not come back at the next launch - while preprocessing stays off. The control is a file of the current version with the same OFF: no notice.
///
/// <para>Input is UI Automation patterns only (Invoke, Toggle, ExpandCollapse): no global keyboard or mouse input.</para>
/// </summary>
public sealed class PreprocessOffNoticeScenarios(ITestOutputHelper output)
{
    private const string NoticeText = "PreprocessOffNoticeText";
    private const string NoticeOk = "PreprocessOffNoticeOkButton";

    private const string EarlierBuildFile = """
    { "voiWindowCenter": 1234, "preprocessInChain": false }
    """;

    private const string CurrentVersionFile = """
    { "settingsSchemaVersion": 1, "voiWindowCenter": 1234, "preprocessInChain": false }
    """;

    [SkippableFact]
    public void AnEarlierBuildsPreprocessOff_IsOnScreenUntilOkIsInvoked_AndDoesNotComeBack()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c234e-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        var path = Path.Combine(directory, "appsettings.json");
        File.WriteAllText(path, EarlierBuildFile);

        try
        {
            // ---- first launch: the notice is shown ----
            using (var app = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
                var window = app.MainWindow!;

                var notice = WaitForNotice(window, present: true);
                Assert.True(notice is not null, "The notice is not in the window for a settings file written by an earlier build.");
                Assert.False(notice!.IsOffscreen, "The notice is in the tree but off screen.");
                output.WriteLine($"1 notice text: {notice.Name}");
                Assert.Contains("earlier version", notice.Name ?? string.Empty, StringComparison.Ordinal);
                Assert.True(window.FindFirstDescendant(cf => cf.ByAutomationId(NoticeOk)) is { IsOffscreen: false }, "The OK button is not on screen with the notice.");
                Assert.False(PreprocessChecked(window), "Preprocessing is not off although the file says so.");

                // ---- Clear Alerts does not acknowledge it ----
                OpenLogs(window);
                Invoke(window, "ClearAlertsButton");
                Thread.Sleep(400);
                var afterClear = window.FindFirstDescendant(cf => cf.ByAutomationId(NoticeText));
                output.WriteLine($"2 after Clear Alerts: notice present = {afterClear is not null}");
                Assert.True(afterClear is { IsOffscreen: false }, "Clear Alerts took the notice away.");

                // ---- OK (Invoke pattern): the notice goes, the acknowledgement is stored ----
                Invoke(window, NoticeOk);
                var gone = WaitForNotice(window, present: false);
                output.WriteLine($"3 after OK: notice present = {gone is not null}");
                Assert.True(gone is null || gone.IsOffscreen, "The notice stayed after OK was invoked.");

                var stored = File.ReadAllText(path);
                output.WriteLine($"3 stored file: {stored.Replace(Environment.NewLine, " ")}");
                Assert.Contains("\"preprocessOffNoticeUnacknowledged\": false", stored, StringComparison.Ordinal);
                Assert.Contains("\"preprocessInChain\": false", stored, StringComparison.Ordinal);
                Assert.False(PreprocessChecked(window), "Acknowledging the notice switched preprocessing on.");
            }

            // ---- restart with the same settings: no notice, preprocessing still off ----
            using (var again = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!again.IsAvailable, again.SkipReason ?? "The application is not available.");
                var window = again.MainWindow!;
                Assert.False(PreprocessChecked(window), "After the restart preprocessing is on.");   // also gives the window time to finish startup
                var notice = WaitForNotice(window, present: true, seconds: 3);
                output.WriteLine($"4 after restart: notice present = {notice is not null}");
                Assert.True(notice is null || notice.IsOffscreen, "The notice came back after it was acknowledged.");
            }

            // ---- control: the same OFF in a file of the current version is the operator's choice - no notice ----
            File.WriteAllText(path, CurrentVersionFile);
            using (var control = new SettingsFileApplicationFixture(path))
            {
                Skip.If(!control.IsAvailable, control.SkipReason ?? "The application is not available.");
                var window = control.MainWindow!;
                Assert.False(PreprocessChecked(window), "Control: the current-version file's OFF was not applied, so the control proves nothing.");
                var notice = WaitForNotice(window, present: true, seconds: 3);
                output.WriteLine($"5 control (current version, OFF): notice present = {notice is not null}");
                Assert.True(notice is null || notice.IsOffscreen, "A current-version OFF was announced as an earlier version's.");
            }
        }
        finally
        {
            try
            {
                Directory.Delete(directory, recursive: true);
            }
            catch
            {
                // a leftover temp directory is not worth failing a measurement over
            }
        }
    }

    /// <summary>The notice element once it is present (or, with <paramref name="present"/> false, until it is gone), or null.</summary>
    private static AutomationElement? WaitForNotice(Window window, bool present, int seconds = 15)
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(seconds);
        AutomationElement? found = null;
        while (DateTime.UtcNow < deadline)
        {
            found = window.FindFirstDescendant(cf => cf.ByAutomationId(NoticeText));
            if (present ? found is not null : found is null || found.IsOffscreen)
            {
                return found;
            }

            Thread.Sleep(200);
        }

        return found;
    }

    private static bool PreprocessChecked(Window window)
    {
        OpenParameters(window);
        var box = window.FindFirstDescendant(cf => cf.ByAutomationId("PreprocessInChainCheckBox"));
        Assert.True(box is not null, "PreprocessInChainCheckBox is not in the Parameters tab.");
        return box!.AsCheckBox().IsChecked == true;
    }

    private static void Invoke(Window window, string automationId)
    {
        var button = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
        Assert.True(button is not null, $"{automationId} is not in the tree.");
        button!.AsButton().Invoke();
        Thread.Sleep(300);
    }

    /// <summary>View ▸ Logs then the Log tab (the Clear Alerts button lives there) - by Toggle/ExpandCollapse/Invoke patterns only.</summary>
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
        Thread.Sleep(500);
    }
}
