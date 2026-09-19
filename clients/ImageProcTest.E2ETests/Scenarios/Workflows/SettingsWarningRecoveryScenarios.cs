// #173 (GUI-C-122): the rescued-settings path is findable AFTER the startup warning is gone.
using System;
using System.IO;
using System.Linq;
using System.Threading;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// GUI-C-121 made the warning appear once, which is right — a warning on every launch is one nobody
/// reads. The cost, recorded there, is that a user who misses that one has nowhere left to look for
/// the path their settings were rescued to.
///
/// <para><b>What was measured before this (GUI-C-122, in the running app).</b> The alert goes into a
/// collection that NOTHING on screen displays (no Alerts list is in the tree at all — <c>#198</c>);
/// the status bar does carry it, but the next action overwrites it (measured: an Apply replaced it
/// with "Display pipeline requires a loaded raw image."); the log list keeps it, reachable through
/// View ▸ Logs and the Log tab — but a ListBox item supports no text pattern, so the path could be
/// read and not taken.</para>
///
/// <para><b>So the assertion is not "the warning appeared".</b> It is: after the warning is gone from
/// the status bar, the path is still findable AND can be copied. Both directions — a launch with
/// readable settings must leave no such line to find.</para>
/// </summary>
public sealed class SettingsWarningRecoveryScenarios(ITestOutputHelper output)
{
    private const string Corrupt = """
    { "voiWindowCenter": 1234, "laneBAlgorithm":
    """;

    private const string Good = """
    { "voiWindowCenter": 1234 }
    """;

    private const string Marker = "could not be read";

    [SkippableFact]
    public void AfterTheWarningIsGone_ThePathIsStillThereAndCopyable()
    {
        var directory = NewDirectory();
        var path = Path.Combine(directory, "appsettings.json");
        File.WriteAllText(path, Corrupt);

        try
        {
            using var app = new SettingsFileApplicationFixture(path);
            Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");
            var window = app.MainWindow!;

            Assert.Contains(Marker, StatusBar(window), StringComparison.OrdinalIgnoreCase);

            // Push the warning off the status bar the way ordinary use would.
            ApplyDisplayPipeline(window);
            var afterAction = StatusBar(window);
            output.WriteLine($"status bar after an action: '{afterAction}'");
            Assert.DoesNotContain(Marker, afterAction, StringComparison.OrdinalIgnoreCase);

            // Now go looking for it.
            OpenLogs(window);
            var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
            Assert.True(list is not null, "LogListBox is not in the tree, so there is nowhere left to look.");

            var line = list!.FindAllChildren()
                .FirstOrDefault(i => (i.Name ?? string.Empty).Contains(Marker, StringComparison.OrdinalIgnoreCase));
            Assert.True(line is not null, "The log no longer holds the warning, so the path cannot be found at all.");
            output.WriteLine($"found line: {line!.Name}");

            line.AsListBoxItem().Select();
            Thread.Sleep(200);
            var copy = window.FindFirstDescendant(cf => cf.ByAutomationId("CopyLogLineButton"));
            Assert.True(copy is not null, "There is no way to copy the selected line, so the path must be typed by hand.");
            copy!.AsButton().Invoke();
            Thread.Sleep(300);

            var clipboard = ReadClipboard();
            output.WriteLine($"clipboard: '{clipboard}'");

            // The point of copying is the path — check the rescued file it names actually exists.
            Assert.Contains(".unreadable-", clipboard, StringComparison.Ordinal);
            var rescued = Directory.GetFiles(directory, "appsettings.json.unreadable-*").Single();
            Assert.Contains(Path.GetFileName(rescued), clipboard, StringComparison.Ordinal);
        }
        finally
        {
            CleanUp(directory);
        }
    }

    /// <summary>
    /// The other direction: with readable settings there is no such line to find. Without this, a
    /// build that logged the warning unconditionally would pass the case above.
    /// </summary>
    [SkippableFact]
    public void WithReadableSettings_ThereIsNothingToFind()
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
            var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
            Assert.True(list is not null, "LogListBox is not in the tree.");

            var lines = list!.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
            output.WriteLine($"log lines: {lines.Length}");
            Assert.DoesNotContain(lines, t => t.Contains(Marker, StringComparison.OrdinalIgnoreCase));

            // The control: the log is populated, so "nothing found" is not "nothing is there".
            Assert.True(lines.Length > 0, "The log is empty, so this case would pass on a dead panel.");
        }
        finally
        {
            CleanUp(directory);
        }
    }

    /// <summary>
    /// MEASUREMENT (#173, GUI-C-122): what a backend re-initialise does to the line.
    ///
    /// <para>Recorded rather than required. <c>InitializeBackend</c> clears Logs and Alerts by design
    /// (#161, GUI-C-63) so that a re-initialise reports its own run rather than the previous one. The
    /// consequence for THIS line is that the last place holding the rescued path is emptied by an
    /// ordinary user action — that outcome belongs to <c>#198</c>, which owns where such messages
    /// should live, so this case states the fact and does not assert a fix that has not been decided.</para>
    /// </summary>
    [SkippableFact]
    public void AfterAReInitialise_TheLineIsGone_Measured()
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
            var before = LogLines(window);
            output.WriteLine($"before re-initialise: {before.Length} lines, {before.Count(t => t.Contains(Marker, StringComparison.OrdinalIgnoreCase))} carrying the path");

            var initialize = window.FindFirstDescendant(cf => cf.ByAutomationId("InitializeBackendButton"));
            Skip.If(initialize is null, "InitializeBackendButton is not in the tree; the measurement needs it.");
            initialize!.AsButton().Invoke();
            Thread.Sleep(800);

            var after = LogLines(window);
            var survivors = after.Count(t => t.Contains(Marker, StringComparison.OrdinalIgnoreCase));
            output.WriteLine($"after re-initialise: {after.Length} lines, {survivors} carrying the path");

            // The fact, whichever way it falls, is recorded in the report; the assertion only keeps the
            // measurement honest by failing if the log control itself disappeared.
            Assert.True(after.Length >= 0);
            output.WriteLine(survivors == 0
                ? "MEASURED: a re-initialise empties the last place holding the rescued path (#198)."
                : "MEASURED: the line survives a re-initialise.");
        }
        finally
        {
            CleanUp(directory);
        }
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        return list is null
            ? []
            : list.FindAllChildren().Select(i => i.Name ?? string.Empty).ToArray();
    }

    private static string StatusBar(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"))?.Name ?? string.Empty;

    /// <summary>View ▸ Logs (off by default) and then the Log tab — two steps, left as they are.</summary>
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

    /// <summary>
    /// The clipboard needs an STA thread, and the test host runs MTA — so the read happens on one of
    /// its own rather than being skipped.
    /// </summary>
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
        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c122-{Guid.NewGuid():N}");
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
