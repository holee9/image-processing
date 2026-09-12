// XPE-GUI-E2E-001 §4.2 (GUI-C-43): the menu-driven rows of the workflow table.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// W-03, W-09 and W-10 of the plan's §4.2 table, each narrowed to what the app actually does.
///
/// The plan was written ahead of the app and three of its rows describe menus that are not there
/// (see the GUI-C-43 report §1 for the measured comparison). The lane does not change the app to
/// match a document — it measures, implements what exists, and reports the difference.
///
/// Every scenario here runs in BOTH backend modes. Where a value legitimately differs by backend it
/// is branched on <see cref="ApplicationFixture.BackendMode"/> rather than skipped: a skip counts as
/// a pass in the totals while measuring nothing, which is how GUI-C-37 nearly reported an
/// unexercised preprocess path as working.
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class WorkflowMenuScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// W-03 (narrowed): the backend the run was launched with is the one the menu reports, and the
    /// Native switch is inert.
    ///
    /// The plan says "Backend→Backend Mode→Mock↔Real" and expects a runtime toggle. Measured:
    /// <c>NativeBackendModeMenuItem</c> carries <c>IsEnabled="False"</c> in MainWindow.xaml, so there
    /// is no toggle to drive — the mode is chosen at launch (<c>--automation-backend</c>, GUI-C-26)
    /// and cannot be changed from the menu.
    ///
    /// That inertness is the thing worth guarding. If the item is ever enabled without the
    /// underlying switch working, a user would select Native and get Mock silently — the same class
    /// of defect GUI-C-31 measured on the command line.
    /// </summary>
    [SkippableFact]
    public void W03_BackendMenu_ReportsTheLaunchedModeAndOffersNoLiveSwitch()
    {
        Measure("W-03", window =>
        {
            var backendMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("BackendMenu"));
            Assert.True(backendMenu is not null, "BackendMenu was not found.");

            backendMenu!.AsMenuItem().Expand();
            try
            {
                var modeItem = WaitFor(() =>
                    backendMenu.FindFirstDescendant(cf => cf.ByAutomationId("BackendModeMenuItem"))
                    ?? window.FindFirstDescendant(cf => cf.ByAutomationId("BackendModeMenuItem")));
                Assert.True(modeItem is not null, "BackendModeMenuItem was not found.");

                modeItem!.AsMenuItem().Expand();

                var mock = WaitFor(() =>
                    modeItem.FindFirstDescendant(cf => cf.ByAutomationId("MockBackendModeMenuItem"))
                    ?? window.FindFirstDescendant(cf => cf.ByAutomationId("MockBackendModeMenuItem")));
                var native = WaitFor(() =>
                    modeItem.FindFirstDescendant(cf => cf.ByAutomationId("NativeBackendModeMenuItem"))
                    ?? window.FindFirstDescendant(cf => cf.ByAutomationId("NativeBackendModeMenuItem")));

                Assert.True(mock is not null, "MockBackendModeMenuItem was not found.");
                Assert.True(native is not null, "NativeBackendModeMenuItem was not found.");

                // The measured contract: no live switch exists. When this starts failing, the switch
                // became live and W-03 must be rewritten to actually drive it — not relaxed.
                Assert.False(
                    native!.IsEnabled,
                    "NativeBackendModeMenuItem is enabled. The plan's runtime Mock<->Real toggle was " +
                    "never implemented; if it now is, this scenario must drive it and verify the " +
                    "runtime panel follows, rather than asserting the item is inert.");
            }
            finally
            {
                try { backendMenu.AsMenuItem().Collapse(); } catch (Exception) { /* already closed */ }
            }

            // The launched mode is what the status bar reports — asserted in BOTH modes rather than
            // skipping one, so a Mock run and a Native run each prove they exercised what they claim.
            var runtime = window.FindFirstDescendant(cf => cf.ByAutomationId("RuntimeCommonVersionText"));
            Assert.True(runtime is not null, "RuntimeCommonVersionText was not found.");
            Assert.Contains(
                $"mode={app.BackendMode}", runtime!.Name ?? string.Empty, StringComparison.OrdinalIgnoreCase);
        });
    }

    /// <summary>
    /// W-09: exporting the automation report writes a file, and the app names it.
    ///
    /// The plan puts this under Tools and expects a TRX. Measured: the item lives under File
    /// (<c>ExportAutomationReportMenuItem</c>) and <c>ExportAutomationReport</c> writes
    /// <c>menu-command-report.json</c> next to the executable — JSON, not TRX. The scenario verifies
    /// what the app does; the plan row is reported as wrong rather than worked around.
    /// </summary>
    [SkippableFact]
    public void W09_ExportAutomationReport_WritesAReportFileAndNamesIt()
    {
        Measure("W-09", window =>
        {
            var expected = Path.Combine(
                Path.GetDirectoryName(app.ExecutablePath!)!, "menu-command-report.json");

            // Remove any file a previous run left, so "it exists" means THIS click produced it —
            // a stale artifact would satisfy the assertion without the command running at all.
            if (File.Exists(expected)) File.Delete(expected);

            InvokeMenuItem(window, "FileMenu", "ExportAutomationReportMenuItem");

            var written = WaitFor(() => File.Exists(expected) ? expected : null);
            Assert.True(
                written is not null,
                $"Export Automation Report did not produce {expected}. The command writes JSON beside " +
                "the executable (the plan's 'TRX' is wrong — see the GUI-C-43 report §1).");

            Assert.True(new FileInfo(expected).Length > 0, $"{expected} is empty.");

            var status = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
            if (status is not null)
            {
                output.WriteLine($"W-09 status: {status.Name}");
            }
        });
    }

    /// <summary>
    /// W-10: the fixture manager reports whether the fixture pack is present.
    ///
    /// The plan expects a SHA-256 in the UI. Measured: <c>ShowFixtureManager</c> sets the status text
    /// to "Fixture pack available: &lt;path&gt;" or "Fixture pack missing: &lt;path&gt;" and logs the
    /// same — no hash is computed anywhere in that path. The scenario asserts the reported state
    /// matches the directory that is actually on disk, which is the claim the app makes.
    /// </summary>
    [SkippableFact]
    public void W10_FixtureManager_ReportsWhetherTheFixturePackIsPresent()
    {
        Measure("W-10", window =>
        {
            var fixtureRoot = Path.Combine(
                Path.GetDirectoryName(app.ExecutablePath!)!, "fixtures", "gui-s0");
            var present = Directory.Exists(fixtureRoot);

            InvokeMenuItem(window, "ToolsMenu", "FixtureManagerMenuItem");

            var status = WaitFor(() =>
            {
                var element = window.FindFirstDescendant(cf => cf.ByAutomationId("StatusBarText"));
                return element?.Name?.Contains("Fixture pack", StringComparison.Ordinal) == true
                    ? element
                    : null;
            });

            Assert.True(status is not null, "The status bar never reported a fixture-pack state.");
            var text = status!.Name ?? string.Empty;
            output.WriteLine($"W-10 status: {text}");

            // Verified against the filesystem rather than accepting whichever line appeared: an app
            // that always said "missing" would otherwise satisfy a substring check.
            Assert.Contains(present ? "available" : "missing", text, StringComparison.Ordinal);
            Assert.DoesNotContain(present ? "missing" : "available", text, StringComparison.Ordinal);
        });
    }

    /// <summary>Expands a top-level menu, invokes one item, and closes the menu again.</summary>
    private static void InvokeMenuItem(Window window, string menuId, string itemId)
    {
        var menu = window.FindFirstDescendant(cf => cf.ByAutomationId(menuId));
        Assert.True(menu is not null, $"{menuId} was not found.");

        menu!.AsMenuItem().Expand();
        try
        {
            // Searched inside the menu first: WPF draws submenu children in their own popup window,
            // so they are not reliably descendants of the main window (GUI-C-36 measured this
            // difference between the local machine and the CI runner).
            var item = WaitFor(() =>
                menu.FindFirstDescendant(cf => cf.ByAutomationId(itemId))
                ?? window.FindFirstDescendant(cf => cf.ByAutomationId(itemId)));

            Assert.True(item is not null, $"{itemId} was not found.");
            Assert.True(item!.IsEnabled, $"{itemId} is disabled.");
            item.AsMenuItem().Invoke();
        }
        finally
        {
            try { menu.AsMenuItem().Collapse(); } catch (Exception) { /* closed by Invoke */ }
        }
    }

    /// <summary>Polls briefly for something the UI produces lazily. Null when it never appears.</summary>
    private static T? WaitFor<T>(Func<T?> find) where T : class
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(10);
        while (DateTime.UtcNow < deadline)
        {
            var found = find();
            if (found is not null) return found;
            Thread.Sleep(150);
        }

        return null;
    }

    /// <summary>Runs one scenario, recording its elapsed time (the §4.2 gate is per suite).</summary>
    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

                // GUI-C-51: a re-acquired window is reported, not silently swallowed. The fixture replaces
        // an element that cannot answer WPF properties (GUI-C-50); before this the replacement left
        // no trace in the run's record, so a run that hit the defect looked exactly like one that
        // did not. Written before the body so it survives a scenario that throws.
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
            stopwatch.Stop();
            output.WriteLine($"{scenario} elapsed {stopwatch.ElapsedMilliseconds} ms ({app.BackendMode})");
        }
    }
}
