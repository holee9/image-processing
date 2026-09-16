// #165 (GUI-C-65): the View menu's panel toggles, after they were matched to the current layout.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// Three different claims about the View menu's panel toggles, asserted separately.
///
/// <para>The menu named eight panels; seven of them did nothing. GUI-C-63 measured it: pressing one
/// flipped its check and left the window's UIA descendant count at 146, while the Analysis panel's
/// own Log button moved the same count 146 → 155. GUI-C-64 found why — <c>MENU-001</c> §9.2 predates
/// the Evaluation Workbench redesign by ten days, so three of the panels it names were replaced
/// rather than never built.</para>
///
/// <para>So the menu now says three different things, and each is checked on its own: <b>what was
/// removed is gone</b>, <b>what is scheduled is disabled</b>, and <b>what is wired changes the
/// screen</b>. One scenario covering all three would pass while two of the claims quietly broke.</para>
///
/// <para>The descendant count is GUI-C-63's measure, reused deliberately: it is the one that showed
/// the defect, so it is the one that can show the fix. Counts are taken with the menu CLOSED on both
/// sides — an open menu adds its own popup elements (GUI-C-64 measured 228 with it open), and
/// comparing across that difference would measure the menu.</para>
/// </summary>
[Collection(ApplicationCollection.Name)]
[Trait("Category", "Smoke")]
public sealed class PanelToggleScenarios(ApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>Toggles whose panels the workbench redesign replaced; they are gone from the menu.</summary>
    public static IEnumerable<object[]> RemovedItems() =>
    [
        ["ShowRuntimePanelMenuItem"],
        ["ShowRawSettingsPanelMenuItem"],
        ["ShowAlertsPanelMenuItem"],
        // Added in GUI-C-66. These two were held back one card because the requirement table did
        // not name them and the judgement was to keep what works — then the measurement showed they
        // do not work either: same missing region, same 146->146 (GUI-C-63). Same state, same
        // treatment.
        ["ShowImageSummaryPanelMenuItem"],
        ["ShowMetadataPanelMenuItem"],
    ];

    /// <summary>Toggles MENU-001 §9.2 schedules for a later phase.</summary>
    public static IEnumerable<object[]> ScheduledItems() =>
    [
        ["ShowCalibrationPanelMenuItem"],
        ["ShowDisplaySettingsPanelMenuItem"],
    ];

    /// <summary>
    /// S06: a removed toggle is not in the View menu.
    ///
    /// Asserted by opening the menu and failing to find it — not by reading the XAML, which would
    /// only say what the markup contains and not what the app offers.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(RemovedItems))]
    public void S06_ReplacedPanelToggle_IsNotInTheMenu(string automationId)
    {
        Measure($"S06 {automationId}", window =>
        {
            OpenViewMenu(window);
            var item = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
            var anchor = window.FindFirstDescendant(cf => cf.ByAutomationId("ShowLogsPanelMenuItem"));
            CloseMenu();

            // An absence proves nothing until something present is found beside it. Measured
            // (GUI-C-65): on a Native run where the window element went bad mid-suite, every lookup
            // returned null — S05 could not find the status bar either — and this scenario passed
            // three times for the wrong reason. The anchor is an item that MUST still be in this
            // menu, so an unreadable window fails here instead of quietly agreeing.
            Assert.True(
                anchor is not null,
                "The View menu did not yield ShowLogsPanelMenuItem, so nothing in it was readable — " +
                $"this run cannot say whether '{automationId}' is present or absent.");

            Assert.True(
                item is null,
                $"'{automationId}' is back in the View menu. It names a panel the Evaluation " +
                "Workbench redesign replaced, so pressing it can only flip a checkmark (#165).");
        });
    }

    /// <summary>
    /// S07: a scheduled toggle is present and disabled.
    ///
    /// Both halves matter. Present, because removing it would lose the record that it is coming;
    /// disabled, because an enabled control that does nothing is the defect this card closed.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(ScheduledItems))]
    public void S07_ScheduledPanelToggle_IsPresentButDisabled(string automationId)
    {
        Measure($"S07 {automationId}", window =>
        {
            OpenViewMenu(window);
            var item = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
            var enabled = item?.IsEnabled;
            CloseMenu();

            Assert.True(item is not null, $"'{automationId}' is gone; MENU-001 §9.2 still schedules it.");
            Assert.False(
                enabled,
                $"'{automationId}' is enabled again. Its phase has not arrived, so it has nothing to " +
                "show — and GUI-C-63 measured that pressing it changes nothing on screen (#165).");
        });
    }

    /// <summary>
    /// S08: the Logs toggle changes the screen, in both directions.
    ///
    /// <para>Both directions on purpose: asserting only that it turns something on would pass a
    /// control that can never be switched off again.</para>
    ///
    /// <para>The tab is selected first because the toggle is one of two conditions and does not touch
    /// the other — that separation is the judgement this card implemented, so the scenario has to
    /// honour it rather than route around it.</para>
    /// </summary>
    [SkippableFact]
    public void S08_LogsToggle_ShowsAndHidesTheLogRegion()
    {
        Measure("S08 logs toggle", window =>
        {
            window.FindFirstDescendant(cf => cf.ByName("Log"))!.AsButton().Invoke();
            Thread.Sleep(400);

            // Start from OFF rather than assuming it. The scenarios below share one app, and the
            // first version of this assumed the default and failed once S10 ran before it — a test
            // that depends on what another test left behind measures the order, not the toggle.
            HideTheLog(window);

            var hidden = window.FindAllDescendants().Length;
            var listWhileOff = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));

            InvokeLogsToggle(window);
            var shown = window.FindAllDescendants().Length;
            var listWhileOn = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));

            InvokeLogsToggle(window);
            var hiddenAgain = window.FindAllDescendants().Length;

            output.WriteLine(
                $"S08 descendants off={hidden} on={shown} offAgain={hiddenAgain} " +
                $"(LogListBox off={listWhileOff is not null} on={listWhileOn is not null})");

            Assert.True(
                listWhileOn is not null && shown > hidden,
                $"Turning Logs Panel on left the window at {shown} descendants (was {hidden}) and " +
                $"LogListBox {(listWhileOn is null ? "absent" : "present")}. The toggle is not wired " +
                "to the log region again (#165).");

            Assert.True(
                hiddenAgain == hidden,
                $"Turning it off again left {hiddenAgain} descendants, not the {hidden} it started " +
                "with — the region does not come back down, so the toggle only works once.");
        });
    }

    /// <summary>
    /// S09: Tools → Calibration Settings does not claim a panel appeared.
    ///
    /// <para>It used to log <c>"Menu command: calibration settings panel shown."</c> and set the
    /// status line to <c>"Calibration settings panel visible."</c> while no such panel exists
    /// (measured in GUI-C-65). A log that reports something which did not happen is worse than a
    /// silent one: the next reader believes it.</para>
    ///
    /// <para>Asserted from the log the user can actually read, not from the source — the point is
    /// what the app tells someone, and GUI-C-62 measured that this list is reachable.</para>
    /// </summary>
    [SkippableFact]
    public void S09_CalibrationSettings_DoesNotClaimAPanelAppeared()
    {
        Measure("S09 calibration settings", window =>
        {
            ShowTheLog(window);

            window.FindFirstDescendant(cf => cf.ByAutomationId("ToolsMenu"))!.AsMenuItem().Click();
            Thread.Sleep(300);
            window.FindFirstDescendant(cf => cf.ByAutomationId("CalibrationSettingsMenuItem"))!
                .AsMenuItem().Invoke();
            Thread.Sleep(500);

            var lines = LogLines(window);
            output.WriteLine($"S09 log lines={lines.Length}");
            foreach (var line in lines.Where(l => l.Contains("calibration", StringComparison.OrdinalIgnoreCase)))
            {
                output.WriteLine($"S09 >> {line}");
            }

            var claims = lines.Where(l =>
                l.Contains("panel shown", StringComparison.OrdinalIgnoreCase) ||
                l.Contains("panel visible", StringComparison.OrdinalIgnoreCase)).ToArray();

            Assert.True(
                claims.Length == 0,
                $"The log says a panel appeared: '{string.Join(" | ", claims)}'. No calibration panel " +
                "exists (#165) — do not restore the old wording; build the panel or keep the notice.");

            Assert.Contains(
                lines,
                l => l.Contains("calibration settings", StringComparison.OrdinalIgnoreCase)
                     && l.Contains("not implemented", StringComparison.OrdinalIgnoreCase));
        });
    }

    /// <summary>
    /// S10: Reset Layout puts the state back, rather than only reporting that it ran.
    ///
    /// <para>The observable default is the Logs toggle: <c>ResetLayout</c> turns it on, so with the
    /// log tab selected the region has to reappear. Turning it OFF first is what makes this a
    /// measurement — asserting against a state that was already correct measures nothing, the trap
    /// GUI-C-47 found in the comparison-mode scenarios.</para>
    /// </summary>
    [SkippableFact]
    public void S10_ResetLayout_RestoresTheState()
    {
        Measure("S10 reset layout", window =>
        {
            ShowTheLog(window);
            var regionWhileOn = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));

            HideTheLog(window);
            var regionWhileOff = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));

            // The off state is asserted before the reset: without it, a Reset Layout that did
            // nothing would still find the region present and pass.
            Assert.True(regionWhileOn is not null, "The log region never appeared, so nothing is being reset.");
            Assert.True(regionWhileOff is null, "Turning the toggle off left the log region up.");

            OpenViewMenu(window);
            window.FindFirstDescendant(cf => cf.ByAutomationId("ResetLayoutMenuItem"))!.AsMenuItem().Invoke();
            Thread.Sleep(600);

            var regionAfterReset = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
            output.WriteLine(
                $"S10 log region on={regionWhileOn is not null} off={regionWhileOff is null} " +
                $"afterReset={regionAfterReset is not null}");

            // The whole-window count is NOT used here, though S08 uses it: Reset Layout also restores
            // the comparison view, which adds and removes elements of its own (measured: 155 before,
            // 157 after). A count that moves for two reasons cannot answer a question about one.
            Assert.True(
                regionAfterReset is not null,
                "Reset Layout did not bring the log region back, so it reported a reset it did not " +
                "perform (#165).");

            HideTheLog(window);
        });
    }

    /// <summary>Turns the log region off if it is on, so a case can start from a known state.</summary>
    private static void HideTheLog(Window window)
    {
        if (window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox")) is not null)
        {
            InvokeLogsToggle(window);
        }
    }

    /// <summary>Selects the log tab and makes sure the region is on.</summary>
    private static void ShowTheLog(Window window)
    {
        window.SetForeground();
        window.FindFirstDescendant(cf => cf.ByName("Log"))!.AsButton().Invoke();
        Thread.Sleep(350);

        if (window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox")) is null)
        {
            InvokeLogsToggle(window);
        }
    }

    private static string[] LogLines(Window window)
    {
        var list = window.FindFirstDescendant(cf => cf.ByAutomationId("LogListBox"));
        Assert.True(list is not null, "LogListBox was not reachable, so nothing about the log can be said.");
        return list!.FindAllChildren().Select(i => i.Name).ToArray();
    }

    private static void InvokeLogsToggle(Window window)
    {
        OpenViewMenu(window);
        window.FindFirstDescendant(cf => cf.ByAutomationId("ShowLogsPanelMenuItem"))!.AsMenuItem().Invoke();
        Thread.Sleep(450);
    }

    /// <summary>
    /// Opens View, having first closed whatever was open.
    ///
    /// GUI-C-58 measured a menu case failing when it ran after other scenarios, and GUI-C-61 could
    /// not find the cause; closing first and taking the foreground is the workaround that lane kept.
    /// </summary>
    private static void OpenViewMenu(Window window)
    {
        window.SetForeground();
        Keyboard.Press(VirtualKeyShort.ESCAPE);
        Thread.Sleep(120);
        window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"))!.AsMenuItem().Click();
        Thread.Sleep(300);
    }

    private static void CloseMenu()
    {
        Keyboard.Press(VirtualKeyShort.ESCAPE);
        Thread.Sleep(200);
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
