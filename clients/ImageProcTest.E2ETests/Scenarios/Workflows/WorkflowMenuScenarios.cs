// XPE-GUI-E2E-001 §4.2 (GUI-C-43): the menu-driven rows of the workflow table.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

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
///
/// <para><b>One exception, added with its reason (#225, GUI-C-153).</b> W-03b skips under Mock. That is
/// not a value differing by backend — under Mock the native DLLs are absent, so "switch back to Native"
/// has nothing to switch to and the observation cannot be made at all. The alternative was to weaken the
/// assertion until both configurations passed, which would make a working switch and a missing DLL the
/// same green. W-03 itself still runs in both.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class WorkflowMenuScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>
    /// W-03 (rewritten, #225 GUI-C-153 row 4): the Backend &gt; Mode menu is a LIVE switch, and driving
    /// it changes what the app runs on.
    ///
    /// <para><b>Why this replaces the old assertion.</b> W-03 used to assert
    /// <c>NativeBackendModeMenuItem.IsEnabled == false</c> and said, in its own words, that when that
    /// started failing the scenario "must drive it and verify the runtime panel follows, rather than
    /// asserting the item is inert". The switch became live; this is that rewrite, not a relaxation.</para>
    ///
    /// <para><b>What is asserted is a RESULT, not a setting.</b> Reading back
    /// <c>AppSettings.BackendMode</c> would pass even if nothing were re-initialised. Instead this drives
    /// the menu to Mock and requires two independent surfaces to follow: the red MOCK BACKEND banner
    /// (bound to <c>IsMockBackend</c>, i.e. the backend OBJECT, not the request) and the status bar's
    /// <c>mode=</c> text. A request that fell back, or never re-initialised, moves neither.</para>
    ///
    /// <para><b>Runs in both backends.</b> Driving toward Mock is deterministic everywhere: the mock
    /// backend cannot fail to load. The reverse direction — Mock back to Native — can only be observed
    /// where the native DLLs exist, so it is a separate case (<see cref="W03b_SwitchingBackToNative_RestoresTheNativeBackend"/>)
    /// rather than an assertion weakened until both configurations pass.</para>
    /// </summary>
    [SkippableFact]
    public void W03_BackendMenu_DrivesTheBackendAndTheAppFollows()
    {
        Measure("W-03", window =>
        {
            var launched = app.BackendMode;
            output.WriteLine($"W-03 launched mode: {launched}");

            var native = FindModeItem(window, "NativeBackendModeMenuItem");
            Assert.True(
                native.IsEnabled,
                "NativeBackendModeMenuItem is disabled. Row 4 of #225 enabled it together with the " +
                "command behind it; if it is inert again the command was removed and this scenario " +
                "should fail rather than be rewritten back.");
            CloseBackendMenu(window);

            // Drive to Mock and require the app — not the setting — to follow.
            InvokeModeItem(window, "MockBackendModeMenuItem");
            var bannerAfterMock = WaitFor(() => MockBannerText(window));
            var runtimeAfterMock = RuntimeText(window);
            output.WriteLine($"W-03 after Mock: banner='{bannerAfterMock}' runtime='{runtimeAfterMock}'");

            Assert.True(
                bannerAfterMock is not null,
                "The MOCK BACKEND banner did not appear after selecting Mock. The banner is bound to " +
                "IsMockBackend — the backend object — so its absence means the backend was not replaced.");
            Assert.Contains("mode=Mock", runtimeAfterMock, StringComparison.OrdinalIgnoreCase);

            // Leave the fixture on the mode it was launched with; the collection is shared.
            InvokeModeItem(window, launched == "Native" ? "NativeBackendModeMenuItem" : "MockBackendModeMenuItem");
        });
    }

    /// <summary>
    /// W-03b: switching back to Native actually returns to the native backend.
    ///
    /// <para>Native only, and for a structural reason rather than convenience: a run launched with the
    /// mock backend has no native DLLs on the search path, so <c>XpeBackendFactory</c> falls back and
    /// the observation cannot exist. Loosening the assertion so it passed there would make "the switch
    /// works" and "the DLLs are missing" the same green.</para>
    /// </summary>
    [SkippableFact]
    public void W03b_SwitchingBackToNative_RestoresTheNativeBackend()
    {
        Skip.If(app.BackendMode != "Native",
            "The Mock backend has no native DLLs to switch back to; the round trip cannot be observed there.");

        Measure("W-03b", window =>
        {
            InvokeModeItem(window, "MockBackendModeMenuItem");
            Assert.True(WaitFor(() => MockBannerText(window)) is not null, "Mock did not take effect.");

            InvokeModeItem(window, "NativeBackendModeMenuItem");
            var runtime = WaitFor(() =>
            {
                var text = RuntimeText(window);
                return text.Contains("mode=Native", StringComparison.OrdinalIgnoreCase) ? text : null;
            });
            output.WriteLine($"W-03b after Native: runtime='{runtime}'");

            Assert.True(runtime is not null, "The status bar never reported mode=Native after switching back.");
            Assert.True(
                MockBannerText(window) is null,
                "The MOCK BACKEND banner is still shown after switching back to Native — the request was " +
                "accepted but the factory fell back.");
        });
    }

    /// <summary>Opens Backend &gt; Backend Mode and returns the named child item.</summary>
    private static AutomationElement FindModeItem(Window window, string automationId)
    {
        var backendMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("BackendMenu"));
        Assert.True(backendMenu is not null, "BackendMenu was not found.");
        backendMenu!.AsMenuItem().Expand();

        var modeItem = WaitFor(() =>
            backendMenu.FindFirstDescendant(cf => cf.ByAutomationId("BackendModeMenuItem"))
            ?? window.FindFirstDescendant(cf => cf.ByAutomationId("BackendModeMenuItem")));
        Assert.True(modeItem is not null, "BackendModeMenuItem was not found.");
        modeItem!.AsMenuItem().Expand();

        var item = WaitFor(() =>
            modeItem.FindFirstDescendant(cf => cf.ByAutomationId(automationId))
            ?? window.FindFirstDescendant(cf => cf.ByAutomationId(automationId)));
        Assert.True(item is not null, $"{automationId} was not found.");
        return item!;
    }

    private static void InvokeModeItem(Window window, string automationId)
    {
        var item = FindModeItem(window, automationId);
        item.AsMenuItem().Invoke();
        Thread.Sleep(400);
        CloseBackendMenu(window);
    }

    private static void CloseBackendMenu(Window window)
    {
        var backendMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("BackendMenu"));
        try { backendMenu?.AsMenuItem().Collapse(); } catch (Exception) { /* already closed */ }
        Thread.Sleep(120);
    }

    private static string RuntimeText(Window window)
    {
        var runtime = window.FindFirstDescendant(cf => cf.ByAutomationId("RuntimeCommonVersionText"));
        Assert.True(runtime is not null, "RuntimeCommonVersionText was not found.");
        return runtime!.Name ?? string.Empty;
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
