// #149 G-1/G-2/G-3 (GUI-C-58): the menu and the key gestures that reach the comparison modes.
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
/// point moved it, read back from the viewport's <c>HelpText</c>, which is bound straight to
/// <c>Settings.ComparisonMode</c> (#149 G-6, GUI-C-47).</para>
///
/// <para><b>Three gestures are bound, three are not</b>, and that is a finding rather than an
/// omission: MENU-001 §9.3/§10.1 and ACCESS-001 §5.2 — which MENU-001 §10 calls a single source of
/// truth — disagree about F6 and Ctrl+1. The unbound three are covered by the menu cases; the
/// conflict is in the GUI-C-58 report.</para>
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

    /// <summary>The gestures that are actually bound, with the mode each selects.</summary>
    public static IEnumerable<object[]> Gestures() =>
    [
        ["F5", "SwipeVertical"],
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
        Measure($"W-14 {gesture}", window =>
        {
            PrimeADifferentMode(window, mode);

            window.SetForeground();
            window.Focus();
            Thread.Sleep(200);
            Keyboard.Press(KeyFor(gesture));

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

        var button = window.FindFirstDescendant(cf => cf.ByName(label));
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
        // Escape first, and take the foreground. Measured (GUI-C-58): run in isolation all six menu
        // cases passed, and in the full suite the LAST of them failed after the keyboard scenarios
        // had run — those bring the window to the foreground and press keys, and a menu left open or
        // a focus that moved makes the next click land somewhere else. Closing any open menu and
        // re-taking the foreground removes the dependency on what the previous scenario left behind.
        window.SetForeground();
        Keyboard.Press(VirtualKeyShort.ESCAPE);
        Thread.Sleep(100);

        window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"))!.AsMenuItem().Click();
        Thread.Sleep(200);

        var compare = WaitFor(() => window.FindFirstDescendant(cf => cf.ByAutomationId("CompareModeMenuItem")));
        Assert.True(compare is not null, "View → Compare Mode was not found after opening the View menu.");
        compare!.AsMenuItem().Click();
        Thread.Sleep(200);

        var item = WaitFor(() => window.FindFirstDescendant(cf => cf.ByAutomationId(automationId)));
        Assert.True(item is not null, $"'{automationId}' was not found after opening View → Compare Mode.");
        item!.AsMenuItem().Invoke();
        Thread.Sleep(200);
    }

    private static VirtualKeyShort KeyFor(string gesture) => gesture switch
    {
        "F5" => VirtualKeyShort.F5,
        "F7" => VirtualKeyShort.F7,
        "F8" => VirtualKeyShort.F8,
        _ => throw new ArgumentOutOfRangeException(nameof(gesture), gesture, "No virtual key for this gesture."),
    };

    /// <summary>The comparison mode the viewport currently reports, or null when unreadable.</summary>
    private static string? ReportedMode(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("ViewportShell"))?.HelpText;

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
