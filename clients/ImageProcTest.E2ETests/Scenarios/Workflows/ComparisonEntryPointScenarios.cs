// #149 G-1/G-2/G-3 (GUI-C-58, GUI-C-96): the menu, the key gestures and the toolbar buttons that reach the comparison modes.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.Input;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Drives the two entry points GUI-C-58 added: <c>View → Compare Mode</c> and the key gestures.
///
/// <para>The renderer has answered to seven modes since GUI-C-46 and four of them could be reached
/// by the segmented buttons; <c>SourceOnly</c> and <c>ProcessedOnly</c> had no caller at all, and no
/// gesture reached anything (a search for <c>KeyBinding</c> matched zero files). The feature existed
/// and could not be called.</para>
///
/// <para><b>What is asserted is the transition, not the element.</b> GUI-C-43 read the menus and
/// concluded a feature did not exist; GUI-C-45 corrected it and wrote the lesson down — 존재는
/// 동작이 아니다. So every case here primes a different mode first and then asserts that the entry
/// point moved it. Since GUI-C-96 the mode is read from the viewport's own automation peer
/// (<c>rendered=&lt;mode&gt;</c>, set in its render pass). It used to be <c>ViewportShell</c>'s HelpText,
/// bound to <c>Settings.ComparisonMode</c>: with the viewport's CompareMode binding removed, W-14 still
/// passed 4 of 4 on that reading and fails 4 of 4 on this one (#172).</para>
///
/// <para><b>Four gestures are bound, two are not.</b> GUI-C-58 left three unbound because MENU-001
/// §9.3/§10.1 and ACCESS-001 §5.2 — which MENU-001 §10 calls a single source of truth — disagreed
/// about F6 and Ctrl+1. Both were decided in <c>6dffce0</c>: F6 is Split (ACCESS-001 was wrong, and
/// the value it carried also collided with F8), and Ctrl+1 belongs to Zoom 100 %, so Source Only and
/// Processed Only keep no key at all and reach the user through the menu.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class ComparisonEntryPointScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>Every item under View → Compare Mode, with the mode it selects.</summary>
    public static IEnumerable<object[]> MenuItems() =>
    [
        ["CompareSwipeMenuItem", "SwipeVertical"],
        ["CompareSplitMenuItem", "SplitLocked"],
        ["CompareOverlayMenuItem", "OverlayOpacity"],
        ["CompareDifferenceMenuItem", "DifferenceHeatmap"],
        ["CompareSourceOnlyMenuItem", "SourceOnly"],
        ["CompareProcessedOnlyMenuItem", "ProcessedOnly"],
    ];

    /// <summary>The six segmented buttons on the viewport toolbar (#149 G-3, GUI-C-96).</summary>
    public static IEnumerable<object[]> Buttons() =>
    [
        ["CompareButtonSwipeVertical", "SwipeVertical"],
        ["CompareButtonSplitLocked", "SplitLocked"],
        ["CompareButtonOverlayOpacity", "OverlayOpacity"],
        ["CompareButtonDifferenceHeatmap", "DifferenceHeatmap"],
        ["CompareButtonSourceOnly", "SourceOnly"],
        ["CompareButtonProcessedOnly", "ProcessedOnly"],
    ];

    /// <summary>The gestures that are actually bound, with the mode each selects.</summary>
    public static IEnumerable<object[]> Gestures() =>
    [
        ["F5", "SwipeVertical"],
        ["F6", "SplitLocked"],
        ["F7", "OverlayOpacity"],
        ["F8", "DifferenceHeatmap"],
    ];

    /// <summary>
    /// W-13: each View → Compare Mode item selects its mode.
    ///
    /// Two of the six — Source Only and Processed Only — have no other entry point at all, so this
    /// is the whole of #149 G-3 rather than a second route to something already reachable.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(MenuItems))]
    public void W13_CompareMenuItem_SelectsThatMode(string automationId, string mode)
    {
        Measure($"W-13 {automationId}", window =>
        {
            PrimeADifferentMode(window, mode);

            InvokeCompareMenuItem(window, automationId);

            var reported = WaitFor(() => ReportedMode(window) == mode ? mode : null);
            Assert.True(
                reported is not null,
                $"'{automationId}' should select {mode}; the viewport reports " +
                $"'{ReportedMode(window)}'. HelpText is bound directly to Settings.ComparisonMode, " +
                "so this says the menu item did not reach the setting.");

            output.WriteLine($"W-13 {automationId}: viewport reports {reported}");
        });
    }

    /// <summary>
    /// W-14: each bound key gesture selects its mode.
    ///
    /// <para>The window is brought to the foreground first — a gesture goes to whatever has focus,
    /// so a scenario that skipped this would be testing whichever window the desktop happened to be
    /// showing. The press itself is a real keystroke through the OS, not an invoke on a command
    /// object: what #149 G-2 asks is whether a user pressing F8 changes the mode.</para>
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(Gestures))]
    public void W14_BoundKeyGesture_SelectsThatMode(string gesture, string mode)
    {
        // GUI-C-171: the key press is the subject here, so it cannot become a UI Automation call — and a real key
        // press goes to whichever window is in front. Skipped unless real desktop input was allowed.
        GlobalInput.Require("W-14 (a bound key gesture)");
        Measure($"W-14 {gesture}", window =>
        {
            PrimeADifferentMode(window, mode);

            window.SetForeground();
            window.Focus();
            Thread.Sleep(200);
            GlobalInput.Press(KeyFor(gesture));

            var reported = WaitFor(() => ReportedMode(window) == mode ? mode : null);
            Assert.True(
                reported is not null,
                $"{gesture} should select {mode}; the viewport reports '{ReportedMode(window)}'. " +
                "The gesture is declared in MainWindow's InputBindings — this says the press did " +
                "not reach the command.");

            output.WriteLine($"W-14 {gesture}: viewport reports {reported}");
        });
    }

    /// <summary>
    /// W-27: each viewport toolbar button draws its mode. Priming goes through a KEY GESTURE, so a
    /// broken button cannot make its own case vacuous. (The first version primed through the menu and
    /// its first case failed to find the menu item after the W-14 key presses — the #163 symptom,
    /// GUI-C-96. The route under test is the button, so the priming route was changed rather than
    /// retried.)
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(Buttons))]
    public void W27_CompareButton_DrawsThatMode(string automationId, string mode)
    {
        Measure($"W-27 {automationId}", window =>
        {
            // GUI-C-171: primed through View → Compare Mode, not through a key press. The key press was chosen
            // because priming through the menu failed after the W-14 key presses (GUI-C-96) — the same cause
            // as the S07 failure: input addressed to whichever window is in front. The menu is opened through
            // UI Automation now, and it is still a route different from the button under test.
            var (primeItem, primeMode) = mode == "SwipeVertical"
                ? ("CompareDifferenceMenuItem", "DifferenceHeatmap")
                : ("CompareSwipeMenuItem", "SwipeVertical");
            InvokeCompareMenuItem(window, primeItem);
            Assert.True(
                WaitFor(() => ReportedMode(window) == primeMode ? "ok" : null) is not null,
                $"Priming through View → Compare Mode ({primeItem}) did not take effect (viewport drew '{ReportedMode(window)}').");

            var button = window.FindFirstDescendant(cf => cf.ByAutomationId(automationId));
            Assert.True(button is not null, $"'{automationId}' is not in the automation tree.");
            button!.AsButton().Invoke();

            var reported = WaitFor(() => ReportedMode(window) == mode ? mode : null);
            Assert.True(
                reported is not null,
                $"'{automationId}' should draw {mode}; the viewport's last frame was '{ReportedMode(window)}'.");

            output.WriteLine($"W-27 {automationId}: viewport drew {reported}");
        });
    }

    /// <summary>
    /// Puts the app into a mode OTHER than the one under test, and asserts that it got there.
    ///
    /// <para>Without this the Swipe cases cannot fail: <c>SwipeVertical</c> is the default, so an
    /// entry point that never reaches the setting still leaves the expected value in place. Measured
    /// in GUI-C-47's falsification, where three of four cases failed and Swipe passed for exactly
    /// that reason. The priming is asserted for the same reason it exists.</para>
    ///
    /// <para>It primes through the BUTTONS rather than through the menu, so a broken menu cannot
    /// make a menu case vacuous — the priming route and the route under test are different code.</para>
    /// </summary>
    private static void PrimeADifferentMode(Window window, string modeUnderTest)
    {
        var (label, primedMode) = string.Equals(modeUnderTest, "SwipeVertical", StringComparison.Ordinal)
            ? ("Difference", "DifferenceHeatmap")
            : ("Swipe", "SwipeVertical");

        var button = window.FindFirstDescendant(cf => cf.ByAutomationId($"CompareButton{primedMode}"));
        Assert.True(button is not null, $"The '{label}' comparison button, used for priming, was not found.");

        button!.AsButton().Invoke();
        var primed = WaitFor(() => ReportedMode(window) == primedMode ? "ok" : null);

        Assert.True(
            primed is not null,
            $"Priming with '{label}' did not take effect (viewport reports '{ReportedMode(window)}'), " +
            "so this case cannot tell a working entry point from a dead one.");
    }

    /// <summary>
    /// Opens View → Compare Mode and invokes one item.
    ///
    /// WPF realises a submenu's children only once the parent is expanded, so both ancestors are
    /// clicked before the item is looked for — a straight descendant search from the window finds
    /// nothing while the menu is closed (GUI-C-43).
    /// </summary>
    private static void InvokeCompareMenuItem(Window window, string automationId)
    {
        // GUI-C-58 measured the last of six menu cases failing in the full suite after the keyboard scenarios had
        // run, and answered with "Escape first, and take the foreground". GUI-C-171 found the cause: the ESC
        // and the click are input addressed to whichever window is in front, and on a shared desktop that is not
        // the app. The View menu is therefore opened through UI Automation (UiaMenu), which is addressed to the
        // menu itself — no Escape, no foreground grab, no click.
        UiaMenu.Open(window, "ViewMenu");
        Thread.Sleep(200);

        var compare = WaitFor(() => window.FindFirstDescendant(cf => cf.ByAutomationId("CompareModeMenuItem")));
        Assert.True(compare is not null, "View → Compare Mode was not found after opening the View menu.");
        // #163 (GUI-C-108 → GUI-C-130 → GUI-C-133): Expand, and NO click fallback.
        //
        // Clicking a submenu header TOGGLES it, and moving the mouse onto it can already have opened it
        // on hover — so the click can CLOSE the menu instead of opening the submenu. GUI-C-130 measured
        // that as the cause, in both directions: forcing the click failed 5 of 20 runs (run 2, 4, 13,
        // 17, 20) while Expand passed 20 of 20, and restoring Expand made the failures go away again.
        //
        // The click used to live here as a fallback for a missing ExpandCollapse pattern. GUI-C-133
        // measured how often that fallback actually ran: 0 times in 120 invocations across 20 runs. It
        // was dead code that would resurrect the defect on any machine or UIA version where the pattern
        // IS unavailable — so it is removed rather than made safer. Pressing a toggle without knowing
        // its state is right about half the time; failing loudly beats being silently wrong half the
        // time, and a retry would only shake the state further.
        Assert.True(compare!.Patterns.ExpandCollapse.TryGetPattern(out var expand),
            "The ExpandCollapse pattern for View → Compare Mode could not be obtained, and the click " +
            "fallback was REMOVED in #163 (GUI-C-133) because clicking a submenu header toggles it and " +
            "closes the parent popup about half the time (measured 5 of 20). If this fires, the menu " +
            "needs a non-toggling way in — do not restore the click.");
        expand.Expand();

        Thread.Sleep(200);

        var item = WaitFor(() => window.FindFirstDescendant(cf => cf.ByAutomationId(automationId)));
        Assert.True(item is not null,
            $"'{automationId}' was not found after opening View → Compare Mode. {DescribeMenuState(window, automationId)}");
        item!.AsMenuItem().Invoke();
        Thread.Sleep(200);
    }

    /// <summary>
    /// What the menu looked like at the moment a lookup failed (#163, GUI-C-108).
    ///
    /// <para>This runs ONLY on the failure path, after the wait has already timed out. That matters:
    /// the issue records that adding a probe which ran every cycle erased the symptom (48 cycles, 0
    /// reproductions), so anything on the passing path changes the thing being measured. A read taken
    /// after the failure has already happened cannot have caused it.</para>
    ///
    /// <para>It separates the two shapes the remaining hypothesis splits into: the parent popup GONE
    /// (the click closed it instead of opening the submenu) versus the parent still there with its
    /// children not realised (a timing problem one level down). It also repeats the search from the
    /// desktop, because a popup can live outside the window's tree.</para>
    /// </summary>
    private static string DescribeMenuState(Window window, string automationId)
    {
        try
        {
            var view = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
            var viewState = view is null ? "absent"
                : view.Patterns.ExpandCollapse.TryGetPattern(out var ec)
                    ? ec.ExpandCollapseState.Value.ToString()
                    : "no-pattern";
            var compare = window.FindFirstDescendant(cf => cf.ByAutomationId("CompareModeMenuItem"));
            var compareState = compare is null ? "absent"
                : compare.Patterns.ExpandCollapse.TryGetPattern(out var ec2)
                    ? ec2.ExpandCollapseState.Value.ToString()
                    : "present";
            var desktop = window.Automation.GetDesktop().FindFirstDescendant(cf => cf.ByAutomationId(automationId));

            return $"[#163 probe] ViewMenu={viewState}; CompareModeMenuItem={compareState}; " +
                   $"desktopSearch={(desktop is null ? "MISS" : "HIT")}";
        }
        catch (Exception ex)
        {
            return $"[#163 probe] threw: {ex.GetType().Name} {ex.Message}";
        }
    }

    private static VirtualKeyShort KeyFor(string gesture) => gesture switch
    {
        "F5" => VirtualKeyShort.F5,
        "F6" => VirtualKeyShort.F6,
        "F7" => VirtualKeyShort.F7,
        "F8" => VirtualKeyShort.F8,
        _ => throw new ArgumentOutOfRangeException(nameof(gesture), gesture, "No virtual key for this gesture."),
    };

    /// <summary>
    /// The mode the viewport DREW in its last frame, or null when unreadable (#149, GUI-C-96).
    ///
    /// <para>This used to read <c>ViewportShell</c>'s HelpText, which is bound to
    /// <c>Settings.ComparisonMode</c>: it proved the command reached the setting, not that the viewport
    /// drew anything different (#172). The viewport's automation peer now reports
    /// <c>rendered=&lt;mode&gt;</c>, set inside its render pass.</para>
    /// </summary>
    private static string? ReportedMode(Window window)
    {
        var help = window.FindFirstDescendant(cf => cf.ByAutomationId("WorkbenchViewport"))?.HelpText;
        return help is not null && help.StartsWith("rendered=", StringComparison.Ordinal) ? help["rendered=".Length..].Split(';')[0] : null;
    }

    private static T? WaitFor<T>(Func<T?> find) where T : class
    {
        var deadline = DateTime.UtcNow + TimeSpan.FromSeconds(5);
        while (DateTime.UtcNow < deadline)
        {
            var found = find();
            if (found is not null) return found;
            Thread.Sleep(100);
        }

        return null;
    }

    private void Measure(string scenario, Action<Window> body)
    {
        Skip.If(!app.IsAvailable, app.SkipReason ?? "The application is not available.");

        // GUI-C-51: a re-acquired window is reported rather than silently swallowed.
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
