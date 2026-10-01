// GUI-C-172 (Codex audit #8): the two ways a PERSON drives the window, tested as a person drives it.
using System.Diagnostics;
using FlaUI.Core.AutomationElements;
using FlaUI.Core.WindowsAPI;
using ImageProcTest.E2ETests.Fixtures;
using ImageProcTest.E2ETests.Scenarios.Smoke;
using Xunit;
using Xunit.Abstractions;
using static ImageProcTest.E2ETests.Scenarios.Workflows.WorkbenchObservation;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

/// <summary>
/// The scenarios that need REAL mouse and keyboard input, and nothing else.
///
/// <para><b>Why only two.</b> GUI-C-171 moved every other scenario to UI Automation: a click or a key press is
/// addressed to whichever window is in front, which on a shared desktop is not the application (see
/// <see cref="GlobalInput"/>). The functional assertions all stayed. What the ordinary scenarios no longer
/// show is that a PERSON can open a menu with the mouse and can type into a box and Tab out of it, because they
/// open menus through <see cref="UiaMenu"/> and write text through UI Automation. These two scenarios are where
/// that is still shown, so the claim "the real path works" is covered by two named scenarios instead of by
/// fourteen incidental ones.</para>
///
/// <para><b>Gated.</b> Each begins with <see cref="GlobalInput.Require"/>: skipped, before anything is sent,
/// unless <c>XPE_E2E_ALLOW_GLOBAL_INPUT=1</c>. They are meant for a desktop that nothing else uses (the CI
/// runner), and are NOT run on a shared workstation.</para>
///
/// <para><b>Each has a control.</b> A check that cannot fail proves nothing, so each scenario first shows the
/// state in which the real input is needed — the menu closed, the stale indicator absent, the box holding the
/// keyboard focus — and only then sends the input.</para>
/// </summary>
[Collection(WorkflowApplicationCollection.Name)]
public sealed class RealInputScenarios(WorkflowApplicationFixture app, ITestOutputHelper output)
{
    private const string AlwaysInTheViewMenu = "ShowLogsPanelMenuItem";

    /// <summary>
    /// R01: a real mouse click on the View menu opens it.
    ///
    /// <para>The click is <see cref="GlobalInput.Click"/> — a real mouse event at the menu's position — and not an
    /// Expand. Control: the menu is shown closed first (its items are not in the tree), so an item that is there
    /// afterwards came from the click.</para>
    /// </summary>
    [SkippableFact]
    public void R01_TheViewMenu_OpensWithARealMouseClick()
    {
        GlobalInput.Require("R01 (a real mouse click on the View menu)");
        Measure("R01", window =>
        {
            UiaMenu.CollapseAll(window);     // set-up through UI Automation: not what is under test
            Thread.Sleep(300);
            Assert.True(
                ViewMenuItem(window) is null,
                $"'{AlwaysInTheViewMenu}' was already visible before the click, so the click cannot be shown to be what opened the menu.");

            window.SetForeground();
            var viewMenu = window.FindFirstDescendant(cf => cf.ByAutomationId("ViewMenu"));
            Assert.True(viewMenu is not null, "ViewMenu is not in the automation tree.");

            try
            {
                GlobalInput.Click(viewMenu!);

                var item = WaitFor(() => ViewMenuItem(window));
                output.WriteLine($"R01 item after the click: {(item is null ? "(absent)" : "present")}");
                Assert.True(
                    item is not null,
                    $"A real mouse click on the View menu did not open it: '{AlwaysInTheViewMenu}' was not visible within 2 s. " +
                    PanelToggleScenarios.DescribeWhatIsInFront(window));
            }
            finally
            {
                UiaMenu.CollapseAll(window);
            }
        });
    }

    /// <summary>
    /// R02: real key presses edit a text box, and a real Tab moves the focus out of it.
    ///
    /// <para>Controls, in order: the image starts not stale; the center box really holds the keyboard focus (so
    /// the keystrokes can reach it); the typed text shows in the box (what a person would see). The edit is
    /// observed the way W22 observes one — the stale indicator, which only appears once the value has reached the
    /// view model. Then a real Tab. A Tab goes to whichever window holds the focus when it is sent, so it is
    /// checked on both sides: just before it, the keyboard focus must still be this box in this app; after it, the
    /// focus must be on an element INSIDE this app (compared by process id), not the box, and — read from the XAML
    /// — the next text box in tab order. A check on the box alone would also pass if another window had taken the
    /// focus and received the Tab (the GUI-C-171 failure shape; Codex audit #9).</para>
    ///
    /// <para><b>What this does not claim.</b> Every text box in this app binds with
    /// <c>UpdateSourceTrigger=PropertyChanged</c>, so the value reaches the view model as it is typed — a Tab is
    /// not what commits it. An earlier note that "the binding takes the value when the box loses focus" was
    /// wrong for these boxes (GUI-C-172). This scenario therefore shows typing and Tab-to-move, not
    /// Tab-to-commit.</para>
    /// </summary>
    [SkippableFact]
    public void R02_RealKeystrokes_EditTheVoiBox_AndARealTabMovesOn()
    {
        GlobalInput.Require("R02 (real key presses into a text box, then a real Tab)");
        Measure("R02", window =>
        {
            OpenParameters(window);          // set-up through UI Automation
            var original = BodyPart(window);
            try
            {
                // A preset change re-renders and leaves nothing stale (W22's first control): a known start.
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                Assert.True(
                    StaleIndicator(window) is null,
                    $"The image was already stale ('{StaleIndicator(window)}') before anything was typed, so the edit cannot be shown to be what made it so.");

                var input = window.FindFirstDescendant(cf => cf.ByAutomationId("VoiWindowCenterInput"))!.AsTextBox();
                window.SetForeground();
                input.Focus();               // puts the caret in the box; the KEYS below are real
                Assert.True(
                    input.Properties.HasKeyboardFocus.ValueOrDefault,
                    "The center box does not hold the keyboard focus, so real key presses would not reach it. " +
                    PanelToggleScenarios.DescribeWhatIsInFront(window));

                const string typed = "4321";
                Assert.NotEqual(typed, input.Text);

                GlobalInput.SelectAll();
                GlobalInput.Type(typed);
                Thread.Sleep(600);

                Assert.Equal(typed, input.Text);
                var stale = StaleIndicator(window);
                output.WriteLine($"R02 after typing: box='{input.Text}' indicator='{stale}'");
                Assert.True(
                    stale is not null && stale.Contains("parameters changed", StringComparison.Ordinal),
                    $"Real key presses changed the box to '{input.Text}' but the image was not marked stale (indicator: '{stale ?? "(absent)"}'), " +
                    "so the typed value did not reach the view model.");

                // Just BEFORE the Tab: the keys above took 600 ms, and a Tab goes to whichever window holds the
                // focus when it is sent. Re-establish that it is still this box, in this app.
                var appProcess = window.Properties.ProcessId.ValueOrDefault;
                var beforeTab = FocusedInfo(window);
                Assert.True(
                    input.Properties.HasKeyboardFocus.ValueOrDefault
                    && beforeTab is { AutomationId: CenterBox } && beforeTab.Value.ProcessId == appProcess,
                    $"Just before the Tab the keyboard focus was not in the center box of this app (focused: {Describe(beforeTab)}), " +
                    "so the Tab would not have been addressed to it. " + PanelToggleScenarios.DescribeWhatIsInFront(window));

                GlobalInput.Press(VirtualKeyShort.TAB);
                var afterTab = WaitForFocusToLeave(window, CenterBox);

                // In this order, because each says something different. A focus in ANOTHER PROCESS means the Tab
                // went to a different window (the GUI-C-171 failure shape) — a box that merely lost the focus
                // would have passed an assertion on the box alone.
                Assert.True(
                    afterTab is not null && afterTab.Value.ProcessId == appProcess,
                    $"After the Tab the keyboard focus is not inside the app (focused: {Describe(afterTab)}; app pid {appProcess}): " +
                    "the Tab went to a different window. " + PanelToggleScenarios.DescribeWhatIsInFront(window));
                Assert.True(
                    afterTab!.Value.AutomationId != CenterBox,
                    $"After a real Tab the center box still holds the keyboard focus: Tab did not move it on (focused: {Describe(afterTab)}).");
                // The next focusable control in document order. Both boxes sit in one Grid (AnalysisPanel.xaml) and
                // neither the panel nor MainWindow sets TabIndex, IsTabStop or KeyboardNavigation, so the default
                // order applies: Center, then Width. Read from the XAML, not yet observed in a run.
                Assert.True(
                    afterTab.Value.AutomationId == NextBoxInTabOrder,
                    $"After a real Tab the focus is on {Describe(afterTab)}, not on '{NextBoxInTabOrder}', the next text box in tab order.");
            }
            finally
            {
                // The fixture is shared: put the preset back, which also clears the typed value and the stale mark.
                SelectBodyPart(window, original == "Bone" ? "Lung" : "Bone");
                SelectBodyPart(window, original);
            }
        });
    }

    private const string CenterBox = "VoiWindowCenterInput";
    private const string NextBoxInTabOrder = "VoiWindowWidthInput";

    /// <summary>Who holds the keyboard focus right now, as UI Automation reports it — in any process.</summary>
    private readonly record struct FocusInfo(string AutomationId, int ProcessId, string Name);

    private static FocusInfo? FocusedInfo(Window window)
    {
        try
        {
            var focused = window.Automation.FocusedElement();
            return focused is null
                ? null
                : new FocusInfo(
                    focused.Properties.AutomationId.ValueOrDefault ?? string.Empty,
                    focused.Properties.ProcessId.ValueOrDefault,
                    focused.Properties.Name.ValueOrDefault ?? string.Empty);
        }
        catch (Exception)
        {
            return null;
        }
    }

    /// <summary>Waits (up to 2 s) until the focused element is no longer <paramref name="automationId"/>, and returns what it is.</summary>
    private static FocusInfo? WaitForFocusToLeave(Window window, string automationId)
    {
        var waited = Stopwatch.StartNew();
        FocusInfo? now;
        while ((now = FocusedInfo(window)) is { } info && info.AutomationId == automationId && waited.ElapsedMilliseconds < 2000)
        {
            Thread.Sleep(50);
        }

        return now;
    }

    private static string Describe(FocusInfo? info) =>
        info is { } i ? $"'{i.AutomationId}' (name '{i.Name}', pid {i.ProcessId})" : "(nothing, or it could not be read)";

    private static AutomationElement? ViewMenuItem(Window window) =>
        window.FindFirstDescendant(cf => cf.ByAutomationId(AlwaysInTheViewMenu));

    private static AutomationElement? WaitFor(Func<AutomationElement?> look)
    {
        var waited = Stopwatch.StartNew();
        AutomationElement? found;
        while ((found = look()) is null && waited.ElapsedMilliseconds < 2000)
        {
            Thread.Sleep(50);
        }

        return found;
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
