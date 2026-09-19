// #198 ① (GUI-C-125): an alert the app raises is findable on screen, and tells itself apart from a log line.
using System;
using System.IO;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Until this card the <c>Alerts</c> collection had nowhere on screen to appear: GUI-C-122 measured the
/// automation tree carrying <c>ClearAlertsButton</c> and no list for it to clear. So every alert the app
/// raised was written to a place only the code could read.
///
/// <para><b>Which alert is raised here, and why this one.</b> The alerts named in the issue (#194,
/// #196) come from the native modules, and the GUI never drains them — that is layer ② of #198 and a
/// separate card. This case uses an alert the GUI raises itself: the unreadable-settings warning
/// (<c>SETTINGS_UNREADABLE</c>), produced by putting a corrupt settings file in front of a real
/// launch. It exercises the same path — <c>RaiseAlert</c> — that the drain will use.</para>
///
/// <para><b>The assertion is "findable AND distinguishable".</b> Merged into the log without a marker,
/// an alert would be buried rather than shown, which is the same silence with more steps.</para>
/// </summary>
public sealed class AlertVisibilityScenarios(ITestOutputHelper output)
{
    private const string Corrupt = """
    { "voiWindowCenter": 1234, "laneBAlgorithm":
    """;

    private const string Good = """
    { "voiWindowCenter": 1234 }
    """;

    /// <summary>The marker the merged alert carries. A word, not a colour: the item template is
    /// unchanged (GUI-C-122), automation can read text, and a copy takes the text with it.</summary>
    private const string AlertMarker = "ALERT";

    private const string Code = "SETTINGS_UNREADABLE";

    [SkippableFact]
    public void AnAlert_IsOnScreenAndTellsItselfApartFromALogLine()
    {
        var directory = NewDirectory();
        var path = Path.Combine(directory, "appsettings.json");
        File.WriteAllText(path, Corrupt);

        try
        {
            using var app = new SettingsFileApplicationFixture(path);
            Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
            var window = app.MainWindow!;

            OpenLogs(window);
            var lines = LogLines(window);
            output.WriteLine($"log lines: {lines.Length}");
            foreach (var l in lines) output.WriteLine($"  {l}");

            // ① the alert is there at all
            var alertLines = lines.Where(t => t.Contains(AlertMarker, StringComparison.Ordinal)).ToArray();
            Assert.True(alertLines.Length > 0,
                "No alert is on screen; the alert went into a collection nothing displays (#198 ①).");
            var alert = alertLines.Single(t => t.Contains(Code, StringComparison.Ordinal));
            output.WriteLine($"alert line: {alert}");

            // ② it names its severity and code, so it is an alert rather than a sentence that happens
            //    to be logged.
            Assert.Contains("WARN", alert, StringComparison.Ordinal);
            Assert.Contains(Code, alert, StringComparison.Ordinal);

            // ③ ordinary log lines do NOT carry the marker — otherwise "distinguishable" is vacuous.
            var ordinary = lines.Where(t => !t.Contains(AlertMarker, StringComparison.Ordinal)).ToArray();
            output.WriteLine($"ordinary lines: {ordinary.Length}");
            Assert.True(ordinary.Length > 0,
                "Every line carries the alert marker, so the marker distinguishes nothing.");

            // ④ what GUI-C-122 and GUI-C-123 established still holds on this line.
            var copy = window.FindFirstDescendant(cf => cf.ByAutomationId("CopyLogLineButton"));
            Assert.True(copy is not null, "CopyLogLineButton is gone.");
            Assert.False(copy!.IsEnabled, "Copy is pressable with nothing selected (GUI-C-123).");

            var item = LogItems(window).First(i => (i.Name ?? string.Empty).Contains(Code, StringComparison.Ordinal));
            item.AsListBoxItem().Select();
            Thread.Sleep(250);
            Assert.True(copy.IsEnabled, "Copy stayed disabled with a line selected.");
            copy.AsButton().Invoke();
            Thread.Sleep(300);

            var clipboard = ReadClipboard();
            output.WriteLine($"clipboard: '{clipboard}'");
            Assert.Contains(AlertMarker, clipboard, StringComparison.Ordinal);
            Assert.Contains(Code, clipboard, StringComparison.Ordinal);
        }
        finally
        {
            CleanUp(directory);
        }
    }

    /// <summary>
    /// The control: with nothing to alert about, no alert line is there — and the log is not empty, so
    /// "no alert found" is not "nothing was looked at".
    /// </summary>
    [SkippableFact]
    public void WithNothingWrong_NoAlertLineIsThere()
    {
        var directory = NewDirectory();
        var path = Path.Combine(directory, "appsettings.json");
        File.WriteAllText(path, Good);

        try
        {
            using var app = new SettingsFileApplicationFixture(path);
            Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
            var window = app.MainWindow!;

            OpenLogs(window);
            var lines = LogLines(window);
            output.WriteLine($"log lines: {lines.Length}");

            Assert.True(lines.Length > 0, "The log is empty, so this case would pass on a dead panel.");
            Assert.DoesNotContain(lines, t => t.Contains(Code, StringComparison.Ordinal));
        }
        finally
        {
            CleanUp(directory);
        }
    }

    private static AutomationElement[] LogItems(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        Assert.True(list is not null, "LogListBox is not in the tree.");
        return list!.FindAllChildren();
    }

    private static string[] LogLines(Window window) =>
        LogItems(window).Select(i => i.Name ?? string.Empty).ToArray();

    /// <summary>View ▸ Logs (off by default) then the Log tab — unchanged by this card.</summary>
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

    private static string ReadClipboard()
    {
        var text = string.Empty;
        var thread = new Thread(() =>
        {
            try
            {
                text = System.Windows.Clipboard.GetText();
            }
            catch (Exception ex)
            {
                text = $"(clipboard read failed: {ex.Message})";
            }
        });

        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join(TimeSpan.FromSeconds(5));
        return text;
    }

    private static string NewDirectory()
    {
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c125-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);
        return directory;
    }

    private static void CleanUp(string directory)
    {
        try
        {
            Directory.Delete(directory, recursive: true);
        }
        catch
        {
            // A leftover temp directory is not worth failing a measurement over.
        }
    }
}
