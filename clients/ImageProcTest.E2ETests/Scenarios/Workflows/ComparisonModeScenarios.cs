// #149 (GUI-C-46): the comparison buttons exist — this presses them.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// Drives the four comparison-mode buttons in <c>ViewportShell</c>.
///
/// GUI-C-43 read the menus and concluded the feature did not exist. GUI-C-45 found the buttons and
/// corrected that — then stopped, and wrote its own limit down: <b>존재는 동작이 아니다</b>. Twice the
/// lane answered a question about behaviour with evidence about existence. This presses them.
///
/// <para><b>What this suite does NOT establish, measured rather than assumed.</b> It shows the
/// buttons are live and the app survives each press. It does NOT show which mode was selected,
/// because no surface reachable through UIA reports <c>Settings.ComparisonMode</c>. Three candidate
/// observation points were tried in GUI-C-46 and each failed for its own reason:</para>
///
/// <list type="number">
/// <item>The opacity slider — rejected by the falsification: commenting out
/// <c>vm.Settings.ComparisonMode = mode</c> left the suite green, because the click handler sets the
/// slider's visibility itself from a local variable (<c>ViewportShell.xaml.cs:30-31</c>), downstream
/// of the assignment under test.</item>
/// <item>The lane badges, which two <c>DataTrigger</c>s on the setting were expected to collapse in
/// Difference mode — measured identical in both modes (84 element names either way).</item>
/// <item>The detached comparison viewer, whose status line binds to the setting — the window never
/// appeared after invoking <c>DetachComparisonViewerMenuItem</c>; the only top-level windows were
/// the taskbar, the main window and Program Manager.</item>
/// </list>
///
/// The mode's effect is verified instead where it is observable: by rendering the control directly
/// (<c>Rendering/ComparisonRenderObservationTests</c>). Writing a scenario that passed without an
/// observation point would put back exactly the gap this card exists to close.
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class ComparisonModeScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    /// <summary>Button labels as the user sees them, with the mode each one selects.</summary>
    public static IEnumerable<object[]> Buttons() =>
    [
        ["Swipe", "SwipeVertical"],
        ["Split", "SplitLocked"],
        ["Overlay", "OverlayOpacity"],
        ["Difference", "DifferenceHeatmap"],
    ];

    /// <summary>
    /// W-11: every comparison button is present, enabled, and can be pressed without taking the app
    /// down — and the viewport is still there afterwards.
    ///
    /// The last clause is the point. A click that throws inside the renderer would kill the window,
    /// and a scenario that only checked "the button exists" would still pass.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(Buttons))]
    public void W11_ComparisonButton_CanBePressedAndTheViewportSurvives(string label, string mode)
    {
        Measure($"W-11 {label}", window =>
        {
            var button = FindButton(window, label);
            Assert.True(button is not null, $"The '{label}' comparison button was not found.");
            Assert.True(button!.IsEnabled, $"The '{label}' comparison button is disabled.");

            button.AsButton().Invoke();
            Thread.Sleep(200);

            var viewport = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewportShell"));
            Assert.True(viewport is not null, $"ViewportShell is gone after pressing '{label}'.");
            Assert.True(
                viewport!.BoundingRectangle.Width > 0 && viewport.BoundingRectangle.Height > 0,
                $"ViewportShell has an empty rectangle after pressing '{label}' ({mode}).");
        });
    }

    /// <summary>
    /// W-12: pressing a button actually sets <c>Settings.ComparisonMode</c> to that mode.
    ///
    /// The observation point is the viewport's <c>HelpText</c>, bound straight to the setting
    /// (<c>MainWindow.xaml</c>, #149 G-6). Straight matters: GUI-C-46 tried three points and the
    /// most promising one — the opacity slider — sat downstream of a SECOND assignment in the click
    /// handler, so removing the setting write left the whole suite green. Nothing writes HelpText
    /// but the binding, so the falsification that defeated the old scenario now fails this one.
    ///
    /// Run for all four buttons in both backend modes: a mode that only worked under Mock would
    /// otherwise pass here and surprise someone on a Native run.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(Buttons))]
    public void W12_PressingAButton_SelectsThatComparisonMode(string label, string mode)
    {
        Measure($"W-12 {label}", window =>
        {
            // Move to a DIFFERENT mode first, so every case is a transition. Without this the Swipe
            // case cannot fail: SwipeVertical is the default, so a click that never reaches the
            // setting still leaves the expected value in place. Measured in the GUI-C-47
            // falsification — three of four cases failed and Swipe passed for exactly that reason.
            var priming = string.Equals(label, "Swipe", StringComparison.Ordinal) ? "Difference" : "Swipe";
            var primedMode = Buttons()
                .First(row => string.Equals((string)row[0], priming, StringComparison.Ordinal))[1];

            FindButton(window, priming)!.AsButton().Invoke();
            var primed = WaitFor(() => ReportedMode(window) == (string)primedMode ? "ok" : null);

            // The priming itself is asserted. Otherwise the Swipe case is undecidable: SwipeVertical
            // is the default, so when no click reaches the setting the expected value is already
            // there and the case passes while measuring nothing — measured in the first GUI-C-47
            // falsification, where three of four cases failed and Swipe passed for that reason.
            Assert.True(
                primed is not null,
                $"Priming with '{priming}' did not take effect (viewport reports " +
                $"'{ReportedMode(window)}'), so this case cannot tell a working click from a dead one.");

            FindButton(window, label)!.AsButton().Invoke();

            var reported = WaitFor(() => ReportedMode(window) == mode ? mode : null);
            var current = ReportedMode(window);
            Assert.True(
                reported is not null,
                $"Pressing '{label}' should select {mode}; the viewport reports '{current}'. " +
                "HelpText is bound directly to Settings.ComparisonMode, so this says the click did " +
                "not reach the setting — do not relax this scenario, it is the only external read " +
                "of that state (#149 G-6).");

            output.WriteLine($"W-12 {label}: viewport reports {reported}");
        });
    }

    /// <summary>The comparison mode the viewport currently reports, or null when unreadable.</summary>
    private static string? ReportedMode(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId("ViewportShell"))?.HelpText;

    /// <summary>
    /// Finds a comparison button by its visible label.
    ///
    /// By name, not by automation id: these buttons carry neither an AutomationId nor an x:Name —
    /// the mode travels in Tag. Reported as #149 rather than worked around by editing the app,
    /// which this card does not permit.
    /// </summary>
    private static AutomationElement? FindButton(Window window, string label) =>
        window.FindFirstDescendant(cf => cf.ByName(label));

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
